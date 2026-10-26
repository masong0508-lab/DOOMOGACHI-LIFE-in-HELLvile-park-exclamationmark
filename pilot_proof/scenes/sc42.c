/* sc42.c - SC42 (3-03) INT. THE ABSOLUTELY NOT SCARY HOUSE - NIGHT (FIRST
 * PERSON). 40 s, 1,200 f, no dialogue. HELLIFY rots in over 21 f; the house
 * front and its sign; the door; the HUD slams up (GORE: BRUTAL), the Board of
 * Decency sticker, the first BLAM tears it off. Wave 1 (cardboard ghosts),
 * the cardboard ghost's back, the knock-knock, wave 2 (the room of doors),
 * PARTY POPPERS, wave 3 (the costume room), the EXIT and the Big Puff's
 * shadow on the bake tent. Gore is on demons only; no child in this scene. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "cast.h"
#include "show.h"
#include "fx.h"
#include "fp.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) X(fp_house, 40.0)
#include "scene_tl.h"

static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

/* ---- the map: outside (x<3), the front with the door, the corridor of
 * cardboard ghosts (x 4-13), the big room (doors, then the costumes), the EXIT */
static int door_open;
static char cell(int x, int y) {
    if (x < 0 || y < 0 || x > 27 || y > 6) return '#';
    if (y == 0 || y == 6 || x == 0) return '#';
    if (x == 27) return y == 3 ? 'E' : '#';
    if (x == 3) return y == 3 ? (door_open ? '.' : 'D') : 'F';
    if (x >= 4 && x <= 13 && (y == 1 || y == 5)) return 'C';
    return '.';
}
static uint8_t wall(char c, int mx, int my, float u, float v) {
    int tu = (int)(u * 16), tv = (int)(v * 16);
    switch (c) {
    case 'F': {   /* the house front: lavender siding; a painted smiling ghost by the door */
        if (tv % 4 == 0) return PAL_PAINT(PAINT_LAVENDER, 0);
        if (my == 2 || my == 4) { float gx = u - 0.5f, gy = v - 0.55f; if (gx * gx * 3 + gy * gy * 2 < 0.09f) return (fabsf(gx + (my == 2 ? 0.1f : -0.1f)) < 0.05f && fabsf(gy - 0.05f) < 0.05f) ? PAL_PAINT(PAINT_JET, 1) : PAL_WHITE; }
        return PAL_PAINT(PAINT_LAVENDER, 1);
    }
    case 'D': return (tu == 0 || tu == 15 || tv == 15) ? PAL_PAINT(PAINT_GRAPE, 0) : (tu == 12 && tv == 7) ? PAL_UI_GOLD : PAL_PAINT(PAINT_GRAPE, 1);
    case 'C': {   /* cardboard wallpaper: pastel stripes, a cardboard edge */
        if (tv == 2) return PAL_PAINT(PAINT_CARAMEL, 0);
        return (tu / 4) & 1 ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_BONE, 2);
    }
    case 'E': {   /* the EXIT: a strip of night; the bake tent's canvas, lit, with the Big Puff's shadow */
        if (u < 0.08f || u > 0.92f || v > 0.92f) return PAL_PAINT(PAINT_CHOCOLATE, 0);
        if (v > 0.75f) return PAL_PAINT(PAINT_NAVY, 0);
        float gx = u - 0.5f, gy = v - 0.35f;
        if (gx * gx + gy * gy * 1.4f < 0.07f) return PAL_PAINT(PAINT_RUST, 0);   /* the shadow that fills it */
        return (tu & 1) ? PAL_PAINT(PAINT_HONEY, 2) : PAL_PAINT(PAINT_LEMON, 2);
    }
    default:
        if (mx <= 2) return v > 0.5f ? PAL_PAINT(PAINT_NAVY, 1) : PAL_RAMP(RAMP_GRASS, 1);   /* outside: night */
        if (mx >= 18) return (tu / 2) & 1 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 2);   /* costume room */
        return (tv % 5 == 0) ? PAL_PAINT(PAINT_LAVENDER, 0) : PAL_PAINT(PAINT_LAVENDER, 2);
    }
}
static uint8_t floor_(float wx, float wy) {
    if (wx < 3) return PAL_RAMP(RAMP_GRASS, 2);
    int b = (int)floorf(wx * 4) + ((int)floorf(wy * 1) & 1) * 2;
    return (b & 3) == 0 ? PAL_PAINT(PAINT_CARAMEL, 0) : PAL_PAINT(PAINT_CARAMEL, 1);
}
static uint8_t ceil_(float wx, float wy) {
    if (wx < 3) return ((int)(wx * 13 + wy * 7) % 23 == 0) ? PAL_WHITE : PAL_PAINT(PAINT_NAVY, 0);
    return PAL_PAINT(PAINT_GRAPE, 0);
}
static const FpMap MAP = { cell, wall, floor_, ceil_, PAL_PAINT(PAINT_NAVY, 0), 0.06f };

