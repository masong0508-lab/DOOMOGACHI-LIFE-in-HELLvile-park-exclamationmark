/* sc44.c - SC44 (3-05) INT. BAKE TENT - NIGHT (FIRST PERSON). 30 s, 900 f.
 * The Big Puff sits on Nana's stall like a hen on an egg, the recipe tin
 * under it. It spits (two hearts to one); the pet's HUD face grows a horn
 * nub; "beep." and the whole Big Puff flinches. BLAM: the last shell, half a
 * face. CLICK. The slot spins: WELDED SAW, PURRING SAW -> MEATCLEAVER. The
 * saw goes all the way down; the geyser; the boss pop; the eyeball on the tin.
 * REWIND to the filter: hearts, a tuft on the tin, BAKE TEN. No child here. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "cast.h"
#include "show.h"
#include "fx.h"
#include "fp.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) X(fp_tent, 30.0)
#include "scene_tl.h"

static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

#define F_SPIT 130
#define F_NUB  150
#define F_BEEP 200
#define F_BLAM 260
#define F_CLICK 360
#define F_SPIN 380
#define F_REV  420
#define F_SAW  450
#define F_POP  600
#define F_EYE  630
#define F_REW  660
#define F_CUTE 681
#define F_LIFT 760
#define F_TURN 800
#define F_SIGN 840

static char cell(int x, int y) { return (x < 0 || y < 0 || x > 6 || y > 6 || x == 0 || y == 0 || x == 6 || y == 6) ? '#' : '.'; }
static int cute;
static uint8_t wall(char c, int mx, int my, float u, float v) {
    int tu = (int)(u * 8);
    if (v > 0.9f) return PAL_PAINT(PAINT_ROSE, 2);
    return (tu & 1) ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_BONE, 2);
    (void)c; (void)mx; (void)my;
}
static uint8_t floor_(float wx, float wy) { return ((int)(wx * 2) + (int)(wy * 2)) & 1 ? PAL_RAMP(RAMP_GRASS, 2) : PAL_RAMP(RAMP_GRASS, 3); }
static uint8_t ceil_(float wx, float wy) {
    if (fabsf(wx - 4.5f) < 0.6f && fabsf(wy - 3.5f) < 0.6f) return PAL_PAINT(PAINT_NAVY, 0);   /* the hole in the roof */
    return ((int)(wy * 6)) & 1 ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_BONE, 2);
}
static const FpMap MAP = { cell, wall, floor_, ceil_, PAL_PAINT(PAINT_NAVY, 0), 0.05f };

