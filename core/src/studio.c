/* studio.c - SCENE_STUDIO, the Music Studio (owner: studio): tabs, input
 * routing, the undo journal, popup menus, the citizen picker, unlock
 * rules and the drawing helpers the tabs share.
 *
 * CONTROLS (GBA buttons; PS1 extras in brackets)
 *   L / R            previous / next tab (in NOTES: previous / next pattern)
 *   SELECT + L / R   previous / next tab from anywhere            [L2 / R2]
 *   SELECT (tap)     the tab's tools menu (copy / paste, quick fills ...)
 *   SELECT + B       undo                                          [Triangle]
 *   START            play / stop
 *   B                back (leave the studio; in NOTES: erase / tie)
 *
 * UNDO keeps a byte journal of the working song: before an edit the
 * changed bytes are run-length coded into a STUDIO_UNDO_BYTES ring
 * (oldest steps fall out first), so 16+ ordinary edits (a note, a lyric
 * line, a setting) always fit and even whole-song changes usually do.
 */
#include "studio_int.h"
#include <stdarg.h>

Studio studio;

static void studio_enter(int arg);
static void studio_leave(void);
static void studio_update(void);
static void studio_draw(void);

const DoomoScene SCENE_STUDIO = { "STUDIO", studio_enter, studio_leave, studio_update,
                                  studio_draw, 0 };

/* The concert module may hand our songs to its big stage (song.h). */
__attribute__((weak)) int concert_stage_song(const DoomoSong *s)
{
    (void)s;
    return 0;
}

static const char *const tab_names[STAB_COUNT] = {
    "Song", "Notes", "Order", "Lyrics", "Choir", "Stage", "File"
};

/* ====================================================================== */
/* small services                                                           */
int st_wide(void) { return plat_screen_w() >= 300; }

const char *st_name(int id)
{
    const DoomoResident *r;
    if (id == DOOMO_PLAYER_ID) {
        r = &doomo_town.player;
        return r->name[0] ? r->name : "You";
    }
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return "";
    r = &doomo_town.res[id];
    return r->status == RES_EMPTY ? "" : r->name;
}

void st_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    doomo_vsnprintf(studio.msg, sizeof(studio.msg), fmt, ap);
    va_end(ap);
    studio.msg_t = 75;
}

StRect st_content(void)
{
    StRect r;
    r.x = 4;
    r.y = 19;
    r.w = plat_screen_w() - 8;
    r.h = plat_screen_h() - 19 - 15;
    return r;
}

void st_stop(void)
{
    if (song_playing()) song_stop();
}

void st_play_from(int order_pos)
{
    if (song_playing()) { song_stop(); return; }
    song_play_at(SS, -1, order_pos, 0);
}

void st_set_tab(int tab)
{
    tab = (tab + STAB_COUNT) % STAB_COUNT;
    if (tab == studio.tab) return;
    if (studio.tab == STAB_STAGE) stage_leave();
    studio.tab = (uint8_t)tab;
    studio.tab_t = 0;
    studio.a_edit = 0;
    audio_sfx(SFX_CURSOR);
}

/* ====================================================================== */
/* undo journal                                                             */
#define JOIN 0x8000u

static int rle_encode(const uint8_t *src, int n, uint8_t *dst, int cap)
{
    int i = 0, o = 0;
    while (i < n) {
        int run = 1;
        while (i + run < n && src[i + run] == src[i] && run < 129) run++;
        if (run >= 2) {
            if (dst) { if (o + 2 > cap) return -1; dst[o] = (uint8_t)(0x80 | (run - 2)); dst[o + 1] = src[i]; }
            o += 2;
            i += run;
        } else {
            int lit = 1;
            while (i + lit < n && lit < 128 &&
                   !(i + lit + 1 < n && src[i + lit] == src[i + lit + 1])) lit++;
            if (dst) {
                if (o + 1 + lit > cap) return -1;
                dst[o] = (uint8_t)(lit - 1);
                memcpy(dst + o + 1, src + i, (size_t)lit);
            }
            o += 1 + lit;
            i += lit;
        }
    }
    return o;
}

static void rle_decode(const uint8_t *src, int enc, uint8_t *dst, int n)
{
    int i = 0, o = 0;
    while (i < enc && o < n) {
        int c = src[i++];
        if (c & 0x80) {
            int run = (c & 0x7F) + 2;
            uint8_t v = i < enc ? src[i++] : 0;
            while (run-- && o < n) dst[o++] = v;
        } else {
            int lit = c + 1;
            while (lit-- && i < enc && o < n) dst[o++] = src[i++];
        }
    }
}

