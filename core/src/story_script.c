/* story_script.c - the main plot of Story Mode, beat by beat (owner:
 * story). Each beat has: who stands where (script_cast), what the HUD says
 * (script_objective), what talking / walking / entering does
 * (script_npc / script_trigger / script_place), what a finished sub-scene
 * means (script_resume), and how a test reaches it (script_debug_*).
 *
 * Tone: sugar first. Chapter 1 is a pastel daydream, chapter 2 lets the
 * purple in, chapter 3 is Doom, chapter 4 is a hug.
 */
#include "story_int.h"

#define ME   DOOMO_PLAYER_ID
#define NAR  (-1)

/* Conversation callbacks run by id from SCENE_STORY's update (story.c
 * story_poll_callbacks), never through a function pointer: a pointer would
 * escape into the resident dialogue queue and pin this file's code and text
 * resident on the PS1 (SPEC C6). */
enum { CB_NONE, CB_AFTER_NANA_KEY, CB_C3_CHECK, CB_C3_EXO_CHOICE, CB_CELLAR_CHOICE, CB_ERUPT, CB_EXORCISE_CHOICE, CB_FOG_CHOICE, CB_HELP_CHECK, CB_HOME_HATCH, CB_LULLA_DONE, CB_MAYOR_CAME, CB_OPEN_WORKSHOP_C2, CB_PRACTICE, CB_SMOG_CHOICE, CB_SPIRE_CHOICE, CB_STAGE_DONE, CB_SUMMIT_DONE, CB_TO_BUNNY, CB_TO_BUNNY_RETURN, CB_TO_CHAPTER2, CB_TO_EXORCISE, CB_TO_FRIENDS, CB_TO_FUZZ, CB_TO_KEY, CB_TO_PIP, CB_TO_RIVET, CB_TRAVEL_FROM_HH, CB_TRAVEL_FROM_HV, CB_TRAVEL_FROM_PK, CB_VAPULA_DOWN, CB_VEX_CHOICE, CB_WEAPON_CHOICE };

static void beat(int b) { story_set_beat(b); }
static void flag(int f) { story_set_flag(f, 1); }
static int  has(int f) { return story_flag(f); }
static int  cur(void) { return story_beat(); }

static int alive(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status != RES_EMPTY && r->status != RES_DEAD;
}

static int possessed(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status == RES_POSSESSED;
}

/* A story possession arms the host's authored possessor row (exorcism.h;
 * the @armed windows in legacy_ch*.dsc re-arm it after a load). */
static void possess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY || r->status == RES_DEAD) return;
    r->stat[STAT_CORRUPTION] = 100;
    r->status = RES_POSSESSED;
    r->exorcism = 0;
    exorcism_possess(id, story_possess_row_of(id));
    portrait_invalidate(id);
}

static void unpossess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status != RES_POSSESSED) return;
    exorcism_unpossess(id);                       /* disarms; ALIVE, corruption 10 */
    portrait_invalidate(id);
}

static void chapter_card(int ch)
{
    if (ch >= 1 && ch <= 4 && !has(SF_TITLE_CH1 + ch - 1)) {
        flag(SF_TITLE_CH1 + ch - 1);
        story_push_cutscene(CUT_CHAPTER1 + ch - 1, CTX_FREE, 0);
    }
}

/* ======================================================================
 * CHAPTER 1 - Welcome to HaHaland
 * ==================================================================== */
/* ======================================================================
 * CHAPTER 2 - Something in the Vapor
 * ==================================================================== */
/* ======================================================================
 * CHAPTER 3 - Hell Invades
 * ==================================================================== */
/* ======================================================================
 * CHAPTER 4 - Hellvile Park
 * ==================================================================== */
/* ======================================================================
 * beat transitions
 * ==================================================================== */
static void to_mayor(void) { beat(BEAT_C1_MAYOR); }
static void to_key(void) { beat(BEAT_C1_KEY); }
static void to_pip(void) { beat(BEAT_C1_PIP); }
static void to_bunny(void) { beat(BEAT_C1_BUNNY); }
static void to_bunny_return(void) { beat(BEAT_C1_BUNNY_RETURN); }
static void to_wall(void) { beat(BEAT_C1_WALL); }

static void after_nana_key(void)
{
    if (!inv_count(ITEM_HOUSE_KEY)) inv_add(ITEM_HOUSE_KEY, 1);
    if (!inv_count(ITEM_DOOMOGACHI_EGG) && !pet_exists()) inv_add(ITEM_DOOMOGACHI_EGG, 1);
    flag(SF_KEY_GIVEN);
    flag(SF_EGG_GIVEN);
    story_befriend(ID_NANA, 3);
    beat(BEAT_C1_HOME);
}

static void home_hatch(void)
{
    story_push_scene(&SCENE_PET, 0, PEND_PET, CTX_BEAT, BEAT_C1_HOME);
}

static void to_friends(void)
{
    inv_take(ITEM_LOST_BUNNY, 1);
    story_reward(30, ITEM_SKULL_TRINKET, 1, USTORY_NONE);
    story_befriend(ID_PIP, 4);
    beat(BEAT_C1_FRIENDS);
}

static void cupcake_given(void)
{
    flag(SF_CUPCAKE_GIVEN);
    if (!inv_count(ITEM_CUPCAKE)) inv_add(ITEM_CUPCAKE, 1);
}

static void lulla_done(void)
{
    inv_take(ITEM_CUPCAKE, 1);
    story_befriend(ID_LULLA, 4);
    story_reward(40, ITEM_NONE, 0, USTORY_NONE);
    to_wall();
}

static void to_chapter2(void) { beat(BEAT_C2_VAPOR); }

static void vex_choice(int c)
{
    story_befriend(ID_VEX, 1);
    if (c == 1) {
        DoomoResident *p = town_res(ME);
        flag(SF_VAPE_SAMPLE);
        if (p) p->stat[STAT_CORRUPTION] = (uint8_t)DOOMO_MIN(100, p->stat[STAT_CORRUPTION] + 15);
        inv_add(ITEM_VAPE_PEN, 1);
        story_conv(SCR_L_VEX_YES, 0);
    } else {
        story_conv(SCR_L_VEX_NO, 0);
    }
    beat(BEAT_C2_WEIRD);
}

static void to_fuzz(void) { beat(BEAT_C2_FUZZ); }

static void practice(void)
{
    story_push_exorcism(-1, CTX_BEAT, BEAT_C2_FUZZ);
}

static void to_exorcise(void) { beat(BEAT_C2_EXORCISE); }

static void exorcise_choice(int c)
{
    if (c == 0) story_push_exorcism(ID_STEAMY, CTX_BEAT, BEAT_C2_EXORCISE);
}

static void to_rivet(void) { beat(BEAT_C2_RIVET); }

static void give_weapon(int brutal)
{
    int i, slot = -1;
    DoomoWeapon w;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (!(doomo_town.weapons[i].flags & WF_VALID)) { slot = i; break; }
    if (slot < 0) slot = DOOMO_MAX_WEAPONS - 1;
    memset(&w, 0, sizeof(w));
    weld_default(&w, brutal ? WFRAME_PUMP_SHOTGUN : WFRAME_BUBBLE_WAND,
                 brutal ? WSTYLE_BRUTAL : WSTYLE_CUTE);
    w.quality = 72;
    w.charm = brutal ? WCHARM_SKULL : WCHARM_HEART;
    w.flags = WF_VALID | WF_FAVORITE;
    doomo_town.weapons[slot] = w;
    doomo_town.story.equipped = (uint8_t)slot;
    progress_add(PROG_WEAPONS_WELDED, 1);
    weld_gfx_invalidate();
}

