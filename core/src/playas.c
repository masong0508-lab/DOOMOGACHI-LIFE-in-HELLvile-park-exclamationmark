/* playas.c - PA1: who the player controls (owner: playas; docs/PLAYAS.md).
 * State is zero-initialised statics: the avatar is stored as an offset from
 * the hero, so all-zero means "you are the hero". The 'PLAY' chunk is the
 * 32 B header only (no pockets yet); it is empty while everything is default.
 * Strong definitions of the weak defaults in weak_playas.c. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/ending.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/life_contract.h"
#include "doomo/platform.h"
#include "doomo/ui.h"
#include "doomo/town.h"

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

static int in_family(int id) { return id >= PLAYAS_ID_FAMILY_FIRST && id <= PLAYAS_ID_FAMILY_LAST; }

int playas_avatar(void) { return PLAYAS_ID_HERO + P.off; }
int playas_is_hero(void) { return P.off == 0; }
int playas_story_locked(void) { return P.off != 0; }

int playas_available(void)
{
    if (P.flags & PF_NEWLIFE) return 1;
    if (doomo_town.mode == TOWN_STORY) return doomo_town.story.chapter == STORY_CH_DONE;
    return ending_count_seen() > 0;
}

/* the id is a valid avatar (ignoring availability and "already them") */
static int valid(int id)
{
    const DoomoResident *r;
    if (id == PLAYAS_ID_HERO) return 1;
    if (in_family(id)) {
        const FamilyMember *m = family_find(id);
        return m && m->role != FAMROLE_PET;      /* pets wait for F5's uplift */
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
    else if ((r = town_res(id)) != NULL && r->status != RES_EMPTY) pr = r->pronoun;
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
    if (playas_available() && !(P.flags & PF_NEWLIFE)) f |= PLAYAS_HF_STORY_CLEARED;
    return f;
}

int playas_switch(int id, int how)
{
    int from = playas_avatar();
    (void)how;                                   /* PA4 presents it */
    if (!playas_can_be(id)) return 0;
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
    P.off = 0;
    recent_drop(PLAYAS_ID_HERO);                  /* the avatar is never in the ring */
    ui_toast("You are back as yourself.");
    plat_log("PLAYAS: avatar lost, back to the hero");
    return 1;
}

/* ---- the 'PLAY' chunk: 32 B header (docs/PLAYAS.md section 4) ------------- */
static int is_default(void) { return P.off == 0 && P.kind == 0 && P.nrecent == 0 && P.switches == 0 && !P.flags; }
static uint32_t play_size(void) { return is_default() ? 0u : PLAY_SZ_HEADER; }

static uint32_t play_write(uint8_t *d, uint32_t cap)
{
    int i, a = playas_avatar();
    if (is_default() || cap < PLAY_SZ_HEADER) return 0;
    memset(d, 0, PLAY_SZ_HEADER);
    d[0] = 1; d[1] = P.flags & PF_NEWLIFE ? 1 : 0;
    d[2] = (uint8_t)a; d[3] = (uint8_t)(a >> 8);
    d[4] = P.kind; d[5] = P.nrecent;
    d[6] = (uint8_t)P.switches; d[7] = (uint8_t)(P.switches >> 8);
    for (i = 0; i < RECENT_N; i++) { d[8 + 2 * i] = (uint8_t)P.recent[i]; d[9 + 2 * i] = (uint8_t)(P.recent[i] >> 8); }
    d[16] = (uint8_t)P.pos.x; d[17] = (uint8_t)((uint16_t)P.pos.x >> 8);
    d[18] = (uint8_t)P.pos.y; d[19] = (uint8_t)((uint16_t)P.pos.y >> 8);
    d[20] = P.pos.map; d[21] = P.pos.dir;
    d[24] = P.flags & PF_HEROPOS ? 1 : 0;
    return PLAY_SZ_HEADER;
}

static void play_reset(void) { memset(&P, 0, sizeof P); }

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
    /* the avatar may be gone by now: clamp to the hero */
    P.off = (int16_t)(a - PLAYAS_ID_HERO);
    if (a != PLAYAS_ID_HERO && !valid(a)) P.off = 0;
    for (i = P.nrecent - 1; i >= 0; i--)
        if (P.recent[i] == playas_avatar() || (P.recent[i] != PLAYAS_ID_HERO && !valid(P.recent[i]))) recent_drop(P.recent[i]);
    return 1;
}

static const DoomoSaveExt play_ext = {
    SAVE_EXT_TAG_PLAY, 1, play_size, play_write, play_read, play_reset, SAVE_EXT_MAX_PLAY
};
const DoomoSaveExt *const doomo_save_ext_playas = &play_ext;
