/* levels_select.c - SCENE_LEVEL_SELECT: the Hell Breach level select
 * (owner: levels). arg = LVM_* mode | (level + 1) << 4 to focus a level
 * (0 = the first level that still needs you).
 *
 * A carousel: a LIVE 3D preview of the level (the real map through
 * plat_view3d in a viewport, the camera slowly orbiting its preview spot,
 * the level's demons, props and goodies standing around as billboards),
 * a row of rank pips for the seven levels, the title, a blurb, par / gold
 * time, secrets and the mini-boss waiting at the end. L / R switch the
 * mode (Hell Breach, Time Trial, Cute-Only, Brutal-Only - the ones that
 * are unlocked), SELECT lets the autopilot play a demo. Coming back from a
 * clear that opened a new level moves the cursor to it.
 */
#include "levels_int.h"

#define PV_BB 20

static struct {
    uint8_t  mode, sel, launched, loaded, flash_lv;
    uint8_t  unlocked_mask;
    uint16_t t, pv_t, flash_t, msg_t;
    int16_t  slide;
    const char *msg;
} ls;

static int mode_open(int m)
{
    switch (m) {
    case LVM_TIME_TRIAL: return unlock_has(LVU_TIME_TRIAL);
    case LVM_CUTE_ONLY: case LVM_BRUTAL_ONLY: return unlock_has(LVU_CHALLENGES);
    default: return 1;
    }
}

static int playable(int level)
{
    if (!lv_unlocked(level)) return 0;
    if (ls.mode == LVM_TIME_TRIAL && lv_rank(level) == RANK_NONE) return 0;
    return 1;
}

static uint8_t unlocked_mask(void)
{
    int i, m = 0;
    for (i = 0; i < LV_COUNT; i++) if (lv_unlocked(i)) m |= 1 << i;
    return (uint8_t)m;
}

static void load_preview(void)
{
    map_load(lv_info[ls.sel].map);
    ls.loaded = ls.sel;
    ls.pv_t = 0;
}

static void ls_enter(int arg)
{
    int focus = (arg >> 4) & 15, i;
    memset(&ls, 0, sizeof(ls));
    ls.mode = (uint8_t)((arg & 15) < LVM_COUNT ? (arg & 15) : LVM_NORMAL);
    if (!mode_open(ls.mode)) ls.mode = LVM_NORMAL;
    lv_check_monster();
    if (focus > 0 && focus <= LV_COUNT) ls.sel = (uint8_t)(focus - 1);
    else {
        /* the first new level that is open but not cleared, else the last open one */
        ls.sel = 0;
        for (i = 0; i < LV_COUNT; i++) if (lv_unlocked(i)) ls.sel = (uint8_t)i;
        for (i = LV_FIRST_NEW; i < LV_COUNT; i++)
            if (lv_unlocked(i) && lv_rank(i) == RANK_NONE) { ls.sel = (uint8_t)i; break; }
    }
    ls.unlocked_mask = unlocked_mask();
    load_preview();
    audio_music(SONG_HELL_BREACH);
}

static void say(const char *m)
{
    ls.msg = m;
    ls.msg_t = 70;
}

static void move_sel(int dir)
{
    ls.sel = (uint8_t)((ls.sel + dir + LV_COUNT) % LV_COUNT);
    ls.slide = (int16_t)(dir * 24);
    load_preview();
    audio_sfx(SFX_CURSOR);
}

static void launch(int demo)
{
    if (!lv_unlocked(ls.sel)) { say("Locked! Clear the one before."); audio_sfx(SFX_ERROR); return; }
    if (!demo && !playable(ls.sel)) { say("Clear it once to race the clock!"); audio_sfx(SFX_ERROR); return; }
    audio_sfx(SFX_SELECT);
    ls.launched = 1;
    ls.unlocked_mask = unlocked_mask();
    scene_push(&SCENE_BREACH, LV_ARG(ls.sel, demo ? LVM_NORMAL : ls.mode) | (demo ? LVF_DEMO : 0));
}

