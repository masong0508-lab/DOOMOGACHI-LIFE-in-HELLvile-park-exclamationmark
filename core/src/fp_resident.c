/* fp_resident.c - SCENE_RESIDENT (arg = resident id): the care screen
 * (owner: freeplay).
 *
 * Tabs (L / R): Care (needs meters + every care / social action), About
 * (profile), Friends (relationships, sorted, with kind icons; cross-border
 * ones are marked) and Fun (activities that need a resident, e.g. Peek at
 * a dream, Put on a concert).
 *
 * Killing off is gated twice: "Lay to Rest" only exists when
 * sim_can_lay_to_rest() says so (a soul lost after 3 failed exorcisms, or
 * Monster Mode), needs two confirmations, and in Monster Mode the player
 * must type "I AM A MONSTER" first. Cute right up to the moment it isn't.
 */
#include "fp_int.h"

enum { TAB_CARE, TAB_ABOUT, TAB_FRIENDS, TAB_FUN, TAB_COUNT };
static const char *const tab_names[TAB_COUNT] = { "Care", "About", "Friends", "Fun" };


static const char *const act_label[FPA_COUNT] = {
    "Help!", "Feed", "Gift", "Clean", "Play", "Talk", "Heal", "Comfort", "Introduce",
    "Advice", "Edit look", "Favourite", "Exorcise", "Flowers", "Raise ghost", "Visit grave",
    "Their home", "Lay to rest", "Create here"
};
static const uint8_t act_icon[FPA_COUNT] = {
    ICON_CLOUD, ICON_FOOD, ICON_HEART, ICON_DROP, ICON_STAR, ICON_SMILE, ICON_CROSS,
    ICON_HEART, ICON_ARROW_R, ICON_SPARK, ICON_WRENCH, ICON_STAR, ICON_CROSS, ICON_SPARK,
    ICON_GHOST, ICON_SKULL, ICON_HOUSE, ICON_SKULL, ICON_EGG
};

enum { POP_NONE, POP_ITEMS };
enum { PH_NORMAL, PH_FAREWELL };

#define REL_MAX 64
#define ACTS_MAX 24

static struct {
    int16_t id;
    uint8_t tab, popup, phase, wait, item_mode;
    int16_t sel, top;                 /* care actions */
    uint8_t acts[FPA_COUNT];
    int     nacts;
    int16_t react_t, react_expr;
    uint8_t items[ITEM_COUNT];
    int     nitems;
    int16_t item_sel, item_top;
    uint16_t rel[REL_MAX];
    int     nrel;
    int16_t rel_sel, rel_top, rel_built_t;
    const DoomoActivity *fun[ACTS_MAX];
    int     nfun;
    int16_t fun_sel;
    int16_t about_scroll;
    int16_t farewell_t;
    int16_t tab_t;
    char    typed[24];
    int16_t pending_other;
} R;

static DoomoResident *me(void) { return fp_res(R.id); }

/* ====================================================================== */
/* building the lists                                                       */
static void build_actions(void)
{
    const DoomoResident *r = me();
    int n = 0, s;
    if (!r) { R.nacts = 0; return; }
    s = r->status;
#define ADD(a) (R.acts[n++] = (uint8_t)(a))
    if (s == RES_EMPTY) { ADD(FPA_CREATE); R.nacts = n; return; }
    if (sim_problem_for(R.id) >= 0 && s != RES_DEAD) ADD(FPA_HELP);
    switch (s) {
    case RES_ALIVE:
        ADD(FPA_FEED); ADD(FPA_GIFT); ADD(FPA_CLEAN); ADD(FPA_PLAY); ADD(FPA_TALK); ADD(FPA_HEAL);
        ADD(FPA_COMFORT); ADD(FPA_INTRODUCE); ADD(FPA_ADVICE); ADD(FPA_EDIT); ADD(FPA_FAV); ADD(FPA_HOME);
        break;
    case RES_POSSESSED:
        ADD(FPA_EXORCISE); ADD(FPA_TALK); ADD(FPA_FEED); ADD(FPA_HEAL); ADD(FPA_COMFORT); ADD(FPA_FAV);
        ADD(FPA_HOME);
        break;
    case RES_SOUL_LOST:
        ADD(FPA_EXORCISE); ADD(FPA_COMFORT); ADD(FPA_TALK); ADD(FPA_FAV);
        break;
    case RES_GHOST:
        ADD(FPA_TALK); ADD(FPA_PLAY); ADD(FPA_GIFT); ADD(FPA_COMFORT); ADD(FPA_INTRODUCE); ADD(FPA_FEED);
        ADD(FPA_EDIT); ADD(FPA_FAV); ADD(FPA_HOME);
        break;
    case RES_DEAD:
        ADD(FPA_FLOWERS); ADD(FPA_GHOST); ADD(FPA_GRAVE); ADD(FPA_FAV);
        break;
    default: break;
    }
    if (sim_can_lay_to_rest(R.id)) ADD(FPA_REST);
#undef ADD
    R.nacts = n;
    if (R.sel >= n) R.sel = (int16_t)(n ? n - 1 : 0);
}

static void build_rel(void)
{
    R.nrel = R.id >= 0 && R.id < DOOMO_MAX_RESIDENTS ? rel_list(R.id, R.rel, REL_MAX) : 0;
    if (R.rel_sel >= R.nrel) R.rel_sel = (int16_t)(R.nrel ? R.nrel - 1 : 0);
    R.rel_built_t = (int16_t)(fp_t() & 0x7FFF);
}

static void build_fun(void)
{
    int i, n = activity_count();
    R.nfun = 0;
    for (i = 0; i < n && R.nfun < ACTS_MAX; i++) {
        const DoomoActivity *a = activity_at(i);
        if (!a || !(a->flags & ACTF_NEEDS_RESIDENT) || !fp_activity_visible(a)) continue;
        R.fun[R.nfun++] = a;
    }
    if (R.fun_sel >= R.nfun) R.fun_sel = 0;
}

static void react(int expr, int fx, int n)
{
    int big = fp_big();
    int px = big ? 4 + 51 : 4 + 27, py = big ? 22 + 50 : 18 + 30;
    R.react_t = 70;
    R.react_expr = (int16_t)expr;
    if (fx >= 0) fp_fx_burst(px, py, fx, n);
}

/* ====================================================================== */
/* item popup                                                               */
static int item_mode_ok(int item)
{
    int c = doomo_items[item].category;
    switch (R.item_mode) {
    case FPA_FEED: return c == ICAT_FOOD;
    case FPA_GIFT: return c == ICAT_GIFT;
    case FPA_HEAL: return c == ICAT_CARE || c == ICAT_EXORCISM;
    default: return 0;
    }
}

