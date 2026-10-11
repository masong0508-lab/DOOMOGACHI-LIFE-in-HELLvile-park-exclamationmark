/* test_render.c - the native platform: 2D primitives, the reference
 * raycaster, the overworld and the PNG writer (owner: foundation).
 * Renders are also written to the tmp dir as PNGs for eyeballing. */
#include <stdlib.h>
#include "test.h"

static DoomoMap tmap;

static void build_room(void)
{
    int x, y;
    memset(&tmap, 0, sizeof(tmap));
    tmap.w = 16; tmap.h = 16; tmap.kind = MAP_DUNGEON; tmap.sky = SKY_PASTEL_DAY;
    tmap.ambient = 12;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            DoomoCell *c = &tmap.cells[y * 16 + x];
            c->floor_tex = TEX_WOOD_FLOOR;
            c->ceil_tex = x >= 10 ? DOOMO_TEX_SKY : TEX_CLOUD_PUFF;
            c->ceil_h = 12;
            c->light = 14;
            if (x == 0 || y == 0 || x == 15 || y == 15) { c->wall = TEX_PASTEL_BRICK; c->ceil_h = 16; }
            if (x >= 6 && x <= 8 && y >= 5 && y <= 9) c->floor_h = 3;   /* platform */
            if (x == 5 && y >= 5 && y <= 9) c->floor_h = 1;             /* step */
            if (x == 12 && y == 4) { c->wall = TEX_CANDY_STRIPE; c->ceil_h = 4; } /* low block */
            if (x >= 11 && x <= 12 && y >= 10 && y <= 11) c->floor_h = -2;   /* pit */
        }
}

static int count_color(int x0, int y0, int w, int h, uint8_t c)
{
    int x, y, n = 0;
    for (y = y0; y < y0 + h; y++)
        for (x = x0; x < x0 + w; x++)
            if (nat_fb[y * nat_w + x] == c) n++;
    return n;
}

static void save_png(const char *name)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/%s.png", test_tmp_dir, name);
    nat_png_write(path, nat_fb, nat_w, nat_h, 2);
}

TEST(plat_2d_primitives_clip)
{
    plat_clear(PAL_WHITE);
    plat_rect(-10, -10, 20, 20, PAL_UI_PINK);
    CHECK_EQ(nat_fb[0], PAL_UI_PINK);
    CHECK_EQ(nat_fb[10], PAL_WHITE);
    plat_rect(nat_w - 5, nat_h - 5, 100, 100, PAL_UI_MINT);
    CHECK_EQ(nat_fb[nat_w * nat_h - 1], PAL_UI_MINT);
    plat_rect(5000, 5000, 10, 10, 1);
    plat_rect(10, 10, -5, 4, 1);
    plat_line(-50, -50, 400, 300, PAL_BLACK);
    plat_text(-3, -3, "Hi\nthere", PAL_BLACK);
    plat_text(nat_w - 3, nat_h - 3, "clip", PAL_BLACK);
    plat_shade_rect(-5, -5, 1000, 1000, 8);
    CHECK(nat_fb[nat_w * 20 + 100] != PAL_WHITE);       /* darkened */
    {
        static const uint8_t px[4] = { 0, PAL_UI_RED, PAL_UI_RED, 0 };
        DoomoImage img = { 2, 2, px, 1 };
        plat_clear(PAL_WHITE);
        plat_image(10, 10, &img, 2, 0, 0);
        CHECK_EQ(nat_fb[10 * nat_w + 12], PAL_UI_RED);
        CHECK_EQ(nat_fb[10 * nat_w + 10], PAL_WHITE);     /* index 0 transparent */
        plat_image(30, 10, &img, 1, IMG_FLIPX | IMG_SILHOUETTE, PAL_UI_GOLD);
        CHECK_EQ(nat_fb[10 * nat_w + 30], PAL_UI_GOLD);
        plat_image(-100, -100, &img, 4, IMG_SHADOW, 0);
        plat_image(0, 0, NULL, 1, 0, 0);
    }
}

