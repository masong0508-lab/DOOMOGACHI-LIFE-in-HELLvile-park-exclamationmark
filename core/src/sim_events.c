/* sim_events.c - thought-bubble problems and the town news ticker
 * (owner: life).
 *
 * Problems: a small queue (SIM_MAX_PROBLEMS) of things residents want the
 * player to sort out, one per resident. Each has an age in game minutes;
 * unanswered ones expire with a small consequence (romantic ones are
 * eventually acted on without the player, see sim_social.c). Resolving a
 * problem applies its effect and pays a little reward, like Tomodachi Life.
 *
 * News: the newest NEWS_MAX headlines (ring buffer, newest first). The
 * sim writes headlines for its events, and adds charming filler from
 * templates so the ticker always has something to say.
 */
#include <stdarg.h>
#include "sim_int.h"

const char *const doomo_problem_names[PROB_COUNT] = {
    "None", "Hungry", "Dirty", "Bored", "Sick", "Wants a friend", "Fight",
    "Crush", "Confession advice", "Relationship trouble", "Wants an item",
    "Nightmare", "Corruption", "Possessed", "Proposal advice", "Lonely"
};

static char g_reaction[96];

const char *sim_last_reaction(void) { return g_reaction; }

/* ---------------------------------------------------------------------- */
/* problems                                                                 */
void sim_events_reset(void)
{
    int i;
    sim_s.nprob = 0;
    sim_s.news_head = 0;
    sim_s.news_n = 0;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status == RES_POSSESSED)
            sim_problem_raise(i, PROB_POSSESSED, sim_possessor_of(i), 0);
}

int sim_problem_count(void)
{
    sim_ensure_init();
    return sim_s.nprob;
}

const DoomoProblem *sim_problem(int index)
{
    if (index < 0 || index >= sim_s.nprob) return NULL;
    return &sim_s.prob[index];
}

int sim_problem_find(int id, int kind)
{
    int i;
    for (i = 0; i < sim_s.nprob; i++)
        if (sim_s.prob[i].resident == id && (kind < 0 || sim_s.prob[i].kind == kind)) return i;
    return -1;
}

int sim_problem_for(int resident_id)
{
    sim_ensure_init();
    return sim_problem_find(resident_id, -1);
}

void sim_problem_remove(int index)
{
    if (index < 0 || index >= sim_s.nprob) return;
    memmove(&sim_s.prob[index], &sim_s.prob[index + 1],
            (size_t)(sim_s.nprob - index - 1) * sizeof(DoomoProblem));
    sim_s.nprob--;
}

void sim_problem_drop_for(int id)
{
    int i;
    for (i = sim_s.nprob - 1; i >= 0; i--)
        if (sim_s.prob[i].resident == id || sim_s.prob[i].other == id) sim_problem_remove(i);
}

int sim_problem_raise(int resident, int kind, int other, int item)
{
    DoomoProblem *p;
    int i;
    if (resident < 0 || resident >= DOOMO_MAX_RESIDENTS || kind <= PROB_NONE || kind >= PROB_COUNT)
        return -1;
    if (doomo_town.res[resident].status == RES_EMPTY || doomo_town.res[resident].status == RES_DEAD)
        return -1;
    i = sim_problem_find(resident, -1);
    if (i >= 0) {
        if (kind != PROB_POSSESSED || sim_s.prob[i].kind == PROB_POSSESSED) return -1;
        sim_problem_remove(i);                 /* possession trumps all  */
    }
    if (sim_s.nprob >= SIM_MAX_PROBLEMS) {
        if (kind != PROB_POSSESSED) return -1;
        for (i = 0; i < sim_s.nprob; i++)
            if (sim_s.prob[i].kind != PROB_POSSESSED) { sim_problem_remove(i); break; }
        if (sim_s.nprob >= SIM_MAX_PROBLEMS) return -1;
    }
    p = &sim_s.prob[sim_s.nprob];
    p->resident = (uint16_t)resident;
    p->other = (uint16_t)(other >= 0 ? other : 0xFFFF);
    p->kind = (uint8_t)kind;
    p->item = (uint8_t)item;
    p->age = 0;
    sim_s.stats.problems++;
    return sim_s.nprob++;
}

