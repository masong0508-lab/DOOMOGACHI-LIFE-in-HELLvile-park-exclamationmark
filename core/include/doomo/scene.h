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

/* (ps1 overlays, additive) A platform may keep a scene's code out of RAM
 * until it runs: the PS1 game disc loads it from the disc into one shared
 * slot (ps1/src/overlay.c, docs/PS1.md section 3). scene.c brackets every
 * call into a scene (enter / leave / update / draw) with
 *     t = plat_scene_code(s);  s->fn();  plat_scene_code_end(t);
 * so the code is there while it runs and the caller's code is back when a
 * scene was switched from inside another scene. The defaults (weak, in
 * scene.c) do nothing. Never call a scene's functions directly. */
int  plat_scene_code(const DoomoScene *s);
void plat_scene_code_end(int token);
/* --- H4 (additive): a scene's code may also come in PARTS that share one
 * address range (ps1/tools/ovl_plan.py PARTS): the code of one mode of a
 * scene. Before the scene's own code calls into a part it asks for it;
 * after that, only while that mode runs, may it call the part's functions.
 * The default (weak, scene.c) does nothing. --- */
enum { PLAT_PART_NONE, PLAT_PART_HOME_VISIT, PLAT_PART_HOME_DEFEND, PLAT_PART_HOME_MART,
       PLAT_PART_HOME_ROOMTEST /* the PS1 renderer's harness layouts */ };
void plat_code_part(int part);
/* --- end H4 --- */

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
/* (integration) The block itself (a fixed address), without claiming it:
 * for accessors that must stay `const` (mg_state()). Claim it first. */
void *scene_scratch_block(void) __attribute__((const));
/* (integration, memory budget) Who uses the scratch (keep this list
 * current; tags are SAVE_EXT_TAG()s, ARCHITECTURE.md has the sizes):
 *   WELD  weld_gfx.c        first-person weapon sprites: combat, arena,
 *                           workshop, the library's weapon wing
 *   OWLD  overworld*.c      story overworld citizens, props, bubbles
 *   CREA  creator*.c        the creator's editor state
 *   PETS  pet_gfx.c, pet_scene.c   SCENE_PET's LCD sprites
 *   MGAM  mg.c, mg_fx.c     the running minigame's state + effects
 *   WGAM  weld_game.c       the welding minigame's trails
 *   FP..  fp_island.c, fp_roster.c, fp_unlocks.c   free play lists
 *   FCOK  fishcook.c, fish_scene.c, cook_scene.c   fishing / cooking state
 *                           + their particle pool (fishcook_int.h)
 *   GGFX  grumbleguts_gfx.c  the story cutscenes' cached Grumbleguts layers
 *                           (story_feast.c) and the lantern string
 *   HOME  home*.c           (S0 list, SPEC sec.4) the house editor state, the
 *                           visit room, the defence; its tail is the PS1
 *                           vertex cache (DoomoRoomView.plat_cache)
 *   OWLD ext                (S0 list, SPEC R17) the 512 B ow_scratch_ext()
 *                           area of OwScratch: scripted overworld scenes (the
 *                           Feast guests, the Say It walk). A cache: rebuilt
 *                           from STRY whenever another owner took the block
 * A scene that keeps its own state here must not call anything that
 * claims it for another owner (e.g. weld_view_image()). */

/* (integration, memory budget) ONE per-frame billboard list for whichever
 * scene draws a 3D view or the overworld this frame (combat / arena, the
 * story overworld, the credits parade, ...). A scene fills it inside its
 * draw() and hands it straight to plat_view3d() / plat_overworld(), which
 * read it during the call only, so the scenes can share it instead of
 * keeping 2.4 KB each. Never keep billboards in it across frames. */
struct DoomoBillboard;
#define DOOMO_FRAME_BB_MAX 120
extern struct DoomoBillboard *const doomo_frame_bb;

#endif
