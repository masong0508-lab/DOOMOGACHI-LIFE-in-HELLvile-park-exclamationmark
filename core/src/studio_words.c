/* studio_words.c - the Music Studio's LYRICS and CHOIR tabs (owner: studio).
 *
 * LYRICS: 8 lines, each sung over its order position (song.h). The
 * syllable split is shown live (alternating colours), with a fit badge
 * against the lead-track onsets of the line's pattern: a heart when it
 * fits, +n when there are too many syllables, -n when notes go unsung.
 * The detail strip lays the syllables under the pattern's onsets. Who
 * sings each line (solo, duet, a section, a half of the choir or the full
 * choir) is picked with LEFT / RIGHT.
 *
 * CHOIR: up to 25 citizens on a 9/8/8 grid in their section colours,
 * the stage layout row on top, quick fills (friends of someone, a whole
 * district, both districts, random 25), section editing and voice
 * previews (the citizen sings the first sung line alone).
 */
#include "studio_int.h"

static const uint8_t section_paint[SECTION_COUNT] = { PAINT_ROSE, PAINT_PEACH, PAINT_MINT,
                                                      PAINT_SKY };
static const char section_letter[SECTION_COUNT] = { 'S', 'A', 'T', 'B' };

/* stop a one-line preview when its pass is over */
static void preview_watch(void)
{
    if (studio.sing_pos < 0) return;
    if (!song_playing() || song_pos_order() != studio.sing_pos) {
        studio.sing_pos = -1;
        if (song_playing()) song_stop();
    }
}

static int first_sung_pos(const DoomoSong *s)
{
    int p;
    for (p = 0; p < s->order_len; p++) if (s->lyrics[SONG_LINE_OF(p)][0]) return p;
    return 0;
}

static void preview_line(int pos, int singer)
{
    song_stop();
    if (pos < 0) { st_msg("That line isn't in the song"); return; }
    song_play_at(SS, singer, pos, 0);
    studio.sing_pos = (int8_t)pos;
}

/* ====================================================================== */
/* LYRICS                                                                   */
static void lyric_done(int ok)
{
    char *line = SS->lyrics[studio.lsel & 7];
    if (!ok) return;
    if (studio_clean_text(studio.edit, SONG_LYRIC_LEN)) st_msg("Keep it cute! (fixed a word)");
    ed_touch(line, SONG_LYRIC_LEN);
    memset(line, 0, SONG_LYRIC_LEN);
    memcpy(line, studio.edit, strlen(studio.edit) < SONG_LYRIC_LEN ? strlen(studio.edit)
                                                                   : SONG_LYRIC_LEN - 1);
    ed_commit();
    if (line[0] && studio_stats.lines < 0xFFFF) studio_stats.lines++;
    studio_check_unlocks();
    audio_sfx(SFX_HAPPY);
}

/* fit of a line: onsets - syllables (0 = fits); *sung = 0 if the line is
 * never sung */
static int line_fit(const DoomoSong *s, int line, int *onsets, int *syls)
{
    int pos = studio_line_pos(s, line);
    *syls = song_syllables(s->lyrics[line], NULL, 0);
    *onsets = pos >= 0 ? song_lead_onsets(s, s->order[pos] & 3) : -1;
    return pos >= 0;
}

/* all the (part, who) choices for the line part cycler */
static int part_options(const DoomoSong *s, uint8_t *part, int8_t *who, int cap)
{
    int n = 0, p, k, singers = song_singer_count(s);
    for (p = 0; p < PART_COUNT && n < cap; p++) {
        int c = studio_part_choices(s, p);
        if (singers <= 1 && p != PART_SOLO) continue;
        if (p == PART_DUET && singers < 2) continue;
        if (p == PART_HALF && singers < 4) continue;
        for (k = 0; k < c && n < cap; k++) {
            if (p == PART_SECTION) {
                int i, any = 0;
                for (i = 0; i < SONG_SINGERS; i++)
                    if (song_singer_id(s, i) >= 0 && song_singer_section(s, i) == k) any = 1;
                if (!any) continue;
            }
            part[n] = (uint8_t)p;
            who[n] = (int8_t)k;
            n++;
        }
    }
    return n;
}

