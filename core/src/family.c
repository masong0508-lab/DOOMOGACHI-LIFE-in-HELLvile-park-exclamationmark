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

static FamilyMember fam_[FAMILY_MAX];
static uint8_t fam_n_;

/* ---- ids and the cap ------------------------------------------------------ */
int family_cap(void)
{
    int tier = unlock_has(UL_LIFE_HOUSEHOLD_T2) ? HOUSEHOLD_TIER_NGPLUS_DONE
             : unlock_has(UL_LIFE_HOUSEHOLD_T1) ? HOUSEHOLD_TIER_GAME_DONE
             : HOUSEHOLD_TIER_START;
#ifdef DOOMO_PLATFORM_GBA
    return HOUSEHOLD_CAP(1, tier);
#else
    return HOUSEHOLD_CAP(0, tier);
#endif
}

/* alive or a ghost (play-as.h: both can be played and may live in the house) */
static int res_lives_(const DoomoResident *r)
{
    return r && (r->status == RES_ALIVE || r->status == RES_GHOST);
}

static int is_pool(int id) { return id >= PLAYAS_ID_FAMILY_FIRST && id <= PLAYAS_ID_FAMILY_LAST; }

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
    for (id = PLAYAS_ID_FAMILY_FIRST; id <= PLAYAS_ID_FAMILY_LAST; id++)
        if (find_slot(id) < 0) return id;
    return -1;
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
    return fam_n_++;
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
    memmove(&fam_[i], &fam_[i + 1], (size_t)(fam_n_ - i - 1) * sizeof(fam_[0]));
    fam_n_--;
    memset(&fam_[fam_n_], 0, sizeof(fam_[0]));
    return 1;
}

void family_clear(void)
{
    memset(fam_, 0, sizeof(fam_));
    fam_n_ = 0;
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
    dst[1] = 0;
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
    fam_n_ = (uint8_t)n;
    return 1;
}

typedef char family_worst_fits_[(LIFE_SEC_HDR + FAMILY_SZ_WORST <= LIFE_SZ_LS1_PLAN - LIFE_SEC_HDR - LIFE_SZ_NEWS) ? 1 : -1];
typedef char family_rec_is_8_[(sizeof(FamilyMember) == 8) ? 1 : -1];
