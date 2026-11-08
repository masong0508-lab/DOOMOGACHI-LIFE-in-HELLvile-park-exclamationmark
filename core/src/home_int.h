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

/* (H11) The HOME .bss is on every platform, the GBA included: the memory
 * pass (G0) freed the EWRAM it needs (SPEC C9: >= 12,352 B free before it,
 * >= 8,192 B after; docs/GBA.md section 5). The GBA no longer edits a starter
 * room in the scene scratch that is never saved: the house state is kept
 * and saved like on the other builds. HOME_PERSIST stays as the one switch
 * the code tests; build with -DDOOMO_HOME_OFF_GBA=1 to take the state off the
 * GBA again if EWRAM ever gets that tight. */
#if defined(DOOMO_PLATFORM_GBA) && defined(DOOMO_HOME_OFF_GBA)
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
/* (H1 M3) the half-tile sub-position of a small on a surface: bit 0 = +1/2
 * tile x, bit 1 = +1/2 tile y (w0 bits 12-13; 14-15 stay reserved) */
#define HO_SUB(o)      (((o)->w0 >> 12) & 3)
#define HO_W0S(furn, rot, surf, sub) ((uint16_t)(HO_W0(furn, rot, surf) | (((sub) & 3) << 12)))
#define HO_POS(x, y)  ((uint8_t)(((x) & 15) | (((y) & 15) << 4)))
#define HOS_OFF       0x20  /* pa state: a lamp switched off                     */
#define HOS_OPEN      0x40  /* pa state: a door open / curtains drawn            */

/* (H4) door_skin / window_skins record the fittings: the rank of the door
 * piece among the catalog's FR_DOOR pieces (0 = the first, the Pink Front
 * Door) and of the windows' piece among the FR_WINDOW pieces (0 = the Wood
 * Window; the Walls menu and the themes skin every window slot alike, the
 * first slot's is the one kept), HSK_NONE where the room has no such slot.
 * The pieces in the room are the truth; home_room_skins_sync() writes the
 * bytes, and a saved blueprint (no fixed pieces) carries them. */
typedef struct HomeRoom {
    uint8_t wallpaper, floor;        /* pattern 0..31, 0xFF = the room's default    */
    uint8_t wall_paint, floor_paint; /* PAINT_*, 0xFF = default                     */
    uint8_t size;                    /* lo nibble w-1, hi nibble d-1                */
    uint8_t flags;                   /* HRF_*                                        */
    uint8_t nobj, name, window_skins, door_skin;
    HomeObj obj[HOME_OBJ_MAX];
} HomeRoom;                          /* 250 B in RAM; nobj x 6 + 10 on disk          */
DOOMO_STATIC_ASSERT(sizeof(HomeRoom) == HOME_SZ_ROOM_MAX, home_room_is_250_bytes);
#define HSK_NONE 0xFF                /* door_skin / window_skins: no such slot      */
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
    uint8_t  haunt[HOME_SZ_HAUNT];               /* Ch2: HH_* below                    */
    uint8_t  wants[HOME_SZ_WANTS];
    uint8_t  vlog[HOME_SZ_VISITOR_LOG];
    uint8_t  decorate_for;
    uint16_t gag[HOME_PENDING_GAGS];             /* pending "Later" prop gags         */
    uint8_t  cache[HOME_SZ_CACHE];
    HomeRoom bp[HOME_BLUEPRINTS];                /* the 2 saved blueprints (nobj 0 = empty) */
    uint8_t  photo[HOME_PHOTOS][8];              /* 8-byte photo recipes (home_photo_*) */
} HomeState;

/* HomeState.haunt[] (the Ch2 haunting, HF sec.10.2; the story advances
 * HH_NIGHT through house_api.c, the home group applies it) */
enum { HH_NIGHT, HH_APPLIED, HH_EYE, HH_FLAGS, HH_MOVES };
enum { HHF_LAMP_OFF = 1, HHF_PENTA = 2, HHF_ENDED = 4, HHF_EYE = 8 };

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
    /* (H4) the fittings the room comes with: its door and window skins
     * (FR_DOOR / FR_WINDOW FF_PORTAL pieces) and its ceiling tint 0..7 */
    uint16_t door_furn, win_furn;
    uint8_t ceiling;
} HomeRoomDef;
extern const HomeRoomDef home_room_defs[HOME_ROOM_COUNT];

/* ---- house_api.c extras (resident) --------------------------------------- */
int  home_remove_only_on(void);

/* ---- home.c ---------------------------------------------------------------- */
void home_reset(void);               /* the chunk's reset()                         */
void home_move_in(void);             /* moved in + the Living Room with its fixed slots */
void home_unlock_room(int room);
void home_story_rooms(void);        /* (H4 M2) open the rooms the story has reached */
int  home_room_open(int room);
/* Copy a room (from the state, or the room's empty template when there is
 * no state) / commit a working copy back. */
