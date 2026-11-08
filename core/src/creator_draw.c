/* creator_draw.c - everything SCENE_CREATOR puts on screen (owner:
 * creator): the tab strip, the live preview stage (full-body figure, a
 * close-up bust on the face tabs, the turntable, sparkles, the voice
 * bubble, locked silhouettes), the option list with inline sliders /
 * swatches / toggles, the option picker, the swatch grid, the START menu,
 * the copy-a-look picker and the "moving in!" finale. Layouts adapt to
 * 240x160 (GBA) and 320x240 (PS1).
 */
#include "creator_int.h"
#include "doomo/look_pool.h"

CrLayout crl;

/* the close-up crop of the bust on small screens (shown at 2x) */
#define CR_CROP_X 5
#define CR_CROP_Y 0
#define CR_CROP_W 38
#define CR_CROP_H 44

/* ====================================================================== */
/* layout                                                                   */
void cr_layout(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    crl.sw = sw;
    crl.sh = sh;
    crl.big = sw >= 300 && sh >= 200;
    crl.tcell = crl.big ? 29 : 21;
    crl.ty = crl.big ? 4 : 2;
    crl.tx = (sw - (CR_TAB_COUNT * crl.tcell + 30)) / 2 + 15;
    crl.px = crl.big ? 4 : 3;
    crl.py = crl.big ? 30 : 22;
    crl.pw = crl.big ? 120 : 86;
    crl.ph = sh - crl.py - (crl.big ? 18 : 16);
    crl.lx = crl.px + crl.pw + 3;
    crl.ly = crl.py;
    crl.lw = sw - crl.lx - crl.px;
    crl.lh = crl.ph;
    crl.row_h = crl.big ? 12 : 11;
    crl.rows_y = crl.ly + (crl.big ? 21 : 18);
    crl.foot_y = crl.ly + crl.lh - (crl.big ? 26 : 13);     /* CR6: two footer lines on big screens */
    crl.rows = (crl.foot_y - 2 - crl.rows_y) / crl.row_h;
    crl.fig_scale = crl.big ? 2 : 1;
    crl.por_scale = crl.big ? 2 : 1;
    crl.ftr_scale = crl.big ? 3 : 2;
}

/* ====================================================================== */
/* colours                                                                  */
typedef struct Stage {
    uint8_t wall[3], wall_dot, floor, floor_d, rug, rug_d, rug_l, shadow, mote;
    int style;
} Stage;

static void stage_colors(Stage *s)
{
    if (cr_is_hellish()) {
        s->wall[0] = PAL_RAMP(RAMP_VAPE, 4); s->wall[1] = PAL_RAMP(RAMP_VAPE, 3);
        s->wall[2] = PAL_RAMP(RAMP_VAPE, 2); s->wall_dot = PAL_RAMP(RAMP_VAPE, 5);
        s->floor = PAL_RAMP(RAMP_STONE, 2); s->floor_d = PAL_RAMP(RAMP_STONE, 1);
        s->rug = PAL_RAMP(RAMP_PASTEL_PINK, 2); s->rug_d = PAL_RAMP(RAMP_PASTEL_PINK, 1);
        s->rug_l = PAL_RAMP(RAMP_PASTEL_PINK, 3); s->shadow = PAL_RAMP(RAMP_STONE, 0);
        s->mote = PAL_RAMP(RAMP_LAVA, 6);
        s->style = WIN_DARK;
    } else {
        s->wall[0] = PAL_RAMP(RAMP_PASTEL_PINK, 7); s->wall[1] = PAL_RAMP(RAMP_PASTEL_PINK, 6);
        s->wall[2] = PAL_RAMP(RAMP_PASTEL_PINK, 5); s->wall_dot = PAL_WHITE;
        s->floor = PAL_RAMP(RAMP_MINT, 5); s->floor_d = PAL_RAMP(RAMP_MINT, 4);
        s->rug = PAL_RAMP(RAMP_CANDY, 6); s->rug_d = PAL_RAMP(RAMP_CANDY, 4);
        s->rug_l = PAL_RAMP(RAMP_CANDY, 7); s->shadow = PAL_RAMP(RAMP_MINT, 3);
        s->mote = PAL_PAINT(PAINT_BUBBLEGUM, 2);
        s->style = WIN_CUTE;
    }
}

static int list_style(void) { return cr_is_hellish() ? WIN_DARK : WIN_CUTE; }

/* ====================================================================== */
/* little helpers                                                           */
/* s, cut with ".." to fit maxw pixels (buf >= 48). */
const char *cr_fit(const char *s, int maxw, char *buf)
{
    int n, i;
    if (ui_text_width(s) <= maxw) return s;
    n = (int)strlen(s);
    if (n > 45) n = 45;                 /* + ".." + NUL: at most buf[47] */
    for (i = n; i > 0; i--) {
        memcpy(buf, s, (size_t)i);
        buf[i] = '.';
        buf[i + 1] = '.';
        buf[i + 2] = 0;
        if (ui_text_width(buf) <= maxw) return buf;
    }
    buf[0] = 0;
    return buf;
}

/* rounded pill: outline, fill and a glossy top line (like the UI kit's) */
/* s scrolled sideways (a marquee) so that it fits maxw pixels. */
const char *cr_marquee(const char *s, int maxw, char *buf)
{
    int n = (int)strlen(s), k = maxw / FONT_ADVANCE, span, off, i;
    if (ui_text_width(s) <= maxw || k < 3) return cr_fit(s, maxw, buf);
    if (k > 46) k = 46;
    span = n - k;
    off = (int)(cr.t / 7) % (span + 12);       /* scroll, then rest a moment */
    off = off < 6 ? 0 : off - 6;
    if (off > span) off = span;
    for (i = 0; i < k; i++) buf[i] = s[off + i];
    buf[k] = 0;
    return buf;
}

void cr_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
{
    int r = DOOMO_MIN(h / 2, 4);
    if (w <= 2 || h <= 2) { plat_rect(x, y, w, h, fill); return; }
    ui_round_rect(x, y, w, h, r, outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, DOOMO_MAX(r - 1, 0), fill);
    if (h >= 6) {
        int in = DOOMO_MAX(r - 1, 1) + 1;
        plat_rect(x + in, y + 1, w - 2 * in, 1, hi);
    }
}

/* ui_cursor() at rest (no bob): the creator's list is a retained panel (CR9) */
void ui_cursor_glyph(int x, int y, int kind, uint8_t fill);
static void still_cursor(int x, int y)
{
    static const uint8_t cols[6] = {
        PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_CARAMEL, 1), PAL_UI_GOLD,
        PAL_UI_GOLD, PAL_PAINT(PAINT_BONE, 1), PAL_PAINT(PAINT_WHITE, 1)
    };
    int kind = opt_get(OPT_CURSOR);
    if (kind < 0 || kind > 5) kind = 0;
    ui_cursor_glyph(x - 1, y, kind, cols[kind]);
}

void cr_padlock(int x, int y, uint8_t c, uint8_t hole)
{
    plat_rect(x + 1, y, 3, 1, c);
    plat_rect(x, y + 1, 1, 2, c);
    plat_rect(x + 4, y + 1, 1, 2, c);
    plat_rect(x - 1, y + 3, 7, 4, c);
    plat_rect(x + 2, y + 4, 1, 2, hole);
}

static void icon_palette(int tab, int dim, uint8_t pal[6])
{
    int p = cr_tabs[tab].paint, i;
    pal[0] = ui_darker(PAL_PAINT(p, 0));       /* o outline */
    pal[1] = PAL_PAINT(p, 0);                  /* s shade   */
    pal[2] = PAL_PAINT(p, 1);                  /* b base    */
    pal[3] = PAL_PAINT(p, 2);                  /* l light   */
    pal[4] = PAL_WHITE;                        /* w         */
    pal[5] = PAL_PAINT(PAINT_ROSE, 1);         /* p accent  */
    if (tab == CR_TAB_PERSONA || tab == CR_TAB_SPOOKY) pal[5] = PAL_PAINT(PAINT_CHERRY, 1);
    if (dim) for (i = 1; i < 6; i++) pal[i] = ui_lighter(pal[i]);
}

static int icon_code(char c)
{
    switch (c) {
    case 'o': return 0;
    case 's': return 1;
    case 'b': return 2;
    case 'l': return 3;
    case 'w': return 4;
    case 'p': return 5;
    default:  return -1;
    }
}

/* An icon drawn as horizontal runs (the selected tab, headers). */
static void draw_icon(int x, int y, int tab, int scale, int dim)
{
    int r, c;
    uint8_t pal[6];
    icon_palette(tab, dim, pal);
    for (r = 0; r < CR_ICON_W; r++) {
        const char *row = cr_icons[tab][r];
        for (c = 0; c < CR_ICON_W; c++) {
            int k = icon_code(row[c]), run;
            if (k < 0) continue;
            for (run = 1; c + run < CR_ICON_W && row[c + run] == row[c]; run++) {}
            plat_rect(x + c * scale, y + r * scale, run * scale, scale, pal[k]);
            c += run - 1;
        }
    }
}

void cr_draw_tab_icon(int x, int y, int tab) { draw_icon(x, y, tab, 1, 0); }

/* The dimmed icons of the unselected tabs are blitted from small cached
 * images (one plat_image each instead of ~45 rects). */
/* (in the creator's scratch state: zeroed, i.e. made again, per session) */
#define icon_pix (cr.icon_pix)
#define icon_img (cr.icon_img)

static const DoomoImage *dim_icon(int tab)
{
    if (!icon_img[tab].uid) {
        int r, c;
        uint8_t pal[6];
        icon_palette(tab, 1, pal);
        for (r = 0; r < CR_ICON_W; r++)
            for (c = 0; c < CR_ICON_W; c++) {
                int k = icon_code(cr_icons[tab][r][c]);
                icon_pix[tab][r * CR_ICON_W + c] = k < 0 ? 0 : pal[k];
            }
        icon_img[tab].w = CR_ICON_W;
        icon_img[tab].h = CR_ICON_W;
        icon_img[tab].pix = icon_pix[tab];
        icon_img[tab].uid = doomo_image_new_uid();
    }
    return &icon_img[tab];
}

static void swatch(int x, int y, int w, int h, int kind, int idx, int locked)
{
    uint8_t d, b, l;
    if (locked) {
        d = PAL_RAMP(RAMP_STONE, 3); b = PAL_RAMP(RAMP_STONE, 5); l = PAL_RAMP(RAMP_STONE, 6);
    } else if (kind == RK_SKIN && idx < PAL_SKIN_COUNT) {
        d = PAL_SKIN(idx, 0); b = PAL_SKIN(idx, 1); l = PAL_SKIN(idx, 2);
    } else if (kind == RK_PAINT_NONE && idx == 0) {
        d = PAL_GRAY; b = PAL_WHITE; l = PAL_WHITE;
    } else {
        int p = kind == RK_SKIN ? idx - 8 : kind == RK_PAINT_NONE ? idx - 1 : idx;
        p = DOOMO_CLAMP(p, 0, PAL_PAINT_COUNT - 1);
        d = PAL_PAINT(p, 0); b = PAL_PAINT(p, 1); l = PAL_PAINT(p, 2);
    }
    ui_round_rect(x, y, w, h, 2, ui_darker(d));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 1, b);
    if (h > 4) plat_rect(x + 2, y + h - 2, w - 4, 1, d);
    plat_rect(x + 2, y + 1, DOOMO_MAX(1, w / 3), 1, l);
    plat_rect(x + 1, y + 2, 1, 1, l);
    if (kind == RK_PAINT_NONE && idx == 0 && !locked) {
        plat_line(x + 2, y + h - 3, x + w - 3, y + 2, PAL_PAINT(PAINT_CHERRY, 1));
    }
    if (locked) plat_text(x + (w - 5) / 2, y + (h - 8) / 2 + 1, "?", PAL_RAMP(RAMP_STONE, 1));
}

static void toggle_switch(int x, int y, int on, int sel)
{
    const UiStyle *st = ui_style(list_style());
    uint8_t fill = on ? st->accent : PAL_RAMP(RAMP_STONE, 5);
    int b = 0;                          /* CR9: still (the list is a retained panel) */
    (void)sel;
    ui_round_rect(x, y, 16, 8, 3, ui_darker(ui_darker(fill)));
    ui_round_rect(x + 1, y + 1, 14, 6, 2, fill);
    plat_rect(x + 3, y + 1, 10, 1, ui_lighter(fill));
    ui_fill_circle(on ? x + 11 : x + 4, y + 4 - b, 3, ui_darker(fill));
    ui_fill_circle(on ? x + 11 : x + 4, y + 4 - b, 2, PAL_WHITE);
}

