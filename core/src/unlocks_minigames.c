/* unlocks_minigames.c - the Fun Fair's unlockables (ids 832..863, owner:
 * minigames; the unused part of art's range, see mg.h).
 *
 * Every arcade game has three medals, granted automatically from its best
 * score (PROG_HS_*): bronze is a fair trophy, silver unlocks a window
 * skin / cursor / title theme, gold a creator part. Playing all six
 * earns Cat Ears, gold in all six the Showstopper Heels. The thresholds
 * are the MG_*_B/S/G constants of mg.h, shared with MgGame.medal[]. Trophies for special
 * feats are granted from code (need == 0). */
#include "doomo/unlocks.h"
#include "doomo/catalog.h"
#include "doomo/town.h"
#include "doomo/ui.h"
#include "mg.h"

#define MG_LIST 0x46        /* UL_CAT_MISC list id of the fair's trophies */
#define TROPHY(id, name, how, prog, need) { id, UL_CAT_MISC, MG_LIST, (id) - MG_UL_BASE, name, how, prog, need }
#define SKIN(id, opt, val, name, how, prog, need) { id, UL_CAT_SKIN, opt, val, name, how, prog, need }
#define PART(id, list, idx, name, how, prog, need) { id, UL_CAT_PART, list, idx, name, how, prog, need }

const DoomoUnlock doomo_unlocks_minigames[] = {
    /* ---- bronze: fair trophies ---- */
    TROPHY(832, "Sprinkle Ribbon", "Cupcake Catch: bronze medal", PROG_HS_CATCH, MG_CATCH_B),
    TROPHY(833, "Bonk Badge", "Whack-an-Imp: bronze medal", PROG_HS_WHACK, MG_WHACK_B),
    TROPHY(834, "Puff Pin", "Vape Cloud Dodge: bronze medal", PROG_HS_DODGE, MG_DODGE_B),
    TROPHY(835, "Popper Patch", "Bubble Blaster: bronze medal", PROG_HS_GALLERY, MG_GALLERY_B),
    TROPHY(836, "Derby Rosette", "Doomogachi Derby: bronze medal", PROG_HS_DERBY, MG_DERBY_B),
    TROPHY(837, "Moth Brooch", "Memory Match: bronze medal", PROG_HS_MEMORY, MG_MEMORY_B),
    /* ---- silver: skins, cursors, a title theme ---- */
    SKIN(838, OPT_WINDOW_SKIN, WSKIN_BUBBLEGUM, "Bubblegum Windows", "Cupcake Catch: silver medal", PROG_HS_CATCH, MG_CATCH_S),
    SKIN(839, OPT_CURSOR, 2, "Star Cursor", "Whack-an-Imp: silver medal", PROG_HS_WHACK, MG_WHACK_S),
    SKIN(840, OPT_CURSOR, 3, "Sparkle Cursor", "Vape Cloud Dodge: silver medal", PROG_HS_DODGE, MG_DODGE_S),
    SKIN(841, OPT_WINDOW_SKIN, WSKIN_SKY, "Sky Windows", "Bubble Blaster: silver medal", PROG_HS_GALLERY, MG_GALLERY_S),
    SKIN(842, OPT_WINDOW_SKIN, WSKIN_LEMON, "Lemon Windows", "Doomogachi Derby: silver medal", PROG_HS_DERBY, MG_DERBY_S),
    SKIN(843, OPT_TITLE_THEME, 1, "Starry Night Title", "Memory Match: silver medal", PROG_HS_MEMORY, MG_MEMORY_S),
    /* ---- gold: creator parts (ones no other table locks and no creator
     * preset uses) ---- */
    PART(844, CLIST_HAT, HAT_BUNNY_EARS, "Bunny Ears", "Cupcake Catch: gold medal", PROG_HS_CATCH, MG_CATCH_G),
    PART(845, CLIST_HORNS, HORNS_ANTLERS, "Trophy Antlers", "Whack-an-Imp: gold medal", PROG_HS_WHACK, MG_WHACK_G),
    PART(846, CLIST_ACC, ACC_MASK, "Vapeproof Mask", "Vape Cloud Dodge: gold medal", PROG_HS_DODGE, MG_DODGE_G),
    PART(847, CLIST_ACC, ACC_EYEPATCH, "Sharpshooter Eyepatch", "Bubble Blaster: gold medal", PROG_HS_GALLERY, MG_GALLERY_G),
    PART(848, CLIST_HAIR, HAIR_BRAIDS, "Racing Braids", "Doomogachi Derby: gold medal", PROG_HS_DERBY, MG_DERBY_G),
    PART(849, CLIST_BOTTOM, BOTTOM_LONG_SKIRT, "Fortune Teller Skirt", "Memory Match: gold medal", PROG_HS_MEMORY, MG_MEMORY_G),
    /* ---- feats (granted from code) ---- */
    PART(850, CLIST_HAT, HAT_CAT_EARS, "Fairgoer Cat Ears", "Play all six Fun Fair games", 0, 0),
    PART(851, CLIST_SHOES, SHOES_HEELS, "Showstopper Heels", "Gold medals in all six Fun Fair games", 0, 0),
    TROPHY(852, "Jackpot!", "Hit the Wheel of Fate jackpot", 0, 0),
    TROPHY(853, "Untouchable", "Finish Vape Cloud Dodge without a scratch", 0, 0),
    TROPHY(854, "Fuzz Fan", "Whack-an-Imp: bronze without bonking Father Fuzz", 0, 0),
    TROPHY(855, "Perfect Recall", "Clear Memory Match without a miss", 0, 0),
    TROPHY(856, "Photo Finish", "Win the Doomogachi Derby by a nose", 0, 0),
    TROPHY(857, "Sugar Rush x3", "Three Sugar Rushes in one Cupcake Catch", 0, 0),
    TROPHY(858, "Cardboard Pacifist", "Bubble Blaster: bronze without hitting a citizen", 0, 0),
    TROPHY(859, "Cursed!", "Get cursed by the Wheel of Fate", 0, 0),
    TROPHY(860, "Hellfire Champion", "Win a gold medal on the Hellfire tier", 0, 0),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
