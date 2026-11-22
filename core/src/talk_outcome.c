/* talk_outcome.c - T6-OUT: what a conversation does to the sim.
 * Owner: TK. Design 5.5; docs/TALK.md 6g.
 *
 * talk_dm_end() hands a TalkSummary and applies nothing. This file applies it:
 *   bond with the player  rel_adjust(avatar, id, d)  +-12 a resident a town day,
 *                         friendship level +3 a day, family_care for a family member
 *   resident-resident     rel_adjust(a, b, d)        +-8 a pair a day
 *   moods                 STAT_HAPPY +-12 a day, STAT_SOCIAL +4..+12 by engagement (12 a day)
 *   problems              sim_problem_resolve, one a resident a day
 *   mediation             sim_make_up(a, b), one a pair a day
 *   news                  NT_TALK_*, one headline a conversation, none for a row of minors
 *   romance               only ever ASKED: a yes to CONFESS / ASK_OUT calls sim_confess,
 *                         once a pair a day; the sim owns every crush bit and dating edge
 *   actions (TM2e)        TEV_ACT_INVITE sim_nudge_activity (1 a resident, 3 a town day), TEV_ACT_GIFT inv_add
 *                         (1 and 3), TEV_ACT_ASK sim_problem_raise(PROB_WANTS_ITEM) (1 a resident, only without an
 *                         open problem), TEV_ACT_CALL sim_introduce for strangers (the scene lets them in).
 *                         talk_outcome_can asks the same caps at plan time, so a line never promises what the
 *                         caps would deny; the apply re-checks the minor rules and counts what it refuses
 *                         (TalkOutcome.skipped: a test fails on it)
 * A ledger of the day (keyed by town day and seed) keeps the caps across conversations,
 * so no number of talks farms more than the caps. The same-day invitations
 * are saved. Other daily totals live in the current session.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/items.h"
#include "sim_int.h"

#define LED_P 64
#define LED_Q 64
#define LPF_PROBLEM 0x01u          /* a problem was resolved today */
#define LPF_CARE    0x02u          /* family_care was given today  */
#define LPF_GIFT    0x08u          /* TM2e: the resident gave a gift today */
#define LPF_ASK     0x10u          /* TM2e: the resident asked for an item today */
#define LQF_MADE_UP 0x01u
#define LQF_ASKED   0x02u          /* sim_confess was asked today  */

typedef struct { uint16_t id; int8_t rel, mood; uint8_t social, level, flags; } LedP;   /* 8 B */
typedef struct { uint16_t a, b; int8_t d; uint8_t flags; } LedQ;                              /* 6 B */
typedef struct { uint16_t guest, avatar; uint8_t venue; } LedMeet;   /* venue+1, or 0 if used */
static struct {
    uint32_t day, seed;
    uint8_t  live, np, nq, rp, rq, news;
    uint8_t  invn, giftn;          /* TM2e: invitations taken and gifts given today (the town caps); in the struct's padding */
    LedP p[LED_P];
    LedQ q[LED_Q];
    LedMeet meet[TALK_CAP_INVITE_TOWN];
} led;

void talk_outcome_reset(void) { memset(&led, 0, sizeof led); }

static void roll(void)
{
    if (led.live && led.day == doomo_town.day && led.seed == doomo_town.seed) return;
    memset(&led, 0, sizeof led);
    led.live = 1;
    led.day = doomo_town.day;
    led.seed = doomo_town.seed;
}

static LedP *led_p(int id)
{
    int i;
    for (i = 0; i < led.np; i++) if (led.p[i].id == (uint16_t)id) return &led.p[i];
    if (led.np < LED_P) i = led.np++;
    else { i = led.rp; led.rp = (uint8_t)((led.rp + 1) % LED_P); }
    memset(&led.p[i], 0, sizeof led.p[i]);
    led.p[i].id = (uint16_t)id;
    return &led.p[i];
}

static const LedP *led_peek(int id)
{
    int i;
    for (i = 0; i < led.np; i++) if (led.p[i].id == (uint16_t)id) return &led.p[i];
    return NULL;
}

static LedQ *led_q(int a, int b)
{
    int i;
    if (a > b) { int t = a; a = b; b = t; }
    for (i = 0; i < led.nq; i++) if (led.q[i].a == (uint16_t)a && led.q[i].b == (uint16_t)b) return &led.q[i];
    if (led.nq < LED_Q) i = led.nq++;
    else { i = led.rq; led.rq = (uint8_t)((led.rq + 1) % LED_Q); }
    memset(&led.q[i], 0, sizeof led.q[i]);
    led.q[i].a = (uint16_t)a;
    led.q[i].b = (uint16_t)b;
    return &led.q[i];
}

