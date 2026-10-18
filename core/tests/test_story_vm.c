/* test_story_vm.c - the story bytecode VM (S2; SPEC sec.7 S2, C6, C9,
 * C14, C18; SF sec.15.2, 18.1 story_vm_scripts_valid). Fixtures:
 * core/data/story/test/vmtest.dsc. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/beat.h"
#include "doomo/home.h"
#include "../src/story_int.h"
#include "../src/ui_internal.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static void fresh(void)
{
    test_reset();
    town_new(TOWN_STORY, 77, TOWN_POP_PREMADE);
    scene_set(&SCENE_UI_GALLERY, 0);
    vm_stop();
}

/* tick the VM (no host): returns the last result; ext ops are skipped */
static int run(int max)
{
    int r = VM_IDLE, i;
    for (i = 0; i < max; i++) {
        r = vm_tick();
        while (r == VM_EXT) r = vm_tick();
        if (r != VM_RUN) break;
        frame(0);
    }
    return r;
}

TEST(story_vm_flags_vars_and_jumps)
{
    fresh();
    vm_start(SCR_VMT_FLAGS);
    CHECK(vm_running());
    CHECK_EQ(run(5), VM_IDLE);
    CHECK(story_flag(SF_SPARE_253));
    CHECK_EQ(vm_var(2), 5);
    CHECK_EQ(vm_var(3), 42);
    CHECK(!vm_running());
}

TEST(story_vm_say_queues_lines_in_order_then_waits_for_the_box)
{
    UiDialogDebug d;
    DoomoResident *p;
    fresh();
    p = town_res(DOOMO_PLAYER_ID);
    memcpy(p->name, "Bun", 4);
    vm_set_var(1, 0);
    vm_start(SCR_VMT_SAY);
    CHECK_EQ(vm_tick(), VM_RUN);            /* both lines queued, then END waits */
    CHECK_EQ(ui_dialog_queued(), 2);
    CHECK_EQ(vm_var(1), 9);                 /* setv runs without waiting */
    ui_dialog_debug(&d);
    CHECK_STR(d.text, "Hi there, Bun.");
    CHECK(d.flags & DL_HEART);
    CHECK_EQ(d.expr, EXPR_HAPPY);
    CHECK(vm_running());
    {
        int i;
        for (i = 0; i < 400 && ui_dialog_active(); i++) { frame(BTN_A); frame(0); }
    }
    CHECK(!ui_dialog_active());
    CHECK_EQ(vm_tick(), VM_IDLE);
}

TEST(story_vm_choice_jumps_to_the_picked_label)
{
    int i;
    fresh();
    vm_start(SCR_VMT_CHOICE);
    CHECK_EQ(vm_tick(), VM_RUN);
    CHECK(ui_dialog_active());
    for (i = 0; i < 90; i++) frame(0);      /* let the question type out: the menu opens */
    frame(BTN_DOWN); frame(0);
    for (i = 0; i < 200 && ui_dialog_active(); i++) { frame(BTN_A); frame(0); }
    CHECK_EQ(run(10), VM_IDLE);
    CHECK_EQ(vm_var(0), 1);
    CHECK_EQ(vm_var(4), 20);
}

TEST(story_vm_call_ret_wait_hold_push)
{
    int n;
    const StoryOp *o;
    fresh();
    vm_start(SCR_VMT_CALL);
    CHECK_EQ(run(5), VM_IDLE);
    CHECK_EQ(vm_var(5), 1);
    CHECK_EQ(vm_var(6), 2);
    /* WAIT 5 + HOLD 3: the hold cannot be cut short */
    vm_set_var(7, 0);
    vm_start(SCR_VMT_WAIT);
    for (n = 0; n < 40 && vm_tick() != VM_IDLE; n++) frame(BTN_A);
    CHECK_EQ(vm_var(7), 1);
    CHECK(n >= 3 && n <= 10);
    /* PUSH: the host gets the op once, then the VM waits for the result */
    vm_start(SCR_VMT_PUSH);
    CHECK_EQ(vm_tick(), VM_PUSHED);
    o = vm_ext_op();
    CHECK(o != NULL);
    CHECK_EQ(o->op, OP_PUSH);
    CHECK_EQ(o->a, PEND_TIMING);
    CHECK_EQ(o->b, 0x0301);
    CHECK_EQ(vm_tick(), VM_PUSHED);
    CHECK(vm_ext_op() == NULL);
    vm_resume(3);
    CHECK_EQ(vm_tick(), VM_IDLE);
    CHECK_EQ(vm_var(0), 3);
    CHECK_EQ(vm_var(8), 1);
}

