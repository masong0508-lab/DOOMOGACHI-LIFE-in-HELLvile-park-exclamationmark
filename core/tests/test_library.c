/* test_library.c - the Grand Library (owner: library). */
#include "test.h"
#include "../src/lib_int.h"

DOOMO_STATIC_ASSERT(sizeof(LibWingData) + sizeof(LibBrowse) <= 3072, library_bss_budget);

static const DoomoScene *const wings[LIB_WING_COUNT] = {
    &SCENE_LIB_BESTIARY, &SCENE_LIB_ALMANAC, &SCENE_LIB_WEAPONS, &SCENE_LIB_ITEMS,
    &SCENE_LIB_TROPHIES, &SCENE_LIB_JUKEBOX, &SCENE_LIB_GALLERY
};

static void tick(uint32_t buttons)
{
    input_update(buttons);
    scene_update();
}

static void press(uint32_t b)
{
    tick(b);
    tick(0);
}

static void premade(void)
{
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    sim_init();
}

static int upper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static int name_le(const char *a, const char *b)
{
    while (*a && upper((unsigned char)*a) == upper((unsigned char)*b)) { a++; b++; }
    return upper((unsigned char)*a) <= upper((unsigned char)*b);
}

TEST(lib_activities_registered)
{
    const DoomoActivity *a = doomo_activities_collections, *out[16];
    int n, lobby = 0;
    CHECK(a != NULL);
    for (n = 0; a[n].name; n++) {
        CHECK(a[n].location == ACTLOC_LIBRARY);
        CHECK(a[n].scene != NULL);
        CHECK(strlen(a[n].desc) <= 40);
        CHECK(a[n].flags & ACTF_FREEPLAY);
        if (a[n].scene == &SCENE_LIBRARY) {
            lobby = 1;
            CHECK(a[n].flags & ACTF_STORY);
            CHECK_EQ(a[n].unlock_id, ACT_ALWAYS);
        } else {
            CHECK(!(a[n].flags & ACTF_STORY));        /* story shows the lobby only */
        }
    }
    CHECK(lobby);
    CHECK_EQ(n, 1 + LIB_WING_COUNT);
    /* free play: the lobby and the five books that are always open */
    CHECK_EQ(activities_at(ACTLOC_LIBRARY, out, 16), 6);
    unlock_grant(LIB_UL_JUKEBOX);
    unlock_grant(LIB_UL_GALLERY);
    CHECK_EQ(activities_at(ACTLOC_LIBRARY, out, 16), 8);
    /* story: just the lobby, from chapter 1 */
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = 0;
    CHECK_EQ(activities_at(ACTLOC_LIBRARY, out, 16), 0);
    doomo_town.story.chapter = 1;
    CHECK_EQ(activities_at(ACTLOC_LIBRARY, out, 16), 1);
    CHECK(out[0]->scene == &SCENE_LIBRARY);
}

TEST(lib_scenes_by_name)
{
    int w;
    CHECK(scene_by_name("LIBRARY") == &SCENE_LIBRARY);
    CHECK(scene_by_name("SCENE_LIB_BESTIARY") == &SCENE_LIB_BESTIARY);
    for (w = 0; w < LIB_WING_COUNT; w++) {
        CHECK(lib_wings[w].scene == wings[w]);
        CHECK(scene_by_name(wings[w]->name) == wings[w]);
        CHECK(lib_wings[w].title && lib_wings[w].name && lib_wings[w].blurb);
    }
}

