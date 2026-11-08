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
#include <string.h>

int nat_stage_on;
int nat_stage_buffers = 2;

static uint8_t bd_pix[NAT_MAX_W * NAT_MAX_H];
static int bd_id = -1, bd_w, bd_h;
static uint32_t bd_key;
/* the emulated buffers: their pictures, the serial of the stage frame each holds (0 none),
 * the box drawn over each after its plat_stage_end */
static uint8_t buf_pix[2][NAT_MAX_W * NAT_MAX_H];
static uint32_t buf_serial[2], serial;
static int16_t buf_dirt[2][4];
static int cur_buf, frame_stage, tracking, frames, last_kept;
static int16_t trk[4];

/* the backdrop (id, key) drawn with `draw` into bd_pix: the screen is saved around it */
static void render(int id, uint32_t key, void (*draw)(uint32_t))
{
    static uint8_t save[NAT_MAX_W * NAT_MAX_H];
    size_t n = (size_t)nat_w * (size_t)nat_h;
    int on = tracking;
    tracking = 0;
    memcpy(save, nat_fb, n);
    draw(key);
    memcpy(bd_pix, nat_fb, n);
    memcpy(nat_fb, save, n);
    tracking = on;
    bd_id = id;
    bd_key = key;
    bd_w = nat_w;
    bd_h = nat_h;
}

int plat_stage_begin(int id, uint32_t key, void (*draw)(uint32_t key), PlatStageInfo *info)
{
    if (!nat_stage_on || !draw || !info) return 0;
    if (bd_id != id || bd_key != key || bd_w != nat_w || bd_h != nat_h) render(id, key, draw);
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
    if (bd_id < 0 || x0 >= x1 || y0 >= y1) return;
    for (j = y0; j < y1; j++)
        memcpy(&nat_fb[j * nat_w + x0], &bd_pix[j * nat_w + x0], (size_t)(x1 - x0));
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

void nat_stage_reset(void)
{
    memset(buf_serial, 0, sizeof(buf_serial));
    memset(buf_dirt, 0, sizeof(buf_dirt));
    bd_id = -1;
    cur_buf = frame_stage = tracking = frames = 0;
}

/* ---- the baker: every key of every family at the runner's screen size --------------------
 * File: "BKD1", u16 w, u16 h, u32 n, n x { u32 id << 24 | key, u32 offset }, sorted, then the
 * pictures (w * h bytes each, 4-aligned). Little endian. */
static void put32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    fwrite(b, 1, 4, f);
}

int nat_bake_backdrops(const char *path)
{
    enum { MAXK = 512 };
    static uint32_t ids[MAXK];
    uint32_t keys[MAXK];
    int nf = backdrop_family_count(), i, k, n = 0, j;
    size_t img = (size_t)nat_w * (size_t)nat_h, pad = (4 - img % 4) % 4;
    uint32_t off;
    FILE *f;
    for (i = 0; i < nf; i++) {
        const DoomoBackdrop *b = backdrop_family(i);
        int c = b ? b->keys(keys, MAXK) : 0;
        if (c > MAXK || n + c > MAXK) { fprintf(stderr, "bake: too many keys\n"); return 0; }
        for (k = 0; k < c; k++) ids[n++] = (uint32_t)b->id << 24 | (keys[k] & 0xFFFFFFu);
    }
    /* sorted (insertion sort: a few hundred keys) */
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && ids[j - 1] > ids[j]; j--) { uint32_t t = ids[j]; ids[j] = ids[j - 1]; ids[j - 1] = t; }
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "bake: cannot write %s\n", path); return 0; }
    fwrite("BKD1", 1, 4, f);
    fputc(nat_w & 255, f); fputc(nat_w >> 8, f);
    fputc(nat_h & 255, f); fputc(nat_h >> 8, f);
    put32(f, (uint32_t)n);
    off = 12u + 8u * (uint32_t)n;
    for (i = 0; i < n; i++) {
        put32(f, ids[i]);
        put32(f, off);
        off += (uint32_t)(img + pad);
    }
    for (i = 0; i < n; i++) {
        int id = (int)(ids[i] >> 24);
        const DoomoBackdrop *b = NULL;
        for (k = 0; k < nf; k++)
            if (backdrop_family(k) && backdrop_family(k)->id == id) b = backdrop_family(k);
        memset(nat_fb, 0, img);
        if (b) b->draw(ids[i] & 0xFFFFFFu);
        fwrite(nat_fb, 1, img, f);
        for (k = 0; k < (int)pad; k++) fputc(0, f);
    }
    if (fclose(f) != 0) return 0;
    return n;
}
