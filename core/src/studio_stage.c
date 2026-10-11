/* studio_stage.c - PERFORM: the Music Studio's stage show (owner: studio).
 *
 * The poster (tab view) shows the bill; A raises the curtain and the show
 * goes full screen: velvet curtains, a back wall of LED hearts pulsing on
 * the beat, sweeping spotlights and a follow spot on the lead voice, the
 * whole choir (up to 25 chibis) on the chosen arrangement - risers,
 * semicircle, sections or pyramid - every assigned singer bopping and
 * opening their mouth on each syllable, section glows when a section has
 * the line, a conductor (Father Fuzz, or you) waving a baton in 4/4, a
 * crowd of citizens in the dark with glowsticks, karaoke lyrics with the
 * syllable highlight and an applause meter. Afterwards the singers are
 * happier (and a little closer to each other), and the studio stats move
 * the unlocks along. SELECT on the poster hands the song to the Concert
 * Hall stage when that module offers it (concert_stage_song, song.h).
 */
#include "studio_int.h"

#define CURTAIN_T 30            /* ticks of curtain before the music     */

static const uint8_t sec_paint[SECTION_COUNT] = { PAINT_ROSE, PAINT_PEACH, PAINT_MINT, PAINT_SKY };

int stage_fullscreen(void) { return studio.show != 0; }

void stage_leave(void)
{
    if (studio.show) song_stop();
    studio.show = 0;
}

static int in_choir(int id)
{
    int i;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(SS, i) == id) return 1;
    return 0;
}

static void pick_crowd(void)
{
    DoomoRng r;
    int i, n = 0, tries;
    rng_seed(&r, studio.t * 2654435761u + SS->seed);
    for (i = 0; i < STUDIO_CROWD; i++) studio.crowd[i] = -1;
    /* friends of the singers first, then anybody */
    for (i = 0; i < SONG_SINGERS && n < STUDIO_CROWD / 2; i++) {
        uint16_t ids[4];
        int id = song_singer_id(SS, i), k, m;
        if (id < 0) continue;
        m = rel_list(id, ids, 4);
        for (k = 0; k < m && n < STUDIO_CROWD / 2; k++) {
            int j, dup = 0;
            if (!st_alive(ids[k]) || in_choir(ids[k]) || ids[k] == DOOMO_PLAYER_ID) continue;
            for (j = 0; j < n; j++) if (studio.crowd[j] == ids[k]) dup = 1;
            if (!dup) studio.crowd[n++] = (int16_t)ids[k];
        }
    }
    for (tries = 0; tries < 400 && n < STUDIO_CROWD; tries++) {
        int id = rng_range(&r, DOOMO_MAX_RESIDENTS), j, dup = 0;
        if (!st_alive(id) || in_choir(id)) continue;
        for (j = 0; j < n; j++) if (studio.crowd[j] == id) dup = 1;
        if (!dup) studio.crowd[n++] = (int16_t)id;
    }
}

void stage_start(void)
{
    studio.show = 1;
    studio.show_t = 0;
    studio.applause = 15;
    studio.sung_lines = studio.fit_lines = 0;
    studio.last_line = 0xFF;
    studio.encore = 0;
    studio.happy_gain = 0;
    song_stop();
    pick_crowd();
    if (st_alive(ROSTER_ID_FATHER_FUZZ) && !in_choir(ROSTER_ID_FATHER_FUZZ))
        studio.conductor = ROSTER_ID_FATHER_FUZZ;
    else
        studio.conductor = DOOMO_PLAYER_ID;
    audio_sfx(SFX_BELL);
}

