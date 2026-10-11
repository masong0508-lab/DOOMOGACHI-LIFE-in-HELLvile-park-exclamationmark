/* studio_edit.c - the Music Studio's SONG, NOTES and ORDER tabs (owner:
 * studio).
 *
 * NOTES is a 4-track x 32-step tracker for the current pattern:
 *   D-pad      move (steps scroll)            L / R    pattern A..D
 *   A          place the last note (or pick up the note under the cursor)
 *   hold A +   UP / DOWN  one scale step (drums: next / previous drum)
 *              L / R      an octave down / up
 *   B          erase a note; on a rest after a note: a tie (hold)
 *   START      play / stop the pattern from the cursor
 *   SELECT     tools: copy / paste pattern or track, clear, nudge
 * Every new note is snapped to the song's key and scale, previewed on its
 * channel and lights up the mini keyboard.
 */
#include "studio_int.h"

#define KEY(tab, row) ((tab) * 256 + (row))

/* ====================================================================== */
/* hints                                                                    */
const char *tab_hint(int tab)
{
    switch (tab) {
    case STAB_SONG:
        return STR_ICON_ARROW_L STR_ICON_ARROW_R "Change " STR_ICON_A "Edit START:Play SEL:Tools";
    case STAB_NOTES:
        if (studio.a_edit) return "Hold " STR_ICON_A ": " STR_ICON_ARROW_U STR_ICON_ARROW_D "Pitch  L/R:Octave";
        return STR_ICON_A "Note " STR_ICON_B "Erase/Tie L/R:Pattern START:Play";
    case STAB_ORDER:
        return STR_ICON_ARROW_L STR_ICON_ARROW_R "Pattern " STR_ICON_A "Insert START:Play SEL:Tools";
    case STAB_LYRICS:
        return STR_ICON_A "Write " STR_ICON_ARROW_L STR_ICON_ARROW_R "Who sings START:Fit SEL:Sing";
    case STAB_CHOIR:
        return STR_ICON_A "Pick START:Section SEL:Quick fill " STR_ICON_B "Back";
    case STAB_STAGE:
        if (stage_fullscreen()) return STR_ICON_B "Stop the show";
        return STR_ICON_A "Perform! SEL:Concert Hall " STR_ICON_B "Back";
    default:
        return STR_ICON_A "Choose L/R:Tabs SEL+" STR_ICON_B ":Undo " STR_ICON_B "Back";
    }
}

/* ====================================================================== */
/* SONG                                                                     */
enum { SR_TITLE, SR_GENRE, SR_TEMPO, SR_KEY, SR_SCALE, SR_SWING, SR_LOOP,
       SR_WAVE0, SR_VOL0, SR_WAVE1, SR_VOL1, SR_WAVE2, SR_VOL2, SR_KIT, SR_VOL3,
       SR_COUNT };
static const char *const track_names[SONG_TRACKS] = { "Lead", "Harmony", "Bass", "Drums" };

static void title_done(int ok)
{
    if (!ok) return;
    studio_clean_text(studio.edit, SONG_TITLE_LEN);
    ed_touch(SS->title, SONG_TITLE_LEN);
    memset(SS->title, 0, SONG_TITLE_LEN);
    memcpy(SS->title, studio.edit, strlen(studio.edit) < SONG_TITLE_LEN ? strlen(studio.edit)
                                                                       : SONG_TITLE_LEN - 1);
    if (!SS->title[0]) memcpy(SS->title, "Untitled", 9);
    ed_commit();
    audio_sfx(SFX_HAPPY);
}

static int next_wave(int w, int d, int drums)
{
    int k;
    for (k = 0; k < SONG_WAVES; k++) {
        w = (w + d + SONG_WAVES) % SONG_WAVES;
        if (drums ? studio_kit_ok(w) : studio_wave_ok(w)) return w;
    }
    return w;
}

