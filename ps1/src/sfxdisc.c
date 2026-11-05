/* sfxdisc.c - the sound effect PCM, read from the disc (owner: ps1).
 *
 * core/gen/sfx_data.c's 47 KB PCM array is not linked into the game EXE
 * (DOOMO_SFX_ON_DISC, tools/sfxgen.py): tools/sfxbin.py writes it to
 * SFX.BIN, and the core's audio_sfx_pcm() asks plat_sfx_pcm() for the bytes
 * of one effect. Every effect is converted to SPU RAM once at boot
 * (audio_hw_sfx_preload(), spu.c), so the disc is not touched while playing;
 * the only later reader is the Library jukebox's waveform, one small read
 * when the selection changes.
 *
 * The window holds the sectors that cover one effect (the longest is 2,836
 * bytes: at most 3 sectors). The pointer is valid until the next call.
 */
#include <psxapi.h>
#include <psxcd.h>
#include "ps1.h"

#if defined(DOOMO_HAVE_CORE) && defined(DOOMO_SFX_ON_DISC)
#include "doomo/audio.h"

#define SECTOR    2048
#define WIN_SECT  3

static uint32_t s_win[WIN_SECT * SECTOR / 4];
static int      s_lba = -1;              /* SFX.BIN: -1 not looked up yet, -2 missing */
static int      s_have = -1;             /* first sector in the window, -1 none       */
static int      s_have_n;

static int read_sectors(int lba, int n, void *dst)
{
    CdlLOC loc;
    int tries;
    for (tries = 0; tries < 4; tries++) {
        CdIntToPos(lba, &loc);
        if (!CdControl(CdlSetloc, &loc, 0))
            continue;
        if (!CdRead(n, (uint32_t *)dst, CdlModeSpeed))
            continue;
        if (CdReadSync(0, 0) >= 0)
            return 1;
    }
    return 0;
}

const int8_t *plat_sfx_pcm(uint32_t offset, uint32_t len)
{
    uint32_t first = offset / SECTOR, last = (offset + len - 1) / SECTOR;
    int n = (int)(last - first + 1);
    if (!len || n > WIN_SECT)
        return NULL;
    if (s_lba == -1) {
        CdlFILE f;
        s_lba = CdSearchFile(&f, "\\SFX.BIN;1") ? CdPosToInt(&f.pos) : -2;
    }
    if (s_lba < 0)
        return NULL;
    if (!(s_have >= 0 && (uint32_t)s_have <= first && (uint32_t)(s_have + s_have_n) > last)) {
        s_have = -1;
        if (!read_sectors(s_lba + (int)first, n, s_win))
            return NULL;
        s_have = (int)first;
        s_have_n = n;
    }
    return (const int8_t *)s_win + (offset - (uint32_t)s_have * SECTOR);
}
#endif
