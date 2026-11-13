/* lv_script.c - the level script VM (owner: levels, LV2; core/include/doomo/lv_script.h).
 *
 * No statics, nothing allocated, no clock and no rng: the state is the
 * caller's LvVm. Nothing in the game calls it until a level has a script,
 * so every build's --gc-sections drops it (on the PS1 it belongs to the
 * combat + levels overlay group: ps1/tools/ovl_plan.py).
 */
#include "doomo/lv_script.h"

/* operand bytes of each action */
static const uint8_t LV_OP_LEN[LVO_COUNT] = {
    0, 1, 1, 1, 1, 2, 1, 2, 2, 1, 2, 4, 2, 2, 1, 1, 0, 5, 2, 1, 1
};

/* compile-time budget checks (a negative array size fails the build) */
/* the state without its 4 pointers stays within the plan's 96 bytes (8.3) */
typedef char lv_assert_vm_size[(sizeof(LvVm) - 4 * sizeof(void *) <= 96) ? 1 : -1];
typedef char lv_assert_fired[(LV_VM_HANDLERS % 8 == 0 && LV_VM_HANDLERS <= 255) ? 1 : -1];
typedef char lv_assert_ops[(sizeof(LV_OP_LEN) == LVO_COUNT) ? 1 : -1];

/* ---- walking the script -------------------------------------------------- */
/* Offset of handler `h`'s header (handlers are variable length), or -1. */
static int hdr_of(const uint8_t *s, int len, int h)
{
    int i, off = 1;
    if (!s || len < 1 || h < 0 || h >= s[0]) return -1;
    for (i = 0; i < h; i++) {
        if (off + LV_VM_HDR > len) return -1;
        off += LV_VM_HDR + s[off + 4];
    }
    return off + LV_VM_HDR <= len ? off : -1;
}

int lv_script_check(const uint8_t *s, int len, const DoomoMapExt *x)
{
    int n, h, off = 1;
    if (!s || len == 0) return LVERR_OK;
    if (len < 0 || len > LV_VM_BYTES) return LVERR_SIZE;
    n = s[0];
    if (n > LV_VM_HANDLERS) return LVERR_SIZE;
    for (h = 0; h < n; h++) {
        int ev, end, a, b;
        if (off + LV_VM_HDR > len) return LVERR_FORMAT;
        ev = s[off]; a = s[off + 1]; b = s[off + 2];
        if (ev <= LVE_NONE || ev >= LVE_COUNT) return LVERR_FORMAT;
        if (ev == LVE_TIMER && (a | (b << 8)) == 0) return LVERR_OPERAND;
        if (ev == LVE_VAR && a >= LV_VM_VARS) return LVERR_OPERAND;
        if (s[off + 3] & ~LVSF_REPEAT) return LVERR_FORMAT;
        end = off + LV_VM_HDR + s[off + 4];
        if (end > len) return LVERR_FORMAT;
        off += LV_VM_HDR;
        while (off < end) {
            int op = s[off];
            if (op >= LVO_COUNT) return LVERR_OP;
            if (off + 1 + LV_OP_LEN[op] > end) return LVERR_FORMAT;
            a = LV_OP_LEN[op] > 0 ? s[off + 1] : 0;       /* never read past the action */
            b = LV_OP_LEN[op] > 1 ? s[off + 2] : 0;
            switch (op) {
            case LVO_SWAP: case LVO_UNSWAP:
                if (x && a >= x->swap_count) return LVERR_OPERAND;
                break;
            case LVO_ZONE:
                if (x && a >= x->zone_count) return LVERR_OPERAND;
                break;
            case LVO_SET: case LVO_ADD:
                if (a >= LV_VM_VARS) return LVERR_OPERAND;
                break;
            case LVO_CAM:
                if (x && !map_path_by_id(x, a)) return LVERR_OPERAND;
                if ((b | (s[off + 3] << 8)) == 0) return LVERR_OPERAND;
                break;
            case LVO_CHOICE:
                if (a >= LV_VM_VARS) return LVERR_OPERAND;
                break;
            default: break;
            }
            off += 1 + LV_OP_LEN[op];
        }
    }
    return off == len ? LVERR_OK : LVERR_FORMAT;
}

/* ---- events and the queue ------------------------------------------------- */
static void enqueue(LvVm *vm, int h)
{
    if (vm->q_n >= LV_VM_QUEUE) { vm->err |= LVVE_QUEUE; return; }
    vm->q[(vm->q_head + vm->q_n) % LV_VM_QUEUE] = (uint8_t)h;
    vm->q_n++;
}

/* Does handler header `p` answer (ev, a)? A var event also needs the value. */
static int matches(const LvVm *vm, const uint8_t *p, int ev, int a)
{
    if (p[0] != ev) return 0;
    if (ev == LVE_START) return 1;
    if (ev == LVE_VAR) return p[1] == a && vm->var[a] == p[2];
    return p[1] == a;
}