static void song_value(int row, int d)
{
    DoomoSong *s = SS;
    int t;
    switch (row) {
    case SR_GENRE:
        ed_touch_key(KEY(0, row), &s->genre, 1);
        s->genre = (uint8_t)((s->genre + d + MUSIC_COUNT) % MUSIC_COUNT);
        break;
    case SR_TEMPO:
        ed_touch_key(KEY(0, row), &s->bpm, 1);
        s->bpm = (uint8_t)DOOMO_CLAMP(s->bpm + d * (btn_held_ticks(BTN_LEFT | BTN_RIGHT) > 30 ? 5 : 1),
                                      60, 220);
        break;
    case SR_KEY: {
        /* the notes move with the key */
        int p, k, i, step = d > 0 ? 1 : -1;
        ed_touch(&s->key, 1);
        ed_touch(s->notes, (int)sizeof(s->notes));
        s->key = (uint8_t)((s->key + 12 + step) % 12);
        for (p = 0; p < SONG_PATTERNS; p++)
            for (k = 0; k < 3; k++)
                for (i = 0; i < SONG_STEPS; i++)
                    if (s->notes[p][k][i] >= 2)
                        s->notes[p][k][i] = (uint8_t)DOOMO_CLAMP(s->notes[p][k][i] + step, 2, 127);
        ed_commit();
        return; }
    case SR_SCALE:
        ed_touch_key(KEY(0, row), &s->scale, 1);
        s->scale = (uint8_t)((s->scale + d + SONG_SCALE_COUNT) % SONG_SCALE_COUNT);
        break;
    case SR_SWING:
        ed_touch_key(KEY(0, row), &s->swing, 1);
        s->swing = (uint8_t)DOOMO_CLAMP(s->swing + d, 0, 8);
        break;
    case SR_LOOP:
        ed_touch(&s->flags, 1);
        s->flags ^= SONGF_LOOP;
        break;
    case SR_WAVE0: case SR_WAVE1: case SR_WAVE2: case SR_KIT:
        t = (row - SR_WAVE0) / 2;
        ed_touch_key(KEY(0, row), &s->wave[t], 1);
        s->wave[t] = (uint8_t)next_wave(s->wave[t], d, t == 3);
        song_preview(s, t, t == 3 ? SONG_DRUM_SNARE : studio.last[t]);
        break;
    case SR_VOL0: case SR_VOL1: case SR_VOL2: case SR_VOL3:
        t = (row - SR_VOL0) / 2;
        ed_touch_key(KEY(0, row), &s->vol[t], 1);
        s->vol[t] = (uint8_t)DOOMO_CLAMP(s->vol[t] + d, 0, 15);
        song_preview(s, t, t == 3 ? SONG_DRUM_KICK : studio.last[t]);
        break;
    default:
        return;
    }
    ed_commit_key(KEY(0, row));
    audio_sfx(SFX_CURSOR);
}

static void song_tools(int c)
{
    DoomoSong *s = SS;
    switch (c) {
    case 0: {                                   /* re-compose, keep the words */
        uint32_t seed = doomo_hash32(studio.t + s->seed * 7u + 1u);
        ed_touch(s, (int)sizeof(*s));
        studio_compose(s, s->genre, seed);
        ed_commit();
        st_msg("Fresh tune in %s!", doomo_music_names[s->genre]);
        break; }
    case 1: case 2: {                           /* transpose */
        int d = c == 1 ? 1 : -1;
        song_value(SR_KEY, d);
        st_msg("Key of %s", doomo_song_key_names[s->key]);
        break; }
    case 3: {                                   /* snap every note to the scale */
        int p, t, i, n = 0;
        ed_touch(s->notes, (int)sizeof(s->notes));
        for (p = 0; p < SONG_PATTERNS; p++)
            for (t = 0; t < 3; t++)
                for (i = 0; i < SONG_STEPS; i++)
                    if (s->notes[p][t][i] >= 2) {
                        int v = song_scale_snap(s, s->notes[p][t][i]);
                        n += v != s->notes[p][t][i];
                        s->notes[p][t][i] = (uint8_t)v;
                    }
        ed_commit();
        st_msg("%d notes snapped to %s", n, doomo_song_scale_names[s->scale]);
        break; }
    default: break;
    }
}

