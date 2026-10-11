/* artgen.c - build-time art generator (native host tool, owner: art).
 *
 *   artgen --out-root DIR     write DIR/core/gen/art_data.c
 *   artgen --out FILE         write the data file to FILE
 *   artgen --sheet DIR        write PNG contact sheets (palette, colormap,
 *                             tints, textures + light levels, tiling
 *                             previews, skies, sprites) for review
 *   artgen --zoom N           integer zoom of the sheets (default 2)
 *   artgen --check            run the generators and validations only
 *
 * Everything is integer-only and seeded, so art_data.c is byte-identical on
 * every machine (make -C core check-gen relies on it). This tool may use
 * stdio/malloc freely: it never ships in a ROM.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "art.h"

/* palette.c references these generated tables; artgen itself never calls
 * palette_init(), so empty placeholders keep it linking. */
const uint8_t doomo_art_colormap[DOOMO_LIGHT_LEVELS][256];
const uint8_t doomo_art_tintmap[TINT_COUNT][256];
const uint8_t doomo_art_darker[256];
const uint8_t doomo_art_lighter[256];

/* ---- frame pixel pool ---------------------------------------------------- */
#define POOL_SIZE (4 * 1024 * 1024)
static uint8_t pool[POOL_SIZE];
static int pool_used;

uint8_t *art_alloc(int bytes)
{
    uint8_t *p;
    if (bytes < 0 || pool_used + bytes > POOL_SIZE) {
        fprintf(stderr, "artgen: frame pool exhausted\n");
        exit(1);
    }
    p = pool + pool_used;
    pool_used += bytes;
    return p;
}

/* ---- generated results ------------------------------------------------------ */
#define TEXPX (64 * 64)
static uint8_t tex_pix[DOOMO_TEX_MAX_FRAMES][TEXPX];
static int tex_nframes;
typedef struct { int first, frames, speed, mode, avg, dark; const char *name; int defined; } AnimOut;
static AnimOut tex_out[TEX_COUNT];
static uint8_t sky_pix[DOOMO_SKY_MAX_FRAMES][SKY_W * SKY_H];
static int sky_nframes;
static AnimOut sky_out[SKY_COUNT];

typedef struct { int first, frames, flags; const char *name; int defined; } SprOut;
static ArtFrame frames[DOOMO_SPR_MAX_IMAGES];
static int nframes_total;
static SprOut spr_out[SPR_COUNT];

static int errors;
static int quiet;
static int verbose;     /* --stats: sizes + seam diagnostics */
static int partial;     /* --partial: tolerate missing ids (development) */

static void fail(const char *what, const char *name)
{
    fprintf(stderr, "artgen: ERROR %s (%s)\n", what, name);
    errors++;
}

/* seam metric: mean colour jump across the wrap edges vs. the interior */
static int jump(uint8_t a, uint8_t b)
{
    int r1, g1, b1, r2, g2, b2;
    art_rgb(a, &r1, &g1, &b1);
    art_rgb(b, &r2, &g2, &b2);
    return art_abs(r1 - r2) + art_abs(g1 - g2) + art_abs(b1 - b2);
}

static void check_seams(const uint8_t *p, int w, int h, int wrapy, const char *name)
{
    long in = 0, edge = 0, ny = 0;
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x + 1 < w; x++) in += jump(p[y * w + x], p[y * w + x + 1]);
        edge += jump(p[y * w + w - 1], p[y * w]);
    }
    in /= (long)(w - 1);
    if (!verbose) return;
    if (edge > in * 2 + 40L * h) {
        fprintf(stderr, "artgen: warning: %s horizontal seam %ld vs %ld\n", name, edge, in);
    }
    if (wrapy) {
        long iny = 0;
        for (x = 0; x < w; x++) {
            for (y = 0; y + 1 < h; y++) iny += jump(p[y * w + x], p[(y + 1) * w + x]);
            ny += jump(p[(h - 1) * w + x], p[x]);
        }
        iny /= (long)(h - 1);
        if (ny > iny * 2 + 40L * w)
            fprintf(stderr, "artgen: warning: %s vertical seam %ld vs %ld\n", name, ny, iny);
    }
}

static uint8_t darkest(const uint8_t *p, int n)
{
    int i, best = 999;
    uint8_t c = PAL_BLACK;
    for (i = 0; i < n; i++)
        if (p[i] && art_luma(p[i]) < best) { best = art_luma(p[i]); c = p[i]; }
    return c;
}

