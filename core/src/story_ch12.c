/* story_ch12.c - Chapters 1-2 of Story Mode: the glue between the legacy
 * beat spine and the VM content (owner: S2; docs/design/story/SPEC.md sec.7
 * S2 content; story_final.md sec.9-11; the scripts are core/data/story/
 * ch1/ and ch2/, the constants story_ch12.h).
 *
 * The 18 legacy beats BEAT_C1_ARRIVE .. BEAT_C2_CELLAR are still the saved
 * spine (doomo_town.story.quest); each one holds an episode or two, and
 * quest_step is the stage inside it. This file answers, for those beats
 * only, what story_script.c asks: who stands where (ch12_cast), what the
 * HUD says (ch12_objective), which script a talk / a walk / a door runs
 * (ch12_npc / ch12_trigger / ch12_place), what starts by itself
 * (ch12_auto), and how a test plays it (ch12_debug_*). Everything the
 * player sees and hears is in the scripts; they change the state with VM
 * ops and the CB_C12 actions below (ch12_action).
 *
 * Also the chapters' content tables: the wrongness rows W1-W16 (S1's
 * overworld_spawn.c reads them through doomo_ow_wrong_rows) and the cast's
 * idle lines (story_talk.c).
 *
 * PS1: the story group, like the rest of the plot. No function pointer
 * leaves this file (SPEC C6): scripts are started by id.
 */
#include "story_int.h"
#include "story_ch12.h"
#include "overworld_int.h"
#include "doomo/house_api.h"
#include "doomo/camfx.h"

#define ID_BONEJANGLES 5
#define ID_SANDWICHY   4
#define ID_SPRINKLE    81
#define ID_RIBBONETTA  83
#define ID_MASCOTTY    106
#define ID_CANTOR      426

static int cur(void) { return story_beat(); }
static int stage(void) { return story_step(); }
static int has(int f) { return story_flag(f); }
static void run(unsigned scr) { story_conv(scr, 0); }

int ch12_beat(int b) { return b >= BEAT_C1_ARRIVE && b <= BEAT_C2_CELLAR; }

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

/* a story possession arms the host's authored row (the @armed windows of
 * ch2/common.dsc re-arm it after a load) */
static void possess(int id)
{
    const DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY || r->status == RES_DEAD || r->status == RES_POSSESSED) return;
    exorcism_possess(id, story_possess_row_of(id));
    portrait_invalidate(id);
}

static int clues(void)
{
    int k, n = 0, c = story_stry()->clues;
    for (k = 0; k < 5; k++) n += (c >> k) & 1;
    return n;
}

static int clue(int k) { return (story_stry()->clues >> k) & 1; }

static int met(int bit) { return (stage() >> bit) & 1; }

/* Rivet's first weapon (Ep 2.6), also the debug jumps' */
void ch12_give_weapon(int brutal)
{
    int i, slot = -1;
    DoomoWeapon w;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (!(doomo_town.weapons[i].flags & WF_VALID)) { slot = i; break; }
    if (slot < 0) slot = DOOMO_MAX_WEAPONS - 1;
    memset(&w, 0, sizeof(w));
    weld_default(&w, brutal ? WFRAME_PUMP_SHOTGUN : WFRAME_BUBBLE_WAND, brutal ? WSTYLE_BRUTAL : WSTYLE_CUTE);
    w.quality = 72;
    w.charm = brutal ? WCHARM_SKULL : WCHARM_HEART;
    w.flags = WF_VALID | WF_FAVORITE;
    doomo_town.weapons[slot] = w;
    doomo_town.story.equipped = (uint8_t)slot;
    progress_add(PROG_WEAPONS_WELDED, 1);
    weld_gfx_invalidate();
}

/* ======================================================================
 * the CB_C12 actions (a script's `native CB_C12 C12A_X [P]`)
 * ==================================================================== */