/* ---- results -------------------------------------------------------------- */
static void finish_show(void)
{
    int i, n = song_singer_count(SS), happy, prev = -1;
    song_stop();
    studio.show = 2;
    studio.show_t = 0;
    studio.applause = (uint8_t)DOOMO_MIN(100, studio.applause + n + 10 * studio.fit_lines /
                                                 DOOMO_MAX(studio.sung_lines, 1));
    studio.encore = studio.applause >= 80;
    happy = 6 + studio.applause / 8;
    studio.happy_gain = (uint8_t)happy;
    for (i = 0; i < SONG_SINGERS; i++) {
        int id = song_singer_id(SS, i);
        DoomoResident *r = id >= 0 ? town_res(id) : NULL;
        if (!r) continue;
        r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + happy);
        r->stat[STAT_SOCIAL] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SOCIAL] + 5);
        /* singing together brings neighbours on the risers closer */
        if (prev >= 0 && prev != id) rel_adjust(prev, id, 2);
        prev = id;
    }
    if (studio_stats.performed < 0xFFFF) studio_stats.performed++;
    if (n > studio_stats.best_choir) studio_stats.best_choir = (uint8_t)n;
    if (studio.encore && studio_stats.encores < 255) studio_stats.encores++;
    studio_check_unlocks();
    audio_sfx(studio.encore ? SFX_CONFETTI : SFX_HAPPY);
}

static int line_fits(int line)
{
    int pos = studio_line_pos(SS, line);
    if (pos < 0 || !SS->lyrics[line][0]) return 0;
    return song_syllables(SS->lyrics[line], NULL, 0) == song_lead_onsets(SS, SS->order[pos] & 3);
}

void tab_stage_update(void)
{
    if (!studio.show) {
        if (btn_pressed(BTN_A)) stage_start();
        if (studio_sel_tap) {
            if (concert_stage_song(SS)) audio_sfx(SFX_SELECT);
            else { st_msg("The Concert Hall is booked - sing here!"); audio_sfx(SFX_ERROR); }
        }
        return;
    }
    studio.show_t++;
    if (studio.show == 1) {
        int pl;
        if (studio.show_t == CURTAIN_T) song_play(SS, -1);
        if (btn_pressed(BTN_B)) { stage_leave(); st_msg("Show cancelled"); return; }
        pl = song_pos_line();
        if (song_playing() && pl >= 0 && pl != studio.last_line) {
            int n = 0;
            uint32_t m = song_line_mask(SS, pl);
            while (m) { m &= m - 1; n++; }
            studio.last_line = (uint8_t)pl;
            studio.sung_lines++;
            if (line_fits(pl)) studio.fit_lines++;
            studio.applause = (uint8_t)DOOMO_MIN(100, studio.applause + 5 + 3 * line_fits(pl) + n / 5);
        }
        if (!song_playing() && song_pos_line() < 0 && studio.last_line == 0xFF && studio.show_t > CURTAIN_T + 2)
            studio.last_line = 0xFE;      /* an instrumental: keep going */
        /* the end: a one-shot song stops by itself, a looping one after a pass */
        if (studio.show_t > CURTAIN_T + 2 &&
            (!song_playing() || song_pos_ticks() >= song_length_ticks(SS)))
            finish_show();
        return;
    }
    /* bows */
    if (studio.show_t > 20 && (btn_pressed(BTN_A) || btn_pressed(BTN_B))) {
        studio.show = 0;
        audio_sfx(SFX_SELECT);
    } else if (studio.show_t > 20 && btn_pressed(BTN_START)) {
        stage_start();
    }
}

/* ====================================================================== */
/* layout                                                                   */
typedef struct { int x, y, w, floor; } StageBox;

/* position (feet centre) of the k-th of n singers */
static int row_sp(const StageBox *b, int inrow)
{
    int sp = b->w >= 280 ? 26 : 19;
    if (inrow > 1) sp = DOOMO_MIN(sp, (b->w - 16) / (inrow - 1));
    return sp;
}

