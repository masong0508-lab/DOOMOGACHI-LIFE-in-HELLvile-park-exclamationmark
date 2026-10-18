/* scriptvm.c - the story bytecode VM (owner: S2; docs/design/story/SPEC.md
 * sec.4, C6, C9, C14, C18; story_final.md sec.15.2; the language and the
 * op table are in docs/STORY_ENGINE.md).
 *
 * One resident interpreter for every episode, conversation hook and side
 * mission. Resident on the PS1 (the name keeps it out of the story group's
 * regex, SPEC R4), so it NEVER calls overlay code (C6): the ops that need
 * scene code (CAST, NPC, CAM, TELEPORT, INSERT and FREEZE_CAP drawing,
 * BEAT, TINT, NATIVE, the quest UI, REWARD, BEFRIEND, SET_BEAT, POSSESS)
 * come back to the host scene as VM_EXT, and PUSH / HOME DEFEND / VISIT as
 * VM_PUSHED. Everything else runs here: text, flags, vars, waits, audio,
 * camfx, inventory, the house's resident API, time, LATER and the RNG.
 *
 * Dialogue: SAY lines are queued into the dialogue box at once (the
 * decoded, formatted line is copied into the queue entry; nothing resident
 * points into a pack), so a script's lines appear in order before anything
 * the caller queues next. Every op that is not a line, a choice, a flag, a
 * var, a jump or a call waits until the box has closed: a sting, a reward
 * or a scene push lands after the reader dismissed the line before it.
 *
 * Everything advances on the 30 Hz tick; player input only ends WAIT_A and
 * shortens WAIT (never HOLD). No wall clock, no floats, no malloc.
 *
 * RAM: ~84 B of .bss.
 */
#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"
#include "doomo/house_api.h"
#include "doomo/unlock_ids.h"

enum { ST_IDLE, ST_RUN, ST_WAIT, ST_HOLD, ST_WAIT_A, ST_DLG, ST_ROOM, ST_CHOICE,
       ST_PUSHED, ST_BEAT };

#define VM_STACK 4
#define VM_SLICE 96              /* ops per tick before yielding (a runaway guard) */
#define DLG_ROOM 7               /* SAY waits while the box holds this many        */
#define LINE 200                 /* = the dialogue entry (ui_dialog.c DLG_TEXT)     */

static struct {
    const StoryOp *ops;
    uint16_t n, pc, script, chain, wait;
    uint8_t  state, sp;
    int8_t   choice;             /* -1 = not answered yet                           */
    uint8_t  nopt;
    struct { uint16_t script, pc; } stack[VM_STACK];
    int16_t  var[VM_VARS];
    uint16_t labels[4];          /* the open choice's jump labels                   */
    StoryOp  ext;
    uint16_t ext_who, ext_arg, ext_pc;
    uint16_t last_who;
} vm;

int16_t vm_fmt_target = -1;

/* ---- the formatter ------------------------------------------------------------ */
static const char *const vape_flavours[] = {
    "Brimstone Blast", "Unholy Grape", "Cotton Candy Coffin", "Sulfur Sherbet",
    "Midnight Mango", "Graveyard Gum"
};

static int put_s(char *out, int cap, int n, const char *s)
{
    while (s && *s && n < cap - 1) out[n++] = *s++;
    return n;
}

static int put_n(char *out, int cap, int n, int v)
{
    char d[8];
    int k = 0;
    unsigned u;
    if (v < 0 && n < cap - 1) { out[n++] = '-'; v = -v; }
    u = (unsigned)v;
    do { d[k++] = (char)('0' + u % 10u); u /= 10u; } while (u && k < 7);
    while (k && n < cap - 1) out[n++] = d[--k];
    return n;
}

