/* test_integration.c - cross-module consistency (owner: integration).
 *
 * Every team registers into shared registries (unlock tables, activities,
 * save extension chunks, the scene table) and allocates from shared id
 * spaces (unlock ids, UL_CAT_MISC list ids, PROG_* counters). These tests
 * check ALL of them together against doomo/unlock_ids.h, so a collision
 * between two teams fails here instead of silently breaking an unlock.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/unlocks.h"
#include "doomo/unlock_ids.h"
#include "doomo/catalog.h"
#include "doomo/weld.h"
#include "doomo/activities.h"
#include "doomo/save_ext.h"
#include "doomo/scenes.h"
#include "doomo/library.h"

/* doomo_unlock_tables[] slot -> owning team's id range (unlock_ids.h) */
static const struct { const char *team; int first, last; } slot_range[] = {
    { "foundation", UL_IDS_FOUNDATION_FIRST, UL_IDS_FOUNDATION_LAST },
    { "creator",    UL_IDS_CREATOR_FIRST,    UL_IDS_CREATOR_LAST },
    { "life",       UL_IDS_LIFE_FIRST,       UL_IDS_LIFE_LAST },
    { "action",     UL_IDS_ACTION_FIRST,     UL_IDS_ACTION_LAST },
    { "story",      UL_IDS_STORY_FIRST,      UL_IDS_STORY_LAST },
    { "freeplay",   UL_IDS_FREEPLAY_FIRST,   UL_IDS_FREEPLAY_LAST },
    { "art",        1, 0 },                 /* no unlockables */
    { "portrait",   1, 0 },
    { "roster",     1, 0 },
    { "minigames",  UL_IDS_MINIGAMES_FIRST,  UL_IDS_MINIGAMES_LAST },
    { "concert",    UL_IDS_CONCERT_FIRST,    UL_IDS_CONCERT_LAST },
    { "studio",     UL_IDS_STUDIO_FIRST,     UL_IDS_STUDIO_LAST },
    { "library",    UL_IDS_LIBRARY_FIRST,    UL_IDS_LIBRARY_LAST },
    { "fishcook",   UL_IDS_FISHCOOK_FIRST,   UL_IDS_FISHCOOK_LAST },
    { "levels",     UL_IDS_LEVELS_FIRST,     UL_IDS_LEVELS_LAST },
    { "home",       UL_IDS_HOME_FIRST,       UL_IDS_HOME_LAST },    /* S0 (SPEC R7) */
};
#define SLOTS ((int)(sizeof(slot_range) / sizeof(slot_range[0])))

/* MISC list ids a team may use (unlock_ids.h); -1 = no MISC entries */
static int misc_list_ok(int slot, int list)
{
    switch (slot) {
    case 2:  return list == UL_MISC_LIFE;
    case 3:  return list == UL_MISC_ACTION;
    case 4:  return list == UL_MISC_STORY;
    case 5:  return list == UL_MISC_FREEPLAY;
    case 9:  return list == UL_MISC_MINIGAMES;
    case 10: return list == UL_MISC_CONCERT;
    case 11: return list >= UL_MISC_STUDIO && list <= UL_MISC_STUDIO + 3;
    case 12: return list == UL_MISC_LIBRARY;
    case 13: return list == 0xC0 || list == 0xC1 || list == 0xF1 || list == 0xF2;
    case 14: return list == UL_MISC_LEVELS;
    case 15: return list == UL_MISC_HOME;          /* S0 */
    default: return 0;
    }
}

/* option count of each creator list (CLIST_SPECIES .. CLIST_PATTERN) */
static int clist_count(int list)
{
    static const int n[CLIST_WFRAME] = {
        SPECIES_COUNT, BODY_COUNT, HEAD_COUNT, EYE_COUNT, BROW_COUNT,
        NOSE_COUNT, MOUTH_COUNT, EARS_COUNT, HAIR_COUNT, FHAIR_COUNT,
        HORNS_COUNT, TAIL_COUNT, WINGS_COUNT, HALO_COUNT, MARK_COUNT,
        TOP_COUNT, BOTTOM_COUNT, SHOES_COUNT, HAT_COUNT, ACC_COUNT,
        AURA_COUNT, PATTERN_COUNT
    };
    return (list >= 0 && list < CLIST_WFRAME) ? n[list] : 0;
}

static int same_key(const DoomoUnlock *a, const DoomoUnlock *b)
{
    return a->cat == b->cat && a->list == b->list && a->index == b->index;
}

