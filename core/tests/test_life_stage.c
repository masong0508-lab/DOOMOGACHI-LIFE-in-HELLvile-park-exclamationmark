/* test_life_stage.c - LS0: the saved life stage and the both-direction
 * romance gate (owner: life). The child-safety blocker:
 *
 *   * the LIFE_* stage lives in bits 4..6 of DoomoResident.romance and is
 *     kept by every setter, sanitize and the creator rows;
 *   * sim_romance_ok / sim_marriage_ok allow adults with adults, teens with
 *     teens (never married), and never a baby or a kid, whichever side asks;
 *   * the PROPERTY: on Empty, Classic and Random islands (seeds 1..3), with
 *     20 creator kids who "crush on anyone" and 20 forced babies, 30 game
 *     days of turbo sim never produce a crush bit, a dating / married edge,
 *     a crush / confession / proposal problem or a romance headline that
 *     pairs a non-adult with anyone but a fellow teen (and no teen ever
 *     marries or is proposed to). Checked after EVERY sim tick. If
 *     core/data/story/kid_cast.txt exists (E5), none of its cast appear in
 *     any romance either;
 *   * the public romance calls refuse such pairs without a headline, and
 *     sim_romance_sweep() undoes illegal romance left by old saves or by
 *     the creator changing someone's age.
 */
#include <ctype.h>
#include <stdlib.h>
#include "test.h"
#include "../src/story_int.h"

static void fast_clock(void)
{
    opt_set(OPT_DAY_LENGTH, 4);          /* 15 minutes per day           */
    opt_set(OPT_SIM_SPEED, 3);           /* turbo                        */
}

/* ---------------------------------------------------------------------- */
/* resident contract                                                        */
TEST(life_stage_bits_and_helpers)
{
    DoomoResident r;
    int s, p;
    resident_default(&r);
    CHECK_EQ(RES_STAGE(&r), LIFE_ADULT);
    CHECK_EQ(RES_ROMANCE(&r), ROMANCE_ANY);
    for (s = 0; s < LIFE_COUNT; s++)
        for (p = 0; p < ROMANCE_COUNT; p++) {
            resident_set_romance(&r, p);
            resident_set_stage(&r, s);
            CHECK_EQ(RES_STAGE(&r), s);
            CHECK_EQ(RES_ROMANCE(&r), p);
            resident_set_romance(&r, (p + 1) % ROMANCE_COUNT);
            CHECK_EQ(RES_STAGE(&r), s);                   /* setters keep the other half */
            resident_set_stage(&r, (s + 1) % LIFE_COUNT);
            CHECK_EQ(RES_ROMANCE(&r), (p + 1) % ROMANCE_COUNT);
            resident_set_stage(&r, s);
            CHECK_EQ(resident_sanitize(&r), 0);           /* every combination is valid */
            CHECK_EQ(RES_STAGE(&r), s);
        }
    CHECK(doomo_life_stage_names[LIFE_CHILD] && strcmp(doomo_life_stage_names[LIFE_CHILD], "Child") == 0);
    /* sanitize: reserved bits cleared, a bad stage fails SAFE (kid), a bad
     * preference becomes ANY; the stage survives */
    r.romance = (uint8_t)(0x80 | 0x08 | (LIFE_TEEN << LIFE_STAGE_SHIFT) | ROMANCE_HE);
    CHECK(resident_sanitize(&r) > 0);
    CHECK_EQ(r.romance, (LIFE_TEEN << LIFE_STAGE_SHIFT) | ROMANCE_HE);
    r.romance = (uint8_t)((7 << LIFE_STAGE_SHIFT) | ROMANCE_SHE);
    CHECK(resident_sanitize(&r) > 0);
    CHECK_EQ(RES_STAGE(&r), LIFE_CHILD);
    CHECK_EQ(RES_ROMANCE(&r), ROMANCE_SHE);
    r.romance = (uint8_t)((LIFE_ELDER << LIFE_STAGE_SHIFT) | 7);
    CHECK(resident_sanitize(&r) > 0);
    CHECK_EQ(RES_STAGE(&r), LIFE_ELDER);
    CHECK_EQ(RES_ROMANCE(&r), ROMANCE_ANY);
    /* generated residents are adults; out-of-range setters clamp */
    resident_generate(&r, 1234);
    CHECK_EQ(RES_STAGE(&r), LIFE_ADULT);
    resident_generate_hellborn(&r, 77);
    CHECK_EQ(RES_STAGE(&r), LIFE_ADULT);
    resident_set_stage(&r, 99);
    CHECK_EQ(RES_STAGE(&r), LIFE_ADULT);
    resident_set_romance(&r, -1);
    CHECK_EQ(RES_ROMANCE(&r), ROMANCE_ANY);
}

