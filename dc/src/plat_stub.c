/* plat_stub.c - placeholders for the platform calls the later Dreamcast
 * packages implement (owner: dreamcast).
 *
 * DC2 was the skeleton: the game booted, ran its 30 Hz loop and changed
 * scenes, but drew nothing except the clear colour. Each group below is
 * replaced, and this file shrinks, as the packages land:
 *   2D layer, post effects, held frame  -> DC3: done (dc2d.h, gfx2d.c, ...)
 *   saves on the VMU                -> DC4 (vmusave.c); until then a RAM disk
 *   3D view, overworld, house       -> DC6 / DC7 / DC8
 * Audio needs nothing here: the core's weak legacy bridge
 * (core/src/plat_weak_music.c) drives plat_tone, which is silent until
 * DC5 (audio.c).
 */
#include <string.h>
#include "dc_plat.h"
#include "doomo/save.h"

/* ---- display (the logical screen, dc2d.h) ------------------------------------------------------------ */
int plat_screen_w(void) { return DC_SCREEN_W; }
int plat_screen_h(void) { return DC_SCREEN_H; }

/* every 2D call, the post effects and plat_hold_frame: gfx2d.c (DC3) */

/* ---- 3D and the world renderers (DC6 / DC7 / DC8) ----------------------------- */
void plat_view3d(const DoomoView3D *v) { (void)v; }
void plat_overworld(const DoomoOverworldView *v) { (void)v; }

/* ---- audio (DC5) ------------------------------------------------------------------- */
void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{ (void)channel; (void)freq_hz; (void)vol; (void)wave; }
void plat_sample(int sfx_id, uint8_t vol) { (void)sfx_id; (void)vol; }

/* ---- saves (DC4: the VMU) ------------------------------------------------------------
 * Until the VMU code lands, a RAM disk: one buffer per slot, lost at power
 * off. It behaves like a device that never fails, so the core's save, load
 * and verify paths (and the harness's save / load / expect-save) run. The
 * whole-image scratch buffer is static: the Dreamcast has 16 MB of RAM. */
static uint8_t  ram_slot0[DOOMO_SAVE_SLOT0_MAX];
static uint8_t  ram_slot1[DOOMO_SAVE_SLOT1_MAX];
static uint32_t ram_size[2];            /* 0 = empty */
static uint8_t  save_scratch[DOOMO_SAVE_SCRATCH_MIN] __attribute__((aligned(32)));

static uint8_t *ram_slot(int slot, uint32_t *cap)
{
    if (slot == 0) { *cap = sizeof(ram_slot0); return ram_slot0; }
    if (slot == 1) { *cap = sizeof(ram_slot1); return ram_slot1; }
    *cap = 0;
    return NULL;
}

int plat_save_write(int slot, const void *data, uint32_t size)
{
    uint32_t cap;
    uint8_t *p = ram_slot(slot, &cap);
    if (!p || !data || size == 0 || size > cap) return 0;
    memcpy(p, data, size);
    ram_size[slot] = size;
    return 1;
}

int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    uint32_t scap;
    uint8_t *p = ram_slot(slot, &scap);
    if (!p || !ram_size[slot] || !data || ram_size[slot] > cap) return 0;
    memcpy(data, p, ram_size[slot]);
    if (size) *size = ram_size[slot];
    return 1;
}

int plat_save_erase(int slot)
{
    uint32_t cap;
    if (!ram_slot(slot, &cap)) return 0;
    ram_size[slot] = 0;
    return 1;
}

void *plat_save_scratch(uint32_t *cap)
{
    *cap = sizeof(save_scratch);
    return save_scratch;
}
