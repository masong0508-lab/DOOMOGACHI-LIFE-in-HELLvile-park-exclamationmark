/* studio_adv.c - ST-ADV: the Advanced ORDER screen (owner: studio; docs/design/STUDIO_PLAN.md 2.6, docs/STUDIO.md "ST-ADV screens").
 *
 * With the Studio level on Advanced (FILE tab), the ORDER tab ("Arrange") turns the classic 16 passes into an arrangement:
 *   ARRANGEMENT   a list of up to 64 entries; each entry is a PART (a section: Intro, Verse, Pre, Chorus, Bridge, Outro)
 *                 and a part is 1..8 passes of a pattern (A..D). A part can be used by many entries (a chorus that comes
 *                 back three times is one part): editing it changes all of them; "Make it unique" gives one entry its own copy.
 *   PART VIEW     one part's settings: its name, a key change (-12..+12 semitones), a tempo (50..200 %), a fade (none / in /
 *                 out) and its passes. The key / tempo / fade are the part's own: a last chorus a tone higher is one setting.
 * The arrangement is a SongExt (song_ext.h) kept in `studio.xw`, next to the working song, so the undo journal covers it:
 * every edit is one step (SELECT + B undoes, X redoes; nudges of one setting join into one step). It is made from the classic
 * order the first time the ORDER tab opens on Advanced and plays exactly the same until it is edited. It is saved with the
 * song's book page (songx_book_put) and comes back when the page is opened.
 *
 * EXPERT VIEW (part tools, "Expert view..."): the Free notes switch (new notes keep the pitch you last used instead of snapping to the
 *   scale), note counts per track for the whole song, and a memory meter (parts, entries, pool blocks, page bytes). Free notes is one undo step.
 *
 * Builds with no room for arrangement pages (GBA, PS1: SONGX_BOOK_PAGES 0) compile only the no-op versions at the end.
 */
#include "studio_int.h"

#if STUDIO_ADV

#define X (&studio.xw)
/* the bytes of a SongExt an arrangement edit can change: header, the 16 parts and the 64 entries (the pool and the cell map
 * are the note grid's, not touched here) */
#define META ((int)(offsetof(SongExt, trk) + sizeof(((SongExt *)0)->trk)))   /* ... and the 8 track settings of the Mix view */
#define AK(r) (0x4000 + (r))            /* coalescing keys: repeated nudges of one setting are one undo step */

static uint8_t mix_col;                 /* Mix view: 0 pan, 1 sound, 2 swing */
static uint8_t g_pat, g_trk, g_stp, g_top;      /* the Note grid's cursor: pattern, track, step; g_top = the highest pitch on screen */
static uint8_t g_note[SX_TRACKS];               /* each track's pitch cursor (a drum track: its drum 1..8); 0 = not set yet */
static char pat_ch(int p) { return (char)(p < 26 ? 'A' + p : 'a' + p - 26); }     /* a pattern's letter: A..Z, then a..f (32 patterns) */
static uint8_t g_box, g_ax, g_at;       /* the Note grid's box: on, and its anchor step / track (the other corner is the cursor) */
static uint8_t g_clip[SX_TRACKS * SONG_STEPS];  /* the box clipboard, row by row: g_ch tracks of g_cw steps (256 B in .bss) */
static uint8_t g_cw, g_ch, g_cdrum;     /* its size, and one bit per row: that row was a drum track */
static uint8_t b_eaten;                 /* B left the part view this frame: the studio must not take it as "leave" */

int st_adv_on(void) { return studio.xw.version != 0; }
int st_adv_free(void) { return studio.xw.version != 0 && (studio.xw.flags & SX_FLAG_FREE) != 0; }
int st_adv_eats_b(void) { return b_eaten; }
int st_adv_grid(void) { return studio.xw.version != 0 && studio.aview == 4; }
int st_adv_box(void) { return g_box; }

void st_adv_enter(int slot)
{
    studio.aview = 0; studio.asel = 0; studio.atop = 0; mix_col = 0;
    studio.osel = studio.otop = 0;
    g_pat = g_trk = g_stp = g_top = 0; g_box = 0; g_cw = g_ch = g_cdrum = 0; memset(g_note, 0, sizeof(g_note));
    if (slot >= 0 && songx_book_get(slot, SS, X)) return;
    X->version = 0;                     /* a failed decode may leave a half-read header */
}

void st_adv_take(void)
{
    if (!X->version) return;
    ed_touch(&X->version, 1);           /* the caller's ed_commit() closes the step with the take itself */
    X->version = 0;
    studio.aview = 0;
    studio.osel = studio.otop = 0;
}

int st_adv_saved(int slot)
{
    if (!X->version) return 1;
    return songx_book_put(slot, &doomo_song_book[slot], X);
}

void st_adv_play(int pos, int singer)
{
    if (X->version) song_play_ext_at(SS, X, singer, pos, 0);
    else song_play_at(SS, singer, pos, 0);
}

int st_adv_pass_of_cursor(void)
{
    return X->version ? songx_entry_pass(X, studio.osel) : studio.osel;
}

uint32_t st_adv_ticks(void)
{
    return X->version ? songx_ticks(SS, X) : song_length_ticks(SS);
}

const SongExt *st_adv_ext(void) { return X->version ? X : NULL; }

int st_adv_line_pass(int line)
{
    int i, n, sec, pass;
    if (!X->version) return -1;
    n = songx_len(X);
    for (i = 0; i < n; i++) {
        songx_at(X, i, &sec, &pass);
        if (X->sect[sec].line[pass % SX_LINES] == line) return i;
    }
    return -1;
}

int st_adv_line_pattern(int line)
{
    int i, n, sec, pass, pat;
    if (!X->version) {
        int pos = studio_line_pos(SS, line);
        return pos < 0 ? -1 : SS->order[pos] & 3;
    }
    n = songx_len(X);
    for (i = 0; i < n; i++) {
        pat = songx_at(X, i, &sec, &pass);
        if (X->sect[sec].line[pass % SX_LINES] == line) return pat < SONG_PATTERNS ? pat : -1;
    }
    return -1;
}

/* ---- one edit = one undo step ------------------------------------------------------------------------------------------ */
typedef int (*AdvFn)(SongExt *x, int a, int b);

/* Runs fn on the arrangement. When it changed anything the before-image of the arrangement's bytes goes on the journal (one step,
 * or one joined step for a repeated `key`), and 1 is returned. fn returns 0 when it could not do it (nothing is touched). */
static int adv_edit(AdvFn fn, int a, int b, int key)
{
    uint8_t before[META], after[META];
    memcpy(before, X, (size_t)META);
    if (!fn(X, a, b)) { memcpy(X, before, (size_t)META); return 0; }
    if (memcmp(before, X, (size_t)META) == 0) return 0;
    memcpy(after, X, (size_t)META);
    memcpy(X, before, (size_t)META);
    if (key >= 0) ed_touch_key(key, X, META); else ed_touch(X, META);
    memcpy(X, after, (size_t)META);
    if (key >= 0) ed_commit_key(key); else ed_commit();
    return 1;
}

static int sec_of(const SongExt *x, int entry) { return x->arrange[DOOMO_CLAMP(entry, 0, x->arrange_len - 1)] % SX_SECTIONS; }

static int e_entry_section(SongExt *x, int entry, int d)
{
    if (x->sections < 2) return 0;
    x->arrange[entry] = (uint8_t)((x->arrange[entry] + d + x->sections) % x->sections);
    return 1;
}
static int e_insert_copy(SongExt *x, int entry, int unused) { (void)unused; return songx_arrange_insert(x, entry + 1, x->arrange[entry]); }
static int e_delete(SongExt *x, int entry, int unused) { (void)unused; return songx_arrange_remove(x, entry); }
static int e_move(SongExt *x, int entry, int d)
{
    int o = entry + d;
    uint8_t t;
    if (o < 0 || o >= x->arrange_len) return 0;
    t = x->arrange[entry]; x->arrange[entry] = x->arrange[o]; x->arrange[o] = t;
    return 1;
}
static int e_unique(SongExt *x, int entry, int unused)
{
    int n;
    (void)unused;
    if (songx_sect_users(x, x->arrange[entry]) < 2) return 0;
    n = songx_section_dup(x, x->arrange[entry]);
    if (n < 0) return 0;
    x->arrange[entry] = (uint8_t)n;
    return 1;
}
static int e_new_part(SongExt *x, int entry, int name)
{
    int n, first = x->sect[sec_of(x, entry)].pat[0];
    n = songx_add_section(x, name);
    if (n < 0) return 0;
    x->sect[n].pat[0] = (uint8_t)first;
    if (!songx_arrange_insert(x, entry + 1, n)) return 0;       /* the caller restores the bytes */
    return 1;
}
static int e_name(SongExt *x, int sec, int v) { x->sect[sec].name = (uint8_t)v; return 1; }
static int e_key(SongExt *x, int sec, int v) { x->sect[sec].key = (int8_t)DOOMO_CLAMP(v, -12, 12); return 1; }
static int e_tempo(SongExt *x, int sec, int v) { x->sect[sec].tempo = (uint8_t)DOOMO_CLAMP(v, 50, 200); return 1; }
static int e_fade(SongExt *x, int sec, int v) { x->sect[sec].fade = (uint8_t)(v % SX_FADE_COUNT); return 1; }
static int e_pass_pat(SongExt *x, int sec, int packed)       /* packed = pass * 8 + (delta + 4) */
{
    SxSection *c = &x->sect[sec];
    int pass = packed / 8, d = packed % 8 - 4;
    int n;
    if (pass >= c->npat) return 0;
    n = DOOMO_CLAMP(x->patterns, SONG_PATTERNS, SX_PATTERNS);       /* every pattern the song has (the Note grid makes new ones) */
    c->pat[pass] = (uint8_t)((c->pat[pass] % n + d + n) % n);
    return 1;
}
static int e_pass_add(SongExt *x, int sec, int at)           /* a copy of pass `at` just after it */
{
    SxSection *c = &x->sect[sec];
    if (c->npat >= SX_SECT_PATS || at < 0 || at >= c->npat) return 0;
    memmove(c->pat + at + 1, c->pat + at, (size_t)(c->npat - at));
    c->npat++;
    return 1;
}
static int e_pass_del(SongExt *x, int sec, int at)
{
    SxSection *c = &x->sect[sec];
    if (c->npat <= 1 || at < 0 || at >= c->npat) return 0;
    memmove(c->pat + at, c->pat + at + 1, (size_t)(c->npat - at - 1));
    c->pat[--c->npat] = 0;
    return 1;
}
static int e_trk_pan(SongExt *x, int t, int v) { x->trk[t].pan = (uint8_t)DOOMO_CLAMP(v, 0, 2); return 1; }
static int e_trk_env(SongExt *x, int t, int v) { x->trk[t].env = (uint8_t)(v & 3); return 1; }
static int e_trk_swing(SongExt *x, int t, int v) { x->trk[t].swing = (uint8_t)DOOMO_CLAMP(v, 0, 8); return 1; }
static int e_free(SongExt *x, int on, int unused) { (void)unused; if (on) x->flags |= SX_FLAG_FREE; else x->flags &= (uint8_t)~SX_FLAG_FREE; return 1; }
static int e_bright(SongExt *x, int sec, int v) { x->sect[sec].bright = (uint8_t)(v & 3); return 1; }
static int e_chorus_up(SongExt *x, int steps, int unused) { (void)unused; return songx_last_chorus_up(x, steps); }

