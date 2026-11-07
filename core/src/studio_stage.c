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
    if (inrow > 1) sp = DOOMO_MIN(sp, (b->w - 24) / (inrow - 1));
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
/* the curtain's half-width at `open` (0 closed .. 256 open), and the columns its 6-px strips really cover on each
 * side (they are opaque and run the whole screen height) */
static int curtain_cw(int sw, int open)
{
    int half = sw / 2;
    return half - (half - 22) * open / 256;
}

static int curtain_cover(int sw, int open)
{
    return (curtain_cw(sw, open) + 5) / 6 * 6;
}

/* MS-PF7: the stage's few fixed-size ellipses (the singers' glows 8x2, the conductor's shadow 7x2, the valance
 * scallops 8x4) as the rects ellipse_draw makes for them (half-widths 8,8,4 / 7,7,3 / 8,8,7,6,2, merged by
 * nested_bands): same pixels, no fx_isqrt. The PS1 / native build keeps the cached shape. */
static void stage_ellipse(int cx, int cy, int rx, int ry, uint8_t c)
{
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_STUDIO_RECT_ELL)
    if (rx == 8 && ry == 2) {
        plat_rect(cx - 4, cy - 2, 9, 5, c);
        plat_rect(cx - 8, cy - 1, 17, 3, c);
        return;
    }
    if (rx == 7 && ry == 2) {
        plat_rect(cx - 3, cy - 2, 7, 5, c);
        plat_rect(cx - 7, cy - 1, 15, 3, c);
        return;
    }
    if (rx == 8 && ry == 4) {
        plat_rect(cx - 2, cy - 4, 5, 9, c);
        plat_rect(cx - 6, cy - 3, 13, 7, c);
        plat_rect(cx - 7, cy - 2, 15, 5, c);
        plat_rect(cx - 8, cy - 1, 17, 3, c);
        return;
    }
#endif
    ui_fill_ellipse(cx, cy, rx, ry, c);
}

static void draw_curtains(int sw, int sh, int open)
{
    /* open 0 = closed .. 256 = fully open */
    int cw = curtain_cw(sw, open), i, x;
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
        stage_ellipse(i + 8, 9, 8, 4, PAL_PAINT(PAINT_BLOOD, 0));
        plat_rect(i + 7, 11, 2, 2, PAL_PAINT(PAINT_HONEY, 1));
    }
    plat_rect(0, 7, sw, 1, PAL_PAINT(PAINT_HONEY, 1));
}

#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_STUDIO_RECT_HEART)
/* MS-PF2: a lit LED, ui_heart_flat(x, y, 3, c) as the five rects its spans make (hf_row for s = 3: rows -3..3 hold
 * [-2,-1] + [1,2], [-3,3] x 3, [-2,2], [0,0]). Same pixels, none of the per-row maths and span merging, which were
 * the cost of the ~12 hearts lit at any time. The PS1 / native build keeps the call. */
static void led_heart(int x, int y, uint8_t c)
{
    plat_rect(x - 2, y - 3, 2, 1, c);
    plat_rect(x + 1, y - 3, 2, 1, c);
    plat_rect(x - 3, y - 2, 7, 3, c);
    plat_rect(x - 2, y + 1, 5, 1, c);
    plat_rect(x, y + 2, 1, 1, c);
}
#else
#define led_heart(x, y, c) ui_heart_flat((x), (y), 3, (c))
#endif

