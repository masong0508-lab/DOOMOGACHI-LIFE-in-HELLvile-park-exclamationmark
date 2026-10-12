/* sim_social.c - the relationship graph comes alive (owner: life).
 *
 *   meeting at the same venue -> acquaintance -> friend -> best friend
 *   (rel_adjust promotes / demotes the casual kinds and counts
 *   PROG_FRIENDSHIPS itself), arguments -> rivals -> nemeses (the player
 *   can make up), crushes (romance preference, pronoun groups, kids never,
 *   teens only with teens), confessions (player advice sways them),
 *   dating, proposals, weddings, breakups, exes, cross-border Hillvile <->
 *   Hellvile bonds, and - rarely - babies.
 *
 * Work is edge-centric: every game minute earns edge_count/60 "visits",
 * and sim_tick spends at most SIM_EDGE_BATCH of them, so each edge is
 * looked at about once per game hour. A visit only touches the two
 * residents of that edge. Meetings between strangers come from a tiny
 * ring of recent visitors per venue (sim_venue_meet), and at most two
 * new edges are created per tick (an insert into the sorted edge list is
 * a memmove; when the graph is full town.c evicts the weakest bond).
 */
#include "sim_int.h"

static int is_taken(int id) { return bit_get(sim_s.taken, id); }

int sim_partner_of(int id)
{
    int i;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS || !is_taken(id)) return -1;
    /* rare (UI) call: a linear scan over the graph is fine */
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int k = e->kind & REL_KIND_MASK;
        if (k != REL_DATING && k != REL_MARRIED) continue;
        if (e->a == id) return e->b;
        if (e->b == id) return e->a;
    }
    return -1;
}

void sim_taken_update(int id)
{
    bit_set(sim_s.taken, id, 1);
    bit_set(sim_s.taken, id, sim_partner_of(id) >= 0);
}

void sim_edge_remove(int index)
{
    int n = doomo_town.edge_count;
    if (index < 0 || index >= n) return;
    memmove(&doomo_town.edges[index], &doomo_town.edges[index + 1],
            (size_t)(n - index - 1) * sizeof(DoomoEdge));
    doomo_town.edge_count = (uint16_t)(n - 1);
}

void sim_social_init(void)
{
    int i;
    memset(sim_s.taken, 0, sizeof(sim_s.taken));
    memset(sim_s.crush, 0, sizeof(sim_s.crush));
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int k = e->kind & REL_KIND_MASK;
        if (k == REL_DATING || k == REL_MARRIED) {
            bit_set(sim_s.taken, e->a, 1);
            bit_set(sim_s.taken, e->b, 1);
        }
        if (e->kind & REL_CRUSH_AB) bit_set(sim_s.crush, e->a, 1);
        if (e->kind & REL_CRUSH_BA) bit_set(sim_s.crush, e->b, 1);
    }
    memcpy(sim_s.taken_nx, sim_s.taken, sizeof(sim_s.taken));
    memcpy(sim_s.crush_nx, sim_s.crush, sizeof(sim_s.crush));
    sim_s.edge_cursor = 0;
}

static int cross(int a, int b)
{
    return a < DOOMO_MAX_RESIDENTS && b < DOOMO_MAX_RESIDENTS &&
           RES_DISTRICT(a) != RES_DISTRICT(b);
}

/* Can the two sides of the wall mingle yet? Free play: always a little,
 * fully once the united park is unlocked; story: only after the wall
 * comes down (chapter 4). */
static int cross_border_pct(void)
{
    if (doomo_town.mode == TOWN_STORY)
        return doomo_town.story.chapter >= 4 ? 100 : doomo_town.story.chapter >= 2 ? 6 : 0;
    return unlock_mode_ok(MODE_UNITED_PARK) ? 100 : 25;
}

