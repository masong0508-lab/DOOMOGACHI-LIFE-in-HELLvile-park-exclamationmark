/* test_roster.c - the 1000 premade citizens (owner: roster).
 *
 * Validates the compiled roster (core/gen/roster_data.c) against the
 * contract: every record is already sane (resident_sanitize fixes nothing),
 * names / bios / looks are unique, flags and districts are right, the story
 * cast is in place, the relationship graph is valid and loads into a town,
 * and the cast is as diverse as the design asks for (every catalog option
 * used, ~60% critter-folk in Hillvile, ~15% secretly lovely Hellvile folk).
 * tools/roster_build.py --audit runs the same quality checks on the text. */
#include <stdlib.h>
#include "test.h"

static int is_critter(int sp)
{
    return sp == SPECIES_CATFOLK || sp == SPECIES_FOXKIN || sp == SPECIES_BUNNYKIN ||
           sp == SPECIES_BEARKIN || sp == SPECIES_PUPKIN || sp == SPECIES_MOUSEKIN ||
           sp == SPECIES_SHEEPKIN || sp == SPECIES_RACCOONKIN || sp == SPECIES_OWLKIN ||
           sp == SPECIES_FROGLING || sp == SPECIES_MOTHKIN;
}

static int is_demonic(int sp)
{
    return sp == SPECIES_IMP || sp == SPECIES_GHOUL || sp == SPECIES_SKELETON ||
           sp == SPECIES_HELLHOUND || sp == SPECIES_CACOBLOB || sp == SPECIES_BATLING ||
           sp == SPECIES_MUMMY || sp == SPECIES_SLIME || sp == SPECIES_GHOST;
}

/* romance preference of `who` accepts `other`'s pronoun group */
static int accepts(const DoomoResident *who, const DoomoResident *other)
{
    switch (who->romance) {
    case ROMANCE_ANY:  return 1;
    case ROMANCE_NONE: return 0;
    case ROMANCE_SHE:  return other->pronoun == PRONOUN_SHE;
    case ROMANCE_HE:   return other->pronoun == PRONOUN_HE;
    default:
        return other->pronoun == PRONOUN_THEY || other->pronoun == PRONOUN_XE ||
               other->pronoun == PRONOUN_IT;
    }
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int name_cmp_ci(const char *a, const char *b)
{
    int i;
    for (i = 0; i < DOOMO_NAME_LEN; i++) {
        int x = lower((unsigned char)a[i]), y = lower((unsigned char)b[i]);
        if (x != y) return x - y;
        if (!x) return 0;
    }
    return 0;
}

static int printable(const char *s, int max)
{
    int n = 0;
    for (; *s; s++, n++) if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7E) return 0;
    return n > 0 && n <= max;
}

TEST(roster_records_are_sane)
{
    int i;
    CHECK(roster_premade(-1) == NULL);
    CHECK(roster_premade(ROSTER_COUNT) == NULL);
    for (i = 0; i < ROSTER_COUNT; i++) {
        const DoomoResident *r = roster_premade(i);
        DoomoResident copy;
        int k;
        CHECK_MSG(r != NULL, "[%d] missing", i);
        copy = *r;
        CHECK_MSG(resident_sanitize(&copy) == 0, "[%d] %s: sanitize fixed fields", i, r->name);
        CHECK_MSG(memcmp(&copy, r, sizeof(copy)) == 0, "[%d] sanitize changed the record", i);
        CHECK_MSG(r->status == RES_ALIVE, "[%d] not alive", i);
        CHECK(r->exorcism == 0 && r->level == 0 && r->death_cause == DEATH_NONE);
        CHECK_MSG(r->like_food != r->hate_food, "[%d] likes and hates the same food", i);
        CHECK_MSG(r->stat[STAT_CORRUPTION] < 100, "[%d] corruption at possession level", i);
        CHECK_MSG(r->stat[STAT_HEALTH] >= 60 && r->stat[STAT_HUNGER] >= 40,
                  "[%d] %s does not start healthy", i, r->name);
        for (k = 0; k < STAT_COUNT; k++) CHECK(r->stat[k] <= 100);
        CHECK(r->bday_month < 12 && r->bday_day >= 1 && r->bday_day <= 28);
        if (r->look.species == SPECIES_CACOBLOB) CHECK(r->look.eye_count == 1);
        if (r->look.species == SPECIES_OWLKIN) CHECK(r->look.mouth_style == MOUTH_BEAK);
    }
}

