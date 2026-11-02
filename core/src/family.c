/* family.c - the household table (owner: life; F1 part 1; family.h).
 *
 * Strong versions of life.h's family_count / family_member /
 * life_player_partner / life_family_count (weak "no family yet" defaults
 * live in weak_life.c). life_ending_facts stays LS5's.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/font.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/portrait.h"
#include "doomo/sim.h"
#include "doomo/items.h"
#include "doomo/town.h"
#include "doomo/story.h"
#include "sim_int.h"

static uint8_t will_ = 2;   /* F7 free will level (FAMILY_WILL_NORMAL), saved in the family section's spare byte */
static FamilyMember fam_[FAMILY_MAX];
static uint16_t sq_a_, sq_b_;      /* F8 pending squabble */
static uint8_t sq_on_;
static uint8_t dis_kind_;          /* F10 pending disaster (not saved) */
static uint16_t dis_id_;
#define BOND_DEFAULT 0x83u
static uint8_t bond_[FAMILY_MAX];   /* F7/F8: low nibble bond, high nibble mood, parallel to fam_ */
static uint8_t fam_n_;
static uint8_t neglect_grace_;   /* F6: skip the first neglect pass after a load */
static uint8_t loss_on_ = 1;     /* F6: the daily loss passes */
/* pool identities, by pool index (id - FAMILY_POOL_FIRST); a set bit = filled in */
static FamilyIdentity ident_[FAMILY_MAX];
static uint32_t ident_mask_;

/* ---- ids and the cap ------------------------------------------------------ */
int family_cap(void)
{
    int tier = family_tier();
#ifdef DOOMO_PLATFORM_GBA
    return HOUSEHOLD_CAP(1, tier);
#else
    return HOUSEHOLD_CAP(0, tier);
#endif
}

int family_tier(void)
{
    return unlock_has(UL_LIFE_HOUSEHOLD_T2) ? HOUSEHOLD_TIER_NGPLUS_DONE
         : unlock_has(UL_LIFE_HOUSEHOLD_T1) ? HOUSEHOLD_TIER_GAME_DONE
         : HOUSEHOLD_TIER_START;
}

/* one toast for a new tier, with the cap this platform now has */
static void tier_toast_(void)
{
    char msg[64];
    doomo_snprintf(msg, sizeof(msg), STR_ICON_STAR " NEW UNLOCK! Household: room for %d", family_cap());
    ui_toast(msg);
    audio_sfx(SFX_SPARKLE);
}

int family_tier_story_done(void)
{
    int id = !unlock_has(UL_LIFE_HOUSEHOLD_T1) ? UL_LIFE_HOUSEHOLD_T1
           : !unlock_has(UL_LIFE_HOUSEHOLD_T2) ? UL_LIFE_HOUSEHOLD_T2 : -1;
    if (id < 0 || !unlock_grant_quiet(id)) return 0;
    tier_toast_();
    return id == UL_LIFE_HOUSEHOLD_T1 ? HOUSEHOLD_TIER_GAME_DONE : HOUSEHOLD_TIER_NGPLUS_DONE;
}

int family_tier_sync(int story_done)
{
    if (!story_done || !unlock_grant_quiet(UL_LIFE_HOUSEHOLD_T1)) return 0;
    tier_toast_();
    return 1;
}

/* alive or a ghost (play-as.h: both can be played and may live in the house) */
static int res_lives_(const DoomoResident *r)
{
    return r && (r->status == RES_ALIVE || r->status == RES_GHOST);
}

static int is_pool(int id) { return id >= FAMILY_POOL_FIRST && id <= FAMILY_POOL_LAST; }

int family_id_ok(int id)
{
    return (id >= 0 && id < DOOMO_MAX_RESIDENTS) || is_pool(id);
}

static int find_slot(int id)
{
    int i;
    for (i = 0; i < fam_n_; i++)
        if (fam_[i].id == id) return i;
    return -1;
}

const FamilyMember *family_find(int id)
{
    int i = find_slot(id);
    return i < 0 ? NULL : &fam_[i];
}

int family_role(int id)
{
    int i = find_slot(id);
    return i < 0 ? FAMROLE_NONE : fam_[i].role;
}

int family_count_role(int role)
{
    int i, n = 0;
    for (i = 0; i < fam_n_; i++)
        if (fam_[i].role == role) n++;
    return n;
}

int family_pool_alloc(void)
{
    int id;
    for (id = FAMILY_POOL_FIRST; id <= FAMILY_POOL_LAST; id++)
        if (find_slot(id) < 0) return id;
    return -1;
}


/* ---- pool identities ---------------------------------------------------------- */
static int name_ok(const char *name)
{
    size_t i, n;
    if (!name) return 0;
    n = strlen(name);
    if (n < 1 || n > DOOMO_NAME_LEN - 1) return 0;
    for (i = 0; i < n; i++)
        if ((unsigned char)name[i] < 0x20 || (unsigned char)name[i] > 0x7E) return 0;
    return 1;
}

/* the identity of pool id from seed (the generated name / pronoun / romance),
 * with the caller's name and pronoun on top when given */
static void make_ident(int id, uint32_t seed, const char *name, int pronoun)
{
    DoomoResident r;
    FamilyIdentity *f = &ident_[id - FAMILY_POOL_FIRST];
    resident_generate(&r, seed);
    resident_sanitize(&r);
    memset(f, 0, sizeof(*f));
    f->seed = seed;
    memcpy(f->name, r.name, DOOMO_NAME_LEN);
    f->pronoun = r.pronoun;
    f->romance = (uint8_t)RES_ROMANCE(&r);
    if (name) {
        memset(f->name, 0, sizeof(f->name));
        memcpy(f->name, name, strlen(name));
    }
    if (pronoun >= 0 && pronoun < PRONOUN_COUNT) f->pronoun = (uint8_t)pronoun;
    ident_mask_ |= 1u << (id - FAMILY_POOL_FIRST);
}

/* the town's seed and the id: every household differs, a household is stable */
static uint32_t default_seed(int id)
{
    return doomo_town.seed ^ ((uint32_t)id * 0x9E3779B1u) ^ 0xFA11C0DEu;
}

static FamilyIdentity *ident_for(int id)
{
    if (!(ident_mask_ & (1u << (id - FAMILY_POOL_FIRST)))) make_ident(id, default_seed(id), NULL, -1);
    return &ident_[id - FAMILY_POOL_FIRST];
}

