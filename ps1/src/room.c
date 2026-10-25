/* room.c - the dream house on the PS1: plat_room() as a GTE diorama
 * (owner: H3; docs/design/story/SPEC.md sec.7 H3, C3, C9, C17;
 * docs/design/story/housing_final.md sec.7).
 *
 * The same DoomoRoomView the GBA draws as a static iso bitmap is built here
 * as a small 3D set, seen through a long lens:
 *
 *  - scale: a tile is RT = 32 world units and one unit of the iso art (1/16
 *    tile, one pixel of height) is RU = 2, so walls are 128 units tall (the
 *    room_ref 64 px wall of a 320x240 view) and the furniture keeps the iso
 *    sprites' proportions next to the 3D chibi, whom model.c builds 56 units
 *    tall. (HF said 64 units per tile; at that scale the chibi would stand
 *    shorter than a picket fence.)
 *  - the room box: the floor is one textured quad per tile (8bpp, 16 texels
 *    per tile), the four walls are slabs (inner face textured per tile
 *    column, skirting and cornice bands, a cap and an outer face). The two
 *    walls whose outward normal faces the camera are cut down to 12-unit
 *    stubs; when an orbit carries the camera across a quadrant the swap is a
 *    4-frame drop-and-pop (the old pair falls, bounces, the new pair rises
 *    with an overshoot).
 *  - recolours: the floor and wallpaper texels are the marker paints
 *    (PAINT_MINT for the floor, PAINT_BUBBLEGUM for the walls); one 256-entry
 *    CLUT row borrowed from the silhouette rows (y 481..495, vram_sil_clut's
 *    key 0: a transparent silhouette nobody asks for) maps them to the
 *    room's paints. 512 B are uploaded when the paints change or the row
 *    was taken back; nothing per pixel.
 *  - furniture: each piece's DoomoFurnPrim list (boxes, blobs, cylinders,
 *    cones) as flat / gouraud polygons, rotated, squashed (sq_x about the
 *    footprint centre, sq_y up from its base), jittered (possessed: +-1-2
 *    units at 15 Hz and a red emissive), lit by a key light that turns with
 *    the camera (the iso art's top-light / +y mid / +x dark, whatever the
 *    view) and the day colour; RO_FULLBRIGHT pieces ignore the dusk. The PS1
 *    links no baked iso sprites (SPEC C3), so FF_BILLBOARD pieces are drawn
 *    from their prims too. Round prims get 8 / 6 / 4 sides by their radius
 *    on screen (a 6x6 room keeps 8 from 5 iso units; an 8x8 room is drawn
 *    smaller), and faces that look down (the camera never goes below the
 *    floor) are not even tested. Ghosts are 50% translucent (RO_INVALID
 *    red, RO_SEMI grey), RO_SELECTED blinks lighter.
 *  - characters: the player (RA_PLAYER) is model.c's 3D chibi, facing where
 *    it walks; every other actor is a camera-facing billboard of its image
 *    with its own x / y squash, standing on its feet.
 *  - light: ambient = the overworld's day colour, up to 4 lamp pools (lit
 *    FF_LIGHT pieces, 1 / (1 + d^2/r^2) from a 64-entry table) on the floor
 *    vertices, darker floor along the wall seams, blob contact shadows under
 *    pieces and actors; the hell tint is the overworld's full-view TILE.
 *  - camera: pitch 35 degrees down, yaw 45 + 90k degrees on a TW_BOING
 *    spring (any other yaw the core sends - L2/R2 free orbit - is followed
 *    the same way), the room fitted to the viewport from the target view,
 *    zoom 1 = a 1.6x close-up on the player, punch_q8 and the S1 GTE zoom
 *    multiply the projection distance, pan_x / pan_y shift the projection
 *    centre (the brake shake).
 *
 * THE BACKDROP (the PS1's answer to the GBA's static bitmap; it replaces
 * SPEC C17's vertex cache, which still re-emitted every polygon): while the
 * camera and the furniture rest, nothing but the actors, animated pieces,
 * ghosts and marks changes, so the static layer - background, floor, walls,
 * every resting piece and their contact shadows - is drawn ONCE, off
 * screen, into the first three world-texture pages (x 320.., y 0..255;
 * vram_wtex_lend(): the house uses no world textures), and every frame
 * starts by drawing it back with two 15-bit sprites (which follow the S1
 * shake like everything else). Then each moving thing is drawn again in one
 * clipped pass: every element of the pass carries its own drawing area (a
 * GP0 E3h/E4h pair before its polygons, shared by consecutive polygons of
 * one slot), a moving element's is its box on screen, a resting piece's is
 * the bounding box of its box inside the boxes of what it can be in front
 * of (the ordering slots and the floor footprints decide "can"), grown
 * over the resting pieces in front of those until nothing changes. Inside
 * a clip that is the full render's painter order, outside it the backdrop
 * is untouched; the harness compares the two paths pixel for pixel
 * (room-test 6..9). A resting piece's prims keep their screen boxes and
 * nearest slots from the fill, so the pass draws again only those that
 * reach its clip and are not wholly behind what it is drawn over. Modes per
 * frame: REPLAY (the backdrop is valid), FILL (draw it; right after a
 * change from rest, or one frame after motion stopped) and LIVE (the
 * single full pass: while orbiting, during the wall drop-and-pop, or when
 * the pass would cost about as much as a full render).
 *
 * THE RESTING PLAYER (H4): the 3D chibi is the dearest thing in the room
 * (about 9 ms in Redux), and in Live / Build it mostly stands still. While
 * it has not moved, turned or changed frame for a moment it joins the
 * static layer: drawn into the backdrop with everything else (model.c
 * g_model_hold keeps its walk cycle where it was), and drawn again in the
 * clipped pass only when something moving overlaps it, without its contact
 * shadow (the backdrop has it already). It leaves the layer the frame it
 * moves.
 *
 * Every vertex is transformed once (RTPT per 3), faces are culled by their
 * screen winding and emitted as POLY_F4/G4/F3/G3/GT4 straight into the
 * ordering table; a group of vertices wholly outside the clip is dropped,
 * one wholly inside skips the per-face clip test.
 *
 * Memory: the code and its tables live in the `home` overlay group with the
 * rest of the house (tools/ovl_plan.py, SPEC C3), which loads over the 3D
 * views' tables below the slot (ovl_plan.py BORROW); the per-vertex and
 * per-polygon paths are -O2, the rest -Os (COLD_BEGIN). .bss is the camera
 * and backdrop state (about 110 B); the textures are generated on the
 * stack when vram_image() needs them; the resting elements' boxes, their
 * prims' boxes and their adjacency (STab, 5 KB) are kept in
 * DoomoRoomView.plat_cache (scene scratch) when it is there. Harness:
 * g_room_test (hcmd.c, `room-test N`) swaps in fixed test layouts for the
 * fps gates and the parity checks (harness/scripts/home.txt); they are in
 * room_test.c, a part of the home group of their own on the disc (H4,
 * ovl_plan.py PARTS), loaded only when a test layout is asked for.
 */
#define COLD_BEGIN _Pragma("GCC push_options") _Pragma("GCC optimize (\"Os\")")
#define COLD_END   _Pragma("GCC pop_options")
#include "r3d.h"

#ifdef DOOMO_HAVE_CORE
#include <string.h>
#include <inline_c.h>
#include "postfx.h"
#include "doomo/palette.h"
#include "doomo/furn.h"
#include "doomo/home.h"
#include "doomo/tween.h"
#include "doomo/portrait.h"
#include "../../core/gen/furn_ids.h"
#include "room_int.h"

/* ---- scale, camera, ordering ------------------------------------------------ */
#define RT          32              /* world units per tile                       */
#define RU          2               /* world units per iso unit (1/16 tile, z px) */
#define WALL_H      128
#define STUB_H      12
#define WALL_T      4
#define SKIRT_H     8
#define CORN_H      8
#define LIP_H       10
#define LOOK_Z      40              /* the look-at point's height                 */
#define CAM_DIST    1100
#define CAM_PITCH   (-398)          /* 35 degrees down (4096 = 360)               */
#define CAM_FAR     (CAM_DIST + 800)
#define YAW_BASE    2560            /* camera heading of view 0: towards -x -y    */

#define SLOT_FLOOR  (OT_LEN - 3)
#define SLOT_WALL   (OT_LEN - 4)
#define SLOT_TRIM   (OT_LEN - 5)
#define SLOT_RUG    (OT_LEN - 6)
#define SLOT_SHADOW (OT_LEN - 7)
#define SLOT_MAXD   (OT_LEN - 12)   /* deepest depth-sorted slot                  */
#define SLOT_MIND   8
#define SLOT_STUB   4               /* front stubs: nearer than everything        */
#define SLOT_DEPTH  (-1)
#define MARK_BIAS   12              /* a floor mark sorts behind what stands on it */


/* DoomoRoomObj flags of H1 M3 (platform.h RO_HALF_X / RO_HALF_Y): a small
 * on a surface sits half a tile further along +x / +y */
#define RH_HALF_X   32
#define RH_HALF_Y   64

/* ---- the backdrop (VRAM) -------------------------------------------------------- */
#define BD_PAGES    3               /* world-texture pages lent: x 320..703         */
#define BD_X0       320

/* ---- persistent state (.bss) -------------------------------------------------- */
static struct {
    TwSpring yaw;                   /* Q4 yaw units, unwrapped                    */
    int32_t  yaw_tgt;               /* unwrapped target                           */
    uint16_t time;                  /* v->time of the last call                   */
    uint8_t  have;
    uint8_t  wall_back[4];          /* 1 = full wall (seen from inside)           */
    uint8_t  wall_age[4];           /* frames since it flipped, 0xFF settled      */
    int32_t  zoom_q8;               /* smoothed close-up 256..410                 */
    int32_t  lx, ly;                /* smoothed look-at, world 16.16              */
    int32_t  px, py;                /* the player's last position (fix16)         */
    int16_t  pang;                  /* the player's facing                        */
    int32_t  rp_x, rp_y;            /* the player actor last frame (fix16) ...     */
    int16_t  rp_z;                  /* ... its height                              */
    uint8_t  still;                 /* frames it has not moved (capped)           */
    uint16_t clut;                  /* the borrowed CLUT row (getClut id)         */
    uint32_t clut_key;
    uint32_t fit_key;               /* fitted projection for this room / view     */
    int32_t  fit_h;                 /* H at CAM_DIST for zoom 1.0                 */
    int16_t  fit_dx, fit_dy;        /* projection centre offset from the viewport's */
    uint32_t last_key;              /* the static key of the last frame           */
    uint32_t bd_key;                /* what the backdrop shows                    */
    uint32_t bd_up;                 /* vram_wtex_lent_uploads() when it was drawn */
    uint8_t  bd_ok, prev_mode;
} R;

/* ---- per-frame context (stack) ------------------------------------------------- */
#define VMAX 40
typedef struct VIn { int16_t x, y, z, pad; } VIn;
/* screen box (inclusive), OT slot range, floor footprint (world units
 * relative to the camera) */
typedef struct SBox { int16_t x0, y0, x1, y1, s0, s1, fx0, fy0, fx1, fy1; } SBox;
typedef struct RC {
    const DoomoRoomView *v;
    int W, D;                       /* tiles                                      */
    int vw, vh;
    int32_t cx, cy, cz;             /* integer camera position                    */
    int lx, ly, lz;                 /* key light, Q8 unit vector                  */
    int amb_r, amb_g, amb_b;        /* day colour, 128 = 1.0                      */
    int nlamp;
    int32_t lamp_x[4], lamp_y[4], lamp_r2[4];
    int wall_h[4];                  /* current height of each wall                */
    /* the piece being drawn */
    int32_t m00, m01, m10, m11, m22;  /* local Q4 -> world, Q8                    */
    int32_t tx, ty, tz;             /* its origin, world rel. camera, Q8          */
    int rot, fx, slot, bias, bias0; /* fx: FX_* effects                           */
    int plx, ply;                   /* the key light in the piece's frame         */
    int gmode;                      /* the GTE holds camera x piece: vloc is local */
    int ar, ag, ab;                 /* its ambient (fullbright or day)            */
    /* where the polygons go */
    uint32_t *ot;                   /* the ordering table                         */
    int areas;                      /* the pass: a drawing area before each prim  */
    uint32_t area[2];               /* its GP0 E3h / E4h words (set_clip)         */
    uint32_t *last_area, *last_slot; /* the newest area prim and its slot         */
    int clx0, cly0, clx1, cly1;     /* the clip (screen, inclusive)               */
    int allin;                      /* the current vertex group is inside it      */
    int lod;                        /* 1 while the camera moves: fewer sides      */
    int px_q8;                      /* screen px per iso unit x 256 (round LOD)   */
    int prest;                      /* the resting player's actor index, or -1    */
    uint32_t polys;
    VIn in[VMAX];
    uint32_t sxy[VMAX];
    int32_t sz[VMAX];
    uint32_t col[VMAX];
} RC;
enum { FX_SEMI = 1, FX_RED = 2, FX_GREY = 4, FX_BLINK = 8, FX_HELL = 16 };

#define MEMBAR() __asm__ volatile("" ::: "memory")
#define SXV(c, i) ((int)(int16_t)((c)->sxy[i]))
#define SYV(c, i) ((int)(int16_t)((c)->sxy[i] >> 16))

/* ---- small helpers -------------------------------------------------------------- */
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline uint32_t rgb_q7(uint32_t c, int r, int g, int b)
{
    int cr = (int)((c & 0xff) * (uint32_t)r) >> 7;
    int cg = (int)(((c >> 8) & 0xff) * (uint32_t)g) >> 7;
    int cb = (int)(((c >> 16) & 0xff) * (uint32_t)b) >> 7;
    return r3_rgb(cr, cg, cb);
}

/* 0x00BBGGRR lerp, t 0..256: red and blue share one multiply */
static inline uint32_t rgb_lerp(uint32_t a, uint32_t b, uint32_t t256)
{
    uint32_t u = 256 - t256;
    uint32_t rb = (((a & 0xff00ffu) * u + (b & 0xff00ffu) * t256) >> 8) & 0xff00ffu;
    uint32_t g = (((a & 0xff00u) * u + (b & 0xff00u) * t256) >> 8) & 0xff00u;
    return rb | g;
}

static uint32_t mix(uint32_t h, uint32_t v)
{
    h ^= v;
    h *= 0x01000193u;
    return h ^ (h >> 13);
}

/* 0..4095 heading of (dx, dy) */
static int ang_of(int32_t dx, int32_t dy)
{
    int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int a;
    if (!ax && !ay) return 0;
    /* atan on [0, 45 deg] ~ 512 * t * (1.27 - 0.27 t), t = small / big */
    if (ax >= ay) {
        int t = (int)((ay << 12) / ax);
        a = (t * (5202 - ((t * 1106) >> 12))) >> 14;  /* 0..512 */
    } else {
        int t = (int)((ax << 12) / ay);
        a = 1024 - ((t * (5202 - ((t * 1106) >> 12))) >> 14);
    }
    if (dx < 0) a = 2048 - a;
    if (dy < 0) a = 4096 - a;
    return a & 4095;
}

/* 1 / (1 + 16 d^2 / r^2 / 16), minus its value at 4 r^2, as 0..128 */
static const uint8_t k_lamp[64] = {
    128, 118, 110, 102, 96, 89, 84, 79, 74, 70, 66, 62, 59, 56, 53, 50, 48, 45, 43, 41, 39,
    37, 35, 33, 32, 30, 28, 27, 26, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 12,
    11, 10, 9, 9, 8, 8, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0 };

