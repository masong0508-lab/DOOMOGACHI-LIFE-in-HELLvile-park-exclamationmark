/* test_lv_script.c - LV2 (owner: levels): the level script VM (core/src/lv_script.c).
 * A test per action and per event on hand-assembled bytecode, the budgets, the script checker, and
 * the script of core/tests/maps/t2_script.txt (compiled by tools/mapc.py) driven twice: both runs
 * must hash the same. The host function only records its calls. */
#include <stdarg.h>
#include "test.h"
#include "../src/levels_int.h"

extern const DoomoMapExt map_ext_t2_script;

/* ---- the recording host --------------------------------------------------- */
typedef struct { int op, a, b; } Call;
static Call g_log[512];
static int  g_n;
static uint32_t g_opmask;

static void host(void *ctx, int op, int a, int b)
{
    (void)ctx;
    if (g_n < 512) { g_log[g_n].op = op; g_log[g_n].a = a; g_log[g_n].b = b; g_n++; }
    g_opmask |= 1u << op;
}

static uint32_t log_hash(void)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < g_n; i++) {
        h = (h ^ (uint32_t)g_log[i].op) * 16777619u;
        h = (h ^ (uint32_t)g_log[i].a) * 16777619u;
        h = (h ^ (uint32_t)g_log[i].b) * 16777619u;
    }
    return h;
}

static int count_op(int op)
{
    int i, n = 0;
    for (i = 0; i < g_n; i++) n += g_log[i].op == op;
    return n;
}

/* ---- a bytecode builder ----------------------------------------------------- */
typedef struct { uint8_t b[4096]; int n, hdr; } SB;

static void sb_begin(SB *s) { s->b[0] = 0; s->n = 1; s->hdr = -1; }
static void sb_h(SB *s, int ev, int a, int b, int fl)
{
    s->hdr = s->n;
    s->b[s->n++] = (uint8_t)ev; s->b[s->n++] = (uint8_t)a; s->b[s->n++] = (uint8_t)b;
    s->b[s->n++] = (uint8_t)fl; s->b[s->n++] = 0;
    s->b[0]++;
}
static void sb_a(SB *s, int op, int nargs, ...)
{
    va_list ap;
    int i;
    va_start(ap, nargs);
    s->b[s->n++] = (uint8_t)op;
    for (i = 0; i < nargs; i++) s->b[s->n++] = (uint8_t)va_arg(ap, int);
    va_end(ap);
    s->b[s->hdr + 4] = (uint8_t)(s->n - s->hdr - LV_VM_HDR);
}

static DoomoMapExt g_x;
static DoomoMap    g_map;
static LvVm        g_vm;

/* Start a VM on the t2_script map with the script of `s`. Returns lv_vm_begin()'s result. */
static int start(const SB *s)
{
    const DoomoMapDef *d = map_ext_t2_script.def;
    g_x = map_ext_t2_script;
    g_x.script = s->b;
    g_x.script_len = (uint16_t)s->n;
    memset(&g_map, 0, sizeof(g_map));
    g_map.w = d->w;
    g_map.h = d->h;
    memcpy(g_map.cells, d->cells, (size_t)d->w * d->h * sizeof(DoomoCell));
    g_n = 0;
    g_opmask = 0;
    return lv_vm_begin(&g_vm, &g_map, &g_x, host, NULL);
}

static void ticks(int n) { while (n-- > 0) lv_vm_tick(&g_vm, -1, -1); }

/* one handler `on start do <op args>`: begin + one tick */
#define ONE(sb, op, nargs, ...) do { sb_begin(&sb); sb_h(&sb, LVE_START, 0, 0, 0); \
        sb_a(&sb, op, nargs, __VA_ARGS__); CHECK_EQ(start(&sb), LVERR_OK); ticks(1); } while (0)

