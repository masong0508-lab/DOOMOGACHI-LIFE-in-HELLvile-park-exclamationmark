/* show.h - shared scene toolkit: takes placed by line ID, lip-sync from take
 * amplitude, speech bubbles, and the per-scene timeline macro. */
#ifndef SHOW_H
#define SHOW_H
#include <stdint.h>

/* Audio pass: register that line ID starts at absolute time t (seconds).
 * Writes "id t" to $CUES_OUT; assemble_scene.sh picks the best take
 * (rec > lib > tts), runs the dialogue chain and places it there. */
void line_at(const char *id, double t);

/* Draw pass: mouth opening 0..1 for line ID, `fr` frames after its cue.
 * Reads $ENV_DIR/<id>.env (one value per frame, written by
 * assemble_scene.sh from the chained take). Without it: a babble flap for
 * `fallback` frames. */
float lip(const char *id, int fr, int fallback);

/* Speech bubble in screen space: text typed on (`chars`), tail pointing at
 * the speaker's mouth (sx, sy). Keeps the speaker visible, unlike the box. */
extern int bubble_side;   /* 0 auto (beside), 1 right, -1 left, 2 below the speaker */
void bubble(const char *text, int chars, int sx, int sy);
#endif
