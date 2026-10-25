/* platform.h - everything the core needs from a platform.
 *
 * Implemented three times:
 *   gba/    - real GBA ROM (Mode 4 8bpp, height raycaster, PSG + DirectSound,
 *             128 KB flash save)
 *   ps1/    - real PS1 disc (GPU + GTE true-3D engine, SPU, memory card)
 *   core/native/ - headless test runner (software 8bpp framebuffer, PNG
 *             screenshots, scripted input) used by tests and CI
 *
 * Frame protocol (platform main loop, 30 Hz logic):
 *     plat_init_hw(); doomo_init();
 *     for (;;) { buttons = read_pad(); plat_begin_frame();
 *                doomo_frame(buttons); plat_end_frame(); }
 * doomo_frame() issues all drawing for the frame in painter's order:
 * usually one plat_view3d()/plat_overworld() first, then 2D on top.
 *
 * All colours are master-palette indices (palette.h). All coordinates are
 * screen pixels in a plat_screen_w() x plat_screen_h() space (GBA 240x160,
 * PS1 320x240). Everything must clip safely against the screen.
 */
#ifndef DOOMO_PLATFORM_H
#define DOOMO_PLATFORM_H

#include "types.h"
#include "map.h"

/* ---- display ------------------------------------------------------------ */
int  plat_screen_w(void);
int  plat_screen_h(void);
void plat_begin_frame(void);
void plat_end_frame(void);

/* ---- 2D layer ----------------------------------------------------------- */
void plat_clear(uint8_t color);
void plat_rect(int x, int y, int w, int h, uint8_t color);
/* Darken/tint a region through doomo_colormap[level] (level 0..15) -
 * used for modal dimming. GBA: colormap remap; PS1: semi-transparent quad. */
void plat_shade_rect(int x, int y, int w, int h, int level);
void plat_line(int x0, int y0, int x1, int y1, uint8_t color);
/* Text in the core font (font.h): 6 px advance, 8 px line, ASCII + icon
 * glyphs 0x80..0x9F. '\n' starts a new line at the original x. */
void plat_text(int x, int y, const char *s, uint8_t color);
/* Outlined text (foundation, additive): plat_text(x, y, s, color) over an
 * outline of `outline` at the eight 1-pixel neighbours and at (+1, +2),
 * i.e. exactly what ui_text_outline() drew as ten plat_text passes. The
 * core has a weak default (core/src/ui.c) that draws those passes; a
 * platform implements it to draw the outline in one pass (GBA: dilated
 * glyph masks; PS1: a dilated glyph page). */
void plat_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline);
/* Draw an 8bpp image; index 0 transparent. scale 1..4 (integer
 * nearest-neighbour). flags: IMG_FLIPX, IMG_SHADOW (1px dark outline
 * drop shadow), IMG_SILHOUETTE (all opaque pixels drawn as `color`). */
enum { IMG_FLIPX = 1, IMG_SHADOW = 2, IMG_SILHOUETTE = 4 };
void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color);
/* 50% checkerboard in `color` (foundation, additive): pixel (x+i, y+j) of
 * the rect is drawn when i + j is even, the others are left alone. The UI
 * uses it for the soft one-row seams between colour bands. The core has a
 * weak default (one plat_rect per pixel, core/src/ui_raster.c), so a
 * platform only implements it to be faster; it may also draw it as a
 * true 50% blend of `color` over the rect (PS1). */
void plat_checker(int x, int y, int w, int h, uint8_t color);
/* 1 while the frame being built will not be shown (foundation, additive):
 * the GBA skips the drawing of a catch-up tick when it falls behind (its
 * plat_* calls then do nothing). The core's pure drawing helpers return
 * early then, so such ticks cost little. Weak default (core/src/
 * ui_raster.c): 0, every frame is shown. */
int  plat_draw_skipped(void);

/* ---- 3D views ------------------------------------------------------------ */
/* A billboard sprite placed in the world. */
typedef struct DoomoBillboard {
    fix16 x, y, z;              /* z = world height of the sprite's FEET  */
    const DoomoImage *img;
    uint16_t scale;             /* 8.8: 256 = 1 texel per world unit      */
    uint8_t  flags;             /* BB_* */
    uint8_t  light;             /* 0..15 extra brightness (fullbright fx) */
} DoomoBillboard;
enum { BB_FLIPX = 1, BB_FULLBRIGHT = 2, BB_TRANSLUCENT = 4, BB_SHADOW = 8 };

