/* talk.h - Talk, the live house conversation (owner: TK; docs/TALK.md).
 *
 * CONTRACT (T0; add-only after T0). Talk is the Facade-style conversation
 * mode of the house (HM_TALK, SCENE_HOME) on the PS1 and the Dreamcast
 * (PLATF_TALK, plat_ext.h). Native runs it for tests; the GBA never sets
 * the bit and links none of it.
 *
 * What lands where (docs/TALK.md "Packages"):
 *   T0  this header, PLATF_TALK, DoomoRoomView.focus_* + zoom 2,
 *       plat_room_project, the 'TALK' save chunk's tag and budget, the
 *       NT_TALK_* news ids, core/src/weak_talk.c
 *   T1  the word network (core/data/talk/), the compiler
 *       core/tools/talkc.py, TALK_<lang>.BIN, core/gen/talk_fixture.c
 *   T2  the database reader core/src/talk_db.c (the talk_db_* below)
 *   T3+ parser, safety gate, generator, drama manager, the mode itself
 *
 * SAFETY (docs/TALK.md "Safety"): nothing generated is reachable in play
 * before the T4 gate and its property tests are green. Talk has no list of
 * minors of its own: it asks sim_life_stage(), the roster traits and
 * ending_kid_cast() (talk_safe_ctx, T4), and every romance act goes
 * through sim_romance_ok() and sim_confess() (sim.h).
 */
#ifndef DOOMO_TALK_H
#define DOOMO_TALK_H

#include "types.h"

/* ---- discourse acts (language independent; add-only, never reorder) ------
 * The parser (DC, native) and the PS1 composer both produce these; the
 * drama manager never sees raw text. TA_NONE fills an unused act slot. */
enum {
    TA_NONE = 0,
    /* openers and closers */
    TA_GREET, TA_FAREWELL, TA_THANK, TA_APOLOGIZE, TA_INTRODUCE,
    /* questions */
    TA_ASK_ABOUT, TA_ASK_OPINION, TA_ASK_FEELING, TA_ASK_FACT, TA_ASK_WHO,
    TA_ASK_ADVICE, TA_ASK_FAVOR, TA_ASK_REPEAT,
    /* statements */
    TA_TELL, TA_OPINE_POS, TA_OPINE_NEG, TA_AGREE, TA_DISAGREE, TA_ANSWER,
    TA_YES, TA_NO,
    /* warm */
    TA_COMPLIMENT, TA_PRAISE_OTHER, TA_COMFORT, TA_ENCOURAGE, TA_SYMPATHIZE,
    TA_JOKE, TA_LAUGH, TA_OFFER, TA_INVITE, TA_PROMISE,
    /* cool */
    TA_INSULT, TA_CRITICIZE, TA_TEASE, TA_MOCK, TA_COMPLAIN, TA_BLAME,
    TA_DISMISS, TA_THREATEN,
    /* third party */
    TA_GOSSIP, TA_DEFEND, TA_ACCUSE, TA_TAKE_SIDE, TA_MEDIATE,
    /* flow */
    TA_CHANGE_TOPIC, TA_CONTINUE, TA_BACKCHANNEL, TA_UNKNOWN,
    /* romance: gated (adults only, sim_romance_ok, no minor or pet in the
     * room; docs/TALK.md K-4). The composer never lists them otherwise. */
    TA_FLIRT, TA_CONFESS, TA_ASK_OUT, TA_KISS,
    /* gestures */
    TA_WAVE, TA_NOD, TA_SHAKE_HEAD, TA_HUG, TA_GIVE, TA_WALK_AWAY,
    TA_HIGH_FIVE,           /* an ordinary gesture. A hug stays a hug toward
                             * any child (DECISIONS.md "TALK HUG"): wholesome
                             * only, and the child may decline by familiarity */
    /* safety: what the gate names (input classes and its rewrites); never
     * an act a resident picks on its own */
    TA_REFUSE_SEXUAL, TA_REFUSE_MINOR, TA_REFUSE_HATE, TA_DISTRESS,
    TA_DEFLECT_ROMANCE,     /* a romance act the gate turned down (an adult target) */
    TA_COUNT
};
#define TA_FIRST_ROMANCE TA_FLIRT
#define TA_LAST_ROMANCE  TA_KISS
#define TA_FIRST_GESTURE TA_WAVE
#define TA_LAST_GESTURE  TA_HIGH_FIVE
#define TA_FIRST_SAFETY  TA_REFUSE_SEXUAL
#define TA_LAST_SAFETY   TA_DEFLECT_ROMANCE
#define TA_IS_ROMANCE(a) ((a) >= TA_FIRST_ROMANCE && (a) <= TA_LAST_ROMANCE)
#define TA_IS_GESTURE(a) ((a) >= TA_FIRST_GESTURE && (a) <= TA_LAST_GESTURE)
#define TA_IS_SAFETY(a)  ((a) >= TA_FIRST_SAFETY && (a) <= TA_LAST_SAFETY)

