/* family.h - the household: who lives with the player (owner: life; F1 part 1,
 * LIFESIM_PLAN R4, R12, R13 + section 6 AMENDMENTS; add-only after F1).
 *
 * This is the data model only: the table, the cap by tier, the rules for
 * who may join and in what role, the 'LIFE' chunk's family section and the
 * strong versions of life.h's family_* / life_* functions. The dashboard
 * and the add-member wizard are F2; babies, ageing and family events are
 * F4; pets' uplift is F5; loss is F6. They add rows and flags here.
 *
 * A member is either a resident (id 0..DOOMO_MAX_RESIDENTS-1, someone who
 * moved in) or a family pool member (PLAYAS_ID_FAMILY_FIRST.., 1001..1020:
 * a person or pet who exists only in the household; the GBA's pool is its
 * first 10 ids). A pool member is a small identity (FamilyIdentity: a look
 * seed, a name, a pronoun and who they can fall for, 20 B), not a 96 B
 * DoomoResident: family_resident() builds the full resident on demand, the
 * same on every platform (resident_generate), so a household of 20 costs
 * 400 B of RAM and 440 B of save. Roles are
 * chosen by the player, Sims style (any person may be a child, a parent, a
 * sibling or a housemate), with two rules that are not a choice: a PARTNER
 * must pass the life stage gate of LS0 (adults only, both ways, never
 * kin; a minor is never a partner) and there is at most one.
 *
 * The cap counts members, not the player, and pets count: 3 at first, then
 * 10 (GBA 6) after the game is completed and 20 (GBA 10) after New Game+
 * (life_contract.h HOUSEHOLD_CAP, unlock ids UL_LIFE_HOUSEHOLD_T1 / T2).
 * The GBA table is sized for its own cap (10), the others for 20.
 *
 * Nothing is written to the save while the household is empty, so an
 * island without a family loads and saves byte for byte as before.
 */
#ifndef DOOMO_FAMILY_H
#define DOOMO_FAMILY_H

#include "types.h"
#include "life.h"
#include "life_contract.h"

/* Roles (saved; add-only). */
enum {
    FAMROLE_NONE = 0,
    FAMROLE_PARTNER,        /* the life partner (at most one; adults only)   */
    FAMROLE_CHILD,
    FAMROLE_PARENT,
    FAMROLE_SIBLING,
    FAMROLE_GRANDPARENT,
    FAMROLE_HOUSEMATE,      /* a friend who lives with you                   */
    FAMROLE_PET,            /* a pet (pool ids only); counts toward the cap  */
    FAMROLE_COUNT
};

/* Results of family_add() / family_set_role() (0 or a member's slot is OK). */
enum {
    FAM_ERR_FULL = -1,      /* the household is at its cap                   */
    FAM_ERR_DUP = -2,       /* already a member                              */
    FAM_ERR_ID = -3,        /* no such resident / pool id, or not alive      */
    FAM_ERR_ROLE = -4,      /* bad role, a pet that is not a pool member,    */
                            /* or a second partner                           */
    FAM_ERR_GATE = -5,      /* the partner failed the LS0 life stage gate    */
    FAM_ERR_NAME = -6,      /* an empty, too long or undrawable name         */
    FAM_ERR_NOPARTNER = -7  /* a birth needs the player's life partner (F4)  */
};

typedef struct FamilyMember {
    uint16_t id;        /* resident id, or pool id 1001..1020                */
    uint8_t  role;      /* FAMROLE_*                                         */
    uint8_t  stage;     /* LIFE_* (a resident's is read from the sim when
                           the member is added)                              */
    uint8_t  flags;     /* reserved (F4 / F5 / F6), 0                        */
    uint8_t  spare;     /* 0                                                 */
    uint16_t since;     /* town day when the member joined (saturates)       */
} FamilyMember;         /* 8 bytes, as saved */

/* A pool member's identity (saved in the POOL section; add-only). */
typedef struct FamilyIdentity {
    uint32_t seed;                  /* resident_generate() seed: the look and the rest */
    char     name[DOOMO_NAME_LEN];  /* 1..11 printable ASCII chars, NUL padded         */
    uint8_t  pronoun;               /* PRONOUN_*                                       */
    uint8_t  romance;               /* ROMANCE_* (who they can fall for)               */
    uint8_t  spare[2];              /* 0                                               */
} FamilyIdentity;                   /* 20 bytes */

