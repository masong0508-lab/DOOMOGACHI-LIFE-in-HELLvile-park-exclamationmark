/* native.h - internals of the headless native platform (owner:
 * foundation). See core/native/README.md. */
#ifndef DOOMO_NATIVE_H
#define DOOMO_NATIVE_H

#include "doomo/doomo.h"

#define NAT_MAX_W 640
#define NAT_MAX_H 480

/* the 8bpp framebuffer (row-major, nat_w * nat_h) */
extern uint8_t nat_fb[NAT_MAX_W * NAT_MAX_H];
extern int nat_w, nat_h;

typedef struct NatOptions {
    int w, h;
    const char *save_dir;     /* where slotN.sav files live          */
    uint32_t seed;            /* plat_entropy() value                */
    int quiet;                /* suppress plat_log echo              */
    int verbose;              /* also log audio calls                */
} NatOptions;

void nat_init(const NatOptions *o);
void nat_set_entropy(uint32_t v);      /* S2: the runner's `seed N` (SPEC C18) */
/* "STUB:" lines logged so far (unique names). */
int  nat_stub_count(void);
const char *nat_stub_name(int i);
/* > 0: plat_save_scratch() offers only a window of this many bytes (plus
 * the codec's DOOMO_SAVE_WORK_SIZE, save.h), so saves stream through
 * plat_save_stream_* / plat_save_read_at (the GBA's way); 0: the
 * whole-image path. Counters of streamed chunks (tests). */
extern int nat_save_stream;
extern uint32_t nat_stream_writes, nat_stream_reads;
extern int nat_stream_fail_at;          /* > 0: that stream write fails  */
/* audio call counters (tests) */
extern uint32_t nat_tone_calls, nat_sample_calls;
extern int nat_last_sample;

/* draw-call counters (runner "stats"): calls since start, per primitive */
enum { NC_RECT, NC_TEXT, NC_IMAGE, NC_CHECKER, NC_SHADE, NC_LINE, NC_CLEAR, NC_VIEW, NC_N };
extern uint32_t nat_calls[NC_N];

/* PNG writer: 8bpp indexed image -> indexed PNG with the master palette,
 * optionally zoomed by an integer factor. Returns 1 on success. */
int nat_png_write(const char *path, const uint8_t *pix, int w, int h, int zoom);
/* Generic PNG encoder used by the above (stored deflate, no zlib). */
int nat_png_encode(const char *path, const uint8_t *pix, int w, int h,
                   const uint8_t *rgb_palette /* 768 bytes */);

/* framebuffer helpers shared by the renderers */
static inline void nat_px(int x, int y, uint8_t c)
{
    if ((unsigned)x < (unsigned)nat_w && (unsigned)y < (unsigned)nat_h)
        nat_fb[y * nat_w + x] = c;
}

/* 4x4 ordered dither threshold 0..15 */
static inline int nat_bayer(int x, int y)
{
    static const uint8_t b[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 },
                                     { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    return b[y & 3][x & 3];
}

#endif
