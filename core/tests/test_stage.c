/* test_stage.c - the retained stage (SP7, backdrop.h): the exorcism drawn as layers over a
 * kept backdrop must be the picture it draws in full, frame for frame. The native platform
 * keeps the backdrop when nat_stage_on is set and emulates the GBA's two Mode 4 pages (a frame
 * starts from the picture of two frames ago), so a box the stage fails to put back, a layer it
 * fails to redraw or a layer key that misses a change shows up as a differing frame. The whole
 * practice ritual (intro dialogue, tools, prayer, holy water, true name, verdict, result) at
 * 240x160 and 320x240; the stage must also really keep frames (most are not drawn in full).
 * Also: the baked keys cover every key the scene can ask for. */
#include "test.h"
#include "doomo/backdrop.h"
#include "../src/exorcism_int.h"

#define MAXF 1400

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static uint32_t fb_hash(void)
{
    uint32_t h = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

static void set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

/* core/tests/scripts/life_exorcism.txt as a pad: the dialogue, the tools, the prayer (a few
 * presses on the beat), the holy water (aim, throw), the true name, the verdict, the result */
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
    if (f >= 550 && f < 570) return BTN_UP;                 /* aim up a little */
    for (i = 0; i < sizeof(p) / sizeof(p[0]); i++)
        if (p[i].at == f) return p[i].b;
    return 0;
}

static int retained_n, full_n;

static void run(int n, uint32_t *out, int stage)
{
    int f;
    StageStats st;
    nat_stage_on = stage;
    nat_stage_buffers = 2;
    nat_stage_reset();
    test_reset();
    doomo_init();
    scene_set(&SCENE_TITLE, 0);
    frame(0);
    scene_set(&SCENE_EXORCISM, -1);
    retained_n = full_n = 0;
    for (f = 0; f < n; f++) {
        frame(pad(f));
        out[f] = fb_hash();
        stage_stats(&st);
        if (st.retained) { retained_n++; if (st.full) full_n++; }
    }
    nat_stage_on = 0;
}

static int compare(int w, int h, int n)
{
    static uint32_t off[MAXF], on[MAXF];
    int f;
    set_res(w, h);
    run(n, off, 0);
    run(n, on, 1);
    for (f = 0; f < n; f++)
        if (off[f] != on[f]) {
            static uint8_t a[NAT_MAX_W * NAT_MAX_H];
            char m[300], path[600];
            int i, x0 = w, y0 = h, x1 = -1, y1 = -1, nd = 0;
            run(f + 1, off, 0);
            memcpy(a, nat_fb, (size_t)(w * h));
            snprintf(path, sizeof(path), "%s/stage_%dx%d_off.png", test_tmp_dir, w, h);
            nat_png_write(path, nat_fb, w, h, 2);
            run(f + 1, on, 1);
            snprintf(path, sizeof(path), "%s/stage_%dx%d_on.png", test_tmp_dir, w, h);
            nat_png_write(path, nat_fb, w, h, 2);
            for (i = 0; i < w * h; i++)
                if (a[i] != nat_fb[i]) {
                    nd++;
                    x0 = DOOMO_MIN(x0, i % w); x1 = DOOMO_MAX(x1, i % w);
                    y0 = DOOMO_MIN(y0, i / w); y1 = DOOMO_MAX(y1, i / w);
                }
            snprintf(m, sizeof(m), "exorcism %dx%d: frame %d differs with the stage on (%d px in %d,%d..%d,%d)",
                     w, h, f, nd, x0, y0, x1, y1);
            test_fail(__FILE__, __LINE__, m);
            return 0;
        }
    printf("      stage exorcism %dx%d: %d frames, %d retained, %d of them drawn in full\n", w, h, n,
           retained_n, full_n);
    if (retained_n < n * 9 / 10 || full_n * 4 > retained_n) {
        char m[200];
        snprintf(m, sizeof(m), "exorcism %dx%d: the stage kept too little (%d retained, %d full of %d)",
                 w, h, retained_n, full_n, n);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    return 1;
}

TEST(stage_exorcism_layers_are_pixel_identical)
{
    int ok = compare(240, 160, MAXF) && compare(320, 240, MAXF);
    set_res(240, 160);
    CHECK(ok);
}

/* every key exo_church_key can return for any dread and tick is in the baked list */
TEST(stage_church_keys_cover_every_frame)
{
    static uint32_t keys[512];
    int n = exo_church_keys(keys, 512), d, i, j;
    uint32_t t;
    CHECK(n > 0 && n <= 512);
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) CHECK(keys[i] != keys[j]);
    for (d = -2; d < 18; d++)
        for (t = 0; t < 600; t++) {
            uint32_t k = exo_church_key(d, t);
            for (i = 0; i < n && keys[i] != k; i++) {}
            CHECK_MSG(i < n, "dread %d tick %u: key 0x%x is not baked", d, (unsigned)t, (unsigned)k);
        }
    CHECK(backdrop_family_count() >= 1 && backdrop_family(0) == &exo_church_family);
}

/* ---- (S2-4) the Fun Fair booths over their kept playfields ----------------------------------
 * Each game with a kept playfield (mg.h) played by its bot (Sweet, Hellfire, Spicy at 320x240: the
 * READY / GO, the round, the END banner, the results; a pause on the way) with the kept picture on
 * and off: every frame the same, and most frames really drawn over the kept picture. */
