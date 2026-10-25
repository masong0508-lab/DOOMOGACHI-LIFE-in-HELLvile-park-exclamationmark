/* story.c - the story state machine (owner: story): flags, chapters and
 * beats, conversations longer than the dialogue queue, sub-scenes the
 * plot starts (and what their results mean), rewards, friendship, the
 * debug hooks the scripted playthrough uses, and the dispatch of NPC /
 * trigger / place events to the main plot, the side quests and small talk.
 * The plot itself is in story_script.c.
 */
#include "story_int.h"
#include "combat_int.h"     /* AF_USED (read only: actor slots) */
#include "overworld_int.h"  /* the VM host: ownpc_cast / ownpc_act / ow_cam / ow_run_beat */
#include "doomo/txt.h"
#include "doomo/house_api.h"
#include "doomo/unlock_ids.h"
#include "story_ch12.h"       /* S2: Chapters 1-2 as VM content */
#include "story_ch34.h"       /* S3: Chapters 3-4 as VM content */

StoryPending story_pending;
static uint8_t autowin;
static uint8_t friends_dirty = 1;
static int16_t friends_cache[2];

/* ---- flags ------------------------------------------------------------------ */
int story_flag(int f)
{
    if (f < 0 || f >= DOOMO_STORY_FLAGS) return 0;
    return (doomo_town.story.flags[f >> 3] >> (f & 7)) & 1;
}

void story_set_flag(int f, int on)
{
    if (f < 0 || f >= DOOMO_STORY_FLAGS) return;
    if (on) doomo_town.story.flags[f >> 3] |= (uint8_t)(1 << (f & 7));
    else    doomo_town.story.flags[f >> 3] &= (uint8_t)~(1 << (f & 7));
}

/* ---- chapters & beats -------------------------------------------------------- */
static const char *const ch_titles[STORY_CH_DONE + 1] = {
    "", "Welcome to HaHaland", "Something in the Vapor", "Hell Invades",
    "Hellvile Park", "Happily Ever After"
};

static const uint8_t beat_ch[BEAT_COUNT] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,      /* C1 */
    2, 2, 2, 2, 2, 2, 2,                  /* C2 */
    3, 3, 3,                              /* C3 */
    4, 4, 4, 4, 4, 4,                     /* C4 */
    5                                     /* DONE */
};

static const char *const beat_names[BEAT_COUNT] = {
    "C1_ARRIVE", "C1_MAYOR", "C1_KEY", "C1_HOME", "C1_HATCH", "C1_PIP",
    "C1_BUNNY", "C1_BUNNY_RETURN", "C1_FRIENDS", "C1_CUPCAKE", "C1_WALL",
    "C2_VAPOR", "C2_WEIRD", "C2_SCREAM", "C2_FUZZ", "C2_EXORCISE",
    "C2_RIVET", "C2_CELLAR",
    "C3_ERUPTION", "C3_NESTS", "C3_SPIRE",
    "C4_GATE", "C4_PRISONERS", "C4_HELP", "C4_MAYOR", "C4_FESTIVAL",
    "C4_STAGE", "DONE"
};

int story_chapter(void) { return doomo_town.story.chapter; }
int story_beat(void) { return doomo_town.story.quest; }
int story_step(void) { return doomo_town.story.quest_step; }
void story_set_step(int n) { doomo_town.story.quest_step = (uint8_t)DOOMO_CLAMP(n, 0, 255); }

const char *story_chapter_title(int ch)
{
    return (ch >= 0 && ch <= STORY_CH_DONE) ? ch_titles[ch] : "";
}

const char *story_beat_name(int beat)
{
    return (beat >= 0 && beat < BEAT_COUNT) ? beat_names[beat] : "?";
}

int story_beat_chapter(int beat)
{
    return (beat >= 0 && beat < BEAT_COUNT) ? beat_ch[beat] : 0;
}

int story_map(void) { return doomo_town.story.map; }

/* Own an unlock without the "NEW UNLOCK!" toast (jukebox songs and
 * gallery cutscenes arrive in batches; one summary toast is enough). */
int story_unlock_quiet(int id)
{
    if (id < 0 || id >= DOOMO_UNLOCK_BITS || !unlock_by_id(id) || unlock_has(id)) return 0;
    doomo_town.unlocks[id >> 3] |= (uint8_t)(1u << (id & 7));
    return 1;
}