/* add `want` to a signed total that stays within +-cap; returns what was really added */
static int take_signed(int8_t *used, int want, int cap)
{
    int v = *used + want, d;
    if (v > cap) v = cap;
    if (v < -cap) v = -cap;
    d = v - *used;
    *used = (int8_t)v;
    return d;
}

/* add `want` (>= 0) to an unsigned total up to cap; returns what was really added */
static int take_up(uint8_t *used, int want, int cap)
{
    int d = want;
    if (*used + d > cap) d = cap - *used;
    if (d < 0) d = 0;
    *used = (uint8_t)(*used + d);
    return d;
}

static int is_pet_id(int id) { return id == TALK_PET_ID || talk_safe_is_pet(id); }
static int resident_id(int id) { return id >= 0 && id < DOOMO_MAX_RESIDENTS && !is_pet_id(id); }
static int pool_id(int id) { return id > DOOMO_PLAYER_ID && !is_pet_id(id); }

static int slot_ok(const TalkSummary *s, int k) { return k >= 0 && k < s->n && k < TALK_PARTS_MAX; }

/* the n-th pair (0,1) (0,2) (0,3) (1,2) (1,3) (2,3) of TalkSummary.pair_d */
static void pair_slots(int idx, int *i, int *j)
{
    static const uint8_t pi[6] = { 0, 0, 0, 1, 1, 2 }, pj[6] = { 1, 2, 3, 2, 3, 3 };
    *i = pi[idx];
    *j = pj[idx];
}

static int pair_index(int i, int j)
{
    int k, a, b;
    for (k = 0; k < 6; k++) { pair_slots(k, &a, &b); if ((a == i && b == j) || (a == j && b == i)) return k; }
    return -1;
}

static int minor_id(int id) { return talk_safe_minor(id); }

/* a headline, once per conversation, never about a pair of minors; the row and the
 * walk-out need adults (the ticker would otherwise name a child in a quarrel) */
static void post_news(TalkOutcome *out, int tmpl, int ida, int idb)
{
    if (out->news || led.news >= TALK_CAP_NEWS_DAY) return;      /* one a conversation, a few a day: the ticker is not Talk's */
    if (!resident_id(ida) || (idb >= 0 && !resident_id(idb))) return;
    if (tmpl == NT_TALK_ROW && (minor_id(ida) || (idb >= 0 && minor_id(idb)))) return;
    if (tmpl == NT_TALK_WALKOUT && minor_id(ida)) return;
    if (tmpl != NT_TALK_ROW && tmpl != NT_TALK_WALKOUT && idb >= 0 && minor_id(ida) && minor_id(idb)) return;
    sim_news_rec(tmpl, 0, NEWS_IDS(ida, idb >= 0 ? idb : (int)NEWS_NONE, (int)NEWS_NONE));
    out->news = (uint8_t)tmpl;
    led.news++;
}

static int alive_resident(int id)
{
    const DoomoResident *r = resident_id(id) ? town_res(id) : NULL;
    return r && (r->status == RES_ALIVE);
}

static LedMeet *meet_find(int guest)
{
    int i;
    for (i = 0; i < led.invn; i++) if (led.meet[i].guest == guest) return &led.meet[i];
    return NULL;
}

void talk_outcome_meet_forget(int id)
{
    int i;
    /* Cancel the booking while keeping the invitations already counted today. */
    for (i = 0; i < led.invn; i++)
        if (led.meet[i].guest == id || led.meet[i].avatar == id) led.meet[i].venue = 0;
}

static int meet_avatar_ok(int id)
{
    if (id >= FAMILY_POOL_FIRST && id <= FAMILY_POOL_LAST) {
        const FamilyMember *m = family_find(id);
        return m && m->role != FAMROLE_PET;
    }
    return (id == DOOMO_PLAYER_ID || resident_id(id)) && sim_alive_ok(town_res(id));
}

/* A u16 town day followed by three u24 records. The saved avatar can be
 * the hero or a household member. The zero record leaves the slot empty. */
#define MEET_AVATAR_MAX (FAMILY_POOL_FIRST + HOUSEHOLD_CAP_MAX - 1)
DOOMO_STATIC_ASSERT(MEET_AVATAR_MAX < 1024, meet_avatar_fits);
DOOMO_STATIC_ASSERT(ACT_COUNT < 16, meet_venue_fits);