static void ident_drop(int id)
{
    if (!is_pool(id)) return;
    ident_mask_ &= ~(1u << (id - FAMILY_POOL_FIRST));
    memset(&ident_[id - FAMILY_POOL_FIRST], 0, sizeof(ident_[0]));
}

/* ---- the rules -------------------------------------------------------------- */
/* May `id` (with stage `stage`) hold `role`, ignoring the member's own current
 * role `self` (a slot or -1)? 0 or FAM_ERR_*. */
static int role_ok(int id, int stage, int role, int self)
{
    int i;
    if (role <= FAMROLE_NONE || role >= FAMROLE_COUNT) return FAM_ERR_ROLE;
    if ((role == FAMROLE_PET) != 0 && !is_pool(id)) return FAM_ERR_ROLE;   /* pets are pool members */
    if (role != FAMROLE_PARTNER) return 0;
    for (i = 0; i < fam_n_; i++)
        if (i != self && fam_[i].role == FAMROLE_PARTNER) return FAM_ERR_ROLE;   /* one partner */
    /* LS0: adults only, both ways; a resident also passes the full gate */
    if (!LIFE_IS_ADULT(stage) || !LIFE_IS_ADULT(sim_life_stage(DOOMO_PLAYER_ID))) return FAM_ERR_GATE;
    if (id < DOOMO_MAX_RESIDENTS &&
        !(sim_romance_ok(DOOMO_PLAYER_ID, id) && sim_romance_ok(id, DOOMO_PLAYER_ID)))
        return FAM_ERR_GATE;
    return 0;
}

static int stage_of(int id, int stage)
{
    if (id < DOOMO_MAX_RESIDENTS) return sim_life_stage(id);
    return (stage >= 0 && stage < LIFE_COUNT) ? stage : LIFE_ADULT;
}

int family_add(int id, int role, int stage)
{
    FamilyMember *m;
    int st, r;
    if (!family_id_ok(id)) return FAM_ERR_ID;
    if (id < DOOMO_MAX_RESIDENTS && !res_lives_(town_res(id))) return FAM_ERR_ID;
    if (find_slot(id) >= 0) return FAM_ERR_DUP;
    if (fam_n_ >= FAMILY_MAX || fam_n_ >= family_cap()) return FAM_ERR_FULL;
    st = stage_of(id, stage);
    if ((r = role_ok(id, st, role, -1)) != 0) return r;
    m = &fam_[fam_n_];
    memset(m, 0, sizeof(*m));
    m->id = (uint16_t)id;
    m->role = (uint8_t)role;
    m->stage = (uint8_t)st;
    m->since = (uint16_t)(doomo_town.day > 0xFFFFu ? 0xFFFFu : doomo_town.day);
    if (is_pool(id)) make_ident(id, default_seed(id), NULL, -1);   /* never a stale identity */
    bond_[fam_n_] = BOND_DEFAULT;
    return fam_n_++;
}

int family_pool_add(int role, int stage, uint32_t seed, const char *name, int pronoun)
{
    int id = family_pool_alloc(), r;
    if (id < 0) return FAM_ERR_FULL;
    if (name && !name_ok(name)) return FAM_ERR_NAME;
    if ((r = family_add(id, role, stage)) < 0) return r;
    make_ident(id, seed, name, pronoun);
    return id;
}

const FamilyIdentity *family_identity(int id)
{
    return (is_pool(id) && find_slot(id) >= 0) ? ident_for(id) : NULL;
}

int family_set_name(int id, const char *name)
{
    FamilyIdentity *f;
    if (!is_pool(id) || find_slot(id) < 0) return FAM_ERR_ID;
    if (!name_ok(name)) return FAM_ERR_NAME;
    f = ident_for(id);
    memset(f->name, 0, sizeof(f->name));
    memcpy(f->name, name, strlen(name));
    return 0;
}

int family_resident(int id, DoomoResident *out)
{
    int i = find_slot(id);
    const FamilyIdentity *f;
    if (i < 0 || !out) return 0;
    if (id < DOOMO_MAX_RESIDENTS) {
        const DoomoResident *r = town_res(id);
        if (!r) return 0;
        *out = *r;
        return 1;
    }
    if (fam_[i].role == FAMROLE_PET) return 0;
    f = ident_for(id);
    resident_generate(out, f->seed);
    memset(out->name, 0, sizeof(out->name));
    memcpy(out->name, f->name, strlen(f->name));
    out->pronoun = f->pronoun;
    resident_set_romance(out, f->romance);
    resident_set_stage(out, fam_[i].stage);
    out->status = RES_ALIVE;
    resident_sanitize(out);
    return 1;
}

int family_set_role(int id, int role)
{
    int i = find_slot(id), r;
    if (i < 0) return FAM_ERR_ID;
    if ((r = role_ok(id, stage_of(id, fam_[i].stage), role, i)) != 0) return r;
    fam_[i].role = (uint8_t)role;
    return 0;
}

int family_remove(int id)
{
    int i = find_slot(id);
    if (i < 0) return 0;
    ident_drop(id);
    memmove(&fam_[i], &fam_[i + 1], (size_t)(fam_n_ - i - 1) * sizeof(fam_[0]));
    memmove(&bond_[i], &bond_[i + 1], (size_t)(fam_n_ - i - 1));
    bond_[fam_n_ - 1] = BOND_DEFAULT;
    fam_n_--;
    memset(&fam_[fam_n_], 0, sizeof(fam_[0]));
    return 1;
}

void family_clear(void)
{
    neglect_grace_ = 0;
    sq_on_ = 0;
    dis_kind_ = 0;
    will_ = FAMILY_WILL_NORMAL;
    memset(fam_, 0, sizeof(fam_));
    memset(bond_, BOND_DEFAULT, sizeof(bond_));
    fam_n_ = 0;
    family_pool_reset();
}

/* ---- F4 part 1: the life cycle (family.h) ------------------------------------- */
static const uint8_t stage_days_[LIFE_COUNT] = {
    0,      /* ADULT: does not age here  */
    4,      /* BABY                      */
    6,      /* TODDLER                   */
    10,     /* CHILD                     */
    10,     /* TEEN                      */
    0       /* ELDER                     */
};
static uint16_t aged_id_[FAMILY_MAX];
static uint8_t  aged_st_[FAMILY_MAX];
static uint8_t  aged_n_;

int family_stage_days(int stage)
{
    return (unsigned)stage < LIFE_COUNT ? stage_days_[stage] : 0;
}

static void aged_note_(const FamilyMember *m)
{
    if (aged_n_ < FAMILY_MAX) {
        aged_id_[aged_n_] = m->id;
        aged_st_[aged_n_] = m->stage;
        aged_n_++;
    }
}

