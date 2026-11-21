/* talkdisc.c — PS1 bounded CD reader for the shared Talk database.
 * Stores exactly one 2048-byte sector of TALK_EN.BIN in main RAM.
 * Supports arbitrary small unaligned reads, including sector crossings.
 * The English language bank remains on the CD, never in a static array,
 * an EXE data section or heap. All I/O is on explicit Talk actions.
 *
 * The core already has a small 256 B logical read-through cache; this
 * sector window serves those cache misses with bounded memory.
 * CD seek/latency and CD ownership must be tested on PCSX-Redux.
 */
#include <string.h>
#include <psxapi.h>
#include <psxcd.h>
#include "ps1.h"

#ifdef DOOMO_HAS_TALK
#include "doomo/plat_ext.h"

#define TALK_SECTOR 2048u

static uint32_t s_page[TALK_SECTOR / 4u];  /* 2 KB, DMA-aligned */
static uint32_t s_length;
static int s_lba = -1;                    /* -1 unopened, -2 absent/error */
static int s_sector = -1;

static int read_sector(int lba)
{
    CdlLOC loc;
    int retries;
    for (retries = 0; retries < 3; retries++) {
        CdIntToPos(lba, &loc);
        if (!CdControl(CdlSetloc, &loc, 0)) continue;
        if (!CdRead(1, s_page, CdlModeSpeed)) continue;
        if (CdReadSync(0, 0) >= 0) return 1;
    }
    return 0;
}

uint32_t plat_talk_load(const char *code)
{
    CdlFILE f;
    if (!code || strcmp(code, "en")) return 0; /* other packs not shipped */
    if (s_lba >= 0) return s_length;
    if (s_lba == -2) return 0;
    /* The regular PS1 game disc calls overlay_init() at boot. Do not
     * reset the CD controller while a scene/movie/overlay is active. */
    if (!CdSearchFile(&f, "\\TALK_EN.BIN;1") || f.size == 0) {
        s_lba = -2;
        return 0;
    }
    s_lba = CdPosToInt(&f.pos);
    s_length = (uint32_t)f.size;
    s_sector = -1;
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
        if (n > len - done) n = len - done;
        if (s_sector != (int)sector) {
            s_sector = -1; /* a failed transfer cannot expose stale data */
            if (!read_sector(s_lba + (int)sector)) {
                s_lba = -2; /* fail closed: no infinite retry freeze */
                return 0;
            }
            s_sector = (int)sector;
        }
        memcpy(out + done, (const uint8_t *)s_page + index, n);
        done += n;
    }
    return done;
}

/* The PS1 has an actual Talk scene and disc bank with this build flag.
 * Advertise it for input routing, subject to talk_ready() validating the
 * bank itself before opening any conversation. */
uint32_t plat_features(void) { return PLATF_TALK; }
#endif
