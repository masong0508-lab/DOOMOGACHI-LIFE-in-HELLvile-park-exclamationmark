/* vram.c - VRAM texture caches (see the layout in ps1.h).
 *
 *  - world textures: 80 slots of 64x64 8bpp, keyed by pixel pointer (so
 *    every frame of an animated texture is its own slot), LRU.
 *  - image cache: DoomoImage tiles (<= 128x128) shelf-packed into six 8bpp
 *    pages, keyed by (uid or pixel pointer, tile, flip). A page is evicted
 *    as a whole (oldest first, never one used in the frame being built).
 *  - CLUTs: master palette, silhouette colours (LRU), 256 font colours.
 *  - sky (256x96, re-uploaded when the sky frame changes), font, blob.
 * All run-time uploads go through gfx_upload*(), i.e. into the head of the
 * current frame's display list.
 */
#include <string.h>
#include "ps1.h"

/* ---- world textures ------------------------------------------------------- */
typedef struct WSlot { const uint8_t *key; uint32_t used; } WSlot;
static WSlot  s_wslot[VR_WTEX_SLOTS];
static TexRef s_wref[VR_WTEX_SLOTS];
static struct { const uint8_t *key; uint16_t slot; uint32_t frame; } s_wmemo[64];
static uint32_t s_upbytes;
static uint32_t s_vframe = 1;

static void wslot_ref(int s)
{
    int page = s >> 4, cell = s & 15;
    int col = cell & 3, row = cell >> 2;
    TexRef *r = &s_wref[s];
    r->tpage = getTPage(1, 0, VR_WTEX_X + page * 128, 0);
    r->clut  = getClut(VR_CLUT_MASTER_X, VR_CLUT_MASTER_Y);
    r->u = (uint8_t)(col * 64);
    r->v = (uint8_t)(row * 64);
    r->ok = 1;
}

static void wslot_upload(int s, const uint8_t *pix)
{
    int page = s >> 4, cell = s & 15;
    int x = VR_WTEX_X + page * 128 + (cell & 3) * 32;
    int y = (cell >> 2) * 64;
    gfx_upload_strided(x, y, 32, 64, pix, 64);
    s_upbytes += 4096;
}

const TexRef *vram_wtex(int tex_id, uint16_t time)
{
    const uint8_t *pix;
    int i, best, s;
    uint32_t oldest;
    if (tex_id <= 0 || tex_id >= 64)
        tex_id = 1;
    pix = core_texture_pixels(tex_id, time);
    if (!pix)
        return NULL;
    if (s_wmemo[tex_id].frame == s_vframe && s_wmemo[tex_id].key == pix)
        return &s_wref[s_wmemo[tex_id].slot];
    s = -1;
    for (i = 0; i < VR_WTEX_SLOTS; i++)
        if (s_wslot[i].key == pix) { s = i; break; }
    if (s < 0) {
        best = -1;
        oldest = 0xffffffffu;
        for (i = 0; i < VR_WTEX_SLOTS; i++) {
            if (!s_wslot[i].key) { best = i; break; }
            if (s_wslot[i].used < oldest && s_wslot[i].used != s_vframe) {
                oldest = s_wslot[i].used;
                best = i;
            }
        }
        if (best < 0)
            return NULL;            /* 80 different textures in one frame */
        if (gfx_free_bytes() < 6000)
            return NULL;
        s = best;
        s_wslot[s].key = pix;
        wslot_upload(s, pix);
    }
    s_wslot[s].used = s_vframe;
    s_wmemo[tex_id].key = pix;
    s_wmemo[tex_id].slot = (uint16_t)s;
    s_wmemo[tex_id].frame = s_vframe;
    return &s_wref[s];
}

/* ---- image cache ---------------------------------------------------------- */
#define IMG_PAGES   6
#define MAX_SHELF   40
#define HASH_SIZE   1024
#define HASH_MASK   (HASH_SIZE - 1)

typedef struct Shelf { uint8_t y, h; uint16_t x; } Shelf;
typedef struct Page {
    uint16_t vx;          /* VRAM x of the page                  */
    uint16_t w;           /* width in texels (256 or 128)        */
    uint16_t top;         /* next free row                       */
    uint16_t nshelf;
    uint32_t used;        /* last frame any entry was used       */
    uint32_t gen;
    Shelf    shelf[MAX_SHELF];
} Page;

