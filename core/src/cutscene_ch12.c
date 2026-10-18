/* cutscene_ch12.c - the beat-stage cutscenes of Chapters 1-2 (owner: S2;
 * docs/design/story/SPEC.md sec.7 S2 content, C6; story_final.md sec.9-11;
 * comic_timing.md sec.6).
 *
 * SCENE_CUTSCENE hands these args here (the S2 hunk in cutscene.c):
 *   CUT_P0            P0 "The Egg Crossing", the crayon prologue (ch1/p0.dsc)
 *   CUT_BOTTLE        "The Bottle", Ep 2.5 (ch2/ep25.dsc bv_bottle)
 *   CUT_F1            Flashback F1 "The Fountain Gift" (ch2/f1.dsc)
 *   CUTV_FOG          the Peek at the Wall, Ep 1.6 (ch1/ep16.dsc bv_fog)
 *   CUTV_RITE_FAIL    Sprinklepop's rite, Ep 2.5 (ch2/ep25.dsc bv_rite)
 *   CUTV_GATE_CRACKS  "The Gate Cracks", Ep 2.7: Grumbleguts' first scene
 *                     (ch2/ep27.dsc bv_gate; grumbleguts_gfx, cartoon-modest)
 *
 * As in cutscene.c, a cutscene is a STAGE plus a BeatScript, but the script
 * comes out of a text pack (txt_beat) and the captions are text ids. All
 * timing is the script's: the stages read only the shot (`panel`, BOP_SHOT),
 * the shot's age, how many captions the shot has shown, the age of the
 * pops (BOP_POP obj -> its TWT_* table), beat_age(L_PUNCH / L_BUTTON) and
 * the actor clocks (FREEZE / HITSTOP stop them). Host kinds handled here:
 * SHOT, CAPTION, SAY, POP, INSERT (3-4 frames drawn instead of the stage),
 * IRIS (centred on the shot's subject). Every frame is a pure function of
 * that state, so the native, GBA and PS1 runners agree frame for frame.
 *
 * PS1: the cutscene group with cutscene.c (ovl_plan.py `cutscene(_\w+)?$`).
 * RAM: ~230 B of .bss.
 */
#include "story_int.h"
#include "story_ch12.h"
#include "grumbleguts_gfx.h"
#include "doomo/camfx.h"
#include "doomo/tween.h"
#include "doomo/beat.h"

#define ID_SPRINKLE 81
#define IABS(v) ((v) < 0 ? -(v) : (v))

typedef struct { int16_t cut; uint16_t beat; uint8_t style; } C12Cut;
static const C12Cut cuts12[] = {
    { CUT_P0, BSCR_P0_EGG, WIN_CREAM },
    { CUT_BOTTLE, BSCR_BV_BOTTLE, WIN_DARK },
    { CUT_F1, BSCR_BV_F1, WIN_CREAM },
    { CUTV_FOG, BSCR_BV_FOG, WIN_DARK },
    { CUTV_RITE_FAIL, BSCR_BV_RITE, WIN_CUTE },
    { CUTV_GATE_CRACKS, BSCR_BV_GATE, WIN_HELL },
};
enum { K_P0, K_BOTTLE, K_F1, K_FOG, K_RITE, K_GATE, K_COUNT };

#define POPS 8
static struct {
    int8_t on;                 /* index into cuts12, -1 = none */
    int8_t panel, ins, dead;   /* shot, insert id (0 none), caption is deadpan */
    uint8_t capn, shotcap, ins_n, pops;
    uint16_t shot_t0, cap_t0, ins_t0;
    uint16_t pop_t[POPS];
    uint8_t pop_tw[POPS];
    uint32_t ct;
    char cap[120];
} k = { -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, 0, { 0 } };

/* per-frame geometry */
static int sw, sh, s, gy, capy;   /* the frame's geometry (geo()) */

int cut12_owns(int arg)
{
    int i;
    for (i = 0; i < K_COUNT; i++) if (cuts12[i].cut == arg) return 1;
    return 0;
}

static int now16(void) { return (int)(uint16_t)beat_now(); }
static int pt(void) { return (int)(uint16_t)(beat_now() - k.shot_t0); }
static int cap_age(void) { return (int)(uint16_t)(beat_now() - k.cap_t0); }
static int pop_age(int obj)
{
    if (obj < 0 || obj >= POPS || !((k.pops >> obj) & 1)) return -1;
    return (int)(uint16_t)(beat_now() - k.pop_t[obj]);
}
/* a pop's scale, Q8 (0 before it popped) */
static int pop_q8(int obj)
{
    int a = pop_age(obj);
    const TwTab *t;
    if (a < 0) return 0;
    t = tw_table(k.pop_tw[obj]);
    return t ? tw_tab(t, a) : 256;
}

/* the caption text: {codes} dropped, " / " line breaks joined (the box
 * wraps at its own width), {d} = it appears whole */
static void set_caption(uint16_t id)
{
    char raw[200];
    const char *p;
    int n = 0, sp = 0;
    k.dead = 0;
    k.cap[0] = 0;
    if (txt_get(id, raw, (int)sizeof(raw)) < 0) return;
    {
        char f[200];
        vm_fmt(f, (int)sizeof(f), raw);
        for (p = f; *p && n < (int)sizeof(k.cap) - 1; p++) {
            if (*p == '{') {
                const char *e = strchr(p, '}');
                if (e) {
                    if (p[1] == 'd' && p[2] == '}') k.dead = 1;
                    p = e;
                    continue;
                }
            }
            if (*p == '/' && (p == f || p[-1] == ' ' || p[1] == ' ')) { sp = 1; continue; }
            if (*p == ' ') { sp = 1; continue; }
            if (sp && n > 0 && n < (int)sizeof(k.cap) - 1) k.cap[n++] = ' ';
            sp = 0;
            k.cap[n++] = *p;
        }
    }
    k.cap[n] = 0;
}

void cut12_enter(int arg)
{
    BeatScript bs;
    int i;
    k.on = -1;
    for (i = 0; i < K_COUNT; i++) if (cuts12[i].cut == arg) k.on = (int8_t)i;
    k.panel = 0;
    k.ins = 0;
    k.dead = 0;
    k.capn = k.shotcap = 0;
    k.pops = 0;
    k.shot_t0 = k.cap_t0 = 0;
    k.ct = 0;
    k.cap[0] = 0;
    camfx_reset();
    if (k.on < 0 || !txt_beat(cuts12[(int)k.on].beat, &bs) || !beat_script_valid(&bs)) {
        plat_log("cut12: beat script not live");
        k.on = -1;
        return;
    }
    beat_start(&bs);
}


static void host(const BeatEv *e)
{
    int r;
    switch (e->op) {
    case BOP_SHOT:
        k.panel = (int8_t)e->a;
        k.shot_t0 = (uint16_t)now16();
        k.shotcap = 0;
        k.cap[0] = 0;                                         /* a new shot starts clean */
        if (k.on == K_P0 && k.panel == 7) camfx_iris_off();     /* the title, out of the heart */
        break;
    case BOP_CAPTION: case BOP_SAY:
        set_caption((uint16_t)e->b);
        k.cap_t0 = (uint16_t)now16();
        k.capn++;
        k.shotcap++;
        break;
    case BOP_POP:
        if (e->a >= 0 && e->a < POPS) {
            k.pops = (uint8_t)(k.pops | (1u << e->a));
            k.pop_t[e->a] = (uint16_t)now16();
            k.pop_tw[e->a] = (uint8_t)e->b;
        }
        break;
    case BOP_INSERT:
        k.ins = (int8_t)e->a;
        k.ins_n = (uint8_t)DOOMO_CLAMP(e->b, 1, 4);
        k.ins_t0 = (uint16_t)now16();
        break;
    case BOP_IRIS_OUT: case BOP_IRIS_IN:
        r = ((e->b >> 7) & 63) * 2;                       /* on the shot's subject: the stages */
        if (!r) r = plat_screen_h() / 5;                   /* keep it at 2/5 of the height */
        if (e->op == BOP_IRIS_OUT) camfx_iris_out(plat_screen_w() / 2, plat_screen_h() * 2 / 5, r, (e->b >> 6) & 1, e->b & 63);
        else camfx_iris_in(plat_screen_w() / 2, plat_screen_h() * 2 / 5, r, (e->b >> 6) & 1, e->b & 63);
        break;
    default: break;
    }
}