static void open_items(int mode)
{
    int i;
    R.item_mode = (uint8_t)mode;
    R.nitems = 0;
    for (i = 1; i < ITEM_COUNT; i++)
        if (item_mode_ok(i) && (inv_count(i) > 0 || opt_get(OPT_CHEAT_INFINITE_CANDY)))
            R.items[R.nitems++] = (uint8_t)i;
    R.item_sel = 0;
    R.item_top = 0;
    R.popup = POP_ITEMS;
    R.tab_t = 0;
    audio_sfx(SFX_SELECT);
}

static void use_item(int item)
{
    const DoomoResident *r = me();
    const char *line = "";
    int fi = item - ITEM_CUPCAKE;
    int fav = r && doomo_items[item].category == ICAT_FOOD && fi == r->like_food;
    int hate = r && doomo_items[item].category == ICAT_FOOD && fi == r->hate_food;
    int creepy = r && (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST);
    R.popup = POP_NONE;
    switch (R.item_mode) {
    case FPA_FEED:
        line = sim_feed(R.id, item);
        audio_sfx(SFX_EAT);
        if (creepy) react(EXPR_POSSESSED, FX_SKULL, 4);
        else if (fav) { react(EXPR_LOVE, FX_HEART, 10); audio_sfx(SFX_LOVE); fp_achieve(UL_FP_FAV_FOOD); }
        else if (hate) { react(EXPR_SICK, FX_STINK, 8); audio_sfx(SFX_SAD); fp_achieve(UL_FP_HATED_FOOD); }
        else react(EXPR_HAPPY, FX_STAR, 6);
        break;
    case FPA_GIFT:
        line = sim_gift(R.id, item);
        audio_sfx(SFX_HAPPY);
        react(creepy ? EXPR_POSSESSED : EXPR_LOVE, creepy ? FX_SKULL : FX_HEART, 8);
        break;
    case FPA_HEAL:
        line = sim_heal(R.id, item);
        audio_sfx(doomo_items[item].holy ? SFX_HOLY : SFX_SPARKLE);
        react(creepy ? EXPR_ANGRY : EXPR_HAPPY, FX_SPARK, 8);
        break;
    default: break;
    }
    ui_say(R.id, line);
}

static void items_update(void)
{
    int vis = fp_big() ? 9 : 6;
    if (R.nitems == 0) {
        if (btn_pressed(BTN_A)) {
            R.popup = POP_NONE;
            audio_sfx(SFX_SELECT);
            scene_push(&SCENE_SHOP, RES_DISTRICT(R.id) ? FP_SHOP_ARG_BLACK_MARKET : FP_SHOP_ARG_SHOP);
            return;
        }
        if (btn_pressed(BTN_B)) { R.popup = POP_NONE; audio_sfx(SFX_CANCEL); }
        return;
    }
    if (btn_repeat(BTN_UP)) { R.item_sel = (int16_t)((R.item_sel + R.nitems - 1) % R.nitems); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { R.item_sel = (int16_t)((R.item_sel + 1) % R.nitems); audio_sfx(SFX_CURSOR); }
    if (R.item_sel < R.item_top) R.item_top = R.item_sel;
    if (R.item_sel >= R.item_top + vis) R.item_top = (int16_t)(R.item_sel - vis + 1);
    if (btn_pressed(BTN_A)) { audio_sfx(SFX_SELECT); use_item(R.items[R.item_sel]); return; }
    if (btn_pressed(BTN_B)) { R.popup = POP_NONE; audio_sfx(SFX_CANCEL); }
}

/* ====================================================================== */
/* advice & problems                                                        */
static int16_t adv_target[4];
static uint8_t adv_kind[4];
static int adv_n;
static char adv_txt[4][28];
static const char *adv_opts[5];
enum { ADV_CONFESS, ADV_PROPOSE, ADV_BREAKUP, ADV_MAKEUP };

static void cb_advice(int c)
{
    const char *line = "";
    if (c < 0 || c >= adv_n) return;
    switch (adv_kind[c]) {
    case ADV_CONFESS:
        line = sim_advise_confess(R.id, adv_target[c], 1);
        if (rel_kind(R.id, adv_target[c]) == REL_DATING) fp_achieve(UL_FP_MATCHMAKER);
        break;
    case ADV_PROPOSE: line = sim_advise_propose(R.id, 1); break;
    case ADV_BREAKUP: line = sim_advise_breakup(R.id, 0); break;
    case ADV_MAKEUP: line = sim_make_up(R.id, adv_target[c]); break;
    default: return;
    }
    react(EXPR_LOVE, FX_HEART, 6);
    ui_say(R.id, line);
}

static void open_advice(void)
{
    uint16_t out[24];
    int n, i, partner = sim_partner_of(R.id), crush = -1, foe = -1, foe_v = 0;
    adv_n = 0;
    n = rel_list(R.id, out, 24);
    for (i = 0; i < n; i++) {
        int o = out[i], k = rel_kind(R.id, o);
        if (o >= DOOMO_MAX_RESIDENTS || !fp_alive(o)) continue;
        if (crush < 0 && rel_has_crush(R.id, o) && o != partner) crush = o;
        if ((k == REL_RIVAL || k == REL_NEMESIS || rel_value(R.id, o) < -20) &&
            rel_value(R.id, o) < foe_v) { foe = o; foe_v = rel_value(R.id, o); }
    }
    if (crush >= 0) {
        adv_kind[adv_n] = ADV_CONFESS; adv_target[adv_n] = (int16_t)crush;
        doomo_snprintf(adv_txt[adv_n], 28, "Confess to %s", doomo_town.res[crush].name);
        adv_n++;
    }
    if (partner >= 0 && partner < DOOMO_MAX_RESIDENTS && rel_kind(R.id, partner) == REL_DATING) {
        adv_kind[adv_n] = ADV_PROPOSE; adv_target[adv_n] = (int16_t)partner;
        doomo_snprintf(adv_txt[adv_n], 28, "Propose to %s", doomo_town.res[partner].name);
        adv_n++;
    }
    if (partner >= 0 && partner < DOOMO_MAX_RESIDENTS) {
        adv_kind[adv_n] = ADV_BREAKUP; adv_target[adv_n] = (int16_t)partner;
        doomo_snprintf(adv_txt[adv_n], 28, "Break up with %s", doomo_town.res[partner].name);
        adv_n++;
    }
    if (foe >= 0) {
        adv_kind[adv_n] = ADV_MAKEUP; adv_target[adv_n] = (int16_t)foe;
        doomo_snprintf(adv_txt[adv_n], 28, "Make up with %s", doomo_town.res[foe].name);
        adv_n++;
    }
    if (!adv_n) {
        ui_say(R.id, "Advice? Hmm... I think I'm doing okay right now! But thanks!");
        return;
    }
    for (i = 0; i < adv_n; i++) adv_opts[i] = adv_txt[i];
    adv_opts[adv_n] = "Never mind";
    ui_choice(R.id, "What do you think I should do?", adv_opts, adv_n + 1, cb_advice);
}

static int help_index;
static void cb_help_choice(int c)
{
    const DoomoProblem *p = sim_problem(help_index);
    const char *line = "";
    if (!p || p->resident != R.id) return;
    switch (p->kind) {
    case PROB_CRUSH: case PROB_CONFESS_HELP: {
        int other = p->other;
        line = sim_advise_confess(R.id, other, c == 0);
        if (c == 0 && rel_kind(R.id, other) == REL_DATING) fp_achieve(UL_FP_MATCHMAKER);
        break; }
    case PROB_BREAKUP: line = sim_advise_breakup(R.id, c == 0); break;
    case PROB_PROPOSAL: line = sim_advise_propose(R.id, c == 0); break;
    default: return;
    }
    react(c == 0 ? EXPR_LOVE : EXPR_SAD, c == 0 ? FX_HEART : FX_TEAR, 6);
    ui_say(R.id, line);
    fp_achieve(UL_FP_HELPER);
}

static void do_help(void)
{
    char buf[64];
    const DoomoProblem *p;
    help_index = sim_problem_for(R.id);
    p = sim_problem(help_index);
    if (!p) return;
    sim_problem_text(help_index, buf, sizeof(buf));
    switch (p->kind) {
    case PROB_CRUSH: case PROB_CONFESS_HELP: {
        static const char *const o[2] = { "Go for it!", "Better not..." };
        ui_choice(R.id, "Should I tell them how I feel?", o, 2, cb_help_choice);
        return; }
    case PROB_BREAKUP: {
        static const char *const o[2] = { "Work it out", "Let them go" };
        ui_choice(R.id, "We keep fighting... what should I do?", o, 2, cb_help_choice);
        return; }
    case PROB_PROPOSAL: {
        static const char *const o[2] = { "Pop the question!", "Take it slow" };
        ui_choice(R.id, "I want to propose... is it too soon?", o, 2, cb_help_choice);
        return; }
    case PROB_FIGHT:
        if (p->other != 0xFFFF) {
            ui_say(R.id, sim_make_up(R.id, p->other));
            react(EXPR_HAPPY, FX_HEART, 6);
            fp_achieve(UL_FP_HELPER);
        }
        return;
    case PROB_POSSESSED:
        R.wait = FP_WAIT_EXORCISM;
        scene_push(&SCENE_EXORCISM, R.id);
        return;
    case PROB_HUNGRY:
        open_items(FPA_FEED);
        return;
    default: break;
    }
    {
        uint32_t m0 = doomo_town.money;
        sim_problem_resolve(help_index);
        ui_say(R.id, sim_last_reaction());
        if (sim_problem_for(R.id) < 0) fp_achieve(UL_FP_HELPER);
        if (doomo_town.money > m0) {
            doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " +%u H$ thank-you tip!",
                           (unsigned)(doomo_town.money - m0));
            ui_toast(buf);
            react(EXPR_HAPPY, FX_COIN, 6);
            audio_sfx(SFX_HAPPY);
        } else {
            react(EXPR_HAPPY, FX_STAR, 4);
        }
    }
}

