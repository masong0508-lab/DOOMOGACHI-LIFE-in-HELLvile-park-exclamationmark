/* test_weld_slice.c - S2-9: the weapon picture composed in slices (weld.h weld_view_image_sliced /
 * weld_view_prefetch; core/src/weld_slice.c; docs/GBA.md "Speed wave 2: S2-9").
 *
 *   - weld_slice.c builds weld_gfx.c's recipes a second time on primitives of its own (units a pass can stop
 *     between, faster rows). Its pictures must be weld_view_image()'s, pixel for pixel: every frame type in both
 *     styles and thousands of random weapons, all four animation frames, composed at once and in slices of every
 *     size down to one unit.
 *   - The job: nothing half drawn is ever returned, the old picture stays up meanwhile, a prefetch never takes
 *     the slot on show nor one asked for in the same tick, a slot taken by someone else is not written to, and
 *     the two far slots (the GBA's: pictures of the portrait cache's pool) hold two more pictures until the pool
 *     takes them back.
 */
#include <stdlib.h>
#include "test.h"
#include "doomo/scene.h"
#include "doomo/portrait.h"
#include "../src/weld_gfx_int.h"

#define ONE_PASS (1 << 28)              /* a slice no picture fills: composed in one weld_idle_run() */

#define NPIX (WELD_VIEW_W * WELD_VIEW_H)

static void random_weapon(DoomoRng *r, DoomoWeapon *w)
{
    memset(w, 0, sizeof(*w));
    w->frame = (uint8_t)rng_range(r, WFRAME_COUNT);
    w->barrel = (uint8_t)rng_range(r, WBARREL_COUNT);
    w->core = (uint8_t)rng_range(r, WCORE_COUNT);
    w->mod = (uint8_t)rng_range(r, WMOD_COUNT);
    w->sight = (uint8_t)rng_range(r, WSIGHT_COUNT);
    w->stock = (uint8_t)rng_range(r, WSTOCK_COUNT);
    w->mag = (uint8_t)rng_range(r, WMAG_COUNT);
    w->under = (uint8_t)rng_range(r, WUNDER_COUNT);
    w->charm = (uint8_t)rng_range(r, WCHARM_COUNT);
    w->style = (uint8_t)rng_range(r, 2);
    w->paint = (uint8_t)rng_range(r, 32);
    w->accent = (uint8_t)rng_range(r, 32);
    w->quality = 60;
    w->flags = WF_VALID;
}

/* the picture of (w, frame) by the sliced build, `units` to a slice; *slices = the slices it took. Returns its
 * pixels, or NULL when something else than a finished picture ever came back. */
static const uint8_t *sliced(const DoomoWeapon *w, int frame, int units, int *slices)
{
    const DoomoImage *img;
    int ready = 0, n = 0;
    weld_gfx_invalidate();                      /* the cache is shared: compose it again */
    weld_slice_test(1, units, 0);
    img = weld_view_image_sliced(w, frame, &ready);
    while (!ready) {
        if (img || n > 200000 || weld_idle_next() < 0) return NULL;    /* nothing was on show: nothing to return yet */
        weld_idle_run(0);
        n++;
        img = weld_view_image_sliced(w, frame, &ready);
    }
    if (slices) *slices = n;
    weld_slice_test(0, 0, 0);
    return img && img->w == WELD_VIEW_W && img->h == WELD_VIEW_H ? img->pix : NULL;
}

static int same_as_plain(const DoomoWeapon *w, int frame, int units, int *slices)
{
    static uint8_t ref[NPIX];
    const uint8_t *got;
    weld_gfx_invalidate();
    memcpy(ref, weld_view_image(w, frame)->pix, NPIX);
    got = sliced(w, frame, units, slices);
    return got && memcmp(ref, got, NPIX) == 0;
}

