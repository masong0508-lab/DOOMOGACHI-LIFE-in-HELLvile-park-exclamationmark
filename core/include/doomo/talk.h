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
#define TCF_GRIEF      0x4000u   /* grief or loss (K-10): talked about only in the
                                    sincere bank, never in a joke ("funeral")   */

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
/* (T5a) string idx of a TBK_TXT bank (TB_STRINGS: the template strings)
 * into out: its length, -1 on error. */
int  talk_db_string(int bank, int idx, char *out, int cap);

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
/* (T5a) The same lint on a piece of text that is not a bank line, before
 * the generator puts it in a line: a name the player typed (a custom
 * resident's; strict: a register word in it is a problem whatever the
 * room) or a filled-in word ("spice's" squeezes to a hate-list plural; an
 * adults-only word with a child here). TL_OK or the problem; it counts
 * nothing and a swear in it is a problem (K-9 is the template's). The
 * final line still goes through talk_safe_lint(), which counts. */
int  talk_safe_lint_probe(const char *text, const TalkAudience *aud, int strict);

/* =========================================================================
 * T5a: the generator (core/src/talk_gen.c; PS1, Dreamcast, native). Design
 * 3.6, docs/TALK.md "The generator". It writes only from the compiled
 * database: template strings (TB_STRINGS) chosen by the grammar (TB_GRAMMAR,
 * core/data/talk/grammar/<lang>/ and voice/<lang>/ .tlk files compiled by
 * talkc.py; the format is in core/tools/talkc_gen.py) and concept forms
 * (TB_FORMS). Nothing the player typed is ever copied into a line.
 *
 * SAFETY: talk_gen_exchange() runs talk_safe_gate() on the player's
 * utterance first, every line it writes goes through talk_safe_lint() (S3),
 * and a hit replaces the line with a `safe` bank line and counts
 * (talk_safe_lint_hits(); a test fails on any). Romance replies need
 * TGP_ADULTS_ONLY and !TGP_KID_PRESENT (talkc lint TK3); a concept that may
 * not be said in the room (adults-only with a child here, a never-generated
 * one) is never put in a line (talk_gen_concept_ok).
 * K-10 (grief is not a punchline, SCRIPT_STYLE R6): a topic that is a
 * TCF_GRIEF concept (grief, loss, funeral, ...) takes the sincere `grief`
 * bank in place of core.<act>, whatever the act (not the safety acts, whose
 * banks are sincere already): the core part alone, no opener, callback, tag
 * or hand-off, a reply act the adjacency table allows. Another resident may
 * add one line from the sincere `comfort` bank; the pet stays quiet. A
 * TCF_GRIEF concept is never filled into any other line (an association, the
 * talk memory). TGP_FACT_LOSS / TCB_LOSS: the speaker, a household member,
 * lost someone in the last TALK_GRIEF_DAYS town days (family_memorial*: the
 * newest memorial of a death, not a move away; the value is the lost one's
 * id); {lost} says that one's name.
 *
 * A reply line has five optional parts (design 3.6):
 *   [opener] [core] [callback] [tag] [hand-off]
 * chosen by weighted random from a per-conversation RNG (the seed is a hash
 * of the town seed, the day and the participants), so the text is the same
 * on every platform. It must fit the bubble (TALK_BUBBLE_LINES x
 * TALK_BUBBLE_COLS): the generator re-picks up to 4 times, then drops the
 * tag, the callback, the opener and the hand-off.
 * ========================================================================= */
#define TALK_LINE_MAX      96      /* a generated line: text + line codes, NUL */
#define TALK_NOVELTY       6       /* the novelty ring of each participant      */
#define TALK_ROOM_NOVELTY  16      /* the room's ring: no line twice in a talk   */
#define TALK_EXCHANGE_MAX  4       /* lines one exchange can produce            */
#define TALK_SENTENCES_MAX 3       /* sentences a line (Q1, design 3.7)         */
#define TALK_GRIEF_DAYS    28      /* town days a loss is fresh (TCB_LOSS, K-10) */
#define TALK_TRACE_ALTS    12
#define TALK_TRACE_FILLS   12

/* stance on the topic */
enum { TST_NEUTRAL = 0, TST_PRO, TST_CON, TST_COUNT };
/* the drama manager's beats (T6 drives them; the grammar reads them) */
enum { TBEAT_NONE = 0, TBEAT_ARRIVE, TBEAT_SMALLTALK, TBEAT_TOPIC_DIVE, TBEAT_HOUSE, TBEAT_GOSSIP,
       TBEAT_PROBLEM, TBEAT_RIVALRY, TBEAT_ALLY_ASK, TBEAT_STORY, TBEAT_GAME, TBEAT_KID_MOMENT,
       TBEAT_ROMANCE_ADULT, TBEAT_WIND_DOWN, TBEAT_COUNT };
