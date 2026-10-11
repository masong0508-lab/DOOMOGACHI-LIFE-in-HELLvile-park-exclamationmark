/* fp_apartments.c - SCENE_APARTMENTS (owner: freeplay).
 *
 * arg 0 = Hillvile Towers, 1 = Hellvile Pits, arg >= 2 = the home of
 * resident (arg - 2). 500 homes per district: 50 floors x 10 rooms
 * (floor = slot / 10 + 1, room = slot % 10 + 1, the creator's numbering).
 *
 * Every window shows who lives there doing what they're doing: bobbing at
 * home, tucked in with floating Zzz when asleep, an "out" note with the
 * place when away, curtains and a candle for the departed, a flickering
 * red glow for the possessed, a see-through ghost, thought bubbles for
 * problems. Empty rooms invite you to create a citizen right there.
 *
 *   D-pad  room / floor     L / R   5 floors     START  jump to a floor
 *   SELECT next room with a problem             A open / create   B back
 */
#include "fp_int.h"

#define FLOORS 50
#define ROOMS 10

static struct {
    int district, floor, room;        /* floor 1..50, room 1..10 */
    int scroll;                       /* pixel scroll of the facade (x16) */
    int t, wait, jump_open;
    UiList jump;
    uint8_t seen[DOOMO_DISTRICT_SIZE * 2 / 8];
    int seen_n;
} A;

typedef struct { int wx, ww, wh, fh, top, bottom, visible; } ALayout;

static void alayout(ALayout *L)
{
    int sw = fp_sw(), sh = fp_sh(), big = fp_big();
    L->ww = big ? 26 : 19;
    L->wx = (sw - ROOMS * (L->ww + 3) + 3) / 2;
    L->wh = big ? 30 : 26;
    L->fh = L->wh + (big ? 12 : 9);
    L->top = big ? 30 : 24;
    L->bottom = sh - (big ? 44 : 34);
    L->visible = (L->bottom - L->top) / L->fh + 2;
}

static int slot_of(int floor, int room)
{
    return A.district * DOOMO_DISTRICT_SIZE + (floor - 1) * ROOMS + (room - 1);
}

static void ap_enter(int arg)
{
    int keep_seen = A.seen_n;
    uint8_t seen[sizeof(A.seen)];
    memcpy(seen, A.seen, sizeof(seen));
    memset(&A, 0, sizeof(A));
    memcpy(A.seen, seen, sizeof(seen));
    A.seen_n = keep_seen;
    if (arg >= 2 && arg - 2 < DOOMO_MAX_RESIDENTS) {
        int id = arg - 2;
        A.district = RES_DISTRICT(id);
        A.floor = fp_floor(id);
        A.room = fp_room(id);
    } else {
        int i;
        A.district = arg ? 1 : 0;
        A.floor = 1;
        A.room = 1;
        /* start at the first occupied home */
        for (i = 0; i < DOOMO_DISTRICT_SIZE; i++)
            if (doomo_town.res[A.district * DOOMO_DISTRICT_SIZE + i].status != RES_EMPTY) {
                A.floor = i / ROOMS + 1;
                A.room = i % ROOMS + 1;
                break;
            }
    }
    {
        ALayout L;
        alayout(&L);
        A.scroll = A.floor * L.fh * 16;
    }
    audio_music(A.district ? SONG_HELLVILE : fp_island_song());
}

static void ap_leave(void) { audio_music(fp_island_song()); }

static const char *jump_item(int index, char *buf)
{
    int fl = FLOORS - index, k, n = 0, pr = 0;
    for (k = 1; k <= ROOMS; k++) {
        int id = slot_of(fl, k);
        if (doomo_town.res[id].status != RES_EMPTY) n++;
        if (sim_problem_for(id) >= 0) pr++;
    }
    if (pr) doomo_snprintf(buf, 48, "Floor %2d  %2d home  " STR_ICON_HEART "%d", fl, n, pr);
    else doomo_snprintf(buf, 48, "Floor %2d  %2d home", fl, n);
    return buf;
}

