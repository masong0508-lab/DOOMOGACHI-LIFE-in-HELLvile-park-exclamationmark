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
 *     option U0 will add); an explicit family_lose() still works for adults
 *     and pets. A MINOR (amended by the owner, F10) can be lost to ILLNESS or
 *     DEMON (a disaster, an illness, the story), never to old age or neglect,
 *     and only while Loss is on; with Loss off they are safe (a rescue).
 *     Always off-screen and gentle: a toast and a memorial, no detail.
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

/* ---- F7: free will ---------------------------------------------------------
 * While you are not controlling them, household members look after each other:
 * once per town day a capable member (teen, adult or elder, not the avatar)
 * cares for whoever needs it most. Levels: 0 Off, 1 Min, 2 Normal (default),
 * 3 High, 4 Subconscious (the most, and silent: no news line). Called once a
 * day from sim.c, before family_loss_daily(). Returns the acts done.
 * Saved in the family section's spare header byte (0 = default Normal). */
#define FAMILY_WILL_OFF 0
#define FAMILY_WILL_MIN 1
#define FAMILY_WILL_NORMAL 2
#define FAMILY_WILL_HIGH 3
#define FAMILY_WILL_SUBCONSCIOUS 4
int  family_freewill_level(void);
void family_freewill_set(int level);       /* clamped; saved with the household */
int  family_freewill_daily(void);

/* ---- grief: losing your life partner -----------------------------------------
 * After a PARTNER is lost (any cause but FAMLOSS_GONE) you grieve for
 * FAMILY_GRIEF_DAYS town days. While grieving your shots do only
 * family_grief_damage_pct() percent damage (50 at first, easing back to 100) and
 * hits on you do family_grief_hurt_pct() percent (150 at first). Derived from
 * the memorials, so nothing new is saved. 100 = no effect. */
#define FAMILY_GRIEF_DAYS 14
/* ---- F9: household helpers (Phase 4, a bonus; nothing saved) ---------------------
 * A member with bond 5+ (not the avatar, not a pet) may lend a hand for the day:
 * an adult, elder or teen brings home some Hell-Bucks, a younger child leaves a
 * small gift in your bag (cupcake, flowers or plushie); babies and toddlers only
 * give a smile (news line). Never required by the story. Not when free will is
 * Off; silent at Subconscious. In a STORY island the daily pass waits until the
 * ending was seen (STORY_CH_DONE); free play has no such wait. Returns 1 if someone helped. family_help_do()
 * is the one act (used by the daily pass and the tests): 0 = none, 1 = money,
 * 2 = gift, 3 = a smile. */
int  family_help_do(int id);
int  family_help_daily(void);
/* Away helping (Phase 4): the daily pass sends an adult, elder or teen (bond 5+) out for a day;
 * the next town day they come home with H$ or a small find (flowers, scrap, gears, glitter, a
 * blood orange). One trip at a time, nothing saved (a load cancels it). Children, babies and
 * toddlers never leave. family_away_id() = who is away, -1 = nobody. */
int  family_away_id(void);
int  family_away_start(int id);     /* 1 = they left */
int  family_away_return(void);      /* 1 = they brought something home */

/* ---- F8: bond-gated events (nothing saved) --------------------------------------
 * Run daily after free will (not when it is Off; silent at Subconscious). A member
 * you are close to (bond 10+) may CONFIDE in you: a kind news line and a mood lift.
 * Two non-pet members (not the avatar) may SQUABBLE: both moods drop, and the next
 * day they MAKE UP (moods lift, bond between you and each is untouched). Kid-safe:
 * words only. Returns events shown. */
int  family_squabble_start(int id_a, int id_b);   /* 1 if started (none pending) */
int  family_squabble_pending(void);               /* 1 while one waits to be made up */
int  family_events_daily(void);

/* ---- F10: random disasters (Phase 5, self-contained, nothing saved) -------------
 * Once a day (Loss on) there is a small chance of a disaster aimed at one member
 * who is not the avatar: an illness wave or a demon night (can take the member),
 * or a storm, fire or power cut (only frighten and hurt: mood drops). A warning
 * toast and news line come first; the outcome lands the NEXT town day, unless
 * someone cared for the target in between (you, or a housemate under free
 * will): then they are saved. Adults, elders and pets can be lost; a minor only
 * ever to illness or Hell, and only one time in three (otherwise they recover),
 * always off-screen via family_lose(). Loss off: no disasters at all. A pending
 * disaster is dropped by a load or family_clear (the warning day is the player's
 * one chance, and a load gives a fresh one). */
enum { FAMDIS_NONE = 0, FAMDIS_ILLNESS, FAMDIS_DEMON, FAMDIS_STORM, FAMDIS_FIRE, FAMDIS_POWER,
       FAMDIS_COUNT };
int  family_disaster_start(int kind, int id);   /* 1 if started (a member, none pending) */
int  family_disaster_pending(void);             /* the FAMDIS_* kind, 0 = none */
int  family_disaster_target(void);              /* member id, -1 = none */
int  family_disaster_daily(void);               /* resolve a pending one or roll a new one;
                                                   returns members lost */

/* ---- F7/F8: bond and mood (one byte per member, in table order) -------------
 * low nibble bond 0..15 (default 3), high nibble mood 0..15 (default 8). A care
 * act (yours or a housemate's) raises the bond; days nobody cares for a pool
 * member wear it down slowly (only while free will is not Off). Saved in
 * LIFE_SEC_BOND (length = member count; omitted while everyone is default). */
#define FAMILY_BOND_MAX 15
int  family_bond(int id);                  /* 0..15; 0 for a non-member */
int  family_mood(int id);                  /* 0..15 */
void family_mood_add(int id, int delta);   /* clamped 0..15 (LS3 events); no-op for a non-member */
const char *family_name(int id);           /* the member's name, "" if none (LS3 news lines) */
void family_bond_add(int id, int delta);   /* clamped */
const char *family_bond_line(int id);      /* "Close to you" / "Wants more time with you" / NULL */
int  family_close_id(int id);              /* the housemate this member is closest to, or -1 (derived, not saved) */
const char *family_close_line(int id);     /* "Close to Pip" or NULL (static buffer) */
uint32_t family_bond_save_size(void);
uint32_t family_bond_save(uint8_t *dst, uint32_t cap);
int      family_bond_load(const uint8_t *src, uint32_t len);
void     family_bond_clear(void);
#define FAMILY_BOND_SZ_WORST 20u

int  family_grief_days_left(void);        /* 0 = not grieving */
int  family_grief_damage_pct(void);
int  family_grief_hurt_pct(void);

#endif
