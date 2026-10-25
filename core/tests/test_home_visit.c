/* test_home_visit.c - the citizens' generated homes (owner: H4; HF sec.3,
 * SPEC H4 acceptance).
 *
 * home_visit_determinism: all 1000 ids on two seeds; the same input gives
 * the same room bytes, the rooms hash to core/tests/golden/home_visit.txt,
 * no room ever holds the citizen's fear trigger, every lived-in room has
 * 8..18 pieces and The Dark always gets a lamp. home_visit_time: the
 * generator's own work counter stays under 60k per room. Regenerate the
 * golden with `$(BUILD)/tests --update-golden home_visit_determinism`.
 */
#include "test.h"
#include "../src/home_int.h"
#include "../gen/furn_ids.h"

static uint32_t fnv_room(uint32_t h, const HomeRoom *r)
{
    const uint8_t *p = (const uint8_t *)r;
    int i, n = HOME_SZ_ROOM_HEADER + r->nobj * (int)sizeof(HomeObj);
    for (i = 0; i < HOME_SZ_ROOM_HEADER; i++) { h ^= p[i]; h *= 16777619u; }
    p = (const uint8_t *)r->obj;
    for (i = 0; i < n - HOME_SZ_ROOM_HEADER; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static uint32_t sweep(uint32_t seed, uint32_t *max_ops, int *bad_fear, int *bad_size, int *bad_dark)
{
    uint32_t h = 2166136261u;
    int id;
    town_new(TOWN_FREEPLAY, seed, TOWN_POP_RANDOM);
    home_reset();
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        HomeRoom a, b;
        HomeVisit va, vb;
        DoomoResident *r = town_res(id);
        int i;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        if (!home_visit_gen(id, &a, &va)) continue;
        home_visit_gen(id, &b, &vb);
        if (memcmp(&a, &b, sizeof(a)) != 0) return 0;          /* not deterministic */
        h = fnv_room(h, &a);
        h ^= va.kind;
        h *= 16777619u;
        if (va.ops > *max_ops) *max_ops = va.ops;
        for (i = 0; i < a.nobj; i++)
            if (home_visit_fear_hit(r->fear, HO_FURN(&a.obj[i]))) (*bad_fear)++;
        if (va.kind == HVK_HOME && (a.nobj < 8 || a.nobj > 18)) (*bad_size)++;
        if (va.kind == HVK_HOME && home_visit_fear_room(r, &a)) (*bad_dark)++;
    }
    return h;
}

TEST(home_visit_determinism)
{
    static const uint32_t seeds[2] = { 7, 0xC0FFEEu };
    uint32_t h[2], max_ops = 0;
    int bad_fear = 0, bad_size = 0, bad_dark = 0, k;
    char path[600];
    FILE *f;
    for (k = 0; k < 2; k++) {
        h[k] = sweep(seeds[k], &max_ops, &bad_fear, &bad_size, &bad_dark);
        CHECK_MSG(h[k] != 0, "seed %u: a room was not deterministic", seeds[k]);
    }
    CHECK_EQ(bad_fear, 0);
    CHECK_EQ(bad_size, 0);
    CHECK_EQ(bad_dark, 0);
    CHECK(h[0] != h[1]);
    snprintf(path, sizeof(path), "%s/home_visit.txt", test_golden_dir);
    if (test_update_golden) {
        f = fopen(path, "w");
        CHECK_MSG(f != NULL, "cannot write %s", path);
        fprintf(f, "# home_visit_gen over all 1000 ids of a TOWN_POP_RANDOM town (core/tests/\n"
                   "# test_home_visit.c): seed fnv. Regenerate with\n"
                   "#   $(BUILD)/tests --update-golden home_visit_determinism\n");
        for (k = 0; k < 2; k++) fprintf(f, "%u %08x\n", seeds[k], h[k]);
        fclose(f);
        return;
    }
    f = fopen(path, "r");
    CHECK_MSG(f != NULL, "missing %s (run tests --update-golden home_visit_determinism)", path);
    {
        char line[128];
        int n = 0;
        while (fgets(line, sizeof(line), f)) {
            unsigned s, v;
            if (line[0] == '#' || sscanf(line, "%u %x", &s, &v) != 2) continue;
            for (k = 0; k < 2; k++)
                if (s == seeds[k]) {
                    if (v != h[k]) { fclose(f); CHECK_MSG(0, "seed %u: %08x, golden %08x", s, h[k], v); }
                    n++;
                }
        }
        fclose(f);
        CHECK_EQ(n, 2);
    }
}

TEST(home_visit_time)
{
    uint32_t max_ops = 0;
    int bad_fear = 0, bad_size = 0, bad_dark = 0;
    sweep(12345, &max_ops, &bad_fear, &bad_size, &bad_dark);
    CHECK_MSG(max_ops < 60000u, "worst room took %u ops", max_ops);
    CHECK(max_ops > 0);
}

TEST(home_visit_variants)
{
    HomeRoom a;
    HomeVisit v;
    DoomoResident *r;
    int id = 10, i, candles = 0;
    town_new(TOWN_FREEPLAY, 99, TOWN_POP_RANDOM);
    home_reset();
    r = town_res(id);
    r->status = RES_POSSESSED;
    CHECK(home_visit_gen(id, &a, &v));
    CHECK_EQ(v.kind, HVK_POSSESSED);
    CHECK_EQ(a.wall_paint, PAINT_BLOOD);
    r->status = RES_GHOST;
    home_visit_gen(id, &a, &v);
    CHECK_EQ(v.kind, HVK_GHOST);
    for (i = 0; i < a.nobj; i++) {
        const DoomoFurn *f = home_furn(HO_FURN(&a.obj[i]));
        if (f && f->paint_a != 0xFF && !(f->flags2 & FF_PORTAL)) CHECK_EQ(a.obj[i].pa & 0x1F, PAINT_SILVER);
    }
    r->status = RES_DEAD;
    home_visit_gen(id, &a, &v);
    CHECK_EQ(v.kind, HVK_SEALED);
    CHECK_EQ(v.music, SONG_NONE);
    for (i = 0; i < a.nobj; i++) {
        const DoomoFurn *f = home_furn(HO_FURN(&a.obj[i]));
        if (f && (f->role == FR_CANDLE)) candles++;
    }
    CHECK(candles >= 1);
    r->status = RES_ALIVE;
    r->flags |= RF_TRAVELER;
    home_visit_gen(id, &a, &v);
    CHECK_EQ(v.kind, HVK_AWAY);
    r->status = RES_EMPTY;
    home_visit_gen(id, &a, &v);
    CHECK_EQ(v.kind, HVK_VACANT);
}

TEST(home_visit_gifts_wants_reactions)
{
    HomeRoom a, b;
    HomeVisit v;
    DoomoResident *r;
    uint8_t w[3], w2[3];
    int id = 3, x, y, slot = -1, met, thanks, lvl;
    const char *line;
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_RANDOM);
    home_reset();
    home_move_in();
    r = town_res(id);
    r->status = RES_ALIVE;
    r->flags &= (uint8_t)~RF_TRAVELER;
    CHECK(home_visit_gen(id, &a, &v));
    home_grant(FURN_CUPCAKE_PLUSH);
    /* a free floor tile for the gift */
    for (y = 1; y < 6 && slot < 0; y++)
        for (x = 1; x < 6 && slot < 0; x++)
            if (home_fit(&a, v.room, FURN_GARDEN_GNOME, x, y, 0, -1, NULL) == HPR_OK) {
                home_grant(FURN_GARDEN_GNOME);
                slot = home_visit_gift(id, FURN_GARDEN_GNOME, x, y, 0, -1);
                CHECK(slot >= 0);
                home_visit_gen(id, &b, &v);
                CHECK_EQ(v.gifts, 1);
                CHECK(home_obj_at(&b, x, y) >= 0);
                CHECK_EQ(HO_FURN(&b.obj[home_obj_at(&b, x, y)]), FURN_GARDEN_GNOME);
            }
    CHECK(slot >= 0);
    CHECK_EQ(home_visit_gift_count(id), 1);
    CHECK_EQ(home_owned(FURN_GARDEN_GNOME), 0);                 /* it left storage */
    /* wants are stable within the week and change across weeks */
    home_visit_wants(id, w);
    home_visit_wants(id, w2);
    CHECK(memcmp(w, w2, 3) == 0);
    CHECK(w[0] != w[1] && w[1] != w[2] && w[0] != w[2]);
    /* a met want raises friendship by 8 */
    lvl = r->level > 91 ? 91 : r->level;
    r->level = (uint8_t)lvl;
    met = home_visit_thank(id, FURN_CUPCAKE_PLUSH, PAINT_BUBBLEGUM, &thanks);
    if (met) CHECK_EQ(r->level, lvl + 8);
    else CHECK_EQ(r->level, lvl);
    /* reactions: a fear trigger always makes them flee */
    r->fear = 10;                                                 /* Mirrors */
    CHECK_EQ(home_visit_react(r, FURN_HEART_MIRROR, -1, &line), HVR_FLEE);
    CHECK(line && line[0]);
    r->fear = 0;
    r->like_color = PAINT_LIME;
    CHECK_EQ(home_visit_react(r, FURN_GARDEN_GNOME, PAINT_LIME, &line) == HVR_HEARTS ||
             home_visit_react(r, FURN_GARDEN_GNOME, PAINT_LIME, &line) == HVR_BANG, 1);
    /* the visitor log */
    home_visit_log(id);
    home_visit_log(5);
    home_visit_log(id);
    CHECK_EQ(home_visit_recent(0), id);
    CHECK_EQ(home_visit_recent(1), 5);
    CHECK_EQ(home_visit_recent(2), -1);
    CHECK(home_visited(id) && home_visited(5) && !home_visited(6));
}