TEST(lib_unlock_table_is_sane)
{
    const DoomoUnlock *u = doomo_unlocks_library;
    int n, i, total = unlock_count();
    for (n = 0; u[n].name; n++) {
        CHECK(u[n].id >= LIB_UL_FIRST && u[n].id < LIB_UL_VISITED(0));
        CHECK(u[n].how && u[n].how[0]);
        CHECK(unlock_by_id(u[n].id) == &u[n]);
        /* nobody else gates the same thing (unlock_find would pick one) */
        CHECK_MSG(unlock_find(u[n].cat, u[n].list, u[n].index) == &u[n], "%s shares its target", u[n].name);
        for (i = 0; i < total; i++) {
            const DoomoUnlock *o = unlock_at(i);
            if (o == &u[n]) continue;
            CHECK_MSG(o->id != u[n].id, "id %d also used by %s", u[n].id, o->name);
            CHECK_MSG(!(o->cat == u[n].cat && o->list == u[n].list && o->index == u[n].index),
                      "%s and %s gate the same thing", u[n].name, o->name);
        }
    }
    CHECK_EQ(n, LIB_UL_VISITED(0) - LIB_UL_FIRST);
    /* the "wing visited" bits are raw: not in any table */
    for (i = 0; i < LIB_WING_COUNT; i++) CHECK(unlock_by_id(LIB_UL_VISITED(i)) == NULL);
    /* the two wings are real game modes */
    CHECK(!unlock_mode_ok(MODE_JUKEBOX));
    progress_add(PROG_DAYS_LIVED, 1);
    CHECK(unlock_mode_ok(MODE_JUKEBOX));
    CHECK(!unlock_mode_ok(MODE_GALLERY));
    progress_add(PROG_DAYS_LIVED, 2);
    CHECK(unlock_mode_ok(MODE_GALLERY));
}

TEST(lib_mark_seen_grants_once)
{
    int t;
    CHECK_EQ(library_seen_count(), 0);
    CHECK(!library_mark_seen(THING_NPC));
    CHECK(!library_mark_seen(THING_HEALTH_SMALL));
    CHECK(!library_mark_seen(-5));
    CHECK(library_mark_seen(THING_IMPLING));
    CHECK(ui_toast_pending() >= 1);
    CHECK(!library_mark_seen(THING_IMPLING));
    CHECK(library_seen(THING_IMPLING));
    CHECK(!library_seen(THING_BRUTE));
    CHECK_EQ(library_seen_count(), 1);
    CHECK(unlock_has(LIB_UL_SEEN(THING_IMPLING)));
    CHECK(!unlock_has(LIB_UL_DEMONOLOGIST));
    for (t = THING_IMPLING; t <= THING_LORD_VAPULA; t++) library_mark_seen(t);
    CHECK_EQ(library_seen_count(), LIBRARY_DEMONS);
    CHECK(unlock_has(LIB_UL_DEMONOLOGIST));
    CHECK_EQ(library_wing_pct(LIB_WING_BESTIARY), 100);
}

TEST(lib_defeated_demons_count_as_seen)
{
    int t;
    CHECK(!library_defeated(THING_GARGOYLE));
    /* what combat.c does when a demon dies */
    CHECK(unlock_grant_match(UL_CAT_GALLERY, GAL_DEMONS, THING_GARGOYLE - THING_IMPLING));
    CHECK(library_defeated(THING_GARGOYLE));
    CHECK(library_seen(THING_GARGOYLE));
    CHECK(!unlock_has(LIB_UL_SEEN(THING_GARGOYLE)));
    /* opening the Bestiary copies the sighting into the library's own bit */
    scene_set(&SCENE_LIB_BESTIARY, 0);
    CHECK(unlock_has(LIB_UL_SEEN(THING_GARGOYLE)));
    CHECK(!unlock_has(LIB_UL_FIELD));
    for (t = THING_IMPLING; t <= THING_LORD_VAPULA; t++)
        unlock_grant_match(UL_CAT_GALLERY, GAL_DEMONS, t - THING_IMPLING);
    scene_set(&SCENE_LIB_BESTIARY, 0);
    CHECK(unlock_has(LIB_UL_FIELD));
    CHECK(unlock_has(LIB_UL_DEMONOLOGIST));
    CHECK_EQ(lib_b.count, LIBRARY_DEMONS);
}

