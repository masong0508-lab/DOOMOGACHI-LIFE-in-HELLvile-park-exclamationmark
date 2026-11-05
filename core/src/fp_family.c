/* fp_family.c - SCENE_FAMILY: the family dashboard, add-member wizard,
 * care-for-everyone and the memorial page (owner: freeplay, F2).
 *
 * Two pages (L / R): Household (every member, A opens that member's menu,
 * the last two rows are "Care for everyone" and "Add a member") and
 * Remembered (the memorials family_lose() leaves behind; A leaves flowers,
 * up to three posies each). Pets get their own sprite, Feed and Play (F5).
 * Replaces the weak K0 placeholder in weak_scenes.c.
 * Gentle by design: a minor's memorial never states a cause.
 */
#include "fp_int.h"
#include "doomo/family.h"
#include "doomo/playas.h"

static struct {
    int tab, sel, top, t;
    /* wizard / menu state */
    int target;            /* member id the menu is about          */
    int kind;              /* wizard: 0 neighbour 1 person 2 pet 3 baby */
    int pick;              /* wizard: chosen resident id            */
    int role, stage, pronoun, rolectx;   /* rolectx: 0 wizard, 1 change a role */
    char name[DOOMO_NAME_LEN];
    int acts[6];           /* member menu: action codes             */
    const char *opts[6];
} W;

enum { MA_CARE, MA_ROLE, MA_RENAME, MA_PROFILE, MA_REMOVE, MA_PLAYAS, MA_FEED, MA_PLAY };
enum { CARE_PLAY, CARE_TALK, CARE_CLEAN, CARE_COMFORT };

static const char *const tabs[2] = { "Household", "Remembered" };
static const char *const role_names[FAMROLE_COUNT] = {
    "-", "Partner", "Child", "Parent", "Sibling", "Grandparent", "Housemate", "Pet"
};
static const uint8_t role_pick[6] = { FAMROLE_PARTNER, FAMROLE_CHILD, FAMROLE_PARENT,
                                      FAMROLE_SIBLING, FAMROLE_GRANDPARENT, FAMROLE_HOUSEMATE };
static const char *const role_opts[6] = { "Partner", "Child", "Parent", "Sibling", "More...",
                                          "Never mind" };
static const char *const role_opts2[3] = { "Grandparent", "Housemate", "Never mind" };
static const uint8_t stage_pick[5] = { LIFE_ADULT, LIFE_TEEN, LIFE_CHILD, LIFE_TODDLER, LIFE_BABY };
static const char *const stage_opts[6] = { "Adult", "Teen", "Child", "Toddler", "Baby", "Never mind" };
static const char *const pron_opts[4] = { "they/them", "she/her", "he/him", "Never mind" };
static const uint8_t pron_pick[3] = { PRONOUN_THEY, PRONOUN_SHE, PRONOUN_HE };
static const char *const kind_opts[5] = { "A neighbour moves in", "A new person", "A new pet",
                                          "A baby is born", "Never mind" };
static const char *const care_top[5] = { "Quick care", "Choose the care...",
                                         "Check on everyone", "Family free will...", "Never mind" };
static const char *const will_opts[5] = { "Off", "Min", "Normal", "High", "Subconscious" };
static const char *const care_opts[5] = { "Play with everyone", "Talk to everyone",
                                          "Clean everyone up", "Comfort everyone", "Never mind" };
static const char *const cause_names[FAMLOSS_COUNT] = {
    "", "Passed peacefully", "Passed after an illness", "Lost to neglect", "Lost to a demon",
    "Moved far away"
};

/* the last row ("Switch back") exists only while you play someone other than the hero (PA1) */
static int rows(void) { return family_count() + 2 + (playas_is_hero() ? 0 : 1); }

static int back_target(void)
{
    int t = playas_recent(0);
    return t != PLAYAS_ID_NONE && playas_can_be(t) ? t : PLAYAS_ID_HERO;
}
static int vis(void) { return fp_big() ? 6 : 4; }

