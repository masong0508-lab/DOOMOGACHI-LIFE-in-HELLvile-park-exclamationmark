/* test_save.c - save codec round trips, worst cases, slot limits and
 * corruption handling (owner: foundation). */
#include <stdlib.h>
#include "test.h"
#include "doomo/life.h"
#include "data/save_v1_fixtures.h"     /* LS0: two real version-1 saves */
#include "doomo/save_budget.h"

static DoomoTown snapshot;
static uint8_t image[256 * 1024];

/* Semantic equality: everything that is saved (unused edge / grave
 * entries beyond their counts are not). */
static int towns_equal(const DoomoTown *a, const DoomoTown *b, char *why, int cap)
{
    int i;
#define FIELD(f) if (memcmp(&a->f, &b->f, sizeof(a->f)) != 0) { snprintf(why, cap, "%s differs", #f); return 0; }
    FIELD(mode) FIELD(weekday) FIELD(minute) FIELD(day) FIELD(money) FIELD(seed)
    FIELD(monster_meter) FIELD(exorcisms_won) FIELD(edge_count) FIELD(grave_count)
    FIELD(items) FIELD(weapons) FIELD(pet) FIELD(story) FIELD(opt) FIELD(unlocks)
    FIELD(progress) FIELD(player) FIELD(population)
#undef FIELD
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (memcmp(&a->res[i], &b->res[i], sizeof(DoomoResident)) != 0) {
            snprintf(why, cap, "resident %d differs", i);
            return 0;
        }
    if (memcmp(a->graves, b->graves, a->grave_count * sizeof(DoomoGrave)) != 0) {
        snprintf(why, cap, "graves differ");
        return 0;
    }
    if (memcmp(a->edges, b->edges, a->edge_count * sizeof(DoomoEdge)) != 0) {
        snprintf(why, cap, "edges differ");
        return 0;
    }
    return 1;
}

/* Fill the non-resident parts of the town with varied data. */
static void scribble_misc(uint32_t seed)
{
    DoomoRng r;
    int i;
    rng_seed(&r, seed);
    doomo_town.weekday = 3;
    doomo_town.minute = 1234;
    doomo_town.day = 4321;
    doomo_town.money = 0xDEADBEEFu;
    doomo_town.monster_meter = 7;
    doomo_town.exorcisms_won = 12;
    for (i = 1; i < ITEM_COUNT; i++) doomo_town.items[i] = (uint8_t)rng_range(&r, 100);
    for (i = 0; i < 5; i++) {
        DoomoWeapon *w = &doomo_town.weapons[i];
        w->frame = (uint8_t)rng_range(&r, 39); w->core = (uint8_t)i; w->paint = 7;
        w->quality = (uint8_t)rng_range(&r, 101); w->flags = WF_VALID; w->xp = 200;
    }
    memcpy(doomo_town.pet.name, "Puffy", 6);
    doomo_town.pet.stage = 3; doomo_town.pet.holiness = 80;
    doomo_town.pet.stage_timer = 999; doomo_town.pet.care_score = 31337;
    doomo_town.story.chapter = 2; doomo_town.story.x = -300; doomo_town.story.y = 4000;
    doomo_town.story.ammo[3] = 65000; doomo_town.story.kills = 4567;
    doomo_town.story.flags[5] = 0xA5;
    for (i = 0; i < 32; i++) doomo_town.progress[i] = rng_next(&r);
    doomo_town.unlocks[3] = 0x5A;
    opt_set(OPT_GORE, GORE_BRUTAL);
    for (i = 0; i < 20; i++) grave_add(rng_range(&r, 1000));
    memcpy(doomo_town.player.name, "Player One", 11);
    doomo_town.player.look.hair_style = HAIR_FLAME;
}

static uint32_t roundtrip(const char *what)
{
    char why[128];
    uint32_t n;
    resident_sanitize(&doomo_town.player);
    rel_normalize();
    snapshot = doomo_town;
    n = save_serialize(image, sizeof(image));
    if (!n) { fprintf(stderr, "    %s: serialize failed: %s\n", what, save_last_error()); return 0; }
    memset(&doomo_town, 0x5A, sizeof(doomo_town));          /* clobber */
    if (!save_deserialize(image, n)) {
        fprintf(stderr, "    %s: deserialize failed: %s\n", what, save_last_error());
        return 0;
    }
    if (!towns_equal(&snapshot, &doomo_town, why, sizeof(why))) {
        fprintf(stderr, "    %s: round trip mismatch: %s\n", what, why);
        return 0;
    }
    printf("      %-34s %6u bytes (%3u KB)\n", what, n, (n + 1023) / 1024);
    return n;
}

TEST(save_empty_island)
{
    uint32_t n = roundtrip("empty island");
    CHECK(n > 0 && n < 2000);
}