void home_room_load(int room, HomeRoom *out);
void home_room_store(int room, const HomeRoom *in);
/* An empty room of `room`'s template with its fixed pieces (door, windows,
 * hearth) placed. */
void home_room_template(int room, HomeRoom *out);
/* (H4) The room as the player first gets it: the template plus the designed
 * pieces of Towers 7B (the tenant's things under dust sheets, S114) and the
 * Hell Pits flat (the landlord's basics). home_unlock_room() places them and
 * grants each one, so what you clear goes to storage; home_room_load() opens
 * those two homes the first time they load. Visits keep the bare template. */
void home_room_default(int room, HomeRoom *out);
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
/* HU_MOVE restores the whole object (moves, rotations and recolours); HU_WALL
 * the room header (old = wallpaper, floor, wall_paint, floor_paint, flags);
 * HU_TMASK_A / B a theme's recolour masks (idx = bits 32-39, old[0..3] =
 * bits 0-31: those slots go back to the catalog default); HU_GROUP closes a
 * step of idx entries (a group move, a theme, a paste), undone as one. */
enum { HU_NONE, HU_PLACE, HU_REMOVE, HU_MOVE, HU_WALL, HU_TMASK_A, HU_TMASK_B, HU_GROUP };
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

/* (H4) home_edit.c's helpers that home_build.c (the Build-mode operations,
 * the PS1's `build` part) shares: a footprint box in tiles (x1 / y1
 * exclusive), the layer an object lives in, a wall piece's span (returns 1
 * on wall B), one undo entry, and taking object idx out of a room */
typedef struct HomeBox { int x0, y0, x1, y1; } HomeBox;
enum { HOL_RUG, HOL_FLOOR, HOL_STACK, HOL_WALL };
HomeBox home_obj_box(const HomeObj *o);
int  home_obj_layer(const HomeObj *o);
int  home_wall_span(const HomeRoom *r, const HomeObj *o, int *a0, int *a1, int *z0, int *z1); /* the edge, HE_* (H5) */
void home_edit_push(HomeEdit *e, int op, int idx, const HomeObj *old);
void home_room_remove(HomeRoom *r, int idx);
int  home_is_skin(const DoomoFurn *f, int role);   /* an FF_PORTAL piece of that role */

/* ---- H1 M3: half tiles, recolours, walls, themes, groups, the clipboard -------
 * (H4: the walls, themes, groups, the clipboard and the blueprints live in
 * home_build.c; the PS1 loads them as a part, PLAT_PART_HOME_BUILD) */
int  home_fit_sub(const HomeRoom *r, int room, int furn, int x, int y, int rot, int sub,
                  int ignore, HomeFit *out);
int  home_edit_place_sub(HomeEdit *e, int furn, int x, int y, int rot, int sub, int pa, int pb);
int  home_edit_paint(HomeEdit *e, int idx, int pa, int pb);          /* PAINT_* or -1 default */
int  home_edit_walls(HomeEdit *e, int wallpaper, int floor, int wall_paint, int floor_paint);
int  home_edit_theme(HomeEdit *e, int theme);      /* recolours the free paint slots     */
int  home_edit_lights(HomeEdit *e, int idx);       /* a lamp on / off (no undo)          */
#define HOME_SEL_MAX 8
/* the non-fixed pieces whose footprints lie inside tiles (x0, y0)..(x1, y1)
 * inclusive, with the smalls stacked on them; at most cap (8) */
int  home_edit_select(const HomeEdit *e, int x0, int y0, int x1, int y1, uint8_t *out, int cap);
int  home_edit_group_move(HomeEdit *e, const uint8_t *idx, int n, int dx, int dy);
int  home_edit_group_rotate(HomeEdit *e, const uint8_t *idx, int n, int dir);
int  home_edit_group_remove(HomeEdit *e, const uint8_t *idx, int n);   /* to storage */
typedef struct HomeClip { uint8_t n; HomeObj o[HOME_SEL_MAX]; } HomeClip;   /* relative pos */
int  home_edit_copy(const HomeEdit *e, const uint8_t *idx, int n, HomeClip *c);
int  home_edit_paste(HomeEdit *e, const HomeClip *c, int x, int y);    /* pieces placed */

/* ---- H4: door and window skins (themes.txt `door` / `window`) -------------------
 * The fixed door and the window slots keep their place but change their piece:
 * any FF_PORTAL piece with role FR_DOOR / FR_WINDOW. The rooms' own fittings
 * (home_room_defs[]) and the themes' are free; the others (HomeMart's Heart
 * Door and windows, gifts) once you own one. Without a house state (the GBA
 * showroom) every skin is free. */
