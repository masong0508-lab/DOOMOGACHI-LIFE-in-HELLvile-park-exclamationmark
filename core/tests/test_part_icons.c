/* test_part_icons.c - the creator's prerendered part icons (CR7, core/tools/part_icons.c).
 *
 * - one icon per live part of every slot the icon tool draws (the look's part lists and the
 *   species), none for hidden / planned parts, rows sorted and unique;
 * - the packed icons (the PS1's, part_icons_rle.c) unpack to exactly the raw ones (GBA / native);
 * - every icon shows something, at most 15 colours, and part_icon_image() gives stable uids.
 * That the files match the catalog and the renderer is `make check-gen` (the tool regenerates them
 * byte for byte). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/part_icons.h"
#include "doomo/parts.h"
#include "doomo/gene.h"

static int icon_list(int list)
{
    switch (list) {
    case PL_SPECIES: case GF_BODY: case GF_HEAD_SHAPE: case GF_EYE_STYLE: case GF_BROW_STYLE:
    case GF_NOSE_STYLE: case GF_MOUTH_STYLE: case GF_HAIR_STYLE: case GF_FACIAL_HAIR: case GF_EARS:
    case GF_HORNS: case GF_TAIL: case GF_WINGS: case GF_HALO: case GF_MARKING: case GF_TOP:
    case GF_BOTTOM: case GF_SHOES: case GF_HAT: case GF_ACCESSORY: case GF_AURA: case GF_PATTERN:
    /* CR3: the gene's new fields */
    case GF_PUPIL: case GF_SCLERA: case GF_FUR_PATTERN: case GF_GLASSES: case GF_OUTER: case GF_LEGWEAR:
    case GF_BLUSH_STYLE: case GF_EAR_FOLD:
        return 1;
    default:
        return 0;
    }
}

TEST(part_icons_one_per_live_part)
{
    int i, n = 0;
    for (i = 1; i < (int)doomo_part_icon_count; i++)
        CHECK_MSG(doomo_part_icon_rows[i - 1] < doomo_part_icon_rows[i], "icon rows not ascending at %d", i);
    for (i = 0; i < PART_META_COUNT; i++) {
        const PartMeta *m = &doomo_part_meta[i];
        int want = icon_list(m->list) && PMF_STATE(m->flags) == PSTATE_LIVE;
        CHECK_MSG((part_icon_index_in(i, 0) >= 0) == want, "%s \"%s\": icon %s", doomo_part_slots[m->slot].key,
                  part_meta_name(m), want ? "missing" : "for a part that is not live");
        /* CR3: the PS1's packed set has exactly the ones it can draw (no part scripts there) */
        CHECK_MSG((part_icon_index_in(i, 1) >= 0) == (want && (m->list >= 128 || m->value < doomo_part_minv[m->list])),
                  "%s \"%s\": packed icon wrong", doomo_part_slots[m->slot].key, part_meta_name(m));
        n += want;
    }
    CHECK_EQ(n, doomo_part_icon_count);
    CHECK(n >= 300);
    for (i = 1; i < (int)doomo_part_icon_rle_count; i++)
        CHECK_MSG(doomo_part_icon_rle_rows[i - 1] < doomo_part_icon_rle_rows[i], "packed rows not ascending at %d", i);
}

TEST(part_icons_packed_equal_raw)
{
    static uint8_t buf[PART_ICON_PIX];
    int i, k;
    for (i = 0; i < (int)doomo_part_icon_rle_count; i++) {
        int r = part_icon_index_in(doomo_part_icon_rle_rows[i], 0);
        memset(buf, 0xAA, sizeof(buf));
        CHECK_MSG(part_icon_unpack(i, buf), "icon %d does not unpack", i);
        CHECK_MSG(r >= 0 && memcmp(buf, doomo_part_icon_pix[r], PART_ICON_PIX) == 0, "packed icon %d != raw", i);
    }
    for (i = 0; i < (int)doomo_part_icon_count; i++) {
        uint8_t seen[256];
        int ncol = 0, opaque = 0;
        memset(seen, 0, sizeof(seen));
        for (k = 0; k < PART_ICON_PIX; k++) {
            uint8_t c = doomo_part_icon_pix[i][k];
            if (!c) continue;
            opaque++;
            if (!seen[c]) { seen[c] = 1; ncol++; }
        }
        CHECK_MSG(opaque >= 20, "icon %d is (nearly) empty", i);
        CHECK_MSG(ncol <= 15, "icon %d has %d colours", i, ncol);
    }
    CHECK(!part_icon_unpack(-1, buf));
    CHECK(!part_icon_unpack((int)doomo_part_icon_rle_count, buf));
}

TEST(part_icons_image_kept_unpacks_a_row_once_per_buffer)
{
    /* S3-3: the PS1's grid keeps one buffer per cell of the page: the same row again unpacks nothing */
    static uint8_t buf[PART_ICON_PIX], ref[PART_ICON_PIX];
    const PartMeta *m = part_meta_find(GF_HAT, 3);
    const PartMeta *m2 = part_meta_find(GF_HAT, 4);
    DoomoImage a, b;
    uint16_t tag = 0;
    int row, row2;
    CHECK(m != NULL && m2 != NULL);
    row = (int)(m - doomo_part_meta);
    row2 = (int)(m2 - doomo_part_meta);
    CHECK(part_icon_image_kept(row, &a, buf, &tag));
    CHECK_EQ((int)tag, row + 1);
    CHECK(a.pix == buf);
    CHECK(part_icon_image(row, &b, ref));            /* the plain call: same pixels, same uid */
    CHECK(memcmp(buf, ref, PART_ICON_PIX) == 0 || memcmp(buf, b.pix, PART_ICON_PIX) == 0);
    CHECK_EQ((int)a.uid, (int)b.uid);
    memset(buf, 0x5A, sizeof(buf));                  /* the same row again: the buffer is not touched */
    CHECK(part_icon_image_kept(row, &a, buf, &tag));
    CHECK_EQ(buf[0], 0x5A);
    CHECK(a.pix == buf && a.w == PART_ICON_W && a.h == PART_ICON_W);
    CHECK(part_icon_image_kept(row2, &a, buf, &tag));  /* another row: unpacked into it */
    CHECK_EQ((int)tag, row2 + 1);
    CHECK(buf[0] != 0x5A || buf[PART_ICON_PIX - 1] != 0x5A);
    tag = 0;                                          /* a lost buffer (tag 0): unpacked again */
    memset(buf, 0x5A, sizeof(buf));
    CHECK(part_icon_image_kept(row, &a, buf, &tag));
    CHECK(memcmp(buf, doomo_part_icon_pix[part_icon_index(row)], PART_ICON_PIX) == 0);
    CHECK(!part_icon_image_kept(-1, &a, buf, &tag));
}

TEST(part_icons_image_has_a_stable_uid)
{
    static uint8_t buf[PART_ICON_PIX];
    const PartMeta *m = part_meta_find(GF_HAT, 3);           /* Top Hat */
    DoomoImage a, b;
    int row;
    CHECK(m != NULL);
    row = (int)(m - doomo_part_meta);
    CHECK(part_icon_image(row, &a, buf));
    CHECK(part_icon_image(row, &b, buf));
    CHECK_EQ(a.w, PART_ICON_W);
    CHECK_EQ(a.h, PART_ICON_W);
    CHECK(a.uid != 0 && a.uid == b.uid);
    CHECK(a.pix != NULL);
    CHECK(!part_icon_image(-5, &a, buf));
}