TEST(save_all_premade_untouched)
{
    uint32_t n;
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    n = roundtrip("1000 premade, untouched");
    CHECK(n > 0);
    CHECK(n <= DOOMO_SAVE_SLOT0_MAX);
}

TEST(save_500_custom_500_premade_changed)
{
    uint32_t n;
    int i;
    DoomoRng r;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_PREMADE);
    scribble_misc(1);
    rng_seed(&r, 99);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *res = &doomo_town.res[i];
        if (i % 2 == 0) {                                  /* custom citizen */
            resident_generate(res, 50000u + (uint32_t)i);
            res->flags |= RF_CUSTOM;
        } else {                                           /* lived-in premade */
            int k;
            for (k = 0; k < STAT_COUNT; k++) res->stat[k] = (uint8_t)rng_range(&r, 101);
            res->mood = (uint8_t)rng_range(&r, MOOD_COUNT);
            res->activity = (uint8_t)rng_range(&r, ACT_COUNT);
            res->level = (uint8_t)rng_range(&r, 100);
        }
    }
    for (i = 0; i < 1500; i++) rel_adjust(rng_range(&r, 1000), rng_range(&r, 1000), rng_between(&r, -60, 60));
    n = roundtrip("500 custom + 500 premade changed");
    CHECK(n > 0);
    CHECK(n <= DOOMO_SAVE_SLOT1_MAX);
}

static void fill_random_custom(uint32_t seed, int full_edges)
{
    int i;
    DoomoRng r;
    town_new(TOWN_FREEPLAY, seed, TOWN_POP_EMPTY);
    scribble_misc(seed);
    rng_seed(&r, seed);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *res = &doomo_town.res[i];
        size_t k;
        /* fully random bytes, then made valid: the worst case for the codec */
        for (k = 0; k < sizeof(*res); k++) ((uint8_t *)res)[k] = (uint8_t)rng_next(&r);
        res->status = RES_ALIVE;
        resident_sanitize(res);
    }
    if (full_edges) {
        while (doomo_town.edge_count < DOOMO_MAX_EDGES) {
            DoomoEdge *e = rel_get_or_add(rng_range(&r, 1001), rng_range(&r, 1001));
            if (e) { e->value = (int8_t)rng_between(&r, -100, 100); e->kind = (uint8_t)rng_range(&r, 9); }
        }
    }
}

TEST(save_1000_fully_random_custom_and_slot_limits)
{
    uint32_t n, old_size = 0;
    uint8_t *old;
    fill_random_custom(4242, 1);
    n = roundtrip("1000 random custom + 2048 edges");
    CHECK(n > 0);
    CHECK_MSG(n <= DOOMO_SAVE_SLOT1_MAX, "worst case %u > slot 1 %u", n, DOOMO_SAVE_SLOT1_MAX);
    CHECK(n > DOOMO_SAVE_SLOT0_MAX);              /* too custom for the story slot */
    /* slot 1 takes it */
    CHECK(save_game(1));
    CHECK_EQ(save_last_size(), n);
    CHECK(save_exists(1));
    /* slot 0 refuses cleanly and keeps the previous save intact */
    {
        DoomoTown keep = doomo_town;
        town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
        CHECK(save_game(0));
        old = (uint8_t *)malloc(DOOMO_SAVE_SLOT0_MAX);
        CHECK(old != NULL);
        CHECK(plat_save_read(0, old, DOOMO_SAVE_SLOT0_MAX, &old_size));
        doomo_town = keep;
    }
    CHECK(!save_game(0));
    CHECK(strstr(save_last_error(), "too big") != NULL);
    {
        uint8_t *now = (uint8_t *)malloc(DOOMO_SAVE_SLOT0_MAX);
        uint32_t now_size = 0;
        CHECK(plat_save_read(0, now, DOOMO_SAVE_SLOT0_MAX, &now_size));
        CHECK_EQ(now_size, old_size);
        CHECK(memcmp(now, old, old_size) == 0);
        free(now);
    }
    free(old);
    /* a too-small buffer fails and reports the size it needed */
    CHECK_EQ(save_serialize(image, 1000), 0);
    CHECK_EQ(save_last_size(), n);
    CHECK_EQ(save_serialize(image, n - 1), 0);
    CHECK_EQ(save_serialize(image, n), n);
    CHECK_EQ(save_serialize(NULL, 0), 0);
    /* and slot 1 loads back exactly */
    snapshot = doomo_town;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    CHECK(load_game(1));
    {
        char why[128];
        CHECK_MSG(towns_equal(&snapshot, &doomo_town, why, sizeof(why)), "%s", why);
    }
}

