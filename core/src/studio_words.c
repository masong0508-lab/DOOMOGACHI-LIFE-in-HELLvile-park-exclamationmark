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
    st_adv_play(pos, singer);
    studio.sing_pos = (int8_t)pos;
}

/* ====================================================================== */
/* LYRICS                                                                   */
static void lyric_done(int ok)
{
    char *line = st_line_buf(SS, studio.lsel);
    if (!ok || !line) return;
    if (studio_clean_text(studio.edit, SONG_LYRIC_LEN)) st_msg("Keep it cute! (fixed a word)");
    st_line_touch_text(SS, studio.lsel);
    memset(line, 0, SONG_LYRIC_LEN);
    memcpy(line, studio.edit, strlen(studio.edit) < SONG_LYRIC_LEN ? strlen(studio.edit)
                                                                   : SONG_LYRIC_LEN - 1);
    ed_commit();
    if (line[0] && STUDIO_LINES(&studio_stats) < STUDIO_LINES_MASK) studio_stats.lines++;
    studio_check_unlocks();
    audio_sfx(SFX_HAPPY);
}

/* fit of a line: onsets - syllables (0 = fits); *sung = 0 if the line is
 * never sung */
static int line_fit(const DoomoSong *s, int line, int *onsets, int *syls)
{
    int pos = studio_line_pos(s, line), pt = studio_line_pat(s, line);
    *syls = song_syllables(st_line_text(s, line), NULL, 0);
    *onsets = pos >= 0 && pt >= 0 ? song_lead_onsets(s, pt) : -1;
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
    st_line_touch_mask(SS, line);
    studio_set_part(SS, line, part[cur], who[cur]);
    ed_commit();
    audio_sfx(SFX_CURSOR);
}

/* A deliberately cheap lyric check: compare the vowel tail of the last word
 * in neighbouring line pairs. It runs only from the tools menu and allocates
 * nothing, so the editor's frame time and RAM stay unchanged. */
static int rhyme_tail(const char *s, char out[8])
{
    int end = (int)strlen(s), start, vowel = -1, i, n = 0;
    while (end && !((s[end - 1] >= 'A' && s[end - 1] <= 'Z') || (s[end - 1] >= 'a' && s[end - 1] <= 'z'))) end--;
    start = end;
    while (start && ((s[start - 1] >= 'A' && s[start - 1] <= 'Z') || (s[start - 1] >= 'a' && s[start - 1] <= 'z'))) start--;
    for (i = start; i < end; i++) {
        char c = (char)(s[i] | 32);
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y') vowel = i;
    }
    if (vowel < 0) vowel = DOOMO_MAX(start, end - 2);
    for (i = vowel; i < end && n < 7; i++) out[n++] = (char)(s[i] | 32);
    out[n] = 0;
    return n;
}

static void rhyme_report(void)
{
    int i, pairs = 0, rhymes = 0, lines = st_line_count(SS);
    for (i = 0; i + 1 < lines; i += 2) {
        char a[8], b[8];
        if (!rhyme_tail(st_line_text(SS, i), a) || !rhyme_tail(st_line_text(SS, i + 1), b)) continue;
        pairs++;
        if (!strcmp(a, b)) rhymes++;
    }
    if (!pairs) st_msg("Write two lyric lines first");
    else st_msg("Rhymes: %d of %d line pairs", rhymes, pairs);
    audio_sfx(rhymes ? SFX_SPARKLE : SFX_CURSOR);
}