static const char *member_name_(int id)
{
    if (id < DOOMO_MAX_RESIDENTS) return town_res(id)->name;
    return is_pool(id) ? ident_for(id)->name : "";
}

/* "Mo grew up! Now: Toddler" */
static void aged_toast_(int n)
{
    char msg[64];
    doomo_snprintf(msg, sizeof(msg), STR_ICON_STAR " %s grew up! Now: %s",
                   member_name_(aged_id_[n]), doomo_life_stage_names[aged_st_[n]]);
    ui_toast(msg);
}

int family_age_daily(void)
{
    int i;
    aged_n_ = 0;
    for (i = 0; i < fam_n_; i++) {
        FamilyMember *m = &fam_[i];
        int days, p;
        if (m->role == FAMROLE_PET) continue;
        if (m->id < DOOMO_MAX_RESIDENTS) {              /* a resident: the sim owns the stage */
            int st = sim_life_stage(m->id);
            if (st != m->stage) {
                m->stage = (uint8_t)st;
                m->flags = (uint8_t)(m->flags & ~FAMILY_FL_AGE);
                aged_note_(m);
            }
            continue;
        }
        days = family_stage_days(m->stage);
        if (days <= 0) continue;
        p = (int)(m->flags & FAMILY_FL_AGE) + 1;
        if (p < days) {
            m->flags = (uint8_t)((m->flags & ~FAMILY_FL_AGE) | (unsigned)p);
            continue;
        }
        m->stage = (uint8_t)life_stage_next(m->stage);
        m->flags = (uint8_t)(m->flags & ~FAMILY_FL_AGE);
        aged_note_(m);
    }
    for (i = 0; i < aged_n_; i++) {
        aged_toast_(i);
        portrait_invalidate(aged_id_[i]);     /* their old face and body are stale */
    }
    return aged_n_;
}

int family_birth(const char *name, int pronoun)
{
    int id, r;
    char msg[64];
    if (family_count_role(FAMROLE_PARTNER) < 1) return FAM_ERR_NOPARTNER;
    if (!LIFE_IS_ADULT(sim_life_stage(DOOMO_PLAYER_ID))) return FAM_ERR_GATE;
    id = family_pool_alloc();
    if (id < 0) return FAM_ERR_FULL;
    r = family_pool_add(FAMROLE_CHILD, LIFE_BABY,
                        default_seed(id) ^ ((uint32_t)doomo_town.day * 0x85EBCA6Bu), name, pronoun);
    if (r < 0) return r;
    doomo_snprintf(msg, sizeof(msg), STR_ICON_HEART " It's a baby! Welcome, %s!", family_identity(r)->name);
    ui_toast(msg);
    audio_sfx(SFX_SPARKLE);
    return r;
}

int family_aged_count(void) { return aged_n_; }
int family_aged_id(int n) { return (n >= 0 && n < aged_n_) ? (int)aged_id_[n] : -1; }
int family_aged_stage(int n) { return (n >= 0 && n < aged_n_) ? (int)aged_st_[n] : -1; }

int family_stage_progress(int id)
{
    int i = find_slot(id);
    return i < 0 ? 0 : (int)(fam_[i].flags & FAMILY_FL_AGE);
}

/* ---- life.h: the strong versions ------------------------------------------------ */
int family_count(void) { return fam_n_; }

int family_member(int i) { return (i >= 0 && i < fam_n_) ? (int)fam_[i].id : -1; }

int life_player_partner(void)
{
    int i;
    for (i = 0; i < fam_n_; i++)
        if (fam_[i].role == FAMROLE_PARTNER) return fam_[i].id;
    return -1;
}

int life_family_count(void) { return fam_n_ ? fam_n_ + 1 : 0; }

/* life.h: the facts the ending resolver reads (partner, household, kids). */
void life_ending_facts(LifeEndingFacts *out)
{
    int i, kids = 0;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->partner = (int16_t)life_player_partner();
    for (i = 0; i < fam_n_; i++)
        if (fam_[i].role == FAMROLE_CHILD) kids++;
    out->household = (uint8_t)life_family_count();
    out->kids = (uint8_t)kids;
    for (i = 0; i < family_memorial_count(); i++) {
        const FamilyMemorial *m = family_memorial(i);
        if (m && m->cause == FAMLOSS_NEGLECT && out->neglect_deaths < 255) out->neglect_deaths++;
    }
    {
        const DoomoSimStats *st = sim_stats();
        if (st) {
            out->couples = (uint8_t)(st->marriages > 255 ? 255 : st->marriages);
            out->exorcisms_won = (uint8_t)(st->exorcisms_auto > 255 ? 255 : st->exorcisms_auto);
        }
    }
}

/* ---- the LIFE chunk's section: [count u8][reserved u8] then 8 B per member ------ */
uint32_t family_save_size(void)
{
    return fam_n_ ? FAMILY_SEC_HEAD + 8u * fam_n_ : 0;
}

uint32_t family_save(uint8_t *dst, uint32_t cap)
{
    uint32_t o = FAMILY_SEC_HEAD;
    int i;
    if (!fam_n_ || cap < family_save_size()) return 0;
    dst[0] = fam_n_;
    dst[1] = will_ == FAMILY_WILL_NORMAL ? 0 : (uint8_t)(will_ + 1);   /* 0 = the default */
    for (i = 0; i < fam_n_; i++, o += 8) {
        dst[o] = (uint8_t)fam_[i].id;
        dst[o + 1] = (uint8_t)(fam_[i].id >> 8);
        dst[o + 2] = fam_[i].role;
        dst[o + 3] = fam_[i].stage;
        dst[o + 4] = fam_[i].flags;
        dst[o + 5] = 0;
        dst[o + 6] = (uint8_t)fam_[i].since;
        dst[o + 7] = (uint8_t)(fam_[i].since >> 8);
    }
    return o;
}

/* Rejects (and resets) a section that is cut short, too big for this
 * platform, repeats an id, has a bad id / role / stage, a pet that is not a
 * pool member or more than one partner. It does not re-apply the gate: a
 * saved partner was legal when chosen (sim_romance_sweep keeps the sim
 * safe) and the cap may have been higher on another platform. */
