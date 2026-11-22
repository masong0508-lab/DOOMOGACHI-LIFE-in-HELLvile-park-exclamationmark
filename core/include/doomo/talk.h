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
    uint8_t  unk_off, unk_len;        /* TM2d: the first unknown word of `shown` (byte offset, length) that the
                                         parser lets the reply name (E1, E3); 0, 0 none. talk_parse sets it; the
                                         generator re-checks every fence on the text itself (E2 to E7)        */
} TalkUtter;                           /* 108 B */

/* The room's audience for the gate (T4 talk_safe_ctx). */
typedef struct TalkAudience {
    uint8_t any_minor, avatar_minor, target_minor, pet, mature_ok, cast_kid;
} TalkAudience;

/* ---- limits ---------------------------------------------------------------- */
#define TALK_PARTS_MAX     4      /* residents in a conversation (PS1: 3)          */
#define TALK_PARTS_PS1     3      /* = HOME_HOUSEMATES                             */
#ifdef DOOMO_PLATFORM_DC
#define TALK_BUBBLE_COLS   40     /* TD2: the Dreamcast's dialogue box holds 41 columns beside a portrait (250 px of 6 px) */
#else
#define TALK_BUBBLE_COLS   26     /* a bubble's columns at 320x240 (the census runs at this, the stricter value) */
#endif
#define TALK_BUBBLE_LINES  3

/* ---- the talk memory chunk 'TALK' (T6 owns it; T0 fixes its size) ---------
 * [u8 version][u8 count][u16 day_of_last] then count x 12 B entries
 * {u16 id; u16 day; u16 topic_a; u16 topic_b; u8 flags; u8 likes_known;
 *  u16 spare}, at most TALK_MEM_MAX (LRU). Entry layout is unchanged:
 * existing smaller saves load without migration. The cap varies by port,
 * with Dreamcast retaining deeper social history. A full table protects
 * unfinished requests from eviction. */
#if defined(DOOMO_PLATFORM_DC)
#define TALK_MEM_MAX       128
#elif defined(DOOMO_PLATFORM_PS1)
#define TALK_MEM_MAX       64
#elif defined(DOOMO_PLATFORM_GBA)
#define TALK_MEM_MAX       32
#else
#define TALK_MEM_MAX       48
#endif
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
/* TM2d (design 4, step 1): the nearest known concept to an unknown word, by edit distance 2 (distance 1 for a
 * word of 4 letters or less) or by a shared stem of 4 letters. Returns the distance (1, 2; 3 for a stem) and
 * *concept, or 0 when none is near. *unsafe = 1 when any word of the lexicon within edit distance 1 is a
 * register, hate, sexual, minor, distress or real-world word (E4: such a word is never echoed); a guess is
 * only ever a concept that carries no such flag. Native and Dreamcast only (a few thousand records). */
int  talk_lex_near(const char *word, uint16_t *concept, int *unsafe);
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
       /* (T6) the drama manager's lines (talk_gen_entry; grammar/<lang>/beats_lines.tlk) */
       TGE_BEAT,     /* a beat's goal line: rules beat.<name>, each guarded beat=<name> */
       TGE_PROD,     /* after two idle chats: "You're very quiet, {you}."     */
       TGE_EXCUSE,   /* the excuse of one who walks out                       */
       TGE_GRUMBLE,  /* cut off by another resident: "I was TALKING."         */
       TGE_JAB,      /* an argument, step 1 (the only step with a child here) */
       TGE_ROW,      /* step 2                                                */
       TGE_ULTIMATUM,/* step 3: the player is told to take a side             */
       TGE_MAKEUP,   /* after a mediation that worked                         */
       TGE_KIDASK,   /* a child: "Are you guys fighting?" (the room cools)    */
       TGE_FOLLOW,   /* the one who answered hands the talk back to the player */
       /* TM2d: the connective lines (grammar/<lang>/depth_*.tlk, hooks_*.tlk, unk_*.tlk) */
       TGE_REASON,   /* depth 1: a clause that gives the reason for a stance (a part of the answer line) */
       TGE_STORY,    /* depth 2: the bridge into a story, then the fact (a second line)                  */
       TGE_SECRET,   /* depth 3: the reveal of a like, a fear, the hated food or a small problem          */
       TGE_UNK,      /* the reply to a line the parser did not understand: guess, name or ask             */
       TGE_HOOK,     /* layer d: the question, the offer, the call or the acknowledgement after a reply   */
       TGE_ACT,      /* TM2e: the line that says a sim action is being done (act.start, act.ask_item, act.call) */
       TGE_QUEST,    /* TQ: a request and its answers (quest.<phase>.<kind>; talk_gen_quest) */
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
    TGP_KIND,                        /* TM2c: the plan's reply kind, TK_* (kind=refuse) */
    TGP_TRAIT,                       /* TM2c: the speaker has trait TT_* in this room (trait=shy); edgy
                                        traits never hold with a non-adult speaker or in a kid room */
    TGP_DEPTH,                       /* TM2d: the plan's topic depth, 0 to 3 (depth=2): the line is the
                                        reason (1), the story (2) or the secret (3) of the thread */
    TGP_HOOK,                        /* TM2d: the plan's hook, TH_* (hook=offer) */
    TGP_SECRET,                      /* TM2d: which secret a depth 3 line tells, TSEC_* (secret=fear) */
    TGP_UNK,                         /* TM2d: how an unknown line is answered, TU_* (unk=name) */
    TGP_QKIND,                       /* TQ: the request's kind, TQK_* (qkind=fetch) */
    TGP_QPHASE,                      /* TQ: the phase of the request, TQP_* (qphase=ask) */
    TGP_COUNT
};

/* One conversation as the generator needs it between lines (T6 keeps it in
 * its TalkState). talk_gen_begin() fills it. */
#ifdef DOOMO_PLATFORM_GBA
#define TALK_NO_UNK 1              /* the GBA has no typed line: TalkConv keeps no unknown-word state (see TC_* below) */
#endif
#define TPLAIN_NOYES 0x10u         /* TalkConv.plain bit 4: the plan takes no accept / agree kind (a cap or a rule forbids the action it would promise) */
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
    uint8_t  plain;                              /* bit 0: the core part only, no opener, callback, tag or hand-off (tests; a short UI preview);
                                                    bits 1-3: a forced plan kind, TK_* (the creator's preview; 0 none);
                                                    bit 4 (TPLAIN_NOYES): TM2e, the plan may not say yes (the cap or rule of the action it would promise is spent) */
    uint8_t  care;                               /* 1: the care window (K-11, TDF_CARE): the next answer is the sincere bank, no joke, no reaction */
    TalkAudience aud;                            /* talk_safe_ctx() of the room       */
    uint16_t recent[TALK_PARTS_MAX][TALK_NOVELTY]; /* novelty rings: alternatives used, newest first */
    uint16_t room_recent[TALK_ROOM_NOVELTY];     /* the room's last core alternatives, any speaker (no line twice) */
    uint16_t mem_topic[TALK_PARTS_MAX];          /* talk memory (T6): last topic      */
    uint8_t  mem_flags[TALK_PARTS_MAX];          /* talk memory (T6): TMF_*           */
    /* TM2d: topic depth, hooks, the unknown word (23 + 12 B; the house scratch, nothing saved but mem_dep) */
    uint8_t  dep[TALK_PARTS_MAX];                /* the depth state of each participant (TDEP_*)                    */
    uint16_t dep_tw[TALK_PARTS_MAX];             /* ... the thread's topic and the last reply kind                  */
    uint8_t  mem_dep[TALK_PARTS_MAX];            /* talk memory: bits 0-1 the level reached last time, 2-5 the secrets told (TalkMem.spare) */
    uint8_t  engage;                             /* the answering resident's engagement (the drama manager sets it before the answer; 55 when unset) */
    uint16_t more;                               /* a second line the last answer asks for: bits 0-1 TMORE_*, 2-3 the slot, 8-15 the player's act; 0 none */
#ifndef TALK_NO_UNK
    uint8_t  unk_st;                             /* the unknown word's state, packed (TC_UNK_MODE, TC_TAUGHT, TC_UNK_SAID below)        */
    uint16_t unk_guess;                          /* the nearest known concept to the unknown word (a real concept), 0 none */
    char     unk[18];                            /* the word to name, lower case (E1 to E6 held), else empty      */
    uint16_t taught_hash[4];                     /* the ring of words the player has named a kind for: hash16 (never saved)  */
    uint8_t  taught_root[4];                     /* ... and the category root they were said to be                */
