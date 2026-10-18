/* exorcism.h - Father Fuzz's exorcism ritual (owner: life).
 * A three-phase minigame scene (SCENE_EXORCISM, arg = resident id):
 *   1. Prayer rhythm - button prompts slide into a heart-shaped hit zone.
 *   2. Holy water   - aim and throw at the demon darting around the host.
 *   3. True name    - pick the demon's true name from clues.
 * Items (holy water, sage, plush cross, rosary, salt, hymn book) and the
 * host's sanity/corruption shift the odds. Failure increments the
 * resident's exorcism fail counter; at DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL
 * the resident becomes RES_SOUL_LOST and may be laid to rest. */
#ifndef DOOMO_EXORCISM_H
#define DOOMO_EXORCISM_H

#include "types.h"

/* Odds preview 0..100 shown before starting. */
int exorcism_odds(int resident_id);
/* Apply a finished ritual's result (used by the scene and by tests). */
void exorcism_apply(int resident_id, int success);

/* ======================================================================
 * life additions (owner: life; additive)
 * ==================================================================== */

/* SCENE_EXORCISM: arg = resident id. A POSSESSED resident gets the full
 * ritual; a SOUL_LOST one goes straight to the "soul is lost" moment and
 * the Lay to Rest offer; arg < 0 (or an empty slot) is Father Fuzz's
 * practice ritual on a plush dummy (no stakes - tutorials, scripts). B on
 * the result screen pops the scene. */

/* The prayer rhythm is played at this tempo (SONG_EXORCISM is written for
 * it; the scene also rings a soft bell on every beat). 30 Hz ticks. */
#define EXORCISM_BPM            120
#define EXORCISM_TICKS_PER_BEAT 15

/* Ritual tools (bit i of an item mask = doomo_exorcism_items[i]). */
enum { EXI_HOLY_WATER, EXI_SAGE, EXI_PLUSH_CROSS, EXI_ROSARY, EXI_SALT,
       EXI_HYMN_BOOK, EXI_COUNT };
extern const uint8_t doomo_exorcism_items[EXI_COUNT];      /* ITEM_*    */
/* What each tool does in the ritual, one short line ("+1 flask"). */
extern const char *const doomo_exorcism_item_help[EXI_COUNT];

/* Odds 0..100 with a set of tools (mask of EXI_* bits). exorcism_odds()
 * is the same with no tools. Host sanity and species holiness, failed
 * attempts, the possessor's character, difficulty and an angelic adult
 * Doomogachi all count. */
int exorcism_odds_with(int resident_id, unsigned item_mask);
/* Final success chance from the odds and the three phase results
 * (rhythm 0..100, holy water 0..100, true name guessed 0/1). */
int exorcism_chance(int odds, int rhythm, int water, int name_ok);

/* True-name puzzle: four candidate names (Hellvile citizen ids) in
 * out[4]; returns the index of the real possessor. Deterministic. */
int exorcism_name_choices(int resident_id, int out[4]);
/* Clue k (0..3) about the possessor: job, personality, favourite food,
 * bio. Returns buf. */
const char *exorcism_clue(int resident_id, int k, char *buf, int cap);

/* Exorcism counters of a resident, for UI: failures so far and how many
 * more failures until the soul is lost. */
int exorcism_fails(int resident_id);
int exorcism_tries_left(int resident_id);

/* ======================================================================
 * S3 additions (docs/design/story/SPEC.md S3 M1, story_final.md sec.1 and
 * sec.15.11; additive): AUTHORED POSSESSORS.
 *
 * Story possessions are not the sim's random pick: each one is a row of
 * the authored possessor table (host, true name, the names offered, clue
 * props, the near miss and the strike, a ritual skin, flags, the scripts
 * run after a save or a loss). The VM's POSSESS op arms a row on a host
 * (exorcism_possess); from then on sim_possessor_of() (through
 * story_possessor_override) and the ritual's clues and names come from
 * the row. The slots are not saved: the story re-arms them on every
 * resume (S2: story.c story_rearm_possessors, from the @armed windows of
 * the .dsc packs), and a possession without an armed slot is the sim's
 * own pick, also for a story host (sim_possession_cap from Ch2).
 *
 * The table is generated: core/tools/textpack.py (S2) compiles the
 * `@possessor` blocks of the .dsc packs into core/gen/possess_table.c,
 * which defines doomo_possess_table / doomo_possess_count strongly. Until
 * it does, exorcism.c's weak defaults hold the rows of story_final.md
 * sec.1 (read them through a volatile pointer, ARCHITECTURE.md "weak
 * defaults of registries"). Rows are append-only: the row index is the
 * true-name id the VM's POSSESS op carries.
 * ==================================================================== */