#define FAMILY_SEC_HEAD 2u          /* the sections: count u8, reserved u8    */
#define FAMILY_SZ_WORST (FAMILY_SEC_HEAD + 8u * HOUSEHOLD_CAP_MAX)
#define FAMILY_POOL_REC (2u + 20u)  /* pool section: id u16 + an identity      */
#define FAMILY_POOL_SZ_WORST (FAMILY_SEC_HEAD + FAMILY_POOL_REC * HOUSEHOLD_CAP_MAX)
#ifdef DOOMO_PLATFORM_GBA
#define FAMILY_MAX HOUSEHOLD_CAP_SMALL_T2
#else
#define FAMILY_MAX HOUSEHOLD_CAP_FULL_T2
#endif
/* The pool ids this platform has: FIRST .. FIRST + FAMILY_MAX - 1. */
#define FAMILY_POOL_FIRST (DOOMO_PLAYER_ID + 1)
#define FAMILY_POOL_LAST  (DOOMO_PLAYER_ID + FAMILY_MAX)

/* The cap of the household now (tier from the unlocks; the GBA's smaller
 * numbers on the GBA). */
int  family_cap(void);
/* The household tiers (S-HOUSE): 0 at the start, 1 once the story is
 * finished (UL_LIFE_HOUSEHOLD_T1), 2 once it is finished again in New Game+
 * (UL_LIFE_HOUSEHOLD_T2). The tier ids have no unlock table entry (K0's
 * contract), so these grant them quietly and show their own toast. */
int  family_tier(void);
/* The story's credits just rolled (once per run): the first finish grants
 * tier 1, a later finish (unlocks outlive a new story) tier 2. Returns the
 * tier newly reached, 0 if none. */
int  family_tier_story_done(void);
/* Catch-up when a story save is loaded: a story that was finished before
 * the tiers existed gets tier 1. Never grants tier 2. Returns 1 if granted. */
int  family_tier_sync(int story_done);
/* Is id a household id at all (a resident slot or a pool id)? */
int  family_id_ok(int id);
/* Add a member. stage is only read for a pool member (a resident's comes
 * from the sim). Returns the slot (>= 0) or FAM_ERR_*. */
int  family_add(int id, int role, int stage);
/* Remove a member; 1 if it was one. */
int  family_remove(int id);
/* Change a member's role under the same rules as family_add; 0 or FAM_ERR_*. */
int  family_set_role(int id, int role);
/* The member record of id, or NULL. */
const FamilyMember *family_find(int id);
/* A member's role, FAMROLE_NONE if id is not a member. */
int  family_role(int id);
/* How many members have this role. */
int  family_count_role(int role);
/* A new pool member in one step: the first free pool id, with this look
 * seed (resident_generate), name (NULL = the generated one) and pronoun
 * (-1 = the generated one). Returns the pool id or FAM_ERR_* (nothing is
 * changed on an error). family_add() on a pool id does the same with a seed
 * made from the town seed and the id. */
int  family_pool_add(int role, int stage, uint32_t seed, const char *name, int pronoun);
/* The identity of a pool member (always filled in), NULL for anyone else. */
const FamilyIdentity *family_identity(int id);
/* Rename a pool member (FAM_ERR_NAME / FAM_ERR_ID); 0 on success. */
int  family_set_name(int id, const char *name);
/* The full resident of a member: a copy of the town's resident, or for a
 * pool person one built from the identity (alive, with the member's life
 * stage). 1 on success; 0 for a non-member and for a pet (F5 draws pets). */
int  family_resident(int id, DoomoResident *out);
/* The first free pool id (1001..), or -1 when every pool id in range is used. */
int  family_pool_alloc(void);
/* Forget the household (a new town does this through the LIFE chunk). */
void family_clear(void);

/* The LIFE chunk's family section (life.c's table). */
uint32_t family_save_size(void);
uint32_t family_save(uint8_t *dst, uint32_t cap);
int      family_load(const uint8_t *src, uint32_t len);
/* ... and the POOL section: the identities of the pool members (life.c). A
 * member whose identity is not in the save gets the default one. */
uint32_t family_pool_save_size(void);
uint32_t family_pool_save(uint8_t *dst, uint32_t cap);
int      family_pool_load(const uint8_t *src, uint32_t len);
void     family_pool_reset(void);

/* ---- F4 part 1: the life cycle ------------------------------------------
 * A pool member grows up with the town's days: BABY -> TODDLER -> CHILD ->
 * TEEN -> ADULT (life_stage_next), one step per town day counted in the low
 * 5 bits of FamilyMember.flags (FAMILY_FL_AGE; reset to 0 on every stage
 * change, so an old save's 0 means "just arrived"). An ADULT or ELDER does
 * not age here (growing old is F6's). A resident member's stage is the
 * sim's: it is copied over, and the change is reported like a pool one. A
 * pet has no life stage and is skipped. Roles never change by themselves:
 * a CHILD who grows up is still the family's child. */
#define FAMILY_FL_AGE 0x1Fu
/* Days a stage lasts here (0 = it does not end). */
int  family_stage_days(int stage);
/* Once per town day (sim.c's day_rollover). Returns how many members
 * changed stage today; the list below stays valid until the next call. */