static void chapter_unlocks(int finished)
{
    /* songs heard in each chapter go to the jukebox */
    static const uint8_t songs[5][6] = {
        { SONG_TITLE, SONG_HAHALAND_DAY, SONG_HAHALAND_NIGHT, SONG_CREATOR, SONG_PET, SONG_SHOP },
        { SONG_CHURCH, SONG_EXORCISM, SONG_WORKSHOP, SONG_VAPE_LOUNGE, SONG_OMINOUS, SONG_HELL_BREACH },
        { SONG_INVASION, SONG_BOSS, SONG_VICTORY, SONG_SAD, 0, 0 },
        { SONG_HELLVILE, SONG_FESTIVAL, SONG_CREDITS, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 }
    };
    int i, n = 0;
    if (finished < 1 || finished > 4) return;
    for (i = 0; i < 6; i++)
        if (songs[finished - 1][i]) n += story_unlock_quiet(USTORY_SONG_FIRST + songs[finished - 1][i] - 1);
    unlock_grant(USTORY_CH1_DONE + finished - 1);
    if (n) {
        char buf[40];
        doomo_snprintf(buf, sizeof(buf), STR_ICON_NOTE " %d new songs in the jukebox!", n);
        ui_toast(buf);
    }
}

void story_set_beat(int beat)
{
    DoomoStoryState *s = &doomo_town.story;
    int ch;
    if (beat < 0 || beat >= BEAT_COUNT) return;
    ch = beat_ch[beat];
    if (ch > s->chapter && s->chapter >= 1) {
        int c;
        for (c = s->chapter; c < ch; c++) chapter_unlocks(c);
    }
    s->chapter = (uint8_t)ch;
    s->quest = (uint8_t)beat;
    s->quest_step = 0;
    {   /* (S2) every episode / special before this beat's episode is done */
        int o = story_episode_order(beat), e;
        for (e = BEAT_P0; e < BEAT_ALL_COUNT; e++)
            if (story_episode_order(e) < o) story_episode_set_done(e, 1);
    }
    script_enter_beat(beat);
    story_objective_refresh();
}

/* ---- the player & the pet ------------------------------------------------------ */
const char *story_player_name(void)
{
    const DoomoResident *p = town_res(DOOMO_PLAYER_ID);
    return (p && p->name[0]) ? p->name : "Friend";
}

const char *story_pet_name(void)
{
    if (pet_exists()) return doomo_town.pet.name;
    return "your egg";
}

static void default_player(void)
{
    DoomoResident *p = town_res(DOOMO_PLAYER_ID);
    if (!p) return;
    if (p->status == RES_EMPTY || !p->name[0]) {
        resident_generate(p, doomo_town.seed ^ 0x5EEDu);
        memcpy(p->name, "Sunny", 6);
        p->look.species = SPECIES_BUNNYKIN;
        p->look.ears = EARS_BUNNY;
        p->look.tail = TAIL_BUNNY;
        p->look.horns = HORNS_NONE;
        p->look.wings = WINGS_NONE;
        p->status = RES_ALIVE;
        resident_sanitize(p);
    }
    p->flags = (uint8_t)((p->flags | RF_PLAYER) & ~RF_HELLBORN);
    p->status = RES_ALIVE;
}

void story_new_game(void)
{
    DoomoStoryState *s = &doomo_town.story;
    int i;
    doomo_town.mode = TOWN_STORY;
    default_player();
    memset(s, 0, sizeof(*s));
    memset(&doomo_town.pet, 0, sizeof(doomo_town.pet));
    s->chapter = STORY_CH1;
    s->map = MAPID_HAHALAND;
    s->x = 34 * DOOMO_CELL_UNITS + 32;
    s->y = 26 * DOOMO_CELL_UNITS + 32;
    s->facing = 3;
    s->hp = 100;
    story_set_flag(SF_STARTED, 1);
    s->quest = BEAT_C1_ARRIVE;
    /* the story cast never dies of neglect */
    for (i = 0; i < NPC_COUNT; i++) {
        DoomoResident *r = town_res(roster_story_npc(i));
        if (r && r->status != RES_EMPTY) r->flags |= RF_STORY_NPC;
    }
    doomo_town.minute = 9 * 60;
    if (doomo_town.money < DOOMO_START_MONEY) doomo_town.money = DOOMO_START_MONEY;
    memset(&story_pending, 0, sizeof(story_pending));
    friends_dirty = 1;
    story_rearm_possessors();                    /* no stale slot from another session */
    script_enter_beat(BEAT_C1_ARRIVE);
    story_objective_refresh();
}

void story_begin(void)
{
    uint32_t seed = doomo_hash32(plat_entropy() ^ (plat_ticks() * 2654435761u));
    town_new(TOWN_STORY, seed, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_NEW);
}

/* ---- friendship ---------------------------------------------------------------- */
int story_friend_count(int district)
{
    if (friends_dirty) {
        int i;
        friends_cache[0] = friends_cache[1] = 0;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            const DoomoResident *r = &doomo_town.res[i];
            if (r->status != RES_EMPTY && r->level >= STORY_FRIEND_LEVEL)
                friends_cache[RES_DISTRICT(i)]++;
        }
        friends_dirty = 0;
    }
    if (district < 0) return friends_cache[0] + friends_cache[1];
    return friends_cache[district & 1];
}

int story_hellvile_friends(void) { return story_friend_count(DISTRICT_HELLVILE); }

