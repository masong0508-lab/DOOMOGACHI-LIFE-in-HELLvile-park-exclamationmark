/* ps1.h - internal interfaces of the PlayStation platform layer (owner: ps1).
 *
 * VRAM layout (1024 x 512 halfwords):
 *
 *   x 0..319    y   0..239  framebuffer 0
 *   x 0..319    y 240..479  framebuffer 1
 *   x 0..255    y 480       master CLUT (doomo_palette, STP set, 0 = clear)
 *   x 0..255    y 481..495  silhouette CLUT cache (15 slots, LRU)
 *   x 0..255    y 496..511  font CLUTs: 256 colours x 16 entries (4bpp)
 *   x 256..319  y 480..511  misc CLUTs (shadow blob, ...)
 *   x 320..959  y   0..255  world textures: 5 8bpp tpages x 16 slots of 64x64
 *   x 960..1023 y   0..191  sky: 256x96 8bpp split in two 128x96 halves
 *   x 960..991  y 192..255  font: 128 glyphs, 4bpp, 16x8 grid of 8x8 cells
 *   x 992..999  y 192..223  shadow blob (32x32 4bpp)
 *   x 320..1023 y 256..511  image cache: 5 256x256 8bpp pages + one
 *                           128x256 strip, shelf packed, LRU per page
 */
#ifndef PS1_H
#define PS1_H

#include <stdint.h>
#include <stddef.h>
#include <psxgpu.h>
#include <psxgte.h>
#include "doomo/doomo.h"

#define SCR_W 320
#define SCR_H 240

/* ---- VRAM layout ---------------------------------------------------------- */
#define VR_CLUT_MASTER_X   0
#define VR_CLUT_MASTER_Y   480
#define VR_CLUT_SIL_Y      481
#define VR_CLUT_SIL_SLOTS  15
#define VR_CLUT_FONT_Y     496
#define VR_CLUT_MISC_X     256
#define VR_CLUT_MISC_Y     480
#define VR_WTEX_X          320
#define VR_WTEX_PAGES      5
#define VR_WTEX_SLOTS      (VR_WTEX_PAGES * 16)
#define VR_SKY_X           960
#define VR_SKY_Y           0
#define VR_FONT_X          960
#define VR_FONT_Y          192
#define VR_BLOB_X          992
#define VR_BLOB_Y          192
#define VR_IMG_Y           256

/* ---- frame / display list (gfx.c) ----------------------------------------- */
#define OT_LEN        1536   /* depth slots per 3D view                     */
#define OT_VIEWS      2      /* 3D views per frame (view3d / overworld)     */
/* Display-list bytes per frame. Both frames' lists together are the save
 * scratch (gfx.c plat_save_scratch): 2 x PRIM_BYTES must hold
 * DOOMO_SAVE_SCRATCH_MIN (save.h: a whole slot plus the codec's work area,
 * 102.25 KB) so saves never stream on the PS1. A frame has needed at most
 * ~36 KB; a packed sky's first upload needs 2 x 24 KB + 4 KB at the start
 * of a 3D view (vram.c). */
#define PRIM_BYTES    (56 * 1024)

typedef struct GfxPerf {
    uint32_t cpu_us;              /* begin_frame .. end_frame (CPU)       */
    uint32_t sync_us;             /* waiting for the GPU (GPU-bound)       */
    uint32_t frame_vblanks;       /* vblanks between presented frames     */
    uint32_t fps_x10;             /* presented frames per second x10      */
    uint32_t prim_bytes, prims;   /* display list size of the last frame  */
} GfxPerf;
extern GfxPerf g_perf;

extern int      g_cur;            /* buffer being built                  */
extern uint32_t g_frame;          /* frames presented                    */
extern volatile uint32_t g_loop_frames; /* game loop iterations (harness) */
void model_set_look(const DoomoLook *l); /* engine test showcase */
void hcmd_poll(void);                   /* harness mailbox, between frames */
extern uint16_t g_last_draw_y;          /* VRAM y of the newest frame */
void gfx_busy_banner(const char *msg);  /* "Saving..." while the BIOS blocks */
void gfx_busy_end(void);                /* ... and remove it again */
/* full-screen "Saving... / Loading..." screen with a progress bar (done of
 * total blocks); call again after every block, total 0 = empty bar */
