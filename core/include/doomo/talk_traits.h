/* talk_traits.h - the trait model of Talk and the personality API (TM1-TRAITS).
 * Owner: TALK. docs/TALK.md "Traits and the personality API". Owner decision
 * 2026-11-15 "TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN".
 *
 * A person's Talk personality is: the five Tomodachi sliders (the persona
 * gene, 16 types, persona_type()), up to THREE traits of the 24 below, and a
 * few liked and disliked topics (roots of the word network). All of it is a
 * TalkTraitSet. The sliders stay in the resident's gene as they are.
 *
 * WHERE IT IS STORED (the memory rule). The 384-bit gene is full (3 spare
 * bits), and every bit of the record costs about 125 B over 1,000 residents,
 * so the traits are NOT a new gene field. A resident's effective set is
 * talk_trait_get(): the creator's explicit choice when there is one (a table
 * of at most TALK_TRAIT_CUSTOM_MAX people, saved in the 'TALK' chunk, native
 * only), else a default that is a pure function of the record (the five
 * sliders, the look seed, the life stage). Premade residents, the family pool,
 * generated residents and every old save therefore have traits at no cost, and
 * the same record gives the same traits on every platform.
 *
 * Plain C99, no allocation, no float, bounded time.
 */
#ifndef DOOMO_TALK_TRAITS_H
#define DOOMO_TALK_TRAITS_H

#include "types.h"
#include "resident.h"

/* ---- the 24 traits (ids are saved: add at the end, never reorder) ---------- */
enum {
    TT_NONE = 0,
    TT_HOTHEADED, TT_ROMANTIC, TT_GOOFBALL, TT_SNOB, TT_NEAT, TT_SLOB, TT_BOOKWORM, TT_CHEERFUL,
    TT_GLOOMY, TT_JEALOUS, TT_LOYAL, TT_MEAN, TT_KIND, TT_LAZY, TT_AMBITIOUS, TT_PARANOID,
    TT_SELF_ASSURED, TT_SHY, TT_OUTGOING, TT_CREATIVE, TT_GLUTTON, TT_MORBID, TT_CURIOUS, TT_HONEST,
    TT_COUNT                                  /* 25 = none + 24 */
};
#define TALK_TRAIT_MAX     3                  /* traits a person has */
#define TALK_TRAIT_TOPICS  2                  /* liked topics, and disliked topics */
#define TALK_TRAIT_ROOT_MAX 15                /* topic roots 1..15 = TCR_PEOPLE .. TCR_ROMANCE */
#define TALK_TRAIT_CUSTOM_MAX 24              /* people with an explicit set (saved) */
#define TALK_PREVIEW_ID    0x7FFE             /* the id of the not-yet-created character in a preview */

typedef struct TalkTraitSet {
    uint8_t trait[TALK_TRAIT_MAX];            /* TT_*, filled from slot 0; 0 = empty            */
    uint8_t like[TALK_TRAIT_TOPICS];          /* topic root 1..15 (TCR_*), 0 = none             */
    uint8_t dislike[TALK_TRAIT_TOPICS];
} TalkTraitSet;                               /* 7 B */

/* validation results */
enum { TTE_OK = 0, TTE_RANGE,                 /* a trait or topic id out of range               */
       TTE_GAP,                               /* an empty slot before a filled one              */
       TTE_DUP,                               /* the same trait or topic twice                  */
       TTE_CONFLICT,                          /* an impossible pair (kind + mean)               */
       TTE_TOPIC_BOTH,                        /* a topic both liked and disliked                */
       TTE_FULL,                              /* three traits already / the custom table is full */
       TTE_NOBODY                             /* no such resident                               */
};