void ch12_action(int arg)
{
    int a = arg & 0xFF, p = (arg >> 8) & 0xFF;
    DoomoStry *st = story_stry();
    switch (a) {
    case C12A_STEP:
        story_set_step(p);
        ow_refresh_cast();
        story_objective_refresh();
        break;
    case C12A_BIT:
        story_set_step(stage() | (1 << (p & 7)));
        if (cur() == BEAT_C1_FRIENDS && met(C12_MET_STEAMY) && met(C12_MET_HATTIE) && met(C12_MET_GRAMPY))
            run(SCR_E14_DONE);
        story_objective_refresh();
        break;
    case C12A_TIME_AT:
        if (doomo_town.minute < (unsigned)p * 60u) doomo_town.minute = (uint16_t)(p * 60);
        break;
    case C12A_WEAPON:
        story_set_flag(SF_WEAPON_CHOSEN, 1);
        if (p) {
            story_set_flag(SF_WEAPON_BRUTAL, 1);
            st->branch = (uint8_t)(st->branch | 0x80);
            ch12_give_weapon(1);
            unlock_grant(USTORY_BRUTAL_START);
        } else {
            ch12_give_weapon(0);
            unlock_grant(USTORY_CUTE_START);
        }
        break;
    case C12A_ODDS: vm_set_var(1, exorcism_odds(ID_STEAMY)); break;
    case C12A_SOUL_LOST: {
        DoomoResident *r = town_res(ID_SPRINKLE);
        if (r && r->status == RES_POSSESSED) exorcism_apply(ID_SPRINKLE, 0);   /* her row is a scripted fail */
        if (r && r->status == RES_POSSESSED) r->status = RES_SOUL_LOST;      /* (no armed row: still lost) */
        portrait_invalidate(ID_SPRINKLE);
        ow_refresh_cast();
        break; }
    case C12A_CLUE:
        st->clues = (uint8_t)(st->clues | (1u << (p & 7)));
        vm_set_var(2, clues());
        story_objective_refresh();
        break;
    case C12A_CLUES: vm_set_var(2, clues()); break;
    case C12A_NIGHT: vm_set_var(2, town_is_night()); break;
    case C12A_PET_FORM: st->pet_form = (uint8_t)(st->pet_form | p); break;
    case C12A_REFRESH: ow_refresh_cast(); break;
    case C12A_HOME_DOOR: ow_enter_map(MAPID_HAHALAND, 4, 7); break;
    case C12A_EP_DONE: story_episode_set_done(p, 1); break;
    case C12A_SLEEP:
        doomo_town.day++;
        doomo_town.minute = 8 * 60;
        break;
    case C12A_MASCOTTY:
        ownpc_act(ID_MASCOTTY, NACT_THAW, 0);
        ownpc_act(ID_MASCOTTY, NACT_HOP, 0);
        ownpc_act(ID_MASCOTTY, NACT_EMOTE, EMO_HEART);
        break;
    case C12A_PUFF: {
        DoomoResident *pl = town_res(DOOMO_PLAYER_ID);
        if (pl) pl->stat[STAT_CORRUPTION] = (uint8_t)DOOMO_MIN(100, pl->stat[STAT_CORRUPTION] + 15);
        st->branch = (uint8_t)(st->branch | 0x40);
        break; }
    default: break;
    }
}

/* ======================================================================
 * talking
 * ==================================================================== */
/* who says what (first match wins). cond: N_STAGE stage == arg, N_FIRST
 * neighbour bit arg not met yet, N_CLUE the clue hunt and clue arg not
 * found, N_HUNT the clue hunt, N_POSS Steamy possessed (arg 1: stage 0) */
enum { N_ANY, N_STAGE, N_FIRST, N_CLUE, N_HUNT, N_POSS };
typedef struct { int16_t id; uint16_t scr; uint8_t beat, cond, arg, flavour; } NpcRow;
static const NpcRow npc_rows[] = {
    { ID_MAYOR, SCR_E11_MAYOR, BEAT_C1_MAYOR, N_ANY, 0, 0 },
    { ID_NANA, SCR_E12_NANA, BEAT_C1_KEY, N_ANY, 0, 0 },
    { ID_MAYOR, SCR_E11_MAYOR_AGAIN, BEAT_C1_KEY, N_ANY, 0, 1 },
    { ID_NANA, SCR_E12_NANA_AGAIN, BEAT_C1_HOME, N_ANY, 0, 1 },
    { ID_PIP, SCR_E13_PIP, BEAT_C1_PIP, N_ANY, 0, 0 },
    { ID_PIP, SCR_E13_PIP_WAIT, BEAT_C1_BUNNY, N_ANY, 0, 1 },
    { ID_PIP, SCR_E13_PIP_RETURN, BEAT_C1_BUNNY_RETURN, N_ANY, 0, 0 },
    { ID_STEAMY, SCR_E14_STEAMY, BEAT_C1_FRIENDS, N_FIRST, C12_MET_STEAMY, 0 },
    { ID_STEAMY, SCR_E14_STEAMY_AGAIN, BEAT_C1_FRIENDS, N_ANY, 0, 1 },
    { ID_HATTIE, SCR_E14_HATTIE, BEAT_C1_FRIENDS, N_FIRST, C12_MET_HATTIE, 0 },
    { ID_HATTIE, SCR_E14_HATTIE_AGAIN, BEAT_C1_FRIENDS, N_ANY, 0, 1 },
    { ID_GRAMPY, SCR_E14_GRAMPY, BEAT_C1_FRIENDS, N_FIRST, C12_MET_GRAMPY, 0 },
    { ID_GRAMPY, SCR_E14_GRAMPY_AGAIN, BEAT_C1_FRIENDS, N_ANY, 0, 1 },
    { ID_BONEJANGLES, SCR_E14_BONEJANGLES, BEAT_C1_FRIENDS, N_ANY, 0, 1 },
    { ID_LULLA, SCR_E14_LULLA, BEAT_C1_FRIENDS, N_ANY, 0, 1 },
    { ID_NANA, SCR_E15_NANA, BEAT_C1_CUPCAKE, N_STAGE, 0, 0 },
    { ID_LULLA, SCR_E15_LULLA, BEAT_C1_CUPCAKE, N_STAGE, 1, 0 },
    { ID_VEX, SCR_E21_VEX, BEAT_C2_VAPOR, N_ANY, 0, 0 },
    { ID_RIBBONETTA, SCR_E21_RIBBON, BEAT_C2_VAPOR, N_ANY, 0, 1 },
    { ID_SPRINKLE, SCR_E21_SPRINKLE, BEAT_C2_VAPOR, N_ANY, 0, 1 },
    { ID_HAZE, SCR_E21_HAZE, BEAT_C2_VAPOR, N_ANY, 0, 1 },
    { ID_BLOOM, SCR_E22_BLOOM, BEAT_C2_WEIRD, N_STAGE, 0, 0 },
    { ID_BLOOM, SCR_E22_REPORT, BEAT_C2_WEIRD, N_STAGE, 2, 0 },
    { ID_BLOOM, SCR_E22_BLOOM_WAIT, BEAT_C2_WEIRD, N_ANY, 0, 1 },
    { ID_SANDWICHY, SCR_E22_TASTE, BEAT_C2_WEIRD, N_CLUE, 0, 0 },
    { ID_RIBBONETTA, SCR_E22_WE, BEAT_C2_WEIRD, N_CLUE, 1, 0 },
    { ID_BONEJANGLES, SCR_E22_SHADOW, BEAT_C2_WEIRD, N_CLUE, 2, 0 },
    { ID_CANTOR, SCR_E22_BELLS, BEAT_C2_WEIRD, N_CLUE, 3, 0 },
    { ID_LULLA, SCR_E22_DRAWINGS, BEAT_C2_WEIRD, N_CLUE, 4, 0 },
    { ID_HATTIE, SCR_E22_HATTIE, BEAT_C2_WEIRD, N_HUNT, 0, 1 },
    { ID_HAZE, SCR_E21_HAZE, BEAT_C2_WEIRD, N_ANY, 0, 1 },
    { ID_STEAMY, SCR_E24_SCREAM, BEAT_C2_SCREAM, N_ANY, 0, 0 },
    { ID_FUZZ, SCR_E24_FUZZ, BEAT_C2_FUZZ, N_ANY, 0, 0 },
    { ID_STEAMY, SCR_E24_STEAMY_WAIT, BEAT_C2_FUZZ, N_ANY, 0, 1 },
    { ID_FUZZ, SCR_E25_RITE, BEAT_C2_EXORCISE, N_POSS, 1, 0 },
    { ID_STEAMY, SCR_E25_RITE, BEAT_C2_EXORCISE, N_POSS, 1, 0 },
    { ID_FUZZ, SCR_E25_STEAMY, BEAT_C2_EXORCISE, N_POSS, 0, 0 },
    { ID_STEAMY, SCR_E25_STEAMY, BEAT_C2_EXORCISE, N_POSS, 0, 0 },
    { ID_RIVET, SCR_E26_RIVET, BEAT_C2_RIVET, N_STAGE, 0, 0 },
    { ID_RIVET, SCR_E26_RIVET_AGAIN, BEAT_C2_RIVET, N_ANY, 0, 1 },
};

