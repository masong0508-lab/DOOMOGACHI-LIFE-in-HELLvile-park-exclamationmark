/* part_icons.c - the creator's part icon generator (CR7; owner: creator). Host-only: links the
 * whole core (the real renderer and the part catalog) plus the native platform (PNG writer).
 *
 *   part_icons --out-root ROOT          write ROOT/core/gen/part_icons_data.c (raw 20x20 icons)
 *                                       and ROOT/core/gen/part_icons_rle.c (the same, packed)
 *   part_icons --sheet DIR [--zoom N]   write DIR/part_icons.png, the icon contact sheet
 *
 * Every live part of a slot drawn from the look (doomo_part_slots whose list is a DoomoLook field)
 * and every species gets one icon, rendered by the game's own renderer on a fixed mannequin:
 *   - the camera is the bust (portrait_render) for head slots and the full figure (figure_render)
 *     for body, outfit, tail, wings, markings and aura;
 *   - in slots whose first choice is "None" (hats, horns, tops ...) the part is shown alone on a
 *     neutral grey mannequin: every pixel the part did not change is greyed by its brightness, so
 *     the part's own colours are the only colour in the icon; choice 0 is the grey mannequin with
 *     a red slash. In slots where every choice is a shape (species, eyes, head shapes, mouths)
 *     the icon is the part in colour;
 *   - each slot uses ONE window for all its icons (the union of the pixels its choices change,
 *     squared), so a slot's icons share their zoom; a window wider than 20 px is box-sampled down,
 *     each output pixel taking the most common part colour of its box (the darkest on a tie, so
 *     outlines survive) and the mannequin only where the part is absent.
 * Deterministic: the same catalog and renderer give byte-identical files (make check-gen). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/part_meta.h"
#include "doomo/part_icons.h"
#include "doomo/parts.h"
#include "doomo/look_fields.h"
#include "../src/creator_int.h"
#include "native.h"

#define IW PART_ICON_W
#define MAXV 256

enum { ICAM_BUST, ICAM_FIG };

typedef struct Icon { int row; uint8_t pix[PART_ICON_PIX]; } Icon;
static Icon *icons;
static int nicons;

/* the look parts the icon tool draws (look byte 1 + GF_x) */
static int is_look_list(int list)
{
    switch (list) {
    case GF_BODY: case GF_HEAD_SHAPE: case GF_EYE_STYLE: case GF_BROW_STYLE: case GF_NOSE_STYLE:
    case GF_MOUTH_STYLE: case GF_HAIR_STYLE: case GF_FACIAL_HAIR: case GF_EARS: case GF_HORNS:
    case GF_TAIL: case GF_WINGS: case GF_HALO: case GF_MARKING: case GF_TOP: case GF_BOTTOM:
    case GF_SHOES: case GF_HAT: case GF_ACCESSORY: case GF_AURA: case GF_PATTERN:
    /* CR3: the gene's new fields (drawn through the resolved set, look_part_set) */
    case GF_PUPIL: case GF_SCLERA: case GF_FUR_PATTERN: case GF_GLASSES: case GF_OUTER: case GF_LEGWEAR:
    case GF_BLUSH_STYLE: case GF_EAR_FOLD:
        return 1;
    default:
        return 0;
    }
}

static int cam_of(int list)
{
    switch (list) {
    case GF_BODY: case GF_TAIL: case GF_WINGS: case GF_TOP: case GF_BOTTOM: case GF_SHOES:
    case GF_PATTERN: case GF_AURA: case GF_MARKING: case GF_FUR_PATTERN: case GF_OUTER: case GF_LEGWEAR:
        return ICAM_FIG;
    default:
        return ICAM_BUST;
    }
}

