/* test_home_sheets.c - houseshot: the house's contact sheets through the
 * reference renderer (core/src/room_ref.c, the native plat_room) (owner:
 * H4; H1 of the house plan).
 *
 * Every room record's template in all four views, every ROM blueprint in
 * its room (four views, then six light levels), every theme on the Pastel
 * Parlour blueprint, the whole furniture catalog on 8x8 pages, and the
 * room_ref golden frames (core/tests/room_shells.h). As a test it renders
 * all of them at 240x160 and checks that each frame is a picture, that
 * every blueprint places pieces and that the catalog shows every piece
 * once. To regenerate the contact sheets (and every single frame) run
 *
 *   DOOMO_HOUSESHOT=DIR core/build/tests home_sheets
 *
 * which writes DIR/<w>x<h>/sheet_{rooms,blueprints,light,themes,catalog,
 * shells}.png plus the frames, at 240x160 and 320x240 (DOOMO_HOUSESHOT_RES
 * picks others, e.g. "320x240"). DIR "1" means the tests' tmp dir
 * (core/build/testtmp/houseshot). Nothing is written without it.
 */
#include <stdlib.h>
#include <sys/stat.h>
#include "test.h"
#include "../src/home_int.h"
#include "../gen/furn_ids.h"
#include "room_shells.h"

static uint8_t hs_cache[64 * 1024];
static DoomoRoomObj hs_objs[HOME_OBJ_MAX];

typedef struct HsSheet {
    uint8_t *pix;
    int cols, rows, w, h, n;
    const char *name;
} HsSheet;

typedef struct Hs {
    char dir[512];          /* "" = check only                              */
    int frames, bad;        /* frames drawn, frames that were not a picture */
    char bad_name[96];
} Hs;

#define HS_GAP 2

static void sheet_begin(HsSheet *s, const char *name, int cols, int count)
{
    s->name = name;
    s->cols = cols;
    s->rows = (count + cols - 1) / cols;
    s->w = cols * (nat_w + HS_GAP) + HS_GAP;
    s->h = s->rows * (nat_h + HS_GAP) + HS_GAP;
    s->n = 0;
    s->pix = (uint8_t *)calloc((size_t)s->w * (size_t)s->h, 1);
    if (s->pix) memset(s->pix, PAL_RAMP(RAMP_VAPE, 0), (size_t)s->w * (size_t)s->h);
}

static void sheet_add(HsSheet *s)
{
    int x0, y0, y;
    if (!s->pix || s->n >= s->cols * s->rows) return;
    x0 = HS_GAP + (s->n % s->cols) * (nat_w + HS_GAP);
    y0 = HS_GAP + (s->n / s->cols) * (nat_h + HS_GAP);
    for (y = 0; y < nat_h; y++) memcpy(s->pix + (size_t)(y0 + y) * (size_t)s->w + x0, nat_fb + y * nat_w, (size_t)nat_w);
    s->n++;
}

static void sheet_end(Hs *hs, HsSheet *s)
{
    char p[700];
    if (s->pix && hs->dir[0]) {
        snprintf(p, sizeof(p), "%s/sheet_%s.png", hs->dir, s->name);
        nat_png_write(p, s->pix, s->w, s->h, 1);
    }
    free(s->pix);
    s->pix = NULL;
}

/* the frame is a picture: at least 8 colours */
static int is_picture(void)
{
    uint8_t seen[256];
    int i, n = 0;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < nat_w * nat_h; i++)
        if (!seen[nat_fb[i]]) { seen[nat_fb[i]] = 1; n++; }
    return n >= 8;
}

static void hs_frame(Hs *hs, HsSheet *s, const char *name)
{
    char p[700];
    hs->frames++;
    if (!is_picture() && !hs->bad++) snprintf(hs->bad_name, sizeof(hs->bad_name), "%s", name);
    if (hs->dir[0]) {
        snprintf(p, sizeof(p), "%s/%s.png", hs->dir, name);
        nat_png_write(p, nat_fb, nat_w, nat_h, 1);
    }
    if (s) sheet_add(s);
}

/* a room as SCENE_HOME shows it (home_draw.c build_view): the room's own
 * walls or its record's defaults */