/* mood bands (from MOOD_*) */
enum { TMB_OK = 0, TMB_HAPPY, TMB_SAD, TMB_ANGRY, TMB_SCARED, TMB_TIRED, TMB_WEIRD, TMB_COUNT };
/* relationship bands, speaker to addressee */
enum { TRB_STRANGER = 0, TRB_ACQUAINTANCE, TRB_FRIEND, TRB_BEST, TRB_PARTNER, TRB_RIVAL, TRB_FAMILY,
       TRB_COUNT };
/* time of day */
enum { TTOD_MORNING = 0, TTOD_DAY, TTOD_EVENING, TTOD_NIGHT, TTOD_COUNT };
/* player act classes (core/data/talk/acts.tsv; openers are chosen by them) */
enum { TAC_OPEN = 0, TAC_ASK, TAC_TELL, TAC_WARM, TAC_COOL, TAC_THIRD, TAC_FLOW, TAC_ROMANCE,
       TAC_GESTURE, TAC_SAFETY, TAC_COUNT };
/* callback kinds: a sim fact that is true now (read through the sim's
 * accessors when the line is made, so a callback is true by construction) */
enum { TCB_NONE = 0, TCB_JOB, TCB_PARTNER, TCB_PET, TCB_NEWS, TCB_PROBLEM, TCB_BDAY, TCB_GHOST,
       TCB_POSSESSED, TCB_HELLBORN, TCB_NIGHT, TCB_MEMORY, TCB_LIKE, TCB_FEAR, TCB_CHAPTER, TCB_FAMILY,
       TCB_LOSS,     /* K-10: the speaker lost a household member lately (family_memorial*) */
       TCB_COUNT };
/* talk memory flags (T6 keeps them in the 'TALK' chunk; TalkConv.mem_flags) */
#define TMF_COMPLIMENTED 0x01u
#define TMF_INSULTED     0x02u
#define TMF_PROMISED     0x04u
#define TMF_REFUSED_TODAY 0x08u   /* T6 (K-5): said no to a romance act this town day; the table only
                                   * (talk_mem_begin turns it into TalkConv.refused), never in mem_flags */
/* grammar entry points besides core.<act> and react.<act> */
enum { TGE_OPEN = 0, TGE_CB, TGE_TAG, TGE_HANDOFF, TGE_SAFE, TGE_END_K7, TGE_PET, TGE_IDLE,
       TGE_GRIEF,    /* K-10: the sincere core of a line about grief or loss */
       TGE_COMFORT,  /* K-10: another resident's sincere reaction to it      */
       TGE_COUNT };

/* Guard predicates (talkc_gen.py reads this enum: a guard term "kid_present"
 * is TGP_KID_PRESENT, "fact.job" TGP_FACT_JOB, "x.countable"
 * TGP_X_COUNTABLE; from TGP_FIRST_VALUED on a term carries a value:
 * "stance=pro", "speaker=grumbleguts", "mood=sad"). Add-only. */