void vm_fmt_n(char *out, int cap, const char *t, const int *nums, int nn)
{
    int n = 0;
    if (cap <= 0) return;
    if (!t) t = "";
    while (*t && n < cap - 1) {
        if (t[0] == '@' && t[1]) {
            const DoomoResident *r;
            char c = t[1];
            if (c >= '0' && c <= '9') {
                int k = c - '0';
                n = put_n(out, cap, n, nums ? (k < nn ? nums[k] : 0) : (k < VM_VARS ? vm.var[k] : 0));
            } else if (c == 'P') {
                r = town_res(DOOMO_PLAYER_ID);
                n = put_s(out, cap, n, (r && r->name[0]) ? r->name : "Friend");
            } else if (c == 'E') {
                n = put_s(out, cap, n, doomo_town.pet.name[0] ? doomo_town.pet.name : "your egg");
            } else if (c == 'D') {
                const char *d = vm_fmt_target >= 0 ? sim_possessor_name(vm_fmt_target) : "";
                n = put_s(out, cap, n, (d && d[0]) ? d : "a Hellvile demon");
            } else if (c == 'T') {
                r = town_res(vm_fmt_target);
                n = put_s(out, cap, n, (r && r->name[0]) ? r->name : "them");
            } else if (c == 'V') {
                n = put_s(out, cap, n, vape_flavours[doomo_town.day % DOOMO_COUNTOF(vape_flavours)]);
            } else if (c == '@') {
                out[n++] = '@';
            } else {
                out[n++] = t[0];
                if (n < cap - 1) out[n++] = c;
            }
            t += 2;
            continue;
        }
        out[n++] = *t++;
    }
    out[n] = 0;
}

void vm_fmt(char *out, int cap, const char *in) { vm_fmt_n(out, cap, in, NULL, 0); }

int vm_who(unsigned code)
{
    if (code == VM_WHO_NAR || code == VM_SAY_NAR) return -1;
    if (code == VM_WHO_TARGET || code == VM_SAY_TARGET) return vm_fmt_target;
    if (code == VM_SAY_PLAYER) return DOOMO_PLAYER_ID;
    return (int)code;
}

/* ---- the RNG and the fingerprint (C18) ---------------------------------------- */
uint32_t story_rng(uint32_t ctx)
{
    uint32_t x = doomo_town.seed ^ ((uint32_t)doomo_town.day * 0x85EBCA6Bu) ^
                 ((uint32_t)doomo_town.story.quest * 0xC2B2AE35u) ^ (ctx * 0x9E3779B9u) ^ 0x5EED5EEDu;
    if (!x) x = 0x6D2B79F5u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}

static uint32_t fnv(uint32_t h, const uint8_t *p, int n)
{
    while (n-- > 0) { h ^= *p++; h *= 16777619u; }
    return h;
}

uint32_t story_fingerprint(void)
{
    const DoomoStry *st = story_stry();
    uint32_t h = 2166136261u;
    uint8_t b[3];
    h = fnv(h, (const uint8_t *)st, (int)sizeof(*st));
    h = fnv(h, doomo_town.story.flags, (int)sizeof(doomo_town.story.flags));
    b[0] = doomo_town.story.chapter;
    b[1] = doomo_town.story.quest;
    b[2] = doomo_town.pet.name[0] ? 1 : 0;
    return fnv(h, b, 3);
}

/* ---- state ---------------------------------------------------------------------- */
static int bind(uint16_t script, uint16_t pc)
{
    int n;
    const StoryOp *o = txt_script(script, &n);
    if (!o && txt_pack_load(TXT_PACK(script))) o = txt_script(script, &n);
    if (!o) {
        plat_log("vm: script not live");
        return 0;
    }
    vm.ops = o;
    vm.n = (uint16_t)n;
    vm.script = script;
    vm.pc = pc;
    return 1;
}

void vm_start(int script_id)
{
    vm.state = ST_IDLE;
    vm.sp = 0;
    vm.chain = 0;
    if (!bind((uint16_t)script_id, 0)) return;
    vm.state = ST_RUN;
}

