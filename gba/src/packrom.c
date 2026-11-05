/* packrom.c - the mature content pack baked into the ROM (owner: gba; E8).
 *
 * `make -C gba MATURE_PACK=/path/pack.dmpk` builds build/doomogachi_mature.gba: the normal game
 * plus a DMPK v1 pack (built for port gba by tools/mpack/mpack.py, E7) linked into the ROM
 * image. The pack is addressable, so core/src/pack.c opens it through plat_pack_blob() and
 * serves it in place: stills (stored RAW, 240x160) are drawn straight from the cartridge and
 * the slot scripts are registered with the text engine without a copy. Nothing is copied to RAM
 * and plat_mslot_buf() / plat_mslot_script_buf() stay the weak defaults (no buffers needed).
 *
 * Built only for the mature variant (DOOMO_PACK_ROM): the stock ROM links none of this and is
 * byte for byte what it was. The ROM is a 32 MB cartridge image either way (CART_BYTES), so the
 * default 4 MB cap (PACK_CAP_GBA_DEFAULT, mpack.py validate --gba-default-cap) keeps the variant
 * well inside it; pack.c itself accepts up to PACK_CAP_GBA_HARD (8 MB).
 */
#include "gba_plat.h"

#ifdef DOOMO_PACK_ROM
#include "doomo/pack.h"
#include "doomo/plat_ext.h"

#ifndef DOOMO_PACK_INCBIN
#error "DOOMO_PACK_ROM needs -DDOOMO_PACK_INCBIN=\"path/to/pack.dmpk\" (gba/Makefile MATURE_PACK=)"
#endif

/* the file's bytes, 4-aligned (the pack's own payloads are 4-aligned from its start) */
__asm__(".section .rodata.packrom,\"a\"\n\t"
        ".balign 4\n\t"
        ".global gba_pack_blob\n"
        "gba_pack_blob:\n\t"
        ".incbin \"" DOOMO_PACK_INCBIN "\"\n\t"
        ".global gba_pack_blob_end\n"
        "gba_pack_blob_end:\n\t"
        ".balign 4\n\t"
        ".previous\n");
extern const uint8_t gba_pack_blob[], gba_pack_blob_end[];

const uint8_t *plat_pack_blob(uint32_t *size)
{
    if (size)
        *size = (uint32_t)(gba_pack_blob_end - gba_pack_blob);
    return gba_pack_blob;
}
#endif
