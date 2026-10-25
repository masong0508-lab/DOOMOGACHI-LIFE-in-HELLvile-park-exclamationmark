/* show_hosts.h - the game's own renderers as show stage kinds (owner: show,
 * ENGINE lane 3 / WP15; spec 8.3).
 *
 *   OVERWORLD  plat_overworld() on the game's overworld maps (hahaland,
 *              hahaland_invaded, hellvile, park_united): a keyed camera,
 *              world actors standing and walking, seeded NPC citizens
 *   ROOM       the 2.5D house renderer (ps1/src/room.c) on a room built
 *              from the house modules (home.c, furn_*): the cottage
 *   VIEW3D     plat_view3d() first person on the game's 3D maps, with
 *              cam3d rails, monsters from the combat art, gore by the
 *              game's cute/brutal filter, muzzle flash, hit-stop and the
 *              first-person HUD (show_hud.c)
 *
 * The plug-in contract (one plane of the shot is a "host plane"):
 *
 *   show_host_prep(rec, bytes)  at shot prep, with the plane's record
 *                               (written by show/tools/showtk/hosts.py);
 *                               it loads the map, fills NPC looks, resets
 *                               the renderer's own state (purity, 8.2)
 *   show_host_tick(t)           EVERY shot frame t = 0, 1, 2 ... in order,
 *                               pre-roll included (it fires the edge
 *                               events: camfx shakes, the game's gore
 *                               option); everything it draws is a pure
 *                               function of t
 *   show_host_warm()            uncaptured frames after prep, until 0
 *   show_host_draw(t)           draw the plane (skipped during pre-roll)
 *   show_host_draw_hud(t)       VIEW3D: the HUD, the weapon and the lens,
 *                               after camfx_apply() (the HUD does not shake)
 *   show_host_end()             the shot is over
 *
 * The record carries its own timeline (camera keys, actor paths, events),
 * so the host needs no BOP_SHOT op numbers: its only coupling to the
 * player is the five calls above.
 *
 * Every number in a record is little-endian and every item is 4-aligned;
 * show/tools/showtk/hosts.py mirrors this file (and a test checks the two
 * agree, show/tests/engine/hosts/test_hosts.py).
 */
#ifndef SHOW_HOSTS_H
#define SHOW_HOSTS_H

#include <stdint.h>

#define SHOST_MAGIC 0x54534853u     /* "SHST" */
#define SHOST_ABI   1

enum { SHOST_NONE, SHOST_OVERWORLD, SHOST_ROOM, SHOST_VIEW3D, SHOST_KIND_COUNT };

/* the filter (the game's OPT_GORE: GORE_CUTE / GORE_BRUTAL) */
enum { SG_CUTE = 0, SG_BRUTAL = 1 };

/* ---- the record ------------------------------------------------------------- */
typedef struct ShowHostHdr {
    uint32_t magic;             /* SHOST_MAGIC                                   */
    uint16_t abi;               /* SHOST_ABI                                     */
    uint8_t  kind;              /* SHOST_*                                       */
    uint8_t  nsec;              /* sections that follow                          */
    uint32_t bytes;             /* the whole record, this header included        */
    uint16_t len;               /* the shot's length in frames                   */
    uint8_t  gore;              /* SG_* at frame 0 (the shot header's gore=)     */
    uint8_t  flags;             /* SHF_*                                         */
    uint32_t seed;              /* the shot seed (gibs, NPC walks)               */
} ShowHostHdr;                  /* 20 B */

enum {
    SHF_KIDS = 1                /* STD-KIDS: a child is in this scene: the
                                   filter is CUTE in every frame, whatever
                                   gore= or a `gore` event says             */
};

typedef struct ShowHostSec {
    uint16_t tag;               /* SEC_*                                         */
    uint16_t count;             /* items                                         */
    uint32_t bytes;             /* payload bytes (count x item size, or the
                                   string table's size), a multiple of 4         */
} ShowHostSec;

