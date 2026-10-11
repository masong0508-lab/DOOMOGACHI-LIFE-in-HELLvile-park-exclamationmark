/* save.h - versioned, checksummed serialisation of doomo_town.
 * Explicit little-endian field-by-field encoding (never raw struct dumps
 * of multi-byte fields). Residents are stored as deltas against their
 * premade roster record (roster.h) - a premade citizen whose look is
 * untouched costs only its changed stats - and fully for custom ones;
 * the stream is then compressed. Capacities per slot:
 *   slot 0 story     <= DOOMO_SAVE_SLOT0_MAX
 *   slot 1 free play <= DOOMO_SAVE_SLOT1_MAX
 * If an island is too custom to fit, save_game() fails cleanly and the UI
 * says so (never corrupts the old save). */
#define DOOMO_SAVE_SLOT0_MAX  (40u * 1024u)
#define DOOMO_SAVE_SLOT1_MAX  (88u * 1024u)
#ifndef DOOMO_SAVE_H
#define DOOMO_SAVE_H

#include "types.h"

#define DOOMO_SAVE_MAGIC   0x4F4F4D44u  /* "DMOO" */
#define DOOMO_SAVE_VERSION 1

/* Serialise doomo_town into buf. Returns bytes written or 0 if cap too small. */
uint32_t save_serialize(uint8_t *buf, uint32_t cap);
/* Load into doomo_town. Returns 1 on success, 0 on bad magic/version/CRC
 * (doomo_town untouched on failure). Sanitises every resident. */
int      save_deserialize(const uint8_t *buf, uint32_t size);
uint32_t save_crc32(const uint8_t *buf, uint32_t size);

/* High level: serialise + plat_save_write / plat_save_read + deserialise.
 * Uses a static scratch buffer. */
int save_game(int slot);
int load_game(int slot);
int save_exists(int slot);

#endif