/* ---- the run: camera path, blasts, targets -------------------------------- */
typedef struct { int f; float x; } Key;
static const Key PATH[] = { { 0, 1.2f }, { 75, 1.2f }, { 135, 2.6f }, { 190, 3.6f }, { 340, 8.5f }, { 450, 11.0f }, { 520, 13.6f },
                            { 680, 14.0f }, { 760, 15.6f }, { 990, 19.5f }, { 1200, 24.0f } };
static float cam_x(float f) {
    for (int i = 0; i < 10; i++) if (f < PATH[i + 1].f) return lerp(PATH[i].x, PATH[i + 1].x, seg(f, PATH[i].f, PATH[i + 1].f));
    return PATH[10].x;
}
/* blasts: frame, target kind, world x, y; kind 0 demon, 1 cardboard ghost (falls flat), 2 door demon, 3 sheet */
typedef struct { int f, kind; float x, y; } Blast;
static const Blast B[] = {
    { 160, 0, 5.2f, 3.0f },
    { 230, 0, 6.8f, 2.4f }, { 300, 0, 9.0f, 3.6f },
    { 360, 1, 11.6f, 3.0f },
    { 545, 2, 17.0f, 1.6f }, { 590, 2, 17.0f, 3.4f }, { 655, 2, 17.0f, 5.2f },
    { 800, 3, 21.0f, 2.0f }, { 840, 3, 21.5f, 4.0f }, { 870, 3, 22.5f, 3.0f }, { 925, 3, 23.0f, 2.2f }, { 965, 3, 23.6f, 4.2f },
};
#define NB ((int)(sizeof B / sizeof B[0]))
static int last_blast(int f) { int k = -1; for (int i = 0; i < NB; i++) if (B[i].f <= f) k = i; return k; }
static int shells_at(int f) {
    int s = 8;
    for (int i = 0; i < NB; i++) if (B[i].f <= f) s--;
    if (f >= 300) s -= 1;     /* the double-up in wave 1 */
    if (f >= 720) s = 6 - (f >= 800) - (f >= 840) - (f >= 870) - (f >= 925) - (f >= 965);
    return s < 0 ? 0 : s;
}

