/* music.c - the score for the pilot proof: the Mingle (the creator's melody,
 * pad1.mid: 150 bpm, 3/4, C minor) as a music box and as a jaunty waltz,
 * on the teaser's chiptune synth. The hum in shot 2 is the creator's own
 * recorded take, mixed in by assemble.sh. The score drops out for the
 * comic beats (the hees, the rolls, the red N, the glance, "Stay.", the egg).
 */
#include "tk.h"
#include "timeline.h"

#define BEAT 0.4   /* 150 bpm */
/* the Mingle, from pad1.mid: midi note, length in beats */
static const int MEL[][2] = {
    { 60, 4 }, { 58, 4 }, { 55, 4 }, { 60, 4 }, { 58, 4 }, { 55, 4 },
    { 58, 4 }, { 56, 4 }, { 53, 4 }, { 58, 4 }, { 56, 4 }, { 53, 4 },
    { 56, 4 }, { 55, 4 }, { 51, 4 }, { 48, 12 }, { 60, 4 }, { 58, 4 }, { 56, 4 }, { 51, 9 } };
#define NMEL ((int)(sizeof MEL / sizeof MEL[0]))
static const int BASS[8] = { 36, 36, 41, 41, 44, 36, 44, 36 };   /* one root a bar */

static void music_box(double t, int midi, double len, float v) {
    tk_note(t, len < 0.5 ? len : 0.5, midi, TKV_SINE, v);
    tk_note(t, 0.1, midi + 12, TKV_TRI, v * 0.35f);
}

/* play the Mingle from t0 until t1 (cut off mid-phrase), style 0 = music box, 1 = waltz */
static void mingle(double t0, double t1, int style, int transpose, float vol) {
    double t = t0;
    for (int rep = 0; rep < 4 && t < t1; rep++) {
        int bar = 0; double tb = t;
        for (int i = 0; i < NMEL && t < t1; i++) {
            double len = MEL[i][1] / 4.0 * BEAT;
            int m = MEL[i][0] + transpose;
            if (style == 0) music_box(t, m + 12, len * 0.95, vol);
            else {
                tk_note(t, len * 0.85, m + 12, TKV_SQ25, vol * 0.55f);
                tk_note(t, len * 0.85, m, TKV_TRI, vol * 0.25f);
            }
            t += len;
        }
        if (style == 1)
            for (double b = tb; b < t && b < t1; b += 3 * BEAT, bar++) {
                int root = BASS[bar % 8] + transpose;
                tk_note(b, BEAT * 0.9, root, TKV_TRI, vol * 0.7f);
                tk_kick(b, vol * 0.35f);
                for (int k = 1; k < 3; k++) {
                    if (b + k * BEAT >= t1) break;
                    tk_note(b + k * BEAT, 0.08, root + 24 + 3, TKV_SQ50, vol * 0.18f);
                    tk_note(b + k * BEAT, 0.08, root + 24 + 7, TKV_SQ50, vol * 0.18f);
                    tk_hat(b + k * BEAT, vol * 0.3f);
                }
            }
        t += 3 * BEAT;   /* breath between repeats */
    }
}

void tk_music(void) {
    /* after "Perfect.": the music box takes the Mingle up again, alone... */
    double a = tk_scene_start("p06_perfect") + 112 / 30.0;
    double stop = tk_scene_start("p07_glance") + 84 / 30.0;   /* ...and stops, mid-phrase, at the Wall */
    mingle(a, stop, 0, 0, 0.45f);
    /* he climbs down: the jaunty waltz, through the plaza */
    double w0 = tk_scene_start("p10_climb") + 44 / 30.0;
    double w1 = tk_scene_start("p12_window");
    mingle(w0, w1, 1, 0, 0.5f);
    /* the bakery: back to the music box, softer, until the egg */
    mingle(w1 + 0.2, tk_scene_start("p13_egg") - 0.1, 0, 0, 0.32f);
}