/* ---- names and descriptions (English, for the creator's list) -------------- */
const char *talk_trait_name(int t);           /* "Hot-headed"; "" for TT_NONE / bad              */
const char *talk_trait_desc(int t);           /* one short sentence: how they talk and react     */
const char *talk_trait_adj(int t);            /* "hot-headed", "bookish": for the one-line read  */
const char *talk_trait_topic_name(int root);  /* "food", "Hellvile", ...: for liked / disliked   */
/* 1 for a romance trait (romantic, jealous): inert for a minor (their bias is the empty set's). */
int  talk_trait_is_romance(int t);
/* 1 if the two traits cannot be had together (kind / mean, cheerful / gloomy, ...). */
int  talk_trait_conflict(int a, int b);
/* 1 if trait t has no effect at this life stage (a romance trait on a minor). */
int  talk_trait_inert(int t, int stage);
/* 1 if a topic root may be liked / disliked at this stage (romance: adults only). */
int  talk_trait_topic_ok(int root, int stage);

/* ---- editing a TalkTraitSet (the creator keeps one while the player picks) -- */
void talk_traitset_clear(TalkTraitSet *ts);
int  talk_traitset_count(const TalkTraitSet *ts);
int  talk_traitset_has(const TalkTraitSet *ts, int t);
/* Add a trait: TTE_OK, or TTE_FULL (three), TTE_DUP, TTE_CONFLICT, TTE_RANGE; nothing changes on an error. */
int  talk_traitset_add(TalkTraitSet *ts, int t);
void talk_traitset_remove(TalkTraitSet *ts, int t);     /* closes the gap */
/* Like / dislike a topic root: TTE_OK, TTE_FULL, TTE_DUP, TTE_TOPIC_BOTH, TTE_RANGE. */
int  talk_traitset_like(TalkTraitSet *ts, int root);
int  talk_traitset_dislike(TalkTraitSet *ts, int root);
void talk_traitset_forget_topic(TalkTraitSet *ts, int root);   /* drops it from both lists */
/* 0 = a valid set (TTE_OK), else the first TTE_* problem. `stage` is the life stage (romance topics). */
int  talk_trait_validate(const TalkTraitSet *ts, int stage);
/* Make any set valid (drop bad, duplicate, conflicting entries, close gaps); returns the fixes made. */
int  talk_trait_normalize(TalkTraitSet *ts, int stage);
/* 31 bits: 3 x 5 trait, 2 x 4 like, 2 x 4 dislike (the save format). unpack normalises. */
uint32_t talk_traitset_pack(const TalkTraitSet *ts);
void talk_traitset_unpack(uint32_t v, TalkTraitSet *ts);

/* ---- a person's set ---------------------------------------------------------- */
/* The default set of a record: a pure function of the five sliders, the look seed and the life
 * stage (two traits, one liked and one disliked topic). The same on every platform. Minors never get
 * a romance trait or "mean". */
void talk_trait_default(const DoomoResident *r, TalkTraitSet *out);
/* The effective set of resident, hero, pool member or the preview id: the explicit one if there is one,
 * else the default. Returns 1, or 0 (and an empty set) when nobody has that id. */
int  talk_trait_get(int id, TalkTraitSet *out);
/* The creator / a story sets a person's traits explicitly (normalised; stage-checked). TTE_OK, or an
 * error (TTE_FULL when the custom table is full; TTE_NOBODY). */
int  talk_trait_set(int id, const TalkTraitSet *ts);
int  talk_trait_is_custom(int id);            /* 1 if id has an explicit set */
void talk_trait_clear(int id);                /* back to the default */
void talk_trait_forget(int id);               /* the person left: drop the explicit set (talk_mem_forget calls it) */
int  talk_trait_custom_count(void);
void talk_trait_reset(void);                  /* a new town */

/* ---- the personality screen --------------------------------------------------- */
/* The persona-type title of a record: "Quiet Bookworm". */
const char *talk_trait_title(const DoomoResident *r);
/* The one-line read of a personality: "Quiet Bookworm: shy, kind. Loves food, avoids work." ts NULL
 * means the default set of r. Returns the length written (always NUL ended, cut to cap-1). */
int  talk_trait_read(const DoomoResident *r, const TalkTraitSet *ts, char *out, int cap);

