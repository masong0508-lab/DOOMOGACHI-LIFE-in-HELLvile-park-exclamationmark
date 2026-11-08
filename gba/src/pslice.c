/* pslice.c - when the portrait cache's sliced jobs run on the GBA (SP2, X1; owner: gba).
 *
 * The core cuts every portrait / figure / chibi re-render into slices (portrait.c: the set-up and 25 pieces of
 * the layer groups; mostly 1-8 ms each on the GBA). This file decides where they go:
 *
 *   - Idle time (gba_pslice_idle, from video_flip): after a drawn frame, while it waits for its vblank, slices
 *     run as long as the next one's measured cost fits before that vblank. A frame of 20 ms leaves ~13 ms a
 *     tick, and nothing on screen waits for it.
 *   - Inside a frame (plat_portrait_slice_ok): a slice runs when the last frame's own work (without the slices
 *     it ran) plus the slices already run this tick plus this one stay inside the vblank the frame needs anyway
 *     (a 22 ms frame has room up to 33.4 ms, a 40 ms one up to 50.1 ms), or when the job has waited PATIENCE
 *     ticks (PATIENCE_BIG for a slice over ~5 ms; one slice then: animations always move on).
 *   - The core forces slices itself for a job behind a stand-in that is due (portrait.c, PC_STANDIN).
 *
 * Costs are learned per slice class (PORTRAIT_SLICE_CLASSES: kind x slice): the newest sample, or the old
 * value less an eighth if that is more (a slow outlier fades in a few samples, a fast one never lowers the
 * guess at once). 172 B of EWRAM. */
#include "gba_plat.h"
#include "doomo/portrait.h"

/* weak references: the engine test's NOCORE build links no portrait cache (then there is nothing to run) */
extern int portrait_idle_next(void) __attribute__((weak));
extern void portrait_idle_run(void) __attribute__((weak));

#define FRAME_CYC   280896u                 /* one video frame */
#define LINE_CYC    1232u
#define UNKNOWN_CYC 300000u                 /* a class never timed yet: more than any measured slice */
#define MARGIN_CYC  6000u
#define MARGIN_WIDE_CYC 60000u
#define PATIENCE    4                       /* ticks a job may wait for room before one cheap slice is forced */
#define PATIENCE_BIG 15                     /* ... before one dear slice (over CHEAP_CYC) is */
#define CHEAP_CYC   90000u
#define NCLS        PORTRAIT_SLICE_CLASSES

static uint16_t cost64[NCLS];               /* learned cost, 64-cycle units (0: never timed) */
static uint32_t c0;                         /* gba_cycles() when the slice started */
static uint32_t spent;                      /* cycles of slices run inside this tick's frame */
static uint32_t spent_tick;                 /* the tick `spent` belongs to */
static uint32_t base_last;                  /* the last drawn frame's work without its slices */
static uint8_t  in_idle;

static uint32_t est(int cls)
{
    uint32_t c = (cls >= 0 && cls < NCLS) ? (uint32_t)cost64[cls] << 6 : 0;
    return c ? c + c / 10 + MARGIN_CYC : UNKNOWN_CYC;
}

static void learn(int cls, uint32_t c)
{
    uint32_t old, v;
    if (cls < 0 || cls >= NCLS) return;
    old = (uint32_t)cost64[cls] << 6;
    v = old - old / 8;
    if (c > v) v = c;
    v >>= 6;
    cost64[cls] = (uint16_t)(v > 0xFFFFu ? 0xFFFFu : v ? v : 1);
}

int plat_portrait_slice_ok(int cls, int waited)
{
    uint32_t vb, limit;
    c0 = gba_cycles();
    if (in_idle) return 1;
    if (spent_tick != gba_frames) { spent_tick = gba_frames; spent = 0; }
    if (waited < 0) return 1;                               /* forced by the core: just timed */
    vb = (base_last + FRAME_CYC - 1) / FRAME_CYC;
    if (vb < 2) vb = 2;
    /* a frame that is over 2 vblanks anyway varies more from tick to tick (a dialogue over the overworld): a
     * wider margin keeps its slices off the next vblank */
    limit = vb * FRAME_CYC - (vb > 2 ? MARGIN_WIDE_CYC : MARGIN_CYC);
    if (base_last + spent + est(cls) <= limit) return 1;
    if (spent) return 0;                                    /* one forced slice a tick at most */
    return waited >= (est(cls) <= CHEAP_CYC ? PATIENCE : PATIENCE_BIG);
}

void plat_portrait_slice_done(int cls)
{
    uint32_t c = gba_cycles() - c0;
    learn(cls, c);
    if (!in_idle) {
        if (spent_tick != gba_frames) { spent_tick = gba_frames; spent = 0; }
        spent += c;
    }
}

/* plat_end_frame (after its gba_frames++): the work of the drawn frame of tick gba_frames - 1 (gba_work_last),
 * so the next tick knows its base */
void gba_pslice_frame_end(uint32_t work)
{
    uint32_t s = spent_tick == gba_frames - 1 ? spent : 0;
    base_last = work > s ? work - s : 0;
}

/* cycles until gba_vblanks reaches `target` (0 if it has) */
static uint32_t cycles_until(uint32_t target)
{
    uint32_t v, vc, v2, lines;
    do { v = gba_vblanks; vc = REG_VCOUNT; v2 = gba_vblanks; } while (v != v2);
    if ((int32_t)(target - v) <= 0) return 0;
    if (vc >= 160 && vc < 228) lines = 228 - vc + 160;
    else lines = 160 - (vc < 160 ? vc : 160);
    lines += (target - v - 1) * 228u;
    return lines * LINE_CYC;
}

/* video_flip, before it waits for vblank `target`: run slices while the next one fits */
void gba_pslice_idle(uint32_t target)
{
    int n;
    if (!portrait_idle_next || !portrait_idle_run) return;
    in_idle = 1;
    for (n = 0; n < 24; n++) {
        int cls = portrait_idle_next();
        if (cls < 0) break;
        if (est(cls) > cycles_until(target)) break;
        portrait_idle_run();                /* calls plat_portrait_slice_ok / _done: timed and learned */
    }
    in_idle = 0;
}