static void stall_and_tin(int f) {
    if (fp_sprite(4.0f, 3.5f, 0.15f, 0.02f)) {
        wd_rect(-30, -4, 60, 14, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 1));
        for (int k = 0; k < 8; k++) wd_rect(-30 + k * 7.5f, -4, 3.5f, 14, PAL_PAINT(PAINT_ROSE, 2), 0);
        /* NANA'S RECIPE TIN, poking out from under */
        wd_rect(10, -12, 14, 8, PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_NAVY, 1));
        wd_rect(10, -14, 14, 3, PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_NAVY, 1));
        if (f >= F_EYE && f < F_REW + 10) {   /* an eyeball rolls along the stall, stops on the lid, facing the Wall */
            float ex = lerp(-20, 17, seg(f, F_EYE, F_EYE + 25)); if (f >= F_REW) ex = lerp(17, -20, seg(f, F_REW, F_REW + 10));
            wd_blob_ol(ex, -17, 2.6f, 2.6f, 0, PAL_WHITE, PAL_PAINT(PAINT_JET, 1), 0); wd_blob(ex + 1.2f, -17, 1.1f, 1.1f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
        }
        if (f >= 760 - 40 && f < F_LIFT) wd_blob(17, -16, 3, 2, 0, PAL_WHITE, WB_OUT);   /* a tuft of stuffing on the lid */
    }
}
static void big_puff(int f) {
    if (f >= F_SAW + 150 && f < F_REW) return;   /* gone after the boss pop */
    float flinch = (f >= F_BEEP + 5 && f < F_BEEP + 15) ? -0.06f : 0;
    float lunge = f >= F_CLICK - 20 && f < F_CLICK + 5 ? seg(f, F_CLICK - 20, F_CLICK) * 0.9f : 0;
    float split = seg(f, F_SAW, F_POP);
    if (!fp_sprite(4.6f - lunge, 3.5f, 0.6f + flinch, 0.02f)) return;
    if (f >= F_CUTE) return;
    float s = 9;
    /* it sits on the stall like a hen on an egg; its eyes roll down to the lens */
    for (int side = -1; side <= 1; side += 2) {
        float off = side * split * 18;
        for (int k = 0; k < 10; k++) { float a = k * 0.628f + f * 0.02f; float bx = cosf(a) * 3 * s, by = sinf(a) * 2.4f * s; if (bx * side < -2 && split > 0) continue; wd_blob(bx + off, by, 2.6f * s, 2.4f * s, 0, PAL_PAINT(PAINT_GRAPE, 1), 0); }
    }
    if (split == 0) {
        wd_blob(0, 0, 3.8f * s, 3.4f * s, 0, PAL_PAINT(PAINT_GRAPE, 2), 0);
        for (int k = -1; k <= 1; k += 2) {
            wd_tri(k * 3 * s, -3.6f * s, k * 1.6f * s, -4.6f * s, k * 3.6f * s, -7.6f * s, PAL_PAINT(PAINT_BONE, 2));
            int gone = f >= F_BLAM && k > 0;
            if (!gone) { wd_blob(k * 1.6f * s, -0.6f * s + (f < 100 ? seg(f, 20, 60) * 0.6f * s : 0.6f * s), 0.9f * s, 0.9f * s, 0, PAL_PAINT(PAINT_CHERRY, 1), 0); }
        }
        float mo = 1.0f + lunge * 1.5f;
        wd_blob(-0.4f * s, 1.6f * s, 2.4f * s * mo, 1.0f * s * mo, 0, PAL_PAINT(PAINT_JET, 1), 0);
        for (int k = -2; k <= 2; k++) wd_tri(k * 0.9f * s - 0.4f * s, 0.9f * s, k * 0.9f * s + 0.4f * s, 0.9f * s, k * 0.9f * s, 1.8f * s, PAL_WHITE);
        if (f >= F_BLAM) wd_blob(1.8f * s, 0, 2.2f * s, 3.2f * s, 0, PAL_PAINT(PAINT_CHERRY, 0), 0);   /* half its face is gone */
    }
}

