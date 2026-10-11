/* lib_gallery.c - SCENE_LIB_GALLERY: the Art Gallery (owner: library).
 *
 * Unlocked by MODE_GALLERY (unlocks_library.c). Four rooms (L/R):
 *   TEXTURES  every world texture in a gold frame, animated ones playing;
 *             the detail page tiles it to show it is seamless
 *   SKIES     the five skies, panoramic, slowly panning
 *   SPRITES   every sprite with its frames playing (SELECT pauses,
 *             up/down steps); demons only once the Bestiary has them
 *   PALETTE   all 256 colours: the cursor shows each one's family, shade,
 *             RGB and its lighter / darker neighbours
 * Visiting all four rooms in one go earns the Art Critic trophy.
 *
 * PS1 note: texture / sky pixels are only handed to plat_image (they are
 * virtual addresses there), never read.
 */
#include "lib_int.h"

#define G (lib_w.gal)
enum { ROOM_TEX, ROOM_SKY, ROOM_SPR, ROOM_PAL, N_ROOMS };
static const char *const room_names[N_ROOMS] = { "Textures", "Skies", "Sprites", "Palette" };
static const char *const sky_names[SKY_COUNT] = {
    "Pastel Day", "Candy Sunset", "Starry Night", "Hellfire", "Vape Storm"
};
static const char *const sky_notes[SKY_COUNT] = {
    "HaHaland on a good day: cotton clouds and a sky the colour of milk.",
    "Strawberry, peach and lemon, stirred slowly. Picnic weather.",
    "Every star was counted by the owls. They lost count. Twice.",
    "What the sky looks like after the Gate opens. Bring a hat.",
    "Lord Vapula's weather. It smells of Unholy Grape up there.",
};
static const char *const ui_colour_names[16] = {
    "Clear", "Black", "White", "Light grey", "Grey", "Dark grey", "UI pink",
    "UI pink (dark)", "UI mint", "UI mint (dark)", "UI cream", "UI red",
    "UI red (dark)", "UI gold", "UI purple", "LCD green"
};

static int room(void) { return lib_b.tab; }

static int is_demon_spr(int spr) { return spr >= SPR_IMPLING && spr <= SPR_LORD_VAPULA; }
static int spr_open(int spr)
{
    return lib_b.demo || !is_demon_spr(spr) || library_seen(THING_IMPLING + spr - SPR_IMPLING);
}

int lib_gallery_pct(void)
{
    int s, n = 0, have = 0;
    if (!library_wing_open(LIB_WING_GALLERY)) return 0;
    n = (TEX_COUNT - 1) + SKY_COUNT + 1;
    have = n;
    for (s = 1; s < SPR_COUNT; s++) {
        n++;
        if (!is_demon_spr(s) || library_seen(THING_IMPLING + s - SPR_IMPLING)) have++;
    }
    lib_have = (int16_t)have;
    lib_total = (int16_t)n;
    return lib_pct(have, n);
}

/* "vape_cloud_wall" -> "Vape cloud wall" */
static const char *pretty(const char *s, char *buf, int cap)
{
    int i;
    for (i = 0; s && s[i] && i < cap - 1; i++) {
        char c = s[i] == '_' ? ' ' : s[i];
        if (i == 0 && c >= 'a' && c <= 'z') c = (char)(c - 32);
        buf[i] = c;
    }
    buf[i] = 0;
    return buf;
}

static void rebuild(void)
{
    switch (room()) {
    case ROOM_TEX: lib_b.count = TEX_COUNT - 1; break;
    case ROOM_SKY: lib_b.count = SKY_COUNT; break;
    case ROOM_SPR: lib_b.count = SPR_COUNT - 1; break;
    default: lib_b.count = 0; break;        /* the palette has its own cursor */
    }
    lib_clamp();
    G.frame = 0;
    G.rooms |= (uint8_t)(1u << room());
    if (G.rooms == (1u << N_ROOMS) - 1) unlock_grant(LIB_UL_CRITIC);
}

static uint8_t gal_pct;

