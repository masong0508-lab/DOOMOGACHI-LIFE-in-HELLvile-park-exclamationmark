/* timeline.h - the trailer's scene list (fixed order and durations). */
#ifndef TIMELINE_H
#define TIMELINE_H

typedef struct TkScene {
    const char *name;
    double seconds;
    void (*draw)(int f, int n);
    void (*audio)(double t0, double dur);
} TkScene;

extern const TkScene TK_SCENES[];
extern const int TK_SCENE_COUNT;
/* Start time (seconds) of a scene by name, -1 if unknown. */
double tk_scene_start(const char *name);
double tk_total_seconds(void);
/* Whole-trailer soundtrack (music.c). Scenes add their own sfx. */
void tk_music(void);

#endif
