/* story_side.c - side quests (owner: story).
 *
 * 46 errands from named citizens of both districts, spread over the four
 * chapters and the free roam after the credits: fetch, deliver,
 * matchmaking, lost pets, photos, mini-boss hunts, exorcism requests,
 * cross-border reconciliation and visits. Each quest is one table row;
 * its state is 2 bits in the story flags (64 + 2q): 0 not taken, 1 active,
 * 2 ready to hand in, 3 done.
 *
 * (S4) The 49 quests are re-timed: their offers and thank-yous are VM
 * scripts with beat codes (core/data/story/quests49/, packs q49a / q49b),
 * their titles, hints and the SS lines live in the `quests` index pack
 * with every S100..S217 title (one side pack for the quest log and the
 * HUD, so the PS1's single side slot never thrashes).
 */
#include "story_int.h"
#include "overworld_int.h"   /* S4: ownpc_find / ownpc_cast (a verb's resident) */
#include "doomo/unlock_ids.h" /* S4: UL_STORY_SONG2 (CB_SIDE_ONCE) */

typedef struct SideQuest {
    uint16_t title;            /* text ids (the `quests` pack)               */
    int16_t  giver;            /* roster id                                  */
    uint8_t  ch_min, ch_max;   /* offered in these chapters (5 = free roam)  */
    uint8_t  gmap, gx, gy;     /* where the giver waits                      */
    uint8_t  kind;             /* SQ_*                                       */
    int16_t  target;           /* FETCH: item; DELIVER / MATCH / RECONCILE /
                                  EXORCISE: resident; VISIT: place; HUNT: map */
    int16_t  other;            /* DELIVER: item; MATCH: who they confess to;
                                  HUNT: demon set (invasion nests)           */
    uint8_t  count;            /* FETCH: how many                            */
    uint8_t  tmap, tx, ty;     /* target spot (find / photo / pet / NPC)     */
    uint16_t money;
    uint8_t  item, item_n;
    uint16_t unlock;
    uint16_t offer, hint, thanks;  /* (S4) offer / thanks: script ids; hint: text */
} SideQuest;

#define HH MAPID_HAHALAND
#define HV MAPID_HELLVILE
#define PK MAPID_PARK_UNITED
#define NONE USTORY_NONE

