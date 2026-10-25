/* show_game.c - doomo_init / doomo_frame for the show (owner: show; spec 4.1).
 *
 * ps1/src/main.c is linked unchanged: plat_init_hw(); doomo_init(); then per
 * tick hcmd_poll(), pad_read(), plat_begin_frame(), doomo_frame(b),
 * plat_end_frame(). This file stands in for everything game.c and scene.c
 * would define, so neither (nor any scene) is linked:
 *
 *   doomo_init, doomo_frame, doomo_frame_count, doomo_initialized,
 *   doomo_image_new_uid  (the PS1 VRAM image cache keys on it),
 *   scene_set / scene_push / scene_top / scene_by_name as no-ops: hcmd.c's
 *   weak references to them do NOT resolve to NULL at link time (measured,
 *   spec 15.1), so they must exist.
 *
 * WP1 boots to the test chart (show_chart.c). WP2 adds the episode index,
 * the shot loop and the mailbox (show_load.c, show_mbox.c, show_purity.c)
 * behind the same two entry points. Nothing here reads a wall clock: the
 * RNG and every look come from fixed seeds.
 */
#include <stdio.h>
#include "doomo/doomo.h"
#include "doomo/scene.h"
#include "show/show_game.h"

static uint32_t s_frames;
static uint8_t  s_inited;

uint32_t doomo_image_new_uid(void)
{
    /* game.c's scheme: uids below 0x01000000 are free for const images */
    static uint32_t next = 0x01000000u;
    if (++next == 0) next = 0x01000000u;
    return next;
}

uint32_t doomo_frame_count(void) { return s_frames; }
int doomo_initialized(void) { return s_inited; }

/* the four scene entry points hcmd.c references (no scenes in the show) */
void scene_set(const DoomoScene *s, int arg) { (void)s; (void)arg; }
void scene_push(const DoomoScene *s, int arg) { (void)s; (void)arg; }
const DoomoScene *scene_top(void) { return 0; }
const DoomoScene *scene_by_name(const char *name) { (void)name; return 0; }

void doomo_init(void)
{
    printf("show: init (show.exe %s, canvas %dx%d at y %d)\n", SHOW_BUILD_ID,
           SHOW_CANVAS_W, SHOW_CANVAS_H, SHOW_CANVAS_Y);
    palette_init();
    textures_init();
    sprites_init();
    rng_seed(&doomo_rng, SHOW_SEED);
    opt_defaults();
    show_chart_init();
#ifdef SHOW_SELFTEST
    show_selftest_init();
#endif
    s_frames = 0;
    s_inited = 1;
}

void doomo_frame(uint32_t buttons)
{
    (void)buttons;              /* the show is never steered by the pad */
    s_frames++;
#ifdef SHOW_SELFTEST
    if (show_selftest_frame(s_frames))
        return;
#endif
    show_chart_draw(s_frames);
}