#endif
} TalkConv;
#ifndef TALK_NO_UNK
/* TalkConv.unk_st: bits 0-2 how the next answer to an unknown line goes (TU_*; set by talk_gen_unk_prepare), bits 3-5 the
 * ring index + 1 of the unknown word the player has been asked the kind of (0 none), bit 6 a word was named this talk (E7: once) */
#define TC_UNK_MODE(c)        ((unsigned)((c)->unk_st & 7u))
#define TC_SET_UNK_MODE(c, m) ((c)->unk_st = (uint8_t)(((c)->unk_st & ~7u) | ((unsigned)(m) & 7u)))
#define TC_TAUGHT(c)          ((unsigned)(((c)->unk_st >> 3) & 7u))
#define TC_SET_TAUGHT(c, v)   ((c)->unk_st = (uint8_t)(((c)->unk_st & ~0x38u) | (((unsigned)(v) & 7u) << 3)))
#define TC_UNK_SAID(c)        ((unsigned)(((c)->unk_st >> 6) & 1u))
#define TC_SET_UNK_SAID(c)    ((c)->unk_st = (uint8_t)((c)->unk_st | 0x40u))
#else
/* The GBA has no typed line (talk_scene.c TALK_TYPING): no unknown word reaches Talk there and TalkConv keeps none of its
 * state (33 B of the GBA's EWRAM: the TM2d fields are offset, docs/GBA.md ledger). */
#define TC_UNK_MODE(c)        ((void)(c), 0u)
#define TC_SET_UNK_MODE(c, m) ((void)(c), (void)(m))
#define TC_TAUGHT(c)          ((void)(c), 0u)
#define TC_SET_TAUGHT(c, v)   ((void)(c), (void)(v))
#define TC_UNK_SAID(c)        ((void)(c), 0u)
#define TC_SET_UNK_SAID(c)    ((void)(c))
#endif

/* ---- T9: the Talk screen's pointer (core/src/talk_scene.c; docs/TALK.md "The pointer") ------------
 * Only where DOOMO_POINTER (input.h): a console build declares and links none of it. The view is for the
 * runner's expect-talk and the pointer test. talk_scene_view: 0 when no Talk screen is open; page 0 replies,
 * 1 acts of a category, 2 topics, 3 who joins in, 4 options. talk_scene_find: the row starting with a text, -1. */
#include "input.h"
#if DOOMO_POINTER
int talk_scene_view(int *page, int *sel, int *top, int *to, int *nrows, int *busy, char *you);
int talk_scene_find(const char *prefix);
const char *talk_scene_row(int i, char *buf48);
int talk_scene_person_x(int i);
#endif

/* ---- T6: talk memory (core/src/talk_save.c; docs/TALK.md 6e) ------------------
 * What a resident remembers of the player's talks: the 'TALK' chunk's table
 * (TALK_MEM_MAX entries, the most recently used first; an overflow evicts
 * the last). Ids are a resident, the hero or a family pool id; the pet and
 * anyone who is gone have no entry. Every call first clears
 * TMF_REFUSED_TODAY when the town day has changed. The chunk itself is
 * registered only where the build defines DOOMO_HAS_TALK (the link-time
 * twin of PLATF_TALK: the native host today); without it this is a plain
 * table nobody saves, and --gc-sections drops it where nobody calls it. */
#define TALK_MEM_VERSION 1            /* the registry's chunk version, and payload byte 0 of a memory-only chunk */
#define TALK_MEM_VERSION_TRAITS 2     /* legacy payload with a trait tail: high likes bits were unused */
#define TALK_MEM_VERSION_EVENTS 3     /* memory-only payload, with shared-event flags in likes_known */
#define TALK_MEM_VERSION_EVENTS_TRAITS 4 /* memory and traits, with shared events; outer registry stays v1 */
#define TALK_SAVE_WORST (TALK_MEM_WORST + 1 + 6 * 24)   /* + the trait tail: 1 + 6 x TALK_TRAIT_CUSTOM_MAX */
typedef struct TalkMem {
    uint16_t id;                  /* a resident, the hero or a pool id            */
    uint16_t day;                 /* the town day (low 16 bits) of the last talk  */
    uint16_t topic_a, topic_b;    /* the last topic and the one before (12 bits)  */
    uint8_t  flags;               /* TMF_* (the high nibble is the running conversation's, never saved) */
    uint8_t  likes_known;         /* bits 0..4: known likes; bits 5..7: shared-event memories */
    uint16_t spare;               /* 0                                            */
} TalkMem;                        /* TALK_MEM_ENTRY = 12 B                        */
/* Entries held, and entry i (0 the most recent), NULL past the end. */
int  talk_mem_count(void);
/* Lightweight status for UI and tests: capacity and pinned unfinished
 * commitments. Neither query creates entries or reorders the LRU. */
int  talk_mem_capacity(void);
int  talk_mem_pinned_count(void);
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
/* Three compact, persistent, factual shared-event markers in the high
 * unused bits of TalkMem.likes_known. These describe what truly happened,
 * NOT a transcript, invited destination, or completed travel.
 * Existing saves and the 12-byte entry size are unchanged. */
#define TALK_SHARED_PLAYED  0x20u
#define TALK_SHARED_GIFTED  0x40u
#define TALK_SHARED_INVITED 0x80u
#define TALK_SHARED_ALL     0xE0u
void talk_mem_note_shared(int id, unsigned event_flags);
int  talk_mem_shared(int id, unsigned event_flags);
void talk_mem_know_like(int id, int which);
int  talk_mem_like_known(int id, int which);
/* TQ-QUESTS: the request id has put to the player, kept in the high bits of the memory entry (talk_save.c; no byte
 * more in the chunk). get: 1 when there is one (kind TQK_*, state TQS_*, days left 0..TQ_DAYS, bonus 0/1; a pointer
 * may be NULL). set: touches id and writes it, kind 0 clears it; 0 for nobody. talk_quest.c is the one caller. */
unsigned talk_mem_pace(void);                       /* TQ: the spare header byte of the table: the pace of requests (talk_quest.c; not saved) */
void talk_mem_set_pace(unsigned v);
int  talk_mem_quest_get(int id, int *kind, int *state, int *days, int *bonus);
int  talk_mem_quest_set(int id, int kind, int state, int days, int bonus);
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
#define TLF_MORE     0x80u          /* TM2d: the second line of one answer (a story or a secret) by the same speaker */
typedef struct TalkLine {
    char     text[TALK_LINE_MAX];   /* the bubble text (ui_dialog line codes kept)      */
    int8_t   who;                   /* participant slot                                 */
    int8_t   to;                    /* the slot addressed; -1 the player; -2 everyone    */
    uint8_t  act;                   /* the reply act (TA_*)                             */
    uint8_t  stance : 4;            /* TST_*                                            */
    uint8_t  action : 4;            /* TM2e: TACT_* the line says it is doing (0 none); the drama manager sets it with the event */
    uint8_t  face;                  /* EXPR_* + 1 of a [FACE] code, 0 none              */
    uint8_t  flags;                 /* TLF_*                                            */
    uint16_t topic;                 /* what it is about, TC_NONE                         */
} TalkLine;

/* ---- TM2c: the plan (core/src/talk_plan.c; docs/TALK.md 6m; design TALK_MULTILEVEL 2.1, 2.2) ----
 * The reaction KIND of an answer line (layer a), chosen once per line before the core is picked, from the
 * player's act, the stance, the relationship, the mood, the effective voice, the traits and the liked and
 * disliked topic roots. The core is then picked among the rules whose reply act serves that kind (acts.tsv,
 * column KINDS); if none passes the guards the filter drops and the miss is counted (TPL_MISS). */
enum { TK_NONE = 0, TK_ACCEPT, TK_AGREE, TK_DENY, TK_REFUSE, TK_DEFLECT, TK_COUNTER, TK_ASKBACK, TK_COUNT };
#define TKM(k) (1u << (k))                 /* a kind as a bit of a uint8_t mask */
enum { TPL_MISS = 1,                       /* the soft filter found no rule and dropped (counted by the census) */
       TPL_REASON = 2,                     /* the kind goes against the lean for a reason (relationship, mood, traits) */
       TPL_KIDSAFE = 4,                    /* a minor is in the room: the cool reply acts are out, edgy traits inert */
       TPL_FORCED = 8,                     /* the kind was forced (the creator's preview) */
       TPL_OPEN = 16 };                    /* no kind won by a margin: the plan names a kind but filters nothing */