static void after_workshop_c2(void) { story_conv(SCR_L_RIVET_VEX, 0); flag(SF_VEX_GONE); beat(BEAT_C2_CELLAR); }

static void open_workshop_c2(void)
{
    story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_BEAT, BEAT_C2_RIVET);
}

static void weapon_choice(int c)
{
    flag(SF_WEAPON_CHOSEN);
    story_befriend(ID_RIVET, 4);
    if (c == 1) {
        flag(SF_WEAPON_BRUTAL);
        give_weapon(1);
        unlock_grant(USTORY_BRUTAL_START);
        story_conv(SCR_L_RIVET_BRUTAL, CB_OPEN_WORKSHOP_C2);
    } else {
        give_weapon(0);
        unlock_grant(USTORY_CUTE_START);
        story_conv(SCR_L_RIVET_CUTE, CB_OPEN_WORKSHOP_C2);
    }
}

static void cellar_choice(int c)
{
    if (c == 0) story_push_combat(MAPID_VAPE_CELLAR, CTX_BEAT, BEAT_C2_CELLAR, -1, -1, -1);
}

static void erupt(void)
{
    story_push_cutscene(CUT_ERUPTION, CTX_BEAT, BEAT_C3_ERUPTION);
}

static void spire_choice(int c)
{
    if (c == 0) story_push_combat(MAPID_CLOUD_NINE_SPIRE, CTX_BEAT, BEAT_C3_SPIRE, -1, -1, -1);
}

static void vapula_down(void)
{
    story_push_cutscene(CUT_VAPULA_DOWN, CTX_BEAT, BEAT_C3_SPIRE);
}

static void to_prisoners(void) { beat(BEAT_C4_PRISONERS); }

static void help_check(void)
{
    if (cur() == BEAT_C4_HELP && has(SF_C4_SOOTKIN) && has(SF_C4_LADY_SMOG) && has(SF_C4_RIVET_HOME))
        story_conv(SCR_L_HELP_DONE, 0), beat(BEAT_C4_MAYOR);
}

static void smog_choice(int c)
{
    if (c == 0) story_push_exorcism(ID_LADY_SMOG, CTX_C4_SMOG, 0);
}

static void summit_done(void)
{
    story_push_cutscene(CUT_WALL_FALLS, CTX_BEAT, BEAT_C4_MAYOR);
}

static void mayor_came(void)
{
    flag(SF_C4_MAYOR_CAME);
    story_befriend(ID_MAYOR, 3);
    ow_refresh_cast();
}

static void stage_done(void)
{
    story_push_scene(&SCENE_CREDITS, 0, PEND_CREDITS, CTX_BEAT, BEAT_C4_STAGE);
}

static int c3_exo_index(int id)
{
    return id == ID_HATTIE ? 0 : id == ID_GRAMPY ? 1 : -1;
}

static void c3_exo_choice(int c)
{
    int k = c3_exo_index(story_fmt_target);
    if (c == 0 && k >= 0) story_push_exorcism(story_fmt_target, CTX_C3_EXO, k);
}

static int nests_left(void)
{
    int i, n = 0;
    for (i = 0; i < STORY_NESTS; i++) n += !has(SF_NEST0 + i);
    return n;
}

static int exos_left(void)
{
    int i, n = 0;
    for (i = 0; i < STORY_C3_POSSESSED; i++) n += !has(SF_C3_EXO0 + i);
    return n;
}

