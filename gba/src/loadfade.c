/* loadfade.c - a short fade over a loading freeze (SP5, G-LD; plat_ext.h plat_loading; owner: gba;
 * docs/GBA.md "LD: loading fades").
 *
 * A scene change (scene.c) or a story map load calls plat_loading(): the next frame may take long (a scene's
 * enter builds its caches, a map is unpacked, the island's sim starts; 0.2-2 s on the GBA). Until then the last
 * frame stays on screen, which reads as a hang. So from the call until the next flip, the VBlank IRQ watches the
 * clock: once LD_LATE vblanks (150 ms, more than the slowest normal frame: combat, 100-160 ms) have passed without a flip, it darkens the screen with the hardware fade
 * (BLDCNT brightness decrease on every layer, BLDY) by LD_STEP a vblank, to black in about 130 ms more. The flip
 * that ends the wait shows the new picture at that darkness and lifts it by LD_IN a flip (about 130 ms). A load
 * shorter than LD_LATE shows nothing at all. Registers only: no pixel of any page changes, logic and timing are
 * the same (the core does not know), and a frame with its own blend (postfx.c) keeps the darker of the two.
 *
 * The VBlank IRQ (irq.iwram.c) calls gba_vbl_hook while it is set: this file's THUMB code in ROM, set only from
 * plat_loading() to the end of the fade-out wait, so the IRQ costs one load and test otherwise. RAM: 12 bytes. */
#include "gba_plat.h"
#include "doomo/plat_ext.h"

#define LD_LATE 9           /* vblanks (150 ms) without a flip before the fade starts: past any normal slow frame */
#define LD_STEP 2           /* BLDY per vblank while it fades out (0..16) */
#define LD_IN   4           /* BLDY per flip while the new picture fades in */
#define LD_BLDCNT 0x00FFu   /* brightness decrease, first target BG0-3 + OBJ + backdrop */

void (*volatile gba_vbl_hook)(void);
static volatile uint32_t ld_v0;      /* gba_vblanks at plat_loading() */
static volatile uint8_t ld_level;    /* darkness on screen now, 0..16 */

static void ld_vblank(void)
{
    if (gba_vblanks - ld_v0 < LD_LATE) return;
    if (ld_level < 16) ld_level = (uint8_t)DOOMO_MIN(16, ld_level + LD_STEP);
    REG_BLDCNT = LD_BLDCNT;
    REG_BLDY = ld_level;
}

void plat_loading(void)
{
    if (gba_mailbox.draw_all) return;            /* the harness hashes every frame (vhash-seq): never touch them */
    ld_v0 = gba_vblanks;
    gba_vbl_hook = ld_vblank;
}

/* postfx_vblank, in the vblank of a flip, with the frame's own BLDCNT / BLDY about to be written: the wait is
 * over; the new picture comes in from the darkness reached, keeping a darker blend of its own */
void ld_flip(int *bldcnt, int *bldy)
{
    gba_vbl_hook = 0;
    if (!ld_level) return;
    if ((*bldcnt & 0x00C0) != 0x00C0 || *bldy < ld_level) {   /* none, white, or lighter than ours: ours */
        *bldcnt = (int)LD_BLDCNT;
        *bldy = ld_level;
    }
    ld_level = ld_level > LD_IN ? (uint8_t)(ld_level - LD_IN) : 0;
}
