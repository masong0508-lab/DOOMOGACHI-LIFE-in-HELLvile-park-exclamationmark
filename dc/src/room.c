/* room.c - the dream house on the Dreamcast: plat_room() on the PVR (DC8;
 * owner: dreamcast). Portable C: no KallistiOS (room_int.h).
 *
 * WHAT IT DRAWS. What core/src/room_ref.c (the reference, the native
 * runner's renderer) draws, pixel for pixel, at the 2D layer's 2x: the
 * same projection (view k from the yaw, 1 tile = 16 units, the back
 * corner centred in the viewport, pan), the same shell (the background or
 * the open kinds' sky and stars, the roof, the backdrop, the two back walls
 * with every wallpaper 0..7, the cornice ramp, the corner post, every floor
 * pattern 0..7 with its grout, the plot's lip), the same light (H8: the
 * daylight level, up to 4 lamp pools on the floor tiles, the wall columns
 * and the pieces, glow pixels at full light, the windows' panes and the
 * roof's glass by daylight), the same sprites (furngen's iso art through
 * room_ref_sprite(): the four facings with the backs (H9), the paints, the
 * door and window skins), the same tints, and the same order (wall pieces,
 * then rugs, pieces and actors in room_ref's topological order of their
 * footprints, then the open kinds' front-edge pieces, then the cursor).
 * The host test (dc/tests/gfx2d/test_dc_room.c) renders every frame of
 * core/tests/room_shells.h (the golden's vocabulary) both ways and wants
 * them identical.
 *
 * Where the Dreamcast does more than the reference (DC8 in the plan):
 *  - RO_GHOST / RO_SEMI pieces and BB_TRANSLUCENT residents are really
 *    translucent (a GLASS prim at 50%; room_ref dithers a checker), the
 *    blueprint ghost (RO_GHOST | RO_SEMI) its grey silhouette at 50%, an
 *    invalid ghost (RO_INVALID) the same silhouette in red at 75% (room_ref
 *    darkens it a shade; the red reads as "not here" at a glance);
 *  - RO_SELECTED pulses: an additive glow over the piece's shape that
 *    swells and fades every 32 ticks (room_ref blinks a shade lighter);
 *  - DoomoRoomView.zoom: 1 is a 1.6x close-up on the player (the PS1's),
 *    2 the Talk framing (focus_x / focus_y / focus_r fill the viewport);
 *    both glide there (a third of the way a frame) and back. The PVR scales
 *    every prim; plat_room_project() follows the same transform, so Talk's
 *    bubbles stay on the heads. room_ref ignores zoom.
 *
 * HOW (30 fps with room to spare; DC3's 2D route sent room_ref's floor
 * row by row: 10,287 prims in the busiest frame, near the list's 12,288).
 *  1. THE SHELL is static while the room, the view, the paints and the
 *     light rest, so it is drawn ONCE into a PAL8 canvas at pan 0 (the
 *     roof, the backdrop, the walls, the floor, the lip: room_ref's own
 *     pixel code, ported to write bytes; a first pass measures its box so
 *     the canvas is only as big as the shell, at most 448 x 400 with 64 px
 *     of margin round the screen for pans and zooms) and becomes ONE
 *     texture (a fresh uid each time it changes). Every frame draws it as
 *     one prim (a sprite, or a 4-vertex polygon when its UVs do not fit the
 *     sprite's 16 bits).
 *     The key that decides "changed" is everything the shell reads: the
 *     turned size, the origin, the wall height, the patterns, paints,
 *     ceiling ramp, kind, roof, light level and tint, and the derived
 *     RoomLight / RoomBackdrop / RoomRoof (lamps, the door's backdrop, the
 *     chimney over the hearth). Pans and zooms only move or scale it.
 *  2. THE PIECES are textures keyed by everything their pixels depend on
 *     (piece, frame and facing entry, paints, light level and the night /
 *     dusk / dark-lamp bits, the grey / invalid recolour, the tint) in a
 *     table of 1,024 keys with uids of their own (room_ref's xor of those
 *     fields can collide; a uid-keyed cache must not). A key seen before
 *     whose texture is still in video RAM costs no decode at all; otherwise
 *     room_ref_sprite() decodes it once (the scene's plat_cache cap decides,
 *     exactly as in room_ref, when a piece is too big and becomes a shaded
 *     box). A piece is one sprite.
 *  3. THE ORDER is room_ref's: the same items (footprints turned with the
 *     view, layers, heights), the same behind() relation and the same pick
 *     (the free item with the smallest key; on a cycle the smallest plain
 *     depth key), found with Kahn's algorithm in O(n^2) instead of
 *     room_ref's O(n^3) search (dc_room_sort; the host test checks the two
 *     agree on random layouts). The PVR's presorted translucent list draws
 *     the prims in exactly that order. (A depth buffer cannot replace the
 *     sort here: an iso sprite has one depth per pixel at best, and the
 *     overlap rules of the art are its footprints'.)
 *  4. Lines (the rubber band, the cursor) and the background are the 2D
 *     layer's own exact calls; residents are their images' textures
 *     (texcache.c), tinted through a remapped copy (dc2d_tex_remap with
 *     doomo_tintmap, index 0 kept clear) and squashed as one quad, as
 *     plat_image_xform does.
 * A view at rest costs the SH4 the key, the sort and 50-190 prims (0.2-0.7
 * ms on Flycast); the shell is re-rendered on a change (a lamp switched, a
 * paint, a turn, the hour's light: 12-17 ms on Flycast, the first frame in
 * a room). Counters: dc_room_stats (room_int.h), in the mailbox.
 */
#include <string.h>
#include "dc2d.h"
#include "dc3d.h"
#include "room_int.h"
#include "doomo/palette.h"
#include "doomo/furn.h"
#include "doomo/home.h"
#include "doomo/plat_ext.h"
#include "../../core/src/home_int.h"

DcRoomStats dc_room_stats;

/* ---- room_ref's projection and light ------------------------------------------- */
typedef struct RR {
    int ox, oy, W, D, k, wall_h, level, tint;
    const DoomoRoomView *v;
    const RoomLight *L;
} RR;

static uint8_t lit(const RR *r, uint8_t c)
{
    c = r->level >= 15 ? c : palette_shade(c, r->level);
    return r->tint ? doomo_tintmap[r->tint][c] : c;
}

static uint8_t glow(const RR *r, uint8_t c)
{
    return r->tint ? doomo_tintmap[r->tint][c] : c;
}

static uint32_t rr_hash(int a, int b)
{
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return h ^ (h >> 12);
}

static void rot_box(int k, int w, int d, int *x0, int *y0, int *x1, int *y1)
{
    int i;
    for (i = 0; i < k; i++) {
        int nx0 = d - *y1, nx1 = d - *y0, ny0 = *x0, ny1 = *x1, t;
        *x0 = nx0; *x1 = nx1; *y0 = ny0; *y1 = ny1;
        t = w; w = d; d = t;
    }
}

static int scr_x(const RR *r, int X, int Y) { return r->ox + X - Y; }
static int scr_y(const RR *r, int X, int Y, int Z) { return r->oy + ((X + Y) >> 1) - Z; }

