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
#include "doomo/home.h"                /* H4: HOME_ARG_VISIT */

enum { TAB_CARE, TAB_ABOUT, TAB_FRIENDS, TAB_FUN, TAB_COUNT };
static const char *const tab_names[TAB_COUNT] = { "Care", "About", "Friends", "Fun" };


static const char *const act_label[FPA_COUNT] = {
    "Help!", "Feed", "Gift", "Clean", "Play", "Talk", "Heal", "Comfort", "Introduce",
    "Advice", "Edit look", "Favourite", "Exorcise", "Flowers", "Raise ghost", "Visit grave",
    "Their home", "Lay to rest", "Create here", "Give", "Manage"
};
static const uint8_t act_icon[FPA_COUNT] = {
    ICON_CLOUD, ICON_FOOD, ICON_HEART, ICON_DROP, ICON_STAR, ICON_SMILE, ICON_CROSS,
    ICON_HEART, ICON_ARROW_R, ICON_SPARK, ICON_WRENCH, ICON_STAR, ICON_CROSS, ICON_SPARK,
    ICON_GHOST, ICON_SKULL, ICON_HOUSE, ICON_SKULL, ICON_EGG, ICON_HEART, ICON_WRENCH
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
    uint8_t has_advice, dirty;        /* Advice only shows when there is something to advise on */
} R;

static DoomoResident *me(void) { return fp_res(R.id); }
static int advice_options(void);

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
        /* decluttered: Gift + Heal are one Give bag; Advice only when there is advice to give */
        ADD(FPA_FEED); ADD(FPA_GIVE); ADD(FPA_CLEAN); ADD(FPA_PLAY); ADD(FPA_TALK);
        ADD(FPA_COMFORT); ADD(FPA_INTRODUCE); if (R.has_advice) ADD(FPA_ADVICE);
        ADD(FPA_MANAGE);                      /* Edit look, Favourite, Their home in one place */
        break;
    case RES_POSSESSED:
        ADD(FPA_EXORCISE); ADD(FPA_TALK); ADD(FPA_FEED); ADD(FPA_GIVE); ADD(FPA_COMFORT);
        ADD(FPA_MANAGE);
        break;
    case RES_SOUL_LOST:
        ADD(FPA_EXORCISE); ADD(FPA_COMFORT); ADD(FPA_TALK); ADD(FPA_FAV);
        break;
    case RES_GHOST:
        ADD(FPA_TALK); ADD(FPA_PLAY); ADD(FPA_GIVE); ADD(FPA_COMFORT); ADD(FPA_INTRODUCE); ADD(FPA_FEED);
        ADD(FPA_MANAGE);
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
    R.has_advice = (uint8_t)(me() && me()->status == RES_ALIVE && advice_options() > 0);
    R.dirty = 0;
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
    case FPA_GIVE: return c == ICAT_GIFT || c == ICAT_CARE || c == ICAT_EXORCISM;
    default: return 0;
    }
}