const char *sim_problem_text(int index, char *buf, int cap)
{
    const DoomoProblem *p = sim_problem(index);
    const char *n, *o;
    if (!buf || cap <= 0) return "";
    if (!p) { buf[0] = 0; return buf; }
    n = sim_name(p->resident);
    o = p->other != 0xFFFF ? sim_name(p->other) : "someone";
    switch (p->kind) {
    case PROB_HUNGRY: doomo_snprintf(buf, cap, "%s is starving!", n); break;
    case PROB_DIRTY: doomo_snprintf(buf, cap, "%s really needs a bath.", n); break;
    case PROB_BORED: doomo_snprintf(buf, cap, "%s is bored stiff.", n); break;
    case PROB_SICK: doomo_snprintf(buf, cap, "%s feels sick...", n); break;
    case PROB_WANTS_FRIEND: doomo_snprintf(buf, cap, "%s wants a new friend!", n); break;
    case PROB_FIGHT: doomo_snprintf(buf, cap, "%s had a fight with %s!", n, o); break;
    case PROB_CRUSH: doomo_snprintf(buf, cap, "%s has a crush on %s!", n, o); break;
    case PROB_CONFESS_HELP: doomo_snprintf(buf, cap, "%s: should I confess to %s?", n, o); break;
    case PROB_BREAKUP: doomo_snprintf(buf, cap, "%s and %s are on the rocks.", n, o); break;
    case PROB_WANTS_ITEM:
        doomo_snprintf(buf, cap, "%s really wants: %s!", n,
                       p->item < ITEM_COUNT ? doomo_items[p->item].name : "something");
        break;
    case PROB_NIGHTMARE: doomo_snprintf(buf, cap, "%s had a horrible nightmare.", n); break;
    case PROB_CORRUPTION: doomo_snprintf(buf, cap, "%s feels a darkness creeping in...", n); break;
    case PROB_POSSESSED:
        doomo_snprintf(buf, cap, "%s is POSSESSED by %s!", n, sim_possessor_name(p->resident));
        break;
    case PROB_PROPOSAL: doomo_snprintf(buf, cap, "%s wants to propose to %s!", n, o); break;
    case PROB_LONELY: doomo_snprintf(buf, cap, "%s feels so lonely.", n); break;
    default: doomo_snprintf(buf, cap, "%s needs you.", n); break;
    }
    return buf;
}

static void reward(DoomoResident *r, int id, int cash)
{
    if (cash > 0) {
        doomo_town.money += (uint32_t)cash;
        progress_add(PROG_MONEY_EARNED, cash);
    }
    r->level = (uint8_t)DOOMO_MIN(99, r->level + 3);
    rel_adjust(id, DOOMO_PLAYER_ID, 4);
}

/* A compatible stranger for a lonely resident (bounded sampling). */
static int find_new_friend(int id)
{
    int tries, best = -1, best_c = -100;
    for (tries = 0; tries < 24; tries++) {
        int o = sim_rand(DOOMO_MAX_RESIDENTS), c;
        if (o == id || !sim_alive_ok(&doomo_town.res[o])) continue;
        if (rel_find(id, o)) continue;
        c = sim_compat(id, o) + (RES_DISTRICT(o) == RES_DISTRICT(id) ? 1 : 0);
        if (c > best_c) { best_c = c; best = o; }
    }
    return best;
}

static void set_reaction(const char *s)
{
    int i;
    for (i = 0; i < (int)sizeof(g_reaction) - 1 && s[i]; i++) g_reaction[i] = s[i];
    g_reaction[i] = 0;
}

