/* story_script.c - the main plot of Story Mode, beat by beat (owner:
 * story). Each beat has: who stands where (script_cast), what the HUD says
 * (script_objective), what talking / walking / entering does
 * (script_npc / script_trigger / script_place), what a finished sub-scene
 * means (script_resume), and how a test reaches it (script_debug_*).
 *
 * Tone: sugar first. Chapter 1 is a pastel daydream, chapter 2 lets the
 * purple in, chapter 3 is Doom, chapter 4 is a hug.
 *
 * S2: Chapters 1-2 (BEAT_C1_ARRIVE .. BEAT_C2_CELLAR) are VM scripts in the
 * ch1/ and ch2/ packs; every per-beat function here hands those beats to
 * story_ch12.c first (ch12_*). (S3) Chapters 3-4 and the epilogue
 * (BEAT_C3_ERUPTION on) are VM content too: every per-beat function hands
 * them to story_ch34.c (ch34_*), one marked line each. What is left below
 * is the cumulative state of the debug jumps.
 */
#include "story_int.h"
#include "story_ch12.h"       /* S2: Chapters 1-2 are VM content (story_ch12.c) */
#include "story_ch34.h"       /* S3: Chapters 3-4 are VM content (story_ch34.c) */

#define ME   DOOMO_PLAYER_ID
#define NAR  (-1)

/* Conversation callbacks run by id from SCENE_STORY's update (story.c
 * story_poll_callbacks), never through a function pointer: a pointer would
 * escape into the resident dialogue queue and pin this file's code and text
 * resident on the PS1 (SPEC C6). */
enum { CB_NONE };                  /* (S2/S3) the chapters own their callbacks */

static void flag(int f) { story_set_flag(f, 1); }
static int  has(int f) { return story_flag(f); }
static int  cur(void) { return story_beat(); }

static void unpossess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status != RES_POSSESSED) return;
    exorcism_unpossess(id);                       /* disarms; ALIVE, corruption 10 */
    portrait_invalidate(id);
}

/* Called by the overworld when the player walks into a nest. */
void script_nest_encounter(int nest)
{
    int cx, cy;
    if (nest < 0 || nest >= STORY_NESTS || has(SF_NEST0 + nest)) return;
    if (!ow_nest_cell(nest, &cx, &cy)) { cx = 30; cy = 30; }
    ui_toast(STR_ICON_SKULL " DEMONS! " STR_ICON_SKULL);
    audio_sfx(SFX_DEMON_ROAR);
    story_push_combat(MAPID_HAHALAND_INVADED, CTX_NEST, nest, cx, cy + 2, nest);
}

/* ======================================================================
 * script_npc: talking
 * ==================================================================== */
int script_npc(int id)
{
    int b = cur();
    if (ch12_beat(b)) return ch12_npc(id);              /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_npc(id);              /* S3: Chapters 3-4 */
    return 0;
}

/* What starts by itself on an idle overworld (story.c's pump). */
int script_auto(void)
{
    int b = cur();
    if (ch12_beat(b)) return ch12_auto();              /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_auto();              /* S3: Chapters 3-4 */
    return 0;
}

/* ======================================================================
 * script_trigger: walk-on events
 * ==================================================================== */
int script_trigger(int tag)
{
    int b = cur();
    if (ch12_beat(b) && ch12_trigger(tag)) return 1;    /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_trigger(tag);         /* S3: Chapters 3-4 */
    return 0;
}

/* ======================================================================
 * script_place: entering buildings
 * ==================================================================== */
int script_place(int place)
{
    int b = cur();
    if (ch12_beat(b) && ch12_place(place)) return 1;    /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_place(place);         /* S3: Chapters 3-4 */
    return 0;
}

/* ======================================================================
 * results of sub-scenes
 * ==================================================================== */
void script_resume(const StoryPending *p, int result)
{
    if (ch12_resume(p, result)) return;                 /* S2: Chapters 1-2 */
    if (ch34_beat(cur())) ch34_resume(p, result);       /* S3: Chapters 3-4 */
}

/* ======================================================================
 * entering a beat
 * ==================================================================== */
void script_enter_beat(int b)
{
    if (ch12_beat(b)) ch12_enter_beat(b);              /* S2: Chapters 1-2 */
    if (ch34_beat(b)) { ch34_enter_beat(b); return; }  /* S3: Chapters 3-4 */
    ow_refresh_cast();
}

/* Progress counted by the overworld's small talk: none any more (S2 / S3:
 * every episode has its own verbs). */