uint32_t talk_outcome_meet_save(uint8_t *dst)
{
    int i;
    if (!led.live || !led.invn || led.day != doomo_town.day || led.seed != doomo_town.seed) return 0;
    if (dst) {
        memset(dst, 0, TALK_MEET_SAVE_BYTES);
        dst[0] = (uint8_t)led.day;
        dst[1] = (uint8_t)(led.day >> 8);
        for (i = 0; i < led.invn; i++) {
            const LedMeet *m = &led.meet[i];
            uint32_t rec = m->guest | ((uint32_t)m->avatar << 10) | ((uint32_t)m->venue << 20);
            uint8_t *p = dst + 2 + 3 * i;
            p[0] = (uint8_t)rec;
            p[1] = (uint8_t)(rec >> 8);
            p[2] = (uint8_t)(rec >> 16);
        }
    }
    return TALK_MEET_SAVE_BYTES;
}

int talk_outcome_meet_load(const uint8_t *src)
{
    int i;
    roll();
    led.invn = 0;
    if (((unsigned)src[0] | (unsigned)src[1] << 8) != (uint16_t)doomo_town.day) return 1;
    for (i = 0; i < TALK_CAP_INVITE_TOWN; i++) {
        const uint8_t *p = src + 2 + 3 * i;
        uint32_t rec = p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
        int guest = rec & 1023u, avatar = (rec >> 10) & 1023u, venue = rec >> 20;
        const DoomoResident *owner;
        LedMeet *m;
        if (!rec) continue;
        if (guest >= DOOMO_MAX_RESIDENTS || avatar > MEET_AVATAR_MAX || guest == avatar ||
            meet_find(guest) || (venue && !talk_act_invite_ok(TA_INVITE, venue - 1, 0, 12)))
            return 0;
        m = &led.meet[led.invn++];
        m->guest = (uint16_t)guest;
        m->avatar = (uint16_t)avatar;
        /* An older save may still hold a booking for a vacant ID. */
        owner = town_res(avatar);
        m->venue = town_res(guest)->status != RES_EMPTY &&
                   (owner ? owner->status != RES_EMPTY : family_find(avatar) != NULL) ? (uint8_t)venue : 0;
    }
    return 1;
}

void talk_outcome_meet_capture(uint8_t *activities)
{
    int i;
    roll();
    for (i = 0; i < led.invn; i++) {
        const LedMeet *m = &led.meet[i];
        activities[i] = m->venue && alive_resident(m->guest) ? town_res(m->guest)->activity : ACT_COUNT;
    }
}

void talk_outcome_meet_restore(const uint8_t *activities)
{
    int i;
    for (i = 0; i < led.invn; i++) {
        if (activities[i] < ACT_COUNT) town_res(led.meet[i].guest)->activity = activities[i];
    }
}

/* ---- TM2e: the caps of the actions, asked at plan time and again at apply ---- */
static int cap_flag(int kind)
{
    return kind == TEV_ACT_GIFT ? LPF_GIFT : kind == TEV_ACT_ASK ? LPF_ASK : 0;
}

static int cap_town(int kind)
{
    return kind == TEV_ACT_INVITE ? led.invn : kind == TEV_ACT_GIFT ? led.giftn : 0;
}

static int cap_town_max(int kind)
{
    return kind == TEV_ACT_INVITE ? TALK_CAP_INVITE_TOWN : kind == TEV_ACT_GIFT ? TALK_CAP_GIFT_TOWN : 1000;
}

static int capped_resident(int kind, int id)
{
    const LedP *lp;
    if (kind == TEV_ACT_INVITE) return meet_find(id) != NULL;
    lp = led_peek(id);
    return lp && (lp->flags & cap_flag(kind));
}

/* A dead/missing resident must never appear able to offer a real action.
 * The apply path has always rejected them; keep the UI/plan gate in sync. */
int talk_outcome_can(int kind, int id)
{
    if ((kind != TEV_ACT_INVITE && !cap_flag(kind)) || !alive_resident(id)) return 0;
    roll();
    return !capped_resident(kind, id) && cap_town(kind) < cap_town_max(kind);
}

int talk_outcome_why_in(const TalkState *s, int kind, int id)
{
    int k, mine = 0, all = 0;
    if (!s || (kind != TEV_ACT_INVITE && !cap_flag(kind)) || !alive_resident(id)) return TAW_RULE;
    if (kind == TEV_ACT_INVITE && (s->conv.avatar == id || !meet_avatar_ok(s->conv.avatar))) return TAW_RULE;
    roll();
    for (k = 0; k < s->nev && k < TALK_EVENTS_MAX; k++) {
        if (s->ev[k].kind != kind) continue;
        all++;
        if (s->ev[k].a < s->nparts && s->part[s->ev[k].a].id == id) mine++;
    }
    if (capped_resident(kind, id) || mine) return TAW_CAP_DAY;
    return cap_town(kind) + all >= cap_town_max(kind) ? TAW_CAP_TOWN : TAW_NONE;
}