/* 1 when the cutscene is over (cutscene.c then pops it) */
int cut12_update(void)
{
    int r, guard = 64;
    camfx_tick();
    k.ct++;
    if (k.on < 0) return 1;
    if (btn_pressed(BTN_START)) { audio_sfx(SFX_CANCEL); beat_skip(); camfx_reset(); return 1; }
    if (btn_pressed(BTN_A) || btn_pressed(BTN_B)) beat_press_a();
    do {
        r = beat_tick();
        if (r == BT_EXT) host(beat_ext_event());
    } while (r == BT_EXT && guard-- > 0);
    if (k.ins && (int)(uint16_t)(now16() - k.ins_t0) >= k.ins_n) k.ins = 0;
    if (r == BT_DONE) { camfx_reset(); return 1; }
    return 0;
}

/* ======================================================================
 * drawing: stage units
 * Every stage draws in units of s px (s = 1 at 240x160, 2 at 320x240)
 * around an origin (at()), and the scenery that never moves is a table of
 * prims, not code: the PS1 keeps this file in the cutscene overlay, and
 * the overlay slot is the size of the biggest group (SPEC C4).
 * ==================================================================== */
static int ox, oy, fx = 1;

static void geo(void)
{
    sw = plat_screen_w();
    sh = plat_screen_h();
    s = sh >= 200 ? 2 : 1;
    fx = 1;
    capy = sh;
    if (k.cap[0]) capy = sh - (ui_text_wrap(0, 0, sw - 32, k.cap, 0, 0) * FONT_LINE + 12) - 6;
    gy = DOOMO_MIN(sh * 5 / 8, capy - 6 * s);
}

static void at(int x, int y) { ox = x; oy = y; }
static int X(int u) { return ox + fx * u * s; }
static int Y(int u) { return oy + u * s; }
/* the crayon line "boils": its ends shift by a pixel every 4 frames */
static int boil(int seed) { return (int)(((k.ct / 4u) + (uint32_t)(seed & 255) * 7u) % 3u) - 1; }
static void R(int x, int y, int w, int h, int c)
{
    int x0 = X(x), x1 = X(x + w);
    plat_rect(DOOMO_MIN(x0, x1), Y(y), w * s, h * s, (uint8_t)c);
}
static void C(int x, int y, int r, int c) { ui_fill_circle(X(x), Y(y), r * s, (uint8_t)c); }
static void E(int x, int y, int rx, int ry, int c) { gg_ellipse(X(x), Y(y), rx * s, ry * s, (uint8_t)c); }
static void T(int x0, int y0, int x1, int y1, int x2, int y2, int c)
{
    gg_tri(X(x0), Y(y0), X(x1), Y(y1), X(x2), Y(y2), (uint8_t)c);
}
static void L(int x0, int y0, int x1, int y1, int c)
{
    plat_line(X(x0) + boil(x0), Y(y0) + boil(y0 + 1), X(x1) + boil(x1 + 2), Y(y1) + boil(y1 + 3), (uint8_t)c);
}

typedef struct { uint8_t op, c; int8_t a, b, w, h, e, f; } Prim;
enum { P_END, P_RECT, P_CIRC, P_ELL, P_TRI, P_LINE, P_RRECT, P_HEART, P_CHECK };
#define PR(x, y, w, h, c) { P_RECT, c, x, y, w, h, 0, 0 }
#define PC(x, y, r, c) { P_CIRC, c, x, y, r, 0, 0, 0 }
#define PE(x, y, rx, ry, c) { P_ELL, c, x, y, rx, ry, 0, 0 }
#define PT(x0, y0, x1, y1, x2, y2, c) { P_TRI, c, x0, y0, x1, y1, x2, y2 }
#define PL(x0, y0, x1, y1, c) { P_LINE, c, x0, y0, x1, y1, 0, 0 }
#define PO(x, y, w, h, c) { P_RRECT, c, x, y, w, h, 0, 0 }
#define PH(x, y, sz, c) { P_HEART, c, x, y, sz, 0, 0, 0 }
#define PK(x, y, w, h, c) { P_CHECK, c, x, y, w, h, 0, 0 }
#define PEND { P_END, 0, 0, 0, 0, 0, 0, 0 }
#define RP(r, k) PAL_RAMP(r, k)

static void prims(const Prim *p)
{
    for (; p->op != P_END; p++)
        switch (p->op) {
        case P_RECT: R(p->a, p->b, p->w, p->h, p->c); break;
        case P_CIRC: C(p->a, p->b, p->w, p->c); break;
        case P_ELL: E(p->a, p->b, p->w, p->h, p->c); break;
        case P_TRI: T(p->a, p->b, p->w, p->h, p->e, p->f, p->c); break;
        case P_LINE: L(p->a, p->b, p->w, p->h, p->c); break;
        case P_RRECT: ui_round_rect(DOOMO_MIN(X(p->a), X(p->a + p->w)), Y(p->b), p->w * s, p->h * s, 2 * s, p->c); break;
        case P_HEART: ui_heart(X(p->a), Y(p->b), p->w * s, p->c); break;
        default: plat_checker(DOOMO_MIN(X(p->a), X(p->a + p->w)), Y(p->b), p->w * s, p->h * s, p->c); break;
        }
}
/* a table at (x, y), drawn `k` times bigger (the close-ups) */
static void prims_at(const Prim *p, int x, int y, int scale)
{
    int keep = s;
    at(x, y);
    s *= scale;
    prims(p);
    s = keep;
}

/* crayon hatching: 45-degree strokes clipped to the rect, every `step` px */
static void hatch(int x, int y, int w, int h, int step, int c)
{
    int i;
    for (i = step / 2; i < w + h; i += step)
        plat_line(x + DOOMO_MIN(i, w - 1), y + DOOMO_MAX(0, i - (w - 1)),
                  x + DOOMO_MAX(0, i - (h - 1)), y + DOOMO_MIN(i, h - 1), (uint8_t)c);
}

static void sky(int ramp, int y1)
{
    int h3 = y1 / 3, i;
    for (i = 0; i < 3; i++) {
        plat_rect(0, i * h3, sw, i == 2 ? y1 - 2 * h3 : h3, PAL_RAMP(ramp, 1 + i));
        if (i) plat_checker(0, i * h3 - 1, sw, 2, PAL_RAMP(ramp, 1 + i));
    }
}

/* a cartoon cloud: three outlined circles and a highlight, radius r px */
static void puff(int cx, int cy, int r, int ramp, int k0)
{
    int i;
    for (i = 0; i < 2; i++) {
        int rr = r + 1 - i;
        uint8_t c = PAL_RAMP(ramp, k0 + 2 * i);
        ui_fill_circle(cx - r, cy + r / 3, rr * 2 / 3, c);
        ui_fill_circle(cx + r, cy + r / 3, rr * 2 / 3, c);
        ui_fill_circle(cx, cy, rr, c);
    }
    ui_fill_circle(cx - r / 3, cy - r / 3, DOOMO_MAX(1, r / 3), PAL_RAMP(ramp, DOOMO_MIN(7, k0 + 4)));
}

static void chibi(int id, int x, int yfeet, int scale, int dir, int flags)
{
    const DoomoImage *b = bodyspr_image(id, dir, 0);
    if (!b) return;
    if (!flags) gg_ellipse(x, yfeet - 1, b->w * scale / 3 + 1, scale + 1, PAL_RAMP(RAMP_STONE, 1));
    plat_image(x - b->w * scale / 2, yfeet - b->h * scale + scale, b, scale, flags, PAL_RAMP(RAMP_SKY, 7));
}