TEST(save_random_island_and_story)
{
    int i;
    town_new(TOWN_FREEPLAY, 31337, TOWN_POP_RANDOM);
    CHECK(roundtrip("random island, untouched") < 16 * 1024);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i += 3) doomo_town.res[i].stat[STAT_HUNGER] ^= 7;
    town_free(10);
    town_free(600);
    CHECK(roundtrip("random island, lived in") > 0);
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    scribble_misc(5);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        doomo_town.res[i].stat[STAT_HAPPY] = (uint8_t)(i % 101);
        doomo_town.res[i].stat[STAT_ENERGY] = (uint8_t)((i * 7) % 101);
        doomo_town.res[i].mood = (uint8_t)(i % MOOD_COUNT);
    }
    doomo_town.res[3].status = RES_POSSESSED;
    doomo_town.res[700].status = RES_DEAD;
    doomo_town.res[700].death_cause = DEATH_MONSTER;
    {
        uint32_t n = roundtrip("story, all premade lived in");
        CHECK(n > 0 && n <= DOOMO_SAVE_SLOT0_MAX);
    }
}

TEST(save_info_header)
{
    DoomoSaveInfo info;
    uint32_t n;
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    doomo_town.story.chapter = 3;
    doomo_town.day = 12;
    doomo_town.money = 777;
    memcpy(doomo_town.player.name, "Mochi", 6);
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    CHECK(save_info_from(image, n, &info));
    CHECK_EQ(info.mode, TOWN_STORY);
    CHECK_EQ(info.chapter, 3);
    CHECK_EQ(info.population, TOWN_POP_PREMADE);
    CHECK_EQ(info.residents, 1000);
    CHECK_EQ(info.day, 12);
    CHECK_EQ(info.money, 777);
    CHECK_EQ(info.size, n);
    CHECK_STR(info.player, "Mochi");
    CHECK(save_game(0));
    CHECK(save_peek(0, &info));
    CHECK_EQ(info.chapter, 3);
    CHECK(save_verify(0));
    plat_save_erase(1);
    CHECK(!save_peek(1, &info));
    CHECK(!save_verify(1));
    CHECK_EQ(save_slot_max(0), DOOMO_SAVE_SLOT0_MAX);
    CHECK_EQ(save_slot_max(1), DOOMO_SAVE_SLOT1_MAX);
    CHECK_EQ(save_slot_max(2), 0);
    CHECK(!save_game(2));
}

TEST(save_rejects_corruption_without_touching_town)
{
    uint32_t n, i;
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_RANDOM);
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    scribble_misc(3);
    snapshot = doomo_town;
    /* every single-byte corruption must be refused */
    for (i = 0; i < n; i += (n / 97) + 1) {
        image[i] ^= 0x10;
        CHECK_MSG(!save_deserialize(image, n), "corruption at %u accepted", i);
        image[i] ^= 0x10;
    }
    CHECK(!save_deserialize(image, n - 1));      /* truncated */
    CHECK(!save_deserialize(image, 10));
    CHECK(!save_deserialize(NULL, 0));
    image[4] = 99;                                 /* wrong version */
    CHECK(!save_deserialize(image, n));
    image[4] = DOOMO_SAVE_VERSION;
    CHECK(memcmp(&snapshot, &doomo_town, sizeof(snapshot)) == 0);
    CHECK(save_deserialize(image, n));
    CHECK_EQ(town_count(-1), 1000);
}