int  home_skin_free(int furn);                 /* a room's or a theme's fitting  */
int  home_skin_ok(int furn, int role);         /* may go on that slot now        */
int  home_skin_next(int role, int cur, int dir);   /* the next one (wraps), cur when alone */
int  home_room_door(const HomeRoom *r);         /* index of the door piece, -1    */
int  home_room_window(const HomeRoom *r);       /* index of the first window, -1  */
int  home_skin_code(int furn, int role);        /* rank among the role's pieces, HSK_NONE */
int  home_skin_from_code(int code, int role);   /* the piece, -1                  */
void home_room_skins_sync(HomeRoom *r);         /* door_skin / window_skins       */
/* Re-skin the door and every window slot (-1 keeps it) as one undo step;
 * returns the pieces changed. */
int  home_edit_skins(HomeEdit *e, int door, int window);
/* The Walls menu's "Paint it!": walls, floor and both fittings, one undo step. */
int  home_edit_fixtures(HomeEdit *e, int wallpaper, int floor, int wall_paint, int floor_paint,
                        int door, int window);

/* ---- H1 M3: blueprints (12 ROM + 2 saved) ---------------------------------- */
enum { HPS_PLACED, HPS_MISSING, HPS_BLOCKED };
typedef struct HomeGhost { uint16_t furn; uint8_t x, y, rot, status; } HomeGhost;
typedef struct HomePlan {
    uint8_t first;          /* index of the first piece placed (the montage order)  */
    uint8_t placed, missing, blocked, nghost;
    HomeGhost ghost[HOME_OBJ_MAX];   /* what could not be placed (grey outlines)   */
} HomePlan;
/* Apply blueprint `bp` (0..11 ROM, 12..13 the saved slots): the room's own
 * pieces go to storage, the walls take the blueprint's theme, then every
 * piece you own is placed; the rest become ghosts (a shopping list). A
 * blueprint is a fresh start: it clears the undo ring. */
int  home_edit_blueprint(HomeEdit *e, int bp, HomePlan *plan);
int  home_blueprint_count(void);                  /* ROM + saved slots in use     */
const char *home_blueprint_name(int bp);          /* NULL: empty slot             */
int  home_blueprint_save(int slot, const HomeRoom *r);   /* 1 saved (0..1)       */

/* ---- H1 M3: decorate-for (layouts_*.txt, SPEC R19) ------------------------- */
const DoomoHomeSet *home_layout_find(int id);
/* how close room `r` is to layout `id`: 0..8 (8 = done); tol < 0 = the
 * layout's own tolerance. Stores layout << 4 | progress for house_api.c. */
int  home_layout_progress(const HomeRoom *r, int id, int tol);

/* ---- H1 M3: photos (8-byte recipes, HF sec.2.3) ----------------------------- */
#define HOME_PHOTO_OWN 1000          /* place >= this: HOME_ROOM_* + 1000            */
typedef struct HomePhoto {
    uint16_t place;          /* a citizen id 0..999 (a visit) or 1000 + HOME_ROOM_* */
    uint8_t  view, zoom, filter, daylight;      /* 0..3, 0..1, 0..7, 0..7            */
    uint8_t  sticker[3], sx[3], sy[3];          /* kind 0..3 (0 none), x 0..15, y 0..7 */
    uint16_t hash;           /* the layout's hash when it was taken                 */
} HomePhoto;
void     home_photo_pack(const HomePhoto *p, uint8_t out[8]);
int      home_photo_unpack(const uint8_t in[8], HomePhoto *p);   /* 0: an empty slot */
uint16_t home_room_hash(const HomeRoom *r);
int      home_photo_add(const HomePhoto *p);       /* the slot, -1 when all 16 full */
int      home_photo_get(int slot, HomePhoto *p);
void     home_photo_delete(int slot);
int      home_photo_count(void);

/* ---- H1 M3: the Ch2 haunting (home.c; HF sec.10.2, rule of three) ------------ */
enum { HHA_CRATE = 1, HHA_LAMP = 2, HHA_EYE = 4, HHA_TWO = 8, HHA_PENTA = 16, HHA_ENDED = 32 };
/* Apply the nights the story has reached (HH_NIGHT) and not yet applied, to
 * the Living Room (`work` when it is the room being edited) and the Bedroom's
 * bed; returns HHA_* bits. Reaching LIT ends the moves: the eye stays. */
int  home_haunt_apply(HomeRoom *work, int room);
int  home_haunt_3am(HomeRoom *work, int room);    /* the live 3 AM move: index or -1 */
int  home_haunt_eye_blinks(void);

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

/* ---- story_rng contexts of the house (SPEC C18: unique, documented) --------- */
#define HOME_CTX_DEFEND 0x484F4D01u      /* "HOM" 1: the defence waves             */
#define HOME_CTX_VISIT  0x484F4D02u      /* 2: generated rooms (home_visit.c)      */
#define HOME_CTX_WANTS  0x484F4D03u      /* 3: the weekly room wants               */
#define HOME_CTX_MART   0x484F4D04u      /* 4: HomeMart's daily stock and deal     */
#define HOME_CTX_HAUNT  0x484F4D05u      /* 5: the Ch2 haunting's picks            */

