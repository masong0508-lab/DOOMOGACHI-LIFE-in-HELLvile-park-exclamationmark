/* scene_tl.h - one timeline per scene: #define SHOTS(X) X(name, seconds) ...
 * then #include "scene_tl.h". Provides TK_SCENES and the start-time helpers. */
#include "timeline.h"
#include "tk.h"
#include <string.h>
#define DECL(name, secs) void scene_##name(int f, int n); void scene_##name##_audio(double t0, double dur);
SHOTS(DECL)
#define ROW(name, secs) { #name, secs, scene_##name, scene_##name##_audio },
const TkScene TK_SCENES[] = { SHOTS(ROW) };
const int TK_SCENE_COUNT = (int)(sizeof TK_SCENES / sizeof TK_SCENES[0]);
double tk_scene_start(const char *name) {
    double t = 0;
    for (int i = 0; i < TK_SCENE_COUNT; i++) { if (!strcmp(TK_SCENES[i].name, name)) return t; t += TK_SCENES[i].seconds; }
    return -1;
}
double tk_total_seconds(void) {
    double t = 0;
    for (int i = 0; i < TK_SCENE_COUNT; i++) t += TK_SCENES[i].seconds;
    return t;
}
