/* overworld_fx.c - the overworld's look and HUD (owner: story).
 *
 *  - quest markers and emote bubbles: little glossy bubbles generated once
 *    at runtime (soft 3-tone shading, coloured outline, highlight) with a
 *    font glyph stamped in; drawn as billboards above heads so they sit in
 *    the right place on every platform (top-down GBA, 3D PS1)
 *  - weather / mood particles in screen space that drift with the camera:
 *    petals by day, fireflies at night, purple wisps near the Wall, embers
 *    and ash in the invaded town, bats over Hellvile, confetti and
 *    fireworks at the festival
 *  - the HUD: objective pill, clock and money, region banner, action
 *    prompt, a compass arrow toward the current goal, the mini-map
 *  - the pause panel
 */
#include "overworld_int.h"

/* ---- generated bubble images ------------------------------------------------------- */
#define MK_W 13
#define MK_H 15
#define EM_W 11
#define EM_H 12
enum { MK_MAIN, MK_SIDE, MK_READY, MK_COUNT };
static uint8_t mk_pix[MK_COUNT][MK_W * MK_H];
static DoomoImage mk_img[MK_COUNT];
static uint8_t em_pix[8][EM_W * EM_H];
static DoomoImage em_img[8];
static uint8_t fx_ready;

typedef struct { uint8_t outline, shade, fill, light, glyph; } BubbleCols;

static void glyph_stamp(uint8_t *pix, int w, int x0, int y0, int ch, uint8_t col, int rows)
{
    int cx, cy;
    if (ch < FONT_FIRST || ch > FONT_LAST) return;
    for (cx = 0; cx < 5; cx++) {
        uint8_t bits = doomo_font[ch - FONT_FIRST][cx];
        for (cy = 0; cy < rows; cy++)
            if (bits & (1 << cy)) pix[(y0 + cy) * w + x0 + cx] = col;
    }
}

/* Round bubble of radius r centred at (c, c) with a tail, shaded. */
static void bubble(uint8_t *pix, int w, int h, int r, const BubbleCols *c, int tail)
{
    int x, y, cc = r;
    memset(pix, 0, (size_t)(w * h));
    for (y = 0; y <= 2 * r; y++)
        for (x = 0; x <= 2 * r; x++) {
            int dx = x - cc, dy = y - cc, d = dx * dx + dy * dy;
            uint8_t col;
            if (d > r * r + r / 2) continue;
            if (d > (r - 1) * (r - 1) + r / 2) col = c->outline;
            else if (dx + dy > r - 1) col = c->shade;
            else if (dx + dy < -r + 1 && d > (r - 3) * (r - 3)) col = c->light;
            else col = c->fill;
            pix[y * w + x] = col;
        }
    if (tail) {
        int tx = cc;
        if (2 * r + 1 < h) { pix[(2 * r) * w + tx] = c->fill; pix[(2 * r + 1) * w + tx] = c->outline; }
        if (2 * r < h) { pix[(2 * r) * w + tx - 1] = c->outline; pix[(2 * r) * w + tx + 1] = c->outline; }
        if (2 * r + 2 < h) pix[(2 * r + 2) * w + tx] = c->outline;
    }
    /* glossy two-dot highlight */
    pix[(cc - r + 2) * w + cc - r + 3] = PAL_WHITE;
    pix[(cc - r + 3) * w + cc - r + 2] = PAL_WHITE;
}