TEST(life_stage_pair_rules)
{
    int a, b;
    for (a = 0; a < LIFE_COUNT; a++)
        for (b = 0; b < LIFE_COUNT; b++) {
            int adult = LIFE_IS_ADULT(a) && LIFE_IS_ADULT(b);
            int teens = a == LIFE_TEEN && b == LIFE_TEEN;
            CHECK_EQ(life_romance_pair_ok(a, b), adult || teens);
            CHECK_EQ(life_romance_pair_ok(a, b), life_romance_pair_ok(b, a));
            CHECK_EQ(life_marriage_pair_ok(a, b), adult);
            if (life_stage_youth(a) >= 2) {                 /* baby, toddler, child */
                CHECK(!life_romance_pair_ok(a, b));
                CHECK(!life_marriage_pair_ok(a, b));
            }
        }
    /* the user's stages: newborn, toddler, child, teen, adult, elder; they
     * grow up in that order and ADULT stays 0 (old records are adults) */
    CHECK_EQ(LIFE_ADULT, 0);
    CHECK_EQ(LIFE_COUNT, 6);
    CHECK_EQ(life_stage_next(LIFE_BABY), LIFE_TODDLER);
    CHECK_EQ(life_stage_next(LIFE_TODDLER), LIFE_CHILD);
    CHECK_EQ(life_stage_next(LIFE_CHILD), LIFE_TEEN);
    CHECK_EQ(life_stage_next(LIFE_TEEN), LIFE_ADULT);
    CHECK_EQ(life_stage_next(LIFE_ADULT), LIFE_ELDER);
    CHECK_EQ(life_stage_next(LIFE_ELDER), LIFE_ELDER);
    CHECK_EQ(life_stage_next(99), LIFE_ADULT);
    CHECK(life_stage_youth(LIFE_BABY) > life_stage_youth(LIFE_TODDLER));
    CHECK(life_stage_youth(LIFE_TODDLER) > life_stage_youth(LIFE_CHILD));
    CHECK(life_stage_youth(LIFE_CHILD) > life_stage_youth(LIFE_TEEN));
    CHECK(life_stage_youth(LIFE_TEEN) > life_stage_youth(LIFE_ADULT));
    CHECK_EQ(life_stage_youth(LIFE_ELDER), 0);
    {   /* growing up through every stage only allows romance from TEEN on,
         * and only with a peer of the same stage until ADULT */
        int s = LIFE_BABY, steps = 0;
        while (s != LIFE_ADULT && steps++ < 8) {
            CHECK_EQ(life_romance_pair_ok(s, LIFE_ADULT), 0);
            CHECK_EQ(life_romance_pair_ok(s, s), s == LIFE_TEEN);
            s = life_stage_next(s);
        }
        CHECK_EQ(steps, 4);
    }
    CHECK(!life_romance_pair_ok(-1, LIFE_ADULT));
    CHECK(!life_romance_pair_ok(LIFE_ADULT, LIFE_COUNT));
}

/* ---------------------------------------------------------------------- */
/* the gate on real residents                                               */
static void make_custom(int id, int stage, int pronoun, int romance, uint32_t seed)
{
    DoomoResident *r = town_res(id);
    resident_generate(r, seed);
    r->flags |= RF_CUSTOM;
    r->pronoun = (uint8_t)pronoun;
    resident_set_romance(r, romance);
    resident_set_stage(r, stage);
    doomo_snprintf(r->name, DOOMO_NAME_LEN, "Z%03d", id);
}

TEST(life_stage_gate_both_directions)
{
    int sa, sb;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    for (sa = 0; sa < LIFE_COUNT; sa++)
        for (sb = 0; sb < LIFE_COUNT; sb++) {
            int a = sa * LIFE_COUNT + sb, b = 100 + a;
            int want = life_romance_pair_ok(sa, sb);
            make_custom(a, sa, PRONOUN_SHE, ROMANCE_ANY, 10u + (uint32_t)a);
            make_custom(b, sb, PRONOUN_HE, ROMANCE_ANY, 500u + (uint32_t)a);
            sim_init();
            CHECK_MSG(sim_romance_ok(a, b) == want, "%s -> %s", doomo_life_stage_names[sa],
                      doomo_life_stage_names[sb]);
            CHECK_MSG(sim_romance_ok(b, a) == want, "%s <- %s", doomo_life_stage_names[sa],
                      doomo_life_stage_names[sb]);
            CHECK_EQ(sim_marriage_ok(a, b), life_marriage_pair_ok(sa, sb));
            CHECK_EQ(sim_romance_level(a, b), life_marriage_pair_ok(sa, sb) ? 2 : want);
            CHECK_EQ(sim_romance_level(a, b), sim_romance_level(b, a));
            /* the preference of the asking side still matters */
            resident_set_romance(town_res(a), ROMANCE_SHE);
            CHECK(!sim_romance_ok(a, b));
            resident_set_romance(town_res(a), ROMANCE_NONE);
            CHECK(!sim_romance_ok(a, b));
            resident_set_romance(town_res(a), ROMANCE_ANY);
            /* never family, never the dead or the possessed */
            if (want) {
                rel_set_kind(a, b, REL_FAMILY);
                CHECK(!sim_romance_ok(a, b) && !sim_romance_ok(b, a));
                rel_remove_all(a);
                town_res(b)->status = RES_POSSESSED;
                CHECK(!sim_romance_ok(a, b) && !sim_romance_ok(b, a));
                town_res(b)->status = RES_ALIVE;
            }
        }
}