static void draw_target(const Blast *b, int f, float t30) {
    int age = f - b->f;
    float sx, sy, sc;
    if (b->kind == 1) {      /* a cardboard ghost: falls flat; THANK YOU FOR NOT SCREAMING on its back */
        if (!fp_project(b->x, b->y, age < 0 ? 0.5f : 0.05f, &sx, &sy, &sc)) return;
        if (age < 0) { fp_sprite(b->x, b->y, 0.5f, 0.03f); wd_blob_ol(0, 0, 9, 13, 0.1f * sinf(t30 * 0.1f), PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0), 0); wd_blob(-3, -3, 1.4f, 2, 0, PAL_PAINT(PAINT_JET, 1), 0); wd_blob(3, -3, 1.4f, 2, 0, PAL_PAINT(PAINT_JET, 1), 0); wd_line(-4, 3, 4, 3, 0.8f, PAL_PAINT(PAINT_JET, 1)); return; }
        fp_sprite(b->x, b->y, 0.02f, 0.03f);
        wd_rect(-14, -4, 28, 8, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 0));
        if (age > 20 && age < 110) {   /* the sign reads for over 2 s */
            tk_rect(40, 90, 160, 26, PAL_PAINT(PAINT_CARAMEL, 1)); tk_frame(40, 90, 160, 26, PAL_PAINT(PAINT_CHOCOLATE, 1));
            tk_text(52, 94, "THANK YOU FOR NOT", PAL_PAINT(PAINT_CHOCOLATE, 1), 1);
            tk_text(52, 104, "SCREAMING  -Mayor", PAL_PAINT(PAINT_CHOCOLATE, 1), 1);
        }
        if (age >= 0 && age < 10) tk_text_outline(150, 60, "fwap", PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0), 1);
        return;
    }
    if (age < 0) {
        if (b->kind == 2 && f < 520) return;   /* door demons wait behind their doors */
        if (b->kind == 0 && f < b->f - 40) return;
        if (!fp_sprite(b->x, b->y, 0.35f, 0.03f)) return;
        if (b->kind == 3) {   /* a bedsheet ghost costume on its hanger, red eyes in the eyeholes */
            wd_line(0, -18, 0, -14, 0.8f, PAL_GRAY);
            wd_quad(-8, -14, 8, -14, 11, 14, -11, 14, PAL_WHITE, PAL_GRAY_L);
            wd_blob(-3, -6, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0); wd_blob(3, -6, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
        } else puff_demon(0, 0, 2.2f, t30, -1);
        return;
    }
    /* BLAM: red mist, gibs, a loose eyeball whose pupil turns toward the Wall */
    if (!fp_project(b->x, b->y, 0.35f, &sx, &sy, &sc)) return;
    if (age < 26) tk_gibs((int)sx, (int)sy, age, (uint32_t)b->f);
    if (age < 10) for (int k = 0; k < 10; k++) { uint32_t h = tk_hash(b->f * 7u + k); tk_circle((int)sx + (int)(h % 40) - 20, (int)sy + (int)((h >> 6) % 30) - 15, 3 + age / 2, PAL_PAINT(PAINT_CHERRY, (h >> 3) & 1)); }
    if (b->kind == 3 && age < 60) { fp_sprite(b->x, b->y, 0.35f, 0.03f); wd_quad(-8, -14, 8, -14, 11, 14 - age * 0.3f, -11, 14 - age * 0.3f, PAL_WHITE, PAL_GRAY_L); wd_blob(0, 0, 6 + age * 0.1f, 6, 0, PAL_PAINT(PAINT_CHERRY, 0), 0); }
    if (age < 120) {
        float ex = b->x - 0.25f + fminf(age, 20) * 0.01f;
        if (fp_sprite(ex, b->y + 0.2f, 0.03f, 0.03f)) {
            wd_blob_ol(0, 0, 2.6f, 2.6f, 0, PAL_WHITE, PAL_PAINT(PAINT_JET, 1), 0);
            float look = b->f == 160 && age > 30 ? 1 : -0.4f;   /* the button: tik, toward the Wall */
            wd_blob(look * 1.0f, 0, 1.1f, 1.1f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
        }
    }
}

void scene_fp_house(int f, int n) {
    float t30 = f;
    door_open = f >= 100;
    float x = cam_x(f);
    float sway = 0.04f * sinf(f * 0.05f) + (f >= 990 ? 0 : 0);
    float bob = 1.5f * sinf(f * 0.33f) * (x != cam_x(f - 1));
    fp_camera(x, 3.5f, sway, bob);
    fp_render(&MAP);
    /* sprites, back to front */
    if (f < 520) for (int k = 0; k < 4; k++) {   /* cardboard ghosts swinging on wires (harmless) */
        float gx = 6 + k * 2, gy = (k & 1) ? 2.4f : 4.6f;
        if (fp_sprite(gx, gy, 0.55f, 0.03f)) { float sw = 0.15f * sinf(t30 * 0.06f + k); wd_line(0, -30, sinf(sw) * 6, -12, 0.5f, PAL_GRAY_L); wd_blob_ol(sinf(sw) * 6, 0, 8, 12, sw, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0), 0); wd_blob(sinf(sw) * 6 - 3, -3, 1.2f, 1.8f, 0, PAL_PAINT(PAINT_JET, 1), 0); wd_blob(sinf(sw) * 6 + 3, -3, 1.2f, 1.8f, 0, PAL_PAINT(PAINT_JET, 1), 0); wd_line(sinf(sw) * 6 - 4, 3, sinf(sw) * 6 + 4, 3, 0.8f, PAL_PAINT(PAINT_JET, 1)); }
    }
    if (f >= 140 && f < 175 && fp_sprite(5.2f, 3.0f, 0.1f, 0.03f)) wd_rect(-8, -14, 16, 16, PAL_GRAY_L, PAL_GRAY_D);   /* the cardboard tombstone */
    if (f >= 440 && f < 780) for (int k = 0; k < 6; k++) {   /* six painted doors with brass knockers */
        float dy = 1.3f + k * 0.8f;
        if (!fp_sprite(17.2f, dy, 0.5f, 0.03f)) continue;
        int open = f >= 520 + k * 6;
        wd_rect(-9, -16, 18, 32, open ? PAL_PAINT(PAINT_NAVY, 0) : PAL_PAINT(PAINT_TEAL, 1), PAL_PAINT(PAINT_TEAL, 0));
        if (!open) { float lift = (k == 0 && ((f >= 475 && f < 480) || (f >= 483 && f < 488))) ? -2 : 0; wd_blob_ol(0, -4 + lift, 2, 2, 0, PAL_UI_GOLD, PAL_PAINT(PAINT_CHOCOLATE, 0), 0); }
        if (k == 5 && f >= 655) wd_rect(-9, -16, 18, 32, PAL_PAINT(PAINT_NAVY, 0), 0);   /* most of the door with it */
    }
    if (f >= 680 && f < 760 && fp_sprite(15.4f, 3.5f, 0.2f, 0.03f)) {   /* PARTY POPPERS (over SHELLS) on a doily plinth */
        wd_rect(-6, -2, 12, 14, PAL_PAINT(PAINT_BONE, 2), PAL_GRAY);
        for (int k = 0; k < 8; k++) wd_blob(-6 + k * 1.7f, -2, 1, 1, 0, PAL_WHITE, 0);
        if (f < 735) { wd_rect(-5, -10, 10, 8, PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 0)); }
        if (f >= 690 && f < 735) { int sx = (int)wd_sx(0), sy = (int)wd_sy(-16); tk_text_outline(sx - 36, sy - 18, "PARTY POPPERS", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1); tk_text(sx - 18, sy - 8, "(SHELLS)", PAL_GRAY_L, 1); }
    }
    for (int i = NB - 1; i >= 0; i--) draw_target(&B[i], f, t30);
    if (f >= 600 && f < 640 && fp_sprite(15.2f, 3.6f, 0.35f, 0.03f)) puff_demon(0, 0, 2.6f, t30, -1);   /* door six gets to the lens first */
    /* the sign on the front, read while the rot crawls over it */
    if (f < 100) { tk_rect(70, 22, 100, 30, PAL_WHITE); tk_frame(70, 22, 100, 30, PAL_PAINT(PAINT_GRAPE, 0)); tk_text(82, 26, "NOTHING INSIDE!", PAL_PAINT(PAINT_CHERRY, 0), 1); tk_text(80, 38, "(we checked) -Mayor", PAL_PAINT(PAINT_NAVY, 1), 1); }
    /* HELLIFY: the palette rots in over 21 f and stays rotted */
    tk_post_hellify((int)(16 * seg(f, 0, 21)));
    /* lens: blood on door six's lunge */
    fp_lens_blood(f - 630, 6);
    for (int i = 0; i < NB; i++) if (f - B[i].f >= 0 && f - B[i].f < 6 && B[i].kind != 1) tk_post_shake(2, 1);
    /* the gun rises at the door; fire and pump ages from the blast list */
    int lb = last_blast(f);
    int fire = lb >= 0 ? f - B[lb].f : -1, pump = lb >= 0 ? f - B[lb].f - 10 : -1;
    if (f >= 1150) pump = f - 1150;
    fp_shotgun(fire, pump, seg(f, 75, 110));
    if (f >= 135) {
        FpHud h = { 0 };
        h.hearts = f >= 630 ? 2 : 3; h.shells = shells_at(f); h.brutal = 1;
        h.pet_look = (f / 20) % 3 - 1; h.pet_mood = (lb >= 0 && fire < 30) ? 1 : (f >= 630 && f < 660) || (f >= 360 && f < 390) ? -1 : 0;
        h.sticker_age = f >= 143 ? f - 143 : -1; h.sticker_torn_age = f >= 160 ? f - 160 : -1;
        int slam = f < 140 ? (140 - f) * 6 : 0;
        if (slam) { for (int y = TH - 1; y >= FP_VH; y--) for (int x2 = 0; x2 < TW; x2++) tk_fb[y][x2] = tk_fb[y][x2]; }
        fp_hud(&h, f);
    } else tk_rect(0, FP_VH, TW, TH - FP_VH, PAL_BLACK);
    if (f >= 475 && f < 500) tk_text(8, 8, "[knock, knock]", PAL_WHITE, 1);
    for (int i = 0; i < NB; i++) if (f - B[i].f >= 0 && f - B[i].f < 8 && B[i].kind != 1) tk_text_outline(10, 10, "BLAM", PAL_WHITE, PAL_UI_RED_D, 2);
    (void)n;
}
void scene_fp_house_audio(double t0, double dur) { (void)t0; (void)dur; }

