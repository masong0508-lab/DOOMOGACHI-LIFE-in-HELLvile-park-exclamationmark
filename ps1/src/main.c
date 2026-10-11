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

static void plat_init_hw(void)
{
    pad_init();          /* BIOS pad + card drivers before the SDK IRQ hook */
    memcard_early();
    gfx_init();
    dbg("ps1: gfx ok\n");
    memcard_init();
    dbg("ps1: card ok\n");
    InitGeom();
    timer_init();
    vram_init();
    dbg("ps1: vram ok\n");
    audio_hw_init();
    dbg("ps1: spu ok\n");

}

int main(void)
{
    plat_init_hw();
    dbg("DOOMOGACHI PS1: boot (core game %s)\n", core_has_game() ? "linked" : "missing");
#ifdef DOOMO_ENGINE_TEST
    etest_run();
#endif
    if (!core_has_game())
        etest_run();
    core_doomo_init();
    for (;;) {
        uint32_t b = pad_read();
        plat_begin_frame();
        core_doomo_frame(b);
        plat_end_frame();
    }
    return 0;
}
