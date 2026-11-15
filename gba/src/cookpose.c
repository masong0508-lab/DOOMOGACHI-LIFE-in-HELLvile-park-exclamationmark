/* cookpose.c - Granny's baked poses on the GBA (S2-4c; core/src/fishcook_int.h plat_cook_pose;
 * owner: gba; docs/GBA.md "Speed wave 2: S2-4c").
 *
 * The build bakes every pose the kept kitchen draws Granny in (cook_poses(): her calm, talking,
 * cheering and dismayed poses at the counter, her talking poses under the recipe book) with the
 * core's own granny_draw at 240x160 and cut off where the counter covers her (cookpose_blob.s,
 * build/gen/poses.bin: "GRN1", n, a table sorted by key of { key, offset, cx, cy, clip, x0, y0, w,
 * h, pad } (24 B), then 8bpp pictures, index 0 a hole).
 * A pose is one scale-1 image (~1.3 ms for her 35 x 62 px) where drawing her is ~10 ms of
 * ellipses. A key that is not baked, or baked for another place, says "cannot": the kitchen
 * draws her. No RAM. */
#include "gba_plat.h"
#include "../../core/src/fishcook_int.h"

extern const uint8_t gba_poses[];             /* cookpose_blob.s */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int rd16s(const uint8_t *p) { return (int16_t)(uint16_t)(p[0] | p[1] << 8); }

int plat_cook_pose(uint32_t key, int cx, int cy, int clip, int s, int t, int mood, int box[4])
{
    const uint8_t *b = gba_poses, *e;
    int lo = 0, hi, mid;
    DoomoImage img;
    (void)s; (void)t; (void)mood;
    if (b[0] != 'G' || b[1] != 'R' || b[2] != 'N' || b[3] != '1') return 0;
    hi = (int)rd32(b + 4) - 1;
    while (lo <= hi) {                         /* the first entry with this key */
        mid = (lo + hi) / 2;
        if (rd32(b + 8 + 24 * mid) < key) lo = mid + 1;
        else hi = mid - 1;
    }
    for (e = b + 8 + 24 * lo; lo < (int)rd32(b + 4) && rd32(e) == key; lo++, e += 24) {
        if (rd16s(e + 8) != cx || rd16s(e + 10) != cy || rd16s(e + 12) != clip) continue;
        if (!rd32(e + 4)) return 0;            /* a pose that paints index 0: drawn in full */
        if (box) {
            box[0] = rd16s(e + 14); box[1] = rd16s(e + 16); box[2] = rd16s(e + 18); box[3] = rd16s(e + 20);
            return 1;
        }
        if (gba_nodraw == 1) return 1;
        img.w = (uint16_t)rd16s(e + 18);
        img.h = (uint16_t)rd16s(e + 20);
        img.pix = b + rd32(e + 4);
        img.uid = 0xC00C0000u + (uint32_t)lo;
        plat_image(rd16s(e + 14), rd16s(e + 16), &img, 1, 0, 0);
        return 1;
    }
    return 0;
}
