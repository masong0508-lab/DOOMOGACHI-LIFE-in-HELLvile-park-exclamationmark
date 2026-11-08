/* test_part_meta.c - the creator's part catalog (CR1) and the age gate for parts (CR12).
 *
 * - every live value of every creator list is catalogued under its catalog.c name, and a slot's
 *   live count is the list's *_COUNT;
 * - names are unique in a slot, every part has a set and a tag, values fit their field;
 * - shipped ids are append-only (core/tests/golden/part_ids.txt);
 * - the name index is sorted and search finds by name and by set;
 * - the age gate: a minor never wears (or is drawn in) a part above their stage. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/gene.h"
#include "doomo/portrait.h"

typedef struct NameList { int list; const char *const *names; int count; } NameList;
static const NameList name_lists[] = {
    { PL_SPECIES, doomo_species_names, SPECIES_COUNT }, { GF_BODY, doomo_body_names, BODY_COUNT },
    { GF_HEAD_SHAPE, doomo_head_names, HEAD_COUNT }, { GF_EYE_STYLE, doomo_eye_names, EYE_COUNT },
    { GF_BROW_STYLE, doomo_brow_names, BROW_COUNT }, { GF_NOSE_STYLE, doomo_nose_names, NOSE_COUNT },
    { GF_MOUTH_STYLE, doomo_mouth_names, MOUTH_COUNT }, { GF_EARS, doomo_ear_names, EARS_COUNT },
    { GF_HAIR_STYLE, doomo_hair_names, HAIR_COUNT }, { GF_FACIAL_HAIR, doomo_fhair_names, FHAIR_COUNT },
    { GF_HORNS, doomo_horn_names, HORNS_COUNT }, { GF_TAIL, doomo_tail_names, TAIL_COUNT },
    { GF_WINGS, doomo_wing_names, WINGS_COUNT }, { GF_HALO, doomo_halo_names, HALO_COUNT },
    { GF_MARKING, doomo_mark_names, MARK_COUNT }, { GF_TOP, doomo_top_names, TOP_COUNT },
    { GF_BOTTOM, doomo_bottom_names, BOTTOM_COUNT }, { GF_SHOES, doomo_shoes_names, SHOES_COUNT },
    { GF_HAT, doomo_hat_names, HAT_COUNT }, { GF_ACCESSORY, doomo_acc_names, ACC_COUNT },
    { GF_AURA, doomo_aura_names, AURA_COUNT }, { GF_PATTERN, doomo_pattern_names, PATTERN_COUNT },
};

static int lc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int cmp_ci(const char *a, const char *b)
{
    while (*a && lc(*a) == lc(*b)) { a++; b++; }
    return lc(*a) - lc(*b);
}

TEST(part_catalog_names_every_live_option_like_the_catalog)
{
    int i, v;
    for (i = 0; i < (int)(sizeof(name_lists) / sizeof(name_lists[0])); i++) {
        const NameList *n = &name_lists[i];
        const PartSlotInfo *s = part_slot_of_list(n->list);
        CHECK_MSG(s != NULL, "list %d has no catalog slot", n->list);
        CHECK_MSG(s->live == n->count, "slot %s: %d live parts, the list has %d", s->key, s->live, n->count);
        for (v = 0; v < n->count; v++) {
            const PartMeta *m = part_meta_find(n->list, v);
            CHECK_MSG(m != NULL, "slot %s value %d is not catalogued", s->key, v);
            CHECK_MSG(PMF_STATE(m->flags) == PSTATE_LIVE, "slot %s value %d is not live", s->key, v);
            CHECK_MSG(strcmp(part_meta_name(m), n->names[v]) == 0, "slot %s value %d: \"%s\" vs catalog.c \"%s\"",
                      s->key, v, part_meta_name(m), n->names[v]);
        }
    }
    /* the painted B76 halos are catalogued but not live */
    for (v = HALO_COUNT; v < HALO_ALL_COUNT; v++) {
        const PartMeta *m = part_meta_find(GF_HALO, v);
        CHECK(m && PMF_STATE(m->flags) == PSTATE_HIDDEN);
        CHECK_STR(part_meta_name(m), doomo_halo_names[v]);
    }
}

