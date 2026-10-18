/* home_int.h - private glue of the dream house (owners: H1 for the state,
 * the chunk, the editor rules and the score; H4 for the scene). Not part
 * of the contract: story code uses house_api.h and scene_push(&SCENE_HOME).
 *
 * Files: home.c (state, 'HOME' chunk, grants, move-in, the cache),
 * house_api.c (resident getters, reads/writes HOME .bss only, SPEC C6),
 * home_edit.c (catalog access, placement rules, stacking, undo ring),
 * home_score.c (the four stats + Barricade, feng shui, grades),
 * home_draw.c (SCENE_HOME, H4), room_ref.c (the reference renderer, H4).
 */
#ifndef DOOMO_HOME_INT_H
#define DOOMO_HOME_INT_H

#include "doomo/doomo.h"
#include "doomo/home.h"
#include "doomo/furn.h"
#include "doomo/house_api.h"
#include "doomo/save_budget.h"

/* The HOME .bss lands on the GBA only once the memory pass (task #77, G0)
 * has freed EWRAM (SPEC C9: post-G0 >= 12,352 B free; risk 2: "H1 M1 can
 * land behind a build flag"). Until then the GBA keeps no house state: the
 * scene edits a starter room in the scene scratch and nothing is saved.
 * Build with -DDOOMO_HOME_ON_GBA=1 to link it anyway. */
#if defined(DOOMO_PLATFORM_GBA) && !defined(DOOMO_HOME_ON_GBA)
#define HOME_PERSIST 0
#else
#define HOME_PERSIST 1
#endif

/* ---- saved records (HF sec.9.2 with SPEC R9/R10/C9) ---------------------- */
typedef struct HomeObj {
    uint16_t w0;            /* bits 0-8 furn, 9-10 rot, 11 on_surface, 12-15 sub-pos */
    uint8_t  pos;           /* lo nibble x, hi nibble y                              */
    uint8_t  z;             /* px/4: stackers = surface top, wall pieces = height    */
    uint8_t  pa, pb;        /* lo 5 bits PAINT_* (0x1F = catalog default), 5-7 state */
} HomeObj;                  /* 6 B */
#define HO_FURN(o)    ((o)->w0 & 0x1FF)
#define HO_ROT(o)     (((o)->w0 >> 9) & 3)
#define HO_ONSURF(o)  (((o)->w0 >> 11) & 1)
#define HO_X(o)       ((o)->pos & 15)
#define HO_Y(o)       ((o)->pos >> 4)
#define HO_W0(furn, rot, surf) ((uint16_t)(((furn) & 0x1FF) | (((rot) & 3) << 9) | (((surf) & 1) << 11)))
#define HO_POS(x, y)  ((uint8_t)(((x) & 15) | (((y) & 15) << 4)))
#define HOS_OFF       0x20  /* pa state: a lamp switched off                     */
#define HOS_OPEN      0x40  /* pa state: a door open / curtains drawn            */

typedef struct HomeRoom {
    uint8_t wallpaper, floor;        /* pattern 0..31, 0xFF = the room's default    */
    uint8_t wall_paint, floor_paint; /* PAINT_*, 0xFF = default                     */
    uint8_t size;                    /* lo nibble w-1, hi nibble d-1                */
    uint8_t flags;                   /* HRF_*                                        */
    uint8_t nobj, name, window_skins, door_skin;
    HomeObj obj[HOME_OBJ_MAX];
} HomeRoom;                          /* 250 B in RAM; nobj x 6 + 10 on disk          */
DOOMO_STATIC_ASSERT(sizeof(HomeRoom) == HOME_SZ_ROOM_MAX, home_room_is_250_bytes);
enum { HRF_LIGHTS = 1, HRF_CURTAINS = 2, HRF_HAUNTED = 4, HRF_PLATED = 8,
       HRF_CEIL_SHIFT = 4, HRF_CEIL_MASK = 0x70, HRF_OPEN = 0x80 /* unlocked */ };