/* the overworld's day colour (overworld.c day_colour), 128 = 1.0 */
static const uint8_t k_day[16][3] = {
    { 52, 60, 104}, { 56, 64, 108}, { 62, 70, 112}, { 70, 76, 116},
    { 84, 82, 116}, {104, 92, 112}, {124, 104, 104}, {132, 116, 108},
    {132, 122, 114}, {132, 126, 120}, {130, 128, 124}, {130, 128, 126},
    {130, 129, 127}, {130, 130, 128}, {131, 130, 129}, {132, 132, 132} };

/* ---- vertices ------------------------------------------------------------------- */
static void xform(RC *c, int n)
{
    int i = 0;
    for (; i + 3 <= n; i += 3) {
        MEMBAR();
        gte_ldv3(&c->in[i], &c->in[i + 1], &c->in[i + 2]);
        gte_rtpt();
        gte_stsxy3(&c->sxy[i], &c->sxy[i + 1], &c->sxy[i + 2]);
        gte_stsz3(&c->sz[i], &c->sz[i + 1], &c->sz[i + 2]);
    }
    for (; i < n; i++) {
        MEMBAR();
        gte_ldv0(&c->in[i]);
        gte_rtps();
        gte_stsxy(&c->sxy[i]);
        gte_stsz(&c->sz[i]);
    }
    MEMBAR();
}

/* the screen box of vertices [0, n) */
static void sbox_of(const RC *c, int n, SBox *b)
{
    int i, x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;
    for (i = 0; i < n; i++) {
        int x = SXV(c, i), y = SYV(c, i);
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    b->x0 = (int16_t)x0; b->y0 = (int16_t)y0; b->x1 = (int16_t)x1; b->y1 = (int16_t)y1;
}

/* the floor footprint of in[0, n) (camera-relative), grown by m */
COLD_BEGIN
static void foot_of(const RC *c, int n, int m, SBox *b)
{
    int i, x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;
    for (i = 0; i < n; i++) {
        int x = c->in[i].x, y = c->in[i].y;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    b->fx0 = (int16_t)(x0 - m); b->fy0 = (int16_t)(y0 - m);
    b->fx1 = (int16_t)(x1 + m); b->fy1 = (int16_t)(y1 + m);
}
COLD_END

/* transform the group [0, n); 0 = it is wholly outside the clip */
static int group(RC *c, int n)
{
    SBox b;
    xform(c, n);
    sbox_of(c, n, &b);
    if (b.x1 < c->clx0 || b.x0 > c->clx1 || b.y1 < c->cly0 || b.y0 > c->cly1) return 0;
    c->allin = b.x0 >= c->clx0 && b.x1 <= c->clx1 && b.y0 >= c->cly0 && b.y1 <= c->cly1;
    return 1;
}

/* a world point relative to the camera */
static inline void vset(RC *c, int i, int32_t wx, int32_t wy, int32_t wz)
{
    c->in[i].x = (int16_t)(wx - c->cx);
    c->in[i].y = (int16_t)(wy - c->cy);
    c->in[i].z = (int16_t)(wz - c->cz);
}

/* a piece-local point (iso units x 16) through the piece transform (in
 * GTE mode the GTE does it: the point goes in as it is) */
static inline void vloc(RC *c, int i, int32_t lx, int32_t ly, int32_t lz)
{
    if (c->gmode) {
        c->in[i].x = (int16_t)lx;
        c->in[i].y = (int16_t)ly;
        c->in[i].z = (int16_t)lz;
        return;
    }
    c->in[i].x = (int16_t)((c->tx + c->m00 * lx + c->m01 * ly) >> 8);
    c->in[i].y = (int16_t)((c->ty + c->m10 * lx + c->m11 * ly) >> 8);
    c->in[i].z = (int16_t)((c->tz + c->m22 * lz) >> 8);
}

/* GTE mode: rotation = camera x piece (local x16 units -> camera, 4.12),
 * translation = the piece origin in camera space */
static void piece_gte(RC *c)
{
    MATRIX g;
    const MATRIX *m = &g_cam.m;
    int i;
    int32_t t0 = c->tx >> 6, t1 = c->ty >> 6, t2 = c->tz >> 6;   /* world, Q2 */
    for (i = 0; i < 3; i++) {
        g.m[i][0] = (int16_t)((m->m[i][0] * c->m00 + m->m[i][1] * c->m10) >> 8);
        g.m[i][1] = (int16_t)((m->m[i][0] * c->m01 + m->m[i][1] * c->m11) >> 8);
        g.m[i][2] = (int16_t)((m->m[i][2] * c->m22) >> 8);
        g.t[i] = m->t[i] + ((m->m[i][0] * t0 + m->m[i][1] * t1 + m->m[i][2] * t2) >> 14);
    }
    MEMBAR();                        /* the macros read g through a pointer */
    gte_SetRotMatrix(&g);
    gte_SetTransMatrix(&g);
    c->gmode = 1;
}

static void camera_gte(RC *c)
{
    gte_SetRotMatrix(&g_cam.m);
    gte_SetTransMatrix(&g_cam.m);
    c->gmode = 0;
}

/* ---- polygons ------------------------------------------------------------------- */
static inline int depth_slot(RC *c, int32_t zsum, int nv)
{
    int s = (int)((nv == 4 ? zsum >> 2 : (zsum * 85) >> 8) >> g_cam.ot_shift) + c->bias;
    return clampi(s, SLOT_MIND, SLOT_MAXD);
}

/* prim memory (gfx_alloc inlined: sizes are multiples of 4) */
static inline void *palloc(uint32_t n)
{
    uint8_t *p = g_prim_ptr;
    if ((uint32_t)(g_prim_end - p) < n) return NULL;
    g_prim_ptr = p + n;
    return p;
}

/* add a prim (in the backdrop pass: after its element's drawing area; when
 * that slot still starts with this clip's area, right after it) */
static inline void put(RC *c, void *p, int slot)
{
    uint32_t *s = c->ot + slot, *a;
    if (!c->areas) {
        addPrim(s, p);
        return;
    }
    a = c->last_area;
    if (a && c->last_slot == s && (*s & 0x00ffffffu) == ((uint32_t)a & 0x00ffffffu)) {
        uint32_t *q = (uint32_t *)p;
        *q = (*q & 0xff000000u) | (*a & 0x00ffffffu);
        *a = (*a & 0xff000000u) | ((uint32_t)q & 0x00ffffffu);
        return;
    }
    addPrim(s, p);
    a = (uint32_t *)palloc(12);
    if (a) {
        a[0] = 0x02000000u;
        a[1] = c->area[0];
        a[2] = c->area[1];
        addPrim(s, a);
        c->last_area = a;
        c->last_slot = s;
    }
}

static inline int on_screen(RC *c, int a, int b, int d, int e)
{
    int x0, x1, x2, x3, y0, y1, y2, y3;
    if (c->allin) return 1;
    x0 = SXV(c, a); x1 = SXV(c, b); x2 = SXV(c, d); x3 = SXV(c, e);
    y0 = SYV(c, a); y1 = SYV(c, b); y2 = SYV(c, d); y3 = SYV(c, e);
    if (x0 < c->clx0 && x1 < c->clx0 && x2 < c->clx0 && x3 < c->clx0) return 0;
    if (x0 > c->clx1 && x1 > c->clx1 && x2 > c->clx1 && x3 > c->clx1) return 0;
    if (y0 < c->cly0 && y1 < c->cly0 && y2 < c->cly0 && y3 < c->cly0) return 0;
    if (y0 > c->cly1 && y1 > c->cly1 && y2 > c->cly1 && y3 > c->cly1) return 0;
    return 1;
}

/* screen winding of (a, b, d): > 0 = the front (Z order a b / d e) */
static inline int facing(RC *c, int a, int b, int d)
{
    int ax = SXV(c, a), ay = SYV(c, a);
    return (SXV(c, b) - ax) * (SYV(c, d) - ay) - (SYV(c, b) - ay) * (SXV(c, d) - ax) > 0;
}

static uint32_t fxcol(const RC *c, uint32_t col)
{
    int fx = c->fx;
    if (fx & FX_GREY) {
        int l = ((col & 0xff) * 77 + ((col >> 8) & 0xff) * 150 + ((col >> 16) & 0xff) * 29) >> 8;
        l = (l + 160) >> 1;
        col = (uint32_t)l * 0x010101u;
    }
    if (fx & FX_RED) col = r3_rgb((int)(col & 0xff) + 70, (int)(((col >> 8) & 0xff) * 90) >> 8,
                                  (int)(((col >> 16) & 0xff) * 90) >> 8);
    if (fx & FX_BLINK) col = rgb_q7(col, 166, 166, 166);
    if (fx & FX_HELL) col = r3_rgb((int)(col & 0xff) + 56, (int)((col >> 8) & 0xff),
                                   (int)((col >> 16) & 0xff));
    return col;
}
#define FXC(c, col) ((c)->fx ? fxcol((c), (col)) : (col))

/* translucent prims need ABR 0 (50/50): a texpage just before them */
static void semi_page(RC *c, int slot)
{
    DR_TPAGE *t = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE));
    if (!t) return;
    setDrawTPage(t, 1, 1, getTPage(0, 0, 0, 0));
    addPrim(c->ot + slot, t);
}

/* flat quad, Z order a b / d e (front = clockwise on screen) */
static void f4(RC *c, int a, int b, int d, int e, uint32_t col, int slot)
{
    POLY_F4 *p;
    if (!facing(c, a, b, d) || !on_screen(c, a, b, d, e)) return;
    p = (POLY_F4 *)palloc(sizeof(POLY_F4));
    if (!p) return;
    setPolyF4(p);
    *(uint32_t *)&p->r0 = FXC(c, col) | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->x0 = c->sxy[a];
    *(uint32_t *)&p->x1 = c->sxy[b];
    *(uint32_t *)&p->x2 = c->sxy[d];
    *(uint32_t *)&p->x3 = c->sxy[e];
    if (slot < 0) slot = depth_slot(c, c->sz[a] + c->sz[b] + c->sz[d] + c->sz[e], 4);
    if (c->fx & FX_SEMI) setSemiTrans(p, 1);
    put(c, p, slot);
    if (c->fx & FX_SEMI) semi_page(c, slot);
    c->polys++;
}

/* gouraud quad: the vertices' own colours */
static void g4(RC *c, int a, int b, int d, int e, int slot)
{
    POLY_G4 *p;
    if (!facing(c, a, b, d) || !on_screen(c, a, b, d, e)) return;
    p = (POLY_G4 *)palloc(sizeof(POLY_G4));
    if (!p) return;
    setPolyG4(p);
    if (c->fx) {
        *(uint32_t *)&p->r0 = fxcol(c, c->col[a]) | ((uint32_t)p->code << 24);
        *(uint32_t *)&p->r1 = fxcol(c, c->col[b]);
        *(uint32_t *)&p->r2 = fxcol(c, c->col[d]);
        *(uint32_t *)&p->r3 = fxcol(c, c->col[e]);
    } else {
        *(uint32_t *)&p->r0 = c->col[a] | ((uint32_t)p->code << 24);
        *(uint32_t *)&p->r1 = c->col[b];
        *(uint32_t *)&p->r2 = c->col[d];
        *(uint32_t *)&p->r3 = c->col[e];
    }
    *(uint32_t *)&p->x0 = c->sxy[a];
    *(uint32_t *)&p->x1 = c->sxy[b];
    *(uint32_t *)&p->x2 = c->sxy[d];
    *(uint32_t *)&p->x3 = c->sxy[e];
    if (slot < 0) slot = depth_slot(c, c->sz[a] + c->sz[b] + c->sz[d] + c->sz[e], 4);
    if (c->fx & FX_SEMI) setSemiTrans(p, 1);
    put(c, p, slot);
    if (c->fx & FX_SEMI) semi_page(c, slot);
    c->polys++;
}

/* gouraud triangle (front = clockwise on screen) */
static void g3(RC *c, int a, int b, int d, int slot)
{
    POLY_G3 *p;
    if (!facing(c, a, b, d) || !on_screen(c, a, b, d, d)) return;
    p = (POLY_G3 *)palloc(sizeof(POLY_G3));
    if (!p) return;
    setPolyG3(p);
    if (c->fx) {
        *(uint32_t *)&p->r0 = fxcol(c, c->col[a]) | ((uint32_t)p->code << 24);
        *(uint32_t *)&p->r1 = fxcol(c, c->col[b]);
        *(uint32_t *)&p->r2 = fxcol(c, c->col[d]);
    } else {
        *(uint32_t *)&p->r0 = c->col[a] | ((uint32_t)p->code << 24);
        *(uint32_t *)&p->r1 = c->col[b];
        *(uint32_t *)&p->r2 = c->col[d];
    }
    *(uint32_t *)&p->x0 = c->sxy[a];
    *(uint32_t *)&p->x1 = c->sxy[b];
    *(uint32_t *)&p->x2 = c->sxy[d];
    if (slot < 0) slot = depth_slot(c, c->sz[a] + c->sz[b] + c->sz[d], 3);
    if (c->fx & FX_SEMI) setSemiTrans(p, 1);
    put(c, p, slot);
    if (c->fx & FX_SEMI) semi_page(c, slot);
    c->polys++;
}

/* flat triangle */
static void f3(RC *c, int a, int b, int d, uint32_t col, int slot)
{
    POLY_F3 *p;
    if (!facing(c, a, b, d) || !on_screen(c, a, b, d, d)) return;
    p = (POLY_F3 *)palloc(sizeof(POLY_F3));
    if (!p) return;
    setPolyF3(p);
    *(uint32_t *)&p->r0 = FXC(c, col) | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->x0 = c->sxy[a];
    *(uint32_t *)&p->x1 = c->sxy[b];
    *(uint32_t *)&p->x2 = c->sxy[d];
    if (slot < 0) slot = depth_slot(c, c->sz[a] + c->sz[b] + c->sz[d], 3);
    if (c->fx & FX_SEMI) setSemiTrans(p, 1);
    put(c, p, slot);
    if (c->fx & FX_SEMI) semi_page(c, slot);
    c->polys++;
}

/* textured gouraud quad; uv: u0 v0 u1 v1 of the texel box (a = u0 v0) */
static void gt4(RC *c, int a, int b, int d, int e, const TexRef *t, int u0, int v0, int u1, int v1,
                int slot)
{
    POLY_GT4 *p;
    if (!facing(c, a, b, d) || !on_screen(c, a, b, d, e)) return;
    p = (POLY_GT4 *)palloc(sizeof(POLY_GT4));
    if (!p) return;
    setPolyGT4(p);
    *(uint32_t *)&p->r0 = c->col[a] | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->r1 = c->col[b];
    *(uint32_t *)&p->r2 = c->col[d];
    *(uint32_t *)&p->r3 = c->col[e];
    *(uint32_t *)&p->x0 = c->sxy[a];
    *(uint32_t *)&p->x1 = c->sxy[b];
    *(uint32_t *)&p->x2 = c->sxy[d];
    *(uint32_t *)&p->x3 = c->sxy[e];
    setUV4(p, t->u + u0, t->v + v0, t->u + u1, t->v + v0, t->u + u0, t->v + v1, t->u + u1, t->v + v1);
    p->tpage = t->tpage;
    p->clut = t->clut;
    put(c, p, slot);
    c->polys++;
}

/* a soft blob (vertices i..i+3, transformed) as a translucent textured quad */
static void blob_quad(RC *c, int i, int slot)
{
    TexRef tb;
    POLY_FT4 *p;
    if (!on_screen(c, i, i + 1, i + 2, i + 3)) return;
    vram_blob(&tb);
    p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
    if (!p) return;
    setPolyFT4(p);
    setSemiTrans(p, 1);
    *(uint32_t *)&p->r0 = 0x606060u | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->x0 = c->sxy[i];
    *(uint32_t *)&p->x1 = c->sxy[i + 1];
    *(uint32_t *)&p->x2 = c->sxy[i + 2];
    *(uint32_t *)&p->x3 = c->sxy[i + 3];
    setUV4(p, tb.u, tb.v, tb.u + 31, tb.v, tb.u, tb.v + 31, tb.u + 31, tb.v + 31);
    p->tpage = tb.tpage;
    p->clut = tb.clut;
    if (slot < 0) slot = depth_slot(c, c->sz[i] + c->sz[i + 1] + c->sz[i + 2] + c->sz[i + 3], 4);
    put(c, p, slot);
    c->polys++;
}