/* a stamp: an outlined word in a frame, slammed by TW_SLAM_STAMP */
static void stamp(int cx, int cy, const char *txt, int age, int col)
{
    int big = (tw_tab(&TW_SLAM_STAMP, age) >= 330 ? 3 : 2) + (s > 1), w = ui_text_big_width(txt, big), h = 9 * big;
    if (age < 0) return;
    ui_frame(cx - w / 2 - 4, cy - h / 2 - 3, w + 8, h + 6, (uint8_t)col);
    ui_frame(cx - w / 2 - 3, cy - h / 2 - 2, w + 6, h + 4, (uint8_t)col);
    ui_text_big(cx - w / 2, cy - h / 2, txt, big, (uint8_t)col, PAL_WHITE);
}

/* ======================================================================
 * P0 "The Egg Crossing" (crayon; PAL_CRAYON lerp from the script)
 * shots: 0 the potluck  1 everybody came (the bridge, the cherub and the
 * cloud on a string)  2 fireworks; the string slips at L_PUNCH  3 the pan
 * south with the cloud  4 SMASH: the Wall through the bandstand  5 fog,
 * Nana and the boats (the cookies come back; she eats one 21 frames into
 * that caption)  6 the egg in its boat (the heart iris)  7 the title
 * ==================================================================== */
static const Prim P0_BANDSTAND[] = {
    PR(-32, -6, 64, 5, RP(RAMP_WOOD, 4)),
    PR(-28, -34, 3, 28, PAL_WHITE), PR(-10, -34, 3, 28, PAL_WHITE), PR(8, -34, 3, 28, PAL_WHITE), PR(26, -34, 3, 28, PAL_WHITE),
    PL(-28, -34, -28, -6, RP(RAMP_WOOD, 2)), PL(26, -34, 26, -6, RP(RAMP_WOOD, 2)),
    PT(-38, -33, 38, -33, 0, -52, PAL_UI_RED), PL(-38, -33, 0, -52, PAL_UI_RED_D), PL(38, -33, 0, -52, PAL_UI_RED_D),
    PL(-38, -33, 38, -33, PAL_UI_RED_D), PH(0, -56, 3, PAL_UI_PINK), PEND
};
static const Prim P0_TABLE[] = {
    PR(-18, -12, 36, 3, RP(RAMP_WOOD, 3)), PR(-15, -9, 2, 9, RP(RAMP_WOOD, 2)), PR(13, -9, 2, 9, RP(RAMP_WOOD, 2)), PEND
};
static const Prim P0_TOWER[] = {
    PO(-12, -16, 25, 5, RP(RAMP_PASTEL_PINK, 4)), PO(-9, -22, 18, 5, RP(RAMP_PASTEL_PINK, 4)),
    PO(-5, -28, 11, 5, RP(RAMP_PASTEL_PINK, 4)), PC(0, -30, 2, PAL_UI_RED), PEND
};
static const Prim P0_CUPCAKE[] = { PO(-5, -16, 11, 5, RP(RAMP_PASTEL_PINK, 4)), PC(0, -18, 2, PAL_UI_RED), PEND };
static const Prim P0_CAULDRON[] = { PE(0, -18, 11, 7, RP(RAMP_STONE, 1)), PE(0, -23, 10, 2, RP(RAMP_MINT, 4)), PEND };
static const Prim P0_BOWLS[] = {
    PE(-7, -15, 6, 3, RP(RAMP_STEEL, 4)), PE(-7, -17, 5, 1, RP(RAMP_LAVA, 5)),
    PE(7, -15, 6, 3, RP(RAMP_STEEL, 4)), PE(7, -17, 5, 1, RP(RAMP_LAVA, 5)), PEND
};
static const Prim P0_CHERUB[] = {
    PE(-5, -12, 3, 2, PAL_WHITE), PE(5, -12, 3, 2, PAL_WHITE), PT(-4, 0, 4, 0, 0, -10, RP(RAMP_PASTEL_PINK, 6)),
    PC(0, -13, 4, RP(RAMP_PEACH, 6)), PE(0, -19, 4, 1, PAL_UI_GOLD), PR(-2, -14, 1, 1, PAL_BLACK), PR(1, -14, 1, 1, PAL_BLACK), PEND
};
static const Prim P0_CROWN[] = {
    PT(-4, -6, 4, -6, -3, -11, PAL_UI_GOLD), PT(-4, -6, 4, -6, 0, -12, PAL_UI_GOLD), PT(-4, -6, 4, -6, 3, -11, PAL_UI_GOLD), PEND
};
static const Prim P0_EGG[] = {
    PE(0, -6, 4, 5, PAL_UI_CREAM), PC(-1, -8, 1, PAL_UI_PINK), PC(2, -5, 1, PAL_UI_PINK), PEND
};
static const Prim P0_COOKIES[] = { PO(-4, -9, 8, 8, RP(RAMP_WOOD, 4)), PR(-2, -6, 2, 1, RP(RAMP_STONE, 1)), PEND };
static const Prim P0_HULL[] = {
    PT(-10, -3, 10, -3, -6, 2, PAL_WHITE), PT(10, -3, 6, 2, -6, 2, PAL_WHITE),
    PL(-10, -3, 10, -3, PAL_GRAY), PL(-6, 2, 6, 2, PAL_GRAY), PEND
};
static const Prim P0_NANA[] = {
    PC(-6, -8, 6, PAL_GRAY_L), PC(-2, -11, 6, PAL_GRAY_L), PC(2, -8, 6, PAL_GRAY_L), PC(6, -11, 6, PAL_GRAY_L),
    PC(-6, -8, 5, PAL_WHITE), PC(-2, -11, 5, PAL_WHITE), PC(2, -8, 5, PAL_WHITE), PC(6, -11, 5, PAL_WHITE),
    PE(3, -19, 2, 1, PAL_WHITE), PC(5, -21, 2, PAL_WHITE), PC(8, -21, 2, PAL_WHITE), PC(11, -21, 2, PAL_WHITE), PEND
};

static void p0_paper(int c, int y1, int line, int step)
{
    plat_rect(0, 0, sw, sh, (uint8_t)c);
    hatch(0, 0, sw, y1, step * s, line);
}

static const Prim P0_SUN[] = {             /* boiling rays */
    PC(0, 0, 9, PAL_UI_GOLD), PL(12, 0, 17, 0, PAL_UI_GOLD), PL(-12, 0, -17, 0, PAL_UI_GOLD), PL(0, 12, 0, 17, PAL_UI_GOLD),
    PL(0, -12, 0, -17, PAL_UI_GOLD), PL(8, 8, 12, 12, PAL_UI_GOLD), PL(-8, 8, -12, 12, PAL_UI_GOLD),
    PL(8, -8, 12, -12, PAL_UI_GOLD), PL(-8, -8, -12, -12, PAL_UI_GOLD), PEND
};

static void p0_hill(int c, int line)
{
    ui_fill_ellipse(sw / 2, gy + 30 * s, sw * 3 / 4, 34 * s, (uint8_t)c);
    hatch(0, gy + 2 * s, sw, sh - gy, 9 * s, line);
}

#define FLAG(x, c) PT(x, 1, x + 9, 1, x + 4, 9, c)
static const Prim P0_BUNTING[] = {         /* around the centre, 10 units down */
    PL(-120, 0, 120, 2, RP(RAMP_WOOD, 2)),
    FLAG(-116, PAL_UI_PINK), FLAG(-102, PAL_UI_MINT), FLAG(-88, PAL_UI_GOLD), FLAG(-74, PAL_UI_PINK),
    FLAG(-60, PAL_UI_MINT), FLAG(-46, PAL_UI_GOLD), FLAG(-32, PAL_UI_PINK), FLAG(-18, PAL_UI_MINT),
    FLAG(-4, PAL_UI_GOLD), FLAG(10, PAL_UI_PINK), FLAG(24, PAL_UI_MINT), FLAG(38, PAL_UI_GOLD),
    FLAG(52, PAL_UI_PINK), FLAG(66, PAL_UI_MINT), FLAG(80, PAL_UI_GOLD), FLAG(94, PAL_UI_PINK), FLAG(108, PAL_UI_MINT), PEND
};