static uint32_t rd16(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8); }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static void recount(void)
{
    int pos = studio.undo_used, steps = 0;
    while (pos >= 6) {
        const uint8_t *tr = studio.undo + pos - 6;
        if (!(rd16(tr + 2) & JOIN)) steps++;
        pos -= 6 + (int)rd16(tr + 4);
    }
    studio.undo_steps = (uint8_t)DOOMO_MIN(steps, 255);
}

/* drop the oldest record (and the rest of its step) */
static int evict_oldest(void)
{
    int pos = studio.undo_used, first = 0;
    if (!pos) return 0;
    do {
        while (pos > 0) {
            int start = pos - 6 - (int)rd16(studio.undo + pos - 6 + 4);
            if (start <= 0) { first = pos; break; }
            pos = start;
        }
        memmove(studio.undo, studio.undo + first, (size_t)(studio.undo_used - first));
        studio.undo_used = (uint16_t)(studio.undo_used - first);
        pos = studio.undo_used;
        /* the new oldest record joined the one we dropped: drop it too */
        if (!pos) break;
        {
            int p2 = pos, f2 = pos;
            while (p2 > 0) {
                int start = p2 - 6 - (int)rd16(studio.undo + p2 - 6 + 4);
                if (start <= 0) { f2 = p2; break; }
                p2 = start;
            }
            if (!(rd16(studio.undo + f2 - 6 + 2) & JOIN)) break;
        }
    } while (studio.undo_used);
    recount();
    return 1;
}

void ed_reset(void)
{
    studio.undo_used = 0;
    studio.undo_open = 0;
    studio.undo_steps = 0;
}

void ed_touch(const void *field, int len)
{
    const uint8_t *base = (const uint8_t *)&studio.song, *p = (const uint8_t *)field;
    int off, enc;
    if (p < base || len <= 0 || p + len > base + sizeof(DoomoSong)) return;
    off = (int)(p - base);
    enc = rle_encode(p, len, NULL, 0);
    if (enc + 6 > STUDIO_UNDO_BYTES) {
        /* too big to remember: the history before it is useless now */
        ed_reset();
        studio.undo_open = 2;              /* this step cannot be undone */
        return;
    }
    while (studio.undo_used + enc + 6 > STUDIO_UNDO_BYTES) {
        if (!evict_oldest()) break;
    }
    if (studio.undo_open == 2) return;
    rle_encode(p, len, studio.undo + studio.undo_used, STUDIO_UNDO_BYTES - studio.undo_used);
    studio.undo_used = (uint16_t)(studio.undo_used + enc);
    wr16(studio.undo + studio.undo_used, (uint32_t)off);
    wr16(studio.undo + studio.undo_used + 2, (uint32_t)len | (studio.undo_open ? JOIN : 0));
    wr16(studio.undo + studio.undo_used + 4, (uint32_t)enc);
    studio.undo_used = (uint16_t)(studio.undo_used + 6);
    studio.undo_open = 1;
}

void ed_commit(void)
{
    if (studio.undo_open == 1) recount();
    studio.undo_open = 0;
    studio.dirty = 1;
}

int ed_undo_steps(void) { return studio.undo_steps; }

/* Repeated nudges of one setting (tempo +1 +1 +1 ...) make one step. */
static int coal_key = -1;
static uint16_t coal_used;

void ed_touch_key(int key, const void *field, int len)
{
    if (key == coal_key && studio.undo_used == coal_used && !studio.undo_open &&
        studio.undo_used >= 6)
        return;
    ed_touch(field, len);
}

void ed_commit_key(int key)
{
    ed_commit();
    coal_key = key;
    coal_used = studio.undo_used;
}

int ed_undo(void)
{
    if (studio.undo_open) ed_commit();
    if (studio.undo_used < 6) return 0;
    for (;;) {
        uint8_t *tr = studio.undo + studio.undo_used - 6;
        uint32_t off = rd16(tr), ln = rd16(tr + 2), enc = rd16(tr + 4);
        int join = (ln & JOIN) != 0;
        ln &= ~JOIN;
        if (off + ln <= sizeof(DoomoSong) && enc + 6 <= studio.undo_used)
            rle_decode(tr - enc, (int)enc, (uint8_t *)&studio.song + off, (int)ln);
        studio.undo_used = (uint16_t)(studio.undo_used - 6 - enc);
        if (!join || studio.undo_used < 6) break;
    }
    recount();
    studio.dirty = 1;
    return 1;
}