TEST(weld_slice_every_frame_type_is_the_plain_picture)
{
    static const int sizes[] = { ONE_PASS, 350, 97, 13 };
    DoomoWeapon w;
    int f, s, fr, k;
    for (f = 0; f < WFRAME_COUNT; f++)
        for (s = 0; s < 2; s++) {
            weld_default(&w, f, s);
            for (fr = 0; fr < 4; fr++)
                for (k = 0; k < (int)DOOMO_COUNTOF(sizes); k++)
                    CHECK_MSG(same_as_plain(&w, fr, sizes[k], NULL), "frame type %d style %d anim %d, slices of %d",
                              f, s, fr, sizes[k]);
        }
}

TEST(weld_slice_random_weapons_are_the_plain_picture)
{
    DoomoRng r;
    DoomoWeapon w;
    int i, most = 0;
    rng_seed(&r, 20261112);
    for (i = 0; i < 6000; i++) {
        /* at once, the game's slice, and odd sizes down to a few units; now and then one unit a slice */
        int fr = (int)rng_range(&r, 4), pick = (int)rng_range(&r, 8), n = 0;
        int units = pick == 0 ? ONE_PASS : pick == 1 ? 350 : pick == 2 ? 1 + (int)rng_range(&r, 12)
                    : 20 + (int)rng_range(&r, 3000);
        random_weapon(&r, &w);
        if (pick == 2 && (i % 40) != 0) units += 40;            /* (one unit a slice takes ~10,000 slices) */
        CHECK_MSG(same_as_plain(&w, fr, units, &n),
                  "weapon %d (frame type %d barrel %d core %d mod %d sight %d stock %d mag %d under %d charm %d "
                  "style %d paints %d/%d) anim %d, slices of %d", i, w.frame, w.barrel, w.core, w.mod, w.sight,
                  w.stock, w.mag, w.under, w.charm, w.style, w.paint, w.accent, fr, units);
        if (units == 350 && n > most) most = n;
    }
    /* a picture is a few dozen slices of the in-frame size at most */
    CHECK_MSG(most >= 12 && most <= 60, "the longest composition took %d slices of 350 units", most);
}

/* ---- the job ------------------------------------------------------------------------------------------- */
static uint8_t ref_pix[4][NPIX];        /* the plain pictures of the test weapons' frame 0 */
static DoomoWeapon tw[4];

static void job_setup(int far)
{
    int i;
    test_reset();
    for (i = 0; i < 4; i++) {
        weld_default(&tw[i], i == 0 ? WFRAME_PISTOL : i == 1 ? WFRAME_PUMP_SHOTGUN : i == 2 ? WFRAME_SMG : WFRAME_ASSAULT_RIFLE, i & 1);
        weld_gfx_invalidate();
        memcpy(ref_pix[i], weld_view_image(&tw[i], 0)->pix, NPIX);
    }
    weld_gfx_invalidate();
    weld_slice_test(1, 350, far);
}

static int is_ref(const DoomoImage *img, int i)
{
    return img && img->w == WELD_VIEW_W && img->h == WELD_VIEW_H && memcmp(img->pix, ref_pix[i], NPIX) == 0;
}

/* idle slices until nothing is under way; returns how many ran */
static int run_out(void)
{
    int n = 0;
    while (weld_idle_next() >= 0 && n < 1000) { weld_idle_run(0); n++; }
    return n;
}

/* a tick goes by (the title screen: it leaves the scene scratch alone) */
static void tick(void) { plat_begin_frame(); doomo_frame(0); plat_end_frame(); }

TEST(weld_slice_old_picture_stays_up_and_nothing_half_drawn_shows)
{
    const DoomoImage *img, *old;
    int ready = 1, n = 0;
    job_setup(0);
    /* the first picture: nothing to show until it is whole */
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(img == NULL && ready == 0);
    while (!ready && n++ < 1000) {
        CHECK(img == NULL);
        weld_idle_run(0);
        img = weld_view_image_sliced(&tw[0], 0, &ready);
    }
    CHECK(n > 3 && is_ref(img, 0));
    /* the next one: the first stays up, untouched, until the call that says ready */
    old = img;
    n = 0;
    img = weld_view_image_sliced(&tw[1], 0, &ready);
    CHECK(ready == 0);
    while (!ready && n++ < 1000) {
        CHECK(img == old && is_ref(img, 0));
        weld_idle_run(0);
        img = weld_view_image_sliced(&tw[1], 0, &ready);
    }
    CHECK(n > 3 && img != old && is_ref(img, 1) && is_ref(old, 0));
    /* both are cached now: asked again they are ready at once and nothing is under way */
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 0) && weld_idle_next() < 0);
    weld_slice_test(0, 0, 0);
}