/* ---- unlock ids ------------------------------------------------------------ */
TEST(integ_unlock_ids_unique_across_all_tables)
{
    static int16_t owner[DOOMO_UNLOCK_BITS];
    int t, i, total = 0;
    CHECK_EQ(SLOTS, 16);
    for (i = 0; i < DOOMO_UNLOCK_BITS; i++) owner[i] = -1;
    for (t = 0; doomo_unlock_tables[t]; t++) {
        const DoomoUnlock *u = doomo_unlock_tables[t];
        CHECK_MSG(t < SLOTS, "unlock table slot %d has no range in unlock_ids.h", t);
        for (i = 0; u[i].name; i++, total++) {
            int id = u[i].id;
            CHECK_MSG(id < DOOMO_UNLOCK_BITS, "%s: id %d >= %d", slot_range[t].team, id, DOOMO_UNLOCK_BITS);
            CHECK_MSG(owner[id] < 0, "unlock id %d used by %s (\"%s\") and %s", id,
                      owner[id] >= 0 ? slot_range[owner[id]].team : "?", u[i].name,
                      slot_range[t].team);
            owner[id] = (int16_t)t;
            CHECK_MSG(id >= slot_range[t].first && id <= slot_range[t].last,
                      "%s unlock \"%s\" id %d is outside its range %d..%d (unlock_ids.h)",
                      slot_range[t].team, u[i].name, id, slot_range[t].first, slot_range[t].last);
            CHECK_MSG(u[i].how && u[i].how[0], "unlock %d has no hint", id);
            CHECK(unlock_by_id(id) == &u[i]);
        }
    }
    CHECK_EQ(t, SLOTS);
    CHECK_EQ(unlock_count(), total);
    /* raw bits (no table entry) stay free */
    for (i = LIB_UL_VISITED(0); i <= LIB_UL_LAST; i++) CHECK_MSG(owner[i] < 0, "raw library bit %d has an entry", i);
    for (i = 944; i <= 959; i++) CHECK_MSG(owner[i] < 0, "dream journal bit %d has an entry", i);
}

TEST(integ_unlock_keys_unique_or_alternate_routes)
{
    int n = unlock_count(), i, j, alias = 0;
    for (i = 0; i < n; i++) {
        const DoomoUnlock *a = unlock_at(i);
        for (j = i + 1; j < n; j++) {
            const DoomoUnlock *b = unlock_at(j);
            if (!same_key(a, b)) continue;
            /* only creator / weapon parts may have alternate routes */
            CHECK_MSG(a->cat == UL_CAT_PART || a->cat == UL_CAT_WEAPON_PART,
                      "unlocks %d \"%s\" and %d \"%s\" share (cat %d, list %d, index %d)",
                      a->id, a->name, b->id, b->name, a->cat, a->list, a->index);
            alias++;
        }
    }
    CHECK_MSG(alias <= UL_ALIAS_MAX, "%d alternate-route part keys (max %d)", alias, UL_ALIAS_MAX);
}

TEST(integ_alternate_routes_each_unlock_the_part)
{
    int n = unlock_count(), i, j, routes = 0;
    for (i = 0; i < n; i++) {
        const DoomoUnlock *a = unlock_at(i);
        if (a->cat != UL_CAT_PART && a->cat != UL_CAT_WEAPON_PART) continue;
        for (j = 0; j < n; j++) {
            const DoomoUnlock *b = unlock_at(j);
            if (i == j || !same_key(a, b)) continue;
            /* own ONLY route a: the part must be available */
            memset(doomo_town.unlocks, 0, sizeof(doomo_town.unlocks));
            CHECK(!unlock_option_ok(a->list, a->index));
            doomo_town.unlocks[a->id >> 3] |= (uint8_t)(1u << (a->id & 7));
            CHECK_MSG(unlock_option_ok(a->list, a->index),
                      "owning \"%s\" (%d) does not unlock list %d index %d (alias of %d)",
                      a->name, a->id, a->list, a->index, b->id);
            CHECK(unlock_find(a->cat, a->list, a->index) == a);
            routes++;
        }
    }
    /* the concert's and the creator's routes to Sparkle Eyes, Star
     * Freckles, Platform Shoes and the Pompadour */
    CHECK(routes >= 8);
    memset(doomo_town.unlocks, 0, sizeof(doomo_town.unlocks));
    progress_set(PROG_CITIZENS_CREATED, 0);
    CHECK(!unlock_option_ok(CLIST_EYE, EYE_SPARKLE));
    unlock_grant(UL_IDS_CONCERT_FIRST + 7);           /* concert: hit single */
    CHECK(unlock_option_ok(CLIST_EYE, EYE_SPARKLE));
}

