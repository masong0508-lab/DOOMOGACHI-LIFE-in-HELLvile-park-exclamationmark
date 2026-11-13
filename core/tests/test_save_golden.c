/* test_save_golden.c - the bytes the town section of a save puts on the medium never change (S2-10; owner: speed).
 *
 * S2-10 made the screen live while a save, a load and a town build run (plat_busy / plat_busy_step); the saved
 * bytes had to stay exactly what they were, because old saves load and the GBA, PS1 and Dreamcast ports compare
 * files. The CRC-32 of the town section of a few islands was taken on the tree before S2-10 (ee686c74) and is fixed
 * here: a change to the codec's field packing, its bit writer, its LZ stage or the section order shows up as a
 * different number. (A deliberate format change bumps DOOMO_SAVE_VERSION and these numbers with it.)
 *
 * What is hashed: the image is decoded (the format's LZ77 is a few lines) and the raw body is cut at the first
 * extension chunk (save_ext.h), so a new chunk, a Talk or level-pack chunk's growth, or a changed default option
 * does not move it. The islands are built by hand (no roster, no generator, no parts table): a content change
 * cannot move it either. Only the codec can. */
#include "test.h"
#include "doomo/save.h"
#include "doomo/save_ext.h"

/* taken on the tree before S2-10 (ee686c74): the length and CRC-32 of the raw town section */
#define GOLDEN_EMPTY_LEN     881u
#define GOLDEN_EMPTY_CRC     0xBD046F26u
#define GOLDEN_CUSTOM_LEN    19669u
#define GOLDEN_CUSTOM_CRC    0x5E49493Au
#define GOLDEN_STORY_LEN     3206u
#define GOLDEN_STORY_CRC     0xA40F875Eu

static uint8_t image[DOOMO_SAVE_SLOT1_MAX + 64];
static uint8_t raw[256 * 1024];

/* the format's LZ77 (save.c "LZ codec"): c < 0x80: c + 1 literals; else (c & 0x7F) + 3 bytes from distance u16 LE */
static uint32_t unlz(const uint8_t *src, uint32_t n, uint8_t *dst, uint32_t cap)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t c = src[i++];
        if (c < 0x80) {
            uint32_t k = c + 1;
            if (i + k > n || o + k > cap) return 0;
            memcpy(dst + o, src + i, k);
            i += k; o += k;
        } else {
            uint32_t len = (c & 0x7F) + 3, d;
            if (i + 2 > n) return 0;
            d = (uint32_t)src[i] | ((uint32_t)src[i + 1] << 8);
            i += 2;
            if (d == 0 || d > o || o + len > cap) return 0;
            while (len--) { dst[o] = dst[o - d]; o++; }
        }
    }
    return o;
}

/* the raw town section of the current town: its length and CRC-32 (0 on a decode problem) */
static uint32_t town_section_crc(uint32_t *len)
{
    uint32_t n, total, rawn, pos, i, c;
    const DoomoSaveExt *first = NULL;
    resident_sanitize(&doomo_town.player);
    rel_normalize();
    n = save_serialize(image, sizeof(image));
    if (!n) return 0;
    total = (uint32_t)image[8] | ((uint32_t)image[9] << 8) | ((uint32_t)image[10] << 16) | ((uint32_t)image[11] << 24);
    if (total != n) return 0;
    rawn = unlz(image + DOOMO_SAVE_HEADER_SIZE, n - DOOMO_SAVE_HEADER_SIZE, raw, sizeof(raw));
    if (!rawn) return 0;
    for (i = 0; i < (uint32_t)save_ext_count(); i++) {
        const DoomoSaveExt *e = save_ext_at((int)i);
        if (e && e->write) { first = e; break; }
    }
    pos = rawn;
    if (first) {                                       /* the town section ends where the first chunk's tag starts */
        uint8_t t[4];
        t[0] = (uint8_t)first->tag; t[1] = (uint8_t)(first->tag >> 8);
        t[2] = (uint8_t)(first->tag >> 16); t[3] = (uint8_t)(first->tag >> 24);
        for (pos = 0; pos + 4 <= rawn; pos++)
            if (memcmp(raw + pos, t, 4) == 0) break;
        if (pos + 4 > rawn) return 0;
    }
    c = save_crc32(raw, pos);
    if (len) *len = pos;
    return c ? c : 1;
}

