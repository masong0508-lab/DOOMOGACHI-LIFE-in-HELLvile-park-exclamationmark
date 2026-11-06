/* playas.c - PA1: who the player controls (owner: playas; docs/PLAYAS.md).
 * State is zero-initialised statics: the avatar is stored as an offset from
 * the hero, so all-zero means "you are the hero". The 'PLAY' chunk is the
 * 32 B header plus the PA10 pockets (money, bag and health per avatar); it is
 * empty while everything is default.
 * Strong definitions of the weak defaults in weak_playas.c. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/profiles.h"
#include "doomo/ending.h"
#include "doomo/unlocks.h"
#include "doomo/unlock_ids.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/life_contract.h"
#include "doomo/platform.h"
#include "doomo/ui.h"
#include "doomo/town.h"
#include "doomo/sim.h"
#include "doomo/roster.h"
#include "doomo/resident.h"

#define RECENT_N 4
#define PF_NEWLIFE 1u
#define PF_HEROPOS 2u

static struct {
    int16_t  off;               /* avatar - PLAYAS_ID_HERO (0 = the hero) */
    uint8_t  kind, nrecent;
    uint16_t switches;
    uint16_t recent[RECENT_N];  /* newest first */
    PlayasPos pos;
    uint8_t  flags;
} P;
static uint8_t armed_;          /* PA10 part 3: a fresh free play world may start a New life (not saved) */

static int in_family(int id) { return id >= PLAYAS_ID_FAMILY_FIRST && id <= PLAYAS_ID_FAMILY_LAST; }

int playas_avatar(void) { return PLAYAS_ID_HERO + P.off; }
int playas_is_hero(void) { return P.off == 0; }
int playas_story_locked(void) { return P.off != 0; }

int playas_free_unlocked(void)
{
    int k;
    if (ending_count_seen() > 0) return 1;
    for (k = 0; k < ENDING_MAIN_COUNT; k++)
        if (!ENDING_IS_RETIRED(k) && unlock_has(UL_STORY_END(k))) return 1;
    return 0;
}

int playas_available(void)
{
    if (P.flags & PF_NEWLIFE) return 1;
    if (doomo_town.mode == TOWN_STORY) return doomo_town.story.chapter == STORY_CH_DONE;
    /* FREE (PA10 part 4): Free play unlocks play-as once any ending has been reached (the unlocks
     * outlive the world); a fresh-world New life, a save already mid-switch and a save with guests
     * stay open, so nobody is stranded. */
    if (armed_ || P.off != 0 || prof_guest_count() > 0) return 1;
    return playas_free_unlocked();
}

/* the id is a valid avatar (ignoring availability and "already them") */
static int valid(int id)
{
    const DoomoResident *r;
    if (id == PLAYAS_ID_HERO) return 1;
    if (in_family(id)) {
        const FamilyMember *m = family_find(id);
        return m && m->role != FAMROLE_PET;      /* a pet is playable once uplifted: family_uplift() makes it a housemate */
    }
    if (id < 0 || id >= DOOMO_PLAYER_ID) return 0;
    r = town_res(id);
    return r && (r->status == RES_ALIVE || r->status == RES_GHOST);
}

int playas_can_be(int id)
{
    return playas_available() && id != playas_avatar() && valid(id);
}

int playas_gender(int id)
{
    int pr;
    const FamilyIdentity *fi;
    const DoomoResident *r;
    if ((fi = family_identity(id)) != NULL) pr = fi->pronoun;
    else if ((r = town_res(id)) != NULL && r->status != RES_EMPTY) pr = RES_PRONOUN(r);
    else return GENDER_N;
    return pr == PRONOUN_SHE ? GENDER_F : pr == PRONOUN_HE ? GENDER_M : GENDER_N;
}

int playas_you_gender(void) { return playas_gender(playas_avatar()); }
int playas_hero_kind(void) { return P.kind; }
void playas_set_hero_kind(int k) { if (k >= 0 && k < HERO_KIND_COUNT) P.kind = (uint8_t)k; }

int playas_recent(int i) { return i >= 0 && i < P.nrecent ? P.recent[i] : PLAYAS_ID_NONE; }

