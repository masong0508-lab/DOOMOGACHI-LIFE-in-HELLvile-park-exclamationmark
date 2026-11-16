/* studio_grid.c - ST-TOOLS (owner: studio): the drum editor and the chord track, two screens of the NOTES tab.
 *
 * Both are views of the song's own bytes (no new song data, so old songs and saves are untouched): the drum editor shows
 * the drums track of one pattern as eight lanes by 32 steps, the chord track shows the chord of every half bar of
 * every pattern (the bass root and the harmony third that the chord suggester writes). Every press is one undo step,
 * every change has a sound and a picture, and B goes back to the grid.
 *
 *   DRUMS   up / down: lane, left / right: step, A: put the lane's drum on the step (or take it off), L / R: pattern.
 *           One drum per step: the song format has one drums cell per step, so a new drum replaces the one there.
 *           SELECT: groove, clear lane, clear drums, repeat bar 1, loop this bar.
 *   CHORDS  up / down: pattern, left / right: half bar, A: pick the chord (four that fit, all, clear), L / R: pattern.
 *           SELECT: pick, clear, repeat to the end of the part, hear the part.
 *
 * The cursor lives in studio.pat / studio.stp / studio.lane (SONG's row cursor is idle while NOTES is open). */
#include "studio_int.h"

static const char *const lane_short[8] = { "Kik", "Snr", "Hat", "Opn", "Clp", "Tom", "Crs", "Cow" };
static const char *const roman[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };

void st_sub_open(int sub)
{
    studio.sub = (uint8_t)sub;
    studio.a_edit = 0;
    studio.rng_a = -1;
    studio.lane = (uint8_t)(studio.lane & 7);
    st_msg(sub == SUB_DRUMS ? "Drum editor: B goes back" : "Chord track: B goes back");
    audio_sfx(SFX_SELECT);
}

static void sub_close(void)
{
    studio.sub = SUB_NONE;
    audio_sfx(SFX_CANCEL);
}

static void next_pattern(int d)
{
    studio.pat = (uint8_t)((studio.pat + d + SONG_PATTERNS) % SONG_PATTERNS);
    audio_sfx(SFX_CURSOR);
}

/* ---- the drum editor ------------------------------------------------------------------------------------------- */
static int op_drum_lane(DoomoSong *s, const int *a)   { return tl_drum_lane_clear(s, a[0], a[1]); }
static int op_drum_clear(DoomoSong *s, const int *a)  { return tl_clear(s, a[0], 3, 0, SONG_STEPS); }
static int op_drum_repeat(DoomoSong *s, const int *a) { return tl_repeat(s, a[0], 3, 0, 16); }

static void drum_tools(int c)
{
    int a[2], n, p = studio.pat;
    a[0] = p; a[1] = (studio.lane & 7) + 1;
    switch (c) {
    case 0: st_groove_menu(); break;
    case 1:
        n = st_tool_apply(SS->notes[p], (int)sizeof(SS->notes[p]), op_drum_lane, a);
        st_tool_done(n, "Lane cleared");
        break;
    case 2:
        n = st_tool_apply(SS->notes[p], (int)sizeof(SS->notes[p]), op_drum_clear, a);
        st_tool_done(n, "Drums cleared");
        break;
    case 3:
        n = st_tool_apply(SS->notes[p], (int)sizeof(SS->notes[p]), op_drum_repeat, a);
        if (n) st_drum_hear(p);
        st_tool_done(n, "Bar 1 repeated in bar 2");
        break;
    case 4:
        song_play_range(SS, p, studio.stp >= 16 ? 16 : 0, studio.stp >= 16 ? 32 : 16);
        st_msg("Looping bar %d of %c", studio.stp / 16 + 1, 'A' + p);
        break;
    default: break;
    }
}

static void drum_menu(void)
{
    static const char *const items[5] = { "Pick a groove", "Clear this lane", "Clear all drums", "Repeat bar 1 in bar 2",
                                          "Loop this bar" };
    static const char *const help[5] = { "A ready-made beat for this part.", "Take out every hit of this lane.",
                                         "Empty the drums of this part.", "Copy bar 1's beat over bar 2.",
                                         "Hear this bar again and again." };
    st_popup("Drum tools", items, 5, 0, drum_tools);
    st_popup_help(help);
}

