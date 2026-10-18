/* test_tween.c - the comic-timing curves, tables and springs (tween.h;
 * owner: S1, docs/design/story/SPEC.md sec.7 S1 "tween_matches_trailer").
 * The golden core/tests/data/trailer_curves.h comes from the trailer's own
 * formulas (core/tools/trailer_curves.c). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/tween.h"
#include "data/trailer_curves.h"

static long labs_(long v) { return v < 0 ? -v : v; }

TEST(tween_matches_trailer)
{
    int i;
    /* tw_bounce within 0.2% of tk_bounce at the 33 LUT points (t = i/32);
     * the tail from i = 30 is clamped to 1.0 on purpose (CT sec.10) */
    for (i = 0; i <= 32; i++) {
        fix16 t = (fix16)(i << 11);
        long want = i >= 30 ? 65536 : TC_BOUNCE_FX[i * 2];
        CHECK_MSG(labs_((long)tw_bounce(t) - want) <= 131, "bounce(%d/32) = %ld, trailer %ld",
                  i, (long)tw_bounce(t), want);
    }
    /* and between the points the interpolation stays within 0.5% */
    for (i = 1; i < 58; i += 2) {
        fix16 t = (fix16)(i << 10);
        CHECK_MSG(labs_((long)tw_bounce(t) - TC_BOUNCE_FX[i]) <= 328, "bounce(%d/64)", i);
    }
    /* the peak is 1.139 at t ~ 0.42 */
    CHECK(tw_bounce(0x6B85) > 74000);
    /* tw_smooth is tk_ease to +-1/256 */
    for (i = 0; i <= 16; i++)
        CHECK_MSG(labs_((tw_smooth((fix16)(i << 12)) + 128) / 256 - TC_EASE_Q8[i]) <= 1,
                  "smooth(%d/16)", i);
}

/* CT Appendix A, verbatim (the ROM tables must never drift) */
typedef struct { const TwTab *t; int id; int n; int16_t v[12]; } TabRow;
static const TabRow rows[] = {
    { &TW_POP_BUBBLE,   TWT_POP_BUBBLE,   5, { 102, 307, 236, 266, 256 } },
    { &TW_POP_LETTER,   TWT_POP_LETTER,   5, { 102, 333, 230, 269, 256 } },
    { &TW_POP_SOUL,     TWT_POP_SOUL,     5, {  77, 333, 236, 269, 256 } },
    { &TW_POP_LOCKUP,   TWT_POP_LOCKUP,   6, {  51, 346, 230, 271, 251, 256 } },
    { &TW_POP_EYE,      TWT_POP_EYE,      5, {  77, 346, 218, 282, 256 } },
    { &TW_POP_MENU,     TWT_POP_MENU,     6, {  38, 141, 287, 243, 264, 256 } },
    { &TW_POP_BLOB,     TWT_POP_BLOB,     5, {  51, 320, 230, 269, 256 } },
    { &TW_POP_BURST,    TWT_POP_BURST,    6, {  77, 205, 410, 486, 410, 410 } },
    { &TW_SLAM_STAMP,   TWT_SLAM_STAMP,   5, { 614, 410, 236, 266, 256 } },
    { &TW_SLAM_COUNTER, TWT_SLAM_COUNTER, 6, { 563, 384, 218, 282, 248, 256 } },
    { &TW_SLAM_STICKER, TWT_SLAM_STICKER, 5, { 410, 320, 236, 266, 256 } },
    { &TW_SLAM_STICK2,  TWT_SLAM_STICK2,  4, { 461, 333, 243, 256 } },
    { &TW_DROP_TITLE_SX, TWT_DROP_TITLE_SX, 6, { 353, 307, 230, 246, 264, 256 } },
    { &TW_DROP_TITLE_SY, TWT_DROP_TITLE_SY, 6, { 159, 205, 287, 269, 251, 256 } },
    { &TW_DROP_TITLE_DY, TWT_DROP_TITLE_DY, 6, {   0,   0,  -4,  -4,  -1,   0 } },
    { &TW_DROP_LET_SX,  TWT_DROP_LET_SX,  5, { 333, 287, 241, 264, 256 } },
    { &TW_DROP_LET_SY,  TWT_DROP_LET_SY,  5, { 184, 230, 276, 251, 256 } },
    { &TW_DROP_LET_DY,  TWT_DROP_LET_DY,  5, {   0,   0,  -3,  -1,   0 } },
    { &TW_DROP_SOFT_SX, TWT_DROP_SOFT_SX, 6, { 312, 282, 243, 251, 261, 256 } },
    { &TW_DROP_SOFT_SY, TWT_DROP_SOFT_SY, 6, { 200, 230, 274, 264, 253, 256 } },
    { &TW_DROP_SOFT_DY, TWT_DROP_SOFT_DY, 6, {   6,   3,  -3,  -2,   0,   0 } },
    { &TW_HEART_LAND_SQ, TWT_HEART_LAND_SQ, 3, { 87, 61, 20 } },
    { &TW_DEVICE_DY,    TWT_DEVICE_DY,    7, { 5, 3, -3, -3, -1, 1, 0 } },
    { &TW_SIGN_DY,      TWT_SIGN_DY,      6, { -14, -6, 2, 0, -1, 0 } },
    { &TW_CLUNK,        TWT_CLUNK,        5, { 210, 236, 271, 261, 256 } },
    { &TW_CLAP,         TWT_CLAP,         5, { 230, 241, 266, 261, 256 } },
    { &TW_WINDOW_BOING_DY, TWT_WINDOW_BOING_DY, 11, { 160, 120, 70, 28, -2, -9, -6, 1, 3, 1, 0 } },
    { &TW_BRAKE_DX,     TWT_BRAKE_DX,     8, { 6, 8, 6, 2, -1, -2, -1, 0 } },
    { &TW_ZOOM_WOBBLE_DY, TWT_ZOOM_WOBBLE_DY, 4, { 6, -3, 1, 0 } },
    { &TW_HOP1,         TWT_HOP1,         6, { -3, -2, 0, 1, 0, 0 } },
    { &TW_HOP2,         TWT_HOP2,         6, { -7, -4, 1, 2, 1, 0 } },
    { &TW_TAG_DY,       TWT_TAG_DY,       3, { -3, 1, 0 } },
    { &TW_DEADPAN_DY,   TWT_DEADPAN_DY,   5, { 8, 6, 4, 2, 0 } },
};