int  family_age_daily(void);
int  family_aged_count(void);
int  family_aged_id(int n);        /* the n-th member that changed stage, or -1 */
int  family_aged_stage(int n);     /* the stage it grew into, or -1             */
/* Days a member has spent in its present stage (0 for a non-member). */
int  family_stage_progress(int id);

/* ---- F4 part 2: births and the growing-up toast ----------------------------
 * family_age_daily() shows one toast per member that grew up ("Mo grew up!
 * Now: Toddler"). A birth is a new BABY pool member with the role CHILD:
 * the player must be an adult with a life partner (FAMROLE_PARTNER, so the
 * LS0 gate already held), and the household needs room (family_cap(); a
 * pet counts as a member). name NULL = the generated one, pronoun -1 = the
 * generated one. Returns the pool id or FAM_ERR_* (nothing is changed on an
 * error); shows its own toast. Adopting stays family_pool_add() with any
 * stage and the CHILD role. */
int  family_birth(const char *name, int pronoun);

/* ---- F6: loss ---------------------------------------------------------------
 * A pool member (a person or a pet) can be lost; a resident member is the
 * sim's, so family_lose() only takes them out of the household. Rules
 * (SCRIPT_STYLE R5, R6: a child is never shown dying, grief is not a joke):
 *   - NEGLECT: a pool member nobody cared for (family_care) for
 *     FAMILY_NEGLECT_LOSS town days is lost; a warning toast comes at
 *     FAMILY_NEGLECT_WARN. The count is the top 3 bits of FamilyMember.flags.
 *     A MINOR is never lost this way: the count stops at FAMILY_NEGLECT_WARN + 2
 *     and they stay sad.
 *   - OLD AGE: a pool ELDER passes peacefully after FAMILY_ELDER_DAYS town
 *     days in the household (counted in the age bits). Pets and minors do not.
 *   - family_loss_set(0) turns loss off for the daily passes (the "Loss"
 *     option U0 will add); an explicit family_lose() still works, but a minor
 *     can only "move away" (FAMLOSS_GONE), never die.
 * Every loss leaves a memorial (8 B, the newest FAMILY_MEMORIAL_MAX kept) for
 * the graveyard screen and the endings. A saved memorial keeps no name: a
 * resident's comes from the town, a pool member's is regenerated when it was
 * never renamed (FAMILY_MEM_DEFNAME); otherwise family_memorial_name()
 * returns 0 and the screen says "your child", "your pet", ... */
enum { FAMLOSS_AGE = 1, FAMLOSS_ILLNESS, FAMLOSS_NEGLECT, FAMLOSS_DEMON, FAMLOSS_GONE,
       FAMLOSS_COUNT };
#define FAMILY_FL_NEGLECT       0xE0u
#define FAMILY_NEGLECT_SHIFT    5
#define FAMILY_NEGLECT_WARN     4
#define FAMILY_NEGLECT_LOSS     7
#define FAMILY_ELDER_DAYS       31
#define FAMILY_MEMORIAL_MAX     18
#define FAMILY_MEM_POOL         0x40u   /* bits: stage 0-2, role 3-5, pool, default name */
#define FAMILY_MEM_DEFNAME      0x80u
typedef struct FamilyMemorial {
    uint16_t id;        /* resident id or pool id                           */
    uint16_t day;       /* town day of the loss (saturates)                 */
    uint8_t  cause;     /* FAMLOSS_*                                        */
    uint8_t  bits;      /* stage | role << 3 | FAMILY_MEM_*                 */
    uint8_t  spare[2];  /* 0                                                */
} FamilyMemorial;       /* 8 bytes, as saved */
#define FAMILY_LOSS_SZ_WORST (2u + 8u * FAMILY_MEMORIAL_MAX)

void family_loss_set(int on);
int  family_loss_enabled(void);
/* Lose a member now (an event, a script, the daily passes). Shows a toast
 * and leaves a memorial. 1 if it happened, 0 for a non-member or a refused
 * cause (see above). */
int  family_lose(int id, int cause);
/* Someone cared for this member today (feed, play, talk): the count resets. */
void family_care(int id);
/* Once per town day, after family_age_daily(): neglect and old age. Returns
 * how many members were lost. */
int  family_loss_daily(void);
/* The memorials, newest first. */
int  family_memorial_count(void);
const FamilyMemorial *family_memorial(int n);
int  family_memorial_stage(int n);
int  family_memorial_role(int n);
/* Copy the name (DOOMO_NAME_LEN bytes) of the n-th memorial into out; 0 when
 * it is not known. */
int  family_memorial_name(int n, char *out);
/* The LIFE chunk's LOSS section (life.c's table). */
uint32_t family_loss_save_size(void);
uint32_t family_loss_save(uint8_t *dst, uint32_t cap);
int      family_loss_load(const uint8_t *src, uint32_t len);
void     family_loss_clear(void);

#endif
