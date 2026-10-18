/* obj.c - plat_image_xform through affine OBJs, and the OAM shadow
 * (owner: S1; postfx.h, docs/design/story/SPEC.md sec.7 S1, R18, C7.4-C7.8).
 *
 * A squashed hero actor (sx/sy != 1.0) becomes one 8bpp affine OBJ,
 * double-size, scaled about its bottom-centre anchor: the bitmap renderer
 * never pays for the scale. The rules (C7):
 *   - up to OBJ_XF_SLOTS squash OBJs per frame, in call order; any more,
 *     and images that do not fit 64x64 or use IMG_SHADOW / IMG_SILHOUETTE,
 *     draw unsquashed into the bitmap (plat_image);
 *   - never while a 3D view runs (r3d owns OBJ VRAM then, C7.4): a frame
 *     with a plat_view3d now or one frame ago draws unsquashed;
 *   - OBJs sit above the whole mode-4 bitmap, so a squash OBJ may only be
 *     shown if nothing drawn LATER in the frame covers its box (the
 *     dialogue box, the HUD, a neighbour, an iris). Slots follow the call
 *     order. A slot's first frame is a probe: the image is drawn unsquashed
 *     into the bitmap and the bitmap under its box is checksummed right
 *     after and again before the flip. Unchanged: the following frames use
 *     the OBJ (still checked every frame). Changed (or the shared bbox
 *     accumulator reports a later rect over it): the slot stays in the
 *     bitmap, unsquashed, until the caller has not used it for 15 frames.
 *     A cover that appears while an OBJ is up costs one frame with the
 *     OBJ in front, never a frame without the actor;
 *   - shake and zoom (postfx.c) are composed into each OBJ's position and
 *     matrix at the flip, so the OBJ moves with the bitmap.
 * VRAM: 4 KB per slot from 0x06014000 (tiles 512..1023), uploaded only
 * when the image changes. OAM: entries 0..3 + affine groups 0..3, through
 * a 16-entry own shadow (128 B), DMA'd in the vblank of the flip.
 *
 * RAM: 128 B shadow + 4 x 24 B slots + 8 B = 232 B of .bss and .data.
 */
#include "postfx.h"

#define OBJ_VRAM      ((vu16 *)0x06014000)
#define SLOT_BYTES    4096u
#define SLOT_TILE(k)  (512 + (k) * (int)(SLOT_BYTES / 32u))
#define SAMPLE_STEP_Y 2

typedef struct {
    uint32_t key;              /* uploaded image: (uid or pix) ^ sampled content */
    uint32_t sum;              /* bitmap checksum under the box at issue time   */
    int16_t  ax, cy;           /* OBJ centre before the post transform          */
    int16_t  pa, pd;           /* image matrix (8.8, inverse scale)             */
    uint8_t  bx, by, bw, bh;   /* the scaled image's screen box (clipped)       */
    uint8_t  shape, size;      /* OBJ shape / size codes                        */
    uint8_t  state;            /* S_* | M_* (this frame's mode) << 4            */
    uint8_t  idle;             /* frames since the caller last used the slot    */
} XfSlot;                      /* 24 B */
enum { S_FREE, S_PROBE, S_SHOW, S_BAD };               /* the slot's state      */
enum { M_NONE, M_PROBE, M_OBJ };                       /* this frame            */
#define ST(x)    ((x)->state & 15)
#define MODE(x)  ((x)->state >> 4)
#define SET_ST(x, st, m) ((x)->state = (uint8_t)((st) | ((m) << 4)))
#define IDLE_FREE 15

static uint16_t own_shadow[OBJ_SHADOW_N * 4] __attribute__((aligned(4)));
static uint16_t *shadow = own_shadow;
static uint8_t  shadow_n = OBJ_SHADOW_N;
static uint8_t  slots_used, any_shown;
static XfSlot   xs[OBJ_XF_SLOTS];

