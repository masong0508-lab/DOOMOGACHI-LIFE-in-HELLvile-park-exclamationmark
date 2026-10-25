/* room_ref.c - the reference room renderer (owner: H4; HF sec.6.1, 9.1,
 * 11.5). It defines what plat_room() draws: the GBA (gba/src/room.c, H2)
 * must match it pixel for pixel on the parity layout, and the PS1 (H3)
 * draws the same room in 3D. Until those land, this weak plat_room() is
 * the renderer on every platform; it only uses plat_rect / plat_line /
 * plat_image, so it is slow but correct. Full redraw every frame.
 *
 * The rules (all integer):
 *  - view rotation k = ((yaw + 512) >> 10) & 3 turns the room by k * 90
 *    degrees, (x, y) -> (W - 1 - y, x) per step; a piece's rot becomes
 *    (rot - k) & 3. Only the two back walls (x' = 0: wall B', y' = 0:
 *    wall A') are drawn; wall pieces on the other two walls are cut away.
 *  - projection: 1 tile = 16 units; screen x = ox + X - Y, y = oy + (X + Y)
 *    / 2 - Z (units, Z in px). (ox, oy) is the back corner: the room is
 *    centred in the viewport; when it is taller than the viewport the
 *    floor stays whole and the walls lose their top. pan_x / pan_y shift it.
 *  - walls are wall_h = 64 px tall when the viewport is >= 200 px high,
 *    else 48 (the GBA's 136 px room viewport).
 *  - order: background, walls, floor, wall pieces, rugs, then floor
 *    pieces, stacked smalls and actors in a topological back-to-front
 *    order of their footprints, then the cursor.
 *  - a piece's facing: rot 0 = facing 0, 1 = facing 1, 2 = facing 1
 *    mirrored, 3 = facing 0 mirrored (furn.h); the image's (ox, oy) sits
 *    on its footprint's back corner. Paint slots A / B replace the marker
 *    shades; the light level comes from `daylight` (6..15); RO_FULLBRIGHT
 *    pieces (lit lamps) ignore it.
 *  - the sprites are decoded into the view's plat_cache (the scene scratch
 *    tail). Without a cache or without sprites (the PS1 links none) each
 *    piece is drawn as a shaded box of its footprint and height.
 *  - (H4 M2) patterns are `& 7`: wallpaper 0 stripes, 1 dots, 2 wainscot,
 *    3 panels, 4 hearts, 5 harlequin, 6 stone, 7 stars; floor 0 checker,
 *    1 planks, 2 tiles, 3 herringbone, 4 grass, 5 flagstone, 6 lava crack,
 *    7 cloud. `ceiling` 0..7 picks the cornice ramp (0 = wood). A tint
 *    (TINT_*, tint_level >= 4) maps every colour through doomo_tintmap.
 *    RO_GHOST is a 50% checker dither (RO_INVALID: darker), RO_GHOST |
 *    RO_SEMI a grey dithered silhouette, RO_SELECTED blinks lighter;
 *    RO_HALF_X / Y shift a piece +8 units in room space. Actors scale
 *    about their feet when sx / sy != 256 and BB_TRANSLUCENT dithers them.
 *    The open kinds' sky follows `daylight`.
 */
#include "home_int.h"

typedef struct RR {
    int ox, oy, W, D, k, wall_h, level, tint;
    const DoomoRoomView *v;
} RR;

static uint8_t lit(const RR *r, uint8_t c)
{
    c = r->level >= 15 ? c : palette_shade(c, r->level);
    return r->tint ? doomo_tintmap[r->tint][c] : c;
}

static uint32_t rr_hash(int a, int b)
{
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return h ^ (h >> 12);
}

/* rotate a tile box (x0, y0, x1, y1 exclusive) by k quarter turns */
static void rot_box(int k, int w, int d, int *x0, int *y0, int *x1, int *y1)
{
    int i;
    for (i = 0; i < k; i++) {
        int nx0 = w - *y1, nx1 = w - *y0, ny0 = *x0, ny1 = *x1, t;
        *x0 = nx0; *x1 = nx1; *y0 = ny0; *y1 = ny1;
        t = w; w = d; d = t;
    }
}

static int scr_x(const RR *r, int X, int Y) { return r->ox + X - Y; }
static int scr_y(const RR *r, int X, int Y, int Z) { return r->oy + ((X + Y) >> 1) - Z; }

/* ---- floor and walls ------------------------------------------------------------ */
/* the colour of the 4x4-unit cell (cx, cy) 0..3 of tile (i, j) for floor
 * patterns 2..7 (0 / 1 keep their M1 rows exactly: the parity golden) */