TEST(integ_part_unlocks_reference_valid_options)
{
    int n = unlock_count(), i;
    for (i = 0; i < n; i++) {
        const DoomoUnlock *u = unlock_at(i);
        if (u->cat == UL_CAT_PART) {
            CHECK_MSG(u->list < CLIST_WFRAME, "part unlock %d \"%s\" uses list %d (not a creator list)",
                      u->id, u->name, u->list);
            CHECK_MSG(u->index < clist_count(u->list), "part unlock %d \"%s\": index %d >= %d options",
                      u->id, u->name, u->index, clist_count(u->list));
        } else if (u->cat == UL_CAT_WEAPON_PART) {
            CHECK_MSG(u->list >= CLIST_WFRAME && u->list < CLIST_COUNT,
                      "weapon unlock %d \"%s\" uses list %d", u->id, u->name, u->list);
            CHECK_MSG(u->index < weld_part_count(u->list), "weapon unlock %d \"%s\": index %d >= %d",
                      u->id, u->name, u->index, weld_part_count(u->list));
        } else if (u->cat == UL_CAT_MODE) {
            CHECK_MSG(u->index < MODE_COUNT, "mode unlock %d: mode %d", u->id, u->index);
        } else if (u->cat == UL_CAT_GALLERY) {
            CHECK_MSG(u->list < GAL_COUNT, "gallery unlock %d: list %d", u->id, u->list);
        } else if (u->cat == UL_CAT_PAINT) {
            CHECK_MSG(u->index < 32, "paint unlock %d: paint %d", u->id, u->index);
        }
        CHECK_MSG(u->cat <= UL_CAT_MISC, "unlock %d: category %d", u->id, u->cat);
        CHECK_MSG(u->need == 0 || u->prog < PROG_COUNT, "unlock %d reads PROG %d", u->id, u->prog);
    }
}

TEST(integ_misc_unlocks_use_their_teams_list)
{
    int t, i;
    for (t = 0; doomo_unlock_tables[t] && t < SLOTS; t++) {
        const DoomoUnlock *u = doomo_unlock_tables[t];
        for (i = 0; u[i].name; i++)
            if (u[i].cat == UL_CAT_MISC)
                CHECK_MSG(misc_list_ok(t, u[i].list), "%s MISC unlock %d \"%s\" uses list 0x%02X (unlock_ids.h)",
                          slot_range[t].team, u[i].id, u[i].name, u[i].list);
    }
}

/* ---- PROG_* counters ---------------------------------------------------------- */
TEST(integ_progress_counters_fit_and_are_named)
{
    int i, j;
    CHECK(PROG_COUNT <= UL_PROG_SLOTS);
    CHECK_EQ(sizeof(doomo_town.progress) / sizeof(doomo_town.progress[0]), UL_PROG_SLOTS);
    for (i = 0; i < PROG_COUNT; i++) {
        CHECK_MSG(doomo_progress_names[i] && doomo_progress_names[i][0], "PROG %d has no name", i);
        for (j = 0; j < i; j++)
            CHECK_MSG(strcmp(doomo_progress_names[i], doomo_progress_names[j]) != 0,
                      "PROG %d and %d are both \"%s\"", j, i, doomo_progress_names[i]);
    }
    /* high-score slots are separate counters: setting one leaves the rest */
    progress_set(PROG_HS_CATCH, 1234);
    progress_add(PROG_ARENA_WAVES, 2);
    progress_add(PROG_CONCERTS, 1);
    CHECK_EQ(progress_get(PROG_HS_CATCH), 1234);
    CHECK_EQ(progress_get(PROG_HS_WHACK), 0);
    CHECK_EQ(progress_get(PROG_ARENA_WAVES), 2);
    CHECK_EQ(progress_get(PROG_CONCERTS), 1);
    CHECK_EQ(progress_get(PROG_HIT_SINGLES), 0);
}

/* ---- registries ---------------------------------------------------------------- */
static int scene_registered(const DoomoScene *s)
{
    int i;
    for (i = 0; doomo_all_scenes[i]; i++)
        if (doomo_all_scenes[i] == s) return 1;
    return 0;
}

