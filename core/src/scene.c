/* scene.c - the scene stack and the scene registry (owner: foundation).
 *
 * Changes take effect immediately (enter/leave run inside the call) and
 * swallow the rest of the tick's input, so a press that opens a screen
 * never also activates something on it. Overlay scenes (modals) draw the
 * scenes below them first.
 */
#include "doomo/scene.h"
#include "doomo/scenes.h"
#include "doomo/input.h"
#include "doomo/platform.h"

typedef struct {
    const DoomoScene *s;
    uint32_t ticks;
} SceneSlot;

static SceneSlot stack[SCENE_STACK_MAX];
static int depth;

/* (ps1 overlays) weak defaults: the code of every scene is always there */
__attribute__((weak)) int plat_scene_code(const DoomoScene *s) { (void)s; return 0; }
__attribute__((weak)) void plat_scene_code_end(int token) { (void)token; }

static void call_enter(const DoomoScene *s, int arg)
{
    int t;
    if (!s->enter) return;
    t = plat_scene_code(s);
    s->enter(arg);
    plat_scene_code_end(t);
}

static void call_leave(const DoomoScene *s)
{
    int t;
    if (!s || !s->leave) return;
    t = plat_scene_code(s);
    s->leave();
    plat_scene_code_end(t);
}

const DoomoScene *const doomo_all_scenes[] = {
    &SCENE_BOOT, &SCENE_CONFIRM, &SCENE_TEXT_ENTRY, &SCENE_DIALOG,
    &SCENE_UI_GALLERY,
    &SCENE_TITLE, &SCENE_NEW_ISLAND, &SCENE_ISLAND, &SCENE_APARTMENTS,
    &SCENE_ROSTER, &SCENE_RESIDENT, &SCENE_SHOP, &SCENE_GRAVEYARD,
    &SCENE_NEWS, &SCENE_OPTIONS, &SCENE_SAVELOAD,
    &SCENE_UNLOCKS, &SCENE_PICKER,
    &SCENE_CREATOR,
    &SCENE_EXORCISM, &SCENE_PET,
    &SCENE_COMBAT, &SCENE_WORKSHOP, &SCENE_WELD_GAME, &SCENE_ARENA,
    &SCENE_STORY, &SCENE_QUESTLOG, &SCENE_CUTSCENE, &SCENE_CREDITS,
    &SCENE_MG_CATCH, &SCENE_MG_WHACK, &SCENE_MG_DODGE, &SCENE_MG_GALLERY,
    &SCENE_MG_DERBY, &SCENE_MG_MEMORY, &SCENE_MG_WHEEL,
    &SCENE_CONCERT, &SCENE_DREAM,
    &SCENE_STUDIO,
    &SCENE_LIBRARY, &SCENE_LIB_BESTIARY, &SCENE_LIB_ALMANAC, &SCENE_LIB_WEAPONS,
    &SCENE_LIB_ITEMS, &SCENE_LIB_TROPHIES, &SCENE_LIB_JUKEBOX, &SCENE_LIB_GALLERY,
    &SCENE_FISHING, &SCENE_COOKING,
    &SCENE_LEVEL_SELECT, &SCENE_BREACH, &SCENE_ARENA_MODE,
    /* --- S0: story + house scenes (SPEC sec.4, C14) --- */
    &SCENE_HOME, &SCENE_HOME_MART, &SCENE_STORY_MG, &SCENE_STORY_TIMING,
    /* --- end S0 --- */
    NULL
};

static int name_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

const DoomoScene *scene_by_name(const char *name)
{
    int i;
    if (!name) return NULL;
    if (name[0] == 'S' && name[1] == 'C' && name[2] == 'E' && name[3] == 'N' &&
        name[4] == 'E' && name[5] == '_')
        name += 6;
    for (i = 0; doomo_all_scenes[i]; i++)
        if (doomo_all_scenes[i]->name && name_eq(doomo_all_scenes[i]->name, name))
            return doomo_all_scenes[i];
    return NULL;
}

static void log_scene(const char *what, const DoomoScene *s)
{
    char buf[64];
    int i = 0;
    const char *p = what;
    while (*p && i < 40) buf[i++] = *p++;
    p = s && s->name ? s->name : "?";
    while (*p && i < 63) buf[i++] = *p++;
    buf[i] = 0;
    plat_log(buf);
}

void scene_set(const DoomoScene *s, int arg)
{
    while (depth > 0) {
        const DoomoScene *top = stack[--depth].s;
        call_leave(top);
    }
    if (!s) return;
    stack[0].s = s;
    stack[0].ticks = 0;
    depth = 1;
    log_scene("scene: set ", s);
    input_eat();
    call_enter(s, arg);
}

void scene_push(const DoomoScene *s, int arg)
{
    if (!s) return;
    if (depth >= SCENE_STACK_MAX) {
        plat_log("scene: stack full, replacing top");
        call_leave(stack[depth - 1].s);
        depth--;
    }
    stack[depth].s = s;
    stack[depth].ticks = 0;
    depth++;
    log_scene("scene: push ", s);
    input_eat();
    call_enter(s, arg);
}

void scene_pop(void)
{
    const DoomoScene *s;
    if (depth <= 0) return;
    s = stack[depth - 1].s;
    depth--;
    log_scene("scene: pop ", s);
    input_eat();
    call_leave(s);
}

const DoomoScene *scene_top(void)
{
    return depth > 0 ? stack[depth - 1].s : NULL;
}

int scene_depth(void) { return depth; }

uint32_t scene_ticks(void)
{
    return depth > 0 ? stack[depth - 1].ticks : 0;
}

void scene_update(void)
{
    const DoomoScene *s;
    if (depth <= 0) return;
    s = stack[depth - 1].s;
    stack[depth - 1].ticks++;
    if (s->update) {
        int t = plat_scene_code(s);
        s->update();
        plat_scene_code_end(t);
    }
}

/* While a lower scene of the stack is drawn, the stack is temporarily
 * cut above it, so scene_top()/scene_ticks() describe the scene drawing. */
void scene_draw(void)
{
    int first, i, real = depth;
    if (depth <= 0) return;
    first = depth - 1;
    while (first > 0 && stack[first].s->overlay) first--;
    for (i = first; i < real; i++) {
        if (!stack[i].s->draw) continue;
        depth = i + 1;
        {
            int t = plat_scene_code(stack[i].s);
            stack[i].s->draw();
            plat_scene_code_end(t);
        }
        depth = real;
    }
}
