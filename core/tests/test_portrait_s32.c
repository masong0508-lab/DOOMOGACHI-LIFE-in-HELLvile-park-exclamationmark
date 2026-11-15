/* test_portrait_s32.c - S3-2 (docs/design/SPEED_AUDIT3.md): no whole portrait, figure or chibi inside a drawn frame.
 *
 * - portrait_image_ui / figure_image_res_ui / bodyspr_image_ui (a UI preview): with stand-ins on and nothing cached
 *   the first tick gives NULL (the plain calls draw the whole image at once), then the picture, equal to one drawn
 *   at once; with stand-ins off (native, Dreamcast) it is there at once; the caller's own hold / lazy mode is kept;
 * - a neighbour prefetched in idle time is on screen at the first tick the cursor lands on it. */
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

#define K_PORTRAIT 0
#define K_FIGURE   1
#define K_BODY     2

static const DoomoImage *ask(int kind, int ui, int id)
{
    if (kind == K_PORTRAIT) return ui ? portrait_image_ui(id, EXPR_SHOCK) : portrait_image(id, EXPR_SHOCK);
    if (kind == K_FIGURE) {
        const DoomoResident *r = town_res(id);
        return ui ? figure_image_res_ui(r, RES_ALIVE, EXPR_SHOCK) : figure_image_res(r, RES_ALIVE, EXPR_SHOCK);
    }
    return ui ? bodyspr_image_ui(id, 1, 2) : bodyspr_image(id, 1, 2);
}

static uint32_t at_once(int kind, int id)
{
    const DoomoImage *img;
    portrait_cache_reset();
    portrait_cache_standin(0);
    portrait_cache_clock(0, 0);
    img = ask(kind, 0, id);
    return img ? fnv(img->pix, img->w * img->h) : 0;
}

static void ui_call_is_a_job(int kind)
{
    int id = new_resident(4242u + (uint32_t)kind, kind & 1), t, got = -1;
    uint32_t want = at_once(kind, id);
    CHECK(id >= 0 && want != 0);
    portrait_cache_reset();
    portrait_cache_standin(4);
    for (t = 0; t < 40 && got < 0; t++) {
        const DoomoImage *img;
        portrait_cache_clock((uint32_t)t, 1);
        img = ask(kind, 1, id);
        if (t == 0) CHECK_MSG(img == NULL, "kind %d: the first tick drew a whole image", kind);
        if (img) {
            got = t;
            CHECK_MSG(fnv(img->pix, img->w * img->h) == want, "kind %d: differs from one drawn at once", kind);
        }
    }
    CHECK_MSG(got >= 1 && got <= 30, "kind %d: ready at tick %d", kind, got);
    /* stand-ins off: ready at once */
    portrait_cache_reset();
    portrait_cache_standin(0);
    portrait_cache_clock(0, 0);
    CHECK(ask(kind, 1, id) != NULL);
    portrait_cache_reset();
    town_free(id);
}

TEST(s32_a_ui_portrait_is_a_job_not_a_whole_render) { ui_call_is_a_job(K_PORTRAIT); }
TEST(s32_a_ui_figure_is_a_job_not_a_whole_render) { ui_call_is_a_job(K_FIGURE); }
TEST(s32_a_ui_chibi_is_a_job_not_a_whole_render) { ui_call_is_a_job(K_BODY); }

TEST(s32_the_callers_own_mode_is_kept)
{
    int id = new_resident(515u, 0);
    CHECK(id >= 0);
    portrait_cache_reset();
    portrait_cache_standin(4);
    portrait_cache_clock(0, 1);
    portrait_hold(2);                       /* a holding caller: NULL until ready, as hold 2 says */
    (void)portrait_image_ui(id, EXPR_HAPPY);
    portrait_hold(0);
    /* after the call hold is what the caller had: a plain call with nothing cached draws at once (hold 0, no stand-in) */
    portrait_cache_reset();
    portrait_cache_standin(4);
    portrait_cache_clock(0, 1);
    CHECK(portrait_image_ui(id, EXPR_ANGRY) == NULL);
    CHECK(portrait_image(id, EXPR_SAD) != NULL);
    portrait_cache_reset();
    town_free(id);
}

TEST(s32_a_neighbour_prefetch_is_drawn_in_idle_time_and_then_shown_without_a_job)
{
    int a = new_resident(60u, 0), b = new_resident(73u, 1), n;
    const DoomoImage *img;
    CHECK(a >= 0 && b >= 0);
    portrait_cache_reset();
    portrait_cache_standin(4);
    portrait_cache_clock(100, 1);
    CHECK(portrait_prefetch(a, EXPR_HAPPY) == 0);               /* a list cursor asks for its neighbours */
    for (n = 0; portrait_idle_next() >= 0 && n < 100; n++) portrait_idle_run();
    CHECK(n >= 20);
    CHECK(portrait_prefetch(a, EXPR_HAPPY) == 1);               /* cached in idle time */
    CHECK(portrait_prefetch(b, EXPR_HAPPY) == 0);
    for (n = 0; portrait_idle_next() >= 0 && n < 100; n++) portrait_idle_run();
    /* the cursor moves to a: its picture is there on the first tick, from an idle-time job */
    portrait_cache_clock(101, 1);
    img = portrait_image_ui(a, EXPR_HAPPY);
    CHECK(img != NULL);
    portrait_cache_reset();
    town_free(a);
    town_free(b);
}