void tab_song_update(void)
{
    int vis = (st_content().h - 10) / 12;
    if (btn_repeat(BTN_UP)) { studio.srow = (uint8_t)((studio.srow + SR_COUNT - 1) % SR_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.srow = (uint8_t)((studio.srow + 1) % SR_COUNT); audio_sfx(SFX_CURSOR); }
    if (studio.srow < studio.stop) studio.stop = studio.srow;
    if (studio.srow >= studio.stop + vis) studio.stop = (uint8_t)(studio.srow - vis + 1);
    if (btn_repeat(BTN_LEFT)) song_value(studio.srow, -1);
    if (btn_repeat(BTN_RIGHT)) song_value(studio.srow, 1);
    if (btn_pressed(BTN_A)) {
        if (studio.srow == SR_TITLE) {
            memcpy(studio.edit, SS->title, SONG_TITLE_LEN);
            studio.edit[SONG_TITLE_LEN - 1] = 0;
            ui_text_entry("Song title", studio.edit, SONG_TITLE_LEN, title_done);
        } else {
            song_value(studio.srow, 1);
        }
    }
    if (btn_pressed(BTN_START)) st_play_from(0);
    if (studio_sel_tap) {
        static const char *const items[4] = { "Re-compose music", "Key up", "Key down",
                                              "Snap notes to scale" };
        st_popup("Song tools", items, 4, 0, song_tools);
    }
}

static void song_row_value(int row, char *buf, int cap)
{
    const DoomoSong *s = SS;
    int t;
    switch (row) {
    case SR_TITLE: doomo_snprintf(buf, cap, "%s", s->title); break;
    case SR_GENRE: doomo_snprintf(buf, cap, "%s", doomo_music_names[s->genre % MUSIC_COUNT]); break;
    case SR_TEMPO: doomo_snprintf(buf, cap, "%d bpm", s->bpm); break;
    case SR_KEY: doomo_snprintf(buf, cap, "%s", doomo_song_key_names[s->key % 12]); break;
    case SR_SCALE: doomo_snprintf(buf, cap, "%s", doomo_song_scale_names[s->scale % SONG_SCALE_COUNT]); break;
    case SR_SWING: doomo_snprintf(buf, cap, s->swing ? "%d" : "Straight", s->swing); break;
    case SR_LOOP: doomo_snprintf(buf, cap, "%s", (s->flags & SONGF_LOOP) ? "Loop" : "Once"); break;
    case SR_WAVE0: case SR_WAVE1: case SR_WAVE2:
        t = (row - SR_WAVE0) / 2;
        doomo_snprintf(buf, cap, "%s", doomo_song_wave_names[s->wave[t] % SONG_WAVES]);
        break;
    case SR_KIT: doomo_snprintf(buf, cap, "%s", doomo_song_kit_names[s->wave[3] % SONG_KIT_COUNT]); break;
    default:
        t = (row - SR_VOL0) / 2;
        doomo_snprintf(buf, cap, "%d", s->vol[t]);
        break;
    }
}

static void song_row_label(int row, char *buf, int cap)
{
    static const char *const fixed[SR_WAVE0] = { "Title", "Genre", "Tempo", "Key", "Scale",
                                                 "Swing", "Repeat" };
    if (row < SR_WAVE0) { doomo_snprintf(buf, cap, "%s", fixed[row]); return; }
    if (row == SR_KIT) { doomo_snprintf(buf, cap, "Drum kit"); return; }
    doomo_snprintf(buf, cap, "%s %s", track_names[(row - SR_WAVE0) / 2],
                   ((row - SR_WAVE0) & 1) ? "vol" : "sound");
}

/* a spinning record with the title on its label */
static void draw_record(int cx, int cy, int r, int t, int playing)
{
    int a = playing ? t * 3 : t / 2;
    ui_fill_circle(cx + 2, cy + 3, r, PAL_PAINT(PAINT_GRAPE, 0));
    ui_fill_circle(cx, cy, r, PAL_PAINT(PAINT_JET, 1));
    ui_fill_circle(cx, cy, r - 3, PAL_PAINT(PAINT_CHARCOAL, 0));
    ui_fill_circle(cx, cy, r - 5, PAL_PAINT(PAINT_JET, 1));
    ui_fill_circle(cx, cy, r / 2 - 1, st_pattern_color(SS->genre & 3, 1));
    ui_fill_circle(cx, cy, r / 2 - 3, st_pattern_color(SS->genre & 3, 2));
    ui_fill_circle(cx, cy, 1, PAL_PAINT(PAINT_JET, 1));
    /* the shine sweeps around */
    {
        int dx = ui_wave(a, 120, r - 3), dy = ui_wave(a + 30, 120, r - 3);
        plat_rect(cx + dx, cy + dy, 2, 1, PAL_GRAY_L);
        plat_rect(cx - dx / 2, cy - dy / 2, 1, 1, PAL_GRAY);
    }
}

void tab_song_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int wide = st_wide(), lw = wide ? 182 : 146, vis = (r.h - 10) / 12, i;
    int cx = r.x + lw + 4, cw = r.w - lw - 4;
    char lab[24], val[24];
    st_panel(r.x, r.y, lw, r.h, WIN_CREAM);
    for (i = 0; i < vis && studio.stop + i < SR_COUNT; i++) {
        int row = studio.stop + i, y = r.y + 5 + i * 12, sel = row == studio.srow;
        song_row_label(row, lab, sizeof(lab));
        song_row_value(row, val, sizeof(val));
        if (row >= SR_WAVE0) {
            int t = (row - SR_WAVE0) / 2;
            plat_rect(r.x + 4, y + 1, 2, 8, st_track_color(t, 1));
        }
        ui_value_row(r.x + 8, y, lw - 14, lab, val, sel);
        /* locked sounds are greyed with a cross */
        if ((row == SR_WAVE0 || row == SR_WAVE1 || row == SR_WAVE2) &&
            !studio_wave_ok(SS->wave[(row - SR_WAVE0) / 2]))
            ui_text(r.x + lw - 14, y + 1, STR_ICON_X, st->text_dim);
    }
    ui_scroll_arrows(r.x + lw / 2, r.y + 1, r.y + r.h - 3, studio.stop > 0,
                     studio.stop + vis < SR_COUNT);
    /* the song card */
    if (cw >= 70) {
        int y = r.y, t = (int)studio.t, playing = song_playing();
        int rr = wide ? 30 : 22, rcx = cx + cw / 2, rcy = y + rr + 22;
        uint32_t len = song_length_ticks(SS) / DOOMO_TICK_HZ;
        st_panel(cx, y, cw, r.h, WIN_MINT);
        ui_text_center(rcx, y + 5, SS->title, ui_style_text(WIN_MINT));
        draw_record(rcx, rcy, rr, t, playing);
        /* the tone arm */
        plat_line(cx + cw - 8, y + 18, rcx + rr / 2, rcy - rr / 3, PAL_GRAY_D);
        ui_fill_circle(cx + cw - 8, y + 18, 2, PAL_GRAY);
        y = rcy + rr + 4;
        ui_text_center(rcx, y, doomo_music_names[SS->genre % MUSIC_COUNT], st->text);
        doomo_snprintf(val, sizeof(val), "%s %s", doomo_song_key_names[SS->key % 12],
                       doomo_song_scale_names[SS->scale % SONG_SCALE_COUNT]);
        ui_text_center(rcx, y + 10, val, st->text_dim);
        doomo_snprintf(val, sizeof(val), "%u:%02u", (unsigned)(len / 60), (unsigned)(len % 60));
        ui_text_center(rcx, y + 20, val, st->text_dim);
        if (y + 40 < r.y + r.h) {
            int n = song_singer_count(SS);
            doomo_snprintf(val, sizeof(val), STR_ICON_NOTE "%d singer%s", n, n == 1 ? "" : "s");
            ui_text_center(rcx, y + 31, val, st->text_dim);
        }
        if (playing) {
            int k;
            for (k = 0; k < 3; k++)
                st_note_glyph(rcx + rr - 4 + k * 6, rcy - rr - 2 - ((t / 3 + k * 7) % 16),
                              PAL_PAINT(k == 1 ? PAINT_ROSE : PAINT_LAVENDER, 1));
        }
    }
}