static void drum_update(void)
{
    int lane = studio.lane & 7;
    uint8_t *c;
    if (btn_repeat(BTN_UP)) { studio.lane = (uint8_t)((lane + 7) & 7); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.lane = (uint8_t)((lane + 1) & 7); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) { studio.stp = (uint8_t)((studio.stp + SONG_STEPS - 1) % SONG_STEPS); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_RIGHT)) { studio.stp = (uint8_t)((studio.stp + 1) % SONG_STEPS); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_L)) next_pattern(-1);
    if (btn_pressed(BTN_R)) next_pattern(1);
    lane = studio.lane & 7;
    c = &SS->notes[studio.pat & 3][3][studio.stp & 31];
    if (btn_pressed(BTN_A)) {
        int v;
        ed_touch(c, 1);
        v = tl_drum_toggle(SS, studio.pat, studio.stp, lane + 1);
        ed_commit();
        if (v) { if (!song_playing()) song_preview(SS, 3, v); }
        else audio_sfx(SFX_BLIP);
    } else if (btn_pressed(BTN_B)) {
        sub_close();
    }
    if (studio_sel_tap) drum_menu();
}

void st_drum_hear(int p)
{
    int i;
    if (song_playing()) return;
    for (i = 0; i < SONG_STEPS; i++)
        if (SS->notes[p & 3][3][i] > 0) { song_preview(SS, 3, SS->notes[p & 3][3][i]); return; }
}

static void drum_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoSong *s = SS;
    int wide = st_wide(), lw = wide ? 34 : 26, colw = (r.w - lw - 10) / SONG_STEPS, rowh = wide ? 12 : 10;
    int gx = r.x + lw + 4, gy = r.y + 20, l, b, i, ps = song_pos_step(), here = 0;
    char buf[40];
    const uint8_t *row3 = s->notes[studio.pat & 3][3];
    if (song_playing() && song_pos_order() >= 0 && song_pos_order() < s->order_len)
        here = (s->order[song_pos_order()] & 3) == studio.pat;
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    st_pattern_pills(r.x + 6, r.y + 4);
    ui_text(r.x + 84, r.y + 6, "Drum editor", st->text_dim);
    for (l = 0; l < 8; l++) {
        int y = gy + l * rowh, cur = l == (studio.lane & 7);
        ui_text(r.x + 4, y + (rowh - 9) / 2 + 1, lane_short[l], cur ? st_track_color(3, 0) : st->text_dim);
        for (b = 0; b < SONG_STEPS / 4; b++)                      /* a beat is one block of four steps */
            plat_rect(gx + b * 4 * colw, y, 4 * colw - 1, rowh - 1,
                      cur ? PAL_PAINT(PAINT_LEMON, 2) : ((b & 1) ? st->fill_lo : st->fill));
        if (here) plat_rect(gx + ps * colw, y, colw, rowh - 1, PAL_PAINT(PAINT_LEMON, 1));
    }
    for (i = 0; i < SONG_STEPS; i++) {                              /* the hits: one block each, in the lane's row */
        int v = row3[i];
        if (v > 0 && v < SONG_DRUM_COUNT)
            plat_rect(gx + i * colw + 1, gy + (v - 1) * rowh + 1, colw - 2 < 2 ? 2 : colw - 2, rowh - 3, st_track_color(3, 1));
    }
    plat_rect(gx + 16 * colw - 1, gy - 2, 1, 8 * rowh + 3, st->outline);   /* the bar line */
    {                                                                /* the cursor: a frame round the cell */
        int cx = gx + studio.stp * colw, cy = gy + (studio.lane & 7) * rowh, cw = colw < 3 ? 3 : colw;
        plat_rect(cx - 1, cy, cw + 1, 1, PAL_PAINT(PAINT_ROSE, 0));
        plat_rect(cx - 1, cy + rowh - 2, cw + 1, 1, PAL_PAINT(PAINT_ROSE, 0));
        plat_rect(cx - 1, cy, 1, rowh - 1, PAL_PAINT(PAINT_ROSE, 0));
        plat_rect(cx + cw - 1, cy, 1, rowh - 1, PAL_PAINT(PAINT_ROSE, 0));
    }
    {                                                                /* the cursor's cell on one line */
        int v = row3[studio.stp & 31];
        doomo_snprintf(buf, sizeof(buf), "Step %d: %s", studio.stp + 1,
                       v ? doomo_song_drum_names[v % SONG_DRUM_COUNT] : "no drum");
        ui_text(r.x + 6, gy + 8 * rowh + 3, buf, st->text);
        ui_text(r.x + (wide ? 150 : 110), gy + 8 * rowh + 3, doomo_song_kit_names[s->wave[3] % SONG_KIT_COUNT], st->text_dim);
    }
}