void story_befriend(int id, int amount)
{
    DoomoResident *r = town_res(id);
    int before;
    if (!r || id == DOOMO_PLAYER_ID || r->status == RES_EMPTY || amount <= 0) return;
    before = r->level;
    r->level = (uint8_t)DOOMO_MIN(99, r->level + amount);
    rel_adjust(DOOMO_PLAYER_ID, id, amount * 4);
    if (before < STORY_FRIEND_LEVEL && r->level >= STORY_FRIEND_LEVEL) {
        progress_add(PROG_FRIENDSHIPS, 1);
        if (RES_DISTRICT(id) == DISTRICT_HELLVILE) progress_add(PROG_CROSS_BORDER_FRIENDS, 1);
        friends_cache[RES_DISTRICT(id)]++;
        if (story_friend_count(-1) >= 25) unlock_grant(USTORY_FRIEND_OF_ALL);
    }
}

/* ---- rewards ----------------------------------------------------------------------- */
void story_reward(int money, int item, int count, int unlock_id)
{
    char buf[48];
    if (money > 0) {
        doomo_town.money += (uint32_t)money;
        progress_add(PROG_MONEY_EARNED, (uint32_t)money);
    }
    if (item > ITEM_NONE && item < ITEM_COUNT && count > 0) inv_add(item, count);
    if (money > 0 && item > ITEM_NONE && item < ITEM_COUNT && count > 0)
        doomo_snprintf(buf, sizeof(buf), "+%d H$  " STR_ICON_STAR " %s x%d", money, doomo_items[item].name, count);
    else if (money > 0)
        doomo_snprintf(buf, sizeof(buf), "+%d H$!", money);
    else if (item > ITEM_NONE && item < ITEM_COUNT && count > 0)
        doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR " Got %s x%d", doomo_items[item].name, count);
    else
        buf[0] = 0;
    if (buf[0]) ui_toast(buf);
    if (unlock_id > 0 && unlock_id < DOOMO_UNLOCK_BITS) unlock_grant(unlock_id);
    audio_sfx(SFX_PICKUP);
}

/* ---- formatting (the resident formatter in scriptvm.c) ---------------------------- */
void story_fmt(char *out, int cap, const char *t) { vm_fmt(out, cap, t); }

/* ---- conversations ------------------------------------------------------------------
 * The plot's lines are VM scripts in the text packs (story_conv); a few
 * places still format a C string (story_say). After a script, its callback
 * id (story_script.c CB_*, story_side.c 64.., story_places.c 128..) runs
 * with the choice in var 0. Callbacks are ids, never function pointers: a
 * story-group pointer must not reach the resident dialogue queue or VM
 * (SPEC C6). */
#define CB_FIFO 4
static struct { uint8_t id, ready; int8_t choice; } cbq[CB_FIFO];
static uint8_t cbq_n;
#define CONV_Q 4
static struct { uint16_t scr; uint8_t cb; int16_t arg; } convq[CONV_Q];
static uint8_t convq_n, vm_cb, vm_live;
static uint16_t later_minute = 0xFFFFu;

static void cb_attach(int id)
{
    if (!id) return;
    if (cbq_n >= CB_FIFO) { plat_log("story: callback queue full"); return; }
    cbq[cbq_n].id = (uint8_t)id;
    cbq[cbq_n].ready = 0;
    cbq[cbq_n].choice = 0;
    cbq_n++;
}

static void cb_mark(int choice)
{
    int i;
    for (i = 0; i < cbq_n; i++)
        if (!cbq[i].ready) { cbq[i].ready = 1; cbq[i].choice = (int8_t)choice; return; }
}

static void cb_choice_tramp(int c) { cb_mark(c); }

static void cb_run(int id, int c)
{
    if (id >= 128) place_callback(id, c);
    else if (id >= 64) side_callback(id, c);
    else script_callback(id, c);
}

void story_poll_callbacks(void)
{
    while (cbq_n && cbq[0].ready && !ui_dialog_active()) {
        int id = cbq[0].id, c = cbq[0].choice, i;
        for (i = 1; i < cbq_n; i++) cbq[i - 1] = cbq[i];
        cbq_n--;
        cb_run(id, c);
    }
}

void story_say(int who, const char *text)
{
    char buf[200];
    story_fmt(buf, sizeof(buf), text);
    ui_say(who, buf);
}

void story_say_txt_n(int who, unsigned tid, int n0)
{
    char raw[200], buf[200];
    int n[1];
    n[0] = n0;
    if (txt_get((uint16_t)tid, raw, (int)sizeof(raw)) < 0) return;
    vm_fmt_n(buf, (int)sizeof(buf), raw, n, 1);
    ui_say(who, buf);
}

void story_say_txt(int who, unsigned tid) { story_say_txt_n(who, tid, 0); }

void story_say_then(void (*done)(void))
{
    if (ui_dialog_active()) ui_say_done(done);
    else if (done) done();
}