static void ls_update(void)
{
    ls.t++;
    ls.pv_t++;
    if (ls.msg_t) ls.msg_t--;
    if (ls.flash_t) ls.flash_t--;
    if (ls.slide) ls.slide = (int16_t)(ls.slide * 3 / 4);
    if (ls.launched) {
        /* back from a run: a fresh preview, and point at anything new */
        uint8_t now = unlocked_mask(), fresh = (uint8_t)(now & ~ls.unlocked_mask);
        int i;
        ls.launched = 0;
        for (i = 0; i < LV_COUNT; i++)
            if (fresh & (1 << i)) {
                ls.sel = (uint8_t)i;
                ls.flash_lv = (uint8_t)i;
                ls.flash_t = 90;
                say("NEW LEVEL UNLOCKED!");
                audio_sfx(SFX_SPARKLE);
                break;
            }
        ls.unlocked_mask = now;
        load_preview();
        audio_music(SONG_HELL_BREACH);
        input_eat();
        return;
    }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); scene_pop(); return; }
    if (btn_repeat(BTN_LEFT)) move_sel(-1);
    if (btn_repeat(BTN_RIGHT)) move_sel(1);
    if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        int dir = btn_pressed(BTN_L) ? -1 : 1, m = ls.mode, k;
        for (k = 0; k < LVM_COUNT; k++) {
            m = (m + dir + LVM_COUNT) % LVM_COUNT;
            if (mode_open(m)) break;
        }
        if (m != ls.mode) { ls.mode = (uint8_t)m; audio_sfx(SFX_BLIP); }
        else { say("More modes unlock as you clear levels."); audio_sfx(SFX_ERROR); }
    }
    if (btn_pressed(BTN_A | BTN_START)) launch(0);
    else if (btn_pressed(BTN_SELECT)) launch(1);
}

/* ---- the live preview -------------------------------------------------------- */
static int thing_image(int type, const DoomoImage **img, int *scale, int *flags)
{
    int spr = 0;
    *flags = BB_SHADOW;
    if (IS_DEMON(type)) {
        const DemonInfo *di = DEMON(type);
        *img = sprite_image(di->spr, ((ls.pv_t / 12) & 1) ? DFRAME_WALK1 : DFRAME_WALK0);
        *scale = di->scale;
        if (sprite_flags(di->spr) & SPRF_TRANSLUCENT) *flags |= BB_TRANSLUCENT;
        return *img != NULL;
    }
    *scale = 380;
    switch (type) {
    case THING_PROP_TREE: spr = SPR_TREE; *scale = 640; break;
    case THING_PROP_LAMP: spr = SPR_LAMP; *scale = 520; *flags |= BB_FULLBRIGHT; break;
    case THING_PROP_BENCH: spr = SPR_BENCH; break;
    case THING_PROP_FOUNTAIN: spr = SPR_FOUNTAIN; *scale = 420; break;
    case THING_PROP_BARREL: spr = SPR_BARREL; break;
    case THING_PROP_SKULLS: spr = SPR_SKULL_PILE; *scale = 360; break;
    case THING_PROP_TORCH: spr = SPR_TORCH; *flags |= BB_FULLBRIGHT; break;
    case THING_PROP_SIGN: spr = SPR_SIGN; break;
    case THING_PROP_FLOWERS: spr = SPR_FLOWER_BED; break;
    case THING_PROP_MAILBOX: spr = SPR_MAILBOX; break;
    case THING_VAPE_PORTAL: spr = SPR_VAPE_PORTAL; *scale = 430; *flags |= BB_FULLBRIGHT; break;
    case THING_HEALTH_SMALL: spr = SPR_HEALTH_SMALL; *scale = 300; break;
    case THING_HEALTH_BIG: spr = SPR_HEALTH_BIG; *scale = 300; break;
    case THING_ARMOR: spr = SPR_ARMOR; *scale = 300; break;
    case THING_SOUL_ORB: spr = SPR_SOUL_ORB; *scale = 300; *flags |= BB_FULLBRIGHT; break;
    case THING_CANDY: spr = SPR_CANDY; *scale = 300; break;
    case THING_KEY_RED: spr = SPR_KEY_RED; *scale = 300; *flags |= BB_FULLBRIGHT; break;
    case THING_KEY_BLUE: spr = SPR_KEY_BLUE; *scale = 300; *flags |= BB_FULLBRIGHT; break;
    default: return 0;
    }
    *img = sprite_image(spr, 0);
    return *img != NULL;
}

