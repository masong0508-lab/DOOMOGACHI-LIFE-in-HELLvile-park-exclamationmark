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
 * story_ch12.c first (ch12_*). What is left below is Chapters 3-4 (the
 * legacy port, S3's) and the cumulative state of the debug jumps.
 */
#include "story_int.h"
#include "story_ch12.h"       /* S2: Chapters 1-2 are VM content (story_ch12.c) */

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
    if (ch12_beat(b)) return ch12_npc(id);              /* S2: Chapters 1-2 */
    switch (b) {
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

/* What starts by itself on an idle overworld (story.c's pump). */
int script_auto(void)
{
    int b = cur();
    if (ch12_beat(b)) return ch12_auto();              /* S2: Chapters 1-2 */
    return 0;
}

/* ======================================================================
 * script_trigger: walk-on events
 * ==================================================================== */
int script_trigger(int tag)
{
    int b = cur();
    if (ch12_beat(b) && ch12_trigger(tag)) return 1;    /* S2: Chapters 1-2 */
    switch (tag) {
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
    if (ch12_beat(b) && ch12_place(place)) return 1;    /* S2: Chapters 1-2 */
    switch (place) {
    case PLACE_WORKSHOP:
        if (b == BEAT_C3_NESTS || b == BEAT_C3_SPIRE) {
            if (!has(SF_RIVET_UPGRADE)) return script_npc(ID_RIVET);
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        break;
    case PLACE_VAPE_SHOP:
        if (b == BEAT_C3_SPIRE) return script_trigger(STORY_TAG_EVENT_SPIRE);
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
void script_resume(const StoryPending *p, int result)
{
    int b = cur();
    if (ch12_resume(p, result)) return;                 /* S2: Chapters 1-2 */
    if (p->kind == PEND_CUTSCENE) {
        switch (p->arg) {
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
    case PEND_EXORCISM:
        if (p->ctx == CTX_C3_EXO) {
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
    if (ch12_beat(b)) ch12_enter_beat(b);              /* S2: Chapters 1-2 */
    switch (b) {
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

/* Progress counted by the overworld's small talk (PRISONERS, FESTIVAL; S2:
 * Ep 1.4 and Ep 2.2 replaced the FRIENDS and WEIRD counters with authored
 * neighbours and clues). */
void script_ambient_talked(int id, int new_friend)
{
    int b = cur();
    (void)id;
    (void)new_friend;
    if (b == BEAT_C4_PRISONERS) {
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
    if (ch12_beat(b)) return ch12_objective();         /* S2: Chapters 1-2 */
    switch (b) {
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
    Target t;
    if (ch12_beat(cur())) return ch12_target(map_id, cx, cy);   /* S2: Chapters 1-2 */
    t = target_of(cur());
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
    if (ch12_beat(b)) return ch12_cast(map, out, max);  /* S2: Chapters 1-2 */
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
    ch12_debug_setup(b);                               /* S2: the Ch1-2 content's own state */
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
    if (ch12_beat(b)) return ch12_debug_step();        /* S2: Chapters 1-2 */
    switch (b) {
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
    if (cb == CB_C12) { ch12_action((uint16_t)choice); return; }   /* S2: a script's native CB_C12 */
    switch (cb) {
    case CB_C3_CHECK: c3_check(); break;
    case CB_C3_EXO_CHOICE: c3_exo_choice(choice); break;
    case CB_HELP_CHECK: help_check(); break;
    case CB_MAYOR_CAME: mayor_came(); break;
    case CB_SMOG_CHOICE: smog_choice(choice); break;
    case CB_SPIRE_CHOICE: spire_choice(choice); break;
    case CB_STAGE_DONE: stage_done(); break;
    case CB_SUMMIT_DONE: summit_done(); break;
    case CB_TRAVEL_FROM_HH: travel_from_hh(choice); break;
    case CB_TRAVEL_FROM_HV: travel_from_hv(choice); break;
    case CB_TRAVEL_FROM_PK: travel_from_pk(choice); break;
    case CB_VAPULA_DOWN: vapula_down(); break;
    default: (void)choice; break;
    }
}