static const char *member_name(int id, char *buf)
{
    const DoomoResident *r;
    const FamilyIdentity *f;
    buf[0] = 0;
    if ((r = fp_res(id)) != NULL) { fp_strcpy(buf, r->name, DOOMO_NAME_LEN); return buf; }
    if ((f = family_identity(id)) != NULL) { fp_strcpy(buf, f->name, DOOMO_NAME_LEN); return buf; }
    fp_strcpy(buf, "?", DOOMO_NAME_LEN);
    return buf;
}

static const char *err_text(int e)
{
    switch (e) {
    case FAM_ERR_FULL: return "The household is full.";
    case FAM_ERR_DUP: return "They already live with you.";
    case FAM_ERR_ID: return "That isn't possible right now.";
    case FAM_ERR_ROLE: return "That role doesn't work (one partner at most, pets are pets).";
    case FAM_ERR_GATE: return "A partner must be an adult, and not family.";
    case FAM_ERR_NAME: return "That name won't do. Try 1 to 11 plain letters.";
    case FAM_ERR_NOPARTNER: return "A baby needs you to have a partner at home.";
    default: return "Something went wrong.";
    }
}

static void result(int r, const char *ok)
{
    if (r < 0) { audio_sfx(SFX_ERROR); ui_say(-1, err_text(r)); return; }
    audio_sfx(SFX_HAPPY);
    fp_fx_burst(fp_sw() / 2, 60, FX_HEART, 10);
    ui_toast(ok);
}

/* ---- add-member wizard ------------------------------------------------- */
static void wz_finish(void)
{
    int r;
    uint32_t seed = (uint32_t)doomo_hash32((uint32_t)fp_t() * 2654435761u ^ (doomo_town.day << 8) ^
                                            (uint32_t)family_count());
    if (W.kind == 3) r = family_birth(W.name, W.pronoun);
    else r = family_pool_add(W.role, W.stage, seed, W.name, W.pronoun);
    result(r, STR_ICON_HEART " Welcome to the family!");
}

static void wz_pron(int c)
{
    if (c < 0 || c > 2) return;      /* 3 = never mind */
    W.pronoun = pron_pick[c];
    wz_finish();
}
static void wz_ask_pron(void)
{
    ui_choice(-1, "How should we call them?", pron_opts, 4, wz_pron);
}

static void wz_stage(int c)
{
    if (c < 0 || c > 4) return;
    W.stage = stage_pick[c];
    wz_ask_pron();
}

static void wz_named(int ok)
{
    if (!ok) return;
    if (W.kind == 2) { W.stage = LIFE_ADULT; wz_ask_pron(); }
    else if (W.kind == 3) wz_ask_pron();
    else ui_choice(-1, "How old are they?", stage_opts, 6, wz_stage);
}
static void wz_ask_name(void)
{
    ui_text_entry(W.kind == 2 ? "Pet's name?" : "Name?", W.name, DOOMO_NAME_LEN, wz_named);
}

static void role_apply(int role)
{
    W.role = role;
    if (W.rolectx) result(family_set_role(W.target, role), STR_ICON_HEART " Role changed.");
    else if (W.kind == 0) result(family_add(W.pick, role, 0), STR_ICON_HEART " Moved in with you!");
    else wz_ask_name();
}
static void role_more(int c)
{
    if (c == 0) role_apply(FAMROLE_GRANDPARENT);
    else if (c == 1) role_apply(FAMROLE_HOUSEMATE);
}
static void role_done(int c)
{
    if (c < 0 || c > 4) return;                 /* 5 = never mind */
    if (c == 4) ui_choice(-1, "And...?", role_opts2, 3, role_more);
    else role_apply(role_pick[c]);
}
static void ask_role(int ctx, const char *q)
{
    W.rolectx = ctx;
    ui_choice(-1, q, role_opts, 6, role_done);
}

static void wz_neighbour(int id)
{
    if (id < 0) return;
    W.pick = id;
    ask_role(0, "What are they to you?");
}