static void settle(void)
{
    /* nothing the content tables or the option defaults could move */
    memset(doomo_town.opt, 0, sizeof(doomo_town.opt));
    memset(doomo_town.unlocks, 0, sizeof(doomo_town.unlocks));
    memset(doomo_town.items, 0, sizeof(doomo_town.items));
    memset(doomo_town.weapons, 0, sizeof(doomo_town.weapons));
    memset(doomo_town.progress, 0, sizeof(doomo_town.progress));
    doomo_town.day = 77;
    doomo_town.minute = 600;
    doomo_town.money = 4321;
    memcpy(doomo_town.pet.name, "Puffy", 6);
    memcpy(doomo_town.player.name, "Tester", 7);
}

/* residents by formula: small ids that stay valid when the tables grow */
static void make_residents(int n)
{
    int i, k;
    for (i = 0; i < n; i++) {
        DoomoResident *r = &doomo_town.res[i];
        memset(r, 0, sizeof(*r));
        r->status = RES_ALIVE;
        r->flags = RF_CUSTOM;
        doomo_snprintf(r->name, sizeof(r->name), "C%03d", i);
        for (k = 0; k < STAT_COUNT; k++) r->stat[k] = (uint8_t)((i * 7 + k * 13) % 101);
        r->mood = (uint8_t)(i % 5);
        r->activity = (uint8_t)(i % 7);
        r->level = (uint8_t)(i % 60);
        RES_SET_LOOK(r, GF_HAIR_STYLE, i % 8);
        RES_SET_LOOK(r, GF_EYE_STYLE, i % 6);
        RES_SET_LOOK(r, GF_TOP, i % 9);
    }
}

TEST(save_bytes_golden_empty_island)
{
    uint32_t len = 0, c;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    settle();
    c = town_section_crc(&len);
    printf("      golden empty island: %u raw bytes, crc 0x%08X\n", len, c);
    CHECK(c != 0);
    CHECK_EQ(len, GOLDEN_EMPTY_LEN);
    CHECK_EQ_U(c, GOLDEN_EMPTY_CRC);
}

TEST(save_bytes_golden_custom_citizens_and_relationships)
{
    uint32_t len = 0, c;
    int i;
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_EMPTY);
    settle();
    make_residents(300);
    for (i = 0; i < 600; i++) rel_adjust((i * 37) % 300, (i * 91 + 5) % 300, (i % 61) - 30);
    for (i = 0; i < 10; i++) grave_add((i * 29) % 300);
    c = town_section_crc(&len);
    printf("      golden 300 custom citizens, %d relationships: %u raw bytes, crc 0x%08X\n", (int)doomo_town.edge_count, len, c);
    CHECK(c != 0);
    CHECK_EQ(len, GOLDEN_CUSTOM_LEN);
    CHECK_EQ_U(c, GOLDEN_CUSTOM_CRC);
}

TEST(save_bytes_golden_story_island)
{
    uint32_t len = 0, c;
    town_new(TOWN_STORY, 5, TOWN_POP_EMPTY);
    settle();
    make_residents(40);
    doomo_town.story.chapter = 2;
    doomo_town.story.x = -300;
    doomo_town.story.y = 4000;
    doomo_town.story.ammo[3] = 65000;
    doomo_town.story.kills = 4567;
    doomo_town.story.flags[5] = 0xA5;
    c = town_section_crc(&len);
    printf("      golden story island: %u raw bytes, crc 0x%08X\n", len, c);
    CHECK(c != 0);
    CHECK_EQ(len, GOLDEN_STORY_LEN);
    CHECK_EQ_U(c, GOLDEN_STORY_CRC);
}