static void recent_drop(int id)
{
    int i, j;
    for (i = 0; i < P.nrecent; i++)
        if (P.recent[i] == id) {
            for (j = i; j + 1 < P.nrecent; j++) P.recent[j] = P.recent[j + 1];
            P.nrecent--;
            i--;
        }
}

static void recent_push(int id)
{
    int i;
    recent_drop(id);
    if (P.nrecent < RECENT_N) P.nrecent++;
    for (i = P.nrecent - 1; i > 0; i--) P.recent[i] = P.recent[i - 1];
    P.recent[0] = (uint16_t)id;
}

void playas_set_hero_pos(int x, int y, int map, int dir)
{
    P.pos.x = (int16_t)x; P.pos.y = (int16_t)y; P.pos.map = (uint8_t)map; P.pos.dir = (uint8_t)dir;
    P.flags |= PF_HEROPOS;
}

int playas_hero_pos(PlayasPos *out)
{
    if (P.off == 0 || !(P.flags & PF_HEROPOS) || !out) return 0;
    *out = P.pos;
    return 1;
}

uint8_t playas_profile_flags(void)
{
    uint8_t f = PLAYAS_HF_MAKE_HERO(P.kind);
    if (P.off != 0) f |= PLAYAS_HF_PLAYING_OTHER;
    if (P.flags & PF_NEWLIFE) f |= PLAYAS_HF_NEW_LIFE;
    if (prof_guest_count()) f |= PLAYAS_HF_PROFILES;     /* MP0: guests live in this world */
    if (playas_available() && !(P.flags & PF_NEWLIFE)) f |= PLAYAS_HF_STORY_CLEARED;
    return f;
}

int playas_new_life_ok(void)
{
    if (P.flags & PF_NEWLIFE) return 0;
    if (armed_ && doomo_town.mode == TOWN_FREEPLAY) return 1;
    return doomo_town.mode == TOWN_STORY && doomo_town.story.chapter == STORY_CH_DONE;
}

void playas_new_life_arm(void)
{
    armed_ = (uint8_t)(doomo_town.mode == TOWN_FREEPLAY && !(P.flags & PF_NEWLIFE));
}

int playas_new_life(int id)
{
    if (!playas_new_life_ok() || id == PLAYAS_ID_HERO || !playas_can_be(id)) return 0;
    P.flags |= PF_NEWLIFE;
    if (!playas_switch(id, PLAYAS_SWITCH_FADE)) { P.flags &= (uint8_t)~PF_NEWLIFE; return 0; }
    armed_ = 0;
    return 1;
}


/* ---- PA10: per-avatar pockets (docs/PLAYAS.md section 9) ---------------------
 * Money, the bag and health follow the avatar. The live fields stay where every
 * shop, gift and cooking site already reads them (doomo_town.money / .items /
 * .story.hp / .armor), so they always belong to WHOEVER YOU PLAY. A switch saves
 * the outgoing avatar's three things into a pocket and loads the incoming one's.
 *   - the hero's whole bag (64 kinds) is one wide stash of two pocket slots, held
 *     only while somebody else is played;
 *   - a villager or family member keeps a pocket of 16 stacks (the 16 biggest);
 *   - 14 villager pockets, most recently used first; the oldest falls back to
 *     what the sim generates (a small purse and two snacks);
 *   - a save from before PA10 that is mid play-as has no hero stash: switching
 *     to the hero keeps what is carried (the old, shared behaviour). */
#define PK_HERO_SLOTS  2
#define PK_MAX         (PLAY_POCKETS - PK_HERO_SLOTS)
#define PK_HP_MAX      200
#define PK_WIRE_ITEMS  8                         /* offset of the item bytes */
#define PK_WIRE_ARMOR  40
#define PK_HERO_ARMOR  72
DOOMO_STATIC_ASSERT(DOOMO_MAX_ITEMS == 64, pk_hero_stash_counts_fit_two_slots);
DOOMO_STATIC_ASSERT(PK_WIRE_ITEMS + PLAY_POCKET_ITEMS * 2 <= PK_WIRE_ARMOR, pk_items_before_armor);
DOOMO_STATIC_ASSERT(PK_WIRE_ARMOR + 1 <= PLAY_SZ_POCKET, pk_armor_fits);
DOOMO_STATIC_ASSERT(PK_HERO_ARMOR + 1 <= PK_HERO_SLOTS * PLAY_SZ_POCKET, pk_hero_fits);

