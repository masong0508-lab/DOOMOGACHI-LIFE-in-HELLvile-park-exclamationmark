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

#endif