TEST(save_crc32_known_vector)
{
    CHECK_EQ_U(save_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
    CHECK_EQ_U(save_crc32((const uint8_t *)"", 0), 0u);
}

/* ---- streaming saves (the GBA's path, platform.h) ------------------------ */
static uint8_t file_a[DOOMO_SAVE_SLOT1_MAX], file_b[DOOMO_SAVE_SLOT1_MAX];

static uint32_t slot_bytes(int slot, uint8_t *buf)
{
    uint32_t size = 0;
    if (!plat_save_read(slot, buf, DOOMO_SAVE_SLOT1_MAX, &size)) return 0;
    return size;
}

/* save the current town in `slot` with the whole-image path and streamed
 * with each chunk size: the files must be byte-identical, and the
 * streamed load must restore the town exactly */
static int stream_matches(int slot, const char *what)
{
    static const int chunks[] = { 64, 100, 1000, 4096, 20480 };
    char why[128];
    uint32_t na, nb;
    int k;
    DoomoTown keep;
    resident_sanitize(&doomo_town.player);
    rel_normalize();
    keep = doomo_town;
    nat_save_stream = 0;
    if (!save_game(slot)) { fprintf(stderr, "    %s: save failed\n", what); return 0; }
    na = slot_bytes(slot, file_a);
    for (k = 0; k < (int)(sizeof(chunks) / sizeof(chunks[0])); k++) {
        nat_save_stream = chunks[k];
        plat_save_erase(slot);
        nat_stream_writes = nat_stream_reads = 0;
        if (!save_game(slot)) {
            fprintf(stderr, "    %s: streamed save (%d) failed: %s\n", what, chunks[k], save_last_error());
            goto fail;
        }
        nb = slot_bytes(slot, file_b);
        if (nb != na || memcmp(file_a, file_b, na) != 0) {
            fprintf(stderr, "    %s: streamed file (%d) differs (%u vs %u bytes)\n", what, chunks[k], nb, na);
            goto fail;
        }
        if (na > (uint32_t)chunks[k] && nat_stream_writes < 2) {
            fprintf(stderr, "    %s: did not stream (%u writes)\n", what, nat_stream_writes);
            goto fail;
        }
        if (!save_verify(slot) || !save_exists(slot)) {
            fprintf(stderr, "    %s: streamed verify failed\n", what);
            goto fail;
        }
        memset(&doomo_town, 0x5A, sizeof(doomo_town));
        if (!load_game(slot)) {
            fprintf(stderr, "    %s: streamed load (%d) failed: %s\n", what, chunks[k], save_last_error());
            goto fail;
        }
        if (!towns_equal(&keep, &doomo_town, why, sizeof(why))) {
            fprintf(stderr, "    %s: streamed load (%d) mismatch: %s\n", what, chunks[k], why);
            goto fail;
        }
        doomo_town = keep;
    }
    nat_save_stream = 0;
    printf("      %-34s %6u bytes streamed identically\n", what, na);
    return 1;
fail:
    nat_save_stream = 0;
    doomo_town = keep;
    return 0;
}

TEST(save_streamed_identical_and_round_trips)
{
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    CHECK(stream_matches(0, "empty island"));
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    scribble_misc(11);
    CHECK(stream_matches(0, "story, premade"));
    town_new(TOWN_FREEPLAY, 31337, TOWN_POP_RANDOM);
    CHECK(stream_matches(1, "random island"));
    fill_random_custom(4242, 1);
    CHECK(stream_matches(1, "1000 random custom + 2048 edges"));
}

TEST(save_streamed_limits_and_failures)
{
    uint32_t keep_size, now;
    DoomoTown big;
    /* a story save in slot 0 to protect */
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    nat_save_stream = 512;
    CHECK(save_game(0));
    keep_size = slot_bytes(0, file_a);
    CHECK(keep_size > 0);
    /* too big for slot 0: refused before anything is written */
    fill_random_custom(77, 1);
    big = doomo_town;
    nat_stream_writes = 0;
    CHECK(!save_game(0));
    CHECK(strstr(save_last_error(), "too big") != NULL);
    CHECK_EQ(nat_stream_writes, 0);
    now = slot_bytes(0, file_b);
    CHECK_EQ(now, keep_size);
    CHECK(memcmp(file_a, file_b, keep_size) == 0);
    /* a write failing half-way reports an error and never leaves a valid
     * but wrong save (natively the old one survives) */
    CHECK(save_game(1));
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_RANDOM);
    nat_stream_fail_at = 5;
    CHECK(!save_game(1));
    nat_stream_fail_at = 0;
    CHECK(save_verify(1));
    memset(&doomo_town, 0, sizeof(doomo_town));
    CHECK(load_game(1));
    {
        char why[128];
        CHECK_MSG(towns_equal(&big, &doomo_town, why, sizeof(why)), "%s", why);
    }
    /* a damaged byte anywhere is caught by the streamed CRC pass, and the
     * town stays untouched */
    {
        uint32_t n = slot_bytes(1, file_a), i;
        town_new(TOWN_FREEPLAY, 9, TOWN_POP_RANDOM);
        snapshot = doomo_town;
        for (i = 0; i < n; i += n / 13 + 1) {
            file_a[i] ^= 0x04;
            CHECK(plat_save_write(1, file_a, n));
            CHECK_MSG(!load_game(1), "streamed load accepted corruption at %u", i);
            CHECK(!save_verify(1));
            file_a[i] ^= 0x04;
        }
        CHECK(plat_save_write(1, file_a, n - 7));          /* truncated */
        CHECK(!load_game(1));
        CHECK(memcmp(&snapshot, &doomo_town, sizeof(snapshot)) == 0);
        CHECK(plat_save_write(1, file_a, n));
        CHECK(load_game(1));
    }
    plat_save_erase(0);
    CHECK(!load_game(0));
    CHECK(!save_exists(0));
    nat_save_stream = 0;
}

/* ---- extension chunks (save_ext.h) and the work area (save.h) ------------ */
/* the tests-only registry slot: a chunk of ext_len bytes of ext_data */
static uint8_t ext_data[DOOMO_SAVE_EXT_MAX];
static uint32_t ext_len, ext_size_override;
static int ext_resets, ext_reads, ext_reject;
static uint32_t t_ext_size(void) { return ext_size_override ? ext_size_override : ext_len; }
static uint32_t t_ext_write(uint8_t *dst, uint32_t cap)
{
    memcpy(dst, ext_data, ext_len < cap ? ext_len : cap);
    return ext_len;
}
static int t_ext_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    ext_reads++;
    if (ext_reject || version != 3 || len > sizeof(ext_data)) return 0;
    memcpy(ext_data, src, len);
    ext_len = len;
    return 1;
}
static void t_ext_reset(void)
{
    ext_resets++;
    ext_len = 0;
    memset(ext_data, 0, sizeof(ext_data));
}
static DoomoSaveExt t_ext = { SAVE_EXT_TAG('T', 'E', 'S', 'T'), 3, t_ext_size, t_ext_write,
                              t_ext_read, t_ext_reset, 0 /* max_size (S0) */ };