/* ====================================================================== */
/* popups                                                                   */
void st_popup(const char *title, const char *const *items, int n, uint32_t lock,
              StudioPopDone done)
{
    int i;
    n = DOOMO_CLAMP(n, 1, 8);
    studio.pop_title = title;
    for (i = 0; i < n; i++) studio.pop_items[i] = items[i];
    studio.pop_n = (uint8_t)n;
    studio.pop_sel = 0;
    while (studio.pop_sel < n - 1 && (lock & (1u << studio.pop_sel))) studio.pop_sel++;
    studio.pop_lock = lock;
    studio.pop_done = done;
    studio.pop_t = 0;
    audio_sfx(SFX_BLIP);
}

int st_popup_open(void) { return studio.pop_n != 0; }

static void popup_update(void)
{
    int n = studio.pop_n;
    studio.pop_t++;
    if (btn_repeat(BTN_UP)) { studio.pop_sel = (uint8_t)((studio.pop_sel + n - 1) % n); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.pop_sel = (uint8_t)((studio.pop_sel + 1) % n); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A)) {
        StudioPopDone d = studio.pop_done;
        int c = studio.pop_sel;
        if (studio.pop_lock & (1u << c)) { audio_sfx(SFX_ERROR); st_msg("Locked! Keep making music"); return; }
        studio.pop_n = 0;
        audio_sfx(SFX_SELECT);
        if (d) d(c);
    } else if (btn_pressed(BTN_B)) {
        StudioPopDone d = studio.pop_done;
        studio.pop_n = 0;
        audio_sfx(SFX_CANCEL);
        if (d) d(-1);
    }
}

static void popup_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, w = 0, h, x, y;
    const UiStyle *st = ui_style(WIN_CREAM);
    for (i = 0; i < studio.pop_n; i++) w = DOOMO_MAX(w, ui_text_width(studio.pop_items[i]));
    w = DOOMO_MAX(w + 30, ui_text_width(studio.pop_title) + 20);
    h = studio.pop_n * 12 + 24;
    x = (sw - w) / 2;
    y = (sh - h) / 2;
    plat_shade_rect(0, 0, sw, sh, 9);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, studio.pop_t)) return;
    ui_title(sw / 2, y - 5, studio.pop_title, WIN_CUTE);
    for (i = 0; i < studio.pop_n; i++) {
        int yy = y + 13 + i * 12, locked = (studio.pop_lock >> i) & 1;
        if (i == studio.pop_sel) {
            ui_round_rect(x + 5, yy - 2, w - 10, 12, 3, st->accent);
            ui_cursor(x + 7, yy);
        }
        ui_text(x + 18, yy, studio.pop_items[i],
                locked ? st->text_dim : (i == studio.pop_sel ? PAL_WHITE : st->text));
        if (locked) ui_text(x + w - 14, yy, STR_ICON_X, st->text_dim);
    }
}

/* ====================================================================== */
/* citizen picker                                                           */
static struct { const char *title; void (*done)(int id); uint16_t t; } pk;

static int seq_of(int id) { return id == DOOMO_PLAYER_ID ? 0 : id + 1; }
static int id_of(int seq) { return seq == 0 ? DOOMO_PLAYER_ID : seq - 1; }

static int next_alive(int id, int dir)
{
    int s = seq_of(id), k;
    for (k = 0; k < DOOMO_MAX_RESIDENTS + 1; k++) {
        s = (s + dir + DOOMO_MAX_RESIDENTS + 1) % (DOOMO_MAX_RESIDENTS + 1);
        if (st_alive(id_of(s))) return id_of(s);
    }
    return DOOMO_PLAYER_ID;
}

void st_pick_citizen(const char *title, int start_id, void (*done)(int id))
{
    pk.title = title;
    pk.done = done;
    pk.t = 0;
    studio.picking = 1;
    studio.pick_id = (int16_t)(st_alive(start_id) ? start_id : DOOMO_PLAYER_ID);
}

int st_picker_open(void) { return studio.picking; }

void st_picker_update(void)
{
    int k;
    pk.t++;
    if (btn_repeat(BTN_UP)) { studio.pick_id = (int16_t)next_alive(studio.pick_id, -1); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.pick_id = (int16_t)next_alive(studio.pick_id, 1); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_L | BTN_LEFT)) {
        for (k = 0; k < 10; k++) studio.pick_id = (int16_t)next_alive(studio.pick_id, -1);
        audio_sfx(SFX_CURSOR);
    }
    if (btn_repeat(BTN_R | BTN_RIGHT)) {
        for (k = 0; k < 10; k++) studio.pick_id = (int16_t)next_alive(studio.pick_id, 1);
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_A)) {
        studio.picking = 0;
        audio_sfx(SFX_SELECT);
        if (pk.done) pk.done(studio.pick_id);
    } else if (btn_pressed(BTN_B)) {
        studio.picking = 0;
        audio_sfx(SFX_CANCEL);
        if (pk.done) pk.done(-1);
    }
}

