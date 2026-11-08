/* sim.c - the Tomodachi-style life simulation of Hellvile Park (owner:
 * life): the clock, round-robin resident updates (needs shaped by
 * personality and species, daily routines by hour / weekday / likes /
 * district, moods) and the bounded per-tick budget.
 *
 * Cost model: sim_tick() advances the clock, then updates SIM_BATCH (32)
 * resident slots round robin (a full pass over the 1000 slots takes ~32
 * ticks, about a second) and visits at most SIM_EDGE_BATCH relationship
 * edges (sim_social.c, about one pass over the graph per game hour).
 * Every resident keeps the absolute game minute of its last update, so an
 * update simply applies however many minutes have passed since then:
 * rates are per game hour and are scaled with unbiased stochastic
 * rounding, which keeps the integer math cheap and the result independent
 * of the frame rate, OPT_DAY_LENGTH and OPT_SIM_SPEED.
 */
#include "doomo/playas.h"
#include "doomo/profiles.h"
#include "sim_int.h"
#include "doomo/family.h"
#include "doomo/ending.h"
#include "doomo/life_events.h"

SimState sim_s;

const char *const doomo_mood_names[MOOD_COUNT] = {
    "Okay", "Happy", "Ecstatic", "Sad", "Angry", "Lovesick", "Hungry",
    "Sleepy", "Sick", "Filthy", "Lonely", "Scared", "Corrupted", "Possessed"
};
const char *const doomo_stat_names[8] = {
    "Food", "Joy", "Clean", "Energy", "Health", "Mind", "Dark", "Social"
};
DOOMO_STATIC_ASSERT(STAT_COUNT == 8, stat_names_cover_every_need);
const char *const doomo_activity_names[ACT_COUNT] = {
    "At home", "Sleeping", "At the park", "At the cafe", "Shopping",
    "At church", "At the workshop", "At the beach", "At a concert",
    "At the vape shop", "At the hospital", "Wandering", "Haunting"
};
const char *const doomo_death_names[DEATH_COUNT] = {
    "", "Laid to rest", "Monster Mode", "Demon attack", "Neglect", "Old age"
};

static const char *const venue_names[ACT_COUNT] = {
    "home", "bed", "the park", "Brimstone Bites", "the shop",
    "St. Cuddles", "the workshop", "the beach", "the concert hall",
    "Cloud Nine Vapor", "the hospital", "the streets", "the night"
};

/* ---------------------------------------------------------------------- */
/* small helpers                                                            */
uint32_t sim_now(void)
{
    return doomo_town.day * 1440u + doomo_town.minute;
}

/* 0..n-1 by multiply-shift (no division: the ARM7 has no divider) */
int sim_rand(int n)
{
    if (n <= 1) return 0;
    if (n > 65536) return (int)(rng_next(&sim_s.rng) % (uint32_t)n);
    return (int)(((rng_next(&sim_s.rng) >> 16) * (uint32_t)n) >> 16);
}
uint32_t sim_roll(uint32_t den)
{
    return den > 1 ? rng_next(&sim_s.rng) % den : 0;
}

int sim_chance(int percent)
{
    if (percent <= 0) return 0;
    if (percent >= 100) return 1;
    return sim_rand(100) < percent;
}

/* X4 (SP2): exact division by the needs' constants without a divide. The GBA's ARM7 has no divider and Thumb
 * code has no long multiply, so even `q / 60` with a constant was a library call (gba_udivmod was ~6 ms of a GBA
 * map-entry frame, when 32 residents a tick are brought up to date). Each quotient is a 32-bit multiply and a
 * shift, exact over the stated range (test_sim_x4.c checks every value in it), with the plain division beyond.
 *   q / 60, 120, 240 = (q >> 2, 3, 4) / 15 and q / 360 = (q >> 3) / 45 (floor of a floor), x / 15 =
 *   (x * 34953) >> 19 for x < 74898 and x / 45 = (x * 23302) >> 20 for x < 74898; x / 100 = (x * 5243) >> 19 for
 *   0 <= x < 43699 (truncated toward zero for negatives, as C does). */
#define SIM_DIVQ_MAX (1u << 18)
static inline uint32_t sim_udiv(uint32_t q, uint32_t div)
{
    if (q >= SIM_DIVQ_MAX) return q / div;
    switch (div) {
    case 60:  return ((q >> 2) * 34953u) >> 19;
    case 120: return ((q >> 3) * 34953u) >> 19;
    case 240: return ((q >> 4) * 34953u) >> 19;
    case 360: return ((q >> 3) * 23302u) >> 20;
    default:  return q / div;
    }
}
static inline int sim_div100(int x)
{
    if (x >= 0) return x < 43699 ? (int)(((uint32_t)x * 5243u) >> 19) : x / 100;
    return x > -43699 ? -(int)(((uint32_t)-x * 5243u) >> 19) : x / 100;
}
uint32_t sim_udiv_test(uint32_t q, uint32_t div) { return sim_udiv(q, div); }    /* test_sim_x4.c */
int sim_div100_test(int x) { return sim_div100(x); }

/* round(rate * minutes / div) with unbiased stochastic rounding. Always
 * inlined with a constant div (sim_udiv: a multiply and a shift). A Dither
 * hands out 8 random bits at a time, so one RNG draw serves four stats. */
typedef struct Dither { uint32_t bits; int left; } Dither;

static inline int dither8(Dither *d)
{
    int v;
    if (d->left < 8) { d->bits = rng_next(&sim_s.rng); d->left = 32; }
    v = (int)(d->bits & 255);
    d->bits >>= 8;
    d->left -= 8;
    return v;
}

static inline int dith_div(Dither *dz, int rate, int minutes, int div)
{
    int q, d, rem, neg = rate < 0;
    if (!rate || minutes <= 0) return 0;
    q = (neg ? -rate : rate) * minutes;
    d = (int)sim_udiv((uint32_t)q, (uint32_t)div);
    rem = q - d * div;
    if (rem && ((dither8(dz) * div) >> 8) < rem) d++;
    return neg ? -d : d;
}