static void gen_textures(void)
{
    const ArtTexDef *sets[2];
    int ns[2], s, k, f;
    sets[0] = art_tex_cute; ns[0] = art_tex_cute_n;
    sets[1] = art_tex_hell; ns[1] = art_tex_hell_n;
    for (s = 0; s < 2; s++)
        for (k = 0; k < ns[s]; k++) {
            const ArtTexDef *d = &sets[s][k];
            AnimOut *o;
            if (d->id <= 0 || d->id >= TEX_COUNT) { fail("bad texture id", d->name); continue; }
            o = &tex_out[d->id];
            if (o->defined) { fail("texture defined twice", d->name); continue; }
            if (d->frames < 1 || d->frames > 4) { fail("bad frame count", d->name); continue; }
            if (tex_nframes + d->frames > DOOMO_TEX_MAX_FRAMES) { fail("too many texture frames", d->name); continue; }
            o->defined = 1; o->first = tex_nframes; o->frames = d->frames;
            o->speed = d->speed < 1 ? 1 : d->speed; o->mode = d->mode; o->name = d->name;
            for (f = 0; f < d->frames; f++) {
                ArtImg im;
                int i;
                img_init(&im, tex_pix[tex_nframes], 64, 64, 1, 1);
                d->fn(&im, f, d->frames);
                for (i = 0; i < TEXPX; i++)
                    if (!im.p[i]) { im.p[i] = PAL_BLACK; }
                check_seams(im.p, 64, 64, 1, d->name);
                if (f == 0) {
                    o->avg = img_avg(&im);
                    o->dark = darkest(im.p, TEXPX);
                }
                tex_nframes++;
            }
        }
    for (k = 1; k < TEX_COUNT; k++)
        if (!tex_out[k].defined && !partial) {
            char buf[32];
            sprintf(buf, "texture id %d", k);
            fail("texture missing", buf);
        }
}

static void gen_skies(void)
{
    int k, f;
    for (k = 0; k < art_skies_n; k++) {
        const ArtSkyDef *d = &art_skies[k];
        AnimOut *o;
        if (d->id < 0 || d->id >= SKY_COUNT) { fail("bad sky id", d->name); continue; }
        o = &sky_out[d->id];
        if (o->defined) { fail("sky defined twice", d->name); continue; }
        if (sky_nframes + d->frames > DOOMO_SKY_MAX_FRAMES) { fail("too many sky frames", d->name); continue; }
        o->defined = 1; o->first = sky_nframes; o->frames = d->frames;
        o->speed = d->speed < 1 ? 1 : d->speed; o->mode = d->mode; o->name = d->name;
        for (f = 0; f < d->frames; f++) {
            ArtImg im;
            int i;
            img_init(&im, sky_pix[sky_nframes], SKY_W, SKY_H, 1, 0);
            d->fn(&im, f, d->frames);
            for (i = 0; i < SKY_W * SKY_H; i++)
                if (!im.p[i]) im.p[i] = PAL_BLACK;
            check_seams(im.p, SKY_W, SKY_H, 0, d->name);
            if (f == 0) {
                ArtImg band;
                o->avg = img_avg(&im);
                /* horizon: average of the bottom 12 rows */
                band = im;
                band.p = im.p + (SKY_H - 12) * SKY_W;
                band.h = 12;
                o->dark = img_avg(&band);
            }
            sky_nframes++;
        }
    }
    for (k = 0; k < SKY_COUNT; k++)
        if (!sky_out[k].defined && !partial) fail("sky missing", "?");
}

static void gen_sprites(void)
{
    const ArtSprDef *sets[3];
    int ns[3], s, k, f;
    sets[0] = art_spr_demons; ns[0] = art_spr_demons_n;
    sets[1] = art_spr_fx; ns[1] = art_spr_fx_n;
    sets[2] = art_spr_items; ns[2] = art_spr_items_n;
    for (s = 0; s < 3; s++)
        for (k = 0; k < ns[s]; k++) {
            const ArtSprDef *d = &sets[s][k];
            SprOut *o;
            if (d->id <= 0 || d->id >= SPR_COUNT) { fail("bad sprite id", d->name); continue; }
            o = &spr_out[d->id];
            if (o->defined) { fail("sprite defined twice", d->name); continue; }
            if (d->frames < 1 || d->frames > SPR_MAX_FRAMES) { fail("bad sprite frames", d->name); continue; }
            if (nframes_total + d->frames > DOOMO_SPR_MAX_IMAGES) { fail("too many sprite frames", d->name); continue; }
            o->defined = 1; o->first = nframes_total; o->frames = d->frames;
            o->flags = d->flags; o->name = d->name;
            if (d->anchor == ANCHOR_CENTER) o->flags |= SPRF_CENTERED;
            for (f = 0; f < d->frames; f++) {
                spr_begin();
                d->fn(f);
                spr_finish(&frames[nframes_total], d->anchor);
                if (frames[nframes_total].w > 255 || frames[nframes_total].h > 255)
                    fail("sprite frame too big", d->name);
                nframes_total++;
            }
        }
    for (k = 1; k < SPR_COUNT; k++)
        if (!spr_out[k].defined && !partial) {
            char buf[32];
            sprintf(buf, "sprite id %d", k);
            fail("sprite missing", buf);
        }
}