/* ---- tone (the PS1 tone ring; the parser's guess on DC) ------------------ */
enum { TONE_PLAIN = 0, TONE_WARM, TONE_CHEEKY, TONE_SNARKY,
       TONE_FLIRTY,            /* gated like the romance acts */
       TONE_COUNT };

/* ---- what the input safety classifier found (TalkUtter.safety) ----------- */
#define TS_SEXUAL    0x01u    /* a sexual-class word or pattern               */
#define TS_MINOR     0x02u    /* a minor word ("kid", "child", ...)           */
#define TS_HATE      0x04u    /* a slur or real-world hate                    */
#define TS_DISTRESS  0x08u    /* self-harm words: the comedy stops (K-11)     */
#define TS_REALWORLD 0x10u    /* real people, religions, games, studios       */
#define TS_STRONG    0x20u    /* a swear (register.tsv strong)                */
#define TS_CRUDE     0x40u    /* register.tsv crude                           */
#define TS_MASKED    0x80u    /* some of `shown` was masked (####)            */

#define TC_NONE 0u            /* no concept                                   */
#define TALK_SHOWN_MAX 96

/* One player turn, whatever the input path (typed, composed, quick reply). */
typedef struct TalkUtter {
    uint8_t  act[2];                  /* TA_*; act[1] = TA_NONE if only one    */
    uint16_t topic[2];                /* concept ids (12 bits), TC_NONE         */
    int8_t   target;                  /* participant slot 0..3, -1 = everyone   */
    uint8_t  tone;                    /* TONE_*                                 */
    uint8_t  safety;                  /* TS_* bits                              */
    uint8_t  conf;                    /* parser confidence 0..15 (composer: 15) */
    char     shown[TALK_SHOWN_MAX];   /* the player's bubble text (masked)      */
} TalkUtter;                           /* 106 B */

/* The room's audience for the gate (T4 talk_safe_ctx). */
typedef struct TalkAudience {
    uint8_t any_minor, avatar_minor, target_minor, pet, mature_ok, cast_kid;
} TalkAudience;

/* ---- limits ---------------------------------------------------------------- */
#define TALK_PARTS_MAX     4      /* residents in a conversation (PS1: 3)          */
#define TALK_PARTS_PS1     3      /* = HOME_HOUSEMATES                             */
#define TALK_BUBBLE_COLS   26     /* a bubble's columns at 320x240 (DC HD text: 40) */
#define TALK_BUBBLE_LINES  3

/* ---- the talk memory chunk 'TALK' (T6 owns it; T0 fixes its size) ---------
 * [u8 version][u8 count][u16 day_of_last] then count x 12 B entries
 * {u16 id; u16 day; u16 topic_a; u16 topic_b; u8 flags; u8 likes_known;
 *  u16 spare}, at most TALK_MEM_MAX (LRU). Worst case 4 + 24 x 12 = 292 B
 * (SAVE_EXT_MAX_TALK in save_budget.h is 320). Nothing is written while it
 * is empty. PS1, DC and native only: the GBA never registers it. */
#define TALK_MEM_MAX       24
#define TALK_MEM_HDR       4
#define TALK_MEM_ENTRY     12
#define TALK_MEM_WORST     (TALK_MEM_HDR + TALK_MEM_MAX * TALK_MEM_ENTRY)