#define HR_W(r) (((r)->size & 15) + 1)
#define HR_D(r) (((r)->size >> 4) + 1)

/* ver_flags */
enum { HVF_MOVED_IN = 1, HVF_ANNEX = 0x10, HVF_SOUTH_FENCE = 0x20, HVF_TOUCHED = 0x80 };

/* The cache served by house_api.c without the home group (C6, C9). */
enum { HC_STAT = 0,                              /* HS_COUNT stats of the active room */
       HC_STAMPS = HS_COUNT,                     /* HOME_ROOM_COUNT stamp bitmasks    */
       HC_DECOR = HS_COUNT + HOME_ROOM_COUNT,    /* last decorate_for progress        */
       HC_DEFEND = HS_COUNT + HOME_ROOM_COUNT + 1 };
#define HOME_STAMP(g) (1u << ((g) - 1))          /* HG_COSY .. HG_ETERNAL -> bits 0..4 */

typedef struct HomeState {
    uint8_t  ver_flags, active_room;
    uint16_t house_points;
    uint8_t  theme, defence;
    uint16_t days_since_fire;                    /* header: 8 B                       */
    HomeRoom room[HOME_ROOM_COUNT];
    uint8_t  owned[HOME_SZ_OWNED];               /* nibble counts                     */
    uint8_t  seen_new[HOME_SZ_SEEN_NEW];         /* "NEW!" bits                       */
    uint8_t  gifts[HOME_GIFTS][4];               /* left in other homes (H4 M2)       */
    uint8_t  visited[HOME_SZ_VISITED];           /* 1000 bits                         */
    uint8_t  haunt[HOME_SZ_HAUNT];               /* Ch2: [0] night, [1] moved piece, [2] eye pos */
    uint8_t  wants[HOME_SZ_WANTS];
    uint8_t  vlog[HOME_SZ_VISITOR_LOG];
    uint8_t  decorate_for;
    uint16_t gag[HOME_PENDING_GAGS];             /* pending "Later" prop gags         */
    uint8_t  cache[HOME_SZ_CACHE];
    /* The 2 saved blueprints (500 B) and the 16 photo recipes (128 B) are
     * H1 M3: the chunk already writes their slots (empty), so M3 adds them
     * here without a format change. */
} HomeState;

/* The house's unlock ids (unlocks_home.c; UL_IDS_HOME_FIRST..LAST, R7):
 * 20 trophies, then 12 raw bits that are not in the table. */
enum { HUL_FIRST_HOUSE = UL_IDS_HOME_FIRST, HUL_COSY, HUL_LIT, HUL_HELD, HUL_HOME,
       HUL_PIECES_100, HUL_EVERY_CAT, HUL_ALL_ROOMS, HUL_VISIT_50, HUL_VISIT_500,
       HUL_VISIT_1000, HUL_FIVE_STARS, HUL_PHOTOS_10, HUL_BLUEPRINT, HUL_HARMONY_100,
       HUL_BIG_SPOON, HUL_STARER_20, HUL_SEASONAL_SET, HUL_PLAIN_HUMAN, HUL_EVERY_GAG,
       HUL_TROPHY_END,                             /* = 584                     */
       HUL_RAW_TUTORIAL = HUL_TROPHY_END,          /* 584..589 room tutorials   */
       HUL_RAW_DEFEND_TUTORIAL = HUL_TROPHY_END + 6,
       HUL_RAW_BLACK_MARKET, HUL_RAW_LAST = UL_IDS_HOME_LAST };

/* The live state: NULL where it is not linked (the GBA before G0). */
HomeState *home_state(void);

