/* test_weld.c - welding (owner: action): weld_stats is sane for every
 * combination of parts (exhaustive over the parts that interact, random
 * sampling of the rest), names are short and varied, costs are valid, and
 * every frame composes non-empty first-person sprites and icons in both
 * styles (a contact sheet lands in the test tmp dir). */
#include <stdlib.h>
#include "test.h"

static int stats_sane(const DoomoWeapon *w, char *why, int cap)
{
    DoomoWeaponStats s;
    uint8_t r[WRATE_COUNT];
    int i;
    weld_stats(w, &s);
#define BAD(...) do { snprintf(why, (size_t)cap, __VA_ARGS__); return 0; } while (0)
    if (s.damage < 1) BAD("damage %d", s.damage);
    if (s.pellets < 1 || s.pellets > 12) BAD("pellets %d", s.pellets);
    if (s.refire < 1) BAD("refire %d", s.refire);
    if (s.range < 1 || s.range > 64) BAD("range %d", s.range);
    if (s.ammo_type != 0xFF && s.ammo_type >= AMMO_COUNT) BAD("ammo type %d", s.ammo_type);
    if (s.ammo_type != 0xFF && s.ammo_use < 1) BAD("ammo use 0");
    if (s.ammo_type == 0xFF && (s.mag_size || s.ammo_use)) BAD("melee with a magazine");
    if (s.mag_size == 0 && s.reload) BAD("reload without magazine");
    if (s.mag_size && (s.reload < 8 || s.mag_size < s.ammo_use)) BAD("mag %d use %d reload %d", s.mag_size, s.ammo_use, s.reload);
    if (s.behaviour >= WB_COUNT) BAD("behaviour %d", s.behaviour);
    if ((s.behaviour == WB_PROJECTILE || s.behaviour == WB_LOB || s.behaviour == WB_FLOAT ||
         s.behaviour == WB_STREAM) && (s.pkind == PK_NONE || s.pkind >= PK_COUNT || s.speed < 6 || !s.projectile))
        BAD("projectile beh %d pk %d speed %d", s.behaviour, s.pkind, s.speed);
    if ((s.behaviour == WB_HITSCAN || s.behaviour == WB_RAIL || s.behaviour == WB_MELEE) && (s.pkind || s.projectile))
        BAD("hitscan with a projectile");
    if (s.wclass >= WCLASS_COUNT || s.zoom > 3 || s.burst < 1 || s.crit < 16 || s.holy < 16) BAD("misc");
    if ((s.fx & WFX_SPLASH) && !s.splash) BAD("splash flag without radius");
    if (s.charge && s.charge > 60) BAD("charge");
    if (weld_dps(&s) <= 0) BAD("dps %d", weld_dps(&s));
    weld_ratings(&s, r);
    for (i = 0; i < WRATE_COUNT; i++) if (r[i] < 1 || r[i] > 100) BAD("rating %d = %d", i, r[i]);
#undef BAD
    return 1;
}

TEST(weld_stats_every_frame_barrel_core_mod)
{
    DoomoWeapon w;
    char why[128];
    int f, b, c, m, n = 0;
    for (f = 0; f < WFRAME_COUNT; f++)
        for (b = 0; b < WBARREL_COUNT; b++)
            for (c = 0; c < WCORE_COUNT; c++)
                for (m = 0; m < WMOD_COUNT; m++) {
                    weld_default(&w, f, (b + c) & 1);
                    w.barrel = (uint8_t)b; w.core = (uint8_t)c; w.mod = (uint8_t)m;
                    w.quality = (uint8_t)((f * 7 + c) % 101);
                    CHECK_MSG(stats_sane(&w, why, sizeof(why)), "frame %d barrel %d core %d mod %d: %s", f, b, c, m, why);
                    n++;
                }
    CHECK_EQ(n, WFRAME_COUNT * WBARREL_COUNT * WCORE_COUNT * WMOD_COUNT);
}