/* the mannequin: the default citizen, with readable part colours */
static void mannequin(DoomoLook *l, int list)
{
    DoomoResident r;
    resident_default(&r);
    res_get_look(&r, l);
    l->hat_color = PAINT_LAVENDER;
    l->accessory_color = PAINT_COBALT;
    l->marking_color = PAINT_CHOCOLATE;
    l->horn_color = PAINT_CHERRY;
    if (list == GF_FACIAL_HAIR) l->hair_color = PAINT_CHOCOLATE;
    if (list == GF_PATTERN) { l->top = TOP_TSHIRT; l->pattern_color = PAINT_CHERRY; }
    if (list == GF_HAT || list == GF_HORNS || list == GF_HALO || list == GF_HEAD_SHAPE || list == GF_EARS ||
        list == GF_EYE_STYLE || list == GF_MOUTH_STYLE || list == GF_BROW_STYLE || list == GF_NOSE_STYLE ||
        list == GF_GLASSES || list == GF_EAR_FOLD)
        l->hair_style = 0;                        /* bald: the part reads alone */
    if (list == GF_EAR_FOLD) l->ears = EARS_BUNNY;              /* CR3: long ears show a fold */
    if (list == GF_LEGWEAR) { l->bottom = BOTTOM_SHORTS; l->shoes = SHOES_SANDALS; l->pattern_color = PAINT_CHERRY; }
    if (list == GF_OUTER) l->top_color = PAINT_WHITE;
    if (list == GF_FUR_PATTERN) { l->top = TOP_NONE; l->skin = 8 + PAINT_CARAMEL; }
}

static void species_look(DoomoLook *l, int sp)
{
    DoomoResident r;
    const CrPreset *p = &cr_species_presets[sp * CR_PRESETS_PER_SPECIES];
    int i;
    resident_default(&r);
    for (i = 0; i + 1 < CR_PRESET_KV && p->kv[i] != CR_NOFIELD; i += 2)
        if (p->kv[i] < RES_F_NAME) res_field_set(&r, p->kv[i], p->kv[i + 1]);
    RES_SET_SPECIES(&r, (uint8_t)sp);
    res_get_look(&r, l);
}

static void render(int cam, const DoomoLook *l, const DoomoDeep *d, uint8_t *out)
{
    if (cam == ICAM_FIG) figure_render_deep(l, d, RES_ALIVE, EXPR_NEUTRAL, 0, out);
    else portrait_render_deep(l, d, RES_ALIVE, EXPR_NEUTRAL, 0, out);
}

/* The mannequin of a grey icon is a flat silhouette: light grey with a mid grey rim where it meets
 * the transparent background, so the part's own colours are the only colour in the icon. */
/* the baseline `b` moved by (dx, dy) (pixels moved in from outside are transparent) */
static uint8_t at(const uint8_t *b, int iw, int ih, int x, int y, int dx, int dy)
{
    x -= dx; y -= dy;
    return x >= 0 && y >= 0 && x < iw && y < ih ? b[y * iw + x] : 0;
}

/* Some parts move the whole citizen (platform soles lift the body, a tall hat squashes the head):
 * compare with the mannequin moved the same way, so only the part itself counts as the part. */
static void best_shift(const uint8_t *p, const uint8_t *b, int iw, int ih, int *bdx, int *bdy)
{
    int dx, dy, x, y, best = -1;
    *bdx = 0; *bdy = 0;
    for (dy = -8; dy <= 8; dy++)
        for (dx = -2; dx <= 2; dx++) {
            int d = 0;
            for (y = 0; y < ih; y++)
                for (x = 0; x < iw; x++) d += p[y * iw + x] != at(b, iw, ih, x, y, dx, dy);
            if (best < 0 || d < best || (d == best && abs(dx) + abs(dy) < abs(*bdx) + abs(*bdy))) {
                best = d; *bdx = dx; *bdy = dy;
            }
        }
}

/* one icon from a render `p` (and the mannequin `b`, moved by (dx, dy), in grey mode, else NULL)
 * through the window */
