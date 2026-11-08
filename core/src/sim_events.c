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
#include "doomo/playas.h"
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
    sim_s.nprob = 0;                     /* (the news feed persists: LS1) */
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
    rel_adjust(id, playas_slot(), 4);
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
    else if (s[STAT_SOCIAL] < 18) kind = (RES_PERSONA(r, PERS_EXPRESSION) >= 8) ? PROB_WANTS_FRIEND : PROB_LONELY;
    else if (s[STAT_HAPPY] < 32) kind = PROB_BORED;
    else if (s[STAT_HAPPY] < 75 && sim_chance(3)) {
        kind = PROB_WANTS_ITEM;
        if (RES_LIKE_FOOD(r) < 12 && sim_chance(50)) item = ITEM_CUPCAKE + RES_LIKE_FOOD(r);
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
/* news (LS1: records formatted at draw time)                               */
/* A headline is a DoomoNewsRec: a template id, a table index (arg8) and
 * three 10-bit resident ids or one number (v). The template's `k` string
 * says what each %s takes, in order:
 *   a b c  name of id slot 0 / 1 / 2       p  the possessor name of slot 1
 *   n      v as a number                   V  venue name (arg8 = ACT_*)
 *   F      food (arg8)    M  music genre (arg8)    K  catchphrase (arg8)
 *   H      hobby (arg8)   W  weekday (arg8)        D  death cause (arg8)
 * Only "%s" (and "%%") appear in the formats. */
typedef struct NewsTmpl { const char *fmt; const char *k; } NewsTmpl;

static const NewsTmpl news_tmpl[NT_COUNT] = {
    [NT_FREE]            = { "%s", "" },
    [NT_WELCOME_EMPTY]   = { "Welcome to your island! It's quiet... make some citizens!", "" },
    [NT_WELCOME_NEW]     = { "Welcome to Hellvile Park! Population: %s.", "n" },
    [NT_WELCOME_BACK]    = { "Welcome back! Day %s in Hellvile Park.", "n" },
    [NT_BIRTHDAY_HELL]   = { "Happy birthday, %s! Cake at Cloud Nine tonight!", "a" },
    [NT_BIRTHDAY_HILL]   = { "Happy birthday, %s! Cake at Brimstone Bites tonight!", "a" },
    [NT_POSSESSED]       = { "Uh oh! %s is acting strange... POSSESSED by %s!", "ap" },
    [NT_FUZZ_CHASED]     = { "Father Fuzz chased %s out of %s! Phew!", "pa" },
    [NT_STRANGER_CHASED] = { "A kind stranger chased %s out of %s! Phew!", "pa" },
    [NT_EXORCISM_OK]     = { "St. Cuddles rejoices! %s is free again!", "a" },
    [NT_EXORCISM_LOST]   = { "The exorcisms have failed... %s's soul is lost.", "a" },
    [NT_EXORCISM_FAIL]   = { "Exorcism failed! %s still has a demon inside.", "a" },
    [NT_VANISHED]        = { "%s has... vanished. Folks whisper about a MONSTER.", "a" },
    [NT_LAID_TO_REST]    = { "Farewell, %s. Laid to rest under the willow.", "a" },
    [NT_TRAGEDY]         = { "Tragedy: %s is gone (%s). Flowers at the cemetery.", "aD" },
    [NT_GHOST]           = { "Boo! %s is back... as a friendly GHOST!", "a" },
    [NT_DATING_GRUMBLE]  = { "%s is dating Grumbleguts. Gate's shut. Don't knock.", "a" },
    [NT_DATING_CROSS]    = { "Love over the wall! %s and %s are dating!", "ab" },
    [NT_DATING]          = { "%s confessed to %s... and it's a YES! New couple!", "ab" },
    [NT_HEARTBREAK]      = { "Heartbreak at %s: %s turned %s down.", "Vab" },
    [NT_WED_GRUMBLE]     = { "%s wed Grumbleguts! Suite closed till Monday.", "a" },
    [NT_WED_CROSS]       = { "WEDDING BELLS across the wall: %s + %s!", "ab" },
    [NT_WED]             = { "%s proposed to %s - and they said YES! Wedding!", "ab" },
    [NT_PROPOSAL_FIZZLED]= { "Ouch: %s's proposal to %s fizzled.", "ab" },
    [NT_SPLIT_MARRIED]   = { "%s and %s have split up. The cake is being returned.", "ab" },
    [NT_BROKE_UP]        = { "%s and %s broke up. Ice cream sales soar.", "ab" },
    [NT_ITS_OVER]        = { "It's over for %s and %s. Hugs needed.", "ab" },
    [NT_BABY_HELL]       = { "A hellspawn! %s and %s welcome little %s!", "abc" },
    [NT_BABY_HILL]       = { "An angel baby! %s and %s welcome little %s!", "abc" },
    [NT_CRUSH_RUMOUR]    = { "Rumour has it someone has a crush on %s...", "a" },
    [NT_BIG_FIGHT]       = { "Trouble in paradise? %s and %s had a big fight.", "ab" },
    [NT_REKINDLED]       = { "Back together! %s and %s rekindled their romance.", "ab" },
    [NT_NEMESES]         = { "%s and %s are now sworn NEMESES!", "ab" },
    [NT_RIVALS]          = { "Drama at %s! %s and %s are rivals now.", "Vab" },
    [NT_BFF_CROSS]       = { "%s and %s: best friends from both sides of the wall!", "ab" },
    [NT_BFF]             = { "%s and %s are now BEST FRIENDS!", "ab" },
    [NT_FRIENDS_CROSS]   = { "Hillvile meets Hellvile: %s and %s are friends!", "ab" },
    [NT_DEMONS]          = { "DEMONS IN THE STREETS! Stay inside, hug a plush.", "" },
    [NT_RESCUE]          = { "Rescue squads needed! Grab a welded friend.", "" },
    [NT_NIGHT0]          = { "Strange purple lights seen near the Gate...", "" },
    [NT_NIGHT1]          = { "Residents report giggling coming from the wall.", "" },
    [NT_NIGHT2]          = { "Someone left a vape pen by the river. Spooky.", "" },
    [NT_NIGHT3]          = { "The moon looks extra round tonight. Suspicious.", "" },
    [NT_WEATHER0]        = { "Weather: 100%% chance of sprinkles over Hillvile.", "" },
    [NT_WEATHER1]        = { "Weather: fluffy clouds, light giggling from the east.", "" },
    [NT_WEATHER2]        = { "Weather: a gentle drizzle of glitter. Bring a brolly!", "" },
    [NT_WEATHER3]        = { "Weather: rainbow spotted over the river. Twice!", "" },
    [NT_WEATHER4]        = { "Hellvile forecast: sulfur fog, then more sulfur fog.", "" },
    [NT_ODD0]            = { "Hellvile Pits elevator broken again. Imps blame imps.", "" },
    [NT_ODD1]            = { "Father Fuzz reminds everyone: hugs are free.", "" },
    [NT_ODD2]            = { "Lost: one left sock. Last seen being haunted.", "" },
    [NT_ODD3]            = { "Town Hall approves record budget for sprinkles.", "" },
    [NT_ODD4]            = { "Mayor Giggs declares this week 'Extra Cozy Week'.", "" },
    [NT_ODD5]            = { "Duck pond ducks demand better bread. Talks ongoing.", "" },
    [NT_ODD6]            = { "Hellvile tax office opens 3 new queues. All slow.", "" },
    [NT_ODD7]            = { "Glitter shortage at the workshop! Rivet in despair.", "" },
    [NT_MENU]            = { "Brimstone Bites adds %s to the menu!", "F" },
    [NT_CONCERT_NIGHT]   = { "Tonight at the concert hall: %s night!", "M" },
    [NT_PIE]             = { "%s won the pie-eating contest. Again.", "a" },
    [NT_SAYS]            = { "%s says: \"%s\"", "aK" },
    [NT_CLASS]           = { "%s is teaching a %s class!", "aH" },
    [NT_SHARING]         = { "%s spotted sharing %s with %s. Cute!", "aFb" },
    [NT_STARGAZING]      = { "%s and %s caught stargazing together.", "ab" },
    [NT_GIGGLING]        = { "%s and %s caught giggling at a cloud together.", "ab" },
    [NT_GOOD_MORNING]    = { "Good morning! It's %s, day %s in Hellvile Park.", "Wn" },
    [NT_WHEEL_SPOOKED]   = { "%s got spooked by the Wheel of Fate!", "a" },
    /* T0: Talk (docs/TALK.md 5.5); "yours" is the player's house */
    [NT_TALK_ROW]        = { "%s and %s had words at your place.", "ab" },
    [NT_TALK_MADE_UP]    = { "%s and %s made up at your place. Aww.", "ab" },
    [NT_TALK_GREAT_NIGHT] = { "Big night at yours! %s and %s still giggle.", "ab" },
    [NT_TALK_WALKOUT]    = { "%s walked out of a chat at yours. Bold.", "a" },
};

/* The feed lives outside sim_s: sim_init (a population change, a load)
 * keeps it; the LIFE chunk saves it; a new town clears it. */
static struct {
    DoomoNewsRec rec[NEWS_MAX];          /* newest first                   */
    char     pool[NEWS_FREE][NEWS_LEN];  /* free-text lines (NT_FREE)      */
    uint8_t  n;
    uint8_t  welcomed;                   /* sim_init's greeting is posted  */
} news_s;
static char news_out[2][NEWS_LEN];       /* sim_news's rotating buffers    */
static uint8_t news_flip;

static const char *table_name(const char *const *t, int n, int i)
{
    return (i >= 0 && i < n) ? t[i] : "";
}

void sim_news_format(const DoomoNewsRec *r, char *out, int cap)
{
    const NewsTmpl *t;
    const char *arg[4] = { "", "", "", "" };
    char num[12];
    int i;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!r || r->tmpl >= NT_COUNT) return;
    if (r->tmpl == NT_FREE) {
        doomo_snprintf(out, cap, "%s", r->arg8 < NEWS_FREE ? news_s.pool[r->arg8] : "");
        return;
    }
    t = &news_tmpl[r->tmpl];
    for (i = 0; i < 4 && t->k[i]; i++) {
        int a8 = r->arg8, id;
        switch (t->k[i]) {
        case 'a': case 'b': case 'c':
            id = (int)((r->v >> (10 * (t->k[i] - 'a'))) & 0x3FFu);
            arg[i] = id == (int)NEWS_NONE ? "Someone" : sim_name(id);
            break;
        case 'p':
            id = (int)((r->v >> 10) & 0x3FFu);
            arg[i] = sim_possessor_name_by(id == (int)NEWS_NONE ? -1 : id);
            break;
        case 'n': doomo_snprintf(num, sizeof num, "%d", (int)r->v); arg[i] = num; break;
        case 'V': arg[i] = sim_venue_name(a8); break;
        case 'F': arg[i] = table_name(doomo_food_names, FOOD_COUNT, a8); break;
        case 'M': arg[i] = table_name(doomo_music_names, MUSIC_COUNT, a8); break;
        case 'K': arg[i] = table_name(doomo_catchphrases, CATCHPHRASE_COUNT, a8); break;
        case 'H': arg[i] = table_name(doomo_hobby_names, HOBBY_COUNT, a8); break;
        case 'W': arg[i] = table_name(doomo_weekday_names, 7, a8); break;
        case 'D': arg[i] = table_name(doomo_death_names, DEATH_COUNT, a8); break;
        default: break;
        }
    }
    doomo_snprintf(out, cap, t->fmt, arg[0], arg[1], arg[2], arg[3]);
}

