/* test_beat.c - the beat scheduler (beat.h; owner: S1, SPEC sec.7 S1
 * "test_beat": events fire at label_frame + at; A moves anchors only;
 * beat_skip applies flags only; beat_clock freeze / 2x / normal; the cost
 * is O(events fired)). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"

/* Run a script to the end (or `max` frames), recording the frame of every
 * host event: rec[i] = frame of the i-th BT_EXT, ids[i] = its b. */
typedef struct { int n; int frame[64]; int id[64]; } Rec;

static int run_until(Rec *r, int max, int press_a_at, int press_a_at2)
{
    int f;
    for (f = 0; f < max; f++) {
        int st;
        if (f == press_a_at || f == press_a_at2) beat_press_a();
        while ((st = beat_tick()) == BT_EXT) {
            const BeatEv *e = beat_ext_event();
            if (r && e && r->n < 64) { r->frame[r->n] = beat_now(); r->id[r->n] = e->b; r->n++; }
        }
        if (st == BT_DONE) return f;
    }
    return -1;
}

/* the CT sec.7.2 example (the pet's first dark evolution), sorted by anchor */
enum { L0, L_HATCHED, L_INSERT, L_BACK, L_LINE };
static const BeatEv EV_PET_TURN[] = {
    {  0, L0,        BOP_LABEL,      L_HATCHED, 0 },
    { 40, L_HATCHED, BOP_SAY,        1, 101 },                 /* "beep."            */
    { 40, L_HATCHED, BOP_MUSIC_DUCK, 6, 140 },
    { 90, L_HATCHED, BOP_LABEL,      L_INSERT, 0 },
    {  0, L_INSERT,  BOP_INSERT,     7, 4 },                   /* host + mutes 4 f   */
    {  0, L_INSERT,  BOP_SHAKE,      SHK_IMPACT, 0 },
    {  4, L_INSERT,  BOP_LABEL,      L_BACK, 0 },
    {  0, L_BACK,    BOP_FLAG,       200, 1 },                 /* persistent change  */
    {  2, L_BACK,    BOP_SAY,        1, 102 },
    { 10, L_BACK,    BOP_LABEL,      L_LINE, 0 },
    {  0, L_LINE,    BOP_CAPTION,    2, 103 },
    { 21, L_LINE,    BOP_IRIS_OUT,   3, 10 | 1 << 6 | 13 << 7 },
    { 60, L_LINE,    BOP_END,        0, 0 },
};
static const BeatScript PET_TURN = { EV_PET_TURN, sizeof EV_PET_TURN / sizeof EV_PET_TURN[0], 5 };

TEST(beat_events_fire_at_label_plus_at)
{
    Rec r;
    int end;
    memset(&r, 0, sizeof r);
    CHECK(beat_script_valid(&PET_TURN));
    beat_start(&PET_TURN);
    CHECK(beat_running());
    end = run_until(&r, 1000, -1, -1);
    CHECK_EQ(end, 90 + 4 + 10 + 60);
    CHECK_EQ(r.n, 5);
    CHECK_EQ(r.frame[0], 40);  CHECK_EQ(r.id[0], 101);        /* L_HATCHED + 40 */
    CHECK_EQ(r.frame[1], 90);                                 /* the insert      */
    CHECK_EQ(r.frame[2], 96);  CHECK_EQ(r.id[2], 102);        /* L_BACK(94) + 2  */
    CHECK_EQ(r.frame[3], 104); CHECK_EQ(r.id[3], 103);        /* L_LINE(104)     */
    CHECK_EQ(r.frame[4], 125);                                /* + R21 iris      */
    CHECK(story_flag(200));
    CHECK(!beat_running());
    CHECK_EQ(beat_age(L_INSERT), 164 - 90);
    CHECK_EQ(beat_age(L_LINE), 60);
}