/* =========================================================================
 * The database: TALK_<lang>.BIN (T1 writes it, T2 reads it)
 * =========================================================================
 * Built by core/tools/talkc.py from core/data/talk/ (make -C core talk ->
 * core/build/talk/TALK_<lang>.BIN; never committed). Little endian.
 *
 *   header, 20 B:
 *     u32 magic 'TALK'   u16 version   u16 lang (the LANG_* save byte)
 *     u32 dict_hash      (txt_codec_hash() of the story codec it was built
 *                         with: strings use the story's canonical Huffman
 *                         code, txt.h, so a story text change rebuilds it)
 *     u16 nbanks         u16 nconcepts (ids 1..nconcepts; 0 = none)
 *     u32 total          (the file size)
 *   directory, nbanks x 20 B:
 *     u16 id (TB_*)  u8 kind (TBK_*)  u8 flags (TBF_*)
 *     u32 offset  u32 packed (stored bytes)  u32 raw (decoded bytes)
 *     u32 crc32 (IEEE, reflected, init/xorout 0xFFFFFFFF, of the stored bytes)
 *   banks, each aligned to 64 B (talkc --align; SPU DMA), in directory order.
 *
 * Bank layouts (T1; later packages add banks, never change these):
 *   TB_CONCEPT  nconcepts + 1 rows of 8 B (row 0 unused):
 *               u8 category, u8 sent:3 (0..6 = -3..+3) | reg:3 << 3 | age:2 << 6,
 *               u16 attr_first (index into TB_ATTR), u16 form (string index
 *               in TB_FORMS), u16 flags (TCF_*, low 4 bits the attribute count)
 *   TB_ASSOC    nconcepts + 1 rows of 16 B: 8 x u16 (id:12 | weight:4 << 12),
 *               strongest first, 0 = none                (the top 8)
 *   TB_ATTR     u16 concept ids (the attribute lists, TB_CONCEPT points in)
 *   TB_FORMS    TBK_TXT: one string per concept, "lemma/plural" (the plural
 *               only when the concept has one; TCF_* say which is the default)
 *   TB_CATS     u16 ncats, u16 0, then ncats rows of 4 B {u8 parent, u8 flags
 *               (TCAT_*), u16 name (string index in TB_CATNAMES)}
 *   TB_CATNAMES TBK_TXT: the category keys ("food.dish")
 *   TB_EDGES    (DC, native) every reviewed edge: u16 first[nconcepts + 2]
 *               then u16 edges (id:12 | weight:4 << 12)
 * TBK_TXT banks: u16 nstr, u8 block_shift, u8 0, u32 nblocks, then
 * u32 block_base[nblocks], u16 off[nstr] (from its block's base), then the
 * Huffman streams (byte aligned, each ends with symbol 0; txt_decode()).
 */
#define TALK_MAGIC      0x4B4C4154u          /* "TALK" */
#define TALK_VERSION    1u
#define TALK_HDR_SIZE   20u
#define TALK_DIR_ENTRY  20u
#define TALK_MAX_CONCEPTS 4095               /* 12-bit ids, 0 = none */
#define TALK_ASSOC_TOP  8
#define TALK_ATTR_MAX   8
#define TALK_ALIGN      64u

enum {                                       /* bank ids (directory u16 id) */
    TB_NONE = 0, TB_CONCEPT, TB_ASSOC, TB_ATTR, TB_FORMS, TB_CATS, TB_CATNAMES,
    TB_EDGES,
    /* reserved for the later packages (T3-T7), in this order */
    TB_GRAMMAR, TB_STRINGS, TB_BEATS, TB_VOICE, TB_PARSE, TB_SAFETY,
    TB_COUNT
};
enum { TBK_RAW = 0, TBK_TXT = 1 };           /* directory kind */
#define TBF_DC_ONLY 0x01u                     /* not loaded on the PS1 (PARSE, EDGES) */

/* register classes (concept reg:3) */
enum { TR_CLEAN = 0, TR_CRUDE, TR_STRONG,
       TR_SEXUAL,      /* recognised, refused, never generated   */
       TR_HATE,        /* recognised, refused, never generated   */
       TR_REALWORLD,   /* recognised, deflected, never generated */
       TR_DISTRESS,    /* recognised: the comedy stops (K-11)    */
       TR_COUNT };
/* age classes (concept age:2) */
enum { TAGE_ANY = 0, TAGE_ADULT, TAGE_NEVER };