/* ====================================================================== */
/* NOTES                                                                    */
static uint8_t *cell(int p, int t, int s) { return &SS->notes[p & 3][t & 3][s & 31]; }

static void preview_cell(int t, int v)
{
    if (song_playing()) return;
    song_preview(SS, t, v);
    if (t < 3 && v >= 2) { studio.kb_note = (uint8_t)v; studio.kb_flash = 10; }
}

static void notes_tools(int c)
{
    DoomoSong *s = SS;
    int p = studio.pat, t = studio.trk, i;
    switch (c) {
    case 0: studio.clip_pat = (int8_t)p; studio.clip_trk = -1; st_msg("Pattern %c copied", 'A' + p); break;
    case 1:
        if (studio.clip_pat < 0) { st_msg("Copy something first"); break; }
        if (studio.clip_trk >= 0) {
            st_msg("That's a track - paste track");
            break;
        }
        if (studio.clip_pat == p) { st_msg("It's the same pattern!"); break; }
        ed_touch(s->notes[p], (int)sizeof(s->notes[p]));
        memcpy(s->notes[p], s->notes[studio.clip_pat], sizeof(s->notes[p]));
        ed_commit();
        st_msg("Pasted %c into %c", 'A' + studio.clip_pat, 'A' + p);
        break;
    case 2: studio.clip_pat = (int8_t)p; studio.clip_trk = (int8_t)t; st_msg("%s %c copied", track_names[t], 'A' + p); break;
    case 3:
        if (studio.clip_pat < 0 || studio.clip_trk < 0) { st_msg("Copy a track first"); break; }
        if ((studio.clip_trk == 3) != (t == 3)) { st_msg("Drums and notes don't mix!"); break; }
        if (studio.clip_pat == p && studio.clip_trk == t) { st_msg("It's the same track!"); break; }
        ed_touch(s->notes[p][t], SONG_STEPS);
        memcpy(s->notes[p][t], s->notes[studio.clip_pat][studio.clip_trk], SONG_STEPS);
        ed_commit();
        st_msg("Track pasted");
        break;
    case 4:
        ed_touch(s->notes[p][t], SONG_STEPS);
        memset(s->notes[p][t], 0, SONG_STEPS);
        ed_commit();
        st_msg("%s cleared", track_names[t]);
        break;
    case 5: case 6:
        if (t == 3) { st_msg("Drums have no pitch"); break; }
        ed_touch(s->notes[p][t], SONG_STEPS);
        for (i = 0; i < SONG_STEPS; i++)
            if (s->notes[p][t][i] >= 2)
                s->notes[p][t][i] = (uint8_t)song_scale_shift(s, s->notes[p][t][i], c == 5 ? 1 : -1);
        ed_commit();
        break;
    default: break;
    }
}

