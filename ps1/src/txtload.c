/* txtload.c - the story text packs on the PS1 (owner: S2;
 * docs/design/story/SPEC.md R2, C3, C5).
 *
 * The packs are not linked into the EXE (CMakeLists.txt drops
 * core/gen/text_*.c): textpack.py writes them to TEXT.BIN on the disc, and
 * this file reads the ones the story needs into a RESIDENT window, so a
 * line can be decoded from any scene (the overworld, a cutscene, combat's
 * HUD objective, the house) without the story overlay group:
 *
 *   slot E  12 KB  the episode / Feast / legacy pack (and a chapter-common
 *                  pack too big for slot C)
 *   slot C   2 KB  the chapter-common pack (objectives, idle lines, the
 *                  wrongness captions: read while an episode runs, so they
 *                  must not evict it; S2 content: ch1 1.6 KB, ch2 1.2 KB)
 *   slot S   8 KB  the side pack (until H3's overlay-tail window lands,
 *                  SPEC C5: then S and C move into the last 14 KB of the
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
#include "doomo/lang.h"

#define SECTOR    2048
#define SLOT_E    (12 * 1024)
#define SLOT_S    (8 * 1024)
#define SLOT_C    (2 * 1024)
#define TXT_MAGIC 0x54584554u           /* "TEXT" */

static uint32_t win_e[SLOT_E / 4], win_s[SLOT_S / 4], win_c[SLOT_C / 4];
static int16_t  live[3] = { -1, -1, -1 };    /* pack in slot E / S / C */
static int      s_lba = -1;              /* TEXT.BIN, -1 = not found yet, -2 = missing */
static uint16_t s_sector[TXT_PACK_MAX];  /* first sector of each pack */
/* the Spanish half of TEXT.BIN (textpack.py): same packs, other strings;
 * bytes 0 = no Spanish version of that pack */
static uint16_t s_sector_es[TXT_PACK_MAX], s_bytes_es[TXT_PACK_MAX];
static uint8_t  live_es[3];              /* slot E / S / C holds the Spanish version */

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
    {   /* the Spanish directory follows the English one (absent in old TEXT.BIN: bytes 0) */
        unsigned base = 2 + doomo_txt_npacks * 2;
        for (i = 0; i < doomo_txt_npacks && i < TXT_PACK_MAX && base + i * 2 + 1 < SECTOR / 4; i++) {
            s_sector_es[i] = (uint16_t)h[base + i * 2];
            s_bytes_es[i] = (uint16_t)h[base + 1 + i * 2];
        }
    }
    (void)sec;
    live[1] = -1;                        /* slot S held the directory */
    s_lba = CdPosToInt(&f.pos);
    return 1;
}

/* Spanish wanted and this pack has a Spanish version */
static int want_es(int pack)
{
    return pack < TXT_PACK_MAX && s_bytes_es[pack] && lang_get() == LANG_ES;
}

const uint8_t *txt_plat_pack(int pack)
{
    int es;
    if (pack < 0) return NULL;
    es = want_es(pack);
    if (live[0] == pack && live_es[0] == es) return (const uint8_t *)win_e;
    if (live[1] == pack && live_es[1] == es) return (const uint8_t *)win_s;
    if (live[2] == pack && live_es[2] == es) return (const uint8_t *)win_c;
    return NULL;
}

int txt_plat_load(int pack)
{
    int s, cap, n, es;
    uint32_t t0;
    if (pack < 0 || pack >= (int)doomo_txt_npacks) return 0;
    if (txt_plat_pack(pack)) return 1;
    if (!find_text_bin()) return 0;
    es = want_es(pack);
    n = ((es ? s_bytes_es[pack] : doomo_txt_pack_bytes[pack]) + SECTOR - 1) / SECTOR;
    s = doomo_txt_pack_kinds[pack] == TXT_KIND_SIDE ? 1 :
        (doomo_txt_pack_kinds[pack] == TXT_KIND_COMMON && n * SECTOR <= SLOT_C) ? 2 : 0;
    if (s == 1 && n * SECTOR > SLOT_S) s = 0;   /* a big Spanish side pack takes the episode slot */
    cap = s == 1 ? SLOT_S : s == 2 ? SLOT_C : SLOT_E;
    if (n * SECTOR > cap) {
        dbg("txt: pack #%d does not fit its slot\n", pack);
        return 0;
    }
    t0 = timer_us();
    live[s] = -1;
    if (!read_sectors(s_lba + (es ? s_sector_es[pack] : s_sector[pack]), n, s == 1 ? win_s : s == 2 ? win_c : win_e)) {
        dbg("txt: read error, pack #%d\n", pack);
        return 0;
    }
    live[s] = (int16_t)pack;
    live_es[s] = (uint8_t)es;
    {   /* --- S5: the name rides in the last sector's padding (textpack.py
         * --bin), so doomo_txt_pack_names (532 B) stays out of resident RAM;
         * a pack that fills its sectors exactly logs "?" --- */
        const char *w = (const char *)(s == 1 ? win_s : s == 2 ? win_c : win_e);
        unsigned b = es ? 0 : doomo_txt_pack_bytes[pack];
        dbg("txt: pack %s (%u B) in %u ms\n", (b & (SECTOR - 1)) && w[b] ? w + b : "?", b,
            (unsigned)((timer_us() - t0) / 1000));
    }   /* --- end S5 --- */
    return 1;
}

#endif /* DOOMO_HAVE_CORE */