/* ---- what the traits do to Talk ------------------------------------------------ */
/* Leanings of one set on one act about one topic root (TCR_*, 0 for none). All small:
 *   accept 0..4  how much more gladly they take it, deny 0..4  how much more they turn it down,
 *   stance -3..3 added to the stance score on that topic, mood -3..3 how it lands (STAT_HAPPY lean).
 * A minor's romance traits are inert (the empty set's answer). When `minor_room` is 1 (the speaker or
 * anyone addressed is a minor, or a minor is in the room) no trait ever turns a request down harder,
 * stings or makes the speaker cool: deny 0, mood >= 0 (a mean adult is gruff to adults only). */
typedef struct TalkTraitBias { int8_t accept, deny, stance, mood; } TalkTraitBias;
TalkTraitBias talk_trait_bias(const TalkTraitSet *ts, int stage, int act, int topic_root, int minor_room);
/* The same for a person by id (the engine's hook); minor_room is derived from the speaker's stage. */
TalkTraitBias talk_trait_bias_id(int id, int act, int topic_root, int minor_room);
/* Net lean of accept - deny, the number talk_dm adds to its take value. */
int  talk_trait_take(const TalkTraitBias *b);
/* A trait's liking for a topic root only (the stance term): -3..3. */
int  talk_trait_topic_lean(const TalkTraitSet *ts, int stage, int root);

/* ---- the plan's inputs (TM2c; docs/TALK.md 6j) ------------------------------------- */
/* 1 for an edgy trait (hot-headed, romantic, snob, jealous, mean): it has no effect on a person who is
 * not an adult, nor when anyone who is not an adult is in the room. */
int  talk_trait_edgy(int t);
/* The set as it counts in this room: the edgy traits are dropped when `stage` is a minor's or
 * `minor_room` is 1 (a minor speaks, is addressed or listens). The engine reads only this set. */
void talk_trait_room(TalkTraitSet *ts, int stage, int minor_room);
/* The trait deltas to the five stored sliders (PERS_*), summed over the set and capped at +-6 each
 * (twice the table's pull). Added to the stored sliders and clamped 0..15 they are the effective
 * sliders the voice guards read; persona_type() still reads the stored ones. */
void talk_trait_slider_delta(const TalkTraitSet *ts, int8_t out[PERS_COUNT]);

/* ---- the live preview ------------------------------------------------------------- */
enum { TP_GREETING = 0, TP_ANSWER, TP_REFUSAL, TP_KINDS };
/* A line from the REAL Talk engine (talk_gen_line) in the voice of a character that need not exist
 * yet: r holds the sliders (persona), the life stage and the look seed; ts the traits and topics (NULL:
 * the default set). kind: a greeting, an answer (an opinion on a topic they like, else on any topic),
 * a refusal (a no to a topic they dislike, else to an invite). The player is the other speaker. The
 * same arguments give the same line on every platform. No allocation, a bounded scan of at most 512
 * concepts. Returns the length, or 0 when the Talk database is not open (the creator then shows its
 * own text). Needs talk_db_open() / talk_gen_ready(). */
int  talk_preview(const DoomoResident *r, const TalkTraitSet *ts, int kind, uint32_t seed, char *out, int cap);

/* The same, as the engine's TalkLine (text, reply act, face): TM2c. The plan takes a forced kind (a greeting
 * accepts, an answer agrees or denies by the stance, a refusal says no), so a refusal preview really is a
 * "no" (line.act == TA_NO when the bank has one for this voice). Returns the text length, 0 on nothing. */
int  talk_preview_line(const DoomoResident *r, const TalkTraitSet *ts, int kind, uint32_t seed, TalkLine *out);

/* ---- the save chunk's tail (talk_save.c; native only) ----------------------------------- */
uint32_t talk_trait_save_size(void);                       /* 0, or 1 + 6 x custom_count */
uint32_t talk_trait_save_write(uint8_t *dst, uint32_t cap);
int  talk_trait_save_read(const uint8_t *src, uint32_t len);   /* 1 well formed; drops the gone and bad */

/* ---- engine hooks (talk_gen.c, talk_safe.c; not for the creator) -------------------- */
/* While a preview runs, the resident the id TALK_PREVIEW_ID stands for (else NULL), and its stage. */
const DoomoResident *talk_preview_resident(void);
int  talk_preview_stage(void);

#endif
