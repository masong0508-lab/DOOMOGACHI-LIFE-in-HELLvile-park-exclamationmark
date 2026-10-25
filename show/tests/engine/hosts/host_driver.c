/* host_driver.c - the hosts' test player (owner: show, ENGINE lane 3).
 *
 * Stands in for the player's shot loop (lane 1) until it lands, and stays
 * as the stage kinds' own test: plays every shot of the generated table
 * (the scenes folder's .shw files, packed by showtk/hosts.py) in order, each for its length,
 * through exactly the plug-in calls the player makes:
 *
 *     t == 0: show_host_prep(rec);  every t: show_host_tick(t);
 *     camfx_tick(); show_host_draw(t); camfx_apply(); show_host_draw_hud(t);
 *     then the matte bars.
 *
 * Timing: g_perf (gfx.c) of every frame goes into a RAM table and is printed
 * on the TTY when a shot ends ("hostperf ..."), so the printf never lands in
 * a measured frame; run_hosts.py turns it into the frame/timing report.
 * `scene <shot> 0` (the harness mailbox) jumps to a shot; a `seed`-free,
 * wall-clock-free run is fully deterministic.
 */
#include <stdio.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "show/show_hosts.h"
#include "ps1.h"

typedef struct HostScene { const char *name; int len; const uint32_t *rec; uint32_t bytes; } HostScene;
extern const HostScene host_scenes[];
extern const int host_scene_count;

static uint32_t s_frames, s_uid = 0x01000000u;
static int s_init;
static int s_scene = -1, s_t, s_jump = -1;
static int s_warm = -1, s_warm_more;

uint32_t doomo_image_new_uid(void)
{
    if (++s_uid == 0) s_uid = 0x01000000u;
    return s_uid;
}
uint32_t doomo_frame_count(void) { return s_frames; }
int doomo_initialized(void) { return s_init; }

int host_select(const char *name, int arg);
int host_select(const char *name, int arg)
{
    int i;
    (void)arg;
    for (i = 0; i < host_scene_count; i++)
        if (!strcmp(host_scenes[i].name, name)) { s_jump = i; return 1; }
    return 0;
}

/* the test cast: fixed looks in doomo_town.res[0..7] (the player's
 * show_cast.c fills these from the LOOK chunk) */
static void cast_init(void)
{
    static const uint32_t seeds[8] = { 0x5101u, 0x5102u, 0x5103u, 0x5104u, 0x5105u, 0x5106u, 0x5107u, 0x5108u };
    int i;
    for (i = 0; i < 8; i++) {
        DoomoResident *r = &doomo_town.res[i];
        resident_default(r);
        resident_randomize_look(r, seeds[i]);
        r->status = RES_ALIVE;
    }
    doomo_town.res[0].look.species = SPECIES_BUNNYKIN;    /* "sunny" */
    doomo_town.res[1].look.species = SPECIES_CHERUB;      /* "mayor" */
    doomo_town.res[2].look.species = SPECIES_SHEEPKIN;    /* "nana"  */
    doomo_town.res[3].look.species = SPECIES_IMP;         /* "pip" (a kid in the kid test) */
    for (i = 0; i < 8; i++) portrait_invalidate(i);
}

void doomo_init(void)
{
    palette_init();
    textures_init();
    sprites_init();
    rng_seed(&doomo_rng, 0x00101101u);
    opt_defaults();
    opt_set(OPT_SCREEN_SHAKE, 2);       /* the show profile: shakes at 100 % */
    cast_init();
    portrait_cache_reset();
    printf("hosttest: %d shot(s)\n", host_scene_count);
    s_frames = 0;
    s_init = 1;
}

/* ---- per-frame timing, printed when the shot ends ---------------------------------- */
#define PERF_MAX 600
static uint16_t s_cpu[PERF_MAX], s_sync[PERF_MAX], s_draw[PERF_MAX], s_hud[PERF_MAX];
static uint16_t s_prims[PERF_MAX], s_bb[PERF_MAX];
static uint32_t s_prep_us;