/* ---------------------------------------------------------------------- */
/* romance                                                                  */
int sim_confess(int from, int to, int bonus)
{
    DoomoResident *a = town_res(from), *b = town_res(to);
    int chance, ok, v;
    if (!a || !b || from >= DOOMO_MAX_RESIDENTS || to >= DOOMO_MAX_RESIDENTS) return 0;
    rel_set_crush(from, to, 0);
    sim_s.stats.confessions++;
    v = rel_value(from, to);
    chance = 12 + v / 2 + sim_compat(from, to) * 4 + bonus;
    if (rel_has_crush(to, from)) chance += 45;
    if (!sim_romance_ok(to, from) || !sim_romance_ok(from, to) || is_taken(to) || is_taken(from))
        chance = 0;
    chance = DOOMO_CLAMP(chance, 0, 97);
    ok = sim_chance(chance);
    {
        int pi;
        if ((pi = sim_problem_find(from, PROB_CRUSH)) >= 0) sim_problem_remove(pi);
        if ((pi = sim_problem_find(from, PROB_CONFESS_HELP)) >= 0) sim_problem_remove(pi);
    }
    if (ok) {
        rel_set_crush(to, from, 0);
        rel_set_kind(from, to, REL_DATING);
        rel_adjust(from, to, 12);
        bit_set(sim_s.taken, from, 1);
        bit_set(sim_s.taken, to, 1);
        bit_set(sim_s.crush, from, 0);
        bit_set(sim_s.crush, to, 0);
        sim_set_transient(from, TR_ECSTATIC, 10);
        sim_set_transient(to, TR_ECSTATIC, 10);
        sim_stat_add(a, STAT_HAPPY, 25);
        sim_stat_add(b, STAT_HAPPY, 20);
        sim_s.stats.dates++;
        if (from == ROSTER_ID_GRUMBLEGUTS || to == ROSTER_ID_GRUMBLEGUTS)
            sim_newsf("%s is dating Grumbleguts. Gate's shut. Don't knock.",
                      from == ROSTER_ID_GRUMBLEGUTS ? b->name : a->name);
        else if (cross(from, to))
            sim_newsf("Love over the wall! %s and %s are dating!", a->name, b->name);
        else
            sim_newsf("%s confessed to %s... and it's a YES! New couple!", a->name, b->name);
    } else {
        rel_adjust(from, to, -10);
        sim_set_transient(from, TR_SAD, 10);
        sim_stat_add(a, STAT_HAPPY, -20);
        sim_s.stats.rejections++;
        if (sim_chance(60))
            sim_newsf("Heartbreak at %s: %s turned %s down.", sim_venue_name(a->activity),
                      b->name, a->name);
    }
    bit_set(sim_s.crush, from, 0);
    return ok;
}

int sim_propose(int from, int to, int bonus)
{
    DoomoResident *a = town_res(from), *b = town_res(to);
    int chance, ok, v, pi;
    if (!a || !b || from >= DOOMO_MAX_RESIDENTS || to >= DOOMO_MAX_RESIDENTS) return 0;
    if (rel_kind(from, to) != REL_DATING) return 0;
    if ((pi = sim_problem_find(from, PROB_PROPOSAL)) >= 0) sim_problem_remove(pi);
    sim_s.stats.proposals++;
    v = rel_value(from, to);
    chance = v - 20 + sim_compat(from, to) * 3 + bonus;
    if (b->status != RES_ALIVE || a->status != RES_ALIVE) chance = 0;
    ok = sim_chance(DOOMO_CLAMP(chance, 3, 96));
    if (ok) {
        rel_set_kind(from, to, REL_MARRIED);
        rel_adjust(from, to, 15);
        sim_stat_add(a, STAT_HAPPY, 35);
        sim_stat_add(b, STAT_HAPPY, 35);
        sim_set_transient(from, TR_ECSTATIC, 15);
        sim_set_transient(to, TR_ECSTATIC, 15);
        sim_s.stats.marriages++;
        progress_add(PROG_MARRIAGES, 1);
        if (from == ROSTER_ID_GRUMBLEGUTS || to == ROSTER_ID_GRUMBLEGUTS)
            sim_newsf("%s wed Grumbleguts! Suite closed till Monday.",
                      from == ROSTER_ID_GRUMBLEGUTS ? b->name : a->name);
        else if (cross(from, to))
            sim_newsf("WEDDING BELLS across the wall: %s + %s!", a->name, b->name);
        else
            sim_newsf("%s proposed to %s - and they said YES! Wedding!", a->name, b->name);
    } else {
        rel_adjust(from, to, -20);
        sim_set_transient(from, TR_SAD, 12);
        sim_stat_add(a, STAT_HAPPY, -25);
        sim_newsf("Ouch: %s's proposal to %s fizzled.", a->name, b->name);
        if (rel_value(from, to) < 30 && sim_chance(50)) sim_breakup(from, to, 0);
    }
    return ok;
}

