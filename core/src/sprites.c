/* sprites.c - lookup of the generated demon / effect / pickup / prop
 * sprites (owner: art).
 *
 * Pixels are const tables in core/gen/art_data.c (ROM on GBA), generated
 * by core/tools/artgen from core/art/spr_*.c. Each frame gets a stable
 * DoomoImage (RAM, 8 bytes + uid) whose uid is assigned once at init from
 * doomo_image_new_uid(), so platforms that upload images (PS1) can cache
 * them. Every image is centred on its x axis; see SPRF_* in sprites.h for
 * how to anchor it vertically.
 */
#include "doomo/sprites.h"

static DoomoImage spr_img[DOOMO_SPR_MAX_IMAGES];
static const uint8_t empty_pix[4] = {0, 0, 0, 0};
static DoomoImage empty_img = {2, 2, empty_pix, 0};
static uint8_t spr_ready;

void sprites_init(void)
{
    int i;
    for (i = 0; i < (int)doomo_art_frame_total && i < DOOMO_SPR_MAX_IMAGES; i++) {
        spr_img[i].w = doomo_art_frames[i].w;
        spr_img[i].h = doomo_art_frames[i].h;
        spr_img[i].pix = doomo_art_frames[i].pix;
        spr_img[i].uid = doomo_image_new_uid();
    }
    empty_img.uid = doomo_image_new_uid();
    spr_ready = 1;
}

const DoomoImage *sprite_image(int spr_id, int frame)
{
    const DoomoArtSprite *s;
    if (!spr_ready) sprites_init();
    if (spr_id <= SPR_NONE || spr_id >= SPR_COUNT) return &empty_img;
    s = &doomo_art_spr[spr_id];
    if (s->frames == 0) return &empty_img;
    if (frame < 0) frame = 0;
    if (frame >= s->frames) frame = s->frames - 1;
    return &spr_img[s->first + frame];
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
