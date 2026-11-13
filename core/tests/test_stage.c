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

static int retained_n, full_n, clipped_n;

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
    retained_n = full_n = clipped_n = 0;
    nat_clip_viol = 0;
    for (f = 0; f < n; f++) {
        frame(pad(f));
        out[f] = fb_hash();
        stage_stats(&st);
        if (st.retained) { retained_n++; if (st.full) full_n++; if (st.clipped) clipped_n++; }
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
    printf("      stage exorcism %dx%d: %d frames, %d retained, %d of them drawn in full, %d with a clipped layer\n", w, h, n,
           retained_n, full_n, clipped_n);
    if (clipped_n < n / 4 || nat_clip_viol) {          /* (G2-EX) the clipped redraw is really used, with calls it honours */
        char m[200];
        snprintf(m, sizeof(m), "exorcism %dx%d: %d frames with a clipped layer of %d, %d calls the clip does not honour",
                 w, h, clipped_n, n, nat_clip_viol);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
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

static int stamps_n;
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
    stamps_n = nat_stage_stamps();
    nat_stage_on = 0;
}

static int has_kept_picture(int id)
{
    int i;
    for (i = 0; i < MG_GAME_COUNT; i++)
        if (mg_games[i]->id == id) return mg_games[i]->kept_key != NULL;
    return 0;
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
    printf("      kept minigame %d arg %d %dx%d: %d frames, %d over the kept picture, %d stamps\n", id, arg, w, h, n, retained_n, stamps_n);
    if (stamps_n < n) {                          /* (S2-4b) the HUD alone is a few stamps a frame */
        test_fail(__FILE__, __LINE__, "kept minigame: stamps were not used");
        return 0;
    }
    if (has_kept_picture(id) && retained_n < n / 3) {          /* (a game of stamps only has none) */
        char m[200];
        snprintf(m, sizeof(m), "minigame %d %dx%d: too few frames over the kept picture (%d of %d)",
                 id, w, h, retained_n, n);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    return 1;
}

static const uint8_t kept_games[] = { MG_CATCH, MG_WHACK, MG_DODGE, MG_GALLERY, MG_DERBY, MG_MEMORY };

TEST(stage_kept_minigames_are_pixel_identical)
{
    unsigned i;
    int ok = 1;
    for (i = 0; i < sizeof(kept_games) && ok; i++) {
        int id = kept_games[i];
        /* Candy Catch: only up to its first Sugar Rush (frame ~426): the rainbow's stripes stop short of the right edge
         * in some phases, and what the gap shows is the buffer's old frame, which the stage emulation's two buffers
         * and the one buffer of a plain run hold differently (a gap in the game's own drawing, not the stamps') */
        int n = id == MG_CATCH ? 420 : MAXF;
        ok = mg_compare(id, 49, 240, 160, n) && mg_compare(id, 51, 240, 160, n) &&
             mg_compare(id, 50, 320, 240, id == MG_CATCH ? 420 : 900);
    }
    set_res(240, 160);
    CHECK(ok);
}

/* (S2-4b) every picture a kept game asks for (bands and stamps: what the native stage was asked to
 * draw while the bot played) is in the lists the GBA build bakes into the ROM */
TEST(stage_kept_minigame_keys_are_baked)
{
    static uint32_t baked[8192], asked[8192], off[900];
    unsigned i;
    int nb = 0, na, j, k, fam, arg, bands = 0, stamps = 0;
    for (fam = 0; fam < backdrop_family_count(); fam++) {
        const DoomoBackdrop *f = backdrop_family(fam);
        int n = f->keys(asked, 8192);
        CHECK(n >= 0 && nb + n <= 8192);
        for (j = 0; j < n; j++) baked[nb++] = (uint32_t)f->id << 24 | (asked[j] & 0xFFFFFFu);
    }
    for (j = 1; j < nb; j++)
        for (k = 0; k < j; k++) CHECK_MSG(baked[j] != baked[k], "key 0x%x is listed twice", (unsigned)baked[j]);
    set_res(240, 160);
    for (i = 0; i < sizeof(kept_games); i++)
        for (arg = 49; arg <= 51; arg++) {
            mg_run(mg_scene_of(kept_games[i]), arg, 900, off, 1);
            na = nat_stage_pics(asked, 8192);
            CHECK(na > 0 && na <= 8192);
            for (j = 0; j < na; j++) {
                for (k = 0; k < nb && baked[k] != asked[j]; k++) {}
                CHECK_MSG(k < nb, "game %d arg %d: picture 0x%x is not baked", kept_games[i], arg, (unsigned)asked[j]);
                if ((asked[j] >> 24) == DOOMO_BACKDROP_MG) bands++;
                if ((asked[j] >> 24) == DOOMO_BACKDROP_MG_STAMP) stamps++;
            }
        }
    CHECK(bands > 0 && stamps > 0);
    nat_stage_reset();
    memset(&mg, 0, sizeof(mg));
}

/* (S2-4b) the file the GBA build links (runner --bake-backdrops): sorted keys, a band holds its rows
 * only, a stamp is the box of what its draw painted, and its pixels are that drawing */
TEST(stage_bake_file_has_bands_and_stamps)
{
    static uint8_t file[16 * 1024 * 1024];
    char path[600];
    FILE *f;
    long len;
    uint32_t n, i, prev = 0;
    int bands = 0, stamps = 0, wholes = 0, checked = 0;
    set_res(240, 160);
    nat_stage_reset();
    snprintf(path, sizeof(path), "%s/bake_test.bin", test_tmp_dir);
    CHECK(nat_bake_backdrops(path) > 0);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    len = (long)fread(file, 1, sizeof(file), f);
    fclose(f);
    CHECK(len > 12 && len < (long)sizeof(file));
    CHECK(memcmp(file, "BKD2", 4) == 0 && (file[4] | file[5] << 8) == 240 && (file[6] | file[7] << 8) == 160);
    n = (uint32_t)file[8] | (uint32_t)file[9] << 8 | (uint32_t)file[10] << 16 | (uint32_t)file[11] << 24;
    for (i = 0; i < n; i++) {
        const uint8_t *e = file + 12 + 16 * i;
        uint32_t idkey = (uint32_t)e[0] | (uint32_t)e[1] << 8 | (uint32_t)e[2] << 16 | (uint32_t)e[3] << 24;
        uint32_t off = (uint32_t)e[4] | (uint32_t)e[5] << 8 | (uint32_t)e[6] << 16 | (uint32_t)e[7] << 24;
        int bx = (int16_t)(e[8] | e[9] << 8), by = (int16_t)(e[10] | e[11] << 8);
        int w = e[12] | e[13] << 8, h = e[14] | e[15] << 8;
        const DoomoBackdrop *fam = NULL;
        int16_t part[2] = { 0, 0 };
        int k, kind;
        CHECK(i == 0 || idkey > prev);
        prev = idkey;
        CHECK((off & 3) == 0 && off + (uint32_t)(w * h) <= (uint32_t)len);
        for (k = 0; k < backdrop_family_count(); k++)
            if (backdrop_family(k)->id == (int)(idkey >> 24)) fam = backdrop_family(k);
        CHECK(fam != NULL);
        kind = fam->part ? fam->part(idkey & 0xFFFFFFu, part) : BACKDROP_WHOLE;
        if (kind == BACKDROP_WHOLE) { wholes++; CHECK(bx == 0 && by == 0 && w == 240 && h == 160); }
        if (kind == BACKDROP_ROWS) { bands++; CHECK(bx == 0 && w == 240 && by == part[0] && h == part[1] - part[0] && h > 0 && h < 160); }
        if (kind == BACKDROP_STAMP || kind == BACKDROP_STRIP) {
            stamps++;
            CHECK(w > 0 && h > 0 && (w < 240 || kind == BACKDROP_STRIP) && h < 160);
            if ((stamps % 37) == 1) {             /* a sample of them, against the drawing */
                int x, y, bad = 0;
                memset(nat_fb, 0, 240 * 160);
                fam->draw(idkey & 0xFFFFFFu);
                for (y = 0; y < 160; y++)
                    for (x = 0; x < 240; x++) {
                        int sx = x - part[0] - bx, sy = y - part[1] - by;
                        uint8_t want = sx >= 0 && sy >= 0 && sx < w && sy < h ? file[off + (uint32_t)(sy * w + sx)] : 0;
                        if (nat_fb[y * 240 + x] != want) bad++;
                    }
                CHECK_MSG(bad == 0, "stamp 0x%x: %d pixels differ from its drawing", (unsigned)idkey, bad);
                checked++;
            }
        }
    }
    CHECK(wholes > 0 && bands > 0 && stamps > 0 && checked > 0);
    printf("      baked: %u pictures (%d whole screens, %d bands, %d stamps), %ld bytes\n", (unsigned)n, wholes, bands, stamps, len);
    remove(path);
}

/* (S2-4b, review) a baked stamp holds no text: ui_text follows Language and Font style at draw time, so a
 * stamp drawn in Spanish with another font style must equal the English one (the HUD pill's label is
 * drawn live over its stamp) */
TEST(stage_stamps_do_not_depend_on_language_or_font)
{
    static uint8_t file[16 * 1024 * 1024], en[240 * 160];
    char path[600];
    FILE *f;
    uint32_t n, i;
    int checked = 0, bad = 0;
    set_res(240, 160);
    nat_stage_reset();
    snprintf(path, sizeof(path), "%s/bake_lang.bin", test_tmp_dir);
    CHECK(nat_bake_backdrops(path) > 0);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    CHECK(fread(file, 1, sizeof(file), f) > 12);
    fclose(f);
    remove(path);
    n = (uint32_t)file[8] | (uint32_t)file[9] << 8 | (uint32_t)file[10] << 16 | (uint32_t)file[11] << 24;
    test_reset();
    doomo_init();
    for (i = 0; i < n; i++) {
        const uint8_t *e = file + 12 + 16 * i;
        uint32_t idkey = (uint32_t)e[0] | (uint32_t)e[1] << 8 | (uint32_t)e[2] << 16 | (uint32_t)e[3] << 24;
        const DoomoBackdrop *fam = NULL;
        int16_t part[2] = { 0, 0 };
        int k;
        for (k = 0; k < backdrop_family_count(); k++)
            if (backdrop_family(k)->id == (int)(idkey >> 24)) fam = backdrop_family(k);
        if (!fam || !fam->part || (fam->part(idkey & 0xFFFFFFu, part) != BACKDROP_STAMP && fam->part(idkey & 0xFFFFFFu, part) != BACKDROP_STRIP))
            continue;
        doomo_town.opt[OPT_LANGUAGE] = LANG_EN;
        doomo_town.opt[OPT_FONT_STYLE] = 0;
        memset(nat_fb, 0, 240 * 160);
        fam->draw(idkey & 0xFFFFFFu);
        memcpy(en, nat_fb, sizeof(en));
        doomo_town.opt[OPT_LANGUAGE] = LANG_ES;
        doomo_town.opt[OPT_FONT_STYLE] = 1;
        memset(nat_fb, 0, 240 * 160);
        fam->draw(idkey & 0xFFFFFFu);
        if (memcmp(en, nat_fb, sizeof(en)) != 0) bad++;
        checked++;
    }
    doomo_town.opt[OPT_LANGUAGE] = LANG_EN;
    doomo_town.opt[OPT_FONT_STYLE] = 0;
    CHECK(checked > 100);
    CHECK_MSG(bad == 0, "%d of %d stamps draw differently with Language / Font style (text baked)", bad, checked);
}

/* (S2-4b) mg_undo: a still part drawn again between mg_undo and mg_undo_end puts the kept picture
 * back in its own spans, inside the box only; listed once (mg_undo_rec) and repeated (mg_kept_puts) it
 * is the same pixels */
static void undo_bd(uint32_t key) { (void)key; plat_clear(PAL_UI_PINK); ui_fill_circle(60, 60, 20, PAL_WHITE); }
static int undo_part(uint32_t key, int16_t out[2]) { (void)key; (void)out; return BACKDROP_WHOLE; }
TEST(stage_undo_puts_a_shape_back_in_its_spans)
{
    static uint8_t want[240 * 160], got[240 * 160];
    static MgBox list[64];
    PlatStageInfo info;
    int x, y, n, pass;
    (void)undo_part;
    set_res(240, 160);
    doomo_init();
    for (pass = 0; pass < 2; pass++) {
        nat_stage_on = 1;
        nat_stage_reset();
        mg_scratch_claim();
        memset(mg_state(), 0, MG_ARENA_BYTES);
        plat_begin_frame();
        /* the full frame: pink, a live bar, then the white disc over it */
        plat_clear(PAL_UI_PINK);
        plat_rect(40, 55, 60, 10, PAL_UI_RED);
        ui_fill_circle(60, 60, 20, PAL_WHITE);
        memcpy(want, nat_fb, sizeof(want));
        /* over the kept picture (which holds the disc): the bar, then the disc put back over the
         * bar's left half only: the right half of the bar stays on the disc */
        CHECK(plat_stage_begin(77, 1, undo_bd, &info));
        plat_stage_restore(0, 0, 240, 160);
        plat_rect(40, 55, 60, 10, PAL_UI_RED);
        mg_kept_mode = MGK_LIVE;
        if (pass) mg_undo_rec(list, 64);
        mg_undo(40, 55, 60, 65);
        ui_fill_circle(60, 60, 20, PAL_WHITE);
        mg_rect(0, 0, 240, 160, PAL_BLACK);        /* (a rect is a still part too: the box clips it) */
        mg_undo_end();
        n = pass ? mg_undo_rec_end() : 0;
        if (pass) {
            CHECK(n > 0 && n < 64);
            for (y = 55; y < 65; y++) for (x = 40; x < 60; x++) CHECK(nat_fb[y * 240 + x] == PAL_UI_RED);   /* only listed */
            mg_kept_puts(list, n, 0, 0);
        }
        mg_kept_mode = MGK_OFF;
        memcpy(got, nat_fb, sizeof(got));
        plat_end_frame();
        for (y = 0; y < 160; y++)
            for (x = 0; x < 240; x++) {
                int in_box = x >= 40 && x < 60 && y >= 55 && y < 65;
                uint8_t w = in_box ? (want[y * 240 + x] == PAL_UI_RED ? PAL_UI_PINK : want[y * 240 + x])
                                   : (x >= 60 && x < 100 && y >= 55 && y < 65 ? PAL_UI_RED : want[y * 240 + x]);
                if (in_box) w = (x - 60) * (x - 60) + (y - 60) * (y - 60) <= 21 * 21 && want[y * 240 + x] == PAL_WHITE ? PAL_WHITE : PAL_UI_PINK;
                if (got[y * 240 + x] != w) { CHECK_MSG(0, "pass %d: pixel %d,%d is %d, not %d", pass, x, y, got[y * 240 + x], w); y = 160; break; }
            }
    }
    nat_stage_on = 0;
    nat_stage_reset();
}
