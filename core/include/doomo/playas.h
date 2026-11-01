/* playas.h - play-as: who the player controls, and which hero the story
 * belongs to (owner: playas; contract landed by PA0, LIFESIM_PLAN R8, R9,
 * R13 + section 6 AMENDMENTS; add-only after PA0).
 *
 * After the story is cleared (and in Free play after the game-completion
 * unlock) the player can switch who they control at any time: the hero,
 * any living or ghost resident, any family member (people, and a pet only
 * once it was uplifted, F5). The story stays the hero's: while someone
 * else is played, story writes are refused (playas_story_locked(), PA1's
 * barrier). The hero is SUNNY (she), MARVIN (he, a fox) or one you make
 * yourself (PA2); more hero kinds are reserved for the co-op heroes of
 * extra player profiles (MP1).
 *
 * Until PA1 (playas.c) and PA2 (hero.c) land, every function is a weak
 * default in core/src/weak_playas.c: the avatar is the hero
 * (DOOMO_PLAYER_ID), nothing is locked, play-as is not available, and save
 * files stay byte-identical. The getters are quiet; only the mutators log
 * "STUB: playas_<name>", so a runner script with --assert-no-stubs
 * notices a caller that came too early.
 *
 * Every id below is a play-as id: 0..999 a resident slot, DOOMO_PLAYER_ID
 * (1000) the hero, PLAYAS_ID_FAMILY_FIRST..LAST (1001..1020) a family pool
 * member (family_member() in life.h, F1; R13). The PLAY chunk stores ids
 * as u16. The call-site table (BODY / ACTOR / HERO sites), the lock list,
 * the PLAY chunk layout and the header byte are in docs/PLAYAS.md.
 */
#ifndef DOOMO_PLAYAS_H
#define DOOMO_PLAYAS_H

#include "types.h"
#include "town.h"
#include "life_contract.h"

/* ---- ids ----------------------------------------------------------------- */
#define PLAYAS_ID_HERO          DOOMO_PLAYER_ID                  /* 1000 */
#define PLAYAS_ID_FAMILY_FIRST  (DOOMO_PLAYER_ID + 1)            /* 1001 */
#define PLAYAS_ID_FAMILY_LAST   (DOOMO_PLAYER_ID + HOUSEHOLD_CAP_MAX)  /* 1020 */
#define PLAYAS_ID_NONE          (-1)

/* ---- hero kinds (PA2; 3 bits: the header byte and the PROF records) -------
 * HERO_CUSTOM is "make your own" and every save made before PA2. The co-op
 * block is reserved for the extra heroes of guest profiles (MP1, DECISIONS
 * "EXTRA HEROES"): each gets one id here and its own preset, side arc and
 * abilities; the main hero (profile 0) is always CUSTOM, SUNNY or MARVIN and
 * drives the plot flags. */
enum {
    HERO_CUSTOM = 0,        /* make your own (the creator's arg -3)          */
    HERO_SUNNY  = 1,        /* female, PRONOUN_SHE (the show's lead)         */
    HERO_MARVIN = 2,        /* male, PRONOUN_HE, a fox                       */
    HERO_COOP_FIRST = 3,    /* reserved: co-op heroes of guest profiles      */
    HERO_COOP_LAST  = 7,
    HERO_KIND_COUNT = 8
};
#define HERO_IS_MAIN(k) ((unsigned)(k) <= HERO_MARVIN)
#define HERO_IS_COOP(k) ((k) >= HERO_COOP_FIRST && (k) <= HERO_COOP_LAST)

/* ---- grammatical gender for text (PA3a's @( m | f | n ) codes) ------------
 * From the resident's pronoun: SHE -> F, HE -> M, everything else
 * (they, it, xe) -> N. English text never genders the hero; translations
 * pick a variant with this. */
enum { GENDER_N = 0, GENDER_F = 1, GENDER_M = 2, GENDER_COUNT };

/* ---- how a switch is shown (PA4 presents it) ------------------------------ */
enum {
    PLAYAS_SWITCH_TAGIN = 0,  /* the target walks in / faces you, camera pans */
    PLAYAS_SWITCH_FADE,       /* another map, or an eject: a short fade        */
    PLAYAS_SWITCH_INSTANT     /* load, tests, the start-mode switcher          */
};