/* Code that runs once a frame or less (textures, walls, the camera, the
 * backdrop's bookkeeping, the harness layouts) is compiled for size; the
 * per-vertex and per-polygon paths above keep -O2 (the home group's size,
 * SPEC C4). */

/* ---- textures and the borrowed CLUT row ------------------------------------------ */
COLD_BEGIN
#define MW0 PAL_PAINT(FURN_MARK_A, 0)   /* wall markers: line, alt, base   */
#define MW1 PAL_PAINT(FURN_MARK_A, 1)
#define MW2 PAL_PAINT(FURN_MARK_A, 2)
#define MF0 PAL_PAINT(FURN_MARK_B, 0)   /* floor markers: grout, a, b      */
#define MF1 PAL_PAINT(FURN_MARK_B, 1)
#define MF2 PAL_PAINT(FURN_MARK_B, 2)
#define FLOOR_TW 48                     /* tile A | tile B | planks, 16x16 */
#define WALL_TW  16
#define WALL_TH  56                     /* (WALL_H - SKIRT_H - CORN_H) / RU */

static void gen_floor(uint8_t *p)
{
    int x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < FLOOR_TW; x++) {
            int u = x & 15, k = x >> 4;
            uint8_t col;
            if (k < 2) {
                col = (u == 0 || y == 0) ? MF0 : (k ? MF2 : MF1);
                if (u == 1 && y > 0 && y < 15) col = k ? MF1 : MF2;      /* a soft bevel */
            } else {
                int board = y >> 2, seam = (board & 1) ? 10 : 3;
                col = ((y & 3) == 0 || u == seam) ? MF0 : (board & 1) ? MF2 : MF1;
            }
            p[y * FLOOR_TW + x] = col;
        }
}

/* wallpaper pattern 0 stripes, 1 dots, 2 wainscot, 3 panels (room_ref) */
static void gen_wall(uint8_t *p, int pat)
{
    int x, y;
    for (y = 0; y < WALL_TH; y++)
        for (x = 0; x < WALL_TW; x++) {
            uint8_t col = MW2;
            if (pat == 0 && (x >> 3) & 1) col = MW1;
            else if (pat == 1) {
                int yy = y - 4 - (((x >> 3) & 1) << 2);
                if ((x & 7) < 2 && yy >= 0 && (yy & 7) < 2) col = MW1;
            } else if (pat == 2) {
                if (y > WALL_TH - WALL_TH / 3) col = MW1;
                if (y == WALL_TH - WALL_TH / 3) col = MW0;
            } else if (pat == 3 && x == 0) {
                col = MW1;
            }
            p[y * WALL_TW + x] = col;
        }
}

/* The CLUT row: the master palette with the 6 marker shades swapped for the
 * room's paints. vram_sil_clut(0) pins a silhouette row (colour 0 is the
 * transparent index: nobody draws its silhouette) and the upload after it
 * overwrites that row; re-sent when the paints or the palette change, when
 * the row moved or was re-filled, and after any frame the house missed. */
static uint16_t room_clut(int wall_paint, int floor_paint, int fresh)
{
    uint8_t *before = g_prim_ptr;
    uint16_t id = vram_sil_clut(0);
    uint32_t key = mix(mix(mix(0x9E37u, (uint32_t)wall_paint), (uint32_t)floor_paint),
                       (uint32_t)g_pal15[PAL_PAINT(wall_paint, 1)] ^ ((uint32_t)g_pal15[PAL_PAINT(floor_paint, 1)] << 16));
    int s;
    if (fresh || id != R.clut || before != g_prim_ptr || key != R.clut_key) {
        uint16_t buf[256];
        memcpy(buf, g_pal15, sizeof(buf));
        for (s = 0; s < 3; s++) {
            buf[MW0 + s] = g_pal15[PAL_PAINT(wall_paint, s)];
            buf[MF0 + s] = g_pal15[PAL_PAINT(floor_paint, s)];
        }
        if (gfx_upload(0, (id >> 6) & 0x1ff, 256, 1, buf)) {
            R.clut = id;
            R.clut_key = key;
        } else {
            R.clut_key = 0;
        }
    }
    return id;
}

/* the floor and wall textures with the room's CLUT; 0 = not this frame */
static int room_textures(const DoomoRoomView *v, int dt, TexRef *wt, TexRef *ft)
{
    int pat = v->wallpaper == 0xFF ? 0 : (v->wallpaper & 3);
    uint8_t wall_pix[WALL_TW * WALL_TH], floor_pix[FLOOR_TW * 16];
    DoomoImage wimg, fimg;
    uint16_t clut;
    gen_wall(wall_pix, pat);
    gen_floor(floor_pix);
    memset(&wimg, 0, sizeof(wimg));
    memset(&fimg, 0, sizeof(fimg));
    wimg.w = WALL_TW; wimg.h = WALL_TH; wimg.pix = wall_pix; wimg.uid = 0x524F4F00u | (uint32_t)pat;
    fimg.w = FLOOR_TW; fimg.h = 16; fimg.pix = floor_pix; fimg.uid = 0x524F4F10u;
    clut = room_clut(v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM,
                     v->floor_paint < 32 ? v->floor_paint : PAINT_CARAMEL, dt != 1);
    if (!vram_image(&wimg, 0, 0, wt) || !vram_image(&fimg, 0, 0, ft)) return 0;
    wt->clut = ft->clut = clut;
    return 1;
}
COLD_END

/* ---- the room box --------------------------------------------------------------- */
/* lamp pools + seam shade on one floor vertex (world x, y), textured scale */
static uint32_t floor_light(RC *c, int32_t wx, int32_t wy, int seam)
{
    int r = c->amb_r, g = c->amb_g, b = c->amb_b, i;
    for (i = 0; i < c->nlamp; i++) {
        int32_t dx = wx - c->lamp_x[i], dy = wy - c->lamp_y[i];
        int32_t d2 = dx * dx + dy * dy;
        int k = (int)((d2 << 4) / c->lamp_r2[i]);
        if (k < 64) {
            int a = k_lamp[k];
            r += (a * 120) >> 7;
            g += (a * 92) >> 7;
            b += (a * 52) >> 7;
        }
    }
    if (seam) { r = (r * 3) >> 2; g = (g * 3) >> 2; b = (b * 3) >> 2; }
    return r3_rgb(r, g, b);
}

static void draw_floor(RC *c, const TexRef *t)
{
    const DoomoRoomView *v = c->v;
    int W = c->W, D = c->D, i, j, k, plank = (v->floor != 0xFF && (v->floor & 3) == 1);
    int walls = v->room_kind == ROOMK_WALLS;
    int row = W + 1;
    if (row * 2 > VMAX) return;
    c->fx = 0;
    c->bias = 0;
    /* two rows of vertices at a time: row j in [0, row), row j + 1 in [row, 2 row) */
    for (j = 0; j < D; j++) {
        for (k = 0; k < 2; k++)
            for (i = 0; i <= W; i++) {
                int jj = j + k, n = k * row + i;
                int32_t wx = i * RT, wy = jj * RT;
                vset(c, n, wx, wy, 0);
                c->col[n] = floor_light(c, wx, wy, walls && (i == 0 || jj == 0 || i == W || jj == D));
            }
        if (!group(c, row * 2)) continue;
        for (i = 0; i < W; i++) {
            int u0 = plank ? 32 : (((i + j) & 1) ? 16 : 0);
            /* seen from above: Z order (x0 y0) (x1 y0) / (x0 y1) (x1 y1) */
            gt4(c, i, i + 1, row + i, row + i + 1, t, u0, 0, u0 + 15, 15, SLOT_FLOOR);
        }
    }
}

COLD_BEGIN
/* wall w: origin, the "right" direction seen from inside, length (tiles) */
static void wall_frame(const RC *c, int w, int32_t *ox, int32_t *oy, int *rx, int *ry, int *len)
{
    switch (w) {
    case 0: *ox = 0; *oy = 0; *rx = 1; *ry = 0; *len = c->W; break;
    case 1: *ox = 0; *oy = c->D * RT; *rx = 0; *ry = -1; *len = c->D; break;
    case 2: *ox = c->W * RT; *oy = c->D * RT; *rx = -1; *ry = 0; *len = c->W; break;
    default: *ox = c->W * RT; *oy = 0; *rx = 0; *ry = 1; *len = c->D; break;
    }
}

/* lighting of a vertical face with outward normal (nx, ny): Q7 multiplier */
static int face_q7(const RC *c, int nx, int ny)
{
    int s = nx * c->lx + ny * c->ly;                  /* -256..256 */
    return 92 + ((s * 40) >> 8) + (s > 0 ? 0 : (s * 20) >> 8);
}

/* wall w's slab: vertices e..e+7 = inner z=0 at both ends, inner z=h at
 * both ends, outer (s=-T, s=len+T) at z=0 and z=h */
static void wall_slab(RC *c, int w, int h, int e)
{
    int32_t ox, oy, ex, ey;
    int rx, ry, len, nx, ny;
    wall_frame(c, w, &ox, &oy, &rx, &ry, &len);
    nx = -ry; ny = rx;
    ex = -nx * WALL_T; ey = -ny * WALL_T;
    vset(c, e + 0, ox, oy, 0);
    vset(c, e + 1, ox + rx * len * RT, oy + ry * len * RT, 0);
    vset(c, e + 2, ox, oy, h);
    vset(c, e + 3, ox + rx * len * RT, oy + ry * len * RT, h);
    vset(c, e + 4, ox + ex - rx * WALL_T, oy + ey - ry * WALL_T, 0);
    vset(c, e + 5, ox + ex + rx * (len * RT + WALL_T), oy + ey + ry * (len * RT + WALL_T), 0);
    vset(c, e + 6, ox + ex - rx * WALL_T, oy + ey - ry * WALL_T, h);
    vset(c, e + 7, ox + ex + rx * (len * RT + WALL_T), oy + ey + ry * (len * RT + WALL_T), h);
}

static void draw_wall(RC *c, int w, const TexRef *t)
{
    int32_t ox, oy;
    int rx, ry, len, h = c->wall_h[w], s, nx, ny, n;
    int slot = R.wall_back[w] ? SLOT_WALL : SLOT_STUB, tslot = R.wall_back[w] ? SLOT_TRIM : SLOT_STUB;
    int qin, qout, qcap, low = h <= 40, k, e;
    uint32_t wood, wood_l, cap, outer, base;
    wall_frame(c, w, &ox, &oy, &rx, &ry, &len);
    if (len < 1 || h <= 0 || (!low && !t)) return;
    nx = -ry; ny = rx;                                 /* inward normal */
    qin = face_q7(c, nx, ny);
    qout = face_q7(c, -nx, -ny);
    qcap = 116;
    {
        int paint = c->v->wall_paint < 32 ? c->v->wall_paint : PAINT_BUBBLEGUM;
        base = g_rgb[PAL_PAINT(paint, 2)];
        wood = g_rgb[PAL_RAMP(RAMP_WOOD, 3)];
        wood_l = g_rgb[PAL_RAMP(RAMP_WOOD, 5)];
        cap = g_rgb[PAL_RAMP(RAMP_WOOD, 2)];
        outer = g_rgb[PAL_RAMP(RAMP_STONE, 4)];
    }
    c->fx = 0;
    c->bias = 0;
    /* vertices:  0..len   inner, z = SKIRT_H (or 0 when low)
     *            len+1..  inner, z = h - CORN_H (or h when low)
     *            then the slab (wall_slab) */
    n = (len + 1) * 2 + 8;
    if (n > VMAX) return;
    {
        int zlo = low ? 0 : SKIRT_H, zhi = low ? h : h - CORN_H;
        k = len + 1;
        for (s = 0; s <= len; s++) {
            int32_t px = ox + rx * s * RT, py = oy + ry * s * RT;
            vset(c, s, px, py, zlo);
            vset(c, k + s, px, py, zhi);
            if (low) {
                c->col[s] = rgb_q7(base, (c->amb_r * qin) >> 7, (c->amb_g * qin) >> 7, (c->amb_b * qin) >> 7);
                c->col[k + s] = c->col[s];
            } else {
                /* textured: 128 = 1.0, a touch darker at the floor */
                c->col[s] = r3_rgb((c->amb_r * qin * 7) >> 10, (c->amb_g * qin * 7) >> 10, (c->amb_b * qin * 7) >> 10);
                c->col[k + s] = r3_rgb((c->amb_r * qin) >> 7, (c->amb_g * qin) >> 7, (c->amb_b * qin) >> 7);
            }
        }
        e = 2 * (len + 1);
        wall_slab(c, w, h, e);
        if (!group(c, n)) return;
    }
    {
        uint32_t c_skirt = rgb_q7(wood, (c->amb_r * qin) >> 7, (c->amb_g * qin) >> 7, (c->amb_b * qin) >> 7);
        uint32_t c_corn = rgb_q7(wood_l, (c->amb_r * qin) >> 7, (c->amb_g * qin) >> 7, (c->amb_b * qin) >> 7);
        uint32_t c_cap = rgb_q7(cap, (c->amb_r * qcap) >> 7, (c->amb_g * qcap) >> 7, (c->amb_b * qcap) >> 7);
        uint32_t c_out = rgb_q7(outer, (c->amb_r * qout) >> 7, (c->amb_g * qout) >> 7, (c->amb_b * qout) >> 7);
        if (low) {
            /* a stub: one painted band per column */
            for (s = 0; s < len; s++)
                g4(c, k + s, k + s + 1, s, s + 1, slot);
        } else {
            int vt = 0;
            if (h < WALL_H) vt = clampi(WALL_TH - (h - SKIRT_H - CORN_H) / RU, 0, WALL_TH - 1);
            for (s = 0; s < len; s++)
                gt4(c, k + s, k + s + 1, s, s + 1, t, 0, vt, WALL_TW - 1, WALL_TH - 1, slot);
            /* skirting (z 0..SKIRT_H) and cornice (h - CORN_H..h): reuse the
             * band rows' end vertices */
            f4(c, 0, len, e + 0, e + 1, c_skirt, tslot);
            f4(c, e + 2, e + 3, k, k + len, c_corn, tslot);
        }
        /* the cap (seen from above) and the outer face (seen from outside) */
        f4(c, e + 6, e + 7, e + 2, e + 3, c_cap, R.wall_back[w] ? tslot : SLOT_STUB);
        f4(c, e + 7, e + 6, e + 5, e + 4, c_out, R.wall_back[w] ? slot : SLOT_STUB);
    }
}

/* garden plots: a soil lip around the floor */
static void draw_lip(RC *c)
{
    int W = c->W * RT, D = c->D * RT;
    uint32_t soil = g_rgb[PAL_RAMP(RAMP_PEACH, 2)], s0, s1;
    vset(c, 0, 0, 0, 0); vset(c, 1, W, 0, 0); vset(c, 2, W, D, 0); vset(c, 3, 0, D, 0);
    vset(c, 4, 0, 0, -LIP_H); vset(c, 5, W, 0, -LIP_H); vset(c, 6, W, D, -LIP_H); vset(c, 7, 0, D, -LIP_H);
    s0 = rgb_q7(soil, c->amb_r, c->amb_g, c->amb_b);
    s1 = rgb_q7(soil, (c->amb_r * 3) >> 2, (c->amb_g * 3) >> 2, (c->amb_b * 3) >> 2);
    c->fx = 0;
    c->bias = 0;
    if (!group(c, 8)) return;
    /* the four outer faces: -y, +x, +y, -x (seen from outside) */
    f4(c, 1, 0, 5, 4, s0, SLOT_TRIM);
    f4(c, 2, 1, 6, 5, s1, SLOT_TRIM);
    f4(c, 3, 2, 7, 6, s0, SLOT_TRIM);
    f4(c, 0, 3, 4, 7, s1, SLOT_TRIM);
}
COLD_END