TEST(tween_tables_equal_ct_appendix_a)
{
    int i, k;
    CHECK_EQ((int)(sizeof rows / sizeof rows[0]), TWT_COUNT);
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        const TabRow *r = &rows[i];
        CHECK_MSG(r->t->n == r->n, "table %d has %d entries", i, r->t->n);
        CHECK(tw_table(r->id) == r->t);
        for (k = 0; k < r->n; k++)
            CHECK_MSG(r->t->v[k] == r->v[k], "table %d[%d] = %d, CT %d", i, k, r->t->v[k], r->v[k]);
        /* clamps: before = first, after = last */
        CHECK_EQ(tw_tab(r->t, -5), r->v[0]);
        CHECK_EQ(tw_tab(r->t, 99), r->v[r->n - 1]);
    }
    CHECK(tw_table(-1) == NULL);
    CHECK(tw_table(TWT_COUNT) == NULL);
    /* every scale table settles at 1.0 (or 1.6x for the burst), every offset at 0 */
    CHECK_EQ(tw_tab(&TW_SLAM_STAMP, 4), 256);
    CHECK_EQ(tw_tab(&TW_POP_BURST, 40), 410);
}

TEST(tween_springs_match_trailer_curves)
{
    int p, n;
    for (p = 0; p < TW_SPRING_COUNT; p++) {
        TwSpring s;
        CHECK_EQ(TC_SPRINGS, TW_SPRING_COUNT);
        tw_spring_init(&s, p, 0);
        CHECK_EQ(s.a1, TC_SPRING[p].a1);
        CHECK_EQ(s.a2, TC_SPRING[p].a2);
        CHECK_EQ(tw_spring_step(&s), 0);                      /* at rest: stays */
        /* a 64 px step from rest (Q4): frame 0 is the old position, then
         * the exact continuous curve to +-1/16 px */
        tw_spring_target(&s, 1024);
        for (n = 0; n < TC_STEPS; n++) {
            int32_t pos = n == 0 ? s.target + s.e : tw_spring_step(&s);
            CHECK_MSG(labs_(pos - TC_SPRING[p].pos[n]) <= 1, "spring %d frame %d: %ld, want %d",
                      p, n, (long)pos, TC_SPRING[p].pos[n]);
        }
        /* and it settles exactly */
        for (n = 0; n < 400 && !tw_spring_resting(&s); n++) tw_spring_step(&s);
        CHECK_MSG(tw_spring_resting(&s), "spring %d never rests", p);
        CHECK_EQ(tw_spring_step(&s), 1024);
    }
}