enum {
    TGP_NONE = 0,
    /* the room (talk_safe_ctx) */
    TGP_KID_PRESENT, TGP_PET, TGP_ADULTS_ONLY, TGP_JMAT, TGP_MATURE_ADULT_OK,
    /* the speaker */
    TGP_KID_SPEAKER, TGP_TEEN, TGP_ELDER, TGP_POLITE, TGP_BLUNT, TGP_FLAT, TGP_LOUD, TGP_SERIOUS,
    TGP_EASYGOING, TGP_QUIRKY, TGP_QUICK, TGP_SLOW, TGP_HELLBORN, TGP_GHOST, TGP_POSSESSED, TGP_CAST,
    /* the line */
    TGP_TOPIC, TGP_TO_PLAYER, TGP_TO_RES, TGP_OTHER, TGP_GROUP, TGP_CONSENT, TGP_CATCH, TGP_NIGHT,
    TGP_LIKES_TOPIC, TGP_FEARS_TOPIC, TGP_HATES_TOPIC, TGP_FIRST,
    /* facts (callbacks) */
    TGP_FACT_JOB, TGP_FACT_PARTNER, TGP_FACT_PET, TGP_FACT_NEWS, TGP_FACT_PROBLEM, TGP_FACT_BDAY,
    TGP_FACT_MEMORY, TGP_FACT_COMPLIMENTED, TGP_FACT_INSULTED, TGP_FACT_PROMISED, TGP_FACT_FAMILY,
    TGP_FACT_LOSS,                   /* K-10: TCB_LOSS holds for the speaker */
    /* the topic's forms and data */
    TGP_X_COUNTABLE, TGP_X_PROPER, TGP_X_MASS, TGP_X_PLURAL, TGP_X_GERUND, TGP_X_PERSON,
    TGP_X_ADJ, TGP_X_GOOD, TGP_X_BAD, TGP_X_ASSOC,
    TGP_X_GRIEF,                     /* K-10: the topic is a TCF_GRIEF concept */
    /* valued */
    TGP_FIRST_VALUED,
    TGP_STANCE = TGP_FIRST_VALUED,   /* TST_*                              */
    TGP_SPEAKER,                     /* a cast index (TB_GRAMMAR cast table) */
    TGP_TYPE,                        /* persona_type() 0..15               */
    TGP_MOOD,                        /* TMB_*                              */
    TGP_REL,                         /* TRB_*                              */
    TGP_ROOT,                        /* the topic's TCR_*                  */
    TGP_CHAPTER,                     /* STORY_CH_*                         */
    TGP_TIME,                        /* TTOD_*                             */
    TGP_BEAT,                        /* TBEAT_*                            */
    TGP_TONE,                        /* the player's TONE_*                */
    TGP_PACT,                        /* the act class replied to (TAC_*)   */
    TGP_ACT,                         /* the act replied to (TA_*)          */
    TGP_PROB,                        /* the speaker's PROB_*               */
    TGP_COUNT
};

/* One conversation as the generator needs it between lines (T6 keeps it in
 * its TalkState). talk_gen_begin() fills it. */
typedef struct TalkConv {
    uint32_t seed;                               /* the conversation seed            */
    int16_t  avatar;                             /* the player's id (play-as aware)  */
    int16_t  id[TALK_PARTS_MAX];                 /* residents, pool ids, TALK_PET_ID  */
    uint8_t  n;                                  /* participants                     */
    uint8_t  beat;                               /* TBEAT_* (T6; SMALLTALK)          */
    uint8_t  turn;                               /* the player's turn number         */
    uint8_t  swears;                             /* K-9: everyday swears said        */
    uint8_t  spoke, catch_used, refused;         /* a bit per slot                   */
    uint8_t  consent;                            /* a bit per slot: says yes to a romance act the gate passed (T6 / the sim decide) */
    uint8_t  ended;                              /* K-7: it is over                  */
    uint8_t  tone;                               /* the player's last TONE_*         */
    uint8_t  chapter;                            /* story_chapter() at the start     */
    uint8_t  lines;                              /* lines made (the RNG stream)      */
    uint8_t  plain;                              /* 1: the core part only, no opener, callback, tag or hand-off (tests; a short UI preview) */
    TalkAudience aud;                            /* talk_safe_ctx() of the room       */
    uint16_t recent[TALK_PARTS_MAX][TALK_NOVELTY]; /* novelty rings: alternatives used, newest first */
    uint16_t room_recent[TALK_ROOM_NOVELTY];     /* the room's last core alternatives, any speaker (no line twice) */
    uint16_t mem_topic[TALK_PARTS_MAX];          /* talk memory (T6): last topic      */
    uint8_t  mem_flags[TALK_PARTS_MAX];          /* talk memory (T6): TMF_*           */
} TalkConv;

/* ---- T6: talk memory (core/src/talk_save.c; docs/TALK.md 6e) ------------------
 * What a resident remembers of the player's talks: the 'TALK' chunk's table
 * (TALK_MEM_MAX entries, the most recently used first; a 25th person evicts
 * the last). Ids are a resident, the hero or a family pool id; the pet and
 * anyone who is gone have no entry. Every call first clears
 * TMF_REFUSED_TODAY when the town day has changed. The chunk itself is
 * registered only where the build defines DOOMO_HAS_TALK (the link-time
 * twin of PLATF_TALK: the native host today); without it this is a plain
 * table nobody saves, and --gc-sections drops it where nobody calls it. */