static void mini_slider(int x, int y, int w, int v, int max, int sel)
{
    const UiStyle *st = ui_style(list_style());
    int kx;
    if (max < 1) max = 1;
    ui_round_rect(x, y + 2, w, 5, 2, st->outline);
    ui_round_rect(x + 1, y + 3, w - 2, 3, 1, st->fill_lo);
    kx = x + 2 + (w - 4) * v / max;
    if (kx > x + 2) plat_rect(x + 1, y + 3, kx - x - 1, 3, sel ? st->accent : st->title_bg);
    plat_rect(x + 2, y + 3, DOOMO_MAX(kx - x - 3, 0), 1, PAL_WHITE);
    if (sel) {
        int b = 0;                      /* CR9: still (the list is a retained panel) */
        ui_heart(kx, y + 4 - b, 3, st->accent);
    } else {
        ui_fill_circle(kx, y + 4, 2, st->outline);
        plat_rect(kx - 1, y + 3, 2, 2, st->fill_hi);
    }
}

/* ====================================================================== */
/* tab strip                                                                */
/* CR9: the strip's parts that stay put: the L / R bumpers (side 0 / 1) and the dimmed tabs */
static void tab_bumper(int side, const UiStyle *st)
{
    int lx = crl.tx - 15, rx = crl.tx + CR_TAB_COUNT * crl.tcell + 2, x = side ? rx : lx;
    cr_pill(x, crl.ty + 3, 12, 11, st->fill_lo, st->outline, st->fill_hi);
    ui_text(x + 3, crl.ty + 5, side ? "R" : "L", st->text);
}

static void tab_pill(int i, const UiStyle *st)
{
    int ch = crl.big ? 21 : 17, x = crl.tx + i * crl.tcell, w = crl.tcell - 2;
    cr_pill(x, crl.ty + 2, w, ch - 2, st->fill_lo, st->outline, st->fill_hi);
    plat_image(x + (w - CR_ICON_W) / 2, crl.ty + 2 + (ch - 2 - CR_ICON_W) / 2, dim_icon(i), 1, 0, 0);
}

/* CR9: the tab shelf: a bar across the top of the screen behind the tabs (one rect, so the tab
 * strip is a cheap retained panel on the GBA: the pills lie on it) */
static void tab_shelf(int x0, int x1, int pb, const UiStyle *st)
{
    plat_rect(x0, 0, x1 - x0, pb - 2, st->fill_hi);
    plat_rect(x0, pb - 2, x1 - x0, 1, st->fill_lo);
    plat_rect(x0, pb - 1, x1 - x0, 1, st->outline);
}

static void draw_tabs(void)
{
    const UiStyle *st = ui_style(list_style());
    int i, t = (int)cr.t;
    int ch = crl.big ? 21 : 17;
    int lx = crl.tx - 15, rx = crl.tx + CR_TAB_COUNT * crl.tcell + 2;
    /* CR9: the strip is a retained panel (GBA): the shelf, bumpers and dimmed tabs are put back
     * from the last frame; the selected tab bounces and sparkles in its hole, which also holds its
     * two neighbours (the sparkle reaches over them), redrawn every frame */
    int pb = crl.py;                                     /* the preview pane (drawn next) starts below */
    int hx0 = crl.tx + (cr.tab - 1) * crl.tcell, hx1 = crl.tx + (cr.tab + 2) * crl.tcell;
    int panel = UI_PANELS && cr.phase != CRP_HERO;
    /* CR_CALM (GBA): the selected tab stands still too once it has popped up, so the whole strip
     * is kept (no hole) */
    int still = CR_CALM && cr.tab_t >= 6, key;
    hx0 = DOOMO_MAX(hx0, lx);
    hx1 = DOOMO_MIN(hx1, rx + 12);
    key = ui_key_int(ui_key_int(ui_key_int(ui_panel_key(), list_style()), cr.tab), crl.big | still << 1);
    if (still) key = ui_key_int(key, (int)cr.tab_t < 6 ? (int)cr.tab_t : 6);
    if (!panel || plat_panel_begin(PANEL_CR_TABS, 0, 0, crl.sw, pb, key)) {
        if (panel && CR_CALM) plat_panel_solid();       /* CR9: a still backdrop */
        tab_shelf(0, crl.sw, pb, st);
        tab_bumper(0, st);
        tab_bumper(1, st);
        for (i = 0; i < CR_TAB_COUNT; i++)
            if (i != cr.tab) tab_pill(i, st);
        if (panel && !still) plat_panel_hole(hx0, 0, hx1 - hx0, pb);
        if (still) goto selected;
    }
    if (panel) {
        plat_panel_end();
        if (still) return;
        /* the hole: whatever of the strip lies in it */
        tab_shelf(hx0, hx1, pb, st);
        if (hx0 < lx + 12) tab_bumper(0, st);
        if (hx1 > rx) tab_bumper(1, st);
        for (i = cr.tab - 1; i <= cr.tab + 1; i += 2)
            if (i >= 0 && i < CR_TAB_COUNT) tab_pill(i, st);
    }
selected:
    /* the selected tab: raised, bigger, bouncing, with a pop when it changes */
    {
        int x = crl.tx + cr.tab * crl.tcell - 1, w = crl.tcell, b = still ? 0 : ui_wave(t, 20, 1) > 0;
        int pop = cr.tab_t < 6 ? (6 - (int)cr.tab_t) / 2 : 0;
        int y = crl.ty - b - pop;
        uint8_t acc = PAL_PAINT(cr_tabs[cr.tab].paint, 1);
        ui_round_rect(x + 1, y + 3, w, DOOMO_MIN(ch + 1, pb - 2 - y - 3), 4, PAL_RAMP(RAMP_STONE, 2));
        ui_round_rect(x, y, w, ch + 1, 4, ui_darker(ui_darker(acc)));
        ui_round_rect(x + 1, y + 1, w - 2, ch - 1, 3, PAL_WHITE);
        ui_round_rect(x + 2, y + 2, w - 4, ch - 3, 2, ui_lighter(ui_lighter(acc)));
        draw_icon(x + (w - CR_ICON_W) / 2, y + (ch + 1 - CR_ICON_W) / 2, cr.tab, 1, 0);
        if (!still) ui_sparkle(x + w - 2, y + 1, (t / 3) & 15, PAL_UI_GOLD);
    }
    if (panel && still) plat_panel_end();
}

/* ====================================================================== */
/* preview stage                                                            */
static uint32_t look_hash(const DoomoLook *l)
{
    const uint8_t *p = (const uint8_t *)l;
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(*l); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* CR6: a locked part shown on the citizen: the part alone turns lavender (the pixels where the
 * look wearing it differs from the draft), the rest of the citizen stays as it is.
 * CR9: the draft picture's colours and size are kept per picture (duid; 0 = work them out), the
 * pixels that agree are skipped a word at a time, and the dark / light choice is a table: the GBA
 * did this in ~17 ms, now a few. */
typedef uint32_t __attribute__((may_alias)) lc_word;
static int lc_lavender(uint8_t c)
{
    if (!cr.lc_dark_ok) {
        int i;
        memset(cr.lc_dark, 0, sizeof(cr.lc_dark));
        for (i = 0; i < 256; i++)
            if (palette_luma((uint8_t)i) < 100) cr.lc_dark[i >> 3] |= (uint8_t)(1 << (i & 7));
        cr.lc_dark_ok = 1;
    }
    return ((cr.lc_dark[c >> 3] >> (c & 7)) & 1) ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 5);
}

static void lock_compose(uint8_t *l, const uint8_t *d, int n, uint32_t duid)
{
    int i, diff = 0, strict, words = ((((uintptr_t)l | (uintptr_t)d) & 3u) == 0) ? n / 4 : 0;
    const lc_word *lw = (const lc_word *)l, *dw = (const lc_word *)d;
    if (!duid || duid != cr.lc_uid) {
        /* the draft's colours, and how many pixels it covers */
        memset(cr.lc_has, 0, sizeof(cr.lc_has));
        cr.lc_opaque = 0;
        for (i = 0; i < n; i++)
            if (d[i]) { cr.lc_has[d[i] >> 3] |= (uint8_t)(1 << (d[i] & 7)); cr.lc_opaque++; }
        cr.lc_uid = duid;
    }
    for (i = 0; i < n; i++) {
        if ((i & 3) == 0 && (i >> 2) < words && lw[i >> 2] == dw[i >> 2]) { i += 3; continue; }
        diff += d[i] && l[i] != d[i];
    }
    /* a part that reshapes the head (a tall hat, big horns) moves the citizen's own pixels too:
     * then, inside the citizen, only colours the draft does not use belong to the part */
    strict = diff * 3 > (int)cr.lc_opaque;
    for (i = 0; i < n; i++) {
        if ((i & 3) == 0 && (i >> 2) < words && lw[i >> 2] == dw[i >> 2]) { i += 3; continue; }
        if (l[i] && l[i] != d[i] && (!strict || !d[i] || !((cr.lc_has[l[i] >> 3] >> (l[i] & 7)) & 1)))
            l[i] = (uint8_t)lc_lavender(l[i]);
    }
}

/* CR9: the view the pane wants this tick */
static void want_view(CrView *v)
{
    memset(v, 0, sizeof(*v));
    v->look = cr.shown;
    v->locked = cr.shown_locked;
    if (v->locked) v->base = cr.shown_base;
    v->cam = (uint8_t)cr_camera();
    /* CR9 (CR_CALM): the camera moves once the cursor has rested on a row for a moment, so scrolling
     * through the rows draws no bust / legs / back view for every row passed */
    if (CR_SLICE && cr.disp.ok && cr.phase == CRP_MAIN && cr.row_t < 6) v->cam = cr.disp.cam;
    v->ok = 1;
}

/* The close-up bust of a view (the portrait cache's image; composed / cropped in cr.pix). */
static const DoomoImage *bust_image(const CrView *v, int status, int expr)
{
    const DoomoImage *l = portrait_image_look(&v->look, status, expr), *d = NULL;
    uint32_t key;
    if (!l) return NULL;
    if (v->locked) {                          /* the look without the locked part, to compare */
        d = portrait_image_look(&v->base, status, expr);
        if (d && (d->w != l->w || d->h != l->h)) d = NULL;
    }
    if (crl.big && !d) return l;              /* as cached */
    key = l->uid * 2654435761u ^ (d ? d->uid : 0u) ^ 0xB0570000u ^ (uint32_t)crl.big;
    if (cr.pix_kind != CRB_PORTRAIT || cr.pix_key != key || !cr.img.uid) {
        memcpy(cr.pix, l->pix, (size_t)l->w * l->h);
        if (d) lock_compose(cr.pix, d->pix, l->w * l->h, d->uid);
        cr.img.w = l->w;
        cr.img.h = l->h;
        if (!crl.big) {
            /* small screens: crop the head in place and show it at 2x */
            int r, c;
            for (r = 0; r < CR_CROP_H; r++)
                for (c = 0; c < CR_CROP_W; c++)
                    cr.pix[r * CR_CROP_W + c] = cr.pix[(r + CR_CROP_Y) * PORTRAIT_W + c + CR_CROP_X];
            cr.img.w = CR_CROP_W;
            cr.img.h = CR_CROP_H;
        }
        cr.img.pix = cr.pix;
        cr.img.uid = doomo_image_new_uid();
        cr.pix_kind = CRB_PORTRAIT;
        cr.pix_key = key;
    }
    return &cr.img;
}

/* CR6: the full figure of a view; a locked part composed in lavender (cr.lockpix) */
static const DoomoImage *body_image(const CrView *v, int status, int expr)
{
    const DoomoImage *l = figure_image(&v->look, status, expr), *d;
    uint32_t key;
    if (!l || !v->locked) return l;
    d = figure_image(&v->base, status, expr);
    if (!d || d->w != l->w || d->h != l->h) return l;
    key = l->uid * 2654435761u ^ d->uid ^ 0x10C0u;
    if (cr.lockkey != key || !cr.lockimg.uid) {
        memcpy(cr.lockpix, l->pix, (size_t)l->w * l->h);
        lock_compose(cr.lockpix, d->pix, l->w * l->h, d->uid);
        cr.lockimg.w = l->w;
        cr.lockimg.h = l->h;
        cr.lockimg.pix = cr.lockpix;
        cr.lockimg.uid = doomo_image_new_uid();
        cr.lockkey = key;
    }
    return &cr.lockimg;
}

