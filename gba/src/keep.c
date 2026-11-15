/* keep.c - rows a bitmap scene keeps at half rate (G2-CO; plat_ext.h plat_keep_rows; owner: gba; docs/GBA.md
 * "G2-CO: the concert at 30").
 *
 * The concert's LED wall, truss lamps and the beams down to the wall's foot move at 15 Hz on the GBA (the owner's
 * 2026-11-13 trade): the core draws the next picture of those rows over two ticks into a buffer and puts it into
 * two frames in a row (the two Mode 4 pages). The buffer is the OBJ VRAM (16 KB at 0x06014000), which a bitmap
 * scene without sprites leaves unused: rows * 240 bytes, 68 rows at most. While the core draws into it
 * (PLAT_KEEP_DRAW .. PLAT_KEEP_END) gba_back points at it, so every 2D loop writes there as into a page; the core
 * keeps those calls inside the buffer's rows (one more row would land in the OBJ VRAM's mirror, page 1's rows).
 * Owner OBJV_KEEP (postfx.c gba_objvram_claim): like a held 3D view it keeps obj.c's OBJs off and makes the
 * overworld's billboards start again; any other claim means the buffer is lost (PUT and HELD say 0).
 * PUT copies the rows by 32-bit DMA, 16 rows (3,840 B) a transfer, so the music IRQ waits at most ~0.25 ms.
 * RAM: 2 B of .bss.
 */
#include "gba_plat.h"
#include "postfx.h"
#include "doomo/plat_ext.h"

#define KEEP_VRAM  ((uint16_t *)0x06014000)
#define KEEP_BYTES 16384u

enum { KP_DRAWING = 1, KP_PAGE1 = 2, KP_HAVE = 4 };
static uint8_t keep_st;                        /* KP_*: drawing into the buffer (and the page gba_back was), holds rows */
static uint8_t keep_n;                         /* the buffer holds rows [0, keep_n) drawn by the scene */

int plat_keep_rows(int op, int rows)
{
    int y;
    if (gba_nodraw == 1 || rows <= 0 || (uint32_t)rows * SCREEN_W > KEEP_BYTES) return 0;
    switch (op) {
    case PLAT_KEEP_DRAW:
        /* only inside a plain Mode 4 frame (after its first call) drawn into one of the two pages */
        if ((keep_st & KP_DRAWING) || gba_nodraw != 0 ||
            (gba_back != (uint16_t *)VRAM_PAGE0 && gba_back != (uint16_t *)VRAM_PAGE1))
            return 0;
        gba_objvram_claim(OBJV_KEEP);
        keep_st = (uint8_t)(KP_DRAWING | KP_HAVE | (gba_back == (uint16_t *)VRAM_PAGE1 ? KP_PAGE1 : 0));
        gba_back = KEEP_VRAM;
        keep_n = (uint8_t)rows;
        return 1;
    case PLAT_KEEP_END:
        if (!(keep_st & KP_DRAWING)) return 0;
        gba_back = (uint16_t *)((keep_st & KP_PAGE1) ? VRAM_PAGE1 : VRAM_PAGE0);
        keep_st = KP_HAVE;
        return 1;
    case PLAT_KEEP_HELD:
        return keep_st == KP_HAVE && rows <= keep_n && gba_objvram_owner() == OBJV_KEEP;
    case PLAT_KEEP_PUT:
        if (keep_st != KP_HAVE || rows > keep_n || gba_objvram_owner() != OBJV_KEEP) return 0;
        gba_objvram_claim(OBJV_KEEP);                         /* this frame too: obj.c's OBJs stay off */
        for (y = 0; y < rows; y += 16) {
            int n = rows - y < 16 ? rows - y : 16;
            dma3_copy32(gba_back + y * (SCREEN_W / 2), KEEP_VRAM + y * (SCREEN_W / 2), (uint32_t)(n * SCREEN_W / 4));
        }
        return 1;
    default:
        if (!(keep_st & KP_DRAWING)) keep_st = 0;
        return 0;
    }
}