/* ---- the fixed room templates (home.c) ------------------------------------- */
enum { HW_A, HW_B };                 /* wall A: y = 0 (upper right), B: x = 0 (upper left) */
typedef struct HomeRoomDef {
    const char *name;
    uint8_t w, d, kind;              /* ROOMK_*                                      */
    uint8_t door_wall, door_at;      /* the fixed door slot                          */
    uint8_t nwin;
    uint8_t win_wall[2], win_at[2];  /* window slots                                 */
    uint8_t hearth_at;               /* along wall B, 0xFF = none (the Living Room)  */
    uint8_t wallpaper, floor, wall_paint, floor_paint;   /* defaults                 */
    uint8_t dim;                     /* light level before lamps (Cellar 6), 0 = day */
} HomeRoomDef;
extern const HomeRoomDef home_room_defs[HOME_ROOM_COUNT];

/* ---- house_api.c extras (resident) --------------------------------------- */
int  home_remove_only_on(void);

/* ---- home.c ---------------------------------------------------------------- */
void home_reset(void);               /* the chunk's reset()                         */
void home_move_in(void);             /* moved in + the Living Room with its fixed slots */
void home_unlock_room(int room);
int  home_room_open(int room);
/* Copy a room (from the state, or the room's empty template when there is
 * no state) / commit a working copy back. */
void home_room_load(int room, HomeRoom *out);
void home_room_store(int room, const HomeRoom *in);
/* An empty room of `room`'s template with its fixed pieces (door, windows,
 * hearth) placed. */
void home_room_template(int room, HomeRoom *out);
/* Owned count of `furn` (0..15) and how many are placed in the rooms
 * (the working copy `work` of room `work_room` replaces that room). */
int  home_owned(int furn);
int  home_placed(int furn, const HomeRoom *work, int work_room);
void home_take_grants(void);         /* unlock-granted pieces -> storage (at load) */
/* Re-score every open room and store the stats, stamps and trophies in the
 * cache (the scene calls it on leave and after each placement). */
void home_refresh_cache(const HomeRoom *work, int work_room);
int  home_serialize_size(void);      /* exactly what the chunk writes               */
uint32_t home_chunk_write(uint8_t *dst, uint32_t cap);
int  home_chunk_read(const uint8_t *src, uint32_t len, uint16_t version);
extern const DoomoSaveExt home_ext;

/* ---- home_edit.c: catalog access, placement, undo ---------------------------- */
const DoomoFurn *home_furn(int furn);          /* NULL for a bad id                */
int  home_furn_count(void);
int  home_furn_by_role(int role);               /* first piece with that role, -1   */
/* tests: run the rules on a hand-written catalog (NULL = the generated one) */
void home_test_catalog(const DoomoFurn *rows, int n);

/* Placement result codes (the "nope" reasons the HUD explains). */
enum { HPR_OK, HPR_OFFGRID, HPR_OCCUPIED, HPR_WALL_ONLY, HPR_FLOOR_ONLY,
       HPR_FIREPLACE, HPR_FIXED, HPR_FULL, HPR_NOT_OWNED, HPR_SLOT_ONLY,
       HPR_REMOVE_ONLY, HPR_DOOR, HPR_COUNT };
extern const char *const home_nope_text[HPR_COUNT];

typedef struct HomeFit {
    uint8_t z;              /* resolved height px/4 (stackers: the surface top)   */
    uint8_t on_surface;
    int8_t  surface;        /* index of the surface it sits on, -1               */
    int8_t  replaces;       /* a window slot swap: the index it replaces, -1     */
} HomeFit;
/* Footprint of a piece at rotation `rot` (odd rotations swap w and d). */
void home_footprint(const DoomoFurn *f, int rot, int *w, int *d);
/* Can `furn` go at tile (x, y) with `rot` in room `r` (template `room`)?
 * `ignore` is an object index that is being moved (-1). */
int  home_fit(const HomeRoom *r, int room, int furn, int x, int y, int rot,
              int ignore, HomeFit *out);
/* Topmost object covering tile (x, y) (wall pieces count on their wall
 * row/column), -1 none. */
int  home_obj_at(const HomeRoom *r, int x, int y);
int  home_obj_is_fixed(const HomeRoom *r, int idx);
int  home_stackers_on(const HomeRoom *r, int idx);   /* smalls sitting on it    */