static uint8_t floor_cell(const RR *r, int pat, int i, int j, int cx, int cy, uint8_t a, uint8_t b)
{
    uint32_t h = rr_hash(i * 4 + cx + 1, j * 4 + cy + 7);
    switch (pat) {
    case 2: return ((cx >> 1) ^ (cy >> 1)) & 1 ? a : b;                         /* tiles */
    case 3: return (((i + j) & 1) ? (cy & 1) : (cx & 1)) ? a : b;               /* herringbone */
    case 4: return (h & 7) == 0 ? lit(r, PAL_RAMP(RAMP_GRASS, 6)) : ((h >> 3) & 3) == 0 ? b : a;
    case 5: return (rr_hash(i, j) & 1) ? a : b;                                 /* flagstone */
    case 6: return (h % 11) == 0 ? lit(r, PAL_RAMP(RAMP_LAVA, 5)) : (h % 11) == 1 ? lit(r, PAL_RAMP(RAMP_LAVA, 3)) : b;
    default: return (h & 3) == 0 ? lit(r, PAL_PAINT(PAINT_WHITE, 0)) : a;       /* cloud */
    }
}

static void floor_tile(const RR *r, int i, int j, uint8_t a, uint8_t b, uint8_t grout)
{
    int cx = scr_x(r, i * 16, j * 16), cy = scr_y(r, i * 16, j * 16, 0), row;
    int pat = r->v->floor == 0xFF ? 0 : (r->v->floor & 7);
    int plank = pat == 1;
    for (row = 0; row < 16; row++) {
        int half = row < 8 ? 2 * (row + 1) : 2 * (16 - row);
        if (pat < 2) {
            uint8_t c = plank ? (((row >> 2) & 1) ? a : b) : (((i + j) & 1) ? a : b);
            plat_rect(cx - half, cy + row, 1, 1, grout);
            plat_rect(cx - half + 1, cy + row, half * 2 - 2, 1, c);
            plat_rect(cx + half - 1, cy + row, 1, 1, grout);
        } else {
            /* split the row where X or Y (tile units 0..16) crosses 4, 8, 12:
             * dx = 2X - 2row' and dx = 2row' - 2Y, with row' = row + 1/2 */
            int cut[8], nc = 0, k, x0 = -half + 1, x1 = half - 1;
            int grid = pat == 5 || pat == 2 ? 8 : 4;
            for (k = grid; k < 16; k += grid) {
                int d1 = 2 * k - 2 * row - 1, d2 = 2 * row + 1 - 2 * k;
                if (d1 > x0 && d1 < x1) cut[nc++] = d1;
                if (d2 > x0 && d2 < x1) cut[nc++] = d2;
            }
            for (k = 1; k < nc; k++) {               /* insertion sort, nc <= 6 */
                int v = cut[k], m = k - 1;
                while (m >= 0 && cut[m] > v) { cut[m + 1] = cut[m]; m--; }
                cut[m + 1] = v;
            }
            plat_rect(cx - half, cy + row, 1, 1, grout);
            for (k = 0; k <= nc; k++) {
                int s0 = k == 0 ? x0 : cut[k - 1], s1 = k == nc ? x1 : cut[k], mid = (s0 + s1) / 2;
                int X = (2 * row + 1 + mid) / 2, Y = (2 * row + 1 - mid) / 2;
                uint8_t c;
                if (s1 <= s0) continue;
                X = X < 0 ? 0 : X > 15 ? 15 : X;
                Y = Y < 0 ? 0 : Y > 15 ? 15 : Y;
                c = floor_cell(r, pat, i, j, X >> 2, Y >> 2, a, b);
                if (k > 0 && (pat == 2 || pat == 5 || pat == 3)) { plat_rect(cx + s0, cy + row, 1, 1, grout); s0++; }
                if (s1 > s0) plat_rect(cx + s0, cy + row, s1 - s0, 1, c);
            }
            plat_rect(cx + half - 1, cy + row, 1, 1, grout);
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
    if (pat == 0 && ((u >> 3) & 1)) fill = alt;                       /* stripes   */
    plat_rect(x, top, 2, 1, lit(r, PAL_RAMP(ramp, 1)));
    plat_rect(x, top + 1, 2, cor - 1, lit(r, ceil));
    if (pat == 5) {                                                   /* harlequin */
        /* diamonds 16 wide, 16 tall: the colour flips where |dv| crosses
         * the column's distance to the nearest diamond centre */
        int du = u & 15, d = du < 8 ? du : 16 - du, ph = (u >> 4) & 1;
        for (y = y0; y < y1; ) {
            int vv = (y - y0) & 15, dv = vv < 8 ? vv : 16 - vv, inside = d + dv < 8, len, ny;
            len = inside ? 8 - d - dv : d + dv - 7;
            if (len < 1) len = 1;
            ny = y + len < y1 ? y + len : y1;
            plat_rect(x, y, 2, ny - y, lit(r, (inside ^ ph ^ (((y - y0) >> 4) & 1)) ? alt : base));
            y = ny;
        }
    } else if (pat == 6) {                                            /* stone     */
        for (y = y0; y < y1; y += 8) {
            int row = (y - y0) >> 3, ny = y + 7 < y1 ? y + 7 : y1, uu = u + (row & 1) * 8;
            uint8_t c = (rr_hash(uu >> 4, row) & 1) ? base : alt;
            if ((uu & 15) < 2) c = palette_darker(PAL_PAINT(paint, 2));          /* the joint */
            plat_rect(x, y, 2, ny - y, lit(r, c));
            if (ny < y1) plat_rect(x, ny, 2, 1, lit(r, palette_darker(PAL_PAINT(paint, 2))));
        }
    } else {
        plat_rect(x, y0, 2, y1 - y0, lit(r, fill));
    }
    if (pat == 1) {                                                   /* dots      */
        for (y = top + cor + 4 + (((u >> 3) & 1) << 2); y < bottom - skirt - 2; y += 8)
            if ((u & 7) < 2) plat_rect(x, y, 2, 2, lit(r, alt));
    } else if (pat == 2) {                                            /* wainscot  */
        plat_rect(x, bottom - skirt - r->wall_h / 3, 2, r->wall_h / 3, lit(r, alt));
        plat_rect(x, bottom - skirt - r->wall_h / 3, 2, 1, lit(r, PAL_PAINT(paint, 0)));
    } else if (pat == 3 && (u & 15) == 0) {                           /* panels    */
        plat_rect(x, top + cor, 1, bottom - skirt - top - cor, lit(r, alt));
    } else if (pat == 4) {                                            /* hearts    */
        /* a 6 px heart = three 2 px columns: lobe, point, lobe */
        int c = (u >> 1) & 7, ph = ((u >> 4) & 1) * 7;
        static const int8_t h0[3] = { 0, 1, 0 }, h1[3] = { 3, 5, 3 };
        if (c >= 2 && c <= 4)
            for (y = y0 + 3 + ph; y + 5 < y1; y += 14)
                plat_rect(x, y + h0[c - 2], 2, h1[c - 2] - h0[c - 2], lit(r, PAL_PAINT(PAINT_CHERRY, side ? 1 : 0)));
    } else if (pat == 7) {                                            /* stars     */
        int c = u % 24;
        if (c == 8 || c == 10)
            for (y = y0 + 5 + (int)(rr_hash(u / 24, 3) % 9); y + 3 < y1; y += 17)
                plat_rect(x, y + (c == 8 ? 1 : 0), 2, c == 8 ? 1 : 3, lit(r, PAL_RAMP(RAMP_CANDY, 7)));
    }
    plat_rect(x, bottom - skirt, 2, skirt, lit(r, sk));
}

static void draw_room_shell(const RR *r)
{
    const DoomoRoomView *v = r->v;
    int i, j, u;
    int fp = v->floor_paint < 32 ? v->floor_paint : PAINT_CARAMEL;
    uint8_t a = lit(r, PAL_PAINT(fp, 1)), b = lit(r, PAL_PAINT(fp, 2)), g = lit(r, PAL_PAINT(fp, 0));
    if (v->room_kind == ROOMK_WALLS) {
        for (u = 0; u < r->W * 16; u += 2)                 /* wall A': y' = 0 */
            wall_column(r, r->ox + u, r->oy + (u >> 1), u, 0);
        for (u = 2; u <= r->D * 16; u += 2)                /* wall B': x' = 0 */
            wall_column(r, r->ox - u, r->oy + (u >> 1), u, 1);
        plat_rect(r->ox - 1, r->oy - r->wall_h, 2, r->wall_h, lit(r, PAL_PAINT(v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM, 0)));
    }
    for (j = 0; j < r->D; j++)
        for (i = 0; i < r->W; i++) floor_tile(r, i, j, a, b, g);
    if (v->room_kind != ROOMK_WALLS) {                     /* a lip for the plot edges */
        for (u = 0; u < r->W * 16; u += 2)
            plat_rect(r->ox + u - r->D * 16, r->oy + ((u + r->D * 16) >> 1) + 8, 2, 3, lit(r, PAL_RAMP(RAMP_PEACH, 2)));
    }
}

/* ---- pieces ---------------------------------------------------------------------- */
typedef struct Item {
    int8_t x0, y0, x1, y1;
    uint8_t layer, z;
    int16_t idx;          /* obj index, or -1 - actor */
} Item;
#define RR_MAX_ITEMS (HOME_OBJ_MAX + 32)   /* pieces, ghosts and actors */

static int behind(const Item *a, const Item *b)
{
    int ox = a->x0 < b->x1 && b->x0 < a->x1, oy = a->y0 < b->y1 && b->y0 < a->y1;
    if (ox && oy) return a->layer < b->layer || (a->layer == b->layer && a->z < b->z);
    if (a->x1 <= b->x0 && oy) return 1;
    if (a->y1 <= b->y0 && ox) return 1;
    if (a->x1 <= b->x0 && a->y1 <= b->y0) return 1;
    return 0;
}

/* the 8 recoloured indices: the two marker paints' shades and outlines */
static uint8_t recolour(uint8_t c, int pa, int pb)
{
    int s;
    if (pa >= 0 && pa < 32) {
        for (s = 0; s < 3; s++) if (c == PAL_PAINT(FURN_MARK_A, s)) return PAL_PAINT(pa, s);
        if (c == palette_darker(PAL_PAINT(FURN_MARK_A, 0))) return palette_darker(PAL_PAINT(pa, 0));
    }
    if (pb >= 0 && pb < 32) {
        for (s = 0; s < 3; s++) if (c == PAL_PAINT(FURN_MARK_B, s)) return PAL_PAINT(pb, s);
        if (c == palette_darker(PAL_PAINT(FURN_MARK_B, 0))) return palette_darker(PAL_PAINT(pb, 0));
    }
    return c;
}

/* A shaded box: the fallback when there is no sprite (the PS1, no cache). */
static void iso_box(const RR *r, int x0, int y0, int x1, int y1, int z, int h, uint8_t base)
{
    int uB = x0 - y0, uR = x1 - y0, uF = x1 - y1, uL = x0 - y1, u;
    uint8_t top = lit(r, palette_lighter(base)), left = lit(r, base), right = lit(r, palette_darker(base));
    for (u = uL; u < uR; u += 2) {
        int vtop = u < uB ? ((2 * x0 - u) >> 1) : ((u + 2 * y0) >> 1);
        int vfront = u < uF ? ((u + 2 * y1) >> 1) : ((2 * x1 - u) >> 1);
        int x = r->ox + u, ytop = r->oy + vtop - z - h, yf = r->oy + vfront - z - h;
        plat_rect(x, ytop, 2, yf - ytop, top);
        plat_rect(x, yf, 2, h, u < uF ? left : right);
        plat_rect(x, yf + h, 2, 1, lit(r, palette_darker(palette_darker(base))));
    }
}

/* ---- the maquette: a piece's prims as iso blocks (no sprites: the PS1) ---- */
static uint8_t prim_base(const DoomoFurn *f, const DoomoFurnPrim *p, int pa, int pb)
{
    int paint = p->mat == FM_A ? (pa >= 0 && pa < 32 ? pa : f->paint_a) :
                p->mat == FM_B ? (pb >= 0 && pb < 32 ? pb : f->paint_b) : -1;
    if (p->mat == FM_A || p->mat == FM_B) return PAL_PAINT(paint >= 0 && paint < 32 ? paint : PAINT_CARAMEL, 1);
    return p->mat;
}

/* the prim's box in units (z, h in px): boxes from their min corner, the
 * round ones from their centre and diameters (a cone: a narrower box) */
static void prim_extent(const DoomoFurnPrim *p, int *x0, int *y0, int *x1, int *y1, int *z, int *h)
{
    int k = p->kind & 3, rw = p->w, rd = k == FP_BLOB ? p->d : p->w;
    if (k == FP_BOX) { *x0 = p->x; *y0 = p->y; *x1 = p->x + p->w; *y1 = p->y + p->d; }
    else {
        if (k == FP_CONE) { rw = rw * 2 / 3; rd = rd * 2 / 3; }
        *x0 = p->x - rw / 2; *y0 = p->y - rd / 2; *x1 = *x0 + (rw ? rw : 1); *y1 = *y0 + (rd ? rd : 1);
    }
    *z = p->z;
    *h = p->h ? p->h : 1;
}

int room_ref_maquette(int furn, int pa, int pb, int x, int y, int w, int h)
{
    const DoomoFurn *f = home_furn(furn);
    const DoomoFurnPrim *p0;
    int i, j, n, sc, minx = 1 << 14, maxx = -(1 << 14), miny = 1 << 14, maxy = -(1 << 14);
    uint8_t ord[32];
    RR r;
    if (!f || !f->nprim) return 0;
    p0 = &doomo_furn_prims[f->prim0];
    n = f->nprim < 32 ? f->nprim : 32;
    /* the screen extent at 1x: u = X - Y, v = (X + Y) / 2 - Z */
    for (i = 0; i < n; i++) {
        int x0, y0, x1, y1, z, hh;
        prim_extent(&p0[i], &x0, &y0, &x1, &y1, &z, &hh);
        minx = DOOMO_MIN(minx, x0 - y1); maxx = DOOMO_MAX(maxx, x1 - y0);
        miny = DOOMO_MIN(miny, ((x0 + y0) >> 1) - z - hh); maxy = DOOMO_MAX(maxy, ((x1 + y1) >> 1) - z + 1);
    }
    sc = (maxx - minx) * 2 + 8 <= w && (maxy - miny) * 2 + 8 <= h ? 2 : 1;
    /* back to front: the far corner's x + y, then the bottom */
    for (i = 0; i < n; i++) ord[i] = (uint8_t)i;
    for (i = 1; i < n; i++) {
        uint8_t t = ord[i];
        const DoomoFurnPrim *a = &p0[t];
        for (j = i - 1; j >= 0; j--) {
            const DoomoFurnPrim *b = &p0[ord[j]];
            if (b->x + b->y < a->x + a->y || (b->x + b->y == a->x + a->y && b->z <= a->z)) break;
            ord[j + 1] = ord[j];
        }
        ord[j + 1] = t;
    }
    memset(&r, 0, sizeof(r));
    r.level = 15;
    r.ox = x + (w - (maxx - minx) * sc) / 2 - minx * sc;
    r.oy = y + (h - (maxy - miny) * sc) / 2 - miny * sc;
    for (i = 0; i < n; i++) {
        const DoomoFurnPrim *p = &p0[ord[i]];
        int x0, y0, x1, y1, z, hh;
        if (FURN_PRIM_FRAMES(p) && !(FURN_PRIM_FRAMES(p) & 1)) continue;     /* frame 0 only */
        prim_extent(p, &x0, &y0, &x1, &y1, &z, &hh);
        iso_box(&r, x0 * sc, y0 * sc, x1 * sc, y1 * sc, z * sc, hh * sc, prim_base(f, p, pa, pb));
    }
    return 1;
}

int room_ref_sprite(int furn, int facing, int frame, int pa, int pb, int lvl,
                    uint8_t *buf, unsigned cap, DoomoImage *img, int *ox, int *oy)
{
    const DoomoFurn *f = home_furn(furn);
    const DoomoFurnIso *im;
    const uint8_t *base;
    int frames, e, y;
    if (!f || !buf || !doomo_furn_iso_count || furn >= doomo_furn_count) return 0;
    frames = f->frames ? f->frames : 1;
    e = doomo_furn_iso_first[furn] + (frame % frames) * 2 + (facing & 1);
    if (e >= doomo_furn_iso_count) return 0;
    im = &doomo_furn_iso[e];
    if ((unsigned)(im->w * im->h) > cap || !im->w || !im->h) return 0;
    base = doomo_furn_iso_data + im->off;
    memset(buf, 0, (size_t)im->w * im->h);
    for (y = 0; y < im->h; y++) {
        const uint8_t *p = base + (base[y * 2] | (base[y * 2 + 1] << 8));
        int x = 0;
        while (*p != 0xFF) {
            int skip = p[0], len = p[1], i;
            p += 2;
            x += skip;
            for (i = 0; i < len && x + i < im->w; i++) {
                uint8_t c = recolour(p[i], pa, pb);
                buf[y * im->w + x + i] = lvl >= 15 ? c : palette_shade(c, lvl);
            }
            p += len;
            x += len;
        }
    }
    img->w = im->w; img->h = im->h; img->pix = buf;
    img->uid = 0xF0A00000u ^ ((uint32_t)furn << 6) ^ (uint32_t)(e << 1) ^ ((uint32_t)(pa & 31) << 20) ^
               ((uint32_t)(pb & 31) << 25) ^ ((uint32_t)lvl << 15);
    *ox = im->ox;
    *oy = im->oy;
    return 1;
}

/* after the decode: the ghost / selection / tint effects on the image's
 * own pixels (0 stays transparent) */
static void sprite_fx(const RR *r, uint8_t *pix, int w, int h, int flags)
{
    int x, y, ph = (r->v->time >> 1) & 1, blink = (flags & RO_SELECTED) && ((r->v->time >> 3) & 1);
    if (!(flags & (RO_GHOST | RO_SEMI)) && !blink && !r->tint) return;
    for (y = 0; y < h; y++) {
        uint8_t *p = pix + y * w;
        for (x = 0; x < w; x++) {
            uint8_t c = p[x];
            if (!c) continue;
            if ((flags & (RO_GHOST | RO_SEMI)) && ((x + y + ph) & 1)) { p[x] = 0; continue; }
            if ((flags & (RO_GHOST | RO_SEMI)) == (RO_GHOST | RO_SEMI)) {
                int l = palette_luma(c);                                  /* a grey outline */
                c = PAL_RAMP(RAMP_STONE, l > 200 ? 7 : l > 150 ? 6 : l > 100 ? 5 : l > 60 ? 4 : 3);
            } else if (flags & RO_INVALID) {
                c = palette_shade(c, 9);
            }
            if (blink) c = palette_lighter(c);
            if (r->tint) c = doomo_tintmap[r->tint][c];
            p[x] = c;
        }
    }
}

static void draw_piece(const RR *r, const DoomoRoomObj *o, const Item *it)
{
    const DoomoFurn *f = home_furn(o->furn);
    int rot = (RO_ROT(o) - r->k) & 3, facing = rot & 1, flip = rot >= 2;
    int sx = scr_x(r, it->x0 * 16, it->y0 * 16), sy = scr_y(r, it->x0 * 16, it->y0 * 16, o->z);
    DoomoImage img;
    int ox, oy, lvl = (o->flags & RO_FULLBRIGHT) ? 15 : r->level;
    int pa = o->paint_a == 0xFF ? -1 : o->paint_a, pb = o->paint_b == 0xFF ? -1 : o->paint_b;
    if (!f) return;
    if (o->flags & (RO_HALF_X | RO_HALF_Y)) {           /* the half-tile, turned with the view */
        int hx = (o->flags & RO_HALF_X) ? 8 : 0, hy = (o->flags & RO_HALF_Y) ? 8 : 0, k, t;
        for (k = 0; k < r->k; k++) { t = hx; hx = -hy; hy = t; }
        sx += hx - hy;
        sy += (hx + hy) >> 1;
    }
    if (o->jitter) sx += (int)((r->v->time >> 1) & 1) * o->jitter - o->jitter / 2;
    if (room_ref_sprite(o->furn, facing, RO_ANIM(o), pa, pb, lvl, (uint8_t *)r->v->plat_cache,
                        (unsigned)r->v->plat_cache_cap, &img, &ox, &oy)) {
        int flags = flip ? IMG_FLIPX : 0;
        uint8_t col = 0;
        sprite_fx(r, (uint8_t *)r->v->plat_cache, img.w, img.h, o->flags);
        img.uid ^= (uint32_t)(o->flags & 0x1B) << 27 ^ (uint32_t)r->tint << 9;
        if (o->sq_x || o->sq_y) {
            int ax = sx - (flip ? img.w - ox : ox) + img.w / 2, ay = sy - oy + img.h;
            plat_image_xform(ax, ay, &img, 256 + o->sq_x * 2, 256 + o->sq_y * 2, flags, col);
        } else {
            plat_image(sx - (flip ? img.w - ox : ox), sy - oy, &img, 1, flags, col);
        }
        return;
    }
    {
        int h = f->fh ? f->fh * 4 : 2, pa2 = o->paint_a < 32 ? o->paint_a : PAINT_CARAMEL;
        uint8_t base = (o->flags & RO_SEMI) ? PAL_RAMP(RAMP_STONE, 5) : PAL_PAINT(pa2, 1);
        iso_box(r, it->x0 * 16 + 1, it->y0 * 16 + 1, it->x1 * 16 - 1, it->y1 * 16 - 1, o->z, h, base);
    }
}

/* an actor (a chibi): feet on its tile point, squashed about the feet,
 * BB_TRANSLUCENT dithered through the cache */
static void draw_actor(const RR *r, const DoomoRoomActor *a)
{
    const DoomoRoomView *v = r->v;
    fix16 ax = a->x, ay = a->y;
    int k, t, u, vv, fl = a->flags & BB_FLIPX ? IMG_FLIPX : 0;
    DoomoImage img;
    if (!a->img) return;
    for (k = 0; k < r->k; k++) { t = ax; ax = INT2FIX(v->w) - ay; ay = t; }
    u = scr_x(r, FIX2INT(ax * 16), FIX2INT(ay * 16));
    vv = scr_y(r, FIX2INT(ax * 16), FIX2INT(ay * 16), a->z);
    img = *a->img;
    if (((a->flags & BB_TRANSLUCENT) || r->tint) && v->plat_cache &&
        (uint32_t)(img.w * img.h) <= v->plat_cache_cap && img.pix) {
        uint8_t *d = (uint8_t *)v->plat_cache;
        memcpy(d, img.pix, (size_t)img.w * img.h);
        sprite_fx(r, d, img.w, img.h, (a->flags & BB_TRANSLUCENT) ? RO_SEMI : 0);
        img.pix = d;
        img.uid ^= 0x5A000000u;
    }
    if (a->sx && a->sy && (a->sx != 256 || a->sy != 256))
        plat_image_xform(u, vv + 2, &img, a->sx, a->sy, fl, 0);
    else
        plat_image(u - img.w / 2, vv - img.h + 2, &img, 1, fl, 0);
}

static void draw_cursor(const RR *r, int cx, int cy)
{
    int x0 = cx, y0 = cy, x1 = cx + 1, y1 = cy + 1, bx, by;
    uint8_t c = ((r->v->time >> 3) & 1) ? PAL_WHITE : PAL_UI_GOLD;
    rot_box(r->k, r->v->w, r->v->d, &x0, &y0, &x1, &y1);
    bx = scr_x(r, x0 * 16, y0 * 16);
    by = scr_y(r, x0 * 16, y0 * 16, 0);
    plat_line(bx, by, bx + 16, by + 8, c);
    plat_line(bx + 16, by + 8, bx, by + 16, c);
    plat_line(bx, by + 16, bx - 16, by + 8, c);
    plat_line(bx - 16, by + 8, bx, by, c);
    plat_line(bx, by + 17, bx - 15, by + 9, PAL_UI_PINK_D);
    plat_line(bx + 1, by + 17, bx + 16, by + 9, PAL_UI_PINK_D);
}

void room_ref_draw(const DoomoRoomView *v)
{
    RR r;
    Item items[RR_MAX_ITEMS];
    uint8_t done[RR_MAX_ITEMS];
    int n = 0, i, j, total, vw, vh;
    if (!v || plat_draw_skipped()) return;
    r.v = v;
    r.k = ((v->yaw + 512) >> 10) & 3;
    r.W = (r.k & 1) ? v->d : v->w;
    r.D = (r.k & 1) ? v->w : v->d;
    vw = v->vw ? v->vw : plat_screen_w();
    vh = v->vh ? v->vh : plat_screen_h();
    r.wall_h = vh >= 200 ? 64 : 48;
    r.level = 6 + (v->daylight > 15 ? 15 : v->daylight) * 9 / 15;
    r.tint = (v->tint && v->tint < TINT_COUNT && v->tint_level >= 4) ? v->tint : 0;
    total = (v->room_kind == ROOMK_WALLS ? r.wall_h : 8) + (r.W + r.D) * 8;
    r.ox = v->vx + vw / 2 - (r.W - r.D) * 8 + v->pan_x;
    if (total + 4 <= vh) r.oy = v->vy + (vh - total) / 2 + (total - (r.W + r.D) * 8) + v->pan_y;
    else r.oy = v->vy + vh - (r.W + r.D) * 8 - 4 + v->pan_y;
    if (v->room_kind == ROOMK_WALLS) {
        plat_rect(v->vx, v->vy, vw, vh, r.tint ? lit(&r, PAL_RAMP(RAMP_VAPE, 1)) : PAL_RAMP(RAMP_VAPE, 1));
    } else {
        /* the open kinds' sky: noon, a peach dusk, a navy night */
        int dl = v->daylight > 15 ? 15 : v->daylight;
        uint8_t sky = dl >= 13 ? PAL_RAMP(RAMP_SKY, 5) : dl >= 9 ? PAL_RAMP(RAMP_PEACH, 5) : PAL_RAMP(RAMP_SKY, 1);
        plat_rect(v->vx, v->vy, vw, vh, lit(&r, sky));
        if (dl < 9) {                                      /* a few stars */
            for (i = 0; i < 12; i++) {
                uint32_t h = rr_hash(i, 77);
                plat_rect(v->vx + (int)(h % (uint32_t)vw), v->vy + (int)((h >> 12) % (uint32_t)(vh / 3 + 1)), 1, 1,
                          PAL_RAMP(RAMP_CANDY, 7));
            }
        }
    }
    draw_room_shell(&r);
    /* the rubber band of a multi-select (room-space bits y * w + x) */
    for (j = 0; j < v->d; j++)
        for (i = 0; i < v->w; i++) {
            int bit = j * v->w + i;
            if (bit < 128 && (v->sel_mask[bit >> 3] >> (bit & 7) & 1)) {
                int x0 = i, y0 = j, x1 = i + 1, y1 = j + 1, bx, by;
                rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
                bx = scr_x(&r, x0 * 16, y0 * 16);
                by = scr_y(&r, x0 * 16, y0 * 16, 0);
                plat_line(bx, by + 1, bx + 14, by + 8, PAL_UI_GOLD);
                plat_line(bx + 14, by + 8, bx, by + 15, PAL_UI_GOLD);
                plat_line(bx, by + 15, bx - 14, by + 8, PAL_UI_GOLD);
                plat_line(bx - 14, by + 8, bx, by + 1, PAL_UI_GOLD);
            }
        }
    /* wall pieces on the two visible walls */
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = home_furn(o->furn);
        int side, x0, y0, x1, y1, k;
        Item it;
        if (!f || !(f->flags & FF_WALL)) continue;
        /* the wall it hangs on, as a direction: A = (0,-1), B = (-1,0) */
        {
            int dx = (RO_ROT(o) & 1) ? -1 : 0, dy = (RO_ROT(o) & 1) ? 0 : -1;
            for (k = 0; k < r.k; k++) { int t = dx; dx = -dy; dy = t; }
            if (dx == 0 && dy == -1) side = 0; else if (dx == -1 && dy == 0) side = 1; else continue;
        }
        x0 = o->x; y0 = o->y;
        if (RO_ROT(o) & 1) { x1 = x0 + 1; y1 = y0 + (f->fw ? f->fw : 1); }
        else { x1 = x0 + (f->fw ? f->fw : 1); y1 = y0 + 1; }
        rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
        it.x0 = (int8_t)x0; it.y0 = (int8_t)y0; it.x1 = (int8_t)x1; it.y1 = (int8_t)y1;
        if (side == 0) it.y0 = 0; else it.x0 = 0;
        {
            DoomoRoomObj tmp = *o;
            tmp.rot_anim = RO_ROT_ANIM((side == 0 ? 0 : 1) + r.k, RO_ANIM(o));
            draw_piece(&r, &tmp, &it);
        }
    }
    /* rugs, then everything that stands */
    for (i = 0; i < v->nobj && n < RR_MAX_ITEMS; i++) {
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
    for (i = 0; i < v->nact && n < RR_MAX_ITEMS; i++) {
        int x0 = FIX2INT(v->actors[i].x), y0 = FIX2INT(v->actors[i].y), x1 = x0 + 1, y1 = y0 + 1;
        rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
        items[n].x0 = (int8_t)x0; items[n].y0 = (int8_t)y0; items[n].x1 = (int8_t)x1; items[n].y1 = (int8_t)y1;
        items[n].layer = 2;
        items[n].z = 0;
        items[n].idx = (int16_t)(-1 - i);
        n++;
    }
    memset(done, 0, sizeof(done));
    for (total = 0; total < n; total++) {
        int pick = -1, best = 0x7FFF;
        for (i = 0; i < n; i++) {
            int free_ = 1, key;
            if (done[i]) continue;
            for (j = 0; j < n && free_; j++)
                if (j != i && !done[j] && behind(&items[j], &items[i]) && !behind(&items[i], &items[j])) free_ = 0;
            key = (items[i].x0 + items[i].y0) * 8 + items[i].layer * 2 + (items[i].z ? 1 : 0);
            if (items[i].layer == 0) key -= 1000;      /* rugs under everything */
            if (free_ && key < best) { best = key; pick = i; }
        }
        if (pick < 0)                                   /* a cycle: plain depth order */
            for (i = 0; i < n; i++)
                if (!done[i]) {
                    int key = (items[i].x0 + items[i].y0) * 8 + items[i].layer * 2;
                    if (key < best) { best = key; pick = i; }
                }
        done[pick] = 1;
        if (items[pick].idx >= 0) {
            draw_piece(&r, &v->objs[items[pick].idx], &items[pick]);
        } else {
            draw_actor(&r, &v->actors[-1 - items[pick].idx]);
        }
    }
    if (v->cursor_x >= 0 && v->cursor_y >= 0) draw_cursor(&r, v->cursor_x, v->cursor_y);
}

__attribute__((weak)) void plat_room(const DoomoRoomView *v) { room_ref_draw(v); }