TEST(story_vm_host_ops_come_back_as_ext)
{
    const StoryOp *o;
    int r, flowers;
    fresh();
    flowers = inv_count(ITEM_FLOWERS);
    vm_start(SCR_VMT_EXT);
    r = vm_tick();
    CHECK_EQ(r, VM_EXT);
    o = vm_ext_op();
    CHECK(o && o->op == OP_NATIVE && o->a == 63 && o->b == 5);
    r = vm_tick();
    CHECK_EQ(r, VM_EXT);
    o = vm_ext_op();
    CHECK(o && o->op == OP_SET_BEAT && o->b == BEAT_C1_KEY);
    CHECK_EQ(inv_count(ITEM_FLOWERS), flowers);
    CHECK_EQ(vm_tick(), VM_IDLE);
    CHECK_EQ(inv_count(ITEM_FLOWERS), flowers + 2);
}

TEST(story_vm_later_is_saved_and_fires_on_the_game_clock)
{
    DoomoStry *st;
    int s, i;
    fresh();
    st = story_stry();
    memset(st->later, 0, sizeof(st->later));
    doomo_town.minute = 600;
    vm_start(SCR_VMT_LATER);
    CHECK_EQ(run(3), VM_IDLE);
    CHECK_EQ(st->later[0].script, SCR_VMT_SUB);
    CHECK_EQ(vm_later_due(), 0);
    doomo_town.minute = 602;
    s = vm_later_due();
    CHECK_EQ(s, (int)SCR_VMT_SUB);
    CHECK_EQ(st->later[0].script, 0);
    /* a fifth pending callback runs right after its script instead */
    for (i = 0; i < 4; i++) { st->later[i].script = SCR_VMT_FLAGS; st->later[i].due = 9999; }
    vm_set_var(5, 0);
    vm_start(SCR_VMT_LATER);
    CHECK_EQ(run(3), VM_IDLE);
    CHECK_EQ(vm_var(5), 1);
}

TEST(story_rng_is_deterministic_and_seeded_by_the_town)
{
    uint32_t a, b, c;
    fresh();
    doomo_town.seed = 1234;
    a = story_rng(7);
    b = story_rng(7);
    c = story_rng(8);
    CHECK_EQ_U(a, b);
    CHECK(a != c);
    doomo_town.seed = 1235;
    CHECK(story_rng(7) != a);
    /* the VM's RNG op goes through it */
    doomo_town.seed = 1234;
    vm_start(SCR_VMT_LATER);
    run(3);
    CHECK_EQ(vm_var(9), (int)(story_rng(0x524E0000u | 7) & 0x7FFF));
}

TEST(story_fingerprint_follows_the_story_state)
{
    uint32_t a, b;
    fresh();
    a = story_fingerprint();
    CHECK_EQ_U(a, story_fingerprint());
    story_set_flag(SF_BOTTLE_SEEN, 1);
    b = story_fingerprint();
    CHECK(a != b);
    story_stry()->knocks = 3;
    CHECK(story_fingerprint() != b);
}

TEST(story_vm_fmt_codes)
{
    char out[64];
    int n[2];
    DoomoResident *p;
    fresh();
    p = town_res(DOOMO_PLAYER_ID);
    memcpy(p->name, "Bun", 4);
    n[0] = 3; n[1] = -12;
    vm_fmt_n(out, (int)sizeof(out), "@P has @0 of @1 @@ @Q", n, 2);
    CHECK_STR(out, "Bun has 3 of -12 @ @Q");
    vm_fmt_n(out, 8, "@P@P@P@P", n, 2);
    CHECK_STR(out, "BunBunB");
}

