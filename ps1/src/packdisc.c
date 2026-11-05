/* packdisc.c - the mature content pack, read from the disc (owner: ps1; E9).
 *
 * An optional PACK.DMP (a DMPK v1 pack built for port ps1 by tools/mpack/mpack.py, E7) sits on
 * the disc. Nothing of it is resident: core/src/pack.c opens it through plat_pack_read(), which
 * serves any byte range from a one-sector window (the open pass reads the header, the entry
 * directory and the CRC in small sequential steps, so one sector is enough), and the mslot scene
 * (core/src/mslot.c) reads the slot's script whole into plat_mslot_script_buf().
 *
 * Built only when the disc has a pack (cmake -DDOOMO_PACK_FILE=/path/pack.dmpk, which also sets
 * DOOMO_PACK_ON_DISC): the stock disc links none of this and pays none of its RAM
 * (2 KB window + 8 KB script buffer, both BSS). The stills (PK_KIND_CG) are not shown on this
 * port yet: the display lists share RAM with the save scratch, so there is no 75 KB to hold one.
 * plat_mslot_buf() stays the weak default (NULL) and the scene plays the script without art.
 */
#include <string.h>
#include <psxapi.h>
#include <psxcd.h>
#include "ps1.h"

#if defined(DOOMO_HAVE_CORE) && defined(DOOMO_PACK_ON_DISC)
#include "doomo/pack.h"
#include "doomo/plat_ext.h"

#define SECTOR 2048

static uint32_t s_win[SECTOR / 4];        /* one sector of the pack */
static int      s_lba = -1;               /* PACK.DMP: -1 not looked up yet, -2 missing */
static int      s_have = -1;              /* sector index (from the file start) in the window, -1 none */
static uint32_t s_size;                   /* file size in bytes */
static uint32_t s_script[PACK_SCRIPT_CAP_PS1 / 4];

static int read_sector(int lba, void *dst)
{
    CdlLOC loc;
    int tries;
    for (tries = 0; tries < 4; tries++) {
        CdIntToPos(lba, &loc);
        if (!CdControl(CdlSetloc, &loc, 0))
            continue;
        if (!CdRead(1, (uint32_t *)dst, CdlModeSpeed))
            continue;
        if (CdReadSync(0, 0) >= 0)
            return 1;
    }
    return 0;
}

uint32_t plat_pack_read(uint32_t off, void *dst, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    uint32_t done = 0;
    if (s_lba == -1) {
        CdlFILE f;
        if (CdSearchFile(&f, "\\PACK.DMP;1")) {
            s_lba = CdPosToInt(&f.pos);
            s_size = f.size;
        } else {
            s_lba = -2;
        }
    }
    if (s_lba < 0 || off > s_size || len > s_size - off)
        return 0;
    while (done < len) {
        uint32_t pos = off + done, sec = pos / SECTOR, in = pos % SECTOR, n = SECTOR - in;
        if (n > len - done)
            n = len - done;
        if (s_have != (int)sec) {
            s_have = -1;
            if (!read_sector(s_lba + (int)sec, s_win))
                return 0;
            s_have = (int)sec;
        }
        memcpy(d + done, (const uint8_t *)s_win + in, n);
        done += n;
    }
    return done;
}

uint8_t *plat_mslot_script_buf(uint32_t *cap)
{
    if (cap)
        *cap = sizeof s_script;
    return (uint8_t *)s_script;
}
#endif