TEST(roster_names_are_unique_and_findable)
{
    static int order[ROSTER_COUNT];
    int i, j;
    for (i = 0; i < ROSTER_COUNT; i++) {
        const char *n = roster_premade(i)->name;
        CHECK_MSG(printable(n, DOOMO_NAME_LEN - 1), "[%d] bad name", i);
        CHECK(roster_premade(i)->name[DOOMO_NAME_LEN - 1] == 0);
        CHECK(n[0] != ' ' && n[strlen(n) - 1] != ' ');
        CHECK_STR(roster_name(i), n);
        order[i] = i;
    }
    /* names unique, ignoring case (town_find_by_name is case-insensitive) */
    for (i = 1; i < ROSTER_COUNT; i++) {           /* insertion sort, once */
        int x = order[i];
        for (j = i; j > 0 && name_cmp_ci(roster_name(order[j - 1]), roster_name(x)) > 0; j--)
            order[j] = order[j - 1];
        order[j] = x;
    }
    for (i = 1; i < ROSTER_COUNT; i++)
        CHECK_MSG(name_cmp_ci(roster_name(order[i - 1]), roster_name(order[i])) != 0,
                  "duplicate name %s", roster_name(order[i]));
    for (i = 0; i < ROSTER_COUNT; i += 37) CHECK_EQ(roster_find(roster_name(i)), i);
    CHECK_EQ(roster_find("father fuzz"), ROSTER_ID_FATHER_FUZZ);
    CHECK_EQ(roster_find("LORD VAPULA"), ROSTER_ID_LORD_VAPULA);
    CHECK_EQ(roster_find("Nobody Atall"), -1);
    CHECK_EQ(roster_find(""), -1);
    CHECK_EQ(roster_find(NULL), -1);
    CHECK_STR(roster_name(-5), "");
}

static int look_cmp(const void *a, const void *b)
{
    return memcmp(&roster_premade(*(const int *)a)->look,
                  &roster_premade(*(const int *)b)->look, sizeof(DoomoLook));
}

TEST(roster_looks_are_unique)
{
    static int order[ROSTER_COUNT];
    int i;
    for (i = 0; i < ROSTER_COUNT; i++) order[i] = i;
    qsort(order, ROSTER_COUNT, sizeof(order[0]), look_cmp);
    for (i = 1; i < ROSTER_COUNT; i++)
        CHECK_MSG(look_cmp(&order[i - 1], &order[i]) != 0, "[%d] and [%d] look identical",
                  order[i - 1], order[i]);
}

TEST(roster_bios_and_jobs)
{
    static const char *bios[ROSTER_COUNT];
    int i, j, distinct_jobs = 0;
    for (i = 0; i < ROSTER_COUNT; i++) {
        const char *b = roster_bio(i), *job = roster_job(i);
        CHECK_MSG(printable(b, ROSTER_BIO_MAX), "[%d] bio %s", i, b);
        CHECK_MSG(printable(job, ROSTER_JOB_MAX), "[%d] job %s", i, job);
        CHECK_MSG((b[0] >= 'A' && b[0] <= 'Z') || (b[0] >= '0' && b[0] <= '9') || b[0] == '"',
                  "[%d] bio starts lowercase: %s", i, b);
        bios[i] = b;
        for (j = 0; j < i; j++) {
            if (strcmp(roster_job(j), job) == 0) break;
        }
        if (j == i) distinct_jobs++;
    }
    for (i = 0; i < ROSTER_COUNT; i++)
        for (j = i + 1; j < ROSTER_COUNT; j++)
            CHECK_MSG(strcmp(bios[i], bios[j]) != 0, "[%d] and [%d] share a bio", i, j);
    CHECK_MSG(distinct_jobs >= 250, "only %d different jobs", distinct_jobs);
    CHECK_STR(roster_bio(-1), "");
    CHECK_STR(roster_job(ROSTER_COUNT), "");
}

TEST(roster_flags_and_districts)
{
    int i, story = 0, hellborn_in_hill = 0;
    for (i = 0; i < ROSTER_COUNT; i++) {
        const DoomoResident *r = roster_premade(i);
        CHECK_MSG(!(r->flags & (RF_CUSTOM | RF_PLAYER | RF_FAVORITE | RF_MONSTER_KILL)),
                  "[%d] carries player-only flags", i);
        if (RES_DISTRICT(i) == DISTRICT_HELLVILE) CHECK_MSG(r->flags & RF_HELLBORN, "[%d]", i);
        else if (r->flags & RF_HELLBORN) hellborn_in_hill++;
        if (r->flags & RF_STORY_NPC) story++;
        if (roster_traits(i) & ROSTER_T_SOFTIE) CHECK(r->flags & RF_HELLBORN);
        if (roster_traits(i) & ROSTER_T_KID) CHECK(r->romance == ROMANCE_NONE);
    }
    CHECK_EQ(story, NPC_COUNT);
    CHECK_EQ(hellborn_in_hill, 1);                       /* Rivet, the runaway */
    CHECK_EQ(roster_traits(-1), 0);
}