void sim_breakup(int a, int b, int quiet)
{
    DoomoResident *ra = town_res(a), *rb = town_res(b);
    int k = rel_kind(a, b), pi;
    if (!ra || !rb || (k != REL_DATING && k != REL_MARRIED)) return;
    rel_set_kind(a, b, REL_EX);
    rel_adjust(a, b, -10);
    bit_set(sim_s.taken, a, 0);
    bit_set(sim_s.taken, b, 0);
    sim_set_transient(a, TR_SAD, 12);
    sim_set_transient(b, TR_SAD, 12);
    sim_stat_add(ra, STAT_HAPPY, -20);
    sim_stat_add(rb, STAT_HAPPY, -20);
    if ((pi = sim_problem_find(a, PROB_BREAKUP)) >= 0) sim_problem_remove(pi);
    if ((pi = sim_problem_find(b, PROB_BREAKUP)) >= 0) sim_problem_remove(pi);
    sim_s.stats.breakups++;
    if (quiet) return;
    if (k == REL_MARRIED)
        sim_newsf("%s and %s have split up. The cake is being returned.", ra->name, rb->name);
    else if (sim_chance(50))
        sim_newsf("%s and %s broke up. Ice cream sales soar.", ra->name, rb->name);
    else
        sim_newsf("It's over for %s and %s. Hugs needed.", ra->name, rb->name);
}

/* ---------------------------------------------------------------------- */
/* babies                                                                   */
static uint8_t pick8(uint8_t a, uint8_t b) { return sim_chance(50) ? a : b; }

int sim_make_baby(int pa, int pb)
{
    DoomoResident *a = town_res(pa), *b = town_res(pb), baby;
    int district, slot, hellish;
    uint32_t seed;
    if (!a || !b || pa >= DOOMO_MAX_RESIDENTS || pb >= DOOMO_MAX_RESIDENTS) return -1;
    if (a->status == RES_EMPTY || b->status == RES_EMPTY) return -1;
    /* same district -> there; across the wall -> the more devilish parent
     * decides (hellspawn vs angel baby) */
    if (RES_DISTRICT(pa) == RES_DISTRICT(pb)) district = RES_DISTRICT(pa);
    else district = a->stat[STAT_CORRUPTION] + ((a->flags & RF_HELLBORN) ? 30 : 0) >=
                    b->stat[STAT_CORRUPTION] + ((b->flags & RF_HELLBORN) ? 30 : 0)
                    ? RES_DISTRICT(pa) : RES_DISTRICT(pb);
    slot = town_alloc(district);
    if (slot < 0) { district = !district; slot = town_alloc(district); }
    if (slot < 0) return -1;
    hellish = district == DISTRICT_HELLVILE;
    seed = doomo_hash32(doomo_town.seed ^ ((uint32_t)pa << 16) ^ (uint32_t)pb ^
                        (doomo_town.day * 7919u) ^ (uint32_t)sim_rand(65536));
    if (hellish) resident_generate_hellborn(&baby, seed);
    else resident_generate(&baby, seed);
    /* inherit: every feature comes from mum, dad or the dice */
    baby.look.species = pick8(a->look.species, b->look.species);
    baby.look.skin = pick8(a->look.skin, b->look.skin);
    baby.look.head_shape = pick8(a->look.head_shape, b->look.head_shape);
    baby.look.eye_style = pick8(a->look.eye_style, b->look.eye_style);
    baby.look.eye_color = pick8(a->look.eye_color, b->look.eye_color);
    baby.look.nose_style = pick8(a->look.nose_style, b->look.nose_style);
    baby.look.hair_color = pick8(a->look.hair_color, b->look.hair_color);
    baby.look.hair_highlight = pick8(a->look.hair_highlight, b->look.hair_highlight);
    baby.look.ears = pick8(a->look.ears, b->look.ears);
    baby.look.horns = pick8(a->look.horns, b->look.horns);
    baby.look.horn_color = pick8(a->look.horn_color, b->look.horn_color);
    baby.look.tail = pick8(a->look.tail, b->look.tail);
    baby.look.wings = pick8(a->look.wings, b->look.wings);
    baby.look.halo = pick8(a->look.halo, b->look.halo);
    baby.look.marking = pick8(a->look.marking, b->look.marking);
    baby.look.marking_color = pick8(a->look.marking_color, b->look.marking_color);
    baby.look.cheeks = (uint8_t)(CHEEK_BLUSH | ((a->look.cheeks | b->look.cheeks) & CHEEK_FRECKLES));
    /* babies are tiny, big-headed and big-eyed */
    baby.look.body = BODY_TINY;
    baby.look.height = (uint8_t)sim_rand(3);
    baby.look.build = (uint8_t)(6 + sim_rand(5));
    baby.look.head_size = (uint8_t)(12 + sim_rand(4));
    baby.look.eye_size = (uint8_t)(11 + sim_rand(5));
    baby.look.facial_hair = FHAIR_NONE;
    baby.look.hair_style = sim_chance(60) ? HAIR_BALD : HAIR_MESSY;
    baby.look.accessory = sim_chance(50) ? ACC_PACIFIER : ACC_NONE;
    baby.look.top = TOP_PAJAMAS;
    baby.look.hat = HAT_NONE;
    baby.romance = ROMANCE_NONE;                       /* far too young  */
    {
        int i;
        for (i = 0; i < PERS_COUNT; i++)
            baby.persona[i] = (uint8_t)DOOMO_CLAMP((a->persona[i] + b->persona[i]) / 2 +
                                                   sim_rand(5) - 2, 0, 15);
    }
    baby.status = RES_ALIVE;
    baby.flags = (uint8_t)(hellish ? RF_HELLBORN : 0);
    baby.stat[STAT_HUNGER] = 70; baby.stat[STAT_HAPPY] = 90; baby.stat[STAT_HYGIENE] = 90;
    baby.stat[STAT_ENERGY] = 80; baby.stat[STAT_HEALTH] = 100; baby.stat[STAT_SANITY] = 100;
    baby.stat[STAT_CORRUPTION] = (uint8_t)(hellish ? 20 : 0);
    baby.stat[STAT_SOCIAL] = 80;
    baby.exorcism = 0;
    baby.level = 0;
    baby.death_cause = DEATH_NONE;
    resident_sanitize(&baby);
    doomo_town.res[slot] = baby;
    sim_s.stamp[slot] = (uint16_t)sim_now();
    sim_s.transient[slot] = 0;
    sim_s.job[slot] = 0xFF;                         /* babies don't work */
    doomo_town.res[slot].activity = ACT_HOME;
    rel_set_kind(pa, slot, REL_FAMILY);
    rel_adjust(pa, slot, 85);
    rel_set_kind(pb, slot, REL_FAMILY);
    rel_adjust(pb, slot, 85);
    portrait_invalidate(slot);
    sim_set_transient(pa, TR_ECSTATIC, 15);
    sim_set_transient(pb, TR_ECSTATIC, 15);
    sim_s.stats.babies++;
    progress_add(PROG_BABIES, 1);
    sim_newsf("%s! %s and %s welcome little %s!",
              hellish ? "A hellspawn" : "An angel baby", a->name, b->name,
              doomo_town.res[slot].name);
    sim_mood_of(slot);
    return slot;
}

