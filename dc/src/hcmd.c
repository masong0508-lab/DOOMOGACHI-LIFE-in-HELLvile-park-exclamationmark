/* hcmd.c - commands the test harness posts in the mailbox (owner:
 * dreamcast). Mirrors gba/src/hcmd.c command for command (same ids,
 * dc_plat.h), so the harness script language carries over:
 *
 * dc/harness writes dc_mailbox.arg (and .name), then .cmd; the game runs the
 * command at the top of its loop (between two logic ticks), stores the
 * result and the microseconds it took, and clears .cmd.
 *   DC_HC_SAVE / LOAD / EXISTS / VERIFY slot   the core's save calls
 *   DC_HC_TOWN_FILL seed   a worst-case free-play island (1000 random
 *                          custom citizens + 2048 relationships); seed |
 *                          0x80000000: a typical lived-in story island
 *   DC_HC_TOWN_CRC         CRC-32 of everything a save stores
 *   DC_HC_SCENE / PUSH a   scene_set / scene_push(scene_by_name(name), a)
 *   DC_HC_SEED seed        town seed + doomo_rng, and plat_entropy() pinned
 *   DC_HC_FLAG f           story_flag(f)
 *   DC_HC_TEXT_ARM         name = needle: plat_text calls are searched for it
 *   DC_HC_TEXT_HIT         1 once the needle was drawn (disarms on a hit)
 *   DC_HC_STORY_FP         story_fingerprint()
 *   DC_HC_TEXT_FEED c      ui_text_entry_feed(c): the native runner's
 *                          `type` (dcrun posts one char per tick)
 *   DC_HC_NATIVE_SEED s    the start the native runner gets from
 *                          `--seed s` (its default 0x5EED1234), posted by
 *                          `dcrun --native-seed` while held at tick 0:
 *                          plat_entropy() pinned to s, and the random part
 *                          of doomo_init() (game.c) redone with it, so
 *                          doomo_rng and the start town match the native
 *                          runner's. The console boots on real entropy
 *                          before the harness can reach it.
 *   DC_HC_SIDE, TALK, CELL (DC7) the native runner's `side M S`, `talk
 *                          ID`, `cell X Y` (side missions, S4)
 *   DC_HC_STORY_STATE      talking? the overworld on top? (`skip-talk`)
 *   DC_HC_SAYS / TOP_IS    does the dialogue's top line contain name / is
 *                          the top scene called name (`expect-text`,
 *                          `skip-to`, `skip-to-scene`)
 *   DC_HC_CLOCK m          the town clock (`clock HH MM`)
 *   DC_HC_ROOM_PARITY l    (DC8) golden line l of core/tests/room_shells.h
 *                          drawn by plat_room (dc/src/room.c) at 320x240 as
 *                          a picture of its own (dc2d_side_*), shown for 8
 *                          vblanks: `room-parity L NAME` shoots it and the
 *                          run script compares it with room_ref's frame
 *   DC_HC_HOME_ROOM n      (DC8) the native runner's `home-room N`
 *   DC_HC_MONEY n          (DC8) the native runner's `money N`
 * and the tick hold of the harness's "sync on" (dc_harness_hold).
 */
#include <string.h>
#include <dc/video.h>                         /* vid_waitvbl (DC8) */
#include "dc_plat.h"
#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/plat_ext.h"
#include "../../core/src/ui_internal.h"     /* UiDialogDebug (expect-text) */
#include "../../core/src/home_int.h"        /* DC8: home_move_in, the parity rooms */
#include "../../core/gen/furn_ids.h"
#include "../../core/tests/room_shells.h"   /* DC8: the room_ref golden's frames */
#include "dc2d.h"

/* core/src/story_int.h (the native runner declares them the same way) */
void story_verbs_cell(int cx, int cy);
int  story_talking(void);

uint8_t  dc_entropy_pinned;
uint32_t dc_entropy_pin;
uint8_t  dc_text_armed;
static uint8_t text_hit;
static char text_needle[sizeof(dc_mailbox.name)];

void dc_text_probe(const char *s)
{
    const char *a, *b;
    for (; *s; s++) {
        for (a = s, b = text_needle; *b && *a == *b; a++, b++) {}
        if (!*b) { text_hit = 1; dc_text_armed = 0; return; }
    }
}