static void make_images(void)
{
    static const BubbleCols mcol[MK_COUNT] = {
        { PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_CANDY, 4), PAL_RAMP(RAMP_CANDY, 6), PAL_RAMP(RAMP_CANDY, 7), PAL_RAMP(RAMP_WOOD, 1) },
        { PAL_RAMP(RAMP_PASTEL_PINK, 1), PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE },
        { PAL_RAMP(RAMP_MINT, 1), PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 7), PAL_WHITE }
    };
    static const int mglyph[MK_COUNT] = { '!', '?', ICON_CHECK };
    static const int eglyph[8] = { 0, '.', ICON_HEART, ICON_NOTE, ICON_ZZZ, ICON_DROP, ICON_SKULL, 0 };
    static const uint8_t ecol[8] = {
        0, PAL_GRAY_D, PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_COBALT, 1), PAL_PAINT(PAINT_SKY, 0),
        PAL_PAINT(PAINT_CYAN, 0), PAL_PAINT(PAINT_VAPE_PURPLE, 0), 0
    };
    BubbleCols eb = { PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 6), PAL_WHITE, PAL_WHITE, 0 };
    int i;
    for (i = 0; i < MK_COUNT; i++) {
        bubble(mk_pix[i], MK_W, MK_H, 6, &mcol[i], 1);
        glyph_stamp(mk_pix[i], MK_W, 4, 3, mglyph[i], mcol[i].glyph, 7);
        if (mglyph[i] == '!') glyph_stamp(mk_pix[i], MK_W, 5, 3, '!', mcol[i].glyph, 7);   /* bold */
        mk_img[i].w = MK_W;
        mk_img[i].h = MK_H;
        mk_img[i].pix = mk_pix[i];
        mk_img[i].uid = doomo_image_new_uid();
    }
    for (i = 1; i < 7; i++) {
        bubble(em_pix[i], EM_W, EM_H, 5, &eb, 1);
        if (i == EMO_TALK) {
            int k;
            for (k = 0; k < 3; k++) em_pix[i][5 * EM_W + 3 + k * 2] = ecol[i];
        } else {
            glyph_stamp(em_pix[i], EM_W, 3, 2, eglyph[i], ecol[i], 7);
        }
        em_img[i].w = EM_W;
        em_img[i].h = EM_H;
        em_img[i].pix = em_pix[i];
        em_img[i].uid = doomo_image_new_uid();
    }
}

const DoomoImage *owfx_marker(int flags, int t)
{
    (void)t;
    if (!fx_ready) { make_images(); fx_ready = 1; }
    if (flags & NPCF_MAIN) return &mk_img[MK_MAIN];
    if (flags & NPCF_READY) return &mk_img[MK_READY];
    return &mk_img[MK_SIDE];
}

const DoomoImage *owfx_emote(int e)
{
    if (!fx_ready) { make_images(); fx_ready = 1; }
    if (e <= EMO_NONE || e > EMO_SKULL) return NULL;
    return &em_img[e];
}

/* ---- particles ------------------------------------------------------------------------ */
enum { PT_PETAL = 1, PT_FIREFLY, PT_WISP, PT_EMBER, PT_ASH, PT_BAT, PT_CONFETTI };
typedef struct { int16_t x, y; int8_t vx, vy; uint8_t kind, life, col, ph; } Part;
#define NPART 40
static Part parts[NPART];
static DoomoRng frng;
static int32_t last_camx, last_camy;
typedef struct { int16_t x, y; int16_t t; uint8_t col; } Firework;
static Firework fw[4];

void owfx_init(void)
{
    if (!fx_ready) { make_images(); fx_ready = 1; }
    memset(parts, 0, sizeof(parts));
    memset(fw, 0, sizeof(fw));
    rng_seed(&frng, 0xF00Du);
    last_camx = ow.camx;
    last_camy = ow.camy;
}

int owfx_daylight(void)
{
    int h = doomo_town.minute / 60, d;
    static const uint8_t by_hour[24] = { 4, 4, 4, 4, 4, 6, 9, 12, 15, 15, 15, 15, 15, 15, 15, 15, 15, 14, 12, 10, 8, 6, 5, 4 };
    d = by_hour[h % 24];
    if (ow.map == MAPID_HAHALAND_INVADED) d = 11;
    if (ow.map == MAPID_HELLVILE && d > 11) d = 11;
    return d;
}