static const SideQuest quests[] = {
    /* ---------------- chapter 1: pastel errands ---------------------------- */
    { TXT_Q0_TITLE, 97, 1, 2, HH, 20, 20, SQ_DELIVER, 52, ITEM_LOVE_LETTER, 1, HH, 42, 26,
      60, ITEM_FLOWERS, 1, USTORY_REWARD_FIRST + 1,
      SCR_Q0_OFFER,
      TXT_Q0_HINT,
      SCR_Q0_THANKS },
    { TXT_Q1_TITLE, 29, 1, 2, HH, 30, 16, SQ_FETCH, ITEM_COFFEE, -1, 1, HH, 0, 0,
      45, ITEM_NONE, 0, USTORY_REWARD_FIRST + 2,
      SCR_Q1_OFFER,
      TXT_Q1_HINT,
      SCR_Q1_THANKS },
    { TXT_Q2_TITLE, 96, 1, 2, HH, 17, 9, SQ_PET, 0, -1, 1, HH, 18, 15,
      80, ITEM_NONE, 0, NONE,
      SCR_Q2_OFFER,
      TXT_Q2_HINT,
      SCR_Q2_THANKS },
    { TXT_Q3_TITLE, 46, 1, 5, HH, 37, 24, SQ_PHOTO, 0, -1, 1, HH, 34, 24,
      50, ITEM_NONE, 0, NONE,
      SCR_Q3_OFFER,
      TXT_Q3_HINT,
      SCR_Q3_THANKS },
    { TXT_Q4_TITLE, 117, 1, 2, HH, 44, 28, SQ_MATCH, 11, 25, 1, HH, 45, 36,
      100, ITEM_PERFUME, 1, NONE,
      SCR_Q4_OFFER,
      TXT_Q4_HINT,
      SCR_Q4_THANKS },
    { TXT_Q5_TITLE, 5, 1, 2, HH, 8, 18, SQ_FETCH, ITEM_MILK, -1, 2, HH, 0, 0,
      70, ITEM_NONE, 0, NONE,
      SCR_Q5_OFFER,
      TXT_Q5_HINT,
      SCR_Q5_THANKS },
    { TXT_Q6_TITLE, 16, 1, 5, HH, 40, 36, SQ_VISIT, PLACE_LIBRARY, -1, 1, HH, 41, 35,
      40, ITEM_HYMN_BOOK, 1, USTORY_REWARD_FIRST + 4,
      SCR_Q6_OFFER,
      TXT_Q6_HINT,
      SCR_Q6_THANKS },
    { TXT_Q7_TITLE, 83, 1, 2, HH, 8, 43, SQ_VISIT, PLACE_FUN_FAIR, -1, 1, HH, 4, 42,
      60, ITEM_NONE, 0, NONE,
      SCR_Q7_OFFER,
      TXT_Q7_HINT,
      SCR_Q7_THANKS },
    { TXT_Q8_TITLE, 27, 1, 2, HH, 6, 10, SQ_FIND, 0, -1, 1, HH, 44, 55,
      70, ITEM_PUDDING, 1, USTORY_REWARD_FIRST + 3,
      SCR_Q8_OFFER,
      TXT_Q8_HINT,
      SCR_Q8_THANKS },
    { TXT_Q9_TITLE, 112, 1, 5, HH, 36, 53, SQ_PHOTO, 0, -1, 1, HH, 38, 61,
      50, ITEM_NONE, 0, USTORY_REWARD_FIRST + 14,
      SCR_Q9_OFFER,
      TXT_Q9_HINT,
      SCR_Q9_THANKS },
    { TXT_Q10_TITLE, 39, 1, 2, HH, 44, 10, SQ_FETCH, ITEM_SOAP, -1, 3, HH, 0, 0,
      90, ITEM_NONE, 0, NONE,
      SCR_Q10_OFFER,
      TXT_Q10_HINT,
      SCR_Q10_THANKS },
    { TXT_Q11_TITLE, 142, 1, 5, HH, 23, 36, SQ_VISIT, PLACE_CONCERT_HALL, -1, 1, HH, 25, 35,
      60, ITEM_NONE, 0, USTORY_SONG_FIRST + SONG_CHURCH - 1,
      SCR_Q11_OFFER,
      TXT_Q11_HINT,
      SCR_Q11_THANKS },

    /* ---------------- chapter 2: something in the vapor -------------------- */
    { TXT_Q12_TITLE, 57, 2, 2, HH, 50, 24, SQ_PHOTO, 0, -1, 1, HH, 51, 38,
      80, ITEM_NONE, 0, USTORY_REWARD_FIRST + 8,
      SCR_Q12_OFFER,
      TXT_Q12_HINT,
      SCR_Q12_THANKS },
    { TXT_Q13_TITLE, 41, 2, 4, HH, 21, 26, SQ_DELIVER, ID_STEAMY, ITEM_SOAP, 1, HH, 38, 26,
      70, ITEM_SOAP, 2, NONE,
      SCR_Q13_OFFER,
      TXT_Q13_HINT,
      SCR_Q13_THANKS },
    { TXT_Q14_TITLE, 354, 2, 3, HH, 11, 27, SQ_FETCH, ITEM_HOLY_WATER, -1, 2, HH, 0, 0,
      100, ITEM_ROSARY, 1, NONE,
      SCR_Q14_OFFER,
      TXT_Q14_HINT,
      SCR_Q14_THANKS },
    { TXT_Q15_TITLE, 69, 2, 4, HH, 50, 12, SQ_FETCH, ITEM_SCRAP, -1, 3, HH, 0, 0,
      80, ITEM_GEARS, 2, NONE,
      SCR_Q15_OFFER,
      TXT_Q15_HINT,
      SCR_Q15_THANKS },
    { TXT_Q16_TITLE, 23, 2, 2, HH, 19, 30, SQ_EXORCISE, 14, -1, 1, HH, 21, 31,
      150, ITEM_CHILL_PILL, 1, USTORY_REWARD_FIRST + 7,
      SCR_Q16_OFFER,
      TXT_Q16_HINT,
      SCR_Q16_THANKS },
    { TXT_Q17_TITLE, 36, 2, 3, HH, 30, 42, SQ_FIND, 0, -1, 1, HH, 5, 49,
      60, ITEM_NONE, 0, NONE,
      SCR_Q17_OFFER,
      TXT_Q17_HINT,
      SCR_Q17_THANKS },
    { TXT_Q18_TITLE, 13, 2, 3, HH, 53, 10, SQ_FETCH, ITEM_CANDY_EYEBALLS, -1, 2, HH, 0, 0,
      60, ITEM_WELD_ROD, 2, NONE,
      SCR_Q18_OFFER,
      TXT_Q18_HINT,
      SCR_Q18_THANKS },
    { TXT_Q19_TITLE, 52, 2, 5, HH, 40, 26, SQ_MATCH, 9, 86, 1, HH, 12, 21,
      120, ITEM_RING, 1, USTORY_REWARD_FIRST + 0,
      SCR_Q19_OFFER,
      TXT_Q19_HINT,
      SCR_Q19_THANKS },
    { TXT_Q20_TITLE, 42, 2, 2, HH, 11, 47, SQ_PHOTO, 0, -1, 1, HH, 55, 27,
      50, ITEM_PLUSHIE, 1, USTORY_REWARD_FIRST + 15,
      SCR_Q20_OFFER,
      TXT_Q20_HINT,
      SCR_Q20_THANKS },

    /* ---------------- chapter 3: hell invades ------------------------------ */
    { TXT_Q21_TITLE, 99, 3, 3, HH, 45, 22, SQ_HUNT, MAPID_HAHALAND_INVADED, 4, 1, HH, 45, 24,
      150, ITEM_HELLSTEEL, 1, USTORY_REWARD_FIRST + 5,
      SCR_Q21_OFFER,
      TXT_Q21_HINT,
      SCR_Q21_THANKS },
    { TXT_Q22_TITLE, 1, 3, 3, HH, 9, 28, SQ_FETCH, ITEM_SCRAP, -1, 3, HH, 0, 0,
      100, ITEM_CUPCAKE, 3, NONE,
      SCR_Q22_OFFER,
      TXT_Q22_HINT,
      SCR_Q22_THANKS },
    { TXT_Q23_TITLE, 111, 3, 3, HH, 10, 29, SQ_FETCH, ITEM_MEDICINE, -1, 2, HH, 0, 0,
      120, ITEM_ENERGY_DRINK, 2, NONE,
      SCR_Q23_OFFER,
      TXT_Q23_HINT,
      SCR_Q23_THANKS },
    { TXT_Q24_TITLE, 426, 3, 4, HH, 3, 27, SQ_EXORCISE, 91, -1, 1, HH, 16, 28,
      150, ITEM_BLESSED_SALT, 2, NONE,
      SCR_Q24_OFFER,
      TXT_Q24_HINT,
      SCR_Q24_THANKS },
    { TXT_Q25_TITLE, 127, 3, 4, HH, 8, 27, SQ_PET, 0, -1, 1, HH, 16, 35,
      100, ITEM_NONE, 0, NONE,
      SCR_Q25_OFFER,
      TXT_Q25_HINT,
      SCR_Q25_THANKS },
    { TXT_Q26_TITLE, 354, 3, 3, HH, 11, 27, SQ_HUNT, MAPID_HAHALAND_INVADED, 5, 1, HH, 14, 48,
      200, ITEM_BLESSED_SALT, 1, USTORY_REWARD_FIRST + 6,
      SCR_Q26_OFFER,
      TXT_Q26_HINT,
      SCR_Q26_THANKS },
    { TXT_Q27_TITLE, 17, 3, 3, HH, 46, 40, SQ_PHOTO, 0, -1, 1, HH, 52, 39,
      80, ITEM_NONE, 0, USTORY_REWARD_FIRST + 11,
      SCR_Q27_OFFER,
      TXT_Q27_HINT,
      SCR_Q27_THANKS },
    { TXT_Q28_TITLE, 86, 3, 3, HH, 24, 23, SQ_FETCH, ITEM_ENERGY_DRINK, -1, 2, HH, 0, 0,
      100, ITEM_HELLSTEEL, 1, NONE,
      SCR_Q28_OFFER,
      TXT_Q28_HINT,
      SCR_Q28_THANKS },
    { TXT_Q29_TITLE, 4, 3, 4, HH, 39, 22, SQ_DELIVER, 26, ITEM_ONIGIRI, 1, HH, 22, 20,
      70, ITEM_ONIGIRI, 1, NONE,
      SCR_Q29_OFFER,
      TXT_Q29_HINT,
      SCR_Q29_THANKS },
    { TXT_Q30_TITLE, 43, 3, 4, HH, 14, 44, SQ_EXORCISE, 20, -1, 1, HH, 17, 47,
      150, ITEM_PLUSHIE, 1, NONE,
      SCR_Q30_OFFER,
      TXT_Q30_HINT,
      SCR_Q30_THANKS },

    /* ---------------- chapter 4: hellvile, reconciliation ------------------ */
    { TXT_Q31_TITLE, 753, 4, 5, HV, 36, 36, SQ_PHOTO, 0, -1, 1, HH, 34, 24,
      120, ITEM_NONE, 0, NONE,
      SCR_Q31_OFFER,
      TXT_Q31_HINT,
      SCR_Q31_THANKS },
    { TXT_Q32_TITLE, 566, 4, 5, HV, 20, 22, SQ_DELIVER, ID_NANA, ITEM_BRIMSTONE_BAGEL, 1, HH, 25, 17,
      150, ITEM_ANGEL_CAKE, 1, NONE,
      SCR_Q32_OFFER,
      TXT_Q32_HINT,
      SCR_Q32_THANKS },
    { TXT_Q33_TITLE, 582, 4, 5, HV, 24, 31, SQ_RECONCILE, 29, -1, 1, HH, 30, 16,
      120, ITEM_FLOWERS, 3, USTORY_REWARD_FIRST + 10,
      SCR_Q33_OFFER,
      TXT_Q33_HINT,
      SCR_Q33_THANKS },
    { TXT_Q34_TITLE, 730, 4, 5, HV, 22, 12, SQ_FETCH, ITEM_BOARD_GAME, -1, 1, HH, 0, 0,
      100, ITEM_NONE, 0, NONE,
      SCR_Q34_OFFER,
      TXT_Q34_HINT,
      SCR_Q34_THANKS },
    { TXT_Q35_TITLE, 701, 4, 5, HV, 55, 34, SQ_VISIT, PLACE_CONCERT_HALL, -1, 1, HH, 25, 35,
      100, ITEM_NONE, 0, NONE,
      SCR_Q35_OFFER,
      TXT_Q35_HINT,
      SCR_Q35_THANKS },
    { TXT_Q36_TITLE, 855, 4, 5, HV, 54, 31, SQ_FETCH, ITEM_PLUSHIE, -1, 1, HH, 0, 0,
      90, ITEM_MEAT_SLAB, 1, NONE,
      SCR_Q36_OFFER,
      TXT_Q36_HINT,
      SCR_Q36_THANKS },
    { TXT_Q37_TITLE, 766, 4, 5, HV, 9, 17, SQ_EXORCISE, 788, -1, 1, HV, 11, 19,
      180, ITEM_SAGE_BUNDLE, 1, NONE,
      SCR_Q37_OFFER,
      TXT_Q37_HINT,
      SCR_Q37_THANKS },
    { TXT_Q38_TITLE, 712, 4, 5, HV, 9, 31, SQ_RECONCILE, 104, -1, 1, HH, 46, 24,
      120, ITEM_NONE, 0, NONE,
      SCR_Q38_OFFER,
      TXT_Q38_HINT,
      SCR_Q38_THANKS },
    { TXT_Q39_TITLE, 519, 4, 5, HV, 52, 37, SQ_HUNT, MAPID_CLOUD_NINE_SPIRE, -1, 1, HV, 52, 37,
      250, ITEM_HELLSTEEL, 2, USTORY_REWARD_FIRST + 12,
      SCR_Q39_OFFER,
      TXT_Q39_HINT,
      SCR_Q39_THANKS },
    { TXT_Q40_TITLE, 669, 4, 5, HV, 17, 40, SQ_RECONCILE, 73, -1, 1, HH, 29, 19,
      120, ITEM_CUPCAKE, 2, NONE,
      SCR_Q40_OFFER,
      TXT_Q40_HINT,
      SCR_Q40_THANKS },
    { TXT_Q41_TITLE, 820, 4, 5, HV, 45, 38, SQ_PET, 0, -1, 1, HV, 44, 41,
      100, ITEM_NONE, 0, USTORY_REWARD_FIRST + 9,
      SCR_Q41_OFFER,
      TXT_Q41_HINT,
      SCR_Q41_THANKS },
    { TXT_Q42_TITLE, 565, 4, 5, HV, 11, 24, SQ_PHOTO, 0, -1, 1, HV, 3, 25,
      100, ITEM_NONE, 0, USTORY_REWARD_FIRST + 13,
      SCR_Q42_OFFER,
      TXT_Q42_HINT,
      SCR_Q42_THANKS },

    /* ---------------- after the credits: the united park -------------------- */
    { TXT_Q43_TITLE, 511, 5, 5, PK, 36, 12, SQ_VISIT, PLACE_STAGE, -1, 1, PK, 31, 8,
      200, ITEM_NONE, 0, USTORY_SONG_FIRST + SONG_FESTIVAL - 1,
      SCR_Q43_OFFER,
      TXT_Q43_HINT,
      SCR_Q43_THANKS },
    { TXT_Q44_TITLE, 62, 5, 5, PK, 20, 12, SQ_MATCH, 912, 82, 1, PK, 45, 40,
      300, ITEM_RING, 1, USTORY_REWARD_FIRST + 16,
      SCR_Q44_OFFER,
      TXT_Q44_HINT,
      SCR_Q44_THANKS },
    { TXT_Q45_TITLE, 46, 5, 5, PK, 22, 15, SQ_PHOTO, 0, -1, 1, PK, 31, 18,
      150, ITEM_NONE, 0, NONE,
      SCR_Q45_OFFER,
      TXT_Q45_HINT,
      SCR_Q45_THANKS },
    { TXT_Q46_TITLE, 778, 5, 5, PK, 40, 22, SQ_FETCH, ITEM_SOUL_SOUP, -1, 2, HH, 0, 0,
      150, ITEM_ANGEL_CAKE, 2, NONE,
      SCR_Q46_OFFER,
      TXT_Q46_HINT,
      SCR_Q46_THANKS },
    { TXT_Q47_TITLE, 999, 5, 5, PK, 45, 31, SQ_DELIVER, ID_MAYOR, ITEM_FLOWERS, 1, PK, 29, 10,
      200, ITEM_NONE, 0, NONE,
      SCR_Q47_OFFER,
      TXT_Q47_HINT,
      SCR_Q47_THANKS },
    { TXT_Q48_TITLE, 15, 4, 5, HH, 14, 25, SQ_VISIT, PLACE_MARKET, -1, 1, HV, 19, 17,
      120, ITEM_NONE, 0, NONE,
      SCR_Q48_OFFER,
      TXT_Q48_HINT,
      SCR_Q48_THANKS },
};
#define NQ ((int)(sizeof(quests) / sizeof(quests[0])))
DOOMO_STATIC_ASSERT(sizeof(quests) / sizeof(quests[0]) <= STORY_SIDE_MAX, side_quests_fit);

