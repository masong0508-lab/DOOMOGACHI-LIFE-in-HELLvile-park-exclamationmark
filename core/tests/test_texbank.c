/* test_texbank.c - LV7a: the per-level texture bank (doomo/texbank.h): the format and its refusals,
 * the texture_pixels() hook for the ids 56..63, and the bank drawn by the raycaster at 240x160 and
 * 320x240. The sample is core/data/texbanks/sample.bank (core/gen/tb_sample.c, tools/texbank.py). */
#include <stdlib.h>
#include "test.h"
#include "doomo/texbank.h"

#define SAMPLE_LEN 16444u        /* 12 + 3 * 16 + 4 * 4096 */

static uint8_t buf[SAMPLE_LEN + 8];

static void fresh(void) { memcpy(buf, doomo_texbank_sample, SAMPLE_LEN); }

TEST(texbank_sample_opens)
{
    DoomoTexBank b;
    CHECK_EQ(doomo_texbank_sample_len, SAMPLE_LEN);
    CHECK_EQ(texbank_open(&b, doomo_texbank_sample, SAMPLE_LEN), TB_OK);
    CHECK_EQ(b.count, 3);
    CHECK_STR(texbank_name(&b, 0), "vat_brick");
    CHECK_STR(texbank_name(&b, 2), "dust");
    CHECK_EQ(texbank_slot(&b, "ink_stripe"), 1);
    CHECK_EQ(texbank_slot(&b, "nope"), -1);
    CHECK_EQ(texbank_frames(&b, 0), 1);
    CHECK_EQ(texbank_frames(&b, 1), 2);
    CHECK_EQ(texbank_frames(&b, 7), 0);
    CHECK(texbank_avg(&b, 0) != 0);
    CHECK(texbank_pixels(&b, 3, 0) == NULL);
    CHECK(texbank_pixels(&b, 0, 0) != NULL);
    /* the pixels are the blob's own bytes after the header (pack-ready: no copy) */
    CHECK(texbank_pixels(&b, 0, 0) == doomo_texbank_sample + 12 + 48);
}

TEST(texbank_slots_animate_like_the_shared_textures)
{
    DoomoTexBank b;
    const uint8_t *f0, *f1;
    CHECK_EQ(texbank_open(&b, doomo_texbank_sample, SAMPLE_LEN), TB_OK);
    /* ink_stripe: 2 frames, pingpong, 12 ticks each */
    f0 = texbank_pixels(&b, 1, 0);
    f1 = texbank_pixels(&b, 1, 12);
    CHECK(f0 != f1);
    CHECK(memcmp(f0, f1, 64 * 64) != 0);
    CHECK(texbank_pixels(&b, 1, 11) == f0);
    CHECK(texbank_pixels(&b, 1, 24) == f0);
    CHECK(texbank_pixels(&b, 1, 36) == f1);
    /* a one-frame slot never moves */
    CHECK(texbank_pixels(&b, 0, 0) == texbank_pixels(&b, 0, 999));
}

TEST(texbank_refuses_a_bad_blob)
{
    DoomoTexBank b, keep;
    memset(&keep, 0x5A, sizeof keep);
    b = keep;
    CHECK_EQ(texbank_open(NULL, doomo_texbank_sample, SAMPLE_LEN), TB_ERR_NULL);
    CHECK_EQ(texbank_open(&b, NULL, SAMPLE_LEN), TB_ERR_NULL);
    CHECK_EQ(texbank_open(&b, doomo_texbank_sample, 11), TB_ERR_SHORT);
    CHECK_EQ(texbank_open(&b, doomo_texbank_sample, 40), TB_ERR_SHORT);      /* header fits, entries do not */
    fresh(); buf[0] = 'X';
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_MAGIC);
    fresh(); buf[4] = 2;
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_VERSION);
    fresh(); buf[6] = 1;
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_VERSION);
    fresh(); buf[5] = 0;
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_COUNT);
    fresh(); buf[5] = 9;
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_COUNT);
    fresh();                                                                  /* a truncated blob */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN - 1), TB_ERR_LENGTH);
    fresh(); buf[8]++;                                                        /* a lying length */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_LENGTH);
    fresh(); buf[12] = 'V';                                                   /* upper case in a name */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_NAME);
    fresh(); buf[12] = 0;                                                     /* empty name */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_NAME);
    fresh(); memset(buf + 12, 'a', 12);                                       /* no NUL */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_NAME);
    fresh(); buf[12 + 11] = 'z';                                              /* a byte after the NUL */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_NAME);
    fresh(); memcpy(buf + 12 + 16, buf + 12, 12);                             /* two slots, one name */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_NAME);
    fresh(); buf[12 + 12] = 0;                                                /* 0 frames */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_FRAMES);
    fresh(); buf[12 + 12] = 5;                                                /* 5 frames */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_FRAMES);
    fresh(); buf[12 + 14] = DOOMO_ANIM_FLASH;                                 /* a mode the bank does not do */
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_ERR_MODE);
    CHECK(memcmp(&b, &keep, sizeof b) == 0);                                  /* untouched on every error */
    fresh();
    CHECK_EQ(texbank_open(&b, buf, SAMPLE_LEN), TB_OK);                       /* the copy itself is fine */
}