static void sample(const uint8_t *p, const uint8_t *b, int iw, int ih, int dx, int dy, int x0, int y0, int side,
                   const uint8_t *foreign, uint8_t *out)
{
    uint8_t man[PART_ICON_PIX];
    int ox, oy;
    memset(man, 0, sizeof(man));
    for (oy = 0; oy < IW; oy++)
        for (ox = 0; ox < IW; ox++) {
            int sx0 = x0 + ox * side / IW, sx1 = x0 + (ox + 1) * side / IW;
            int sy0 = y0 + oy * side / IW, sy1 = y0 + (oy + 1) * side / IW;
            uint8_t cols[64];
            int cnt[64], nc = 0, x, y, best = -1, i, nback = 0;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (y = sy0; y < sy1; y++)
                for (x = sx0; x < sx1; x++) {
                    uint8_t c = p[y * iw + x];
                    uint8_t bc = b ? at(b, iw, ih, x, y, dx, dy) : 0;
                    int part = c && (!b || bc != c);
                    /* a part that reshapes the head (tall hats, big horns): inside the mannequin only
                     * colours the mannequin does not use are the part */
                    if (part && foreign && bc && !foreign[c]) part = 0;
                    if (!part) {
                        if (c) nback++;
                        if (!b && c) part = 1;
                        else continue;
                    }
                    for (i = 0; i < nc && cols[i] != c; i++) {}
                    if (i == nc && nc < 64) { cols[nc] = c; cnt[nc] = 0; nc++; }
                    if (i < nc) cnt[i]++;
                }
            for (i = 0; i < nc; i++)
                if (best < 0 || cnt[i] > cnt[best] ||
                    (cnt[i] == cnt[best] && palette_luma(cols[i]) < palette_luma(cols[best])))
                    best = i;
            if (best >= 0) out[oy * IW + ox] = cols[best];
            else if (nback) {
                out[oy * IW + ox] = PAL_RAMP(RAMP_STONE, 6);
                man[oy * IW + ox] = 1;
            } else out[oy * IW + ox] = 0;
        }
    /* the silhouette's rim */
    for (oy = 0; oy < IW; oy++)
        for (ox = 0; ox < IW; ox++) {
            if (!man[oy * IW + ox]) continue;
            if ((ox > 0 && !out[oy * IW + ox - 1]) || (ox < IW - 1 && !out[oy * IW + ox + 1]) ||
                (oy > 0 && !out[(oy - 1) * IW + ox]) || (oy < IW - 1 && !out[(oy + 1) * IW + ox]))
                out[oy * IW + ox] = PAL_RAMP(RAMP_STONE, 4);
        }
}

/* a red slash over the mannequin: "none" */
static void slash(uint8_t *out)
{
    int i;
    for (i = 3; i < IW - 3; i++) {
        out[(IW - 1 - i) * IW + i] = PAL_PAINT(PAINT_CHERRY, 1);
        out[(IW - 1 - i) * IW + i + 1] = PAL_PAINT(PAINT_CHERRY, 0);
    }
}

/* At most 15 colours per icon (plus transparent), so every icon packs at 4 bits a pixel: the
 * least used colour merges into its nearest neighbour until 15 are left. */
static void quantize(uint8_t *pix)
{
    for (;;) {
        int cnt[256], i, nc = 0, worst = -1, best = -1;
        uint32_t bd = 0xFFFFFFFFu;
        uint8_t wr, wg, wb;
        memset(cnt, 0, sizeof(cnt));
        for (i = 0; i < PART_ICON_PIX; i++) if (pix[i]) cnt[pix[i]]++;
        for (i = 1; i < 256; i++)
            if (cnt[i]) { nc++; if (worst < 0 || cnt[i] < cnt[worst]) worst = i; }
        if (nc <= 15) return;
        palette_rgb((uint8_t)worst, &wr, &wg, &wb);
        for (i = 1; i < 256; i++) {
            uint8_t r, g, b;
            uint32_t d;
            if (!cnt[i] || i == worst) continue;
            palette_rgb((uint8_t)i, &r, &g, &b);
            d = palette_distance(wr, wg, wb, r, g, b);
            if (d < bd) { bd = d; best = i; }
        }
        for (i = 0; i < PART_ICON_PIX; i++) if (pix[i] == worst) pix[i] = (uint8_t)best;
    }
}

