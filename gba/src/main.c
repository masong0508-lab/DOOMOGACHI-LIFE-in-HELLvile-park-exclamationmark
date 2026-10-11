/* main.c - GBA entry point, frame pacing, input and misc platform calls
 * (owner: gba).
 *
 * Frame protocol (ARCHITECTURE.md):
 *     plat_init_hw(); doomo_init();
 *     for (;;) { b = read_pad(); plat_begin_frame(); doomo_frame(b);
 *                plat_end_frame(); }
 * Logic runs at 30 Hz: a drawn frame is presented every 2nd vblank. When
 * a frame takes longer (heavy 3D), the next tick runs as a "catch-up"
 * tick with drawing skipped (gba_nodraw), so game speed stays 30 Hz while
 * the display drops to 20/15 fps instead of the game slowing down.
 */
#include <stdarg.h>
#include "gba_plat.h"
#include "doomo/game.h"
#include "doomo/input.h"

volatile uint32_t gba_vblanks;
uint32_t gba_frames;
uint32_t gba_keys;
int gba_nodraw;

static uint32_t last_flip_vbl;
static int      skipped_in_row;
static uint32_t logic_base_vbl;     /* vblank count at logic tick 0 */

/* ---------------------------------------------------------------------- */
/* debug log (mGBA: 0x04FFF780 enable, 0x04FFF600 string, 0x04FFF700 flags) */
static int mgba_log_ok = -1;

void gba_log(const char *msg)
{
    if (mgba_log_ok < 0) {
        REG_MGBA_ENABLE = 0xC0DE;
        mgba_log_ok = (REG_MGBA_ENABLE == 0x1DEA);
    }
    if (!mgba_log_ok || !msg) return;
    {
        volatile char *dst = MGBA_STRING;
        int i = 0;
        while (msg[i] && i < 255) { dst[i] = msg[i]; i++; }
        dst[i] = 0;
        REG_MGBA_FLAGS = 3 | 0x100;    /* level INFO, send */
    }
}

static char *fmt_uint(char *p, char *end, uint32_t v, int base, int width,
                      char pad)
{
    char tmp[12];
    int n = 0;
    do {
        uint32_t d = v % (uint32_t)base;
        tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (uint32_t)base;
    } while (v && n < 11);
    while (n < width && n < 11) tmp[n++] = pad;
    while (n > 0 && p < end) *p++ = tmp[--n];
    return p;
}

void gba_logf(const char *fmt, ...)
{
    char buf[200];
    char *p = buf, *end = buf + sizeof(buf) - 1;
    va_list ap;
    va_start(ap, fmt);
    while (*fmt && p < end) {
        if (*fmt != '%') { *p++ = *fmt++; continue; }
        fmt++;
        {
            char pad = ' ';
            int width = 0;
            if (*fmt == '0') { pad = '0'; fmt++; }
            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
            switch (*fmt) {
            case 'd': {
                int v = va_arg(ap, int);
                if (v < 0 && p < end) { *p++ = '-'; v = -v; }
                p = fmt_uint(p, end, (uint32_t)v, 10, width, pad);
                break; }
            case 'u': p = fmt_uint(p, end, va_arg(ap, uint32_t), 10, width, pad); break;
            case 'x': p = fmt_uint(p, end, va_arg(ap, uint32_t), 16, width, pad); break;
            case 's': {
                const char *s = va_arg(ap, const char *);
                while (s && *s && p < end) *p++ = *s++;
                break; }
            case 'c': *p++ = (char)va_arg(ap, int); break;
            case '%': *p++ = '%'; break;
            default: break;
            }
            if (*fmt) fmt++;
        }
    }
    va_end(ap);
    *p = 0;
    gba_log(buf);
}

/* ---------------------------------------------------------------------- */
/* platform.h misc */
void plat_log(const char *msg) { gba_log(msg); }
const char *plat_name(void) { return "GBA"; }
uint32_t plat_ticks(void) { return gba_frames; }

