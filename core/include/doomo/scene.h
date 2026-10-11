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

#endif