/* ---- furniture -------------------------------------------------------------------- */
/* tones of a material: dark, base, light, highlight (0x00BBGGRR) */
static void mat_tones(int mat, const DoomoRoomObj *o, uint32_t t[4])
{
    uint8_t i0, i1, i2, i3;
    if (mat == FM_A || mat == FM_B) {
        int p = mat == FM_A ? (o->paint_a < 32 ? o->paint_a : FURN_MARK_A)
                            : (o->paint_b < 32 ? o->paint_b : FURN_MARK_B);
        i0 = PAL_PAINT(p, 0); i1 = PAL_PAINT(p, 1); i2 = PAL_PAINT(p, 2);
        i3 = palette_lighter(i2);
    } else if (mat >= PAL_PAINT_BASE && mat < PAL_SKIN_BASE) {
        int p = (mat - PAL_PAINT_BASE) / 3;
        i0 = PAL_PAINT(p, 0); i1 = PAL_PAINT(p, 1); i2 = PAL_PAINT(p, 2);
        i3 = palette_lighter(i2);
    } else if (mat >= PAL_RAMP_BASE) {
        int r = (mat - PAL_RAMP_BASE) / PAL_RAMP_SHADES;
        i0 = PAL_RAMP(r, 2); i1 = PAL_RAMP(r, 4); i2 = PAL_RAMP(r, 6); i3 = PAL_RAMP(r, 7);
    } else {
        i1 = (uint8_t)mat;
        i0 = palette_darker(i1);
        i2 = palette_lighter(i1);
        i3 = palette_lighter(i2);
    }
    t[0] = g_rgb[i0]; t[1] = g_rgb[i1]; t[2] = g_rgb[i2]; t[3] = g_rgb[i3];
}

/* the iso art's tone ramp (furngen: > 150 light, > 95 base, else dark),
 * smooth; t[] is already lit (draw_piece), the lerp weights x256/63 and
 * x256/62 are multiply-shifts */
static uint32_t tone(const RC *c, const uint32_t t[4], int s, int shiny)
{
    (void)c;
    if (shiny && s > 225) return t[3];
    if (s >= 185) return t[2];
    if (s >= 122) return rgb_lerp(t[1], t[2], (uint32_t)((s - 122) * 1040) >> 8);
    if (s >= 60) return rgb_lerp(t[0], t[1], (uint32_t)((s - 60) * 1057) >> 8);
    return t[0];
}

/* shade of a piece-local normal (Q12): the key light is turned into the
 * piece's frame by piece_setup (plx, ply) */
static inline int shade(const RC *c, int nx, int ny, int nz)
{
    return (nx * c->plx + ny * c->ply + nz * c->lz) >> 12;
}

/* LOD of round prims: longitudes by the radius on screen (r iso units;
 * px_q8: screen pixels per iso unit x 256 at the look-at distance, 380 in
 * a 6x6 room at 320x240). 8 sides from a 7 px radius, 6 from 2.5 px: a
 * 6x6 room's pieces keep their sides (5 iso units: 7.4 px), an 8x8 room
 * is drawn smaller and its small round parts lose sides as on screen. */
static inline int ring_n(const RC *c, int r)
{
    int rp16 = (r * c->px_q8) >> 4;          /* radius on screen, px x 16 */
    if (c->lod) return rp16 >= 7 * 16 ? 6 : 4;
    return rp16 >= 7 * 16 ? 8 : rp16 >= 40 ? 6 : 4;
}

static const int16_t k_cos8[8] = { 4096, 2896, 0, -2896, -4096, -2896, 0, 2896 };
static const int16_t k_sin8[8] = { 0, 2896, 4096, 2896, 0, -2896, -4096, -2896 };
static const int16_t k_cos6[6] = { 4096, 2048, -2048, -4096, -2048, 2048 };
static const int16_t k_sin6[6] = { 0, 3547, 3547, 0, -3547, -3547 };
static const int16_t k_cos4[4] = { 4096, 0, -4096, 0 };
static const int16_t k_sin4[4] = { 0, 4096, 0, -4096 };

static void ring_tabs(int n, const int16_t **kc, const int16_t **ks)
{
    *kc = n == 8 ? k_cos8 : n == 6 ? k_cos6 : k_cos4;
    *ks = n == 8 ? k_sin8 : n == 6 ? k_sin6 : k_sin4;
}