static void draw_wall(int sw, int top, int floor, int t, int beat, int open)
{
    int y, x, i, h = floor - top, cover = curtain_cover(sw, open);
    for (i = 0; i < 4; i++)
        plat_rect(0, top + h * i / 4, sw, h / 4 + 1, PAL_RAMP(RAMP_VAPE, 1 + i));
    /* LED hearts: a grid that ripples out from the centre on the beat */
    for (y = top + 8; y < floor - 10; y += 14)
        for (x = 10; x < sw - 6; x += 16) {
            int d = (x > sw / 2 ? x - sw / 2 : sw / 2 - x) / 16 + (y - top) / 14;
            int lit = ((t / 4 - d) & 7) == 0 || (beat && d < 3);
            /* MS-PF8: an LED wholly under the (opaque, full-height) curtain strips is painted over: skip it */
            if (x + 3 < cover || x - 3 >= sw - cover) continue;
            /* only the lit LEDs are hearts: the rest are cheap dots (GBA) */
            if (lit) led_heart(x, y, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            else { plat_rect(x - 2, y - 1, 2, 2, PAL_RAMP(RAMP_VAPE, 3)); plat_rect(x + 1, y - 1, 2, 2, PAL_RAMP(RAMP_VAPE, 3)); plat_rect(x - 1, y + 1, 3, 1, PAL_RAMP(RAMP_VAPE, 3)); }
        }
}

static void draw_floor(int sw, int floor, int bottom)
{
    int y;
    for (y = floor; y < bottom; y += 4)
        plat_rect(0, y, sw, 4, PAL_RAMP(RAMP_WOOD, ((y - floor) / 4) & 1 ? 4 : 5));
    plat_rect(0, floor, sw, 1, PAL_RAMP(RAMP_WOOD, 7));
}

/* MS-PF4: the follow / sweep spots. The cone is a checker row every 2 px with w = 4 + k * 26 / den and a centre
 * x = x0 + (x1 - x0) * k / den (k = y - top, den = bottom - top, C division truncating toward zero). The GBA has
 * no divide instruction (gba_udivmod: two calls a row, ~130 rows a frame), so both are stepped: floor(m * k / den)
 * grows by floor(2m / den) plus a carried remainder per 2-px step; a negative x extent is the negated floor of its
 * magnitude, which is what truncation toward zero gives. On the GBA / native test build consecutive rows with the
 * same left edge and width (the follow spot's, and the cone's flat stretches) are one taller plat_checker: the dot
 * phase is taken from the call's own corner and a 2-row shift keeps it, so the pixels are the same. */
static void spot(int x0, int x1, int top, int bottom, uint8_t c)
{
    int den = DOOMO_MAX(bottom - top, 1), dx = x1 - x0, m = dx < 0 ? -dx : dx, y;
    int qw = 0, rw = 0, qx = 0, rx = 0;
    int w_q = 52 / den, w_r = 52 % den, x_q = (2 * m) / den, x_r = (2 * m) % den;
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_STUDIO_SPOT_RUNS)
    int ox = 0, oy = 0, ow = 0, oh = 0;
#endif
    for (y = top; y < bottom; y += 2) {
        int w = 4 + qw, x = x0 + (dx < 0 ? -qx : qx);
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_STUDIO_SPOT_RUNS)
        int l = x - w / 2;
        if (oh && l == ox && w == ow) oh += 2;
        else {
            if (oh) plat_checker(ox, oy, ow, oh, c);
            ox = l; oy = y; ow = w; oh = 2;
        }
#else
        plat_checker(x - w / 2, y, w, 2, c);
#endif
        qw += w_q; rw += w_r;
        if (rw >= den) { rw -= den; qw++; }
        qx += x_q; rx += x_r;
        if (rx >= den) { rx -= den; qx++; }
    }
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_STUDIO_SPOT_RUNS)
    if (oh) plat_checker(ox, oy, ow, oh, c);
