/* story_ch34.c - Chapters 3-4, Interlude III, the flashbacks F2-F4, the
 * dream D2 and the epilogue on the overworld (owner: S3; SPEC sec.7 S3,
 * story_final.md sec.12-13a). The words, the timing and most of the logic
 * are VM scripts in core/data/story/ch3/, ch4/, epi/ (one pack per episode
 * run, so a running script and the HUD read one pack: the PS1 keeps one
 * episode pack resident at a time). This file is the glue: which script a
 * talk / walk / door starts in the current episode and stage, the cast,
 * the HUD objective and target, the C actions the scripts call
 * (`native CB_C34 C34A_*`), the scenes the VM cannot push (the Breaches,
 * the pet-care boss, the credits), and the debug steps.
 *
 * The spine (story_ch34.h): the legacy beats C3_ERUPTION .. DONE each hold
 * a run of episodes; the current episode is the first of the run whose
 * STRY done bit is clear; its stage is quest_step. State that earlier
 * episodes leave behind (possessions, the house phase, the pet's horns) is
 * re-derived from the done bits whenever the episode changes (sync), so a
 * debug jump to any episode lands consistent. sync never sets a done bit
 * of Chapters 3-4.
 *
 * story_script.c hands every beat from BEAT_C3_ERUPTION on to the ch34_*
 * functions (one marked hunk per script_* function).
 */
#include "story_int.h"
#include "story_ch34.h"
#include "doomo/house_api.h"
#include "doomo/unlock_ids.h"

#define NAR (-1)

/* Hell Breach levels (levels_int.h LV_*; test_story_ch34.c checks) */
enum { BR_SEWERS = 2, BR_FACTORY = 3, BR_GRAVEYARD = 4, BR_UNDERBELLY = 6 };
/* what a pushed sub-scene's result is for (StoryPending.extra, CTX_BEAT) */
enum { X_BREACH = 0x100, X_PETBOSS = 0x200, X_CUT = 0x300, X_CREDITS = 0x400 };

/* conversation callbacks of the C side (story_script.c's range 1..63) */
enum { CB_C34_PETBOSS = CB_C34_FIRST, CB_C34_HEART, CB_C34_CREDITS, CB_C34_RETRY,
       CB_C34_BREACH, CB_C34_CHECK, CB_C34_FINISH, CB_C34_TRAVEL };

/* ---- the spine ------------------------------------------------------------------ */
enum { E31, E32, E33, E34, EF3, E35, E36, E37, EF2, EI3, E41, E42, E43, E44, ED2, E45, EF4,
       E46, E47, EE1, EE2, E_COUNT };
static const uint8_t ep_beat[E_COUNT] = {
    BEAT_EP3_1, BEAT_EP3_2, BEAT_EP3_3, BEAT_EP3_4, BEAT_F3, BEAT_EP3_5, BEAT_EP3_6,
    BEAT_EP3_7, BEAT_F2, BEAT_INT3, BEAT_EP4_1, BEAT_EP4_2, BEAT_EP4_3, BEAT_EP4_4,
    BEAT_D2, BEAT_EP4_5, BEAT_F4, BEAT_EP4_6, BEAT_EP4_7, BEAT_E1, BEAT_E2
};
/* the run of episodes of each legacy beat: [lo, hi) */
static const uint8_t run_lo[BEAT_COUNT - BEAT_C3_ERUPTION] = { E31, E33, E36, E41, E42, E43, E46, E47, E47, EE1 };
static const uint8_t run_hi[BEAT_COUNT - BEAT_C3_ERUPTION] = { E33, E36, E41, E42, E43, E46, E47, EE1, EE1, E_COUNT };

static uint8_t busy;                  /* inside the glue: nothing starts by itself */
static uint8_t retry_breach = BR_SEWERS;
static int8_t pending_heart = -1;
static int8_t auto_ep = -1;           /* Interlude III runs once per session */
static uint8_t quiet;                 /* after a debug jump: nothing starts until the player acts */
static int8_t synced_ep = -2;
static uint8_t synced_beat = 0xFF;

int ch34_beat(int b) { return b >= BEAT_C3_ERUPTION && b < BEAT_COUNT; }

static int done(int e) { return story_episode_done(ep_beat[e]); }
static int cur(void) { return story_beat(); }

/* the current episode (E*), -1 before Chapter 3 */
static int ep(void)
{
    int b = cur(), e, end;
    if (!ch34_beat(b)) return -1;
    e = run_lo[b - BEAT_C3_ERUPTION];
    end = run_hi[b - BEAT_C3_ERUPTION];
    while (e < end - 1 && done(e)) e++;
    return e;
}

int ch34_episode(void)
{
    int e = ep();
    return e < 0 ? -1 : ep_beat[e];
}

/* the pack an episode's scripts and HUD lines are in */
int ch34_pack(void)
{
    static const uint8_t pk[E_COUNT] = {
        PACK_EP31, PACK_EP31, PACK_EP33, PACK_EP34, PACK_EP34, PACK_EP34, PACK_EP36, PACK_EP36,
        PACK_EP36, PACK_INT3, PACK_EP41, PACK_EP42, PACK_EP43, PACK_EP44, PACK_EP44, PACK_EP45,
        PACK_EP46, PACK_EP46, PACK_EP47, PACK_EPI, PACK_EPI
    };
    int e = ep();
    return e < 0 ? -1 : pk[e];
}

static int stage(void) { return story_step(); }
static void set_stage(int n) { story_set_step(n); story_objective_refresh(); ow_refresh_cast(); }
static int has(int f) { return story_flag(f); }
static void flag(int f) { story_set_flag(f, 1); }
static void conv(unsigned scr) { story_conv(scr, 0); }
static void conv_cb(unsigned scr, int cb) { story_conv(scr, cb); }
/* --- S4: an idle line lets a side quest's business come first (the cast's
 * side missions stay reachable in Chapters 3-5; story_ch12.c's npc rows do
 * the same with their `flavour` bit) --- */
static int idle(int id, unsigned scr)
{
    if (!side_npc(id)) conv(scr);
    return 1;
}
/* --- end S4 --- */

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

/* a story possession arms the host's authored row (the @armed windows in
 * ch3/ep31.dsc and ch4/ep41.dsc re-arm it after a load) */
static void possess(int id, int row)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status != RES_ALIVE) return;
    r->stat[STAT_CORRUPTION] = 100;
    r->status = RES_POSSESSED;
    r->exorcism = 0;
    exorcism_possess(id, row);
    portrait_invalidate(id);
}

static void unpossess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status != RES_POSSESSED) return;
    exorcism_unpossess(id);
    portrait_invalidate(id);
}

static int heart(int nest) { return (story_stry()->nests >> nest) & 1; }

static int hearts(void)
{
    int k, n = 0;
    for (k = 0; k < STORY_NESTS; k++) n += heart(k);
    return n;
}

static int nests_flagged(void)
{
    int k, n = 0;
    for (k = 0; k < STORY_NESTS; k++) n += has(SF_NEST0 + k);
    return n;
}