/* ---- opening: the classic order becomes the first arrangement ---------------------------------------------------------- */
static int e_from_order(SongExt *x, int a, int b)
{
    (void)a; (void)b;
    songx_from_order(x, SS);
    return 1;
}

/* Epic: the made song as 8 parts of 4 passes = 32 passes = 64 bars: Verse, Chorus, Verse, Chorus, Bridge, Chorus, Chorus, Outro */
static int e_epic(SongExt *x, int a, int b)
{
    static const uint8_t arr[8] = { 0, 1, 0, 1, 2, 1, 1, 3 };
    (void)a; (void)b;
    songx_from_order(x, SS);
    if (x->sections < 4) return 0;
    memcpy(x->arrange, arr, sizeof(arr));
    x->arrange_len = 8;
    return 1;
}

int st_adv_epic(void)
{
    if (!st_level()) return 0;
    if (!X->version) memset(X, 0, sizeof(*X));
    return adv_edit(e_epic, 0, 0, -1);
}

int st_adv_folded(char *buf, int cap) { return X->version ? songx_folded(X, buf, cap) : 0; }

int st_adv_try_open(void)
{
    int cls = studio.osel, dirty = studio.dirty, ok;
    StEasy *ez = st_easy();
    uint8_t edited = ez ? ez->edited : 0;
    uint16_t help_t = ez ? ez->help_t : 0;
    if (X->version || !st_level()) return 0;
    memset(X, 0, sizeof(*X));            /* not journaled: the cell map and the pool are empty, as they are after undo too */
    ok = adv_edit(e_from_order, 0, 0, -1);
    /* opening the tab is not an edit of the person's: the song stays clean and the one-line help stays up */
    studio.dirty = (uint8_t)dirty;
    if (ez) { ez->edited = edited; ez->help_t = help_t; }
    if (!ok) return 0;
    studio.aview = 0; studio.asel = 0; studio.atop = 0;
    studio.osel = (uint8_t)DOOMO_MIN(cls / 4, X->arrange_len - 1);
    studio.otop = 0;
    st_msg("Sections on: the same song");
    return 1;
}

/* ---- the menus ------------------------------------------------------------------------------------------------------------ */
static const char *const name_items[SX_NAMES] = { "Intro", "Verse", "Pre-chorus", "Chorus", "Bridge", "Outro" };
static const char *const key_items[10] = { "No change", "Up 1", "Up 2", "Up 3", "Up 5", "Up 7", "Down 1", "Down 2", "Down 5", "Down 7" };
static const int8_t key_val[10] = { 0, 1, 2, 3, 5, 7, -1, -2, -5, -7 };
static const char *const tempo_items[8] = { "Slow 70%", "80%", "90%", "Normal 100%", "110%", "120%", "135%", "Fast 150%" };
static const uint8_t tempo_val[8] = { 70, 80, 90, 100, 110, 120, 135, 150 };
static const char *const fade_items[SX_FADE_COUNT] = { "No fade", "Fade in", "Fade out" };

static int cur_sec(void) { return sec_of(X, studio.osel); }

static void say_users(const char *what, int sec)
{
    int u = songx_sect_users(X, sec);
    if (u > 1) st_msg("%s, all %d %s", what, u, songx_name(X->sect[sec].name));
    else st_msg("%s", what);
}

static void name_done(int c)
{
    if (c < 0 || c >= SX_NAMES) return;
    if (adv_edit(e_name, cur_sec(), c, -1)) { say_users("Renamed", cur_sec()); audio_sfx(SFX_SELECT); }
}
static void key_done(int c)
{
    int sec = cur_sec(), k;
    char buf[24];
    if (c < 0 || c >= 10) return;
    k = key_val[c];
    if (adv_edit(e_key, sec, k, -1)) {
        doomo_snprintf(buf, sizeof(buf), k ? "Key %+d" : "Key as written", k);
        say_users(buf, sec);
        audio_sfx(SFX_SELECT);
    }
}
static void tempo_done(int c)
{
    int sec = cur_sec();
    char buf[24];
    if (c < 0 || c >= 8) return;
    if (adv_edit(e_tempo, sec, tempo_val[c], -1)) {
        doomo_snprintf(buf, sizeof(buf), "Tempo %d%%", tempo_val[c]);
        say_users(buf, sec);
        audio_sfx(SFX_SELECT);
    }
}
static void fade_done(int c)
{
    int sec = cur_sec();
    if (c < 0 || c >= SX_FADE_COUNT) return;
    if (adv_edit(e_fade, sec, c, -1)) { say_users(fade_items[c], sec); audio_sfx(SFX_SELECT); }
}
static void newpart_done(int c)
{
    if (c < 0 || c >= SX_NAMES) return;
    if (X->arrange_len >= SX_ARRANGE) { st_msg("The song is full (64)"); audio_sfx(SFX_ERROR); return; }
    if (X->sections >= SX_SECTIONS) { st_msg("Too many parts (16)"); audio_sfx(SFX_ERROR); return; }
    if (adv_edit(e_new_part, studio.osel, c, -1)) {
        studio.osel++;
        st_msg("New part: %s", songx_name(c));
        audio_sfx(SFX_SPARKLE);
    }
}

static void pop_name(void) { st_popup("Name this part", name_items, SX_NAMES, 0, name_done); }
static void pop_key(void) { st_popup("Key change", key_items, 10, 0, key_done); }
static void pop_tempo(void) { st_popup("Tempo", tempo_items, 8, 0, tempo_done); }
static void pop_fade(void) { st_popup("Fade", fade_items, SX_FADE_COUNT, 0, fade_done); }

static void do_insert_copy(void)
{
    if (X->arrange_len >= SX_ARRANGE) { st_msg("The song is full (64)"); audio_sfx(SFX_ERROR); return; }
    if (adv_edit(e_insert_copy, studio.osel, 0, -1)) { studio.osel++; audio_sfx(SFX_SELECT); }
}
static void do_delete(void)
{
    if (X->arrange_len <= 1) { st_msg("A song needs one part"); audio_sfx(SFX_ERROR); return; }
    if (adv_edit(e_delete, studio.osel, 0, -1)) {
        if (studio.osel >= X->arrange_len) studio.osel = (uint8_t)(X->arrange_len - 1);
        audio_sfx(SFX_CANCEL);
    }
}
static void do_unique(void)
{
    if (songx_sect_users(X, cur_sec()) < 2) { st_msg("It is the only one already"); audio_sfx(SFX_ERROR); return; }
    if (X->sections >= SX_SECTIONS) { st_msg("Too many parts (16)"); audio_sfx(SFX_ERROR); return; }
    if (adv_edit(e_unique, studio.osel, 0, -1)) { st_msg("Its own copy now"); audio_sfx(SFX_SPARKLE); }
}
static void do_chorus_up(void)
{
    if (adv_edit(e_chorus_up, 2, 0, -1)) { st_msg("The last chorus is up 2"); audio_sfx(SFX_SPARKLE); }
    else { st_msg("No chorus to lift"); audio_sfx(SFX_ERROR); }
}
static void do_loop(void)
{
    ed_touch(&SS->flags, 1);
    SS->flags ^= SONGF_LOOP;
    ed_commit();
    st_msg((SS->flags & SONGF_LOOP) ? "The song loops forever" : "The song plays once");
}
static void do_pass_add(int at)
{
    int sec = cur_sec();
    if (X->sect[sec].npat >= SX_SECT_PATS) { st_msg("A part has 8 passes"); audio_sfx(SFX_ERROR); return; }
    if (adv_edit(e_pass_add, sec, at, -1)) audio_sfx(SFX_SELECT);
}
static void do_pass_del(int at)
{
    int sec = cur_sec();
    if (X->sect[sec].npat <= 1) { st_msg("A part needs one pass"); audio_sfx(SFX_ERROR); return; }
    if (adv_edit(e_pass_del, sec, at, -1)) {
        int n = X->sect[sec].npat;
        if (studio.asel >= 4 + n && n < SX_SECT_PATS) studio.asel = (uint8_t)(3 + n);
        audio_sfx(SFX_CANCEL);
    }
}

static void to_mix_view(void) { studio.aview = 2; studio.asel = 0; studio.atop = 0; audio_sfx(SFX_SELECT); }
static void to_expert_view(void) { studio.aview = 3; studio.asel = 0; studio.atop = 0; audio_sfx(SFX_SELECT); }
static void to_part_view(void) { studio.aview = 1; studio.asel = 0; studio.atop = 0; audio_sfx(SFX_SELECT); }

static void arrange_tools(int c)
{
    switch (c) {
    case 0: to_part_view(); break;
    case 1: do_insert_copy(); break;
    case 2: do_unique(); break;
    case 3: st_popup("New part", name_items, SX_NAMES, 0, newpart_done); break;
    case 4: do_delete(); break;
    case 5: pop_key(); break;
    case 6: pop_tempo(); break;
    case 7: pop_fade(); break;
    case 8: do_chorus_up(); break;
    case 9: do_loop(); break;
    default: break;
    }
}

static void part_tools(int c)
{
    int pass = studio.asel >= 4 ? studio.asel - 4 : 0;
    switch (c) {
    case 0: do_pass_add(DOOMO_MIN(pass, X->sect[cur_sec()].npat - 1)); break;
    case 1: do_pass_del(DOOMO_MIN(pass, X->sect[cur_sec()].npat - 1)); break;
    case 2: do_unique(); break;
    case 3: pop_name(); break;
    case 4: pop_key(); break;
    case 5: pop_tempo(); break;
    case 6: pop_fade(); break;
    case 7:
        if (song_playing()) song_stop();
        st_adv_play(songx_entry_pass(X, studio.osel), -1);
        break;
    case 8: to_mix_view(); break;
    case 9: to_expert_view(); break;
    default: break;
    }
}