#endif
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
    {
        /* MS-PF3: with the plain font style in English every glyph is one plat_text at its own x, so a run of
         * glyphs with one colour and one bounce is one call at the run's x: the same pixels (as draw_syllables
         * on the LYRICS tab does), about a quarter of the calls. Other styles / languages keep the old path. */
        int runs = opt_get(OPT_FONT_STYLE) == 0 && lang_get() == LANG_EN && !plat_draw_skipped();
        char run[SONG_LYRIC_LEN + 1];
        int rn = 0, rx = 0, rb = 0;
        uint8_t rc = 0;
        for (i = 0; i < len; i++) {
            int k = 0, bounce = 0;
            char ch[2] = { t[i], 0 };
            uint8_t c;
            while (k + 1 < n && st[k + 1] <= i) k++;
            if (n == 0 || i < st[0]) k = -1;
            if (k >= 0 && k < syl) c = PAL_PAINT(PAINT_LEMON, 2);
            else if (k == syl && syl >= 0) { c = PAL_PAINT(PAINT_BUBBLEGUM, 2); bounce = song_beat_age() < 4; }
            else c = PAL_WHITE;
            if (runs && (unsigned char)ch[0] >= 32 && (unsigned char)ch[0] < 127 && rn < (int)sizeof(run) - 1) {
                if (rn && (c != rc || bounce != rb)) { run[rn] = 0; plat_text(rx, y + 7 - rb, run, rc); rn = 0; }
                if (!rn) { rx = x + i * 6; rc = c; rb = bounce; }
                run[rn++] = ch[0];
            } else {
                if (rn) { run[rn] = 0; plat_text(rx, y + 7 - rb, run, rc); rn = 0; }
                ui_text(x + i * 6, y + 7 - bounce, ch, c);
            }
        }
        if (rn) { run[rn] = 0; plat_text(rx, y + 7 - rb, run, rc); }
    }
    /* who has the line */
    studio_part_label(SS, line, buf, sizeof(buf));
    if (wide || song_singer_count(SS) > 1) {
        int bw = ui_text_width(buf) + 10;
        ui_round_rect(cx - bw / 2, y - 9, bw, 10, 3, PAL_PAINT(PAINT_GRAPE, 0));
        ui_text(cx - bw / 2 + 5, y - 8, buf, PAL_PAINT(PAINT_LAVENDER, 2));
    }
}

/* ---- option C: the choir as hardware sprites (docs/STUDIO_30FPS_PLAN.md section 11) -----------------------------
 * A platform that has sprites (plat_stage_obj_begin) shows a singer's chibi above the whole bitmap. That is the same
 * picture only when nothing the bitmap gets LATER in the frame touches the chibi's 16x24 box, so each singer is
 * decided by what draw_show draws after it, worked out here from the same numbers, backwards from the front:
 *   - the fixed things drawn after the choir: the HUD band, both curtain strips (and their tie-backs), the karaoke
 *     bar, the conductor, the crowd and its glow sticks, the floating hearts, the follow spot and its star;
 *   - the floating notes of the singers drawn after this one (a note sits above its own singer and over the rows
 *     behind), and every later chibi that stays in the bitmap (its box, so its mouth too).
 * The mouth is part of the sprite (a patch list); the singer's own note is drawn after it and can reach its box (low
 * note, bopping singer), then it stays in the bitmap. All the
 * boxes are generous, a false "touched" only keeps a singer in the bitmap. The checks of this are in
 * test_studio.c (studio_stage_sprites_equal_the_bitmap). */
typedef struct { int16_t x0, y0, x1, y1; } SBox;

#ifdef DOOMO_HITCHECK
/* test only (the native build): bits switch a check of choir_obj_plan off, so a test can show it catches the
 * mismatch. 1 notes, 2 later bitmap chibis, 4 curtains, 8 follow spot, 16 conductor, 32 a singer's own note. */
int studio_stage_obj_fault;
#define OBJ_FAULT(b) (studio_stage_obj_fault & (b))
#else
#define OBJ_FAULT(b) 0
#endif

static int sbox_hit(const SBox *b, int x0, int y0, int x1, int y1)
{
    return b->x0 < x1 && x0 < b->x1 && b->y0 < y1 && y0 < b->y1;
}

typedef struct {
    int sw, sh, floor, top, open, wide, t, beat, n, lx, ly;
    uint32_t singing;
    const uint8_t *slots, *ord;
    const int *px, *py;
} StageCtx;

static int choir_bop(const StageCtx *c, int kk)
{
    int sing = (c->singing >> c->slots[kk]) & 1;
    return (c->beat ? 1 : 0) + (sing && song_beat_age() < 2 ? 1 : 0);
}

