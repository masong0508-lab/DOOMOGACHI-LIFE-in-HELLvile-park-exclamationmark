/* test_resident.c - generation, validation, fingerprints, catalog and
 * item tables (owner: foundation). */
#include <stdlib.h>
#include "test.h"

static int in_range(const DoomoResident *r)
{
    const DoomoLook *l = &r->look;
    int i;
    if (l->species >= SPECIES_COUNT || l->body >= BODY_COUNT || l->height > 15 ||
        l->build > 15 || l->skin > 39 || l->head_shape >= HEAD_COUNT || l->head_size > 15 ||
        l->eye_style >= EYE_COUNT || l->eye_color > 31 || l->eye_count < 1 ||
        l->eye_count > 3 || l->brow_style >= BROW_COUNT || l->nose_style >= NOSE_COUNT ||
        l->mouth_style >= MOUTH_COUNT || l->fangs > 3 || l->hair_style >= HAIR_COUNT ||
        l->hair_color > 31 || (l->hair_highlight > 31 && l->hair_highlight != LOOK_NO_COLOR) ||
        l->facial_hair >= FHAIR_COUNT || l->ears >= EARS_COUNT || l->horns >= HORNS_COUNT ||
        l->tail >= TAIL_COUNT || l->wings >= WINGS_COUNT || l->halo >= HALO_COUNT ||
        l->marking >= MARK_COUNT || l->top >= TOP_COUNT || l->bottom >= BOTTOM_COUNT ||
        l->shoes >= SHOES_COUNT || l->hat >= HAT_COUNT || l->accessory >= ACC_COUNT ||
        l->aura >= AURA_COUNT || l->pattern >= PATTERN_COUNT)
        return 0;
    for (i = 0; i < STAT_COUNT; i++) if (r->stat[i] > 100) return 0;
    if (r->stat[STAT_CORRUPTION] >= 100) return 0;
    if (!r->name[0] || r->name[DOOMO_NAME_LEN - 1]) return 0;
    if (r->status != RES_ALIVE) return 0;
    if (r->like_food == r->hate_food) return 0;
    return 1;
}

TEST(generate_20000_seeds_valid_and_sane)
{
    static int species_hits[SPECIES_COUNT];
    int seed, critters = 0, hellish = 0;
    memset(species_hits, 0, sizeof(species_hits));
    for (seed = 0; seed < 20000; seed++) {
        DoomoResident r, h, copy;
        resident_generate(&r, (uint32_t)seed);
        copy = r;
        CHECK_MSG(resident_sanitize(&copy) == 0, "seed %d: sanitize fixed fields", seed);
        CHECK_MSG(in_range(&r), "seed %d out of range", seed);
        CHECK(!(r.flags & RF_HELLBORN));
        resident_generate_hellborn(&h, (uint32_t)seed);
        copy = h;
        CHECK_MSG(resident_sanitize(&copy) == 0, "hellborn seed %d: sanitize fixed fields", seed);
        CHECK_MSG(in_range(&h), "hellborn seed %d out of range", seed);
        CHECK(h.flags & RF_HELLBORN);
        species_hits[r.look.species]++;
        if (r.look.species == SPECIES_BUNNYKIN ||
            (r.look.species >= SPECIES_CATFOLK && r.look.species <= SPECIES_OWLKIN)) critters++;
        if (h.look.species == SPECIES_IMP || h.look.species == SPECIES_GHOUL ||
            h.look.species == SPECIES_SKELETON || h.look.species == SPECIES_HELLHOUND) hellish++;
        /* species-coherent critters */
        if (r.look.species == SPECIES_CATFOLK) CHECK(r.look.ears == EARS_CAT && r.look.tail == TAIL_CAT);
        if (r.look.species == SPECIES_FOXKIN) CHECK(r.look.ears == EARS_FOX && r.look.tail == TAIL_FOX);
        if (r.look.species == SPECIES_OWLKIN) CHECK(r.look.mouth_style == MOUTH_BEAK);
        if (r.look.species == SPECIES_CACOBLOB) CHECK(r.look.eye_count == 1);
    }
    /* the vaguely furry heart of the cast: most Hillvile folk are critters */
    CHECK_MSG(critters > 20000 * 55 / 100, "only %d critter-folk", critters);
    CHECK_MSG(hellish > 20000 * 30 / 100, "only %d demonic hellborn", hellish);
    for (seed = 0; seed < SPECIES_COUNT; seed++)
        CHECK_MSG(species_hits[seed] > 0 || seed == SPECIES_SKELETON || seed == SPECIES_HELLHOUND ||
                  seed == SPECIES_CACOBLOB || seed == SPECIES_MUMMY,
                  "species %d never generated", seed);
}

