/* test_cook_kept.c - (S2-4c) the kept kitchen (cook_scene.c, cook_gfx.c; docs/GBA.md "Speed
 * wave 2: S2-4c"). The kitchen's moving parts are layers of the retained stage (backdrop.h):
 * each one's box must hold every pixel it paints, and its key must change whenever its picture
 * would. Here every part is painted alone, at many ticks, on two different backgrounds: a pixel
 * that differs from the background in either run is painted, and must lie inside the box; two
 * ticks with the same key must paint the same picture. */
#include "test.h"
#include "../src/fishcook_int.h"

static void set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

typedef void (*PartFn)(int t, int a, int b);

static uint8_t pa[NAT_MAX_W * NAT_MAX_H], pb[NAT_MAX_W * NAT_MAX_H];

/* paint part(t, a, b) on a screen cleared to `bg` */
static void paint(PartFn fn, int t, int a, int b, uint8_t bg, uint8_t *out)
{
    memset(nat_fb, bg, (size_t)nat_w * (size_t)nat_h);
    fn(t, a, b);
    memcpy(out, nat_fb, (size_t)nat_w * (size_t)nat_h);
}

/* 1 when every painted pixel of part(t, a, b) is inside box (x, y, w, h); *hash: its picture */
static int inside(PartFn fn, int t, int a, int b, const int box[4], uint32_t *hash, char *why, int whylen)
{
    int i, n = nat_w * nat_h;
    uint32_t h = 2166136261u;
    paint(fn, t, a, b, PAL_RAMP(RAMP_STONE, 2), pa);
    paint(fn, t, a, b, PAL_RAMP(RAMP_SKY, 5), pb);
    for (i = 0; i < n; i++) {
        int x = i % nat_w, y = i / nat_w;
        int painted = pa[i] != PAL_RAMP(RAMP_STONE, 2) || pb[i] != PAL_RAMP(RAMP_SKY, 5);
        h = (h ^ (painted ? pb[i] : 0xFFu)) * 16777619u;
        if (painted && (x < box[0] || y < box[1] || x >= box[0] + box[2] || y >= box[1] + box[3])) {
            snprintf(why, (size_t)whylen, "pixel %d,%d outside the box %d,%d %dx%d", x, y, box[0], box[1], box[2], box[3]);
            return 0;
        }
    }
    *hash = h;
    return 1;
}

/* a small table of (key -> picture) seen so far: the same key must mean the same picture */
#define KMAX 512
static uint32_t kk[KMAX], kh[KMAX];
static int kn;
static int key_agrees(uint32_t key, uint32_t hash)
{
    int i;
    for (i = 0; i < kn; i++)
        if (kk[i] == key) return kh[i] == hash;
    if (kn < KMAX) { kk[kn] = key; kh[kn] = hash; kn++; }
    return 1;
}

static void part_granny(int t, int mood, int s) { granny_draw(nat_w / 2, nat_h / 2 + 20, s, t, mood); }

TEST(cook_kept_granny_box_and_key)
{
    static const int sizes[] = { 6, 8, 9, 12, 14, 18 };
    int si, mood, t, b[4];
    char why[160];
    set_res(320, 240);
    for (si = 0; si < (int)(sizeof(sizes) / sizeof(sizes[0])); si++)
        for (mood = GRAN_CALM; mood <= GRAN_DISMAY; mood++) {
            kn = 0;
            granny_box(nat_w / 2, nat_h / 2 + 20, sizes[si], b);
            for (t = 0; t < 400; t += (t < 130 ? 1 : 3)) {
                uint32_t h;
                CHECK_MSG(inside(part_granny, t, mood, sizes[si], b, &h, why, sizeof(why)),
                          "granny s %d mood %d t %d: %s", sizes[si], mood, t, why);
                CHECK_MSG(key_agrees(granny_key(sizes[si], t, mood), h),
                          "granny s %d mood %d t %d: same key, another picture", sizes[si], mood, t);
            }
        }
    set_res(240, 160);
}

static void part_dish(int t, int r, int rw) { dish_draw(r & 255, nat_w / 2, nat_h / 2, rw, t, r >> 8); }

TEST(cook_kept_dish_box)
{
    static const int sizes[] = { 5, 6, 8, 12, 17, 22, 28, 30 };
    int r, burnt, si, t, b[4];
    char why[160];
    set_res(320, 240);
    for (r = 0; r < RECIPE_COUNT; r++)
        for (burnt = 0; burnt <= 1; burnt++)
            for (si = 0; si < (int)(sizeof(sizes) / sizeof(sizes[0])); si++) {
                dish_box(r, nat_w / 2, nat_h / 2, sizes[si], burnt, b);
                for (t = 0; t < 240; t += 1 + (t >= 96)) {
                    uint32_t h;
                    CHECK_MSG(inside(part_dish, t, r | burnt << 8, sizes[si], b, &h, why, sizeof(why)),
                              "dish %d burnt %d rw %d t %d: %s", r, burnt, sizes[si], t, why);
                }
            }
    set_res(240, 160);
}

