/* home_draw.c - SCENE_HOME, the dream house scene (owner: H4; HF sec.2.1, 5,
 * SPEC sec.7 H4, C9, C14).
 *
 * M1 is the shell: the room drawn through plat_room() (room_ref.c until the
 * GBA / PS1 renderers land), the 12 px HUD bars, Live and Build modes, the
 * START menu with the catalog and the Score panel, and the core Build
 * animations of HF sec.5.1 (heart iris in, pick-up anticipation, the hover
 * ghost, the soft DROP, the nope, the squash-flip, the view brake, undo).
 * Visits (arg 2 + id) and the story modes (0x4000 | mode) open the player's
 * Living Room until M2 adds home_visit.c and the story hooks.
 *
 * Controls (Build): D-pad moves the cursor one tile (rotated with the view),
 * A picks up / places, B puts the held piece back (or leaves), L / R rotate
 * the held piece or, empty-handed, the view, SELECT undoes, START opens the
 * menu. Live: the D-pad walks, START opens the menu, B leaves.
 *
 * State: the scene scratch ('HOME', about 2 KB); its tail is the view's
 * plat_cache (the reference renderer decodes sprites there; the PS1 uses it
 * as its vertex cache, C9/C17). Nothing here is saved: on leave the working
 * room is stored into HOME (home_room_store) and the cache refreshed.
 *
 * Timing: every frame count lives in the table below (HF sec.5.1, SPEC risk 6:
 * no literal frame counts in home_*.c outside tables); the curves are
 * tween.h's (TW_DROP_SOFT_*, tw_in2, the TW_FOLLOW spring).
 */
#include "home_int.h"
#include "doomo/tween.h"
#include "doomo/audio.h"
#include "doomo/portrait.h"

#define HD_TAG SAVE_EXT_TAG('H', 'O', 'M', 'E')

/* ---- the timing table (frames at 30 Hz; HF sec.5.1) --------------------------- */
enum { HT_IRIS, HT_LIFT, HT_DROP_FALL, HT_DROP_LAND, HT_NOPE, HT_FLIP, HT_FLIP_SWAP,
       HT_BRAKE, HT_BRAKE_SHAKE, HT_MSG, HT_CUR_REPEAT, HT_GHOST_PERIOD, HT_BLINK,
       HT_WALK_FRAME, HT_UNDO_POP, HT_COUNT };
static const uint8_t ht[HT_COUNT] = {
    6,      /* HT_IRIS        heart iris in                                    */
    3,      /* HT_LIFT        pick-up lift (after a 2-frame anticipation)      */
    6,      /* HT_DROP_FALL   DROP soft: t^2 fall                              */
    6,      /* HT_DROP_LAND   the s14 impact table's length                    */
    4,      /* HT_NOPE        x shake +2 -2 +2 -2                              */
    5,      /* HT_FLIP        squash-flip of the held piece                    */
    3,      /* HT_FLIP_SWAP   the edge-on frame where the facing swaps         */
    10,     /* HT_BRAKE       view rotate: the hold                            */
    2,      /* HT_BRAKE_SHAKE the shear wobble at its start                    */
    60,     /* HT_MSG         a nope caption stays                             */
    6,      /* HT_CUR_REPEAT  cursor auto-repeat                               */
    39,     /* HT_GHOST_PERIOD the breathing period of the hover bob           */
    16,     /* HT_BLINK       cursor blink                                     */
    6,      /* HT_WALK_FRAME  chibi walk frame                                 */
    4,      /* HT_UNDO_POP    reverse DROP                                     */
};
static const int8_t nope_dx[4] = { 2, -2, 2, -2 };
static const int16_t flip_sx[6] = { 256, 128, 0, -128, -266, -256 };   /* Q8 */
#define DROP_FROM_PX 24          /* GBA px the DROP falls (HF sec.5.1)        */
#define LIFT_PX      6

enum { HM_LIVE, HM_BUILD };
enum { HMN_NONE, HMN_MAIN, HMN_CATALOG, HMN_SCORE };
enum { MI_CATALOG, MI_MODE, MI_SCORE, MI_LEAVE, MI_COUNT };

