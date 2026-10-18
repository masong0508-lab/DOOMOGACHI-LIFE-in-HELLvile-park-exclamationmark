/* unlock_ids.h - THE central allocation of unlock ids, UL_CAT_MISC list
 * ids and PROG_* counter slots (owner: integration; every team reads it).
 *
 * Unlock ids are bit indices into DoomoTown.unlocks (1024 bits, saved),
 * so two teams must never hand out the same id: each team owns exactly
 * one range below and allocates ONLY inside it. Raw bits that a module
 * sets without a table entry (journal / "visited" flags) also come out of
 * the owner's range. core/tests/test_integration.c checks every
 * registered table against this file (ids unique and inside the owner's
 * range, (cat, list, index) keys unique, creator parts valid).
 *
 *   team        ids          table (unlocks.h)          notes
 *   ----------  -----------  -------------------------  ---------------------------
 *   foundation     0 .. 239  doomo_unlocks_foundation   (202..239 reserved: customisation)
 *   studio       240 .. 255  doomo_unlocks_studio       Music Studio
 *   creator      256 .. 319  doomo_unlocks_creator
 *   life         320 .. 383  doomo_unlocks_life
 *   library      384 .. 415  doomo_unlocks_library      409..415 raw bits (wings visited)
 *   action       416 .. 563  doomo_unlocks_action       (551..563 spare)
 *   home         564 .. 595  doomo_unlocks_home         dream house (unlocks_home.c, H1)
 *   (reserve)    596 .. 607  -                          unassigned, kept as a reserve
 *   story        608 .. 735  doomo_unlocks_story        sub-blocks: see below
 *   freeplay     736 .. 831  doomo_unlocks_freeplay
 *   minigames    832 .. 863  doomo_unlocks_minigames    Fun Fair
 *   levels       864 .. 895  doomo_unlocks_levels       RESERVED: levels + arena modes (WIP)
 *   concert      896 .. 959  doomo_unlocks_concert      944..959 raw bits (dream journal)
 *   fishcook     960 .. 1023 doomo_unlocks_fishcook     Lake of Fire + Brimstone Bites (WIP)
 *
 * art, portrait and roster register no unlockables (their tables stay the
 * weak empty defaults); the ranges they were given originally went to
 * minigames + levels, concert and fishcook. A new team gets a block from
 * a "spare" tail above (re-assign it here first).
 *
 * Every unlock_* table is NULL-terminated and has a weak empty default in
 * core/src/stubs.c, so a module that is not linked (yet) costs nothing.
 */
#ifndef DOOMO_UNLOCK_IDS_H
#define DOOMO_UNLOCK_IDS_H

#define UL_IDS_FOUNDATION_FIRST    0
#define UL_IDS_FOUNDATION_LAST   239
#define UL_IDS_STUDIO_FIRST      240
#define UL_IDS_STUDIO_LAST       255
#define UL_IDS_CREATOR_FIRST     256
#define UL_IDS_CREATOR_LAST      319
#define UL_IDS_LIFE_FIRST        320
#define UL_IDS_LIFE_LAST         383
#define UL_IDS_LIBRARY_FIRST     384
#define UL_IDS_LIBRARY_LAST      415
#define UL_IDS_ACTION_FIRST      416
#define UL_IDS_ACTION_LAST       563     /* S0: 564..607 went to home + reserve */
/* --- S0: docs/design/story/SPEC.md R7 --- */
#define UL_IDS_HOME_FIRST        564     /* 20 trophies + 12 raw bits (HF sec.9.6) */
#define UL_IDS_HOME_LAST         595
#define UL_IDS_RESERVE_FIRST     596     /* unassigned on purpose */
#define UL_IDS_RESERVE_LAST      607
/* Inside foundation: reserved for the customisation spec (both candidate
 * designs plan it). Foundation allocates nothing here. */