typedef struct Entry {
    uint32_t k1, k2;      /* key: uid or pixel pointer, tile|flip|w|h    */
    uint32_t gen;         /* valid iff == page gen                       */
    uint8_t  page, state; /* state: 0 empty, 1 used, 2 tombstone         */
    uint8_t  u, v;
} Entry;

static Page   s_page[IMG_PAGES];
static Entry  s_hash[HASH_SIZE];
static int    s_hash_used;
static uint8_t s_stage[(IMG_TILE + 2) * (IMG_TILE + 2)];

static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    return h;
}

static void page_reset(Page *p)
{
    p->top = 0;
    p->nshelf = 0;
    p->gen++;
}

static void hash_rebuild(void)
{
    static Entry tmp[HASH_SIZE];
    int i;
    memcpy(tmp, s_hash, sizeof(tmp));
    memset(s_hash, 0, sizeof(s_hash));
    s_hash_used = 0;
    for (i = 0; i < HASH_SIZE; i++) {
        Entry *e = &tmp[i];
        if (e->state == 1 && e->gen == s_page[e->page].gen) {
            uint32_t h = hash2(e->k1, e->k2) & HASH_MASK;
            while (s_hash[h].state)
                h = (h + 1) & HASH_MASK;
            s_hash[h] = *e;
            s_hash_used++;
        }
    }
}

/* Allocate w x h texels (w includes padding). Returns page or -1. */
static int img_alloc(int w, int h, int *ou, int *ov)
{
    int pass, i, j;
    int sh = (h + 7) & ~7;
    w = (w + 1) & ~1;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < IMG_PAGES; i++) {
            Page *p = &s_page[i];
            if (w > p->w)
                continue;
            for (j = 0; j < p->nshelf; j++) {
                Shelf *s = &p->shelf[j];
                if (s->h >= h && s->h <= sh + 16 && s->x + w <= p->w) {
                    *ou = s->x;
                    *ov = s->y;
                    s->x = (uint16_t)(s->x + w);
                    return i;
                }
            }
            if (p->nshelf < MAX_SHELF && p->top + sh <= 256) {
                Shelf *s = &p->shelf[p->nshelf++];
                s->y = (uint8_t)p->top;
                s->h = (uint8_t)(sh > 255 ? 255 : sh);
                s->x = (uint16_t)w;
                p->top = (uint16_t)(p->top + sh);
                *ou = 0;
                *ov = s->y;
                return i;
            }
        }
        if (pass == 0) {
            /* evict the least recently used page not used this frame */
            int best = -1;
            uint32_t oldest = 0xffffffffu;
            for (i = 0; i < IMG_PAGES; i++)
                if (s_page[i].used != s_vframe && s_page[i].used < oldest &&
                    w <= s_page[i].w) {
                    oldest = s_page[i].used;
                    best = i;
                }
            if (best < 0)
                return -1;
            page_reset(&s_page[best]);
        }
    }
    return -1;
}

