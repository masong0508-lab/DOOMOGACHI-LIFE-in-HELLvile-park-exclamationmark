/* test_art.c - palette, textures, skies and sprites (owner: art).
 * Checks the runtime API in palette.c / textures.c / sprites.c against the
 * generated tables in core/gen/art_data.c. */
#include "test.h"

static void art_boot(void)
{
    palette_init();
    textures_init();
    sprites_init();
}

TEST(art_colormap_contract)
{
    int i, lv, t;
    art_boot();
    for (i = 0; i < 256; i++) {
        CHECK_EQ(doomo_colormap[15][i], i);
        CHECK_EQ(doomo_colormap[0][i], i ? PAL_BLACK : 0);
        CHECK_EQ(doomo_tintmap[TINT_NONE][i], i);
        for (lv = 0; lv < DOOMO_LIGHT_LEVELS; lv++) {
            CHECK_MSG((doomo_colormap[lv][i] == 0) == (i == 0), "colormap[%d][%d]", lv, i);
            if (lv)
                CHECK_MSG(palette_luma(doomo_colormap[lv - 1][i]) <= palette_luma(doomo_colormap[lv][i]),
                          "colormap brighter at lower level %d idx %d", lv, i);
        }
        for (t = 0; t < TINT_COUNT; t++) CHECK_MSG((doomo_tintmap[t][i] == 0) == (i == 0), "tint %d idx %d", t, i);
    }
}

TEST(art_colormap_keeps_hue_in_family)
{
    /* a pastel pink at half light must stay pink (not turn grey) */
    uint8_t c, r, g, b;
    art_boot();
    c = doomo_colormap[8][PAL_RAMP(RAMP_PASTEL_PINK, 6)];
    palette_rgb(c, &r, &g, &b);
    CHECK_MSG(r > g + 30 && r > b, "pink at light 8 -> idx %d rgb %d,%d,%d", c, r, g, b);
    /* lava at half light stays fiery */
    c = doomo_colormap[8][PAL_RAMP(RAMP_LAVA, 6)];
    palette_rgb(c, &r, &g, &b);
    CHECK_MSG(r > b + 60, "lava at light 8 -> idx %d rgb %d,%d,%d", c, r, g, b);
    /* night tint keeps emissive lava bright */
    CHECK_EQ(doomo_tintmap[TINT_NIGHT][PAL_RAMP(RAMP_LAVA, 6)], PAL_RAMP(RAMP_LAVA, 6));
}

TEST(art_palette_helpers)
{
    int i;
    art_boot();
    for (i = 1; i < 256; i++) {
        uint8_t r, g, b, r2, g2, b2;
        palette_rgb((uint8_t)i, &r, &g, &b);
        palette_rgb(palette_nearest(r, g, b), &r2, &g2, &b2);
        CHECK_MSG(r == r2 && g == g2 && b == b2, "nearest exact %d", i);
        CHECK_MSG(i == PAL_BLACK || palette_luma(palette_darker((uint8_t)i)) <= palette_luma((uint8_t)i), "darker %d", i);
        CHECK_MSG(i == PAL_WHITE || palette_luma(palette_lighter((uint8_t)i)) >= palette_luma((uint8_t)i), "lighter %d", i);
        CHECK_EQ(palette_shade((uint8_t)i, 99), i);
        CHECK_EQ(palette_shade((uint8_t)i, -3), PAL_BLACK);
    }
    CHECK_EQ(palette_darker(0), 0);
    CHECK_EQ(palette_lighter(0), 0);
    for (i = 0; i < PAL_PAINT_COUNT; i++) CHECK(doomo_paint_names[i] && doomo_paint_names[i][0]);
    for (i = 0; i < PAL_SKIN_COUNT; i++) CHECK(doomo_skin_names[i] && doomo_skin_names[i][0]);
    for (i = 0; i < PAL_RAMP_COUNT; i++) CHECK(doomo_ramp_names[i] && doomo_ramp_names[i][0]);
}

TEST(art_textures_complete_and_opaque)
{
    int t, k;
    art_boot();
    for (t = 1; t < TEX_COUNT; t++) {
        const uint8_t *p = texture_pixels(t, 0);
        const DoomoImage *a = texture_image(t, 0);
        int nf = texture_frames(t);
        CHECK_MSG(p != NULL, "tex %d", t);
        CHECK_MSG(nf >= 1 && nf <= 4, "tex %d frames %d", t, nf);
        CHECK_MSG(texture_name(t)[0] != 0, "tex %d name", t);
        CHECK_MSG(texture_avg(t) != 0, "tex %d avg", t);
        CHECK(texture_is_animated(t) == (nf > 1));
        CHECK(a == texture_image(t, 0));
        CHECK(a->w == 64 && a->h == 64 && a->pix == p && a->uid != 0);
        for (k = 0; k < 64 * 64; k++) CHECK_MSG(p[k] != 0, "tex %d has index 0 at %d", t, k);
    }
    /* bad ids never crash and fall back to a real texture */
    CHECK(texture_pixels(0, 0) != NULL);
    CHECK(texture_pixels(-1, 0) != NULL);
    CHECK(texture_pixels(TEX_COUNT + 9, 0) != NULL);
    CHECK(texture_image(TEX_COUNT, 0) != NULL);
}

