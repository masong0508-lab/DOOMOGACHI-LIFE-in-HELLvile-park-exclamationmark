/* sprites.c - lookup of the generated demon / effect / pickup / prop
 * sprites (owner: art).
 *
 * Pixels are const tables in core/gen/art_data.c (ROM on GBA), generated
 * by core/tools/artgen from core/art/spr_*.c. (integration, memory budget)
 * Each frame is a const DoomoImage there too, with a fixed uid (sprites.h),
 * so platforms that upload images (PS1) can cache them and no RAM copy of
 * the 194 image headers is needed. Every image is centred on its x axis;
 * see SPRF_* in sprites.h for how to anchor it vertically.
 */
#include "doomo/sprites.h"

static const uint8_t empty_pix[4] = {0, 0, 0, 0};
static const DoomoImage empty_img = {2, 2, empty_pix, DOOMO_ART_SPR_UID + 0xFFFFu};

void sprites_init(void)
{
    /* nothing left to set up: the images are const (art_data.c) */
}

const DoomoImage *sprite_image(int spr_id, int frame)
{
    const DoomoArtSprite *s;
    if (spr_id <= SPR_NONE || spr_id >= SPR_COUNT) return &empty_img;
    s = &doomo_art_spr[spr_id];
    if (s->frames == 0) return &empty_img;
    if (frame < 0) frame = 0;
    if (frame >= s->frames) frame = s->frames - 1;
    return &doomo_art_frames[s->first + frame];
}

int sprite_frames(int spr_id)
{
    if (spr_id <= SPR_NONE || spr_id >= SPR_COUNT) return 0;
    return doomo_art_spr[spr_id].frames;
}

int sprite_flags(int spr_id)
{
    if (spr_id <= SPR_NONE || spr_id >= SPR_COUNT) return 0;
    return doomo_art_spr[spr_id].flags;
}

const char *sprite_name(int spr_id)
{
    if (spr_id < 0 || spr_id >= SPR_COUNT) return "";
    return doomo_art_spr_names[spr_id];
}