int sim_news_count(void)
{
    sim_ensure_init();
    return news_s.n;
}

const DoomoNewsRec *sim_news_record(int index)
{
    return (index >= 0 && index < news_s.n) ? &news_s.rec[index] : NULL;
}

const char *sim_news(int index)
{
    const DoomoNewsRec *r = sim_news_record(index);
    char *o;
    if (!r) return "";
    if (r->tmpl == NT_FREE && r->arg8 < NEWS_FREE) return news_s.pool[r->arg8];
    news_flip ^= 1;
    o = news_out[news_flip];
    sim_news_format(r, o, NEWS_LEN);
    return o;
}

static void news_remove(int i)
{
    for (; i + 1 < news_s.n; i++) news_s.rec[i] = news_s.rec[i + 1];
    news_s.n--;
}

static void news_add(int tmpl, int arg8, uint32_t v)
{
    int i;
    sim_ensure_init();
    i = news_s.n < NEWS_MAX ? news_s.n : NEWS_MAX - 1;
    for (; i > 0; i--) news_s.rec[i] = news_s.rec[i - 1];
    news_s.rec[0].tmpl = (uint8_t)tmpl;
    news_s.rec[0].arg8 = (uint8_t)arg8;
    news_s.rec[0].day = (uint16_t)doomo_town.day;
    news_s.rec[0].v = v;
    if (news_s.n < NEWS_MAX) news_s.n++;
    sim_s.stats.news++;
}