void vm_stop(void) { vm.state = ST_IDLE; vm.sp = 0; vm.chain = 0; }
int vm_running(void) { return vm.state != ST_IDLE; }
int vm_waiting_dialog(void) { return vm.state == ST_DLG || vm.state == ST_ROOM || vm.state == ST_CHOICE; }
int vm_var(int i) { return (i >= 0 && i < VM_VARS) ? vm.var[i] : 0; }
void vm_set_var(int i, int v) { if (i >= 0 && i < VM_VARS) vm.var[i] = (int16_t)v; }
const StoryOp *vm_ext_op(void) { return vm.ext.op ? &vm.ext : NULL; }
int vm_ext_who(void) { return vm_who(vm.ext_who); }
int vm_ext_arg(void) { return (int16_t)vm.ext_arg; }

void vm_resume(int result)
{
    if (vm.state != ST_PUSHED) return;
    vm.var[0] = (int16_t)result;
    vm.state = ST_RUN;
}

static void choice_done(int c)
{
    if (vm.state == ST_CHOICE) vm.choice = (int8_t)c;
}

/* ---- helpers -------------------------------------------------------------------- */
static int flag(int f) { return (doomo_town.story.flags[(f >> 3) & 31] >> (f & 7)) & 1; }
static void set_flag(int f, int on)
{
    uint8_t *b = &doomo_town.story.flags[(f >> 3) & 31];
    if (on) *b = (uint8_t)(*b | (1u << (f & 7)));
    else *b = (uint8_t)(*b & ~(1u << (f & 7)));
}

static uint16_t data_at(uint16_t pc, int tag, uint16_t dflt)
{
    for (; pc < vm.n && vm.ops[pc].op == OP_DATA; pc++)
        if (vm.ops[pc].a == tag) return vm.ops[pc].b;
    return dflt;
}

int vm_ext_data(int tag, int dflt) { return vm.ext.op ? (int)data_at(vm.ext_pc, tag, (uint16_t)dflt) : dflt; }

static uint16_t skip_data(uint16_t pc)
{
    while (pc < vm.n && vm.ops[pc].op == OP_DATA) pc++;
    return pc;
}

static int now_minutes(void) { return (int)((doomo_town.day * 1440u + doomo_town.minute) & 0xFFFFu); }

static void op_later(int minutes, uint16_t script)
{
    DoomoStry *st = story_stry();
    int i;
    for (i = 0; i < 4; i++)
        if (!st->later[i].script) {
            st->later[i].script = script;
            st->later[i].due = (uint16_t)(now_minutes() + minutes);
            return;
        }
    vm.chain = script;                         /* a fifth fires right after this one */
}

int vm_later_due(void)
{
    DoomoStry *st = story_stry();
    int i, now = now_minutes();
    for (i = 0; i < 4; i++)
        if (st->later[i].script && (int16_t)(uint16_t)(now - st->later[i].due) >= 0) {
            int s = st->later[i].script;
            st->later[i].script = 0;
            return s;
        }
    return 0;
}

static void op_time(int mode, int n)
{
    if (mode == TIME_SET_DAY) {
        if (n > 0) doomo_town.day = (uint16_t)n;
        doomo_town.minute = 9 * 60;
    } else if (mode == TIME_SET_NIGHT) {
        doomo_town.minute = 21 * 60 + 30;
    } else {
        int m = doomo_town.minute + n;
        while (m >= 1440) { m -= 1440; doomo_town.day++; }
        doomo_town.minute = (uint16_t)m;
    }
}

static void say(const StoryOp *o, uint16_t pc_next)
{
    char raw[LINE], line[LINE];
    unsigned who = o->a == VM_SAY_BIG ? data_at(pc_next, VMD_WHO, VM_WHO_NAR) : o->a;
    vm.last_who = (uint16_t)who;
    unsigned fe = data_at(pc_next, VMD_LINE, 0);
    if (txt_get(o->b, raw, (int)sizeof(raw)) < 0) return;
    vm_fmt(line, (int)sizeof(line), raw);
    ui_say_ex(vm_who(who), (int)(fe >> 8), (int)(fe & 0xFF), line);
}