TEST(weld_slice_prefetch_keeps_off_the_pictures_in_use)
{
    const DoomoImage *img;
    int ready = 0;
    job_setup(0);
    (void)weld_view_image_sliced(&tw[0], 0, &ready);
    run_out();
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 0));
    /* ahead: the second picture goes to the other slot ... */
    CHECK(weld_view_prefetch(&tw[1], 0) == 0);
    CHECK(run_out() > 3);
    CHECK(is_ref(img, 0));
    CHECK(weld_view_prefetch(&tw[1], 0) == 1);
    /* ... and a third finds no slot in this tick: one is on show, the other was asked for */
    CHECK(weld_view_prefetch(&tw[2], 0) == 0);
    CHECK(weld_idle_next() < 0);
    CHECK(is_ref(img, 0) && weld_view_prefetch(&tw[1], 0) == 1);
    /* a tick later, the picture on show asked again: the third may take the slot nobody asked for (the second's) */
    tick();
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 0));
    CHECK(weld_view_prefetch(&tw[2], 0) == 0);
    CHECK(run_out() > 3);
    CHECK(is_ref(img, 0) && weld_view_prefetch(&tw[2], 0) == 1);
    /* a prefetch does not replace a picture someone waits for; the waited one replaces a prefetch */
    tick();
    img = weld_view_image_sliced(&tw[3], 0, &ready);            /* waits (the first stays up) */
    CHECK(ready == 0 && is_ref(img, 0));
    CHECK(weld_view_prefetch(&tw[1], 0) == 0);
    run_out();
    img = weld_view_image_sliced(&tw[3], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 3));
    tick();
    img = weld_view_image_sliced(&tw[3], 0, &ready);
    CHECK(weld_view_prefetch(&tw[1], 0) == 0);                  /* under way ... */
    weld_idle_run(0);
    img = weld_view_image_sliced(&tw[2], 0, &ready);            /* ... dropped for the one that is wanted */
    CHECK(ready == 0 && is_ref(img, 3));
    run_out();
    img = weld_view_image_sliced(&tw[2], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 2));
    weld_slice_test(0, 0, 0);
}

TEST(weld_slice_a_slot_taken_by_someone_else_is_left_alone)
{
    const DoomoImage *img;
    uint8_t *blk;
    int ready = 0, i, clean = 1;
    job_setup(0);
    (void)weld_view_image_sliced(&tw[0], 0, &ready);
    weld_idle_run(0);
    CHECK(weld_idle_next() >= 0);
    /* another scene claims the scratch: the job is over, nothing more is written there */
    blk = (uint8_t *)scene_scratch(SAVE_EXT_TAG('T', 'E', 'S', 'T'), DOOMO_SCENE_SCRATCH);
    memset(blk, 0xA5, DOOMO_SCENE_SCRATCH);
    CHECK(weld_idle_next() < 0);
    weld_idle_run(0);
    for (i = 0; i < (int)DOOMO_SCENE_SCRATCH; i++) if (blk[i] != 0xA5) clean = 0;
    CHECK(clean);
    /* back in the workshop: composed again from the start */
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(img == NULL && ready == 0);
    run_out();
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 0));
    /* weld_view_image() (another scene's call) composes over the slot of a picture under way: that job is over */
    (void)weld_view_image_sliced(&tw[1], 0, &ready);
    weld_idle_run(0);
    CHECK(is_ref(weld_view_image(&tw[2], 0), 2));
    CHECK(weld_idle_next() < 0);
    /* weld_gfx_invalidate(): what was cached and what was under way is composed again */
    (void)weld_view_image_sliced(&tw[1], 0, &ready);
    run_out();
    (void)weld_view_image_sliced(&tw[1], 0, &ready);
    CHECK(ready == 1);
    weld_gfx_invalidate();
    img = weld_view_image_sliced(&tw[1], 0, &ready);
    CHECK(img == NULL && ready == 0);
    run_out();
    img = weld_view_image_sliced(&tw[1], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 1));
    weld_slice_test(0, 0, 0);
}

