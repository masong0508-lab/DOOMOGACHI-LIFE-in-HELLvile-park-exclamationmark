/* gba500_old_save.c - makes the old 1000-resident saves core/tests/test_res_live.c loads
 * (GBA-500): with 500 live residents an island keeps the 500 the player is closest to.
 *
 *   oldsave OUT              gba500_old_1000.dmoo: a Hellvile Park Classic free play island
 *                            with all 1000 residents, saved by the build from before GBA-500
 *                            (origin ae1bd94d, which the GBA ran too: the bytes of a save are
 *                            the same on every platform)
 *   oldsave OUT players      gba500_old_players.dmoo: the same island played by more than
 *                            the hero: the hero plays as villager 330 (married to 331, best
 *                            friends with 332), two guests play villagers 320 and 720 (Ann's
 *                            own tie: 321), and two sim-born babies with their parents (334 +
 *                            335 -> 336, 334 the hero's best friend; 337 + 338 -> 339)
 *   oldsave OUT one-over     gba500_old_251.dmoo: an Empty island with 251 made citizens in
 *                            Hillvile (one more than the GBA's slots) and 3 in Hellvile
 * The last two were made 2026-11-15 by the GBA-500 landing with the 1000-resident native
 * build (its save code at 1000 is the code from before GBA-500: the same bytes).
 *
 * Not part of the test build (core/tests/fixtures/ is not compiled). To make a file again,
 * from a checkout with `make -C core lib` done (all 1000 live, no LIVE=):
 *   cc -std=c99 -O2 -Icore/include -Icore/native -Icore/src -D_POSIX_C_SOURCE=200809L \
 *      -DNAT_TALK_DIR='"core/build/talk"' core/tests/fixtures/gba500_old_save.c \
 *      $(ls core/build/obj/src/*.o core/build/obj/gen/*.o) \
 *      $(ls core/build/obj/native/*.o | grep -v runner_main) -o /tmp/oldsave
 *   /tmp/oldsave core/tests/fixtures/gba500_old_1000.dmoo
 *
 * The ids below are all outside the GBA's default 500 (res_live.h), so only the closeness
 * rule keeps them; the test lists the same ids. */
#include <stdio.h>
#include <stdlib.h>
#include "native.h"
#include "doomo/family.h"
#include "home_int.h"
#include "doomo/profiles.h"
#include "doomo/unlocks.h"
#include "doomo/unlock_ids.h"
#include "doomo/ending.h"
#include "doomo/playas.h"

static int baby(int pa, int pb, int free_id)
{
    int b;
    town_free(free_id);                         /* a full town: the baby takes this id */
    rel_set_kind(pa, pb, REL_MARRIED);
    b = sim_make_baby(pa, pb);
    if (b != free_id) { fprintf(stderr, "baby %d + %d: %d, not %d\n", pa, pb, b, free_id); exit(1); }
    return b;
}

/* the island played by the hero as a villager, two guests, two sim-born families */
static void players(void)
{
    unlock_grant_quiet(UL_STORY_END(0));        /* play-as and guests open after an ending */
    if (!playas_switch(330, PLAYAS_SWITCH_INSTANT)) { fprintf(stderr, "play as 330\n"); exit(1); }
    rel_set_kind(330, 331, REL_MARRIED);
    rel_adjust(330, 331, 95);
    rel_adjust(330, 332, 95);
    if (prof_guest_add("Ann", 320, -1) < 1 || prof_guest_add("Bo", 720, -1) < 1) {
        fprintf(stderr, "guests\n");
        exit(1);
    }
    prof_rel_set(1, 321, 200);
    baby(334, 335, 336);
    rel_adjust(334, DOOMO_PLAYER_ID, 90);
    baby(337, 338, 339);
}

/* an Empty island, 251 made citizens in Hillvile and 3 in Hellvile */
static void one_over(void)
{
    int i;
    town_new(TOWN_FREEPLAY, 251, TOWN_POP_EMPTY);
    sim_init();
    for (i = 0; i < 254; i++) {
        int d = i < 251 ? 0 : 1, id = town_alloc(d);
        if (id < 0) { fprintf(stderr, "alloc %d\n", i); exit(1); }
        if (d) resident_generate_hellborn(town_res(id), 7000u + (uint32_t)i);
        else resident_generate(town_res(id), 7000u + (uint32_t)i);
        town_res(id)->flags |= RF_CUSTOM;
    }
}

int main(int argc, char **argv)
{
    static uint8_t buf[DOOMO_SAVE_SLOT1_MAX];
    NatOptions o;
    uint32_t n;
    FILE *f;
    int r;
    if (argc < 2 || argc > 3) { fprintf(stderr, "usage: oldsave OUT [players | one-over]\n"); return 2; }
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.seed = 1; o.quiet = 1; o.save_dir = "/tmp";
    nat_init(&o);
    doomo_init();
    town_new(TOWN_FREEPLAY, 777, TOWN_POP_PREMADE);
    sim_init();
    doomo_town.day = 30;
    /* the household: a housemate and a partner */
    r = family_add(304, FAMROLE_HOUSEMATE, -1);
    if (r < 0) { fprintf(stderr, "housemate: %d\n", r); return 1; }
    r = family_add(810, FAMROLE_PARTNER, -1);
    if (r < 0) { fprintf(stderr, "partner: %d\n", r); return 1; }
    rel_set_kind(810, DOOMO_PLAYER_ID, REL_MARRIED);
    rel_adjust(810, DOOMO_PLAYER_ID, 95);
    /* friends, a rival, friendship levels */
    rel_adjust(305, DOOMO_PLAYER_ID, 90);
    rel_adjust(306, DOOMO_PLAYER_ID, 90);
    rel_adjust(307, DOOMO_PLAYER_ID, 40);
    rel_adjust(317, DOOMO_PLAYER_ID, -85);
    rel_adjust(807, DOOMO_PLAYER_ID, 80);
    rel_adjust(808, DOOMO_PLAYER_ID, 80);
    town_res(308)->level = 70;
    town_res(309)->level = 30;
    town_res(809)->level = 50;
    /* made over by the player, a favourite, two houses visited */
    town_res(310)->flags |= RF_CUSTOM;
    memcpy(town_res(310)->name, "Zed Custom", 11);
    town_res(312)->flags |= RF_FAVORITE;
    home_visit_log(313);
    home_visit_log(314);
    /* a close friend who died (kept, with the grave) and a stranger who died (moves away) */
    rel_adjust(316, DOOMO_PLAYER_ID, 90);
    sim_lay_to_rest(316, DEATH_OLD_AGE);
    sim_lay_to_rest(960, DEATH_OLD_AGE);
    if (argc == 3 && strcmp(argv[2], "players") == 0) players();
    else if (argc == 3 && strcmp(argv[2], "one-over") == 0) one_over();
    else if (argc == 3) { fprintf(stderr, "unknown mode %s\n", argv[2]); return 2; }
    n = save_serialize(buf, sizeof(buf));
    if (!n) { fprintf(stderr, "save: %s\n", save_last_error()); return 1; }
    f = fopen(argv[1], "wb");
    if (!f || fwrite(buf, 1, n, f) != n) { fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
    fclose(f);
    printf("%s: %u bytes, %d residents, %d edges, %d graves\n", argv[1], (unsigned)n, town_count(-1),
           doomo_town.edge_count, doomo_town.grave_count);
    return 0;
}
