/* fair_common.h - shared bits for the Giggle crawl scenes SC17-SC20 */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "cast.h"
#include "show.h"
#include "fx.h"
#include <math.h>
#include <string.h>
#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
#define SHOT(name, ...) \
    void scene_##name(int f, int n) { float ta = (float)tk_scene_start(#name) + f / 30.0f; (void)n; (void)ta; __VA_ARGS__ } \
    void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }
/* HaHaland Day from the fair's speakers, at a level; ducked between d0 and d1 */
static void hahaland_bed(double t0, double t1, float vol, double d0, double d1) {
    static const int M[16] = { 72, 76, 79, 76, 77, 81, 79, 0, 72, 76, 79, 84, 83, 79, 77, 76 };
    for (int i = 0; ; i++) {
        double t = t0 + i * 0.214; if (t >= t1) break;
        if (t >= d0 && t < d1) continue;
        if (M[i % 16]) tk_note(t, 0.15, M[i % 16], TKV_SQ25, vol);
        if (i % 2 == 0) tk_note(t, 0.3, 48 + (i / 8 % 4 == 1 ? 5 : i / 8 % 4 == 2 ? 7 : 0), TKV_TRI, vol * 0.8f);
    }
}
static void crowd_bg(float ta, float x0, float x1, uint32_t seed) {
    CrowdOpts o = { 0 };
    o.n = 8; o.x0 = x0; o.x1 = x1; o.y0 = -6; o.y1 = 2; o.t = ta * 30; o.seed = seed; o.freeze_at = -1; o.scale = 0.8f;
    crowd_draw(&o);
}