static void add_icon(int row, const uint8_t *pix)
{
    icons[nicons].row = row;
    memcpy(icons[nicons].pix, pix, PART_ICON_PIX);
    quantize(icons[nicons].pix);
    nicons++;
}

static void do_slot(const PartSlotInfo *sl)
{
    static uint8_t img[MAXV][FIGURE_W * FIGURE_H];
    static uint8_t base[FIGURE_W * FIGURE_H];
    uint8_t foreign[256];
    int list = sl->list, cam, iw, ih, grey, i, n = 0, rows[MAXV], sdx[MAXV], sdy[MAXV];
    int bx0 = 1 << 20, by0 = 1 << 20, bx1 = -1, by1 = -1, x, y, side, cx, cy, x0, y0;
    const char *n0;
    if (list == PL_SPECIES) cam = ICAM_BUST;
    else if (is_look_list(list)) cam = cam_of(list);
    else return;
    iw = cam == ICAM_FIG ? FIGURE_W : PORTRAIT_W;
    ih = cam == ICAM_FIG ? FIGURE_H : PORTRAIT_H;
    n0 = sl->count ? part_meta_name(&doomo_part_meta[sl->first]) : "";
    grey = list != PL_SPECIES && (!strcmp(n0, "None") || !strcmp(n0, "Bald") || !strcmp(n0, "Barefoot") ||
                                  !strcmp(n0, "Solid"));
    /* the mannequin (grey mode: the slot's choice 0) */
    if (grey) {
        DoomoLook l;
        DoomoDeep d = doomo_deep_zero;
        mannequin(&l, list);
        look_part_set(&l, &d, list, 0);
        render(cam, &l, &d, base);
    }
    for (i = sl->first; i < sl->first + sl->count && n < MAXV; i++) {
        const PartMeta *m = &doomo_part_meta[i];
        DoomoLook l;
        DoomoDeep d = doomo_deep_zero;
        if (PMF_STATE(m->flags) != PSTATE_LIVE) continue;
        if (list == PL_SPECIES) species_look(&l, m->value);
        else { mannequin(&l, list); look_part_set(&l, &d, list, m->value); }
        render(cam, &l, &d, img[n]);
        sdx[n] = sdy[n] = 0;
        if (grey) best_shift(img[n], base, iw, ih, &sdx[n], &sdy[n]);
        rows[n++] = i;
    }
    if (!n) return;
    memset(foreign, 1, sizeof(foreign));
    if (grey) for (i = 0; i < iw * ih; i++) foreign[base[i]] = 0;
    /* the window: what the slot's choices change */
    for (y = 0; y < ih; y++)
        for (x = 0; x < iw; x++) {
            int k, ch = 0;
            for (k = 0; k < n && !ch; k++) {
                uint8_t c = img[k][y * iw + x];
                ch = grey ? (c != at(base, iw, ih, x, y, sdx[k], sdy[k])) :
                            (c != img[0][y * iw + x] || (list == PL_SPECIES && c));
            }
            if (!ch) continue;
            if (x < bx0) bx0 = x;
            if (y < by0) by0 = y;
            if (x > bx1) bx1 = x;
            if (y > by1) by1 = y;
        }
    if (bx1 < 0) { bx0 = 0; by0 = 0; bx1 = iw - 1; by1 = ih - 1; }
    side = DOOMO_MAX(bx1 - bx0 + 1, by1 - by0 + 1) + 2;
    if (side < IW) side = IW;
    if (side > DOOMO_MIN(iw, ih)) side = DOOMO_MIN(iw, ih);
    cx = (bx0 + bx1 + 1) / 2;
    cy = (by0 + by1 + 1) / 2;
    x0 = DOOMO_CLAMP(cx - side / 2, 0, iw - side);
    y0 = DOOMO_CLAMP(cy - side / 2, 0, ih - side);
    for (i = 0; i < n; i++) {
        uint8_t out[PART_ICON_PIX];
        sample(img[i], grey ? base : NULL, iw, ih, sdx[i], sdy[i], x0, y0, side,
               list == GF_HAT || list == GF_HORNS ? foreign : NULL, out);
        if (grey && doomo_part_meta[rows[i]].value == 0) slash(out);
        add_icon(rows[i], out);
    }
    printf("part_icons: %-10s %3d icons, %s, window %d,%d %dx%d%s\n", sl->key, n,
           cam == ICAM_FIG ? "figure" : "bust", x0, y0, side, side, grey ? ", grey mannequin" : "");
}