static inline int scaled_div(int rate, int minutes, int div)
{
    Dither dz = { 0, 0 };
    return dith_div(&dz, rate, minutes, div);
}

int sim_scaled(int rate, int minutes) { return scaled_div(rate, minutes, 60); }

void sim_stat_add(DoomoResident *r, int stat, int delta)
{
    int v = r->stat[stat] + delta;
    r->stat[stat] = (uint8_t)DOOMO_CLAMP(v, 0, 100);
}

int bit_get(const uint8_t *b, int i)
{
    return (i >= 0 && i < DOOMO_MAX_RESIDENTS) ? (b[i >> 3] >> (i & 7)) & 1 : 0;
}
void bit_set(uint8_t *b, int i, int on)
{
    if (i < 0 || i >= DOOMO_MAX_RESIDENTS) return;
    if (on) b[i >> 3] |= (uint8_t)(1 << (i & 7));
    else b[i >> 3] &= (uint8_t)~(1 << (i & 7));
}

/* id is a premade citizen on a Classic island who still carries their
 * roster name (made over or not) */
static int roster_named(int id)
{
    const char *n;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    if (doomo_town.population != TOWN_POP_PREMADE) return 0;
    n = roster_name(id);
    return n[0] && strncmp(n, doomo_town.res[id].name, DOOMO_NAME_LEN) == 0;
}

int sim_traits(int id)
{
    if (!roster_named(id) || (doomo_town.res[id].flags & RF_CUSTOM)) return 0;
    return roster_traits(id);
}
int sim_life_stage(int id)
{
    const DoomoResident *r = town_res(id);
    int s, rs;
    if (!r) return LIFE_ADULT;
    s = RES_STAGE(r);
    if (s >= LIFE_COUNT) s = LIFE_CHILD;                   /* fail safe   */
    /* a premade citizen who still carries their roster name is still that
     * person: a makeover (RF_CUSTOM) or a saved stage never makes them
     * older than the roster says (a canon kid stays a kid) */
    if (roster_named(id)) {
        rs = roster_stage(id);
        if (life_stage_youth(rs) > life_stage_youth(s)) s = rs;
    }
    return s;
}
int sim_is_kid(int id) { return life_stage_youth(sim_life_stage(id)) >= 2; }   /* baby, toddler, child */
int sim_is_teen(int id) { return sim_life_stage(id) == LIFE_TEEN; }

int sim_alive_ok(const DoomoResident *r)
{
    return r && (r->status == RES_ALIVE || r->status == RES_GHOST);
}

void sim_set_transient(int id, int kind, int hours)
{
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return;
    hours = DOOMO_CLAMP(hours, 0, 15);
    sim_s.transient[id] = (uint8_t)(kind ? (kind << 4) | hours : 0);
}

int sim_transient(int id)
{
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return TR_NONE;
    return (sim_s.transient[id] & 15) ? sim_s.transient[id] >> 4 : TR_NONE;
}

int sim_is_venue(int act)
{
    return act != ACT_HOME && act != ACT_SLEEPING && act != ACT_HOSPITAL &&
           act < ACT_COUNT;
}

const char *sim_name(int id)
{
    const DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY || !r->name[0]) return "Someone";
    return r->name;
}

const char *sim_venue_name(int act)
{
    return (act >= 0 && act < ACT_COUNT) ? venue_names[act] : "town";
}

void sim_seed(uint32_t seed)
{
    sim_ensure_init();
    rng_seed(&sim_s.rng, seed);
}

uint32_t sim_last_tick_ops(void) { return sim_s.ops; }
uint32_t sim_max_tick_ops(void) { return sim_s.max_ops; }
const DoomoSimStats *sim_stats(void) { return &sim_s.stats; }

void sim_date(int *month, int *dom)
{
    uint32_t d = doomo_town.day;
    if (month) *month = (int)((3 + d / 28) % 12);
    if (dom) *dom = (int)(d % 28) + 1;
}

int sim_concert_genre(void)
{
    return (int)(doomo_hash32(doomo_town.seed ^ (doomo_town.day * 0x9E3779B1u)) % MUSIC_COUNT);
}

/* ---------------------------------------------------------------------- */
/* personality & likes                                                      */
int sim_compat(int a, int b)
{
    const DoomoResident *ra = town_res(a), *rb = town_res(b);
    int i, diff = 0, c;
    if (!ra || !rb || ra->status == RES_EMPTY || rb->status == RES_EMPTY) return 0;
    for (i = 0; i < PERS_COUNT; i++) {
        int d = RES_PERSONA(ra, i) - RES_PERSONA(rb, i);
        diff += d < 0 ? -d : d;
    }
    /* 0 (twins) .. 75 (opposites) -> +3 .. -3 */
    c = 3 - diff / 11;
    if (RES_LIKE_HOBBY(ra) == RES_LIKE_HOBBY(rb)) c += 2;
    if (RES_LIKE_MUSIC(ra) == RES_LIKE_MUSIC(rb)) c += 1;
    if (RES_LIKE_FOOD(ra) == RES_LIKE_FOOD(rb)) c += 1;
    if (RES_LIKE_FOOD(ra) == RES_HATE_FOOD(rb) || RES_LIKE_FOOD(rb) == RES_HATE_FOOD(ra)) c -= 1;
    /* two blunt, unhinged folks rub each other the wrong way */
    if (RES_PERSONA(ra, PERS_SPEECH) >= 12 && RES_PERSONA(rb, PERS_SPEECH) >= 12) c -= 1;
    return DOOMO_CLAMP(c, -6, 6);
}

static int pronoun_group(int p)
{
    return p == PRONOUN_SHE ? ROMANCE_SHE : p == PRONOUN_HE ? ROMANCE_HE : ROMANCE_THEY;
}