/* the potluck: the bandstand between a cupcake tower and a soup cauldron
 * (smash: the Wall through it, one cupcake, two bowls) */
static void p0_potluck(int smash)
{
    int i;
    prims_at(P0_BANDSTAND, sw / 2, gy, 1);
    prims_at(P0_TABLE, sw / 2 - 60 * s, gy, 1);
    prims(smash ? P0_CUPCAKE : P0_TOWER);
    prims_at(P0_TABLE, sw / 2 + 60 * s, gy, 1);
    prims(smash ? P0_BOWLS : P0_CAULDRON);
    if (!smash)
        for (i = 0; i < 3; i++)
            L(-5 + i * 5 + ui_wave((int)k.ct + i * 9, 30, 2), -27, -5 + i * 5 - ui_wave((int)k.ct + i * 9, 30, 2), -36, PAL_GRAY_L);
}

/* a crayon kid: a Hillvile kid with a bow, or a little Hellvile imp */
static const Prim P0_KIDS[2][8] = {
    { PT(-4, -5, 4, -5, 0, -13, RP(RAMP_SKY, 4)), PC(0, -16, 4, RP(RAMP_PEACH, 6)), PR(-2, -17, 1, 1, PAL_BLACK),
      PR(1, -17, 1, 1, PAL_BLACK), PH(3, -20, 1, PAL_UI_PINK), PEND },
    { PT(-4, -5, 4, -5, 0, -13, RP(RAMP_STONE, 2)), PC(0, -16, 4, RP(RAMP_BLOOD, 5)), PR(-2, -17, 1, 1, PAL_BLACK),
      PR(1, -17, 1, 1, PAL_BLACK), PT(-4, -18, -2, -19, -4, -23, RP(RAMP_BONE, 6)), PT(4, -18, 2, -19, 4, -23, RP(RAMP_BONE, 6)), PEND }
};
static void p0_kid(int x, int y, int imp, int frame)
{
    int lg = (frame & 1) ? 2 : -2;
    prims_at(P0_KIDS[imp & 1], x, y, 1);
    L(0, -5, -lg, 0, RP(RAMP_STONE, 1));
    L(0, -5, lg, 0, RP(RAMP_STONE, 1));
}

/* the cloud on a string: a little crowned purple cloud; love = heart eyes;
 * str = the string's other end (px), or -1 = it hangs loose */
static void p0_cloud(int x, int y, int love, int sx, int sy)
{
    if (sx >= 0) plat_line(sx, sy, x, y + 5 * s, PAL_GRAY_L);
    else plat_line(x, y + 5 * s, x - 2 * s, y + 22 * s, PAL_GRAY_L);
    puff(x, y, 6 * s, RAMP_VAPE, 2);
    prims_at(P0_CROWN, x, y, 1);
    if (love) {
        ui_heart(X(-3), Y(-2), s + 1, PAL_UI_PINK);
        ui_heart(X(3), Y(-2), s + 1, PAL_UI_PINK);
    } else {
        R(-3, -1, 1, 1, PAL_BLACK);
        R(2, -1, 1, 1, PAL_BLACK);
    }
}

static void p0_firework(int x, int y, int age, int c)
{
    int i, r;
    if (age < 0 || age > 30) return;
    r = DOOMO_MIN(age * 2, 22);
    at(x, y);
    for (i = 0; i < 10; i++) {
        int cx = fx_cos(i * 410) >> 8, cy = fx_sin(i * 410) >> 8;    /* Q8 */
        if (age < 18) L(cx * r / 512, cy * r / 512, cx * r / 256, cy * r / 256, c);
        else R(cx * r / 256, cy * r / 256 + (age - 18) / 2, 1, 1, c);
    }
}

/* a paper boat; cargo 0 the egg, 1 the bag of burnt cookies */
static void p0_boat(int x, int y, int cargo, int scale)
{
    prims_at(cargo ? P0_COOKIES : P0_EGG, x, y, scale);
    prims_at(P0_HULL, x, y, scale);
}

/* Nana Pudding, crayon: a sheepkin, kneeling. face 0 calm, 1 sick, 2 love */
static void p0_nana(int x, int y, int face)
{
    int i;
    prims_at(P0_NANA, x, y, 1);
    C(8, -16, 5, face == 1 ? RP(RAMP_SICK, 5) : RP(RAMP_PEACH, 6));
    if (face == 2) {
        ui_heart(X(7), Y(-17), s + 1, PAL_UI_PINK);
        ui_heart(X(11), Y(-17), s + 1, PAL_UI_PINK);
        for (i = 0; i < 3; i++) ui_heart(X(10 + i * 5), Y(-26) - (int)((k.ct + (uint32_t)i * 9) % 20u), s + 1, PAL_UI_PINK);
    } else {
        R(7, -17, 1, 1, PAL_BLACK);
        R(10, -17, 1, 1, PAL_BLACK);
    }
    R(7, -13, 3, 1, face == 1 ? RP(RAMP_SICK, 2) : PAL_UI_RED_D);
}

static void p0_river(int y)
{
    plat_rect(0, y, sw, sh - y, RP(RAMP_SKY, 5));
    plat_checker(0, y + 3 * s + (int)((k.ct / 8) & 1), sw, 2, PAL_WHITE);
}

static void p0_wall(int y1)
{
    int y;
    at(sw / 2, 0);
    plat_rect(X(-8), 0, 16 * s, y1, PAL_GRAY);
    for (y = 6; y * s < y1; y += 8) L(-8, y, 8, y, PAL_GRAY_D);
    L(-8, 0, -8, y1 / s, PAL_GRAY_D);
    L(8, 0, 8, y1 / s, PAL_GRAY_D);
}

