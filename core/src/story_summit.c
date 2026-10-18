/* story_summit.c - Ep 4.6 "Say It": who walks with you along the Wall and
 * what each of them finally says (owner: S3; story_final.md sec.13, SPEC
 * sec.7 S3). Eight friends, one unsaid thing each, then your own line (the
 * third player line, in the script). A friend with an authored line of
 * their own (C46_*, ch4/ep46.dsc) says it the first time; everyone else,
 * and every later run (NG+), draws from the pool (P46_00 .. P46_47) by a
 * hash of who they are, skipping what was already said. STRY.sayit_said
 * remembers it: pool bits 0..47, cast bits 48..77.
 *
 * Drawn by SCENE_CUTX (cutstage.c, CUT_SAYIT), so on the PS1 this file is
 * in the cutscene group; the ep46 pack is live (the summit's script pushed
 * the cutscene from it).
 */
#include "story_int.h"
#include "story_ch34.h"

typedef struct { int16_t id; uint16_t txt; } SayCast;
static const SayCast cast[] = {
    { ID_NANA, TXT_C46_NANA }, { ID_PIP, TXT_C46_PIP }, { ID_FUZZ, TXT_C46_FUZZ },
    { ID_RIVET, TXT_C46_RIVET }, { ID_BLOOM, TXT_C46_BLOOM }, { ID_STEAMY, TXT_C46_STEAMY },
    { ID_HATTIE, TXT_C46_HATTIE }, { ID_GRAMPY, TXT_C46_GRAMPY }, { ID_LULLA, TXT_C46_LULLA },
    { ID_VEX, TXT_C46_VEX }, { ID_MOTHER_NOX, TXT_C46_NOX }, { ID_TALLYWORN, TXT_C46_TALLY },
    { ID_SOOTKIN, TXT_C46_SOOTKIN }, { ID_LADY_SMOG, TXT_C46_SMOG }, { ID_ANVIL, TXT_C46_ANVIL },
    { ID_WISP, TXT_C46_WISP }, { ID_MASCOTTY, TXT_C46_MASCOTTY }, { ID_HAZE, TXT_C46_HAZE },
    { ID_WHEEZLE, TXT_C46_WHEEZLE }, { ID_THUMPKIN, TXT_C46_THUMPKIN }, { ID_GRUMBLE, TXT_C46_GRUMBLE },
    { ID_RATTLECUP, TXT_C46_RATTLE }, { ID_SQUEAKER, TXT_C46_SQUEAKER }, { ID_SPRINKLE, TXT_C46_SPRINKLE },
    { ID_SPROCKET, TXT_C46_SPROCKET }, { ID_CLINKER, TXT_C46_CLINKER }, { ID_SIGNHERE, TXT_C46_SIGNHERE },
    { ID_HAZEY, TXT_C46_HAZEY }, { ID_SHUTTERBUG, TXT_C46_SHUTTER }, { ID_CHARLOTTE, TXT_C46_CHARLOTTE },
};
#define NCAST DOOMO_COUNTOF(cast)
DOOMO_STATIC_ASSERT(SAYIT_POOL + 30 <= 96, sayit_bits_fit_stry);

static int16_t friends[SAYIT_FRIENDS];
static uint16_t lines[SAYIT_FRIENDS];

static int said(int bit) { return (story_stry()->sayit_said[bit >> 3] >> (bit & 7)) & 1; }
static void mark(int bit) { story_stry()->sayit_said[bit >> 3] |= (uint8_t)(1u << (bit & 7)); }

static int here(int id)
{
    const DoomoResident *r = town_res(id);
    return r && (r->status == RES_ALIVE || r->status == RES_GHOST);
}

/* the pool line for `id`: its hash, then the next one nobody said yet */
static uint16_t pool_line(int id)
{
    int k, start = (int)(doomo_hash32((uint32_t)id * 2654435761u ^ story_stry()->ngplus) % SAYIT_POOL);
    for (k = 0; k < SAYIT_POOL; k++)
        if (!said((start + k) % SAYIT_POOL)) break;
    if (k == SAYIT_POOL) {                          /* the pool ran dry: start over */
        for (k = 0; k < SAYIT_POOL; k++) story_stry()->sayit_said[k >> 3] &= (uint8_t)~(1u << (k & 7));
        k = 0;
    }
    k = (start + k) % SAYIT_POOL;
    mark(k);
    return (uint16_t)(TXT_P46_00 + k);
}

/* friends first (the ones you befriended), in an order the town's seed
 * picks, then anyone else of the cast who is still around */
void summit_begin(void)
{
    int n = 0, pass, i, off = (int)(story_rng(0x53415949u) % NCAST);
    for (pass = 0; pass < 2 && n < SAYIT_FRIENDS; pass++)
        for (i = 0; i < NCAST && n < SAYIT_FRIENDS; i++) {
            int c = (off + i) % NCAST, id = cast[c].id, k, dup = 0;
            const DoomoResident *r = town_res(id);
            if (!here(id) || id == ID_MAYOR) continue;
            if (pass == 0 && (!r || r->level < STORY_FRIEND_LEVEL)) continue;
            for (k = 0; k < n; k++) dup |= friends[k] == id;
            if (dup) continue;
            friends[n] = (int16_t)id;
            if (!said(SAYIT_POOL + c)) { mark(SAYIT_POOL + c); lines[n] = cast[c].txt; }
            else lines[n] = pool_line(id);
            n++;
        }
    for (; n < SAYIT_FRIENDS; n++) {                /* an empty town: strangers say it too */
        friends[n] = (int16_t)(100 + n * 61);
        lines[n] = pool_line(friends[n]);
    }
}

int summit_friend(int k) { return (k >= 0 && k < SAYIT_FRIENDS) ? friends[k] : -1; }

int summit_line(int k, char *line, int cap)
{
    if (k < 0 || k >= SAYIT_FRIENDS || !lines[k]) { if (cap > 0) line[0] = 0; return -1; }
    return txt_get(lines[k], line, cap);
}