static const char *district_of(int id)
{
    if (id == DOOMO_PLAYER_ID) return "You!";
    return RES_DISTRICT(id) ? "Hellvile" : "Hillvile";
}

void st_picker_draw(void)
{
    static const uint8_t sec_paint[SECTION_COUNT] = { PAINT_ROSE, PAINT_PEACH, PAINT_MINT, PAINT_SKY };
    static const char sec_letter[SECTION_COUNT] = { 'S', 'A', 'T', 'B' };
    int sw = plat_screen_w(), sh = plat_screen_h(), wide = st_wide();
    int rows = wide ? 7 : 4, rh = 24, w = wide ? 250 : 220, h = rows * rh + 22, x = (sw - w) / 2, y = (sh - h) / 2 + 2;
    int ids[7], i, mid = rows / 2, id, lw = w - 62;
    const UiStyle *st = ui_style(WIN_CREAM);
    plat_shade_rect(0, 0, sw, sh, 9);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, pk.t)) return;
    ui_title(sw / 2, y - 5, pk.title ? pk.title : "Who sings?", WIN_CUTE);
    ids[mid] = studio.pick_id;
    for (i = mid - 1; i >= 0; i--) ids[i] = next_alive(ids[i + 1], -1);
    for (i = mid + 1; i < rows; i++) ids[i] = next_alive(ids[i - 1], 1);
    for (i = 0; i < rows; i++) {
        int yy = y + 10 + i * rh, sel = i == mid;
        const DoomoResident *r;
        char sl[2] = { 0, 0 };
        id = ids[i];
        r = town_res(id);
        if (sel) {
            ui_round_rect(x + 5, yy, lw, rh - 1, 4, st->accent);
            ui_cursor(x + 6, yy + 8);
        }
        st_chibi(id, x + 16, yy, 0, sel ? (int)(studio.t / 8) & 3 : 0, 0, 0);
        ui_text(x + 36, yy + 3, st_name(id), sel ? PAL_WHITE : st->text);
        ui_text(x + 36, yy + 12, district_of(id), sel ? ui_lighter(st->accent) : st->text_dim);
        if (r) {
            int sec = song_section_of_voice(r->voice);
            sl[0] = sec_letter[sec];
            ui_round_rect(x + lw - 6, yy + 6, 9, 11, 3, PAL_PAINT(sec_paint[sec], 0));
            ui_text(x + lw - 4, yy + 7, sl, PAL_WHITE);
            if (wide) ui_text(x + lw - 12 - ui_text_width(doomo_section_names[sec]), yy + 7,
                              doomo_section_names[sec], sel ? PAL_WHITE : st->text_dim);
        }
    }
    /* the chosen one, big */
    {
        const DoomoImage *img = portrait_image(studio.pick_id, EXPR_HAPPY);
        int px = x + w - 52, py = y + h / 2 - 28 + ui_wave((int)studio.t, 40, 1);
        ui_round_rect(px - 2, py - 2, 52, 60, 6, st->fill_hi);
        if (img) plat_image(px, py, img, 1, 0, 0);
    }
    ui_scroll_arrows(x + lw / 2, y + 6, y + h - 6, 1, 1);
}

/* ====================================================================== */
/* look                                                                     */
uint8_t st_track_color(int t, int shade)
{
    static const uint8_t paints[SONG_TRACKS] = { PAINT_BUBBLEGUM, PAINT_MINT, PAINT_LAVENDER,
                                                 PAINT_HONEY };
    return PAL_PAINT(paints[t & 3], DOOMO_CLAMP(shade, 0, 2));
}

uint8_t st_pattern_color(int p, int shade)
{
    static const uint8_t paints[SONG_PATTERNS] = { PAINT_SKY, PAINT_ROSE, PAINT_LIME,
                                                   PAINT_TANGERINE };
    return PAL_PAINT(paints[p & 3], DOOMO_CLAMP(shade, 0, 2));
}

void st_panel(int x, int y, int w, int h, int style)
{
    ui_window(x, y, w, h, style);
}

