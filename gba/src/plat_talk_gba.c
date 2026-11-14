/* plat_talk_gba.c - Talk's database on the GBA (owner: gba; docs/TALK.md, plat_ext.h).
 *
 * The compiled bank (TALK_en.BIN) is linked into the ROM (talk_blob.s), and the ROM is
 * memory-mapped, so plat_talk_read is a bounded copy out of it: Talk costs no RAM for data.
 * Only English is built in (the bank is ~0.9 MB of the 32 MB ROM). */
#include "doomo/doomo.h"
#include "doomo/plat_ext.h"

#ifdef DOOMO_HAS_TALK

extern const uint8_t gba_talk_bin[];
extern const uint8_t gba_talk_bin_end[];

uint32_t plat_talk_load(const char *code)
{
    if (!code || code[0] != 'e' || code[1] != 'n' || code[2]) return 0;
    return (uint32_t)(gba_talk_bin_end - gba_talk_bin);
}

uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    uint32_t size = (uint32_t)(gba_talk_bin_end - gba_talk_bin);
    uint8_t *d = (uint8_t *)dst;
    uint32_t i;
    if (!dst || off > size || len > size - off) return 0;
    for (i = 0; i < len; i++) d[i] = gba_talk_bin[off + i];
    return len;
}

#endif /* DOOMO_HAS_TALK */