typedef struct { uint16_t id, hp; uint32_t money; uint8_t item[PLAY_POCKET_ITEMS], count[PLAY_POCKET_ITEMS]; uint8_t armor; } Pocket;
typedef struct { uint32_t money; uint8_t hp, armor, have; uint8_t items[DOOMO_MAX_ITEMS]; } HeroStash;

static Pocket    pk_[PK_MAX];                    /* newest first */
static uint8_t   npk_;
static HeroStash hs_;

static uint8_t hp_live(void) { return doomo_town.story.hp > PK_HP_MAX ? PK_HP_MAX : doomo_town.story.hp; }

static void hero_capture(void)
{
    hs_.money = doomo_town.money; hs_.hp = hp_live(); hs_.armor = doomo_town.story.armor;
    memcpy(hs_.items, doomo_town.items, DOOMO_MAX_ITEMS);
    hs_.have = 1;
}

static void hero_apply(void)
{
    doomo_town.money = hs_.money; doomo_town.story.hp = hs_.hp; doomo_town.story.armor = hs_.armor;
    memcpy(doomo_town.items, hs_.items, DOOMO_MAX_ITEMS);
    memset(&hs_, 0, sizeof hs_);
}

/* the live money, health and the 16 biggest stacks (ties: the lower item id) */
static void live_to_pocket(Pocket *p, int id)
{
    uint8_t used[DOOMO_MAX_ITEMS];
    int n, i, best;
    memset(p, 0, sizeof *p);
    memset(used, 0, sizeof used);
    p->id = (uint16_t)id; p->hp = hp_live(); p->armor = doomo_town.story.armor; p->money = doomo_town.money;
    for (n = 0; n < PLAY_POCKET_ITEMS; n++) {
        best = -1;
        for (i = 1; i < ITEM_COUNT; i++)
            if (doomo_town.items[i] && !used[i] && (best < 0 || doomo_town.items[i] > doomo_town.items[best])) best = i;
        if (best < 0) break;
        used[best] = 1;
        p->item[n] = (uint8_t)best; p->count[n] = doomo_town.items[best];
    }
}

static void pocket_to_live(const Pocket *p)
{
    int n;
    doomo_town.money = p->money; doomo_town.story.hp = (uint8_t)p->hp; doomo_town.story.armor = p->armor;
    memset(doomo_town.items, 0, DOOMO_MAX_ITEMS);
    for (n = 0; n < PLAY_POCKET_ITEMS; n++)
        if (p->item[n] > 0 && p->item[n] < ITEM_COUNT) doomo_town.items[p->item[n]] = p->count[n];
}

/* what the sim generates for someone with no pocket: the town seed and the id make
 * a purse of H$40..199, full health and two different snacks. Never random. */
static void pocket_default(Pocket *p, int id)
{
    uint32_t h = doomo_town.seed ^ ((uint32_t)id * 2654435761u);
    int a, b;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    memset(p, 0, sizeof *p);
    p->id = (uint16_t)id; p->hp = 100; p->money = 40u + (h % 160u);
    a = ITEM_CUPCAKE + (int)((h >> 8) % 12u);
    b = ITEM_CUPCAKE + (int)(((h >> 12) % 11u + (unsigned)(a - ITEM_CUPCAKE) + 1u) % 12u);   /* never a */
    p->item[0] = (uint8_t)a; p->count[0] = (uint8_t)(1u + ((h >> 16) % 3u));
    p->item[1] = (uint8_t)b; p->count[1] = (uint8_t)(1u + ((h >> 20) % 3u));
}

static int pk_find(int id)
{
    int i;
    for (i = 0; i < npk_; i++) if (pk_[i].id == id) return i;
    return -1;
}

static void pk_remove(int i)
{
    for (; i + 1 < npk_; i++) pk_[i] = pk_[i + 1];
    npk_--;
}

static void pk_store(const Pocket *p)        /* newest first; the oldest is dropped when full */
{
    int i = pk_find(p->id);
    if (i >= 0) pk_remove(i);
    if (npk_ == PK_MAX) npk_--;
    for (i = npk_; i > 0; i--) pk_[i] = pk_[i - 1];
    pk_[0] = *p;
    npk_++;
}

