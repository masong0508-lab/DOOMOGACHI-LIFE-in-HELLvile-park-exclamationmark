/* exorcism.c - the rules of Father Fuzz's exorcism (owner: life): odds,
 * ritual tools, the final success chance, the true-name puzzle and what
 * success or failure does to the host. The scene is exorcism_scene.c.
 *
 * Failure counts up in the low nibble of DoomoResident.exorcism; at
 * DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL (3) the host becomes RES_SOUL_LOST and
 * only then may be laid to rest (sim_can_lay_to_rest).
 */
#include "sim_int.h"

/* --- S3: authored possessors (SPEC S3 M1, story_final.md sec.1, sec.15.11) ---
 * The weak default table (the rows of story_final.md; S2's textpack.py
 * replaces it with core/gen/possess_table.c, row for row), the armed
 * slots (the 24 B "possessor working row" of the SPEC budget) and the
 * hooks the sim, the VM and the ritual read. */
static const DoomoPossessor exo_rows_default[] = {
    /* 0 Ep 2.5: Steamy / TALLYWORN */
    { "TALLYWORN", { "TALLYWORN", "DIRGELET", "MOPSWORTH", "HAZE" },
      { "a chalk stub", "a tally wall", "a bellows", "a scrap: \"3,652\"" },
      2, 712, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_STANDARD, 0, 0, 0 },
    /* 1 Ep 2.5: Sprinklepop / WHEEZLE, scripted to fail (never offered) */
    { "WHEEZLE", { "DIRGELET", "MOPSWORTH", "CLINKER", "PLODDY" },
      { "purple glitter", "a party hat", "a grape vape", "\"I'm fine!\"" },
      81, 707, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_STANDARD, EXF_SCRIPTED_FAIL | EXF_FIXED_ODDS, 0, 0 },
    /* 2 Ch3: Hattie Haha / HAZE (the prayer is the timing game) */
    { "HAZE", { "HAZE", "CLINKER", "PLODDY", "WHEEZLE" },
      { "a watering can", "a plain-water pen", "an apron", "a wilted tomato" },
      71, 709, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_PRAYER_TIMING, 0, 0, SF_HATTIE_LOST },
    /* 3 Ch3: Grampy Bean / SOOTKIN (the name is the tomato memory game) */
    { "SOOTKIN", { "SOOTKIN", "SHACKLES", "DRUDGE", "CINDERS" },
      { "a seed packet", "a tomato with an eye", "a whistle", "soot on the fence" },
      88, 525, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_NAME_TOMATO, 0, 0, SF_GRAMPY_LOST },
    /* 4 Ch4: Lady Smog / FLUFFY (two-faith rite; VAPULA near, GIGGLESWORTH strikes) */
    { "FLUFFY", { "VAPULA", "GIGGLESWORTH", "FLUFFY", "NIMBUS" },
      { "a crooked crown", "a lost string", "a school bag", "a gravestone" },
      502, EXO_NONE, 0, 0, 0, 1, EXS_TWO_FAITH, EXF_FIXED_ODDS, SF_SMOG_NEAR_MISS, SF_SMOG_LOST },
    /* 5 Ch3 end: the leftover breath / GIGGLESWORTH (no rite: the pet eats it) */
    { "GIGGLESWORTH", { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
      EXO_HOST_SCRIPT, 0, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_STANDARD, EXF_NO_SOUL_LOSS, 0, 0 },
    /* 6 S186: the Feast culprit (names drawn from the guests) */
    { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
      EXO_HOST_SCRIPT, EXO_NONE, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_STANDARD, EXF_GUEST_NAMES | EXF_NO_SOUL_LOSS, 0, 0 },
    /* 7-9 Mr. Huggins the 2nd, 4th and 9th (practice skins) */
    { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
      EXO_HOST_PRACTICE, EXO_NONE, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_PRACTICE, EXF_NO_SOUL_LOSS, 0, 0 },
    { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
      EXO_HOST_PRACTICE, EXO_NONE, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_PRACTICE, EXF_NO_SOUL_LOSS, 0, 0 },
    { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
      EXO_HOST_PRACTICE, EXO_NONE, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_PRACTICE, EXF_NO_SOUL_LOSS, 0, 0 },
    /* 10 S133: Sprinklepop's reverse rite, EXORCISM UNFAILED (type WHEEZLE) */
    { "WHEEZLE", { 0, 0, 0, 0 }, { "a map, upside down", "a heart balloon", "a bell that un-rings", "a kazoo" },
      81, 707, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_REVERSE, EXF_TYPE_NAME | EXF_UNFAIL | EXF_FIXED_ODDS, 0, 0 },
    /* 11 S211: the wisp on the bell rope (a single prompt) */
    { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
      EXO_HOST_SCRIPT, EXO_NONE, 0, 0, EXO_NO_PICK, EXO_NO_PICK, EXS_WISP_PROMPT, EXF_NO_SOUL_LOSS | EXF_FIXED_ODDS, 0, 0 },
};
__attribute__((weak)) const DoomoPossessor *const doomo_possess_table = exo_rows_default;
__attribute__((weak)) const uint16_t doomo_possess_count = DOOMO_COUNTOF(exo_rows_default);

/* the table through a volatile read: a strong possess_table.c may replace
 * the weak defaults above, and GCC would fold them otherwise */
static const DoomoPossessor *exo_table(int *n)
{
    const DoomoPossessor *const volatile *tp = &doomo_possess_table;
    const volatile uint16_t *np = &doomo_possess_count;
    *n = *tp ? (int)*np : 0;
    return *tp;
}

/* Armed slots: host + row (EWRAM 4 x 4 B = 16 B of the SPEC's 24 B). */
#define EXO_SLOTS 4
static struct { uint16_t host; uint8_t row, used; } exo_slot[EXO_SLOTS];

static unsigned exo_host_key(int resident_id)
{
    return resident_id < 0 ? EXO_HOST_PRACTICE : (unsigned)resident_id;
}

int exorcism_authored_row(int resident_id)
{
    int n, i;
    const DoomoPossessor *t = exo_table(&n);
    unsigned key = exo_host_key(resident_id);
    const DoomoResident *r;
    if (!t || !n) return -1;
    /* only the story arms a citizen (S2): a slot left over from a story
     * session never reaches a free-play town */
    if (resident_id >= 0 && doomo_town.mode != TOWN_STORY) return -1;
    for (i = 0; i < EXO_SLOTS; i++) {
        if (!exo_slot[i].used || exo_slot[i].host != key) continue;
        if (resident_id >= 0) {               /* a stale slot (a new game, a load) */
            r = town_res(resident_id);
            if (!r || (r->status != RES_POSSESSED && r->status != RES_SOUL_LOST)) { exo_slot[i].used = 0; continue; }
        }
        if (exo_slot[i].row < n) return exo_slot[i].row;
    }
    /* No slot: the sim's own pick. The slots are not saved; the story
     * re-arms its possessions on every resume (S2: story.c
     * story_rearm_possessors, from the @armed rows), so a natural possession
     * of a story host is never read as an authored one. */
    return -1;
}

void exorcism_slots_clear(void)
{
    memset(exo_slot, 0, sizeof(exo_slot));
}

const DoomoPossessor *exorcism_authored(int resident_id)
{
    int n, row = exorcism_authored_row(resident_id);
    const DoomoPossessor *t = exo_table(&n);
    return row >= 0 ? &t[row] : NULL;
}

static void exo_disarm(unsigned key)
{
    int i;
    for (i = 0; i < EXO_SLOTS; i++)
        if (exo_slot[i].used && exo_slot[i].host == key) exo_slot[i].used = 0;
}

static void exo_arm(unsigned key, int row)
{
    int i;
    exo_disarm(key);
    for (i = 0; i < EXO_SLOTS && exo_slot[i].used; i++) {}
    if (i == EXO_SLOTS) {                         /* full: the oldest goes */
        for (i = 0; i + 1 < EXO_SLOTS; i++) exo_slot[i] = exo_slot[i + 1];
        i = EXO_SLOTS - 1;
    }
    exo_slot[i].host = (uint16_t)key;
    exo_slot[i].row = (uint8_t)row;
    exo_slot[i].used = 1;
}

void exorcism_possess(int resident_id, int row)
{
    int n;
    DoomoResident *r;
    exo_table(&n);
    if (row >= n) row = -1;
    if (resident_id < 0) {                         /* the practice plush */
        if (row >= 0) exo_arm(EXO_HOST_PRACTICE, row); else exo_disarm(EXO_HOST_PRACTICE);
        return;
    }
    r = town_res(resident_id);
    if (!r || resident_id >= DOOMO_MAX_RESIDENTS || r->status == RES_EMPTY) return;
    if (row >= 0) exo_arm((unsigned)resident_id, row); else exo_disarm((unsigned)resident_id);
    if (r->status == RES_ALIVE) {
        r->status = RES_POSSESSED;
        r->stat[STAT_CORRUPTION] = 100;
        sim_recount_possessed();
    }
}

void exorcism_unpossess(int resident_id)
{
    DoomoResident *r;
    exo_disarm(exo_host_key(resident_id));
    if (resident_id < 0) return;
    r = town_res(resident_id);
    if (!r || resident_id >= DOOMO_MAX_RESIDENTS || r->status != RES_POSSESSED) return;
    r->status = RES_ALIVE;
    r->stat[STAT_CORRUPTION] = 10;
    sim_recount_possessed();
}

/* sim_soul.c's pick for a host of an authored row (C13, sec.6.3 hand-off) */
int story_possessor_override(int resident_id)
{
    const DoomoPossessor *p = exorcism_authored(resident_id);
    return p && p->name_res != EXO_NONE ? (int)p->name_res : -1;
}

int exorcism_forced_result(int resident_id)
{
    const DoomoPossessor *p = exorcism_authored(resident_id);
    return p && (p->flags & EXF_SCRIPTED_FAIL) ? 0 : -1;
}
/* --- end S3 --- */

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
    /* --- S3: scripted rites ignore the difficulty and the tools --- */
    {
        const DoomoPossessor *ap = exorcism_authored(resident_id);
        if (ap && (ap->flags & EXF_FIXED_ODDS)) bonus = 0;
        else odds += diff[DOOMO_CLAMP(opt_get(OPT_DIFFICULTY), 0, 3)];
    }
    /* --- end S3 --- */
    odds += DOOMO_MIN(bonus, 40);
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
    const DoomoPossessor *ap = exorcism_authored(resident_id);   /* S3 */
    if (!buf || cap <= 0) return "";
    if (ap && ap->clue[k & 3]) { doomo_snprintf(buf, cap, "%s", ap->clue[k & 3]); return buf; }
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
    const DoomoPossessor *a;
    int att, fails;
    if (!r || resident_id < 0 || resident_id >= DOOMO_MAX_RESIDENTS) return;
    a = exorcism_authored(resident_id);                        /* S3 */
    if (a && (a->flags & EXF_UNFAIL) && r->status == RES_SOUL_LOST) {
        /* the reverse rite: EXORCISM UNFAILED x1 */
        if (success) {
            r->status = RES_ALIVE;
            r->exorcism = (uint8_t)(r->exorcism & 0xF0);
            r->stat[STAT_CORRUPTION] = 10;
            exo_disarm(resident_id);
            sim_recount_possessed();
            sim_on_exorcism(resident_id, 1);
        }
        return;
    }
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
        exo_disarm(resident_id);                               /* S3 */
    } else {
        if (fails < 15) fails++;
        /* --- S3: scripted losses and rites that never cost a soul --- */
        if (a && (a->flags & EXF_SCRIPTED_FAIL)) fails = DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL;
        if (a && (a->flags & EXF_NO_SOUL_LOSS) && fails >= DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL)
            fails = DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL - 1;
        /* --- end S3 --- */
        r->stat[STAT_SANITY] = (uint8_t)DOOMO_MAX(0, r->stat[STAT_SANITY] - 15);
        progress_add(PROG_EXORCISMS_FAILED, 1);
        if (fails >= DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL) {
            r->status = RES_SOUL_LOST;
            if (a && a->lost_flag) story_set_flag(a->lost_flag, 1);   /* S3: losses have consequences */
        }
    }
    r->exorcism = (uint8_t)((att << 4) | fails);
    sim_on_exorcism(resident_id, success);
}

