/* grumbleguts.c - concept sheets for GATEKEEPER GRUMBLEGUTS, drawn with the
 * trailer toolkit in the game's real palette.
 *   gcc -O2 -std=c11 -I.. -I../../core/include -I../build grumbleguts.c ../tk.c ../tk_chibi.c -lm
 *   ./a.out OUTDIR   -> OUTDIR/gg_turnaround.png, gg_poses.png, gg_cards.png (4x)
 * See docs/design/story/GRUMBLEGUTS_AND_DIRECTOR_NOTES.md for the brief:
 * pot-bellied cartoon devil (Baloo posture, buff gym-demon muscle, bat wings,
 * spiked tail) who is also a big-butt devil, in hell's leather-and-spikes
 * dress code. Cartoon-modest: smooth peach cheeks, nothing else. */
#include <stdio.h>
#include <string.h>
#include "tk.h"

int  tk_write_png_rgb(const char *path, const uint8_t *rgb, int w, int h);
void tk_fb_to_rgb(uint8_t *out);

/* ---- palette picks: 8-shade ramps, dithered, so it reads as paint, not CGA */
#define LINE    PAL_RAMP(RAMP_BLOOD, 0)        /* deep wine outline              */
static const uint8_t SKINM[5]  = { PAL_RAMP(RAMP_BLOOD, 2), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_BLOOD, 5), PAL_RAMP(RAMP_BLOOD, 6), PAL_RAMP(RAMP_BLOOD, 7) };
static const uint8_t BELLYM[5] = { PAL_RAMP(RAMP_PEACH, 2), PAL_RAMP(RAMP_PEACH, 3), PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7) };
#define SKIN    PAL_RAMP(RAMP_BLOOD, 5)
#define SKIN_D  PAL_RAMP(RAMP_BLOOD, 3)
#define SKIN_L  PAL_RAMP(RAMP_BLOOD, 6)
#define HORN    PAL_RAMP(RAMP_BONE, 6)
#define HORN_D  PAL_RAMP(RAMP_BONE, 3)
#define WING    PAL_RAMP(RAMP_VAPE, 2)
#define WING_D  PAL_RAMP(RAMP_VAPE, 1)
#define WING_L  PAL_RAMP(RAMP_VAPE, 3)
#define LEATHER PAL_RAMP(RAMP_WOOD, 1)
#define LEATHER_D PAL_RAMP(RAMP_WOOD, 0)
#define STUD    PAL_RAMP(RAMP_CANDY, 4)       /* brass */
#define CHAIN   PAL_RAMP(RAMP_STEEL, 6)
#define EYE_W   PAL_RAMP(RAMP_LAVA, 6)
#define EYE_O   PAL_RAMP(RAMP_LAVA, 4)
#define PUPIL   PAL_RAMP(RAMP_STONE, 0)
#define FANG    PAL_RAMP(RAMP_BONE, 7)
#define MOUTH   PAL_RAMP(RAMP_FLESH, 1)
#define TONGUE  PAL_RAMP(RAMP_FLESH, 4)

enum { V_FRONT, V_SIDE, V_BACK, V_BACK_LOOK };
enum { E_GRIN, E_WINK, E_SHOCK, E_SMUG, E_SUCK, E_YELP, E_LOVE, E_BLUSH };
enum { P_STAND, P_SNACK, P_SUCK_IN, P_SPRING, P_WIGGLE, P_UNBUCKLE,
       P_CROP, P_INFLATED, P_MIRROR };

/* shaded blob: outline, then 5 tones lit from the upper-left, with a
 * checkerboard dither on every seam and a thin rim light bottom-right. */
static void blob(int cx, int cy, int rx, int ry, const uint8_t *m) {
    tk_ellipse(cx, cy, rx + 1, ry + 1, LINE);
    tk_ellipse(cx, cy, rx, ry, m[0]);
    tk_ellipse(cx + rx / 10, cy + ry / 10, rx - rx / 8, ry - ry / 8, m[4]);   /* rim light */
    tk_ellipse(cx, cy, rx - rx / 7, ry - ry / 7, m[1]);
    for (int k = 2; k <= 3; k++) {
        int sh = k - 1;
        int ex = cx - rx * sh / 6, ey = cy - ry * sh / 6;
        int erx = rx - rx * (k + 1) / 7, ery = ry - ry * (k + 1) / 7;
        tk_ellipse(ex, ey, erx, ery, m[k]);
        /* dither seam just outside this tone */
        for (int y = ey - ery - 2; y <= ey + ery + 2; y++)
            for (int x = ex - erx - 2; x <= ex + erx + 2; x++) {
                if ((x + y) & 1) continue;
                long dx = x - ex, dy = y - ey;
                long inside = dx * dx * ery * ery + dy * dy * erx * erx;
                long r0 = (long)erx * ery, r1 = (long)(erx + 2) * (ery + 2);
                if (inside > r0 * r0 && inside <= r1 * r1 && tk_get(x, y) == m[k - 1]) tk_px(x, y, m[k]);
            }
    }
    tk_ellipse(cx - rx / 2, cy - ry / 2, rx / 5 + 1, ry / 6 + 1, m[4]);         /* specular */
}
static void blob_outline_only(int cx, int cy, int rx, int ry, uint8_t b) {
    tk_ellipse(cx, cy, rx + 1, ry + 1, LINE);
    tk_ellipse(cx, cy, rx, ry, b);
}

/* torso slab: a shaded trapezoid with round shoulder caps, so he has a
 * chest with heft instead of a stack of circles */
static void torso(int cx, int top, int bottom, int wtop, int wbot) {
    tk_tri(cx - wtop - 1, top, cx + wtop + 1, top, cx + wbot + 1, bottom + 1, LINE);
    tk_tri(cx - wtop - 1, top, cx + wbot + 1, bottom + 1, cx - wbot - 1, bottom + 1, LINE);
    tk_tri(cx - wtop, top, cx + wtop, top, cx + wbot, bottom, SKINM[2]);
    tk_tri(cx - wtop, top, cx + wbot, bottom, cx - wbot, bottom, SKINM[2]);
    tk_tri(cx - wtop, top, cx - wtop / 3, top, cx - wbot, bottom, SKINM[1]);      /* left shade */
    tk_tri(cx + wtop / 3, top, cx + wtop, top, cx + wbot, bottom, SKINM[3]);      /* right light */
    blob(cx - wtop + 2, top + 3, 7, 6, SKINM); blob(cx + wtop - 2, top + 3, 7, 6, SKINM); /* deltoids */
}
static void pecs(int cx, int y, int w) {
    blob(cx - w / 2, y, w / 2 + 1, w / 3, SKINM);
    blob(cx + w / 2, y, w / 2 + 1, w / 3, SKINM);
    tk_line(cx, y - w / 3, cx, y + w / 3, LINE);
    tk_line(cx - w / 2 - 2, y + w / 3 + 1, cx + w / 2 + 2, y + w / 3 + 1, SKINM[1]); /* under-pec shadow */
}

