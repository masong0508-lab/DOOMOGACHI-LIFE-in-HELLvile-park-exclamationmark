/* test_concert_gfx.c - the concert stage draws the same pixels after its speed passes (SP6, G-K1 / P-K1;
 * owner: concert).
 *
 * The stage (concert_gfx.c) is drawn over a matrix of views: every genre (so every LED wall show), every stage
 * theme, the three acts and the four choir layouts, lights from dark to full, the curtain, beat / kick / snare /
 * onset flashes, pyro, confetti, cheers, encores, every song section, anonymous and real crowds, at 240x160 and
 * 320x240, each at three scene clocks. Every frame's FNV-1a is checked against core/tests/golden/
 * concert_frames.txt, which was recorded on the tree BEFORE the speed passes (7729ada) with
 * `tests --update-golden concert_stage_frames_match_the_golden`: a pass means every frame is still pixel for
 * pixel what it was. A changed line names the frame (resolution, genre, variant, tick). */
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "../src/concert_int.h"

static uint32_t fb_hash(int w, int h)
{
    uint32_t hsh = 2166136261u;
    int i, n = w * h;
    for (i = 0; i < n; i++) hsh = (hsh ^ nat_fb[i]) * 16777619u;
    return hsh;
}

static uint32_t mix(uint32_t a, uint32_t b) { return doomo_hash32(a * 2654435761u ^ (b + 0x5157u)); }

/* view k of genre g: a spread of everything the stage looks at */
static void make_view(StageView *v, ConcertCast *cast, int g, int k, int res)
{
    static const int lights[6] = { 16, 0, 5, 12, 16, 9 };
    static const int curtains[6] = { 0, 256, 0, 90, 0, 0 };
    uint32_t r = mix((uint32_t)(g * 31 + k), (uint32_t)res);
    int i, mode = (g + k) % 3;
    memset(v, 0, sizeof(*v));
    concert_cast_make(cast, (g * 37 + k * 11 + 3) % 120, mode, 2 + (int)(r % 24u), (int)((r >> 5) % 4u));
    v->cast = cast;
    v->genre = g;
    v->stage = (g + k) % STAGE_COUNT;
    v->lights = lights[k % 6];
    v->curtain = curtains[k % 6];
    v->beat = (int)((r >> 8) % 8u);
    v->kick = (int)((r >> 11) % 7u);
    v->snare = (int)((r >> 14) % 7u);
    v->onset = (int)((r >> 17) % 7u);
    v->hype = (int)((r >> 20) % 101u);
    v->pyro = (k == 2 || k == 5) ? 3 + (int)((r >> 3) % 25u) : 0;
    v->confetti = (k & 1) ? 3 : 0;
    v->cheer = (k % 3 == 1) ? 6 : 0;
    v->encore = k == 4;
    v->sec = (int)((r >> 24) % (SEC_END + 1));
    v->singing = mix(r, 7u) & ((1u << cast->n) - 1u);
    for (i = 0; i < CROWD_MAX; i++) {
        uint32_t q = mix(r, (uint32_t)i);
        v->crowd[i] = (int16_t)((q & 7u) == 0 ? -1 : (int)(q % 200u));
    }
    v->ncrowd = res ? 18 : 15;
}

static void draw_frame(StageView *v, const StageLayout *L, uint32_t t, int g, int k)
{
    static const char *const lines[4] = { "Ate my heart", "~ \x8C ~", "Sing it Grimbert, sing it lo", "Hey" };
    v->t = t;
    portrait_cache_clock(t, 0);
    concert_draw_stage(v, L);
    concert_draw_performers(v, L);
    concert_draw_fx(v, L);
    concert_draw_crowd(v, L);
    concert_draw_curtain(v, L);
    concert_draw_lyric(L, lines[(g + k) & 3], (int)(t % 12u), (int)(t % 9u) - 1, concert_genres[g].edgy, t);
}