static void mailbox_name(char *nm)
{
    int i;
    for (i = 0; i < (int)sizeof(dc_mailbox.name) - 1 && dc_mailbox.name[i]; i++)
        nm[i] = dc_mailbox.name[i];
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
        DoomoCitizen c;
        size_t k;
        /* (DC4, as gba/src/hcmd.c) fully random v1 bytes, then res_pack
         * (clamps + sanitizes): saves use the v1 wire image, so only what v1
         * carries may be random. Randomising the 76 B DoomoResident itself
         * also fills gene bits no save brings back, and town_crc() of the
         * loaded town would never match. */
        for (k = 0; k < sizeof(c); k++) ((uint8_t *)&c)[k] = (uint8_t)rng_next(&r);
        c.status = RES_ALIVE;
        res_pack(&c, &doomo_town.res[i]);
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

/* Test flags (plat_ext.h): what the harness wrote in the mailbox; 0 on a
 * real console, which never writes it (DC4: the first-boot language picker
 * is skipped in scripted runs; CL4: so are the first-time tip cards, unless
 * the script says "tips on") */
uint32_t plat_test_flags(void)
{
    return dc_mailbox.test_flags;
}

/* G1: "sync on" scripts hold the loop at an exact tick boundary (before
 * the pad is read) and post their commands there; the harness releases
 * the hold by moving hold_at to the next stop */
void dc_harness_hold(void)
{
    if (dc_mailbox.hold_at != dc_frames) return;
    dc_audio_stall(1);                  /* DC5: no records while held, on purpose */
    while (dc_mailbox.hold_at == dc_frames) {
        if (dc_mailbox.cmd) dc_harness_cmd();
    }
}

/* DC8: golden line `line` drawn by plat_room as the frame on screen */
static uint8_t parity_cache[12 * 1024];
static DoomoRoomObj parity_objs[HOME_OBJ_MAX];

static void room_parity(int line)
{
    HomeRoom r;
    DoomoRoomView v;
    int i, keep;
    if (line < ROOM_SHELL_FIRST) rs_parity_room(&r);
    else rs_shell_room(line - ROOM_SHELL_FIRST, &r);
    rs_view(&v, parity_objs, &r, line);
    v.vy = 12;
    v.vw = (int16_t)plat_screen_w();
    v.vh = (int16_t)(plat_screen_h() - 24);
    v.plat_cache = parity_cache;
    v.plat_cache_cap = sizeof(parity_cache);
    keep = dc_nodraw;                   /* the tick before may have been a catch-up */
    dc_nodraw = 0;
    dc2d_side_begin();
    plat_clear(PAL_BLACK);
    plat_room(&v);
    dc2d_side_end();
    dc_nodraw = keep;
    for (i = 0; i < 8; i++) vid_waitvbl();
}

void dc_harness_cmd(void)
{
    uint32_t cmd = dc_mailbox.cmd, arg = dc_mailbox.arg, r = 0, c0 = dc_us();
    switch (cmd) {
    case DC_HC_SAVE:
        r = (uint32_t)save_game((int)arg);
        dc_logf("hcmd: save %u -> %u (%u bytes) %s", (unsigned)arg, (unsigned)r, (unsigned)save_last_size(), save_last_error());
        break;
    case DC_HC_LOAD:
        r = (uint32_t)load_game((int)arg);
        dc_logf("hcmd: load %u -> %u %s", (unsigned)arg, (unsigned)r, save_last_error());
        break;
    case DC_HC_EXISTS:
        r = (uint32_t)save_exists((int)arg);
        break;
    case DC_HC_VERIFY:
        r = (uint32_t)save_verify((int)arg);
        break;
    case DC_HC_TOWN_FILL:
        town_fill(arg);
        r = 1;
        break;
    case DC_HC_SCENE:
    case DC_HC_PUSH: {
        /* scene NAME [ARG] / push NAME [ARG], like the native runner's */
        char nm[sizeof(dc_mailbox.name)];
        const DoomoScene *s;
        mailbox_name(nm);
        s = scene_by_name(nm);
        if (s) {
            if (cmd == DC_HC_SCENE) scene_set(s, (int)arg);
            else scene_push(s, (int)arg);
            r = 1;
        } else {
            dc_logf("hcmd: unknown scene %s", nm);
        }
        break; }
    case DC_HC_TOWN_CRC:
        r = town_crc();
        dc_logf("hcmd: town crc %x", (unsigned)r);
        break;
    case DC_HC_SEED:
        doomo_town.seed = arg;
        rng_seed(&doomo_rng, arg);
        dc_entropy_pin = arg;
        dc_entropy_pinned = 1;
        r = 1;
        dc_logf("hcmd: seed %u", (unsigned)arg);
        break;
    case DC_HC_FLAG:
        r = (uint32_t)story_flag((int)arg);
        break;
    case DC_HC_TEXT_ARM:
        mailbox_name(text_needle);
        text_hit = 0;
        dc_text_armed = text_needle[0] != 0;
        r = dc_text_armed;
        break;
    case DC_HC_TEXT_HIT:
        r = text_hit;
        if (r) { text_hit = 0; dc_text_armed = 0; }
        break;
    case DC_HC_STORY_FP:
        r = story_fingerprint();
        dc_logf("hcmd: story-fp %x", (unsigned)r);
        break;
    case DC_HC_TEXT_FEED:
        r = (uint32_t)ui_text_entry_feed((int)arg);
        break;
    case DC_HC_NATIVE_SEED:
        /* as doomo_init(): rng_seed(plat_entropy()), then
         * town_new(TOWN_FREEPLAY, rng_next(), TOWN_POP_EMPTY) */
        dc_entropy_pin = arg;
        dc_entropy_pinned = 1;
        rng_seed(&doomo_rng, plat_entropy());
        town_new(TOWN_FREEPLAY, rng_next(&doomo_rng), TOWN_POP_EMPTY);
        r = 1;
        dc_logf("hcmd: native seed %x", (unsigned)arg);
        break;
    /* --- DC7: the side-mission commands (side_sample_a.txt) --- */
    case DC_HC_SIDE:
        if ((arg & 0xFFFFu) >= 100 && (arg & 0xFFFFu) < 100u + STORY_SIDE2_ROWS) {
            story_side2_set_state((int)(arg & 0xFFFFu) - 100, (int)(arg >> 16));
            r = 1;
        }
        break;
    case DC_HC_TALK:
        story_interact_npc((int)arg);
        r = 1;
        break;
    case DC_HC_CELL:
        story_verbs_cell((int)(arg & 0xFFFFu), (int)(arg >> 16));
        r = 1;
        break;
    case DC_HC_STORY_STATE:
        r = 1u + (story_talking() ? 1u : 0u) + (scene_top() == &SCENE_STORY ? 2u : 0u);
        break;
    case DC_HC_SAYS: {
        char nm[sizeof(dc_mailbox.name)];
        UiDialogDebug d;
        memset(&d, 0, sizeof(d));
        ui_dialog_debug(&d);
        mailbox_name(nm);
        r = d.text && strstr(d.text, nm) ? 2u : 1u;
        break; }
    case DC_HC_TOP_IS: {
        char nm[sizeof(dc_mailbox.name)];
        mailbox_name(nm);
        r = scene_top() && strcmp(scene_top()->name, nm) == 0 ? 2u : 1u;
        break; }
    case DC_HC_CLOCK:                   /* the native runner's `clock HH MM` */
        doomo_town.minute = (uint16_t)(arg % 1440u);
        r = 1;
        break;
    /* --- DC8: the house --- */
    case DC_HC_ROOM_PARITY:
        if (arg < (uint32_t)ROOM_SHELL_LINES) {
            room_parity((int)arg);
            r = 1 + arg;
        }
        break;
    case DC_HC_HOME_ROOM:               /* the native runner's `home-room N` */
        home_move_in();
        home_unlock_room((int)arg);
        r = 1;
        break;
    case DC_HC_MONEY:                   /* the native runner's `money N` */
        doomo_town.money = arg;
        r = 1;
        break;
    default:
        break;
    }
    dc_mailbox.cycles = dc_us() - c0;
    dc_mailbox.result = r;
    dc_mailbox.cmd = 0;
    dc_pacing_reset();
}
