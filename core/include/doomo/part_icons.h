/* part_icons.h - the creator's prerendered part icons (CR7; owner: creator).
 *
 * Every live part of a slot the renderer draws from the look (and every species) has a 20x20
 * 8-bit icon: the part alone on a neutral grey mannequin (slots whose first choice is "None"),
 * or the part in colour where every choice is a shape (eyes, head shapes, species). They are made
 * at build time from the real renderer by core/tools/part_icons.c (`make -C core gen`, checked by
 * `make check-gen`) into core/gen/part_icons_data.c (raw pixels: GBA and native read them straight
 * from ROM, so browsing the creator's grid renders no portrait) and core/gen/part_icons_rle.c (the
 * same icons run-length packed: the PS1 links only the tables of these, in the creator overlay group,
 * keeps the packed bytes in SPU RAM (CR10: PARTS.BIN) and unpacks one into the caller's buffer before
 * plat_image uploads it to the VRAM image cache).
 *
 * Palette indices are the game's master palette; 0 is transparent. docs/CREATOR.md "Part icons". */
#ifndef DOOMO_PART_ICONS_H
#define DOOMO_PART_ICONS_H

#include "types.h"

#define PART_ICON_W   20
#define PART_ICON_PIX (PART_ICON_W * PART_ICON_W)

/* generated (core/gen/part_icons_*.c). CR10: the packed set holds the same icons as the raw one (the PS1 draws
 * scripted parts now); each file keeps its own rows and indices. */
extern const uint16_t doomo_part_icon_count;                      /* part_icons_data.c (raw)         */
extern const uint16_t doomo_part_icon_rows[];                     /* doomo_part_meta rows, ascending */
extern const uint8_t  doomo_part_icon_pix[][PART_ICON_PIX];
extern const uint16_t doomo_part_icon_rle_count;                  /* part_icons_rle.c (packed)       */
extern const uint16_t doomo_part_icon_rle_rows[];                 /* doomo_part_meta rows, ascending */
extern const uint32_t doomo_part_icon_rle_ofs[];                  /* count + 1                       */
extern const uint8_t  doomo_part_icon_rle[];                      /* not linked on the PS1 (DOOMO_PARTS_SPU) */

/* The longest packed icon (the tool refuses a longer one): part_icon_unpack reads one into the stack. */
#define PART_ICON_PACK_MAX 256
#if defined(DOOMO_PARTS_SPU) || defined(DOOMO_PARTS_SPU_TEST)
/* The platform: copy `n` (at most PART_ICON_PACK_MAX) bytes of the packed icon bytes, from byte offset `ofs`
 * (doomo_part_icon_rle_ofs), to dst. 0 when they are not there (PARTS.BIN missing). */
int plat_part_icon_read(uint32_t ofs, uint8_t *dst, unsigned n);
#endif

/* The icon index of doomo_part_meta row `row` in this build's set (the packed one on DOOMO_ICONS_RLE
 * builds), or -1 when it has none (not live, a slot the icon tool does not draw). */
int part_icon_index(int row);
/* The same in a given set (tests): packed != 0 the packed rows, else the raw rows. */
int part_icon_index_in(int row, int packed);
/* The icon of row `row` as an image: 1 and *out filled, 0 when there is none. `buf` must hold
 * PART_ICON_PIX bytes: builds that link the packed icons (DOOMO_ICONS_RLE, the PS1) unpack into
 * it; the others point *out at ROM and leave it untouched. out->uid is stable per icon, so a
 * platform image cache keeps an icon across frames. */
int part_icon_image(int row, DoomoImage *out, uint8_t *buf);
/* Unpack packed icon `i` (an index into the packed rows) into dst (PART_ICON_PIX bytes); returns 1
 * on success. Every build has it (the host tests check it against the raw icons). */
int part_icon_unpack(int i, uint8_t *dst);
/* The same image with the unpacked pixels kept in `buf` between calls: `*tag` (0 at first) says which row
 * `buf` holds (row + 1), and a call with the same row unpacks nothing. 0 when the row has no icon. */
int part_icon_image_kept(int row, DoomoImage *out, uint8_t *buf, uint16_t *tag);

#endif