static void next_problem(void)
{
    int k, start = (A.floor - 1) * ROOMS + (A.room - 1);
    for (k = 1; k <= DOOMO_DISTRICT_SIZE; k++) {
        int s = (start + k) % DOOMO_DISTRICT_SIZE;
        int id = A.district * DOOMO_DISTRICT_SIZE + s;
        if (sim_problem_for(id) >= 0) {
            A.floor = s / ROOMS + 1;
            A.room = s % ROOMS + 1;
            audio_sfx(SFX_BLIP);
            return;
        }
    }
    audio_sfx(SFX_ERROR);
    ui_toast("Nobody here needs help right now!");
}

static void open_room(void)
{
    int id = slot_of(A.floor, A.room);
    const DoomoResident *r = &doomo_town.res[id];
    if (r->status == RES_EMPTY) {
        audio_sfx(SFX_SELECT);
        A.wait = FP_WAIT_CREATOR;
        scene_push(&SCENE_CREATOR, id);
        return;
    }
    if (!(A.seen[id >> 3] & (1 << (id & 7)))) {
        A.seen[id >> 3] |= (uint8_t)(1 << (id & 7));
        if (++A.seen_n >= 25) fp_achieve(UL_FP_HOUSEWARMING);
    }
    audio_sfx(SFX_DOOR);
    fp_open_resident(id);
}

static void ap_update(void)
{
    ALayout L;
    int target;
    fp_live();
    A.t++;
    alayout(&L);
    if (A.wait) {
        A.wait = 0;
        if (creator_last_result() >= 0) {
            fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_CONFETTI, 20);
            audio_sfx(SFX_HAPPY);
            if (fp_is_freeplay()) fp_achieve(UL_FP_FIRST_CITIZEN);
            fp_check_island_achievements();
        }
    }
    if (A.jump_open) {
        int r = ui_list_update(&A.jump);
        if (r == UI_SELECT) {
            A.floor = FLOORS - A.jump.sel;
            A.jump_open = 0;
        } else if (r == UI_CANCEL) {
            A.jump_open = 0;
        }
        return;
    }
    if (btn_repeat(BTN_LEFT)) { A.room = A.room > 1 ? A.room - 1 : ROOMS; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_RIGHT)) { A.room = A.room < ROOMS ? A.room + 1 : 1; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_UP) && A.floor < FLOORS) { A.floor++; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN) && A.floor > 1) { A.floor--; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_R)) { A.floor = DOOMO_MIN(FLOORS, A.floor + 5); audio_sfx(SFX_BLIP); }
    if (btn_repeat(BTN_L)) { A.floor = DOOMO_MAX(1, A.floor - 5); audio_sfx(SFX_BLIP); }
    if (btn_pressed(BTN_SELECT)) next_problem();
    if (btn_pressed(BTN_START)) {
        memset(&A.jump, 0, sizeof(A.jump));
        A.jump.item_fn = jump_item;
        A.jump.count = FLOORS;
        A.jump.visible = fp_big() ? 12 : 7;
        A.jump.sel = FLOORS - A.floor;
        A.jump.top = DOOMO_MAX(0, A.jump.sel - A.jump.visible / 2);
        A.jump_open = 1;
        audio_sfx(SFX_SELECT);
        return;
    }
    if (btn_pressed(BTN_A)) { open_room(); return; }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else fp_goto_island(FP_ISLAND_ARG_RETURN);
        return;
    }
    /* smooth "elevator" scroll toward the cursor floor */
    target = A.floor * L.fh * 16;
    A.scroll += (target - A.scroll) / 4;
    if (A.scroll < target && target - A.scroll < 16) A.scroll = target;
    if (A.scroll > target && A.scroll - target < 16) A.scroll = target;
}