TEST(lib_completion_in_range)
{
    int pass, w;
    for (pass = 0; pass < 2; pass++) {
        if (pass) premade();
        for (w = 0; w < LIB_WING_COUNT; w++) {
            int p = library_wing_pct(w);
            CHECK_MSG(p >= 0 && p <= 100, "wing %d: %d%%", w, p);
        }
        CHECK(library_total_pct() >= 0 && library_total_pct() <= 100);
    }
    CHECK_EQ(library_wing_pct(LIB_WING_COUNT), -1);
    /* locked wings are 0% until they open */
    CHECK_EQ(library_wing_pct(LIB_WING_JUKEBOX), 0);
    CHECK(!library_wing_open(LIB_WING_JUKEBOX));
    unlock_grant(LIB_UL_JUKEBOX);
    CHECK(library_wing_open(LIB_WING_JUKEBOX));
    CHECK_EQ(library_wing_pct(LIB_WING_JUKEBOX), 100);       /* free play: every song */
    unlock_grant_all();
    CHECK_EQ(library_wing_pct(LIB_WING_TROPHIES), 100);
    CHECK_EQ(library_wing_pct(LIB_WING_WEAPONS), 100);
    CHECK(unlock_has(LIB_UL_GUN_NERD));
}

TEST(lib_almanac_known_citizens)
{
    int base;
    premade();
    base = lib_almanac_known();
    CHECK(!lib_known(300) || base > 0);
    town_res(300)->level = 0;
    town_res(300)->flags &= (uint8_t)~RF_FAVORITE;
    base = lib_almanac_known();
    CHECK(!lib_known(300));
    rel_adjust(300, DOOMO_PLAYER_ID, 12);
    CHECK(lib_known(300));
    CHECK_EQ(lib_almanac_known(), base + 1);
    town_res(300)->level = 5;                           /* counted once */
    CHECK_EQ(lib_almanac_known(), base + 1);
    town_res(700)->level = 3;
    CHECK(lib_known(700));
    CHECK_EQ(lib_almanac_known(), base + (lib_known(700) ? 2 : 1));
    CHECK(!lib_known(5000));
}

TEST(lib_almanac_shelves_sort_and_search)
{
    int i, n;
    premade();
    scene_set(&SCENE_LIB_ALMANAC, 0);
    tick(0);
    CHECK_EQ(lib_b.count, 1000);
    press(BTN_R);                                       /* Hillvile */
    CHECK_EQ(lib_b.count, 500);
    for (i = 0; i < lib_b.count; i++) CHECK(lib_w.alm.idx[i] < 500);
    press(BTN_R);                                       /* Hellvile */
    CHECK_EQ(lib_b.count, 500);
    for (i = 0; i < lib_b.count; i++) CHECK(lib_w.alm.idx[i] >= 500);
    press(BTN_R);                                       /* story cast */
    CHECK_EQ(lib_b.count, NPC_COUNT);
    press(BTN_L);
    press(BTN_L);
    press(BTN_L);                                       /* everyone */
    CHECK_EQ(lib_b.count, 1000);
    press(BTN_SELECT);                                  /* sort by name */
    n = lib_b.count;
    for (i = 1; i < n; i++)
        CHECK_MSG(name_le(town_res(lib_w.alm.idx[i - 1])->name, town_res(lib_w.alm.idx[i])->name),
                  "%s before %s", town_res(lib_w.alm.idx[i - 1])->name, town_res(lib_w.alm.idx[i])->name);
    /* search by letter: START, pick 'B' (one step right of A... the picker
     * starts on "All", so go to A first) */
    press(BTN_START);
    CHECK(lib_w.alm.picking);
    press(BTN_RIGHT);                                   /* All -> A */
    press(BTN_RIGHT);                                   /* B */
    press(BTN_A);
    CHECK(!lib_w.alm.picking);
    CHECK_EQ(lib_w.alm.letter, 'B');
    CHECK(lib_b.count > 0 && lib_b.count < 1000);
    for (i = 0; i < lib_b.count; i++) CHECK(upper((unsigned char)town_res(lib_w.alm.idx[i])->name[0]) == 'B');
    press(BTN_B);                                       /* clears the search */
    CHECK_EQ(lib_w.alm.letter, 0);
    CHECK_EQ(lib_b.count, 1000);
    CHECK(scene_top() == &SCENE_LIB_ALMANAC);
}

