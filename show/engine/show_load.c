/* show_load.c - the episode from the disc, and the bound shot chunk
 * (owner: ENGINE lane 1; spec 4.1, 7.3, 8.1).
 *
 * At the first tick the player looks for \SHOW\EP<nnn>.SHW;1 on the disc
 * (mkpsxiso puts it there: show render builds the image in seconds). The
 * header, the shot index and the string table are read once and stay
 * resident; a shot's chunk is read into the chunk buffer when the shot is
 * bound (one synchronous CD read: the lockstep capture does not care how
 * long a shot's prep takes). No episode on the disc = the test chart.
 *
 * Chunk access for modules (show_api.h): show_chunk_find(), show_str(),
 * show_args(). Each record type appears at most once per chunk.
 * Deferred: prefetching the next chunk into a second buffer (only the disc
 * edition needs it; DEFERRED.md), PCdrv and Lua inject.
 */
#include <stdio.h>
#include <string.h>
#include <psxapi.h>
#include <psxcd.h>
#include "ps1.h"
#include "show_core.h"

DOOMO_STATIC_ASSERT(sizeof(ShwEpHeader) == 64, shw_ep_header_64);
DOOMO_STATIC_ASSERT(sizeof(ShwShotIndex) == 48, shw_shot_index_48);
DOOMO_STATIC_ASSERT(sizeof(ShwShotHdr) == 32, shw_shot_hdr_32);
DOOMO_STATIC_ASSERT(sizeof(ShwRec) == 8, shw_rec_8);
DOOMO_STATIC_ASSERT(sizeof(ShwPlane) == 24, shw_plane_24);
DOOMO_STATIC_ASSERT(sizeof(ShwImage) == 12, shw_image_12);
DOOMO_STATIC_ASSERT(sizeof(ShwSlug) == 16, shw_slug_16);
DOOMO_STATIC_ASSERT(sizeof(ShowMbox) == 176, show_mbox_176);

#define INDEX_MAX_BYTES (24 * 1024)     /* header + index + strings: ~480 shots */

static uint32_t s_head[INDEX_MAX_BYTES / 4];
static uint32_t s_chunk[SHW_CHUNK_MAX / 4];
static int      s_lba = -1;             /* the episode file's first sector  */
static int      s_bound = -1;           /* shot whose chunk is in s_chunk   */
static uint32_t s_chunk_bytes;

/* the record directory of the bound chunk */
#define REC_TYPES_MAX 64
static struct { uint16_t type, n; uint32_t bytes; const void *p; } s_rec[REC_TYPES_MAX];
static int s_nrec;
static const char *s_strs;  static uint32_t s_strs_bytes;
static const int32_t *s_args; static int s_nargs;
static const int32_t k_zero_args[16];

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

const ShwEpHeader *show_episode(void)
{
    return s_lba >= 0 ? (const ShwEpHeader *)s_head : NULL;
}

const ShwShotIndex *show_shot_index(int i)
{
    const ShwEpHeader *h = show_episode();
    if (!h || i < 0 || i >= h->nshots) return NULL;
    return (const ShwShotIndex *)((const uint8_t *)s_head + h->index_off) + i;
}

int show_load_episode(int episode)
{
    static int cd_up;
    CdlFILE f;
    char name[32];
    ShwEpHeader *h = (ShwEpHeader *)s_head;
    uint32_t need;
    int sectors;

    if (episode <= 0) episode = 101;
    if (!cd_up) { CdInit(); cd_up = 1; }
    snprintf(name, sizeof name, "\\SHOW\\EP%03d.SHW;1", episode);
    s_lba = -1;
    if (!CdSearchFile(&f, name)) {
        printf("show: no %s on the disc: test chart\n", name);
        return 0;
    }
    if (!read_sectors(CdPosToInt(&f.pos), 1, s_head) || h->magic != SHW_MAGIC) {
        printf("show: E %s: not an SHW1 file\n", name);
        return 0;
    }
    if (h->abi != SHW_ABI) {
        printf("show: E %s: format abi %u, this show.exe reads %u (rebuild one of them)\n",
               name, (unsigned)h->abi, (unsigned)SHW_ABI);
        return 0;
    }
    need = h->strtab_off + h->strtab_bytes;
    if (need < h->index_off + h->nshots * sizeof(ShwShotIndex)) need = h->index_off + h->nshots * sizeof(ShwShotIndex);
    if (need > INDEX_MAX_BYTES) {
        printf("show: E %s: index %u B > %u B\n", name, (unsigned)need, (unsigned)INDEX_MAX_BYTES);
        return 0;
    }
    sectors = (int)((need + SHW_SECTOR - 1) / SHW_SECTOR);
    if (sectors > 1 && !read_sectors(CdPosToInt(&f.pos), sectors, s_head)) {
        printf("show: E %s: read failed\n", name);
        return 0;
    }
    s_lba = CdPosToInt(&f.pos);
    s_bound = -1;
    printf("show: episode %u: %u shots, %u frames, seed %08x\n", (unsigned)h->episode,
           (unsigned)h->nshots, (unsigned)h->total_frames, (unsigned)h->seed);
    return 1;
}