/* ====================================================================== */
/* lay to rest (gated, double confirmed)                                     */
static void begin_farewell(void)
{
    int monster = doomo_town.res[R.id].status != RES_SOUL_LOST;
    sim_lay_to_rest(R.id, monster ? DEATH_MONSTER : DEATH_LAID_TO_REST);
    R.phase = PH_FAREWELL;
    R.farewell_t = 0;
    R.popup = POP_NONE;
    audio_music(SONG_SAD);
    audio_sfx(monster ? SFX_POSSESS : SFX_BELL);
    fp_fx_clear();
}

static void cb_rest_final(int yes)
{
    if (!yes) { ui_toast(STR_ICON_HEART " Phew... not today."); return; }
    if (!sim_can_lay_to_rest(R.id)) return;
    begin_farewell();
}

static void cb_rest_first(int yes)
{
    if (!yes) return;
    doomo_snprintf(fp_tmp, sizeof(fp_tmp),
                   "Really? %s will be gone forever, and their friends will grieve.",
                   doomo_town.res[R.id].name);
    ui_confirm(fp_tmp, cb_rest_final);
}

static int monster_typed_ok(const char *s)
{
    static const char want[] = "IAMAMONSTER";
    int i = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c == ' ') continue;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (!want[i] || c != want[i]) return 0;
        i++;
    }
    return want[i] == 0;
}

static void cb_monster_typed(int ok)
{
    if (!ok) return;
    if (!monster_typed_ok(R.typed)) {
        audio_sfx(SFX_ERROR);
        ui_toast("That's not what a monster would type.");
        return;
    }
    audio_sfx(SFX_DEMON_ROAR);
    doomo_snprintf(fp_tmp, sizeof(fp_tmp),
                   "%s has done nothing wrong. Kill them off anyway? Everyone will remember.",
                   doomo_town.res[R.id].name);
    ui_confirm(fp_tmp, cb_rest_final);
}

static void do_rest(void)
{
    const DoomoResident *r = me();
    if (!r || !sim_can_lay_to_rest(R.id)) { audio_sfx(SFX_ERROR); return; }
    if (r->status == RES_SOUL_LOST) {
        doomo_snprintf(fp_tmp, sizeof(fp_tmp),
                       "%s's soul is lost after %d failed exorcisms. Lay them to rest?",
                       r->name, RES_EXO_FAILS(r));
        ui_confirm(fp_tmp, cb_rest_first);
    } else {
        R.typed[0] = 0;
        ui_text_entry("MONSTER MODE: type I AM A MONSTER", R.typed, sizeof(R.typed),
                      cb_monster_typed);
    }
}

/* ====================================================================== */
/* other actions                                                            */
static void cb_introduce(int other)
{
    if (other < 0) return;
    ui_say(R.id, sim_introduce(R.id, other));
    react(EXPR_HAPPY, FX_HEART, 5);
    audio_sfx(SFX_HAPPY);
}

static void raise_ghost(void)
{
    if (fp_ghost_rite(R.id)) react(EXPR_HAPPY, FX_SPARK, 14);
}

