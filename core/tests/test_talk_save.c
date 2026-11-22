/* test_talk_save.c - T6: talk memory and the 'TALK' save chunk
 * (core/src/talk_save.c). Owner: TK. docs/TALK.md 6e; design 6.8.
 *
 * The table (find, touch, the last two topics, the LRU, likes, who may
 * have an entry), the day rollover (refused_today only), the flags of a
 * running conversation (seen from the next one on), the chunk (empty: 0
 * bytes; the worst case; a full save round trip; an old save; truncated,
 * too long, wrong version, a count over the cap; a removed resident, the
 * pet, a double id and a retired concept in a loaded chunk), and the
 * generator's memory guards and callback through the test grammar
 * (core/tests/talk_gen/mem.tlk), K-5 across two conversations included.
 */
#include "test.h"
#include <stdlib.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/roster.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/save.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/sim.h"

#define ID_NANA  1
#define ID_RIVET 13
#define ID_VEX   666

static uint8_t *gentest;
static uint32_t gentest_size;

static int open_gentest(void)
{
    if (!gentest) {
        FILE *f = fopen(NAT_TALK_DIR "/TALK_gentest.BIN", "rb");
        long n;
        if (!f) return 0;
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        gentest = (uint8_t *)malloc((size_t)n);
        if (!gentest || fread(gentest, 1, (size_t)n, f) != (size_t)n) { fclose(f); return 0; }
        fclose(f);
        gentest_size = (uint32_t)n;
    }
    return talk_db_open_mem(gentest, gentest_size) == TALK_DB_OK && talk_gen_ready();
}

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

TEST(talk_links_keep_a_rehearsed_pair_and_drop_a_departed_avatar)
{
    int avatar, replacement, i;
    premade_town();
    avatar = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 7u, "Sam", -1);
    CHECK(avatar >= FAMILY_POOL_FIRST);
    talk_link_note(ID_NANA, avatar, 1, 2);
    talk_link_note(ID_NANA, avatar, 2, 1);
    for (i = 3; i < 3 + (int)SAVE_TLNK_FREE_LINKS * 2; i++)
        talk_link_note(ID_NANA, avatar, 1, (uint16_t)i);
    CHECK_EQ(talk_link_weight(ID_NANA, avatar, 1, 2), 2);
    CHECK_EQ(doomo_save_ext_tlnk->size(), SAVE_EXT_MAX_TLNK);
    CHECK(family_remove(avatar));
    replacement = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 13u, "Alex", -1);
    CHECK_EQ(replacement, avatar);
    CHECK_EQ(talk_link_weight(ID_NANA, replacement, 1, 2), 0);
    CHECK_EQ(doomo_save_ext_tlnk->size(), 0);
}

TEST(talk_archive_resident_reuse_does_not_inherit_memories)
{
    DoomoResident *replacement;
    TalkMemoryEvent event;
    premade_town();
    sim_init();
    doomo_town.day = 23;
    talk_mem_note_topic(ID_NANA, 12);
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    talk_visit_note(ID_RIVET, ID_NANA, ACT_CAFE);
    sim_news_rec(NT_TALK_MEET, ACT_PARK, NEWS_IDS(ID_NANA, DOOMO_PLAYER_ID, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET_FAV, ACT_CAFE, NEWS_IDS(ID_RIVET, ID_NANA, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET, ACT_SHOP, NEWS_IDS(ID_RIVET, DOOMO_PLAYER_ID, NEWS_NONE));
    talk_event_note(TALK_EV_PRAISE, ID_NANA, DOOMO_PLAYER_ID, TALK_PLACE_HOME);
    talk_event_note(TALK_EV_HURT, ID_RIVET, ID_NANA, ACT_CAFE + 1);
    talk_event_note(TALK_EV_ARGUMENT, ID_NANA, ID_RIVET, ACT_PARK + 1);
    talk_event_note(TALK_EV_PRAISE, ID_RIVET, DOOMO_PLAYER_ID, TALK_PLACE_HOME);
    talk_event_note(TALK_EV_BATTLE_WON, -1, -1, TALK_PLACE_NEST_PLAZA);

    town_free(ID_NANA);
    CHECK(town_bind(ID_NANA) >= 0);
    replacement = town_res(ID_NANA);
    resident_generate(replacement, 123u);
    CHECK(town_alive(ID_NANA));
    CHECK_EQ(talk_event_page(ID_NANA, DOOMO_PLAYER_ID, 0, NULL, 3), 0);
    CHECK(talk_mem_find(ID_NANA) == NULL);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, 1), 0);
    CHECK_EQ(talk_visit_page(ID_RIVET, ID_NANA, 0, 0, NULL, 1), 0);
    CHECK_EQ(talk_event_page(ID_RIVET, ID_NANA, 0, NULL, 3), 0);

    /* With an empty visit archive, the old newspaper importer runs on load. */
    CHECK(save_game(1));
    CHECK(load_game(1));
    sim_init();
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, 1), 0);
    CHECK_EQ(talk_visit_page(ID_RIVET, ID_NANA, 0, 0, NULL, 1), 0);
    CHECK_EQ(talk_visit_page(ID_RIVET, DOOMO_PLAYER_ID, 0, 0, NULL, 1), 1);
    CHECK_EQ(talk_event_page(ID_RIVET, DOOMO_PLAYER_ID, 0, &event, 2), 1);
    CHECK_EQ(event.kind, TALK_EV_PRAISE);
    CHECK_EQ(talk_event_page(-1, DOOMO_PLAYER_ID, 0, &event, 1), 1);
    CHECK_EQ(event.kind, TALK_EV_BATTLE_WON);
}

TEST(talk_archive_pool_reuse_does_not_inherit_memories)
{
    int avatar, replacement;
    premade_town();
    avatar = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 7u, "Sam", -1);
    CHECK(avatar >= FAMILY_POOL_FIRST);
    talk_mem_note_topic(avatar, 12);
    talk_visit_note(ID_NANA, avatar, ACT_PARK);
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_CAFE);
    talk_event_note(TALK_EV_PRAISE, ID_NANA, avatar, TALK_PLACE_HOME);
    talk_event_note(TALK_EV_PRAISE, ID_NANA, DOOMO_PLAYER_ID, TALK_PLACE_HOME);

    CHECK(family_remove(avatar));
    replacement = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 13u, "Alex", -1);
    CHECK_EQ(replacement, avatar);
    CHECK_EQ(talk_visit_page(ID_NANA, replacement, 0, 0, NULL, 1), 0);
    CHECK_EQ(talk_event_page(ID_NANA, replacement, 0, NULL, 1), 0);
    CHECK(talk_mem_find(replacement) == NULL);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, 2), 1);
    CHECK_EQ(talk_event_page(ID_NANA, DOOMO_PLAYER_ID, 0, NULL, 2), 1);
}

TEST(talk_archive_elder_reuse_starts_recall_at_the_new_resident)
{
    DoomoResident *replacement;
    unsigned day;
    test_reset();
    town_new(TOWN_FREEPLAY, 10, TOWN_POP_PREMADE);
    RES_SET_STAGE(town_res(ID_NANA), LIFE_ELDER);
    opt_set(OPT_MEMORY_LAPSES, 1);
    doomo_town.day = 1700;
    CHECK_EQ(talk_visit_recall(ID_NANA, 0), TALK_RECALL_CLEAR);
    for (day = 1868; day < 1920; day++) {
        doomo_town.day = day;
        if (talk_visit_recall(ID_NANA, 0) == TALK_RECALL_LAPSE) break;
    }
    CHECK(day < 1920);

    town_free(ID_NANA);
    CHECK(town_bind(ID_NANA) >= 0);
    replacement = town_res(ID_NANA);
    resident_generate(replacement, 123u);
    RES_SET_STAGE(replacement, LIFE_ELDER);
    CHECK_EQ(talk_visit_recall(ID_NANA, 0), TALK_RECALL_CLEAR);
}