void scene_fp_tent(int f, int n) {
    cute = f >= F_CUTE;
    float yaw = 0, cxp = 1.4f;
    if (f >= F_TURN) { yaw = 3.14159f * seg(f, F_TURN, F_TURN + 30); cxp = lerp(1.4f, 1.15f, seg(f, F_TURN, F_TURN + 30)); }
    float hit = (f >= F_BLAM && f < F_BLAM + 3) || (f >= F_POP && f < F_POP + 8);
    int fr = hit ? (f >= F_POP ? F_POP : F_BLAM) : f;      /* hit-stops */
    if (f < F_SIGN) {
        fp_camera(cxp, 3.5f, yaw, (fr >= F_SAW && fr < F_POP) ? ((fr & 1) ? 2 : -2) : 1.0f * sinf(fr * 0.2f));
        fp_render(&MAP);
        big_puff(fr);
        stall_and_tin(fr);   /* the tin pokes out from under it */
        /* the geyser through the roof, red rain, then (after the rewind) a shower of hearts */
        if (fr >= F_SAW && fr < F_REW) { float sx, sy, sc; if (fp_project(4.6f, 3.5f, 0.9f, &sx, &sy, &sc)) { tk_gibs((int)sx, (int)sy, (fr - F_SAW) % 30, fr / 30); for (int k = 0; k < 20; k++) { uint32_t h = tk_hash(k * 13u + fr); tk_rect(h % TW, (int)((h >> 8) % FP_VH + fr * 3) % FP_VH, 1, 3, PAL_PAINT(PAINT_CHERRY, 1)); } } }
        if (fr >= F_REW && fr < F_CUTE) { float sx, sy, sc; if (fp_project(4.6f, 3.5f, 0.6f, &sx, &sy, &sc)) tk_hearts((int)sx, (int)sy, (fr - F_REW) * 2, 5); }
        if (cute) for (int k = 0; k < 14; k++) { uint32_t h = tk_hash(k * 7u + 3u); int x = (int)(h % TW), y = (int)((h >> 8) % 140 + (f - F_CUTE) * (0.5f + (h % 3) * 0.2f)) % FP_VH; tk_heart(x, y, 1, (h & 1) ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 2)); }
        /* hellify, then the REWIND runs it backwards over 21 f */
        int hell = f < F_REW ? 16 : (int)(16 * (1 - seg(f, F_REW, F_CUTE)));
        tk_post_hellify(hell);
        /* lens: the spit; the red lens after the pop (ramps in 8 f, drains up in the rewind) */
        fp_lens_blood(fr - F_SPIT, 44);
        if (fr >= F_POP && f < F_CUTE) {
            int lvl = f < F_REW ? (int)(10 * seg(f, F_POP, F_POP + 8)) : (int)(10 * (1 - seg(f, F_REW, F_CUTE)));
            tk_dither_rect(0, 0, TW, FP_VH, PAL_PAINT(PAINT_CHERRY, 0), lvl);
        }
        if (f == F_POP) tk_post_flash(PAL_WHITE, 8);
        /* weapons: the pump to the CLICK, then the saw; then Sunny's paws lift the tin */
        if (fr < F_SPIN) {
            int fire = fr >= F_BLAM ? fr - F_BLAM : -1, pump = fr >= F_BLAM + 10 && fr < F_BLAM + 22 ? fr - F_BLAM - 10 : -1;
            fp_shotgun(fire, pump, 1);
        } else if (!cute) {
            float rev = fr >= F_REV ? 1 : 0.2f, raise = seg(fr, F_SPIN + 10, F_REV), dip = fr >= F_SAW && fr < F_POP ? 30 * seg(fr, F_SAW, F_POP) : 0;
            fp_saw(fr, rev, raise, dip);
        }
        if (cute && f >= F_LIFT) {   /* the paws lift the tin into frame */
            float u = seg(f, F_LIFT, F_LIFT + 20);
            wd_cam(TW * 0.5f, TH * 0.5f, 1);
            wd_rect(96, 150 - u * 50, 48, 30, PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_NAVY, 1));
            wd_rect(96, 146 - u * 50, 48, 6, PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_NAVY, 1));
            tk_text(104, (int)(160 - u * 50), "RECIPES", PAL_PAINT(PAINT_NAVY, 1), 1);
            wd_blob_ol(92, 166 - u * 50, 10, 9, 0, PAL_WHITE, PAL_PAINT(PAINT_JET, 1), WB_SHADE);
            wd_blob_ol(148, 166 - u * 50, 10, 9, 0, PAL_WHITE, PAL_PAINT(PAINT_JET, 1), WB_SHADE);
        }
    } else {
        /* outside, looking up at the sign over the flap. ONE CHANGE: BAKE TEN */
        static const uint8_t cols[] = { PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_GRAPE, 1) };
        tk_vgrad(0, 0, TW, FP_VH, cols, 2);
        for (int i = 0; i < 6; i++) tk_rect(i * 40, 50, 20, 80, PAL_PAINT(PAINT_MINT, 2)), tk_rect(i * 40 + 20, 50, 20, 80, PAL_PAINT(PAINT_BONE, 2));
        for (int i = 0; i < 12; i++) tk_ellipse(i * 20 + 10, 50, 10, 7, (i & 1) ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_ROSE, 2));
        tk_rect(64, 14, 112, 26, PAL_PAINT(PAINT_ROSE, 2)); tk_frame(64, 14, 112, 26, PAL_PAINT(PAINT_ROSE, 0));
        tk_text(86, 20, "BAKE TEN", PAL_PAINT(PAINT_MAGENTA, 0), 2);
        for (int k = 0; k < 10; k++) { uint32_t h = tk_hash(k * 5u); tk_heart((int)(h % TW), (int)((h >> 8) % 60 + (f - F_SIGN)) % 60, 1, PAL_PAINT(PAINT_ROSE, 2)); }
    }
    FpHud h = { 0 };
    h.hearts = f < F_SPIT ? 2 : 1; h.shells = f < F_BLAM ? 1 : 0; h.brutal = 1; h.mode = cute ? "CUTE" : "SLAUGHTER";
    h.pet_look = (f / 20) % 3 - 1; h.horn = f >= F_NUB && f < F_CUTE; h.pet_mood = cute ? 1 : f >= F_NUB ? -1 : 0;
    h.sticker_age = -1; h.sticker_torn_age = -1;
    if (f >= F_SPIN && !cute) {
        static const char *SLOT[4] = { "PARTY PUMP", "PLUSH LAUNCHER", "MINIGUN", "WELDED SAW" };
        h.weapon = f < F_SPIN + 20 ? SLOT[(f / 2) % 4] : "WELDED SAW";
        h.plate = f < F_SPIN + 24 ? "PURRING SAW" : "MEATCLEAVER";
        h.shells = 0;
    }
    if (cute) { h.weapon = "PARTY PUMP"; h.plate = "PURRING SAW"; }
    fp_hud(&h, f);
    if (f >= F_BEEP && f < F_BEEP + 30) { bubble_side = 2; bubble("beep.", 5, 130, FP_VH + 2); bubble_side = 0; tk_text(6, 6, "[beep.]", PAL_WHITE, 1); }
    if (fr >= F_BLAM && fr < F_BLAM + 8) tk_text_outline(10, 10, "BLAM", PAL_WHITE, PAL_UI_RED_D, 2);
    if (f >= F_CLICK && f < F_CLICK + 14) tk_text_outline(100, 60, "click", PAL_WHITE, PAL_PAINT(PAINT_CHARCOAL, 1), 1);
    if (f >= F_SPIN && f < F_SPIN + 20) tk_text_outline(64, 100, "brrrrrt", PAL_WHITE, PAL_PAINT(PAINT_CHARCOAL, 1), 1);
    if (f >= F_EYE + 25 && f < F_EYE + 34) tk_text(150, 70, "tik", PAL_WHITE, 1);
    (void)n;
}
void scene_fp_tent_audio(double t0, double dur) { (void)t0; (void)dur; }