/* CR6: the legs close up (shoes, bottoms): the lower part of the figure, shown at 2x / 3x */
#define LEGS_W(big) ((big) ? 36 : 40)
static const DoomoImage *legs_image(const CrView *v, int status, int expr)
{
    const DoomoImage *f = body_image(v, status, expr);
    int lw = LEGS_W(crl.big), x0 = (FIGURE_W - lw) / 2, y0 = FIGURE_H - lw, r, c;
    if (!f) return NULL;
    if (cr.pix_kind != CRB_LEGS || cr.pix_key != f->uid || !cr.img.uid) {
        for (r = 0; r < lw; r++)
            for (c = 0; c < lw; c++) cr.pix[r * lw + c] = f->pix[(y0 + r) * FIGURE_W + x0 + c];
        cr.img.w = (uint16_t)lw;
        cr.img.h = (uint16_t)lw;
        cr.img.pix = cr.pix;
        cr.img.uid = doomo_image_new_uid();
        cr.pix_kind = CRB_LEGS;
        cr.pix_key = f->uid;
    }
    return &cr.img;
}

/* The turntable body (fighter render, 8 directions); back = 1: the back camera (CR6, tails and
 * wings), standing still facing away. */
static const DoomoImage *turn_image(const CrView *v, int status, int *flip, int back)
{
    static const uint8_t views[8] = { 4, 3, 2, 1, 0, 1, 2, 3 };
    int v8 = back ? 4 : cr.view8 & 7, anim, view = views[v8], locked = v->locked;
    uint32_t key;
    *flip = v8 >= 1 && v8 <= 3;
    anim = cr.roll ? 5 : cr.walking ? (int)((cr.t >> 2) & 3) : (int)((cr.t >> 4) & 1);
    if (!cr.walking && !cr.roll) anim = ((cr.t >> 4) & 1) ? 1 : 0;   /* gentle idle bob */
    if (back) anim = 0;
    key = look_hash(&v->look) ^ (uint32_t)(view * 31 + anim * 977 + status * 7) ^ 0xF1670000u ^ (uint32_t)locked << 20;
    if (cr.pix_kind != CRB_FIGHTER || cr.pix_key != key || !cr.img.uid) {
        fighter_render(&v->look, status, view, anim, -1, cr.pix);
        if (locked) {
            fighter_render(&v->base, status, view, anim, -1, cr.lockpix);
            lock_compose(cr.pix, cr.lockpix, FIGHTER_W * FIGHTER_H, 0);
            cr.lockkey = 0;                   /* lockpix was reused */
        }
        cr.img.w = FIGHTER_W;
        cr.img.h = FIGHTER_H;
        cr.img.pix = cr.pix;
        cr.img.uid = doomo_image_new_uid();
        cr.pix_kind = CRB_FIGHTER;
        cr.pix_key = key;
    }
    return &cr.img;
}

/* CR9 (CR_CALM): are a view's pictures in the portrait cache? Asks for the missing one with
 * portrait_hold(2), so the cache draws it in slices over the next ticks. */
static int view_ready(const CrView *v, int status, int expr)
{
    int ok = 1;
    portrait_hold(2);
    switch (v->cam) {
    case CAM_BACK:                            /* the back view is a fighter render, drawn at once */
        break;
    case CAM_BUST:
        ok = portrait_image_look(&v->look, status, expr) != NULL;
        if (ok && v->locked) ok = portrait_image_look(&v->base, status, expr) != NULL;
        break;
    default:
        ok = figure_image(&v->look, status, expr) != NULL;
        if (ok && v->locked) ok = figure_image(&v->base, status, expr) != NULL;
        break;
    }
    portrait_hold(CR_CALM ? 1 : 0);           /* (the PS1's preview animates) */
    return ok;
}

/* CR9: the view the pane shows this tick. Native, PS1, DC: the one the creator wants. GBA
 * (CR_CALM): a new look or camera is drawn in slices by the portrait cache while the last view
 * stays up (SPEED DEFAULTS: a stand-in for a few ticks), then it pops in with a hop; a view that is
 * still not ready after 24 ticks (the cache could not start it) is drawn at once. */
static const CrView *pane_view(int status, int expr)
{
    CrView w;
    want_view(&w);
    if (!CR_SLICE || !cr.disp.ok || cr.phase == CRP_TURN || cr.phase == CRP_MOVEIN) {
        cr.disp = w;
        cr.pend.ok = 0;
        return &cr.disp;
    }
    if (memcmp(&w, &cr.disp, sizeof(w)) == 0) { cr.pend.ok = 0; return &cr.disp; }
    if (!cr.pend.ok) {
        /* a new look is started once it has rested two ticks (a held direction streams past) */
        if (cr.t - cr.shown_t < 2) return &cr.disp;
        cr.pend = w;
        cr.pend_t = 0;
    }
    if (view_ready(&cr.pend, status, expr) || ++cr.pend_t >= 24) {
        /* it lands: with a hop and a sparkle, unless the next one is already wanted (a held
         * direction: the pane stays a still, retained panel while the looks stream past) */
        if (memcmp(&cr.pend.look, &cr.disp.look, sizeof(DoomoLook)) != 0 && memcmp(&cr.pend, &w, sizeof(w)) == 0) {
            cr.hop = 9;
            cr_sparkle_burst(3, -1);
        }
        cr.disp = cr.pend;
        cr.pend.ok = 0;
    }
    return &cr.disp;
}

static void draw_sparks(int fx, int fy, int clip_x, int clip_y, int clip_w, int clip_h)
{
    int i;
    for (i = 0; i < CR_SPARKS; i++) {
        const CrSpark *s = &cr.spark[i];
        int x, y;
        if (!s->life) continue;
        x = fx + s->x / 16;
        y = fy + s->y / 16;
        if (x < clip_x + 3 || y < clip_y + 3 || x > clip_x + clip_w - 4 || y > clip_y + clip_h - 4)
            continue;
        switch (s->kind) {
        case 1: ui_heart(x, y, s->life > 8 ? 3 : 2, PAL_PAINT(PAINT_ROSE, 1)); break;
        case 2: ui_star(x, y, s->life > 8 ? 3 : 2, PAL_UI_GOLD); break;
        case 3:
            if ((s->life >> 1) & 1) plat_rect(x, y, 2, 1, s->color);
            else plat_rect(x, y, 1, 2, s->color);
            break;
        default: ui_sparkle(x, y, (s->life * 2) & 15, s->color); break;
        }
    }
}

/* soft room: wallpaper bands with polka hearts, a floor and a round rug */
static void draw_room(int x, int y, int w, int h, int floor_y, const Stage *sg, uint32_t t)
{
    int i, b1 = y + (floor_y - y) / 3, b2 = y + (floor_y - y) * 2 / 3;
    plat_rect(x, y, w, b1 - y, sg->wall[0]);
    plat_rect(x, b1, w, b2 - b1, sg->wall[1]);
    plat_rect(x, b2, w, floor_y - b2, sg->wall[2]);
    plat_checker(x, b1, w, 1, sg->wall[0]);
    plat_checker(x, b2, w, 1, sg->wall[1]);
    /* drifting polka hearts on the wallpaper */
    for (i = 0; i < 6; i++) {
        int hx = x + 6 + ((i * 37 + (int)(t / 4)) % DOOMO_MAX(w - 12, 1));
        int hy = y + 8 + ((i * 53) % DOOMO_MAX(floor_y - y - 20, 1));
        if (cr_is_hellish()) ui_star(hx, hy, 2, sg->wall_dot);
        else ui_heart_flat(hx, hy, 2, sg->wall_dot);
    }
    /* skirting board + floor */
    plat_rect(x, floor_y - 2, w, 2, sg->floor_d);
    plat_rect(x, floor_y, w, y + h - floor_y, sg->floor);
    plat_checker(x, floor_y, w, 1, sg->floor_d);
}

/* rising motes behind the figure */
static void draw_motes(int x, int y, int w, int h, const Stage *sg)
{
    int i;
    for (i = 0; i < 5; i++) {
        int period = 90 + i * 17;
        int ph = (int)((cr.t + (uint32_t)i * 41u) % (uint32_t)period);
        int mx = x + 8 + (i * 29) % DOOMO_MAX(w - 16, 1) + ui_wave((int)cr.t + i * 20, 40, 2);
        int my = y + h - 12 - ph * (h - 20) / period;
        if (my < y + 4) continue;
        if (cr_is_hellish()) plat_rect(mx, my, 1 + (i & 1), 1 + (i & 1), sg->mote);
        else ui_sparkle(mx, my, (ph / 3) & 15, sg->mote);
    }
}

static void name_plate(int cx, int y, int w)
{
    const UiStyle *st = ui_style(list_style());
    char buf[48];
    const char *name = cr.draft.name[0] ? cr.draft.name : "Name me!";
    int x = cx - w / 2;
    cr_pill(x, y, w, 13, st->title_bg, st->outline, ui_lighter(st->title_bg));
    if (cr.mode == CR_PLAYER) ui_star(x + 7, y + 6, 3, PAL_UI_GOLD);
    else if (cr_is_hellish()) plat_text(x + 4, y + 3, STR_ICON_PENTA, PAL_PAINT(PAINT_CHERRY, 1));
    else ui_heart(x + 7, y + 6, 3, PAL_PAINT(PAINT_ROSE, 1));
    ui_text_shadow(x + 13, y + 3, cr_fit(name, w - 18, buf), st->title, st->outline);
}

static void speech_bubble(int x, int y, int w)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int lines = ui_text_wrap(0, 0, w - 8, cr.talk, 0, 0);
    int h = lines * FONT_LINE + 6;
    ui_round_rect(x, y, w, h, 3, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, PAL_WHITE);
    plat_rect(x + w / 2 - 2, y + h - 1, 5, 1, PAL_WHITE);
    plat_rect(x + w / 2 - 1, y + h, 3, 1, PAL_WHITE);
    plat_rect(x + w / 2, y + h + 1, 1, 1, st->outline);
    plat_rect(x + w / 2 - 3, y + h - 1, 1, 1, st->outline);
    plat_rect(x + w / 2 + 3, y + h - 1, 1, 1, st->outline);
    plat_rect(x + w / 2 - 2, y + h, 1, 1, st->outline);
    plat_rect(x + w / 2 + 2, y + h, 1, 1, st->outline);
    ui_text_wrap_n(x + 4, y + 3, w - 8, cr.talk, st->text, cr.talk_pos);
    if (cr.talk_pos < cr.talk_len) plat_text(x + w - 9, y + h - 9, STR_ICON_NOTE,
                                             PAL_PAINT(PAINT_ROSE, 1));
}

/* the unlock hint the bubble shows */
static const char *bubble_hint(void)
{
    const char *hint = "";
    const CrRow *row = cr_cur_row();
    if (cr.phase == CRP_PICK || cr.phase == CRP_COLOR) hint = cr_lock_hint(row, cr.pick_sel);
    else if (cr.browse >= 0) hint = cr_lock_hint(row, cr.browse);
    return hint[0] ? hint : "Locked!";
}

/* the "Locked!" bubble on the preview: at the top (top = 1) or ending at y (top = 0); at most 3 lines
 * of hint (4 on big screens): the footer always has the whole hint */
static void lock_bubble(int x, int y, int w, int top)
{
    const char *hint = bubble_hint();
    int lines, h, maxl = crl.big ? 4 : 3, chars;
    lines = ui_text_wrap(0, 0, w - 8, hint, 0, 0);
    chars = lines > maxl ? ui_wrap_skip(hint, w - 8, maxl) : (int)strlen(hint);
    if (lines > maxl) lines = maxl;
    h = lines * FONT_LINE + 15;
    if (!top) y -= h;
    ui_round_rect(x, y, w, h, 3, PAL_RAMP(RAMP_VAPE, 1));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, PAL_RAMP(RAMP_VAPE, 6));
    cr_padlock(x + 6, y + 3, PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 6));
    ui_text(x + 14, y + 3, "Locked!", PAL_RAMP(RAMP_VAPE, 0));
    ui_text_wrap_n(x + 4, y + 12, w - 8, hint, PAL_RAMP(RAMP_VAPE, 1), chars);
}

/* CR9: is a sparkle still flying? (the pane is redrawn every tick while one is) */
static int sparks_alive(void)
{
    int i;
    for (i = 0; i < CR_SPARKS; i++)
        if (cr.spark[i].life) return 1;
    return 0;
}

