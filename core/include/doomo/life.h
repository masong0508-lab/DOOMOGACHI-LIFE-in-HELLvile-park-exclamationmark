/* life.h - the life sim's saved state and the family / endings contract
 * other areas read (owner: life; LIFESIM_PLAN LS1, R4, R8, R12, R14).
 *
 * The 'LIFE' save chunk (save_ext.h SAVE_EXT_TAG_LIFE, core/src/life.c) is
 * a list of sections, [id u8][version u8][length u16 LE][bytes], so later
 * packages add a section (a row in life.c's table) without a new chunk or
 * a chunk version bump. On load an unknown section is skipped and a
 * missing one is reset; a new town resets them all. The sizes stay within
 * life_contract.h (LIFE_SZ_*) and save_budget.h SAVE_EXT_MAX_LIFE.
 *
 * The family / endings functions below are weak defaults in
 * core/src/weak_life.c ("no family yet") until their owners (F1, LS5)
 * define the strong ones, so play-as (PA5) and endings (E1) can call them
 * from the start.
 */
#ifndef DOOMO_LIFE_H
#define DOOMO_LIFE_H

#include "types.h"
#include "save_ext.h"

#define LIFE_CHUNK_VERSION 1
/* Section ids (add-only; saved). */
enum {
    LIFE_SEC_NEWS = 1,      /* LS1: the news feed (sim.h DoomoNewsRec)     */
    LIFE_SEC_EVENTS = 2,    /* LS3: life event state                       */
    LIFE_SEC_FAMILY = 3,    /* F1: the household                           */
    LIFE_SEC_RESERVED = 4,  /* R8: 8 B kept for the play-as character;
                               never written (PLAY owns the avatar)        */
    LIFE_SEC_PETS = 5,      /* F5                                          */
    LIFE_SEC_LOSS = 6,      /* F6                                          */
    LIFE_SEC_TIER = 7       /* the household tier (F1-F4)                  */
};
#define LIFE_SEC_HDR 4u
/* The news section's worst case: 4 B header, NEWS_MAX records, NEWS_FREE
 * lines of at most NEWS_LEN - 1 bytes + a length byte. */
#define LIFE_SZ_NEWS (4u + 32u * 8u + 2u * 64u)

extern const DoomoSaveExt *const doomo_save_ext_life;

/* ---- weak until F1 / LS5 (core/src/weak_life.c) ---------------------- */
/* Household members (F1): count and the i-th member's resident or family
 * pool id (1001..1020), or -1. */
int family_count(void);
int family_member(int i);
/* The player's life partner (spouse or partner; R12), or -1. */
int life_player_partner(void);
/* Household size including the player (0 = no household yet). */
int life_family_count(void);

/* Facts the ending resolver reads (R14; LS5 fills them). */
typedef struct LifeEndingFacts {
    int16_t  partner;          /* life partner id or -1                    */
    uint8_t  partner_adult;    /* 1 only if both are adults (LS0 gate)     */
    uint8_t  household;        /* members, the player included             */
    uint8_t  kids;
    uint8_t  best_friends;     /* the player's best friends                */
    uint8_t  couples;          /* couples the player helped match          */
    uint8_t  exorcisms_won;
    uint8_t  neglect_deaths;
    uint8_t  happiness;        /* island happiness 0..100                  */
    uint8_t  reserved[6];
} LifeEndingFacts;
void life_ending_facts(LifeEndingFacts *out);

#endif
