/* shw_format.h - the EP###.SHW episode file the compiler (showc) writes and
 * show.exe reads (SHOW_ENGINE_SPEC.md sec.7.3).
 *
 * Mirrored field for field by show/tools/showtk/shw_format.py; the AT4.2 test
 * (show/tests/compiler/test_at4_2_format.py) parses this header, compiles it
 * with the host gcc and round-trips a file through both. Change both in the
 * same commit and bump SHW_ABI.
 *
 * LAYOUT. Little-endian; every record is naturally aligned and every section
 * starts on a 4-byte boundary.
 *
 *   file offset 0      ShwEpHeader                         (40 B)
 *               40     ShwShotIndex[nshots]                (36 B each)
 *               ...    episode string table (strtab_off, strtab_bytes):
 *                      NUL-terminated ASCII names (shot ids, beat ids, set and
 *                      cast ids) referenced by *_name fields as byte offsets
 *               k*2048 one ShwShotChunk per shot, each starting on a 2048-byte
 *                      sector (chunk_sector), chunk_bytes long
 *
 *   ShwShotChunk = ShwShotHdr, then the sections it points at (offsets are from
 *   the chunk start; a section with n == 0 has offset 0):
 *     looks     ShwLook[look_n]       48 B DoomoLook each (resident.h order)
 *     planes    ShwPlane[plane_n]
 *     actors    ShwActor[actor_n]     actor slot = index (BeatEv actor = slot)
 *     props     ShwProp[prop_n]       prop slot = index
 *   An iris centre (BOP_IRIS_IN/OUT a) is 0 = the frame centre, 1..63 = prop
 *   slot + 1 (co02's bandstand, prop 0, is 1), 64 + n = actor slot n.
 *     crowds    ShwCrowd[crowd_n]
 *     camkeys   ShwCamKey[camkey_n]   sorted by frame
 *     events    BeatEv[ev_n]          8 B, beat.h layout, sorted by (anchor, at),
 *                                     ending with BOP_END at the shot's length
 *     args      int32_t[arg_n]        the show ops' argument table (below)
 *     tracks    ShwTrack[track_n] + their runs (ShwRun pairs)
 *     strokes   ShwStroke[stroke_n] + their points (ShwPoint)
 *     strings   NUL-terminated text with the game's inline codes kept as
 *               written ("{d}", "{w}", "{..}"); offsets into it are u16
 *     texpages  ShwTexPage[texpage_n] + their pixel data (8bpp; comp 0 raw,
 *               1 = the game's LZ)
 *     cluts     ShwClut[clut_n] + BGR555 entries
 *
 * SHOW OPS. Engine-native ops (shake, flash, iris, fade, expr, freeze, ...)
 * are plain BeatEv, exactly what core/tools/textpack.py's compile_beat makes.
 * A show op is BeatEv{at, anchor, BOP_SHOT, a = SHW_SOP_FLAG | SOP_*, b = arg
 * index}: args[b] is the op's word count n, args[b+1 .. b+n] its words (layouts
 * by op below). beat.c hands BOP_SHOT back as BT_EXT; show_ext.c runs it.
 * Every sound is SOP_AUDIO carrying its event index k from audio_events.json
 * (the PS1 publishes the k values it fired, P4). The show never emits
 * BOP_SFX / BOP_MUSIC* / BOP_FLAG / BOP_WAIT_A.
 *
 * UNITS. Positions are show pixels on the 320x180 canvas (y 0 = the top of the
 * picture; screen y = y + canvas_y). Q16 = 16.16 fixed point, Q8 = 8.8.
 */
#ifndef SHOW_SHW_FORMAT_H
#define SHOW_SHW_FORMAT_H

#include <stdint.h>

#define SHW_MAGIC        0x31574853u  /* "SHW1" as bytes in the file          */
#define SHW_SHOT_MAGIC   0x544F4853u  /* "SHOT" at the start of every chunk   */
#define SHW_ABI          1
#define SHW_SECTOR       2048
#define SHW_CHUNK_MAX    (160u * 1024u)  /* sec.7.3: a bigger shot splits     */
#define SHW_NONE         0xFFFFu      /* "no shot / no index" in u16 fields   */
#define SHW_SOP_FLAG     0x4000       /* BeatEv.a of a show op on BOP_SHOT    */

#define SHW_ASSERT_SIZE(T, n) typedef char shw_size_##T[(sizeof(T) == (n)) ? 1 : -1]