static void draw_pane(void)
{
    Stage sg;
    const UiStyle *st;
    int x = crl.px, y = crl.py, w = crl.pw, h = crl.ph;
    int ix = x + 3, iy = y + 3, iw = w - 6, ih = h - 6;
    int floor_y = iy + ih * 7 / 10, feet = iy + ih - 20 - (crl.big ? 6 : 0);
    int cx = ix + iw / 2, status = cr_preview_status(), expr = cr_preview_expr();
    int hop = cr.hop ? -ui_wave((int)(9 - cr.hop) * 2, 36, crl.big ? 5 : 3) : 0;
    const CrView *v = pane_view(status, expr);
    int cam = v->cam, locked = v->locked, turn = cr.phase == CRP_TURN, flip = 0, panel;
    const DoomoImage *im;
    stage_colors(&sg);
    st = ui_style(sg.style);
    if (hop > 0) hop = -hop;
    /* the picture first (it may be drawn now), so the panel's key knows it */
    if (turn || cam == CAM_BACK) im = turn_image(v, status, &flip, !turn);
    else if (cam == CAM_BUST) im = bust_image(v, status, expr);
    else if (cam == CAM_LEGS) im = legs_image(v, status, expr);
    else im = body_image(v, status, expr);
    /* CR9 (CR_CALM): the stage stands still, so the pane is a retained panel (GBA): put back from
     * the last frame while the picture, the hop, the bubbles and the name plate are unchanged */
    panel = UI_PANELS && CR_CALM && !turn && !sparks_alive();
    if (panel) {
        uint32_t k = ui_key_int(ui_panel_key(), im ? (int32_t)(im->uid & 0x7FFFFFFFu) : -1);
        k = ui_key_int(k, cam | locked << 3 | crl.big << 4 | sg.style << 5 | cr.spooky << 12 | cr.mode << 13 |
                              cr_is_hellish() << 16 | (int)cr.hop << 17);
        k = ui_key_int(k, ui_window_glint(x, y, w, h));
        k = ui_key_str(k, cr.draft.name);
        if (locked) k = ui_key_str(k, bubble_hint());
        if (cr.talk) k = ui_key_int(ui_key_str(k, cr.talk), cr.talk_pos);
        if (!plat_panel_begin(PANEL_CR_PANE, x, y, w + 3, DOOMO_MIN(h + 3, crl.sh - 13 - y), k)) {
            ui_window_shadow(x, y, w, h);               /* kept: its drop shadow only */
            plat_panel_end();
            return;
        }
        plat_panel_solid();                             /* a still backdrop (CR_CALM) */
    }
    /* frame */
    ui_window(x, y, w, h, sg.style);
    ui_round_rect(ix - 1, iy - 1, iw + 2, ih + 2, 3, st->outline);
    ui_round_rect(ix, iy, iw, ih, 2, sg.wall[0]);
    draw_room(ix + 1, iy + 1, iw - 2, ih - 2, floor_y, &sg, CR_CALM ? 0u : cr.t);
    if (!CR_CALM) draw_motes(ix, iy, iw, floor_y - iy, &sg);
    /* rug */
    ui_fill_ellipse(cx, feet + 1, iw / 2 - 4, crl.big ? 9 : 6, sg.rug_d);
    ui_fill_ellipse(cx, feet, iw / 2 - 5, crl.big ? 8 : 5, sg.rug);
    ui_fill_ellipse(cx - 2, feet - 1, iw / 3, crl.big ? 4 : 2, sg.rug_l);

    if (turn || cam == CAM_BACK) {
        int sc = crl.ftr_scale;
        int fw = im->w * sc, fh = im->h * sc;
        int b = ui_wave((int)cr.t, 14, 2);
        ui_fill_ellipse(cx, feet, fw / 4 + 2, crl.big ? 4 : 3, sg.shadow);
        plat_image(cx - fw / 2, feet - fh + 2 + (turn ? 0 : hop), im, sc, flip ? IMG_FLIPX : 0, 0);
        if (turn) {                           /* turn arrows */
            plat_text(ix + 3 - (b > 0), feet - fh / 2, STR_ICON_ARROW_L, st->accent);
            plat_text(ix + iw - 9 + (b > 0), feet - fh / 2, STR_ICON_ARROW_R, st->accent);
        } else {
            ui_text_center(cx, iy + 3, "Back view", st->text_dim);
        }
    } else if (cam == CAM_BUST) {
        if (im) {
            int sc = crl.big ? crl.por_scale : 2, bw = im->w * sc, bh = im->h * sc;
            int by = crl.big ? floor_y - bh + 18 + hop / 2 : iy + 6 + hop / 2;
            int rr = crl.big ? bw / 2 : bw / 2 - 1;
            ui_fill_circle(cx, by + bh / 2, rr + 2, sg.wall[0]);
            ui_fill_circle(cx, by + bh / 2, rr, PAL_WHITE);
            ui_fill_circle(cx, by + bh / 2, rr - 2, sg.wall[0]);
            plat_image(cx - bw / 2, by, im, sc, 0, 0);
        }
    } else if (cam == CAM_LEGS) {
        int sc = crl.big ? 3 : 2;
        if (im) {
            int fw = im->w * sc, fh = im->h * sc;
            ui_fill_ellipse(cx, feet, fw / 3, crl.big ? 5 : 3, sg.shadow);
            plat_image(cx - fw / 2, feet - fh + 2 + hop / 2, im, sc, 0, 0);
        }
    } else {
        int sc = crl.fig_scale;
        if (im) {
            int fw = im->w * sc, fh = im->h * sc;
            int sq = cr.hop > 6 ? 1 : 0;
            ui_fill_ellipse(cx, feet, fw / 4 + (hop < -2 ? -2 : 1), crl.big ? 4 : 2, sg.shadow);
            plat_image(cx - fw / 2, feet - fh + 1 + hop + sq, im, sc, 0, 0);
        }
    }
    draw_sparks(cx, feet, ix, iy, iw, ih);
    /* bubbles & plates */
    if (locked) {
        /* CR6: on the legs camera the bubble sits at the top, clear of the shoes it is about; else
         * at the bottom, inside the pane */
        if (cam == CAM_LEGS) lock_bubble(ix + 2, iy + 2, iw - 4, 1);
        else lock_bubble(ix + 2, iy + ih - 4, iw - 4, 0);
    } else {
        name_plate(cx, iy + ih - 15, iw - 8);
        if (cr.talk) speech_bubble(ix + 3, iy + 3, iw - 6);
    }
    if (cr.spooky && !locked)
        ui_text_outline(ix + 4, iy + 3 + (cr.talk ? 30 : 0), "POSSESSED!", PAL_PAINT(PAINT_CHERRY, 1),
                        PAL_RAMP(RAMP_BLOOD, 0));
    if (panel) plat_panel_end();
    if (turn) {
        int b = ui_wave((int)cr.t, 20, 1) > 0;
        ui_frame(x - 1 - b, y - 1 - b, w + 2 + 2 * b, h + 2 + 2 * b, PAL_UI_GOLD);
    }
}

/* ====================================================================== */
/* option list                                                              */
static void draw_value(const CrRow *row, int vx_right, int y, int maxw, int sel)
{
    const UiStyle *st = ui_style(list_style());
    char buf[48], buf2[48];
    int idx, locked = 0, n;
    uint8_t vc = sel ? st->accent : st->text;
    if (sel && cr.browse >= 0 && (row->kind != RK_ACTION)) { idx = cr.browse; locked = 1; }
    else if (sel && cr.browse >= 0) { idx = cr.browse; locked = cr_value_locked(row, idx); }
    else idx = cr_row_value(&cr.draft, row);
    n = cr_row_count(row);
    switch (row->kind) {
    case RK_SLIDER: {
        int sw = DOOMO_CLAMP(maxw - 12, 18, crl.big ? 64 : 40);
        doomo_snprintf(buf, sizeof(buf), "%2d", idx);
        mini_slider(vx_right - sw - 14, y, sw, idx, n - 1, sel);
        ui_text(vx_right - 12, y, buf, vc);
        return; }
    case RK_FLAG:
        toggle_switch(vx_right - 17, y, idx, sel);
        return;
    case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN: {
        const char *nm = locked ? "???" : cr_choice_name(row, idx, buf2);
        const char *f;
        int tw;
        if (!sel && ui_text_width(nm) > maxw - 14) {      /* tight: swatch only */
            swatch(vx_right - 18, y, 18, 8, row->kind, idx, locked);
            return;
        }
        f = sel ? cr_marquee(nm, maxw - 14, buf) : cr_fit(nm, maxw - 14, buf);
        tw = ui_text_width(f);
        swatch(vx_right - tw - 13, y, 11, 8, row->kind, idx, locked);
        ui_text(vx_right - tw, y, f, locked ? st->text_dim : vc);
        return; }
    default: {
        const char *nm = locked ? "???" : cr_choice_name(row, idx, buf2);
        const char *f = sel ? cr_marquee(nm, maxw - (locked ? 8 : 0), buf)
                            : cr_fit(nm, maxw - (locked ? 8 : 0), buf);
        int tw = ui_text_width(f);
        if (locked) cr_padlock(vx_right - tw - 8, y + 1, st->text_dim, st->fill);
        else if (row->kind == RK_ACTION && cr_value_locked(row, idx))
            cr_padlock(vx_right - tw - 8, y + 1, st->text_dim, st->fill);
        ui_text(vx_right - tw, y, f, locked ? st->text_dim : vc);
        return; }
    }
}

static void draw_footer(const CrRow *row, int x, int y, int w)
{
    const UiStyle *st = ui_style(list_style());
    char buf[64], b2[48], b3[48];
    const char *s = "";
    uint8_t c = st->text_dim;
    int idx;
    if (!row) return;
    idx = cr.browse >= 0 ? cr.browse : cr_row_value(&cr.draft, row);
    if (cr.browse >= 0 && cr_value_locked(row, idx)) {
        /* CR6: the whole hint, never cut: a marquee on the GBA, two lines on big screens */
        const PartMeta *m = cr_choice_meta(row, idx);
        cr_padlock(x + 1, y + 1, PAL_PAINT(PAINT_CHERRY, 1), st->fill);
        if (crl.big) {
            ui_text(x + 9, y, cr_fit(m ? part_meta_name(m) : "Locked!", w - 10, b2), PAL_PAINT(PAINT_CHERRY, 0));
            ui_text(x, y + 11, cr_marquee(cr_lock_hint(row, idx), w, b3), PAL_PAINT(PAINT_CHERRY, 0));
        } else {
            doomo_snprintf(buf, sizeof(buf), "%s%s%s", m ? part_meta_name(m) : "", m ? ": " : "", cr_lock_hint(row, idx));
            ui_text(x + 9, y, cr_marquee(buf, w - 10, b2), PAL_PAINT(PAINT_CHERRY, 0));
        }
        return;
    }
    switch (row->kind) {
    case RK_SLIDER:
        doomo_snprintf(buf, sizeof(buf), "%s " STR_ICON_ARROW_L STR_ICON_ARROW_R " %s",
                       row->names ? row->names[0] : "", row->names ? row->names[1] : "");
        s = buf;
        break;
    case RK_ENUM: {
        const PartMeta *m = cr_choice_meta(row, idx);
        c = st->text;
        if (m) {                              /* CR6: a part names its themed set too */
            doomo_snprintf(buf, sizeof(buf), "%s %d/%d - %s", cr_choice_name(row, idx, b3),
                           cr_visible_pos(row, idx) + 1, cr_visible_count(row), doomo_part_sets[m->set].name);
            /* CR9: on small screens the set only when it fits (a still footer); the grid names it */
            if (!crl.big && ui_text_width(buf) > w)
                doomo_snprintf(buf, sizeof(buf), "%s %d/%d", cr_choice_name(row, idx, b3),
                               cr_visible_pos(row, idx) + 1, cr_visible_count(row));
            ui_text(x, y, crl.big ? cr_marquee(buf, w, b2) : cr_fit(buf, w, b2), c);
            return;
        }
        doomo_snprintf(buf, sizeof(buf), "%s  %d/%d", cr_choice_name(row, idx, b3), idx + 1,
                       cr_row_count(row));
        s = buf;
        break; }
    case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN:
        doomo_snprintf(buf, sizeof(buf), STR_ICON_A " palette  %s", cr_choice_name(row, idx, b3));
        s = buf;
        break;
    case RK_NAME:  s = STR_ICON_A " type  " STR_ICON_ARROW_L STR_ICON_ARROW_R " idea"; break;
    case RK_NICK:  s = STR_ICON_A " type  " STR_ICON_ARROW_L STR_ICON_ARROW_R " idea"; break;
    case RK_FLAG: case RK_RFLAG: s = STR_ICON_A " toggle"; break;
    case RK_DISTRICT:
        s = cr.district_fixed ? (cr.mode == CR_PLAYER ? "Home sweet home!" : "Their home stays put")
                              : STR_ICON_ARROW_L STR_ICON_ARROW_R " pick a side";
        break;
    case RK_ACTION:
        if (row->aux == CA_TEST_VOICE) s = STR_ICON_A " hear catchphrase";
        else if (row->aux == CA_MATCH_FUR) s = STR_ICON_A " hair = fur colour";
        else {
            if (crl.big)
                doomo_snprintf(buf, sizeof(buf), STR_ICON_ARROW_L STR_ICON_ARROW_R " browse  "
                               STR_ICON_A " apply   %d/%d", idx + 1, cr_row_count(row));
            else
                doomo_snprintf(buf, sizeof(buf), STR_ICON_ARROW_L STR_ICON_ARROW_R " browse  "
                               STR_ICON_A " apply");
            s = buf;
        }
        break;
    default: break;
    }
    ui_text(x, y, cr_fit(s, w, b2), c);
}