static void lyric_tools(int c)
{
    int i, n = 0;
    switch (c) {
    case 0: preview_line(studio_line_pos(SS, studio.lsel), -1); break;
    case 1:
        ed_touch(SS->notes, (int)sizeof(SS->notes));
        ed_touch(SS->lyrics, (int)sizeof(SS->lyrics));
        for (i = SONG_LYRIC_LINES; i < st_line_count(SS); i++) st_line_touch_text(SS, i);
        for (i = 0; i < st_line_count(SS); i++) n += studio_autofit(SS, i);
        ed_commit();
        st_msg(n ? "%d lines fitted" : "Everything fits!", n);
        break;
    case 2:
        ed_touch(SS->line_singers, (int)sizeof(SS->line_singers));
        st_xmasks(XM_TOUCH, 0, NULL);
        studio_auto_parts(SS);
        ed_commit();
        st_msg("Parts shared out");
        break;
    case 3: {
        int muse = song_singer_id(SS, 0);
        ed_touch(SS->lyrics, (int)sizeof(SS->lyrics));
        ed_touch(SS->title, SONG_TITLE_LEN);
        { int ln; for (ln = SONG_LYRIC_LINES; ln < st_line_count(SS); ln++) st_line_touch_text(SS, ln); }   /* ST-ADV: lines 9..16 */
        studio_write_lyrics(SS, muse >= 0 ? muse : DOOMO_PLAYER_ID, doomo_hash32(studio.t + 77u));
        ed_commit();
        st_msg("New words about %s", st_name(muse >= 0 ? muse : DOOMO_PLAYER_ID));
        break; }
    case 4:
        st_line_touch_text(SS, studio.lsel);
        memset(st_line_buf(SS, studio.lsel), 0, SONG_LYRIC_LEN);
        ed_commit();
        break;
    case 5:                                   /* was START (START plays now): fit one line to its notes */
        ed_touch(SS->notes, (int)sizeof(SS->notes));
        st_line_touch_text(SS, studio.lsel);
        if (studio_autofit(SS, studio.lsel)) { st_msg("Line %d fits now!", studio.lsel + 1); audio_sfx(SFX_SPARKLE); }
        else st_msg(st_line_text(SS, studio.lsel)[0] ? "Already a perfect fit" : "Write some words first");
        ed_commit();
        break;
    case 6: {                                   /* ST-TOOLS: the syllable fitter's report */
        int w = 0, nn = 0, v = tl_fit_status(SS, studio.lsel, &w, &nn);
        if (v < 0) st_msg(st_line_text(SS, studio.lsel)[0] ? "This line is not sung yet" : "Write some words first");
        else if (v == 0) st_msg("%d word%s, %d note%s: a fit!", w, w == 1 ? "" : "s", nn, nn == 1 ? "" : "s");
        else if (v == 1) st_msg("%d word%s, %d note%s: la la fills", w, w == 1 ? "" : "s", nn, nn == 1 ? "" : "s");
        else st_msg("%d word%s, %d note%s: too many", w, w == 1 ? "" : "s", nn, nn == 1 ? "" : "s");
        audio_sfx(v == 0 ? SFX_SPARKLE : SFX_CURSOR);
        break; }
    case 7: rhyme_report(); break;
    default: break;
    }
}