static void place(int layout, int k, int n, const StageBox *b, const uint8_t *secs, int *x, int *y)
{
    int row = 0, col = 0, inrow = n, i, sp;
    int curve = b->w >= 280 ? 22 : 15;
    switch (layout) {
    case CHOIR_SEMICIRCLE: {
        /* two arcs; the middle stands furthest back */
        int back = n > 12 ? (n + 1) / 2 : 0, front = n - back, cnt, idx, dx, half;
        if (k < back) { cnt = back; idx = k; row = 1; } else { cnt = front; idx = k - back; row = 0; }
        sp = row_sp(b, cnt);
        half = (cnt - 1) * sp / 2;
        dx = -half + idx * sp;
        *x = b->x + b->w / 2 + dx;
        *y = b->floor - row * 14 - (half ? curve - curve * dx * dx / (half * half) : curve) + curve / 3;
        return; }
    case CHOIR_PYRAMID: {
        int cap = 1, rows = 0, total_rows = 0, c = 1, m = n;
        for (i = k; i >= cap; cap += 2) { i -= cap; rows++; }
        row = rows; col = i;
        while (m > 0) { m -= c; c += 2; total_rows++; }
        inrow = DOOMO_MIN(2 * row + 1, n - row * row);
        sp = row_sp(b, inrow);
        *x = b->x + b->w / 2 - (inrow - 1) * sp / 2 + col * sp;
        *y = b->floor - (total_rows - 1 - row) * 13;
        return; }
    case CHOIR_SECTIONS: {
        int sec = secs[k], before = 0, gw = b->w / 4, cols = 3, csp;
        for (i = 0; i < k; i++) if (secs[i] == sec) before++;
        row = before / cols;
        col = before % cols;
        csp = DOOMO_MIN(gw / 3, 16);
        *x = b->x + sec * gw + gw / 2 - csp + col * csp + (row & 1) * (csp / 2);
        *y = b->floor - row * 12;
        return; }
    default: {
        /* risers: front row first, up to 3 rows of 9 / 8 / 8 */
        int r0 = n <= 9 ? n : 8, r1 = n <= 9 ? 0 : n <= 17 ? n - 8 : 8;
        if (k < r0) { row = 0; col = k; inrow = r0; }
        else if (k < r0 + r1) { row = 1; col = k - r0; inrow = r1; }
        else { row = 2; col = k - r0 - r1; inrow = n - r0 - r1; }
        sp = row_sp(b, inrow);
        *x = b->x + b->w / 2 - (inrow - 1) * sp / 2 + col * sp + (row & 1) * sp / 2 - sp / 4;
        *y = b->floor - row * 15;
        return; }
    }
}

/* ====================================================================== */
/* drawing                                                                  */
static void draw_curtains(int sw, int sh, int open)
{
    /* open 0 = closed .. 256 = fully open */
    int half = sw / 2, cw = half - (half - 22) * open / 256, i, x;
    for (x = 0; x < cw; x += 6) {
        int sh2 = (x / 6) & 1;
        plat_rect(x, 0, 6, sh, PAL_PAINT(PAINT_BLOOD, sh2 ? 0 : 1));
        plat_rect(x + 2, 0, 1, sh, PAL_PAINT(PAINT_CHERRY, 2 - sh2));
        plat_rect(sw - x - 6, 0, 6, sh, PAL_PAINT(PAINT_BLOOD, sh2 ? 0 : 1));
        plat_rect(sw - x - 4, 0, 1, sh, PAL_PAINT(PAINT_CHERRY, 2 - sh2));
    }
    /* gold tie-backs */
    if (open > 200) {
        ui_fill_circle(cw - 2, sh * 2 / 3, 3, PAL_PAINT(PAINT_HONEY, 1));
        ui_fill_circle(sw - cw + 1, sh * 2 / 3, 3, PAL_PAINT(PAINT_HONEY, 1));
    }
    /* the valance with scallops */
    plat_rect(0, 0, sw, 9, PAL_PAINT(PAINT_BLOOD, 0));
    for (i = 0; i < sw; i += 16) {
        ui_fill_ellipse(i + 8, 9, 8, 4, PAL_PAINT(PAINT_BLOOD, 0));
        plat_rect(i + 7, 11, 2, 2, PAL_PAINT(PAINT_HONEY, 1));
    }
    plat_rect(0, 7, sw, 1, PAL_PAINT(PAINT_HONEY, 1));
}

static void draw_wall(int sw, int top, int floor, int t, int beat)
{
    int y, x, i, h = floor - top;
    for (i = 0; i < 4; i++)
        plat_rect(0, top + h * i / 4, sw, h / 4 + 1, PAL_RAMP(RAMP_VAPE, 1 + i));
    /* LED hearts: a grid that ripples out from the centre on the beat */
    for (y = top + 8; y < floor - 10; y += 14)
        for (x = 10; x < sw - 6; x += 16) {
            int d = (x > sw / 2 ? x - sw / 2 : sw / 2 - x) / 16 + (y - top) / 14;
            int lit = ((t / 4 - d) & 7) == 0 || (beat && d < 3);
            ui_heart_flat(x, y, 3, lit ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_RAMP(RAMP_VAPE, 3));
        }
}