/* the OBJ size table: shape 0 square, 1 wide, 2 tall; size 0..3 */
static const uint8_t obj_w[3][4] = { { 8, 16, 32, 64 }, { 16, 32, 32, 64 }, { 8, 8, 16, 32 } };
static const uint8_t obj_h[3][4] = { { 8, 16, 32, 64 }, { 8, 8, 16, 32 }, { 16, 32, 32, 64 } };

void obj_init(void)
{
    int i;
    vu16 *oam = (vu16 *)MEM_OAM;
    for (i = 0; i < 128; i++) { oam[i * 4] = 0x0200; oam[i * 4 + 1] = 0; oam[i * 4 + 2] = 0; }
    for (i = 0; i < OBJ_SHADOW_N; i++) { own_shadow[i * 4] = 0x0200; own_shadow[i * 4 + 1] = 0; own_shadow[i * 4 + 2] = 0; own_shadow[i * 4 + 3] = 0; }
}

void obj_set_shadow(uint16_t *buf, int n)
{
    int i;
    if (!buf || n <= 0) { buf = own_shadow; n = OBJ_SHADOW_N; }
    if (n > 128) n = 128;
    /* entries the new shadow does not cover go hidden in OAM */
    for (i = n; i < shadow_n; i++) ((vu16 *)MEM_OAM)[i * 4] = 0x0200;
    shadow = buf;
    shadow_n = (uint8_t)n;
}

uint16_t *obj_shadow(int *n) { if (n) *n = shadow_n; return shadow; }
int obj_active(void) { return any_shown; }

/* smallest OBJ that holds w x h */
static int pick_size(int w, int h, uint8_t *shape, uint8_t *size)
{
    int best = -1, s, z, area = 1 << 30;
    for (s = 0; s < 3; s++)
        for (z = 0; z < 4; z++)
            if (obj_w[s][z] >= w && obj_h[s][z] >= h && obj_w[s][z] * obj_h[s][z] < area) {
                area = obj_w[s][z] * obj_h[s][z];
                *shape = (uint8_t)s; *size = (uint8_t)z;
                best = 1;
            }
    return best > 0;
}

static uint32_t content_hash(const DoomoImage *img)
{
    uint32_t h = 2166136261u, n = (uint32_t)img->w * img->h, i, step = n / 61u + 1u;
    for (i = 0; i < n; i += step) h = (h ^ img->pix[i]) * 16777619u;
    return h ^ n;
}

/* the image, bottom-centred in a W x H box, as 8bpp 1D-mapped tiles */
static void upload(int k, const DoomoImage *img, int W, int H)
{
    vu16 *dst = OBJ_VRAM + (uint32_t)k * (SLOT_BYTES / 2u);
    int ox = (W - img->w) / 2, oy = H - img->h, tx, ty, r, c;
    for (ty = 0; ty < H / 8; ty++)
        for (tx = 0; tx < W / 8; tx++)
            for (r = 0; r < 8; r++) {
                int sy = ty * 8 + r - oy;
                const uint8_t *row = (sy >= 0 && sy < img->h) ? img->pix + (uint32_t)sy * img->w : NULL;
                for (c = 0; c < 8; c += 2) {
                    int sx = tx * 8 + c - ox;
                    uint16_t lo = 0, hi = 0;
                    if (row) {
                        if (sx >= 0 && sx < img->w) lo = row[sx];
                        if (sx + 1 >= 0 && sx + 1 < img->w) hi = row[sx + 1];
                    }
                    *dst++ = (uint16_t)(lo | (hi << 8));
                }
            }
}

/* checksum of the back page under a box (every other row, 2 px a read) */
static uint32_t box_sum(int x, int y, int w, int h)
{
    uint32_t s = 0x9E3779B9u;
    int yy, xx, x0 = x >> 1, x1 = (x + w + 1) >> 1;
    for (yy = y; yy < y + h; yy += SAMPLE_STEP_Y) {
        const uint16_t *row = gba_back + yy * (SCREEN_W / 2);
        for (xx = x0; xx < x1; xx++) s = (s << 5) + s + row[xx];
    }
    return s;
}