TEST(concert_stage_frames_match_the_golden)
{
    static ConcertCast cast;
    char path[512], line[160];
    FILE *f;
    int res, g, k, n = 0, bad = 0;
    uint32_t t;
    town_new(TOWN_FREEPLAY, 777, TOWN_POP_PREMADE);
    sim_init();
    snprintf(path, sizeof(path), "%s/concert_frames.txt", test_golden_dir);
    f = fopen(path, test_update_golden ? "w" : "r");
    CHECK_MSG(f != NULL, "cannot open %s", path);
    if (test_update_golden)
        fprintf(f, "# concert stage frames (test_concert_gfx.c): res genre view tick fnv1a\n");
    for (res = 0; res < 2 && !bad; res++) {
        int w = res ? 320 : 240, h = res ? 240 : 160;
        nat_w = w; nat_h = h;
        portrait_cache_reset();
        for (g = 0; g < MUSIC_COUNT && !bad; g++)
            for (k = 0; k < 6 && !bad; k++) {
                StageView v;
                StageLayout L;
                make_view(&v, &cast, g, k, res);
                concert_layout(&L);
                for (t = 0; t < 3 && !bad; t++) {
                    uint32_t tick = 100u + (uint32_t)(g * 97 + k * 41) + t * 7u, hv;
                    memset(nat_fb, 0, (size_t)(w * h));
                    draw_frame(&v, &L, tick, g, k);
                    hv = fb_hash(w, h);
                    if (test_update_golden) {
                        fprintf(f, "%d %d %d %u %08x\n", res, g, k, (unsigned)tick, (unsigned)hv);
                    } else {
                        int rr, gg, kk;
                        unsigned tt, want;
                        do {
                            if (!fgets(line, sizeof(line), f)) { line[0] = 0; break; }
                        } while (line[0] == '#');
                        if (sscanf(line, "%d %d %d %u %x", &rr, &gg, &kk, &tt, &want) != 5 || rr != res ||
                            gg != g || kk != k || tt != tick) {
                            test_fail(__FILE__, __LINE__, "golden file out of step with the test");
                            bad = 1;
                        } else if (want != hv) {
                            char m[200];
                            snprintf(m, sizeof(m), "frame res %d genre %d view %d tick %u: %08x, golden %08x",
                                     res, g, k, (unsigned)tick, (unsigned)hv, want);
                            test_fail(__FILE__, __LINE__, m);
                            bad = 1;
                        }
                    }
                    n++;
                }
            }
    }
    fclose(f);
    nat_w = 240; nat_h = 160;
    portrait_cache_reset();
    portrait_cache_clock(0, 0);
    CHECK(n == 2 * MUSIC_COUNT * 6 * 3 || bad);
}

/* the LED wall's wave stepper (concert_gfx.c ws_*) gives ui_wave's value at every step: periods from 1 to 400,
 * steps smaller and larger than the period, amplitudes the shows use and beyond, long runs from many starts */
int concert_wave_step_check(int arg, int period, int step, int n, int amp);
TEST(concert_wave_step_equals_ui_wave)
{
    static const int periods[] = { 1, 7, 40, 45, 60, 64, 90, 120, 260, 400 };
    static const int steps[] = { 0, 1, 3, 6, 12, 23, 40, 41, 397, 400, 1000 };
    unsigned pi, si;
    int amp, start;
    for (pi = 0; pi < sizeof(periods) / sizeof(periods[0]); pi++)
        for (si = 0; si < sizeof(steps) / sizeof(steps[0]); si++)
            for (amp = 1; amp <= 12; amp += (amp < 4 ? 1 : 4))
                for (start = 0; start < 5000; start += 977) {
                    int bad = concert_wave_step_check(start, periods[pi], steps[si], 600, amp);
                    CHECK_MSG(bad < 0, "period %d step %d amp %d from %d: step %d differs", periods[pi], steps[si],
                              amp, start, bad);
                }
    CHECK(concert_wave_step_check(1234567, 120, 12, 2000, 3) < 0);
}

/* the stage's division-free helpers: x % 3 and x % 6 by folding (every 32-bit value of many bit-pattern classes and a
 * long pseudo-random run), and the quotient stepper against num / den */