void sim_news_rec(int tmpl, int arg8, uint32_t v)
{
    if (tmpl <= NT_FREE || tmpl >= NT_COUNT) return;
    news_add(tmpl, arg8, v);
}

void sim_news_push(const char *msg)
{
    unsigned used = 0;
    int i, k, oldest = -1;
    if (!msg || !msg[0]) return;
    sim_ensure_init();
    for (i = 0; i < news_s.n; i++)
        if (news_s.rec[i].tmpl == NT_FREE && news_s.rec[i].arg8 < NEWS_FREE) {
            used |= 1u << news_s.rec[i].arg8;
            oldest = i;
        }
    for (k = 0; k < NEWS_FREE && (used & (1u << k)); k++) {}
    if (k == NEWS_FREE) {                 /* every line is in the feed    */
        k = news_s.rec[oldest].arg8;
        news_remove(oldest);
    }
    for (i = 0; i < NEWS_LEN - 1 && msg[i]; i++) news_s.pool[k][i] = msg[i];
    news_s.pool[k][i] = 0;
    news_add(NT_FREE, k, 0);
}

/* sim_init's greeting: once per town or load (sim_init also runs when the
 * population changes, which used to clear the feed). */
void sim_news_welcome(int pop)
{
    if (news_s.welcomed) return;
    news_s.welcomed = 1;
    if (pop == 0) sim_news_rec(NT_WELCOME_EMPTY, 0, 0);
    else if (doomo_town.day == 0) sim_news_rec(NT_WELCOME_NEW, 0, (uint32_t)pop);
    else sim_news_rec(NT_WELCOME_BACK, 0, doomo_town.day + 1u);
}