static void choice(const StoryOp *o, uint16_t pc_next)
{
    char raw[LINE], q[LINE], opt[4][28];
    const char *ptr[4];
    int n = DOOMO_MIN(o->a, 4), k = 0;
    unsigned who = data_at(pc_next, VMD_WHO, VM_WHO_NAR);
    uint16_t pc = pc_next;
    if (txt_get(o->b, raw, (int)sizeof(raw)) < 0) raw[0] = 0;
    vm_fmt(q, (int)sizeof(q), raw);
    for (; pc < vm.n && vm.ops[pc].op == OP_DATA; pc++) {
        if (vm.ops[pc].a == VMD_OPT && k < n) {
            if (txt_get(vm.ops[pc].b, raw, (int)sizeof(raw)) < 0) raw[0] = 0;
            vm_fmt(opt[k], (int)sizeof(opt[k]), raw);
            ptr[k] = opt[k];
            k++;
        }
    }
    for (k = 0, pc = pc_next; pc < vm.n && vm.ops[pc].op == OP_DATA; pc++)
        if (vm.ops[pc].a == VMD_LABEL && k < 4) vm.labels[k++] = vm.ops[pc].b;
    vm.nopt = (uint8_t)n;
    vm.choice = -1;
    vm.state = ST_CHOICE;
    ui_choice(vm_who(who), q, ptr, n, choice_done);
}

/* the ops that run without waiting for the dialogue box */
static int no_wait(int op)
{
    switch (op) {
    case OP_SAY: case OP_SAY_X: case OP_CHOICE: case OP_PLAYER_LINE: case OP_DATA:
    case OP_JIF: case OP_JIFN: case OP_JLT: case OP_JGE: case OP_JMP:
    case OP_SETF: case OP_CLRF: case OP_SETV: case OP_ADDV: case OP_CALL: case OP_RET:
    case OP_RNG: case OP_LATER: case OP_GIVE: case OP_TAKE: case OP_QUEST: case OP_STASH:
        return 1;
    default:
        return 0;
    }
}

static int ext(const StoryOp *o, uint16_t pc_next, int result)
{
    vm.ext = *o;
    vm.ext_pc = pc_next;
    vm.ext_who = data_at(pc_next, VMD_WHO, o->a);
    vm.ext_arg = data_at(pc_next, VMD_ARG, data_at(pc_next, VMD_UNLOCK, 0));
    if (result == VM_PUSHED) vm.state = ST_PUSHED;
    return result;
}

static void home_op(int sub, uint16_t b)
{
    switch (sub) {
    case HOME_SET_PHASE: home_set_phase(b); break;
    case HOME_PLACE_GIFT: vm.var[0] = (int16_t)home_place_gift(b); break;
    case HOME_REMOVE_ONLY: home_remove_only(b); break;
    case HOME_GOAL: vm.var[0] = (int16_t)home_goal_check(b); break;
    case HOME_DECORATE_FOR: vm.var[0] = (int16_t)home_decorate_for(b & 0xFFF, b >> 12); break;
    case HOME_HAUNT_STEP: home_haunt_step(b); break;
    case HOME_GRANT: if (b != 0xFFFFu) vm.var[0] = (int16_t)home_grant(b); break;
    case HOME_PROP_GAG: home_prop_gag(b); break;
    default: break;
    }
}