/* obj[singer index] = 1: this singer can be a sprite; returns how many */
static int choir_obj_plan(const StageCtx *c, uint8_t *obj)
{
    SBox cov[12], lb[SONG_SINGERS], ln[SONG_SINGERS];
    int nc = 0, nlb = 0, nln = 0, m, k, nobj = 0, cut = curtain_cover(c->sw, c->open) + 4;
    int cx = c->sw / 2, cy = c->floor + (c->wide ? 34 : 19);
    cov[nc++] = (SBox){ 0, 0, (int16_t)c->sw, 28 };                                   /* the title and the applause bar */
    if (!OBJ_FAULT(4)) {
        cov[nc++] = (SBox){ 0, 0, (int16_t)cut, (int16_t)c->sh };                     /* the curtain strips */
        cov[nc++] = (SBox){ (int16_t)(c->sw - cut), 0, (int16_t)c->sw, (int16_t)c->sh };
    }
    cov[nc++] = (SBox){ 0, (int16_t)(c->sh - (c->wide ? 44 : 25) - 10), (int16_t)c->sw, (int16_t)c->sh };   /* karaoke */
    if (!OBJ_FAULT(16))
        cov[nc++] = (SBox){ (int16_t)(cx - 9), (int16_t)(cy - 33), (int16_t)(cx + 22), (int16_t)(cy + 4) };  /* conductor */
    cov[nc++] = (SBox){ 0, (int16_t)(c->floor + 2), (int16_t)c->sw, (int16_t)c->sh };                        /* crowd */
    for (k = 0; k < studio.applause / 40 && nc < 10; k++) {                           /* hearts */
        int hx = (k * 47 + 13) % c->sw, hy = c->sh - ((c->t * 2 + k * 31) % (c->sh / 2));
        cov[nc++] = (SBox){ (int16_t)(hx - 7), (int16_t)(hy - 7), (int16_t)(hx + 8), (int16_t)(hy + 8) };
    }
    if (c->lx >= 0 && !OBJ_FAULT(8))                                                  /* follow spot + star */
        cov[nc++] = (SBox){ (int16_t)(c->lx - 30), (int16_t)c->top, (int16_t)(c->lx + 31), (int16_t)(c->ly + 1) };   /* the beam is 4 wide at the top, 56 at the lead */
    for (m = c->n - 1; m >= 0; m--) {
        int kk = c->ord[m], x = c->px[kk], y = c->py[kk], bop = choir_bop(c, kk), i, ok = 1;
        int x0 = x - BODYSPR_W / 2, y0 = y - BODYSPR_H - bop, x1 = x0 + BODYSPR_W, y1 = y0 + BODYSPR_H;
        int hasn = ((c->singing >> c->slots[kk]) & 1) && ((c->t + kk * 5) % 24) < 12;
        SBox nb = { 0, 0, 0, 0 };
        if (hasn) {                                                                   /* its note: drawn after it */
            int nx = x + 4, ny = y - BODYSPR_H - 8 - ((c->t + kk * 5) % 24) / 2;
            nb = (SBox){ (int16_t)nx, (int16_t)ny, (int16_t)(nx + 6), (int16_t)(ny + 8) };
        }
        for (i = 0; i < nc && ok; i++) if (sbox_hit(&cov[i], x0, y0, x1, y1)) ok = 0;
        for (i = 0; i < nln && ok && !OBJ_FAULT(1); i++) if (sbox_hit(&ln[i], x0, y0, x1, y1)) ok = 0;
        for (i = 0; i < nlb && ok && !OBJ_FAULT(2); i++) if (sbox_hit(&lb[i], x0, y0, x1, y1)) ok = 0;
        /* its own note reaches its box when the note is low and the singer bops up (a note's last row is y + 7) */
        if (hasn && ok && !OBJ_FAULT(32) && sbox_hit(&nb, x0, y0, x1, y1)) ok = 0;
        obj[kk] = (uint8_t)ok;
        if (ok) nobj++;
        else lb[nlb++] = (SBox){ (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1 };
        if (hasn) ln[nln++] = nb;
    }
    return nobj;
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
    int pxs[SONG_SINGERS], pys[SONG_SINGERS];
    StageBox b;
    char buf[40];
    /* the choir in order (sections layout: grouped) */
    for (i = 0; i < SONG_SINGERS; i++)
        if (song_singer_id(SS, i) >= 0) { slots[n] = (uint8_t)i; secs[n] = (uint8_t)song_singer_section(SS, i); n++; }
    b.x = 24; b.w = sw - 48; b.floor = floor; b.y = top;
    for (i = 0; i < n; i++) place(SS->choir_layout, i, n, &b, secs, &pxs[i], &pys[i]);   /* once a frame, used twice below */
    draw_wall(sw, top, floor, t, beat, open);
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
            int x = pxs[k], y = pys[k];
            if (!((singing >> slots[k]) & 1)) continue;
            stage_ellipse(x, y, 8, 2, PAL_PAINT(sec_paint[secs[k]], 2));
        }
    }
    /* the choir, back rows first (smaller y first) */
    {
        int k, m, *px = pxs, *py = pys;
        uint8_t ord[SONG_SINGERS], obj[SONG_SINGERS];
        for (k = 0; k < n; k++) {
            for (m = k; m > 0 && py[ord[m - 1]] > py[k]; m--) ord[m] = ord[m - 1];
            ord[m] = (uint8_t)k;
        }
        memset(obj, 0, sizeof(obj));
        /* option C: sprites only in the sung show proper (not the curtain's "Showtime!", not the bows, nothing over it) */
        if (n > 0 && studio.show == 1 && t >= CURTAIN_T + 20 && !studio.pop_n && !studio.msg_t && !studio.picking &&
            !ui_toast_pending() && plat_stage_obj_begin()) {
            StageCtx c;
            for (k = 0; k < n; k++) if (slots[k] == lead) { lx = px[k]; ly = py[k]; }
            c.sw = sw; c.sh = sh; c.floor = floor; c.top = top; c.open = open; c.wide = wide; c.t = t; c.beat = beat;
            c.n = n; c.lx = lx; c.ly = ly; c.singing = singing; c.slots = slots; c.ord = ord; c.px = px; c.py = py;
            choir_obj_plan(&c, obj);
            lx = -1; ly = 0;                                   /* found again below, as before */
        }
        for (m = 0; m < n; m++) {
            int kk = ord[m], x = px[kk], y = py[kk], id = song_singer_id(SS, slots[kk]);
            int sing = (singing >> slots[kk]) & 1, bop, open_mouth = (song_beat_age() & 2) == 0, np = 0;
            int cx0, cy0;
            const DoomoImage *img = bodyspr_image(id, 0, 0);
            DoomoPatch pt[2] = { { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 } };
            bop = (beat ? 1 : 0) + (sing && song_beat_age() < 2 ? 1 : 0);
            cx0 = x - BODYSPR_W / 2;
            cy0 = y - BODYSPR_H - bop;
            if (sing && obj[kk]) {                              /* the mouth, as the patches draw_mouth would paint */
                if (open_mouth) {
                    pt[0] = (DoomoPatch){ 7, 14, 3, 3, PAL_PAINT(PAINT_BLOOD, 0) };
                    pt[1] = (DoomoPatch){ 8, 16, 1, 1, PAL_PAINT(PAINT_ROSE, 2) };
                    np = 2;
                } else {
                    pt[0] = (DoomoPatch){ 7, 15, 3, 1, PAL_PAINT(PAINT_BLOOD, 0) };
                    np = 1;
                }
            }
            if (!(obj[kk] && img && plat_stage_obj(slots[kk], cx0, cy0, img, sing ? 1 + open_mouth : 0, pt, np))) {
                if (img) plat_image(cx0, cy0, img, 1, 0, 0);
                if (sing) draw_mouth(cx0, cy0, open_mouth);
            }
            if (sing) {
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
        stage_ellipse(cx, cy, 7, 2, PAL_RAMP(RAMP_WOOD, 2));
        st_chibi(studio.conductor, cx - BODYSPR_W / 2, cy - BODYSPR_H, 3, (age < 2) ? 1 : 0, 0, 0);
        plat_line(cx + 4, cy - 12, px, py, PAL_WHITE);
        ui_sparkle(px, py, (t >> 1) & 15, PAL_PAINT(PAINT_LEMON, 2));
    }
    /* the crowd in the dark */
    {
        int k, cw = sw / (STUDIO_CROWD / 2 + 1), cyb = sh - 4;
        for (k = 0; k < STUDIO_CROWD; k++) {
            int id = studio.crowd[k], row = k & 1, x = cw / 2 + (k / 2) * cw + row * cw / 2 - 8;
            int y = cyb - BODYSPR_H + 8 - row * 6;                 /* nobody bobs on the beat: the crowd stands still */
            if (y < floor + 8) continue;
            if (id >= 0) st_chibi(id, x, y, 3, 0, IMG_SILHOUETTE, PAL_PAINT(PAINT_GRAPE, 0));
            else ui_fill_circle(x + 8, y + 10, 7, PAL_PAINT(PAINT_GRAPE, 0));
            if (studio.applause > 70 && !(k & 1)) {                /* glow sticks: every other fan, only at high applause */
                int gx = x + 12, gy = y + 4 - ui_wave(t + k * 9, 30, 2);
                plat_line(gx, gy, gx + 2, gy - 6, PAL_PAINT(k & 1 ? PAINT_LIME : PAINT_BUBBLEGUM, 2));
            }
        }
        /* hearts float up from a happy crowd */
        for (k = 0; k < studio.applause / 40; k++) {                 /* at most two hearts */
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
    if (wide)
        doomo_snprintf(buf, sizeof(buf), "%s  " STR_ICON_HEART "  %d singer%s  " STR_ICON_HEART "  %s",
                       doomo_music_names[SS->genre % MUSIC_COUNT], n, n == 1 ? "" : "s",
                       doomo_choir_layout_names[SS->choir_layout % CHOIR_LAYOUT_COUNT]);
    else
        doomo_snprintf(buf, sizeof(buf), "%d singer%s " STR_ICON_HEART " %s", n, n == 1 ? "" : "s",
                       doomo_choir_layout_names[SS->choir_layout % CHOIR_LAYOUT_COUNT]);
    ui_text_center(r.x + r.w / 2, r.y + 30, buf, PAL_PAINT(PAINT_LAVENDER, 2));
    /* the line-up on a little stage between two curtains */
    b.x = r.x + 18; b.w = r.w - 36; b.floor = r.y + r.h - (wide ? 40 : 28); b.y = r.y + 40;
    plat_rect(r.x + 8, b.floor, r.w - 16, r.y + r.h - 9 - b.floor, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(r.x + 8, b.floor, r.w - 16, 1, PAL_RAMP(RAMP_WOOD, 6));
    for (i = 0; i < 3; i++) {
        plat_rect(r.x + 8 + i * 4, r.y + 9, 4, b.floor - r.y - 9, PAL_PAINT(PAINT_BLOOD, (i & 1) ? 0 : 1));
        plat_rect(r.x + r.w - 12 - i * 4, r.y + 9, 4, b.floor - r.y - 9, PAL_PAINT(PAINT_BLOOD, (i & 1) ? 0 : 1));
    }
    spot(r.x + r.w / 2 + ui_wave(t, 120, r.w / 5), r.x + r.w / 2, r.y + 36, b.floor, PAL_PAINT(PAINT_LEMON, 2));
    for (k = 0; k < n; k++) {
        int x, y;
        place(SS->choir_layout, k, n, &b, secs, &x, &y);
        st_chibi(song_singer_id(SS, slots[k]), x - BODYSPR_W / 2,
                 y - BODYSPR_H - (((t / 10) + k) % 8 == 0), 0, 0, 0, 0);
    }
    if (!n) ui_text_center(r.x + r.w / 2, b.floor - 20, "No singers yet: an instrumental!", PAL_GRAY_L);
    {
        int pulse = ui_wave(t, 30, 1);
        ui_button(r.x + r.w / 2 - 60, r.y + r.h - 23 - pulse, 120, STR_ICON_A " Raise the curtain!", 1, WIN_CUTE);
    }
    if (studio_stats.performed) {
        doomo_snprintf(buf, sizeof(buf), "Shows: %d  Best choir: %d", studio_stats.performed,
                       studio_stats.best_choir);
        if (wide) ui_text(r.x + 8, r.y + r.h - 18, buf, PAL_GRAY_L);
    }
}
