/* test_save_golden.c - the bytes a save puts on the medium never change (S2-10; owner: speed).
 *
 * S2-10 made the screen live while a save, a load and a town build run (plat_busy / plat_busy_step); the saved
 * bytes had to stay exactly what they were, because old saves load and the GBA, PS1 and Dreamcast ports compare
 * files. The CRC-32 of the whole image of a few deterministic islands was taken on the tree before S2-10 and is
 * fixed here: a change to the codec, the record rebuild or the section order shows up as a different number.
 * (A deliberate format change bumps DOOMO_SAVE_VERSION and these numbers with it.) */
#include "test.h"
#include "doomo/save.h"

/* taken on the tree before S2-10 (ee686c74): the whole image's size and CRC-32 */
#define GOLDEN_EMPTY_SIZE   420u
#define GOLDEN_EMPTY_CRC    0x401C4EB3u
#define GOLDEN_CLASSIC_SIZE 6890u
#define GOLDEN_CLASSIC_CRC  0x7653CF70u
#define GOLDEN_LIVED_SIZE   29894u
#define GOLDEN_LIVED_CRC    0x81EBCB91u
#define GOLDEN_RANDOM_SIZE  27691u
#define GOLDEN_RANDOM_CRC   0xA7A2BF6Fu

static uint8_t image[DOOMO_SAVE_SLOT1_MAX + 64];

/* the image of the current town, as the whole-image path writes it */
static uint32_t image_crc(uint32_t *size)
{
    uint32_t n;
    resident_sanitize(&doomo_town.player);
    rel_normalize();
    n = save_serialize(image, sizeof(image));
    if (size) *size = n;
    return n ? save_crc32(image, n) : 0;
}

/* a lived-in island: every third citizen is custom, the others got older and moodier */
static void live_in(uint32_t seed)
{
    DoomoRng r;
    int i;
    rng_seed(&r, seed);
    doomo_town.day = 77;
    doomo_town.minute = 600;
    doomo_town.money = 4321;
    memcpy(doomo_town.pet.name, "Puffy", 6);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *res = &doomo_town.res[i];
        if (res->status == RES_EMPTY) continue;
        if (i % 3 == 0) {
            resident_generate(res, 70000u + (uint32_t)i);
            res->flags |= RF_CUSTOM;
        } else if (i % 3 == 1) {
            res->mood = (uint8_t)rng_range(&r, MOOD_COUNT);
            res->level = (uint8_t)rng_range(&r, 100);
        }
    }
    for (i = 0; i < 300; i++) rel_adjust(rng_range(&r, 1000), rng_range(&r, 1000), rng_between(&r, -50, 50));
}

TEST(save_bytes_golden_empty_island)
{
    uint32_t n = 0, c;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    c = image_crc(&n);
    printf("      golden empty island: %u bytes, crc 0x%08X\n", n, c);
    CHECK_EQ(n, GOLDEN_EMPTY_SIZE);
    CHECK_EQ_U(c, GOLDEN_EMPTY_CRC);
}

TEST(save_bytes_golden_classic_untouched)
{
    uint32_t n = 0, c;
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    c = image_crc(&n);
    printf("      golden classic, untouched: %u bytes, crc 0x%08X\n", n, c);
    CHECK_EQ(n, GOLDEN_CLASSIC_SIZE);
    CHECK_EQ_U(c, GOLDEN_CLASSIC_CRC);
}

TEST(save_bytes_golden_classic_lived_in)
{
    uint32_t n = 0, c;
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_PREMADE);
    live_in(3);
    c = image_crc(&n);
    printf("      golden classic, lived in: %u bytes, crc 0x%08X\n", n, c);
    CHECK_EQ(n, GOLDEN_LIVED_SIZE);
    CHECK_EQ_U(c, GOLDEN_LIVED_CRC);
}

TEST(save_bytes_golden_random_island)
{
    uint32_t n = 0, c;
    town_new(TOWN_FREEPLAY, 31337, TOWN_POP_RANDOM);
    live_in(8);
    c = image_crc(&n);
    printf("      golden random island: %u bytes, crc 0x%08X\n", n, c);
    CHECK_EQ(n, GOLDEN_RANDOM_SIZE);
    CHECK_EQ_U(c, GOLDEN_RANDOM_CRC);
}