static void prim_box(RC *c, const DoomoFurnPrim *p, const uint32_t t[4])
{
    int i, shiny = FURN_PRIM_STYLE(p) & FPS_SHINY;
    int32_t x0 = p->x * 16, y0 = p->y * 16, z0 = p->z * 16;
    int32_t x1 = x0 + p->w * 16, y1 = y0 + p->d * 16, z1 = z0 + p->h * 16;
    for (i = 0; i < 8; i++)
        vloc(c, i, (i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
    if (!group(c, 8)) return;
    /* the bottom always faces away from a camera above the floor */
    f4(c, 4, 5, 6, 7, tone(c, t, shade(c, 0, 0, 4096), shiny), c->slot);      /* top */
    f4(c, 6, 7, 2, 3, tone(c, t, shade(c, 0, 4096, 0), shiny), c->slot);      /* +y  */
    f4(c, 5, 4, 1, 0, tone(c, t, shade(c, 0, -4096, 0), shiny), c->slot);     /* -y  */
    f4(c, 7, 5, 3, 1, tone(c, t, shade(c, 4096, 0, 0), shiny), c->slot);      /* +x  */
    f4(c, 4, 6, 0, 2, tone(c, t, shade(c, -4096, 0, 0), shiny), c->slot);     /* -x  */
}

/* blob: centre (x, y, z), diameters (w, d, h); rings at -45, 0, +45 deg */
static void prim_blob(RC *c, const DoomoFurnPrim *p, const uint32_t t[4])
{
    static const int16_t lat_c[3] = { 2896, 4096, 2896 }, lat_s[3] = { -2896, 0, 2896 };
    int rxy = (p->w > p->d ? p->w : p->d) / 2, n = ring_n(c, rxy), i, j;
    const int16_t *kc, *ks;
    int shiny = FURN_PRIM_STYLE(p) & FPS_SHINY;
    int32_t cx = p->x * 16, cy = p->y * 16, cz = p->z * 16;
    int32_t rx = p->w * 8, ry = p->d * 8, rz = p->h * 8;     /* radii, x16 */
    ring_tabs(n, &kc, &ks);
    for (j = 0; j < 3; j++)
        for (i = 0; i < n; i++) {
            int nx = (kc[i] * lat_c[j]) >> 12, ny = (ks[i] * lat_c[j]) >> 12, nz = lat_s[j];
            vloc(c, j * n + i, cx + ((rx * nx) >> 12), cy + ((ry * ny) >> 12), cz + ((rz * nz) >> 12));
        }
    vloc(c, 3 * n, cx, cy, cz + rz);
    if (!group(c, 3 * n + 1)) return;
    for (j = 0; j < 3; j++)
        for (i = 0; i < n; i++)
            c->col[j * n + i] = tone(c, t, shade(c, (kc[i] * lat_c[j]) >> 12, (ks[i] * lat_c[j]) >> 12, lat_s[j]), shiny);
    c->col[3 * n] = tone(c, t, shade(c, 0, 0, 4096), shiny);
    /* the bottom cap looks down: never seen */
    for (i = 0; i < n; i++) {
        int k = i + 1 == n ? 0 : i + 1;
        for (j = 0; j < 2; j++)
            g4(c, (j + 1) * n + k, (j + 1) * n + i, j * n + k, j * n + i, c->slot);
        g3(c, 3 * n, 2 * n + i, 2 * n + k, c->slot);              /* top cap    */
    }
}

/* cylinder / cone: centre (x, y), bottom z, diameter w, height h */
static void prim_round(RC *c, const DoomoFurnPrim *p, const uint32_t t[4], int cone)
{
    int r = p->w / 2, n = ring_n(c, r), nv = cone ? n + 1 : 2 * n + 1, i;
    const int16_t *kc, *ks;
    int shiny = FURN_PRIM_STYLE(p) & FPS_SHINY;
    int32_t cx = p->x * 16, cy = p->y * 16, z0 = p->z * 16, z1 = (p->z + p->h) * 16;
    int32_t rr = p->w * 8;
    int nz = cone ? (int)(((int32_t)p->w * 2048) / (p->h ? p->h : 1)) : 0;
    ring_tabs(n, &kc, &ks);
    if (nz > 3000) nz = 3000;
    for (i = 0; i < n; i++) {
        int32_t x = cx + ((rr * kc[i]) >> 12), y = cy + ((rr * ks[i]) >> 12);
        vloc(c, i, x, y, z0);
        if (!cone) vloc(c, n + i, x, y, z1);
    }
    vloc(c, cone ? n : 2 * n, cx, cy, z1);
    if (!group(c, nv)) return;
    for (i = 0; i < n; i++) {
        c->col[i] = tone(c, t, shade(c, kc[i], ks[i], nz), shiny);
        if (!cone) c->col[n + i] = c->col[i];
    }
    c->col[cone ? n : 2 * n] = tone(c, t, shade(c, 0, 0, 4096), shiny);   /* the tip / cap */
    for (i = 0; i < n; i++) {
        int k = i + 1 == n ? 0 : i + 1;
        if (cone) g3(c, n, i, k, c->slot);
        else g4(c, n + k, n + i, k, i, c->slot);
    }
    if (!cone)
        for (i = 1; i + 1 < n; i++)
            f3(c, n, n + i, n + i + 1, c->col[2 * n], c->slot);
}

static const DoomoFurn *furn_of(const DoomoRoomObj *o)
{
    return o->furn < doomo_furn_count ? &doomo_furn[o->furn] : NULL;
}

static int has_frames(const DoomoFurn *f)
{
    int k;
    for (k = 0; k < f->nprim; k++)
        if (FURN_PRIM_FRAMES(&doomo_furn_prims[f->prim0 + k])) return 1;
    return 0;
}

/* What of a piece the backdrop can hold while the camera rests: all of it,
 * its body (the prims of every frame: an animated piece's frame prims are
 * drawn in a pass), or nothing (it moves, blinks, jitters or is a ghost). */
enum { PK_STATIC, PK_SPLIT, PK_DYNAMIC };
static int piece_kind(const DoomoRoomObj *o, const DoomoFurn *f)
{
    if (o->flags & (RO_GHOST | RO_SELECTED | RO_INVALID | RO_SEMI)) return PK_DYNAMIC;
    if (o->sq_x || o->sq_y || o->jitter) return PK_DYNAMIC;
    return has_frames(f) ? PK_SPLIT : PK_STATIC;
}
/* draw_piece / piece_box parts */
enum { DP_SHADOW = 1, DP_BODY = 2, DP_ANIM = 4, DP_ALL = DP_BODY | DP_ANIM };

/* the contact shadow (pieces standing on the floor only) */
static int has_shadow(const DoomoRoomObj *o, const DoomoFurn *f)
{
    return !((f->flags & (FF_RUG | FF_WALL | FF_SMALL)) || o->z);
}

/* where the walls' pieces hang: rot even -> wall 0 (y = 0), odd -> wall 1 (x = 0) */
static int piece_hidden(const RC *c, const DoomoRoomObj *o, const DoomoFurn *f)
{
    if (!(f->flags & FF_WALL) || c->v->room_kind != ROOMK_WALLS) return 0;
    {
        int w = RO_ROT(o) & 1;
        return !R.wall_back[w] || c->wall_h[w] < WALL_H - 16;
    }
}

/* the piece transform, effects and light (draw_piece / piece_box); returns
 * the footprint in iso units */
COLD_BEGIN
static void piece_setup(RC *c, const DoomoRoomObj *o, const DoomoFurn *f, int *pW, int *pD)
{
    int rot = (f->flags & FF_WALL) ? (RO_ROT(o) & 1) : RO_ROT(o);
    int W = (f->fw ? f->fw : 1) * 16, D = (f->fd ? f->fd : 1) * 16;     /* iso units */
    int Wr = (rot & 1) ? D : W, Dr = (rot & 1) ? W : D;
    int32_t ox = o->x * 16, oy = o->y * 16, oz = o->z;
    int32_t sx = 256 + o->sq_x * 2, sz = 256 + o->sq_y * 2;
    int32_t r00, r01, r10, r11, bx, by, cxr, cyr;
    int jit = 0;
    if (o->flags & RH_HALF_X) ox += 8;
    if (o->flags & RH_HALF_Y) oy += 8;
    if (f->flags & FF_WALL) {
        /* hang on the wall's inner face */
        if (rot & 1) ox = 0; else oy = 0;
    }
    if (o->jitter) jit = (int)((c->v->time >> 1) & 1) * o->jitter - o->jitter / 2;
    /* local (iso units) -> room (iso units): rotation + where local (0,0) lands */
    switch (rot) {
    case 1: r00 = 0; r01 = 1; r10 = -1; r11 = 0; bx = ox; by = oy + W; break;
    case 2: r00 = -1; r01 = 0; r10 = 0; r11 = -1; bx = ox + W; by = oy + D; break;
    case 3: r00 = 0; r01 = -1; r10 = 1; r11 = 0; bx = ox + D; by = oy; break;
    default: r00 = 1; r01 = 0; r10 = 0; r11 = 1; bx = ox; by = oy; break;
    }
    /* squash about the footprint centre (x / y) and the base (z); then world
     * (RU per iso unit) relative to the camera, Q8; local inputs are x16 */
    cxr = ox * 16 + Wr * 8;
    cyr = oy * 16 + Dr * 8;
    c->rot = rot;
    /* world (wx, wy) = R(local): the light's local components are R^T l */
    switch (rot) {
    case 1: c->plx = -c->ly; c->ply = c->lx; break;
    case 2: c->plx = -c->lx; c->ply = -c->ly; break;
    case 3: c->plx = c->ly; c->ply = -c->lx; break;
    default: c->plx = c->lx; c->ply = c->ly; break;
    }
    c->m00 = (r00 * sx * RU) >> 4; c->m01 = (r01 * sx * RU) >> 4;
    c->m10 = (r10 * sx * RU) >> 4; c->m11 = (r11 * sx * RU) >> 4;
    c->m22 = (sz * RU) >> 4;
    c->tx = (int32_t)(((cxr + (((bx * 16 - cxr) * sx) >> 8)) * RU + jit * 16) << 4) - (c->cx << 8);
    c->ty = (int32_t)(((cyr + (((by * 16 - cyr) * sx) >> 8)) * RU) << 4) - (c->cy << 8);
    c->tz = (oz * RU << 8) - (c->cz << 8);
    /* effects and light */
    c->fx = 0;
    if (o->flags & RO_GHOST) c->fx |= FX_SEMI | ((o->flags & RO_INVALID) ? FX_RED : 0) |
                                      ((o->flags & RO_SEMI) ? FX_GREY : 0);
    else if (o->flags & RO_SEMI) c->fx |= FX_SEMI | FX_GREY;
    if ((o->flags & RO_SELECTED) && ((c->v->time >> 3) & 1)) c->fx |= FX_BLINK;
    if (o->jitter) c->fx |= FX_HELL;
    if (o->flags & RO_FULLBRIGHT) { c->ar = 136; c->ag = 130; c->ab = 120; }
    else { c->ar = c->amb_r; c->ag = c->amb_g; c->ab = c->amb_b; }
    c->slot = (f->flags & FF_RUG) ? SLOT_RUG : SLOT_DEPTH;
    c->bias0 = (o->flags & RO_GHOST) ? -24 : oz ? -6 : 0;
    *pW = W;
    *pD = D;
}
COLD_END

/* a prim's local bounds (iso units x 16): x0 x1 y0 y1 z0 z1 */
static void prim_ext(const DoomoFurnPrim *p, int32_t e[6])
{
    int kind = FURN_PRIM_KIND(p);
    if (kind == FP_BOX) {
        e[0] = p->x * 16; e[1] = e[0] + p->w * 16;
        e[2] = p->y * 16; e[3] = e[2] + p->d * 16;
        e[4] = p->z * 16; e[5] = e[4] + p->h * 16;
    } else {
        int32_t ry = (kind == FP_BLOB ? p->d : p->w) * 8;
        e[0] = p->x * 16 - p->w * 8; e[1] = p->x * 16 + p->w * 8;
        e[2] = p->y * 16 - ry; e[3] = p->y * 16 + ry;
        if (kind == FP_BLOB) { e[4] = p->z * 16 - p->h * 8; e[5] = p->z * 16 + p->h * 8; }
        else { e[4] = p->z * 16; e[5] = (p->z + p->h) * 16; }
    }
}

/* a prim's screen box through the piece transform (its 8 corners) */
static void prim_sbox(RC *c, const DoomoFurnPrim *p, SBox *b)
{
    int32_t e[6];
    int i;
    prim_ext(p, e);
    for (i = 0; i < 8; i++)
        vloc(c, i, e[i & 1], e[2 + ((i >> 1) & 1)], e[4 + (i >> 2)]);
    xform(c, 8);
    sbox_of(c, 8, b);
}

/* The screen box and OT slot range of a piece's parts (DP_*: the extents of
 * those prims, and the footprint when its shadow is wanted, through the
 * piece transform). 0 = nothing of it on screen. */
COLD_BEGIN
static int piece_box(RC *c, const DoomoRoomObj *o, const DoomoFurn *f, int parts, SBox *b)
{
    int W, D, k, i, m, any = 0, anim = RO_ANIM(o);
    int32_t x0 = 0x7fff, y0 = 0x7fff, z0 = 0x7fff, x1 = -0x7fff, y1 = -0x7fff, z1 = -0x7fff;
    int32_t zmin = 0x7fffffff, zmax = 0;
    piece_setup(c, o, f, &W, &D);
    if (parts & DP_SHADOW) {
        x0 = 0; y0 = 0; z0 = 0; x1 = W * 16; y1 = D * 16; z1 = 8;
        any = 1;
    }
    for (k = 0; k < f->nprim; k++) {
        const DoomoFurnPrim *p = &doomo_furn_prims[f->prim0 + k];
        int32_t e[6];
        int fm = FURN_PRIM_FRAMES(p);
        if (fm ? (!(parts & DP_ANIM) || !(fm & (1 << anim))) : !(parts & DP_BODY)) continue;
        any = 1;
        prim_ext(p, e);
        if (e[0] < x0) x0 = e[0];
        if (e[1] > x1) x1 = e[1];
        if (e[2] < y0) y0 = e[2];
        if (e[3] > y1) y1 = e[3];
        if (e[4] < z0) z0 = e[4];
        if (e[5] > z1) z1 = e[5];
    }
    if (!any) { c->fx = 0; return 0; }
    for (i = 0; i < 8; i++)
        vloc(c, i, (i & 1) ? x1 + 16 : x0 - 16, (i & 2) ? y1 + 16 : y0 - 16, (i & 4) ? z1 : z0);
    foot_of(c, 8, 1 + (o->jitter < 0 ? -o->jitter : o->jitter), b);
    xform(c, 8);
    sbox_of(c, 8, b);
    for (i = 0; i < 8; i++) {
        if (c->sz[i] < zmin) zmin = c->sz[i];
        if (c->sz[i] > zmax) zmax = c->sz[i];
    }
    m = 2 + (o->jitter < 0 ? -o->jitter : o->jitter) * 2;
    b->x0 = (int16_t)(b->x0 - m); b->x1 = (int16_t)(b->x1 + m);
    b->y0 = (int16_t)(b->y0 - m); b->y1 = (int16_t)(b->y1 + m);
    if (f->flags & FF_RUG) {
        b->s0 = b->s1 = SLOT_RUG;
    } else {
        b->s0 = (int16_t)clampi((int)(zmin >> g_cam.ot_shift) + c->bias0 - f->nprim / 2 - 1, SLOT_MIND, SLOT_MAXD);
        b->s1 = (int16_t)clampi((int)(zmax >> g_cam.ot_shift) + c->bias0 + 1, SLOT_MIND, SLOT_MAXD);
    }
    c->fx = 0;
    return b->x1 >= c->clx0 && b->x0 <= c->clx1 && b->y1 >= c->cly0 && b->y0 <= c->cly1;
}
COLD_END

/* blob shadow under a footprint (centre and radius in iso units x 16) */
static void contact_shadow(RC *c, int32_t cx, int32_t cy, int32_t r)
{
    vloc(c, 0, cx - r, cy - r, 8);
    vloc(c, 1, cx + r, cy - r, 8);
    vloc(c, 2, cx - r, cy + r, 8);
    vloc(c, 3, cx + r, cy + r, 8);
    if (!group(c, 4)) return;
    blob_quad(c, 0, SLOT_SHADOW);
}

/* a screen box (inclusive) misses the clip */
static inline int off_clip(const RC *c, int x0, int y0, int x1, int y1)
{
    return x1 < c->clx0 || x0 > c->clx1 || y1 < c->cly0 || y0 > c->cly1;
}

/* a resting piece's prim: its screen box and its nearest ordering slot */
typedef struct PBox { int16_t x0, y0, x1, y1, s0; } PBox;

/* parts: DP_* (the shadow; the body prims; the current frame's prims);
 * pb: the prims' cached boxes (a resting piece in a pass) or NULL, and
 * lim: then only prims nearer than this slot are drawn (what lies behind
 * every moving thing it is drawn over is in the backdrop already) */
static void draw_piece(RC *c, const DoomoRoomObj *o, const DoomoFurn *f, int parts, const PBox *pb, int lim)
{
    int W, D, k, anim = RO_ANIM(o);
    piece_setup(c, o, f, &W, &D);
    piece_gte(c);
    /* the contact shadow first (a group of its own) */
    if ((parts & DP_SHADOW) && has_shadow(o, f)) {
        int Wr = (c->rot & 1) ? D : W, Dr = (c->rot & 1) ? W : D;
        int32_t rr = (Wr < Dr ? Wr : Dr) * 9;               /* x16, a bit past the edge */
        int sfx = c->fx;
        c->fx = 0;
        contact_shadow(c, W * 8, D * 8, rr);
        c->fx = sfx;
    }
    for (k = 0; k < f->nprim; k++) {
        const DoomoFurnPrim *p = &doomo_furn_prims[f->prim0 + k];
        uint32_t t[4];
        int fm = FURN_PRIM_FRAMES(p);
        if (fm ? (!(parts & DP_ANIM) || !(fm & (1 << anim))) : !(parts & DP_BODY)) continue;
        /* orbiting: the smallest details wait for the camera to rest */
        if (c->lod && p->w < 3 && p->h < 3 && (FURN_PRIM_KIND(p) != FP_BOX || p->d < 3)) continue;
        /* in a pass, only the prims that reach the clip */
        if (pb) {
            if (pb[k].s0 > lim || off_clip(c, pb[k].x0, pb[k].y0, pb[k].x1, pb[k].y1)) continue;
        } else if (c->areas && FURN_PRIM_KIND(p) != FP_BOX) {
            SBox b;
            prim_sbox(c, p, &b);
            if (off_clip(c, b.x0, b.y0, b.x1, b.y1)) continue;
        }
        mat_tones(p->mat, o, t);
        t[0] = rgb_q7(t[0], c->ar, c->ag, c->ab);
        t[1] = rgb_q7(t[1], c->ar, c->ag, c->ab);
        t[2] = rgb_q7(t[2], c->ar, c->ag, c->ab);
        t[3] = rgb_q7(t[3], c->ar, c->ag, c->ab);
        c->bias = c->bias0 - (k >> 1);
        switch (FURN_PRIM_KIND(p)) {
        case FP_BOX: prim_box(c, p, t); break;
        case FP_BLOB: prim_blob(c, p, t); break;
        case FP_CYL: prim_round(c, p, t, 0); break;
        default: prim_round(c, p, t, 1); break;
        }
    }
    camera_gte(c);
    c->bias = 0;
    c->fx = 0;
}

/* ---- actors ---------------------------------------------------------------------- */
int model_player_draw(const DoomoView3D *v, const DoomoBillboard *bb, uint32_t light);
extern uint8_t g_model_hold;          /* model.c: keep the pose (a resting player) */

static void blob_set(RC *c, int i, int32_t wx, int32_t wy, int32_t wz, int32_t r)
{
    vset(c, i, wx - r, wy - r, wz + 1);
    vset(c, i + 1, wx + r, wy - r, wz + 1);
    vset(c, i + 2, wx - r, wy + r, wz + 1);
    vset(c, i + 3, wx + r, wy + r, wz + 1);
}

/* a billboard sorts by its card; its shadow just behind it (not in the far
 * shadow slot: what stands behind it stays behind, and a pass needs to draw
 * again only what is in front) */
static int actor_slot(RC *c)
{
    return clampi((int)((c->sz[0] + c->sz[3]) >> (1 + g_cam.ot_shift)) - 2, SLOT_MIND, SLOT_MAXD - 1);
}

/* a camera-facing billboard standing on its feet, with its own x / y squash:
 * vertices 0..3 the card, 4..7 the shadow; 0 = nothing to draw */
COLD_BEGIN
static int bill_verts(RC *c, const DoomoRoomActor *a, int *tw, int *th)
{
    const DoomoImage *img = a->img;
    int32_t wx = (a->x * RT) >> 16, wy = (a->y * RT) >> 16, wz = a->z * RU;
    int hw, ww, rx, ry, ux, uy, uz, i;
    if (!img || !img->pix || !img->w || !img->h) return 0;
    *tw = img->w > IMG_TILE ? IMG_TILE : img->w;
    *th = img->h > IMG_TILE ? IMG_TILE : img->h;
    /* chibi sprites at the iso scale; tall figures shrink to a chibi's height */
    hw = img->h <= 40 ? img->h * RU : 60;
    ww = (img->w * hw) / img->h;
    if (a->sx) ww = (ww * a->sx) >> 8;
    if (a->sy) hw = (hw * a->sy) >> 8;
    if (ww < 2 || hw < 2) return 0;
    rx = -g_cam.sa; ry = g_cam.ca;                         /* camera right (4.12) */
    ux = (-g_cam.ca * g_cam.sp) >> 12;                     /* camera up            */
    uy = (-g_cam.sa * g_cam.sp) >> 12;
    uz = g_cam.cp;
    for (i = 0; i < 4; i++) {
        int sxn = (i & 1) ? ww / 2 : -ww / 2, up = (i & 2) ? 0 : hw;
        vset(c, i, wx + ((rx * sxn + ux * up) >> 12), wy + ((ry * sxn + uy * up) >> 12),
             wz + ((uz * up) >> 12));
    }
    blob_set(c, 4, wx, wy, wz, ww / 2 + 2);
    xform(c, 8);
    return 1;
}
COLD_END

COLD_BEGIN
static void billboard(RC *c, const DoomoRoomActor *a)
{
    TexRef tr;
    POLY_FT4 *p;
    int tw, th, slot, flip = (a->flags & BB_FLIPX) ? 1 : 0;
    if (!bill_verts(c, a, &tw, &th)) return;
    c->allin = 0;
    slot = actor_slot(c);
    if (a->flags & BB_SHADOW) blob_quad(c, 4, slot + 1);
    if (!on_screen(c, 0, 1, 2, 3) || !vram_image(a->img, flip << 8, 0, &tr)) return;
    p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
    if (!p) return;
    setPolyFT4(p);
    setSemiTrans(p, (a->flags & BB_TRANSLUCENT) ? 1 : 0);
    *(uint32_t *)&p->r0 = r3_rgb(c->amb_r, c->amb_g, c->amb_b) | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->x0 = c->sxy[0];
    *(uint32_t *)&p->x1 = c->sxy[1];
    *(uint32_t *)&p->x2 = c->sxy[2];
    *(uint32_t *)&p->x3 = c->sxy[3];
    setUV4(p, tr.u, tr.v, tr.u + tw - 1, tr.v, tr.u, tr.v + th - 1, tr.u + tw - 1, tr.v + th - 1);
    p->tpage = tr.tpage;
    p->clut = tr.clut;
    put(c, p, slot);
    c->polys++;
}
COLD_END

/* the player's box: about the model's size (56 units tall) and its shadow;
 * its slots are a little wider than the model's polygons */
COLD_BEGIN
static void player_box(RC *c, const DoomoRoomActor *a, SBox *b)
{
    int32_t wx = (a->x * RT) >> 16, wy = (a->y * RT) >> 16, wz = a->z * RU;
    int i, s0 = OT_LEN, s1 = 0;
    for (i = 0; i < 8; i++)
        vset(c, i, wx + ((i & 1) ? 15 : -15), wy + ((i & 2) ? 15 : -15), wz + ((i & 4) ? 62 : 0));
    xform(c, 8);
    foot_of(c, 8, 1, b);
    sbox_of(c, 8, b);
    for (i = 0; i < 8; i++) {
        int s = (int)(c->sz[i] >> g_cam.ot_shift);
        if (s < s0) s0 = s;
        if (s > s1) s1 = s;
    }
    b->s0 = (int16_t)clampi(s0 - 16, 1, OT_LEN - 2);
    b->s1 = (int16_t)clampi(s1 + 16, 1, OT_LEN - 2);
    b->x0 = (int16_t)(b->x0 - 2); b->x1 = (int16_t)(b->x1 + 2);
    b->y0 = (int16_t)(b->y0 - 2); b->y1 = (int16_t)(b->y1 + 2);
}
COLD_END

/* the player: model.c's 3D chibi, facing where it walks (the core's combat
 * aim angle is borrowed for the call and put back); r3d puts it into the
 * view's full ordering table (g_cam.ot) */
COLD_BEGIN
static void player(RC *c, const DoomoRoomActor *a, int shadow)
{
    DoomoBillboard bb;
    DoomoPlayerState *ps = core_player();
    int16_t sa = 0, sr = 0;
    int32_t dx = a->x - R.px, dy = a->y - R.py;
    int light = (c->amb_r + c->amb_g + c->amb_b) / 3;
    if (!R.pang) R.pang = (int16_t)(((g_cam.ang + 2048) & 4095) | 1);   /* face the camera */
    if ((dx > 256 || dx < -256 || dy > 256 || dy < -256) && dx < FIX_ONE && dx > -FIX_ONE &&
        dy < FIX_ONE && dy > -FIX_ONE)
        R.pang = (int16_t)(ang_of(dx, dy) | 1);
    R.px = a->x;
    R.py = a->y;
    if (shadow) {                       /* its contact shadow (the backdrop has a resting one's) */
        blob_set(c, 0, (a->x * RT) >> 16, (a->y * RT) >> 16, a->z * RU, 13);
        xform(c, 4);
        c->allin = 0;
        blob_quad(c, 0, -1);
    }
    memset(&bb, 0, sizeof(bb));
    bb.x = a->x * RT;
    bb.y = a->y * RT;
    bb.z = ((int32_t)a->z * RU) << 16;
    bb.img = a->img;
    bb.scale = 256;
    if (ps) {
        sa = ps->angle;
        sr = ps->recoil;
        ps->angle = R.pang;
        ps->recoil = 0;
    }
    g_model_hold = (uint8_t)(c->prest >= 0 && a == &c->v->actors[c->prest]);
    model_player_draw(NULL, &bb, (uint32_t)light);
    g_model_hold = 0;
    if (ps) {
        ps->angle = sa;
        ps->recoil = sr;
    }
}
COLD_END

/* ---- marks: cursor, rubber band ---------------------------------------------------- */
COLD_BEGIN
static void tile_verts(RC *c, int tx, int ty)
{
    vset(c, 0, tx * RT + 2, ty * RT + 2, 1);
    vset(c, 1, tx * RT + RT - 2, ty * RT + 2, 1);
    vset(c, 2, tx * RT + 2, ty * RT + RT - 2, 1);
    vset(c, 3, tx * RT + RT - 2, ty * RT + RT - 2, 1);
    xform(c, 4);
}
COLD_END

/* a floor mark sorts at its tile's depth, behind what stands on the tile */
COLD_BEGIN
static int mark_slot(RC *c)
{
    int s;
    c->bias = MARK_BIAS;
    s = depth_slot(c, c->sz[0] + c->sz[1] + c->sz[2] + c->sz[3], 4);
    c->bias = 0;
    return s;
}
COLD_END

COLD_BEGIN
static void tile_mark(RC *c, int tx, int ty, uint32_t col, int semi)
{
    int slot;
    tile_verts(c, tx, ty);
    c->allin = 0;
    if (!on_screen(c, 0, 1, 2, 3)) return;
    slot = mark_slot(c);
    if (semi) {
        POLY_F4 *p = (POLY_F4 *)gfx_alloc(sizeof(POLY_F4));
        DR_TPAGE *tp;
        if (!p) return;
        setPolyF4(p);
        setSemiTrans(p, 1);
        *(uint32_t *)&p->r0 = col | ((uint32_t)p->code << 24);
        *(uint32_t *)&p->x0 = c->sxy[0];
        *(uint32_t *)&p->x1 = c->sxy[1];
        *(uint32_t *)&p->x2 = c->sxy[2];
        *(uint32_t *)&p->x3 = c->sxy[3];
        put(c, p, slot);
        tp = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE));
        if (tp) {
            setDrawTPage(tp, 1, 1, getTPage(0, 1, 0, 0));      /* additive */
            put(c, tp, slot);
        }
    } else {
        static const uint8_t ed[4][2] = { { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 } };
        int e;
        for (e = 0; e < 4; e++) {
            LINE_F2 *l = (LINE_F2 *)gfx_alloc(sizeof(LINE_F2));
            if (!l) return;
            setLineF2(l);
            *(uint32_t *)&l->r0 = col | ((uint32_t)l->code << 24);
            *(uint32_t *)&l->x0 = c->sxy[ed[e][0]];
            *(uint32_t *)&l->x1 = c->sxy[ed[e][1]];
            put(c, l, slot);
        }
    }
    c->polys++;
}
COLD_END