enum {
    SEC_WORLD = 1,              /* 1 x ShowHostWorld                             */
    SEC_CAMKEY,                 /* n x ShowHostKey, sorted by frame              */
    SEC_ACTOR,                  /* n x ShowHostActor                             */
    SEC_PATH,                   /* n x ShowHostKey: actor position keys          */
    SEC_EVENT,                  /* n x ShowHostEvent, sorted by frame            */
    SEC_FURN,                   /* n x ShowHostFurn (ROOM)                       */
    SEC_NPC,                    /* n x ShowHostNpc (OVERWORLD)                   */
    SEC_STR,                    /* NUL-terminated strings, referenced by offset  */
    SEC_HUD,                    /* 1 x ShowHostHud (VIEW3D)                      */
    SEC_COUNT
};

typedef struct ShowHostWorld {
    uint8_t  map;               /* MAPID_* (OVERWORLD, VIEW3D)                   */
    uint8_t  daylight;          /* 0 midnight .. 15 noon                         */
    uint8_t  tint, tint_level;  /* TINT_*, 0..15                                 */
    int16_t  vx, vy, vw, vh;    /* screen viewport (0, 30, 320, 180 = the 16:9
                                   picture)                                      */
    uint8_t  flags;             /* SWF_*                                         */
    uint8_t  fov;               /* VIEW3D: OPT_FOV 0..2                          */
    uint8_t  view_dist;         /* VIEW3D: cells drawn, 11..18 (0 = 18)          */
    uint8_t  pad0;
    /* ROOM */
    uint8_t  room_w, room_d;    /* tiles                                         */
    uint8_t  wallpaper, floor;  /* WALLPAPER_* / FLOOR_* (home.h)                */
    uint8_t  wall_paint, floor_paint;  /* PAINT_*                                */
    uint8_t  ceiling, room_kind;       /* ROOMK_*                                */
    uint8_t  weather;
    uint8_t  tmpl;              /* ROOM: HOME_ROOM_* + 1: the game's room template
                                   (size, paints, its fixed door/windows/hearth
                                   via home_room_template + home_obj_view); 0 none */
    uint8_t  pad[2];
} ShowHostWorld;                /* 28 B */

enum {
    SWF_DOORS_OPEN = 1,         /* every door of the map starts fully open        */
    SWF_NO_SHADOWS = 2          /* no blob shadows under world actors             */
};

/* A key: the camera (SEC_CAMKEY) or an actor's position (SEC_PATH). The
 * value at frame t eases from the last key at or before t to the next
 * (ease of the NEXT key: how it is reached).
 *   OVERWORLD camera  a, b = focus x, y (16.16 world units)
 *   ROOM camera       a = yaw 0..4095 (unwrapped), b = zoom 0/1,
 *                     c = punch_q8 (0 = 256), d, e = pan x, y
 *   VIEW3D camera     a, b, c = eye x, y, z (16.16), d = yaw (unwrapped),
 *                     e = pitch -256..256
 *   actor path        a, b = x, y (16.16 world; ROOM: 16.16 tiles),
 *                     c = z above the floor (world units), d = facing
 *                     (SDIR_*), e = mode (SPM_*)                           */
typedef struct ShowHostKey {
    uint16_t f;                 /* shot frame                                    */
    uint8_t  ease;              /* SE_*                                          */
    uint8_t  flags;             /* SKF_*                                         */
    int32_t  a, b, c;
    int16_t  d, e;
} ShowHostKey;                  /* 20 B */

enum { SE_HOLD, SE_LINEAR, SE_IN, SE_OUT, SE_INOUT };
enum { SKF_SNAP = 1 };          /* no blend from the key before (a cut)          */
enum { SDIR_DOWN, SDIR_LEFT, SDIR_RIGHT, SDIR_UP, SDIR_AUTO = 255 };
enum { SPM_STAND, SPM_WALK, SPM_RUN, SPM_HOP };

typedef struct ShowHostActor {
    uint8_t  id;                /* the record's actor index (events target it)   */
    uint8_t  kind;              /* SAK_*                                         */
    uint16_t ref;               /* SAK_CHIBI / SAK_FIGURE: doomo_town.res[] slot;
                                   SAK_SPRITE / SAK_MONSTER: SPR_*               */
    uint16_t path0, npath;      /* its keys in SEC_PATH                          */
    uint8_t  flags;             /* SAF_*                                         */
    uint8_t  expr;              /* EXPR_* (figures)                              */
    uint16_t scale;             /* 8.8, 0 = the renderer's default               */
    uint8_t  hp;                /* monsters: pain frames before a kill (info)    */
    uint8_t  pad[3];
} ShowHostActor;                /* 16 B                                          */

