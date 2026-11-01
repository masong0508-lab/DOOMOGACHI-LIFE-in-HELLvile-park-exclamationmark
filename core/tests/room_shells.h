/* room_shells.h - the room_ref parity frames, shared by the golden test
 * (core/tests/test_home_render.c) and the GBA harness (gba/src/room.c
 * room_hc_parity), so the two can never drift apart (owner: H4; H1 of the
 * house plan).
 *
 * Golden lines 0..4 (core/tests/golden/room_ref.txt) are the classic
 * parity layout: the Living Room template plus 12 pieces, views 0..3 and
 * view 0 at night, on the Living Room's default shell (wallpaper 0,
 * floor 0, no tint). Lines ROOM_SHELL_FIRST.. are the shell frames below:
 * every wallpaper, floor and ceiling ramp (each pattern in two views), the
 * five tints at the levels the game uses, the Garden and Park Plot kinds
 * (day, dusk, night), the non-square 8x6 room in its turned views, and a
 * few frames in the vocabulary the GBA already draws (lines 33..37).
 * Append new frames at the end only: the GBA harness addresses them by
 * golden line (gba/harness/scripts/home.txt).
 *
 * Header-only (static inline + one const table): the includer provides
 * home_int.h (HomeRoom, home_room_template, home_obj_view, home_room_defs)
 * and gen/furn_ids.h.
 */
#ifndef DOOMO_ROOM_SHELLS_H
#define DOOMO_ROOM_SHELLS_H

/* RoomShell.furn: what stands in the room */
enum {
    RSF_PARITY,     /* the classic layout (Living Room template + 12 pieces), resized */
    RSF_FLOOR,      /* a bare room (no door / windows) + the 9 floor-standing pieces  */
    RSF_EMPTY       /* a bare room: only the shell                                   */
};

typedef struct RoomShell {
    uint8_t w, d, kind, furn;                 /* size, ROOMK_*, RSF_*            */
    uint8_t wallpaper, floor, ceiling;        /* patterns 0..7, ramp 0..7        */
    uint8_t wall_paint, floor_paint;          /* PAINT_*                         */
    uint8_t tint, tint_level, daylight, view; /* TINT_*, 0..16, 0..15, k 0..3    */
} RoomShell;

#define ROOM_SHELL_FIRST 5                    /* golden line of room_shells[0]  */