void st_chibi(int id, int x, int y, int dir, int frame, int flags, uint8_t col)
{
    const DoomoImage *img = bodyspr_image(id, dir, frame);
    if (img) plat_image(x, y, img, 1, flags, col);
}

void st_note_glyph(int x, int y, uint8_t c)
{
    /* a tiny eighth note: head at (x, y + 5), stem up, flag */
    ui_fill_ellipse(x + 2, y + 6, 2, 1, c);
    plat_rect(x + 3, y, 1, 6, c);
    plat_rect(x + 4, y + 1, 1, 1, c);
    plat_rect(x + 5, y + 2, 1, 2, c);
}

void st_drum_icon(int d, int cx, int cy, int t)
{
    uint8_t dk = PAL_PAINT(PAINT_CHOCOLATE, 0);
    switch (d) {
    case SONG_DRUM_KICK:
        ui_fill_circle(cx, cy, 4, PAL_PAINT(PAINT_CHERRY, 0));
        ui_fill_circle(cx, cy, 3, PAL_PAINT(PAINT_CHERRY, 1));
        ui_fill_circle(cx - 1, cy - 1, 1, PAL_PAINT(PAINT_ROSE, 2));
        break;
    case SONG_DRUM_SNARE:
        ui_fill_ellipse(cx, cy + 1, 4, 3, PAL_PAINT(PAINT_SILVER, 0));
        ui_fill_ellipse(cx, cy, 4, 2, PAL_WHITE);
        plat_rect(cx - 4, cy + 1, 9, 1, PAL_PAINT(PAINT_CHERRY, 1));
        break;
    case SONG_DRUM_HAT:
        ui_fill_ellipse(cx, cy, 4, 1, PAL_PAINT(PAINT_HONEY, 1));
        plat_rect(cx, cy - 3, 1, 3, dk);
        break;
    case SONG_DRUM_OPEN_HAT:
        ui_fill_ellipse(cx, cy - 1, 4, 1, PAL_PAINT(PAINT_HONEY, 2));
        ui_fill_ellipse(cx, cy + 2, 4, 1, PAL_PAINT(PAINT_HONEY, 1));
        break;
    case SONG_DRUM_CLAP:
        ui_sparkle(cx, cy, (t >> 1) & 15, PAL_PAINT(PAINT_PEACH, 2));
        ui_fill_circle(cx, cy, 1, PAL_PAINT(PAINT_PEACH, 1));
        break;
    case SONG_DRUM_TOM:
        ui_fill_ellipse(cx, cy + 1, 3, 3, PAL_PAINT(PAINT_GRAPE, 0));
        ui_fill_ellipse(cx, cy, 3, 2, PAL_PAINT(PAINT_LAVENDER, 2));
        break;
    case SONG_DRUM_CRASH:
        ui_fill_ellipse(cx, cy, 5, 2, PAL_PAINT(PAINT_LEMON, 1));
        ui_sparkle(cx + 3, cy - 2, (t >> 1) & 15, PAL_WHITE);
        break;
    case SONG_DRUM_COWBELL:
        plat_rect(cx - 2, cy - 3, 4, 2, PAL_PAINT(PAINT_HONEY, 2));
        plat_rect(cx - 3, cy - 1, 6, 4, PAL_PAINT(PAINT_HONEY, 1));
        plat_rect(cx, cy + 3, 1, 1, dk);
        break;
    default:
        break;
    }
}

static void tab_icon(int tab, int cx, int cy, uint8_t c)
{
    switch (tab) {
    case STAB_SONG: st_note_glyph(cx - 3, cy - 4, c); break;
    case STAB_NOTES:
        plat_rect(cx - 3, cy - 3, 2, 2, c); plat_rect(cx + 1, cy - 3, 2, 2, c);
        plat_rect(cx - 3, cy + 1, 2, 2, c); plat_rect(cx + 1, cy + 1, 2, 2, c);
        break;
    case STAB_ORDER:
        plat_rect(cx - 3, cy - 3, 7, 2, c); plat_rect(cx - 3, cy, 5, 2, c);
        plat_rect(cx - 3, cy + 3, 6, 1, c);
        break;
    case STAB_LYRICS:
        ui_fill_ellipse(cx, cy - 1, 4, 3, c);
        plat_rect(cx - 2, cy + 2, 2, 2, c);
        break;
    case STAB_CHOIR:
        ui_fill_circle(cx - 3, cy + 1, 2, c); ui_fill_circle(cx + 3, cy + 1, 2, c);
        ui_fill_circle(cx, cy - 2, 2, c);
        break;
    case STAB_STAGE: ui_star(cx, cy, 4, c); break;
    default:
        plat_rect(cx - 3, cy - 4, 7, 8, c);
        plat_rect(cx - 2, cy - 3, 5, 2, PAL_WHITE);
        break;
    }
}

