/* lib_jukebox.c - SCENE_LIB_JUKEBOX: Jukebox & Sound Test (owner: library).
 *
 * Unlocked by MODE_JUKEBOX (unlocks_library.c). The SONGS shelf plays any
 * SONG_* on a spinning record with a four-channel level meter fed by the
 * real sequencer (audio_channel_state) and a Doomogachi dancing to the
 * beat; the SOUND FX shelf plays every SFX_* and draws its waveform from
 * the PCM. In Story Mode a song has to be heard first (its jukebox unlock
 * from unlocks_story.c); in free play every song is on the shelf. The
 * music that was playing comes back when you leave.
 */
#include "lib_int.h"

#define J (lib_w.juke)
enum { TAB_SONGS, TAB_SFX, N_TABS };
static const char *const tab_names[N_TABS] = { "Songs", "Sound FX" };

static const char *const song_notes[] = {
    "",
    "The theme! Hum it and a Doomogachi hatches somewhere.",
    "Sunshine, sprinkles and a bassline that skips.",
    "Fireflies, porch lights and sleepy bunnies.",
    "Make a friend! Or forty. The tune won't judge.",
    "Mintworth's till goes ka-ching right on the beat.",
    "Father Fuzz wrote it. The choir is mostly plushies.",
    "Beep. Boop. Beep. Feed me. Beep.",
    "Saxophone, fog and somebody's bad decisions.",
    "Ominous chanting, but make it catchy.",
    "Clang! Hiss! Rivet sings along, badly.",
    "The day the music turned metal.",
    "Shotgun shells for a drum kit.",
    "Three phases. Three key changes. Zero chill.",
    "Ta-daa! Short, sweet and a little smug.",
    "For goodbyes. Have a tissue. Have two.",
    "Everybody, both sides of the Wall, dancing at last.",
    "A thousand citizens, one very long parade.",
    "Too smooth. Too purple. Don't trust it.",
    "The Wall hums this at night. Listen closely.",
};

static int song_ok(int s)
{
    if (s <= SONG_NONE || s >= SONG_COUNT) return 0;
    if (lib_b.demo || doomo_town.mode != TOWN_STORY) return 1;
    return unlock_ok(UL_CAT_GALLERY, GAL_JUKEBOX, s);
}

int lib_jukebox_pct(void)
{
    int s, n = 0, have = 0;
    if (!library_wing_open(LIB_WING_JUKEBOX)) return 0;
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++) {
        n++;
        if (doomo_town.mode != TOWN_STORY || unlock_ok(UL_CAT_GALLERY, GAL_JUKEBOX, s)) have++;
    }
    lib_have = (int16_t)have;
    lib_total = (int16_t)n;
    return lib_pct(have, n);
}

static int sfx_tab(void) { return lib_b.tab == TAB_SFX; }

static void rebuild(void)
{
    lib_b.count = (int16_t)(sfx_tab() ? SFX_COUNT - 1 : SONG_COUNT - 1);
    lib_clamp();
}

static int entry(int i) { return i + 1; }      /* skip SONG_NONE / SFX_NONE */

static int bits_count(uint32_t v)
{
    int n = 0;
    while (v) { n += (int)(v & 1); v >>= 1; }
    return n;
}

/* Waveform columns (min / max) of the selected sound. */
static void build_wave(int sfx)
{
    uint32_t len = 0, rate = 0, i;
    const int8_t *pcm;
    int c;
    if (J.wave_for == sfx) return;
    J.wave_for = (int16_t)sfx;
    memset(J.lo, 0, sizeof(J.lo));
    memset(J.hi, 0, sizeof(J.hi));
    pcm = audio_sfx_pcm(sfx, &len, &rate);
    if (!pcm || len == 0) return;
    for (c = 0; c < LIB_WAVE_COLS; c++) {
        uint32_t a = len * (uint32_t)c / LIB_WAVE_COLS, b = len * (uint32_t)(c + 1) / LIB_WAVE_COLS;
        int lo = 0, hi = 0;
        if (b <= a) b = a + 1;
        for (i = a; i < b && i < len; i += 2) {          /* every other sample */
            if (pcm[i] < lo) lo = pcm[i];
            if (pcm[i] > hi) hi = pcm[i];
        }
        J.lo[c] = (int8_t)lo;
        J.hi[c] = (int8_t)hi;
    }
}

/* ---- scene ------------------------------------------------------------------ */
static uint8_t juke_pct;