void sim_problem_resolve(int index)
{
    DoomoProblem p;
    DoomoResident *r;
    int id;
    sim_ensure_init();
    if (index < 0 || index >= sim_s.nprob) return;
    p = sim_s.prob[index];
    id = p.resident;
    r = &doomo_town.res[id];
    set_reaction("Thank you!");
    switch (p.kind) {
    case PROB_HUNGRY:
        sim_stat_add(r, STAT_HUNGER, 45); sim_stat_add(r, STAT_HAPPY, 10);
        set_reaction("Mmm! That hit the spot!");
        break;
    case PROB_DIRTY:
        r->stat[STAT_HYGIENE] = 100; sim_stat_add(r, STAT_HAPPY, 6);
        set_reaction("Squeaky clean! *sparkle*");
        break;
    case PROB_BORED:
        sim_stat_add(r, STAT_HAPPY, 18); sim_stat_add(r, STAT_SOCIAL, 10);
        set_reaction("That was so much fun!");
        break;
    case PROB_SICK:
        sim_stat_add(r, STAT_HEALTH, 45);
        set_reaction("I feel much better now!");
        break;
    case PROB_WANTS_FRIEND: case PROB_LONELY: {
        int o = find_new_friend(id);
        sim_stat_add(r, STAT_SOCIAL, 30); sim_stat_add(r, STAT_HAPPY, 10);
        if (o >= 0) set_reaction(sim_introduce(id, o));
        else set_reaction("Thanks for keeping me company!");
        break;
    }
    case PROB_FIGHT:
        if (p.other != 0xFFFF) set_reaction(sim_make_up(id, p.other));
        break;
    case PROB_CRUSH: case PROB_CONFESS_HELP:
        if (p.other != 0xFFFF) set_reaction(sim_advise_confess(id, p.other, 1));
        break;
    case PROB_PROPOSAL:
        set_reaction(sim_advise_propose(id, 1));
        break;
    case PROB_BREAKUP:
        set_reaction(sim_advise_breakup(id, 1));
        break;
    case PROB_WANTS_ITEM:
        if (p.item > ITEM_NONE && p.item < ITEM_COUNT && !opt_get(OPT_CHEAT_INFINITE_CANDY) &&
            !inv_take(p.item, 1)) {
            set_reaction("You don't have one... it's okay.");
            return;                                  /* stays open */
        }
        sim_stat_add(r, STAT_HAPPY, 28);
        progress_add(PROG_GIFTS_GIVEN, 1);
        set_reaction("It's PERFECT! You're the best!");
        break;
    case PROB_NIGHTMARE:
        sim_stat_add(r, STAT_SANITY, 22); sim_stat_add(r, STAT_CORRUPTION, -10);
        if (sim_transient(id) == TR_SCARED) sim_set_transient(id, TR_NONE, 0);
        set_reaction("It was only a dream... thank you.");
        break;
    case PROB_CORRUPTION:
        sim_stat_add(r, STAT_CORRUPTION, -25); sim_stat_add(r, STAT_SANITY, 10);
        set_reaction("The whispers are quieter now.");
        break;
    case PROB_POSSESSED:
        if (r->status == RES_POSSESSED) {
            set_reaction("Only an exorcism can help now!");
            return;                                  /* needs SCENE_EXORCISM */
        }
        break;
    default: break;
    }
    /* the advice helpers may already have removed it */
    index = sim_problem_find(id, p.kind);
    if (index >= 0) sim_problem_remove(index);
    reward(r, id, 10 + sim_rand(16));
    sim_mood_of(id);
}