typedef struct HomeScn {
    HomeEdit ed;
    uint8_t  mode, menu, view_k, room, kind;
    int8_t   cx, cy;                 /* cursor tile (room space)            */
    int16_t  held;                   /* furn in hand, -1 none               */
    uint8_t  held_rot, held_from_room;   /* 1 = picked up from the room     */
    uint8_t  held_pa, held_pb;
    int16_t  drop_idx;               /* object playing the DROP, -1         */
    uint8_t  drop_age, nope_age, flip_age, brake_age, lift_age, undo_age;
    int8_t   brake_dir;
    uint16_t t, msg_age;
    const char *msg;                 /* ROM strings only                    */
    TwSpring cur_x, cur_y;           /* the cursor glide (TW_FOLLOW)        */
    fix16    px, py;                 /* the player chibi (Live)             */
    uint8_t  pdir, pwalk;
    uint16_t score_gen;
    HomeScore score;
    UiList   list;
    uint16_t ncat;
    uint16_t cat[FURN_MAX];          /* the catalog modal's rows            */
    const char *cat_name[FURN_MAX];  /* their names (no item_fn: a function
                                        pointer would pull the home group
                                        resident on the PS1, SPEC C6)      */
    DoomoRoomObj objs[HOME_OBJ_MAX + 1];
} HomeScn;

static HomeScn *H;

static const char *const menu_items_build[MI_COUNT] = { "Catalog", "Live mode", "Score", "Leave" };
static const char *const menu_items_live[MI_COUNT] = { "Catalog", "Build mode", "Score", "Leave" };
static const char *const stat_names[HS_COUNT] = { "Comfort", "Spook", "Style", "Light", "Barricade" };
static const char *const grade_names[HG_COUNT] = { "-", "COSY", "LIT", "HELD", "HOME", "ETERNAL" };
static const uint8_t stat_cols[HS_COUNT] = { PAL_UI_PINK, PAL_UI_PURPLE, PAL_UI_MINT, PAL_UI_GOLD, PAL_UI_RED };

/* ---- helpers ------------------------------------------------------------------ */
static int room_w(void) { return HR_W(&H->ed.room); }
static int room_d(void) { return HR_D(&H->ed.room); }

void home_obj_view(const HomeObj *o, DoomoRoomObj *out)
{
    const DoomoFurn *f = home_furn(HO_FURN(o));
    int pa = o->pa & 0x1F, pb = o->pb & 0x1F;
    memset(out, 0, sizeof(*out));
    out->furn = HO_FURN(o);
    out->x = (uint8_t)HO_X(o);
    out->y = (uint8_t)HO_Y(o);
    out->z = (uint8_t)(o->z * 4);
    out->rot_anim = RO_ROT_ANIM(HO_ROT(o), 0);
    out->paint_a = f ? (pa == FURN_PAINT_DEFAULT ? f->paint_a : (uint8_t)pa) : 0xFF;
    out->paint_b = f ? (pb == FURN_PAINT_DEFAULT ? f->paint_b : (uint8_t)pb) : 0xFF;
    if (f && (f->flags & FF_LIGHT) && !(o->pa & HOS_OFF)) out->flags |= RO_FULLBRIGHT;
}

/* view-space d-pad delta -> room space (inverse of the view's k turns) */
static void view_to_room(int k, int *dx, int *dy)
{
    int i, t;
    for (i = 0; i < k; i++) { t = *dx; *dx = *dy; *dy = -t; }
}

static int dpad(int *dx, int *dy)
{
    *dx = *dy = 0;
    if (btn_repeat(BTN_UP)) *dy = -1;
    else if (btn_repeat(BTN_DOWN)) *dy = 1;
    else if (btn_repeat(BTN_LEFT)) *dx = -1;
    else if (btn_repeat(BTN_RIGHT)) *dx = 1;
    if (!*dx && !*dy) return 0;
    view_to_room(H->view_k, dx, dy);
    return 1;
}

static void say(const char *m)
{
    H->msg = m;
    H->msg_age = 0;
}

static void nope(int rc)
{
    H->nope_age = 0;
    audio_sfx(SFX_ERROR);
    if (rc > 0 && rc < HPR_COUNT) say(home_nope_text[rc]);
}

static int light_level(void)
{
    int m = doomo_town.minute, l;
    const HomeRoomDef *d = &home_room_defs[H->room];
    if (m >= 7 * 60 && m < 18 * 60) l = 15;
    else if (m >= 5 * 60 && m < 7 * 60) l = 10;
    else if (m >= 18 * 60 && m < 20 * 60) l = 11;
    else l = 5;
    if (d->dim && l > d->dim) l = d->dim;
    return l;
}