static void draw_persona_panel(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(list_style());
    int pt = persona_type(&cr.draft), q = RES_PERSONA(&cr.draft, PERS_QUIRK) / 4;
    char text[200];
    int fitn, total, pages, page, skip, nb;
    if (h < 22) return;
    ui_round_rect(x, y, w, h, 3, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, st->fill_hi);
    ui_heart(x + 8, y + 6, 3 + ((cr.t / 10) & 1), PAL_PAINT(PAINT_ROSE, 1));
    ui_text_shadow(x + 15, y + 2, doomo_persona_type_names[pt], st->accent, st->outline);
    /* description, quirk and a hello in their own words - paged when short */
    doomo_snprintf(text, sizeof(text), "%s %s \"%s\"", cr_persona_desc[pt],
                   cr_quirk_names[DOOMO_CLAMP(q, 0, 3)], cr_persona_greet[pt]);
    fitn = (h - 13) / FONT_LINE;
    if (fitn < 1) return;
    total = ui_text_wrap(0, 0, w - 10, text, 0, 0);
    pages = (total + fitn - 1) / fitn;
    page = pages > 1 ? (int)(cr.t / 75) % pages : 0;
    skip = ui_wrap_skip(text, w - 10, page * fitn);
    nb = ui_wrap_skip(text + skip, w - 10, fitn);
    ui_text_wrap_n(x + 5, y + 12, w - 10, text + skip, st->text, nb);
    if (pages > 1) {
        int i;
        for (i = 0; i < pages; i++)
            plat_rect(x + w - 6 - (pages - i) * 4, y + 4, 2, 2, i == page ? st->accent : st->fill_lo);
    }
}