TEST(generate_is_deterministic)
{
    DoomoResident a, b;
    int s;
    for (s = 0; s < 500; s++) {
        resident_generate(&a, (uint32_t)s * 7919u);
        resident_generate(&b, (uint32_t)s * 7919u);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
        CHECK_EQ_U(resident_fingerprint(&a), resident_fingerprint(&b));
    }
    resident_generate(&a, 1);
    resident_generate(&b, 2);
    CHECK(resident_fingerprint(&a) != resident_fingerprint(&b));
}

/* Golden fingerprints: native, GBA and PS1 must agree. The file lists
 * "seed normal hellborn" per line; --update-golden rewrites it. */
TEST(fingerprint_golden_file)
{
    char path[600];
    FILE *f;
    uint32_t seeds[96];
    int n = 0, i;
    for (i = 0; i < 64; i++) seeds[n++] = (uint32_t)i;
    for (i = 0; i < 32; i++) seeds[n++] = 0x9E3779B9u * (uint32_t)(i + 1);
    snprintf(path, sizeof(path), "%s/fingerprints.txt", test_golden_dir);
    if (test_update_golden) {
        f = fopen(path, "w");
        CHECK_MSG(f != NULL, "cannot write %s", path);
        fprintf(f, "# resident_fingerprint(resident_generate(seed)) and (_hellborn(seed))\n"
                   "# seed normal hellborn  (core/tests/test_resident.c; regenerate with\n"
                   "#   $(BUILD)/tests --update-golden fingerprint_golden_file)\n");
        for (i = 0; i < n; i++) {
            DoomoResident a, b;
            resident_generate(&a, seeds[i]);
            resident_generate_hellborn(&b, seeds[i]);
            fprintf(f, "%u %08x %08x\n", seeds[i], resident_fingerprint(&a), resident_fingerprint(&b));
        }
        fclose(f);
        return;
    }
    f = fopen(path, "r");
    CHECK_MSG(f != NULL, "missing %s (run tests --update-golden)", path);
    {
        char line[256];
        int lines = 0;
        while (fgets(line, sizeof(line), f)) {
            unsigned s, fa, fb;
            DoomoResident a, b;
            if (line[0] == '#') continue;
            if (sscanf(line, "%u %x %x", &s, &fa, &fb) != 3) continue;
            resident_generate(&a, s);
            resident_generate_hellborn(&b, s);
            if (resident_fingerprint(&a) != fa || resident_fingerprint(&b) != fb) {
                fclose(f);
                CHECK_MSG(0, "seed %u: fingerprint %08x/%08x, golden %08x/%08x", s,
                          resident_fingerprint(&a), resident_fingerprint(&b), fa, fb);
            }
            lines++;
        }
        fclose(f);
        CHECK_EQ(lines, n);
    }
}

TEST(fingerprint_is_fnv1a)
{
    DoomoResident r;
    uint32_t h = 2166136261u;
    size_t i;
    memset(&r, 0, sizeof(r));
    for (i = 0; i < sizeof(r); i++) h = (h ^ 0) * 16777619u;
    CHECK_EQ_U(resident_fingerprint(&r), h);
}