TEST(roster_story_cast)
{
    static const struct { int npc, id; const char *name; int species, district; } cast[] = {
        { NPC_MAYOR, ROSTER_ID_MAYOR, "Mayor Giggs", SPECIES_CHERUB, DISTRICT_HILLVILE },
        { NPC_FATHER_FUZZ, ROSTER_ID_FATHER_FUZZ, "Father Fuzz", SPECIES_PLUSHIE, DISTRICT_HILLVILE },
        { NPC_RIVET, ROSTER_ID_RIVET, "Rivet", SPECIES_IMP, DISTRICT_HILLVILE },
        { NPC_DR_BLOOM, ROSTER_ID_DR_BLOOM, "Dr. Bloom", SPECIES_GHOUL, DISTRICT_HILLVILE },
        { NPC_NANA_PUDDING, ROSTER_ID_NANA_PUDDING, "Nana Puddin", SPECIES_SHEEPKIN, DISTRICT_HILLVILE },
        { NPC_PIP, ROSTER_ID_PIP, "Pip", SPECIES_BUNNYKIN, DISTRICT_HILLVILE },
        { NPC_VEX, ROSTER_ID_VEX, "Vex", SPECIES_FOXKIN, DISTRICT_HELLVILE },
        { NPC_LORD_VAPULA, ROSTER_ID_LORD_VAPULA, "Lord Vapula", SPECIES_CACOBLOB, DISTRICT_HELLVILE },
    };
    int k;
    for (k = 0; k < DOOMO_COUNTOF(cast); k++) {
        const DoomoResident *r = roster_premade(cast[k].id);
        CHECK_EQ(doomo_story_npc[cast[k].npc], cast[k].id);
        CHECK_EQ(roster_story_npc(cast[k].npc), cast[k].id);
        CHECK_EQ(roster_npc_of(cast[k].id), cast[k].npc);
        CHECK_STR(r->name, cast[k].name);
        CHECK_EQ(r->look.species, cast[k].species);
        CHECK_EQ(RES_DISTRICT(cast[k].id), cast[k].district);
        CHECK(r->flags & RF_STORY_NPC);
    }
    CHECK_EQ(roster_story_npc(-1), -1);
    CHECK_EQ(roster_story_npc(NPC_COUNT), -1);
    CHECK_EQ(roster_npc_of(2), -1);
    /* Rivet ran away from Hellvile: hellborn, but lives in Hillvile */
    CHECK(roster_premade(ROSTER_ID_RIVET)->flags & RF_HELLBORN);
    CHECK(roster_traits(ROSTER_ID_PIP) & ROSTER_T_KID);
    CHECK(roster_traits(ROSTER_ID_FATHER_FUZZ) & ROSTER_T_HOLY);
    CHECK(roster_traits(ROSTER_ID_VEX) & ROSTER_T_INFILTRATOR);
    CHECK(roster_traits(ROSTER_ID_LORD_VAPULA) & ROSTER_T_LOYALIST);
    CHECK(roster_premade(ROSTER_ID_VEX)->stat[STAT_CORRUPTION] >= 80);
    CHECK(roster_premade(ROSTER_ID_MAYOR)->stat[STAT_CORRUPTION] <= 5);
}

