/* test_portrait_s27.c - S2-7 (docs/SPEED_WAVE2.md): the credits parade's body path.
 *
 * - bodyspr_lazy(3) (every platform): a body the cache lacks is drawn by the job slot over ticks, never at once;
 *   meanwhile it is NULL (not drawn), at most one new body is finished a tick, and each one, once there, is the
 *   same picture as a body drawn at once;
 * - the look-hash memo keeps working when a tick asks for more bodies than it has slots (the parade asks for
 *   ~46): every picture is still the right one. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/portrait.h"

#define N_LAZY 4
#define N_MANY 60

static uint32_t fnv(const uint8_t *p, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static int new_resident(uint32_t seed, int hell)
{
    DoomoResident *r;
    int id = town_alloc(hell ? DISTRICT_HELLVILE : DISTRICT_HILLVILE);
    if (id < 0) return id;
    r = town_res(id);
    if (hell) resident_generate_hellborn(r, seed);
    else resident_generate(r, seed);
    r->status = RES_ALIVE;
    portrait_invalidate(id);
    return id;
}

static uint32_t at_once(int id, int dir, int frame)
{
    const DoomoImage *img;
    portrait_cache_reset();
    portrait_cache_clock(0, 0);
    img = bodyspr_image(id, dir, frame);
    return img ? fnv(img->pix, img->w * img->h) : 0;
}

TEST(s27_lazy3_draws_one_body_at_a_time_and_exactly)
{
    int ids[N_LAZY], i, t, done_at[N_LAZY];
    uint32_t want[N_LAZY];
    for (i = 0; i < N_LAZY; i++) {
        ids[i] = new_resident(900u + (uint32_t)i * 77u, i & 1);
        CHECK(ids[i] >= 0);
        want[i] = at_once(ids[i], 2, 0);
        CHECK(want[i] != 0);
        done_at[i] = -1;
    }
    portrait_cache_reset();
    portrait_cache_standin(4);
    for (t = 0; t < 60; t++) {
        int fresh = 0;
        portrait_cache_clock((uint32_t)t, 1);               /* one layer group's worth of slices a tick */
        bodyspr_lazy(3);
        for (i = 0; i < N_LAZY; i++) {
            const DoomoImage *img = bodyspr_image(ids[i], 2, 0);
            if (!img) {
                CHECK_MSG(done_at[i] < 0, "body %d vanished at tick %d", i, t);
                continue;
            }
            CHECK_MSG(fnv(img->pix, img->w * img->h) == want[i], "body %d differs from one drawn at once", i);
            if (done_at[i] < 0) { done_at[i] = t; fresh++; }
        }
        bodyspr_lazy(0);
        CHECK_MSG(fresh <= 1, "%d new bodies in tick %d", fresh, t);
    }
    for (i = 0; i < N_LAZY; i++) {
        CHECK_MSG(done_at[i] >= 1, "body %d was drawn at once (tick %d)", i, done_at[i]);
        /* each job is due like a stand-in's (PC_STANDIN ticks), one after the other */
        CHECK_MSG(done_at[i] <= 6 * (i + 1), "body %d only came at tick %d", i, done_at[i]);
    }
    /* the modes 1 / 2 stay the GBA's: on native they are no-ops (drawn at once) */
    portrait_cache_reset();
    portrait_cache_clock(100, 1);
    bodyspr_lazy(1);
    CHECK(bodyspr_image(ids[0], 1, 1) != NULL);
    bodyspr_lazy(0);
    portrait_cache_reset();
    for (i = 0; i < N_LAZY; i++) town_free(ids[i]);
}

TEST(s27_memo_with_more_bodies_than_slots_gives_the_right_pictures)
{
    static uint32_t want[N_MANY];
    int ids[N_MANY], i, t, n = 0;
    for (i = 0; i < N_MANY; i++) {
        ids[i] = new_resident(5000u + (uint32_t)i * 13u, i % 3 == 0);
        if (ids[i] < 0) break;
        n++;
    }
    CHECK(n >= 50);
    for (i = 0; i < n; i++) want[i] = at_once(ids[i], 0, 0);
    portrait_cache_reset();
    for (t = 0; t < 3; t++) {                               /* the same 50+ a tick: the memo is full every tick */
        portrait_cache_clock((uint32_t)(200 + t), 0);
        for (i = 0; i < n; i++) {
            const DoomoImage *img = bodyspr_image(ids[i], 0, 0);
            CHECK(img != NULL);
            if (img) CHECK_MSG(fnv(img->pix, img->w * img->h) == want[i], "tick %d body %d", t, i);
        }
    }
    /* a look edit is still seen (the memo compares the genes of every body it serves) */
    {
        DoomoResident *r = town_res(ids[0]);
        const DoomoImage *img;
        uint32_t before = want[0];
        resident_generate(r, 999999u);
        r->status = RES_ALIVE;
        portrait_cache_clock(300, 0);
        img = bodyspr_image(ids[0], 0, 0);
        CHECK(img != NULL && fnv(img->pix, img->w * img->h) == at_once(ids[0], 0, 0));
        CHECK(at_once(ids[0], 0, 0) != before);
    }
    portrait_cache_reset();
    for (i = 0; i < n; i++) town_free(ids[i]);
}