TEST(raycaster_room_views)
{
    DoomoView3D v;
    DoomoBillboard bb[3];
    static const char *const names[4] = { "r3d_east", "r3d_platform", "r3d_up", "r3d_sky" };
    static const int16_t ang[4] = { 0, 3072 + 200, 0, 0 };
    static const int16_t pitch[4] = { 0, -60, 200, 100 };
    int i;
    build_room();
    memset(&v, 0, sizeof(v));
    v.map = &tmap;
    v.x = INT2FIX(2 * 64 + 32);
    v.y = INT2FIX(7 * 64 + 20);
    v.z = INT2FIX(DOOMO_EYE_HEIGHT);
    v.player_sprite = -1;
    bb[0].x = INT2FIX(9 * 64 + 32); bb[0].y = INT2FIX(7 * 64 + 32); bb[0].z = INT2FIX(24);
    bb[0].img = sprite_image(SPR_IMPLING, 0); bb[0].scale = 256; bb[0].flags = BB_SHADOW; bb[0].light = 0;
    bb[1] = bb[0]; bb[1].x = INT2FIX(4 * 64); bb[1].y = INT2FIX(3 * 64); bb[1].z = 0;
    bb[1].img = sprite_image(SPR_TREE, 0); bb[1].flags = BB_FULLBRIGHT;
    bb[2] = bb[0]; bb[2].x = INT2FIX(13 * 64); bb[2].y = INT2FIX(7 * 64); bb[2].z = 0;
    bb[2].img = sprite_image(SPR_CANDY, 0); bb[2].flags = BB_TRANSLUCENT;
    v.sprites = bb;
    v.sprite_count = 3;
    for (i = 0; i < 4; i++) {
        int fy;
        plat_clear(PAL_UI_RED);
        v.angle = ang[i];
        v.pitch = pitch[i];
        v.time = (uint16_t)(i * 10);
        plat_view3d(&v);
        save_png(names[i]);
        /* every pixel of the view was written */
        CHECK_MSG(count_color(0, 0, nat_w, nat_h, PAL_UI_RED) < nat_w, "%s left holes", names[i]);
        fy = nat_h - 2;
        CHECK(nat_fb[fy * nat_w + nat_w / 2] != PAL_UI_RED);
    }
    /* viewport + tint + a camera standing inside a wall cell must not crash */
    v.vx = 20; v.vy = 10; v.vw = 120; v.vh = 90;
    v.tint = TINT_RED; v.tint_level = 8;
    v.x = INT2FIX(5); v.y = INT2FIX(5);
    plat_clear(PAL_UI_MINT);
    plat_view3d(&v);
    CHECK_EQ(nat_fb[0], PAL_UI_MINT);                    /* outside the viewport */
    v.map = NULL;
    plat_view3d(&v);
}

TEST(raycaster_occludes_sprites_behind_walls)
{
    DoomoView3D v;
    DoomoBillboard bb;
    static uint8_t px[16 * 16];
    DoomoImage img = { 16, 16, px, 77 };
    int x, y;
    build_room();
    /* a full-height wall column between camera and sprite */
    for (y = 1; y < 15; y++) tmap.cells[y * 16 + 4].wall = TEX_PASTEL_BRICK;
    memset(px, PAL_UI_GOLD, sizeof(px));
    memset(&v, 0, sizeof(v));
    v.map = &tmap; v.x = INT2FIX(2 * 64); v.y = INT2FIX(7 * 64 + 32);
    v.z = INT2FIX(DOOMO_EYE_HEIGHT); v.player_sprite = -1;
    bb.x = INT2FIX(8 * 64); bb.y = INT2FIX(7 * 64 + 32); bb.z = 0;
    bb.img = &img; bb.scale = 512; bb.flags = BB_FULLBRIGHT; bb.light = 0;
    v.sprites = &bb; v.sprite_count = 1;
    plat_view3d(&v);
    CHECK_EQ(count_color(0, 0, nat_w, nat_h, PAL_UI_GOLD), 0);
    /* remove the wall: now it shows */
    for (y = 1; y < 15; y++) tmap.cells[y * 16 + 4].wall = 0;
    plat_view3d(&v);
    CHECK(count_color(0, 0, nat_w, nat_h, PAL_UI_GOLD) > 50);
    /* a low step in front hides only its feet */
    for (y = 1; y < 15; y++) tmap.cells[y * 16 + 5].floor_h = 4;
    for (x = 6; x < 15; x++) for (y = 1; y < 15; y++) tmap.cells[y * 16 + x].floor_h = 0;
    plat_view3d(&v);
    save_png("r3d_step_occlusion");
    CHECK(count_color(0, 0, nat_w, nat_h, PAL_UI_GOLD) > 20);
}