#define UL_IDS_CUSTOM_FIRST      202
#define UL_IDS_CUSTOM_LAST       239
/* --- end S0 --- */
#define UL_IDS_STORY_FIRST       608
#define UL_IDS_STORY_LAST        735
/* --- S0: the story range's sub-blocks (SPEC R7, C14) ---
 *   608..610  modes (united park, Hellvile visits, NG+)
 *   611..629  jukebox: USTORY_SONG_FIRST + SONG_* - 1 (the 19 old songs)
 *   632..643  gallery: USTORY_CUT_FIRST + CUT_* (the 12 old cutscenes)
 *   644..659  chapter / collection achievements
 *   660..676  side quest rewards (USTORY_REWARD_FIRST + k, 17 ids)
 *   677..688  the 12 new cutscenes: UL_STORY_CUT2(c) = 677 + (c - CUT_P0)
 *   689..697  the 9 new songs:      UL_STORY_SONG2(s) = 689 + (s - SONG_MINGLE)
 *   698..710  misc: 3 title variants, 6 Breach variants, 2 arena modes,
 *             2 creator parts (UL_STORY_MISC2_FIRST + k)
 *   711..735  spare
 * (Trophies are not unlock ids: SPEC R8.) */
#define UL_STORY_CUT2_FIRST      677
#define UL_STORY_CUT2_LAST       688
#define UL_STORY_SONG2_FIRST     689
#define UL_STORY_SONG2_LAST      697
#define UL_STORY_MISC2_FIRST     698
#define UL_STORY_MISC2_LAST      710
#define UL_STORY_SPARE_FIRST     711
#define UL_STORY_CUT2(c)  (UL_STORY_CUT2_FIRST + ((c) - CUT_P0))      /* story.h  */
#define UL_STORY_SONG2(s) (UL_STORY_SONG2_FIRST + ((s) - SONG_MINGLE)) /* audio.h  */
/* --- end S0 --- */
#define UL_IDS_FREEPLAY_FIRST    736
#define UL_IDS_FREEPLAY_LAST     831
#define UL_IDS_MINIGAMES_FIRST   832
#define UL_IDS_MINIGAMES_LAST    863
#define UL_IDS_LEVELS_FIRST      864     /* reserved for the levels team */
#define UL_IDS_LEVELS_LAST       895
#define UL_IDS_CONCERT_FIRST     896
#define UL_IDS_CONCERT_LAST      959
#define UL_IDS_FISHCOOK_FIRST    960     /* reserved for the fishcook team */
#define UL_IDS_FISHCOOK_LAST    1023

/* ---- UL_CAT_MISC list ids --------------------------------------------------
 * For UL_CAT_MISC ("Secrets": trophies, achievements, module-private
 * gates) DoomoUnlock.list is free for the owner, but unlock_find() and
 * unlock_grant_match() look entries up by (cat, list, index), so every
 * team keeps its MISC entries under its own list id(s):
 *
 *   0x00       life             0x4C 'L'   library
 *   0x28..0x2B studio           0x50 'P'   freeplay
 *   0x41 'A'   action           0x53 'S'   story
 *   0x48 'H'   home
 *   0x43 'C'   concert          0x56 'V'   levels (reserved)
 *   0x46 'F'   minigames        0xC0, 0xC1, 0xF1, 0xF2  fishcook
 */
#define UL_MISC_LIFE        0x00
#define UL_MISC_STUDIO      0x28    /* .. 0x2B */
#define UL_MISC_ACTION      0x41
#define UL_MISC_CONCERT     0x43
#define UL_MISC_MINIGAMES   0x46
#define UL_MISC_LIBRARY     0x4C
#define UL_MISC_FREEPLAY    0x50
#define UL_MISC_STORY       0x53
#define UL_MISC_LEVELS      0x56    /* reserved for the levels team */
#define UL_MISC_HOME        0x48    /* 'H' (S0) the dream house's trophies */

/* ---- (cat, list, index) keys ---------------------------------------------
 * Keys are unique, with one exception: a creator / weapon part may be
 * gated by up to UL_ALIAS_MAX entries of different teams ("alternate
 * routes", e.g. Sparkle Eyes: the creator's "create your first citizen"
 * and the concert's "release a hit single"). Owning ANY of them unlocks
 * the part; unlock_find() returns an owned entry when there is one. */
#define UL_ALIAS_MAX 16

/* ---- PROG_* counters -------------------------------------------------------
 * DoomoTown.progress has 32 saved slots and PROG_COUNT is 31 (20 core,
 * 7 minigames, 3 concert, 1 fishcook). The last slot is reserved for the
 * levels team; anything more must live in a save_ext.h chunk. Every
 * counter has one meaning and one owner (test_integration.c checks the
 * names table). */
#define UL_PROG_SLOTS 32

#endif