/* ---- home_defend.c: Hold the Door (H1 M2; HF sec.10.3, SPEC R11) ------------
 * The state lives in the scene scratch (SCENE_HOME); the scene draws it. */
#define HOME_DEFEND_WAVES 4
#define HD_DEMONS 16
enum { HDK_IMP, HDK_BRUTE };
enum { HDS_NONE, HDS_WALK, HDS_HIT, HDS_STARE, HDS_INSIDE, HDS_FLEE, HDS_DEAD, HDS_GONE };
enum { HDL_DOOR, HDL_WIN0, HDL_WIN1, HDL_COUNT };     /* lanes: the door, the window slots */
enum { HDP_READY, HDP_WAVE, HDP_QUIET, HDP_DAWN, HDP_DONE };
enum { HDG_PIP, HDG_LULLA, HDG_NANA, HDG_KEVIN, HDG_MAYOR, HDG_COUNT };
enum { HDA_REPAIR, HDA_REWIND, HDA_COMFORT, HDA_PLATE };
/* what happened on the last tick (the scene's sounds, shakes and pops) */
enum { HDE_WAVE = 1u << 0, HDE_DOOR_HIT = 1u << 1, HDE_RIMSHOT = 1u << 2, HDE_SHOT = 1u << 3,
       HDE_KILL = 1u << 4, HDE_PLATE_HIT = 1u << 5, HDE_PLATE_FALL = 1u << 6,
       HDE_WINDOW_BREAK = 1u << 7, HDE_DOOR_BREAK = 1u << 8, HDE_ENTER = 1u << 9,
       HDE_SCARE = 1u << 10, HDE_FLEE = 1u << 11, HDE_DECOY = 1u << 12, HDE_ALARM = 1u << 13,
       HDE_CHIMNEY = 1u << 14, HDE_THAW = 1u << 15, HDE_DAWN = 1u << 16, HDE_REPAIR = 1u << 17,
       HDE_REWIND = 1u << 18, HDE_COMFORT = 1u << 19, HDE_PLATE_BACK = 1u << 20 };
typedef struct HomeDemon {
    uint8_t kind, state, lane, hp, nerve, timer, hops, tx, ty;   /* tx, ty: indoors tile */
    int16_t dist;                    /* px outside the wall along the lane, 0 = at it   */
} HomeDemon;
typedef struct HomeGuest {
    uint8_t x, y, present, safe, calm, fled, flinch;
} HomeGuest;
typedef struct HomeDefend {
    uint8_t  table, nwaves, wave, phase, room, w, d, nwin;
    uint16_t t, wave_t, phase_t, freeze;
    int16_t  door_hp, door_max;
    uint8_t  barricade, door_broken, lanes, held;
    uint8_t  lane_wall[HDL_COUNT], lane_x[HDL_COUNT], lane_y[HDL_COUNT];
    uint8_t  plate_hp[2], plated, plate_down, broken;  /* bits per window slot      */
    int8_t   turret, mayor_win, target;
    uint8_t  turret_x, turret_y, ammo, fire_cd, busy, busy_act;
    uint8_t  spawned, brutes_spawned, killed, decoy_used, alarm, alarm_t, chimney, panic;
    uint16_t door_hits, shots;
    uint32_t ev;                     /* HDE_* of the last tick                         */
    DoomoRng rng;
    HomeDemon dm[HD_DEMONS];
    HomeGuest g[HDG_COUNT];
} HomeDefend;
extern const int16_t home_defend_guest_id[HDG_COUNT];   /* resident ids (-1: Kevin)  */
/* Set up `table` (HDT_*; HDT_AUTO asks the story) on room record `r` of
 * room `room`: lanes, door HP = Barricade, plates, the turret, the guests.
 * Tests and the Arena's continue may start mid-table by setting d->wave
 * before the first tick. */
void home_defend_begin(HomeDefend *d, int table, const HomeRoom *r, int room);
int  home_defend_tick(HomeDefend *d);        /* 1 while running, 0 once dawn is over */
int  home_defend_can(const HomeDefend *d, int act, int idx);
int  home_defend_act(HomeDefend *d, int act, int idx);   /* 1 done                   */
int  home_defend_action_at(const HomeDefend *d, int x, int y, int *idx); /* HDA_* or -1 */
int  home_defend_safe(const HomeDefend *d);  /* guests still safe                   */
void home_defend_finish(HomeDefend *d);      /* the result into HOME                 */
int  home_defend_table_for_story(void);
int  home_defend_nwaves(int table);
int  home_defend_wave_imps(int table, int wave);
int  home_defend_wave_brutes(int table, int wave);
int  home_defend_red_sky(int table);
typedef struct HomeDefendMap {
    uint8_t w, d, door_x, door_y, nwin, win_x[2], win_y[2];
    uint8_t barricade, plated, turret_win, nwaves;
    uint8_t imps[HOME_DEFEND_WAVES], brutes[HOME_DEFEND_WAVES];
} HomeDefendMap;
int  home_defend_export(HomeDefendMap *m);   /* the Arena's map of the player's house */