static void preview(int x, int y, int w, int h)
{
    const LevelInfo *L = &lv_info[ls.sel];
    const DoomoMapDef *d = doomo_map.def;
    DoomoBillboard bb[PV_BB];
    DoomoView3D v;
    int i, n = 0, cx = L->pv_x, cy = L->pv_y;
    memset(&v, 0, sizeof(v));
    v.map = &doomo_map;
    v.vx = (int16_t)x; v.vy = (int16_t)y; v.vw = (int16_t)w; v.vh = (int16_t)h;
    v.x = INT2FIX(cx * CELLU + CELLU / 2);
    v.y = INT2FIX(cy * CELLU + CELLU / 2);
    v.z = INT2FIX(map_floor_z(&doomo_map, cx, cy) + L->pv_z * 8);
    v.angle = (int16_t)((ls.pv_t * 5 + ls.sel * 700) & DOOMO_ANG_MASK);
    v.pitch = (int16_t)(ui_wave(ls.pv_t, 240, 14) - 6);
    v.time = (uint16_t)ls.pv_t;
    v.camera = CAM_FIRST_PERSON;
    v.player_sprite = -1;
    if (d && d->things)
        for (i = 0; i < d->thing_count && n < PV_BB; i++) {
            const DoomoThing *th = &d->things[i];
            int dx = th->x - cx, dy = th->y - cy, scale, flags;
            const DoomoImage *img;
            if (dx * dx + dy * dy > 81 || (th->flags & THF_DORMANT)) continue;
            if (!thing_image(th->type, &img, &scale, &flags)) continue;
            bb[n].x = INT2FIX(th->x * CELLU + CELLU / 2);
            bb[n].y = INT2FIX(th->y * CELLU + CELLU / 2);
            bb[n].z = INT2FIX(map_floor_z(&doomo_map, th->x, th->y) +
                              (IS_DEMON(th->type) && (sprite_flags(DEMON(th->type)->spr) & SPRF_FLOATS) ? 24 : 0));
            bb[n].img = img;
            bb[n].scale = (uint16_t)scale;
            bb[n].flags = (uint8_t)flags;
            bb[n].light = 0;
            n++;
        }
    v.sprites = bb;
    v.sprite_count = n;
    plat_view3d(&v);
}

/* ---- drawing ------------------------------------------------------------------ */
static int theme_style(void)
{
    if (ls.mode == LVM_CUTE_ONLY) return WIN_CUTE;
    if (ls.mode == LVM_BRUTAL_ONLY) return WIN_HELL;
    return lv_info[ls.sel].theme;
}

