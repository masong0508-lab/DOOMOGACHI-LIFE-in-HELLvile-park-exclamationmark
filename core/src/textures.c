/* textures.c - lookup of the generated 64x64 world textures and skies
 * (owner: art).
 *
 * All pixels live in const tables in core/gen/art_data.c (ROM on GBA),
 * produced at build time by core/tools/artgen from core/art/ *.c. This
 * file only maps (id, time) to a frame and hands out its stable, const
 * DoomoImage (art_data.c's doomo_art_tex_img, fixed uids), so the PS1 can
 * cache uploaded frames by uid. (integration, memory budget: the images
 * used to be RAM copies made at init.)
 */
#include "doomo/textures.h"
#include "doomo/palette.h"

static int frame_of(const DoomoArtAnim *a, uint16_t time)
{
    int n = a->frames, sp = a->speed ? a->speed : 1;
    if (n <= 1) return 0;
    switch (a->mode) {
    case DOOMO_ANIM_PINGPONG: {
        int period = 2 * (n - 1), k = (int)((time / (unsigned)sp) % (unsigned)period);
        return k < n ? k : period - k;
    }
    case DOOMO_ANIM_FLASH: {
        /* rest on frame 0; every sp*16 ticks the other frames flash by,
         * two ticks each */
        unsigned cyc = (unsigned)sp * 16u, pos = time % cyc;
        if (pos < (unsigned)(n - 1) * 2u) return 1 + (int)(pos / 2u);
        return 0;
    }
    default:
        return (int)((time / (unsigned)sp) % (unsigned)n);
    }
}

static int valid_tex(int id)
{
    return id > TEX_NONE && id < TEX_COUNT && doomo_art_tex[id].frames > 0;
}

void textures_init(void)
{
    /* nothing left to set up: the images are const (art_data.c) */
}

const uint8_t *texture_pixels(int tex_id, uint16_t time)
{
    const DoomoArtAnim *a;
    if (!valid_tex(tex_id)) tex_id = TEX_GRASS;     /* matches map_cell_safe's wall 1 */
    a = &doomo_art_tex[tex_id];
    return doomo_art_tex_pix[a->first + frame_of(a, time)];
}

const DoomoImage *texture_image(int tex_id, uint16_t time)
{
    const DoomoArtAnim *a;
    if (!valid_tex(tex_id)) tex_id = TEX_GRASS;
    a = &doomo_art_tex[tex_id];
    return &doomo_art_tex_img[a->first + frame_of(a, time)];
}

int texture_is_animated(int tex_id)
{
    return valid_tex(tex_id) && doomo_art_tex[tex_id].frames > 1;
}

int texture_frames(int tex_id)
{
    return valid_tex(tex_id) ? doomo_art_tex[tex_id].frames : 0;
}

uint8_t texture_avg(int tex_id)
{
    if (!valid_tex(tex_id)) return PAL_BLACK;
    return doomo_art_tex[tex_id].avg;
}

const char *texture_name(int tex_id)
{
    if (tex_id < 0 || tex_id >= TEX_COUNT) return "";
    return doomo_art_tex_names[tex_id];
}

const uint8_t *sky_pixels(int sky_id, uint16_t time)
{
    const DoomoArtAnim *a;
    if (sky_id < 0 || sky_id >= SKY_COUNT) sky_id = SKY_PASTEL_DAY;
    a = &doomo_art_sky[sky_id];
    return doomo_art_sky_pix[a->first + frame_of(a, time)];
}

int sky_is_animated(int sky_id)
{
    return sky_id >= 0 && sky_id < SKY_COUNT && doomo_art_sky[sky_id].frames > 1;
}

uint8_t sky_horizon(int sky_id)
{
    if (sky_id < 0 || sky_id >= SKY_COUNT) sky_id = SKY_PASTEL_DAY;
    return doomo_art_sky[sky_id].horizon;
}