/* The one romance gate (LS0). The safety half is checked for BOTH sides
 * here, so a caller that asks about one direction only (a one-way crush)
 * can never pair a non-adult with anyone: both alive, and the life stages
 * of both allow it (adults with adults, teens with teens, babies and kids
 * never). Then `from`'s preference must accept `to`'s pronoun (a crush is
 * one-way, so `to`'s own preference only decides the answer to a
 * confession), and family never. */
int sim_romance_ok(int from, int to)
{
    const DoomoResident *a = town_res(from), *b = town_res(to);
    int pa;
    if (!a || !b || from == to) return 0;
    if (a->status != RES_ALIVE || b->status != RES_ALIVE) return 0;
    if (!life_romance_pair_ok(sim_life_stage(from), sim_life_stage(to))) return 0;
    pa = RES_ROMANCE(a);
    if (pa == ROMANCE_NONE) return 0;
    if (pa != ROMANCE_ANY && pa != pronoun_group(RES_PRONOUN(b))) return 0;
    if (rel_kind(from, to) == REL_FAMILY) return 0;
    return 1;
}

int sim_romance_level(int a, int b)
{
    int sa, sb;
    if (!town_res(a) || !town_res(b) || a == b || rel_kind(a, b) == REL_FAMILY) return 0;
    sa = sim_life_stage(a);
    sb = sim_life_stage(b);
    return life_marriage_pair_ok(sa, sb) ? 2 : life_romance_pair_ok(sa, sb);
}

/* the safety half only: a dating couple got past sim_romance_ok when they
 * got together, and adult couples propose exactly as before LS0 */
int sim_marriage_ok(int a, int b) { return sim_romance_level(a, b) == 2; }

/* ---------------------------------------------------------------------- */
/* daily routines                                                           */
/* Favourite hobby -> where they like to spend free time. */
static const uint8_t hobby_venue[HOBBY_COUNT] = {
    ACT_CAFE, ACT_HOME, ACT_PARK, ACT_PARK, ACT_PARK,          /* bake knit garden star skate */
    ACT_HOME, ACT_PARK, ACT_CONCERT, ACT_CONCERT, ACT_HOME,   /* games paint sing dance read */
    ACT_BEACH, ACT_HOME, ACT_SHOP, ACT_CONCERT, ACT_CAFE,     /* fish nap cosplay karaoke board */
    ACT_PARK, ACT_PARK, ACT_BEACH, ACT_WORKSHOP, ACT_HOME,    /* bugs birds yoga pottery origami */
    ACT_WORKSHOP, ACT_BEACH, ACT_PARK, ACT_CAFE, ACT_VAPE_SHOP,/* plush clouds bubbles poetry tarot */
    ACT_HOME, ACT_WANDERING, ACT_CONCERT, ACT_WANDERING,      /* seances ghosts drums pranks */
    ACT_WANDERING, ACT_WORKSHOP, ACT_SHOP                     /* graffiti welding skulls */
};

/* Where a premade citizen works, from their job title (0xFF = nowhere
 * in particular). */
