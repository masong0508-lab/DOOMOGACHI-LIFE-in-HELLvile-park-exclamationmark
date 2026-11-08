/* pslice.c - when the portrait cache's sliced jobs run on the PS1 (SP2: X1, P-D1; owner: ps1).
 *
 * The same policy as the GBA's gba/src/pslice.c, in microseconds of CPU time (timer_us(), root counter 1):
 *
 *   - Idle time (ps1_pslice_idle, from gfx_end before DrawSync): after the frame's own CPU work, slices run while
 *     the next one's measured cost still ends inside the frame's two vblanks (33.3 ms from gfx_begin). The GPU
 *     is still drawing the previous list meanwhile, so nothing waits for them.
 *   - Inside a frame (plat_portrait_slice_ok): a slice runs when the last frame's own CPU time (without its
 *     slices) plus the slices already run this frame plus this one stay inside the vblanks the frame needs
 *     anyway, or when the job has waited PATIENCE frames (PATIENCE_BIG for a dear slice; one forced slice a
 *     frame at most). The PS1 runs one logic tick per drawn frame, so a frame over 33.3 ms is the game running
 *     slower: the room matters more than on the GBA.
 *   - The core forces slices itself for a job behind a stand-in that is due (portrait.c, PC_STANDIN).
 *
 * Costs are learned per slice class (the newest sample, or the old value less an eighth if that is more).
 * 156 B of RAM. */
#include "ps1.h"
#include "doomo/portrait.h"

/* weak references: the engine-test build links no portrait cache */
extern int portrait_idle_next(void) __attribute__((weak));
extern void portrait_idle_run(void) __attribute__((weak));

#ifdef DOOMO_PAL
#define FRAME_US    20000u                  /* one vblank */
#else
#define FRAME_US    16683u
#endif
#define UNKNOWN_US  12000u                  /* a class never timed yet */
#define MARGIN_US   700u
#define MARGIN_WIDE_US 3000u
#define PATIENCE    4
#define PATIENCE_BIG 15
#define CHEAP_US    4000u
#define NCLS        PORTRAIT_SLICE_CLASSES

static uint16_t cost_us[NCLS];
static uint32_t c0, spent, base_last, spent_frame;
static uint8_t  in_idle;

static uint32_t est(int cls)
{
    uint32_t c = (cls >= 0 && cls < NCLS) ? cost_us[cls] : 0;
    return c ? c + c / 10 + MARGIN_US : UNKNOWN_US;
}

static void learn(int cls, uint32_t c)
{
    uint32_t old, v;
    if (cls < 0 || cls >= NCLS) return;
    old = cost_us[cls];
    v = old - old / 8;
    if (c > v) v = c;
    cost_us[cls] = (uint16_t)(v > 0xFFFFu ? 0xFFFFu : v ? v : 1);
}

int plat_portrait_slice_ok(int cls, int waited)
{
    uint32_t vb, limit;
    c0 = timer_us();
    if (in_idle) return 1;
    if (spent_frame != g_frame) { spent_frame = g_frame; spent = 0; }
    if (waited < 0) return 1;
    vb = (base_last + FRAME_US - 1) / FRAME_US;
    if (vb < 2) vb = 2;
    limit = vb * FRAME_US - (vb > 2 ? MARGIN_WIDE_US : MARGIN_US);
    if (base_last + spent + est(cls) <= limit) return 1;
    if (spent) return 0;
    return waited >= (est(cls) <= CHEAP_US ? PATIENCE : PATIENCE_BIG);
}

void plat_portrait_slice_done(int cls)
{
    uint32_t c = timer_us() - c0;
    learn(cls, c);
    if (!in_idle) {
        if (spent_frame != g_frame) { spent_frame = g_frame; spent = 0; }
        spent += c;
    }
}

/* gfx_end: the CPU time of the frame that just ended, so the next one knows its base */
void ps1_pslice_frame_end(uint32_t cpu_us)
{
    uint32_t s = spent_frame == g_frame ? spent : 0;
    base_last = cpu_us > s ? cpu_us - s : 0;
}

/* gfx_end, before DrawSync: run slices while the next one ends inside the frame's two vblanks (from t_begin) */
void ps1_pslice_idle(uint32_t t_begin)
{
    int n;
    if (!portrait_idle_next || !portrait_idle_run) return;
    in_idle = 1;
    for (n = 0; n < 32; n++) {
        uint32_t used;
        int cls = portrait_idle_next();
        if (cls < 0) break;
        used = timer_us() - t_begin;
        if (used + est(cls) + MARGIN_US > 2u * FRAME_US) break;
        portrait_idle_run();
    }
    in_idle = 0;
}