const DoomoSaveExt *const doomo_save_ext_test = &t_ext;

static void ext_fill(uint32_t seed, uint32_t n)
{
    DoomoRng r;
    uint32_t i;
    rng_seed(&r, seed);
    for (i = 0; i < n; i++) ext_data[i] = (uint8_t)(i < n / 2 ? rng_next(&r) : i / 7);
    ext_len = n;
}

TEST(save_extension_chunks_round_trip)
{
    static uint8_t keep[sizeof(ext_data)];
    static const int windows[] = { 0, 64, 4096 };
    uint32_t n;
    int k;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_RANDOM);
    CHECK_EQ(ext_len, 0u);                          /* town_new reset it */
    ext_fill(99, sizeof(ext_data));
    memcpy(keep, ext_data, sizeof(keep));
    /* in memory */
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    t_ext_reset();
    CHECK(save_deserialize(image, n));
    CHECK_EQ(ext_len, (uint32_t)sizeof(ext_data));
    CHECK(memcmp(ext_data, keep, sizeof(keep)) == 0);
    /* whole-image and streamed slot saves agree, and load it back */
    for (k = 0; k < 3; k++) {
        nat_save_stream = windows[k];
        CHECK(save_game(1));
        t_ext_reset();
        CHECK(load_game(1));
        CHECK_EQ(ext_len, (uint32_t)sizeof(ext_data));
        CHECK(memcmp(ext_data, keep, sizeof(keep)) == 0);
    }
    nat_save_stream = 0;
    /* a save without the chunk (here: another tag) resets the extension,
     * and the unknown chunk is skipped */
    t_ext.tag = SAVE_EXT_TAG('T', 'E', 'S', '2');
    ext_resets = ext_reads = 0;
    CHECK(save_deserialize(image, n));
    CHECK_EQ(ext_reads, 0);
    CHECK_EQ(ext_resets, 1);
    CHECK_EQ(ext_len, 0u);
    t_ext.tag = SAVE_EXT_TAG('T', 'E', 'S', 'T');
    /* a chunk read() rejects is reset; the town still loads */
    ext_reject = 1;
    ext_resets = 0;
    CHECK(save_deserialize(image, n));
    CHECK_EQ(ext_resets, 1);
    ext_reject = 0;
    /* a chunk bigger than the staging area fails the save cleanly */
    ext_fill(7, 100);
    CHECK(save_game(0));
    ext_size_override = DOOMO_SAVE_EXT_MAX + 1;
    CHECK(!save_game(0));
    CHECK(strstr(save_last_error(), "extension") != NULL);
    nat_save_stream = 512;
    CHECK(!save_game(0));
    nat_save_stream = 0;
    ext_size_override = 0;
    ext_len = 0;
    CHECK(load_game(0));
    CHECK_EQ(ext_len, 100u);
    t_ext_reset();
}

/* --- K0 (LIFESIM_PLAN 2.5, section 6): the joint worst case on the real
 * codec. The five chunks K0 registers (LIFE, PLAY, ENDS, OPT2, PROF) are
 * weak until their owners land, so the test slot carries what they would
 * add at their declared maxima: incompressible bytes, the five maxima plus
 * the four extra chunk headers. (The computed bound with HOME: test_save_budget.c.) */
static void ext_fill_k0(uint32_t seed)
{
    DoomoRng r;
    uint32_t i;
    rng_seed(&r, seed);
    ext_len = SAVE_BUDGET_K0_RAW + (SAVE_BUDGET_K0_CHUNKS - 1) * SAVE_BUDGET_CHUNK_HDR;
    for (i = 0; i < ext_len; i++) ext_data[i] = (uint8_t)rng_next(&r);
}