/* ---- update ------------------------------------------------------------------------------------------------------------ */
static void arrange_update(void)
{
    int n = X->arrange_len, vis = (st_content().h - 40) / (st_wide() ? 15 : 12);
    if (studio.osel >= n) studio.osel = (uint8_t)(n - 1);
    if (btn_pressed(BTN_A)) { studio.a_edit = 0; studio.o_a = 1; }
    if (btn_down(BTN_A)) {
        /* hold A + up / down: carry the entry (as on the classic list) */
        if (btn_held_ticks(BTN_A) > 6) {
            if (btn_repeat(BTN_UP) && adv_edit(e_move, studio.osel, -1, -1)) { studio.osel--; studio.a_edit = 1; audio_sfx(SFX_CURSOR); }
            if (btn_repeat(BTN_DOWN) && adv_edit(e_move, studio.osel, 1, -1)) { studio.osel++; studio.a_edit = 1; audio_sfx(SFX_CURSOR); }
        }
    } else {
        if (btn_repeat(BTN_UP)) { studio.osel = (uint8_t)((studio.osel + n - 1) % n); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { studio.osel = (uint8_t)((studio.osel + 1) % n); audio_sfx(SFX_CURSOR); }
    }
    if (btn_released(BTN_A) && !studio.a_edit && studio.o_a) to_part_view();     /* a tap opens the part (a menu choice's A is not a tap) */
    if (btn_released(BTN_A)) { studio.a_edit = 0; studio.o_a = 0; }
    if (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT)) {
        /* the entry plays another part */
        if (adv_edit(e_entry_section, studio.osel, btn_repeat(BTN_LEFT) ? -1 : 1, AK(studio.osel))) audio_sfx(SFX_CURSOR);
    }
    if (studio_sel_tap) {
        static const char *const items[10] = { "Edit this part", "Insert a copy", "Make it unique", "New part...", "Delete",
                                               "Key change...", "Tempo change...", "Fade...", "Last chorus up", "Repeat on/off" };
        static const char *const help[10] = { "Name, key, tempo, passes.", "Play this part again, just below.",
                                              "Its own copy: edits stay here.", "Add a verse, a chorus, a bridge...",
                                              "Take this entry out.", "Lift or drop the whole part.",
                                              "Faster or slower part.", "Fade the part in or out.",
                                              "Up 2 semitones for the end.", "Play once, or over and over." };
        st_popup("Arrange", items, 10, 0, arrange_tools);
        st_popup_help(help);
    }
    if (vis < 3) vis = 3;
    if (studio.osel < studio.otop) studio.otop = studio.osel;
    if (studio.osel >= studio.otop + vis) studio.otop = (uint8_t)(studio.osel - vis + 1);
}

static int part_rows(void) { return 4 + X->sect[cur_sec()].npat + (X->sect[cur_sec()].npat < SX_SECT_PATS ? 1 : 0); }

static void part_adjust(int d)
{
    int sec = cur_sec(), row = studio.asel;
    const SxSection *c = &X->sect[sec];
    if (row == 0) {
        int cur = c->name < SX_NAMES ? c->name : 0;
        if (adv_edit(e_name, sec, (cur + d + SX_NAMES) % SX_NAMES, AK(0x100))) audio_sfx(SFX_CURSOR);
    } else if (row == 1) {
        int k = DOOMO_CLAMP(c->key + d, -12, 12);
        if (k != c->key && adv_edit(e_key, sec, k, AK(0x101))) audio_sfx(SFX_CURSOR);
    } else if (row == 2) {
        int t = DOOMO_CLAMP((int)c->tempo + d * 5, 50, 200);
        if (t != c->tempo && adv_edit(e_tempo, sec, t, AK(0x102))) audio_sfx(SFX_CURSOR);
    } else if (row == 3) {
        if (adv_edit(e_fade, sec, (c->fade + d + SX_FADE_COUNT) % SX_FADE_COUNT, AK(0x103))) audio_sfx(SFX_CURSOR);
    } else if (row - 4 < c->npat) {
        if (adv_edit(e_pass_pat, sec, (row - 4) * 8 + (d + 4), AK(0x110 + row))) audio_sfx(SFX_CURSOR);
    }
}

static void part_update(void)
{
    int rows = part_rows(), vis = (st_content().h - 24 - (st_wide() ? 15 : 12)) / (st_wide() ? 15 : 12);
    int sec = cur_sec(), npat = X->sect[sec].npat;
    if (studio.asel >= rows) studio.asel = (uint8_t)(rows - 1);
    if (btn_pressed(BTN_B)) { studio.aview = 0; b_eaten = 1; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_UP)) { studio.asel = (uint8_t)((studio.asel + rows - 1) % rows); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.asel = (uint8_t)((studio.asel + 1) % rows); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) part_adjust(-1);
    if (btn_repeat(BTN_RIGHT)) part_adjust(1);
    if (btn_pressed(BTN_A)) {
        int row = studio.asel;
        if (row >= 4 + npat) do_pass_add(npat - 1);                  /* the "+ Add a pass" row */
        else if (row >= 4) do_pass_add(row - 4);                     /* a pass: another one just after it */
        else part_adjust(1);                                       /* the settings: A is the same as RIGHT */
    }
    if (studio_sel_tap) {
        static const char *const items[10] = { "Add a pass", "Delete this pass", "Make it unique", "Name...", "Key change...",
                                              "Tempo change...", "Fade...", "Hear this part", "Mix and sound...", "Notes, expert..." };
        static const char *const help[10] = { "A copy of the pass, just after.", "Take the pass out.",
                                             "Its own copy: edits stay here.", "Verse, chorus, bridge...",
                                             "Lift or drop the whole part.", "Faster or slower part.",
                                             "Fade the part in or out.", "Play from this part.",
                                             "Pan, sound and swing per track.", "Note grid, free notes, memory." };
        st_popup("This part", items, 10, 0, part_tools);
        st_popup_help(help);
    }
    if (vis < 3) vis = 3;
    if (studio.asel < studio.atop) studio.atop = studio.asel;
    if (studio.asel >= studio.atop + vis) studio.atop = (uint8_t)(studio.asel - vis + 1);
}

/* ---- the Mix view: pan, sound (envelope) and swing per track, and the lead brightness of the part ------------------------- */
static int mix_tracks(void) { return DOOMO_CLAMP((int)X->tracks, 4, SX_TRACKS); }
static int mix_rows(void) { return mix_tracks() + 1; }              /* the last row is the part's brightness */

static void mix_adjust(int d)
{
    int row = studio.asel, nt = mix_tracks();
    const SxTrack *t;
    if (row >= nt) {
        int sec = cur_sec(), v = (X->sect[sec].bright + d + 4) & 3;
        if (adv_edit(e_bright, sec, v, AK(0x200))) audio_sfx(SFX_CURSOR);
        return;
    }
    t = &X->trk[row];
    if (mix_col == 0) { if (adv_edit(e_trk_pan, row, t->pan + d, AK(0x210 + row))) audio_sfx(SFX_CURSOR); }
    else if (mix_col == 1) { if (adv_edit(e_trk_env, row, t->env + d + 4, AK(0x220 + row))) audio_sfx(SFX_CURSOR); }
    else if (adv_edit(e_trk_swing, row, t->swing + d, AK(0x230 + row))) audio_sfx(SFX_CURSOR);
}

static void mix_update(void)
{
    int rows = mix_rows();
    if (studio.asel >= rows) studio.asel = (uint8_t)(rows - 1);
    if (btn_pressed(BTN_B)) { studio.aview = 0; b_eaten = 1; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_UP)) { studio.asel = (uint8_t)((studio.asel + rows - 1) % rows); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.asel = (uint8_t)((studio.asel + 1) % rows); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) mix_adjust(-1);
    if (btn_repeat(BTN_RIGHT)) mix_adjust(1);
    if (btn_pressed(BTN_A) && studio.asel < mix_tracks()) { mix_col = (uint8_t)((mix_col + 1) % 3); audio_sfx(SFX_CURSOR); }
    else if (btn_pressed(BTN_A)) mix_adjust(1);
}


/* ---- the Note grid: every note of every pattern and track (a piano roll) ----------------------------------------------------
 * One pattern (2 bars, 32 steps) of one track. Melodic rows are pitches (the scale's own notes are the light rows), the drum
 * tracks have their 8 lanes. Patterns past the classic four and tracks 5..8 live in the arrangement's pool of note blocks; the
 * four classic tracks of A..D are the song's own cells. Every press is one undo step (it journals exactly the bytes it changes). */
static const char *const trk_names[SX_TRACKS] = { "Lead", "Harmony", "Bass", "Drums", "Extra 5", "Extra 6", "Extra 7", "Extra 8" };
static const char *const lane_names[8] = { "Kik", "Snr", "Hat", "Opn", "Clp", "Tom", "Crs", "Cow" };

static int g_nt(void) { return mix_tracks(); }
static int g_npat(void) { return DOOMO_CLAMP((int)X->patterns, SONG_PATTERNS, SX_PATTERNS); }
static int g_drum(int t) { return songx_lane_of(t) == 3; }
static int g_cell(int pat, int trk, int step) { return songx_cell(SS, X, pat, trk, step); }
static int g_classic(int pat, int trk) { return pat < SONG_PATTERNS && trk < SONG_TRACKS; }
static int g_rowh(int drum) { return drum ? (st_wide() ? 10 : 9) : (st_wide() ? 8 : 6); }
static int g_vis(int drum)
{
    int v = (st_content().h - 17 - 26) / g_rowh(drum);
    return drum ? 8 : DOOMO_CLAMP(v, 4, 24);
}

static int g_default_note(int t)
{
    int i, v;
    for (i = 0; i < SONG_STEPS; i++) {
        v = g_cell(g_pat, t, i);
        if (g_drum(t) ? (v > 0 && v < SONG_DRUM_COUNT) : v >= 2) return v;
    }
    if (g_drum(t)) return SONG_DRUM_KICK;
    v = songx_lane_of(t) == 2 ? 48 : songx_lane_of(t) == 1 ? 64 : 72;
    return st_adv_free() ? v : song_scale_snap(SS, v);
}