static int has_word(const char *s, const char *w)
{
    int i, j;
    for (i = 0; s[i]; i++) {
        for (j = 0; w[j]; j++) {
            int a = (unsigned char)s[i + j], b = (unsigned char)w[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (a != b) break;
        }
        if (!w[j]) return 1;
    }
    return 0;
}

static int job_venue_scan(int id)
{
    static const struct { const char *w; uint8_t act; } words[] = {
        { "bak", ACT_CAFE }, { "cafe", ACT_CAFE }, { "chef", ACT_CAFE },
        { "cook", ACT_CAFE }, { "barista", ACT_CAFE }, { "waiter", ACT_CAFE },
        { "priest", ACT_CHURCH }, { "nun", ACT_CHURCH }, { "choir", ACT_CHURCH },
        { "church", ACT_CHURCH }, { "deacon", ACT_CHURCH },
        { "weld", ACT_WORKSHOP }, { "mechanic", ACT_WORKSHOP },
        { "smith", ACT_WORKSHOP }, { "tinker", ACT_WORKSHOP },
        { "engineer", ACT_WORKSHOP }, { "carpenter", ACT_WORKSHOP },
        { "doctor", ACT_HOSPITAL }, { "nurse", ACT_HOSPITAL }, { "medic", ACT_HOSPITAL },
        { "vape", ACT_VAPE_SHOP }, { "cloud", ACT_VAPE_SHOP },
        { "shop", ACT_SHOP }, { "clerk", ACT_SHOP }, { "merchant", ACT_SHOP },
        { "tailor", ACT_SHOP }, { "florist", ACT_SHOP }, { "seller", ACT_SHOP },
        { "lifeguard", ACT_BEACH }, { "surf", ACT_BEACH }, { "fisher", ACT_BEACH },
        { "sailor", ACT_BEACH },
        { "dj", ACT_CONCERT }, { "singer", ACT_CONCERT }, { "band", ACT_CONCERT },
        { "drummer", ACT_CONCERT }, { "musician", ACT_CONCERT }, { "idol", ACT_CONCERT },
        { "garden", ACT_PARK }, { "ranger", ACT_PARK }, { "park", ACT_PARK },
        { "mail", ACT_WANDERING }, { "courier", ACT_WANDERING }, { "guard", ACT_WANDERING },
        { "patrol", ACT_WANDERING }, { "detective", ACT_WANDERING }
    };
    const char *job;
    int i;
    if (!sim_traits(id) && doomo_town.population != TOWN_POP_PREMADE) return 0xFF;
    if (doomo_town.res[id].flags & RF_CUSTOM) return 0xFF;
    job = roster_job(id);
    if (!job || !job[0]) return 0xFF;
    for (i = 0; i < DOOMO_COUNTOF(words); i++)
        if (has_word(job, words[i].w)) return words[i].act;
    return 0xFF;
}

/* Cached per resident; the (string scanning) lookup runs at most a few
 * times per tick, the rest of the time an unknown job means "no venue". */
static int job_venue(int id)
{
    uint8_t j = sim_s.job[id];
    if (!j) {
        if (!sim_s.job_budget) return 0xFF;
        sim_s.job_budget--;
        j = (uint8_t)job_venue_scan(id);
        j = j == 0xFF ? 0xFF : (uint8_t)(j + 1);
        sim_s.job[id] = j;
    }
    return j == 0xFF ? 0xFF : j - 1;
}

static int story_vape_open(void)
{
    return doomo_town.mode != TOWN_STORY || doomo_town.story.chapter >= 2;
}

/* Pick what resident `id` is doing this hour. Deterministic for (town,
 * id, day, hour) so it stays put within the hour, then bent by needs. */
/* the roster trait whose hours a LIFE_* stage keeps */
static const uint16_t stage_hours[LIFE_COUNT] = {
    0, ROSTER_T_KID, ROSTER_T_KID, ROSTER_T_KID, ROSTER_T_TEEN, ROSTER_T_ELDER
};

/* X4 (SP2): the gene-packed inputs a resident update reads twice (the routine and the needs), unpacked once per
 * update instead of once per reader: persona (PERS_COUNT gene reads) and the liked hobby. Pure reads of the gene,
 * which neither decide_activity nor apply_needs writes, so the values are the ones each reader read before. */
typedef struct ResIn { uint8_t p[PERS_COUNT]; uint8_t hobby; } ResIn;
static void res_in(const DoomoResident *r, ResIn *in)
{
    res_get_persona(r, in->p);
    in->hobby = RES_LIKE_HOBBY(r);
}

static int decide_activity(int id, const DoomoResident *r, const ResIn *in)
{
    int h = town_hour(), wd = doomo_town.weekday, weekend = wd >= 5;
    uint32_t hh = doomo_hash32((uint32_t)id * 2654435761u ^ doomo_town.seed ^
                               (doomo_town.day * 0x9E3779B1u) ^ ((uint32_t)h * 0x85EBCA6Bu));
    int traits = sim_traits(id), stage = RES_STAGE(r);
    int hell = RES_DISTRICT(id) || (r->flags & RF_HELLBORN);
    int sleep_at, wake_at, asleep, w[ACT_COUNT], total, i, pick;
    const uint8_t *p = in->p;

    /* the restless dead and the possessed keep their own hours */
    if (r->status == RES_POSSESSED)
        return sim_s.night ? ACT_HAUNTING : (hh % 3 == 0 ? ACT_VAPE_SHOP : ACT_WANDERING);
    if (r->status == RES_SOUL_LOST)
        return sim_s.night ? ACT_HAUNTING : ACT_HOME;
    if (r->status == RES_GHOST) {
        if (sim_s.night) return (hh & 3) ? ACT_HAUNTING : ACT_WANDERING;
        return (hh & 1) ? ACT_HOME : ((hh >> 1) & 1) ? ACT_PARK : ACT_CHURCH;
    }

    /* the saved life stage (LS0) counts as much as the roster's traits:
     * creator kids and sim babies keep kids' hours */
    if (stage < LIFE_COUNT) traits |= stage_hours[stage];

    /* chronotype */
    sleep_at = 22; wake_at = 6;
    if (hell) { sleep_at = 1; wake_at = 9; }
    if (traits & ROSTER_T_NIGHT_OWL) { sleep_at = 3; wake_at = 11; }
    else if (traits & ROSTER_T_ELDER) { sleep_at = 21; wake_at = 5; }
    else if (traits & ROSTER_T_KID) { sleep_at = 20; wake_at = 7; }
    else if (traits & ROSTER_T_TEEN) { sleep_at = 23; wake_at = weekend ? 10 : 7; }
    if (p[PERS_MOVEMENT] < 4) wake_at++;                 /* slowpokes sleep in */
    if (weekend && !(traits & (ROSTER_T_KID | ROSTER_T_ELDER))) wake_at++;
    if ((hh >> 8) & 1) sleep_at++;                       /* a little jitter    */
    sleep_at %= 24; wake_at %= 24;
    asleep = sleep_at > wake_at ? (h >= sleep_at || h < wake_at)
                                : (h >= sleep_at && h < wake_at);
    if (asleep) return ACT_SLEEPING;
    if (r->stat[STAT_ENERGY] < 12) return ACT_SLEEPING;  /* collapse nap */

    /* needs come first */
    if (r->stat[STAT_HEALTH] < 25) return ACT_HOSPITAL;
    if (r->stat[STAT_HUNGER] < 28) return (hh & 1) ? ACT_CAFE : ACT_HOME;
    if (r->stat[STAT_HYGIENE] < 18) return ACT_HOME;
    if (h == wake_at) return ACT_HOME;                   /* breakfast + shower */

    /* work */
    if (!weekend && ((h >= 9 && h < 12) || (h >= 13 && h < 17)) &&
        !(traits & (ROSTER_T_KID | ROSTER_T_ELDER))) {
        int jv = job_venue(id);
        if (jv != 0xFF && (hh % 10) < 8) return jv;
    }
    if (h == 12 && (hh % 3) != 0) return (hh & 4) ? ACT_CAFE : ACT_HOME;

    /* free time: weighted pick */
    for (i = 0; i < ACT_COUNT; i++) w[i] = 0;
    w[ACT_HOME] = 16 + (15 - p[PERS_MOVEMENT]);
    w[ACT_PARK] = 14 + p[PERS_MOVEMENT] / 2;
    w[ACT_CAFE] = 12 + p[PERS_EXPRESSION] / 2;
    w[ACT_SHOP] = 8;
    w[ACT_BEACH] = weekend ? 14 : 5;
    w[ACT_CONCERT] = (h >= 18 || h < 2) ? 10 + p[PERS_EXPRESSION] : 1;
    w[ACT_CHURCH] = (wd == 6 && h < 13) ? 22 : 2;
    w[ACT_WORKSHOP] = 3 + (15 - p[PERS_ATTITUDE]) / 4;
    w[ACT_WANDERING] = 5 + p[PERS_QUIRK] / 2;
    if (traits & ROSTER_T_HOLY) w[ACT_CHURCH] += 20;
    if (h >= 21 || h < 6) { w[ACT_PARK] /= 3; w[ACT_BEACH] /= 3; w[ACT_SHOP] = 1; }
    if (hell) {
        w[ACT_VAPE_SHOP] = 14 + p[PERS_QUIRK];
        w[ACT_CONCERT] += 10;
        w[ACT_WANDERING] += 8;
        w[ACT_CHURCH] = (traits & ROSTER_T_SOFTIE) ? 4 : 1;
        w[ACT_PARK] = w[ACT_PARK] * 2 / 3;
    } else if (story_vape_open()) {
        w[ACT_VAPE_SHOP] = 1 + p[PERS_QUIRK] / 4 + r->stat[STAT_CORRUPTION] / 12 +
                           ((traits & ROSTER_T_VAPER) ? 30 : 0);
    }
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter < 2 && !hell)
        w[ACT_VAPE_SHOP] = 0;
    w[hobby_venue[in->hobby % HOBBY_COUNT]] += 22;
    if (RES_LIKE_MUSIC(r) == sim_concert_genre() && (h >= 17 || h < 2)) w[ACT_CONCERT] += 30;
    if (r->stat[STAT_SOCIAL] < 35) { w[ACT_CAFE] += 10; w[ACT_PARK] += 8; w[ACT_HOME] /= 2; }
    if (r->stat[STAT_ENERGY] < 30) w[ACT_HOME] += 25;
    if (r->stat[STAT_SANITY] < 35) w[ACT_CHURCH] += 10;
    if (r->stat[STAT_CORRUPTION] >= 60) w[ACT_CHURCH] += 6;

    for (i = 0, total = 0; i < ACT_COUNT; i++) total += w[i];
    pick = (int)(((hh >> 16) * (uint32_t)(total > 0 ? total : 1)) >> 16);
    for (i = 0; i < ACT_COUNT; i++) {
        if (pick < w[i]) return i;
        pick -= w[i];
    }
    return ACT_HOME;
}

