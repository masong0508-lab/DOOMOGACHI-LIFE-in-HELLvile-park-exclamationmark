/* portrait_sheet.c - native contact-sheet tool for the resident renderer
 * (owner: portrait). Host-only (uses stdio/malloc); links the whole core
 * plus the native platform (for the PNG writer).
 *
 *   portrait_sheet --out DIR [--zoom N] [--only NAME] [--stats]
 *                  [--bust LIST [--fig]]
 *
 * Writes PNG sheets (see docs/PORTRAIT.md):
 *   opt_<list>.png     every option of one catalog list on a fixed citizen
 *                      (bust portrait above the full figure)
 *   sliders.png        every slider at 0, 3, 7, 11, 15 plus eye count/fangs
 *   random_good.png / random_hell.png / *_fig.png
 *                      64 resident_generate() / _hellborn() citizens
 *   expressions.png, statuses.png, anim.png
 *   walk.png           chibi walk cycles, 4 directions x 4 frames
 *   fighter.png, weapons.png   third-person fighter views / anims / guns
 * --stats prints the average and worst pixel-visit cost per render.
 * --bust LIST writes bust_<LIST>.png: big close-ups of every option of one
 * list (--fig: full figures instead, fig_<LIST>.png).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/weld.h"
#include "native.h"

static const char *outdir = ".";
static const char *only = NULL;
static int zoom = 2;

/* ---- sheet canvas ---------------------------------------------------------- */
typedef struct Sheet { uint8_t *p; int w, h; } Sheet;

static void sh_new(Sheet *s, int w, int h, uint8_t bg)
{
    s->w = w; s->h = h;
    s->p = (uint8_t *)malloc((size_t)w * h);
    memset(s->p, bg, (size_t)w * h);
}

static void sh_rect(Sheet *s, int x, int y, int w, int h, uint8_t c)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < s->w && j < s->h) s->p[j * s->w + i] = c;
}

static void sh_blit(Sheet *s, int x, int y, const uint8_t *pix, int w, int h)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint8_t c = pix[j * w + i];
            if (c && x + i >= 0 && y + j >= 0 && x + i < s->w && y + j < s->h)
                s->p[(y + j) * s->w + x + i] = c;
        }
}

static void sh_text(Sheet *s, int x, int y, const char *t, uint8_t c)
{
    for (; *t; t++, x += FONT_ADVANCE) {
        int ch = (unsigned char)*t, col, row;
        if (ch < FONT_FIRST || ch > FONT_LAST) continue;
        for (col = 0; col < 5; col++) {
            uint8_t bits = doomo_font[ch - FONT_FIRST][col];
            for (row = 0; row < 8; row++)
                if (bits & (1 << row)) sh_rect(s, x + col, y + row, 1, 1, c);
        }
    }
}

static int want(const char *name)
{
    return !only || strstr(name, only) != NULL;
}

static void sh_save(Sheet *s, const char *name, int z)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", outdir, name);
    if (!nat_png_write(path, s->p, s->w, s->h, z)) fprintf(stderr, "portrait_sheet: cannot write %s\n", path);
    else printf("portrait_sheet: %s (%dx%d)\n", path, s->w * z, s->h * z);
    free(s->p);
    s->p = NULL;
}

/* checker backdrop cells so transparent edges are visible */
static void cell_bg(Sheet *s, int x, int y, int w, int h, int k)
{
    sh_rect(s, x, y, w, h, (k & 1) ? PAL_RAMP(RAMP_MINT, 6) : PAL_RAMP(RAMP_PASTEL_PINK, 7));
}

/* ---- the fixed showcase citizen ------------------------------------------------ */
static DoomoLook base_look(void)
{
    DoomoResident r;
    resident_default(&r);
    return r.look;
}

/* ---- option lists ------------------------------------------------------------- */
typedef struct OptList {
    const char *name;
    int off;            /* offsetof field in DoomoLook, -1 for special */
    int count;
    const char *const *names;
} OptList;

