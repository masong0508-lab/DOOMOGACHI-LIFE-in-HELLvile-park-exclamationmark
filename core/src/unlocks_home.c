/* unlocks_home.c - the dream house's unlockables (owner: H1), ids 564..595
 * (SPEC R7, HF sec.9.6): 20 house trophies (564..583), then 12 raw bits
 * that are not listed here (584..595: which rooms' tutorials ran, the
 * defence tutorial, the Black Market opened). All are granted by the house
 * code (home.c, home_draw.c), none by an automatic rule. The furniture a
 * trophy gives is in the catalog (catalog.txt `unlock` + furn_grant.c).
 */
#include "home_int.h"
#include "doomo/activities.h"
#include "doomo/scenes.h"
#include "doomo/font.h"

#define TROPHY(id, name, how) \
    { (uint16_t)(id), UL_CAT_MISC, UL_MISC_HOME, (uint16_t)((id) - UL_IDS_HOME_FIRST), name, how, 0, 0 }

const DoomoUnlock doomo_unlocks_home[] = {
    TROPHY(HUL_FIRST_HOUSE, "Home Sweet Home", "Move into your own house"),
    TROPHY(HUL_COSY, "Cosy", "Earn the COSY stamp in any room"),
    TROPHY(HUL_LIT, "Keep the Lights On", "Earn the LIT stamp in any room"),
    TROPHY(HUL_HELD, "Held", "Hold the Door and earn the HELD stamp"),
    TROPHY(HUL_HOME, "Home", "Earn the HOME stamp: the Open House"),
    TROPHY(HUL_PIECES_100, "Hoarder", "Own 100 pieces of furniture"),
    TROPHY(HUL_EVERY_CAT, "A Bit of Everything", "Own a piece from every catalog category"),
    TROPHY(HUL_ALL_ROOMS, "The Whole Cottage", "Open all six rooms of the cottage"),
    TROPHY(HUL_VISIT_50, "Good Neighbour", "Visit 50 homes"),
    TROPHY(HUL_VISIT_500, "Door to Door", "Visit 500 homes"),
    TROPHY(HUL_VISIT_1000, "Every Single Door", "Visit all 1000 homes"),
    TROPHY(HUL_FIVE_STARS, "Five Stars", "Get a 5-star rating from your visitors"),
    TROPHY(HUL_PHOTOS_10, "House Proud", "Take 10 photos of your rooms"),
    TROPHY(HUL_BLUEPRINT, "By the Book", "Complete a blueprint with every piece"),
    TROPHY(HUL_HARMONY_100, "Perfect Harmony", "Reach Harmony 100 in a room"),
    TROPHY(HUL_BIG_SPOON, "Hellvile's Biggest Spoon", "Save the Feast of Rebuilding"),
    TROPHY(HUL_STARER_20, "We See You Too", "See the starer at the window 20 mornings"),
    TROPHY(HUL_SEASONAL_SET, "All Seasons", "Own a full seasonal set"),
    TROPHY(HUL_PLAIN_HUMAN, "It Does Nothing", "Buy the Plain Human figurine"),
    TROPHY(HUL_EVERY_GAG, "Running Gags", "Own every gag piece"),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};

/* ---- (H4 M2) the house's activities (SPEC sec.4 doomo_activities_home) -------
 * The table lives here, in a resident file, and not in home_mart.c: the hubs
 * read it at any time, and the home group must keep nothing resident on the
 * PS1 (HF sec.11.4). Every row is a scene edge (SCENE_HOME, SCENE_HOME_MART;
 * arg encodings in home.h and home_mart.c). */
static const DoomoActivity home_acts[] = {
    { "My Dream House", "Decorate your cottage", ACTLOC_APARTMENTS,
      ACTF_FREEPLAY | ACTF_STORY, ICON_HOUSE, 1, ACT_ALWAYS, &SCENE_HOME, HOME_ARG_OWN(HOME_COTTAGE) },
    { "Visit a Neighbour", "Knock on a citizen's door", ACTLOC_APARTMENTS,
      ACTF_FREEPLAY | ACTF_STORY, ICON_HEART, 1, ACT_ALWAYS, &SCENE_HOME_MART, 4 },
    { "HomeMart", "Furniture, 12 new pieces a day", ACTLOC_APARTMENTS,
      ACTF_FREEPLAY | ACTF_STORY, ICON_COIN, 1, ACT_ALWAYS, &SCENE_HOME_MART, 0 },
    { "Black Market", "Hellish furniture. No refunds.", ACTLOC_HELLVILE,
      ACTF_FREEPLAY | ACTF_STORY, ICON_PENTAGRAM, 2, ACT_ALWAYS, &SCENE_HOME_MART, 1 },
    { "Expand the House", "Knock a wall through (H$)", ACTLOC_TOWN_HALL,
      ACTF_FREEPLAY | ACTF_STORY, ICON_HOUSE, 1, ACT_ALWAYS, &SCENE_HOME_MART, 3 },
    { "Hold the Door", "Defend your house, wave by wave", ACTLOC_ARENA,
      ACTF_FREEPLAY | ACTF_STORY, ICON_SKULL, 0, (uint16_t)(UL_STORY_MISC2_FIRST + 10), &SCENE_HOME,
      HOME_ARG_DEFEND(HDT_ARENA) },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_home = home_acts;