/* ---------------------------------------------------------------------- */
/* needs                                                                    */
/* Per game hour while doing an activity:
 *              hunger happy hygiene energy health sanity corrupt social */
static const int8_t act_fx[ACT_COUNT][STAT_COUNT] = {
    /* HOME     */ {  2,  1,  2,   2,  1,  1,  0, -2 },
    /* SLEEPING */ { -1,  0, -1,  12,  1,  2,  0, -1 },
    /* PARK     */ { -4,  3, -2,  -3,  1,  2, -1,  3 },
    /* CAFE     */ { 14,  2, -1,  -1,  0,  1,  0,  4 },
    /* SHOP     */ { -3,  3, -1,  -3,  0,  1,  0,  2 },
    /* CHURCH   */ { -2,  1,  0,  -2,  0,  4, -5,  2 },
    /* WORKSHOP */ { -4,  2, -4,  -4,  0,  1,  0,  2 },
    /* BEACH    */ { -4,  4, -2,  -3,  1,  3, -1,  3 },
    /* CONCERT  */ { -4,  4, -2,  -5,  0,  1,  1,  5 },
    /* VAPE     */ { -3,  2, -2,  -2, -3, -3,  4,  2 },
    /* HOSPITAL */ { -2, -1,  1,   0, 12,  1,  0,  1 },
    /* WANDER   */ { -3,  1, -2,  -3,  0,  0,  0,  1 },
    /* HAUNTING */ {  0,  0,  0,  -2,  0, -3,  2,  1 }
};

static int species_pct(const DoomoResident *r, int stat)
{
    int sp = RES_SPECIES(r), fl = doomo_species_info[sp % SPECIES_COUNT].flags;
    if (stat == STAT_HUNGER) {
        if (sp == SPECIES_GHOST) return 20;
        if (fl & SPF_MECHANICAL) return 30;
        if (fl & SPF_UNDEAD) return 50;
        if (sp == SPECIES_PLUSHIE) return 50;
        if (sp == SPECIES_SLIME || sp == SPECIES_HELLHOUND) return 125;
        return 100;
    }
    if (stat == STAT_HYGIENE) {
        if (sp == SPECIES_GHOST) return 20;
        if (fl & SPF_SQUISHY) return 40;                 /* slime is a lifestyle */
        if (fl & SPF_MECHANICAL) return 60;
        if (sp == SPECIES_SHEEPKIN || sp == SPECIES_RACCOONKIN) return 120; /* fluff! */
        return 100;
    }
    return 100;
}