/* --- S3: the board (exorcism.h) --- */
int exorcism_name_answer(int resident_id)
{
    const DoomoPossessor *a = exorcism_authored(resident_id);
    int ids[4], k;
    if (a) {
        if (a->flags & EXF_SCRIPTED_FAIL) return -1;
        if (a->offer[0] && a->name) {
            for (k = 0; k < 4; k++)
                if (a->offer[k] && strcmp(a->offer[k], a->name) == 0) return k;
            return -1;
        }
    }
    return exorcism_name_choices(resident_id, ids);
}

const char *exorcism_name_text(int resident_id, int k, char *buf, int cap)
{
    const DoomoPossessor *a = exorcism_authored(resident_id);
    int ids[4], correct;
    if (!buf || cap <= 0) return "";
    k &= 3;
    if (a && a->offer[k]) { doomo_snprintf(buf, cap, "%s", a->offer[k]); return buf; }
    correct = exorcism_name_choices(resident_id, ids);
    if (a && a->name && k == correct) {             /* the true name is not a citizen */
        doomo_snprintf(buf, cap, "%s", a->name);
        return buf;
    }
    doomo_snprintf(buf, cap, "%s", who_name(ids[k]));
    return buf;
}

int exorcism_name_pick(int resident_id, int k)
{
    const DoomoPossessor *a = exorcism_authored(resident_id);
    if (k == exorcism_name_answer(resident_id)) return EXP_RIGHT;
    if (a && a->near_miss == k) {
        if (a->near_flag) story_set_flag(a->near_flag, 1);
        return EXP_NEAR_MISS;
    }
    if (a && a->herring == k) return EXP_HERRING;
    return EXP_WRONG;
}
/* --- end S3 --- */
