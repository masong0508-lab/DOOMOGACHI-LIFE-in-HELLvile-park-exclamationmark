/* Read TALK_EN.BIN through two main-RAM sectors and a cache in SPU RAM.
 * A CD miss is read once while its sector remains in either cache. The
 * language file stays on disc and all transfers finish before returning. */
#include <string.h>
#include <psxapi.h>
#include <psxcd.h>
#include <psxspu.h>
#include "ps1.h"

#ifdef DOOMO_HAS_TALK
#include "doomo/plat_ext.h"

#define TALK_SECTOR 2048u
#define TALK_CACHE_SECTORS ((SPU_SPRITES_BASE - SPU_TALK_BASE) / TALK_SECTOR)
#define TALK_DISC_LIMIT (4u * 1024u * 1024u)

static uint32_t s_page[2][TALK_SECTOR / 4u];  /* DMA-aligned */
static uint32_t s_length;
static int s_lba = -1;                    /* -1 unopened, -2 absent/error */
static int s_sector[2] = { -1, -1 };
static unsigned s_recent = 1;
static uint16_t s_spu_tag[TALK_CACHE_SECTORS];   /* file sector + 1, zero = empty */
static unsigned s_spu_next;

static int read_sector(int lba, uint32_t *page)
{
    CdlLOC loc;
    int retries;
    for (retries = 0; retries < 3; retries++) {
        CdIntToPos(lba, &loc);
        if (!CdControl(CdlSetloc, &loc, 0)) continue;
        if (!CdRead(1, page, CdlModeSpeed)) continue;
        if (CdReadSync(0, 0) >= 0) return 1;
    }
    return 0;
}

static const uint8_t *talk_page(unsigned sector)
{
    unsigned i, cache, slot = s_recent;
    uint16_t tag = (uint16_t)(sector + 1u);
    uint32_t *page;
    if (s_sector[slot] == (int)sector)
        return (const uint8_t *)s_page[slot];
    slot ^= 1u;
    page = s_page[slot];
    if (s_sector[slot] != (int)sector) {
        size_t bytes = 0;
        s_sector[slot] = -1;
        for (i = 0; i < TALK_CACHE_SECTORS; i++)
            if (s_spu_tag[i] == tag) break;
        if (i < TALK_CACHE_SECTORS) {
            SpuSetTransferStartAddr(SPU_TALK_BASE + i * TALK_SECTOR);
            bytes = SpuRead(page, TALK_SECTOR);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        }
        if (bytes != TALK_SECTOR) {
            if (!read_sector(s_lba + (int)sector, page))
                return NULL;
            cache = i < TALK_CACHE_SECTORS ? i : s_spu_next;
            s_spu_tag[cache] = 0;
            SpuSetTransferStartAddr(SPU_TALK_BASE + cache * TALK_SECTOR);
            bytes = SpuWrite(page, TALK_SECTOR);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            if (bytes == TALK_SECTOR) {
                s_spu_tag[cache] = tag;
                if (i == TALK_CACHE_SECTORS)
                    s_spu_next = (s_spu_next + 1u) % TALK_CACHE_SECTORS;
            }
        }
        s_sector[slot] = (int)sector;
    }
    s_recent = slot;
    return (const uint8_t *)page;
}

uint32_t plat_talk_load(const char *code)
{
    CdlFILE f;
    if (!code || strcmp(code, "en")) return 0; /* other packs not shipped */
    if (s_lba >= 0) return s_length;
    if (s_lba == -2) return 0;
    /* The regular PS1 game disc calls overlay_init() at boot. Do not
     * reset the CD controller while a scene/movie/overlay is active. */
    if (!CdSearchFile(&f, "\\TALK_EN.BIN;1") || f.size < 20 || (uint32_t)f.size > TALK_DISC_LIMIT) {
        s_lba = -2;
        return 0;
    }
    s_lba = CdPosToInt(&f.pos);
    s_length = (uint32_t)f.size;
    dbg("talkps1: TALK_EN.BIN available (%u bytes)\n", (unsigned)s_length);
    return s_length;
}

uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    uint8_t *out = (uint8_t *)dst;
    uint32_t done = 0;
    if (!out || s_lba < 0 || off > s_length || len > s_length - off)
        return 0;
    while (done < len) {
        uint32_t at = off + done, sector = at / TALK_SECTOR;
        uint32_t index = at % TALK_SECTOR, n = TALK_SECTOR - index;
        const uint8_t *page = talk_page(sector);
        if (!page) return 0;
        if (n > len - done) n = len - done;
        memcpy(out + done, page + index, n);
        done += n;
    }
    return done;
}

#endif