TEST(talk_archive_keeps_a_tombstoned_residents_history)
{
    TalkMemoryEvent event;
    premade_town();
    sim_init();
    CHECK(family_add(ID_RIVET, FAMROLE_HOUSEMATE, LIFE_ADULT) >= 0);
    talk_visit_note(ID_NANA, ID_RIVET, ACT_PARK);
    talk_event_note(TALK_EV_FRIENDSHIP, ID_NANA, ID_RIVET, ACT_PARK + 1);

    sim_lay_to_rest(ID_RIVET, DEATH_OLD_AGE);
    CHECK_EQ(town_res(ID_RIVET)->status, RES_DEAD);
    CHECK(family_find(ID_RIVET) == NULL);
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(talk_visit_page(ID_NANA, ID_RIVET, 0, 0, NULL, 1), 1);
    CHECK_EQ(talk_event_page(ID_NANA, DOOMO_PLAYER_ID, 0, &event, 1), 1);
    CHECK_EQ(event.kind, TALK_EV_FRIENDSHIP);
    CHECK_EQ(event.other, ID_RIVET);
}

static int reserve_meetup(int avatar, int guest, int venue)
{
    TalkSummary sum = {0};
    TalkOutcome out;
    sum.avatar = avatar;
    sum.n = sum.nev = 1;
    sum.id[0] = (int16_t)guest;
    sum.ev[0].kind = TEV_ACT_INVITE;
    sum.ev[0].b = (uint8_t)venue;
    town_res(guest)->activity = ACT_HOME;
    talk_outcome_apply(&sum, &out);
    return out.invited;
}

TEST(talk_meetup_does_not_pass_to_a_replacement_guest)
{
    premade_town();
    doomo_town.minute = 12 * 60;
    sim_init();
    CHECK(reserve_meetup(DOOMO_PLAYER_ID, ID_NANA, ACT_CAFE));
    CHECK(reserve_meetup(DOOMO_PLAYER_ID, ID_RIVET, ACT_PARK));
    CHECK(reserve_meetup(DOOMO_PLAYER_ID, ID_VEX, ACT_PARK));

    town_free(ID_NANA);
    CHECK(town_bind(ID_NANA) >= 0);
    resident_generate(town_res(ID_NANA), 123u);
    town_res(ID_NANA)->activity = ACT_CAFE;
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    CHECK_EQ(talk_outcome_meet_plan(ID_RIVET), ACT_PARK);
    CHECK(town_alive(42));
    CHECK(!talk_outcome_can(TEV_ACT_INVITE, 42));
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
    CHECK_EQ(talk_outcome_meet_plan(ID_RIVET), ACT_PARK);
    CHECK(!talk_outcome_can(TEV_ACT_INVITE, 42));
}