typedef struct TalkPlan {
    uint8_t kind;                          /* TK_*, 0: no plan (a safety act, grief, the pet, an unknown act) */
    uint8_t allowed;                       /* TKM() mask of the kinds acts.tsv allows after the player's act   */
    uint8_t flags;                         /* TPL_*                                                            */
    int8_t  lean;                          /* -2..+2: the stance as the speaker sees it                        */
    uint8_t eff[5];                        /* the effective sliders (stored + trait deltas), PERS_* order      */
    uint8_t next[2];                       /* the next best kinds (kind | 0x80 when it goes against the lean for a
                                              reason), tried when no rule of `kind` passes the guards; 0 none   */
    uint8_t depth;                         /* TM2d: topic depth of the reply, 0 surface .. 3 secret (TDEP_*)   */
    uint8_t content;                       /* TM2d: TCN_*, what the b part says                                */
    uint8_t hook;                          /* TM2d: TH_*, the layer d hook                                     */
} TalkPlan;                                /* 14 B, on the stack; also in TalkTrace (tools only) */

/* What the plan reads (talk_plan_kind is a pure function of this: a test can drive it without a database). */
typedef struct TalkPlanIn {
    uint8_t act;                           /* the player's act (TA_*)                                      */
    uint8_t allowed;                       /* TKM mask of the kinds after it (acts.tsv)                    */
    uint8_t dflt;                          /* TKM mask of the kinds the default reply act serves (base 3)  */
    int8_t  lean;                          /* -2..+2                                                       */
    uint8_t rel, mood;                     /* TRB_*, TMB_*                                                 */
    uint8_t stage;                         /* the speaker's life stage                                     */
    uint8_t kid_room;                      /* a minor speaks, is addressed or listens                      */
    uint8_t consent;                       /* romance acts: the resident says yes (TalkConv.consent)       */
    uint8_t to_minor;                      /* the one addressed is a minor                                 */
    int8_t  topic_pref;                    /* +1 the topic is in a liked root, -1 a disliked root, 0 none  */
    uint8_t eff[5];                        /* the effective sliders                                        */
    int8_t  accept, deny;                  /* the trait bias of the room's trait set (talk_trait_bias)     */
    uint8_t margin;                        /* a kind binds (filters the core) when it wins by this much; 0: always */
    uint32_t seed;                         /* the plan's own noise: the conversation seed and the turn     */
} TalkPlanIn;
/* Choose the kind (TK_*) and fill `out` (kind, allowed, flags, lean, eff); 0 when in->allowed is empty. */
int  talk_plan_kind(const TalkPlanIn *in, TalkPlan *out);
/* The soft filter found no rule of p->kind: take the next best kind (1), or 0 when none is left. */
int  talk_plan_next(TalkPlan *p);
/* The kinds a reply act serves, as a TKM() mask (acts.tsv KINDS; 0 for an act no kind serves), and the
 * kinds allowed after a player's act. Read from the open database. */
int  talk_gen_kinds_of(int reply_act);
int  talk_gen_kinds_after(int act);
int  talk_gen_kinds_reply(int player_act, int reply_act);   /* TM2F-Q8: kinds_of read against the player's act */
int  talk_gen_reply_side(int player_act, int reply_act);    /* +1 for the topic, -1 against, 0 none */
/* The speaker's effective sliders in the room `aud` describes (stored + trait deltas, the edgy traits
 * dropped for a minor speaker or a minor in the room), PERS_COUNT bytes; 0 if nobody has that id. */
int  talk_gen_eff_persona(int id, int minor_room, uint8_t out[5]);
/* ---- TM2d: topic depth, hooks and the unknown word (docs/TALK.md 6n; design TALK_MULTILEVEL 2.3 to 2.5, 3, 4) ----
 * Depth: 0 surface (a one-line answer), 1 opinion plus its reason, 2 a story of the speaker's, 3 a secret.
 * The state of one participant is a byte and a word (TalkConv.dep[who], dep_tw[who]):
 *   dep:    bits 0-1 level now, bits 2-4 thread turns so far (saturates at 7), bit 5 the hook was used on this thread
 *   dep_tw: bits 0-11 the thread's topic (a concept id), bits 12-14 the last reply kind (TK_*)
 * talk_depth_step() is a pure function of a TalkDepthIn: tests drive it without a database. */
enum { TCN_NONE = 0, TCN_STANCE, TCN_REASON, TCN_STORY, TCN_SECRET, TCN_NAME, TCN_COUNT };
enum { TMORE_NONE = 0, TMORE_STORY, TMORE_SECRET };
/* how an unknown line is answered (unk.* rules, TGP_UNK): a guess from the word network, the word named back,
 * a plain ask, an ask that echoes nothing, the answer to "what kind of thing is it" */
enum { TU_NONE = 0, TU_GUESS, TU_NAME, TU_ASK, TU_NOECHO, TU_TAUGHT, TU_COUNT };
enum { TH_NONE = 0, TH_ASK, TH_OFFER, TH_CALL, TH_ACK,
       TH_START, TH_ITEM, TH_GIFT, TH_CALLER,      /* TM2e: the action lines (talk_gen_action): start an activity, ask for an item, give a gift, call someone over */
       TH_COUNT };
#define TDEP_LEVEL(b)    ((b) & 3u)
#define TDEP_TURNS(b)    (((b) >> 2) & 7u)
#define TDEP_HOOKED      0x20u
#define TDEP_TOPIC(w)    ((w) & 0xFFFu)
#define TDEP_LAST(w)     (((w) >> 12) & 7u)
/* the secrets a participant has told (TalkMem.spare bits 2-5 and TalkConv.mem_dep bits 2-5), one per kind */
enum { TSEC_LIKE = 0, TSEC_FEAR, TSEC_HATE, TSEC_PROBLEM, TSEC_COUNT };
#define TDEP_MEM_LEVEL(m)   ((m) & 3u)
#define TDEP_MEM_SECRET(k)  (1u << (2 + (k)))
typedef struct TalkDepthIn {
    uint8_t act;                           /* the player's act                                                */
    uint8_t rel;                           /* TRB_*                                                           */
    uint8_t stage;                         /* the speaker's life stage                                        */
    uint8_t kid_room;                      /* a minor speaks, is addressed or listens                         */
    uint8_t engage;                        /* 0..100                                                          */
    uint8_t level;                         /* the bond with the player, rel_value clamped to 0..100           */
    uint8_t root;                          /* the topic's root now (TCR_*), 0 none (no topic given)           */
    uint8_t thread_root;                   /* the root of the thread's topic (TCR_*), 0 no thread yet          */
    uint8_t same;                          /* the topic is the thread's or one hop from it                    */
    uint8_t grief;                         /* the topic is a grief topic                                      */
    uint8_t shy, paranoid, outgoing, loyal, gloomy;   /* trait flags (effective: inert edgy ones already dropped) */
} TalkDepthIn;
/* The deepest level the relationship, the thread, the engagement and the traits allow (design 3.1): 0..3.
 * `turns` is the number of thread turns before this one. */
int  talk_depth_gate(const TalkDepthIn *in, int turns);
/* One answer line's step: rise by one at most when the act is a thread act on the thread's topic and the gate
 * holds; hold on a change of topic inside the root; fall by one after an insult, a mock, a dismissal or a
 * threat; reset on another root. Writes the new state (*b the byte, *tw the topic word, topic = the topic now,
 * 0 none) and returns the level of this reply. */
int  talk_depth_step(const TalkDepthIn *in, uint8_t *b, uint16_t *tw, uint16_t topic);
/* A walk-out or a K-5 refusal: back to the surface (the thread's topic is kept). */
void talk_depth_reset(uint8_t *b);
/* 1 if the act keeps a thread going (ask_about, ask_opinion, ask_feeling, tell, opine, continue, agree,
 * sympathize, compliment, comfort). */
int  talk_depth_thread_act(int act);
/* The hook a reply wants (TH_*): required after a counter and after the refusal of a request, else 0. Pure. */
int  talk_hook_wanted(int act, int kind, int romance, int kid_room, int sincere);
/* The cool reply acts a kid room never gets from the plan: tease, mock, insult, threaten. */
#define TALK_COOL_REPLY(a) ((a) == TA_TEASE || (a) == TA_MOCK || (a) == TA_INSULT || (a) == TA_THREATEN)