static void rescore(void)
{
    if (H->score_gen == H->ed.gen) return;
    H->score_gen = H->ed.gen;
    home_score_room(&H->ed.room, H->room,
                    (doomo_town.minute < 6 * 60 || doomo_town.minute >= 20 * 60) ? HSC_NIGHT : 0, &H->score);
}

static void commit(void)
{
    if (!H) return;
    home_room_store(H->room, &H->ed.room);
    home_refresh_cache(&H->ed.room, H->room);
}

static void build_catalog(void)
{
    int i, n = home_furn_count();
    H->ncat = 0;
    for (i = 0; i < n && H->ncat < FURN_MAX; i++) {
        const DoomoFurn *f = home_furn(i);
        if (!f || (f->flags2 & FF_PORTAL)) continue;
        if (home_edit_avail(&H->ed, i) > 0) {
            H->cat_name[H->ncat] = f->name;
            H->cat[H->ncat++] = (uint16_t)i;
        }
    }
}

/* ---- the scene ------------------------------------------------------------------ */
static void home_enter(int arg)
{
    int room = HOME_ROOM_LIVING;
    if (arg == 0 || arg == 1) {             /* reserved (SPEC C14): refused */
        plat_log("home: refused arg");
        H = NULL;
        return;
    }
    H = (HomeScn *)scene_scratch(HD_TAG, (uint32_t)sizeof(HomeScn));
    if (!H) return;
    memset(H, 0, sizeof(*H));
    if (HOME_ARG_IS_OWN(arg)) {
        HomeState *s = home_state();
        if (s && !home_moved_in()) home_move_in();
        if (s && arg == HOME_ARG_OWN(HOME_COTTAGE) && home_room_open(s->active_room)) room = s->active_room;
        else if (arg == HOME_ARG_OWN(HOME_TOWERS_7B)) room = HOME_ROOM_TOWERS_7B;
        else if (arg == HOME_ARG_OWN(HOME_PITS_FLAT)) room = HOME_ROOM_PITS_FLAT;
        else if (arg == HOME_ARG_OWN(HOME_PARK_PLOT)) room = HOME_ROOM_PARK_PLOT;
        H->kind = 0;
    } else {
        H->kind = HOME_ARG_IS_VISIT(arg) ? 1 : 2;   /* M2: visits and story modes */
    }
    H->room = (uint8_t)room;
    home_edit_begin(&H->ed, room);
    H->mode = HM_BUILD;
    H->held = -1;
    H->drop_idx = -1;
    H->cx = (int8_t)(room_w() / 2);
    H->cy = (int8_t)(room_d() / 2);
    tw_spring_init(&H->cur_x, TW_FOLLOW, H->cx * 16 * 16);
    tw_spring_init(&H->cur_y, TW_FOLLOW, H->cy * 16 * 16);
    H->px = INT2FIX(room_w() / 2) + FIX_ONE / 2;
    H->py = INT2FIX(room_d() - 1) + FIX_ONE / 2;
    H->nope_age = H->flip_age = H->brake_age = H->lift_age = H->undo_age = 0xFF;
    H->drop_age = 0xFF;
    H->msg_age = 0xFFFF;
    H->score_gen = 0xFFFF;
    audio_sfx(SFX_SHWIP);
}

static void home_leave(void)
{
    if (H && scene_scratch_mine(HD_TAG)) commit();
    H = NULL;
}

static void leave_now(void)
{
    commit();
    scene_pop();
}

static void hand_from_catalog(int furn)
{
    const DoomoFurn *f = home_furn(furn);
    if (!f) return;
    H->held = (int16_t)furn;
    H->held_rot = 0;
    H->held_from_room = 0;
    H->held_pa = H->held_pb = FURN_PAINT_DEFAULT;
    H->lift_age = 0;
    H->mode = HM_BUILD;
    audio_sfx(SFX_POP2);
}

