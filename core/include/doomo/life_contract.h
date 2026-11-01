/* life_contract.h - numbers the life sim, family, pets, loss, play-as and
 * player profile packages share (owner: K0 contract, LIFESIM_PLAN R1 + section 6
 * AMENDMENTS of 2026-11-01; add-only after K0).
 *
 * K0 fixes them once so the save budget (save_budget.h SAVE_EXT_MAX_LIFE,
 * SAVE_EXT_MAX_PLAY, SAVE_EXT_MAX_PROF) holds room for every package before any of them
 * lands. The packages own the code and the exact byte layouts inside
 * their sections; they must stay within the sizes here (a change is a
 * change to this contract, with the joint worst case re-checked in
 * core/tests/test_save_budget.c).
 */
#ifndef DOOMO_LIFE_CONTRACT_H
#define DOOMO_LIFE_CONTRACT_H

/* ---- household cap tiers (F1-F4; user answer 4, 6) ---------------------
 * The household grows in three tiers: at first, after the game is
 * completed, after New Game+ is completed. Pets count toward the cap.
 * FULL is PS1 / native / Dreamcast, SMALL the GBA (its RAM). */
enum { HOUSEHOLD_TIER_START = 0, HOUSEHOLD_TIER_GAME_DONE, HOUSEHOLD_TIER_NGPLUS_DONE,
       HOUSEHOLD_TIER_COUNT };
#define HOUSEHOLD_CAP_FULL_T0   3
#define HOUSEHOLD_CAP_FULL_T1  10
#define HOUSEHOLD_CAP_FULL_T2  20
#define HOUSEHOLD_CAP_SMALL_T0  3
#define HOUSEHOLD_CAP_SMALL_T1  6
#define HOUSEHOLD_CAP_SMALL_T2 10
#define HOUSEHOLD_CAP_MAX      HOUSEHOLD_CAP_FULL_T2     /* sizes the LIFE chunk */
#define HOUSEHOLD_CAP(small, tier) \
    ((small) ? ((tier) <= 0 ? HOUSEHOLD_CAP_SMALL_T0 : (tier) == 1 ? HOUSEHOLD_CAP_SMALL_T1 \
                                                                   : HOUSEHOLD_CAP_SMALL_T2) \
             : ((tier) <= 0 ? HOUSEHOLD_CAP_FULL_T0 : (tier) == 1 ? HOUSEHOLD_CAP_FULL_T1 \
                                                                  : HOUSEHOLD_CAP_FULL_T2))

/* ---- the 'LIFE' chunk's sections (max SAVE_EXT_MAX_LIFE = 2,688 B) --------
 * LS1's plan (2,432 B: news ring, life events, the family of 20, the
 * reserved 8 B of R8) plus the amendments' new sections: */
#define LIFE_SZ_LS1_PLAN    2432   /* LS1 / F1 / LS3 / F4 as planned            */
#define LIFE_SZ_F5_PETS       64   /* F5: per pet member: species, uplift state  */
#define LIFE_SZ_F6_LOSS      160   /* F6: the lost / deceased: 20 x 8 B memorial */
#define LIFE_SZ_TIER           8   /* the household tier, cap flags              */
#define LIFE_SZ_SPARE         24   /* later sections (a content pack's state ...) */
#define LIFE_SZ_TOTAL (LIFE_SZ_LS1_PLAN + LIFE_SZ_F5_PETS + LIFE_SZ_F6_LOSS + LIFE_SZ_TIER + \
                       LIFE_SZ_SPARE)

/* ---- the 'PLAY' chunk (max SAVE_EXT_MAX_PLAY = 800 B; PA1) -----------------
 * Money, items and health are per villager (user answer 13): a villager you
 * play as gets a pocket; the PLAY chunk keeps the most recent ones (LRU;
 * the rest fall back to what the sim generates for them). */
#define PLAY_SZ_HEADER        32   /* avatar id, hero kind, flags (PA0/PA1 plan) */
#define PLAY_POCKETS          16   /* villagers with a saved pocket              */
#define PLAY_POCKET_ITEMS     16   /* item slots per pocket (item u8, count u8)  */
#define PLAY_SZ_POCKET        48   /* resident u16, health u16, money u32, 16 x 2 B
                                      items, 8 B flags / quest progress          */
#define PLAY_SZ_TOTAL (PLAY_SZ_HEADER + PLAY_POCKETS * PLAY_SZ_POCKET)

/* ---- the 'PROF' chunk (max SAVE_EXT_MAX_PROF = 2,336 B; MP0) -------------
 * Player profiles (DECISIONS "PLAYER PROFILES + MULTIPLAYER", "PROFILES
 * FLOW"): one shared world per save, Animal Crossing style. Profile 0 is
 * the town's representative (the hero); its state is the save itself
 * (town body, STRY, HOME, PLAY). Up to PROF_GUESTS more people move in,
 * each a resident with a profile record here: name, the villager / hero
 * they are, personal progress and quest state, relationships, their room
 * and the letters waiting for them. Their money, items and health are the
 * PLAY pocket of the villager they play (per villager, user answer 13); a
 * guest's room is an apartment room in the town body, not a second HOME
 * chunk (a whole house per guest is not budgeted: that is a change to
 * this contract). Four profiles = four pads on the Dreamcast. */
#define PROF_PROFILES_MAX      4   /* the hero + PROF_GUESTS                    */
#define PROF_GUESTS            3
#define PROF_SZ_HEADER        32   /* count, active / per-pad profile, flags    */
#define PROF_SZ_GUEST        768   /* name + resident + hero kind 32 B, progress
                                      flags 128 B, quest state 128 B,
                                      relationships 256 B, room + mail 192 B,
                                      spare 32 B                                */
#define PROF_SZ_TOTAL (PROF_SZ_HEADER + PROF_GUESTS * PROF_SZ_GUEST)

/* ---- the machine config record's security code (U5, E11; user answer 20) --
 * plat_ext.h PLAT_CONFIG_MAX holds U0's 32 B settings record plus this
 * block: the adults' security code (parental PIN) that turning Mature
 * content on asks for: a salted hash of the code, the lock flags and a
 * retry counter. Never the code itself. */
#define CONFIG_SZ_SETTINGS    32
#define CONFIG_SZ_SECURITY    16

#endif