static void g_scroll(int centre)
{
    int vis = g_vis(g_drum(g_trk)), n = g_note[g_trk];
    if (g_drum(g_trk)) { g_top = 0; return; }
    if (centre) g_top = (uint8_t)DOOMO_MIN(SONG_NOTE_MAX, n + vis / 2);
    if (g_top < SONG_NOTE_MIN + vis - 1) g_top = (uint8_t)(SONG_NOTE_MIN + vis - 1);
    if (n > g_top) g_top = (uint8_t)n;
    if (n < g_top - vis + 1) g_top = (uint8_t)(n + vis - 1);
}

static void g_focus(void)                       /* the track under the cursor has a pitch and it is on screen */
{
    if (!g_note[g_trk]) g_note[g_trk] = (uint8_t)g_default_note(g_trk);
    g_scroll(1);
}

static void g_preview(int t, int v)
{
    if (!song_playing() && v >= (g_drum(t) ? 1 : 2)) song_preview(SS, songx_lane_of(t), v);
}

/* A cell whose extra block is all rests gives the block back (the last block moves into the hole): a cleared track costs nothing. */
static int g_free_block(int pat, int trk)
{
    int b = X->map[pat][trk], last = X->nblocks, i, p, t;
    if (g_classic(pat, trk) || !b) return 0;
    for (i = 0; i < SONG_STEPS; i++) if (X->pool[b - 1][i]) return 0;
    ed_touch(&X->nblocks, 1);
    ed_touch(&X->map[pat][trk], 1);
    if (b != last) {
        for (p = 0; p < SX_PATTERNS; p++)
            for (t = 0; t < SX_TRACKS; t++)
                if (X->map[p][t] == last) {
                    ed_touch(&X->map[p][t], 1);
                    X->map[p][t] = (uint8_t)b;
                    p = SX_PATTERNS; break;
                }
        ed_touch(X->pool[b - 1], SONG_STEPS);
        memcpy(X->pool[b - 1], X->pool[last - 1], SONG_STEPS);
    }
    X->map[pat][trk] = 0;
    X->nblocks--;
    return 1;
}

/* Stores v in one cell and journals what that changes (the caller commits): 1 = changed, 0 = it was so already, -1 = no room. */
static int g_set(int pat, int trk, int step, int v)
{
    if (g_cell(pat, trk, step) == v) return 0;
    if (g_classic(pat, trk)) ed_touch(&SS->notes[pat][trk][step], 1);
    else if (!X->map[pat][trk]) {
        if (X->nblocks >= SX_POOL) return -1;
        ed_touch(&X->nblocks, 1);
        ed_touch(&X->map[pat][trk], 1);
        ed_touch(X->pool[X->nblocks], SONG_STEPS);
    } else ed_touch(&X->pool[X->map[pat][trk] - 1][step], 1);
    songx_set_cell(SS, X, pat, trk, step, v);
    return 1;
}

static int g_count(int pat, int trk)
{
    int i, n = 0;
    for (i = 0; i < SONG_STEPS; i++) {
        int v = g_cell(pat, trk, i);
        n += g_drum(trk) ? v > 0 : v >= 2;
    }
    return n;
}

static void g_place(void)                       /* A: this pitch (this drum) on the step, or off it when it is there already */
{
    int t = g_trk, v = g_cell(g_pat, t, g_stp), want = g_note[t], nv = v == want ? 0 : want, r, i;
    r = g_set(g_pat, t, g_stp, nv);
    if (r < 0) { st_msg("No room for more notes"); audio_sfx(SFX_ERROR); return; }
    if (!r) return;
    if (!nv) {
        if (!g_drum(t)) for (i = g_stp + 1; i < SONG_STEPS && g_cell(g_pat, t, i) == SONG_HOLD; i++) g_set(g_pat, t, i, 0);   /* its ties go too */
        g_free_block(g_pat, t);
    }
    ed_commit();
    if (nv) g_preview(t, nv); else audio_sfx(SFX_BLIP);
}

static void g_pitch(int d)                      /* d = +1: the row above */
{
    int t = g_trk, n = g_note[t];
    if (g_drum(t)) n = (n - 1 - d + 16) % 8 + 1;
    else n = DOOMO_CLAMP(st_adv_free() ? n + d : song_scale_shift(SS, n, d), SONG_NOTE_MIN, SONG_NOTE_MAX);
    if (n == g_note[t]) return;
    g_note[t] = (uint8_t)n;
    g_scroll(0);
    if (!g_drum(t)) g_preview(t, n); else audio_sfx(SFX_CURSOR);
}

static void g_track(int d)
{
    int nt = g_nt();
    g_trk = (uint8_t)((g_trk + d + nt) % nt);
    g_focus();
    audio_sfx(SFX_CURSOR);
    st_msg(g_trk >= songx_tracks_max() ? "%s (mixed in here)" : "%s", trk_names[g_trk]);
}

static void g_pattern(int d)
{
    int n = g_npat();
    g_pat = (uint8_t)((g_pat + d + n) % n);
    g_note[g_trk] = 0;
    g_focus();
    audio_sfx(SFX_CURSOR);
    st_msg("Pattern %c", pat_ch(g_pat));
}

static void g_new_pattern(void)
{
    int np = X->patterns, cost = songx_pattern_cost(SS, X, g_pat);
    if (np >= SX_PATTERNS) { st_msg("32 patterns is the most"); audio_sfx(SFX_ERROR); return; }
    if (cost > songx_blocks_free(X)) { st_msg("No room: needs %d note blocks", cost); audio_sfx(SFX_ERROR); return; }
    ed_touch(&X->patterns, 1);
    ed_touch(&X->nblocks, 1);
    ed_touch(&X->map[np][0], SX_TRACKS);
    if (cost) ed_touch(X->pool[X->nblocks], cost * SONG_STEPS);
    if (songx_pattern_copy(SS, X, g_pat) < 0) return;
    ed_commit();
    g_pat = (uint8_t)np;
    g_note[g_trk] = 0;
    g_focus();
    st_msg("Pattern %c: pick it in a pass", pat_ch(np));
    audio_sfx(SFX_SPARKLE);
}

static void g_tie(void)
{
    int t = g_trk, v = g_cell(g_pat, t, g_stp), prev = g_stp ? g_cell(g_pat, t, g_stp - 1) : 0, r;
    if (g_drum(t)) { st_msg("Drums do not tie"); audio_sfx(SFX_ERROR); return; }
    if (v == SONG_HOLD) {
        g_set(g_pat, t, g_stp, 0);
        g_free_block(g_pat, t);
        ed_commit();
        st_msg("Tie taken off");
        audio_sfx(SFX_BLIP);
        return;
    }
    if (prev < 1) { st_msg("Nothing to tie to"); audio_sfx(SFX_ERROR); return; }
    r = g_set(g_pat, t, g_stp, SONG_HOLD);
    if (r < 0) { st_msg("No room for more notes"); audio_sfx(SFX_ERROR); return; }
    ed_commit();
    st_msg("Tied: the note rings on");
    audio_sfx(SFX_SELECT);
}

static void g_clear_track(void)
{
    int i, n = 0, t = g_trk;
    for (i = 0; i < SONG_STEPS; i++) n += g_set(g_pat, t, i, 0) > 0;
    if (!n) { st_msg("Nothing to clear"); audio_sfx(SFX_ERROR); return; }
    g_free_block(g_pat, t);
    ed_commit();
    st_msg("%s cleared here", trk_names[t]);
    audio_sfx(SFX_CANCEL);
}

static void g_octave(int d)
{
    int i, t = g_trk, n = 0;
    if (g_drum(t)) { st_msg("Drums have no octaves"); audio_sfx(SFX_ERROR); return; }
    for (i = 0; i < SONG_STEPS; i++) {
        int v = g_cell(g_pat, t, i);
        if (v < 2) continue;
        if (v + d * 12 < SONG_NOTE_MIN || v + d * 12 > SONG_NOTE_MAX) { st_msg("Out of range: it stays"); audio_sfx(SFX_ERROR); return; }
        n++;
    }
    if (!n) { st_msg("No notes to move"); audio_sfx(SFX_ERROR); return; }
    for (i = 0; i < SONG_STEPS; i++) {
        int v = g_cell(g_pat, t, i);
        if (v >= 2) g_set(g_pat, t, i, v + d * 12);          /* the cell is stored already: no new block is needed */
    }
    ed_commit();
    g_note[t] = (uint8_t)DOOMO_CLAMP(g_note[t] + d * 12, SONG_NOTE_MIN, SONG_NOTE_MAX);
    g_scroll(1);
    st_msg(d > 0 ? "Up an octave" : "Down an octave");
    audio_sfx(SFX_SELECT);
}

static int e_tracks(SongExt *x, int d, int unused)
{
    int n = x->tracks + d;
    (void)unused;
    if (n < SONG_TRACKS || n > SX_TRACKS) return 0;
    x->tracks = (uint8_t)n;
    return 1;
}

static int g_track_used(int t)                  /* any note of track t in any pattern */
{
    int p;
    for (p = 0; p < g_npat(); p++) if (g_count(p, t)) return 1;
    return 0;
}

static void g_more_tracks(int d)
{
    int nt = g_nt();
    if (d > 0 && nt >= SX_TRACKS) { st_msg("8 tracks is the most"); audio_sfx(SFX_ERROR); return; }
    if (d < 0 && nt <= SONG_TRACKS) { st_msg("The four tracks stay"); audio_sfx(SFX_ERROR); return; }
    if (d < 0 && g_track_used(nt - 1)) { st_msg("Clear %s first", trk_names[nt - 1]); audio_sfx(SFX_ERROR); return; }
    if (!adv_edit(e_tracks, d, 0, -1)) return;
    if (d < 0) { if (g_trk >= g_nt()) g_trk = (uint8_t)(g_nt() - 1); g_focus(); st_msg("%d tracks", g_nt()); }
    else { g_trk = (uint8_t)(g_nt() - 1); g_focus(); st_msg("%s added", trk_names[g_trk]); }
    audio_sfx(d > 0 ? SFX_SPARKLE : SFX_CANCEL);
}