static void juke_enter(int arg)
{
    lib_wing_enter(LIB_WING_JUKEBOX, arg);
    if (lib_b.tab >= N_TABS) lib_b.tab = 0;
    J.prev_song = (uint8_t)audio_music_current();
    J.playing = 0;
    J.wave_for = -1;
    juke_pct = (uint8_t)lib_jukebox_pct();
    rebuild();
    /* start on the song that is playing */
    if (!sfx_tab() && J.prev_song > SONG_NONE && J.prev_song < SONG_COUNT) {
        lib_b.sel = (int16_t)(J.prev_song - 1);
        lib_clamp();
    }
}

static void juke_leave(void)
{
    if (scene_scratch_mine(LIB_SCRATCH_TAG)) audio_music(J.prev_song);
}

static void play_selected(void)
{
    int e = entry(lib_b.sel);
    if (sfx_tab()) {
        audio_sfx(e);
        J.last_sfx = (uint8_t)e;
        J.play_t = 0;
        J.sfx_played[e >> 5] |= 1u << (e & 31);
        return;
    }
    if (!song_ok(e)) { audio_sfx(SFX_ERROR); return; }
    if (J.playing == e) {                  /* press again: stop */
        audio_music(SONG_NONE);
        J.playing = 0;
        return;
    }
    audio_music(e);
    J.playing = (uint8_t)e;
    J.play_t = 0;
    J.played |= 1u << e;
    if (bits_count(J.played) >= 10) unlock_grant(LIB_UL_DJ);
}

static void juke_update(void)
{
    if (lib_w_lost(1)) return;
    if (J.play_t < 60000) J.play_t++;
    J.spin = (uint8_t)(J.spin + (J.playing ? 1 : 0));
    switch (lib_nav(N_TABS, 0)) {
    case LIBE_TAB: rebuild(); break;
    case LIBE_OPEN:
        /* A plays instead of opening a page */
        lib_b.detail = 0;
        play_selected();
        break;
    case LIBE_FILTER:          /* SELECT: stop the music */
        audio_music(SONG_NONE);
        J.playing = 0;
        break;
    case LIBE_BACK: lib_wing_leave(); break;
    default: break;
    }
}

/* ---- drawing -------------------------------------------------------------------- */
static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_DARK);
    int e = entry(i);
    char buf[48];
    (void)sel;
    if (sfx_tab()) {
        int heard = (J.sfx_played[e >> 5] >> (e & 31)) & 1;
        doomo_snprintf(buf, sizeof(buf), "SE-%02d", e);
        ui_text(x, y + 2, buf, st->text_dim);
        ui_text(x + 36, y + 2, lib_fit(audio_sfx_name(e), w - 44, buf, sizeof(buf)), st->text);
        if (heard) plat_text(x + w - 6, y + 2, STR_ICON_CHECK, PAL_PAINT(PAINT_LIME, 1));
        return;
    }
    if (!song_ok(e)) {
        doomo_snprintf(buf, sizeof(buf), "%02d", e);
        ui_text(x, y + 2, buf, st->text_dim);
        ui_text(x + 16, y + 2, "? ? ? ? ?", st->text_dim);
        lib_padlock(x + w - 8, y + 2, st->text_dim);
        return;
    }
    doomo_snprintf(buf, sizeof(buf), "%02d", e);
    ui_text(x, y + 2, buf, J.playing == e ? PAL_UI_GOLD : st->text_dim);
    ui_text(x + 16, y + 2, lib_fit(audio_song_name(e), w - 26, buf, sizeof(buf)), st->text);
    if (J.playing == e)
        plat_text(x + w - 6, y + 2 - ((lib_b.t / 8) & 1), STR_ICON_NOTE, PAL_UI_GOLD);
}