/* ---- the episode --------------------------------------------------------- */
typedef struct ShwEpHeader {
    uint32_t magic;            /*  0 SHW_MAGIC                                 */
    uint16_t abi;              /*  4 SHW_ABI                                   */
    uint16_t nshots;           /*  6                                           */
    uint8_t  fps;              /*  8 30                                        */
    uint8_t  flags;            /*  9 SHW_EP_*                                  */
    uint16_t canvas_w;         /* 10 320                                       */
    uint16_t canvas_h;         /* 12 180                                       */
    uint16_t canvas_y;         /* 14 30 (show y + 30 = screen y)               */
    uint32_t seed;             /* 16 episode seed                              */
    uint8_t  toolchain_hash[8];/* 20 first 8 bytes of sha256(compiler inputs)  */
    uint32_t strtab_off;       /* 28 file offset of the string table           */
    uint32_t strtab_bytes;     /* 32                                           */
    uint32_t total_frames;     /* 36 sum of every shot's len                   */
} ShwEpHeader;
SHW_ASSERT_SIZE(ShwEpHeader, 40);

enum { SHW_EP_SLUG = 1 };      /* every shot is a slug card (`show slug`)     */

typedef struct ShwShotIndex {
    uint32_t start_frame;      /*  0 episode frame of the shot's frame 0       */
    uint16_t len;              /*  4 frames                                    */
    uint8_t  px;               /*  6 1|2|3                                     */
    uint8_t  look;             /*  7 SHW_LOOK_*                                */
    uint32_t chunk_sector;     /*  8 file offset / SHW_SECTOR                  */
    uint32_t chunk_bytes;      /* 12                                           */
    uint16_t base_shot;        /* 16 shot index of base=, else SHW_NONE        */
    uint16_t base_frame;       /* 18                                           */
    uint16_t replay_shot;      /* 20 shot index of replay=, else SHW_NONE      */
    uint16_t replay_from;      /* 22                                           */
    uint16_t flags;            /* 24 SHW_SHOT_*                                */
    uint16_t name;             /* 26 strtab offset of the shot id ("co01")     */
    uint32_t seed;             /* 28 episode seed ^ shot seed                  */
    uint32_t chunk_crc;        /* 32 CRC-32 (zlib) of the chunk bytes          */
} ShwShotIndex;
SHW_ASSERT_SIZE(ShwShotIndex, 36);

enum { SHW_LOOK_PIXEL, SHW_LOOK_CRAYON, SHW_LOOK_SEPIA, SHW_LOOK_LCD, SHW_LOOK_HELLIFY };
enum {                         /* ShwShotIndex.flags                          */
    SHW_SHOT_SLUG       = 1u << 0,  /* a slug card: draw the beat name + TC   */
    SHW_SHOT_INSERT     = 1u << 1,  /* `insert` (whiplash, owns its audio)    */
    SHW_SHOT_MATTE_BLACK= 1u << 2,  /* matte=black (else open)                */
    SHW_SHOT_TEXT_OFF   = 1u << 3,  /* text=off (textless version)            */
    SHW_SHOT_GORE_BRUTAL= 1u << 4,  /* gore=BRUTAL (else CUTE: filter on)     */
    SHW_SHOT_FADE_IN    = 1u << 5,  /* setup camfx_fade(16) (fade in at +0)   */
    SHW_SHOT_PROVISIONAL= 1u << 6   /* some time is a provisional estimate    */
};
/* the cut INTO this shot, bits 8-11 of flags */
enum { SHW_CUT_CUT, SHW_CUT_SMASH, SHW_CUT_MATCH, SHW_CUT_FADE, SHW_CUT_IRIS, SHW_CUT_WIPE_BRICK };
#define SHW_SHOT_CUT(flags) (((flags) >> 8) & 15)