static void cycle_part(int line, int d)
{
    uint8_t part[64];
    int8_t who[64];
    int n = part_options(SS, part, who, 64), cur = 0, w, p, i;
    if (n <= 1) { st_msg("Pick more singers in Choir"); audio_sfx(SFX_ERROR); return; }
    p = studio_part_of(SS, line, &w);
    for (i = 0; i < n; i++) if (part[i] == p && who[i] == w) { cur = i; break; }
    cur = (cur + d + n) % n;
    ed_touch(&SS->line_singers[line], 4);
    studio_set_part(SS, line, part[cur], who[cur]);
    ed_commit();
    audio_sfx(SFX_CURSOR);
}

static void lyric_tools(int c)
{
    int i, n = 0;
    switch (c) {
    case 0: preview_line(studio_line_pos(SS, studio.lsel), -1); break;
    case 1:
        ed_touch(SS->notes, (int)sizeof(SS->notes));
        ed_touch(SS->lyrics, (int)sizeof(SS->lyrics));
        for (i = 0; i < SONG_LYRIC_LINES; i++) n += studio_autofit(SS, i);
        ed_commit();
        st_msg(n ? "%d lines fitted" : "Everything fits!", n);
        break;
    case 2:
        ed_touch(SS->line_singers, (int)sizeof(SS->line_singers));
        studio_auto_parts(SS);
        ed_commit();
        st_msg("Parts shared out");
        break;
    case 3: {
        int muse = song_singer_id(SS, 0);
        ed_touch(SS->lyrics, (int)sizeof(SS->lyrics));
        ed_touch(SS->title, SONG_TITLE_LEN);
        studio_write_lyrics(SS, muse >= 0 ? muse : DOOMO_PLAYER_ID, doomo_hash32(studio.t + 77u));
        ed_commit();
        st_msg("New words about %s", st_name(muse >= 0 ? muse : DOOMO_PLAYER_ID));
        break; }
    case 4:
        ed_touch(SS->lyrics[studio.lsel], SONG_LYRIC_LEN);
        memset(SS->lyrics[studio.lsel], 0, SONG_LYRIC_LEN);
        ed_commit();
        break;
    default: break;
    }
}

void tab_lyrics_update(void)
{
    preview_watch();
    if (btn_repeat(BTN_UP)) { studio.lsel = (uint8_t)((studio.lsel + 7) % 8); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.lsel = (uint8_t)((studio.lsel + 1) % 8); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) cycle_part(studio.lsel, -1);
    if (btn_repeat(BTN_RIGHT)) cycle_part(studio.lsel, 1);
    if (btn_pressed(BTN_A)) {
        char prompt[32];                     /* ui_text_entry copies it */
        int on, sy;
        line_fit(SS, studio.lsel, &on, &sy);
        if (on >= 0) doomo_snprintf(prompt, sizeof(prompt), "Line %d (%d notes)", studio.lsel + 1, on);
        else doomo_snprintf(prompt, sizeof(prompt), "Line %d", studio.lsel + 1);
        memcpy(studio.edit, SS->lyrics[studio.lsel], SONG_LYRIC_LEN);
        ui_text_entry(prompt, studio.edit, SONG_LYRIC_LEN, lyric_done);
    }
    if (btn_pressed(BTN_START)) {
        ed_touch(SS->notes, (int)sizeof(SS->notes));
        ed_touch(SS->lyrics[studio.lsel], SONG_LYRIC_LEN);
        if (studio_autofit(SS, studio.lsel)) { st_msg("Line %d fits now!", studio.lsel + 1); audio_sfx(SFX_SPARKLE); }
        else st_msg(SS->lyrics[studio.lsel][0] ? "Already a perfect fit" : "Write some words first");
        ed_commit();
    }
    if (studio_sel_tap) {
        static const char *const items[5] = { "Sing this line", "Auto-fit all lines",
                                              "Share out parts", "Write lyrics for me",
                                              "Clear this line" };
        st_popup("Lyric tools", items, 5, 0, lyric_tools);
    }
}

/* the line's text with its syllables in alternating colours; returns the
 * x after it. maxch clips long lines. */