TEST(life_stage_canon_kids_stay_kids)
{
    DoomoResident *pip;
    int adult = -1, i;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    sim_init();
    pip = town_res(ROSTER_ID_PIP);
    CHECK_EQ(RES_STAGE(pip), LIFE_CHILD);
    CHECK_EQ(sim_life_stage(ROSTER_ID_PIP), LIFE_CHILD);
    for (i = 0; i < DOOMO_DISTRICT_SIZE && adult < 0; i++)
        if (sim_life_stage(i) == LIFE_ADULT && RES_ROMANCE(town_res(i)) == ROMANCE_ANY) adult = i;
    CHECK(adult >= 0);
    /* every premade stage is the roster's */
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) CHECK_EQ(sim_life_stage(i), roster_stage(i));
    /* a makeover (RF_CUSTOM, preference ANY, even a stored "Adult") never
     * ages a canon kid up while they carry their roster name */
    pip->flags |= RF_CUSTOM;
    resident_set_romance(pip, ROMANCE_ANY);
    resident_set_stage(pip, LIFE_ADULT);
    CHECK_EQ(sim_life_stage(ROSTER_ID_PIP), LIFE_CHILD);
    CHECK(!sim_romance_ok(adult, ROSTER_ID_PIP));
    CHECK(!sim_romance_ok(ROSTER_ID_PIP, adult));
    /* ...and a younger stored stage always wins */
    resident_set_stage(town_res(adult), LIFE_CHILD);
    CHECK_EQ(sim_life_stage(adult), LIFE_CHILD);
    resident_set_stage(town_res(adult), LIFE_ADULT);
    /* a renamed slot is somebody new: their saved stage decides */
    memcpy(pip->name, "Grownup", 8);
    CHECK_EQ(sim_life_stage(ROSTER_ID_PIP), LIFE_ADULT);
    resident_set_stage(pip, LIFE_TEEN);
    CHECK_EQ(sim_life_stage(ROSTER_ID_PIP), LIFE_TEEN);
}

/* ---------------------------------------------------------------------- */
/* the property                                                             */
static uint8_t kid_cast[DOOMO_MAX_RESIDENTS];
static int kid_cast_loaded = -1;          /* -1 not tried, 0 missing, 1 read */

/* E5's core/data/story/kid_cast.txt (soft: skipped while it is missing).
 * Lenient format: one entry per line, '#' comments; the first token is a
 * roster id or a premade name (underscores for spaces). */
static void load_kid_cast(void)
{
    char path[512], line[160];
    FILE *f;
    if (kid_cast_loaded >= 0) return;
    memset(kid_cast, 0, sizeof(kid_cast));
    snprintf(path, sizeof(path), "%s/../../data/story/kid_cast.txt", test_golden_dir);
    f = fopen(path, "r");
    kid_cast_loaded = f != NULL;
    if (!f) { printf("    kid_cast.txt not found: the cast check is skipped (E5 adds it)\n"); return; }
    while (fgets(line, sizeof(line), f)) {
        char *p = line, *e;
        int id;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#' || *p == '\n' || *p == '\r') continue;
        for (e = p; *e && *e != '#' && *e != '\t' && *e != '\n' && *e != '\r' && *e != ',' &&
                    !(*e == ' ' && isdigit((unsigned char)*p)); e++) {}
        while (e > p && e[-1] == ' ') e--;
        *e = 0;
        if (isdigit((unsigned char)*p)) id = atoi(p);
        else {
            char *u;
            for (u = p; *u; u++) if (*u == '_') *u = ' ';
            id = roster_find(p);
        }
        if (id >= 0 && id < DOOMO_MAX_RESIDENTS) kid_cast[id] = 1;
    }
    fclose(f);
}

