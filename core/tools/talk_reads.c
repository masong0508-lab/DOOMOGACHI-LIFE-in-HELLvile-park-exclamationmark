/* talk_reads.c - TKP0: counts plat_talk_read() calls and bytes by bank while the census runs
 * (owner: TK; docs/design/talk/TKP_PLAN.md). Not part of any test. Link it into the census with
 * -Wl,--wrap=plat_talk_read:   sh core/tools/talk_reads.sh
 * On the PS1 every plat_talk_read is a SpuRead (a DMA from SPU RAM), so reads and bytes per line
 * are what the plan's per-turn budget is about. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "doomo/talk.h"

uint32_t __real_plat_talk_read(uint32_t off, void *dst, uint32_t len);
int __real_talk_gen_exchange(TalkConv *c, TalkUtter *u, TalkExchange *ex, TalkTrace *tr);
static int inside;                  /* only the reads made by talk_gen_exchange count */
static uint64_t n_ex, n_lines;

static uint64_t rd[TB_COUNT + 1], by[TB_COUNT + 1];
static uint32_t boff[TB_COUNT + 1], bend[TB_COUNT + 1];
static int have;

/* the plan's read-through block cache, simulated: BLOCKS blocks of BSZ bytes, least recently used out;
 * a miss is a SpuRead of one block. TALK_CACHE=blocks:bytes (default 64:512), TALK_CACHE_RESET=1 empties it
 * at the start of every exchange (the per-turn scratch case). */
#define MAXB 4096
static uint32_t ctag[MAXB], cage[MAXB];
static int cn = 64, cbsz = 512, creset;
static uint64_t cclock, cmiss, cmissB[TB_COUNT + 1], chit;

static void cache_touch(uint32_t off, uint32_t len, int bank)
{
    uint32_t b0 = off / (uint32_t)cbsz, b1 = (off + len - 1) / (uint32_t)cbsz, b;
    for (b = b0; b <= b1; b++) {
        int i, hit = -1, old = 0;
        cclock++;
        for (i = 0; i < cn; i++) {
            if (cage[i] && ctag[i] == b) { hit = i; break; }
            if (cage[i] < cage[old]) old = i;
        }
        if (hit >= 0) { cage[hit] = (uint32_t)cclock; chit++; }
        else { ctag[old] = b; cage[old] = (uint32_t)cclock; cmiss++; cmissB[bank] += (uint64_t)cbsz; }
    }
}

static void report(void)
{
    static const char *const nm[] = {"-", "CONCEPT", "ASSOC", "ATTR", "FORMS", "CATS", "CATNAMES", "EDGES",
                                     "GRAMMAR", "STRINGS", "BEATS", "VOICE", "PARSE", "SAFETY", "?"};
    uint64_t tr = 0, tb = 0;
    int i;
    for (i = 0; i <= TB_COUNT; i++) { tr += rd[i]; tb += by[i]; }
    for (i = 0; i <= TB_COUNT; i++)
        if (rd[i]) fprintf(stderr, "talk_reads: %-9s %12llu reads %14llu B\n", nm[i],
                           (unsigned long long)rd[i], (unsigned long long)by[i]);
    fprintf(stderr, "talk_reads: %llu exchanges, %llu lines\n", (unsigned long long)n_ex, (unsigned long long)n_lines);
    {
        uint64_t mb = 0;
        for (i = 0; i <= TB_COUNT; i++) mb += cmissB[i];
        fprintf(stderr, "talk_reads: cache %d x %d B%s: %llu hits, %llu misses, %llu B read (%.1f KB a line)\n", cn, cbsz,
                creset ? " reset every exchange" : "", (unsigned long long)chit, (unsigned long long)cmiss,
                (unsigned long long)mb, n_lines ? mb / 1024.0 / (double)n_lines : 0.0);
        for (i = 0; i <= TB_COUNT; i++)
            if (cmissB[i]) fprintf(stderr, "talk_reads:   cache misses %-9s %10llu B\n", nm[i], (unsigned long long)cmissB[i]);
    }
    fprintf(stderr, "talk_reads: TOTAL     %12llu reads %14llu B\n", (unsigned long long)tr, (unsigned long long)tb);
}

uint32_t __wrap_plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    int i, b = TB_COUNT;
    if (!inside) return __real_plat_talk_read(off, dst, len);
    if (!have) {
        for (i = 1; i < TB_COUNT; i++) {
            uint32_t o, p, r;
            if (talk_db_bank(i, &o, &p, &r)) { boff[i] = o; bend[i] = o + p; }
        }
        have = 1;
        if (getenv("TALK_CACHE")) sscanf(getenv("TALK_CACHE"), "%d:%d", &cn, &cbsz);
        if (cn > MAXB) cn = MAXB;
        creset = getenv("TALK_CACHE_RESET") != NULL;
        atexit(report);
    }
    for (i = 1; i < TB_COUNT; i++)
        if (bend[i] && off >= boff[i] && off < bend[i]) { b = i; break; }
    rd[b]++; by[b] += len;
    if (len) cache_touch(off, len, b);
    return __real_plat_talk_read(off, dst, len);
}

int __wrap_talk_gen_exchange(TalkConv *c, TalkUtter *u, TalkExchange *ex, TalkTrace *tr)
{
    int r;
    if (creset) { int k; for (k = 0; k < MAXB; k++) cage[k] = 0; }
    inside++;
    r = __real_talk_gen_exchange(c, u, ex, tr);
    inside--;
    n_ex++;
    if (ex) n_lines += (uint64_t)ex->n;
    return r;
}