TEST(texbank_hook_answers_the_ids_56_to_63)
{
    DoomoTexBank b;
    const uint8_t *grass = texture_pixels(TEX_GRASS, 0), *old[3];
    int i;
    texbank_activate(NULL);
    for (i = 0; i < 3; i++) old[i] = texture_pixels(TEX_STEAM_TILE + i, 0);
    CHECK_EQ(texbank_open(&b, doomo_texbank_sample, SAMPLE_LEN), TB_OK);
    /* no active bank: a bank id is the plain fallback, as before */
    CHECK(texture_pixels(DOOMO_TEXBANK_FIRST, 0) == grass);
    CHECK_EQ(texture_frames(DOOMO_TEXBANK_FIRST), 0);
    texbank_activate(&b);
    CHECK(texbank_active() == &b);
    CHECK(texture_pixels(DOOMO_TEXBANK_FIRST, 0) == texbank_pixels(&b, 0, 0));
    CHECK(texture_pixels(DOOMO_TEXBANK_FIRST + 2, 5) == texbank_pixels(&b, 2, 5));
    CHECK(texture_pixels(DOOMO_TEXBANK_FIRST + 3, 0) == grass);               /* a slot the bank lacks */
    CHECK(texture_pixels(DOOMO_TEXBANK_FIRST + 7, 0) == grass);
    CHECK_STR(texture_name(DOOMO_TEXBANK_FIRST + 1), "ink_stripe");
    CHECK_EQ(texture_frames(DOOMO_TEXBANK_FIRST + 1), 2);
    CHECK(texture_is_animated(DOOMO_TEXBANK_FIRST + 1));
    CHECK(!texture_is_animated(DOOMO_TEXBANK_FIRST));
    CHECK_EQ(texture_avg(DOOMO_TEXBANK_FIRST), texbank_avg(&b, 0));
    {
        const DoomoImage *im = texture_image(DOOMO_TEXBANK_FIRST, 0), *im1 = texture_image(DOOMO_TEXBANK_FIRST + 1, 12);
        CHECK(im->pix == texbank_pixels(&b, 0, 0));
        CHECK(im->w == 64 && im->h == 64);
        CHECK(im1->pix == texbank_pixels(&b, 1, 12));
        CHECK(im->uid != im1->uid);
        CHECK(im->uid < 0x01000000u && im->uid != 0);
    }
    /* the shared ids do not move */
    CHECK(texture_pixels(TEX_GRASS, 0) == grass);
    for (i = 0; i < 3; i++) CHECK(texture_pixels(TEX_STEAM_TILE + i, 0) == old[i]);
    /* a second activation gives fresh uids (a renderer caching by uid never shows the old bank) */
    {
        uint32_t u0 = texture_image(DOOMO_TEXBANK_FIRST, 0)->uid;
        texbank_activate(&b);
        CHECK(texture_image(DOOMO_TEXBANK_FIRST, 0)->uid != u0);
    }
    texbank_activate(NULL);
    CHECK(texture_pixels(DOOMO_TEXBANK_FIRST, 0) == grass);
}

/* ---- drawn: a room whose walls and floor are bank textures, at both screen sizes ---- */
static DoomoMap tmap;