/* the premade citizen `id` is (still) on the kid cast list */
static int is_kid_cast(int id)
{
    if (kid_cast_loaded != 1 || id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    if (doomo_town.population != TOWN_POP_PREMADE || !kid_cast[id]) return 0;
    return strncmp(roster_name(id), town_res(id)->name, DOOMO_NAME_LEN) == 0;
}

typedef struct {
    uint32_t crush_bits, dating, married, teen_pairs, problems, romance_news, news_seen;
} PropSeen;

/* sim_life_stage() and the kid-cast flag of every slot and the player
 * (DOOMO_PLAYER_ID: the hero can be in edges too), read once per island and again whenever somebody is born or renamed (nothing else in
 * the sim changes a stage or a name), so the per-tick scan stays cheap */
static uint8_t st_cache[DOOMO_PLAYER_ID + 1], kc_cache[DOOMO_PLAYER_ID + 1];

static void stage_cache_refresh(void)
{
    int i;
    for (i = 0; i <= DOOMO_PLAYER_ID; i++) {
        st_cache[i] = (uint8_t)sim_life_stage(i);
        kc_cache[i] = (uint8_t)is_kid_cast(i);
    }
}

static int romance_pair_bad(int a, int b, int married)
{
    int sa, sb;
    if (a < 0 || b < 0 || a > DOOMO_PLAYER_ID || b > DOOMO_PLAYER_ID) return 1;
    sa = st_cache[a];
    sb = st_cache[b];
    if (kc_cache[a] || kc_cache[b]) return 1;
    return married ? !life_marriage_pair_ok(sa, sb) : !life_romance_pair_ok(sa, sb);
}

/* Romance headlines (every sim_newsf in sim_social.c that pairs people or
 * names a crush) and whether they announce a wedding. */
static const struct { const char *key; uint8_t wedding; } romance_keys[] = {
    { "crush", 0 }, { "confessed", 0 }, { "New couple", 0 }, { "dating", 0 },
    { "Love over the wall", 0 }, { "turned", 0 }, { "Heartbreak", 0 },
    { "rekindled", 0 }, { "broke up", 0 }, { "It's over for", 0 }, { "split up", 0 },
    { "Trouble in paradise", 0 }, { "Hugs needed", 0 },
    { "proposed", 1 }, { "proposal", 1 }, { "WEDDING", 1 }, { " wed ", 1 }
};

static int word_at(const char *line, const char *name)
{
    size_t n = strlen(name);
    const char *p = line;
    if (n == 0) return 0;
    while ((p = strstr(p, name)) != NULL) {
        int before = p == line ? 0 : isalnum((unsigned char)p[-1]);
        int after = isalnum((unsigned char)p[n]);
        if (!before && !after) return 1;
        p++;
    }
    return 0;
}

static int check_news_line(const char *line, PropSeen *seen, char *why, int cap)
{
    int k, wedding = -1, i, minors = 0, teens = 0, others = 0, kc = 0;
    for (k = 0; k < (int)DOOMO_COUNTOF(romance_keys); k++)
        if (strstr(line, romance_keys[k].key)) { if (wedding < 1) wedding = romance_keys[k].wedding; }
    if (wedding < 0) return 1;
    seen->romance_news++;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        int s;
        if (r->status == RES_EMPTY || !word_at(line, r->name)) continue;
        s = st_cache[i];
        if (life_stage_youth(s) >= 2) minors++;          /* baby, toddler, child */
        else if (s == LIFE_TEEN) teens++;
        else others++;
        if (kc_cache[i]) kc++;
    }
    if (minors || kc || (teens && (others || wedding))) {
        snprintf(why, cap, "romance headline with a minor (%d kids, %d teens, %d adults, %d kid cast): \"%s\"",
                 minors, teens, others, kc, line);
        return 0;
    }
    return 1;
}

static int check_no_minor_romance(PropSeen *seen, char *why, int cap)
{
    int i;
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int k = e->kind & REL_KIND_MASK;
        if (e->kind & (REL_CRUSH_AB | REL_CRUSH_BA)) {
            seen->crush_bits++;
            if (romance_pair_bad(e->a, e->b, 0)) {
                snprintf(why, cap, "crush between %d (%s) and %d (%s)", e->a,
                         doomo_life_stage_names[sim_life_stage(e->a)], e->b,
                         doomo_life_stage_names[sim_life_stage(e->b)]);
                return 0;
            }
        }
        if (k == REL_DATING || k == REL_MARRIED) {
            if (k == REL_DATING) seen->dating++; else seen->married++;
            if (st_cache[e->a] == LIFE_TEEN) seen->teen_pairs++;
            if (romance_pair_bad(e->a, e->b, k == REL_MARRIED)) {
                snprintf(why, cap, "%s edge between %d (%s) and %d (%s)",
                         k == REL_DATING ? "dating" : "married", e->a,
                         doomo_life_stage_names[sim_life_stage(e->a)], e->b,
                         doomo_life_stage_names[sim_life_stage(e->b)]);
                return 0;
            }
        }
    }
    for (i = 0; i < sim_problem_count(); i++) {
        const DoomoProblem *p = sim_problem(i);
        if (!p || (p->kind != PROB_CRUSH && p->kind != PROB_CONFESS_HELP && p->kind != PROB_PROPOSAL))
            continue;
        seen->problems++;
        if (p->other > DOOMO_PLAYER_ID || romance_pair_bad(p->resident, p->other, p->kind == PROB_PROPOSAL)) {
            snprintf(why, cap, "%s problem %d (%s) -> %d", doomo_problem_names[p->kind], p->resident,
                     doomo_life_stage_names[sim_life_stage(p->resident)], p->other);
            return 0;
        }
    }
    return 1;
}

/* the newest `fresh` headlines (the ring holds NEWS_MAX) */
static int check_fresh_news(uint32_t *seen_total, PropSeen *seen, char *why, int cap)
{
    uint32_t total = sim_stats()->news, fresh = total - *seen_total, i;
    *seen_total = total;
    if (fresh > NEWS_MAX) { snprintf(why, cap, "%u headlines in one tick: cannot check them all", fresh); return 0; }
    for (i = 0; i < fresh; i++) {
        seen->news_seen++;
        if (!check_news_line(sim_news((int)i), seen, why, cap)) return 0;
    }
    return 1;
}

/* every living name unique and short, so a headline names exactly who it
 * means (premade islands keep their roster names: the canon identity) */