static void spawn(int kind)
{
    int i, sw = plat_screen_w(), sh = plat_screen_h();
    for (i = 0; i < NPART; i++) if (!parts[i].kind) break;
    if (i == NPART) return;
    parts[i].kind = (uint8_t)kind;
    parts[i].ph = (uint8_t)rng_range(&frng, 256);
    parts[i].life = (uint8_t)rng_between(&frng, 120, 250);
    switch (kind) {
    case PT_PETAL:
        parts[i].x = (int16_t)(rng_range(&frng, sw + 40) * 8 - 40 * 8);
        parts[i].y = -8 * 4;
        parts[i].vx = (int8_t)rng_between(&frng, 3, 6);
        parts[i].vy = (int8_t)rng_between(&frng, 3, 5);
        parts[i].col = (uint8_t)(rng_chance(&frng, 60) ? PAL_RAMP(RAMP_PASTEL_PINK, 6) : PAL_WHITE);
        break;
    case PT_FIREFLY:
        parts[i].x = (int16_t)(rng_range(&frng, sw) * 8);
        parts[i].y = (int16_t)(rng_range(&frng, sh) * 8);
        parts[i].vx = (int8_t)rng_between(&frng, -2, 2);
        parts[i].vy = (int8_t)rng_between(&frng, -2, 1);
        parts[i].col = PAL_RAMP(RAMP_CANDY, 7);
        break;
    case PT_WISP:
        parts[i].x = (int16_t)(rng_range(&frng, sw) * 8);
        parts[i].y = (int16_t)(sh * 8 + 8);
        parts[i].vx = (int8_t)rng_between(&frng, -2, 2);
        parts[i].vy = (int8_t)rng_between(&frng, -6, -3);
        parts[i].col = PAL_RAMP(RAMP_VAPE, (int)rng_between(&frng, 4, 7));
        break;
    case PT_EMBER:
        parts[i].x = (int16_t)(rng_range(&frng, sw) * 8);
        parts[i].y = (int16_t)(sh * 8 + 8);
        parts[i].vx = (int8_t)rng_between(&frng, -3, 3);
        parts[i].vy = (int8_t)rng_between(&frng, -10, -5);
        parts[i].col = PAL_RAMP(RAMP_LAVA, (int)rng_between(&frng, 4, 7));
        break;
    case PT_ASH:
        parts[i].x = (int16_t)(rng_range(&frng, sw) * 8);
        parts[i].y = -8;
        parts[i].vx = (int8_t)rng_between(&frng, -2, 2);
        parts[i].vy = (int8_t)rng_between(&frng, 2, 5);
        parts[i].col = PAL_RAMP(RAMP_STONE, (int)rng_between(&frng, 2, 5));
        break;
    case PT_BAT:
        parts[i].x = -8 * 8;
        parts[i].y = (int16_t)(rng_range(&frng, sh / 2) * 8 + 8 * 8);
        parts[i].vx = (int8_t)rng_between(&frng, 8, 14);
        parts[i].vy = (int8_t)rng_between(&frng, -2, 2);
        parts[i].col = PAL_RAMP(RAMP_VAPE, 0);
        break;
    case PT_CONFETTI:
        parts[i].x = (int16_t)(rng_range(&frng, sw) * 8);
        parts[i].y = -8;
        parts[i].vx = (int8_t)rng_between(&frng, -4, 4);
        parts[i].vy = (int8_t)rng_between(&frng, 4, 8);
        parts[i].col = PAL_PAINT(rng_range(&frng, 32), 2);
        break;
    default: break;
    }
}

static void fireworks_update(void)
{
    int i, sw = plat_screen_w(), sh = plat_screen_h();
    for (i = 0; i < 4; i++) {
        if (fw[i].t) {
            fw[i].t++;
            if (fw[i].t == 14) audio_sfx(SFX_CONFETTI);
            if (fw[i].t > 56) fw[i].t = 0;
        } else if (rng_chance(&frng, 2)) {
            fw[i].x = (int16_t)rng_between(&frng, 24, sw - 24);
            fw[i].y = (int16_t)rng_between(&frng, 18, sh / 3);
            fw[i].col = (uint8_t)rng_range(&frng, 32);
            fw[i].t = 1;
        }
    }
}