/* SF sec.18.1 story_vm_scripts_valid: every script in every pack ends,
 * every jump lands inside its script, every DATA follows an op that takes
 * one, every text / script / beat id it names exists. */
TEST(story_vm_scripts_valid)
{
    int p, np = txt_pack_count(), total = 0;
    CHECK(np >= 2);
    for (p = 0; p < np; p++) {
        int s, ns = txt_pack_scripts(p);
        CHECK_MSG(txt_pack_load(p), "pack %d not loadable", p);
        for (s = 0; s < ns; s++) {
            int n = 0, i;
            const StoryOp *o = txt_script((uint16_t)TXT_ID(p, s), &n);
            CHECK_MSG(o && n > 0, "pack %d script %d", p, s);
            CHECK_MSG(o[n - 1].op == OP_END, "pack %d script %d does not end", p, s);
            for (i = 0; i < n; i++) {
                const StoryOp *q = &o[i];
                char buf[200];
                CHECK_MSG(q->op < OP_COUNT, "bad op %d", q->op);
                if (q->op == OP_JIF || q->op == OP_JIFN || q->op == OP_JLT || q->op == OP_JGE || q->op == OP_JMP)
                    CHECK_MSG(q->b < n, "pack %d script %d op %d jumps out", p, s, i);
                if (q->op == OP_DATA && q->a == VMD_LABEL && q->b != 0xFFFFu)
                    CHECK_MSG(q->b < n, "pack %d script %d: a choice label out of range", p, s);
                if (q->op == OP_DATA)
                    CHECK_MSG(i > 0, "pack %d script %d starts with DATA", p, s);
                if (q->op == OP_SAY || q->op == OP_SAY_X || q->op == OP_CHOICE || q->op == OP_PLAYER_LINE ||
                    (q->op == OP_DATA && q->a == VMD_OPT))
                    CHECK_MSG(txt_get(q->b, buf, (int)sizeof(buf)) >= 0, "pack %d script %d: text %u missing", p, s, q->b);
                if (q->op == OP_CALL || q->op == OP_LATER)
                    CHECK_MSG(txt_script(q->b, NULL) != NULL, "pack %d script %d: script %u missing", p, s, q->b);
                if (q->op == OP_BEAT) {
                    BeatScript bs;
                    CHECK_MSG(txt_beat(q->b, &bs) && beat_script_valid(&bs), "pack %d script %d: beat %u", p, s, q->b);
                }
            }
            total++;
        }
    }
    CHECK(total >= 60);
}

static const DoomoSaveExt *stry_ext(void)
{
    int i;
    for (i = 0; i < save_ext_count(); i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        if (e && e->tag == STRY_CHUNK_TAG) return e;
    }
    return NULL;
}

/* SF sec.18.1 story_save_round_trip_mid_story (the STRY half): exactly 96 B,
 * size() == bytes written, and old saves migrate from their beat (C19) */