static void update_menu(void)
{
    int r = ui_list_update(&H->list);
    if (r == UI_CANCEL) { H->menu = HMN_NONE; audio_sfx(SFX_CANCEL); return; }
    if (r != UI_SELECT) return;
    if (H->menu == HMN_MAIN) {
        switch (H->list.sel) {
        case MI_CATALOG:
            build_catalog();
            memset(&H->list, 0, sizeof(H->list));
            H->list.items = H->cat_name;
            H->list.count = H->ncat;
            H->list.visible = plat_screen_h() >= 200 ? 9 : 6;
            H->menu = HMN_CATALOG;
            if (!H->ncat) { say("Storage is empty."); H->menu = HMN_NONE; }
            break;
        case MI_MODE:
            H->mode = H->mode == HM_BUILD ? HM_LIVE : HM_BUILD;
            H->menu = HMN_NONE;
            break;
        case MI_SCORE:
            rescore();
            H->menu = HMN_SCORE;
            break;
        default:
            H->menu = HMN_NONE;
            leave_now();
            break;
        }
    } else if (H->menu == HMN_CATALOG) {
        if (H->list.sel >= 0 && H->list.sel < H->ncat) hand_from_catalog(H->cat[H->list.sel]);
        H->menu = HMN_NONE;
    } else {
        H->menu = HMN_NONE;
    }
}

static void open_main_menu(void)
{
    memset(&H->list, 0, sizeof(H->list));
    H->list.items = H->mode == HM_BUILD ? menu_items_build : menu_items_live;
    H->list.count = MI_COUNT;
    H->list.visible = MI_COUNT;
    H->menu = HMN_MAIN;
    audio_sfx(SFX_SELECT);
}

static void update_build(void)
{
    int dx, dy;
    if (dpad(&dx, &dy)) {
        int nx = H->cx + dx, ny = H->cy + dy;
        if (nx >= 0 && ny >= 0 && nx < room_w() && ny < room_d()) {
            H->cx = (int8_t)nx;
            H->cy = (int8_t)ny;
            audio_sfx(SFX_CURSOR);
        }
    }
    if (btn_pressed(BTN_A)) {
        if (H->held >= 0) {
            int rc = home_edit_place(&H->ed, H->held, H->cx, H->cy, H->held_rot,
                                     H->held_pa == FURN_PAINT_DEFAULT ? -1 : H->held_pa,
                                     H->held_pb == FURN_PAINT_DEFAULT ? -1 : H->held_pb);
            if (rc >= 0) {
                H->drop_idx = (int16_t)rc;
                H->drop_age = 0;
                H->held = -1;
                audio_sfx(SFX_WHOOSH);
            } else {
                nope(-rc);
            }
        } else {
            int idx = home_obj_at(&H->ed.room, H->cx, H->cy);
            if (idx >= 0) {
                HomeObj o = H->ed.room.obj[idx];
                int furn = home_edit_pickup(&H->ed, idx);
                if (furn >= 0) {
                    H->held = (int16_t)furn;
                    H->held_rot = (uint8_t)HO_ROT(&o);
                    H->held_from_room = 1;
                    H->held_pa = o.pa & 0x1F;
                    H->held_pb = o.pb & 0x1F;
                    H->lift_age = 0;
                    H->drop_idx = -1;
                    audio_sfx(SFX_POP2);
                } else {
                    nope(-furn);
                }
            }
        }
    } else if (btn_pressed(BTN_B)) {
        if (H->held >= 0) {
            if (H->held_from_room) home_edit_undo(&H->ed);    /* put it back */
            H->held = -1;
            H->undo_age = 0;
            audio_sfx(SFX_CANCEL);
        } else {
            leave_now();
        }
    } else if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        int dir = btn_pressed(BTN_R) ? 1 : -1;
        if (H->held >= 0) {
            H->held_rot = (uint8_t)((H->held_rot + dir) & 3);
            H->flip_age = 0;
            audio_sfx(SFX_WHOOSH);
        } else if (H->brake_age == 0xFF) {
            H->brake_age = 0;
            H->brake_dir = (int8_t)dir;
            audio_sfx(SFX_WHOOSH);
        }
    } else if (btn_pressed(BTN_SELECT) && H->held < 0) {
        if (home_edit_undo(&H->ed)) { H->undo_age = 0; audio_sfx(SFX_SHWIP); }
        else nope(0);
    }
}

static int walkable(int tx, int ty)
{
    int idx;
    const DoomoFurn *f;
    if (tx < 0 || ty < 0 || tx >= room_w() || ty >= room_d()) return 0;
    idx = home_obj_at(&H->ed.room, tx, ty);
    if (idx < 0) return 1;
    f = home_furn(HO_FURN(&H->ed.room.obj[idx]));
    return !f || (f->flags & (FF_RUG | FF_WALL | FF_SMALL)) != 0;
}