/* plot: only the plot's rows (a door asks for its keeper's plot line) */
static int talk(int id, int plot)
{
    int b = cur(), s = stage(), i;
    if (b == BEAT_C1_ARRIVE && id == ID_MAYOR) {
        story_set_beat(BEAT_C1_MAYOR);
        b = BEAT_C1_MAYOR;
    }
    for (i = 0; i < DOOMO_COUNTOF(npc_rows); i++) {
        const NpcRow *r = &npc_rows[i];
        int ok;
        if (r->beat != b || r->id != id) continue;
        switch (r->cond) {
        case N_STAGE: ok = s == r->arg; break;
        case N_FIRST: ok = !met(r->arg); break;
        case N_CLUE: ok = s == 1 && !clue(r->arg); break;
        case N_HUNT: ok = s == 1; break;
        case N_POSS: ok = possessed(ID_STEAMY) && (r->arg ? s == 0 : 1); break;
        default: ok = 1; break;
        }
        if (!ok || (plot && r->flavour)) continue;
        if (b == BEAT_C2_SCREAM || b == BEAT_C2_EXORCISE) story_fmt_target = ID_STEAMY;
        if (r->flavour && side_npc(id)) return 1;      /* a side quest's business comes first */
        run(r->scr);
        return 1;
    }
    return 0;
}

int ch12_npc(int id) { return talk(id, 0); }

/* ======================================================================
 * walk-on events
 * ==================================================================== */