void tab_notes_update(void)
{
    DoomoSong *s = SS;
    int t = studio.trk, vis;
    uint8_t *c = cell(studio.pat, t, studio.stp);
    {
        StRect r = st_content();
        vis = (r.h - 46) / 9;
        if (vis < 4) vis = 4;
    }
    if (studio.kb_flash) studio.kb_flash--;
    if (studio.a_edit) {
        int v = *c, nv = v;
        if (!btn_down(BTN_A)) {
            studio.a_edit = 0;
            ed_commit();
            studio.stp = (uint8_t)((studio.stp + 1) % SONG_STEPS);
        } else if (t == 3) {
            if (btn_repeat(BTN_UP)) nv = v % (SONG_DRUM_COUNT - 1) + 1;
            if (btn_repeat(BTN_DOWN)) nv = (v + SONG_DRUM_COUNT - 3) % (SONG_DRUM_COUNT - 1) + 1;
        } else {
            if (btn_repeat(BTN_UP)) nv = song_scale_shift(s, v, 1);
            if (btn_repeat(BTN_DOWN)) nv = song_scale_shift(s, v, -1);
            if (btn_repeat(BTN_L)) nv = v - 12 >= SONG_NOTE_MIN ? v - 12 : v;
            if (btn_repeat(BTN_R)) nv = v + 12 <= SONG_NOTE_MAX ? v + 12 : v;
            nv = DOOMO_CLAMP(nv, SONG_NOTE_MIN, SONG_NOTE_MAX);
        }
        if (nv != v && studio.a_edit) {
            *c = (uint8_t)nv;
            studio.last[t] = (uint8_t)nv;
            preview_cell(t, nv);
        }
    } else {
        if (btn_repeat(BTN_UP)) studio.stp = (uint8_t)((studio.stp + SONG_STEPS - 1) % SONG_STEPS);
        if (btn_repeat(BTN_DOWN)) studio.stp = (uint8_t)((studio.stp + 1) % SONG_STEPS);
        if (btn_repeat(BTN_LEFT)) studio.trk = (uint8_t)((studio.trk + 3) % 4);
        if (btn_repeat(BTN_RIGHT)) studio.trk = (uint8_t)((studio.trk + 1) % 4);
        if (btn_pressed(BTN_L)) { studio.pat = (uint8_t)((studio.pat + 3) % 4); audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_R)) { studio.pat = (uint8_t)((studio.pat + 1) % 4); audio_sfx(SFX_CURSOR); }
        t = studio.trk;
        c = cell(studio.pat, t, studio.stp);
        if (btn_pressed(BTN_A)) {
            ed_touch(c, 1);
            if (t == 3) {
                if (!*c) *c = studio.last[3] ? studio.last[3] : SONG_DRUM_KICK;
                studio.last[3] = *c;
            } else if (*c >= 2) {
                studio.last[t] = *c;                      /* pick it up */
            } else {
                *c = (uint8_t)song_scale_snap(s, studio.last[t] >= 2 ? studio.last[t] : 72);
                studio.last[t] = *c;
            }
            studio.a_edit = 1;
            preview_cell(t, *c);
        } else if (btn_pressed(BTN_B)) {
            int above = studio.stp ? *cell(studio.pat, t, studio.stp - 1) : 0;
            ed_touch(c, 1);
            if (t == 3) *c = 0;
            else if (*c == SONG_REST && above >= 1) *c = SONG_HOLD;    /* tie */
            else *c = SONG_REST;
            ed_commit();
            audio_sfx(SFX_BLIP);
        }
        if (btn_pressed(BTN_START)) {
            if (song_playing()) song_stop();
            else song_play_pattern(s, studio.pat, studio.stp);
        }
        if (studio_sel_tap) {
            static const char *const items[7] = { "Copy pattern", "Paste pattern", "Copy track",
                                                  "Paste track", "Clear track", "Track up a step",
                                                  "Track down a step" };
            st_popup("Pattern tools", items, 7, 0, notes_tools);
        }
    }
    if (studio.stp < studio.ntop) studio.ntop = studio.stp;
    if (studio.stp >= studio.ntop + vis) studio.ntop = (uint8_t)(studio.stp - vis + 1);
}

static const char *const drum_short[SONG_DRUM_COUNT] = {
    "", "Kik", "Snr", "Hat", "Opn", "Clp", "Tom", "Crs", "Cow"
};