/* ---- H4: room_ref.c (the reference renderer) and home_draw.c ---------------- */
/* The whole room, full redraw (the weak plat_room() calls it). */
void room_ref_draw(const DoomoRoomView *v);
/* (T0) room point -> screen in room_ref_draw()'s projection (an actor's feet
 * anchor); plat_room_project's weak default (plat_ext.h). */
void room_ref_project(const DoomoRoomView *v, fix16 x, fix16 y, int z, int16_t *sx, int16_t *sy);
/* Decode facing `facing` (0 / 1) of animation frame `frame` of `furn` into
 * buf (cap bytes), recoloured to paints pa / pb (-1 = the markers' own
 * shades) and lit at level lvl (15 = fullbright). (*ox, *oy) is where the
 * footprint's back corner lands in the image. 0 = no sprite (the PS1 links
 * none) or too big for buf. */
int  room_ref_sprite(int furn, int facing, int frame, int pa, int pb, int lvl,
                     uint8_t *buf, unsigned cap, DoomoImage *img, int *ox, int *oy);
/* (H9) the catalog / HomeMart turntable: a quarter turn every 64 ticks
 * through a piece's four facings, its back included; a wall piece only
 * swaps between its two walls (its back is never seen hung) */
static inline int room_ref_turn(const DoomoFurn *f, unsigned t)
{
    return (int)((t >> 6) & (f && (f->flags & FF_WALL) ? 1u : 3u));
}
/* The catalog / HomeMart preview where there is no sprite (the PS1 links
 * none): the piece's prims as shaded iso blocks, a little maquette, centred
 * in (x, y, w, h) at 1x or 2x. 0 = nothing to draw. */
int  room_ref_maquette(int furn, int pa, int pb, int x, int y, int w, int h);
/* (HART) The main roof behind the two back walls (DoomoRoomView.roof), in
 * screen space: room_ref_draw() and the GBA (gba/src/room.c) both set it up
 * with room_roof_setup() and draw it row by row with room_roof_row(), so
 * the two match pixel for pixel. Its colours are lit at `level` (6..15) and
 * pushed through doomo_tintmap[tint] (0 = none). */
enum { RFC_GUT_D, RFC_GUT_L, RFC_RIDGE_D, RFC_RIDGE_L, RFC_VALLEY, RFC_HIP,
       RFC_A_D, RFC_A_M, RFC_A_L, RFC_A_X, RFC_B_D, RFC_B_M, RFC_B_L, RFC_B_X,
       RFC_F_L, RFC_F_D, RFC_F_M, RFC_F_CAP, RFC_GLASS, RFC_GLINT, RFC_SPIKE_L, RFC_SPIKE_D,
       RFC_SIDING, RFC_SIDING_D, RFC_COUNT };
typedef struct RoomRoof {
    int16_t ox, top, LA, LB;         /* the back corner's x, the walls' top row there,
                                        the visible walls' lengths in px            */
    int16_t fx0, fx1, ftop;          /* the feature (chimney / water tank): screen x
                                        [fx0, fx1) from row ftop; fx1 <= fx0: none   */
    int16_t sky0, sky1;              /* ROOF_ATTIC: the skylight's columns p          */
    uint8_t style, band, ch, ffoot;  /* ROOF_*, rows above the wall top, course rows,
                                        the feature stands ffoot rows above the top   */
    int16_t dp;                      /* the dormer's centre column p, 0 = none        */
    uint8_t fkind, sky_side, dside, dw; /* 1 chimney, 2 water tank; the skylight's and
                                        the dormer's walls, its half width            */
    uint8_t dh, dmax, pad[2];        /* the dormer's face height to the eaves; the
                                        highest row d any slope pixel reaches        */
    uint8_t c[RFC_COUNT];            /* lit colours                                  */
} RoomRoof;
/* 0 when the view has no roof. k, W, D, ox, oy, wall_h: the frame's view,
 * turned room size, back corner and wall height (room_ref's projection). */
int  room_roof_setup(RoomRoof *f, const DoomoRoomView *v, int k, int W, int D, int ox, int oy,
                     int wall_h, int level, int tint);
/* The rows and the columns the roof may touch: [*y0, *y1), [*x0, *x1). */
void room_roof_rows(const RoomRoof *f, int *y0, int *y1);
void room_roof_cols(const RoomRoof *f, int *x0, int *x1);
/* Row y, pixels [x0, x1): out[i] = the colour of x0 + i, 0 where the roof
 * leaves the background. Returns the number of roof pixels. */