static int cmp_icon(const void *a, const void *b)
{
    return ((const Icon *)a)->row - ((const Icon *)b)->row;
}

/* ---- output ------------------------------------------------------------------------------ */
/* runs over bytes: n < 0x80 repeats the next byte n + 1 times; n >= 0x80 is n - 0x7F literal bytes */
static int runs(const uint8_t *in, int len, uint8_t *out)
{
    int i = 0, o = 0;
    while (i < len) {
        int n = 1;
        while (i + n < len && in[i + n] == in[i] && n < 128) n++;
        if (n >= 3) {
            out[o++] = (uint8_t)(n - 1);
            out[o++] = in[i];
        } else {
            n = 0;
            while (i + n < len && n < 128 && !(i + n + 2 < len && in[i + n] == in[i + n + 1] && in[i + n] == in[i + n + 2]))
                n++;
            if (!n) n = 1;
            out[o++] = (uint8_t)(0x7F + n);
            memcpy(out + o, in + i, (size_t)n);
            o += n;
        }
        i += n;
    }
    return o;
}

/* packed icon: [k = colours, 1..15][k palette indices][runs over the 200 bytes of 4-bit pixels,
 * high nibble first; nibble 0 transparent, j = palette[j - 1]] (part_icon_unpack) */
static int pack(const uint8_t *pix, uint8_t *out)
{
    uint8_t pal[15], nib[PART_ICON_PIX / 2];
    int k = 0, i, o;
    for (i = 0; i < PART_ICON_PIX; i++) {
        int j;
        if (!pix[i]) continue;
        for (j = 0; j < k && pal[j] != pix[i]; j++) {}
        if (j == k) pal[k++] = pix[i];
    }
    for (i = 0; i < PART_ICON_PIX; i += 2) {
        int a = 0, b = 0, j;
        for (j = 0; j < k; j++) { if (pal[j] == pix[i]) a = j + 1; if (pal[j] == pix[i + 1]) b = j + 1; }
        nib[i / 2] = (uint8_t)(a << 4 | b);
    }
    out[0] = (uint8_t)k;
    memcpy(out + 1, pal, (size_t)k);
    o = 1 + k;
    return o + runs(nib, PART_ICON_PIX / 2, out + o);
}

static FILE *open_out(const char *root, const char *name)
{
    char path[1024];
    FILE *f;
    snprintf(path, sizeof(path), "%s/core/gen/%s", root, name);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "part_icons: cannot write %s\n", path); exit(1); }
    return f;
}

/* CR3: a part the PS1 can draw (no part-script VM until CR10: the values before the list's first
 * script, parts.h doomo_part_minv); only these are packed for it (part_icons.h) */
static int vm0_drawable(int row)
{
    const PartMeta *m = &doomo_part_meta[row];
    return m->list >= 128 || m->value < doomo_part_minv[m->list];
}