/* A VM_PUSHED op: push the scene the script asked for (PEND_* kinds); the
 * result comes back through story_resume -> dispatch_result -> vm_resume. */
static void host_push(const StoryOp *o)
{
    int arg = (int16_t)o->b;
    if (o->op == OP_HOME) {
        int a = o->a == HOME_VISIT ? HOME_ARG_VISIT(arg) : HOME_ARG_STORY(HSM_DEFEND);
        story_push_scene(&SCENE_HOME, a, PEND_HOME, CTX_VM, o->b);
        return;
    }
    switch (o->a) {
    case PEND_COMBAT: story_push_combat(arg, CTX_VM, 0, -1, -1, -1); break;
    case PEND_EXORCISM: story_push_exorcism(arg, CTX_VM, 0); break;
    case PEND_EXO_PRACTICE: story_push_exorcism(-1, CTX_VM, 0); break;
    case PEND_CUTSCENE: story_push_cutscene(arg, CTX_VM, 0); break;
    case PEND_PET: story_push_scene(&SCENE_PET, 0, PEND_PET, CTX_VM, 0); break;
    case PEND_WORKSHOP: story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_VM, 0); break;
    case PEND_CREATOR: story_push_scene(&SCENE_CREATOR, arg, PEND_CREATOR, CTX_VM, 0); break;
    case PEND_CREDITS: story_push_scene(&SCENE_CREDITS, 0, PEND_CREDITS, CTX_VM, 0); break;
    case PEND_HOME: story_push_scene(&SCENE_HOME, arg, PEND_HOME, CTX_VM, 0); break;
    case PEND_COOKING: story_push_scene(&SCENE_COOKING, arg, PEND_COOKING, CTX_VM, 0); break;
    case PEND_CONCERT: story_push_scene(&SCENE_CONCERT, arg, PEND_CONCERT, CTX_VM, 0); break;
    case PEND_DREAM: story_push_scene(&SCENE_DREAM, arg, PEND_DREAM, CTX_VM, 0); break;
    case PEND_TIMING: story_push_scene(&SCENE_STORY_TIMING, (uint16_t)arg, PEND_TIMING, CTX_VM, 0); break;
    case PEND_MINIGAME_SIDE: story_push_scene(&SCENE_STORY_MG, (uint16_t)arg, PEND_MINIGAME_SIDE, CTX_VM, 0); break;
    default: vm_resume(1); break;
    }
}

/* A VM_EXT op (SPEC C6): the story group's half of the VM. */
static void host_ext(const StoryOp *o)
{
    int who = vm_ext_who();
    switch (o->op) {
    case OP_NATIVE:
        if (o->a >= STORY_CB_VERB && o->a < STORY_CB_VERB + VERB_COUNT)   /* S2: the verbs */
            story_verb_begin(o->a - STORY_CB_VERB, o->b, vm_ext_arg(), vm_ext_data(VMD_SCRIPT, 0));
        else
            cb_run(o->a, (int16_t)o->b);
        break;
    case OP_SET_BEAT: story_set_beat(o->b); break;
    case OP_REWARD:
        story_reward(o->a * 10, o->b & 0xFF, o->b >> 8, vm_ext_arg() ? vm_ext_arg() : USTORY_NONE);
        break;
    case OP_BEFRIEND: story_befriend(who, (int16_t)o->b); break;
    case OP_POSSESS: exorcism_possess(who, (int16_t)o->b); break;   /* S3: arms the authored row */
    case OP_UNPOSSESS: exorcism_unpossess(who); break;
    case OP_TELEPORT: if (ow_active()) ow_enter_map(o->a, o->b >> 8, o->b & 0xFF); break;
    case OP_CAST: if (ow_active()) ownpc_cast(who, o->b, vm_ext_arg()); break;
    case OP_NPC: if (ow_active()) ownpc_act(who, o->b & 0xFF, vm_ext_data(VMD_ARG, o->b >> 8)); break;
    case OP_CAM: if (ow_active()) ow_cam(o->a, (int16_t)o->b); break;
    case OP_BEAT: {
        BeatScript bs;
        if (ow_active() && txt_beat(o->b, &bs)) ow_run_beat(&bs);
        break; }
    default: break;                         /* INSERT / CUT / FREEZE_CAP / TINT: stage hosts */
    }
}

static void conv_start(unsigned scr, int cb, int arg)
{
    vm_start((int)scr);
    if (!vm_running()) { cb_attach(cb); cb_mark(0); return; }
    vm_set_var(1, arg);
    vm_cb = (uint8_t)cb;
    vm_live = 1;
    story_vm_pump();
}