/* ---- the save header's byte 47 (R9: PA0 owns it) --------------------------
 * The 48-byte header (save.c) keeps the name in bytes 36..46 (11 chars), so
 * byte 47 was always 0. It is now a summary for the title and the load
 * screen, read without decoding the save (DoomoSaveInfo.flags):
 *   bit 0    the story was cleared in this save (chapter DONE + credits)
 *   bit 1    the avatar is not the hero (playing as someone else)
 *   bit 2    a New life save (PA6: started as a villager after the story)
 *   bit 3    guest player profiles live in this world (MP0, PROF chunk)
 *   bits 4-6 the hero kind (HERO_*), so the menus can say Sunny / Marvin
 *   bit 7    reserved (0)
 * It is only a summary: loading never trusts it (the PLAY / PROF chunks and
 * STRY are the truth). Old saves have 0: "custom hero, nothing unlocked". */
#define PLAYAS_HF_STORY_CLEARED 0x01u
#define PLAYAS_HF_PLAYING_OTHER 0x02u
#define PLAYAS_HF_NEW_LIFE      0x04u
#define PLAYAS_HF_PROFILES      0x08u
#define PLAYAS_HF_HERO_SHIFT    4
#define PLAYAS_HF_HERO_MASK     0x70u
#define PLAYAS_HF_RESERVED      0x80u
#define PLAYAS_HF_HERO(flags)   ((int)(((flags) & PLAYAS_HF_HERO_MASK) >> PLAYAS_HF_HERO_SHIFT))
#define PLAYAS_HF_MAKE_HERO(k)  ((uint8_t)(((unsigned)(k) << PLAYAS_HF_HERO_SHIFT) & PLAYAS_HF_HERO_MASK))

/* ---- player profiles (MP0 / MP1 reserve) -----------------------------------
 * Profile 0 is the town's representative: the hero, whose avatar the
 * functions above report. Guests 1..PROF_GUESTS (life_contract.h) are other
 * people living in the same world save, each playing a villager or a co-op
 * hero; their records are in the PROF chunk. */
#define PLAYAS_PROFILES_MAX PROF_PROFILES_MAX

/* Where the hero was left when you switched away (PA1 records it, PA4
 * spawns the hero NPC there). */
typedef struct PlayasPos {
    int16_t x, y;            /* overworld pixels                              */
    uint8_t map;             /* overworld map id                              */
    uint8_t dir;             /* facing                                        */
} PlayasPos;

/* ---- queries (quiet) ------------------------------------------------------ */
int  playas_avatar(void);          /* the id you control (PLAYAS_ID_HERO by default) */
int  playas_is_hero(void);         /* 1 while you play the hero                       */
int  playas_story_locked(void);    /* 1 while story writes are refused (not the hero) */
int  playas_available(void);       /* the post-game unlock (story cleared, or a New
                                      life save, or Free play after completion)       */
int  playas_can_be(int id);        /* id may be switched to now (PA1 rules)           */
int  playas_gender(int id);        /* GENDER_* of id (from its pronoun); N if none    */
int  playas_you_gender(void);      /* GENDER_* of whoever "you" is: the avatar        */
int  playas_hero_kind(void);       /* HERO_* of this save's hero                       */
/* the i-th most recently played id (0 = the newest), PLAYAS_ID_NONE past the end */
int  playas_recent(int i);
/* 1 and *out = where the hero was left, 0 when the hero is the avatar */
int  playas_hero_pos(PlayasPos *out);
/* the header byte 47 for the save being written (PLAYAS_HF_*) */
uint8_t playas_profile_flags(void);
/* The preset resident of a hero kind (PA2's hero.c), NULL for HERO_CUSTOM,
 * an unfilled co-op id or before PA2. */
const DoomoResident *hero_preset(int kind);

/* The play-as romance gate (PA9): sim_romance_ok (both directions, LS0
 * stages) + the hero rule + kid / teen refusal. Until PA9 it is exactly
 * sim_romance_ok(from, to). Every romance hook touching the hero or the
 * avatar calls this. */
int  playas_romance_ok(int from, int to);

/* Profiles (MP0): the active profile (0 = the hero), the id a profile plays
 * (PLAYAS_ID_NONE for an empty one) and its hero kind (-1 if none). */
int  playas_profile(void);
int  playas_profile_avatar(int profile);
int  playas_profile_hero(int profile);

/* ---- mutators (log "STUB: playas_<name>" until PA1) ----------------------- */
/* Switch to id; how = PLAYAS_SWITCH_*. 1 on success; 0 (nothing changed)
 * if play-as is unavailable or playas_can_be(id) fails. */
int  playas_switch(int id, int how);
/* The new-game hero choice (PA2 calls it once, before the story starts). */
void playas_set_hero_kind(int kind);

#endif