/* ---- the LIFE chunk's news section (life.c) ----
 * u8 n, u8 free lines, u16 0; n x 8 B records (tmpl, arg8, day u16 LE,
 * v u32 LE), newest first; then each free line as u8 length + bytes. */
void sim_news_clear(void)
{
    memset(&news_s, 0, sizeof(news_s));
}

uint32_t sim_news_save_size(void)
{
    uint32_t sz = 4u + 8u * news_s.n;
    int k;
    for (k = 0; k < NEWS_FREE; k++) sz += 1u + (uint32_t)strlen(news_s.pool[k]);
    return sz;
}

uint32_t sim_news_save(uint8_t *dst, uint32_t cap)
{
    uint32_t sz = sim_news_save_size(), o = 4, w;
    int i, k;
    if (!dst || cap < sz) return 0;
    dst[0] = news_s.n;
    dst[1] = NEWS_FREE;
    dst[2] = dst[3] = 0;
    for (i = 0; i < news_s.n; i++) {               /* explicit LE (save.h) */
        const DoomoNewsRec *r = &news_s.rec[i];
        dst[o++] = r->tmpl;
        dst[o++] = r->arg8;
        for (w = r->day, k = 0; k < 2; k++, w >>= 8) dst[o++] = (uint8_t)w;
        for (w = r->v, k = 0; k < 4; k++, w >>= 8) dst[o++] = (uint8_t)w;
    }
    for (k = 0; k < NEWS_FREE; k++) {
        uint32_t len = (uint32_t)strlen(news_s.pool[k]);
        dst[o++] = (uint8_t)len;
        memcpy(dst + o, news_s.pool[k], len);
        o += len;
    }
    return o;
}