/* ---- the Box: a rectangle of steps over several tracks of one pattern (Note grid, "Box and paste...") ----------------------
 * Select a box (anchor at the cursor, stretch with LEFT / RIGHT / L / R), then A (or SELECT) opens the box tools: Copy, Cut, Clear,
 * Octave up / down, Paste here, Drop the box. Paste puts the copied box with its top-left corner at the cursor, on this pattern
 * (or any other: leave the box, pick a pattern, paste). A drum row only pastes onto a drum track and a pitch row onto a pitch track.
 * Every command is one undo step. While the box is open the grid shows every track at once (the box view). */
static void g_rect(int *s0, int *s1, int *t0, int *t1)
{
    *s0 = DOOMO_MIN(g_ax, g_stp); *s1 = DOOMO_MAX(g_ax, g_stp);
    *t0 = DOOMO_MIN(g_at, g_trk); *t1 = DOOMO_MAX(g_at, g_trk);
}

static int g_is_note(int t, int v) { return g_drum(t) ? v > 0 && v < SONG_DRUM_COUNT : v >= 2; }

static int g_box_notes(void)
{
    int s0, s1, t0, t1, s, t, n = 0;
    g_rect(&s0, &s1, &t0, &t1);
    for (t = t0; t <= t1; t++) for (s = s0; s <= s1; s++) n += g_is_note(t, g_cell(g_pat, t, s));
    return n;
}

/* A tie needs a note (or a tie) before it: after a clear or a paste, the ties that lost their note go too. */
static void g_fix_ties(int t)
{
    int i, prev;
    if (g_drum(t)) return;
    for (i = 0; i < SONG_STEPS; i++) {
        if (g_cell(g_pat, t, i) != SONG_HOLD) continue;
        prev = i ? g_cell(g_pat, t, i - 1) : 0;
        if (prev < 2 && prev != SONG_HOLD) g_set(g_pat, t, i, 0);
    }
}

static void g_box_off(void)                     /* the cursor goes back to the box's top-left corner: the place a paste lands */
{
    int s0, s1, t0, t1;
    g_rect(&s0, &s1, &t0, &t1);
    g_stp = (uint8_t)s0; g_trk = (uint8_t)t0;
    g_box = 0;
    g_focus();
}

static void box_copy_cells(void)
{
    int s0, s1, t0, t1, s, t;
    g_rect(&s0, &s1, &t0, &t1);
    g_cw = (uint8_t)(s1 - s0 + 1); g_ch = (uint8_t)(t1 - t0 + 1); g_cdrum = 0;
    for (t = t0; t <= t1; t++) {
        if (g_drum(t)) g_cdrum |= (uint8_t)(1u << (t - t0));
        for (s = s0; s <= s1; s++) g_clip[(t - t0) * g_cw + (s - s0)] = (uint8_t)g_cell(g_pat, t, s);
    }
}

static int box_clear_cells(void)                 /* returns how many cells it emptied; the caller commits */
{
    int s0, s1, t0, t1, s, t, n = 0;
    g_rect(&s0, &s1, &t0, &t1);
    for (t = t0; t <= t1; t++) {
        int any = 0;
        for (s = s0; s <= s1; s++) any += g_set(g_pat, t, s, 0) > 0;
        if (any) { g_fix_ties(t); g_free_block(g_pat, t); }
        n += any;
    }
    return n;
}

static void box_copy(void)
{
    int n = g_box_notes();
    box_copy_cells();
    st_msg("Copied %d notes", n);
    audio_sfx(SFX_SELECT);
}

static void box_clear(int cut)
{
    int n = g_box_notes();
    if (cut) box_copy_cells();
    if (!box_clear_cells()) {
        st_msg(cut ? "Copied: nothing to cut" : "Nothing in the box");
        audio_sfx(SFX_ERROR);
        return;
    }
    ed_commit();
    st_msg(cut ? "Cut %d notes" : "Cleared %d notes", n);
    audio_sfx(SFX_CANCEL);
}

static void box_octave(int d)
{
    int s0, s1, t0, t1, s, t, n = 0;
    g_rect(&s0, &s1, &t0, &t1);
    for (t = t0; t <= t1; t++) {
        if (g_drum(t)) continue;
        for (s = s0; s <= s1; s++) {
            int v = g_cell(g_pat, t, s);
            if (v < 2) continue;
            if (v + d * 12 < SONG_NOTE_MIN || v + d * 12 > SONG_NOTE_MAX) { st_msg("Out of range: it stays"); audio_sfx(SFX_ERROR); return; }
            n++;
        }
    }
    if (!n) { st_msg("No notes to move"); audio_sfx(SFX_ERROR); return; }
    for (t = t0; t <= t1; t++) {
        if (g_drum(t)) continue;
        for (s = s0; s <= s1; s++) {
            int v = g_cell(g_pat, t, s);
            if (v >= 2) g_set(g_pat, t, s, v + d * 12);
        }
    }
    ed_commit();
    st_msg(d > 0 ? "Box up an octave" : "Box down an octave");
    audio_sfx(SFX_SELECT);
}

static void box_paste(void)
{
    int c, r, t, i, need = 0, notes = 0, changed = 0, skipped = 0, nt = g_nt();
    if (!g_cw) { st_msg("Copy a box first"); audio_sfx(SFX_ERROR); return; }
    for (r = 0; r < g_ch && g_trk + r < nt; r++) {          /* room first: a track with no block yet needs one */
        t = g_trk + r;
        if (g_drum(t) != ((g_cdrum >> r) & 1) || g_classic(g_pat, t) || X->map[g_pat][t]) continue;
        for (c = 0; c < g_cw && g_stp + c < SONG_STEPS; c++) if (g_clip[r * g_cw + c]) { need++; break; }
    }
    if (need > songx_blocks_free(X)) { st_msg("No room: needs %d note blocks", need); audio_sfx(SFX_ERROR); return; }
    for (r = 0; r < g_ch && g_trk + r < nt; r++) {
        t = g_trk + r;
        if (g_drum(t) != ((g_cdrum >> r) & 1)) { skipped++; continue; }
        for (c = 0; c < g_cw && g_stp + c < SONG_STEPS; c++) {
            i = g_stp + c;
            if (g_set(g_pat, t, i, g_clip[r * g_cw + c]) > 0) { changed++; notes += g_is_note(t, g_clip[r * g_cw + c]); }
        }
        g_fix_ties(t);
        g_free_block(g_pat, t);
    }
    if (!changed) { st_msg(skipped ? "Drums and pitches do not mix" : "Nothing changed"); audio_sfx(SFX_ERROR); return; }
    ed_commit();
    if (skipped) st_msg("Pasted; %d track%s skipped", skipped, skipped > 1 ? "s" : "");
    else st_msg("Pasted %d notes", notes);
    audio_sfx(SFX_SPARKLE);
}

static void box_tools(int c)
{
    switch (c) {
    case 0: box_copy(); break;
    case 1: box_clear(1); g_box_off(); break;
    case 2: box_clear(0); break;
    case 3: box_octave(1); break;
    case 4: box_octave(-1); break;
    case 5: g_box_off(); box_paste(); break;
    case 6: g_box_off(); audio_sfx(SFX_CANCEL); break;
    default: break;
    }
}

static void box_menu(void)
{
    static const char *const items[7] = { "Copy", "Cut", "Clear", "Octave up", "Octave down", "Paste here", "Drop the box" };
    static const char *const help[7] = { "Remember the box.", "Copy it, then empty it.", "Empty the box.",
                                         "Pitch tracks in the box, up 12.", "Pitch tracks in the box, down 12.",
                                         "Corner of the copy at the cursor.", "Leave the box alone." };
    st_popup("Box", items, 7, 0, box_tools);
    st_popup_help(help);
}

static void box_update(void)
{
    int nt = g_nt();
    if (btn_pressed(BTN_B)) { g_box_off(); b_eaten = 1; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_LEFT) && g_stp > 0) g_stp--;
    if (btn_repeat(BTN_RIGHT) && g_stp < SONG_STEPS - 1) g_stp++;
    if (btn_pressed(BTN_L) && g_trk > 0) { g_trk--; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_R) && g_trk < nt - 1) { g_trk++; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A) || studio_sel_tap) box_menu();
}

static void grid_more(int c)                    /* grid tools item 9: the box and the sound check */
{
    switch (c) {
    case 0:
        g_box = 1; g_ax = g_stp; g_at = g_trk;
        st_msg("Box: stretch it, A for tools");
        audio_sfx(SFX_SELECT);
        break;
    case 1: box_paste(); break;
    case 2:
        if (song_playing()) song_stop();
        st_adv_play(songx_entry_pass(X, studio.osel), -1);
        break;
    default: break;
    }
}

static void box_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int nt = g_nt(), lw = 26, cw = DOOMO_MAX(3, (r.w - lw - 8) / SONG_STEPS), x0 = r.x + lw, y0 = r.y + 17;
    int rh = DOOMO_CLAMP((st_content().h - 17 - 26) / nt, 4, 10), t, s, s0, s1, t0, t1;
    uint8_t dark = ui_darker(st->fill), beat_c = ui_darker(dark);
    static const char *const shorts[SX_TRACKS] = { "Led", "Har", "Bas", "Drm", "X5", "X6", "X7", "X8" };
    char buf[48];
    g_rect(&s0, &s1, &t0, &t1);
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    buf[0] = pat_ch(g_pat); buf[1] = 0;
    ui_round_rect(r.x + 8, r.y + 4, 14, 10, 3, st_pattern_color(g_pat, 0));
    ui_text(r.x + 12, r.y + 5, buf, PAL_WHITE);
    doomo_snprintf(buf, sizeof(buf), "Box: %d steps, %d tracks", s1 - s0 + 1, t1 - t0 + 1);
    ui_text(r.x + 30, r.y + 5, buf, st->text);
    for (t = 0; t < nt; t++) {
        int y = y0 + t * rh;
        plat_rect(x0, y, cw * SONG_STEPS, rh, (t & 1) ? dark : st->fill);
        ui_text(r.x + 2, y + (rh - 8) / 2, shorts[t], t == g_trk ? st->accent : st->text_dim);
    }
    for (s = 0; s <= SONG_STEPS; s += 4) plat_rect(x0 + s * cw, y0, 1, nt * rh, s % 16 == 0 ? st->text_dim : beat_c);
    for (t = 0; t < nt; t++) {
        uint8_t col = st_pattern_color(t, 0);
        for (s = 0; s < SONG_STEPS; s++) {
            int v = g_cell(g_pat, t, s);
            if (g_is_note(t, v)) plat_rect(x0 + s * cw + 1, y0 + t * rh + 1, cw - 1, rh - 2, col);
            else if (v == SONG_HOLD && !g_drum(t)) plat_rect(x0 + s * cw, y0 + t * rh + rh / 2, cw + 1, 1, col);
        }
    }
    {   /* the box: a frame, and the cursor corner marked */
        int bx = x0 + s0 * cw, by = y0 + t0 * rh, bw = (s1 - s0 + 1) * cw + 1, bh = (t1 - t0 + 1) * rh;
        plat_rect(bx, by, bw, 1, st->accent); plat_rect(bx, by + bh - 1, bw, 1, st->accent);
        plat_rect(bx, by, 1, bh, st->accent); plat_rect(bx + bw - 1, by, 1, bh, st->accent);
        plat_rect(x0 + g_stp * cw, y0 - 3, cw + 1, 2, st->accent);
    }
    doomo_snprintf(buf, sizeof(buf), "%d notes in the box. A: box tools", g_box_notes());
    ui_text(r.x + 8, y0 + nt * rh + 4, buf, st->text);
    if (g_cw) doomo_snprintf(buf, sizeof(buf), "Copied: %d steps x %d tracks", g_cw, g_ch);
    else doomo_snprintf(buf, sizeof(buf), "B drops the box");
    ui_text(r.x + 8, y0 + nt * rh + 14, buf, st->text_dim);
}