static void wz_kind(int c)
{
    if (c < 0 || c > 3) return;                 /* 4 = never mind */
    W.kind = c;
    W.name[0] = 0;
    W.pronoun = -1;
    if (family_count() >= family_cap()) { result(FAM_ERR_FULL, ""); return; }
    switch (c) {
    case 0: fp_pick("Who moves in?", PICK_LIVING, -1, wz_neighbour); break;
    case 1: ask_role(0, "What are they to you?"); break;
    case 2: W.role = FAMROLE_PET; wz_ask_name(); break;
    default: W.role = FAMROLE_CHILD; wz_ask_name(); break;
    }
}

static void add_member(void)
{
    char buf[48];
    doomo_snprintf(buf, sizeof(buf), "Add to the household (%d/%d)", family_count(), family_cap());
    ui_choice(-1, buf, kind_opts, 5, wz_kind);
}

/* ---- care for everyone -------------------------------------------------- */
#define NEED_LOW 40
/* The weakest need of a resident member (0 = none worth mentioning). */
static int weakest_need(const DoomoResident *r, int *value)
{
    static const uint8_t needs[6] = { STAT_HUNGER, STAT_HAPPY, STAT_HYGIENE, STAT_ENERGY,
                                      STAT_HEALTH, STAT_SANITY };
    int i, best = -1, bv = 101;
    for (i = 0; i < 6; i++)
        if (r->stat[needs[i]] < bv) { bv = r->stat[needs[i]]; best = needs[i]; }
    if (value) *value = bv;
    return bv < NEED_LOW ? best : -1;
}

/* Days a pool member went without care (0 for anyone else). */
static int days_uncared(int id)
{
    const FamilyMember *m = family_find(id);
    if (!m || id < FAMILY_POOL_FIRST) return 0;
    return (m->flags & FAMILY_FL_NEGLECT) >> FAMILY_NEGLECT_SHIFT;
}

static int needs_care(int id)
{
    const DoomoResident *r = id < DOOMO_MAX_RESIDENTS ? fp_res(id) : NULL;
    if (r) return fp_alive(id) && weakest_need(r, NULL) >= 0;
    return days_uncared(id) >= 2;
}

static void care_one(int id, int kind)
{
    if (id < DOOMO_MAX_RESIDENTS && fp_alive(id)) {
        switch (kind) {
        case CARE_PLAY: sim_play(id); break;
        case CARE_TALK: sim_talk(id); break;
        case CARE_CLEAN: sim_clean(id); break;
        default: sim_comfort(id); break;
        }
    }
    family_care(id);
}

static void care_done(int n, int helped, int kind)
{
    char buf[56];
    if (!n) { ui_say(-1, "Nobody lives with you yet. Add someone!"); return; }
    audio_sfx(SFX_HAPPY);
    fp_fx_burst(fp_sw() / 2, 70, kind == CARE_PLAY ? FX_STAR : FX_HEART, 14);
    if (helped) doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " Looked after %d (%d needed it most)", n, helped);
    else doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " Looked after %d of the family", n);
    ui_toast(buf);
}

static void care_all(int c)
{
    int i, n = 0, helped = 0, m = family_count();
    if (c < 0 || c > 3) return;                 /* 4 = never mind */
    for (i = 0; i < m; i++) {
        int id = family_member(i);
        if (id < 0) continue;
        helped += needs_care(id);
        care_one(id, c);
        n++;
    }
    care_done(n, helped, c);
}

/* Quick care: each member gets the kind of care they need most. */
static void care_quick(void)
{
    int i, n = 0, helped = 0, m = family_count();
    for (i = 0; i < m; i++) {
        int id = family_member(i), kind = CARE_TALK;
        const DoomoResident *r;
        if (id < 0) continue;
        r = id < DOOMO_MAX_RESIDENTS ? fp_res(id) : NULL;
        if (r) {
            int need = weakest_need(r, NULL);
            helped += need >= 0;
            switch (need) {
            case STAT_HYGIENE: kind = CARE_CLEAN; break;
            case STAT_HAPPY: kind = CARE_PLAY; break;
            case STAT_SANITY: case STAT_HEALTH: case STAT_HUNGER: case STAT_ENERGY: kind = CARE_COMFORT; break;
            default: kind = CARE_TALK; break;
            }
        } else {
            helped += days_uncared(id) >= 2;
            kind = CARE_PLAY;
        }
        care_one(id, kind);
        n++;
    }
    care_done(n, helped, CARE_TALK);
}

