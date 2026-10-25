/* show_actor.h - WP6: the cast, actors and crowds of the show player
 * (SHOW_ENGINE_SPEC.md sec.4.1, 7.3, 8.2, 8.4, 8.5, 9.3; ENGINE lane 2).
 *
 *   show_cast.c   LOOK[] -> doomo_town.res[0..255] at shot prep, so the game's
 *                 own town_res() serves every bust / chibi / fighter fetch;
 *                 pins doomo_town.opt[] to the show profile.
 *   show_actor.c  busts, figures, chibis, fighters; per-actor clocks; the
 *                 expression / mouth / blink tracks; flipbook poses; motion
 *                 (move, walk, pop, squash, face, headturn); the portrait
 *                 override variables of the patch hunks (b)-(d).
 *   show_crowd.c  crowds: lanes and formations, 0-3 f ripples, stoppable
 *                 clocks, K-look reuse (K <= 48 looks drawn many times).
 *
 * Every record below is little-endian, 4-byte aligned, and has a fixed size
 * (static-asserted here, mirrored by show/tools/showtk/actors.py and checked
 * by show/tests/engine/wp6). The player hands the module the raw bytes of a
 * shot's LOOK / ACTR / CRWD / TRAK / FLIP sections; nothing is copied except
 * the small per-actor state.
 *
 * Frame protocol (one shot; the player calls, show_ext dispatches ops):
 *   prep:   show_wp6_reset(gore)                  every shot start (purity)
 *           show_cast_load(looks, n)              LOOK chunk
 *           show_actor_load(recs, n)              ACTR
 *           show_actor_tracks(blob, bytes)        TRAK
 *           show_actor_flips(blob, bytes)         FLIP (flipbook frames)
 *           show_crowd_load(recs, n)              CRWD
 *   tick t: beat_tick() ... BT_EXT -> show_actor_op / show_crowd_op
 *           show_wp6_tick(t)                      after the beat ops of frame t
 *   draw:   show_crowd_draw(ox, oy, SHOW_LAYER_BACK, draw)
 *           show_actor_draw(ox, oy, draw)
 *           show_crowd_draw(ox, oy, SHOW_LAYER_FRONT, draw)
 *   (ox, oy) = the screen position of show pixel (0, 0) of the actors' plane
 *   (y_off 30 plus camera and shake); draw = 0 during a seek's pre-roll: every
 *   image is still fetched, so the portrait cache matches a full run (8.2).
 */
#ifndef SHOW_ACTOR_H
#define SHOW_ACTOR_H

#include "doomo/doomo.h"
#include "doomo/resident.h"

#define SHOW_MAX_LOOKS   256     /* doomo_town.res[0..255]                     */
#define SHOW_MAX_ACTORS  16      /* actors 0..7 own beat.c's clocks 0..7       */
#define SHOW_MAX_CROWDS  8
#define SHOW_CROWD_MAX_N 256     /* citizens per crowd                         */
#define SHOW_CROWD_MAX_K 48      /* unique looks per crowd (sec.8.5)           */

/* ---- LOOK: 48-byte DoomoLook records (resident.h), slot i = res[i] ---------- */

/* ---- ACTR: one actor (32 B) ------------------------------------------------- */
enum { SHA_BUST, SHA_FIGURE, SHA_CHIBI, SHA_FIGHTER, SHA_KIND_COUNT };
enum { SHI_ON, SHI_STILL, SHI_FROZEN, SHI_NONE };          /* idle= */
enum { SHFACE_R, SHFACE_L, SHFACE_FRONT, SHFACE_BACK };     /* face= */
enum { SHAF_HIDDEN = 1 };                                  /* flags */
/* visemes (sec.9.3), the values of a MOUTH track */
enum { VIS_X, VIS_M, VIS_C, VIS_E, VIS_A, VIS_O, VIS_U, VIS_F, VIS_COUNT };
#define SHOW_NO_MOUTH 0xFF       /* vis[] entry: the look's own mouth          */

