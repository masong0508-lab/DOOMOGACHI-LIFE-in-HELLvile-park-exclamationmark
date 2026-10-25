/* show_purity.c - what resets at every shot start (owner: ENGINE lane 1;
 * spec 8.2).
 *
 * After this a shot's frames depend only on its chunk, the seeds and the
 * frame number, so any frame renders alone (a seek pre-rolls from frame 0
 * with drawing skipped) and two renders are identical (AT2.3, AT3.3).
 * Order: the core's own state, then every module's reset() in registry
 * order (each must restore ALL of its statics), then the plane hooks are
 * cleared so the camera module re-installs them in prep().
 */
#include "show_core.h"

void show_chunk_clear(void);

void show_purity_reset(void)
{
    int i, n;
    camfx_reset();
    portrait_cache_reset();                 /* portrait.c: drop every cached image */
    beat_set_actor_clock(-1, BCLK_NORMAL);
    show_rng_seed(g_show_shot.seed);
    rng_seed(&doomo_rng, g_show_shot.seed); /* the core's RNG too (resident looks) */
    show_ext_reset();
    g_show_plane_xf_hook = 0;
    g_show_draw_image_hook = 0;
    n = show_module_count();
    for (i = 0; i < n; i++) {
        const ShowModule *m = show_module(i);
        if (m->reset) m->reset();
    }
}