/* the tab strip: icons, the current tab spelled out, play status */
static void draw_tabs(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int i, x = 4, y = 3, wide = 1, b = ui_wave((int)studio.t, 20, 1) > 0, total = 0;
    for (i = 0; i < STAB_COUNT; i++) total += ui_text_width(tab_names[i]) + 22;
    if (total > plat_screen_w() - 40) wide = 0;     /* names only on the current tab */
    for (i = 0; i < STAB_COUNT; i++) {
        int sel = i == studio.tab, w;
        const char *nm = tab_names[i];
        w = (sel || wide) ? ui_text_width(nm) + 20 : 17;
        if (sel) {
            ui_round_rect(x + 1, y + 1 - b, w, 14, 5, ui_darker(ui_darker(st->accent)));
            ui_round_rect(x, y - 1 - b, w, 14, 5, ui_darker(st->accent));
            ui_round_rect(x + 1, y - b, w - 2, 12, 4, st->accent);
            plat_rect(x + 4, y + 1 - b, w - 8, 1, ui_lighter(st->accent));
            tab_icon(i, x + 8, y + 5 - b, PAL_WHITE);
            ui_text_shadow(x + 15, y + 2 - b, nm, PAL_WHITE, ui_darker(st->accent));
        } else {
            ui_round_rect(x, y + 1, w, 12, 4, st->outline);
            ui_round_rect(x + 1, y + 2, w - 2, 10, 3, st->fill_hi);
            tab_icon(i, x + 8, y + 7, st->text_dim);
            if (wide) ui_text(x + 15, y + 3, nm, st->text_dim);
        }
        x += w + 2;
    }
    /* right side: playing indicator / tempo heart */
    {
        int sw = plat_screen_w(), age = song_beat_age();
        int beat = song_playing() ? (age < 3) : ((int)(studio.t * SS->bpm / 1800) & 1);
        char buf[12];
        int rx = sw - 4;
        doomo_snprintf(buf, sizeof(buf), "%d", SS->bpm);
        rx -= ui_text_width(buf);
        if (rx > x + 12) {
            ui_text_outline(rx, y + 3, buf, PAL_WHITE, st->outline);
            ui_heart(rx - 7, y + 7, beat ? 5 : 4, song_playing() ? PAL_PAINT(PAINT_ROSE, 1)
                                                                 : PAL_PAINT(PAINT_BUBBLEGUM, 2));
        }
    }
}

/* floating notes behind everything */
static void draw_backdrop(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i;
    uint32_t t = studio.t;
    ui_backdrop(WIN_CUTE, t);
    for (i = 0; i < 7; i++) {
        int x = (int)((i * 53 + 17) % sw) + ui_wave((int)t + i * 40, 90, 6);
        int y = sh - (int)((t / 2 + (uint32_t)i * 37) % (uint32_t)(sh + 20));
        st_note_glyph(x, y, PAL_PAINT(i & 1 ? PAINT_LAVENDER : PAINT_ROSE, 2));
    }
}

static void draw_msg(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), w, x, y, age;
    if (!studio.msg_t) return;
    age = 75 - studio.msg_t;
    w = ui_text_width(studio.msg) + 16;
    x = (sw - w) / 2;
    y = sh - 34;
    if (studio.msg_t < 8) y += (8 - studio.msg_t) * 2;
    if (!ui_window_pop(x, y, w, 15, WIN_MINT, age)) return;
    ui_text(x + 8, y + 4, studio.msg, ui_style_text(WIN_MINT));
}

/* ====================================================================== */
/* unlocks                                                                  */
int studio_wave_ok(int wave)
{
    if (wave == WAVE_PULSE12) return unlock_has(UL_STUDIO_WAVE_TINY);
    if (wave == WAVE_SAW) return unlock_has(UL_STUDIO_WAVE_SAW);
    if (wave == WAVE_NOISE) return unlock_has(UL_STUDIO_WAVE_STATIC);
    return 1;
}

int studio_kit_ok(int kit)
{
    switch (kit) {
    case SONG_KIT_TOY: return unlock_has(UL_STUDIO_KIT_TOY);
    case SONG_KIT_BONES: return unlock_has(UL_STUDIO_KIT_BONES);
    case SONG_KIT_808: return unlock_has(UL_STUDIO_KIT_808);
    case SONG_KIT_FORGE: return unlock_has(UL_STUDIO_KIT_FORGE);
    default: return 1;
    }
}