/* "Check on everyone": who needs what, in a few kind lines. */
static void care_report(void)
{
    int i, m = family_count(), said = 0, fine = 0;
    char buf[88], nm[DOOMO_NAME_LEN];
    if (!m) { ui_say(-1, "Nobody lives with you yet. Add someone!"); return; }
    for (i = 0; i < m; i++) {
        int id = family_member(i), v = 0, need;
        const DoomoResident *r;
        if (id < 0) continue;
        member_name(id, nm);
        r = id < DOOMO_MAX_RESIDENTS ? fp_res(id) : NULL;
        if (r && fp_alive(id) && (need = weakest_need(r, &v)) >= 0) {
            if (said < 4) {
                doomo_snprintf(buf, sizeof(buf), "%s is low on %s (%d%%).", nm, fp_stat_names[need], v);
                ui_say(-1, buf);
            }
            said++;
        } else if (!r && days_uncared(id) >= 2) {
            if (said < 4) {
                doomo_snprintf(buf, sizeof(buf), "%s hasn't had attention for %d days.", nm, days_uncared(id));
                ui_say(-1, buf);
            }
            said++;
        } else fine++;
    }
    if (said > 4) {
        doomo_snprintf(buf, sizeof(buf), "...and %d more could use some love.", said - 4);
        ui_say(-1, buf);
    }
    if (!said) ui_say(-1, "Everyone is doing fine. What a happy household!");
    else if (fine) {
        doomo_snprintf(buf, sizeof(buf), "The other %d are doing fine.", fine);
        ui_say(-1, buf);
    }
}

static void will_pick(int c)
{
    if (c >= 0 && c < 5) { family_freewill_set(c); ui_toast(STR_ICON_HEART " Family free will set."); }
}

static void care_top_pick(int c)
{
    switch (c) {
    case 0: care_quick(); break;
    case 1: ui_choice(-1, "Which kind of care?", care_opts, 5, care_all); break;
    case 2: care_report(); break;
    case 3: ui_choice(-1, "How much do they look after each other?", will_opts, 5, will_pick); break;
    default: break;
    }
}

/* ---- member menu --------------------------------------------------------- */
static void do_remove(int yes)
{
    if (yes && family_remove(W.target)) { audio_sfx(SFX_CANCEL); ui_toast("They moved out of the household."); }
}

static void do_renamed(int ok)
{
    if (ok) result(family_set_name(W.target, W.name), STR_ICON_STAR " Renamed.");
}

static void member_act(int c)
{
    if (c < 0 || c > 5 || W.acts[c] < 0) return;
    switch (W.acts[c]) {
    case MA_CARE:
        family_care(W.target);
        if (W.target < DOOMO_MAX_RESIDENTS && fp_alive(W.target)) ui_say(W.target, sim_talk(W.target));
        else { audio_sfx(SFX_SQUEAK); ui_toast(STR_ICON_HEART " A little love goes a long way."); }
        break;
    case MA_FEED:
    case MA_PLAY: {
        int hungry = family_pet_hungry(W.target), feed = W.acts[c] == MA_FEED;
        char nm[DOOMO_NAME_LEN], msg[64];
        if (!family_pet_tend(W.target, feed ? FAMPET_FEED : FAMPET_PLAY)) break;
        member_name(W.target, nm);
        doomo_snprintf(msg, sizeof(msg), feed ? (hungry ? "%s wolfs it all down!" : "%s munches happily.")
                                              : "%s plays until it flops over.", nm);
        audio_sfx(SFX_SQUEAK);
        ui_toast(msg);
        break; }
    case MA_ROLE: ask_role(1, "New role?"); break;
    case MA_RENAME:
        member_name(W.target, W.name);
        ui_text_entry("New name?", W.name, DOOMO_NAME_LEN, do_renamed);
        break;
    case MA_PROFILE: fp_open_resident(W.target); break;
    case MA_PLAYAS:
        if (!playas_available()) ui_say(-1, "Playing as your family unlocks after you finish the story.");
        else if (!playas_can_be(W.target)) ui_say(-1, "You can't play as them right now.");
        else if (playas_switch(W.target, PLAYAS_SWITCH_TAGIN)) ui_toast(STR_ICON_STAR " Now playing as them!");
        break;
    default: ui_confirm("Move them out of the household?", do_remove); break;
    }
}

