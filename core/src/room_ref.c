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
 *  - (H8) the light (home_int.h RoomLight): up to 4 lamp pools raise the
 *    level of each floor tile, each back-wall tile column (and the corner
 *    post) and each piece (at its view box's centre); glow pixels keep full
 *    light (a switched-off lamp's go dark), the lava crack's cracks glow,
 *    window panes and the roof's glass show the sky by daylight.
 */
#include "home_int.h"

typedef struct RR {
    int ox, oy, W, D, k, wall_h, level, tint;
    const DoomoRoomView *v;
    const struct RoomLight *L;        /* (H8) lamp pools, panes */
} RR;

static uint8_t lit(const RR *r, uint8_t c)
{
    c = r->level >= 15 ? c : palette_shade(c, r->level);
    return r->tint ? doomo_tintmap[r->tint][c] : c;
}

/* (H8) a glowing colour: full light, still tinted */
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

/* rotate a tile box (x0, y0, x1, y1 exclusive) by k quarter turns */
static void rot_box(int k, int w, int d, int *x0, int *y0, int *x1, int *y1)
{
    int i;
    for (i = 0; i < k; i++) {
        int nx0 = d - *y1, nx1 = d - *y0, ny0 = *x0, ny1 = *x1, t;   /* (H5) the turned width is d */
        *x0 = nx0; *x1 = nx1; *y0 = ny0; *y1 = ny1;
        t = w; w = d; d = t;
    }
}

/* ---- H8: the light (home_int.h) ------------------------------------------------- */
/* 1 / (1 + k / 16), minus its value at k = 64, as 0..128 (the PS1's table);
 * k = 32 d^2 / r^2, so a pool fades out at d = 1.41 r */
static const uint8_t k_lamp[64] = {
    128, 118, 110, 102, 96, 89, 84, 79, 74, 70, 66, 62, 59, 56, 53, 50, 48, 45, 43, 41, 39,
    37, 35, 33, 32, 30, 28, 27, 26, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 12,
    11, 10, 9, 9, 8, 8, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0 };

int room_light_at(const RoomLight *L, int X, int Y)
{
    int i, a = 0, lv;
    for (i = 0; i < L->n; i++) {
        int32_t dx = X - L->lx[i], dy = Y - L->ly[i];
        uint32_t kk = ((uint32_t)(dx * dx + dy * dy) * L->inv[i]) >> 16;
        if (kk < 64) a += k_lamp[kk];
    }
    if (!a) return L->level;
    lv = L->level + ((a * (15 - L->level) + 64) >> 7);
    return lv > 15 ? 15 : lv;
}

int room_pane_mode(int daylight)
{
    return daylight >= 13 ? RPANE_DAY : daylight >= 9 ? RPANE_DUSK : RPANE_NIGHT;
}

int room_light_lamps(RoomLight *L, const DoomoRoomView *v, int k, int level)
{
    int i;
    memset(L->lx, 0, sizeof(L->lx));
    memset(L->ly, 0, sizeof(L->ly));
    memset(L->inv, 0, sizeof(L->inv));
    L->n = 0;
    L->level = (uint8_t)level;
    L->pane = (uint8_t)room_pane_mode(v->daylight > 15 ? 15 : v->daylight);
    if (level >= 15) { L->corner = (uint8_t)level; return 0; }
    for (i = 0; i < v->nobj && L->n < ROOM_LAMPS; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = home_furn(o->furn);
        int x0, y0, x1, y1, w, d, r;
        if (!f || (o->flags & (RO_GHOST | RO_SEMI))) continue;
        if ((f->flags & FF_LIGHT) ? !(o->flags & RO_FULLBRIGHT) : (f->role != FR_HEARTH || !f->light_r)) continue;
        if (f->flags & FF_WALL) {
            w = (RO_ROT(o) & 1) ? 1 : (f->fw ? f->fw : 1);
            d = (RO_ROT(o) & 1) ? (f->fw ? f->fw : 1) : 1;
        } else {
            home_footprint(f, RO_ROT(o), &w, &d);
        }
        x0 = o->x; y0 = o->y; x1 = x0 + w; y1 = y0 + d;
        rot_box(k, v->w, v->d, &x0, &y0, &x1, &y1);
        r = 16 * (f->light_r ? f->light_r : 2);
        L->lx[L->n] = (int16_t)((x0 + x1) * 8);
        L->ly[L->n] = (int16_t)((y0 + y1) * 8);
        L->inv[L->n] = (uint16_t)((32u << 16) / (uint32_t)(r * r));
        L->n++;
    }
    if (!L->n) L->corner = (uint8_t)level;           /* else room_light_fill() sets it */
    return L->n;
}

void room_light_fill(RoomLight *L, int W, int D)
{
    int i, j;
    if (!L->n) return;
    if (W > ROOM_LIGHT_N) W = ROOM_LIGHT_N;
    if (D > ROOM_LIGHT_N) D = ROOM_LIGHT_N;
    for (j = 0; j < D; j++)
        for (i = 0; i < W; i++) L->tile[j][i] = (uint8_t)room_light_at(L, i * 16 + 8, j * 16 + 8);
    for (i = 0; i < W; i++) L->wall[0][i] = (uint8_t)room_light_at(L, i * 16 + 8, 0);
    for (j = 0; j < D; j++) L->wall[1][j] = (uint8_t)room_light_at(L, 0, j * 16 + 8);
    L->corner = (uint8_t)room_light_at(L, 0, 0);
}

int room_light_setup(RoomLight *L, const DoomoRoomView *v, int k, int W, int D, int level)
{
    if (room_light_lamps(L, v, k, level)) room_light_fill(L, W, D);
    return L->n;
}

/* the sky in a pane: the glass's luma picks the shade */
uint8_t room_pane_colour(uint8_t c, int pane)
{
    int l = palette_luma(c), t = l > 170 ? 3 : l > 120 ? 2 : l > 80 ? 1 : 0;
    if (pane == RPANE_DUSK) return PAL_RAMP(RAMP_PEACH, 4 + t);
    if (pane == RPANE_NIGHT) return PAL_RAMP(RAMP_SKY, t);
    return c;
}

int room_pane_star(int x, int y)
{
    return rr_hash(x + 31, y + 57) % 23u == 0;
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
    case 6: return (h % 11) == 0 ? glow(r, PAL_RAMP(RAMP_LAVA, 5)) : (h % 11) == 1 ? glow(r, PAL_RAMP(RAMP_LAVA, 3)) : b;  /* (H8) the cracks glow */
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

/* ---- H5: the plot edge, the door's backdrop, the wall pieces' edges ------------- */
static uint8_t lt(uint8_t c, int level, int tint)
{
    c = level >= 15 ? c : palette_shade(c, level);
    return tint ? doomo_tintmap[tint][c] : c;
}

void room_lip_colours(const DoomoRoomView *v, int level, int tint, uint8_t c[5])
{
    int fp = v->floor_paint < 32 ? v->floor_paint : PAINT_CARAMEL;
    c[0] = lt(PAL_PAINT(fp, 0), level, tint);
    c[1] = lt(PAL_RAMP(RAMP_PEACH, 2), level, tint);
    c[2] = lt(PAL_RAMP(RAMP_PEACH, 1), level, tint);
    c[3] = lt(PAL_RAMP(RAMP_PEACH, 1), level, tint);
    c[4] = lt(PAL_RAMP(RAMP_PEACH, 0), level, tint);
}

int room_wall_side(const DoomoRoomView *v, const DoomoRoomObj *o, int k)
{
    const DoomoFurn *f = home_furn(o->furn);
    int e = home_wall_edge(RO_ROT(o), o->x, o->y, v->w, v->d), dx, dy, i, side;
    static const int8_t ex[4] = { 0, -1, 0, 1 }, ey[4] = { -1, 0, 1, 0 };
    dx = ex[e]; dy = ey[e];
    for (i = 0; i < k; i++) { int t = dx; dx = -dy; dy = t; }
    side = dy == -1 ? 0 : dx == -1 ? 1 : dy == 1 ? 2 : 3;
    if (side >= 2 && (v->room_kind == ROOMK_WALLS || !f || (f->flags2 & FF_PORTAL))) return -1;
    return side;
}

int room_backdrop_setup(RoomBackdrop *b, const DoomoRoomView *v, int k, int W, int D,
                        int wall_h, int level, int tint)
{
    int i, s;
    memset(b, 0, sizeof(*b));
    if (!v || v->room_kind == ROOMK_WALLS) return RBD_NONE;
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = home_furn(o->furn);
        int x0, y0, x1, y1, a0, a1, L, side;
        if (!f || f->role != FR_DOOR || !(f->flags2 & FF_PORTAL) || (o->flags & (RO_GHOST | RO_SEMI))) continue;
        side = room_wall_side(v, o, k);
        if (side < 0 || side > 1) return RBD_NONE;       /* on the front: cut away */
        x0 = o->x; y0 = o->y;
        if (RO_ROT(o) & 1) { x1 = x0 + 1; y1 = y0 + (f->fw ? f->fw : 1); }
        else { x1 = x0 + (f->fw ? f->fw : 1); y1 = y0 + 1; }
        rot_box(k, v->w, v->d, &x0, &y0, &x1, &y1);
        a0 = side ? y0 : x0; a1 = side ? y1 : x1;
        L = 16 * (side ? D : W);
        b->side = (uint8_t)side;
        if (v->room_kind == ROOMK_GARDEN) {
            b->style = RBD_WALL;
            b->h = (uint8_t)wall_h;
            b->u0 = (int16_t)(a0 > 0 ? 16 * (a0 - 1) : 0);
            b->u1 = (int16_t)(16 * (a1 + 1) < L ? 16 * (a1 + 1) : L);
        } else {
            b->style = RBD_POSTS;
            b->h = (uint8_t)(wall_h * 3 / 4);
            b->p0 = (int16_t)(16 * a0 - 4);
            b->p1 = (int16_t)(16 * a1);
            b->u0 = (int16_t)(b->p0 > 0 ? b->p0 : 0);
            b->u1 = (int16_t)(b->p1 + 4 < L ? b->p1 + 4 : L);
        }
        if (b->u1 <= b->u0) { b->style = RBD_NONE; return RBD_NONE; }
        s = side;
        b->c[RBC_TRIM_D] = lt(PAL_RAMP(RAMP_WOOD, 1), level, tint);
        b->c[RBC_TRIM] = lt(PAL_RAMP(RAMP_WOOD, 4 - s), level, tint);
        b->c[RBC_SIDING] = lt(PAL_PAINT(PAINT_BONE, 2 - s), level, tint);
        b->c[RBC_SIDING_D] = lt(PAL_PAINT(PAINT_BONE, s ? 0 : 1), level, tint);
        b->c[RBC_CORNER] = lt(PAL_RAMP(RAMP_WOOD, 3 - s), level, tint);
        b->c[RBC_STONE] = lt(PAL_RAMP(RAMP_STONE, 4 - s), level, tint);
        b->c[RBC_STONE_D] = lt(PAL_RAMP(RAMP_STONE, 2 - s), level, tint);
        b->c[RBC_CAP] = lt(PAL_RAMP(RAMP_STONE, 6), level, tint);
        return b->style;
    }
    return RBD_NONE;
}

uint8_t room_backdrop_px(const RoomBackdrop *b, int u, int t)
{
    int h = b->h;
    if (u < b->u0 || u >= b->u1 || t < 0 || t >= h) return 0;
    if (b->style == RBD_WALL) {
        if (t == 0) return b->c[RBC_TRIM_D];
        if (t < 3) return b->c[RBC_TRIM];
        if (t == h - 1 || t == h - 6) return b->c[RBC_STONE_D];
        if (t > h - 6) return ((u + (t > h - 3 ? 4 : 0)) & 7) == 0 ? b->c[RBC_STONE_D] : b->c[RBC_STONE];
        if (u == b->u0 || u == b->u1 - 2) return b->c[RBC_CORNER];
        return (t - 3) % 5 == 4 ? b->c[RBC_SIDING_D] : b->c[RBC_SIDING];
    }
    if (b->style == RBD_POSTS) {
        int q = u >= b->p1 ? u - b->p1 : u - b->p0;
        if (q < 0 || q >= 4) return 0;
        if (t < 2) return b->c[RBC_CAP];
        if (t < 4) return b->c[RBC_STONE];
        if (t == h - 1) return b->c[RBC_STONE_D];
        return q < 2 ? b->c[RBC_STONE] : b->c[RBC_STONE_D];
    }
    return 0;
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
            if (c) plat_rect(x, top + t, 2, n, c);
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
    RR t = *r;                                             /* (H8) a tile's / column's light */
    if (bd->style) draw_backdrop(r, bd);
    if (v->room_kind == ROOMK_WALLS) {
        for (u = 0; u < r->W * 16; u += 2) {               /* wall A': y' = 0 */
            t.level = L->n ? L->wall[0][DOOMO_MIN(u >> 4, ROOM_LIGHT_N - 1)] : r->level;
            wall_column(&t, r->ox + u, r->oy + (u >> 1), u, 0);
        }
        for (u = 2; u <= r->D * 16; u += 2) {              /* wall B': x' = 0 */
            t.level = L->n ? L->wall[1][DOOMO_MIN((u - 2) >> 4, ROOM_LIGHT_N - 1)] : r->level;
            wall_column(&t, r->ox - u, r->oy + (u >> 1), u, 1);
        }
        t.level = L->corner;
        plat_rect(r->ox - 1, r->oy - r->wall_h, 2, r->wall_h, lit(&t, PAL_PAINT(v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM, 0)));
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
    if (v->room_kind != ROOMK_WALLS) {                     /* (H5) the lip on both front edges */
        uint8_t c[5];
        room_lip_colours(v, r->level, r->tint, c);
        for (u = 0; u < r->W * 16; u += 2) {
            int x = r->ox + u - r->D * 16, y = r->oy + ((u + r->D * 16) >> 1);
            plat_rect(x, y, 2, 1, c[0]);
            plat_rect(x, y + 1, 2, ROOM_LIP_H - 2, c[1]);
            plat_rect(x, y + ROOM_LIP_H - 1, 2, 1, c[3]);
        }
        for (u = 0; u < r->D * 16; u += 2) {
            int x = r->ox + r->W * 16 - u - 2, y = r->oy + ((u + r->W * 16) >> 1);
            plat_rect(x, y, 2, 1, c[0]);
            plat_rect(x, y + 1, 2, ROOM_LIP_H - 2, c[2]);
            plat_rect(x, y + ROOM_LIP_H - 1, 2, 1, c[4]);
        }
    }
}

/* ---- the roof (HART) -------------------------------------------------------------
 * The house's main roof seen behind the cut-away room: on each visible back
 * wall a slope rises away from the camera from the wall's top (the gutter),
 * in shingle courses, to a ridge cap; the two slopes meet in a valley over
 * the back corner, and the eaves overhang the walls' outer ends by RF_EAVE px
 * to a hip cap. A slope rising away at 45 degrees climbs straight up the
 * screen at the same 1:2 as the wall top, so everything is measured in rows
 * d above the wall top of the pixel's column (p: px from the corner along
 * the wall): the courses are rows of d, the shingle joints climb one pixel
 * sideways per two rows. A chimney stands over a hearth that backs onto a
 * visible wall (bricks, a cap), Towers 7B has a flat roof with a parapet and
 * a water tank, the Hell Pits a slate roof with bone spikes on the ridge,
 * the Roof Annex a skylight. Colours follow the ceiling ramp (the theme). */
#define RF_EAVE ROOM_ROOF_EAVE
static const uint8_t roof_paint[8] = { PAINT_CHERRY, PAINT_COBALT, PAINT_BUBBLEGUM, PAINT_TANGERINE,
                                       PAINT_GRAPE, PAINT_BONE, PAINT_CHARCOAL, PAINT_SILVER };

int home_room_roof(int room)
{
    static const uint8_t rr[HOME_ROOM_COUNT] = { ROOF_COTTAGE, ROOF_COTTAGE, ROOF_COTTAGE, ROOF_NONE,
                                                 ROOF_NONE, ROOF_ATTIC, ROOF_FLAT, ROOF_SPIKE, ROOF_NONE };
    return room >= 0 && room < HOME_ROOM_COUNT ? rr[room] : ROOF_NONE;
}

/* the wall top row of column p (px from the corner) on side `side` */
int room_roof_base(const RoomRoof *f, int side, int p)
{
    return f->top + (side ? (p ? (p + 2) >> 1 : 0) : p >> 1);
}

int room_roof_setup(RoomRoof *f, const DoomoRoomView *v, int k, int W, int D, int ox, int oy,
                    int wall_h, int level, int tint)
{
    int i, paint, sh, side = -1, u = 0, cw;
    uint8_t c[RFC_COUNT];
    memset(f, 0, sizeof(*f));
    if (!v || v->room_kind != ROOMK_WALLS || !v->roof || v->roof >= ROOF_COUNT) return 0;
    f->style = v->roof;
    f->ox = (int16_t)ox;
    f->top = (int16_t)(oy - wall_h);
    f->LA = (int16_t)(W * 16);
    f->LB = (int16_t)(D * 16);
    f->ch = (uint8_t)(wall_h >= 64 ? 4 : 3);
    f->band = (uint8_t)(4 + 6 * f->ch);
    paint = roof_paint[v->ceiling & 7];
    if (f->style == ROOF_SPIKE && !(v->ceiling & 7)) paint = PAINT_CHARCOAL;
    if (f->style == ROOF_FLAT) paint = PAINT_SILVER;
    /* the slopes: A (facing +y) a shade lighter than B (facing +x) */
    for (sh = 0; sh < 2; sh++) {
        uint8_t l = PAL_PAINT(paint, 2 - sh), m = PAL_PAINT(paint, 1 - sh);
        uint8_t d = sh ? palette_darker(PAL_PAINT(paint, 0)) : PAL_PAINT(paint, 0);
        c[RFC_A_D + sh * 4] = palette_darker(d);
        c[RFC_A_M + sh * 4] = m;
        c[RFC_A_L + sh * 4] = l;
        c[RFC_A_X + sh * 4] = d;
    }
    c[RFC_GUT_D] = PAL_RAMP(RAMP_STONE, 2);
    c[RFC_GUT_L] = PAL_PAINT(PAINT_BONE, 2);
    c[RFC_RIDGE_D] = palette_darker(PAL_PAINT(paint, 0));
    c[RFC_RIDGE_L] = PAL_PAINT(PAINT_BONE, 1);
    c[RFC_VALLEY] = PAL_RAMP(RAMP_STEEL, 3);
    c[RFC_HIP] = PAL_PAINT(PAINT_BONE, 1);
    c[RFC_F_L] = PAL_PAINT(PAINT_RUST, 2);
    c[RFC_F_D] = PAL_PAINT(PAINT_RUST, 0);
    c[RFC_F_M] = PAL_RAMP(RAMP_STONE, 6);
    c[RFC_F_CAP] = PAL_RAMP(RAMP_STONE, 5);
    c[RFC_GLASS] = PAL_RAMP(RAMP_SKY, 4);
    c[RFC_GLINT] = PAL_RAMP(RAMP_SKY, 7);
    switch (room_pane_mode(v->daylight > 15 ? 15 : v->daylight)) {   /* (H8) the sky in the glass */
    case RPANE_DUSK: c[RFC_GLASS] = PAL_RAMP(RAMP_PEACH, 4); c[RFC_GLINT] = PAL_RAMP(RAMP_PEACH, 6); break;
    case RPANE_NIGHT: c[RFC_GLASS] = PAL_RAMP(RAMP_SKY, 1); c[RFC_GLINT] = PAL_RAMP(RAMP_SKY, 3); break;
    default: break;
    }
    c[RFC_SPIKE_L] = PAL_PAINT(PAINT_BONE, 2);
    c[RFC_SPIKE_D] = PAL_PAINT(PAINT_BONE, 0);
    c[RFC_SIDING] = PAL_PAINT(PAINT_BONE, 2);
    c[RFC_SIDING_D] = PAL_PAINT(PAINT_BONE, 1);
    if (f->style == ROOF_SPIKE) {
        c[RFC_GUT_L] = PAL_PAINT(PAINT_BLOOD, 1);
        c[RFC_GUT_D] = PAL_PAINT(PAINT_BLOOD, 0);
        c[RFC_RIDGE_L] = PAL_PAINT(PAINT_BLOOD, 1);
        c[RFC_HIP] = PAL_PAINT(PAINT_BLOOD, 1);
        c[RFC_VALLEY] = PAL_RAMP(RAMP_LAVA, 4);
        c[RFC_F_L] = PAL_PAINT(PAINT_CHARCOAL, 1);
        c[RFC_F_D] = PAL_PAINT(PAINT_JET, 1);
        c[RFC_F_M] = PAL_PAINT(PAINT_HELLFIRE, 1);
        c[RFC_SIDING] = PAL_PAINT(PAINT_CHARCOAL, 1);
        c[RFC_SIDING_D] = PAL_PAINT(PAINT_CHARCOAL, 0);
        c[RFC_GLASS] = PAL_RAMP(RAMP_LAVA, 4);
        c[RFC_GLINT] = PAL_RAMP(RAMP_LAVA, 6);
    } else if (f->style == ROOF_FLAT) {
        c[RFC_GUT_L] = PAL_RAMP(RAMP_STONE, 6);
        c[RFC_RIDGE_L] = PAL_RAMP(RAMP_STONE, 6);
        c[RFC_F_L] = PAL_RAMP(RAMP_STEEL, 5);
        c[RFC_F_D] = PAL_RAMP(RAMP_STEEL, 3);
        c[RFC_F_M] = PAL_RAMP(RAMP_STEEL, 1);
        c[RFC_F_CAP] = PAL_RAMP(RAMP_STEEL, 6);
    }
    for (i = 0; i < RFC_COUNT; i++) {
        uint8_t x = level >= 15 ? c[i] : palette_shade(c[i], level);
        f->c[i] = tint ? doomo_tintmap[tint][x] : x;
    }
    /* the feature: a chimney over a hearth on a visible wall; the tower's tank */
    if (f->style == ROOF_FLAT) {
        side = 0;
        u = f->LA * 3 / 4;
    } else {
        for (i = 0; i < v->nobj; i++) {
            const DoomoRoomObj *o = &v->objs[i];
            const DoomoFurn *fu = home_furn(o->furn);
            int fw, fd, x0, y0, x1, y1;
            if (!fu || fu->role != FR_HEARTH || (o->flags & (RO_GHOST | RO_SEMI))) continue;
            home_footprint(fu, RO_ROT(o), &fw, &fd);
            x0 = o->x; y0 = o->y; x1 = x0 + fw; y1 = y0 + fd;
            rot_box(k, v->w, v->d, &x0, &y0, &x1, &y1);
            if (y0 == 0) { side = 0; u = (x0 + x1) * 8; }
            else if (x0 == 0) { side = 1; u = (y0 + y1) * 8; }
            break;
        }
    }
    if (side >= 0) {
        int b;
        f->fkind = (uint8_t)(f->style == ROOF_FLAT ? 2 : 1);
        cw = f->fkind == 2 ? 16 : 12;
        f->fx0 = (int16_t)(side ? ox - u - cw / 2 : ox + u - cw / 2);
        f->fx1 = (int16_t)(f->fx0 + cw);
        b = room_roof_base(f, side, u);
        f->ffoot = (uint8_t)(f->band / 2);
        f->ftop = (int16_t)(b - f->band - (f->fkind == 2 ? 6 : 3 * f->ch + 2));
    }
    /* a dormer gable on the longer wall, clear of the chimney */
    if (f->style == ROOF_COTTAGE || f->style == ROOF_SPIKE) {
        int L;
        f->dside = (uint8_t)(f->LB > f->LA);
        L = f->dside ? f->LB : f->LA;
        f->dw = (uint8_t)(f->ch * 4);
        f->dh = (uint8_t)(f->band - f->ch);
        f->dp = (int16_t)(L * 5 / 8);
        if (side == (int)f->dside && f->dp + f->dw + 8 > u - 8 && f->dp - f->dw - 8 < u + 8)
            f->dp = (int16_t)(u > L / 2 ? L * 5 / 16 : L * 13 / 16);
        if (f->dp - f->dw < 6 || f->dp + f->dw + 4 > L) f->dp = 0;
    }
    f->dmax = (uint8_t)(f->band + (f->style == ROOF_SPIKE ? 6 : 0));
    if (f->dp && f->dh + f->dw + 1 > f->dmax) f->dmax = (uint8_t)(f->dh + f->dw + 1);
    if (f->style == ROOF_ATTIC) {
        f->sky_side = (uint8_t)(f->LB > f->LA);
        u = (f->sky_side ? f->LB : f->LA) / 2;
        if (side == (int)f->sky_side && u > 32) u = u / 2;   /* clear of the chimney */
        f->sky0 = (int16_t)(u - 10);
        f->sky1 = (int16_t)(u + 10);
    }
    return 1;
}

void room_roof_rows(const RoomRoof *f, int *y0, int *y1)
{
    int a = f->top - f->band - f->dw - 8;
    if (f->fx1 > f->fx0 && f->ftop - 12 < a) a = f->ftop - 12;
    *y0 = a;
    *y1 = f->top + ((DOOMO_MAX(f->LA, f->LB) + RF_EAVE + 2) >> 1);
}

void room_roof_cols(const RoomRoof *f, int *x0, int *x1)
{
    *x0 = f->ox - f->LB - RF_EAVE;
    *x1 = f->ox + f->LA + RF_EAVE;
    if (f->fx1 > f->fx0) {                      /* the chimney's cap may stand out */
        if (f->fx0 - 1 < *x0) *x0 = f->fx0 - 1;
        if (f->fx1 + 1 > *x1) *x1 = f->fx1 + 1;
    }
}

/* The slope's repeating pattern: column pp = p & 15 (everything here
 * repeats every 16 px along the wall), d rows above the wall top. The GBA
 * caches it as a table (room_roof_pattern); room_roof_slope() adds what
 * does not repeat (the valley, the hip, the dormer, the skylight, the ends
 * of the spike row) in the ranges room_roof_specials() lists. */
uint8_t room_roof_pattern(const RoomRoof *f, int side, int pp, int d)
{
    const int band = f->band, s4 = side * 4;
    if (d < 1) return 0;
    if (d > band) {
        /* the Hell Pits' bone spikes along the ridge, one per 16 px */
        if (f->style == ROOF_SPIKE && d <= band + 6) {
            int m = ((pp + 6) & 15) - 6, hw = (band + 6 - d) >> 1;
            if (m >= -hw && m <= hw) return f->c[m < 0 || (m == 0 && side) ? RFC_SPIKE_D : RFC_SPIKE_L];
        }
        return 0;
    }
    if (f->style == ROOF_FLAT) {
        if (d == 1) return f->c[RFC_GUT_D];
        if (d <= 4) return f->c[(pp == 15 ? RFC_A_D : RFC_A_M) + s4];   /* the parapet */
        if (d == 5) return f->c[RFC_GUT_L];
        if (d == 6) return f->c[RFC_A_D + s4];
        if (d == band) return f->c[RFC_RIDGE_L];
        if (((pp * 7 + d * 13) & 15) == 0) return f->c[RFC_A_L + s4];   /* gravel */
        if (((pp * 5 + d * 11) & 31) == 7) return f->c[RFC_A_D + s4];
        return f->c[RFC_A_M + s4];
    }
    if (d == 1) return f->c[RFC_GUT_D];
    if (d == 2) return f->c[RFC_GUT_L];
    if (d >= band - 1) return f->c[d == band ? RFC_RIDGE_L : RFC_RIDGE_D];
    {
        /* shingles: course cc, row r above its butt edge; joints every 8 px,
         * half a tab staggered per course, climbing with d; every other tab
         * of every other pair of courses a shade darker */
        int e = d - 3, cc = f->ch == 4 ? e >> 2 : (e * 11) >> 5, r = e - cc * f->ch;
        int q = pp - (d >> 1) + (cc & 1) * 4 + 64;
        if (r == 0 || (q & 7) == 0) return f->c[RFC_A_D + s4];
        if (((q >> 3) & 1) && ((cc + 1) & 2)) return f->c[RFC_A_X + s4];
        return f->c[(r == f->ch - 1 ? RFC_A_L : RFC_A_M) + s4];
    }
}

/* The p ranges [r[2i], r[2i + 1]) of a side where the pattern does not
 * hold (sorted, may overlap); returns how many. */
int room_roof_specials(const RoomRoof *f, int side, int16_t r[8])
{
    int L = side ? f->LB : f->LA, n = 0;
    r[n++] = 0; r[n++] = 6;                                  /* valley, spike row start */
    if (f->dp && side == f->dside) { r[n++] = (int16_t)(f->dp - f->dw - 2); r[n++] = (int16_t)(f->dp + f->dw + 3); }
    if (f->style == ROOF_ATTIC && side == f->sky_side) { r[n++] = f->sky0; r[n++] = f->sky1; }
    r[n++] = (int16_t)(L - 2); r[n++] = (int16_t)(L + RF_EAVE);   /* spike row end, hip */
    return n / 2;
}

/* the colour of slope pixel (side, p) d rows above its wall top, 0 = none */
uint8_t room_roof_slope(const RoomRoof *f, int side, int p, int d)
{
    const int band = f->band, L = side ? f->LB : f->LA;
    if (f->dp && side == f->dside && d >= 3) {
        /* the dormer gable: a little wall face in the back wall's plane (so
         * a triangle in (p, d)), a window, its roof's edge on top */
        int a = p - f->dp, m = a < 0 ? -a : a, dt = f->dh + f->dw - m;
        if (m <= f->dw + 2 && d <= dt + 1 && d >= dt - 2)
            return f->c[d == dt + 1 ? RFC_RIDGE_L : d == dt - 2 ? RFC_RIDGE_D :
                        ((m + d) & 1) ? RFC_A_M + side * 4 : RFC_A_D + side * 4];
        if (m < f->dw && d < dt - 2) {
            int wy = d - 4, wh = f->ch * 3, ww = f->ch + 1;
            if (d == 3) return f->c[RFC_SIDING_D];
            if (m <= ww && wy >= 1 && wy <= wh) {
                if (m == ww || wy == 1 || wy == wh || a == 0 || wy == (wh + 1) >> 1) return f->c[RFC_HIP];
                return f->c[(wy == 2 && a < 0) ? RFC_GLINT : RFC_GLASS];
            }
            return f->c[(d & 1) ? RFC_SIDING : RFC_SIDING_D];
        }
    }
    if (p >= L + RF_EAVE || d < 1) return 0;
    if (d > band) return (p >= 6 && p < L - 2) ? room_roof_pattern(f, side, p & 15, d) : 0;
    if (f->style == ROOF_FLAT) {
        if (p >= L + RF_EAVE - 1) return f->c[RFC_GUT_D];
    } else if (d >= 3) {
        if (p >= L + RF_EAVE - 2) return f->c[(p == L + RF_EAVE - 1) ? RFC_GUT_D : RFC_HIP];
        if (d < band - 1 && p == 0) return f->c[RFC_VALLEY];
        if (f->style == ROOF_ATTIC && side == f->sky_side && p >= f->sky0 && p < f->sky1 && d >= 5 && d <= band - 3) {
            if (p < f->sky0 + 2 || p >= f->sky1 - 2 || d == 5 || d == band - 3) return f->c[RFC_HIP];
            return f->c[((p + 64 - (d >> 1) * 3) & 7) == 0 ? RFC_GLINT : RFC_GLASS];
        }
    }
    return room_roof_pattern(f, side, p & 15, d);
}

/* the chimney / tank (in front of the slope, the antenna above) on row y:
 * the row's constants first (the GBA calls this for every roof row) */
int room_roof_feature_row(const RoomRoof *f, int y, int x0, int x1, uint8_t *out)
{
    int x, n = 0, w = f->fx1 - f->fx0, r = y - f->ftop, w35 = w * 3 / 5, rb = r - 2;
    uint8_t lit_c, dark_c, mort = f->c[RFC_F_M];
    if (f->fx1 <= f->fx0 || y < f->ftop - 12) return 0;
    /* the row's two faces: the tank's (cap, hoop or body), the chimney's
     * (cap or bricks: 4 rows a course, the lit face's joints staggered) */
    if (f->fkind == 2) {
        lit_c = r < 3 ? f->c[RFC_F_CAP] : ((r - 3) & 7) == 6 ? mort : f->c[RFC_F_L];
        dark_c = r < 3 ? f->c[RFC_F_CAP] : ((r - 3) & 7) == 6 ? mort : f->c[RFC_F_D];
    } else {
        lit_c = r < 2 ? f->c[RFC_F_CAP] : (rb & 3) == 3 ? mort : f->c[RFC_F_L];
        dark_c = r < 2 ? f->c[RFC_F_CAP] : (rb & 3) == 3 ? mort : f->c[RFC_F_D];
    }
    for (x = DOOMO_MAX(x0, f->fx0 - 1); x <= f->fx1 && x < x1; x++) {
        int sd = x < f->ox, p = sd ? f->ox - 1 - x : x - f->ox, fx = x - f->fx0;
        int base = f->top + (sd ? (p ? (p + 2) >> 1 : 0) : p >> 1);
        uint8_t c = 0;
        if (y >= f->ftop && y < base - f->ffoot) {
            if (fx >= 0 && fx < w) {
                if (f->fkind == 2) {
                    if (r < 3) c = (fx == 0 || fx == w - 1) ? 0 : lit_c;
                    else c = fx < w / 2 ? lit_c : fx < w - 2 ? dark_c : mort;
                } else if (r < 2) {
                    c = lit_c;
                } else if (fx >= w35) {
                    c = dark_c;
                } else {
                    c = ((fx + ((rb >> 2) & 1) * 2) & 3) == 3 ? mort : lit_c;
                }
            } else if (f->fkind == 1 && y < f->ftop + 2) {
                c = f->c[RFC_F_CAP];
            }
        } else if (f->fkind == 2 && y < f->ftop) {
            if (fx == 3 || ((y == f->ftop - 9 || y == f->ftop - 5) && fx >= 1 && fx <= 5)) c = mort;
        }
        if (c) { n += !out[x - x0]; out[x - x0] = c; }
    }
    return n;
}

/* Row y, [x0, x1): only the columns whose rows above the wall top can reach
 * y are looked at (two spans of 2 * dmax px), then the feature on top. */
int room_roof_row(const RoomRoof *f, int y, int x0, int x1, uint8_t *out)
{
    int x, n = 0, side, rel = y - f->top;
    memset(out, 0, (size_t)(x1 - x0));
    for (side = 0; side < 2; side++) {
        int L = side ? f->LB : f->LA, p0 = 2 * rel - 2, p1 = 2 * (rel + f->dmax) + 2, p;
        if (p0 < 0) p0 = 0;
        if (p1 > L + RF_EAVE) p1 = L + RF_EAVE;
        for (p = p0; p < p1; p++) {
            uint8_t c;
            x = side ? f->ox - 1 - p : f->ox + p;
            if (x < x0 || x >= x1) continue;
            c = room_roof_slope(f, side, p, room_roof_base(f, side, p) - y);
            if (c) { out[x - x0] = c; n++; }
        }
    }
    return n + room_roof_feature_row(f, y, x0, x1, out);
}

static void draw_roof(const RR *r)
{
    RoomRoof f;
    uint8_t row[512];
    int y, y0, y1, x0, x1;
    if (!room_roof_setup(&f, r->v, r->k, r->W, r->D, r->ox, r->oy, r->wall_h, r->level, r->tint)) return;
    room_roof_rows(&f, &y0, &y1);
    room_roof_cols(&f, &x0, &x1);
    if (x0 < 0) x0 = 0;
    if (x1 > plat_screen_w()) x1 = plat_screen_w();
    if (y0 < 0) y0 = 0;
    if (y1 > plat_screen_h()) y1 = plat_screen_h();
    if (x1 - x0 > (int)sizeof(row)) x1 = x0 + (int)sizeof(row);
    for (y = y0; y < y1; y++) {
        int x = x0;
        if (!room_roof_row(&f, y, x0, x1, row)) continue;
        while (x < x1) {                              /* runs of one colour */
            int e = x + 1;
            uint8_t c = row[x - x0];
            while (e < x1 && row[e - x0] == c) e++;
            if (c) plat_rect(x, y, e - x, 1, c);
            x = e;
        }
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
    int cls, m = furn_prim_mat(p, &cls);               /* (H8) the light class rides in mat */
    int paint = m == FM_A ? (pa >= 0 && pa < 32 ? pa : f->paint_a) :
                m == FM_B ? (pb >= 0 && pb < 32 ? pb : f->paint_b) : -1;
    if (m == FM_A || m == FM_B) return PAL_PAINT(paint >= 0 && paint < 32 ? paint : PAINT_CARAMEL, 1);
    return (uint8_t)m;
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

/* (H6) The PS1 links no iso sprites (SPEC C3: doomo_furn_iso_count is 0
 * there) and its callers fall back to room_ref_maquette(). A constant 0
 * lets the compiler drop the dead decoder from the PS1 house group (1 KB
 * of its RAM slot). */
#ifdef DOOMO_PLATFORM_PS1
#define FURN_ISO_COUNT 0
#else
#define FURN_ISO_COUNT doomo_furn_iso_count
#endif

int room_ref_sprite(int furn, int facing, int frame, int pa, int pb, int lvl,
                    uint8_t *buf, unsigned cap, DoomoImage *img, int *ox, int *oy)
{
    const DoomoFurn *f = home_furn(furn);
    const DoomoFurnIso *im;
    const uint8_t *base, *cl;
    uint8_t raw[256];
    int frames, e, y, fl = lvl & ~15, pane = (fl & RR_SPR_NIGHT) ? RPANE_NIGHT : (fl & RR_SPR_DUSK) ? RPANE_DUSK : RPANE_DAY;
    lvl &= 15;
    if (!f || !buf || !FURN_ISO_COUNT || furn >= doomo_furn_count) return 0;
    frames = f->frames ? f->frames : 1;
    e = doomo_furn_iso_first[furn] + (frame % frames) * 2 + (facing & 1);
    if (e >= doomo_furn_iso_count) return 0;
    im = &doomo_furn_iso[e];
    if ((unsigned)(im->w * im->h) > cap || !im->w || !im->h) return 0;
    base = doomo_furn_iso_data + im->off;
    cl = base + im->h * 2;                               /* (H8) the light classes */
    memset(buf, 0, (size_t)im->w * im->h);
    for (y = 0; y < im->h; y++) {
        const uint8_t *p = base + (base[y * 2] | (base[y * 2 + 1] << 8));
        uint8_t *d = buf + y * im->w;
        int x = 0;
        while (*p != 0xFF) {
            int skip = p[0], len = p[1], i;
            p += 2;
            x += skip;
            for (i = 0; i < len && x + i < im->w; i++) {
                uint8_t c = recolour(p[i], pa, pb);
                raw[x + i] = c;
                d[x + i] = lvl >= 15 ? c : palette_shade(c, lvl);
            }
            p += len;
            x += len;
        }
        /* glow spans at full light (not a switched-off lamp's), panes the sky */
        for (; *cl != FURN_CLASS_END && *cl == y; cl += 4) {
            int i, x0 = cl[1], n = DOOMO_MIN(cl[2], im->w - x0);
            for (i = 0; i < n; i++) {
                uint8_t c = raw[x0 + i];
                if (cl[3] == FPL_GLOW) {
                    if (!(fl & RR_SPR_DARK)) d[x0 + i] = c;
                } else if (pane != RPANE_DAY) {
                    if (pane == RPANE_NIGHT && room_pane_star(x0 + i, y)) d[x0 + i] = ROOM_STAR_COLOUR;
                    else {
                        c = room_pane_colour(c, pane);
                        d[x0 + i] = lvl >= 15 ? c : palette_shade(c, lvl);
                    }
                }
            }
        }
    }
    img->w = im->w; img->h = im->h; img->pix = buf;
    img->uid = 0xF0A00000u ^ ((uint32_t)furn << 6) ^ (uint32_t)(e << 1) ^ ((uint32_t)(pa & 31) << 20) ^
               ((uint32_t)(pb & 31) << 25) ^ ((uint32_t)(lvl | fl) << 15);
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
    int ox, oy, lvl, fl;
    int pa = o->paint_a == 0xFF ? -1 : o->paint_a, pb = o->paint_b == 0xFF ? -1 : o->paint_b;
    if (!f) return;
    /* (H8) a lit lamp at full light; anything else in the lamps' pools;
     * a switched-off lamp's glow goes dark; the panes take the sky */
    lvl = (o->flags & RO_FULLBRIGHT) ? 15 : room_light_at(r->L, (it->x0 + it->x1) * 8, (it->y0 + it->y1) * 8);
    fl = ((f->flags & FF_LIGHT) && !(o->flags & RO_FULLBRIGHT) ? RR_SPR_DARK : 0) |
         (r->L->pane == RPANE_NIGHT ? RR_SPR_NIGHT : r->L->pane == RPANE_DUSK ? RR_SPR_DUSK : 0);
    if (o->flags & (RO_HALF_X | RO_HALF_Y)) {           /* the half-tile, turned with the view */
        int hx = (o->flags & RO_HALF_X) ? 8 : 0, hy = (o->flags & RO_HALF_Y) ? 8 : 0, k, t;
        for (k = 0; k < r->k; k++) { t = hx; hx = -hy; hy = t; }
        sx += hx - hy;
        sy += (hx + hy) >> 1;
    }
    if (o->jitter) sx += (int)((r->v->time >> 1) & 1) * o->jitter - o->jitter / 2;
    if (room_ref_sprite(o->furn, facing, RO_ANIM(o), pa, pb, lvl | fl, (uint8_t *)r->v->plat_cache,
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
        RR t = *r;
        t.level = lvl;
        iso_box(&t, it->x0 * 16 + 1, it->y0 * 16 + 1, it->x1 * 16 - 1, it->y1 * 16 - 1, o->z, h, base);
    }
}

/* an actor (a chibi): feet on its tile point, squashed about the feet,
 * BB_TRANSLUCENT dithered through the cache */
static void draw_actor(const RR *r, const DoomoRoomActor *a)
{
    const DoomoRoomView *v = r->v;
    fix16 ax = a->x, ay = a->y;
    int k, t, u, vv, w, d, fl = a->flags & BB_FLIPX ? IMG_FLIPX : 0;
    DoomoImage img;
    if (!a->img) return;
    for (k = 0, w = v->w, d = v->d; k < r->k; k++) { t = ax; ax = INT2FIX(d) - ay; ay = t; t = w; w = d; d = t; }
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
    Item items[RR_MAX_ITEMS], front[RR_MAX_ITEMS];
    uint8_t done[RR_MAX_ITEMS];
    int16_t front_idx[RR_MAX_ITEMS];
    int n = 0, nf = 0, i, j, total, vw, vh;
    RoomBackdrop bd;
    RoomLight L;
    if (!v || plat_draw_skipped()) return;
    r.v = v;
    r.L = &L;
    r.k = ((v->yaw + 512) >> 10) & 3;
    r.W = (r.k & 1) ? v->d : v->w;
    r.D = (r.k & 1) ? v->w : v->d;
    vw = v->vw ? v->vw : plat_screen_w();
    vh = v->vh ? v->vh : plat_screen_h();
    r.wall_h = vh >= 200 ? 64 : 48;
    r.level = 6 + (v->daylight > 15 ? 15 : v->daylight) * 9 / 15;
    r.tint = (v->tint && v->tint < TINT_COUNT && v->tint_level >= 4) ? v->tint : 0;
    room_backdrop_setup(&bd, v, r.k, r.W, r.D, r.wall_h, r.level, r.tint);
    room_light_setup(&L, v, r.k, r.W, r.D, r.level);
    total = (v->room_kind == ROOMK_WALLS || bd.style ? r.wall_h : 8) + (r.W + r.D) * 8;
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
    if (v->room_kind == ROOMK_WALLS && v->roof) draw_roof(&r);
    draw_room_shell(&r, &bd);
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
    /* wall pieces on the two visible walls; (H5) an open kind's front-edge
     * pieces stand on the lines y' = D / x' = W, drawn after everything */
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = home_furn(o->furn);
        int side, x0, y0, x1, y1;
        Item it;
        if (!f || !(f->flags & FF_WALL)) continue;
        side = room_wall_side(v, o, r.k);
        if (side < 0) continue;
        x0 = o->x; y0 = o->y;
        if (RO_ROT(o) & 1) { x1 = x0 + 1; y1 = y0 + (f->fw ? f->fw : 1); }
        else { x1 = x0 + (f->fw ? f->fw : 1); y1 = y0 + 1; }
        rot_box(r.k, v->w, v->d, &x0, &y0, &x1, &y1);
        it.x0 = (int8_t)x0; it.y0 = (int8_t)y0; it.x1 = (int8_t)x1; it.y1 = (int8_t)y1;
        if (side == 0) it.y0 = 0;
        else if (side == 1) it.x0 = 0;
        else if (side == 2) it.y0 = (int8_t)r.D;
        else it.x0 = (int8_t)r.W;
        if (side >= 2) {
            if (nf >= RR_MAX_ITEMS) continue;
            it.layer = (uint8_t)(side & 1);
            front[nf] = it;
            front_idx[nf++] = (int16_t)i;
            continue;
        }
        {
            DoomoRoomObj tmp = *o;
            tmp.rot_anim = RO_ROT_ANIM(side + r.k, RO_ANIM(o));
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
    for (i = 0; i < nf; i++) {                          /* (H5) the front edges, in object order */
        DoomoRoomObj tmp = v->objs[front_idx[i]];
        tmp.rot_anim = RO_ROT_ANIM(front[i].layer + r.k, RO_ANIM(&v->objs[front_idx[i]]));
        draw_piece(&r, &tmp, &front[i]);
    }
    if (v->cursor_x >= 0 && v->cursor_y >= 0) draw_cursor(&r, v->cursor_x, v->cursor_y);
}

__attribute__((weak)) void plat_room(const DoomoRoomView *v) { room_ref_draw(v); }