/* ---- derived state: what every episode before the current one left ---------------- */
static void sync(void)
{
    int e = ep(), k;
    DoomoStry *st = story_stry();
    synced_ep = (int8_t)e;
    synced_beat = (uint8_t)cur();
    if (e < 0) return;
    /* Chapters 1-2 are over */
    for (k = BEAT_P0; k < BEAT_ALL_COUNT; k++)
        if (story_episode_order(k) >= 0 && story_episode_order(k) < story_episode_order(BEAT_EP3_1))
            story_episode_set_done(k, 1);
    /* Hattie and Grampy: from the eruption until their rites */
    if (e <= E35) {
        if (!has(SF_C3_EXO0)) possess(ID_HATTIE, 2);
        if (!has(SF_C3_EXO0 + 1)) possess(ID_GRAMPY, 3);
    } else {
        flag(SF_C3_EXO0);
        flag(SF_C3_EXO0 + 1);
        unpossess(ID_HATTIE);
        unpossess(ID_GRAMPY);
    }
    if (e > E31) { flag(SF_HOUSE_PLATED); flag(SF_HOUSE_TURRET); }
    if (e > E33) {
        for (k = 0; k < STORY_NESTS; k++) { flag(SF_NEST0 + k); st->nests = (uint8_t)(st->nests | (1u << k)); }
        flag(SF_INVASION_HINT);                     /* no "walk into a nest" toast after the nests */
    }
    if (e > E37) {
        st->pet_form = (uint8_t)(st->pet_form | 3);
        flag(SF_PET_NUBS);
        flag(SF_PET_HORNS);
    }
    /* the house: BARRICADED through Chapter 3, OPEN from Ep 4.1 */
    if (e >= E41) { if (home_phase() < HP_OPEN) home_set_phase(HP_OPEN); }
    else if (e > E31 && home_phase() < HP_BARRICADED) home_set_phase(HP_BARRICADED);
    /* Lady Smog: from the gate until her rite */
    if (e >= E41 && e <= E43 && !has(SF_C4_LADY_SMOG)) possess(ID_LADY_SMOG, 4);
    if (e > E42) story_set_flag(SF_MET0 + 8, 1);
    if (e > E43) {
        story_set_flag(SF_MET0 + 9, 1);
        flag(SF_C4_SOOTKIN); flag(SF_C4_LADY_SMOG); flag(SF_C4_RIVET_HOME);
        unpossess(ID_LADY_SMOG);
    }
    if (e > E44) flag(SF_WISP_AWAKE);
    if (e > ED2) flag(SF_VAPULA_PLUSH);
    if (e > EF4) flag(SF_C4_MAYOR_CAME);
    if (e > E46) { flag(SF_MASCOTTY_HEAD_OFF); flag(SF_FOUNTAIN_TWOWAY); }
}

static void sync_if_moved(void)
{
    if (ep() != synced_ep || cur() != synced_beat) sync();
}

/* ---- the next episode / beat --------------------------------------------------------- */
static void finish(void)
{
    int e = ep(), b = cur();
    if (e < 0) return;
    busy++;
    story_episode_set_done(ep_beat[e], 1);
    /* the flashbacks F3 and F2 play inside Ep 3.4 and Ep 3.7 */
    if (e == E34) story_episode_set_done(BEAT_F3, 1);
    if (e == E37) story_episode_set_done(BEAT_F2, 1);
    story_set_step(0);
    if (ep() != e || e == E_COUNT - 1) {
        /* the next episode of the same run (or the epilogue's last) */
        sync();
        story_objective_refresh();
        ow_refresh_cast();
        busy--;
        return;
    }
    switch (b) {
    case BEAT_C3_ERUPTION: story_set_beat(BEAT_C3_NESTS); break;
    case BEAT_C3_NESTS: story_set_beat(BEAT_C3_SPIRE); break;
    case BEAT_C3_SPIRE: story_set_beat(BEAT_C4_GATE); break;
    case BEAT_C4_GATE: story_set_beat(BEAT_C4_PRISONERS); break;
    case BEAT_C4_PRISONERS: story_set_beat(BEAT_C4_HELP); break;
    case BEAT_C4_HELP: story_set_beat(BEAT_C4_MAYOR); break;
    case BEAT_C4_MAYOR: story_set_beat(BEAT_C4_FESTIVAL); break;
    case BEAT_C4_FESTIVAL: case BEAT_C4_STAGE:
        /* the credits rolled and E1, the post-credits button, played */
        flag(SF_CREDITS_SEEN);
        story_episode_set_done(BEAT_E1, 1);
        story_set_beat(BEAT_DONE);
        unlock_grant(USTORY_UNITED_PARK);
        unlock_grant(USTORY_NEW_GAME_PLUS);
        unlock_grant(173);                        /* foundation: United Park title theme */
        unlock_grant(174);                        /* foundation: Cute + Gore gore mode */
        unlock_grant(UL_STORY_MISC2_FIRST + 0);   /* Title: On Fire */
        unlock_grant(UL_STORY_MISC2_FIRST + 8);   /* Breach 3-7 post-story */
        break;
    default: break;
    }
    sync();
    busy--;
}

static void chapter_card(int ch)
{
    if (!has(SF_TITLE_CH1 + ch - 1)) {
        flag(SF_TITLE_CH1 + ch - 1);
        story_push_cutscene(CUT_CHAPTER1 + ch - 1, CTX_FREE, 0);
    }
}

void ch34_enter_beat(int b)
{
    busy++;
    sync();
    switch (b) {
    case BEAT_C3_ERUPTION:
        ow_enter_map(MAPID_HAHALAND_INVADED, 4, 8);
        chapter_card(3);
        break;
    case BEAT_C4_GATE:
        ow_enter_map(MAPID_HAHALAND, 53, 30);
        chapter_card(4);
        unlock_grant(USTORY_HELLVILE_VISITS);
        break;
    case BEAT_C4_FESTIVAL:
        ow_enter_map(MAPID_PARK_UNITED, 16, 26);
        break;
    default: break;
    }
    ow_refresh_cast();
    busy--;
}

/* ---- pushes the VM cannot do ---------------------------------------------------------- */
static void breach_won(int lv);

static void push_breach(int lv)
{
    retry_breach = (uint8_t)lv;
    if (story_debug_autowin_on()) {                  /* SCENE_BREACH is not one of autowin's */
        progress_add(PROG_DEMONS_KILLED, 3);
        breach_won(lv);
        return;
    }
    story_push_scene(&SCENE_BREACH, lv, PEND_COMBAT, CTX_BEAT, X_BREACH | lv);
}

static void retry_choice(void)
{
    static const char *const opt[2] = { "Go back down", "Not yet" };
    story_choice_id(NAR, "Go back down into the Breach?", opt, 2, CB_C34_RETRY);
}

static int nest_lv(int nest) { return nest == 0 ? BR_SEWERS : nest == 1 ? BR_FACTORY : BR_GRAVEYARD; }

/* a nest's citizens come out: one of its six lines (N33_<nest>_<k>, consecutive) */
static void nest_line(int nest)
{
    static const uint16_t first[STORY_NESTS] = { TXT_N33_0_0, TXT_N33_1_0, TXT_N33_2_0, TXT_N33_3_0 };
    int k = (int)(story_rng(0x4E455354u + (uint32_t)nest) % 6u);
    story_say_txt(NAR, (unsigned)(first[nest & 3] + k));
}

static void nest_heart(int nest)
{
    DoomoStry *st = story_stry();
    if (nest < 0 || nest >= STORY_NESTS) return;
    st->nests = (uint8_t)(st->nests | (1u << nest));
    nest_line(nest);
    /* after Fuzz Goes Dark, the bell rings at the wrong time: bedtime (stage bit 2) */
    if (nest != 1 && heart(1) && ep() == E33 && !(stage() & 2)) {
        story_set_step(stage() | 2);
        conv(SCR_S33_BELL);
    }
    if (hearts() == STORY_NESTS && ep() == E33) finish();
    else {
        if (nests_flagged() == 2 && !has(SF_RIVET_UPGRADE)) conv(SCR_S33_RIVET_CALL);
        story_objective_refresh();
        ow_refresh_cast();
    }
}

static void breach_won(int lv)
{
    int e = ep();
    if (lv == BR_UNDERBELLY) {
        if (e != E45) return;
        story_set_step(2);
        conv(SCR_S45_FED);
        conv(SCR_S45_PUMP);
        conv_cb(SCR_S45_BOTTOM, CB_C34_PETBOSS);
        return;
    }
    if (e != E33) return;
    pending_heart = (int8_t)(lv == BR_SEWERS ? 0 : lv == BR_FACTORY ? 1 : 3);
    conv_cb(lv == BR_SEWERS ? SCR_S33_PLAZA_WON : lv == BR_FACTORY ? SCR_S33_SPEECH : SCR_S33_GRAVE_WON,
            CB_C34_HEART);
}