int studio_layout_ok(int layout)
{
    return layout != CHOIR_PYRAMID || unlock_has(UL_STUDIO_LAYOUT_PYRAMID);
}

int studio_template_ok(int t)
{
    int u = studio_template_unlock(t);
    return u < 0 || unlock_has(u);
}

static int popcount16(unsigned v) { int n = 0; while (v) { v &= v - 1; n++; } return n; }

void studio_check_unlocks(void)
{
    const StudioStats *s = &studio_stats;
    if (s->saved >= 1) unlock_grant(UL_STUDIO_WAVE_TINY);
    if (s->saved >= 2) unlock_grant(UL_STUDIO_KIT_TOY);
    if (s->saved >= 3) unlock_grant(UL_STUDIO_TPL_SHANTY);
    if (s->performed >= 1) unlock_grant(UL_STUDIO_WAVE_SAW);
    if (s->performed >= 2) unlock_grant(UL_STUDIO_KIT_808);
    if (s->performed >= 3) unlock_grant(UL_STUDIO_TPL_DISCO);
    if (s->inspired >= 1) unlock_grant(UL_STUDIO_KIT_BONES);
    if (s->inspired >= 3) unlock_grant(UL_STUDIO_TPL_JAZZ);
    if (s->lines >= 20) unlock_grant(UL_STUDIO_WAVE_STATIC);
    if (s->encores >= 1) unlock_grant(UL_STUDIO_TPL_SYNTH);
    if (popcount16(s->genres) >= 4) unlock_grant(UL_STUDIO_KIT_FORGE);
    if (s->best_choir >= 10) unlock_grant(UL_STUDIO_LAYOUT_PYRAMID);
    if (s->best_choir >= SONG_SINGERS) unlock_grant(UL_STUDIO_TPL_FORGE);
}

/* ====================================================================== */
/* the scene                                                                */
void studio_edit(const DoomoSong *s)
{
    if (!s) return;
    studio.song = *s;
    song_sanitize(&studio.song);
    studio.song.flags |= SONGF_VALID | SONGF_CUSTOM;
    scene_push(&SCENE_STUDIO, -3);
}

static void new_song(void)
{
    song_clear(SS);
    studio_choir_fill(SS, FILL_CLEAR, -1, 0);
    SS->singer[0] = DOOMO_PLAYER_ID;
    studio.slot = -1;
}

static void studio_enter(int arg)
{
    studio.active = 1;
    studio.t = 0;
    studio.tab_t = 0;
    studio.msg_t = 0;
    studio.pop_n = 0;
    studio.picking = 0;
    studio.show = 0;
    studio.sel_held = studio.sel_used = 0;
    studio.prev_music = (int16_t)audio_music_current();
    audio_music(SONG_NONE);
    if (arg <= STUDIO_ARG_DEMO(0) && arg > STUDIO_ARG_DEMO(STUDIO_TEMPLATES)) {
        /* runner / tests: premade citizens move in, a template is loaded */
        if (!town_count(-1)) town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
        studio_template_load(SS, STUDIO_ARG_DEMO(0) - arg);
        studio.slot = -1;
        studio.tab = STAB_SONG;
        arg = -2;
        ed_reset();
    } else if (arg >= 0 && arg < SONG_BOOK_SIZE) {
        if (doomo_song_book[arg].flags & SONGF_VALID) {
            studio.song = doomo_song_book[arg];
            studio.slot = (int8_t)arg;
        } else {
            new_song();
        }
        studio.tab = STAB_SONG;
    } else if (arg == -3) {
        studio.slot = -1;                  /* studio_edit() copied it in */
        studio.tab = STAB_SONG;
    } else if (arg == -2 || !(studio.song.flags & SONGF_VALID)) {
        new_song();
        studio.tab = STAB_FILE;
    }
    song_sanitize(SS);
    if (arg != -1) {
        ed_reset();
        studio.dirty = arg == -3;
        studio.pat = studio.trk = studio.stp = studio.ntop = 0;
        studio.osel = studio.otop = studio.lsel = studio.ltop = 0;
        studio.csel = 0;
        studio.fsel = studio.ftop = 0;
        studio.srow = studio.stop = 0;
    }
    if (!studio.last[0]) {
        studio.last[0] = 72; studio.last[1] = 64; studio.last[2] = 48;
        studio.last[3] = SONG_DRUM_KICK;
    }
    studio.clip_pat = studio.clip_trk = -1;
    studio.muse = -1;
    studio.sing_pos = -1;
    studio.fmode = 0;
}