TEST(talk_meetup_does_not_pass_to_a_replacement_avatar)
{
    int avatar, replacement;
    premade_town();
    doomo_town.minute = 12 * 60;
    sim_init();
    avatar = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 7u, "Sam", -1);
    CHECK(avatar >= FAMILY_POOL_FIRST);
    CHECK(reserve_meetup(avatar, ID_NANA, ACT_CAFE));

    CHECK(family_remove(avatar));
    replacement = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 13u, "Alex", -1);
    CHECK_EQ(replacement, avatar);
    playas_new_life_arm();
    CHECK(playas_switch(replacement, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    CHECK(!talk_outcome_can(TEV_ACT_INVITE, ID_NANA));
    CHECK(talk_outcome_can(TEV_ACT_INVITE, ID_RIVET));
}

TEST(talk_meetup_load_clears_a_pending_reservation_for_a_vacant_guest)
{
    premade_town();
    doomo_town.minute = 12 * 60;
    sim_init();
    CHECK(reserve_meetup(DOOMO_PLAYER_ID, ID_NANA, ACT_CAFE));
    town_res(ID_NANA)->status = RES_EMPTY;    /* A save made before release cleared reservations. */
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK(town_bind(ID_NANA) >= 0);
    resident_generate(town_res(ID_NANA), 123u);
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
}

TEST(talk_visit_legacy_news_cannot_attach_to_a_replacement_resident)
{
    premade_town();
    sim_init();
    sim_news_rec(NT_TALK_MEET, ACT_CAFE, NEWS_IDS(ID_NANA, DOOMO_PLAYER_ID, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET, ACT_PARK, NEWS_IDS(ID_RIVET, DOOMO_PLAYER_ID, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET_FAV, ACT_PARK, NEWS_IDS(ID_RIVET, ID_VEX, NEWS_NONE));
    sim_lay_to_rest(ID_VEX, DEATH_OLD_AGE);
    CHECK_EQ(town_res(ID_VEX)->status, RES_DEAD);
    town_res(ID_NANA)->status = RES_EMPTY;    /* A save made before release cleared Talk news. */
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK(town_bind(ID_NANA) >= 0);
    resident_generate(town_res(ID_NANA), 123u);
    sim_init();
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, 1), 0);
    CHECK_EQ(talk_visit_page(ID_RIVET, DOOMO_PLAYER_ID, 0, 0, NULL, 1), 1);
    CHECK_EQ(talk_visit_page(ID_RIVET, ID_VEX, 0, 0, NULL, 1), 1);
}

TEST(talk_events_keep_real_participants_places_and_days_after_save)
{
    TalkMemoryEvent events[4];
    premade_town();
    sim_init();
    doomo_town.day = 23;
    talk_event_note(TALK_EV_ARGUMENT, ID_NANA, ID_RIVET, ACT_CAFE + 1);
    talk_event_note(TALK_EV_ARGUMENT, ID_RIVET, ID_NANA, ACT_CAFE + 1);
    doomo_town.day += SIM_YEAR_DAYS * 3;
    talk_event_note(TALK_EV_PRAISE, ID_NANA, DOOMO_PLAYER_ID, TALK_PLACE_HOME);
    talk_event_note(TALK_EV_BATTLE_LOST, -1, -1, TALK_PLACE_NEST_FUN_FAIR);
    doomo_town.day++;
    talk_event_note(TALK_EV_BATTLE_WON, -1, -1, TALK_PLACE_NEST_FUN_FAIR);
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(talk_event_page(ID_NANA, DOOMO_PLAYER_ID, 0, events, 4), 2);
    CHECK_EQ(events[0].kind, TALK_EV_PRAISE);
    CHECK_EQ(events[0].place, TALK_PLACE_HOME);
    CHECK_EQ(events[1].day, 23);
    CHECK_EQ(events[1].place, ACT_CAFE + 1);
    CHECK_EQ(events[1].other, ID_RIVET);
    CHECK(talk_event_has(ID_NANA, DOOMO_PLAYER_ID, &events[1]));
    CHECK(!talk_event_has(ID_VEX, DOOMO_PLAYER_ID, &events[1]));
    CHECK_EQ(talk_event_page(ID_NANA, ID_RIVET, 0, events, 4), 1);
    CHECK_EQ(talk_event_page(ID_RIVET, DOOMO_PLAYER_ID, 0, events, 4), 1);
    CHECK_EQ(events[0].other, ID_NANA);
    CHECK_EQ(talk_event_page(-1, DOOMO_PLAYER_ID, 0, events, 4), 2);
    CHECK_EQ(events[0].kind, TALK_EV_BATTLE_WON);
    CHECK_EQ(events[1].kind, TALK_EV_BATTLE_LOST);
    CHECK_EQ(events[0].place, TALK_PLACE_NEST_FUN_FAIR);
    CHECK_EQ(events[0].day - events[1].day, 1);
}

TEST(talk_events_keep_battle_milestones_under_social_churn)
{
    const DoomoSaveExt *ext = doomo_save_ext_tevt;
    TalkMemoryEvent event;
    uint8_t bytes[SAVE_EXT_MAX_TEVT];
    uint32_t size, day;
    premade_town();
    sim_init();
    talk_event_note(TALK_EV_BATTLE_WON, -1, -1, TALK_PLACE_NEST_PLAZA);
    for (day = 1; day <= SAVE_TEVT_EVENTS * 3; day++) {
        doomo_town.day = day;
        talk_event_note(TALK_EV_ARGUMENT, ID_NANA, ID_RIVET, ACT_PARK + 1);
    }
    talk_event_note(TALK_EV_BATTLE_WON, -1, -1, TALK_PLACE_NEST_PLAZA);
    size = ext->write(bytes, sizeof bytes);
    CHECK_EQ(size, SAVE_EXT_MAX_TEVT);
    CHECK(!ext->read(bytes, size - 1, ext->version));
    CHECK_EQ(ext->size(), 0);
    CHECK(ext->read(bytes, size, ext->version));
    CHECK_EQ(talk_event_page(-1, DOOMO_PLAYER_ID, 0, &event, 1), 1);
    CHECK_EQ(event.day, 0);
    CHECK_EQ(talk_event_page(ID_NANA, DOOMO_PLAYER_ID, 0, NULL, SAVE_TEVT_EVENTS), SAVE_TEVT_EVENTS - 1);
}

TEST(talk_visit_archive_imports_legacy_news_and_filters_by_participant)
{
    int venues[ACT_COUNT], i;
    premade_town();
    sim_init();
    sim_news_rec(NT_TALK_MEET, ACT_CAFE, NEWS_IDS(ID_NANA, DOOMO_PLAYER_ID, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET, ACT_PARK, NEWS_IDS(ID_NANA, ID_RIVET, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET, ACT_CAFE, NEWS_IDS(ID_NANA, DOOMO_PLAYER_ID, NEWS_NONE));
    sim_news_rec(NT_TALK_MEET, ACT_CONCERT, NEWS_IDS(ID_NANA, DOOMO_PLAYER_ID, NEWS_NONE));
    CHECK_EQ(doomo_save_ext_tvis->size(), 0);
    CHECK(save_game(1));      /* legacy visit history exists only in the newspaper */
    CHECK(load_game(1));
    sim_init();
    CHECK_EQ(doomo_save_ext_tvis->size(), 3 + 3 * 5);
    for (i = 0; i < NEWS_MAX; i++) sim_news_rec(NT_PIE, 0, NEWS_IDS(ID_NANA, NEWS_NONE, NEWS_NONE));
    CHECK_EQ(talk_visit_venues(ID_NANA, DOOMO_PLAYER_ID, 0, venues, ACT_COUNT), 2);
    CHECK_EQ(venues[0], ACT_CONCERT);
    CHECK_EQ(venues[1], ACT_CAFE);
    CHECK_EQ(talk_visit_venues(ID_NANA, ID_RIVET, 0, venues, ACT_COUNT), 1);
    CHECK_EQ(venues[0], ACT_PARK);
    CHECK_EQ(talk_visit_venues(ID_NANA, DOOMO_PLAYER_ID, 1, venues, ACT_COUNT), 1);
    CHECK_EQ(venues[0], ACT_CAFE);
}

TEST(talk_visit_archive_keeps_recent_records_within_the_save_budget)
{
    const DoomoSaveExt *ext = doomo_save_ext_tvis;
    uint8_t bytes[SAVE_EXT_MAX_TVIS];
    TalkVisit row;
    int i, cap = SAVE_TVIS_VISITS;
    uint32_t size;
    premade_town();
    sim_init();
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_TVIS), SAVE_EXT_MAX_TVIS);
    for (i = 0; i < cap; i++) {
        doomo_town.day = (uint32_t)i * 7u;
        talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    }
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, cap - 1, &row, 1), 1);
    CHECK_EQ(row.day, 0);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, &row, 1), 1);
    CHECK_EQ(row.day, (cap - 1) * 7u);
    doomo_town.day += 7u;
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_CAFE);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, cap - 1, &row, 1), 1);
    CHECK_EQ(row.day, 0);     /* compact a crowded older month before dropping its anchor */
    size = ext->write(bytes, sizeof bytes);
    CHECK_EQ(size, 3u + cap * 5u);
    CHECK(!ext->read(bytes, size - 1, ext->version));
    CHECK_EQ(ext->size(), 0);
    CHECK(ext->read(bytes, size, ext->version));
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, cap + 1), cap);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, &row, 1), 1);
    CHECK_EQ(row.venue, ACT_CAFE);
    CHECK_EQ(row.day, doomo_town.day);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, cap - 1, &row, 1), 1);
    CHECK_EQ(row.day, 0);
    CHECK_EQ(talk_visit_page(ID_NANA, ID_RIVET, 0, 0, &row, 1), 0);
}

TEST(talk_visit_archive_keeps_repeat_outings_years_apart)
{
    const DoomoSaveExt *ext = doomo_save_ext_tvis;
    uint32_t first_size, later_size;
    premade_town();
    sim_init();
    doomo_town.day = 1;
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    first_size = ext->size();
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    CHECK_EQ(ext->size(), first_size);
    doomo_town.day += 25u * 336u;
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    later_size = ext->size();
    CHECK(later_size > first_size);
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(ext->size(), later_size);
}

TEST(talk_visit_archive_retains_early_outings_under_daily_churn)
{
    TalkVisit oldest, newest;
    unsigned day, end = SAVE_TVIS_VISITS / 4u * SIM_MONTH_DAYS;
    premade_town();
    sim_init();
    talk_visit_note(ID_RIVET, DOOMO_PLAYER_ID, ACT_CAFE);
    for (day = 1; day <= end; day++) {
        doomo_town.day = day;
        talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    }
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(talk_visit_page(ID_RIVET, DOOMO_PLAYER_ID, 0, 0, &oldest, 1), 1);
    CHECK_EQ(oldest.day, 0);
    CHECK_EQ(oldest.venue, ACT_CAFE);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, &newest, 1), 1);
    CHECK_EQ(newest.day, end);
}

TEST(talk_visit_archive_preserves_unknown_dates_from_v1)
{
    const DoomoSaveExt *ext = doomo_save_ext_tvis;
    TalkVisit rows[2];
    uint32_t record = NEWS_IDS(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    uint8_t v1[] = { (uint8_t)record, (uint8_t)(record >> 8), (uint8_t)(record >> 16) };
    premade_town();
    sim_init();
    doomo_town.day = 77;
    CHECK(ext->read(v1, sizeof v1, 1));
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, rows, 2), 2);
    CHECK_EQ(rows[0].day, 77u);
    CHECK_EQ(rows[1].day, TALK_VISIT_UNDATED);
    CHECK(talk_visit_has(ID_NANA, DOOMO_PLAYER_ID, 0, &rows[1]));
    CHECK(!talk_visit_has(ID_NANA, ID_RIVET, 0, &rows[1]));
}