static void do_action(int a)
{
    const DoomoResident *r = me();
    const char *line;
    if (!r) return;
    switch (a) {
    case FPA_HELP: do_help(); break;
    case FPA_FEED: case FPA_GIFT: case FPA_HEAL: open_items(a); break;
    case FPA_CLEAN:
        line = sim_clean(R.id);
        audio_sfx(SFX_SPLASH);
        react(r->status == RES_POSSESSED ? EXPR_POSSESSED : EXPR_HAPPY, FX_BUBBLE, 12);
        ui_say(R.id, line);
        break;
    case FPA_PLAY:
        line = sim_play(R.id);
        audio_sfx(SFX_HAPPY);
        react(r->status == RES_POSSESSED ? EXPR_POSSESSED : EXPR_HAPPY, FX_NOTE, 8);
        ui_say(R.id, line);
        break;
    case FPA_TALK:
        line = sim_talk(R.id);
        ui_say(R.id, line);
        if (r->status == RES_ALIVE && (fp_t() & 3) == 0)
            ui_say(R.id, doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT]);
        break;
    case FPA_COMFORT:
        line = sim_comfort(R.id);
        audio_sfx(SFX_LOVE);
        react(r->status == RES_ALIVE ? EXPR_LOVE : EXPR_NEUTRAL, FX_HEART, 6);
        ui_say(R.id, line);
        break;
    case FPA_INTRODUCE:
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Introduce %s to...", r->name);
        fp_pick(fp_tmp, PICK_LIVING, R.id, cb_introduce);
        break;
    case FPA_ADVICE: open_advice(); break;
    case FPA_EDIT: case FPA_CREATE:
        R.wait = FP_WAIT_CREATOR;
        audio_sfx(SFX_SELECT);
        scene_push(&SCENE_CREATOR, R.id);
        break;
    case FPA_FAV:
        doomo_town.res[R.id].flags ^= RF_FAVORITE;
        if (doomo_town.res[R.id].flags & RF_FAVORITE) {
            audio_sfx(SFX_SPARKLE);
            react(EXPR_LOVE, FX_STAR, 8);
            ui_toast(STR_ICON_STAR " Added to favourites!");
            fp_check_island_achievements();
        } else {
            audio_sfx(SFX_BLIP);
            ui_toast("Removed from favourites.");
        }
        break;
    case FPA_EXORCISE:
        R.wait = FP_WAIT_EXORCISM;
        audio_sfx(SFX_BELL);
        scene_push(&SCENE_EXORCISM, R.id);
        break;
    case FPA_FLOWERS: {
        int f = fp_flowers(R.id);
        if (f >= 3) { ui_toast("The grave is covered in flowers already."); break; }
        if (inv_count(ITEM_FLOWERS) <= 0 && !opt_get(OPT_CHEAT_INFINITE_CANDY)) {
            ui_toast("You need Flowers (sold at the shop).");
            audio_sfx(SFX_ERROR);
            break;
        }
        if (!opt_get(OPT_CHEAT_INFINITE_CANDY)) inv_take(ITEM_FLOWERS, 1);
        fp_set_flowers(R.id, f + 1);
        fp_fx_burst(fp_big() ? 55 : 31, fp_big() ? 72 : 48, FX_PETAL, 12);
        audio_sfx(SFX_SPARKLE);
        ui_toast(STR_ICON_HEART " You leave a little posy.");
        fp_achieve(UL_FP_GARDENER);
        break; }
    case FPA_GHOST: raise_ghost(); break;
    case FPA_GRAVE: scene_push(&SCENE_GRAVEYARD, 0); break;
    case FPA_HOME: scene_push(&SCENE_APARTMENTS, 2 + R.id); break;
    case FPA_REST: do_rest(); break;
    default: break;
    }
}

/* ====================================================================== */
/* scene                                                                    */
static void set_resident(int id)
{
    R.id = (int16_t)id;
    R.sel = 0;
    R.top = 0;
    R.react_t = 0;
    R.popup = POP_NONE;
    R.about_scroll = 0;
    R.rel_sel = 0;
    R.rel_top = 0;
    R.tab_t = 0;
    build_actions();
    build_rel();
    build_fun();
}

static void res_enter(int arg)
{
    memset(&R, 0, sizeof(R));
    R.tab = TAB_CARE;
    set_resident(DOOMO_CLAMP(arg, 0, DOOMO_MAX_RESIDENTS - 1));
}

static void res_leave(void)
{
    if (R.phase == PH_FAREWELL) audio_music(fp_island_song());
}

static void leave(void)
{
    if (scene_depth() > 1) scene_pop();
    else fp_goto_island(FP_ISLAND_ARG_RETURN);
}

static int list_vis(void) { return fp_big() ? 13 : 9; }

static void care_update(void)
{
    int vis = list_vis();
    if (!R.nacts) return;
    if (btn_repeat(BTN_UP)) { R.sel = (int16_t)((R.sel + R.nacts - 1) % R.nacts); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { R.sel = (int16_t)((R.sel + 1) % R.nacts); audio_sfx(SFX_CURSOR); }
    if (R.sel < R.top) R.top = R.sel;
    if (R.sel >= R.top + vis) R.top = (int16_t)(R.sel - vis + 1);
    if (btn_pressed(BTN_A)) do_action(R.acts[R.sel]);
}

static void friends_update(void)
{
    int vis = list_vis();
    if (!R.nrel) return;
    if (btn_repeat(BTN_UP)) { R.rel_sel = (int16_t)((R.rel_sel + R.nrel - 1) % R.nrel); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { R.rel_sel = (int16_t)((R.rel_sel + 1) % R.nrel); audio_sfx(SFX_CURSOR); }
    if (R.rel_sel < R.rel_top) R.rel_top = R.rel_sel;
    if (R.rel_sel >= R.rel_top + vis) R.rel_top = (int16_t)(R.rel_sel - vis + 1);
    if (btn_pressed(BTN_A)) {
        int o = R.rel[R.rel_sel];
        if (o < DOOMO_MAX_RESIDENTS && doomo_town.res[o].status != RES_EMPTY) {
            audio_sfx(SFX_SELECT);
            set_resident(o);
            R.tab = TAB_CARE;
        } else {
            audio_sfx(SFX_ERROR);
        }
    }
}

static void fun_update(void)
{
    if (!R.nfun) return;
    if (btn_repeat(BTN_UP)) { R.fun_sel = (int16_t)((R.fun_sel + R.nfun - 1) % R.nfun); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { R.fun_sel = (int16_t)((R.fun_sel + 1) % R.nfun); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A)) {
        const DoomoActivity *a = R.fun[R.fun_sel];
        if (!activity_available(a) || !fp_alive(R.id)) {
            audio_sfx(SFX_ERROR);
            ui_toast(fp_alive(R.id) ? fp_activity_reason(a) : "They can't join in right now.");
            return;
        }
        fp_launch_with(a, R.id);
    }
}

static void res_update(void)
{
    const DoomoResident *r;
    fp_live();
    if (R.react_t > 0) R.react_t--;
    R.tab_t++;
    if (R.phase == PH_FAREWELL) {
        R.farewell_t++;
        if (R.farewell_t % 9 == 0) fp_fx_burst(fp_sw() / 2 + rng_between(&doomo_rng, -60, 60), 0, FX_PETAL, 1);
        if (R.farewell_t > 40 && btn_pressed(BTN_A | BTN_B | BTN_START)) {
            audio_music(fp_island_song());
            R.phase = PH_NORMAL;
            audio_sfx(SFX_SELECT);
            if (scene_depth() > 1) scene_pop();
            scene_push(&SCENE_GRAVEYARD, 0);
        }
        return;
    }
    if (R.wait) {
        R.wait = FP_WAIT_NONE;
        portrait_invalidate(R.id);
        build_rel();
    }
    build_actions();
    if ((fp_t() & 63) == 0) build_rel();
    r = me();
    if (R.popup == POP_ITEMS) { items_update(); return; }
    if (btn_pressed(BTN_L)) { R.tab = (uint8_t)((R.tab + TAB_COUNT - 1) % TAB_COUNT); R.tab_t = 0; audio_sfx(SFX_BLIP); return; }
    if (btn_pressed(BTN_R)) { R.tab = (uint8_t)((R.tab + 1) % TAB_COUNT); R.tab_t = 0; audio_sfx(SFX_BLIP); return; }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); leave(); return; }
    if (btn_pressed(BTN_SELECT) && r && r->status != RES_EMPTY) {
        do_action(FPA_TALK);
        return;
    }
    switch (R.tab) {
    case TAB_CARE: care_update(); break;
    case TAB_ABOUT:
        if (btn_repeat(BTN_UP) && R.about_scroll > 0) { R.about_scroll--; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN) && R.about_scroll < 14) { R.about_scroll++; audio_sfx(SFX_CURSOR); }
        break;
    case TAB_FRIENDS: friends_update(); break;
    default: fun_update(); break;
    }
}