static int draw_syllables(int x, int y, const char *t, uint8_t c0, uint8_t c1, int maxch, int hl)
{
    uint8_t st[SONG_LYRIC_LEN];
    int n = song_syllables(t, st, SONG_LYRIC_LEN), i, len = (int)strlen(t), k = 0;
    char ch[2] = { 0, 0 };
    if (n > SONG_LYRIC_LEN) n = SONG_LYRIC_LEN;
    for (i = 0; i < len && i < maxch; i++) {
        while (k + 1 < n && st[k + 1] <= i) k++;
        ch[0] = t[i];
        if (i == maxch - 1 && len > maxch) ch[0] = '~';
        ui_text(x + i * 6, y, ch, (n && i >= st[0] && k == hl) ? PAL_PAINT(PAINT_ROSE, 0)
                                                               : ((k & 1) ? c1 : c0));
    }
    return x + DOOMO_MIN(len, maxch) * 6;
}

void tab_lyrics_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoSong *s = SS;
    int wide = st_wide(), rowh = wide ? 13 : 10, i, top = r.y + 4;
    int dh = r.h - 8 * rowh - 10, maxch = (r.w - 64) / 6;
    int pl = song_playing() ? song_pos_line() : -1;
    char buf[40];
    st_panel(r.x, r.y, r.w, 8 * rowh + 8, WIN_CREAM);
    for (i = 0; i < SONG_LYRIC_LINES; i++) {
        int y = top + i * rowh, sel = i == studio.lsel, on, sy, sung = line_fit(s, i, &on, &sy);
        int pos = studio_line_pos(s, i);
        if (sel) {
            ui_round_rect(r.x + 3, y - 1, r.w - 6, rowh, 3, st->fill_hi);
            ui_cursor(r.x + 3, y + (rowh - 8) / 2);
        }
        doomo_snprintf(buf, sizeof(buf), "%d", i + 1);
        ui_text(r.x + 13, y + (rowh - 9) / 2, buf, st->text_dim);
        if (pos >= 0) {
            char pc[2] = { (char)('A' + (s->order[pos] & 3)), 0 };
            ui_round_rect(r.x + 21, y, 10, rowh - 2, 3, st_pattern_color(s->order[pos], 1));
            ui_text(r.x + 23, y + (rowh - 9) / 2, pc, PAL_WHITE);
        }
        if (s->lyrics[i][0]) {
            draw_syllables(r.x + 35, y + (rowh - 9) / 2, s->lyrics[i], st->text,
                           PAL_PAINT(PAINT_GRAPE, 0), maxch, pl == i ? song_pos_syllable() : -1);
        } else {
            ui_text(r.x + 35, y + (rowh - 9) / 2, pos >= 0 ? "~ instrumental ~" : "~ not in the song ~",
                    st->text_dim);
        }
        /* fit badge */
        if (s->lyrics[i][0] && sung && on >= 0) {
            int bx = r.x + r.w - 22, by = y + rowh / 2 - 1;
            if (sy == on) {
                ui_heart(bx + 6, by, 3 + (sel && ((studio.t >> 3) & 1)), PAL_PAINT(PAINT_ROSE, 1));
            } else {
                doomo_snprintf(buf, sizeof(buf), "%c%d", sy > on ? '+' : '-', sy > on ? sy - on : on - sy);
                ui_round_rect(bx - 2, by - 4, 18, 9, 3, sy > on ? PAL_PAINT(PAINT_CHERRY, 1)
                                                                : PAL_PAINT(PAINT_SKY, 1));
                ui_text(bx, by - 4, buf, PAL_WHITE);
            }
        }
    }
    /* detail: syllables under the onsets, and who sings */
    if (dh >= 30) {
        int dy = r.y + 8 * rowh + 10, line = studio.lsel, pos = studio_line_pos(s, line);
        int gx = r.x + 6, gw = r.w - 12, k, nsy;
        uint8_t stt[SONG_LYRIC_LEN];
        st_panel(r.x, dy, r.w, dh, WIN_MINT);
        studio_part_label(s, line, buf, sizeof(buf));
        ui_text(gx + 2, dy + 4, STR_ICON_ARROW_L, ui_style_text(WIN_MINT));
        ui_text(gx + 10, dy + 4, buf, ui_style_text(WIN_MINT));
        ui_text(gx + 12 + ui_text_width(buf), dy + 4, STR_ICON_ARROW_R, ui_style_text(WIN_MINT));
        nsy = song_syllables(s->lyrics[line], stt, SONG_LYRIC_LEN);
        if (pos >= 0 && s->lyrics[line][0]) {
            const uint8_t *lead = s->notes[s->order[pos] & 3][0];
            int sx = gw / SONG_STEPS, ky = dy + 17, syl = 0, lastx = -100;
            for (k = 0; k < SONG_STEPS; k++) {
                int x = gx + k * sx;
                plat_rect(x, ky + 4, sx - 1, 1, (k & 3) ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_MINT, 0));
                if (lead[k] >= 2) {
                    int ok = syl < nsy;
                    ui_fill_circle(x + sx / 2, ky + 4, 2, ok ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_SKY, 0));
                    if (ok && x >= lastx + 2 && dh >= 34) {
                        char sb[8];
                        int a = stt[syl], b = syl + 1 < nsy ? stt[syl + 1] : (int)strlen(s->lyrics[line]), j;
                        for (j = 0; j < 6 && a + j < b && s->lyrics[line][a + j] != ' '; j++) sb[j] = s->lyrics[line][a + j];
                        sb[j] = 0;
                        if (j > (wide ? 4 : 3)) { sb[wide ? 4 : 3] = 0; j = wide ? 4 : 3; }
                        ui_text(x, ky + 8 + ((syl & 1) ? 8 : 0), sb, (syl & 1) ? PAL_PAINT(PAINT_GRAPE, 0)
                                                                         : ui_style_text(WIN_MINT));
                        lastx = x + j * 6 - 6;
                    }
                    syl++;
                } else if (lead[k] == SONG_HOLD) {
                    plat_rect(x, ky + 3, sx, 3, PAL_PAINT(PAINT_ROSE, 2));
                }
            }
            /* who sings it (room permitting) */
            if (dh >= 70) {
                uint32_t m = song_line_mask(s, line), sing = song_pos_line() == line ? song_pos_singers() : 0;
                int k2, cxp = gx + 4, bob;
                for (k2 = 0; k2 < SONG_SINGERS && cxp < gx + gw - 16; k2++) {
                    if (!((m >> k2) & 1)) continue;
                    bob = ((sing >> k2) & 1) ? ((studio.t >> 2) & 1) : 0;
                    st_chibi(song_singer_id(s, k2), cxp, dy + dh - 28 - bob, 0, 0, 0, 0);
                    cxp += 18;
                }
            }
            if (syl < nsy && dh >= 34) {
                doomo_snprintf(buf, sizeof(buf), "+%d", nsy - syl);
                ui_text(gx + gw - 14, dy + 4, buf, PAL_PAINT(PAINT_CHERRY, 0));
            }
        } else {
            ui_text(gx + 2, dy + 18, pos < 0 ? "Add more passes in Order to sing this line"
                                             : "Write words to sing here", PAL_GRAY);
        }
    }
}

