/* test_home_render.c - the room_ref golden (owner: H4; HF sec.11.5, SPEC H4).
 *
 * One fixed layout (the Living Room template plus 12 pieces from the M1
 * art set, recoloured, lit, stacked and on both walls) is drawn by the
 * reference renderer at 240x160 in all four views. The FNV-1a of each
 * frame is compared with core/tests/golden/room_ref.txt: the GBA renderer
 * (H2) must reproduce these frames pixel for pixel (OBJ hidden, cursor
 * off). Regenerate with `$(BUILD)/tests --update-golden room_ref_golden`
 * after an intended change, and look at the PNGs in the tests' tmp dir.
 */
#include "test.h"
#include "../src/home_int.h"
#include "../gen/furn_ids.h"

static uint8_t rr_cache[12 * 1024];

static uint32_t fnv(const uint8_t *p, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void put(HomeRoom *r, int furn, int x, int y, int rot, int z4, int pa, int pb)
{
    HomeObj *o = &r->obj[r->nobj++];
    o->w0 = HO_W0(furn, rot, z4 > 0);
    o->pos = HO_POS(x, y);
    o->z = (uint8_t)z4;
    o->pa = (uint8_t)(pa < 0 ? FURN_PAINT_DEFAULT : pa);
    o->pb = (uint8_t)(pb < 0 ? FURN_PAINT_DEFAULT : pb);
}

/* The parity layout (keep it stable: H2's harness rebuilds the same room). */
void home_test_parity_room(HomeRoom *r);
void home_test_parity_room(HomeRoom *r)
{
    home_room_template(HOME_ROOM_LIVING, r);
    put(r, FURN_CLOUD_BED, 4, 1, 0, 0, -1, -1);
    put(r, FURN_MARSHMALLOW_SOFA, 1, 4, 0, 0, PAINT_LAVENDER, -1);
    put(r, FURN_TEACUP_TABLE, 2, 2, 0, 0, -1, -1);
    put(r, FURN_CUPCAKE_LAMP, 5, 4, 0, 0, -1, -1);
    put(r, FURN_DAISY_RUG, 3, 3, 0, 0, -1, -1);
    put(r, FURN_BUNNY_BEANBAG, 4, 4, 1, 0, PAINT_SKY, -1);
    put(r, FURN_RAINBOW_BOOKCASE, 2, 0, 0, 0, -1, -1);
    put(r, FURN_HEART_MIRROR, 3, 0, 0, 6, -1, -1);
    put(r, FURN_PIPS_BEE_DRAWING, 0, 3, 1, 8, -1, -1);
    put(r, FURN_JELLYBEAN_JAR, 2, 2, 0, 5, -1, -1);
    put(r, FURN_HORNED_POOP_PLUSH, 0, 5, 0, 0, -1, -1);
    put(r, FURN_POTTED_FERN, 5, 2, 0, 0, -1, -1);
}

static uint32_t render(const HomeRoom *r, int k, int daylight)
{
    DoomoRoomObj objs[HOME_OBJ_MAX];
    DoomoRoomView v;
    int i;
    memset(&v, 0, sizeof(v));
    for (i = 0; i < r->nobj; i++) home_obj_view(&r->obj[i], &objs[i]);
    v.w = (uint8_t)HR_W(r);
    v.d = (uint8_t)HR_D(r);
    v.wallpaper = home_room_defs[HOME_ROOM_LIVING].wallpaper;
    v.floor = home_room_defs[HOME_ROOM_LIVING].floor;
    v.wall_paint = home_room_defs[HOME_ROOM_LIVING].wall_paint;
    v.floor_paint = home_room_defs[HOME_ROOM_LIVING].floor_paint;
    v.room_kind = ROOMK_WALLS;
    v.objs = objs;
    v.nobj = r->nobj;
    v.yaw = (int16_t)(k * 1024);
    v.punch_q8 = 256;
    v.daylight = (uint8_t)daylight;
    v.cursor_x = v.cursor_y = -1;
    v.vy = 12;
    v.vw = (int16_t)nat_w;
    v.vh = (int16_t)(nat_h - 24);
    v.plat_cache = rr_cache;
    v.plat_cache_cap = sizeof(rr_cache);
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);
    return fnv(nat_fb, nat_w * nat_h);
}

TEST(room_ref_golden)
{
    HomeRoom r;
    char path[600];
    uint32_t h[5];
    int k;
    FILE *f;
    CHECK_EQ(nat_w, 240);
    home_test_parity_room(&r);
    CHECK(r.nobj >= 12);
    for (k = 0; k < 5; k++) {
        h[k] = render(&r, k & 3, k < 4 ? 15 : 3);
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
        for (k = 0; k < 5; k++) fprintf(f, "%d %08x\n", k, h[k]);
        fclose(f);
        return;
    }
    f = fopen(path, "r");
    CHECK_MSG(f != NULL, "missing %s (run tests --update-golden room_ref_golden)", path);
    {
        char line[128];
        int n = 0;
        while (fgets(line, sizeof(line), f)) {
            int i;
            unsigned v;
            if (line[0] == '#') continue;
            if (sscanf(line, "%d %x", &i, &v) != 2 || i < 0 || i > 4) continue;
            if (v != h[i]) {
                fclose(f);
                CHECK_MSG(0, "view %d: %08x, golden %08x (see %s/room_ref_%d.png)", i, h[i], v, test_tmp_dir, i);
            }
            n++;
        }
        fclose(f);
        CHECK_EQ(n, 5);
    }
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
