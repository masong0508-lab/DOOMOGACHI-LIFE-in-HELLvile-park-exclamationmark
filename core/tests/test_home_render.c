/* test_home_render.c - the room_ref golden (owner: H4; HF sec.11.5, SPEC H4).
 *
 * One fixed layout (the Living Room template plus 12 pieces from the M1
 * art set, recoloured, lit, stacked and on both walls) is drawn by the
 * reference renderer at 240x160 in all four views, then the shell frames
 * of room_shells.h (every wallpaper, floor, ceiling, tint and room kind).
 * The FNV-1a of each frame is compared with core/tests/golden/room_ref.txt:
 * the GBA renderer (H2) must reproduce these frames pixel for pixel (OBJ
 * hidden, cursor off; gba/harness/scripts/home.txt). Regenerate with
 * `$(BUILD)/tests --update-golden room_ref_golden` after an intended
 * change, and look at the PNGs in the tests' tmp dir.
 */
#include "test.h"
#include "../src/home_int.h"
#include "../gen/furn_ids.h"
#include "room_shells.h"

static uint8_t rr_cache[12 * 1024];

static uint32_t fnv(const uint8_t *p, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* The parity layout (keep it stable: the GBA harness rebuilds the same
 * room from room_shells.h). */
void home_test_parity_room(HomeRoom *r);
void home_test_parity_room(HomeRoom *r)
{
    rs_parity_room(r);
}

/* golden line `line`'s frame (room_shells.h) at 240x160 */
static uint32_t render_line(int line, DoomoRoomView *out)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    HomeRoom r;
    DoomoRoomView v;
    if (line < ROOM_SHELL_FIRST) rs_parity_room(&r);
    else rs_shell_room(line - ROOM_SHELL_FIRST, &r);
    rs_view(&v, objs, &r, line);
    v.vy = 12;
    v.vw = (int16_t)nat_w;
    v.vh = (int16_t)(nat_h - 24);
    v.plat_cache = rr_cache;
    v.plat_cache_cap = sizeof(rr_cache);
    if (out) *out = v;
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);
    return fnv(nat_fb, nat_w * nat_h);
}

static uint32_t render(const HomeRoom *r, int k, int daylight)
{
    DoomoRoomObj objs[HOME_OBJ_MAX];
    DoomoRoomView v;
    rs_view(&v, objs, r, 0);
    v.yaw = (int16_t)(k * 1024);
    v.daylight = (uint8_t)daylight;
    v.vy = 12;
    v.vw = (int16_t)nat_w;
    v.vh = (int16_t)(nat_h - 24);
    v.plat_cache = rr_cache;
    v.plat_cache_cap = sizeof(rr_cache);
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);
    return fnv(nat_fb, nat_w * nat_h);
}

/* a short label of golden line `line` (the golden file's comment column) */
static void line_label(int line, char *buf, size_t n)
{
    static const char *const kinds[3] = { "walls", "garden", "open" };
    static const char *const furn[6] = { "parity", "floor", "empty", "fenced", "lamps", "lamps-off" };
    const RoomShell *f;
    if (line < 4) { snprintf(buf, n, "classic view %d", line); return; }
    if (line < ROOM_SHELL_FIRST) { snprintf(buf, n, "classic view 0 night"); return; }
    f = &room_shells[line - ROOM_SHELL_FIRST];
    snprintf(buf, n, "%dx%d %s %s wp %d fl %d ce %d paint %s/%s tint %s %d light %d view %d", f->w, f->d,
             kinds[f->kind < 3 ? f->kind : 0], furn[f->furn < 6 ? f->furn : 0], f->wallpaper, f->floor,
             f->ceiling, doomo_paint_names[f->wall_paint & 31], doomo_paint_names[f->floor_paint & 31],
             doomo_tint_names[f->tint < TINT_COUNT ? f->tint : 0], f->tint_level, f->daylight, f->view);
    if (f->roof) {
        static const char *const roofs[ROOF_COUNT] = { "none", "cottage", "attic", "flat", "spike" };
        size_t l = strlen(buf);
        snprintf(buf + l, n - l, " roof %s", roofs[f->roof < ROOF_COUNT ? f->roof : 0]);
    }
}