static void studio_leave(void)
{
    song_stop();
    stage_leave();
    studio.active = 0;
    audio_music(studio.prev_music);
}

static void leave_now(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

static void leave_choice(int c)
{
    if (c == 1) leave_now();
}

void studio_try_leave(void)
{
    static const char *const items[2] = { "Keep writing", "Leave (unsaved!)" };
    if (studio.dirty) { st_popup("Leave the studio?", items, 2, 0, leave_choice); return; }
    leave_now();
}

static void global_input(void)
{
    /* SELECT: chords while held, the tab's tools on a clean tap */
    if (btn_pressed(BTN_SELECT)) { studio.sel_held = 1; studio.sel_used = 0; }
    if (studio.sel_held) {
        if (btn_pressed(BTN_L)) { st_set_tab(studio.tab - 1); studio.sel_used = 1; }
        if (btn_pressed(BTN_R)) { st_set_tab(studio.tab + 1); studio.sel_used = 1; }
        if (btn_pressed(BTN_B | BTN_Y | BTN_A | BTN_X | BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))
            studio.sel_used = 1;
    }
    if (btn_pressed(BTN_Y) || (studio.sel_held && btn_pressed(BTN_B))) {
        if (ed_undo()) { st_msg("Undo! (%d left)", ed_undo_steps()); audio_sfx(SFX_CANCEL); }
        else { st_msg("Nothing to undo"); audio_sfx(SFX_ERROR); }
    }
    if (btn_pressed(BTN_L2)) st_set_tab(studio.tab - 1);
    if (btn_pressed(BTN_R2)) st_set_tab(studio.tab + 1);
}

int studio_sel_tap;

static void studio_update(void)
{
    studio.t++;
    if (studio.tab_t < 60000) studio.tab_t++;
    if (studio.msg_t) studio.msg_t--;
    song_tick();
    studio_sel_tap = 0;
    if (studio.picking) { st_picker_update(); return; }
    if (studio.pop_n) { popup_update(); return; }
    if (studio.tab == STAB_STAGE && stage_fullscreen()) { tab_stage_update(); return; }
    global_input();
    if (btn_released(BTN_SELECT) && studio.sel_held) {
        studio.sel_held = 0;
        if (!studio.sel_used) studio_sel_tap = 1;
    }
    if (!btn_down(BTN_SELECT)) studio.sel_held = 0;
    if (studio.sel_held) return;                /* chords only while SELECT is down */
    if (studio.tab != STAB_NOTES) {
        if (btn_pressed(BTN_L)) { st_set_tab(studio.tab - 1); return; }
        if (btn_pressed(BTN_R)) { st_set_tab(studio.tab + 1); return; }
    }
    switch (studio.tab) {
    case STAB_SONG: tab_song_update(); break;
    case STAB_NOTES: tab_notes_update(); break;
    case STAB_ORDER: tab_order_update(); break;
    case STAB_LYRICS: tab_lyrics_update(); break;
    case STAB_CHOIR: tab_choir_update(); break;
    case STAB_STAGE: tab_stage_update(); break;
    default: tab_file_update(); break;
    }
    if (studio.tab != STAB_NOTES && !studio.pop_n && !studio.picking && btn_pressed(BTN_B) &&
        !(studio.tab == STAB_STAGE && stage_fullscreen()) && scene_top() == &SCENE_STUDIO)
        studio_try_leave();
}

static void studio_draw(void)
{
    StRect r;
    if (studio.tab == STAB_STAGE && stage_fullscreen()) {
        stage_draw_full();
        draw_msg();
        if (studio.pop_n) popup_draw();
        return;
    }
    draw_backdrop();
    draw_tabs();
    r = st_content();
    switch (studio.tab) {
    case STAB_SONG: tab_song_draw(r); break;
    case STAB_NOTES: tab_notes_draw(r); break;
    case STAB_ORDER: tab_order_draw(r); break;
    case STAB_LYRICS: tab_lyrics_draw(r); break;
    case STAB_CHOIR: tab_choir_draw(r); break;
    case STAB_STAGE: tab_stage_draw(r); break;
    default: tab_file_draw(r); break;
    }
    ui_hint(tab_hint(studio.tab), WIN_CUTE);
    draw_msg();
    if (studio.pop_n) popup_draw();
    if (studio.picking) st_picker_draw();
}