TEST(talk_elder_recall_lapses_progress_and_journal_cues_help)
{
    uint32_t day, outset = 1700;
    int misses[3] = {0}, cues = 0, answer = -1, repeat = TALK_RECALL_CLEAR;
    test_reset();
    town_new(TOWN_FREEPLAY, 10, TOWN_POP_PREMADE);   /* Nana is susceptible on this island */
    RES_SET_STAGE(town_res(ID_NANA), LIFE_ELDER);
    sim_init();
    doomo_town.day = outset;
    CHECK_EQ(opt_get(OPT_MEMORY_LAPSES), 0);
    opt_set(OPT_MEMORY_LAPSES, 1);
    talk_visit_note(ID_NANA, DOOMO_PLAYER_ID, ACT_PARK);
    CHECK_EQ(talk_visit_recall(ID_NANA, 0), TALK_RECALL_CLEAR);
    for (day = 1; day < 420; day++) {
        int recall, cue;
        doomo_town.day = outset + day;
        recall = talk_visit_recall(ID_NANA, 0);
        cue = talk_visit_recall(ID_NANA, 1);
        CHECK(cue <= recall);
        if (day < 28) CHECK_EQ(recall, TALK_RECALL_CLEAR);
        else misses[day < 84 ? 0 : day < 168 ? 1 : 2] += recall != TALK_RECALL_CLEAR;
        cues += cue < recall;
        CHECK_EQ(talk_visit_recall(ID_RIVET, 0), TALK_RECALL_CLEAR);  /* adult */
        if (day >= 168 && recall == TALK_RECALL_LAPSE) { answer = (int)day; repeat = recall; }
    }
    CHECK(misses[0] > 0 && misses[0] * 84 < misses[1] * 56);
    CHECK(misses[1] * 252 < misses[2] * 84);
    CHECK(cues > 0 && answer >= 0);
    if (answer < 0) return;
    doomo_town.day = outset + (uint32_t)answer;
    CHECK(save_game(1));
    town_new(TOWN_FREEPLAY, 12, TOWN_POP_PREMADE);
    CHECK(load_game(1));
    CHECK_EQ(opt_get(OPT_MEMORY_LAPSES), 1);
    CHECK_EQ(talk_visit_recall(ID_NANA, 0), repeat);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, 2), 1);
    opt_set(OPT_MEMORY_LAPSES, 0);
    CHECK_EQ(talk_visit_recall(ID_NANA, 0), TALK_RECALL_CLEAR);
    CHECK_EQ(talk_visit_page(ID_NANA, DOOMO_PLAYER_ID, 0, 0, NULL, 2), 1);
}

static uint16_t concept(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    if (talk_lex_find(key, &c, &wf, tags) < 0) return 0;
    return c;
}

/* the n-th resident who is on the island */
static int alive(int n)
{
    int id;
    for (id = 0; id < DOOMO_PLAYER_ID; id++) {
        const DoomoResident *r = town_res(id);
        if (r && r->status != RES_EMPTY && n-- == 0) return id;
    }
    return -1;
}

static const DoomoSaveExt *ext(void) { return doomo_save_ext_talk; }

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* a chunk by hand: the header and n entries */
static uint32_t chunk(uint8_t *b, int version, int count, unsigned day)
{
    memset(b, 0, TALK_MEM_HDR + (size_t)(count > 0 ? count : 0) * TALK_MEM_ENTRY);
    b[0] = (uint8_t)version;
    b[1] = (uint8_t)count;
    put16(b + 2, day);
    return (uint32_t)(TALK_MEM_HDR + count * TALK_MEM_ENTRY);
}

static void entry(uint8_t *b, int i, int id, unsigned day, unsigned ta, unsigned tb, unsigned flags, unsigned likes)
{
    uint8_t *p = b + TALK_MEM_HDR + i * TALK_MEM_ENTRY;
    put16(p, (unsigned)id); put16(p + 2, day); put16(p + 4, ta); put16(p + 6, tb);
    p[8] = (uint8_t)flags; p[9] = (uint8_t)likes;
    put16(p + 10, 0xBEC0);                               /* spare: only bits 0-5 are read (TM2d: depth reached, secrets told today) */
}

TEST(talk_mem_registered_here)
{
    /* the native host has Talk: the chunk is in the registry, within its budget */
    int i, n = 0;
    CHECK(ext() != NULL);
    CHECK_EQ_U(ext()->tag, SAVE_EXT_TAG_TALK);
    CHECK_EQ(ext()->version, TALK_MEM_VERSION);
    CHECK_EQ(ext()->max_size, TALK_SAVE_WORST);
    CHECK(ext()->max_size <= save_budget_declared_max(SAVE_EXT_TAG_TALK));
    for (i = 0; i < save_ext_count(); i++) n += save_ext_at(i) == ext();
    CHECK_EQ(n, 1);
    CHECK_EQ(sizeof(TalkMem), TALK_MEM_ENTRY);
}

TEST(talk_mem_table)
{
    const TalkMem *e;
    premade_town();
    CHECK_EQ(talk_mem_count(), 0);                       /* a new town: empty */
    CHECK(talk_mem_find(ID_NANA) == NULL);
    CHECK(talk_mem_at(0) == NULL);
    doomo_town.day = 9;
    CHECK(talk_mem_touch(ID_NANA));
    CHECK_EQ(talk_mem_count(), 1);
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->id == ID_NANA && e->day == 9 && !e->topic_a && !e->flags && !e->likes_known && !e->spare);
    /* the last two topics, newest first; the same topic again changes nothing */
    talk_mem_note_topic(ID_NANA, 100);
    talk_mem_note_topic(ID_NANA, 200);
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->topic_a == 200 && e->topic_b == 100);
    talk_mem_note_topic(ID_NANA, 200);
    CHECK(e->topic_a == 200 && e->topic_b == 100);
    talk_mem_note_topic(ID_NANA, 4095);
    CHECK(e->topic_a == 4095 && e->topic_b == 200);
    talk_mem_note_topic(ID_NANA, TC_NONE);               /* no topic, or not a 12-bit id: ignored */
    talk_mem_note_topic(ID_NANA, 4096);
    CHECK(e->topic_a == 4095 && e->topic_b == 200);
    /* noting makes the entry */
    talk_mem_note_topic(ID_RIVET, 7);
    CHECK_EQ(talk_mem_count(), 2);
    CHECK_EQ(talk_mem_at(0)->id, ID_RIVET);              /* the most recent first */
    CHECK_EQ(talk_mem_at(1)->id, ID_NANA);
    CHECK(talk_mem_touch(DOOMO_PLAYER_ID));              /* the hero (somebody else is played) */
    /* nobody: the pet, a bad id, an empty slot, a pool id nobody has */
    CHECK(!talk_mem_touch(TALK_PET_ID));
    CHECK(!talk_mem_touch(-1));
    CHECK(!talk_mem_touch(FAMILY_POOL_FIRST));
    CHECK(!talk_mem_touch(40000));
    talk_mem_note_topic(TALK_PET_ID, 5);
    talk_mem_set_flags(-1, TMF_INSULTED);
    talk_mem_know_like(FAMILY_POOL_FIRST, 0);
    CHECK_EQ(talk_mem_count(), 3);
    /* forget, and a new town */
    talk_mem_forget(ID_RIVET);
    CHECK_EQ(talk_mem_count(), 2);
    CHECK(talk_mem_find(ID_RIVET) == NULL);
    talk_mem_forget(ID_RIVET);
    CHECK_EQ(talk_mem_count(), 2);
    town_new(TOWN_FREEPLAY, 8, TOWN_POP_PREMADE);
    CHECK_EQ(talk_mem_count(), 0);
}