static void unique_names(int keep_roster)
{
    int i, j;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = &doomo_town.res[i];
        int dup = 0;
        if (r->status == RES_EMPTY) continue;
        if (keep_roster && strncmp(roster_name(i), r->name, DOOMO_NAME_LEN) == 0) {
            for (j = 0; j < i && !dup; j++)
                if (doomo_town.res[j].status != RES_EMPTY &&
                    strncmp(doomo_town.res[j].name, r->name, DOOMO_NAME_LEN) == 0) dup = 1;
            if (!dup) continue;
        }
        if (r->name[0] == 'Z' && atoi(r->name + 1) == i && strlen(r->name) == 4) continue;
        doomo_snprintf(r->name, DOOMO_NAME_LEN, "Z%03d", i);
    }
}

/* the ids of `n` alive adults (ANY preference, unattached) from `from` up */
static int pick_adults(int *out, int n, int from)
{
    int i, k = 0;
    for (i = from; i < DOOMO_MAX_RESIDENTS && k < n; i++) {
        const DoomoResident *r = town_res(i);
        if (r->status != RES_ALIVE || !LIFE_IS_ADULT(sim_life_stage(i))) continue;
        if (RES_ROMANCE(r) != ROMANCE_ANY || sim_partner_of(i) >= 0) continue;
        out[k++] = i;
    }
    return k;
}

static void befriend(int a, int b, int v)
{
    if (a == b || rel_kind(a, b) == REL_FAMILY) return;
    rel_set_kind(a, b, v >= 70 ? REL_BEST_FRIEND : REL_FRIEND);
    rel_adjust(a, b, v);
}

/* 20 creator kids (custom, "crushes on: anyone", made best friends with
 * single adults and each other), 10 creator teens likewise, and 20 forced
 * babies of married adult couples, also adored by the adults around them.
 * Returns how many babies were born. */
static int add_minors(int island, int *kids, int *teens)
{
    int adults[80], na, i, born = 0, slot;
    na = pick_adults(adults, 80, 0);
    /* room: free slots for the newcomers (never one of the picked adults) */
    for (i = 0, slot = DOOMO_DISTRICT_SIZE - 1; i < 50 && slot > 0; slot--) {
        int j, used = 0;
        for (j = 0; j < na; j++) if (adults[j] == slot) used = 1;
        if (used) continue;
        if (town_res(slot)->status != RES_EMPTY) town_free(slot);
        i++;
    }
    for (i = 0; i < 20; i++) {
        int id = town_alloc(i & 1);
        if (id < 0) id = town_alloc(!(i & 1));
        if (id < 0) break;
        make_custom(id, i % 4 == 0 ? LIFE_TODDLER : LIFE_CHILD, i % 3 == 0 ? PRONOUN_SHE : i % 3 == 1 ? PRONOUN_HE : PRONOUN_THEY,
                    ROMANCE_ANY, 9000u + (uint32_t)(island * 100 + i));
        kids[i] = id;
    }
    for (i = 0; i < 10; i++) {
        int id = town_alloc(i & 1);
        if (id < 0) id = town_alloc(!(i & 1));
        if (id < 0) break;
        make_custom(id, LIFE_TEEN, i & 1 ? PRONOUN_SHE : PRONOUN_HE, ROMANCE_ANY,
                    7000u + (uint32_t)(island * 100 + i));
        teens[i] = id;
    }
    /* 20 babies: married adult couples (made married if needed) */
    for (i = 0; i + 1 < na && born < 20; i += 2) {
        int a = adults[i], b = adults[i + 1], baby;
        if (rel_kind(a, b) == REL_FAMILY) continue;
        rel_set_kind(a, b, REL_MARRIED);
        rel_adjust(a, b, 90);
        baby = sim_make_baby(a, b);
        if (baby < 0) break;
        if (RES_STAGE(town_res(baby)) != LIFE_BABY) return -1;
        born++;
    }
    /* the pressure: every minor adored by single adults, and by each other */
    for (i = 0; i < 20; i++) {
        int j;
        for (j = 0; j < 4 && na > 40; j++) befriend(kids[i], adults[40 + (i * 4 + j) % (na - 40)], 95);
        befriend(kids[i], kids[(i + 1) % 20], 95);
        befriend(kids[i], teens[i % 10], 90);
    }
    for (i = 0; i < 10; i++) {
        int j;
        for (j = 0; j < 4 && na > 40; j++) befriend(teens[i], adults[40 + (i * 7 + j) % (na - 40)], 95);
        befriend(teens[i], teens[(i + 1) % 10], 95);
        befriend(teens[i], teens[(i + 3) % 10], 80);
    }
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (town_res(i)->status == RES_ALIVE && sim_life_stage(i) == LIFE_BABY && na > 50)
            befriend(i, adults[50 + i % (na - 50)], 95);
    return born;
}

static void empty_island_people(void)
{
    int i;
    /* 120 single adults, a few elders, all keen */
    for (i = 0; i < 120; i++)
        make_custom(i + (i & 1) * DOOMO_DISTRICT_SIZE, i % 10 == 0 ? LIFE_ELDER : LIFE_ADULT,
                    i % 3 == 0 ? PRONOUN_SHE : i % 3 == 1 ? PRONOUN_HE : PRONOUN_THEY,
                    ROMANCE_ANY, 31000u + (uint32_t)i);
    for (i = 0; i + 2 < 120; i += 2) befriend(i + (i & 1) * DOOMO_DISTRICT_SIZE, i + 2, 85);
}