static const CookIng *ing_cur;
static void part_ing(int t, int rw, int unused) { (void)unused; ingredient_draw(ing_cur, nat_w / 2, nat_h / 2, rw, t); }

/* every ingredient the book can show (each item of a recipe, every fish and the fish kinds) */
TEST(cook_kept_ingredient_box_and_key)
{
    static const int sizes[] = { 5, 8 };
    CookIng g;
    int kind, id, si, t, b[4];
    char why[160];
    set_res(320, 240);
    for (kind = ING_ITEM; kind <= ING_ANY_FISH; kind++)
        for (id = 0; id < (kind == ING_ITEM ? ITEM_COUNT : kind == ING_FISH ? FISH_COUNT : 1); id++) {
            int used = kind != ING_ITEM, r, k;
            for (r = 0; r < RECIPE_COUNT && !used; r++)
                for (k = 0; k < 3; k++)
                    if (cook_recipes[r].ing[k].kind == ING_ITEM && cook_recipes[r].ing[k].id == id) used = 1;
            if (!used) continue;
            g.kind = (uint8_t)kind; g.id = (uint8_t)id; g.qty = 1;
            ing_cur = &g;
            for (si = 0; si < 2; si++) {
                kn = 0;
                ingredient_box(&g, nat_w / 2, nat_h / 2, sizes[si], b);
                for (t = 0; t < 200; t++) {
                    uint32_t h;
                    CHECK_MSG(inside(part_ing, t, sizes[si], 0, b, &h, why, sizeof(why)), "ingredient kind %d id %d rw %d t %d: %s",
                              kind, id, sizes[si], t, why);
                    CHECK_MSG(key_agrees(ingredient_key(&g, t), h), "ingredient kind %d id %d t %d: same key, another picture",
                              kind, id, t);
                }
            }
        }
    set_res(240, 160);
}

static void part_window(int t, int night, int unused) { (void)unused; kitchen_window((uint32_t)t, night); }
static void part_pans(int t, int a, int b) { (void)a; (void)b; kitchen_draw_back((uint32_t)t, 0); }
static void part_back_busy(int t, int a, int b) { (void)a; (void)b; kitchen_draw_back((uint32_t)t, 1); }
static void part_logo(int t, int y, int unused) { (void)unused; kitchen_counter_logo(y, (uint32_t)t); }

/* the window, the pans and the logo at both screen sizes: inside their boxes, keyed; and the back
 * is the still part plus the window plus the pans (the pans only add pixels inside their box) */
TEST(cook_kept_back_parts_box_and_key)
{
    static const int res[2][2] = { { 240, 160 }, { 320, 240 } };
    int ri, night, t, b[4];
    char why[160];
    for (ri = 0; ri < 2; ri++) {
        int y = res[ri][1] * 64 / 100;
        set_res(res[ri][0], res[ri][1]);
        for (night = 0; night <= 1; night++) {
            kn = 0;
            kitchen_window_box(b);
            for (t = 0; t < 600; t++) {
                uint32_t h;
                CHECK_MSG(inside(part_window, t, night, 0, b, &h, why, sizeof(why)), "window %dx%d night %d t %d: %s",
                          nat_w, nat_h, night, t, why);
                CHECK_MSG(key_agrees(kitchen_window_key((uint32_t)t, night), h), "window night %d t %d: same key, another picture",
                          night, t);
            }
        }
        /* the pans: the busy back and the back with its pans differ only inside the pans' box */
        kitchen_pans_box(b);
        for (t = 0; t < 240; t += 2) {
            int i, n = nat_w * nat_h;
            paint(part_back_busy, t, 0, 0, 0, pa);
            paint(part_pans, t, 0, 0, 0, pb);
            for (i = 0; i < n; i++) {
                int x = i % nat_w, yy = i / nat_w;
                CHECK_MSG(pa[i] == pb[i] || (x >= b[0] && yy >= b[1] && x < b[0] + b[2] && yy < b[1] + b[3]),
                          "pans %dx%d t %d: pixel %d,%d outside the box", nat_w, nat_h, t, x, yy);
            }
        }
        /* the logo at the cooking counter and the book's */
        {
            int yi;
            for (yi = 0; yi < 2; yi++) {
                int cy = y + yi * 10;
                kn = 0;
                kitchen_logo_box(cy, b);
                for (t = 0; t < 60; t++) {
                    uint32_t h;
                    CHECK_MSG(inside(part_logo, t, cy, 0, b, &h, why, sizeof(why)), "logo %dx%d t %d: %s", nat_w, nat_h, t, why);
                    CHECK_MSG(key_agrees(kitchen_logo_key((uint32_t)t), h), "logo t %d: same key, another picture", t);
                }
            }
        }
    }
    set_res(240, 160);
}