static void dispatch(LvVm *vm, int ev, int a)
{
    const uint8_t *s = vm->x ? vm->x->script : NULL;
    int n, h, off = 1;
    if (!s || vm->ended) return;
    n = s[0];
    for (h = 0; h < n; h++) {
        const uint8_t *p = s + off;
        off += LV_VM_HDR + p[4];
        if (!matches(vm, p, ev, a)) continue;
        if (!(p[3] & LVSF_REPEAT)) {
            if (vm->fired[h >> 3] & (1u << (h & 7))) continue;
            vm->fired[h >> 3] = (uint8_t)(vm->fired[h >> 3] | (1u << (h & 7)));
        }
        enqueue(vm, h);
    }
}

void lv_vm_event(LvVm *vm, int ev, int a)
{
    if (!vm || ev <= LVE_NONE || ev >= LVE_COUNT) return;
    dispatch(vm, ev, a & 255);
}

int lv_vm_begin(LvVm *vm, DoomoMap *map, const DoomoMapExt *x, LvHostFn host, void *ctx)
{
    int i, err;
    uint8_t *b = (uint8_t *)vm;
    for (i = 0; i < (int)sizeof(*vm); i++) b[i] = 0;
    vm->run_h = 255;
    err = x ? lv_script_check(x->script, x->script_len, x) : LVERR_OK;
    if (err) return err;
    vm->x = x;
    vm->map = map;
    vm->host = host;
    vm->ctx = ctx;
    dispatch(vm, LVE_START, 0);
    return LVERR_OK;
}

/* ---- running --------------------------------------------------------------- */
static void say(LvVm *vm, int op, int a, int b)
{
    if (vm->host) vm->host(vm->ctx, op, a, b);
}

static void set_var(LvVm *vm, int v, int val)
{
    if (vm->var[v] == val) return;
    vm->var[v] = (uint8_t)val;
    dispatch(vm, LVE_VAR, v);
}

/* One action at vm->run_pc of the running handler. */
static void step(LvVm *vm)
{
    const uint8_t *p = vm->x->script + vm->run_pc;
    int op = p[0], a = LV_OP_LEN[op] > 0 ? p[1] : 0, b = LV_OP_LEN[op] > 1 ? p[2] : 0;
    vm->run_pc = (uint16_t)(vm->run_pc + 1 + LV_OP_LEN[op]);
    switch (op) {
    case LVO_NOP: break;
    case LVO_SWAP:
        say(vm, op, a, map_swap_apply(vm->map, vm->x, a, &vm->applied));
        break;
    case LVO_UNSWAP:
        say(vm, op, a, map_swap_revert(vm->map, vm->x, a, &vm->applied));
        break;
    case LVO_ZONE: case LVO_TINT: case LVO_FLAG:
        say(vm, op, a, b);
        break;
    case LVO_CAPTION: case LVO_BARK:
        say(vm, op, a | (b << 8), 0);
        break;
    case LVO_CAM:
        if (vm->cam_n) break;                       /* one sweep at a time */
        vm->cam_path = (uint8_t)a;
        vm->cam_n = (uint16_t)(b | (p[3] << 8));
        vm->cam_t = 0;
        vm->cam_tag = p[4];
        say(vm, LVO_CAM, a, 0);
        break;
    case LVO_SET:
        set_var(vm, a, b);
        break;
    case LVO_ADD: {
        int v = vm->var[a] + (int8_t)b;
        set_var(vm, a, v < 0 ? 0 : v > 255 ? 255 : v);
        break;
    }
    case LVO_CHOICE:
        if (vm->choice_open) { vm->err |= LVVE_CHOICE; break; }
        vm->choice_open = 1;
        vm->choice_var = (uint8_t)a;
        vm->choice_tag0 = p[4];
        vm->choice_tag1 = p[5];
        say(vm, op, b | (p[3] << 8), a);
        break;
    case LVO_END:
        vm->ended = 1;
        vm->result = (uint8_t)a;
        vm->q_n = 0;
        vm->run_h = 255;
        say(vm, op, a, 0);
        break;
    case LVO_POST:
        dispatch(vm, LVE_TAG, a);
        break;
    default:                                         /* open close spawn music give take checkpoint */
        say(vm, op, a, 0);
        break;
    }
}

void lv_vm_choose(LvVm *vm, int pick)
{
    int tag;
    if (!vm || !vm->choice_open) return;
    pick = pick ? 1 : 0;
    vm->choice_open = 0;
    tag = pick ? vm->choice_tag1 : vm->choice_tag0;
    set_var(vm, vm->choice_var, pick);
    if (tag) dispatch(vm, LVE_TAG, tag);
}