static void draw_unsquashed(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                            int flags, uint8_t color)
{
    int asx = sx_q8 < 0 ? -sx_q8 : sx_q8, s = ((asx + sy_q8) / 2 + 128) >> 8;
    s = DOOMO_CLAMP(s, 1, 4);
    if (sx_q8 < 0) flags ^= IMG_FLIPX;
    plat_image(ax - (int)img->w * s / 2, ay - (int)img->h * s, img, s, flags, color);
}

static void set_box(XfSlot *x, int ax, int ay, const DoomoImage *img, int asx, int sy_q8)
{
    int x0 = ax - (img->w * asx) / 512 - 1, x1 = ax + (img->w * asx + 511) / 512 + 1;
    int y0 = ay - (img->h * sy_q8 + 255) / 256 - 1, y1 = ay + 1;
    x0 = DOOMO_CLAMP(x0, 0, SCREEN_W); x1 = DOOMO_CLAMP(x1, 0, SCREEN_W);
    y0 = DOOMO_CLAMP(y0, 0, SCREEN_H); y1 = DOOMO_CLAMP(y1, 0, SCREEN_H);
    x->bx = (uint8_t)x0; x->by = (uint8_t)y0;
    x->bw = (uint8_t)(x1 - x0); x->bh = (uint8_t)(y1 - y0);
    x->sum = (x->bw > 0 && x->bh > 0) ? box_sum(x0, y0, x->bw, x->bh) : 0;
}

void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                      int flags, uint8_t color)
{
    int asx, k, W, H, st;
    XfSlot *x;
    uint32_t key;
    if (gba_nodraw || !img || !img->pix || !img->w || !img->h || sx_q8 == 0 || sy_q8 <= 0) return;
    asx = sx_q8 < 0 ? -sx_q8 : sx_q8;
    if (flags & IMG_FLIPX) sx_q8 = -sx_q8;
    flags &= ~IMG_FLIPX;
    if (slots_used >= OBJ_XF_SLOTS) { draw_unsquashed(ax, ay, img, sx_q8, sy_q8, flags, color); return; }
    k = slots_used++;                          /* every call keeps its slot: identity = call order */
    x = &xs[k];
    x->idle = 0;
    st = ST(x);
    if ((asx == 256 && sy_q8 == 256) || st == S_BAD ||
        (flags & (IMG_SHADOW | IMG_SILHOUETTE)) || img->w > 64 || img->h > 64 ||
        gba_frames - gba_objvram_r3d_frame() <= 1u || asx > 512 || sy_q8 > 512 ||
        asx < 32 || sy_q8 < 32 || !pick_size(img->w, img->h, &x->shape, &x->size)) {
        draw_unsquashed(ax, ay, img, sx_q8, sy_q8, flags, color);
        SET_ST(x, st, M_NONE);
        return;
    }
    if (st == S_FREE) {                        /* probe: bitmap now, OBJ from the next frame */
        draw_unsquashed(ax, ay, img, sx_q8, sy_q8, flags, color);
        set_box(x, ax, ay, img, asx, sy_q8);
        SET_ST(x, S_PROBE, M_PROBE);
        return;
    }
    W = obj_w[x->shape][x->size];
    H = obj_h[x->shape][x->size];
    if (gba_objvram_claim(OBJV_OBJ) != OBJV_OBJ) {           /* r3d's tiles were there */
        int j;
        for (j = 0; j < OBJ_XF_SLOTS; j++) xs[j].key = 0;
    }
    key = (img->uid ? img->uid : (uint32_t)(uintptr_t)img->pix) * 2654435761u;
    key ^= content_hash(img) ^ ((uint32_t)W << 8) ^ (uint32_t)H;
    key |= 1u;                                 /* 0 = nothing uploaded */
    if (x->key != key) {
        upload(k, img, W, H);
        x->key = key;
    }
    /* matrix: texture = centre + M^-1 (screen - centre) */
    x->pa = (int16_t)(65536 / sx_q8);
    x->pd = (int16_t)(65536 / sy_q8);
    x->ax = (int16_t)ax;
    x->cy = (int16_t)(ay - (H * sy_q8) / 512);
    set_box(x, ax, ay, img, asx, sy_q8);
    SET_ST(x, S_SHOW, M_OBJ);
}