static void apply_needs(int id, DoomoResident *r, int m, const ResIn *in)
{
    const int8_t *fx = act_fx[r->activity < ACT_COUNT ? r->activity : 0];
    const uint8_t *p = in->p;
    int h = doomo_town.minute / 60, s, rate, target, tr, sp_hunger, sp_hygiene;
    Dither dz = { 0, 0 };

    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        /* the demon doesn't eat, bathe or sleep - the host just withers */
        sim_stat_add(r, STAT_SANITY, dith_div(&dz, -6, m, 60));
        sim_stat_add(r, STAT_HAPPY, dith_div(&dz, -4, m, 60));
        sim_stat_add(r, STAT_HYGIENE, dith_div(&dz, -2, m, 60));
        sim_stat_add(r, STAT_SOCIAL, dith_div(&dz, -2, m, 60));
        if (r->stat[STAT_HEALTH] > 30) sim_stat_add(r, STAT_HEALTH, dith_div(&dz, -1, m, 60));
        return;
    }
    sp_hunger = species_pct(r, STAT_HUNGER);
    sp_hygiene = species_pct(r, STAT_HYGIENE);
    for (s = 0; s < STAT_COUNT; s++) {
        if (s == STAT_CORRUPTION || s == STAT_HAPPY) continue;   /* below */
        rate = fx[s] * 4;                                    /* quarter units */
        if (rate < 0) {
            int pct = s == STAT_HUNGER ? sp_hunger : s == STAT_HYGIENE ? sp_hygiene : 100;
            if (s == STAT_HUNGER) pct = sim_div100(pct * (100 + (p[PERS_MOVEMENT] - 8) * 4));
            if (s == STAT_HYGIENE) pct = sim_div100(pct * (100 + (p[PERS_MOVEMENT] - 8) * 2 +
                                                           (p[PERS_QUIRK] - 8) * 3));
            if (s == STAT_ENERGY) pct = sim_div100(pct * (100 + (p[PERS_MOVEMENT] - 8) * 3));
            if (s == STAT_SOCIAL) pct = sim_div100(pct * (100 + (p[PERS_EXPRESSION] - 8) * 5));
            rate = sim_div100(rate * pct);
        }
        /* everyone gets peckish and grubby, whatever they do */
        if (s == STAT_HUNGER) rate -= sim_div100(8 * sp_hunger);
        if (s == STAT_HYGIENE) rate -= sim_div100(4 * sp_hygiene);
        if (s == STAT_SOCIAL && r->activity == ACT_HOME) rate -= (p[PERS_EXPRESSION] >= 8) ? 4 : 0;
        if (r->activity == ACT_HOME && h >= 5 && h <= 11) {  /* breakfast & shower */
            if (s == STAT_HUNGER) rate += 60;
            if (s == STAT_HYGIENE) rate += 200;
        }
        if (r->activity == ACT_HOME && h >= 18 && h <= 23 && s == STAT_HYGIENE) rate += 60;
        if (r->activity == ACT_HOME && (h == 12 || h == 19) && s == STAT_HUNGER) rate += 36;
        if (r->status == RES_GHOST && (s == STAT_HUNGER || s == STAT_HYGIENE)) rate = 0;
        sim_stat_add(r, s, dith_div(&dz, rate, m, 240));
    }
    /* health: neglect makes you ill, a good routine heals */
    if (r->stat[STAT_HUNGER] < 12 || r->stat[STAT_HYGIENE] < 8) rate = -3;
    else if (r->stat[STAT_ENERGY] < 8) rate = -1;
    else rate = 1;
    sim_stat_add(r, STAT_HEALTH, dith_div(&dz, rate, m, 60));
    /* sanity: darkness, fear of the dark, a corrupted soul */
    rate = 0;
    if (r->stat[STAT_CORRUPTION] > 60) rate -= 2;
    if (sim_s.night && r->activity != ACT_SLEEPING) {
        rate -= 1;
        if (RES_FEAR(r) == 1 /* The Dark */) rate -= 2;
    }
    if (r->stat[STAT_SOCIAL] < 15 && RES_FEAR(r) == 11 /* Being Alone */) rate -= 2;
    if (r->stat[STAT_SANITY] < 60 && rate == 0) rate = 1;
    sim_stat_add(r, STAT_SANITY, dith_div(&dz, rate, m, 60));

    /* happiness: the activity, a favourite pastime, then a drift toward
     * how well cared-for they are */
    rate = fx[STAT_HAPPY];
    if (hobby_venue[in->hobby % HOBBY_COUNT] == r->activity && r->activity != ACT_HOME) rate += 2;
    if (r->activity == ACT_CONCERT && RES_LIKE_MUSIC(r) == sim_concert_genre()) rate += 4;
    sim_stat_add(r, STAT_HAPPY, dith_div(&dz, rate, m, 120));
    target = (r->stat[STAT_HUNGER] + r->stat[STAT_HYGIENE] + r->stat[STAT_ENERGY] +
              r->stat[STAT_HEALTH] + r->stat[STAT_SOCIAL] * 2 + r->stat[STAT_SANITY]) * 3 / 28;
    /* a little love from the player goes a long way */
    target += r->level / 6;
    tr = sim_transient(id);
    if (tr == TR_ECSTATIC) target += 25;
    else if (tr == TR_SAD || tr == TR_GRIEF) target -= 25;
    else if (tr == TR_ANGRY) target -= 12;
    else if (tr == TR_LOVESICK) target += 5;
    else if (tr == TR_SCARED) target -= 10;
    if (p[PERS_ATTITUDE] >= 12) target += 5;               /* easygoing      */
    target = DOOMO_CLAMP(target, 0, 100);
    {
        int gap = target - r->stat[STAT_HAPPY];
        int mm = m > 180 ? 180 : m;
        int d = gap < 0 ? -(int)sim_udiv((uint32_t)(-gap * mm), 360) : (int)sim_udiv((uint32_t)(gap * mm), 360);
        if (!d && gap && ((dither8(&dz) * 360) >> 8) < (gap < 0 ? -gap : gap) * mm)
            d = gap > 0 ? 1 : -1;
        sim_stat_add(r, STAT_HAPPY, d);
    }
}

static void tick_transient(int id, int m)
{
    uint8_t t = sim_s.transient[id];
    int hours, left;
    if (!(t & 15)) { sim_s.transient[id] = 0; return; }
    hours = m / 60;
    if (sim_rand(60) < m - hours * 60) hours++;
    left = (t & 15) - hours;
    sim_s.transient[id] = left > 0 ? (uint8_t)((t & 0xF0) | left) : 0;
}