TEST(save_k0_joint_worst_case)
{
    uint32_t base, n;
    CHECK(SAVE_BUDGET_K0_RAW + (SAVE_BUDGET_K0_CHUNKS - 1) * SAVE_BUDGET_CHUNK_HDR <= sizeof(ext_data));
    /* the story slot: a lived-in premade story island */
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    scribble_misc(7);
    base = roundtrip("story, no K0 chunks");
    ext_fill_k0(11);
    n = roundtrip("story + LIFE/PLAY/ENDS/OPT2/PROF at max");
    CHECK(base > 0 && n > base);
    CHECK(n - base >= SAVE_BUDGET_K0_RAW);          /* nothing compressed away */
    CHECK(n - base <= SAVE_BUDGET_K0_WORST);         /* ... and the bound holds */
    CHECK_MSG(n <= SAVE_SLOT_STORY_CAP_CFG, "%u > the 36 KB story slot", (unsigned)n);
    CHECK(save_game(0));
    CHECK_EQ(save_last_size(), n);
    t_ext_reset();
    CHECK(load_game(0));
    CHECK_EQ(ext_len, SAVE_BUDGET_K0_RAW + (SAVE_BUDGET_K0_CHUNKS - 1) * SAVE_BUDGET_CHUNK_HDR);
    /* the free play slot: the codec's worst island (1000 random custom +
     * 2048 edges) */
    fill_random_custom(4242, 1);
    ext_fill_k0(12);
    n = roundtrip("worst free play + K0 at max");
    CHECK_MSG(n > 0 && n <= SAVE_SLOT_FP_CAP, "%u > slot 1", (unsigned)n);
    t_ext_reset();
}

TEST(save_work_area_is_the_scratch_tail)
{
    uint32_t cap = 0, n;
    uint8_t *s = (uint8_t *)plat_save_scratch(&cap);
    CHECK(cap >= DOOMO_SAVE_SCRATCH_MIN);
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    /* an image buffer overlapping the work area is refused ... */
    CHECK_EQ(save_serialize(s, cap), 0u);
    CHECK_EQ(save_deserialize(s, cap), 0);
    /* ... the front part is fine */
    n = save_serialize(s, cap - DOOMO_SAVE_WORK_SIZE);
    CHECK(n > 0);
    CHECK(save_deserialize(s, n));
    /* the smallest scratch a streaming platform may offer still works */
    nat_save_stream = 64;
    CHECK(save_game(0));
    CHECK(load_game(0));
    nat_save_stream = 0;
}

/* ---- LS0: the life stage in saves ---------------------------------------- */
TEST(save_life_stage_round_trips)
{
    uint32_t n0, n1;
    int i;
    /* an untouched Classic island: every premade's stage is its base
     * record's (from the ROM), so re-stating it costs nothing */
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    n0 = roundtrip("Classic, untouched (LS0)");
    CHECK(n0 > 0);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        CHECK_EQ(RES_STAGE(town_res(i)), roster_stage(i));
        resident_set_stage(town_res(i), roster_stage(i));
    }
    CHECK_EQ(roundtrip("Classic, stages re-stated"), n0);
    /* changed stages ride in delta records and come back */
    resident_set_stage(town_res(3), LIFE_TEEN);
    resident_set_stage(town_res(ROSTER_ID_PIP), LIFE_ADULT);
    resident_set_romance(town_res(ROSTER_ID_PIP), ROMANCE_HE);
    n1 = roundtrip("Classic, two stages changed");
    CHECK(n1 > 0 && n1 <= n0 + 16);
    CHECK_EQ(RES_STAGE(town_res(3)), LIFE_TEEN);
    CHECK_EQ(RES_STAGE(town_res(ROSTER_ID_PIP)), LIFE_ADULT);
    CHECK_EQ(RES_ROMANCE(town_res(ROSTER_ID_PIP)), ROMANCE_HE);
    /* full records (Empty island) and the player keep every stage */
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_EMPTY);
    for (i = 0; i < LIFE_COUNT * ROMANCE_COUNT; i++) {
        DoomoResident *r = town_res(i * 7);
        resident_generate(r, 900u + (uint32_t)i);
        resident_set_stage(r, i % LIFE_COUNT);
        resident_set_romance(r, i / LIFE_COUNT);
    }
    resident_set_stage(&doomo_town.player, LIFE_ELDER);
    CHECK(roundtrip("Empty, every stage x preference") > 0);
    for (i = 0; i < LIFE_COUNT * ROMANCE_COUNT; i++) {
        CHECK_EQ(RES_STAGE(town_res(i * 7)), i % LIFE_COUNT);
        CHECK_EQ(RES_ROMANCE(town_res(i * 7)), i / LIFE_COUNT);
    }
    CHECK_EQ(RES_STAGE(&doomo_town.player), LIFE_ELDER);
    /* Random island deltas */
    town_new(TOWN_FREEPLAY, 31337, TOWN_POP_RANDOM);
    resident_set_stage(town_res(10), LIFE_CHILD);
    resident_set_stage(town_res(900), LIFE_BABY);
    CHECK(roundtrip("Random, two stages changed") > 0);
    CHECK_EQ(RES_STAGE(town_res(10)), LIFE_CHILD);
    CHECK_EQ(RES_STAGE(town_res(900)), LIFE_BABY);
    CHECK_EQ(RES_STAGE(town_res(11)), LIFE_ADULT);
}