static void horns(int cx, int cy, int s, int flip) {
    /* two curved bone horns on the brow, like ref 1 */
    for (int side = -1; side <= 1; side += 2) {
        if (flip && side == 1) continue;
        int x = cx + side * 9 * s / 2;
        tk_ellipse(x, cy - 2 * s, 3 * s / 2 + 1, 4 * s + 1, LINE);
        tk_ellipse(x, cy - 2 * s, 3 * s / 2, 4 * s, HORN_D);
        tk_ellipse(x - side, cy - 3 * s, s, 3 * s, HORN);
        tk_ellipse(x + side * s, cy - 6 * s, s, s, HORN);
    }
}

static void wing(int x, int y, int s, int dir, int open) {
    /* bat wing: three membrane triangles between finger bones */
    int tip = open ? 34 * s / 2 : 10 * s / 2;
    int x1 = x + dir * tip, y1 = y - 16 * s / 2;
    int x2 = x + dir * (tip - 2 * s), y2 = y + 2 * s;
    int x3 = x + dir * (tip / 2), y3 = y + 6 * s / 2;
    tk_tri(x, y, x1, y1, x2, y2, LINE);
    tk_tri(x, y, x2, y2, x3, y3, LINE);
    tk_tri(x + dir, y, x1 - dir, y1 + 1, x2 - dir, y2 - 1, WING);
    tk_tri(x + dir, y, x2 - dir, y2 - 1, x3 - dir, y3 - 1, WING_D);
    tk_line(x, y, x1, y1, WING_L);
    tk_line(x, y, x2, y2, WING_L);
    tk_line(x, y, x3, y3, WING_L);
    tk_circle(x1, y1, 1, HORN); /* claw on the thumb */
}

static void tail(int x, int y, int s, int dir) {
    /* thick spiked tail curling out, spade tip */
    for (int i = 0; i < 9; i++) {
        int tx = x + dir * i * 3 * s / 2;
        int ty = y + (i * i) / 3 - i * 2;
        int r = s + 2 - i / 4;
        tk_circle(tx, ty, r + 1, LINE);
        tk_circle(tx, ty, r, i % 2 ? SKIN : SKIN_D);
        if (i % 3 == 1) tk_tri(tx, ty - r, tx - 2, ty - r - 3 * s / 2, tx + 2, ty - r - 3 * s / 2, HORN);
    }
    int tx = x + dir * 9 * 3 * s / 2, ty = y + 27 - 18;
    tk_tri(tx - dir * 2, ty - 4 * s / 2 - 1, tx + dir * 5 * s / 2 + dir, ty, tx - dir * 2, ty + 4 * s / 2 + 1, LINE);
    tk_tri(tx - dir * 2, ty - 4 * s / 2, tx + dir * 5 * s / 2, ty, tx - dir * 2, ty + 4 * s / 2, SKIN_D);
}

static void studs(int x0, int y0, int x1, int y1, int n) {
    for (int i = 0; i <= n; i++) {
        int x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n;
        tk_px(x, y, STUD); tk_px(x + 1, y, STUD); tk_px(x, y + 1, STUD); tk_px(x + 1, y + 1, STUD);
    }
}
static void strap(int x0, int y0, int x1, int y1, int w) {
    for (int k = -w; k <= w; k++) tk_line(x0 + k, y0, x1 + k, y1, LEATHER);
    tk_line(x0 - w - 1, y0, x1 - w - 1, y1, LINE);
    tk_line(x0 + w + 1, y0, x1 + w + 1, y1, LINE);
    studs(x0, y0, x1, y1, 4);
}