TEST(roster_uses_every_catalog_option)
{
    static uint8_t seen[24][64];
    static const char *const lists[24] = {
        "species", "body", "head", "eye", "brow", "nose", "mouth", "ears", "hair", "fhair",
        "horns", "tail", "wings", "halo", "mark", "top", "bottom", "shoes", "hat", "acc",
        "aura", "pattern", "pronoun", "romance" };
    static const int counts[24] = {
        SPECIES_COUNT, BODY_COUNT, HEAD_COUNT, EYE_COUNT, BROW_COUNT, NOSE_COUNT, MOUTH_COUNT,
        EARS_COUNT, HAIR_COUNT, FHAIR_COUNT, HORNS_COUNT, TAIL_COUNT, WINGS_COUNT, HALO_COUNT,
        MARK_COUNT, TOP_COUNT, BOTTOM_COUNT, SHOES_COUNT, HAT_COUNT, ACC_COUNT, AURA_COUNT,
        PATTERN_COUNT, PRONOUN_COUNT, ROMANCE_COUNT };
    static uint8_t food[FOOD_COUNT], hobby[HOBBY_COUNT], music[MUSIC_COUNT], fear[FEAR_COUNT],
        phrase[CATCHPHRASE_COUNT], month[12], top_col[PAL_PAINT_COUNT], hair_col[PAL_PAINT_COUNT],
        eye_col[PAL_PAINT_COUNT];
    int i, k, cheeks = 0;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < ROSTER_COUNT; i++) {
        const DoomoResident *r = roster_premade(i);
        const DoomoLook *l = &r->look;
        const int v[24] = { l->species, l->body, l->head_shape, l->eye_style, l->brow_style,
            l->nose_style, l->mouth_style, l->ears, l->hair_style, l->facial_hair, l->horns,
            l->tail, l->wings, l->halo, l->marking, l->top, l->bottom, l->shoes, l->hat,
            l->accessory, l->aura, l->pattern, r->pronoun, r->romance };
        for (k = 0; k < 24; k++) seen[k][v[k]] = 1;
        food[r->like_food] = hobby[r->like_hobby] = music[r->like_music] = 1;
        fear[r->fear] = phrase[r->catchphrase] = month[r->bday_month] = 1;
        top_col[l->top_color] = hair_col[l->hair_color] = eye_col[l->eye_color] = 1;
        cheeks |= l->cheeks;
    }
    for (k = 0; k < 24; k++)
        for (i = 0; i < counts[k]; i++)
            CHECK_MSG(seen[k][i], "%s option %d is never used", lists[k], i);
    for (i = 0; i < FOOD_COUNT; i++) CHECK_MSG(food[i], "food %d unused", i);
    for (i = 0; i < HOBBY_COUNT; i++) CHECK_MSG(hobby[i], "hobby %d unused", i);
    for (i = 0; i < MUSIC_COUNT; i++) CHECK_MSG(music[i], "music %d unused", i);
    for (i = 0; i < FEAR_COUNT; i++) CHECK_MSG(fear[i], "fear %d unused", i);
    for (i = 0; i < CATCHPHRASE_COUNT; i++) CHECK_MSG(phrase[i], "catchphrase %d unused", i);
    for (i = 0; i < 12; i++) CHECK_MSG(month[i], "nobody born in month %d", i);
    for (i = 0; i < PAL_PAINT_COUNT; i++) {
        CHECK_MSG(top_col[i], "paint %d never worn", i);
        CHECK_MSG(hair_col[i], "paint %d never a hair colour", i);
        CHECK_MSG(eye_col[i], "paint %d never an eye colour", i);
    }
    CHECK_EQ(cheeks, 0xFF);
}

TEST(roster_cast_makeup)
{
    int i, crit = 0, dem = 0, hcrit = 0, soft = 0, they = 0, kids = 0, elders = 0;
    int corr_hill = 0, corr_hell = 0, corr_soft = 0;
    for (i = 0; i < ROSTER_COUNT; i++) {
        const DoomoResident *r = roster_premade(i);
        int t = roster_traits(i);
        if (r->pronoun == PRONOUN_THEY) they++;
        if (t & ROSTER_T_KID) kids++;
        if (t & ROSTER_T_ELDER) elders++;
        if (i < DOOMO_DISTRICT_SIZE) {
            if (is_critter(r->look.species)) crit++;
            corr_hill += r->stat[STAT_CORRUPTION];
        } else {
            if (is_demonic(r->look.species)) dem++;
            if (is_critter(r->look.species)) hcrit++;
            if (t & ROSTER_T_SOFTIE) { soft++; corr_soft += r->stat[STAT_CORRUPTION]; }
            corr_hell += r->stat[STAT_CORRUPTION];
        }
    }
    /* the vaguely furry heart of the cast */
    CHECK_MSG(crit >= 275 && crit <= 350, "Hillvile critter-folk: %d of 500", crit);
    CHECK_MSG(dem >= 250, "Hellvile demonic folk: %d of 500", dem);
    CHECK_MSG(hcrit >= 75, "Hellvile edgy critter-folk: %d", hcrit);
    /* ~15% of the bad side is secretly lovely (Chapter 4) */
    CHECK_MSG(soft >= 60 && soft <= 90, "softies: %d", soft);
    CHECK_EQ(roster_count_traits(ROSTER_T_SOFTIE, DISTRICT_HELLVILE), soft);
    CHECK_EQ(roster_count_traits(ROSTER_T_SOFTIE, -1), soft + 1);      /* + Rivet */
    CHECK_MSG(they >= 100, "they/them: %d", they);
    CHECK(kids >= 40 && elders >= 30);
    CHECK_MSG(corr_hell / 500 >= corr_hill / 500 + 25, "corruption hill %d hell %d",
              corr_hill / 500, corr_hell / 500);
    CHECK(corr_soft / soft < corr_hell / 500);
}