int talk_outcome_can_in(const TalkState *s, int kind, int id) { return talk_outcome_why_in(s, kind, id) == TAW_NONE; }

/* the stage rules again, from the summary: is anyone in the room (or the player) a minor */
static int minor_room_of(const TalkSummary *sum)
{
    int k;
    if (sum->avatar >= 0 && minor_id(sum->avatar)) return 1;
    for (k = 0; k < sum->n && k < TALK_PARTS_MAX; k++) if (minor_id(sum->id[k])) return 1;
    return 0;
}

static void apply_action(const TalkSummary *sum, const TalkEvent *e, int ia, TalkOutcome *out)
{
    LedP *lp;
    if (ia < 0 || !alive_resident(ia)) { out->skipped++; return; }
    if (!talk_outcome_can(e->kind, ia)) { out->skipped++; return; }
    lp = led_p(ia);
    switch (e->kind) {
    case TEV_ACT_INVITE:
        if (sum->avatar != ia && meet_avatar_ok(sum->avatar) &&
            talk_act_invite_ok(TA_INVITE, e->b, minor_room_of(sum), town_hour()) &&
            sim_nudge_activity(ia, e->b, 60)) {
            LedMeet *m = &led.meet[led.invn++];
            m->guest = (uint16_t)ia;
            m->avatar = (uint16_t)sum->avatar;
            m->venue = (uint8_t)(e->b + 1);
            out->invited++;
            /* An acceptance is a remembered *invitation*, not a
             * fabricated claim that the player already travelled. */
            talk_mem_note_shared(ia, TALK_SHARED_INVITED);
        } else out->skipped++;
        break;
    case TEV_ACT_GIFT: {
        int before;
        if (e->b >= ITEM_COUNT || !talk_act_gift_ok(e->b, minor_id(ia), minor_room_of(sum))) { out->skipped++; break; }
        before = inv_count(e->b);
        if (before < 99 && inv_add(e->b, 1) == before + 1) {
            lp->flags |= LPF_GIFT;
            led.giftn++;
            out->gifted++;
        } else out->skipped++;
        break;
    }
    case TEV_ACT_ASK:
        if (e->b < ITEM_COUNT && talk_act_gift_ok(e->b, minor_id(ia), minor_room_of(sum)) && sim_can_raise_problem(ia) &&
            sim_problem_raise(ia, PROB_WANTS_ITEM, -1, e->b) >= 0) {
            lp->flags |= LPF_ASK;
            out->asked++;
        } else out->skipped++;
        break;
    default: break;
    }
}

/* The day-scoped reservation belongs to a real person and real location.
 * Unlike meet_present, this reports a missed/pending appointment too, so the
 * Talk journal can explain why no guest appeared at the expected place.
 * Queried only on menu construction and selection. */
int talk_outcome_meet_plan(int guest_id)
{
    const LedMeet *m;
    if (!alive_resident(guest_id)) return -1;
    roll();
    m = meet_find(guest_id);
    return m && m->avatar == playas_avatar() ? (int)m->venue - 1 : -1;
}

static int meet_present(const LedMeet *m, int venue, int avatar)
{
    return m && m->venue == venue + 1 && m->avatar == avatar && meet_avatar_ok(avatar) &&
           alive_resident(m->guest) &&
           town_res(m->guest)->activity == venue &&
           talk_act_invite_ok(TA_INVITE, venue, minor_id(avatar) || minor_id(m->guest), town_hour());
}

int talk_outcome_meet_present(int guest_id, int venue)
{
    if (venue < 0 || venue >= ACT_COUNT) return 0;
    roll();
    return meet_present(meet_find(guest_id), venue, playas_avatar());
}

int talk_outcome_meet_guests(int venue, int *guests, int capacity)
{
    int i, count = 0, avatar = playas_avatar();
    if (venue < 0 || venue >= ACT_COUNT) return 0;
    roll();
    for (i = 0; i < led.invn && count < capacity; i++) {
        const LedMeet *m = &led.meet[i];
        if (meet_present(m, venue, avatar)) guests[count++] = m->guest;
    }
    return count;
}

int talk_outcome_meet_ready(int venue)
{
    int guest;
    return talk_outcome_meet_guests(venue, &guest, 1) ? guest : -1;
}