uint32_t concert_mod3(uint32_t x);
uint32_t concert_mod6(uint32_t x);
int concert_div_step_check(int num, int den, int step, int n);
TEST(concert_mod3_mod6_and_div_step_equal_division)
{
    uint32_t x, r = 12345u;
    int b, i, den, step;
    for (x = 0; x < 200000u; x++) {
        CHECK_EQ_U(concert_mod3(x), x % 3u);
        CHECK_EQ_U(concert_mod6(x), x % 6u);
    }
    for (b = 0; b < 32; b++)
        for (i = -300; i <= 300; i++) {
            x = (1u << b) + (uint32_t)i;
            CHECK_EQ_U(concert_mod3(x), x % 3u);
            CHECK_EQ_U(concert_mod6(x), x % 6u);
            x = ~x;
            CHECK_EQ_U(concert_mod3(x), x % 3u);
            CHECK_EQ_U(concert_mod6(x), x % 6u);
        }
    for (i = 0; i < 2000000; i++) {
        r = r * 1664525u + 1013904223u;
        CHECK_EQ_U(concert_mod3(r), r % 3u);
        CHECK_EQ_U(concert_mod6(r), r % 6u);
    }
    for (den = 1; den <= 40; den++)
        for (step = 0; step <= 700; step += 7)
            CHECK_MSG(concert_div_step_check(den * 3 + step, den, step, 300) < 0, "den %d step %d", den, step);
    CHECK(concert_div_step_check(240, 18, 480, 9) < 0);
    CHECK(concert_div_step_check(320, 14, 640, 7) < 0);
}

/* concert_confetti (also the dreams' and the results') against its pre-SP6 body, verbatim but for the name: every
 * piece's rect over many boxes, clocks (incl. the far ones that take the old path) and piece counts */
static uint32_t old_hash2(uint32_t a, uint32_t b) { return doomo_hash32(a * 2654435761u ^ (b + 0x9E37u)); }
static void old_confetti(int x0, int y0, int w, int h, uint32_t t, int n, int fall)
{
    static const uint8_t cols[6] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_SKY, PAINT_MINT,
                                     PAINT_LAVENDER, PAINT_TANGERINE };
    int i;
    if (w <= 0 || h <= 0) return;
    for (i = 0; i < n; i++) {
        uint32_t r = old_hash2((uint32_t)i, 77u);
        int x = x0 + (int)(r % (uint32_t)w) + ui_wave((int)t + i * 7, 40, 3);
        int y = y0 + (int)(((r >> 8) % (uint32_t)h + (uint32_t)(fall ? t * (1 + (r >> 20) % 3) : 0)) %
                           (uint32_t)h);
        int ph = (int)((t / 3 + r) & 3);
        uint8_t c = PAL_PAINT(cols[(r >> 4) % 6], 2 - (ph & 1));
        if (ph == 0) plat_rect(x, y, 2, 2, c);
        else if (ph == 1) plat_rect(x, y, 3, 1, c);
        else if (ph == 2) plat_rect(x, y, 1, 2, c);
        else plat_rect(x, y, 2, 1, c);
    }
}

TEST(concert_confetti_equals_the_old_confetti)
{
    static uint8_t a[240 * 160];
    static const uint32_t far[] = { 0x1FFFFFF0u, 0x20000000u, 0x7FFFFF00u, 0x80000005u, 0xFFFFFFF0u };
    int k, fall;
    nat_w = 240; nat_h = 160;
    for (k = 0; k < 400; k++)
        for (fall = 0; fall < 2; fall++) {
            int x0 = (k * 7) % 40 - 8, y0 = (k * 3) % 30 - 5, w = 1 + (k * 37) % 240, h = 1 + (k * 53) % 160;
            int n = (k % 7) * 10;
            uint32_t t = k < 395 ? (uint32_t)(k * 131) : far[k - 395];
            memset(nat_fb, 0, sizeof(a));
            concert_confetti(x0, y0, w, h, t, n, fall);
            memcpy(a, nat_fb, sizeof(a));
            memset(nat_fb, 0, sizeof(a));
            old_confetti(x0, y0, w, h, t, n, fall);
            CHECK_MSG(memcmp(a, nat_fb, sizeof(a)) == 0, "box %d,%d %dx%d t %u n %d fall %d", x0, y0, w, h,
                      (unsigned)t, n, fall);
        }
}