void owfx_update(void)
{
    int i, b = story_beat(), cx = ow.px / OW_CELL;
    int night = owfx_daylight() < 9, sw = plat_screen_w(), sh = plat_screen_h();
    int32_t dcx = (ow.camx - last_camx) * 2 / OWQ, dcy = (ow.camy - last_camy) * 2 / OWQ;   /* 1/8 px */
    last_camx = ow.camx;
    last_camy = ow.camy;
    if (dcx > 400 || dcx < -400 || dcy > 400 || dcy < -400) dcx = dcy = 0;
    /* spawn by mood */
    if (ow.map == MAPID_HAHALAND) {
        if (night) { if ((ow.t & 7) == 0) spawn(PT_FIREFLY); }
        else if ((ow.t & 7) == 0) spawn(PT_PETAL);
        if (!ow.visit && b < BEAT_C3_ERUPTION && cx >= 44 && (ow.t & 3) == 0) spawn(PT_WISP);
        if (!ow.visit && b >= BEAT_C2_VAPOR && b < BEAT_C3_ERUPTION && (ow.t & 31) == 0) spawn(PT_WISP);
    } else if (ow.map == MAPID_HAHALAND_INVADED) {
        if ((ow.t & 1) == 0) spawn(PT_EMBER);
        if ((ow.t & 3) == 0) spawn(PT_ASH);
    } else if (ow.map == MAPID_HELLVILE) {
        if ((ow.t % 47) == 0) spawn(PT_BAT);
        if ((ow.t & 7) == 0) spawn(PT_WISP);
        if (night && (ow.t & 15) == 0) spawn(PT_FIREFLY);
    } else if (ow.map == MAPID_PARK_UNITED) {
        if ((ow.t & 3) == 0) spawn(PT_CONFETTI);
        if (night || b == BEAT_C4_STAGE || (ow.t & 1)) fireworks_update();
    }
    for (i = 0; i < NPART; i++) {
        Part *p = &parts[i];
        if (!p->kind) continue;
        p->x = (int16_t)(p->x + p->vx - dcx);
        p->y = (int16_t)(p->y + p->vy - dcy);
        if (p->kind == PT_PETAL || p->kind == PT_CONFETTI) p->x = (int16_t)(p->x + ui_wave((int)ow.t + p->ph, 40, 3));
        if (p->kind == PT_FIREFLY) { p->vx = (int8_t)ui_wave((int)ow.t + p->ph * 4, 80, 3); p->vy = (int8_t)ui_wave((int)ow.t + p->ph * 7, 110, 2); }
        if (--p->life == 0 || p->x < -64 * 8 || p->x > (sw + 64) * 8 || p->y < -32 * 8 || p->y > (sh + 32) * 8)
            p->kind = 0;
    }
}