/* ====================================================================== */
/* drawing                                                                  */
typedef struct { int px, py, scale, pw, ph, cx, cy, cw, ch; } RLayout;

static void rlayout(RLayout *L)
{
    int sw = fp_sw(), sh = fp_sh();
    L->scale = fp_big() ? 2 : 1;
    L->pw = PORTRAIT_W * L->scale + 6;
    L->ph = PORTRAIT_H * L->scale + 6;
    L->px = 4;
    L->py = fp_big() ? 22 : 18;
    L->cx = L->px + L->pw + 6;
    L->cy = L->py - 1;
    L->cw = sw - L->cx - 4;
    L->ch = sh - L->cy - 17;
}

static int window_style(const DoomoResident *r)
{
    if (!r) return WIN_CUTE;
    switch (r->status) {
    case RES_POSSESSED: return WIN_HELL;
    case RES_SOUL_LOST: case RES_DEAD: return WIN_DARK;
    case RES_GHOST: return WIN_MINT;
    default: return RES_DISTRICT(R.id) ? WIN_CREAM : WIN_CUTE;
    }
}

static void draw_hearts(int x, int y, int level, uint8_t c)
{
    int i, full = level / 20, t = fp_t();
    for (i = 0; i < 5; i++) {
        int b = (i == full && level % 20) ? (ui_wave(t + i * 4, 30, 1) > 0) : 0;
        ui_heart(x + i * 10 + 4, y + 4 - b, 4, i < full ? c : (i == full && level % 20 >= 10) ?
                 ui_lighter(c) : PAL_RAMP(RAMP_STONE, 5));
    }
}