TEST(life_stage_property_no_minor_romance)
{
    static const struct { int pop; uint32_t seed; const char *name; } islands[] = {
        { TOWN_POP_EMPTY, 11, "Empty" }, { TOWN_POP_PREMADE, 1, "Classic" },
        { TOWN_POP_RANDOM, 1, "Random 1" }, { TOWN_POP_RANDOM, 2, "Random 2" },
        { TOWN_POP_RANDOM, 3, "Random 3" }
    };
    PropSeen all;
    uint32_t adult_crushes = 0, adult_dates = 0, ticks = 0;
    int is;
    char why[256];
    memset(&all, 0, sizeof(all));
    load_kid_cast();
    for (is = 0; is < (int)DOOMO_COUNTOF(islands); is++) {
        int kids[20], teens[10], born;
        uint32_t end, news_total, babies;
        PropSeen seen;
        memset(&seen, 0, sizeof(seen));
        town_new(TOWN_FREEPLAY, islands[is].seed, islands[is].pop);
        if (islands[is].pop == TOWN_POP_EMPTY) empty_island_people();
        fast_clock();
        sim_init();
        memset(kids, 0xFF, sizeof(kids));
        memset(teens, 0xFF, sizeof(teens));
        born = add_minors(is, kids, teens);
        CHECK_MSG(born == 20, "%s: only %d forced babies", islands[is].name, born);
        {
            int i, placed = 1;
            for (i = 0; i < 20; i++) placed &= kids[i] >= 0;
            for (i = 0; i < 10; i++) placed &= teens[i] >= 0;
            CHECK_MSG(placed, "%s: no room for every creator kid and teen", islands[is].name);
            if (!placed) continue;
        }
        unique_names(islands[is].pop == TOWN_POP_PREMADE);
        sim_init();                       /* (the load path: sweep + bitsets) */
        sim_seed(4242u + (uint32_t)is);
        news_total = sim_stats()->news;
        babies = sim_stats()->babies;
        stage_cache_refresh();
        CHECK_MSG(check_no_minor_romance(&seen, why, sizeof(why)), "%s at start: %s",
                  islands[is].name, why);
        end = doomo_town.day + 30;
        while (doomo_town.day < end) {
            sim_tick();
            ticks++;
            if (sim_stats()->babies != babies) {            /* a newborn: name + stage */
                babies = sim_stats()->babies;
                unique_names(islands[is].pop == TOWN_POP_PREMADE);
                stage_cache_refresh();
            }
            CHECK_MSG(check_no_minor_romance(&seen, why, sizeof(why)), "%s day %u: %s",
                      islands[is].name, (unsigned)doomo_town.day, why);
            CHECK_MSG(check_fresh_news(&news_total, &seen, why, sizeof(why)), "%s day %u: %s",
                      islands[is].name, (unsigned)doomo_town.day, why);
        }
        /* the minors are all still there and still minors */
        {
            int i;
            for (i = 0; i < 20; i++)
                CHECK_EQ(sim_life_stage(kids[i]), i % 4 == 0 ? LIFE_TODDLER : LIFE_CHILD);
            for (i = 0; i < 10; i++) CHECK_EQ(sim_life_stage(teens[i]), LIFE_TEEN);
        }
        printf("    %-8s 30 days: crushes %u dates %u weddings %u babies %u | seen: crush bits %u "
               "dating %u married %u teen couples %u romance problems %u romance news %u / %u\n",
               islands[is].name, sim_stats()->crushes, sim_stats()->dates, sim_stats()->marriages,
               sim_stats()->babies, seen.crush_bits, seen.dating, seen.married, seen.teen_pairs,
               seen.problems, seen.romance_news, seen.news_seen);
        adult_crushes += sim_stats()->crushes;
        adult_dates += sim_stats()->dates;
        all.romance_news += seen.romance_news;
        all.news_seen += seen.news_seen;
        all.teen_pairs += seen.teen_pairs;
    }
    printf("    %u ticks checked, %u headlines (%u romance) read, kid cast %s\n", (unsigned)ticks,
           (unsigned)all.news_seen, (unsigned)all.romance_news,
           kid_cast_loaded == 1 ? "checked" : "skipped");
    /* the oracle has teeth: adult romance did happen, and was read */
    CHECK(adult_crushes > 10);
    CHECK(adult_dates > 3);
    CHECK(all.romance_news > 10);
}

/* The checker itself flags every kind of violation (so a green property
 * run means something). */