static const char *const kind_names[SQ_KIND_COUNT] = {
    "Fetch", "Deliver", "Find", "Lost pet", "Photo", "Matchmaking", "Hunt",
    "Exorcism", "Reconcile", "Visit"
};

/* ---- state ------------------------------------------------------------------ */
static int raw_state(int q)
{
    int f = STORY_FLAG_SIDE_FIRST + q * 2;
    return story_flag(f) | (story_flag(f + 1) << 1);
}

static void set_state(int q, int s)
{
    int f = STORY_FLAG_SIDE_FIRST + q * 2;
    story_set_flag(f, s & 1);
    story_set_flag(f + 1, (s >> 1) & 1);
}

static int alive_ok(int id)
{
    const DoomoResident *r = town_res(id);
    return r && (r->status == RES_ALIVE || r->status == RES_POSSESSED || r->status == RES_GHOST);
}

static int chapter_now(void)
{
    int ch = story_chapter();
    return ch == STORY_CH_DONE ? 5 : ch;
}

static int available(int q)
{
    const SideQuest *s = &quests[q];
    int ch = chapter_now();
    if (ch < s->ch_min || ch > s->ch_max) return 0;
    if (!alive_ok(s->giver)) return 0;
    {
        const DoomoResident *g = town_res(s->giver);
        if (g->status == RES_POSSESSED) return 0;
    }
    if ((s->kind == SQ_DELIVER || s->kind == SQ_MATCH || s->kind == SQ_RECONCILE) && !alive_ok(s->target))
        return 0;
    if (s->kind == SQ_EXORCISE) {
        const DoomoResident *t = town_res(s->target);
        if (!t || (t->status != RES_ALIVE && t->status != RES_POSSESSED)) return 0;
    }
    if (s->kind == SQ_DELIVER && s->target == ID_STEAMY && story_flag(SF_STEAMY_LOST)) return 0;
    /* the plot's own big moments come first */
    if (ch == 1 && story_beat() < BEAT_C1_FRIENDS) return 0;
    if (ch == 2 && story_beat() < BEAT_C2_RIVET && s->kind == SQ_EXORCISE) return 0;
    if (s->target == ID_STEAMY && story_beat() < BEAT_C2_RIVET) return 0;
    if (ch == 3 && story_beat() < BEAT_C3_NESTS) return 0;
    if (ch == 4 && s->gmap == HV && story_beat() < BEAT_C4_PRISONERS) return 0;
    return 1;
}

