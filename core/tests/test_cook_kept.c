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