TEST(story_stry_chunk_round_trips_and_migrates_old_beats)
{
    const DoomoSaveExt *e = stry_ext();
    uint8_t buf[128];
    DoomoStry copy;
    uint32_t n;
    fresh();
    CHECK(e != NULL);
    CHECK_EQ(e->size(), 96);
    CHECK_EQ(e->max_size, 96);
    story_stry()->knocks = 5;
    story_stry()->later[2].script = 1234;
    story_side2_set_state(117, 3);
    story_side2_set_state(0, 2);
    n = e->write(buf, sizeof(buf));
    CHECK_EQ(n, 96);
    copy = *story_stry();
    memset(story_stry(), 0, sizeof(DoomoStry));
    CHECK(e->read(buf, n, STRY_CHUNK_VERSION));
    CHECK(memcmp(&copy, story_stry(), sizeof(copy)) == 0);
    CHECK_EQ(story_side2_state(117), 3);
    CHECK_EQ(story_side2_state(0), 2);
    CHECK_EQ(story_side2_state(1), 0);
    CHECK(!e->read(buf, 95, STRY_CHUNK_VERSION));
    CHECK(!e->read(buf, 96, 2));
    /* a new town: nothing done */
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    CHECK(!story_episode_done(BEAT_P0));
    CHECK_EQ(story_stry()->ver, STRY_CHUNK_VERSION);
    /* an old save at C2_EXORCISE: everything before Ep 2.5 is done */
    story_set_flag(SF_STARTED, 1);
    doomo_town.story.quest = BEAT_C2_EXORCISE;
    e->reset();
    CHECK(story_episode_done(BEAT_P0));
    CHECK(story_episode_done(BEAT_EP1_6));
    CHECK(story_episode_done(BEAT_INT1));
    CHECK(story_episode_done(BEAT_EP2_4));
    CHECK(story_episode_done(BEAT_C1_WALL));             /* legacy ids alias */
    CHECK(!story_episode_done(BEAT_EP2_5));
    CHECK(!story_episode_done(BEAT_C2_EXORCISE));
    CHECK_EQ(story_stry()->home_phase, HP_HAUNTED);
    /* every legacy beat maps to an episode, in story order */
    {
        int b, last = -1;
        for (b = 0; b < BEAT_COUNT; b++) {
            int o = story_episode_order(b);
            CHECK_MSG(o >= last, "beat %s goes back in the story order", story_beat_name(b));
            last = o;
        }
    }
}

/* SPEC C19: the spare met / nest bits STRY reuses (SF_KNOCK, SF_WRONG) are
 * only safe if no map prop sets them: nest props extra < STORY_NESTS */
TEST(story_nest_props_never_touch_the_reused_bits)
{
    int m;
    static const int maps[] = { MAPID_HAHALAND, MAPID_HAHALAND_INVADED, MAPID_HELLVILE, MAPID_PARK_UNITED };
    for (m = 0; m < 4; m++) {
        int i;
        const DoomoMapDef *d = doomo_maps[maps[m]];
        CHECK(d != NULL);
        for (i = 0; i < d->thing_count; i++) {
            const DoomoThing *t = &d->things[i];
            if (t->type == THING_SPAWNER)
                CHECK_MSG(t->arg < STORY_NESTS, "map %d nest prop %d has arg %d", maps[m], i, t->arg);
        }
    }
    {
        int k, w;
        for (k = 0; k < STORY_KNOCKS; k++) {
            CHECK(SF_KNOCK(k) >= 36 && SF_KNOCK(k) < 64);
            CHECK(SF_KNOCK(k) < SF_NEST0 + STORY_NESTS || SF_KNOCK(k) >= SF_NEST0 + 4);
        }
        for (w = 0; w < STORY_WRONGS; w++) CHECK(SF_WRONG(w) >= 44 && SF_WRONG(w) < 64);
    }
}

/* ---- S2 engine milestone: the stash, the verbs, the minigames, the walker ---- */

/* SPEC C5: the VM decodes a pushed scene's captions into the resident stash */
TEST(story_vm_stash_and_push_captions)
{
    const StoryOp *o;
    fresh();
    vm_start(SCR_VMT_STASH);
    CHECK_EQ(vm_tick(), VM_PUSHED);
    o = vm_ext_op();
    CHECK(o && o->op == OP_PUSH && o->a == PEND_COMBAT && o->b == 1);
    CHECK_STR(txt_stash_get(2), "A stashed line");
    CHECK_STR(txt_stash_get(0), "Boss caption one");
    CHECK_STR(txt_stash_get(1), "Boss caption two");
    vm_resume(1);
    CHECK_EQ(vm_tick(), VM_IDLE);
    CHECK_EQ(vm_var(8), 2);
}