/* ---- the checker and the budgets --------------------------------------------- */
TEST(lvs_check_accepts_and_refuses)
{
    SB s;
    int i;
    CHECK_EQ(lv_script_check(NULL, 0, NULL), LVERR_OK);
    CHECK_EQ(lv_script_check(map_ext_t2_script.script, map_ext_t2_script.script_len, &map_ext_t2_script), LVERR_OK);
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_SET, 2, 0, 1);
    CHECK_EQ(lv_script_check(s.b, s.n, &map_ext_t2_script), LVERR_OK);
    CHECK_EQ(lv_script_check(s.b, s.n - 1, &map_ext_t2_script), LVERR_FORMAT);       /* cut short */
    CHECK_EQ(lv_script_check(s.b, s.n + 1, &map_ext_t2_script), LVERR_FORMAT);       /* trailing byte */
    s.b[s.hdr] = LVE_COUNT;
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_FORMAT);                         /* unknown event */
    s.b[s.hdr] = LVE_START; s.b[s.hdr + 3] = 2;
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_FORMAT);                         /* unknown flag */
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_COUNT, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_OP);
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_SET, 2, LV_VM_VARS, 1);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_OPERAND);                        /* variable 32 */
    sb_begin(&s); sb_h(&s, LVE_VAR, LV_VM_VARS, 1, 0); sb_a(&s, LVO_NOP, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_OPERAND);
    sb_begin(&s); sb_h(&s, LVE_TIMER, 0, 0, 0); sb_a(&s, LVO_NOP, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_OPERAND);                        /* timer 0 */
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_SWAP, 1, 2);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_OK);                             /* no ext: no set check */
    CHECK_EQ(lv_script_check(s.b, s.n, &map_ext_t2_script), LVERR_OPERAND);          /* the map has 2 sets */
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_ZONE, 2, 2, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, &map_ext_t2_script), LVERR_OPERAND);          /* the map has 2 zones */
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_CAM, 4, 9, 4, 0, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, &map_ext_t2_script), LVERR_OPERAND);          /* no path 9 */
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_CAM, 4, 3, 0, 0, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, &map_ext_t2_script), LVERR_OPERAND);          /* 0 ticks */
    /* the size budgets: 64 handlers fit, 65 do not; the byte budget is 2048 */
    sb_begin(&s);
    for (i = 0; i < LV_VM_HANDLERS; i++) sb_h(&s, LVE_TAG, 1, 0, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_OK);
    sb_h(&s, LVE_TAG, 1, 0, 0);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_SIZE);
    sb_begin(&s);
    for (i = 0; i < 9; i++) { sb_h(&s, LVE_TAG, 1, 0, 0); while (s.b[s.hdr + 4] < 250) sb_a(&s, LVO_NOP, 0); }
    CHECK(s.n > LV_VM_BYTES);
    CHECK_EQ(lv_script_check(s.b, s.n, NULL), LVERR_SIZE);
    /* lv_vm_begin refuses a bad script and stays inert */
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_COUNT, 0);
    CHECK_EQ(start(&s), LVERR_OP);
    ticks(3);
    CHECK_EQ(g_n, 0);
}

TEST(lvs_state_budget)
{
    /* the plan: at most 64 B of mechanic state per level, 96 B with the 32 variables; the VM's own data */
    CHECK((int)(sizeof(LvVm) - 4 * sizeof(void *)) <= 96);
    CHECK_EQ(LV_VM_VARS, 32);
    CHECK_EQ(LV_VM_BUDGET, 16);
}

TEST(lvs_actions_per_tick_budget)
{
    SB s;
    int i;
    sb_begin(&s); sb_h(&s, LVE_START, 0, 0, 0);
    for (i = 0; i < 40; i++) sb_a(&s, LVO_ADD, 2, 0, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    ticks(1); CHECK_EQ(g_vm.var[0], 16);               /* 16 actions a tick: the rest waits */
    CHECK(!lv_vm_idle(&g_vm));
    ticks(1); CHECK_EQ(g_vm.var[0], 32);
    ticks(1); CHECK_EQ(g_vm.var[0], 40);
    CHECK(lv_vm_idle(&g_vm));
    ticks(5); CHECK_EQ(g_vm.var[0], 40);               /* once: it never runs again */
}

TEST(lvs_queue_budget_drops_deterministically)
{
    SB s;
    int i;
    sb_begin(&s); sb_h(&s, LVE_TAG, 1, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 0, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    for (i = 0; i < 20; i++) lv_vm_event(&g_vm, LVE_TAG, 1);
    CHECK(g_vm.err & LVVE_QUEUE);                      /* 20 woken, 16 fit */
    ticks(1);
    CHECK_EQ(g_vm.var[0], LV_VM_QUEUE);
    CHECK(lv_vm_idle(&g_vm));
}

/* ---- one test per action -------------------------------------------------------- */
TEST(lvs_action_nop)
{
    SB s;
    ONE(s, LVO_NOP, 0, 0);
    CHECK_EQ(g_n, 0);
    CHECK(lv_vm_idle(&g_vm));
}

TEST(lvs_action_open_close)
{
    SB s;
    ONE(s, LVO_OPEN, 1, 9);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_OPEN); CHECK_EQ(g_log[0].a, 9);
    ONE(s, LVO_CLOSE, 1, 4);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_CLOSE); CHECK_EQ(g_log[0].a, 4);
}