static void face(int cx, int cy, int s, int expr, int dir) {
    /* eyes: yellow sclera, heavy brow, black pupils; big fanged grin */
    int ex = 4 * s, ey = cy - 2 * s;
    for (int side = -1; side <= 1; side += 2) {
        int x = cx + side * ex;
        int winked = (expr == E_WINK && side == dir);
        if (winked) {
            tk_line(x - 2 * s, ey, x + 2 * s, ey + 1, LINE);
            tk_line(x - 2 * s, ey + 1, x + 2 * s, ey + 2, LINE);
            continue;
        }
        int rx = 3 * s, ry = expr == E_SHOCK || expr == E_YELP ? 3 * s : 2 * s;
        tk_ellipse(x, ey, rx + 1, ry + 1, LINE);
        tk_ellipse(x, ey, rx, ry, EYE_O); tk_ellipse(x - 1, ey - 1, rx - 1, ry - 1, EYE_W);
        if (expr == E_LOVE) tk_heart(x - 2, ey - 2, 2, PAL_PAINT(PAINT_CHERRY, 1));
        else {
            int px = x + dir * s, pr = expr == E_SHOCK ? 1 : s + 1;
            tk_circle(px, ey, pr, PUPIL);
            tk_px(px - 1, ey - 1, PAL_WHITE);
        }
        /* brow: angry V for grin/smug, raised for shock */
        int by = ey - ry - 2;
        if (expr == E_SHOCK || expr == E_YELP) { tk_line(x - 3 * s, by - 2, x + 3 * s, by - 2, LINE); tk_line(x - 3 * s, by - 3, x + 3 * s, by - 3, LINE); }
        else { tk_line(x - 3 * s * side, by + 2, x + 3 * s * side, by - 1, LINE); tk_line(x - 3 * s * side, by + 3, x + 3 * s * side, by, LINE); }
    }
    if (expr == E_BLUSH) {
        /* big blush patches, eyes bulging one more than the other, tiny "o" mouth */
        tk_ellipse(cx - 7 * s, cy + 2 * s, 4 * s, 2 * s, PAL_RAMP(RAMP_CANDY, 6));
        tk_ellipse(cx + 7 * s, cy + 2 * s, 4 * s, 2 * s, PAL_RAMP(RAMP_CANDY, 6));
        for (int i = 0; i < 3; i++) { tk_line(cx - 9 * s + i * 2, cy + s, cx - 8 * s + i * 2, cy + 3 * s, PAL_RAMP(RAMP_BLOOD, 7)); tk_line(cx + 5 * s + i * 2, cy + s, cx + 6 * s + i * 2, cy + 3 * s, PAL_RAMP(RAMP_BLOOD, 7)); }
        tk_ellipse(cx + 4 * s, cy - 2 * s, 4 * s, 4 * s, LINE); tk_ellipse(cx + 4 * s, cy - 2 * s, 4 * s - 1, 4 * s - 1, EYE_W); tk_circle(cx + 4 * s, cy - 2 * s, 1, PUPIL);
        tk_ellipse(cx, cy + 5 * s, 2, 2, LINE); tk_ellipse(cx, cy + 5 * s, 1, 1, MOUTH);
        return;
    }
    /* snout + mouth */
    int my = cy + 4 * s;
    if (expr == E_SUCK) { tk_ellipse(cx, my, 2, 2, LINE); tk_ellipse(cx, my, 1, 1, MOUTH); return; }
    if (expr == E_SHOCK || expr == E_YELP) {
        tk_ellipse(cx, my + s, 3 * s, 3 * s + 1, LINE); tk_ellipse(cx, my + s, 3 * s - 1, 3 * s, MOUTH);
        tk_ellipse(cx, my + 2 * s + 1, 2 * s, s, TONGUE);
        tk_tri(cx - 2 * s, my - s, cx - s, my + s, cx - 3 * s + 1, my + s, FANG);
        tk_tri(cx + 2 * s, my - s, cx + s, my + s, cx + 3 * s - 1, my + s, FANG);
        return;
    }
    /* wide grin */
    int w = expr == E_SMUG ? 6 * s : 8 * s;
    tk_ellipse(cx + dir * s, my, w + 1, 3 * s, LINE);
    tk_ellipse(cx + dir * s, my, w, 3 * s - 1, MOUTH);
    tk_ellipse(cx + dir * s, my - 2 * s, w + 1, 2 * s, SKIN);      /* upper lip cut */
    tk_line(cx + dir * s - w - 1, my - s, cx + dir * s + w + 1, my - s, LINE);
    for (int i = -2; i <= 2; i++) {                                 /* fangs */
        int fx = cx + dir * s + i * w / 2;
        tk_tri(fx - 2, my - s, fx + 2, my - s, fx, my + s + (i == 0 ? 2 : 0), FANG);
    }
    tk_ellipse(cx + dir * s, my + 2 * s, w / 2, s, TONGUE);
    if (expr == E_WINK || expr == E_LOVE) { tk_circle(cx + dir * 6 * s, cy + 2 * s, 2, PAL_PAINT(PAINT_ROSE, 0)); tk_circle(cx - dir * 6 * s, cy + 2 * s, 2, PAL_PAINT(PAINT_ROSE, 0)); }
}