int story_side_count(void) { return NQ; }

int story_side_state(int q)
{
    int s;
    if (q < 0 || q >= NQ) return SIDE_LOCKED;
    s = raw_state(q);
    if (s == 0) return available(q) ? SIDE_OFFERED : SIDE_LOCKED;
    return s == 1 ? SIDE_ACTIVE : s == 2 ? SIDE_READY : SIDE_DONE;
}

/* Titles and hints live in the legside pack: decoded into a small ring
 * so a caller may hold two at once (the quest log's row and its hint). */
static const char *side_text(unsigned tid)
{
    static char ring[2][96];
    static uint16_t ring_id[2] = { TXT_NONE, TXT_NONE };   /* S4: what each slot holds */
    static uint8_t k;
    char *b;
    /* (S4) the HUD asks for the same hint every tick: decode it once (on the
     * PS1 a decode can mean a CD read when a mission pack holds the slot) */
    if (ring_id[0] == tid) return ring[0];
    if (ring_id[1] == tid) return ring[1];
    b = ring[k & 1];
    ring_id[k & 1] = (uint16_t)tid;
    k++;
    if (txt_get((uint16_t)tid, b, (int)sizeof(ring[0])) < 0) { b[0] = 0; ring_id[(k - 1) & 1] = TXT_NONE; }
    return b;
}

const char *story_side_title(int q) { return (q >= 0 && q < NQ) ? side_text(quests[q].title) : ""; }
int story_side_kind(int q) { return (q >= 0 && q < NQ) ? quests[q].kind : 0; }
int story_side_giver(int q) { return (q >= 0 && q < NQ) ? quests[q].giver : -1; }
int story_side_chapter(int q) { return (q >= 0 && q < NQ) ? quests[q].ch_min : 0; }
const char *story_side_hint(int q) { return (q >= 0 && q < NQ) ? side_text(quests[q].hint) : ""; }
const char *story_side_kind_name(int k) { return (k >= 0 && k < SQ_KIND_COUNT) ? kind_names[k] : ""; }

void story_side_where(int q, int *map_id, int *cx, int *cy)
{
    if (q < 0 || q >= NQ) return;
    if (map_id) *map_id = quests[q].gmap;
    if (cx) *cx = quests[q].gx;
    if (cy) *cy = quests[q].gy;
}

int story_side_target(int q, int *map_id, int *cx, int *cy)
{
    const SideQuest *s;
    if (q < 0 || q >= NQ) return 0;
    s = &quests[q];
    if (map_id) *map_id = s->tmap;
    if (cx) *cx = s->tx;
    if (cy) *cy = s->ty;
    return s->kind == SQ_FIND || s->kind == SQ_PET || s->kind == SQ_PHOTO ||
           s->kind == SQ_DELIVER || s->kind == SQ_MATCH || s->kind == SQ_EXORCISE ||
           s->kind == SQ_RECONCILE || s->kind == SQ_HUNT || s->kind == SQ_VISIT;
}

int story_side_done_count(void)
{
    int q, n = 0;
    for (q = 0; q < NQ; q++) n += raw_state(q) == 3;
    return n;
}

int side_active_count(void)
{
    int q, n = 0;
    for (q = 0; q < NQ; q++) n += raw_state(q) == 1 || raw_state(q) == 2;
    return n;
}

const char *side_objective(void)
{
    int q;
    for (q = 0; q < NQ; q++) if (raw_state(q) == 2) return "Hand in a finished side quest!";
    for (q = 0; q < NQ; q++) if (raw_state(q) == 1) return side_text(quests[q].hint);
    for (q = 0; q < STORY_SIDE2_ROWS; q++)          /* S4: an S100+ mission under way */
        if (doomo_side_rows[q].kind && story_side2_state(q) == 2) return story_side2_hint(q);
    return "";
}

/* ---- S2: the side-table walker (S100..S217, core/gen/side_table.c) ----------
 * The new missions are VM scripts in the side packs; their rows say who
 * offers them, when and after what. The gate (SPEC S2): a mission is offered
 * in its chapter window, after the mission it requires; part k of a chain
 * after part k-1, and at most one part of a chain per chapter; parts 3+ of a
 * chain that crosses the Wall only from Ep 4.1. STRY.side2 keeps 2 bits per
 * row (0 none, 1 offered, 2 active, 3 done). Which chapter a chain's last
 * part was turned in is kept for the session only (STRY is full): after a
 * reload the next part may come one chapter early, never out of order. */