static void record(int cx, int cy, int r, int song, int playing)
{
    uint8_t lab = PAL_PAINT((song * 7 + 4) % PAL_PAINT_COUNT, 1);
    int a = J.spin * 6, k;
    ui_fill_circle(cx + 1, cy + 2, r, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_circle(cx, cy, r, PAL_GRAY_D);
    ui_fill_circle(cx, cy, r - 1, PAL_BLACK);
    for (k = r - 4; k > r / 2; k -= 3) {
        ui_fill_circle(cx, cy, k, PAL_RAMP(RAMP_STONE, 1));
        ui_fill_circle(cx, cy, k - 1, PAL_BLACK);
    }
    ui_fill_circle(cx, cy, r / 2 - 2, ui_darker(lab));
    ui_fill_circle(cx, cy, r / 2 - 3, lab);
    ui_fill_circle(cx, cy, 2, PAL_WHITE);
    /* a glint sweeping round as it spins */
    {
        int gx = ui_wave(a, 256, r - 4), gy = ui_wave(a + 64, 256, r - 4);
        plat_rect(cx + gx, cy + gy, 2, 2, PAL_GRAY);
        plat_rect(cx + gx / 2, cy + gy / 2, 1, 1, PAL_GRAY_D);
        gx = ui_wave(a, 256, r / 2 - 5);
        gy = ui_wave(a + 64, 256, r / 2 - 5);
        plat_rect(cx + gx, cy + gy, 2, 2, PAL_WHITE);
    }
    /* the tonearm */
    {
        int px = cx + r + 4, py = cy - r + 2;
        int nx = playing ? cx + r / 2 + 2 : cx + r + 2, ny = playing ? cy + 2 : cy - r / 2 + 6;
        ui_fill_circle(px, py, 4, PAL_RAMP(RAMP_STEEL, 3));
        ui_fill_circle(px, py, 2, PAL_RAMP(RAMP_STEEL, 6));
        plat_line(px, py, nx, ny, PAL_RAMP(RAMP_STEEL, 6));
        plat_line(px + 1, py, nx + 1, ny, PAL_RAMP(RAMP_STEEL, 4));
        plat_rect(nx - 1, ny - 1, 4, 3, PAL_RAMP(RAMP_STEEL, 5));
    }
}

static void meters(int x, int y, int w, int h)
{
    int ch, bw = (w - 6) / 4;
    static const uint8_t cols[4] = { PAINT_BUBBLEGUM, PAINT_SKY, PAINT_LIME, PAINT_LEMON };
    for (ch = 0; ch < 4; ch++) {
        DoomoToneState ts;
        int bh, bx = x + ch * (bw + 2), seg;
        audio_channel_state(ch, &ts);
        bh = ts.freq ? (int)ts.vol * h / 15 : 0;
        ui_round_rect(bx, y, bw, h, 2, PAL_RAMP(RAMP_STONE, 1));
        for (seg = 0; seg < bh; seg += 3) {
            uint8_t c = seg > h * 3 / 4 ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(cols[ch], 1);
            plat_rect(bx + 1, y + h - 2 - seg, bw - 2, 2, c);
        }
    }
}

static void song_panel(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_DARK);
    int e = entry(lib_b.sel), big = lib_big(), r = big ? 34 : 24;
    int cx = x + r + 8, cy = y + r + 6, ok = song_ok(e);
    char buf[48];
    record(cx, cy, r, ok ? e : 0, J.playing == e);
    /* the dancing Doomogachi */
    {
        int beat = J.playing ? (int)(audio_music_ticks() / 8) : (int)(lib_b.t / 30);
        const DoomoImage *img = sprite_image(SPR_PET, beat & 3);
        int px = x + w - 30, hop = J.playing ? ((beat & 1) ? 4 : 0) : 0;
        if (img) {
            ui_fill_ellipse(px + img->w / 2, y + 2 * r - 2, 10, 2, PAL_RAMP(RAMP_STONE, 0));
            plat_image(px, y + 2 * r - 2 - img->h - hop, img, 1, (beat & 2) ? IMG_FLIPX : 0, 0);
        }
        if (J.playing) {
            int k;
            for (k = 0; k < 3; k++) {
                int t = (int)((J.play_t + k * 20) % 60);
                plat_text(px - 6 + k * 9 + ui_wave(t, 30, 2), y + 2 * r - 24 - t / 2, STR_ICON_NOTE,
                          PAL_PAINT((k * 9 + 7) % PAL_PAINT_COUNT, 2));
            }
        }
    }
    {
        int ty = y + 2 * r + 12;
        ui_text_shadow(x + 6, ty, ok ? lib_fit(audio_song_name(e), w - 12, buf, sizeof(buf)) : "Locked song",
                       st->accent, PAL_BLACK);
        ty += 11;
        if (ok) {
            uint32_t len = audio_song_length(e), at = J.playing == e ? audio_music_ticks() % (len ? len : 1) : 0;
            doomo_snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d", (int)(at / 30 / 60), (int)(at / 30 % 60),
                           (int)(len / 30 / 60), (int)(len / 30 % 60));
            ui_text(x + 6, ty, buf, st->text_dim);
            ui_bar(x + 6, ty + 10, w - 12, 5, (int)at, (int)(len ? len : 1), PAL_PAINT(PAINT_LAVENDER, 1));
            ty += 19;
            if (ty + 2 * FONT_LINE < y + h - (big ? 34 : 4) && e < (int)DOOMO_COUNTOF(song_notes))
                ui_text_wrap(x + 6, ty, w - 12, song_notes[e], st->text, 1);
        } else {
            const DoomoUnlock *u = unlock_find(UL_CAT_GALLERY, GAL_JUKEBOX, e);
            ui_text_wrap(x + 6, ty, w - 12, u ? u->how : "Keep playing!", st->text_dim, 1);
        }
    }
    if (big) meters(x + 6, y + h - 30, w - 12, 24);
    else meters(x + 2 * r + 18, y + 6, 26, 2 * r - 12);
}

