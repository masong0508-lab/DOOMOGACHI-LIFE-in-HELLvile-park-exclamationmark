/* show_stats.c - per-frame stats, the shot RNG and the diagnostics lines
 * (owner: ENGINE lane 1; spec 8.9, 8.10, 13.4).
 *
 * Times are microseconds from ps1/src/timer.c (timer_us): update = start
 * of doomo_frame to the end of logic, draw = logic end to the end of
 * doomo_frame (display-list building), sync = the GPU wait of the previous
 * frame (g_perf.sync_us). They are stats only: nothing that reaches a pixel
 * or P1 ever reads a timer. prims / prim_bytes are the previous frame's
 * display list (g_perf), uploads are vram_uploads() bytes.
 */
#include <stdio.h>
#include "ps1.h"
#include "show_core.h"

static uint32_t s_t0, s_t1, s_t2;
static uint32_t s_stat[SHOW_STAT_COUNT];
static uint32_t s_rng = 1;

void show_stats_begin_frame(void)
{
    int i;
    s_t0 = timer_us();
    s_t1 = s_t2 = s_t0;
    for (i = 0; i < SHOW_STAT_COUNT; i++) s_stat[i] = 0;
}

void show_stats_mark_update(void) { s_t1 = timer_us(); }
void show_stats_mark_draw(void) { s_t2 = timer_us(); }

void show_stat_add(int stat, uint32_t n)
{
    if (stat >= 0 && stat < SHOW_STAT_COUNT) s_stat[stat] += n;
}

void show_stat_max(int stat, uint32_t n)
{
    if (stat >= 0 && stat < SHOW_STAT_COUNT && n > s_stat[stat]) s_stat[stat] = n;
}

void show_stats_publish(void)
{
    ShowMbox *m = &g_show_mbox;
    m->cyc_update = s_t1 - s_t0;
    m->cyc_draw = s_t2 - s_t1;
    m->cyc_sync = g_perf.sync_us;
    m->prims = g_perf.prims;
    m->prim_bytes = g_perf.prim_bytes;
    m->uploads = vram_uploads() + s_stat[SHOW_STAT_UPLOADS];
    m->evictions = s_stat[SHOW_STAT_EVICTIONS];
    m->pool_blocks_used = s_stat[SHOW_STAT_POOL_BLOCKS];
    m->sprites = s_stat[SHOW_STAT_SPRITES];
    m->user0 = s_stat[SHOW_STAT_USER0];
}

/* ---- RNG ---------------------------------------------------------------- */
void show_rng_seed(uint32_t s) { s_rng = s ? s : 0x6D2B79F5u; }
uint32_t show_rng_state(void) { return s_rng; }

uint32_t show_rng(void)
{
    uint32_t x = s_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s_rng = x;
}

uint32_t show_hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/* ---- diagnostics ---------------------------------------------------------- */
static void diag(char kind, const char *rule, const char *msg)
{
    const char *id = g_show_shot.ix ? g_show_shot.ix->id : "-";
    printf("show: %c %.8s %+d [%s] %s\n", kind, id, g_show_shot.t, rule ? rule : "?",
           msg ? msg : "");
}

void show_warn(const char *rule, const char *msg)
{
    g_show_mbox.warn_n++;
    diag('W', rule, msg);
}

void show_error(const char *rule, const char *msg)
{
    g_show_mbox.err_n++;
    diag('E', rule, msg);
}