TEST(lib_items_tally_and_gourmet)
{
    int f, love = 0, hate = 0, it;
    premade();
    scene_set(&SCENE_LIB_ITEMS, 0);
    for (f = 0; f < FOOD_COUNT; f++) { love += lib_w.item.love[f]; hate += lib_w.item.hate[f]; }
    CHECK_EQ(love, town_count(-1) - town_count(RES_DEAD));
    CHECK_EQ(hate, love);
    CHECK(!unlock_has(LIB_UL_GOURMET));
    for (it = 1; it < ITEM_COUNT; it++)
        if (doomo_items[it].category == ICAT_FOOD) inv_add(it, 1);
    CHECK(library_wing_pct(LIB_WING_ITEMS) > 0);
    CHECK(unlock_has(LIB_UL_GOURMET));
    /* the bag filter splits the shelf */
    {
        int all, in, out;
        tick(0);
        all = lib_b.count;
        press(BTN_SELECT);
        in = lib_b.count;
        press(BTN_SELECT);
        out = lib_b.count;
        CHECK_EQ(in + out, all);
        CHECK(in >= 12);
    }
}

TEST(lib_trophy_filters_partition)
{
    int all, won, to_win, i, prev;
    unlock_grant(LIB_UL_CARD);
    progress_add(PROG_DEMONS_KILLED, 30);
    scene_set(&SCENE_LIB_TROPHIES, 0);
    tick(0);
    all = lib_b.count;
    CHECK_EQ(all, unlock_count());
    press(BTN_SELECT);
    won = lib_b.count;
    press(BTN_SELECT);
    to_win = lib_b.count;
    CHECK_EQ(won + to_win, all);
    CHECK_EQ(won, unlock_owned_count());
    press(BTN_SELECT);                      /* almost there: closest first */
    CHECK(lib_b.count > 0);
    prev = 1001;
    for (i = 0; i < lib_b.count; i++) {
        const DoomoUnlock *u = unlock_at(lib_w.tro.idx[i]);
        int pm = (int)((uint64_t)DOOMO_MIN(progress_get(u->prog), u->need) * 1000u / u->need);
        CHECK(u->need > 0 && !unlock_has(u->id));
        CHECK(pm <= prev);
        prev = pm;
    }
    /* the stats page lists every counter */
    press(BTN_L);
    CHECK_EQ(lib_b.count, PROG_COUNT);
}

TEST(lib_weapon_catalog_lists)
{
    int tab;
    scene_set(&SCENE_LIB_WEAPONS, 0);
    for (tab = 0; tab <= CLIST_WCHARM - CLIST_WFRAME; tab++) {
        int all, ok, locked;
        tick(0);
        all = lib_b.count;
        CHECK_EQ(all, weld_part_count(CLIST_WFRAME + tab));
        press(BTN_SELECT);
        ok = lib_b.count;
        press(BTN_SELECT);
        locked = lib_b.count;
        press(BTN_SELECT);
        CHECK_EQ(ok + locked, all);
        press(BTN_R);
    }
    /* the detail page opens, flips and closes */
    press(BTN_A);
    CHECK(lib_b.detail);
    press(BTN_RIGHT);
    CHECK_EQ(lib_b.flip, 1);
    press(BTN_B);
    CHECK(!lib_b.detail);
}

