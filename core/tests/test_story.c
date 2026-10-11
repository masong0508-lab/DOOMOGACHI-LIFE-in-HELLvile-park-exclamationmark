/* test_story.c - Story Mode (owner: story): a scripted playthrough of the
 * whole arc through the real overworld scene (dialogue answered with A,
 * cutscenes and the credits skipped, minigames auto-won), every chapter's
 * key beats, map validation (every quest target reachable on foot on all
 * four overworld maps), the side-quest table, small talk, places, flags,
 * and a save/load round trip mid-story. */
#include "test.h"
#include "../src/story_int.h"
#include "../src/overworld_int.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void frames(int n, uint32_t b) { while (n-- > 0) frame(b); }
static void press(uint32_t b) { frame(b); frame(0); }

static void new_story(uint32_t seed)
{
    story_debug_autowin(1);
    town_new(TOWN_STORY, seed, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

static uint8_t seen[BEAT_COUNT];

/* Play until the beat is reached: A through dialogue / cutscenes, START
 * through the credits, story_debug_step() on the overworld. */
static int play_until(int beat, int max_iter)
{
    int i;
    for (i = 0; i < max_iter; i++) {
        const DoomoScene *top = scene_top();
        seen[story_beat()] = 1;
        if (story_beat() >= beat && top == &SCENE_STORY && !story_talking()) return 1;
        if (top == &SCENE_DIALOG || top == &SCENE_CUTSCENE || top == &SCENE_CONFIRM) { press(BTN_A); continue; }
        if (top == &SCENE_CREDITS) { press(BTN_START); frames(40, 0); press(BTN_A); continue; }
        if (top == &SCENE_STORY) {
            story_debug_step();
            frame(0);
            continue;
        }
        /* anything else the plot opened: close it like a player would */
        press(BTN_B);
        if (scene_top() == top) scene_pop();
    }
    return story_beat() >= beat;
}

TEST(story_full_playthrough_reaches_the_credits)
{
    static const int key[] = {
        BEAT_C1_MAYOR, BEAT_C1_KEY, BEAT_C1_HOME, BEAT_C1_PIP, BEAT_C1_FRIENDS, BEAT_C1_WALL,
        BEAT_C2_VAPOR, BEAT_C2_FUZZ, BEAT_C2_EXORCISE, BEAT_C2_RIVET, BEAT_C2_CELLAR,
        BEAT_C3_NESTS, BEAT_C3_SPIRE, BEAT_C4_GATE, BEAT_C4_PRISONERS, BEAT_C4_HELP,
        BEAT_C4_MAYOR, BEAT_C4_FESTIVAL, BEAT_C4_STAGE, BEAT_DONE
    };
    int i;
    memset(seen, 0, sizeof(seen));
    new_story(4242);
    CHECK(scene_top() == &SCENE_STORY);
    CHECK_EQ(story_chapter(), STORY_CH1);
    CHECK_MSG(play_until(BEAT_C1_PIP, 3000), "stuck at %s", story_beat_name(story_beat()));
    CHECK(pet_exists());
    CHECK(inv_count(ITEM_HOUSE_KEY) == 1);
    CHECK_MSG(play_until(BEAT_C2_VAPOR, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK_EQ(story_chapter(), STORY_CH2);
    CHECK(story_flag(SF_FOG_SEEN));
    CHECK_MSG(play_until(BEAT_C2_CELLAR, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK(story_flag(SF_STEAMY_SAVED) || story_flag(SF_STEAMY_LOST));
    CHECK(story_flag(SF_WEAPON_CHOSEN));
    CHECK(doomo_town.weapons[doomo_town.story.equipped].flags & WF_VALID);
    CHECK_MSG(play_until(BEAT_C3_NESTS, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK_EQ(story_chapter(), STORY_CH3);
    CHECK_EQ(story_map(), MAPID_HAHALAND_INVADED);
    CHECK_EQ(map_current_id(), MAPID_HAHALAND_INVADED);
    CHECK_MSG(play_until(BEAT_C4_GATE, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK_EQ(story_chapter(), STORY_CH4);
    CHECK_EQ(story_map(), MAPID_HAHALAND);
    CHECK_MSG(play_until(BEAT_C4_HELP, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK_EQ(story_map(), MAPID_HELLVILE);
    CHECK(story_hellvile_friends() >= 5);
    CHECK_MSG(play_until(BEAT_C4_FESTIVAL, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK_EQ(story_map(), MAPID_PARK_UNITED);
    CHECK(story_flag(SF_C4_SOOTKIN) && story_flag(SF_C4_LADY_SMOG) && story_flag(SF_C4_RIVET_HOME));
    CHECK_MSG(play_until(BEAT_DONE, 4000), "stuck at %s", story_beat_name(story_beat()));
    CHECK_EQ(story_chapter(), STORY_CH_DONE);
    CHECK(story_flag(SF_CREDITS_SEEN));
    CHECK(unlock_has(USTORY_UNITED_PARK));
    CHECK(unlock_has(USTORY_CH1_DONE) && unlock_has(USTORY_CH4_DONE));
    CHECK(unlock_has(USTORY_SONG_FIRST + SONG_FESTIVAL - 1));
    for (i = 0; i < DOOMO_COUNTOF(key); i++)
        CHECK_MSG(seen[key[i]], "never saw beat %s", story_beat_name(key[i]));
    frames(60, 0);
    CHECK(scene_top() == &SCENE_STORY);
    story_debug_autowin(0);
}

TEST(story_debug_jump_sets_up_every_beat)
{
    int b;
    for (b = 0; b < BEAT_COUNT; b++) {
        int m, cx, cy;
        test_reset();
        town_new(TOWN_STORY, 7 + (uint32_t)b, TOWN_POP_PREMADE);
        sim_init();
        story_new_game();
        story_debug_jump(story_beat_chapter(b), b);
        CHECK_MSG(story_beat() == b, "jump to %s landed on %s", story_beat_name(b), story_beat_name(story_beat()));
        CHECK_EQ(story_chapter(), story_beat_chapter(b));
        CHECK_MSG(story_objective()[0] != 0, "no objective at %s", story_beat_name(b));
        story_target(&m, &cx, &cy);
        if (m >= 0) CHECK(cx < 64 && cy < 64);
        if (b >= BEAT_C1_HATCH) CHECK(pet_exists());
        if (b >= BEAT_C2_CELLAR) CHECK(story_flag(SF_WEAPON_CHOSEN));
        while (scene_depth() > 0) scene_pop();
    }
}

/* ---- map validation ------------------------------------------------------------ */
static uint8_t reach[64 * 64];

static void flood(int map, int sx, int sy)
{
    static uint16_t q[64 * 64];
    int qh = 0, qt = 0;
    const DoomoMap *m;
    map_load(map);
    m = &doomo_map;
    memset(reach, 0, sizeof(reach));
    if (!ow_walkable_cell(sx, sy, map_cell(m, sx, sy)->floor_h)) return;
    reach[sy * 64 + sx] = 1;
    q[qt++] = (uint16_t)(sy * 64 + sx);
    while (qh < qt) {
        int c = q[qh++], x = c % 64, y = c / 64, d;
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        int fh = map_cell(m, x, y)->floor_h;
        for (d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || reach[ny * 64 + nx]) continue;
            if (!ow_walkable_cell(nx, ny, fh)) continue;
            reach[ny * 64 + nx] = 1;
            q[qt++] = (uint16_t)(ny * 64 + nx);
        }
    }
}

static int reachable(int x, int y) { return x >= 0 && y >= 0 && x < 64 && y < 64 && reach[y * 64 + x]; }

static int door_reachable(int x, int y)
{
    return reachable(x, y + 1) || reachable(x, y - 1) || reachable(x - 1, y) || reachable(x + 1, y);
}

static int map_check(int map, int sx, int sy, char *why, int cap)
{
    const DoomoMapDef *d;
    int i, x, y, doors = 0, triggers = 0;
    flood(map, sx, sy);
    d = doomo_map.def;
    if (!d) { doomo_snprintf(why, cap, "map %d missing", map); return 0; }
    for (i = 0; i < d->thing_count; i++) {
        const DoomoThing *t = &d->things[i];
        if ((t->type == THING_NPC || t->type == THING_SPAWNER) && !reachable(t->x, t->y)) {
            doomo_snprintf(why, cap, "map %d: %s spot (%d,%d) unreachable", map,
                           t->type == THING_NPC ? "wander" : "nest", t->x, t->y);
            return 0;
        }
    }
    for (y = 0; y < doomo_map.h; y++)
        for (x = 0; x < doomo_map.w; x++) {
            const DoomoCell *c = map_cell(&doomo_map, x, y);
            if (c->wall && c->tag >= STORY_TAG_DOOR && c->tag < STORY_TAG_DOOR + PLACE_COUNT) {
                doors++;
                if (!door_reachable(x, y)) { doomo_snprintf(why, cap, "map %d: door %d at (%d,%d) unreachable", map, c->tag - 64, x, y); return 0; }
            }
            if (!c->wall && (c->flags & CELL_TRIGGER) && c->tag >= 128) {
                triggers++;
                if (!reachable(x, y)) { doomo_snprintf(why, cap, "map %d: trigger %d at (%d,%d) unreachable", map, c->tag, x, y); return 0; }
            }
        }
    if (doors < 1 || triggers < 3) { doomo_snprintf(why, cap, "map %d: %d doors %d triggers", map, doors, triggers); return 0; }
    return 1;
}

/* NPCs stand on reachable cells or next to them (e.g. behind a counter). */
static int spot_ok(int x, int y)
{
    return reachable(x, y) || door_reachable(x, y);
}

TEST(story_overworld_maps_are_valid_and_reachable)
{
    static const struct { uint8_t map, x, y; } starts[4] = {
        { MAPID_HAHALAND, 34, 26 }, { MAPID_HAHALAND_INVADED, 52, 39 },
        { MAPID_HELLVILE, 3, 27 }, { MAPID_PARK_UNITED, 16, 26 }
    };
    char why[96];
    int k;
    for (k = 0; k < 4; k++) {
        CHECK(doomo_maps[starts[k].map] != NULL);
        CHECK_EQ(doomo_maps[starts[k].map]->kind, MAP_OVERWORLD);
        CHECK(doomo_maps[starts[k].map]->w <= 64 && doomo_maps[starts[k].map]->h <= 64);
        why[0] = 0;
        CHECK_MSG(map_check(starts[k].map, starts[k].x, starts[k].y, why, sizeof(why)), "%s", why);
    }
}

TEST(story_every_target_and_npc_spot_is_reachable)
{
    int b, map, i;
    StoryNpcSpot sp[32];
    static const struct { uint8_t map, x, y; } starts[4] = {
        { MAPID_HAHALAND, 34, 26 }, { MAPID_HAHALAND_INVADED, 52, 39 },
        { MAPID_HELLVILE, 3, 27 }, { MAPID_PARK_UNITED, 16, 26 }
    };
    for (b = 0; b < BEAT_COUNT; b++) {
        int m, cx, cy;
        test_reset();
        town_new(TOWN_STORY, 11, TOWN_POP_PREMADE);
        sim_init();
        story_new_game();
        story_debug_jump(story_beat_chapter(b), b);
        story_target(&m, &cx, &cy);
        for (i = 0; i < 4; i++) {
            int n, j;
            map = starts[i].map;
            flood(map, starts[i].x, starts[i].y);
            if (m == map) CHECK_MSG(spot_ok(cx, cy), "beat %s target (%d,%d) on map %d unreachable", story_beat_name(b), cx, cy, map);
            n = script_cast(map, sp, 32);
            for (j = 0; j < n; j++)
                CHECK_MSG(spot_ok(sp[j].x, sp[j].y), "beat %s: cast %d at (%d,%d) map %d unreachable",
                          story_beat_name(b), sp[j].id, sp[j].x, sp[j].y, map);
        }
    }
}

TEST(story_side_quests_table)
{
    int q, n = story_side_count(), kinds = 0, hill = 0, hell = 0, ch[6] = { 0 };
    char why[96];
    CHECK(n >= 40 && n <= STORY_SIDE_MAX);
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    for (q = 0; q < n; q++) {
        int gm, gx, gy, tm, tx, ty, g = story_side_giver(q);
        const DoomoResident *r = town_res(g);
        CHECK_MSG(r && r->status == RES_ALIVE, "quest %d giver %d missing", q, g);
        CHECK(story_side_title(q)[0] && story_side_hint(q)[0]);
        CHECK(story_side_chapter(q) >= 1 && story_side_chapter(q) <= 5);
        ch[story_side_chapter(q)]++;
        kinds |= 1 << story_side_kind(q);
        if (RES_DISTRICT(g)) hell++; else hill++;
        story_side_where(q, &gm, &gx, &gy);
        flood(gm, gm == MAPID_HELLVILE ? 3 : gm == MAPID_PARK_UNITED ? 16 : 34, gm == MAPID_HELLVILE ? 27 : 26);
        doomo_snprintf(why, sizeof(why), "quest %s giver at (%d,%d) map %d", story_side_title(q), gx, gy, gm);
        CHECK_MSG(spot_ok(gx, gy), "%s unreachable", why);
        if (gm == MAPID_HAHALAND && story_side_chapter(q) <= 3) {
            flood(MAPID_HAHALAND_INVADED, 52, 39);
            if (story_side_chapter(q) == 3) CHECK_MSG(spot_ok(gx, gy), "%s unreachable when invaded", why);
        }
        if (story_side_target(q, &tm, &tx, &ty) && story_side_kind(q) != SQ_FETCH && story_side_kind(q) != SQ_HUNT) {
            flood(tm, tm == MAPID_HELLVILE ? 3 : tm == MAPID_PARK_UNITED ? 16 : 34, tm == MAPID_HELLVILE ? 27 : 26);
            CHECK_MSG(spot_ok(tx, ty), "quest %s target (%d,%d) map %d unreachable", story_side_title(q), tx, ty, tm);
        }
    }
    CHECK_EQ(kinds, (1 << SQ_KIND_COUNT) - 1);          /* every kind is used */
    CHECK(hill >= 15 && hell >= 10);                     /* both districts give quests */
    CHECK(ch[1] >= 8 && ch[2] >= 6 && ch[3] >= 6 && ch[4] >= 8);
}

/* Take a few side quests through the real talk path. */
static int talk(int id)
{
    int i;
    story_interact_npc(id);
    for (i = 0; i < 40 && scene_top() != &SCENE_STORY; i++) press(BTN_A);
    for (i = 0; i < 40 && story_talking(); i++) press(BTN_A);
    return 1;
}

static int find_quest(const char *title)
{
    int q;
    for (q = 0; q < story_side_count(); q++) if (!strcmp(story_side_title(q), title)) return q;
    return -1;
}

static void settle(void);

TEST(story_side_quests_play)
{
    int q, money;
    new_story(31337);
    story_debug_jump(1, BEAT_C1_FRIENDS);
    frames(5, 0);
    settle();
    /* fetch: Strong Bones */
    q = find_quest("Strong Bones");
    CHECK(q >= 0);
    inv_take(ITEM_MILK, inv_count(ITEM_MILK));
    CHECK_EQ(story_side_state(q), SIDE_OFFERED);
    talk(story_side_giver(q));
    CHECK_EQ(story_side_state(q), SIDE_ACTIVE);
    talk(story_side_giver(q));                           /* not enough milk yet */
    CHECK_EQ(story_side_state(q), SIDE_ACTIVE);
    inv_add(ITEM_MILK, 2);
    money = (int)doomo_town.money;
    talk(story_side_giver(q));
    CHECK_EQ(story_side_state(q), SIDE_DONE);
    CHECK_EQ(inv_count(ITEM_MILK), 0);
    CHECK((int)doomo_town.money > money);
    /* deliver: Mail Mix-Up */
    q = find_quest("Mail Mix-Up");
    talk(story_side_giver(q));
    CHECK_EQ(story_side_state(q), SIDE_ACTIVE);
    CHECK_EQ(inv_count(ITEM_LOVE_LETTER), 1);
    talk(52);                                            /* Valentina */
    CHECK_EQ(story_side_state(q), SIDE_READY);
    talk(story_side_giver(q));
    CHECK_EQ(story_side_state(q), SIDE_DONE);
    CHECK(unlock_has(USTORY_REWARD_FIRST + 1));
    /* photo: Say Cheese! */
    q = find_quest("Say Cheese!");
    talk(story_side_giver(q));
    CHECK_EQ(story_side_state(q), SIDE_ACTIVE);
    CHECK(side_trigger_spot(MAPID_HAHALAND, 34, 23, 0) == 0);   /* photos need A */
    CHECK(side_trigger_spot(MAPID_HAHALAND, 34, 23, 1) == 1);
    while (story_talking()) press(BTN_A);
    CHECK_EQ(story_side_state(q), SIDE_READY);
    talk(story_side_giver(q));
    CHECK_EQ(story_side_state(q), SIDE_DONE);
    /* visit: Ribbon Cutting */
    q = find_quest("Ribbon Cutting");
    talk(story_side_giver(q));
    story_enter_place(PLACE_FUN_FAIR);
    while (story_talking()) press(BTN_A);
    CHECK_EQ(story_side_state(q), SIDE_READY);
    /* match */
    q = find_quest("Cupid's Little Helper");
    talk(story_side_giver(q));
    talk(11);
    CHECK_EQ(story_side_state(q), SIDE_READY);
    CHECK(rel_value(11, 25) != 0 || rel_kind(11, 25) >= 0);
    /* the quest log lists them */
    story_push_scene(&SCENE_QUESTLOG, 1, PEND_OTHER, CTX_FREE, 0);
    frames(10, 0);
    CHECK(scene_top() == &SCENE_QUESTLOG);
    press(BTN_B);
    frames(3, 0);
    CHECK(scene_top() == &SCENE_STORY);
    story_debug_autowin(0);
}

TEST(story_small_talk_has_something_to_say)
{
    char buf[160];
    int id, v;
    town_new(TOWN_STORY, 9, TOWN_POP_PREMADE);
    story_new_game();
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id += 37)
        for (v = 0; v < 6; v++) {
            story_small_talk(id, v, buf, sizeof(buf));
            CHECK_MSG(buf[0] != 0, "citizen %d says nothing", id);
            CHECK_MSG(!strchr(buf, '{'), "unformatted: %s", buf);
        }
    CHECK(talk_cast_idle(ID_MAYOR) != NULL);
    CHECK(talk_cast_idle(ID_NANA) != NULL);
}

TEST(story_flags_and_formatting)
{
    char buf[120];
    town_new(TOWN_STORY, 3, TOWN_POP_PREMADE);
    story_new_game();
    CHECK(story_flag(SF_STARTED));
    story_set_flag(255, 1);
    CHECK(story_flag(255));
    story_set_flag(255, 0);
    CHECK(!story_flag(255));
    CHECK(!story_flag(-1) && !story_flag(256));
    story_fmt(buf, sizeof(buf), "Hi @P! @E says hi. 100@@");
    CHECK(strstr(buf, story_player_name()) != NULL);
    CHECK(strstr(buf, "@@") == NULL && strstr(buf, "100@") != NULL);
    CHECK(STORY_FLAG_SIDE_FIRST + 2 * STORY_SIDE_MAX <= STORY_FLAG_SECRET_FIRST);
    CHECK(STORY_FLAG_SECRET_FIRST + 32 <= STORY_FLAG_PLACE_FIRST);
    CHECK(STORY_FLAG_PLACE_FIRST + PLACE_COUNT <= DOOMO_STORY_FLAGS);
    CHECK(SF_MET0 + 16 <= STORY_FLAG_SIDE_FIRST);
    CHECK(story_place_name(PLACE_CAFE)[0]);
    CHECK(story_region_name(MAPID_HAHALAND, 34, 20)[0]);
    CHECK(story_cutscene_name(CUT_FOG)[0]);
}

static void settle(void)
{
    int i;
    for (i = 0; i < 200 && scene_top() != &SCENE_STORY; i++) {
        const DoomoScene *t = scene_top();
        press(BTN_A);
        if (scene_top() == t && t != &SCENE_DIALOG && t != &SCENE_CUTSCENE) scene_pop();
    }
    for (i = 0; i < 200 && story_talking(); i++) press(BTN_A);
}

TEST(story_places_and_activities_menu)
{
    int cupcakes;
    new_story(77);
    story_debug_jump(1, BEAT_C1_FRIENDS);
    settle();
    story_enter_place(PLACE_FUN_FAIR);
    CHECK(story_talking());
    settle();
    CHECK(scene_top() == &SCENE_STORY);
    /* buying in the cafe: Buy is the second option, the cupcake the first item */
    doomo_town.money = 100;
    cupcakes = inv_count(ITEM_CUPCAKE);
    story_place_menu(PLACE_CAFE);
    CHECK(story_talking());
    press(BTN_A);                     /* finish typing the question */
    press(BTN_DOWN);
    press(BTN_A);                     /* Buy */
    frames(4, 0);
    press(BTN_A);
    press(BTN_A);                     /* the cupcake */
    frames(4, 0);
    CHECK_EQ(inv_count(ITEM_CUPCAKE), cupcakes + 1);
    CHECK(doomo_town.money < 100);
    settle();
    story_debug_autowin(0);
}

TEST(story_save_round_trip_mid_story)
{
    int beat, map;
    new_story(999);
    story_debug_jump(3, BEAT_C3_NESTS);
    story_set_flag(SF_NEST0 + 1, 1);
    frames(10, 0);
    settle();
    beat = story_beat();
    map = story_map();
    ow_sync_to_save();
    CHECK(save_game(0));
    story_set_beat(BEAT_C1_KEY);
    CHECK(load_game(0));
    CHECK_EQ(story_beat(), beat);
    CHECK_EQ(story_map(), map);
    CHECK(story_flag(SF_NEST0 + 1));
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    frames(20, 0);
    CHECK_EQ(map_current_id(), MAPID_HAHALAND_INVADED);
    story_debug_autowin(0);
}

TEST(story_overworld_walks_and_collides)
{
    int32_t x0;
    int i;
    new_story(55);
    story_debug_jump(1, BEAT_C1_FRIENDS);
    settle();
    (void)i;
    ow_enter_map(MAPID_HAHALAND, 34, 26);
    frames(5, 0);
    x0 = ow.px;
    frames(20, BTN_LEFT);
    CHECK(ow.px < x0);                                   /* walked west */
    /* the Wall stops you */
    ow_enter_map(MAPID_HAHALAND, 54, 45);
    frames(5, 0);
    frames(80, BTN_RIGHT);
    CHECK(ow.px / OW_CELL <= 55);
    /* the sea stops you */
    ow_enter_map(MAPID_HAHALAND, 20, 56);
    frames(80, BTN_DOWN);
    CHECK(ow.py / OW_CELL <= 57);
    /* the pause panel opens and closes */
    settle();
    press(BTN_START);
    CHECK(ow.pause);
    press(BTN_B);
    CHECK(!ow.pause);
    story_debug_autowin(0);
}

TEST(story_nest_encounter_fights_real_demons)
{
    /* without autowin: walking into a nest pushes combat on the invaded
     * map with the nest's demons alive (the map's many props must not eat
     * the actor pool), and losing / quitting leaves the nest standing */
    int cx, cy, n;
    scene_set(&SCENE_STORY, STORY_ARG_DEBUG_BEAT + BEAT_C3_NESTS);
    story_debug_autowin(0);
    settle();
    CHECK_EQ(story_beat(), BEAT_C3_NESTS);
    CHECK(ow_nest_cell(0, &cx, &cy));
    ow_enter_map(MAPID_HAHALAND_INVADED, cx, cy + 1);
    frames(3, 0);
    for (n = 0; n < 30 && scene_top() == &SCENE_STORY; n++) frame(BTN_UP);
    CHECK_MSG(scene_top() == &SCENE_COMBAT, "top %s", scene_top() ? scene_top()->name : "-");
    n = combat_demons_alive();
    CHECK_MSG(n >= 3, "only %d demons", n);
    CHECK_EQ(map_current_id(), MAPID_HAHALAND_INVADED);
    frames(40, 0);
    CHECK(!combat_finished());
    scene_pop();                                          /* quit the fight */
    settle();
    CHECK(scene_top() == &SCENE_STORY);
    CHECK(!story_flag(SF_NEST0));
    CHECK_EQ(story_beat(), BEAT_C3_NESTS);
}