static void draw_floor(int sw, int floor, int bottom)
{
    int y;
    for (y = floor; y < bottom; y += 4)
        plat_rect(0, y, sw, 4, PAL_RAMP(RAMP_WOOD, ((y - floor) / 4) & 1 ? 4 : 5));
    plat_rect(0, floor, sw, 1, PAL_RAMP(RAMP_WOOD, 7));
}

static void spot(int x0, int x1, int top, int bottom, uint8_t c)
{
    int y;
    for (y = top; y < bottom; y += 2) {
        int w = 4 + (y - top) * 26 / DOOMO_MAX(bottom - top, 1);
        int x = x0 + (x1 - x0) * (y - top) / DOOMO_MAX(bottom - top, 1);
        plat_checker(x - w / 2, y, w, 2, c);
    }
}

static void draw_mouth(int x, int y, int open)
{
    /* x, y: top-left of the chibi; a round singing "o" under the eyes */
    if (open) {
        plat_rect(x + 7, y + 14, 3, 3, PAL_PAINT(PAINT_BLOOD, 0));
        plat_rect(x + 8, y + 16, 1, 1, PAL_PAINT(PAINT_ROSE, 2));
    } else {
        plat_rect(x + 7, y + 15, 3, 1, PAL_PAINT(PAINT_BLOOD, 0));
    }
}

static void karaoke(int cx, int y, int w)
{
    int line = song_pos_line(), syl = song_pos_syllable(), wide = st_wide();
    char buf[40];
    uint8_t st[SONG_LYRIC_LEN];
    const char *t;
    int n, i, len, x;
    plat_shade_rect(cx - w / 2, y, w, 22, 11);
    ui_frame(cx - w / 2, y, w, 22, PAL_PAINT(PAINT_GRAPE, 1));
    if (line < 0 || !SS->lyrics[line][0]) {
        ui_text_center(cx, y + 7, song_playing() ? STR_ICON_NOTE " ~ " STR_ICON_NOTE " ~ " STR_ICON_NOTE
                                                 : "", PAL_PAINT(PAINT_LAVENDER, 2));
        return;
    }
    t = SS->lyrics[line];
    n = DOOMO_MIN(song_syllables(t, st, SONG_LYRIC_LEN), SONG_LYRIC_LEN);
    len = (int)strlen(t);
    x = cx - len * 3;
    for (i = 0; i < len; i++) {
        int k = 0, bounce = 0;
        char ch[2] = { t[i], 0 };
        uint8_t c;
        while (k + 1 < n && st[k + 1] <= i) k++;
        if (n == 0 || i < st[0]) k = -1;
        if (k >= 0 && k < syl) c = PAL_PAINT(PAINT_LEMON, 2);
        else if (k == syl && syl >= 0) { c = PAL_PAINT(PAINT_BUBBLEGUM, 2); bounce = song_beat_age() < 4; }
        else c = PAL_WHITE;
        ui_text(x + i * 6, y + 7 - bounce, ch, c);
    }
    /* who has the line */
    studio_part_label(SS, line, buf, sizeof(buf));
    if (wide || song_singer_count(SS) > 1) {
        int bw = ui_text_width(buf) + 10;
        ui_round_rect(cx - bw / 2, y - 9, bw, 10, 3, PAL_PAINT(PAINT_GRAPE, 0));
        ui_text(cx - bw / 2 + 5, y - 8, buf, PAL_PAINT(PAINT_LAVENDER, 2));
    }
}