static void gal_enter(int arg)
{
    int s;
    lib_wing_enter(LIB_WING_GALLERY, arg);
    if (lib_b.tab >= N_ROOMS) lib_b.tab = 0;
    for (s = 0; s < SKY_COUNT; s++) G.sky_uid[s] = doomo_image_new_uid();
    G.pal_x = 6;
    G.pal_y = 1;
    gal_pct = (uint8_t)lib_gallery_pct();
    rebuild();
}

static void gal_update(void)
{
    int ev;
    if (room() == ROOM_PAL) {
        int old = G.pal_y * 16 + G.pal_x;
        if (btn_repeat(BTN_LEFT)) G.pal_x = (uint8_t)((G.pal_x + 15) & 15);
        if (btn_repeat(BTN_RIGHT)) G.pal_x = (uint8_t)((G.pal_x + 1) & 15);
        if (btn_repeat(BTN_UP)) G.pal_y = (uint8_t)((G.pal_y + 15) & 15);
        if (btn_repeat(BTN_DOWN)) G.pal_y = (uint8_t)((G.pal_y + 1) & 15);
        if (G.pal_y * 16 + G.pal_x != old) { audio_sfx(SFX_CURSOR); lib_b.rest = 0; }
    }
    ev = lib_nav(N_ROOMS, 0);
    switch (ev) {
    case LIBE_TAB: rebuild(); break;
    case LIBE_BACK: lib_wing_leave(); break;
    case LIBE_MOVED: case LIBE_FLIP: G.frame = 0; break;
    case LIBE_TOGGLE: G.paused ^= 1; break;
    case LIBE_FILTER: G.paused ^= 1; break;
    default: break;
    }
    /* step through sprite frames while paused */
    if (room() == ROOM_SPR && G.paused && lib_b.count > 0) {
        int n = sprite_frames(lib_b.sel + 1);
        if (lib_b.detail && btn_repeat(BTN_UP)) G.frame = (uint8_t)((G.frame + n - 1) % (n ? n : 1));
        if (lib_b.detail && btn_repeat(BTN_DOWN)) G.frame = (uint8_t)((G.frame + 1) % (n ? n : 1));
    } else if (room() == ROOM_SPR && lib_b.count > 0) {
        int n = sprite_frames(lib_b.sel + 1), sp = (sprite_flags(lib_b.sel + 1) & SPRF_ONESHOT) ? 5 : 8;
        if (n > 0 && (lib_b.t % sp) == 0) G.frame = (uint8_t)((G.frame + 1) % n);
    }
}

/* ---- drawing -------------------------------------------------------------------- */
/* An ornate gold frame around (x, y, w, h); with fill = 0 only the ring is
 * drawn (the picture under it stays). */
static void gold_frame_ex(int x, int y, int w, int h, int fill)
{
    if (fill) {
        ui_round_rect(x - 5, y - 5, w + 10, h + 10, 3, PAL_RAMP(RAMP_CANDY, 1));
        ui_round_rect(x - 4, y - 4, w + 8, h + 8, 2, PAL_UI_GOLD);
        plat_rect(x - 2, y - 2, w + 4, h + 4, PAL_RAMP(RAMP_CANDY, 2));
        plat_rect(x - 1, y - 1, w + 2, h + 2, PAL_RAMP(RAMP_WOOD, 1));
    } else {
        plat_rect(x - 5, y - 5, w + 10, 5, PAL_RAMP(RAMP_CANDY, 1));
        plat_rect(x - 5, y + h, w + 10, 5, PAL_RAMP(RAMP_CANDY, 1));
        plat_rect(x - 5, y, 5, h, PAL_RAMP(RAMP_CANDY, 1));
        plat_rect(x + w, y, 5, h, PAL_RAMP(RAMP_CANDY, 1));
        plat_rect(x - 4, y - 4, w + 8, 3, PAL_UI_GOLD);
        plat_rect(x - 4, y + h + 1, w + 8, 3, PAL_UI_GOLD);
        plat_rect(x - 4, y - 1, 3, h + 2, PAL_UI_GOLD);
        plat_rect(x + w + 1, y - 1, 3, h + 2, PAL_UI_GOLD);
        plat_rect(x - 1, y - 1, w + 2, 1, PAL_RAMP(RAMP_WOOD, 1));
        plat_rect(x - 1, y + h, w + 2, 1, PAL_RAMP(RAMP_WOOD, 1));
        plat_rect(x - 1, y, 1, h, PAL_RAMP(RAMP_WOOD, 1));
        plat_rect(x + w, y, 1, h, PAL_RAMP(RAMP_WOOD, 1));
    }
    plat_rect(x - 3, y - 4, w + 6, 1, PAL_RAMP(RAMP_CANDY, 7));
    ui_fill_circle(x - 4, y - 4, 2, PAL_RAMP(RAMP_CANDY, 7));
    ui_fill_circle(x + w + 3, y - 4, 2, PAL_RAMP(RAMP_CANDY, 7));
    ui_fill_circle(x - 4, y + h + 3, 2, PAL_RAMP(RAMP_CANDY, 6));
    ui_fill_circle(x + w + 3, y + h + 3, 2, PAL_RAMP(RAMP_CANDY, 6));
}