static void sfx_panel(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_DARK);
    int e = entry(lib_b.sel), c, wh = lib_big() ? 60 : 40, wy = y + 18, cols = LIB_WAVE_COLS;
    int cw = (w - 12) / cols, ox, flash = J.last_sfx == e && J.play_t < 30;
    uint32_t len = 0, rate = 0;
    char buf[48];
    if (cw < 1) cw = 1;
    ox = x + (w - cw * cols) / 2;
    if (lib_b.rest >= 2) build_wave(e);
    audio_sfx_pcm(e, &len, &rate);
    ui_text_shadow(x + 6, y + 5, lib_fit(audio_sfx_name(e), w - 12, buf, sizeof(buf)), st->accent, PAL_BLACK);
    ui_round_rect(x + 5, wy, w - 10, wh, 3, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(x + 7, wy + wh / 2, w - 14, 1, PAL_RAMP(RAMP_STONE, 2));
    if (J.wave_for == e) {
        for (c = 0; c < cols; c++) {
            int lo = J.lo[c] * (wh / 2 - 2) / 128, hi = J.hi[c] * (wh / 2 - 2) / 128;
            int played = flash && c * 30 < (int)J.play_t * cols;
            uint8_t col = played ? PAL_UI_GOLD : PAL_PAINT(PAINT_LIME, 1);
            if (hi - lo < 1) hi = lo + 1;
            plat_rect(ox + c * cw, wy + wh / 2 - hi, cw, hi - lo, col);
        }
    }
    doomo_snprintf(buf, sizeof(buf), "%u ms  %u Hz", (unsigned)(rate ? len * 1000u / rate : 0), (unsigned)rate);
    ui_text(x + 6, wy + wh + 4, buf, st->text_dim);
    doomo_snprintf(buf, sizeof(buf), "Heard %d of %d", bits_count(J.sfx_played[0]) + bits_count(J.sfx_played[1]),
                   SFX_COUNT - 1);
    if (wy + wh + 4 + FONT_LINE * 2 < y + h) ui_text(x + 6, wy + wh + 4 + FONT_LINE, buf, st->text_dim);
    /* a little speaker that thumps while the sound plays */
    if (wy + wh + 4 + FONT_LINE * 2 + 40 < y + h) {
        int sx = x + w / 2, sy = y + h - 26, pump = flash ? ((J.play_t / 2) & 1) * 2 : 0, r = 10 + pump;
        ui_round_rect(sx - 18, sy - 18, 36, 38, 4, PAL_RAMP(RAMP_WOOD, 1));
        ui_round_rect(sx - 17, sy - 17, 34, 36, 3, PAL_RAMP(RAMP_WOOD, 3));
        ui_fill_circle(sx, sy + 4, r, PAL_GRAY_D);
        ui_fill_circle(sx, sy + 4, r - 3, PAL_BLACK);
        ui_fill_circle(sx, sy + 4, 3, PAL_GRAY);
        ui_fill_circle(sx, sy - 11, 3, PAL_GRAY_D);
        if (flash) {
            plat_text(sx + 22, sy - 6 - (J.play_t & 3), STR_ICON_NOTE, PAL_UI_GOLD);
            plat_text(sx - 28, sy - 2 - ((J.play_t + 2) & 3), STR_ICON_NOTE, PAL_PAINT(PAINT_LIME, 2));
        }
    }
    if (flash) {
        int k;
        for (k = 0; k < 3; k++)
            ui_sparkle(x + 12 + k * (w - 24) / 2, wy - 2 + (k & 1) * (wh + 2), (int)(J.play_t + k * 5) & 15, PAL_UI_GOLD);
    }
}

static void juke_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    if (lib_w_lost(0)) return;
    lib_chrome(LIB_WING_JUKEBOX, juke_pct);
    if (plat_draw_skipped()) return;
    y = 18;
    y += lib_tab_pill(sw / 2, y, tab_names[lib_b.tab], lib_b.count, lib_b.tab, N_TABS, WIN_DARK);
    {
        char b[24];
        doomo_snprintf(b, sizeof(b), STR_ICON_NOTE " %d played", bits_count(J.played));
        ui_text_outline(6, 5, b, PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1));
    }
    lw = lib_big() ? 150 : 112;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_DARK, row);
    ui_window(lw + 8, y + 1, sw - lw - 12, sh - y - 16, WIN_DARK);
    if (lib_b.count > 0) {
        if (sfx_tab()) sfx_panel(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
        else song_panel(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    }
    lib_hint(STR_ICON_A " play  L/R shelf  SEL stop  " STR_ICON_B " back", WIN_DARK);
}

const DoomoScene SCENE_LIB_JUKEBOX = { "LIB_JUKEBOX", juke_enter, juke_leave, juke_update, juke_draw, 0 };