/* ====================================================================== */
/* CHOIR                                                                    */
#define CHOIR_ROW 25

static void slot_xy(StRect r, int i, int *x, int *y, int *tw, int *th)
{
    int wide = st_wide(), row = i < 9 ? 0 : i < 17 ? 1 : 2, col = i < 9 ? i : i < 17 ? i - 9 : i - 17;
    int cw = wide ? 33 : 25, ch = wide ? 40 : 31, n = row == 0 ? 9 : 8;
    int gx = r.x + (r.w - n * cw) / 2;
    *tw = cw - 2;
    *th = ch - 3;
    *x = gx + col * cw;
    *y = r.y + 20 + row * ch;
}

static void pick_done(int id)
{
    int i, slot = studio.csel;
    if (id < 0 || slot >= SONG_SINGERS) return;
    for (i = 0; i < SONG_SINGERS; i++)
        if (song_singer_id(SS, i) == id && i != slot) {
            st_msg("%s already sings!", st_name(id));
            audio_sfx(SFX_ERROR);
            return;
        }
    ed_touch(&SS->singer[slot], 2);
    ed_touch(SS->section, (int)sizeof(SS->section));
    SS->singer[slot] = (uint16_t)id;
    song_set_section(SS, slot, -1);
    ed_commit();
    st_msg("%s joins as %s!", st_name(id), doomo_section_names[song_singer_section(SS, slot)]);
    audio_sfx(SFX_HAPPY);
}