static void grid_tools(int c)
{
    switch (c) {
    case 0: g_pattern(1); break;
    case 1: g_pattern(-1); break;
    case 2: g_new_pattern(); break;
    case 3: g_tie(); break;
    case 4: g_clear_track(); break;
    case 5: g_octave(1); break;
    case 6: g_octave(-1); break;
    case 7: g_more_tracks(1); break;
    case 8: g_more_tracks(-1); break;
    case 9: {
        static const char *const more[3] = { "Select a box", "Paste here", "Hear this part" };
        static const char *const mhelp[3] = { "Stretch over steps and tracks.", "Corner of the copy at the cursor.", "Play from this part." };
        st_popup("Box and sound", more, 3, 0, grid_more);
        st_popup_help(mhelp);
        break;
    }
    default: break;
    }
}

static void to_grid(void)
{
    const SxSection *c = &X->sect[cur_sec()];
    g_pat = (uint8_t)(c->pat[0] % g_npat());
    g_box = 0;
    if (g_trk >= g_nt()) g_trk = 0;
    g_focus();
    studio.aview = 4; studio.asel = 0; studio.atop = 0;
    audio_sfx(SFX_SELECT);
}

static void grid_update(void)
{
    if (g_trk >= g_nt()) g_trk = (uint8_t)(g_nt() - 1);
    if (g_pat >= g_npat()) g_pat = 0;
    if (!g_note[g_trk]) g_focus();
    if (g_box) { box_update(); return; }
    if (btn_pressed(BTN_B)) { studio.aview = 3; studio.asel = 1; b_eaten = 1; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_LEFT)) { g_stp = (uint8_t)((g_stp + SONG_STEPS - 1) % SONG_STEPS); }
    if (btn_repeat(BTN_RIGHT)) { g_stp = (uint8_t)((g_stp + 1) % SONG_STEPS); }
    if (btn_repeat(BTN_UP)) g_pitch(1);
    if (btn_repeat(BTN_DOWN)) g_pitch(-1);
    if (btn_pressed(BTN_L)) g_track(-1);
    if (btn_pressed(BTN_R)) g_track(1);
    if (btn_pressed(BTN_A)) g_place();
    if (studio_sel_tap) {
        static const char *const items[10] = { "Next pattern", "Previous pattern", "New pattern (copy)", "Tie to the note before",
                                               "Clear this track here", "Octave up", "Octave down", "Add a track",
                                               "Take the last track away", "Box and paste..." };
        static const char *const help[10] = { "Go to the next pattern.", "Go to the one before.",
                                              "A copy to change; pick it in a pass.", "The note before rings on.",
                                              "Empty this track in this pattern.", "Move this track's notes up 12.",
                                              "Move this track's notes down 12.", "Up to 8 tracks.",
                                              "Only when it is empty.", "Select, copy, paste; hear." };
        st_popup("Note grid", items, 10, 0, grid_tools);
        st_popup_help(help);
    }
}

static void grid_line(char *buf, int cap)
{
    int t = g_trk, v = g_cell(g_pat, t, g_stp), want = g_note[t], bar = g_stp / 16 + 1, beat = g_stp % 16 / 4 + 1;
    char here[12], put[12];
    if (g_drum(t)) {
        doomo_snprintf(here, sizeof(here), "%s", v > 0 && v < SONG_DRUM_COUNT ? lane_names[v - 1] : "rest");
        doomo_snprintf(put, sizeof(put), "%s", lane_names[(want - 1) & 7]);
    } else {
        if (v >= 2) song_note_name(v, here); else doomo_snprintf(here, sizeof(here), "%s", v == SONG_HOLD ? "tied" : "rest");
        song_note_name(want, put);
    }
    doomo_snprintf(buf, (size_t)cap, "Bar %d beat %d: %s. A %s %s", bar, beat, here,
                   v == want ? "takes it off, puts" : v >= 1 && !(g_drum(t) && v == 0) ? "swaps to" : "puts", put);
}

static void grid_draw(StRect r)
{
    if (g_box) { box_draw(r); return; }
    const UiStyle *st = ui_style(WIN_CREAM);
    int t = g_trk, drum = g_drum(t), rh = g_rowh(drum), rows = g_vis(drum), cur = g_note[t];
    int lw = 28, cw = DOOMO_MAX(3, (r.w - lw - 8) / SONG_STEPS), x0 = r.x + lw, w = cw * SONG_STEPS, y0 = r.y + 17;
    int i, s, po = song_playing() ? song_pos_order() : -1, ysum, notes = g_count(g_pat, t);
    uint8_t dark = ui_darker(st->fill), beat_c = ui_darker(dark), col = st_pattern_color(t, 0), hi = st_pattern_color(t, 2);
    char buf[48];
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    ui_text(r.x + 8, r.y + 5, "Pattern", st->text_dim);
    ui_round_rect(r.x + 54, r.y + 4, 14, 10, 3, st_pattern_color(g_pat, 0));
    buf[0] = pat_ch(g_pat); buf[1] = 0;
    ui_text(r.x + 58, r.y + 5, buf, PAL_WHITE);
    doomo_snprintf(buf, sizeof(buf), "< %s >", trk_names[t]);
    ui_text(r.x + 76, r.y + 5, buf, st->text);
    doomo_snprintf(buf, sizeof(buf), "%d notes", notes);
    ui_text(r.x + r.w - 10 - ui_text_width(buf), r.y + 5, buf, st->text_dim);
    /* the roll: a stripe a row (the scale's own notes are the light ones), the beat lines, the notes, the cursor */
    for (i = 0; i < rows; i++) {
        int y = y0 + i * rh, m = g_top - i, on = drum ? (i & 1) == 0 : song_scale_snap(SS, m) == m;
        plat_rect(x0, y, w, rh, on ? st->fill : dark);
        if (drum) ui_text(r.x + 4, y + (rh - 8) / 2, lane_names[i], i + 1 == cur ? st->accent : st->text_dim);
        else if ((m % 12 == 0 && (m > cur ? m - cur : cur - m) > 1) || m == cur) {
            song_note_name(m, buf);
            ui_text(r.x + 4, y + (rh - 8) / 2, buf, m == cur ? st->accent : st->text_dim);
        }
    }
    ysum = rows * rh;
    for (s = 0; s <= SONG_STEPS; s += 4) plat_rect(x0 + s * cw, y0, 1, ysum, s % 16 == 0 ? st->text_dim : beat_c);
    for (s = 0; s < SONG_STEPS; s++) {
        int v = g_cell(g_pat, t, s), row, len = 1, x = x0 + s * cw + 1;
        if (drum ? (v < 1 || v >= SONG_DRUM_COUNT) : v < 2) continue;
        if (drum) row = v - 1;
        else {
            row = g_top - v;
            while (s + len < SONG_STEPS && g_cell(g_pat, t, s + len) == SONG_HOLD) len++;
        }
        if (row < 0) plat_rect(x, y0, DOOMO_MAX(1, len * cw - 1), 1, col);                  /* above the window */
        else if (row >= rows) plat_rect(x, y0 + ysum - 1, DOOMO_MAX(1, len * cw - 1), 1, col);
        else {
            plat_rect(x, y0 + row * rh + 1, len * cw - 1, rh - 2, col);
            plat_rect(x, y0 + row * rh + 1, len * cw - 1, 1, hi);
        }
    }
    if (po >= 0) {
        int sec, pass;
        if (songx_at(X, po, &sec, &pass) == g_pat) plat_rect(x0 + song_pos_step() * cw, y0, 1, ysum, PAL_PAINT(PAINT_ROSE, 0));
    }
    {
        int cx = x0 + g_stp * cw, cy = y0 + (drum ? cur - 1 : g_top - cur) * rh;
        plat_rect(cx, y0 - 3, cw + 1, 2, st->accent);                                        /* the step marker */
        if (cy >= y0 && cy < y0 + ysum) {
            plat_rect(cx, cy, cw + 1, 1, st->accent);
            plat_rect(cx, cy + rh - 1, cw + 1, 1, st->accent);
            plat_rect(cx, cy, 1, rh, st->accent);
            plat_rect(cx + cw, cy, 1, rh, st->accent);
        }
    }
    grid_line(buf, sizeof(buf));
    ui_text(r.x + 8, y0 + ysum + 4, buf, st->text);
    if (t >= songx_tracks_max()) doomo_snprintf(buf, sizeof(buf), "Mixed in on this console");
    else doomo_snprintf(buf, sizeof(buf), "Pattern %c of %d", pat_ch(g_pat), g_npat());
    ui_text(r.x + 8, y0 + ysum + 14, buf, st->text_dim);
    doomo_snprintf(buf, sizeof(buf), "Room %d/%d", songx_blocks_free(X), SX_POOL);
    ui_text(r.x + r.w - 10 - ui_text_width(buf), y0 + ysum + 14, buf, st->text_dim);
}

/* ---- the Expert view: the Free notes switch, note counts and a memory meter ------------------------------------------------ */
typedef struct ExStats {
    int per_trk[SX_TRACKS];             /* notes in the patterns the arrangement uses (each pattern once) */
    int played;                         /* notes the whole song plays (patterns counted every pass) */
    int pats_used;                      /* distinct patterns the arrangement plays */
    int page;                           /* bytes of the encoded book page (0 = does not fit) */
} ExStats;

