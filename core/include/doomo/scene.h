/* scene.h - tiny scene stack. Scenes are static const structs. */
#ifndef DOOMO_SCENE_H
#define DOOMO_SCENE_H

#include "types.h"

typedef struct DoomoScene {
    const char *name;
    void (*enter)(int arg);   /* may be NULL */
    void (*leave)(void);      /* may be NULL */
    void (*update)(void);     /* 30 Hz logic */
    void (*draw)(void);       /* issue plat_* drawing */
    uint8_t overlay;          /* 1 = draw the scene below first (modals) */
} DoomoScene;

#define SCENE_STACK_MAX 8

void scene_set(const DoomoScene *s, int arg);   /* clear stack, enter s   */
void scene_push(const DoomoScene *s, int arg);
void scene_pop(void);                           /* resumes the one below  */
const DoomoScene *scene_top(void);
int  scene_depth(void);
void scene_update(void);                        /* called by doomo_frame  */
void scene_draw(void);
/* Ticks since the top scene was entered. */
uint32_t scene_ticks(void);

/* ---- shared scene scratch (perf pass, additive) --------------------------
 * On the GBA every byte of .bss is EWRAM, and most scenes' big buffers
 * (rendered images, editor state, per-scene caches) are only needed while
 * that scene runs. Instead of a static buffer each, a scene can take this
 * ONE shared block: scene_scratch(owner, size) returns it (NULL if size >
 * DOOMO_SCENE_SCRATCH), and from then on it belongs to `owner` (any
 * unique tag, e.g. SAVE_EXT_TAG('W','E','L','D')) until another owner
 * claims it - which may happen whenever another scene runs (a pushed
 * scene, a scene_set). So: claim it in enter() and initialise it; check
 * scene_scratch_mine(owner) wherever the scene may resume (after a pushed
 * scene returns) and re-claim + rebuild when it is gone. Ideal for caches
 * that can be rebuilt; modals that draw over a scene must not claim it.
 * Unused, it costs nothing (--gc-sections drops the block). */
#define DOOMO_SCENE_SCRATCH (12u * 1024u)
void *scene_scratch(uint32_t owner, uint32_t size);
int   scene_scratch_mine(uint32_t owner);

#endif