void story_vm_pump(void)
{
    int guard = 64;
    while (guard-- > 0) {
        int r = vm_tick();
        if (r == VM_EXT) { host_ext(vm_ext_op()); continue; }
        if (r == VM_PUSHED) {
            const StoryOp *o = vm_ext_op();
            if (!o) return;                       /* still waiting for the scene */
            host_push(o);
            continue;                             /* autowin resumes at once */
        }
        if (r == VM_IDLE && vm_live) {
            int cb = vm_cb;
            vm_live = 0;
            vm_cb = 0;
            cb_attach(cb);
            cb_mark(vm_var(0));
        }
        if (r == VM_IDLE && convq_n) {      /* (S2) also after a script the overworld started */
            int i;
            unsigned scr = convq[0].scr;
            int c = convq[0].cb, a = convq[0].arg;
            for (i = 1; i < convq_n; i++) convq[i - 1] = convq[i];
            convq_n--;
            conv_start(scr, c, a);
            return;
        }
        if (r == VM_IDLE && !cbq_n && !ui_dialog_active() && story_pending.kind == PEND_NONE &&
            scene_top() == &SCENE_STORY && ow_active())
            script_auto();                  /* (S2) a moment that starts by itself */
        return;
    }
}

/* Once per game minute on the overworld: a due "Later" callback (C14). */
void story_vm_minute(void)
{
    int s;
    if (later_minute == doomo_town.minute) return;
    later_minute = doomo_town.minute;
    if (vm_running() || convq_n || story_talking()) return;
    if ((s = vm_later_due()) != 0) story_conv((unsigned)s, 0);
}

void story_conv_arg(unsigned scr, int cb, int arg)
{
    if (vm_running() || convq_n) {
        if (convq_n >= CONV_Q) { plat_log("story: conversation queue full"); return; }
        convq[convq_n].scr = (uint16_t)scr;
        convq[convq_n].cb = (uint8_t)cb;
        convq[convq_n].arg = (int16_t)arg;
        convq_n++;
        return;
    }
    conv_start(scr, cb, arg);
}

void story_conv(unsigned scr, int cb) { story_conv_arg(scr, cb, 0); }

/* Make the story's text live (PS1: CD reads into the resident window, at a
 * boundary that already has a fade; GBA / native: nothing to do). */
void story_text_prefetch(void)
{
    int p, n = txt_pack_count(), b = doomo_town.story.quest, ch12 = ch12_beat(b) && doomo_town.story.chapter != STORY_CH_NONE;
    int ch34 = ch34_beat(b) && doomo_town.story.chapter != STORY_CH_NONE;   /* (S3) */
    for (p = 0; p < n; p++) {
        int k = txt_pack_kind(p);
        /* (S4) of the side packs only the index (titles, hints: the quest log
         * and the HUD); a mission's own pack loads when its script starts.
         * Reading every side pack into the PS1's one side slot was a CD read
         * per pack at each boundary, of which only the last stayed. */
        if ((k == TXT_KIND_SIDE && p == PACK_QUESTS) || (k == TXT_KIND_LEGACY && !ch12 && !ch34)) txt_pack_load(p);
    }
    if (ch12) {                         /* (S2) the chapter's common pack and the beat's episode */
        txt_pack_load(story_chapter() >= STORY_CH2 ? PACK_CH2 : PACK_CH1);
        if (ch12_pack(b) >= 0) txt_pack_load(ch12_pack(b));
    }
    if (ch34 && ch34_pack() >= 0) txt_pack_load(ch34_pack());   /* (S3) the episode's pack */
}

void story_choice_id(int who, const char *q, const char *const *opts, int n, int cb)
{
    cb_attach(cb);
    story_choice(who, q, opts, n, cb_choice_tramp);
}

void story_choice(int who, const char *q, const char *const *opts, int n,
                  void (*done)(int))
{
    char buf[200];
    story_fmt(buf, sizeof(buf), q);
    ui_choice(who, buf, opts, n, done);
}

int story_talking(void)
{
    return ui_dialog_active() || vm_running() || convq_n != 0 || cbq_n != 0;
}

/* ---- sub-scenes --------------------------------------------------------------------- */
static void set_pending(int kind, int ctx, int arg, int extra)
{
    story_pending.kind = (uint8_t)kind;
    story_pending.ctx = (uint8_t)ctx;
    story_pending.arg = (int16_t)arg;
    story_pending.extra = (int16_t)extra;
}

static int pending_result(const StoryPending *p)
{
    const DoomoResident *r;
    switch (p->kind) {
    case PEND_COMBAT: return autowin ? 1 : combat_last_result();
    case PEND_EXORCISM:
        r = town_res(p->arg);
        if (!r) return 0;
        if (r->status == RES_ALIVE || r->status == RES_GHOST) return 1;
        if (r->status == RES_POSSESSED) return 0;
        return -1;                                  /* soul lost / laid to rest */
    case PEND_PET: return pet_exists() ? 1 : 0;
    case PEND_TIMING: return autowin ? 3 : story_timing_result();
    case PEND_MINIGAME_SIDE: return autowin ? 1 : story_mg_result();
    case PEND_CREATOR: {
        const DoomoResident *pl = town_res(DOOMO_PLAYER_ID);
        return (pl && pl->status != RES_EMPTY && pl->name[0]) ? 1 : 0; }
    default: return 1;
    }
}