int family_load(const uint8_t *src, uint32_t len)
{
    FamilyMember tmp[FAMILY_MAX];
    int n, i, j, partners = 0;
    if (len < FAMILY_SEC_HEAD) return 0;
    n = src[0];
    if (n < 1 || n > FAMILY_MAX || len != FAMILY_SEC_HEAD + 8u * (uint32_t)n) return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *p = src + FAMILY_SEC_HEAD + 8 * i;
        tmp[i].id = (uint16_t)(p[0] | (p[1] << 8));
        tmp[i].role = p[2];
        tmp[i].stage = p[3];
        tmp[i].flags = p[4];
        tmp[i].spare = 0;
        tmp[i].since = (uint16_t)(p[6] | (p[7] << 8));
        if (!family_id_ok(tmp[i].id) || tmp[i].stage >= LIFE_COUNT) return 0;
        if (tmp[i].role <= FAMROLE_NONE || tmp[i].role >= FAMROLE_COUNT) return 0;
        if (tmp[i].role == FAMROLE_PET && !is_pool(tmp[i].id)) return 0;
        if (tmp[i].role == FAMROLE_PARTNER && ++partners > 1) return 0;
        for (j = 0; j < i; j++)
            if (tmp[j].id == tmp[i].id) return 0;
    }
    memset(fam_, 0, sizeof(fam_));
    memcpy(fam_, tmp, (size_t)n * sizeof(tmp[0]));
    memset(bond_, BOND_DEFAULT, sizeof(bond_));
    fam_n_ = (uint8_t)n;
    will_ = (uint8_t)((src[1] >= 1 && src[1] <= 5) ? src[1] - 1 : FAMILY_WILL_NORMAL);
    dis_kind_ = 0;
    sq_on_ = 0;
    neglect_grace_ = 1;                 /* F6: a load never costs a day of care */
    return 1;
}


/* ---- the POOL section: [count u8][reserved u8], then per pool member in table
 * order [id u16][seed u32][name 12][pronoun u8][romance u8][0][0] ---------------- */
static int pool_members(void)
{
    int i, n = 0;
    for (i = 0; i < fam_n_; i++)
        if (is_pool(fam_[i].id)) n++;
    return n;
}

uint32_t family_pool_save_size(void)
{
    int n = pool_members();
    return n ? FAMILY_SEC_HEAD + FAMILY_POOL_REC * (uint32_t)n : 0;
}

uint32_t family_pool_save(uint8_t *dst, uint32_t cap)
{
    uint32_t o = FAMILY_SEC_HEAD;
    int i;
    if (!pool_members() || cap < family_pool_save_size()) return 0;
    dst[0] = (uint8_t)pool_members();
    dst[1] = 0;
    for (i = 0; i < fam_n_; i++) {
        const FamilyIdentity *f;
        if (!is_pool(fam_[i].id)) continue;
        f = ident_for(fam_[i].id);
        dst[o] = (uint8_t)fam_[i].id;
        dst[o + 1] = (uint8_t)(fam_[i].id >> 8);
        dst[o + 2] = (uint8_t)f->seed;
        dst[o + 3] = (uint8_t)(f->seed >> 8);
        dst[o + 4] = (uint8_t)(f->seed >> 16);
        dst[o + 5] = (uint8_t)(f->seed >> 24);
        memcpy(dst + o + 6, f->name, DOOMO_NAME_LEN);
        dst[o + 18] = f->pronoun;
        dst[o + 19] = f->romance;
        dst[o + 20] = dst[o + 21] = 0;
        o += FAMILY_POOL_REC;
    }
    return o;
}

void family_pool_reset(void)
{
    memset(ident_, 0, sizeof(ident_));
    ident_mask_ = 0;
}

/* Rejects a section that is cut short, has too many entries for this
 * platform, repeats an id, names an id outside the pool, or holds an
 * identity that is not valid (a name that is empty, not NUL padded or not
 * printable ASCII, a pronoun or romance out of range). The entries are kept
 * by id and need not be members yet: the family section may load after. */
int family_pool_load(const uint8_t *src, uint32_t len)
{
    FamilyIdentity tmp[FAMILY_MAX];
    uint32_t seen = 0, mask = 0;
    int n, i, j;
    if (len < FAMILY_SEC_HEAD) return 0;
    n = src[0];
    if (n < 1 || n > FAMILY_MAX || len != FAMILY_SEC_HEAD + FAMILY_POOL_REC * (uint32_t)n) return 0;
    memset(tmp, 0, sizeof(tmp));
    for (i = 0; i < n; i++) {
        const uint8_t *p = src + FAMILY_SEC_HEAD + FAMILY_POOL_REC * i;
        int id = p[0] | (p[1] << 8), k, len_name = 0;
        FamilyIdentity *f;
        if (!is_pool(id)) return 0;
        k = id - FAMILY_POOL_FIRST;
        if (seen & (1u << k)) return 0;
        seen |= 1u << k;
        f = &tmp[k];
        f->seed = (uint32_t)p[2] | ((uint32_t)p[3] << 8) | ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24);
        memcpy(f->name, p + 6, DOOMO_NAME_LEN);
        f->pronoun = p[18];
        f->romance = p[19];
        while (len_name < DOOMO_NAME_LEN && f->name[len_name]) len_name++;
        if (len_name < 1 || len_name > DOOMO_NAME_LEN - 1) return 0;
        for (j = 0; j < len_name; j++)
            if ((unsigned char)f->name[j] < 0x20 || (unsigned char)f->name[j] > 0x7E) return 0;
        for (j = len_name; j < DOOMO_NAME_LEN; j++)
            if (f->name[j]) return 0;
        if (f->pronoun >= PRONOUN_COUNT || f->romance >= ROMANCE_COUNT || p[20] || p[21]) return 0;
        mask |= 1u << k;
    }
    memcpy(ident_, tmp, sizeof(ident_));
    ident_mask_ = mask;
    return 1;
}

typedef char family_worst_fits_[(LIFE_SEC_HDR + FAMILY_SZ_WORST <= LIFE_SZ_LS1_PLAN - LIFE_SEC_HDR - LIFE_SZ_NEWS) ? 1 : -1];
typedef char family_rec_is_8_[(sizeof(FamilyMember) == 8) ? 1 : -1];
typedef char family_pool_fits_[(LIFE_SEC_HDR + FAMILY_SZ_WORST + LIFE_SEC_HDR + FAMILY_POOL_SZ_WORST + LIFE_SEC_HDR + LIFE_SZ_NEWS + 8u <= LIFE_SZ_LS1_PLAN) ? 1 : -1];
typedef char family_ident_is_20_[(sizeof(FamilyIdentity) == 20) ? 1 : -1];

/* ---- F6: loss (family.h) --------------------------------------------------------- */
static FamilyMemorial mem_[FAMILY_MEMORIAL_MAX];   /* oldest first */
static uint8_t mem_n_;

