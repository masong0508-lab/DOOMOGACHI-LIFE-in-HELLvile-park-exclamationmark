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
 * horns and wings are the player's choices and are left alone.
 */
#include "portrait_int.h"
#include "doomo/resident.h"

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
}

const DoomoImage *figure_image_stage(const DoomoLook *l, int stage, int status, int expr)
{
    DoomoLook a;
    if (!l) return figure_image(NULL, status, expr);
    look_for_stage(l, stage, &a);
    return figure_image(&a, status, expr);
}