/* Real version-1 saves (made by the pre-LS0 codec, data/save_v1_fixtures.h)
 * still load: residents with no known stage are adults, the roster's kids,
 * teens and elders and the sim's babies get theirs back, and the romance
 * the old one-way hole let in is undone when the sim starts. */
TEST(save_version1_loads_with_life_stages)
{
    DoomoResident *r;
    uint32_t n;
    int i;
    CHECK_EQ(save_v1_classic[4], 1);                       /* really version 1 */
    CHECK(save_deserialize(save_v1_classic, sizeof(save_v1_classic)));
    CHECK_EQ(doomo_town.population, TOWN_POP_PREMADE);
    CHECK_EQ(RES_STAGE(town_res(V1A_KID_UNTOUCHED)), LIFE_CHILD);
    r = town_res(V1A_KID_RENAMED);
    CHECK_STR(r->name, "Kiddo");
    CHECK_EQ(RES_STAGE(r), LIFE_CHILD);                      /* the delta kept its base's */
    r = town_res(V1A_KID_MADEOVER);
    CHECK(r->flags & RF_CUSTOM);
    CHECK_EQ(RES_ROMANCE(r), ROMANCE_ANY);
    CHECK_EQ(RES_STAGE(r), LIFE_CHILD);
    CHECK_EQ(RES_ROMANCE(town_res(V1A_TEEN)), ROMANCE_ANY);
    CHECK_EQ(RES_STAGE(town_res(V1A_TEEN)), LIFE_TEEN);
    CHECK_EQ(RES_STAGE(town_res(V1A_ELDER)), LIFE_ELDER);
    CHECK_EQ(RES_STAGE(town_res(V1A_ADULT1)), LIFE_ADULT);
    CHECK_EQ(RES_STAGE(town_res(V1A_ADULT2)), LIFE_ADULT);
    CHECK_EQ(RES_STAGE(town_res(V1A_BABY)), LIFE_BABY);    /* a pre-LS0 sim baby */
    CHECK_EQ(RES_ROMANCE(town_res(V1A_BABY)), ROMANCE_NONE);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        if (town_res(i)->status == RES_EMPTY) continue;
        CHECK_MSG(resident_sanitize(town_res(i)) == 0, "resident %d", i);
        if (i != V1A_BABY && i != V1A_KID_RENAMED)
            CHECK_MSG(RES_STAGE(town_res(i)) == roster_stage(i), "resident %d stage %d", i,
                      RES_STAGE(town_res(i)));
    }
    /* the stored illegal romance is still on the graph... */
    CHECK_EQ(rel_kind(V1A_KID_MADEOVER, V1A_ADULT1), REL_DATING);
    CHECK(rel_has_crush(V1A_ADULT2, V1A_KID_RENAMED));
    /* ...until the sim starts (every load does sim_init) */
    sim_init();
    CHECK_EQ(rel_kind(V1A_KID_MADEOVER, V1A_ADULT1), REL_FRIEND);
    CHECK(!rel_has_crush(V1A_ADULT2, V1A_KID_RENAMED));
    CHECK_EQ(rel_kind(V1A_ADULT1, V1A_ADULT3), REL_MARRIED);   /* adults untouched */
    /* saved again it is a version-2 save of about the same size, and the
     * stages survive the round trip */
    n = roundtrip("version-1 Classic re-saved");
    CHECK(n > 0);
    CHECK_EQ(image[4] | (image[5] << 8), DOOMO_SAVE_VERSION);
    printf("      version 1: %u bytes, re-saved as version %d: %u bytes\n",
           (unsigned)sizeof(save_v1_classic), DOOMO_SAVE_VERSION, (unsigned)n);
    /* (+ the LIFE chunk LS1 adds: the news feed sim_init wrote, which a
     * version-1 save never had: its header, its bytes and the bit stream's
     * flags) */
    /* (+ PA1: the empty PLAY chunk's header, written for everyone) */
    /* (+ U0: the OPT2 chunk's header and its OPT2_COUNT value bytes, written for everyone) */
    /* (+ MP0: the empty PROF chunk's header, written for everyone) */
    CHECK(n <= sizeof(save_v1_classic) + 32 + SAVE_BUDGET_CHUNK_HDR + 8 + SAVE_BUDGET_CHUNK_HDR +
               SAVE_BUDGET_CHUNK_HDR + SAVE_BUDGET_CHUNK_HDR + OPT2_COUNT +
               SAVE_BUDGET_LZ_WORST(doomo_save_ext_life->size()));
    CHECK_EQ(RES_STAGE(town_res(V1A_BABY)), LIFE_BABY);
    CHECK_EQ(RES_STAGE(town_res(V1A_KID_MADEOVER)), LIFE_CHILD);
    /* an Empty island of version 1: full records, everyone an adult */
    CHECK(save_deserialize(save_v1_empty, sizeof(save_v1_empty)));
    CHECK_EQ(town_count(-1), 2);
    CHECK_EQ(RES_STAGE(town_res(0)), LIFE_ADULT);
    CHECK_EQ(RES_STAGE(town_res(DOOMO_DISTRICT_SIZE)), LIFE_ADULT);
    CHECK_EQ(RES_ROMANCE(town_res(DOOMO_DISTRICT_SIZE)), ROMANCE_SHE);
    CHECK(town_res(0)->flags & RF_CUSTOM);
    /* versions outside 1..DOOMO_SAVE_VERSION are refused */
    {
        static uint8_t v0[sizeof(save_v1_empty)];
        memcpy(v0, save_v1_empty, sizeof(v0));
        v0[4] = 0;
        CHECK(!save_deserialize(v0, sizeof(v0)));
        v0[4] = DOOMO_SAVE_VERSION + 1;
        CHECK(!save_deserialize(v0, sizeof(v0)));
    }
}

