/* texcache.c - the 2D layer's PAL8 texture cache in PVR video RAM (DC3;
 * owner: dreamcast). Portable: video RAM is reached through the backend
 * (dc2d_be_tex_alloc / _free / _write).
 *
 * Every DoomoImage the core draws becomes one twiddled PAL8 texture of
 * power-of-two size (8..1024 per side), uploaded the first time it is
 * drawn and kept while there is room:
 *   key   the image's uid when it has one (the core gives an image a new
 *         uid whenever its pixels change, docs/ART.md); otherwise the pixel
 *         pointer PLUS a checksum of the pixels, because an image without
 *         a uid may be rebuilt in place (a scratch buffer reused within
 *         the frame, a portrait being edited)
 *   upload  at the draw call, so the pixels are read before the core can
 *         reuse the buffer
 *   evict   when video RAM or the table is full: first the textures unused
 *         for 120 frames, then any not used by this frame or the last one.
 *         The last frame may still be on the PVR (it renders frame N while
 *         the game builds N+1), so its textures are never freed or reused.
 * The font pages and the checker are "static" slots that are never
 * evicted (font.c).
 */
#include <string.h>
#include "dc2d.h"

#define TEX_STATIC  8
#define TEX_DYN     2048
#define TEX_SLOTS   (TEX_STATIC + TEX_DYN)
#define HASH_SIZE   4096                     /* power of two, > 1.5 * TEX_DYN */
#define TEX_MAXDIM  1024
#define SCRATCH_TEXELS (512 * 1024)          /* the largest texture: 1024x512 */

typedef struct Ent {
    uint32_t key, sum;
    uint32_t last;                           /* frame it was last used        */
    uint8_t  live, uid;                      /* uid: 0 none, 1 the image's, 2 a remap */
} Ent;

static DcTex    tex[TEX_SLOTS];
static Ent      ent[TEX_SLOTS];
static uint16_t hash[HASH_SIZE];             /* slot + 1, 0 = empty           */
static int      nstatic, ndyn_live;
static uint32_t cur_frame = 1;
static uint16_t scratch[SCRATCH_TEXELS / 2];
static int      warned_big;

/* ---- twiddling ----------------------------------------------------------------- */
static uint32_t spread(uint32_t v)          /* bit i -> bit 2i */
{
    v &= 0xFFFFu;
    v = (v | (v << 8)) & 0x00FF00FFu;
    v = (v | (v << 4)) & 0x0F0F0F0Fu;
    v = (v | (v << 2)) & 0x33333333u;
    v = (v | (v << 1)) & 0x55555555u;
    return v;
}

uint32_t dc2d_twiddle_index(uint32_t x, uint32_t y, int tw, int th)
{
    uint32_t m = (uint32_t)(tw < th ? tw : th), blk;
    if (tw > th) { blk = x / m; x %= m; }
    else         { blk = y / m; y %= m; }
    return blk * m * m + (spread(x) << 1) + spread(y);
}

void dc2d_twiddle8(uint16_t *dst, const uint8_t *src, int w, int h, int tw, int th)
{
    uint8_t *d = (uint8_t *)dst;
    int x, y;
    memset(dst, 0, (size_t)tw * (size_t)th);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint8_t c = src[y * w + x];
            if (c) d[dc2d_twiddle_index((uint32_t)x, (uint32_t)y, tw, th)] = c;
        }
    /* the PVR reads texels as little-endian 16-bit pairs: on a big-endian
     * host this would need a swap; the SH4 and the test hosts are LE */
}

/* ---- slots -------------------------------------------------------------------------- */
static int pow2(int v)
{
    int p = 8;
    while (p < v) p <<= 1;
    return p;
}

static uint32_t hkey(uint32_t key, uint32_t sum, int w, int h)
{
    uint32_t k = key * 2654435761u ^ (sum + 0x9E3779B9u) * 0x85EBCA6Bu ^ ((uint32_t)w << 16 | (uint32_t)h);
    k ^= k >> 15;
    return k & (HASH_SIZE - 1);
}

static void hash_insert(int slot)
{
    uint32_t i = hkey(ent[slot].key, ent[slot].sum, tex[slot].w, tex[slot].h);
    while (hash[i]) i = (i + 1) & (HASH_SIZE - 1);
    hash[i] = (uint16_t)(slot + 1);
}