/* ---- contract validation (always on; make gen / check-gen fail on it) ----- */
static void validate(void)
{
    int i, lv, k, f;
    char buf[64];
    /* colormap: identity at 15, black at 0, index 0 fixed, never brighter
     * at a lower level; tint 0 identity, index 0 fixed */
    for (i = 0; i < 256; i++) {
        if (art_colormap[15][i] != i) fail("colormap level 15 not identity", "palette");
        if (art_colormap[0][i] != (i ? PAL_BLACK : 0)) fail("colormap level 0 not black", "palette");
        for (lv = 0; lv < DOOMO_LIGHT_LEVELS; lv++) {
            if ((art_colormap[lv][i] == 0) != (i == 0)) fail("colormap touches index 0", "palette");
            if (lv && art_luma(art_colormap[lv - 1][i]) > art_luma(art_colormap[lv][i]))
                fail("colormap not monotonic", "palette");
        }
        for (k = 0; k < TINT_COUNT; k++)
            if ((art_tintmap[k][i] == 0) != (i == 0)) fail("tintmap touches index 0", "palette");
        if (art_tintmap[TINT_NONE][i] != i) fail("TINT_NONE not identity", "palette");
    }
    /* sprites: centred (even width), non-empty, tight; resting frames sit
     * on their bottom row */
    for (k = 1; k < SPR_COUNT; k++) {
        if (!spr_out[k].defined) continue;
        if (k <= SPR_LORD_VAPULA && spr_out[k].frames != DFRAME_COUNT) fail("demon must have 8 frames", spr_out[k].name);
        for (f = 0; f < spr_out[k].frames; f++) {
            const ArtFrame *a = &frames[spr_out[k].first + f];
            int x, y, any = 0, bottom = 0;
            if (a->w & 1) fail("sprite frame has odd width (not centred)", spr_out[k].name);
            for (y = 0; y < a->h; y++)
                for (x = 0; x < a->w; x++)
                    if (a->pix[y * a->w + x]) { any = 1; if (y == a->h - 1) bottom = 1; }
            sprintf(buf, "%s frame %d", spr_out[k].name, f);
            if (!any) fail("empty sprite frame", buf);
            if (!(spr_out[k].flags & (SPRF_CENTERED | SPRF_FLOATS)) && f == 0 && !bottom)
                fail("ground sprite does not touch the ground", buf);
            if (k <= SPR_LORD_VAPULA && f == DFRAME_DEATH3 && !bottom) fail("demon corpse floats", buf);
        }
    }
}

/* ---- C writer ------------------------------------------------------------- */
static void put_bytes(FILE *f, const uint8_t *p, int n)
{
    int i, col = 0;
    for (i = 0; i < n; i++) {
        char buf[8];
        int l = sprintf(buf, "%d,", p[i]);
        if (col + l > 100) { fputc('\n', f); col = 0; }
        fputs(buf, f);
        col += l;
    }
    fputc('\n', f);
}

