/* hcmd.h - harness command ids and hooks beyond gba_plat.h's (owner: H2,
 * SPEC R20/C2). gba_plat.h's GBA_HC_* enum stays frozen until the GBA
 * memory pass merges (SPEC G0), so the story/house runner commands
 * continue its numbering here; gba/harness/harness.c mirrors the values.
 */
#ifndef GBA_HCMD_H
#define GBA_HCMD_H

#include <stdint.h>

enum {
    GBA_HC_SEED = 9,          /* arg seed: town seed, doomo_rng, plat_entropy pin  */
    GBA_HC_FLAG = 10,         /* arg flag id -> story_flag(id)                     */
    GBA_HC_TEXT_ARM = 11,     /* name = needle: watch plat_text for it              */
    GBA_HC_TEXT_HIT = 12,     /* -> 1 if the needle was drawn since the arm         */
    GBA_HC_STORY_FP = 13,     /* -> story_fingerprint()                            */
    GBA_HC_ROOM_PARITY = 14,  /* arg view | path: -> FNV of the parity frame (room.c) */
    GBA_HC_ROOM_TEST = 15     /* arg ROOM_TEST_*: the house test layout (room.c)   */
};

/* "seed N": plat_entropy() returns the pinned value while gba_entropy_pinned
 * (like the native runner's fixed entropy), so the next town_new() and every
 * later entropy read are reproducible. */
extern uint8_t  gba_entropy_pinned;
extern uint32_t gba_entropy_pin;

/* "expect-text": plat_text / plat_text_outline call gba_text_probe() while
 * gba_text_armed (one byte test per call otherwise). */
extern uint8_t gba_text_armed;
void gba_text_probe(const char *s);

#endif
