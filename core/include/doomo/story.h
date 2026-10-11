/* story.h - Story Mode (owner: story).
 *
 * The full arc (all playable in this build):
 * Chapter 1 "Welcome to HaHaland": extremely cutesy open-world life in
 *   HaHaland and on the Hillvile (good) side of Hellvile Park. Meet the
 *   citizens, hatch your Doomogachi, little fetch quests, all pastel. The
 *   Hellvile (bad) side is walled off; its citizens are only rumours.
 * Chapter 2 "Something in the Vapor": Cloud Nine Vapor opens by the wall.
 *   People get weird. First exorcism (the possessor is a named Hellvile
 *   citizen). Rivet, a Hellvile runaway, teaches you to weld. Hell Breach
 *   1: the Vape Cellar (first-person).
 * Chapter 3 "Hell Invades": the gate under Hellvile erupts out of a giant
 *   vape cloud; demons of hell invade wherever you are. The cute town turns
 *   into Doom. Weld cute or brutal weapons, clear the infestation, climb
 *   Cloud Nine Spire, defeat Lord Vapula.
 * Chapter 4 "Hellvile Park" (the ending - why the park is in the title):
 *   with Vapula gone you cross into Hellvile itself and learn most of its
 *   500 bad citizens were Vapula's prisoners too. You redeem, befriend and
 *   exorcise your way to peace; the wall between Hillvile and Hellvile
 *   comes down and every citizen of both sides finally comingles in
 *   Hellvile Park for one giant festival. Credits = a parade of all 1000
 *   citizens through the united park. Free play then unlocks the united
 *   park (cross-border friendships, romances, families).
 *
 * A small bytecode-free script engine: each scene step is a C function or
 * a table row; flags live in doomo_town.story.flags.
 */
#ifndef DOOMO_STORY_H
#define DOOMO_STORY_H

#include "types.h"

int  story_flag(int f);
void story_set_flag(int f, int on);
void story_new_game(void);           /* after the player made their citizen */
/* Called by the overworld when the player presses A on an NPC / object or
 * steps on a CELL_TRIGGER cell with `tag`. */
void story_interact_npc(int resident_id);
void story_trigger(int tag);
const char *story_objective(void);   /* current quest text for the HUD */

#endif