int vram_image(const DoomoImage *img, int tx, int ty, TexRef *out)
{
    uint32_t k1, k2, h;
    int flip = (tx >> 8) & 1;
    int tw, th, x0, y0, u, v, pg, row, col;
    Entry *e, *free_e = NULL;
    out->ok = 0;
    tx &= 0xff;
    if (!img || !img->pix || !img->w || !img->h)
        return 0;
    x0 = tx * IMG_TILE;
    y0 = ty * IMG_TILE;
    if (x0 >= img->w || y0 >= img->h)
        return 0;
    tw = img->w - x0; if (tw > IMG_TILE) tw = IMG_TILE;
    th = img->h - y0; if (th > IMG_TILE) th = IMG_TILE;
    k1 = img->uid ? img->uid : (uint32_t)(uintptr_t)img->pix;
    k2 = ((uint32_t)tx) | ((uint32_t)ty << 6) | ((uint32_t)flip << 12) |
         ((uint32_t)img->w << 13) | ((uint32_t)(img->h & 0x7ff) << 21) |
         (img->uid ? 0u : 0x80000000u);
    h = hash2(k1, k2) & HASH_MASK;
    for (;;) {
        e = &s_hash[h];
        if (e->state == 0)
            break;
        if (e->state == 1 && e->k1 == k1 && e->k2 == k2) {
            if (e->gen == s_page[e->page].gen)
                goto hit;
            if (!free_e) free_e = e;  /* stale: reuse */
        } else if (e->state == 2 && !free_e) {
            free_e = e;
        }
        h = (h + 1) & HASH_MASK;
    }
    /* miss: allocate and upload */
    if (gfx_free_bytes() < (size_t)(tw + 2) * (th + 1) + 2048)
        return 0;
    pg = img_alloc(tw + 2, th + 1, &u, &v);
    if (pg < 0)
        return 0;
    if (!free_e) {
        if (s_hash_used >= HASH_SIZE * 3 / 4) {
            hash_rebuild();
            return vram_image(img, tx | (flip << 8), ty, out);
        }
        free_e = e;
        s_hash_used++;
    }
    e = free_e;
    e->k1 = k1; e->k2 = k2; e->state = 1;
    e->page = (uint8_t)pg;
    e->gen = s_page[pg].gen;
    e->u = (uint8_t)(u + 1);     /* 1 transparent column on the left  */
    e->v = (uint8_t)v;
    {
        /* stage: (tw+2) x (th+1), transparent border right/bottom/left */
        int sw = (tw + 2 + 1) & ~1;
        memset(s_stage, 0, (size_t)sw * (th + 1));
        for (row = 0; row < th; row++) {
            const uint8_t *src = img->pix + (size_t)(y0 + row) * img->w;
            uint8_t *d = s_stage + row * sw + 1;
            if (!flip) {
                memcpy(d, src + x0, (size_t)tw);
            } else {
                /* flipped image: tile tx counts from the right edge */
                int sx = img->w - 1 - x0;
                for (col = 0; col < tw; col++)
                    d[col] = src[sx - col];
            }
        }
        gfx_upload_strided(s_page[pg].vx + (u >> 1), VR_IMG_Y + v,
                           sw >> 1, th + 1, s_stage, sw);
        s_upbytes += (uint32_t)(sw * (th + 1));
    }
hit:
    s_page[e->page].used = s_vframe;
    out->tpage = getTPage(1, 0, s_page[e->page].vx, VR_IMG_Y);
    out->clut = getClut(VR_CLUT_MASTER_X, VR_CLUT_MASTER_Y);
    out->u = e->u;
    out->v = e->v;
    out->ok = 1;
    return 1;
}

/* ---- CLUTs ----------------------------------------------------------------- */
static struct { int16_t color; uint32_t used; } s_sil[VR_CLUT_SIL_SLOTS];
static uint16_t s_clutbuf[256];

uint16_t vram_sil_clut(uint8_t color)
{
    int i, best = 0;
    uint32_t oldest = 0xffffffffu;
    for (i = 0; i < VR_CLUT_SIL_SLOTS; i++) {
        if (s_sil[i].color == color) {
            s_sil[i].used = s_vframe;
            return getClut(0, VR_CLUT_SIL_Y + i);
        }
        if (s_sil[i].used < oldest) { oldest = s_sil[i].used; best = i; }
    }
    for (i = 1; i < 256; i++)
        s_clutbuf[i] = g_pal15[color] ? g_pal15[color] : 0x8000;
    s_clutbuf[0] = 0;
    gfx_upload(0, VR_CLUT_SIL_Y + best, 256, 1, s_clutbuf);
    s_sil[best].color = color;
    s_sil[best].used = s_vframe;
    return getClut(0, VR_CLUT_SIL_Y + best);
}

uint16_t vram_font_clut(uint8_t color)
{
    return getClut((color & 15) * 16, VR_CLUT_FONT_Y + (color >> 4));
}

uint16_t vram_font_tpage(void) { return getTPage(0, 0, VR_FONT_X, 0); }

/* ---- sky -------------------------------------------------------------------- */
static const uint8_t *s_sky_ptr;

int vram_sky(int sky_id, uint16_t time, TexRef *left, TexRef *right)
{
    const uint8_t *p = core_sky_pixels(sky_id, time);
    if (!p)
        return 0;
    if (p != s_sky_ptr) {
        if (gfx_free_bytes() < SKY_W * SKY_H + 4096)
            return 0;
        gfx_upload_strided(VR_SKY_X, VR_SKY_Y, 64, SKY_H, p, SKY_W);
        gfx_upload_strided(VR_SKY_X, VR_SKY_Y + SKY_H, 64, SKY_H, p + 128, SKY_W);
        s_upbytes += SKY_W * SKY_H;
        s_sky_ptr = p;
    }
    left->tpage = right->tpage = getTPage(1, 0, VR_SKY_X, 0);
    left->clut = right->clut = getClut(VR_CLUT_MASTER_X, VR_CLUT_MASTER_Y);
    left->u = right->u = 0;
    left->v = 0;
    right->v = SKY_H;
    left->ok = right->ok = 1;
    return 1;
}

