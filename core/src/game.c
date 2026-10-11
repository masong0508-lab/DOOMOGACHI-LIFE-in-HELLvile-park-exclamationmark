/* game.c - the two entry points the platforms call (owner: foundation).
 *
 *   plat_init_hw(); doomo_init();
 *   for (;;) { b = read_pad(); plat_begin_frame(); doomo_frame(b);
 *              plat_end_frame(); }
 */
#include "doomo/doomo.h"

static uint32_t frames;
static uint8_t  inited;

uint32_t doomo_image_new_uid(void)
{
    /* uids below 0x01000000 are free for const images (art, stubs) */
    static uint32_t next = 0x01000000u;
    if (++next == 0) next = 0x01000000u;
    return next;
}

uint32_t doomo_frame_count(void) { return frames; }

int doomo_initialized(void) { return inited; }

void doomo_init(void)
{
    plat_log("doomo: init");
    palette_init();
    /* The light colormaps are the generated art tables (palette.h). A
     * build without them shades wrongly; it says so (the runtime builder,
     * doomo_palette_fallback(), is only linked by bring-up builds, so its
     * 5.6 KB of RAM tables stay out of the game). */
    if (doomo_colormap[DOOMO_LIGHT_LEVELS - 1][PAL_WHITE] != PAL_WHITE ||
        doomo_colormap[DOOMO_LIGHT_LEVELS / 2][PAL_WHITE] == 0)
        plat_log("STUB: doomo_art_colormap (generated art tables missing)");
    textures_init();
    sprites_init();
    audio_init();
    rng_seed(&doomo_rng, plat_entropy());
    input_reset();
    ui_toast_clear();
    opt_defaults();
    town_new(TOWN_FREEPLAY, rng_next(&doomo_rng), TOWN_POP_EMPTY);
    frames = 0;
    inited = 1;
    scene_set(&SCENE_BOOT, 0);
}

void doomo_frame(uint32_t buttons)
{
    frames++;
    input_update(buttons);
    audio_tick();
    scene_update();
    scene_draw();
    ui_toast_draw();
}