static uint16_t sat16(uint32_t v) { return (uint16_t)(v > 65535u ? 65535u : v); }

static void perf_dump(int sc)
{
    int i, n = host_scenes[sc].len < PERF_MAX ? host_scenes[sc].len : PERF_MAX;
    printf("hostperf begin %s len=%d prep_us=%u forced=%d\n", host_scenes[sc].name, host_scenes[sc].len,
           (unsigned)s_prep_us, show_gore_forced_frames());
    for (i = 0; i < n; i++)
        printf("hostperf %s %d %u %u %u %u %u %u\n", host_scenes[sc].name, i, s_cpu[i], s_sync[i], s_draw[i],
               s_hud[i], s_prims[i], s_bb[i]);
    printf("hostperf end %s\n", host_scenes[sc].name);
}

static void bars(void)
{
    plat_rect(0, 0, 320, 30, PAL_BLACK);
    plat_rect(0, 210, 320, 30, PAL_BLACK);
}

void doomo_frame(uint32_t buttons)
{
    char buf[48];
    (void)buttons;
    /* the timing of the frame before this one (g_perf: gfx_end of it) */
    if (s_scene >= 0 && s_t > 0 && s_t <= PERF_MAX) {
        s_cpu[s_t - 1] = sat16(g_perf.cpu_us);
        s_sync[s_t - 1] = sat16(g_perf.sync_us);
        s_prims[s_t - 1] = sat16(g_perf.prim_bytes);
    }
    if (s_scene >= host_scene_count) s_jump = -1;
    else if (s_jump >= 0 || s_scene < 0 || s_t >= host_scenes[s_scene].len) {
        if (s_scene >= 0) perf_dump(s_scene);
        show_host_end();
        if (s_jump >= 0) { s_scene = s_jump; s_jump = -1; }
        else s_scene++;
        s_t = 0;
        s_warm = -1;
        if (s_scene >= host_scene_count) {
            printf("hosttest: done\n");
            s_scene = host_scene_count;
        }
    }
    s_frames++;
    if (s_scene >= host_scene_count) {
        plat_clear(PAL_BLACK);
        plat_text(100, 116, "hosttest: done", PAL_WHITE);
        return;
    }
    if (s_t == 0 && s_warm < 0) {
        uint32_t t0 = timer_us();
        camfx_reset();
        portrait_cache_reset();
        if (show_host_prep(host_scenes[s_scene].rec, host_scenes[s_scene].bytes) < 0)
            printf("hosttest: %s: prep failed: %s\n", host_scenes[s_scene].name, show_host_error());
        s_prep_us = timer_us() - t0;
        s_warm = 0;
        s_warm_more = 1;
    }
    /* the warm-up: always SHOST_WARM_MAX uncaptured frames (a fixed count
     * keeps run_hosts.py's frame arithmetic simple; the player stops at 0) */
    if (s_warm < SHOST_WARM_MAX) {
        plat_clear(PAL_BLACK);
        if (s_warm_more) s_warm_more = show_host_warm();
        s_warm++;
        bars();
        return;
    }
    show_host_tick(s_t);
    camfx_tick();
    plat_clear(PAL_BLACK);
    show_host_draw(s_t);
    camfx_apply();
    show_host_draw_hud(s_t);
    bars();
    doomo_snprintf(buf, sizeof(buf), "%s +%d", host_scenes[s_scene].name, s_t);
    plat_text(4, 226, buf, PAL_GRAY);
    if (s_t < PERF_MAX) {
        s_draw[s_t] = sat16(g_show_host_stats.draw_us);
        s_hud[s_t] = sat16(g_show_host_stats.hud_us);
        s_bb[s_t] = g_show_host_stats.sprites;
    }
    s_t++;
}