/* A wait-for-A anchor: the punch inside stays frame-exact wherever A lands */
enum { W0, W_ASK, W_PUNCH };
static const BeatEv EV_WAIT[] = {
    {  0, W0,      BOP_SAY,    0, 1 },
    {  0, W0,      BOP_WAIT_A, W_ASK, 12 },        /* A after 12 f fires W_ASK  */
    {  0, W_ASK,   BOP_SAY,    0, 2 },
    {  8, W_ASK,   BOP_HOLD,   W_PUNCH, 13 },      /* R8, then an unskippable R13 */
    {  0, W_PUNCH, BOP_CAPTION, 0, 3 },
    {  3, W_PUNCH, BOP_SAY,    0, 4 },
    { 21, W_PUNCH, BOP_END,    0, 0 },
};
static const BeatScript WAIT = { EV_WAIT, sizeof EV_WAIT / sizeof EV_WAIT[0], 3 };

TEST(beat_a_moves_anchors_only)
{
    Rec a, b, c;
    int i;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); memset(&c, 0, sizeof c);
    /* A too early is ignored; A later moves everything after it */
    beat_start(&WAIT); run_until(&a, 400, 5, 20);
    beat_start(&WAIT); run_until(&b, 400, 50, -1);
    CHECK_EQ(a.n, 4); CHECK_EQ(b.n, 4);
    CHECK_EQ(a.frame[1], 20);
    CHECK_EQ(b.frame[1], 50);
    for (i = 1; i < 4; i++) CHECK_EQ(b.frame[i] - b.frame[1], a.frame[i] - a.frame[1]);
    CHECK_EQ(a.frame[2] - a.frame[1], 8 + 13);
    CHECK_EQ(a.frame[3] - a.frame[2], 3);
    /* A during the HOLD never shortens it */
    beat_start(&WAIT); run_until(&c, 400, 20, 30);
    CHECK_EQ(c.frame[2], a.frame[2]);
    /* no A: nothing past the wait ever fires */
    memset(&c, 0, sizeof c);
    beat_start(&WAIT);
    CHECK_EQ(run_until(&c, 300, -1, -1), -1);
    CHECK_EQ(c.n, 1);
    CHECK(beat_running());
    CHECK_EQ(beat_age(W_ASK), -1);
}

TEST(beat_skip_applies_flags_only)
{
    static const BeatEv ev[] = {
        {  0, 0, BOP_FLAG, 210, 1 },
        { 30, 0, BOP_SHAKE, SHK_ROAR, 0 },
        { 30, 0, BOP_FLAG, 211, 1 },
        { 40, 0, BOP_SAY, 0, 1 },
        { 50, 0, BOP_FLAG, 212, 0 },
        { 60, 0, BOP_LABEL, 1, 0 },
        {  5, 1, BOP_FLAG, 213, 1 },
        { 99, 1, BOP_END, 0, 0 },
    };
    static const BeatScript s = { ev, sizeof ev / sizeof ev[0], 2 };
    int f;
    story_set_flag(212, 1);
    camfx_reset();
    beat_start(&s);
    for (f = 0; f < 10; f++) { camfx_tick(); while (beat_tick() == BT_EXT) {} }
    CHECK(story_flag(210));
    beat_skip();
    CHECK(!beat_running());
    CHECK(story_flag(211));
    CHECK(!story_flag(212));
    CHECK(story_flag(213));                 /* a label that never fired: its flags too */
    CHECK(!camfx_shaking());                /* no shake, no sound, no host event */
    CHECK_EQ(beat_tick(), BT_DONE);
}