/* ---------------------------------------------------------------------- */
/* advice from the player                                                   */
const char *sim_advise_confess(int id, int target, int encourage)
{
    DoomoResident *r = town_res(id);
    int pi;
    if (!r || id >= DOOMO_MAX_RESIDENTS || !town_res(target)) return "...?";
    if (r->status != RES_ALIVE) return "...";
    if (!encourage) {
        rel_set_crush(id, target, 0);
        bit_set(sim_s.crush, id, 0);
        if ((pi = sim_problem_find(id, PROB_CRUSH)) >= 0) sim_problem_remove(pi);
        if ((pi = sim_problem_find(id, PROB_CONFESS_HELP)) >= 0) sim_problem_remove(pi);
        sim_stat_add(r, STAT_HAPPY, -5);
        sim_set_transient(id, TR_SAD, 3);
        return r->persona[PERS_EXPRESSION] >= 8 ? "You're right... I'll keep it in my heart. *sniff*"
                                                : "Okay. Forget I said anything.";
    }
    r->level = (uint8_t)DOOMO_MIN(99, r->level + 2);
    if (sim_confess(id, target, 18)) {
        sim_stat_add(r, STAT_HAPPY, 10);
        return r->persona[PERS_EXPRESSION] >= 8 ? "THEY SAID YES!! I'm floating!! Thank you!!"
                                                : "...They said yes. Thank you. Really.";
    }
    return r->persona[PERS_SPEECH] >= 10 ? "They said no. Great advice, genius."
                                         : "They said no... but thanks for believing in me.";
}

const char *sim_advise_propose(int id, int encourage)
{
    DoomoResident *r = town_res(id);
    int partner = sim_partner_of(id), pi;
    if (!r || partner < 0 || rel_kind(id, partner) != REL_DATING) return "Propose? To who?!";
    if (!encourage) {
        if ((pi = sim_problem_find(id, PROB_PROPOSAL)) >= 0) sim_problem_remove(pi);
        return "You're right. We'll take it slow.";
    }
    if (sim_propose(id, partner, 15))
        return r->persona[PERS_EXPRESSION] >= 8 ? "WE'RE GETTING MARRIED!! Will you come?!"
                                                : "We're engaged. I'm... very happy.";
    return "They weren't ready... I need a hug.";
}

