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
    case 5: plat_text(x, y, "Holy water\nPray 90%", (uint8_t)c); break;
    case 6: plat_text_outline(x, y, "Name: Nox", (uint8_t)c, (uint8_t)(c ^ 0x55)); break;
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
    nat_clip_viol = 0;
    rnd_s = 12345u;
    for (i = 0; i < (int)sizeof(pix); i++) pix[i] = (uint8_t)(rnd(3) ? 1 + rnd(200) : 0);   /* holes */
    img.w = 20; img.h = 14; img.pix = pix; img.uid = doomo_image_new_uid();
    for (n = 0; n < 400 && !bad; n++) {
        int kind = rnd(7), x = rnd(260) - 20, y = rnd(180) - 20, w = 1 + rnd(50), h = 1 + rnd(40);
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

/* ---- the ritual to its end, against a full redraw ------------------------------------------------ */
#include "../src/exorcism_int.h"          /* exo_aura_bind */
uint32_t exo_probe_host_uid(void);        /* the host's picture of the last frame */
int exo_probe_result(void);          /* exorcism_scene.c (DOOMO_HITCHECK): 0 not at the result, 1 a failure, 2 a cast out */

#define RITUAL_FRAMES 1700                 /* the result screen to its fade to black */

static void rframe(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static uint32_t fb_hash(void)
{
    uint32_t h = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

/* core/tests/scripts/life_exorcism.txt as a pad (test_stage.c: the same presses) */
static uint32_t pad(int f)
{
    static const struct { int16_t at; uint16_t b; } p[] = {
        { 40, BTN_A }, { 46, BTN_A }, { 52, BTN_A }, { 58, BTN_A }, { 64, BTN_A }, { 70, BTN_A },
        { 92, BTN_DOWN }, { 94, BTN_A }, { 96, BTN_DOWN }, { 98, BTN_DOWN }, { 100, BTN_DOWN }, { 102, BTN_A },
        { 114, BTN_START }, { 126, BTN_A }, { 134, BTN_A },
        { 206, BTN_A }, { 213, BTN_LEFT }, { 222, BTN_B }, { 233, BTN_RIGHT },
        { 505, BTN_A }, { 513, BTN_A }, { 521, BTN_A }, { 529, BTN_A },
        { 575, BTN_A }, { 590, BTN_A }, { 605, BTN_A }, { 620, BTN_A }, { 635, BTN_A }, { 650, BTN_A },
        { 1180, BTN_A }, { 1188, BTN_A }, { 1196, BTN_A }, { 1204, BTN_A },
        { 1230, BTN_RIGHT }, { 1240, BTN_A },
    };
    unsigned i;
    if (f >= 550 && f < 570) return BTN_UP;
    for (i = 0; i < sizeof(p) / sizeof(p[0]); i++)
        if (p[i].at == f) return p[i].b;
    return 0;
}

/* plays the ritual with `seed` into out[]; returns the result screen's kind (exo_probe_result) and counts
 * the frames drawn over the kept backdrop with a clipped layer */
static int ritual_noaura;                  /* 1: the aura without its rect list, as the shape code draws it */
static int ritual(uint32_t seed, int stage, uint32_t *out, int *clipped)
{
    NatOptions o;
    StageStats st;
    int f;
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.seed = seed; o.quiet = 1; o.save_dir = test_tmp_dir;
    nat_init(&o);
    nat_stage_on = stage;
    nat_stage_buffers = 2;
    nat_stage_reset();
    test_reset();
    doomo_init();
    scene_set(&SCENE_TITLE, 0);
    rframe(0);
    scene_set(&SCENE_EXORCISM, -1);
    if (ritual_noaura) exo_aura_bind(NULL);
    *clipped = 0;
    nat_clip_viol = 0;
    for (f = 0; f < RITUAL_FRAMES; f++) {
        rframe(pad(f));
        out[f] = fb_hash();
        stage_stats(&st);
        if (st.retained && st.clipped) (*clipped)++;
    }
    nat_stage_on = 0;
    return exo_probe_result();
}

/* a failure to the end of its fade, and a cast out (a seed whose roll succeeds is found, not assumed) */
TEST(stage_exorcism_result_screens_are_pixel_identical)
{
    static uint32_t off[RITUAL_FRAMES], off2[RITUAL_FRAMES], on[RITUAL_FRAMES];
    uint32_t seed;
    int kinds = 0, f, clipped, got;
    for (seed = 0x5EED1234u; seed < 0x5EED1234u + 80u && kinds != 3; seed++) {
        got = ritual(seed, 0, off2, &clipped);             /* (the first run of a seed warms the portrait caches) */
        if (got < 1 || (kinds & got)) continue;            /* not at the result, or this kind is done */
        ritual(seed, 0, off, &clipped);
        ritual(seed, 1, on, &clipped);
        for (f = 0; f < RITUAL_FRAMES; f++)
            CHECK_MSG(on[f] == off[f], "seed %u (%s): frame %d differs with the stage on", (unsigned)seed,
                      got == 2 ? "cast out" : "failure", f);
        CHECK_MSG(clipped > RITUAL_FRAMES / 4 && !nat_clip_viol, "seed %u: %d frames with a clipped layer, %d unhonoured calls",
                  (unsigned)seed, clipped, nat_clip_viol);
        kinds |= got;
    }
    CHECK_MSG(kinds == 3, "no seed gave both a failure and a cast out (got %d)", kinds);
    set_res(240, 160);
}

/* the aura's kept rect list draws the pixels the shape code draws (prayer to the result: dread 5 to 12) */
TEST(exorcism_aura_rect_list_equals_the_shape_code)
{
    static uint32_t a[RITUAL_FRAMES], b[RITUAL_FRAMES];
    int f, clipped, i, diff = 0;
    ritual_noaura = 0;
    ritual(0x5EED1234u, 0, a, &clipped);                   /* (the first run warms the portrait caches) */
    ritual(0x5EED1234u, 0, a, &clipped);
    ritual_noaura = 1;
    ritual(0x5EED1234u, 0, b, &clipped);
    ritual_noaura = 0;
    for (f = 0, i = 0; f < RITUAL_FRAMES; f++) diff += a[f] != b[f], i = f;
    CHECK_MSG(diff == 0, "%d of %d frames differ without the rect list (last %d)", diff, RITUAL_FRAMES, i);
    set_res(240, 160);
}

/* (G2-EX, pixel-changing) the wall's drips hold one length: the church's key no longer changes every 8 ticks from
 * dread 9 on (it redrew the whole screen on two frames in a row each time) */
TEST(exorcism_church_key_does_not_carry_a_drip_step)
{
    int d;
    uint32_t t;
    for (d = 0; d <= 15; d++)
        for (t = 0; t < 240; t++) {
            uint32_t k = exo_church_key(d, t), k0 = exo_church_key(d, d >= 13 ? 1u : 0u);
            if (d < 13) CHECK_MSG(k == k0, "dread %d tick %u: key 0x%x moves", d, (unsigned)t, (unsigned)k);
            CHECK_MSG((k & ~0x80u) == (uint32_t)d, "dread %d tick %u: key 0x%x holds more than the dread and the beams", d, (unsigned)t, (unsigned)k);
        }
}

/* (G2-EX, pixel-changing) the host holds one pose: its picture changes with its expression, not with the blink,
 * breath and aura frames (each re-rendered 6-8 ms of a GBA frame, every few ticks) */
TEST(exorcism_host_holds_one_pose)
{
    static uint32_t out[RITUAL_FRAMES];
    uint32_t seen[16];
    int f, clipped, n = 0, i;
    NatOptions o;
    StageStats st;
    (void)out; (void)st; (void)clipped;
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.seed = 0x5EED1234u; o.quiet = 1; o.save_dir = test_tmp_dir;
    nat_init(&o);
    nat_stage_on = 0;
    nat_stage_reset();
    test_reset();
    doomo_init();
    scene_set(&SCENE_TITLE, 0);
    rframe(0);
    scene_set(&SCENE_EXORCISM, -1);
    for (f = 0; f < 1300; f++) {                      /* the intro to the true name: the host's expression is the same */
        uint32_t u;
        rframe(pad(f));
        u = exo_probe_host_uid();
        for (i = 0; i < n && seen[i] != u; i++) {}
        if (i == n && n < 16) seen[n++] = u;
    }
    CHECK_MSG(n <= 3, "the host showed %d different pictures (a held pose is 1, a re-render at most 2 more)", n);
    set_res(240, 160);
}

/* (G2-EX landing) the ritual's first pictures are fetched as before, not held: a held pose with no picture of the look
 * to stand in was drawn at once and its sliced job pumped as well (+43 ms on the GBA's first exorcism frame). The
 * cache runs as on the GBA: stand-ins for 4 ticks, one layer group a tick, a fake clock; the slices are counted
 * (the native platform has no plat_portrait_slice_done of its own: this one only counts) */
static int slices_run;
void plat_portrait_slice_done(int cls) { (void)cls; slices_run++; }
TEST(exorcism_first_pictures_are_not_held)
{
    NatOptions o;
    int f, n[3];
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.seed = 0x5EED1234u; o.quiet = 1; o.save_dir = test_tmp_dir;
    nat_init(&o);
    nat_stage_on = 0;
    nat_stage_reset();
    test_reset();
    doomo_init();
    scene_set(&SCENE_TITLE, 0);
    rframe(0);
    portrait_cache_reset();
    portrait_cache_standin(4);
    scene_set(&SCENE_EXORCISM, -1);
    for (f = 0; f < 3; f++) {
        slices_run = 0;
        portrait_cache_clock(1000u + (uint32_t)f, 1);
        rframe(0);
        n[f] = slices_run;
    }
    CHECK(exo_probe_host_uid() != 0);
    CHECK_MSG(n[0] == 0, "%d portrait slices ran in the ritual's first frame (a held pose's job beside the render)", n[0]);
    CHECK_MSG(n[1] + n[2] > 0, "the held pose's job did not start after the first frame");
    portrait_cache_reset();
    portrait_cache_clock(0, 0);
    set_res(240, 160);
}