/* The man himself. s = scale (2 = ~110 px tall). view V_*, pose P_*, expr E_*. */
static void grumbleguts(int cx, int yf, int s, int view, int pose, int expr) {
    int belly_r = (pose == P_INFLATED) ? 27 * s : (pose == P_SUCK_IN) ? 13 * s : 18 * s;
    int belly_ry = (pose == P_INFLATED) ? 22 * s : (pose == P_SUCK_IN) ? 9 * s : 14 * s;
    int butt_r  = (pose == P_INFLATED) ? 19 * s : 13 * s;
    int body_y  = yf - 30 * s;        /* reference: waist */
    int chest_top = body_y - 24 * s, chest_bot = body_y - 6 * s;
    int belly_cy = chest_bot - 2 * s + belly_ry;   /* belly hangs from the chest */
    int head_y  = yf - 60 * s;
    int dir = (view == V_SIDE) ? 1 : (view == V_BACK_LOOK ? -1 : 1);

    /* ---- wings (behind everything) ---- */
    int wy = body_y - 26 * s;
    if (view == V_FRONT) { wing(cx - 14 * s, wy, s, -1, 1); wing(cx + 14 * s, wy, s, 1, 1); }
    if (view == V_BACK || view == V_BACK_LOOK) { wing(cx - 10 * s, wy, s, -1, 1); wing(cx + 10 * s, wy, s, 1, 1); }
    if (view == V_SIDE) wing(cx - 8 * s, wy, s, -1, 1);

    /* ---- tail ---- */
    if (view == V_SIDE) tail(cx - 12 * s, yf - 22 * s, s, -1);
    else if (view == V_FRONT) tail(cx + 22 * s, yf - 16 * s, s, 1);
    else tail(cx + 2 * s, yf - 24 * s, s, 1);

    /* ---- legs: hips, thighs and calves that tuck up under the belly ---- */
    {
        int hip_y = belly_cy + belly_ry - 6 * s;
        if (view == V_BACK || view == V_BACK_LOOK) hip_y = body_y + 8 * s;
        if (view != V_SIDE) blob(cx, hip_y, 17 * s, 7 * s, SKINM);                       /* hips */
        for (int side = -1; side <= 1; side += 2) {
            int lx = cx + side * 9 * s;
            if (view == V_SIDE) lx = cx + side * 5 * s - 2 * s;
            blob(lx, (hip_y + yf - 8 * s) / 2, 7 * s, (yf - 8 * s - hip_y) / 2 + 4 * s, SKINM); /* thigh */
            blob(lx + side * s, yf - 7 * s, 6 * s, 7 * s, SKINM);                            /* calf */
            tk_line(lx - 5 * s, yf - 12 * s, lx + 5 * s, yf - 12 * s, SKINM[1]);             /* knee crease */
            for (int c = -1; c <= 1; c++) tk_tri(lx + c * 3 * s - 2, yf - 2 * s, lx + c * 3 * s + 2, yf - 2 * s, lx + c * 3 * s, yf + 2, HORN);
        }
    }

    /* ---- body ---- */
    if (view == V_SIDE) {
        /* profile: chest slab leaning forward, belly hanging in front, butt behind */
        blob(cx - 10 * s, body_y + 4 * s, butt_r, butt_r - 2 * s, SKINM);                   /* butt */
        tk_line(cx - 10 * s, body_y, cx - 10 * s, body_y + butt_r - 2 * s, LINE);           /* cleft */
        torso(cx + 2 * s, chest_top, chest_bot, 12 * s, 10 * s);
        blob(cx + 9 * s, chest_top + 8 * s, 8 * s, 5 * s, SKINM);                             /* pec */
        blob(cx + 6 * s, belly_cy, belly_r - 2 * s, belly_ry, BELLYM);                       /* belly */
        tk_circle(cx + 6 * s + belly_r - 5 * s, belly_cy + 2 * s, 2, PAL_RAMP(RAMP_PEACH, 3)); /* navel */
        strap(cx - 4 * s, chest_top + 2 * s, cx + 10 * s, chest_bot + 2 * s, s);
        int by = belly_cy + belly_ry - 5 * s;
        tk_rect(cx - 10 * s - butt_r + 2, by, 16 * s + butt_r + belly_r, 3 * s, LEATHER);
        tk_frame(cx - 10 * s - butt_r + 2, by, 16 * s + butt_r + belly_r, 3 * s, LINE);
        studs(cx - 6 * s, by + s, cx + 4 * s + belly_r, by + s, 6);
    } else if (view == V_FRONT) {
        torso(cx, chest_top, chest_bot, 19 * s, 15 * s);
        pecs(cx, chest_top + 8 * s, 16 * s);
        blob(cx, belly_cy, belly_r, belly_ry, BELLYM);
        tk_ellipse(cx, belly_cy + belly_ry / 3, 2, 3, PAL_RAMP(RAMP_PEACH, 3));              /* navel */
        if (pose == P_INFLATED) { tk_ring(cx, belly_cy, belly_r - 4 * s, PAL_RAMP(RAMP_PEACH, 4)); tk_ring(cx, belly_cy, belly_r - 9 * s, PAL_RAMP(RAMP_PEACH, 4)); }
        /* X harness over the chest, ring at the sternum */
        strap(cx - 15 * s, chest_top + 2 * s, cx + 11 * s, chest_bot + 4 * s, s);
        strap(cx + 15 * s, chest_top + 2 * s, cx - 11 * s, chest_bot + 4 * s, s);
        tk_circle(cx, chest_top + 10 * s, 3 * s, LINE); tk_circle(cx, chest_top + 10 * s, 2 * s, CHAIN);
        /* belt + buckle under the belly */
        int by = belly_cy + belly_ry - 5 * s;
        tk_rect(cx - belly_r + 2, by, 2 * belly_r - 4, 3 * s, LEATHER);
        tk_frame(cx - belly_r + 2, by, 2 * belly_r - 4, 3 * s, LINE);
        studs(cx - belly_r + 6, by + s, cx + belly_r - 6, by + s, 7);
        if (pose == P_UNBUCKLE) { tk_rect(cx - 3 * s, by - s, 6 * s, 5 * s, STUD); tk_frame(cx - 3 * s, by - s, 6 * s, 5 * s, LINE); tk_line(cx + 3 * s, by + s, cx + 9 * s, by + 6 * s, LEATHER); }
        else { tk_rect(cx - 2 * s, by, 4 * s, 3 * s, STUD); tk_frame(cx - 2 * s, by, 4 * s, 3 * s, LINE); }
    } else {
        /* back: broad shoulders, X straps, and THE BUTT front and centre */
        torso(cx, chest_top, chest_bot + 4 * s, 19 * s, 14 * s);
        tk_line(cx, chest_top + 4 * s, cx, chest_bot, SKINM[1]);                            /* spine */
        strap(cx - 15 * s, chest_top + 2 * s, cx + 10 * s, chest_bot + 2 * s, s);
        strap(cx + 15 * s, chest_top + 2 * s, cx - 10 * s, chest_bot + 2 * s, s);
        int by = body_y + 4 * s;
        blob(cx - butt_r / 2 - s, by + butt_r / 2, butt_r, butt_r, SKINM);
        blob(cx + butt_r / 2 + s, by + butt_r / 2, butt_r, butt_r, SKINM);
        tk_line(cx, by - 2, cx, by + butt_r, LINE);
        /* belt sits above the cheeks */
        tk_rect(cx - 17 * s, by - 2 * s, 34 * s, 3 * s, LEATHER); tk_frame(cx - 17 * s, by - 2 * s, 34 * s, 3 * s, LINE);
        studs(cx - 14 * s, by - s, cx + 14 * s, by - s, 7);
        if (pose == P_WIGGLE) {
            for (int i = 0; i < 3; i++) {
                tk_line(cx - butt_r - 4 * s - i * 3, by + 2 + i * 4, cx - butt_r - 1 * s - i * 3, by + 2 + i * 4, LINE);
                tk_line(cx + butt_r + 1 * s + i * 3, by + 2 + i * 4, cx + butt_r + 4 * s + i * 3, by + 2 + i * 4, LINE);
            }
            tk_sparkles(cx, by + butt_r, 10, 7, 14 * s);
        }
        if (pose == P_INFLATED) {
            /* a tiny pressure gauge on a cheek */
            tk_circle(cx + butt_r / 2 + s, by + butt_r / 2, 4, PAL_WHITE); tk_ring(cx + butt_r / 2 + s, by + butt_r / 2, 4, LINE);
            tk_line(cx + butt_r / 2 + s, by + butt_r / 2, cx + butt_r / 2 + s + 3, by + butt_r / 2 - 2, PAL_UI_RED);
        }
    }

    /* ---- arms: thick, biceps, clawed hands ---- */
    for (int side = -1; side <= 1; side += 2) {
        if (view == V_SIDE && side == -1) continue;
        int ax = cx + side * 22 * s, ay = chest_top + 10 * s;
        int raised = (pose == P_SPRING || pose == P_CROP) && side == 1;
        if (pose == P_WIGGLE || view == V_BACK || view == V_BACK_LOOK) { ax = cx + side * 22 * s; ay = chest_top + 12 * s; }
        blob(ax, raised ? ay - 10 * s : ay, 6 * s, 9 * s, SKINM);           /* bicep */
        int hx = ax + side * 2 * s, hy = raised ? ay - 22 * s : ay + 12 * s;
        if (pose == P_SNACK && side == 1) { hx = cx + 12 * s; hy = head_y + 6 * s; }
        if (pose == P_MIRROR && side == 1) { hx = cx + 26 * s; hy = body_y - 18 * s; }
        blob(hx, hy, 5 * s, 5 * s, SKINM);                                   /* fist */
        /* studded cuff */
        tk_rect(hx - 5 * s, hy - 7 * s, 10 * s, 2 * s, LEATHER); studs(hx - 4 * s, hy - 6 * s, hx + 4 * s, hy - 6 * s, 3);
        for (int c = -1; c <= 1; c++) tk_tri(hx + c * 3 * s - 1, hy + 4 * s, hx + c * 3 * s + 1, hy + 4 * s, hx + c * 3 * s, hy + 6 * s + 1, HORN);
        if (pose == P_SNACK && side == 1) { tk_ring(hx + 4 * s, hy - 2 * s, 4, PAL_PAINT(PAINT_CARAMEL, 1)); tk_ring(hx + 4 * s, hy - 2 * s, 3, PAL_PAINT(PAINT_CARAMEL, 0)); tk_px(hx + 5 * s, hy - 4 * s, PAL_PAINT(PAINT_HELLFIRE, 1)); }
        if (pose == P_CROP && side == 1) { tk_line(hx, hy - 4 * s, hx + 2 * s, hy - 30 * s, LINE); tk_line(hx + 1, hy - 4 * s, hx + 2 * s + 1, hy - 30 * s, LEATHER); tk_rect(hx + 2 * s - 2, hy - 34 * s, 5, 5 * s, LEATHER_D); }
    }

    /* ---- collar with spikes ---- */
    int ny = head_y + 15 * s;
    if (view == V_SIDE) ny = head_y + 15 * s;
    tk_rect(cx - 10 * s, ny, 20 * s, 3 * s, LEATHER); tk_frame(cx - 10 * s, ny, 20 * s, 3 * s, LINE);
    for (int i = -3; i <= 3; i++) tk_tri(cx + i * 3 * s - 1, ny, cx + i * 3 * s + 1, ny, cx + i * 3 * s, ny - 3 * s, CHAIN);
    /* the gate key on a chain */
    if (view == V_FRONT) { tk_line(cx, ny + 3 * s, cx, ny + 8 * s, CHAIN); tk_ring(cx, ny + 10 * s, 2, STUD); tk_line(cx, ny + 12 * s, cx, ny + 16 * s, STUD); tk_px(cx + 1, ny + 15 * s, STUD); tk_px(cx + 1, ny + 14 * s, STUD); }

    /* ---- head ---- */
    if (view == V_BACK) {
        blob(cx, head_y, 13 * s, 13 * s, SKINM);
        horns(cx, head_y - 8 * s, s, 0);
        /* pointy ears */
        tk_tri(cx - 12 * s, head_y, cx - 18 * s, head_y - 6 * s, cx - 10 * s, head_y - 6 * s, SKIN);
        tk_tri(cx + 12 * s, head_y, cx + 18 * s, head_y - 6 * s, cx + 10 * s, head_y - 6 * s, SKIN);
        return;
    }
    int hx = cx + (view == V_BACK_LOOK ? -5 * s : 0);
    blob(hx, head_y, 14 * s, 13 * s, SKINM);
    tk_tri(hx - 13 * s, head_y, hx - 19 * s, head_y - 7 * s, hx - 10 * s, head_y - 7 * s, LINE);
    tk_tri(hx - 13 * s, head_y + 1, hx - 18 * s, head_y - 6 * s, hx - 11 * s, head_y - 6 * s, SKIN);
    tk_tri(hx + 13 * s, head_y, hx + 19 * s, head_y - 7 * s, hx + 10 * s, head_y - 7 * s, LINE);
    tk_tri(hx + 13 * s, head_y + 1, hx + 18 * s, head_y - 6 * s, hx + 11 * s, head_y - 6 * s, SKIN);
    horns(hx, head_y - 9 * s, s, 0);
    /* goatee */
    tk_tri(hx - 3 * s, head_y + 11 * s, hx + 3 * s, head_y + 11 * s, hx, head_y + 17 * s, WING_D);
    face(hx, head_y, s, expr, view == V_BACK_LOOK ? -1 : dir);
}