void tab_lyrics_update(void)
{
    preview_watch();
    {
        int nl = st_line_count(SS);                      /* 8 lines, or 16 in Advanced: the list scrolls (8 rows are shown) */
        if (studio.lsel >= nl) studio.lsel = 0;
        if (btn_repeat(BTN_UP)) { studio.lsel = (uint8_t)((studio.lsel + nl - 1) % nl); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { studio.lsel = (uint8_t)((studio.lsel + 1) % nl); audio_sfx(SFX_CURSOR); }
        if (nl <= 8) studio.ltop = 0;
        else if (studio.lsel < studio.ltop) studio.ltop = studio.lsel;
        else if (studio.lsel >= studio.ltop + 8) studio.ltop = (uint8_t)(studio.lsel - 7);
    }
    if (btn_repeat(BTN_LEFT)) cycle_part(studio.lsel, -1);
    if (btn_repeat(BTN_RIGHT)) cycle_part(studio.lsel, 1);
    if (btn_pressed(BTN_A)) {
        char prompt[32];                     /* ui_text_entry copies it */
        int on, sy;
        line_fit(SS, studio.lsel, &on, &sy);
        if (on >= 0) doomo_snprintf(prompt, sizeof(prompt), "Line %d (%d notes)", studio.lsel + 1, on);
        else doomo_snprintf(prompt, sizeof(prompt), "Line %d", studio.lsel + 1);
        memcpy(studio.edit, st_line_text(SS, studio.lsel), SONG_LYRIC_LEN);
        ui_text_entry(prompt, studio.edit, SONG_LYRIC_LEN, lyric_done);
    }
    if (studio_sel_tap) {
        static const char *const items[8] = { "Sing this line", "Auto-fit all lines",
                                              "Share out parts", "Write lyrics for me",
                                              "Clear this line", "Fit this line", "How does it fit?", "Check the rhymes" };
        static const char *const help[8] = { "Hear this line sung.", "Fit every line to its notes.",
                                             "Share the lines out.", "Have lyrics written for you.",
                                             "Empty this line.", "Fit this line to its notes.",
                                             "Count words against notes.", "Compare each pair's ending sound." };
        st_popup("Lyric tools", items, 8, 0, lyric_tools);
        st_popup_help(help);
    }
}

/* the line's text with its syllables in alternating colours; returns the
 * x after it. maxch clips long lines. */
static int draw_syllables(int x, int y, const char *t, const uint8_t *st, int n, uint8_t c0, uint8_t c1, int maxch, int hl)
{
    int i, len = (int)strlen(t), k = 0;
    /* MS-P3: with the plain font style in English every glyph is one plat_text at its own x, so a run of
     * glyphs of one colour is one call at the run's x: same pixels (as ui_text_clip does), a quarter of
     * the calls. Other styles / languages keep the one-call-per-glyph path (shadow order, translation). */
    int runs = opt_get(OPT_FONT_STYLE) == 0 && lang_get() == LANG_EN && !plat_draw_skipped();
    /* MS-P3b: the font is fixed-pitch (6 px) and a space draws nothing, so a whole line is at most three plat_text
     * calls, one per colour (even syllables, odd syllables, the sung one): each colour's string keeps its own
     * glyphs at their own columns and spaces elsewhere. Same pixels (the glyph cells do not overlap), about a
     * fifth of the calls of one call per colour change. */
    char cb[3][SONG_LYRIC_LEN + 1], ch[2] = { 0, 0 };
    uint8_t cc[3] = { 0, 0, 0 };
    int ncol = 0, lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 }, j;
    if (n > SONG_LYRIC_LEN) n = SONG_LYRIC_LEN;
    /* SP4 (G-S1): the same three strings built a syllable at a time (a syllable is one colour: its chars are copied
     * into that colour's string in one go) instead of a char at a time, which was 10% of the LYRICS tab on the GBA.
     * Only for a line of printable chars with its syllable starts in order; anything else takes the loop below. */
    if (runs && len < (int)sizeof(cb[0])) {
        int end = DOOMO_MIN(len, maxch), ok;
        unsigned bad = 0;
        for (i = 0; i < end; i++) bad |= (unsigned)((unsigned char)t[i] - 32u) >= 95u;   /* (no branch a char) */
        ok = !bad;
        for (j = 1; j < n && ok; j++) ok = st[j - 1] <= st[j];
        if (ok) {
            int seg, jlast = -1;
            for (seg = -1; seg < n; seg++) {
                /* seg -1: the chars before the first syllable (colour c0); seg k: syllable k up to the next start */
                int a = seg < 0 ? 0 : st[seg], b = seg + 1 < n ? st[seg + 1] : end;
                uint8_t col = seg < 0 ? c0 : seg == hl ? PAL_PAINT(PAINT_ROSE, 0) : ((seg & 1) ? c1 : c0);
                if (seg < 0 && n == 0) b = end;
                if (b > end) b = end;
                if (a >= b) continue;
                for (j = 0; j < ncol && cc[j] != col; j++) {}
                if (j == ncol) {
                    memset(cb[j], ' ', sizeof(cb[j]));
                    cc[j] = col; lo[j] = a; ncol++;
                }
                memcpy(cb[j] + a, t + a, (size_t)(b - a));
                hi[j] = b - 1;
                if (b == end) jlast = j;
            }
            if (jlast >= 0 && len > maxch) cb[jlast][end - 1] = '~';   /* the clipped line's last cell */
            for (j = 0; j < ncol; j++) {
                cb[j][hi[j] + 1] = 0;
                plat_text(x + lo[j] * 6, y, cb[j] + lo[j], cc[j]);
            }
            return x + end * 6;
        }
    }
    for (i = 0; i < len && i < maxch; i++) {
        uint8_t col;
        while (k + 1 < n && st[k + 1] <= i) k++;
        ch[0] = t[i];
        if (i == maxch - 1 && len > maxch) ch[0] = '~';
        col = (n && i >= st[0] && k == hl) ? PAL_PAINT(PAINT_ROSE, 0) : ((k & 1) ? c1 : c0);
        if (runs && (unsigned char)ch[0] >= 32 && (unsigned char)ch[0] < 127) {
            for (j = 0; j < ncol && cc[j] != col; j++) {}
            if (j == ncol) {
                if (ncol == 3) { ui_text(x + i * 6, y, ch, col); continue; }   /* cannot happen: three colours */
                memset(cb[j], ' ', sizeof(cb[j]));
                cc[j] = col; lo[j] = i; ncol++;
            }
            cb[j][i] = ch[0];
            hi[j] = i;
        } else {
            ui_text(x + i * 6, y, ch, col);
        }
    }
    for (j = 0; j < ncol; j++) {
        cb[j][hi[j] + 1] = 0;
        plat_text(x + lo[j] * 6, y, cb[j] + lo[j], cc[j]);
    }
    return x + DOOMO_MIN(len, maxch) * 6;
}