TEST(lib_jukebox_plays_and_restores_music)
{
    int i;
    scene_set(&SCENE_LIBRARY, 0);
    audio_music(SONG_SHOP);
    scene_push(&SCENE_LIB_JUKEBOX, LIB_ARG_DEMO);
    tick(0);
    CHECK_EQ(lib_b.count, SONG_COUNT - 1);
    CHECK_EQ(lib_b.sel, SONG_SHOP - 1);            /* starts on what's playing */
    press(BTN_UP);
    press(BTN_A);
    CHECK_EQ(audio_music_current(), SONG_SHOP - 1);
    CHECK(!lib_b.detail);
    press(BTN_A);                                   /* again: stop */
    CHECK_EQ(audio_music_current(), SONG_NONE);
    for (i = 0; i < 10; i++) {
        press(BTN_DOWN);
        press(BTN_A);
    }
    CHECK(unlock_has(LIB_UL_DJ));
    press(BTN_R);                                   /* sound effects */
    CHECK_EQ(lib_b.count, SFX_COUNT - 1);
    press(BTN_A);
    press(BTN_B);
    CHECK_EQ(audio_music_current(), SONG_SHOP);
}

TEST(lib_gallery_rooms_and_critic)
{
    scene_set(&SCENE_LIB_GALLERY, LIB_ARG_DEMO);
    tick(0);
    CHECK_EQ(lib_b.count, TEX_COUNT - 1);
    CHECK(!unlock_has(LIB_UL_CRITIC));
    press(BTN_R);
    CHECK_EQ(lib_b.count, SKY_COUNT);
    press(BTN_R);
    CHECK_EQ(lib_b.count, SPR_COUNT - 1);
    press(BTN_R);
    CHECK_EQ(lib_b.count, 0);                       /* the palette's own cursor */
    CHECK(unlock_has(LIB_UL_CRITIC));
    press(BTN_RIGHT);
    CHECK_EQ(lib_w.gal.pal_x, 7);
}

TEST(lib_lobby_milestones)
{
    int w;
    scene_set(&SCENE_LIBRARY, 0);
    CHECK(unlock_has(LIB_UL_CARD));
    CHECK(!unlock_has(LIB_UL_BOOKWORM));
    CHECK_EQ(town_is_night(), unlock_has(LIB_UL_NIGHT_OWL));
    /* a locked book only rattles */
    for (w = 0; w < LIB_WING_JUKEBOX; w++) press(BTN_RIGHT);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_LIBRARY);
    /* the first sighting opens the jukebox */
    library_mark_seen(THING_IMPLING);
    scene_set(&SCENE_LIBRARY, 0);
    CHECK(unlock_has(LIB_UL_JUKEBOX));
    for (w = 0; w < LIB_WING_COUNT; w++) {
        scene_push(wings[w], LIB_ARG_DEMO);
        CHECK(library_wing_visited(w));
        press(BTN_B);
        CHECK(scene_top() == &SCENE_LIBRARY);
    }
    CHECK(unlock_has(LIB_UL_BOOKWORM));
    press(BTN_B);
}

/* Every screen, every page, both window sizes' code paths: no crash, the
 * scene stays up, and a frame never leaves the framebuffer untouched. */
TEST(lib_every_page_draws)
{
    int w, k;
    premade();
    library_mark_seen(THING_IMPLING);
    library_mark_seen(THING_LORD_VAPULA);
    scene_set(&SCENE_LIBRARY, LIB_ARG_DEMO);
    for (k = 0; k < 20; k++) { tick(0); scene_draw(); }
    for (w = 0; w < LIB_WING_COUNT; w++) {
        int tab;
        scene_push(wings[w], LIB_ARG_DEMO);
        for (tab = 0; tab < 4; tab++) {
            for (k = 0; k < 6; k++) { tick(0); scene_draw(); }
            press(BTN_DOWN);
            press(BTN_A);                   /* open (or play) */
            for (k = 0; k < 16; k++) { tick(0); scene_draw(); }
            press(BTN_R);                   /* sub page */
            press(BTN_SELECT);              /* toggle */
            press(BTN_RIGHT);               /* next entry */
            for (k = 0; k < 12; k++) { tick(0); scene_draw(); }
            if (lib_b.detail) press(BTN_B);
            press(BTN_R);                   /* next category */
            scene_draw();
            CHECK(scene_top() == wings[w]);
        }
        press(BTN_B);
        CHECK(scene_top() == &SCENE_LIBRARY);
    }
    CHECK(library_wing_visited(LIB_WING_GALLERY));
}
