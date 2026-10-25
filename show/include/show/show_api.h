/* show_api.h - the show player's PLUG-IN API (owner: ENGINE lane 1; spec 8.1,
 * 8.2, 8.9). Stable: never change a signature or a struct field without
 * updating every user on origin in the same push (add-only otherwise).
 * Documented for humans in show/README.md, "Plug-in API".
 *
 * THE MODEL. The player core (show_game.c, show_load.c, show_mbox.c,
 * show_purity.c, show_ext.c, show_stage.c, show_hash.c, show_stats.c) owns
 * the episode, the shot loop, the mailbox and the hashes. Everything that
 * draws or animates is a MODULE: one `const ShowModule show_mod_<name>` in
 * the package's own .c file. show_registry.c lists every module by name as
 * a WEAK reference, in a fixed order, so a module that is not linked yet is
 * simply skipped and no package ever edits the registry to plug in (the
 * names are reserved there already; a new one is a one-line add).
 *
 * A module may also bring PLANE KINDS (SHWP_*, shw_format.h): a table of
 * ShowPlaneKind ending with kind 0. The core walks the shot's planes in
 * `order` and calls the matching kind's draw. Stage-kind hosts (VIEW3D,
 * OVERWORLD, ROOM) are plane kinds. Actor kinds, text styles and FX are
 * entries in their own module's tables (the core only needs the module).
 *
 * PER-FRAME CALL ORDER (one 30 Hz tick, spec 8.1)
 *   1  hcmd_poll()                      (ps1/src/main.c, the lockstep stop)
 *   2  show_mbox_poll()                 seek requests from Lua
 *   3  at a shot start (t = 0, a cut or a seek):
 *        bind the chunk;  core reset (camfx_reset, portrait_cache_reset,
 *        RNG = episode seed ^ shot seed, beat script, plane table);
 *        module->reset()   for every module, registry order    [PURITY]
 *        plane kind->prep() for every plane of the shot
 *        module->prep(shot) for every module                   [find your records]
 *        then `preroll` + seek frames of logic (steps 4-5) with drawing skipped
 *   4  camfx_tick(); beat_tick() until BT_RUN/BT_DONE; every BT_EXT event
 *        goes to module->ext(ev) in registry order until one returns 1;
 *        the core handles what is left (IRIS, TINT, POP, EXPR, show ops 0x000-0x03F)
 *   5  module->tick(t)                   every frame, also during pre-roll/seek
 *   6  draw (skipped during pre-roll/seek: show_drawing() == 0):
 *        plat_clear(bg); planes by order (kind->draw); module->draw(SHOW_L_PLANES)
 *        module->draw(SHOW_L_ACTORS)     actors
 *        module->draw(SHOW_L_CROWDS)     crowds and props
 *        module->draw(SHOW_L_FX)         particles, wipes
 *        module->draw(SHOW_L_TEXT_UNDER) text a fade must cover
 *        camfx_apply()                   fades, flashes, irises, palette lerps
 *                                        (its shake OFFSET is a no-op in the show:
 *                                         the stage camera applies the shake, spec 2.3)
 *        module->draw(SHOW_L_TEXT)       SUPERs, cards (steady, over the fade)
 *        module->draw(SHOW_L_HUD)        the first-person HUD
 *        matte bars (core, last)
 *   7  P1 = CRC32 of the core record + module->hash(); P4; stats -> g_show_mbox
 *   8  plat_end_frame()                  (main.c)
 *
 * PURITY (spec 8.2): a shot's frames depend on nothing but its chunk, the
 * seeds and the frame number. reset() must put EVERY static of the module
 * back to its boot value. Seeding: use show_rng() (seeded per shot) or your
 * own state seeded from show_shot()->seed in prep; never plat_entropy(),
 * never a counter that runs across shots. Portrait fetches during pre-roll
 * still happen (the cache state then matches a full run): fetch in tick(),
 * or fetch in draw() and call show_actor-side helpers that keep fetching
 * when !show_drawing() (lane 2 decides; the rule is "same fetches, same order").
 *
 * COORDINATES: modules draw with the plat_* calls in SCREEN pixels; the
 * 320x180 picture is screen rows SHOW_CANVAS_Y..+180 (SHOW_Y(y) below).
 */