TEST(lvs_action_swap_unswap)
{
    SB s;
    const DoomoMapDef *d = map_ext_t2_script.def;
    size_t bytes = (size_t)d->w * d->h * sizeof(DoomoCell);
    int cell = 1 * d->w + 5;                           /* the gate's cell */
    sb_begin(&s);
    sb_h(&s, LVE_TAG, 1, 0, LVSF_REPEAT); sb_a(&s, LVO_SWAP, 1, 0);
    sb_h(&s, LVE_TAG, 2, 0, LVSF_REPEAT); sb_a(&s, LVO_UNSWAP, 1, 0);
    CHECK_EQ(start(&s), LVERR_OK);
    CHECK(g_map.cells[cell].wall != 0);
    lv_vm_event(&g_vm, LVE_TAG, 1); ticks(1);
    CHECK(g_map.cells[cell].wall == 0);                /* opened */
    CHECK_EQ(g_vm.applied, 1);
    CHECK_EQ(g_log[0].op, LVO_SWAP); CHECK_EQ(g_log[0].a, 0); CHECK_EQ(g_log[0].b, 1);   /* 1 cell changed */
    lv_vm_event(&g_vm, LVE_TAG, 1); ticks(1);
    CHECK_EQ(g_log[1].b, 0);                           /* already applied: nothing changes */
    lv_vm_event(&g_vm, LVE_TAG, 2); ticks(1);
    CHECK_EQ(g_vm.applied, 0);
    CHECK(memcmp(g_map.cells, d->cells, bytes) == 0);  /* reverted exactly */
    CHECK_EQ(g_log[2].op, LVO_UNSWAP); CHECK_EQ(g_log[2].b, 1);
}

TEST(lvs_action_zone)
{
    SB s;
    ONE(s, LVO_ZONE, 2, 1, 77);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_ZONE); CHECK_EQ(g_log[0].a, 1); CHECK_EQ(g_log[0].b, 77);
}

TEST(lvs_action_spawn)
{
    SB s;
    ONE(s, LVO_SPAWN, 1, 6);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_SPAWN); CHECK_EQ(g_log[0].a, 6);
}

TEST(lvs_action_caption_bark)
{
    SB s;
    ONE(s, LVO_CAPTION, 2, 0x34, 0x12);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_CAPTION); CHECK_EQ(g_log[0].a, 0x1234);
    ONE(s, LVO_BARK, 2, 7, 0);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_BARK); CHECK_EQ(g_log[0].a, 7);
}

TEST(lvs_action_music_tint)
{
    SB s;
    ONE(s, LVO_MUSIC, 1, 3);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_MUSIC); CHECK_EQ(g_log[0].a, 3);
    ONE(s, LVO_TINT, 2, 12, 5);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_TINT); CHECK_EQ(g_log[0].a, 12); CHECK_EQ(g_log[0].b, 5);
}

TEST(lvs_action_camera_sweep)
{
    SB s;
    int i;
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_CAM, 4, 3, 4, 0, 8);      /* path 3, 4 ticks, done tag 8 */
    sb_h(&s, LVE_TAG, 8, 0, 0); sb_a(&s, LVO_SET, 2, 0, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    CHECK(!lv_vm_frozen(&g_vm));
    ticks(1);
    CHECK(lv_vm_frozen(&g_vm));                        /* input frozen */
    CHECK_EQ(g_log[0].op, LVO_CAM); CHECK_EQ(g_log[0].a, 3); CHECK_EQ(g_log[0].b, 0);
    for (i = 1; i <= 3; i++) {
        ticks(1);
        CHECK(lv_vm_frozen(&g_vm));
        CHECK_EQ(g_log[i].b, i * 64);                  /* progress 0..256 */
        CHECK_EQ(g_vm.var[0], 0);
    }
    ticks(1);
    CHECK_EQ(g_log[4].b, 256);
    CHECK(!lv_vm_frozen(&g_vm));
    CHECK_EQ(g_vm.var[0], 1);                          /* the done tag fired and its handler ran */
    CHECK_EQ(g_n, 5);
}

