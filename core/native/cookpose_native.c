/* cookpose_native.c - (S2-4c) Granny's baked poses on the native platform (fishcook_int.h
 * plat_cook_pose; owner: fishcook; docs/GBA.md "Speed wave 2: S2-4c").
 *
 * A pose is granny_draw() painted on a screen cleared to index 0 and cropped to what it painted:
 * an image (index 0 a hole) drawn at the same place stands for her exactly, as she paints only
 * opaque pixels over her own body (the blush's checker lies on her fur). A pose that paints index
 * 0 cannot be an image: it is left out (the kitchen draws her). The baker (runner --bake-poses)
 * writes every pose cook_poses() lists at the runner's screen size for the GBA ROM; with
 * nat_stage_on (tests) plat_cook_pose renders a pose the same way on first use and draws it, so
 * test_cook_kept compares the very images the GBA uses with Granny drawn in full. Off by default:
 * screenshots never depend on it.
 *
 * File: "GRN1", u32 n, n x { u32 key, u32 offset, i16 cx, i16 cy, i16 clip, i16 x0, i16 y0, u16 w,
 * u16 h, u16 pad } (24 B) sorted by key, then the pictures (w * h bytes each, 4-aligned; offset 0:
 * none). Little endian. */
#include "native.h"
#include "../src/fishcook_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t key;
    int16_t cx, cy, clip, x0, y0;
    uint16_t w, h;
    uint8_t *pix;
} NPose;

/* granny_draw(cx, cy, s, t, mood) above row clip as an image; 0 when it paints index 0 or nothing */
static int pose_render(int cx, int cy, int clip, int s, int t, int mood, NPose *p)
{
    static uint8_t save[NAT_MAX_W * NAT_MAX_H], a[NAT_MAX_W * NAT_MAX_H];
    size_t n = (size_t)nat_w * (size_t)nat_h;
    int i, x, y, x0 = nat_w, y0 = nat_h, x1 = -1, y1 = -1, bad = 0;
    memcpy(save, nat_fb, n);
    memset(nat_fb, 0, n);
    granny_draw(cx, cy, s, t, mood);
    memcpy(a, nat_fb, n);
    memset(nat_fb, 1, n);
    granny_draw(cx, cy, s, t, mood);
    for (i = 0; i < (int)n; i++) {
        if (a[i] != nat_fb[i] || i / nat_w >= clip) { a[i] = 0; continue; }   /* not painted, or covered */
        if (!a[i]) bad = 1;                                      /* painted with index 0 */
        x = i % nat_w; y = i / nat_w;
        x0 = DOOMO_MIN(x0, x); x1 = DOOMO_MAX(x1, x);
        y0 = DOOMO_MIN(y0, y); y1 = DOOMO_MAX(y1, y);
    }
    memcpy(nat_fb, save, n);
    if (bad || x1 < x0) return 0;
    p->key = granny_key(s, t, mood);
    p->cx = (int16_t)cx; p->cy = (int16_t)cy; p->clip = (int16_t)clip;
    p->x0 = (int16_t)x0; p->y0 = (int16_t)y0;
    p->w = (uint16_t)(x1 - x0 + 1); p->h = (uint16_t)(y1 - y0 + 1);
    p->pix = (uint8_t *)malloc((size_t)p->w * p->h);
    if (!p->pix) return 0;
    for (y = 0; y < p->h; y++) memcpy(p->pix + (size_t)y * p->w, a + (y0 + y) * nat_w + x0, p->w);
    return 1;
}

/* ---- the tests' poses: rendered on first use ---------------------------------------------- */
#define NP_CAP 2048
static NPose np[NP_CAP];
static uint8_t np_ok[NP_CAP];             /* 1 an image, 2 cannot be one */
static int np_n, np_w, np_h;

int plat_cook_pose(uint32_t key, int cx, int cy, int clip, int s, int t, int mood, int box[4])
{
    int i;
    DoomoImage img;
    if (!nat_stage_on) return 0;
    if (np_w != nat_w || np_h != nat_h) {
        for (i = 0; i < np_n; i++) free(np[i].pix);
        np_n = 0;
        np_w = nat_w; np_h = nat_h;
    }
    for (i = 0; i < np_n; i++)
        if (np[i].key == key && np[i].cx == cx && np[i].cy == cy && np[i].clip == clip) break;
    if (i == np_n) {
        if (np_n >= NP_CAP) return 0;
        memset(&np[i], 0, sizeof(np[i]));
        np_ok[i] = pose_render(cx, cy, clip, s, t, mood, &np[i]) ? 1 : 2;
        np[i].key = key; np[i].cx = (int16_t)cx; np[i].cy = (int16_t)cy; np[i].clip = (int16_t)clip;
        np_n++;
    }
    if (np_ok[i] != 1) return 0;
    if (box) {
        box[0] = np[i].x0; box[1] = np[i].y0; box[2] = np[i].w; box[3] = np[i].h;
        return 1;
    }
    img.w = np[i].w; img.h = np[i].h; img.pix = np[i].pix; img.uid = 0xC00C0000u + (uint32_t)i;
    plat_image(np[i].x0, np[i].y0, &img, 1, 0, 0);
    return 1;
}