static void build_room(void)
{
    int x, y;
    memset(&tmap, 0, sizeof(tmap));
    tmap.w = 16; tmap.h = 16; tmap.kind = MAP_DUNGEON; tmap.sky = SKY_NIGHT_STARS;
    tmap.ambient = 15;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            DoomoCell *c = &tmap.cells[y * 16 + x];
            c->floor_tex = DOOMO_TEXBANK_FIRST + 2;
            c->ceil_tex = TEX_TILE_FLOOR;
            c->ceil_h = 12;
            c->light = 15;
            if (x == 0 || y == 0 || x == 15 || y == 15) { c->wall = DOOMO_TEXBANK_FIRST; c->ceil_h = 16; }
        }
}

static uint32_t fb_hash(void)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < nat_w * nat_h; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

static void render(int tex_wall)
{
    DoomoView3D v;
    int x, y;
    for (y = 1; y < 15; y++)
        for (x = 1; x < 15; x++) tmap.cells[y * 16 + x].wall = 0;
    for (y = 0; y < 16; y++) { tmap.cells[y * 16].wall = (uint8_t)tex_wall; tmap.cells[y * 16 + 15].wall = (uint8_t)tex_wall; }
    memset(&v, 0, sizeof v);
    v.map = &tmap;
    v.x = INT2FIX(3 * 64 + 32);
    v.y = INT2FIX(7 * 64 + 32);
    v.z = INT2FIX(DOOMO_EYE_HEIGHT);
    v.angle = 2048;                 /* west: the wall is 2.5 cells ahead and fills the middle of the picture */
    v.player_sprite = -1;
    plat_clear(PAL_UI_RED);
    plat_view3d(&v);
}

TEST(texbank_draws_at_both_sizes)
{
    static const int sizes[2][2] = { { 240, 160 }, { 320, 240 } };
    DoomoTexBank b;
    int s;
    CHECK_EQ(texbank_open(&b, doomo_texbank_sample, SAMPLE_LEN), TB_OK);
    for (s = 0; s < 2; s++) {
        NatOptions o;
        uint32_t with_bank, again, without;
        int x, y, in_set = 0, n = 0, l, i, red = 0;
        uint8_t set[256];
        char path[600];
        memset(&o, 0, sizeof(o));
        o.w = sizes[s][0]; o.h = sizes[s][1]; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
        build_room();
        texbank_activate(&b);
        render(DOOMO_TEXBANK_FIRST);
        with_bank = fb_hash();
        snprintf(path, sizeof path, "%s/texbank_%dx%d.png", test_tmp_dir, nat_w, nat_h);
        nat_png_write(path, nat_fb, nat_w, nat_h, 2);
        for (i = 0; i < nat_w * nat_h; i++) red += nat_fb[i] == PAL_UI_RED;
        CHECK_MSG(red == 0, "%dx%d: %d pixels left unwritten", nat_w, nat_h, red);
        /* the middle of the picture is the bank's wall: only colours the bank slot (through any light
         * level) can make */
        memset(set, 0, sizeof set);
        for (i = 0; i < 64 * 64; i++)
            for (l = 0; l < DOOMO_LIGHT_LEVELS; l++) set[doomo_art_colormap[l][texbank_pixels(&b, 0, 0)[i]]] = 1;
        for (y = nat_h * 2 / 5; y < nat_h * 3 / 5; y++)
            for (x = nat_w * 2 / 5; x < nat_w * 3 / 5; x++, n++) in_set += set[nat_fb[y * nat_w + x]];
        CHECK_MSG(in_set == n, "%dx%d: %d of %d wall pixels are not the bank's colours", nat_w, nat_h, n - in_set, n);
        render(DOOMO_TEXBANK_FIRST);
        again = fb_hash();
        CHECK_EQ_U(with_bank, again);                       /* the same picture twice */
        render(DOOMO_TEXBANK_FIRST + 2);                    /* another slot: another picture */
        CHECK(fb_hash() != with_bank);
        texbank_activate(NULL);
        render(DOOMO_TEXBANK_FIRST);                        /* no bank: the fallback texture */
        without = fb_hash();
        CHECK(without != with_bank);
    }
}