TEST(art_animated_textures_cycle)
{
    static const int anim[] = {TEX_WATER, TEX_LAVA, TEX_SLIME, TEX_VAPE_CLOUD_WALL, TEX_DEMON_EYES};
    size_t i;
    art_boot();
    for (i = 0; i < sizeof(anim) / sizeof(anim[0]); i++) {
        const uint8_t *seen[4];
        int ns = 0, tm, j, nf = texture_frames(anim[i]);
        uint32_t uid0 = texture_image(anim[i], 0)->uid;
        CHECK_MSG(nf >= 2, "tex %d should animate", anim[i]);
        for (tm = 0; tm < 1000; tm++) {
            const uint8_t *p = texture_pixels(anim[i], (uint16_t)tm);
            int found = 0;
            for (j = 0; j < ns; j++) found |= seen[j] == p;
            if (!found && ns < 4) seen[ns++] = p;
            if (texture_image(anim[i], (uint16_t)tm)->pix != p) CHECK_MSG(0, "image/pixels disagree");
            if (p != texture_pixels(anim[i], 0)) CHECK(texture_image(anim[i], (uint16_t)tm)->uid != uid0);
        }
        CHECK_MSG(ns == nf, "tex %d showed %d of %d frames", anim[i], ns, nf);
    }
    CHECK_EQ(texture_frames(TEX_WATER), 4);
    CHECK_EQ(texture_frames(TEX_LAVA), 4);
    CHECK_EQ(texture_frames(TEX_SLIME), 4);
    CHECK_EQ(texture_frames(TEX_VAPE_CLOUD_WALL), 4);
}

TEST(art_skies)
{
    int s, k;
    art_boot();
    for (s = 0; s < SKY_COUNT; s++) {
        const uint8_t *p = sky_pixels(s, 0);
        CHECK(p != NULL);
        for (k = 0; k < SKY_W * SKY_H; k++) CHECK_MSG(p[k] != 0, "sky %d index 0 at %d", s, k);
        CHECK(sky_horizon(s) != 0);
    }
    CHECK(sky_pixels(-1, 0) != NULL && sky_pixels(SKY_COUNT, 0) != NULL);
}

TEST(art_sprites_complete_centred_tight)
{
    int s, f;
    art_boot();
    for (s = 1; s < SPR_COUNT; s++) {
        int nf = sprite_frames(s), fl = sprite_flags(s);
        CHECK_MSG(nf >= 1 && nf <= SPR_MAX_FRAMES, "spr %d frames %d", s, nf);
        CHECK_MSG(sprite_name(s)[0] != 0, "spr %d name", s);
        if (s <= SPR_LORD_VAPULA) CHECK_MSG(nf == DFRAME_COUNT, "demon %d frames %d", s, nf);
        for (f = 0; f < nf; f++) {
            const DoomoImage *im = sprite_image(s, f);
            int x, y, opaque = 0, top = 0, bottom = 0, side = 0;
            CHECK(im && im->pix && im->w > 0 && im->h > 0 && im->uid != 0);
            CHECK_MSG((im->w & 1) == 0, "spr %d f %d odd width %d", s, f, im->w);
            CHECK_MSG(im->w <= 128 && im->h <= 128, "spr %d f %d too big", s, f);
            for (y = 0; y < im->h; y++)
                for (x = 0; x < im->w; x++)
                    if (im->pix[y * im->w + x]) {
                        opaque++;
                        top |= y == 0;
                        bottom |= y == im->h - 1;
                        side |= x == 0 || x == im->w - 1;
                    }
            CHECK_MSG(opaque > 0, "spr %d f %d empty", s, f);
            CHECK_MSG(side && (top || ((fl & SPRF_CENTERED) && bottom)), "spr %d f %d box not tight", s, f);
            if (!(fl & (SPRF_CENTERED | SPRF_FLOATS)) && f == 0) CHECK_MSG(bottom, "spr %d floats", s);
            if (s <= SPR_LORD_VAPULA && f == DFRAME_DEATH3) CHECK_MSG(bottom, "demon %d corpse floats", s);
        }
        CHECK(sprite_image(s, 99) == sprite_image(s, nf - 1));
        CHECK(sprite_image(s, -3) == sprite_image(s, 0));
    }
    CHECK(sprite_image(SPR_NONE, 0) != NULL);
    CHECK(sprite_image(SPR_COUNT + 3, 2) != NULL);
    CHECK_EQ(sprite_frames(SPR_NONE), 0);
    /* effects are centred, glowing projectiles are fullbright */
    CHECK(sprite_flags(SPR_EXPLOSION) & SPRF_CENTERED);
    CHECK(sprite_flags(SPR_FIREBALL) & SPRF_FULLBRIGHT);
    CHECK(sprite_flags(SPR_VAPE_WRAITH) & SPRF_FLOATS);
}

TEST(art_image_uids_unique)
{
    /* every texture frame and sprite frame has its own uid */
    static uint32_t uids[DOOMO_TEX_MAX_FRAMES + DOOMO_SPR_MAX_IMAGES];
    int n = 0, i, j, s, f, t;
    art_boot();
    for (t = 1; t < TEX_COUNT; t++)
        for (f = 0; f < 400; f += 1) {
            uint32_t u = texture_image(t, (uint16_t)f)->uid;
            int dup = 0;
            for (j = 0; j < n; j++) dup |= uids[j] == u;
            if (!dup && n < (int)(sizeof(uids) / sizeof(uids[0]))) uids[n++] = u;
            if (!texture_is_animated(t)) break;
        }
    for (s = 1; s < SPR_COUNT; s++)
        for (f = 0; f < sprite_frames(s); f++) {
            uint32_t u = sprite_image(s, f)->uid;
            for (j = 0; j < n; j++) CHECK_MSG(uids[j] != u, "uid clash spr %d f %d", s, f);
            if (n < (int)(sizeof(uids) / sizeof(uids[0]))) uids[n++] = u;
        }
    for (i = 0; i < n; i++) CHECK(uids[i] != 0);
}
