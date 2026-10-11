/* timeline.c - scene order & durations. Each scene lives in scenes/NAME.c
 * and defines scene_NAME(int f, int n) and scene_NAME_audio(double, double).
 * Weak placeholders keep the trailer building while scenes are written. */
#include "timeline.h"
#include "tk.h"
#include <string.h>

#define SCENES(X) \
    X(s01_splash,     2.5) \
    X(s02_title,      4.0) \
    X(s03_hahaland,   5.0) \
    X(s04_doomogachi, 5.5) \
    X(s05_creator,    5.0) \
    X(s06_life,       5.0) \
    X(s07_vape,       5.5) \
    X(s08_invasion,   6.0) \
    X(s09_weld,       4.5) \
    X(s10_shooter,    8.5) \
    X(s11_exorcism,   7.5) \
    X(s12_census,     4.0) \
    X(s13_ending,     5.0) \
    X(s14_platforms,  4.5) \
    X(s15_logo,       5.5)

#define DECL(name, secs) \
    void scene_##name(int f, int n); \
    void scene_##name##_audio(double t0, double dur); \
    __attribute__((weak)) void scene_##name(int f, int n) { \
        tk_clear(PAL_UI_CREAM); \
        tk_caption(70, #name, PAL_WHITE, PAL_UI_PINK_D, 2); \
        tk_caption(100, "(scene coming soon)", PAL_GRAY_D, PAL_UI_CREAM, 1); \
        (void)f; (void)n; } \
    __attribute__((weak)) void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }
SCENES(DECL)

#define ROW(name, secs) { #name, secs, scene_##name, scene_##name##_audio },
const TkScene TK_SCENES[] = { SCENES(ROW) };
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
__attribute__((weak)) void tk_music(void) {}