#define PW 120  /* panel width: left 120 px of the framebuffer */
#define MAXP 6
static uint8_t panels[MAXP][TH][PW][3];
static int npanels;
static void panel(void) {
    static uint8_t rgb[TW * TH * 3];
    tk_fb_to_rgb(rgb);
    for (int y = 0; y < TH; y++) memcpy(panels[npanels][y], &rgb[y * TW * 3], PW * 3);
    npanels++;
}
static void save(const char *dir, const char *name) {
    int W = PW * npanels, S = 3;
    static uint8_t big[PW * MAXP * 3 * TH * 3 * 3];
    char path[512];
    for (int y = 0; y < TH * S; y++)
        for (int x = 0; x < W * S; x++) {
            int px = x / S, py = y / S;
            memcpy(&big[(y * W * S + x) * 3], panels[px / PW][py][px % PW], 3);
        }
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    tk_write_png_rgb(path, big, W * S, TH * S);
    printf("wrote %s\n", path);
    npanels = 0;
}
static void shot(const char *label, int view, int pose, int expr, const char *note) {
    static const uint8_t cols[4] = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4) };
    tk_vgrad(0, 0, TW, TH, cols, 4);
    tk_rect(0, 126, TW, 34, PAL_RAMP(RAMP_STONE, 2));
    tk_line(0, 126, TW, 126, PAL_RAMP(RAMP_LAVA, 3));
    grumbleguts(60, 122, 1, view, pose, expr);
    tk_rect(0, 0, PW, 11, PAL_RAMP(RAMP_STONE, 1));
    tk_text_outline(3, 2, label, PAL_UI_GOLD, LINE, 1);
    tk_text_wrap(3, 130, PW - 6, note, PAL_UI_CREAM, 19);
    panel();
}

static void backdrop(const char *title) {
    static const uint8_t cols[4] = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4) };
    tk_vgrad(0, 0, TW, TH, cols, 4);
    tk_rect(0, 128, TW, 32, PAL_RAMP(RAMP_STONE, 2));
    tk_line(0, 128, TW, 128, PAL_RAMP(RAMP_LAVA, 3));
    tk_text_outline(4, 3, title, PAL_UI_GOLD, LINE, 1);
}