static int cursor_on(const RC *c)
{
    const DoomoRoomView *v = c->v;
    return v->cursor_x >= 0 && v->cursor_y >= 0 && v->cursor_x < c->W && v->cursor_y < c->D;
}

COLD_BEGIN
static void draw_cursor(RC *c)
{
    const DoomoRoomView *v = c->v;
    uint32_t col = ((v->time >> 3) & 1) ? 0xFFFFFFu : g_rgb[PAL_UI_GOLD];
    tile_mark(c, v->cursor_x, v->cursor_y, col, 0);
    tile_mark(c, v->cursor_x, v->cursor_y, 0x302010u, 1);
}
COLD_END

static int sel_on(const RC *c, int i)
{
    return i < 128 && ((c->v->sel_mask[i >> 3] >> (i & 7)) & 1);
}

/* ---- camera --------------------------------------------------------------------- */
COLD_BEGIN
static void look_from(int ang, int32_t lx, int32_t ly, int32_t lz, int32_t *ex, int32_t *ey, int32_t *ez)
{
    /* eye = look-at - forward * CAM_DIST; forward = (cos a cos p, sin a cos p, sin p) */
    int cp = icos(CAM_PITCH), sp = isin(CAM_PITCH);
    int32_t fwd = (cp * CAM_DIST) >> 4;                    /* x 2^8 -> 16.16 below */
    *ex = lx - ((icos(ang) * fwd) >> 4);
    *ey = ly - ((isin(ang) * fwd) >> 4);
    *ez = lz - ((sp * CAM_DIST) << 4);
}

/* fit the room's box into the viewport for view k at zoom 1.0 */
static void fit(RC *c, int k)
{
    const DoomoRoomView *v = c->v;
    uint32_t key = mix(mix(mix(mix(0x51u, (uint32_t)c->W), (uint32_t)c->D), (uint32_t)k),
                       (uint32_t)(c->vw | (c->vh << 10) | (v->room_kind << 20)));
    int32_t ex, ey, ez, lx = (c->W * RT) << 15, ly = (c->D * RT) << 15, lz = LOOK_Z << 16;
    int ang = (YAW_BASE - k * 1024) & 4095, i, H0 = 600, top = v->room_kind == ROOMK_WALLS ? WALL_H : 8;
    int minx = 9999, maxx = -9999, miny = 9999, maxy = -9999, bw, bh, s;
    if (key == R.fit_key) return;
    look_from(ang, lx, ly, lz, &ex, &ey, &ez);
    r3_camera(ex, ey, ez, ang, CAM_PITCH, 0, 0, c->vw, c->vh, H0, CAM_FAR, CAM_FAR, 0);
    c->cx = g_cam.px; c->cy = g_cam.py; c->cz = g_cam.pz;
    for (i = 0; i < 8; i++)
        vset(c, i, (i & 1) ? c->W * RT : 0, (i & 2) ? c->D * RT : 0, (i & 4) ? top : -LIP_H);
    xform(c, 8);
    for (i = 0; i < 8; i++) {
        int x = SXV(c, i), y = SYV(c, i);
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }
    bw = maxx - minx + 1;
    bh = maxy - miny + 1;
    s = ((c->vw - 20) << 8) / bw;
    if (((c->vh - 14) << 8) / bh < s) s = ((c->vh - 14) << 8) / bh;
    R.fit_h = (H0 * s) >> 8;
    /* the box's centre goes to the viewport's centre */
    R.fit_dx = (int16_t)(-(((minx + maxx) / 2 - c->vw / 2) * s >> 8));
    R.fit_dy = (int16_t)(-(((miny + maxy) / 2 - c->vh / 2) * s >> 8));
    R.fit_key = key;
}

/* the spring, the cutaway and the drop-and-pop; dt = ticks since last call */
static int camera_step(RC *c, int dt)
{
    const DoomoRoomView *v = c->v;
    int32_t tgt, cur;
    int a, w;
    int reset = !R.have || dt > 40;
    /* unwrap the target next to the spring's */
    tgt = R.yaw_tgt + ((((int)v->yaw - (int)(R.yaw_tgt & 4095)) + 2048) & 4095) - 2048;
    if (reset) {
        tgt = v->yaw & 4095;
        tw_spring_init(&R.yaw, TW_BOING, tgt * 16);
        R.have = 1;
    }
    if (tgt != R.yaw_tgt || reset) tw_spring_target(&R.yaw, tgt * 16);
    R.yaw_tgt = tgt;
    cur = tgt * 16;
    {
        int i;
        for (i = 0; i < dt && i < 4; i++) cur = tw_spring_step(&R.yaw);
        if (!dt) cur = R.yaw.e + R.yaw.target;
    }
    if (reset) cur = tgt * 16;
    a = (YAW_BASE - (cur >> 4)) & 4095;
    /* cutaway: a wall whose outward normal faces the camera is a stub */
    {
        int ca = icos(a), sa = isin(a), d[4], hy = 600;
        d[0] = sa; d[1] = ca; d[2] = -sa; d[3] = -ca;        /* > 0: faces the camera */
        for (w = 0; w < 4; w++) {
            int back = R.wall_back[w];
            if (reset) back = d[w] <= 0;
            else if (d[w] > hy) back = 0;
            else if (d[w] < -hy) back = 1;
            if (back != R.wall_back[w] && !reset) R.wall_age[w] = 0;
            if (reset) R.wall_age[w] = 0xFF;
            R.wall_back[w] = (uint8_t)back;
        }
    }
    /* the drop-and-pop: falling walls bounce off the floor, rising ones overshoot */
    for (w = 0; w < 4; w++) {
        static const uint8_t drop[4] = { 70, 22, 4, 16 }, rise[4] = { 40, 104, 140, 124 };
        int age = R.wall_age[w];
        int h = R.wall_back[w] ? WALL_H : STUB_H;
        if (age != 0xFF) {
            if (age < 4) h = R.wall_back[w] ? rise[age] : drop[age];
            age += dt;
            R.wall_age[w] = (uint8_t)(age >= 4 ? 0xFF : age);
        }
        c->wall_h[w] = v->room_kind == ROOMK_WALLS ? h : 0;
    }
    return a;
}

/* ---- the frame ------------------------------------------------------------------ */
static void lamps(RC *c)
{
    const DoomoRoomView *v = c->v;
    int i;
    c->nlamp = 0;
    for (i = 0; i < v->nobj && c->nlamp < 4; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = furn_of(o);
        int W, D;
        if (!f || !(f->flags & FF_LIGHT) || (o->flags & RO_GHOST)) continue;
        if (!(o->flags & RO_FULLBRIGHT) && v->daylight >= 9) continue;
        W = (RO_ROT(o) & 1) ? f->fd : f->fw;
        D = (RO_ROT(o) & 1) ? f->fw : f->fd;
        c->lamp_x[c->nlamp] = o->x * RT + (W ? W : 1) * RT / 2;
        c->lamp_y[c->nlamp] = o->y * RT + (D ? D : 1) * RT / 2;
        c->lamp_r2[c->nlamp] = (f->light_r ? f->light_r : 2) * RT;
        c->lamp_r2[c->nlamp] *= c->lamp_r2[c->nlamp];
        c->nlamp++;
    }
}

static void bg_tile(const RC *c, uint32_t *ot_slot)
{
    TILE *bg = (TILE *)gfx_alloc(sizeof(TILE));
    uint32_t col;
    if (!bg) return;
    col = c->v->room_kind == ROOMK_WALLS ? g_rgb[PAL_RAMP(RAMP_VAPE, 1)]
                                         : rgb_q7(g_rgb[PAL_RAMP(RAMP_SKY, 5)], c->amb_r, c->amb_g, c->amb_b);
    setTile(bg);
    *(uint32_t *)&bg->r0 = col | ((uint32_t)bg->code << 24);
    setXY0(bg, c->v->vx, c->v->vy);
    setWH(bg, c->vw, c->vh);
    if (ot_slot) addPrim(ot_slot, bg);
    else gfx_chain(bg);
}

/* the room box, then the pieces (all, or the resting ones only) */
static void draw_set(RC *c, const TexRef *wt, const TexRef *ft, int resting_only)
{
    const DoomoRoomView *v = c->v;
    int i;
    draw_floor(c, ft);
    if (v->room_kind == ROOMK_WALLS) {
        for (i = 0; i < 4; i++) draw_wall(c, i, wt);
    } else {
        draw_lip(c);
    }
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = furn_of(o);
        int k;
        if (!f || piece_hidden(c, o, f)) continue;
        k = resting_only ? piece_kind(o, f) : PK_STATIC;
        if (k == PK_DYNAMIC) continue;
        draw_piece(c, o, f, DP_SHADOW | (k == PK_SPLIT ? DP_BODY : DP_ALL), NULL, 0);
    }
    if (resting_only && c->prest >= 0) player(c, &v->actors[c->prest], 1);
}

/* the actors and the marks, in the order of the full render */
static void draw_actors_marks(RC *c)
{
    const DoomoRoomView *v = c->v;
    int i;
    for (i = 0; i < v->nact; i++) {
        const DoomoRoomActor *ac = &v->actors[i];
        if (ac->flags & RA_PLAYER) player(c, ac, 1);
        else billboard(c, ac);
    }
    if (cursor_on(c)) draw_cursor(c);
    for (i = 0; i < c->W * c->D && i < 128; i++)
        if (sel_on(c, i)) tile_mark(c, i % c->W, i / c->W, 0x402818u, 1);
}

/* ---- the backdrop pass ------------------------------------------------------------ */
/* Over the backdrop, one ordering table holds the moving elements and the
 * resting ones that can be in front of them, each polygon preceded by its
 * element's drawing area (its clip): a moving element is clipped to its own
 * box; a resting one is drawn again only inside the boxes of what it can
 * cover, and pulls in, the same way, whatever can be in front of it there.
 * Every pixel of a clip then gets the full render's painter order. */
#define MAXEL   56                  /* moving / resting elements per frame        */
enum { EK_OBJ, EK_BODY, EK_ANIM, EK_STUB, EK_ACTOR, EK_PLAYER, EK_CURSOR, EK_SEL };
typedef struct El { SBox b; uint8_t kind, idx, far, pad; } El;   /* 24 B */
typedef struct Clip { int16_t x0, y0, x1, y1; } Clip;         /* x0 > x1: none */

/* the resting elements' boxes are kept in the scene scratch while the
 * backdrop is valid (the key covers everything they depend on), with the
 * screen box of each of their prims: a pass draws again only the prims
 * that reach its clip */
#define ST_MAGIC 0x52485333u        /* "RHS3" */
#define AW ((MAXEL + 31) / 32)
#define MAXPB 320                   /* resting prims with a cached box            */
#define PB_NONE 0xFFFF
typedef struct STab {
    uint32_t magic, key, n, npb;
    El el[MAXEL];
    uint16_t pb0[MAXEL];            /* the element's first prim box, PB_NONE      */
    PBox pb[MAXPB];                 /* per prim of the piece                      */
    uint32_t adj[MAXEL][AW];        /* adj[j]: the resting ones that can cover j */
} STab;
COLD_END

/* (the per-frame bookkeeping of the pass keeps -O2) */
COLD_BEGIN
static int el_cost(const RC *c, const El *e)
{
    if (e->kind == EK_OBJ || e->kind == EK_BODY) return 2 + doomo_furn[c->v->objs[e->idx].furn].nprim * 3;
    return e->kind == EK_PLAYER ? 40 : e->kind == EK_ANIM ? 4 : 2;
}
COLD_END

static const PBox *st_pb(const STab *t, int i)
{
    return t->pb0[i] == PB_NONE ? NULL : &t->pb[t->pb0[i]];
}

