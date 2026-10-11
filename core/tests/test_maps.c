/* test_maps.c - every registered map validates (owner: action):
 * a player start, textures and heights in range, sane doors and lifts,
 * things inside the map and not in walls, and for dungeons an exit that a
 * player can actually reach from the start: an independent BFS over the
 * cells that respects step-up, ceilings (crouch height), lifts (both
 * heights), doors and the keys found along the way. Also the live map:
 * doors open and close, lifts travel and return, events open remote doors.
 * Preview renders of every map land in the test tmp dir. */
#include "test.h"

#define CELLW DOOMO_CELL_UNITS

static int def_floor(const DoomoMapDef *d, int x, int y) { return d->cells[y * d->w + x].floor_h * DOOMO_HEIGHT_STEP; }

/* walkable floor range of a cell (lifts have two heights) */
static void frange(const DoomoMapDef *d, int x, int y, int *lo, int *hi)
{
    const DoomoCell *c = &d->cells[y * d->w + x];
    int a = c->floor_h * DOOMO_HEIGHT_STEP, b = a;
    if (c->flags & CELL_LIFT) b = (int8_t)c->tag * DOOMO_HEIGHT_STEP;
    *lo = DOOMO_MIN(a, b);
    *hi = DOOMO_MAX(a, b);
}

static int ceil_open(const DoomoMapDef *d, int x, int y)
{
    const DoomoCell *c = &d->cells[y * d->w + x];
    if (c->ceil_tex == DOOMO_TEX_SKY) return 30000;
    return c->ceil_h * DOOMO_HEIGHT_STEP;    /* doors: the open height */
}

static int can_step(const DoomoMapDef *d, int x, int y, int nx, int ny, int keys)
{
    const DoomoCell *n = &d->cells[ny * d->w + nx];
    int alo, ahi, blo, bhi;
    if (n->wall) return 0;
    if (n->flags & CELL_DOOR) {
        if (n->tag == DOOMO_DOOR_TAG_RED && !(keys & 1)) return 0;
        if (n->tag == DOOMO_DOOR_TAG_BLUE && !(keys & 2)) return 0;
        /* remote doors are opened by events (cleared groups, bosses,
         * triggers) - the combat test proves those fire */
    }
    frange(d, x, y, &alo, &ahi);
    frange(d, nx, ny, &blo, &bhi);
    if (blo - ahi > DOOMO_STEP_UP) return 0;
    if (ceil_open(d, nx, ny) - DOOMO_MAX(blo, alo) < DOOMO_CROUCH_HEIGHT) return 0;
    return 1;
}

/* BFS from the start with `keys`; marks reach[]; returns keys found. */
static int bfs(const DoomoMapDef *d, int sx, int sy, int keys, uint8_t *reach)
{
    static uint16_t q[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
    int head = 0, tail = 0, found = 0, i;
    memset(reach, 0, (size_t)d->w * d->h);
    reach[sy * d->w + sx] = 1;
    q[tail++] = (uint16_t)(sy * d->w + sx);
    while (head < tail) {
        int c = q[head++], x = c % d->w, y = c / d->w, k;
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k], ni;
            if (nx < 0 || ny < 0 || nx >= d->w || ny >= d->h) continue;
            ni = ny * d->w + nx;
            if (reach[ni] || !can_step(d, x, y, nx, ny, keys)) continue;
            reach[ni] = 1;
            q[tail++] = (uint16_t)ni;
        }
    }
    for (i = 0; i < d->thing_count; i++) {
        const DoomoThing *t = &d->things[i];
        if (!reach[t->y * d->w + t->x]) continue;
        if (t->type == THING_KEY_RED) found |= 1;
        if (t->type == THING_KEY_BLUE) found |= 2;
    }
    return found;
}