/* concept flags (TB_CONCEPT u16 flags; bits 0-3 = the attribute count) */
#define TCF_NATTR_MASK 0x000Fu
#define TCF_COUNTABLE  0x0010u   /* has a plural form                         */
#define TCF_MASS       0x0020u   /* "some butter": no a/an, no plural          */
#define TCF_PROPER     0x0040u   /* a name: no article, keeps its capitals     */
#define TCF_PLURAL     0x0080u   /* usually said in the plural ("pancakes")    */
#define TCF_AN         0x0100u   /* the indefinite article is "an"             */
#define TCF_THE        0x0200u   /* a proper name said with "the" ("the Feast") */
#define TCF_GENERATED  0x0400u   /* seeded from game data (talkc --seed-from-game) */
#define TCF_MINOR_WORD 0x0800u   /* names a child (the safety lexicon, T4)     */
#define TCF_MULTIWORD  0x1000u   /* the lemma has a space                       */
#define TCF_GERUND     0x2000u   /* an activity said as "-ing" ("baking")       */

/* category flags (TB_CATS) */
#define TCAT_TALKABLE  0x01u     /* can be a topic                          */
#define TCAT_GATED     0x02u     /* romance: adults only (K-4)               */
#define TCAT_SAFETY    0x04u     /* recognised only, never generated         */
#define TCAT_DESCRIBE  0x08u     /* attribute vocabulary (adjectives, verbs) */

/* The 18 category roots, in the order core/data/talk/categories.tsv lists
 * them (talkc checks it): their ids are fixed. */
enum {
    TCR_NONE = 0, TCR_PEOPLE, TCR_PLACES, TCR_FOOD, TCR_MUSIC, TCR_HOBBIES,
    TCR_FEARS, TCR_CREATURES, TCR_HOUSE, TCR_LORE, TCR_HELLVILE, TCR_FEELINGS,
    TCR_WORK, TCR_TIME, TCR_SCHOOL, TCR_ROMANCE, TCR_SAFETY, TCR_DESCRIBE,
    TCR_STUFF,
    TCR_COUNT
};

/* One concept row, decoded. */
typedef struct TalkConcept {
    uint16_t id;
    uint8_t  cat;            /* category id (TB_CATS row)        */
    int8_t   sent;           /* -3..+3 (before persona overrides) */
    uint8_t  reg;            /* TR_*                              */
    uint8_t  age;            /* TAGE_*                            */
    uint16_t flags;          /* TCF_*                             */
    uint16_t attr_first;
    uint16_t form;
} TalkConcept;

/* Which surface form talk_db_form() writes. */
enum {
    TF_DEFAULT = 0,          /* the lemma, or the plural for TCF_PLURAL        */
    TF_LEMMA,                /* "pie"                                          */
    TF_PLURAL,               /* "pies" (the lemma when it has none)            */
    TF_INDEF,                /* "a pie", "an owl", "some butter", "Vex"        */
    TF_DEF                   /* "the pie", "the pies", "the Feast", "Vex"       */
};

/* ---- the reader (T2, core/src/talk_db.c) ---------------------------------
 * One database is open at a time. It is read only through
 * plat_talk_read() (or the memory image of talk_db_open_mem), a few bytes
 * per call: nothing of it is resident (PS1: it lives in SPU RAM). */
enum {                                       /* talk_db_open*() results */
    TALK_DB_OK = 0, TALK_DB_NONE,            /* no file / not loaded     */
    TALK_DB_LOADING,                         /* the platform is still staging it: call again */
    TALK_DB_BAD_MAGIC, TALK_DB_BAD_VERSION, TALK_DB_DICT_MISMATCH,
    TALK_DB_BAD_DIR, TALK_DB_BAD_CRC
};
/* Open TALK_<code>.BIN of language `lang` (LANG_* id) through the platform. */
int  talk_db_open(int lang);
/* Open a database image in memory (tests: core/gen/talk_fixture.c). */
int  talk_db_open_mem(const uint8_t *image, uint32_t size);
void talk_db_close(void);
int  talk_db_is_open(void);
/* Check every bank's CRC (streams the whole file; tests, the PS1 loader). */
int  talk_db_verify(void);
int  talk_db_nconcepts(void);
int  talk_db_lang(void);
/* A bank's directory entry: 1 found. */
int  talk_db_bank(int id, uint32_t *offset, uint32_t *packed, uint32_t *raw);
/* len bytes at offset `at` of bank `id` (TB_*) into dst: 1 ok (T3 / T4
 * read TB_PARSE and TB_SAFETY with it). */
int  talk_db_read(int id, uint32_t at, void *dst, uint32_t len);
/* Concept `id` (1..nconcepts): 1 ok, 0 out of range / not open. */
int  talk_db_concept(int id, TalkConcept *out);
/* Its top associations, strongest first: ids[] and weights 1..15; returns
 * the count (0..TALK_ASSOC_TOP). */