static void c3_check(void)
{
    if (cur() == BEAT_C3_NESTS && nests_left() == 0 && exos_left() == 0) {
        story_conv(SCR_L_ALL_NESTS, 0);
        beat(BEAT_C3_SPIRE);
    }
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
    switch (b) {
    case BEAT_C1_ARRIVE:
    case BEAT_C1_MAYOR:
        if (id == ID_MAYOR) {
            story_befriend(ID_MAYOR, 3);
            if (b == BEAT_C1_ARRIVE) beat(BEAT_C1_MAYOR);
            story_conv(SCR_L_MAYOR_HELLO, CB_TO_KEY);
            return 1;
        }
        break;
    case BEAT_C1_KEY:
        if (id == ID_NANA) { story_conv(SCR_L_NANA_KEY, CB_AFTER_NANA_KEY); return 1; }
        break;
    case BEAT_C1_PIP:
        if (id == ID_PIP) { story_conv(SCR_L_PIP_LOST, CB_TO_BUNNY); return 1; }
        break;
    case BEAT_C1_BUNNY_RETURN:
        if (id == ID_PIP) { story_conv(SCR_L_PIP_THANKS, CB_TO_FRIENDS); return 1; }
        break;
    case BEAT_C1_CUPCAKE:
        if (id == ID_NANA && !has(SF_CUPCAKE_GIVEN)) {
            cupcake_given();
            story_conv(SCR_L_NANA_CUPCAKE, 0);
            return 1;
        }
        if (id == ID_LULLA && has(SF_CUPCAKE_GIVEN)) {
            story_conv(SCR_L_LULLA, CB_LULLA_DONE);
            return 1;
        }
        break;
    case BEAT_C2_VAPOR:
        if (id == ID_VEX) {
            story_conv(SCR_L_VEX_HELLO, 0);
            story_conv(SCR_CH_VEX_CHOICE, CB_VEX_CHOICE);
            return 1;
        }
        break;
    case BEAT_C2_SCREAM:
        if (id == ID_STEAMY) { story_fmt_target = ID_STEAMY; story_conv(SCR_L_STEAMY_POSSESSED, CB_TO_FUZZ); return 1; }
        break;
    case BEAT_C2_FUZZ:
        if (id == ID_FUZZ) { story_conv(SCR_L_FUZZ_HELLO, CB_PRACTICE); return 1; }
        break;
    case BEAT_C2_EXORCISE:
        if ((id == ID_STEAMY || id == ID_FUZZ) && possessed(ID_STEAMY)) {
            story_fmt_target = ID_STEAMY;
            story_conv_arg(SCR_CH_EXORCISE_CHOICE, CB_EXORCISE_CHOICE, exorcism_odds(ID_STEAMY));
            return 1;
        }
        break;
    case BEAT_C2_RIVET:
        if (id == ID_RIVET) {
            story_conv(SCR_L_RIVET_HELLO, 0);
            story_conv(SCR_CH_WEAPON_CHOICE, CB_WEAPON_CHOICE);
            return 1;
        }
        break;
    case BEAT_C3_NESTS:
        if ((id == ID_HATTIE || id == ID_GRAMPY) && possessed(id)) {
            story_fmt_target = (int16_t)id;
            story_conv(SCR_L_POSSESSED_C3, 0);
            story_conv(SCR_CH_C3_EXO_CHOICE, CB_C3_EXO_CHOICE);
            return 1;
        }
        if (id == ID_RIVET && !has(SF_RIVET_UPGRADE)) {
            flag(SF_RIVET_UPGRADE);
            story_conv(SCR_L_RIVET_UPGRADE, 0);
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        break;
    case BEAT_C4_PRISONERS:
        if (id == ID_TALLYWORN && !story_flag(SF_MET0 + 8)) {
            story_set_flag(SF_MET0 + 8, 1);
            story_befriend(ID_TALLYWORN, 2);
            story_conv(SCR_L_TALLY, 0);
            return 1;
        }
        break;
    case BEAT_C4_HELP:
        if (id == ID_MOTHER_NOX && !story_flag(SF_MET0 + 9)) {
            story_set_flag(SF_MET0 + 9, 1);
            story_befriend(ID_MOTHER_NOX, 2);
            story_conv(SCR_L_NOX_HELLO, CB_HELP_CHECK);
            return 1;
        }
        if (id == ID_SOOTKIN && !has(SF_C4_SOOTKIN)) {
            if (inv_count(ITEM_FLOWERS) > 0) {
                inv_take(ITEM_FLOWERS, 1);
                flag(SF_C4_SOOTKIN);
                story_befriend(ID_SOOTKIN, 4);
                story_conv(SCR_L_SOOTKIN_THANKS, CB_HELP_CHECK);
            } else {
                story_conv(SCR_L_SOOTKIN_ASK, 0);
            }
            return 1;
        }
        if (id == ID_LADY_SMOG && !has(SF_C4_LADY_SMOG) && possessed(ID_LADY_SMOG)) {
            story_fmt_target = ID_LADY_SMOG;
            story_conv(SCR_L_SMOG, 0);
            story_conv(SCR_CH_SMOG_CHOICE, CB_SMOG_CHOICE);
            return 1;
        }
        if ((id == ID_ANVIL || id == ID_RIVET) && !has(SF_C4_RIVET_HOME)) {
            flag(SF_C4_RIVET_HOME);
            story_befriend(ID_ANVIL, 4);
            story_befriend(ID_RIVET, 2);
            story_conv(SCR_L_ANVIL, CB_HELP_CHECK);
            return 1;
        }
        if (id == ID_MOTHER_NOX) { help_check(); if (cur() == BEAT_C4_MAYOR) return 1; }
        break;
    case BEAT_C4_MAYOR:
        if (id == ID_MAYOR && !has(SF_C4_MAYOR_CAME)) {
            story_conv(SCR_L_MAYOR_CROSS, CB_MAYOR_CAME);
            return 1;
        }
        if ((id == ID_MAYOR || id == ID_MOTHER_NOX) && has(SF_C4_MAYOR_CAME)) {
            story_conv(SCR_L_SUMMIT, CB_SUMMIT_DONE);
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * script_trigger: walk-on events
 * ==================================================================== */
static void fog_choice(int c)
{
    if (c == 0) story_push_cutscene(CUT_FOG, CTX_BEAT, BEAT_C1_WALL);
}

int script_trigger(int tag)
{
    int b = cur();
    switch (tag) {
    case STORY_TAG_EVENT_BEACH:
        if (b == BEAT_C1_BUNNY && !has(SF_BUNNY_FOUND)) {
            flag(SF_BUNNY_FOUND);
            inv_add(ITEM_LOST_BUNNY, 1);
            audio_sfx(SFX_SQUEAK);
            story_conv(SCR_L_BUNNY_FOUND, CB_TO_BUNNY_RETURN);
            return 1;
        }
        break;
    case STORY_TAG_EVENT_FOG:
        if (b == BEAT_C1_WALL) {
            story_conv(SCR_CH_FOG_CHOICE, CB_FOG_CHOICE);
            return 1;
        }
        if (b <= BEAT_C1_CUPCAKE && !has(SF_WALL_HINT)) {
            flag(SF_WALL_HINT);
            story_say_txt(NAR, TXT_LS_1);
            return 1;
        }
        break;
    case STORY_TAG_EVENT_CELLAR:
        if (b == BEAT_C2_CELLAR) {
            story_conv(SCR_CH_CELLAR_CHOICE, CB_CELLAR_CHOICE);
            return 1;
        }
        if (b >= BEAT_C2_VAPOR && b < BEAT_C2_CELLAR) {
            story_say_txt(NAR, TXT_LS_2);
            return 1;
        }
        break;
    case STORY_TAG_EVENT_SPIRE:
        if (b == BEAT_C3_SPIRE) {
            story_conv(SCR_L_SPIRE_BASE, 0);
            story_conv(SCR_CH_SPIRE_CHOICE, CB_SPIRE_CHOICE);
            return 1;
        }
        if (b == BEAT_C3_NESTS) {
            story_say_txt(NAR, TXT_LS_3);
            return 1;
        }
        break;
    case STORY_TAG_EVENT_STAGE:
        if (b == BEAT_C4_STAGE) { story_conv(SCR_L_STAGE, CB_STAGE_DONE); return 1; }
        break;
    case STORY_TAG_EVENT_GATE_SEAL:
        story_say_txt(NAR, b >= BEAT_C4_GATE ?
                  TXT_LS_4 :
                  TXT_LS_5);
        return 1;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * script_place: entering buildings
 * ==================================================================== */
static int travel_ok_park(void) { return cur() >= BEAT_C4_FESTIVAL; }

static void travel_from_hh(int c)
{
    if (c == 0) {
        ow_enter_map(MAPID_HELLVILE, 3, 27);
        if (cur() == BEAT_C4_GATE) to_prisoners();
    } else if (c == 1 && travel_ok_park()) {
        ow_enter_map(MAPID_PARK_UNITED, 34, 24);
    }
}

static void travel_from_hv(int c)
{
    if (c == 0) ow_enter_map(MAPID_HAHALAND, 55, 30);
    else if (c == 1 && travel_ok_park()) ow_enter_map(MAPID_PARK_UNITED, 34, 24);
}

static void travel_from_pk(int c)
{
    if (c == 0) ow_enter_map(MAPID_HAHALAND, 55, 30);
    else if (c == 1) ow_enter_map(MAPID_HELLVILE, 3, 27);
}

int script_place(int place)
{
    int b = cur();
    int map = story_map();
    switch (place) {
    case PLACE_HOME:
        if (b == BEAT_C1_HOME) {
            inv_take(ITEM_HOUSE_KEY, 0);
            if (!has(SF_HOME_SEEN)) {
                flag(SF_HOME_SEEN);
                story_push_cutscene(CUT_HOME, CTX_BEAT, BEAT_C1_HOME);
            } else {
                story_conv(SCR_L_HOME, CB_HOME_HATCH);
            }
            return 1;
        }
        if (b < BEAT_C1_HOME) {
            story_say_txt(NAR, TXT_LS_6);
            return 1;
        }
        break;
    case PLACE_CAFE:
        if (b == BEAT_C1_KEY || (b == BEAT_C1_CUPCAKE && !has(SF_CUPCAKE_GIVEN))) return script_npc(ID_NANA);
        break;
    case PLACE_CHURCH:
        if (b == BEAT_C2_FUZZ) return script_npc(ID_FUZZ);
        break;
    case PLACE_WORKSHOP:
        if (b == BEAT_C2_RIVET) return script_npc(ID_RIVET);
        if (b == BEAT_C3_NESTS || b == BEAT_C3_SPIRE) {
            if (!has(SF_RIVET_UPGRADE)) return script_npc(ID_RIVET);
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        break;
    case PLACE_VAPE_SHOP:
        if (b < BEAT_C2_VAPOR) {
            story_say_txt(NAR, TXT_LS_7);
            return 1;
        }
        if (b == BEAT_C2_VAPOR) return script_npc(ID_VEX);
        if (b == BEAT_C3_SPIRE) return script_trigger(STORY_TAG_EVENT_SPIRE);
        if (b > BEAT_C2_VAPOR && b < BEAT_C3_ERUPTION) {
            story_say_txt(NAR, has(SF_VEX_GONE) ? TXT_LS_8 :
                      TXT_LS_9);
            return 1;
        }
        break;
    case PLACE_WALL_GATE:
        if (b < BEAT_C4_GATE) {
            story_say_txt(NAR, b >= BEAT_C3_ERUPTION ?
                      TXT_LS_10 :
                      TXT_LS_11);
            return 1;
        }
        if (map == MAPID_HAHALAND || map == MAPID_HAHALAND_INVADED) {
            if (b == BEAT_C4_GATE) { travel_from_hh(0); return 1; }
            story_conv(travel_ok_park() ? SCR_CH_TRAVEL_FROM_HH : SCR_CH_TRAVEL_FROM_HH2, CB_TRAVEL_FROM_HH);
            return 1;
        }
        if (map == MAPID_HELLVILE) {
            story_conv(SCR_CH_TRAVEL_FROM_HV, CB_TRAVEL_FROM_HV);
            return 1;
        }
        if (map == MAPID_PARK_UNITED) {
            story_conv(SCR_CH_TRAVEL_FROM_PK, CB_TRAVEL_FROM_PK);
            return 1;
        }
        break;
    case PLACE_STAGE:
        if (b == BEAT_C4_STAGE) return script_trigger(STORY_TAG_EVENT_STAGE);
        break;
    case PLACE_FORGE:
        if (b == BEAT_C4_HELP && !has(SF_C4_RIVET_HOME)) return script_npc(ID_ANVIL);
        break;
    case PLACE_CHAPEL:
        if (b == BEAT_C4_HELP) return script_npc(ID_MOTHER_NOX);
        break;
    case PLACE_TOWN_HALL:
        if (b == BEAT_C4_MAYOR && !has(SF_C4_MAYOR_CAME)) return script_npc(ID_MAYOR);
        break;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * results of sub-scenes
 * ==================================================================== */
static void to_hatch_lines(void) { story_conv(SCR_L_HATCHED, CB_TO_PIP); }

void script_resume(const StoryPending *p, int result)
{
    int b = cur();
    if (p->kind == PEND_CUTSCENE) {
        switch (p->arg) {
        case CUT_ARRIVAL:
            if (b == BEAT_C1_ARRIVE) to_mayor();
            break;
        case CUT_HOME:
            story_conv(SCR_L_HOME, CB_HOME_HATCH);
            break;
        case CUT_FOG:
            flag(SF_FOG_SEEN);
            story_conv(SCR_L_FOG_AFTER, CB_TO_CHAPTER2);
            break;
        case CUT_ERUPTION:
            beat(BEAT_C3_ERUPTION);
            break;
        case CUT_VAPULA_DOWN:
            beat(BEAT_C4_GATE);
            break;
        case CUT_WALL_FALLS:
            ow_enter_map(MAPID_PARK_UNITED, 16, 26);
            beat(BEAT_C4_FESTIVAL);
            break;
        case CUT_FREEPLAY:
            story_conv(SCR_L_DONE, 0);
            break;
        case CUT_CHAPTER1:
            if (b == BEAT_C1_ARRIVE) story_push_cutscene(CUT_ARRIVAL, CTX_BEAT, BEAT_C1_ARRIVE);
            break;
        case CUT_CHAPTER2:
            if (b == BEAT_C2_VAPOR) story_conv(SCR_L_CH2_OPEN, 0);
            break;
        case CUT_CHAPTER3:
            if (b == BEAT_C3_ERUPTION) { story_conv(SCR_L_INVADED, 0); beat(BEAT_C3_NESTS); }
            break;
        case CUT_CHAPTER4:
            if (b == BEAT_C4_GATE) story_conv(SCR_L_C4_OPEN, 0);
            break;
        default:
            break;
        }
        return;
    }
    switch (p->kind) {
    case PEND_PET:
        if (result > 0) {
            inv_take(ITEM_DOOMOGACHI_EGG, 1);
            if (b == BEAT_C1_HOME) { beat(BEAT_C1_HATCH); to_hatch_lines(); }
        } else {
            story_say_txt(NAR, TXT_LS_12);
        }
        break;
    case PEND_EXO_PRACTICE:
        if (b == BEAT_C2_FUZZ) {
            flag(SF_FUZZ_PRACTICE);
            story_befriend(ID_FUZZ, 3);
            story_conv(SCR_L_FUZZ_PRACTISED, CB_TO_EXORCISE);
        }
        break;
    case PEND_EXORCISM:
        if (p->ctx == CTX_BEAT && b == BEAT_C2_EXORCISE) {
            story_fmt_target = ID_STEAMY;
            if (result > 0) {
                flag(SF_STEAMY_SAVED);
                story_befriend(ID_STEAMY, 4);
                story_conv(SCR_L_STEAMY_SAVED, CB_TO_RIVET);
            } else if (result < 0) {
                flag(SF_STEAMY_LOST);
                story_conv(SCR_L_STEAMY_LOST, CB_TO_RIVET);
            } else {
                int left = exorcism_tries_left(ID_STEAMY);
                story_say_txt_n(ID_FUZZ, left == 1 ? TXT_LS_TRIES_1 : TXT_LS_TRIES_N, left);
            }
        } else if (p->ctx == CTX_C3_EXO) {
            story_fmt_target = p->arg;
            if (result != 0) {
                story_set_flag(SF_C3_EXO0 + (p->extra & 3), 1);
                if (result > 0) story_befriend(p->arg, 3);
                story_conv(result > 0 ? SCR_L_C3_SAVED : SCR_L_C3_LOST, CB_C3_CHECK);
            } else {
                story_say_txt(NAR, TXT_LS_13);
            }
        } else if (p->ctx == CTX_C4_SMOG) {
            story_fmt_target = ID_LADY_SMOG;
            if (result != 0) {
                flag(SF_C4_LADY_SMOG);
                if (result > 0) story_befriend(ID_LADY_SMOG, 4);
                story_conv(result > 0 ? SCR_L_SMOG_SAVED : SCR_L_SMOG_LOST, CB_HELP_CHECK);
            } else {
                story_say_txt(NAR, TXT_LS_14);
            }
        }
        break;
    case PEND_WORKSHOP:
        if (p->ctx == CTX_BEAT && b == BEAT_C2_RIVET) after_workshop_c2();
        break;
    case PEND_COMBAT:
        if (p->ctx == CTX_NEST) {
            if (result > 0) {
                story_set_flag(SF_NEST0 + (p->extra & 7), 1);
                story_reward(40, ITEM_SCRAP, 2, USTORY_NONE);
                story_conv(SCR_L_NEST_CLEARED, CB_C3_CHECK);
                if (nests_left() == 2 && !has(SF_RIVET_UPGRADE)) story_conv(SCR_L_RIVET_CALL, CB_C3_CHECK);
            } else {
                story_say_txt(NAR, TXT_LS_15);
            }
        } else if (p->extra == BEAT_C2_CELLAR && b == BEAT_C2_CELLAR) {
            if (result > 0) { story_reward(100, ITEM_SCRAP, 3, USTORY_NONE); story_conv(SCR_L_CELLAR_WON, CB_ERUPT); }
            else story_conv(SCR_L_CELLAR_LOST, 0);
        } else if (p->extra == BEAT_C3_SPIRE && b == BEAT_C3_SPIRE) {
            if (result > 0) { story_reward(300, ITEM_GATE_SIGIL, 1, USTORY_NONE); story_conv(SCR_L_SPIRE_WON, CB_VAPULA_DOWN); }
            else story_say_txt(NAR, TXT_LS_16);
        }
        break;
    case PEND_CREDITS:
        if (b == BEAT_C4_STAGE) {
            flag(SF_CREDITS_SEEN);
            beat(BEAT_DONE);
            unlock_grant(USTORY_UNITED_PARK);
            unlock_grant(USTORY_NEW_GAME_PLUS);
            unlock_grant(173);                    /* foundation: United Park title theme */
            unlock_grant(174);                    /* foundation: Cute + Gore gore mode */
            story_push_cutscene(CUT_FREEPLAY, CTX_BEAT, BEAT_DONE);
        }
        break;
    default:
        break;
    }
}

/* ======================================================================
 * entering a beat
 * ==================================================================== */
void script_enter_beat(int b)
{
    switch (b) {
    case BEAT_C1_FRIENDS:
        break;
    case BEAT_C1_WALL:
        break;
    case BEAT_C2_VAPOR:
        chapter_card(2);
        break;
    case BEAT_C2_WEIRD:
        story_set_step(0);
        break;
    case BEAT_C2_SCREAM:
        possess(ID_STEAMY);
        ow_refresh_cast();
        break;
    case BEAT_C3_ERUPTION:
        possess(ID_HATTIE);
        possess(ID_GRAMPY);
        ow_enter_map(MAPID_HAHALAND_INVADED, 52, 39);
        chapter_card(3);
        break;
    case BEAT_C3_SPIRE:
        break;
    case BEAT_C4_GATE:
        /* the town heals; anyone still possessed by the cloud is freed */
        unpossess(ID_HATTIE);
        unpossess(ID_GRAMPY);
        possess(ID_LADY_SMOG);
        ow_enter_map(MAPID_HAHALAND, 52, 39);
        chapter_card(4);
        unlock_grant(USTORY_HELLVILE_VISITS);
        break;
    case BEAT_C4_PRISONERS:
        break;
    case BEAT_C4_FESTIVAL:
        story_conv(SCR_L_FESTIVAL, 0);
        break;
    case BEAT_C4_STAGE:
        story_conv(SCR_L_STAGE_CALL, 0);
        break;
    default:
        break;
    }
    ow_refresh_cast();
}

/* Progress counted by the overworld's small talk (FRIENDS, WEIRD,
 * PRISONERS, FESTIVAL). */
void script_ambient_talked(int id, int new_friend)
{
    int b = cur();
    if (b == BEAT_C1_FRIENDS && new_friend && !RES_DISTRICT(id)) {
        story_set_step(story_step() + 1);
        if (story_step() >= 3) { story_conv(SCR_L_THREE_FRIENDS, 0); beat(BEAT_C1_CUPCAKE); }
    } else if (b == BEAT_C2_WEIRD) {
        story_set_step(story_step() + 1);
        if (story_step() >= 3) { story_conv(SCR_L_SCREAM, 0); beat(BEAT_C2_SCREAM); }
    } else if (b == BEAT_C4_PRISONERS) {
        if (story_hellvile_friends() >= 5) { story_conv(SCR_L_WORD_SPREADS, 0); beat(BEAT_C4_HELP); }
    } else if (b == BEAT_C4_FESTIVAL) {
        story_set_step(story_step() + 1);
        if (story_step() >= 4) beat(BEAT_C4_STAGE);
    }
}

/* ======================================================================
 * objectives + targets
 * ==================================================================== */
/* The HUD line, decoded from the legacy pack and formatted only when it
 * changes (the HUD asks every frame). */
static const char *obj(unsigned tid, int n0, int n1, int n2, int n3)
{
    static char buf[72];
    static uint16_t last = 0xFFFFu;
    static int16_t ln[4];
    int n[4];
    n[0] = n0; n[1] = n1; n[2] = n2; n[3] = n3;
    if (last != tid || ln[0] != n0 || ln[1] != n1 || ln[2] != n2 || ln[3] != n3) {
        char raw[72];
        last = (uint16_t)tid;
        ln[0] = (int16_t)n0; ln[1] = (int16_t)n1; ln[2] = (int16_t)n2; ln[3] = (int16_t)n3;
        if (txt_get((uint16_t)tid, raw, (int)sizeof(raw)) < 0) raw[0] = 0;
        vm_fmt_n(buf, (int)sizeof(buf), raw, n, 4);
    }
    return buf;
}
#define OBJ(t) obj(t, 0, 0, 0, 0)

const char *script_objective(void)
{
    int b = cur();
    switch (b) {
    case BEAT_C1_ARRIVE:
    case BEAT_C1_MAYOR: return OBJ(TXT_OBJ_C1_MAYOR);
    case BEAT_C1_KEY: return OBJ(TXT_OBJ_C1_KEY);
    case BEAT_C1_HOME: return OBJ(TXT_OBJ_C1_HOME);
    case BEAT_C1_HATCH: return OBJ(TXT_OBJ_C1_HATCH);
    case BEAT_C1_PIP: return OBJ(TXT_OBJ_C1_PIP);
    case BEAT_C1_BUNNY: return OBJ(TXT_OBJ_C1_BUNNY);
    case BEAT_C1_BUNNY_RETURN: return OBJ(TXT_OBJ_C1_BUNNY_RETURN);
    case BEAT_C1_FRIENDS: return obj(TXT_OBJ_C1_FRIENDS, story_step(), 0, 0, 0);
    case BEAT_C1_CUPCAKE: return OBJ(has(SF_CUPCAKE_GIVEN) ? TXT_OBJ_C1_CUPCAKE_LULLA : TXT_OBJ_C1_CUPCAKE_NANA);
    case BEAT_C1_WALL: return OBJ(TXT_OBJ_C1_WALL);
    case BEAT_C2_VAPOR: return OBJ(TXT_OBJ_C2_VAPOR);
    case BEAT_C2_WEIRD: return obj(TXT_OBJ_C2_WEIRD, story_step(), 0, 0, 0);
    case BEAT_C2_SCREAM: return OBJ(TXT_OBJ_C2_SCREAM);
    case BEAT_C2_FUZZ: return OBJ(TXT_OBJ_C2_FUZZ);
    case BEAT_C2_EXORCISE: return OBJ(TXT_OBJ_C2_EXORCISE);
    case BEAT_C2_RIVET: return OBJ(TXT_OBJ_C2_RIVET);
    case BEAT_C2_CELLAR: return OBJ(TXT_OBJ_C2_CELLAR);
    case BEAT_C3_ERUPTION: return OBJ(TXT_OBJ_C3_ERUPTION);
    case BEAT_C3_NESTS:
        return obj(TXT_OBJ_C3_NESTS, STORY_NESTS - nests_left(), STORY_NESTS,
                   STORY_C3_POSSESSED - exos_left(), STORY_C3_POSSESSED);
    case BEAT_C3_SPIRE: return OBJ(TXT_OBJ_C3_SPIRE);
    case BEAT_C4_GATE: return OBJ(TXT_OBJ_C4_GATE);
    case BEAT_C4_PRISONERS:
        if (!story_flag(SF_MET0 + 8)) return OBJ(TXT_OBJ_C4_TALLY);
        return obj(TXT_OBJ_C4_PRISONERS, DOOMO_MIN(5, story_hellvile_friends()), 0, 0, 0);
    case BEAT_C4_HELP:
        if (!story_flag(SF_MET0 + 9)) return OBJ(TXT_OBJ_C4_NOX);
        return obj(TXT_OBJ_C4_HELP, has(SF_C4_SOOTKIN) + has(SF_C4_LADY_SMOG) + has(SF_C4_RIVET_HOME), 0, 0, 0);
    case BEAT_C4_MAYOR: return OBJ(has(SF_C4_MAYOR_CAME) ? TXT_OBJ_C4_SUMMIT : TXT_OBJ_C4_MAYOR);
    case BEAT_C4_FESTIVAL: return obj(TXT_OBJ_C4_FESTIVAL, story_step(), 0, 0, 0);
    case BEAT_C4_STAGE: return OBJ(TXT_OBJ_C4_STAGE);
    case BEAT_DONE: {
        const char *s = side_objective();
        return (s && s[0]) ? s : OBJ(TXT_OBJ_DONE);
    }
    default: return "";
    }
}

/* Where the main target is: resident id (or -1) and map/cell. */
typedef struct { int16_t id; uint8_t map, x, y; } Target;

static Target target_of(int b)
{
    Target t = { -1, MAPID_HAHALAND, 0, 0 };
    switch (b) {
    case BEAT_C1_ARRIVE: case BEAT_C1_MAYOR: t.id = ID_MAYOR; t.x = 33; t.y = 25; break;
    case BEAT_C1_KEY: t.id = ID_NANA; t.x = 25; t.y = 17; break;
    case BEAT_C1_HOME: t.x = 4; t.y = 7; break;
    case BEAT_C1_PIP: case BEAT_C1_BUNNY_RETURN: t.id = ID_PIP; t.x = 10; t.y = 47; break;
    case BEAT_C1_BUNNY: t.x = 12; t.y = 55; break;
    case BEAT_C1_CUPCAKE:
        if (has(SF_CUPCAKE_GIVEN)) { t.id = ID_LULLA; t.x = 30; t.y = 42; }
        else { t.id = ID_NANA; t.x = 25; t.y = 17; }
        break;
    case BEAT_C1_WALL: t.x = 55; t.y = 24; break;
    case BEAT_C2_VAPOR: t.id = ID_VEX; t.x = 54; t.y = 37; break;
    case BEAT_C2_SCREAM: t.id = ID_STEAMY; t.x = 38; t.y = 24; break;
    case BEAT_C2_FUZZ: t.id = ID_FUZZ; t.x = 7; t.y = 28; break;
    case BEAT_C2_EXORCISE: t.id = ID_STEAMY; t.x = 38; t.y = 24; break;
    case BEAT_C2_RIVET: t.id = ID_RIVET; t.x = 53; t.y = 10; break;
    case BEAT_C2_CELLAR: t.x = 53; t.y = 37; break;
    case BEAT_C3_NESTS: {
        int i;
        t.map = MAPID_HAHALAND_INVADED;
        for (i = 0; i < STORY_NESTS; i++)
            if (!has(SF_NEST0 + i)) { int cx, cy; if (ow_nest_cell(i, &cx, &cy)) { t.x = (uint8_t)cx; t.y = (uint8_t)cy; } return t; }
        if (!has(SF_C3_EXO0)) { t.id = ID_HATTIE; t.x = 40; t.y = 21; }
        else { t.id = ID_GRAMPY; t.x = 27; t.y = 54; }
        break; }
    case BEAT_C3_SPIRE: t.map = MAPID_HAHALAND_INVADED; t.x = 53; t.y = 37; break;
    case BEAT_C4_GATE: t.x = 55; t.y = 30; break;
    case BEAT_C4_PRISONERS:
        t.map = MAPID_HELLVILE;
        if (!story_flag(SF_MET0 + 8)) { t.id = ID_TALLYWORN; t.x = 9; t.y = 30; }
        else { t.x = 20; t.y = 27; }
        break;
    case BEAT_C4_HELP:
        t.map = MAPID_HELLVILE;
        if (!story_flag(SF_MET0 + 9)) { t.id = ID_MOTHER_NOX; t.x = 7; t.y = 13; }
        else if (!has(SF_C4_SOOTKIN)) {
            if (inv_count(ITEM_FLOWERS) > 0) { t.id = ID_SOOTKIN; t.x = 41; t.y = 35; }
            else { t.map = MAPID_HAHALAND; t.x = 31; t.y = 18; }
        }
        else if (!has(SF_C4_LADY_SMOG)) { t.id = ID_LADY_SMOG; t.x = 49; t.y = 17; }
        else { t.id = ID_ANVIL; t.x = 31; t.y = 9; }
        break;
    case BEAT_C4_MAYOR:
        if (!has(SF_C4_MAYOR_CAME)) { t.id = ID_MAYOR; t.x = 54; t.y = 30; }
        else { t.map = MAPID_HELLVILE; t.id = ID_MOTHER_NOX; t.x = 8; t.y = 24; }
        break;
    case BEAT_C4_FESTIVAL: t.map = MAPID_PARK_UNITED; t.x = 16; t.y = 30; break;
    case BEAT_C4_STAGE: t.map = MAPID_PARK_UNITED; t.x = 31; t.y = 9; break;
    default: t.map = 0xFF; break;
    }
    return t;
}

int script_target(int *map_id, int *cx, int *cy)
{
    Target t = target_of(cur());
    if (map_id) *map_id = t.map == 0xFF ? -1 : t.map;
    if (cx) *cx = t.x;
    if (cy) *cy = t.y;
    return t.id;
}

/* ======================================================================
 * the cast on the map
 * ==================================================================== */
static int add(StoryNpcSpot *out, int n, int max, int id, int x, int y, int dir, int flags)
{
    if (n >= max || !alive(id)) return n;
    out[n].id = (int16_t)id;
    out[n].x = (uint8_t)x;
    out[n].y = (uint8_t)y;
    out[n].dir = (uint8_t)dir;
    out[n].flags = (uint8_t)flags;
    return n + 1;
}

int script_cast(int map, StoryNpcSpot *out, int max)
{
    int b = cur(), n = 0;
    Target t = target_of(b);
#define MAINF(who) ((t.id == (who)) ? NPCF_MAIN | NPCF_FIXED : NPCF_FIXED)
    if (map == MAPID_HAHALAND) {
        /* the Mayor */
        if (b <= BEAT_C1_MAYOR) n = add(out, n, max, ID_MAYOR, 33, 25, 0, MAINF(ID_MAYOR));
        else if (b == BEAT_C2_SCREAM || b == BEAT_C2_EXORCISE) n = add(out, n, max, ID_MAYOR, 35, 25, 2, NPCF_FIXED);
        else if (b == BEAT_C4_GATE || (b == BEAT_C4_MAYOR && !has(SF_C4_MAYOR_CAME))) n = add(out, n, max, ID_MAYOR, 54, 30, 1, MAINF(ID_MAYOR));
        else if (b < BEAT_C3_ERUPTION || b >= BEAT_DONE) n = add(out, n, max, ID_MAYOR, 29, 8, 0, NPCF_FIXED);
        /* Nana on her terrace */
        n = add(out, n, max, ID_NANA, 25, 17, 0, MAINF(ID_NANA));
        /* Pip at the playground */
        n = add(out, n, max, ID_PIP, 10, 47, 0, MAINF(ID_PIP));
        /* Miss Lulla on her park bench */
        n = add(out, n, max, ID_LULLA, 30, 42, 0, MAINF(ID_LULLA));
        /* Father Fuzz */
        if (b == BEAT_C2_EXORCISE) n = add(out, n, max, ID_FUZZ, 36, 24, 2, NPCF_MAIN | NPCF_FIXED);
        else n = add(out, n, max, ID_FUZZ, 7, 28, 0, MAINF(ID_FUZZ));
        /* Steamy */
        if (b == BEAT_C2_SCREAM || b == BEAT_C2_EXORCISE) n = add(out, n, max, ID_STEAMY, 38, 24, 0, MAINF(ID_STEAMY));
        /* Dr. Bloom at the hospital */
        n = add(out, n, max, ID_BLOOM, 17, 23, 0, NPCF_FIXED);
        /* Rivet at the workshop (chapter 2 on, until she goes home) */
        if (b >= BEAT_C2_VAPOR && !has(SF_C4_RIVET_HOME)) {
            if (b == BEAT_C4_GATE) n = add(out, n, max, ID_RIVET, 54, 28, 1, NPCF_FIXED);
            else n = add(out, n, max, ID_RIVET, 53, 10, 0, MAINF(ID_RIVET));
        }
        /* Vex outside Cloud Nine */
        if (b >= BEAT_C2_VAPOR && b <= BEAT_C2_RIVET) n = add(out, n, max, ID_VEX, 54, 37, 0, MAINF(ID_VEX));
    } else if (map == MAPID_HAHALAND_INVADED) {
        /* refugees at the church */
        n = add(out, n, max, ID_MAYOR, 4, 28, 0, NPCF_FIXED);
        n = add(out, n, max, ID_FUZZ, 7, 28, 0, NPCF_FIXED);
        n = add(out, n, max, ID_NANA, 9, 28, 0, NPCF_FIXED);
        n = add(out, n, max, ID_PIP, 5, 29, 0, NPCF_FIXED);
        n = add(out, n, max, ID_BLOOM, 10, 29, 0, NPCF_FIXED);
        n = add(out, n, max, ID_LULLA, 3, 29, 0, NPCF_FIXED);
        n = add(out, n, max, ID_RIVET, 53, 10, 0, (b == BEAT_C3_NESTS && !has(SF_RIVET_UPGRADE)) ? NPCF_FIXED | NPCF_SIDE : NPCF_FIXED);
        if (possessed(ID_HATTIE)) n = add(out, n, max, ID_HATTIE, 40, 21, 0, MAINF(ID_HATTIE) | NPCF_MAIN);
        if (possessed(ID_GRAMPY)) n = add(out, n, max, ID_GRAMPY, 27, 54, 0, MAINF(ID_GRAMPY) | NPCF_MAIN);
    } else if (map == MAPID_HELLVILE) {
        n = add(out, n, max, ID_TALLYWORN, 9, 30, 0, MAINF(ID_TALLYWORN));
        if (b == BEAT_C4_MAYOR && has(SF_C4_MAYOR_CAME)) {
            n = add(out, n, max, ID_MAYOR, 6, 24, 2, NPCF_MAIN | NPCF_FIXED);
            n = add(out, n, max, ID_MOTHER_NOX, 8, 24, 1, NPCF_MAIN | NPCF_FIXED);
        } else {
            n = add(out, n, max, ID_MOTHER_NOX, 7, 13, 0, MAINF(ID_MOTHER_NOX));
        }
        n = add(out, n, max, ID_SOOTKIN, 41, 35, 0, MAINF(ID_SOOTKIN));
        n = add(out, n, max, ID_LADY_SMOG, 49, 17, 0, MAINF(ID_LADY_SMOG));
        n = add(out, n, max, ID_ANVIL, 31, 9, 0, MAINF(ID_ANVIL));
        if (b == BEAT_C4_HELP && !has(SF_C4_RIVET_HOME)) n = add(out, n, max, ID_RIVET, 29, 9, 2, NPCF_FIXED);
        else if (has(SF_C4_RIVET_HOME) && b < BEAT_C4_FESTIVAL) n = add(out, n, max, ID_RIVET, 29, 9, 2, NPCF_FIXED);
        if (b >= BEAT_C4_GATE && b < BEAT_C4_FESTIVAL) n = add(out, n, max, ID_VEX, 47, 15, 0, NPCF_FIXED);
        if (b >= BEAT_C4_GATE) n = add(out, n, max, ID_HAZE, 44, 13, 0, NPCF_FIXED);
    } else if (map == MAPID_PARK_UNITED) {
        n = add(out, n, max, ID_MAYOR, 29, 10, 0, MAINF(ID_MAYOR));
        n = add(out, n, max, ID_MOTHER_NOX, 34, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_FUZZ, 26, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_RIVET, 37, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_ANVIL, 38, 11, 0, NPCF_FIXED);
        n = add(out, n, max, ID_PIP, 31, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_NANA, 4, 18, 0, NPCF_FIXED);
        n = add(out, n, max, ID_VEX, 37, 18, 0, NPCF_FIXED);
        n = add(out, n, max, ID_SOOTKIN, 28, 20, 0, NPCF_FIXED);
        n = add(out, n, max, ID_VAPULA, 45, 30, 0, NPCF_FIXED);
        n = add(out, n, max, ID_BLOOM, 12, 26, 0, NPCF_FIXED);
        n = add(out, n, max, ID_TALLYWORN, 35, 20, 0, NPCF_FIXED);
    }
#undef MAINF
    return n;
}

/* ======================================================================
 * debug: jump + step
 * ==================================================================== */
static void ensure_pet(void) { if (!pet_exists()) pet_new("Doomo"); }

static void ensure_weapon(void)
{
    if (!has(SF_WEAPON_CHOSEN)) { flag(SF_WEAPON_CHOSEN); give_weapon(0); }
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
    if (b == BEAT_C2_SCREAM || b == BEAT_C2_FUZZ || b == BEAT_C2_EXORCISE) possess(ID_STEAMY);
    if (b > BEAT_C2_FUZZ) flag(SF_FUZZ_PRACTICE);
    if (b > BEAT_C2_EXORCISE) { unpossess(ID_STEAMY); flag(SF_STEAMY_SAVED); }
    if (b > BEAT_C2_RIVET) { ensure_weapon(); flag(SF_VEX_GONE); }
    if (b >= BEAT_C3_ERUPTION && b < BEAT_C4_GATE) { s->map = MAPID_HAHALAND_INVADED; }
    if (b == BEAT_C3_NESTS) { possess(ID_HATTIE); possess(ID_GRAMPY); }
    if (b > BEAT_C3_NESTS) {
        for (i = 0; i < STORY_NESTS; i++) flag(SF_NEST0 + i);
        for (i = 0; i < STORY_C3_POSSESSED; i++) flag(SF_C3_EXO0 + i);
        unpossess(ID_HATTIE);
        unpossess(ID_GRAMPY);
    }
    if (b >= BEAT_C4_GATE) s->map = MAPID_HAHALAND;
    if (b >= BEAT_C4_PRISONERS) s->map = MAPID_HELLVILE;
    if (b > BEAT_C4_PRISONERS) {
        static const int16_t hv[5] = { ID_TALLYWORN, 507, 509, 531, 537 };
        story_set_flag(SF_MET0 + 8, 1);
        for (i = 0; i < 5; i++) story_befriend(hv[i], STORY_FRIEND_LEVEL);
    }
    if (b == BEAT_C4_HELP) possess(ID_LADY_SMOG);
    if (b > BEAT_C4_HELP) {
        story_set_flag(SF_MET0 + 9, 1);
        flag(SF_C4_SOOTKIN); flag(SF_C4_LADY_SMOG); flag(SF_C4_RIVET_HOME);
        unpossess(ID_LADY_SMOG);
    }
    if (b >= BEAT_C4_FESTIVAL) { flag(SF_C4_MAYOR_CAME); s->map = MAPID_PARK_UNITED; }
    if (b >= BEAT_DONE) flag(SF_CREDITS_SEEN);
    {
        int m, cx, cy;
        script_target(&m, &cx, &cy);
        if (m < 0) { cx = 16; cy = 26; }
        if (b <= BEAT_C1_MAYOR) { cx = 34; cy = 26; }
        s->x = (int16_t)(cx * DOOMO_CELL_UNITS + 32);
        s->y = (int16_t)((cy + 1) * DOOMO_CELL_UNITS + 32);
    }
}

/* A citizen of a district the player has not befriended yet. */
static int fresh_citizen(int district)
{
    int i, lo = district ? 500 : 100;
    for (i = lo; i < lo + 400; i++) {
        const DoomoResident *r = town_res(i);
        if (r && r->status == RES_ALIVE && r->level < STORY_FRIEND_LEVEL && roster_npc_of(i) < 0 &&
            !(r->flags & RF_STORY_NPC))
            return i;
    }
    return lo;
}

int script_debug_step(void)
{
    int b = cur(), i;
    switch (b) {
    case BEAT_C1_ARRIVE: beat(BEAT_C1_MAYOR); return 1;
    case BEAT_C1_MAYOR: story_interact_npc(ID_MAYOR); return 1;
    case BEAT_C1_KEY: story_interact_npc(ID_NANA); return 1;
    case BEAT_C1_HOME: story_enter_place(PLACE_HOME); return 1;
    case BEAT_C1_HATCH: beat(BEAT_C1_PIP); return 1;
    case BEAT_C1_PIP: story_interact_npc(ID_PIP); return 1;
    case BEAT_C1_BUNNY: story_trigger_ow(STORY_TAG_EVENT_BEACH); return 1;
    case BEAT_C1_BUNNY_RETURN: story_interact_npc(ID_PIP); return 1;
    case BEAT_C1_FRIENDS: story_interact_npc(fresh_citizen(0)); return 1;
    case BEAT_C1_CUPCAKE: story_interact_npc(has(SF_CUPCAKE_GIVEN) ? ID_LULLA : ID_NANA); return 1;
    case BEAT_C1_WALL: story_trigger_ow(STORY_TAG_EVENT_FOG); return 1;
    case BEAT_C2_VAPOR: story_interact_npc(ID_VEX); return 1;
    case BEAT_C2_WEIRD: story_interact_npc(fresh_citizen(0)); return 1;
    case BEAT_C2_SCREAM: story_interact_npc(ID_STEAMY); return 1;
    case BEAT_C2_FUZZ: story_interact_npc(ID_FUZZ); return 1;
    case BEAT_C2_EXORCISE: story_interact_npc(ID_STEAMY); return 1;
    case BEAT_C2_RIVET: story_interact_npc(ID_RIVET); return 1;
    case BEAT_C2_CELLAR: story_trigger_ow(STORY_TAG_EVENT_CELLAR); return 1;
    case BEAT_C3_ERUPTION:
        beat(BEAT_C3_NESTS);
        return 1;
    case BEAT_C3_NESTS:
        for (i = 0; i < STORY_NESTS; i++)
            if (!has(SF_NEST0 + i)) { script_nest_encounter(i); return 1; }
        if (possessed(ID_HATTIE)) { story_interact_npc(ID_HATTIE); return 1; }
        if (possessed(ID_GRAMPY)) { story_interact_npc(ID_GRAMPY); return 1; }
        for (i = 0; i < STORY_C3_POSSESSED; i++) flag(SF_C3_EXO0 + i);
        c3_check();
        return 1;
    case BEAT_C3_SPIRE: story_trigger_ow(STORY_TAG_EVENT_SPIRE); return 1;
    case BEAT_C4_GATE: story_enter_place(PLACE_WALL_GATE); return 1;
    case BEAT_C4_PRISONERS:
        if (!story_flag(SF_MET0 + 8)) story_interact_npc(ID_TALLYWORN);
        else story_interact_npc(fresh_citizen(1));
        return 1;
    case BEAT_C4_HELP:
        if (!story_flag(SF_MET0 + 9)) { story_interact_npc(ID_MOTHER_NOX); return 1; }
        if (!has(SF_C4_SOOTKIN)) {
            if (!inv_count(ITEM_FLOWERS)) inv_add(ITEM_FLOWERS, 1);
            story_interact_npc(ID_SOOTKIN);
            return 1;
        }
        if (!has(SF_C4_LADY_SMOG)) { story_interact_npc(ID_LADY_SMOG); return 1; }
        if (!has(SF_C4_RIVET_HOME)) { story_interact_npc(ID_ANVIL); return 1; }
        help_check();
        return 1;
    case BEAT_C4_MAYOR: story_interact_npc(ID_MAYOR); return 1;
    case BEAT_C4_FESTIVAL: story_interact_npc(fresh_citizen(story_step() & 1)); return 1;
    case BEAT_C4_STAGE: story_trigger_ow(STORY_TAG_EVENT_STAGE); return 1;
    default: return 0;
    }
}

/* ======================================================================
 * conversation callbacks by id (story.c story_poll_callbacks)
 * ==================================================================== */
void script_callback(int cb, int choice)
{
    switch (cb) {
    case CB_AFTER_NANA_KEY: after_nana_key(); break;
    case CB_C3_CHECK: c3_check(); break;
    case CB_C3_EXO_CHOICE: c3_exo_choice(choice); break;
    case CB_CELLAR_CHOICE: cellar_choice(choice); break;
    case CB_ERUPT: erupt(); break;
    case CB_EXORCISE_CHOICE: exorcise_choice(choice); break;
    case CB_FOG_CHOICE: fog_choice(choice); break;
    case CB_HELP_CHECK: help_check(); break;
    case CB_HOME_HATCH: home_hatch(); break;
    case CB_LULLA_DONE: lulla_done(); break;
    case CB_MAYOR_CAME: mayor_came(); break;
    case CB_OPEN_WORKSHOP_C2: open_workshop_c2(); break;
    case CB_PRACTICE: practice(); break;
    case CB_SMOG_CHOICE: smog_choice(choice); break;
    case CB_SPIRE_CHOICE: spire_choice(choice); break;
    case CB_STAGE_DONE: stage_done(); break;
    case CB_SUMMIT_DONE: summit_done(); break;
    case CB_TO_BUNNY: to_bunny(); break;
    case CB_TO_BUNNY_RETURN: to_bunny_return(); break;
    case CB_TO_CHAPTER2: to_chapter2(); break;
    case CB_TO_EXORCISE: to_exorcise(); break;
    case CB_TO_FRIENDS: to_friends(); break;
    case CB_TO_FUZZ: to_fuzz(); break;
    case CB_TO_KEY: to_key(); break;
    case CB_TO_PIP: to_pip(); break;
    case CB_TO_RIVET: to_rivet(); break;
    case CB_TRAVEL_FROM_HH: travel_from_hh(choice); break;
    case CB_TRAVEL_FROM_HV: travel_from_hv(choice); break;
    case CB_TRAVEL_FROM_PK: travel_from_pk(choice); break;
    case CB_VAPULA_DOWN: vapula_down(); break;
    case CB_VEX_CHOICE: vex_choice(choice); break;
    case CB_WEAPON_CHOICE: weapon_choice(choice); break;
    default: (void)choice; break;
    }
}