static void open_member(int id)
{
    const FamilyMember *m = family_find(id);
    int n = 0;
    char q[40], nm[DOOMO_NAME_LEN];
    if (!m) return;
    W.target = id;
    if (m->role == FAMROLE_PET) {              /* F5: a pet is fed and played with, never "played as" */
        W.acts[n] = MA_FEED; W.opts[n++] = family_pet_hungry(id) ? "Feed (hungry!)" : "Feed";
        W.acts[n] = MA_PLAY; W.opts[n++] = "Play";
    } else {
        W.acts[n] = MA_CARE; W.opts[n++] = "Spend time together";
        W.acts[n] = MA_ROLE; W.opts[n++] = "Change role";
    }
    if (id >= FAMILY_POOL_FIRST) { W.acts[n] = MA_RENAME; W.opts[n++] = "Rename"; }
    else { W.acts[n] = MA_PROFILE; W.opts[n++] = "Open profile"; }
    if (id != playas_avatar() && m->role != FAMROLE_PET) {
        /* say up front when the story still locks it, instead of a dialog after the pick */
        W.acts[n] = MA_PLAYAS;
        W.opts[n++] = playas_available() ? "Play as them" : "Play as them (locked)";
    }
    W.acts[n] = MA_REMOVE; W.opts[n++] = "Move out";
    W.acts[n] = -1; W.opts[n++] = "Never mind";
    doomo_snprintf(q, sizeof(q), "%s (%s)", member_name(id, nm), role_names[m->role % FAMROLE_COUNT]);
    ui_choice(id < DOOMO_MAX_RESIDENTS ? id : -1, q, W.opts, n, member_act);
}

/* ---- the Remembered page: leave flowers (F6) --------------------------------- */
static void mem_flowers(int c)
{
    int idx = W.target;
    if (c != 0) return;
    if (family_memorial_flowers(idx) >= FAMILY_MEM_FLOWERS_MAX) { ui_toast("Their memorial is covered in flowers already."); return; }
    if (inv_count(ITEM_FLOWERS) <= 0 && !opt_get(OPT_CHEAT_INFINITE_CANDY)) {
        audio_sfx(SFX_ERROR);
        ui_toast("You need Flowers (the shop sells them).");
        return;
    }
    if (!family_memorial_leave_flowers(idx)) return;
    if (!opt_get(OPT_CHEAT_INFINITE_CANDY)) inv_take(ITEM_FLOWERS, 1);
    audio_sfx(SFX_SPARKLE);
    fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_PETAL, 14);
    fp_achieve(UL_FP_GARDENER);
    ui_toast(STR_ICON_HEART " You leave a little posy.");
}

static void open_memorial(int idx)
{
    static const char *const opts[2] = { "Leave flowers", "Never mind" };
    char nm[DOOMO_NAME_LEN], q[48];
    int role = family_memorial_role(idx);
    if (!family_memorial_name(idx, nm))
        fp_strcpy(nm, role == FAMROLE_PET ? "Your pet" : role == FAMROLE_CHILD ? "Your child" : "Someone dear",
                  DOOMO_NAME_LEN);
    W.target = idx;
    doomo_snprintf(q, sizeof(q), "In memory of %s.", nm);
    ui_choice(-1, q, opts, 2, mem_flowers);
}

/* ---- scene --------------------------------------------------------------- */
static void fam_enter(int arg)
{
    memset(&W, 0, sizeof(W));
    if (arg == 1 && family_count() < family_cap()) add_member();   /* from a new house: build the family */
}

