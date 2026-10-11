/* exorcism.c - the rules of Father Fuzz's exorcism (owner: life): odds,
 * ritual tools, the final success chance, the true-name puzzle and what
 * success or failure does to the host. The scene is exorcism_scene.c.
 *
 * Failure counts up in the low nibble of DoomoResident.exorcism; at
 * DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL (3) the host becomes RES_SOUL_LOST and
 * only then may be laid to rest (sim_can_lay_to_rest).
 */
#include "sim_int.h"

const uint8_t doomo_exorcism_items[EXI_COUNT] = {
    ITEM_HOLY_WATER, ITEM_SAGE_BUNDLE, ITEM_PLUSH_CROSS, ITEM_ROSARY,
    ITEM_BLESSED_SALT, ITEM_HYMN_BOOK
};

const char *const doomo_exorcism_item_help[EXI_COUNT] = {
    "+3 holy water flasks",
    "Wider prayer timing",
    "One less splash needed",
    "Big holy power boost",
    "Slows the demon down",
    "Reveals an extra clue"
};

/* The possessor, also for the practice dummy (id < 0). */
static int possessor(int id)
{
    if (id >= 0 && id < DOOMO_MAX_RESIDENTS) {
        int p = sim_possessor_of(id);
        if (p >= 0) return p;
    }
    return DOOMO_DISTRICT_SIZE + (int)(doomo_hash32(0xF022u ^ doomo_town.seed ^ (uint32_t)id) %
                                       DOOMO_DISTRICT_SIZE);
}

/* The record describing citizen c: who lives there now, else the roster. */
static const DoomoResident *who(int c)
{
    const DoomoResident *r = town_res(c);
    if (r && r->status != RES_EMPTY) return r;
    return roster_premade(c);
}

static const char *who_name(int c)
{
    const DoomoResident *r = who(c);
    return (r && r->name[0]) ? r->name : "Nameless One";
}

/* Is the roster's job/bio about the citizen who lives in slot c now? */
static int roster_matches(int c)
{
    const DoomoResident *r = town_res(c);
    if (!r || r->status == RES_EMPTY) return roster_name(c)[0] != 0;
    if (r->flags & RF_CUSTOM) return 0;
    return strncmp(r->name, roster_name(c), DOOMO_NAME_LEN) == 0;
}

int exorcism_fails(int resident_id)
{
    const DoomoResident *r = town_res(resident_id);
    return r ? RES_EXO_FAILS(r) : 0;
}

int exorcism_tries_left(int resident_id)
{
    int f = exorcism_fails(resident_id);
    return f >= DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL ? 0 : DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL - f;
}

int exorcism_odds_with(int resident_id, unsigned item_mask)
{
    const DoomoResident *r = town_res(resident_id);
    int odds = 30, bonus = 0, i, p, tr;
    static const int8_t diff[4] = { 15, 0, -10, -20 };
    if (r && resident_id >= 0 && resident_id < DOOMO_MAX_RESIDENTS && r->status != RES_EMPTY) {
        odds += r->stat[STAT_SANITY] / 5;
        odds += doomo_species_info[r->look.species % SPECIES_COUNT].holy_affinity / 10;
        odds -= RES_EXO_FAILS(r) * 6;                   /* it digs in deeper */
        if (r->level >= 50) odds += 5;                  /* they trust you    */
    } else {
        odds += 10;                                     /* the practice plush */
    }
    for (i = 0; i < EXI_COUNT; i++)
        if (item_mask & (1u << i)) bonus += doomo_items[doomo_exorcism_items[i]].holy / 2;
    odds += DOOMO_MIN(bonus, 40);
    odds += diff[DOOMO_CLAMP(opt_get(OPT_DIFFICULTY), 0, 3)];
    if (doomo_town.pet.stage == PET_STAGE_ADULT && pet_line() == PET_LINE_ANGEL) odds += 5;
    /* who's in there matters: a softie lets go easily, a loyalist won't */
    p = possessor(resident_id);
    tr = roster_matches(p) ? roster_traits(p) : 0;
    if (tr & ROSTER_T_SOFTIE) odds += 10;
    if (tr & ROSTER_T_LOYALIST) odds -= 10;
    return DOOMO_CLAMP(odds, 5, 95);
}

int exorcism_odds(int resident_id)
{
    return exorcism_odds_with(resident_id, 0);
}