static void index_records(void)
{
    const ShwShotHdr *hd = (const ShwShotHdr *)s_chunk;
    const uint8_t *p = (const uint8_t *)s_chunk + sizeof(ShwShotHdr);
    const uint8_t *end = (const uint8_t *)s_chunk + s_chunk_bytes;
    int i;
    s_nrec = 0;
    s_strs = NULL; s_strs_bytes = 0; s_args = NULL; s_nargs = 0;
    for (i = 0; i < hd->nrec && p + sizeof(ShwRec) <= end; i++) {
        const ShwRec *r = (const ShwRec *)p;
        const uint8_t *pay = p + sizeof(ShwRec);
        if (pay + r->bytes > end) {
            show_error("SHW-CHUNK", "record runs past the chunk");
            break;
        }
        if (s_nrec < REC_TYPES_MAX) {
            s_rec[s_nrec].type = r->type;
            s_rec[s_nrec].n = r->n;
            s_rec[s_nrec].bytes = r->bytes;
            s_rec[s_nrec].p = pay;
            s_nrec++;
        }
        if (r->type == SHWR_STRINGS) { s_strs = (const char *)pay; s_strs_bytes = r->bytes; }
        if (r->type == SHWR_ARGS) { s_args = (const int32_t *)pay; s_nargs = r->n; }
        p = pay + ((r->bytes + 3u) & ~3u);
    }
}

void show_chunk_bind_mem(const uint8_t *chunk, uint32_t bytes)
{
    if (bytes > SHW_CHUNK_MAX) bytes = SHW_CHUNK_MAX;
    if (chunk != (const uint8_t *)s_chunk) memcpy(s_chunk, chunk, bytes);
    s_chunk_bytes = bytes;
    index_records();
}

int show_load_chunk(int shot)
{
    const ShwShotIndex *ix = show_shot_index(shot);
    const ShwShotHdr *hd = (const ShwShotHdr *)s_chunk;
    uint32_t t0 = timer_us();
    int sectors;
    if (!ix) return 0;
    if (s_bound == shot) { index_records(); return 1; }
    if (ix->chunk_bytes > SHW_CHUNK_MAX || ix->chunk_bytes < sizeof(ShwShotHdr)) {
        show_error("SHW-CHUNK", "chunk size out of range");
        return 0;
    }
    sectors = (int)((ix->chunk_bytes + SHW_SECTOR - 1) / SHW_SECTOR);
    if (!read_sectors(s_lba + (int)ix->chunk_sector, sectors, s_chunk)) {
        show_error("SHW-READ", "chunk read failed");
        return 0;
    }
    if (hd->magic != SHW_SHOT_MAGIC || hd->shot != shot) {
        show_error("SHW-CHUNK", "bad chunk magic or shot number");
        return 0;
    }
    s_bound = shot;
    s_chunk_bytes = ix->chunk_bytes;
    index_records();
    g_show_mbox.load_us = timer_us() - t0;
    return 1;
}

const ShwShotHdr *show_chunk_hdr(void);
const ShwShotHdr *show_chunk_hdr(void) { return (const ShwShotHdr *)s_chunk; }

const void *show_chunk_find(uint16_t type, int *n, uint32_t *bytes)
{
    int i;
    for (i = 0; i < s_nrec; i++)
        if (s_rec[i].type == type) {
            if (n) *n = s_rec[i].n;
            if (bytes) *bytes = s_rec[i].bytes;
            return s_rec[i].p;
        }
    if (n) *n = 0;
    if (bytes) *bytes = 0;
    return NULL;
}

const char *show_str(uint32_t off)
{
    if (!s_strs || off >= s_strs_bytes) return "";
    return s_strs + off;
}

const int32_t *show_args(int b)
{
    if (!s_args || b < 0 || b >= s_nargs) return k_zero_args;
    return s_args + b;
}

int show_nargs(void) { return s_nargs; }

void show_chunk_clear(void);
void show_chunk_clear(void)
{
    s_nrec = 0;
    s_strs = NULL; s_strs_bytes = 0; s_args = NULL; s_nargs = 0;
}