static void draw_show(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), t = studio.show_t, wide = st_wide();
    int beat = song_playing() && song_beat_age() < 3;
    int floor = wide ? sh * 64 / 100 : sh * 63 / 100, top = 12, i, n = 0;
    int open = studio.show == 1 ? DOOMO_MIN(256, t * 256 / CURTAIN_T) : 256;
    uint32_t singing = song_pos_singers();
    int lead = song_pos_lead(), lx = -1, ly = 0;
    uint8_t slots[SONG_SINGERS], secs[SONG_SINGERS];
    StageBox b;
    char buf[40];
    /* the choir in order (sections layout: grouped) */
    for (i = 0; i < SONG_SINGERS; i++)
        if (song_singer_id(SS, i) >= 0) { slots[n] = (uint8_t)i; secs[n] = (uint8_t)song_singer_section(SS, i); n++; }
    b.x = 24; b.w = sw - 48; b.floor = floor; b.y = top;
    draw_wall(sw, top, floor, t, beat);
    draw_floor(sw, floor, sh);
    /* risers behind the choir */
    if (SS->choir_layout == CHOIR_RISERS || SS->choir_layout == CHOIR_PYRAMID) {
        int rows = n > 17 || SS->choir_layout == CHOIR_PYRAMID ? 3 : n > 9 ? 2 : 1;
        for (i = rows - 1; i >= 1; i--) {
            int rw = (sw - 60) - i * 20, ry = floor - i * 15 + 1;
            plat_rect(sw / 2 - rw / 2, ry - 1, rw, 15 * i, PAL_RAMP(RAMP_WOOD, 2 + i));
            plat_rect(sw / 2 - rw / 2, ry - 1, rw, 1, PAL_RAMP(RAMP_WOOD, 6));
        }
    }
    /* sweeping spotlights */
    if (open > 128) {
        int a = ui_wave(t, 140, sw / 4), c2 = ui_wave(t + 50, 110, sw / 5);
        spot(sw / 4, sw / 2 + a, top, floor + 8, PAL_PAINT(PAINT_LEMON, 2));
        spot(sw * 3 / 4, sw / 2 - c2, top, floor + 8, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    }
    /* section glows (who has the line) */
    {
        int k;
        for (k = 0; k < n; k++) {
            int x, y;
            if (!((singing >> slots[k]) & 1)) continue;
            place(SS->choir_layout, k, n, &b, secs, &x, &y);
            ui_fill_ellipse(x, y, 8, 2, PAL_PAINT(sec_paint[secs[k]], 2));
        }
    }
    /* the choir, back rows first (smaller y first) */
    {
        int k, m, px[SONG_SINGERS], py[SONG_SINGERS];
        uint8_t ord[SONG_SINGERS];
        for (k = 0; k < n; k++) {
            place(SS->choir_layout, k, n, &b, secs, &px[k], &py[k]);
            for (m = k; m > 0 && py[ord[m - 1]] > py[k]; m--) ord[m] = ord[m - 1];
            ord[m] = (uint8_t)k;
        }
        for (m = 0; m < n; m++) {
            int kk = ord[m], x = px[kk], y = py[kk], id = song_singer_id(SS, slots[kk]);
            int sing = (singing >> slots[kk]) & 1, bop;
            bop = (beat ? 1 : 0) + (sing && song_beat_age() < 2 ? 1 : 0);
            st_chibi(id, x - BODYSPR_W / 2, y - BODYSPR_H - bop, 0, 0, 0, 0);
            if (sing) {
                draw_mouth(x - BODYSPR_W / 2, y - BODYSPR_H - bop, (song_beat_age() & 2) == 0);
                if (((t + kk * 5) % 24) < 12)
                    st_note_glyph(x + 4, y - BODYSPR_H - 8 - ((t + kk * 5) % 24) / 2,
                                  PAL_PAINT(sec_paint[secs[kk]], 2));
            }
            if (slots[kk] == lead) { lx = x; ly = y; }
        }
    }
    /* follow spot on the lead voice */
    if (lx >= 0) {
        spot(lx, lx, top, ly, PAL_WHITE);
        ui_star(lx, ly - BODYSPR_H - 6 + ui_wave(t, 12, 1), 3, PAL_PAINT(PAINT_LEMON, 2));
    }
    /* the conductor, back to us, baton in 4/4 */
    {
        int cx = sw / 2, cy = floor + (wide ? 34 : 19), age = song_beat_age();
        int beatn = (int)(song_pos_step() >= 0 ? (song_pos_step() / 4) & 3 : 0);
        static const int8_t bx[4] = { 0, -6, 6, 2 }, byy[4] = { 6, 2, 2, -6 };
        int px = cx + 6 + bx[beatn] * DOOMO_MAX(0, 4 - age) / 4;
        int py = cy - 18 + byy[beatn] * DOOMO_MAX(0, 4 - age) / 4;
        ui_fill_ellipse(cx, cy, 7, 2, PAL_RAMP(RAMP_WOOD, 2));
        st_chibi(studio.conductor, cx - BODYSPR_W / 2, cy - BODYSPR_H, 3, (age < 2) ? 1 : 0, 0, 0);
        plat_line(cx + 4, cy - 12, px, py, PAL_WHITE);
        ui_sparkle(px, py, (t >> 1) & 15, PAL_PAINT(PAINT_LEMON, 2));
    }
    /* the crowd in the dark */
    {
        int k, cw = sw / (STUDIO_CROWD / 2 + 1), cyb = sh - 4;
        for (k = 0; k < STUDIO_CROWD; k++) {
            int id = studio.crowd[k], row = k & 1, x = cw / 2 + (k / 2) * cw + row * cw / 2 - 8;
            int y = cyb - BODYSPR_H + 8 - row * 6 - ((beat && ((k + t / 8) & 1)) ? 2 : 0);
            if (y < floor + 8) continue;
            if (id >= 0) st_chibi(id, x, y, 3, 0, IMG_SILHOUETTE, PAL_PAINT(PAINT_GRAPE, 0));
            else ui_fill_circle(x + 8, y + 10, 7, PAL_PAINT(PAINT_GRAPE, 0));
            if ((k * 7 + t / 16) % 5 == 0 || studio.applause > 70) {
                int gx = x + 12, gy = y + 4 - ui_wave(t + k * 9, 30, 2);
                plat_line(gx, gy, gx + 2, gy - 6, PAL_PAINT(k & 1 ? PAINT_LIME : PAINT_BUBBLEGUM, 2));
            }
        }
        /* hearts float up from a happy crowd */
        for (k = 0; k < studio.applause / 20; k++) {
            int hx = (k * 47 + 13) % sw, hy = sh - ((t * 2 + k * 31) % (sh / 2));
            ui_heart(hx, hy, 3, PAL_PAINT(PAINT_ROSE, 1));
        }
    }
    draw_curtains(sw, sh, open);
    /* HUD: title and applause */
    ui_title(sw / 2, 2, SS->title, WIN_CUTE);
    {
        int bw = wide ? 64 : 40, ax = wide ? 30 : 24, ay = 15;
        ui_round_rect(ax, ay, bw + 18, 11, 4, PAL_PAINT(PAINT_GRAPE, 0));
        ui_round_rect(ax + 13, ay + 3, bw, 5, 2, PAL_RAMP(RAMP_VAPE, 1));
        if (studio.applause)
            ui_round_rect(ax + 13, ay + 3, DOOMO_MAX(4, bw * studio.applause / 100), 5, 2,
                          PAL_PAINT(PAINT_BUBBLEGUM, 2));
        ui_heart(ax + 7, ay + 5, 3 + (beat && studio.applause > 50), PAL_PAINT(PAINT_ROSE, 1));
    }
    if (studio.show == 1 && t < CURTAIN_T + 20)
        ui_text_big(sw / 2 - ui_text_big_width("Showtime!", 2) / 2, sh / 3, "Showtime!", 2,
                    PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_BLOOD, 0));
    karaoke(sw / 2, sh - (wide ? 44 : 25), sw - (wide ? 60 : 24));
    (void)buf;
}