/* ====================================================================== */
/* drawing                                                                  */
static void draw_window(const ALayout *L, int x, int y, int id, int sel)
{
    const DoomoResident *r = &doomo_town.res[id];
    int hell = A.district, w = L->ww, h = L->wh, t = A.t, night = town_is_night();
    int st = r->status, act = r->activity, home = st != RES_EMPTY && st != RES_DEAD &&
        (act == ACT_HOME || act == ACT_SLEEPING || st == RES_SOUL_LOST);
    uint8_t frame = hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_ROSE, 0);
    uint8_t wall0, wall1;
    /* frame + sill */
    plat_rect(x - 1, y - 1, w + 2, h + 2, frame);
    plat_rect(x - 2, y + h, w + 4, 2, hell ? PAL_RAMP(RAMP_STONE, 3) : PAL_RAMP(RAMP_PEACH, 7));
    plat_rect(x - 2, y + h + 2, w + 4, 1, hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_PEACH, 4));
    if (st == RES_EMPTY) {
        /* dark empty room with a "for rent" heart sign */
        plat_rect(x, y, w, h, hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_SKY, 2));
        plat_checker(x, y, w, h, hell ? PAL_RAMP(RAMP_VAPE, 1) : PAL_RAMP(RAMP_SKY, 3));
        plat_rect(x + w / 2 - 4, y + h / 2 - 3, 9, 7, PAL_UI_CREAM);
        plat_text(x + w / 2 - 2, y + h / 2 - 3, "+", sel ? PAL_UI_RED : PAL_RAMP(RAMP_WOOD, 2));
        return;
    }
    /* wallpaper in the citizen's favourite colour */
    wall0 = PAL_PAINT(r->like_color % 32, 2);
    wall1 = PAL_PAINT(r->like_color % 32, 1);
    if (hell) { wall0 = ui_darker(wall1); wall1 = ui_darker(wall0); }
    if (st == RES_DEAD) {
        plat_rect(x, y, w, h, PAL_RAMP(RAMP_STONE, 1));
        /* drawn curtains + a candle */
        plat_rect(x, y, w / 2, h, PAL_PAINT(PAINT_GRAPE, 0));
        plat_rect(x + w / 2, y, w - w / 2, h, PAL_PAINT(PAINT_GRAPE, 0));
        plat_rect(x + w / 2 - 1, y, 1, h, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(x + 2, y + 1, 1, h - 2, PAL_PAINT(PAINT_GRAPE, 1));
        plat_rect(x + w / 2 - 1, y + h - 7, 2, 5, PAL_RAMP(RAMP_BONE, 7));
        plat_rect(x + w / 2 - 1, y + h - 9 + ui_wave(t, 10, 1), 2, 2, PAL_RAMP(RAMP_LAVA, 6));
        return;
    }
    {
        int lit = !night || (home && act != ACT_SLEEPING);
        int dark = night && !lit;
        plat_rect(x, y, w, h, dark ? ui_darker(ui_darker(wall1)) : wall0);
        plat_rect(x, y + h - 6, w, 6, dark ? PAL_RAMP(RAMP_STONE, 1) : wall1);
        plat_checker(x, y + h - 7, w, 1, dark ? PAL_RAMP(RAMP_STONE, 1) : wall1);
        if (!dark) {
            plat_rect(x + 1, y + 1, w - 2, 1, PAL_WHITE);
            /* tiny polka hearts on the wallpaper */
            plat_rect(x + 3, y + 5, 1, 1, wall1);
            plat_rect(x + w - 4, y + 9, 1, 1, wall1);
        }
        if (st == RES_POSSESSED && (t / 4 + id) % 6 < 3) plat_shade_rect(x, y, w, h, 9);
    }
    if (home) {
        int cx = x + (w - BODYSPR_W) / 2, cy = y + h - BODYSPR_H - 1;
        int bob = ui_wave(t + id * 11, 50, 1) > 0, shake = 0, flip = 0;
        if (st == RES_POSSESSED) shake = ((t + id) & 2) ? 1 : -1;
        if (act != ACT_SLEEPING && ((t / 90 + id) % 4) == 1) flip = IMG_FLIPX;
        if (act == ACT_SLEEPING) {
            fp_chibi(id, cx, cy + 4, 0, 0, 0);
            /* tucked in: blanket + pillow */
            plat_rect(x + 1, y + h - 9, w - 2, 8, PAL_PAINT(PAINT_SKY, 1));
            plat_rect(x + 1, y + h - 9, w - 2, 1, PAL_PAINT(PAINT_SKY, 2));
            plat_rect(x + 3, y + h - 6, w - 6, 1, PAL_PAINT(PAINT_SKY, 0));
            {
                int ph = (t + id * 7) % 60;
                plat_text(x + w - 8 + ph / 20, y + 4 - ph / 12, "z", PAL_WHITE);
            }
        } else {
            fp_chibi(id, cx + shake, cy - bob, 0, 0, flip);
            if (st == RES_GHOST) plat_checker(cx, cy - bob, BODYSPR_W, BODYSPR_H, PAL_RAMP(RAMP_MINT, 6));
        }
    } else {
        /* out and about: a little note on the door */
        char g[2] = { (char)fp_activity_icon(act), 0 };
        plat_rect(x + w / 2 - 5, y + h / 2 - 5, 11, 9, PAL_UI_CREAM);
        plat_rect(x + w / 2 - 5, y + h / 2 + 4, 11, 1, PAL_RAMP(RAMP_PEACH, 5));
        plat_text(x + w / 2 - 2, y + h / 2 - 4, g, PAL_RAMP(RAMP_WOOD, 2));
    }
    /* window glass glint + mullion */
    plat_rect(x + w - 3, y + 2, 1, 3, PAL_WHITE);
    if (sim_problem_for(id) >= 0) fp_bubble(x + w - 4, y + 2, ICON_HEART, PAL_UI_RED, t + id * 9);
}