#ifndef SHOW_API_H
#define SHOW_API_H

#include <stdint.h>
#include "doomo/beat.h"
#include "show/shw_format.h"
#include "show/show_game.h"

#define SHOW_API_VERSION 1
#define SHOW_Y(y)  ((y) + SHOW_CANVAS_Y)

/* ---- the current shot (read-only for modules) ---------------------------- */
typedef struct ShowShot {
    int                  index;     /* shot index in the episode, -1 = none (chart) */
    const ShwShotIndex  *ix;        /* its index entry                            */
    const ShwShotHdr    *hdr;       /* its chunk                                  */
    int                  len;       /* frames                                     */
    int                  px;        /* 1|2|3                                      */
    uint32_t             seed;      /* episode seed ^ shot seed                   */
    int                  t;         /* shot frame being made (0..len-1); pre-roll
                                       frames are negative: -preroll..-1           */
} ShowShot;

const ShowShot *show_shot(void);
int  show_drawing(void);            /* 0 during pre-roll and seek (skip plat_* calls) */
uint32_t show_episode_frame(void);  /* start_frame + t                             */

/* ---- reading the chunk ------------------------------------------------------ *
 * Each record type appears at most once per chunk (the compiler merges). */
const void *show_chunk_find(uint16_t type, int *n, uint32_t *bytes); /* payload or NULL */
const char *show_str(uint32_t off);         /* SHWR_STRINGS + off ("" if none)     */
const int32_t *show_args(int b);            /* SHWR_ARGS + b (a static zero row if out of range) */
int  show_nargs(void);
/* SHWR_IMAGE item i as a DoomoImage (decoded at prep into the shot's image
 * pool, fresh uid per shot so the VRAM cache never serves a stale plate) */
const struct DoomoImage *show_image(int i);
int  show_image_count(void);

/* planes: the shot's ShwPlane list, in draw order */
int  show_plane_count(void);
const ShwPlane *show_plane(int i);           /* i-th in draw order            */
const void *show_plane_payload(const ShwPlane *p);
const ShwPlane *show_plane_by_id(int id);
int  show_plane_visible(int id);             /* SOP_PLANE_SHOW/HIDE state     */
void show_plane_set_visible(int id, int on);

/* ---- the plane transform (the stage camera plugs in here) ------------------ *
 * Where plane pixel (0,0) lands this frame and how big a plane pixel is:
 * screen show-px X = ox + u * s (s Q16; s includes the shot's px). Field order
 * matches lane 2's ShowXform (show_cam.h): ox, oy, fx, fy, s.
 * Core default (no camera module): ox = at_x, oy = at_y, fractions 0, s = px.
 * The camera module installs its own in prep() (the core clears both hooks
 * at every shot start); every lane-1 plane kind goes through these two calls,
 * so parallax, scale moves and the shake reach BANDS/IMAGE/GRAIN unchanged. */
typedef struct ShowPlaneXf { int32_t ox, oy, fx, fy, s; } ShowPlaneXf;
void show_plane_xf(const ShwPlane *p, ShowPlaneXf *out);
/* draw an image under a transform (screen y = show y + SHOW_CANVAS_Y) */
void show_draw_image(const struct DoomoImage *img, const ShowPlaneXf *xf, int flags);
extern void (*g_show_plane_xf_hook)(const ShwPlane *p, int px, ShowPlaneXf *out);
extern void (*g_show_draw_image_hook)(const struct DoomoImage *img, const ShowPlaneXf *xf, int flags);