/* LYRICS-30 step 2: the detail strip under the selected line (32 step ticks, the onset dots, the hold bars) is
 * a pile of small rects: ~60 plat_rect calls a frame on the GBA, though it only changes when the selected line
 * (or its words) does. Two halves of 16 steps, each one baked picture (st_bake: the same pixels, drawn once through
 * the UI kit's rasteriser, then one plat_image). Per step a 2-bit state: 0 tick only, 1 onset with a syllable,
 * 2 onset without one, 3 hold. a[0] sx, a[1] state bits of this half (step k in bits 2k..2k+1). */
static void ly_strip_half(int x, int y, const int *a)
{
    int sx = a[0], k;
    uint32_t bits = (uint32_t)a[1];
    for (k = 0; k < SONG_STEPS / 2; k++) {
        int st = (int)((bits >> (2 * k)) & 3u), cx = x + k * sx;
        ST_RECT(cx, y + 4, sx - 1, 1, (k & 3) ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_MINT, 0));
        if (st == 1 || st == 2)
            ui_fill_circle(cx + sx / 2, y + 4, 2, st == 1 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_SKY, 0));
        else if (st == 3)
            ST_RECT(cx, y + 3, sx, 3, PAL_PAINT(PAINT_ROSE, 2));
    }
}

/* st[0] = states of steps 0..15, st[1] = steps 16..31; (gx, ky) = the strip's origin */
static void ly_strip(int gx, int ky, int sx, const uint32_t st[2])
{
    int h;
    for (h = 0; h < 2; h++) {
        int a[2], ax = gx + h * (SONG_STEPS / 2) * sx;
        a[0] = sx; a[1] = (int)st[h];
#if ST_BAKE
        if (st_bake(st[h], 0x4C590000u | (uint32_t)h << 8 | (uint32_t)sx, ax, ky, ly_strip_half, a, 1024)) continue;
#endif
        ly_strip_half(ax, ky, a);
    }
}

/* LYRICS-30 step 3: each of the 8 list rows paints two small rounded shapes under its text, the pattern chip and the
 * "+n / -n" fit badge (a few rects each, ~50 plat_rect calls a frame in all). Baked like the strip (st_bake: same
 * pixels, painted once, then one plat_image); the text goes on top afterwards, as before. The heart (it pulses on
 * the selected row) stays as it was. a[0] colour, a[1] chip height. Keys: kinds 0x32 / 0x33 (0x10 0x11 0x31 0x40 are taken). */
static void ly_chip_shape(int x, int y, const int *a)
{
    ui_round_rect(x, y, 10, a[1], 3, (uint8_t)a[0]);
}

static void ly_badge_shape(int x, int y, const int *a)
{
    ui_round_rect(x, y, 18, 9, 3, (uint8_t)a[0]);
}

static void ly_chip(int x, int y, int h, uint8_t col)
{
    int a[2];
    a[0] = col; a[1] = h;
#if ST_BAKE
    if (st_bake(0x32u | (uint32_t)h << 8, col, x, y, ly_chip_shape, a, 256)) return;
#endif
    ly_chip_shape(x, y, a);
}

