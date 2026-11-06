/* profiles.c - MP0: player profiles in a shared world save (include/doomo/profiles.h).
 * Zero-initialised statics: no guests. Strong definitions of the weak profile
 * queries in weak_playas.c, and the 'PROF' chunk. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/profiles.h"
#include "doomo/playas.h"
#include "doomo/family.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/life_contract.h"
#include "doomo/ui.h"
#include "doomo/town.h"
#include "doomo/resident.h"
#include "doomo/resident_acc.h"

typedef struct {
    char     name[PROF_NAME_LEN];
    uint16_t res;                          /* the villager played                  */
    int8_t   hero;                         /* -1 villager, else a co-op HERO_*     */
    uint8_t  room, nmail, nrel;
    uint16_t mail[PROF_MAIL_MAX];
    uint8_t  flags[PROF_FLAG_BITS / 8];
    uint8_t  quest[PROF_QUEST_BYTES];
    uint16_t rel_id[PROF_REL_MAX];
    uint8_t  rel_v[PROF_REL_MAX];
} Guest;

static Guest G[PROF_GUESTS];
static uint8_t n_;

static Guest *guest(int p) { return p >= 1 && p <= n_ ? &G[p - 1] : NULL; }

static int name_ok(const char *s)
{
    int i;
    if (!s || !s[0]) return 0;
    for (i = 0; s[i]; i++)
        if (i >= PROF_NAME_LEN - 1 || (unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7E) return 0;
    return 1;
}

static int villager_ok(int id)
{
    const DoomoResident *r;
    if (id < 0 || id >= DOOMO_PLAYER_ID) return 0;
    r = town_res(id);
    return r && r->status == RES_ALIVE;
}

/* is the guest's character still in the world (a villager alive, or a co-op hero's housemate) */
static int guest_ok(const Guest *g)
{
    if (g->hero >= 0) return family_find(g->res) != NULL;
    return villager_ok(g->res);
}

int prof_guest_count(void) { return n_; }
int prof_count(void) { return 1 + n_; }

int prof_find_resident(int resident)
{
    int i;
    for (i = 0; i < n_; i++) if (G[i].res == resident) return i + 1;
    return -1;
}

int prof_hero_used(int kind)
{
    int i;
    for (i = 0; i < n_; i++) if (G[i].hero == kind && kind >= 0) return 1;
    return 0;
}

int prof_hero(int p) { Guest *g = guest(p); return g ? g->hero : -1; }

int prof_is_coop(int p) { Guest *g = guest(p); return g && g->hero >= 0; }

int prof_guest_add(const char *name, int resident, int hero_kind)
{
    Guest *g;
    if (n_ >= PROF_GUESTS) return PROF_ERR_FULL;
    if (!name_ok(name)) return PROF_ERR_NAME;
    if (hero_kind != -1) {
        if (!(HERO_IS_COOP(hero_kind) && hero_preset(hero_kind) != NULL) || prof_hero_used(hero_kind)) return PROF_ERR_HERO;
    } else if (!villager_ok(resident) || prof_find_resident(resident) >= 0) return PROF_ERR_RESIDENT;
    if (!playas_available()) return PROF_ERR_LOCKED;
    if (hero_kind != -1) {                     /* MP1: the co-op hero moves in as a housemate made from their look */
        resident = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, hero_coop_seed(hero_kind), name,
                                   RES_PRONOUN(hero_preset(hero_kind)));
        if (resident < 0) return PROF_ERR_HOUSEHOLD;
    }
    g = &G[n_++];
    memset(g, 0, sizeof *g);
    strncpy(g->name, name, PROF_NAME_LEN - 1);
    g->res = (uint16_t)resident;
    g->hero = (int8_t)hero_kind;
    return n_;
}

int prof_rename(int p, const char *name)
{
    Guest *g = guest(p);
    if (!g) return 0;
    if (!name_ok(name)) return PROF_ERR_NAME;
    memset(g->name, 0, sizeof g->name);
    strncpy(g->name, name, PROF_NAME_LEN - 1);
    if (g->hero >= 0) family_set_name(g->res, name);   /* the housemate they play is called the same */
    return 1;
}

int prof_active(void) { return playas_profile(); }

int prof_switch(int p)
{
    Guest *g;
    if (p == 0) return playas_is_hero() ? 1 : playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_FADE);
    g = guest(p);
    if (!g) return 0;
    return playas_avatar() == g->res ? 1 : playas_switch(g->res, PLAYAS_SWITCH_FADE);
}