static void update_live(void)
{
    int dx = 0, dy = 0;
    if (btn_down(BTN_UP)) dy = -1;
    else if (btn_down(BTN_DOWN)) dy = 1;
    else if (btn_down(BTN_LEFT)) dx = -1;
    else if (btn_down(BTN_RIGHT)) dx = 1;
    if (dx || dy) {
        fix16 nx, ny;
        int vdx = dx, vdy = dy;
        view_to_room(H->view_k, &dx, &dy);
        nx = H->px + dx * (FIX_ONE / 10);
        ny = H->py + dy * (FIX_ONE / 10);
        if (walkable(FIX2INT(nx + dx * (FIX_ONE / 4)), FIX2INT(ny + dy * (FIX_ONE / 4)))) { H->px = nx; H->py = ny; }
        H->pwalk++;
        /* screen direction: down-ish = 0, up-ish = 3; left/right by view */
        H->pdir = (uint8_t)(vdy > 0 || vdx < 0 ? 0 : 3);
        if (vdx > 0 || vdy < 0) H->pdir = 3;
    } else {
        H->pwalk = 0;
    }
    if (btn_pressed(BTN_B)) leave_now();
}

static void home_update(void)
{
    if (!H) { scene_pop(); return; }
    H->t++;
    if (H->msg_age != 0xFFFF && ++H->msg_age > ht[HT_MSG]) H->msg_age = 0xFFFF;
    if (H->drop_age != 0xFF) {
        H->drop_age++;
        if (H->drop_age == ht[HT_DROP_FALL]) audio_sfx(SFX_THUD);
        if (H->drop_age == ht[HT_DROP_FALL] + 1) audio_sfx(SFX_BOING);
        if (H->drop_age > ht[HT_DROP_FALL] + ht[HT_DROP_LAND]) { H->drop_age = 0xFF; H->drop_idx = -1; }
    }
    if (H->nope_age != 0xFF && ++H->nope_age >= ht[HT_NOPE]) H->nope_age = 0xFF;
    if (H->flip_age != 0xFF && ++H->flip_age >= ht[HT_FLIP]) H->flip_age = 0xFF;
    if (H->lift_age != 0xFF && ++H->lift_age > 2 + ht[HT_LIFT]) H->lift_age = 0xFF;
    if (H->undo_age != 0xFF && ++H->undo_age > ht[HT_UNDO_POP]) H->undo_age = 0xFF;
    if (H->brake_age != 0xFF) {
        H->brake_age++;
        if (H->brake_age == ht[HT_BRAKE]) {
            H->view_k = (uint8_t)((H->view_k + H->brake_dir) & 3);
            audio_sfx(SFX_THUD);
        }
        if (H->brake_age > ht[HT_BRAKE]) H->brake_age = 0xFF;
    }
    {
        /* the cursor glides in view space */
        int x0 = H->cx, y0 = H->cy, x1 = x0 + 1, y1 = y0 + 1, i, w = room_w(), d = room_d(), t;
        for (i = 0; i < H->view_k; i++) {
            int nx0 = w - y1, nx1 = w - y0, ny0 = x0, ny1 = x1;
            x0 = nx0; x1 = nx1; y0 = ny0; y1 = ny1;
            t = w; w = d; d = t;
        }
        tw_spring_target(&H->cur_x, (x0 * 16) * 16);
        tw_spring_target(&H->cur_y, (y0 * 16) * 16);
        tw_spring_step(&H->cur_x);
        tw_spring_step(&H->cur_y);
    }
    if (H->menu != HMN_NONE) { update_menu(); return; }
    if (btn_pressed(BTN_START)) { open_main_menu(); return; }
    if (H->mode == HM_BUILD) update_build();
    else update_live();
}