static uint8_t chain_done_ch[27];        /* the chapter (0..5) each chain's last part was done; [26]: S5 */
/* --- S5: the old-quest / S100+ alternation of side_npc lives in the last
 * byte of chain_done_ch (its alignment slack: no RAM), so a new game starts
 * both afresh - before, both carried over from the last game of the
 * session --- */
#define SIDE_TURN chain_done_ch[26]
void story_side_reset(void)
{
    memset(chain_done_ch, 0, sizeof(chain_done_ch));
}
/* --- end S5 --- */
static int16_t cur2 = -1;                 /* the row whose script is running */

static int side2_bits(const uint8_t *side2, int sid)
{
    return (side2[sid >> 2] >> ((sid & 3) * 2)) & 3;
}

static int chain_crosses(const StorySideRow *rows, int chain)
{
    int i, seen = 0;
    for (i = 0; i < STORY_SIDE2_ROWS; i++) {
        const StorySideRow *r = &rows[i];
        if (!r->kind || r->chain != chain) continue;
        if (r->side == 3) return 1;
        if (r->side <= 1) seen |= 1 << r->side;
    }
    return seen == 3;
}

int story_side2_gate(const StorySideRow *rows, int sid, const uint8_t *side2,
                     int chapter, int ep41, const uint8_t *done_in_chapter)
{
    const StorySideRow *r;
    int i;
    if (!rows || !side2 || sid < 0 || sid >= STORY_SIDE2_ROWS) return 0;
    r = &rows[sid];
    if (!r->kind || side2_bits(side2, sid) != 0) return 0;
    if (chapter < r->ch_lo || chapter > r->ch_hi) return 0;
    if (r->requires && (r->requires - 1 >= STORY_SIDE2_ROWS || side2_bits(side2, r->requires - 1) != 3)) return 0;
    if (r->chain >= 'A' && r->chain <= 'Z') {
        if (r->step > 1) {
            int prev = -1;
            for (i = 0; i < STORY_SIDE2_ROWS; i++)
                if (rows[i].kind && rows[i].chain == r->chain && rows[i].step == r->step - 1) { prev = i; break; }
            if (prev < 0 || side2_bits(side2, prev) != 3) return 0;
            /* (S4) grafts are exempt; (S5) so is the post-game: Chapter 5
             * has no chapters left to space the parts out (chains L, U and
             * T live there), their Laters do it */
            if (done_in_chapter && done_in_chapter[r->chain - 'A'] == chapter &&
                sid < STORY_SIDE2_GRAFT_FIRST && chapter < STORY_CH_DONE) return 0;
        }
        if (r->step >= 3 && !ep41 && sid < STORY_SIDE2_GRAFT_FIRST && chain_crosses(rows, r->chain)) {
            /* (S4) "part 3" counts the chain's own parts, not its grafts */
            int part = 0;
            for (i = 0; i < STORY_SIDE2_GRAFT_FIRST; i++)
                if (rows[i].kind && rows[i].chain == r->chain && rows[i].step <= r->step) part++;
            if (part >= 3) return 0;
        }
    }
    return 1;
}

int story_side2_available(int sid)
{
    const DoomoResident *g;
    int ch = chapter_now(), ep41;
    if (sid < 0 || sid >= STORY_SIDE2_ROWS || !doomo_side_rows[sid].kind) return 0;
    g = town_res(doomo_side_rows[sid].giver);
    if (!g || g->status != RES_ALIVE) return 0;
    /* (S4) after=: the episode the mission follows from is done */
    if (doomo_side_rows[sid].tag && !story_episode_done(doomo_side_rows[sid].tag)) return 0;
    ep41 = story_episode_order(story_beat()) >= story_episode_order(BEAT_EP4_1);
    return story_side2_gate(doomo_side_rows, sid, story_stry()->side2, ch, ep41, chain_done_ch);
}

/* Where a mission's giver stands: Hillvile givers are on the invaded town
 * in Ch3 (SPEC S2), Hellvile's in Hellvile, the park's in the united park. */
int story_side2_giver_map(int sid)
{
    int side;
    if (sid < 0 || sid >= STORY_SIDE2_ROWS) return -1;
    side = doomo_side_rows[sid].side;
    if (side == 1) return MAPID_HELLVILE;
    if (side == 2) return MAPID_PARK_UNITED;
    if (side == 3 && story_map() == MAPID_HELLVILE) return MAPID_HELLVILE;
    return chapter_now() == 3 ? MAPID_HAHALAND_INVADED : MAPID_HAHALAND;
}

int story_trophies(const StorySideRow *rows, const uint8_t *side2, uint8_t *out, int max)
{
    int i, n = 0;
    if (!rows || !side2) return 0;
    for (i = 0; i < STORY_SIDE2_ROWS && n < max; i++)
        if (rows[i].kind && rows[i].trophy != 0xFFFFu && side2_bits(side2, i) == 3) out[n++] = (uint8_t)i;
    return n;
}

/* --- S4: the quest log's S100..S217 rows ---
 * Visible once its giver has offered it (or it is on offer now): offered,
 * active and done rows; title and hint (title + 1) from the index pack. */
int story_side2_visible(int sid)
{
    int st;
    if (sid < 0 || sid >= STORY_SIDE2_ROWS || !doomo_side_rows[sid].kind || doomo_side_rows[sid].title == TXT_NONE)
        return 0;
    st = story_side2_state(sid);
    return st != 0 || story_side2_available(sid);
}

const char *story_side2_title(int sid)
{
    return (sid >= 0 && sid < STORY_SIDE2_ROWS && doomo_side_rows[sid].title != TXT_NONE) ?
           side_text(doomo_side_rows[sid].title) : "";
}

const char *story_side2_hint(int sid)
{
    return (sid >= 0 && sid < STORY_SIDE2_ROWS && doomo_side_rows[sid].title != TXT_NONE) ?
           side_text(doomo_side_rows[sid].title + 1u) : "";
}
/* --- end S4 --- */

/* Talking to a giver: an offered (or newly available) mission runs its
 * offer script (offers != 0), an active one its `active` script (offers
 * == 0); the scripts set the state themselves (quest S1xx active / done). */