static void ly_badge(int x, int y, uint8_t col)
{
    int a[2];
    a[0] = col; a[1] = 0;
#if ST_BAKE
    if (st_bake(0x33u, col, x, y, ly_badge_shape, a, 256)) return;
#endif
    ly_badge_shape(x, y, a);
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
    int onc[SONG_PATTERNS] = { -1, -1, -1, -1 };      /* song_lead_onsets once a pattern a frame, not once a line */
    uint8_t sel_st[SONG_LYRIC_LEN];                   /* the selected line's syllables, for the detail strip */
    int sel_n = 0;
    int nlines = st_line_count(s), first = nlines > 8 ? DOOMO_MIN((int)studio.ltop, nlines - 8) : 0;
    for (i = first; i < first + 8 && i < nlines; i++) {
        int y = top + (i - first) * rowh, sel = i == studio.lsel, on = -1, sy, sung, pat;
        int pos = studio_line_pos(s, i);
        const char *lw = st_line_text(s, i);
        uint8_t sts[SONG_LYRIC_LEN];
        sy = song_syllables(lw, sts, SONG_LYRIC_LEN);   /* once a line a frame (it was 2-3 lookups) */
        /* the pattern once a row (studio_line_pat scans the order again each call; same value) */
        pat = pos < 0 ? -1 : (s == &studio.song && st_adv_on()) ? st_adv_line_pattern(i) : (s->order[pos] & 3);
        sung = pos >= 0 && pat >= 0;
        if (sung) {
            int pt = pat;
            if (onc[pt] < 0) onc[pt] = song_lead_onsets(s, pt);
            on = onc[pt];
        }
        if (sel) { memcpy(sel_st, sts, sizeof(sel_st)); sel_n = sy; }
        if (sel) {
            ui_round_rect(r.x + 3, y - 1, r.w - 6, rowh, 3, st->fill_hi);
            ui_cursor(r.x + 3, y + (rowh - 8) / 2);
        }
        if (pl == i)
            plat_rect(r.x + 5, y, 2, rowh - 1, PAL_PAINT(PAINT_LEMON, 1));
        if (i < 9) { buf[0] = (char)('1' + i); buf[1] = 0; }       /* lines 1..9: no snprintf */
        else { buf[0] = (char)('0' + (i + 1) / 10); buf[1] = (char)('0' + (i + 1) % 10); buf[2] = 0; }
        ui_text(i < 9 ? r.x + 13 : r.x + 8, y + (rowh - 9) / 2, buf, st->text_dim);
        if (pos >= 0) {
            char pc[2] = { (char)('A' + (pat & 3)), 0 };
            ly_chip(r.x + 21, y, rowh - 2, st_pattern_color(pat, 1));
            ui_text(r.x + 23, y + (rowh - 9) / 2, pc, PAL_WHITE);
        }
        if (lw[0]) {
            draw_syllables(r.x + 35, y + (rowh - 9) / 2, lw, sts, sy, st->text,
                           PAL_PAINT(PAINT_GRAPE, 0), maxch, pl == i ? song_pos_syllable() : -1);
        } else {
            ui_text(r.x + 35, y + (rowh - 9) / 2, pos >= 0 ? "~ instrumental ~" : "~ not in the song ~",
                    st->text_dim);
        }
        /* fit badge */
        if (lw[0] && sung && on >= 0) {
            int bx = r.x + r.w - 22, by = y + rowh / 2 - 1;
            if (sy == on) {
                ui_heart(bx + 6, by, 3 + (sel && ((studio.t >> 3) & 1)), PAL_PAINT(PAINT_ROSE, 1));
            } else {
                doomo_snprintf(buf, sizeof(buf), "%c%d", sy > on ? '+' : '-', sy > on ? sy - on : on - sy);
                ly_badge(bx - 2, by - 4, sy > on ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_SKY, 1));
                ui_text(bx, by - 4, buf, PAL_WHITE);
            }
        }
    }
    /* detail: syllables under the onsets, and who sings */
    if (dh >= 30) {
        int dy = r.y + 8 * rowh + 10, line = studio.lsel, pos = studio_line_pos(s, line);
        int gx = r.x + 6, gw = r.w - 12, k, nsy;
        const uint8_t *stt = sel_st;
        st_panel(r.x, dy, r.w, dh, WIN_MINT);
        studio_part_label(s, line, buf, sizeof(buf));
        ui_text(gx + 2, dy + 4, STR_ICON_ARROW_L, ui_style_text(WIN_MINT));
        ui_text(gx + 10, dy + 4, buf, ui_style_text(WIN_MINT));
        ui_text(gx + 12 + ui_text_width(buf), dy + 4, STR_ICON_ARROW_R, ui_style_text(WIN_MINT));
        nsy = sel_n;
        if (pos >= 0 && st_line_text(s, line)[0]) {
            const char *ltx = st_line_text(s, line);
            const uint8_t *lead = s->notes[studio_line_pat(s, line) & 3][0];
            int sx = gw / SONG_STEPS, ky = dy + 17, syl = 0, lastx = -100;
            uint32_t sbits[2] = { 0, 0 };                       /* the strip's 2-bit step states (ly_strip_half) */
            for (k = 0; k < SONG_STEPS; k++) {
                int x = gx + k * sx;
                if (lead[k] >= 2) {
                    int ok = syl < nsy;
                    sbits[k >> 4] |= (uint32_t)(ok ? 1 : 2) << (2 * (k & 15));
                    if (ok && x >= lastx + 2 && dh >= 34) {
                        char sb[8];
                        int a = stt[syl], b = syl + 1 < nsy ? stt[syl + 1] : (int)strlen(ltx), j;
                        for (j = 0; j < 6 && a + j < b && ltx[a + j] != ' '; j++) sb[j] = ltx[a + j];
                        sb[j] = 0;
                        if (j > (wide ? 4 : 3)) { sb[wide ? 4 : 3] = 0; j = wide ? 4 : 3; }
                        ui_text(x, ky + 8 + ((syl & 1) ? 8 : 0), sb, (syl & 1) ? PAL_PAINT(PAINT_GRAPE, 0)
                                                                         : ui_style_text(WIN_MINT));
                        lastx = x + j * 6 - 6;
                    }
                    syl++;
                } else if (lead[k] == SONG_HOLD) {
                    sbits[k >> 4] |= 3u << (2 * (k & 15));
                }
            }
            ly_strip(gx, ky, sx, sbits);
            /* who sings it (room permitting) */
            if (dh >= 70) {
                uint32_t m = st_line_mask(s, line), sing = song_pos_line() == line ? song_pos_singers() : 0;
                int k2, cxp = gx + 4, bob;
                for (k2 = 0; k2 < SONG_SINGERS && cxp < gx + gw - 16; k2++) {
                    if (!((m >> k2) & 1)) continue;
                    bob = ((sing >> k2) & 1) ? ((studio.t >> 2) & 1) : 0;
                    st_chibi_ui(song_singer_id(s, k2), cxp, dy + dh - 28 - bob, 0, 0, 0, 0);
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
#define CHOIR_ROW SONG_SINGERS                       /* csel value of the stage row */
/* the grid is 3 rows: 25 -> 9/8/8, 24 -> 8/8/8, 16 -> 6/5/5 */
#define CH_R1 (SONG_SINGERS / 3)                     /* slots in rows 1 and 2 */
#define CH_R0 (SONG_SINGERS - 2 * CH_R1)             /* slots in row 0 */
#define CH_S1 CH_R0                                  /* first slot of row 1 */
#define CH_S2 (CH_R0 + CH_R1)                        /* first slot of row 2 */

static void slot_xy(StRect r, int i, int *x, int *y, int *tw, int *th)
{
    int wide = st_wide(), row = i < CH_S1 ? 0 : i < CH_S2 ? 1 : 2, col = i < CH_S1 ? i : i < CH_S2 ? i - CH_S1 : i - CH_S2;
    int cw = wide ? 33 : 25, ch = wide ? 40 : 31, n = row == 0 ? CH_R0 : CH_R1;
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
    st_xmasks(XM_TOUCH, 0, NULL);
    SS->singer[slot] = SONG_NO_SINGER;
    for (i = 0; i < SONG_LYRIC_LINES; i++) SS->line_singers[i] &= ~(1u << slot);
    st_xmasks(XM_DROP, slot, NULL);
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
    st_xmasks(XM_TOUCH, 0, NULL);
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
    st_xmasks(XM_REMAP, 0, map);
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
    if (c == 5) { ed_touch(SS->line_singers, (int)sizeof(SS->line_singers)); st_xmasks(XM_TOUCH, 0, NULL); studio_auto_parts(SS); ed_commit(); st_msg("Parts shared out"); return; }
    if (c == 6) { sort_by_section(); st_msg("Sorted: S A T B"); return; }
    if (c == 8 || c == 9) {                      /* ST-TOOLS: auto-harmony from the lead */
        int n = st_harmonize_song(c - 8);
        if (n) { st_msg(c == 8 ? "Harmony: thirds under the lead" : "Harmony: fifths under the lead"); audio_sfx(SFX_SPARKLE); }
        else { st_msg("Write a lead melody first"); audio_sfx(SFX_ERROR); }
        return;
    }
    ed_touch(SS->singer, (int)sizeof(SS->singer));
    ed_touch(SS->section, (int)sizeof(SS->section));
    ed_touch(SS->line_singers, (int)sizeof(SS->line_singers));
    st_xmasks(XM_TOUCH, 0, NULL);
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
        int row = s < CH_S1 ? 0 : s < CH_S2 ? 1 : 2, col = s < CH_S1 ? s : s < CH_S2 ? s - CH_S1 : s - CH_S2;
        if (btn_repeat(BTN_LEFT)) s = s > 0 ? s - 1 : SONG_SINGERS - 1;
        if (btn_repeat(BTN_RIGHT)) s = s < SONG_SINGERS - 1 ? s + 1 : 0;
        if (btn_repeat(BTN_UP)) {
            if (row == 0) s = CHOIR_ROW;
            else s = row == 1 ? DOOMO_MIN(col, CH_R0 - 1) : CH_S1 + DOOMO_MIN(col, CH_R1 - 1);
        }
        if (btn_repeat(BTN_DOWN) && row < 2) s = row == 0 ? CH_S1 + DOOMO_MIN(col, CH_R1 - 1) : CH_S2 + col;
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
    }
    if (studio_sel_tap) {
        static const char *const items[10] = { "Friends of this singer", "Hillvile choir",
                                               "Hellvile choir", "Both districts!", "Random choir",
                                               "Share out parts", "Sort by section",
                                               "Clear the choir", "Harmony: thirds", "Harmony: fifths" };
        static const char *const help[10] = { "Singers who are their friends.", "Everyone from Hillvile.",
                                              "Everyone from Hellvile.", "Both districts sing!", "Pick singers by chance.",
                                              "Share the lines out.", "Order by voice part.", "Send everyone home.",
                                              "A third under the lead.", "A fifth under the lead." };
        st_popup("Quick fill", items, 10, 0, fill_menu);
        st_popup_help(help);
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

/* The section badge's rounded body: the same rects as before, drawn straight or, on the GBA, as one small baked image
 * (st_bake, studio.c). A whole singer's tile is NOT baked: on the GBA a 23 x 28 opaque blit costs more than the
 * tile's 7 rect fills (measured: +4.3 ms for 25 tiles). */

static void badge_shape(int x, int y, const int *a)
{
    ui_round_rect(x, y, 7, 8, 2, (uint8_t)a[0]);
}

static void badge_draw(int x, int y, uint8_t fill)
{
    int a[1];
    a[0] = fill;
    if (st_bake(0x31u, fill, x, y, badge_shape, a, 64)) return;
    badge_shape(x, y, a);
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
    doomo_snprintf(buf, sizeof(buf), "%d/%d", n, SONG_SINGERS);
    ui_text(r.x + r.w - 6 - ui_text_width(buf), r.y + 5, buf, st->text_dim);
    for (i = 0; i < SONG_SINGERS; i++) {
        int x, y, tw, th, id = song_singer_id(s, i), sel = i == studio.csel;
        slot_xy(r, i, &x, &y, &tw, &th);
        if (id >= 0) {
            int sec = song_singer_section(s, i), bop = ((singing >> i) & 1) ? ((studio.t >> 2) & 1) : 0;
            char sl[2] = { section_letter[sec], 0 };
            ui_round_rect(x, y, tw, th, 4, PAL_PAINT(section_paint[sec], sel ? 1 : 2));
            if (sel) ui_frame(x - 1, y - 1, tw + 2, th + 2,
                              (singing >> i) & 1 ? PAL_PAINT(section_paint[sec], 0)
                                                : PAL_PAINT(PAINT_ROSE, 0));
            st_chibi_ui(id, x + (tw - BODYSPR_W) / 2, y + th - BODYSPR_H - 1 - bop, 0,
                     sel ? (int)(studio.t / 8) & 3 : 0, 0, 0);
            badge_draw(x + tw - 8, y + 1, PAL_PAINT(section_paint[sec], 0));
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
                const DoomoImage *img = portrait_image_ui(id, (singing >> studio.csel) & 1 ? EXPR_TALK : EXPR_HAPPY);
                if (img) plat_image(r.x + r.w - 56, iy - 2, img, 1, 0, 0);
                doomo_snprintf(buf, sizeof(buf), "Loves %s, fears %s",
                               doomo_music_names[RES_LIKE_MUSIC(res) % MUSIC_COUNT],
                               doomo_fear_names[RES_FEAR(res) % FEAR_COUNT]);
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