/* ---- one shot ------------------------------------------------------------- */
typedef struct ShwShotHdr {
    uint32_t magic;            /*  0 SHW_SHOT_MAGIC                            */
    uint8_t  set_kind;         /*  4 SHW_SET_*                                 */
    uint8_t  plane_n;          /*  5                                           */
    uint8_t  actor_n;          /*  6                                           */
    uint8_t  prop_n;           /*  7                                           */
    uint8_t  crowd_n;          /*  8                                           */
    uint8_t  look_n;           /*  9                                           */
    uint8_t  track_n;          /* 10                                           */
    uint8_t  badge_est;        /* 11 SHW_BADGE_*                               */
    uint16_t ev_n;             /* 12                                           */
    uint16_t arg_n;            /* 14 int32 words                               */
    uint16_t camkey_n;         /* 16                                           */
    uint16_t stroke_n;         /* 18                                           */
    uint16_t texpage_n;        /* 20                                           */
    uint16_t clut_n;           /* 22                                           */
    uint16_t strings_bytes;    /* 24                                           */
    uint16_t set_name;         /* 26 strtab offset (episode table) of the set  */
    uint32_t off_looks;        /* 28                                           */
    uint32_t off_planes;       /* 32                                           */
    uint32_t off_actors;       /* 36                                           */
    uint32_t off_props;        /* 40                                           */
    uint32_t off_crowds;       /* 44                                           */
    uint32_t off_camkeys;      /* 48                                           */
    uint32_t off_events;       /* 52                                           */
    uint32_t off_args;         /* 56                                           */
    uint32_t off_tracks;       /* 60                                           */
    uint32_t off_strokes;      /* 64                                           */
    uint32_t off_strings;      /* 68                                           */
    uint32_t off_texpages;     /* 72                                           */
    uint32_t off_cluts;        /* 76                                           */
    uint16_t vram_plan[4];     /* 80 VRAM pages used: set, cast, crowd, text   */
} ShwShotHdr;
SHW_ASSERT_SIZE(ShwShotHdr, 88);

enum { SHW_SET_SLUG, SHW_SET_2D, SHW_SET_VIEW3D, SHW_SET_OVERWORLD, SHW_SET_ROOM };
enum { SHW_BADGE_PS1_LEGAL, SHW_BADGE_EMU_ONLY };

typedef struct ShwLook { uint8_t b[48]; } ShwLook;      /* DoomoLook bytes      */
SHW_ASSERT_SIZE(ShwLook, 48);

typedef struct ShwPlane {
    uint8_t  kind;             /*  0 SHW_PLANE_*                               */
    uint8_t  order;            /*  1 SHW_ORDER_*                               */
    uint8_t  flags;            /*  2 SHW_PLANE_F_*                             */
    uint8_t  boil;             /*  3 boil period in frames (0 = none)          */
    int32_t  par_q16;          /*  4 parallax factor (0.05 = 3277)             */
    uint16_t w, h;             /*  8 plane size in show px                     */
    uint16_t tex;              /* 12 texpage index, SHW_NONE = none            */
    uint16_t clut;             /* 14 clut index, SHW_NONE = none               */
    int16_t  x, y;             /* 16 top-left at camera 0,0                    */
    uint32_t arg;              /* 20 kind-specific: GRAIN density, BANDS seed, */
} ShwPlane;                    /*    STROKES first stroke | count << 16        */
SHW_ASSERT_SIZE(ShwPlane, 24);

enum { SHW_PLANE_BANDS, SHW_PLANE_IMAGE, SHW_PLANE_TILEMAP, SHW_PLANE_STROKES, SHW_PLANE_GRAIN,
       SHW_PLANE_VIEW3D, SHW_PLANE_OVERWORLD, SHW_PLANE_ROOM };
enum { SHW_ORDER_BACK, SHW_ORDER_FRONT, SHW_ORDER_TOP };  /* behind actors / over / over all */
enum { SHW_PLANE_F_TILE = 1, SHW_PLANE_F_OUTSIDE = 2 };

typedef struct ShwActor {
    uint8_t  kind;             /*  0 SHW_ACTOR_*                               */
    uint8_t  look;             /*  1 index into looks (SHW_NONE & 0xFF = none) */
    uint8_t  face;             /*  2 0 = R, 1 = L                              */
    uint8_t  expr;             /*  3 EXPR_*                                    */
    uint8_t  idle;             /*  4 SHW_IDLE_*                                */
    uint8_t  phase;            /*  5 idle clock offset (cast.shw phase=)       */
    uint8_t  px;               /*  6 draw scale                                */
    uint8_t  state;            /*  7 SHW_STATE_*                               */
    int16_t  x, y;             /*  8 anchor (feet centre; bust: bottom centre) */
    uint16_t name;             /* 12 strtab offset of the cast id              */
    uint16_t roster;           /* 14 roster id, SHW_NONE = none                */
} ShwActor;
SHW_ASSERT_SIZE(ShwActor, 16);

enum { SHW_ACTOR_BUST, SHW_ACTOR_FIGURE, SHW_ACTOR_CHIBI, SHW_ACTOR_FIGHTER, SHW_ACTOR_MODEL3D,
       SHW_ACTOR_SPRITE };
enum { SHW_IDLE_ON, SHW_IDLE_STILL, SHW_IDLE_FROZEN, SHW_IDLE_NONE };
enum { SHW_STATE_NORMAL, SHW_STATE_POSSESSED, SHW_STATE_GHOST, SHW_STATE_DEAD };

