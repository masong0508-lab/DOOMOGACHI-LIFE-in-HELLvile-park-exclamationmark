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

/* ---- the house hook (T8; weak in core/src/weak_talk.c) ---------------------
 * Start Talk with resident `who` in the room: 1 started, 0 not available
 * (no PLATF_TALK, or not built: the default). The house reaches Talk only
 * through this and the feature test, so --gc-sections drops it all on the
 * GBA. */
int  home_talk_enter(int who);

#endif