TEST(talk_mem_pool_member_and_removed_resident)
{
    int kid;
    premade_town();
    CHECK(family_add(ID_NANA, FAMROLE_HOUSEMATE, 0) >= 0);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 77u, "Pim", -1);
    CHECK(kid >= FAMILY_POOL_FIRST);
    CHECK(talk_mem_touch(kid));
    talk_mem_note_topic(kid, 12);
    CHECK(talk_mem_find(kid) && talk_mem_find(kid)->topic_a == 12);
    /* The entry goes when the pool ID is released. */
    CHECK(family_remove(kid));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK(talk_mem_find(kid) == NULL);
    /* a resident whose slot is freed */
    CHECK(talk_mem_touch(ID_RIVET));
    town_free(ID_RIVET);
    CHECK(!talk_mem_refused_today(ID_RIVET));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK(!talk_mem_touch(ID_RIVET));
    talk_mem_reset();
}

TEST(talk_mem_lru_evicts_the_oldest)
{
    int i, first, second;
    premade_town();
    first = alive(0);
    second = alive(1);
    for (i = 0; i < TALK_MEM_MAX; i++) {
        CHECK(alive(i) >= 0);
        talk_mem_note_topic(alive(i), (uint16_t)(i + 1));
    }
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, first);
    CHECK_EQ(ext()->size(), TALK_MEM_WORST);              /* the worst case is the full table */
    /* a 25th person: the oldest goes, nobody else */
    CHECK(talk_mem_touch(alive(TALK_MEM_MAX)));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK(talk_mem_find(first) == NULL);
    CHECK_EQ(talk_mem_at(0)->id, alive(TALK_MEM_MAX));
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, second);
    for (i = 1; i < TALK_MEM_MAX; i++) {
        const TalkMem *e = talk_mem_find(alive(i));
        CHECK(e && e->topic_a == i + 1);
    }
    /* using an entry saves it: `second` is touched, so the 26th evicts alive(2) */
    talk_mem_know_like(second, 1);
    CHECK_EQ(talk_mem_at(0)->id, second);
    CHECK(talk_mem_touch(alive(TALK_MEM_MAX + 1)));
    CHECK(talk_mem_find(second) != NULL);
    CHECK(talk_mem_find(alive(2)) == NULL);
    /* a look (find, the queries) does not count as use */
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, alive(3));
    CHECK(talk_mem_find(alive(3)) && !talk_mem_refused_today(alive(3)) && !talk_mem_like_known(alive(3), 0));
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, alive(3));
    CHECK_EQ(ext()->size(), TALK_MEM_WORST);
    talk_mem_reset();
}

TEST(talk_mem_expanded_capacity_is_real)
{
    uint8_t bytes[TALK_SAVE_WORST];
    uint32_t n;
    int i;
    premade_town();
    CHECK_EQ(talk_mem_capacity(), TALK_MEM_MAX);
    CHECK_EQ(talk_mem_pinned_count(), 0);
    CHECK(TALK_MEM_MAX > 24);
    /* The old limit must not cause eviction on a newly supported port. */
    for (i = 0; i < 25; i++)
        CHECK(talk_mem_touch(alive(i)));
    CHECK_EQ(talk_mem_count(), 25);
    CHECK(talk_mem_find(alive(0)) != NULL);
    CHECK(talk_mem_find(alive(24)) != NULL);
    CHECK_EQ(ext()->size(), TALK_MEM_HDR + 25 * TALK_MEM_ENTRY);
    n = ext()->write(bytes, sizeof bytes);
    CHECK_EQ(n, TALK_MEM_HDR + 25 * TALK_MEM_ENTRY);
    talk_mem_reset();
    CHECK_EQ(talk_mem_count(), 0);
    CHECK(ext()->read(bytes, n, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 25);
    CHECK(talk_mem_find(alive(0)) != NULL);
    CHECK(talk_mem_find(alive(24)) != NULL);
    talk_mem_reset();
}

TEST(talk_mem_prefers_shared_experiences_over_smalltalk)
{
    int i, first, second;
    premade_town();
    first = alive(0);
    second = alive(1);
    /* A real shared experience with the oldest resident should survive
     * a newcomer while a newer casual chat can be forgotten. */
    CHECK(talk_mem_touch(first));
    talk_mem_note_shared(first, TALK_SHARED_PLAYED);
    for (i = 1; i < TALK_MEM_MAX; i++)
        CHECK(talk_mem_touch(alive(i)));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK(talk_mem_touch(alive(TALK_MEM_MAX)));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK(talk_mem_shared(first, TALK_SHARED_PLAYED));
    CHECK(talk_mem_find(second) == NULL);
    /* Eventually a meaningful memory may be evicted if every candidate
     * is meaningful; the cap is bounded and does not grow at runtime. */
    talk_mem_reset();
}

TEST(talk_mem_full_preserves_pending_promises)
{
    int i;
    premade_town();
    /* A full LRU must not silently delete active or ready requests,
     * even if a new resident starts a conversation. */
    for (i = 0; i < TALK_MEM_MAX; i++) {
        int state = (i & 1) ? TQS_READY : TQS_ACTIVE;
        CHECK(alive(i) >= 0);
        CHECK(talk_mem_quest_set(alive(i), TQK_FETCH, state, 3, 0));
    }
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK_EQ(talk_mem_pinned_count(), TALK_MEM_MAX);
    CHECK(!talk_mem_touch(alive(TALK_MEM_MAX)));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK_EQ(talk_mem_pinned_count(), TALK_MEM_MAX);
    for (i = 0; i < TALK_MEM_MAX; i++) {
        int kind = 0, state = 0;
        CHECK(talk_mem_quest_get(alive(i), &kind, &state, NULL, NULL));
        CHECK_EQ(kind, TQK_FETCH);
        CHECK_EQ(state, (i & 1) ? TQS_READY : TQS_ACTIVE);
    }
    /* Settled memories can be evicted; the oldest settled one goes first. */
    CHECK(talk_mem_quest_set(alive(0), TQK_FETCH, TQS_SETTLED, 2, 0));
    CHECK(talk_mem_touch(alive(TALK_MEM_MAX)));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK(talk_mem_find(alive(0)) == NULL);
    for (i = 1; i < TALK_MEM_MAX; i++)
        CHECK(talk_mem_quest_get(alive(i), NULL, NULL, NULL, NULL));
    talk_mem_reset();
}

TEST(talk_mem_day_rollover_clears_refused_today_only)
{
    const TalkMem *e;
    premade_town();
    doomo_town.day = 20;
    talk_mem_note_topic(ID_NANA, 31);
    talk_mem_note_topic(ID_NANA, 32);
    talk_mem_know_like(ID_NANA, 0);
    talk_mem_know_like(ID_NANA, 3);
    talk_mem_set_flags(ID_NANA, TMF_REFUSED_TODAY | TMF_PROMISED);
    talk_mem_set_flags(ID_VEX, TMF_REFUSED_TODAY);
    CHECK(talk_mem_refused_today(ID_NANA) && talk_mem_refused_today(ID_VEX));
    CHECK(!talk_mem_refused_today(ID_RIVET));
    {   /* the promise becomes "last time" when the conversation ends */
        TalkConv c;
        int ids[1] = { ID_NANA };
        memset(&c, 0, sizeof c);
        c.n = 1; c.id[0] = (int16_t)ids[0]; c.spoke = 1;
        talk_mem_end(&c);
    }
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->flags == (TMF_REFUSED_TODAY | TMF_PROMISED));
    /* the same day, later: still refused */
    CHECK(talk_mem_refused_today(ID_NANA));
    /* the next day: only refused_today goes */
    doomo_town.day = 21;
    CHECK(!talk_mem_refused_today(ID_NANA));
    CHECK(!talk_mem_refused_today(ID_VEX));
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->flags == TMF_PROMISED);
    CHECK(e->topic_a == 32 && e->topic_b == 31 && e->likes_known == 0x09 && e->day == 20);
    CHECK_EQ(talk_mem_count(), 2);
    /* a promise kept */
    talk_mem_clear_flags(ID_NANA, TMF_PROMISED);
    CHECK_EQ(talk_mem_find(ID_NANA)->flags, 0);
    CHECK_EQ(talk_mem_find(ID_NANA)->day, 20);            /* clearing is not a talk */
    talk_mem_reset();
}