/* `verb KIND ...` compiles to a NATIVE verb op with its cell and continuation */
TEST(story_vm_verbs_come_back_as_native_ext_ops)
{
    const StoryOp *o;
    fresh();
    vm_start(SCR_VMT_VERB);
    CHECK_EQ(vm_tick(), VM_EXT);
    o = vm_ext_op();
    CHECK(o && o->op == OP_NATIVE && o->a == STORY_CB_VERB + VERB_FIND);
    CHECK_EQ(vm_ext_arg(), 10 | (12 << 8));
    CHECK_EQ(vm_ext_data(VMD_SCRIPT, 0), (int)SCR_VMT_SUB);
    CHECK_EQ(vm_tick(), VM_EXT);
    o = vm_ext_op();
    CHECK(o && o->a == STORY_CB_VERB + VERB_ESCORT && o->b == ID_PIP);
    CHECK_EQ(vm_ext_arg(), 40 | (21 << 8));
    CHECK_EQ(vm_tick(), VM_EXT);
    o = vm_ext_op();
    CHECK(o && o->a == STORY_CB_VERB + VERB_DELIVER && o->b == ID_NANA);
    CHECK(((unsigned)vm_ext_arg() & 0xFFFFu) & VERB_ARG_FRAGILE);
    CHECK_EQ(vm_tick(), VM_IDLE);              /* the VM never waits for a verb */
    CHECK_EQ(vm_var(10), 1);
    vm_start(SCR_VMT_TIMING);
    CHECK_EQ(vm_tick(), VM_PUSHED);
    o = vm_ext_op();
    CHECK(o && o->a == PEND_TIMING && o->b == TIMING_ARG(TPAT_RIMSHOT, TSKIN_LAUGH | TIMING_DRIFT));
}

/* the verbs' rules (story_verbs.c): escort distance, photo frame, find heat */
TEST(story_verb_rules)
{
    CHECK_EQ(story_escort_judge(1, 0), ESC_FOLLOW);
    CHECK_EQ(story_escort_judge(5, 0), ESC_WAIT);
    CHECK_EQ(story_escort_judge(12, 10), ESC_FAR);
    CHECK_EQ(story_escort_judge(12, 1000), ESC_LOST);
    /* facing right (2) at (10,10): the subject 4 cells ahead, dead centre */
    CHECK_EQ(story_photo_judge(10, 10, 2, 14, 10), 2);
    CHECK_EQ(story_photo_judge(10, 10, 2, 14, 11), 1);     /* in the frame */
    CHECK_EQ(story_photo_judge(10, 10, 2, 14, 13), 0);     /* outside it   */
    CHECK_EQ(story_photo_judge(10, 10, 2, 11, 10), 0);     /* too close    */
    CHECK_EQ(story_photo_judge(10, 10, 1, 14, 10), 0);     /* behind you   */
    CHECK_EQ(story_photo_judge(10, 10, 3, 10, 6), 2);      /* facing up    */
    CHECK_EQ(story_find_heat(0, 0), 4);
    CHECK_EQ(story_find_heat(1, -2), 3);
    CHECK_EQ(story_find_heat(4, 0), 2);
    CHECK_EQ(story_find_heat(-7, 3), 1);
    CHECK_EQ(story_find_heat(20, 20), 0);
    CHECK_EQ(story_timing_judge(0), 2);
    CHECK_EQ(story_timing_judge(-2), 2);
    CHECK_EQ(story_timing_judge(4), 1);
    CHECK_EQ(story_timing_judge(-6), 0);
}

/* a verb runs on the overworld: the find verb ends on its cell and runs the
 * continuation with v1 = the result; deliveries slow you down */
TEST(story_verbs_run_on_the_overworld)
{
    int i;
    fresh();
    story_verb_begin(VERB_DELIVER, ID_NANA, VERB_ARG_FRAGILE, SCR_VMT_SUB);
    CHECK_EQ(story_verb_active(), VERB_DELIVER);
    CHECK(story_verbs_walk_q8() < 256);
    CHECK(story_verbs_objective() != NULL);
    CHECK(!story_verbs_npc(ID_PIP));
    CHECK(story_verbs_npc(ID_NANA));
    CHECK_EQ(story_verb_active(), -1);
    CHECK_EQ(story_verbs_walk_q8(), 256);
    for (i = 0; i < 20 && vm_running(); i++) { story_vm_pump(); frame(0); }
    CHECK_EQ(vm_var(1), 2);                    /* delivered perfectly */
    CHECK_EQ(vm_var(5), 1);                    /* vmt_sub ran        */
    vm_set_var(5, 0);
    story_verb_begin(VERB_FIND, 0, 10 | (12 << 8), SCR_VMT_SUB);
    story_verbs_cell(30, 30);
    CHECK_EQ(story_verb_active(), VERB_FIND);
    story_verbs_cell(11, 12);
    CHECK_EQ(story_verb_active(), VERB_FIND);
    story_verbs_cell(10, 12);
    CHECK_EQ(story_verb_active(), -1);
    for (i = 0; i < 20 && vm_running(); i++) { story_vm_pump(); frame(0); }
    CHECK_EQ(vm_var(1), 1);
    CHECK_EQ(vm_var(5), 1);
}