static void draw_pips(int cy, int big)
{
    int sw = plat_screen_w(), gap = big ? 30 : 22, i, x0 = sw / 2 - gap * (LV_COUNT - 1) / 2;
    char nb[4];
    for (i = 0; i < LV_COUNT; i++) {
        int x = x0 + i * gap, sel = i == ls.sel, r = (big ? 8 : 6) + sel * 2, rank = lv_rank(i);
        int y = cy - (sel ? (ui_wave(ls.t, 24, 2) + 2) : 0);
        uint8_t c = !lv_unlocked(i) ? PAL_GRAY_D : rank ? lv_rank_color(rank) : PAL_UI_CREAM;
        if (i < LV_COUNT - 1) plat_rect(x + r, cy, gap - 2 * r + 1, 2, PAL_GRAY_D);
        ui_fill_circle(x + 1, y + 2, r, PAL_BLACK);
        ui_fill_circle(x, y, r + 1, sel ? PAL_WHITE : ui_darker(c));
        ui_fill_circle(x, y, r, c);
        if (!lv_unlocked(i)) {
            ui_text(x - 2, y - 3, STR_ICON_X, PAL_GRAY);
        } else if (rank) {
            ui_text_outline(x - 2, y - 3, lv_rank_names[rank], PAL_WHITE, ui_darker(ui_darker(c)));
        } else {
            doomo_snprintf(nb, sizeof(nb), "%d", i + 1);
            ui_text(x - 2, y - 3, nb, PAL_UI_PINK_D);
        }
        if (i == ls.flash_lv && ls.flash_t) ui_sparkle(x + r, y - r, (ls.t / 2) & 15, PAL_UI_GOLD);
        if (lv_gold(i)) ui_text(x + r - 3, y + r - 6, STR_ICON_STAR, PAL_UI_GOLD);
    }
}