void gfx_loading_screen(const char *title, const char *detail, int done, int total);
extern uint32_t g_rgb[256];       /* palette -> 0x00BBGGRR for prims      */
extern uint16_t g_pal15[256];     /* palette -> VRAM 15-bit (STP set)     */

void  gfx_init(void);
void  gfx_begin(void);
void  gfx_end(void);
void *gfx_alloc(size_t bytes);    /* prim memory in the current buffer, or NULL */
extern uint8_t *g_prim_ptr, *g_prim_end;
size_t gfx_free_bytes(void);
/* Append a primitive (tag word with len set) to the painter's chain. */
void  gfx_chain(void *prim);
/* Begin/finish a 3D view: returns a cleared OT (index 0 = nearest), links
 * it into the chain with the viewport clip applied, restores full clip. */
uint32_t *gfx_ot_begin(int vx, int vy, int vw, int vh);
void  gfx_ot_end(void);
/* Queue a VRAM upload (executed in order at the start of this frame's list,
 * data copied into the prim buffer). w is in halfwords. Returns 0 if the
 * prim buffer is full (caller may retry next frame). */
int   gfx_upload(int x, int y, int w, int h, const uint16_t *data);
/* Synchronous upload (boot time / when no frame is in flight). */
int   gfx_upload_strided(int x, int y, int w, int h, const uint8_t *src, int stride);
void  gfx_upload_now(int x, int y, int w, int h, const void *data);
/* Set the GPU texpage / semi-transparency mode for following 2D prims. */
void  gfx_tpage(uint16_t tpage);
void  gfx_fb_xy(int *x, int *y);  /* VRAM origin of the buffer being built */
uint32_t gfx_prims_used(void);

/* ---- texture cache (vram.c) ------------------------------------------------ */
typedef struct TexRef {
    uint16_t tpage;               /* 8bpp tpage, semi-trans mode 0       */
    uint16_t clut;
    uint8_t  u, v;                /* top-left texel                      */
    uint8_t  ok;
} TexRef;

void vram_init(void);
void vram_frame(void);
/* World texture (64x64) for the current frame, by texture id. */
const TexRef *vram_wtex(int tex_id, uint16_t time);
/* Image cache: tile (tx,ty) of an image, tiles are IMG_TILE texels. */
#define IMG_TILE 128
int  vram_image(const DoomoImage *img, int tx, int ty, TexRef *out);
uint16_t vram_sil_clut(uint8_t color);   /* all opaque pixels -> color */
uint16_t vram_font_clut(uint8_t color);
uint16_t vram_font_tpage(void);
/* Sky: uploads if the sky id / frame changed. Returns 0 if no sky. */
int  vram_sky(int sky_id, uint16_t time, TexRef *left, TexRef *right);
void vram_blob(TexRef *out);             /* soft round shadow (4bpp)   */
uint32_t vram_uploads(void);             /* bytes uploaded this frame  */

/* ---- 2D layer (gfx2d.c) ---------------------------------------------------- */
void gfx2d_init(void);
/* plat_rect() batches runs of rects (merging adjacent spans, turning 50%
 * checker rows into one blended tile); anything else that appends to the
 * display list must flush the pending run first (gfx.c does). */
void gfx2d_flush(void);
void gfx2d_reset(void);
extern uint32_t *g_chain_tail;
/* append prim p (len words after the tag) to the draw chain, no flush */
static inline void gfx_link_raw(uint32_t *p, uint32_t len)
{
    *g_chain_tail = (*g_chain_tail & 0xff000000u) | ((uint32_t)p & 0x00ffffffu);
    p[0] = len << 24;
    g_chain_tail = p;
}

