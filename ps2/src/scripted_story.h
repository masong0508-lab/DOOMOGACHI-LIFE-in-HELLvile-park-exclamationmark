/* PS2 CI-only navigation timeline. These are SYNTHETIC game-core button
 * masks, NOT evidence that Play! has delivered a physical DualShock input.
 * Run as a separate diagnostic ELF; the shipped game never includes it.
 */
#ifndef PS2_SCRIPTED_STORY_H
#define PS2_SCRIPTED_STORY_H
#include <stdint.h>
#include "doomo/input.h"

/* Exactly one 30Hz tick pressed, then releases until the next input.
 * With an empty save card: title -> Story Mode -> New Story.
 * Delay until boot/title effects have settled so screenshot #1
 * (3 seconds) captures the title, not the scripted game transition.
 */
static inline uint32_t ps2_scripted_story_buttons(uint32_t tick)
{
    if(tick==120u) return BTN_START;
    if(tick==150u || tick==180u) return BTN_A;
    return 0;
}
#endif