#define TALK_MEM_VERSION 1
typedef struct TalkMem {
    uint16_t id;                  /* a resident, the hero or a pool id            */
    uint16_t day;                 /* the town day (low 16 bits) of the last talk  */
    uint16_t topic_a, topic_b;    /* the last topic and the one before (12 bits)  */
    uint8_t  flags;               /* TMF_* (the high nibble is the running conversation's, never saved) */
    uint8_t  likes_known;         /* bit `which` of talk_gen_like(): 0 food, 1 hobby, 2 music, 3 fear, 4 hated food */
    uint16_t spare;               /* 0                                            */
} TalkMem;                        /* TALK_MEM_ENTRY = 12 B                        */
/* Entries held, and entry i (0 the most recent), NULL past the end. */
int  talk_mem_count(void);
const TalkMem *talk_mem_at(int i);
/* The entry of id, or NULL. An entry whose person is gone is dropped here. */
const TalkMem *talk_mem_find(int id);
/* Find or make the entry of id and move it to the front (day = today). 1,
 * or 0 for the pet and for an id that is nobody. */
int  talk_mem_touch(int id);
/* The topic just talked about with id (touches): it becomes topic_a. */
void talk_mem_note_topic(int id, uint16_t topic);
/* Set TMF_* for id (touches). TMF_REFUSED_TODAY holds at once. The other
 * three belong to the running conversation: later conversations see them
 * once talk_mem_end() has run, never this one ("last time" stays true). */
void talk_mem_set_flags(int id, unsigned flags);
/* Clear TMF_* of id, set or pending (a promise kept). Does not touch. */
void talk_mem_clear_flags(int id, unsigned flags);
/* The player learned id's like `which` (0..4, as talk_gen_like; touches);
 * and the query (1 known). */
void talk_mem_know_like(int id, int which);
int  talk_mem_like_known(int id, int which);
/* 1 if id said no to a romance act today (K-5): the gate's refused_today. */
int  talk_mem_refused_today(int id);
/* A conversation begins (after talk_gen_begin): fill c->mem_topic and
 * c->mem_flags (TMF_COMPLIMENTED / INSULTED / PROMISED) of every participant
 * and set c->refused for those who said no today. A remembered topic the
 * open database no longer has (a retired id) is left out. */
void talk_mem_begin(TalkConv *c);
/* A conversation ends: every participant who spoke, said no or got a flag
 * is touched; "complimented" and "insulted" become this conversation's,
 * "promised" stays until cleared, c->refused becomes TMF_REFUSED_TODAY
 * (for everyone in the room after a K-7 end). */
void talk_mem_end(const TalkConv *c);
/* Forget id (the resident left the island; a new one may get the slot). */
void talk_mem_forget(int id);
/* Empty the table (a new town does it through the save registry). */
void talk_mem_reset(void);

/* One generated line. */
#define TLF_REACTION 0x01u          /* a reaction to another resident                    */
#define TLF_SAFE     0x02u          /* S3 replaced it with a safe line (a test fails)    */
#define TLF_SINCERE  0x04u          /* a refusal, distress or K-7 line: no joke          */
#define TLF_END      0x08u          /* the conversation ends (K-7)                        */
#define TLF_HANDOFF  0x10u          /* it hands the talk to another participant           */
#define TLF_FALLBACK 0x20u          /* nothing fitted the bubble: a safe line (a Q1 failure) */
typedef struct TalkLine {
    char     text[TALK_LINE_MAX];   /* the bubble text (ui_dialog line codes kept)      */
    int8_t   who;                   /* participant slot                                 */
    int8_t   to;                    /* the slot addressed; -1 the player; -2 everyone    */
    uint8_t  act;                   /* the reply act (TA_*)                             */
    uint8_t  stance;                /* TST_*                                            */
    uint8_t  face;                  /* EXPR_* + 1 of a [FACE] code, 0 none              */
    uint8_t  flags;                 /* TLF_*                                            */
    uint16_t topic;                 /* what it is about, TC_NONE                         */
} TalkLine;