/* ---- 3D (r3d.c, view3d.c, overworld.c, model.c) ---------------------------- */
typedef struct R3Stats {
    uint16_t cells, faces, polys, subdiv, clipped, sprites, culled, backing;
    uint32_t cpu_us, t_setup, t_vis, t_cells;
} R3Stats;
extern R3Stats g_r3stats;

/* ---- core access (coreref.c): optional core pieces during bring-up ---------- */
int  core_has_game(void);
void core_doomo_init(void);
void core_doomo_frame(uint32_t buttons);
const uint8_t *core_texture_pixels(int tex_id, uint16_t time);
const uint8_t *core_sky_pixels(int sky_id, uint16_t time);
const DoomoImage *core_sprite_image(int spr, int frame);
const uint8_t (*core_font(void))[5];
int  core_has_maps(void);
int  core_map_load(int map_id);          /* 1 if loaded into doomo_map */
DoomoMap *core_map(void);
const DoomoResident *core_premade(int id);    /* roster citizen or NULL */
const DoomoResident *core_resident(int id);
uint8_t core_look_paint(uint8_t paint, int shade);
uint8_t core_look_skin(const DoomoLook *l, int shade);
int  core_has_combat(void);
void core_combat_start(int map_id);
void core_combat_build_view(DoomoView3D *v);
DoomoPlayerState *core_player(void);
void core_palette_init(void);
const int8_t *core_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate);
const uint16_t *core_palette(void);
int  core_opt(int opt, int def);                 /* opt_get() or default */
int  core_floor_z(const DoomoMap *m, int cx, int cy);   /* incl. door/lift anim */
int  core_ceil_z(const DoomoMap *m, int cx, int cy);

/* ---- packed art (packart.c, tools/packart.py) ------------------------------ */
/* The core's texture and sky frames are LZ-compressed in RAM; the pointers
 * texture_pixels() / sky_pixels() return are virtual and never read
 * directly. packart_resolve(p): readable pixels for p (a texture pointer
 * is decoded into a one-frame buffer, valid until the next call; any other
 * pointer is returned as is). packart_sky_decode(p, dst): a whole sky
 * frame into dst (SKY_W * SKY_H bytes), 0 if p is not a packed sky. */
int  packart_is_tex(const uint8_t *p);
int  packart_is_sky(const uint8_t *p);
const uint8_t *packart_resolve(const uint8_t *p);
int  packart_sky_decode(const uint8_t *p, uint8_t *dst);
int  packart_sky_edges(const uint8_t *p, uint8_t *top, uint8_t *bottom);

/* ---- scene overlays (overlay.c, tools/ovl_plan.py) --------------------------
 * The game disc reads scene code from OVL.BIN into one RAM slot when a
 * scene starts (core scene.h plat_scene_code). overlay_init(): CD init +
 * the OVL.BIN header (boot). The engine test links everything resident. */
void     overlay_init(void);
uint32_t overlay_loads(void);

/* ---- engine test / fallback assets (etest*.c) ------------------------------- */
const uint8_t *etest_texture(int tex_id, uint16_t time);
const uint8_t *etest_sky(uint16_t time);
const DoomoImage *etest_sprite(int which, int frame);
void etest_build_map(DoomoMap *m);
void etest_run(void);                    /* never returns */
extern const uint8_t etest_font[128][5];
const uint16_t *etest_palette(void);

/* ---- pad / audio / memcard / timing ------------------------------------------ */
void     pad_init(void);
uint32_t pad_read(void);                 /* BTN_* bitmask */
int      pad_analog(int *lx, int *ly, int *rx, int *ry);
void     audio_hw_init(void);
void     audio_hw_frame(void);
void     audio_hw_hush(int on);      /* silence the SPU while the BIOS blocks the loop */
int      audio_selftest(void);       /* SPU checks, PASS/FAIL on the TTY */
void     memcard_early(void);
void     memcard_init(void);
void     timer_init(void);
uint32_t timer_us(void);                 /* microseconds since boot (wraps) */

/* Debug output to the TTY (pcsx-redux -stdout), printf-like subset. */
void dbg(const char *fmt, ...);

#endif