/* room_ref's rr_origin; pan 0 leaves pan_x / pan_y out */
static void rr_origin(RR *r, const DoomoRoomView *v, int tall, int pan)
{
    int total, vw, vh, px = pan ? v->pan_x : 0, py = pan ? v->pan_y : 0;
    r->k = ((v->yaw + 512) >> 10) & 3;
    r->W = (r->k & 1) ? v->d : v->w;
    r->D = (r->k & 1) ? v->w : v->d;
    vw = v->vw ? v->vw : plat_screen_w();
    vh = v->vh ? v->vh : plat_screen_h();
    r->wall_h = vh >= 200 ? 64 : 48;
    total = (tall ? r->wall_h : 8) + (r->W + r->D) * 8;
    r->ox = v->vx + vw / 2 - (r->W - r->D) * 8 + px;
    if (total + 4 <= vh) r->oy = v->vy + (vh - total) / 2 + (total - (r->W + r->D) * 8) + py;
    else r->oy = v->vy + vh - (r->W + r->D) * 8 - 4 + py;
}

/* ---- the view transform (zoom) ------------------------------------------------- */
static struct { int on; float s, fx, fy, cx, cy; } Z;          /* this frame's        */
static struct { int have; float s, fx, fy; uint16_t t; } ZS;   /* the glide's state   */

static float zx(float x) { return Z.on ? 2.0f * (Z.cx + Z.s * (x - Z.fx)) : 2.0f * x; }
static float zy(float y) { return Z.on ? 2.0f * (Z.cy + Z.s * (y - Z.fy)) : 2.0f * y; }

static void rp_rect(int x, int y, int w, int h, uint8_t c)
{
    if (w <= 0 || h <= 0) return;
    if (!Z.on) { DC2D_FN(rect)(x, y, w, h, c); return; }
    dc2d_frect(zx((float)x), zy((float)y), zx((float)(x + w)), zy((float)(y + h)), c);
}

/* a line: the 2D layer's exact walk at zoom 1, else a Bresenham walk of
 * scaled pixels (runs along x merged) */