void tk_music(void) {
#define FT(fr) ((fr) / 30.0)
    tk_note(0, 0.08, 50, TKV_NOISE, 0.3f);                                   /* —CHK */
    tk_slide(0, 0.7, 84, 48, TKV_SAW, 0.12f);                                /* the hell slide, falling */
    tk_slide(FT(80), 1.2, 50, 43, TKV_SQ12, 0.08f);                          /* the door creaks open */
    /* DOOM-side riff: detuned guitars (saw), kick and snare, 150 bpm; ducks for the knocks; tape-stops after wave 2 */
    static const int R[8] = { 40, 40, 43, 40, 46, 45, 43, 38 };
    for (int i = 0; ; i++) {
        double t = FT(39) + i * 0.2;
        if (t >= 40) break;
        if (t >= FT(470) && t < FT(500)) continue;
        if (t >= FT(690) && t < FT(770)) continue;
        int m = R[(i / 2) % 8];
        tk_note(t, 0.18, m, TKV_SAW, 0.12f); tk_note(t, 0.18, m + 7, TKV_SAW, 0.06f);
        if (i % 2 == 0) tk_kick(t, 0.2f);
        if (i % 4 == 2) tk_snare(t, 0.15f);
    }
    tk_slide(FT(685), 0.6, 52, 30, TKV_SAW, 0.15f);                          /* tape-stop */
    tk_note(FT(135), 0.1, 36, TKV_NOISE, 0.4f);                              /* HUD slam */
    tk_note(FT(143), 0.06, 70, TKV_NOISE, 0.3f);                             /* sticker stamp */
    for (int i = 0; i < NB; i++) {
        double t = FT(B[i].f);
        if (B[i].kind == 1) { tk_sfx(t, SFX_SHOTGUN, 0.6f); tk_note(t + 0.15, 0.08, 45, TKV_NOISE, 0.2f); continue; }   /* fwap */
        tk_sfx(t, SFX_SHOTGUN, 0.7f); tk_sfx(t + 0.03, SFX_SPLAT, 0.4f);
        tk_note(t + 0.33, 0.05, 66, TKV_NOISE, 0.25f); tk_note(t + 0.45, 0.05, 62, TKV_NOISE, 0.25f);   /* CHK-CHK */
    }
    tk_note(FT(195), 0.04, 98, TKV_SINE, 0.2f);                              /* tik: the eyeball turns */
    tk_note(FT(475), 0.06, 50, TKV_NOISE, 0.3f); tk_note(FT(483), 0.06, 50, TKV_NOISE, 0.3f);   /* knock, knock */
    tk_sfx(FT(630), SFX_SPLAT, 0.6f);
    tk_note(FT(1150), 0.05, 66, TKV_NOISE, 0.3f); tk_note(FT(1160), 0.05, 62, TKV_NOISE, 0.3f);
}