static void fam_update(void)
{
    int n;
    fp_live();
    W.t++;
    if (btn_pressed(BTN_L | BTN_R | BTN_SELECT)) { W.tab ^= 1; W.sel = W.top = 0; audio_sfx(SFX_BLIP); return; }
    n = W.tab == 0 ? rows() : family_memorial_count();
    if (n) {
        if (btn_repeat(BTN_UP)) { W.sel = (W.sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { W.sel = (W.sel + 1) % n; audio_sfx(SFX_CURSOR); }
        W.sel = DOOMO_MIN(W.sel, n - 1);
        if (W.sel < W.top) W.top = W.sel;
        if (W.sel >= W.top + vis()) W.top = W.sel - vis() + 1;
    }
    if (W.tab == 0 && btn_pressed(BTN_A)) {
        int m = family_count();
        audio_sfx(SFX_SELECT);
        if (W.sel < m) open_member(family_member(W.sel));
        else if (W.sel == m) ui_choice(-1, "Care for everyone", care_top, 5, care_top_pick);
        else if (W.sel == m + 1) add_member();
        else if (playas_switch(back_target(), PLAYAS_SWITCH_TAGIN)) ui_toast(STR_ICON_STAR " Switched back!");
        return;
    }
    if (W.tab == 1 && n && btn_pressed(BTN_A)) { audio_sfx(SFX_SELECT); open_memorial(W.sel); return; }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else fp_goto_island(FP_ISLAND_ARG_RETURN);
    }
}

static uint8_t role_icon(int role)
{
    switch (role) {
    case FAMROLE_PARTNER: return ICON_HEART;
    case FAMROLE_CHILD: return ICON_EGG;
    case FAMROLE_PET: return ICON_STAR;
    case FAMROLE_HOUSEMATE: return ICON_SMILE;
    default: return ICON_HOUSE;
    }
}

/* F5: a 12 x 10 pixel pet for the household rows, one look per kind, bobbing a little. */
static int row_pet = -1;           /* set before draw_row: draw this pet kind instead of the icon */

static void pet_sprite(int x, int y, int kind, int t)
{
    uint8_t eye = PAL_RAMP(RAMP_STONE, 0), pale = PAL_WHITE;
    uint8_t body;
    y += ui_wave(t + kind * 9, 40, 1);
    switch (kind) {
    case FAMPET_IMPCAT:                       /* pink cat with little horns and a curly tail */
        body = PP(PAINT_ROSE, 1);
        plat_rect(x + 2, y + 4, 8, 5, body);  plat_rect(x + 3, y + 1, 6, 4, body);
        plat_rect(x + 3, y, 1, 1, PAL_UI_RED); plat_rect(x + 8, y, 1, 1, PAL_UI_RED);
        plat_rect(x + 10, y + 3, 1, 5, body); plat_rect(x + 9, y + 3, 1, 1, body);
        break;
    case FAMPET_HOUND:                        /* brown pup, floppy ears, a red collar */
        body = PAL_RAMP(RAMP_WOOD, 3);
        plat_rect(x + 1, y + 4, 9, 5, body);  plat_rect(x + 3, y + 1, 6, 4, body);
        plat_rect(x + 2, y + 1, 1, 3, PAL_RAMP(RAMP_WOOD, 1)); plat_rect(x + 9, y + 1, 1, 3, PAL_RAMP(RAMP_WOOD, 1));
        plat_rect(x + 4, y + 5, 4, 1, PAL_UI_RED); plat_rect(x + 10, y + 4, 1, 2, body);
        break;
    case FAMPET_BAT:                          /* grey bat, wings out */
        body = PAL_RAMP(RAMP_STONE, 3);
        plat_rect(x + 4, y + 2, 4, 6, body);
        plat_rect(x, y + 3, 4, 3, PAL_RAMP(RAMP_STONE, 2)); plat_rect(x + 8, y + 3, 4, 3, PAL_RAMP(RAMP_STONE, 2));
        plat_rect(x + 4, y + 1, 1, 1, body); plat_rect(x + 7, y + 1, 1, 1, body);
        break;
    case FAMPET_SLUG:                         /* a yellow slug with feelers */
        body = PP(PAINT_LEMON, 1);
        plat_rect(x + 1, y + 6, 10, 3, body); plat_rect(x + 8, y + 3, 3, 4, body);
        plat_rect(x + 8, y + 1, 1, 2, body);  plat_rect(x + 10, y + 1, 1, 2, body);
        break;
    case FAMPET_CROW:                         /* black crow with a gold beak */
        body = PAL_RAMP(RAMP_STONE, 1);
        plat_rect(x + 3, y + 4, 7, 5, body);  plat_rect(x + 2, y + 1, 5, 4, body);
        plat_rect(x, y + 3, 2, 1, PAL_UI_GOLD); plat_rect(x + 9, y + 6, 3, 1, body);
        pale = PAL_UI_GOLD;
        break;
    default:                                  /* bone hamster: round, pink, holding a bone */
        body = PP(PAINT_BUBBLEGUM, 1);
        plat_rect(x + 2, y + 3, 8, 6, body);  plat_rect(x + 3, y + 2, 6, 1, body);
        plat_rect(x + 2, y + 1, 2, 2, body);  plat_rect(x + 8, y + 1, 2, 2, body);
        plat_rect(x + 4, y + 8, 4, 1, pale);
        break;
    }
    plat_rect(x + 4, y + 3, 1, 1, eye);
    plat_rect(x + 7, y + 3, 1, 1, eye);
    if (kind == FAMPET_BAT || kind == FAMPET_SLUG) plat_rect(x + 5, y + 5, 2, 1, pale);
}

static void draw_row(int x, int y, int w, int rh, int sel, uint8_t icon, uint8_t color,
                     const char *line1, const char *line2)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    if (sel) {
        fp_pill(x + 6, y, w - 8, rh - 2, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
        ui_cursor(x - 2, y + 4);
    }
    if (row_pet >= 0) pet_sprite(x + 8, y + 3, row_pet, W.t);
    else fp_icon(x + 10, y + 3, icon, color);
    row_pet = -1;
    ui_text(x + 22, y + 1, line1, st->text);
    if (line2) ui_text(x + 22, y + 11, line2, st->text_dim);
}

static void fam_draw(void)
{
    int sw = fp_sw(), big = fp_big(), i, k, x = 8, w = sw - 16;
    int y0 = big ? 50 : 40, rh = 24, n = W.tab == 0 ? rows() : family_memorial_count();
    const UiStyle *st = ui_style(WIN_CUTE);
    char buf[64], nm[DOOMO_NAME_LEN], l2[48];
    ui_backdrop(WIN_CUTE, (uint32_t)fp_t());
    ui_window(4, 2, sw - 8, big ? 30 : 24, WIN_CUTE);   /* ends above the tabs (y0 - 13) at both sizes */
    ui_text_center(sw / 2, 8, "Family", st->text);
    doomo_snprintf(buf, sizeof(buf), "Household: %d of %d", family_count(), family_cap());
    ui_text_center(sw / 2, big ? 22 : 17, buf, st->text_dim);
    ui_tabs(x, y0 - 13, tabs, 2, W.tab, WIN_CUTE);
    ui_window(x - 2, y0 - 2, w + 4, vis() * rh + 4, WIN_CUTE);
    if (W.tab == 1 && !n)
        ui_text_center(sw / 2, y0 + 20, "Nobody to remember. Hug your family!", st->text_dim);
    for (i = 0; i < vis() && W.top + i < n; i++) {
        int idx = W.top + i, y = y0 + i * rh, sel = idx == W.sel;
        if (W.tab == 0) {
            int m = family_count();
            if (idx < m) {
                int id = family_member(idx);
                const FamilyMember *f = family_find(id);
                if (!f) continue;
                doomo_snprintf(l2, sizeof(l2), "%s - %s", role_names[f->role % FAMROLE_COUNT],
                               f->role == FAMROLE_PET ? "Pet" : doomo_life_stage_names[f->stage % LIFE_COUNT]);
                if (family_stage_days_left(id) > 0)
                    doomo_snprintf(l2, sizeof(l2), "%s - %s in %dd", doomo_life_stage_names[f->stage % LIFE_COUNT],
                                   doomo_life_stage_names[life_stage_next(f->stage) % LIFE_COUNT],
                                   family_stage_days_left(id));
                if (f->role == FAMROLE_PET)
                    doomo_snprintf(l2, sizeof(l2), "%s - %s", family_pet_kind_name(family_pet_kind(id)),
                                   family_pet_hungry(id) ? "Hungry!" : "Fed");
                if (id == family_away_id()) doomo_snprintf(l2, sizeof(l2), "Away helping today");
                else if (family_bond_line(id)) doomo_snprintf(l2, sizeof(l2), "%s", family_bond_line(id));
                else if (family_close_line(id)) doomo_snprintf(l2, sizeof(l2), "%s", family_close_line(id));
                if (f->role == FAMROLE_PET) row_pet = family_pet_kind(id);
                draw_row(x, y, w, rh, sel, role_icon(f->role), PP(PAINT_ROSE, 1), member_name(id, nm), l2);
                if (id == playas_avatar()) fp_tag(x + w - 30, y + 3, "you", PAL_UI_GOLD, PAL_WHITE);
                else if (needs_care(id) || family_pet_hungry(id)) fp_bubble(x + w - 16, y + 12, ICON_CLOUD, PAL_UI_RED, W.t / 6);
            } else if (idx == m) {
                draw_row(x, y, w, rh, sel, ICON_HEART, PAL_UI_RED, "Care for everyone", "Play, talk, clean, comfort");
            } else if (idx == m + 1) {
                int full = family_count() >= family_cap();
                draw_row(x, y, w, rh, sel, ICON_STAR, PAL_UI_GOLD, "Add a member",
                         full ? "The household is full" : "Neighbour, new person, pet, baby");
            } else {
                member_name(back_target(), nm);
                draw_row(x, y, w, rh, sel, ICON_SMILE, PAL_UI_GOLD, "Switch back", nm);
            }
        } else {
            const FamilyMemorial *mm = family_memorial(idx);
            int stage = family_memorial_stage(idx), role = family_memorial_role(idx);
            if (!mm) continue;
            if (!family_memorial_name(idx, nm))
                fp_strcpy(nm, role == FAMROLE_PET ? "Your pet" : role == FAMROLE_CHILD ? "Your child" : "Someone dear",
                          DOOMO_NAME_LEN);
            if (LIFE_IS_MINOR(stage) || role == FAMROLE_PET)
                doomo_snprintf(l2, sizeof(l2), "Day %u - always remembered", (unsigned)mm->day);
            else
                doomo_snprintf(l2, sizeof(l2), "Day %u - %s", (unsigned)mm->day,
                               cause_names[mm->cause < FAMLOSS_COUNT ? mm->cause : 0]);
            draw_row(x, y, w, rh, sel, ICON_GHOST, PP(PAINT_CYAN, 0), nm, l2);
            for (k = 0; k < family_memorial_flowers(idx); k++) {        /* the posies left here */
                plat_rect(x + w - 14 + k * 4, y + 12, 1, 4, PAL_RAMP(RAMP_GRASS, 4));
                plat_rect(x + w - 15 + k * 4, y + 10, 3, 2, k == 1 ? PP(PAINT_LEMON, 1) : PP(PAINT_BUBBLEGUM, 1));
            }
        }
    }
    if (n > vis()) ui_scroll_arrows(x + w - 4, y0 - 6, y0 + vis() * rh - 2, W.top > 0, W.top + vis() < n);
    ui_hint(W.tab ? STR_ICON_A " Flowers  L/R Page  " STR_ICON_B " Back" : STR_ICON_A " Open  L/R Page  " STR_ICON_B " Back", WIN_CUTE);
    fp_fx_draw();
}

const DoomoScene SCENE_FAMILY = { "FAMILY", fam_enter, NULL, fam_update, fam_draw, 0 };