TEST(talk_mem_shared_experiences_keep_the_save_budget)
{
    uint8_t bytes[TALK_SAVE_WORST];
    uint32_t size;
    premade_town();
    CHECK(!talk_mem_shared(ID_NANA, TALK_SHARED_PLAYED));
    CHECK(!talk_mem_shared(ID_NANA, TALK_SHARED_GIFTED));
    talk_mem_note_shared(ID_NANA, 0);         /* no spurious entry */
    CHECK_EQ(talk_mem_count(), 0);
    talk_mem_know_like(ID_NANA, 1);
    talk_mem_note_shared(ID_NANA, TALK_SHARED_PLAYED | TALK_SHARED_GIFTED);
    CHECK(talk_mem_shared(ID_NANA, TALK_SHARED_PLAYED));
    CHECK(talk_mem_shared(ID_NANA, TALK_SHARED_GIFTED));
    CHECK(!talk_mem_shared(ID_NANA, TALK_SHARED_INVITED));
    CHECK(talk_mem_like_known(ID_NANA, 1));   /* likes unaffected */
    CHECK_EQ(sizeof(TalkMem), TALK_MEM_ENTRY);
    size = ext()->write(bytes, sizeof bytes);
    CHECK(size > 0 && size <= TALK_SAVE_WORST);
    CHECK_EQ(bytes[0], TALK_MEM_VERSION_EVENTS);
    talk_mem_reset();
    CHECK(!talk_mem_shared(ID_NANA, TALK_SHARED_GIFTED));
    CHECK(ext()->read(bytes, size, TALK_MEM_VERSION));
    CHECK(talk_mem_shared(ID_NANA, TALK_SHARED_PLAYED | TALK_SHARED_GIFTED));
    CHECK(talk_mem_like_known(ID_NANA, 1));
    doomo_town.day++;
    CHECK(talk_mem_shared(ID_NANA, TALK_SHARED_PLAYED));
    talk_mem_reset();
}

TEST(talk_mem_likes_known)
{
    int w;
    premade_town();
    for (w = 0; w < 5; w++) CHECK(!talk_mem_like_known(ID_NANA, w));
    talk_mem_know_like(ID_NANA, 1);
    talk_mem_know_like(ID_NANA, 3);
    CHECK(!talk_mem_like_known(ID_NANA, 0) && talk_mem_like_known(ID_NANA, 1));
    CHECK(!talk_mem_like_known(ID_NANA, 2) && talk_mem_like_known(ID_NANA, 3));
    CHECK(!talk_mem_like_known(ID_NANA, 4));
    talk_mem_know_like(ID_NANA, 4);
    CHECK(talk_mem_like_known(ID_NANA, 4));
    talk_mem_know_like(ID_NANA, 5);                       /* not a like */
    talk_mem_know_like(ID_NANA, -1);
    CHECK_EQ(talk_mem_find(ID_NANA)->likes_known, 0x1A);
    CHECK(!talk_mem_like_known(ID_NANA, 5) && !talk_mem_like_known(ID_NANA, -1));
    CHECK(!talk_mem_like_known(ID_RIVET, 1));
    talk_mem_reset();
}

TEST(talk_mem_chunk_empty_and_old_saves)
{
    static uint8_t img[262144];
    uint8_t b[TALK_MEM_WORST + 16];
    uint32_t n;
    premade_town();
    /* an empty table writes nothing */
    CHECK_EQ(ext()->size(), 0u);
    CHECK_EQ(ext()->write(b, sizeof b), 0u);
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    /* a save without talk memory loads as an empty table, whatever was there */
    talk_mem_note_topic(ID_NANA, 5);
    CHECK_EQ(talk_mem_count(), 1);
    CHECK(save_deserialize(img, n));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK_EQ(ext()->size(), 0u);
    /* an old save has no 'TALK' chunk at all: save.c resets a registered
     * chunk the stream lacks (test_save.c) through this */
    talk_mem_note_topic(ID_NANA, 5);
    ext()->reset();
    CHECK_EQ(talk_mem_count(), 0);
    /* the empty chunk reads as empty too */
    talk_mem_note_topic(ID_NANA, 5);
    CHECK(ext()->read(b, 0, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
}

TEST(talk_mem_chunk_round_trip)
{
    static uint8_t img[262144];
    TalkMem want[TALK_MEM_MAX];
    uint8_t b[TALK_MEM_WORST + 16], b2[TALK_MEM_WORST + 16];
    uint32_t n, len;
    int i, kid;
    premade_town();
    doomo_town.day = 70000;                               /* past 16 bits: the low half is kept */
    CHECK(family_add(ID_NANA, FAMROLE_HOUSEMATE, 0) >= 0);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 78u, "Pim", -1);
    CHECK(kid >= FAMILY_POOL_FIRST);
    talk_mem_note_topic(kid, 300);
    for (i = 0; i < TALK_MEM_MAX - 1; i++) {
        int id = alive(i);
        talk_mem_note_topic(id, (uint16_t)(1000 + i));
        talk_mem_note_topic(id, (uint16_t)(2000 + i));
        if (i % 3 == 0) talk_mem_know_like(id, i % 5);
        if (i % 4 == 0) talk_mem_set_flags(id, TMF_REFUSED_TODAY);
        if (i % 5 == 0) talk_mem_set_flags(id, TMF_COMPLIMENTED | TMF_PROMISED);
    }
    {   /* Finish the conversation with the flagged resident checked below. */
        TalkConv c;
        memset(&c, 0, sizeof c);
        c.n = 3; c.spoke = 7;
        for (i = 0; i < 2; i++) c.id[i] = (int16_t)alive(TALK_MEM_MAX - 2 - i);
        c.id[2] = (int16_t)alive(20);
        talk_mem_end(&c);
    }
    talk_mem_set_flags(alive(0), TMF_INSULTED);           /* pending: never saved */
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    for (i = 0; i < TALK_MEM_MAX; i++) {
        want[i] = *talk_mem_at(i);
        want[i].flags &= 0x0F;
    }
    /* the chunk: the worst case, exactly */
    CHECK_EQ(ext()->size(), TALK_MEM_WORST);
    CHECK_EQ(ext()->write(b, TALK_MEM_WORST - 1), 0u);    /* too small: writes nothing */
    len = ext()->write(b, sizeof b);
    CHECK_EQ(len, TALK_MEM_WORST);
    CHECK(len <= SAVE_EXT_MAX_TALK);
    CHECK_EQ(b[0], TALK_MEM_VERSION_EVENTS);
    CHECK_EQ(b[1], TALK_MEM_MAX);
    CHECK_EQ(b[2] | (b[3] << 8), 70000 & 0xFFFF);
    CHECK_EQ(ext()->write(b2, sizeof b2), len);           /* twice: the same bytes (a streamed save) */
    CHECK(memcmp(b, b2, len) == 0);
    /* through a whole save */
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    talk_mem_reset();
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_PREMADE);
    CHECK(save_deserialize(img, n));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    for (i = 0; i < TALK_MEM_MAX; i++) CHECK(memcmp(talk_mem_at(i), &want[i], sizeof(TalkMem)) == 0);
    CHECK(talk_mem_find(kid) && talk_mem_find(kid)->topic_a == 300);
    CHECK(talk_mem_refused_today(alive(0)));              /* the same town day */
    CHECK_EQ(ext()->write(b2, sizeof b2), len);
    CHECK(memcmp(b, b2, len) == 0);
    /* loaded on a later day: nobody has refused today */
    CHECK(save_deserialize(img, n));
    doomo_town.day++;
    CHECK(!talk_mem_refused_today(alive(0)));
    for (i = 0; i < TALK_MEM_MAX; i++) CHECK(!(talk_mem_at(i)->flags & TMF_REFUSED_TODAY));
    CHECK_EQ(talk_mem_find(alive(20))->flags, TMF_COMPLIMENTED | TMF_PROMISED);   /* 20: complimented, promised, refused */
    talk_mem_reset();
}