const char *sim_advise_breakup(int id, int encourage)
{
    DoomoResident *r = town_res(id);
    int partner = sim_partner_of(id), pi;
    if (!r || partner < 0) return "We're... not even together?";
    if ((pi = sim_problem_find(id, PROB_BREAKUP)) >= 0) sim_problem_remove(pi);
    if (encourage) {
        rel_adjust(id, partner, 25 + sim_compat(id, partner) * 2);
        sim_stat_add(r, STAT_HAPPY, 10);
        return "You're right. I'll talk to them. We can fix this!";
    }
    sim_breakup(id, partner, 0);
    return "...Okay. It's better this way. I think.";
}

const char *sim_make_up(int a, int b)
{
    DoomoResident *ra = town_res(a), *rb = town_res(b);
    int k, c, pi;
    if (!ra || !rb || a == b) return "...?";
    if (!sim_alive_ok(ra) || !sim_alive_ok(rb)) return "It's too late for that...";
    k = rel_kind(a, b);
    c = sim_compat(a, b);
    if (k < 0) return "We don't even know each other!";
    if ((pi = sim_problem_find(a, PROB_FIGHT)) >= 0) sim_problem_remove(pi);
    if ((pi = sim_problem_find(b, PROB_FIGHT)) >= 0) sim_problem_remove(pi);
    if (sim_transient(a) == TR_ANGRY) sim_set_transient(a, TR_NONE, 0);
    if (sim_transient(b) == TR_ANGRY) sim_set_transient(b, TR_NONE, 0);
    if (k == REL_NEMESIS && c < 0 && sim_chance(50)) {
        rel_adjust(a, b, 15);
        return "Hmph. FINE. A truce. For now.";
    }
    rel_adjust(a, b, 28 + c * 3);
    sim_stat_add(ra, STAT_HAPPY, 8);
    sim_stat_add(rb, STAT_HAPPY, 8);
    ra->level = (uint8_t)DOOMO_MIN(99, ra->level + 1);
    if (k == REL_EX) return "We're better as friends. And that's okay!";
    if (k == REL_DATING || k == REL_MARRIED) return "I'm sorry, sweetie. Hug?";
    if (ra->persona[PERS_SPEECH] >= 10) return "Ugh, fine. Sorry. Happy now?";
    return "I'm sorry! Friends again? *pinky promise*";
}

const char *sim_introduce(int a, int b)
{
    DoomoResident *ra = town_res(a), *rb = town_res(b);
    int c, k;
    if (!ra || !rb || a == b || a >= DOOMO_MAX_RESIDENTS || b >= DOOMO_MAX_RESIDENTS)
        return "...?";
    if (!sim_alive_ok(ra) || !sim_alive_ok(rb)) return "They can't meet right now...";
    k = rel_kind(a, b);
    if (k >= 0 && k != REL_ACQUAINTANCE) return "We already know each other, silly!";
    c = sim_compat(a, b);
    rel_adjust(a, b, 10 + c * 3 + (k < 0 ? 0 : 4));
    sim_stat_add(ra, STAT_SOCIAL, 15);
    sim_stat_add(rb, STAT_SOCIAL, 15);
    {
        int pi;
        if ((pi = sim_problem_find(a, PROB_WANTS_FRIEND)) >= 0) sim_problem_remove(pi);
        if ((pi = sim_problem_find(a, PROB_LONELY)) >= 0) sim_problem_remove(pi);
    }
    sim_s.stats.meetings++;
    if (cross(a, b))
        return c >= 0 ? "From over the wall?! ...You seem really nice!"
                      : "A Hellvile type, huh. ...Hmph. Hi.";
    if (c >= 3) return "We like all the same things! Best friends?!";
    if (c >= 0) return "Nice to meet you!";
    return "Hmm. Hello, I suppose.";
}