static void draw_home_panel(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(list_style());
    int hell = cr.district == DISTRICT_HELLVILE && cr.mode != CR_PLAYER;
    uint8_t sky = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_SKY, 6);
    uint8_t sky_l = hell ? PAL_RAMP(RAMP_VAPE, 3) : PAL_RAMP(RAMP_SKY, 7);
    uint8_t grass = hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_GRASS, 5);
    uint8_t grass_d = hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_GRASS, 3);
    uint8_t wall = hell ? PAL_RAMP(RAMP_VAPE, 4) : PAL_RAMP(RAMP_PASTEL_PINK, 6);
    uint8_t wall_d = hell ? PAL_RAMP(RAMP_VAPE, 1) : PAL_RAMP(RAMP_PASTEL_PINK, 3);
    uint8_t wall_l = hell ? PAL_RAMP(RAMP_VAPE, 5) : PAL_RAMP(RAMP_PASTEL_PINK, 7);
    uint8_t roof = hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_SKY, 1);
    uint8_t roof_d = hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_PAINT(PAINT_SKY, 0);
    uint8_t win = hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_SKY, 2);
    uint8_t tc = hell ? PAL_WHITE : PAL_RAMP(RAMP_SKY, 1);
    uint8_t lit = PAL_UI_GOLD;
    char buf[64];
    int floor, room, i, j, bw, bx, by, bh, cw, chh, rows, gy, t = (int)cr.t;
    if (h < 30) return;
    ui_round_rect(x, y, w, h, 3, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, sky);
    plat_rect(x + 3, y + 1, w - 6, (h - 2) / 3, sky_l);
    plat_checker(x + 2, y + 1 + (h - 2) / 3, w - 4, 1, sky_l);
    gy = y + h - (crl.big ? 16 : 11);                       /* ground line */
    plat_rect(x + 1, gy, w - 2, y + h - 1 - gy - 1, grass);
    plat_rect(x + 1, gy, w - 2, 1, grass_d);
    /* clouds / stars */
    for (i = 0; i < 3; i++) {
        int cx = x + 8 + ((i * 47 + t / 6) % DOOMO_MAX(w - 16, 1)), cy = y + 4 + i * 5;
        if (hell) plat_rect(cx, cy, 1, 1, PAL_RAMP(RAMP_LAVA, 6));
        else { plat_rect(cx, cy, 7, 2, PAL_WHITE); plat_rect(cx + 2, cy - 1, 3, 1, PAL_WHITE); }
    }
    if (cr.mode == CR_PLAYER) {
        /* your cottage */
        bw = DOOMO_MIN(w / 3, 60);
        bh = DOOMO_MIN(gy - y - 12, bw * 3 / 4);
        bx = x + (w - bw) / 2;
        by = gy - bh;
        ui_round_rect(bx - 1, by - 1, bw + 2, bh + 1, 1, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(bx, by, bw, bh, PAL_RAMP(RAMP_CANDY, 6));
        for (i = 0; i < bw / 2 + 2; i++)
            plat_rect(bx - 2 + i, by - 1 - i, bw + 4 - 2 * i, 1, PAL_PAINT(PAINT_ROSE, i < 2 ? 0 : 1));
        plat_rect(bx + bw / 2 - 3, gy - DOOMO_MIN(10, bh - 2), 6, DOOMO_MIN(10, bh - 2), PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(bx + 3, by + 3, 6, 5, lit);
        plat_rect(bx + bw - 9, by + 3, 6, 5, lit);
        ui_heart(bx + bw / 2, by - bw / 2 - 4 + ui_wave(t, 30, 1), 3, PAL_PAINT(PAINT_ROSE, 1));
        ui_text_center(x + w / 2, y + h - (crl.big ? 12 : 9), "Your cottage!", PAL_WHITE);
        return;
    }
    cr_home_floor_room(cr.home_id, &floor, &room);
    /* a slice of the apartment block around the home's floor */
    cw = DOOMO_MIN((w - 20) / 10, crl.big ? 14 : 10);
    chh = crl.big ? 11 : 7;
    bw = cw * 10 + 5;
    bx = x + (w - bw) / 2;
    rows = (gy - y - (crl.big ? 24 : 18) - (hell ? 3 : 0)) / chh;
    rows = DOOMO_CLAMP(rows, 1, 6);
    bh = rows * chh + 4;
    by = gy - bh;
    plat_rect(bx - 1, by, bw + 2, bh, wall_d);
    plat_rect(bx, by, bw, bh, wall);
    plat_rect(bx + 1, by, 2, bh, wall_l);
    plat_rect(bx + bw - 2, by, 1, bh, wall_d);
    plat_rect(bx - 3, by - 3, bw + 6, 3, roof);
    plat_rect(bx - 3, by - 1, bw + 6, 1, roof_d);
    if (hell) for (i = 0; i < bw; i += 6) plat_rect(bx + i, by - 6, 3, 3, roof);
    {
        int bottom = DOOMO_MAX(1, floor - rows / 2), topf;
        if (bottom + rows - 1 > 50) bottom = 50 - rows + 1;
        topf = bottom + rows - 1;
        for (j = 0; j < rows; j++) {
            int fl = topf - j;
            for (i = 0; i < 10; i++) {
                int wx = bx + 3 + i * cw, wy = by + 3 + j * chh;
                int here = cr.home_id >= 0 && fl == floor && i + 1 == room;
                int slot = cr.district * DOOMO_DISTRICT_SIZE + (fl - 1) * 10 + i;
                int taken = slot < DOOMO_MAX_RESIDENTS && doomo_town.res[slot].status != RES_EMPTY;
                uint8_t c = here ? (((t / 8) & 1) ? lit : PAL_WHITE) : taken ? lit : win;
                plat_rect(wx, wy, cw - 3, chh - 3, c);
                plat_rect(wx, wy, cw - 3, 1, here ? PAL_WHITE : ui_lighter(c));
                if (here) {
                    ui_frame(wx - 1, wy - 1, cw - 1, chh - 1, PAL_PAINT(PAINT_ROSE, 1));
                    ui_heart(wx + (cw - 3) / 2, wy - 4 + ui_wave(t, 24, 1), 2, PAL_PAINT(PAINT_ROSE, 1));
                }
            }
        }
    }
    /* entrance on the ground floor */
    if (floor <= rows) plat_rect(bx + bw / 2 - 3, gy - 5, 6, 5, PAL_RAMP(RAMP_WOOD, 3));
    ui_text(x + 5, y + 2, cr_home_names[hell], tc);
    if (cr.home_id >= 0)
        doomo_snprintf(buf, sizeof(buf), "Floor %d Room %d", floor, room);
    else
        doomo_snprintf(buf, sizeof(buf), "No free homes!");
    ui_text_center(x + w / 2, y + h - (crl.big ? 12 : 9), buf, PAL_WHITE);
    if (cr.mode == CR_NEW && crl.big) {
        doomo_snprintf(buf, sizeof(buf), "%d free", town_count_district(cr.district, RES_EMPTY));
        ui_text(x + w - 5 - ui_text_width(buf), y + 2, buf, tc);
    }
}

/* one (non-header) row of the list */
static void draw_list_row(const CrRow *row, int ry, int sel, int is_cur, int inner_r, const UiStyle *st)
{
    int x = crl.lx, lw;
    if (sel || (is_cur && cr.phase != CRP_TURN)) {
        int shake = cr.pick_err ? ((cr.pick_err & 2) ? 1 : -1) : 0;
        cr_pill(x + 11 + shake, ry - 1, inner_r - x - 9, crl.row_h, st->accent_d,
                ui_darker(st->accent_d), ui_lighter(st->accent_d));
        if (sel) still_cursor(x + 3, ry + 1);
    }
    ui_text(x + 15, ry + 1, cr_row_label(row), is_cur ? st->text : st->text_dim);
    lw = ui_text_width(cr_row_label(row));
    draw_value(row, inner_r - (sel ? 9 : 4), ry + 1, inner_r - x - 15 - lw - 10 - (sel ? 16 : 6), sel);
    if (sel && row->kind != RK_SLIDER && row->kind != RK_FLAG)
        plat_text(inner_r - 7, ry + 1, STR_ICON_ARROW_R, st->accent);   /* CR9: still (a retained panel) */
}

/* CR9: will the selected row's value scroll (a marquee changes every 7 ticks)? draw_value's room */
static int value_moves(const CrRow *row, int inner_r)
{
    char b[48];
    int maxw, idx, locked;
    if (!row) return 0;
    maxw = inner_r - crl.lx - 15 - ui_text_width(cr_row_label(row)) - 10 - 16;
    if (cr.browse >= 0) { idx = cr.browse; locked = row->kind != RK_ACTION || cr_value_locked(row, idx); }
    else { idx = cr_row_value(&cr.draft, row); locked = 0; }
    switch (row->kind) {
    case RK_SLIDER: case RK_FLAG: return 0;
    case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN:
        return ui_text_width(locked ? "???" : cr_choice_name(row, idx, b)) > maxw - 14;
    default:
        return ui_text_width(locked ? "???" : cr_choice_name(row, idx, b)) > maxw - (locked ? 8 : 0);
    }
}

/* CR9: will the footer scroll (a marquee changes every 7 ticks)? */
static int footer_moves(const CrRow *row)
{
    int idx;
    if (!row) return 0;
    idx = cr.browse >= 0 ? cr.browse : cr_row_value(&cr.draft, row);
    if (crl.big) return cr.browse >= 0 && cr_value_locked(row, idx) && ui_text_width(cr_lock_hint(row, idx)) > crl.lw - 14;
    return cr.browse >= 0 && cr_value_locked(row, idx);
}

/* CR9: everything the list's pixels depend on but the selected row (in the panel's hole) */
static uint32_t list_key(int top, int glint, int inner_r)
{
    uint32_t k = ui_panel_key();
    const uint8_t *p = (const uint8_t *)&cr.draft;
    size_t i;
    for (i = 0; i < sizeof(cr.draft); i++) k = (k ^ p[i]) * 16777619u;
    k = ui_key_str(k, cr.nick);
    k = ui_key_int(k, list_style() | cr.tab << 4 | top << 8 | cr.row[cr.tab] << 16 | cr.show << 24 | crl.big << 26);
    k = ui_key_int(k, cr.browse | cr.district << 16 | cr.district_fixed << 17 | cr.mode << 18);
    k = ui_key_int(k, cr.preset_i | cr.outfit_i << 8 | cr.voice_i << 16);
    k = ui_key_int(k, cr.home_id);
    k = ui_key_int(k, DOOMO_MIN((int)cr.tab_t, 6) | glint << 4);
    k = ui_key_int(k, cr.pick_err);                   /* the selected row shakes */
    if (footer_moves(cr_cur_row()) || value_moves(cr_cur_row(), inner_r)) k = ui_key_int(k, (int)(cr.t / 7));
    return k;
}

static void draw_list(void)
{
    const UiStyle *st;
    const CrTab *tab = &cr_tabs[cr.tab];
    int style = list_style();
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh;
    int top = cr.top[cr.tab], vis = crl.rows, i, n = tab->nrows, shown = 0, drawn = 0, top_ord = 0;
    int bar, inner_r;
    int t = (int)cr.tab_t;
    /* CR9: on the main list the window is a retained panel (GBA): put back from the last frame while
     * nothing in it changes (the cursor and arrow stand still). Not under the persona / home panels
     * (animated). */
    int panel = UI_PANELS && cr.phase == CRP_MAIN && cr.tab != CR_TAB_PERSONA && cr.tab != CR_TAB_HOME;
    /* CR6: only the rows the current mode shows (cr_row_shown) */
    for (i = 0; i < n; i++) {
        if (!cr_row_shown(cr.tab, i)) continue;
        if (i < top) top_ord++;
        shown++;
    }
    bar = shown > vis;
    inner_r = x + w - (bar ? 12 : 7);
    st = ui_style(style);
    /* (its rect stops above the button bar, drawn after it) */
    if (panel && !plat_panel_begin(PANEL_CR_LIST, x, y, w + 3, DOOMO_MIN(h + 3, crl.sh - 13 - y),
                                   list_key(top, ui_window_glint(x, y, w, h), inner_r))) {
        ui_window_shadow(x, y, w, h);              /* kept: its drop shadow only */
        plat_panel_end();
        return;
    }
    if (panel && CR_CALM) plat_panel_solid();      /* a still backdrop */
    ui_window(x, y, w, h, style);
    /* header: icon + tab name > the selected row's section + page */
    {
        char pg[8], hb[64], b2[48];
        const char *sec = cr_row_section(cr.tab, cr.row[cr.tab]);
        int hx = x + 6, slide = t < 6 ? (6 - t) * 3 : 0, room;
        draw_icon(hx + slide, y + 4, cr.tab, 1, 0);
        doomo_snprintf(pg, sizeof(pg), "%d/%d", cr.tab + 1, CR_TAB_COUNT);
        room = w - 14 - 14 - ui_text_width(pg) - 6;
        if (sec[0] && strcmp(sec, tab->name) != 0 && cr.phase != CRP_GRID) {
            doomo_snprintf(hb, sizeof(hb), "%s " STR_ICON_ARROW_R " %s", tab->name, sec);
            if (ui_text_width(hb) > room)
                doomo_snprintf(hb, sizeof(hb), "%s " STR_ICON_ARROW_R " %s", tab->shortname, sec);
            if (ui_text_width(hb) > room)          /* the tab's icon says which tab: the section alone */
                doomo_snprintf(hb, sizeof(hb), "%s", sec);
        } else {
            doomo_snprintf(hb, sizeof(hb), "%s", tab->name);
        }
        ui_text_shadow(hx + 14 + slide, y + 5, cr_fit(hb, room, b2), st->title_bg, st->outline);
        ui_text(x + w - 8 - ui_text_width(pg), y + 5, pg, st->text_dim);
        plat_rect(x + 5, y + (crl.big ? 17 : 15), w - 10, 1, st->fill_lo);
    }
    for (i = top; i < n && drawn < vis; i++) {
        const CrRow *row = &tab->rows[i];
        int ry = crl.rows_y + drawn * crl.row_h;
        int sel = i == cr.row[cr.tab] && cr.phase == CRP_MAIN;
        int is_cur = i == cr.row[cr.tab];
        if (!cr_row_shown(cr.tab, i)) continue;
        drawn++;
        if (row->kind == RK_HEADER) {
            int tw = ui_text_width(row->label), cx = x + w / 2;
            plat_rect(x + 8, ry + 4, (w - 16 - tw) / 2 - 6, 1, st->fill_lo);
            plat_rect(cx + tw / 2 + 6, ry + 4, (w - 16 - tw) / 2 - 6, 1, st->fill_lo);
            ui_heart_flat(cx - tw / 2 - 4, ry + 4, 2, st->title_bg);
            ui_heart_flat(cx + tw / 2 + 3, ry + 4, 2, st->title_bg);
            ui_text(cx - tw / 2, ry + 1, row->label, st->title_bg);
            continue;
        }
        draw_list_row(row, ry, sel, is_cur, inner_r, st);
    }
    if (bar) {
        /* CR9: still arrows (no bob), so the list stays put between changes */
        int bx = x + w - 8, th = vis * crl.row_h - 2;
        int thumb = DOOMO_MAX(5, th * vis / shown);
        int tyy = crl.rows_y + (th - thumb) * top_ord / DOOMO_MAX(1, shown - vis);
        ui_round_rect(bx, crl.rows_y, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, tyy, 3, thumb, 1, st->accent);
        if (top_ord > 0) {                         /* ui_scroll_arrows' up arrow at rest */
            int ax = bx - 1, ay = crl.rows_y - 5;
            plat_rect(ax, ay + 2, 5, 1, st->outline);
            plat_rect(ax + 1, ay + 1, 3, 1, st->accent);
            plat_rect(ax + 2, ay, 1, 1, st->accent);
            plat_rect(ax + 1, ay + 2, 3, 1, st->accent);
        }
        if (top_ord + vis < shown) {               /* ... and its down arrow */
            int ax = bx - 1, ay = crl.rows_y + th + 1;
            plat_rect(ax, ay, 5, 1, st->outline);
            plat_rect(ax + 1, ay, 3, 1, st->accent);
            plat_rect(ax + 1, ay + 1, 3, 1, st->accent);
            plat_rect(ax + 2, ay + 2, 1, 1, st->accent);
        }
    }
    /* extra panels under short tabs */
    {
        int used = DOOMO_MIN(vis, shown) * crl.row_h;
        int py = crl.rows_y + used + 2, ph = crl.foot_y - 3 - py;
        if (cr.tab == CR_TAB_PERSONA) draw_persona_panel(x + 6, py, w - 12, ph);
        if (cr.tab == CR_TAB_HOME) draw_home_panel(x + 4, py, w - 8, ph);
    }
    plat_rect(x + 5, crl.foot_y - 2, w - 10, 1, st->fill_lo);
    if (cr.phase == CRP_MAIN) {
        draw_footer(cr_cur_row(), x + 7, crl.foot_y, w - 14);
        /* CR6: on big screens a second line says what the row is for */
        if (crl.big && !(cr.browse >= 0 && cr_value_locked(cr_cur_row(), cr.browse))) {
            char b2[48];
            ui_text(x + 7, crl.foot_y + 11, cr_fit(cr_row_help(cr_cur_row()), w - 14, b2), st->text_dim);
        }
    }
    if (panel) plat_panel_end();
}

/* ====================================================================== */
/* option picker                                                            */
static int pick_locked_fn_row;
/* CR12: the i-th choice at or after `from` that the draft's age may see, or -1 */
static int pick_nth_visible(const CrRow *row, int from, int i)
{
    int n = cr_row_count(row), v;
    for (v = from; v < n; v++)
        if (!cr_value_hidden(row, v) && i-- == 0) return v;
    return -1;
}

static void draw_pick(void)
{
    const UiStyle *st;
    const CrRow *row = cr_cur_row();
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh, i, n, vis = cr_pick_rows();
    int rh = 12, ry0 = y + 19, t = (int)cr.phase_t;
    char buf[48], b2[48], title[48];
    int cr_pick_top_visible;
    (void)pick_locked_fn_row;
    if (!row) return;
    n = cr_row_count(row);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) return;
    st = ui_style(WIN_CREAM);
    doomo_snprintf(title, sizeof(title), "%s", cr_row_label(row));
    draw_icon(x + 6, y + 4, cr.tab, 1, 0);
    ui_text_shadow(x + 20, y + 5, title, st->title_bg, st->outline);
    {   /* CR12: choices above the draft's age are not listed, nor counted */
        int nv = cr_visible_count(row), top = cr.pick_top;
        doomo_snprintf(buf, sizeof(buf), "%d/%d", cr_visible_pos(row, cr.pick_sel) + 1, nv);
        n = nv;
        while (top < cr_row_count(row) && cr_value_hidden(row, top)) top++;
        cr_pick_top_visible = top;
    }
    ui_text(x + w - 8 - ui_text_width(buf), y + 5, buf, st->text_dim);
    for (i = 0; i < vis; i++) {
        int idx = pick_nth_visible(row, cr_pick_top_visible, i), yy = ry0 + i * rh;
        int locked, sel;
        if (idx < 0) break;
        locked = cr_value_locked(row, idx);
        sel = idx == cr.pick_sel;
        int cur = idx == cr.pick_orig;
        const char *nm = locked ? "???" : cr_choice_name(row, idx, b2);
        if (sel) {
            int shake = cr.pick_err ? ((cr.pick_err & 2) ? 2 : -2) : 0;
            cr_pill(x + 11 + shake, yy - 1, w - 26, rh, st->accent_d, ui_darker(st->accent_d),
                    ui_lighter(st->accent_d));
            ui_cursor(x + 3, yy + 1);
        }
        if (cur) ui_star(x + w - 21, yy + 4, 3, PAL_UI_GOLD);
        if (locked) cr_padlock(x + 15, yy + 1, st->text_dim, sel ? st->accent_d : st->fill);
        ui_text(x + (locked ? 23 : 15), yy + 1, cr_fit(nm, w - 44, buf), locked ? st->text_dim : st->text);
    }
    if (n > vis) {
        int bx = x + w - 8, th = vis * rh - 2;
        int thumb = DOOMO_MAX(5, th * vis / n);
        int tyy = ry0 + (th - thumb) * cr_visible_pos(row, cr.pick_top) / DOOMO_MAX(1, n - vis);
        ui_round_rect(bx, ry0, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, tyy, 3, thumb, 1, st->accent);
    }
    {
        int fy = y + h - 13;
        plat_rect(x + 5, fy - 2, w - 10, 1, st->fill_lo);
        if (cr_value_locked(row, cr.pick_sel)) {
            cr_padlock(x + 8, fy + 1, PAL_PAINT(PAINT_CHERRY, 1), st->fill);
            ui_text(x + 16, fy, cr_fit(cr_lock_hint(row, cr.pick_sel), w - 22, buf), PAL_PAINT(PAINT_CHERRY, 0));
        } else {
            ui_text(x + 8, fy, STR_ICON_A " choose  " STR_ICON_B " back  L/R page", st->text_dim);
        }
    }
}

/* ====================================================================== */
/* swatch grid                                                              */
static void draw_color(void)
{
    const UiStyle *st;
    const CrRow *row = cr_cur_row();
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh, t = (int)cr.phase_t;
    int n, cols, rowsn, cw, chh, gx, gy, i, sx = 0, sy = 0;
    char buf[48], b2[48];
    if (!row) return;
    n = cr_row_count(row);
    cols = cr_color_cols(row);
    rowsn = (n + cols - 1) / cols;
    cw = DOOMO_MIN((w - 14) / cols, crl.big ? 20 : 16);
    chh = crl.big ? 16 : 12;
    gx = x + (w - cw * cols) / 2;
    gy = y + 20;
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) return;
    st = ui_style(WIN_CREAM);
    draw_icon(x + 6, y + 4, cr.tab, 1, 0);
    ui_text_shadow(x + 20, y + 5, cr_row_label(row), st->title_bg, st->outline);
    for (i = 0; i < n; i++) {
        int cx = gx + (i % cols) * cw, cy = gy + (i / cols) * chh;
        if (i == cr.pick_sel) { sx = cx; sy = cy; continue; }
        swatch(cx + 1, cy + 1, cw - 2, chh - 2, row->kind, i, cr_value_locked(row, i));
        if (i == cr.pick_orig) plat_rect(cx + cw / 2 - 1, cy + chh - 2, 2, 1, st->outline);
    }
    /* the selected swatch: bigger, framed, bouncing */
    {
        int b = ui_wave((int)cr.t, 18, 1) > 0, shake = cr.pick_err ? ((cr.pick_err & 2) ? 2 : -2) : 0;
        int cx = sx - 2 + shake, cy = sy - 2 - b;
        ui_round_rect(cx - 1, cy - 1, cw + 4, chh + 4, 3, st->outline);
        ui_round_rect(cx, cy, cw + 2, chh + 2, 3, PAL_WHITE);
        swatch(cx + 2, cy + 2, cw - 2, chh - 2, row->kind, cr.pick_sel,
               cr_value_locked(row, cr.pick_sel));
        ui_sparkle(cx + cw + 2, cy, (int)(cr.t / 3) & 15, PAL_UI_GOLD);
    }
    {
        int ny = gy + rowsn * chh + 4;
        int locked = cr_value_locked(row, cr.pick_sel);
        const char *nm = locked ? "???" : cr_choice_name(row, cr.pick_sel, b2);
        ui_text_center(x + w / 2, ny, nm, locked ? st->text_dim : st->text);
        if (locked) {
            int lines = ui_text_wrap(0, 0, w - 16, cr_lock_hint(row, cr.pick_sel), 0, 0);
            if (ny + 11 + lines * FONT_LINE < y + h - 2)
                ui_text_wrap(x + 8, ny + 11, w - 16, cr_lock_hint(row, cr.pick_sel),
                             PAL_PAINT(PAINT_CHERRY, 0), 1);
        } else if (ny + 22 < y + h) {
            ui_text_center(x + w / 2, y + h - 13, cr_fit(STR_ICON_A " paint!  " STR_ICON_B " back", w - 12, buf),
                           st->text_dim);
        }
    }
}