TEST(tween_spring_punch_shove_retarget)
{
    TwSpring s;
    int n, peak = 0;
    int32_t prev;
    /* punch: an offset that snaps back elastically (TW_SLAM4 = bounce(a/4)) */
    tw_spring_init(&s, TW_SLAM4, 160);
    tw_spring_punch(&s, 64);
    CHECK_EQ(s.target + s.e, 224);
    for (n = 0; n < 40; n++) tw_spring_step(&s);
    CHECK_EQ(tw_spring_step(&s), 160);
    /* shove: a velocity impulse moves it, then it returns */
    tw_spring_shove(&s, 32);
    prev = tw_spring_step(&s);
    CHECK(prev > 160);
    for (n = 0; n < 60; n++) tw_spring_step(&s);
    CHECK(tw_spring_resting(&s));
    /* retarget mid-flight keeps the velocity: no jump in position */
    tw_spring_init(&s, TW_FOLLOW, 0);
    tw_spring_target(&s, 1600);
    for (n = 0; n < 4; n++) prev = tw_spring_step(&s);
    tw_spring_target(&s, 3200);
    CHECK(s.target + s.e == prev);
    for (n = 0; n < 200; n++) { int32_t p = tw_spring_step(&s); if (p > peak) peak = p; }
    CHECK(peak >= 3200 && peak < 3400);                       /* zeta 0.85: barely overshoots */
}

TEST(tween_curves_and_squash)
{
    TwSquash q;
    CHECK_EQ(tw_t(0, 10), 0);
    CHECK_EQ(tw_t(5, 10), FIX_HALF);
    CHECK_EQ(tw_t(12, 10), FIX_ONE);
    CHECK_EQ(tw_t(3, 0), FIX_ONE);
    CHECK_EQ(tw_t(-3, 10), 0);
    CHECK(tw_t(30000, 60000) > FIX_HALF - 300 && tw_t(30000, 60000) < FIX_HALF + 300);
    CHECK_EQ(tw_in2(FIX_HALF), FIX_ONE / 4);
    CHECK_EQ(tw_out2(FIX_HALF), FIX_ONE * 3 / 4);
    CHECK_EQ(tw_smooth(0), 0);
    CHECK_EQ(tw_smooth(FIX_ONE), FIX_ONE);
    CHECK_EQ(tw_back(0, 0x13333), 0);
    CHECK_EQ(tw_back(FIX_ONE, 0x13333), FIX_ONE);
    {
        int i, peak = 0;
        for (i = 0; i <= 64; i++) { int v = tw_back((fix16)(i << 10), 0x13333); if (v > peak) peak = v; }
        CHECK(peak > FIX_ONE + 1000 && peak < FIX_ONE + FIX_ONE / 10);   /* about 5% */
    }
    CHECK_EQ(tw_lerp(10, 30, FIX_HALF), 20);
    CHECK_EQ(tw_lerp(-100, 100, FIX_ONE / 4), -50);
    CHECK_EQ(tw_lerp(0, 1 << 20, FIX_HALF), 1 << 19);
    /* squash: full amount = the tables, 0 = rest on the same frames */
    q = tw_squash(&TW_DROP_TITLE_SX, &TW_DROP_TITLE_SY, &TW_DROP_TITLE_DY, 2, 256);
    CHECK_EQ(q.sx, 230); CHECK_EQ(q.sy, 287); CHECK_EQ(q.dy, -4);
    q = tw_squash(&TW_DROP_TITLE_SX, &TW_DROP_TITLE_SY, &TW_DROP_TITLE_DY, 2, 128);
    CHECK_EQ(q.sx, 243); CHECK_EQ(q.sy, 271); CHECK_EQ(q.dy, -2);
    q = tw_squash(&TW_DROP_TITLE_SX, &TW_DROP_TITLE_SY, &TW_DROP_TITLE_DY, 2, 0);
    CHECK_EQ(q.sx, 256); CHECK_EQ(q.sy, 256); CHECK_EQ(q.dy, 0);
    q = tw_squash(NULL, &TW_CLUNK, NULL, 0, 256);
    CHECK_EQ(q.sx, 256); CHECK_EQ(q.sy, 210); CHECK_EQ(q.dy, 0);
    /* OPT_UI_BOUNCE -> amount */
    test_reset();
    opt_set(OPT_UI_BOUNCE, 0); CHECK_EQ(tw_bounce_amount(), 0);
    opt_set(OPT_UI_BOUNCE, 1); CHECK_EQ(tw_bounce_amount(), 128);
    opt_set(OPT_UI_BOUNCE, 2); CHECK_EQ(tw_bounce_amount(), 256);
}