static void mini_keyboard(int x, int y, int w, int note)
{
    static const int8_t white_of[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const uint8_t black_semis[5] = { 1, 3, 6, 8, 10 };
    static const uint8_t black_after[5] = { 0, 1, 3, 4, 5 };
    int kw = w >= 160 ? 8 : 6, oct0, o, k, base, flash = studio.kb_flash;
    const DoomoSong *s = SS;
    if (note < 2) note = studio.last[studio.trk < 3 ? studio.trk : 0];
    if (note < 2) note = 72;
    oct0 = note / 12;
    if (note % 12 < 6 && oct0 > 1) oct0--;
    base = oct0 * 12;
    plat_rect(x - 1, y - 1, kw * 14 + 2, 16, PAL_PAINT(PAINT_GRAPE, 0));
    for (o = 0; o < 2; o++)
        for (k = 0; k < 12; k++) {
            int wi = white_of[k], m = base + o * 12 + k, kx;
            if (wi < 0) continue;
            kx = x + (o * 7 + wi) * kw;
            plat_rect(kx, y, kw - 1, 14,
                      m == note ? (flash & 2 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_ROSE, 1))
                                : PAL_WHITE);
            plat_rect(kx, y + 12, kw - 1, 2, m == note ? PAL_PAINT(PAINT_ROSE, 0) : PAL_GRAY_L);
            if (song_scale_snap(s, m) == m) plat_rect(kx + kw / 2 - 1, y + 9, 2, 2, PAL_PAINT(PAINT_MINT, 0));
        }
    for (o = 0; o < 2; o++)
        for (k = 0; k < 5; k++) {
            int m = base + o * 12 + black_semis[k];
            int kx = x + (o * 7 + black_after[k] + 1) * kw - kw / 3 - 1;
            plat_rect(kx, y, kw / 2 + 2, 8, m == note ? PAL_PAINT(PAINT_ROSE, 0) : PAL_PAINT(PAINT_JET, 1));
            if (song_scale_snap(s, m) == m) plat_rect(kx + 1, y + 5, 2, 1, PAL_PAINT(PAINT_MINT, 1));
        }
    {
        char nm[6];
        song_note_name(note, nm);
        ui_text_outline(x + kw * 14 + 6, y + 3, nm, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 0));
    }
}

static void pattern_pills(int x, int y)
{
    int p, b = ui_wave((int)studio.t, 20, 1) > 0;
    for (p = 0; p < SONG_PATTERNS; p++) {
        char nm[2] = { (char)('A' + p), 0 };
        int sel = p == studio.pat, px = x + p * 18, py = y - (sel ? 1 + b : 0);
        ui_round_rect(px, py, 16, 12, 4, st_pattern_color(p, sel ? 0 : 1));
        ui_round_rect(px + 1, py + 1, 14, 10, 3, st_pattern_color(p, sel ? 1 : 2));
        ui_text(px + 5, py + 2, nm, sel ? PAL_WHITE : st_pattern_color(p, 0));
    }
}

void tab_notes_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoSong *s = SS;
    int wide = st_wide(), colw = (r.w - 24) / 4, gx = r.x + 20, gy = r.y + 26;
    int vis = (r.h - 46) / 9, i, t, playing_here, ps = song_pos_step();
    char buf[24];
    if (vis < 4) vis = 4;
    playing_here = song_playing() && (song_pos_order() < 0 ? 0 : 1);
    /* header: patterns, where this one plays */
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    pattern_pills(r.x + 6, r.y + 4);
    {
        int k, n = 0, x = r.x + 84;
        buf[0] = 0;
        for (k = 0; k < s->order_len && n < 6; k++)
            if (s->order[k] == studio.pat) {
                char one[5];
                doomo_snprintf(one, sizeof(one), n ? ",%d" : "%d", k + 1);
                if ((int)strlen(buf) + 4 < (int)sizeof(buf)) memcpy(buf + strlen(buf), one, strlen(one) + 1);
                n++;
            }
        ui_text(x, r.y + 6, n ? "plays at" : "not in the song", st->text_dim);
        if (n) ui_text(x + 54, r.y + 6, buf, st_pattern_color(studio.pat, 0));
    }
    /* track headers */
    for (t = 0; t < 4; t++) {
        int x = gx + t * colw;
        ui_round_rect(x + 1, r.y + 16, colw - 2, 9, 3, st_track_color(t, t == studio.trk ? 1 : 2));
        ui_text(x + 4, r.y + 17, track_names[t], t == studio.trk ? PAL_WHITE : st_track_color(t, 0));
    }
    /* the grid */
    for (i = 0; i < vis && studio.ntop + i < SONG_STEPS; i++) {
        int stp = studio.ntop + i, y = gy + i * 9;
        int head = playing_here && ps == stp;
        uint8_t rowbg = (stp & 3) == 0 ? st->fill_lo : st->fill;
        plat_rect(gx, y, colw * 4, 9, head ? PAL_PAINT(PAINT_LEMON, 2) : rowbg);
        if ((stp & 15) == 0) plat_rect(gx, y, colw * 4, 1, st->outline);
        doomo_snprintf(buf, sizeof(buf), "%2d", stp + 1);
        ui_text(r.x + 5, y + 1, buf, (stp & 3) == 0 ? st->text : st->text_dim);
        for (t = 0; t < 4; t++) {
            int x = gx + t * colw, v = s->notes[studio.pat][t][stp];
            int cur = t == studio.trk && stp == studio.stp;
            if (cur) {
                ui_round_rect(x + 1, y, colw - 2, 9, 2, studio.a_edit ? PAL_PAINT(PAINT_ROSE, 1)
                                                                      : st_track_color(t, 1));
            }
            if (t == 3) {
                if (v > 0 && v < SONG_DRUM_COUNT) {
                    st_drum_icon(v, x + 7, y + 4, (int)studio.t);
                    if (colw > 30) ui_text(x + 14, y + 1, drum_short[v], cur ? PAL_WHITE : st->text);
                } else {
                    plat_rect(x + colw / 2, y + 4, 1, 1, st->text_dim);
                }
            } else if (v >= 2) {
                song_note_name(v, buf);
                ui_text(x + 4, y + 1, buf, cur ? PAL_WHITE : st_track_color(t, 0));
                if (colw > 40) ui_fill_circle(x + colw - 6, y + 4, 2, st_track_color(t, 1));
            } else if (v == SONG_HOLD) {
                plat_rect(x + 8, y, 2, 9, cur ? PAL_WHITE : st_track_color(t, 1));
            } else {
                plat_rect(x + colw / 2, y + 4, 1, 1, st->text_dim);
            }
        }
    }
    /* column rules */
    for (t = 1; t < 4; t++) plat_rect(gx + t * colw, gy, 1, vis * 9, st->fill_hi);
    /* the cursor heart bounces next to the cell */
    {
        int cy = gy + (studio.stp - studio.ntop) * 9;
        if (studio.stp >= studio.ntop && studio.stp < studio.ntop + vis)
            ui_heart(gx + studio.trk * colw - 1, cy + 4 + ui_wave((int)studio.t, 16, 1),
                     3, PAL_PAINT(PAINT_ROSE, 1));
    }
    ui_scroll_arrows(r.x + 10, gy, gy + vis * 9 - 4, studio.ntop > 0, studio.ntop + vis < SONG_STEPS);
    /* mini keyboard */
    {
        int v = s->notes[studio.pat][studio.trk][studio.stp];
        int ky = r.y + r.h - 18;
        if (studio.trk == 3) {
            int d = v ? v : studio.last[3];
            st_drum_icon(d, r.x + 14, ky + 7, (int)studio.t);
            ui_text(r.x + 24, ky + 3, doomo_song_drum_names[d % SONG_DRUM_COUNT], st->text);
            ui_text(r.x + (wide ? 120 : 90), ky + 3, doomo_song_kit_names[s->wave[3] % SONG_KIT_COUNT],
                    st->text_dim);
        } else {
            mini_keyboard(r.x + 8, ky, r.w, studio.kb_flash ? studio.kb_note : (v >= 2 ? v : 0));
        }
    }
}