/* Spending time at a real place should feel different to merely chatting.
 * This work is performed once, after the player reaches the accepted venue:
 * no per-frame updates, scene allocations, or new resident/save fields. */
static void meet_local_activity(DoomoResident *who, int venue)
{
    if (!who || who->status != RES_ALIVE) return;
    switch (venue) {
    case ACT_CAFE:       /* a purchased snack restores hunger */
        sim_stat_add(who, STAT_HUNGER, 18);
        sim_stat_add(who, STAT_ENERGY, 2);
        break;
    case ACT_CHURCH:     /* a reflective visit */
        sim_stat_add(who, STAT_SANITY, 12);
        break;
    case ACT_CONCERT:    /* music is exciting, but a little tiring */
        sim_stat_add(who, STAT_HAPPY, 12);
        sim_stat_add(who, STAT_ENERGY, -4);
        break;
    case ACT_WORKSHOP:   /* tinkering has an energy cost */
        sim_stat_add(who, STAT_HAPPY, 7);
        sim_stat_add(who, STAT_ENERGY, -7);
        break;
    case ACT_PARK:
    case ACT_BEACH:      /* pleasant outdoor time, no free inventory */
        sim_stat_add(who, STAT_HAPPY, 8);
        sim_stat_add(who, STAT_ENERGY, -3);
        break;
    case ACT_SHOP:       /* browsing does not purchase an object */
        sim_stat_add(who, STAT_HAPPY, 5);
        break;
    default:
        sim_stat_add(who, STAT_SOCIAL, 3);
        break;
    }
}

/* The same little human(ish) voice in both world modes. A quiet person,
 * a lover of the place, and a spontaneous visitor shouldn't all sound the
 * same. This is read-only and doesn't touch the deterministic sim RNG. */
const char *talk_outcome_meet_reply(int guest_id, int venue, int kind)
{
    const DoomoResident *r =
        guest_id >= 0 && guest_id < DOOMO_MAX_RESIDENTS ? town_res(guest_id) : NULL;
    if (!r || r->status != RES_ALIVE || venue < 0 || venue >= ACT_COUNT)
        return "Thanks for spending a little time together.";
    if (r->mood == MOOD_SAD || r->mood == MOOD_SCARED)
        return "Thanks for staying with me. I didn't have many words today.";
    if (sim_favourite_venue(guest_id) == venue)
        return "You remembered my favorite place? That means a lot!";
    if (kind == TALK_MEET_CHAT)
        return ((RES_CATCHPHRASE(r) + guest_id) & 1) ?
               "I saved you the good seat. It's a very important seat." :
               "I like talking to you. Even when we run out of words.";
    switch (venue) {
    case ACT_CAFE: return "This snack deserves its own tiny parade.";
    case ACT_CONCERT: return "That music made the whole town sound different.";
    case ACT_CHURCH: return "The quiet did me good. So did your company.";
    case ACT_WORKSHOP: return "Two minds, one project, several suspicious sparks.";
    case ACT_SHOP: return "We didn't buy everything. A historic achievement.";
    case ACT_PARK: return "The wind almost stole my thoughts. Nice walk!";
    case ACT_BEACH: return "The sea kept all its secrets. I told you one.";
    default: return "Thanks for spending time together!";
    }
}

int talk_outcome_meet_kind(int id, int venue, int kind)
{
    LedMeet *meet;
    int avatar, owner = playas_avatar();
    DoomoResident *r, *player;
    if (kind != TALK_MEET_CHAT && kind != TALK_MEET_ACTIVITY) return -1;
    if (venue < 0 || venue >= ACT_COUNT) return -1;
    roll();
    meet = meet_find(id);
    if (!meet_present(meet, venue, owner)) return -1;
    /* The café charges the player for two small snacks. Check before
     * consuming the appointment; the free conversation remains available.
     * No free item or money is created by simply selecting the menu. */
    if (kind == TALK_MEET_ACTIVITY && venue == ACT_CAFE &&
        doomo_town.money < 6u)
        return -1;

    /* This is a real meeting at an actual door or outdoor arrival point.
     * Consume the pending reservation once, before applying its effects. */
    meet->venue = 0;
    if (kind == TALK_MEET_ACTIVITY && venue == ACT_CAFE)
        doomo_town.money -= 6u;
    r = town_res(id);
    avatar = playas_slot();
    player = town_res(avatar);
    sim_stat_add(r, STAT_SOCIAL, 12);
    sim_stat_add(r, STAT_HAPPY, 10);
    if (player && player != r && player->status == RES_ALIVE) {
        sim_stat_add(player, STAT_SOCIAL, 10);
        sim_stat_add(player, STAT_HAPPY, 8);
    }
    if (kind == TALK_MEET_ACTIVITY) {
        meet_local_activity(r, venue);
        if (player && player != r) meet_local_activity(player, venue);
    }
    rel_adjust(id, avatar, 4);
    sim_mood_of(id);
    if (player && player != r && player->status == RES_ALIVE)
        sim_mood_of(avatar);
    /* Shared memory is recorded only after the actual meetup happened,
     * never when the NPC merely accepted the original invitation. */
    talk_mem_note_shared(id, TALK_SHARED_PLAYED);
    /* Tomodachi-style public-life continuity: a named event in the
     * existing 8-byte Town News ring, only after the player arrived,
     * paid any required cost and shared an actual activity. Since the
     * appointment was consumed above, revisiting the menu cannot
     * fabricate another headline. */
#ifdef DOOMO_HAS_TALK
    talk_visit_note(id, owner, venue);
    sim_news_rec(sim_favourite_venue(id) == venue ? NT_TALK_MEET_FAV : NT_TALK_MEET,
                 venue, NEWS_IDS(id, owner, NEWS_NONE));
#endif
    return id;
}

