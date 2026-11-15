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
#include "doomo/options_store.h"

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
/* One loop for both jobs (one body in the PS1's resident bytes). scale = 0: the sanitizer's clamp (portrait.c), so a
 * saved 0..15 look is as it always was and a look drawn under the cheat holds each slider within its safe range;
 * scale = 1: the look to draw for a grown-up, each slider def + (value - def) * m, then the same clamp. */
__attribute__((noinline)) void look_sliders_fix(DoomoLook *l, int scale)   /* noinline: one body, not one per caller */
{
    uint8_t *b = (uint8_t *)l;
    int i, m = opt2_get(OPT_SLIDER_RANGE) + 1;
    if (scale && m == 1) return;
    for (i = 0; i < LOOK_SLIDERS; i++) {
        const LookSlider *s = &look_sliders[i];
        int lo = 0, hi = 15, v = b[s->off];
        if (m > 1) {
            lo = s->lo; hi = s->hi;
            v = scale ? s->def + (DOOMO_MIN(v, 15) - s->def) * m : (int8_t)v;
        }
        b[s->off] = (uint8_t)DOOMO_CLAMP(v, lo, hi);
    }
}

/* the stages work on the saved 0..15 values: zero-extended maths (the fields are int8_t for the cheat: a signed divide or compare costs resident bytes) */
#define UB(x) ((int)(uint8_t)(x))
#define RAWB(f) (*(uint8_t *)&(f))      /* and stores the byte as is: no sign extension */
#define AGE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AGE_MAX(a, b) ((a) > (b) ? (a) : (b))

static void young_face_(DoomoLook *o, int eye_min, int nose_max, int mouth_max, int fangs_max)
{
    RAWB(o->eye_size) = (uint8_t)AGE_MAX(UB(o->eye_size), eye_min);
    RAWB(o->nose_size) = (uint8_t)AGE_MIN(UB(o->nose_size), nose_max);
    RAWB(o->mouth_size) = (uint8_t)AGE_MIN(UB(o->mouth_size), mouth_max);
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
        RAWB(out->build) = (uint8_t)((UB(in->build) + 13) / 2);
        out->head_size = 15;
        young_face_(out, 13, 1, 3, 0);
        out->cheeks = (uint8_t)((out->cheeks | CHEEK_BLUSH) & ~CHEEK_SCAR);
        break;
    case LIFE_TODDLER:
        RAWB(out->height) = (uint8_t)AGE_MIN(UB(in->height) / 4 + 1, 3);
        RAWB(out->build) = (uint8_t)((UB(in->build) + 11) / 2);
        RAWB(out->head_size) = (uint8_t)AGE_MAX(UB(in->head_size), 14);
        young_face_(out, 12, 2, 5, 0);
        out->cheeks = (uint8_t)((out->cheeks | CHEEK_BLUSH) & ~CHEEK_SCAR);
        break;
    case LIFE_CHILD:
        RAWB(out->height) = (uint8_t)(UB(in->height) / 4 + 3);          /* 3..6 */
        RAWB(out->build) = (uint8_t)((UB(in->build) + 8) / 2);
        RAWB(out->head_size) = (uint8_t)AGE_MAX(UB(in->head_size), 11);
        young_face_(out, 10, 4, 8, 1);
        out->cheeks |= CHEEK_BLUSH;
        break;
    case LIFE_TEEN:
        RAWB(out->height) = (uint8_t)(UB(in->height) - UB(in->height) / 4 + 1);   /* 1..12 */
        RAWB(out->head_size) = (uint8_t)AGE_MAX(UB(in->head_size), 8);
        RAWB(out->eye_size) = (uint8_t)AGE_MAX(UB(in->eye_size), 8);
        out->facial_hair = FHAIR_NONE;
        out->fangs = (uint8_t)AGE_MIN(in->fangs, 2);
        break;
    case LIFE_ELDER:
        out->hair_color = PAINT_SILVER;
        out->hair_highlight = PAINT_WHITE;
        RAWB(out->height) = (uint8_t)(UB(in->height) > 1 ? UB(in->height) - 1 : UB(in->height));
        RAWB(out->nose_size) = (uint8_t)AGE_MIN(UB(in->nose_size) + 2, 15);
        break;
    default:                      /* ADULT and anything unknown: as saved */
        break;
    }
    /* CR12: a minor is never drawn in a part above their stage (catalog age class), whoever
     * made the look: the part is drawn as the slot's default. A no-op for adults and elders. */
    part_age_filter_look(out, stage);
    if (LIFE_IS_ADULT(stage)) look_sliders_fix(out, 1);        /* CR-SLIDERX: grown-ups only, never a minor */
}

const DoomoImage *figure_image_stage(const DoomoLook *l, int stage, int status, int expr)
{
    DoomoLook a;
    if (!l) return figure_image(NULL, status, expr);
    look_for_stage(l, stage, &a);
    return figure_image(&a, status, expr);
}