/* ---------------------------------------------------------------------- */
/* meeting at a venue                                                       */
void sim_venue_meet(int id, DoomoResident *r, int minutes)
{
    int act = r->activity, slot, other, chance;
    DoomoResident *o;
    if (act >= ACT_COUNT) return;
    /* about two chats an hour at a busy venue */
    chance = minutes * 2 * 100 / 60;
    if (sim_chance(chance)) {
        slot = sim_rand(SIM_RING);
        other = sim_s.ring[act][slot];
        o = other < DOOMO_MAX_RESIDENTS ? &doomo_town.res[other] : NULL;
        if (o && other != id && o->activity == act && sim_alive_ok(o) &&
            (!cross(id, other) || sim_chance(cross_border_pct()))) {
            DoomoEdge *e = rel_find(id, other);
            int c = sim_compat(id, other);
            sim_s.ops++;
            sim_s.stats.chats++;
            if (e) {
                int d = 1 + (c > 0 ? c / 2 : 0);
                if (c < -2 && sim_chance(30)) d = -2;
                rel_adjust(id, other, d);
            } else if (sim_s.new_edges == 0 && doomo_town.edge_count < DOOMO_MAX_EDGES - 48 &&
                       sim_chance(c >= 3 ? 22 : c >= 0 ? 10 : 4)) {
                /* small talk turned into an actual acquaintance; the graph
                 * is kept below its cap (forgotten acquaintances are
                 * dropped in visit_edge) so town.c never has to evict */
                sim_s.new_edges++;
                sim_s.stats.meetings++;
                rel_adjust(id, other, 6 + c * 2 + sim_rand(5));
            }
            sim_stat_add(r, STAT_SOCIAL, 6);
            sim_stat_add(o, STAT_SOCIAL, 4);
            /* the possessed whisper to whoever sits next to them */
            if (r->status == RES_POSSESSED && o->status == RES_ALIVE) {
                sim_corrupt_natural(other, 3);
                sim_stat_add(o, STAT_SANITY, -3);
            }
        }
    }
    sim_s.ring[act][sim_s.ring_pos[act]] = (uint16_t)id;
    sim_s.ring_pos[act] = (uint8_t)((sim_s.ring_pos[act] + 1) % SIM_RING);
}

/* ---------------------------------------------------------------------- */
/* one edge                                                                 */
static void romance_step(DoomoEdge *e, int a, int b, DoomoResident *ra, DoomoResident *rb)
{
    int k = e->kind & REL_KIND_MASK;
    int pi;
    /* crushes bloom between good friends */
    if ((k == REL_FRIEND || k == REL_BEST_FRIEND) && e->value >= 40) {
        int dir;
        for (dir = 0; dir < 2; dir++) {
            int from = dir ? b : a, to = dir ? a : b;
            uint8_t fwd = dir ? REL_CRUSH_BA : REL_CRUSH_AB, back = dir ? REL_CRUSH_AB : REL_CRUSH_BA;
            uint32_t x;
            if ((e->kind & fwd) || is_taken(from) || bit_get(sim_s.crush, from)) continue;
            /* ~ a crush or two a day across the whole town; much likelier
             * when it could be mutual and the other one is single. The
             * roll comes first: the checks behind it are the costly part */
            x = sim_roll(100000u);
            if (x >= 90) continue;
            if (!sim_romance_ok(from, to)) continue;
            {
                int c = sim_compat(from, to);
                int w = 2 + (c > 0 ? c : 0) + (e->value >= 70 ? 2 : 0);
                if (sim_romance_ok(to, from) && !is_taken(to)) w *= 3;
                if (e->kind & back) w *= 3;
                if (x >= (uint32_t)w) continue;
            }
            rel_set_crush(from, to, 1);
            bit_set(sim_s.crush, from, 1);
            bit_set(sim_s.crush_nx, from, 1);
            sim_set_transient(from, TR_LOVESICK, 12);
            sim_s.stats.crushes++;
            if (sim_chance(55)) sim_problem_raise(from, PROB_CRUSH, to, 0);
            if (sim_chance(25))
                sim_newsf("Rumour has it someone has a crush on %s...", town_res(to)->name);
            return;
        }
    }
    /* crushes get confessed (or wait for the player's advice a while) */
    if (e->kind & (REL_CRUSH_AB | REL_CRUSH_BA)) {
        int from = (e->kind & REL_CRUSH_AB) ? a : b, to = from == a ? b : a;
        pi = sim_problem_find(from, PROB_CRUSH);
        if (pi < 0) pi = sim_problem_find(from, PROB_CONFESS_HELP);
        if (pi >= 0) {
            const DoomoProblem *p = sim_problem(pi);
            if (p->kind == PROB_CRUSH && p->age > 240) {
                /* the crush has grown: now they want advice */
                sim_problem_remove(pi);
                sim_problem_raise(from, PROB_CONFESS_HELP, to, 0);
            } else if (p->age > 600) {
                sim_confess(from, to, 0);
            }
        } else if (sim_rand(1000) < 15 && sim_alive_ok(town_res(to))) {
            sim_confess(from, to, 0);
        }
        return;
    }
    if ((k == REL_DATING || k == REL_MARRIED) &&
        sim_rand(2000) < (k == REL_MARRIED ? 3 : 8) - (sim_compat(a, b) >= 3 ? 1 : 0)) {
        /* every couple hits a rough patch now and then */
        rel_adjust(a, b, -(15 + sim_rand(20)));
        sim_set_transient(a, TR_ANGRY, 4);
        sim_set_transient(b, TR_SAD, 4);
        if (sim_s.rough_day < 2) {
            sim_s.rough_day++;
            sim_newsf("Trouble in paradise? %s and %s had a big fight.", ra->name, rb->name);
        }
        return;
    }
    if (k == REL_DATING) {
        if (e->value < 0) { sim_breakup(a, b, 0); return; }
        if (e->value < 15 && sim_problem_find(a, PROB_BREAKUP) < 0 && sim_chance(30)) {
            sim_problem_raise(a, PROB_BREAKUP, b, 0);
            return;
        }
        pi = sim_problem_find(a, PROB_PROPOSAL);
        if (pi < 0) pi = sim_problem_find(b, PROB_PROPOSAL);
        if (pi >= 0) {
            const DoomoProblem *p = sim_problem(pi);
            if (p->age > 480) sim_propose(p->resident, p->other, 0);
            return;
        }
        if (e->value >= 88 && sim_rand(1000) < 2) {
            int from = sim_chance(50) ? a : b;
            if (sim_problem_raise(from, PROB_PROPOSAL, from == a ? b : a, ITEM_RING) < 0)
                sim_propose(from, from == a ? b : a, 0);
        }
        return;
    }
    if (k == REL_MARRIED) {
        if (e->value < -20) { sim_breakup(a, b, 0); return; }
        if (ra->status == RES_ALIVE && rb->status == RES_ALIVE && e->value >= 50 &&
            !sim_is_kid(a) && !sim_is_kid(b) && !sim_is_teen(a) && !sim_is_teen(b) &&
            sim_rand(5000) == 0)
            sim_make_baby(a, b);
        return;
    }
    if (k == REL_EX && e->value >= 60 && ra->activity == rb->activity && !is_taken(a) &&
        !is_taken(b) && sim_romance_ok(a, b) && sim_romance_ok(b, a) && sim_chance(5)) {
        rel_set_kind(a, b, REL_DATING);
        bit_set(sim_s.taken, a, 1);
        bit_set(sim_s.taken, b, 1);
        sim_newsf("Back together! %s and %s rekindled their romance.", ra->name, rb->name);
    }
}