TEST(beat_clock_freeze_double_still_hitstop)
{
    static const BeatEv ev[] = {
        { 10, 0, BOP_FREEZE, 1, 0 },
        { 10, 0, BOP_PANIC, 2, 0 },
        { 10, 0, BOP_STILL, 3, 0 },
        { 20, 0, BOP_THAW, -1, 0 },
        { 30, 0, BOP_HITSTOP, 4, 0 },
        { 50, 0, BOP_END, 0, 0 },
    };
    static const BeatScript s = { ev, sizeof ev / sizeof ev[0], 1 };
    int f;
    beat_start(&s);
    for (f = 0; f <= 15; f++) while (beat_tick() == BT_EXT) {}
    /* frames 1..10 normal (the first tick is frame 0), 11..15 per mode */
    CHECK_EQ(beat_clock(0), 15);
    CHECK_EQ(beat_clock(1), 10);
    CHECK_EQ(beat_clock(2), 10 + 10);
    CHECK_EQ(beat_clock(3), 0);
    for (; f <= 25; f++) while (beat_tick() == BT_EXT) {}
    CHECK_EQ(beat_clock(0), 25);
    CHECK_EQ(beat_clock(1), 15);            /* thawed at frame 20 */
    CHECK_EQ(beat_clock(3), 15);            /* STILL kept counting under the hood: 0 shown */
    for (; f <= 40; f++) while (beat_tick() == BT_EXT) {}
    CHECK_EQ(beat_clock(0), 40 - 4);        /* a 4-frame hit-stop */
    CHECK_EQ(beat_now(), 40);               /* the script clock never stops */
    beat_set_actor_clock(5, BCLK_FROZEN);
    for (; f <= 45; f++) while (beat_tick() == BT_EXT) {}
    CHECK_EQ(beat_clock(5), 36);
}

TEST(beat_cost_is_events_fired)
{
    /* 300 events on 30 labels; a frame with nothing due costs one look per
     * live label, never a scan of the script */
    static BeatEv ev[302];
    BeatScript s;
    int i, n = 0, f, fired = 0;
    uint32_t w0;
    for (i = 1; i < 30; i++) ev[n++] = (BeatEv){ (uint16_t)(i * 20), 0, BOP_LABEL, (int16_t)i, 0 };
    for (i = 1; i < 30; i++) {
        int k;
        for (k = 0; k < 9; k++) ev[n++] = (BeatEv){ (uint16_t)(k * 2), (uint8_t)i, BOP_SAY, 0, (int16_t)k };
    }
    ev[n++] = (BeatEv){ 700, 0, BOP_END, 0, 0 };
    /* sorted by (anchor, at): put the END among label 0's run */
    {
        BeatEv end = ev[n - 1];
        memmove(&ev[30], &ev[29], sizeof(BeatEv) * (size_t)(n - 30));
        ev[29] = end;
    }
    s.ev = ev; s.n = (uint16_t)n; s.nlabels = 30;
    CHECK(beat_script_valid(&s));
    beat_start(&s);
    w0 = beat_debug_work();
    for (f = 0; f < 800; f++) {
        int st;
        while ((st = beat_tick()) == BT_EXT) fired++;
        if (st == BT_DONE) break;
    }
    CHECK_EQ(fired, 29 * 9);
    CHECK_EQ(f, 700);
    /* events fired + one look per live cursor per frame + binary searches */
    CHECK_MSG(beat_debug_work() - w0 < (uint32_t)(n + 701 * 3 + 30 * 10),
              "work %u", (unsigned)(beat_debug_work() - w0));
}

TEST(beat_validates_order_and_camfx_ops)
{
    static const BeatEv bad[] = { { 5, 1, BOP_SAY, 0, 0 }, { 0, 0, BOP_END, 0, 0 } };
    static const BeatScript sb = { bad, 2, 2 };
    static const BeatEv ev[] = {
        { 0, 0, BOP_FLASH, PAL_WHITE, 10 | 2 << 8 },
        { 0, 0, BOP_SHAKE, SHK_SLAM, 0 },
        { 3, 0, BOP_PAL_LERP, PAL_SEPIA, 16 | 4 << 8 },
        { 3, 0, BOP_NOBUTTON, 0, 0 },
        { 9, 0, BOP_END, 0, 0 },
    };
    static const BeatScript s = { ev, sizeof ev / sizeof ev[0], 1 };
    int lvl;
    uint8_t c;
    CHECK(!beat_script_valid(&sb));
    CHECK(!beat_script_valid(NULL));
    camfx_reset();
    beat_start(&s);
    camfx_tick(); beat_tick();
    CHECK(camfx_shaking());
    CHECK_EQ(camfx_blend(&c), 10);
    CHECK_EQ(c, PAL_WHITE);
    camfx_tick(); beat_tick();
    CHECK_EQ(camfx_blend(&c), 5);
    camfx_tick(); beat_tick();
    camfx_tick(); beat_tick();               /* frame 3: the sepia ramp starts */
    CHECK_EQ(camfx_pal_state(&lvl), PAL_SEPIA);
    CHECK_EQ(lvl, 4);
    camfx_tick(); beat_tick();
    camfx_tick(); beat_tick();
    camfx_tick(); beat_tick();
    camfx_pal_state(&lvl);
    CHECK_EQ(lvl, 16);
}