void sim_problems_minute(void)
{
    int i;
    for (i = sim_s.nprob - 1; i >= 0; i--) {
        DoomoProblem *p = &sim_s.prob[i];
        DoomoResident *r = &doomo_town.res[p->resident];
        int limit;
        if (p->age < 0xFFFF) p->age++;
        if (r->status == RES_EMPTY || r->status == RES_DEAD) { sim_problem_remove(i); continue; }
        if (p->kind == PROB_POSSESSED) {
            if (r->status != RES_POSSESSED) sim_problem_remove(i);
            continue;
        }
        limit = (p->kind == PROB_CRUSH || p->kind == PROB_CONFESS_HELP ||
                 p->kind == PROB_PROPOSAL || p->kind == PROB_BREAKUP) ? 1440 : 720;
        if (p->age < limit) continue;
        switch (p->kind) {
        case PROB_HUNGRY: case PROB_BORED: case PROB_LONELY: case PROB_WANTS_FRIEND:
            sim_stat_add(r, STAT_HAPPY, -6); break;
        case PROB_DIRTY: sim_stat_add(r, STAT_HAPPY, -4); break;
        case PROB_SICK: sim_stat_add(r, STAT_HEALTH, -5); break;
        case PROB_NIGHTMARE: sim_stat_add(r, STAT_SANITY, -5); break;
        default: break;
        }
        sim_problem_remove(i);
    }
}

void sim_check_problems(int id, DoomoResident *r)
{
    const uint8_t *s = r->stat;
    int kind = PROB_NONE, item = 0;
    if (r->status == RES_POSSESSED) {
        if (sim_problem_find(id, PROB_POSSESSED) < 0)
            sim_problem_raise(id, PROB_POSSESSED, sim_possessor_of(id), 0);
        return;
    }
    if (r->status != RES_ALIVE) return;
    if (sim_s.nprob >= SIM_MAX_PROBLEMS - 2) return;        /* keep room for drama */
    if (sim_problem_find(id, -1) >= 0) return;
    if (!sim_chance(12)) return;                            /* not everyone at once */
    if (s[STAT_CORRUPTION] >= 72) kind = PROB_CORRUPTION;
    else if (s[STAT_HEALTH] < 35) kind = PROB_SICK;
    else if (s[STAT_HUNGER] < 22) kind = PROB_HUNGRY;
    else if (s[STAT_HYGIENE] < 18) kind = PROB_DIRTY;
    else if (s[STAT_SOCIAL] < 18) kind = (r->persona[PERS_EXPRESSION] >= 8) ? PROB_WANTS_FRIEND : PROB_LONELY;
    else if (s[STAT_HAPPY] < 32) kind = PROB_BORED;
    else if (s[STAT_HAPPY] < 75 && sim_chance(3)) {
        kind = PROB_WANTS_ITEM;
        if (r->like_food < 12 && sim_chance(50)) item = ITEM_CUPCAKE + r->like_food;
        else {
            static const uint8_t gifts[] = { ITEM_PLUSHIE, ITEM_FLOWERS, ITEM_VIDEO_GAME,
                                             ITEM_SKULL_TRINKET, ITEM_PERFUME, ITEM_BOARD_GAME,
                                             ITEM_LOVE_LETTER };
            item = gifts[sim_rand(DOOMO_COUNTOF(gifts))];
        }
    }
    if (kind != PROB_NONE) sim_problem_raise(id, kind, -1, item);
}

/* ---------------------------------------------------------------------- */
/* news                                                                     */
int sim_news_count(void)
{
    sim_ensure_init();
    return sim_s.news_n;
}

const char *sim_news(int index)
{
    if (index < 0 || index >= sim_s.news_n) return "";
    return sim_s.news[(sim_s.news_head + index) % NEWS_MAX];
}

void sim_news_push(const char *msg)
{
    char *d;
    int i;
    if (!msg || !msg[0]) return;
    sim_ensure_init();
    sim_s.news_head = (uint8_t)((sim_s.news_head + NEWS_MAX - 1) % NEWS_MAX);
    d = sim_s.news[sim_s.news_head];
    for (i = 0; i < NEWS_LEN - 1 && msg[i]; i++) d[i] = msg[i];
    d[i] = 0;
    if (sim_s.news_n < NEWS_MAX) sim_s.news_n++;
    sim_s.stats.news++;
}

void sim_newsf(const char *fmt, ...)
{
    char buf[NEWS_LEN];
    va_list ap;
    va_start(ap, fmt);
    doomo_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    sim_news_push(buf);
}