int ch12_trigger(int tag)
{
    int b = cur();
    switch (tag) {
    case STORY_TAG_EVENT_BEACH:
        if (b == BEAT_C1_BUNNY && !has(SF_BUNNY_FOUND)) { run(SCR_E13_FLOPS); return 1; }
        break;
    case STORY_TAG_EVENT_FOG:
        if (b == BEAT_C1_WALL && stage() == 1) { run(SCR_E16_FOG); return 1; }
        if (b < BEAT_C1_WALL && !has(SF_WALL_HINT)) { run(SCR_RG_WALL); return 1; }
        break;
    case STORY_TAG_EVENT_CELLAR:
        if (b == BEAT_C2_CELLAR) { run(SCR_E27_HATCH); return 1; }
        if (b >= BEAT_C2_VAPOR) { run(SCR_E27_HATCH_LOCKED); return 1; }
        break;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * doors
 * ==================================================================== */
int ch12_place(int place)
{
    int b = cur(), s = stage();
    switch (place) {
    case PLACE_HOME:
        if (b < BEAT_C1_HOME) { run(SCR_E12_LOCKED); return 1; }
        if (b == BEAT_C1_HOME) { run(SCR_E12_COTTAGE); return 1; }
        if (b == BEAT_C1_HATCH && !pet_exists()) { run(SCR_E13_HATCH); return 1; }
        if (b == BEAT_C1_CUPCAKE && s) { run(s == 1 ? SCR_E15_EARLY : SCR_E15_PARTY); return 1; }
        if (b == BEAT_C2_WEIRD && s >= 3 && s <= 5) { run(s == 3 ? SCR_E23_NIGHT1 : s == 4 ? SCR_E23_NIGHT2 : SCR_E23_NIGHT3); return 1; }
        if (b == BEAT_C2_RIVET && s == 1) { run(SCR_E26_SLEEP); return 1; }
        return 0;
    /* a door is its keeper's plot line, if they have one now */
    case PLACE_CAFE: return talk(ID_NANA, 1);
    case PLACE_CHURCH: return talk(ID_FUZZ, 1) || talk(ID_CANTOR, 1);
    case PLACE_HOSPITAL: return talk(ID_BLOOM, 1);
    case PLACE_WORKSHOP: return talk(ID_RIVET, 1);
    case PLACE_CONCERT_HALL: return talk(ID_HATTIE, 1);
    case PLACE_VAPE_SHOP:
        if (b == BEAT_C2_VAPOR) return talk(ID_VEX, 1);
        run(b < BEAT_C2_VAPOR ? SCR_RG_VAPE : SCR_E21_VEX_CLOSED);
        return 1;
    case PLACE_WALL_GATE: run(SCR_RG_GATE); return 1;
    case PLACE_GRAVEYARD:
        if (has(SF_VISITED_GRAVE)) return 0;
        story_set_flag(SF_WRONG(15), 1);                  /* W16: noticed */
        run(SCR_C1_GRAVE);
        return 1;
    default: return 0;
    }
}

/* ======================================================================
 * what starts by itself (SCENE_STORY idle, nothing talking or pending)
 * ==================================================================== */
int ch12_auto(void)
{
    int b = cur(), s = stage();
    if (b == BEAT_C1_ARRIVE && !has(SF_TITLE_CH1) && !story_episode_done(BEAT_P0)) {
        story_push_cutscene(CUT_P0, CTX_BEAT, BEAT_C1_ARRIVE);   /* the crayon prologue */
        return 1;
    }
    if (b == BEAT_C1_HATCH && s == 0 && !pet_exists()) { story_set_step(1); run(SCR_E13_HATCH); return 1; }
    if (b == BEAT_C1_WALL && s == 0) { run(SCR_E16_THUMP); return 1; }
    if (b == BEAT_C1_WALL && s == 2) { story_set_step(3); run(SCR_I1_NIGHT); return 1; }
    if (b == BEAT_C2_VAPOR && s == 0 && has(SF_TITLE_CH2)) { story_set_step(1); run(SCR_E21_MORNINGS); return 1; }
    return 0;
}

/* ======================================================================
 * entering a beat
 * ==================================================================== */
void ch12_enter_beat(int b)
{
    DoomoStry *st = story_stry();
    switch (b) {
    case BEAT_C2_VAPOR: {
        int w;
        if (!has(SF_TITLE_CH2)) {
            story_set_flag(SF_TITLE_CH2, 1);
            story_push_cutscene(CUT_CHAPTER2, CTX_FREE, 0);
        }
        for (w = 0; w < STORY_WRONGS; w++)                 /* the Ch1 rows noticed: NG+ copy */
            if (has(SF_WRONG(w))) st->wrong[w >> 3] = (uint8_t)(st->wrong[w >> 3] | (1u << (w & 7)));
        break; }
    case BEAT_C2_SCREAM:
        possess(ID_STEAMY);                                /* TALLYWORN, row 0 */
        possess(ID_SPRINKLE);                              /* WHEEZLE, row 1 (a scripted fail) */
        if (home_phase() < HP_HAUNTED) home_set_phase(HP_HAUNTED);
        break;
    default:
        break;
    }
}

/* results of the pushes the glue made itself (the VM's come back to it) */
int ch12_resume(const StoryPending *p, int result)
{
    (void)result;
    if (p->kind != PEND_CUTSCENE) return 0;
    switch (p->arg) {
    case CUT_P0:
        story_episode_set_done(BEAT_P0, 1);
        return 1;
    case CUT_CHAPTER1:
        if (cur() == BEAT_C1_ARRIVE) story_push_cutscene(CUT_ARRIVAL, CTX_BEAT, BEAT_C1_ARRIVE);
        return 1;
    case CUT_ARRIVAL:
        if (cur() == BEAT_C1_ARRIVE) story_set_beat(BEAT_C1_MAYOR);
        return 1;
    case CUT_CHAPTER2:
        return 1;                                          /* ch12_auto starts the mornings */
    default:
        return 0;
    }
}

/* ======================================================================
 * the HUD objective
 * ==================================================================== */
static const char *obj(unsigned tid, int n0)
{
    static char buf[72];
    static uint16_t last = 0xFFFFu;
    static int16_t ln = -1;
    if (last != tid || ln != n0) {
        char raw[72];
        int n[1];
        last = (uint16_t)tid;
        ln = (int16_t)n0;
        n[0] = n0;
        if (txt_get((uint16_t)tid, raw, (int)sizeof(raw)) < 0) raw[0] = 0;
        vm_fmt_n(buf, (int)sizeof(buf), raw, n, 1);
    }
    return buf;
}

static const uint16_t obj_ids[BEAT_C2_CELLAR + 1] = {      /* stage 0 of each beat */
    TXT_OBJ_C1_MAYOR, TXT_OBJ_C1_MAYOR, TXT_OBJ_C1_KEY, TXT_OBJ_C1_HOME, TXT_OBJ_C1_HATCH, TXT_OBJ_C1_PIP,
    TXT_OBJ_C1_BUNNY, TXT_OBJ_C1_BUNNY_RETURN, TXT_OBJ_C1_FRIENDS, TXT_OBJ_C1_FRIENDS_DONE, TXT_OBJ_C1_HOME_NIGHT,
    TXT_OBJ_C2_VAPOR, TXT_OBJ_C2_BLOOM, TXT_OBJ_C2_SCREAM, TXT_OBJ_C2_FUZZ, TXT_OBJ_C2_EXORCISE,
    TXT_OBJ_C2_RIVET, TXT_OBJ_C2_CELLAR
};
static const uint16_t obj_weird[5] = {                       /* Ep 2.2-2.3, stages 1-5 */
    TXT_OBJ_C2_CLUES, TXT_OBJ_C2_REPORT, TXT_OBJ_C2_NIGHT, TXT_OBJ_C2_NIGHT2, TXT_OBJ_C2_NIGHT3
};

const char *ch12_objective(void)
{
    int b = cur(), s = stage(), n0 = 0;
    unsigned tid;
    if (!ch12_beat(b)) return "";
    tid = obj_ids[b];
    if (b == BEAT_C1_FRIENDS) n0 = met(C12_MET_STEAMY) + met(C12_MET_HATTIE) + met(C12_MET_GRAMPY);
    if (b == BEAT_C1_CUPCAKE && s) tid = s == 1 ? TXT_OBJ_C1_CUPCAKE_LULLA : TXT_OBJ_C1_PARTY;
    if (b == BEAT_C1_WALL && s == 1) tid = TXT_OBJ_C1_WALL;
    if (b == BEAT_C2_WEIRD && s) {
        tid = obj_weird[DOOMO_MIN(s, 5) - 1];
        if (s == 1) n0 = clues();
    }
    if (b == BEAT_C2_RIVET && s) tid = TXT_OBJ_C2_REST;
    return obj(tid, n0);
}

/* ======================================================================
 * targets and the cast
 * ==================================================================== */
typedef struct { int16_t id; uint8_t x, y; } Spot;
#define COTTAGE_X 4
#define COTTAGE_Y 7


static const Spot targets[BEAT_C2_CELLAR + 1] = {           /* stage 0 of each beat */
    { ID_MAYOR, 33, 25 }, { ID_MAYOR, 33, 25 }, { ID_NANA, 25, 17 }, { -1, COTTAGE_X, COTTAGE_Y },
    { -1, COTTAGE_X, COTTAGE_Y }, { ID_PIP, 10, 47 }, { -1, 12, 55 }, { ID_PIP, 10, 47 },
    { ID_STEAMY, 27, 43 }, { ID_NANA, 25, 17 }, { -1, COTTAGE_X, COTTAGE_Y },
    { ID_VEX, 54, 37 }, { ID_BLOOM, 17, 23 }, { ID_STEAMY, 38, 24 }, { ID_FUZZ, 7, 28 },
    { ID_FUZZ, 36, 24 }, { ID_RIVET, 53, 10 }, { -1, 53, 37 }
};
static const Spot neighbours[4] = {                          /* Ep 1.4, in order; then Nana */
    { ID_STEAMY, 27, 43 }, { ID_HATTIE, 27, 35 }, { ID_GRAMPY, 40, 47 }, { ID_NANA, 25, 17 }
};
static const Spot clue_spots[5] = {                          /* Ep 2.2: the clue each gives */
    { ID_SANDWICHY, 27, 17 }, { ID_RIBBONETTA, 51, 38 }, { ID_BONEJANGLES, 36, 22 }, { ID_CANTOR, 9, 27 },
    { ID_LULLA, 30, 42 }
};

static Spot target_of(int b)
{
    static const Spot none = { -1, 0xFF, 0xFF }, home = { -1, COTTAGE_X, COTTAGE_Y };
    static const Spot lulla = { ID_LULLA, 30, 42 }, wall = { -1, 55, 24 };
    static const uint8_t clue_order[5] = { 0, 4, 1, 2, 3 };
    int s = stage(), k;
    if (!ch12_beat(b)) return none;
    switch (b) {
    case BEAT_C1_FRIENDS:
        for (k = 0; k < 3 && met(k); k++) {}
        return neighbours[k];
    case BEAT_C1_CUPCAKE: return s == 0 ? targets[b] : s == 1 ? lulla : home;
    case BEAT_C1_WALL: return s == 1 ? wall : home;
    case BEAT_C2_WEIRD:
        if (s == 1) {
            for (k = 0; k < 4 && clue(clue_order[k]); k++) {}
            return clue_spots[clue_order[k]];
        }
        return s <= 2 ? targets[b] : home;
    case BEAT_C2_RIVET: return s == 0 ? targets[b] : home;
    default: return targets[b];
    }
}

int ch12_target(int *map_id, int *cx, int *cy)
{
    Spot t = target_of(cur());
    if (map_id) *map_id = t.x == 0xFF ? -1 : MAPID_HAHALAND;
    if (cx) *cx = t.x;
    if (cy) *cy = t.y;
    return t.id;
}

/* who stands where, from beat `from` to beat `to` (CR_HUNT: only during
 * Ep 2.2's clue hunt; CR_POSSESSED: only while possessed) */
enum { CR_HUNT = 1, CR_POSSESSED = 2 };
typedef struct { int16_t id; uint8_t x, y, from, to, cond; } CastRow;
static const CastRow cast_rows[] = {
    { ID_MAYOR, 33, 25, BEAT_C1_ARRIVE, BEAT_C1_MAYOR, 0 },      /* at the fountain on day one */
    { ID_MAYOR, 29, 8, BEAT_C1_KEY, BEAT_C2_WEIRD, 0 },
    { ID_MAYOR, 35, 25, BEAT_C2_SCREAM, BEAT_C2_EXORCISE, 0 },   /* in the plaza for the scream */
    { ID_MAYOR, 29, 8, BEAT_C2_RIVET, BEAT_C2_CELLAR, 0 },
    { ID_NANA, 25, 17, BEAT_C1_ARRIVE, BEAT_C2_CELLAR, 0 },
    { ID_PIP, 10, 47, BEAT_C1_ARRIVE, BEAT_C2_CELLAR, 0 },
    { ID_LULLA, 30, 42, BEAT_C1_ARRIVE, BEAT_C2_CELLAR, 0 },
    { ID_FUZZ, 7, 28, BEAT_C1_ARRIVE, BEAT_C2_FUZZ, 0 },
    { ID_FUZZ, 36, 24, BEAT_C2_EXORCISE, BEAT_C2_EXORCISE, 0 },
    { ID_FUZZ, 7, 28, BEAT_C2_RIVET, BEAT_C2_CELLAR, 0 },
    { ID_BLOOM, 17, 23, BEAT_C1_ARRIVE, BEAT_C2_CELLAR, 0 },
    { ID_STEAMY, 27, 43, BEAT_C1_FRIENDS, BEAT_C2_WEIRD, 0 },    /* the three neighbours from Ep 1.4 */
    { ID_STEAMY, 38, 24, BEAT_C2_SCREAM, BEAT_C2_EXORCISE, 0 },  /* in the plaza while possessed */
    { ID_STEAMY, 27, 43, BEAT_C2_RIVET, BEAT_C2_CELLAR, 0 },
    { ID_HATTIE, 27, 35, BEAT_C1_FRIENDS, BEAT_C2_CELLAR, 0 },
    { ID_GRAMPY, 40, 47, BEAT_C1_FRIENDS, BEAT_C2_CELLAR, 0 },
    { ID_BONEJANGLES, 36, 22, BEAT_C1_FRIENDS, BEAT_C2_CELLAR, 0 },
    { ID_MASCOTTY, 40, 27, BEAT_C1_ARRIVE, BEAT_C2_CELLAR, 0 },  /* the mascot who does not bob (W4) */
    { ID_RIVET, 53, 10, BEAT_C2_VAPOR, BEAT_C2_CELLAR, 0 },
    { ID_HAZE, 55, 36, BEAT_C2_VAPOR, BEAT_C2_CELLAR, 0 },
    { ID_VEX, 54, 37, BEAT_C2_VAPOR, BEAT_C2_RIVET, 0 },
    { ID_RIBBONETTA, 51, 38, BEAT_C2_VAPOR, BEAT_C2_VAPOR, 0 },
    { ID_RIBBONETTA, 51, 38, BEAT_C2_WEIRD, BEAT_C2_WEIRD, CR_HUNT },
    { ID_SPRINKLE, 53, 38, BEAT_C2_VAPOR, BEAT_C2_WEIRD, 0 },
    { ID_SPRINKLE, 40, 25, BEAT_C2_SCREAM, BEAT_C2_EXORCISE, CR_POSSESSED },
    { ID_SANDWICHY, 27, 17, BEAT_C2_WEIRD, BEAT_C2_WEIRD, CR_HUNT },
    { ID_CANTOR, 9, 27, BEAT_C2_WEIRD, BEAT_C2_WEIRD, CR_HUNT },
};

int ch12_cast(int map, StoryNpcSpot *out, int max)
{
    int b = cur(), i, n = 0, hunt = b == BEAT_C2_WEIRD && stage() == 1;
    Spot t = target_of(b);
    if (map != MAPID_HAHALAND) return 0;
    for (i = 0; i < DOOMO_COUNTOF(cast_rows) && n < max; i++) {
        const CastRow *c = &cast_rows[i];
        if (b < c->from || b > c->to || !alive(c->id)) continue;
        if ((c->cond & CR_HUNT) && !hunt) continue;
        if ((c->cond & CR_POSSESSED) && !possessed(c->id)) continue;
        out[n].id = c->id;
        out[n].x = c->x;
        out[n].y = c->y;
        out[n].dir = 0;
        out[n].flags = (uint8_t)(t.id == c->id && c->id != ID_MASCOTTY ? NPCF_MAIN | NPCF_FIXED : NPCF_FIXED);
        n++;
    }
    return n;
}

/* ======================================================================
 * debug: set up a beat (on top of story_script.c's cumulative setup) and
 * play it like a player (autowin answers every menu with its first option)
 * ==================================================================== */
void ch12_debug_setup(int b)
{
    DoomoStry *st = story_stry();
    if (b > BEAT_C1_ARRIVE) story_episode_set_done(BEAT_P0, 1);
    if (b > BEAT_C1_WALL) {
        story_set_flag(SF_PET_NUBS, 1);
        st->pet_form = (uint8_t)(st->pet_form | 1);
    }
    if (b >= BEAT_C2_SCREAM) { if (home_phase() < HP_HAUNTED) home_set_phase(HP_HAUNTED); }
    if (b == BEAT_C2_SCREAM || b == BEAT_C2_FUZZ || b == BEAT_C2_EXORCISE) {
        possess(ID_STEAMY);
        possess(ID_SPRINKLE);
    }
    if (b > BEAT_C2_EXORCISE) {
        DoomoResident *r = town_res(ID_SPRINKLE);
        if (r && (r->status == RES_POSSESSED || r->status == RES_ALIVE)) r->status = RES_SOUL_LOST;
        story_set_flag(SF_BOTTLE_SEEN, 1);
    }
    if (b > BEAT_C2_WEIRD) st->clues = (uint8_t)(st->clues | 0x13);
}

/* the stage a new episode id starts at inside its legacy beat */
void ch12_debug_episode(int ep)
{
    switch (ep) {
    case BEAT_EP2_3: story_set_step(3); story_stry()->clues |= 0x13; break;
    case BEAT_D1: story_set_step(1); break;
    case BEAT_INT1: story_set_step(2); break;
    case BEAT_F1: story_set_step(1); break;
    default: break;
    }
    story_objective_refresh();
}

int ch12_debug_step(void)
{
    int b = cur(), s = stage();
    Spot t = target_of(b);
    if (!ch12_beat(b)) return 0;
    if (b == BEAT_C1_ARRIVE) story_set_beat(BEAT_C1_MAYOR);
    else if (b == BEAT_C1_BUNNY) story_trigger_ow(STORY_TAG_EVENT_BEACH);
    else if (b == BEAT_C1_WALL && s != 1) { if (s) story_set_step(3); run(s ? SCR_I1_NIGHT : SCR_E16_THUMP); }
    else if (b == BEAT_C1_WALL) story_trigger_ow(STORY_TAG_EVENT_FOG);
    else if (b == BEAT_C2_VAPOR && s == 0) { story_set_step(1); run(SCR_E21_MORNINGS); }
    else if (b == BEAT_C2_CELLAR) story_trigger_ow(STORY_TAG_EVENT_CELLAR);
    else if (t.id >= 0) story_interact_npc(t.id);           /* talk to whoever the HUD points at */
    else story_enter_place(PLACE_HOME);                     /* or go home */
    return 1;
}

/* the episode pack of a beat (the PS1 prefetch at SCENE_STORY's enter) */
int ch12_pack(int b)
{
    static const uint8_t packs[BEAT_C2_CELLAR + 1] = {
        PACK_EP11, PACK_EP11, PACK_EP12, PACK_EP12, PACK_EP13, PACK_EP13, PACK_EP13, PACK_EP13,
        PACK_EP14, PACK_EP15, PACK_EP16,
        PACK_EP21, PACK_EP22, PACK_EP24, PACK_EP24, PACK_EP25, PACK_EP26, PACK_EP27
    };
    if (!ch12_beat(b)) return -1;
    if (b == BEAT_C2_WEIRD && stage() >= 3) return PACK_EP23;
    return packs[b];
}

/* ======================================================================
 * the content tables
 * ==================================================================== */
/* The wrongness rows (story_final.md sec.6). map 0xFF ends the table.
 * Chapter masks: bit c = chapter c. W15 is the house's (the chair one
 * cell off, seen through your window); W16 is the joke grave (the
 * graveyard door, ch12_place). */
static const OwWrongRow wrong_rows[] = {
    /* w   map             x   y  r  chapters flags      delay     sfx         pal         caption        script */
    {  0, MAPID_HAHALAND, 34, 20, 3, 0x06, 0,          RUNG_R8, SFX_BUBBLE, 0,          TXT_W1_CAP,  SCR_W1_AGAIN },
    {  1, MAPID_HAHALAND, 36, 27, 1, 0x02, 0,          0,       SFX_TINK,   0,          TXT_W2_CAP,  0 },
    {  2, MAPID_HAHALAND, 40, 45, 2, 0x02, OWW_NIGHT,  0,       0,          0,          TXT_W3_CAP,  SCR_W3_PICK },
    {  3, MAPID_HAHALAND, 40, 27, 2, 0x06, OWW_STILL,  0,       0,          0,          TXT_W4_CAP,  SCR_W4_WAVE },
    {  4, MAPID_HAHALAND, 45, 26, 2, 0x02, 0,          0,       SFX_CREAK,  0,          TXT_W5_CAP,  0 },
    {  5, MAPID_HAHALAND, 54, 20, 3, 0x02, OWW_NIGHT,  0,       SFX_THUD,   0,          TXT_W6_CAP,  0 },
    {  6, MAPID_HAHALAND, 51, 38, 2, 0x04, 0,          RUNG_R5, 0,          0,          TXT_W7_CAP,  SCR_W7_WE },
    {  7, MAPID_HAHALAND, 30, 22, 2, 0x04, OWW_DAY,    RUNG_R2, 0,          0,          TXT_W8_CAP,  0 },
    {  8, MAPID_HAHALAND, 24, 18, 2, 0x04, 0,          0,       SFX_SPARKLE, 0,         TXT_W9_CAP,  0 },
    {  9, MAPID_HAHALAND, 27, 42, 1, 0x04, OWW_STILL,  RUNG_R5, 0,          0,          TXT_W10_CAP, SCR_W10_WAVE },
    { 10, MAPID_HAHALAND,  7, 27, 3, 0x04, OWW_NIGHT,  0,       SFX_BELL,   0,          TXT_W11_CAP, SCR_W11_BELL },
    { 11, MAPID_HAHALAND, 30, 41, 2, 0x04, OWW_DAY,    0,       0,          0,          TXT_W12_CAP, 0 },
    { 12, MAPID_HAHALAND, 34, 20, 3, 0x04, 0,          0,       SFX_BEEP,   PAL_LILAC,  TXT_W13_CAP, 0 },
    { 13, MAPID_HAHALAND, 25, 16, 1, 0x04, 0,          0,       0,          0,          TXT_W14_CAP, SCR_W14_JUKEBOX },
    { 14, MAPID_HAHALAND,  4,  7, 1, 0x04, OWW_NIGHT,  RUNG_R8, SFX_CREAK,  0,          TXT_W15_CAP, 0 },
    { 0, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, TXT_NONE, 0 }
};
const OwWrongRow *const doomo_ow_wrong_rows = wrong_rows;

/* the cast's idle lines in Chapters 1-2 (text ids in the ch1 / ch2 packs) */
typedef struct { int16_t id; uint8_t ch; uint16_t text; } IdleLine;
static const IdleLine idle_lines[] = {
    { ID_MAYOR, 1, TXT_IDLE_MAYOR_1A }, { ID_MAYOR, 1, TXT_IDLE_MAYOR_1B },
    { ID_NANA, 1, TXT_IDLE_NANA_1A }, { ID_NANA, 1, TXT_IDLE_NANA_1B },
    { ID_PIP, 1, TXT_IDLE_PIP_1A }, { ID_PIP, 1, TXT_IDLE_PIP_1B },
    { ID_FUZZ, 1, TXT_IDLE_FUZZ_1A }, { ID_BLOOM, 1, TXT_IDLE_BLOOM_1A },
    { ID_LULLA, 1, TXT_IDLE_LULLA_1A }, { ID_STEAMY, 1, TXT_IDLE_STEAMY_1A },
    { ID_HATTIE, 1, TXT_IDLE_HATTIE_1A }, { ID_GRAMPY, 1, TXT_IDLE_GRAMPY_1A },
    { ID_MASCOTTY, 1, TXT_IDLE_MASCOTTY_1A }, { ID_MASCOTTY, 2, TXT_IDLE_MASCOTTY_1A },
    { ID_MAYOR, 2, TXT_IDLE_MAYOR_2A }, { ID_MAYOR, 2, TXT_IDLE_MAYOR_2B },
    { ID_NANA, 2, TXT_IDLE_NANA_2A }, { ID_NANA, 2, TXT_IDLE_NANA_2B },
    { ID_PIP, 2, TXT_IDLE_PIP_2A }, { ID_FUZZ, 2, TXT_IDLE_FUZZ_2A },
    { ID_RIVET, 2, TXT_IDLE_RIVET_2A }, { ID_BLOOM, 2, TXT_IDLE_BLOOM_2A },
    { ID_VEX, 2, TXT_IDLE_VEX_2A }, { ID_LULLA, 2, TXT_IDLE_LULLA_2A },
    { ID_STEAMY, 2, TXT_IDLE_STEAMY_2A }, { ID_HAZE, 2, TXT_IDLE_HAZE_2A },
    { ID_HATTIE, 2, TXT_IDLE_HATTIE_2A }, { ID_GRAMPY, 2, TXT_IDLE_GRAMPY_2A },
};

/* An idle line of the cast in Chapters 1-2, decoded and formatted into out;
 * 0 if `id` has none in this chapter. */
int ch12_idle(int id, int chapter, char *out, int cap)
{
    uint16_t found[4];
    int i, n = 0;
    char raw[200];
    if (chapter < 1 || chapter > 2 || !out || cap <= 0) return 0;
    for (i = 0; i < DOOMO_COUNTOF(idle_lines) && n < 4; i++)
        if (idle_lines[i].id == id && idle_lines[i].ch == chapter) found[n++] = idle_lines[i].text;
    if (!n || txt_get(found[(doomo_town.minute / 7 + doomo_town.day) % (unsigned)n], raw, (int)sizeof(raw)) < 0) return 0;
    vm_fmt(out, cap, raw);
    return 1;
}