int  room_roof_row(const RoomRoof *f, int y, int x0, int x1, uint8_t *out);
/* The pieces room_roof_row() is made of, for a renderer that caches the
 * repeating part (the GBA): the wall top row of column p, the pattern at
 * column p & 15 (0..15) d rows above it, the p ranges where it does not
 * hold (pairs [r[2i], r[2i+1]), returns the count), the slope pixel
 * anywhere, and the chimney / tank row (over what out[] holds). */
int     room_roof_base(const RoomRoof *f, int side, int p);
uint8_t room_roof_pattern(const RoomRoof *f, int side, int pp, int d);
int     room_roof_specials(const RoomRoof *f, int side, int16_t r[8]);
uint8_t room_roof_slope(const RoomRoof *f, int side, int p, int d);
int     room_roof_feature_row(const RoomRoof *f, int y, int x0, int x1, uint8_t *out);
#define ROOM_ROOF_PAT_ROWS 34            /* d 0..33: band + spikes, the dormer's apex at wall_h 48 */
#define ROOM_ROOF_EAVE 4                 /* px the eaves overhang the walls' ends */
/* The roof style of a room of the player's house (ROOF_*). */
int  home_room_roof(int room);

/* ---- H5: the open kinds' plot edge and the door's backdrop ----------------------
 * The lip (ROOMK_GARDEN / ROOMK_OPEN): a soil band under BOTH front edges, a
 * 2 px column per 2 units, ROOM_LIP_H rows from the edge row: row 0 the
 * floor's grout shade (the turf's cut edge), then soil (front A' face the
 * lighter, front B' the darker), the last row darker still. Column u = 0, 2,
 * .. 16W - 2 of front A' (the line y' = D) sits at x = ox - 16D + u, row
 * oy + ((u + 16D) >> 1); column u = 0, 2, .. 16D - 2 of front B' (x' = W) at
 * x = ox + 16W - u - 2, row oy + ((u + 16W) >> 1). A' is drawn first.
 * The backdrop: behind the fixed door when it stands on a back wall of the
 * view, the Garden shows a strip of the cottage's back wall (clapboard
 * siding, a trim, a stone footing; the door span plus a tile each side) and
 * the Park Plot a gate's two stone posts. Column u (even, [u0, u1)) of side
 * A' is at x = ox + u with its bottom (exclusive) at oy + (u >> 1), of side
 * B' at x = ox - u - 2 with its bottom at oy + ((u + 2) >> 1); its rows t =
 * 0 .. h - 1 run down from bottom - h. room_backdrop_px() is 0 where the
 * sky shows. A frame with a backdrop is laid out as a walled room (wall_h
 * counts in the height that centres it). */
#define ROOM_LIP_H 6
enum { RBD_NONE, RBD_WALL, RBD_POSTS };
enum { RBC_TRIM_D, RBC_TRIM, RBC_SIDING, RBC_SIDING_D, RBC_CORNER, RBC_STONE, RBC_STONE_D,
       RBC_CAP, RBC_COUNT };
typedef struct RoomBackdrop {
    uint8_t style, side, h, pad;     /* RBD_*, 0 = A' / 1 = B', rows               */
    int16_t u0, u1, p0, p1;          /* columns [u0, u1); the posts' first columns */
    uint8_t c[RBC_COUNT];            /* lit colours                                */
} RoomBackdrop;
/* RBD_NONE (0) when the view shows none. k, W, D: the view and turned size. */
int     room_backdrop_setup(RoomBackdrop *b, const DoomoRoomView *v, int k, int W, int D,
                            int wall_h, int level, int tint);
uint8_t room_backdrop_px(const RoomBackdrop *b, int u, int t);
/* The lip's colours for a view: [0] the turf edge, [1] / [2] soil on front
 * A' / B', [3] / [4] the bottom row on A' / B'. */
void    room_lip_colours(const DoomoRoomView *v, int level, int tint, uint8_t c[5]);
/* (H5) a wall piece's place in view k: 0 / 1 on back wall A' / B', 2 / 3 on
 * the front edge y' = D / x' = W of an open kind, -1 cut away (the front of
 * a walled room; the door and windows on an open kind's front). */
int     room_wall_side(const DoomoRoomView *v, const DoomoRoomObj *o, int k);
/* (H9) a wall piece's view-space tile box on side `side` of view k (shared
 * by room_ref and the GBA) and the view rot to draw it at: on a back wall
 * the tile against it, rot = side (its front); on an open kind's front
 * edge the tile inside the edge with its back to the camera, rot = side
 * (2 / 3); a flat FF_MIRROR decal has no back: its front on the edge line
 * (rot = side & 1, as before H9). */