/* What went into a line (tests, the census: Q1-Q4; NULL in the game). */
enum { TFK_NONE = 0, TFK_NAME, TFK_CONCEPT, TFK_TEXT };
#define TPART_OPEN    0x01u
#define TPART_CORE    0x02u
#define TPART_CB      0x04u
#define TPART_TAG     0x08u
#define TPART_HANDOFF 0x10u
#define TPART_REASON  0x20u         /* TM2d: the reason clause of a depth 1 answer */
#define TPART_HOOK    0x40u         /* TM2d: the hook of layer d (a question, an offer, a call, an acknowledgement) */
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
    TalkPlan plan;                  /* TM2c: the plan of an answer line (kind 0: none); eff is set for every line */
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
/* TM2d: the second line of the last answer (a story at depth 2, a secret at depth 3), by the same speaker, when
 * the answer asked for one (TalkConv.more); 1 and `out` filled, or 0 (nothing fits, no fact, no secret left).
 * Tells at most one secret a talk and marks it (TalkConv.mem_dep); the drama manager turns that into TEV_LEARNED. */
int  talk_gen_more(TalkConv *c, TalkLine *out, TalkTrace *tr);
/* TM2d: what the answer to the unknown line u will do (design 4): the guess from the word network, the word
 * named back (only when every fence E1 to E7 holds) or a plain ask. Run once per player turn, after the gate,
 * before the answer is made; it only sets TalkConv.unk_st (the mode) / unk / unk_guess. */
void talk_gen_unk_prepare(TalkConv *c, const TalkUtter *u);
/* The word the generator may name back from u (E1 to E6 and the kid-room length, not E7), lower case, into
 * out[18]: its length, or 0 when it must not be echoed. The adversarial-corpus test and the scene use it. */
int  talk_gen_unk_word(const TalkConv *c, const TalkUtter *u, char out[18]);
/* The speaker's stance on a concept: their likes +3, their hated food and
 * fear -3, Hellvile-born flips holy things, plus a fixed personal lean. */
int  talk_gen_stance(int id, uint16_t topic);
/* May this concept be said in this room (age class, register, a child here)? */
int  talk_gen_concept_ok(uint16_t concept, const TalkAudience *aud);
/* May the act ask or say something about the current topic? */
int  talk_gen_topic_act(int act);
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

/* (T6) One line from a fixed entry of the grammar (TGE_*: a beat's goal
 * line, idle chatter, a prod, an excuse, an argument step, the K-7 ending)
 * said by `who` to `to` (a slot, -1 the player, -2 everyone); `act` is what
 * it follows (the pact= / act= guards; TA_NONE). The core part alone. The
 * same lint (S3) as every line. Returns 1; 0 when no rule of the entry may
 * be said here (out->text is then empty, or a safe line with TLF_FALLBACK).
 * The pet never says one. A grief topic (K-10) is not passed on. */
int  talk_gen_entry(TalkConv *c, int who, int to, int entry, int act, uint16_t topic, TalkLine *out,
                    TalkTrace *tr);
/* (T6) A reaction of `who` to the line `prev` of another resident: an act
 * acts.tsv allows after prev->act (agree, disagree, laugh, tease, joke, a
 * backchannel), chosen by stance clash and persona; after a sincere line a
 * comfort line or nothing (K-10). Returns 1, 0 for no reaction. */
int  talk_gen_react(TalkConv *c, int who, const TalkLine *prev, uint16_t topic, TalkLine *out, TalkTrace *tr);
/* TM2e: the line that says a sim action is being done (tact TACT_START / GIFT / ASKITEM / CALL): the rules act.start,
 * act.ask_item, act.call and hook.gift, picked by the guard hook=. topic is the venue concept for TACT_START (0: no
 * venue named). The caller (the drama manager) adds the event only when this returns 1 with no TLF_SAFE / TLF_FALLBACK. */
int  talk_gen_action(TalkConv *c, int who, int to, int tact, uint16_t topic, TalkLine *out, TalkTrace *tr);
/* The relationship band (TRB_*) of a towards b as the generator sees it (b may be the player's id). */
int  talk_gen_rel_band(int a, int b);

/* =========================================================================
 * T6: the drama manager (core/src/talk_dm.c; PS1, Dreamcast, native). Design
 * 5.1-5.4, docs/TALK.md "The drama manager". It is the POLICY over the
 * generator: which beat the room is in, who answers, who reacts, who cuts
 * in, who argues, who walks out. Lines are still made by talk_gen_line /
 * talk_gen_react / talk_gen_entry, the T4 gate runs first on every player
 * utterance and the S3 lint on every line.
 *
 * SAFETY (in C, whatever beats.tlk says): TBEAT_ROMANCE_ADULT is never
 * chosen with a minor or the pet in the room, with a minor avatar, or when
 * talk_safe_romance_ok() says no; no resident line the drama manager starts
 * is a romance act (every one goes through one check, and a romance beat
 * line is only said by the adult the gate passed, to the adult player); an
 * argument stops at the jab when anyone here is not an adult; the room's
 * audience is the one worked out at the start and never gets less strict
 * (a child who walks out leaves a kids' room behind). Romance consent
 * (TalkConv.consent) is asked of the sim (talk_dm_consent); the drama
 * manager never writes romance state, and nothing else of the sim either:
 * what a session did is handed to talk_dm_end() for T6-OUT to apply.
 *
 * No static RAM: everything between calls is in TalkState (the house's
 * scene scratch). One call makes at most one line, so the PS1 can spread an
 * exchange over frames; nothing points into plat_save_scratch between calls.
 *
 *   talk_dm_begin(&s, avatar, ids, n, seed);
 *   talk_dm_say(&s, &u);                       // the player's turn: gate, beat, plan
 *   while (talk_dm_step(&s, NULL)) show(&s.line[s.nlines - 1]);
 *   if (talk_dm_tick(&s, frames)) while (talk_dm_step(&s, NULL)) ...;   // idle chatter
 *   talk_dm_end(&s, &summary);                 // T6-OUT applies it to the sim
 * ========================================================================= */
#define TALK_STATE_MAX      1536   /* sizeof(TalkState) at most (design 5.1, 6.4)  */
#define TALK_HIST           16
#define TALK_EVENTS_MAX     8
#define TALK_ENGAGE_START   55
#define TALK_ENGAGE_AWAY    25     /* below: they turn away                         */
#define TALK_ENGAGE_LEAVE   15     /* below for 2 turns: an excuse and a walk-out   */
#define TALK_ENGAGE_LEAN    85     /* above: they lean in                           */
#define TALK_TENSION_MAX    10
#define TALK_TENSION_ARGUE  6      /* an argument goes past the jab from here       */
#define TALK_IDLE_FRAMES    240    /* 8 s at 30 fps without the player              */
#define TALK_IDLE_CHATS     2      /* idle chats before someone prods the player    */
#define TALK_INTERRUPT_GAP  3      /* exchanges between two interruptions           */
#define TALK_REL_D_MAX      12     /* a session's bond change with the player (5.5) */
#define TALK_PAIR_D_MAX     8      /* a session's bond change of a pair (5.5)       */
#define TLF_CUT      0x40u         /* another resident cut this line off ("-")      */

/* TalkPart.flags */
#define TPF_LEFT      0x01u        /* walked out                                    */
#define TPF_REFUSED   0x02u        /* said no to romance today (K-5); the house sets it from talk memory */
#define TPF_AWAY      0x04u        /* turned away (engage < TALK_ENGAGE_AWAY)       */
#define TPF_LEAN      0x08u        /* leaning in (engage > TALK_ENGAGE_LEAN)        */
#define TPF_SPOKE     0x10u        /* spoke in the last exchange                    */
#define TPF_PET       0x20u        /* the pet: no engagement, never leaves          */
#define TPF_PROBLEM   0x40u        /* their problem was talked through              */
/* TalkPart.stage_class */
enum { TSC_ADULT = 0, TSC_MINOR, TSC_PET };

typedef struct TalkPart {             /* one participant, 12 B (the novelty ring is TalkConv's) */
    int16_t  id;                      /* resident id, pool id or TALK_PET_ID           */
    uint8_t  stage_class;             /* TSC_* (talk_safe_minor, cached)               */
    uint8_t  engage;                  /* 0..100                                        */
    int8_t   mood_d;                  /* mood change this session                      */
    int8_t   rel_d_player;            /* bond change with the player this session      */
    uint8_t  stance;                  /* TST_* on the current topic                    */
    uint8_t  turns_ignored;
    uint8_t  flags;                   /* TPF_*                                         */
    uint8_t  low_turns;               /* turns in a row under TALK_ENGAGE_LEAVE        */
    uint8_t  actp, actw;              /* TM2e: the last invitation's plan (TACT_ACCEPT / TACT_REFUSE) and why not (TAW_*) */
} TalkPart;

