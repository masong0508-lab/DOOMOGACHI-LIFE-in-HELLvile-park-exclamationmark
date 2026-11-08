/* room_int.h - the dream house on the Dreamcast (DC8; owner: dreamcast).
 *
 * plat_room() and plat_room_project() on the PVR, through the 2D layer's
 * one presorted list (dc2d.h): room.c. Portable C (no KallistiOS), so the
 * host tests (dc/tests/gfx2d: test_dc_room.c, the runner's tee) build it
 * too, with -DDC2D_HOST giving the two calls dc2d_ names. The design and
 * the rules are in room.c's header and docs/DREAMCAST.md section 8.
 */
#ifndef DC_ROOM_INT_H
#define DC_ROOM_INT_H

#include <stdint.h>
#include "dc2d.h"
#include "doomo/types.h"
#include "doomo/home.h"

/* Counters (also in the harness mailbox: dc_mailbox.room_*, main.c). */
typedef struct DcRoomStats {
    uint32_t us_last, us_max, us_sum, n;   /* plat_room's SH4 time per drawn view (dc3d_be_us) */
    uint32_t shells, shell_us_max;         /* shell textures rendered, the slowest (us)          */
    uint32_t decodes;                      /* piece sprites decoded and uploaded                 */
    uint32_t items, items_max;             /* sorted pieces + actors: the last view, the most    */
    uint32_t prims, prims_max;             /* prims the view added to the frame's list           */
    uint32_t boxes;                        /* pieces drawn as shaded boxes (no sprite), total    */
} DcRoomStats;
extern DcRoomStats dc_room_stats;

/* The platform calls (plat_room / plat_room_project on the console). */
void DC2D_FN(room)(const DoomoRoomView *v);
void DC2D_FN(room_project)(const struct DoomoRoomView *v, fix16 x, fix16 y, int z,
                           int16_t *sx, int16_t *sy);

/* Forget the shell, the piece table and the zoom (tests). */
void dc_room_reset(void);

/* The shell texture of the last view: its image (pixels, size, uid) and
 * where its top-left lands on the logical screen at pan 0. 0 = none yet. */
int  dc_room_shell(DoomoImage *img, int *x, int *y);

/* The view transform of the last view: logical point p is drawn at
 * c + s * (p - f) (the room's zoom, DoomoRoomView.zoom 1 / 2). Returns 0
 * when it is the identity (zoom 0 at rest). */
int  dc_room_xform(float *s, float *fx, float *fy, float *cx, float *cy);

/* room_ref.c's topological order of the floor items (its Item, its
 * behind() and its pick rule), computed in O(n^2) (Kahn's algorithm with
 * room_ref's priority) instead of room_ref's O(n^3) search. order[] gets
 * the item indices in drawing order. Exposed for the equivalence test. */
typedef struct DcRoomItem {
    int8_t  x0, y0, x1, y1;
    uint8_t layer, z;
    int16_t idx;          /* obj index, or -1 - actor */
} DcRoomItem;
#define DC_ROOM_MAX_ITEMS (HOME_OBJ_MAX + 32)
void dc_room_sort(const DcRoomItem *it, int n, int16_t *order);

#endif