/* The editing session: a working copy of one room plus the undo ring
 * (HF sec.9.3). Lives in the scene scratch, never saved. */
enum { HU_NONE, HU_PLACE, HU_REMOVE, HU_MOVE };
typedef struct HomeUndo { uint8_t op, idx; uint8_t old[6]; } HomeUndo;   /* 8 B */
#define HOME_UNDO_MAX 32
typedef struct HomeEdit {
    HomeRoom room;
    uint8_t  room_id, undo_head, undo_n, remove_only;
    HomeUndo undo[HOME_UNDO_MAX];
    uint16_t gen;            /* bumped on every change (renderer cache key)     */
} HomeEdit;
void home_edit_begin(HomeEdit *e, int room);
/* Place / pick up / move; each pushes one undo step and bumps gen.
 * place returns the new index or -(HPR_*); pickup returns the furn id or
 * -(HPR_*) (fixed pieces, surfaces with smalls on them). */
int  home_edit_place(HomeEdit *e, int furn, int x, int y, int rot, int pa, int pb);
int  home_edit_pickup(HomeEdit *e, int idx);
int  home_edit_move(HomeEdit *e, int idx, int x, int y, int rot);
int  home_edit_undo(HomeEdit *e);          /* 1 undone, 0 ring empty             */
/* Available to place: owned minus placed (fixed slot pieces are free). */
int  home_edit_avail(const HomeEdit *e, int furn);

/* ---- home_score.c (HF sec.4) --------------------------------------------- */
enum { HSC_NIGHT = 1, HSC_CH3 = 2, HSC_CH4 = 4 };      /* context bits             */
/* feng-shui rules that fired (the Score panel lists them) */
enum { HRULE_FIREPLACE_CLEAR = 1u << 0, HRULE_BED_NOT_DOOR = 1u << 1,
       HRULE_BED_FACES_DOOR = 1u << 2, HRULE_LAMP_NEAR_BED = 1u << 3,
       HRULE_PLUSH_ON_BED = 1u << 4, HRULE_MIRROR_WINDOW = 1u << 5,
       HRULE_PENTAGRAM_BED = 1u << 6, HRULE_TABLES_TOGETHER = 1u << 7,
       HRULE_MIXED_PAIRS = 1u << 8, HRULE_KITCHEN_OUTSIDE = 1u << 9,
       HRULE_PLAIN_HUMAN = 1u << 10, HRULE_BED_UNREACHABLE = 1u << 11,
       HRULE_CLUTTER = 1u << 12 };
typedef struct HomeScore {
    uint8_t  stat[HS_COUNT];     /* 0..100                                      */
    uint8_t  stamps;             /* HOME_STAMP(HG_*) bits this room earns         */
    uint8_t  pieces, seats, lamps, beds, hell, hill, cursed, turret;
    uint16_t rules;              /* HRULE_*                                      */
} HomeScore;
void home_score_room(const HomeRoom *r, int room, unsigned ctx, HomeScore *out);
/* The grade a stamp bitmask shows (highest HG_*). */
int  home_stamps_grade(unsigned stamps);

/* ---- H4: room_ref.c (the reference renderer) and home_draw.c ---------------- */
/* The whole room, full redraw (the weak plat_room() calls it). */
void room_ref_draw(const DoomoRoomView *v);
/* Decode facing `facing` (0 / 1) of animation frame `frame` of `furn` into
 * buf (cap bytes), recoloured to paints pa / pb (-1 = the markers' own
 * shades) and lit at level lvl (15 = fullbright). (*ox, *oy) is where the
 * footprint's back corner lands in the image. 0 = no sprite (the PS1 links
 * none) or too big for buf. */
int  room_ref_sprite(int furn, int facing, int frame, int pa, int pb, int lvl,
                     uint8_t *buf, unsigned cap, DoomoImage *img, int *ox, int *oy);
/* Fill a render-ready DoomoRoomObj from a room object (catalog paints
 * resolved, z in px). */
void home_obj_view(const HomeObj *o, DoomoRoomObj *out);

#endif