static void draw_p0(void)
{
    int t = pt(), cx = sw / 2, i, a = beat_age(L_PUNCH);
    switch (k.panel) {
    case 0: case 1:
        p0_paper(RP(RAMP_SKY, 7), gy, RP(RAMP_SKY, 6), 11);
        prims_at(P0_SUN, sw - 30 * s, 24 * s, 1);
        p0_hill(RP(RAMP_GRASS, 5), RP(RAMP_GRASS, 4));
        prims_at(P0_BUNTING, sw / 2, 10 * s, 1);
        p0_potluck(0);
        if (k.panel == 1) {
            int by = gy + 16 * s;
            p0_river(by);
            plat_rect(cx - 40 * s, by - 3 * s, 80 * s, 3 * s, RP(RAMP_WOOD, 4));          /* the bridge */
            plat_rect(cx - 24 * s, by - 5 * s, 48 * s, 2 * s, RP(RAMP_WOOD, 4));
            for (i = 0; i < 6; i++) {                      /* both ways across the bridge */
                int dir = (i & 1) ? -1 : 1, x = cx + dir * (((t * 2 + i * 23) % (100 * s)) - 50 * s);
                p0_kid(x, by - 1 - (IABS(x - cx) < 24 * s ? 2 * s : 0), i & 1, (t / 4 + i) & 1);
            }
            prims_at(P0_CHERUB, sw - 44 * s, gy - 2 * s, 1);
            p0_cloud(sw - 30 * s, gy - 50 * s + ui_wave(t, 30, 2), 0, sw - 40 * s, gy - 10 * s);
        }
        break;
    case 2: {
        int ux = sw - 50 * s, uy = gy - 4 * s, ccx = ux + 10 * s, ccy = gy - 52 * s;
        p0_paper(RP(RAMP_SKY, 1), gy, RP(RAMP_SKY, 2), 9);
        p0_firework(sw / 4, sh / 5, t - 5, PAL_UI_GOLD);
        p0_firework(sw / 2, sh / 7, t - 13, PAL_UI_PINK);
        p0_firework(sw / 3 + 20 * s, sh / 4, t - 21, PAL_UI_MINT);
        p0_firework(sw / 2 + 30 * s, sh / 5, t - 34, PAL_UI_GOLD);
        p0_hill(RP(RAMP_GRASS, 2), RP(RAMP_GRASS, 1));
        prims_at(P0_BANDSTAND, cx - 40 * s, gy, 1);
        prims_at(P0_CHERUB, ux, uy, 1);
        if (a >= 0) { ccx += a * s / 4; ccy -= a * s / 2; }
        p0_cloud(ccx, ccy, t >= 8, a < 0 ? ux + 4 * s : -1, uy - 8 * s);
        break; }
    case 3: {
        int pan = t * 2 * s, y = gy - 50 * s + t * s / 3 + ui_wave(t, 40, 3);
        p0_paper(RP(RAMP_SKY, 1), gy, RP(RAMP_SKY, 2), 9);
        ui_fill_ellipse(sw / 2 - pan / 2, gy + 14 * s, sw, 20 * s, RP(RAMP_GRASS, 2));   /* the hills slide by */
        p0_firework(sw / 5 - pan / 2, sh / 6, 26 + t / 3, PAL_UI_GOLD);
        p0_cloud(sw / 3 + t * s / 2, y, 0, -1, 0);
        break; }
    case 4:
        p0_paper(PAL_GRAY_L, gy, PAL_GRAY, 11);
        p0_hill(RP(RAMP_GRASS, 4), RP(RAMP_GRASS, 3));
        p0_potluck(1);                                     /* one cupcake on Giggs' side, two bowls on Nox's */
        p0_wall(gy + 12 * s);
        break;
    case 5: {
        int ry = gy + 6 * s, back = k.shotcap >= 2, eat = back ? cap_age() - 21 : -1;
        p0_paper(RP(RAMP_VAPE, 6), ry, RP(RAMP_VAPE, 5), 9);
        p0_river(ry);
        p0_wall(ry + 4 * s);
        for (i = 0; i < 5; i++)                             /* the fog on the far side */
            puff(cx + 30 * s + ((i * 31 * s + t) % (sw / 2)), ry - 14 * s - (i % 3) * 10 * s, 8 * s, RAMP_VAPE, 3);
        if (!back) p0_boat(cx - 50 * s + DOOMO_MIN(t, 26) * 4 * s, ry + 6 * s, 0, 1);
        else p0_boat(DOOMO_MAX(cx - 44 * s, cx + 40 * s - cap_age() * 3 * s), ry + 6 * s, 1, 1);
        p0_nana(cx - 62 * s, ry + 2 * s, eat < 0 ? 0 : eat < 8 ? 1 : 2);
        break; }
    case 6:
        plat_rect(0, 0, sw, sh, RP(RAMP_SKY, 6));
        p0_river(sh / 2 + 6 * s);
        p0_boat(cx + (t >= 21 && t < 29 ? ui_wave(t * 4, 8, 2 * s) : 0), sh / 2 + 8 * s, 0, 3);
        break;
    default: {                                              /* 7: the title */
        char l1[48];
        const char *l2 = k.cap;
        int n = 0, dy = tw_tab(&TW_DROP_TITLE_DY, t), big = s + 2;
        p0_paper(PAL_UI_CREAM, sh, RP(RAMP_PASTEL_PINK, 6), 13);
        while (*l2 && n < 15) l1[n++] = *l2++;            /* "DOOMOGACHI LIFE" | "in HELLvile Park!" */
        l1[n] = 0;
        while (*l2 == ' ') l2++;
        while (big > 2 && ui_text_big_width(l1, big) > sw - 12) big--;
        ui_text_big(cx - ui_text_big_width(l1, big) / 2, sh / 3 - 12 * s + dy, l1, big, PAL_UI_PINK, PAL_UI_PINK_D);
        ui_text_big(cx - ui_text_big_width(l2, 2) / 2, sh / 3 + 16 * s + dy, l2, 2, PAL_UI_PURPLE, PAL_WHITE);
        prims_at(P0_EGG, cx, sh * 2 / 3 + 16 * s, 2);
        break; }
    }
}

/* ======================================================================
 * CUTV_FOG: shot 0 the fog wall and the eyes (pops 1-4); insert 1 the
 * kids' tent, eleven years ago; shot 1 SMASH: the Mayor's face
 * ==================================================================== */
static const Prim FOG_TENT[] = {           /* the insert, around the screen's centre */
    PT(-70, 58, 70, 58, 0, -50, RP(RAMP_BLOOD, 3)),
    PL(0, -50, -60, 58, RP(RAMP_BLOOD, 5)), PL(0, -50, -30, 58, RP(RAMP_BLOOD, 5)), PL(0, -50, 0, 58, RP(RAMP_BLOOD, 5)),
    PL(0, -50, 30, 58, RP(RAMP_BLOOD, 5)), PL(0, -50, 60, 58, RP(RAMP_BLOOD, 5)),
    PC(30, -6, 14, RP(RAMP_STONE, 0)), PC(18, 0, 9, RP(RAMP_STONE, 0)), PC(42, 0, 9, RP(RAMP_STONE, 0)),
    PT(22, -20, 38, -20, 30, -32, PAL_UI_GOLD), PL(40, 0, 60, 58, PAL_GRAY),
    PC(-10, 8, 16, RP(RAMP_VAPE, 6)), PC(-26, 14, 10, RP(RAMP_VAPE, 6)), PC(6, 14, 10, RP(RAMP_VAPE, 6)),
    PC(-16, 6, 2, PAL_UI_GOLD), PC(-4, 6, 2, PAL_UI_GOLD), PEND
};

static const Prim SPEED[] = {             /* the smash's speed lines */
    PL(70, 0, 120, 0, PAL_GRAY_L), PL(-70, 0, -120, 0, PAL_GRAY_L), PL(0, 50, 0, 100, PAL_GRAY_L), PL(0, -50, 0, -100, PAL_GRAY_L),
    PL(56, 40, 100, 72, PAL_GRAY_L), PL(-56, 40, -100, 72, PAL_GRAY_L), PL(56, -40, 100, -72, PAL_GRAY_L),
    PL(-56, -40, -100, -72, PAL_GRAY_L), PEND
};

static void draw_fog12(void)
{
    static const int8_t ex[4] = { -40, 30, -8, 58 }, ey[4] = { -18, -32, 2, -6 };
    int i, x, t = beat_clock(0), blink = (beat_clock(1) % 70) > 66, y0 = sh / 4, y1 = sh * 3 / 5;
    if (k.ins) {                                           /* insert 1: the kids' tent, eleven years ago */
        plat_rect(0, 0, sw, sh, RP(RAMP_BLOOD, 0));
        prims_at(FOG_TENT, sw / 2, sh / 2, 1);
        return;
    }
    if (k.panel >= 1) {                                    /* SMASH: the Mayor, panting */
        const DoomoImage *f = portrait_image(ID_MAYOR, EXPR_SHOCK);
        int p = pt(), sc = s + 2;
        plat_rect(0, 0, sw, sh, PAL_WHITE);
        prims_at(SPEED, sw / 2, sh / 2, 1);
        if (f) plat_image(sw / 2 - f->w * sc / 2, sh / 2 - f->h * sc / 2 + ui_wave(p, 10, 2 * s), f, sc, 0, 0);
        return;
    }
    sky(RAMP_VAPE, sh);
    plat_rect(0, y0, sw, y1 - y0, RP(RAMP_STEEL, 2));      /* the Wall */
    plat_rect(0, y0, sw, s, RP(RAMP_STEEL, 4));
    for (x = 0; x < sw; x += 16 * s) {
        plat_rect(x, y0, s, y1 - y0, RP(RAMP_STEEL, 1));
        plat_rect(x + 4 * s, y0 + 4 * s, s + 1, s + 1, RP(RAMP_STEEL, 4));
    }
    for (i = 0; i < 8; i++)
        puff((int)((uint32_t)(i * 67 * s) + (uint32_t)t * (uint32_t)(1 + (i & 1)) / 2u) % (uint32_t)(sw + 80 * s) - 40 * s,
             y0 + (i % 4) * 14 * s + ui_wave(t + i * 9, 60, 3), (10 + (i % 3) * 4) * s, RAMP_VAPE, 3 + (i & 1));
    at(sw / 2, sh / 2);
    for (i = 0; i < 4 && !blink; i++) {                     /* the eyes: two each */
        int r = (2 * s + 1) * pop_q8(i + 1) / 256;
        if (r <= 0) continue;
        plat_checker(X(ex[i] - 10), Y(ey[i] - 3), 20 * s, 6 * s, RP(RAMP_BLOOD, 2));
        ui_fill_circle(X(ex[i] - 6), Y(ey[i]), r, PAL_UI_RED);
        ui_fill_circle(X(ex[i] + 6), Y(ey[i]), r, PAL_UI_RED);
    }
    plat_rect(0, gy + 4 * s, sw, sh - gy, RP(RAMP_VAPE, 1));
    plat_checker(0, gy + 4 * s, sw, 2, RP(RAMP_VAPE, 2));
    chibi(DOOMO_PLAYER_ID, sw / 5, gy + 14 * s, s, 3, 0);  /* you, from behind */
}

