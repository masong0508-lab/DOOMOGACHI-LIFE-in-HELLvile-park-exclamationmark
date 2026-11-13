/* part_age.c - the age gate for creator parts (CR12; owner: creator / portrait).
 *
 * Every part has an age class in the catalog (core/data/parts/catalog/, PAGE_*): ALL, TEEN+ or
 * ADULT. A minor never wears a part above their stage, whoever made the look:
 *   - the renderer draws look_for_stage() (portrait_age.c), which calls part_age_filter_look();
 *   - the creator hides such parts and swaps them on an Age change (creator.c);
 *   - resident_generate*() and babies (sim_make_baby) filter what they roll or inherit.
 * Only the parts that are not ALL are listed (core/gen/part_age_data.c), so the table is tiny and
 * resident on every platform. Pure, no state (host tests add an override table). */
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/parts.h"
#include "doomo/gene.h"
#include "doomo/look_fields.h"

#if defined(DOOMO_PART_AGE_TEST)
#define AGE_TEST_MAX 8
static PartAgeRow test_rows[AGE_TEST_MAX];
static int test_n;

void part_age_test_set(int list, int value, int age)
{
    int i;
    for (i = 0; i < test_n; i++)
        if (test_rows[i].list == list && test_rows[i].value == value) {
            if (age < 0) test_rows[i] = test_rows[--test_n];
            else test_rows[i].age = (uint8_t)age;
            return;
        }
    if (age >= 0 && test_n < AGE_TEST_MAX) {
        test_rows[test_n].list = (uint16_t)list;
        test_rows[test_n].value = (uint8_t)value;
        test_rows[test_n].age = (uint8_t)age;
        test_n++;
    }
}
#endif

int part_age_class(int list, int value)
{
    int lo = 0, hi = (int)doomo_part_age_count - 1;
#if defined(DOOMO_PART_AGE_TEST)
    int i;
    for (i = 0; i < test_n; i++)
        if (test_rows[i].list == list && test_rows[i].value == value) return test_rows[i].age;
#endif
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const PartAgeRow *r = &doomo_part_age_rows[mid];
        int cmp = r->list != list ? (r->list < list ? -1 : 1) : (r->value != value ? (r->value < value ? -1 : 1) : 0);
        if (cmp == 0) return r->age;
        if (cmp < 0) lo = mid + 1; else hi = mid - 1;
    }
    return PAGE_ALL;
}

int part_age_ok(int list, int value, int stage)
{
    int a = part_age_class(list, value);
    if (a == PAGE_ALL) return 1;
    if (LIFE_IS_ADULT(stage)) return 1;
    return a == PAGE_TEEN && stage == LIFE_TEEN;
}

/* the look's part fields (GF_BODY + i is look byte 1 + i) */
static const uint8_t look_parts[] = {
    GF_BODY, GF_HEAD_SHAPE, GF_EYE_STYLE, GF_BROW_STYLE, GF_NOSE_STYLE, GF_MOUTH_STYLE,
    GF_HAIR_STYLE, GF_FACIAL_HAIR, GF_EARS, GF_HORNS, GF_TAIL, GF_WINGS, GF_HALO, GF_MARKING,
    GF_TOP, GF_BOTTOM, GF_SHOES, GF_HAT, GF_ACCESSORY, GF_AURA, GF_PATTERN
};
/* gene-only part fields (not in DoomoLook yet) */
static const uint8_t gene_parts[] = {
    GF_PUPIL, GF_SCLERA, GF_FUR_PATTERN, GF_GLASSES, GF_OUTER, GF_LEGWEAR, GF_BLUSH_STYLE, GF_EAR_FOLD
};

int part_age_filter_look(DoomoLook *l, int stage)
{
    int i, n = 0;
    if (!l || LIFE_IS_ADULT(stage)) return 0;
    if (!doomo_part_age_count
#if defined(DOOMO_PART_AGE_TEST)
        && !test_n
#endif
        ) return 0;                                     /* nothing is gated: the common case */
    for (i = 0; i < (int)sizeof(look_parts); i++) {
        uint8_t *v = (uint8_t *)l + 1 + look_parts[i];
        if (*v && !part_age_ok(look_parts[i], *v, stage)) { *v = 0; n++; }
    }
    if (!part_age_ok(PL_SPECIES, l->species, stage)) { l->species = 0; n++; }
    return n;
}

int part_age_filter_resident(DoomoResident *r, int stage)
{
    int i, n = 0;
    if (!r) return 0;
    if (stage < 0) stage = RES_STAGE(r);
    if (LIFE_IS_ADULT(stage)) return 0;
    for (i = 0; i < (int)sizeof(look_parts); i++) {
        uint32_t v = gene_get(r->gene, look_parts[i]);
        if (v && !part_age_ok(look_parts[i], (int)v, stage)) { gene_set(r->gene, look_parts[i], 0); n++; }
    }
    for (i = 0; i < (int)sizeof(gene_parts); i++) {
        uint32_t v = gene_get(r->gene, gene_parts[i]);
        if (v && !part_age_ok(gene_parts[i], (int)v, stage)) { gene_set(r->gene, gene_parts[i], 0); n++; }
    }
    if (!part_age_ok(PL_SPECIES, r->species, stage)) { r->species = 0; n++; }
    return n;
}