/* ---- the interpreter ---------------------------------------------------------------- */
int vm_tick(void)
{
    int slice = VM_SLICE;
    vm.ext.op = 0;
    /* (S2) the script's pack left the window (PS1: a cutscene or the HUD read
     * another pack into the slot): read it back before the next op */
    if (vm.state != ST_IDLE && !txt_pack_is_live(TXT_PACK(vm.script)) && !bind(vm.script, vm.pc)) {
        vm_stop();
        return VM_IDLE;
    }
    for (;;) {
        const StoryOp *o;
        uint16_t pc, nx;
        switch (vm.state) {
        case ST_IDLE: return VM_IDLE;
        case ST_PUSHED: return VM_PUSHED;
        case ST_WAIT:
            if (btn_pressed(BTN_A)) vm.wait = 0;
            /* fall through */
        case ST_HOLD:
            if (vm.wait > 0) { vm.wait--; return VM_RUN; }
            vm.state = ST_RUN;
            break;
        case ST_WAIT_A:
            if (vm.wait > 0) { vm.wait--; return VM_RUN; }
            if (!btn_pressed(BTN_A)) return VM_RUN;
            vm.state = ST_RUN;
            break;
        case ST_DLG:
            if (ui_dialog_active()) return VM_RUN;
            vm.state = ST_RUN;
            break;
        case ST_ROOM:
            if (ui_dialog_active() && ui_dialog_queued() >= DLG_ROOM) return VM_RUN;
            vm.state = ST_RUN;
            break;
        case ST_CHOICE:
            if (vm.choice < 0) return VM_RUN;
            vm.var[0] = vm.choice;
            vm.state = ST_RUN;
            if (vm.choice < vm.nopt && vm.labels[(int)vm.choice] != 0xFFFFu) vm.pc = vm.labels[(int)vm.choice];
            break;
        case ST_BEAT:
            if (beat_running()) return VM_RUN;
            vm.state = ST_RUN;
            break;
        default: break;
        }
        if (--slice <= 0) return VM_RUN;
        if (vm.pc >= vm.n) { vm.state = ST_IDLE; return VM_IDLE; }
        pc = vm.pc;
        o = &vm.ops[pc];
        nx = skip_data((uint16_t)(pc + 1));
        if (!no_wait(o->op) && ui_dialog_active()) { vm.state = ST_DLG; return VM_RUN; }
        vm.pc = nx;
        switch (o->op) {
        case OP_END:
            if (vm.sp) {                                 /* END inside a CALL = RET */
                vm.sp--;
                if (!bind(vm.stack[vm.sp].script, vm.stack[vm.sp].pc)) { vm.state = ST_IDLE; return VM_IDLE; }
                break;
            }
            if (vm.chain) {
                uint16_t c = vm.chain;
                vm.chain = 0;
                if (bind(c, 0)) break;
            }
            vm.state = ST_IDLE;
            return VM_IDLE;
        case OP_SAY:
            if (ui_dialog_queued() >= DLG_ROOM) { vm.pc = pc; vm.state = ST_ROOM; return VM_RUN; }
            say(o, (uint16_t)(pc + 1));
            break;
        case OP_SAY_X: {
            char raw[LINE], line[LINE];                  /* the last speaker, this expression */
            if (ui_dialog_queued() >= DLG_ROOM) { vm.pc = pc; vm.state = ST_ROOM; return VM_RUN; }
            if (txt_get(o->b, raw, (int)sizeof(raw)) >= 0) {
                vm_fmt(line, (int)sizeof(line), raw);
                ui_say_ex(vm_who(vm.last_who), o->a, 0, line);
            }
            break; }
        case OP_CHOICE: case OP_PLAYER_LINE:
            choice(o, (uint16_t)(pc + 1));
            return VM_RUN;
        case OP_JIF: if (flag(o->a)) vm.pc = o->b; break;
        case OP_JIFN: if (!flag(o->a)) vm.pc = o->b; break;
        case OP_JLT: if (vm_var(o->a) < (int16_t)data_at((uint16_t)(pc + 1), VMD_CMP, 0)) vm.pc = o->b; break;
        case OP_JGE: if (vm_var(o->a) >= (int16_t)data_at((uint16_t)(pc + 1), VMD_CMP, 0)) vm.pc = o->b; break;
        case OP_JMP: vm.pc = o->b; break;
        case OP_SETF: set_flag(o->a, 1); break;
        case OP_CLRF: set_flag(o->a, 0); break;
        case OP_SETV: vm_set_var(o->a, (int16_t)o->b); break;
        case OP_ADDV: vm_set_var(o->a, vm_var(o->a) + (int16_t)o->b); break;
        case OP_WAIT: vm.wait = o->a; vm.state = ST_WAIT; return VM_RUN;
        case OP_HOLD: vm.wait = o->a; vm.state = ST_HOLD; return VM_RUN;
        case OP_WAIT_A: vm.wait = o->a; vm.state = ST_WAIT_A; return VM_RUN;
        case OP_GIVE: inv_add(o->a, o->b ? o->b : 1); break;
        case OP_TAKE: inv_take(o->a, o->b ? o->b : 1); break;
        case OP_SFX:
            if (o->b >= SFX_SYNTH_FIRST) audio_sfx_synth(o->b);
            else audio_sfx(o->b);
            break;
        case OP_MUSIC:
            audio_music(o->b);
            if (doomo_town.mode == TOWN_STORY && o->b >= SONG_MINGLE && o->b < SONG_COUNT) {
                int id = UL_STORY_SONG2(o->b);          /* heard: into the jukebox, quietly */
                if (unlock_by_id(id)) doomo_town.unlocks[id >> 3] |= (uint8_t)(1u << (id & 7));
            }
            break;
        case OP_MUTE: audio_music_mute(o->b == 0xFFFFu ? 0x7FFF : o->b); break;
        case OP_DUCK: audio_music_duck(o->a, o->b); break;
        case OP_TAPESTOP: audio_tapestop(o->b); break;
        case OP_RESUME_ON_BEAT: audio_music_resume_on_beat(); break;
        case OP_SHAKE: camfx_shake(o->a); break;
        case OP_PUNCH: camfx_punch((int8_t)o->a, (int16_t)o->b); break;
        case OP_FLASH: camfx_flash(o->a, o->b & 0xFF, o->b >> 8); break;
        case OP_FADE: camfx_fade(o->b); break;
        case OP_IRIS:
            if (o->b) camfx_iris(plat_screen_w() / 2, plat_screen_h() / 2, o->b, o->a, PAL_BLACK);
            else camfx_iris_off();
            break;
        case OP_PAL_LERP: camfx_pal_lerp(o->a, o->b); break;
        case OP_HOME:
            if (o->a == HOME_DEFEND || o->a == HOME_VISIT) return ext(o, (uint16_t)(pc + 1), VM_PUSHED);
            home_op(o->a, o->b);
            break;
        case OP_QUEST: {
            int q = (o->b & 0xFFF) >= 100 ? (o->b & 0xFFF) - 100 : (o->b & 0xFFF);
            story_side2_set_state(q, o->b >> 12);
            break; }
        case OP_CALL:
            if (vm.sp >= VM_STACK) { plat_log("vm: call stack full"); break; }
            vm.stack[vm.sp].script = vm.script;
            vm.stack[vm.sp].pc = vm.pc;
            vm.sp++;
            if (!bind(o->b, 0)) { vm.sp--; break; }
            break;
        case OP_RET:
            if (!vm.sp) { vm.state = ST_IDLE; return VM_IDLE; }
            vm.sp--;
            if (!bind(vm.stack[vm.sp].script, vm.stack[vm.sp].pc)) { vm.state = ST_IDLE; return VM_IDLE; }
            break;
        case OP_RNG: vm_set_var(o->a, (int)(story_rng(0x524E0000u | o->b) & 0x7FFFu)); break;
        case OP_LATER: op_later(o->a, o->b); break;
        case OP_TIME: op_time(o->a, o->b); break;
        case OP_STASH: txt_stash(o->a, o->b); break;      /* C5: resident captions */
        case OP_DATA: break;
        case OP_PUSH:
            return ext(o, (uint16_t)(pc + 1), VM_PUSHED);
        case OP_BEAT:
            vm.state = ST_BEAT;
            return ext(o, (uint16_t)(pc + 1), VM_EXT);
        default:                                         /* host kinds (C6) */
            return ext(o, (uint16_t)(pc + 1), VM_EXT);
        }
    }
}