int     room_wall_box(const DoomoRoomView *v, const DoomoRoomObj *o, int k, int side,
                      int *x0, int *y0, int *x1, int *y1);

/* ---- H8: the light ---------------------------------------------------------------
 * Lamp pools: up to ROOM_LAMPS light sources (a lit FF_LIGHT piece, i.e.
 * RO_FULLBRIGHT, or a hearth with a `light` radius; held ghosts never) in
 * object order raise the level of the floor tiles, the back walls' tile
 * columns and the pieces around them: at a point (X, Y) in view units (16
 * a tile), lamp i adds k_lamp[d^2 * 32 / r_i^2] (1 / (1 + 2 d^2/r^2) less
 * its tail, 0..128, zero from d = 1.41 r: the PS1's table, which lights its
 * floor vertices the same way), and the level becomes
 * level + (sum * (15 - level) + 64) / 128, at most 15. A tile takes its
 * centre, wall A' column i the point (16i + 8, 0), wall B' column j
 * (0, 16j + 8), the corner post (0, 0), a piece the centre of its view box.
 * Nothing changes at level 15 (n = 0).
 * Glow pixels (furn.h FPL_GLOW class spans) keep full light unless their
 * piece is a switched-off lamp; the lava-crack floor's cracks glow too.
 * Window panes (FPL_PANE) show the sky by daylight (the open kinds'
 * thresholds: 13 and up day, the art's own glass; 9..12 a peach dusk;
 * below 9 a navy night with stars), lit like the rest of the piece but for
 * the stars; the roof's
 * dormer and skylight glass follow the same sky. room_ref_draw() and the
 * GBA (gba/src/room.c) both use these, so the two match pixel for pixel. */
#define ROOM_LAMPS   4
#define ROOM_LIGHT_N 16                  /* the largest turned room side         */
enum { RPANE_DAY, RPANE_DUSK, RPANE_NIGHT };
typedef struct RoomLight {
    uint8_t  n, level, pane, corner;     /* lamps, ambient level, RPANE_*, the post */
    int16_t  lx[ROOM_LAMPS], ly[ROOM_LAMPS];   /* centres, view units            */
    uint16_t inv[ROOM_LAMPS];            /* (32 << 16) / r^2 (r in units)        */
    uint8_t  wall[2][ROOM_LIGHT_N];      /* wall A' column i, wall B' column j   */
    uint8_t  tile[ROOM_LIGHT_N][ROOM_LIGHT_N];  /* [j][i] in the turned view    */
} RoomLight;
/* Sets L up for view k (W x D turned) at the ambient level; returns the
 * number of lamps (0: every level is `level`). It is room_light_lamps()
 * (the lamps, the level and the panes' sky; every field but the tables)
 * then room_light_fill() (the tile, wall and corner levels, which follow
 * from those and W, D only: the GBA refills them only when they change). */
int     room_light_setup(RoomLight *L, const DoomoRoomView *v, int k, int W, int D, int level);
int     room_light_lamps(RoomLight *L, const DoomoRoomView *v, int k, int level);
void    room_light_fill(RoomLight *L, int W, int D);
/* The level at (X, Y), view units. */
int     room_light_at(const RoomLight *L, int X, int Y);
/* RPANE_* of a daylight 0..15 */
int     room_pane_mode(int daylight);
/* A pane pixel's colour under the sky (c recoloured, unlit), and whether
 * pixel (x, y) of a facing (unmirrored) is a star at night. */
uint8_t room_pane_colour(uint8_t c, int pane);
int     room_pane_star(int x, int y);
#define ROOM_STAR_COLOUR PAL_RAMP(RAMP_CANDY, 7)
/* room_ref_sprite()'s lvl flags (H8): a switched-off lamp (its glow pixels
 * shade too), the panes' sky */
#define RR_SPR_DARK  0x10
#define RR_SPR_DUSK  0x20
#define RR_SPR_NIGHT 0x40

/* Fill a render-ready DoomoRoomObj from a room object (catalog paints
 * resolved, z in px). */
void home_obj_view(const HomeObj *o, DoomoRoomObj *out);

/* ---- H4 M2: home_visit.c (HF sec.3; SPEC H4) ----------------------------------
 * The 1000 citizens' rooms are generated, never saved: seed = town seed ^
 * id * 0x9E3779B1 ^ HOME_CTX_VISIT, then the roster record and its live
 * state pick the template, paints, pieces and the variant. Gifts the
 * player left (HOME.gifts) override tiles. */
enum { HVK_HOME, HVK_POSSESSED, HVK_GHOST, HVK_SEALED, HVK_AWAY, HVK_VACANT, HVK_COUNT };
enum { HWANT_SEAT, HWANT_PINK, HWANT_PLANT, HWANT_LAMP, HWANT_PLUSH, HWANT_SPOOKY,
       HWANT_CUTE, HWANT_BOOK, HWANT_SNACK, HWANT_MUSIC, HWANT_HOBBY, HWANT_COLOR,
       HWANT_RUG, HWANT_WALL, HWANT_TABLE, HWANT_GAME, HWANT_COUNT };