static void hs_draw(const HomeRoom *r, int room, int k, int daylight, const char *label)
{
    const HomeRoomDef *d = &home_room_defs[room];
    DoomoRoomView v;
    int i;
    memset(&v, 0, sizeof(v));
    for (i = 0; i < r->nobj && i < HOME_OBJ_MAX; i++) home_obj_view(&r->obj[i], &hs_objs[i]);
    v.w = (uint8_t)HR_W(r);
    v.d = (uint8_t)HR_D(r);
    v.wallpaper = r->wallpaper == 0xFF ? d->wallpaper : r->wallpaper;
    v.floor = r->floor == 0xFF ? d->floor : r->floor;
    v.wall_paint = r->wall_paint == 0xFF ? d->wall_paint : r->wall_paint;
    v.floor_paint = r->floor_paint == 0xFF ? d->floor_paint : r->floor_paint;
    v.ceiling = (uint8_t)((r->flags & HRF_CEIL_MASK) >> HRF_CEIL_SHIFT);
    v.room_kind = d->kind;
    v.objs = hs_objs;
    v.nobj = r->nobj < HOME_OBJ_MAX ? r->nobj : HOME_OBJ_MAX;
    v.yaw = (int16_t)(k * 1024);
    v.punch_q8 = 256;
    v.daylight = (uint8_t)daylight;
    v.cursor_x = v.cursor_y = -1;
    v.vy = 12;
    v.vw = (int16_t)nat_w;
    v.vh = (int16_t)(nat_h - 24);
    v.plat_cache = hs_cache;
    v.plat_cache_cap = sizeof(hs_cache);
    plat_clear(PAL_BLACK);
    room_ref_draw(&v);
    plat_text(2, 2, label, PAL_WHITE);
}

/* everything at the current nat_w x nat_h; returns the catalog pieces shown */
static int hs_run(Hs *hs, int *bp_empty)
{
    HomeState *s = home_state();
    HsSheet sh;
    HomeEdit e;
    HomePlan plan;
    char nm[96], label[96];
    int i, k, room, nbp = (int)doomo_home_blueprint_count, placed = 0;
    static const uint8_t levels[6] = { 0, 3, 6, 9, 12, 15 };
    *bp_empty = -1;
    home_reset();
    home_move_in();
    for (room = 0; room < HOME_ROOM_COUNT; room++) home_unlock_room(room);
    memset(s->owned, 0xFF, sizeof(s->owned));           /* own 15 of everything */

    /* 1. every room record's template, 4 views */
    sheet_begin(&sh, "rooms", 4, HOME_ROOM_COUNT * 4);
    for (room = 0; room < HOME_ROOM_COUNT; room++) {
        HomeRoom r;
        home_room_template(room, &r);
        for (k = 0; k < 4; k++) {
            snprintf(label, sizeof(label), "%s v%d", home_room_defs[room].name, k);
            hs_draw(&r, room, k, 15, label);
            snprintf(nm, sizeof(nm), "room%d_empty_v%d", room, k);
            hs_frame(hs, &sh, nm);
        }
    }
    sheet_end(hs, &sh);

    /* 2. every ROM blueprint in its room: 4 views, then the light levels */
    sheet_begin(&sh, "blueprints", 4, nbp * 4);
    for (i = 0; i < nbp; i++) {
        room = doomo_home_blueprints[i].room;
        home_edit_begin(&e, room);
        home_edit_blueprint(&e, i, &plan);
        if (plan.placed == 0 && *bp_empty < 0) *bp_empty = i;
        for (k = 0; k < 4; k++) {
            snprintf(label, sizeof(label), "%s v%d", doomo_home_blueprints[i].name, k);
            hs_draw(&e.room, room, k, 15, label);
            snprintf(nm, sizeof(nm), "bp%02d_room%d_v%d", i, room, k);
            hs_frame(hs, &sh, nm);
        }
    }
    sheet_end(hs, &sh);
    sheet_begin(&sh, "light", 6, nbp * 6);
    for (i = 0; i < nbp; i++) {
        room = doomo_home_blueprints[i].room;
        home_edit_begin(&e, room);
        home_edit_blueprint(&e, i, &plan);
        for (k = 0; k < 6; k++) {
            snprintf(label, sizeof(label), "%s light %d", doomo_home_blueprints[i].name, levels[k]);
            hs_draw(&e.room, room, 0, levels[k], label);
            snprintf(nm, sizeof(nm), "bp%02d_room%d_light%02d", i, room, levels[k]);
            hs_frame(hs, &sh, nm);
        }
    }
    sheet_end(hs, &sh);

    /* 3. every theme on the Pastel Parlour (blueprint 1) */
    sheet_begin(&sh, "themes", 5, THEME_COUNT);
    for (i = 0; i < THEME_COUNT; i++) {
        home_edit_begin(&e, HOME_ROOM_LIVING);
        home_edit_blueprint(&e, 1, &plan);
        home_edit_theme(&e, i);
        hs_draw(&e.room, HOME_ROOM_LIVING, 0, 15, doomo_home_themes[i].name);
        snprintf(nm, sizeof(nm), "theme%02d", i);
        hs_frame(hs, &sh, nm);
    }
    sheet_end(hs, &sh);

    /* 4. the catalog: every piece once, 16 to an 8x8 page (wall pieces in
     * a row along wall A) */
    {
        int f = 0, page = 0, n = home_furn_count();
        HomeRoom r;
        sheet_begin(&sh, "catalog", 4, (n + 15) / 16);
        while (f < n) {
            home_room_template(HOME_ROOM_LIVING, &r);
            r.size = (uint8_t)((8 - 1) | ((8 - 1) << 4));
            r.nobj = 0;
            for (; f < n && r.nobj < 16; f++) {
                const DoomoFurn *df = home_furn(f);
                int slot = r.nobj, x = (slot % 4) * 2, y = (slot / 4) * 2;
                HomeObj *ob;
                if (!df) continue;
                if (df->flags & FF_WALL) { x = slot % 8; y = 0; }
                ob = &r.obj[r.nobj++];
                ob->w0 = HO_W0(f, 0, 0);
                ob->pos = HO_POS(x, y);
                ob->z = (uint8_t)((df->flags & FF_WALL) ? 6 : 0);
                ob->pa = ob->pb = FURN_PAINT_DEFAULT;
                placed++;
            }
            snprintf(label, sizeof(label), "catalog %d", page);
            hs_draw(&r, HOME_ROOM_LIVING, 0, 15, label);
            snprintf(nm, sizeof(nm), "catalog_p%02d", page++);
            hs_frame(hs, &sh, nm);
        }
        sheet_end(hs, &sh);
    }

    /* 5. the room_ref golden frames (core/tests/golden/room_ref.txt order) */
    sheet_begin(&sh, "shells", 6, ROOM_SHELL_LINES);
    for (i = 0; i < ROOM_SHELL_LINES; i++) {
        HomeRoom r;
        DoomoRoomView v;
        if (i < ROOM_SHELL_FIRST) rs_parity_room(&r);
        else rs_shell_room(i - ROOM_SHELL_FIRST, &r);
        rs_view(&v, hs_objs, &r, i);
        v.vy = 12;
        v.vw = (int16_t)nat_w;
        v.vh = (int16_t)(nat_h - 24);
        v.plat_cache = hs_cache;
        v.plat_cache_cap = sizeof(hs_cache);
        plat_clear(PAL_BLACK);
        room_ref_draw(&v);
        snprintf(label, sizeof(label), "room_ref %d", i);
        plat_text(2, 2, label, PAL_WHITE);
        snprintf(nm, sizeof(nm), "shell%02d", i);
        hs_frame(hs, &sh, nm);
    }
    sheet_end(hs, &sh);
    home_reset();
    return placed;
}

