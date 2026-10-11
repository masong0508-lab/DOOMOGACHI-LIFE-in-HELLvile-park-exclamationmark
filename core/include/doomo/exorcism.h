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

#endif
