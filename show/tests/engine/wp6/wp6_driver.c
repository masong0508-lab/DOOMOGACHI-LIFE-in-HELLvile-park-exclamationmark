/* wp6_driver.c - the WP6 test program on the PS1 (show/tests/engine/wp6).
 *
 * Linked into a TEST build of show.exe only (show/build/test-wp6/, through
 * wp6_inject.cmake): it takes the frame through show_game.c's self-test hook
 * (show_selftest_init / show_selftest_frame) and plays gen_wp6.py's scenes one
 * after the other, each a "shot" in the player's sense: show_wp6_reset(), the
 * LOOK / ACTR / CRWD / TRAK / FLIP sections, a beat script, timed show ops,
 * then per frame beat_tick() -> show_wp6_ext / show_wp6_sop -> show_wp6_tick()
 * -> crowds (back), actors, crowds (front) -> matte. Until WP2's shot loop
 * lands this is the loop; the calls are the ones show_ext.c / the player make.
 *
 * It prints what the acceptance tests read on the TTY ("wp6 ..." lines) and
 * draws model-sheet pages (AT6.1) the host composes into sheets.
 */
#include <stdio.h>
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/beat.h"
#include "show/show_game.h"
#include "show/show_portrait.h"
#include "show/show_actor.h"
#include "wp6_data.h"

uint32_t vram_uploads(void);                  /* ps1/src/vram.c: bytes uploaded this frame */

static int s_scene = -1, s_start = 1, s_done;
static BeatScript s_script;