/* ====================================================================== */
/* ORDER                                                                    */
static void order_insert(int at, int pat)
{
    DoomoSong *s = SS;
    if (s->order_len >= SONG_ORDER_MAX) { st_msg("The song is full (16)"); audio_sfx(SFX_ERROR); return; }
    ed_touch(&s->order_len, 1 + SONG_ORDER_MAX);
    memmove(s->order + at + 1, s->order + at, (size_t)(s->order_len - at));
    s->order[at] = (uint8_t)pat;
    s->order_len++;
    ed_commit();
    audio_sfx(SFX_SELECT);
}

static void order_delete(int at)
{
    DoomoSong *s = SS;
    if (s->order_len <= 1) { st_msg("A song needs one pattern"); audio_sfx(SFX_ERROR); return; }
    ed_touch(&s->order_len, 1 + SONG_ORDER_MAX);
    memmove(s->order + at, s->order + at + 1, (size_t)(s->order_len - at - 1));
    s->order_len--;
    s->order[s->order_len] = 0;
    ed_commit();
    if (studio.osel >= s->order_len) studio.osel = (uint8_t)(s->order_len - 1);
    audio_sfx(SFX_CANCEL);
}

static void order_move(int d)
{
    DoomoSong *s = SS;
    int a = studio.osel, b = a + d;
    uint8_t tmp;
    if (b < 0 || b >= s->order_len) return;
    ed_touch(s->order, SONG_ORDER_MAX);
    tmp = s->order[a]; s->order[a] = s->order[b]; s->order[b] = tmp;
    ed_commit();
    studio.osel = (uint8_t)b;
    audio_sfx(SFX_CURSOR);
}

static void order_tools(int c)
{
    switch (c) {
    case 0: order_insert(studio.osel + 1, SS->order[studio.osel]); studio.osel++; break;
    case 1: order_delete(studio.osel); break;
    case 2: order_move(-1); break;
    case 3: order_move(1); break;
    case 4:
        ed_touch(&SS->flags, 1);
        SS->flags ^= SONGF_LOOP;
        ed_commit();
        st_msg((SS->flags & SONGF_LOOP) ? "The song loops forever" : "The song plays once");
        break;
    default: break;
    }
}