typedef struct ShwProp {
    uint8_t  plane;            /*  0 plane index it rides                      */
    uint8_t  face;             /*  1 SHW_FACE_*                                */
    uint8_t  anchor;           /*  2 0 = top-left, 1 = centre                  */
    uint8_t  flags;            /*  3                                           */
    int16_t  x, y;             /*  4 in its plane's coordinates                */
    uint16_t art;              /*  8 strtab offset of the art source name      */
    uint16_t tex;              /* 10 texpage index, SHW_NONE = slug box        */
} ShwProp;
SHW_ASSERT_SIZE(ShwProp, 12);

enum { SHW_FACE_FRONT, SHW_FACE_L, SHW_FACE_R, SHW_FACE_WALL };

typedef struct ShwCrowd {
    uint8_t  look_base;        /*  0 first look index (look_k looks, reused)   */
    uint8_t  n;                /*  1 members                                   */
    uint8_t  lane;             /*  2 path index in the set (0xFF = y below)    */
    uint8_t  dir;              /*  3 0 = R, 1 = L                              */
    uint8_t  kind;             /*  4 SHW_ACTOR_*                               */
    uint8_t  ripple_lo;        /*  5 frames                                    */
    uint8_t  ripple_hi;        /*  6                                           */
    uint8_t  age;              /*  7 SHW_AGE_*                                 */
    uint16_t spacing_q8;       /*  8 px between members                        */
    int16_t  from;             /* 10 first member's x                          */
    int16_t  y;                /* 12 lane y + dy                               */
    uint16_t cycle;            /* 14 strtab offset of the walk cycle name      */
    int32_t  speed_q16;        /* 16 px per frame                              */
    uint32_t seed;             /* 20 ripple + resident_randomize_look() seed   */
    uint32_t clock_mask;       /* 24 members whose idle clock runs             */
    uint8_t  gen;              /* 28 SHW_CROWD_LOOKS | SHW_CROWD_RANDOMIZE     */
    uint8_t  species;          /* 29 SPECIES_* forced after the generator, 0xFF keep */
    uint8_t  look_k;           /* 30 looks in the K-set (LOOKS mode)           */
    uint8_t  flags;            /* 31 SHW_CROWD_F_*                             */
} ShwCrowd;
SHW_ASSERT_SIZE(ShwCrowd, 32);

/* LOOKS: member i draws looks[look_base + i % look_k]. RANDOMIZE: member i is
 * resident_randomize_look(seed + i) on the PS1 at shot prep, then `species`
 * and the KID tag (age=child) are applied (spec 4.2). */
enum { SHW_CROWD_LOOKS, SHW_CROWD_RANDOMIZE };
enum { SHW_CROWD_F_KID = 1, SHW_CROWD_F_FLIP = 2 };

enum { SHW_AGE_ADULT, SHW_AGE_TEEN, SHW_AGE_CHILD };

typedef struct ShwCamKey {
    uint16_t frame;            /*  0 shot frame the key starts                 */
    uint8_t  mode;             /*  2 SHW_CAM_*                                 */
    uint8_t  ease;             /*  3 SHW_EASE_*                                */
    int32_t  x_q16, y_q16;     /*  4 camera position at `frame`                */
    int32_t  vx_q16, vy_q16;   /* 12 DRIFT: px per frame from `frame` on        */
    uint16_t scale_q8;         /* 20 256 = 1.0                                 */
    uint16_t over;             /* 22 MOVE: frames to reach the next key        */
} ShwCamKey;
SHW_ASSERT_SIZE(ShwCamKey, 24);

/* SET: jump to (x, y, scale). DRIFT: x = x_q16 + vx_q16 * (f - frame), exact in
 * Q16 (co02: x_q16 = -0.8333*90 px so the camera is at 0 on +90). MOVE: ease
 * from the previous position to this key's over `over` frames. */
enum { SHW_CAM_SET, SHW_CAM_DRIFT, SHW_CAM_MOVE };
enum { SHW_EASE_LINEAR, SHW_EASE_IN, SHW_EASE_OUT, SHW_EASE_INOUT };

typedef struct ShwTrack {
    uint8_t  actor;            /*  0 actor slot                                */
    uint8_t  kind;             /*  1 SHW_TRACK_*                               */
    uint16_t run_n;            /*  2                                           */
    uint32_t off_runs;         /*  4 from the chunk start                      */
} ShwTrack;
SHW_ASSERT_SIZE(ShwTrack, 8);
typedef struct ShwRun { uint8_t value, count; } ShwRun;  /* RLE from frame 0  */
SHW_ASSERT_SIZE(ShwRun, 2);
/* MOUTH value = MOUTH_* + 1 (0 = the engine's own); BLINK 0 = engine, 1 = open,
 * 2 = shut; EXPR value = EXPR_* + 1. */
