/* backdrop.h - full-screen backdrops a platform may keep, and the retained stage over them
 * (owner: SP7; docs/EXORCISM_SPEED.md). Additive.
 *
 * A backdrop family paints the whole screen from a small key (the exorcism's church: its
 * dread level and its slow animation steps). Every key a scene can ask for is listed, so a
 * platform can bake them all ahead of time: the GBA build runs the native runner with
 * --bake-backdrops and links the pictures into the ROM (gba/src/stage.c), and plat_stage_begin
 * (plat_ext.h) then answers from the ROM. Platforms without it draw the backdrop as before.
 *
 * The stage (stage_*) keeps, per frame buffer, the boxes and keys of the layers a scene drew
 * over the backdrop, and on the next frame drawn into that buffer puts the backdrop back and
 * redraws only the layers that changed or that lie on a box that changed. The result is the
 * same picture as drawing everything (core/tests/test_stage.c checks it frame by frame).
 */
#ifndef DOOMO_BACKDROP_H
#define DOOMO_BACKDROP_H

#include "types.h"

enum { DOOMO_BACKDROP_EXO_CHURCH = 1 };

typedef struct DoomoBackdrop {
    int id;                                    /* DOOMO_BACKDROP_*                     */
    void (*draw)(uint32_t key);                /* paints every pixel of the screen      */
    int (*keys)(uint32_t *out, int cap);       /* every key; returns the count          */
} DoomoBackdrop;

/* The families, 0..count-1 (the baker walks them). */
int backdrop_family_count(void);
const DoomoBackdrop *backdrop_family(int i);

/* ---- the retained stage ------------------------------------------------------------
 * Per frame, in the scene's draw:
 *     if (stage_begin(id, key, draw)) {          (1: the platform keeps the backdrop)
 *         stage_layer(0, x, y, w, h, key0); ...   every layer, in drawing order
 *         stage_resolve();                         puts the backdrop back where needed
 *     } else draw(key);                            (0: draw the backdrop yourself)
 *     for each layer i: if (stage_draw(i)) draw it;   (always 1 when stage_begin gave 0)
 *     stage_end();                                 overlays (dialogue, toasts) follow
 * A layer's box must hold every pixel it draws (shading included); its key must change
 * whenever its pixels would. An empty box (w or h <= 0) is a layer that draws nothing. */
#define STAGE_MAX_LAYERS 24
int  stage_begin(int id, uint32_t key, void (*draw)(uint32_t key));
void stage_layer(int i, int x, int y, int w, int h, uint32_t key);
void stage_resolve(void);
int  stage_draw(int i);
void stage_end(void);
/* The stage's memory, stage_mem_bytes() long (~1.1 KB): the caller binds a block it owns (the
 * exorcism binds its scene scratch, and binds it again when another scene took the scratch);
 * binding forgets every record. Unbound (NULL), stage_begin always returns 0. */
uint32_t stage_mem_bytes(void);
void stage_bind(void *mem);
/* Forget every record (a scene enter: the next two frames draw in full). */
void stage_reset(void);
/* Statistics of the last resolved frame (tests, harness logs): layers drawn, boxes put back,
 * and whether the frame was drawn in full. */
typedef struct StageStats { uint8_t drawn, restored, full, retained; } StageStats;
void stage_stats(StageStats *s);

#endif