#define OL(n, f, c, nm) { n, (int)offsetof(DoomoLook, f), c, nm }
static const OptList lists[] = {
    OL("species", species, SPECIES_COUNT, doomo_species_names),
    OL("body", body, BODY_COUNT, doomo_body_names),
    OL("head", head_shape, HEAD_COUNT, doomo_head_names),
    OL("eyes", eye_style, EYE_COUNT, doomo_eye_names),
    OL("brows", brow_style, BROW_COUNT, doomo_brow_names),
    OL("nose", nose_style, NOSE_COUNT, doomo_nose_names),
    OL("mouth", mouth_style, MOUTH_COUNT, doomo_mouth_names),
    OL("ears", ears, EARS_COUNT, doomo_ear_names),
    OL("cheeks", cheeks, 9, NULL),
    OL("hair", hair_style, HAIR_COUNT, doomo_hair_names),
    OL("fhair", facial_hair, FHAIR_COUNT, doomo_fhair_names),
    OL("horns", horns, HORNS_COUNT, doomo_horn_names),
    OL("tail", tail, TAIL_COUNT, doomo_tail_names),
    OL("wings", wings, WINGS_COUNT, doomo_wing_names),
    OL("halo", halo, HALO_COUNT, doomo_halo_names),
    OL("marking", marking, MARK_COUNT, doomo_mark_names),
    OL("top", top, TOP_COUNT, doomo_top_names),
    OL("bottom", bottom, BOTTOM_COUNT, doomo_bottom_names),
    OL("shoes", shoes, SHOES_COUNT, doomo_shoes_names),
    OL("hat", hat, HAT_COUNT, doomo_hat_names),
    OL("accessory", accessory, ACC_COUNT, doomo_acc_names),
    OL("aura", aura, AURA_COUNT, doomo_aura_names),
    OL("pattern", pattern, PATTERN_COUNT, doomo_pattern_names),
};

static void set_opt(DoomoLook *l, const OptList *o, int v)
{
    uint8_t *p = (uint8_t *)l + o->off;
    if (o->off == (int)offsetof(DoomoLook, cheeks)) *p = (uint8_t)(v ? 1 << (v - 1) : 0);
    else *p = (uint8_t)v;
}

static void opt_name(const OptList *o, int v, char *buf, int cap)
{
    if (o->names) snprintf(buf, (size_t)cap, "%s", o->names[v]);
    else if (v == 0) snprintf(buf, (size_t)cap, "none");
    else snprintf(buf, (size_t)cap, "%s", doomo_cheek_names[v - 1]);
}

static void short_name(char *dst, const char *src, int max)
{
    int i;
    for (i = 0; i < max && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static uint32_t total_ops, worst_ops, n_renders;
static void count_ops(void)
{
    uint32_t o = portrait_last_ops();
    total_ops += o;
    if (o > worst_ops) worst_ops = o;
    n_renders++;
}

static void sheet_options(void)
{
    int li;
    uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    for (li = 0; li < (int)(sizeof(lists) / sizeof(lists[0])); li++) {
        const OptList *o = &lists[li];
        char fname[64];
        Sheet s;
        int per = 16, rows = (o->count + per - 1) / per, cw = 50, chh = 56 + 80 + 14, v;
        snprintf(fname, sizeof(fname), "opt_%s.png", o->name);
        if (!want(fname)) continue;
        sh_new(&s, 4 + per * cw, 14 + rows * chh, PAL_UI_CREAM);
        sh_text(&s, 2, 2, o->name, PAL_UI_PINK_D);
        for (v = 0; v < o->count; v++) {
            DoomoLook l = base_look();
            int x = 2 + (v % per) * cw, y = 14 + (v / per) * chh;
            char nm[32], sn[9];
            set_opt(&l, o, v);
            /* make the option readable on the showcase citizen */
            if (o->off == (int)offsetof(DoomoLook, pattern)) l.top = TOP_TSHIRT;
            if (o->off == (int)offsetof(DoomoLook, marking)) l.marking_color = PAINT_CHOCOLATE;
            if (o->off == (int)offsetof(DoomoLook, accessory)) l.accessory_color = PAINT_COBALT;
            if (o->off == (int)offsetof(DoomoLook, hat)) l.hat_color = PAINT_LAVENDER;
            if (o->off == (int)offsetof(DoomoLook, facial_hair)) l.hair_color = PAINT_CHOCOLATE;
            portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, por);
            count_ops();
            figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, fig);
            count_ops();
            cell_bg(&s, x, y, 48, 56, v);
            sh_blit(&s, x, y, por, PORTRAIT_W, PORTRAIT_H);
            cell_bg(&s, x, y + 56, 48, 80, v + 1);
            sh_blit(&s, x, y + 56, fig, FIGURE_W, FIGURE_H);
            opt_name(o, v, nm, sizeof(nm));
            short_name(sn, nm, 8);
            sh_text(&s, x, y + 137, sn, PAL_GRAY_D);
        }
        sh_save(&s, fname, zoom);
    }
}