static void gold_frame(int x, int y, int w, int h) { gold_frame_ex(x, y, w, h, 1); }

static void plaque(int cx, int y, const char *s)
{
    int w = ui_text_width(s) + 10;
    ui_round_rect(cx - w / 2, y, w, 11, 2, PAL_RAMP(RAMP_CANDY, 1));
    ui_round_rect(cx - w / 2 + 1, y + 1, w - 2, 9, 2, PAL_UI_GOLD);
    ui_text(cx - w / 2 + 5, y + 2, s, PAL_RAMP(RAMP_WOOD, 1));
}

static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    char buf[40];
    (void)sel;
    if (room() == ROOM_TEX) {
        int id = i + 1;
        ui_round_rect(x, y + 2, 7, 7, 1, texture_avg(id));
        ui_text(x + 10, y + 2, lib_fit(pretty(texture_name(id), buf, sizeof(buf)), w - 18, buf, sizeof(buf)), st->text);
        if (texture_is_animated(id)) plat_text(x + w - 6, y + 2, STR_ICON_SPARK, PAL_UI_GOLD);
    } else if (room() == ROOM_SPR) {
        int id = i + 1, ok = spr_open(id);
        ui_text(x, y + 2, lib_fit(ok ? pretty(sprite_name(id), buf, sizeof(buf)) : "? ? ? ?", w - 10, buf, sizeof(buf)),
                ok ? st->text : st->text_dim);
        if (!ok) lib_padlock(x + w - 8, y + 2, st->text_dim);
    } else if (room() == ROOM_SKY) {
        ui_round_rect(x, y + 2, 7, 7, 1, sky_horizon(i));
        ui_text(x + 10, y + 2, sky_names[i], st->text);
    }
}

static void tex_preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int id = lib_b.sel + 1, fx = x + (w - 64) / 2, fy = y + 10;
    const DoomoImage *img = texture_image(id, (uint16_t)lib_b.t);
    char buf[40];
    gold_frame(fx, fy, 64, 64);
    if (img) plat_image(fx, fy, img, 1, 0, 0);
    plaque(x + w / 2, fy + 68, lib_fit(pretty(texture_name(id), buf, sizeof(buf)), w - 16, buf, sizeof(buf)));
    if (fy + 84 + FONT_LINE < y + h) {
        doomo_snprintf(buf, sizeof(buf), "%s  %d frame%s", id >= TEX_FLESH_WALL && id <= TEX_SLIME ? "Hell" : "Cute",
                       texture_frames(id), texture_frames(id) == 1 ? "" : "s");
        ui_text_center(x + w / 2, fy + 84, buf, st->text_dim);
    }
}

static void spr_view(int cx, int bottom, int id, int max_w, int max_h)
{
    const DoomoImage *img = sprite_image(id, G.frame), *f0 = sprite_image(id, 0);
    int s = 1, ok = spr_open(id);
    if (!img || !f0) return;
    if (f0->w * 2 <= max_w && f0->h * 2 <= max_h) s = 2;
    if (f0->w * 3 <= max_w && f0->h * 3 <= max_h) s = 3;
    ui_fill_ellipse(cx, bottom - 1, img->w * s / 3 + 2, 2, PAL_RAMP(RAMP_PEACH, 4));
    if (ok) plat_image(cx - img->w * s / 2, bottom - img->h * s, img, s, 0, 0);
    else plat_image(cx - img->w * s / 2, bottom - img->h * s, img, s, IMG_SILHOUETTE, PAL_RAMP(RAMP_STONE, 1));
}