enum { SHW_TRACK_MOUTH, SHW_TRACK_EXPR, SHW_TRACK_EYES, SHW_TRACK_BLINK };

typedef struct ShwStroke {
    uint8_t  nib;              /*  0 palette index of the crayon               */
    uint8_t  width;            /*  1 px                                        */
    uint8_t  flags;            /*  2 SHW_PLANE_F_OUTSIDE = draws over the lines*/
    uint8_t  boil;             /*  3                                           */
    uint16_t pt_n;             /*  4                                           */
    uint16_t reveal_from;      /*  6 shot frame the reveal starts              */
    uint16_t reveal_to;        /*  8 shot frame it reaches 100 %               */
    uint16_t name;             /* 10 strtab offset of the stroke id            */
    uint32_t off_pts;          /* 12 ShwPoint[pt_n] from the chunk start       */
} ShwStroke;
SHW_ASSERT_SIZE(ShwStroke, 16);
typedef struct ShwPoint { int16_t x, y; } ShwPoint;
SHW_ASSERT_SIZE(ShwPoint, 4);

typedef struct ShwTexPage {
    uint16_t w, h;             /*  0 px                                        */
    uint8_t  bpp;              /*  4 8 (4 later)                               */
    uint8_t  comp;             /*  5 0 raw, 1 game LZ                          */
    uint16_t name;             /*  6 strtab offset of the source               */
    uint32_t bytes;            /*  8 stored bytes                              */
    uint32_t off;              /* 12 from the chunk start                      */
} ShwTexPage;
SHW_ASSERT_SIZE(ShwTexPage, 16);

typedef struct ShwClut {
    uint16_t n;                /*  0 entries                                   */
    uint16_t pad;              /*  2                                           */
    uint32_t off;              /*  4 uint16_t BGR555[n] from the chunk start   */
} ShwClut;
SHW_ASSERT_SIZE(ShwClut, 8);

/* ---- the show ops (BeatEv.a = SHW_SOP_FLAG | op) ---------------------------
 * Word layouts in the arg table after the count word. Frames are shot frames.
 * "str" = byte offset into the chunk's strings; "q16" = 16.16.            */
enum {
    SOP_NOP = 0,
    SOP_AUDIO,       /* k                                   (P4: publish k)      */
    SOP_MOVE,        /* actor, x, y, over, TW_* ease id                          */
    SOP_WALK,        /* actor, x, over                                           */
    SOP_SQUASH,      /* actor, q8, over                                          */
    SOP_FACE,        /* actor, 0 = R | 1 = L                                     */
    SOP_POSE,        /* actor, str flipbook, fps                                 */
    SOP_BLINK,       /* actor, frames                                            */
    SOP_STATE,       /* actor, SHW_STATE_*                                       */
    SOP_SAY,         /* actor (-1 = off screen), line str, end frame, mouth track */
    SOP_STOP,        /* actor                                                    */
    SOP_HUM,         /* actor, on                                                */
    SOP_SUPER,       /* str, style (SHW_TXT_*), x, y, from, to, until, glyphs    */
    SOP_TEXT_STEP,   /* super event index, glyphs shown                          */
    SOP_TEXT_CLEAR,  /* super event index                                        */
    SOP_SIGN,        /* str, prop slot                                           */
    SOP_CARD,        /* str template, str k=v text                               */
    SOP_CAM,         /* camkey index                                             */
    SOP_CAM_DRIFT,   /* camkey index                                             */
    SOP_CAM3D,       /* str rail, from, to                                       */
    SOP_REVEAL,      /* stroke index, from, to                                   */
    SOP_FX,          /* str kind, x, y, n, seed                                  */
    SOP_BOIL,        /* on, every                                                */
    SOP_PROP,        /* prop slot, field (SHW_PF_*), value  (`change`, base=)    */
    SOP_IRIS,        /* in(1)|out(0), x, y, shape (0 circle, 1 heart), over, hold */
    SOP_COUNT
};
enum { SHW_TXT_TYPE, SHW_TXT_POP, SHW_TXT_DEADPAN };
enum { SHW_PF_FACE, SHW_PF_X, SHW_PF_Y, SHW_PF_HIDE };

#endif