void family_loss_set(int on) { loss_on_ = (uint8_t)(on != 0); }
int  family_loss_enabled(void) { return loss_on_; }

static int is_minor_member_(const FamilyMember *m)
{
    return m->role != FAMROLE_PET && LIFE_IS_MINOR(m->stage);
}

/* does the pool member keep the name its id would be generated with? */
static int default_name_(int id)
{
    DoomoResident r;
    const FamilyIdentity *f = ident_for(id);
    if (f->seed != default_seed(id)) return 0;
    resident_generate(&r, f->seed);
    resident_sanitize(&r);
    return strncmp(f->name, r.name, DOOMO_NAME_LEN) == 0;
}

static void memorial_add_(const FamilyMember *m, int cause)
{
    FamilyMemorial *e;
    if (mem_n_ >= FAMILY_MEMORIAL_MAX) {
        memmove(&mem_[0], &mem_[1], (size_t)(FAMILY_MEMORIAL_MAX - 1) * sizeof(mem_[0]));
        mem_n_ = FAMILY_MEMORIAL_MAX - 1;
    }
    e = &mem_[mem_n_++];
    memset(e, 0, sizeof(*e));
    e->id = m->id;
    e->day = (uint16_t)(doomo_town.day > 0xFFFFu ? 0xFFFFu : doomo_town.day);
    e->cause = (uint8_t)cause;
    e->bits = (uint8_t)((m->stage & 7) | ((m->role & 7) << 3));
    if (is_pool(m->id)) {
        e->bits |= FAMILY_MEM_POOL;
        if (default_name_(m->id)) e->bits |= FAMILY_MEM_DEFNAME;
    }
}

int family_lose(int id, int cause)
{
    char msg[64], name[DOOMO_NAME_LEN + 1];
    int minor_gone;
    int i = find_slot(id);
    if (i < 0 || cause < FAMLOSS_AGE || cause >= FAMLOSS_COUNT) return 0;
    /* R5 (amended by the owner): a child can be lost to illness or Hell, only while Loss is on,
     * never to old age or neglect; always off-screen and gentle. */
    if (is_minor_member_(&fam_[i]) && cause != FAMLOSS_GONE &&
        (!loss_on_ || (cause != FAMLOSS_ILLNESS && cause != FAMLOSS_DEMON))) return 0;
    if (fam_[i].role == FAMROLE_PARTNER && cause != FAMLOSS_GONE)
        sim_news_push("You are grieving. Your strength is down for a while.");
    minor_gone = is_minor_member_(&fam_[i]) && cause != FAMLOSS_GONE;
    memcpy(name, member_name_(id), DOOMO_NAME_LEN);
    name[DOOMO_NAME_LEN] = 0;
    memorial_add_(&fam_[i], cause);
    family_remove(id);
    if (minor_gone) {
        doomo_snprintf(msg, sizeof(msg), "%s has gone to rest. Always remembered.", name);
    } else switch (cause) {
    case FAMLOSS_AGE:      doomo_snprintf(msg, sizeof(msg), "%s passed peacefully. Rest well.", name); break;
    case FAMLOSS_NEGLECT:  doomo_snprintf(msg, sizeof(msg), "%s didn't make it. Look after the others.", name); break;
    case FAMLOSS_DEMON:    doomo_snprintf(msg, sizeof(msg), "%s was lost to Hell. They're missed.", name); break;
    case FAMLOSS_GONE:     doomo_snprintf(msg, sizeof(msg), "%s moved away. Visit soon!", name); break;
    default:               doomo_snprintf(msg, sizeof(msg), "%s passed away. They're missed.", name); break;
    }
    ui_toast(msg);
    return 1;
}

void family_care(int id)
{
    int i = find_slot(id);
    if (i < 0) return;
    fam_[i].spare = 1;
    if ((bond_[i] & 15u) < FAMILY_BOND_MAX) bond_[i]++;
    if ((bond_[i] >> 4) < 15u) bond_[i] = (uint8_t)(bond_[i] + 0x10u);
    fam_[i].flags = (uint8_t)(fam_[i].flags & ~FAMILY_FL_NEGLECT);
}

int family_loss_daily(void)
{
    int i, lost = 0;
    if (neglect_grace_) {                       /* the day a save was loaded */
        neglect_grace_ = 0;
        for (i = 0; i < fam_n_; i++) fam_[i].spare = 0;
        return 0;
    }
    for (i = 0; i < fam_n_; ) {
        FamilyMember *m = &fam_[i];
        int lvl, cap;
        if (!is_pool(m->id)) { m->spare = 0; i++; continue; }   /* residents: the sim's */
        /* neglect: the count rises a day at a time unless someone cared */
        lvl = (int)((m->flags & FAMILY_FL_NEGLECT) >> FAMILY_NEGLECT_SHIFT);
        cap = is_minor_member_(m) ? FAMILY_NEGLECT_WARN + 2 : FAMILY_NEGLECT_LOSS;
        if (m->spare) lvl = 0;
        else if (lvl < cap) lvl++;
        m->spare = 0;
        m->flags = (uint8_t)((m->flags & ~FAMILY_FL_NEGLECT) | ((unsigned)lvl << FAMILY_NEGLECT_SHIFT));
        if (loss_on_ && lvl >= FAMILY_NEGLECT_LOSS) {
            lost += family_lose(m->id, FAMLOSS_NEGLECT);   /* removes the slot: do not advance */
            continue;
        }
        if (loss_on_ && lvl == FAMILY_NEGLECT_WARN) {
            char msg[64];
            doomo_snprintf(msg, sizeof(msg), STR_ICON_HEART " %s needs you! Go and care for them.",
                           member_name_(m->id));
            ui_toast(msg);
        }
        /* old age: a pool ELDER (not a pet) spends FAMILY_ELDER_DAYS here */
        if (loss_on_ && m->role != FAMROLE_PET && m->stage == LIFE_ELDER) {
            int p = (int)(m->flags & FAMILY_FL_AGE) + 1;
            if (p >= FAMILY_ELDER_DAYS) {
                lost += family_lose(m->id, FAMLOSS_AGE);
                continue;
            }
            m->flags = (uint8_t)((m->flags & ~FAMILY_FL_AGE) | (unsigned)p);
        }
        i++;
    }
    return lost;
}

/* ---- F7/F8: bond and mood -------------------------------------------------------- */
int family_bond(int id) { int i = find_slot(id); return i < 0 ? 0 : bond_[i] & 15; }
int family_mood(int id) { int i = find_slot(id); return i < 0 ? 0 : bond_[i] >> 4; }