static void write_gen(const char *root)
{
    FILE *f;
    int i, k, np = 0;
    uint8_t *rle = (uint8_t *)malloc((size_t)nicons * (PART_ICON_PIX + PART_ICON_PIX / 64 + 8));
    uint32_t *ofs = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)(nicons + 1));
    int *prow = (int *)malloc(sizeof(int) * (size_t)(nicons + 1));
    uint32_t total = 0;
    for (i = 0; i < nicons; i++) {
        if (!vm0_drawable(icons[i].row)) continue;
        ofs[np] = total;
        total += (uint32_t)pack(icons[i].pix, rle + total);
        prow[np++] = icons[i].row;
    }
    ofs[np] = total;

    f = open_out(root, "part_icons_data.c");
    fprintf(f, "/* part_icons_data.c - GENERATED by core/tools/part_icons.c (make -C core gen) from the\n"
               " * renderer and core/data/parts/catalog/. DO NOT EDIT. The raw 20x20 part icons (CR7,\n"
               " * part_icons.h): GBA and native blit these from ROM. %d icons, %d bytes. */\n"
               "#include \"doomo/part_icons.h\"\n\n", nicons, nicons * PART_ICON_PIX);
    fprintf(f, "/* the raw icons' doomo_part_meta rows */\n");
    fprintf(f, "const uint16_t doomo_part_icon_count = %d;\n\n", nicons);
    fprintf(f, "const uint16_t doomo_part_icon_rows[%d] = {", nicons);
    for (i = 0; i < nicons; i++) fprintf(f, "%s%d,", i % 16 ? " " : "\n    ", icons[i].row);
    fprintf(f, "\n};\n\n");
    fprintf(f, "const uint8_t doomo_part_icon_pix[%d][PART_ICON_PIX] = {\n", nicons);
    for (i = 0; i < nicons; i++) {
        fprintf(f, "    {   /* %d %s */", icons[i].row, part_meta_name(&doomo_part_meta[icons[i].row]));
        for (k = 0; k < PART_ICON_PIX; k++) fprintf(f, "%s%d,", k % 20 ? "" : "\n        ", icons[i].pix[k]);
        fprintf(f, "\n    },\n");
    }
    fprintf(f, "};\n");
    fclose(f);

    f = open_out(root, "part_icons_rle.c");
    fprintf(f, "/* part_icons_rle.c - GENERATED by core/tools/part_icons.c (make -C core gen). DO NOT EDIT.\n"
               " * The part icons the PS1 can draw (CR3: no part scripts until CR10), packed at 4 bits a pixel\n"
               " * (part_icon_unpack): the PS1 links these instead of part_icons_data.c, in the creator overlay\n"
               " * group. %d icons, %u bytes. */\n"
               "#include \"doomo/part_icons.h\"\n\n", np, (unsigned)total);
    fprintf(f, "/* the packed icons' doomo_part_meta rows (every build links these) */\n");
    fprintf(f, "const uint16_t doomo_part_icon_rle_count = %d;\n\n", np);
    fprintf(f, "const uint16_t doomo_part_icon_rle_rows[%d] = {", np);
    for (i = 0; i < np; i++) fprintf(f, "%s%d,", i % 16 ? " " : "\n    ", prow[i]);
    fprintf(f, "\n};\n\n");
    fprintf(f, "const uint32_t doomo_part_icon_rle_ofs[%d] = {", np + 1);
    for (i = 0; i <= np; i++) fprintf(f, "%s%u,", i % 12 ? " " : "\n    ", (unsigned)ofs[i]);
    fprintf(f, "\n};\n\nconst uint8_t doomo_part_icon_rle[%u] = {", (unsigned)(total ? total : 1));
    for (k = 0; k < (int)total; k++) fprintf(f, "%s%d,", k % 24 ? "" : "\n    ", rle[k]);
    if (!total) fprintf(f, "0");
    fprintf(f, "\n};\n");
    fclose(f);
    printf("part_icons: %d icons, raw %d B; %d packed for the PS1, %u B\n", nicons, nicons * PART_ICON_PIX, np,
           (unsigned)total);
    free(rle);
    free(ofs);
    free(prow);
}

/* ---- the contact sheet ------------------------------------------------------------------- */
static void put(uint8_t *s, int sw, int sh, int x, int y, uint8_t c)
{
    if (x >= 0 && y >= 0 && x < sw && y < sh) s[y * sw + x] = c;
}

