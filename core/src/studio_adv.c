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
 * Builds with no room for arrangement pages (GBA, PS1: SONGX_BOOK_PAGES 0) compile only the no-op versions at the end.
 */
#include "studio_int.h"

#if STUDIO_ADV

#define X (&studio.xw)
/* the bytes of a SongExt an arrangement edit can change: header, the 16 parts and the 64 entries (the pool and the cell map
 * are the note grid's, not touched here) */
#define META ((int)(offsetof(SongExt, arrange) + SX_ARRANGE))
#define AK(r) (0x4000 + (r))            /* coalescing keys: repeated nudges of one setting are one undo step */

static uint8_t b_eaten;                 /* B left the part view this frame: the studio must not take it as "leave" */

int st_adv_on(void) { return studio.xw.version != 0; }
int st_adv_eats_b(void) { return b_eaten; }

void st_adv_enter(int slot)
{
    studio.aview = 0; studio.asel = 0; studio.atop = 0;
    studio.osel = studio.otop = 0;
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
    if (pass >= c->npat) return 0;
    c->pat[pass] = (uint8_t)(((c->pat[pass] & 3) + d + 4) & 3);
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
static int e_chorus_up(SongExt *x, int steps, int unused) { (void)unused; return songx_last_chorus_up(x, steps); }

/* ---- opening: the classic order becomes the first arrangement ---------------------------------------------------------- */
static int e_from_order(SongExt *x, int a, int b)
{
    (void)a; (void)b;
    songx_from_order(x, SS);
    return 1;
}

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
        static const char *const items[8] = { "Add a pass", "Delete this pass", "Make it unique", "Name...", "Key change...",
                                              "Tempo change...", "Fade...", "Hear this part" };
        static const char *const help[8] = { "A copy of the pass, just after.", "Take the pass out.",
                                             "Its own copy: edits stay here.", "Verse, chorus, bridge...",
                                             "Lift or drop the whole part.", "Faster or slower part.",
                                             "Fade the part in or out.", "Play from this part." };
        st_popup("This part", items, 8, 0, part_tools);
        st_popup_help(help);
    }
    if (vis < 3) vis = 3;
    if (studio.asel < studio.atop) studio.atop = studio.asel;
    if (studio.asel >= studio.atop + vis) studio.atop = (uint8_t)(studio.asel - vis + 1);
}

void tab_order_adv_update(void)
{
    b_eaten = 0;
    if (!X->version) return;
    if (X->arrange_len < 1 || studio.osel >= X->arrange_len) studio.osel = 0;
    if (studio.aview) part_update(); else arrange_update();
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
    for (k = 0; k < c->npat && k < cap - 1; k++) out[k] = (char)('A' + (c->pat[k] & 3));
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
            char l[2] = { (char)('A' + (c->pat[pass] & 3)), 0 };
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

void tab_order_adv_draw(StRect r)
{
    if (!X->version) return;
    if (studio.aview) part_draw(r); else arrange_draw(r);
}

int st_adv_prompts(UiPrompt *p)
{
    int n = 0;
#define PR(b, pr, v) do { p[n].btn = (uint16_t)(b); p[n].prio = (uint8_t)(pr); p[n].verb = (v); n++; } while (0)
    if (studio.aview) {
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
int st_adv_eats_b(void) { return 0; }
void st_adv_enter(int slot) { (void)slot; }
void st_adv_take(void) {}
int st_adv_saved(int slot) { (void)slot; return 1; }
void st_adv_play(int pos, int singer) { song_play_at(SS, singer, pos, 0); }
int st_adv_pass_of_cursor(void) { return studio.osel; }
uint32_t st_adv_ticks(void) { return song_length_ticks(SS); }
int st_adv_try_open(void) { return 0; }
void tab_order_adv_update(void) {}
void tab_order_adv_draw(StRect r) { (void)r; }
int st_adv_prompts(UiPrompt *p) { (void)p; return 0; }

#endif