/* ---- full-frame storyboard panels ------------------------------------ */
static uint8_t frames[6][TH][TW][3];
static int nframes;
static void frame(const char *title) {
    static uint8_t rgb[TW * TH * 3];
    tk_rect(0, 0, TW, 10, PAL_RAMP(RAMP_STONE, 1));
    tk_text_outline(3, 1, title, PAL_UI_GOLD, LINE, 1);
    tk_fb_to_rgb(rgb);
    memcpy(frames[nframes++], rgb, sizeof rgb);
}
static void save_frames(const char *dir, const char *name) {
    int cols = nframes < 3 ? nframes : 3, rows = (nframes + 2) / 3, S = nframes == 1 ? 4 : 2, W = TW * cols, H = TH * rows;
    static uint8_t big[TW * 4 * TH * 4 * 3 * 2];
    char path[512];
    for (int y = 0; y < H * S; y++)
        for (int x = 0; x < W * S; x++) {
            int fx = x / S, fy = y / S, f = (fy / TH) * cols + fx / TW;
            const uint8_t *src = f < nframes ? frames[f][fy % TH][fx % TW] : (const uint8_t[]){ 0, 0, 0 };
            memcpy(&big[(y * W * S + x) * 3], src, 3);
        }
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    tk_write_png_rgb(path, big, W * S, H * S);
    printf("wrote %s\n", path);
    nframes = 0;
}
static void hall(void) {
    static const uint8_t cols[4] = { PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_LAVA, 1), PAL_RAMP(RAMP_STONE, 3) };
    tk_vgrad(0, 0, TW, TH, cols, 4);
    tk_rect(0, 118, TW, 42, PAL_RAMP(RAMP_STONE, 2));              /* park floor */
    tk_line(0, 118, TW, 118, PAL_RAMP(RAMP_LAVA, 3));
    for (int i = 0; i < 9; i++) { tk_circle(20 + i * 26, 30 + (i % 3) * 6, 3, PAL_RAMP(RAMP_CANDY, 4)); } /* festival lanterns */
}
static void crowd(int y, int n, int flip, int expr) {
    for (int i = 0; i < n; i++) {
        TkChibi c = tk_random_citizen(100 + i, i & 1);
        tk_chibi(14 + i * 30, y, &c, 1, expr, expr == TKE_SHOCK ? TKM_O : TKM_SMILE, i * 7, flip);
    }
}
static void bubble(int x, int y, int w, const char *txt) {
    tk_rect(x, y, w, 12, PAL_UI_CREAM); tk_frame(x, y, w, 12, LINE);
    tk_tri(x + 8, y + 12, x + 14, y + 12, x + 10, y + 16, PAL_UI_CREAM);
    tk_text(x + 3, y + 2, txt, PAL_BLACK, 1);
}
static void gas(int x, int y, int r, int cute) {
    uint8_t g = cute ? PAL_RAMP(RAMP_SICK, 5) : PAL_RAMP(RAMP_SICK, 3);
    tk_circle(x, y, r, g); tk_circle(x - r, y + 2, r * 2 / 3, g); tk_circle(x + r, y + 2, r * 2 / 3, g);
    if (cute) { tk_heart(x - 2, y - 2, 2, PAL_RAMP(RAMP_CANDY, 6)); tk_star(x + r / 2, y - r / 2, 2, PAL_WHITE); }
    else { tk_px(x + 1, y - 1, PAL_BLACK); tk_px(x - r / 2, y + 1, PAL_BLACK); tk_px(x + r / 2 + 1, y + 2, PAL_BLACK); }
}
static void storyboard(const char *out) {
    TkChibi culprit = tk_random_citizen(66, 1);
    /* 1. the feast: butt to camera, everyone watching */
    hall(); crowd(156, 8, 0, TKE_NORMAL);
    tk_rect(60, 96, 120, 8, PAL_RAMP(RAMP_WOOD, 3)); tk_frame(60, 96, 120, 8, LINE);     /* table */
    for (int i = 0; i < 6; i++) tk_ring(70 + i * 20, 94, 4, PAL_RAMP(RAMP_CANDY, 3));   /* bagels */
    grumbleguts(120, 118, 1, V_BACK, P_STAND, E_GRIN);
    tk_rect(150, 14, 84, 7, PAL_RAMP(RAMP_STONE, 1)); tk_rect(151, 15, 70, 5, PAL_UI_RED); tk_text(152, 23, "FULLNESS", PAL_UI_CREAM, 1);
    bubble(4, 40, 108, "Let him eat?");  tk_text(10, 56, "> yes, let's see", PAL_UI_GOLD, 1); tk_text(10, 64, "  exorcise now!", PAL_UI_CREAM, 1);
    frame("THE FEAST OF REBUILDING");
    /* 2. the line */
    hall(); crowd(156, 8, 0, TKE_NORMAL);
    grumbleguts(96, 118, 1, V_BACK, P_STAND, E_GRIN);
    tk_chibi(160, 118, &culprit, 2, TKE_POSSESSED, TKM_CAT, 3, 1);
    bubble(118, 28, 118, "Look who's full of");  bubble(118, 44, 118, "himself, now?");
    tk_text_outline(170, 70, "*SCRTCH*", PAL_UI_GOLD, LINE, 1);
    frame("THE LINE (eyebrow wiggle at YOU)");
    /* 3. reverse rope climb, hands on */
    hall(); crowd(156, 8, 0, TKE_SHOCK);
    grumbleguts(150, 118, 1, V_SIDE, P_STAND, E_SHOCK);
    tk_chibi(84, 118, &culprit, 2, TKE_ANGRY, TKM_SCREAM, 5, 0);
    for (int i = 0; i < 14; i++) tk_line(40 + i * 3, 110 - (i % 2) * 3, 43 + i * 3, 110 - ((i + 1) % 2) * 3, PAL_RAMP(RAMP_SICK, 4)); /* hose coil */
    tk_line(84, 96, 118, 92, PAL_RAMP(RAMP_SICK, 4)); tk_line(84, 97, 118, 93, PAL_RAMP(RAMP_SICK, 2));
    blob(118, 94, 4, 3, SKINM); blob(116, 102, 4, 3, SKINM);                              /* hands on the cheeks */
    tk_text_outline(60, 70, "hnnng", PAL_UI_CREAM, LINE, 1); tk_text_outline(30, 112, "surely that's enough?", PAL_UI_CREAM, LINE, 1);
    frame("REVERSE ROPE CLIMB");
    /* 4. the slap */
    hall(); crowd(156, 8, 0, TKE_SHOCK);
    grumbleguts(120, 118, 1, V_BACK_LOOK, P_STAND, E_YELP);
    for (int f = 0; f < 4; f++) tk_ellipse(132 + f * 4, 96 - (f == 1 || f == 2 ? 3 : 0), 2, 4, PAL_RAMP(RAMP_PEACH, 6)); /* hand print */
    tk_ellipse(137, 104, 6, 5, PAL_RAMP(RAMP_PEACH, 6));
    tk_text_outline(150, 60, "SLAP!", PAL_UI_GOLD, LINE, 2);
    frame("THE SLAP (then the valve)");
    /* 5. inflation + gas both ways */
    hall(); crowd(156, 8, 1, TKE_SHOCK);
    grumbleguts(120, 118, 1, V_FRONT, P_INFLATED, E_YELP);
    gas(150, 46, 8, 1); tk_text(158, 36, "BURP", PAL_UI_CREAM, 1);
    gas(84, 112, 7, 1);  tk_text(50, 104, "BRAAP", PAL_UI_CREAM, 1);
    tk_rect(190, 90, 10, 10, PAL_RAMP(RAMP_STEEL, 5)); tk_ring(195, 95, 6, PAL_UI_RED);            /* valve */
    tk_line(166, 100, 190, 96, PAL_RAMP(RAMP_SICK, 4));
    frame("INFLATION: two false pops, gas both ways");
    /* 6. the pop (cute) + the last line */
    hall(); crowd(156, 8, 0, TKE_SHOCK);
    tk_confetti(120, 70, 12, 9); tk_hearts(120, 60, 10, 4); tk_sparkles(120, 70, 8, 2, 40);
    for (int i = 0; i < 12; i++) tk_circle(70 + (i * 37) % 100, 50 + (i * 53) % 60, 2, PAL_RAMP(RAMP_BONE, 7)); /* stuffing */
    tk_ring(120, 34, 10, PAL_UI_GOLD);                                                      /* halo */
    blob(114, 84, 8, 8, SKINM); blob(128, 84, 8, 8, SKINM); tk_line(121, 78, 121, 92, LINE); /* lone cheeks */
    tk_line(121, 92, 121, 104, PAL_RAMP(RAMP_STEEL, 6)); tk_text(104, 100, "pfffff", PAL_UI_CREAM, 1);
    { TkChibi c = tk_random_citizen(103, 1); c.blush = 1; tk_chibi(70, 156, &c, 1, TKE_LOVE, TKM_O, 1, 0); }
    bubble(60, 110, 82, "...that's hot."); tk_text_outline(150, 112, "(instantly mobbed)", PAL_UI_CREAM, LINE, 1);
    frame("POP (cute setting) + THE LAST LINE");
    save_frames(out, "gg_storyboard");

    /* 7. CLOSE-UP: hands on the butt during the climb */
    static const uint8_t bg[3] = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_LAVA, 1), PAL_RAMP(RAMP_STONE, 3) };
    tk_vgrad(0, 0, TW, TH, bg, 3);
    /* belt edge across the top, then two enormous cheeks filling the frame */
    tk_rect(0, 14, TW, 12, LEATHER); tk_frame(0, 14, TW, 12, LINE); studs(12, 19, TW - 12, 19, 9);
    blob(84, 104, 70, 64, SKINM);
    blob(176, 104, 70, 64, SKINM);
    tk_line(130, 44, 130, 160, LINE); tk_line(131, 44, 131, 160, SKINM[1]);             /* cleft */
    /* the hose runs across the near cheek and goes in at the cleft (the crease
     * is redrawn over its tip, so it simply vanishes into the line) */
    for (int i = 0; i < 6; i++) tk_line(0, 136 + i, 128, 112 + i, PAL_RAMP(RAMP_SICK, 4 - (i == 0 || i == 5 ? 2 : 0)));
    tk_ellipse(127, 115, 3, 5, PAL_RAMP(RAMP_SICK, 2));
    tk_line(130, 44, 130, 160, LINE); tk_line(131, 44, 131, 160, SKINM[1]);
    for (int i = -4; i <= 4; i++) tk_px(130 + (i < 0 ? -1 : 1), 115 + i, LINE);        /* crease pinches round the hose */
    /* the culprit's two hands braced on the cheeks, cheeks squashing under them */
    for (int h = 0; h < 2; h++) {
        int hx = h ? 150 : 94, hy = h ? 96 : 92;
        blob(hx, hy, 15, 11, (const uint8_t[]){ PAL_RAMP(RAMP_PEACH, 2), PAL_RAMP(RAMP_PEACH, 3), PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7) });
        for (int f = 0; f < 4; f++) { tk_ellipse(hx - 12 + f * 8, hy - 11 - (f == 1 || f == 2 ? 3 : 0), 3, 7, PAL_RAMP(RAMP_PEACH, 5)); tk_ring(hx - 12 + f * 8, hy - 11 - (f == 1 || f == 2 ? 3 : 0), 3, LINE); }
        tk_ellipse(hx + (h ? -16 : 16), hy + 2, 4, 6, PAL_RAMP(RAMP_PEACH, 5)); tk_ring(hx + (h ? -16 : 16), hy + 2, 4, LINE); /* thumb */
        /* squash creases radiating from under the palm */
        for (int c = 0; c < 4; c++) tk_line(hx - 18 + c * 12, hy + 14, hx - 22 + c * 14, hy + 26, SKINM[1]);
    }
    /* effort: motion lines, one bead of sweat, a squeak */
    for (int i = 0; i < 3; i++) { tk_line(20, 60 + i * 6, 40, 56 + i * 6, PAL_UI_CREAM); tk_line(200, 60 + i * 6, 220, 56 + i * 6, PAL_UI_CREAM); }
    tk_ellipse(120, 36, 2, 3, PAL_RAMP(RAMP_SKY, 6)); tk_px(120, 33, PAL_RAMP(RAMP_SKY, 6));
    tk_text_outline(150, 40, "*squeak*", PAL_UI_CREAM, LINE, 1);
    tk_text_outline(8, 140, "\"she'll hold.\"", PAL_UI_CREAM, LINE, 1);
    frame("CLOSE-UP: hands on, hose going in at the cleft");

    /* 8. THE REACTION CUT: his face as it goes in, blushing hard */
    hall();
    grumbleguts(120, 196, 2, V_FRONT, P_STAND, E_BLUSH);         /* 2x: head fills the frame */
    tk_rect(0, 118, TW, 42, PAL_RAMP(RAMP_STONE, 2));
    tk_text_outline(8, 124, "*gurgle?*", PAL_UI_CREAM, LINE, 1);
    tk_text_outline(150, 124, "...oh.", PAL_UI_CREAM, LINE, 1);
    frame("REACTION CUT: eyes bulge one at a time, blush");

    /* 9. THE CENSOR STICKER: the board's sticker slaps on, a corner peels as the cheek wobbles */
    tk_vgrad(0, 0, TW, TH, bg, 3);
    tk_rect(0, 14, TW, 12, LEATHER); tk_frame(0, 14, TW, 12, LINE); studs(12, 19, TW - 12, 19, 9);
    blob(84, 104, 70, 64, SKINM); blob(176, 104, 70, 64, SKINM);
    tk_line(130, 44, 130, 160, LINE); tk_line(131, 44, 131, 160, SKINM[1]);
    for (int i = 0; i < 6; i++) tk_line(0, 136 + i, 100, 116 + i, PAL_RAMP(RAMP_SICK, 4 - (i == 0 || i == 5 ? 2 : 0)));
    tk_rect(96, 88, 72, 44, PAL_UI_CREAM); tk_frame(96, 88, 72, 44, LINE);
    tk_rect(96, 88, 72, 10, PAL_UI_RED); tk_text(99, 90, "HELLVILE PARK", PAL_WHITE, 1);
    tk_text(100, 102, "BOARD OF", PAL_BLACK, 1); tk_text(100, 110, "DECENCY", PAL_BLACK, 1); tk_text(100, 120, "(approved)", PAL_GRAY_D, 1);
    tk_tri(160, 124, 168, 132, 160, 132, PAL_RAMP(RAMP_PEACH, 6));                     /* peeling corner */
    tk_tri(160, 124, 168, 124, 168, 132, PAL_UI_CREAM); tk_line(160, 124, 168, 132, LINE);
    for (int i = 0; i < 3; i++) { tk_line(176, 70 + i * 5, 190, 66 + i * 5, PAL_UI_CREAM); }   /* wobble */
    tk_text_outline(8, 140, "*wobble*", PAL_UI_CREAM, LINE, 1);
    frame("CENSOR STICKER: slaps on, corner peels");

    /* 10. CUTAWAY: cartoon X-ray of the belly filling like a thermometer */
    hall();
    grumbleguts(120, 118, 1, V_SIDE, P_STAND, E_BLUSH);
    tk_ring(126, 80, 22, PAL_UI_CREAM); tk_ring(126, 80, 21, PAL_UI_CREAM);                /* x-ray circle */
    tk_circle(126, 80, 20, PAL_RAMP(RAMP_STEEL, 2));
    tk_ellipse(126, 82, 15, 14, PAL_RAMP(RAMP_STEEL, 4));                                   /* stomach outline */
    tk_ellipse(126, 88, 13, 8, PAL_RAMP(RAMP_SICK, 5));                                     /* air level rising */
    for (int i = 0; i < 5; i++) tk_ring(118 + (i * 7) % 16, 84 - (i * 5) % 12, 1, PAL_WHITE); /* bubbles */
    tk_line(126, 96, 126, 118, PAL_RAMP(RAMP_SICK, 4)); tk_line(127, 96, 127, 118, PAL_RAMP(RAMP_SICK, 2)); /* hose from below */
    tk_rect(150, 62, 6, 36, PAL_RAMP(RAMP_STONE, 1)); tk_rect(151, 80, 4, 17, PAL_UI_RED); tk_text(158, 62, "FULL", PAL_UI_CREAM, 1); /* thermometer */
    tk_text_outline(8, 124, "X-RAY: 60% air, 40% bagel", PAL_UI_CREAM, LINE, 1);
    frame("CUTAWAY: thermometer belly");
    save_frames(out, "gg_closeup");
}