int prof_guest_remove(int p)
{
    Guest *g = guest(p);
    int i, coop;
    if (!g) return 0;
    if (playas_avatar() == g->res) playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_FADE);
    coop = g->hero >= 0 ? (int)g->res : -1;
    for (i = p - 1; i + 1 < n_; i++) G[i] = G[i + 1];
    memset(&G[--n_], 0, sizeof G[0]);
    if (coop >= 0) family_remove(coop);              /* the co-op hero's housemate moves out with them */
    return 1;
}

const char *prof_name(int p)
{
    Guest *g = guest(p);
    return g ? g->name : p == 0 ? doomo_town.player.name : "";
}

int prof_resident(int p)
{
    Guest *g = guest(p);
    return g ? (int)g->res : p == 0 ? PLAYAS_ID_HERO : -1;
}

int prof_check_daily(void)
{
    int i, gone = 0;
    for (i = n_; i >= 1; i--) {
        Guest *g = guest(i);
        if (guest_ok(g)) continue;
        {
            char msg[40];
            doomo_snprintf(msg, sizeof msg, "%s moved out.", g->name);
            ui_toast(msg);
        }
        prof_guest_remove(i);
        gone++;
    }
    return gone;
}

/* ---- the queries playas.h reserves for MP0 ---- */
int playas_profile(void)
{
    return prof_find_resident(playas_avatar()) > 0 ? prof_find_resident(playas_avatar()) : 0;
}

int playas_profile_avatar(int p)
{
    Guest *g = guest(p);
    if (g) return g->res;
    if (p != 0) return PLAYAS_ID_NONE;
    return prof_find_resident(playas_avatar()) > 0 ? PLAYAS_ID_HERO : playas_avatar();
}

int playas_profile_hero(int p)
{
    Guest *g = guest(p);
    if (g) return g->hero;
    return p == 0 ? playas_hero_kind() : -1;
}

/* ---- a guest's own state ---- */
int prof_flag(int p, int bit)
{
    Guest *g = guest(p);
    return g && bit >= 0 && bit < PROF_FLAG_BITS ? (g->flags[bit >> 3] >> (bit & 7)) & 1 : 0;
}

void prof_flag_set(int p, int bit, int on)
{
    Guest *g = guest(p);
    if (!g || bit < 0 || bit >= PROF_FLAG_BITS) return;
    if (on) g->flags[bit >> 3] |= (uint8_t)(1u << (bit & 7));
    else g->flags[bit >> 3] &= (uint8_t)~(1u << (bit & 7));
}

int prof_quest(int p, int i)
{
    Guest *g = guest(p);
    return g && i >= 0 && i < PROF_QUEST_BYTES ? g->quest[i] : 0;
}

void prof_quest_set(int p, int i, int v)
{
    Guest *g = guest(p);
    if (g && i >= 0 && i < PROF_QUEST_BYTES) g->quest[i] = (uint8_t)v;
}

static int rel_slot(const Guest *g, int res)
{
    int i;
    for (i = 0; i < g->nrel; i++) if (g->rel_id[i] == res) return i;
    return -1;
}

int prof_rel(int p, int res)
{
    Guest *g = guest(p);
    int i = g ? rel_slot(g, res) : -1;
    return i >= 0 ? g->rel_v[i] : 0;
}

int prof_rel_set(int p, int res, int v)
{
    Guest *g = guest(p);
    int i;
    if (!g || res < 0 || res >= DOOMO_PLAYER_ID) return 0;
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    i = rel_slot(g, res);
    if (i >= 0) {
        if (v == 0) {                                   /* drop the tie, keep the order */
            for (; i + 1 < g->nrel; i++) { g->rel_id[i] = g->rel_id[i + 1]; g->rel_v[i] = g->rel_v[i + 1]; }
            g->nrel--;
        } else g->rel_v[i] = (uint8_t)v;
        return 1;
    }
    if (v == 0) return 1;
    if (g->nrel >= PROF_REL_MAX) return 0;
    g->rel_id[g->nrel] = (uint16_t)res;
    g->rel_v[g->nrel++] = (uint8_t)v;
    return 1;
}

int prof_room(int p) { Guest *g = guest(p); return g ? g->room : 0; }
void prof_room_set(int p, int room) { Guest *g = guest(p); if (g) g->room = (uint8_t)room; }

int prof_mail_send(int p, int letter)
{
    Guest *g = guest(p);
    if (!g || g->nmail >= PROF_MAIL_MAX) return 0;
    g->mail[g->nmail++] = (uint16_t)letter;
    return 1;
}

int prof_mail_count(int p) { Guest *g = guest(p); return g ? g->nmail : 0; }

int prof_mail_take(int p)
{
    Guest *g = guest(p);
    int i, m;
    if (!g || !g->nmail) return -1;
    m = g->mail[0];
    for (i = 0; i + 1 < g->nmail; i++) g->mail[i] = g->mail[i + 1];
    g->nmail--;
    return m;
}

