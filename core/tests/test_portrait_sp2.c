/* test_portrait_sp2.c - SP2 (X1): the portrait cache's finer slices, stand-ins, portrait_hold(3), render-ahead
 * (portrait_prefetch / figure_prefetch) and the idle-time pump (portrait_idle_next / portrait_idle_run).
 *
 * - a whole render cut into its 26 slices (one a tick, other renders in between) is the same picture as one in a
 *   tick (the slices are the same calls in the same order), for busts, figures and the turkey;
 * - with stand-ins on (the GBA / PS1 default), a new expression shows the old picture for at most PC_STANDIN
 *   ticks, then the new one, and every picture shown is a whole render of one of the two;
 * - portrait_hold(3) with nothing cached gives NULL for a few ticks, then the picture;
 * - a prefetch is drawn by idle-time slices only, and is then an exact hit equal to a fresh render. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/portrait.h"

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

/* the hashes of every whole render the cache could show for (id, expr) over ticks [0, n): native renders whole
 * jobs in the tick they start, so this is the truth for each tick */
static void truth(int id, int figure, int expr, int n, uint32_t *h)
{
    int t;
    portrait_cache_reset();
    portrait_cache_standin(0);
    for (t = 0; t < n; t++) {
        const DoomoImage *img;
        portrait_cache_clock((uint32_t)t, 99);
        img = figure ? figure_image(RES_LOOK_VIEW(town_res(id)), RES_ALIVE, expr) : portrait_image(id, expr);
        h[t] = fnv(img->pix, img->w * img->h);
    }
    portrait_cache_reset();
}

static int found(uint32_t v, const uint32_t *h, int n)
{
    int k;
    for (k = 0; k < n; k++) if (h[k] == v) return 1;
    return 0;
}

TEST(sp2_one_slice_a_tick_with_noise_shows_only_whole_renders)
{
    enum { N = 160 };
    static uint32_t ha[N], hb[N];
    static uint8_t junk[FIGURE_W * FIGURE_H];
    DoomoResident other;
    int ids[3], j, fig;
    ids[0] = new_resident(777u, 0);
    ids[1] = new_resident(31337u, 1);
    ids[2] = town_alloc(DISTRICT_HILLVILE);                 /* the turkey: its head is four pieces */
    CHECK(ids[0] >= 0 && ids[1] >= 0 && ids[2] >= 0);
    *town_res(ids[2]) = *roster_premade(ROSTER_ID_MAYOR);
    town_res(ids[2])->status = RES_ALIVE;
    portrait_invalidate(ids[2]);
    RES_SET_LOOK(town_res(ids[1]), GF_AURA, AURA_HEARTS);  /* more phases */
    resident_generate_hellborn(&other, 4242u);
    for (j = 0; j < 3; j++)
        for (fig = 0; fig < 2; fig++) {
            int t, ok = 1, changes = 0;
            truth(ids[j], fig, EXPR_LOVE, N, ha);
            portrait_cache_reset();
            portrait_cache_standin(0);
            for (t = 0; t < N; t++) {
                const DoomoImage *img;
                /* steps 1 = 4 slices a tick; noise renders clobber the canvas between them */
                portrait_cache_clock((uint32_t)t, 1);
                img = fig ? figure_image(RES_LOOK_VIEW(town_res(ids[j])), RES_ALIVE, EXPR_LOVE)
                          : portrait_image(ids[j], EXPR_LOVE);
                hb[t] = fnv(img->pix, img->w * img->h);
                if (!found(hb[t], ha, N)) ok = 0;
                if (t && hb[t] != hb[t - 1]) changes++;
                if (t % 5 == 2) portrait_render(RES_LOOK_VIEW(&other), RES_ALIVE, EXPR_SAD, t, junk);
                if (t % 7 == 4) (void)bodyspr_image(ids[j], t & 3, (t >> 2) & 3);
            }
            CHECK_MSG(ok, "look %d %s: a sliced frame matches no whole render", j, fig ? "figure" : "bust");
            CHECK_MSG(changes >= 5, "look %d %s: only %d changes", j, fig ? "figure" : "bust", changes);
        }
    portrait_cache_reset();
    for (j = 0; j < 3; j++) town_free(ids[j]);
}