TEST(sanitize_fixes_junk_and_is_idempotent)
{
    DoomoResident r;
    int fixed, s;
    memset(&r, 0xFF, sizeof(r));
    fixed = resident_sanitize(&r);
    CHECK(fixed > 40);
    CHECK_EQ(resident_sanitize(&r), 0);
    CHECK(r.look.eye_count >= 1 && r.look.eye_count <= 3);
    CHECK(r.look.hair_highlight == LOOK_NO_COLOR);
    CHECK(r.bday_day >= 1 && r.bday_day <= 28);
    /* random garbage over many seeds */
    for (s = 0; s < 2000; s++) {
        DoomoRng g;
        size_t i;
        rng_seed(&g, (uint32_t)s);
        for (i = 0; i < sizeof(r); i++) ((uint8_t *)&r)[i] = (uint8_t)rng_next(&g);
        resident_sanitize(&r);
        CHECK_EQ(resident_sanitize(&r), 0);
        CHECK(r.name[DOOMO_NAME_LEN - 1] == 0);
    }
    /* names: junk truncates, empty becomes "Nameless" */
    memset(&r, 0, sizeof(r));
    resident_default(&r);
    memcpy(r.name, "Bun\001ny", 7);
    CHECK(resident_sanitize(&r) > 0);
    CHECK_STR(r.name, "Bun");
    r.name[0] = 0;
    resident_sanitize(&r);
    CHECK_STR(r.name, "Nameless");
}

TEST(default_and_helpers)
{
    DoomoResident r;
    int i;
    resident_default(&r);
    CHECK_EQ(resident_sanitize(&r), 0);
    CHECK_EQ(r.status, RES_ALIVE);
    CHECK(r.name[0]);
    for (i = 0; i < 1000; i++) {
        resident_generate(&r, (uint32_t)i);
        CHECK(persona_type(&r) >= 0 && persona_type(&r) < PERSONA_TYPE_COUNT);
    }
    CHECK_EQ(look_paint(PAINT_ROSE, 1), PAL_PAINT(PAINT_ROSE, 1));
    CHECK_EQ(look_paint(PAINT_ROSE, 9), PAL_PAINT(PAINT_ROSE, 2));
    CHECK_EQ(look_paint(LOOK_NO_COLOR, 1), 0);
    r.look.skin = SKIN_TAN;
    CHECK_EQ(look_skin(&r.look, 0), PAL_SKIN(SKIN_TAN, 0));
    r.look.skin = 8 + PAINT_MINT;
    CHECK_EQ(look_skin(&r.look, 2), PAL_PAINT(PAINT_MINT, 2));
    /* randomize_look keeps the name & personality */
    resident_generate(&r, 5);
    {
        DoomoResident before = r;
        resident_randomize_look(&r, 77);
        CHECK(memcmp(r.name, before.name, sizeof(r.name)) == 0);
        CHECK(memcmp(r.persona, before.persona, sizeof(r.persona)) == 0);
        CHECK(memcmp(&r.look, &before.look, sizeof(r.look)) != 0);
        CHECK_EQ(resident_sanitize(&r), 0);
    }
}

static int check_table(const char *const *t, int n, int maxlen, const char *what)
{
    int i, j;
    for (i = 0; i < n; i++) {
        if (!t[i] || !t[i][0]) { fprintf(stderr, "    %s[%d] empty\n", what, i); return 0; }
        if ((int)strlen(t[i]) > maxlen) { fprintf(stderr, "    %s[%d] too long\n", what, i); return 0; }
        for (j = 0; t[i][j]; j++)
            if ((unsigned char)t[i][j] < 0x20 || (unsigned char)t[i][j] > 0x9F) {
                fprintf(stderr, "    %s[%d] bad char\n", what, i);
                return 0;
            }
    }
    return 1;
}