static int random_citizen(int district)
{
    int tries;
    for (tries = 0; tries < 16; tries++) {
        int id = sim_rand(DOOMO_DISTRICT_SIZE) + (district < 0 ? sim_rand(2) : district) * DOOMO_DISTRICT_SIZE;
        if (sim_alive_ok(&doomo_town.res[id])) return id;
    }
    return -1;
}

void sim_news_fluff(void)
{
    static const char *const weather[] = {
        "Weather: 100%% chance of sprinkles over Hillvile.",
        "Weather: fluffy clouds, light giggling from the east.",
        "Weather: a gentle drizzle of glitter. Bring a brolly!",
        "Weather: rainbow spotted over the river. Twice!",
        "Hellvile forecast: sulfur fog, then more sulfur fog."
    };
    static const char *const odd[] = {
        "Hellvile Pits elevator broken again. Imps blame imps.",
        "Father Fuzz reminds everyone: hugs are free.",
        "Lost: one left sock. Last seen being haunted.",
        "Town Hall approves record budget for sprinkles.",
        "Mayor Giggs declares this week 'Extra Cozy Week'.",
        "Duck pond ducks demand better bread. Talks ongoing.",
        "Hellvile tax office opens 3 new queues. All slow.",
        "Glitter shortage at the workshop! Rivet in despair."
    };
    static const char *const night[] = {
        "Strange purple lights seen near the Gate...",
        "Residents report giggling coming from the wall.",
        "Someone left a vape pen by the river. Spooky.",
        "The moon looks extra round tonight. Suspicious."
    };
    int a, b, k = sim_rand(9);
    DoomoResident *ra, *rb;
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter == 3 && sim_chance(50)) {
        sim_newsf("%s", sim_chance(50) ? "DEMONS IN THE STREETS! Stay inside, hug a plush."
                                       : "Rescue squads needed! Grab a welded friend.");
        return;
    }
    if (town_is_night() && sim_chance(40)) {
        sim_newsf(night[sim_rand(DOOMO_COUNTOF(night))]);
        return;
    }
    a = random_citizen(-1);
    b = random_citizen(-1);
    if (a < 0 || b < 0 || a == b) k = sim_chance(50) ? 0 : 1;
    ra = a >= 0 ? &doomo_town.res[a] : NULL;
    rb = b >= 0 ? &doomo_town.res[b] : NULL;
    switch (k) {
    case 0: sim_newsf(weather[sim_rand(DOOMO_COUNTOF(weather))]); break;
    case 1: sim_newsf(odd[sim_rand(DOOMO_COUNTOF(odd))]); break;
    case 2:
        sim_newsf("Brimstone Bites adds %s to the menu!",
                  doomo_food_names[sim_rand(FOOD_COUNT)]);
        break;
    case 3:
        sim_newsf("Tonight at the concert hall: %s night!",
                  doomo_music_names[sim_concert_genre()]);
        break;
    case 4:
        sim_newsf("%s won the pie-eating contest. Again.", ra->name);
        break;
    case 5:
        sim_newsf("%s says: \"%s\"", ra->name,
                  doomo_catchphrases[ra->catchphrase % CATCHPHRASE_COUNT]);
        break;
    case 6:
        sim_newsf("%s is teaching a %s class!", ra->name,
                  doomo_hobby_names[ra->like_hobby % HOBBY_COUNT]);
        break;
    case 7:
        sim_newsf("%s spotted sharing %s with %s. Cute!", ra->name,
                  doomo_food_names[ra->like_food % FOOD_COUNT], rb->name);
        break;
    default:
        sim_newsf("%s and %s caught %s together.", ra->name, rb->name,
                  sim_chance(50) ? "stargazing" : "giggling at a cloud");
        break;
    }
}

void sim_news_daily(void)
{
    sim_newsf("Good morning! It's %s, day %d in Hellvile Park.",
              doomo_weekday_names[doomo_town.weekday % 7], (int)doomo_town.day + 1);
}