static void remove_singer(int slot)
{
    int i;
    ed_touch(&SS->singer[slot], 2);
    ed_touch(SS->line_singers, (int)sizeof(SS->line_singers));
    SS->singer[slot] = SONG_NO_SINGER;
    for (i = 0; i < SONG_LYRIC_LINES; i++) SS->line_singers[i] &= ~(1u << slot);
    ed_commit();
}

static void cycle_section(int slot)
{
    int v = (SS->section[slot >> 1] >> ((slot & 1) * 4)) & 15;   /* 0 auto, 1..4 */
    ed_touch(SS->section, (int)sizeof(SS->section));
    v = (v + 1) % (SECTION_COUNT + 1);
    song_set_section(SS, slot, v - 1);
    ed_commit();
    st_msg("%s: %s%s", st_name(song_singer_id(SS, slot)),
           doomo_section_names[song_singer_section(SS, slot)], v ? "" : " (auto)");
    audio_sfx(SFX_CURSOR);
}

static void member_menu(int c)
{
    int slot = studio.csel, id = song_singer_id(SS, slot);
    switch (c) {
    case 0: preview_line(first_sung_pos(SS), id); break;
    case 1: st_pick_citizen("Who sings here?", id, pick_done); break;
    case 2: cycle_section(slot); break;
    case 3: remove_singer(slot); st_msg("%s takes a bow", st_name(id)); break;
    default: break;
    }
}

static void sort_by_section(void)
{
    DoomoSong *s = SS;
    uint16_t ids[SONG_SINGERS];
    uint8_t secv[SONG_SINGERS], map[SONG_SINGERS];
    int i, k, n = 0, sec;
    ed_touch(s->singer, (int)sizeof(s->singer));
    ed_touch(s->section, (int)sizeof(s->section));
    ed_touch(s->line_singers, (int)sizeof(s->line_singers));
    for (i = 0; i < SONG_SINGERS; i++) map[i] = 0xFF;
    for (sec = 0; sec < SECTION_COUNT; sec++)
        for (i = 0; i < SONG_SINGERS; i++)
            if (song_singer_id(s, i) >= 0 && song_singer_section(s, i) == sec) {
                ids[n] = s->singer[i];
                secv[n] = (uint8_t)((s->section[i >> 1] >> ((i & 1) * 4)) & 15);
                map[i] = (uint8_t)n++;
            }
    for (k = 0; k < SONG_LYRIC_LINES; k++) {
        uint32_t m = s->line_singers[k], nm = 0;
        for (i = 0; i < SONG_SINGERS; i++) if ((m >> i) & 1 && map[i] != 0xFF) nm |= 1u << map[i];
        s->line_singers[k] = nm;
    }
    memset(s->section, 0, sizeof(s->section));
    for (i = 0; i < SONG_SINGERS; i++) {
        s->singer[i] = i < n ? ids[i] : SONG_NO_SINGER;
        if (i < n) song_set_section(s, i, secv[i] ? secv[i] - 1 : -1);
    }
    ed_commit();
}

static void fill_menu(int c)
{
    static const uint8_t modes[5] = { FILL_FRIENDS, FILL_HILLVILE, FILL_HELLVILE, FILL_BOTH,
                                      FILL_RANDOM };
    int n, anchor = studio.csel < SONG_SINGERS ? song_singer_id(SS, studio.csel) : -1;
    if (c < 0) return;
    if (c == 5) { ed_touch(SS->line_singers, (int)sizeof(SS->line_singers)); studio_auto_parts(SS); ed_commit(); st_msg("Parts shared out"); return; }
    if (c == 6) { sort_by_section(); st_msg("Sorted: S A T B"); return; }
    ed_touch(SS->singer, (int)sizeof(SS->singer));
    ed_touch(SS->section, (int)sizeof(SS->section));
    ed_touch(SS->line_singers, (int)sizeof(SS->line_singers));
    if (c == 7) {
        studio_choir_fill(SS, FILL_CLEAR, -1, 0);
        ed_commit();
        st_msg("The choir went home");
        return;
    }
    n = studio_choir_fill(SS, modes[c], anchor >= 0 ? anchor : DOOMO_PLAYER_ID,
                          doomo_hash32(studio.t * 31u + (uint32_t)c));
    ed_commit();
    st_msg("%d singers on the risers!", n);
    audio_sfx(SFX_SPARKLE);
}