/* --bust LIST [--fig]: just the portraits (or figures) of one list, 8 per row */
static void sheet_bust(const char *name, int figure)
{
    int li, v;
    uint8_t por[FIGURE_W * FIGURE_H];
    for (li = 0; li < (int)(sizeof(lists) / sizeof(lists[0])); li++) {
        const OptList *o = &lists[li];
        Sheet s;
        char fname[64];
        int per = 8, cw = 50, chh = (figure ? 80 : 56) + 10, w = figure ? FIGURE_W : PORTRAIT_W;
        int hh = figure ? FIGURE_H : PORTRAIT_H;
        if (strcmp(o->name, name) != 0) continue;
        sh_new(&s, 2 + per * cw, 2 + ((o->count + per - 1) / per) * chh, PAL_UI_CREAM);
        for (v = 0; v < o->count; v++) {
            DoomoLook l = base_look();
            int x = 2 + (v % per) * cw, y = 2 + (v / per) * chh;
            char nm[32], sn[9];
            set_opt(&l, o, v);
            if (o->off == (int)offsetof(DoomoLook, pattern)) l.top = TOP_TSHIRT;
            if (o->off == (int)offsetof(DoomoLook, marking)) l.marking_color = PAINT_CHOCOLATE;
            if (o->off == (int)offsetof(DoomoLook, accessory)) l.accessory_color = PAINT_COBALT;
            if (o->off == (int)offsetof(DoomoLook, hat)) l.hat_color = PAINT_LAVENDER;
            if (o->off == (int)offsetof(DoomoLook, facial_hair)) l.hair_color = PAINT_CHOCOLATE;
            if (figure) figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, por);
            else portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, por);
            cell_bg(&s, x, y, w, hh, v);
            sh_blit(&s, x, y, por, w, hh);
            opt_name(o, v, nm, sizeof(nm));
            short_name(sn, nm, 8);
            sh_text(&s, x, y + hh + 1, sn, PAL_GRAY_D);
        }
        snprintf(fname, sizeof(fname), "%s_%s.png", figure ? "fig" : "bust", name);
        sh_save(&s, fname, 4);
    }
}

typedef struct Slider { const char *name; int off, lo, hi; } Slider;
#define SL(n, f, lo, hi) { n, (int)offsetof(DoomoLook, f), lo, hi }
static const Slider sliders[] = {
    SL("height", height, 0, 15), SL("build", build, 0, 15),
    SL("head size", head_size, 0, 15), SL("eye size", eye_size, 0, 15),
    SL("eye spacing", eye_spacing, 0, 15), SL("eye height", eye_height, 0, 15),
    SL("eye tilt", eye_tilt, 0, 15), SL("eye count", eye_count, 1, 3),
    SL("brow height", brow_height, 0, 15), SL("nose size", nose_size, 0, 15),
    SL("mouth size", mouth_size, 0, 15), SL("fangs", fangs, 0, 3),
    SL("horn size", horn_size, 0, 15), SL("hair hilite", hair_highlight, 0, 4),
};