/* ---- the whole kitchen with the stage on and off -------------------------------------------
 * The native platform keeps the backdrop when nat_stage_on is set and emulates the GBA's two
 * Mode 4 pages (a frame starts from the picture of two frames ago), so a box the stage fails to
 * put back, a layer it fails to redraw or a key that misses a change shows up as a differing
 * frame. One tour: the recipe book (every page, a jump, the counter and back), the buy popup,
 * a dish cooked by hand (late on every step: misses, a burnt bake, the result's buttons), the
 * bot cooking a bake, a fry, a simmer, a scorch and a second simmer, then the counter (cursor,
 * the dish list, serving, the reaction). Day and night, 240x160 and 320x240. */
#include "doomo/backdrop.h"
#define KF_MAX 6400

static void kframe(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static uint32_t kfb_hash(void)
{
    uint32_t h = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

typedef struct { int16_t at; uint16_t b; } KPad;
typedef struct { int arg, frames, unlock; const KPad *pad; } KSeg;

static const KPad pad_book[] = {
    { 40, BTN_RIGHT }, { 54, BTN_RIGHT }, { 68, BTN_RIGHT }, { 82, BTN_RIGHT }, { 96, BTN_RIGHT },
    { 110, BTN_RIGHT }, { 124, BTN_RIGHT }, { 138, BTN_RIGHT }, { 152, BTN_RIGHT }, { 166, BTN_RIGHT },
    { 180, BTN_RIGHT }, { 194, BTN_RIGHT }, { 208, BTN_RIGHT }, { 222, BTN_RIGHT }, { 236, BTN_RIGHT },
    { 250, BTN_RIGHT }, { 264, BTN_RIGHT }, { 278, BTN_RIGHT }, { 292, BTN_RIGHT }, { 306, BTN_RIGHT },
    { 320, BTN_RIGHT }, { 334, BTN_RIGHT }, { 348, BTN_R }, { 362, BTN_L }, { 376, BTN_LEFT },
    { 400, BTN_START }, { 460, BTN_RIGHT }, { 480, BTN_B }, { -1, 0 }
};
static const KPad pad_buy[] = {
    { 30, BTN_A }, { 70, BTN_B }, { 90, BTN_A }, { 110, BTN_A }, { 900, BTN_RIGHT }, { 920, BTN_RIGHT },
    { 940, BTN_LEFT }, { 960, BTN_B }, { -1, 0 }
};
static const KPad pad_counter[] = {
    { 60, BTN_RIGHT }, { 72, BTN_RIGHT }, { 84, BTN_LEFT }, { 96, BTN_A }, { 116, BTN_DOWN }, { 124, BTN_DOWN },
    { 132, BTN_UP }, { 140, BTN_A }, { 180, BTN_A }, { 210, BTN_RIGHT }, { 222, BTN_A }, { 242, BTN_A },
    { 342, BTN_A }, { 362, BTN_LEFT }, { 380, BTN_A }, { 392, BTN_B }, { -1, 0 }
};
static const KSeg ksegs[] = {
    { COOK_ARG_STOCK, 520, 0, pad_book },
    { 0, 1000, 0, pad_buy },
    { COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(0), 640, 0, NULL },     /* bake   */
    { COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(2), 640, 0, NULL },     /* fry    */
    { COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(1), 640, 0, NULL },     /* simmer */
    { COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(18), 640, 1, NULL },    /* scorch */
    { COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(4), 640, 0, NULL },     /* simmer */
    { COOK_ARG_COUNTER | COOK_ARG_DEMO, 420, 0, pad_counter },
};

static uint32_t kpad(const KPad *p, int f)
{
    for (; p && p->at >= 0; p++)
        if (p->at == f) return p->b;
    return 0;
}

static int kept_frames, full_frames;

static int krun(uint32_t *out, int stage, int night, int upto)
{
    int s, f, n = 0;
    nat_stage_on = stage;
    nat_stage_buffers = 2;
    nat_stage_reset();
    test_reset();
    doomo_init();
    portrait_cache_reset();             /* (the counter's faces: both runs start with none) */
    scene_set(&SCENE_TITLE, 0);
    kframe(0);
    kept_frames = full_frames = 0;
    for (s = 0; s < (int)(sizeof(ksegs) / sizeof(ksegs[0])) && n < upto && n < KF_MAX; s++) {
        const KSeg *g = &ksegs[s];
        if (g->unlock) {
            int r;
            for (r = 0; r < RECIPE_COUNT; r++)
                if (cook_recipes[r].unlock != ACT_ALWAYS) unlock_grant(cook_recipes[r].unlock);
            ui_toast_clear();
        }
        doomo_town.minute = (uint16_t)(night ? 22 * 60 : 12 * 60);
        if (s == 0) scene_push(&SCENE_COOKING, g->arg);
        else scene_set(&SCENE_COOKING, g->arg);
        for (f = 0; f < g->frames && n < KF_MAX && n < upto; f++) {
            kframe(kpad(g->pad, f));
            out[n++] = kfb_hash();
            if (nat_stage_kept()) {
                StageStats st;
                stage_stats(&st);
                kept_frames++;
                full_frames += st.full;
            }
        }
    }
    nat_stage_on = 0;
    return n;
}

static int kcompare(int w, int h, int night, int *kept)
{
    static uint32_t off[KF_MAX], on[KF_MAX];
    int n, f;
    set_res(w, h);
    n = krun(off, 0, night, KF_MAX);
    krun(on, 1, night, KF_MAX);
    *kept = kept_frames;
    for (f = 0; f < n; f++)
        if (off[f] != on[f]) {
            static uint8_t a[NAT_MAX_W * NAT_MAX_H];
            char m[300], path[600];
            int i, x0 = w, y0 = h, x1 = -1, y1 = -1, nd = 0;
            krun(off, 0, night, f + 1);
            memcpy(a, nat_fb, (size_t)(w * h));
            snprintf(path, sizeof(path), "%s/cook_kept_%dx%d_off.png", test_tmp_dir, w, h);
            nat_png_write(path, nat_fb, w, h, 2);
            krun(on, 1, night, f + 1);
            snprintf(path, sizeof(path), "%s/cook_kept_%dx%d_on.png", test_tmp_dir, w, h);
            nat_png_write(path, nat_fb, w, h, 2);
            for (i = 0; i < w * h; i++)
                if (a[i] != nat_fb[i]) {
                    nd++;
                    x0 = DOOMO_MIN(x0, i % w); x1 = DOOMO_MAX(x1, i % w);
                    y0 = DOOMO_MIN(y0, i / w); y1 = DOOMO_MAX(y1, i / w);
                }
            snprintf(m, sizeof(m), "kitchen %dx%d night %d: frame %d (state %d) differs with the stage on (%d px in %d,%d..%d,%d)",
                     w, h, night, f, cook_scene_state(), nd, x0, y0, x1, y1);
            test_fail(__FILE__, __LINE__, m);
            return 0;
        }
    printf("      kept kitchen %dx%d night %d: %d frames, %d over the kept picture, %d of them drawn in full\n", w, h, night, n,
           *kept, full_frames);
    if (full_frames * 8 > *kept) {
        test_fail(__FILE__, __LINE__, "the stage drew too many kitchen frames in full");
        return 0;
    }
    return n;
}

TEST(cook_kept_kitchen_is_pixel_identical)
{
    int kept, n, asked;
    CHECK(stage_mem_bytes() <= 2048u);
    n = kcompare(240, 160, 0, &kept);
    CHECK(n > 0);
    /* every pose of Granny the tour drew from an image is one the GBA bakes (and some were) */
    CHECK_MSG(nat_cook_pose_unlisted(&asked) == 0, "%d of %d poses are not in cook_poses()", nat_cook_pose_unlisted(&asked), asked);
    CHECK_MSG(asked >= 40, "only %d poses drawn from images", asked);
    CHECK_MSG(kept > n / 3, "only %d of %d frames over the kept picture", kept, n);
    CHECK(kcompare(240, 160, 1, &kept) > 0);
    CHECK(kcompare(320, 240, 0, &kept) > 0);
    CHECK(kcompare(320, 240, 1, &kept) > 0);
    set_res(240, 160);
}

/* Granny's pose is periodic: every t >= 0 shows a pose of the first cycle (the baker's range) */
TEST(cook_kept_granny_poses_cycle)
{
    int mood, t;
    for (mood = GRAN_CALM; mood <= GRAN_DISMAY; mood++)
        for (t = 0; t < 3 * GRANNY_POSE_CYCLE; t++)
            CHECK_MSG(granny_key(14, t, mood) == granny_key(14, t % GRANNY_POSE_CYCLE, mood), "mood %d t %d", mood, t);
}