TEST(part_catalog_rows_are_well_formed)
{
    int s, i, k;
    CHECK(PART_META_COUNT > 400);
    for (s = 0; s < PART_SLOT_COUNT; s++) {
        const PartSlotInfo *sl = &doomo_part_slots[s];
        int live = 0;
        if (s) CHECK(doomo_part_slots[s - 1].list < sl->list);                /* sorted by list id */
        CHECK(part_slot_of_list(sl->list) == sl);
        for (i = sl->first; i < sl->first + sl->count; i++) {
            const PartMeta *m = &doomo_part_meta[i];
            CHECK_EQ(m->slot, s);
            CHECK_EQ(m->list, sl->list);
            CHECK(m->set < PART_SET_COUNT);
            CHECK_MSG(m->tags != 0, "%s %d has no tag", sl->key, m->value);
            CHECK(PMF_AGE(m->flags) <= PAGE_ADULT);
            if (PMF_STATE(m->flags) != PSTATE_TEST) CHECK_MSG(m->value <= sl->maxv, "%s %d does not fit", sl->key, m->value);
            if (m->shape != 0xFF) CHECK(m->shape < sl->nshapes);
            if (i > sl->first) CHECK(doomo_part_meta[i - 1].value < m->value);
            if (PMF_STATE(m->flags) == PSTATE_LIVE) live++;
            for (k = sl->first; k < i; k++)
                CHECK_MSG(cmp_ci(part_meta_name(&doomo_part_meta[k]), part_meta_name(m)) != 0,
                          "%s: name \"%s\" repeats", sl->key, part_meta_name(m));
            CHECK(part_meta_find(m->list, m->value) == m);
        }
        CHECK_EQ(live, sl->live);
        if (PL_IS_DEEP(sl->list)) CHECK(sl->maxv == 127);
    }
    for (i = 1; i < PART_META_COUNT; i++)
        CHECK(cmp_ci(part_meta_name(&doomo_part_meta[doomo_part_name_order[i - 1]]),
                     part_meta_name(&doomo_part_meta[doomo_part_name_order[i]])) <= 0);
    for (i = 0; i < PART_SET_COUNT; i++) {
        CHECK(doomo_part_sets[i].theme < PART_TAG_COUNT);
        CHECK(doomo_part_sets[i].name[0] != 0);
    }
    CHECK(part_meta_find(GF_HAT, 63) == NULL);
    CHECK(part_meta_find(999, 0) == NULL);
    CHECK_STR(part_name_of(GF_HAT, HAT_FEZ), "Fez");
    CHECK_STR(part_name_of(GF_HAT, 63), "");
}

TEST(part_catalog_shipped_ids_are_append_only)
{
    char path[512], line[256];
    FILE *f;
    int n = 0, live = 0, i;
    snprintf(path, sizeof(path), "%s/part_ids.txt", test_golden_dir);
    f = fopen(path, "r");
    CHECK_MSG(f != NULL, "missing %s", path);
    while (fgets(line, sizeof(line), f)) {
        char key[32], name[64];
        int v, s;
        const PartSlotInfo *sl = NULL;
        const PartMeta *m;
        if (line[0] == '#' || line[0] == '\n') continue;
        CHECK(sscanf(line, "%31s %d \"%63[^\"]\"", key, &v, name) == 3);
        for (s = 0; s < PART_SLOT_COUNT; s++)
            if (strcmp(doomo_part_slots[s].key, key) == 0) sl = &doomo_part_slots[s];
        CHECK_MSG(sl != NULL, "shipped slot %s is gone", key);
        m = part_meta_find(sl->list, v);
        CHECK_MSG(m != NULL, "shipped part %s %d was removed", key, v);
        CHECK_MSG(strcmp(part_meta_name(m), name) == 0, "shipped part %s %d renamed", key, v);
        CHECK_MSG(PMF_STATE(m->flags) == PSTATE_LIVE, "shipped part %s %d is no longer live", key, v);
        n++;
    }
    fclose(f);
    for (i = 0; i < PART_META_COUNT; i++) live += PMF_STATE(doomo_part_meta[i].flags) == PSTATE_LIVE;
    CHECK_MSG(n == live, "%d shipped ids, %d live parts: run tools/partcat_build.py --freeze", n, live);
}

TEST(part_search_finds_by_name_and_by_set)
{
    uint16_t out[64];
    const PartSlotInfo *hat = part_slot_of_list(GF_HAT);
    int slot = (int)(hat - doomo_part_slots), n, i, cap = 0, skull = 0;
    n = part_search(slot, "CAP", 1, out, 64);
    for (i = 0; i < n; i++) {
        const char *nm = part_meta_name(&doomo_part_meta[out[i]]);
        cap += strcmp(nm, "Cap") == 0;
        skull += strcmp(nm, "Skull Cap") == 0;
    }
    CHECK(cap == 1 && skull == 1);
    n = part_search(slot, "royal", 1, out, 64);                     /* "Royal Court": crown, tiara */
    CHECK(n >= 2);
    for (i = 0; i < n; i++) CHECK(strcmp(doomo_part_sets[doomo_part_meta[out[i]].set].name, "Royal Court") == 0 ||
                                  strstr(part_meta_name(&doomo_part_meta[out[i]]), "oyal") != NULL);
    CHECK_EQ(part_search(slot, "", 1, NULL, 0), HAT_COUNT);
    CHECK_EQ(part_search(slot, "zzzq", 0, out, 64), 0);
    CHECK(part_search(-1, "", 0, NULL, 0) == PART_META_COUNT);
    CHECK(part_set_size(PSET_ROYAL, 1) >= 2);
}