/* ---- the chord track ------------------------------------------------------------------------------------------- */
static void hear_chord(int p, int h)
{
    if (song_playing()) return;
    if (SS->notes[p & 3][2][h * 8] >= 2) song_preview(SS, 2, SS->notes[p & 3][2][h * 8]);
    if (SS->notes[p & 3][1][h * 8] >= 2) song_preview(SS, 1, SS->notes[p & 3][1][h * 8]);
}

static int op_chord_repeat(DoomoSong *s, const int *a) { return tl_chord_repeat(s, a[0], a[1]); }
static int op_chord_clear2(DoomoSong *s, const int *a) { return tl_chord_clear(s, a[0], a[1]); }

static void chord_tools(int c)
{
    int a[2], n, h = studio.stp / 8 & 3;
    a[0] = studio.pat; a[1] = h;
    switch (c) {
    case 0: st_chord_menu(); break;
    case 1:
        n = st_tool_apply(SS->notes[studio.pat], (int)sizeof(SS->notes[studio.pat]), op_chord_clear2, a);
        st_tool_done(n, "Chord cleared");
        break;
    case 2:
        n = st_tool_apply(SS->notes[studio.pat], (int)sizeof(SS->notes[studio.pat]), op_chord_repeat, a);
        if (n) hear_chord(studio.pat, h);
        st_tool_done(n, "Chord repeated to the end");
        break;
    case 3:
        song_play_range(SS, studio.pat, 0, SONG_STEPS);
        st_msg("Looping part %c", 'A' + studio.pat);
        break;
    default: break;
    }
}

static void chord_menu(void)
{
    static const char *const items[4] = { "Pick a chord", "Clear this chord", "Repeat to the end", "Hear this part" };
    static const char *const help[4] = { "Four chords that fit, or any.", "Rest the bass and harmony here.",
                                         "Use this chord to the part's end.", "Play this part over and over." };
    st_popup("Chord tools", items, 4, 0, chord_tools);
    st_popup_help(help);
}

static void chord_update(void)
{
    int col = studio.stp / 8 & 3, moved = 0;
    if (btn_repeat(BTN_UP)) { next_pattern(-1); moved = 1; }
    if (btn_repeat(BTN_DOWN)) { next_pattern(1); moved = 1; }
    if (btn_pressed(BTN_L)) { next_pattern(-1); moved = 1; }
    if (btn_pressed(BTN_R)) { next_pattern(1); moved = 1; }
    if (btn_repeat(BTN_LEFT)) { col = (col + 3) & 3; moved = 1; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_RIGHT)) { col = (col + 1) & 3; moved = 1; audio_sfx(SFX_CURSOR); }
    studio.stp = (uint8_t)(col * 8);
    if (moved) hear_chord(studio.pat, col);                          /* the chord under the cursor sounds */
    if (btn_pressed(BTN_A)) st_chord_menu();
    else if (btn_pressed(BTN_B)) sub_close();
    if (studio_sel_tap) chord_menu();
}

