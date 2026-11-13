/* test_stage_clip.c - clipped redraw (G2-EX, backdrop.h stage_layer_clip, plat_ext.h plat_clip,
 * docs/EXORCISM_SPEED.md): a layer that did not change but lies on a dirty box is drawn only inside
 * the dirty set, so it does not widen the set and drag every layer that lies on its box into the
 * redraw; and the platform's clip draws exactly the pixels of the unclipped call that lie in the box.
 * The whole ritual against a full redraw is test_stage.c. */
#include "test.h"
#include "doomo/backdrop.h"
#include "doomo/plat_ext.h"
#include <stdlib.h>

static void set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

static void fill_backdrop(uint32_t key)
{
    plat_clear((uint8_t)(1 + (key & 7)));
}

/* ---- the primitives under plat_clip ------------------------------------------------------ */
static uint32_t rnd_s;
static int rnd(int n) { rnd_s = rnd_s * 1664525u + 1013904223u; return (int)((rnd_s >> 8) % (uint32_t)n); }

static void noise(void)
{
    int i;
    for (i = 0; i < nat_w * nat_h; i++) nat_fb[i] = (uint8_t)(1 + rnd(120));
}

/* one random 2D call of the kinds a clip honours */
static void call(int kind, int x, int y, int w, int h, const DoomoImage *img, int c)
{
    switch (kind) {
    case 0: plat_rect(x, y, w, h, (uint8_t)c); break;
    case 1: plat_shade_rect(x, y, w, h, c & 15); break;
    case 2: plat_image(x, y, img, 1, 0, 0); break;
    case 3: plat_image(x, y, img, 1, IMG_SHADOW, 0); break;
    default: plat_image(x, y, img, 1, IMG_SILHOUETTE, (uint8_t)c); break;
    }
}

TEST(clip_draws_exactly_the_unclipped_pixels_inside_the_box)
{
    static uint8_t pix[20 * 14], ref[NAT_MAX_W * NAT_MAX_H], start[NAT_MAX_W * NAT_MAX_H];
    DoomoImage img;
    int n, i, bad = 0;
    set_res(240, 160);
    nat_stage_reset();
    rnd_s = 12345u;
    for (i = 0; i < (int)sizeof(pix); i++) pix[i] = (uint8_t)(rnd(3) ? 1 + rnd(200) : 0);   /* holes */
    img.w = 20; img.h = 14; img.pix = pix; img.uid = doomo_image_new_uid();
    for (n = 0; n < 400 && !bad; n++) {
        int kind = rnd(5), x = rnd(260) - 20, y = rnd(180) - 20, w = 1 + rnd(50), h = 1 + rnd(40);
        int bx = rnd(240), by = rnd(160), bw = 1 + rnd(120), bh = 1 + rnd(90), c = 1 + rnd(200);
        int x0 = bx, y0 = by, x1 = DOOMO_MIN(bx + bw, nat_w), y1 = DOOMO_MIN(by + bh, nat_h);
        noise();
        memcpy(start, nat_fb, (size_t)(nat_w * nat_h));
        call(kind, x, y, w, h, &img, c);
        memcpy(ref, nat_fb, (size_t)(nat_w * nat_h));
        memcpy(nat_fb, start, (size_t)(nat_w * nat_h));
        plat_clip(x0, y0, x1, y1);
        call(kind, x, y, w, h, &img, c);
        plat_clip_end();
        for (i = 0; i < nat_w * nat_h; i++) {
            int px = i % nat_w, py = i / nat_w, in = px >= x0 && px < x1 && py >= y0 && py < y1;
            if (nat_fb[i] != (in ? ref[i] : start[i])) { bad = n + 1; break; }
        }
        CHECK_MSG(!bad, "call %d kind %d at %d,%d %dx%d clipped to %d,%d..%d,%d: pixel %d differs", n, kind, x, y, w, h,
                  x0, y0, x1, y1, i);
    }
    CHECK_EQ(nat_clip_viol, 0);
    CHECK(!nat_clip_on);
}

/* ---- the stage's decisions ------------------------------------------------------------------ */
enum { A = 0, B = 1, C = 2, D = 3 };
typedef struct { int drawn[4], n[4]; int16_t piece[4][8][4]; int full; } Frame;

/* one frame: A (clippable, still), B (a box that moves with `bx`), C (clippable, lies on A only),
 * D (not clippable, lies on B's old place) */