static void draw_left(const RLayout *L, const DoomoResident *r, int style)
{
    const UiStyle *st = ui_style(style);
    int expr = R.react_t > 0 ? R.react_expr : fp_mood_expr(r);
    int y, t = fp_t(), w = L->pw;
    char buf[40];
    if (r->status == RES_EMPTY) {
        fp_portrait(L->px, L->py, -1, L->scale, 0, WIN_MINT);
        ui_text_center(L->px + w / 2, L->py + L->ph / 2 - 4, "Empty", PAL_RAMP(RAMP_MINT, 2));
    } else {
        fp_portrait(L->px, L->py, R.id, L->scale, expr, style == WIN_HELL ? WIN_DARK : WIN_MINT);
    }
    if (r->flags & RF_FAVORITE) ui_star(L->px + w - 6, L->py + 6, 4, PAL_UI_GOLD);
    y = L->py + L->ph + 3;
    /* name plate */
    {
        int nw = DOOMO_MIN(ui_text_width(r->name) + 12, w + 4);
        int nx = L->px + (w - nw) / 2;
        fp_pill(nx, y, nw, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
        ui_text_shadow(nx + 6, y + 2, r->status == RES_EMPTY ? "Vacant" : r->name, st->title, st->outline);
        y += 14;
    }
    if (r->status == RES_EMPTY) return;
    draw_hearts(L->px + (w - 50) / 2, y, r->level, PP(PAINT_ROSE, 1));
    y += 11;
    if (fp_living_status(r->status)) {
        fp_icon(L->px + 2, y, fp_mood_icon(r->mood), fp_mood_color(r->mood));
        fp_marquee(L->px + 11, y, w - 11, doomo_mood_names[r->mood % MOOD_COUNT], st->text, t);
    } else {
        fp_icon(L->px + 2, y, fp_status_icon(r->status), fp_status_color(r->status));
        ui_text(L->px + 11, y, fp_status_name(r->status), st->text);
    }
    y += FONT_LINE;
    if (y + FONT_LINE < fp_sh() - 14) {
        doomo_snprintf(buf, sizeof(buf), "%s %d-%d", RES_DISTRICT(R.id) ? "Pits" : "Towers",
                       fp_floor(R.id), fp_room(R.id));
        fp_icon(L->px + 2, y, ICON_HOUSE, RES_DISTRICT(R.id) ? PP(PAINT_VAPE_PURPLE, 1) : PP(PAINT_BUBBLEGUM, 0));
        ui_text(L->px + 11, y, buf, st->text_dim);
    }
}

static void draw_problem(int x, int y, int w)
{
    int pi = sim_problem_for(R.id), t = fp_t();
    char buf[64];
    if (pi < 0) return;
    sim_problem_text(pi, buf, sizeof(buf));
    fp_pill(x, y, w, 12, PAL_WHITE, PAL_UI_RED, PAL_WHITE);
    fp_icon(x + 4, y + 2 - (ui_wave(t, 30, 1) > 0), ICON_CLOUD, PAL_UI_RED);
    fp_marquee(x + 14, y + 2, w - 18, buf, PAL_UI_RED_D, t);
}

static void draw_care(const RLayout *L, const DoomoResident *r, int style)
{
    const UiStyle *st = ui_style(style);
    int vis = list_vis(), i, lw = fp_big() ? 104 : 80, x = L->cx + 6, y = L->cy + 8;
    int mx = L->cx + lw + 10, mw = L->cw - lw - 16, t = fp_t();
    int has_prob = sim_problem_for(R.id) >= 0;
    if (has_prob) vis--;
    for (i = 0; i < vis && R.top + i < R.nacts; i++) {
        int a = R.acts[R.top + i], ry = y + i * 11, sel = R.top + i == R.sel;
        uint8_t tc = st->text, ic = st->accent;
        const char *label = act_label[a];
        if (a == FPA_FAV && (r->flags & RF_FAVORITE)) label = "Unfavourite";
        if (a == FPA_REST) { tc = PAL_UI_RED_D; ic = PAL_UI_RED; }
        if (a == FPA_HELP) { tc = PAL_UI_RED; ic = PAL_UI_RED; }
        if (sel) {
            uint8_t f = a == FPA_REST ? PAL_RAMP(RAMP_BLOOD, 2) : st->accent_d;
            fp_pill(x + 8, ry - 1, lw - 8, 11, f, ui_darker(f), ui_lighter(f));
            ui_cursor(x - 1, ry + 1);
            if (a == FPA_REST) tc = PAL_WHITE;
        }
        fp_icon(x + 11, ry + 1 - (sel && ui_wave(t, 20, 1) > 0), act_icon[a], ic);
        ui_text(x + 20, ry + 1, label, tc);
    }
    if (R.nacts > vis)
        ui_scroll_arrows(x + lw - 6, y - 6, y + vis * 11 - 2, R.top > 0, R.top + vis < R.nacts);
    /* needs meters */
    if (r->status == RES_DEAD) {
        int gy = L->cy + 14;
        ui_text(mx, gy, "Resting in", st->text_dim);
        ui_text(mx, gy + 10, "peace.", st->text_dim);
        ui_text(mx, gy + 26, doomo_death_names[r->death_cause % DEATH_COUNT], st->text);
        {
            int f = fp_flowers(R.id), k;
            for (k = 0; k < f; k++) ui_heart(mx + 4 + k * 10, gy + 44, 3, PP(PAINT_BUBBLEGUM, 1));
        }
    } else if (r->status != RES_EMPTY) {
        int s, rows = STAT_COUNT, rh = fp_big() ? 15 : 12;
        for (s = 0; s < rows; s++) {
            int v = r->stat[s];
            int ry = L->cy + 7 + s * rh;
            if (fp_big()) fp_meter(mx, ry, mw, fp_stat_icon(s), fp_stat_names[s], v,
                                   fp_stat_color(s, v), st->text);
            else {
                fp_icon(mx, ry, fp_stat_icon(s), fp_stat_color(s, v));
                ui_bar(mx + 9, ry + 1, mw - 9, 7, v, 100, fp_stat_color(s, v));
            }
        }
    }
    if (has_prob) draw_problem(L->cx + 4, L->cy + L->ch - 15, L->cw - 8);
}

static int about_line(int i, char *buf, int cap, uint8_t *icon)
{
    const DoomoResident *r = me();
    int m = r->bday_month % 12;
    *icon = ICON_STAR;
    switch (i) {
    case 0: *icon = ICON_WRENCH; doomo_snprintf(buf, cap, "Job: %s", fp_job(R.id)); return 1;
    case 1: *icon = ICON_SMILE; doomo_snprintf(buf, cap, "Type: %s",
                                        doomo_persona_type_names[persona_type(r) % PERSONA_TYPE_COUNT]); return 1;
    case 2: *icon = ICON_SPARK; doomo_snprintf(buf, cap, "%s, %s", doomo_species_names[r->look.species % SPECIES_COUNT],
                                        doomo_pronoun_names[r->pronoun % PRONOUN_COUNT]); return 1;
    case 3: *icon = ICON_EGG; doomo_snprintf(buf, cap, "Birthday: %s %d", doomo_month_names[m], r->bday_day); return 1;
    case 4: *icon = ICON_HEART; doomo_snprintf(buf, cap, "Falls for: %s", doomo_romance_names[r->romance % ROMANCE_COUNT]); return 1;
    case 5: *icon = ICON_FOOD; doomo_snprintf(buf, cap, "Loves: %s", doomo_food_names[r->like_food % FOOD_COUNT]); return 1;
    case 6: *icon = ICON_X; doomo_snprintf(buf, cap, "Hates: %s", doomo_food_names[r->hate_food % FOOD_COUNT]); return 1;
    case 7: *icon = ICON_STAR; doomo_snprintf(buf, cap, "Hobby: %s", doomo_hobby_names[r->like_hobby % HOBBY_COUNT]); return 1;
    case 8: *icon = ICON_NOTE; doomo_snprintf(buf, cap, "Music: %s", doomo_music_names[r->like_music % MUSIC_COUNT]); return 1;
    case 9: *icon = ICON_DROP; doomo_snprintf(buf, cap, "Colour: %s", doomo_paint_names[r->like_color % PAL_PAINT_COUNT]); return 2;
    case 10: *icon = ICON_GHOST; doomo_snprintf(buf, cap, "Fear: %s", doomo_fear_names[r->fear % FEAR_COUNT]); return 1;
    case 11: *icon = ICON_HOUSE; doomo_snprintf(buf, cap, "Born in %s, lives in %s",
                                         (r->flags & RF_HELLBORN) ? "Hellvile" : "Hillvile",
                                         fp_district_name(RES_DISTRICT(R.id))); return 1;
    case 12: *icon = ICON_CROSS; doomo_snprintf(buf, cap, "Exorcisms: %d tried, %d failed",
                                         RES_EXO_ATTEMPTS(r), RES_EXO_FAILS(r)); return 1;
    case 13:
        if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
            *icon = ICON_PENTAGRAM;
            doomo_snprintf(buf, cap, "Possessed by: %s", sim_possessor_name(R.id));
            return 1;
        }
        *icon = ICON_SPARK;
        doomo_snprintf(buf, cap, "Friendship with you: %d", r->level);
        return 1;
    case 14: *icon = ICON_BOLT; doomo_snprintf(buf, cap, "Made by: %s", (r->flags & RF_CUSTOM) ? "you" : "HaHaland"); return 1;
    default: return 0;
    }
}