typedef struct DoomoView3D {
    const DoomoMap *map;
    fix16   x, y, z;            /* eye position, world units (z absolute)  */
    int16_t angle;              /* 0..4095 yaw                              */
    int16_t pitch;              /* -256..256: look down..up (~ +-40 deg)    */
    int16_t bob;                /* view bob offset in world units           */
    int16_t vx, vy, vw, vh;     /* screen viewport                          */
    const DoomoBillboard *sprites;
    int     sprite_count;
    uint8_t tint;               /* TINT_* (palette.h)                       */
    uint8_t tint_level;         /* 0..15 strength                           */
    uint16_t time;              /* ticks, for animated textures / sky       */
    uint8_t camera;             /* CAM_FIRST_PERSON / CAM_THIRD_PERSON      */
    int8_t  player_sprite;      /* index into sprites[] of the player's own
                                   body in third person (-1 = none). The
                                   renderer must never clip the camera into
                                   it; it may draw it translucent when the
                                   camera is very close.                   */
} DoomoView3D;
enum { CAM_FIRST_PERSON, CAM_THIRD_PERSON };

/* 3D view of the map from (x,y,z,angle,pitch): walls, step faces,
 * textured floors and ceilings, sky, per-cell light + distance fade,
 * billboards clipped against geometry. Used for both first-person and
 * third-person (the core positions an over-the-shoulder camera behind the
 * player, collision-checked against walls, and adds the player's body as a
 * billboard). GBA: height raycaster. PS1: GTE polygons (the PS1 may render
 * the player in third person as a real low-poly 3D model built from the
 * resident's look instead of the billboard - see ps1 docs). */
void plat_view3d(const DoomoView3D *v);

typedef struct DoomoOverworldView {
    const DoomoMap *map;
    fix16   cx, cy;             /* camera focus, world units               */
    int16_t vx, vy, vw, vh;     /* screen viewport                          */
    const DoomoBillboard *sprites; /* residents, pet, player, props (feet) */
    int     sprite_count;
    uint8_t daylight;           /* 0 = midnight .. 15 = noon               */
    uint8_t tint, tint_level;   /* e.g. vape haze over the invaded town     */
    uint16_t time;
} DoomoOverworldView;

/* Cutesy overworld. GBA: top-down 2D, each cell a 16x16 tile derived from
 * its floor texture, walls drawn as raised blocks with a front face, sprite
 * y-sorted. PS1: real 3D terrain from the same cells, tilted third-person
 * camera, sprites as upright billboards. */
void plat_overworld(const DoomoOverworldView *v);

/* ---- audio --------------------------------------------------------------- */
/* Four chiptune tone channels driven by the core sequencer (audio.c).
 * freq_hz 0 silences the channel. vol 0..15. Channel 3 is noise-capable. */
enum { WAVE_PULSE12, WAVE_PULSE25, WAVE_PULSE50, WAVE_TRIANGLE, WAVE_SAW,
       WAVE_NOISE, WAVE_COUNT };
void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave);
/* One-shot sample (sfx id from audio.h). The core supplies the PCM via
 * audio_sfx_pcm(); platforms may pre-convert it at boot. vol 0..15. */
void plat_sample(int sfx_id, uint8_t vol);

/* --- music: the engine's platform contract (docs/design/music/SPEC.md sec.9.4;
 * maintainer MU0/MU1, add-only). Weak defaults (core/src/plat_weak_music.c)
 * are the LEGACY BRIDGE: 4 voices (deck A lead, harm, bass, kit) onto the
 * plat_tone channels 0-3, with the 240 Hz driver run by the core inside
 * music_update() (8 mt per tick, 30 Hz control frames: today's sound).
 * GBA (MU2, M2) and PS1 (MU3, M3) override them and call mus_drv_tick()
 * from their IRQ (DoomoAudioCaps.flags CAP_DRV_IRQ). Records: mus_data.h.
 *   plat_audio_caps   tier, soft voices, flags, latency (main thread)
 *   plat_music_plan   -> mask of the voices the platform renders (main thread;
 *                     at cue start, stem commits and tier changes)
 *   plat_voice        one voice changed (from mus_drv, on change only)
 *   plat_audio_bus    a bus gain (Q8)
 *   plat_audio_fx     REVERB ECHO FREEZE FADE PROFILE CRACKLE PSG_DUCK
 *   plat_music_stall  STALL_* (music.h)
 *   plat_tape_*       TAPE re-render and scrub (M7)
 *   plat_sample_ex    a sample with pan, priority and pitch */
#include "mus_data.h"
#define MUS_MT_PER_TICK 8
void     plat_audio_caps(DoomoAudioCaps *out);
uint32_t plat_music_plan(const DoomoVoicePlan *p, int n);   /* -> rendered-voice mask (main thread) */
void     plat_voice(int voice, const DoomoVoice *v);        /* IRQ, from mus_drv, on change only   */
void     plat_audio_bus(int bus, uint8_t gain_q8);
void     plat_audio_fx(int fx, int a, int b);
void     plat_music_stall(int kind);
int      plat_tape_begin(uint16_t rate, uint16_t samples);
void     plat_tape_render(const DoomoVoiceChange *c, int n, uint16_t s_from, uint16_t s_to);
void     plat_tape_play(int16_t speed_q8, uint16_t pos, uint8_t gain);
void     plat_tape_end(void);
void     plat_sample_ex(int sfx, uint8_t vol, int8_t pan, uint8_t prio, int8_t semis);
void     mus_drv_tick(void);                                /* core; the platform calls it per mt  */
/* --- end music --- */

