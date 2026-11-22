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
#define TALK_BASE_SECTORS ((SPU_SPRITES_BASE - SPU_TALK_BASE) / TALK_SECTOR)
#define TALK_TAIL_SECTORS 24u
#define TALK_PREWARM_SECTORS 72u
#define TALK_STREAM_SECTORS 24u
#define TALK_DISC_LIMIT (4u * 1024u * 1024u)
DOOMO_STATIC_ASSERT(TALK_PREWARM_SECTORS < TALK_BASE_SECTORS, talk_prewarm_leaves_cache_space);

static uint32_t s_page[2][TALK_SECTOR / 4u];  /* DMA-aligned */
static uint32_t s_length;
static int s_lba = -1;                    /* -1 unopened, -2 absent/error */
static int s_sector[2] = { -1, -1 };
static unsigned s_recent = 1;
static uint16_t s_spu_tag[TALK_BASE_SECTORS + TALK_TAIL_SECTORS];   /* file sector + 1, zero = empty */
static uint32_t s_spu_tail;
static unsigned s_spu_slots = TALK_BASE_SECTORS;
static unsigned s_spu_next, s_spu_pinned;

static uint32_t spu_page_address(unsigned slot)
{
    if (slot < TALK_BASE_SECTORS) return SPU_TALK_BASE + slot * TALK_SECTOR;
    return s_spu_tail + (slot - TALK_BASE_SECTORS) * TALK_SECTOR;
}

static int read_sectors(int lba, unsigned count, uint32_t *page)
{
    CdlLOC loc;
    int retries;
    for (retries = 0; retries < 3; retries++) {
        CdIntToPos(lba, &loc);
        if (!CdControl(CdlSetloc, &loc, 0)) continue;
        if (!CdRead((int)count, page, CdlModeSpeed)) continue;
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
        for (i = 0; i < s_spu_slots; i++)
            if (s_spu_tag[i] == tag) break;
        if (i < s_spu_slots) {
            SpuSetTransferStartAddr(spu_page_address(i));
            bytes = SpuRead(page, TALK_SECTOR);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        }
        if (bytes != TALK_SECTOR) {
            if (!read_sectors(s_lba + (int)sector, 1, page))
                return NULL;
            cache = i < s_spu_slots ? i : s_spu_next;
            s_spu_tag[cache] = 0;
            SpuSetTransferStartAddr(spu_page_address(cache));
            bytes = SpuWrite(page, TALK_SECTOR);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            if (bytes == TALK_SECTOR) {
                s_spu_tag[cache] = tag;
                if (i == s_spu_slots && ++s_spu_next == s_spu_slots)
                    s_spu_next = s_spu_pinned;
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

/* At boot the display lists are idle. Stage the prefix assembled by
 * talk_controller.py there. When the parts leave enough space for demand
 * reads, retain the prewarmed grammar and safety pages in SPU RAM. */
void talkdisc_prewarm(void)
{
    uint32_t cap = 0, start = timer_us();
    unsigned sectors, total, chunk;
    uint32_t *buf;
    if (!plat_talk_load("en")) {
        s_lba = -1; /* Let the usual lazy open retry after a boot failure. */
        return;
    }
    buf = (uint32_t *)plat_save_scratch(&cap);
    chunk = cap / TALK_SECTOR;
    if (!buf || !chunk) return;
#ifdef DOOMO_PARTS_SPU
    {
        uint32_t end = partstore_spu_end();
        if (end >= SPU_PARTS_BASE && end <= SPU_REVERB_BASE && end % TALK_SECTOR == 0) {
            unsigned tail = (SPU_REVERB_BASE - end) / TALK_SECTOR;
            if (tail > TALK_TAIL_SECTORS) tail = TALK_TAIL_SECTORS;
            /* Keep normal rolling eviction when the parts leave too little room. */
            if (TALK_BASE_SECTORS + tail >= TALK_PREWARM_SECTORS + TALK_STREAM_SECTORS) {
                s_spu_tail = end;
                s_spu_slots = TALK_BASE_SECTORS + tail;
            }
        }
    }
#endif
    total = (s_length + TALK_SECTOR - 1u) / TALK_SECTOR;
    if (total > TALK_PREWARM_SECTORS) total = TALK_PREWARM_SECTORS;
    for (sectors = 0; sectors < total; ) {
        unsigned n = total - sectors < chunk ? total - sectors : chunk;
        unsigned bytes = n * TALK_SECTOR, i;
        size_t transferred;
        if (!read_sectors(s_lba + (int)sectors, n, buf)) break;
        SpuSetTransferStartAddr(SPU_TALK_BASE + sectors * TALK_SECTOR);
        transferred = SpuWrite(buf, bytes);
        SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        if (transferred != bytes) break;
        for (i = 0; i < n; i++) s_spu_tag[sectors + i] = (uint16_t)(sectors + i + 1u);
        sectors += n;
    }
    s_spu_next = sectors;
    s_spu_pinned = s_spu_slots > TALK_BASE_SECTORS ? sectors : 0;
    dbg("talk-prewarm: %u sectors in %u ms (%u streaming slots)\n", sectors,
        (unsigned)((timer_us() - start) / 1000u), s_spu_slots - s_spu_pinned);
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