enum {
    SAK_CHIBI = 1,              /* bodyspr_image(res, dir, frame)                */
    SAK_FIGURE,                 /* figure_image(look, status, expr)              */
    SAK_SPRITE,                 /* sprite_image(SPR_*, frame) (props, the pet)   */
    SAK_MONSTER                 /* a demon of the combat art (DFRAME_* frames)   */
};
enum {
    SAF_KID = 1,                /* a child (STD-AGE): in frame = the filter on   */
    SAF_SHADOW = 2,
    SAF_FULLBRIGHT = 4,
    SAF_TRANSLUCENT = 8,
    SAF_HIDDEN = 16,            /* hidden until a SEV_SHOW                       */
    SAF_FLIPX = 32
};

typedef struct ShowHostEvent {
    uint16_t f;                 /* shot frame                                    */
    uint8_t  op;                /* SEV_*                                         */
    uint8_t  target;            /* actor id, or 0xFF                             */
    int16_t  a, b;
    int32_t  c;
} ShowHostEvent;                /* 12 B */

enum {
    SEV_GORE = 1,               /* a = SG_*: the filter switches                 */
    SEV_FIRE,                   /* the weapon fires: a = flash frames (0 = 3),
                                   b = shells used (0 = 1)                        */
    SEV_PAIN,                   /* target flinches (DFRAME_PAIN for a = frames)  */
    SEV_KILL,                   /* target dies: a = 1 overkill; gore by filter   */
    SEV_HITSTOP,                /* a = frames: world time stops (the HUD runs)   */
    SEV_ATTACK,                 /* target attacks (DFRAME_ATTACK for a frames)   */
    SEV_HURT,                   /* Sunny is hit: a lens splash, a heart pops     */
    SEV_HUD,                    /* a = SHUD_* field, b = value                   */
    SEV_SHAKE,                  /* a = SHK_*: camfx_shake() on this frame        */
    SEV_SHOW,                   /* target appears                                */
    SEV_HIDE,                   /* target disappears                             */
    SEV_EXPR,                   /* target's expression = a                       */
    SEV_LENS,                   /* the lens tint: a = level 0..15, b = frames,
                                   c = TINT_* (0 = red)                           */
    SEV_DOOR,                   /* a, b = cell: the door opens                   */
    SEV_WEAPON,                 /* a = SWPN_*: the weapon swaps (the slot spins) */
    SEV_FALL,                   /* target falls over flat (a cardboard ghost)    */
    SEV_COUNT
};

/* VIEW3D: the first-person HUD (show_hud.c) */
enum { SWPN_NONE, SWPN_PUMP, SWPN_SAW, SWPN_WAND, SWPN_COUNT };
enum { SFACE_IDLE, SFACE_LEFT, SFACE_RIGHT, SFACE_GRIN, SFACE_WINCE, SFACE_GLARE,
       SFACE_COUNT };
enum {
    SHUD_ON = 1,                /* b = 0 off, 1 on, 2 slam in (4 f)              */
    SHUD_HEARTS,                /* b = hearts left                               */
    SHUD_SHELLS,                /* b = shells left                               */
    SHUD_FACE,                  /* b = SFACE_*                                   */
    SHUD_PLATE,                 /* b = 0 cute name, 1 brutal name (the flip)     */
    SHUD_STICKER,               /* b = 0 none, 1 slapped on (4 f), 2 blown off   */
    SHUD_MODE,                  /* b = 0 hidden, 1 the GORE: label shown         */
    SHUD_HORN,                  /* b = 1: the face grows its horn nub (4 f)      */
    SHUD_COUNT
};
typedef struct ShowHostHud {
    uint8_t  on, hearts, hearts_max, shells;
    uint8_t  shells_max, face, weapon, plate;
    uint8_t  sticker, mode, horn, flags;
    uint16_t str_cute[SWPN_COUNT];   /* SEC_STR offsets: the weapon plates (cute) */
    uint16_t str_brutal[SWPN_COUNT]; /* ... and their brutal names               */
    uint16_t str_tiny;               /* "RIP AND TEAR... GENTLY"                  */
    uint16_t str_sticker;            /* "HELLVILE PARK BOARD OF DECENCY"          */
    uint16_t str_sticker2;           /* "(approved)"                              */
    uint16_t pad;
} ShowHostHud;                  /* 12 + 16 + 8 = 36 B */