static void open_items(int mode)
{
    int i, pass;
    R.item_mode = (uint8_t)mode;
    R.nitems = 0;
    for (pass = 0; pass < 2; pass++)              /* Give: presents first, then remedies */
        for (i = 1; i < ITEM_COUNT; i++) {
            int remedy = doomo_items[i].category != ICAT_GIFT;
            if (mode == FPA_GIVE ? remedy != pass : pass) continue;
            if (item_mode_ok(i) && (inv_count(i) > 0 || opt_get(OPT_CHEAT_INFINITE_CANDY)))
                R.items[R.nitems++] = (uint8_t)i;
        }
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
    int fav = r && doomo_items[item].category == ICAT_FOOD && fi == RES_LIKE_FOOD(r);
    int hate = r && doomo_items[item].category == ICAT_FOOD && fi == RES_HATE_FOOD(r);
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
    case FPA_GIVE:
        if (doomo_items[item].category == ICAT_GIFT) {
            line = sim_gift(R.id, item);
            audio_sfx(SFX_HAPPY);
            react(creepy ? EXPR_POSSESSED : EXPR_LOVE, creepy ? FX_SKULL : FX_HEART, 8);
        } else {
            line = sim_heal(R.id, item);
            audio_sfx(doomo_items[item].holy ? SFX_HOLY : SFX_SPARKLE);
            react(creepy ? EXPR_ANGRY : EXPR_HAPPY, FX_SPARK, 8);
        }
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

static int advice_options(void)
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
    if (partner >= 0 && partner < DOOMO_MAX_RESIDENTS && rel_kind(R.id, partner) == REL_DATING &&
        sim_marriage_ok(R.id, partner)) {                    /* LS0: teens never */
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
    return adv_n;
}

static void open_advice(void)
{
    int i;
    if (!advice_options()) {
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
    /* LS-QA: buy on the spot. A problem that wants an item you don't carry buys it for you when you
     * can afford the shop price, so Help! works in one press instead of a trip to the shop. */
    if (p->kind == PROB_WANTS_ITEM && p->item > ITEM_NONE && p->item < ITEM_COUNT &&
        doomo_town.items[p->item] == 0 && doomo_items[p->item].price > 0 &&
        doomo_town.money >= doomo_items[p->item].price) {
        doomo_town.money -= doomo_items[p->item].price;
        inv_add(p->item, 1);
        doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " Bought %s: -%u H$",
                       doomo_items[p->item].name, (unsigned)doomo_items[p->item].price);
        ui_toast(buf);
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

/* Manage: the housekeeping verbs (Edit look, Favourite, Their home) share one entry so the care
 * list is only things you do FOR them. Which ones exist depends on how they are. */
enum { MG_EDIT, MG_FAV, MG_HOME };
static uint8_t mg_kind[3];
static int mg_n;
static char mg_txt[3][16];
static const char *mg_opts[4];
static void do_action(int a);

static void cb_manage(int c)
{
    if (c < 0 || c >= mg_n) return;                  /* Never mind is the last row */
    switch (mg_kind[c]) {
    case MG_EDIT: do_action(FPA_EDIT); break;
    case MG_FAV: do_action(FPA_FAV); break;
    case MG_HOME: do_action(FPA_HOME); break;
    default: break;
    }
}

static void open_manage(void)
{
    const DoomoResident *r = me();
    int n = 0, i;
    if (!r) return;
    if (r->status == RES_ALIVE || r->status == RES_GHOST) { mg_kind[n] = MG_EDIT; fp_strcpy(mg_txt[n], "Edit look", 16); n++; }
    mg_kind[n] = MG_FAV;
    fp_strcpy(mg_txt[n], (r->flags & RF_FAVORITE) ? "Unfavourite" : "Favourite", 16);
    n++;
    mg_kind[n] = MG_HOME; fp_strcpy(mg_txt[n], "Their home", 16); n++;
    for (i = 0; i < n; i++) mg_opts[i] = mg_txt[i];
    mg_n = n;
    mg_opts[n] = "Never mind";
    audio_sfx(SFX_SELECT);
    ui_choice(R.id, "What would you like to do?", mg_opts, n + 1, cb_manage);
}

static void do_action(int a)
{
    const DoomoResident *r = me();
    const char *line;
    if (!r) return;
    switch (a) {
    case FPA_HELP: do_help(); break;
    case FPA_FEED: case FPA_GIVE: open_items(a); break;
    case FPA_MANAGE: open_manage(); break;
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
            ui_say(R.id, doomo_catchphrases[RES_CATCHPHRASE(r) % CATCHPHRASE_COUNT]);
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
    case FPA_HOME: scene_push(&SCENE_HOME, HOME_ARG_VISIT(R.id)); break;   /* H4: a visit (HF sec.3.1) */
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
    build_rel();
    build_actions();
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

/* (CL5, clarity C6) the Care tab's action list keeps the bottom of the window for the help strip that says which
 * need the action fills: 11 rows at 320x240, 8 at 240x160, one fewer while the problem pill is up. The update and
 * the drawing share it, so the cursor never scrolls off (it used list_vis while the drawing took one off). */
static int care_vis(void)
{
    return (fp_big() ? 11 : 8) - (sim_problem_for(R.id) >= 0);
}

static void care_update(void)
{
    int vis = care_vis();
    if (!R.nacts) return;
    if (btn_repeat(BTN_UP)) { R.sel = (int16_t)((R.sel + R.nacts - 1) % R.nacts); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { R.sel = (int16_t)((R.sel + 1) % R.nacts); audio_sfx(SFX_CURSOR); }
    if (R.sel < R.top) R.top = R.sel;
    if (R.sel >= R.top + vis) R.top = (int16_t)(R.sel - vis + 1);
    if (btn_pressed(BTN_A)) { do_action(R.acts[R.sel]); R.dirty = 1; }
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
    if (R.dirty || (fp_t() & 63) == 0) build_rel();
    build_actions();
    r = me();
    if (R.popup == POP_ITEMS) { items_update(); return; }
    if (btn_pressed(BTN_L)) { R.tab = (uint8_t)((R.tab + TAB_COUNT - 1) % TAB_COUNT); R.tab_t = 0; audio_sfx(SFX_BLIP); return; }
    if (btn_pressed(BTN_R)) { R.tab = (uint8_t)((R.tab + 1) % TAB_COUNT); R.tab_t = 0; audio_sfx(SFX_BLIP); return; }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); leave(); return; }
    if (btn_pressed(BTN_SELECT) && r && r->status != RES_EMPTY) {
        do_action(FPA_TALK);
        return;
    }
    if (R.tab == TAB_CARE && r && r->status == RES_ALIVE && R.tab_t > 6 && tip_once(TIP_CARE)) return;   /* CL4 */
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

/* (CL5, clarity C6 1.9c) 240x160: the portrait column is 54 px (9 letters), so the name, cut to "udge H" by the
 * old plate, is drawn in full in the top band (res_draw: name_band) and the lines under the portrait use short
 * words that fit whole: the mood, where they are ("Concert", not "At a co"), the building and the room. */
static const char *activity_short(int act)
{
    static const char *const n[ACT_COUNT] = {
        "Home", "Asleep", "Park", "Cafe", "Shopping", "Church", "Workshop", "Beach", "Concert",
        "Vape shop", "Hospital", "Wandering", "Haunting"
    };
    return (act >= 0 && act < ACT_COUNT) ? n[act] : "Out";
}

static void draw_left_small(const RLayout *L, const DoomoResident *r, int style, int y)
{
    const UiStyle *st = ui_style(style);
    int x = L->px, w = L->cx - 2 - L->px;     /* the column and the gap up to the window */
    char buf[24];
    if (r->status == RES_EMPTY) {
        ui_text_center(x + L->pw / 2, y + 2, "Vacant", st->text_dim);
        return;
    }
    draw_hearts(x + (L->pw - 50) / 2, y, r->level, PP(PAINT_ROSE, 1));
    y += 11;
    if (fp_living_status(r->status)) {
        fp_icon(x, y, fp_mood_icon(r->mood), fp_mood_color(r->mood));
        ui_text_clip(x + 9, y, lang_tr(doomo_mood_names[r->mood % MOOD_COUNT]), st->text, x + 9, x + w);
        y += FONT_LINE;
        fp_icon(x, y, fp_activity_icon(r->activity), st->accent);
        ui_text_clip(x + 9, y, lang_tr(activity_short(r->activity)), st->text_dim, x + 9, x + w);
    } else {
        fp_icon(x, y, fp_status_icon(r->status), fp_status_color(r->status));
        ui_text_clip(x + 9, y, lang_tr(fp_status_name(r->status)), st->text, x + 9, x + w);
    }
    y += FONT_LINE;
    fp_icon(x, y, ICON_HOUSE, RES_DISTRICT(R.id) ? PP(PAINT_VAPE_PURPLE, 1) : PP(PAINT_BUBBLEGUM, 0));
    ui_text_clip(x + 9, y, lang_tr(RES_DISTRICT(R.id) ? "Pits" : "Towers"), st->text_dim, x + 9, x + w);
    y += FONT_LINE;
    doomo_snprintf(buf, sizeof(buf), "Room %d-%d", fp_floor(R.id), fp_room(R.id));
    ui_text_clip(x, y, buf, st->text_dim, x, x + w);
}

/* 240x160: the name over the portrait, in the band left of the tabs (the tabs sit at the right edge) */
static void name_band(const RLayout *L, const DoomoResident *r, int style, int tabs_x)
{
    const UiStyle *st = ui_style(style);
    const char *nm = r->status == RES_EMPTY ? "Vacant" : r->name;
    int x = 2, w = tabs_x - 3 - x, tw = ui_text_width(nm), pw = DOOMO_MIN(tw + 8, w);   /* 11 letters fit: 66 + 8 */
    (void)L;
    fp_pill(x, 3, pw, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
    if (tw > pw - 8) fp_marquee(x + 4, 5, pw - 8, nm, st->title, fp_t());   /* only a translated "Vacant" */
    else ui_text_shadow(x + 4, 5, nm, st->title, st->outline);
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
    if (!fp_big()) { draw_left_small(L, r, style, y); return; }
    /* name plate */
    {
        int nw = DOOMO_MIN(ui_text_width(r->name) + 12, w + 4);
        int nx = L->px + (w - nw) / 2;
        fp_pill(nx, y, nw, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
        if (r->status != RES_EMPTY && ui_text_width(r->name) > nw - 12)
            fp_marquee(nx + 6, y + 2, nw - 12, r->name, st->title, t);   /* a long name scrolls, not clipped */
        else
            ui_text_shadow(nx + 6, y + 2, r->status == RES_EMPTY ? "Vacant" : r->name, st->title, st->outline);
        y += 14;
    }
    if (r->status == RES_EMPTY) return;
    draw_hearts(L->px + (w - 50) / 2, y, r->level, PP(PAINT_ROSE, 1));
    y += 11;
    if (fp_living_status(r->status)) {
        fp_icon(L->px + 2, y, fp_mood_icon(r->mood), fp_mood_color(r->mood));
        fp_marquee(L->px + 11, y, w - 11, doomo_mood_names[r->mood % MOOD_COUNT], st->text, t);
        y += FONT_LINE;
        /* what they're up to right now */
        fp_icon(L->px + 2, y, fp_activity_icon(r->activity), st->accent);
        fp_marquee(L->px + 11, y, w - 11, fp_activity_name(r->activity), st->text_dim, t + 40);
    } else {
        fp_icon(L->px + 2, y, fp_status_icon(r->status), fp_status_color(r->status));
        fp_marquee(L->px + 11, y, w - 11, fp_status_name(r->status), st->text, t);
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

/* (CL5, clarity C6) which need an action fills (STAT_*), -1 for none: the meter it lights up and the help strip */
static int act_stat(int a)
{
    switch (a) {
    case FPA_FEED: return STAT_HUNGER;
    case FPA_GIVE: case FPA_GIFT: case FPA_PLAY: return STAT_HAPPY;
    case FPA_CLEAN: return STAT_HYGIENE;
    case FPA_HEAL: return STAT_HEALTH;
    case FPA_TALK: case FPA_INTRODUCE: return STAT_SOCIAL;
    case FPA_COMFORT: return STAT_SANITY;
    case FPA_EXORCISE: return STAT_CORRUPTION;
    case FPA_HELP: {
        const DoomoProblem *p = sim_problem(sim_problem_for(R.id));
        if (!p) return -1;
        switch (p->kind) {
        case PROB_HUNGRY: return STAT_HUNGER;
        case PROB_DIRTY: return STAT_HYGIENE;
        case PROB_BORED: return STAT_HAPPY;
        case PROB_SICK: return STAT_HEALTH;
        case PROB_LONELY: case PROB_WANTS_FRIEND: return STAT_SOCIAL;
        case PROB_NIGHTMARE: return STAT_SANITY;
        case PROB_CORRUPTION: case PROB_POSSESSED: return STAT_CORRUPTION;
        default: return -1;
        }
    }
    default: return -1;
    }
}

/* Help!'s help line for a problem of this kind, as do_help() handles it: an item that is yours (have), bought on the
 * spot when the purse (money) has the shop price, or not yet (the problem stays open); an exorcism; the need the
 * problem lowers (s, its value v); or a talk. The text-fit hook measures every kind and item through here. */
static void help_text(int kind, int item, int have, uint32_t money, int s, int v, char *buf, int cap)
{
    if (kind == PROB_WANTS_ITEM && item > ITEM_NONE && item < ITEM_COUNT) {
        const char *n = doomo_items[item].name;
        unsigned price = (unsigned)doomo_items[item].price;
        if (have) doomo_snprintf(buf, cap, "Help! gives them your %s.", n);
        else if (price > 0 && money >= price) doomo_snprintf(buf, cap, "Help! buys and gives %s (H$%u).", n, price);
        else if (price > 0) doomo_snprintf(buf, cap, "Help! %s costs H$%u, you have H$%u.", n, price, (unsigned)money);
        else doomo_snprintf(buf, cap, "Help! needs %s first.", n);
    } else if (kind == PROB_POSSESSED) doomo_snprintf(buf, cap, "Help! starts an exorcism.");
    else if (s >= 0) doomo_snprintf(buf, cap, "Help! fixes their problem. %s: %d%%.", doomo_stat_names[s], v);
    else doomo_snprintf(buf, cap, "Help! talks them through their problem.");
}

/* the help strip: what the selected action does, in the need words of the meters ("Feed fills Food: 62%") */
static const char *care_help(int a, char *buf, int cap)
{
    const DoomoResident *r = me();
    int s = act_stat(a), v = (r && s >= 0) ? r->stat[s] : 0, k, low = 0;
    if (!r) { buf[0] = 0; return buf; }
    for (k = 1; k < STAT_COUNT; k++)
        if (k != STAT_CORRUPTION && r->stat[k] < r->stat[low]) low = k;
    switch (a) {
    case FPA_FEED: doomo_snprintf(buf, cap, "Feed fills Food: %d%% now.", v); break;
    case FPA_GIVE: doomo_snprintf(buf, cap, "Give a gift (Joy %d%%) or a remedy (Health %d%%).", v,
                                  r->stat[STAT_HEALTH]); break;
    case FPA_CLEAN: doomo_snprintf(buf, cap, "Clean fills Clean: %d%% now.", v); break;
    case FPA_PLAY: doomo_snprintf(buf, cap, "Play fills Joy (%d%%) and Social. Uses Energy: %d%%.", v,
                                  r->stat[STAT_ENERGY]); break;
    case FPA_TALK: doomo_snprintf(buf, cap, "Talk fills Social (%d%%) and the hearts (bond).", v); break;
    case FPA_COMFORT: doomo_snprintf(buf, cap, "Comfort fills Mind (%d%%), a bit of Joy too.", v); break;
    case FPA_INTRODUCE: doomo_snprintf(buf, cap, "Introduce: new friends fill Social (%d%%).", v); break;
    case FPA_EXORCISE: doomo_snprintf(buf, cap, "Exorcise drives out the Dark (%d%%).", v); break;
    case FPA_HELP: {
        const DoomoProblem *p = sim_problem(sim_problem_for(R.id));
        int item = p ? p->item : ITEM_NONE;
        int have = item > ITEM_NONE && item < ITEM_COUNT &&
                   (doomo_town.items[item] || opt_get(OPT_CHEAT_INFINITE_CANDY));
        help_text(p ? p->kind : PROB_NONE, item, have, doomo_town.money, s, v, buf, cap);
        break; }
    case FPA_ADVICE: doomo_snprintf(buf, cap, "Advice: help them decide something."); break;
    case FPA_MANAGE: doomo_snprintf(buf, cap, "Manage: edit look, favourite, their home."); break;
    default:
        if (fp_living_status(r->status)) doomo_snprintf(buf, cap, "Lowest need: %s %d%%.", doomo_stat_names[low], r->stat[low]);
        else buf[0] = 0;
        break;
    }
    return buf;
}

/* the need meters' geometry (x, width, first row, row pitch), the help strip's box and the problem pill's top
 * (py), shared with the text-fit hook. prob: a problem pill is up. */
typedef struct { int lw, x, y, mx, mw, my, rh, hx, hy, hw, hlines, py; } CareGeom;

#define CARE_HELP_LINES 2

static void care_geom(const RLayout *L, CareGeom *g, int prob)
{
    int big = fp_big();
    g->lw = big ? 104 : 88;            /* (CL5) 240x160: 88 (was 80) so "Raise ghost", "Unfavourite" fit */
    g->x = L->cx + 6;
    g->y = L->cy + 8;
    g->mx = L->cx + g->lw + 10;
    g->mw = L->cw - g->lw - 16;
    g->my = L->cy + (big ? 7 : 6);
    /* (CL5 land) 240x160 with the problem pill up: the meters step 11 px (12 without), so the help strip keeps
     * both its lines above the pill (it had one, and "Help! buys Board Game (H$70) and gives it." lost its price) */
    g->rh = big ? 15 : prob ? 11 : 12;
    g->hx = L->cx + 7;
    g->hw = L->cw - 14;
    g->hy = big ? g->y + 11 * 11 + 4 : g->my + STAT_COUNT * g->rh + 1;
    g->hlines = CARE_HELP_LINES;
    g->py = L->cy + L->ch - (big || !prob ? 15 : 14);
}

static void draw_care(const RLayout *L, const DoomoResident *r, int style)
{
    const UiStyle *st = ui_style(style);
    CareGeom g;
    /* (CL5) the list's labels end at the selection pill's right end, lw - 20 from the list's left */
    int vis = care_vis(), i, lw, x, y, mx, mw, t = fp_t();
    int has_prob = sim_problem_for(R.id) >= 0;
    care_geom(L, &g, has_prob);
    lw = g.lw; x = g.x; y = g.y; mx = g.mx; mw = g.mw;
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
        int s, rows = STAT_COUNT, rh = g.rh, hi = R.sel < R.nacts ? act_stat(R.acts[R.sel]) : -1;
        char help[96];
        for (s = 0; s < rows; s++) {
            int v = r->stat[s];
            int ry = g.my + s * rh;
            if (s == hi) {
                /* (CL5) the need the selected action fills is framed */
                ui_frame(mx - 2, ry - (fp_big() ? 2 : rh - 10), mw + 3, rh, st->accent);   /* 240: bottom row under the bar */
            }
            if (fp_big()) fp_meter(mx, ry, mw, fp_stat_icon(s), doomo_stat_names[s], v,
                                   fp_stat_color(s, v), st->text);
            else {
                /* (CL5, C6 1.9a) 240x160: the need's name and its value over a thin bar (were an icon and a bar) */
                char num[8];
                doomo_snprintf(num, sizeof(num), "%d", v);
                fp_icon(mx, ry, fp_stat_icon(s), fp_stat_color(s, v));
                ui_text(mx + 9, ry, doomo_stat_names[s], s == hi ? st->text : st->text_dim);
                ui_text(mx + mw - ui_text_width(num), ry, num, st->text);
                ui_bar(mx + 9, ry + 8, mw - 9, 2, v, 100, fp_stat_color(s, v));
            }
        }
        if (R.sel < R.nacts) {
            const char *h = care_help(R.acts[R.sel], help, sizeof(help));
            fp_wrap_lines(g.hx, g.hy, g.hw, h, st->accent, g.hlines);
        }
    }
    if (has_prob) draw_problem(L->cx + 4, g.py, L->cw - 8);
}

static int about_line(int i, char *buf, int cap, uint8_t *icon)
{
    const DoomoResident *r = me();
    int m = RES_BDAY_MONTH(r) % 12;
    *icon = ICON_STAR;
    switch (i) {
    case 0: *icon = ICON_WRENCH; doomo_snprintf(buf, cap, "Job: %s", fp_job(R.id)); return 1;
    case 1: *icon = ICON_SMILE; doomo_snprintf(buf, cap, "Type: %s",
                                        doomo_persona_type_names[persona_type(r) % PERSONA_TYPE_COUNT]); return 1;
    case 2: *icon = ICON_SPARK; doomo_snprintf(buf, cap, "%s, %s", doomo_species_names[RES_SPECIES(r) % SPECIES_COUNT],
                                        doomo_pronoun_names[RES_PRONOUN(r) % PRONOUN_COUNT]); return 1;
    case 3: *icon = ICON_EGG; doomo_snprintf(buf, cap, "Birthday: %s %d", doomo_month_names[m], RES_BDAY_DAY(r)); return 1;
    case 4: *icon = ICON_HEART; doomo_snprintf(buf, cap, "Falls for: %s", doomo_romance_names[RES_ROMANCE(r) % ROMANCE_COUNT]); return 1;
    case 5: *icon = ICON_FOOD; doomo_snprintf(buf, cap, "Loves: %s", doomo_food_names[RES_LIKE_FOOD(r) % FOOD_COUNT]); return 1;
    case 6: *icon = ICON_X; doomo_snprintf(buf, cap, "Hates: %s", doomo_food_names[RES_HATE_FOOD(r) % FOOD_COUNT]); return 1;
    case 7: *icon = ICON_STAR; doomo_snprintf(buf, cap, "Hobby: %s", doomo_hobby_names[RES_LIKE_HOBBY(r) % HOBBY_COUNT]); return 1;
    case 8: *icon = ICON_NOTE; doomo_snprintf(buf, cap, "Music: %s", doomo_music_names[RES_LIKE_MUSIC(r) % MUSIC_COUNT]); return 1;
    case 9: *icon = ICON_DROP; doomo_snprintf(buf, cap, "Colour: %s", doomo_paint_names[RES_LIKE_COLOR(r) % PAL_PAINT_COUNT]); return 2;
    case 10: *icon = ICON_GHOST; doomo_snprintf(buf, cap, "Fear: %s", doomo_fear_names[RES_FEAR(r) % FEAR_COUNT]); return 1;
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
        doomo_snprintf(buf, sizeof(buf), "\"%s\"", doomo_catchphrases[RES_CATCHPHRASE(r) % CATCHPHRASE_COUNT]);
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
            ui_round_rect(x + w - 8, y, 8, 8, 2, PP(RES_LIKE_COLOR(r) % 32, 0));
            ui_round_rect(x + w - 7, y + 1, 6, 6, 1, PP(RES_LIKE_COLOR(r) % 32, 1));
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
        /* (CL5, C6 1.9d) the selected friend's bond in words and numbers: what the bar beside each name means */
        int o = R.rel[R.rel_sel], k = rel_kind(R.id, o), v = rel_value(R.id, o);
        char buf[64];
        doomo_snprintf(buf, sizeof(buf), "%s, bond %s%d of 100%s", k >= 0 ? doomo_rel_kind_names[k] : "?",
                       v > 0 ? "+" : "", v,
                       o < DOOMO_MAX_RESIDENTS && RES_DISTRICT(o) != RES_DISTRICT(R.id) ? " (across the Wall!)" : "");
        if (ui_text_width(buf) > w - 4)          /* the Wall note only when it fits */
            doomo_snprintf(buf, sizeof(buf), "%s, bond %s%d of 100", k >= 0 ? doomo_rel_kind_names[k] : "?",
                           v > 0 ? "+" : "", v);
        ui_text_clip(x + 2, L->cy + L->ch - 13, lang_tr(buf), st->accent, x + 2, x + w);
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

static void items_rect(int *x, int *y, int *w, int *h)
{
    int sw = fp_sw(), sh = fp_sh(), vis = fp_big() ? 9 : 6;
    /* (CL5, C13 1.9e) two lines for the item's description, inside the popup (it ran out over the meters) */
    *w = DOOMO_MIN(sw - 40, 200);
    *h = vis * 12 + 43;
    *x = (sw - *w) / 2;
    *y = (sh - *h) / 2 - 4;
}

static void draw_items(int style, int held)
{
    const UiStyle *st;
    const DoomoResident *r = me();
    int sw = fp_sw(), sh = fp_sh(), vis = fp_big() ? 9 : 6, i, w, h, x, y;
    const char *title = R.item_mode == FPA_FEED ? "Feed what?" : "Give what?";
    items_rect(&x, &y, &w, &h);
    if (!held) plat_shade_rect(0, 0, sw, sh, 10);
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
        if (R.item_mode == FPA_FEED && r && fi == RES_LIKE_FOOD(r)) ui_heart(x + w - 40, ry + 4, 3, PP(PAINT_ROSE, 1));
        if (R.item_mode == FPA_FEED && r && fi == RES_HATE_FOOD(r)) fp_icon(x + w - 43, ry + 1, ICON_SKULL, PAL_GRAY);
        doomo_snprintf(cnt, sizeof(cnt), "x%d", inv_count(it));
        ui_text(x + w - 26, ry + 1, cnt, st->text_dim);
    }
    if (R.nitems > vis)
        ui_scroll_arrows(x + w - 8, y + 12, y + 17 + vis * 12 - 2, R.item_top > 0, R.item_top + vis < R.nitems);
    {
        const char *d = doomo_items[R.items[R.item_sel]].desc;
        fp_wrap_lines(x + 8, y + h - 23, w - 16, d, st->text_dim, 2);
    }
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

static int tabs_width(void)
{
    int i, w = 0;
    for (i = 0; i < TAB_COUNT; i++) w += ui_text_width(tab_names[i]) + 12;
    return w - 2;
}

static void res_draw(void)
{
    const DoomoResident *r = me();
    RLayout L;
    const UiStyle *st;
    int style, t = fp_t(), sw = fp_sw();
    int hx, hy, hw, hh, hold_ok;
    if (!r) { ui_modal_hold(&R, 0, 0, 0, 0, 0); return; }
    style = window_style(r);
    /* S2-1 (docs/GBA.md "Speed wave 2"): the Feed / Give popup, fully popped, holds the dimmed page under it
     * (the portrait's idle motion and the backdrop's drift stand still); only the popup repaints */
    items_rect(&hx, &hy, &hw, &hh);
    hold_ok = R.phase != PH_FAREWELL && R.popup == POP_ITEMS && R.tab_t >= 12 && r->status != RES_POSSESSED &&
              !fp_fx_any() && !fp_money_flash();
    if (ui_modal_hold(&R, hold_ok, hx, hy, hw, hh)) {
        draw_items(style, 1);
        ui_modal_hold_end();
        return;
    }
    if (R.phase == PH_FAREWELL) { draw_farewell(); return; }
    st = ui_style(style);
    rlayout(&L);
    /* SP4 (G-L1): the backdrop leaves out what the content window (drawn below, opaque but for its r = 4 corners)
     * covers: the same picture, half the backdrop not drawn */
    ui_backdrop_hole(style == WIN_CREAM ? WIN_CUTE : style, (uint32_t)t,
                     L.cx + 4, L.cy + 2, L.cx + L.cw - 4, L.cy + 2 + L.ch);
    if (r->status == RES_POSSESSED && (t / 6) % 7 == 0) plat_shade_rect(0, 0, sw, fp_sh(), 12);
    /* tabs (CL5: at 240x160 against the right edge, the name in the band left of them) */
    if (fp_big()) ui_tabs(L.cx + 2, 5, tab_names, TAB_COUNT, R.tab, style);
    else {
        int tx = sw - 1 - tabs_width();
        ui_tabs(tx, 5, tab_names, TAB_COUNT, R.tab, style);
        name_band(&L, r, style, tx);
    }
    draw_left(&L, r, style);
    ui_window(L.cx, L.cy + 2, L.cw, L.ch, style);
    switch (R.tab) {
    case TAB_CARE: draw_care(&L, r, style); break;
    case TAB_ABOUT: draw_about(&L, r, style); break;
    case TAB_FRIENDS: draw_friends(&L, style); break;
    default: draw_fun(&L, style); break;
    }
    if (R.popup == POP_ITEMS) draw_items(style, 0);
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
    ui_modal_full(&R, hold_ok);
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

/* (CL5, clarity C13) text fit of the open citizen's Care tab at the current screen size: every action's label in the
 * list, every need's name with a value of 100 in its meter row, every action's help strip in its lines (with and
 * without the problem pill, every need at 100; Help!'s line for every problem kind and every wanted item, yours,
 * bought or not affordable) above the pill, and (at 240x160) the longest possible name in the band over the
 * portrait. Returns the count; the first is in why. */
static int help_too_long(const CareGeom *g, const char *h)
{
    return ui_text_wrap(0, 0, g->hw, h, 0, 0) > g->hlines;
}

int fp_resident_text_overflow(char *why, int cap)
{
    RLayout L;
    CareGeom g;
    char help[96];
    uint8_t keep[STAT_COUNT];
    DoomoResident *r = R.id >= 0 && R.id < DOOMO_MAX_RESIDENTS ? &doomo_town.res[R.id] : NULL;
    int a, s, bad = 0, prob, i, k;
    if (why && cap) why[0] = 0;
    if (!me() || !r) return 0;
    rlayout(&L);
#define BAD(...) do { int l_ = why ? (int)strlen(why) : 0; bad++; \
        if (why && l_ < cap - 8) { \
            if (l_) { doomo_snprintf(why + l_, cap - l_, " | "); l_ += 3; } \
            doomo_snprintf(why + l_, cap - l_, __VA_ARGS__); } } while (0)
    memcpy(keep, r->stat, sizeof(keep));
    memset(r->stat, 100, sizeof(r->stat));                  /* the widest values: every need at 100 */
    for (prob = 0; prob < 2; prob++) {
        care_geom(&L, &g, prob);
        /* the last line's 8-row cell ends above the pill (or the window's bottom without one) */
        if (g.hy + (g.hlines - 1) * FONT_LINE + 8 > g.py + (prob ? 0 : 15))
            BAD("%s: the help strip's %d lines end at %d, the %s starts at %d", prob ? "problem" : "no problem",
                g.hlines, g.hy + (g.hlines - 1) * FONT_LINE + 8, prob ? "problem pill" : "window's edge",
                g.py + (prob ? 0 : 15));
        for (a = 0; a < FPA_COUNT; a++) {
            const char *h;
            if (a == FPA_HELP) continue;                     /* below, for every problem */
            h = care_help(a, help, sizeof(help));
            if (help_too_long(&g, h)) BAD("%shelp of \"%s\" needs %d lines of %d: %s", prob ? "(problem) " : "",
                                          act_label[a], ui_text_wrap(0, 0, g.hw, h, 0, 0), g.hlines, h);
        }
        if (!prob) continue;                                 /* Help! is only offered while a problem is up */
        for (k = 0; k < 3 + STAT_COUNT; k++) {
            help_text(k == 0 ? PROB_POSSESSED : PROB_FIGHT, ITEM_NONE, 0, 0, k < 3 ? -1 : k - 3, 100,
                      help, sizeof(help));
            if (help_too_long(&g, help)) BAD("Help! needs %d lines of %d: %s", ui_text_wrap(0, 0, g.hw, help, 0, 0),
                                             g.hlines, help);
        }
        for (i = ITEM_NONE + 1; i < ITEM_COUNT; i++) {
            uint32_t price = doomo_items[i].price;
            for (k = 0; k < 3; k++) {
                help_text(PROB_WANTS_ITEM, i, k == 0, k == 2 && price ? price - 1 : price, -1, 0, help, sizeof(help));
                if (help_too_long(&g, help)) BAD("Help! needs %d lines of %d: %s",
                                                 ui_text_wrap(0, 0, g.hw, help, 0, 0), g.hlines, help);
            }
        }
    }
    memcpy(r->stat, keep, sizeof(keep));
    care_geom(&L, &g, 0);
    for (a = 0; a < FPA_COUNT; a++)
        if (ui_text_width(act_label[a]) > g.lw - 20) BAD("action \"%s\" wider than %d", act_label[a], g.lw - 20);
    for (s = 0; s < STAT_COUNT; s++) {
        int need = 9 + ui_text_width(doomo_stat_names[s]) + 4 + ui_text_width("100");
        if (!fp_big() && need > g.mw) BAD("need \"%s\" + 100 is %d px, the meter %d", doomo_stat_names[s], need, g.mw);
    }
    if (ui_text_width("Unfavourite") > g.lw - 20) BAD("\"Unfavourite\" wider than %d", g.lw - 20);
    if (!fp_big()) {
        int room = (fp_sw() - 1 - tabs_width()) - 3 - 2 - 8;
        if ((DOOMO_NAME_LEN - 1) * FONT_ADVANCE > room) BAD("an 11-letter name needs %d px, the band has %d",
                                                            (DOOMO_NAME_LEN - 1) * FONT_ADVANCE, room);
    }
#undef BAD
    return bad;
}

/* (CL5 land) the selected action's help strip as drawn now: its text in buf, the lines it needs in *need; returns the
 * lines the strip has (the test checks the two with a problem pill up) */
int fp_resident_help_strip(char *buf, int cap, int *need)
{
    RLayout L;
    CareGeom g;
    if (buf && cap) buf[0] = 0;
    if (need) *need = 0;
    if (!me() || !buf || R.sel >= R.nacts) return 0;
    rlayout(&L);
    care_geom(&L, &g, sim_problem_for(R.id) >= 0);
    care_help(R.acts[R.sel], buf, cap);
    if (need) *need = ui_text_wrap(0, 0, g.hw, buf, 0, 0);
    return g.hlines;
}