void script_ambient_talked(int id, int new_friend)
{
    (void)id;
    (void)new_friend;
}

/* ======================================================================
 * objectives + targets + the cast
 * ==================================================================== */
const char *script_objective(void)
{
    int b = cur();
    if (ch12_beat(b)) return ch12_objective();         /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_objective();         /* S3: Chapters 3-4 */
    return "";
}

int script_target(int *map_id, int *cx, int *cy)
{
    if (ch12_beat(cur())) return ch12_target(map_id, cx, cy);   /* S2: Chapters 1-2 */
    if (ch34_beat(cur())) return ch34_target(map_id, cx, cy);   /* S3: Chapters 3-4 */
    if (map_id) *map_id = -1;
    if (cx) *cx = 0;
    if (cy) *cy = 0;
    return -1;
}

int script_cast(int map, StoryNpcSpot *out, int max)
{
    int b = cur();
    if (ch12_beat(b)) return ch12_cast(map, out, max);  /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_cast(map, out, max);  /* S3: Chapters 3-4 */
    return 0;
}

/* ======================================================================
 * debug: jump + step
 * ==================================================================== */
static void ensure_pet(void) { if (!pet_exists()) pet_new("Doomo"); }

static void ensure_weapon(void)
{
    if (!has(SF_WEAPON_CHOSEN)) { flag(SF_WEAPON_CHOSEN); ch12_give_weapon(0); }
}

void script_debug_setup(int b)
{
    int i;
    DoomoStoryState *s = &doomo_town.story;
    s->quest = (uint8_t)b;                 /* targets below are the new beat's */
    if (b >= BEAT_C1_HOME) { flag(SF_KEY_GIVEN); flag(SF_EGG_GIVEN); }
    if (b >= BEAT_C1_HATCH) { flag(SF_HOME_SEEN); ensure_pet(); inv_take(ITEM_DOOMOGACHI_EGG, 1); }
    if (b == BEAT_C1_BUNNY_RETURN) { flag(SF_BUNNY_FOUND); if (!inv_count(ITEM_LOST_BUNNY)) inv_add(ITEM_LOST_BUNNY, 1); }
    if (b > BEAT_C1_BUNNY_RETURN) { flag(SF_BUNNY_FOUND); inv_take(ITEM_LOST_BUNNY, inv_count(ITEM_LOST_BUNNY)); }
    for (i = 0; i < 4; i++)
        if (b >= BEAT_C1_ARRIVE + 0 && story_beat_chapter(b) > i) flag(SF_TITLE_CH1 + i);
    if (b > BEAT_C1_CUPCAKE) flag(SF_CUPCAKE_GIVEN);
    if (b > BEAT_C1_WALL) flag(SF_FOG_SEEN);
    if (b > BEAT_C2_FUZZ) flag(SF_FUZZ_PRACTICE);
    if (b > BEAT_C2_EXORCISE) { unpossess(ID_STEAMY); flag(SF_STEAMY_SAVED); }
    if (b > BEAT_C2_RIVET) { ensure_weapon(); flag(SF_VEX_GONE); }
    ch12_debug_setup(b);                               /* S2: the Ch1-2 content's own state */
    if (ch34_beat(b)) { ch34_debug_setup(b); return; }  /* S3: Chapters 3-4 */
    {
        int m, cx, cy;
        script_target(&m, &cx, &cy);
        if (m < 0) { cx = 16; cy = 26; }
        if (b <= BEAT_C1_MAYOR) { cx = 34; cy = 26; }
        s->x = (int16_t)(cx * DOOMO_CELL_UNITS + 32);
        s->y = (int16_t)((cy + 1) * DOOMO_CELL_UNITS + 32);
    }
}

int script_debug_step(void)
{
    int b = cur();
    if (ch12_beat(b)) return ch12_debug_step();        /* S2: Chapters 1-2 */
    if (ch34_beat(b)) return ch34_debug_step();        /* S3: Chapters 3-4 */
    return 0;
}

/* ======================================================================
 * conversation callbacks by id (story.c story_poll_callbacks)
 * ==================================================================== */
void script_callback(int cb, int choice)
{
    if (cb == CB_C12) { ch12_action((uint16_t)choice); return; }   /* S2: a script's native CB_C12 */
    if (ch34_owns_cb(cb)) { ch34_callback(cb, choice); return; }   /* S3: Chapters 3-4 */
    (void)choice;
}