/* ---- drawing --------------------------------------------------------------------- */
static void anim_obj(DoomoRoomObj *o, int idx)
{
    const DoomoFurn *f = home_furn(o->furn);
    if (f && (f->flags & FF_ANIM) && f->frames > 1)
        o->rot_anim = RO_ROT_ANIM(RO_ROT(o), (H->t >> 3) % f->frames);
    if (idx == H->drop_idx && H->drop_age != 0xFF && !(f && (f->flags2 & FF_NO_SQUASH))) {
        int a = H->drop_age;
        if (a < ht[HT_DROP_FALL]) {
            fix16 t = tw_t(a, ht[HT_DROP_FALL]);
            int fall = DROP_FROM_PX - tw_lerp(0, DROP_FROM_PX, tw_in2(t));
            o->z = (uint8_t)(o->z + fall);
            o->sq_x = (int8_t)((218 - 256) / 2);      /* stretched 0.85 / 1.18 */
            o->sq_y = (int8_t)((302 - 256) / 2);
        } else {
            int age = a - ht[HT_DROP_FALL];
            TwSquash s = tw_squash(&TW_DROP_SOFT_SX, &TW_DROP_SOFT_SY, &TW_DROP_SOFT_DY, age, tw_bounce_amount());
            o->sq_x = (int8_t)((s.sx - 256) / 2);
            o->sq_y = (int8_t)((s.sy - 256) / 2);
            if (s.dy < 0 && o->z < (uint8_t)(-s.dy)) o->z = 0; else o->z = (uint8_t)(o->z - s.dy);
        }
    }
}

static int build_view(DoomoRoomView *v, uint8_t *cache, uint32_t cap)
{
    const HomeRoom *r = &H->ed.room;
    const HomeRoomDef *d = &home_room_defs[H->room];
    int i, n = 0;
    memset(v, 0, sizeof(*v));
    v->w = (uint8_t)room_w();
    v->d = (uint8_t)room_d();
    v->wallpaper = r->wallpaper == 0xFF ? d->wallpaper : r->wallpaper;
    v->floor = r->floor == 0xFF ? d->floor : r->floor;
    v->wall_paint = r->wall_paint == 0xFF ? d->wall_paint : r->wall_paint;
    v->floor_paint = r->floor_paint == 0xFF ? d->floor_paint : r->floor_paint;
    v->room_kind = d->kind;
    for (i = 0; i < r->nobj && n < HOME_OBJ_MAX; i++) {
        home_obj_view(&r->obj[i], &H->objs[n]);
        anim_obj(&H->objs[n], i);
        n++;
    }
    if (H->held >= 0 && H->mode == HM_BUILD) {
        DoomoRoomObj *g = &H->objs[n];
        const DoomoFurn *f = home_furn(H->held);
        HomeFit fit;
        int ok = home_fit(r, H->room, H->held, H->cx, H->cy, H->held_rot, -1, &fit) == HPR_OK;
        int bob = 2 + ui_wave(H->t, ht[HT_GHOST_PERIOD], 2);
        memset(g, 0, sizeof(*g));
        g->furn = (uint16_t)H->held;
        g->x = (uint8_t)H->cx;
        g->y = (uint8_t)H->cy;
        g->z = (uint8_t)((ok ? fit.z * 4 : (f && (f->flags & FF_WALL) ? 24 : 0)) + LIFT_PX + bob);
        g->rot_anim = RO_ROT_ANIM(H->held_rot, 0);
        g->paint_a = f ? (H->held_pa == FURN_PAINT_DEFAULT ? f->paint_a : H->held_pa) : 0xFF;
        g->paint_b = f ? (H->held_pb == FURN_PAINT_DEFAULT ? f->paint_b : H->held_pb) : 0xFF;
        g->flags = (uint8_t)(RO_GHOST | (ok ? 0 : RO_INVALID));
        if (H->nope_age != 0xFF) g->jitter = (int8_t)(nope_dx[H->nope_age & 3] * 2);
        if (H->lift_age != 0xFF) {
            if (H->lift_age < 2) { g->sq_x = 13; g->sq_y = -13; }        /* 1.10 / 0.90 */
            else if (H->lift_age < 2 + ht[HT_LIFT]) { g->sq_x = -10; g->sq_y = 13; }
        }
        if (H->flip_age != 0xFF) {
            int sx = flip_sx[H->flip_age < 6 ? H->flip_age : 5];
            if (sx < 0) sx = -sx;
            g->sq_x = (int8_t)((sx < 2 ? 2 - 256 : sx - 256) / 2);
            if (H->flip_age < ht[HT_FLIP_SWAP]) g->rot_anim = RO_ROT_ANIM(H->held_rot - 1, 0);
        }
        n++;
    }
    v->objs = H->objs;
    v->nobj = n;
    v->gen = H->ed.gen;
    v->yaw = (int16_t)(H->view_k * 1024);
    v->punch_q8 = 256;
    v->daylight = (uint8_t)light_level();
    v->cursor_x = (int8_t)(H->mode == HM_BUILD && H->menu == HMN_NONE ? H->cx : -1);
    v->cursor_y = (int8_t)(H->mode == HM_BUILD && H->menu == HMN_NONE ? H->cy : -1);
    v->time = H->t;
    v->vx = 0;
    v->vy = 12;
    v->vw = (int16_t)plat_screen_w();
    v->vh = (int16_t)(plat_screen_h() - 24);
    if (H->brake_age != 0xFF && H->brake_age < ht[HT_BRAKE_SHAKE]) v->pan_x = (int8_t)(H->brake_dir * 2);
    v->plat_cache = cache;
    v->plat_cache_cap = cap;
    return n;
}

