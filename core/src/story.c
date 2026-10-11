/* story.c - the story state machine (owner: story): flags, chapters and
 * beats, conversations longer than the dialogue queue, sub-scenes the
 * plot starts (and what their results mean), rewards, friendship, the
 * debug hooks the scripted playthrough uses, and the dispatch of NPC /
 * trigger / place events to the main plot, the side quests and small talk.
 * The plot itself is in story_script.c.
 */
#include "story_int.h"

StoryPending story_pending;
int16_t story_fmt_target = -1;
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
    int i;
    if (finished < 1 || finished > 4) return;
    for (i = 0; i < 6; i++)
        if (songs[finished - 1][i]) unlock_grant(USTORY_SONG_FIRST + songs[finished - 1][i] - 1);
    unlock_grant(USTORY_CH1_DONE + finished - 1);
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
    script_enter_beat(beat);
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
    script_enter_beat(BEAT_C1_ARRIVE);
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

/* ---- formatting ------------------------------------------------------------------- */
static const char *const vape_flavours[] = {
    "Brimstone Blast", "Unholy Grape", "Cotton Candy Coffin", "Sulfur Sherbet",
    "Midnight Mango", "Graveyard Gum"
};

static int append(char *out, int cap, int n, const char *s)
{
    while (*s && n < cap - 1) out[n++] = *s++;
    return n;
}

void story_fmt(char *out, int cap, const char *t)
{
    int n = 0;
    const DoomoResident *r;
    if (cap <= 0) return;
    if (!t) t = "";
    while (*t && n < cap - 1) {
        if (t[0] == '@' && t[1]) {
            switch (t[1]) {
            case 'P': n = append(out, cap, n, story_player_name()); break;
            case 'E': n = append(out, cap, n, story_pet_name()); break;
            case 'D': {
                const char *d = story_fmt_target >= 0 ? sim_possessor_name(story_fmt_target) : "";
                n = append(out, cap, n, (d && d[0]) ? d : "a Hellvile demon");
                break; }
            case 'T':
                r = town_res(story_fmt_target);
                n = append(out, cap, n, (r && r->name[0]) ? r->name : "them");
                break;
            case 'V':
                n = append(out, cap, n, vape_flavours[doomo_town.day % DOOMO_COUNTOF(vape_flavours)]);
                break;
            case '@': out[n++] = '@'; break;
            default:
                out[n++] = t[0];
                if (n < cap - 1) out[n++] = t[1];
                break;
            }
            t += 2;
            continue;
        }
        out[n++] = *t++;
    }
    out[n] = 0;
}

/* ---- conversations ---------------------------------------------------------------- */
static const StoryLine *conv;
static int conv_i;
static void (*conv_done)(void);

void story_say(int who, const char *text)
{
    char buf[200];
    story_fmt(buf, sizeof(buf), text);
    ui_say(who, buf);
}

void story_say_then(void (*done)(void))
{
    if (ui_dialog_active()) ui_say_done(done);
    else if (done) done();
}

static void conv_feed(void)
{
    int n = 0;
    while (conv && conv[conv_i].text && n < 6) {
        story_say(conv[conv_i].who, conv[conv_i].text);
        conv_i++;
        n++;
    }
    if (conv && conv[conv_i].text) {
        ui_say_done(conv_feed);
    } else {
        void (*d)(void) = conv_done;
        conv = NULL;
        conv_done = NULL;
        if (n > 0) { if (d) ui_say_done(d); }
        else if (d) d();
    }
}

void story_say_lines(const StoryLine *lines, void (*done)(void))
{
    conv = lines;
    conv_i = 0;
    conv_done = done;
    conv_feed();
}

void story_choice(int who, const char *q, const char *const *opts, int n,
                  void (*done)(int))
{
    char buf[200];
    story_fmt(buf, sizeof(buf), q);
    ui_choice(who, buf, opts, n, done);
}

int story_talking(void) { return ui_dialog_active() || conv != NULL; }

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
    if (p.ctx == CTX_SIDE) side_resume(&p, result);
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
                    pend == PEND_OTHER)) {
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
            if (r && r->status == RES_POSSESSED) exorcism_apply(resident_id, 1);
        }
        dispatch_result();
        return;
    }
    scene_push(&SCENE_EXORCISM, resident_id);
}

void story_push_cutscene(int cut, int ctx, int extra)
{
    set_pending(PEND_CUTSCENE, ctx, cut, extra);
    ow_sync_to_save();
    scene_push(&SCENE_CUTSCENE, cut);
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
    if (script_npc(id)) return;
    if (side_npc(id)) return;
    talk_ambient(id);
}

void story_trigger(int tag)
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

const char *story_objective(void)
{
    if (doomo_town.story.chapter == STORY_CH_NONE) return "";
    return script_objective();
}

int story_target(int *map_id, int *cx, int *cy)
{
    return script_target(map_id, cx, cy);
}

void story_goto(int map_id, int cx, int cy)
{
    ow_enter_map(map_id, cx, cy);
}

/* ---- debug / tests ------------------------------------------------------------------- */
void story_debug_autowin(int on) { autowin = (uint8_t)(on != 0); }
int  story_debug_autowin_on(void) { return autowin; }

void story_debug_jump(int chapter, int beat)
{
    (void)chapter;
    if (beat < 0 || beat >= BEAT_COUNT) return;
    if (doomo_town.story.chapter == STORY_CH_NONE) story_new_game();
    friends_dirty = 1;
    script_debug_setup(beat);
    story_set_beat(beat);
    friends_dirty = 1;
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