static void dispatch_result(void)
{
    StoryPending p = story_pending;
    int result = pending_result(&p);
    memset(&story_pending, 0, sizeof(story_pending));
    if (p.kind == PEND_NONE) return;
    if (p.ctx == CTX_VM) vm_resume(result);
    else if (p.ctx == CTX_SIDE) side_resume(&p, result);
    else script_resume(&p, result);
}

void story_resume(void)
{
    if (story_pending.kind == PEND_NONE) return;
    dispatch_result();
}

void story_push_scene(const DoomoScene *s, int arg, int pend, int ctx, int extra)
{
    set_pending(pend, ctx, arg, extra);
    ow_sync_to_save();
    if (autowin && (pend == PEND_PET || pend == PEND_WORKSHOP || pend == PEND_EXO_PRACTICE ||
                    pend == PEND_TIMING || pend == PEND_MINIGAME_SIDE ||
                    /* a script's house beat, dream or show; "Go inside" still opens the house */
                    (ctx == CTX_VM && (pend == PEND_HOME || pend == PEND_DREAM ||
                                       pend == PEND_COOKING || pend == PEND_CONCERT)))) {
        if (pend == PEND_PET && !pet_exists()) pet_new("Doomo");
        dispatch_result();
        return;
    }
    scene_push(s, arg);
}

/* Demons that make up a chapter-3 nest / an ambush. */
static const uint8_t nest_demons[STORY_NESTS + 2][4] = {
    { THING_IMPLING, THING_IMPLING, THING_PUFF_DEMON, THING_IMPLING },          /* plaza     */
    { THING_HELLHOUND_PUP, THING_SLIME_BELCHER, THING_HELLHOUND_PUP, 0 },       /* beach     */
    { THING_BONE_JESTER, THING_IMPLING, THING_CYBER_PIGGY, THING_IMPLING },     /* park      */
    { THING_VAPE_WRAITH, THING_GARGOYLE, THING_VAPE_WRAITH, 0 },               /* graveyard */
    { THING_BRUTE, THING_IMPLING, THING_IMPLING, 0 },                           /* hunt      */
    { THING_CLOUD_SPIDER, THING_PUFF_DEMON, THING_GARGOYLE, THING_IMPLING }     /* hunt 2    */
};

static int spot_open(int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
    return !c->wall && !(c->flags & (CELL_WATER | CELL_LAVA));
}

/* The overworld maps are dressed with more props (trees, lamps, benches)
 * than the combat actor pool holds, which would leave no room for the
 * encounter's demons and their effects. Keep the props nearest the fight
 * and drop the far ones (out in the fog anyway). Bounded: one pass of at
 * most ACTOR_MAX x ACTOR_MAX steps, once per encounter. */
#define STORY_KEEP_PROPS 72
static void make_room_for_demons(int cx, int cy)
{
    int i, n = 0;
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a = &doomo_actors[i];
        if ((a->flags & AF_USED) && a->type >= THING_PROP_TREE && a->type <= THING_VAPE_PORTAL) n++;
    }
    while (n > STORY_KEEP_PROPS) {
        int far = -1;
        int32_t fd = -1;
        for (i = 0; i < ACTOR_MAX; i++) {
            const DoomoActor *a = &doomo_actors[i];
            int32_t dx, dy, d;
            if (!(a->flags & AF_USED) || a->type < THING_PROP_TREE || a->type > THING_VAPE_PORTAL) continue;
            dx = FIX2INT(a->x) / DOOMO_CELL_UNITS - cx;
            dy = FIX2INT(a->y) / DOOMO_CELL_UNITS - cy;
            d = dx * dx + dy * dy;
            if (d > fd) { fd = d; far = i; }
        }
        if (far < 0) break;
        doomo_actors[far].flags = 0;
        n--;
    }
}

void story_push_combat(int map_id, int ctx, int extra, int cx, int cy, int nest)
{
    set_pending(PEND_COMBAT, ctx, map_id, extra);
    ow_sync_to_save();
    if (autowin) {
        progress_add(PROG_DEMONS_KILLED, 3);
        dispatch_result();
        return;
    }
    scene_push(&SCENE_COMBAT, map_id);
    if (cx >= 0 && cy >= 0) {
        int i, k = 0;
        static const int8_t off[8][2] = { { 0, -3 }, { 2, -3 }, { -2, -3 }, { 3, -1 },
                                          { -3, -1 }, { 1, -4 }, { -1, -4 }, { 0, -5 } };
        combat_start_at(map_id, CGOAL_KILL_ALL, cx, cy, 3072);
        make_room_for_demons(cx, cy);
        if (nest >= 0 && nest < STORY_NESTS + 2) {
            for (i = 0; i < 4; i++) {
                int t = nest_demons[nest][i], tries;
                if (!t) continue;
                for (tries = 0; tries < 8; tries++, k++) {
                    int x = cx + off[k & 7][0], y = cy + off[k & 7][1];
                    if (spot_open(x, y)) { combat_spawn(t, x, y, 1); k++; break; }
                }
            }
        }
    }
}