TEST(talk_mem_chunk_bad_input)
{
    uint8_t b[TALK_MEM_WORST + 64];
    uint32_t len;
    int i;
    premade_town();
    /* a good one first */
    len = chunk(b, TALK_MEM_VERSION, 2, 3);
    entry(b, 0, ID_NANA, 3, 10, 11, TMF_INSULTED, 2);
    entry(b, 1, ID_VEX, 2, 12, 0, 0, 0);
    CHECK(ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 2);
    /* a later version of the chunk, or of the header: ignored */
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION + 1));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK(ext()->read(b, len, TALK_MEM_VERSION));
    b[0] = TALK_MEM_VERSION + 1;
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    b[0] = 0;
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION));
    b[0] = TALK_MEM_VERSION;
    /* truncated at every length, and too long */
    for (i = 1; i < (int)len; i++) {
        CHECK(ext()->read(b, len, TALK_MEM_VERSION));
        CHECK_MSG(!ext()->read(b, (uint32_t)i, TALK_MEM_VERSION), "length %d", i);
        CHECK_EQ(talk_mem_count(), 0);
    }
    CHECK(!ext()->read(b, len + 1, TALK_MEM_VERSION));
    CHECK(!ext()->read(b, len + TALK_MEM_ENTRY, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    /* a count over the cap, with the bytes to match (an oversized chunk) */
    memset(b, 0, sizeof b);
    len = chunk(b, TALK_MEM_VERSION, TALK_MEM_MAX + 1, 3);
    for (i = 0; i <= TALK_MEM_MAX; i++) entry(b, i, alive(i), 3, 10, 11, 0, 0);
    CHECK(len > TALK_MEM_WORST);
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    b[1] = 255;
    CHECK(!ext()->read(b, TALK_MEM_WORST, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    /* a refused chunk never leaves half a table: save.c resets after a 0 */
    ext()->reset();
    CHECK_EQ(talk_mem_count(), 0);
}

TEST(talk_mem_meetup_chunk_clears_stale_or_malformed_plans)
{
    uint8_t legacy[TALK_MEM_WORST], data[TALK_SAVE_WORST];
    TalkSummary sum = {0};
    TalkOutcome out;
    uint32_t old_len, len, tail;
    premade_town();
    doomo_town.minute = 12 * 60;
    sim_init();
    talk_mem_note_topic(ID_NANA, 10);
    old_len = ext()->write(legacy, sizeof legacy);
    CHECK_EQ(legacy[0], TALK_MEM_VERSION_EVENTS);

    sum.avatar = DOOMO_PLAYER_ID;
    sum.n = sum.nev = 1;
    sum.id[0] = ID_NANA;
    sum.ev[0].kind = TEV_ACT_INVITE;
    sum.ev[0].b = ACT_CAFE;
    town_res(ID_NANA)->activity = ACT_HOME;
    talk_outcome_apply(&sum, &out);
    CHECK_EQ(out.invited, 1);
    len = ext()->write(data, sizeof data);
    CHECK_EQ(data[0], TALK_MEM_VERSION_MEETUPS);
    CHECK_EQ(len, old_len + TALK_MEET_SAVE_BYTES);

    CHECK(ext()->read(legacy, old_len, TALK_MEM_VERSION));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
    CHECK(talk_outcome_can(TEV_ACT_INVITE, ID_NANA));
    CHECK(talk_mem_find(ID_NANA) && talk_mem_find(ID_NANA)->topic_a == 10);
    CHECK(ext()->read(data, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), ACT_CAFE);
    CHECK(!ext()->read(data, len - 1, TALK_MEM_VERSION));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);

    doomo_town.day++;
    CHECK(ext()->read(data, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
    doomo_town.day--;
    tail = len - TALK_MEET_SAVE_BYTES;
    memcpy(data + tail + 5, data + tail + 2, 3);  /* duplicate guest */
    CHECK(!ext()->read(data, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_outcome_meet_plan(ID_NANA), -1);
    CHECK(talk_outcome_can(TEV_ACT_INVITE, ID_NANA));
}

TEST(talk_mem_chunk_stale_entries_are_harmless)
{
    uint8_t b[TALK_MEM_WORST];
    uint32_t len;
    const TalkMem *e;
    TalkConv c;
    TalkLine l;
    int ids[2] = { ID_NANA, ID_VEX };
    uint16_t pie;
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    CHECK(pie);
    doomo_town.day = 5;
    town_free(ID_RIVET);                                  /* Rivet left the island after the save */
    len = chunk(b, TALK_MEM_VERSION, 7, 5);
    entry(b, 0, ID_RIVET, 5, pie, 0, TMF_COMPLIMENTED, 1);      /* removed */
    entry(b, 1, ID_NANA, 5, 0xF000u | 4090u, pie, 0xF0u | TMF_INSULTED, 0xE0u | 2u);   /* a concept id nobody has; stray bits */
    entry(b, 2, TALK_PET_ID, 5, pie, 0, 0, 0);            /* the pet */
    entry(b, 3, FAMILY_POOL_FIRST + 3, 5, pie, 0, 0, 0);  /* a pool id nobody has */
    entry(b, 4, ID_NANA, 4, pie, pie, TMF_PROMISED, 0);   /* twice: the first stands */
    entry(b, 5, 60000, 5, pie, 0, 0, 0);                  /* nobody */
    entry(b, 6, ID_VEX, 5, 4091, 4092, 0, 0);             /* both topics unknown to the database */
    CHECK(ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 2);
    CHECK(talk_mem_find(ID_RIVET) == NULL);
    e = talk_mem_find(ID_NANA);
    CHECK(e != NULL);
    CHECK_EQ(e->topic_a, 4090);                           /* 12 bits; kept: another language pack may know it */
    CHECK_EQ(e->topic_b, pie);
    CHECK_EQ(e->flags, TMF_INSULTED);
    CHECK_EQ(e->likes_known, 2);            /* stray high bits stripped from a v1 chunk */
    CHECK(!talk_mem_shared(ID_NANA, TALK_SHARED_ALL));
    CHECK_EQ(e->spare, 0);
    CHECK(talk_db_nconcepts() < 4090);
    /* a conversation: the unknown topic is skipped for the one before; Vex has none */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 77u);
    talk_mem_begin(&c);
    CHECK_EQ(c.mem_topic[0], pie);
    CHECK_EQ(c.mem_flags[0], TMF_INSULTED);
    CHECK_EQ(c.mem_topic[1], TC_NONE);
    CHECK_EQ(c.mem_flags[1], 0);
    c.plain = 1;
    CHECK(talk_gen_line(&c, 1, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL));
    CHECK_STR(l.text, "Nothing yet.");
    /* and without a database nothing is remembered aloud */
    talk_db_close();
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 77u);
    talk_mem_begin(&c);
    CHECK_EQ(c.mem_topic[0], TC_NONE);
    CHECK_EQ(c.mem_flags[0], TMF_INSULTED);
    talk_mem_reset();
}

/* Q4: what the generator says about "last time" is what the table holds */
TEST(talk_mem_conversation_sees_the_memory)
{
    TalkConv c;
    TalkLine l;
    TalkTrace tr;
    char want[64], f[24];
    int ids[2] = { ID_NANA, ID_VEX }, i, ncb = 0;
    uint16_t pie, pancake;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    pancake = concept("pancake");
    CHECK(pie && pancake);
    /* the first conversation: nothing to remember */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 31u);
    talk_mem_begin(&c);
    c.plain = 1;
    CHECK_EQ(c.mem_topic[0], TC_NONE);
    CHECK_EQ(c.mem_flags[0], 0);
    talk_gen_line(&c, 0, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Nothing yet.");
    talk_gen_line(&c, 0, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Oh, thanks.");
    /* in it: pancakes, then pie, a compliment and a promise to Nana, an insult to Vex */
    talk_mem_note_topic(ID_NANA, pancake);
    talk_mem_note_topic(ID_NANA, pie);
    talk_mem_set_flags(ID_NANA, TMF_COMPLIMENTED | TMF_PROMISED);
    talk_mem_set_flags(ID_VEX, TMF_INSULTED);
    /* "last time" is never this conversation: its own lines do not see it, nor a begin
     * without an end (the pending flags of a talk that never ended are dropped) */
    talk_gen_line(&c, 0, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Oh, thanks.");
    talk_gen_line(&c, 0, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Nothing yet.");
    CHECK_EQ(talk_mem_find(ID_NANA)->flags & 0x0F, 0);
    talk_mem_end(&c);
    CHECK_EQ(talk_mem_find(ID_NANA)->flags, TMF_COMPLIMENTED | TMF_PROMISED);
    CHECK_EQ(talk_mem_find(ID_VEX)->flags, TMF_INSULTED);
    /* the next conversation */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 32u);
    talk_mem_begin(&c);
    c.plain = 1;
    CHECK_EQ(c.mem_topic[0], pie);
    CHECK_EQ(c.mem_flags[0], TMF_COMPLIMENTED | TMF_PROMISED);
    CHECK_EQ(c.mem_topic[1], TC_NONE);
    CHECK_EQ(c.mem_flags[1], TMF_INSULTED);
    CHECK_EQ(c.refused, 0);
    talk_db_form(pie, TF_DEFAULT, f, sizeof f);
    snprintf(want, sizeof want, "Last time: %s.", f);
    talk_gen_line(&c, 0, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, want);
    talk_gen_line(&c, 1, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Nothing yet.");
    talk_gen_line(&c, 0, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "You were kind last time.");
    talk_gen_line(&c, 1, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Oh, thanks.");
    talk_gen_line(&c, 0, -1, TA_PROMISE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "You promised last time.");
    talk_gen_line(&c, 1, -1, TA_PROMISE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "A first promise.");
    talk_gen_line(&c, 0, -1, TA_APOLOGIZE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Sorry for what?");
    talk_gen_line(&c, 1, -1, TA_APOLOGIZE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Still cross about last time.");
    /* the callback part: when it is the memory one, its value is the remembered topic */
    c.plain = 0;
    snprintf(want, sizeof want, "Again %s.", f);
    for (i = 0; i < 300; i++) {
        talk_gen_line(&c, 0, -1, TA_COMPLIMENT, TC_NONE, &l, &tr);
        if (tr.cb != TCB_MEMORY) continue;
        ncb++;
        CHECK_EQ(tr.cb_val, pie);
        CHECK_MSG(strstr(l.text, want) != NULL, "%s", l.text);
    }
    CHECK(ncb >= 3);
    /* nobody is complimented in this one: after it Nana is no longer "complimented
     * last time"; the promise stands until it is cleared; Vex said nothing and keeps hers */
    c.spoke = 1;
    talk_mem_end(&c);
    CHECK_EQ(talk_mem_find(ID_NANA)->flags, TMF_PROMISED);
    CHECK_EQ(talk_mem_find(ID_VEX)->flags, TMF_INSULTED);
    CHECK_EQ(talk_mem_at(0)->id, ID_NANA);
    talk_mem_clear_flags(ID_NANA, TMF_PROMISED);
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 33u);
    talk_mem_begin(&c);
    CHECK_EQ(c.mem_flags[0], 0);
    CHECK_EQ(c.mem_topic[0], pie);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
    talk_mem_reset();
}

/* K-5 across conversations: a no holds for the town day, through a save too */
TEST(talk_mem_refusal_holds_for_the_day)
{
    static uint8_t img[262144];
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    uint32_t n;
    int ids[1], avatar = roster_find("Mayor Giggs"), steamy = roster_find("Steamy");
    premade_town();
    CHECK(open_gentest());
    CHECK(avatar >= 0 && steamy >= 0);
    ids[0] = steamy;
    doomo_town.day = 11;
    talk_gen_begin(&c, avatar, ids, 1, 99u);
    talk_mem_begin(&c);
    c.plain = 1;
    c.consent = 0;
    memset(&u, 0, sizeof u);
    u.act[0] = TA_FLIRT;
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_EQ(x.gate, TG_PASS);
    CHECK_STR(x.line[0].text, "No thanks.");
    CHECK(c.refused & 1);
    talk_mem_end(&c);
    CHECK(talk_mem_refused_today(steamy));
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    talk_mem_reset();
    CHECK(save_deserialize(img, n));
    /* a new conversation the same day, even one where they would say yes: the gate deflects */
    talk_gen_begin(&c, avatar, ids, 1, 100u);
    talk_mem_begin(&c);
    c.plain = 1;
    c.consent = 1;
    CHECK(c.refused & 1);
    memset(&u, 0, sizeof u);
    u.act[0] = TA_FLIRT;
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK_EQ(x.gate, TG_DEFLECT);
    CHECK_STR(x.line[0].text, "Not today.");
    talk_mem_end(&c);
    /* the next town day it is a question again */
    doomo_town.day = 12;
    talk_gen_begin(&c, avatar, ids, 1, 101u);
    talk_mem_begin(&c);
    c.plain = 1;
    c.consent = 1;
    CHECK_EQ(c.refused, 0);
    memset(&u, 0, sizeof u);
    u.act[0] = TA_FLIRT;
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK_EQ(x.gate, TG_PASS);
    CHECK_STR(x.line[0].text, "Oh! Hello.");
    talk_mem_end(&c);
    CHECK(!talk_mem_refused_today(steamy));
    /* a K-7 end locks the whole room for the day */
    {
        int two[2] = { ID_NANA, ID_VEX };
        talk_gen_begin(&c, DOOMO_PLAYER_ID, two, 2, 5u);
        talk_mem_begin(&c);
        c.ended = 1;
        talk_mem_end(&c);
        CHECK(talk_mem_refused_today(ID_NANA) && talk_mem_refused_today(ID_VEX));
        talk_gen_begin(&c, DOOMO_PLAYER_ID, two, 2, 6u);
        talk_mem_begin(&c);
        CHECK_EQ(c.refused, 3);
    }
    talk_db_close();
    talk_mem_reset();
}