static int pk_take(int id, Pocket *out)
{
    int i = pk_find(id);
    if (i < 0) return 0;
    *out = pk_[i];
    pk_remove(i);
    return 1;
}

static void pockets_swap(int from, int to)
{
    Pocket p;
    if (from == to) return;
    if (from == PLAYAS_ID_HERO) hero_capture();
    else { live_to_pocket(&p, from); pk_store(&p); }
    if (to == PLAYAS_ID_HERO) { if (hs_.have) hero_apply(); }          /* no stash: an old save, keep what is carried */
    else { if (!pk_take(to, &p)) pocket_default(&p, to); pocket_to_live(&p); }
}

int playas_pocket_count(void) { return npk_ + (hs_.have ? PK_HERO_SLOTS : 0); }

/* 1 and *money when id has a pocket held right now (not the avatar, whose things are live) */
int playas_pocket_money(int id, uint32_t *money)
{
    int i;
    if (id == PLAYAS_ID_HERO) { if (!hs_.have) return 0; if (money) *money = hs_.money; return 1; }
    if ((i = pk_find(id)) < 0) return 0;
    if (money) *money = pk_[i].money;
    return 1;
}

int playas_switch(int id, int how)
{
    int from = playas_avatar();
    (void)how;                                   /* PA4 presents it */
    if (!playas_can_be(id)) return 0;
    pockets_swap(from, id);
    recent_push(from);
    P.off = (int16_t)(id - PLAYAS_ID_HERO);
    recent_drop(id);
    P.switches++;
    return 1;
}

/* the day's check: an avatar that is gone, possessed or lost sends you home */
int playas_check(void)
{
    if (P.off == 0 || valid(playas_avatar())) return 0;
    recent_drop(playas_avatar());
    pockets_swap(playas_avatar(), PLAYAS_ID_HERO);   /* their pocket is kept, in case they come back */
    P.off = 0;
    recent_drop(PLAYAS_ID_HERO);                  /* the avatar is never in the ring */
    ui_toast("You are back as yourself.");
    plat_log("PLAYAS: avatar lost, back to the hero");
    return 1;
}

/* ---- the 'PLAY' chunk: 32 B header, the hero stash, the pockets (docs/PLAYAS.md sections 4, 9) ---- */
static int is_default(void)
{
    return P.off == 0 && P.kind == 0 && P.nrecent == 0 && P.switches == 0 && !P.flags && !npk_ && !hs_.have;
}

static uint32_t play_size(void)
{
    if (is_default()) return 0u;
    return PLAY_SZ_HEADER + (hs_.have ? PK_HERO_SLOTS * PLAY_SZ_POCKET : 0u) + (uint32_t)npk_ * PLAY_SZ_POCKET;
}

static void put16(uint8_t *d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *d, uint32_t v) { put16(d, v); put16(d + 2, v >> 16); }
static uint32_t get32(const uint8_t *s) { return (uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24); }

static uint32_t play_write(uint8_t *d, uint32_t cap)
{
    int i, a = playas_avatar();
    uint32_t sz = play_size(), at = PLAY_SZ_HEADER;
    if (is_default() || cap < sz) return 0;
    memset(d, 0, sz);
    d[0] = 1; d[1] = P.flags & PF_NEWLIFE ? 1 : 0;
    d[2] = (uint8_t)a; d[3] = (uint8_t)(a >> 8);
    d[4] = P.kind; d[5] = P.nrecent;
    d[6] = (uint8_t)P.switches; d[7] = (uint8_t)(P.switches >> 8);
    for (i = 0; i < RECENT_N; i++) { d[8 + 2 * i] = (uint8_t)P.recent[i]; d[9 + 2 * i] = (uint8_t)(P.recent[i] >> 8); }
    d[16] = (uint8_t)P.pos.x; d[17] = (uint8_t)((uint16_t)P.pos.x >> 8);
    d[18] = (uint8_t)P.pos.y; d[19] = (uint8_t)((uint16_t)P.pos.y >> 8);
    d[20] = P.pos.map; d[21] = P.pos.dir;
    d[22] = (uint8_t)playas_pocket_count();      /* pocket slots that follow (the hero stash is two) */
    d[23] = hs_.have ? 1 : 0;                    /* bit 0: the hero stash comes first */
    d[24] = P.flags & PF_HEROPOS ? 1 : 0;
    if (hs_.have) {
        uint8_t *h = d + at;
        put16(h, PLAYAS_ID_HERO); put16(h + 2, hs_.hp); put32(h + 4, hs_.money);
        memcpy(h + PK_WIRE_ITEMS, hs_.items, DOOMO_MAX_ITEMS);
        h[PK_HERO_ARMOR] = hs_.armor;
        at += PK_HERO_SLOTS * PLAY_SZ_POCKET;
    }
    for (i = 0; i < npk_; i++, at += PLAY_SZ_POCKET) {
        uint8_t *q = d + at;
        int n;
        put16(q, pk_[i].id); put16(q + 2, pk_[i].hp); put32(q + 4, pk_[i].money);
        for (n = 0; n < PLAY_POCKET_ITEMS; n++) { q[PK_WIRE_ITEMS + 2 * n] = pk_[i].item[n]; q[PK_WIRE_ITEMS + 2 * n + 1] = pk_[i].count[n]; }
        q[PK_WIRE_ARMOR] = pk_[i].armor;
    }
    return sz;
}