int exorcism_chance(int odds, int rhythm, int water, int name_ok)
{
    int perf, c;
    rhythm = DOOMO_CLAMP(rhythm, 0, 100);
    water = DOOMO_CLAMP(water, 0, 100);
    perf = (rhythm * 35 + water * 30 + (name_ok ? 3500 : 0)) / 100;
    c = (DOOMO_CLAMP(odds, 0, 100) + perf) / 2 + (name_ok ? 10 : -25);
    return DOOMO_CLAMP(c, 3, 97);
}

int exorcism_name_choices(int resident_id, int out[4])
{
    int real = possessor(resident_id), n = 1, tries, correct, i;
    uint32_t h = doomo_hash32((uint32_t)real * 0x9E3779B1u ^ (uint32_t)resident_id ^ 0x7A3Eu);
    int ids[4];
    ids[0] = real;
    for (tries = 0; n < 4 && tries < 64; tries++) {
        int c = DOOMO_DISTRICT_SIZE + (int)((h >> 3) % DOOMO_DISTRICT_SIZE), dup = 0;
        h = doomo_hash32(h + 0x632BE5ABu);
        if (c == resident_id || !who(c) || !who(c)->name[0]) continue;
        for (i = 0; i < n; i++)
            if (ids[i] == c || strncmp(who_name(ids[i]), who_name(c), DOOMO_NAME_LEN) == 0) dup = 1;
        if (!dup) ids[n++] = c;
    }
    while (n < 4) { ids[n] = ids[0] + n < DOOMO_MAX_RESIDENTS ? ids[0] + n : DOOMO_DISTRICT_SIZE + n; n++; }
    correct = (int)(h % 4);
    for (i = 0; i < 4; i++) out[i] = ids[(i - correct + 4) % 4];
    return correct;
}

const char *exorcism_clue(int resident_id, int k, char *buf, int cap)
{
    int p = possessor(resident_id);
    const DoomoResident *r = who(p);
    int rm = roster_matches(p);
    if (!buf || cap <= 0) return "";
    if (!r) { doomo_snprintf(buf, cap, "It smells of grape vapour."); return buf; }
    switch (k & 3) {
    case 0:
        if (rm && roster_job(p)[0]) doomo_snprintf(buf, cap, "Works as: %s", roster_job(p));
        else doomo_snprintf(buf, cap, "Loves %s", doomo_hobby_names[r->like_hobby % HOBBY_COUNT]);
        break;
    case 1:
        doomo_snprintf(buf, cap, "Acts like a %s", doomo_persona_type_names[persona_type(r)]);
        break;
    case 2:
        doomo_snprintf(buf, cap, "Craves %s", doomo_food_names[r->like_food % FOOD_COUNT]);
        break;
    default:
        if (rm && roster_bio(p)[0]) doomo_snprintf(buf, cap, "\"%s\"", roster_bio(p));
        else doomo_snprintf(buf, cap, "Secretly fears %s", doomo_fear_names[r->fear % FEAR_COUNT]);
        break;
    }
    return buf;
}

void exorcism_apply(int resident_id, int success)
{
    DoomoResident *r = town_res(resident_id);
    int att, fails;
    if (!r || resident_id < 0 || resident_id >= DOOMO_MAX_RESIDENTS) return;
    if (r->status != RES_POSSESSED) return;
    sim_ensure_init();
    att = RES_EXO_ATTEMPTS(r);
    fails = RES_EXO_FAILS(r);
    if (att < 15) att++;
    if (success) {
        fails = 0;
        r->status = RES_ALIVE;
        r->stat[STAT_CORRUPTION] = 10;
        r->stat[STAT_SANITY] = (uint8_t)DOOMO_MAX(r->stat[STAT_SANITY] + 25, 60);
        r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + 30);
        r->level = (uint8_t)DOOMO_MIN(99, r->level + 10);
        rel_adjust(resident_id, DOOMO_PLAYER_ID, 20);
        if (doomo_town.exorcisms_won < 0xFFFF) doomo_town.exorcisms_won++;
        progress_add(PROG_EXORCISMS_WON, 1);
    } else {
        if (fails < 15) fails++;
        r->stat[STAT_SANITY] = (uint8_t)DOOMO_MAX(0, r->stat[STAT_SANITY] - 15);
        progress_add(PROG_EXORCISMS_FAILED, 1);
        if (fails >= DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL) r->status = RES_SOUL_LOST;
    }
    r->exorcism = (uint8_t)((att << 4) | fails);
    sim_on_exorcism(resident_id, success);
}
