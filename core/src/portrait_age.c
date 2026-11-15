/* portrait_age.c - body and face forms by life stage (owner: portrait; F4).
 *
 * look_for_stage() turns a resident's saved look into the look to DRAW at a
 * life stage: the saved look never changes, so a baby that grows up gets the
 * same face back, at adult proportions. It only moves the proportion fields
 * the renderer already has (height, build, head size, eye size and spacing,
 * nose and mouth size, fangs, facial hair, blush), so every species, body
 * type and outfit keeps working, and the portrait cache (keyed by the look's
 * hash) holds one image per stage with no other change.
 *
 *   BABY     tiny and round: shortest, chubbiest, the biggest head and eyes,
 *            a dot of a nose and a small mouth, blush, no fangs, no beard
 *   TODDLER  the same, a little less
 *   CHILD    short, big-headed, big-eyed, small nose, blush
 *   TEEN     nearly grown: a little shorter, head and eyes a touch big
 *   ADULT    the saved look, untouched
 *   ELDER    silver hair (white highlight), a little shorter, a bigger nose
 *
 * Pure: no state, no allocation, the same on every platform. Hair, outfit,
 * horns and wings are the player's choices and are left alone, except that a
 * part whose catalog age class is above the stage (TEEN+ on a child, ADULT on
 * any minor; CR12, part_age.c) is drawn as its slot's default.
 */
#include "portrait_int.h"
#include "doomo/resident.h"
#include "doomo/part_meta.h"

/* CR-SLIDERX: the cheat "Slider range" (portrait.h). One row per build slider, in creator_data.c CR_SLIDERS' order:
 * the DoomoLook byte, the slider's default (test_cr_sliderx.c checks both against CR_SLIDERS) and the range the
 * renderers draw safely (every formula in portrait*.c stays positive and inside the picture's own clip: the
 * low ends are where a size reaches 0 or a width would turn negative, the high ends where a part leaves the head). */
#define LS(f, d, lo, hi) { (uint8_t)offsetof(DoomoLook, f), d, lo, hi }
const LookSlider look_sliders[LOOK_SLIDERS] = {
    LS(height, 7, -14, 29), LS(build, 8, -6, 27), LS(head_size, 11, -8, 27), LS(eye_size, 10, -6, 27),
    LS(eye_spacing, 7, -8, 27), LS(eye_height, 8, -10, 27), LS(eye_tilt, 8, -8, 23), LS(brow_height, 7, -10, 22),
    LS(nose_size, 6, -2, 27), LS(mouth_size, 6, -2, 27), LS(horn_size, 7, -10, 27)
};
static uint8_t s_mult = 1;

void look_slider_range_set(int m)
{
    if (m < 1 || m > 4) m = 1;
    if (m == s_mult) return;
    s_mult = (uint8_t)m;
    portrait_slider_range_changed();
}

int look_slider_range_get(void) { return s_mult; }

void look_slider_range(DoomoLook *l)
{
    uint8_t *b = (uint8_t *)l;
    int i;
    if (!l || s_mult <= 1) return;
    for (i = 0; i < LOOK_SLIDERS; i++) {
        const LookSlider *s = &look_sliders[i];
        int v = b[s->off], e;
        if (v > 15) v = 15;
        e = s->def + (v - s->def) * s_mult;
        b[s->off] = (uint8_t)(int8_t)DOOMO_CLAMP(e, s->lo, s->hi);
    }
}

#define AGE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AGE_MAX(a, b) ((a) > (b) ? (a) : (b))

static void young_face_(DoomoLook *o, int eye_min, int nose_max, int mouth_max, int fangs_max)
{
    o->eye_size = (uint8_t)AGE_MAX(o->eye_size, eye_min);
    o->nose_size = (uint8_t)AGE_MIN(o->nose_size, nose_max);
    o->mouth_size = (uint8_t)AGE_MIN(o->mouth_size, mouth_max);
    o->fangs = (uint8_t)AGE_MIN(o->fangs, fangs_max);
    o->facial_hair = FHAIR_NONE;
}

void look_for_stage(const DoomoLook *in, int stage, DoomoLook *out)
{
    if (!in || !out) return;
    *out = *in;
    switch (stage) {
    case LIFE_BABY:
        out->height = 0;
        out->build = (uint8_t)((in->build + 13) / 2);
        out->head_size = 15;
        young_face_(out, 13, 1, 3, 0);
        out->cheeks = (uint8_t)((out->cheeks | CHEEK_BLUSH) & ~CHEEK_SCAR);
        break;
    case LIFE_TODDLER:
        out->height = (uint8_t)AGE_MIN(in->height / 4 + 1, 3);
        out->build = (uint8_t)((in->build + 11) / 2);
        out->head_size = (uint8_t)AGE_MAX(in->head_size, 14);
        young_face_(out, 12, 2, 5, 0);
        out->cheeks = (uint8_t)((out->cheeks | CHEEK_BLUSH) & ~CHEEK_SCAR);
        break;
    case LIFE_CHILD:
        out->height = (uint8_t)(in->height / 4 + 3);          /* 3..6 */
        out->build = (uint8_t)((in->build + 8) / 2);
        out->head_size = (uint8_t)AGE_MAX(in->head_size, 11);
        young_face_(out, 10, 4, 8, 1);
        out->cheeks |= CHEEK_BLUSH;
        break;
    case LIFE_TEEN:
        out->height = (uint8_t)(in->height - in->height / 4 + 1);   /* 1..12 */
        out->head_size = (uint8_t)AGE_MAX(in->head_size, 8);
        out->eye_size = (uint8_t)AGE_MAX(in->eye_size, 8);
        out->facial_hair = FHAIR_NONE;
        out->fangs = (uint8_t)AGE_MIN(in->fangs, 2);
        break;
    case LIFE_ELDER:
        out->hair_color = PAINT_SILVER;
        out->hair_highlight = PAINT_WHITE;
        out->height = (uint8_t)(in->height > 1 ? in->height - 1 : in->height);
        out->nose_size = (uint8_t)AGE_MIN(in->nose_size + 2, 15);
        break;
    default:                      /* ADULT and anything unknown: as saved */
        break;
    }
    /* CR12: a minor is never drawn in a part above their stage (catalog age class), whoever
     * made the look: the part is drawn as the slot's default. A no-op for adults and elders. */
    part_age_filter_look(out, stage);
    if (LIFE_IS_ADULT(stage)) look_slider_range(out);        /* CR-SLIDERX: grown-ups only, never a minor */
}

const DoomoImage *figure_image_stage(const DoomoLook *l, int stage, int status, int expr)
{
    DoomoLook a;
    if (!l) return figure_image(NULL, status, expr);
    look_for_stage(l, stage, &a);
    return figure_image(&a, status, expr);
}
