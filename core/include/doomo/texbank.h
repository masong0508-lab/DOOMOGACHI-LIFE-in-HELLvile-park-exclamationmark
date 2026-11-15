/* texbank.h - the per-level texture bank (LV7a, plan 8.6 / 8.8; owner: levels / art).
 *
 * A level may carry up to 8 textures of its own on the ids 56..63 (DOOMO_TEXBANK_FIRST), named in
 * its map by `texbank N NAME`. This is the data format of the bank and its reader. The bytes
 * are made by tools/texbank.py from core/data/texbanks/NAME.bank and are "pack-ready": the PS1
 * and Dreamcast level packs (LV8, tools/packbreach.py) copy the blob as it is into BREACH.BIN;
 * the host and the Dreamcast may also link it as const data (core/gen/tb_NAME.c).
 *
 * Blob layout, little endian, all sizes fixed (format version 1):
 *      0   4  'L' 'V' 'T' 'B'
 *      4   1  version (1)
 *      5   1  count of slots, 1..8
 *      6   2  zero
 *      8   4  length of the whole blob in bytes
 *     12  16 * count   slot entries: name[12] (lowercase a-z 0-9 _, NUL padded, at least one
 *                      NUL), frames (1..4), speed (ticks per frame, 0 = 1), mode (LOOP or
 *                      PINGPONG), avg (palette index, the most frequent one in frame 0)
 *      ..  4096 * frames of every slot in slot order: 64 x 64 palette indices, row-major, the
 *                      same pixels as the shared textures (texture_pixels)
 * A bank costs 12 + 16 * count + 4096 * (sum of frames) bytes: 8 static textures are 32,908 B.
 *
 * RAM: the reader keeps no state of its own (texbank_open fills a caller's small view); only the
 * texture_pixels() hook below holds one pointer and is compiled on the host and the Dreamcast.
 * The GBA and the PS1 do not compile the hook (DOOMO_TEXBANK_ACTIVE 0): no resident byte there
 * until the level packs (LV8) take the bank in with their own reader.
 */
#ifndef DOOMO_TEXBANK_H
#define DOOMO_TEXBANK_H

#include "types.h"
#include "map.h"

#define DOOMO_TEXBANK_VERSION    1
#define DOOMO_TEXBANK_HDR        12
#define DOOMO_TEXBANK_ENT        16
#define DOOMO_TEXBANK_FRAMES_MAX 4      /* per texture */
#define DOOMO_TEXBANK_TOTAL_MAX  32     /* frames in one bank */

enum { TB_OK = 0, TB_ERR_NULL = -1, TB_ERR_SHORT = -2, TB_ERR_MAGIC = -3, TB_ERR_VERSION = -4,
       TB_ERR_COUNT = -5, TB_ERR_LENGTH = -6, TB_ERR_NAME = -7, TB_ERR_FRAMES = -8, TB_ERR_MODE = -9 };

#if defined(DOOMO_PLATFORM_PS1) || defined(DOOMO_PLATFORM_GBA)
#define DOOMO_TEXBANK_ACTIVE 0
#else
#define DOOMO_TEXBANK_ACTIVE 1
#endif

typedef struct DoomoTexBank {
    const uint8_t *ent;     /* the slot entries inside the blob */
    const uint8_t *pix;     /* the first pixel of the first frame */
    uint8_t count;
    uint8_t first[DOOMO_TEXBANK_MAX];   /* frame index (in the pixel run) of each slot */
} DoomoTexBank;

/* Check a blob (magic, version, count, length, names, frames, modes) and fill *b. Returns TB_OK
 * or a TB_ERR_*; *b is untouched on an error. Reads nothing past `len`. */
int  texbank_open(DoomoTexBank *b, const uint8_t *blob, uint32_t len);
/* slot of a name, -1 if none */
int  texbank_slot(const DoomoTexBank *b, const char *name);
const char *texbank_name(const DoomoTexBank *b, int slot);
int  texbank_frames(const DoomoTexBank *b, int slot);
uint8_t texbank_avg(const DoomoTexBank *b, int slot);
/* the 64 x 64 pixels of a slot at a time (ticks); NULL for a bad slot */
const uint8_t *texbank_pixels(const DoomoTexBank *b, int slot, uint16_t time);

#if DOOMO_TEXBANK_ACTIVE
/* the sample bank of the format test (core/data/texbanks/sample.bank, generated: core/gen/tb_sample.c) */
extern const uint8_t doomo_texbank_sample[];
extern const uint32_t doomo_texbank_sample_len;

/* Make a bank the level's: texture ids 56..63 then answer from it (texture_pixels, texture_image,
 * texture_frames, texture_is_animated, texture_avg, texture_name). NULL clears it. The bank must
 * outlive its use (the blob is not copied). */
void texbank_activate(const DoomoTexBank *b);
const DoomoTexBank *texbank_active(void);
/* the hook of textures.c: NULL / 0 when no bank is active or the id has no slot */
const uint8_t *texbank_id_pixels(int tex_id, uint16_t time);
const DoomoImage *texbank_id_image(int tex_id, uint16_t time);
int  texbank_id_slot(int tex_id);
#endif

#endif
