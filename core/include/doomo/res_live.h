/* res_live.h - the 500 residents of the GBA (GBA-500, owner 2026-11-15; docs/GBA.md
 * "500 residents").
 *
 * Resident ids are 0..999 on every platform. A build with DOOMO_LIVE_RESIDENTS < 1000
 * (the GBA: 500) keeps that many records, 250 per district, and finds a resident's
 * record through an id -> slot table (town.h: TOWN_RES, town_slot). Which 500 a new
 * Hellvile Park Classic island moves in is fixed by tools/gba_live.py
 * (core/gen/live_set.c):
 *   MUST      the story cast, everyone a story script, a side mission, an exorcism,
 *             an ending, the credits, a shop or a Talk line names, and the kid cast
 *             (KID / TEEN traits, core/data/story/kid_cast.txt); never left out;
 *   DEFAULT   MUST + their households (FAMILY / MARRIED / DATING roster edges) + the
 *             lowest ids, until each district has 250.
 * A Random Island takes the same 500 ids. An old save with more residents than the
 * slots keeps the 500 the player is closest to (save.c, sv_select: the must set, the
 * players' and the must citizens' households, a child's family as one); the others
 * move away (a news item).
 */
#ifndef DOOMO_RES_LIVE_H
#define DOOMO_RES_LIVE_H

#include "types.h"

/* core/gen/live_set.c (tools/gba_live.py): bit id = [id >> 3] >> (id & 7) */
extern const uint8_t  doomo_live_must[DOOMO_MAX_RESIDENTS / 8];
extern const uint8_t  doomo_live_default[DOOMO_MAX_RESIDENTS / 8];
extern const uint16_t doomo_live_must_count;

/* 1 if premade citizen `id` is in the set (0 for a bad id). */
int res_live_must(int id);
int res_live_default(int id);

#endif