uint32_t plat_entropy(void)
{
    static uint32_t mix = 0x9E3779B9u;
    mix ^= gba_cycles() + (gba_vblanks << 16) + REG_VCOUNT + (gba_keys << 7) +
           gba_frames * 2654435761u;
    mix = (mix << 13) | (mix >> 19);
    mix *= 0x85EBCA6Bu;
    mix ^= mix >> 16;
    return mix;
}

/* ---------------------------------------------------------------------- */
/* input */
static uint32_t read_pad(void)
{
    uint32_t k = (~(uint32_t)REG_KEYINPUT) & KEY_MASK;
    uint32_t b = DOOMO_INPUT_SYNTH_XY;
    gba_keys = k;
    if (k & KEY_UP)     b |= BTN_UP;
    if (k & KEY_DOWN)   b |= BTN_DOWN;
    if (k & KEY_LEFT)   b |= BTN_LEFT;
    if (k & KEY_RIGHT)  b |= BTN_RIGHT;
    if (k & KEY_A)      b |= BTN_A;
    if (k & KEY_B)      b |= BTN_B;
    if (k & KEY_L)      b |= BTN_L;
    if (k & KEY_R)      b |= BTN_R;
    if (k & KEY_START)  b |= BTN_START;
    if (k & KEY_SELECT) b |= BTN_SELECT;
    return b;
}
uint32_t gba_read_pad(void) { return read_pad(); }

/* ---------------------------------------------------------------------- */
/* frame pacing */
void plat_begin_frame(void)
{
    /* Catch up when the logic has fallen more than one tick behind the
     * 30 Hz schedule, but never skip two frames in a row (>= 15 fps). */
    uint32_t due = (gba_vblanks - logic_base_vbl) >> 1;
    if (due > gba_frames + 6) {
        /* way behind (loading, saving): forget the debt */
        logic_base_vbl = gba_vblanks - gba_frames * 2;
        due = gba_frames;
    }
    if (due > gba_frames + 1 && !skipped_in_row) {
        gba_nodraw = 1;
        skipped_in_row = 1;
    } else {
        gba_nodraw = 0;
        skipped_in_row = 0;
    }
}

void plat_end_frame(void)
{
    gba_frames++;
    if (gba_nodraw) return;
    video_flip();
}

void video_flip(void)
{
    int waited = 0;
    while (gba_vblanks - last_flip_vbl < 2) {
        vblank_intr_wait();
        waited = 1;
    }
    if (!waited && REG_VCOUNT < 160) vblank_intr_wait();
    REG_DISPCNT ^= DCNT_PAGE;
    gba_back = (REG_DISPCNT & DCNT_PAGE) ? (uint16_t *)VRAM_PAGE0
                                         : (uint16_t *)VRAM_PAGE1;
    last_flip_vbl = gba_vblanks;
}

/* ---------------------------------------------------------------------- */
static void timers_init(void)
{
    /* timers 2+3: free-running 32-bit cycle counter */
    REG_TM2CNT = 0; REG_TM3CNT = 0;
    REG_TM2D = 0;   REG_TM3D = 0;
    REG_TM3CNT = TM_ENABLE | TM_CASCADE;
    REG_TM2CNT = TM_ENABLE | TM_FREQ_1;
}

void plat_init_hw(void)
{
    timers_init();
    video_init();
    gba_irq_init();
    audio_hw_init();
    flash_init();
    r3d_init();
    ow_init();
    gba_logf("DOOMOGACHI GBA: boot ok, flash=%dKB (%s)", flash_detected(),
             flash_chip_name());
}

#ifdef ENGINE_TEST
void enginetest_main(void);
#endif

int main(void)
{
    plat_init_hw();
#ifdef ENGINE_TEST
    enginetest_main();
#else
    doomo_init();
    logic_base_vbl = gba_vblanks;
    last_flip_vbl = gba_vblanks;
    for (;;) {
        uint32_t b = read_pad();
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
#endif
    return 0;
}