static void sheet_sliders(void)
{
    int si, k, n = (int)(sizeof(sliders) / sizeof(sliders[0]));
    uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    Sheet s;
    if (!want("sliders.png")) return;
    sh_new(&s, 80 + 5 * 50 * 2, 4 + n * 84, PAL_UI_CREAM);
    for (si = 0; si < n; si++) {
        const Slider *sl = &sliders[si];
        int y = 4 + si * 84;
        sh_text(&s, 2, y + 30, sl->name, PAL_UI_PINK_D);
        for (k = 0; k < 5; k++) {
            DoomoLook l = base_look();
            int v = sl->lo + (sl->hi - sl->lo) * k / 4, x = 80 + k * 100;
            if (sl->off == (int)offsetof(DoomoLook, horn_size)) l.horns = HORNS_DEVIL_TALL;
            if (sl->off == (int)offsetof(DoomoLook, fangs)) l.mouth_style = MOUTH_SMILE;
            if (sl->off == (int)offsetof(DoomoLook, brow_height)) l.brow_style = BROW_THICK;
            if (sl->off == (int)offsetof(DoomoLook, hair_highlight)) {
                static const uint8_t hl[5] = { LOOK_NO_COLOR, PAINT_BUBBLEGUM, PAINT_SKY, PAINT_LEMON, PAINT_MINT };
                l.hair_style = HAIR_LONG;
                l.hair_highlight = hl[k];
            } else {
                *((uint8_t *)&l + sl->off) = (uint8_t)v;
            }
            portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, por);
            figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, fig);
            cell_bg(&s, x, y, 48, 80, k);
            sh_blit(&s, x, y + 12, por, PORTRAIT_W, PORTRAIT_H);
            cell_bg(&s, x + 49, y, 48, 80, k + 1);
            sh_blit(&s, x + 49, y, fig, FIGURE_W, FIGURE_H);
        }
    }
    sh_save(&s, "sliders.png", zoom);
}

static void sheet_random(int hell)
{
    int i;
    uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    Sheet s, f;
    const char *n1 = hell ? "random_hell.png" : "random_good.png";
    const char *n2 = hell ? "random_hell_fig.png" : "random_good_fig.png";
    if (!want(n1) && !want(n2)) return;
    sh_new(&s, 8 * 50 + 2, 8 * 58 + 2, PAL_UI_CREAM);
    sh_new(&f, 8 * 50 + 2, 8 * 82 + 2, PAL_UI_CREAM);
    for (i = 0; i < 64; i++) {
        DoomoResident r;
        int x = 2 + (i % 8) * 50, y = 2 + (i / 8) * 58, yf = 2 + (i / 8) * 82;
        if (hell) resident_generate_hellborn(&r, 1000u + (uint32_t)i * 7919u);
        else resident_generate(&r, 1000u + (uint32_t)i * 7919u);
        portrait_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, i * 13, por);
        count_ops();
        figure_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, i * 13, fig);
        count_ops();
        cell_bg(&s, x, y, 48, 56, i + (i / 8));
        sh_blit(&s, x, y, por, PORTRAIT_W, PORTRAIT_H);
        cell_bg(&f, x, yf, 48, 80, i + (i / 8));
        sh_blit(&f, x, yf, fig, FIGURE_W, FIGURE_H);
    }
    if (want(n1)) sh_save(&s, n1, zoom); else free(s.p);
    if (want(n2)) sh_save(&f, n2, zoom); else free(f.p);
}

static void sample_resident(int i, DoomoResident *r)
{
    if (i == 0) { resident_default(r); r->status = RES_ALIVE; return; }
    if (i & 1) resident_generate_hellborn(r, 77u + (uint32_t)i * 31u);
    else resident_generate(r, 77u + (uint32_t)i * 31u);
}

static void sheet_expressions(void)
{
    int i, e;
    uint8_t por[PORTRAIT_W * PORTRAIT_H];
    static const char *const en[EXPR_COUNT] = { "neutral", "happy", "sad", "angry", "love",
        "shock", "sleepy", "sick", "possess", "dead", "talk" };
    Sheet s;
    if (!want("expressions.png")) return;
    sh_new(&s, 2 + EXPR_COUNT * 50, 12 + 6 * 58, PAL_UI_CREAM);
    for (e = 0; e < EXPR_COUNT; e++) sh_text(&s, 2 + e * 50, 2, en[e], PAL_UI_PINK_D);
    for (i = 0; i < 6; i++) {
        DoomoResident r;
        sample_resident(i, &r);
        for (e = 0; e < EXPR_COUNT; e++) {
            int x = 2 + e * 50, y = 12 + i * 58;
            portrait_render(&r.look, RES_ALIVE, e, 4, por);
            cell_bg(&s, x, y, 48, 56, e + i);
            sh_blit(&s, x, y, por, PORTRAIT_W, PORTRAIT_H);
        }
    }
    sh_save(&s, "expressions.png", zoom);
}