static void timers(LvVm *vm)
{
    const uint8_t *s = vm->x->script;
    int n = s[0], h, off = 1;
    for (h = 0; h < n; h++) {
        const uint8_t *p = s + off;
        off += LV_VM_HDR + p[4];
        if (p[0] == LVE_TIMER) {
            unsigned t = (unsigned)(p[1] | (p[2] << 8));
            int due = (p[3] & LVSF_REPEAT) ? (vm->tick % t) == 0 : vm->tick == t;
            if (!due) continue;
            if (!(p[3] & LVSF_REPEAT)) {
                if (vm->fired[h >> 3] & (1u << (h & 7))) continue;
                vm->fired[h >> 3] = (uint8_t)(vm->fired[h >> 3] | (1u << (h & 7)));
            }
            enqueue(vm, h);
        }
    }
}

static void zones(LvVm *vm, int cx, int cy)
{
    int i;
    for (i = 0; i < vm->x->zone_count && i < 32; i++) {
        const DoomoZone *z = &vm->x->zones[i];
        int in = cx >= z->x0 && cx <= z->x1 && cy >= z->y0 && cy <= z->y1;
        uint32_t bit = (uint32_t)1 << i;
        if (in && !(vm->zone_in & bit)) {
            vm->zone_in |= bit;
            if (z->tag) dispatch(vm, LVE_ZONE, z->tag);
        } else if (!in) {
            vm->zone_in &= ~bit;
        }
    }
}

void lv_vm_tick(LvVm *vm, int cx, int cy)
{
    int budget = LV_VM_BUDGET;
    if (!vm || !vm->x || !vm->x->script || vm->ended) return;
    vm->tick++;
    if (cx >= 0) zones(vm, cx, cy);
    timers(vm);
    if (vm->cam_n) {
        vm->cam_t++;
        say(vm, LVO_CAM, vm->cam_path, vm->cam_t >= vm->cam_n ? 256 : (vm->cam_t * 256) / vm->cam_n);
        if (vm->cam_t >= vm->cam_n) {
            int tag = vm->cam_tag;
            vm->cam_n = 0;
            if (tag) dispatch(vm, LVE_TAG, tag);
        }
    }
    while (budget > 0 && !vm->ended) {
        if (vm->run_h == 255) {
            int off;
            if (!vm->q_n) break;
            vm->run_h = vm->q[vm->q_head];
            vm->q_head = (uint8_t)((vm->q_head + 1) % LV_VM_QUEUE);
            vm->q_n--;
            off = hdr_of(vm->x->script, vm->x->script_len, vm->run_h);
            vm->run_pc = (uint16_t)(off + LV_VM_HDR);
            vm->run_end = (uint16_t)(vm->run_pc + vm->x->script[off + 4]);
        }
        if (vm->run_pc >= vm->run_end) { vm->run_h = 255; continue; }
        step(vm);
        budget--;
        if (vm->run_h != 255 && vm->run_pc >= vm->run_end) vm->run_h = 255;   /* its last action ran */
    }
}

int lv_vm_frozen(const LvVm *vm) { return vm && vm->cam_n != 0; }

int lv_vm_idle(const LvVm *vm)
{
    return !vm->q_n && vm->run_h == 255 && !vm->cam_n;
}

uint32_t lv_vm_hash(const LvVm *vm)
{
    uint32_t h = 2166136261u;
    int i;
    /* the data, not the pointers (a hash must not depend on addresses) */
    for (i = 0; i < 4; i++) h = (h ^ ((vm->zone_in >> (8 * i)) & 255)) * 16777619u;
    for (i = 0; i < 2; i++) h = (h ^ ((vm->tick >> (8 * i)) & 255)) * 16777619u;
    for (i = 0; i < 2; i++) h = (h ^ ((vm->applied >> (8 * i)) & 255)) * 16777619u;
    for (i = 0; i < 2; i++) h = (h ^ ((vm->run_pc >> (8 * i)) & 255)) * 16777619u;
    for (i = 0; i < 2; i++) h = (h ^ ((vm->cam_t >> (8 * i)) & 255)) * 16777619u;
    for (i = 0; i < 2; i++) h = (h ^ ((vm->cam_n >> (8 * i)) & 255)) * 16777619u;
    for (i = 0; i < LV_VM_VARS; i++) h = (h ^ vm->var[i]) * 16777619u;
    for (i = 0; i < LV_VM_HANDLERS / 8; i++) h = (h ^ vm->fired[i]) * 16777619u;
    for (i = 0; i < LV_VM_QUEUE; i++) h = (h ^ vm->q[(vm->q_head + i) % LV_VM_QUEUE] * (i < vm->q_n)) * 16777619u;
    h = (h ^ vm->q_n) * 16777619u;
    h = (h ^ vm->run_h) * 16777619u;
    h = (h ^ vm->cam_path) * 16777619u;
    h = (h ^ vm->cam_tag) * 16777619u;
    h = (h ^ vm->choice_open) * 16777619u;
    h = (h ^ vm->ended) * 16777619u;
    h = (h ^ vm->result) * 16777619u;
    h = (h ^ vm->err) * 16777619u;
    return h;
}