/* the resting elements: pieces (bodies of the animated ones) and the stubs */
COLD_BEGIN
static int static_boxes(RC *c, STab *t)
{
    const DoomoRoomView *v = c->v;
    El *el = t->el;
    int i, n = 0;
    t->npb = 0;
    for (i = 0; i < v->nobj && n < MAXEL; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = furn_of(o);
        int k, j;
        if (!f || piece_hidden(c, o, f) || (k = piece_kind(o, f)) == PK_DYNAMIC) continue;
        if (!piece_box(c, o, f, k == PK_SPLIT ? DP_BODY : DP_ALL, &el[n].b)) continue;
        el[n].kind = k == PK_SPLIT ? EK_BODY : EK_OBJ;
        el[n].idx = (uint8_t)i;
        el[n].far = 0;
        /* its prims' boxes and nearest slots (piece_box left the piece
         * transform set up; draw_piece's slots: the faces' mean depth +
         * the prim's bias, never nearer than the nearest corner's); the
         * frame prims of a split piece are drawn as moving ones */
        t->pb0[n] = PB_NONE;
        if (t->npb + f->nprim <= MAXPB) {
            PBox *q = &t->pb[t->npb];
            for (j = 0; j < f->nprim; j++) {
                const DoomoFurnPrim *p = &doomo_furn_prims[f->prim0 + j];
                SBox b;
                int32_t zmin = 0x7fffffff;
                int e;
                q[j].x0 = 1; q[j].x1 = 0; q[j].y0 = q[j].y1 = 0; q[j].s0 = OT_LEN;
                if (FURN_PRIM_FRAMES(p)) continue;
                prim_sbox(c, p, &b);
                for (e = 0; e < 8; e++) if (c->sz[e] < zmin) zmin = c->sz[e];
                q[j].x0 = (int16_t)(b.x0 - 2); q[j].y0 = (int16_t)(b.y0 - 2);
                q[j].x1 = (int16_t)(b.x1 + 2); q[j].y1 = (int16_t)(b.y1 + 2);
                q[j].s0 = (int16_t)((f->flags & FF_RUG) ? SLOT_RUG :
                                    clampi((int)(zmin >> g_cam.ot_shift) + c->bias0 - (j >> 1) - 3, SLOT_MIND, SLOT_MAXD));
            }
            t->pb0[n] = (uint16_t)t->npb;
            t->npb += f->nprim;
        }
        c->fx = 0;
        n++;
    }
    if (c->prest >= 0 && n < MAXEL) {
        /* the resting player: drawn again (the same held pose) inside the
         * clips of what moves behind it */
        player_box(c, &v->actors[c->prest], &el[n].b);
        el[n].kind = EK_PLAYER;
        el[n].idx = (uint8_t)c->prest;
        el[n].far = 0;
        t->pb0[n] = PB_NONE;
        n++;
    }
    for (i = 0; i < 4 && n < MAXEL; i++) {
        if (v->room_kind != ROOMK_WALLS || R.wall_back[i] || c->wall_h[i] > 40) continue;
        wall_slab(c, i, c->wall_h[i], 0);
        foot_of(c, 8, 1, &el[n].b);
        xform(c, 8);
        sbox_of(c, 8, &el[n].b);
        el[n].b.s0 = el[n].b.s1 = SLOT_STUB;
        el[n].far = 0;
        el[n].kind = EK_STUB;
        el[n].idx = (uint8_t)i;
        t->pb0[n] = PB_NONE;
        n++;
    }
    return n;
}
COLD_END

/* the moving elements' boxes (on screen only); -1 = too many */
COLD_BEGIN
static int dynamic_boxes(RC *c, El *el)
{
    const DoomoRoomView *v = c->v;
    int i, n = 0, tw, th;
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = furn_of(o);
        int k;
        if (!f || piece_hidden(c, o, f) || (k = piece_kind(o, f)) == PK_STATIC) continue;
        if (n >= MAXEL) return -1;
        el[n].far = 0;
        if (k == PK_SPLIT) {
            /* only this frame's prims; the body and its shadow rest */
            if (!piece_box(c, o, f, DP_ANIM, &el[n].b)) continue;
            el[n].kind = EK_ANIM;
        } else {
            if (!piece_box(c, o, f, DP_ALL | (has_shadow(o, f) ? DP_SHADOW : 0), &el[n].b)) continue;
            /* a contact shadow or a rug goes into a far fixed slot: what
             * rests over its box must be drawn again over it */
            if (has_shadow(o, f) || (f->flags & FF_RUG)) { el[n].b.s1 = OT_LEN; el[n].far = 1; }
            el[n].kind = EK_OBJ;
        }
        el[n].idx = (uint8_t)i;
        n++;
    }
    for (i = 0; i < v->nact; i++) {
        const DoomoRoomActor *a = &v->actors[i];
        SBox *b;
        if (i == c->prest) continue;            /* in the backdrop */
        if (n >= MAXEL) return -1;
        b = &el[n].b;
        el[n].far = 0;
        if (a->flags & RA_PLAYER) {
            player_box(c, a, b);
            el[n].kind = EK_PLAYER;
        } else {
            int s;
            if (!bill_verts(c, a, &tw, &th)) continue;
            foot_of(c, 8, 2, b);
            sbox_of(c, 8, b);
            s = actor_slot(c);
            b->s0 = (int16_t)s;
            b->s1 = (int16_t)(s + 1);
            el[n].kind = EK_ACTOR;
            b->x0 = (int16_t)(b->x0 - 2); b->x1 = (int16_t)(b->x1 + 2);
            b->y0 = (int16_t)(b->y0 - 2); b->y1 = (int16_t)(b->y1 + 2);
        }
        if (b->x1 < c->clx0 || b->x0 > c->clx1 || b->y1 < c->cly0 || b->y0 > c->cly1) continue;
        el[n].idx = (uint8_t)i;
        n++;
    }
    for (i = -1; i < c->W * c->D && i < 128; i++) {
        SBox *b;
        if (i < 0 ? !cursor_on(c) : !sel_on(c, i)) continue;
        if (n >= MAXEL) return -1;
        b = &el[n].b;
        tile_verts(c, i < 0 ? v->cursor_x : i % c->W, i < 0 ? v->cursor_y : i / c->W);
        foot_of(c, 4, 2, b);
        sbox_of(c, 4, b);
        el[n].far = 0;
        b->s0 = b->s1 = (int16_t)mark_slot(c);
        b->x0 = (int16_t)(b->x0 - 1); b->x1 = (int16_t)(b->x1 + 1);
        b->y0 = (int16_t)(b->y0 - 1); b->y1 = (int16_t)(b->y1 + 1);
        if (b->x1 < c->clx0 || b->x0 > c->clx1 || b->y1 < c->cly0 || b->y0 > c->cly1) continue;
        el[n].kind = i < 0 ? EK_CURSOR : EK_SEL;
        el[n].idx = (uint8_t)(i < 0 ? 0 : i);
        n++;
    }
    return n;
}
COLD_END

/* *r = bbox(*r, a ^ b); 1 = it grew, 0 = it did not, -1 = a and b miss */
static int clip_add(Clip *r, const SBox *a, const Clip *b)
{
    int x0 = a->x0 > b->x0 ? a->x0 : b->x0, y0 = a->y0 > b->y0 ? a->y0 : b->y0;
    int x1 = a->x1 < b->x1 ? a->x1 : b->x1, y1 = a->y1 < b->y1 ? a->y1 : b->y1;
    int grew = 0;
    if (x0 > x1 || y0 > y1) return -1;
    if (r->x0 > r->x1) { r->x0 = (int16_t)x0; r->y0 = (int16_t)y0; r->x1 = (int16_t)x1; r->y1 = (int16_t)y1; return 1; }
    if (x0 < r->x0) { r->x0 = (int16_t)x0; grew = 1; }
    if (y0 < r->y0) { r->y0 = (int16_t)y0; grew = 1; }
    if (x1 > r->x1) { r->x1 = (int16_t)x1; grew = 1; }
    if (y1 > r->y1) { r->y1 = (int16_t)y1; grew = 1; }
    return grew;
}

/* Can p be drawn over q somewhere? Not when it is wholly behind q in the
 * ordering table, nor when its footprint lies wholly on q's far side along
 * x or y (the camera, at the origin here, outside both): a ray to any
 * point of q then never reaches p. Something in a far fixed slot (a moving
 * piece's contact shadow, a rug) is drawn under everything. */
static int may_cover(const SBox *p, const El *q)
{
    const SBox *b = &q->b;
    if (q->far) return 1;
    if (p->s0 > b->s1) return 0;
    if ((b->fx1 <= 0 && p->fx1 <= b->fx0) || (b->fx0 >= 0 && p->fx0 >= b->fx1)) return 0;
    if ((b->fy1 <= 0 && p->fy1 <= b->fy0) || (b->fy0 >= 0 && p->fy0 >= b->fy1)) return 0;
    return 1;
}

/* The clips: a moving element's is its box (in the view); a resting one's
 * is the bounding box of its box inside the clips of everything drawn that
 * it can be in front of (its nearest slot is not behind that one's
 * farthest), grown until nothing changes. Returns the work (about prims,
 * a resting element's scaled by how much of it is drawn). */
/* the resting elements that can cover each resting one (their boxes
 * overlap): fixed while the backdrop is */
COLD_BEGIN
static void static_adj(STab *t)
{
    int i, j, ns = (int)t->n;
    memset(t->adj, 0, sizeof(t->adj));
    for (j = 0; j < ns; j++)
        for (i = 0; i < ns; i++) {
            const SBox *a = &t->el[i].b, *b = &t->el[j].b;
            if (i != j && a->x1 >= b->x0 && a->x0 <= b->x1 && a->y1 >= b->y0 && a->y0 <= b->y1 &&
                may_cover(a, &t->el[j]))
                t->adj[j][i >> 5] |= 1u << (i & 31);
        }
}
COLD_END

/* lim[i]: the farthest slot of what resting element i is drawn over (a
 * moving one it may cover, or through the closure all of a resting one
 * drawn again): its prims wholly behind that are left to the backdrop,
 * which already shows them under everything drawn again there */
COLD_BEGIN
static int close_clips(const RC *c, const El *dyn, int nd, Clip *dc, const STab *t, Clip *sc, int16_t *lim)
{
    const El *st = t->el;
    int ns = (int)t->n, i, j, w, cost = 0, top = 0;
    uint8_t todo[MAXEL];
    uint32_t queued[AW];
    memset(queued, 0, sizeof(queued));
    for (i = 0; i < nd; i++) {
        dc[i].x0 = (int16_t)clampi(dyn[i].b.x0, c->clx0, c->clx1);
        dc[i].y0 = (int16_t)clampi(dyn[i].b.y0, c->cly0, c->cly1);
        dc[i].x1 = (int16_t)clampi(dyn[i].b.x1, c->clx0, c->clx1);
        dc[i].y1 = (int16_t)clampi(dyn[i].b.y1, c->cly0, c->cly1);
        cost += el_cost(c, &dyn[i]);
    }
    for (i = 0; i < ns; i++) {
        int grew = 0;
        sc[i].x0 = 1; sc[i].x1 = 0;
        lim[i] = -1;
        for (j = 0; j < nd; j++) {
            int r;
            if (!may_cover(&st[i].b, &dyn[j]) || (r = clip_add(&sc[i], &st[i].b, &dc[j])) < 0) continue;
            grew |= r;
            if (dyn[j].b.s1 > lim[i]) lim[i] = dyn[j].b.s1;
        }
        if (grew) {
            queued[i >> 5] |= 1u << (i & 31);
            todo[top++] = (uint8_t)i;
        }
    }
    /* what can cover a grown clip grows too (and is drawn as deep) */
    while (top > 0) {
        j = todo[--top];
        queued[j >> 5] &= ~(1u << (j & 31));
        for (w = 0; w < AW; w++) {
            uint32_t m = t->adj[j][w];
            for (i = w * 32; m; i++, m >>= 1) {
                int r;
                if (!(m & 1) || (r = clip_add(&sc[i], &st[i].b, &sc[j])) < 0) continue;
                if (st[j].b.s1 > lim[i]) lim[i] = st[j].b.s1;
                if (r && !(queued[i >> 5] & (1u << (i & 31)))) {
                    queued[i >> 5] |= 1u << (i & 31);
                    todo[top++] = (uint8_t)i;
                }
            }
        }
    }
    for (i = 0; i < ns; i++)
        if (sc[i].x0 <= sc[i].x1) {
            const PBox *pb = st_pb(t, i);
            if (pb) {
                /* exactly the prims that reach the clip, in front of lim */
                int k, hit = 0, np = doomo_furn[c->v->objs[st[i].idx].furn].nprim;
                for (k = 0; k < np; k++)
                    if (pb[k].s0 <= lim[i] && !(pb[k].x1 < sc[i].x0 || pb[k].x0 > sc[i].x1 ||
                                                 pb[k].y1 < sc[i].y0 || pb[k].y0 > sc[i].y1))
                        hit++;
                cost += 2 + hit * 3;
            } else {
                int bw = st[i].b.x1 - st[i].b.x0 + 1, bh = st[i].b.y1 - st[i].b.y0 + 1;
                int cw = sc[i].x1 - sc[i].x0 + 1, ch = sc[i].y1 - sc[i].y0 + 1;
                int full = el_cost(c, &st[i]);
                cost += 2 + (bw > 0 && bh > 0 ? (int)((int32_t)full * cw * ch / ((int32_t)bw * bh)) : full) / 2 + full / 2;
            }
        }
    return cost;
}
COLD_END

/* the drawing area of the element being drawn (screen -> frame) */
COLD_BEGIN
static void set_clip(RC *c, const Clip *k)
{
    int fx, fy;
    gfx_fb_xy(&fx, &fy);
    c->last_area = NULL;
    c->clx0 = k->x0; c->cly0 = k->y0; c->clx1 = k->x1; c->cly1 = k->y1;
    c->area[0] = 0xe3000000u | ((uint32_t)(fx + k->x0) & 0x3ff) | (((uint32_t)(fy + k->y0) & 0x3ff) << 10);
    c->area[1] = 0xe4000000u | ((uint32_t)(fx + k->x1) & 0x3ff) | (((uint32_t)(fy + k->y1) & 0x3ff) << 10);
}
COLD_END

/* the backdrop back into the frame: 15-bit sprites from the lent pages */
COLD_BEGIN
static void blit_backdrop(const RC *c, int bx, int by)
{
    int done = 0, w = c->vw;
    while (done < w) {
        int px = bx + done, page = px & ~63, u0 = px - page, seg = w - done;
        DR_TPAGE *t = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE));
        SPRT *s = (SPRT *)gfx_alloc(sizeof(SPRT));
        if (!t || !s) return;
        if (u0 + seg > 256) seg = 256 - u0;
        setDrawTPage(t, 1, 1, getTPage(2, 0, page, 0));
        gfx_chain(t);
        setSprt(s);
        setShadeTex(s, 1);
        setRGB0(s, 128, 128, 128);
        setXY0(s, c->v->vx + done, c->v->vy);
        setUV0(s, u0, by);
        setWH(s, seg, c->vh);
        s->clut = 0;
        gfx_chain(s);
        done += seg;
    }
    g_tpage_cur = 0xffff;
}
COLD_END

/* the pass: everything moving and what it pulled in, in the full render's
 * order (stubs, pieces by index, actors, marks), each in its own clip */
COLD_BEGIN
static void run_pass(RC *c, const El *dyn, int nd, const Clip *dc, const STab *t, const Clip *sc,
                     const int16_t *lim)
{
    const DoomoRoomView *v = c->v;
    const El *st = t->el;
    int i, k, ns = (int)t->n;
    if (!r3_begin()) return;
    c->ot = g_cam.ot;
    c->areas = 1;
    for (i = 0; i < ns; i++)
        if (st[i].kind == EK_STUB && sc[i].x0 <= sc[i].x1) {
            set_clip(c, &sc[i]);
            draw_wall(c, st[i].idx, NULL);
        }
    for (k = 0; k < v->nobj; k++) {
        const DoomoRoomObj *o = &v->objs[k];
        const DoomoFurn *f = furn_of(o);
        if (!f) continue;
        /* resting: no shadow (it is in the backdrop) */
        for (i = 0; i < ns; i++)
            if (st[i].idx == k && (st[i].kind == EK_OBJ || st[i].kind == EK_BODY) && sc[i].x0 <= sc[i].x1) {
                set_clip(c, &sc[i]);
                draw_piece(c, o, f, st[i].kind == EK_OBJ ? DP_ALL : DP_BODY, st_pb(t, i), lim[i]);
            }
        for (i = 0; i < nd; i++)
            if (dyn[i].idx == k && (dyn[i].kind == EK_OBJ || dyn[i].kind == EK_ANIM)) {
                set_clip(c, &dc[i]);
                draw_piece(c, o, f, dyn[i].kind == EK_OBJ ? DP_SHADOW | DP_ALL : DP_ANIM, NULL, 0);
            }
    }
    for (i = 0; i < ns; i++)
        if (st[i].kind == EK_PLAYER && sc[i].x0 <= sc[i].x1) {
            set_clip(c, &sc[i]);
            g_r3_area = c->area;
            player(c, &v->actors[st[i].idx], 0);
            g_r3_area = NULL;
        }
    for (i = 0; i < nd; i++) {
        if (dyn[i].kind != EK_PLAYER && dyn[i].kind != EK_ACTOR) continue;
        set_clip(c, &dc[i]);
        if (dyn[i].kind == EK_PLAYER) {
            g_r3_area = c->area;
            player(c, &v->actors[dyn[i].idx], 1);
            g_r3_area = NULL;
        } else {
            billboard(c, &v->actors[dyn[i].idx]);
        }
    }
    for (i = 0; i < nd; i++)
        if (dyn[i].kind == EK_CURSOR) { set_clip(c, &dc[i]); draw_cursor(c); }
    for (i = 0; i < nd; i++)
        if (dyn[i].kind == EK_SEL) {
            set_clip(c, &dc[i]);
            tile_mark(c, dyn[i].idx % c->W, dyn[i].idx / c->W, 0x402818u, 1);
        }
    c->areas = 0;
    r3_end();
}
COLD_END