int  talk_db_assoc(int id, uint16_t *ids, uint8_t *weights);
/* Every reviewed edge (TB_EDGES, DC / native; 0 when the bank is absent). */
int  talk_db_edges(int id, uint16_t *ids, uint8_t *weights, int cap);
/* Its attributes (adjectives / verbs said of it): returns the count. */
int  talk_db_attrs(int id, uint16_t *ids);
/* A surface form into out (always NUL-terminated); returns the length,
 * -1 on error. */
int  talk_db_form(int id, int which, char *out, int cap);
/* Categories: count, parent / flags, key ("food.dish"), root (TCR_*). */
int  talk_db_ncats(void);
int  talk_db_cat(int cat, uint8_t *parent, uint8_t *flags);
int  talk_db_cat_name(int cat, char *out, int cap);
int  talk_db_cat_root(int cat);
/* IEEE CRC-32 over len bytes, continuing from crc (start with 0). */
uint32_t talk_crc32(uint32_t crc, const uint8_t *p, uint32_t len);

/* =========================================================================
 * T3: the free-text parser (Dreamcast and native; the PS1 never links it:
 * its pad composer builds TalkUtter directly). core/src/talk_lex.c tags
 * the words, core/src/talk_parse.c runs the rules of TB_PARSE (talkc.py
 * compiles core/data/talk/rules/<lang>.tsv; the bank format is in
 * core/tools/talkc_parse.py). docs/TALK.md "The parser".
 * ========================================================================= */
#define TALK_INPUT_MAX   80       /* the Dreamcast typing line                 */
#define TALK_TOK_MAX     40       /* tokens of one line                        */
#define TALK_TOK_LEN     24       /* a token's canonical text                  */
#define TALK_STACK_MAX   8        /* the recent topic stack                    */

/* word flags (TB_PARSE word table, TalkTok.wflags) */
#define TW_FUNC      0x0001u      /* a function word here: never the topic    */
#define TW_SUGGEST   0x0002u      /* a flirting word (register sexual, not explicit) */
#define TW_SEXUAL    0x0004u      /* an explicit sexual word: always refused  */
#define TW_HATE      0x0008u
#define TW_DISTRESS  0x0010u
#define TW_MINOR     0x0020u      /* names a child (K-7)                      */
#define TW_REALWORLD 0x0040u
#define TW_STRONG    0x0080u      /* a swear                                   */
#define TW_CRUDE     0x0100u
#define TW_CAPPED    0x0200u      /* the f-word, the s-word, bastard ...       */
#define TW_TOPIC     0x0400u      /* a +topic class word                       */
#define TW_GROUP     0x0800u      /* a group of real people (hate with HATEV)  */
#define TW_MINORX    0x1000u      /* only ever a child (underage, preteen ...): with a liking word, K-7 */
#define TW_NAME      0x2000u      /* a first name (rules/names_<lang>.txt): known, so never "corrected"
                                     into a safety word; a PERSON when a resident has it */

/* token kinds and flags */
enum { TTK_WORD = 0, TTK_QMARK, TTK_EXCL, TTK_NUM, TTK_PERSON };
#define TTF_UNK      0x01u        /* not in the lexicon                        */
#define TTF_FUZZY    0x02u        /* matched at edit distance 1                */
#define TTF_ABSENT   0x04u        /* a person who is not in the room           */
#define TTF_MASK     0x08u        /* masked in the player's bubble             */
#define TTF_LEET     0x10u        /* found only after the leet / spacing fold  */
#define TTF_POSS     0x20u        /* a person said with 's ("Nana's"): not a vocative */

typedef struct TalkTok {
    char     text[TALK_TOK_LEN];  /* canonical, folded                        */
    uint32_t tags[4];             /* rule classes (128 bits)                   */
    uint32_t hash;                /* FNV-1a 32 of text (the rules' literals)   */
    uint16_t concept;             /* 0 none                                    */
    uint16_t wflags;              /* TW_*                                      */
    int16_t  person;              /* participant slot (TTK_PERSON), or the absent id */
    uint8_t  kind;                /* TTK_*                                     */
    uint8_t  flags;               /* TTF_*                                     */
    uint8_t  src0, src1;          /* the input bytes it covers [src0, src1)    */
    uint8_t  root;                /* TCR_* of the concept's category, 0 none   */
    int8_t   sent;                /* the concept's sentiment                   */
    uint8_t  cat;                 /* the concept's category                    */
    uint8_t  value;               /* TTK_NUM: the number (saturates at 255)    */
} TalkTok;