static const RoomShell room_shells[] = {
    /* 5..12: wallpaper k, floor k, ceiling k on the parity layout, view k & 3 */
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 0, 0, 0, PAINT_ROSE,        PAINT_HONEY,     0, 0, 15, 0 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 1, 1, 1, PAINT_LAVENDER,    PAINT_CHOCOLATE, 0, 0, 15, 1 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 2, 2, 2, PAINT_MINT,        PAINT_WHITE,     0, 0, 15, 2 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 3, 3, 3, PAINT_CHARCOAL,    PAINT_RUST,      0, 0, 15, 3 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 4, 4, 4, PAINT_BUBBLEGUM,   PAINT_LIME,      0, 0, 15, 0 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 5, 5, 5, PAINT_BONE,        PAINT_SILVER,    0, 0, 15, 1 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 6, 6, 6, PAINT_SILVER,      PAINT_CARAMEL,   0, 0, 15, 2 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 7, 7, 7, PAINT_GRAPE,       PAINT_CHARCOAL,  0, 0, 15, 3 },
    /* 13..20: the off-diagonal pairs on a bare room (wallpaper k, floor k + 3,
     * ceiling k + 5, view k + 2), light falling from 15 to 3 */
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  0, 3, 5, PAINT_SKY,         PAINT_TANGERINE, 0, 0, 15, 2 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  1, 4, 6, PAINT_PEACH,       PAINT_FOREST,    0, 0, 15, 3 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  2, 5, 7, PAINT_TEAL,        PAINT_JET,       0, 0, 15, 0 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  3, 6, 0, PAINT_COBALT,      PAINT_BLOOD,     0, 0, 15, 1 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  4, 7, 1, PAINT_LEMON,       PAINT_NAVY,      0, 0, 12, 2 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  5, 0, 2, PAINT_CHERRY,      PAINT_BONE,      0, 0,  9, 3 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  6, 1, 3, PAINT_VAPE_PURPLE, PAINT_HELLFIRE,  0, 0,  6, 0 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  7, 2, 4, PAINT_SLIME,       PAINT_MAGENTA,   0, 0,  3, 1 },
    /* 21..25: the tints (home_draw.c: a possessed visit is red at level 4, a
     * ghost visit night at 8, the hell insert red at 16) */
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 4, 0, 0, PAINT_BUBBLEGUM,   PAINT_CARAMEL,   TINT_RED,    4, 15, 0 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 7, 7, 7, PAINT_GRAPE,       PAINT_CHARCOAL,  TINT_NIGHT,  8, 15, 1 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 0, 0, 0, PAINT_BUBBLEGUM,   PAINT_CARAMEL,   TINT_RED,   16, 15, 0 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 5, 2, 5, PAINT_BONE,        PAINT_MINT,      TINT_GOLD,  12,  9, 2 },
    { 6, 6, ROOMK_WALLS, RSF_PARITY, 1, 3, 6, PAINT_SKY,         PAINT_HONEY,     TINT_GREEN,  8, 15, 3 },
    /* 26..29: the open kinds: the Garden (8x6) at noon and at night, the Park
     * Plot (10x8) at noon and at dusk */
    { 8, 6, ROOMK_GARDEN, RSF_FLOOR, 4, 4, 0, PAINT_SKY,         PAINT_LIME,      0, 0, 15, 0 },
    { 8, 6, ROOMK_GARDEN, RSF_FLOOR, 4, 5, 0, PAINT_SKY,         PAINT_SILVER,    0, 0,  3, 1 },
    {10, 8, ROOMK_OPEN,   RSF_FLOOR, 4, 4, 0, PAINT_SKY,         PAINT_FOREST,    0, 0, 15, 1 },
    {10, 8, ROOMK_OPEN,   RSF_EMPTY, 4, 6, 0, PAINT_SKY,         PAINT_CHARCOAL,  0, 0, 10, 2 },
    /* 30..32: the 8x6 Kitchen shell turned (w != d), and the purple tint */
    { 8, 6, ROOMK_WALLS, RSF_PARITY, 2, 2, 0, PAINT_MINT,        PAINT_WHITE,     0, 0, 15, 1 },
    { 8, 6, ROOMK_WALLS, RSF_PARITY, 2, 2, 0, PAINT_MINT,        PAINT_WHITE,     0, 0, 15, 2 },
    { 8, 6, ROOMK_WALLS, RSF_PARITY, 6, 3, 2, PAINT_ROSE,        PAINT_COBALT,    TINT_PURPLE, 8, 15, 3 },
    /* 33..37: the vocabulary the GBA draws today (wallpapers 0..3, floors
     * 0..1, the wood cornice, no tint) on the non-square room, the open
     * kinds (the noon sky: the GBA has no dusk / night sky yet) and a dim
 * light, so its turned / open / dim paths stay checked
     * while the H2 frames above are expected to fail there */
    { 8, 6, ROOMK_WALLS, RSF_PARITY, 1, 1, 0, PAINT_LAVENDER,    PAINT_HONEY,     0, 0, 15, 1 },
    { 8, 6, ROOMK_WALLS, RSF_PARITY, 0, 0, 0, PAINT_BUBBLEGUM,   PAINT_CARAMEL,   0, 0,  9, 2 },
    { 8, 6, ROOMK_GARDEN, RSF_FLOOR, 0, 1, 0, PAINT_SKY,         PAINT_LIME,      0, 0, 15, 3 },
    {10, 8, ROOMK_OPEN,   RSF_FLOOR, 0, 0, 0, PAINT_SKY,         PAINT_FOREST,    0, 0, 13, 0 },
    { 6, 6, ROOMK_WALLS, RSF_EMPTY,  3, 1, 0, PAINT_COBALT,      PAINT_CHOCOLATE, 0, 0,  6, 1 },
};
#define ROOM_SHELL_COUNT ((int)(sizeof(room_shells) / sizeof(room_shells[0])))
#define ROOM_SHELL_LINES (ROOM_SHELL_FIRST + ROOM_SHELL_COUNT)   /* golden lines in all */

/* The classic layout's 12 pieces. Keep the order: the GBA harness's busy
 * path (room_hc_parity path 1) moves pieces by their index from the end. */
