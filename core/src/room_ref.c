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
 */
#include "home_int.h"

typedef struct RR {
    int ox, oy, W, D, k, wall_h, level;
    const DoomoRoomView *v;
} RR;

static uint8_t lit(const RR *r, uint8_t c)
{
    return r->level >= 15 ? c : palette_shade(c, r->level);
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
static void floor_tile(const RR *r, int i, int j, uint8_t a, uint8_t b, uint8_t grout)
{
    int cx = scr_x(r, i * 16, j * 16), cy = scr_y(r, i * 16, j * 16, 0), row;
    int plank = (r->v->floor & 3) == 1;
    for (row = 0; row < 16; row++) {
        int half = row < 8 ? 2 * (row + 1) : 2 * (16 - row);
        uint8_t c = plank ? (((row >> 2) & 1) ? a : b) : (((i + j) & 1) ? a : b);
        plat_rect(cx - half, cy + row, 1, 1, grout);
        plat_rect(cx - half + 1, cy + row, half * 2 - 2, 1, c);
        plat_rect(cx + half - 1, cy + row, 1, 1, grout);
    }
}

static void wall_column(const RR *r, int x, int bottom, int u, int side)
{
    const DoomoRoomView *v = r->v;
    int paint = v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM;
    int pat = v->wallpaper == 0xFF ? 0 : (v->wallpaper & 3);
    int top = bottom - r->wall_h, cor = 4, skirt = 4, y;
    uint8_t base = PAL_PAINT(paint, side ? 1 : 2), alt = PAL_PAINT(paint, side ? 0 : 1);
    uint8_t ceil = PAL_RAMP(RAMP_WOOD, side ? 3 : 4), sk = PAL_RAMP(RAMP_WOOD, side ? 2 : 3);
    uint8_t fill = base;
    if (pat == 0 && ((u >> 3) & 1)) fill = alt;                       /* stripes   */
    plat_rect(x, top, 2, 1, lit(r, PAL_RAMP(RAMP_WOOD, 1)));
    plat_rect(x, top + 1, 2, cor - 1, lit(r, ceil));
    plat_rect(x, top + cor, 2, bottom - skirt - top - cor, lit(r, fill));
    if (pat == 1) {                                                   /* dots      */
        for (y = top + cor + 4 + (((u >> 3) & 1) << 2); y < bottom - skirt - 2; y += 8)
            if ((u & 7) < 2) plat_rect(x, y, 2, 2, lit(r, alt));
    } else if (pat == 2) {                                            /* wainscot  */
        plat_rect(x, bottom - skirt - r->wall_h / 3, 2, r->wall_h / 3, lit(r, alt));
        plat_rect(x, bottom - skirt - r->wall_h / 3, 2, 1, lit(r, PAL_PAINT(paint, 0)));
    } else if (pat == 3 && (u & 15) == 0) {                           /* panels    */
        plat_rect(x, top + cor, 1, bottom - skirt - top - cor, lit(r, alt));
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
#define RR_MAX_ITEMS (HOME_OBJ_MAX + 8)

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

static void draw_piece(const RR *r, const DoomoRoomObj *o, const Item *it)
{
    const DoomoFurn *f = home_furn(o->furn);
    int rot = (RO_ROT(o) - r->k) & 3, facing = rot & 1, flip = rot >= 2;
    int sx = scr_x(r, it->x0 * 16, it->y0 * 16), sy = scr_y(r, it->x0 * 16, it->y0 * 16, o->z);
    DoomoImage img;
    int ox, oy, lvl = (o->flags & RO_FULLBRIGHT) ? 15 : r->level;
    int pa = o->paint_a == 0xFF ? -1 : o->paint_a, pb = o->paint_b == 0xFF ? -1 : o->paint_b;
    if (!f) return;
    if (o->jitter) sx += (int)((r->v->time >> 1) & 1) * o->jitter - o->jitter / 2;
    if (room_ref_sprite(o->furn, facing, RO_ANIM(o), pa, pb, lvl, (uint8_t *)r->v->plat_cache,
                        (unsigned)r->v->plat_cache_cap, &img, &ox, &oy)) {
        int flags = flip ? IMG_FLIPX : 0;
        uint8_t col = 0;
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
        iso_box(r, it->x0 * 16 + 1, it->y0 * 16 + 1, it->x1 * 16 - 1, it->y1 * 16 - 1, o->z, h,
                PAL_PAINT(pa2, 1));
    }
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
    total = (v->room_kind == ROOMK_WALLS ? r.wall_h : 8) + (r.W + r.D) * 8;
    r.ox = v->vx + vw / 2 - (r.W - r.D) * 8 + v->pan_x;
    if (total + 4 <= vh) r.oy = v->vy + (vh - total) / 2 + (total - (r.W + r.D) * 8) + v->pan_y;
    else r.oy = v->vy + vh - (r.W + r.D) * 8 - 4 + v->pan_y;
    plat_rect(v->vx, v->vy, vw, vh, v->room_kind == ROOMK_WALLS ? PAL_RAMP(RAMP_VAPE, 1) :
              lit(&r, PAL_RAMP(RAMP_SKY, 5)));
    draw_room_shell(&r);
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
            const DoomoRoomActor *a = &v->actors[-1 - items[pick].idx];
            fix16 ax = a->x, ay = a->y;
            int k, t, u, vv;
            for (k = 0; k < r.k; k++) { t = ax; ax = INT2FIX(v->w) - ay; ay = t; }
            u = scr_x(&r, FIX2INT(ax * 16), FIX2INT(ay * 16));
            vv = scr_y(&r, FIX2INT(ax * 16), FIX2INT(ay * 16), a->z);
            if (a->img) plat_image(u - a->img->w / 2, vv - a->img->h + 2, a->img, 1, a->flags & BB_FLIPX, 0);
        }
    }
    if (v->cursor_x >= 0 && v->cursor_y >= 0) draw_cursor(&r, v->cursor_x, v->cursor_y);
}

__attribute__((weak)) void plat_room(const DoomoRoomView *v) { room_ref_draw(v); }