/* what a session did (TalkState.ev, TalkSummary.ev); a / b are slots */
enum { TEV_NONE = 0,
       TEV_ULTIMATUM,     /* a row between a and b reached the ultimatum             */
       TEV_MEDIATED,      /* the player made peace between a and b (T6-OUT: sim_make_up) */
       TEV_SIDE_TAKEN,    /* the player took a's side against b                      */
       TEV_WALKOUT,       /* a walked out                                            */
       TEV_PROBLEM,       /* a's problem was talked through (T6-OUT: sim_problem_resolve) */
       TEV_GREAT_NIGHT,   /* everyone stayed and had a good time (talk_dm_end)       */
       TEV_ROMANCE_YES,   /* a said yes to the player's romance act b (TA_*); the gate
                             passed (T6-OUT: sim_confess for CONFESS / ASK_OUT)       */
       TEV_ROMANCE_NO,    /* a said no: refused for the day (K-5; talk memory)       */
       TEV_K7,            /* the conversation ended on K-7: locked for the day       */
       TEV_HATE,          /* the player was refused for hate (K-8): a bond penalty   */
       TEV_LEARNED,       /* TM2d: a told a secret the player did not know: b = the like (0 food, 1 hobby, 2 music,
                             3 fear, 4 hated food; 5 a small problem). T6-OUT: talk_mem_know_like               */
       TEV_ACT_INVITE,    /* TM2e: a accepted the player's invitation and will go to venue b (ACT_*): sim_nudge_activity */
       TEV_ACT_GIFT,      /* TM2e: a gives the player the item b (ITEM_*): inv_add                                     */
       TEV_ACT_ASK,       /* TM2e: a asks the player for the item b: sim_problem_raise(PROB_WANTS_ITEM)               */
       TEV_ACT_CALL,      /* TM2e: a calls someone over (TalkSummary.call_to is who): the scene's invite              */
       TEV_QUEST_ACCEPT,  /* TQ: the player took a's request of kind b (TQK_*): talk_quest_add, the quest log         */
       TEV_QUEST_DECLINE, /* TQ: the player turned a's request of kind b down                                         */
       TEV_QUEST_DONE,    /* TQ: the player handed in a's request of kind b: the reward, the bond, the news          */
       TEV_QUEST_LAPSED,  /* TQ: a's request of kind b ran out of days unanswered: a small disappointment              */
       TEV_COUNT };
typedef struct TalkEvent { uint8_t kind, a, b, turn; } TalkEvent;

/* TalkHist.flags: what kind of line it was */
#define THF_PLAYER    0x01u        /* the player's turn (who = 0xFF)                */
#define THF_REACT     0x02u        /* a reaction to the line before                 */
#define THF_BEAT      0x04u        /* a beat's goal line                            */
#define THF_INTERRUPT 0x08u        /* it cut the line before                        */
#define THF_IDLE      0x10u        /* idle chatter or the prod                      */
#define THF_ARGUE     0x20u        /* a jab, a row, an ultimatum                    */
#define THF_LEAVE     0x40u        /* an excuse: the speaker walks out              */
#define THF_SINCERE   0x80u        /* a refusal, distress, grief or K-7 line        */
typedef struct TalkHist { uint8_t who, act, flags; int8_t to; uint16_t topic; } TalkHist;      /* 6 B (TM2d: the unused `tmpl` word is gone) */

/* TalkState.flags */
#define TDF_OVER      0x01u        /* the conversation is over                      */
#define TDF_K7        0x02u        /* ... on K-7                                    */
#define TDF_FAREWELL  0x04u        /* the player said goodbye                       */
#define TDF_KIDSAFE   0x08u        /* a kids' room for the rest of the talk (K-2; KID_MOMENT) */
#define TDF_NO_BEATS  0x10u        /* the database has no TB_BEATS: small talk only */
#define TDF_CARE      0x20u        /* the care window is open (K-11): after a distress line or a grief answer the next
                                      TALK_CARE_TURNS player turns are answered by the sincere bank alone */
#define TALK_CARE_TURNS 4          /* player turns of care after a distress line or a grief answer */

typedef struct TalkState {          /* <= TALK_STATE_MAX, in the house's scene scratch */
    TalkConv  conv;                 /* the generator's conversation (beat, consent, novelty, audience) */
    TalkPart  part[TALK_PARTS_MAX];
    uint8_t   nparts;
    uint8_t   beat, beat_step, beat_turns;   /* TBEAT_*, goal lines said, player turns in it */
    uint8_t   tension, peak_tension, peak_turn, turn;
    uint8_t   flags;                /* TDF_*                                          */
    uint8_t   gate;                 /* TG_* of the last utterance                     */
    uint8_t   care_left;            /* player turns of care still to come (TDF_CARE while > 0) */
    int8_t    beat_a, beat_b;       /* the slots the beat is about (the fan, the troubled one, the rivals), -1 */
    uint8_t   arg_step, arg_wait;   /* an argument: 0 none, 1 jab, 2 row, 3 ultimatum; turns it was ignored */
    int8_t    arg_a, arg_b;
    uint8_t   lull;                 /* turns in a row at low tension                  */
    uint8_t   idle_n;               /* idle chats since the player last spoke (3: prodded) */
    uint8_t   q_kind;               /* TQ: the request on the table (TQK_*, 0 none); this byte and q_st fill the struct's two padding holes: no growth */
    uint16_t  idle_frames;
    uint8_t   exchange;             /* exchanges so far (the interruption gap)        */
    uint8_t   last_cut;             /* the exchange of the last interruption          */
    uint8_t   last_act;             /* the player's act before this one               */
    uint8_t   mem_earned;           /* TMF_* aggregate (existing summary format)     */
    uint8_t   mem_to[TALK_PARTS_MAX]; /* TMF_* per addressee; bystanders aren't all insulted */
    uint8_t   nhist, nev;
    uint8_t   nlines;               /* lines of the current exchange so far           */
    uint8_t   nplan, plan_at;       /* the exchange's plan and the next entry         */
    uint8_t   q_st;                 /* TQ: the request's state (TQ_ST_*: the giver's slot, haggled, asked more, closed) */
    uint16_t  beats_used;           /* a bit per TBEAT_* entered this session         */
    uint16_t  beats_recent;         /* a bit per TBEAT_* of the last 2 talks with these people
                                       (novelty; the house sets it from talk memory, 0 = none) */
    uint16_t  call_to;              /* TM2e: resident id + 1 of the one called over (TEV_ACT_CALL), 0 none; the scene lets them in */
    struct { uint8_t kind; int8_t who, to; uint8_t arg; } plan[6];
    uint16_t  topic_stack[TALK_STACK_MAX];   /* newest first                          */
    uint16_t  answered_topic[TALK_PARTS_MAX]; /* latest accepted question or remark each resident answered */
    TalkHist  hist[TALK_HIST];      /* newest first                                   */
    int8_t    pair_d[6];            /* resident pairs (0,1) (0,2) (0,3) (1,2) (1,3) (2,3) */
    TalkEvent ev[TALK_EVENTS_MAX];
    TalkLine  line[TALK_EXCHANGE_MAX];       /* the lines of the current exchange      */
    TalkUtter in;                   /* the player's utterance (after the gate)        */
} TalkState;

/* What a session did, for T6-OUT (talk_outcome.c applies it to the sim with
 * the caps of design 5.5) and T6-MEM (talk memory). Nothing here has been
 * applied yet. */
typedef struct TalkSummary {
    int16_t   avatar;
    int16_t   id[TALK_PARTS_MAX];
    uint8_t   n;
    uint8_t   turns;                /* player turns                                   */
    uint8_t   peak_tension;
    uint8_t   flags;                /* TDF_*                                          */
    uint8_t   engage[TALK_PARTS_MAX];       /* at the end                             */
    uint8_t   pflags[TALK_PARTS_MAX];       /* TPF_* (left, refused, problem)         */
    int8_t    mood_d[TALK_PARTS_MAX];       /* -10..+10                               */
    int8_t    rel_d_player[TALK_PARTS_MAX]; /* -TALK_REL_D_MAX..+TALK_REL_D_MAX        */
    int8_t    pair_d[6];                    /* -TALK_PAIR_D_MAX..+TALK_PAIR_D_MAX      */
    uint16_t  beats_used;
    uint16_t  call_to;              /* TM2e: resident id + 1 of the one called over, 0 none */
    uint16_t  topic[2];             /* the last two topics (talk memory)              */
    uint8_t   mem_flags;            /* TMF_* the player earned (complimented, insulted, promised) */
    uint8_t   nev;
    TalkEvent ev[TALK_EVENTS_MAX];
} TalkSummary;

/* 1 if the open database has the beat table (TB_BEATS). Without it the
 * drama manager still runs: small talk only (TDF_NO_BEATS). */