static void draw_about(const RLayout *L, const DoomoResident *r, int style)
{
    const UiStyle *st = ui_style(style);
    int x = L->cx + 7, y = L->cy + 7, w = L->cw - 14, i, lines, maxy = L->cy + L->ch - 6;
    char buf[64];
    if (r->status == RES_EMPTY) {
        ui_text_wrap(x, y, w, "An empty, cozy apartment. Make a citizen to move in!", st->text, 1);
        return;
    }
    /* bio + catchphrase card */
    lines = ui_text_wrap(x, y, w, fp_bio(R.id), st->text, 0);
    if (R.about_scroll == 0) {
        ui_text_wrap(x, y, w, fp_bio(R.id), st->text, 1);
        y += lines * FONT_LINE + 2;
        doomo_snprintf(buf, sizeof(buf), "\"%s\"", doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT]);
        fp_marquee(x, y, w, buf, st->accent, fp_t());
        y += FONT_LINE + 3;
    }
    for (i = R.about_scroll > 0 ? R.about_scroll - 1 : 0; y + FONT_LINE <= maxy; i++) {
        uint8_t icon;
        int k = about_line(i, buf, sizeof(buf), &icon);
        if (!k) break;
        fp_icon(x, y, icon, st->accent);
        fp_marquee(x + 9, y, w - 9 - (k == 2 ? 10 : 0), buf, st->text, fp_t() + i * 20);
        if (k == 2) {
            ui_round_rect(x + w - 8, y, 8, 8, 2, PP(r->like_color % 32, 0));
            ui_round_rect(x + w - 7, y + 1, 6, 6, 1, PP(r->like_color % 32, 1));
        }
        y += FONT_LINE + 1;
    }
    {
        uint8_t dummy;
        ui_scroll_arrows(x + w - 4, L->cy + 2, maxy - 2, R.about_scroll > 0,
                         about_line(i, buf, sizeof(buf), &dummy));
    }
}