static int side2_npc(int id, int offers)
{
    int sid;
    for (sid = 0; sid < STORY_SIDE2_ROWS; sid++) {
        const StorySideRow *r = &doomo_side_rows[sid];
        int st;
        if (!r->kind || r->giver != (unsigned)id) continue;
        st = story_side2_state(sid);
        if (st == 0 && !story_side2_available(sid)) continue;
        if (st == 3) continue;
        if (!offers && st == 2 && r->active != 0xFFFFu) {
            cur2 = (int16_t)sid;
            story_conv(r->active, 67 /* side2_after */);
            return 1;
        }
        if (offers && st <= 1 && r->offer != 0xFFFFu) {
            if (st == 0) story_side2_set_state(sid, 1);
            cur2 = (int16_t)sid;
            story_conv(r->offer, 67 /* side2_after */);
            return 1;
        }
    }
    return 0;
}

static void side2_after(void)
{
    int sid = cur2;
    cur2 = -1;
    if (sid < 0 || sid >= STORY_SIDE2_ROWS) return;
    if (story_side2_state(sid) == 3 && sid < STORY_SIDE2_GRAFT_FIRST) {   /* (S4) a graft holds nothing back */
        int c = doomo_side_rows[sid].chain;
        if (c >= 'A' && c <= 'Z') chain_done_ch[c - 'A'] = (uint8_t)chapter_now();
    }
    ow_refresh_cast();
}

/* ---- accepting & finishing -------------------------------------------------- */
static int16_t cur_q = -1;

static void on_accept(int q)
{
    const SideQuest *s = &quests[q];
    set_state(q, 1);
    if (s->kind == SQ_DELIVER && s->other > ITEM_NONE && s->other < ITEM_COUNT) inv_add(s->other, 1);
    if (s->kind == SQ_EXORCISE) {
        DoomoResident *t = town_res(s->target);
        if (t && t->status == RES_ALIVE) {
            t->stat[STAT_CORRUPTION] = 100;
            t->status = RES_POSSESSED;
            t->exorcism = 0;
            portrait_invalidate(s->target);
        }
    }
    ui_toast(STR_ICON_HEART " Side quest accepted!");
    audio_sfx(SFX_SELECT);
    ow_refresh_cast();
}

static void finish(int q)
{
    const SideQuest *s = &quests[q];
    int i, all = 1;
    set_state(q, 3);
    story_befriend(s->giver, 4);
    story_reward(s->money, s->item, s->item_n, s->unlock);
    ui_toast(STR_ICON_STAR " Side quest complete!");
    audio_sfx(SFX_HAPPY);
    for (i = 0; i < NQ; i++) if (raw_state(i) != 3) { all = 0; break; }
    if (all) unlock_grant(USTORY_ALL_SIDE);
    ow_refresh_cast();
}

/* (S4) the offer script ends on its accept choice: option 0 takes the quest */
static void accept_choice(int c)
{
    if (cur_q >= 0 && c == 0) on_accept(cur_q);
}

static void say_giver(int q, const char *text)
{
    story_say(quests[q].giver, text);
}

static void hunt_choice(int c)
{
    const SideQuest *s;
    if (cur_q < 0 || c != 0) return;
    s = &quests[cur_q];
    if (s->target == MAPID_HAHALAND_INVADED)
        story_push_combat(s->target, CTX_SIDE, cur_q, s->tx, s->ty, s->other);
    else
        story_push_combat(s->target, CTX_SIDE, cur_q, -1, -1, -1);
}

static const char *const O_hunt[] = { "Let's go!", "Not yet", NULL };

static void exo_choice(int c)
{
    if (cur_q >= 0 && c == 0) story_push_exorcism(quests[cur_q].target, CTX_SIDE, cur_q);
}

/* ---- talking ------------------------------------------------------------------ */
static int old_npc(int id);

/* --- S4: one resident, two lists (Shutterbug gives #3 and S147, Lou #0 and
 * S121): an old quest you can finish here goes first, then the mission under
 * way, then the offers (an old one and a new one take turns, so declining
 * one never hides the other), then an old quest's "still on it?". */
static int old_ready(int id)
{
    int q;
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        int st = raw_state(q);
        if (s->giver == id && (st == 2 || (st == 1 && s->kind == SQ_FETCH && inv_count(s->target) >= s->count)))
            return 1;
        if (s->target != id || st != 1) continue;
        if ((s->kind == SQ_DELIVER && inv_count(s->other) > 0) || s->kind == SQ_MATCH || s->kind == SQ_RECONCILE)
            return 1;
        if (s->kind == SQ_EXORCISE && town_res(id) && town_res(id)->status == RES_POSSESSED) return 1;
    }
    return 0;
}

static int old_offer(int id)
{
    int q;
    for (q = 0; q < NQ; q++)
        if (quests[q].giver == id && raw_state(q) == 0 && available(q)) return 1;
    return 0;
}

int side_npc(int id)
{
    if (old_ready(id)) return old_npc(id);
    if (side2_npc(id, 0)) return 1;                       /* the S100+ mission under way */
    if (!old_offer(id) || (SIDE_TURN ^= 1u) != 0) {
        if (side2_npc(id, 1)) return 1;                   /* an S100+ offer */
    }
    return old_npc(id);
}
/* --- end S4 --- */