typedef struct ShowActorRec {
    uint8_t kind;                /* SHA_*                                      */
    uint8_t look;                /* LOOK slot (doomo_town.res index)           */
    uint8_t face;                /* SHFACE_*                                   */
    uint8_t expr;                /* EXPR_* at frame 0                          */
    int16_t x, y;                /* bottom-centre (feet), show px on its plane */
    uint8_t idle;                /* SHI_*                                      */
    uint8_t phase;               /* idle clock offset (cast.shw phase=)        */
    uint8_t px;                  /* draw scale 1..3                            */
    uint8_t status;              /* RES_* at frame 0 (state=)                  */
    uint8_t clock;               /* beat.c actor clock 0..7; 0xFF = shot clock */
    uint8_t flags;               /* SHAF_*                                     */
    uint8_t view;                /* fighter: view 0..4 / chibi: dir override   */
    uint8_t wframe;              /* fighter weapon frame, 0xFF = none          */
    uint8_t vis[VIS_COUNT];      /* viseme -> MOUTH_* (SHOW_NO_MOUTH = rest)   */
    uint8_t pad[8];
} ShowActorRec;
DOOMO_STATIC_ASSERT(sizeof(ShowActorRec) == 32, show_actor_rec_32);

/* ---- TRAK: per-frame channel tracks, run-length coded ------------------------
 * blob = ShowTrackHdr, then its runs (len 1..255, value), then the next track
 * header at the next 4-byte boundary, until `bytes` is used up. */
enum { SHT_MOUTH, SHT_EXPR, SHT_BLINK, SHT_EYES /* deferred: gaze */ };
#define SHOW_TRACK_KEEP 0xFF     /* a run that leaves the channel alone        */
typedef struct ShowTrackHdr {
    uint8_t  actor, chan;        /* actor index, SHT_*                         */
    uint16_t start;              /* shot frame of the first run                */
    uint16_t nruns, frames;      /* frames = sum of the run lengths            */
} ShowTrackHdr;                  /* + nruns x { uint8 len, uint8 value }       */
DOOMO_STATIC_ASSERT(sizeof(ShowTrackHdr) == 8, show_track_hdr_8);

/* ---- FLIP: flipbook poses ------------------------------------------------------
 * blob = ShowFlipHdr, n hold bytes padded to 4, then n * w * h 8bpp pixels
 * (index 0 transparent) padded to 4, then the next flipbook. A pose is drawn
 * at the actor's anchor: its bottom-centre + (dx, dy) * px, flipped with the
 * actor. mode REPLACE hides the actor's own image, OVER draws on top of it. */
enum { SHFM_REPLACE, SHFM_OVER };
enum { SHFL_LOOP = 1 };
typedef struct ShowFlipHdr {
    uint16_t w, h;
    uint8_t  n, mode, flags, pad;
    int8_t   dx, dy, pad2[2];
} ShowFlipHdr;                   /* + uint8 hold[n] (frames per drawing)       */
DOOMO_STATIC_ASSERT(sizeof(ShowFlipHdr) == 12, show_flip_hdr_12);

/* ---- CRWD: one crowd (48 B) ----------------------------------------------------- */
enum { SHC_LANE, SHC_GRID, SHC_SCATTER };                  /* form=          */
enum { SHCF_FRONT = 1,        /* draw in front of the actors                    */
       SHCF_FLIPMIX = 2,      /* standing members mirror at random (variety)    */
       SHCF_BOB = 4,          /* standing members bob 1 px on their own phase   */
       SHCF_NOCLOCK = 8 };    /* clock none: never moves (Mascotty as a crowd)  */
