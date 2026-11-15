/* cookpose.c - Granny's baked poses on the GBA (S2-4c; core/src/fishcook_int.h plat_cook_pose;
 * owner: gba; docs/GBA.md "Speed wave 2: S2-4c").
 *
 * The build bakes every pose the kept kitchen draws Granny in (cook_poses(): her calm, talking,
 * cheering and dismayed poses at the counter, her talking poses under the recipe book) with the
 * core's own granny_draw at 240x160 and cut off where the counter covers her (cookpose_blob.s,
 * build/gen/poses.bin: "GRN1", n, a table sorted by key of { key, offset, cx, cy, clip, x0, y0, w,
 * h, pad } (24 B), then 8bpp pictures, index 0 a hole; cook_pose_find in cook_scene.c reads it, and
 * test_cook_kept bakes the file and checks every pose found there against granny_draw).
 * A pose is one scale-1 image (~1.3 ms for her 35 x 62 px) where drawing her is ~10 ms of
 * ellipses. A key that is not baked, or baked for another place, says "cannot": the kitchen
 * draws her. No RAM. */
#include "gba_plat.h"
#include "../../core/src/fishcook_int.h"

extern const uint8_t gba_poses[];             /* cookpose_blob.s */

int plat_cook_pose(uint32_t key, int cx, int cy, int clip, int s, int t, int mood, int box[4])
{
    const uint8_t *pix;
    int b[4], i;
    DoomoImage img;
    (void)s; (void)t; (void)mood;
    i = cook_pose_find(gba_poses, key, cx, cy, clip, b, &pix);   /* (the lookup: cook_scene.c, tested natively) */
    if (i < 0) return 0;
    if (box) {
        box[0] = b[0]; box[1] = b[1]; box[2] = b[2]; box[3] = b[3];
        return 1;
    }
    if (gba_nodraw == 1) return 1;
    img.w = (uint16_t)b[2];
    img.h = (uint16_t)b[3];
    img.pix = pix;
    img.uid = 0xC00C0000u + (uint32_t)i;
    plat_image(b[0], b[1], &img, 1, 0, 0);
    return 1;
}