int main(int argc, char **argv) {
    storyboard(argc > 1 ? argv[1] : ".");
    const char *out = argc > 1 ? argv[1] : ".";
    shot("FRONT", V_FRONT, P_STAND, E_GRIN, "Gut + pecs, X harness");
    shot("SIDE", V_SIDE, P_STAND, E_SMUG, "Belly front, butt back");
    shot("BACK", V_BACK, P_STAND, E_GRIN, "Butt front & centre");
    shot("TAUNT", V_BACK_LOOK, P_WIGGLE, E_WINK, "Wiggle, wink back");
    save(out, "gg_turnaround");

    shot("MONOLOGUE", V_FRONT, P_SNACK, E_SMUG, "Speech + bagel");
    shot("SUCKS IN", V_FRONT, P_SUCK_IN, E_SUCK, "Looking menacing?");
    shot("SPROING", V_FRONT, P_SPRING, E_SHOCK, "Springs back. Scratch");
    shot("UNBUCKLE", V_FRONT, P_UNBUCKLE, E_LOVE, "Belt sigh, flirt eyes");
    shot("THE CROP", V_FRONT, P_CROP, E_WINK, "Hell's biggest flirt");
    save(out, "gg_poses");

    shot("FULL (FRONT)", V_FRONT, P_INFLATED, E_YELP, "Feast: meter maxed");
    shot("FULL (BACK)", V_BACK, P_INFLATED, E_YELP, "Cheek gauge in red");
    /* scale vs a citizen */
    {
        static const uint8_t cols[4] = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4) };
        tk_vgrad(0, 0, TW, TH, cols, 4); tk_rect(0, 126, TW, 34, PAL_RAMP(RAMP_STONE, 2));
        grumbleguts(48, 122, 1, V_FRONT, P_STAND, E_GRIN);
        tk_chibi(100, 122, &TK_HERO, 1, TKE_SHOCK, TKM_O, 3, 1);
        tk_rect(0, 0, PW, 11, PAL_RAMP(RAMP_STONE, 1)); tk_text_outline(3, 2, "SCALE", PAL_UI_GOLD, LINE, 1);
        tk_text_wrap(3, 130, PW - 6, "vs a citizen chibi", PAL_UI_CREAM, 19);
        panel();
    }
    /* bestiary card: head only */
    {
        tk_rect(0, 0, TW, TH, PAL_RAMP(RAMP_STONE, 4));
        tk_window(4, 4, PW - 8, TH - 8, 1);
        grumbleguts(60, 150, 1, V_FRONT, P_STAND, E_WINK);          /* head in the card, body under the label */
        tk_rect(4, 96, PW - 8, 60, PAL_UI_CREAM);
        tk_text(10, 100, "GRUMBLEGUTS", PAL_UI_RED, 1);
        tk_text_wrap(10, 110, PW - 20, "Keeper of the Sealed Gate. Also of a very large seat.", PAL_BLACK, 18);
        panel();
    }
    save(out, "gg_feast");
    return 0;
}