static void draw_bows(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), t = studio.show_t, i;
    int stars = 1 + studio.applause / 21, w = st_wide() ? 200 : 170, h = 82;
    int x = (sw - w) / 2, y = (sh - h) / 2;
    char buf[48];
    /* confetti */
    for (i = 0; i < 24; i++) {
        int cx = (i * 37 + t * (1 + i % 3)) % sw, cy = (i * 23 + t * (2 + i % 2)) % sh;
        plat_rect(cx, cy, 2, 2, PAL_PAINT((i * 5) % 32, 2));
    }
    if (!ui_window_pop(x, y, w, h, WIN_CUTE, t)) return;
    ui_title(sw / 2, y - 5, studio.encore ? "ENCORE!!" : "Bravo!", WIN_CUTE);
    for (i = 0; i < 5; i++)
        ui_star(sw / 2 - 28 + i * 14, y + 16 - (i < stars ? ui_wave(t + i * 5, 20, 1) : 0), 5,
                i < stars ? PAL_PAINT(PAINT_LEMON, 2) : PAL_GRAY_L);
    doomo_snprintf(buf, sizeof(buf), "Applause %d%%", studio.applause);
    ui_text_center(sw / 2, y + 28, buf, ui_style_text(WIN_CUTE));
    doomo_snprintf(buf, sizeof(buf), "%d singer%s +%d happy", song_singer_count(SS),
                   song_singer_count(SS) == 1 ? "" : "s", studio.happy_gain);
    ui_text_center(sw / 2, y + 39, buf, ui_style_text(WIN_CUTE));
    doomo_snprintf(buf, sizeof(buf), "%d/%d lines fit the tune", studio.fit_lines, studio.sung_lines);
    ui_text_center(sw / 2, y + 50, buf, ui_style(WIN_CUTE)->text_dim);
    ui_text_center(sw / 2, y + 65, STR_ICON_A " Back   START Again", ui_style(WIN_CUTE)->text_dim);
}