/* What the parser knows about the room. */
typedef struct TalkParseCtx {
    uint8_t     nparts;                       /* residents in the room, 0..4   */
    int8_t      ring;                         /* the facing ring's holder slot */
    uint8_t     hops;                         /* topic activation: 1 (PS1 / default) or 2 (DC) */
    uint8_t     nstack;
    uint8_t     kid_room;                     /* a non-adult is here: crude and flirting words are masked too */
    const char *name[TALK_PARTS_MAX];         /* their names as shown          */
    uint16_t    stack[TALK_STACK_MAX];        /* recent topics, newest first   */
    /* another resident by name (absent people for GOSSIP; S1 asks
     * talk_safe_minor() about them): an id, or -1. canon_name is a folded
     * full name or first name. When several residents share a first name,
     * return a child if one of them is one (fail safe). NULL: none. The
     * game passes the town lookup. */
    int       (*person)(const char *canon_name);
} TalkParseCtx;

/* The parser's extra findings (tests, the drama manager). */
typedef struct TalkParseInfo {
    int16_t  subject;             /* an absent person's id (GOSSIP), -1        */
    int8_t   other;               /* a participant the act is about (TAKE_SIDE(Y)), -1 */
    uint8_t  ntok;
    uint16_t rule[2];             /* which rule made act[0] / act[1] (index), 0xFFFF none */
    uint8_t  fuzzy;               /* tokens matched at edit distance 1          */
    uint8_t  unknown;             /* unknown tokens                             */
    uint8_t  cut;                 /* the line went on past the tagger's limits: the
                                     byte tok[] stops at (the bubble ends there; S1
                                     still read the rest), 0 = all of it was read */
    TalkTok  tok[TALK_TOK_MAX];
} TalkParseInfo;

/* S0: the canonical key of a word run (talkc_parse.py canon() is the same). */
int  talk_lex_canon(const char *in, char *out, int cap);
/* Tag a line: the tokens, persons, concepts, word flags. Needs TB_PARSE
 * (talk_db open). Returns the token count. A person named by a word the
 * lexicon knows (the cast) gets the game's lookup id in TalkTok.person,
 * so S1 can ask talk_safe_minor() about a child named in the line. */
int  talk_lex(const char *text, const TalkParseCtx *ctx, TalkTok *tok, int max);
/* talk_lex, and *cut = the byte where the tagger stopped when the line has
 * more words than max tokens (or its raw-word / unit caps) hold: a raw
 * word's first byte, everything from it on is not in tok[]. 0 when the
 * whole line (up to 2 x TALK_INPUT_MAX bytes) was read. */
int  talk_lex_cut(const char *text, const TalkParseCtx *ctx, TalkTok *tok, int max, int *cut);
/* The topic by spreading activation (design 3.2): topic[0..1] (TC_NONE). */
void talk_lex_topics(const TalkTok *tok, int n, const TalkParseCtx *ctx, uint16_t topic[2]);
/* 1 if the lexicon knows the word as it is or by a lemma (*wflags: its TW_*). */
int  talk_lex_known(const char *word, uint16_t *wflags);
/* Look a canonical key up: its word index, or -1. */
int  talk_lex_find(const char *key, uint16_t *concept, uint16_t *wflags, uint32_t tags[4]);
/* Parse one typed line into out (acts, topic, target, tone, safety, conf,
 * the masked bubble text). The S1 safety classifier runs first and wins.
 * info may be NULL. Returns 1, or 0 when TB_PARSE is missing (no parser:
 * out is UNKNOWN). Deterministic: no RNG, no state. */
int  talk_parse(const char *text, const TalkParseCtx *ctx, TalkUtter *out, TalkParseInfo *info);

/* =========================================================================
 * T4: the safety gate (core/src/talk_safe.c; every build). Design section 7,
 * docs/TALK.md "Safety". Talk keeps NO list of minors of its own.
 * ========================================================================= */
/* minor(id) (design 7.3): a resident: LIFE_IS_MINOR(sim_life_stage(id)), or
 * roster traits KID / TEEN, or ending_kid_cast(id); the avatar (the hero,
 * DOOMO_PLAYER_ID): its stage; a family pool member: its FamilyMember stage;
 * anything else (an unknown id) counts as a minor: fail safe. */
