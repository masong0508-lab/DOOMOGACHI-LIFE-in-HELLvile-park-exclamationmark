/* look_gene.c - the gene's new core fields as part of the resolved look (CR3; owner: portrait).
 *
 * P0.M0a gave every saved gene eleven new core fields (pupil, sclera, fur_pattern, fur_color3,
 * glasses, outer, legwear, blush_style, ear_fold, hood_up, handedness; core/data/look/fields.txt),
 * all 0 in every record so far. They have no DoomoLook byte (the look is the 48 B v1 image), so the
 * renderer reads them from the resolved set (DoomoDeep, look_fields.h) next to the deep fields: the
 * wave-2 painters already draw DF_PUPIL ... DF_HOOD_HEAD, and the hooks of the new slots read the rest.
 * No save byte changes: the bits were always there. Kept out of look_pool.c (the provisional pool
 * shim P0 replaces whole) so P0 keeps it. Pure, no state. */
#include "doomo/doomo.h"
#include "doomo/gene.h"
#include "doomo/look_fields.h"
#include "doomo/part_meta.h"

#if DOOMO_GENE_EXT
/* gene field -> resolved-set slot */
static const uint8_t gmap[][2] = {
    { GF_PUPIL, DF_PUPIL }, { GF_SCLERA, DF_SCLERA }, { GF_BLUSH_STYLE, DF_BLUSH2 },
    { GF_GLASSES, DF_GLASSES2 }, { GF_EAR_FOLD, DF_EAR_FOLD }, { GF_HOOD_UP, DF_HOOD_HEAD },
    { GF_FUR_PATTERN, DF_FUR_PATTERN }, { GF_FUR_COLOR3, DF_FUR_COLOR3 }, { GF_OUTER, DF_OUTER },
    { GF_LEGWEAR, DF_LEGWEAR }, { GF_HANDEDNESS, DF_HANDED },
};
#define HOOD_UP_PICTURE 2        /* hood_up = 1 draws the wave-2 "up, face opening" hood */

void look_gene_deep(const DoomoResident *r, uint32_t lv, DoomoDeep *out)
{
    int i, stage;
    if (!r || !out) return;
    stage = (int)gene_get(r->gene, GF_AGE);
    for (i = 0; i < (int)DOOMO_COUNTOF(gmap); i++) {
        int gf = gmap[i][0], df = gmap[i][1];
        uint32_t v = gene_get(r->gene, gf);
        if (lv == LV_CHIBI && !(doomo_df_flags[df] & DFF_CHIBI)) continue;
        if (v && gf != GF_HOOD_UP && gf != GF_FUR_COLOR3 && gf != GF_HANDEDNESS &&
            !part_age_ok(gf, (int)v, stage))
            v = 0;                                       /* CR12: the face of their age */
        if (gf == GF_HOOD_UP && v) v = HOOD_UP_PICTURE;
        out->v[df] = (uint8_t)v;
    }
}

void look_gene_deep_put(DoomoResident *r, const DoomoDeep *d)
{
    int i;
    if (!r || !d) return;
    for (i = 0; i < (int)DOOMO_COUNTOF(gmap); i++) {
        int gf = gmap[i][0], v = d->v[gmap[i][1]];
        if (gf == GF_HOOD_UP) v = v != 0;
        gene_set(r->gene, gf, (uint32_t)v);
    }
}

/* ---- the gene bits the v1 save stream does not carry (CR3) ---------------------------------------
 * Saves are still the v1 wire image (save.c SaveResV1, until P0.M1 stores the gene): it has no place
 * for the eleven new fields, and it gives aura and print 4 bits and the eye count 2 (their genes have
 * 5, 5 and 3). The creator's own save chunk ('NICK', creator.c) carries these bits after its nicknames for
 * the records that have any: [u16 count] then count x 8 B: u16 id (LE), 48 bits LSB first: pupil 5,
 * sclera 4, fur_pattern 5, fur_color3 6, glasses 5, outer 5, legwear 4, blush_style 4, ear_fold 3,
 * hood_up 1, handedness 1, then aura bit 4, print bit 4, eye_count bit 2, 2 spare (0). The v1 stream
 * keeps writing the low bits (its writer masks them), so an old build reads the same record with the
 * high bit dropped. A town nobody customised has no entry and the chunk stays as it was. */
static const uint8_t xf[] = {
    GF_PUPIL, GF_SCLERA, GF_FUR_PATTERN, GF_FUR_COLOR3, GF_GLASSES, GF_OUTER, GF_LEGWEAR,
    GF_BLUSH_STYLE, GF_EAR_FOLD, GF_HOOD_UP, GF_HANDEDNESS
};
#define XHI_N 3
static const uint8_t xhi[XHI_N][2] = { { GF_AURA, 4 }, { GF_PATTERN, 4 }, { GF_EYE_COUNT, 2 } };

/* One walk over the 14 pieces of an entry's 48 bits (the 11 fields whole, then the high bit of aura,
 * print and eye count), either way: write != 0 packs the record's bits into b[6] (no 64-bit maths: the
 * GBA and the PS1 would call libgcc for it), else sets the record's bits from b. One body keeps the
 * PS1's resident bytes down. */
