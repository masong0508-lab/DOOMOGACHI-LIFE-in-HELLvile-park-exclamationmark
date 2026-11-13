/* stage_native.c - the kept backdrop on the native platform (SP7; plat_ext.h plat_stage_*,
 * owner: foundation; docs/EXORCISM_SPEED.md).
 *
 * Off by default (nat_stage_on = 0): plat_stage_begin says "cannot" and every scene draws its
 * backdrop itself, so screenshots never depend on it. core/tests/test_stage.c turns it on and
 * plays a scene with and without it, frame by frame. When it is on:
 *   - the backdrop is drawn with the scene's own `draw` into a copy (once per key);
 *   - nat_stage_buffers (1 or 2) frame buffers are emulated: with 2 the frame drawn now
 *     starts from the picture of two frames ago, like the GBA's two Mode 4 pages;
 *   - every 2D call after plat_stage_end widens the box the next frame drawn into this
 *     buffer gets as info.dirt (nat_stage_note, called from nat_panel_op).
 * The baker (runner --bake-backdrops) writes every key of every family for the GBA build.
 */
#include "native.h"
#include "doomo/plat_ext.h"
#include "doomo/backdrop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nat_stage_on;
int nat_stage_buffers = 2;

/* the pictures drawn so far ((S2-4b) several: a frame may be built from bands, and stamps are
 * pictures too): whole screens and bands are a full-screen copy, a stamp is the box of what its
 * draw painted on a screen cleared to 0, as an image (index 0 a hole) */
typedef struct {
    int id, kind;                 /* family, BACKDROP_* */
    uint32_t key;
    int16_t y0, y1;               /* BACKDROP_ROWS: its rows */
    int16_t bx, by;               /* BACKDROP_STAMP: the box's corner from the anchor */
    uint8_t *pix;
    DoomoImage img;
} NPic;
#define NPIC_CAP 16384            /* open-addressed (id, key) table */
static NPic *pics[NPIC_CAP];
static int npics, pic_w, pic_h;
static const NPic *cur_pic;
static int stamps_drawn;
/* the emulated buffers: their pictures, the serial of the stage frame each holds (0 none),
 * the box drawn over each after its plat_stage_end */
static uint8_t buf_pix[2][NAT_MAX_W * NAT_MAX_H];
static uint32_t buf_serial[2], serial;
static int16_t buf_dirt[2][4];
static int cur_buf, frame_stage, tracking, frames, last_kept;
static int16_t trk[4];

static void pics_flush(void)
{
    int i;
    for (i = 0; i < NPIC_CAP; i++)
        if (pics[i]) { free(pics[i]->pix); free(pics[i]); pics[i] = NULL; }
    npics = 0;
    cur_pic = NULL;
}

static const DoomoBackdrop *family_of(int id)
{
    int i, n = backdrop_family_count();
    for (i = 0; i < n; i++)
        if (backdrop_family(i) && backdrop_family(i)->id == id) return backdrop_family(i);
    return NULL;
}