void tab_choir_update(void)
{
    int s = studio.csel;
    preview_watch();
    if (s == CHOIR_ROW) {
        if (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT) || btn_pressed(BTN_A)) {
            int d = btn_repeat(BTN_LEFT) ? -1 : 1, k, l = SS->choir_layout;
            for (k = 0; k < CHOIR_LAYOUT_COUNT; k++) {
                l = (l + d + CHOIR_LAYOUT_COUNT) % CHOIR_LAYOUT_COUNT;
                if (studio_layout_ok(l)) break;
            }
            ed_touch(&SS->choir_layout, 1);
            SS->choir_layout = (uint8_t)l;
            ed_commit();
            audio_sfx(SFX_CURSOR);
        }
        if (btn_repeat(BTN_DOWN)) { studio.csel = 4; audio_sfx(SFX_CURSOR); }
    } else {
        int row = s < 9 ? 0 : s < 17 ? 1 : 2, col = s < 9 ? s : s < 17 ? s - 9 : s - 17;
        if (btn_repeat(BTN_LEFT)) s = s > 0 ? s - 1 : SONG_SINGERS - 1;
        if (btn_repeat(BTN_RIGHT)) s = s < SONG_SINGERS - 1 ? s + 1 : 0;
        if (btn_repeat(BTN_UP)) {
            if (row == 0) s = CHOIR_ROW;
            else s = row == 1 ? DOOMO_MIN(col, 8) : 9 + DOOMO_MIN(col, 7);
        }
        if (btn_repeat(BTN_DOWN) && row < 2) s = row == 0 ? 9 + DOOMO_MIN(col, 7) : 17 + col;
        if (s != studio.csel) { studio.csel = (uint8_t)s; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A)) {
            if (song_singer_id(SS, studio.csel) >= 0) {
                static const char *const items[4] = { "Hear their voice", "Change singer",
                                                      "Change section", "Remove" };
                st_popup(st_name(song_singer_id(SS, studio.csel)), items, 4, 0, member_menu);
            } else {
                st_pick_citizen("Who sings here?", DOOMO_PLAYER_ID, pick_done);
            }
        }
        if (btn_pressed(BTN_START) && song_singer_id(SS, studio.csel) >= 0) cycle_section(studio.csel);
    }
    if (studio_sel_tap) {
        static const char *const items[8] = { "Friends of this singer", "Hillvile choir",
                                               "Hellvile choir", "Both districts!", "Random 25",
                                               "Share out parts", "Sort by section",
                                               "Clear the choir" };
        st_popup("Quick fill", items, 8, 0, fill_menu);
    }
}

static void layout_icon(int x, int y, int layout, uint8_t c)
{
    int i;
    switch (layout) {
    case CHOIR_RISERS:
        for (i = 0; i < 3; i++) plat_rect(x + i * 2, y + 6 - i * 3, 14 - i * 4, 2, c);
        break;
    case CHOIR_SEMICIRCLE:
        for (i = 0; i < 7; i++) plat_rect(x + i * 2, y + 1 + (i - 3) * (i - 3) / 2, 2, 2, c);
        break;
    case CHOIR_SECTIONS:
        for (i = 0; i < 4; i++) plat_rect(x + i * 4, y + 2, 3, 6, c);
        break;
    default:
        for (i = 0; i < 4; i++) plat_rect(x + 6 - i * 2, y + i * 2, 2 + i * 4, 2, c);
        break;
    }
}

