/* hcmd.c - commands the test harness posts in the mailbox (owner: gba).
 *
 * gba/harness writes gba_mailbox.arg, then .cmd; the game ROM runs the
 * command at the top of its loop (between two logic ticks), stores the
 * result and the cycles it took, and clears .cmd. This drives the core's
 * save / load from harness scripts (save N, load N, expect-save N), like
 * the native runner and the PS1 harness do, and builds test towns:
 *   GBA_HC_TOWN_FILL seed  a free-play island of 1000 fully random custom
 *                          citizens + 2048 relationships: the codec's
 *                          worst case (~58 KB, slot 1 only, crosses the
 *                          flash bank boundary); seed | 0x80000000: a
 *                          typical story island (1000 premade citizens,
 *                          lived in: ~14 KB)
 *   GBA_HC_TOWN_CRC        CRC-32 of everything a save stores
 * and the story / house runner commands (SPEC R20, hcmd.h):
 *   GBA_HC_SEED seed       doomo_town.seed = seed, doomo_rng reseeded and
 *                          plat_entropy() pinned to seed from now on (the
 *                          next town_new() is reproducible, as on native)
 *   GBA_HC_FLAG f          story_flag(f)
 *   GBA_HC_TEXT_ARM        name = needle (<= 19 chars): plat_text calls are
 *                          searched for it from now on
 *   GBA_HC_TEXT_HIT        1 once the needle was drawn (disarms on a hit)
 *   GBA_HC_STORY_FP        story_fingerprint()
 */
#include "gba_plat.h"
#include "hcmd.h"
#include "doomo/doomo.h"
#include "doomo/story_vm.h"

/* 28 B of .bss (H2 budget <= 64 B) */
uint8_t  gba_entropy_pinned;
uint32_t gba_entropy_pin;
uint8_t  gba_text_armed;
static uint8_t text_hit;
static char text_needle[sizeof(gba_mailbox.name)];

void gba_text_probe(const char *s)
{
    const char *a, *b;
    for (; *s; s++) {
        for (a = s, b = text_needle; *b && *a == *b; a++, b++) {}
        if (!*b) { text_hit = 1; gba_text_armed = 0; return; }
    }
}

static void mailbox_name(char *nm)
{
    int i;
    for (i = 0; i < (int)sizeof(gba_mailbox.name) - 1 && gba_mailbox.name[i]; i++)
        nm[i] = gba_mailbox.name[i];
    nm[i] = 0;
}

static void town_fill(uint32_t seed)
{
    DoomoRng r;
    int i;
    if (seed & 0x80000000u) {
        town_new(TOWN_STORY, seed & 0xFFFFu, TOWN_POP_PREMADE);
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            doomo_town.res[i].stat[STAT_HAPPY] = (uint8_t)(i % 101);
            doomo_town.res[i].stat[STAT_ENERGY] = (uint8_t)((i * 7) % 101);
            doomo_town.res[i].mood = (uint8_t)(i % MOOD_COUNT);
        }
        doomo_town.day = 40;
        resident_sanitize(&doomo_town.player);
        rel_normalize();
        return;
    }
    town_new(TOWN_FREEPLAY, seed, TOWN_POP_EMPTY);
    rng_seed(&r, seed);
    doomo_town.money = rng_next(&r);
    doomo_town.day = (uint32_t)rng_range(&r, 5000);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *res = &doomo_town.res[i];
        size_t k;
        for (k = 0; k < sizeof(*res); k++) ((uint8_t *)res)[k] = (uint8_t)rng_next(&r);
        res->status = RES_ALIVE;
        resident_sanitize(res);
    }
    while (doomo_town.edge_count < DOOMO_MAX_EDGES) {
        DoomoEdge *e = rel_get_or_add(rng_range(&r, 1001), rng_range(&r, 1001));
        if (e) { e->value = (int8_t)rng_between(&r, -100, 100); e->kind = (uint8_t)rng_range(&r, 9); }
    }
    resident_sanitize(&doomo_town.player);
    rel_normalize();
}