TEST(lvs_action_set_add)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0);
    sb_a(&s, LVO_SET, 2, 3, 200);
    sb_a(&s, LVO_ADD, 2, 3, 100);                      /* 300: clamps to 255 */
    sb_a(&s, LVO_SET, 2, 4, 5);
    sb_a(&s, LVO_ADD, 2, 4, (uint8_t)-9);              /* -4: clamps to 0 */
    sb_a(&s, LVO_SET, 2, 5, 10);
    sb_a(&s, LVO_ADD, 2, 5, (uint8_t)-3);
    sb_a(&s, LVO_SET, 2, 31, 77);                      /* the last variable */
    CHECK_EQ(start(&s), LVERR_OK);
    ticks(1);
    CHECK_EQ(g_vm.var[3], 255); CHECK_EQ(g_vm.var[4], 0); CHECK_EQ(g_vm.var[5], 7); CHECK_EQ(g_vm.var[31], 77);
    CHECK_EQ(g_n, 0);                                  /* variables never leave the VM */
}

TEST(lvs_action_give_take)
{
    SB s;
    ONE(s, LVO_GIVE, 1, 4);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_GIVE); CHECK_EQ(g_log[0].a, 4);
    ONE(s, LVO_TAKE, 1, 2);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_TAKE); CHECK_EQ(g_log[0].a, 2);
}

TEST(lvs_action_checkpoint)
{
    SB s;
    ONE(s, LVO_CHECKPOINT, 0, 0);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_CHECKPOINT);
}

TEST(lvs_action_choice)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_CHOICE, 5, 6, 0x2C, 0x01, 12, 13);    /* var 6, text 300 */
    sb_h(&s, LVE_TAG, 12, 0, 0); sb_a(&s, LVO_SET, 2, 0, 1);
    sb_h(&s, LVE_TAG, 13, 0, 0); sb_a(&s, LVO_SET, 2, 0, 2);
    CHECK_EQ(start(&s), LVERR_OK);
    lv_vm_choose(&g_vm, 1);                            /* nothing open yet: ignored */
    ticks(1);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_CHOICE); CHECK_EQ(g_log[0].a, 300); CHECK_EQ(g_log[0].b, 6);
    CHECK_EQ(g_vm.var[0], 0);
    lv_vm_choose(&g_vm, 1);
    CHECK_EQ(g_vm.var[6], 1);
    ticks(1);
    CHECK_EQ(g_vm.var[0], 2);                          /* pick 1 fired tag 13 */
    lv_vm_choose(&g_vm, 0);                            /* closed: ignored */
    CHECK_EQ(g_vm.var[6], 1);
    /* pick 0 fires tag 12 */
    CHECK_EQ(start(&s), LVERR_OK);
    ticks(1); lv_vm_choose(&g_vm, 0); ticks(1);
    CHECK_EQ(g_vm.var[6], 0); CHECK_EQ(g_vm.var[0], 1);
    /* a second choice while one is open is refused, once, deterministically */
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_CHOICE, 5, 6, 1, 0, 0, 0); sb_a(&s, LVO_CHOICE, 5, 7, 2, 0, 0, 0);
    CHECK_EQ(start(&s), LVERR_OK);
    ticks(1);
    CHECK_EQ(g_n, 1);
    CHECK(g_vm.err & LVVE_CHOICE);
}

TEST(lvs_action_flag)
{
    SB s;
    ONE(s, LVO_FLAG, 2, 6, 1);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_FLAG); CHECK_EQ(g_log[0].a, 6); CHECK_EQ(g_log[0].b, 1);
    ONE(s, LVO_FLAG, 2, 7, 0);
    CHECK_EQ(g_log[0].a, 7); CHECK_EQ(g_log[0].b, 0);
}