void vram_blob(TexRef *out)
{
    out->tpage = getTPage(0, 2, VR_BLOB_X, 0);  /* 4bpp, subtractive */
    out->clut = getClut(VR_CLUT_MISC_X, VR_CLUT_MISC_Y);
    out->u = (uint8_t)((VR_BLOB_X - VR_FONT_X) * 4);
    out->v = VR_BLOB_Y;
    out->ok = 1;
}

uint32_t vram_uploads(void) { return s_upbytes; }

void vram_frame(void)
{
    s_vframe++;
    s_upbytes = 0;
}

/* ---- init -------------------------------------------------------------------- */
static void upload_font(void)
{
    static uint16_t tex[32 * 64];
    const uint8_t (*font)[5] = core_font();
    int g, col, row;
    memset(tex, 0, sizeof(tex));
    for (g = 0; g < 128; g++) {
        int gx = (g & 15) * 8, gy = (g >> 4) * 8;
        for (col = 0; col < 5; col++) {
            uint8_t bits = font[g][col];
            for (row = 0; row < 8; row++) {
                if (bits & (1 << row)) {
                    int tx = gx + col, ty = gy + row;
                    tex[ty * 32 + (tx >> 2)] |= (uint16_t)(1u << ((tx & 3) * 4));
                }
            }
        }
    }
    gfx_upload_now(VR_FONT_X, VR_FONT_Y, 32, 64, tex);
}

static void upload_cluts(void)
{
    static uint16_t font_cluts[16][256];
    int c;
    for (c = 0; c < 256; c++)
        s_clutbuf[c] = g_pal15[c];
    gfx_upload_now(VR_CLUT_MASTER_X, VR_CLUT_MASTER_Y, 256, 1, s_clutbuf);
    memset(font_cluts, 0, sizeof(font_cluts));
    for (c = 0; c < 256; c++)
        font_cluts[c >> 4][(c & 15) * 16 + 1] = g_pal15[c] ? g_pal15[c] : 0x8000;
    gfx_upload_now(0, VR_CLUT_FONT_Y, 256, 16, font_cluts);
}

static void upload_blob(void)
{
    /* 32x32 soft disc, 4bpp: value = darkness 0..15 */
    static uint16_t tex[8 * 32];
    uint16_t clut[16];
    int x, y, i;
    memset(tex, 0, sizeof(tex));
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            int dx = x * 2 - 31, dy = y * 2 - 31;
            int d2 = dx * dx + dy * dy;           /* 0 .. ~1922 */
            int v = d2 >= 961 ? 0 : 15 - (d2 * 15) / 961;
            if (v > 11) v = 11;
            tex[y * 8 + (x >> 2)] |= (uint16_t)(v << ((x & 3) * 4));
        }
    gfx_upload_now(VR_BLOB_X, VR_BLOB_Y, 8, 32, tex);
    for (i = 0; i < 16; i++) {
        int g = i * 2;                            /* subtract up to ~0.7 */
        clut[i] = (uint16_t)(i ? (0x8000 | g | (g << 5) | (g << 10)) : 0);
    }
    gfx_upload_now(VR_CLUT_MISC_X, VR_CLUT_MISC_Y, 16, 1, clut);
}

void vram_init(void)
{
    int i;
    memset(s_wslot, 0, sizeof(s_wslot));
    for (i = 0; i < VR_WTEX_SLOTS; i++)
        wslot_ref(i);
    memset(s_page, 0, sizeof(s_page));
    for (i = 0; i < IMG_PAGES; i++) {
        s_page[i].vx = (uint16_t)(i < 5 ? 320 + i * 128 : 960);
        s_page[i].w = (uint16_t)(i < 5 ? 256 : 128);
        s_page[i].gen = 1;
    }
    memset(s_hash, 0, sizeof(s_hash));
    for (i = 0; i < VR_CLUT_SIL_SLOTS; i++)
        s_sil[i].color = -1;
    upload_cluts();
    upload_font();
    upload_blob();
}