static uint32_t crc32(const uint8_t *p, int n)
{
    uint32_t c = 0xFFFFFFFFu;
    int i, k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static uint32_t img_crc(const DoomoImage *im) { return im ? crc32(im->pix, im->w * im->h) : 0; }

/* ---- backdrops ------------------------------------------------------------------ */
static void backdrop(int kind)
{
    int i;
    plat_clear(PAL_BLACK);
    if (kind == 1) {                                   /* model sheet: flat mid grey */
        plat_rect(0, 0, 320, 240, PAL_RAMP(RAMP_STONE, 5));
        return;
    }
    for (i = 0; i < 6; i++)                            /* a palette-band sky       */
        plat_rect(0, SHOW_CANVAS_Y + i * 22, 320, 22, PAL_RAMP(RAMP_SKY, 7 - i));
    plat_rect(0, SHOW_CANVAS_Y + 132, 320, 48, PAL_RAMP(RAMP_GRASS, 4));
    plat_rect(0, SHOW_CANVAS_Y + 132, 320, 2, PAL_RAMP(RAMP_GRASS, 6));
}

static void matte(const char *name, int t)
{
    char buf[48];
    plat_rect(0, 0, 320, SHOW_CANVAS_Y, PAL_BLACK);
    plat_rect(0, SHOW_CANVAS_Y + SHOW_CANVAS_H, 320, 240 - SHOW_CANVAS_Y - SHOW_CANVAS_H, PAL_BLACK);
    snprintf(buf, sizeof buf, "%s +%03d", name, t);
    plat_text(4, 226, buf, PAL_RAMP(RAMP_STONE, 4));
}

/* ---- model sheets (AT6.1) --------------------------------------------------------
 * pages 0-4: busts, 11 expressions x 8 visemes, then the 11 with the eyes shut;
 * page 5: figures, 11 expressions + one shut; page 6: chibi 4 dirs x 4 walk
 * frames, mouth open per dir, and the fighter's 5 views. Every cell is printed
 * ("wp6 cell ...") so the host crops it from the captured page. */
#define SHEET_PAGES 7
static const uint8_t k_vis[VIS_COUNT] = {
    SHOW_NO_MOUTH, MOUTH_FLAT, MOUTH_TEETH, MOUTH_GRIN, MOUTH_OPEN, MOUTH_O, MOUTH_POUT, MOUTH_TEETH
};

static void cell(int page, int i, const char *kind, int a, int b, int x, int y, int w, int h,
                 const DoomoImage *im, int px, int print)
{
    plat_rect(x, y, w, h, PAL_RAMP(RAMP_STONE, 6));
    if (im) plat_image(x + (w - im->w * px) / 2, y + h - im->h * px, im, px, 0, 0);
    if (print)
        printf("wp6 cell page=%d i=%d kind=%s a=%d b=%d x=%d y=%d w=%d h=%d crc=%08x\n",
               page, i, kind, a, b, x, y, w, h, (unsigned)img_crc(im));
}

static void sheet_page(int slot, int page, int first, int print)
{
    const DoomoImage *im;
    int i;
    /* a page is held 3 frames: the first renders and uploads what the 56 KB
     * display list takes, the next two find the rest (the VRAM image cache
     * keys on uid), the third is captured */
    if (first) portrait_cache_reset();
    portrait_cache_clock(4, 6);                       /* a clock with the eyes open */
    if (page <= 4) {
        for (i = 0; i < 24; i++) {
            int k = page * 24 + i, expr, vis, x = 4 + (i % 6) * 52, y = 4 + (i / 6) * 59;
            if (k >= EXPR_COUNT * VIS_COUNT + EXPR_COUNT) break;
            if (k < EXPR_COUNT * VIS_COUNT) {
                expr = k / VIS_COUNT;
                vis = k % VIS_COUNT;
                show_portrait_overrides(k_vis[vis] == SHOW_NO_MOUTH ? -1 : k_vis[vis], -1);
            } else {
                expr = k - EXPR_COUNT * VIS_COUNT;
                vis = -1;
                show_portrait_overrides(-1, 1);
            }
            im = portrait_image(slot, expr);
            show_portrait_overrides(-1, -1);
            cell(page, i, "bust", expr, vis, x, y, 52, 58, im, 1, print);
        }
    } else if (page == 5) {
        for (i = 0; i <= EXPR_COUNT; i++) {
            int x = 4 + (i % 6) * 52, y = 8 + (i / 6) * 100;
            show_portrait_overrides(-1, i == EXPR_COUNT ? 1 : -1);
            im = figure_image(&doomo_town.res[slot].look, RES_ALIVE, i == EXPR_COUNT ? 0 : i);
            show_portrait_overrides(-1, -1);
            cell(page, i, "figure", i == EXPR_COUNT ? 0 : i, i == EXPR_COUNT ? 1 : 0, x, y, 52, 84, im, 1, print);
        }
    } else {
        for (i = 0; i < 20; i++) {
            int x = 4 + (i % 10) * 31, y = 4 + (i / 10) * 54;
            int dir = i < 16 ? i / 4 : i - 16, frame = i < 16 ? i % 4 : 0;
            show_portrait_overrides(i < 16 ? -1 : MOUTH_OPEN, -1);
            im = bodyspr_image(slot, dir, frame);
            show_portrait_overrides(-1, -1);
            cell(page, i, "chibi", dir, i < 16 ? frame : 9, x, y, 30, 52, im, 1, print);
        }
        for (i = 0; i < 5; i++) {
            int x = 4 + i * 62, y = 120;
            im = fighter_image(slot, i, 0, -1);
            cell(page, 20 + i, "fighter", i, 0, x, y, 60, 100, im, 2, print);
        }
    }
}

/* ---- one scene = one shot --------------------------------------------------------- */
static void scene_start(const Wp6Scene *s)
{
    show_wp6_reset(0);
    show_cast_load((const DoomoLook *)s->look, s->nlook);
    show_actor_load((const ShowActorRec *)s->actr, s->nactr);
    show_actor_tracks(s->trak, (uint32_t)s->trak_bytes);
    show_actor_flips(s->flip, (uint32_t)s->flip_bytes);
    show_crowd_load((const ShowCrowdRec *)s->crwd, s->ncrwd);
    s_script.ev = s->beat;
    s_script.n = (uint16_t)s->nbeat;
    s_script.nlabels = 1;
    beat_start(&s_script);
    printf("wp6 scene %s start=%d frames=%d looks=%d actors=%d crowds=%d\n", s->name, s_start,
           s->frames, s->nlook, s->nactr, s->ncrwd);
}

static void scene_frame(const Wp6Scene *s, int t)
{
    int i, r;
    /* 1. the beat script: host kinds to the actors */
    for (;;) {
        r = beat_tick();
        if (r != BT_EXT) break;
        show_wp6_ext(beat_ext_event());
    }
    /* 2. the shot's show ops due now (BOP_SHOT in the real player) */
    for (i = 0; i < s->nops; i++)
        if (s->ops[i].t == t) show_wp6_sop(s->ops[i].sop, s->ops[i].a, 6);
    /* 3. tracks, motion, clocks */
    show_wp6_tick(t);
    /* 4. draw */
    backdrop(s->backdrop);
    show_crowd_draw(0, SHOW_CANVAS_Y, SHOW_LAYER_BACK, 1);
    show_actor_draw(0, SHOW_CANVAS_Y, 1);
    show_crowd_draw(0, SHOW_CANVAS_Y, SHOW_LAYER_FRONT, 1);
    matte(s->name, t);
}

static void report(const Wp6Scene *s, int t)
{
    int i;
    if (s->mode == WP6_AT65) {
        if (t >= 103 && t <= 111)
            for (i = 0; i < show_actor_count(); i++) {
                const ShowActorState *st = show_actor_state(i);
                printf("wp6 at65 t=%d a=%d clk=%d uid=%08x\n", t, i, (int)st->clock, (unsigned)st->uid);
            }
        return;
    }
    if (!strncmp(s->name, "at62", 4)) {
        const ShowActorState *st = show_actor_state(0);
        printf("wp6 at62 t=%d clk=%d uid=%08x\n", t, (int)st->clock, (unsigned)st->uid);
    } else if (!strncmp(s->name, "at63", 4)) {
        int c, m;
        if (t <= 12 || t % 10 == 0)
            for (c = 0; c < 2; c++)
                for (m = 0; m < 6; m++) {
                    ShowMember mb;
                    show_crowd_member(c, m, &mb);
                    printf("wp6 at63 t=%d c=%d m=%d x=%d y=%d dir=%d fr=%d rip=%d clk=%d look=%d\n", t, c, m,
                           mb.x, mb.y, mb.dir, mb.frame, mb.ripple, (int)mb.clock, mb.look);
                }
    } else if (!strncmp(s->name, "at64", 4)) {
        const ShowCrowdStats *cs = show_crowd_stats();
        printf("wp6 at64 t=%d drawn=%u fetched=%u rerender=%u keys=%u new=%u up=%u\n", t, (unsigned)cs->drawn,
               (unsigned)cs->fetched, (unsigned)cs->rerenders, (unsigned)cs->keys, (unsigned)cs->newkeys,
               (unsigned)vram_uploads());
    } else if (s->mode == WP6_DX) {
        for (i = 0; i < show_actor_count(); i++) {
            const ShowActorState *st = show_actor_state(i);
            printf("wp6 dx %s t=%d a=%d mouth=%d blink=%d expr=%d clk=%d sx=%d uid=%08x\n", s->name, t, i,
                   st->mouth, st->blink, st->expr, (int)st->clock, st->sx, (unsigned)st->uid);
        }
    }
}

/* AT6.5's reference: each actor alone, fresh cache, its clock and mouth at t */
static int8_t s_m65[2][SHOW_MAX_ACTORS];
static void at65_solo(void)
{
    int i, t;
    for (t = 105; t <= 106; t++)
        for (i = 0; i < show_actor_count(); i++) {
            const ShowActorState *st = show_actor_state(i);
            const DoomoImage *im;
            int slot = i;                              /* gen_wp6: actor i wears LOOK slot i */
            portrait_cache_reset();
            portrait_cache_clock((uint32_t)t, 6);
            show_portrait_overrides(s_m65[t - 105][i], -1);
            im = figure_image(&doomo_town.res[slot].look, RES_ALIVE, st->expr);
            show_portrait_overrides(-1, -1);
            printf("wp6 at65 solo t=%d a=%d mouth=%d crc=%08x\n", t, i, s_m65[t - 105][i],
                   (unsigned)img_crc(im));
        }
}

void show_selftest_init(void)
{
    int i, f = 1;
    for (i = 0; i < wp6_nscenes; i++) {
        printf("wp6 plan %s start=%d frames=%d\n", wp6_scenes[i].name, f, wp6_scenes[i].frames);
        f += wp6_scenes[i].frames;
    }
    printf("wp6 plan end=%d\n", f);
}

int show_selftest_frame(uint32_t frame)
{
    const Wp6Scene *s;
    int t;
    if (s_done) {
        backdrop(1);
        return 1;
    }
    if (s_scene < 0 || (int)frame >= s_start + wp6_scenes[s_scene].frames) {
        if (s_scene >= 0) {
            if (wp6_scenes[s_scene].mode == WP6_AT65) at65_solo();
            s_start += wp6_scenes[s_scene].frames;
        }
        s_scene++;
        if (s_scene >= wp6_nscenes) {
            printf("wp6 done frame=%u\n", (unsigned)frame);
            s_done = 1;
            backdrop(1);
            return 1;
        }
        scene_start(&wp6_scenes[s_scene]);
    }
    s = &wp6_scenes[s_scene];
    t = (int)frame - s_start;
    if (s->mode == WP6_SHEET) {
        backdrop(1);
        if (t % 3 == 2) printf("wp6 sheet %s page=%d frame=%u\n", s->name, t / 3, (unsigned)frame);
        sheet_page(0, t / 3, t % 3 == 0, t % 3 == 2);
        return 1;
    }
    scene_frame(s, t);
    report(s, t);
    if (s->mode == WP6_AT65 && (t == 105 || t == 106)) {
        int i;
        for (i = 0; i < show_actor_count(); i++) {
            /* the image this frame drew, by content (the same key: a cache hit) */
            const ShowActorState *st = show_actor_state(i);
            const DoomoImage *im;
            s_m65[t - 105][i] = st->mouth;
            show_portrait_overrides(st->mouth, st->blink);
            portrait_cache_clock((uint32_t)st->clock, 6);
            im = figure_image(&doomo_town.res[i].look, RES_ALIVE, st->expr);
            show_portrait_overrides(-1, -1);
            printf("wp6 at65 drawn t=%d a=%d mouth=%d uid=%08x crc=%08x\n", t, i, st->mouth,
                   (unsigned)(im ? im->uid : 0), (unsigned)img_crc(im));
        }
    }
    return 1;
}