static int ex_cell_is_note(int trk, int v) { return trk == 3 ? v > 0 : v >= 2 && v != SONG_HOLD; }

static int ex_pat_notes(int pat, int trk)
{
    int i, n = 0;
    for (i = 0; i < SONG_STEPS; i++) n += ex_cell_is_note(trk, songx_cell(SS, X, pat, trk, i));
    return n;
}

static void ex_stats(ExStats *e)
{
    uint8_t used[SX_PATTERNS], buf[SONGX_PAGE_BYTES];
    int i, n, t, sec, pass, pat, nt = mix_tracks();
    memset(e, 0, sizeof(*e));
    memset(used, 0, sizeof(used));
    n = songx_len(X);
    for (i = 0; i < n; i++) {
        pat = songx_at(X, i, &sec, &pass);
        if (pat < 0 || pat >= SX_PATTERNS) continue;
        if (!used[pat]) { used[pat] = 1; e->pats_used++; }
        for (t = 0; t < nt; t++) e->played += ex_pat_notes(pat, t);
    }
    for (pat = 0; pat < SX_PATTERNS; pat++)
        if (used[pat]) for (t = 0; t < nt; t++) e->per_trk[t] += ex_pat_notes(pat, t);
    e->page = (int)songx_encode(SS, X, buf, sizeof(buf));
}

static void expert_update(void)
{
    if (btn_pressed(BTN_B)) { studio.aview = 1; studio.asel = 0; b_eaten = 1; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_UP) || btn_repeat(BTN_DOWN)) { studio.asel ^= 1; audio_sfx(SFX_CURSOR); }
    if (studio.asel == 1) { if (btn_pressed(BTN_A)) to_grid(); return; }
    if ((btn_pressed(BTN_A) || btn_pressed(BTN_LEFT) || btn_pressed(BTN_RIGHT))) {
        int on = (X->flags & SX_FLAG_FREE) == 0;
        if (adv_edit(e_free, on, 0, -1)) { st_msg(on ? "Free notes: on" : "Free notes: off"); audio_sfx(on ? SFX_SPARKLE : SFX_CURSOR); }
    }
}

static void bar(int x, int y, int w, int used, int cap, uint8_t col, uint8_t back)
{
    int f = cap > 0 ? DOOMO_CLAMP(used * w / cap, used > 0 ? 1 : 0, w) : 0;
    plat_rect(x, y, w, 5, back);
    if (f) plat_rect(x, y, f, 5, col);
}

static void expert_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int rh = st_wide() ? 15 : 12, nt = mix_tracks(), i, y = r.y + 6 + rh, bx = r.x + r.w - 76;
    int blocks = SX_POOL - songx_blocks_free(X);
    ExStats e;
    char buf[40];
    ex_stats(&e);
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    ui_text(r.x + 12, r.y + 5, "Expert", st->text);
    {   /* the switch */
        int sel = studio.asel == 0, on = (X->flags & SX_FLAG_FREE) != 0;
        uint8_t tc = sel ? PAL_WHITE : st->text;
        if (sel) { ui_round_rect(r.x + 4, y - 1, r.w - 8, 12, 4, st->accent); ui_cursor(r.x + 5, y + 1); }
        ui_text(r.x + 16, y + 1, "Free notes", tc);
        ui_text(r.x + 100, y + 1, on ? "< On >" : "< Off >", tc);
        ui_text(r.x + 140, y + 1, on ? "any pitch" : "snap to scale", sel ? PAL_WHITE : st->text_dim);
    }
    y += rh;
    {   /* the way into the Note grid */
        int sel = studio.asel == 1;
        uint8_t tc = sel ? PAL_WHITE : st->text;
        if (sel) { ui_round_rect(r.x + 4, y - 1, r.w - 8, 12, 4, st->accent); ui_cursor(r.x + 5, y + 1); }
        ui_text(r.x + 16, y + 1, "Note grid", tc);
        ui_text(r.x + 100, y + 1, sel ? "A: open it" : "every note, every track", sel ? PAL_WHITE : st->text_dim);
    }
    y += rh + 2;
    ui_text(r.x + 12, y, "Notes in the song", st->text_dim);
    doomo_snprintf(buf, sizeof(buf), "%d played", e.played);
    ui_text(r.x + r.w - 12 - (int)strlen(buf) * 6, y, buf, st->text_dim);
    y += 10;
    for (i = 0; i < nt && y + 8 < r.y + r.h - 38; i++) {         /* two columns: Lead | Harmony, Bass | Drums, ... */
        int cx = r.x + 16 + (i & 1) * (r.w / 2 - 4);
        doomo_snprintf(buf, sizeof(buf), "%s %d", trk_names[i], e.per_trk[i]);
        ui_text(cx, y, buf, st->text);
        if (i & 1) y += 8;
    }
    y = r.y + r.h - 36;
    doomo_snprintf(buf, sizeof(buf), "Parts %d/%d", X->sections, SX_SECTIONS);
    ui_text(r.x + 12, y, buf, st->text_dim);
    bar(bx, y + 1, 64, X->sections, SX_SECTIONS, st->accent, st->fill_lo);
    y += 9;
    doomo_snprintf(buf, sizeof(buf), "Entries %d/%d", X->arrange_len, SX_ARRANGE);
    ui_text(r.x + 12, y, buf, st->text_dim);
    bar(bx, y + 1, 64, X->arrange_len, SX_ARRANGE, st->accent, st->fill_lo);
    y += 9;
    doomo_snprintf(buf, sizeof(buf), "Note blocks %d/%d", blocks, SX_POOL);
    ui_text(r.x + 12, y, buf, st->text_dim);
    bar(bx, y + 1, 64, blocks, SX_POOL, st->accent, st->fill_lo);
    y += 9;
    if (e.page) doomo_snprintf(buf, sizeof(buf), "Save page %d/%d B", e.page, SONGX_PAGE_BYTES);
    else doomo_snprintf(buf, sizeof(buf), "Save page: too big");
    ui_text(r.x + 12, y, buf, e.page ? st->text_dim : st->accent);
    bar(bx, y + 1, 64, e.page ? e.page : SONGX_PAGE_BYTES, SONGX_PAGE_BYTES, st->accent, st->fill_lo);
}

void tab_order_adv_update(void)
{
    b_eaten = 0;
    if (!X->version) return;
    if (X->arrange_len < 1 || studio.osel >= X->arrange_len) studio.osel = 0;
    if (studio.aview == 4) grid_update(); else if (studio.aview == 3) expert_update(); else if (studio.aview == 2) mix_update(); else if (studio.aview) part_update(); else arrange_update();
}

/* ---- draw ------------------------------------------------------------------------------------------------------------- */
static void chip(int x, int y, int w, int sec, int hot)
{
    const SxSection *c = &X->sect[sec];
    char buf[12];
    ui_round_rect(x, y, w, 10, 3, st_pattern_color(c->name, 0));
    ui_round_rect(x + 1, y + 1, w - 2, 8, 2, st_pattern_color(c->name, hot ? 2 : 1));
    doomo_snprintf(buf, sizeof(buf), "%s", songx_name(c->name));
    ui_text(x + 4, y + 1, buf, PAL_WHITE);
    if (songx_sect_users(X, sec) > 1) plat_rect(x + w - 5, y + 4, 2, 2, PAL_WHITE);     /* shared by several entries */
}

static void passes_text(const SxSection *c, char *out, int cap)
{
    int k;
    for (k = 0; k < c->npat && k < cap - 1; k++) out[k] = pat_ch(c->pat[k]);
    out[k] = 0;
}

static void arrange_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int rh = st_wide() ? 15 : 12, vis = (r.h - 40) / rh, i, n = X->arrange_len, total = songx_len(X);
    int po = song_playing() ? song_pos_order() : -1, pe = po >= 0 ? songx_entry_of_pass(X, po) : -1;
    char buf[40];
    uint32_t len = songx_ticks(SS, X) / DOOMO_TICK_HZ;
    if (vis < 3) vis = 3;
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    for (i = 0; i < vis && studio.otop + i < n; i++) {
        int pos = studio.otop + i, sec = X->arrange[pos] % SX_SECTIONS, y = r.y + 5 + i * rh + (rh - 12) / 2, sel = pos == studio.osel, x = r.x;
        const SxSection *c = &X->sect[sec];
        uint8_t tc = sel ? PAL_WHITE : st->text_dim;
        if (sel) {
            ui_round_rect(x + 4, y - 1, r.w - 8, 12, 4, st->accent);
            ui_cursor(x + 5, y + 1);
        }
        doomo_snprintf(buf, sizeof(buf), "%2d", pos + 1);
        ui_text(x + 16, y + 1, buf, tc);
        chip(x + 32, y, 46, sec, pos == pe);
        if (pos == pe) ui_heart(x + 84, y + 5, 3 + (song_beat_age() < 3), PAL_PAINT(PAINT_ROSE, 1));
        passes_text(c, buf, 9);
        ui_text(x + 92, y + 1, buf, sel ? PAL_WHITE : st->text);
        if (c->key) { doomo_snprintf(buf, sizeof(buf), "%+d", c->key); ui_text(x + 146, y + 1, buf, tc); }
        if (c->tempo != 100) { doomo_snprintf(buf, sizeof(buf), "%d%%", c->tempo); ui_text(x + 168, y + 1, buf, tc); }
        if (c->fade) ui_text(x + 196, y + 1, c->fade == SX_FADE_IN ? "in" : "out", tc);
    }
    ui_scroll_arrows(r.x + r.w - 10, r.y + 3, r.y + 5 + vis * rh - 6, studio.otop > 0, studio.otop + vis < n);
    /* the timeline: one block per entry, as wide as its passes; the playhead walks it */
    {
        int bh = st_wide() ? 16 : 10, ty = r.y + r.h - 20 - bh, tw = r.w - 16, x0 = r.x + 8, cum = 0;
        for (i = 0; i < n; i++) {
            int sec = X->arrange[i] % SX_SECTIONS, np = X->sect[sec].npat;
            int xa = x0 + cum * tw / total, xb = x0 + (cum + np) * tw / total, w = DOOMO_MAX(1, xb - xa - 1);
            int lift = (i == pe) ? 2 : 0;
            uint8_t col = st_pattern_color(X->sect[sec].name, i == studio.osel ? 0 : 1);
            if (w >= 4) ui_round_rect(xa, ty - lift, w, bh, 2, col);
            else plat_rect(xa, ty - lift, w, bh, col);
            cum += np;
        }
        if (po >= 0) {
            int px = x0 + ((po * SONG_STEPS + song_pos_step()) * tw) / (total * SONG_STEPS);
            plat_rect(px, ty - 3, 1, bh + 5, PAL_PAINT(PAINT_ROSE, 0));
        }
        doomo_snprintf(buf, sizeof(buf), "%d parts  %d passes  %u:%02u  %s", n, total, (unsigned)(len / 60), (unsigned)(len % 60),
                       (SS->flags & SONGF_LOOP) ? "(loops)" : "");
        ui_text(x0, ty + bh + 3, buf, st->text_dim);
    }
}