/* ====================================================================== */
/* START menu                                                               */
static void draw_menu(void)
{
    const UiStyle *st;
    int sw = crl.sw, sh = crl.sh, ih = crl.big ? 15 : 12;
    uint8_t l[CM_COUNT];
    int n = cr_menu_list(l), gaps = 0, yy = 0, prev = -1;
    int w = crl.big ? 190 : 172, h, x, y, i, t = (int)cr.phase_t;
    char buf[48];
    for (i = 1; i < n; i++) if (cr_menu_group(l[i]) != cr_menu_group(l[i - 1])) gaps++;
    h = n * ih + gaps * 4 + 26;
    x = (sw - w) / 2;
    y = (sh - h) / 2 - 2;
    plat_shade_rect(0, 0, sw, sh, 9);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) return;
    st = ui_style(WIN_CREAM);
    ui_title(x + w / 2, y - 6, "Creator menu", WIN_CREAM);
    yy = y + 12;
    for (int k = 0; k < n; k++) {
        int en = 1, sel;
        const char *lbl;
        i = l[k];
        sel = i == cr.menu_sel;
        lbl = cr_menu_label(i, buf);
        if (prev >= 0 && cr_menu_group(i) != prev) {          /* a thin rule between the groups */
            plat_rect(x + 16, yy + 1, w - 32, 1, st->fill_lo);
            yy += 4;
        }
        prev = cr_menu_group(i);
        if (sel) {
            cr_pill(x + 14, yy - 1, w - 28, ih - 1, st->accent_d, ui_darker(st->accent_d),
                    ui_lighter(st->accent_d));
            ui_cursor(x + 5, yy + 1 + (crl.big ? 1 : 0));
        }
        if (i == CM_DONE) ui_heart(x + 21, yy + 4 + (crl.big ? 1 : 0), 3, PAL_PAINT(PAINT_ROSE, 1));
        if (i == CM_RANDOM)
            ui_star(x + 21, yy + 4 + (crl.big ? 1 : 0), 3, PAL_UI_GOLD);
        else if (i == CM_SPOOKY) plat_text(x + 18, yy + 1, STR_ICON_SKULL, PAL_PAINT(PAINT_GRAPE, 1));
        else if (i == CM_UNDO) plat_text(x + 18, yy + 1, STR_ICON_ARROW_L, st->accent);
        else if (i == CM_REDO) plat_text(x + 18, yy + 1, STR_ICON_ARROW_R, st->accent);
        else if (i == CM_COPY) plat_text(x + 18, yy + 1, STR_ICON_SMILE, PAL_PAINT(PAINT_MINT, 0));
        else if (i == CM_RESET) plat_text(x + 18, yy + 1, STR_ICON_BOLT, PAL_UI_RED);
        else if (i == CM_QUIT) plat_text(x + 18, yy + 1, STR_ICON_X, PAL_UI_RED);
        ui_text(x + 28, yy + 1 + (crl.big ? 1 : 0), lbl, en ? st->text : st->text_dim);
        yy += ih;
    }
}

/* ====================================================================== */
/* copy-a-look picker                                                       */
static void draw_copy(void)
{
    const UiStyle *st;
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh, i, vis = cr_pick_rows();
    int rh = 12, ry0 = y + 32, t = (int)cr.phase_t;
    char buf[48], b2[48];
    vis = DOOMO_MIN(vis, (h - 50) / rh);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) return;
    st = ui_style(WIN_CREAM);
    ui_text_shadow(x + 8, y + 5, "Copy a look", st->title_bg, st->outline);
    /* source pills */
    {
        int bw = (w - 20) / 2;
        cr_pill(x + 6, y + 16, bw, 12, cr.copy_src == 0 ? st->accent : st->fill_lo, st->outline,
                cr.copy_src == 0 ? ui_lighter(st->accent) : st->fill_hi);
        ui_text_center(x + 6 + bw / 2, y + 18, "Town", cr.copy_src == 0 ? PAL_WHITE : st->text_dim);
        cr_pill(x + 10 + bw, y + 16, bw, 12, cr.copy_src == 1 ? st->accent : st->fill_lo, st->outline,
                cr.copy_src == 1 ? ui_lighter(st->accent) : st->fill_hi);
        ui_text_center(x + 10 + bw + bw / 2, y + 18, "Premade", cr.copy_src == 1 ? PAL_WHITE : st->text_dim);
    }
    if (cr.copy_count <= 0) {
        ui_text_wrap(x + 10, ry0 + 4, w - 20, "Nobody lives here yet! Try the premade citizens.",
                     st->text_dim, 1);
    }
    for (i = 0; i < vis && cr.copy_top + i < cr.copy_count; i++) {
        int k = cr.copy_top + i, yy = ry0 + i * rh, id = cr_copy_id_at(k);
        const DoomoResident *r = cr_copy_res(id);
        int sel = k == cr.copy_sel;
        if (!r) continue;
        if (sel) {
            cr_pill(x + 11, yy - 1, w - 22, rh, st->accent_d, ui_darker(st->accent_d),
                    ui_lighter(st->accent_d));
            ui_cursor(x + 3, yy + 1);
        }
        if (RES_DISTRICT(id)) plat_text(x + 14, yy + 1, STR_ICON_PENTA, PAL_PAINT(PAINT_CHERRY, 1));
        else ui_heart(x + 17, yy + 5, 2, PAL_PAINT(PAINT_ROSE, 1));
        {
            const char *sp;
            doomo_snprintf(b2, sizeof(b2), "%s", r->name[0] ? r->name : "???");
            ui_text(x + 24, yy + 1, cr_fit(b2, w / 2, buf), st->text);
            sp = cr_fit(doomo_species_names[RES_SPECIES(r) % SPECIES_COUNT], w / 2 - 34, buf);
            ui_text(x + w - 12 - ui_text_width(sp), yy + 1, sp, st->text_dim);
        }
    }
    ui_text(x + 8, y + h - 13, STR_ICON_A " copy  SEL/L/R source", st->text_dim);
}

/* ====================================================================== */
/* moving in!                                                               */
static void draw_building(int x, int ground, int w, int hell, int t)
{
    int h = w * 5 / 4, i, j, y = ground - h;
    uint8_t wall = hell ? PAL_RAMP(RAMP_VAPE, 3) : PAL_RAMP(RAMP_PASTEL_PINK, 6);
    uint8_t wall_d = hell ? PAL_RAMP(RAMP_VAPE, 1) : PAL_RAMP(RAMP_PASTEL_PINK, 4);
    uint8_t wall_l = hell ? PAL_RAMP(RAMP_VAPE, 4) : PAL_RAMP(RAMP_PASTEL_PINK, 7);
    uint8_t roof = hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_SKY, 1);
    uint8_t roof_d = hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_PAINT(PAINT_SKY, 0);
    int cols = 3, rows = 4, ww = w / 6, wh = h / 9;
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 3, wall_d);
    ui_round_rect(x, y, w, h, 2, wall);
    plat_rect(x + 2, y + 2, w / 3, h - 4, wall_l);
    plat_rect(x + w - 4, y + 4, 2, h - 8, wall_d);
    for (i = 0; i < 8; i++)
        plat_rect(x - 4 + i, y - 1 - i, w + 8 - 2 * i, 1, i < 2 ? roof_d : roof);
    if (hell) {
        for (i = 0; i < w; i += 7) plat_rect(x + i, y - 12, 3, 4, roof);
        ui_text(x + w / 2 - 3, y - 20 + ui_wave(t, 40, 2), STR_ICON_PENTA, PAL_RAMP(RAMP_LAVA, 5));
    } else {
        ui_heart(x + w / 2, y - 15 + ui_wave(t, 40, 2), 4, PAL_PAINT(PAINT_ROSE, 1));
    }
    for (j = 0; j < rows; j++)
        for (i = 0; i < cols; i++) {
            int wx = x + (w - cols * ww * 3 / 2) / 2 + i * ww * 3 / 2 + ww / 4;
            int wy = y + 6 + j * (wh + 5);
            int lit = ((j * 3 + i + t / 20) % 5) != 0;
            plat_rect(wx - 1, wy - 1, ww + 2, wh + 2, wall_d);
            plat_rect(wx, wy, ww, wh, lit ? PAL_UI_GOLD : PAL_PAINT(PAINT_SKY, 2));
            plat_rect(wx, wy, ww, 1, PAL_WHITE);
        }
    /* door */
    plat_rect(x + w / 2 - 6, ground - 16, 12, 16, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(x + w / 2 - 5, ground - 15, 10, 15, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(x + w / 2 + 2, ground - 8, 2, 2, PAL_UI_GOLD);
}


/* the makeover stage: curtains, a spotlight and a sparkly floor */
static void draw_curtain_stage(int sw, int ground, int t)
{
    int i, cw = sw / 5, open = DOOMO_MIN(t * 3, cw);
    uint8_t red = PAL_PAINT(PAINT_ROSE, 1), red_d = PAL_PAINT(PAINT_ROSE, 0), red_l = PAL_PAINT(PAINT_ROSE, 2);
    ui_fill_ellipse(sw / 2, ground, sw / 4, crl.big ? 10 : 7, PAL_RAMP(RAMP_CANDY, 6));
    ui_fill_ellipse(sw / 2, ground, sw / 4 - 6, crl.big ? 7 : 5, PAL_RAMP(RAMP_CANDY, 7));
    for (i = 0; i < 2; i++) {
        int x = i ? sw - cw * 2 + open : 0, w = cw * 2 - open, k;
        if (w <= 0) continue;
        plat_rect(x, 0, w, ground + 4, red);
        for (k = 4; k < w; k += 9) {
            plat_rect(x + k, 0, 2, ground + 4, red_d);
            plat_rect(x + k + 3, 0, 1, ground + 4, red_l);
        }
    }
    plat_rect(0, 0, sw, crl.big ? 14 : 10, red_d);
    for (i = 0; i < sw; i += 12) ui_fill_circle(i + 6, crl.big ? 14 : 10, 5, red);
    for (i = 0; i < sw; i += 24) ui_star(i + 12, crl.big ? 6 : 4, 3, PAL_UI_GOLD);
}

static void draw_cottage(int x, int ground, int w, int t)
{
    int h = w * 3 / 4, i, y = ground - h;
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 2, PAL_RAMP(RAMP_WOOD, 2));
    ui_round_rect(x, y, w, h, 2, PAL_RAMP(RAMP_CANDY, 6));
    plat_rect(x + 2, y + 2, w / 3, h - 4, PAL_RAMP(RAMP_CANDY, 7));
    for (i = 0; i < w / 2 + 4; i++)
        plat_rect(x - 4 + i, y - 1 - i, w + 8 - 2 * i, 1, PAL_PAINT(PAINT_ROSE, i < 2 ? 0 : (i & 4) ? 2 : 1));
    plat_rect(x + w - 14, y - w / 3 - 6, 6, 10, PAL_RAMP(RAMP_WOOD, 3));
    for (i = 0; i < 3; i++)
        ui_fill_circle(x + w - 11 + ui_wave(t + i * 13, 50, 2), y - w / 3 - 10 - i * 5 - (t / 4) % 5, 2 + i / 2,
                       PAL_WHITE);
    plat_rect(x + 5, y + 6, w / 4, h / 4, PAL_UI_GOLD);
    plat_rect(x + w - 5 - w / 4, y + 6, w / 4, h / 4, PAL_UI_GOLD);
    plat_rect(x + w / 2 - 6, ground - 18, 12, 18, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(x + w / 2 - 5, ground - 17, 10, 17, PAL_RAMP(RAMP_WOOD, 4));
    ui_heart(x + w / 2, ground - 22, 3, PAL_PAINT(PAINT_ROSE, 1));
}

/* little parabolic hops: quick ones while walking, big happy ones after */
static int movein_hop(int t, int walking)
{
    int span = walking ? 12 : 16, p = walking ? t % 12 : t % 26;
    int amp = (walking ? 4 : 9) * (crl.big ? 2 : 1);
    if (p >= span) return 0;
    return amp * p * (span - p) * 4 / (span * span);
}

static void draw_movein(void)
{
    int sw = crl.sw, sh = crl.sh, t = (int)cr.phase_t, hell = cr_is_hellish() && cr.mode != CR_PLAYER;
    int ground = sh - (crl.big ? 50 : 34);
    int bw = crl.big ? 96 : 64, bx = sw - bw - (crl.big ? 30 : 18);
    int sc = crl.fig_scale, i, fx, walking;
    const DoomoImage *im;
    if (cr.mode == CR_EDIT) {
        fx = sw / 2;
        walking = 0;
    } else {
        int walk = DOOMO_MIN(t, 60);
        int fx0 = crl.big ? 70 : 46, fx1 = bx - (crl.big ? 40 : 26);
        fx = fx0 + (fx1 - fx0) * walk / 60;
        walking = t < 60;
    }
    ui_backdrop(hell ? WIN_HELL : WIN_CUTE, (uint32_t)t);
    /* ground */
    plat_rect(0, ground, sw, sh - ground, hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(0, ground, sw, 2, hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_GRASS, 3));
    plat_checker(0, ground + 2, sw, 1, hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_GRASS, 3));
    for (i = 0; i < 9; i++) {
        int gx = (i * 53 + 11) % sw, gy = ground + 6 + (i * 7) % DOOMO_MAX(sh - ground - 10, 1);
        if (hell) plat_rect(gx, gy, 2, 1, PAL_RAMP(RAMP_LAVA, 4));
        else ui_heart_flat(gx, gy, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    }
    if (cr.mode == CR_EDIT) draw_curtain_stage(sw, ground, t);
    else if (cr.mode == CR_PLAYER) draw_cottage(bx - (crl.big ? 8 : 4), ground, bw + (crl.big ? 16 : 8), t);
    else draw_building(bx, ground, bw, hell, t);
    im = figure_image(RES_LOOK_VIEW(&cr.draft), RES_ALIVE, cr_preview_expr());
    if (im) {
        int fw = im->w * sc, fh = im->h * sc, hop = movein_hop(t, walking);
        ui_fill_ellipse(fx, ground + 2, fw / 4 - hop / 3, 2 + sc,
                        hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_GRASS, 3));
        plat_image(fx - fw / 2, ground - fh + 3 - hop, im, sc, 0, 0);
    }
    draw_sparks(fx, ground, 0, 0, sw, sh);
    if (t > 50 && ((t / 10) & 1)) ui_heart(fx + (crl.big ? 30 : 20), ground - (crl.big ? 150 : 84) - (t % 20),
                                         3, PAL_PAINT(PAINT_ROSE, 1));
    /* caption (below where the unlock toasts slide in) */
    {
        int cw = DOOMO_MIN(sw - 24, crl.big ? 260 : 196), ch = crl.big ? 32 : 28;
        int cx = (sw - cw) / 2, cy = crl.big ? 26 : 21;
        char buf[64];
        if (ui_window_pop(cx, cy, cw, ch, hell ? WIN_DARK : WIN_CREAM, t - 4)) {
            const UiStyle *st = ui_style(hell ? WIN_DARK : WIN_CREAM);
            int floor, room;
            ui_text_center(sw / 2, cy + 5, cr.msg, st->text);
            if (cr.mode == CR_NEW) {
                cr_home_floor_room(cr.result, &floor, &room);
                doomo_snprintf(buf, sizeof(buf), crl.big ? "%s, Floor %d Room %d" : "%s %dF Room %d",
                               cr_home_names[RES_DISTRICT(cr.result)], floor, room);
            } else if (cr.mode == CR_PLAYER) {
                doomo_snprintf(buf, sizeof(buf), "Ready for HaHaland!");
            } else {
                doomo_snprintf(buf, sizeof(buf), "Ta-da! Looking fabulous!");
            }
            ui_text_center(sw / 2, cy + (crl.big ? 18 : 16), buf, st->text_dim);
            ui_sparkle(cx + 6, cy + 5, (t / 3) & 15, PAL_UI_GOLD);
            ui_sparkle(cx + cw - 7, cy + ch - 7, (t / 3 + 8) & 15, PAL_UI_GOLD);
        }
    }
    if (t > 24) ui_hint(STR_ICON_A " Yay!", hell ? WIN_DARK : WIN_CUTE);
}