static void visit_edge(int index)
{
    DoomoEdge *e = &doomo_town.edges[index];
    int a = e->a, b = e->b, k = e->kind & REL_KIND_MASK, same, night;
    DoomoResident *ra, *rb;
    sim_s.ops++;
    /* rebuild the bitsets as we go (self-healing after edits elsewhere) */
    if (k == REL_DATING || k == REL_MARRIED) {
        bit_set(sim_s.taken_nx, a, 1);
        bit_set(sim_s.taken_nx, b, 1);
    }
    if (e->kind & REL_CRUSH_AB) bit_set(sim_s.crush_nx, a, 1);
    if (e->kind & REL_CRUSH_BA) bit_set(sim_s.crush_nx, b, 1);
    if (a >= DOOMO_MAX_RESIDENTS || b >= DOOMO_MAX_RESIDENTS) return;   /* the player */
    ra = &doomo_town.res[a];
    rb = &doomo_town.res[b];
    if (ra->status == RES_EMPTY || rb->status == RES_EMPTY) return;
    if (ra->status == RES_DEAD || rb->status == RES_DEAD) return;
    night = sim_s.night;

    /* the possessed haunt the people closest to them */
    if (night && e->value >= 15) {
        int pa = ra->status == RES_POSSESSED || ra->status == RES_SOUL_LOST;
        int pb = rb->status == RES_POSSESSED || rb->status == RES_SOUL_LOST;
        if (pa != pb) {
            DoomoResident *v = pa ? rb : ra;
            int vid = pa ? b : a;
            if (v->status == RES_ALIVE && sim_chance(50)) {
                sim_corrupt_natural(vid, 1 + (e->value >= 60));
                sim_stat_add(v, STAT_SANITY, -2);
                if (v->activity == ACT_SLEEPING && sim_chance(4) && sim_problem_find(vid, -1) < 0) {
                    sim_problem_raise(vid, PROB_NIGHTMARE, pa ? a : b, 0);
                    sim_s.stats.nightmares++;
                }
            }
        }
    }
    /* nemeses fuel each other's darkness */
    if (k == REL_NEMESIS && night && sim_chance(30)) {
        sim_corrupt_natural(a, 1);
        sim_corrupt_natural(b, 1);
    }
    if (!sim_alive_ok(ra) || !sim_alive_ok(rb)) return;

    same = ra->activity == rb->activity && sim_is_venue(ra->activity);
    if (same) {
        int c = sim_compat(a, b);
        int argue = 2 + (ra->persona[PERS_SPEECH] + rb->persona[PERS_SPEECH]) / 8 - c * 2;
        if (k == REL_RIVAL) argue += 15;
        else if (k == REL_NEMESIS) argue += 25;
        else if (k == REL_FAMILY || k == REL_MARRIED || k == REL_BEST_FRIEND) argue /= 2;
        if (sim_transient(a) == TR_ANGRY || sim_transient(b) == TR_ANGRY) argue += 6;
        if (sim_chance(DOOMO_CLAMP(argue, 0, 60))) {
            int before = k;
            rel_adjust(a, b, -(10 + sim_rand(10)));
            sim_set_transient(a, TR_ANGRY, 3);
            sim_set_transient(b, TR_ANGRY, 3);
            sim_stat_add(ra, STAT_HAPPY, -6);
            sim_stat_add(rb, STAT_HAPPY, -6);
            sim_s.stats.arguments++;
            k = rel_kind(a, b);
            if (rel_value(a, b) < 25 && sim_problem_find(a, -1) < 0 && sim_chance(50))
                sim_problem_raise(a, PROB_FIGHT, b, 0);
            if (k != before && (k == REL_RIVAL || k == REL_NEMESIS) && sim_s.args_hour < 2) {
                sim_s.args_hour++;
                if (k == REL_NEMESIS)
                    sim_newsf("%s and %s are now sworn NEMESES!", ra->name, rb->name);
                else
                    sim_newsf("Drama at %s! %s and %s are rivals now.",
                              sim_venue_name(ra->activity), ra->name, rb->name);
            }
            return;
        }
        {
            int before = k, d = 1 + (c > 0 ? c / 2 : 0) + (ra->like_hobby == rb->like_hobby);
            rel_adjust(a, b, d);
            sim_stat_add(ra, STAT_SOCIAL, 3);
            sim_stat_add(rb, STAT_SOCIAL, 3);
            k = rel_kind(a, b);
            if (k == REL_BEST_FRIEND && before != REL_BEST_FRIEND && sim_chance(40)) {
                if (cross(a, b))
                    sim_newsf("%s and %s: best friends from both sides of the wall!",
                              ra->name, rb->name);
                else
                    sim_newsf("%s and %s are now BEST FRIENDS!", ra->name, rb->name);
            }
            if (k == REL_FRIEND && before == REL_ACQUAINTANCE && cross(a, b) && sim_chance(50))
                sim_newsf("Hillvile meets Hellvile: %s and %s are friends!",
                          RES_DISTRICT(a) ? rb->name : ra->name, RES_DISTRICT(a) ? ra->name : rb->name);
        }
    } else {
        /* out of sight, slowly out of mind */
        if (k == REL_ACQUAINTANCE && sim_chance(35)) {
            if (e->value != 0) {
                e->value = (int8_t)(e->value + (e->value > 0 ? -1 : 1));
            } else if (!(e->kind & (REL_CRUSH_AB | REL_CRUSH_BA))) {
                /* forgotten: make room for new faces */
                sim_edge_remove(index);
                sim_s.stats.forgotten++;
                return;
            }
        } else if ((k == REL_FRIEND || k == REL_BEST_FRIEND) && sim_chance(3))
            rel_adjust(a, b, -1);
        else if ((k == REL_RIVAL || k == REL_EX) && sim_chance(8))
            rel_adjust(a, b, 1);
        else if (k == REL_MARRIED || k == REL_FAMILY) {
            if (sim_chance(20)) { sim_stat_add(ra, STAT_HAPPY, 1); sim_stat_add(rb, STAT_HAPPY, 1); }
        }
    }
    if (ra->status == RES_ALIVE && rb->status == RES_ALIVE)
        romance_step(&doomo_town.edges[index], a, b, ra, rb);
}

void sim_edges_step(int max_visits)
{
    int n = 0;
    while (n < max_visits && sim_s.edge_credit >= 60) {
        int count = doomo_town.edge_count;
        if (count <= 0) { sim_s.edge_credit = 0; break; }
        if (sim_s.edge_cursor >= count) {
            /* a full pass is done: swap in the rebuilt bitsets */
            memcpy(sim_s.taken, sim_s.taken_nx, sizeof(sim_s.taken));
            memcpy(sim_s.crush, sim_s.crush_nx, sizeof(sim_s.crush));
            memset(sim_s.taken_nx, 0, sizeof(sim_s.taken_nx));
            memset(sim_s.crush_nx, 0, sizeof(sim_s.crush_nx));
            sim_s.edge_cursor = 0;
        }
        visit_edge(sim_s.edge_cursor);
        sim_s.edge_cursor++;
        sim_s.edge_credit -= 60;
        n++;
    }
}