/* ---- save ----------------------------------------------------------------- */
/* Slots: 0 = story, 1 = free play. Data is opaque (save.c serialises),
 * at most DOOMO_SAVE_SLOT0_MAX / DOOMO_SAVE_SLOT1_MAX bytes (save.h).
 * GBA: 128 KB flash, slot 0 at 0x00000, slot 1 at 0x0A000.
 * PS1: one memory card file per slot (BASLUS-DOOMO0 / -DOOMO1).
 * Return 1 on success, 0 on failure. read sets *size. */
int plat_save_write(int slot, const void *data, uint32_t size);
int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size);
int plat_save_erase(int slot);
/* Temporary buffer for save_game()/load_game() (foundation, additive):
 * returns byte-addressable memory and stores its size in *cap. It is only
 * used for the duration of one save/load call, so a platform may hand out
 * memory that is otherwise busy (PS1: the idle display list; GBA: the
 * renderer caches). With at least DOOMO_SAVE_SCRATCH_MIN bytes (save.h)
 * the whole save image is built / read in it. A RAM-starved platform may
 * offer less (>= DOOMO_SAVE_STREAM_MIN) and implement the streaming calls
 * below instead: the core then streams the save through the buffer in
 * chunks. The core has a weak default (a static buffer in
 * core/src/save_scratch.c); platforms that override it should link with
 * --gc-sections (and -fdata-sections) so the default buffer is dropped. */
void *plat_save_scratch(uint32_t *cap);
/* Streaming saves (foundation, additive; only needed by platforms whose
 * plat_save_scratch() is smaller than a slot - the core falls back to
 * plat_save_write / plat_save_read otherwise). Weak defaults
 * (core/src/save_scratch.c) return 0 = not supported.
 *   plat_save_stream_begin(slot, size)  start replacing the slot with a
 *                         save of exactly `size` bytes (it fits the slot)
 *   plat_save_stream_write(data, n)     the next n bytes, in order
 *   plat_save_stream_end(commit)        commit = 1: all `size` bytes were
 *                         written, make them the slot's save (1 = written
 *                         and verified); commit = 0: abandon. Like
 *                         plat_save_write, a failed or abandoned write may
 *                         leave the slot empty, never half-valid.
 *   plat_save_read_at(slot, off, data, n, size)  n bytes from offset off
 *                         of the slot's save (off + n <= its size);
 *                         *size = the save's total size. 0 = no save. */
int plat_save_stream_begin(int slot, uint32_t size);
int plat_save_stream_write(const void *data, uint32_t n);
int plat_save_stream_end(int commit);
int plat_save_read_at(int slot, uint32_t offset, void *data, uint32_t n, uint32_t *size);

/* ---- S0: comic post-FX and squash (docs/design/story/SPEC.md sec.4, R12) -----
 * Maintainer: S1 (add-only). Weak software defaults: core/src/plat_weak_fx.c;
 * GBA (gba/src/postfx.c, obj.c) and PS1 (ps1/src/postfx.c, xform.c)
 * override them. camfx.h drives these; nothing else should call them.
 *
 * Scaled blit about an anchor: (ax, ay) is the image's bottom-centre (the
 * feet / contact point). sx/sy Q8.8 (256 = 1.0), negative sx = flip. flags
 * and color as plat_image. GBA: affine OBJ for hero actors (SPEC C7.4-6),
 * PS1: one POLY_FT4. The software default draws at the nearest integer
 * scale through plat_image (scale 1 when there is no hardware path). */
void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                      int flags, uint8_t color);
/* Full-screen post effects, applied to the frame being built (camfx_apply).
 *   offset: GBA REG_BG2X/Y at flip; PS1 E5 draw offset (a second E5 before
 *           the HUD). Default: no-op (software paths use camfx_offset()).
 *   scale:  GBA BG2PA/PD about (cx, cy); PS1 GTE H. Default: no-op.
 *   blend:  colour over the frame at level16 (0 none .. 16 solid); GBA BLDY
 *           or palette lerp, PS1 ABR TILE. Default: rect / checker / shade.
 *   iris:   everything outside the circle (heart: a heart) of radius r at
 *           (cx, cy) goes black, with a 1 px `rim` colour (0 = none). r < 0
 *           = off. Default: software row spans. */