int  talk_safe_minor(int id);
/* A family pet, or the Boreagotchi (TALK_PET_ID). */
#define TALK_PET_ID  0x7FFF
int  talk_safe_is_pet(int id);
/* The room's audience: avatar, the participants (ids; TALK_PET_ID for the
 * pet), and the addressee (a participant slot, -1 everyone). mature_ok is
 * opt_mature() >= MATURE_ON and mature_adult_ok() for every adult. */
void talk_safe_ctx(int avatar, const int *ids, int n, int target, TalkAudience *out);

/* S1: classify typed input (needs TB_PARSE): TS_* bits; the tokens get
 * TTF_MASK where the bubble must hide them. talk_parse() calls it on every
 * word of the line (a line longer than the token cap is read in parts).
 * A person named in the line who is not in the room (TalkTok.person, an
 * id) and talk_safe_minor() is a child: TS_MINOR. */
uint8_t talk_safe_scan(TalkTok *tok, int n);
/* 1 if a token is an explicit sexual word (not a flirting one): REFUSE_SEXUAL. */
int  talk_safe_explicit(const TalkTok *tok, int n);
/* The hate list (TB_SAFETY): 1 if a word (any case, leet, repeats) is on it,
 * exactly or at edit distance 1 after the first letter (5+ letters). */
int  talk_safe_hate_word(const char *word, int fuzzy);
/* The salted hash of a squeezed word (tests: the C and Python hashes agree);
 * 0 without TB_SAFETY. */
uint64_t talk_safe_hash(const char *word);

/* S2: what the gate did */
enum {
    TG_PASS = 0,         /* the utterance stands                                 */
    TG_REFUSE,           /* REFUSE_SEXUAL (adults' room): no echo                */
    TG_REFUSE_MINOR,     /* a romance act with a non-adult avatar or target, or
                          * about a child (TS_MINOR: "I'm 12", a minor word, a
                          * child named in the line)                            */
    TG_DEFLECT,          /* DEFLECT_ROMANCE: an adult said no / not in front of the kids */
    TG_HATE,             /* REFUSE_HATE: a short clean disapproval, a bond penalty */
    TG_DISTRESS,         /* the comedy stops (K-11)                              */
    TG_END_K7            /* sexual + a child: the conversation ends, locked for the day */
};
/* The audience gate (S2): rewrite u in place (acts, tone, shown) and say
 * why. avatar / target_id are ids (target_id -1: everyone). refused_today:
 * the target already said no today (K-5). */
int  talk_safe_gate(TalkUtter *u, const TalkAudience *aud, int avatar, int target_id, int refused_today);
/* May the avatar do romance acts at target_id here (K-4)? The composer
 * lists FLIRT / CONFESS / ASK_OUT / KISS and the FLIRTY tone only then. */
int  talk_safe_romance_ok(const TalkAudience *aud, int avatar, int target_id);
/* May a template with a jmat variant use it (K-6)? Needs opt_mature() >=
 * MATURE_ON and mature_adult_ok() for every adult id given. */
int  talk_safe_jmat_ok(const int *ids, int n);

/* S3: the output lint, on every generated line, on every platform (needs
 * TB_SAFETY). Returns TL_OK or the first problem; *swears counts the
 * everyday adult swears of the conversation (K-9: at most 1). */
enum { TL_OK = 0, TL_FORBIDDEN, TL_REGISTER_KID, TL_SEXUAL_PET, TL_SWEAR_CAP, TL_HATE, TL_LENGTH,
       TL_NO_DB };
int  talk_safe_lint(const char *line, const TalkAudience *aud, int *swears);
/* Word-wrap a line at TALK_BUBBLE_COLS: the number of bubble lines. */
int  talk_safe_bubble_lines(const char *line, int cols);
/* The lint's hit counter (a test fails on any count > 0; release builds
 * log "talk: lint"). */
uint32_t talk_safe_lint_hits(void);

/* ---- the house hook (T8; weak in core/src/weak_talk.c) ---------------------
 * Start Talk with resident `who` in the room: 1 started, 0 not available
 * (no PLATF_TALK, or not built: the default). The house reaches Talk only
 * through this and the feature test, so --gc-sections drops it all on the
 * GBA. */
int  home_talk_enter(int who);

#endif