static void rp_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx, dy, sx, sy, err, rx = x0, ry = y0, rn = 0;
    if (!Z.on) { DC2D_FN(line)(x0, y0, x1, y1, c); return; }
    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    for (;;) {
        int e2;
        if (rn && ry == y0 && (sx > 0 ? x0 == rx + rn : x0 == rx - 1)) {
            if (sx < 0) rx = x0;
            rn++;
        } else {
            if (rn) rp_rect(rx, ry, rn, 1, c);
            rx = x0; ry = y0; rn = 1;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    if (rn) rp_rect(rx, ry, rn, 1, c);
}

/* a texture over logical rect (x, y, w, h) */
static void rp_tex(int mode, int slot, int x, int y, int w, int h, int iw, int ih, int flip, uint32_t argb)
{
    dc2d_tex_rect(mode, slot, zx((float)x), zy((float)y), zx((float)(x + w)), zy((float)(y + h)),
                  iw, ih, flip, argb, 0);
}

/* plat_image at scale 1, or plat_image_xform's one quad (gfx2d.c's sizes) */
static void rp_image(int mode, int slot, int iw, int ih, int x, int y, int flip, uint32_t argb)
{
    rp_tex(mode, slot, x, y, iw, ih, iw, ih, flip, argb);
}

static void rp_xform(int mode, int slot, int iw, int ih, int ax, int ay, int sx_q8, int sy_q8, int flip,
                     uint32_t argb)
{
    int asx, asy, s, dw, dh;
    if (sx_q8 == 0 || sy_q8 <= 0) return;
    if (sx_q8 < 0) flip ^= 1;
    asx = sx_q8 < 0 ? -sx_q8 : sx_q8;
    asy = sy_q8;
    s = (asx + asy + 256) / 512;
    if (s >= 1 && s <= 4 && asx == s * 256 && asy == s * 256) {
        rp_tex(mode, slot, ax - iw * s / 2, ay - ih * s, iw * s, ih * s, iw, ih, flip, argb);
        return;
    }
    dw = (iw * asx + 128) >> 8;
    dh = (ih * asy + 128) >> 8;
    if (dw <= 0 || dh <= 0) return;
    rp_tex(mode, slot, ax - dw / 2, ay - dh, dw, dh, iw, ih, flip, argb);
}

/* ---- the shell canvas -------------------------------------------------------------
 * Logical screen coordinates at pan 0, clipped to the screen plus a
 * margin. Pass 1 (measure) finds the box the shell covers; pass 2 draws
 * it into a canvas of exactly that box. */
#define CV_X0 (-64)
#define CV_Y0 (-96)
#define CV_X1 (DC2D_LW + 64)
#define CV_Y1 (DC2D_LH + 64)
#define CV_MAX ((CV_X1 - CV_X0) * (CV_Y1 - CV_Y0))        /* 448 x 400 */

static uint8_t cv_pix[CV_MAX];
static struct { int measure, bx0, by0, bx1, by1, x0, y0, w, h; } cv;

static void c_rect(int x, int y, int w, int h, uint8_t c)
{
    int x1 = x + w, y1 = y + h, yy;
    if (x < CV_X0) x = CV_X0;
    if (y < CV_Y0) y = CV_Y0;
    if (x1 > CV_X1) x1 = CV_X1;
    if (y1 > CV_Y1) y1 = CV_Y1;
    if (x1 <= x || y1 <= y) return;
    if (cv.measure) {
        if (x < cv.bx0) cv.bx0 = x;
        if (y < cv.by0) cv.by0 = y;
        if (x1 > cv.bx1) cv.bx1 = x1;
        if (y1 > cv.by1) cv.by1 = y1;
        return;
    }
    if (!c) c = PAL_BLACK;          /* a rect's index 0 is black (never made: the maps keep >= 1) */
    if (x1 - x <= 2) {              /* the walls' 2 px columns: no memset per row */
        uint8_t *d = cv_pix + (size_t)(y - cv.y0) * (size_t)cv.w + (size_t)(x - cv.x0);
        int two = x1 - x == 2;
        for (yy = y; yy < y1; yy++, d += cv.w) { d[0] = c; if (two) d[1] = c; }
        return;
    }
    for (yy = y; yy < y1; yy++)
        memset(cv_pix + (size_t)(yy - cv.y0) * (size_t)cv.w + (size_t)(x - cv.x0), c, (size_t)(x1 - x));
}

/* ---- the shell: room_ref.c's pixel code, writing the canvas ---------------------- */
static uint8_t floor_cell(const RR *r, int pat, int i, int j, int cx, int cy, uint8_t a, uint8_t b)
{
    uint32_t h = rr_hash(i * 4 + cx + 1, j * 4 + cy + 7);
    switch (pat) {
    case 2: return ((cx >> 1) ^ (cy >> 1)) & 1 ? a : b;
    case 3: return (((i + j) & 1) ? (cy & 1) : (cx & 1)) ? a : b;
    case 4: return (h & 7) == 0 ? lit(r, PAL_RAMP(RAMP_GRASS, 6)) : ((h >> 3) & 3) == 0 ? b : a;
    case 5: return (rr_hash(i, j) & 1) ? a : b;
    case 6: return (h % 11) == 0 ? glow(r, PAL_RAMP(RAMP_LAVA, 5)) : (h % 11) == 1 ? glow(r, PAL_RAMP(RAMP_LAVA, 3)) : b;
    default: return (h & 3) == 0 ? lit(r, PAL_PAINT(PAINT_WHITE, 0)) : a;
    }
}

static void floor_tile(const RR *r, int i, int j, uint8_t a, uint8_t b, uint8_t grout)
{
    int cx = scr_x(r, i * 16, j * 16), cy = scr_y(r, i * 16, j * 16, 0), row;
    int pat = r->v->floor == 0xFF ? 0 : (r->v->floor & 7);
    int plank = pat == 1;
    if (cv.measure) { c_rect(cx - 16, cy, 32, 16, 1); return; }   /* the diamond's box */
    for (row = 0; row < 16; row++) {
        int half = row < 8 ? 2 * (row + 1) : 2 * (16 - row);
        if (pat < 2) {
            uint8_t c = plank ? (((row >> 2) & 1) ? a : b) : (((i + j) & 1) ? a : b);
            c_rect(cx - half, cy + row, 1, 1, grout);
            c_rect(cx - half + 1, cy + row, half * 2 - 2, 1, c);
            c_rect(cx + half - 1, cy + row, 1, 1, grout);
        } else {
            int cut[8], nc = 0, k, x0 = -half + 1, x1 = half - 1;
            int grid = pat == 5 || pat == 2 ? 8 : 4;
            for (k = grid; k < 16; k += grid) {
                int d1 = 2 * k - 2 * row - 1, d2 = 2 * row + 1 - 2 * k;
                if (d1 > x0 && d1 < x1) cut[nc++] = d1;
                if (d2 > x0 && d2 < x1) cut[nc++] = d2;
            }
            for (k = 1; k < nc; k++) {
                int v = cut[k], m = k - 1;
                while (m >= 0 && cut[m] > v) { cut[m + 1] = cut[m]; m--; }
                cut[m + 1] = v;
            }
            c_rect(cx - half, cy + row, 1, 1, grout);
            for (k = 0; k <= nc; k++) {
                int s0 = k == 0 ? x0 : cut[k - 1], s1 = k == nc ? x1 : cut[k], mid = (s0 + s1) / 2;
                int X = (2 * row + 1 + mid) / 2, Y = (2 * row + 1 - mid) / 2;
                uint8_t c;
                if (s1 <= s0) continue;
                X = X < 0 ? 0 : X > 15 ? 15 : X;
                Y = Y < 0 ? 0 : Y > 15 ? 15 : Y;
                c = floor_cell(r, pat, i, j, X >> 2, Y >> 2, a, b);
                if (k > 0 && (pat == 2 || pat == 5 || pat == 3)) { c_rect(cx + s0, cy + row, 1, 1, grout); s0++; }
                if (s1 > s0) c_rect(cx + s0, cy + row, s1 - s0, 1, c);
            }
            c_rect(cx + half - 1, cy + row, 1, 1, grout);
        }
    }
}

static const uint8_t ceil_ramp[8] = { RAMP_WOOD, RAMP_SKY, RAMP_PASTEL_PINK, RAMP_PEACH,
                                      RAMP_VAPE, RAMP_BONE, RAMP_LAVA, RAMP_STEEL };

static void wall_column(const RR *r, int x, int bottom, int u, int side)
{
    const DoomoRoomView *v = r->v;
    int paint = v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM;
    int pat = v->wallpaper == 0xFF ? 0 : (v->wallpaper & 7);
    int top = bottom - r->wall_h, cor = 4, skirt = 4, y, ramp = ceil_ramp[v->ceiling & 7];
    int y0 = top + cor, y1 = bottom - skirt;
    uint8_t base = PAL_PAINT(paint, side ? 1 : 2), alt = PAL_PAINT(paint, side ? 0 : 1);
    uint8_t ceil = PAL_RAMP(ramp, side ? 3 : 4), sk = PAL_RAMP(RAMP_WOOD, side ? 2 : 3);
    uint8_t fill = base;
    if (cv.measure) { c_rect(x, top, 2, r->wall_h, 1); return; }  /* the column's box */
    if (pat == 0 && ((u >> 3) & 1)) fill = alt;
    c_rect(x, top, 2, 1, lit(r, PAL_RAMP(ramp, 1)));
    c_rect(x, top + 1, 2, cor - 1, lit(r, ceil));
    if (pat == 5) {
        int du = u & 15, d = du < 8 ? du : 16 - du, ph = (u >> 4) & 1;
        for (y = y0; y < y1; ) {
            int vv = (y - y0) & 15, dv = vv < 8 ? vv : 16 - vv, inside = d + dv < 8, len, ny;
            len = inside ? 8 - d - dv : d + dv - 7;
            if (len < 1) len = 1;
            ny = y + len < y1 ? y + len : y1;
            c_rect(x, y, 2, ny - y, lit(r, (inside ^ ph ^ (((y - y0) >> 4) & 1)) ? alt : base));
            y = ny;
        }
    } else if (pat == 6) {
        for (y = y0; y < y1; y += 8) {
            int row = (y - y0) >> 3, ny = y + 7 < y1 ? y + 7 : y1, uu = u + (row & 1) * 8;
            uint8_t c = (rr_hash(uu >> 4, row) & 1) ? base : alt;
            if ((uu & 15) < 2) c = palette_darker(PAL_PAINT(paint, 2));
            c_rect(x, y, 2, ny - y, lit(r, c));
            if (ny < y1) c_rect(x, ny, 2, 1, lit(r, palette_darker(PAL_PAINT(paint, 2))));
        }
    } else {
        c_rect(x, y0, 2, y1 - y0, lit(r, fill));
    }
    if (pat == 1) {
        for (y = top + cor + 4 + (((u >> 3) & 1) << 2); y < bottom - skirt - 2; y += 8)
            if ((u & 7) < 2) c_rect(x, y, 2, 2, lit(r, alt));
    } else if (pat == 2) {
        c_rect(x, bottom - skirt - r->wall_h / 3, 2, r->wall_h / 3, lit(r, alt));
        c_rect(x, bottom - skirt - r->wall_h / 3, 2, 1, lit(r, PAL_PAINT(paint, 0)));
    } else if (pat == 3 && (u & 15) == 0) {
        c_rect(x, top + cor, 1, bottom - skirt - top - cor, lit(r, alt));
    } else if (pat == 4) {
        int c = (u >> 1) & 7, ph = ((u >> 4) & 1) * 7;
        static const int8_t h0[3] = { 0, 1, 0 }, h1[3] = { 3, 5, 3 };
        if (c >= 2 && c <= 4)
            for (y = y0 + 3 + ph; y + 5 < y1; y += 14)
                c_rect(x, y + h0[c - 2], 2, h1[c - 2] - h0[c - 2], lit(r, PAL_PAINT(PAINT_CHERRY, side ? 1 : 0)));
    } else if (pat == 7) {
        int c = u % 24;
        if (c == 8 || c == 10)
            for (y = y0 + 5 + (int)(rr_hash(u / 24, 3) % 9); y + 3 < y1; y += 17)
                c_rect(x, y + (c == 8 ? 1 : 0), 2, c == 8 ? 1 : 3, lit(r, PAL_RAMP(RAMP_CANDY, 7)));
    }
    c_rect(x, bottom - skirt, 2, skirt, lit(r, sk));
}

static void draw_backdrop(const RR *r, const RoomBackdrop *b)
{
    int u, t;
    for (u = b->u0; u < b->u1; u += 2) {
        int x = b->side ? r->ox - u - 2 : r->ox + u;
        int top = r->oy + (b->side ? (u + 2) >> 1 : u >> 1) - b->h;
        for (t = 0; t < b->h; ) {
            uint8_t c = room_backdrop_px(b, u, t);
            int n = 1;
            while (t + n < b->h && room_backdrop_px(b, u, t + n) == c) n++;
            if (c) c_rect(x, top + t, 2, n, c);
            t += n;
        }
    }
}

static void draw_room_shell(const RR *r, const RoomBackdrop *bd)
{
    const DoomoRoomView *v = r->v;
    const RoomLight *L = r->L;
    int i, j, u;
    int fp = v->floor_paint < 32 ? v->floor_paint : PAINT_CARAMEL;
    uint8_t a = lit(r, PAL_PAINT(fp, 1)), b = lit(r, PAL_PAINT(fp, 2)), g = lit(r, PAL_PAINT(fp, 0));
    RR t = *r;
    if (bd->style) draw_backdrop(r, bd);
    if (v->room_kind == ROOMK_WALLS) {
        for (u = 0; u < r->W * 16; u += 2) {
            t.level = L->n ? L->wall[0][DOOMO_MIN(u >> 4, ROOM_LIGHT_N - 1)] : r->level;
            wall_column(&t, r->ox + u, r->oy + (u >> 1), u, 0);
        }
        for (u = 2; u <= r->D * 16; u += 2) {
            t.level = L->n ? L->wall[1][DOOMO_MIN((u - 2) >> 4, ROOM_LIGHT_N - 1)] : r->level;
            wall_column(&t, r->ox - u, r->oy + (u >> 1), u, 1);
        }
        t.level = L->corner;
        c_rect(r->ox - 1, r->oy - r->wall_h, 2, r->wall_h,
               lit(&t, PAL_PAINT(v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM, 0)));
    }
    for (j = 0; j < r->D; j++)
        for (i = 0; i < r->W; i++) {
            if (L->n && i < ROOM_LIGHT_N && j < ROOM_LIGHT_N && L->tile[j][i] != r->level) {
                t.level = L->tile[j][i];
                floor_tile(&t, i, j, lit(&t, PAL_PAINT(fp, 1)), lit(&t, PAL_PAINT(fp, 2)), lit(&t, PAL_PAINT(fp, 0)));
            } else {
                floor_tile(r, i, j, a, b, g);
            }
        }
    if (v->room_kind != ROOMK_WALLS) {
        uint8_t c[5];
        room_lip_colours(v, r->level, r->tint, c);
        for (u = 0; u < r->W * 16; u += 2) {
            int x = r->ox + u - r->D * 16, y = r->oy + ((u + r->D * 16) >> 1);
            c_rect(x, y, 2, 1, c[0]);
            c_rect(x, y + 1, 2, ROOM_LIP_H - 2, c[1]);
            c_rect(x, y + ROOM_LIP_H - 1, 2, 1, c[3]);
        }
        for (u = 0; u < r->D * 16; u += 2) {
            int x = r->ox + r->W * 16 - u - 2, y = r->oy + ((u + r->W * 16) >> 1);
            c_rect(x, y, 2, 1, c[0]);
            c_rect(x, y + 1, 2, ROOM_LIP_H - 2, c[2]);
            c_rect(x, y + ROOM_LIP_H - 1, 2, 1, c[4]);
        }
    }
}

/* room_ref's draw_roof, clipped to the canvas instead of the screen */
static void draw_roof(const RoomRoof *f)
{
    uint8_t row[512];
    int y, y0, y1, x0, x1;
    room_roof_rows(f, &y0, &y1);
    room_roof_cols(f, &x0, &x1);
    if (x0 < CV_X0) x0 = CV_X0;
    if (x1 > CV_X1) x1 = CV_X1;
    if (y0 < CV_Y0) y0 = CV_Y0;
    if (y1 > CV_Y1) y1 = CV_Y1;
    if (x1 - x0 > (int)sizeof(row)) x1 = x0 + (int)sizeof(row);
    if (cv.measure) {                    /* the roof's box will do (transparent margins) */
        c_rect(x0, y0, x1 - x0, y1 - y0, 1);
        return;
    }
    for (y = y0; y < y1; y++) {
        int x = x0;
        if (!room_roof_row(f, y, x0, x1, row)) continue;
        while (x < x1) {
            int e = x + 1;
            uint8_t c = row[x - x0];
            while (e < x1 && row[e - x0] == c) e++;
            if (c) c_rect(x, y, e - x, 1, c);
            x = e;
        }
    }
}

/* everything the shell's pixels depend on (memset first: padding too) */
typedef struct ShellKey {
    int16_t ox, oy, W, D, k, wall_h, level, tint;
    uint8_t wallpaper, floor, wall_paint, floor_paint, ceiling, room_kind, has_roof, pad;
    RoomLight L;
    RoomBackdrop bd;
    RoomRoof rf;
} ShellKey;

static ShellKey shell_key;
static int shell_valid, shell_x, shell_y;
static DoomoImage shell_img;

static void shell_paint(const RR *r, const RoomBackdrop *bd, const RoomRoof *rf, int has_roof)
{
    if (has_roof) draw_roof(rf);
    draw_room_shell(r, bd);
}

static void shell_render(const RR *r, const RoomBackdrop *bd, const RoomRoof *rf, int has_roof)
{
    uint32_t t0 = dc3d_be_us(), dt;
    cv.measure = 1;
    cv.bx0 = cv.by0 = 1 << 20;
    cv.bx1 = cv.by1 = -(1 << 20);
    shell_paint(r, bd, rf, has_roof);
    cv.measure = 0;
    if (cv.bx1 <= cv.bx0 || cv.by1 <= cv.by0) { shell_valid = 0; return; }
    cv.x0 = cv.bx0; cv.y0 = cv.by0;
    cv.w = cv.bx1 - cv.bx0; cv.h = cv.by1 - cv.by0;
    memset(cv_pix, 0, (size_t)cv.w * (size_t)cv.h);
    shell_paint(r, bd, rf, has_roof);
    shell_img.w = (uint16_t)cv.w;
    shell_img.h = (uint16_t)cv.h;
    shell_img.pix = cv_pix;
    shell_img.uid = doomo_image_new_uid();
    shell_x = cv.x0;
    shell_y = cv.y0;
    shell_valid = 1;
    dc_room_stats.shells++;
    dt = dc3d_be_us() - t0;
    if (dt > dc_room_stats.shell_us_max) dc_room_stats.shell_us_max = dt;
}

int dc_room_shell(DoomoImage *img, int *x, int *y)
{
    if (!shell_valid) return 0;
    if (img) *img = shell_img;
    if (x) *x = shell_x;
    if (y) *y = shell_y;
    return 1;
}

/* ---- the pieces' textures ----------------------------------------------------------- */
#define PT_SIZE 1024                     /* power of two                             */
#define PT_FULL (PT_SIZE * 3 / 4)
typedef struct PTex { uint32_t k0, k1, uid; } PTex;
static PTex ptab[PT_SIZE];
static int pt_n;
static uint8_t dec_buf[65536];           /* any sprite: w, h are bytes                */

enum { PFX_GREY = 1, PFX_INVALID = 2 };  /* + tint << 2                              */

static PTex *pt_find(uint32_t k0, uint32_t k1)
{
    uint32_t i = doomo_hash32(k0 * 0x9E3779B1u ^ k1) & (PT_SIZE - 1);
    if (pt_n >= PT_FULL) {               /* forget them all; their textures age out */
        memset(ptab, 0, sizeof(ptab));
        pt_n = 0;
    }
    while (ptab[i].uid) {
        if (ptab[i].k0 == k0 && ptab[i].k1 == k1) return &ptab[i];
        i = (i + 1) & (PT_SIZE - 1);
    }
    ptab[i].k0 = k0;
    ptab[i].k1 = k1;
    ptab[i].uid = 0;                     /* the caller fills it                       */
    return &ptab[i];
}

/* the recolour room_ref's sprite_fx makes, without its dither and blink */
static void piece_fx(uint8_t *pix, int n, int fx)
{
    int i, tint = fx >> 2;
    if (!(fx & (PFX_GREY | PFX_INVALID)) && !tint) return;
    for (i = 0; i < n; i++) {
        uint8_t c = pix[i];
        if (!c) continue;
        if (fx & PFX_GREY) {
            int l = palette_luma(c);
            c = PAL_RAMP(RAMP_STONE, l > 200 ? 7 : l > 150 ? 6 : l > 100 ? 5 : l > 60 ? 4 : 3);
        } else if (fx & PFX_INVALID) {
            c = palette_shade(c, 9);
        }
        if (tint) c = doomo_tintmap[tint][c];
        pix[i] = c;
    }
}

/* The texture slot of a piece's sprite; -1: no sprite (room_ref draws a
 * box), -2: no texture (video RAM full: nothing drawn). */
static int piece_tex(const DoomoRoomView *v, int furn, int facing, int frame, int pa, int pb, int lvl, int fx,
                     int *w, int *h, int *ox, int *oy)
{
    const DoomoFurn *f = home_furn(furn);
    const DoomoFurnIso *im;
    PTex *p;
    int frames, e, slot;
    if (!f || !v->plat_cache || !doomo_furn_iso_count || furn < 0 || furn >= doomo_furn_count) return -1;
    frames = f->frames ? f->frames : 1;
    e = doomo_furn_iso_first[furn] + (frame % frames) * FURN_ISO_FACINGS + (facing & 3);
    if (e >= doomo_furn_iso_count) return -1;
    im = &doomo_furn_iso[e];
    if (!im->w || !im->h || (unsigned)(im->w * im->h) > v->plat_cache_cap) return -1;
    *w = im->w; *h = im->h; *ox = im->ox; *oy = im->oy;
    p = pt_find((uint32_t)furn | (uint32_t)e << 16,
                (uint32_t)(pa & 0xFF) | (uint32_t)(pb & 0xFF) << 8 | (uint32_t)(lvl & 0xFF) << 16 |
                (uint32_t)(fx & 0xFF) << 24);
    slot = p->uid ? dc2d_tex_find(p->uid, im->w, im->h) : -1;
    if (slot < 0) {
        DoomoImage img;
        int ox2, oy2;
        if (!room_ref_sprite(furn, facing, frame, pa, pb, lvl, dec_buf, sizeof(dec_buf), &img, &ox2, &oy2))
            return -1;
        piece_fx(dec_buf, img.w * img.h, fx);
        if (!p->uid) { p->uid = doomo_image_new_uid(); pt_n++; }
        img.uid = p->uid;
        slot = dc2d_tex_image(&img);
        dc_room_stats.decodes++;
        if (slot < 0) return -2;
    }
    return slot;
}

/* ---- pieces, actors, the cursor ------------------------------------------------- */
#define GHOST_ARGB   0x80FFFFFFu         /* a ghost: the piece at 50%                  */
#define INVALID_ARGB 0xC0FF4040u         /* an invalid ghost: its grey shape in red, 75% */

/* a shaded box (room_ref's iso_box): the fallback when there is no sprite */
static void iso_box(const RR *r, int x0, int y0, int x1, int y1, int z, int h, uint8_t base)
{
    int uB = x0 - y0, uR = x1 - y0, uF = x1 - y1, uL = x0 - y1, u;
    uint8_t top = lit(r, palette_lighter(base)), left = lit(r, base), right = lit(r, palette_darker(base));
    for (u = uL; u < uR; u += 2) {
        int vtop = u < uB ? ((2 * x0 - u) >> 1) : ((u + 2 * y0) >> 1);
        int vfront = u < uF ? ((u + 2 * y1) >> 1) : ((2 * x1 - u) >> 1);
        int x = r->ox + u, ytop = r->oy + vtop - z - h, yf = r->oy + vfront - z - h;
        rp_rect(x, ytop, 2, yf - ytop, top);
        rp_rect(x, yf, 2, h, u < uF ? left : right);
        rp_rect(x, yf + h, 2, 1, lit(r, palette_darker(palette_darker(base))));
    }
}

static void draw_piece(const RR *r, const DoomoRoomObj *o, const DcRoomItem *it)
{
    const DoomoFurn *f = home_furn(o->furn);
    int rot = (RO_ROT(o) - r->k) & 3;
    int sx = scr_x(r, it->x0 * 16, it->y0 * 16), sy = scr_y(r, it->x0 * 16, it->y0 * 16, o->z);
    int w, h, ox, oy, lvl, fl, slot, fx = 0, mode;
    int pa = o->paint_a == 0xFF ? -1 : o->paint_a, pb = o->paint_b == 0xFF ? -1 : o->paint_b;
    uint32_t argb = 0xFFFFFFFFu;
    if (!f) return;
    lvl = (o->flags & RO_FULLBRIGHT) ? 15 : room_light_at(r->L, (it->x0 + it->x1) * 8, (it->y0 + it->y1) * 8);
    fl = ((f->flags & FF_LIGHT) && !(o->flags & RO_FULLBRIGHT) ? RR_SPR_DARK : 0) |
         (r->L->pane == RPANE_NIGHT ? RR_SPR_NIGHT : r->L->pane == RPANE_DUSK ? RR_SPR_DUSK : 0);
    if (o->flags & (RO_HALF_X | RO_HALF_Y)) {
        int hx = (o->flags & RO_HALF_X) ? 8 : 0, hy = (o->flags & RO_HALF_Y) ? 8 : 0, k, t;
        for (k = 0; k < r->k; k++) { t = hx; hx = -hy; hy = t; }
        sx += hx - hy;
        sy += (hx + hy) >> 1;
    }
    if (o->jitter) sx += (int)((r->v->time >> 1) & 1) * o->jitter - o->jitter / 2;
    /* a ghost that cannot go there: the grey silhouette, made red by INVALID_ARGB.
     * A solid invalid piece: room_ref's sprite_fx darkens it only when it
     * recolours at all (a tint; its blink is the glow here) */
    if ((o->flags & (RO_GHOST | RO_SEMI)) == (RO_GHOST | RO_SEMI) ||
        ((o->flags & RO_INVALID) && (o->flags & (RO_GHOST | RO_SEMI))))
        fx |= PFX_GREY;
    else if ((o->flags & RO_INVALID) && r->tint)
        fx |= PFX_INVALID;
    fx |= r->tint << 2;
    slot = piece_tex(r->v, o->furn, rot, RO_ANIM(o), pa, pb, lvl | fl, fx, &w, &h, &ox, &oy);
    if (slot == -2) return;
    if (slot >= 0) {
        mode = DC2D_IMAGE;
        if (o->flags & (RO_GHOST | RO_SEMI)) {
            mode = DC2D_GLASS;
            argb = (o->flags & RO_INVALID) ? INVALID_ARGB : GHOST_ARGB;
        }
        if (o->sq_x || o->sq_y)
            rp_xform(mode, slot, w, h, sx - ox + w / 2, sy - oy + h, 256 + o->sq_x * 2, 256 + o->sq_y * 2, 0, argb);
        else
            rp_image(mode, slot, w, h, sx - ox, sy - oy, 0, argb);
        if (o->flags & RO_SELECTED) {    /* the pulse: a glow over the shape, every 32 ticks */
            int ph = r->v->time & 31, a = 24 + (ph < 16 ? ph : 31 - ph) * 6;
            uint32_t g = ((uint32_t)a << 24) | 0x00FFF4D0u;
            if (o->sq_x || o->sq_y)
                rp_xform(DC2D_ADD, slot, w, h, sx - ox + w / 2, sy - oy + h, 256 + o->sq_x * 2, 256 + o->sq_y * 2, 0, g);
            else
                rp_image(DC2D_ADD, slot, w, h, sx - ox, sy - oy, 0, g);
        }
        return;
    }
    {
        int bh = f->fh ? f->fh * 4 : 2, pa2 = o->paint_a < 32 ? o->paint_a : PAINT_CARAMEL;
        uint8_t base = (o->flags & RO_SEMI) ? PAL_RAMP(RAMP_STONE, 5) : PAL_PAINT(pa2, 1);
        RR t = *r;
        t.level = lvl;
        dc_room_stats.boxes++;
        iso_box(&t, it->x0 * 16 + 1, it->y0 * 16 + 1, it->x1 * 16 - 1, it->y1 * 16 - 1, o->z, bh, base);
    }
}

/* doomo_tintmap[t] with index 0 kept clear (a texture's 0 is transparent) */
static uint8_t tint_map[TINT_COUNT][256];
static uint8_t tint_map_ok[TINT_COUNT];

static void draw_actor(const RR *r, const DoomoRoomActor *a)
{
    const DoomoRoomView *v = r->v;
    fix16 ax = a->x, ay = a->y;
    int k, t, u, vv, w, d, flip = (a->flags & BB_FLIPX) != 0, slot, mode = DC2D_IMAGE;
    uint32_t argb = 0xFFFFFFFFu;
    const DoomoImage *img = a->img;
    if (!img || !img->pix || !img->w || !img->h) return;
    for (k = 0, w = v->w, d = v->d; k < r->k; k++) { t = ax; ax = INT2FIX(d) - ay; ay = t; t = w; w = d; d = t; }
    u = scr_x(r, FIX2INT(ax * 16), FIX2INT(ay * 16));
    vv = scr_y(r, FIX2INT(ax * 16), FIX2INT(ay * 16), a->z);
    slot = dc2d_tex_image(img);
    if (slot < 0) return;
    /* room_ref recolours (dither, tint) only through a cache that holds the image */
    if (((a->flags & BB_TRANSLUCENT) || r->tint) && v->plat_cache &&
        (uint32_t)(img->w * img->h) <= v->plat_cache_cap) {
        if (r->tint) {
            if (!tint_map_ok[r->tint]) {
                memcpy(tint_map[r->tint], doomo_tintmap[r->tint], 256);
                tint_map[r->tint][0] = 0;
                tint_map_ok[r->tint] = 1;
            }
            slot = dc2d_tex_remap(slot, 32 + r->tint, tint_map[r->tint]);
            if (slot < 0) return;
        }
        if (a->flags & BB_TRANSLUCENT) { mode = DC2D_GLASS; argb = GHOST_ARGB; }
    }
    if (a->sx && a->sy && (a->sx != 256 || a->sy != 256))
        rp_xform(mode, slot, img->w, img->h, u, vv + 2, a->sx, a->sy, flip, argb);
    else
        rp_image(mode, slot, img->w, img->h, u - img->w / 2, vv - img->h + 2, flip, argb);
}

static void draw_cursor(const RR *r, int cx, int cy)
{
    int x0 = cx, y0 = cy, x1 = cx + 1, y1 = cy + 1, bx, by;
    uint8_t c = ((r->v->time >> 3) & 1) ? PAL_WHITE : PAL_UI_GOLD;
    rot_box(r->k, r->v->w, r->v->d, &x0, &y0, &x1, &y1);
    bx = scr_x(r, x0 * 16, y0 * 16);
    by = scr_y(r, x0 * 16, y0 * 16, 0);
    rp_line(bx, by, bx + 16, by + 8, c);
    rp_line(bx + 16, by + 8, bx, by + 16, c);
    rp_line(bx, by + 16, bx - 16, by + 8, c);
    rp_line(bx - 16, by + 8, bx, by, c);
    rp_line(bx, by + 17, bx - 15, by + 9, PAL_UI_PINK_D);
    rp_line(bx + 1, by + 17, bx + 16, by + 9, PAL_UI_PINK_D);
}

/* ---- the order --------------------------------------------------------------------- */
static int behind(const DcRoomItem *a, const DcRoomItem *b)
{
    int ox = a->x0 < b->x1 && b->x0 < a->x1, oy = a->y0 < b->y1 && b->y0 < a->y1;
    if (ox && oy) return a->layer < b->layer || (a->layer == b->layer && a->z < b->z);
    if (a->x1 <= b->x0 && oy) return 1;
    if (a->y1 <= b->y0 && ox) return 1;
    if (a->x1 <= b->x0 && a->y1 <= b->y0) return 1;
    return 0;
}

void dc_room_sort(const DcRoomItem *it, int n, int16_t *order)
{
    static uint8_t edge[DC_ROOM_MAX_ITEMS][DC_ROOM_MAX_ITEMS];   /* edge[j][i]: j before i */
    int16_t indeg[DC_ROOM_MAX_ITEMS], key[DC_ROOM_MAX_ITEMS], key2[DC_ROOM_MAX_ITEMS];
    uint8_t done[DC_ROOM_MAX_ITEMS];
    int i, j, total;
    if (n > DC_ROOM_MAX_ITEMS) n = DC_ROOM_MAX_ITEMS;
    for (i = 0; i < n; i++) {
        indeg[i] = 0;
        done[i] = 0;
        key2[i] = (int16_t)((it[i].x0 + it[i].y0) * 8 + it[i].layer * 2);
        key[i] = (int16_t)(key2[i] + (it[i].z ? 1 : 0) - (it[i].layer == 0 ? 1000 : 0));
    }
    for (j = 0; j < n; j++)
        for (i = 0; i < n; i++) {
            uint8_t e = (uint8_t)(i != j && behind(&it[j], &it[i]) && !behind(&it[i], &it[j]));
            edge[j][i] = e;
            indeg[i] = (int16_t)(indeg[i] + e);
        }
    for (total = 0; total < n; total++) {
        int pick = -1, best = 0x7FFF;
        for (i = 0; i < n; i++)
            if (!done[i] && !indeg[i] && key[i] < best) { best = key[i]; pick = i; }
        if (pick < 0)                    /* a cycle: plain depth order */
            for (i = 0; i < n; i++)
                if (!done[i] && key2[i] < best) { best = key2[i]; pick = i; }
        done[pick] = 1;
        order[total] = (int16_t)pick;
        for (i = 0; i < n; i++)
            if (edge[pick][i]) indeg[i]--;
    }
}

/* ---- the zoom ---------------------------------------------------------------------- */
static void zoom_update(const RR *r, const DoomoRoomView *v)
{
    int vw = v->vw ? v->vw : plat_screen_w(), vh = v->vh ? v->vh : plat_screen_h(), i;
    float cx = (float)v->vx + (float)vw * 0.5f, cy = (float)v->vy + (float)vh * 0.5f;
    float ws = 1.0f, wfx = cx, wfy = cy;
    int16_t px, py;
    if (v->zoom == 1) {                  /* the PS1's close-up on the player */
        ws = 1.6f;
        wfx = (float)(scr_x(r, r->W * 8, r->D * 8) + v->pan_x);        /* no player: the floor's centre */
        wfy = (float)(scr_y(r, r->W * 8, r->D * 8, 0) + v->pan_y);
        for (i = 0; i < v->nact; i++)
            if (v->actors[i].flags & RA_PLAYER) {
                room_ref_project(v, v->actors[i].x, v->actors[i].y, v->actors[i].z, &px, &py);
                wfx = (float)px;
                wfy = (float)py - 16.0f;
                break;
            }
    } else if (v->zoom >= 2) {           /* Talk: the circle fills the viewport */
        int16_t fx, fy;
        float rr = (float)v->focus_r / 65536.0f, hw = rr * 22.7f + 12.0f, hh = rr * 11.4f + 36.0f, sx_, sy_;
        room_ref_project(v, v->focus_x, v->focus_y, 20, &fx, &fy);
        sx_ = (float)vw * 0.5f / hw;
        sy_ = (float)vh * 0.5f / hh;
        ws = sx_ < sy_ ? sx_ : sy_;
        ws = ws < 1.0f ? 1.0f : ws > 3.0f ? 3.0f : ws;
        wfx = (float)fx; wfy = (float)fy;
    }
    if (!ZS.have || (uint16_t)(v->time - ZS.t) > 40) {
        ZS.s = ws; ZS.fx = wfx; ZS.fy = wfy;
    } else {
        ZS.s += (ws - ZS.s) / 3.0f;
        ZS.fx += (wfx - ZS.fx) / 3.0f;
        ZS.fy += (wfy - ZS.fy) / 3.0f;
    }
    ZS.have = 1;
    ZS.t = v->time;
    if (!v->zoom && ZS.s - 1.0f < 0.004f && ZS.s - 1.0f > -0.004f &&
        ZS.fx - cx < 0.05f && ZS.fx - cx > -0.05f && ZS.fy - cy < 0.05f && ZS.fy - cy > -0.05f) {
        ZS.s = 1.0f; ZS.fx = cx; ZS.fy = cy;
        Z.on = 0;
    } else {
        Z.on = 1;
    }
    Z.s = ZS.s; Z.fx = ZS.fx; Z.fy = ZS.fy; Z.cx = cx; Z.cy = cy;
}

int dc_room_xform(float *s, float *fx, float *fy, float *cx, float *cy)
{
    if (s) *s = Z.on ? Z.s : 1.0f;
    if (fx) *fx = Z.fx;
    if (fy) *fy = Z.fy;
    if (cx) *cx = Z.cx;
    if (cy) *cy = Z.cy;
    return Z.on;
}

void dc_room_reset(void)
{
    memset(&Z, 0, sizeof(Z));
    memset(&ZS, 0, sizeof(ZS));
    memset(ptab, 0, sizeof(ptab));
    pt_n = 0;
    shell_valid = 0;
    memset(&shell_key, 0, sizeof(shell_key));
}

/* ---- plat_room ----------------------------------------------------------------------- */
void DC2D_FN(room)(const DoomoRoomView *v)
{
    RR r;
    RoomBackdrop bd;
    RoomLight L;
    RoomRoof rf;
    ShellKey key;
    DcRoomItem items[DC_ROOM_MAX_ITEMS], front[DC_ROOM_MAX_ITEMS];
    int16_t order[DC_ROOM_MAX_ITEMS], front_idx[DC_ROOM_MAX_ITEMS];
    int n = 0, nf = 0, i, j, vw, vh, has_roof;
    uint32_t t0, dt;
    int p0;
    if (!v || plat_draw_skipped()) return;
    t0 = dc3d_be_us();
    p0 = dc2d_prim_count();
    memset(&L, 0, sizeof(L));
    memset(items, 0, sizeof(items));
    r.v = v;
    r.L = &L;
    rr_origin(&r, v, 1, 0);
    vw = v->vw ? v->vw : plat_screen_w();
    vh = v->vh ? v->vh : plat_screen_h();
    r.level = 6 + (v->daylight > 15 ? 15 : v->daylight) * 9 / 15;
    r.tint = (v->tint && v->tint < TINT_COUNT && v->tint_level >= 4) ? v->tint : 0;
    room_backdrop_setup(&bd, v, r.k, r.W, r.D, r.wall_h, r.level, r.tint);
    room_light_setup(&L, v, r.k, r.W, r.D, r.level);
    rr_origin(&r, v, v->room_kind == ROOMK_WALLS || bd.style, 0);      /* pan 0: the shell's origin */
    has_roof = v->room_kind == ROOMK_WALLS && v->roof &&
               room_roof_setup(&rf, v, r.k, r.W, r.D, r.ox, r.oy, r.wall_h, r.level, r.tint);
    if (!has_roof) memset(&rf, 0, sizeof(rf));
    zoom_update(&r, v);

    /* 1. the background: the viewport's colour or the open kinds' sky (not panned, not zoomed) */
    if (v->room_kind == ROOMK_WALLS) {
        DC2D_FN(rect)(v->vx, v->vy, vw, vh, r.tint ? lit(&r, PAL_RAMP(RAMP_VAPE, 1)) : PAL_RAMP(RAMP_VAPE, 1));
    } else {
        int dl = v->daylight > 15 ? 15 : v->daylight;
        uint8_t sky = dl >= 13 ? PAL_RAMP(RAMP_SKY, 5) : dl >= 9 ? PAL_RAMP(RAMP_PEACH, 5) : PAL_RAMP(RAMP_SKY, 1);
        DC2D_FN(rect)(v->vx, v->vy, vw, vh, lit(&r, sky));
        if (dl < 9)
            for (i = 0; i < 12; i++) {
                uint32_t h = rr_hash(i, 77);
                DC2D_FN(rect)(v->vx + (int)(h % (uint32_t)vw), v->vy + (int)((h >> 12) % (uint32_t)(vh / 3 + 1)),
                              1, 1, PAL_RAMP(RAMP_CANDY, 7));
            }
    }

    /* 2. the shell: one texture, rendered again only when its key changes */
    memset(&key, 0, sizeof(key));
    key.ox = (int16_t)r.ox; key.oy = (int16_t)r.oy; key.W = (int16_t)r.W; key.D = (int16_t)r.D;
    key.k = (int16_t)r.k; key.wall_h = (int16_t)r.wall_h; key.level = (int16_t)r.level; key.tint = (int16_t)r.tint;
    key.wallpaper = v->wallpaper; key.floor = v->floor; key.wall_paint = v->wall_paint;
    key.floor_paint = v->floor_paint; key.ceiling = v->ceiling; key.room_kind = v->room_kind;
    key.has_roof = (uint8_t)has_roof;
    key.L = L;
    key.bd = bd;
    key.rf = rf;
    if (!shell_valid || memcmp(&key, &shell_key, sizeof(key)) != 0) {
        shell_key = key;
        shell_render(&r, &bd, &rf, has_roof);
    }
    if (shell_valid) {
        int slot = dc2d_tex_image(&shell_img);
        if (slot >= 0)
            rp_image(DC2D_IMAGE, slot, shell_img.w, shell_img.h, shell_x + v->pan_x, shell_y + v->pan_y, 0, 0xFFFFFFFFu);
    }

    /* everything else is drawn at the panned origin */
    r.ox += v->pan_x;
    r.oy += v->pan_y;

    /* 3. the rubber band of a multi-select */
    for (j = 0; j < v->d; j++)
        for (i = 0; i < v->w; i++) {
            int bit = j * v->w + i;
            if (bit < 128 && (v->sel_mask[bit >> 3] >> (bit & 7) & 1)) {
                int x0 = i, y0 = j, x1 = i + 1, y1 = j + 1, bx, by;
                rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
                bx = scr_x(&r, x0 * 16, y0 * 16);
                by = scr_y(&r, x0 * 16, y0 * 16, 0);
                rp_line(bx, by + 1, bx + 14, by + 8, PAL_UI_GOLD);
                rp_line(bx + 14, by + 8, bx, by + 15, PAL_UI_GOLD);
                rp_line(bx, by + 15, bx - 14, by + 8, PAL_UI_GOLD);
                rp_line(bx - 14, by + 8, bx, by + 1, PAL_UI_GOLD);
            }
        }

    /* 4. the wall pieces on the two back walls; the open kinds' front-edge ones last */
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = home_furn(o->furn);
        int side, rot, x0, y0, x1, y1;
        DcRoomItem it;
        if (!f || !(f->flags & FF_WALL)) continue;
        side = room_wall_side(v, o, r.k);
        if (side < 0) continue;
        rot = room_wall_box(v, o, r.k, side, &x0, &y0, &x1, &y1);
        it.x0 = (int8_t)x0; it.y0 = (int8_t)y0; it.x1 = (int8_t)x1; it.y1 = (int8_t)y1;
        it.z = 0;
        it.idx = (int16_t)i;
        if (side >= 2) {
            if (nf >= DC_ROOM_MAX_ITEMS) continue;
            it.layer = (uint8_t)rot;
            front[nf] = it;
            front_idx[nf++] = (int16_t)i;
            continue;
        }
        {
            DoomoRoomObj tmp = *o;
            tmp.rot_anim = RO_ROT_ANIM(rot + r.k, RO_ANIM(o));
            draw_piece(&r, &tmp, &it);
        }
    }

    /* 5. rugs, then everything that stands, in room_ref's order */
    for (i = 0; i < v->nobj && n < DC_ROOM_MAX_ITEMS; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = home_furn(o->furn);
        int w, d, x0, y0, x1, y1;
        if (!f || (f->flags & FF_WALL)) continue;
        home_footprint(f, RO_ROT(o), &w, &d);
        x0 = o->x; y0 = o->y; x1 = x0 + w; y1 = y0 + d;
        rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
        items[n].x0 = (int8_t)x0; items[n].y0 = (int8_t)y0; items[n].x1 = (int8_t)x1; items[n].y1 = (int8_t)y1;
        items[n].layer = (uint8_t)((f->flags & FF_RUG) ? 0 : 1);
        items[n].z = o->z;
        items[n].idx = (int16_t)i;
        n++;
    }
    for (i = 0; i < v->nact && n < DC_ROOM_MAX_ITEMS; i++) {
        int x0 = FIX2INT(v->actors[i].x), y0 = FIX2INT(v->actors[i].y), x1 = x0 + 1, y1 = y0 + 1;
        rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
        items[n].x0 = (int8_t)x0; items[n].y0 = (int8_t)y0; items[n].x1 = (int8_t)x1; items[n].y1 = (int8_t)y1;
        items[n].layer = 2;
        items[n].z = 0;
        items[n].idx = (int16_t)(-1 - i);
        n++;
    }
    dc_room_sort(items, n, order);
    for (i = 0; i < n; i++) {
        const DcRoomItem *it = &items[order[i]];
        if (it->idx >= 0) draw_piece(&r, &v->objs[it->idx], it);
        else draw_actor(&r, &v->actors[-1 - it->idx]);
    }
    for (i = 0; i < nf; i++) {
        DoomoRoomObj tmp = v->objs[front_idx[i]];
        tmp.rot_anim = RO_ROT_ANIM(front[i].layer + r.k, RO_ANIM(&v->objs[front_idx[i]]));
        draw_piece(&r, &tmp, &front[i]);
    }

    /* 6. the cursor */
    if (v->cursor_x >= 0 && v->cursor_y >= 0) draw_cursor(&r, v->cursor_x, v->cursor_y);

    dc_room_stats.prims = (uint32_t)(dc2d_prim_count() - p0);
    if (dc_room_stats.prims > dc_room_stats.prims_max) dc_room_stats.prims_max = dc_room_stats.prims;
    dc_room_stats.items = (uint32_t)n;
    if ((uint32_t)n > dc_room_stats.items_max) dc_room_stats.items_max = (uint32_t)n;
    dt = dc3d_be_us() - t0;
    dc_room_stats.us_last = dt;
    if (dt > dc_room_stats.us_max) dc_room_stats.us_max = dt;
    dc_room_stats.us_sum += dt;
    dc_room_stats.n++;
}

/* (T0) where a room point is on the screen: room_ref's projection, then
 * this view's zoom (the last plat_room's transform) */
void DC2D_FN(room_project)(const struct DoomoRoomView *v, fix16 x, fix16 y, int z, int16_t *sx, int16_t *sy)
{
    int16_t a, b;
    room_ref_project(v, x, y, z, &a, &b);
    if (Z.on) {
        float fa = Z.cx + Z.s * ((float)a - Z.fx), fb = Z.cy + Z.s * ((float)b - Z.fy);
        a = (int16_t)(fa < 0.0f ? fa - 0.5f : fa + 0.5f);
        b = (int16_t)(fb < 0.0f ? fb - 0.5f : fb + 0.5f);
    }
    if (sx) *sx = a;
    if (sy) *sy = b;
}