static void sheet_statuses(void)
{
    static const int st[5] = { RES_ALIVE, RES_POSSESSED, RES_SOUL_LOST, RES_DEAD, RES_GHOST };
    static const char *const sn[5] = { "alive", "possessed", "soul lost", "dead", "ghost" };
    int i, k;
    uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    Sheet s;
    if (!want("statuses.png")) return;
    sh_new(&s, 2 + 5 * 100, 12 + 4 * 84, PAL_UI_CREAM);
    for (k = 0; k < 5; k++) sh_text(&s, 2 + k * 100, 2, sn[k], PAL_UI_PINK_D);
    for (i = 0; i < 4; i++) {
        DoomoResident r;
        sample_resident(i, &r);
        for (k = 0; k < 5; k++) {
            int x = 2 + k * 100, y = 12 + i * 84;
            portrait_render(&r.look, st[k], EXPR_NEUTRAL, 8, por);
            figure_render(&r.look, st[k], EXPR_NEUTRAL, 8, fig);
            cell_bg(&s, x, y, 48, 82, k + i);
            sh_blit(&s, x, y + 12, por, PORTRAIT_W, PORTRAIT_H);
            cell_bg(&s, x + 49, y, 48, 82, k + i + 1);
            sh_blit(&s, x + 49, y, fig, FIGURE_W, FIGURE_H);
        }
    }
    sh_save(&s, "statuses.png", zoom);
}

static void sheet_anim(void)
{
    int i, k;
    uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    static const int an[8] = { 0, 8, 32, 40, 92, 190, 64, 100 };
    Sheet s;
    if (!want("anim.png")) return;
    sh_new(&s, 2 + 8 * 50, 2 + 3 * 140, PAL_UI_CREAM);
    for (i = 0; i < 3; i++) {
        DoomoResident r;
        sample_resident(i * 3 + 2, &r);
        if (i == 0) r.look.aura = AURA_SPARKLES;
        for (k = 0; k < 8; k++) {
            int x = 2 + k * 50, y = 2 + i * 140;
            portrait_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, an[k], por);
            figure_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, an[k], fig);
            cell_bg(&s, x, y, 48, 56, k);
            sh_blit(&s, x, y, por, PORTRAIT_W, PORTRAIT_H);
            cell_bg(&s, x, y + 57, 48, 80, k + 1);
            sh_blit(&s, x, y + 57, fig, FIGURE_W, FIGURE_H);
        }
    }
    sh_save(&s, "anim.png", zoom);
}

static void sheet_walk(void)
{
    int i, d, f;
    uint8_t spr[BODYSPR_W * BODYSPR_H];
    Sheet s;
    if (!want("walk.png")) return;
    sh_new(&s, 2 + 16 * 18, 2 + 12 * 26, PAL_UI_CREAM);
    for (i = 0; i < 12; i++) {
        DoomoResident r;
        sample_resident(i, &r);
        for (d = 0; d < 4; d++)
            for (f = 0; f < 4; f++) {
                int x = 2 + (d * 4 + f) * 18, y = 2 + i * 26;
                bodyspr_render(&r.look, RES_ALIVE, d, f, spr);
                count_ops();
                cell_bg(&s, x, y, 16, 24, d + i);
                sh_blit(&s, x, y, spr, BODYSPR_W, BODYSPR_H);
            }
    }
    sh_save(&s, "walk.png", zoom * 2);
}

static void sheet_fighter(void)
{
    int i, v, a;
    uint8_t spr[FIGHTER_W * FIGHTER_H];
    static const int wf[4] = { WFRAME_ASSAULT_RIFLE, WFRAME_SUPER_SHOTGUN, WFRAME_BUBBLE_WAND, WFRAME_ROCKET_LAUNCHER };
    Sheet s;
    if (!want("fighter.png")) return;
    sh_new(&s, 2 + 7 * 34, 2 + 4 * 5 * 50, PAL_UI_CREAM);
    for (i = 0; i < 4; i++) {
        DoomoResident r;
        sample_resident(i, &r);
        for (v = 0; v < 5; v++)
            for (a = 0; a < 7; a++) {
                int x = 2 + a * 34, y = 2 + (i * 5 + v) * 50;
                fighter_render(&r.look, RES_ALIVE, v, a, wf[i], spr);
                count_ops();
                cell_bg(&s, x, y, 32, 48, v + a);
                sh_blit(&s, x, y, spr, FIGHTER_W, FIGHTER_H);
            }
    }
    sh_save(&s, "fighter.png", zoom);
}

