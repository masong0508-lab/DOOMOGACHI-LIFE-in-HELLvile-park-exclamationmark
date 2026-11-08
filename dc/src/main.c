/* main.c - Dreamcast entry point, video and PVR setup, the 30 Hz frame
 * loop and its pacing (owner: dreamcast). The controls are pad.c (DC4).
 *
 * Frame protocol (ARCHITECTURE.md, platform.h):
 *     plat_init_hw(); doomo_init();
 *     for (;;) { b = dc_read_pad(); plat_begin_frame(); doomo_frame(b);
 *                plat_end_frame(); }
 *
 * Logic runs at 30 Hz, scheduled on the SH4's microsecond timer so the game
 * runs at the same speed on 60 Hz (VGA, NTSC) and 50 Hz (PAL) displays. A
 * drawn tick is one PVR scene: the 2D layer (DC3, gfx2d.c) collects the
 * tick's drawing as a prim list, and plat_end_frame submits it (pvr2d.c:
 * wait until the PVR takes a new scene, send the list); the PVR shows it
 * at the next vblank while the game builds the next tick. When the game
 * falls more than a tick behind (a slow
 * scene), the next tick runs with drawing skipped (dc_nodraw), but never
 * two in a row, like the GBA's catch-up ticks. Video mode: 640x480 in
 * RGB565; KOS picks VGA (480p) or interlaced NTSC/PAL from the cable. The
 * core draws a 320x240 logical screen at 2x (DC3, dc2d.h).
 */
#include <kos.h>
#include <dc/video.h>
#include <dc/pvr.h>
#include <dc/vblank.h>
#include <arch/timer.h>
#include "dc_plat.h"
#include "dc2d.h"
#include "doomo/game.h"
#include "doomo/input.h"
#include "doomo/palette.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define DC_TICK_US 33333u               /* 30 Hz logic */

volatile uint32_t dc_vblanks;
uint32_t dc_frames;
int dc_nodraw;

static uint64_t boot_us;                /* timer at main() */
static uint64_t logic_base_us;          /* timer at logic tick 0 */
static uint64_t work_us0;               /* timer at plat_begin_frame */
static int      skipped_in_row;
static int      scene_open;             /* this tick draws (a PVR scene at its end) */

uint32_t dc_us(void) { return (uint32_t)(timer_us_gettime64() - boot_us); }

static void on_vblank(uint32_t code, void *data)
{
    (void)code; (void)data;
    dc_vblanks++;
}

/* ---- video ----------------------------------------------------------------------- */
static void video_init(void)
{
    int cable = vid_check_cable();
    vid_set_mode(DM_640x480, PM_RGB565);
    dc2d_be_init();                     /* pvr2d.c: the PVR for the 2D layer */
    dc_logf("DC: video 640x480 %s", cable == CT_VGA ? "VGA" :
            cable == CT_RGB ? "RGB" : cable == CT_COMPOSITE ? "composite" : "?");
}

/* ---- frame pacing ------------------------------------------------------------------- */
void dc_pacing_reset(void)
{
    logic_base_us = timer_us_gettime64() - (uint64_t)dc_frames * DC_TICK_US;
    skipped_in_row = 0;
}

void plat_begin_frame(void)
{
    uint64_t now = timer_us_gettime64();
    uint32_t due = (uint32_t)((now - logic_base_us) / DC_TICK_US);
    work_us0 = now;
    if (due > dc_frames + 6) {          /* way behind (loading): forget the debt */
        dc_pacing_reset();
        due = dc_frames;
    }
    if (due > dc_frames + 1 && !skipped_in_row && !dc_mailbox.draw_all) {
        dc_nodraw = 1;
        skipped_in_row = 1;
        dc2d_begin(0);
        return;
    }
    dc_nodraw = 0;
    skipped_in_row = 0;
    dc2d_begin(1);
    scene_open = 1;
}

int plat_draw_skipped(void) { return dc_nodraw == 1; }

void plat_end_frame(void)
{
    uint64_t next;
    dc_frames++;
    dc_mailbox.ticks = dc_frames;
    if (scene_open) {
        uint32_t w;
        dc2d_end();                     /* submit the tick's prims to the PVR */
        w = (uint32_t)(timer_us_gettime64() - work_us0);
        dc_mailbox.work_sum += w;
        dc_mailbox.work_n++;
        if (w > dc_mailbox.work_max) dc_mailbox.work_max = w;
        scene_open = 0;
        dc_mailbox.drawn++;
        dc_mailbox.gfx_prims = dc2d_stats.prims;
        dc_mailbox.gfx_prims_max = dc2d_stats.prims_max;
        dc_mailbox.gfx_vbytes = dc2d_stats.vbytes;
        dc_mailbox.gfx_vbytes_max = dc2d_stats.vbytes_max;
        dc_mailbox.gfx_dropped = dc2d_stats.dropped;
        dc_mailbox.gfx_uploads = dc2d_stats.uploads;
        dc_mailbox.gfx_evictions = dc2d_stats.evictions;
        dc_mailbox.gfx_tex_live = (uint32_t)dc2d_tex_live();
    }
    /* wait for the next tick's slot; the vblank wait keeps the CPU idle */
    next = logic_base_us + (uint64_t)dc_frames * DC_TICK_US;
    while (timer_us_gettime64() + 2000u < next)
        vid_waitvbl();
    while (timer_us_gettime64() < next) {}
}

/* ---- boot ------------------------------------------------------------------------------ */
void plat_init_hw(void)
{
    dc_mailbox.ticks = 0;
    dc_mailbox.drawn = 0;
    dc_mailbox.hold_at = 0xFFFFFFFFu;  /* harness sync: no hold */
    dc_mailbox.draw_all = 0;
    /* the magic last: a harness that sees it may post a hold_at at once */
    __asm__ __volatile__("" ::: "memory");
    dc_mailbox.magic = DC_MAILBOX_MAGIC;
    vblank_handler_add(on_vblank, NULL);
    video_init();
    dc2d_init();                        /* palettes, font pages, texture cache */
    dc_log("DOOMOGACHI DC: boot ok");
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    boot_us = timer_us_gettime64();
    plat_init_hw();
    dc_boot_egg();                      /* X8: hold A+B+X+Y+START at power-on */
    dc_vmu_init();                      /* DC4: learn the VMU once (vmusave.c) */
    doomo_init();
    dc_pad_init();                      /* DC4: analog feel on (pad.c) */
    dc_pacing_reset();
    dc_logf("DC: core up in %u us, loop at 30 Hz", (unsigned)dc_us());
    for (;;) {
        uint32_t b;
        dc_harness_hold();
        if (dc_mailbox.cmd) dc_harness_cmd();
        b = dc_read_pad();
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
        dc_vmu_poll();                  /* DC4: a changed config record, written behind */
    }
    return 0;
}
