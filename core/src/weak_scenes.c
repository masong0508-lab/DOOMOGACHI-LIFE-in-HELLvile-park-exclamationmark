/* weak_scenes.c - weak placeholders for the scenes K0 declares in scenes.h
 * (owner: K0 contract, LIFESIM_PLAN R5): SCENE_FAMILY (F2), SCENE_PLAYAS
 * (PA5), SCENE_ENDING_CARD (E1), SCENE_MSLOT (E2), SCENE_LANG_PICK and
 * SCENE_SECURITY_CODE (U5).
 * Each owner defines the strong scene in its own file; nobody edits
 * stubs.c or this file to do it.
 *
 * Until then a push of one of them is harmless: enter() logs
 * "STUB: SCENE_<name>" (so a runner script with --assert-no-stubs notices),
 * and the first update() pops it again, so the scene below resumes the
 * next tick with no result (the caller's "nothing happened" path). They are
 * overlays with no draw, so the scene below keeps drawing for that tick.
 * If one is ever the only scene (scene_set), it hands over to SCENE_TITLE.
 * No .bss, so they cost no RAM on any port.
 */
#include "doomo/doomo.h"

#define WEAK __attribute__((weak))

static void k0_scene_enter(int arg)
{
    const DoomoScene *s = scene_top();
    char buf[48];
    (void)arg;
    doomo_snprintf(buf, sizeof(buf), "STUB: SCENE_%s", s && s->name ? s->name : "?");
    plat_log(buf);
}

static void k0_scene_update(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

#define K0_SCENE(var, nm) \
    WEAK const DoomoScene var = { nm, k0_scene_enter, NULL, k0_scene_update, NULL, 1 }

K0_SCENE(SCENE_FAMILY, "FAMILY");
K0_SCENE(SCENE_PLAYAS, "PLAYAS");
K0_SCENE(SCENE_ENDING_CARD, "ENDING_CARD");
K0_SCENE(SCENE_MSLOT, "MSLOT");
K0_SCENE(SCENE_LANG_PICK, "LANG_PICK");
K0_SCENE(SCENE_SECURITY_CODE, "SECURITY_CODE");

/* 1 if `s` is still one of the placeholders above (tests; stubs.c's
 * scene_is_stub() covers the older placeholders). */
int scene_is_reserved_stub(const DoomoScene *s)
{
    return s && s->enter == k0_scene_enter && s->update == k0_scene_update;
}