/* the static key: everything the backdrop shows */
COLD_BEGIN
static uint32_t static_key(const RC *c, int a, int H)
{
    const DoomoRoomView *v = c->v;
    uint32_t key = mix(mix(mix(0xC0FFEEu, (uint32_t)a), (uint32_t)H), (uint32_t)(g_cam.cx | (g_cam.cy << 12)));
    int i;
    key = mix(mix(key, (uint32_t)(c->cx ^ (c->cy << 11) ^ (c->cz << 22))),
              (uint32_t)(c->W | (c->D << 5) | (v->room_kind << 10) | (c->vw << 12) | (c->vh << 22)));
    key = mix(key, (uint32_t)(v->wallpaper | (v->floor << 8) | (v->wall_paint << 16) | ((uint32_t)v->floor_paint << 24)));
    key = mix(key, (uint32_t)(v->daylight | (c->wall_h[0] << 4) | (c->wall_h[1] << 12) | (c->wall_h[2] << 20)));
    key = mix(key, (uint32_t)(c->wall_h[3] | (R.wall_back[0] << 8) | (R.wall_back[1] << 9) |
                              (R.wall_back[2] << 10) | (R.wall_back[3] << 11)));
    key = mix(key, (uint32_t)(v->vx | (v->vy << 10)));
    /* the palette (camfx lerps it): a few entries the backdrop uses */
    key = mix(mix(key, g_rgb[PAL_PAINT(PAINT_BUBBLEGUM, 1)] ^ g_rgb[PAL_RAMP(RAMP_WOOD, 4)]),
              g_rgb[PAL_RAMP(RAMP_VAPE, 1)] ^ (g_rgb[PAL_RAMP(RAMP_SKY, 5)] << 1));
    for (i = 0; i < c->nlamp; i++) key = mix(key, (uint32_t)(c->lamp_x[i] | (c->lamp_y[i] << 12)));
    if (c->prest >= 0)
        key = mix(mix(key, (uint32_t)R.rp_x ^ 0x9E3779B9u),
                  (uint32_t)R.rp_y ^ ((uint32_t)R.rp_z << 20) ^ ((uint32_t)(uint16_t)R.pang << 4));
    for (i = 0; i < v->nobj; i++) {
        const DoomoRoomObj *o = &v->objs[i];
        const DoomoFurn *f = furn_of(o);
        /* the frame does not change a resting piece: a split one's frame
         * prims are drawn in the pass, a plain one has none */
        if (f && piece_kind(o, f) != PK_DYNAMIC) {
            key = mix(mix(mix(key, (uint32_t)i | ((uint32_t)o->furn << 8)),
                          (uint32_t)o->x | ((uint32_t)o->y << 8) | ((uint32_t)o->z << 16) |
                          ((uint32_t)RO_ROT(o) << 24)),
                      (uint32_t)o->paint_a | ((uint32_t)o->paint_b << 8) | ((uint32_t)o->flags << 16));
        }
    }
    return key;
}

/* the static layer, off screen into the lent pages; 1 = done */
static int fill_backdrop(RC *c, int dt, int bx, int by)
{
    const DoomoRoomView *v = c->v;
    TexRef wt, ft;
    DR_AREA *ar, *ar2;
    DR_OFFSET *of, *of2;
    int fx, fy, ok = 0;
    vram_wtex_lend(BD_PAGES);
    if (!r3_begin()) return 0;
    c->ot = g_cam.ot;
    c->areas = 0;
    ar = (DR_AREA *)gfx_alloc(sizeof(DR_AREA));
    of = (DR_OFFSET *)gfx_alloc(sizeof(DR_OFFSET));
    ar2 = (DR_AREA *)gfx_alloc(sizeof(DR_AREA));
    of2 = (DR_OFFSET *)gfx_alloc(sizeof(DR_OFFSET));
    if (ar && of && ar2 && of2 && room_textures(v, dt, &wt, &ft)) {
        draw_set(c, &wt, &ft, 1);
        /* drawn first: the lent area, its offset, the background */
        bg_tile(c, c->ot + OT_LEN - 1);
        setDrawOffset(of, bx - v->vx, by - v->vy);
        addPrim(c->ot + OT_LEN - 1, of);
        setDrawAreaXY(ar, bx, by, bx + c->vw - 1, by + c->vh - 1);
        addPrim(c->ot + OT_LEN - 1, ar);
        /* drawn last: back to the frame, at the post-FX shake's offset */
        gfx_fb_xy(&fx, &fy);
        setDrawAreaXY(ar2, fx + v->vx, fy + v->vy, fx + v->vx + c->vw - 1, fy + v->vy + c->vh - 1);
        addPrim(c->ot, ar2);
        setDrawOffset(of2, fx, fy);
        gfx_ofs_follow((uint32_t *)&of2->code[0]);
        addPrim(c->ot, of2);
        ok = 1;
    }
    r3_end();
    if (ok) {
        uint32_t *fl = (uint32_t *)gfx_alloc(8);           /* GP0(01h): texture cache */
        if (fl) { fl[0] = 0x01ffffffu; fl[1] = 0x01000000u; gfx_chain(fl); }
    }
    return ok;
}

void room_render(const DoomoRoomView *v, int force)
{
    RC c;
    int a, k, dt, i, H, zoom, punch, mode, bx, by;
    uint32_t t0 = timer_us(), key;
    int32_t lx, ly, lz, ex, ey, ez;
    El dyn[MAXEL];
    STab local, *tab = NULL;
    Clip dc[MAXEL], sc[MAXEL];
    int16_t lim[MAXEL];
    int nd = 0, ns = 0, moving;
    memset(&c, 0, offsetof(RC, in));
    r3_stats_reset();
    c.v = v;
    c.W = clampi(v->w, 1, 16);
    c.D = clampi(v->d, 1, 16);
    c.vw = v->vw ? v->vw : plat_screen_w();
    c.vh = v->vh ? v->vh : plat_screen_h();
    dt = (uint16_t)(v->time - R.time);
    R.time = v->time;
    a = camera_step(&c, dt);
    k = (((int)(v->yaw & 4095) + 512) >> 10) & 3;
    fit(&c, k);
    /* zoom: overview / close-up on the player, smoothed */
    {
        int want = v->zoom ? 410 : 256;
        int32_t tlx = (c.W * RT) << 15, tly = (c.D * RT) << 15;
        if (v->zoom)
            for (i = 0; i < v->nact; i++)
                if (v->actors[i].flags & RA_PLAYER) {
                    tlx = tlx / 5 + (v->actors[i].x * RT / 5) * 4;
                    tly = tly / 5 + (v->actors[i].y * RT / 5) * 4;
                }
        if (!R.zoom_q8 || dt > 40) { R.zoom_q8 = want; R.lx = tlx; R.ly = tly; }
        R.zoom_q8 += (want - R.zoom_q8) / 3;
        R.lx += (tlx - R.lx) / 3;
        R.ly += (tly - R.ly) / 3;
        zoom = R.zoom_q8;
        lx = R.lx; ly = R.ly; lz = LOOK_Z << 16;
    }
    punch = v->punch_q8 > 0 ? v->punch_q8 : 256;
    H = (int)(((((int64_t)R.fit_h * zoom) >> 8) * punch >> 8) * postfx_zoom_q8() >> 8);
    if (H < 64) H = 64;
    look_from(a, lx, ly, lz, &ex, &ey, &ez);
    r3_camera(ex, ey, ez, a, CAM_PITCH, v->vx, v->vy, c.vw, c.vh, H, CAM_FAR, CAM_FAR, 0);
    c.px_q8 = (int)(((int32_t)H * RU * 256) / CAM_DIST);
    {
        /* the fitted centre (scaled with the zoom) and the brake / pan shake */
        int dx = (R.fit_dx * zoom) >> 8, dy = (R.fit_dy * zoom) >> 8;
        if (v->zoom) dx = dy = 0;
        g_cam.cx += dx + v->pan_x;
        g_cam.cy += dy + v->pan_y;
        gte_SetGeomOffset(g_cam.cx, g_cam.cy);
    }
    c.cx = g_cam.px; c.cy = g_cam.py; c.cz = g_cam.pz;
    c.clx0 = v->vx; c.cly0 = v->vy; c.clx1 = v->vx + c.vw - 1; c.cly1 = v->vy + c.vh - 1;
    /* light: day colour, the key light turning with the camera */
    {
        int d = v->daylight > 15 ? 15 : v->daylight, rot = a - YAW_BASE;
        int l0x = 77, l0y = 141, cs = icos(rot), sn = isin(rot);
        c.amb_r = k_day[d][0]; c.amb_g = k_day[d][1]; c.amb_b = k_day[d][2];
        c.lx = (l0x * cs - l0y * sn) >> 12;
        c.ly = (l0x * sn + l0y * cs) >> 12;
        c.lz = 200;                     /* Q8: shade() gives -256..256 */
    }
    lamps(&c);
    /* the player at rest (Build mode stands it by the door, mostly behind
     * the furniture): after 2 frames on one spot it joins the backdrop with
     * its pose held (model.c g_model_hold), so what stands in front of it
     * is not drawn again every frame */
    c.prest = -1;
    for (i = 0; i < v->nact; i++)
        if (v->actors[i].flags & RA_PLAYER) {
            const DoomoRoomActor *pa = &v->actors[i];
            if (pa->x == R.rp_x && pa->y == R.rp_y && pa->z == R.rp_z) {
                if (R.still < 255) R.still++;
            } else {
                R.still = 0;
            }
            R.rp_x = pa->x; R.rp_y = pa->y; R.rp_z = pa->z;
            if (R.still >= 2) c.prest = i;
            break;
        }
    /* the backdrop: at the frame's dither phase (x and y mod 4) */
    bx = BD_X0 + (v->vx & 3);
    by = v->vy & 3;
    key = static_key(&c, a, H);
    moving = key != R.last_key;
    mode = RM_LIVE;
    if (force != RM_LIVE && c.vw <= 320 && by + c.vh <= 256 && v->vx >= 0 && v->vy >= 0) {
        if (R.bd_ok && key == R.bd_key && R.bd_up == vram_wtex_lent_uploads()) mode = RM_REPLAY;
        else if (key == R.last_key || R.prev_mode == RM_REPLAY || force == RM_FILL) mode = RM_FILL;
    }
    R.last_key = key;
    if (mode != RM_LIVE) {
        nd = dynamic_boxes(&c, dyn);
        if (nd < 0) mode = RM_LIVE;
    }
    if (mode != RM_LIVE) {
        /* a full render: the floor (a quad per tile) and the walls (their
         * columns, trims and slabs), then every piece and actor */
        int live = c.W * c.D + 4 * (c.W + c.D), cost;
        tab = (STab *)v->plat_cache;
        if (!tab || v->plat_cache_cap < sizeof(STab) || ((uintptr_t)tab & 3)) tab = &local;
        if (mode == RM_REPLAY && tab != &local && tab->magic == ST_MAGIC && tab->key == key && tab->n <= MAXEL) {
            ns = (int)tab->n;
        } else {
            ns = static_boxes(&c, tab);
            tab->n = (uint32_t)ns;
            static_adj(tab);
            tab->key = key;
            tab->magic = tab != &local ? ST_MAGIC : 0;
        }
        /* the work of the pass, against a full render's */
        for (i = 0; i < v->nobj; i++) {
            const DoomoFurn *f = furn_of(&v->objs[i]);
            if (f) live += 2 + f->nprim * 3;
        }
        for (i = 0; i < v->nact; i++) live += (v->actors[i].flags & RA_PLAYER) ? 40 : 2;
        cost = close_clips(&c, dyn, nd, dc, tab, sc, lim);
        if (force != RM_FILL && cost * 10 >= live * 8) mode = RM_LIVE;
    }
    if (mode == RM_LIVE) {
        /* one pass over everything, straight into the frame (while the
         * camera moves, with fewer sides and without the tiniest details) */
        TexRef wt, ft;
        c.lod = moving && force != RM_LIVE;
        bg_tile(&c, NULL);
        if (!r3_begin()) return;
        c.ot = g_cam.ot;
        if (room_textures(v, dt, &wt, &ft)) {
            draw_set(&c, &wt, &ft, 0);
            draw_actors_marks(&c);
        }
        r3_end();
    } else {
        if (mode == RM_FILL) {
            R.bd_ok = (uint8_t)fill_backdrop(&c, dt, bx, by);
            R.bd_key = key;
            R.bd_up = vram_wtex_lent_uploads();
        }
        blit_backdrop(&c, bx, by);
        if (nd > 0) run_pass(&c, dyn, nd, dc, tab, sc, lim);
    }
    R.prev_mode = (uint8_t)mode;
    /* the hell / possession tint (the overworld's full-view TILE) */
    if (v->tint != TINT_NONE && v->tint < TINT_COUNT && v->tint_level > 0) {
        static const uint8_t tint_rgb[TINT_COUNT][3] = {
            {0, 0, 0}, {200, 20, 20}, {220, 180, 40}, {40, 200, 60}, {150, 50, 210}, {0, 0, 0} };
        int lv = v->tint_level > 15 ? 15 : v->tint_level;
        TILE *t = (TILE *)gfx_alloc(sizeof(TILE));
        if (t) {
            if (v->tint == TINT_NIGHT) {
                int s = lv * 6;
                gfx_tpage(getTPage(0, 2, 0, 0));
                setTile(t);
                setColor0(t, r3_rgb(s * 2, s * 2, s));
            } else {
                int fl = ((v->time >> 2) & 3) == 0 ? 4 : 0;          /* the flicker */
                gfx_tpage(getTPage(0, 1, 0, 0));
                setTile(t);
                setColor0(t, r3_rgb(tint_rgb[v->tint][0] * (lv + fl) / 24, tint_rgb[v->tint][1] * (lv + fl) / 24,
                                    tint_rgb[v->tint][2] * (lv + fl) / 24));
            }
            setSemiTrans(t, 1);
            setXY0(t, v->vx, v->vy);
            setWH(t, c.vw, c.vh);
            gfx_chain(t);
        }
    }
    g_r3stats.polys = (uint16_t)(g_r3stats.polys + c.polys);
    g_r3stats.cpu_us = timer_us() - t0;
    g_r3stats.cells = (uint16_t)(mode | (nd << 4));
}

void plat_room(const DoomoRoomView *v)
{
    if (!v || plat_draw_skipped())
        return;
    if (g_room_test && g_room_test < ROOM_TEST_COUNT) {
        plat_code_part(PLAT_PART_HOME_ROOMTEST);    /* room_test.c, a part on the disc */
        room_test_render(v);
    } else {
        room_render(v, -1);
    }
}
COLD_END

#endif /* DOOMO_HAVE_CORE */
