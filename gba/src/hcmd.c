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
 */
#include "gba_plat.h"
#include "doomo/doomo.h"

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
    case GBA_HC_TOWN_CRC:
        r = town_crc();
        gba_logf("hcmd: town crc %x", r);
        break;
    default:
        break;
    }
    gba_mailbox.cycles = gba_cycles() - c0;
    gba_mailbox.result = r;
    gba_mailbox.cmd = 0;
    gba_pacing_reset();
}