typedef struct ShowCrowdRec {
    uint8_t  form;               /* SHC_*                                      */
    uint8_t  look_base;          /* first LOOK slot                            */
    uint8_t  k;                  /* unique looks, 1..48                        */
    uint8_t  dir;                /* chibi dir 0 front, 1 left, 2 right, 3 back */
    uint16_t n;                  /* citizens                                   */
    uint16_t seed;               /* ripples, look order, scatter, bob phases   */
    int16_t  x, y;               /* lane: the first citizen's start (feet);
                                    grid / scatter: the top-left of the area  */
    int16_t  w, h;               /* grid / scatter: the area; lane: h = y jitter */
    int16_t  spacing_q4;         /* lane: px between citizens (Q4); grid: column pitch */
    int16_t  row_q4;             /* grid: row pitch (Q4); others 0             */
    int32_t  speed_q16;          /* px per frame along x (lane; signed)        */
    uint8_t  ripple;             /* max stagger 0..3 f (hash of the member id) */
    uint8_t  step;               /* frames per walk frame (walk4: 4)           */
    uint8_t  clock;              /* beat.c clock 0..7 it follows; 0xFF = its own */
    uint8_t  flags;              /* SHCF_*                                     */
    uint8_t  px;                 /* draw scale 1..2                            */
    uint8_t  nspecial;           /* special members used (0..4)                */
    uint8_t  pad[2];
    uint16_t special_idx[4];     /* member index of each special member        */
    uint8_t  special_look[4];    /* its own LOOK slot                          */
    uint8_t  special_flags[4];   /* SHCF_NOCLOCK: it never moves (Mascotty)    */
} ShowCrowdRec;
DOOMO_STATIC_ASSERT(sizeof(ShowCrowdRec) == 48, show_crowd_rec_48);

/* ---- ops (show_ext.c's BT_EXT / BOP_SHOT dispatch; one call each) -------------- */
enum {
    SHOP_EXPR,        /* a = EXPR_*                                  (BOP_EXPR)    */
    SHOP_BLINK,       /* a = frames shut (0 = R3)                                  */
    SHOP_FACE,        /* a = SHFACE_*                                              */
    SHOP_MOVE,        /* a = x, b = y, c = frames, d = SHE_* ease                  */
    SHOP_WALK,        /* a = x, c = frames                                         */
    SHOP_POP,         /* a = TWT_*                                   (BOP_POP)     */
    SHOP_SQUASH,      /* a = height Q8 (256 rest), c = frames                      */
    SHOP_HEADTURN,    /* a = brad (64 = a quarter turn), c = frames  (BOP_HEADTURN) */
    SHOP_POSE,        /* a = flipbook index (-1 = off), b = speed Q8 (256 = as drawn) */
    SHOP_STATE,       /* a = RES_*                                                 */
    SHOP_TWITCH,      /* a = 0/1 (deferred: the engine twitches POSSESSED itself)  */
    SHOP_SHOW,        /* a = 0 hide / 1 show                                       */
    SHOP_COUNT
};
enum { SHE_LINEAR, SHE_SMOOTH, SHE_IN, SHE_OUT, SHE_BOUNCE };
/* crowd ops: a crowd index plus */
enum {
    SHCO_FREEZE,      /* the crowd's clock stops, member i r_i frames late         */
    SHCO_THAW,        /* and resumes the same way                                  */
    SHCO_POP,         /* a = TWT_* on every member, rippled (the snort-laugh hop)  */
    SHCO_FACE,        /* a = chibi dir, rippled                                    */
    SHCO_SPEED,       /* a = speed Q8 px/f (lane), rippled                         */
    SHCO_SHOW,        /* a = 0 hide / 1 show                                       */
    SHCO_COUNT
};

/* ---- show_cast.c ------------------------------------------------------------- */
void show_cast_reset(int gore);                 /* clears res[], pins doomo_town.opt[] */
int  show_cast_load(const DoomoLook *looks, int n);  /* returns slots loaded       */
void show_cast_set_status(int slot, int status);
int  show_cast_status(int slot);
int  show_cast_count(void);