int talk_outcome_meet(int venue)
{
    return talk_outcome_meet_kind(talk_outcome_meet_ready(venue), venue, TALK_MEET_CHAT);
}

void talk_outcome_apply(const TalkSummary *sum, TalkOutcome *out)
{
    TalkOutcome local;
    int k, av, ev, great_a = -1, great_b = -1;
    int news_made = -1, news_row = -1, news_walk = -1, news_quest = -1;
    if (!out) out = &local;
    memset(out, 0, sizeof *out);
    if (!sum) return;
    roll();
    sim_ensure_init();
    av = playas_slot();

    /* ---- the bond, level, mood and needs of each resident ---- */
    for (k = 0; k < sum->n && k < TALK_PARTS_MAX; k++) {
        int id = sum->id[k], applied, want;
        LedP *lp;
        if (is_pet_id(id)) continue;
        if (pool_id(id)) {                                   /* a family pool member: no town slot, the bond nibble only */
            lp = led_p(id);
            if (sum->rel_d_player[k] > 0 && !(lp->flags & LPF_CARE)) { lp->flags |= LPF_CARE; family_care(id); out->rel[k] = 1; }
            if (sum->mood_d[k]) { int d = take_signed(&lp->mood, sum->mood_d[k], TALK_CAP_MOOD_DAY);
                                  if (d) { family_mood_add(id, d > 0 ? 1 : -1); out->mood[k] = (int8_t)d; } }
            continue;
        }
        if (!alive_resident(id)) continue;
        lp = led_p(id);
        want = sum->rel_d_player[k];
        applied = take_signed(&lp->rel, want, TALK_CAP_REL_DAY);
        if (applied) rel_adjust(id, av, applied);
        out->rel[k] = (int8_t)applied;
        if (applied > 0) {
            DoomoResident *r = town_res(id);
            int lv = take_up(&lp->level, (applied + 3) / 4, TALK_CAP_LEVEL_DAY);
            r->level = (uint8_t)DOOMO_MIN(99, r->level + lv);
            out->level = (uint8_t)(out->level + lv);
            if (!(lp->flags & LPF_CARE)) { lp->flags |= LPF_CARE; family_care(id); }   /* a no-op unless they are family */
        }
        {
            DoomoResident *r = town_res(id);
            int d = take_signed(&lp->mood, sum->mood_d[k], TALK_CAP_MOOD_DAY);
            if (d) sim_stat_add(r, STAT_HAPPY, d);
            out->mood[k] = (int8_t)d;
            if (!(sum->pflags[k] & TPF_LEFT)) {              /* STAT_SOCIAL +4..+12 by engagement */
                int soc = take_up(&lp->social, 4 + sum->engage[k] * 8 / 100, TALK_CAP_SOCIAL_DAY);
                sim_stat_add(r, STAT_SOCIAL, soc);
                out->social = (uint8_t)(out->social + soc);
            }
            sim_mood_of(id);
        }
    }

    /* ---- resident-resident bonds ---- */
    for (k = 0; k < 6; k++) {
        int i, j, d, ida, idb;
        pair_slots(k, &i, &j);
        if (!slot_ok(sum, i) || !slot_ok(sum, j) || !sum->pair_d[k]) continue;
        ida = sum->id[i]; idb = sum->id[j];
        if (!alive_resident(ida) || !alive_resident(idb)) continue;
        d = take_signed(&led_q(ida, idb)->d, sum->pair_d[k], TALK_CAP_PAIR_DAY);
        if (d) rel_adjust(ida, idb, d);
        out->pair[k] = (int8_t)d;
    }

    /* ---- the events ---- */
    for (ev = 0; ev < sum->nev && ev < TALK_EVENTS_MAX; ev++) {
        const TalkEvent *e = &sum->ev[ev];
        int ia = slot_ok(sum, e->a) ? sum->id[e->a] : -1, ib = slot_ok(sum, e->b) ? sum->id[e->b] : -1;
        switch (e->kind) {
        case TEV_ULTIMATUM:                                  /* a big row: the bond falls (rel_adjust demotes to rival by itself) */
            if (ia >= 0 && ib >= 0 && alive_resident(ia) && alive_resident(ib)) {
                int d = take_signed(&led_q(ia, ib)->d, -3, TALK_CAP_PAIR_DAY), pk = pair_index(e->a, e->b);
                if (d) {
                    rel_adjust(ia, ib, d);
#ifdef DOOMO_HAS_TALK
                    talk_event_note(TALK_EV_ARGUMENT, ia, ib, TALK_PLACE_UNKNOWN);
#endif
                }
                if (pk >= 0) out->pair[pk] = (int8_t)(out->pair[pk] + d);
                if (news_row < 0) news_row = ev;
            }
            break;
        case TEV_MEDIATED:                                   /* peace: the sim's own make-up, once a pair a day */
            if (ia >= 0 && ib >= 0 && alive_resident(ia) && alive_resident(ib)) {
                LedQ *q = led_q(ia, ib);
                if (!(q->flags & LQF_MADE_UP) && rel_kind(ia, ib) >= 0) {
                    sim_make_up(ia, ib);
                    q->flags |= LQF_MADE_UP;
                    out->made_up++;
                    if (news_made < 0) news_made = ev;
                }
            }
            break;
        case TEV_PROBLEM:                                    /* talked through: one a resident a day; the sim decides if it closes */
            if (ia >= 0 && resident_id(ia) && slot_ok(sum, e->a) && (sum->pflags[e->a] & TPF_PROBLEM)) {
                LedP *lp = led_p(ia);
                int pi = (lp->flags & LPF_PROBLEM) ? -1 : sim_problem_for(ia);
                if (pi >= 0) {
                    sim_problem_resolve(pi);
                    if (sim_problem_for(ia) < 0) { lp->flags |= LPF_PROBLEM; out->problems++; }   /* an open one (an item, a possession) stays open */
                }
            }
            break;
        case TEV_ROMANCE_YES:                                /* Talk only asks: the sim rolls and owns the result */
            if (ia >= 0 && (e->b == TA_CONFESS || e->b == TA_ASK_OUT) && resident_id(ia) && sum->avatar >= 0 &&
                sum->avatar < DOOMO_MAX_RESIDENTS && alive_resident(sum->avatar) && alive_resident(ia) &&
                !minor_id(sum->avatar) && !minor_id(ia)) {   /* defence in depth: the DM and the sim gate it too */
                LedQ *q = led_q(sum->avatar, ia);
                if (!(q->flags & LQF_ASKED)) {
                    q->flags |= LQF_ASKED;
                    out->confessed++;
                    if (sim_confess(sum->avatar, ia, 10)) out->dating++;
                }
            }
            break;
        case TEV_WALKOUT:
            if (ia >= 0 && news_walk < 0) news_walk = ev;
            break;
        case TEV_LEARNED:                                    /* TM2d: a secret told: the player now knows that like, fear or hated food */
            if (ia >= 0 && alive_resident(ia) && e->b < 5) {
                talk_mem_know_like(ia, e->b);
                out->learned++;
            }
            break;
        case TEV_ACT_INVITE: case TEV_ACT_GIFT: case TEV_ACT_ASK:
            apply_action(sum, e, ia, out);
            break;
        case TEV_QUEST_ACCEPT:                               /* TQ: the table (the quest log reads it) */
            if (ia >= 0 && alive_resident(ia) && talk_quest_add(ia, e->b & 15, (e->b >> 4) & 1)) out->quests_new++;
            else out->skipped++;
            break;
        case TEV_QUEST_DECLINE:
            if (ia >= 0 && alive_resident(ia)) { talk_quest_decline(ia); out->quests_declined++; }
            break;
        case TEV_QUEST_DONE: {
            int pay = ia >= 0 && alive_resident(ia) ? talk_quest_complete(ia) : -1;
            if (pay < 0) { out->skipped++; break; }
            out->quests_done++;
            out->reward = (uint16_t)(out->reward + pay);
            if (news_quest < 0 && !minor_id(ia)) news_quest = ia;
            break;
        }
        case TEV_QUEST_LAPSED:
            if (ia >= 0) { talk_quest_lapse_ack(ia); out->quests_lapsed++; }
            break;
        case TEV_ACT_CALL: {                                 /* the scene lets them in; strangers of the same kind meet (sim_introduce) */
            int called = (int)sum->call_to - 1;
            if (ia >= 0 && alive_resident(ia) && alive_resident(called) && called != ia) {
                out->called++;
                if (rel_kind(ia, called) < 0 && minor_id(ia) == minor_id(called)) sim_introduce(ia, called);
            } else out->skipped++;
            break;
        }
        case TEV_GREAT_NIGHT: {
            int j;
            for (j = 0; j < sum->n && j < TALK_PARTS_MAX; j++) {
                if (!resident_id(sum->id[j]) || (sum->pflags[j] & TPF_LEFT)) continue;
                if (great_a < 0) great_a = sum->id[j]; else if (great_b < 0) great_b = sum->id[j];
            }
            break;
        }
        default: break;
        }
    }

    /* ---- one headline: peace, then the row, the walk-out, the great night ---- */
    if (news_made >= 0) {
        const TalkEvent *e = &sum->ev[news_made];
        post_news(out, NT_TALK_MADE_UP, sum->id[e->a], sum->id[e->b]);
    }
    if (!out->news && news_row >= 0) {
        const TalkEvent *e = &sum->ev[news_row];
        post_news(out, NT_TALK_ROW, sum->id[e->a], sum->id[e->b]);
    }
    if (!out->news && news_walk >= 0) post_news(out, NT_TALK_WALKOUT, sum->id[sum->ev[news_walk].a], -1);
    if (!out->news && news_quest >= 0) post_news(out, NT_TALK_QUEST, news_quest, -1);
    if (!out->news && great_a >= 0 && great_b >= 0) post_news(out, NT_TALK_GREAT_NIGHT, great_a, great_b);
}