void owfx_draw_world(void)
{
    int i, k;
    uint32_t t = doomo_frame_count();
    for (i = 0; i < NPART; i++) {
        const Part *p = &parts[i];
        int x = p->x / 8, y = p->y / 8;
        if (!p->kind) continue;
        switch (p->kind) {
        case PT_PETAL:
            plat_rect(x, y, 2, 1, p->col);
            plat_rect(x + (((t + p->ph) >> 3) & 1), y + 1, 1, 1, ui_darker(p->col));
            break;
        case PT_FIREFLY:
            if (((t + p->ph) & 31) < 20) {
                plat_rect(x, y, 1, 1, PAL_WHITE);
                plat_rect(x - 1, y, 1, 1, p->col);
                plat_rect(x + 1, y, 1, 1, p->col);
                plat_rect(x, y - 1, 1, 1, p->col);
                plat_rect(x, y + 1, 1, 1, p->col);
            }
            break;
        case PT_WISP:
            plat_checker(x, y, 3, 3, p->col);
            break;
        case PT_EMBER:
            plat_rect(x, y, 1, ((t + p->ph) & 4) ? 2 : 1, p->col);
            break;
        case PT_ASH:
            plat_rect(x, y, 1, 1, p->col);
            break;
        case PT_BAT: {
            int f = ((t + p->ph) >> 2) & 1;
            plat_rect(x - 2, y - f, 2, 1, p->col);
            plat_rect(x + 1, y - f, 2, 1, p->col);
            plat_rect(x, y, 1, 1, p->col);
            break; }
        case PT_CONFETTI:
            plat_rect(x, y, ((t + p->ph) & 8) ? 2 : 1, 2, p->col);
            break;
        default: break;
        }
    }
    for (i = 0; i < 4; i++) {
        const Firework *f = &fw[i];
        if (!f->t) continue;
        if (f->t < 14) {
            int y = plat_screen_h() - (plat_screen_h() - f->y) * f->t / 14;
            plat_rect(f->x, y, 1, 3, PAL_RAMP(RAMP_CANDY, 7));
        } else {
            int r = (f->t - 14) * 3 / 2, n = 14;
            uint8_t c = PAL_PAINT(f->col, f->t > 44 ? 0 : f->t > 30 ? 1 : 2);
            for (k = 0; k < n; k++) {
                int a = k * DOOMO_ANG_FULL / n + f->t * 8;
                int sx = f->x + FIX2INT(fx_cos(a) * r), sy = f->y + FIX2INT(fx_sin(a) * r) + (f->t - 14) / 6;
                plat_rect(sx, sy, 2, 2, c);
                if (f->t < 30) plat_rect(sx - (FIX2INT(fx_cos(a) * 3)), sy - FIX2INT(fx_sin(a) * 3), 1, 1, PAL_WHITE);
            }
        }
    }
}

/* ---- HUD -------------------------------------------------------------------------------- */
static int hud_style(void)
{
    if (ow.map == MAPID_HAHALAND_INVADED) return WIN_HELL;
    if (ow.map == MAPID_HELLVILE) return WIN_DARK;
    return WIN_CUTE;
}

void owfx_world_to_screen(int32_t x, int32_t y, int *sx, int *sy)
{
    *sx = (int)((x - ow.camx) / (4 * OWQ)) + plat_screen_w() / 2;
    *sy = (int)((y - ow.camy) / (4 * OWQ)) + plat_screen_h() / 2;
}

static void fit(char *dst, const char *src, int cap, int maxw)
{
    int n;
    doomo_snprintf(dst, cap, "%s", src);
    n = (int)strlen(dst);
    while (n > 3 && ui_text_width(dst) > maxw) {
        n--;
        dst[n] = 0;
        dst[n - 1] = '.';
        dst[n - 2] = '.';
    }
}

static void pill(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    ui_round_rect(x, y + 1, w, h, 4, st->outline);
    ui_round_rect(x, y, w, h, 4, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, st->fill);
    plat_rect(x + 3, y + 2, w - 6, 1, st->gloss);
}

static void draw_objective(int style)
{
    const char *o = ow.visit ? "Free stroll - enjoy the town!" : story_objective();
    char buf[80];
    int sw = plat_screen_w(), maxw = sw - 88, w, x = 3, y = 3;
    const UiStyle *st = ui_style(style);
    uint32_t t = doomo_frame_count();
    if (!o || !o[0]) return;
    fit(buf, o, sizeof(buf), maxw);
    w = ui_text_width(buf) + 20;
    pill(x, y, w, 13, style);
    ui_star(x + 8, y + 6 + ui_wave((int)t, 40, 1), 4, ow.visit ? PAL_UI_MINT : PAL_UI_GOLD);
    ui_text(x + 15, y + 3, buf, st->text);
}

