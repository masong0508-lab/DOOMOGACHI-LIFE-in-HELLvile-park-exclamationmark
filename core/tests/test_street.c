/* test_street.c - Home Row's address and length rules (street.h; roadmap M0). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/street.h"

static void empty_town(void)
{
    int i;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) doomo_town.res[i].status = RES_EMPTY;
}

static void fill(int district, int from, int to)       /* occupy lots from..to-1 */
{
    int i;
    for (i = from; i < to; i++) doomo_town.res[district * DOOMO_DISTRICT_SIZE + i].status = RES_ALIVE;
}

TEST(street_address_round_trips_for_all_1000_slots)
{
    int s, seen = 0;
    for (s = 0; s < DOOMO_MAX_RESIDENTS; s++) {
        int d = street_district(s), c = street_chunk_of(s), l = street_lot_of(s);
        CHECK_EQ(d, s / 500);
        CHECK(c >= 0 && c < STREET_CHUNKS_MAX && l >= 0 && l < STREET_LOTS_PER_CHUNK);
        CHECK_EQ(street_slot(d, c, l), s);
        CHECK_EQ(street_side(s), l & 1);
        seen++;
    }
    CHECK_EQ(seen, 1000);
    CHECK_EQ(street_district(-1), -1);
    CHECK_EQ(street_district(1000), -1);
    CHECK_EQ(street_slot(2, 0, 0), -1);
    CHECK_EQ(street_slot(0, 10, 0), -1);
    CHECK_EQ(street_slot(0, 0, 50), -1);
}

TEST(street_lot_matches_the_apartments_floor_and_room)
{
    int s;
    for (s = 0; s < DOOMO_MAX_RESIDENTS; s++) {
        CHECK_EQ(street_block(s), (s % DOOMO_DISTRICT_SIZE) / 10 + 1);
        CHECK_EQ(street_room(s), (s % DOOMO_DISTRICT_SIZE) % 10 + 1);
    }
    CHECK_EQ(street_block(0), 1);   CHECK_EQ(street_room(0), 1);
    CHECK_EQ(street_block(499), 50); CHECK_EQ(street_room(499), 10);
    CHECK_EQ(street_block(500), 1);                    /* Hellvile starts over */
}

TEST(street_length_follows_the_limit_rule)
{
    CHECK_EQ(street_lots_for(0), STREET_FREE_LOTS);    /* nobody: a few free lots */
    CHECK_EQ(street_chunks_for(0), 1);                 /* never shorter than one chunk */
    CHECK_EQ(street_lots_for(1), 11);
    CHECK_EQ(street_chunks_for(1), 1);
    CHECK_EQ(street_chunks_for(40), 1);                /* 50 lots exactly */
    CHECK_EQ(street_chunks_for(41), 2);                /* 51 lots: one more chunk */
    CHECK_EQ(street_lots_for(499), 500);               /* capped at the district */
    CHECK_EQ(street_lots_for(500), 500);
    CHECK_EQ(street_chunks_for(500), STREET_CHUNKS_MAX);
    CHECK_EQ(street_chunks_for(-5), 1);
}

TEST(street_reads_the_live_town)
{
    empty_town();
    CHECK_EQ(street_extent(0), 0);
    CHECK_EQ(street_lots(0), STREET_FREE_LOTS);
    CHECK_EQ(street_free_lots(0), STREET_FREE_LOTS);
    fill(0, 0, 1);                                     /* one person */
    CHECK_EQ(street_extent(0), 1);
    CHECK_EQ(street_free_lots(0), STREET_FREE_LOTS);
    fill(0, 0, 499);
    CHECK_EQ(street_extent(0), 499);
    CHECK_EQ(street_chunks(0), STREET_CHUNKS_MAX);
    CHECK_EQ(street_free_lots(0), 1);                  /* the last lot is still free */
    CHECK(!street_is_full(0));
    CHECK_EQ(street_extent(1), 0);                     /* Hellvile is its own street */
    CHECK_EQ(street_chunks(1), 1);
}

TEST(street_full_district_has_no_free_lot_and_deleting_gives_one_back)
{
    empty_town();
    fill(0, 0, 500);
    fill(1, 0, 500);
    CHECK(street_is_full(0) && street_is_full(1));     /* the 1000 limit */
    CHECK_EQ(street_free_lots(0) + street_free_lots(1), 0);
    CHECK_EQ(street_lots(0), 500);
    doomo_town.res[123].status = RES_EMPTY;            /* delete one person */
    CHECK(!street_is_full(0));
    CHECK_EQ(street_free_lots(0), 1);
    CHECK_EQ(street_lots(0), 500);                     /* the street keeps its length ... */
    CHECK_EQ(street_slot(0, 2, 23), 123);              /* ... and nobody else's address moved */
    CHECK_EQ(street_free_lots(1), 0);
}

TEST(street_shrinks_back_when_the_last_people_leave)
{
    empty_town();
    fill(0, 0, 120);
    CHECK_EQ(street_chunks(0), 3);                     /* 130 lots -> 3 chunks */
    fill(0, 0, 0);
    doomo_town.res[119].status = RES_EMPTY;
    doomo_town.res[118].status = RES_EMPTY;
    CHECK_EQ(street_extent(0), 118);
    doomo_town.res[117].status = RES_EMPTY;
    CHECK_EQ(street_extent(0), 117);
    CHECK_EQ(street_chunks(0), 3);                     /* 127 lots */
    { int i; for (i = 40; i < 120; i++) doomo_town.res[i].status = RES_EMPTY; }
    CHECK_EQ(street_extent(0), 40);
    CHECK_EQ(street_chunks(0), 1);
}