void story_push_exorcism(int resident_id, int ctx, int extra)
{
    set_pending(resident_id >= 0 ? PEND_EXORCISM : PEND_EXO_PRACTICE, ctx, resident_id, extra);
    ow_sync_to_save();
    if (autowin) {
        if (resident_id >= 0) {
            const DoomoResident *r = town_res(resident_id);
            /* (S4) a soul lost is a reverse rite's host (S133): exorcism_apply
             * un-fails it when the rite is armed, else changes nothing */
            if (r && (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST)) exorcism_apply(resident_id, 1);
        }
        dispatch_result();
        return;
    }
    scene_push(&SCENE_EXORCISM, resident_id);
}

void story_push_cutscene(int cut, int ctx, int extra)
{
    if (cut >= CUT_P0 && cut < CUT_ALL_COUNT) story_unlock_quiet(UL_STORY_CUT2(cut));   /* the gallery */
    set_pending(PEND_CUTSCENE, ctx, cut, extra);
    ow_sync_to_save();
    scene_push(CUTX_OWNS(cut) ? &SCENE_CUTX : &SCENE_CUTSCENE, cut);   /* (S3) the Ch3-4 stages */
}

/* ---- events from the overworld ------------------------------------------------------ */
static int overworld_live(void)
{
    return scene_top() == &SCENE_STORY && doomo_town.story.chapter != STORY_CH_NONE;
}

void story_interact_npc(int id)
{
    const DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY) return;
    story_fmt_target = (int16_t)id;
    if (story_verbs_npc(id)) return;            /* S2: a delivery */
    if (script_npc(id)) return;
    if (side_npc(id)) return;
    talk_ambient(id);
}

/* The public entry (story.h) is called from other groups too (combat steps
 * on trigger cells of the story maps), where it never fires: the overworld
 * must be on top. It only records the tag, so no edge leads from another
 * overlay group into the plot (SPEC C6); SCENE_STORY's update runs it. */
static int16_t pend_trigger = -1;

void story_trigger(int tag)
{
    if (overworld_live()) pend_trigger = (int16_t)tag;
}

void story_trigger_poll(void)
{
    int t = pend_trigger;
    pend_trigger = -1;
    if (t >= 0) story_trigger_ow(t);
}

void story_trigger_ow(int tag)
{
    if (!overworld_live()) return;              /* e.g. fired inside combat */
    if (tag >= STORY_TAG_DOOR && tag < STORY_TAG_DOOR + PLACE_COUNT) {
        story_enter_place(tag - STORY_TAG_DOOR);
        return;
    }
    if (tag >= STORY_TAG_EVENT_SECRET && tag < STORY_TAG_EVENT_SECRET + 8) {
        int base = doomo_town.story.map == MAPID_HELLVILE ? 16 :
                   doomo_town.story.map == MAPID_PARK_UNITED ? 24 : 0;
        int f = STORY_FLAG_SECRET_FIRST + base + (tag - STORY_TAG_EVENT_SECRET);
        if (!story_flag(f)) {
            int i, n = 0;
            story_set_flag(f, 1);
            progress_add(PROG_SECRETS_FOUND, 1);
            audio_sfx(SFX_SPARKLE);
            story_reward(25 + base * 2, ITEM_CANDY_EYEBALLS, 1, USTORY_NONE);
            ui_toast(STR_ICON_SPARK " A secret sparkle!");
            for (i = 0; i < 32; i++) n += story_flag(STORY_FLAG_SECRET_FIRST + i);
            if (n >= 15) unlock_grant(USTORY_SECRETS);
        }
        return;
    }
    script_trigger(tag);
}

void story_enter_place(int place)
{
    if (place <= PLACE_NONE || place >= PLACE_COUNT) return;
    if (!story_flag(STORY_FLAG_PLACE_FIRST + place)) {
        int i, n = 0;
        story_set_flag(STORY_FLAG_PLACE_FIRST + place, 1);
        for (i = 1; i < PLACE_COUNT; i++) n += story_flag(STORY_FLAG_PLACE_FIRST + i);
        if (n >= PLACE_COUNT - 3) unlock_grant(USTORY_EXPLORER);
    }
    if (script_place(place)) return;
    if (side_place(place)) return;
    story_place_menu(place);
}

/* The HUD objective is a resident copy (S2): combat's HUD and the quest log
 * read it from other overlay groups, so story_objective() must not reach
 * the plot's code (on the PS1 that pinned the objective tables resident).
 * The story group refreshes it every SCENE_STORY tick and at every beat. */
static char objective[96];        /* the longest side hint is < 96 */

const char *story_objective(void) { return objective; }