/* ---- PA9: the play-as romance gate ------------------------------------------
 * Pairs that do not touch "you" (the hero, the avatar, a family pool member)
 * are exactly LS0's sim_romance_ok: a one-way crush stays one-way. A pair that
 * does touch you is stricter, because mature slots and routes hang off it:
 *   - both sides alive and ADULT / ELDER (no teens, kids or babies);
 *   - BOTH directions accept: each side's preference takes the other's
 *     pronoun group (the player's confession needs the NPC to be open too);
 *   - never family (REL_FAMILY, or a pool child / parent / sibling /
 *     grandparent / pet), never a kid-coded premade (KID / TEEN roster traits). */
static int touches_you(int id) { return id == PLAYAS_ID_HERO || id == playas_avatar() || in_family(id); }

static int pron_grp(int p) { return p == PRONOUN_SHE ? ROMANCE_SHE : p == PRONOUN_HE ? ROMANCE_HE : ROMANCE_THEY; }

typedef struct { int ok, stage, pronoun, pref; } RomSide;

static RomSide rom_side(int id)
{
    RomSide s = { 0, LIFE_ADULT, PRONOUN_THEY, ROMANCE_NONE };
    const FamilyIdentity *fi;
    const FamilyMember *fm;
    const DoomoResident *r;
    int role = family_role(id);
    if (role == FAMROLE_CHILD || role == FAMROLE_PARENT || role == FAMROLE_SIBLING ||
        role == FAMROLE_GRANDPARENT || role == FAMROLE_PET) return s;
    if (in_family(id)) {
        fi = family_identity(id);
        fm = family_find(id);
        if (!fi || !fm) return s;
        s.stage = fm->stage; s.pronoun = fi->pronoun; s.pref = fi->romance;
        s.ok = 1;
        return s;
    }
    r = town_res(id);
    if (!r || r->status != RES_ALIVE) return s;
    if (id < DOOMO_PLAYER_ID && (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN))) return s;
    s.stage = sim_life_stage(id); s.pronoun = RES_PRONOUN(r); s.pref = RES_ROMANCE(r);
    s.ok = 1;
    return s;
}

static int accepts(const RomSide *a, const RomSide *b)
{
    return a->pref != ROMANCE_NONE && (a->pref == ROMANCE_ANY || a->pref == pron_grp(b->pronoun));
}

int playas_romance_ok(int from, int to)
{
    RomSide a, b;
    if (from == to) return 0;
    if (!touches_you(from) && !touches_you(to)) return sim_romance_ok(from, to);
    a = rom_side(from); b = rom_side(to);
    if (!a.ok || !b.ok) return 0;
    if (!LIFE_IS_ADULT(a.stage) || !LIFE_IS_ADULT(b.stage)) return 0;
    if (!accepts(&a, &b) || !accepts(&b, &a)) return 0;
    if (!in_family(from) && !in_family(to) && rel_kind(from, to) == REL_FAMILY) return 0;
    return 1;
}

static void play_reset(void) { armed_ = 0; memset(&P, 0, sizeof P); memset(pk_, 0, sizeof pk_); npk_ = 0; memset(&hs_, 0, sizeof hs_); }