static void check_map(int id, const DoomoMapDef *d)
{
    static uint8_t reach[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
    int i, starts = 0, sx = -1, sy = -1, exits = 0, reached_exit = 0, keys = 0, prev = -1;
    CHECK_MSG(d->w > 2 && d->h > 2 && d->w <= DOOMO_MAP_MAX_W && d->h <= DOOMO_MAP_MAX_H,
              "map %d size %dx%d", id, d->w, d->h);
    CHECK_MSG(d->name && d->name[0], "map %d has no title", id);
    CHECK_MSG(d->sky < SKY_COUNT && d->music < SONG_COUNT && d->ambient <= 15, "map %d header", id);
    for (i = 0; i < d->w * d->h; i++) {
        const DoomoCell *c = &d->cells[i];
        int x = i % d->w, y = i / d->w;
        CHECK_MSG(c->wall < TEX_COUNT, "map %d (%d,%d) wall tex %d", id, x, y, c->wall);
        CHECK_MSG(c->floor_tex < TEX_COUNT, "map %d (%d,%d) floor tex %d", id, x, y, c->floor_tex);
        CHECK_MSG(c->ceil_tex < TEX_COUNT || c->ceil_tex == DOOMO_TEX_SKY, "map %d (%d,%d) ceil tex %d", id, x, y, c->ceil_tex);
        CHECK_MSG(c->light <= 15, "map %d (%d,%d) light", id, x, y);
        if (c->wall) continue;
        CHECK_MSG(c->floor_tex > 0, "map %d (%d,%d) open cell without floor", id, x, y);
        CHECK_MSG(c->ceil_tex > 0, "map %d (%d,%d) open cell without ceiling", id, x, y);
        if (c->ceil_tex != DOOMO_TEX_SKY)
            CHECK_MSG(c->ceil_h > c->floor_h, "map %d (%d,%d) ceiling %d <= floor %d", id, x, y, c->ceil_h, c->floor_h);
        if (c->flags & CELL_DOOR) {
            CHECK_MSG(c->ceil_tex != DOOMO_TEX_SKY, "map %d (%d,%d) door without texture", id, x, y);
            CHECK_MSG((c->ceil_h - c->floor_h) * DOOMO_HEIGHT_STEP >= DOOMO_CROUCH_HEIGHT,
                      "map %d (%d,%d) door too low when open", id, x, y);
        }
        if (c->flags & CELL_LIFT) CHECK_MSG((int8_t)c->tag != c->floor_h, "map %d (%d,%d) lift goes nowhere", id, x, y);
        if (c->flags & CELL_EXIT) exits++;
        /* the outer ring must be closed for dungeons and arenas */
        if (d->kind != MAP_OVERWORLD)
            CHECK_MSG(x > 0 && y > 0 && x < d->w - 1 && y < d->h - 1, "map %d (%d,%d) open cell on the border", id, x, y);
    }
    for (i = 0; i < d->thing_count; i++) {
        const DoomoThing *t = &d->things[i];
        CHECK_MSG(t->x < d->w && t->y < d->h, "map %d thing %d outside", id, i);
        CHECK_MSG(t->type > THING_NONE && t->type < THING_COUNT, "map %d thing %d type %d", id, i, t->type);
        CHECK_MSG(!d->cells[t->y * d->w + t->x].wall, "map %d thing %d (%d,%d) in a wall", id, i, t->x, t->y);
        if (t->type == THING_PLAYER_START) { starts++; sx = t->x; sy = t->y; }
    }
    if (d->kind == MAP_OVERWORLD) {
        CHECK_MSG(starts <= 1, "overworld map %d has %d starts", id, starts);
        return;
    }
    CHECK_MSG(starts == 1, "map %d has %d player starts", id, starts);
    /* reachability: iterate BFS until no new keys */
    while (keys != prev) {
        prev = keys;
        keys |= bfs(d, sx, sy, keys, reach);
    }
    for (i = 0; i < d->w * d->h; i++)
        if ((d->cells[i].flags & CELL_EXIT) && reach[i]) reached_exit = 1;
    if (d->kind == MAP_DUNGEON) {
        CHECK_MSG(exits > 0, "dungeon %d has no exit", id);
        CHECK_MSG(reached_exit, "dungeon %d: exit not reachable from the start", id);
    }
    /* every key / demon / pickup should be reachable or at least visible */
    {
        int unreachable = 0, first = 0;
        for (i = 0; i < d->thing_count; i++) {
            const DoomoThing *t = &d->things[i];
            if (t->type >= THING_HEALTH_SMALL && t->type <= THING_CANDY && !reach[t->y * d->w + t->x]) {
                if (!unreachable) first = i;
                unreachable++;
            }
        }
        CHECK_MSG(unreachable == 0, "map %d: %d pickups cannot be reached (first: type %d at %d,%d)", id, unreachable,
                  d->things[first].type, d->things[first].x, d->things[first].y);
    }
    (void)def_floor;
}

TEST(maps_registered_and_valid)
{
    int id, n = 0;
    for (id = 0; id < MAPID_COUNT; id++) {
        const DoomoMapDef *d = doomo_maps[id];
        if (!d) continue;
        n++;
        check_map(id, d);
        if (test_failed_now()) return;
    }
    CHECK(doomo_maps[MAPID_VAPE_CELLAR] != NULL);
    CHECK(doomo_maps[MAPID_CLOUD_NINE_SPIRE] != NULL);
    CHECK(doomo_maps[MAPID_ARENA] != NULL);
    CHECK(n >= 3);
}

TEST(maps_use_lots_of_heights)
{
    /* the action maps are the showcase for the 3D engines: many distinct
     * floor and ceiling heights, lifts, doors, sky, low ceilings */
    static const int ids[3] = { MAPID_VAPE_CELLAR, MAPID_CLOUD_NINE_SPIRE, MAPID_ARENA };
    int m;
    for (m = 0; m < 3; m++) {
        const DoomoMapDef *d = doomo_maps[ids[m]];
        uint8_t seen_f[256], seen_c[256];
        int i, nf = 0, nc = 0, lifts = 0, doors = 0;
        memset(seen_f, 0, sizeof(seen_f));
        memset(seen_c, 0, sizeof(seen_c));
        for (i = 0; i < d->w * d->h; i++) {
            const DoomoCell *c = &d->cells[i];
            if (c->wall) continue;
            if (!seen_f[(uint8_t)c->floor_h]) { seen_f[(uint8_t)c->floor_h] = 1; nf++; }
            if (c->ceil_tex != DOOMO_TEX_SKY && !seen_c[(uint8_t)c->ceil_h]) { seen_c[(uint8_t)c->ceil_h] = 1; nc++; }
            if (c->flags & CELL_LIFT) lifts++;
            if (c->flags & CELL_DOOR) doors++;
        }
        CHECK_MSG(nf >= 6, "map %d only %d floor heights", ids[m], nf);
        if (ids[m] != MAPID_ARENA) {
            CHECK_MSG(lifts >= 1, "map %d has no lift", ids[m]);
            CHECK_MSG(doors >= 1, "map %d has no door", ids[m]);
        }
        if (ids[m] == MAPID_VAPE_CELLAR) CHECK_MSG(nc >= 6, "vape cellar only %d ceiling heights", nc);
    }
}

TEST(map_load_doors_lifts_events)
{
    int i, door = -1, lift = -1, remote = -1, f0, z;
    map_load(MAPID_VAPE_CELLAR);
    CHECK_EQ(map_current_id(), MAPID_VAPE_CELLAR);
    CHECK(map_mover_count() > 0);
    CHECK_STR(map_title(MAPID_VAPE_CELLAR), "The Vape Cellar");
    for (i = 0; i < doomo_map.w * doomo_map.h; i++) {
        const DoomoCell *c = &doomo_map.cells[i];
        if ((c->flags & CELL_DOOR) && c->tag == 0 && door < 0) door = i;
        if ((c->flags & CELL_DOOR) && c->tag >= DOOMO_DOOR_TAG_REMOTE && remote < 0) remote = i;
        if ((c->flags & CELL_LIFT) && lift < 0) lift = i;
    }
    CHECK(door >= 0 && lift >= 0 && remote >= 0);
    {
        int dx = door % doomo_map.w, dy = door / doomo_map.w;
        /* doors start closed */
        CHECK_EQ(map_ceil_z(&doomo_map, dx, dy), map_floor_z(&doomo_map, dx, dy));
        CHECK_EQ(map_door_open(&doomo_map, dx, dy), 0);
        CHECK_EQ(map_door_bump(&doomo_map, dx, dy, 0), 1);
        for (i = 0; i < 60; i++) map_tick(&doomo_map);
        CHECK_EQ(map_door_open(&doomo_map, dx, dy), 256);
        CHECK(map_ceil_z(&doomo_map, dx, dy) - map_floor_z(&doomo_map, dx, dy) >= DOOMO_PLAYER_HEIGHT);
        /* the live cell follows (renderers that read cells directly) */
        CHECK(doomo_map.cells[door].ceil_h > doomo_map.cells[door].floor_h);
    }
    {
        int rx = remote % doomo_map.w, ry = remote / doomo_map.w;
        CHECK_EQ(map_door_bump(&doomo_map, rx, ry, 3), 0);           /* remote: bumping does nothing */
        CHECK(map_event(&doomo_map, doomo_map.cells[remote].tag) >= 1);
        for (i = 0; i < 60; i++) map_tick(&doomo_map);
        CHECK_EQ(map_door_open(&doomo_map, rx, ry), 256);
    }
    {
        int lx = lift % doomo_map.w, ly = lift / doomo_map.w, lo, hi;
        CHECK(map_lift_range(&doomo_map, lx, ly, &lo, &hi));
        f0 = map_floor_z(&doomo_map, lx, ly);
        CHECK(!map_lift_busy(&doomo_map, lx, ly));
        map_use(&doomo_map, lx, ly);
        for (i = 0; i < 40; i++) map_tick(&doomo_map);
        z = map_floor_z(&doomo_map, lx, ly);
        CHECK(z != f0);
        CHECK(z >= lo && z <= hi);
        CHECK(map_lift_busy(&doomo_map, lx, ly));
        for (i = 0; i < 400; i++) map_tick(&doomo_map);
        CHECK_EQ(map_floor_z(&doomo_map, lx, ly), f0);              /* came back down */
        CHECK(!map_lift_busy(&doomo_map, lx, ly));
    }
    /* locked doors need their key */
    for (i = 0; i < doomo_map.w * doomo_map.h; i++) {
        const DoomoCell *c = &doomo_map.cells[i];
        if ((c->flags & CELL_DOOR) && c->tag == DOOMO_DOOR_TAG_RED) {
            CHECK_EQ(map_door_bump(&doomo_map, i % doomo_map.w, i / doomo_map.w, 0), 0);
            CHECK_EQ(map_door_bump(&doomo_map, i % doomo_map.w, i / doomo_map.w, 1), 1);
            break;
        }
    }
    /* out of range is solid */
    CHECK(map_cell_safe(&doomo_map, -1, 5)->wall != 0);
    CHECK(map_cell_safe(&doomo_map, 5, 999)->wall != 0);
}

TEST(map_fallback_room)
{
    /* an id with no map (story maps may not exist yet) still loads */
    map_load(MAPID_COUNT + 3);
    CHECK(doomo_map.w > 0 && doomo_map.h > 0);
    CHECK(map_cell_safe(&doomo_map, 0, 0)->wall != 0);
}

TEST(map_previews_render)
{
    static const int ids[3] = { MAPID_VAPE_CELLAR, MAPID_CLOUD_NINE_SPIRE, MAPID_ARENA };
    int m;
    for (m = 0; m < 3; m++) {
        DoomoView3D v;
        char path[600];
        int k, solid = 0;
        combat_start(ids[m]);
        combat_tick();
        for (k = 0; k < 2; k++) {
            doomo_player.camera = (uint8_t)k;
            combat_build_view(&v);
            plat_view3d(&v);
            combat_draw_hud();
            snprintf(path, sizeof(path), "%s/map_%d_%s.png", test_tmp_dir, ids[m], k ? "tp" : "fp");
            nat_png_write(path, nat_fb, nat_w, nat_h, 2);
        }
        for (k = 0; k < nat_w * nat_h; k++) solid += nat_fb[k] != 0;
        CHECK(solid > nat_w * nat_h / 2);
    }
}