static void e1_start(void)
{
    story_set_step(1);
    story_push_cutscene(CUT_E1, CTX_BEAT, X_CUT | CUT_E1);
}

/* ---- the native actions (`native CB_C34 C34A_X [P]`) ------------------------------------ */
static void check_episode(void)
{
    int e = ep(), s = stage();
    switch (e) {
    case E35:
        if (has(SF_C3_EXO0) && has(SF_C3_EXO0 + 1)) finish();
        break;
    case E42:
        if ((s & 31) == 31 && !(s & 64)) {
            story_set_step(s | 64);
            conv_cb(SCR_S42_DONE, CB_C34_FINISH);
        }
        break;
    case E43:
        if (s == 1 && has(SF_C4_SOOTKIN) && has(SF_C4_LADY_SMOG) && has(SF_C4_RIVET_HOME)) set_stage(2);
        break;
    case E47:
        if ((s & 15) == 15 && cur() == BEAT_C4_FESTIVAL) {
            conv(SCR_S47_BUFFET);
            story_set_beat(BEAT_C4_STAGE);
        }
        break;
    default: break;
    }
    story_objective_refresh();
    ow_refresh_cast();
}

static void action(int a, int p)
{
    DoomoStry *st = story_stry();
    switch (a) {
    case C34A_NEXT: set_stage(stage() + 1); check_episode(); break;
    case C34A_STEP: set_stage(p); break;
    case C34A_BIT: set_stage(stage() | (1 << (p & 7))); check_episode(); break;
    case C34A_DONE: finish(); break;
    case C34A_PET:
        st->pet_form = (uint8_t)(st->pet_form | (p & 7));
        if (p & 3) flag(SF_PET_NUBS);
        if (p & 2) flag(SF_PET_HORNS);
        if (p & 4) flag(SF_PET_HALO);
        break;
    case C34A_HELD: {
        int held = home_defend_result() >= 4 || story_debug_autowin_on();
        if (held) flag(SF_HOUSE_HELD);
        unlock_grant(UL_STORY_MISC2_FIRST + 10);          /* Arena: Hold the Door */
        vm_set_var(0, held);
        break; }
    case C34A_WALL_GONE: flag(SF_HOUSE_WALL_GONE); break;
    case C34A_WISP: st->branch = (uint8_t)((st->branch & ~3u) | (unsigned)((vm_var(0) + 1) & 3)); break;
    case C34A_SAYIT: st->branch = (uint8_t)((st->branch & ~12u) | (unsigned)((vm_var(0) & 3) << 2)); break;
    case C34A_KNOCK:
        if (p < STORY_KNOCKS) flag(SF_KNOCK(p));
        if (st->knocks < p + 1) st->knocks = (uint8_t)(p + 1);
        story_objective_refresh();
        break;
    case C34A_NIGHT: vm_set_var(2, town_is_night()); break;
    case C34A_LOST: {
        static const int16_t who[4] = { ID_STEAMY, ID_HATTIE, ID_GRAMPY, ID_LADY_SMOG };
        const DoomoResident *r = town_res(who[p & 3]);
        vm_set_var(2, r && (r->status == RES_SOUL_LOST || r->status == RES_GHOST || r->status == RES_DEAD));
        break; }
    case C34A_PACIFIST:
        if (!has(SF_WEAPON_BRUTAL)) {
            unlock_grant(USTORY_PACIFIST);
            flag(SF_PET_HALO);
            st->pet_form = (uint8_t)(st->pet_form | 4);
        }
        break;
    case C34A_BREACH: push_breach(p == 255 ? retry_breach : p); break;
    case C34A_PETBOSS: story_push_scene(&SCENE_PET, PET_ARG_BOSS, PEND_PET, CTX_BEAT, X_PETBOSS); break;
    case C34A_CUT: story_push_cutscene(CUT_P0 + p, CTX_FREE, 0); break;
    case C34A_HOME: ow_enter_map(story_map(), 4, 8); break;
    case C34A_TRAVEL:
        if (p == 0) ow_enter_map(MAPID_HAHALAND, 55, 30);
        else if (p == 1) ow_enter_map(MAPID_HELLVILE, 3, 27);
        else ow_enter_map(MAPID_PARK_UNITED, 34, 24);
        break;
    case C34A_SPIRE_CAPS:
        txt_stash(0, (uint16_t)TXT_V36_F1);
        txt_stash(1, (uint16_t)(has(SF_VAPE_SAMPLE) ? TXT_V36_F3P : TXT_V36_F3N));
        txt_stash(2, (uint16_t)(has(SF_ASKED_OTHER_SIDE) ? TXT_V36_F4A : TXT_V36_F4N));
        txt_stash(3, (uint16_t)TXT_V36_F6);
        break;
    case C34A_FRIENDS: vm_set_var(2, story_hellvile_friends()); break;
    case C34A_GRAVE: nest_heart(p & 3); break;
    case C34A_E2: {
        static const uint16_t say[4] = { TXT_E2_SAY0, TXT_E2_SAY1, TXT_E2_SAY2, TXT_E2_SAY3 };
        flag(SF_SUITCASE_OPEN);
        story_say_txt(NAR, say[(st->branch >> 2) & 3]);
        break; }
    case C34A_REFRESH: ow_refresh_cast(); break;
    default: break;
    }
}

/* the gate between the towns in Chapter 4 (C strings: no pack needed) */
static void travel_menu(void)
{
    static const char *const hh[3] = { "Go to Hellvile", "Go to the united park", "Stay here" };
    static const char *const hv[3] = { "Go to HaHaland", "Go to the united park", "Stay here" };
    static const char *const pk[3] = { "Go to HaHaland", "Go to Hellvile", "Stay here" };
    static const char *const hh2[2] = { "Go to Hellvile", "Stay here" };
    static const char *const hv2[2] = { "Go to HaHaland", "Stay here" };
    int map = story_map(), park = cur() >= BEAT_C4_FESTIVAL;
    if (map == MAPID_PARK_UNITED) story_choice_id(NAR, "Where to?", pk, 3, CB_C34_TRAVEL);
    else if (park) story_choice_id(NAR, "The gate stands open.", map == MAPID_HELLVILE ? hv : hh, 3, CB_C34_TRAVEL);
    else story_choice_id(NAR, "The gate stands open.", map == MAPID_HELLVILE ? hv2 : hh2, 2, CB_C34_TRAVEL);
}

static void travel_pick(int c)
{
    int map = story_map(), park = cur() >= BEAT_C4_FESTIVAL;
    if (map == MAPID_PARK_UNITED) {
        if (c == 0 || c == 1) action(C34A_TRAVEL, c);
        return;
    }
    if (c == 0) action(C34A_TRAVEL, map == MAPID_HELLVILE ? 0 : 1);
    else if (c == 1 && park) action(C34A_TRAVEL, 2);
}

int ch34_owns_cb(int cb) { return cb >= CB_C34_FIRST && cb <= CB_C34; }

void ch34_callback(int cb, int arg)
{
    int e = ep();
    busy++;
    switch (cb) {
    case CB_C34: action(arg & 0xFF, (arg >> 8) & 0xFF); break;
    case CB_C34_TRAVEL: travel_pick(arg); break;
    case CB_C34_FINISH: finish(); break;
    case CB_C34_CHECK: check_episode(); break;
    case CB_C34_BREACH: push_breach(retry_breach); break;
    case CB_C34_RETRY: if (arg == 0) push_breach(retry_breach); break;
    case CB_C34_HEART: nest_heart(pending_heart); pending_heart = -1; break;
    case CB_C34_PETBOSS: if (e == E45) action(C34A_PETBOSS, 0); break;
    case CB_C34_CREDITS:
        if (e == E47) story_push_scene(&SCENE_CREDITS, 0, PEND_CREDITS, CTX_BEAT, X_CREDITS);
        break;
    default: break;
    }
    busy--;
}

