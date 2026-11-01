/* main.c - Dreamcast entry point, video and PVR setup, the 30 Hz frame
 * loop and its pacing, and the controller (owner: dreamcast).
 *
 * Frame protocol (ARCHITECTURE.md, platform.h):
 *     plat_init_hw(); doomo_init();
 *     for (;;) { b = dc_read_pad(); plat_begin_frame(); doomo_frame(b);
 *                plat_end_frame(); }
 *
 * Logic runs at 30 Hz, scheduled on the SH4's microsecond timer so the game
 * runs at the same speed on 60 Hz (VGA, NTSC) and 50 Hz (PAL) displays. A
 * drawn tick is one PVR scene: plat_begin_frame waits until the PVR can take
 * a new scene and opens it, plat_end_frame closes it; the PVR shows it at
 * the next vblank. When the game falls more than a tick behind (a slow
 * scene), the next tick runs with drawing skipped (dc_nodraw), but never
 * two in a row, like the GBA's catch-up ticks. Video mode: 640x480 in
 * RGB565; KOS picks VGA (480p) or interlaced NTSC/PAL from the cable. The
 * core draws a 320x240 logical screen at 2x (DC3).
 */
#include <kos.h>
#include <dc/video.h>
#include <dc/pvr.h>
#include <dc/vblank.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <arch/timer.h>
#include "dc_plat.h"
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
static int      scene_open;             /* a PVR scene is open this tick */
static uint8_t  clear_color;            /* plat_clear's colour, applied at begin */
static int      clear_dirty = 1;

uint32_t dc_us(void) { return (uint32_t)(timer_us_gettime64() - boot_us); }

static void on_vblank(uint32_t code, void *data)
{
    (void)code; (void)data;
    dc_vblanks++;
}

/* ---- video ----------------------------------------------------------------------- */
void dc_set_clear(uint8_t color)
{
    if (color != clear_color) { clear_color = color; clear_dirty = 1; }
}

static void apply_clear(void)
{
    uint8_t r, g, b;
    if (!clear_dirty) return;
    clear_dirty = 0;
    palette_rgb(clear_color, &r, &g, &b);
    pvr_set_bg_color(r / 255.0f, g / 255.0f, b / 255.0f);
}

static void video_init(void)
{
    int cable = vid_check_cable();
    vid_set_mode(DM_640x480, PM_RGB565);
    pvr_init_defaults();
    apply_clear();
    dc_logf("DC: video 640x480 %s", cable == CT_VGA ? "VGA" :
            cable == CT_RGB ? "RGB" : cable == CT_COMPOSITE ? "composite" : "?");
}

/* ---- input (DC4 adds the analog stick, the keyboard and more pads) -------- */
uint32_t dc_read_pad(void)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st;
    uint32_t k, b = 0;
    if (!dev) return 0;
    st = (cont_state_t *)maple_dev_status(dev);
    if (!st) return 0;
    k = st->buttons;
    if (k & CONT_DPAD_UP)    b |= BTN_UP;
    if (k & CONT_DPAD_DOWN)  b |= BTN_DOWN;
    if (k & CONT_DPAD_LEFT)  b |= BTN_LEFT;
    if (k & CONT_DPAD_RIGHT) b |= BTN_RIGHT;
    if (k & CONT_A)          b |= BTN_A;
    if (k & CONT_B)          b |= BTN_B;
    if (k & CONT_X)          b |= BTN_X;
    if (k & CONT_Y)          b |= BTN_Y;
    if (st->ltrig > 127)     b |= BTN_L;
    if (st->rtrig > 127)     b |= BTN_R;
    /* the pad has no SELECT: START+Y is SELECT (DECISIONS.md) */
    if ((k & CONT_START) && (k & CONT_Y)) b = (b & ~(uint32_t)BTN_Y) | BTN_SELECT;
    else if (k & CONT_START) b |= BTN_START;
    return b;
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
        return;
    }
    dc_nodraw = 0;
    skipped_in_row = 0;
    pvr_wait_ready();
    apply_clear();
    pvr_scene_begin();
    scene_open = 1;
}

int plat_draw_skipped(void) { return dc_nodraw == 1; }

void plat_end_frame(void)
{
    uint64_t next;
    dc_frames++;
    dc_mailbox.ticks = dc_frames;
    if (scene_open) {
        uint32_t w = (uint32_t)(timer_us_gettime64() - work_us0);
        dc_mailbox.work_sum += w;
        dc_mailbox.work_n++;
        if (w > dc_mailbox.work_max) dc_mailbox.work_max = w;
        pvr_scene_finish();             /* blank lists for the unused ones */
        scene_open = 0;
        dc_mailbox.drawn++;
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
    dc_mailbox.magic = DC_MAILBOX_MAGIC;
    dc_mailbox.ticks = 0;
    dc_mailbox.drawn = 0;
    dc_mailbox.hold_at = 0xFFFFFFFFu;  /* harness sync: no hold */
    dc_mailbox.draw_all = 0;
    vblank_handler_add(on_vblank, NULL);
    video_init();
    dc_log("DOOMOGACHI DC: boot ok");
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    boot_us = timer_us_gettime64();
    plat_init_hw();
    doomo_init();
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
    }
    return 0;
}