/* ======================================================================
 * CUTV_RITE_FAIL: shot 0 the rite in the plaza; shot 1 FAIL, re-slammed on
 * each caption with its counter (x3 bigger), the candles out one a frame
 * after L_PUNCH, SOUL LOST; shot 2 her soul, a map upside down, waving
 * off toward the Wall
 * ==================================================================== */
static const Prim SOUL[] = {               /* her soul, at r = 7 units; the map hangs off it */
    PE(0, 0, 7, 9, PAL_WHITE), PT(-7, 7, 7, 7, -4, 14, PAL_WHITE), PR(-3, -2, 1, 1, PAL_BLACK), PR(2, -2, 1, 1, PAL_BLACK),
    PR(-14, 2, 9, 7, PAL_UI_CREAM), PL(-13, 3, -7, 7, RP(RAMP_WOOD, 3)), PR(-9, 6, 2, 2, PAL_UI_RED), PEND
};

static const Prim TRY1[] = {              /* the spray bottle */
    PE(-10, -40, 1, 1, RP(RAMP_SKY, 6)), PE(-4, -36, 1, 1, RP(RAMP_SKY, 6)), PE(2, -41, 1, 1, RP(RAMP_SKY, 6)),
    PE(8, -35, 1, 1, RP(RAMP_SKY, 6)), PEND
};
static const Prim TRY2[] = { PH(-14, -30, 4, PAL_UI_PINK), PEND };             /* a real hug */
static const Prim TRY3[] = { PE(0, -34, 18, 10, RP(RAMP_SKY, 5)), PE(-6, -38, 6, 3, RP(RAMP_SKY, 7)), PEND };   /* the bucket */
static const Prim *const TRIES[4] = { TRY1 + 4, TRY1, TRY2, TRY3 };

static void draw_rite(void)
{
    int t = pt(), i, a = beat_age(L_PUNCH), hx = sw / 2 + 36 * s, n = k.shotcap;
    sky(RAMP_SKY, gy);
    plat_rect(0, gy, sw, sh - gy, RP(RAMP_STONE, 5));
    for (i = 0; i < sw; i += 20 * s) plat_rect(i, gy, s, sh - gy, RP(RAMP_STONE, 4));
    if (k.panel >= 2) {                                    /* the soul wanders off */
        int q = tw_tab(&TW_POP_SOUL, t);
        plat_rect(sw - 14 * s, 0, 14 * s, gy + 6 * s, PAL_GRAY);
        chibi(ID_SPRINKLE, hx - 30 * s, gy + 8 * s, 2 * s, 0, 0);
        if (q > 128) {
            prims_at(SOUL, hx + DOOMO_MAX(0, t - 30) * s, gy - 34 * s + ui_wave(t, 24, 2 * s), 1);
            E(9, -2 + ui_wave(t * 3, 12, 3), 2, 2, PAL_WHITE);                         /* bye */
        }
        return;
    }
    chibi(ID_FUZZ, sw / 2 - 60 * s, gy + 8 * s, 2 * s, 2, 0);
    chibi(DOOMO_PLAYER_ID, sw / 2 - 30 * s, gy + 10 * s, 2 * s, 2, 0);
    plat_checker(hx - 14 * s, gy - 44 * s, 28 * s, 48 * s, RP(RAMP_VAPE, 4));            /* the possession */
    chibi(ID_SPRINKLE, hx, gy + 8 * s, 2 * s, 1, 0);
    at(hx, gy);
    if (k.panel == 0) {
        if ((t / 3) & 1) ui_text_big(sw / 2 - 70 * s, gy - 60 * s, "!", 2, PAL_UI_RED, PAL_WHITE);
        E(-84, -30, 3, 4, RP(RAMP_SKY, 5));               /* the holy water */
        return;
    }
    prims(TRIES[DOOMO_MIN(n, 3)]);
    if (n >= 1 && a < 8) {                                 /* FAIL, re-slammed, and its counter */
        char xn[3] = { 'x', (char)('0' + DOOMO_MIN(n, 3)), 0 };
        int age = cap_age(), big = (n >= 3 ? 3 : 2) * (s > 1 ? 2 : 1);
        stamp(sw / 2 - 16 * s, sh / 5, "FAIL", age, PAL_UI_RED);
        if (age >= 2) {
            if (tw_tab(&TW_SLAM_COUNTER, age - 2) > 300 && big < 4) big++;
            ui_text_big(sw / 2 + 18 * s, sh / 5 - 4 * s, xn, big, PAL_UI_GOLD, PAL_UI_RED_D);
        }
    }
    at(sw / 2 - 50 * s, sh - 12 * s - (capy < sh ? sh - capy : 0) / 2);
    for (i = 0; i < 5; i++) {                              /* the candles: out one a frame */
        R(i * 24, -8, 3, 8, RP(RAMP_BONE, 6));
        if (a < 0 || a <= i) E(i * 24 + 1 + ui_wave((int)k.ct * 3 + i * 5, 9, 1) / s, -11, 1, 2, PAL_UI_GOLD);
        else if (a - i < 30) E(i * 24 + 1 + (a - i) / 4, -11 - (a - i) / 2, 1, 1, RP(RAMP_STONE, 5));
    }
    if (a >= 8) stamp(sw / 2, sh / 2 - 6 * s, "SOUL LOST.", a - 8, RP(RAMP_VAPE, 1));
}

/* ======================================================================
 * CUT_BOTTLE: shot 0 Fuzz holds the bottle up to the light (Steamy or her
 * ghost, and you); shot 1 the close-up: a stub of chalk, a tally mark
 * ==================================================================== */
static const Prim BOTTLE[] = {             /* the empty vape bottle, centred */
    PO(-4, -7, 8, 14, RP(RAMP_VAPE, 4)), PK(-3, -6, 6, 12, RP(RAMP_VAPE, 6)), PR(-2, -11, 4, 4, RP(RAMP_VAPE, 3)),
    PR(-3, -13, 6, 2, RP(RAMP_STEEL, 4)), PR(-3, -5, 1, 9, PAL_WHITE), PEND
};
static const Prim BOTTLE_INSIDE[] = {      /* at the close-up's scale */
    PR(-2, 4, 3, 1, PAL_WHITE), PR(2, -3, 1, 4, PAL_WHITE), PEND
};

