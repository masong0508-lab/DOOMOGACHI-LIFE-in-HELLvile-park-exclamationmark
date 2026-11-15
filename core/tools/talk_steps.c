/* talk_steps.c - TKP0: the cost of one drama-manager step on the host (owner: TK;
 * docs/design/talk/TKP_PLAN.md). Plays random sessions in rooms of 2 and 3 residents over the real
 * TALK_en.BIN and times talk_dm_say() and every talk_dm_step() with clock_gettime. The PS1 plan slices
 * generation by step (talk_scene.c gen_run: one talk_dm_step a tick), so the largest step decides
 * whether a frame can hold it. Prints the host distribution; the plan scales it to the R3000.
 * Build and run: sh core/tools/talk_steps.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/town.h"
#include "doomo/sim.h"
#include "doomo/rng.h"
#include "native.h"

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static int cmpd(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static void pr(const char *what, double *v, int n)
{
    if (!n) return;
    qsort(v, (size_t)n, sizeof *v, cmpd);
    printf("talk_steps: %-22s n=%-6d mean %7.1f us  p50 %7.1f  p95 %7.1f  p99 %7.1f  max %7.1f\n", what, n,
           ({ double s = 0; int i; for (i = 0; i < n; i++) s += v[i]; s / n; }), v[n / 2], v[n * 95 / 100],
           v[n * 99 / 100], v[n - 1]);
}

int main(int argc, char **argv)
{
    NatOptions o;
    static double st[200000], sy[20000];
    int nst = 0, nsy = 0, sess, turns = argc > 1 ? atoi(argv[1]) : 6, nsess = argc > 2 ? atoi(argv[2]) : 300;
    memset(&o, 0, sizeof o);
    o.w = 320; o.h = 240; o.seed = 1; o.quiet = 1; o.save_dir = "/tmp";
    nat_init(&o);
    doomo_init();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    if (talk_db_open(lang_find("en")) != TALK_DB_OK || !talk_dm_ready()) { fprintf(stderr, "no TALK_en.BIN\n"); return 2; }
    for (sess = 0; sess < nsess; sess++) {
        TalkState s;
        DoomoRng r;
        int ids[3] = {1, 13, 42}, n = 2 + sess % 2, t;
        ids[0] = 1 + sess % 12; ids[1] = 13 + sess % 11; ids[2] = 30 + sess % 9;
        talk_dm_begin(&s, DOOMO_PLAYER_ID, ids, n, 1000u + (uint32_t)sess);
        rng_seed(&r, 77u + (uint32_t)sess);
        for (t = 0; t < turns && !talk_dm_over(&s); t++) {
            TalkUtter u;
            double t0, t1;
            memset(&u, 0, sizeof u);
            u.act[0] = (uint8_t)(1 + rng_range(&r, TA_COUNT - 1));
            u.target = (int8_t)(rng_range(&r, n + 1) - 1);
            strcpy(u.shown, "zorpy blorp");
            t0 = now_us(); talk_dm_say(&s, &u); t1 = now_us();
            if (nsy < 20000) sy[nsy++] = t1 - t0;
            for (;;) {
                int more;
                t0 = now_us(); more = talk_dm_step(&s, NULL); t1 = now_us();
                if (nst < 200000) st[nst++] = t1 - t0;
                if (!more) break;
            }
        }
        talk_dm_end(&s, &(TalkSummary){0});
    }
    pr("talk_dm_say", sy, nsy);
    pr("talk_dm_step (all)", st, nst);
    return 0;
}