int  talk_dm_ready(void);
/* Start a conversation (talk_gen_begin inside): the audience, beat ARRIVE,
 * everyone at TALK_ENGAGE_START. Then the house may set talk memory:
 * s->conv.mem_topic / mem_flags, TPF_REFUSED on s->part[k].flags,
 * s->beats_recent. */
void talk_dm_begin(TalkState *s, int avatar, const int *ids, int n, uint32_t seed);
/* The player's turn: the gate (u is copied and rewritten in s->in), the
 * engagement of everyone, the beat, consent, and the plan of the exchange.
 * Returns the gate's TG_*. Then call talk_dm_step() until it returns 0. */
int  talk_dm_say(TalkState *s, const TalkUtter *u);
/* The talk opens (right after talk_dm_begin): the resident says the beat's opening line, then hands
 * the talk to the player with a follow-up. Plans the exchange; then talk_dm_step() until it returns
 * 0. Returns 1 when something was planned. */
int  talk_dm_open(TalkState *s);
/* Return a *real, ready* resident favor during the current conversation.
 * Schedules their generated thank-you (TQP_DONE) and the normal quest-complete
 * outcome event; rewards are not paid until talk_outcome_end() applies them.
 * Returns 0 if the request is not ready, was already handed in this session,
 * the resident left, or a dialogue exchange is still pending. No allocations. */
int  talk_dm_hand_in(TalkState *s, int slot);
/* Time without the player: `frames` at 30 fps. After TALK_IDLE_FRAMES a
 * resident talks to another (TALK_IDLE_CHATS times), then someone prods the
 * player, then nothing more until the player speaks. Returns 1 when an
 * exchange was planned (step it). Do not call it while lines are playing. */
int  talk_dm_tick(TalkState *s, int frames);
/* Make the next line of the planned exchange into s->line[s->nlines++]:
 * 1 a line was made, 0 the exchange is complete. tr: NULL or one trace. */
int  talk_dm_step(TalkState *s, TalkTrace *tr);
/* 1 when the conversation is over (everyone left, K-7, a goodbye). */
int  talk_dm_over(const TalkState *s);
/* Would slot `who` say yes to the player's romance act `act` (K-5)? Asks
 * the gate (talk_safe_romance_ok) and the sim (sim_romance_ok from the
 * target: their preference), then their relationship and persona. Changes
 * nothing. talk_dm_say() sets TalkConv.consent with it. */
int  talk_dm_consent(const TalkState *s, int who, int act);
/* The hook for T6-OUT: close the session (the great-night event, the caps)
 * and copy what it did into out. Applies nothing to the sim. May be called
 * at any time; the conversation is over afterwards. */
void talk_dm_end(TalkState *s, TalkSummary *out);
/* ---- T6-OUT: the outcomes into the sim (core/src/talk_outcome.c; docs/TALK.md 6g) ----
 * The ONLY place where Talk writes to the sim. The caps of design 5.5 are
 * per town day and live in a small ledger (native builds only; saved
 * nothing: a reload forgets today's ledger). */
#define TALK_CAP_REL_DAY    12     /* bond with the player, per resident per town day (+-)  */
#define TALK_CAP_LEVEL_DAY  3      /* friendship level, per resident per day                */
#define TALK_CAP_PAIR_DAY   8      /* resident-resident bond, per pair per day (+-)         */
#define TALK_CAP_MOOD_DAY   12     /* STAT_HAPPY change, per resident per day (+-)          */
#define TALK_CAP_NEWS_DAY   4      /* Talk headlines a town day (one a conversation)         */
#define TALK_CAP_SOCIAL_DAY 12     /* STAT_SOCIAL gain, per resident per day                */
#define TALK_CAP_INVITE_TOWN 3     /* TM2e: accepted invitations a town day (1 a resident a day)   */
#define TALK_CAP_GIFT_TOWN   3     /* TM2e: gifts a resident gives a town day (1 a resident a day) */
#define TALK_CAP_ASK_DAY     1     /* TM2e: asks for an item, per resident per town day            */
typedef struct TalkOutcome {
    int8_t   rel[TALK_PARTS_MAX];  /* the bond change with the player really applied        */
    int8_t   mood[TALK_PARTS_MAX]; /* the STAT_HAPPY change really applied                  */
    int8_t   pair[6];              /* the pair bond change really applied (as pair_d)       */
    uint8_t  level;                /* friendship level gained, all of them                  */
    uint8_t  social;               /* STAT_SOCIAL gained, all of them                       */
    uint8_t  problems;             /* problems resolved (sim_problem_resolve)               */
    uint8_t  made_up;              /* sim_make_up calls                                     */
    uint8_t  confessed;            /* sim_confess calls                                     */
    uint8_t  dating;               /* ... and how many of them the sim said yes to          */
    uint8_t  news;                 /* NT_TALK_* posted, 0 none (one headline a conversation) */
    uint8_t  learned;              /* TM2d: likes, fears or hated foods the player learned (talk_mem_know_like) */
    uint8_t  invited;              /* TM2e: invitations the sim took (sim_nudge_activity)    */
    uint8_t  gifted;               /* TM2e: gifts that moved an item into the inventory      */
    uint8_t  asked;                /* TM2e: PROB_WANTS_ITEM raised                           */
    uint8_t  called;               /* TM2e: call-overs (the scene lets the resident in)      */
    uint8_t  skipped;              /* TM2e: action events a rule or a cap refused at apply (a test fails on it) */
    uint8_t  quests_new, quests_declined, quests_done, quests_lapsed;   /* TQ: requests taken, turned down, handed in, run out */
    uint16_t reward;               /* TQ: H$ paid for the hand-ins */
} TalkOutcome;
/* talk_dm_begin, then the talk memory of the participants (talk_mem_begin:
 * last topic, last-time flags, "refused today" as TPF_REFUSED). */
void talk_outcome_begin(TalkState *s, int avatar, const int *ids, int n, uint32_t seed);
/* Close the conversation: the talk memory (topics, flags, refused today), then
 * talk_dm_end, then talk_outcome_apply. sum may be NULL. */
void talk_outcome_end(TalkState *s, TalkOutcome *out, TalkSummary *sum);
/* Apply a summary to the sim under the daily caps. Never sets a crush or a
 * dating edge: a yes to CONFESS / ASK_OUT only asks sim_confess. */
void talk_outcome_apply(const TalkSummary *sum, TalkOutcome *out);
/* Forget today's ledger (a new town does it by itself: the ledger is keyed by day and town seed). */
void talk_outcome_reset(void);
/* Day-scoped real rendezvous: an accepted invite becomes a pending
 * meeting, not a teleported player or an imaginary completed outing.
 * The story-place interaction passes its concrete venue when the player
 * physically uses that location's door/menu. Returns guest ID, or -1. */
int talk_outcome_meet_ready(int venue);
int talk_outcome_meet_guests(int venue, int *guests, int capacity);
/* The still-pending venue of this particular resident, or -1 if no
 * invitation is outstanding today. A plan may be temporarily unavailable
 * after the resident's schedule changes; use meet_present to confirm they
 * are actually present before offering an on-site action. Read-only. */
int talk_outcome_meet_plan(int resident_id);
int talk_outcome_meet_present(int guest_id, int venue);
/* On-site choice, not a remote Talk action. Both variants consume a single
 * accepted meetup; an invalid venue, an insufficient cafe budget, or a
 * declined dialogue choice has no effect. 0 = chat, 1 = local activity. */
#define TALK_MEET_CHAT     0
#define TALK_MEET_ACTIVITY 1
int talk_outcome_meet_kind(int guest_id, int venue, int kind);
int talk_outcome_meet(int venue);      /* legacy/default: a free chat */
/* A stable, factual, venue-specific response for both Free Play and Story
 * menus. Queried only after a successful meeting, not each frame.
 * Drawn from the resident's actual mood, favorite hobby venue and
 * catchphrase seed. No RNG, save bytes, assets or duplicated scene lines. */
const char *talk_outcome_meet_reply(int guest_id, int venue, int kind);

/* ---- TM2e: sim actions a reply can carry (core/src/talk_act.c, talk_outcome.c; docs/TALK.md 6o; design 6) ----
 * A reply carries at most one action. The rule, in code and tested: a line says it only if the sim will do it
 * (the plan asks talk_outcome_can_in and the sim's own predicate first), and the sim does it only if the line
 * was said (the event is added when the line is made, and talk_outcome_apply applies the event). */