TEST(weld_stats_every_frame_with_every_attachment)
{
    DoomoWeapon w;
    char why[128];
    int f, si, st, mg, u, ch, sty, q;
    for (f = 0; f < WFRAME_COUNT; f++)
        for (si = 0; si < WSIGHT_COUNT; si++)
            for (st = 0; st < WSTOCK_COUNT; st++)
                for (mg = 0; mg < WMAG_COUNT; mg++)
                    for (u = 0; u < WUNDER_COUNT; u++)
                        for (ch = 0; ch < WCHARM_COUNT; ch++)
                            for (sty = 0; sty < 2; sty++)
                                for (q = 0; q <= 100; q += 100) {
                                    weld_default(&w, f, sty);
                                    w.sight = (uint8_t)si; w.stock = (uint8_t)st; w.mag = (uint8_t)mg;
                                    w.under = (uint8_t)u; w.charm = (uint8_t)ch; w.quality = (uint8_t)q;
                                    CHECK_MSG(stats_sane(&w, why, sizeof(why)),
                                              "frame %d sight %d stock %d mag %d under %d charm %d style %d q %d: %s",
                                              f, si, st, mg, u, ch, sty, q, why);
                                }
}

TEST(weld_stats_random_full_combinations)
{
    DoomoRng r;
    DoomoWeapon w;
    char why[128];
    int i;
    rng_seed(&r, 4242);
    for (i = 0; i < 300000; i++) {
        memset(&w, 0, sizeof(w));
        w.frame = (uint8_t)rng_range(&r, WFRAME_COUNT);
        w.barrel = (uint8_t)rng_range(&r, WBARREL_COUNT);
        w.core = (uint8_t)rng_range(&r, WCORE_COUNT);
        w.mod = (uint8_t)rng_range(&r, WMOD_COUNT);
        w.sight = (uint8_t)rng_range(&r, WSIGHT_COUNT);
        w.stock = (uint8_t)rng_range(&r, WSTOCK_COUNT);
        w.mag = (uint8_t)rng_range(&r, WMAG_COUNT);
        w.under = (uint8_t)rng_range(&r, WUNDER_COUNT);
        w.charm = (uint8_t)rng_range(&r, WCHARM_COUNT);
        w.style = (uint8_t)rng_range(&r, 2);
        w.paint = (uint8_t)rng_range(&r, 32);
        w.accent = (uint8_t)rng_range(&r, 32);
        w.quality = (uint8_t)rng_range(&r, 101);
        w.xp = (uint8_t)rng_range(&r, 256);
        w.flags = WF_VALID;
        CHECK_MSG(stats_sane(&w, why, sizeof(why)), "random #%d frame %d core %d: %s", i, w.frame, w.core, why);
    }
    /* garbage part ids never crash and stay sane */
    memset(&w, 0xFF, sizeof(w));
    CHECK(stats_sane(&w, why, sizeof(why)));
    weld_stats(NULL, NULL);
}