/* --- LS1: the 'LIFE' chunk keeps the news feed ------------------------- */
TEST(save_life_chunk_keeps_the_news)
{
    static char before[NEWS_MAX][NEWS_LEN];
    static uint8_t sec[64];
    const DoomoSaveExt *life = doomo_save_ext_life;
    uint32_t n;
    int i, count;
    CHECK(life != NULL);
    CHECK_EQ_U(life->tag, SAVE_EXT_TAG_LIFE);
    CHECK_EQ_U(life->max_size, SAVE_EXT_MAX_LIFE);
    town_new(TOWN_FREEPLAY, 77, TOWN_POP_PREMADE);
    opt_set(OPT_DAY_LENGTH, 4);
    opt_set(OPT_SIM_SPEED, 3);
    sim_init();
    sim_seed(77);
    while (doomo_town.day < 2) sim_tick();
    sim_news_push("Fun Fair: new Whack record, 999 pts!");
    sim_news_rec(NT_PIE, 0, NEWS_IDS(12, NEWS_NONE, NEWS_NONE));
    count = sim_news_count();
    CHECK(count >= 10);
    for (i = 0; i < count; i++) snprintf(before[i], NEWS_LEN, "%s", sim_news(i));
    CHECK(life->size() <= LIFE_SEC_HDR + LIFE_SZ_NEWS);
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    /* lose the feed, then load: every headline is back, word for word */
    sim_news_clear();
    sim_news_push("junk");
    CHECK(save_deserialize(image, n));
    CHECK_EQ(sim_news_count(), count);
    for (i = 0; i < count; i++) CHECK_STR(sim_news(i), before[i]);
    /* the first sim_init after a load greets the player again */
    sim_init();
    CHECK_EQ(sim_news_count(), DOOMO_MIN(count + 1, NEWS_MAX));
    CHECK(strstr(sim_news(0), "Welcome back!") != NULL);
    /* streamed slot saves agree */
    CHECK(save_game(1));
    sim_news_clear();
    CHECK(load_game(1));
    CHECK(strstr(sim_news(0), "Welcome back!") != NULL);
    CHECK_STR(sim_news(1), before[0]);
    /* sections: an unknown one is skipped, a missing one resets, a bad one
     * resets, a truncated chunk stops at the last whole section */
    sec[0] = 99; sec[1] = 1; sec[2] = 3; sec[3] = 0; sec[4] = sec[5] = sec[6] = 0xEE;
    sec[7] = LIFE_SEC_NEWS; sec[8] = 1; sec[9] = 12; sec[10] = 0;
    sec[11] = 1; sec[12] = 0; sec[13] = 0; sec[14] = 0;              /* 1 record, 0 free lines */
    sec[15] = NT_PIE; sec[16] = 0; sec[17] = 5; sec[18] = 0;          /* day 5 */
    sec[19] = 12; sec[20] = 0xFC; sec[21] = 0xFF; sec[22] = 0x3F;     /* ids 12, none, none */
    CHECK(life->read(sec, 23, 1));
    CHECK_EQ(sim_news_count(), 1);
    CHECK_EQ(sim_news_record(0)->day, 5);
    CHECK(strstr(sim_news(0), "pie-eating") != NULL);
    CHECK(life->read(sec, 7, 1));                                    /* no news section */
    CHECK_EQ(sim_news_count(), 0);
    sec[11] = NEWS_MAX + 1;                                          /* bad count */
    CHECK(life->read(sec, 23, 1));
    CHECK_EQ(sim_news_count(), 0);
    sec[11] = 1;
    CHECK(life->read(sec, 22, 1));                                   /* truncated */
    CHECK_EQ(sim_news_count(), 0);
    /* a new town clears it */
    sim_news_rec(NT_PIE, 0, NEWS_IDS(1, NEWS_NONE, NEWS_NONE));
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    CHECK(sim_news_record(0) == NULL);
}