/* ---------------------------------------------------------------------- */
/* moods                                                                    */
int sim_mood_of(int id)
{
    DoomoResident *r = town_res(id);
    int m, tr;
    if (!r || r->status == RES_EMPTY) return MOOD_OK;
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) m = MOOD_POSSESSED;
    else if (r->status == RES_DEAD) m = MOOD_OK;
    else {
        tr = id < DOOMO_MAX_RESIDENTS ? sim_transient(id) : TR_NONE;
        if (r->stat[STAT_CORRUPTION] >= 70) m = MOOD_CORRUPTED;
        else if (r->stat[STAT_HEALTH] < 30) m = MOOD_SICK;
        else if (tr == TR_ANGRY) m = MOOD_ANGRY;
        else if (tr == TR_SCARED || r->stat[STAT_SANITY] < 25) m = MOOD_SCARED;
        else if (tr == TR_ECSTATIC) m = MOOD_ECSTATIC;
        else if (r->stat[STAT_HUNGER] < 20) m = MOOD_HUNGRY;
        else if (r->stat[STAT_ENERGY] < 15) m = MOOD_SLEEPY;
        else if (r->stat[STAT_HYGIENE] < 15) m = MOOD_FILTHY;
        else if (tr == TR_SAD || tr == TR_GRIEF) m = MOOD_SAD;
        else if (tr == TR_LOVESICK || bit_get(sim_s.crush, id)) m = MOOD_LOVESICK;
        else if (r->stat[STAT_SOCIAL] < 20) m = MOOD_LONELY;
        else if (r->stat[STAT_HAPPY] >= 88) m = MOOD_ECSTATIC;
        else if (r->stat[STAT_HAPPY] >= 65) m = MOOD_HAPPY;
        else if (r->stat[STAT_HAPPY] < 30) m = MOOD_SAD;
        else m = MOOD_OK;
    }
    r->mood = (uint8_t)m;
    return m;
}

/* ---------------------------------------------------------------------- */
/* one resident                                                             */
/* force_decide: a fast-forward chunk stands on the last minute of the hour the
 * elapsed span belongs to, so the routine is chosen again even though the clock
 * has not crossed an hour boundary since the last update. */
static void update_resident_ex(int id, int force_decide)
{
    DoomoResident *r = &doomo_town.res[id];
    uint16_t now16;
    int el, act;
    ResIn in;
    if (r->status == RES_EMPTY || r->status == RES_DEAD) return;
    sim_s.ops++;
    now16 = (uint16_t)sim_now();
    el = (uint16_t)(now16 - sim_s.stamp[id]);
    if (el < SIM_MIN_ELAPSED) return;
    if (el > SIM_MAX_ELAPSED) el = SIM_MAX_ELAPSED;
    sim_s.stamp[id] = now16;
    res_in(r, &in);

    /* routines change on the hour (or when a need gets urgent) */
    if (force_decide || doomo_town.minute / 60 != ((doomo_town.minute + 1440 - el) % 1440) / 60 || el >= 60 ||
        r->stat[STAT_HEALTH] < 25 || r->stat[STAT_HUNGER] < 28 || r->stat[STAT_ENERGY] < 12 ||
        r->status != RES_ALIVE) {
        act = decide_activity(id, r, &in);
        if (act != r->activity) r->activity = (uint8_t)act;
    }
    apply_needs(id, r, el, &in);
    sim_soul_update(id, r, el);
    if (r->status == RES_EMPTY || r->status == RES_DEAD) return;
    tick_transient(id, el);
    if (sim_is_venue(r->activity)) sim_venue_meet(id, r, el);
    sim_check_problems(id, r);
    sim_mood_of(id);
}

static void update_resident(int id) { update_resident_ex(id, 0); }

/* ---------------------------------------------------------------------- */
/* the clock                                                                */
static void day_rollover(void)
{
    int i, month, dom;
    progress_add(PROG_DAYS_LIVED, 1);
    /* jobs are looked up again lazily (a slot may hold a new citizen) */
    memset(sim_s.job, 0, sizeof(sim_s.job));
    sim_date(&month, &dom);
    /* birthdays! */
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = &doomo_town.res[i];
        if (r->status != RES_ALIVE && r->status != RES_GHOST) continue;
        if (RES_BDAY_MONTH(r) == month && RES_BDAY_DAY(r) == dom) {
            sim_stat_add(r, STAT_HAPPY, 25);
            sim_set_transient(i, TR_ECSTATIC, 8);
            if (sim_chance(60) || (r->flags & RF_FAVORITE))
                sim_news_rec((r->flags & RF_HELLBORN) ? NT_BIRTHDAY_HELL : NT_BIRTHDAY_HILL, 0,
                             NEWS_IDS(i, NEWS_NONE, NEWS_NONE));
        }
    }
    sim_soul_daily();
    sim_news_daily();
    family_age_daily();                  /* F4: the household grows up */
    playas_check();                      /* PA1: a lost or possessed avatar sends you back to the hero */
    prof_check_daily();                  /* MP0: a guest whose villager is gone moves out */
    family_freewill_daily();             /* F7: the household looks after itself */
    family_help_daily();                 /* F9: a helping hand (bonus) */
    family_events_daily();               /* F8: confidences, squabbles, making up */
    life_events_daily();                 /* LS3: a potluck, a lucky find, a rotten day ... */
    family_disaster_daily();             /* F10: warnings and outcomes (Loss on) */
    family_loss_daily();                 /* F6: neglect and old age */
    ending_route_daily();                /* E4: romance routes grow, queue their scenes */
    ending_daily();                      /* E12: Family Portrait and Bonejangles Bows can arrive late */
}

