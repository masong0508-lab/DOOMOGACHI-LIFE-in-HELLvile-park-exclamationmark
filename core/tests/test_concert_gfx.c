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