TEST(weld_stats_make_sense)
{
    DoomoWeapon a, b;
    DoomoWeaponStats sa, sb;
    /* quality scales damage */
    weld_default(&a, WFRAME_ASSAULT_RIFLE, WSTYLE_BRUTAL);
    b = a;
    a.quality = 0; b.quality = 100;
    weld_stats(&a, &sa); weld_stats(&b, &sb);
    CHECK(sb.damage > sa.damage);
    /* the natural archetypes */
    weld_default(&a, WFRAME_SNIPER, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.zoom >= 2 && sa.behaviour == WB_HITSCAN && sa.damage > 60);
    weld_default(&a, WFRAME_SUPER_SHOTGUN, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.pellets >= 10 && sa.ammo_type == AMMO_SHELLS);
    weld_default(&a, WFRAME_ROCKET_LAUNCHER, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_PROJECTILE && sa.pkind == PK_ROCKET && sa.splash > 0);
    weld_default(&a, WFRAME_MINIGUN, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.spin_up > 0 && sa.auto_fire);
    weld_default(&a, WFRAME_FLAMETHROWER, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_STREAM && (sa.fx & WFX_BURN));
    weld_default(&a, WFRAME_RAILGUN, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_RAIL && (sa.fx & WFX_PIERCE));
    weld_default(&a, WFRAME_KATANA, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_MELEE && sa.ammo_type == 0xFF);
    weld_default(&a, WFRAME_BUBBLE_WAND, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_FLOAT && sa.pkind == PK_BUBBLE && sa.ammo_type == AMMO_CANDY);
    weld_default(&a, WFRAME_PLUSH_LAUNCHER, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_LOB && sa.pkind == PK_PLUSH && (sa.fx & WFX_BOUNCE));
    weld_default(&a, WFRAME_HEART_BOW, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK(sa.charge > 0 && (sa.fx & WFX_CHARM));
    weld_default(&a, WFRAME_WATER_PISTOL, WSTYLE_CUTE); weld_stats(&a, &sa);
    CHECK((sa.fx & WFX_HOLY) && sa.holy > 16);                        /* holy water core! */
    weld_default(&a, WFRAME_BFG, WSTYLE_BRUTAL); weld_stats(&a, &sa);
    CHECK(sa.damage >= 200 && sa.splash >= 60);
    /* cores transform: a pistol with a plasma core shoots plasma */
    weld_default(&a, WFRAME_PISTOL, WSTYLE_CUTE); a.core = WCORE_PLASMA; weld_stats(&a, &sa);
    CHECK(sa.behaviour == WB_PROJECTILE && sa.pkind == PK_PLASMA && sa.ammo_type == AMMO_CELLS);
    /* ...and a minigun can be a bubble hose */
    weld_default(&a, WFRAME_MINIGUN, WSTYLE_CUTE); a.core = WCORE_BUBBLE; weld_stats(&a, &sa);
    CHECK(sa.pkind == PK_BUBBLE && sa.ammo_type == AMMO_CANDY);
    /* candy tube makes anything run on candy */
    weld_default(&a, WFRAME_LMG, WSTYLE_BRUTAL); a.mag = WMAG_CANDY_TUBE; weld_stats(&a, &sa);
    CHECK(sa.ammo_type == AMMO_CANDY);
    /* scopes zoom */
    weld_default(&a, WFRAME_DMR, WSTYLE_BRUTAL); a.sight = WSIGHT_SCOPE_8X; weld_stats(&a, &sa);
    CHECK(sa.zoom == 3);
    /* level ups add damage */
    weld_default(&a, WFRAME_SMG, WSTYLE_BRUTAL); b = a; b.xp = 255;
    weld_stats(&a, &sa); weld_stats(&b, &sb);
    CHECK(weld_level(&b) == 7 && sb.damage > sa.damage);
}

TEST(weld_names_short_and_varied)
{
    DoomoRng r;
    DoomoWeapon w;
    static char names[600][32];
    int i, j, uniq = 0;
    char buf[40];
    rng_seed(&r, 99);
    for (i = 0; i < 600; i++) {
        int k;
        memset(&w, 0, sizeof(w));
        w.frame = (uint8_t)rng_range(&r, WFRAME_COUNT);
        w.barrel = (uint8_t)rng_range(&r, WBARREL_COUNT);
        w.core = (uint8_t)rng_range(&r, WCORE_COUNT);
        w.mod = (uint8_t)rng_range(&r, WMOD_COUNT);
        w.sight = (uint8_t)rng_range(&r, WSIGHT_COUNT);
        w.stock = (uint8_t)rng_range(&r, WSTOCK_COUNT);
        w.mag = (uint8_t)rng_range(&r, WMAG_COUNT);
        w.under = (uint8_t)rng_range(&r, WUNDER_COUNT);
        w.charm = (uint8_t)rng_range(&r, WCHARM_COUNT);
        w.style = (uint8_t)rng_range(&r, 2);
        w.paint = (uint8_t)rng_range(&r, 32);
        w.accent = (uint8_t)rng_range(&r, 32);
        w.quality = (uint8_t)rng_range(&r, 101);
        w.xp = (uint8_t)rng_range(&r, 256);
        weld_name(&w, buf, sizeof(buf));
        CHECK_MSG(strlen(buf) > 0 && strlen(buf) <= 31, "name '%s' (%d chars)", buf, (int)strlen(buf));
        for (k = 0; buf[k]; k++) CHECK_MSG(buf[k] >= 32 && buf[k] < 127, "non-ASCII in '%s'", buf);
        memcpy(names[i], buf, 32);
    }
    for (i = 0; i < 600; i++) {
        for (j = 0; j < i; j++) if (strcmp(names[i], names[j]) == 0) break;
        if (j == i) uniq++;
    }
    CHECK_MSG(uniq >= 540, "only %d/600 unique names", uniq);
    /* cute and brutal vocabularies differ */
    weld_default(&w, WFRAME_MINIGUN, WSTYLE_CUTE);
    weld_name(&w, names[0], 32);
    w.style = WSTYLE_BRUTAL;
    weld_name(&w, names[1], 32);
    CHECK(strcmp(names[0], names[1]) != 0);
    /* tiny buffers are safe */
    weld_name(&w, buf, 4);
    CHECK(strlen(buf) <= 3);
    weld_name(&w, buf, 0);
}

TEST(weld_costs_and_parts)
{
    DoomoWeapon w;
    uint8_t items[12], counts[12];
    int f, n, i, l;
    for (f = 0; f < WFRAME_COUNT; f++) {
        int s;
        for (s = 0; s < 2; s++) {
            weld_default(&w, f, s);
            n = weld_cost(&w, items, counts, 12);
            CHECK(n >= 2);
            for (i = 0; i < n; i++) {
                CHECK(items[i] > ITEM_NONE && items[i] < ITEM_COUNT && counts[i] > 0);
                CHECK(doomo_items[items[i]].category == ICAT_MATERIAL);
            }
        }
    }
    CHECK_EQ(weld_cost(&w, items, counts, 1), 1);
    CHECK_EQ(weld_cost(NULL, items, counts, 12), 0);
    for (l = CLIST_WFRAME; l <= CLIST_WCHARM; l++) {
        CHECK(weld_part_count(l) > 1);
        for (i = 0; i < weld_part_count(l); i++) {
            CHECK(weld_part_name(l, i)[0] != 0);
            weld_part_set(&w, l, i);
            CHECK_EQ(weld_part_get(&w, l), i);
        }
        CHECK_STR(weld_part_name(l, 99), "");
    }
    CHECK_EQ(weld_part_count(CLIST_HAIR), 0);
}

TEST(weld_action_unlocks_in_range)
{
    int i, n = 0, frames_locked = 0;
    for (i = 0; doomo_unlocks_action[i].name; i++) {
        const DoomoUnlock *u = &doomo_unlocks_action[i];
        n++;
        CHECK_MSG(u->id >= 416 && u->id <= 607, "action unlock id %d out of range", u->id);
        CHECK(u->how && u->how[0]);
        CHECK(unlock_by_id(u->id) == u);
        if (u->cat == UL_CAT_WEAPON_PART) {
            CHECK(u->index < weld_part_count(u->list));
            if (u->list == CLIST_WFRAME) frames_locked++;
        }
    }
    CHECK(n >= 80);
    /* the basics are available from the start */
    CHECK(unlock_option_ok(CLIST_WFRAME, WFRAME_PISTOL));
    CHECK(unlock_option_ok(CLIST_WFRAME, WFRAME_BUBBLE_WAND));
    CHECK(unlock_option_ok(CLIST_WCORE, WCORE_BULLET));
    CHECK(!unlock_option_ok(CLIST_WFRAME, WFRAME_BFG));
    CHECK(frames_locked >= 20 && frames_locked < WFRAME_COUNT);
    /* defeating demons unlocks parts */
    progress_add(PROG_DEMONS_KILLED, 40);
    CHECK(unlock_option_ok(CLIST_WFRAME, WFRAME_ASSAULT_RIFLE));
}

#define SHEET_W (WELD_VIEW_W * 8)
#define SHEET_H ((WELD_VIEW_H + WELD_ICON_H) * 10)
TEST(weld_sprites_every_frame)
{
    static uint8_t sheet[SHEET_W * SHEET_H];
    char path[512];
    DoomoWeapon w;
    int f, s, fr, x, y;
    for (f = 0; f < WFRAME_COUNT; f++)
        for (s = 0; s < 2; s++) {
            const DoomoImage *ic;
            int k, solid = 0;
            weld_default(&w, f, s);
            for (fr = 0; fr < 4; fr++) {
                const DoomoImage *img = weld_view_image(&w, fr);
                CHECK(img && img->pix && img->w == WELD_VIEW_W && img->h == WELD_VIEW_H);
                solid = 0;
                for (k = 0; k < img->w * img->h; k++) solid += img->pix[k] != 0;
                CHECK_MSG(solid > 150, "frame %d style %d anim %d: only %d pixels", f, s, fr, solid);
                /* outlines are coloured, never the black marker */
                for (k = 0; k < img->w * img->h; k++) CHECK(img->pix[k] != PAL_BLACK);
            }
            ic = weld_icon_image(&w);
            CHECK(ic && ic->w == WELD_ICON_W && ic->h == WELD_ICON_H);
            solid = 0;
            for (k = 0; k < ic->w * ic->h; k++) solid += ic->pix[k] != 0;
            CHECK_MSG(solid > 20, "icon of frame %d style %d: %d pixels", f, s, solid);
            /* a contact sheet of every idle sprite + icon for eyeballing */
            {
                const DoomoImage *img = weld_view_image(&w, 0);
                int cell = f * 2 + s, ox = (cell % 8) * WELD_VIEW_W, oy = (cell / 8) * (WELD_VIEW_H + WELD_ICON_H);
                for (y = 0; y < WELD_VIEW_H + WELD_ICON_H; y++)
                    for (x = 0; x < WELD_VIEW_W; x++) {
                        uint8_t c = PAL_RAMP(RAMP_STONE, ((cell + (cell / 8)) & 1) ? 3 : 4);
                        if (y < WELD_VIEW_H && img->pix[y * img->w + x]) c = img->pix[y * img->w + x];
                        else if (y >= WELD_VIEW_H && x < WELD_ICON_W && ic->pix[(y - WELD_VIEW_H) * ic->w + x])
                            c = ic->pix[(y - WELD_VIEW_H) * ic->w + x];
                        sheet[(oy + y) * SHEET_W + ox + x] = c;
                    }
            }
        }
    snprintf(path, sizeof(path), "%s/weld_sheet.png", test_tmp_dir);
    nat_png_write(path, sheet, SHEET_W, SHEET_H, 1);
    /* the cache keeps the same image for the same weapon + frame */
    weld_default(&w, WFRAME_SMG, WSTYLE_CUTE);
    CHECK(weld_view_image(&w, 0) == weld_view_image(&w, 0));
    {
        uint32_t uid = weld_view_image(&w, 0)->uid;
        w.paint = (uint8_t)(w.paint + 1);
        CHECK(weld_view_image(&w, 0)->uid != uid);
    }
    {
        int mx, my, gx, gy;
        for (f = 0; f < WFRAME_COUNT; f++) {
            weld_default(&w, f, WSTYLE_CUTE);
            weld_view_points(&w, &mx, &my, &gx, &gy);
            CHECK(mx >= -8 && mx < WELD_VIEW_W + 8 && my >= -8 && my < WELD_VIEW_H);
            CHECK(gx >= -8 && gx < WELD_VIEW_W + 16 && gy >= 0 && gy < WELD_VIEW_H + 16);
        }
    }
}