static void draw_bottle(void)
{
    int t = pt(), cx = sw / 2;
    sky(RAMP_PASTEL_PINK, gy);
    plat_checker(cx - 30 * s, 0, 60 * s, gy, PAL_UI_GOLD);              /* the light */
    plat_rect(0, gy, sw, sh - gy, RP(RAMP_STONE, 4));
    if (k.panel == 0) {
        int by = gy - 62 * s - DOOMO_MIN(t, 13) * s;
        chibi(ID_FUZZ, cx, gy + 10 * s, 2 * s, 0, 0);
        gg_hand(cx + 2 * s, by + 14 * s, s, 0, RP(RAMP_PEACH, 6));
        prims_at(BOTTLE, cx + 2 * s, by, 2);
        chibi(ID_STEAMY, cx + 50 * s, gy + 10 * s - (story_flag(SF_STEAMY_SAVED) ? 0 : 4 * s + ui_wave((int)k.ct, 40, 2 * s)),
              2 * s, story_flag(SF_STEAMY_SAVED) ? 1 : 0, story_flag(SF_STEAMY_SAVED) ? 0 : IMG_SILHOUETTE);  /* or her ghost */
        chibi(DOOMO_PLAYER_ID, cx - 50 * s, gy + 10 * s, 2 * s, 2, 0);
        return;
    }
    plat_rect(0, 0, sw, sh, RP(RAMP_PASTEL_PINK, 6));        /* the close-up */
    plat_checker(cx - 40 * s, 0, 80 * s, sh, PAL_UI_GOLD);
    prims_at(BOTTLE, cx, sh * 2 / 5, 4);
    prims_at(BOTTLE_INSIDE, cx, sh * 2 / 5, 4);
    gg_hand(cx, sh * 2 / 5 + 38 * s, 2 * s, 0, RP(RAMP_PEACH, 6));
}

/* ======================================================================
 * CUT_F1 (sepia from the script): shot 0 the forge, little Rivet holding
 * the torch for Anvil; 1 the fountain: two laughing faces, the water arcing
 * between their mouths; 2 the bow; 3 now: Rivet's hand shakes
 * ==================================================================== */
static const Prim FORGE[] = {              /* origin: the right edge, the floor */
    PK(-70, -60, 70, 60, RP(RAMP_LAVA, 3)), PO(-56, -40, 44, 40, RP(RAMP_STONE, 2)), PEND
};
static const Prim ANVIL[] = {              /* origin: the centre, the floor */
    PR(-20, -14, 40, 6, RP(RAMP_STEEL, 2)), PR(-10, -8, 20, 8, RP(RAMP_STEEL, 1)),
    PL(32, -12, 22, -20, RP(RAMP_WOOD, 4)), PE(21, -22, 2, 3, PAL_UI_GOLD), PEND
};
static const Prim FACE[] = {               /* one laughing face (r 9), looking right */
    PC(0, 0, 9, RP(RAMP_STEEL, 2)), PC(0, 0, 8, RP(RAMP_STEEL, 5)), PC(-3, -3, 2, RP(RAMP_STEEL, 7)),
    PR(2, -3, 2, 1, RP(RAMP_STEEL, 1)), PE(4, 2, 3, 2, RP(RAMP_STEEL, 1)), PR(1, 0, 6, 1, RP(RAMP_STEEL, 5)), PEND
};
static const Prim BOW[] = {
    PT(0, 0, -6, -4, -6, 4, PAL_UI_PINK), PT(0, 0, 6, -4, 6, 4, PAL_UI_PINK), PC(0, 0, 2, PAL_UI_PINK_D), PEND
};

static void draw_f1(void)
{
    int i, cx = sw / 2;
    if (k.panel >= 3) {                                    /* now: her hand shakes */
        plat_rect(0, 0, sw, sh, RP(RAMP_WOOD, 5));
        plat_rect(0, gy, sw, sh - gy, RP(RAMP_WOOD, 3));
        plat_rect(cx - 50 * s, gy - 6 * s, 100 * s, 6 * s, RP(RAMP_STEEL, 3));
        gg_hand(cx + ui_wave((int)k.ct * 7, 3, 2 * s), gy - 20 * s, 3 * s, 0, RP(RAMP_BLOOD, 5));
        return;
    }
    plat_rect(0, 0, sw, sh, RP(RAMP_WOOD, 1));
    prims_at(FORGE, sw, gy, 1);
    gg_ellipse(sw - 34 * s, gy - 18 * s, 14 * s, 10 * s, RP(RAMP_LAVA, 5 + ((k.ct / 6) & 1)));
    plat_rect(0, gy, sw, sh - gy, RP(RAMP_STONE, 3));
    prims_at(ANVIL, cx, gy, 1);
    chibi(ID_ANVIL, cx - 46 * s, gy + 2 * s, 2 * s, 2, 0);
    chibi(ID_RIVET, cx + 38 * s, gy + 2 * s, s, 1, 0);
    at(cx, gy);
    if (k.panel == 0) {
        for (i = 0; i < 10; i++) {                        /* sparks off the weld */
            int a = (int)((k.ct * 3u + (uint32_t)i * 7u) % 20u);
            R((i - 5) * a / 3, -16 - a * (10 - IABS(i - 5)) / 8 + a * a / 40, 1, 1, PAL_UI_GOLD);
        }
        return;
    }
    prims_at(FACE, cx - 28 * s, gy - 36 * s, 2);           /* the fountain: two faces, laughing */
    fx = -1;
    prims_at(FACE, cx + 28 * s, gy - 36 * s, 2);
    fx = 1;
    at(cx, gy - 36 * s);
    for (i = 0; i < 7; i++) {                              /* it bubbles up over THERE */
        int u = (int)((k.ct * 2u + (uint32_t)i * 6u) % 42u);
        C(-20 + u * 40 / 42, 4 - u * (42 - u) / 40, 1, RP(RAMP_SKY, 6));
    }
    if (k.panel >= 2) {                                    /* "For the Mingle." */
        prims_at(BOW, cx, gy - 58 * s, 1 + (tw_tab(&TW_POP_BUBBLE, pt()) > 200));
    }
}

/* ======================================================================
 * CUTV_GATE_CRACKS (Grumbleguts' first scene; cartoon-modest, the concept
 * renderer as built): shot 0 Vapula over the sealed gate, Grumbleguts
 * mid-meal on the bagels, his back to us; 1 the forklift; 2 the gut; 3
 * suck in, spring out (the 2nd caption); 4 the belt; 5 the belly-flop
 * (HITSTOP in the air, L_PUNCH the crash: the crack stays); 6 rear-first
 * and stuck, BOING (the 2nd caption) and through; 7 the appetiser
 * ==================================================================== */