TEST(roster_edges_are_valid)
{
    int i, cross_crush = 0, kinds[REL_KIND_COUNT];
    memset(kinds, 0, sizeof(kinds));
    CHECK_MSG(doomo_roster_edge_count >= 1200 && doomo_roster_edge_count <= DOOMO_MAX_EDGES - 400,
              "%d edges", doomo_roster_edge_count);
    for (i = 0; i < doomo_roster_edge_count; i++) {
        const DoomoEdge *e = &doomo_roster_edges[i];
        const DoomoResident *a, *b;
        int k = e->kind & REL_KIND_MASK;
        CHECK_MSG(e->a < e->b && e->b < ROSTER_COUNT, "edge %d: %d-%d", i, e->a, e->b);
        if (i) {
            const DoomoEdge *p = &doomo_roster_edges[i - 1];
            CHECK_MSG(p->a < e->a || (p->a == e->a && p->b < e->b), "edge %d out of order", i);
        }
        CHECK(k < REL_KIND_COUNT);
        CHECK(!(e->kind & ~(REL_KIND_MASK | REL_CRUSH_AB | REL_CRUSH_BA)));
        kinds[k]++;
        a = roster_premade(e->a);
        b = roster_premade(e->b);
        switch (k) {
        case REL_ACQUAINTANCE: CHECK(e->value >= -24 && e->value <= 24); break;
        case REL_FRIEND:       CHECK(e->value >= 15 && e->value <= 69); break;
        case REL_BEST_FRIEND:  CHECK(e->value >= 70); break;
        case REL_RIVAL:        CHECK(e->value <= -15 && e->value >= -69); break;
        case REL_NEMESIS:      CHECK(e->value <= -70); break;
        case REL_DATING: case REL_MARRIED:
            CHECK_MSG(accepts(a, b) && accepts(b, a), "%s & %s", a->name, b->name);
            CHECK(!(roster_traits(e->a) & ROSTER_T_KID) && !(roster_traits(e->b) & ROSTER_T_KID));
            break;
        case REL_FAMILY:
            CHECK(!(e->kind & (REL_CRUSH_AB | REL_CRUSH_BA)));
            CHECK(e->value >= 0);
            break;
        default: break;
        }
        if (e->kind & REL_CRUSH_AB) CHECK_MSG(accepts(a, b), "%s -> %s", a->name, b->name);
        if (e->kind & REL_CRUSH_BA) CHECK_MSG(accepts(b, a), "%s -> %s", b->name, a->name);
        if ((e->kind & (REL_CRUSH_AB | REL_CRUSH_BA)) &&
            RES_DISTRICT(e->a) != RES_DISTRICT(e->b))
            cross_crush++;
    }
    for (i = 0; i < REL_KIND_COUNT; i++) CHECK_MSG(kinds[i] > 0, "no %s edges", doomo_rel_kind_names[i]);
    CHECK(kinds[REL_FAMILY] >= 100 && kinds[REL_MARRIED] + kinds[REL_DATING] >= 50);
    CHECK_MSG(cross_crush >= 20, "%d cross-border crushes", cross_crush);
}

TEST(roster_moves_into_a_town)
{
    int i;
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    CHECK_EQ(town_count(-1), ROSTER_COUNT);
    for (i = 0; i < ROSTER_COUNT; i++)
        CHECK_MSG(memcmp(town_res(i), roster_premade(i), sizeof(DoomoResident)) == 0,
                  "[%d] differs after town_new", i);
    /* the whole graph survives rel_normalize untouched */
    CHECK_EQ(doomo_town.edge_count, doomo_roster_edge_count);
    CHECK(memcmp(doomo_town.edges, doomo_roster_edges,
                 sizeof(DoomoEdge) * doomo_roster_edge_count) == 0);
    CHECK_EQ(town_find_by_name("nana puddin"), ROSTER_ID_NANA_PUDDING);
    CHECK_EQ(town_count_district(DISTRICT_HELLVILE, RES_ALIVE), DOOMO_DISTRICT_SIZE);
    /* Rivet's family waits for her on the other side of the Wall */
    {
        uint16_t ids[64];
        int n = rel_list(ROSTER_ID_RIVET, ids, 64), k, family_in_hell = 0;
        for (k = 0; k < n; k++)
            if (rel_kind(ROSTER_ID_RIVET, ids[k]) == REL_FAMILY && ids[k] >= DOOMO_DISTRICT_SIZE)
                family_in_hell++;
        CHECK(family_in_hell >= 3);
    }
}