static void hash_rebuild(void)
{
    int s;
    memset(hash, 0, sizeof(hash));
    for (s = TEX_STATIC; s < TEX_SLOTS; s++)
        if (ent[s].live) hash_insert(s);
}

static int hash_find(uint32_t key, uint32_t sum, int w, int h, int uid)
{
    uint32_t i = hkey(key, sum, w, h);
    while (hash[i]) {
        int s = hash[i] - 1;
        if (ent[s].key == key && ent[s].sum == sum && ent[s].uid == uid &&
            tex[s].w == w && tex[s].h == h)
            return s;
        i = (i + 1) & (HASH_SIZE - 1);
    }
    return -1;
}

static void free_slot(int s)
{
    dc2d_be_tex_free(tex[s].handle);
    ent[s].live = 0;
    ndyn_live--;
    dc2d_stats.evictions++;
}

/* free the dynamic textures last used more than `age` frames ago (never
 * this frame's or the last frame's); 1 if any went */
static int evict(uint32_t age)
{
    int s, any = 0;
    if (age < 1) age = 1;
    for (s = TEX_STATIC; s < TEX_SLOTS; s++)
        if (ent[s].live && ent[s].last + age < cur_frame) { free_slot(s); any = 1; }
    if (any) hash_rebuild();
    return any;
}

static int alloc_tex(uint32_t bytes, uint32_t *handle)
{
    if (dc2d_be_tex_alloc(bytes, handle)) return 1;
    if (evict(120) && dc2d_be_tex_alloc(bytes, handle)) return 1;
    if (evict(1) && dc2d_be_tex_alloc(bytes, handle)) return 1;
    return 0;
}

static int free_dyn_slot(void)
{
    int s;
    if (ndyn_live >= TEX_DYN && !evict(120) && !evict(1)) return -1;
    for (s = TEX_STATIC; s < TEX_SLOTS; s++)
        if (!ent[s].live) return s;
    return -1;
}

static int upload(int s, const uint8_t *pix, int w, int h)
{
    int tw = pow2(w), th = pow2(h);
    uint32_t bytes = (uint32_t)tw * (uint32_t)th;
    dc2d_twiddle8(scratch, pix, w, h, tw, th);
    dc2d_be_tex_write(tex[s].handle, scratch, bytes);
    dc2d_stats.uploads++;
    dc2d_stats.upload_bytes += bytes;
    return 1;
}

void dc2d_tex_init(void)
{
    memset(ent, 0, sizeof(ent));
    memset(hash, 0, sizeof(hash));
    nstatic = 0;
    ndyn_live = 0;
}

void dc2d_tex_frame(uint32_t frame) { cur_frame = frame; }

void dc2d_tex_mark(int slot, uint32_t frame)
{
    if (slot >= 0 && slot < TEX_SLOTS) ent[slot].last = frame;
}

const DcTex *dc2d_tex(int slot) { return &tex[slot]; }

int dc2d_tex_live(void) { return ndyn_live; }

static int fits(int w, int h)
{
    if (w <= 0 || h <= 0) return 0;
    if (w > TEX_MAXDIM || h > TEX_MAXDIM || (uint32_t)pow2(w) * (uint32_t)pow2(h) > SCRATCH_TEXELS) {
        if (!warned_big) {
            warned_big = 1;
            dc2d_be_log("DC3: image too big for a texture (max 1024x512), not drawn");
        }
        return 0;
    }
    return 1;
}

int dc2d_tex_static(const uint8_t *pix, int w, int h, int tw, int th)
{
    int s = nstatic;
    uint32_t handle;
    if (s >= TEX_STATIC || !fits(tw, th) || !dc2d_be_tex_alloc((uint32_t)tw * (uint32_t)th, &handle))
        return -1;
    nstatic++;
    tex[s].handle = handle;
    tex[s].w = (uint16_t)w; tex[s].h = (uint16_t)h;
    tex[s].tw = (uint16_t)tw; tex[s].th = (uint16_t)th;
    ent[s].live = 1;
    dc2d_twiddle8(scratch, pix, w, h, tw, th);
    dc2d_be_tex_write(handle, scratch, (uint32_t)tw * (uint32_t)th);
    return s;
}