TEST(integ_activities_registered)
{
    /* the finished modules' activities (fishcook / levels add more) */
    static const char *const want[] = {
        "Cupcake Catch", "Whack-an-Imp", "Memory Match", "Doomogachi Derby",
        "Bubble Blaster Gallery", "Vape Cloud Dodge", "Wheel of Fate",       /* minigames */
        "Concert", "Hit Parade", "Peek at a Dream",                           /* concert   */
        "Grand Library", "Bestiary", "Citizen Almanac", "Weapon Catalog",
        "Food & Item Dex", "Trophy Wall", "Jukebox", "Art Gallery",           /* library   */
        "Music Studio",                                                       /* studio    */
    };
    int n = activity_count(), i, k;
    CHECK_MSG(n >= 20, "only %d activities registered", n);
    for (k = 0; k < (int)(sizeof(want) / sizeof(want[0])); k++) {
        int found = 0;
        for (i = 0; i < n && !found; i++) found = strcmp(activity_at(i)->name, want[k]) == 0;
        CHECK_MSG(found, "activity \"%s\" is not registered", want[k]);
    }
    for (i = 0; i < n; i++) {
        const DoomoActivity *a = activity_at(i);
        CHECK_MSG(a->scene && scene_registered(a->scene), "activity \"%s\": scene not in doomo_all_scenes", a->name);
        CHECK_MSG(a->location < ACTLOC_COUNT, "activity \"%s\": location %d", a->name, a->location);
        CHECK_MSG(a->unlock_id == ACT_ALWAYS || unlock_by_id(a->unlock_id),
                  "activity \"%s\" waits for unlock %d, which no table defines", a->name, a->unlock_id);
        CHECK_MSG(a->desc && strlen(a->desc) <= 40, "activity \"%s\": description", a->name);
    }
    CHECK(activity_at(n) == NULL);
}

TEST(integ_save_ext_chunks_registered)
{
    static const uint32_t want[] = {
        SAVE_EXT_TAG('S', 'O', 'N', 'G'),     /* studio song book  */
        SAVE_EXT_TAG('N', 'I', 'C', 'K'),     /* creator nicknames */
        SAVE_EXT_TAG('F', 'P', 'L', 'Y'),     /* free play island  */
    };
    int n = save_ext_count(), i, j, k;
    CHECK_MSG(n >= 3, "only %d save chunks registered", n);
    for (k = 0; k < 3; k++) {
        int found = 0;
        for (i = 0; i < n; i++) found |= save_ext_at(i)->tag == want[k];
        CHECK_MSG(found, "save chunk %.4s is not registered", (const char *)&want[k]);
    }
    for (i = 0; i < n; i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        CHECK(e && e->size && e->write && e->read && e->reset);
        CHECK(e->size() <= DOOMO_SAVE_EXT_MAX);
        for (j = 0; j < i; j++) CHECK_MSG(save_ext_at(j)->tag != e->tag, "save chunk tag %.4s twice",
                                          (const char *)&e->tag);
    }
    CHECK(save_ext_at(n) == NULL);
}

TEST(integ_scene_table_complete_and_unique)
{
    int i, j;
    for (i = 0; doomo_all_scenes[i]; i++) {
        const DoomoScene *s = doomo_all_scenes[i];
        CHECK_MSG(s->name && s->name[0], "scene %d has no name", i);
        CHECK_MSG(scene_by_name(s->name) == s, "scene_by_name(%s)", s->name);
        for (j = 0; j < i; j++) CHECK_MSG(doomo_all_scenes[j] != s && strcmp(doomo_all_scenes[j]->name, s->name),
                                          "scene %s registered twice", s->name);
    }
    CHECK_MSG(i >= 47, "only %d scenes registered", i);
}

/* No finished module's scene may still be the weak stub (stubs.c). */
TEST(integ_finished_scenes_are_not_stubs)
{
    int i;
    for (i = 0; doomo_all_scenes[i]; i++) {
        const DoomoScene *s = doomo_all_scenes[i];
        /* still being written by other teams */
        if (s == &SCENE_FISHING || s == &SCENE_COOKING || s == &SCENE_LEVEL_SELECT ||
            s == &SCENE_BREACH || s == &SCENE_ARENA_MODE) continue;
        /* --- S0: declared by the story/house SPEC; H4 / S2 write them --- */
        if (s == &SCENE_HOME || s == &SCENE_HOME_MART || s == &SCENE_STORY_MG ||
            s == &SCENE_STORY_TIMING) continue;
        /* --- end S0 --- */
        CHECK_MSG(!scene_is_stub(s), "SCENE_%s is still the weak stub", s->name);
    }
}