/* (tests) every pose asked for since the screen size last changed that cook_poses() does not
 * list (the GBA ROM would not hold it: drawn in full there); and how many were asked for */
static int miss_n;
static void miss_one(void *ctx, int cx, int cy, int clip, int s, int t, int mood)
{
    int i;
    uint32_t key = granny_key(s, t, mood);
    (void)ctx;
    for (i = 0; i < np_n; i++)
        if (np[i].key == key && np[i].cx == cx && np[i].cy == cy && np[i].clip == clip && np_ok[i] == 1) np_ok[i] = 3;
}
int nat_cook_pose_unlisted(int *asked)
{
    int i;
    cook_poses(miss_one, NULL);
    miss_n = 0;
    for (i = 0; i < np_n; i++) {
        if (np_ok[i] == 1) miss_n++;
        if (np_ok[i] == 3) np_ok[i] = 1;
    }
    *asked = np_n;
    return miss_n;
}

/* ---- the baker --------------------------------------------------------------------------- */
static NPose *bk;
static int bk_n, bk_cap, bk_skipped;

static void bake_one(void *ctx, int cx, int cy, int clip, int s, int t, int mood)
{
    uint32_t key = granny_key(s, t, mood);
    int i;
    (void)ctx;
    for (i = 0; i < bk_n; i++)
        if (bk[i].key == key && bk[i].cx == cx && bk[i].cy == cy && bk[i].clip == clip) return;
    if (bk_n >= bk_cap) {
        int cap = bk_cap ? bk_cap * 2 : 512;
        NPose *m = (NPose *)realloc(bk, sizeof(*bk) * (size_t)cap);
        if (!m) return;
        bk = m;
        bk_cap = cap;
    }
    memset(&bk[bk_n], 0, sizeof(bk[0]));
    if (!pose_render(cx, cy, clip, s, t, mood, &bk[bk_n])) {
        /* (kept as an entry with no picture: the lookup then says "cannot") */
        bk[bk_n].key = key; bk[bk_n].cx = (int16_t)cx; bk[bk_n].cy = (int16_t)cy; bk[bk_n].clip = (int16_t)clip;
        bk_skipped++;
    }
    bk_n++;
}

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255), f); fputc((int)((v >> 8) & 255), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFFu); put16(f, v >> 16); }

static int cmp_pose(const void *a, const void *b)
{
    const NPose *x = (const NPose *)a, *y = (const NPose *)b;
    if (x->key != y->key) return x->key < y->key ? -1 : 1;
    if (x->cx != y->cx) return x->cx - y->cx;
    if (x->cy != y->cy) return x->cy - y->cy;
    return x->clip - y->clip;
}

int nat_bake_poses(const char *path)
{
    FILE *f;
    uint32_t off;
    int i, k, n;
    bk_n = bk_skipped = 0;
    cook_poses(bake_one, NULL);
    n = bk_n;
    qsort(bk, (size_t)n, sizeof(*bk), cmp_pose);
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "bake: cannot write %s\n", path); return 0; }
    fwrite("GRN1", 1, 4, f);
    put32(f, (uint32_t)n);
    off = 8u + 24u * (uint32_t)n;
    for (i = 0; i < n; i++) {
        uint32_t sz = (uint32_t)bk[i].w * bk[i].h;
        put32(f, bk[i].key);
        put32(f, bk[i].pix ? off : 0u);
        put16(f, (uint16_t)bk[i].cx); put16(f, (uint16_t)bk[i].cy); put16(f, (uint16_t)bk[i].clip);
        put16(f, (uint16_t)bk[i].x0); put16(f, (uint16_t)bk[i].y0);
        put16(f, bk[i].pix ? bk[i].w : 0u); put16(f, bk[i].pix ? bk[i].h : 0u); put16(f, 0u);
        if (bk[i].pix) off += (sz + 3u) & ~3u;
    }
    for (i = 0; i < n; i++) {
        uint32_t sz = (uint32_t)bk[i].w * bk[i].h;
        if (!bk[i].pix) continue;
        fwrite(bk[i].pix, 1, sz, f);
        for (k = 0; k < (int)(((sz + 3u) & ~3u) - sz); k++) fputc(0, f);
        free(bk[i].pix);
        bk[i].pix = NULL;
    }
    if (fclose(f) != 0) return 0;
    if (bk_skipped) fprintf(stderr, "bake: %d of %d poses paint index 0 (drawn in full)\n", bk_skipped, n);
    return n;
}
