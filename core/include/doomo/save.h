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

/* Serialise doomo_town into buf. Returns bytes written or 0 if cap too small.
 * (The codec's work area is the tail of plat_save_scratch(), so buf must
 * not overlap it.) */
uint32_t save_serialize(uint8_t *buf, uint32_t cap);
/* Load into doomo_town. Returns 1 on success, 0 on bad magic/version/CRC
 * (doomo_town untouched on failure). Sanitises every resident. */
int      save_deserialize(const uint8_t *buf, uint32_t size);
uint32_t save_crc32(const uint8_t *buf, uint32_t size);

/* High level: serialise + plat_save_write / plat_save_read + deserialise,
 * in plat_save_scratch() - or streamed through the platform in chunks
 * when that buffer is smaller than the slot (platform.h). */
int save_game(int slot);
int load_game(int slot);
int save_exists(int slot);

/* ---- foundation additions ------------------------------------------------ */
/* The codec keeps NO static buffers: everything it needs lives in
 * plat_save_scratch() (platform.h) for the duration of one call:
 *
 *   [ save image or stream window ...... | ext staging | LZ codec ]
 *                                         \____ DOOMO_SAVE_WORK_SIZE ___/
 *
 * The last DOOMO_SAVE_WORK_SIZE bytes hold the LZ window, its match heads
 * and literal run (DOOMO_SAVE_CODEC_SIZE) and the staging area for one
 * extension chunk (save_ext.h: write() / read() see at most
 * DOOMO_SAVE_EXT_MAX contiguous bytes; a bigger chunk fails the save). */
#define DOOMO_SAVE_CODEC_SIZE  6400u
#define DOOMO_SAVE_EXT_MAX     8192u
#define DOOMO_SAVE_WORK_SIZE   (DOOMO_SAVE_CODEC_SIZE + DOOMO_SAVE_EXT_MAX)
/* plat_save_scratch() with at least this much holds a whole save image
 * in front of the work area: a slot plus the work area is all save/load
 * ever need. */
#define DOOMO_SAVE_SCRATCH_MIN (DOOMO_SAVE_SLOT1_MAX + DOOMO_SAVE_WORK_SIZE)
/* With less, the platform streams saves (plat_save_stream_*,
 * plat_save_read_at) and the front part is only the chunk / read window;
 * the scratch must hold at least this much (bigger windows = fewer
 * calls). */
#define DOOMO_SAVE_STREAM_MIN  (DOOMO_SAVE_WORK_SIZE + 64u)
#define DOOMO_SAVE_HEADER_SIZE 48
uint32_t save_slot_max(int slot);       /* DOOMO_SAVE_SLOT0_MAX / SLOT1_MAX */

/* Summary kept in the uncompressed header, for the save/load screen. */
typedef struct DoomoSaveInfo {
    uint8_t  mode;          /* TOWN_FREEPLAY / TOWN_STORY               */
    uint8_t  chapter;       /* story chapter                            */
    uint8_t  population;    /* TOWN_POP_*                               */
    uint8_t  weekday;
    uint16_t residents;     /* non-empty resident slots                 */
    uint16_t minute;
    uint32_t day;
    uint32_t money;
    uint32_t size;          /* bytes on the medium                      */
    char     player[DOOMO_NAME_LEN];
} DoomoSaveInfo;
/* Read just the 48-byte header of a slot (cheap enough for menus: no
 * scratch buffer, no CRC). 1 if a save with a valid header exists. */
int  save_peek(int slot, DoomoSaveInfo *out);
/* Full integrity check of a slot (reads it all, verifies the CRC). */
int  save_verify(int slot);
/* Parse the header of an in-memory save (no CRC check of the body). */
int  save_info_from(const uint8_t *buf, uint32_t size, DoomoSaveInfo *out);
/* Bytes produced by the last save_serialize()/save_game(), and a short
 * human-readable reason for the last failure ("" if none), e.g.
 * "Island too big: 93 KB > 88 KB". */
uint32_t    save_last_size(void);
const char *save_last_error(void);

#endif