static void ls_draw(void)
{
    const LevelInfo *L = &lv_info[ls.sel];
    int sw = plat_screen_w(), sh = plat_screen_h(), big = lvd_big(), style = theme_style();
    int px = big ? 12 : 10, py = big ? 22 : 19, pw = sw - px * 2, ph = big ? 108 : 54;
    int unl = lv_unlocked(ls.sel), ty, k;
    char buf[64], tb[12];
    ui_backdrop(style, ls.t);
    if (plat_draw_skipped()) return;
    ui_title(sw / 2, 3, ls.mode == LVM_NORMAL ? "HELL BREACH LEVELS" : lv_mode_names[ls.mode], style);
    if (mode_open(LVM_TIME_TRIAL)) {
        ui_text_outline(4, 5, "L", ui_style(style)->accent, PAL_BLACK);
        ui_text_outline(sw - 10, 5, "R", ui_style(style)->accent, PAL_BLACK);
    }
    /* the preview window */
    ui_round_rect(px - 3, py - 3, pw + 6, ph + 6, 5, ui_style(style)->outline);
    ui_round_rect(px - 2, py - 2, pw + 4, ph + 4, 4, ui_style(style)->fill_hi);
    preview(px + ls.slide, py, pw, ph);
    if (ls.slide) plat_rect(ls.slide > 0 ? px : px + pw + ls.slide, py, ls.slide > 0 ? ls.slide : -ls.slide, ph,
                            ui_style(style)->fill_hi);
    if (!unl) {
        plat_shade_rect(px, py, pw, ph, 9);
        lvd_padlock(sw / 2, py + ph / 2 - (big ? 8 : 6), big ? 2 : 1, ls.t);
        doomo_snprintf(buf, sizeof(buf), "Clear %s first", lv_info[DOOMO_MAX(lv_prereq_of(ls.sel), 0)].title);
        ui_text_outline(sw / 2 - ui_text_width(buf) / 2, py + ph / 2 + (big ? 16 : 10), buf, PAL_WHITE, PAL_BLACK);
    } else if (ls.mode == LVM_TIME_TRIAL && lv_rank(ls.sel) == RANK_NONE) {
        plat_shade_rect(px, py + ph - 14, pw, 14, 6);
        ui_text_center(sw / 2, py + ph - 11, "Clear it once to race the clock", PAL_WHITE);
    }
    /* tag pill + rank stamp on the preview */
    {
        int tw = ui_text_width(L->tag) + 10;
        ui_round_rect(px + 4, py + 4, tw, 11, 5, L->accent);
        ui_text(px + 9, py + 6, L->tag, PAL_WHITE);
        if (unl && lv_rank(ls.sel) != RANK_NONE)
            lvd_rank_stamp(px + pw - (big ? 18 : 14), py + (big ? 18 : 14), lv_rank(ls.sel), 40 + ls.t, 0);
        else if (unl) {
            /* never cleared: a bouncy NEW! tag */
            int nw = ui_text_width("NEW!") + 10, nx = px + pw - nw - 4, ny = py + 4 + ui_wave(ls.t, 20, 1);
            ui_round_rect(nx + 1, ny + 1, nw, 11, 5, PAL_BLACK);
            ui_round_rect(nx, ny, nw, 11, 5, (ls.t & 16) ? PAL_UI_GOLD : PAL_WHITE);
            ui_text(nx + 5, ny + 2, "NEW!", PAL_UI_RED_D);
        }
    }
    draw_pips(py + ph + (big ? 15 : 10), big);
    /* title + blurb + numbers */
    ty = py + ph + (big ? 30 : 20);
    if (big && ui_text_big_width(L->title, 2) < sw - 20)
        ui_text_big(sw / 2 - ui_text_big_width(L->title, 2) / 2, ty, L->title, 2, PAL_WHITE, ui_style(style)->outline);
    else
        ui_text_outline(sw / 2 - ui_text_width(L->title) / 2, ty, L->title, PAL_WHITE, ui_style(style)->outline);
    ty += big ? 20 : 11;
    ui_text_wrap(8, ty, sw - 16, L->blurb, ui_style(style)->text, 1);
    ty += 21;
    if (ls.mode == LVM_TIME_TRIAL) {
        lvd_time(tb, sizeof(tb), L->dev);
        doomo_snprintf(buf, sizeof(buf), STR_ICON_BOLT " Gold time %s   %s", tb, lv_gold(ls.sel) ? STR_ICON_STAR " GOLD!" : "no medal yet");
    } else {
        lvd_time(tb, sizeof(tb), L->par);
        doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR " Par %s   Best %s   %s", tb, lv_rank_names[lv_rank(ls.sel)],
                       ls.mode == LVM_CUTE_ONLY ? STR_ICON_HEART "cute only" :
                       ls.mode == LVM_BRUTAL_ONLY ? STR_ICON_SKULL "brutal only" : "");
    }
    /* the numbers on a dark card in the theme's own outline colour */
    ui_round_rect(4, ty - 2, sw - 8, L->boss != LB_NONE ? 22 : 12, 4, ui_style(style)->outline);
    plat_rect(8, ty - 2, sw - 16, 1, ui_lighter(ui_style(style)->outline));
    ui_text_outline(8, ty, buf, PAL_UI_GOLD, PAL_BLACK);
    if (L->boss != LB_NONE) {
        const LevelBoss *b = &lv_boss[L->boss];
        doomo_snprintf(buf, sizeof(buf), STR_ICON_SKULL " %s", unl ? b->name : "???");
        ui_text_outline(8, ty + 10, buf, PAL_RAMP(RAMP_LAVA, 7), PAL_BLACK);
        if (big && unl) lvd_boss_figure(L->boss, sw - 34, sh - 18, ls.t, 1, 1);
    }
    if (ls.msg_t) {
        int w = ui_text_width(ls.msg) + 14, x = sw / 2 - w / 2, y = py + ph / 2 - 6;
        ui_round_rect(x, y + ui_wave(ls.msg_t, 20, 1), w, 13, 6, PAL_UI_GOLD);
        ui_text(x + 7, y + 3 + ui_wave(ls.msg_t, 20, 1), ls.msg, PAL_UI_RED_D);
    }
    for (k = 0; k < 2; k++)
        ui_text_outline(k ? px + pw - 10 + ui_wave(ls.t, 20, 2) : px + 3 - ui_wave(ls.t, 20, 2), py + ph / 2 - 4,
                        k ? STR_ICON_ARROW_R : STR_ICON_ARROW_L, PAL_WHITE, PAL_BLACK);
    ui_hint(big ? STR_ICON_ARROW_L STR_ICON_ARROW_R " Level  " STR_ICON_A " Play  SELECT Demo  " STR_ICON_B " Back"
                : STR_ICON_A " Play  SEL Demo  " STR_ICON_B " Back", style);
}

const DoomoScene SCENE_LEVEL_SELECT = { "LEVEL_SELECT", ls_enter, 0, ls_update, ls_draw, 0 };