/* hide entry i (keeps its attr3: an affine group may live there) */
static void hide(int i) { if (i < shadow_n) shadow[i * 4] = 0x0200; }

void obj_pre_flip(void)
{
    int k, dx, dy, s, cx, cy, have = postfx_xform(&dx, &dy, &s, &cx, &cy);
    int bx0, by0, bx1, by1, bb = postfx_bbox(&bx0, &by0, &bx1, &by1);
    int vram_lost = gba_objvram_owner() != OBJV_OBJ;          /* r3d ran after us */
    any_shown = 0;
    for (k = 0; k < OBJ_XF_SLOTS; k++) {
        XfSlot *x = &xs[k];
        int W, H, px, py, pa, pd, ox, oy, m = MODE(x), covered;
        uint16_t *e = shadow + k * 4;
        if (k >= slots_used) {                 /* not used this frame */
            if (x->idle < 255) x->idle++;
            SET_ST(x, x->idle >= IDLE_FREE ? S_FREE : ST(x), M_NONE);
            hide(k);
            continue;
        }
        if (m == M_NONE) { hide(k); continue; }
        covered = (x->bw > 0 && box_sum(x->bx, x->by, x->bw, x->bh) != x->sum) ||
                  (bb && bx0 < x->bx + x->bw && bx1 > x->bx && by0 < x->by + x->bh && by1 > x->by);
        SET_ST(x, covered ? S_BAD : S_SHOW, M_NONE);
        if (m == M_PROBE || (m == M_OBJ && vram_lost)) {      /* drawn in the bitmap */
            if (vram_lost) x->key = 0;
            hide(k);
            continue;
        }
        /* M_OBJ: shown this frame even if a cover just appeared (the
         * actor never vanishes; the bitmap takes over next frame) */
        W = obj_w[x->shape][x->size];
        H = obj_h[x->shape][x->size];
        px = x->ax; py = x->cy; pa = x->pa; pd = x->pd;
        if (have) {                            /* compose the shake / zoom */
            px = cx + (s * (px - cx)) / 256 + dx;
            py = cy + (s * (py - cy)) / 256 + dy;
            pa = pa * 256 / s;
            pd = pd * 256 / s;
        }
        ox = px - W;                           /* double-size box: 2W x 2H */
        oy = py - H;
        if (ox <= -2 * W || ox >= SCREEN_W || oy <= -2 * H || oy >= SCREEN_H) { hide(k); continue; }
        e[0] = (uint16_t)((oy & 0xFF) | 0x0100 | 0x0200 | 0x2000 | (x->shape << 14));
        e[1] = (uint16_t)((ox & 0x1FF) | (k << 9) | (x->size << 14));
        e[2] = (uint16_t)SLOT_TILE(k);
        /* affine group k: PA..PD in the attr3 of entries 4k..4k+3 */
        shadow[(k * 4 + 0) * 4 + 3] = (uint16_t)pa;
        shadow[(k * 4 + 1) * 4 + 3] = 0;
        shadow[(k * 4 + 2) * 4 + 3] = 0;
        shadow[(k * 4 + 3) * 4 + 3] = (uint16_t)pd;
        any_shown = 1;
    }
    slots_used = 0;
}

void obj_vblank(void)
{
    static uint8_t was_shown;
    if (any_shown || was_shown) dma3_copy32((void *)MEM_OAM, shadow, (uint32_t)shadow_n * 2u);
    was_shown = any_shown;
}