TEST(life_stage_property_oracle_catches_violations)
{
    PropSeen seen;
    char why[256];
    int kid = 0, adult = 1, teen = 2, adult2 = 3;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    make_custom(kid, LIFE_CHILD, PRONOUN_HE, ROMANCE_ANY, 1);
    make_custom(adult, LIFE_ADULT, PRONOUN_SHE, ROMANCE_ANY, 2);
    make_custom(teen, LIFE_TEEN, PRONOUN_SHE, ROMANCE_ANY, 3);
    make_custom(adult2, LIFE_ADULT, PRONOUN_HE, ROMANCE_ANY, 4);
    sim_init();
    stage_cache_refresh();
    memset(&seen, 0, sizeof(seen));
    CHECK(check_no_minor_romance(&seen, why, sizeof(why)));
    rel_set_crush(adult, kid, 1);
    CHECK(!check_no_minor_romance(&seen, why, sizeof(why)));
    rel_set_crush(adult, kid, 0);
    rel_set_kind(teen, adult, REL_DATING);
    CHECK(!check_no_minor_romance(&seen, why, sizeof(why)));
    rel_set_kind(teen, adult, REL_FRIEND);
    rel_set_kind(adult, adult2, REL_MARRIED);
    CHECK(check_no_minor_romance(&seen, why, sizeof(why)));
    CHECK(sim_problem_raise(kid, PROB_CRUSH, adult, 0) >= 0);
    CHECK(!check_no_minor_romance(&seen, why, sizeof(why)));
    CHECK(!check_news_line("Z001 confessed to Z000... and it's a YES! New couple!", &seen, why, sizeof(why)));
    /* a teen alone in a crush rumour is fine (teens crush on teens) */
    CHECK(check_news_line("Rumour has it someone has a crush on Z002...", &seen, why, sizeof(why)));
    CHECK(!check_news_line("Rumour has it someone has a crush on Z000...", &seen, why, sizeof(why)));
    CHECK(check_news_line("Rumour has it someone has a crush on Z003...", &seen, why, sizeof(why)));
    CHECK(!check_news_line("Z002 proposed to Z003 - and they said YES! Wedding!", &seen, why, sizeof(why)));
    CHECK(check_news_line("Z003 won the pie-eating contest. Again.", &seen, why, sizeof(why)));
}

/* ---------------------------------------------------------------------- */
/* the public calls refuse, quietly                                         */
TEST(life_stage_romance_calls_refuse_minors)
{
    int kid = 10, adult = 11, teen = 12, teen2 = 13, adult2 = 14, baby, i;
    uint32_t news0;
    town_new(TOWN_FREEPLAY, 8, TOWN_POP_EMPTY);
    make_custom(kid, LIFE_CHILD, PRONOUN_HE, ROMANCE_ANY, 1);
    make_custom(adult, LIFE_ADULT, PRONOUN_SHE, ROMANCE_ANY, 2);
    make_custom(teen, LIFE_TEEN, PRONOUN_SHE, ROMANCE_ANY, 3);
    make_custom(teen2, LIFE_TEEN, PRONOUN_HE, ROMANCE_ANY, 4);
    make_custom(adult2, LIFE_ADULT, PRONOUN_HE, ROMANCE_ANY, 5);
    sim_init();
    befriend(kid, adult, 95);
    befriend(teen, adult, 95);
    befriend(teen, teen2, 95);
    news0 = sim_stats()->news;
    for (i = 0; i < 50; i++) {
        CHECK(!sim_confess(kid, adult, 97));
        CHECK(!sim_confess(adult, kid, 97));
        CHECK(!sim_confess(teen, adult, 97));
        CHECK(!sim_confess(adult, teen, 97));
    }
    CHECK_EQ(sim_stats()->news, news0);             /* not even a "turned down" headline */
    CHECK_EQ(sim_stats()->confessions, 0);
    CHECK(rel_kind(kid, adult) != REL_DATING && rel_kind(teen, adult) != REL_DATING);
    /* the player's advice gets a non-romantic brush-off, not an answer */
    CHECK(strstr(sim_advise_confess(kid, adult, 1), "Never mind") != NULL);
    CHECK(strstr(sim_advise_confess(adult, teen, 1), "Never mind") != NULL);
    CHECK(rel_kind(kid, adult) != REL_DATING);
    CHECK_EQ(sim_stats()->news, news0);
    /* teens with teens: romance yes, marriage never */
    rel_adjust(teen, teen2, 100);
    for (i = 0; i < 50 && rel_kind(teen, teen2) != REL_DATING; i++) sim_confess(teen, teen2, 97);
    CHECK_EQ(rel_kind(teen, teen2), REL_DATING);
    for (i = 0; i < 50; i++) CHECK(!sim_propose(teen, teen2, 97));
    CHECK_EQ(rel_kind(teen, teen2), REL_DATING);
    CHECK_EQ(sim_stats()->proposals, 0);
    CHECK(strstr(sim_advise_propose(teen, 1), "too young") != NULL);
    CHECK(!sim_marriage_ok(teen, teen2));
    CHECK_EQ(rel_kind(teen, teen2), REL_DATING);
    /* even an injected dating edge with a kid can't be proposed on */
    rel_set_kind(kid, adult, REL_DATING);
    for (i = 0; i < 50; i++) CHECK(!sim_propose(adult, kid, 97));
    CHECK(rel_kind(kid, adult) != REL_MARRIED);
    /* babies only from two adults */
    CHECK_EQ(sim_make_baby(adult, kid), -1);
    CHECK_EQ(sim_make_baby(teen, teen2), -1);
    rel_set_kind(adult, adult2, REL_MARRIED);
    baby = sim_make_baby(adult, adult2);
    CHECK(baby >= 0);
    CHECK_EQ(RES_STAGE(town_res(baby)), LIFE_BABY);
    CHECK_EQ(RES_ROMANCE(town_res(baby)), ROMANCE_NONE);
    CHECK(!sim_romance_ok(adult2, baby) && !sim_romance_ok(baby, adult2));
    /* the gate needs both alive residents */
    CHECK(!sim_romance_ok(adult, -1) && !sim_romance_ok(-1, adult) && !sim_romance_ok(adult, adult));
    CHECK(!sim_marriage_ok(adult, -1) && !sim_marriage_ok(adult, adult));
    CHECK_EQ(sim_romance_level(-1, adult), 0);
    CHECK_EQ(sim_romance_level(adult, DOOMO_MAX_RESIDENTS + 1), 0);
}