/* ====================================================================== */
/* hints                                                                    */
static void draw_hint(void)
{
    const CrRow *row = cr_cur_row();
    const char *s = STR_ICON_A " Edit  L/R Tab  START Menu";
    int style = list_style();
    switch (cr.phase) {
    case CRP_TURN:
        s = crl.big ? STR_ICON_ARROW_L STR_ICON_ARROW_R " Turn  " STR_ICON_ARROW_U " Walk  "
                      STR_ICON_ARROW_D " Spin  " STR_ICON_A " Tumble  " STR_ICON_B " Back"
                    : STR_ICON_ARROW_L STR_ICON_ARROW_R " Turn " STR_ICON_ARROW_U " Walk "
                      STR_ICON_A " Roll " STR_ICON_B " Back";
        break;
    case CRP_HERO:
        s = STR_ICON_ARROW_U STR_ICON_ARROW_D " Look  " STR_ICON_A " Choose  " STR_ICON_B " Back";
        break;
    case CRP_PICK: case CRP_COLOR: case CRP_COPY:
        s = STR_ICON_A " Choose  " STR_ICON_B " Back";
        break;
    case CRP_GRID:          /* CR6 */
        s = crl.big ? STR_ICON_A " Wear  " STR_ICON_B " Back  L/R Filter  START Find  SELECT All"
                    : STR_ICON_A " Wear " STR_ICON_B " Back L/R Filter START Find";
        break;
    case CRP_MENU:
        if (cr.menu_sel == CM_RANDOM && cr.tab != CR_TAB_HOME)
            s = STR_ICON_A " Select  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Scope  " STR_ICON_B " Close";
        else
            s = crl.big ? STR_ICON_A " Select  " STR_ICON_B " Close  (X/Y undo/redo)"
                        : STR_ICON_A " Select  " STR_ICON_B " Close";
        break;
    default:
        if ((cr.t / 120) % 4 == 2) {          /* every few seconds: a tip */
            s = crl.big ? "SELECT Turn around  Square Undo  Triangle Redo"
                        : "SELECT Turn  SEL+" STR_ICON_A " Undo  SEL+" STR_ICON_B " Redo";
            break;
        }
        if ((cr.t / 120) % 4 == 3) {          /* CR6: the section jump */
            s = crl.big ? "SELECT + Up/Down: jump to the next section   L/R Tab"
                        : "SELECT+Up/Down: next section";
            break;
        }
        if (!crl.big && (cr.t / 120) % 4 == 1 && row && cr_row_help(row)[0]) {   /* CR6: what the row is for */
            s = cr_row_help(row);
            break;
        }
        if (row && row->kind == RK_SLIDER)
            s = crl.big ? STR_ICON_ARROW_L STR_ICON_ARROW_R " Adjust  L/R Tab  SELECT Turn  START Menu"
                        : STR_ICON_ARROW_L STR_ICON_ARROW_R " Adjust  L/R Tab  START Menu";
        else if (crl.big)
            s = STR_ICON_A " Edit  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Change  L/R Tab  SELECT Turn  START Menu";
        else
            s = STR_ICON_A " Edit " STR_ICON_ARROW_L STR_ICON_ARROW_R " Change L/R Tab START Menu";
        break;
    }
    /* CR9: the button bar is a retained panel (GBA): put back while its text is the same */
    if (UI_PANELS && !plat_panel_begin(PANEL_CR_HINT, 0, crl.sh - 13, crl.sw, 13,
                                       ui_key_str(ui_key_int(ui_panel_key(), style | crl.big << 4), s))) {
        plat_panel_end();
        return;
    }
    /* a still backdrop under it: not while a window pops open (its shadow reaches into the bar) */
    if (UI_PANELS && CR_CALM && (cr.phase == CRP_MAIN || cr.phase == CRP_GRID) && cr.phase_t >= 12)
        plat_panel_solid();
    ui_hint(s, style);
    if (UI_PANELS) plat_panel_end();
}

/* ====================================================================== */
/* PA2: the card texts, shared with the width test (tests/test_creator.c) */
const char *cr_hero_name(int i)
{
    static const char *const nm[3] = { "Sunny", "Marvin", "Make your own" };
    return nm[i < 0 ? 0 : i > 2 ? 2 : i];
}

const char *cr_hero_tag(int i)
{
    static const char *const tg[3] = { "A bunny. She/her.", "A fox. He/him.", "Pick every detail" };
    return tg[i < 0 ? 0 : i > 2 ? 2 : i];
}

static void draw_hero(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int i, ch = (crl.lh - 22) / 3, y0 = crl.ly + 19;
    ui_window(crl.lx, crl.ly, crl.lw + 2, crl.lh + 3, WIN_CUTE);
    ui_text_center(crl.lx + crl.lw / 2, crl.ly + 6, "Who are you?", st->text);
    for (i = 0; i < 3; i++) {
        int y = y0 + i * ch;
        if (i == cr.hero_sel) ui_cursor(crl.lx + 4, y + ch / 2 - 4);
        ui_text(crl.lx + 16, y + ch / 2 - 9, cr_hero_name(i), st->text);
        ui_text(crl.lx + 16, y + ch / 2 + 1, cr_hero_tag(i), st->text_dim);
    }
}

void cr_draw(void)
{
    if (!scene_scratch_mine(CR_SCRATCH_TAG)) { plat_clear(PAL_UI_PINK); return; }
    cr_layout();
    if (cr.phase == CRP_MOVEIN) { draw_movein(); return; }
    ui_glint_hold(CR_CALM);                     /* CR9 (GBA): still windows, so the panels are kept */
    /* CR9: the list window (and every picker over it) and the preview pane are opaque inside their
     * rounded corners: no backdrop under them; on the GBA (CR_CALM) the backdrop stands still */
    {
        int holes[8];
        holes[0] = crl.lx + 1; holes[1] = crl.ly + 5; holes[2] = crl.lx + crl.lw - 1; holes[3] = crl.ly + crl.lh - 5;
        holes[4] = crl.px + 1; holes[5] = crl.py + 5; holes[6] = crl.px + crl.pw - 1; holes[7] = crl.py + crl.ph - 5;
        ui_backdrop_holes(cr_is_hellish() ? WIN_DARK : WIN_CUTE, CR_CALM ? 0u : cr.t, holes, 2);
    }
    draw_pane();
    switch (cr.phase) {
    case CRP_HERO:  draw_hero(); break;
    /* CR9: a picker window covers the list window exactly once it has popped open: no list under it */
    case CRP_PICK:  if (cr.phase_t < 12) draw_list(); draw_pick(); break;
    case CRP_COLOR: if (cr.phase_t < 12) draw_list(); draw_color(); break;
    case CRP_COPY:  if (cr.phase_t < 12) draw_list(); draw_copy(); break;
    case CRP_GRID:  if (cr.phase_t < 12) draw_list(); cr_grid_draw(); break;
    case CRP_TURN:
        draw_list();
        plat_shade_rect(crl.lx, crl.ly, crl.lw + 2, crl.lh + 3, 10);
        break;
    default: draw_list(); break;
    }
    draw_hint();
    /* CR9: the tab shelf last: a retained panel makes every later 2D call on the GBA check whether
     * it touches it, so nothing is drawn after the panels but what lies elsewhere (the menu spoils
     * them, which is fine) */
    if (cr.phase != CRP_HERO) draw_tabs();
    if (cr.phase == CRP_MENU) draw_menu();
    ui_glint_hold(0);
    /* CR9 (GBA): a new look being drawn in slices gets what is left of this tick (one layer group a
     * tick while a direction is held: the list keeps up, the figure catches up when it rests) */
    if (CR_CALM && !btn_down(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) portrait_pump(16);
}