void story_objective_refresh(void)
{
    const char *v = "";
    if (doomo_town.story.chapter != STORY_CH_NONE) {
        v = story_verbs_objective();                          /* a verb is running */
        if (!v) v = script_objective();
    }
    doomo_snprintf(objective, sizeof(objective), "%s", v ? v : "");
}

int story_target(int *map_id, int *cx, int *cy)
{
    return script_target(map_id, cx, cy);
}

void story_goto(int map_id, int cx, int cy)
{
    ow_enter_map(map_id, cx, cy);
}

/* ---- authored possessions across a reload ---------------------------------------------
 * exorcism.c's armed slots are not saved (STRY is full). The story knows when
 * each authored possession is its own: the @armed rows (core/gen/
 * possess_table.c, from the .dsc packs) give the beats. On every resume the
 * slots are cleared and a host that is still possessed inside its window is
 * re-armed with its row; a possession outside every window (the sim's own,
 * sim_possession_cap from Ch2) keeps the sim's pick. */

/* a beat's position in the story order: the episode, then the legacy beat
 * inside it (legacy ids increase in story order, test_story_vm.c checks) */
static int story_pos(int beat)
{
    int o = story_episode_order(beat);
    if (o < 0) return -1;
    return (o << 5) | (beat < BEAT_COUNT ? beat & 31 : 0);
}

int story_armed_window(const StoryArmed *a, int beat)
{
    int p = story_pos(beat), k;
    if (!a || a->host >= DOOMO_MAX_RESIDENTS || p < 0) return 0;
    if (p < story_pos(a->from) || p > story_pos(a->to)) return 0;
    for (k = 0; k < 2; k++)
        if (a->unless[k] != 0xFFFFu && story_flag(a->unless[k])) return 0;
    return 1;
}

int story_possess_row_of(int host)
{
    const DoomoPossessor *const volatile *tp = &doomo_possess_table;
    const volatile uint16_t *np = &doomo_possess_count;
    const DoomoPossessor *t = *tp;
    int i, n = t ? (int)*np : 0;
    for (i = 0; i < n; i++)
        if (t[i].host == (unsigned)host && !(t[i].flags & EXF_UNFAIL)) return i;
    return -1;
}

void story_rearm_possessors(void)
{
    int i, n = doomo_story_armed_count;
    exorcism_slots_clear();
    if (doomo_town.mode != TOWN_STORY) return;
    for (i = 0; i < n; i++) {
        const StoryArmed *a = &doomo_story_armed[i];
        const DoomoResident *r = town_res(a->host);
        if (!r || (r->status != RES_POSSESSED && r->status != RES_SOUL_LOST)) continue;
        if (!story_armed_window(a, doomo_town.story.quest)) continue;
        exorcism_possess(a->host, a->row);     /* arms only: the host is not RES_ALIVE */
    }
}

/* ---- debug / tests ------------------------------------------------------------------- */
void story_debug_autowin(int on) { autowin = (uint8_t)(on != 0); }
int  story_debug_autowin_on(void) { return autowin; }

/* (S2) A new episode / special id: the state of the legacy beat that is in
 * progress at that point of the story order (the old beats alias onto the
 * episodes), with every episode before it marked done and none after. */
static void debug_jump_episode(int ep)
{
    int o = story_episode_order(ep), b, legacy = BEAT_C1_ARRIVE, e;
    for (b = 0; b < BEAT_COUNT; b++)
        if (story_episode_order(b) <= o) legacy = b;
    story_debug_jump(story_beat_chapter(legacy), legacy);
    for (e = BEAT_P0; e < BEAT_ALL_COUNT; e++) story_episode_set_done(e, story_episode_order(e) < o);
    if (ch12_beat(legacy)) ch12_debug_episode(ep);     /* (S2) the stage inside the beat */
    if (ch34_beat(legacy)) ch34_debug_episode(ep);     /* (S3) the episode inside the beat */
}

void story_debug_jump(int chapter, int beat)
{
    (void)chapter;
    if (beat >= BEAT_P0 && beat < BEAT_ALL_COUNT) { debug_jump_episode(beat); return; }
    if (beat < 0 || beat >= BEAT_COUNT) return;
    if (doomo_town.story.chapter == STORY_CH_NONE) story_new_game();
    friends_dirty = 1;
    script_debug_setup(beat);
    story_set_beat(beat);
    story_rearm_possessors();
    friends_dirty = 1;
    story_objective_refresh();
    if (ow_active()) {
        const DoomoStoryState *s = &doomo_town.story;
        ow_enter_map(s->map, s->x / DOOMO_CELL_UNITS, s->y / DOOMO_CELL_UNITS);
    }
}

int story_debug_step(void)
{
    if (story_talking()) return 0;
    if (story_pending.kind != PEND_NONE && scene_top() == &SCENE_STORY) {
        story_resume();
        return 1;
    }
    return script_debug_step();
}

void story_mark_friends_dirty(void) { friends_dirty = 1; }