static const Prim GATE[] = {               /* origin: the gate's foot */
    PO(-27, -62, 54, 62, RP(RAMP_STONE, 1)), PO(-23, -58, 46, 58, RP(RAMP_STEEL, 2)),
    PR(-14, -54, 2, 54, RP(RAMP_STEEL, 1)), PR(-5, -54, 2, 54, RP(RAMP_STEEL, 1)), PR(4, -54, 2, 54, RP(RAMP_STEEL, 1)),
    PR(13, -54, 2, 54, RP(RAMP_STEEL, 1)), PC(0, -30, 7, RP(RAMP_LAVA, 4)), PC(0, -30, 4, RP(RAMP_STEEL, 1)), PEND
};
static const Prim CRACK[] = {
    PL(0, -56, -6, -48, RP(RAMP_LAVA, 6)), PL(-6, -48, 4, -38, RP(RAMP_LAVA, 6)), PL(4, -38, -3, -28, RP(RAMP_LAVA, 6)),
    PL(-3, -28, 5, -16, RP(RAMP_LAVA, 6)), PL(5, -16, 0, -4, RP(RAMP_LAVA, 6)), PEND
};
static const Prim VAPULA[] = {             /* the crowned cloud over the gate */
    PC(-36, 0, 12, RP(RAMP_VAPE, 1)), PC(-18, 5, 12, RP(RAMP_VAPE, 1)), PC(0, 0, 12, RP(RAMP_VAPE, 1)),
    PC(18, 5, 12, RP(RAMP_VAPE, 1)), PC(36, 0, 12, RP(RAMP_VAPE, 1)),
    PC(-36, 0, 11, RP(RAMP_VAPE, 3)), PC(-18, 5, 11, RP(RAMP_VAPE, 3)), PC(0, 0, 11, RP(RAMP_VAPE, 3)),
    PC(18, 5, 11, RP(RAMP_VAPE, 3)), PC(36, 0, 11, RP(RAMP_VAPE, 3)),
    PT(-12, -9, 12, -9, -8, -20, PAL_UI_GOLD), PT(-12, -9, 12, -9, 2, -22, PAL_UI_GOLD), PT(-12, -9, 12, -9, 11, -18, PAL_UI_GOLD),
    PR(-12, 0, 8, 2, RP(RAMP_LAVA, 6)), PR(4, 0, 8, 2, RP(RAMP_LAVA, 6)), PEND
};
static const Prim FORKLIFT[] = {
    PO(0, -16, 26, 12, PAL_UI_GOLD), PR(14, -28, 2, 12, RP(RAMP_STONE, 1)), PR(24, -28, 2, 12, RP(RAMP_STONE, 1)),
    PR(14, -28, 12, 2, RP(RAMP_STONE, 1)), PR(-8, -6, 10, 1, RP(RAMP_STEEL, 3)),
    PC(5, -3, 3, RP(RAMP_STONE, 1)), PC(20, -3, 3, RP(RAMP_STONE, 1)), PEND
};

static void bagels(int x, int y, int rows)
{
    int r, i;
    at(x, y);
    for (r = 0; r < rows; r++)
        for (i = 0; i < rows - r; i++) {
            E(i * 11 + r * 5, -r * 6, 6, 3, RP(RAMP_WOOD, 3));
            E(i * 11 + r * 5 - 1, -r * 6 - 1, 4, 2, RP(RAMP_WOOD, 5));
            E(i * 11 + r * 5, -r * 6, 2, 1, RP(RAMP_WOOD, 1));
        }
}

/* each shot's figure: view, pose, expression, x (units from a third of
 * the screen), feet (units below the floor line) */
static const uint8_t GG_SHOT[8][3] = {
    { GGV_BACK, GGP_SNACK, GGE_GRIN }, { GGV_BACK_LOOK, GGP_SNACK, GGE_GRIN }, { GGV_SIDE, GGP_STAND, GGE_SMUG },
    { GGV_SIDE, GGP_SUCK_IN, GGE_SUCK }, { GGV_SIDE, GGP_UNBUCKLE, GGE_SULTRY }, { GGV_SIDE, GGP_SPRING, GGE_DOPEY },
    { GGV_BACK, GGP_WIGGLE, GGE_GRIT }, { GGV_FRONT, GGP_STAND, GGE_WINK }
};
static const int8_t GG_X[8] = { 0, 0, 0, 0, 10, -24, 0, 0 }, GG_Y[8] = { 8, 8, 4, 4, 4, 4, 6, 6 };   /* 5, 6: from the gate */

static void draw_gate(void)
{
    GgParams p;
    int t = pt(), a = beat_age(L_PUNCH), gx = sw * 2 / 3, hx = sw / 3, n = k.shotcap, sh_ = DOOMO_MIN(k.panel, 7);
    int pose = GG_SHOT[sh_][1], expr = GG_SHOT[sh_][2], wob = ui_wave(t * 3, 8, 2 * s);
    gg_params_init(&p);
    p.dir = 1;
    p.no_belt = (uint8_t)(k.panel >= 4);
    sky(RAMP_BLOOD, gy);
    plat_rect(0, gy, sw, sh - gy, RP(RAMP_STONE, 2));
    plat_checker(0, gy, sw, 2, RP(RAMP_STONE, 3));
    prims_at(GATE, gx, gy, 1);
    if (k.panel > 5 || (k.panel == 5 && a >= 0)) prims(CRACK);            /* it stays */
    prims_at(VAPULA, gx, 18 * s + ui_wave((int)k.ct, 50, 2), 1);
    if (k.panel <= 1) bagels(hx + 14 * s, gy - 2 * s, 4);
    if (k.panel == 1) {                                    /* the forklift brings the next course */
        int fxp = DOOMO_MAX(sw - t * 6 * s, hx + 60 * s);
        prims_at(FORKLIFT, fxp, gy, 1);
        bagels(t < 13 ? fxp - 6 * s : hx + 50 * s, gy - (t < 13 ? 9 : 2) * s, 2);
    }
    if (k.panel == 0) p.bob = (int16_t)ui_wave(t * 2, 16, s);
    if (k.panel == 2) {                                    /* the gut says hello */
        p.belly_x = (int16_t)wob;
        if ((t / 6) & 1) ui_fill_circle(hx + 26 * s, gy - 30 * s, 2 * s, RP(RAMP_PEACH, 4));
    }
    if (k.panel == 3 && n >= 2) {                          /* it springs back out */
        int ca = cap_age(), amp = DOOMO_MAX(0, 6 - ca / 3) * s;
        pose = GGP_SPRING;
        expr = GGE_RELIEF;
        p.bob = (int16_t)ui_wave(ca * 5, 10, amp);
        p.belly_x = (int16_t)ui_wave(ca * 5 + 3, 10, amp);
    }
    if (k.panel == 4 && t >= 21 && t < 29) ui_text_outline(gx - 10 * s, gy - 50 * s, "creak", PAL_GRAY_L, PAL_BLACK);
    if (k.panel == 5) {
        if (a < 0) {                                       /* in the air (the hit-stop holds it) */
            int k5 = DOOMO_MIN(beat_clock(0) % 64, 8);
            gg_draw(gx - 40 * s + k5 * 3 * s, gy - 14 * s + k5 * s, s, GGV_SIDE, GGP_STAND, GGE_YELP, &p);
            return;
        }
        if (a < 10) gg_burst(gx - 4 * s, gy - 30 * s, (14 + a) * s, PAL_UI_GOLD);
    }
    if (k.panel == 6) {
        if (n >= 2) {                                      /* BOING: through */
            if (cap_age() < 6) gg_burst(gx, gy - 30 * s, (10 + cap_age() * 3) * s, PAL_WHITE);
            return;
        }
        p.tail = (int16_t)(wob * 3 / 2);                   /* rear first, and stuck */
    }
    if (k.panel >= 7 && n >= 2) expr = GGE_SMUG;
    gg_draw_cached((sh_ == 5 || sh_ == 6 ? gx : hx) + GG_X[sh_] * s, gy + GG_Y[sh_] * s, s, GG_SHOT[sh_][0], pose, expr, &p);
}

void cut12_draw(void)
{
    int style, h, y, age, lines;
    if (k.on < 0) { plat_clear(PAL_BLACK); return; }
    geo();
    switch (k.on) {
    case K_P0: draw_p0(); break;
    case K_BOTTLE: draw_bottle(); break;
    case K_F1: draw_f1(); break;
    case K_FOG: draw_fog12(); break;
    case K_RITE: draw_rite(); break;
    default: draw_gate(); break;
    }
    camfx_apply();
    if (!k.cap[0] || (k.on == K_P0 && k.panel == 7) || k.ins) return;   /* the title is the picture */
    style = cuts12[(int)k.on].style;
    lines = ui_text_wrap(0, 0, sw - 32, k.cap, 0, 0);
    h = lines * FONT_LINE + 12;
    y = sh - h - 6;
    age = cap_age();
    if (!ui_window_pop(8, y, sw - 16, h, style, age)) return;
    ui_text_wrap_n(16, y + 6, sw - 32, k.cap, ui_style_text(style), k.dead ? 999 : age * 2);
    if ((k.dead || age * 2 > (int)strlen(k.cap)) && ((k.ct / 10) & 1))
        plat_text(sw - 22, y + h - 10, STR_ICON_ARROW_D, ui_style(style)->accent);
}