TEST(overworld_renders)
{
    DoomoOverworldView ov;
    DoomoBillboard bb[2];
    int d;
    build_room();
    memset(&ov, 0, sizeof(ov));
    ov.map = &tmap;
    ov.cx = INT2FIX(8 * 64); ov.cy = INT2FIX(8 * 64);
    bb[0].x = INT2FIX(8 * 64); bb[0].y = INT2FIX(8 * 64); bb[0].z = 0;
    bb[0].img = sprite_image(SPR_TREE, 0); bb[0].scale = 256; bb[0].flags = BB_SHADOW; bb[0].light = 0;
    bb[1] = bb[0]; bb[1].y = INT2FIX(7 * 64); bb[1].img = sprite_image(SPR_LAMP, 0);
    ov.sprites = bb; ov.sprite_count = 2;
    for (d = 15; d >= 0; d -= 15) {
        plat_clear(PAL_UI_RED);
        ov.daylight = (uint8_t)d;
        plat_overworld(&ov);
        save_png(d ? "ow_noon" : "ow_midnight");
        CHECK(count_color(0, 0, nat_w, nat_h, PAL_UI_RED) == 0);
    }
    ov.cx = INT2FIX(-5000);                    /* far outside the map */
    plat_overworld(&ov);
    ov.map = NULL;
    plat_overworld(&ov);
}

/* Read a PNG back: check chunk CRCs, inflate the stored blocks, compare. */
static int png_roundtrip_ok(const char *path, const uint8_t *pix, int w, int h)
{
    FILE *f = fopen(path, "rb");
    static uint8_t buf[600000];
    static uint8_t raw[300000];
    size_t n, pos = 8, rl = 0;
    int ok = 0, y;
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n < 8 || memcmp(buf, "\x89PNG\r\n\x1a\n", 8) != 0) return 0;
    while (pos + 12 <= n) {
        uint32_t len = ((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos + 1] << 16) |
                       ((uint32_t)buf[pos + 2] << 8) | buf[pos + 3];
        const uint8_t *type = buf + pos + 4, *data = buf + pos + 8;
        uint32_t crc = ((uint32_t)data[len] << 24) | ((uint32_t)data[len + 1] << 16) |
                       ((uint32_t)data[len + 2] << 8) | data[len + 3];
        if (crc != save_crc32(type, len + 4)) return 0;
        if (memcmp(type, "IDAT", 4) == 0) {
            size_t p = 2;
            for (;;) {
                int final = data[p] & 1;
                size_t bl = data[p + 1] | ((size_t)data[p + 2] << 8);
                if ((data[p] & 6) != 0) return 0;
                memcpy(raw + rl, data + p + 5, bl);
                rl += bl;
                p += 5 + bl;
                if (final) break;
            }
        }
        if (memcmp(type, "IEND", 4) == 0) { ok = 1; break; }
        pos += 12 + len;
    }
    if (!ok || rl != (size_t)(w + 1) * h) return 0;
    for (y = 0; y < h; y++)
        if (raw[y * (w + 1)] != 0 || memcmp(raw + y * (w + 1) + 1, pix + y * w, (size_t)w) != 0)
            return 0;
    return 1;
}

TEST(png_writer_roundtrip)
{
    char path[600];
    int i;
    for (i = 0; i < nat_w * nat_h; i++) nat_fb[i] = (uint8_t)(i * 7);
    snprintf(path, sizeof(path), "%s/png_test.png", test_tmp_dir);
    CHECK(nat_png_write(path, nat_fb, nat_w, nat_h, 1));
    CHECK(png_roundtrip_ok(path, nat_fb, nat_w, nat_h));
    CHECK(nat_png_write(path, nat_fb, nat_w, nat_h, 2));
}