/* ROOM: a piece of furniture, exactly platform.h's DoomoRoomObj */
typedef struct ShowHostFurn {
    uint16_t furn;              /* FURN_* (core/gen/furn_ids.h)                  */
    uint8_t  x, y, z;           /* tile, tile, height px                         */
    uint8_t  rot_anim;          /* RO_ROT_ANIM(rot, anim)                        */
    uint8_t  paint_a, paint_b;  /* PAINT_* or 0xFF                               */
    int8_t   sq_x, sq_y, jitter;
    uint8_t  flags;             /* RO_*                                          */
} ShowHostFurn;                 /* 12 B */

/* OVERWORLD: a group of NPC citizens, each walking a seeded loop inside a
 * box of cells, or standing (speed 0). Looks are made at prep with the
 * game's resident_randomize_look(seed + i) in doomo_town.res[rid..]. */
typedef struct ShowHostNpc {
    uint8_t  n;                 /* citizens                                      */
    uint8_t  flags;             /* SAF_* (SAF_KID: they are children)            */
    uint16_t rid;               /* their first doomo_town.res[] slot (the cast
                                   uses 0..255; hosts.py hands out 600..)        */
    uint32_t seed;
    int16_t  x0, y0, x1, y1;    /* the box, world units                          */
    uint8_t  speed_q4;          /* world units per frame x16 (16 = 1 unit/f)     */
    uint8_t  species;           /* SPECIES_* + 1, 0 = any (gen_look's own)       */
    uint8_t  mode;              /* SNM_*                                         */
    uint8_t  looks;             /* K-look reuse (spec 8.5): citizen k wears look
                                   rid + k % looks (0 = n): K looks x their walk
                                   frames stay in the portrait pool              */
} ShowHostNpc;                  /* 20 B */
enum { SNM_WANDER, SNM_STAND, SNM_LANE_E, SNM_LANE_W, SNM_LANE_N, SNM_LANE_S };

/* ---- the plug-in calls ------------------------------------------------------- */
int  show_host_prep(const void *rec, uint32_t bytes);  /* 0 ok, < 0 a bad record */
void show_host_tick(int t);
void show_host_draw(int t);
void show_host_draw_hud(int t);
void show_host_end(void);
#define SHOST_WARM_MAX 10
int  show_host_warm(void);     /* after prep, on UNCAPTURED frames, until it
                                  returns 0: draws frame 0 so its textures are
                                  in VRAM before the captured frame 0          */
int  show_host_kind(void);                             /* SHOST_* of the shot    */
const char *show_host_error(void);                     /* why prep failed        */

/* ---- the filter (STD-KIDS, spec 13.5) ---------------------------------------- */
/* The cute/brutal switch is the game's own OPT_GORE. It is CUTE in every
 * frame a child is drawn in, and in every frame of a scene with a child
 * (SHF_KIDS), whatever the script asks for. Any package that draws a child
 * reports it with show_gore_kid_in_frame() before the host draws. */
void show_gore_kid_in_frame(void);
int  show_gore_effective(void);                        /* SG_* for this frame    */
int  show_gore_forced_frames(void);                    /* frames forced CUTE     */

/* ---- stats (the timing report) ----------------------------------------------- */
typedef struct ShowHostStats {
    uint32_t draw_us;           /* the last show_host_draw() (CPU)               */
    uint32_t hud_us;            /* the last show_host_draw_hud()                 */
    uint16_t sprites;           /* billboards handed to the renderer             */
    uint16_t visible;           /* of them in the viewport                       */
    uint16_t kids_seen;         /* children in the last frame                    */
    uint16_t gore;              /* SG_* drawn in the last frame                  */
} ShowHostStats;
extern ShowHostStats g_show_host_stats;

#endif