static int has_problem(int id, int kind)
{
    int i = sim_problem_for(id);
    return i >= 0 && sim_problem(i)->kind == kind;
}

TEST(life_stage_sweep_undoes_illegal_romance)
{
    int kid = 20, adult = 21, teen = 22, teen2 = 23, adult2 = 24, adult3 = 25;
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_EMPTY);
    make_custom(kid, LIFE_CHILD, PRONOUN_HE, ROMANCE_ANY, 1);
    make_custom(adult, LIFE_ADULT, PRONOUN_SHE, ROMANCE_ANY, 2);
    make_custom(teen, LIFE_TEEN, PRONOUN_SHE, ROMANCE_ANY, 3);
    make_custom(teen2, LIFE_TEEN, PRONOUN_HE, ROMANCE_ANY, 4);
    make_custom(adult2, LIFE_ADULT, PRONOUN_HE, ROMANCE_ANY, 5);
    make_custom(adult3, LIFE_ADULT, PRONOUN_HE, ROMANCE_ANY, 6);
    sim_init();
    /* what an old save (or a creator age change) can leave behind */
    rel_set_kind(kid, adult, REL_DATING);
    rel_set_crush(adult3, kid, 1);
    rel_set_kind(teen, teen2, REL_MARRIED);
    rel_set_kind(teen, adult3, REL_FRIEND);
    rel_set_crush(teen, adult3, 1);
    rel_set_kind(adult, adult2, REL_FRIEND);
    rel_set_crush(adult, adult2, 1);                 /* legal: stays */
    CHECK(sim_problem_raise(kid, PROB_CRUSH, adult3, 0) >= 0);
    CHECK(sim_problem_raise(teen, PROB_PROPOSAL, teen2, ITEM_RING) >= 0);
    CHECK(sim_romance_sweep(-1) >= 6);
    CHECK_EQ(rel_kind(kid, adult), REL_FRIEND);
    CHECK(!rel_has_crush(adult3, kid));
    CHECK_EQ(rel_kind(teen, teen2), REL_DATING);    /* teens: back to dating */
    CHECK(!rel_has_crush(teen, adult3));
    CHECK(rel_has_crush(adult, adult2));
    CHECK(!has_problem(kid, PROB_CRUSH));
    CHECK(!has_problem(teen, PROB_PROPOSAL));
    CHECK_EQ(sim_partner_of(kid), -1);
    CHECK_EQ(sim_partner_of(adult), -1);
    CHECK_EQ(sim_partner_of(teen), teen2);
    CHECK_EQ(sim_romance_sweep(-1), 0);              /* idempotent */
    /* an adult made a kid later: their marriage is undone by the sweep */
    rel_set_kind(adult, adult2, REL_MARRIED);
    sim_init();
    CHECK_EQ(sim_partner_of(adult), adult2);
    resident_set_stage(town_res(adult2), LIFE_CHILD);
    CHECK(sim_romance_sweep(adult2) >= 1);
    CHECK_EQ(rel_kind(adult, adult2), REL_FRIEND);
    CHECK_EQ(sim_partner_of(adult), -1);
    /* sim_init() (every load) sweeps by itself */
    resident_set_stage(town_res(adult2), LIFE_ADULT);
    rel_set_kind(adult, adult2, REL_DATING);
    resident_set_stage(town_res(adult), LIFE_BABY);
    sim_init();
    CHECK_EQ(rel_kind(adult, adult2), REL_FRIEND);
}

/* Grumbleguts' dating ticker never names a minor, even one whose saved
 * stage says so without the roster (a creator kid in a premade slot). */
TEST(life_stage_grumbleguts_never_flirts_with_minors)
{
    int i, day, slot;
    char line[96];
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    sim_init();
    /* every Hellvile citizen but a handful becomes a creator kid */
    for (i = DOOMO_DISTRICT_SIZE, slot = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        if (i == ROSTER_ID_GRUMBLEGUTS) continue;
        if (++slot % 50 == 0) continue;
        resident_set_stage(town_res(i), LIFE_CHILD);
        town_res(i)->flags |= RF_CUSTOM;
    }
    for (day = 0; day < 200; day++) {
        int who = story_grumble_headline((uint32_t)day, line, sizeof(line));
        if (who >= 0 && who < DOOMO_MAX_RESIDENTS)
            CHECK_MSG(LIFE_IS_ADULT(sim_life_stage(who)), "day %d: %s", day, line);
    }
}