void family_bond_add(int id, int delta)
{
    int i = find_slot(id), b;
    if (i < 0) return;
    b = (bond_[i] & 15) + delta;
    b = b < 0 ? 0 : b > FAMILY_BOND_MAX ? FAMILY_BOND_MAX : b;
    bond_[i] = (uint8_t)((bond_[i] & 0xF0u) | (unsigned)b);
}

const char *family_bond_line(int id)
{
    int b = family_bond(id);
    if (!family_find(id) || id == playas_avatar()) return NULL;
    if (b >= 10) return "Close to you";
    if (b <= 1) return "Wants more time with you";
    return NULL;
}

/* F8: who a member is closest to among the housemates. Derived from the two
 * members' bonds with the player (nothing is saved, no RNG): the housemate with
 * the highest combined bond, both at 8+, the lower table slot winning a tie.
 * The player's own character is not a candidate (their bond is not tracked). */
int family_close_id(int id)
{
    int i = find_slot(id), j, best = -1, score = 0;
    if (i < 0 || id == playas_avatar()) return -1;
    if ((bond_[i] & 15) < 8) return -1;
    for (j = 0; j < fam_n_; j++) {
        int s;
        if (j == i || fam_[j].id == playas_avatar() || (bond_[j] & 15) < 8) continue;
        s = (bond_[i] & 15) + (bond_[j] & 15);
        if (s > score) { score = s; best = fam_[j].id; }
    }
    return best;
}

const char *family_close_line(int id)
{
    static char line[DOOMO_NAME_LEN + 12];
    int o = family_close_id(id);
    if (o < 0) return NULL;
    doomo_snprintf(line, sizeof(line), "Close to %s", member_name_(o));
    return line;
}

uint32_t family_bond_save_size(void)
{
    int i;
    for (i = 0; i < fam_n_; i++) if (bond_[i] != BOND_DEFAULT) return (uint32_t)fam_n_;
    return 0;
}

uint32_t family_bond_save(uint8_t *dst, uint32_t cap)
{
    uint32_t n = family_bond_save_size();
    if (!n || cap < n) return 0;
    memcpy(dst, bond_, n);
    return n;
}

int family_bond_load(const uint8_t *src, uint32_t len)
{
    if (len != (uint32_t)fam_n_ || len == 0) return 0;     /* the family section comes first */
    memcpy(bond_, src, len);
    return 1;
}

void family_bond_clear(void) { memset(bond_, BOND_DEFAULT, sizeof(bond_)); }

/* A day nobody cared for a pool member wears the bond down a little. */
static void bond_daily_(void)
{
    int i;
    if (will_ == FAMILY_WILL_OFF) return;
    for (i = 0; i < fam_n_; i++) {
        if (!is_pool(fam_[i].id) || fam_[i].spare || fam_[i].id == playas_avatar()) continue;
        if ((bond_[i] & 15u) > 0 && sim_chance(25)) bond_[i]--;
    }
}

/* ---- grief (derived from the memorials: nothing saved) ---------------------- */
int family_grief_days_left(void)
{
    int n;
    for (n = 0; n < mem_n_; n++) {                 /* newest first */
        const FamilyMemorial *e = family_memorial(n);
        int days;
        if (!e || e->cause == FAMLOSS_GONE || family_memorial_role(n) != FAMROLE_PARTNER) continue;
        days = (int)doomo_town.day - (int)e->day;
        if (days < 0) days = 0;
        return days < FAMILY_GRIEF_DAYS ? FAMILY_GRIEF_DAYS - days : 0;   /* the newest one decides */
    }
    return 0;
}

int family_grief_damage_pct(void)
{
    int left = family_grief_days_left();
    return left ? 50 + 50 * (FAMILY_GRIEF_DAYS - left) / FAMILY_GRIEF_DAYS : 100;
}

int family_grief_hurt_pct(void)
{
    return 100 + (100 - family_grief_damage_pct()) / 2;
}

/* ---- F10: disasters ----------------------------------------------------------- */
static const char *const dis_warn_[FAMDIS_COUNT] = { "",
    "An illness is going around. %s looks unwell!", "A demon night is coming. %s is in danger!",
    "A storm is rolling in. %s is scared!", "Smoke! %s is near a fire!", "The power is out. %s is in the dark!" };

int family_disaster_pending(void) { return dis_kind_; }
int family_disaster_target(void) { return dis_kind_ ? (int)dis_id_ : -1; }

int family_disaster_start(int kind, int id)
{
    char msg[NEWS_LEN];
    if (dis_kind_ || kind <= FAMDIS_NONE || kind >= FAMDIS_COUNT || find_slot(id) < 0) return 0;
    dis_kind_ = (uint8_t)kind;
    dis_id_ = (uint16_t)id;
    doomo_snprintf(msg, sizeof(msg), dis_warn_[kind], member_name_(id));
    ui_toast(msg);
    sim_news_push(msg);
    return 1;
}

int family_disaster_daily(void)
{
    int i, lost = 0;
    if (!loss_on_) { dis_kind_ = 0; return 0; }
    if (dis_kind_) {
        int kind = dis_kind_, id = dis_id_;
        char msg[NEWS_LEN];
        dis_kind_ = 0;
        i = find_slot(id);
        if (i < 0) return 0;
        if (fam_[i].spare) {                           /* cared for in time */
            doomo_snprintf(msg, sizeof(msg), "%s is safe. Good thing you were there!", member_name_(id));
            sim_news_push(msg);
            return 0;
        }
        if (kind == FAMDIS_ILLNESS || kind == FAMDIS_DEMON) {
            int lethal = !is_minor_member_(&fam_[i]) || sim_chance(33);
            if (lethal) {
                lost += family_lose(id, kind == FAMDIS_DEMON ? FAMLOSS_DEMON : FAMLOSS_ILLNESS);
                return lost;
            }
            doomo_snprintf(msg, sizeof(msg), "%s pulled through. Phew!", member_name_(id));
        } else {
            doomo_snprintf(msg, sizeof(msg), "%s got through it, but is shaken.", member_name_(id));
        }
        if ((bond_[i] >> 4) >= 3) bond_[i] = (uint8_t)(bond_[i] - 0x30u);   /* mood drops */
        sim_news_push(msg);
        return 0;
    }
    if (fam_n_ && sim_chance(4)) {                     /* a new one: never the avatar */
        int cands[FAMILY_MAX], nc = 0;
        for (i = 0; i < fam_n_; i++) if (fam_[i].id != playas_avatar()) cands[nc++] = i;
        if (nc) family_disaster_start(FAMDIS_ILLNESS + sim_rand(FAMDIS_COUNT - 1), fam_[cands[sim_rand(nc)]].id);
    }
    return 0;
}