/* ---- the conversation's two ends ------------------------------------------------------ */
void talk_outcome_begin(TalkState *s, int avatar, const int *ids, int n, uint32_t seed)
{
    int k;
    if (!s) return;
    talk_dm_begin(s, avatar, ids, n, seed);
    talk_mem_begin(&s->conv);
    for (k = 0; k < s->nparts; k++)
        if ((s->conv.refused >> k) & 1) s->part[k].flags |= TPF_REFUSED;
}

void talk_outcome_end(TalkState *s, TalkOutcome *out, TalkSummary *sum)
{
    TalkSummary local;
    int k;
    if (!s) return;
    if (!sum) sum = &local;
    for (k = 0; k < s->nparts; k++) {
        int id = s->part[k].id;
        if (is_pet_id(id)) continue;
        /* The saved memory belongs only to this actual addressee. A
         * bystander does not inherit someone else's insult, and a person
         * who walked out still remembers what they heard before leaving. */
        if (s->mem_to[k]) talk_mem_set_flags(id, s->mem_to[k]);
#ifdef DOOMO_HAS_TALK
        if (alive_resident(id)) {
            int shift;
            for (shift = 4; shift >= 0; shift -= 2) {
                int tone = (s->memory_order[k] >> shift) & 3;
                if (tone) talk_event_note(TALK_EV_PRAISE + tone - 1, id, s->conv.avatar, s->place);
            }
        }
#endif
        /* Keep the topic they answered before the room moved on. */
        if (s->answered_topic[k])
            talk_mem_note_topic(id, s->answered_topic[k] & TALK_MAX_CONCEPTS);
    }
#ifdef DOOMO_HAS_TALK
    /* Fold the last distinct pairs in order. The scene can finish through
     * another callback without reinforcing the same observation twice. */
    for (k = TALK_LINK_OBSERVATIONS - 1; k >= 0; k--) {
        uint32_t key = s->topic_links[k];
        unsigned who = key >> 24;
        if (key && who < s->nparts)
            talk_link_note(s->part[who].id, s->conv.avatar, key & TALK_MAX_CONCEPTS,
                           (uint16_t)((key >> 12) & TALK_MAX_CONCEPTS));
        s->topic_links[k] = 0;
    }
#endif
    talk_mem_end(&s->conv);
    talk_dm_end(s, sum);
    talk_outcome_apply(sum, out);
    talk_quest_end(sum);                                     /* TQ: what the talk did to the open requests, and the pace */
}