static void chord_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoSong *s = SS;
    int wide = st_wide(), cw = (r.w - 38) / 4, ch = wide ? 22 : 16, gx = r.x + 30, gy = r.y + 30, p, h, ps = song_pos_step();
    int playing = song_playing() && song_pos_order() >= 0 && song_pos_order() < s->order_len ? s->order[song_pos_order()] & 3 : -1;
    char nm[6], buf[40];
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    st_pattern_pills(r.x + 6, r.y + 4);
    ui_text(r.x + 84, r.y + 6, "Chord track", st->text_dim);
    ui_text(gx + 4, r.y + 18, "Bar 1", st->text_dim);
    ui_text(gx + 2 * cw + 4, r.y + 18, "Bar 2", st->text_dim);
    for (p = 0; p < SONG_PATTERNS; p++) {
        int y = gy + p * (ch + 2), cur_row = p == studio.pat;
        ui_round_rect(r.x + 6, y, 18, ch, 3, st_pattern_color(p, cur_row ? 0 : 1));
        ui_text(r.x + 12, y + (ch - 9) / 2 + 1, (const char[]){ (char)('A' + p), 0 }, PAL_WHITE);
        for (h = 0; h < 4; h++) {
            int x = gx + h * cw, d = tl_chord_at(s, p, h), cur = cur_row && h == (studio.stp / 8 & 3);
            int now = playing == p && ps / 8 == h;
            plat_rect(x, y, cw - 2, ch, cur ? st_track_color(1, 1) : (now ? PAL_PAINT(PAINT_LEMON, 2) : (h & 1 ? st->fill_lo : st->fill)));
            if (d >= 0) {
                tl_chord_name(s, d, nm, sizeof(nm));
                ui_text(x + 4, y + (ch - 9) / 2 + 1, nm, cur ? PAL_WHITE : st->text);
                ui_text(x + cw - 4 - ui_text_width(roman[d % 7]), y + (ch - 9) / 2 + 1, roman[d % 7],
                        cur ? PAL_WHITE : st->text_dim);
            } else {
                ui_text(x + 4, y + (ch - 9) / 2 + 1, "-", st->text_dim);
            }
            if (cur) {                                             /* a frame round the chord under the cursor */
                plat_rect(x - 1, y - 1, cw, 1, PAL_PAINT(PAINT_ROSE, 0));
                plat_rect(x - 1, y + ch, cw, 1, PAL_PAINT(PAINT_ROSE, 0));
                plat_rect(x - 1, y - 1, 1, ch + 2, PAL_PAINT(PAINT_ROSE, 0));
                plat_rect(x + cw - 2, y - 1, 1, ch + 2, PAL_PAINT(PAINT_ROSE, 0));
            }
        }
    }
    {
        int d = tl_chord_at(s, studio.pat, studio.stp / 8 & 3);
        if (d >= 0) {
            tl_chord_name(s, d, nm, sizeof(nm));
            doomo_snprintf(buf, sizeof(buf), "%c, half bar %d: %s", 'A' + studio.pat, (studio.stp / 8 & 3) + 1, nm);
        } else {
            doomo_snprintf(buf, sizeof(buf), "%c, half bar %d: no chord", 'A' + studio.pat, (studio.stp / 8 & 3) + 1);
        }
        ui_text(r.x + 6, gy + SONG_PATTERNS * (ch + 2) + 2, buf, st->text);
    }
}

/* ---- the entry points tab_notes_* call ---------------------------------------------------------------------------- */
void st_sub_update(void)
{
    if (studio.sub == SUB_DRUMS) drum_update();
    else chord_update();
}

void st_sub_draw(StRect r)
{
    if (studio.sub == SUB_DRUMS) drum_draw(r);
    else chord_draw(r);
}

int st_sub_prompts(UiPrompt *p)
{
    int n = 0;
#define PR(b, pr, v) do { p[n].btn = (uint16_t)(b); p[n].prio = (uint8_t)(pr); p[n].verb = (v); n++; } while (0)
    if (studio.sub == SUB_DRUMS) {
        PR(BTN_A, 0, "Drum on / off");
        PR(BTN_START, 1, st_play_verb());
        PR(BTN_SELECT, 2, "Tools");
        PR(BTN_B, 3, "Back");
        PR(BTN_L | BTN_R, 4, "Pattern");
    } else {
        PR(BTN_A, 0, "Pick chord");
        PR(BTN_START, 1, st_play_verb());
        PR(BTN_SELECT, 2, "Tools");
        PR(BTN_B, 3, "Back");
        PR(BTN_UP | BTN_DOWN, 4, "Pattern");
    }
#undef PR
    return n;
}