/* ---- F8: bond-gated events ---------------------------------------------------- */
static void mood_add_(int i, int d)
{
    int m = (bond_[i] >> 4) + d;
    m = m < 0 ? 0 : m > 15 ? 15 : m;
    bond_[i] = (uint8_t)((bond_[i] & 15u) | ((unsigned)m << 4));
}

static void news2_(const char *fmt, int a, int b)
{
    char msg[NEWS_LEN], n1[DOOMO_NAME_LEN + 1], n2[DOOMO_NAME_LEN + 1];
    memcpy(n1, member_name_(a), DOOMO_NAME_LEN); n1[DOOMO_NAME_LEN] = 0;
    memcpy(n2, member_name_(b), DOOMO_NAME_LEN); n2[DOOMO_NAME_LEN] = 0;
    doomo_snprintf(msg, sizeof(msg), fmt, n1, n2);
    sim_news_push(msg);
}

int family_squabble_pending(void) { return sq_on_; }

int family_squabble_start(int a, int b)
{
    int i = find_slot(a), j = find_slot(b);
    if (sq_on_ || a == b || i < 0 || j < 0 || fam_[i].role == FAMROLE_PET || fam_[j].role == FAMROLE_PET) return 0;
    sq_on_ = 1; sq_a_ = (uint16_t)a; sq_b_ = (uint16_t)b;
    mood_add_(i, -3); mood_add_(j, -3);
    if (will_ != FAMILY_WILL_SUBCONSCIOUS) news2_("%s and %s had a little squabble.", a, b);
    return 1;
}

int family_events_daily(void)
{
    static const uint8_t pct[5] = { 0, 5, 10, 16, 20 };
    int i, shown = 0;
    if (will_ == FAMILY_WILL_OFF) { sq_on_ = 0; return 0; }
    if (sq_on_) {                                           /* the day after: make up */
        int a = find_slot(sq_a_), b = find_slot(sq_b_);
        sq_on_ = 0;
        if (a >= 0 && b >= 0) {
            mood_add_(a, 5); mood_add_(b, 5);
            if (will_ != FAMILY_WILL_SUBCONSCIOUS) news2_("%s and %s made up. Hugs all round!", sq_a_, sq_b_);
            shown++;
        }
        return shown;
    }
    if (fam_n_ >= 1 && sim_chance(pct[will_])) {            /* a confidence from someone close */
        int cands[FAMILY_MAX], nc = 0;
        for (i = 0; i < fam_n_; i++)
            if (fam_[i].id != playas_avatar() && fam_[i].role != FAMROLE_PET && (bond_[i] & 15u) >= 10) cands[nc++] = i;
        if (nc) {
            i = cands[sim_rand(nc)];
            mood_add_(i, 3);
            if (will_ != FAMILY_WILL_SUBCONSCIOUS) {
                char msg[NEWS_LEN];
                doomo_snprintf(msg, sizeof(msg), "%s has a secret to tell you. They trust you!", member_name_(fam_[i].id));
                sim_news_push(msg);
            }
            return 1;
        }
    }
    if (fam_n_ >= 2 && sim_chance(pct[will_] / 2)) {        /* two housemates squabble */
        int cands[FAMILY_MAX], nc = 0;
        for (i = 0; i < fam_n_; i++)
            if (fam_[i].id != playas_avatar() && fam_[i].role != FAMROLE_PET) cands[nc++] = i;
        if (nc >= 2) {
            int x = sim_rand(nc), y = sim_rand(nc - 1);
            if (y >= x) y++;
            if (family_squabble_start(fam_[cands[x]].id, fam_[cands[y]].id)) shown++;
        }
    }
    return shown;
}

/* ---- F9: household helpers ------------------------------------------------------- */
int family_help_do(int id)
{
    int i = find_slot(id), kind;
    char msg[NEWS_LEN];
    if (i < 0 || fam_[i].role == FAMROLE_PET || fam_[i].id == playas_avatar() || (bond_[i] & 15u) < 5) return 0;
    if (fam_[i].stage == LIFE_BABY || fam_[i].stage == LIFE_TODDLER) {
        doomo_snprintf(msg, sizeof(msg), "%s gave you a big smile today.", member_name_(id));
        kind = 3;
    } else if (fam_[i].stage == LIFE_CHILD) {
        static const uint8_t gifts[3] = { ITEM_CUPCAKE, ITEM_FLOWERS, ITEM_PLUSHIE };
        inv_add(gifts[sim_rand(3)], 1);
        doomo_snprintf(msg, sizeof(msg), "%s left a little gift in your bag!", member_name_(id));
        kind = 2;
    } else {
        int h = 20 + sim_rand(41);
        doomo_town.money += (uint32_t)h;
        doomo_snprintf(msg, sizeof(msg), "%s helped out and brought home H$%d.", member_name_(id), h);
        kind = 1;
    }
    if (will_ != FAMILY_WILL_SUBCONSCIOUS) sim_news_push(msg);
    return kind;
}

int family_help_daily(void)
{
    static const uint8_t pct[5] = { 0, 3, 6, 10, 14 };
    int cands[FAMILY_MAX], nc = 0, i;
    if (will_ == FAMILY_WILL_OFF || !fam_n_) return 0;
    /* a bonus once the story is cleared; free play has no story to wait for */
    if (doomo_town.mode == TOWN_STORY && story_chapter() != STORY_CH_DONE) return 0;
    if (!sim_chance(pct[will_])) return 0;   /* no household: no RNG draw */
    for (i = 0; i < fam_n_; i++)
        if (fam_[i].id != playas_avatar() && fam_[i].role != FAMROLE_PET && (bond_[i] & 15u) >= 5) cands[nc++] = i;
    return nc ? family_help_do(fam_[cands[sim_rand(nc)]].id) != 0 : 0;
}

/* ---- F7: free will -------------------------------------------------------------- */
int family_freewill_level(void) { return will_; }

void family_freewill_set(int level)
{
    will_ = (uint8_t)(level < 0 ? 0 : level > FAMILY_WILL_SUBCONSCIOUS ? FAMILY_WILL_SUBCONSCIOUS : level);
}