void tk_music(void) {
#define FT(fr) ((fr) / 30.0)
    static const int R[8] = { 40, 40, 43, 40, 46, 45, 43, 38 };
    for (int i = 0; ; i++) {             /* the insert song, DOOM side; stop-time at the CLICK; back in at f384 */
        double t = i * 0.2;
        if (t >= FT(F_REW)) break;
        if (t >= FT(F_CLICK) && t < FT(384)) continue;
        int m = R[(i / 2) % 8];
        tk_note(t, 0.18, m, TKV_SAW, 0.12f); tk_note(t, 0.18, m + 7, TKV_SAW, 0.06f);
        if (i % 2 == 0) tk_kick(t, 0.2f);
        if (i % 4 == 2) tk_snare(t, 0.15f);
        if (t >= FT(282) && t < FT(330) && i % 2 == 0) tk_note(t, 0.18, 76 + (i % 3) * 2, TKV_TRI, 0.12f);   /* "Say it out loud!" sweetly */
    }
    tk_sfx(FT(F_SPIT), SFX_SPLAT, 0.6f);
    tk_note(FT(F_NUB), 0.08, 88, TKV_SQ12, 0.2f);                         /* pop: the horn nub */
    line_at("a3_05_pet_01", FT(F_BEEP));
    tk_sfx(FT(F_BLAM), SFX_SHOTGUN, 0.8f); tk_sfx(FT(F_BLAM) + 0.05, SFX_SPLAT, 0.5f);
    tk_note(FT(F_BLAM + 20), 0.04, 98, TKV_SINE, 0.2f);                  /* a tooth tinks off the lens */
    tk_note(FT(F_BLAM + 30), 0.05, 66, TKV_NOISE, 0.25f); tk_note(FT(F_BLAM + 36), 0.05, 62, TKV_NOISE, 0.25f);   /* CHK-CHK */
    tk_note(FT(F_CLICK), 0.03, 80, TKV_NOISE, 0.3f);                     /* CLICK. Empty. */
    for (int k = 0; k < 20; k++) tk_note(FT(F_SPIN) + k * 0.033, 0.03, 70 + (k % 4), TKV_SQ12, 0.08f);   /* brrrrrt */
    tk_slide(FT(F_REV), 0.5, 55, 70, TKV_SAW, 0.12f);                    /* it purrs... */
    tk_slide(FT(F_REV + 15), 5.0, 70, 82, TKV_SAW, 0.18f);               /* ...then it SCREAMS, all the way down */
    tk_sfx(FT(F_POP), SFX_EXPLOSION, 0.6f);
    tk_note(FT(F_EYE + 25), 0.04, 98, TKV_SINE, 0.2f);                   /* tik */
    tk_slide(FT(F_REW), FT(F_CUTE - F_REW), 50, 90, TKV_NOISE, 0.25f);   /* the tape rewind */
    static const int C[8] = { 72, 76, 79, 76, 77, 74, 72, 0 };           /* a cute bed */
    for (int i = 0; i < 28; i++) { double t = FT(F_CUTE) + i * 0.25; if (C[i % 8]) tk_note(t, 0.22, C[i % 8], TKV_SINE, 0.16f); }
    tk_sfx(FT(F_SIGN), SFX_SPARKLE, 0.2f);
}