/* ---- the 'PROF' chunk: layout 1 = 32 B header + 96 B per guest ---------------
 * header: [0] layout (1), [1] guest count, rest 0.
 * guest:  name 12 | res u16 | hero (0xFF = villager) | room | nmail | nrel |
 *         mail 4 x u16 | flags 16 | quest 16 | rel ids 12 x u16 | rel values 12 | 2 spare */
static uint32_t prof_size(void) { return n_ ? PROF_SZ_HEADER + (uint32_t)n_ * PROF_SZ_REC : 0u; }

static uint32_t prof_write(uint8_t *d, uint32_t cap)
{
    uint32_t need = prof_size();
    int i, k;
    if (!need || cap < need) return 0;
    memset(d, 0, need);
    d[0] = 1; d[1] = n_;
    for (i = 0; i < n_; i++) {
        uint8_t *r = d + PROF_SZ_HEADER + i * PROF_SZ_REC;
        const Guest *g = &G[i];
        memcpy(r, g->name, PROF_NAME_LEN);
        r[12] = (uint8_t)g->res; r[13] = (uint8_t)(g->res >> 8);
        r[14] = g->hero < 0 ? 0xFF : (uint8_t)g->hero;
        r[15] = g->room; r[16] = g->nmail; r[17] = g->nrel;
        for (k = 0; k < PROF_MAIL_MAX; k++) { r[18 + 2 * k] = (uint8_t)g->mail[k]; r[19 + 2 * k] = (uint8_t)(g->mail[k] >> 8); }
        memcpy(r + 26, g->flags, 16);
        memcpy(r + 42, g->quest, 16);
        for (k = 0; k < PROF_REL_MAX; k++) { r[58 + 2 * k] = (uint8_t)g->rel_id[k]; r[59 + 2 * k] = (uint8_t)(g->rel_id[k] >> 8); }
        memcpy(r + 82, g->rel_v, PROF_REL_MAX);
    }
    return need;
}

static void prof_reset(void) { memset(G, 0, sizeof G); n_ = 0; }

static int prof_read(const uint8_t *s, uint32_t len, uint16_t ver)
{
    int i, k, n;
    prof_reset();
    if (ver != 1 || len < PROF_SZ_HEADER || s[0] != 1) return 0;
    n = s[1] < PROF_GUESTS ? s[1] : PROF_GUESTS;
    if (len < PROF_SZ_HEADER + (uint32_t)n * PROF_SZ_REC) return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *r = s + PROF_SZ_HEADER + i * PROF_SZ_REC;
        Guest g;
        int res = r[12] | (r[13] << 8);
        memset(&g, 0, sizeof g);
        memcpy(g.name, r, PROF_NAME_LEN - 1);              /* sanitised below, always NUL ended */
        for (k = 0; g.name[k]; k++) if ((unsigned char)g.name[k] < 0x20 || (unsigned char)g.name[k] > 0x7E) g.name[k] = '?';
        g.hero = (int8_t)-1;
        if (HERO_IS_COOP(r[14]) && hero_preset(r[14]) != NULL) g.hero = (int8_t)r[14];
        /* a co-op hero's housemate is a family pool id (the household may load after this chunk, so the daily check
         * culls a missing one); a villager must be alive now */
        if (!g.name[0] || prof_find_resident(res) >= 0 || prof_hero_used(g.hero)) continue;
        if (g.hero >= 0 ? !(res >= PLAYAS_ID_FAMILY_FIRST && res <= PLAYAS_ID_FAMILY_LAST) : !villager_ok(res)) continue;
        g.res = (uint16_t)res;
        g.room = r[15];
        g.nmail = r[16] < PROF_MAIL_MAX ? r[16] : PROF_MAIL_MAX;
        for (k = 0; k < PROF_MAIL_MAX; k++) g.mail[k] = (uint16_t)(r[18 + 2 * k] | (r[19 + 2 * k] << 8));
        memcpy(g.flags, r + 26, 16);
        memcpy(g.quest, r + 42, 16);
        for (k = 0; k < PROF_REL_MAX && k < r[17]; k++) {
            int rid = r[58 + 2 * k] | (r[59 + 2 * k] << 8);
            if (rid < DOOMO_PLAYER_ID && r[82 + k]) { g.rel_id[g.nrel] = (uint16_t)rid; g.rel_v[g.nrel++] = r[82 + k]; }
        }
        G[n_++] = g;
    }
    return 1;
}

static const DoomoSaveExt prof_ext = {
    SAVE_EXT_TAG_PROF, 1, prof_size, prof_write, prof_read, prof_reset, SAVE_EXT_MAX_PROF
};
const DoomoSaveExt *const doomo_save_ext_profiles = &prof_ext;