int sim_news_load(const uint8_t *src, uint32_t len)
{
    uint32_t o = 4, l;
    int i, k, n;
    sim_news_clear();
    if (!src || len < 4 || src[0] > NEWS_MAX || 4u + 8u * src[0] > len) return 0;
    for (n = src[0], i = 0; i < n; i++, o += 8) {
        DoomoNewsRec *r = &news_s.rec[news_s.n];
        r->tmpl = src[o];
        r->arg8 = src[o + 1];
        r->day = (uint16_t)(src[o + 2] | (src[o + 3] << 8));
        for (r->v = 0, k = 3; k >= 0; k--) r->v = (r->v << 8) | src[o + 4 + k];
        if (r->tmpl < NT_COUNT && (r->tmpl != NT_FREE || r->arg8 < NEWS_FREE))
            news_s.n++;                   /* an unknown template is dropped */
    }
    for (k = 0; k < src[1] && o < len; k++, o += l) {
        l = src[o++];
        if (o + l > len) { sim_news_clear(); return 0; }
        if (k < NEWS_FREE) memcpy(news_s.pool[k], src + o, l < NEWS_LEN - 1 ? l : NEWS_LEN - 1);
    }
    news_s.welcomed = 0;                  /* "Welcome back!" after a load */
    return 1;
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
    int a, b, k = sim_rand(9);
    DoomoResident *ra;
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter == 3 && sim_chance(50)) {
        sim_news_rec(sim_chance(50) ? NT_DEMONS : NT_RESCUE, 0, 0);
        return;
    }
    if (town_is_night() && sim_chance(40)) {
        sim_news_rec(NT_NIGHT0 + sim_rand(4), 0, 0);
        return;
    }
    a = random_citizen(-1);
    b = random_citizen(-1);
    if (a < 0 || b < 0 || a == b) k = sim_chance(50) ? 0 : 1;
    ra = a >= 0 ? &doomo_town.res[a] : NULL;
    switch (k) {
    case 0: sim_news_rec(NT_WEATHER0 + sim_rand(5), 0, 0); break;
    case 1: sim_news_rec(NT_ODD0 + sim_rand(8), 0, 0); break;
    case 2:
        sim_news_rec(NT_MENU, sim_rand(FOOD_COUNT), 0);
        break;
    case 3:
        sim_news_rec(NT_CONCERT_NIGHT, sim_concert_genre(), 0);
        break;
    case 4:
        sim_news_rec(NT_PIE, 0, NEWS_IDS(a, NEWS_NONE, NEWS_NONE));
        break;
    case 5:
        sim_news_rec(NT_SAYS, RES_CATCHPHRASE(ra) % CATCHPHRASE_COUNT, NEWS_IDS(a, NEWS_NONE, NEWS_NONE));
        break;
    case 6:
        sim_news_rec(NT_CLASS, RES_LIKE_HOBBY(ra) % HOBBY_COUNT, NEWS_IDS(a, NEWS_NONE, NEWS_NONE));
        break;
    case 7:
        sim_news_rec(NT_SHARING, RES_LIKE_FOOD(ra) % FOOD_COUNT, NEWS_IDS(a, b, NEWS_NONE));
        break;
    default:
        sim_news_rec(sim_chance(50) ? NT_STARGAZING : NT_GIGGLING, 0, NEWS_IDS(a, b, NEWS_NONE));
        break;
    }
}

void sim_news_daily(void)
{
    sim_news_rec(NT_GOOD_MORNING, doomo_town.weekday % 7, doomo_town.day + 1u);
}