static void ext_xfer(DoomoResident *r, uint8_t *b, int write)
{
    int i, k, at = 0;
    for (i = 0; i < (int)DOOMO_COUNTOF(xf) + XHI_N; i++) {
        int hi = i >= (int)DOOMO_COUNTOF(xf), gf = hi ? xhi[i - DOOMO_COUNTOF(xf)][0] : xf[i];
        int sh = hi ? xhi[i - DOOMO_COUNTOF(xf)][1] : 0, w = hi ? 1 : doomo_gene_field[gf].bits;
        uint32_t v = gene_get(r->gene, gf), x = 0;
        for (k = 0; k < w; k++, at++) {
            int m = at >> 3, bit = at & 7;
            if (write) {
                if ((v >> (sh + k)) & 1u) b[m] = (uint8_t)(b[m] | (1u << bit));
            } else {
                x |= (uint32_t)((b[m] >> bit) & 1u) << k;
            }
        }
        if (!write) gene_set(r->gene, gf, (v & ~(((1u << w) - 1u) << sh)) | (x << sh));
    }
}

/* the 48 extra bits of a record into b[6]; returns 1 if any is set */
static int ext_bits(const DoomoResident *r, uint8_t *b)
{
    memset(b, 0, 6);
    ext_xfer((DoomoResident *)r, b, 1);          /* write mode only reads the record */
    return (b[0] | b[1] | b[2] | b[3] | b[4] | b[5]) != 0;
}

static const DoomoResident *ext_rec(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status != RES_EMPTY ? r : NULL;
}

/* the chunk's entries: dst NULL only counts them; returns the entry count, written (or counted) */
static uint32_t ext_emit(uint8_t *dst)
{
    uint8_t b[6];
    uint32_t n = 0;
    int id;
    for (id = 0; id <= DOOMO_PLAYER_ID; id++) {
        const DoomoResident *r = ext_rec(id);
        if (!r || !ext_bits(r, b)) continue;
        if (dst) {
            uint8_t *e = dst + 2 + 8 * n;
            e[0] = (uint8_t)(id & 255);
            e[1] = (uint8_t)(id >> 8);
            memcpy(e + 2, b, 6);
        }
        n++;
    }
    return n;
}

uint32_t look_gene_ext_size(void)
{
    uint32_t n = ext_emit(NULL);
    return n ? 2 + 8 * n : 0;
}

uint32_t look_gene_ext_write(uint8_t *dst, uint32_t cap)
{
    uint32_t n = ext_emit(NULL);
    if (!n || 2 + 8 * n > cap) return 0;
    ext_emit(dst);
    dst[0] = (uint8_t)(n & 255);
    dst[1] = (uint8_t)(n >> 8);
    return 2 + 8 * n;
}

int look_gene_ext_read(const uint8_t *src, uint32_t len)
{
    uint32_t n, i;
    int ok = 1;
    if (len < 2) return len == 0;
    n = (uint32_t)(src[0] | (src[1] << 8));
    if (2 + 8 * n > len) return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *e = src + 2 + 8 * i;
        int id = e[0] | (e[1] << 8);
        DoomoResident *r = id <= DOOMO_PLAYER_ID ? town_res(id) : NULL;
        if (!r || r->status == RES_EMPTY) { ok = 0; continue; }
        ext_xfer(r, (uint8_t *)(e + 2), 0);           /* read mode only reads the bytes */
    }
    return ok;
}

#endif   /* DOOMO_GENE_EXT: without it look_fields.h has macros for these */

uint32_t deep_hash(const DoomoDeep *d, int chibi)
{
    uint32_t h = 0;                                      /* 0 stays 0: a Classic look adds nothing */
    int f;
    if (!d) return 0;
    for (f = 0; f < DF_COUNT; f++)
        if (!chibi || (doomo_df_flags[f] & DFF_CHIBI)) h = (h + d->v[f]) * 0x01000193u;
    return h;
}

#if DOOMO_GENE_EXT
/* ---- one part on a look you hold (tests, tools) ------------------------------------------------ */
int look_part_set(DoomoLook *l, DoomoDeep *d, int list, int v)
{
    int i;
    if (list == 127) { l->species = (uint8_t)v; return 1; }        /* PL_SPECIES */
    if (list >= 0 && list <= GF_PATTERN_COLOR) { *((uint8_t *)l + 1 + list) = (uint8_t)v; return 1; }
    for (i = 0; i < (int)DOOMO_COUNTOF(gmap); i++)
        if (gmap[i][0] == list) {
            if (list == GF_HOOD_UP && v) v = HOOD_UP_PICTURE;
            d->v[gmap[i][1]] = (uint8_t)v;
            return 1;
        }
    if (list >= 128 && list - 128 < DF_COUNT) { d->v[list - 128] = (uint8_t)v; return 1; }   /* PL_DEEP0 */
    return 0;
}
#endif