void tab_order_update(void)
{
    DoomoSong *s = SS;
    int n = s->order_len, vis = (st_content().h - 40) / 12;
    if (studio.osel >= n) studio.osel = (uint8_t)(n - 1);
    if (btn_pressed(BTN_A)) studio.a_edit = 0;
    if (btn_down(BTN_A)) {
        /* hold A + up / down: carry the entry */
        if (btn_held_ticks(BTN_A) > 6) {
            if (btn_repeat(BTN_UP)) { order_move(-1); studio.a_edit = 1; }
            if (btn_repeat(BTN_DOWN)) { order_move(1); studio.a_edit = 1; }
        }
    } else {
        if (btn_repeat(BTN_UP)) { studio.osel = (uint8_t)((studio.osel + n - 1) % n); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { studio.osel = (uint8_t)((studio.osel + 1) % n); audio_sfx(SFX_CURSOR); }
    }
    if (btn_released(BTN_A) && !studio.a_edit) {
        /* a tap inserts a copy below */
        order_insert(studio.osel + 1, s->order[studio.osel]);
        if (studio.osel + 1 < s->order_len) studio.osel++;
    }
    if (btn_released(BTN_A)) studio.a_edit = 0;
    if (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT)) {
        int d = btn_repeat(BTN_LEFT) ? 3 : 1;
        ed_touch_key(KEY(2, studio.osel), &s->order[studio.osel], 1);
        s->order[studio.osel] = (uint8_t)((s->order[studio.osel] + d) % SONG_PATTERNS);
        ed_commit_key(KEY(2, studio.osel));
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_START)) st_play_from(studio.osel);
    if (studio_sel_tap) {
        static const char *const items[5] = { "Insert a copy", "Delete", "Move up", "Move down",
                                              "Repeat on/off" };
        st_popup("Arrange", items, 5, 0, order_tools);
    }
    if (vis < 3) vis = 3;
    if (studio.osel < studio.otop) studio.otop = studio.osel;
    if (studio.osel >= studio.otop + vis) studio.otop = (uint8_t)(studio.osel - vis + 1);
}

void tab_order_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoSong *s = SS;
    int vis = (r.h - 40) / 12, i, n = s->order_len, po = song_playing() ? song_pos_order() : -1;
    char buf[40];
    uint32_t len = song_length_ticks(s) / DOOMO_TICK_HZ;
    if (vis < 3) vis = 3;
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    for (i = 0; i < vis && studio.otop + i < n; i++) {
        int pos = studio.otop + i, y = r.y + 5 + i * 12, p = s->order[pos] & 3, sel = pos == studio.osel;
        const char *ly = s->lyrics[SONG_LINE_OF(pos)];
        char pl[2] = { (char)('A' + p), 0 };
        if (sel) {
            ui_round_rect(r.x + 4, y - 1, r.w - 8, 12, 4, st->accent);
            ui_cursor(r.x + 5, y + 1);
        }
        doomo_snprintf(buf, sizeof(buf), "%2d", pos + 1);
        ui_text(r.x + 16, y + 1, buf, sel ? PAL_WHITE : st->text_dim);
        ui_round_rect(r.x + 32, y, 14, 10, 3, st_pattern_color(p, 0));
        ui_round_rect(r.x + 33, y + 1, 12, 8, 2, st_pattern_color(p, pos == po ? 2 : 1));
        ui_text(r.x + 36, y + 1, pl, PAL_WHITE);
        if (pos == po) ui_heart(r.x + 54, y + 5, 3 + (song_beat_age() < 3), PAL_PAINT(PAINT_ROSE, 1));
        if (ly[0]) {
            ui_text(r.x + 62, y + 1, ly, sel ? PAL_WHITE : st->text);
        } else {
            ui_text(r.x + 62, y + 1, "~ instrumental ~", sel ? ui_lighter(st->accent) : st->text_dim);
        }
    }
    ui_scroll_arrows(r.x + r.w - 10, r.y + 3, r.y + 5 + vis * 12 - 6, studio.otop > 0,
                     studio.otop + vis < n);
    /* the timeline: one block per pass, the playhead walks it */
    {
        int ty = r.y + r.h - 30, tw = r.w - 16, bw = tw / SONG_ORDER_MAX, x0 = r.x + 8;
        for (i = 0; i < SONG_ORDER_MAX; i++) {
            int x = x0 + i * bw;
            if (i < n) {
                int p = s->order[i] & 3, lift = (i == po) ? 2 : 0;
                ui_round_rect(x, ty - lift, bw - 1, 10, 2, st_pattern_color(p, i == studio.osel ? 0 : 1));
                if (s->lyrics[SONG_LINE_OF(i)][0]) plat_rect(x + 2, ty + 7 - lift, bw - 5, 1, PAL_WHITE);
            } else {
                plat_rect(x + bw / 2 - 1, ty + 4, 2, 2, st->fill_hi);
            }
        }
        if (po >= 0) {
            int px = x0 + po * bw + (song_pos_step() * (bw - 1)) / SONG_STEPS;
            plat_rect(px, ty - 3, 1, 15, PAL_PAINT(PAINT_ROSE, 0));
        }
        doomo_snprintf(buf, sizeof(buf), "%d passes  %u:%02u  %s", n, (unsigned)(len / 60),
                       (unsigned)(len % 60), (s->flags & SONGF_LOOP) ? "(loops)" : "");
        ui_text(x0, ty + 13, buf, st->text_dim);
    }
}