static void sheet_weapons(void)
{
    int w;
    uint8_t spr[FIGHTER_W * FIGHTER_H];
    Sheet s;
    DoomoLook l = base_look();
    if (!want("weapons.png")) return;
    sh_new(&s, 2 + 10 * 34 * 2, 2 + 4 * 58, PAL_UI_CREAM);
    for (w = 0; w < WFRAME_COUNT; w++) {
        int x = 2 + (w % 10) * 68, y = 2 + (w / 10) * 58;
        char sn[12];
        fighter_render(&l, RES_ALIVE, 2, 4, w, spr);
        cell_bg(&s, x, y, 32, 48, w);
        sh_blit(&s, x, y, spr, FIGHTER_W, FIGHTER_H);
        fighter_render(&l, RES_ALIVE, 3, 0, w, spr);
        cell_bg(&s, x + 33, y, 32, 48, w + 1);
        sh_blit(&s, x + 33, y, spr, FIGHTER_W, FIGHTER_H);
        short_name(sn, doomo_wframe_names[w], 11);
        sh_text(&s, x, y + 49, sn, PAL_GRAY_D);
    }
    sh_save(&s, "weapons.png", zoom);
}

/* a few citizens big, for close inspection */
static void sheet_closeup(void)
{
    int i;
    uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    uint8_t spr[BODYSPR_W * BODYSPR_H], ftr[FIGHTER_W * FIGHTER_H];
    Sheet s;
    if (!want("closeup.png")) return;
    sh_new(&s, 2 + 4 * 52, 2 + 4 * 84, PAL_UI_CREAM);
    for (i = 0; i < 4; i++) {
        DoomoResident r;
        int x = 2 + i * 52;
        static const uint32_t seeds[4] = { 0, 1007919u, 1015838u, 1023757u };
        if (i == 0) resident_default(&r);
        else if (i == 3) resident_generate_hellborn(&r, seeds[i]);
        else resident_generate(&r, seeds[i]);
        portrait_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, 0, por);
        figure_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, 0, fig);
        bodyspr_render(&r.look, RES_ALIVE, 0, 0, spr);
        fighter_render(&r.look, RES_ALIVE, 3, 0, WFRAME_PISTOL, ftr);
        cell_bg(&s, x, 2, 48, 56, i);
        sh_blit(&s, x, 2, por, PORTRAIT_W, PORTRAIT_H);
        cell_bg(&s, x, 60, 48, 80, i + 1);
        sh_blit(&s, x, 60, fig, FIGURE_W, FIGURE_H);
        cell_bg(&s, x, 142, 48, 50, i);
        sh_blit(&s, x, 142, ftr, FIGHTER_W, FIGHTER_H);
        sh_blit(&s, x + 32, 160, spr, BODYSPR_W, BODYSPR_H);
    }
    sh_save(&s, "closeup.png", 5);
}

int main(int argc, char **argv)
{
    int i, stats = 0, figure = 0;
    const char *bust = NULL;
    NatOptions o;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) outdir = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
        else if (!strcmp(argv[i], "--stats")) stats = 1;
        else if (!strcmp(argv[i], "--bust") && i + 1 < argc) bust = argv[++i];
        else if (!strcmp(argv[i], "--fig")) figure = 1;
        else { fprintf(stderr, "usage: portrait_sheet --out DIR [--zoom N] [--only NAME] [--stats] [--bust LIST [--fig]]\n"); return 2; }
    }
    if (zoom < 1) zoom = 1;
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.quiet = 1; o.save_dir = outdir;
    nat_init(&o);
    palette_init();
    if (bust) { sheet_bust(bust, figure); return 0; }
    sheet_closeup();
    sheet_options();
    sheet_sliders();
    sheet_random(0);
    sheet_random(1);
    sheet_expressions();
    sheet_statuses();
    sheet_anim();
    sheet_walk();
    sheet_fighter();
    sheet_weapons();
    if (stats && n_renders)
        printf("portrait_sheet: %u renders, avg %u / worst %u pixel visits\n",
               (unsigned)n_renders, (unsigned)(total_ops / n_renders), (unsigned)worst_ops);
    return 0;
}
