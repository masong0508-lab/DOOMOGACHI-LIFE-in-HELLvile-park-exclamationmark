/* dmv.h - the boot FMV's file format and its decoder (DC9C; owner: dreamcast).
 *
 * INTRO.DMV is the Dreamcast's boot movie (docs/DREAMCAST.md, "Boot FMV"). The
 * Dreamcast has no hardware video decoder, so it is a small intra-only DCT
 * codec of our own (dc/tools/mkfmv.py writes it, dmv.c reads it, fmv.c plays
 * it). PORTABLE C99, no KallistiOS: dc/tests/fmv builds it on the host too.
 *
 * File (little endian):
 *   header, DMV_HDR_SIZE bytes
 *     0   "DMV1"
 *     4   u16 width, u16 height   (multiples of 16)
 *     8   u8 fps, u8 0, u16 frames
 *     12  u32 audio rate (Hz, mono 16-bit), u16 audio samples per record, u16 0
 *     20  u16 dequant[2][64]      luma, then chroma, in zigzag order
 *   then `frames` records: u32 vbytes (a multiple of 4), the frame's
 *   bitstream, then the record's audio (s16 mono).
 *
 * A frame is YCbCr 4:2:0 (JPEG's full-range BT.601), 8x8 blocks, the planes
 * one after the other (Y, Cb, Cr), blocks in raster order. A block:
 *   DC    the difference to the previous block of the plane, zigzag-folded
 *         (n >> 1 ^ -(n & 1)) and written as a 7-bit varint
 *   AC    tokens in zigzag order until the end of the block:
 *           0x00          end of block (the rest is zero)
 *           0x10*r        r (1..7): skip 8*r zero coefficients
 *           0x10*r + c    c 1..14: skip r zeros, then the level
 *                         (c odd: +(c+1)/2, even: -c/2), r 0..7
 *           0x10*r + 15   skip r zeros, then the level in the next byte (s8)
 *   the level is multiplied by dequant[zigzag position].
 * The decoder never reads past the bytes it is given and never writes
 * outside the planes: a corrupt frame returns 0.
 */
#ifndef DC_DMV_H
#define DC_DMV_H

#include <stdint.h>

#define DMV_HDR_SIZE   276
#define DMV_MAX_W      320
#define DMV_MAX_H      240
#define DMV_PLANES_MAX (DMV_MAX_W * DMV_MAX_H * 3 / 2)

typedef struct DmvInfo {
    uint16_t w, h;
    uint8_t  fps;
    uint16_t frames;
    uint32_t arate;
    uint16_t apr;                       /* audio samples per record */
    uint16_t dq[2][64];
} DmvInfo;

/* the header: 1 when it is a DMV1 file this decoder can play */
int dmv_parse_header(const uint8_t *buf, uint32_t len, DmvInfo *out);
/* one frame's bitstream into planes (Y w*h, Cb and Cr w/2*h/2 each, packed);
 * 1 when the whole frame was read, 0 when it is corrupt or short */
int dmv_decode(const DmvInfo *in, const uint8_t *vid, uint32_t len, uint8_t *planes);
/* the record that starts at file offset `off` (the first one at DMV_HDR_SIZE):
 * its bitstream, its audio (apr s16 samples, unaligned) and the next offset.
 * 1 when the whole record is inside the first `have` bytes of the file */
int dmv_record(const DmvInfo *in, const uint8_t *file, uint32_t have, uint32_t off,
               const uint8_t **vid, uint32_t *vlen, const uint8_t **aud, uint32_t *next);
/* planes into RGB565, `stride` pixels per row of dst */
void dmv_to_rgb565(const DmvInfo *in, const uint8_t *planes, uint16_t *dst, int stride);

#endif
