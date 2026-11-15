/* pslice_policy.h - S3-2: the one decision of gba/src/pslice.c and ps1/src/pslice.c (may a portrait slice run
 * inside this frame), in one unit so both platforms and the core tests share it. Units are the platform's own
 * (GBA cycles, PS1 microseconds); `frame` is one vblank.
 *
 *   - it runs when the frame's own work (base) + the slices already run this frame (spent) + this one end inside
 *     the vblanks the frame needs anyway (at least two; a frame over two vblanks keeps a wider margin);
 *   - else, after the job has waited `patience` ticks (`patience_big` for a slice over `cheap`), ONE slice runs a
 *     frame even over the room, but only when it costs at most one more vblank than the frame already needs (S3-2: a
 *     forced slice that would cost two more vblanks waits for idle time); a job that has waited four times the big patience runs anyway,
 *     so nothing starves. */
#ifndef DOOMO_PSLICE_POLICY_H
#define DOOMO_PSLICE_POLICY_H
#include <stdint.h>

static inline int pslice_ok(uint32_t base, uint32_t spent, uint32_t est, uint32_t frame, uint32_t margin,
                            uint32_t margin_wide, int waited, int patience, int patience_big, uint32_t cheap)
{
    uint32_t vb = (base + frame - 1) / frame, limit;
    if (vb < 2) vb = 2;
    limit = vb * frame - (vb > 2 ? margin_wide : margin);
    if (base + spent + est <= limit) return 1;
    if (spent) return 0;                                    /* one forced slice a frame at most */
    if (waited < (est <= cheap ? patience : patience_big)) return 0;
    if (base + est <= (vb + 1) * frame - margin) return 1;  /* forced, one more vblank at most */
    return waited >= 4 * patience_big;
}

#endif