TEST(home_sheets)
{
    const char *env = getenv("DOOMO_HOUSESHOT"), *res = getenv("DOOMO_HOUSESHOT_RES");
    int keep_w = nat_w, keep_h = nat_h, pass, nfurn = 0, i, placed, bp_empty;
    Hs hs;
    for (i = 0; i < home_furn_count(); i++) nfurn += home_furn(i) != NULL;
    if (!res) res = "240x160 320x240";
    for (pass = 0; pass < 4; pass++) {
        int w = 240, h = 160;
        memset(&hs, 0, sizeof(hs));
        if (env && env[0]) {
            /* the pass-th "WxH" of the list */
            const char *p = res;
            int j;
            for (j = 0; j < pass && p; j++) { p = strchr(p, ' '); if (p) p++; }
            if (!p || sscanf(p, "%dx%d", &w, &h) != 2) break;
            if (w < 64 || h < 64 || w > NAT_MAX_W || h > NAT_MAX_H) break;
            if (!strcmp(env, "1")) snprintf(hs.dir, sizeof(hs.dir), "%s/houseshot", test_tmp_dir);
            else snprintf(hs.dir, sizeof(hs.dir), "%s", env);
            mkdir(hs.dir, 0777);
            snprintf(hs.dir + strlen(hs.dir), sizeof(hs.dir) - strlen(hs.dir), "/%dx%d", w, h);
            mkdir(hs.dir, 0777);
        } else if (pass > 0) {
            break;
        }
        nat_w = w;
        nat_h = h;
        placed = hs_run(&hs, &bp_empty);
        nat_w = keep_w;
        nat_h = keep_h;
        if (hs.dir[0]) printf("houseshot: %d frames + 6 sheets in %s\n", hs.frames, hs.dir);
        CHECK_MSG(hs.bad == 0, "%dx%d: %d frames are not a picture (first: %s)", w, h, hs.bad, hs.bad_name);
        CHECK_MSG(bp_empty < 0, "blueprint %d placed nothing with everything owned", bp_empty);
        CHECK_EQ(placed, nfurn);
        CHECK_EQ(hs.frames, HOME_ROOM_COUNT * 4 + (int)doomo_home_blueprint_count * 10 + THEME_COUNT +
                            (nfurn + 15) / 16 + ROOM_SHELL_LINES);
    }
}