static void stage_frame(int f, int bx, Frame *out)
{
    StageStats st;
    int i, k;
    plat_begin_frame();
    memset(out, 0, sizeof(*out));
    if (stage_begin(DOOMO_BACKDROP_EXO_CHURCH, 0, fill_backdrop)) {
        stage_layer_clip(A, 10, 10, 60, 60, 1);
        stage_layer(B, bx, 50, 40, 40, (uint32_t)f);
        stage_layer_clip(C, 5, 5, 20, 20, 1);
        stage_layer(D, 120, 52, 30, 30, 1);
        stage_resolve();
        for (i = 0; i < 4; i++) {
            out->drawn[i] = stage_draw(i);
            out->n[i] = out->drawn[i] ? stage_clip_n(i) : 0;
            for (k = 0; k < out->n[i] && k < 8; k++) stage_clip_get(i, k, out->piece[i][k]);
        }
        stage_stats(&st);
        out->full = st.full;
        stage_end();
    }
    plat_end_frame();
}

TEST(stage_clipped_layer_lies_on_a_dirty_box_without_widening_it)
{
    static uint8_t mem[8192];
    Frame f;
    int i;
    CHECK(stage_mem_bytes() <= sizeof(mem));
    set_res(240, 160);
    nat_stage_on = 1;
    nat_stage_buffers = 2;
    nat_stage_reset();
    stage_bind(mem);
    for (i = 0; i < 4; i++) stage_frame(i, 50, &f);          /* B changes its key only: its box stays */
    CHECK(!f.full);
    CHECK(f.drawn[B]);                                       /* changed */
    CHECK(f.drawn[A] && f.n[A] == 1);                        /* lies on B: through one piece */
    CHECK_EQ(f.piece[A][0][0], 50); CHECK_EQ(f.piece[A][0][1], 50);
    CHECK_EQ(f.piece[A][0][2], 70); CHECK_EQ(f.piece[A][0][3], 70);   /* A's box cut to B's */
    CHECK(!f.drawn[C]);       /* C lies on A's box, which is not put back: nothing of it is dirty */
    CHECK(!f.drawn[D]);       /* D is clear of B's box */
    nat_stage_on = 0;
}

TEST(stage_unclipped_layer_still_widens_the_dirty_set)
{
    static uint8_t mem[8192];
    Frame f;
    int i;
    set_res(240, 160);
    nat_stage_on = 1;
    nat_stage_buffers = 2;
    nat_stage_reset();
    stage_bind(mem);
    for (i = 0; i < 4; i++) stage_frame(i, 100, &f);         /* B's box [100,140): D [120,150) lies on it */
    CHECK(!f.full);
    CHECK(f.drawn[B] && f.drawn[D]);
    CHECK_EQ(f.n[D], 0);                                     /* not clippable: drawn whole */
    CHECK(!f.drawn[A] && !f.drawn[C]);
    nat_stage_on = 0;
}

TEST(stage_clip_pieces_are_disjoint_and_cover_the_dirty_part_of_the_layer)
{
    static uint8_t mem[8192], seen[240 * 160];
    Frame f;
    int i, k, x, y, n_in = 0, n_exp = 0;
    set_res(240, 160);
    nat_stage_on = 1;
    nat_stage_buffers = 2;
    nat_stage_reset();
    stage_bind(mem);
    /* B moves: the dirty set is its old box and its new one (an L of two boxes that overlap A) */
    stage_frame(0, 40, &f);
    stage_frame(1, 40, &f);
    stage_frame(2, 40, &f);
    stage_frame(3, 20, &f);                                  /* old [40,80) x [50,90), new [20,60) x [50,90) */
    CHECK(!f.full && f.drawn[A]);
    memset(seen, 0, sizeof(seen));
    CHECK(f.n[A] >= 1 || f.piece[A][0][2] == 0);
    for (i = 0; i < f.n[A] && i < 8; i++)
        for (y = f.piece[A][i][1]; y < f.piece[A][i][3]; y++)
            for (x = f.piece[A][i][0]; x < f.piece[A][i][2]; x++) {
                CHECK_MSG(!seen[y * 240 + x], "pixel %d,%d is in two pieces", x, y);
                seen[y * 240 + x] = 1;
                n_in++;
            }
    for (y = 10; y < 70; y++)
        for (x = 10; x < 70; x++) {
            int dirty = (x >= 40 && x < 80 && y >= 50 && y < 90) || (x >= 20 && x < 60 && y >= 50 && y < 90);
            if (dirty) { n_exp++; CHECK_MSG(f.n[A] == 0 || seen[y * 240 + x], "pixel %d,%d of A's dirty part is in no piece", x, y); }
        }
    if (f.n[A] > 0) CHECK_EQ(n_in, n_exp);
    for (k = 0; k < f.n[A] && k < 8; k++) CHECK(f.piece[A][k][2] > f.piece[A][k][0] && f.piece[A][k][3] > f.piece[A][k][1]);
    nat_stage_on = 0;
}