TEST(lvs_action_end)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_END, 1, 3); sb_a(&s, LVO_ADD, 2, 0, 1);
    sb_h(&s, LVE_TAG, 1, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 1, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    lv_vm_event(&g_vm, LVE_TAG, 1);                    /* queued behind the first handler */
    ticks(3);
    CHECK(g_vm.ended); CHECK_EQ(g_vm.result, 3);
    CHECK_EQ(g_n, 1); CHECK_EQ(g_log[0].op, LVO_END); CHECK_EQ(g_log[0].a, 3);
    CHECK_EQ(g_vm.var[0], 0); CHECK_EQ(g_vm.var[1], 0);    /* nothing runs after the end */
    lv_vm_event(&g_vm, LVE_TAG, 1); ticks(2);
    CHECK_EQ(g_vm.var[1], 0);
}

TEST(lvs_action_post)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_POST, 1, 20);
    sb_h(&s, LVE_TAG, 20, 0, 0); sb_a(&s, LVO_POST, 1, 21);
    sb_h(&s, LVE_TAG, 21, 0, 0); sb_a(&s, LVO_SET, 2, 0, 5);
    CHECK_EQ(start(&s), LVERR_OK);
    ticks(1);
    CHECK_EQ(g_vm.var[0], 5);                          /* a chain of three handlers in one tick */
}

/* ---- one test per event --------------------------------------------------------- */
TEST(lvs_event_start_tag_once_and_repeat)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_START, 0, 0, 0); sb_a(&s, LVO_ADD, 2, 0, 1);
    sb_h(&s, LVE_TAG, 4, 0, 0); sb_a(&s, LVO_ADD, 2, 1, 1);                        /* once */
    sb_h(&s, LVE_TAG, 4, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 2, 1);              /* every time */
    CHECK_EQ(start(&s), LVERR_OK);
    ticks(1);
    CHECK_EQ(g_vm.var[0], 1);
    lv_vm_event(&g_vm, LVE_TAG, 5); ticks(1);
    CHECK_EQ(g_vm.var[1], 0);                          /* another tag */
    lv_vm_event(&g_vm, LVE_TAG, 4); ticks(1);
    lv_vm_event(&g_vm, LVE_TAG, 4); ticks(1);
    lv_vm_event(&g_vm, LVE_TAG, 4); ticks(1);
    CHECK_EQ(g_vm.var[1], 1); CHECK_EQ(g_vm.var[2], 3);
    CHECK_EQ(g_vm.var[0], 1);
    lv_vm_event(&g_vm, LVE_NONE, 4); lv_vm_event(&g_vm, LVE_COUNT, 4); lv_vm_event(&g_vm, 200, 4); ticks(1);
    CHECK_EQ(g_vm.var[2], 3);                          /* bad events are ignored */
}

TEST(lvs_event_kill_group_use_phase)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_CLEAR, 4, 0, 0); sb_a(&s, LVO_SET, 2, 0, 1);
    sb_h(&s, LVE_USE, 1, 0, 0); sb_a(&s, LVO_SET, 2, 1, 1);
    sb_h(&s, LVE_PHASE, 2, 0, 0); sb_a(&s, LVO_SET, 2, 2, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    lv_vm_event(&g_vm, LVE_CLEAR, 3); lv_vm_event(&g_vm, LVE_USE, 0); lv_vm_event(&g_vm, LVE_PHASE, 1); ticks(1);
    CHECK_EQ(g_vm.var[0] + g_vm.var[1] + g_vm.var[2], 0);
    lv_vm_event(&g_vm, LVE_CLEAR, 4); ticks(1);
    CHECK_EQ(g_vm.var[0], 1); CHECK_EQ(g_vm.var[1], 0); CHECK_EQ(g_vm.var[2], 0);
    lv_vm_event(&g_vm, LVE_USE, 1); ticks(1);
    CHECK_EQ(g_vm.var[1], 1); CHECK_EQ(g_vm.var[2], 0);
    lv_vm_event(&g_vm, LVE_PHASE, 2); ticks(1);
    CHECK_EQ(g_vm.var[2], 1);
}