#include "../src/mg.h"
extern const DoomoScene SCENE_MG_CATCH, SCENE_MG_WHACK, SCENE_MG_DODGE, SCENE_MG_GALLERY,
       SCENE_MG_DERBY, SCENE_MG_MEMORY;

static const DoomoScene *mg_scene_of(int id)
{
    switch (id) {
    case MG_CATCH: return &SCENE_MG_CATCH;
    case MG_WHACK: return &SCENE_MG_WHACK;
    case MG_DODGE: return &SCENE_MG_DODGE;
    case MG_GALLERY: return &SCENE_MG_GALLERY;
    case MG_DERBY: return &SCENE_MG_DERBY;
    default: return &SCENE_MG_MEMORY;
    }
}

static uint32_t mg_pad(int f)
{
    if (f == 700 || f == 706) return BTN_START;          /* pause, and back */
    return 0;
}

static void mg_run(const DoomoScene *sc, int arg, int n, uint32_t *out, int stage)
{
    int f;
    nat_stage_on = stage;
    nat_stage_buffers = 2;
    nat_stage_reset();
    test_reset();
    doomo_init();
    scene_set(&SCENE_TITLE, 0);
    frame(0);
    scene_push(sc, arg);
    retained_n = 0;
    for (f = 0; f < n; f++) {
        frame(mg_pad(f));
        out[f] = fb_hash();
        if (nat_stage_kept()) retained_n++;
    }
    nat_stage_on = 0;
}

static int mg_compare(int id, int arg, int w, int h, int n)
{
    static uint32_t off[MAXF], on[MAXF];
    const DoomoScene *sc = mg_scene_of(id);
    int f;
    set_res(w, h);
    mg_run(sc, arg, n, off, 0);
    mg_run(sc, arg, n, on, 1);
    for (f = 0; f < n; f++)
        if (off[f] != on[f]) {
            static uint8_t a[NAT_MAX_W * NAT_MAX_H];
            char m[300], path[600];
            int i, x0 = w, y0 = h, x1 = -1, y1 = -1, nd = 0;
            mg_run(sc, arg, f + 1, off, 0);
            memcpy(a, nat_fb, (size_t)(w * h));
            snprintf(path, sizeof(path), "%s/stage_mg%d_%dx%d_off.png", test_tmp_dir, id, w, h);
            nat_png_write(path, nat_fb, w, h, 2);
            mg_run(sc, arg, f + 1, on, 1);
            snprintf(path, sizeof(path), "%s/stage_mg%d_%dx%d_on.png", test_tmp_dir, id, w, h);
            nat_png_write(path, nat_fb, w, h, 2);
            for (i = 0; i < w * h; i++)
                if (a[i] != nat_fb[i]) {
                    nd++;
                    x0 = DOOMO_MIN(x0, i % w); x1 = DOOMO_MAX(x1, i % w);
                    y0 = DOOMO_MIN(y0, i / w); y1 = DOOMO_MAX(y1, i / w);
                }
            snprintf(m, sizeof(m), "minigame %d arg %d %dx%d: frame %d differs with the stage on (%d px in %d,%d..%d,%d)",
                     id, arg, w, h, f, nd, x0, y0, x1, y1);
            test_fail(__FILE__, __LINE__, m);
            return 0;
        }
    printf("      kept minigame %d arg %d %dx%d: %d frames, %d over the kept picture\n", id, arg, w, h, n, retained_n);
    if (retained_n < n / 2) {
        char m[200];
        snprintf(m, sizeof(m), "minigame %d %dx%d: too few frames over the kept picture (%d of %d)",
                 id, w, h, retained_n, n);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    return 1;
}

static const uint8_t kept_games[] = { MG_WHACK };

TEST(stage_kept_minigames_are_pixel_identical)
{
    unsigned i;
    int ok = 1;
    for (i = 0; i < sizeof(kept_games) && ok; i++) {
        int id = kept_games[i];
        ok = mg_compare(id, 49, 240, 160, MAXF) && mg_compare(id, 51, 240, 160, MAXF) &&
             mg_compare(id, 50, 320, 240, 900);
    }
    set_res(240, 160);
    CHECK(ok);
}

/* every key a staged game asks for is in the baked list */
TEST(stage_kept_minigame_keys_are_baked)
{
    static uint32_t keys[512];
    const DoomoBackdrop *f = backdrop_family(1);
    unsigned i;
    int n, j, t;
    CHECK(f && f->id == DOOMO_BACKDROP_MG);
    n = f->keys(keys, 512);
    CHECK(n > 0 && n <= 512);
    for (i = 0; i < sizeof(kept_games); i++) {
        const MgGame *g = mg_games[kept_games[i]];
        CHECK(g->kept_key != NULL);
        for (t = 0; t < 3000; t++) {
            uint32_t k;
            mg.g = g; mg.t = 0; mg.play_t = (uint16_t)t;
            k = (uint32_t)g->id << 16 | (g->kept_key() & 0x7FFFu);
            for (j = 0; j < n && keys[j] != k; j++) {}
            CHECK_MSG(j < n, "game %d tick %d: key 0x%x is not baked", g->id, t, (unsigned)k);
            for (j = 0; j < n && keys[j] != (k | MG_BK_HELL); j++) {}
            CHECK_MSG(j < n, "game %d tick %d: hell key 0x%x is not baked", g->id, t, (unsigned)k);
        }
    }
    memset(&mg, 0, sizeof(mg));
}