static int old_npc(int id)
{
    int q;
    /* 1) someone you are delivering to / matching / reconciling / exorcising */
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        if (raw_state(q) != 1 || s->target != id) continue;
        if (s->kind == SQ_DELIVER) {
            int it = s->other;
            if (inv_count(it) > 0) {
                inv_take(it, 1);
                story_befriend(id, 3);
                story_say_txt(id, TXT_SS_1);
                set_state(q, 2);
                ui_toast(STR_ICON_CHECK " Delivered! Report back.");
                ow_refresh_cast();
                return 1;
            }
        } else if (s->kind == SQ_MATCH) {
            int ok;
            story_fmt_target = (int16_t)s->other;
            story_say_txt(id, TXT_SS_2);
            ok = sim_confess(id, s->other, 45);
            story_say_txt(-1, ok ? TXT_SS_3 :
                               TXT_SS_4);
            set_state(q, 2);
            ow_refresh_cast();
            return 1;
        } else if (s->kind == SQ_RECONCILE) {
            DoomoEdge *e;
            story_fmt_target = (int16_t)s->giver;
            story_say_txt(id, TXT_SS_5);
            rel_adjust(id, s->giver, 45);
            e = rel_find(id, s->giver);
            if (e && (e->kind & REL_KIND_MASK) == REL_ACQUAINTANCE) rel_set_kind(id, s->giver, REL_FRIEND);
            progress_add(PROG_CROSS_BORDER_FRIENDS, 1);
            story_befriend(id, 3);
            set_state(q, 2);
            ow_refresh_cast();
            return 1;
        } else if (s->kind == SQ_EXORCISE) {
            const DoomoResident *t = town_res(id);
            if (t && t->status == RES_POSSESSED) {
                static const char *const O_ex[] = { "Begin the exorcism!", "Not yet", NULL };
                cur_q = (int16_t)q;
                story_fmt_target = (int16_t)id;
                story_say_txt(-1, TXT_SS_6);
                story_choice_id(-1, "Perform the exorcism?", O_ex, 2, 64 /* exo_choice */);
                return 1;
            }
        }
    }
    /* 2) a giver */
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        int st = raw_state(q);
        if (s->giver != id) continue;
        cur_q = (int16_t)q;
        if (st == 0 && available(q)) {
            /* (S4) the re-timed offer: a script that ends on the accept choice */
            story_fmt_target = (int16_t)(s->kind == SQ_MATCH || s->kind == SQ_DELIVER || s->kind == SQ_RECONCILE ||
                                         s->kind == SQ_EXORCISE ? s->target : id);
            story_conv(s->offer, 65 /* accept_choice */);
            return 1;
        }
        if (st == 2) {
            story_conv(s->thanks, 68 /* S4: finish after the thank-you */);
            return 1;
        }
        if (st == 1) {
            if (s->kind == SQ_FETCH) {
                if (inv_count(s->target) >= s->count) {
                    inv_take(s->target, s->count);
                    story_conv(s->thanks, 68 /* S4: finish after the thank-you */);
                } else {
                    char buf[96];
                    doomo_snprintf(buf, sizeof(buf), "Any luck? I need %d %s. You have %d.", s->count,
                                   doomo_items[s->target].name, inv_count(s->target));
                    say_giver(q, buf);
                }
                return 1;
            }
            if (s->kind == SQ_HUNT) {
                say_giver(q, "Ready to fight? It's dangerous out there!");
                story_choice_id(id, story_side_hint(q), O_hunt, 2, 66 /* hunt_choice */);
                return 1;
            }
            {
                char buf[96];
                doomo_snprintf(buf, sizeof(buf), "Still on it? %s.", story_side_hint(q));
                say_giver(q, buf);
            }
            return 1;
        }
    }
    return 0;
}

/* ---- spots: find / pet / photo ---------------------------------------------------- */
static int same_map(int a, int b)
{
    if (a == b) return 1;
    return (a == MAPID_HAHALAND || a == MAPID_HAHALAND_INVADED) &&
           (b == MAPID_HAHALAND || b == MAPID_HAHALAND_INVADED);
}

int side_spots(int map_id, SideSpot *out, int max)
{
    int q, n = 0;
    for (q = 0; q < NQ && n < max; q++) {
        const SideQuest *s = &quests[q];
        if (raw_state(q) != 1) continue;
        if (s->kind != SQ_FIND && s->kind != SQ_PET && s->kind != SQ_PHOTO) continue;
        if (!same_map(s->tmap, map_id)) continue;
        out[n].x = s->tx;
        out[n].y = s->ty;
        out[n].kind = s->kind;
        out[n].q = (uint8_t)q;
        n++;
    }
    return n;
}

int side_trigger_spot(int map_id, int cx, int cy, int press)
{
    int q;
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        int dx, dy;
        if (raw_state(q) != 1) continue;
        if (s->kind != SQ_FIND && s->kind != SQ_PET && s->kind != SQ_PHOTO) continue;
        if (!same_map(s->tmap, map_id)) continue;
        dx = cx - s->tx; dy = cy - s->ty;
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1) continue;
        if (s->kind == SQ_PHOTO && !press) continue;
        set_state(q, 2);
        if (s->kind == SQ_PHOTO) {
            audio_sfx(SFX_SELECT);
            ow_flash(PAL_WHITE, 6);
            story_say_txt(-1, TXT_SS_7);
        } else if (s->kind == SQ_PET) {
            audio_sfx(SFX_SQUEAK);
            story_say_txt(-1, TXT_SS_8);
        } else {
            audio_sfx(SFX_SPARKLE);
            story_say_txt(-1, TXT_SS_9);
        }
        ow_refresh_cast();
        return 1;
    }
    return 0;
}

/* ---- places (visit quests) ---------------------------------------------------------- */
int side_place(int place)
{
    int q;
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        if (raw_state(q) == 1 && s->kind == SQ_VISIT && s->target == place) {
            set_state(q, 2);
            ui_toast(STR_ICON_CHECK " Visited! Report back.");
            ow_refresh_cast();
            /* fall through to the place's normal menu */
            return 0;
        }
    }
    return 0;
}

/* ---- sub-scene results ------------------------------------------------------------- */
void side_resume(const StoryPending *p, int result)
{
    int q = p->extra;
    if (q < 0 || q >= NQ) return;
    if (p->kind == PEND_COMBAT) {
        if (result > 0) { set_state(q, 2); story_say_txt(-1, TXT_SS_10); }
        else story_say_txt(-1, TXT_SS_11);
    } else if (p->kind == PEND_EXORCISM) {
        story_fmt_target = quests[q].target;
        if (result > 0) { set_state(q, 2); story_say_txt(-1, TXT_SS_12); }
        else if (result < 0) { set_state(q, 2); story_say_txt(-1, TXT_SS_13); }
        else story_say_txt(-1, TXT_SS_14);
    }
    ow_refresh_cast();
}

/* ---- S4: the S100..S217 givers on the maps ----------------------------------------------
 * A mission's giver stands at its at= cell on its map (story_side2_giver_map)
 * while the mission can be offered or is active: "?" over them while it is
 * on offer. In Chapter 3 the Hillvile givers shelter around the church on
 * the invaded map instead (story_final.md sec.14), on the cells left free
 * by the plot's church cast (story_ch34.c). The resident a running verb
 * needs (the escort's follower, the photo's subject, the delivery's
 * recipient) is put back on its district's map after every cast refresh. */
static const uint8_t church_ring[12][2] = {
    { 12, 27 }, { 14, 28 }, { 16, 27 }, { 13, 29 }, { 18, 28 }, { 15, 29 },
    { 11, 27 }, { 17, 29 }, { 20, 27 }, { 19, 29 }, { 2, 27 }, { 21, 28 }
};

static int cast_find(const StoryNpcSpot *out, int n, int id)
{
    int i;
    for (i = 0; i < n; i++) if (out[i].id == id) return i;
    return -1;
}