typedef struct RoomShellPiece { uint16_t furn; uint8_t x, y, rot, z, pa, pb, hung; } RoomShellPiece;
#define RS_DEF 0xFF                                /* the piece's own paint */
static const RoomShellPiece room_shell_pieces[] = {
    { FURN_CLOUD_BED,          4, 1, 0, 0, RS_DEF,         RS_DEF, 0 },
    { FURN_MARSHMALLOW_SOFA,   1, 4, 0, 0, PAINT_LAVENDER, RS_DEF, 0 },
    { FURN_TEACUP_TABLE,       2, 2, 0, 0, RS_DEF,         RS_DEF, 0 },
    { FURN_CUPCAKE_LAMP,       5, 4, 0, 0, RS_DEF,         RS_DEF, 0 },
    { FURN_DAISY_RUG,          3, 3, 0, 0, RS_DEF,         RS_DEF, 0 },
    { FURN_BUNNY_BEANBAG,      4, 4, 1, 0, PAINT_SKY,      RS_DEF, 0 },
    { FURN_RAINBOW_BOOKCASE,   2, 0, 0, 0, RS_DEF,         RS_DEF, 1 },
    { FURN_HEART_MIRROR,       3, 0, 0, 6, RS_DEF,         RS_DEF, 1 },
    { FURN_PIPS_BEE_DRAWING,   0, 3, 1, 8, RS_DEF,         RS_DEF, 1 },
    { FURN_JELLYBEAN_JAR,      2, 2, 0, 5, RS_DEF,         RS_DEF, 0 },
    { FURN_HORNED_POOP_PLUSH,  0, 5, 0, 0, RS_DEF,         RS_DEF, 0 },
    { FURN_POTTED_FERN,        5, 2, 0, 0, RS_DEF,         RS_DEF, 0 },
};

static inline void rs_put(HomeRoom *r, const RoomShellPiece *p)
{
    HomeObj *o = &r->obj[r->nobj++];
    o->w0 = HO_W0(p->furn, p->rot, p->z > 0);
    o->pos = HO_POS(p->x, p->y);
    o->z = p->z;
    o->pa = p->pa == RS_DEF ? FURN_PAINT_DEFAULT : p->pa;
    o->pb = p->pb == RS_DEF ? FURN_PAINT_DEFAULT : p->pb;
}

/* the classic parity room (golden lines 0..4) */
static inline void rs_parity_room(HomeRoom *r)
{
    int i;
    home_room_template(HOME_ROOM_LIVING, r);
    for (i = 0; i < (int)(sizeof(room_shell_pieces) / sizeof(room_shell_pieces[0])); i++)
        rs_put(r, &room_shell_pieces[i]);
}

/* the room of shell frame s (0..ROOM_SHELL_COUNT-1) */
static inline void rs_shell_room(int s, HomeRoom *r)
{
    const RoomShell *f = &room_shells[s];
    int i;
    if (f->furn == RSF_PARITY) {
        rs_parity_room(r);
    } else {
        memset(r, 0, sizeof(*r));
        r->wallpaper = r->floor = r->wall_paint = r->floor_paint = 0xFF;
        r->flags = HRF_OPEN | HRF_LIGHTS;
        if (f->furn == RSF_FLOOR)
            for (i = 0; i < (int)(sizeof(room_shell_pieces) / sizeof(room_shell_pieces[0])); i++)
                if (!room_shell_pieces[i].hung) rs_put(r, &room_shell_pieces[i]);
    }
    r->size = (uint8_t)(((f->w - 1) & 15) | (((f->d - 1) & 15) << 4));
}

/* The view of golden line `line` (0..4: the classic room, view line & 3,
 * line 4 at night; >= ROOM_SHELL_FIRST: its shell frame) over the room the
 * caller built with rs_parity_room / rs_shell_room. The caller sets the
 * viewport and plat_cache. */
static inline void rs_view(DoomoRoomView *v, DoomoRoomObj *objs, const HomeRoom *r, int line)
{
    int i;
    memset(v, 0, sizeof(*v));
    for (i = 0; i < r->nobj; i++) home_obj_view(&r->obj[i], &objs[i]);
    v->w = (uint8_t)HR_W(r);
    v->d = (uint8_t)HR_D(r);
    v->objs = objs;
    v->nobj = r->nobj;
    v->punch_q8 = 256;
    v->cursor_x = v->cursor_y = -1;
    if (line < ROOM_SHELL_FIRST) {
        v->wallpaper = home_room_defs[HOME_ROOM_LIVING].wallpaper;
        v->floor = home_room_defs[HOME_ROOM_LIVING].floor;
        v->wall_paint = home_room_defs[HOME_ROOM_LIVING].wall_paint;
        v->floor_paint = home_room_defs[HOME_ROOM_LIVING].floor_paint;
        v->room_kind = ROOMK_WALLS;
        v->yaw = (int16_t)((line & 3) * 1024);
        v->daylight = (uint8_t)(line < 4 ? 15 : 3);
    } else {
        const RoomShell *f = &room_shells[line - ROOM_SHELL_FIRST];
        v->wallpaper = f->wallpaper;
        v->floor = f->floor;
        v->ceiling = f->ceiling;
        v->wall_paint = f->wall_paint;
        v->floor_paint = f->floor_paint;
        v->room_kind = f->kind;
        v->tint = f->tint;
        v->tint_level = f->tint_level;
        v->daylight = f->daylight;
        v->yaw = (int16_t)(f->view * 1024);
    }
}

#endif