/* ---- show_actor.c ------------------------------------------------------------ */
/* portrait_show_mouth / portrait_show_blink: show/show_portrait.h */
void show_actor_reset(void);
int  show_actor_load(const ShowActorRec *recs, int n);
int  show_actor_tracks(const uint8_t *blob, uint32_t bytes);  /* returns tracks   */
int  show_actor_flips(const uint8_t *blob, uint32_t bytes);   /* returns flipbooks */
void show_actor_op(int actor, int op, int a, int b, int c, int d);
void show_actor_tick(int t);                    /* shot frame t                    */
void show_actor_draw(int ox, int oy, int draw);
int  show_actor_count(void);
/* read-backs for P1, tests and sheets */
typedef struct ShowActorState {
    int16_t x, y;                /* anchor, show px (after motion)             */
    int16_t dy;                  /* pop / walk bob offset (px)                 */
    int16_t sx, sy;              /* Q8 scale (squash, pop, headturn), sign = flip */
    int32_t clock;               /* the portrait clock this frame              */
    int8_t  expr, mouth, blink;  /* mouth / blink: the override (-1 = none)    */
    int8_t  dir, frame, pose;    /* chibi dir + walk frame; pose frame (-1)    */
    uint32_t uid;                /* uid of the image fetched this frame        */
} ShowActorState;
const ShowActorState *show_actor_state(int actor);
uint32_t show_actor_hash(uint32_t h);           /* folds every actor channel  */

/* ---- show_crowd.c ------------------------------------------------------------ */
void show_crowd_reset(void);
int  show_crowd_load(const ShowCrowdRec *recs, int n);
void show_crowd_op(int crowd, int op, int a);
void show_crowd_tick(int t);
void show_crowd_draw(int ox, int oy, int layer, int draw);
enum { SHOW_LAYER_BACK, SHOW_LAYER_FRONT };
/* member i of crowd c this frame (tests, P1): feet x/y, dir, walk frame, the
 * member's clock and its ripple; returns 0 if out of range */
typedef struct ShowMember {
    int16_t x, y; int8_t dir, frame, flip, ripple; int32_t clock; uint8_t look, hidden, pad[2];
} ShowMember;
int  show_crowd_member(int crowd, int i, ShowMember *out);
typedef struct ShowCrowdStats {
    uint32_t drawn;              /* chibi SPRTs drawn this frame               */
    uint32_t fetched;            /* image fetches this frame                   */
    uint32_t rerenders;          /* fetches that rendered an image again that
                                    was cached before: a pool eviction         */
    uint32_t keys;               /* distinct (look, dir, frame, flip) images in use  */
    uint32_t newkeys;            /* of those, first used in this shot this frame */
} ShowCrowdStats;
const ShowCrowdStats *show_crowd_stats(void);
uint32_t show_crowd_hash(uint32_t h);

/* ---- all three --------------------------------------------------------------- */
void show_wp6_reset(int gore);                  /* show_cast/actor/crowd_reset +
                                                   portrait_cache_reset()      */
void show_wp6_tick(int t);                      /* actor + crowd tick          */

/* ---- show_ext.c plugs these in (one line each) --------------------------------
 * show_wp6_ext(e): the engine-native host kinds beat.c hands back that act on
 * an actor: BOP_EXPR (a actor, b EXPR_* | mouth << 8: the mouth half is
 * ignored, mouths come from tracks), BOP_POP (a actor < SHOW_MAX_ACTORS, b
 * TWT_*), BOP_HEADTURN (a actor, b brad | frames << 8), BOP_TWITCH. Returns 1
 * when it handled e (a BOP_POP on a prop id returns 0 for the prop host).
 * FREEZE / THAW / PANIC / STILL never reach the host: beat.c runs the clocks
 * itself (actor i = beat.c clock i).
 * show_wp6_sop(op, argv, argc): the show ops on BOP_SHOT (a = 0x4000 | op)
 *   SHOW_SOP_ACTOR argv = { actor, SHOP_*, a, b, c, d }
 *   SHOW_SOP_CROWD argv = { crowd, SHCO_*, a }
 * Returns 1 when op is one of them. */
#define SHOW_SOP_ACTOR 0x60
#define SHOW_SOP_CROWD 0x61
struct BeatEv;
int show_wp6_ext(const struct BeatEv *e);
int show_wp6_sop(int op, const int16_t *argv, int argc);

#endif