/* How well a member is doing (0 = needs care badly, 100 = fine). */
static int wellbeing_(int id)
{
    const FamilyMember *m = family_find(id);
    const DoomoResident *r = id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    int i, best = 100;
    if (r) {
        for (i = 0; i < STAT_COUNT; i++) if (r->stat[i] < best) best = r->stat[i];
        return best;
    }
    if (!m) return 100;
    return 100 - 15 * (int)((m->flags & FAMILY_FL_NEGLECT) >> FAMILY_NEGLECT_SHIFT);
}

static int can_care_(const FamilyMember *m)
{
    const DoomoResident *r;
    if (m->role == FAMROLE_PET || m->id == playas_avatar()) return 0;
    if (m->stage != LIFE_ADULT && m->stage != LIFE_ELDER && m->stage != LIFE_TEEN) return 0;
    r = m->id < DOOMO_MAX_RESIDENTS ? town_res(m->id) : NULL;
    return !r || r->status == RES_ALIVE;
}

int family_freewill_daily(void)
{
    static const uint8_t pct[5] = { 0, 25, 50, 80, 100 };
    int lvl = will_, acts, done = 0, a;
    bond_daily_();                       /* before today's care marks are cleared */
    if (lvl == FAMILY_WILL_OFF || fam_n_ < 2) return 0;
    acts = lvl >= FAMILY_WILL_HIGH ? 2 : 1;
    for (a = 0; a < acts; a++) {
        int cands[FAMILY_MAX], nc = 0, i, who, to = -1, worst = 101;
        if (!sim_chance(pct[lvl])) continue;
        for (i = 0; i < fam_n_; i++) if (can_care_(&fam_[i])) cands[nc++] = i;
        if (!nc) continue;
        who = cands[sim_rand(nc)];
        for (i = 0; i < fam_n_; i++) {
            int w;
            if (i == who) continue;
            w = wellbeing_(fam_[i].id);
            if (w < worst) { worst = w; to = i; }
        }
        if (to < 0) continue;
        if (fam_[to].id < DOOMO_MAX_RESIDENTS && town_res(fam_[to].id)->status == RES_ALIVE)
            (void)sim_talk(fam_[to].id);
        family_care(fam_[to].id);
        done++;
        if (lvl != FAMILY_WILL_SUBCONSCIOUS) {
            char msg[NEWS_LEN];
            char n1[DOOMO_NAME_LEN + 1], n2[DOOMO_NAME_LEN + 1];
            memcpy(n1, member_name_(fam_[who].id), DOOMO_NAME_LEN); n1[DOOMO_NAME_LEN] = 0;
            memcpy(n2, member_name_(fam_[to].id), DOOMO_NAME_LEN); n2[DOOMO_NAME_LEN] = 0;
            doomo_snprintf(msg, sizeof(msg), "%s looked after %s today.", n1, n2);
            sim_news_push(msg);
        }
    }
    return done;
}

int family_memorial_count(void) { return mem_n_; }
const FamilyMemorial *family_memorial(int n)
{
    return (n >= 0 && n < mem_n_) ? &mem_[mem_n_ - 1 - n] : NULL;
}
int family_memorial_stage(int n) { const FamilyMemorial *e = family_memorial(n); return e ? (e->bits & 7) : -1; }
int family_memorial_role(int n)  { const FamilyMemorial *e = family_memorial(n); return e ? ((e->bits >> 3) & 7) : -1; }

int family_memorial_name(int n, char *out)
{
    const FamilyMemorial *e = family_memorial(n);
    DoomoResident r;
    if (!e || !out) return 0;
    memset(out, 0, DOOMO_NAME_LEN);
    if (e->id < DOOMO_MAX_RESIDENTS) {
        memcpy(out, town_res(e->id)->name, DOOMO_NAME_LEN);
        return out[0] != 0;
    }
    if (!(e->bits & FAMILY_MEM_DEFNAME)) return 0;
    resident_generate(&r, default_seed(e->id));
    resident_sanitize(&r);
    memcpy(out, r.name, DOOMO_NAME_LEN);
    return out[0] != 0;
}

/* LOSS section: [count u8][reserved u8], then per memorial, oldest first,
 * [id u16][day u16][cause u8][bits u8][0][0] */
uint32_t family_loss_save_size(void) { return mem_n_ ? 2u + 8u * mem_n_ : 0u; }

uint32_t family_loss_save(uint8_t *dst, uint32_t cap)
{
    uint32_t o = 2;
    int i;
    if (!mem_n_ || cap < family_loss_save_size()) return 0;
    dst[0] = mem_n_;
    dst[1] = 0;
    for (i = 0; i < mem_n_; i++, o += 8) {
        dst[o] = (uint8_t)mem_[i].id;
        dst[o + 1] = (uint8_t)(mem_[i].id >> 8);
        dst[o + 2] = (uint8_t)mem_[i].day;
        dst[o + 3] = (uint8_t)(mem_[i].day >> 8);
        dst[o + 4] = mem_[i].cause;
        dst[o + 5] = mem_[i].bits;
        dst[o + 6] = 0;
        dst[o + 7] = 0;
    }
    return o;
}

int family_loss_load(const uint8_t *src, uint32_t len)
{
    FamilyMemorial tmp[FAMILY_MEMORIAL_MAX];
    int n, i;
    if (len < 2) return 0;
    n = src[0];
    if (n < 1 || n > FAMILY_MEMORIAL_MAX || len != 2u + 8u * (uint32_t)n) return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *p = src + 2 + 8 * i;
        memset(&tmp[i], 0, sizeof(tmp[i]));
        tmp[i].id = (uint16_t)(p[0] | (p[1] << 8));
        tmp[i].day = (uint16_t)(p[2] | (p[3] << 8));
        tmp[i].cause = p[4];
        tmp[i].bits = p[5];
        if (!family_id_ok(tmp[i].id) || tmp[i].cause < FAMLOSS_AGE || tmp[i].cause >= FAMLOSS_COUNT) return 0;
        if ((tmp[i].bits & 7) >= LIFE_COUNT || ((tmp[i].bits >> 3) & 7) >= FAMROLE_COUNT) return 0;
    }
    memset(mem_, 0, sizeof(mem_));
    memcpy(mem_, tmp, (size_t)n * sizeof(tmp[0]));
    mem_n_ = (uint8_t)n;
    return 1;
}

void family_loss_clear(void)
{
    memset(mem_, 0, sizeof(mem_));
    mem_n_ = 0;
}

typedef char family_loss_fits_[(LIFE_SEC_HDR + FAMILY_LOSS_SZ_WORST <= LIFE_SZ_F6_LOSS) ? 1 : -1];
typedef char family_mem_8b_[(sizeof(FamilyMemorial) == 8) ? 1 : -1];
