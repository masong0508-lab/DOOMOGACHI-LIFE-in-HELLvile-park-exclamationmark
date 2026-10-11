/* unlocks_levels.c - the levels expansion's unlockables (ids 864..895,
 * owner: levels; UL_CAT_MISC list UL_MISC_LEVELS, see unlock_ids.h).
 *
 * Time trial gold medals, the gates of the arena modes (Time Trial,
 * challenge runs, Monster Mode Mayhem), trophies for every new mini-boss,
 * the Boss Rush / Horde / Mayhem feats, and three creator-part rewards
 * that are alternate routes to parts other teams lock too (UL_ALIAS_MAX).
 * Ranks themselves live in the PROG_BREACH_RANKS counter (levels.c); all
 * of these are granted from code (need == 0). 891..895 are spare. */
#include "levels_int.h"

#define TROPHY(id, name, how) { id, UL_CAT_MISC, UL_MISC_LEVELS, (id) - UL_IDS_LEVELS_FIRST, name, how, 0, 0 }
#define PART(id, list, idx, name, how) { id, UL_CAT_PART, list, idx, name, how, 0, 0 }

const DoomoUnlock doomo_unlocks_levels[] = {
    /* ---- time trial golds (beat the dev time, lv_info[].dev) ---- */
    TROPHY(864, "Gold: Vape Cellar", "Time Trial: beat 1:35 in The Vape Cellar"),
    TROPHY(865, "Gold: Cloud Nine Spire", "Time Trial: beat 3:20 in Cloud Nine Spire"),
    TROPHY(866, "Gold: Sugar Rush Sewers", "Time Trial: beat 2:20 in Sugar Rush Sewers"),
    TROPHY(867, "Gold: Plushie Factory", "Time Trial: beat 2:30 in the Plushie Factory"),
    TROPHY(868, "Gold: Graveyard Shift", "Time Trial: beat 2:55 in Graveyard Shift"),
    TROPHY(869, "Gold: Bone Cathedral", "Time Trial: beat 3:10 in the Bone Cathedral"),
    TROPHY(870, "Gold: Hellvile Underbelly", "Time Trial: beat 3:20 in the Hellvile Underbelly"),
    /* ---- mode gates ---- */
    TROPHY(871, "Time Trial", "Clear any level from Hell Breach Levels"),
    TROPHY(872, "Challenge Runs", "Clear 3 different Hell Breach levels"),
    /* ---- feats ---- */
    TROPHY(873, "Confetti Purist", "Clear a Cute-Only Run"),
    TROPHY(874, "Gib Connoisseur", "Clear a Brutal-Only Run"),
    TROPHY(875, "Boss Rush Champion", "Beat all seven bosses in Boss Rush"),
    TROPHY(876, "Speedy Slayer", "Finish Boss Rush in under 7 minutes"),
    TROPHY(877, "Horde Hero", "Reach wave 10 in Horde Mode"),
    TROPHY(878, "Horde Legend", "Reach wave 20 in Horde Mode"),
    PART(879, CLIST_WINGS, WINGS_FLAME, "Horde Wings", "Reach wave 30 in Horde Mode"),
    TROPHY(880, "Monster Mode Mayhem", "Turn on Monster Mode, then visit the Arena"),
    TROPHY(881, "Mayhem Maniac", "Defeat 100 demons in one Monster Mode Mayhem"),
    TROPHY(882, "Hell Breach Tourist", "Clear all seven Hell Breach levels"),
    TROPHY(883, "Sweet Perfection", "Get an S rank on any Hell Breach level"),
    /* ---- the new mini-bosses ---- */
    TROPHY(884, "Syrup Slayer", "Defeat the Gumdrop Gutterking (Sugar Rush Sewers)"),
    TROPHY(885, "Union Buster", "Defeat Foreman Fluffnuts (Plushie Factory)"),
    TROPHY(886, "Ghost Whisperer", "Comfort the Weeping Widow (Graveyard Shift)"),
    TROPHY(887, "Organ Donor", "Silence Bishop Bonebottom (Bone Cathedral)"),
    TROPHY(888, "Gatecrasher", "Defeat Gatekeeper Grumbleguts (Hellvile Underbelly)"),
    /* ---- creator rewards (alternate routes) ---- */
    PART(889, CLIST_HAT, HAT_CROWN, "Crown of Ranks", "Get an S rank on every Hell Breach level"),
    PART(890, CLIST_SHOES, SHOES_ROLLERSKATES, "Speedrun Skates", "Earn every Time Trial gold medal"),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