TEST(catalog_tables_complete)
{
#define T(tab, n, len) CHECK(check_table(tab, n, len, #tab))
    T(doomo_species_names, SPECIES_COUNT, 14);
    T(doomo_body_names, BODY_COUNT, 14);
    T(doomo_head_names, HEAD_COUNT, 14);
    T(doomo_eye_names, EYE_COUNT, 14);
    T(doomo_brow_names, BROW_COUNT, 14);
    T(doomo_nose_names, NOSE_COUNT, 14);
    T(doomo_mouth_names, MOUTH_COUNT, 14);
    T(doomo_ear_names, EARS_COUNT, 14);
    T(doomo_cheek_names, 8, 14);
    T(doomo_hair_names, HAIR_COUNT, 14);
    T(doomo_fhair_names, FHAIR_COUNT, 14);
    T(doomo_horn_names, HORNS_COUNT, 14);
    T(doomo_tail_names, TAIL_COUNT, 14);
    T(doomo_wing_names, WINGS_COUNT, 14);
    T(doomo_halo_names, HALO_COUNT, 14);
    T(doomo_mark_names, MARK_COUNT, 14);
    T(doomo_top_names, TOP_COUNT, 14);
    T(doomo_bottom_names, BOTTOM_COUNT, 14);
    T(doomo_shoes_names, SHOES_COUNT, 14);
    T(doomo_hat_names, HAT_COUNT, 14);
    T(doomo_acc_names, ACC_COUNT, 14);
    T(doomo_aura_names, AURA_COUNT, 14);
    T(doomo_pattern_names, PATTERN_COUNT, 14);
    T(doomo_pronoun_names, PRONOUN_COUNT, 16);
    T(doomo_romance_names, ROMANCE_COUNT, 16);
    T(doomo_persona_axis_names, PERS_COUNT, 16);
    T(doomo_persona_type_names, PERSONA_TYPE_COUNT, 18);
    T(doomo_voice_names, VOICE_COUNT, 16);
    T(doomo_food_names, FOOD_COUNT, 18);
    T(doomo_hobby_names, HOBBY_COUNT, 18);
    T(doomo_music_names, MUSIC_COUNT, 18);
    T(doomo_fear_names, FEAR_COUNT, 18);
    T(doomo_catchphrases, CATCHPHRASE_COUNT, 26);
    T(doomo_first_names, FIRST_NAME_COUNT, DOOMO_NAME_LEN - 1);
    T(doomo_last_names, LAST_NAME_COUNT, 12);
    T(doomo_month_names, 12, 10);
#undef T
    /* first names are unique */
    {
        int i, j;
        for (i = 0; i < FIRST_NAME_COUNT; i++)
            for (j = i + 1; j < FIRST_NAME_COUNT; j++)
                CHECK_MSG(strcmp(doomo_first_names[i], doomo_first_names[j]) != 0,
                          "duplicate first name %s", doomo_first_names[i]);
    }
    CHECK_STR(pronoun_subj(PRONOUN_SHE), "she");
    CHECK_STR(pronoun_obj(PRONOUN_HE), "him");
    CHECK_STR(pronoun_poss(PRONOUN_THEY), "their");
    CHECK_STR(pronoun_subj(99), "they");
}

TEST(items_table_complete)
{
    int i;
    for (i = 0; i < ITEM_COUNT; i++) {
        const DoomoItemInfo *it = &doomo_items[i];
        CHECK_MSG(it->name && it->name[0], "item %d has no name", i);
        CHECK_MSG(it->desc != NULL, "item %d has no desc", i);
        CHECK_MSG(strlen(it->name) <= 16, "item %d name too long", i);
        CHECK_MSG(i == 0 || strlen(it->desc) > 4, "item %d desc too short", i);
        CHECK(it->icon >= 0x80 && it->icon <= 0x9F);
        CHECK(it->category <= ICAT_KEY);
        if (it->category == ICAT_KEY) CHECK(it->price == 0);
        if (it->category == ICAT_FOOD) CHECK(it->hunger > 0 && it->price > 0);
        if (it->category == ICAT_EXORCISM) CHECK(it->holy >= 15);
    }
    /* edible items share names with the favourite-food table */
    for (i = ITEM_CUPCAKE; i <= ITEM_COFFEE; i++)
        CHECK_MSG(strncmp(doomo_items[i].name, doomo_food_names[i - 1], 4) == 0,
                  "food %d mismatch", i);
}

TEST(species_info_sane)
{
    int i;
    for (i = 0; i < SPECIES_COUNT; i++) {
        const DoomoSpeciesInfo *s = &doomo_species_info[i];
        CHECK(s->default_skin < 40);
        CHECK(s->hair_chance <= 100 && s->horn_chance <= 100 && s->tail_chance <= 100);
        CHECK(s->base_corruption < 100 && s->holy_affinity <= 100);
    }
    CHECK(doomo_species_info[SPECIES_CHERUB].flags & SPF_HOLY);
    CHECK(doomo_species_info[SPECIES_GHOST].flags & SPF_FLOATS);
}