enum { TACT_NONE = 0,
       TACT_ACCEPT,       /* plan: an invitation is accepted and the sim will do it (TalkPart.actp)      */
       TACT_REFUSE,       /* plan: an invitation is refused: a cap, a rule or the sim says no (TalkPart.actw has why) */
       TACT_START,        /* line: "let's go" (act.start): TEV_ACT_INVITE                                */
       TACT_GIFT,         /* line: the resident gives a snack (hook.gift): TEV_ACT_GIFT                  */
       TACT_ASKITEM,      /* line: the resident asks for a food (act.ask_item): TEV_ACT_ASK              */
       TACT_CALL,         /* line: "I'll get someone" (act.call): TEV_ACT_CALL                           */
       TACT_WALKOFF,      /* line: the excuse of one who leaves (TEV_WALKOUT, exists)                    */
       TACT_COUNT };
/* why a plan refused an invitation (TalkPart.actw) */
enum { TAW_NONE = 0, TAW_CAP_DAY, TAW_CAP_TOWN, TAW_RULE, TAW_SIM };
/* The pure rules (talk_act.c; the exhaustive tables are tests). venue is an ACT_* (sim.h); the hour 0..23.
 * minor_room is 1 when a minor speaks, listens or is the player. TA_ASK_OUT (a date) is never an invitation. */
int  talk_act_invite_ok(int act, int venue, int minor_room, int hour);
int  talk_act_item_adult(int item);                    /* 1: an item a minor never gives or gets (love letter, perfume, ring, vape pen, pills, caffeine) */
int  talk_act_gift_ok(int item, int minor_giver, int minor_taker);
/* May `called` join a room? room_minor: a minor is in the new room; partner: the called one is a romance partner
 * (dating or married) of someone there; alive: a living resident of the town; not the pet, not a ghost. */
int  talk_act_call_ok(int room_minor, int partner, int alive);
/* The venue (ACT_*) a topic concept names (a place from the venue list: "park", "Brimstone Bites", ...), -1 none. */
int  talk_act_venue_of(uint16_t topic);
/* The item a resident gives or asks for: their liked food as an item, 0 when it is no item, hated or for adults only. */
int  talk_act_food_item(int id, int minor);
/* May the ledger (and the events s already holds) allow one more of kind (TEV_ACT_INVITE / _GIFT / _ASK) for
 * resident id today? talk_outcome_can counts the ledger only, talk_outcome_can_in also the events of the session. */
int  talk_outcome_can(int kind, int id);
int  talk_outcome_can_in(const TalkState *s, int kind, int id);
/* The reason talk_outcome_can_in says no: TAW_NONE (it can), TAW_CAP_DAY (this resident today), TAW_CAP_TOWN (the town day's cap). */
int  talk_outcome_why_in(const TalkState *s, int kind, int id);
/* The target tension of player turn `turn` (design 5.2: up to a peak at
 * turns 8-10, down by 14-16, then "lively"). */
int  talk_dm_target_tension(int turn);
/* A beat's row of TB_BEATS (tests, tools): 1 ok. */
typedef struct TalkBeatInfo {
    uint8_t  pre;                   /* TBP_*                                          */
    int8_t   tension, after;        /* on entry; when its goals are done / it is resolved */
    uint8_t  goals, turns, bonus, involves, lean_bonus, flags;
    uint16_t handles, hurt, lean;   /* masks of act classes (1 << TAC_*)              */
    uint8_t  resolve[4];            /* acts that resolve it at once (TA_*, 0 none)    */
    uint8_t  lean_act[2];           /* single acts that raise its score too           */
} TalkBeatInfo;
enum { TBP_DEFAULT = 0, TBP_START, TBP_LIKE, TBP_HOUSE, TBP_NEWS, TBP_PROBLEM, TBP_RIVALS, TBP_CLASH,
       TBP_STORY, TBP_LULL, TBP_KID, TBP_ROMANCE, TBP_LATE, TBP_COUNT };
enum { TBI_ALL = 0, TBI_A, TBI_PAIR, TBI_COUNT };   /* who a beat involves: everyone, beat_a, beat_a and beat_b */
#define TBTF_KIDSAFE 0x01u          /* forces the kids' register for the rest of the talk */
#define TBTF_ADULTS  0x02u          /* never with a minor or the pet here (also in C)  */
int  talk_dm_beat_info(int beat, TalkBeatInfo *out);
/* The persona type's favoured and dull topic roots (TB_BEATS; TCR_*, 0 none). */
int  talk_dm_affinity_roots(int type, uint8_t *favours, uint8_t *dull);

/* ---- (T6b) personalities react differently; quick replies ----------------------------------
 * talk_dm_expect: how slot `slot` is likely to take the player's act `act` in tone `tone` about
 * `topic` (TC_NONE: none), as the number the drama manager works with: -10 (deeply offended) to
 * +8 (delighted). It reads the five persona sliders, the mood and the bond of that resident as the
 * sim says now, plus what they think of the topic. The same act lands differently: a tease is
 * banter to a blunt, easygoing, quirky one and an insult to a polite, serious one; an apology
 * matters more to the polite one, a hug to the expressive one; a sad one wants comfort, not a
 * joke; a friend's jab is banter and a rival's is not. Romance acts: +4 when the sim and the gate
 * would say yes (talk_dm_consent), else -3. The pet, a slot that is not in the room, a resident
 * who left and a NULL state give 0. Reads, changes nothing. talk_dm_say() applies the persona's
 * share of it to the bond and the mood of whoever the act was for. */
int  talk_dm_expect(const TalkState *s, int slot, int act, int tone, uint16_t topic);

/* One quick reply: an act the drama manager understands, said in a sentence. `text` is a static
 * string; "%t" in it stands for `topic` (the house puts the concept's name in). `to` is the
 * participant slot the reply is for, -1 for whoever the player is talking to now (a reply that takes
 * a side in a row names the one it takes the side of). */
#define TALK_QUICK_MAX 4
typedef struct TalkSugg {
    uint8_t     act;                /* TA_*                                    */
    uint8_t     tone;               /* TONE_*                                  */
    uint16_t    topic;              /* the topic it stays on, TC_NONE          */
    int8_t      to;                 /* slot, -1 as the player is addressing    */
    const char *text;
} TalkSugg;
/* The line the replies answer: the newest line of the current exchange that was said to the player
 * (or to the room), else the newest line; NULL while nothing has been said. *slot (may be NULL) is
 * who said it. */
const TalkLine *talk_quick_thread(const TalkState *s, int *slot);
/* Up to TALK_QUICK_MAX replies for what was just said, for the resident in slot `slot` (the one the
 * player is talking to), into out[]; returns how many (0 when the talk is over). `topic` is the topic
 * the player has in play, `fresh` a different one worth steering to (either may be TC_NONE). The
 * first reply follows the thread (it answers the question, thanks for the kind word, ...), the second
 * is the one that suits this resident (a joke for the easygoing, sympathy for a sad one, a thank-you for
 * the polite), the others change something. After a sincere line, or about a grief topic, only comfort
 * and kindness; in a room with a child never a snark, a dismissal or a tease; nothing romantic ever;
 * no act twice. The same salt gives the same replies (a new salt varies them); the texts are static. */
int  talk_quick_replies(const TalkState *s, int slot, uint16_t topic, uint16_t fresh, uint32_t salt,
                        TalkSugg *out);

/* ---- the runner's eyes on SCENE_TALK (talk_scene.c; only where DOOMO_TALK_DEBUG is defined: the native
 * host and the unit tests, never a console) -------------------------------------------------------
 * talk_scene_debug fills `o` and returns 1 while a talk is running (the dialogue box can be the top scene), else 0. A row is
 * described by its kind ("quick" "cat" "act" "topic" "type" "to" "invite" "bye" "pick" "notopic" "more"),
 * an argument (the quick reply number, the category, the act, 1 for a person in the invite list), a value
 * (the act of a quick reply, the topic or the resident id of a pick) and the text the list shows. */
typedef struct TalkSceneDebug {
    int active, busy, sel, nrows, nparts, to, to_id, tension;
    int gate;                           /* TG_* of the player's last line                */
    int reply_act, nlines;              /* the act of the last resident line, and how many lines the answer had */
    int care, reply_sincere;            /* TDF_CARE is open (K-11); the last resident line is a sincere-bank line */
    const char *page;                   /* "main" "acts" "topic" "invite" "more" */
    int ids[TALK_PARTS_MAX];
    char you[TALK_SHOWN_MAX];
} TalkSceneDebug;
int  talk_scene_debug(TalkSceneDebug *o);
int  talk_scene_row_debug(int i, const char **kind, int *arg, int *v, char *text);
int  talk_scene_act_cat(int act);