/* the picture of (id, key), drawn with `draw` on first use: the screen is saved around it */
static const NPic *pic_get(int id, uint32_t key, void (*draw)(uint32_t))
{
    static uint8_t save[2][NAT_MAX_W * NAT_MAX_H];
    static int depth;
    const DoomoBackdrop *f = family_of(id);
    size_t n = (size_t)nat_w * (size_t)nat_h;
    unsigned h = (unsigned)(((uint32_t)id * 2654435761u) ^ (key * 40503u)) % NPIC_CAP;
    int16_t part[2] = { 0, 0 };
    int on = tracking;
    NPic *p;
    if (pic_w != nat_w || pic_h != nat_h) { pics_flush(); pic_w = nat_w; pic_h = nat_h; }
    while (pics[h]) {
        if (pics[h]->id == id && pics[h]->key == key) return pics[h];
        h = (h + 1) % NPIC_CAP;
    }
    if (!draw || depth >= 2 || npics >= NPIC_CAP / 2) return NULL;
    p = (NPic *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->id = id;
    p->key = key;
    p->kind = f && f->part ? f->part(key, part) : BACKDROP_WHOLE;
    tracking = 0;
    memcpy(save[depth], nat_fb, n);
    depth++;
    if (p->kind == BACKDROP_STAMP) {
        int x, y, x0 = nat_w, y0 = nat_h, x1 = -1, y1 = -1;
        memset(nat_fb, 0, n);
        draw(key);
        for (y = 0; y < nat_h; y++)
            for (x = 0; x < nat_w; x++)
                if (nat_fb[y * nat_w + x]) {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
        if (x1 >= x0) {
            p->img.w = (uint16_t)(x1 - x0 + 1);
            p->img.h = (uint16_t)(y1 - y0 + 1);
            p->pix = (uint8_t *)malloc((size_t)p->img.w * p->img.h);
            for (y = 0; y < p->img.h; y++)
                memcpy(p->pix + (size_t)y * p->img.w, nat_fb + (y0 + y) * nat_w + x0, p->img.w);
            p->img.pix = p->pix;
            p->img.uid = doomo_image_new_uid();
            p->bx = (int16_t)(x0 - part[0]);
            p->by = (int16_t)(y0 - part[1]);
        }
    } else {
        draw(key);
        p->pix = (uint8_t *)malloc(n);
        memcpy(p->pix, nat_fb, n);
        p->y0 = p->kind == BACKDROP_ROWS ? part[0] : 0;
        p->y1 = p->kind == BACKDROP_ROWS ? part[1] : (int16_t)nat_h;
    }
    depth--;
    memcpy(nat_fb, save[depth], n);
    tracking = on;
    pics[h] = p;
    npics++;
    return p;
}

int plat_stage_begin(int id, uint32_t key, void (*draw)(uint32_t key), PlatStageInfo *info)
{
    const NPic *p;
    if (!nat_stage_on || !draw || !info) return 0;
    p = pic_get(id, key, draw);
    if (!p || p->kind == BACKDROP_STAMP) return 0;
    cur_pic = p;
    info->held = buf_serial[cur_buf];
    info->serial = serial + 1;
    info->dx0 = buf_dirt[cur_buf][0]; info->dy0 = buf_dirt[cur_buf][1];
    info->dx1 = buf_dirt[cur_buf][2]; info->dy1 = buf_dirt[cur_buf][3];
    frame_stage = 1;
    return 1;
}

void plat_stage_restore(int x, int y, int w, int h)
{
    int j, x0 = DOOMO_MAX(x, 0), y0 = DOOMO_MAX(y, 0);
    int x1 = DOOMO_MIN(x + w, nat_w), y1 = DOOMO_MIN(y + h, nat_h);
    if (!cur_pic) return;
    y0 = DOOMO_MAX(y0, cur_pic->y0);
    y1 = DOOMO_MIN(y1, cur_pic->y1);
    if (x0 >= x1 || y0 >= y1) return;
    for (j = y0; j < y1; j++)
        memcpy(&nat_fb[j * nat_w + x0], &cur_pic->pix[j * nat_w + x0], (size_t)(x1 - x0));
}

/* (S2-4b) the stamp of a key at (x, y): drawn once with its family's draw, then an image */
int plat_stage_stamp(int id, uint32_t key, int x, int y)
{
    const DoomoBackdrop *f = family_of(id);
    const NPic *p;
    if (!nat_stage_on || !f) return 0;
    p = pic_get(id, key, f->draw);
    if (!p || p->kind != BACKDROP_STAMP) return 0;
    if (p->pix) plat_image(x + p->bx, y + p->by, &p->img, 1, 0, 0);
    stamps_drawn++;
    return 1;
}

void plat_stage_end(void)
{
    if (!frame_stage) return;
    tracking = 1;
    trk[0] = trk[1] = 0x7FFF;
    trk[2] = trk[3] = -0x7FFF;
}

/* nat_panel_op: every 2D call's box (exclusive, unclipped) */
void nat_stage_note(int x0, int y0, int x1, int y1)
{
    if (!tracking) return;
    x0 = DOOMO_MAX(x0, 0); y0 = DOOMO_MAX(y0, 0);
    x1 = DOOMO_MIN(x1, nat_w); y1 = DOOMO_MIN(y1, nat_h);
    if (x0 >= x1 || y0 >= y1) return;
    if (x0 < trk[0]) trk[0] = (int16_t)x0;
    if (y0 < trk[1]) trk[1] = (int16_t)y0;
    if (x1 > trk[2]) trk[2] = (int16_t)x1;
    if (y1 > trk[3]) trk[3] = (int16_t)y1;
}

/* plat_begin_frame: start from the picture this buffer holds */
void nat_stage_frame_begin(void)
{
    if (!nat_stage_on) return;
    if (frames > 0 && nat_stage_buffers == 2)
        memcpy(nat_fb, buf_pix[cur_buf], (size_t)nat_w * (size_t)nat_h);
}

/* plat_end_frame: the frame is presented; the next one is drawn into the other buffer */
void nat_stage_frame_end(void)
{
    if (!nat_stage_on) return;
    serial++;
    buf_serial[cur_buf] = frame_stage ? serial : 0;
    if (frame_stage && tracking && trk[0] < trk[2]) {
        memcpy(buf_dirt[cur_buf], trk, sizeof(trk));
    } else {
        buf_dirt[cur_buf][0] = buf_dirt[cur_buf][1] = buf_dirt[cur_buf][2] = buf_dirt[cur_buf][3] = 0;
    }
    if (nat_stage_buffers == 2) memcpy(buf_pix[cur_buf], nat_fb, (size_t)nat_w * (size_t)nat_h);
    last_kept = frame_stage;
    frame_stage = 0;
    tracking = 0;
    frames++;
    if (nat_stage_buffers == 2) cur_buf ^= 1;
}

/* (S2-4) 1 when the frame last presented was drawn over a kept backdrop (tests) */
int nat_stage_kept(void) { return last_kept; }

/* (S2-4b) every picture asked for since the last reset, as id << 24 | key (tests: all of them must be
 * in the baked lists), and how many stamps were drawn */
int nat_stage_pics(uint32_t *out, int cap)
{
    int i, n = 0;
    for (i = 0; i < NPIC_CAP; i++)
        if (pics[i]) {
            if (n < cap) out[n] = (uint32_t)pics[i]->id << 24 | (pics[i]->key & 0xFFFFFFu);
            n++;
        }
    return n;
}
int nat_stage_stamps(void) { return stamps_drawn; }

void nat_stage_reset(void)
{
    memset(buf_serial, 0, sizeof(buf_serial));
    memset(buf_dirt, 0, sizeof(buf_dirt));
    pics_flush();
    stamps_drawn = 0;
    cur_buf = frame_stage = tracking = frames = 0;
}

/* ---- the baker: every key of every family at the runner's screen size --------------------
 * File: "BKD2", u16 w, u16 h, u32 n, n x { u32 id << 24 | key, u32 offset, i16 bx, i16 by,
 * u16 w, u16 h }, sorted by the first word, then the pictures (w * h bytes each, 4-aligned).
 * A whole screen: box (0, 0, W, H); a band (BACKDROP_ROWS): (0, y0, W, y1 - y0); a stamp
 * (BACKDROP_STAMP): the box of what was painted, its corner counted from the anchor (w = 0:
 * nothing painted). Little endian. */
static void put32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    fwrite(b, 1, 4, f);
}

int nat_bake_backdrops(const char *path)
{
    enum { MAXK = 8192 };
    static uint32_t ids[MAXK], keys[MAXK];
    static struct { int16_t bx, by; uint16_t w, h; uint8_t *pix; } pic[MAXK];
    int nf = backdrop_family_count(), i, k, n = 0, j;
    size_t img = (size_t)nat_w * (size_t)nat_h;
    uint32_t off;
    FILE *f;
    for (i = 0; i < nf; i++) {
        const DoomoBackdrop *b = backdrop_family(i);
        int c = b ? b->keys(keys, MAXK) : 0;
        if (c > MAXK || n + c > MAXK) { fprintf(stderr, "bake: too many keys\n"); return 0; }
        for (k = 0; k < c; k++) ids[n++] = (uint32_t)b->id << 24 | (keys[k] & 0xFFFFFFu);
    }
    /* sorted (insertion sort: a few thousand keys) */
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && ids[j - 1] > ids[j]; j--) { uint32_t t = ids[j]; ids[j] = ids[j - 1]; ids[j - 1] = t; }
    for (i = 1; i < n; i++)
        if (ids[i] == ids[i - 1]) { fprintf(stderr, "bake: key 0x%x listed twice\n", (unsigned)ids[i]); return 0; }
    for (i = 0; i < n; i++) {
        const DoomoBackdrop *b = family_of((int)(ids[i] >> 24));
        uint32_t key = ids[i] & 0xFFFFFFu;
        int16_t part[2] = { 0, 0 };
        int kind = b && b->part ? b->part(key, part) : BACKDROP_WHOLE;
        int x, y, x0 = 0, y0 = 0, x1 = nat_w, y1 = nat_h;
        memset(nat_fb, 0, img);
        if (b) b->draw(key);
        if (kind == BACKDROP_ROWS) {
            y0 = DOOMO_CLAMP(part[0], 0, nat_h);
            y1 = DOOMO_CLAMP(part[1], y0, nat_h);
        } else if (kind == BACKDROP_STAMP) {
            x0 = nat_w; y0 = nat_h; x1 = y1 = 0;
            for (y = 0; y < nat_h; y++)
                for (x = 0; x < nat_w; x++)
                    if (nat_fb[y * nat_w + x]) {
                        if (x < x0) x0 = x;
                        if (x + 1 > x1) x1 = x + 1;
                        if (y < y0) y0 = y;
                        if (y + 1 > y1) y1 = y + 1;
                    }
            if (x1 <= x0) x0 = y0 = x1 = y1 = 0;
            if (x1 > x0 && (x0 == 0 || y0 == 0 || x1 == nat_w || y1 == nat_h)) {
                fprintf(stderr, "bake: stamp 0x%x touches the screen edge (clipped?)\n", (unsigned)ids[i]);
                return 0;
            }
        }
        pic[i].w = (uint16_t)(x1 - x0);
        pic[i].h = (uint16_t)(y1 - y0);
        pic[i].bx = (int16_t)(kind == BACKDROP_STAMP ? x0 - part[0] : x0);
        pic[i].by = (int16_t)(kind == BACKDROP_STAMP ? y0 - part[1] : y0);
        pic[i].pix = (uint8_t *)malloc((size_t)pic[i].w * pic[i].h + 4);
        if (!pic[i].pix) return 0;
        for (y = y0; y < y1; y++)
            memcpy(pic[i].pix + (size_t)(y - y0) * pic[i].w, nat_fb + y * nat_w + x0, pic[i].w);
    }
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "bake: cannot write %s\n", path); return 0; }
    fwrite("BKD2", 1, 4, f);
    fputc(nat_w & 255, f); fputc(nat_w >> 8, f);
    fputc(nat_h & 255, f); fputc(nat_h >> 8, f);
    put32(f, (uint32_t)n);
    off = 12u + 16u * (uint32_t)n;
    for (i = 0; i < n; i++) {
        uint32_t sz = (uint32_t)pic[i].w * pic[i].h;
        put32(f, ids[i]);
        put32(f, off);
        put32(f, (uint32_t)(uint16_t)pic[i].bx | (uint32_t)(uint16_t)pic[i].by << 16);
        put32(f, (uint32_t)pic[i].w | (uint32_t)pic[i].h << 16);
        off += (sz + 3u) & ~3u;
    }
    for (i = 0; i < n; i++) {
        uint32_t sz = (uint32_t)pic[i].w * pic[i].h;
        fwrite(pic[i].pix, 1, sz, f);
        for (k = 0; k < (int)((4 - sz % 4) % 4); k++) fputc(0, f);
        free(pic[i].pix);
    }
    if (fclose(f) != 0) return 0;
    return n;
}
