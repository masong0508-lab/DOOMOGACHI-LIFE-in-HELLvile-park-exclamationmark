/* main.c - PS1 entry point, frame protocol and misc platform services.
 *
 *   plat_init_hw(); doomo_init();
 *   for (;;) { b = pad_read(); plat_begin_frame(); doomo_frame(b);
 *              plat_end_frame(); }
 *
 * Logic runs at 30 Hz (two NTSC vblanks per tick); rendering is double
 * buffered, the GPU draws frame N while the CPU builds frame N+1.
 * Build with -DDOOMO_ENGINE_TEST=ON to boot into the 3D engine showcase.
 */
#include <stdio.h>
#include <stdarg.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxetc.h>
#include <hwregs_c.h>
#include "ps1.h"

void plat_begin_frame(void)
{
    gfx_begin();
    vram_frame();
}

void plat_end_frame(void)
{
    audio_hw_frame();
    gfx_end();
}

uint32_t plat_ticks(void) { return g_frame; }

/* frames run by the game loop (core or engine test) since boot; the test
 * harness uses it as heartbeat and boot sync */
volatile uint32_t g_loop_frames;

uint32_t plat_entropy(void)
{
    uint32_t v = (uint32_t)VSync(-1) * 2654435761u;
    v ^= (uint32_t)TIMER_VALUE(2) << 16;
    v ^= (uint32_t)TIMER_VALUE(1);
    v ^= g_frame * 40503u;
    return v;
}

void plat_log(const char *msg)
{
    if (msg)
        printf("%s\n", msg);
}

const char *plat_name(void) { return "PS1"; }

void dbg(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s", buf);
}

/* A cute splash while the BIOS reads the memory card directories (the
 * BIOS card driver needs a few seconds for both slots). */
static void boot_splash(void)
{
    const DoomoImage *egg = core_sprite_image(SPR_EGG, 0);
    int f;
    for (f = 0; f < 2; f++) {       /* both buffers, so it stays up */
        plat_begin_frame();
        plat_clear(PAL_UI_PINK);
        plat_rect(0, 150, SCR_W, 90, PAL_UI_PINK_D);
        plat_rect(0, 148, SCR_W, 2, PAL_UI_CREAM);
        if (egg && egg->w <= 64 && egg->h <= 64)
            plat_image(SCR_W / 2 - egg->w, 140 - egg->h * 2, egg, 2, IMG_SHADOW, 0);
        plat_text(SCR_W / 2 - 16 * 6 / 2, 32, "DOOMOGACHI LIFE", PAL_UI_PINK_D);
        plat_text(SCR_W / 2 - 17 * 6 / 2, 44, "in HELLvile Park!", PAL_UI_PURPLE);
        plat_text(SCR_W / 2 - 25 * 6 / 2, 196, "Checking MEMORY CARD " STR_ICON_HEART "...", PAL_UI_CREAM);
        plat_end_frame();
    }
    DrawSync(0);
}

static void plat_init_hw(void)
{
    pad_init();          /* BIOS pad + card drivers before the SDK IRQ hook */
    memcard_early();
    gfx_init();
    InitGeom();
    timer_init();
    vram_init();
    boot_splash();
    memcard_init();      /* blocks ~2-3 s (BIOS _bu_init) */
    audio_hw_init();
    dbg("ps1: hardware ready (%s)\n", plat_name());
}

int main(void)
{
    plat_init_hw();
#if defined(DOOMO_ENGINE_TEST) || !defined(DOOMO_HAVE_CORE)
    dbg("DOOMOGACHI PS1: boot (engine test)\n");
    /* Engine showcase / platform bring-up. It never returns, and the game
     * loop below is compiled out so --gc-sections drops the game's scenes
     * (the engine test only keeps the core pieces it uses: maps, art,
     * combat, the roster's looks) - that keeps its disc inside 2 MB. */
    etest_run();
#else
    dbg("DOOMOGACHI PS1: boot (core game %s)\n", core_has_game() ? "linked" : "missing");
    if (!core_has_game()) {
        for (;;) {      /* should never happen: the core is linked */
            plat_begin_frame();
            plat_clear(PAL_UI_RED_D);
            plat_text(40, 110, "DOOMOGACHI: core not linked", PAL_WHITE);
            plat_end_frame();
        }
    }
    core_doomo_init();
    for (;;) {
        uint32_t b;
        hcmd_poll();
        b = pad_read();
        plat_begin_frame();
        core_doomo_frame(b);
        plat_end_frame();
        g_loop_frames++;
        if ((g_frame % 150) == 0) {
            extern uint32_t g_rect_calls, g_rect_prims;
            dbg("perf: frame=%u fps=%d.%d cpu=%uus gpuwait=%uus prims=%u/%uB rects=%u->%u "
                "3d=%uus polys=%d upload=%uB\n",
                (unsigned)g_frame, (int)(g_perf.fps_x10 / 10), (int)(g_perf.fps_x10 % 10),
                (unsigned)g_perf.cpu_us, (unsigned)g_perf.sync_us, (unsigned)g_perf.prims,
                (unsigned)g_perf.prim_bytes, (unsigned)g_rect_calls, (unsigned)g_rect_prims,
                (unsigned)g_r3stats.cpu_us, g_r3stats.polys, (unsigned)vram_uploads());
        }
        {
            extern uint32_t g_rect_calls, g_rect_prims;
            g_rect_calls = g_rect_prims = 0;
        }
    }
#endif
    return 0;
}
