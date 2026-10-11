/* creator_draw.c - everything SCENE_CREATOR puts on screen (owner:
 * creator): the tab strip, the live preview stage (full-body figure, a
 * close-up bust on the face tabs, the turntable, sparkles, the voice
 * bubble, locked silhouettes), the option list with inline sliders /
 * swatches / toggles, the option picker, the swatch grid, the START menu,
 * the copy-a-look picker and the "moving in!" finale. Layouts adapt to
 * 240x160 (GBA) and 320x240 (PS1).
 */
#include "creator_int.h"

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
    crl.foot_y = crl.ly + crl.lh - (crl.big ? 15 : 13);
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
static const char *fit(const char *s, int maxw, char *buf)
{
    int n, i;
    if (ui_text_width(s) <= maxw) return s;
    n = (int)strlen(s);
    if (n > 46) n = 46;
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
static const char *marquee(const char *s, int maxw, char *buf)
{
    int n = (int)strlen(s), k = maxw / FONT_ADVANCE, span, off, i;
    if (ui_text_width(s) <= maxw || k < 3) return fit(s, maxw, buf);
    if (k > 46) k = 46;
    span = n - k;
    off = (int)(cr.t / 7) % (span + 12);       /* scroll, then rest a moment */
    off = off < 6 ? 0 : off - 6;
    if (off > span) off = span;
    for (i = 0; i < k; i++) buf[i] = s[off + i];
    buf[k] = 0;
    return buf;
}

static void pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
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

static void padlock(int x, int y, uint8_t c, uint8_t hole)
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

/* The dimmed icons of the unselected tabs are blitted from small cached
 * images (one plat_image each instead of ~45 rects). */
static uint8_t icon_pix[CR_TAB_COUNT][CR_ICON_W * CR_ICON_W];
static DoomoImage icon_img[CR_TAB_COUNT];

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
    int b = sel ? (ui_wave((int)cr.t, 16, 1) > 0) : 0;
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
        int b = ui_wave((int)cr.t, 16, 1) > 0;
        ui_heart(kx, y + 4 - b, 3, st->accent);
    } else {
        ui_fill_circle(kx, y + 4, 2, st->outline);
        plat_rect(kx - 1, y + 3, 2, 2, st->fill_hi);
    }
}