/* ---- talking ---------------------------------------------------------------------------- */
static const int16_t nest_host[STORY_NESTS] = { ID_NANA, ID_PIP, -1, ID_MAYOR };
static const uint8_t nest_hx[STORY_NESTS] = { 28, 18, 36, 14 }, nest_hy[STORY_NESTS] = { 22, 53, 38, 33 };

/* the nest whose Breach `id` waits to send you down, -1 */
static int host_nest(int id)
{
    int k;
    for (k = 0; k < STORY_NESTS; k++)
        if (nest_host[k] == id && has(SF_NEST0 + k) && !heart(k)) return k;
    return -1;
}

static const int16_t workers[5] = { ID_CLINKER, ID_PLODDY, ID_WHEEZLE, ID_HAZE, ID_THUMPKIN };
static const uint8_t worker_x[5] = { 5, 7, 9, 5, 7 }, worker_y[5] = { 23, 23, 23, 31, 31 };

static int place(int place);

static int npc(int id)
{
    int e = ep(), s = stage(), k;
    switch (e) {
    case E31:
        if (s == 1 && (id == ID_HATTIE || id == ID_GRAMPY)) {
            story_fmt_target = (int16_t)id;
            conv(SCR_S31_KNOCK);                       /* ends with STEP 2 */
            return 1;
        }
        if (id == ID_SQUEAKER) { conv(SCR_S31_SQUEAKER); return 1; }
        if (s == 2 && (id == ID_MAYOR || id == ID_FUZZ || id == ID_NANA)) {
            set_stage(3);
            conv(SCR_S31_CHURCH);
            conv(SCR_S31_WINDOW);
            return 1;
        }
        if (s == 3 && id == ID_RIVET) { conv_cb(SCR_S31_RIVET, CB_C34_FINISH); return 1; }
        if (s >= 3 && id == ID_MAYOR) return idle(id, SCR_S31_MAYOR_IDLE);
        if (s >= 3 && id == ID_NANA) return idle(id, SCR_S31_NANA_IDLE);
        if (s >= 3 && id == ID_PIP) return idle(id, SCR_S31_PIP_IDLE);
        break;
    case E32:
        if (id == ID_RIVET) return idle(id, SCR_S32_RIVET_IDLE);
        if (id == ID_MAYOR) return idle(id, SCR_S31_MAYOR_IDLE);
        break;
    case E33:
        if ((id == ID_HATTIE || id == ID_GRAMPY) && possessed(id)) {
            story_fmt_target = (int16_t)id;
            conv(SCR_S33_POSSESSED);
            return 1;
        }
        k = host_nest(id);
        if (k >= 0) { retry_breach = (uint8_t)nest_lv(k); retry_choice(); return 1; }
        if (id == ID_RIVET && !has(SF_RIVET_UPGRADE)) {
            flag(SF_RIVET_UPGRADE);
            conv(SCR_S33_RIVET_UPGRADE);
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        if (id == ID_MAYOR) return idle(id, SCR_S33_MAYOR_IDLE);
        break;
    case E34: case EF3:
        if (id == ID_RIVET || id == ID_MAYOR || id == ID_FUZZ || id == ID_BLOOM || id == ID_NANA) {
            conv(SCR_S34_FOUNTAIN);
            conv_cb(SCR_S34_CONFESS, CB_C34_FINISH);
            return 1;
        }
        break;
    case E35:
        if ((id == ID_HATTIE || id == ID_GRAMPY) && possessed(id)) {
            story_fmt_target = (int16_t)id;
            conv_cb(id == ID_HATTIE ? SCR_S35_HATTIE : SCR_S35_GRAMPY, CB_C34_CHECK);
            return 1;
        }
        break;
    case E36:
        if (id == ID_FUZZ) return idle(id, SCR_S35_DONE);
        break;
    case E37: case EF2:
        if (s == 0 && id == ID_MAYOR) {
            set_stage(1);
            conv(SCR_S37_PARTY);
            conv(SCR_S37_FOG);
            return 1;
        }
        if (id == ID_MAYOR) return idle(id, SCR_S37_MAYOR_IDLE);
        break;
    case E41:
        if (s <= 1 && (id == ID_RIVET || id == ID_MAYOR || id == ID_FUZZ)) {
            if (s == 0) conv(SCR_S41_DAWN);
            set_stage(2);
            conv(SCR_S41_GATE);
            conv(SCR_S41_CROSS);
            return 1;
        }
        if (s == 2 && id == ID_TALLYWORN) {
            conv(SCR_S41_TALLY);
            conv_cb(SCR_S41_TALLY_NAME, CB_C34_FINISH);
            return 1;
        }
        break;
    case E42: {
        static const uint16_t scr[5] = { SCR_S42_CLINKER, SCR_S42_PLODDY, SCR_S42_WHEEZLE, SCR_S42_HAZE, SCR_S42_THUMPKIN };
        for (k = 0; k < 5; k++)
            if (id == workers[k]) {
                if (s & (1 << k)) conv(SCR_S42_WORKER_IDLE);
                else { set_stage(s | (1 << k)); conv_cb(scr[k], CB_C34_CHECK); }
                return 1;
            }
        break; }
    case E43:
        if (id == ID_MOTHER_NOX) {
            if (s == 0) { set_stage(1); conv(SCR_S43_NOX); }
            else if (s == 2) conv_cb(SCR_S43_DONE, CB_C34_FINISH);
            else { idle(id, SCR_S43_NOX_IDLE); }
            return 1;
        }
        if (id == ID_SOOTKIN && has(SF_C4_SOOTKIN)) return idle(id, SCR_S43_SUNFLOWER);
        if (s != 1) break;
        if (id == ID_SOOTKIN && !has(SF_C4_SOOTKIN)) {
            if (inv_count(ITEM_FLOWERS) > 0) conv_cb(SCR_S43_SOOTKIN, CB_C34_CHECK);
            else conv(SCR_S43_SOOTKIN_ASK);
            return 1;
        }
        if (id == ID_LADY_SMOG && !has(SF_C4_LADY_SMOG) && possessed(ID_LADY_SMOG)) {
            story_fmt_target = ID_LADY_SMOG;
            conv_cb(SCR_S43_SMOG, CB_C34_CHECK);
            return 1;
        }
        if (id == ID_ANVIL && !has(SF_C4_RIVET_HOME)) { conv_cb(SCR_S43_ANVIL, CB_C34_CHECK); return 1; }
        if (id == ID_RIVET && !has(SF_C4_RIVET_HOME)) { conv(SCR_S43_RIVET_NO); return 1; }
        break;
    case E44: case ED2:
        if (id == ID_WISP) return place(PLACE_HELL_PITS);
        break;
    case E45:
        if (id == ROSTER_ID_GRUMBLEGUTS) {
            retry_breach = BR_UNDERBELLY;
            if (s == 0) { set_stage(1); conv_cb(SCR_S45_GRUMBLE, CB_C34_BREACH); }
            else if (s == 1) retry_choice();
            else conv_cb(SCR_S45_BOTTOM, CB_C34_PETBOSS);
            return 1;
        }
        break;
    case EF4:
        if (id == ID_MAYOR) { conv_cb(SCR_SF4_FETCH, CB_C34_FINISH); return 1; }
        break;
    case E46:
        if (id == ID_MAYOR || id == ID_MOTHER_NOX) {
            conv(SCR_S46_SUMMIT);
            conv_cb(SCR_S46_MASCOTTY, CB_C34_FINISH);
            return 1;
        }
        break;
    case E47:
        if (cur() == BEAT_C4_STAGE) {
            if (id == ID_MAYOR) { conv_cb(SCR_S47_STAGE, CB_C34_CREDITS); return 1; }
            break;
        }
        if (id == ID_NANA || id == ID_CHARLOTTE) { conv((s & 1) ? SCR_S47_STALL_IDLE : SCR_S47_COOK); return 1; }
        if (id == ID_WHEEZLE) { conv((s & 2) ? SCR_S47_STALL_IDLE : SCR_S47_FISH); return 1; }
        if (id == ID_SHUTTERBUG) {
            conv((s & 4) ? SCR_S47_STALL_IDLE : story_side2_state(33) == 3 ? SCR_S47_PHOTO_HOME : SCR_S47_PHOTO);
            return 1;
        }
        if (id == ID_HATTIE) { conv((s & 8) ? SCR_S47_STALL_IDLE : SCR_S47_JOKE); return 1; }
        break;
    case EE1: case EE2:
        if (id == ID_TALLYWORN) return idle(id, SCR_SE_TALLY);
        if (id == ID_RIVET) return idle(id, SCR_SE_RIVET);
        if (id == ID_WISP) {
            switch (story_stry()->branch & 3) {
            case 1: conv(SCR_SE_WISP_HUM); return 1;
            case 2: conv(SCR_SE_WISP_LIE); return 1;
            case 3: conv(SCR_SE_WISP_CHAIR); return 1;
            default: break;
            }
            break;
        }
        if (id == ROSTER_ID_GRUMBLEGUTS && !story_grumble_dead()) return idle(id, SCR_SE_GRUMBLE);
        if (id == ID_MAYOR) return idle(id, SCR_SE_MAYOR);
        if (id == ID_VEX) return idle(id, SCR_SE_VEX);
        break;
    default: break;
    }
    return 0;
}

int ch34_npc(int id)
{
    int r;
    if (story_talking()) return 1;            /* a script is running: no second one */
    quiet = 0;
    busy++;
    sync_if_moved();
    r = npc(id);
    busy--;
    return r;
}

/* ---- walk-on events ---------------------------------------------------------------------- */
static int trigger(int tag)
{
    int e = ep();
    switch (tag) {
    case STORY_TAG_EVENT_SPIRE:
        if (e == E36) { conv(SCR_S36_BASE); return 1; }
        if (e >= E33 && e < E36) {
            story_say(NAR, "(The cloud tower is sealed by a wall of screaming vapor. The nests first!)");
            return 1;
        }
        break;
    case STORY_TAG_EVENT_STAGE:
        if (e == E47 && cur() == BEAT_C4_STAGE) { conv_cb(SCR_S47_STAGE, CB_C34_CREDITS); return 1; }
        break;
    case STORY_TAG_EVENT_GATE_SEAL:
        if (e == E45) return npc(ROSTER_ID_GRUMBLEGUTS);
        story_say(NAR, e < E45 ? "(The sealed gate hums. Something on the other side is very, very bored.)" :
                                 "(The old hell gate stands open. A sign: BACK IN FIVE MINUTES.)");
        return 1;
    default: break;
    }
    return 0;
}

int ch34_trigger(int tag)
{
    int r;
    if (story_talking()) return 1;            /* a script is running: no second one */
    quiet = 0;
    busy++;
    sync_if_moved();
    r = trigger(tag);
    busy--;
    return r;
}

/* ---- doors -------------------------------------------------------------------------------- */
static int place(int pl)
{
    int e = ep(), s = stage();
    switch (pl) {
    case PLACE_HOME:
        if (e == E32) { conv_cb(SCR_S32_BOARD, CB_C34_FINISH); return 1; }
        if ((e == E37 || e == EF2) && s == 0) return npc(ID_MAYOR);
        if (e == EE2 && !done(EE2)) { conv_cb(SCR_SE2_SUITCASE, CB_C34_FINISH); return 1; }
        break;
    case PLACE_CHURCH:
        if (e == E31 && s == 2) return npc(ID_FUZZ);
        break;
    case PLACE_VAPE_SHOP:
        if (e == E36) return trigger(STORY_TAG_EVENT_SPIRE);
        break;
    case PLACE_WALL_GATE:
        if ((e == E37 || e == EF2) && s == 1) { conv_cb(SCR_S37_GATE, CB_C34_FINISH); return 1; }
        if (e < E41) {
            story_say(NAR, "(The gate in the Wall is jammed shut by writhing vapor. Not yet.)");
            return 1;
        }
        if (e == E41 && s <= 1) return npc(ID_RIVET);
        travel_menu();
        return 1;
    case PLACE_HELL_PITS:
        if (e == E44) {
            conv(SCR_S44_ROOM);
            conv(SCR_S44_WAKE);
            conv(SCR_S44_ASK);
            conv_cb(SCR_S44_PENPALS, CB_C34_FINISH);
            return 1;
        }
        if (e == ED2) { conv_cb(SCR_S44_D2, CB_C34_FINISH); return 1; }
        break;
    case PLACE_TOWN_HALL:
        if (e == EF4) return npc(ID_MAYOR);
        break;
    case PLACE_CHAPEL:
        if (e == E43) return npc(ID_MOTHER_NOX);
        break;
    case PLACE_FORGE:
        if (e == E43 && s == 1 && !has(SF_C4_RIVET_HOME)) return npc(ID_ANVIL);
        break;
    case PLACE_STAGE:
        if (e == E47) return trigger(STORY_TAG_EVENT_STAGE);
        break;
    case PLACE_WORKSHOP:
        if (e == E33 && !has(SF_RIVET_UPGRADE)) return npc(ID_RIVET);
        if (e >= E33 && e <= E36) {
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        break;
    default: break;
    }
    return 0;
}

int ch34_place(int pl)
{
    int r;
    if (story_talking()) return 1;            /* a script is running: no second one */
    quiet = 0;
    busy++;
    sync_if_moved();
    r = place(pl);
    busy--;
    return r;
}

/* ---- results of sub-scenes ---------------------------------------------------------------- */
void ch34_resume(const StoryPending *p, int result)
{
    int e;
    quiet = 0;
    busy++;
    sync_if_moved();
    e = ep();
    switch (p->kind) {
    case PEND_CUTSCENE:
        if (p->arg == CUT_E1 && (e == E47 || e == EE1)) conv_cb(SCR_SE1_AFTER, CB_C34_FINISH);
        break;
    case PEND_COMBAT:
        if (p->ctx == CTX_NEST) {                   /* an overworld nest (script_nest_encounter) */
            int nest = p->extra & 3;
            if (result <= 0) { conv(SCR_S33_RETREAT); break; }
            story_set_flag(SF_NEST0 + nest, 1);
            story_reward(40, ITEM_SCRAP, 2, USTORY_NONE);
            retry_breach = (uint8_t)nest_lv(nest);
            switch (nest) {
            case 0: conv_cb(SCR_S33_PLAZA, CB_C34_BREACH); break;
            case 1: conv_cb(SCR_S33_BEACH, CB_C34_BREACH); break;
            case 2:
                pending_heart = 2;
                conv_cb(heart(1) ? SCR_S33_PARK_DARK : SCR_S33_PARK, CB_C34_HEART);
                break;
            default: conv_cb(SCR_S33_GRAVE, CB_C34_BREACH); break;
            }
            story_objective_refresh();
            ow_refresh_cast();
        } else if ((p->extra & 0xF00) == X_BREACH) {
            if (result > 0) breach_won(p->extra & 0xFF);
            else if ((p->extra & 0xFF) == BR_UNDERBELLY)
                story_say(NAR, "(You climb back out, puffing. The sealed gate is still sealed.)");
            else conv(SCR_S33_RETREAT);
        }
        break;
    case PEND_PET:
        if (p->extra == X_PETBOSS && e == E45) {
            story_set_step(3);
            conv(SCR_S45_TURN);
            conv_cb(SCR_S45_FINE, CB_C34_FINISH);
        }
        break;
    case PEND_CREDITS:
        if (p->extra == X_CREDITS && e == E47) {
            flag(SF_CREDITS_SEEN);
            e1_start();                            /* the beat moves on after E1 */
        }
        break;
    default: break;
    }
    busy--;
}

/* ---- things that start by themselves on an idle overworld ---------------------------------- */
static int autostart(void)
{
    int e = ep(), s = stage();
    switch (e) {
    case E31: if (s == 0) { story_set_step(1); conv(SCR_S31_WAKE); return 1; } break;
    case E33: if (s == 0) { story_set_step(1); conv(SCR_S33_INTRO); return 1; } break;
    case E36: if (s == 0) { story_set_step(1); conv(SCR_S35_DONE); return 1; } break;
    case EI3: if (auto_ep != EI3) { auto_ep = EI3; conv(SCR_SI3_HOME); return 1; } break;
    case E41: if (s == 0) { story_set_step(1); conv(SCR_S41_DAWN); return 1; } break;
    case E42: if (!(s & 128)) { story_set_step(s | 128); conv(SCR_S42_YARD); return 1; } break;
    case E47:
        if (cur() == BEAT_C4_FESTIVAL && !(s & 16)) { story_set_step(s | 16); conv(SCR_S47_OPEN); return 1; }
        break;
    case EE1: if (s == 0) { e1_start(); return 1; } break;
    default: break;
    }
    return 0;
}

int ch34_auto(void)
{
    int r;
    if (busy || quiet || story_talking() || story_pending.kind != PEND_NONE || !ow_active() ||
        scene_top() != &SCENE_STORY)
        return 0;
    busy++;
    sync_if_moved();
    r = autostart();
    if (r) ow_refresh_cast();
    busy--;
    return r;
}

/* ---- the HUD -------------------------------------------------------------------------------- *
 * Decoded from the episode's pack only when it changes. A pack that is not live is never loaded
 * while a script runs (on the PS1 that would evict the running script's pack). */
static const char *obj(unsigned tid, int n0)
{
    static char buf[72];
    static uint16_t last = 0xFFFFu;
    static int16_t ln0 = -1;
    if (last != tid || ln0 != n0) {
        char raw[72];
        int n[1];
        if (!txt_pack_is_live(TXT_PACK(tid)) && vm_running()) return buf;
        if (txt_get((uint16_t)tid, raw, (int)sizeof(raw)) < 0) return buf;
        n[0] = n0;
        vm_fmt_n(buf, (int)sizeof(buf), raw, n, 1);
        last = (uint16_t)tid;
        ln0 = (int16_t)n0;
    }
    return buf;
}

static int bits(int v, int n)
{
    int k, c = 0;
    for (k = 0; k < n; k++) c += (v >> k) & 1;
    return c;
}

const char *ch34_objective(void)
{
    int e, s, k;
    ch34_auto();
    sync_if_moved();
    e = ep();
    s = stage();
    switch (e) {
    case E31: return obj(s == 0 ? TXT_O31_WAKE : s == 1 ? TXT_O31_RUN : s == 2 ? TXT_O31_CHURCH : TXT_O31_RIVET, 0);
    case E32: return obj(TXT_O32_BOARD, 0);
    case E33:
        for (k = 0; k < STORY_NESTS; k++)
            if (has(SF_NEST0 + k) && !heart(k)) return obj(TXT_O33_HEART, 0);
        if (nests_flagged() == 2 && !has(SF_RIVET_UPGRADE)) return obj(TXT_O33_RIVET, 0);
        return obj(TXT_O33_NESTS, hearts());
    case E34: case EF3: return obj(TXT_O34_FOUNTAIN, 0);
    case E35: return obj(TXT_O35_RITES, has(SF_C3_EXO0) + has(SF_C3_EXO0 + 1));
    case E36: return obj(TXT_O36_SPIRE, 0);
    case E37: case EF2: return obj(s == 0 ? TXT_O37_PARTY : TXT_O37_TRAIL, 0);
    case EI3: {
        int n = story_stry()->knocks;
        return obj(n ? TXT_OI3_KNOCK : TXT_OI3_HOME, DOOMO_MIN(7, n));
    }
    case E41: return obj(s < 2 ? TXT_O41_GATE : TXT_O41_TALLY, 0);
    case E42: return obj(TXT_O42_SWAP, bits(s, 5));
    case E43:
        if (s == 0) return obj(TXT_O43_NOX, 0);
        if (s == 2) return obj(TXT_O43_BACK, 0);
        if (!has(SF_C4_SOOTKIN) && !inv_count(ITEM_FLOWERS) && has(SF_C4_LADY_SMOG) && has(SF_C4_RIVET_HOME))
            return obj(TXT_O43_FLOWERS, 0);
        return obj(TXT_O43_FAVOURS, has(SF_C4_SOOTKIN) + has(SF_C4_LADY_SMOG) + has(SF_C4_RIVET_HOME));
    case E44: return obj(TXT_O44_PITS, 0);
    case ED2: return obj(TXT_O44_SLEEP, 0);
    case E45: return obj(s < 2 ? TXT_O45_GATE : TXT_O45_BOTTOM, 0);
    case EF4: return obj(TXT_OF4_FETCH, 0);
    case E46: return obj(TXT_O46_SUMMIT, 0);
    case E47:
        if (cur() == BEAT_C4_STAGE) return obj(TXT_O47_STAGE, 0);
        return obj(TXT_O47_STALLS, bits(s, 4));
    case EE1: case EE2:
        if (e == EE2 && !done(EE2)) return obj(TXT_OE2_SUITCASE, 0);
        {
            const char *side = side_objective();
            if (side && side[0]) return side;
        }
        return obj(TXT_OE1_FREE, 0);
    default: return "";
    }
}

/* ---- where the main target is ---------------------------------------------------------------- */
typedef struct { int16_t id; uint8_t map, x, y; } Target;

static Target target(void)
{
    Target t;
    int e = ep(), s = stage(), k;
    t.id = -1; t.map = MAPID_HAHALAND_INVADED; t.x = 0; t.y = 0;
#define AT(who, m, cx, cy) do { t.id = (int16_t)(who); t.map = (uint8_t)(m); t.x = (uint8_t)(cx); t.y = (uint8_t)(cy); } while (0)
    switch (e) {
    case E31:
        if (s <= 1) AT(s == 1 ? ID_HATTIE : -1, MAPID_HAHALAND_INVADED, 10, 17);
        else if (s == 2) AT(ID_FUZZ, MAPID_HAHALAND_INVADED, 7, 28);
        else AT(ID_RIVET, MAPID_HAHALAND_INVADED, 11, 28);
        break;
    case E32: AT(-1, MAPID_HAHALAND_INVADED, 4, 8); break;
    case E33:
        for (k = 0; k < STORY_NESTS; k++)
            if (has(SF_NEST0 + k) && !heart(k)) {
                AT(nest_host[k], MAPID_HAHALAND_INVADED, nest_hx[k], nest_hy[k]);
                return t;
            }
        for (k = 0; k < STORY_NESTS; k++)
            if (!has(SF_NEST0 + k)) {
                int cx, cy;
                if (ow_nest_cell(k, &cx, &cy)) AT(-1, MAPID_HAHALAND_INVADED, cx, cy);
                else AT(-1, MAPID_HAHALAND_INVADED, nest_hx[k], nest_hy[k]);
                return t;
            }
        AT(-1, MAPID_HAHALAND_INVADED, 34, 24);
        break;
    case E34: case EF3: AT(ID_RIVET, MAPID_HAHALAND_INVADED, 32, 22); break;
    case E35:
        if (possessed(ID_HATTIE)) AT(ID_HATTIE, MAPID_HAHALAND_INVADED, 25, 17);
        else AT(ID_GRAMPY, MAPID_HAHALAND_INVADED, 27, 54);
        break;
    case E36: AT(-1, MAPID_HAHALAND_INVADED, 53, 37); break;
    case E37: case EF2:
        if (s == 0) AT(ID_MAYOR, MAPID_HAHALAND_INVADED, 5, 8);
        else AT(-1, MAPID_HAHALAND_INVADED, 54, 30);
        break;
    case EI3: AT(-1, MAPID_HAHALAND_INVADED, 4, 8); break;
    case E41:
        if (s < 2) AT(ID_RIVET, MAPID_HAHALAND, 54, 28);
        else AT(ID_TALLYWORN, MAPID_HELLVILE, 9, 30);
        break;
    case E42:
        AT(-1, MAPID_HELLVILE, 9, 28);
        for (k = 0; k < 5; k++)
            if (!(s & (1 << k))) { AT(workers[k], MAPID_HELLVILE, worker_x[k], worker_y[k]); break; }
        break;
    case E43:
        if (s != 1) AT(ID_MOTHER_NOX, MAPID_HELLVILE, 7, 13);
        else if (!has(SF_C4_LADY_SMOG)) AT(ID_LADY_SMOG, MAPID_HELLVILE, 49, 17);
        else if (!has(SF_C4_RIVET_HOME)) AT(ID_ANVIL, MAPID_HELLVILE, 31, 9);
        else if (inv_count(ITEM_FLOWERS) > 0) AT(ID_SOOTKIN, MAPID_HELLVILE, 41, 35);
        else AT(-1, MAPID_HAHALAND, 31, 18);
        break;
    case E44: case ED2: AT(ID_WISP, MAPID_HELLVILE, 20, 10); break;
    case E45: AT(ROSTER_ID_GRUMBLEGUTS, MAPID_HELLVILE, 38, 27); break;
    case EF4: AT(ID_MAYOR, MAPID_HAHALAND, 29, 8); break;
    case E46: AT(ID_MOTHER_NOX, MAPID_HELLVILE, 8, 24); break;
    case E47:
        if (cur() == BEAT_C4_STAGE) AT(ID_MAYOR, MAPID_PARK_UNITED, 29, 10);
        else if (!(s & 1)) AT(ID_NANA, MAPID_PARK_UNITED, 4, 18);
        else if (!(s & 2)) AT(ID_WHEEZLE, MAPID_PARK_UNITED, 28, 20);
        else if (!(s & 4)) AT(ID_SHUTTERBUG, MAPID_PARK_UNITED, 20, 18);
        else AT(ID_HATTIE, MAPID_PARK_UNITED, 38, 18);
        break;
    case EE1: case EE2:
        if (e == EE2 && !done(EE2)) AT(-1, MAPID_HAHALAND, 4, 8);
        else t.map = 0xFF;
        break;
    default: t.map = 0xFF; break;
    }
#undef AT
    return t;
}

int ch34_target(int *map_id, int *cx, int *cy)
{
    Target t;
    sync_if_moved();
    t = target();
    if (map_id) *map_id = t.map == 0xFF ? -1 : t.map;
    if (cx) *cx = t.x;
    if (cy) *cy = t.y;
    return t.id;
}

/* ---- the cast ---------------------------------------------------------------------------------- *
 * One row per placement: the episodes it covers (lo..hi), the map, the cell. A resident appears at
 * most once per map: the first row that matches wins. */
typedef struct { uint8_t lo, hi, map, x, y; int16_t id; } CastRow;
#define HH MAPID_HAHALAND
#define IV MAPID_HAHALAND_INVADED
#define HV MAPID_HELLVILE
#define PK MAPID_PARK_UNITED
static const CastRow cast_rows[] = {
    /* Ep 3.1: the road and the church door */
    { E31, E31, IV, 10, 17, ID_HATTIE }, { E31, E31, IV, 11, 17, ID_GRAMPY },
    { E31, E31, IV, 14, 18, ID_SQUEAKER }, { E31, E31, IV, 11, 28, ID_RIVET },
    { E32, E32, IV, 6, 8, ID_RIVET },
    /* the fountain (Ep 3.4) */
    { E34, EF3, IV, 32, 22, ID_RIVET }, { E34, EF3, IV, 36, 22, ID_MAYOR }, { E34, EF3, IV, 34, 23, ID_FUZZ },
    { E34, EF3, IV, 31, 23, ID_BLOOM }, { E34, EF3, IV, 37, 23, ID_NANA },
    /* Hattie and Grampy, possessed, until their rites */
    { E33, E33, IV, 40, 21, ID_HATTIE }, { E35, E35, IV, 25, 17, ID_HATTIE },
    { E33, E35, IV, 27, 54, ID_GRAMPY },
    /* the party at your house */
    { E37, EF2, IV, 5, 8, ID_MAYOR },
    /* the church shelter, the whole chapter */
    { E31, EI3, IV, 4, 28, ID_MAYOR }, { E31, EI3, IV, 7, 28, ID_FUZZ }, { E31, EI3, IV, 9, 28, ID_NANA },
    { E31, EI3, IV, 5, 29, ID_PIP }, { E31, EI3, IV, 10, 29, ID_BLOOM }, { E31, EI3, IV, 3, 29, ID_LULLA },
    { E33, E36, IV, 53, 10, ID_RIVET },
    /* Chapter 4, HaHaland: the gate, then the town */
    { E41, E41, HH, 54, 30, ID_MAYOR }, { E41, E41, HH, 54, 28, ID_RIVET }, { E41, E41, HH, 54, 32, ID_FUZZ },
    { E42, EF4, HH, 29, 8, ID_MAYOR }, { EE1, EE2, HH, 29, 8, ID_MAYOR }, { E42, E42, HH, 53, 10, ID_RIVET },
    { E41, EE2, HH, 25, 17, ID_NANA }, { E41, EE2, HH, 10, 47, ID_PIP }, { E41, EE2, HH, 30, 42, ID_LULLA },
    { E42, EE2, HH, 7, 28, ID_FUZZ }, { E41, EE2, HH, 17, 23, ID_BLOOM },
    /* Hellvile */
    { E41, EE2, HV, 9, 30, ID_TALLYWORN },
    { E42, E42, HV, 5, 23, ID_CLINKER }, { E42, E42, HV, 7, 23, ID_PLODDY }, { E42, E42, HV, 9, 23, ID_WHEEZLE },
    { E42, E42, HV, 5, 31, ID_HAZE }, { E42, E42, HV, 7, 31, ID_THUMPKIN }, { E42, E43, HV, 8, 27, ID_HAZEY },
    { E46, E46, HV, 6, 24, ID_MAYOR }, { E46, E46, HV, 8, 24, ID_MOTHER_NOX },
    { E41, EE2, HV, 7, 13, ID_MOTHER_NOX }, { E41, EE2, HV, 41, 35, ID_SOOTKIN },
    { E41, EE2, HV, 49, 17, ID_LADY_SMOG }, { E41, EE2, HV, 31, 9, ID_ANVIL },
    { E43, E46, HV, 29, 9, ID_RIVET }, { E42, E46, HV, 47, 15, ID_VEX }, { E43, E46, HV, 44, 13, ID_HAZE },
    { E42, E46, HV, 20, 10, ID_WISP }, { E45, EE2, HV, 38, 27, ROSTER_ID_GRUMBLEGUTS },
    /* the united park */
    { E47, E47, PK, 4, 18, ID_NANA }, { E47, E47, PK, 6, 18, ID_CHARLOTTE }, { E47, E47, PK, 28, 20, ID_WHEEZLE },
    { E47, E47, PK, 20, 18, ID_SHUTTERBUG }, { E47, E47, PK, 38, 18, ID_HATTIE },
    { E47, E47, PK, 16, 30, ROSTER_ID_GRUMBLEGUTS },
    { E47, EE2, PK, 29, 10, ID_MAYOR }, { E47, EE2, PK, 34, 10, ID_MOTHER_NOX }, { E47, EE2, PK, 26, 10, ID_FUZZ },
    { E47, EE2, PK, 37, 10, ID_RIVET }, { E47, EE2, PK, 38, 11, ID_ANVIL }, { E47, EE2, PK, 31, 10, ID_PIP },
    { EE1, EE2, PK, 4, 18, ID_NANA }, { E47, EE2, PK, 37, 18, ID_VEX }, { E47, EE2, PK, 28, 22, ID_SOOTKIN },
    { E47, EE2, PK, 45, 30, ID_VAPULA }, { E47, EE2, PK, 12, 26, ID_BLOOM }, { E47, EE2, PK, 35, 20, ID_TALLYWORN },
    { E47, EE2, PK, 33, 12, ID_WISP },
};
#undef HH
#undef IV
#undef HV
#undef PK

int ch34_cast(int map, StoryNpcSpot *out, int max)
{
    int e, n = 0, i, k;
    Target t;
    sync_if_moved();
    e = ep();
    if (e < 0) return 0;
    t = target();
    for (i = 0; i < DOOMO_COUNTOF(cast_rows) && n < max; i++) {
        const CastRow *c = &cast_rows[i];
        int id = c->id, dup = 0, fl = NPCF_FIXED, x = c->x, y = c->y;
        if (c->map != map || e < c->lo || e > c->hi || !alive(id)) continue;
        for (k = 0; k < n; k++) if (out[k].id == id) dup = 1;
        if (dup) continue;
        if (e == E33 && map == MAPID_HAHALAND_INVADED && (k = host_nest(id)) >= 0) {
            x = nest_hx[k];                        /* waits at the nest until its Breach is beaten */
            y = nest_hy[k];
        }
        if (id == t.id && map == t.map) fl |= NPCF_MAIN;
        if (e == E33 && id == ID_RIVET && !has(SF_RIVET_UPGRADE)) fl |= NPCF_SIDE;
        out[n].id = (int16_t)id;
        out[n].x = (uint8_t)x;
        out[n].y = (uint8_t)y;
        out[n].dir = 0;
        out[n].flags = (uint8_t)fl;
        n++;
    }
    return n;
}

/* ---- debug: set up a beat, step the plot --------------------------------------------------------- */
void ch34_debug_setup(int b)
{
    DoomoStoryState *s = &doomo_town.story;
    int m, cx, cy, i;
    busy++;
    s->quest = (uint8_t)b;
    s->map = b < BEAT_C4_GATE ? MAPID_HAHALAND_INVADED : b == BEAT_C4_GATE ? MAPID_HAHALAND :
             b < BEAT_C4_FESTIVAL ? MAPID_HELLVILE : b < BEAT_DONE ? MAPID_PARK_UNITED : MAPID_HAHALAND;
    if (b > BEAT_C4_PRISONERS)
        for (i = 0; i < 5; i++) story_befriend(workers[i], STORY_FRIEND_LEVEL);
    if (b >= BEAT_DONE) flag(SF_CREDITS_SEEN);
    auto_ep = -1;
    quiet = 1;                                /* a quiet beat, as the legacy jumps were */
    sync();
    ch34_target(&m, &cx, &cy);
    if (m < 0 || m != s->map) {
        cx = s->map == MAPID_HELLVILE ? 3 : s->map == MAPID_PARK_UNITED ? 16 : 34;
        cy = s->map == MAPID_HELLVILE ? 27 : 26;
    }
    s->x = (int16_t)(cx * DOOMO_CELL_UNITS + 32);
    s->y = (int16_t)((cy + 1) * DOOMO_CELL_UNITS + 32);
    busy--;
}

/* a debug jump to an episode id (story.c debug_jump_episode, after it set
 * the done bits): the episode's own state, the player at its target, and
 * its opening plays (not quiet, unlike a legacy beat's jump) */
void ch34_debug_episode(int beat)
{
    DoomoStoryState *s = &doomo_town.story;
    int m, cx, cy;
    (void)beat;
    busy++;
    story_set_step(ep() == EF2 ? 1 : 0);              /* F2 plays at the gate, after the party */
    sync();
    story_rearm_possessors();
    quiet = 0;
    auto_ep = -1;
    if (ch34_target(&m, &cx, &cy), m >= 0) {
        s->map = (uint8_t)m;
        s->x = (int16_t)(cx * DOOMO_CELL_UNITS + 32);
        s->y = (int16_t)((cy + 1) * DOOMO_CELL_UNITS + 32);
    }
    if (ow_active()) ow_enter_map(s->map, s->x / DOOMO_CELL_UNITS, s->y / DOOMO_CELL_UNITS);
    story_objective_refresh();
    busy--;
}

static int debug_step(void)
{
    int e = ep(), s = stage(), k;
    if (autostart()) return 1;
    switch (e) {
    case E31:
        if (s == 1) { story_interact_npc(ID_HATTIE); return 1; }
        if (s == 2) { story_enter_place(PLACE_CHURCH); return 1; }
        story_interact_npc(ID_RIVET);
        return 1;
    case E32: story_enter_place(PLACE_HOME); return 1;
    case E33:
        for (k = 0; k < STORY_NESTS; k++)
            if (has(SF_NEST0 + k) && !heart(k)) {
                if (k == 2) nest_heart(2);
                else push_breach(nest_lv(k));
                return 1;
            }
        for (k = 0; k < STORY_NESTS; k++)
            if (!has(SF_NEST0 + k)) { script_nest_encounter(k); return 1; }
        finish();
        return 1;
    case E34: case EF3: story_interact_npc(ID_RIVET); return 1;
    case E35:
        if (possessed(ID_HATTIE)) { story_interact_npc(ID_HATTIE); return 1; }
        if (possessed(ID_GRAMPY)) { story_interact_npc(ID_GRAMPY); return 1; }
        flag(SF_C3_EXO0);
        flag(SF_C3_EXO0 + 1);
        check_episode();
        return 1;
    case E36: story_trigger_ow(STORY_TAG_EVENT_SPIRE); return 1;
    case E37: case EF2:
        if (s == 0) { story_interact_npc(ID_MAYOR); return 1; }
        story_enter_place(PLACE_WALL_GATE);
        return 1;
    case EI3: auto_ep = -1; return autostart();
    case E41:
        if (s <= 1) { story_interact_npc(ID_RIVET); return 1; }
        story_interact_npc(ID_TALLYWORN);
        return 1;
    case E42:
        for (k = 0; k < 5; k++) if (!(s & (1 << k))) { story_interact_npc(workers[k]); return 1; }
        story_set_step(s & ~64);
        check_episode();
        return 1;
    case E43:
        if (s != 1) { story_interact_npc(ID_MOTHER_NOX); return 1; }
        if (!has(SF_C4_SOOTKIN)) {
            if (!inv_count(ITEM_FLOWERS)) inv_add(ITEM_FLOWERS, 1);
            story_interact_npc(ID_SOOTKIN);
            return 1;
        }
        if (!has(SF_C4_LADY_SMOG)) {
            if (possessed(ID_LADY_SMOG)) story_interact_npc(ID_LADY_SMOG);
            else flag(SF_C4_LADY_SMOG);
            return 1;
        }
        if (!has(SF_C4_RIVET_HOME)) { story_interact_npc(ID_ANVIL); return 1; }
        check_episode();
        return 1;
    case E44: case ED2: story_enter_place(PLACE_HELL_PITS); return 1;
    case E45:
        if (s == 1) { push_breach(BR_UNDERBELLY); return 1; }
        story_interact_npc(ROSTER_ID_GRUMBLEGUTS);
        return 1;
    case EF4: story_interact_npc(ID_MAYOR); return 1;
    case E46: story_interact_npc(ID_MOTHER_NOX); return 1;
    case E47:
        if (cur() == BEAT_C4_STAGE) { story_trigger_ow(STORY_TAG_EVENT_STAGE); return 1; }
        if (!(s & 1)) { story_interact_npc(ID_NANA); return 1; }
        if (!(s & 2)) { story_interact_npc(ID_WHEEZLE); return 1; }
        if (!(s & 4)) { story_interact_npc(ID_SHUTTERBUG); return 1; }
        if (!(s & 8)) { story_interact_npc(ID_HATTIE); return 1; }
        check_episode();
        return 1;
    case EE1: return 0;
    case EE2:
        if (!done(EE2)) { story_enter_place(PLACE_HOME); return 1; }
        return 0;
    default: return 0;
    }
}

int ch34_debug_step(void)
{
    int r;
    quiet = 0;
    busy++;
    sync_if_moved();
    r = debug_step();
    busy--;
    return r;
}
