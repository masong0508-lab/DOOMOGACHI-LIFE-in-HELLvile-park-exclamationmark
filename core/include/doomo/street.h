/* street.h - Home Row, the street of 1000 houses (NEIGHBOURHOOD_ROADMAP M0).
 *
 * Pure addressing and length rules, no drawing and no saved state. Every resident slot has one
 * permanent lot: slot id -> district, chunk, lot, side of the boulevard. The street is built in
 * chunks of STREET_LOTS_PER_CHUNK lots and is only as long as it needs to be: the occupied lots
 * plus a few free ones (STREET_FREE_LOTS), never shorter than one chunk and never longer than
 * the district (500 lots). At the 1000-person limit no free lot is left and the street ends at
 * the last lot; deleting a person gives their lot (and the length) back. Nothing here is saved:
 * a load rebuilds the same street from the town table.
 *
 * The lot's block and room match the apartments view (fp_floor / fp_room): block = floor
 * (1..50), room 1..10, so one address means the same house everywhere.
 */
#ifndef DOOMO_STREET_H
#define DOOMO_STREET_H

#include "types.h"

#define STREET_DISTRICTS       2                       /* Hillvile, Hellvile             */
#define STREET_LOTS_PER_CHUNK  50                      /* 25 per side of the boulevard   */
#define STREET_CHUNKS_MAX      (DOOMO_DISTRICT_SIZE / STREET_LOTS_PER_CHUNK)   /* 10      */
#define STREET_FREE_LOTS       10                      /* empty lots shown past the last */
DOOMO_STATIC_ASSERT(DOOMO_DISTRICT_SIZE % STREET_LOTS_PER_CHUNK == 0, chunks_tile_the_district);
DOOMO_STATIC_ASSERT(DOOMO_MAX_RESIDENTS == STREET_DISTRICTS * DOOMO_DISTRICT_SIZE, two_districts);

enum { STREET_SIDE_LEFT, STREET_SIDE_RIGHT };

/* ---- the address of a slot (pure; -1 for an id that is not a resident slot) ---- */
int street_district(int slot);           /* 0 Hillvile, 1 Hellvile                          */
int street_index(int slot);              /* 0..499: the lot's place along its district      */
int street_chunk_of(int slot);           /* 0..9                                            */
int street_lot_of(int slot);             /* 0..49 inside its chunk                          */
int street_side(int slot);               /* STREET_SIDE_*: even lots left, odd lots right   */
int street_block(int slot);              /* 1..50, the apartments' floor                    */
int street_room(int slot);               /* 1..10, the apartments' room                     */
/* The inverse: the slot at (district, chunk, lot), or -1 when out of range. */
int street_slot(int district, int chunk, int lot);

/* ---- the length (the limit's truncation rule) -------------------------------------------
 * lots(extent) = min(500, extent + STREET_FREE_LOTS), where extent = the last occupied lot + 1
 * (0 for an empty district). chunks = max(1, ceil(lots / 50)). Pure form first (tests), then
 * the one that reads the live town table. */
int street_lots_for(int extent);
int street_chunks_for(int extent);
int street_extent(int district);         /* last occupied lot + 1 in the live town, 0 if none */
int street_lots(int district);           /* lots built: extent + free lots, capped            */
int street_chunks(int district);         /* chunks to stream, 1..10                           */
int street_free_lots(int district);      /* empty lots inside the built street                */
int street_is_full(int district);        /* 1 when all 500 lots are occupied (no free model)  */

#endif
