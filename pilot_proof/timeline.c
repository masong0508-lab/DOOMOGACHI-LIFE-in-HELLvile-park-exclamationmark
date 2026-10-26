/* timeline.c - shot order and durations for the pilot proof (single source
 * of truth for timing; music.c and the shots read their start times here).
 * Reuses the teaser's TkScene table format so ../trailer/main.c drives it.
 * Shots breathe (4-8 s); the energy lives inside them. */
#include "timeline.h"
#include "tk.h"
#include <string.h>

#define SHOTS(X) \
    X(p01_storybook, 3.5) \
    X(p02_banner,    8.0) \
    X(p03_heehee,    6.5) \
    X(p04_roll,      7.0) \
    X(p05_red,       5.0) \
    X(p06_perfect,   6.0) \
    X(p07_glance,    5.0) \
    X(p08_pluck,     0.1334) \
    X(p09_stay,      6.0) \
    X(p10_climb,     5.0) \
    X(p11_plaza,     3.8) \
    X(p12_window,    6.5) \
    X(p13_egg,       8.0)

#define DECL(name, secs) \
    void scene_##name(int f, int n); \
    void scene_##name##_audio(double t0, double dur); \
    __attribute__((weak)) void scene_##name(int f, int n) { \
        tk_clear(PAL_UI_CREAM); \
        tk_caption(70, #name, PAL_WHITE, PAL_UI_PINK_D, 2); \
        (void)f; (void)n; } \
    __attribute__((weak)) void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }
SHOTS(DECL)

#define ROW(name, secs) { #name, secs, scene_##name, scene_##name##_audio },
const TkScene TK_SCENES[] = { SHOTS(ROW) };
const int TK_SCENE_COUNT = (int)(sizeof TK_SCENES / sizeof TK_SCENES[0]);

double tk_scene_start(const char *name) {
    double t = 0;
    for (int i = 0; i < TK_SCENE_COUNT; i++) {
        if (!strcmp(TK_SCENES[i].name, name)) return t;
        t += TK_SCENES[i].seconds;
    }
    return -1;
}
double tk_total_seconds(void) {
    double t = 0;
    for (int i = 0; i < TK_SCENE_COUNT; i++) t += TK_SCENES[i].seconds;
    return t;
}