static void draw_hud(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, x;
    char buf[48];
    const UiStyle *st = ui_style(WIN_CUTE);
    plat_rect(0, 0, sw, 12, st->fill);
    plat_rect(0, 11, sw, 1, st->outline);
    plat_rect(0, sh - 12, sw, 12, st->fill);
    plat_rect(0, sh - 12, sw, 1, st->outline);
    plat_room_ui_opaque(0, 0, sw, 12);
    plat_room_ui_opaque(0, sh - 12, sw, 12);
    ui_text(3, 2, home_room_defs[H->room].name, st->text);
    doomo_snprintf(buf, sizeof(buf), "%02d:%02d", doomo_town.minute / 60, doomo_town.minute % 60);
    x = 6 + ui_text_width(home_room_defs[H->room].name) + 4;
    ui_text(x, 2, buf, st->text_dim);
    rescore();
    /* the four stat pips */
    x = sw - 4 - 4 * 12;
    for (i = 0; i < 4; i++) {
        int v = H->score.stat[i];
        ui_fill_circle(x + i * 12 + 5, 6, 4, ui_darker(stat_cols[i]));
        ui_fill_circle(x + i * 12 + 5, 6, 1 + v * 3 / 100, stat_cols[i]);
    }
    doomo_snprintf(buf, sizeof(buf), "H$%u", (unsigned)doomo_town.money);
    ui_text(x - 4 - ui_text_width(buf), 2, buf, PAL_UI_GOLD);
    /* the bottom bar: what the buttons do */
    if (H->menu != HMN_NONE) ui_text(3, sh - 10, "A:choose  B:back", st->text);
    else if (H->mode == HM_LIVE) ui_text(3, sh - 10, "Walk  START:menu  B:leave", st->text);
    else if (H->held >= 0) {
        const DoomoFurn *f = home_furn(H->held);
        doomo_snprintf(buf, sizeof(buf), "%s  A:put  L/R:turn", f ? f->name : "?");
        ui_text(3, sh - 10, buf, st->text);
    } else ui_text(3, sh - 10, sw >= 300 ? "A:pick up  L/R:view  SELECT:undo  START:menu"
                                         : "A:pick  L/R:view  SEL:undo", st->text);
    if (H->msg_age != 0xFFFF && H->msg) {
        int w = ui_text_width(H->msg) + 10, y = sh - 28;
        int dy = H->msg_age < 4 ? (4 - H->msg_age) * 2 : 0;
        ui_round_rect((sw - w) / 2, y + dy, w, 13, 4, st->fill_hi);
        ui_text_center(sw / 2, y + 3 + dy, H->msg, st->text);
    }
}

static void draw_catalog(uint8_t *cache, uint32_t cap)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int lw = sw >= 300 ? 150 : 120, x = 6, y = 16, h = sh - 34;
    if (!ui_window_pop(x, y, sw - 12, h, WIN_CUTE, (int)H->t)) return;
    ui_text(x + 6, y + 4, "Catalog", ui_style(WIN_CUTE)->text);
    ui_list_draw(&H->list, x + 4, y + 16, lw, WIN_CUTE);
    if (H->list.sel >= 0 && H->list.sel < H->ncat) {
        int furn = H->cat[H->list.sel], ox, oy, px = x + lw + 10, pw = sw - 12 - lw - 14;
        const DoomoFurn *f = home_furn(furn);
        DoomoImage img;
        char buf[24];
        if (!f) return;
        plat_rect(px, y + 16, pw, h - 52, ui_style(WIN_CUTE)->fill_lo);
        if (room_ref_sprite(furn, (H->t >> 6) & 1, (H->t >> 3) & 3, -1, -1, 15, cache, cap, &img, &ox, &oy)) {
            int bob = ui_wave(H->t, ht[HT_GHOST_PERIOD], 1);
            int ix = px + (pw - img.w) / 2, iy = y + 16 + (h - 52 - img.h) / 2 + bob;
            plat_image(ix, iy, &img, 1, 0, 0);
        }
        ui_text_wrap(px, y + h - 34, pw, f->desc, ui_style(WIN_CUTE)->text, 1);
        doomo_snprintf(buf, sizeof(buf), f->price ? "H$%u" : "gift", (unsigned)f->price);
        ui_text(px, y + h - 12, buf, PAL_UI_GOLD);
        if (home_state()) {
            doomo_snprintf(buf, sizeof(buf), "x%d", home_edit_avail(&H->ed, furn));
            ui_text(px + pw - ui_text_width(buf) - 2, y + h - 12, buf, ui_style(WIN_CUTE)->text_dim);
        }
    }
}