/* ====================================================================== */
/* tab strip                                                                */
static void draw_tabs(void)
{
    const UiStyle *st = ui_style(list_style());
    int i, t = (int)cr.t;
    int ch = crl.big ? 21 : 17;
    int lx = crl.tx - 15, rx = crl.tx + CR_TAB_COUNT * crl.tcell + 2;
    /* bumper hints */
    pill(lx, crl.ty + 3, 12, 11, st->fill_lo, st->outline, st->fill_hi);
    ui_text(lx + 3, crl.ty + 5, "L", st->text);
    pill(rx, crl.ty + 3, 12, 11, st->fill_lo, st->outline, st->fill_hi);
    ui_text(rx + 3, crl.ty + 5, "R", st->text);
    for (i = 0; i < CR_TAB_COUNT; i++) {
        int x = crl.tx + i * crl.tcell, w = crl.tcell - 2;
        if (i == cr.tab) continue;
        pill(x, crl.ty + 2, w, ch - 2, st->fill_lo, st->outline, st->fill_hi);
        plat_image(x + (w - CR_ICON_W) / 2, crl.ty + 2 + (ch - 2 - CR_ICON_W) / 2, dim_icon(i), 1, 0, 0);
    }
    /* the selected tab: raised, bigger, bouncing, with a pop when it changes */
    {
        int x = crl.tx + cr.tab * crl.tcell - 1, w = crl.tcell, b = ui_wave(t, 20, 1) > 0;
        int pop = cr.tab_t < 6 ? (6 - (int)cr.tab_t) / 2 : 0;
        int y = crl.ty - b - pop;
        uint8_t acc = PAL_PAINT(cr_tabs[cr.tab].paint, 1);
        ui_round_rect(x + 1, y + 3, w, ch + 1, 4, PAL_RAMP(RAMP_STONE, 2));
        ui_round_rect(x, y, w, ch + 1, 4, ui_darker(ui_darker(acc)));
        ui_round_rect(x + 1, y + 1, w - 2, ch - 1, 3, PAL_WHITE);
        ui_round_rect(x + 2, y + 2, w - 4, ch - 3, 2, ui_lighter(ui_lighter(acc)));
        draw_icon(x + (w - CR_ICON_W) / 2, y + (ch + 1 - CR_ICON_W) / 2, cr.tab, 1, 0);
        ui_sparkle(x + w - 2, y + 1, (t / 3) & 15, PAL_UI_GOLD);
    }
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

/* Effect frames of the close-up bust change every 2^FX_SHIFT ticks (each
 * change is a full ~0.75M-cycle bust render on the GBA). */
#if defined(DOOMO_PLATFORM_GBA)
#define FX_SHIFT 4
#else
#define FX_SHIFT 3
#endif

static uint32_t anim_phase(uint32_t t, int expr, const DoomoLook *l, int status)
{
    uint32_t ph = 0;
    int fx = l->aura != AURA_NONE || status == RES_POSSESSED || expr == EXPR_LOVE ||
             expr == EXPR_HAPPY || expr == EXPR_SAD || expr == EXPR_ANGRY || expr == EXPR_SLEEPY ||
             (l->cheeks & CHEEK_SPARKLES) || l->halo == HALO_STARS || l->halo == HALO_FLAME_RING ||
             l->hair_style == HAIR_FLAME;
    ph |= (t % 110u) >= 106u;
    ph |= ((t >> 5) & 1u) << 1;
    ph |= (uint32_t)((t % 270u) >= 262u) << 2;
    if (fx) ph |= ((t >> FX_SHIFT) & 3u) << 3;
    if (expr == EXPR_TALK) ph |= ((t >> 2) & 1u) << 5;
    return ph;
}

/* The close-up bust of the draft (rendered into the shared buffer). */
static const DoomoImage *bust_image(int status, int expr)
{
    uint32_t key = look_hash(&cr.shown) ^ (uint32_t)(status * 7919 + expr * 104729 + crl.big) ^
                   (anim_phase(cr.t, expr, &cr.shown, status) * 2654435761u) ^ 0xB0570000u;
    if (cr.pix_kind != CRB_PORTRAIT || cr.pix_key != key || !cr.img.uid) {
        portrait_render(&cr.shown, status, expr, (int)cr.t, cr.pix);
        cr.img.w = PORTRAIT_W;
        cr.img.h = PORTRAIT_H;
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

/* The turntable body (fighter render, 8 directions). */
static const DoomoImage *turn_image(int status, int *flip)
{
    static const uint8_t views[8] = { 4, 3, 2, 1, 0, 1, 2, 3 };
    int v8 = cr.view8 & 7, anim, view = views[v8];
    uint32_t key;
    *flip = v8 >= 1 && v8 <= 3;
    anim = cr.roll ? 5 : cr.walking ? (int)((cr.t >> 2) & 3) : (int)((cr.t >> 4) & 1);
    if (!cr.walking && !cr.roll) anim = ((cr.t >> 4) & 1) ? 1 : 0;   /* gentle idle bob */
    key = look_hash(&cr.shown) ^ (uint32_t)(view * 31 + anim * 977 + status * 7) ^ 0xF1670000u;
    if (cr.pix_kind != CRB_FIGHTER || cr.pix_key != key || !cr.img.uid) {
        fighter_render(&cr.shown, status, view, anim, -1, cr.pix);
        cr.img.w = FIGHTER_W;
        cr.img.h = FIGHTER_H;
        cr.img.pix = cr.pix;
        cr.img.uid = doomo_image_new_uid();
        cr.pix_kind = CRB_FIGHTER;
        cr.pix_key = key;
    }
    return &cr.img;
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
static void draw_room(int x, int y, int w, int h, int floor_y, const Stage *sg)
{
    int i, b1 = y + (floor_y - y) / 3, b2 = y + (floor_y - y) * 2 / 3;
    plat_rect(x, y, w, b1 - y, sg->wall[0]);
    plat_rect(x, b1, w, b2 - b1, sg->wall[1]);
    plat_rect(x, b2, w, floor_y - b2, sg->wall[2]);
    plat_checker(x, b1, w, 1, sg->wall[0]);
    plat_checker(x, b2, w, 1, sg->wall[1]);
    /* drifting polka hearts on the wallpaper */
    for (i = 0; i < 6; i++) {
        int hx = x + 6 + ((i * 37 + (int)(cr.t / 4)) % DOOMO_MAX(w - 12, 1));
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
    pill(x, y, w, 13, st->title_bg, st->outline, ui_lighter(st->title_bg));
    if (cr.mode == CR_PLAYER) ui_star(x + 7, y + 6, 3, PAL_UI_GOLD);
    else if (cr_is_hellish()) plat_text(x + 4, y + 3, STR_ICON_PENTA, PAL_PAINT(PAINT_CHERRY, 1));
    else ui_heart(x + 7, y + 6, 3, PAL_PAINT(PAINT_ROSE, 1));
    ui_text_shadow(x + 13, y + 3, fit(name, w - 18, buf), st->title, st->outline);
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

static void lock_bubble(int x, int y, int w)
{
    const char *hint = "";
    const CrRow *row = cr_cur_row();
    int lines, h;
    if (cr.phase == CRP_PICK || cr.phase == CRP_COLOR) hint = cr_lock_hint(row, cr.pick_sel);
    else if (cr.browse >= 0) hint = cr_lock_hint(row, cr.browse);
    if (!hint[0]) hint = "Locked!";
    lines = ui_text_wrap(0, 0, w - 8, hint, 0, 0);
    h = lines * FONT_LINE + 15;
    ui_round_rect(x, y, w, h, 3, PAL_RAMP(RAMP_VAPE, 1));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, PAL_RAMP(RAMP_VAPE, 6));
    padlock(x + 6, y + 3, PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 6));
    ui_text(x + 14, y + 3, "Locked!", PAL_RAMP(RAMP_VAPE, 0));
    ui_text_wrap(x + 4, y + 12, w - 8, hint, PAL_RAMP(RAMP_VAPE, 1), 1);
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
    int face_cam = (cr.tab == CR_TAB_FACE || cr.tab == CR_TAB_HAIR) && cr.phase != CRP_TURN &&
                   cr.phase != CRP_COPY;
    int locked = cr.shown_locked;
    uint8_t sil = PAL_RAMP(RAMP_VAPE, 4);
    stage_colors(&sg);
    st = ui_style(sg.style);
    if (hop > 0) hop = -hop;
    /* frame */
    ui_window(x, y, w, h, sg.style);
    ui_round_rect(ix - 1, iy - 1, iw + 2, ih + 2, 3, st->outline);
    ui_round_rect(ix, iy, iw, ih, 2, sg.wall[0]);
    draw_room(ix + 1, iy + 1, iw - 2, ih - 2, floor_y, &sg);
    draw_motes(ix, iy, iw, floor_y - iy, &sg);
    /* rug */
    ui_fill_ellipse(cx, feet + 1, iw / 2 - 4, crl.big ? 9 : 6, sg.rug_d);
    ui_fill_ellipse(cx, feet, iw / 2 - 5, crl.big ? 8 : 5, sg.rug);
    ui_fill_ellipse(cx - 2, feet - 1, iw / 3, crl.big ? 4 : 2, sg.rug_l);

    if (cr.phase == CRP_TURN) {
        int flip, sc = crl.ftr_scale;
        const DoomoImage *im = turn_image(status, &flip);
        int fw = im->w * sc, fh = im->h * sc;
        int b = ui_wave((int)cr.t, 14, 2);
        ui_fill_ellipse(cx, feet, fw / 4 + 2, crl.big ? 4 : 3, sg.shadow);
        plat_image(cx - fw / 2, feet - fh + 2, im, sc, (flip ? IMG_FLIPX : 0) |
                   (locked ? IMG_SILHOUETTE : 0), sil);
        /* turn arrows */
        plat_text(ix + 3 - (b > 0), feet - fh / 2, STR_ICON_ARROW_L, st->accent);
        plat_text(ix + iw - 9 + (b > 0), feet - fh / 2, STR_ICON_ARROW_R, st->accent);
    } else if (face_cam) {
        const DoomoImage *im = bust_image(status, expr);
        int sc = crl.big ? crl.por_scale : 2, bw = im->w * sc, bh = im->h * sc;
        int by = crl.big ? floor_y - bh + 18 + hop / 2 : iy + 6 + hop / 2;
        int rr = crl.big ? bw / 2 : bw / 2 - 1;
        ui_fill_circle(cx, by + bh / 2, rr + 2, sg.wall[0]);
        ui_fill_circle(cx, by + bh / 2, rr, PAL_WHITE);
        ui_fill_circle(cx, by + bh / 2, rr - 2, sg.wall[0]);
        plat_image(cx - bw / 2, by, im, sc, locked ? IMG_SILHOUETTE : 0, sil);
    } else {
        const DoomoImage *im = figure_image(&cr.shown, status, expr);
        int sc = crl.fig_scale;
        if (im) {
            int fw = im->w * sc, fh = im->h * sc;
            int sq = cr.hop > 6 ? 1 : 0;
            ui_fill_ellipse(cx, feet, fw / 4 + (hop < -2 ? -2 : 1), crl.big ? 4 : 2, sg.shadow);
            plat_image(cx - fw / 2, feet - fh + 1 + hop + sq, im, sc,
                       locked ? IMG_SILHOUETTE : 0, sil);
        }
    }
    if (locked) {
        int tw = ui_text_big_width("???", crl.big ? 3 : 2);
        ui_text_big(cx - tw / 2, iy + ih / 3, "???", crl.big ? 3 : 2, PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1));
    }
    draw_sparks(cx, feet, ix, iy, iw, ih);
    /* bubbles & plates */
    if (locked) {
        lock_bubble(ix + 2, iy + ih - (crl.big ? 46 : 40), iw - 4);
    } else {
        name_plate(cx, iy + ih - 15, iw - 8);
        if (cr.talk) speech_bubble(ix + 3, iy + 3, iw - 6);
    }
    if (cr.spooky && !locked)
        ui_text_outline(ix + 4, iy + 3 + (cr.talk ? 30 : 0), "POSSESSED!", PAL_PAINT(PAINT_CHERRY, 1),
                        PAL_RAMP(RAMP_BLOOD, 0));
    if (cr.phase == CRP_TURN) {
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
        f = sel ? marquee(nm, maxw - 14, buf) : fit(nm, maxw - 14, buf);
        tw = ui_text_width(f);
        swatch(vx_right - tw - 13, y, 11, 8, row->kind, idx, locked);
        ui_text(vx_right - tw, y, f, locked ? st->text_dim : vc);
        return; }
    default: {
        const char *nm = locked ? "???" : cr_choice_name(row, idx, buf2);
        const char *f = sel ? marquee(nm, maxw - (locked ? 8 : 0), buf)
                            : fit(nm, maxw - (locked ? 8 : 0), buf);
        int tw = ui_text_width(f);
        if (locked) padlock(vx_right - tw - 8, y + 1, st->text_dim, st->fill);
        else if (row->kind == RK_ACTION && cr_value_locked(row, idx))
            padlock(vx_right - tw - 8, y + 1, st->text_dim, st->fill);
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
        doomo_snprintf(buf, sizeof(buf), "%s", cr_lock_hint(row, idx));
        padlock(x + 1, y + 1, PAL_PAINT(PAINT_CHERRY, 1), st->fill);
        ui_text(x + 9, y, fit(buf, w - 10, b2), PAL_PAINT(PAINT_CHERRY, 0));
        return;
    }
    switch (row->kind) {
    case RK_SLIDER:
        doomo_snprintf(buf, sizeof(buf), "%s " STR_ICON_ARROW_L STR_ICON_ARROW_R " %s",
                       row->names ? row->names[0] : "", row->names ? row->names[1] : "");
        s = buf;
        break;
    case RK_ENUM:
        doomo_snprintf(buf, sizeof(buf), "%s  %d/%d", cr_choice_name(row, idx, b3), idx + 1,
                       cr_row_count(row));
        s = buf;
        c = st->text;
        break;
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
    ui_text(x, y, fit(s, w, b2), c);
}

static void draw_persona_panel(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(list_style());
    int pt = persona_type(&cr.draft), q = cr.draft.persona[PERS_QUIRK] / 4;
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

static void draw_list(void)
{
    const UiStyle *st;
    const CrTab *tab = &cr_tabs[cr.tab];
    int style = list_style();
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh;
    int top = cr.top[cr.tab], vis = crl.rows, i, n = tab->nrows;
    int bar = n > vis, inner_r = x + w - (bar ? 12 : 7);
    int t = (int)cr.tab_t;
    ui_window(x, y, w, h, style);
    st = ui_style(style);
    /* header: icon + tab name + page */
    {
        char pg[8];
        int hx = x + 6, slide = t < 6 ? (6 - t) * 3 : 0;
        draw_icon(hx + slide, y + 4, cr.tab, 1, 0);
        ui_text_shadow(hx + 14 + slide, y + 5, tab->name, st->title_bg, st->outline);
        doomo_snprintf(pg, sizeof(pg), "%d/%d", cr.tab + 1, CR_TAB_COUNT);
        ui_text(x + w - 8 - ui_text_width(pg), y + 5, pg, st->text_dim);
        plat_rect(x + 5, y + (crl.big ? 17 : 15), w - 10, 1, st->fill_lo);
    }
    for (i = 0; i < vis && top + i < n; i++) {
        const CrRow *row = &tab->rows[top + i];
        int ry = crl.rows_y + i * crl.row_h;
        int sel = top + i == cr.row[cr.tab] && cr.phase == CRP_MAIN;
        int is_cur = top + i == cr.row[cr.tab];
        int lw;
        if (row->kind == RK_HEADER) {
            int tw = ui_text_width(row->label), cx = x + w / 2;
            plat_rect(x + 8, ry + 4, (w - 16 - tw) / 2 - 6, 1, st->fill_lo);
            plat_rect(cx + tw / 2 + 6, ry + 4, (w - 16 - tw) / 2 - 6, 1, st->fill_lo);
            ui_heart_flat(cx - tw / 2 - 4, ry + 4, 2, st->title_bg);
            ui_heart_flat(cx + tw / 2 + 3, ry + 4, 2, st->title_bg);
            ui_text(cx - tw / 2, ry + 1, row->label, st->title_bg);
            continue;
        }
        if (sel || (is_cur && cr.phase != CRP_TURN)) {
            int shake = cr.pick_err ? ((cr.pick_err & 2) ? 1 : -1) : 0;
            pill(x + 11 + shake, ry - 1, inner_r - x - 9, crl.row_h, st->accent_d,
                    ui_darker(st->accent_d), ui_lighter(st->accent_d));
            if (sel) ui_cursor(x + 3, ry + 1);
        }
        ui_text(x + 15, ry + 1, cr_row_label(row), is_cur ? st->text : st->text_dim);
        lw = ui_text_width(cr_row_label(row));
        draw_value(row, inner_r - (sel ? 9 : 4), ry + 1, inner_r - x - 15 - lw - 10 - (sel ? 16 : 6), sel);
        if (sel && row->kind != RK_SLIDER && row->kind != RK_FLAG) {
            int b = ui_wave((int)cr.t, 16, 1) > 0;
            plat_text(inner_r - 7 + b, ry + 1, STR_ICON_ARROW_R, st->accent);
        }
    }
    if (bar) {
        int bx = x + w - 8, th = vis * crl.row_h - 2;
        int thumb = DOOMO_MAX(5, th * vis / n);
        int tyy = crl.rows_y + (th - thumb) * top / DOOMO_MAX(1, n - vis);
        ui_round_rect(bx, crl.rows_y, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, tyy, 3, thumb, 1, st->accent);
        ui_scroll_arrows(bx - 1, crl.rows_y - 5, crl.rows_y + th + 1, top > 0, top + vis < n);
    }
    /* extra panels under short tabs */
    {
        int used = DOOMO_MIN(vis, n) * crl.row_h;
        int py = crl.rows_y + used + 2, ph = crl.foot_y - 3 - py;
        if (cr.tab == CR_TAB_PERSONA) draw_persona_panel(x + 6, py, w - 12, ph);
        if (cr.tab == CR_TAB_HOME) draw_home_panel(x + 4, py, w - 8, ph);
    }
    plat_rect(x + 5, crl.foot_y - 2, w - 10, 1, st->fill_lo);
    if (cr.phase == CRP_MAIN) draw_footer(cr_cur_row(), x + 7, crl.foot_y, w - 14);
}

/* ====================================================================== */
/* option picker                                                            */
static int pick_locked_fn_row;
static void draw_pick(void)
{
    const UiStyle *st;
    const CrRow *row = cr_cur_row();
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh, i, n, vis = cr_pick_rows();
    int rh = 12, ry0 = y + 19, t = (int)cr.phase_t;
    char buf[48], b2[48], title[48];
    (void)pick_locked_fn_row;
    if (!row) return;
    n = cr_row_count(row);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) return;
    st = ui_style(WIN_CREAM);
    doomo_snprintf(title, sizeof(title), "%s", cr_row_label(row));
    draw_icon(x + 6, y + 4, cr.tab, 1, 0);
    ui_text_shadow(x + 20, y + 5, title, st->title_bg, st->outline);
    doomo_snprintf(buf, sizeof(buf), "%d/%d", cr.pick_sel + 1, n);
    ui_text(x + w - 8 - ui_text_width(buf), y + 5, buf, st->text_dim);
    for (i = 0; i < vis && cr.pick_top + i < n; i++) {
        int idx = cr.pick_top + i, yy = ry0 + i * rh;
        int locked = cr_value_locked(row, idx), sel = idx == cr.pick_sel;
        int cur = idx == cr.pick_orig;
        const char *nm = locked ? "???" : cr_choice_name(row, idx, b2);
        if (sel) {
            int shake = cr.pick_err ? ((cr.pick_err & 2) ? 2 : -2) : 0;
            pill(x + 11 + shake, yy - 1, w - 26, rh, st->accent_d, ui_darker(st->accent_d),
                    ui_lighter(st->accent_d));
            ui_cursor(x + 3, yy + 1);
        }
        if (cur) ui_star(x + w - 21, yy + 4, 3, PAL_UI_GOLD);
        if (locked) padlock(x + 15, yy + 1, st->text_dim, sel ? st->accent_d : st->fill);
        ui_text(x + (locked ? 23 : 15), yy + 1, fit(nm, w - 44, buf), locked ? st->text_dim : st->text);
    }
    if (n > vis) {
        int bx = x + w - 8, th = vis * rh - 2;
        int thumb = DOOMO_MAX(5, th * vis / n);
        int tyy = ry0 + (th - thumb) * cr.pick_top / DOOMO_MAX(1, n - vis);
        ui_round_rect(bx, ry0, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, tyy, 3, thumb, 1, st->accent);
    }
    {
        int fy = y + h - 13;
        plat_rect(x + 5, fy - 2, w - 10, 1, st->fill_lo);
        if (cr_value_locked(row, cr.pick_sel)) {
            padlock(x + 8, fy + 1, PAL_PAINT(PAINT_CHERRY, 1), st->fill);
            ui_text(x + 16, fy, fit(cr_lock_hint(row, cr.pick_sel), w - 22, buf), PAL_PAINT(PAINT_CHERRY, 0));
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
            ui_text_center(x + w / 2, y + h - 13, fit(STR_ICON_A " paint!  " STR_ICON_B " back", w - 12, buf),
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
    int w = crl.big ? 190 : 150, h = CM_COUNT * ih + 26;
    int x = (sw - w) / 2, y = (sh - h) / 2 - 2, i, t = (int)cr.phase_t;
    char buf[48];
    plat_shade_rect(0, 0, sw, sh, 9);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) return;
    st = ui_style(WIN_CREAM);
    ui_title(x + w / 2, y - 6, "Creator menu", WIN_CREAM);
    for (i = 0; i < CM_COUNT; i++) {
        int yy = y + 12 + i * ih, en = cr_menu_enabled(i), sel = i == cr.menu_sel;
        const char *lbl = cr_menu_label(i, buf);
        if (sel) {
            pill(x + 14, yy - 1, w - 28, ih - 1, st->accent_d, ui_darker(st->accent_d),
                    ui_lighter(st->accent_d));
            ui_cursor(x + 5, yy + 1 + (crl.big ? 1 : 0));
        }
        if (i == CM_DONE) ui_heart(x + 21, yy + 4 + (crl.big ? 1 : 0), 3, PAL_PAINT(PAINT_ROSE, 1));
        if (i == CM_RANDOM_ALL || i == CM_RANDOM_TAB)
            ui_star(x + 21, yy + 4 + (crl.big ? 1 : 0), 3, PAL_UI_GOLD);
        if (i == CM_SPOOKY) plat_text(x + 18, yy + 1, STR_ICON_SKULL, PAL_PAINT(PAINT_GRAPE, 1));
        ui_text(x + 28, yy + 1 + (crl.big ? 1 : 0), lbl, en ? (sel ? st->text : st->text) : st->text_dim);
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
        pill(x + 6, y + 16, bw, 12, cr.copy_src == 0 ? st->accent : st->fill_lo, st->outline,
                cr.copy_src == 0 ? ui_lighter(st->accent) : st->fill_hi);
        ui_text_center(x + 6 + bw / 2, y + 18, "Town", cr.copy_src == 0 ? PAL_WHITE : st->text_dim);
        pill(x + 10 + bw, y + 16, bw, 12, cr.copy_src == 1 ? st->accent : st->fill_lo, st->outline,
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
            pill(x + 11, yy - 1, w - 22, rh, st->accent_d, ui_darker(st->accent_d),
                    ui_lighter(st->accent_d));
            ui_cursor(x + 3, yy + 1);
        }
        if (RES_DISTRICT(id)) plat_text(x + 14, yy + 1, STR_ICON_PENTA, PAL_PAINT(PAINT_CHERRY, 1));
        else ui_heart(x + 17, yy + 5, 2, PAL_PAINT(PAINT_ROSE, 1));
        {
            const char *sp;
            doomo_snprintf(b2, sizeof(b2), "%s", r->name[0] ? r->name : "???");
            ui_text(x + 24, yy + 1, fit(b2, w / 2, buf), st->text);
            sp = fit(doomo_species_names[r->look.species % SPECIES_COUNT], w / 2 - 34, buf);
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
    im = figure_image(&cr.draft.look, RES_ALIVE, cr_preview_expr());
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
    case CRP_PICK: case CRP_COLOR: case CRP_COPY:
        s = STR_ICON_A " Choose  " STR_ICON_B " Back";
        break;
    case CRP_MENU:
        s = crl.big ? STR_ICON_A " Select  " STR_ICON_B " Close  (X/Y undo/redo)"
                    : STR_ICON_A " Select  " STR_ICON_B " Close";
        break;
    default:
        if ((cr.t / 120) % 3 == 2) {          /* every few seconds: a tip */
            s = crl.big ? "SELECT Turn around  Square Undo  Triangle Redo"
                        : "SELECT Turn  SEL+" STR_ICON_A " Undo  SEL+" STR_ICON_B " Redo";
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
    ui_hint(s, style);
}

/* ====================================================================== */
void cr_draw(void)
{
    cr_layout();
    if (cr.phase == CRP_MOVEIN) { draw_movein(); return; }
    ui_backdrop(cr_is_hellish() ? WIN_DARK : WIN_CUTE, cr.t);
    draw_tabs();
    draw_pane();
    switch (cr.phase) {
    case CRP_PICK:  draw_list(); draw_pick(); break;
    case CRP_COLOR: draw_list(); draw_color(); break;
    case CRP_COPY:  draw_list(); draw_copy(); break;
    case CRP_TURN:
        draw_list();
        plat_shade_rect(crl.lx, crl.ly, crl.lw + 2, crl.lh + 3, 10);
        break;
    default: draw_list(); break;
    }
    draw_hint();
    if (cr.phase == CRP_MENU) draw_menu();
}