/* ---- S1 M4: the cutscenes on beat.h ---------------------------------------- */
int cutscene_script(int c, BeatScript *out);   /* cutscene.c */

TEST(beat_wait_a_fires_by_itself_after_its_timeout)
{
    static const BeatEv ev[] = {
        { 0, 0, BOP_WAIT_A, 1, BEAT_WAIT_A_B(12, 40) },
        { 0, 1, BOP_END, 0, 0 },
    };
    static const BeatScript s = { ev, 2, 2 };
    int f = 0;
    beat_start(&s);
    while (beat_tick() != BT_DONE && f < 100) f++;
    CHECK_EQ(f, 40);
    beat_start(&s);                              /* A after the minimum moves the anchor */
    for (f = 0; f < 20; f++) beat_tick();
    beat_press_a();
    CHECK_EQ(beat_tick(), BT_DONE);
}

TEST(beat_every_cutscene_is_a_valid_script)
{
    int c;
    for (c = 0; c < CUT_COUNT; c++) {
        BeatScript s;
        uint16_t i;
        int shots = 0, ends = 0;
        CHECK(cutscene_script(c, &s));
        CHECK_MSG(beat_script_valid(&s), "cutscene %d is not sorted", c);
        for (i = 0; i < s.n; i++) {
            if (s.ev[i].op == BOP_SHOT) shots++;
            if (s.ev[i].op == BOP_END) ends++;
        }
        CHECK_MSG(shots >= 1 && ends == 1, "cutscene %d: %d shots, %d ends", c, shots, ends);
    }
}

int cutscene_showcase_script(int k, BeatScript *out);

TEST(beat_timing_showcase_scripts_are_valid_and_land_the_punch)
{
    int k;
    for (k = 0; k < 3; k++) {
        BeatScript s;
        uint16_t i;
        int punch = 0, button = 0;
        CHECK(cutscene_showcase_script(k, &s));
        CHECK(beat_script_valid(&s));
        for (i = 0; i < s.n; i++) {
            if (s.ev[i].op == BOP_LABEL && s.ev[i].a == L_PUNCH) punch = 1;
            /* the button sits 12-25 frames after the punch (SPEC sec.10 lint) */
            if (s.ev[i].op == BOP_LABEL && s.ev[i].a == L_BUTTON && s.ev[i].anchor == L_PUNCH)
                button = s.ev[i].at >= 12 && s.ev[i].at <= 25;
        }
        CHECK(punch && button);
    }
}

TEST(beat_cutscenes_play_by_themselves_and_start_skips)
{
    int c, f;
    for (c = 0; c < CUT_COUNT; c++) {
        test_reset();
        town_new(TOWN_STORY, 7, TOWN_POP_EMPTY);
        scene_set(&SCENE_UI_GALLERY, 0);
        scene_push(&SCENE_CUTSCENE, c);
        for (f = 0; f < 6000 && scene_top() == &SCENE_CUTSCENE; f++) {
            plat_begin_frame(); doomo_frame(0); plat_end_frame();
        }
        CHECK_MSG(scene_top() != &SCENE_CUTSCENE, "cutscene %d never ends", c);
        scene_push(&SCENE_CUTSCENE, c);
        plat_begin_frame(); doomo_frame(0); plat_end_frame();
        plat_begin_frame(); doomo_frame(BTN_START); plat_end_frame();
        CHECK_MSG(scene_top() != &SCENE_CUTSCENE, "START does not skip cutscene %d", c);
    }
}