/* SCENE_STORY_TIMING: pressing A on every punch lands all three rounds */
static int play_timing(int arg, int offset)
{
    int i, round, to, st;
    scene_push(&SCENE_STORY_TIMING, arg);
    for (i = 0; i < 12; i++) frame(0);
    frame(BTN_A); frame(0);                      /* the card */
    for (i = 0; i < 3000; i++) {
        st = story_timing_debug(&round, &to);
        if (st == 3) break;
        frame(st == 1 && to == offset ? BTN_A : 0);
    }
    for (i = 0; i < 40; i++) frame(0);
    frame(BTN_A); frame(0);                      /* the result card */
    return story_timing_result();
}

TEST(story_timing_minigame_rounds_and_drift)
{
    fresh();
    CHECK_EQ(play_timing(TIMING_ARG(TPAT_RIMSHOT, TSKIN_LAUGH), 0), 3);
    CHECK(scene_top() != &SCENE_STORY_TIMING);
    CHECK_EQ(play_timing(TIMING_ARG(TPAT_THREE, TSKIN_THUMP), -4), 3);   /* 4 frames late: good */
    CHECK_EQ(play_timing(TIMING_ARG(TPAT_KNOCK, TSKIN_BELL), -12), 0);   /* far too late       */
    CHECK_EQ(play_timing(TIMING_ARG(TPAT_PRAYER, TSKIN_HEART | TIMING_DRIFT), 0), 3);
    CHECK_EQ(play_timing(TIMING_ARG(TPAT_PRAYER, TSKIN_HEART | TIMING_DRIFT), 9), 0);   /* too early */
}

/* SCENE_STORY_MG: the shell around the built-in text entry and an unknown id */
TEST(story_mg_shell_text_entry)
{
    int i;
    fresh();
    txt_stash(0, TXT_VMT_CAP_A);
    txt_stash(1, TXT_VMT_CAP_B);                  /* the answer: "Boss caption two" */
    scene_push(&SCENE_STORY_MG, STORY_MG_TEXT_ENTRY);
    for (i = 0; i < 12; i++) frame(0);
    frame(BTN_A); frame(0);
    for (i = 0; i < 10; i++) frame(0);
    {
        const char *s = "boss captiontwo\n";      /* case and spaces are forgiven */
        for (; *s; s++) { CHECK(ui_text_entry_feed((unsigned char)*s)); frame(0); }
    }
    for (i = 0; i < 40; i++) frame(0);
    CHECK_EQ(story_mg_result(), 1);
    frame(BTN_A); frame(0);
    CHECK(scene_top() != &SCENE_STORY_MG);
    scene_push(&SCENE_STORY_MG, 0x1234);         /* not registered: a lost round */
    for (i = 0; i < 30; i++) frame(0);
    CHECK_EQ(story_mg_result(), 0);
    frame(BTN_A); frame(0);
    CHECK(scene_top() != &SCENE_STORY_MG);
}

/* the scripted dream: a forced Peek, the stashed captions, no stat change */
TEST(story_scripted_dream)
{
    int i;
    DoomoResident *p;
    uint8_t happy;
    fresh();
    p = town_res(DOOMO_PLAYER_ID);
    happy = p ? p->stat[STAT_HAPPY] : 0;
    txt_stash(0, TXT_VMT_CAP_A);
    txt_stash(1, TXT_NONE);
    txt_stash(2, TXT_NONE);
    txt_stash(3, TXT_NONE);
    scene_push(&SCENE_DREAM, DREAM_ARG_SCRIPTED(DREAM_SCRIPTED_D1));
    frame(BTN_B); frame(0);                       /* no tiptoeing away */
    CHECK(scene_top() == &SCENE_DREAM);
    for (i = 0; i < 2000 && scene_top() == &SCENE_DREAM; i++) frame(i % 40 == 39 ? BTN_A : 0);
    CHECK(scene_top() != &SCENE_DREAM);
    if (p) CHECK_EQ(p->stat[STAT_HAPPY], happy);
}