TEST(lvs_event_timer_once_and_repeat)
{
    SB s;
    int t;
    sb_begin(&s);
    sb_h(&s, LVE_TIMER, 3, 0, 0); sb_a(&s, LVO_ADD, 2, 0, 1);
    sb_h(&s, LVE_TIMER, 2, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 1, 1);
    sb_h(&s, LVE_TIMER, 0, 1, 0); sb_a(&s, LVO_ADD, 2, 2, 1);                      /* 256 ticks */
    CHECK_EQ(start(&s), LVERR_OK);
    for (t = 1; t <= 10; t++) {
        ticks(1);
        CHECK_EQ(g_vm.var[0], t >= 3 ? 1 : 0);
        CHECK_EQ(g_vm.var[1], t / 2);
    }
    ticks(246);
    CHECK_EQ(g_vm.tick, 256);
    CHECK_EQ(g_vm.var[2], 1);
    ticks(10);
    CHECK_EQ(g_vm.var[2], 1);
}

TEST(lvs_event_zone_enter)
{
    SB s;
    sb_begin(&s);
    sb_h(&s, LVE_ZONE, 6, 0, 0); sb_a(&s, LVO_ADD, 2, 0, 1);                       /* once */
    sb_h(&s, LVE_ZONE, 6, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 1, 1);             /* every entry */
    sb_h(&s, LVE_ZONE, 5, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 2, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    lv_vm_tick(&g_vm, 6, 2);                           /* outside both */
    lv_vm_tick(&g_vm, 9, 2);                           /* into zone tag 6 */
    lv_vm_tick(&g_vm, 9, 3);                           /* still in: no new event */
    CHECK_EQ(g_vm.var[0], 1); CHECK_EQ(g_vm.var[1], 1); CHECK_EQ(g_vm.var[2], 0);
    lv_vm_tick(&g_vm, 6, 2);                           /* out */
    lv_vm_tick(&g_vm, 10, 1);                          /* in again (its corner: inclusive) */
    CHECK_EQ(g_vm.var[0], 1); CHECK_EQ(g_vm.var[1], 2);
    lv_vm_tick(&g_vm, 2, 2);                           /* the other zone */
    CHECK_EQ(g_vm.var[2], 1);
    lv_vm_tick(&g_vm, -1, -1);                         /* no player: no change, no event */
    CHECK_EQ(g_vm.var[1], 2);
}

TEST(lvs_event_var_reaches_value)
{
    SB s;
    int i;
    sb_begin(&s);
    sb_h(&s, LVE_TAG, 1, 0, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 0, 1);
    sb_h(&s, LVE_VAR, 0, 3, 0); sb_a(&s, LVO_SET, 2, 1, 1);
    sb_h(&s, LVE_VAR, 0, 3, LVSF_REPEAT); sb_a(&s, LVO_ADD, 2, 2, 1);
    CHECK_EQ(start(&s), LVERR_OK);
    for (i = 0; i < 2; i++) { lv_vm_event(&g_vm, LVE_TAG, 1); ticks(1); }
    CHECK_EQ(g_vm.var[0], 2); CHECK_EQ(g_vm.var[1], 0);
    lv_vm_event(&g_vm, LVE_TAG, 1); ticks(1);          /* the variable becomes 3 */
    CHECK_EQ(g_vm.var[1], 1); CHECK_EQ(g_vm.var[2], 1);
    lv_vm_event(&g_vm, LVE_TAG, 1); ticks(1);          /* 4: not the value */
    CHECK_EQ(g_vm.var[2], 1);
    lv_vm_event(&g_vm, LVE_VAR, 40); ticks(1);         /* a stray event: no overrun, nothing runs */
    CHECK_EQ(g_vm.var[2], 1);
}

/* ---- the compiled script, driven twice ------------------------------------------- */
typedef struct { uint32_t vm, log; int ended, result, n; } Out;

static void drive(Out *o, int extra_event)
{
    const DoomoMapDef *d = map_ext_t2_script.def;
    int t;
    g_x = map_ext_t2_script;
    memset(&g_map, 0, sizeof(g_map));
    g_map.w = d->w;
    g_map.h = d->h;
    memcpy(g_map.cells, d->cells, (size_t)d->w * d->h * sizeof(DoomoCell));
    g_n = 0;
    g_opmask = 0;
    if (lv_vm_begin(&g_vm, &g_map, &g_x, host, NULL) != LVERR_OK) { o->n = -1; return; }
    for (t = 1; t <= 60; t++) {
        int px = (t >= 3 && t < 6) || t >= 8 ? 9 : 6, py = 2;
        if (t == 10) lv_vm_event(&g_vm, LVE_TAG, 5);
        if (t == 12) lv_vm_event(&g_vm, LVE_CLEAR, 4);
        if (t == 14) lv_vm_event(&g_vm, LVE_USE, 1);
        if (t == 16) lv_vm_event(&g_vm, LVE_PHASE, 2);
        if (t == 20) lv_vm_event(&g_vm, LVE_TAG, 7);
        if (t == 22 && extra_event) lv_vm_event(&g_vm, LVE_TAG, 5);       /* once: ignored */
        if (t == 24 && extra_event == 2) lv_vm_event(&g_vm, LVE_PHASE, 2);
        if (t == 30 && g_vm.choice_open) lv_vm_choose(&g_vm, 0);
        lv_vm_tick(&g_vm, px, py);
    }
    o->vm = lv_vm_hash(&g_vm);
    o->log = log_hash();
    o->ended = g_vm.ended;
    o->result = g_vm.result;
    o->n = g_n;
}

TEST(lvs_compiled_script_runs)
{
    Out o;
    const DoomoMapDef *d = map_ext_t2_script.def;
    uint32_t want = 0, op;
    CHECK(map_ext_t2_script.script != NULL);
    CHECK_EQ(map_ext_t2_script.script_len, map_ext_t2_script.script[0] ? map_ext_t2_script.script_len : 0);
    CHECK_EQ(map_ext_t2_script.script[0], 13);                      /* 13 `on` lines */
    drive(&o, 0);
    CHECK(o.n > 0);
    CHECK(g_vm.ended); CHECK_EQ(g_vm.result, 2);
    CHECK_EQ(g_vm.var[0], 1);                          /* on start */
    CHECK_EQ(g_vm.var[1], 4);                          /* two entries of zone tag 6, +2 each */
    CHECK_EQ(g_vm.var[2], 9);                          /* use */
    CHECK_EQ(g_vm.var[3], 1);                          /* phase */
    CHECK_EQ(g_vm.var[4], 5);                          /* timer 5 repeat: ticks 5..25, then the end at 30 */
    CHECK_EQ(g_vm.var[5], 0);                          /* the choice: pick 0 */
    CHECK_EQ(g_vm.applied, 2);                         /* gate applied then reverted; bar applied (kill 4) */
    CHECK(g_map.cells[1 * d->w + 5].wall != 0);        /* the gate is shut again */
    CHECK(g_map.cells[2 * d->w + 7].wall != 0);        /* the bar is up */
    CHECK_EQ(count_op(LVO_CAM), 5);                    /* the start (progress 0) and 4 ticks (64 .. 256) */
    CHECK_EQ(count_op(LVO_CHOICE), 1);
    /* every action that leaves the VM was called at least once */
    for (op = LVO_OPEN; op < LVO_COUNT; op++)
        if (op != LVO_SET && op != LVO_ADD && op != LVO_POST) want |= 1u << op;
    CHECK_EQ_U(g_opmask & want, want);
}

TEST(lvs_two_runs_hash_the_same)
{
    Out a, b, c, e;
    drive(&a, 0);
    drive(&b, 0);
    CHECK(a.n > 0);
    CHECK_EQ_U(a.vm, b.vm);
    CHECK_EQ_U(a.log, b.log);
    CHECK_EQ(a.n, b.n);
    drive(&c, 1);                                      /* a once-event repeated: no effect, same hash */
    CHECK_EQ_U(a.vm, c.vm);
    CHECK_EQ_U(a.log, c.log);
    drive(&e, 2);                                      /* a phase handler is once too */
    CHECK_EQ_U(a.vm, e.vm);
}

TEST(lvs_hash_sees_state)
{
    Out a;
    uint32_t h0;
    drive(&a, 0);
    h0 = lv_vm_hash(&g_vm);
    g_vm.var[7]++;
    CHECK(lv_vm_hash(&g_vm) != h0);
    g_vm.var[7]--;
    g_vm.tick++;
    CHECK(lv_vm_hash(&g_vm) != h0);
    g_vm.tick--;
    CHECK_EQ_U(lv_vm_hash(&g_vm), h0);
}