static void text(uint8_t *s, int sw, int sh, int x, int y, const char *t, uint8_t c)
{
    for (; *t; t++, x += FONT_ADVANCE) {
        int ch = (unsigned char)*t, col, row;
        if (ch < FONT_FIRST || ch > FONT_LAST) continue;
        for (col = 0; col < 5; col++)
            for (row = 0; row < 8; row++)
                if (doomo_font[ch - FONT_FIRST][col] & (1 << row)) put(s, sw, sh, x + col, y + row, c);
    }
}

static void write_sheet(const char *dir, int zoom)
{
    int per = 16, cw = 26, chh = 34, i, sl, y = 2, sw = 4 + per * cw, sh = 0;
    uint8_t *s;
    char path[1024];
    for (sl = 0; sl < PART_SLOT_COUNT; sl++) {
        int n = 0;
        for (i = 0; i < nicons; i++) n += doomo_part_meta[icons[i].row].slot == sl;
        if (n) sh += 12 + ((n + per - 1) / per) * chh;
    }
    sh += 4;
    s = (uint8_t *)malloc((size_t)sw * sh);
    memset(s, PAL_UI_CREAM, (size_t)sw * sh);
    for (sl = 0; sl < PART_SLOT_COUNT; sl++) {
        int n = 0;
        for (i = 0; i < nicons; i++) n += doomo_part_meta[icons[i].row].slot == sl;
        if (!n) continue;
        text(s, sw, sh, 2, y + 2, doomo_part_slots[sl].name, PAL_UI_PINK_D);
        y += 12;
        for (i = 0, n = 0; i < nicons; i++) {
            const PartMeta *m = &doomo_part_meta[icons[i].row];
            int x = 2 + (n % per) * cw, yy = y + (n / per) * chh, px, py;
            char nm[6];
            if (m->slot != sl) continue;
            for (py = 0; py < IW + 2; py++)
                for (px = 0; px < IW + 2; px++)
                    put(s, sw, sh, x + px, yy + py, ((px / 4 + py / 4) & 1) ? PAL_RAMP(RAMP_MINT, 6) : PAL_WHITE);
            for (py = 0; py < IW; py++)
                for (px = 0; px < IW; px++)
                    if (icons[i].pix[py * IW + px]) put(s, sw, sh, x + 1 + px, yy + 1 + py, icons[i].pix[py * IW + px]);
            snprintf(nm, sizeof(nm), "%.4s", part_meta_name(m));
            text(s, sw, sh, x, yy + IW + 3, nm, PAL_GRAY_D);
            n++;
        }
        y += ((n + per - 1) / per) * chh;
    }
    snprintf(path, sizeof(path), "%s/part_icons.png", dir);
    if (!nat_png_write(path, s, sw, sh, zoom)) fprintf(stderr, "part_icons: cannot write %s\n", path);
    else printf("part_icons: %s (%dx%d)\n", path, sw * zoom, sh * zoom);
    free(s);
}

int main(int argc, char **argv)
{
    const char *root = NULL, *sheet = NULL;
    int i, zoom = 3;
    NatOptions o;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out-root") && i + 1 < argc) root = argv[++i];
        else if (!strcmp(argv[i], "--sheet") && i + 1 < argc) sheet = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = atoi(argv[++i]);
        else { fprintf(stderr, "usage: part_icons --out-root ROOT | --sheet DIR [--zoom N]\n"); return 2; }
    }
    if (!root && !sheet) { fprintf(stderr, "usage: part_icons --out-root ROOT | --sheet DIR [--zoom N]\n"); return 2; }
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.quiet = 1;
    nat_init(&o);
    palette_init();
    icons = (Icon *)calloc(PART_META_COUNT, sizeof(Icon));
    for (i = 0; i < PART_SLOT_COUNT; i++) do_slot(&doomo_part_slots[i]);
    qsort(icons, (size_t)nicons, sizeof(Icon), cmp_icon);
    if (root) write_gen(root);
    if (sheet) write_sheet(sheet, zoom < 1 ? 1 : zoom);
    free(icons);
    return 0;
}
