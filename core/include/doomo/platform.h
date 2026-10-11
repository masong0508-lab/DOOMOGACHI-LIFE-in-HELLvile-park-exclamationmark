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
/* Draw an 8bpp image; index 0 transparent. scale 1..4 (integer
 * nearest-neighbour). flags: IMG_FLIPX, IMG_SHADOW (1px dark outline
 * drop shadow), IMG_SILHOUETTE (all opaque pixels drawn as `color`). */
enum { IMG_FLIPX = 1, IMG_SHADOW = 2, IMG_SILHOUETTE = 4 };
void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color);

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

/* ---- save ----------------------------------------------------------------- */
/* Slots: 0 = story, 1 = free play. Data is opaque (save.c serialises),
 * at most DOOMO_SAVE_SLOT0_MAX / DOOMO_SAVE_SLOT1_MAX bytes (save.h).
 * GBA: 128 KB flash, slot 0 at 0x00000, slot 1 at 0x0A000.
 * PS1: one memory card file per slot (BASLUS-DOOMO0 / -DOOMO1).
 * Return 1 on success, 0 on failure. read sets *size. */
int plat_save_write(int slot, const void *data, uint32_t size);
int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size);
int plat_save_erase(int slot);
/* Big temporary buffer for save_game()/load_game() (foundation, additive):
 * returns memory of at least DOOMO_SAVE_SCRATCH_MIN bytes (save.h) and
 * stores its size in *cap. It is only used for the duration of one
 * save/load call, so a RAM-starved platform may hand out memory that is
 * otherwise busy (e.g. GBA: OBJ VRAM + the back page while a "Saving..."
 * screen is shown). The core has a weak default (a static buffer in
 * core/src/save_scratch.c); platforms that override it should link with
 * --gc-sections (and -fdata-sections) so the default buffer is dropped. */
void *plat_save_scratch(uint32_t *cap);

/* ---- misc ----------------------------------------------------------------- */
uint32_t plat_entropy(void);           /* any varying value for seeding  */
uint32_t plat_ticks(void);             /* monotonically increasing frames */
void     plat_log(const char *msg);    /* debug output (no-op on hw ok)  */
/* Name of the platform, shown on the title screen ("GBA", "PS1", "NATIVE"). */
const char *plat_name(void);

#endif