/* reactions (HF sec.3.4, 5.3): what the scene plays when a citizen sees a piece */
enum { HVR_NONE, HVR_DOUBLE_TAKE, HVR_DAWNING, HVR_DEADPAN, HVR_HEARTS, HVR_BANG,
       HVR_FLEE, HVR_EW, HVR_SIT, HVR_NOD, HVR_COUNT };
typedef struct HomeVisit {
    uint16_t id;
    uint8_t  kind, room;             /* HVK_*, the template (HOME_ROOM_TOWERS_7B / PITS_FLAT) */
    uint8_t  daylight, tint, music;  /* 0..15 (0 = the clock), TINT_*, SONG_*        */
    uint8_t  layout;                 /* 0 tidy, 1 chaotic, 2 shy, 3 show-off          */
    uint8_t  wants[3];               /* HWANT_* this week                            */
    uint8_t  gifts;                  /* gifts of the player placed                    */
    uint8_t  hobby_obj;              /* index of their hobby piece, 0xFF none         */
    uint8_t  protect;                /* 1: the fear's protective piece was added      */
    uint32_t ops;                    /* work done (the test keeps it < 60k)           */
} HomeVisit;
int  home_visit_gen(int id, HomeRoom *out, HomeVisit *info);   /* 0: no such citizen */
/* the same, shown as variant `kind` (HVK_POSSESSED .. HVK_AWAY; HOME_ARG_VISIT_AS) */
int  home_visit_gen_as(int id, int kind, HomeRoom *out, HomeVisit *info);
/* pieces that set off fear `fear` (FEAR_COUNT entries, -1 terminated) */
int  home_visit_fear_hit(int fear, int furn);
/* the room fails the citizen's fear as a whole (The Dark: no lamp lit) */
int  home_visit_fear_room(const DoomoResident *r, const HomeRoom *room);
void home_visit_wants(int id, uint8_t out[3]);       /* weekly, (day / 7) ^ id      */
int  home_visit_want_met(int want, const DoomoResident *r, int furn, int pa);
const char *home_visit_want_text(int want);
/* What `r` does on seeing `furn` (painted pa), and the line they say. */
int  home_visit_react(const DoomoResident *r, int furn, int pa, const char **line);
/* Leave a gift: placed in the generated room `room` (already checked by
 * home_fit), one is taken from storage. Returns the gift slot, -1 full. */
int  home_visit_gift(int id, int furn, int x, int y, int rot, int pa);
int  home_visit_gift_count(int id);
/* Log a visit: the visited bit, the visitor log, the trophies. */
void home_visit_log(int id);
int  home_visited(int id);
int  home_visit_recent(int i);                       /* the log, -1 empty           */
/* After a gift: friendship +8 per want met this week; all three met gives
 * their thank-you piece once per week. Returns the HWANT_* bits met. */
int  home_visit_thank(int id, int furn, int pa, int *thanks_furn);

/* ---- H4 M2: home_mart.c (HomeMart, the Black Market, the furniture bench) ----- */
#define HOME_MART_STOCK 12
enum { HMT_STOCK, HMT_SEASON, HMT_LAST_SEASON, HMT_WANTS, HMT_SELL, HMT_COUNT };
typedef struct HomeMartRow {
    uint16_t furn, price;
    uint8_t  locked, deal, owned, tab;
} HomeMartRow;
/* Today's rows of tab `tab` (HMT_*); black = the Black Market. */
int  home_mart_rows(int tab, int black, HomeMartRow *out, int cap);
int  home_mart_season(void);                         /* 0 spring .. 3 winter        */
int  home_mart_black_open(void);                     /* night, or by day after Ch4  */
int  home_mart_buy(const HomeMartRow *row);          /* 1 bought                    */
int  home_mart_sell(int furn);                       /* 40%; 0 when it cannot be sold */
/* The Workshop furniture bench: recipe i -> the piece and its items. */
#define HOME_BENCH_RECIPES 24
typedef struct HomeBenchRecipe { uint16_t furn; uint8_t item[3], n[3]; } HomeBenchRecipe;
extern const HomeBenchRecipe home_bench_recipes[HOME_BENCH_RECIPES];
int  home_bench_can(int i);
int  home_bench_make(int i, int grade);              /* grade 0..3 (S = 3: gold paint B) */
/* Room expansion (Town Hall "Expand"): the next room and its price, -1 none. */
int  home_expand_next(int *price);
int  home_expand_buy(void);                          /* the room unlocked, -1 */
extern uint8_t home_mart_expanded;                   /* the room the house shows off next, 0xFF */

#endif