/* ---- CR12: the age gate --------------------------------------------------------------------- */
TEST(part_age_classes_default_to_all)
{
    int i;
    for (i = 0; i < PART_META_COUNT; i++) {
        const PartMeta *m = &doomo_part_meta[i];
        CHECK_EQ(part_age_class(m->list, m->value), PMF_AGE(m->flags));
        CHECK(part_age_ok(m->list, m->value, LIFE_ADULT));
    }
    CHECK_EQ(part_age_class(GF_HAT, 63), PAGE_ALL);
}

TEST(part_age_gate_follows_the_stage)
{
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, PAGE_ADULT);
    part_age_test_set(GF_ACCESSORY, ACC_NOSE_RING, PAGE_TEEN);
    CHECK(part_age_ok(GF_HAT, HAT_TRAFFIC_CONE, LIFE_ADULT));
    CHECK(part_age_ok(GF_HAT, HAT_TRAFFIC_CONE, LIFE_ELDER));
    CHECK(!part_age_ok(GF_HAT, HAT_TRAFFIC_CONE, LIFE_TEEN));
    CHECK(!part_age_ok(GF_HAT, HAT_TRAFFIC_CONE, LIFE_CHILD));
    CHECK(part_age_ok(GF_ACCESSORY, ACC_NOSE_RING, LIFE_TEEN));
    CHECK(!part_age_ok(GF_ACCESSORY, ACC_NOSE_RING, LIFE_CHILD));
    CHECK(!part_age_ok(GF_ACCESSORY, ACC_NOSE_RING, LIFE_BABY));
    CHECK(part_age_ok(GF_HAT, HAT_CAP, LIFE_BABY));
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, -1);
    part_age_test_set(GF_ACCESSORY, ACC_NOSE_RING, -1);
    CHECK(part_age_ok(GF_HAT, HAT_TRAFFIC_CONE, LIFE_CHILD));
}

TEST(part_age_filter_swaps_a_minor_s_parts_for_the_default)
{
    DoomoResident r;
    DoomoLook l, k;
    resident_default(&r);
    RES_SET_LOOK(&r, GF_HAT, HAT_TRAFFIC_CONE);
    RES_SET_LOOK(&r, GF_ACCESSORY, ACC_NOSE_RING);
    RES_SET_LOOK(&r, GF_TOP, TOP_TSHIRT);
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, PAGE_ADULT);
    part_age_test_set(GF_ACCESSORY, ACC_NOSE_RING, PAGE_TEEN);
    res_get_look(&r, &l);
    k = l;
    CHECK_EQ(part_age_filter_look(&k, LIFE_ADULT), 0);
    CHECK(memcmp(&k, &l, sizeof(k)) == 0);
    k = l;
    CHECK_EQ(part_age_filter_look(&k, LIFE_TEEN), 1);                /* the cone goes, the ring stays */
    CHECK_EQ(k.hat, HAT_NONE);
    CHECK_EQ(k.accessory, ACC_NOSE_RING);
    k = l;
    CHECK_EQ(part_age_filter_look(&k, LIFE_CHILD), 2);
    CHECK_EQ(k.accessory, ACC_NONE);
    CHECK_EQ(k.top, TOP_TSHIRT);                                     /* everything else as it was */
    resident_set_stage(&r, LIFE_TODDLER);
    CHECK_EQ(part_age_filter_resident(&r, -1), 2);
    CHECK_EQ(RES_LOOK(&r, GF_HAT), HAT_NONE);
    CHECK_EQ(RES_LOOK(&r, GF_ACCESSORY), ACC_NONE);
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, -1);
    part_age_test_set(GF_ACCESSORY, ACC_NOSE_RING, -1);
}

TEST(part_age_a_minor_never_renders_an_adult_part)
{
    /* look_for_stage drops the part, so every renderer draws the minor without it: 4,000 random
     * looks, each with the adult-tagged hat forced on, at every minor stage */
    static const int minors[4] = { LIFE_BABY, LIFE_TODDLER, LIFE_CHILD, LIFE_TEEN };
    int i, s, bad = 0;
    part_age_test_set(GF_HAT, HAT_TOP_HAT, PAGE_ADULT);
    part_age_test_set(GF_TOP, TOP_SUIT, PAGE_TEEN);
    for (i = 0; i < 4000; i++) {
        DoomoResident r;
        DoomoLook l, d;
        resident_generate(&r, 0xA9E0000u + (uint32_t)i);
        res_get_look(&r, &l);
        l.hat = HAT_TOP_HAT;
        if (i & 1) l.top = TOP_SUIT;
        for (s = 0; s < 4; s++) {
            look_for_stage(&l, minors[s], &d);
            bad += d.hat == HAT_TOP_HAT;
            bad += minors[s] != LIFE_TEEN && d.top == TOP_SUIT;
            bad += minors[s] == LIFE_TEEN && (i & 1) && d.top != TOP_SUIT;   /* TEEN+ is fine for a teen */
        }
        look_for_stage(&l, LIFE_ADULT, &d);
        bad += d.hat != HAT_TOP_HAT;
    }
    part_age_test_set(GF_HAT, HAT_TOP_HAT, -1);
    part_age_test_set(GF_TOP, TOP_SUIT, -1);
    CHECK_EQ(bad, 0);
}
