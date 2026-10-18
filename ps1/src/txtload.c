/* txtload.c - the story text packs on the PS1 (owner: S2;
 * docs/design/story/SPEC.md R2, C3, C5).
 *
 * The packs are not linked into the EXE (CMakeLists.txt drops
 * core/gen/text_*.c): textpack.py writes them to TEXT.BIN on the disc, and
 * this file reads the ones the story needs into a RESIDENT window, so a
 * line can be decoded from any scene (the overworld, a cutscene, combat's
 * HUD objective, the house) without the story overlay group:
 *
 *   slot E  12 KB  the episode / chapter-common / Feast / legacy pack
 *   slot S   8 KB  the side pack (until H3's overlay-tail window lands,
 *                  SPEC C5: then it moves into the last 14 KB of the
 *                  overlay slot and this window shrinks to slot E)
 *   dir    0.5 KB  sector + size of every pack (from txt_dict.c's sizes)
 *
 * txt_plat_load() is called by txt.c when a pack is not live: one CD read
 * of 4-12 KB (~0.1 s) at a story boundary (entering the overworld, a
 * chapter change). TEXT.BIN: sector 0 = {u32 'TEXT', u32 n, n x {u32
 * sector, u32 bytes}}, then the packs at sector boundaries.
 */
#include <string.h>
#include <psxapi.h>
#include <psxcd.h>
#include "ps1.h"

#ifdef DOOMO_HAVE_CORE
#include "doomo/txt.h"

#define SECTOR    2048
#define SLOT_E    (12 * 1024)
#define SLOT_S    (8 * 1024)
#define TXT_MAGIC 0x54584554u           /* "TEXT" */

static uint32_t win_e[SLOT_E / 4], win_s[SLOT_S / 4];
static int16_t  live[2] = { -1, -1 };    /* pack in slot E / S */
static int      s_lba = -1;              /* TEXT.BIN, -1 = not found yet, -2 = missing */
static uint16_t s_sector[TXT_PACK_MAX];  /* first sector of each pack */

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

static int find_text_bin(void)
{
    CdlFILE f;
    uint32_t *h = win_s;                 /* sector 0 lands in slot S for a moment */
    unsigned i, sec = 1;
    if (s_lba >= 0) return 1;
    if (s_lba == -2) return 0;
#ifndef DOOMO_OVERLAYS
    CdInit();                            /* the game disc's overlay_init() did it */
#endif
    if (!CdSearchFile(&f, "\\TEXT.BIN;1") || !read_sectors(CdPosToInt(&f.pos), 1, h) ||
        h[0] != TXT_MAGIC || h[1] != doomo_txt_npacks) {
        dbg("txt: TEXT.BIN missing or stale\n");
        s_lba = -2;
        return 0;
    }
    for (i = 0; i < doomo_txt_npacks && i < TXT_PACK_MAX; i++) {
        s_sector[i] = (uint16_t)h[2 + i * 2];
        if (h[3 + i * 2] != doomo_txt_pack_bytes[i]) { s_lba = -2; return 0; }
        sec += (doomo_txt_pack_bytes[i] + SECTOR - 1) / SECTOR;
    }
    (void)sec;
    live[1] = -1;                        /* slot S held the directory */
    s_lba = CdPosToInt(&f.pos);
    return 1;
}

const uint8_t *txt_plat_pack(int pack)
{
    if (pack < 0) return NULL;
    if (live[0] == pack) return (const uint8_t *)win_e;
    if (live[1] == pack) return (const uint8_t *)win_s;
    return NULL;
}

int txt_plat_load(int pack)
{
    int s, cap, n;
    uint32_t t0;
    if (pack < 0 || pack >= (int)doomo_txt_npacks) return 0;
    if (txt_plat_pack(pack)) return 1;
    if (!find_text_bin()) return 0;
    s = doomo_txt_pack_kinds[pack] == TXT_KIND_SIDE;
    cap = s ? SLOT_S : SLOT_E;
    n = (doomo_txt_pack_bytes[pack] + SECTOR - 1) / SECTOR;
    if (n * SECTOR > cap) {
        dbg("txt: pack %s does not fit its slot\n", doomo_txt_pack_names[pack]);
        return 0;
    }
    t0 = timer_us();
    live[s] = -1;
    if (!read_sectors(s_lba + s_sector[pack], n, s ? win_s : win_e)) {
        dbg("txt: read error, pack %s\n", doomo_txt_pack_names[pack]);
        return 0;
    }
    live[s] = (int16_t)pack;
    dbg("txt: pack %s (%u B) in %u ms\n", doomo_txt_pack_names[pack],
        (unsigned)doomo_txt_pack_bytes[pack], (unsigned)((timer_us() - t0) / 1000));
    return 1;
}

#endif /* DOOMO_HAVE_CORE */