static void draw_score(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, x, y, w = sw >= 300 ? 220 : 180, h = sh >= 200 ? 140 : 110;
    char buf[40];
    x = (sw - w) / 2;
    y = (sh - h) / 2;
    if (!ui_window_pop(x, y, w, h, WIN_CUTE, (int)H->t)) return;
    rescore();
    doomo_snprintf(buf, sizeof(buf), "%s  -  %s", home_room_defs[H->room].name,
                   grade_names[home_stamps_grade(H->score.stamps)]);
    ui_text(x + 8, y + 6, buf, ui_style(WIN_CUTE)->text);
    for (i = 0; i < HS_COUNT; i++) {
        int ry = y + 20 + i * (sh >= 200 ? 15 : 12);
        ui_text(x + 8, ry, stat_names[i], ui_style(WIN_CUTE)->text);
        ui_bar(x + 70, ry + 1, w - 84, 6, H->score.stat[i], 100, stat_cols[i]);
    }
    doomo_snprintf(buf, sizeof(buf), "Pieces: %d  Lamps: %d", H->score.pieces, H->score.lamps);
    ui_text(x + 8, y + h - 14, buf, ui_style(WIN_CUTE)->text_dim);
}

static void home_draw(void)
{
    DoomoRoomView v;
    uint8_t *base = (uint8_t *)H, *cache;
    uint32_t used, cap;
    if (!H) { plat_clear(PAL_BLACK); return; }
    used = ((uint32_t)sizeof(HomeScn) + 255u) & ~255u;
    cache = base + used;
    cap = DOOMO_SCENE_SCRATCH > used ? DOOMO_SCENE_SCRATCH - used : 0;
    build_view(&v, cache, cap);
    {
        DoomoRoomActor a;
        DoomoResident *me = town_res(DOOMO_PLAYER_ID);
        if (me && me->status != RES_EMPTY) {
            int frame = H->pwalk ? (H->pwalk / ht[HT_WALK_FRAME]) & 3 : 0;
            memset(&a, 0, sizeof(a));
            a.img = bodyspr_image(DOOMO_PLAYER_ID, H->pdir, frame);
            a.x = H->px;
            a.y = H->py;
            a.sx = a.sy = 256;
            a.flags = RA_PLAYER;
            v.actors = &a;
            v.nact = 1;
        }
        plat_room(&v);
    }
    draw_hud();
    if (H->menu == HMN_MAIN) {
        int sw = plat_screen_w(), w = 90, h = MI_COUNT * 12 + 12;
        if (ui_window_pop(sw - w - 6, 16, w, h, WIN_CUTE, (int)H->t))
            ui_list_draw(&H->list, sw - w - 2, 22, w - 8, WIN_CUTE);
    } else if (H->menu == HMN_CATALOG) {
        draw_catalog(cache, cap);
    } else if (H->menu == HMN_SCORE) {
        draw_score();
    }
    if (H->t < ht[HT_IRIS]) {
        /* the heart iris in: r = 6 + 250 * smooth(a / 6)^2 */
        fix16 s = tw_smooth(tw_t((int)H->t, ht[HT_IRIS]));
        int r = 6 + (int)(((int64_t)250 * s / FIX_ONE) * s / FIX_ONE);
        plat_post_iris(plat_screen_w() / 2, plat_screen_h() / 2, r, 1, PAL_UI_PINK);
    }
}

const DoomoScene SCENE_HOME = { "HOME", home_enter, home_leave, home_update, home_draw, 0 };