static void write_c(const char *path)
{
    FILE *f = fopen(path, "wb");
    int k, off;
    long pixbytes = 0;
    if (!f) { perror(path); exit(1); }
    fprintf(f,
        "/* art_data.c - GENERATED by core/tools/artgen (owner: art). DO NOT EDIT.\n"
        " * Regenerate with: make -C core gen\n"
        " * Generators: core/art/ *.c. All tables are const so they stay in ROM. */\n"
        "#include \"doomo/palette.h\"\n#include \"doomo/textures.h\"\n#include \"doomo/sprites.h\"\n\n");
    /* palette tables */
    fprintf(f, "const uint8_t doomo_art_colormap[DOOMO_LIGHT_LEVELS][256] = {\n");
    for (k = 0; k < DOOMO_LIGHT_LEVELS; k++) { fprintf(f, "{\n"); put_bytes(f, art_colormap[k], 256); fprintf(f, "},\n"); }
    fprintf(f, "};\n\nconst uint8_t doomo_art_tintmap[TINT_COUNT][256] = {\n");
    for (k = 0; k < TINT_COUNT; k++) { fprintf(f, "{\n"); put_bytes(f, art_tintmap[k], 256); fprintf(f, "},\n"); }
    fprintf(f, "};\n\nconst uint8_t doomo_art_darker[256] = {\n");
    put_bytes(f, art_darker_tab, 256);
    fprintf(f, "};\n\nconst uint8_t doomo_art_lighter[256] = {\n");
    put_bytes(f, art_lighter_tab, 256);
    fprintf(f, "};\n\n");

    /* textures */
    fprintf(f, "DOOMO_STATIC_ASSERT(%d <= DOOMO_TEX_MAX_FRAMES, art_tex_frames_fit);\n", tex_nframes);
    fprintf(f, "const uint16_t doomo_art_tex_frame_total = %d;\n\n", tex_nframes);
    fprintf(f, "const uint8_t doomo_art_tex_pix[%d][DOOMO_TEX_SIZE_PX] = {\n", tex_nframes);
    for (k = 1; k < TEX_COUNT; k++) {
        int fr;
        for (fr = 0; fr < tex_out[k].frames; fr++) {
            fprintf(f, "/* %d: %s frame %d */ {\n", tex_out[k].first + fr, tex_out[k].name, fr);
            put_bytes(f, tex_pix[tex_out[k].first + fr], TEXPX);
            fprintf(f, "},\n");
            pixbytes += TEXPX;
        }
    }
    fprintf(f, "};\n\nconst DoomoArtAnim doomo_art_tex[TEX_COUNT] = {\n    {0, 0, 1, 0, %d, %d, 0},\n",
            PAL_BLACK, PAL_BLACK);
    for (k = 1; k < TEX_COUNT; k++)
        fprintf(f, "    {%d, %d, %d, %d, %d, %d, 0}, /* %s */\n", tex_out[k].first, tex_out[k].frames,
                tex_out[k].speed, tex_out[k].mode, tex_out[k].avg, tex_out[k].dark, tex_out[k].name);
    fprintf(f, "};\n\nconst char *const doomo_art_tex_names[TEX_COUNT] = {\n    \"none\",\n");
    for (k = 1; k < TEX_COUNT; k++) fprintf(f, "    \"%s\",\n", tex_out[k].name);
    fprintf(f, "};\n\n");
    /* (integration, memory budget) every frame as a const DoomoImage with a
     * fixed uid, so texture_image() needs no RAM copies (textures.h) */
    fprintf(f, "const DoomoImage doomo_art_tex_img[%d] = {\n", tex_nframes);
    for (k = 0; k < tex_nframes; k++)
        fprintf(f, "    {64, 64, doomo_art_tex_pix[%d], DOOMO_ART_TEX_UID + %d},\n", k, k);
    fprintf(f, "};\n\n");

    /* skies */
    fprintf(f, "DOOMO_STATIC_ASSERT(%d <= DOOMO_SKY_MAX_FRAMES, art_sky_frames_fit);\n", sky_nframes);
    fprintf(f, "const uint8_t doomo_art_sky_pix[%d][SKY_W * SKY_H] = {\n", sky_nframes);
    for (k = 0; k < SKY_COUNT; k++) {
        int fr;
        for (fr = 0; fr < sky_out[k].frames; fr++) {
            fprintf(f, "/* %s frame %d */ {\n", sky_out[k].name, fr);
            put_bytes(f, sky_pix[sky_out[k].first + fr], SKY_W * SKY_H);
            fprintf(f, "},\n");
            pixbytes += SKY_W * SKY_H;
        }
    }
    fprintf(f, "};\n\nconst DoomoArtAnim doomo_art_sky[SKY_COUNT] = {\n");
    for (k = 0; k < SKY_COUNT; k++)
        fprintf(f, "    {%d, %d, %d, %d, %d, %d, 0}, /* %s */\n", sky_out[k].first, sky_out[k].frames,
                sky_out[k].speed, sky_out[k].mode, sky_out[k].avg, sky_out[k].dark, sky_out[k].name);
    fprintf(f, "};\n\n");

    /* sprites */
    fprintf(f, "static const uint8_t spr_pix[] = {\n");
    for (k = 1; k < SPR_COUNT; k++) {
        int fr;
        for (fr = 0; fr < spr_out[k].frames; fr++) {
            const ArtFrame *a = &frames[spr_out[k].first + fr];
            fprintf(f, "/* %s frame %d (%dx%d) */\n", spr_out[k].name, fr, a->w, a->h);
            put_bytes(f, a->pix, a->w * a->h);
            pixbytes += a->w * a->h;
        }
    }
    fprintf(f, "};\n\n");
    fprintf(f, "DOOMO_STATIC_ASSERT(%d <= DOOMO_SPR_MAX_IMAGES, art_spr_frames_fit);\n", nframes_total);
    fprintf(f, "const uint16_t doomo_art_frame_total = %d;\n", nframes_total);
    fprintf(f, "const DoomoArtFrame doomo_art_frames[%d] = {\n", nframes_total);
    off = 0;
    for (k = 1; k < SPR_COUNT; k++) {
        int fr;
        for (fr = 0; fr < spr_out[k].frames; fr++) {
            const ArtFrame *a = &frames[spr_out[k].first + fr];
            fprintf(f, "    {%d, %d, spr_pix + %d, DOOMO_ART_SPR_UID + %d},\n", a->w, a->h, off,
                    spr_out[k].first + fr);
            off += a->w * a->h;
        }
    }
    fprintf(f, "};\n\nconst DoomoArtSprite doomo_art_spr[SPR_COUNT] = {\n    {0, 0, 0},\n");
    for (k = 1; k < SPR_COUNT; k++)
        fprintf(f, "    {%d, %d, %d}, /* %s */\n", spr_out[k].first, spr_out[k].frames, spr_out[k].flags,
                spr_out[k].name);
    fprintf(f, "};\n\nconst char *const doomo_art_spr_names[SPR_COUNT] = {\n    \"none\",\n");
    for (k = 1; k < SPR_COUNT; k++) fprintf(f, "    \"%s\",\n", spr_out[k].name);
    fprintf(f, "};\n");
    fprintf(f, "/* pixel data: %ld bytes */\n", pixbytes);
    fclose(f);
    if (!quiet)
        printf("artgen: wrote %s (%d texture frames, %d sky frames, %d sprite frames, %ld bytes of pixels)\n",
               path, tex_nframes, sky_nframes, nframes_total, pixbytes);
}