void tab_choir_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoSong *s = SS;
    int i, wide = st_wide(), n = song_singer_count(s);
    uint32_t singing = song_pos_singers();
    char buf[48];
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    /* stage row */
    doomo_snprintf(buf, sizeof(buf), "%s%s", doomo_choir_layout_names[s->choir_layout % CHOIR_LAYOUT_COUNT],
                   studio_layout_ok(CHOIR_PYRAMID) ? "" : "");
    ui_value_row(r.x + 8, r.y + 4, wide ? 180 : 140, "Stage", buf, studio.csel == CHOIR_ROW);
    layout_icon(r.x + (wide ? 196 : 154), r.y + 5, s->choir_layout, PAL_PAINT(PAINT_GRAPE, 1));
    doomo_snprintf(buf, sizeof(buf), "%d/25", n);
    ui_text(r.x + r.w - 6 - ui_text_width(buf), r.y + 5, buf, st->text_dim);
    for (i = 0; i < SONG_SINGERS; i++) {
        int x, y, tw, th, id = song_singer_id(s, i), sel = i == studio.csel;
        slot_xy(r, i, &x, &y, &tw, &th);
        if (id >= 0) {
            int sec = song_singer_section(s, i), bop = ((singing >> i) & 1) ? ((studio.t >> 2) & 1) : 0;
            char sl[2] = { section_letter[sec], 0 };
            ui_round_rect(x, y, tw, th, 4, PAL_PAINT(section_paint[sec], sel ? 1 : 2));
            if (sel) ui_frame(x - 1, y - 1, tw + 2, th + 2, PAL_PAINT(PAINT_ROSE, 0));
            st_chibi(id, x + (tw - BODYSPR_W) / 2, y + th - BODYSPR_H - 1 - bop, 0,
                     sel ? (int)(studio.t / 8) & 3 : 0, 0, 0);
            ui_round_rect(x + tw - 8, y + 1, 7, 8, 2, PAL_PAINT(section_paint[sec], 0));
            ui_text(x + tw - 7, y + 1, sl, PAL_WHITE);
        } else {
            ui_round_rect(x, y, tw, th, 4, st->fill_hi);
            if (sel) ui_frame(x - 1, y - 1, tw + 2, th + 2, PAL_PAINT(PAINT_ROSE, 0));
            ui_text(x + tw / 2 - 2, y + th / 2 - 4, "+", sel ? PAL_PAINT(PAINT_ROSE, 0) : st->text_dim);
        }
    }
    if (studio.csel < SONG_SINGERS) {
        int x, y, tw, th;
        slot_xy(r, studio.csel, &x, &y, &tw, &th);
        ui_heart(x + tw / 2, y - 3 + ui_wave((int)studio.t, 16, 1), 3, PAL_PAINT(PAINT_ROSE, 1));
    }
    /* who is selected */
    {
        int iy = r.y + 20 + 3 * (wide ? 40 : 31) + 1, id = studio.csel < SONG_SINGERS ? song_singer_id(s, studio.csel) : -1;
        if (id >= 0) {
            const DoomoResident *res = town_res(id);
            int sec = song_singer_section(s, studio.csel);
            doomo_snprintf(buf, sizeof(buf), "%s  %s%s", st_name(id), doomo_section_names[sec],
                           ((s->section[studio.csel >> 1] >> ((studio.csel & 1) * 4)) & 15) ? "" : " (auto)");
            ui_text(r.x + 8, iy, buf, st->text);
            if (wide && res && iy + 30 < r.y + r.h) {
                const DoomoImage *img = portrait_image(id, (singing >> studio.csel) & 1 ? EXPR_TALK : EXPR_HAPPY);
                if (img) plat_image(r.x + r.w - 56, iy - 2, img, 1, 0, 0);
                doomo_snprintf(buf, sizeof(buf), "Loves %s, fears %s",
                               doomo_music_names[res->like_music % MUSIC_COUNT],
                               doomo_fear_names[res->fear % FEAR_COUNT]);
                ui_text(r.x + 8, iy + 11, buf, st->text_dim);
                ui_text(r.x + 8, iy + 22, RES_DISTRICT(id) && id < DOOMO_MAX_RESIDENTS ? "From Hellvile"
                        : id == DOOMO_PLAYER_ID ? "That's you!" : "From Hillvile", st->text_dim);
            }
        } else if (studio.csel < SONG_SINGERS) {
            ui_text(r.x + 8, iy, STR_ICON_A " add a singer   SELECT: quick fill", st->text_dim);
        } else {
            ui_text(r.x + 8, iy, STR_ICON_ARROW_L STR_ICON_ARROW_R " how the choir stands on stage", st->text_dim);
        }
    }
}
