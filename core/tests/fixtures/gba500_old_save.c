/* gba500_old_save.c - makes core/tests/fixtures/gba500_old_1000.sav (GBA-500): a Hellvile
 * Park Classic free play island with all 1000 residents, saved by the build from before
 * GBA-500 (origin ae1bd94d, which the GBA ran too: the bytes of a save are the same on every
 * platform). core/tests/test_res_live.c loads it: with 500 live residents the island keeps
 * the 500 the player is closest to.
 *
 * Not part of the test build (core/tests/fixtures/ is not compiled). To make the file again,
 * from a checkout of that commit with `make -C core lib` done:
 *   cc -std=c99 -O2 -Icore/include -Icore/native -Icore/src -D_POSIX_C_SOURCE=200809L \
 *      -DNAT_TALK_DIR='"core/build/talk"' core/tests/fixtures/gba500_old_save.c \
 *      $(ls core/build/obj/src/*.o core/build/obj/gen/*.o) \
 *      $(ls core/build/obj/native/*.o | grep -v runner_main) -o /tmp/oldsave
 *   /tmp/oldsave core/tests/fixtures/gba500_old_1000.sav
 *
 * The ids below are all outside the GBA's default 500 (res_live.h), so only the closeness
 * rule keeps them; the test lists the same ids. */
#include <stdio.h>
#include <stdlib.h>
#include "native.h"
#include "doomo/family.h"
#include "home_int.h"

int main(int argc, char **argv)
{
    static uint8_t buf[DOOMO_SAVE_SLOT1_MAX];
    NatOptions o;
    uint32_t n;
    FILE *f;
    int r;
    if (argc != 2) { fprintf(stderr, "usage: oldsave OUT.sav\n"); return 2; }
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
    n = save_serialize(buf, sizeof(buf));
    if (!n) { fprintf(stderr, "save: %s\n", save_last_error()); return 1; }
    f = fopen(argv[1], "wb");
    if (!f || fwrite(buf, 1, n, f) != n) { fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
    fclose(f);
    printf("%s: %u bytes, %d residents, %d edges, %d graves\n", argv[1], (unsigned)n, town_count(-1),
           doomo_town.edge_count, doomo_town.grave_count);
    return 0;
}