TEST(room_ref_golden)
{
    char path[600], label[160];
    uint32_t h[ROOM_SHELL_LINES];
    int k;
    FILE *f;
    CHECK_EQ(nat_w, 240);
    {
        HomeRoom r;
        home_test_parity_room(&r);
        CHECK(r.nobj >= 12);
    }
    for (k = 0; k < ROOM_SHELL_LINES; k++) {
        h[k] = render_line(k, NULL);
        snprintf(path, sizeof(path), "%s/room_ref_%d.png", test_tmp_dir, k);
        nat_png_write(path, nat_fb, nat_w, nat_h, 2);
    }
    snprintf(path, sizeof(path), "%s/room_ref.txt", test_golden_dir);
    if (test_update_golden) {
        f = fopen(path, "w");
        CHECK_MSG(f != NULL, "cannot write %s", path);
        fprintf(f, "# room_ref.c on the parity layout at 240x160 (core/tests/test_home_render.c):\n"
                   "# FNV-1a of the frame per view 0..3, then view 0 at night. Regenerate with\n"
                   "#   $(BUILD)/tests --update-golden room_ref_golden\n");
        for (k = 0; k < ROOM_SHELL_LINES; k++) {
            if (k == ROOM_SHELL_FIRST)
                fprintf(f, "# %d..%d: the shell frames of core/tests/room_shells.h (every wallpaper,\n"
                           "# floor, ceiling ramp, tint, room kind and roof)\n", ROOM_SHELL_FIRST, ROOM_SHELL_LINES - 1);
            if (k < ROOM_SHELL_FIRST) {
                fprintf(f, "%d %08x\n", k, h[k]);
                continue;
            }
            line_label(k, label, sizeof(label));
            fprintf(f, "%d %08x  %s\n", k, h[k], label);
        }
        fclose(f);
        return;
    }
    f = fopen(path, "r");
    CHECK_MSG(f != NULL, "missing %s (run tests --update-golden room_ref_golden)", path);
    {
        char line[256];
        int n = 0;
        while (fgets(line, sizeof(line), f)) {
            int i;
            unsigned v;
            if (line[0] == '#') continue;
            if (sscanf(line, "%d %x", &i, &v) != 2 || i < 0 || i >= ROOM_SHELL_LINES) continue;
            if (v != h[i]) {
                fclose(f);
                line_label(i, label, sizeof(label));
                CHECK_MSG(0, "line %d (%s): %08x, golden %08x (see %s/room_ref_%d.png)", i, label, h[i], v,
                          test_tmp_dir, i);
            }
            n++;
        }
        fclose(f);
        CHECK_EQ(n, ROOM_SHELL_LINES);
    }
}

/* Every shell frame is its own picture (a parameter that stopped mattering
 * would make two frames equal), every pattern shows up in two views, and
 * the vocabulary is covered: wallpapers, floors and ceilings 0..7, every
 * tint, every room kind, a non-square room in a turned view. */
TEST(room_ref_shells_cover_the_vocabulary)
{
    uint32_t h[ROOM_SHELL_LINES];
    unsigned wp_views[8] = { 0 }, fl_views[8] = { 0 }, ceil = 0, tints = 0, kinds = 0;
    int i, j, turned = 0;
    for (i = 0; i < ROOM_SHELL_LINES; i++) h[i] = render_line(i, NULL);
    for (i = 0; i < ROOM_SHELL_LINES; i++)
        for (j = i + 1; j < ROOM_SHELL_LINES; j++) CHECK_MSG(h[i] != h[j], "lines %d and %d are the same frame", i, j);
    for (i = 0; i < ROOM_SHELL_COUNT; i++) {
        const RoomShell *f = &room_shells[i];
        if (f->kind == ROOMK_WALLS) {
            wp_views[f->wallpaper & 7] |= 1u << f->view;
            ceil |= 1u << (f->ceiling & 7);
        }
        fl_views[f->floor & 7] |= 1u << f->view;
        if (f->tint && f->tint_level >= 4) tints |= 1u << f->tint;
        kinds |= 1u << f->kind;
        if (f->w != f->d && (f->view & 1)) turned = 1;
    }
    for (i = 0; i < 8; i++) {
        CHECK_MSG((wp_views[i] & (wp_views[i] - 1)) != 0, "wallpaper %d is in fewer than two views", i);
        CHECK_MSG((fl_views[i] & (fl_views[i] - 1)) != 0, "floor %d is in fewer than two views", i);
    }
    CHECK_EQ_U(ceil, 0xFFu);
    for (i = 1; i < TINT_COUNT; i++) CHECK_MSG(tints >> i & 1, "tint %s has no frame", doomo_tint_names[i]);
    CHECK_EQ_U(kinds, 7u);
    CHECK(turned);
}