#define EXO_HOST_SCRIPT   0xFFFFu   /* host: whoever the script names (the Feast culprit) */
#define EXO_HOST_PRACTICE 0xFFFEu   /* host: Father Fuzz's practice plush (arg < 0)    */
#define EXO_NONE          0xFFFFu   /* name_res: the true name is not a citizen        */
#define EXO_NO_PICK       0xFF      /* near_miss / herring: none                       */

/* Ritual skins (the scene's phase variants; S3 M2+ draws them). */
enum { EXS_STANDARD,      /* prayer, holy water, true name                         */
       EXS_PRAYER_TIMING, /* Hattie / HAZE: the prayer phase is the timing game    */
       EXS_NAME_TOMATO,   /* Grampy / SOOTKIN: the name phase is the tomato memory  */
       EXS_TWO_FAITH,     /* Smog / FLUFFY: Fuzz forwards, Nox backwards           */
       EXS_REVERSE,       /* Sprinklepop: EXORCISM UNFAILED, the player types it   */
       EXS_PRACTICE,      /* Mr. Huggins the 2nd / 4th / 9th                       */
       EXS_WISP_PROMPT,   /* S211: a rite-less wisp, a single prompt               */
       EXS_COUNT };
/* Row flags. */
enum { EXF_SCRIPTED_FAIL = 1,   /* always lost (Wheezle): the true name is never offered */
       EXF_FIXED_ODDS = 2,      /* ignores OPT_DIFFICULTY and the tools (scripted rites) */
       EXF_NO_SOUL_LOSS = 4,    /* failures never lose the soul (practice, wisps)       */
       EXF_TYPE_NAME = 8,       /* the player types the name (EXS_REVERSE)              */
       EXF_GUEST_NAMES = 16,    /* names are drawn from the guests (the Feast culprit)  */
       EXF_UNFAIL = 32 };       /* success un-loses a lost soul (RES_SOUL_LOST -> ALIVE) */

typedef struct DoomoPossessor {
    const char *name;        /* the true name as shown ("TALLYWORN")                   */
    const char *offer[4];    /* the names on the board; NULL = drawn like the sim's    */
    const char *clue[4];     /* clue props ("a crooked crown"); NULL = the sim's clue   */
    uint16_t host;           /* resident id possessed, or EXO_HOST_*                   */
    uint16_t name_res;       /* the true name as a citizen (Tallyworn 712), EXO_NONE   */
    uint16_t saved, lost;    /* VM script ids run after a save / a loss (0 none)       */
    uint8_t  near_miss;      /* offer index that is a near miss (no strike), or EXO_NO_PICK */
    uint8_t  herring;        /* offer index that is a strike with a gut punch, or EXO_NO_PICK */
    uint8_t  skin;           /* EXS_*                                                  */
    uint8_t  flags;          /* EXF_*                                                  */
    uint16_t near_flag;      /* story flag set by the near miss (SF_SMOG_NEAR_MISS), 0 none */
    uint16_t lost_flag;      /* story flag set when the soul is lost (SF_HATTIE_LOST..), 0 none */
} DoomoPossessor;
extern const DoomoPossessor *const doomo_possess_table;
extern const uint16_t doomo_possess_count;

/* The row the host is possessed by (its armed slot; a citizen's only in
 * story mode), -1 = the sim's own pick. resident_id < 0 is the practice
 * plush (rows with EXO_HOST_PRACTICE). */
int  exorcism_authored_row(int resident_id);
/* Disarm every slot (S2: before the story re-arms them after a load). */
void exorcism_slots_clear(void);
const DoomoPossessor *exorcism_authored(int resident_id);   /* NULL = none */
/* VM POSSESS / UNPOSSESS: arm `row` on the host and possess it (status,
 * corruption); row < 0 possesses with the sim's pick. Unpossess clears
 * the slot and the possession without an exorcism (story cleanup). */
void exorcism_possess(int resident_id, int row);
void exorcism_unpossess(int resident_id);
/* The k-th name on the board (0..3) as text, authored or not; returns buf. */
const char *exorcism_name_text(int resident_id, int k, char *buf, int cap);
/* The board's answer index (0..3), -1 when the true name is never offered. */
int  exorcism_name_answer(int resident_id);
/* Judge a pick on the board: EXP_RIGHT, EXP_WRONG (a strike), EXP_NEAR_MISS
 * (no strike; sets the row's near_flag) or EXP_HERRING (a strike that hurts). */
enum { EXP_WRONG, EXP_RIGHT, EXP_NEAR_MISS, EXP_HERRING };
int  exorcism_name_pick(int resident_id, int k);
/* The result the ritual must have for a scripted row: 0 forced loss,
 * 1 forced save, -1 the dice decide. */
int  exorcism_forced_result(int resident_id);
/* --- end S3 --- */

#endif