static void minute_step(void)
{
    doomo_town.minute++;
    sim_s.night = (uint8_t)(doomo_town.minute >= 21 * 60 || doomo_town.minute < 6 * 60);
    if (doomo_town.minute >= 1440) {
        doomo_town.minute = 0;
        sim_s.rough_day = 0;
        doomo_town.day++;
        doomo_town.weekday = (uint8_t)((doomo_town.weekday + 1) % 7);
        day_rollover();
    }
    if (doomo_town.minute % 60 == 0) {
        sim_s.args_hour = 0;
        if (sim_chance(opt_get(OPT_EVENT_RATE) == 0 ? 4 : opt_get(OPT_EVENT_RATE) == 1 ? 9 : 18))
            sim_news_fluff();
    }
    pet_tick_minute();
    sim_problems_minute();
    /* about one visit per edge per game hour */
    sim_s.edge_credit += doomo_town.edge_count;
    if (sim_s.edge_credit > 60u * DOOMO_MAX_EDGES) sim_s.edge_credit = 60u * DOOMO_MAX_EDGES;
}

static int ticks_per_minute_q(void)
{
    static const uint16_t day_len[5] = { 7200, 600, 300, 150, 75 };
    int d = opt_get(OPT_DAY_LENGTH);
    return day_len[DOOMO_CLAMP(d, 0, 4)];
}

static int speed_q(void)
{
    static const uint8_t spd[4] = { 2, 4, 8, 16 };
    int s = opt_get(OPT_SIM_SPEED);
    return spd[DOOMO_CLAMP(s, 0, 3)];
}

/* (Re)initialise when the town under us was replaced (town_new without a
 * sim_init, or time running backwards after a load). */
void sim_ensure_init(void)
{
    if (!sim_s.inited || sim_s.town_seed != doomo_town.seed ||
        sim_s.town_pop != doomo_town.population || sim_s.town_mode != doomo_town.mode ||
        doomo_town.day < sim_s.day_seen)
        sim_init();
    sim_s.day_seen = doomo_town.day;
}

void sim_init(void)
{
    int i;
    uint16_t now16;
    ResIn in;
    memset(&sim_s, 0, sizeof(sim_s));
    sim_s.inited = 1;
    sim_s.town_seed = doomo_town.seed;
    sim_s.town_pop = doomo_town.population;
    sim_s.town_mode = doomo_town.mode;
    sim_s.day_seen = doomo_town.day;
    sim_s.night = (uint8_t)town_is_night();
    rng_seed(&sim_s.rng, doomo_town.seed ^ 0x5113A11Eu ^ (doomo_town.day * 2654435761u) ^
                             doomo_town.minute);
    now16 = (uint16_t)sim_now();
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) sim_s.stamp[i] = now16;
    for (i = 0; i < ACT_COUNT; i++) {
        int j;
        for (j = 0; j < SIM_RING; j++) sim_s.ring[i][j] = 0xFFFF;
    }
    sim_social_init();
    sim_recount_possessed();
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = &doomo_town.res[i];
        if (r->status == RES_EMPTY || r->status == RES_DEAD) continue;
        res_in(r, &in);
        r->activity = (uint8_t)decide_activity(i, r, &in);
        sim_mood_of(i);
    }
    sim_events_reset();
    sim_news_welcome(town_count(-1));     /* LS1: once per town or load */
}

void sim_tick(void)
{
    int per, steps = 0, i;
    sim_ensure_init();
    sim_s.ops = 0;
    sim_s.new_edges = 0;
    sim_s.job_budget = 2;
    per = ticks_per_minute_q();
    sim_s.acc = (uint16_t)(sim_s.acc + speed_q());
    while (sim_s.acc >= per && steps < 8) {
        sim_s.acc = (uint16_t)(sim_s.acc - per);
        minute_step();
        steps++;
    }
    if (sim_s.acc >= per) sim_s.acc = (uint16_t)(per - 1);
    for (i = 0; i < SIM_BATCH; i++) {
        int id = sim_s.cursor;
        sim_s.cursor = (uint16_t)(sim_s.cursor + 1 >= DOOMO_MAX_RESIDENTS ? 0 : sim_s.cursor + 1);
        update_resident(id);
    }
    sim_edges_step(SIM_EDGE_BATCH);
    if (sim_s.ops > sim_s.max_ops) sim_s.max_ops = sim_s.ops;
}

/* Fast-forward (sleeping, travel). Residents are caught up hour by hour, not
 * once at the end: each span is lived with the routine, the night flag and the
 * breakfast / shower hours of the hour it belongs to. (One catch-up used the
 * activity of the LAST minute for the whole span: eight hours of sleep gave
 * about half the rest, and the breakfast bonus was paid for every one of the
 * eight hours.) A span is closed on the last minute of an hour (:59), where the
 * clock still reads that hour. Needs never look back further than
 * SIM_MAX_ELAPSED, so days before that window pass on the clock alone. Cost:
 * one pass over the town per hour of the window (at most 13), once per call. */
void sim_advance_minutes(int minutes)
{
    int i, passes, window, left, chunk;
    sim_ensure_init();
    if (minutes <= 0) return;
    window = minutes < SIM_MAX_ELAPSED ? minutes : SIM_MAX_ELAPSED;
    for (i = minutes - window; i > 0; i--) minute_step();
    if (minutes > window) {
        uint16_t now16 = (uint16_t)sim_now();
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) sim_s.stamp[i] = now16;
    }
    sim_s.new_edges = 0;
    for (left = window; left > 0; left -= chunk) {
        int to59 = 59 - doomo_town.minute % 60;
        chunk = to59 > 0 ? to59 : 60;
        if (chunk > left) chunk = left;
        for (i = 0; i < chunk; i++) minute_step();
        sim_s.job_budget = 32;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) update_resident_ex(i, 1);
    }
    /* give the graph a proportional share of attention (at most one full pass) */
    passes = (int)(sim_s.edge_credit / 60u);
    if (passes > doomo_town.edge_count) passes = doomo_town.edge_count;
    for (i = 0; i < passes; i += SIM_EDGE_BATCH) {
        sim_s.new_edges = 0;
        sim_edges_step(SIM_EDGE_BATCH);
    }
}