static void draw_friends(const RLayout *L, int style)
{
    const UiStyle *st = ui_style(style);
    int vis = list_vis(), i, x = L->cx + 6, y = L->cy + 8, w = L->cw - 12, t = fp_t();
    if (!R.nrel) {
        ui_text_wrap(x + 4, y + 10, w - 8, "No friends yet... Introduce them to someone!", st->text_dim, 1);
        ui_heart(x + w / 2, y + 44 + ui_wave(t, 40, 2), 5, st->accent);
        return;
    }
    for (i = 0; i < vis && R.rel_top + i < R.nrel; i++) {
        int o = R.rel[R.rel_top + i], ry = y + i * 11, sel = R.rel_top + i == R.rel_sel;
        int k = rel_kind(R.id, o), v = rel_value(R.id, o);
        const DoomoResident *ro = town_res(o);
        int cross = o < DOOMO_MAX_RESIDENTS && RES_DISTRICT(o) != RES_DISTRICT(R.id);
        int bw = fp_big() ? 50 : 30, bx = x + w - bw - 2;
        char nm[DOOMO_NAME_LEN + 1];
        if (sel) {
            fp_pill(x + 8, ry - 1, w - 8, 11, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x - 1, ry + 1);
        }
        fp_icon(x + 11, ry + 1, fp_rel_icon(k), fp_rel_color(k));
        fp_strcpy(nm, ro ? (o == DOOMO_PLAYER_ID ? "You" : ro->name) : "?", sizeof(nm));
        ui_text(x + 20, ry + 1, nm, ro && ro->status == RES_DEAD ? st->text_dim : st->text);
        if (rel_has_crush(R.id, o)) ui_heart(x + 20 + ui_text_width(nm) + 5, ry + 4, 2, PP(PAINT_ROSE, 1));
        if (cross) {
            /* a tiny brick wall: across the border */
            int cx = bx - 9;
            plat_rect(cx, ry + 2, 7, 6, PAL_RAMP(RAMP_STONE, 3));
            plat_rect(cx, ry + 4, 7, 1, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(cx + 3, ry + 2, 1, 2, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(cx + 1, ry + 5, 1, 3, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(cx + 5, ry + 5, 1, 3, PAL_RAMP(RAMP_STONE, 5));
        }
        /* value: -100..100 as a two-sided bar */
        {
            int half = bw / 2, len = v * half / 100;
            plat_rect(bx, ry + 3, bw, 4, PAL_RAMP(RAMP_STONE, 3));
            plat_rect(bx + half, ry + 2, 1, 6, st->outline);
            if (len > 0) plat_rect(bx + half + 1, ry + 3, len, 4, PP(PAINT_ROSE, 1));
            else if (len < 0) plat_rect(bx + half + len, ry + 3, -len, 4, PP(PAINT_GRAPE, 1));
        }
    }
    if (R.nrel > vis)
        ui_scroll_arrows(x + w - 2, y - 6, y + vis * 11 - 2, R.rel_top > 0, R.rel_top + vis < R.nrel);
    {
        int o = R.rel[R.rel_sel], k = rel_kind(R.id, o);
        char buf[48];
        doomo_snprintf(buf, sizeof(buf), "%s%s", k >= 0 ? doomo_rel_kind_names[k] : "?",
                       o < DOOMO_MAX_RESIDENTS && RES_DISTRICT(o) != RES_DISTRICT(R.id) ? " (across the Wall!)" : "");
        ui_text(x + 2, L->cy + L->ch - 13, buf, st->accent);
    }
}

static void draw_fun(const RLayout *L, int style)
{
    const UiStyle *st = ui_style(style);
    int i, x = L->cx + 6, y = L->cy + 8, w = L->cw - 12, t = fp_t();
    if (!R.nfun) {
        ui_text_center(x + w / 2, y + 24, "Coming soon!", st->accent);
        ui_text_wrap(x + 4, y + 38, w - 8, "New things to do together are on their way to the island.", st->text_dim, 1);
        ui_star(x + w / 2, y + 12 + ui_wave(t, 30, 2), 5, PAL_UI_GOLD);
        return;
    }
    for (i = 0; i < R.nfun && i < (fp_big() ? 12 : 8); i++) {
        const DoomoActivity *a = R.fun[i];
        int ry = y + i * 12, sel = i == R.fun_sel, ok = activity_available(a) && fp_alive(R.id);
        if (sel) {
            fp_pill(x + 8, ry - 1, w - 8, 11, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x - 1, ry + 1);
        }
        fp_icon(x + 11, ry + 1, a->icon, ok ? st->accent : st->text_dim);
        ui_text(x + 20, ry + 1, a->name, ok ? st->text : st->text_dim);
    }
    {
        const DoomoActivity *a = R.fun[R.fun_sel];
        const char *why = fp_alive(R.id) ? fp_activity_reason(a) : "They can't join in right now.";
        int by = L->cy + L->ch - 24;
        ui_text_wrap(x + 2, by, w - 4, why[0] ? why : a->desc, why[0] ? PAL_UI_RED : st->text_dim, 1);
    }
}

static void draw_items(int style)
{
    const UiStyle *st;
    const DoomoResident *r = me();
    int sw = fp_sw(), sh = fp_sh(), vis = fp_big() ? 9 : 6, i;
    int w = DOOMO_MIN(sw - 40, 200), h = vis * 12 + 34, x = (sw - w) / 2, y = (sh - h) / 2 - 4;
    const char *title = R.item_mode == FPA_FEED ? "Feed what?" : R.item_mode == FPA_GIFT ? "Give what?" : "Which remedy?";
    plat_shade_rect(0, 0, sw, sh, 10);
    if (!ui_window_pop(x, y, w, h, style, R.tab_t)) return;
    st = ui_style(style);
    ui_text(x + 10, y + 5, title, st->accent);
    fp_money_pill(x + w - 6, y + 3, 1);
    if (!R.nitems) {
        ui_text_center(x + w / 2, y + 26, "Your bag is empty!", st->text);
        ui_text_center(x + w / 2, y + 40, STR_ICON_A " Go shopping  " STR_ICON_B " Back", st->text_dim);
        return;
    }
    for (i = 0; i < vis && R.item_top + i < R.nitems; i++) {
        int it = R.items[R.item_top + i], ry = y + 17 + i * 12, sel = R.item_top + i == R.item_sel;
        int fi = it - ITEM_CUPCAKE;
        char cnt[8];
        if (sel) {
            fp_pill(x + 13, ry - 1, w - 18, 11, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x + 4, ry + 1);
        }
        fp_icon(x + 16, ry + 1, fp_item_icon(it), st->accent);
        ui_text(x + 25, ry + 1, doomo_items[it].name, st->text);
        if (R.item_mode == FPA_FEED && r && fi == r->like_food) ui_heart(x + w - 40, ry + 4, 3, PP(PAINT_ROSE, 1));
        if (R.item_mode == FPA_FEED && r && fi == r->hate_food) fp_icon(x + w - 43, ry + 1, ICON_SKULL, PAL_GRAY);
        doomo_snprintf(cnt, sizeof(cnt), "x%d", inv_count(it));
        ui_text(x + w - 26, ry + 1, cnt, st->text_dim);
    }
    if (R.nitems > vis)
        ui_scroll_arrows(x + w - 8, y + 12, y + 17 + vis * 12 - 2, R.item_top > 0, R.item_top + vis < R.nitems);
    ui_text_wrap(x + 8, y + h - 14, w - 16, doomo_items[R.items[R.item_sel]].desc, st->text_dim, 1);
}

static void draw_farewell(void)
{
    const DoomoResident *r = me();
    int sw = fp_sw(), sh = fp_sh(), t = R.farewell_t, i, cx = sw / 2;
    int dim = DOOMO_MIN(t / 4, 12);
    char buf[64];
    ui_backdrop(WIN_DARK, (uint32_t)fp_t());
    plat_shade_rect(0, 0, sw, sh, 15 - dim);
    /* the portrait, fading to grey in a soft spotlight */
    ui_fill_ellipse(cx, sh / 2 - 6, 46, 50, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_ellipse(cx, sh / 2 - 6, 40, 44, PAL_RAMP(RAMP_STONE, 2));
    if (r) {
        const DoomoImage *img = portrait_image(R.id, EXPR_DEAD);
        if (img) plat_image(cx - img->w / 2, sh / 2 - 6 - img->h / 2, img, 1, 0, 0);
    }
    /* candles */
    for (i = -1; i <= 1; i += 2) {
        int x = cx + i * 58, y = sh / 2 + 18, fl = ui_wave(fp_t() + i * 7, 10, 1);
        plat_rect(x - 2, y, 5, 12, PAL_RAMP(RAMP_BONE, 6));
        plat_rect(x - 2, y, 1, 12, PAL_RAMP(RAMP_BONE, 7));
        ui_fill_ellipse(x, y - 4 + fl, 2, 3 + (fl > 0), PAL_RAMP(RAMP_LAVA, 6));
        plat_rect(x, y - 4 + fl, 1, 2, PAL_WHITE);
        plat_rect(x, y - 1, 1, 1, PAL_RAMP(RAMP_STONE, 0));
    }
    if (t > 20) {
        doomo_snprintf(buf, sizeof(buf), "Rest in peace, %s.", r ? r->name : "friend");
        ui_text_center(cx, sh / 2 + 44, buf, PAL_RAMP(RAMP_BONE, 7));
    }
    if (t > 50) {
        int monster = r && (r->flags & RF_MONSTER_KILL);
        ui_text_center(cx, sh / 2 + 56, monster ? "The island will never forget this." :
                       "Their friends will miss them.", monster ? PAL_UI_RED : PAL_RAMP(RAMP_STONE, 6));
    }
    if (t > 80) ui_hint(STR_ICON_A " To the graveyard", WIN_DARK);
    fp_fx_draw();
}

static void res_draw(void)
{
    const DoomoResident *r = me();
    RLayout L;
    const UiStyle *st;
    int style, t = fp_t(), sw = fp_sw();
    if (!r) return;
    if (R.phase == PH_FAREWELL) { draw_farewell(); return; }
    style = window_style(r);
    st = ui_style(style);
    rlayout(&L);
    ui_backdrop(style == WIN_CREAM ? WIN_CUTE : style, (uint32_t)t);
    if (r->status == RES_POSSESSED && (t / 6) % 7 == 0) plat_shade_rect(0, 0, sw, fp_sh(), 12);
    /* tabs */
    ui_tabs(L.cx + 2, 5, tab_names, TAB_COUNT, R.tab, style);
    draw_left(&L, r, style);
    ui_window(L.cx, L.cy + 2, L.cw, L.ch, style);
    switch (R.tab) {
    case TAB_CARE: draw_care(&L, r, style); break;
    case TAB_ABOUT: draw_about(&L, r, style); break;
    case TAB_FRIENDS: draw_friends(&L, style); break;
    default: draw_fun(&L, style); break;
    }
    if (R.popup == POP_ITEMS) draw_items(style);
    else {
        static const char *const hints[TAB_COUNT] = {
            STR_ICON_A " Do it  L/R Tabs  " STR_ICON_B " Back",
            "\x89\x8A Scroll  L/R Tabs  " STR_ICON_B " Back",
            STR_ICON_A " Visit  L/R Tabs  " STR_ICON_B " Back",
            STR_ICON_A " Play  L/R Tabs  " STR_ICON_B " Back"
        };
        ui_hint(hints[R.tab % TAB_COUNT], style);
    }
    (void)st;
    fp_fx_draw();
}

const DoomoScene SCENE_RESIDENT = { "RESIDENT", res_enter, res_leave, res_update, res_draw, 0 };

/* ---- test hooks (core/tests/test_freeplay.c) ------------------------------ */
int fp_resident_action_count(void) { build_actions(); return R.nacts; }
int fp_resident_select(int a)
{
    int i;
    build_actions();
    for (i = 0; i < R.nacts; i++)
        if (R.acts[i] == a) { R.sel = (int16_t)i; R.tab = TAB_CARE; return 1; }
    return 0;
}
int fp_resident_has_action(int a)
{
    int i;
    build_actions();
    for (i = 0; i < R.nacts; i++) if (R.acts[i] == a) return 1;
    return 0;
}
int fp_resident_id(void) { return R.id; }
int fp_resident_in_farewell(void) { return R.phase == PH_FAREWELL; }