/* the side walker (SPEC S2): chains, requirements, one part per chapter,
 * cross-border parts 3+ from Ep 4.1; and the quest log's trophies (R8) */
TEST(story_side_walker_gates_and_trophies)
{
    static StorySideRow rows[STORY_SIDE2_ROWS];
    uint8_t side2[30], done_ch[26], out[8];
    int i;
    memset(rows, 0, sizeof(rows));
    for (i = 0; i < STORY_SIDE2_ROWS; i++) { rows[i].sid = (uint8_t)i; rows[i].trophy = rows[i].title = 0xFFFF; }
#define ROW(n, lo, hi, sd, ch, stp, req) \
    (rows[n].kind = 1, rows[n].ch_lo = lo, rows[n].ch_hi = hi, rows[n].side = sd, \
     rows[n].chain = ch, rows[n].step = stp, rows[n].requires = req)
    ROW(0, 1, 4, 0, 'J', 1, 0);
    ROW(1, 1, 4, 0, 'J', 2, 0);
    ROW(2, 2, 4, 1, 'J', 3, 0);                 /* J crosses the Wall: part 3 waits for Ep 4.1 */
    ROW(3, 2, 3, 0, 0, 0, 1 + 0);               /* a stand-alone after S100 */
    ROW(4, 1, 5, 0, 'K', 1, 0);
    ROW(5, 1, 5, 0, 'K', 2, 0);
    memset(side2, 0, sizeof(side2));
    memset(done_ch, 0, sizeof(done_ch));
#define ST(n, s) (side2[(n) >> 2] = (uint8_t)((side2[(n) >> 2] & ~(3u << (((n) & 3) * 2))) | ((s) << (((n) & 3) * 2))))
    CHECK(story_side2_gate(rows, 0, side2, 1, 0, done_ch));
    CHECK(!story_side2_gate(rows, 1, side2, 1, 0, done_ch));     /* part 1 first   */
    CHECK(!story_side2_gate(rows, 3, side2, 2, 0, done_ch));     /* requires S100  */
    CHECK(!story_side2_gate(rows, 6, side2, 1, 0, done_ch));     /* not a mission  */
    ST(0, 3);
    done_ch['J' - 'A'] = 1;
    CHECK(!story_side2_gate(rows, 0, side2, 1, 0, done_ch));     /* done           */
    CHECK(!story_side2_gate(rows, 1, side2, 1, 0, done_ch));     /* one per chapter */
    CHECK(story_side2_gate(rows, 1, side2, 2, 0, done_ch));
    CHECK(story_side2_gate(rows, 3, side2, 2, 0, done_ch));
    CHECK(!story_side2_gate(rows, 3, side2, 1, 0, done_ch));     /* its chapter window */
    ST(1, 3);
    done_ch['J' - 'A'] = 2;
    CHECK(!story_side2_gate(rows, 2, side2, 3, 0, done_ch));     /* cross-border part 3 */
    CHECK(story_side2_gate(rows, 2, side2, 4, 1, done_ch));
    ST(1, 2);
    CHECK(!story_side2_gate(rows, 2, side2, 4, 1, done_ch));     /* part 2 not done */
    /* trophies: done rows that name one */
    ST(1, 3);
    rows[1].trophy = 7;
    rows[4].trophy = 9;
    ST(4, 2);
    CHECK_EQ(story_trophies(rows, side2, out, 8), 1);
    CHECK_EQ(out[0], 1);
    ST(4, 3);
    CHECK_EQ(story_trophies(rows, side2, out, 8), 2);
    CHECK_EQ(out[1], 4);
#undef ROW
#undef ST
}