void stage_draw_full(void)
{
    draw_show();
    if (studio.show == 2) {
        plat_shade_rect(0, 0, plat_screen_w(), plat_screen_h(), 6);
        draw_bows();
    }
}

/* ---- the poster ----------------------------------------------------------- */
void tab_stage_draw(StRect r)
{
    int t = (int)studio.t, i, n = 0, k, wide = st_wide();
    uint8_t slots[SONG_SINGERS], secs[SONG_SINGERS];
    StageBox b;
    char buf[48];
    st_panel(r.x, r.y, r.w, r.h, WIN_DARK);
    /* marquee bulbs */
    for (i = 0; i < (r.w - 8) / 8; i++) {
        int on = ((i + t / 6) & 1);
        ui_fill_circle(r.x + 6 + i * 8, r.y + 4, 2, on ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_HONEY, 0));
        ui_fill_circle(r.x + 6 + i * 8, r.y + r.h - 5, 2, !on ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_HONEY, 0));
    }
    ui_text_big(r.x + r.w / 2 - ui_text_big_width(SS->title, 2) / 2, r.y + 10, SS->title, 2,
                PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_BLOOD, 0));
    for (i = 0; i < SONG_SINGERS; i++)
        if (song_singer_id(SS, i) >= 0) { slots[n] = (uint8_t)i; secs[n] = (uint8_t)song_singer_section(SS, i); n++; }
    doomo_snprintf(buf, sizeof(buf), "%s  " STR_ICON_HEART "  %d singer%s  " STR_ICON_HEART "  %s",
                   doomo_music_names[SS->genre % MUSIC_COUNT], n, n == 1 ? "" : "s",
                   doomo_choir_layout_names[SS->choir_layout % CHOIR_LAYOUT_COUNT]);
    ui_text_center(r.x + r.w / 2, r.y + 30, buf, PAL_PAINT(PAINT_LAVENDER, 2));
    /* the line-up */
    b.x = r.x + 10; b.w = r.w - 20; b.floor = r.y + r.h - (wide ? 34 : 26); b.y = r.y + 40;
    plat_rect(r.x + 8, b.floor, r.w - 16, 3, PAL_RAMP(RAMP_WOOD, 5));
    for (k = 0; k < n; k++) {
        int x, y;
        place(SS->choir_layout, k, n, &b, secs, &x, &y);
        st_chibi(song_singer_id(SS, slots[k]), x - BODYSPR_W / 2,
                 y - BODYSPR_H - (((t / 10) + k) % 8 == 0), 0, 0, 0, 0);
    }
    if (!n) ui_text_center(r.x + r.w / 2, b.floor - 20, "No singers yet: an instrumental!", PAL_GRAY_L);
    {
        int pulse = ui_wave(t, 30, 1);
        ui_button(r.x + r.w / 2 - 50, r.y + r.h - 22 - pulse, 100, STR_ICON_A " Raise the curtain!", 1, WIN_CUTE);
    }
    if (studio_stats.performed) {
        doomo_snprintf(buf, sizeof(buf), "Shows: %d  Best choir: %d", studio_stats.performed,
                       studio_stats.best_choir);
        if (wide) ui_text(r.x + 8, r.y + r.h - 18, buf, PAL_GRAY_L);
    }
}