static uint32_t crc_more(uint32_t crc, const void *p, uint32_t n)
{
    /* chain: CRC of (previous CRC bytes ++ data) */
    uint8_t tmp[4];
    uint32_t c = save_crc32((const uint8_t *)p, n);
    tmp[0] = (uint8_t)crc; tmp[1] = (uint8_t)(crc >> 8);
    tmp[2] = (uint8_t)(crc >> 16); tmp[3] = (uint8_t)(crc >> 24);
    return save_crc32(tmp, 4) ^ c;
}

static uint32_t town_crc(void)
{
    const DoomoTown *t = &doomo_town;
    uint32_t c = 0;
    c = crc_more(c, t->res, sizeof(t->res));
    c = crc_more(c, &t->player, sizeof(t->player));
    c = crc_more(c, t->edges, t->edge_count * sizeof(DoomoEdge));
    c = crc_more(c, t->graves, t->grave_count * sizeof(DoomoGrave));
    c = crc_more(c, &t->money, sizeof(t->money));
    c = crc_more(c, &t->day, sizeof(t->day));
    c = crc_more(c, t->items, sizeof(t->items));
    c = crc_more(c, t->weapons, sizeof(t->weapons));
    c = crc_more(c, &t->pet, sizeof(t->pet));
    c = crc_more(c, &t->story, sizeof(t->story));
    c = crc_more(c, t->opt, sizeof(t->opt));
    c = crc_more(c, t->unlocks, sizeof(t->unlocks));
    c = crc_more(c, t->progress, sizeof(t->progress));
    return c;
}

void gba_harness_cmd(void)
{
    uint32_t cmd = gba_mailbox.cmd, arg = gba_mailbox.arg, r = 0, c0 = gba_cycles();
    switch (cmd) {
    case GBA_HC_SAVE:
        r = (uint32_t)save_game((int)arg);
        gba_logf("hcmd: save %u -> %u (%u bytes) %s", arg, r, save_last_size(), save_last_error());
        break;
    case GBA_HC_LOAD:
        r = (uint32_t)load_game((int)arg);
        gba_logf("hcmd: load %u -> %u %s", arg, r, save_last_error());
        break;
    case GBA_HC_EXISTS:
        r = (uint32_t)save_exists((int)arg);
        break;
    case GBA_HC_VERIFY:
        r = (uint32_t)save_verify((int)arg);
        break;
    case GBA_HC_TOWN_FILL:
        town_fill(arg);
        r = 1;
        break;
    case GBA_HC_SCENE:
    case GBA_HC_PUSH: {
        /* scene NAME [ARG] / push NAME [ARG], like the native runner's */
        char nm[sizeof(gba_mailbox.name)];
        const DoomoScene *s;
        mailbox_name(nm);
        s = scene_by_name(nm);
        if (s) {
            if (cmd == GBA_HC_SCENE) scene_set(s, (int)arg);
            else scene_push(s, (int)arg);
            r = 1;
        } else {
            gba_logf("hcmd: unknown scene %s", nm);
        }
        break; }
    case GBA_HC_TOWN_CRC:
        r = town_crc();
        gba_logf("hcmd: town crc %x", r);
        break;
    case GBA_HC_SEED:
        doomo_town.seed = arg;
        rng_seed(&doomo_rng, arg);
        gba_entropy_pin = arg;
        gba_entropy_pinned = 1;
        r = 1;
        gba_logf("hcmd: seed %u", arg);
        break;
    case GBA_HC_FLAG:
        r = (uint32_t)story_flag((int)arg);
        break;
    case GBA_HC_TEXT_ARM:
        mailbox_name(text_needle);
        text_hit = 0;
        gba_text_armed = text_needle[0] != 0;
        r = gba_text_armed;
        break;
    case GBA_HC_TEXT_HIT:
        r = text_hit;
        if (r) { text_hit = 0; gba_text_armed = 0; }
        break;
    case GBA_HC_STORY_FP:
        r = story_fingerprint();
        gba_logf("hcmd: story-fp %x", r);
        break;
    default:
        break;
    }
    gba_mailbox.cycles = gba_cycles() - c0;
    gba_mailbox.result = r;
    gba_mailbox.cmd = 0;
    gba_pacing_reset();
}