/* the pockets after the header; anything that does not make sense is dropped, never trusted */
static void pockets_read(const uint8_t *s, uint32_t len, int avatar)
{
    uint32_t at = PLAY_SZ_HEADER;
    int slots = s[22], i, n;
    if (slots > PLAY_POCKETS) slots = PLAY_POCKETS;
    if ((s[23] & 1) && slots >= PK_HERO_SLOTS && at + PK_HERO_SLOTS * PLAY_SZ_POCKET <= len) {
        const uint8_t *h = s + at;
        hs_.hp = (uint8_t)(h[2] > PK_HP_MAX ? 100 : h[2]); hs_.money = get32(h + 4); hs_.armor = h[PK_HERO_ARMOR];
        for (n = 1; n < ITEM_COUNT; n++) hs_.items[n] = (uint8_t)(h[PK_WIRE_ITEMS + n] > 99 ? 99 : h[PK_WIRE_ITEMS + n]);
        hs_.have = 1;
        at += PK_HERO_SLOTS * PLAY_SZ_POCKET;
        slots -= PK_HERO_SLOTS;
    }
    for (i = 0; i < slots && npk_ < PK_MAX && at + PLAY_SZ_POCKET <= len; i++, at += PLAY_SZ_POCKET) {
        const uint8_t *q = s + at;
        Pocket p;
        int id = q[0] | (q[1] << 8), hp = q[2] | (q[3] << 8), m = 0;
        if (id == PLAYAS_ID_HERO || id == avatar || !valid(id) || pk_find(id) >= 0) continue;
        memset(&p, 0, sizeof p);
        p.id = (uint16_t)id; p.hp = (uint16_t)(hp > PK_HP_MAX ? 100 : hp);
        p.money = get32(q + 4); p.armor = q[PK_WIRE_ARMOR];
        for (n = 0; n < PLAY_POCKET_ITEMS; n++) {
            int it = q[PK_WIRE_ITEMS + 2 * n], c = q[PK_WIRE_ITEMS + 2 * n + 1], k;
            if (it <= 0 || it >= ITEM_COUNT || c <= 0) continue;
            for (k = 0; k < m && p.item[k] != it; k++) {}
            if (k < m) continue;                                        /* a duplicate stack */
            p.item[m] = (uint8_t)it; p.count[m] = (uint8_t)(c > 99 ? 99 : c); m++;
        }
        pk_[npk_++] = p;                                                /* the file is newest first */
    }
}

static int play_read(const uint8_t *s, uint32_t len, uint16_t ver)
{
    int i, a;
    play_reset();
    if (ver != 1 || len < PLAY_SZ_HEADER) return 0;
    a = s[2] | (s[3] << 8);
    P.kind = s[4] < HERO_KIND_COUNT ? s[4] : 0;
    P.nrecent = s[5] < RECENT_N ? s[5] : RECENT_N;
    P.switches = (uint16_t)(s[6] | (s[7] << 8));
    for (i = 0; i < RECENT_N; i++) P.recent[i] = (uint16_t)(s[8 + 2 * i] | (s[9 + 2 * i] << 8));
    P.pos.x = (int16_t)(s[16] | (s[17] << 8)); P.pos.y = (int16_t)(s[18] | (s[19] << 8));
    P.pos.map = s[20]; P.pos.dir = s[21];
    if (s[1] & 1) P.flags |= PF_NEWLIFE;
    if (s[24] & 1) P.flags |= PF_HEROPOS;
    pockets_read(s, len, a);
    /* the avatar may be gone by now: clamp to the hero (the live things were theirs, so the hero's come back) */
    P.off = (int16_t)(a - PLAYAS_ID_HERO);
    if (a != PLAYAS_ID_HERO && !valid(a)) { P.off = 0; if (hs_.have) hero_apply(); }
    for (i = P.nrecent - 1; i >= 0; i--)
        if (P.recent[i] == playas_avatar() || (P.recent[i] != PLAYAS_ID_HERO && !valid(P.recent[i]))) recent_drop(P.recent[i]);
    return 1;
}

static const DoomoSaveExt play_ext = {
    SAVE_EXT_TAG_PLAY, 1, play_size, play_write, play_read, play_reset, SAVE_EXT_MAX_PLAY
};
const DoomoSaveExt *const doomo_save_ext_playas = &play_ext;