static void draw_facade(const ALayout *L)
{
    int sw = fp_sw(), hell = A.district, t = A.t, i;
    /* sky behind the tower */
    plat_rect(0, 0, sw, fp_sh(), hell ? PAL_RAMP(RAMP_VAPE, 1) : (town_is_night() ? PAL_RAMP(RAMP_SKY, 1) : PAL_RAMP(RAMP_SKY, 6)));
    if (hell) {
        for (i = 0; i < 3; i++) {
            int bx = (t * (1 + i) / 2 + i * 101) % (sw + 40) - 20, by = 40 + i * 30 + ui_wave(t * 2 + i * 50, 40, 4);
            plat_rect(bx - 3, by, 7, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(bx - 1, by + 1, 3, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(bx - 4, by - ((t / 4) & 1), 2, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(bx + 3, by - ((t / 4) & 1), 2, 1, PAL_RAMP(RAMP_STONE, 0));
        }
    } else {
        for (i = 0; i < 3; i++) {
            int cx = (i * 97 + t / (3 + i)) % (sw + 60) - 30, cy = 30 + i * 40;
            ui_fill_ellipse(cx, cy, 14, 4, PAL_WHITE);
            ui_fill_ellipse(cx - 5, cy - 3, 7, 4, PAL_WHITE);
        }
    }
    /* the building body */
    {
        int bx = L->wx - 8, bw = ROOMS * (L->ww + 3) + 13;
        uint8_t b0 = hell ? PAL_RAMP(RAMP_VAPE, 3) : PAL_PAINT(PAINT_BUBBLEGUM, 2);
        uint8_t b1 = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_PASTEL_PINK, 6);
        plat_rect(bx - 1, 0, bw + 2, fp_sh(), hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_PAINT(PAINT_ROSE, 0));
        plat_rect(bx, 0, bw, fp_sh(), b0);
        plat_rect(bx, 0, 3, fp_sh(), ui_lighter(b0));
        plat_rect(bx + bw - 4, 0, 4, fp_sh(), b1);
    }
}

static void ap_draw(void)
{
    ALayout L;
    int sw = fp_sw(), sh = fp_sh(), hell = A.district, f, k, t = A.t;
    int style = hell ? WIN_DARK : WIN_CUTE;
    const UiStyle *st = ui_style(style);
    int cur_id = slot_of(A.floor, A.room);
    const DoomoResident *cr = &doomo_town.res[cur_id];
    char buf[64];
    alayout(&L);
    draw_facade(&L);
    /* floors: the cursor floor sits in the middle of the view */
    {
        int mid = (L.top + L.bottom) / 2 - L.fh / 2;
        int base = A.scroll / 16;
        for (f = 1; f <= FLOORS; f++) {
            int y = mid - (f * L.fh - base);
            if (y < L.top - L.fh || y > L.bottom) continue;
            /* ledge with the floor number */
            plat_rect(L.wx - 8, y + L.wh + 4, ROOMS * (L.ww + 3) + 13, 2,
                      hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_ROSE, 0));
            for (k = 1; k <= ROOMS; k++) {
                int x = L.wx + (k - 1) * (L.ww + 3), id = slot_of(f, k);
                int sel = f == A.floor && k == A.room;
                draw_window(&L, x, y + 2, id, sel);
                if (sel) {
                    int pulse = (t / 6) & 1;
                    ui_frame(x - 3, y - 1, L.ww + 6, L.wh + 6, pulse ? PAL_UI_GOLD : PAL_WHITE);
                    ui_frame(x - 2, y, L.ww + 4, L.wh + 4, PAL_UI_GOLD);
                    ui_cursor(x - 2 + L.ww / 2 - 3, y + L.wh + 6);
                }
            }
            doomo_snprintf(buf, sizeof(buf), "%d", f);
            ui_text_outline(L.wx - 7 - (f >= 10 ? 6 : 0) + 4, y + L.wh / 2 - 2, buf,
                            PAL_WHITE, hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_PAINT(PAINT_ROSE, 0));
            if (f == FLOORS) {
                /* the roof */
                int ry = y - 10;
                plat_rect(L.wx - 10, ry, ROOMS * (L.ww + 3) + 17, 8, hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_PAINT(PAINT_ROSE, 1));
                plat_rect(L.wx - 10, ry, ROOMS * (L.ww + 3) + 17, 2, hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_PAINT(PAINT_ROSE, 2));
                if (hell) ui_text_center(sw / 2, ry - 10, STR_ICON_SKULL " HELLVILE PITS " STR_ICON_SKULL, PAL_RAMP(RAMP_LAVA, 6));
                else ui_text_center(sw / 2, ry - 10, STR_ICON_HEART " HILLVILE TOWERS " STR_ICON_HEART, PAL_PAINT(PAINT_ROSE, 1));
            }
            if (f == 1) {
                int ly = y + L.wh + 6;
                plat_rect(L.wx - 8, ly, ROOMS * (L.ww + 3) + 13, sh - ly, hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_PEACH, 6));
                plat_rect(sw / 2 - 8, ly + 2, 16, 14, hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_WOOD, 3));
            }
        }
    }
    /* header */
    {
        int occ = town_count_district(hell, -1);
        doomo_snprintf(buf, sizeof(buf), "%s  %d/500", fp_home_name(hell), occ);
        ui_title(sw / 2, 3, buf, style);
    }
    /* info card */
    {
        int iy = sh - (fp_big() ? 42 : 32), ih = fp_big() ? 26 : 19, pi = sim_problem_for(cur_id);
        ui_window(4, iy, sw - 8, ih, style);
        doomo_snprintf(buf, sizeof(buf), "%d-%d", A.floor, A.room);
        ui_text(10, iy + 5, buf, st->accent);
        if (cr->status == RES_EMPTY) {
            ui_text(46, iy + 5, "Vacant! " STR_ICON_A " Create a citizen", st->text);
        } else {
            char nm[DOOMO_NAME_LEN + 1];
            fp_strcpy(nm, cr->name, sizeof(nm));
            ui_text(46, iy + 5, nm, st->text);
            if (pi >= 0) {
                char pb[64];
                sim_problem_text(pi, pb, sizeof(pb));
                fp_icon(46 + 72, iy + 5, ICON_CLOUD, PAL_UI_RED);
                fp_marquee(46 + 81, iy + 5, sw - 46 - 81 - 12, pb, PAL_UI_RED, t);
            } else if (fp_living_status(cr->status)) {
                fp_icon(46 + 72, iy + 5, fp_mood_icon(cr->mood), fp_mood_color(cr->mood));
                fp_marquee(46 + 81, iy + 5, sw - 46 - 81 - 12, fp_activity_name(cr->activity), st->text_dim, t);
            } else {
                ui_text(46 + 72, iy + 5, fp_status_name(cr->status), st->text_dim);
            }
            if (fp_big()) fp_marquee(10, iy + 14, sw - 24, fp_job(cur_id), st->text_dim, t);
        }
    }
    ui_hint(fp_big() ? STR_ICON_A " Visit  L/R 5 floors  START Floor  SEL Help!" : STR_ICON_A " Visit  START Floor  SEL Help", style);
    if (A.jump_open) {
        int w = fp_big() ? 170 : 150, h = A.jump.visible * UI_LIST_ROW_H + 12;
        int x = (sw - w) / 2, y = (sh - h) / 2;
        plat_shade_rect(0, 0, sw, sh, 10);
        ui_window(x, y, w, h, style);
        ui_list_draw(&A.jump, x + 6, y + 6, w - 12, style);
    }
    fp_fx_draw();
}

const DoomoScene SCENE_APARTMENTS = { "APARTMENTS", ap_enter, ap_leave, ap_update, ap_draw, 0 };