static void part_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int rh = st_wide() ? 15 : 12, sec = cur_sec(), rows = part_rows(), vis = (r.h - 24 - rh) / rh, i, ph = song_playing() ? song_pos_order() : -1;
    const SxSection *c = &X->sect[sec];
    char buf[40];
    int u = songx_sect_users(X, sec), ptop = r.y + 6 + rh + 4;
    if (vis < 3) vis = 3;
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    chip(r.x + 12, r.y + 5, 46, sec, 0);
    doomo_snprintf(buf, sizeof(buf), u > 1 ? "used %d times" : "used once", u);
    ui_text(r.x + 66, r.y + 6, buf, st->text_dim);
    for (i = 0; i < vis && studio.atop + i < rows; i++) {
        int row = studio.atop + i, y = ptop + i * rh, sel = row == studio.asel, vx = r.x + 96, pass = row - 4;
        const char *lab;
        uint8_t tc = sel ? PAL_WHITE : st->text;
        if (sel) {
            ui_round_rect(r.x + 4, y - 1, r.w - 8, 12, 4, st->accent);
            ui_cursor(r.x + 5, y + 1);
        }
        if (row == 0) { lab = "Name"; doomo_snprintf(buf, sizeof(buf), "< %s >", songx_name(c->name)); }
        else if (row == 1) {
            lab = "Key change";
            if (c->key) doomo_snprintf(buf, sizeof(buf), "< %+d >", c->key); else doomo_snprintf(buf, sizeof(buf), "< as written >");
        } else if (row == 2) {
            lab = "Tempo";
            doomo_snprintf(buf, sizeof(buf), "< %d%%  %d bpm >", c->tempo, DOOMO_CLAMP(SS->bpm, 60, 220) * c->tempo / 100);
        } else if (row == 3) {
            lab = "Fade";
            doomo_snprintf(buf, sizeof(buf), "< %s >", c->fade == SX_FADE_IN ? "in" : c->fade == SX_FADE_OUT ? "out" : "none");
        } else if (pass < c->npat) {
            char l[2] = { pat_ch(c->pat[pass]), 0 };
            doomo_snprintf(buf, sizeof(buf), "Pass %d", pass + 1);
            lab = buf;
            ui_text(r.x + 16, y + 1, lab, tc);
            ui_round_rect(vx, y, 14, 10, 3, st_pattern_color(c->pat[pass], 0));
            ui_round_rect(vx + 1, y + 1, 12, 8, 2, st_pattern_color(c->pat[pass], 1));
            ui_text(vx + 4, y + 1, l, PAL_WHITE);
            if (ph >= 0 && songx_entry_of_pass(X, ph) == studio.osel && ph - songx_entry_pass(X, studio.osel) == pass)
                ui_heart(vx + 22, y + 5, 3 + (song_beat_age() < 3), PAL_PAINT(PAINT_ROSE, 1));
            continue;
        } else {
            lab = "+ Add a pass";
            buf[0] = 0;
        }
        ui_text(r.x + 16, y + 1, lab, tc);
        if (buf[0] && lab != buf) ui_text(vx, y + 1, buf, tc);
    }
    ui_scroll_arrows(r.x + r.w - 10, ptop - 2, ptop + vis * rh - 8, studio.atop > 0, studio.atop + vis < rows);
}

static void mix_draw(StRect r)
{
    static const char *const tname[SX_TRACKS] = { "Lead", "Harmony", "Bass", "Drums", "Extra 5", "Extra 6", "Extra 7", "Extra 8" };
    static const char *const pan_s[3] = { "Left", "Middle", "Right" };
    static const char *const env_s[4] = { "Soft", "Punchy", "Long", "Pluck" };
    const UiStyle *st = ui_style(WIN_CREAM);
    int rh = st_wide() ? 15 : 12, nt = mix_tracks(), i, y0 = r.y + 6 + rh;
    int cx[3] = { r.x + 78, r.x + 130, r.x + 186 };
    char buf[24];
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    ui_text(r.x + 12, r.y + 5, "Mix", st->text);
    ui_text(cx[0] + 4, r.y + 5, "Pan", mix_col == 0 ? st->accent : st->text_dim);
    ui_text(cx[1] + 4, r.y + 5, "Sound", mix_col == 1 ? st->accent : st->text_dim);
    ui_text(cx[2] + 4, r.y + 5, "Swing", mix_col == 2 ? st->accent : st->text_dim);
    for (i = 0; i <= nt; i++) {
        int y = y0 + i * rh, sel = i == studio.asel;
        uint8_t tc = sel ? PAL_WHITE : st->text;
        if (sel) { ui_round_rect(r.x + 4, y - 1, r.w - 8, 12, 4, st->accent); ui_cursor(r.x + 5, y + 1); }
        if (i == nt) {
            const SxSection *c = &X->sect[cur_sec()];
            ui_text(r.x + 16, y + 1, "Brightness", tc);
            if (c->bright) doomo_snprintf(buf, sizeof(buf), "< %d >", c->bright); else doomo_snprintf(buf, sizeof(buf), "< as set >");
            ui_text(cx[0], y + 1, buf, tc);
            ui_text(cx[1] + 20, y + 1, songx_name(c->name), tc);
            continue;
        }
        ui_text(r.x + 16, y + 1, tname[i], tc);
        {
            int col;
            const char *v[3];
            char sw[8];
            if (X->trk[i].swing) doomo_snprintf(sw, sizeof(sw), "%d", X->trk[i].swing); else doomo_snprintf(sw, sizeof(sw), "song");
            v[0] = pan_s[X->trk[i].pan % 3]; v[1] = env_s[X->trk[i].env & 3]; v[2] = sw;
            for (col = 0; col < 3; col++) {
                int hot = sel && col == mix_col;
                doomo_snprintf(buf, sizeof(buf), hot ? "<%s>" : "%s", v[col]);
                ui_text(cx[col] + (hot ? 0 : 4), y + 1, buf, tc);
            }
        }
        if (i >= songx_tracks_max()) ui_text(r.x + r.w - 40, y + 1, "mixed in", sel ? PAL_WHITE : st->text_dim);
    }
    ui_text(r.x + 12, r.y + r.h - 14, "Sound and light play. Pan, swing: saved.", st->text_dim);
}

void tab_order_adv_draw(StRect r)
{
    if (!X->version) return;
    if (studio.aview == 4) grid_draw(r); else if (studio.aview == 3) expert_draw(r); else if (studio.aview == 2) mix_draw(r); else if (studio.aview) part_draw(r); else arrange_draw(r);
}

int st_adv_prompts(UiPrompt *p)
{
    int n = 0;
#define PR(b, pr, v) do { p[n].btn = (uint16_t)(b); p[n].prio = (uint8_t)(pr); p[n].verb = (v); n++; } while (0)
    if (studio.aview == 4) {
        PR(BTN_A, 0, "Note on / off");
        PR(BTN_L | BTN_R, 1, "Track");
        PR(BTN_START, 2, st_play_verb());
        PR(BTN_SELECT, 3, "Tools");
        PR(BTN_B, 4, "Back");
    } else if (studio.aview == 3) {
        PR(BTN_A | BTN_LEFT | BTN_RIGHT, 0, studio.asel ? "Open" : "Switch");
        PR(BTN_START, 1, st_play_verb());
        PR(BTN_B, 2, "Back");
    } else if (studio.aview == 2) {
        PR(BTN_LEFT | BTN_RIGHT, 0, "Change");
        PR(BTN_A, 1, studio.asel < mix_tracks() ? "Next column" : "More");
        PR(BTN_START, 2, st_play_verb());
        PR(BTN_B, 3, "Back");
    } else if (studio.aview) {
        PR(BTN_LEFT | BTN_RIGHT, 0, "Change");
        PR(BTN_A, 1, studio.asel >= 4 ? "Add a pass" : "Next");
        PR(BTN_START, 2, st_play_verb());
        PR(BTN_SELECT, 3, "Tools");
        PR(BTN_B, 4, "Back");
    } else {
        PR(BTN_A, 0, "Edit part");
        PR(BTN_LEFT | BTN_RIGHT, 1, "Part");
        PR(BTN_START, 2, st_play_verb());
        PR(BTN_SELECT, 3, "Tools");
        PR(BTN_B, 4, "Exit");
    }
#undef PR
    return n;
}

#else  /* !STUDIO_ADV: the GBA and PS1 have no room for an arrangement yet (docs/STUDIO.md "ST-ADV screens") */

int st_adv_on(void) { return 0; }
int st_adv_free(void) { return 0; }
int st_adv_eats_b(void) { return 0; }
int st_adv_grid(void) { return 0; }
int st_adv_box(void) { return 0; }
void st_adv_enter(int slot) { (void)slot; }
void st_adv_take(void) {}
int st_adv_saved(int slot) { (void)slot; return 1; }
void st_adv_play(int pos, int singer) { song_play_at(SS, singer, pos, 0); }
int st_adv_pass_of_cursor(void) { return studio.osel; }
uint32_t st_adv_ticks(void) { return song_length_ticks(SS); }
int st_adv_epic(void) { return 0; }
int st_adv_folded(char *buf, int cap) { (void)buf; (void)cap; return 0; }
int st_adv_line_pass(int line) { (void)line; return -1; }
const SongExt *st_adv_ext(void) { return NULL; }
int st_adv_line_pattern(int line)
{
    int pos = studio_line_pos(SS, line);
    return pos < 0 ? -1 : SS->order[pos] & 3;
}
int st_adv_try_open(void) { return 0; }
void tab_order_adv_update(void) {}
void tab_order_adv_draw(StRect r) { (void)r; }
int st_adv_prompts(UiPrompt *p) { (void)p; return 0; }

#endif