/* ---- modules ---------------------------------------------------------------- */
enum { SHOW_L_PLANES, SHOW_L_ACTORS, SHOW_L_CROWDS, SHOW_L_FX,
       SHOW_L_TEXT_UNDER, SHOW_L_TEXT, SHOW_L_HUD, SHOW_L_COUNT };

typedef struct ShowCrc { uint32_t v; } ShowCrc;
void show_crc_add(ShowCrc *c, const void *data, uint32_t n);   /* CRC32 (zlib) update */
uint32_t show_crc32(uint32_t crc, const void *data, uint32_t n);

typedef struct ShowPlaneKind {
    uint8_t     kind;                                   /* SHWP_*; 0 ends a table   */
    const char *name;
    void (*prep)(const ShwPlane *p, const void *payload);         /* may be NULL */
    void (*draw)(const ShwPlane *p, const void *payload, int t);  /* only when drawing */
} ShowPlaneKind;

typedef struct ShowModule {
    const char *name;
    uint16_t    api;                                    /* SHOW_API_VERSION         */
    uint16_t    pad;
    void (*reset)(void);                                /* shot start: boot state   */
    void (*prep)(const ShowShot *s);                    /* after the chunk is bound */
    int  (*ext)(const BeatEv *ev);                      /* 1 = handled              */
    void (*tick)(int t);                                /* every frame (pre-roll too) */
    void (*draw)(int layer, int t);                     /* SHOW_L_*, when drawing   */
    void (*hash)(ShowCrc *c);                           /* P1: your state, fixed layout */
    const ShowPlaneKind *planes;                        /* NULL or {..., {0}}       */
} ShowModule;
/* every pointer may be NULL. Define as:
 *   const ShowModule show_mod_cam = { "cam", SHOW_API_VERSION, 0, cam_reset, ... };
 * and check that `show_mod_cam` is reserved in show_registry.c. */

int  show_module_count(void);
const ShowModule *show_module(int i);                   /* linked modules only     */

/* ---- show ops and host kinds (BT_EXT) --------------------------------------- */
static inline int show_op_of(const BeatEv *ev)          /* -1 if not a show op     */
{
    return (ev && ev->op == BOP_SHOT && (ev->a & SHOW_OP_FLAG)) ? (ev->a & 0x3FFF) : -1;
}
/* core defaults for host kinds, readable by any module:
 *   BOP_EXPR  actor a: expr = b & 0xFF, mouth = b >> 8 (0 = engine)  -> show_actor_chan()
 *   BOP_POP   object a: the frame it popped and its TWT table      -> show_pop_age()  */
typedef struct ShowActorChan { int16_t expr, mouth; int16_t expr_t, pad; } ShowActorChan;
const ShowActorChan *show_actor_chan(int actor);        /* 0..BEAT_ACTORS-1        */
int  show_pop_age(int obj, int *twt);                   /* frames since BOP_POP, -1 */

/* ---- randomness (seeded per shot) ------------------------------------------- */
uint32_t show_rng(void);                                /* xorshift32, per shot    */
uint32_t show_hash32(uint32_t x);                       /* stateless mix           */

/* ---- stats and diagnostics (spec 8.9, 8.10) --------------------------------- */
enum { SHOW_STAT_UPLOADS, SHOW_STAT_EVICTIONS, SHOW_STAT_POOL_BLOCKS,
       SHOW_STAT_SPRITES, SHOW_STAT_USER0, SHOW_STAT_COUNT };
void show_stat_add(int stat, uint32_t n);               /* this frame              */
void show_stat_max(int stat, uint32_t n);               /* this frame, the max     */
/* one TTY line "show: W co02 +150 [RULE] msg" (the harness logs it with the
 * frame; capture fails on "show: E") */
void show_warn(const char *rule, const char *msg);
void show_error(const char *rule, const char *msg);

/* audio event ids k fired this frame (P4): a module that schedules sound
 * events by itself calls this; SHWR_AUDIO_K entries are fired by the core */
void show_p4_fire(uint32_t k);

#endif