TEST(weld_slice_far_slots_hold_two_more_pictures_until_the_pool_takes_them)
{
    const DoomoImage *img;
    int ready = 0, i, lost = 0;
    job_setup(2);
    /* four pictures, one a tick: the first on show, the others ahead */
    for (i = 0; i < 4; i++) {
        (void)weld_view_image_sliced(&tw[0], 0, &ready);
        if (i) CHECK(weld_view_prefetch(&tw[i], 0) == 0);
        CHECK(run_out() > 3);
        tick();
    }
    for (i = 0; i < 4; i++) {
        img = weld_view_image_sliced(&tw[i], 0, &ready);
        CHECK_MSG(ready == 1 && is_ref(img, i), "picture %d of four is cached", i);
    }
    CHECK(weld_idle_next() < 0);
    /* the pool gives its blocks away: the far pictures are gone, weld_gfx.c's two stay */
    portrait_cache_reset();
    for (i = 0; i < 4; i++) {
        tick();
        img = weld_view_image_sliced(&tw[i], 0, &ready);
        if (!ready) lost++;
        run_out();
        img = weld_view_image_sliced(&tw[i], 0, &ready);
        CHECK(ready == 1 && is_ref(img, i));
    }
    CHECK_MSG(lost == 2, "%d pictures had to be composed again", lost);
    /* a far slot loses its blocks while a picture is composed into it: the job is over, nothing is written */
    weld_gfx_invalidate();
    portrait_cache_reset();
    tick();
    (void)weld_view_image_sliced(&tw[0], 0, &ready);
    run_out();
    (void)weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(weld_view_prefetch(&tw[1], 0) == 0);
    run_out();
    tick();
    (void)weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(weld_view_prefetch(&tw[1], 0) == 1);
    CHECK(weld_view_prefetch(&tw[2], 0) == 0);                  /* into a far slot */
    weld_idle_run(0);
    CHECK(weld_idle_next() >= 0);
    portrait_cache_reset();
    CHECK(weld_idle_next() < 0);
    weld_idle_run(0);
    CHECK(weld_view_prefetch(&tw[2], 0) == 0);                  /* started again */
    run_out();
    img = weld_view_image_sliced(&tw[2], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 2));
    weld_slice_test(0, 0, 0);
}

TEST(weld_slice_without_slices_is_weld_view_image)
{
    const DoomoImage *img;
    int ready = 0;
    job_setup(0);
    weld_slice_test(0, 0, 0);
    CHECK(weld_view_prefetch(&tw[0], 0) == 0);                  /* nothing is composed ahead */
    CHECK(weld_idle_next() < 0);
    img = weld_view_image_sliced(&tw[0], 0, &ready);
    CHECK(ready == 1 && img == weld_view_image(&tw[0], 0) && is_ref(img, 0));
    CHECK(weld_view_prefetch(&tw[0], 0) == 1);
}

/* The PS1's way (mode 2): slices in the asking call while the platform has room for one more. The native
 * platform always has: a picture is whole in the call that asks for it, and the same picture. */
TEST(weld_slice_in_frame_slices_finish_in_the_call)
{
    const DoomoImage *img;
    int ready = 0;
    job_setup(0);
    weld_slice_test(2, 350, 0);
    img = weld_view_image_sliced(&tw[1], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 1));
    CHECK(weld_view_prefetch(&tw[2], 0) == 1);
    img = weld_view_image_sliced(&tw[2], 0, &ready);
    CHECK(ready == 1 && is_ref(img, 2));
    weld_slice_test(0, 0, 0);
}
