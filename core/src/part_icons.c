/* part_icons.c - lookup and unpacking of the creator's prerendered part icons (CR7; owner:
 * creator). The icons themselves are generated (core/gen/part_icons_data.c raw,
 * core/gen/part_icons_rle.c packed) by core/tools/part_icons.c; see part_icons.h.
 * Pure, no state. */
#include "doomo/doomo.h"
#include "doomo/part_icons.h"

int part_icon_index_in(int row, int packed)
{
    const uint16_t *rows = packed ? doomo_part_icon_rle_rows : doomo_part_icon_rows;
    int lo = 0, hi = (int)(packed ? doomo_part_icon_rle_count : doomo_part_icon_count) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1, r = rows[mid];
        if (r == row) return mid;
        if (r < row) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

int part_icon_index(int row)
{
#if defined(DOOMO_ICONS_RLE)
    return part_icon_index_in(row, 1);
#else
    return part_icon_index_in(row, 0);
#endif
}

/* Packed form (core/tools/part_icons.c): [k colours, 1..15][k palette indices][runs over the 200
 * bytes of 4-bit pixels, high nibble first]. A run byte n < 0x80 repeats the byte after it n + 1
 * times; n >= 0x80 is n - 0x7F literal bytes. Nibble 0 is transparent, j is palette[j - 1]. The
 * 4-bit bytes are decoded into the back half of dst, then widened front to back in place. */
int part_icon_unpack(int i, uint8_t *dst)
{
    const uint8_t *p, *e, *pal;
    uint8_t *nib;
    int o = 0, k, n;
    if (i < 0 || i >= (int)doomo_part_icon_rle_count || !dst) return 0;
    p = doomo_part_icon_rle + doomo_part_icon_rle_ofs[i];
    e = doomo_part_icon_rle + doomo_part_icon_rle_ofs[i + 1];
    if (p >= e || (k = *p++) > 15 || p + k > e) return 0;
    pal = p;
    p += k;
    nib = dst + PART_ICON_PIX / 2;
    while (p < e && o < PART_ICON_PIX / 2) {
        n = *p++;
        if (n < 0x80) {
            n += 1;
            if (o + n > PART_ICON_PIX / 2 || p >= e) return 0;
            memset(nib + o, *p++, (size_t)n);
        } else {
            n -= 0x7F;
            if (o + n > PART_ICON_PIX / 2 || p + n > e) return 0;
            memcpy(nib + o, p, (size_t)n);
            p += n;
        }
        o += n;
    }
    if (o != PART_ICON_PIX / 2 || p != e) return 0;
    for (o = 0; o < PART_ICON_PIX / 2; o++) {
        int b = nib[o], hi = b >> 4, lo = b & 15;
        if (hi > k || lo > k) return 0;
        dst[2 * o] = hi ? pal[hi - 1] : 0;
        dst[2 * o + 1] = lo ? pal[lo - 1] : 0;
    }
    return 1;
}

int part_icon_image(int row, DoomoImage *out, uint8_t *buf)
{
    int i = part_icon_index(row);
    if (i < 0 || !out) return 0;
    out->w = PART_ICON_W;
    out->h = PART_ICON_W;
    out->uid = 0x1C000000u | (uint32_t)i;
#if defined(DOOMO_ICONS_RLE)
    if (!part_icon_unpack(i, buf)) return 0;
    out->pix = buf;
#else
    (void)buf;
    out->pix = doomo_part_icon_pix[i];
#endif
    return 1;
}