/* What went into a line (tests, the census: Q1-Q4; NULL in the game). */
enum { TFK_NONE = 0, TFK_NAME, TFK_CONCEPT, TFK_TEXT };
#define TPART_OPEN    0x01u
#define TPART_CORE    0x02u
#define TPART_CB      0x04u
#define TPART_TAG     0x08u
#define TPART_HANDOFF 0x10u
typedef struct TalkFill { uint8_t off, len, kind, cat; } TalkFill;   /* cat: a concept's category */
typedef struct TalkTrace {
    uint16_t alt[TALK_TRACE_ALTS];  /* every alternative used, in order              */
    uint8_t  nalt, nfill;
    uint8_t  parts;                 /* TPART_* in the line                           */
    uint8_t  cb;                    /* TCB_* of the callback, 0 none                 */
    uint16_t cb_val;                /* its value: the fact as read (Q4)              */
    uint16_t said[4];               /* concepts the line says (Q2)                   */
    uint8_t  nsaid;
    uint8_t  picks;                 /* tries spent fitting the bubble                */
    uint8_t  tic;                   /* the persona type of a tic tag, 0xFF none      */
    uint8_t  gate;                  /* TG_* of the utterance (talk_gen_exchange)     */
    uint16_t core_alt;              /* the core part's alternative (Q5), 0xFFFF none */
    TalkFill fill[TALK_TRACE_FILLS]; /* names, concepts and catchphrases in the text */
} TalkTrace;

/* One exchange: the player's turn and every resident line in reply. */
typedef struct TalkExchange {
    TalkLine line[TALK_EXCHANGE_MAX];
    uint8_t  n;                     /* lines                                         */
    uint8_t  gate;                  /* TG_* of the player's utterance                */
    int8_t   first;                 /* the slot that answered, -1 none               */
    uint8_t  pad;
} TalkExchange;

/* An alternative of the grammar, decoded (tests, the census). */
typedef struct TalkAltInfo {
    uint16_t rule;
    uint8_t  act;                   /* the rule's reply act                           */
    uint8_t  rflags, aflags;        /* talkc_gen.py RF_* / AF_*                       */
    uint8_t  weight, nterm;
    uint8_t  term[2 * 16];          /* the rule's then the alternative's guard terms: {pred | neg << 7, value} */
} TalkAltInfo;

/* 1 if the open database has the generator's banks (TB_GRAMMAR, TB_STRINGS). */
int  talk_gen_ready(void);
/* The conversation seed: town seed, day and the participants. */
uint32_t talk_gen_seed(int avatar, const int *ids, int n);
/* Start a conversation: the audience (talk_safe_ctx), chapter, beat
 * SMALLTALK, empty memory. target -1 (everyone) for the audience. */
void talk_gen_begin(TalkConv *c, int avatar, const int *ids, int n, uint32_t seed);
/* One line from participant `who` to `to` (a slot, -1 the player, -2
 * everyone) in reply to `act` about `topic`. Returns 1, or 0 when nothing
 * fits (the line is then a safe line). tr may be NULL. */
int  talk_gen_line(TalkConv *c, int who, int to, int act, uint16_t topic, TalkLine *out, TalkTrace *tr);
/* One exchange: the gate on u (rewritten in place), who answers (the
 * addressee, else the most interested), up to two reactions between the
 * residents, a hand-off. A K-7 input ends the conversation (c->ended).
 * tr: NULL or TALK_EXCHANGE_MAX traces. Returns the number of lines.
 * (The plain turn-taking here stands in until T6's drama manager.) */
int  talk_gen_exchange(TalkConv *c, TalkUtter *u, TalkExchange *x, TalkTrace *tr);
/* The speaker's stance on a concept: their likes +3, their hated food and
 * fear -3, Hellvile-born flips holy things, plus a fixed personal lean. */
int  talk_gen_stance(int id, uint16_t topic);
/* May this concept be said in this room (age class, register, a child here)? */
int  talk_gen_concept_ok(uint16_t concept, const TalkAudience *aud);
/* acts.tsv: may `reply` answer `act`; the act's class (TAC_*). */
int  talk_gen_adjacent(int act, int reply);
int  talk_gen_act_class(int act);
/* A resident's like as a concept: which 0 food, 1 hobby, 2 music, 3 fear,
 * 4 hated food; 0 none. */
uint16_t talk_gen_like(int id, int which);
/* The roster id of cast index i (a TGP_SPEAKER value), -1 none. */
int  talk_gen_cast_id(int i);
/* An alternative of the grammar (tests, the census): 1 ok. */
int  talk_gen_alt_info(int alt, TalkAltInfo *out);
int  talk_gen_nalts(void);
/* The callback fact of kind cb for resident id as the sim says now (Q4):
 * 1 and *val when it holds. */
int  talk_gen_fact(int id, int cb, uint16_t *val);

/* ---- the house hook (T8; weak in core/src/weak_talk.c) ---------------------
 * Start Talk with resident `who` in the room: 1 started, 0 not available
 * (no PLATF_TALK, or not built: the default). The house reaches Talk only
 * through this and the feature test, so --gc-sections drops it all on the
 * GBA. */
int  home_talk_enter(int who);

#endif
