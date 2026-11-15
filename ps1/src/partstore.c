/* partstore.c - the part scripts and part icons in SPU RAM (owner: ps1; CR10, docs/PS1.md "CR10").
 *
 * The part VM (core/src/portrait_script.c) runs the scripts of every scripted part: hats, hair, horns, eyes...
 * Their raw bytes are not linked into the EXE (DOOMO_PARTS_SPU, core/include/doomo/parts.h): ps1/tools/partsbin.py
 * writes them to PARTS.BIN, this file reads that at boot (after the sound effects, like SFX.BIN) and
 * uploads them to SPU RAM at SPU_PARTS_BASE, followed by the creator's packed part icons (the grid unpacks one at
 * a time through plat_part_icon_read()). plat_part_read() copies one script entry back with an SPU DMA read when a
 * render first needs it (core/src/part_lru.c part_store_ops keeps it in a 2 KB per-render arena).
 *
 * The SPU RAM above SPU_PARTS_BASE belongs to this store; the sound effects stop below it
 * (spu.c SPU_SFX_END). Nothing else writes SPU RAM after boot (every effect is converted before the first game
 * frame, spu.c "staging"), so a read here never races an upload: this is the "audio driver lock".
 *
 * A read costs one DMA of at most 320 bytes (the entry window rounded to the 64-byte blocks the SPU DMA moves).
 * PARTS.BIN is 145,408 B on the disc, 143,360 B in SPU RAM (docs/PS1.md "CR10"); SPU RAM from SPU_PARTS_BASE to the reverb area is 184,320 B.
 */
#include <string.h>
#include <psxapi.h>
#include <psxcd.h>
#include <psxspu.h>
#include "ps1.h"

#if defined(DOOMO_HAVE_CORE) && defined(DOOMO_PARTS_SPU)
#include "doomo/parts.h"
#include "doomo/part_icons.h"

#define SECTOR   2048
#define MAXSETS  24
#define CHUNK    16                     /* sectors read and uploaded at a time (32 KB) */
#define SPU_TOP  0x7e000u               /* the reverb work area starts here */
#define RB_BYTES 320                    /* PART_ENTRY_WIN + 7 rounded up to 64-byte DMA blocks */
typedef char rb_fits[(PART_ENTRY_WIN + 7 + 63) / 64 * 64 <= RB_BYTES && PART_ICON_PACK_MAX <= PART_ENTRY_WIN ? 1 : -1];

static uint32_t s_base[MAXSETS + 1];    /* SPU address of each set's scripts, [count] the packed icons; 0 = none */
static uint8_t  s_rb[RB_BYTES] __attribute__((aligned(4)));
static int      s_ok;
static uint32_t s_reads;

/* n bytes at SPU address `at`, n at most PART_ENTRY_WIN */
static int spu_copy(uint32_t at, uint8_t *dst, unsigned n)
{
    uint32_t a = at & ~7u;              /* SPU transfers start on 8-byte units */
    unsigned skip = (unsigned)(at - a), bytes = (skip + n + 63) & ~63u;
    if (!s_ok || n > PART_ENTRY_WIN || a + bytes > 0x80000u)   /* never past the end of SPU RAM */
        return 0;
    SpuSetTransferStartAddr(a);
    SpuRead((uint32_t *)s_rb, bytes);
    SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
    memcpy(dst, s_rb + skip, n);
    s_reads++;
    return 1;
}

int plat_part_read(unsigned set, unsigned ofs, uint8_t *dst, unsigned n)
{
    return set < MAXSETS && s_base[set] && spu_copy(s_base[set] + ofs, dst, n);
}

int plat_part_icon_read(uint32_t ofs, uint8_t *dst, unsigned n)
{
    unsigned k = doomo_part_script_count;
    return k < MAXSETS && s_base[k] && spu_copy(s_base[k] + ofs, dst, n);
}

static int read_sectors(int lba, int n, void *dst)
{
    CdlLOC loc;
    int tries;
    for (tries = 0; tries < 4; tries++) {
        CdIntToPos(lba, &loc);
        if (CdControl(CdlSetloc, &loc, 0) && CdRead(n, (uint32_t *)dst, CdlModeSpeed) && CdReadSync(0, 0) >= 0)
            return 1;
    }
    return 0;
}

/* After overlay_init() (the CD is up) and audio_hw_sfx_preload(): PARTS.BIN -> SPU RAM. It is read in 32 KB
 * chunks into the display lists' scratch (idle until the game loop starts) and each chunk sent on. */
void partstore_boot(void)
{
    uint32_t cap = 0;
    uint8_t *buf = (uint8_t *)plat_save_scratch(&cap);
    CdlFILE f;
    int lba = 0, nrec = 0, i, ok = 0;
    uint32_t total = 0, done;
    s_ok = 0;
    if (!buf || cap < (uint32_t)(CHUNK + 1) * SECTOR || !CdSearchFile(&f, "\\PARTS.BIN;1"))
        goto done;
    lba = CdPosToInt(&f.pos);
    if (!read_sectors(lba, 1, buf) || memcmp(buf, "PRTS", 4) != 0)   /* the header sector */
        goto done;
    nrec = (buf[4] | (buf[5] << 8)) + 1;                           /* the sets and the icons */
    total = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8) | ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);
    if (nrec != (int)doomo_part_script_count + 1 || nrec > MAXSETS + 1 || total == 0 || (total % SECTOR) != 0 ||
        SPU_PARTS_BASE + total > SPU_TOP)
        goto done;
    for (i = 0; i < nrec; i++)
        s_base[i] = SPU_PARTS_BASE + ((uint32_t)buf[16 + 8 * i] | ((uint32_t)buf[17 + 8 * i] << 8) |
                    ((uint32_t)buf[18 + 8 * i] << 16) | ((uint32_t)buf[19 + 8 * i] << 24));
    for (done = 0, ok = 1; done < total && ok; done += CHUNK * SECTOR) {
        uint32_t n = total - done < CHUNK * SECTOR ? total - done : CHUNK * SECTOR;
        ok = read_sectors(lba + 1 + (int)(done / SECTOR), (int)(n / SECTOR), buf);
        if (ok) {
            SpuSetTransferStartAddr(SPU_PARTS_BASE + done);
            SpuWrite((const uint32_t *)buf, n);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        }
        if (ok && done == 0) {   /* read the first bytes back at an odd offset: the log says whether the SPU read works */
            uint8_t chk[64];
            s_ok = 1;
            ok = spu_copy(SPU_PARTS_BASE + 5, chk, sizeof(chk)) && memcmp(chk, buf + 5, sizeof(chk)) == 0;
        }
    }
    s_ok = ok;
done:
    if (!s_ok)
        memset(s_base, 0, sizeof(s_base));
    dbg("parts: %s, %d sets + icons, %u bytes at spu %05x\n", s_ok ? "loaded" : "MISSING", nrec - 1, (unsigned)total,
        (unsigned)SPU_PARTS_BASE);
}

uint32_t partstore_reads(void) { return s_reads; }
#endif