/* room_ref's tint threshold: below level 4 a tint draws nothing */
TEST(room_ref_tint_below_4_is_untinted)
{
    DoomoRoomView v;
    uint32_t a, b;
    int line = -1, i;
    for (i = 0; i < ROOM_SHELL_COUNT && line < 0; i++)
        if (room_shells[i].tint == TINT_RED && room_shells[i].tint_level == 4) line = ROOM_SHELL_FIRST + i;
    CHECK(line >= 0);
    a = render_line(line, &v);               /* v keeps the frame's (static) objects */
    v.tint_level = 3;
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);
    b = fnv(nat_fb, nat_w * nat_h);
    v.tint = 0;
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);
    CHECK(a != b);
    CHECK_EQ_U(b, fnv(nat_fb, nat_w * nat_h));
}

TEST(room_ref_views_differ_and_fill)
{
    HomeRoom r;
    uint32_t a, b;
    home_test_parity_room(&r);
    a = render(&r, 0, 15);
    b = render(&r, 1, 15);
    CHECK(a != b);
    /* the floor's middle is drawn (not the background) */
    CHECK(nat_fb[(nat_h / 2 + 20) * nat_w + nat_w / 2] != PAL_RAMP(RAMP_VAPE, 1));
}

TEST(room_ref_no_cache_draws_boxes)
{
    HomeRoom r;
    DoomoRoomObj o;
    DoomoRoomView v;
    home_test_parity_room(&r);
    memset(&v, 0, sizeof(v));
    home_obj_view(&r.obj[r.nobj - 1], &o);
    v.w = v.d = 6;
    v.objs = &o;
    v.nobj = 1;
    v.daylight = 15;
    v.cursor_x = v.cursor_y = -1;
    v.vw = (int16_t)nat_w;
    v.vh = (int16_t)nat_h;
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);            /* no plat_cache: the PS1 path, boxes */
    CHECK(nat_fb[(nat_h / 2) * nat_w + nat_w / 2] != PAL_BLACK);
}

TEST(home_scene_enters_and_places)
{
    int i;
    scene_set(&SCENE_HOME, HOME_ARG_OWN(HOME_COTTAGE));
    CHECK(scene_top() == &SCENE_HOME);
    CHECK(home_moved_in());
    for (i = 0; i < 10; i++) doomo_frame(0);
    CHECK(scene_top() == &SCENE_HOME);
    CHECK(home_has(FURN_SINGLE_BED) > 0);
    scene_set(&SCENE_HOME, 0);               /* reserved arg: refused */
    doomo_frame(0);
    CHECK(scene_depth() == 0 || scene_top() != &SCENE_HOME);
}

/* (H8) the light: lamp pools brighten what is near a lit lamp and nothing
 * at full daylight, at most ROOM_LAMPS light, a switched-off lamp lights
 * nothing (the hearth still does), glow pixels keep full light at night
 * unless their lamp is off, window panes turn navy with stars at night and
 * peach at dusk */