/* ---- PNG writer (stored deflate, no zlib needed) ---------------------------- */
static uint32_t crc_tab[256];
static void crc_init(void)
{
    uint32_t c, n, k;
    for (n = 0; n < 256; n++) {
        c = n;
        for (k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_tab[n] = c;
    }
}
static uint32_t crc_upd(uint32_t c, const uint8_t *p, size_t n)
{
    while (n--) c = crc_tab[(c ^ *p++) & 255] ^ (c >> 8);
    return c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t h[8];
    uint32_t c;
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    c = crc_upd(0xFFFFFFFFu, (const uint8_t *)type, 4);
    c = crc_upd(c, data, n) ^ 0xFFFFFFFFu;
    be32(h, c);
    fwrite(h, 1, 4, f);
}

static void write_png(const char *path, const uint8_t *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    size_t raw_n = (size_t)(w * 3 + 1) * h, i, nblocks, zn, pos;
    uint8_t *raw, *z, hdr[13];
    uint32_t a = 1, b = 0;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    int y;
    if (!f) { perror(path); return; }
    raw = (uint8_t *)malloc(raw_n);
    for (y = 0; y < h; y++) {
        raw[(size_t)y * (w * 3 + 1)] = 0;
        memcpy(raw + (size_t)y * (w * 3 + 1) + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    nblocks = (raw_n + 65534) / 65535;
    zn = 2 + nblocks * 5 + raw_n + 4;
    z = (uint8_t *)malloc(zn);
    z[0] = 0x78; z[1] = 0x01;
    pos = 2;
    for (i = 0; i < nblocks; i++) {
        size_t n = raw_n - i * 65535;
        if (n > 65535) n = 65535;
        z[pos++] = (uint8_t)(i == nblocks - 1);
        z[pos++] = (uint8_t)(n & 255); z[pos++] = (uint8_t)(n >> 8);
        z[pos++] = (uint8_t)(~n & 255); z[pos++] = (uint8_t)((~n >> 8) & 255);
        memcpy(z + pos, raw + i * 65535, n);
        pos += n;
    }
    for (i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    be32(z + pos, (b << 16) | a);
    pos += 4;
    fwrite(sig, 1, 8, f);
    be32(hdr, (uint32_t)w); be32(hdr + 4, (uint32_t)h);
    hdr[8] = 8; hdr[9] = 2; hdr[10] = 0; hdr[11] = 0; hdr[12] = 0;
    chunk(f, "IHDR", hdr, 13);
    chunk(f, "IDAT", z, (uint32_t)pos);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
}

/* ---- sheets ----------------------------------------------------------------- */
typedef struct { int w, h; uint8_t *p; } Sheet;   /* indexed, converted at save */

static void sheet_new(Sheet *s, int w, int h, uint8_t bg)
{
    s->w = w; s->h = h;
    s->p = (uint8_t *)malloc((size_t)w * h);
    memset(s->p, bg, (size_t)w * h);
}

static void sheet_put(Sheet *s, int x, int y, uint8_t c)
{
    if (x >= 0 && y >= 0 && x < s->w && y < s->h) s->p[y * s->w + x] = c;
}

static void sheet_blit(Sheet *s, int x, int y, const uint8_t *p, int w, int h, int zoom,
                       const uint8_t *map, int transparent_bg)
{
    int i, j, a, b;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint8_t c = p[j * w + i];
            if (!c && transparent_bg) continue;
            if (map) c = map[c];
            for (b = 0; b < zoom; b++)
                for (a = 0; a < zoom; a++) sheet_put(s, x + i * zoom + a, y + j * zoom + b, c);
        }
}

static void sheet_text(Sheet *s, int x, int y, const char *t, uint8_t c)
{
    ArtImg im;
    if (!t) t = "MISSING";
    im.w = s->w; im.h = s->h; im.wrapx = 0; im.wrapy = 0; im.p = s->p;
    img_text(&im, x, y, t, 1, c);
}

static void sheet_save(const Sheet *s, const char *dir, const char *name)
{
    char path[1024];
    uint8_t *rgb = (uint8_t *)malloc((size_t)s->w * s->h * 3);
    int i;
    for (i = 0; i < s->w * s->h; i++) {
        uint8_t r, g, b;
        palette_rgb(s->p[i], &r, &g, &b);
        rgb[i * 3] = r; rgb[i * 3 + 1] = g; rgb[i * 3 + 2] = b;
    }
    snprintf(path, sizeof path, "%s/%s", dir, name);
    write_png(path, rgb, s->w, s->h);
    free(rgb);
    if (!quiet) printf("artgen: sheet %s\n", path);
}

static void sheet_free(Sheet *s) { free(s->p); s->p = NULL; }

static void sheets(const char *dir, int zoom)
{
    Sheet s;
    int k, i, x, y, cols;
    char buf[64];
    uint8_t bg = PAL_RAMP(RAMP_STONE, 1), fg = PAL_WHITE;

    /* palette */
    sheet_new(&s, 16 * 20 + 8, 16 * 20 + 8, bg);
    for (i = 0; i < 256; i++)
        for (y = 0; y < 18; y++)
            for (x = 0; x < 18; x++) sheet_put(&s, 4 + (i % 16) * 20 + x, 4 + (i / 16) * 20 + y, (uint8_t)i);
    sheet_save(&s, dir, "palette.png");
    sheet_free(&s);

    /* colormap: rows = palette index, cols = light level */
    sheet_new(&s, 16 * 6 + 8 + 16 * 6 + 8, 128 * 4 + 8, bg);
    for (i = 1; i < 256; i++) {
        int col = i < 128 ? 0 : 1, row = i < 128 ? i : i - 128;
        for (k = 0; k < 16; k++)
            for (y = 0; y < 4; y++)
                for (x = 0; x < 6; x++)
                    sheet_put(&s, 4 + col * (16 * 6 + 8) + k * 6 + x, 4 + row * 4 + y, art_colormap[k][i]);
    }
    sheet_save(&s, dir, "colormap.png");
    sheet_free(&s);

    /* tints */
    sheet_new(&s, TINT_COUNT * 16 * 4 + 8, 16 * 4 + 20, bg);
    for (k = 0; k < TINT_COUNT; k++) {
        for (i = 0; i < 256; i++)
            for (y = 0; y < 4; y++)
                for (x = 0; x < 4; x++)
                    sheet_put(&s, 4 + k * 64 + (i % 16) * 4 + x, 14 + (i / 16) * 4 + y, art_tintmap[k][i]);
        sheet_text(&s, 4 + k * 64, 3, doomo_tint_names[k], fg);
    }
    sheet_save(&s, dir, "tints.png");
    sheet_free(&s);

    /* textures: every frame, labelled */
    cols = 8;
    {
        int cw = 64 * zoom + 8, ch = 64 * zoom + 14, n = tex_nframes;
        int rows = (n + cols - 1) / cols, t = 0;
        sheet_new(&s, cols * cw + 8, rows * ch + 8, bg);
        for (k = 1; k < TEX_COUNT; k++) {
            int fr;
            for (fr = 0; fr < tex_out[k].frames; fr++, t++) {
                int cx = 4 + (t % cols) * cw, cy = 4 + (t / cols) * ch;
                sheet_blit(&s, cx, cy + 10, tex_pix[tex_out[k].first + fr], 64, 64, zoom, NULL, 0);
                if (tex_out[k].frames > 1) sprintf(buf, "%s %d", tex_out[k].name, fr);
                else sprintf(buf, "%s", tex_out[k].name ? tex_out[k].name : "MISSING");
                sheet_text(&s, cx, cy + 1, buf, fg);
            }
        }
        sheet_save(&s, dir, "textures.png");
        sheet_free(&s);
    }
    /* each texture tiled 3x3 at 1x plus light levels 15/11/7/3 */
    {
        int cw = 64 * 3 + 64 * 2 + 16, ch = 64 * 3 + 16, n = TEX_COUNT - 1;
        int half = (n + 1) / 2, part;
        for (part = 0; part < 2; part++) {
            int t0 = 1 + part * half, t1 = part ? TEX_COUNT : 1 + half, row = 0;
            cols = 3;
            sheet_new(&s, cols * cw + 8, ((t1 - t0 + cols - 1) / cols) * ch + 8, bg);
            for (k = t0; k < t1; k++, row++) {
                int cx = 4 + (row % cols) * cw, cy = 4 + (row / cols) * ch, a, b, l;
                const uint8_t *p = tex_pix[tex_out[k].first];
                for (b = 0; b < 3; b++)
                    for (a = 0; a < 3; a++) sheet_blit(&s, cx + a * 64, cy + 10 + b * 64, p, 64, 64, 1, NULL, 0);
                for (l = 0; l < 4; l++) {
                    static const int lv[4] = {15, 11, 7, 3};
                    sheet_blit(&s, cx + 64 * 3 + 4 + (l & 1) * 64, cy + 10 + (l >> 1) * 64, p, 64, 64, 1,
                               art_colormap[lv[l]], 0);
                }
                sheet_text(&s, cx, cy + 1, tex_out[k].name, fg);
            }
            sheet_save(&s, dir, part ? "tiles_b.png" : "tiles_a.png");
            sheet_free(&s);
        }
    }
    /* skies (with a 1.5 wrap to show the seam) */
    {
        int sh = SKY_H * zoom + 14;
        sheet_new(&s, (SKY_W + SKY_W / 2) * zoom + 8, sky_nframes * sh + 8, bg);
        for (k = 0, i = 0; k < SKY_COUNT; k++) {
            int fr;
            for (fr = 0; fr < sky_out[k].frames; fr++, i++) {
                const uint8_t *p = sky_pix[sky_out[k].first + fr];
                sheet_blit(&s, 4, 4 + i * sh + 10, p, SKY_W, SKY_H, zoom, NULL, 0);
                for (y = 0; y < SKY_H; y++)
                    for (x = 0; x < SKY_W / 2; x++) {
                        int a, b;
                        for (b = 0; b < zoom; b++)
                            for (a = 0; a < zoom; a++)
                                sheet_put(&s, 4 + (SKY_W + x) * zoom + a, 4 + i * sh + 10 + y * zoom + b,
                                          p[y * SKY_W + x]);
                    }
                sprintf(buf, "%s %d", sky_out[k].name ? sky_out[k].name : "MISSING", fr);
                sheet_text(&s, 4, 4 + i * sh + 1, buf, fg);
            }
        }
        sheet_save(&s, dir, "skies.png");
        sheet_free(&s);
    }
    /* sprites: one row per sprite, frames side by side, on two backdrops */
    {
        int pass;
        for (pass = 0; pass < 3; pass++) {
            int k0 = pass == 0 ? 1 : (pass == 1 ? SPR_FIREBALL : SPR_HEALTH_SMALL);
            int k1 = pass == 0 ? SPR_FIREBALL : (pass == 1 ? SPR_HEALTH_SMALL : SPR_COUNT);
            int z = zoom, W = 8, H = 8, rowy;
            for (k = k0; k < k1; k++) {
                int fr, rw = 100, rh = 0;
                for (fr = 0; fr < spr_out[k].frames; fr++) {
                    const ArtFrame *a = &frames[spr_out[k].first + fr];
                    rw += a->w * z + 6;
                    if (a->h * z > rh) rh = a->h * z;
                }
                if (rw > W) W = rw;
                H += rh + 14;
            }
            W += 8;
            sheet_new(&s, W, H, PAL_RAMP(RAMP_SKY, 6));
            rowy = 4;
            for (k = k0; k < k1; k++) {
                int fr, cx = 100, rh = 0;
                for (fr = 0; fr < spr_out[k].frames; fr++) {
                    const ArtFrame *a = &frames[spr_out[k].first + fr];
                    if (a->h * z > rh) rh = a->h * z;
                }
                /* alternating backdrop rows: pastel and hell */
                for (y = rowy - 2; y < rowy + rh + 12; y++)
                    for (x = 0; x < W; x++)
                        sheet_put(&s, x, y, (k & 1) ? PAL_RAMP(RAMP_MINT, 6) : PAL_RAMP(RAMP_STONE, 2));
                sheet_text(&s, 4, rowy + 2, spr_out[k].name, (k & 1) ? PAL_RAMP(RAMP_STONE, 1) : fg);
                for (fr = 0; fr < spr_out[k].frames; fr++) {
                    const ArtFrame *a = &frames[spr_out[k].first + fr];
                    sheet_blit(&s, cx, rowy + 4 + rh - a->h * z, a->pix, a->w, a->h, z, NULL, 1);
                    cx += a->w * z + 6;
                }
                rowy += rh + 14;
            }
            sheet_save(&s, dir, pass == 0 ? "sprites_demons.png" : (pass == 1 ? "sprites_fx.png" : "sprites_items.png"));
            sheet_free(&s);
        }
    }
}

static void stats(void)
{
    int k;
    long sb = 0;
    for (k = 0; k < nframes_total; k++) sb += frames[k].w * frames[k].h;
    printf("artgen: textures %d frames = %d bytes; skies %d frames = %d bytes; sprites %d frames = %ld bytes\n",
           tex_nframes, tex_nframes * TEXPX, sky_nframes, sky_nframes * SKY_W * SKY_H, nframes_total, sb);
    for (k = 1; k < SPR_COUNT; k++) {
        int fr;
        long b = 0;
        if (!spr_out[k].defined) continue;
        for (fr = 0; fr < spr_out[k].frames; fr++)
            b += frames[spr_out[k].first + fr].w * frames[spr_out[k].first + fr].h;
        printf("  %-16s %d frames %6ld bytes (frame0 %dx%d)\n", spr_out[k].name, spr_out[k].frames, b,
               frames[spr_out[k].first].w, frames[spr_out[k].first].h);
    }
}

int main(int argc, char **argv)
{
    const char *out = NULL, *sheet = NULL;
    char outbuf[1024];
    int i, zoom = 2, check = 0;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out-root") && i + 1 < argc) {
            snprintf(outbuf, sizeof outbuf, "%s/core/gen/art_data.c", argv[++i]);
            out = outbuf;
        } else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--sheet") && i + 1 < argc) sheet = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--stats")) verbose = 1;
        else if (!strcmp(argv[i], "--partial")) partial = 1;
        else {
            fprintf(stderr, "usage: artgen [--out-root DIR | --out FILE] [--sheet DIR [--zoom N]] [--check] [--stats]\n");
            return 2;
        }
    }
    if (zoom < 1) zoom = 1;
    if (zoom > 6) zoom = 6;
    if (!out && !sheet && !check) {
        fprintf(stderr, "artgen: nothing to do (use --out-root DIR, --out FILE, --sheet DIR or --check)\n");
        return 2;
    }
    crc_init();
    art_palette_setup();
    gen_textures();
    gen_skies();
    gen_sprites();
    validate();
    if (verbose) stats();
    if (errors) {
        fprintf(stderr, "artgen: %d error(s)\n", errors);
        return 1;
    }
    if (out) write_c(out);
    if (sheet) sheets(sheet, zoom);
    return 0;
}