static void draw_clock(int style)
{
    char tbuf[16], mbuf[16];
    int sw = plat_screen_w(), w, x, y = 3;
    const UiStyle *st = ui_style(style);
    int night = town_is_night();
    town_time_str(tbuf, sizeof(tbuf));
    doomo_snprintf(mbuf, sizeof(mbuf), "%u", (unsigned)doomo_town.money);
    w = DOOMO_MAX(ui_text_width(tbuf), ui_text_width(mbuf) + 8) + 16;
    x = sw - w - 3;
    pill(x, y, w, 22, style);
    if (night) {
        ui_fill_circle(x + 7, y + 6, 3, PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_circle(x + 8, y + 5, 2, st->fill);
    } else {
        ui_fill_circle(x + 7, y + 6, 3, PAL_RAMP(RAMP_CANDY, 6));
        plat_rect(x + 6, y + 4, 1, 1, PAL_WHITE);
    }
    ui_text(x + 13, y + 3, tbuf, st->text);
    plat_text(x + 4, y + 12, STR_ICON_COIN, PAL_UI_GOLD);
    ui_text(x + 13, y + 12, mbuf, st->text);
}

static void draw_prompt(int style)
{
    char buf[48];
    const char *what = NULL;
    int sw = plat_screen_w(), sh = plat_screen_h(), w, x, y;
    const UiStyle *st = ui_style(style);
    if (story_talking() || ow.pause) return;
    buf[0] = 0;
    if (ow.near_npc >= 0) {
        const DoomoResident *r = town_res(ow_npcs[ow.near_npc].id);
        if (r) { doomo_snprintf(buf, sizeof(buf), STR_ICON_A " Talk to %s", r->name); what = buf; }
    } else if (ow.near_spot >= 0) {
        what = STR_ICON_A " Look closer";
    } else if (ow.near_door) {
        doomo_snprintf(buf, sizeof(buf), STR_ICON_A " Enter %s", story_place_name(ow.near_door));
        what = buf;
    } else if (ow.near_prop >= 0) {
        switch (ow_props[ow.near_prop].type) {
        case THING_PROP_SIGN: what = STR_ICON_A " Read"; break;
        case THING_PROP_FLOWERS: what = STR_ICON_A " Pick flowers"; break;
        case THING_PROP_MAILBOX: what = STR_ICON_A " Peek inside"; break;
        case THING_PROP_FOUNTAIN: what = STR_ICON_A " Make a wish"; break;
        default: break;
        }
    }
    if (!what) return;
    w = ui_text_width(what) + 14;
    x = (sw - w) / 2;
    y = sh - 17 + ui_wave((int)doomo_frame_count(), 50, 1);
    pill(x, y, w, 13, style);
    ui_text(x + 7, y + 3, what, st->text);
}

static void draw_banner(int style)
{
    int sw = plat_screen_w(), y, age;
    if (!ow.banner || !ow.banner_t) return;
    age = 100 - ow.banner_t;
    y = 22;
    if (age < 10) y = 22 - (10 - age) * 3;
    if (ow.banner_t < 10) y = 22 - (10 - ow.banner_t) * 3;
    ui_title(sw / 2, y, ow.banner, style);
}

static void draw_compass(void)
{
    int map, tx, ty, sx, sy, sw = plat_screen_w(), sh = plat_screen_h();
    int32_t wx, wy, dx, dy;
    int id;
    uint32_t t = doomo_frame_count();
    if (ow.visit || story_talking()) return;
    id = story_target(&map, &tx, &ty);
    if (map < 0) return;
    if (map != ow.map && !(map == MAPID_HAHALAND && ow.map == MAPID_HAHALAND_INVADED) &&
        !(map == MAPID_HAHALAND_INVADED && ow.map == MAPID_HAHALAND)) return;
    if (id >= 0) {
        int k = ownpc_find(id);
        if (k >= 0) { wx = ow_npcs[k].x; wy = ow_npcs[k].y; }
        else { wx = (int32_t)tx * OW_CELL + OW_CELL / 2; wy = (int32_t)ty * OW_CELL + OW_CELL / 2; }
    } else {
        wx = (int32_t)tx * OW_CELL + OW_CELL / 2;
        wy = (int32_t)ty * OW_CELL + OW_CELL / 2;
    }
    owfx_world_to_screen(wx, wy, &sx, &sy);
    if (sx > 12 && sx < sw - 12 && sy > 30 && sy < sh - 22) return;     /* on screen */
    dx = sx - sw / 2;
    dy = sy - sh / 2;
    {
        int a = fx_atan2(dy, dx), r, ex, ey, k;
        int hw = sw / 2 - 12, hh = sh / 2 - 22;
        fix16 c = fx_cos(a), s = fx_sin(a);
        /* walk out along the ray until we hit the inset border */
        for (r = 8; r < 400; r += 2) {
            ex = FIX2INT(c * r);
            ey = FIX2INT(s * r);
            if (ex > hw || ex < -hw || ey > hh || ey < -hh) break;
        }
        ex += sw / 2;
        ey += sh / 2 + 4;
        r = 6 + ui_wave((int)t, 30, 1);
        ui_fill_circle(ex, ey, 7, PAL_RAMP(RAMP_WOOD, 2));
        ui_fill_circle(ex, ey, 6, PAL_RAMP(RAMP_CANDY, 6));
        ui_star(ex, ey, 4, PAL_UI_GOLD);
        for (k = 0; k < 3; k++) {
            int px = ex + FIX2INT(c * (r + 3 + k)), py = ey + FIX2INT(s * (r + 3 + k));
            int pw = 3 - k;
            plat_rect(px - pw / 2, py - pw / 2, pw > 0 ? pw : 1, pw > 0 ? pw : 1, PAL_RAMP(RAMP_WOOD, 2));
        }
    }
}

void owfx_draw_hud(void)
{
    int style = hud_style();
    draw_objective(style);
    draw_clock(style);
    draw_banner(style);
    draw_compass();
    draw_prompt(style);
    if (ow.minimap) {
        int s = plat_screen_h() >= 200 ? 2 : 1;
        int mw = doomo_map.w * s, mh = doomo_map.h * s;
        owfx_minimap(plat_screen_w() - mw - 6, plat_screen_h() - mh - 22, s, ow.map);
    }
}

/* ---- the mini-map ------------------------------------------------------------------------ */
static uint8_t mm_pix[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
static DoomoImage mm_img;
static int8_t mm_map = -1;

static void mm_build(void)
{
    int x, y, w = doomo_map.w, h = doomo_map.h;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const DoomoCell *c = map_cell(&doomo_map, x, y);
            uint8_t col;
            if (c->tag >= STORY_TAG_DOOR && c->tag < STORY_TAG_DOOR + PLACE_COUNT) col = PAL_UI_GOLD;
            else if (c->wall) col = palette_darker(texture_avg(c->floor_tex ? c->floor_tex : c->wall));
            else {
                col = texture_avg(c->floor_tex);
                if (c->floor_h > 0) col = palette_lighter(col);
            }
            mm_pix[y * w + x] = col ? col : PAL_GRAY_D;
        }
    mm_img.w = (uint16_t)w;
    mm_img.h = (uint16_t)h;
    mm_img.pix = mm_pix;
    mm_img.uid = doomo_image_new_uid();
    mm_map = (int8_t)map_current_id();
}

void owfx_minimap(int x, int y, int s, int map_id)
{
    int pcx, pcy, tx, ty, tmap, k;
    uint32_t t = doomo_frame_count();
    const UiStyle *st = ui_style(hud_style());
    (void)map_id;
    if (mm_map != map_current_id() || mm_img.w != doomo_map.w) mm_build();
    ui_round_rect(x - 3, y - 3, doomo_map.w * s + 6, doomo_map.h * s + 6, 3, st->outline);
    plat_image(x, y, &mm_img, s, 0, 0);
    /* citizens */
    for (k = 0; k < ow_npc_count; k++) {
        int nx = ow_npcs[k].x / OW_CELL, ny = ow_npcs[k].y / OW_CELL;
        uint8_t c = ow_npcs[k].kind == NK_DEMON ? PAL_UI_RED :
                    (ow_npcs[k].flags & NPCF_MAIN) ? PAL_UI_GOLD :
                    (ow_npcs[k].flags & (NPCF_SIDE | NPCF_READY)) ? PAL_UI_PINK : PAL_WHITE;
        plat_rect(x + nx * s, y + ny * s, s, s, c);
    }
    if (!ow.visit && story_target(&tmap, &tx, &ty) >= -1 && tmap == map_current_id() && (t & 16))
        ui_star(x + tx * s + s / 2, y + ty * s + s / 2, 3 + s, PAL_UI_GOLD);
    pcx = ow.px / OW_CELL;
    pcy = ow.py / OW_CELL;
    if (t & 8) {
        plat_rect(x + pcx * s - 1, y + pcy * s - 1, s + 2, s + 2, PAL_BLACK);
        plat_rect(x + pcx * s, y + pcy * s, s, s, PAL_UI_MINT);
    }
}

/* ---- pause panel ------------------------------------------------------------------------ */
extern const char *const ow_pause_names[];
int ow_pause_count(void);

void owfx_draw_pause(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), style = hud_style();
    int big = sh >= 200, w = big ? 236 : 196, h = big ? 196 : 138;
    int x = (sw - w) / 2, y = (sh - h) / 2 + 4, i, bh = big ? 16 : 13, by;
    const DoomoResident *p = town_res(DOOMO_PLAYER_ID);
    const UiStyle *st = ui_style(style);
    char buf[48];
    uint32_t t = doomo_frame_count();
    plat_shade_rect(0, 0, sw, sh, 8);
    if (!ui_window_pop(x, y, w, h, style, ow.pause_t)) return;
    ui_title(sw / 2, y - 6, "Paused", style);
    /* you, waving */
    if (p) {
        const DoomoImage *fig = figure_image(&p->look, p->status, ((t / 40) & 3) == 1 ? EXPR_HAPPY : EXPR_NEUTRAL);
        int fy = y + 12 + ui_wave((int)t, 60, 1);
        ui_fill_ellipse(x + 34, y + 12 + FIGURE_H - 1, 18, 4, st->fill_lo);
        if (fig) plat_image(x + 10, fy, fig, 1, 0, 0);
        ui_text_center(x + 34, y + FIGURE_H + 16, p->name, st->title);
    }
    by = y + h - (big ? 40 : 30);
    doomo_snprintf(buf, sizeof(buf), "Ch.%d", story_chapter() == STORY_CH_DONE ? 4 : story_chapter());
    if (!ow.visit) ui_text(x + 8, by, buf, st->text_dim);
    doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN "%u", (unsigned)doomo_town.money);
    ui_text(x + 8, by + 9, buf, st->text);
    doomo_snprintf(buf, sizeof(buf), "Day %u", (unsigned)doomo_town.day + 1);
    ui_text(x + 8, by + 18, buf, st->text_dim);
    for (i = 0; i < ow_pause_count(); i++) {
        int bx = x + 72, bw = w - 80;
        ui_button(bx, y + 8 + i * bh, bw, ow_pause_names[i], i == ow.pause_sel, style);
    }
    if (!ow.visit) {
        doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART "%d friends  " STR_ICON_STAR "%d/%d quests",
                       story_friend_count(-1), story_side_done_count(), story_side_count());
        if (big) ui_text_center(sw / 2, y + h - 11, buf, st->text_dim);
    }
}