TEST(room_ref_light_pools_glow_and_panes)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    HomeRoom r;
    DoomoRoomView v;
    RoomLight L;
    int line = -1, line_off = -1, i, n, near, far;
    static uint8_t a[64 * 64], b[64 * 64];
    DoomoImage img;
    int ox, oy, glow_kept = 0, glow_dark = 0, stars = 0, navy = 0, peach = 0;
    for (i = 0; i < ROOM_SHELL_COUNT; i++) {
        if (room_shells[i].furn == RSF_LAMPS && line < 0) line = ROOM_SHELL_FIRST + i;
        if (room_shells[i].furn == RSF_LAMPS_OFF) line_off = ROOM_SHELL_FIRST + i;
    }
    CHECK(line >= 0 && line_off >= 0);
    rs_shell_room(line - ROOM_SHELL_FIRST, &r);
    rs_view(&v, objs, &r, line);
    n = room_light_setup(&L, &v, 0, v.w, v.d, 7);
    CHECK_EQ(n, ROOM_LAMPS);                                  /* five sources, four light */
    near = L.tile[1][0];                                      /* the cupcake lamp's tile */
    far = L.tile[5][5];
    CHECK(near == 15 && far >= 7 && far < near);
    CHECK(L.wall[0][2] > 7 && L.wall[0][2] <= 15);            /* the hearth's wall */
    CHECK_EQ(room_light_setup(&L, &v, 0, v.w, v.d, 15), 0);  /* daylight: nothing to add */
    CHECK_EQ(room_light_at(&L, 0, 0), 15);
    rs_shell_room(line_off - ROOM_SHELL_FIRST, &r);
    rs_view(&v, objs, &r, line_off);
    CHECK_EQ(room_light_setup(&L, &v, 0, v.w, v.d, 7), 1);   /* lights out: only the hearth */
    CHECK_EQ(L.tile[1][0] < near ? 1 : 0, 1);
    /* glow: the hearth's fire at level 7 keeps its full-light colours; a
     * switched-off lamp's glow goes dark with it */
    CHECK(room_ref_sprite(FURN_COTTAGE_HEARTH, 0, 0, -1, -1, 15, a, sizeof(a), &img, &ox, &oy));
    CHECK(room_ref_sprite(FURN_COTTAGE_HEARTH, 0, 0, -1, -1, 7, b, sizeof(b), &img, &ox, &oy));
    for (i = 0; i < img.w * img.h; i++) glow_kept += a[i] && a[i] == b[i] && palette_shade(a[i], 7) != a[i];
    CHECK(glow_kept > 10);
    CHECK(room_ref_sprite(FURN_LAVA_LAMP, 0, 0, -1, -1, 15, a, sizeof(a), &img, &ox, &oy));
    CHECK(room_ref_sprite(FURN_LAVA_LAMP, 0, 0, -1, -1, 7 | RR_SPR_DARK, b, sizeof(b), &img, &ox, &oy));
    for (i = 0; i < img.w * img.h; i++) glow_dark += a[i] && b[i] != palette_shade(a[i], 7);
    CHECK_EQ(glow_dark, 0);
    /* panes: navy and stars at night, peach at dusk, the art by day */
    CHECK(room_ref_sprite(FURN_WOOD_WINDOW, 0, 0, -1, -1, 15, a, sizeof(a), &img, &ox, &oy));
    CHECK(room_ref_sprite(FURN_WOOD_WINDOW, 0, 0, -1, -1, 15 | RR_SPR_NIGHT, b, sizeof(b), &img, &ox, &oy));
    for (i = 0; i < img.w * img.h; i++) {
        stars += b[i] == ROOM_STAR_COLOUR && a[i] != b[i];
        navy += b[i] >= PAL_RAMP(RAMP_SKY, 0) && b[i] <= PAL_RAMP(RAMP_SKY, 3) && a[i] != b[i];
    }
    CHECK(stars > 0 && navy > stars);
    CHECK(room_ref_sprite(FURN_WOOD_WINDOW, 0, 0, -1, -1, 15 | RR_SPR_DUSK, b, sizeof(b), &img, &ox, &oy));
    for (i = 0; i < img.w * img.h; i++) peach += b[i] >= PAL_RAMP(RAMP_PEACH, 4) && b[i] <= PAL_RAMP(RAMP_PEACH, 7);
    CHECK(peach > 20);
    CHECK_EQ(room_pane_mode(15), RPANE_DAY);
    CHECK_EQ(room_pane_mode(10), RPANE_DUSK);
    CHECK_EQ(room_pane_mode(5), RPANE_NIGHT);
}