/* ---- the house hook (T8; weak in core/src/weak_talk.c) ---------------------
 * Start Talk with resident `who` in the room: 1 started, 0 not available
 * (no PLATF_TALK, or not built: the default). The house reaches Talk only
 * through this and the feature test, so --gc-sections drops it all on the
 * GBA. */
int  home_talk_enter(int who);
/* DC10B: A on a housemate while the platform still stages the bank (TALK_DB_LOADING): shows "Talk is warming
 * up..." (once) and returns 1; 0 when Talk is ready, absent or failed (the caller's own fallback runs). */
int  home_talk_note(void);
/* DC10B: 1 while the Talk screen is open (also while the dialogue box or the text entry is on top of it). The
 * Dreamcast draws the plain text of such a frame with its HD glyphs; the layout (6 px advance) is the same. */
int  talk_text_hd(void);
/* TD2: 1 = the Talk screen runs the generator one step a tick (the Dreamcast: a tick under 33.3 ms), 0 (the default) = all
 * at once. Not on the GBA (the function does not exist there). Talk's own lines are identical either way. */
void talk_scene_slice(int on);

/* Others to bring into the next talk (up to TALK_PARTS_MAX - 1; consumed by it): talk_scene.c, DOOMO_HAS_TALK. */
void talk_scene_party(const int *ids, int n);


/* ---- TQ-QUESTS: residents raise requests (core/src/talk_quest.c; docs/TALK.md 6q; owner decision 2026-11-15) ----
 * Now and then (never in two conversations in a row, at most one a town day) a resident raises a request while you
 * talk: from the sim's problems queue (PROB_HUNGRY, PROB_WANTS_ITEM, PROB_BORED, PROB_WANTS_FRIEND, PROB_LONELY,
 * PROB_FIGHT), a small favour (bring a snack, say hi to someone, keep them company) or a Talk-born one (cheer
 * someone up, keep a secret). You answer with the multilevel reply: accept, decline, haggle (what is in it for me)
 * or ask more. An accepted request is one row of the quest log (talk_quest_log_*, read by story_questlog.c) and is
 * remembered in the talk memory (a few bits per resident: no byte more in the save chunk). It ends in a thank-you
 * (reward, bond, a headline) when handed in, or a "never mind" after TQ_DAYS days.
 * Kid rules (tables in talk_quest.c, exhaustive tests): in a room with a minor, or from a minor, only a snack, a toy,
 * a message, company or cheering up someone of the same stage; nothing for adults only (secrets, fights, adult items,
 * places); never a romance or date request in any room (there is no such kind). Grief, the care window and a sad
 * or hostile exchange never raise one. */
enum { TQK_NONE = 0, TQK_FETCH, TQK_TOY, TQK_MESSAGE, TQK_COMPANY, TQK_CHEER, TQK_PEACE, TQK_SECRET, TQK_COUNT };
enum { TQS_NONE = 0, TQS_ACTIVE, TQS_READY, TQS_SETTLED };            /* settled with days 0: ran out unanswered */
enum { TQP_NONE = 0, TQP_ASK, TQP_ACCEPT, TQP_DECLINE, TQP_HAGGLE_YES, TQP_HAGGLE_NO, TQP_MORE, TQP_DONE, TQP_LATE,
       TQP_EXPIRED, TQP_COUNT };
enum { TQR_NONE = 0, TQR_ACCEPT, TQR_DECLINE, TQR_HAGGLE, TQR_MORE, TQR_COUNT };   /* what the player's act means to an open request */
enum { TQSRC_PROBLEM = 0, TQSRC_FAVOUR, TQSRC_TALK };
#define TQ_DAYS        7           /* town days an accepted request waits (3 bits)                       */
#define TQ_COOLDOWN    3           /* days before the same resident asks again after a hand-in or a no   */
#define TALK_QUEST_OPEN_MAX 4      /* accepted requests at once                                           */
#define TQ_ST_SLOT     0x03u       /* TalkState.q_st: the giver's slot                                    */
#define TQ_ST_HAGGLED  0x04u       /* the player has haggled once                                         */
#define TQ_ST_WON      0x08u       /* ... and the giver gave in (the reward is half again)                */
#define TQ_ST_MORE_SH  4           /* bits 4-5: how many times the player asked for more                  */
#define TQ_ST_CLOSED   0x40u       /* accepted or declined: nothing is open on the table                  */
#define TQ_ST_ASKED    0x80u       /* the last line on the table was the request (or its answer to a question): the quick replies are its four */
typedef struct TalkQuest {
    uint8_t  kind, src;            /* TQK_*, TQSRC_*                                                      */
    uint8_t  item, reward;         /* the item wanted (0 none); H$ on a hand-in                          */
    int16_t  target;               /* the resident it is about (-1 none)                                  */
} TalkQuest;
/* The pure rules (tables; no state). minor_giver / minor_room: the giver is a minor / a minor is in the room or is the
 * player; minor_target: the target is a minor (-1 when the kind has no target). */
int  talk_quest_kind_ok(int kind, int minor_giver, int minor_room, int minor_target);
int  talk_quest_item_ok(int item, int minor_giver, int minor_room);   /* a snack or a toy a minor may be asked for; no adult item */
int  talk_quest_reply_class(int act);                                  /* TA_* -> TQR_* (yes / no / what is in it for me / tell me more) */
int  talk_quest_reward(int kind, int bonus);                           /* H$ on a hand-in */
/* What a request of `kind` from `giver` is about (its target and item), the same every time for the same town: 1 when
 * the kind can be raised by the giver now in this room (ids[0..n) are the residents present; a target is never one of them). */
int  talk_quest_derive(int giver, int kind, const int *room, int n, int minor_room, TalkQuest *out);
/* The request `giver` would raise now (a problem, a favour or a Talk-born one), chosen with `salt`; 1 and *out filled. */
int  talk_quest_pick(int giver, const int *room, int n, int minor_room, uint32_t salt, TalkQuest *out);
/* haggle: 1 if the giver gives in (traits and the bond decide; salt only breaks a tie). */
int  talk_quest_haggle(int giver, uint32_t salt);
/* Pacing (not saved: after a load the first conversation never raises). pace_ok: at least one conversation since the last
 * raise, none yet today, fewer than TALK_QUEST_OPEN_MAX open. note_raise when a request is said; note_end when a conversation
 * ends (talk_outcome_end does it); reset with a new town. */
int  talk_quest_pace_ok(void);
int  talk_quest_raise_chance(void);                     /* percent of the calm answers that raise one: 14 (100 when armed by a test) */
void talk_quest_debug_arm(int kind);                    /* only where DOOMO_TALK_DEBUG is defined (the native host): the next calm answer raises one (of `kind`, 0 any) */
void talk_quest_note_raise(void);
void talk_quest_note_end(void);
void talk_quest_reset(void);
/* The table. state(giver): 1 when giver holds a request (kind, TQS_*, days, bonus). add: accept (checks the rules and
 * TALK_QUEST_OPEN_MAX); ready: a hand-in would work now; complete: hand it in (item taken, problem relieved, H$ paid)
 * and return the reward, -1 when it cannot be; decline / lapse_ack settle with the cooldown. */
int  talk_quest_state(int giver, int *kind, int *state, int *days, int *bonus);
int  talk_quest_open_count(void);
int  talk_quest_add(int giver, int kind, int bonus);
int  talk_quest_ready(int giver);
int  talk_quest_complete(int giver);
void talk_quest_decline(int giver);
void talk_quest_lapse_ack(int giver);
/* A conversation is over: open requests whose condition was met in it become ready (a message told, someone cheered up,
 * time spent, a fight made up, a secret kept a day). `accepted_now`: givers accepted in this very conversation are skipped. */
void talk_quest_end(const TalkSummary *sum);
/* The quest log (story_questlog.c, Side tab): rows 0..count-1, open ones first. goal: "Bring Nana a cupcake." */
#define TALK_QUEST_LOG_MAX 6
typedef struct TalkQuestRow { int16_t giver; uint8_t kind, state, days, reward; char goal[48]; } TalkQuestRow;
int  talk_quest_log_count(void);
int  talk_quest_log_row(int i, TalkQuestRow *out);
const char *talk_quest_kind_name(int kind);
/* The generator's line for a phase (talk_gen.c): the guards qphase= and qkind=; {target} and {item} fill from `target` and `item`. */
int  talk_gen_quest(TalkConv *c, int who, int phase, int kind, int target, int item, TalkLine *out, TalkTrace *tr);
/* The four quick replies while a request is on the table (accept, decline, haggle, ask more): count, into out[]. */
int  talk_quest_quick(const TalkState *s, TalkSugg *out);

#endif