static int side2_cast(int map_id, StoryNpcSpot *out, int n, int max)
{
    int sid, ring = 0, who, vx, vy, k;
    for (sid = 0; sid < STORY_SIDE2_ROWS && n < max; sid++) {
        const StorySideRow *r = &doomo_side_rows[sid];
        int st, x, y, invaded;
        if (!r->kind || !r->spot || r->spot > doomo_side_spot_count) continue;
        st = story_side2_state(sid);
        if (st == 3 || (st == 0 && !story_side2_available(sid)) || !alive_ok(r->giver)) continue;
        if (story_side2_giver_map(sid) != map_id) continue;
        if ((k = cast_find(out, n, r->giver)) >= 0) {
            /* already standing for an old quest (or an earlier mission): keep
             * that spot, show the "?" of this offer too */
            if (st != 2) out[k].flags |= NPCF_SIDE;
            continue;
        }
        invaded = map_id == MAPID_HAHALAND_INVADED && r->side == 0;
        x = invaded ? church_ring[ring % 12][0] : doomo_side_spots[r->spot - 1][0];
        y = invaded ? church_ring[ring % 12][1] : doomo_side_spots[r->spot - 1][1];
        ring += invaded;
        out[n].id = (int16_t)r->giver;
        out[n].x = (uint8_t)x;
        out[n].y = (uint8_t)y;
        out[n].dir = 0;
        out[n].flags = (uint8_t)(NPCF_FIXED | (st == 2 ? 0 : NPCF_SIDE));
        n++;
    }
    who = story_verb_who(&vx, &vy);
    if (who >= 0 && who < DOOMO_MAX_RESIDENTS && n < max && cast_find(out, n, who) < 0) {
        int home = RES_DISTRICT(who) ? MAPID_HELLVILE :
                   story_chapter() == STORY_CH3 ? MAPID_HAHALAND_INVADED : MAPID_HAHALAND;
        int px, py;
        if (story_verb_active() == VERB_ESCORT && ow_player_cell(&px, &py)) {
            vx = px + 1;                                 /* the follower stays at your side */
            vy = py;
            home = map_id;
        }
        if (home == map_id && (vx || vy)) {
            if (ownpc_find(who) >= 0) {
                /* the plot's cast placed them at their usual spot this very
                 * refresh (ownpc_spawn_all adds script_cast first): the verb
                 * wins, so move them back to where the verb needs them */
                ownpc_cast(who, vx | (vy << 8), NPCF_FIXED);
            } else {
                out[n].id = (int16_t)who;
                out[n].x = (uint8_t)vx;
                out[n].y = (uint8_t)vy;
                out[n].dir = 0;
                out[n].flags = NPCF_FIXED;
                n++;
            }
        }
    }
    return n;
}
/* --- end S4 --- */

/* ---- the cast of side quests on a map ------------------------------------------------ */
int side_cast(int map_id, StoryNpcSpot *out, int max)
{
    int q, n = 0;
    for (q = 0; q < NQ && n < max; q++) {
        const SideQuest *s = &quests[q];
        int st = raw_state(q);
        if (st == 3) continue;
        if (st == 0 && !available(q)) continue;
        if (same_map(s->gmap, map_id) && n < max) {
            out[n].id = s->giver;
            out[n].x = s->gx;
            out[n].y = s->gy;
            out[n].dir = 0;
            out[n].flags = (uint8_t)(NPCF_FIXED | (st == 2 ? NPCF_READY : st == 0 ? NPCF_SIDE : 0));
            n++;
        }
        if (st == 1 && (s->kind == SQ_DELIVER || s->kind == SQ_MATCH || s->kind == SQ_RECONCILE ||
                        s->kind == SQ_EXORCISE) && same_map(s->tmap, map_id) && n < max) {
            out[n].id = s->target;
            out[n].x = s->tx;
            out[n].y = s->ty;
            out[n].dir = 0;
            out[n].flags = NPCF_FIXED | NPCF_SIDE;
            n++;
        }
    }
    return side2_cast(map_id, out, n, max);         /* S4: the S100..S217 givers */
}

/* ---- debug ---------------------------------------------------------------------------- */
void story_side_debug_accept(int q)
{
    if (q < 0 || q >= NQ) return;
    on_accept(q);
}

void story_side_debug_finish(int q)
{
    if (q < 0 || q >= NQ) return;
    finish(q);
}

/* choice callbacks by id (story.c story_poll_callbacks; see story_int.h) */
void side_callback(int cb, int choice)
{
    switch (cb) {
    case 64: exo_choice(choice); break;
    case 65: accept_choice(choice); break;
    case 66: hunt_choice(choice); break;
    case 67: side2_after(); break;
    /* --- S4: the old quests' thank-you, and the side scripts' questions --- */
    case 68: if (cur_q >= 0 && cur_q < NQ) finish(cur_q); break;
    case CB_SIDE_HAVE: vm_set_var(0, choice > ITEM_NONE && choice < ITEM_COUNT ? inv_count(choice) : 0); break;
    case CB_SIDE_STATE: vm_set_var(0, choice >= 100 && choice < 100 + STORY_SIDE2_ROWS ? story_side2_state(choice - 100) : 0); break;
    case CB_SIDE_NIGHT: vm_set_var(0, town_is_night()); break;
    case CB_SIDE_STATUS: { const DoomoResident *r = town_res(choice); vm_set_var(0, r ? r->status : RES_EMPTY); break; }
    case CB_SIDE_QUEST: vm_set_var(0, story_side_state(choice)); break;
    case CB_SIDE_MONEY: vm_set_var(0, (int)DOOMO_MIN(doomo_town.money / 10u, 32767u)); break;
    case CB_SIDE_DAY: vm_set_var(0, doomo_town.weekday); break;
    case CB_SIDE_MUSIC: audio_music_mute(0); audio_music(ow_music_for_map(story_map())); break;
    case CB_SIDE_ONCE:
        if (choice > SONG_NONE && choice < SONG_COUNT) {
            audio_music_once(choice);
            if (choice >= SONG_MINGLE) story_unlock_quiet(UL_STORY_SONG2(choice));   /* heard: the jukebox */
        }
        break;
    case CB_SIDE_CHAPTER: vm_set_var(0, chapter_now()); break;
    /* --- end S4 --- */
    /* --- S5: batch B's questions and actions (story_int.h) --- */
    case CB_SIDE_NEWS: { const char *s = txt_stash_get(0); if (s && s[0]) sim_news_push(s); break; }
    case CB_SIDE_GRUMBLE: story_grumble_count(choice & 0xFF, (choice >> 8) & 0xFF); break;
    case CB_SIDE_FRIENDS: vm_set_var(0, story_friend_count(-1)); break;
    case CB_SIDE_BRANCH: vm_set_var(0, (story_stry()->branch >> (2 * (choice & 3))) & 3); break;
    /* --- end S5 --- */
    default: break;
    }
}