TEST(sp2_standin_shows_the_old_face_for_at_most_four_ticks)
{
    enum { N = 40 };
    static uint32_t hn[N], hh[N];
    int id = new_resident(9001u, 0), t, swap = 10, first_new = -1, ok = 1;
    CHECK(id >= 0);
    truth(id, 0, EXPR_NEUTRAL, N, hn);
    truth(id, 0, EXPR_HAPPY, N, hh);
    portrait_cache_reset();
    portrait_cache_standin(4);
    for (t = 0; t < N; t++) {
        const DoomoImage *img;
        uint32_t v;
        portrait_cache_clock((uint32_t)t, 1);
        img = portrait_image(id, t < swap ? EXPR_NEUTRAL : EXPR_HAPPY);
        CHECK(img != NULL);
        v = fnv(img->pix, img->w * img->h);
        if (t < swap) {
            if (!found(v, hn, N)) ok = 0;
        } else if (found(v, hh, N)) {
            if (first_new < 0) first_new = t;
        } else {
            if (!found(v, hn, N)) ok = 0;                   /* the stand-in is a whole neutral face */
            if (first_new >= 0) ok = 0;                     /* never back to the old face */
        }
    }
    CHECK_MSG(ok, "a frame was neither a whole neutral nor a whole happy render");
    CHECK_MSG(first_new >= swap && first_new <= swap + 4, "the new face came at tick %d (asked at %d)",
              first_new, swap);
    portrait_cache_reset();
    town_free(id);
}

TEST(sp2_hold3_gives_null_then_the_face)
{
    int id = new_resident(4711u, 1), t, nulls = 0, got = -1;
    CHECK(id >= 0);
    portrait_cache_reset();
    portrait_cache_standin(4);
    for (t = 0; t < 12 && got < 0; t++) {
        const DoomoImage *img;
        portrait_cache_clock((uint32_t)t, 1);
        portrait_hold(3);
        img = portrait_image(id, EXPR_SHOCK);
        portrait_hold(0);
        if (!img) nulls++;
        else got = t;
    }
    CHECK_MSG(got >= 0 && got <= 4, "hold 3: the face came at tick %d", got);
    CHECK(nulls == got);
    /* with stand-ins off (native), hold 3 draws at once: no NULL */
    portrait_cache_reset();
    portrait_cache_standin(0);
    portrait_cache_clock(0, 0);
    portrait_hold(3);
    CHECK(portrait_image(id, EXPR_ANGRY) != NULL);
    portrait_hold(0);
    portrait_cache_reset();
    town_free(id);
}

TEST(sp2_prefetch_is_drawn_in_idle_time_and_is_an_exact_hit)
{
    int id = new_resident(2024u, 0), n = 0, cls;
    uint32_t h0, h1;
    const DoomoImage *img;
    CHECK(id >= 0);
    portrait_cache_reset();
    portrait_cache_clock(100, 1);
    (void)portrait_image(id, EXPR_NEUTRAL);                 /* (starts a look-ahead job: idle time runs it) */
    for (n = 0; portrait_idle_next() >= 0 && n < 100; n++) portrait_idle_run();
    CHECK(n <= 26);
    CHECK(portrait_idle_next() < 0);                        /* nothing queued, no job */
    n = 0;
    CHECK(portrait_prefetch(id, EXPR_SAD) == 0);            /* queued */
    while ((cls = portrait_idle_next()) >= 0 && n < 100) {
        CHECK(cls < PORTRAIT_SLICE_CLASSES);
        portrait_idle_run();
        n++;
    }
    CHECK_MSG(n >= 20 && n <= 30, "the prefetch took %d idle slices", n);
    CHECK(portrait_prefetch(id, EXPR_SAD) == 1);            /* cached now: nothing to queue */
    h0 = portrait_hitcheck_count();
    h1 = portrait_hitcheck_fails();
    portrait_cache_clock(109, 1);                           /* the phase it was drawn for (PC_AHEAD on) */
    img = portrait_image(id, EXPR_SAD);
    CHECK(img != NULL);
    CHECK(portrait_hitcheck_count() == h0 + 1);             /* an exact hit, re-rendered and compared */
    CHECK(portrait_hitcheck_fails() == h1);
    /* a figure too (after the portrait's own look-ahead job, if it started one) */
    for (n = 0; portrait_idle_next() >= 0 && n < 100; n++) portrait_idle_run();
    CHECK(figure_prefetch(RES_LOOK_VIEW(town_res(id)), RES_ALIVE, EXPR_LOVE) == 0);
    for (n = 0; portrait_idle_next() >= 0 && n < 100; n++) portrait_idle_run();
    CHECK(n >= 20 && n <= 30);
    CHECK(figure_prefetch(RES_LOOK_VIEW(town_res(id)), RES_ALIVE, EXPR_LOVE) == 1);
    portrait_cache_reset();
    town_free(id);
}