void plat_post_offset(int dx, int dy);
void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy);
void plat_post_blend(uint8_t color, int level16);
void plat_post_iris(int cx, int cy, int r, int heart, uint8_t rim);
/* Palette lerp toward PAL_SEPIA/CRAYON/HELLIFY/LILAC (camfx.h) at level16;
 * GBA: palette DMA at vblank; PS1: a CLUT swap. Default: no-op. */
void plat_pal_lerp(int pal_id, int level16);

/* ---- S0: the dream house's room renderer (HF sec.9.1, SPEC R12, C7, C9) ------
 * Maintainer: H1 (add-only). GBA: gba/src/room.c (H2), PS1: ps1/src/room.c
 * (H3), native: core/src/room_ref.c (H4, the reference renderer). Until
 * those land plat_room is a weak stub (core/src/stubs.c). */
enum { RO_GHOST = 1, RO_INVALID = 2, RO_FULLBRIGHT = 4, RO_SEMI = 8, RO_SELECTED = 16 };
/* (H1 M3, add-only) a small on a surface sits half a tile further along +x
 * / +y (HomeObj's sub-position); RO_GHOST | RO_SEMI is a blueprint's grey
 * "Buy at HomeMart" ghost; DoomoRoomView.sel_mask is 128 bits, bit
 * y * w + x per tile of the rubber band (room space). */
enum { RO_HALF_X = 32, RO_HALF_Y = 64 };
enum { RA_PLAYER = 0x80 };            /* DoomoRoomActor.flags, with BB_* */
typedef struct DoomoRoomObj {         /* render-ready; the core fills it per frame */
    uint16_t furn;                    /* 0..FURN_MAX-1                              */
    uint8_t  x, y, z;                 /* tile, tile, height px                      */
    uint8_t  rot_anim;                /* bits 0-1 rot 0..3, bits 2-4 anim frame/state */
    uint8_t  paint_a, paint_b;        /* PAINT_* or 0xFF                             */
    int8_t   sq_x, sq_y;              /* squash -64..63 (/128): DROP, ripple, wobble */
    int8_t   jitter;                  /* possessed shake amplitude                   */
    uint8_t  flags;                   /* RO_*                                        */
} DoomoRoomObj;                       /* 12 B */
#define RO_ROT(o)  ((o)->rot_anim & 3)
#define RO_ANIM(o) (((o)->rot_anim >> 2) & 7)
#define RO_ROT_ANIM(rot, anim) ((uint8_t)(((rot) & 3) | (((anim) & 7) << 2)))
typedef struct DoomoRoomActor {
    const DoomoImage *img;            /* chibi (GBA, native) / figure (PS1)         */
    fix16   x, y;                     /* tile space 16.16                            */
    int16_t z;
    int16_t sx, sy;                   /* Q8.8 squash (PS1, native; GBA ignores)      */
    uint8_t flags;                    /* BB_FLIPX, BB_TRANSLUCENT, BB_SHADOW, RA_PLAYER */
} DoomoRoomActor;
enum { ROOMK_WALLS, ROOMK_GARDEN, ROOMK_OPEN };   /* DoomoRoomView.room_kind */
typedef struct DoomoRoomView {
    uint8_t  w, d, wallpaper, floor, wall_paint, floor_paint, ceiling, room_kind;
    const DoomoRoomObj *objs; int nobj;
    const DoomoRoomActor *actors; int nact;
    uint16_t gen;                     /* bump on any layout/paint change (GBA cache key) */
    int16_t  yaw;                     /* 0..4095; GBA snaps to 4 views, PS1 orbits       */
    uint8_t  zoom;                    /* 0 overview, 1 close                             */
    int16_t  punch_q8;                /* 256 = none                                      */
    int8_t   pan_x, pan_y;            /* tiles*16                                        */
    uint8_t  daylight, tint, tint_level, weather;
    int8_t   cursor_x, cursor_y;      /* -1 = hidden (GBA: OBJ)                          */
    uint8_t  sel_mask[16];            /* multi-select rubber band, 1 bit per tile row    */
    uint16_t time;
    int16_t  vx, vy, vw, vh;          /* viewport                                        */
    void    *plat_cache; uint32_t plat_cache_cap;  /* scene scratch tail the platform may
                                         use (PS1 vertex cache, C9/C17); GBA gets 0     */
} DoomoRoomView;
void plat_room(const DoomoRoomView *v);
/* An opaque HUD bar the room renderer may skip under (C7.3). Default no-op. */
void plat_room_ui_opaque(int x, int y, int w, int h);
/* --- end S0 --- */

/* ---- misc ----------------------------------------------------------------- */
uint32_t plat_entropy(void);           /* any varying value for seeding  */
uint32_t plat_ticks(void);             /* monotonically increasing frames */
void     plat_log(const char *msg);    /* debug output (no-op on hw ok)  */
/* Name of the platform, shown on the title screen ("GBA", "PS1", "NATIVE"). */
const char *plat_name(void);

#endif