void dc2d_tex_restatic(int s, const uint8_t *pix, int w, int h)
{
    if (s < 0 || s >= nstatic) return;
    dc2d_twiddle8(scratch, pix, w, h, tex[s].tw, tex[s].th);
    dc2d_be_tex_write(tex[s].handle, scratch, (uint32_t)tex[s].tw * (uint32_t)tex[s].th);
}

/* a cheap checksum of the pixels of an image without a uid */
static uint32_t pix_sum(const uint8_t *p, uint32_t n)
{
    uint32_t a = 1, b = n;
    uint32_t i = 0;
    for (; i + 4 <= n; i += 4) {
        a += (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8) | ((uint32_t)p[i + 2] << 16) | ((uint32_t)p[i + 3] << 24);
        b += a ^ (a >> 7);
    }
    for (; i < n; i++) { a += p[i]; b += a; }
    return a ^ (b * 0x9E3779B1u);
}

/* a copy of a texture with every texel index sent through map[] (a
 * colormap level): the texels are read back from video RAM, so it works
 * whatever the image's pixels hold now; texel order does not matter */
int dc2d_tex_remap(int src, int level, const uint8_t *map)
{
    int s, w, h, i;
    uint32_t key, sum, handle, bytes;
    uint8_t *t = (uint8_t *)scratch;
    if (src < TEX_STATIC || src >= TEX_SLOTS || !ent[src].live) return -1;
    w = tex[src].w; h = tex[src].h;
    key = (ent[src].key ^ ((uint32_t)level * 0x9E3779B1u)) * 2654435761u + ent[src].uid;
    sum = ent[src].sum * 0x85EBCA6Bu + (uint32_t)level + 0x51u;
    s = hash_find(key, sum, w, h, 2);
    if (s >= 0) {
        ent[s].last = cur_frame;
        return s;
    }
    /* src was used by this frame, so no eviction below can take it */
    s = free_dyn_slot();
    bytes = (uint32_t)tex[src].tw * (uint32_t)tex[src].th;
    if (s < 0 || !alloc_tex(bytes, &handle)) {
        dc2d_stats.tex_fail++;
        return -1;
    }
    dc2d_be_tex_read(tex[src].handle, scratch, bytes);
    for (i = 0; i < (int)bytes; i++) t[i] = map[t[i]];
    tex[s] = tex[src];
    tex[s].handle = handle;
    ent[s].key = key; ent[s].sum = sum; ent[s].uid = 2;
    ent[s].last = cur_frame;
    ent[s].live = 1;
    ndyn_live++;
    hash_insert(s);
    dc2d_be_tex_write(handle, scratch, bytes);
    dc2d_stats.uploads++;
    dc2d_stats.upload_bytes += bytes;
    return s;
}

int dc2d_tex_image(const DoomoImage *img)
{
    int w = img->w, h = img->h, uid = img->uid != 0, s;
    uint32_t key, sum, handle;
    key = uid ? img->uid : (uint32_t)(uintptr_t)img->pix;
    sum = uid ? 0u : pix_sum(img->pix, (uint32_t)w * (uint32_t)h);
    s = hash_find(key, sum, w, h, uid);
    if (s >= 0) {
        ent[s].last = cur_frame;
        return s;
    }
    if (!fits(w, h)) { dc2d_stats.tex_fail++; return -1; }
    s = free_dyn_slot();
    if (s < 0 || !alloc_tex((uint32_t)pow2(w) * (uint32_t)pow2(h), &handle)) {
        dc2d_stats.tex_fail++;
        return -1;
    }
    tex[s].handle = handle;
    tex[s].w = (uint16_t)w; tex[s].h = (uint16_t)h;
    tex[s].tw = (uint16_t)pow2(w); tex[s].th = (uint16_t)pow2(h);
    ent[s].key = key; ent[s].sum = sum; ent[s].uid = (uint8_t)uid;
    ent[s].last = cur_frame;
    ent[s].live = 1;
    ndyn_live++;
    hash_insert(s);
    upload(s, img->pix, w, h);
    return s;
}
