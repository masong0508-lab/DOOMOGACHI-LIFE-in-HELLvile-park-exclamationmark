/* test_dialog_hold.c - SP5 (G-D1): the held dialogue backdrop (overworld.c dh_*, plat_ext.h plat_hold_band).
 *
 * Under a dialogue box the story overworld keeps its last world frame and puts back only the band the box draws
 * in. The native platform implements the band (core/native/plat_native.c), so the same run is played twice, with
 * the band off (nat_band_off: every frame drawn in full, as before SP5) and on, and compared frame by frame:
 *   - a frame that is not held (before the first hold, while a toast or a growing choice window interrupts it,
 *     after the box closes) is identical to the full run's: a hold leaves nothing behind;
 *   - a held frame differs from the full run's only where the full run's picture changed since the frame the
 *     band was kept on (the world's clock-driven idle animations, which a hold pauses, and the dialogue itself),
 *     and the box's opaque body (the core rect of ui_dialog_band) is identical;
 *   - the hold engages on most frames of a long dialogue.
 * At 240x160 and 320x240. */
#include <stdlib.h>
#include "test.h"
#include "../src/story_int.h"
#include "doomo/plat_ext.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static void set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

static void story_town(void)
{
    test_reset();
    story_debug_autowin(1);
    town_new(TOWN_STORY, 4242, TOWN_POP_PREMADE);
    sim_init();
}

static int first_resident(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = town_res(i);
        if (r && r->status != RES_EMPTY) return i;
    }
    return -1;
}

typedef void (*Setup)(void);
typedef uint32_t (*Pad)(int f);

/* a narration over story beat 110 (no portrait), advanced twice */
static void s_narration(void) { story_town(); scene_set(&SCENE_STORY, 110); }
static uint32_t p_narration(int f)
{
    if (f == 250 || f == 330) return BTN_A;
    return 0;
}

/* portrait lines with the name pill, a shout, a legacy line and a 3-option choice, a toast in the middle,
 * then the box closes and the world walks on */
static void s_portrait(void)
{
    static const char *const opts[3] = { "Yes", "No way", "Maybe?" };
    int id;
    story_town();
    scene_set(&SCENE_STORY, 10);
    id = first_resident();
    ui_say_ex(id, EXPR_HAPPY, DL_PILL, "Hello there!{.} This line has a name pill / and two lines.");
    ui_say_ex(id, EXPR_NEUTRAL, DL_SHOUT, "WHAT.{!} A shout!");
    ui_say(id, "A legacy line with the name plate.");
    ui_choice(id, "Pick one:", opts, 3, NULL);
}
static uint32_t p_portrait(int f)
{
    if (f == 140) ui_toast("A toast over the box");
    if (f < 400 && f % 90 == 89) return BTN_A;
    if (f > 300 && f < 400 && f % 30 == 15) return BTN_DOWN;
    if (f > 420 && f < 460) return BTN_RIGHT;
    return 0;
}

/* the full run: every frame kept */
static uint8_t *full_run(Setup s, Pad pad, int n)
{
    size_t fs = (size_t)nat_w * (size_t)nat_h;
    uint8_t *buf = (uint8_t *)malloc(fs * (size_t)n);
    int f;
    if (!buf) return NULL;
    nat_band_off = 1;
    doomo_init();
    s();
    for (f = 0; f < n; f++) {
        frame(pad(f));
        memcpy(buf + fs * (size_t)f, nat_fb, fs);
    }
    nat_band_off = 0;
    return buf;
}

static int check(const char *name, Setup s, Pad pad, int n, int w, int h, int min_held, int min_after)
{
    size_t fs;
    uint8_t *off;
    int f, held = 0, after = 0, save_f = -1, ok = 1;
    char m[300];
    set_res(w, h);
    fs = (size_t)w * (size_t)h;
    off = full_run(s, pad, n);
    if (!off) { test_fail(__FILE__, __LINE__, "no memory for the full run"); return 0; }
    nat_band_off = 0;
    doomo_init();
    s();
    for (f = 0; f < n && ok; f++) {
        uint32_t r0 = nat_band_restores, s0 = nat_band_saves;
        const uint8_t *o = off + fs * (size_t)f;
        frame(pad(f));
        if (nat_band_saves != s0) save_f = f;
        if (nat_band_restores == r0) {                 /* a full frame: the same picture */
            if (memcmp(o, nat_fb, fs)) {
                snprintf(m, sizeof(m), "%s %dx%d: full frame %d differs from the run without holds", name, w, h, f);
                test_fail(__FILE__, __LINE__, m);
                ok = 0;
            }
            if (held) after++;
            continue;
        }
        held++;
        {
            const uint8_t *k = off + fs * (size_t)save_f;
            int i, y, x0, y0, x1, y1;
            if (save_f < 0 || save_f >= f) {
                test_fail(__FILE__, __LINE__, "a held frame without a kept band before it");
                ok = 0;
                break;
            }
            for (i = 0; i < (int)fs; i++)
                if (nat_fb[i] != o[i] && o[i] == k[i]) {
                    snprintf(m, sizeof(m), "%s %dx%d: held frame %d differs at (%d,%d) where nothing changed since "
                             "frame %d", name, w, h, f, i % w, i / w, save_f);
                    test_fail(__FILE__, __LINE__, m);
                    ok = 0;
                    break;
                }
            if (ok && ui_dialog_band(&y, &x0, &y0, &x1, &y1))
                for (i = y0; i < y1 && ok; i++)
                    if (memcmp(nat_fb + i * w + x0, o + i * w + x0, (size_t)(x1 - x0))) {
                        snprintf(m, sizeof(m), "%s %dx%d: held frame %d: the box body differs on row %d", name, w,
                                 h, f, i);
                        test_fail(__FILE__, __LINE__, m);
                        ok = 0;
                    }
        }
    }
    free(off);
    if (!ok) return 0;
    if (held < min_held || after < min_after) {
        snprintf(m, sizeof(m), "%s %dx%d: %d frames held (want >= %d), %d full frames after a hold (want >= %d)",
                 name, w, h, held, min_held, after, min_after);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    printf("      dialog hold %-9s %dx%d: %d of %d frames held, %d full frames after\n", name, w, h, held, n, after);
    return 1;
}

TEST(dialog_hold_narration_is_exact_but_for_the_paused_world)
{
    CHECK(check("narration", s_narration, p_narration, 400, 240, 160, 160, 0));
    CHECK(check("narration", s_narration, p_narration, 400, 320, 240, 160, 0));
    set_res(240, 160);
}

TEST(dialog_hold_portrait_choice_toast_and_close)
{
    CHECK(check("portrait", s_portrait, p_portrait, 480, 240, 160, 140, 40));
    CHECK(check("portrait", s_portrait, p_portrait, 480, 320, 240, 140, 40));
    set_res(240, 160);
}

/* the platform default (no band): nothing is held, the story draws as before */
TEST(dialog_hold_default_platform_draws_in_full)
{
    PlatBand b;
    memset(&b, 0, sizeof(b));
    nat_band_off = 1;
    CHECK_EQ(plat_hold_band(PLAT_BAND_SAVE, &b), -1);
    CHECK_EQ(plat_hold_band(PLAT_BAND_RESTORE, &b), 0);
    nat_band_off = 0;
}