static void spr_preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int id = lib_b.sel + 1, n = sprite_frames(id), fl = sprite_flags(id);
    int sh = h - 40;
    char buf[48];
    /* a little stage with a curtain */
    ui_round_rect(x + 6, y + 6, w - 12, sh, 3, PAL_RAMP(RAMP_PEACH, 6));
    plat_rect(x + 7, y + 7, w - 14, 4, PAL_PAINT(PAINT_CHERRY, 1));
    plat_rect(x + 7, y + 11, 3, sh - 10, PAL_PAINT(PAINT_CHERRY, 0));
    plat_rect(x + w - 10, y + 11, 3, sh - 10, PAL_PAINT(PAINT_CHERRY, 0));
    spr_view(x + w / 2, y + sh, id, w - 24, sh - 12);
    ui_text_shadow(x + 6, y + sh + 10, spr_open(id) ? pretty(sprite_name(id), buf, sizeof(buf)) : "Unknown demon",
                   st->accent, st->fill_lo);
    doomo_snprintf(buf, sizeof(buf), "Frame %d/%d%s%s%s", G.frame + 1, n, (fl & SPRF_FULLBRIGHT) ? " glow" : "",
                   (fl & SPRF_FLOATS) ? " float" : "", G.paused ? " (II)" : "");
    ui_text(x + 6, y + sh + 20, lib_fit(buf, w - 12, buf, sizeof(buf)), st->text_dim);
}

/* The panorama: full width, panning when the screen is narrower than the sky. */
static void sky_room(int sw, int y, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int s = lib_b.sel, vw = DOOMO_MIN(sw - 16, SKY_W), vx = (sw - vw) / 2, vy = y + 8;
    int pan = SKY_W > vw ? ui_wave((int)lib_b.t, 600, (SKY_W - vw) / 2) + (SKY_W - vw) / 2 : 0;
    DoomoImage im;
    char buf[48];
    im.w = SKY_W;
    im.h = SKY_H;
    im.pix = sky_pixels(s, (uint16_t)lib_b.t);
    im.uid = G.sky_uid[s];
    if (im.pix) plat_image(vx - pan, vy, &im, 1, 0, 0);
    /* hide what pans out past the frame (only on narrow screens) */
    if (vx > 5) plat_rect(0, vy, vx - 5, SKY_H, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    if (vx + vw + 5 < sw) plat_rect(vx + vw + 5, vy, sw - vx - vw - 5, SKY_H, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    gold_frame_ex(vx, vy, vw, SKY_H, 0);
    plaque(sw / 2, vy + SKY_H + 6, sky_names[s]);
    {
        int ty = vy + SKY_H + 20;
        int lines = (y + h - ty) / FONT_LINE;
        if (lines > 0) {
            ui_window(8, ty - 2, sw - 16, DOOMO_MIN(lines, 3) * FONT_LINE + 6, WIN_CREAM);
            ui_text_wrap(14, ty + 1, sw - 28, sky_notes[s], st->text, 1);
        }
    }
    doomo_snprintf(buf, sizeof(buf), "%d/%d", s + 1, SKY_COUNT);
    ui_text_outline(vx + vw - ui_text_width(buf) - 2, vy + 2, buf, PAL_WHITE, PAL_RAMP(RAMP_SKY, 1));
}

static const char *colour_family(int c, char *buf, int cap)
{
    if (c < 16) return ui_colour_names[c];
    if (c < PAL_SKIN_BASE) {
        int p = (c - PAL_PAINT_BASE) / 3, sh = (c - PAL_PAINT_BASE) % 3;
        static const char *const shades[3] = { "dark", "base", "light" };
        doomo_snprintf(buf, cap, "Paint: %s (%s)", doomo_paint_names[p], shades[sh]);
        return buf;
    }
    if (c < PAL_RAMP_BASE) {
        doomo_snprintf(buf, cap, "Skin: %s %d", doomo_skin_names[(c - PAL_SKIN_BASE) / 3], (c - PAL_SKIN_BASE) % 3);
        return buf;
    }
    doomo_snprintf(buf, cap, "Ramp: %s %d", doomo_ramp_names[(c - PAL_RAMP_BASE) / 8], (c - PAL_RAMP_BASE) % 8);
    return buf;
}

static void palette_room(int sw, int y, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int cell = lib_big() ? 10 : 6, gw = cell * 16, gx = 10, gy = y + 6, i;
    int sel = G.pal_y * 16 + G.pal_x, ix = gx + gw + 10, iw = sw - ix - 6;
    uint8_t r, g, b;
    char buf[48];
    if (gw + 10 > h) { cell = (h - 10) / 16; gw = cell * 16; }    /* very short screens */
    ix = gx + gw + 10;
    iw = sw - ix - 6;
    ui_window(gx - 5, gy - 5, gw + 10, gw + 10, WIN_CREAM);
    for (i = 0; i < 256; i++) {
        int cx = gx + (i & 15) * cell, cy = gy + (i >> 4) * cell;
        if (i == 0) {
            plat_checker(cx, cy, cell, cell, PAL_GRAY_L);
            continue;
        }
        plat_rect(cx, cy, cell, cell, (uint8_t)i);
    }
    {
        int cx = gx + G.pal_x * cell, cy = gy + G.pal_y * cell, b2 = ui_wave((int)lib_b.t, 16, 1) > 0;
        ui_frame(cx - 2 - b2, cy - 2 - b2, cell + 4 + 2 * b2, cell + 4 + 2 * b2, PAL_WHITE);
        ui_frame(cx - 1, cy - 1, cell + 2, cell + 2, PAL_BLACK);
    }
    ui_window(ix, gy - 5, iw, gw + 10, WIN_CREAM);
    ix += 6;
    iw -= 12;
    ui_round_rect(ix, gy + 2, 28, 28, 3, PAL_BLACK);
    if (sel) ui_round_rect(ix + 1, gy + 3, 26, 26, 2, (uint8_t)sel);
    else plat_checker(ix + 1, gy + 3, 26, 26, PAL_GRAY_L);
    ui_round_rect(ix + 32, gy + 2, 12, 12, 2, palette_darker((uint8_t)sel));
    ui_round_rect(ix + 32, gy + 16, 12, 12, 2, palette_lighter((uint8_t)sel));
    doomo_snprintf(buf, sizeof(buf), "#%d", sel);
    ui_text_big(ix + 50, gy + 4, buf, 2, st->accent, st->fill_lo);
    palette_rgb((uint8_t)sel, &r, &g, &b);
    {
        int ty = gy + 36;
        ty += FONT_LINE * ui_text_wrap(ix, ty, iw, colour_family(sel, buf, sizeof(buf)), st->text, 1) + 2;
        doomo_snprintf(buf, sizeof(buf), "R%d G%d B%d", r, g, b);
        ui_text(ix, ty, buf, st->text_dim);
        ty += FONT_LINE;
        doomo_snprintf(buf, sizeof(buf), "Brightness %d", palette_luma((uint8_t)sel));
        ui_text(ix, ty, buf, st->text_dim);
        ty += FONT_LINE;
        if (ty + FONT_LINE <= gy + gw) {
            doomo_snprintf(buf, sizeof(buf), "BGR555 %04x", doomo_palette[sel]);
            ui_text(ix, ty, buf, st->text_dim);
        }
    }
}

static void detail(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15, big = lib_big();
    char buf[48];
    if (!lib_detail_window(x, y, w, h, WIN_CREAM) || plat_draw_skipped()) return;
    lib_b.scroll_max = 0;
    if (room() == ROOM_TEX) {
        int id = lib_b.sel + 1, s = big ? 2 : 1, n = big ? 1 : 3, k;
        const DoomoImage *img = texture_image(id, (uint16_t)lib_b.t);
        int tw = 64 * s * n, fx = x + (w - tw) / 2, fy = y + 10;
        gold_frame(fx, fy, tw, 64 * s);
        for (k = 0; k < n && img; k++) plat_image(fx + k * 64 * s, fy, img, s, 0, 0);
        plaque(x + w / 2, fy + 64 * s + 7, pretty(texture_name(id), buf, sizeof(buf)));
        doomo_snprintf(buf, sizeof(buf), "%s tile %s%s", id >= TEX_FLESH_WALL && id <= TEX_SLIME ? "Hell" : "Cute",
                       n > 1 ? "x3: seamless!" : "64x64 at 2x.",
                       texture_is_animated(id) ? " Animated!" : "");
        ui_text_center(x + w / 2, fy + 64 * s + 22, buf, st->text_dim);
        {
            int sx = x + w / 2 - 12;
            ui_round_rect(sx, fy + 64 * s + 33, 10, 8, 2, texture_avg(id));
            ui_text(sx + 13, fy + 64 * s + 33, "avg", st->text_dim);
        }
    } else if (room() == ROOM_SPR) {
        int id = lib_b.sel + 1, n = sprite_frames(id), k, stripy = y + h - 26;
        spr_view(x + w / 2, stripy - 8, id, w - 20, stripy - y - 20);
        /* the frame strip */
        for (k = 0; k < n; k++) {
            int bx = x + w / 2 - n * 11 + k * 22;
            ui_round_rect(bx, stripy, 20, 18, 3, k == G.frame ? PAL_UI_GOLD : st->fill_lo);
            doomo_snprintf(buf, sizeof(buf), "%d", k + 1);
            ui_text_center(bx + 10, stripy + 5, buf, k == G.frame ? PAL_RAMP(RAMP_WOOD, 1) : st->text_dim);
        }
        ui_text_shadow(x + 8, y + 6, spr_open(id) ? pretty(sprite_name(id), buf, sizeof(buf)) : "Unknown demon",
                       st->accent, st->fill_lo);
        doomo_snprintf(buf, sizeof(buf), "%dx%d", sprite_image(id, G.frame) ? sprite_image(id, G.frame)->w : 0,
                       sprite_image(id, G.frame) ? sprite_image(id, G.frame)->h : 0);
        ui_text(x + w - 8 - ui_text_width(buf), y + 6, buf, st->text_dim);
    }
    lib_detail_end(x, y, w, h, WIN_CREAM);
}

static void gal_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    lib_chrome(LIB_WING_GALLERY, gal_pct);
    if (plat_draw_skipped()) return;
    y = 18;
    if (room() == ROOM_SKY) sky_room(sw, y + 16, sh - y - 32);
    lib_tab_pill(sw / 2, y, room_names[room()], room() == ROOM_PAL ? 256 : lib_b.count, room(), N_ROOMS, WIN_CREAM);
    y += 16;
    if (room() == ROOM_PAL) {
        palette_room(sw, y, sh - y - 16);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R STR_ICON_ARROW_U STR_ICON_ARROW_D " pick  L/R room  " STR_ICON_B " back", WIN_CREAM);
        return;
    }
    if (room() == ROOM_SKY) {
        lib_hint(STR_ICON_ARROW_U STR_ICON_ARROW_D " sky  L/R room  " STR_ICON_B " back", WIN_CREAM);
        return;
    }
    lw = lib_big() ? 136 : 104;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_CREAM, row);
    ui_window(lw + 8, y + 1, sw - lw - 12, sh - y - 16, WIN_CREAM);
    if (lib_b.count > 0) {
        if (room() == ROOM_TEX) tex_preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
        else spr_preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    }
    if (lib_b.detail) {
        lib_dim_for_detail();
        detail(sw, sh);
        lib_hint(room() == ROOM_SPR ? STR_ICON_ARROW_L STR_ICON_ARROW_R " next  SEL pause  " STR_ICON_ARROW_U STR_ICON_ARROW_D " step"
                                    : STR_ICON_ARROW_L STR_ICON_ARROW_R " next  " STR_ICON_B " close", WIN_CREAM);
    } else {
        lib_hint(STR_ICON_A " look  L/R room  SEL pause  " STR_ICON_B " back", WIN_CREAM);
    }
}

const DoomoScene SCENE_LIB_GALLERY = { "LIB_GALLERY", gal_enter, NULL, gal_update, gal_draw, 0 };
