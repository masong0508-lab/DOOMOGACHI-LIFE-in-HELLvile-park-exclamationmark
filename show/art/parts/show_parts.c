/* show_parts.c - the show's own portrait parts (ART track; SHOW_ENGINE_SPEC.md
 * sec.4.3, 9.1).  Mayor Giggs v2: the TURKEY Mad-Hatter mayor.
 *
 * The game's portrait engine has no turkey: no long neck, no wattle, no
 * snood, no crooked tall hat with a card, no floppy bow tie.  These parts are
 * drawn HERE with the engine's own raster primitives (pc_part / pc_ellipse /
 * pc_bezier ..., portrait_int.h), so they get the engine's shading bands,
 * inner contours and selective outline exactly like every other part.  The
 * game never sees this file: the show's patched COPY of core/src/portrait.c
 * (show/engine/patches/portrait.c.patch, hunks (h) and (i)) calls
 *
 *     show_parts_step(c, step)   from draw_step(): 1 = this layer group is
 *                                drawn here (the engine's own is skipped)
 *     show_parts_of(look)        from phase_of(): a part-set look animates its
 *                                fx clock (= the wattle / tuft wobble frames)
 *
 * Which looks get the part set: the TURKEY SIGNATURE, a combination the
 * game's resident_randomize_look() never makes (tests/art/test_parts.py runs
 * the PS1's own generator over 200k seeds to prove it):
 *     species=SPECIES_OWLKIN mouth=MOUTH_BEAK hat=HAT_TOP_HAT
 *     tail=TAIL_FEATHER_FAN accessory=ACC_BOWTIE facial_hair=FHAIR_WIZARD
 * Every other look returns 0 from both hooks: the engine draws it untouched
 * (the AT1.3 portrait CRC self-test stays green).
 *
 * The look's own fields are the part set's knobs (series/looks/cast.shw):
 *     skin            head + neck skin          hair_color   feather tufts
 *     hat_color       the top hat               pattern_color hat band
 *     horn_size 0-15  hat height (crown)        nose_size 0-15 wattle + snood size
 *     marking_color   wattle + snood            accessory_color the bow tie
 *     top_color       the frock coat (TOP_SUIT body + lapels + coat tails)
 *     eye_size 0-15   how far the eyes bulge    horn_color   the card in the band
 *     eye_color       pupils' ring (iris)       brow_style   BROW_NONE = no brows
 * Animation: c->anim bits 3-4 (= the cache's fx phase, 4 frames) swing the
 * wattle, snood, tufts and tail fan (secondary motion); EXPR_SHOCK doubles
 * it.  Blinks follow c->blink, the mouth follows the show's viseme override
 * (portrait_show_mouth, hunk (d)) as beak shapes, and all 11 EXPR_* have
 * their own eyes / brows / beak.
 */
#include "portrait_int.h"
#include "doomo/resident.h"

#if DOOMO_SHOW
extern int8_t portrait_show_mouth;      /* show_actor.c / the host; -1 = off */
#endif

int show_parts_of(const DoomoLook *l);
int show_parts_step(PCtx *c, int step);

int show_parts_of(const DoomoLook *l)
{
    return l && l->species == SPECIES_OWLKIN && l->mouth_style == MOUTH_BEAK &&
           l->hat == HAT_TOP_HAT && l->tail == TAIL_FEATHER_FAN &&
           l->accessory == ACC_BOWTIE && l->facial_hair == FHAIR_WIZARD;
}

/* ---- the part set's frame ------------------------------------------------
 * Head-space design units are Q4 at the BUST's scale (16 = one bust pixel at
 * k = 256); S() scales them to the view.  (ox, oy) is the head centre. */
typedef struct TK {
    int ox, oy, k;          /* head centre (Q4), design scale (Q8)          */
    int nbx, nby;           /* neck base (Q4): the collar                    */
    int tiny, yaw;          /* chibi LOD, head yaw 0 front / 2 profile / 4 back */
    int wob;                /* -1, 0, 1, 0 ... secondary motion frame          */
    int hatc;               /* crown height (design Q4)                       */
    int wat;                /* wattle size 0..15                              */
    int open, grin, narrow; /* beak: drop (design px), corners, O-shape        */
    int frown, tongue, dark;
    int sh;                 /* torso half width (Q4)                         */
} TK;
static TK T;

#define S(v)  ((v) * T.k / 256)
#define HX(v) (T.ox + S(v))
#define HY(v) (T.oy + S(v))
/* x mirrored for the profile view (the head faces the viewer's left) */
#define PXX(v) (T.yaw == 2 ? T.ox - S(v) : T.ox + S(v))

#define INK   PAL_PAINT(PAINT_JET, 0)
#define INK2  PAL_PAINT(PAINT_JET, 1)
#define WHITE PAL_WHITE
#define MOUTHIN_C PAL_RAMP(RAMP_PASTEL_PINK, 1)
#define TONGUE_C  PAL_RAMP(RAMP_PASTEL_PINK, 4)

static const int8_t k_wob[4] = { 0, 1, 0, -1 };

/* the 3x5 digits for the card in the hat band ("18" = Mingle Night, 18:00) */
static const uint8_t k_dig[10][5] = {
    { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 3, 1, 7 }, { 5, 5, 7, 1, 1 },
    { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 2, 2 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
};

static void digit(int x, int y, int d, uint8_t col)
{
    int r, b;
    for (r = 0; r < 5; r++)
        for (b = 0; b < 3; b++)
            if (k_dig[d][r] & (4 >> b)) pc_px(x + b, y + r, col);
}

/* ---- the beak's state from the expression and the viseme override -------- */
static void beak_state(PCtx *c)
{
    int st = MOUTH_BEAK;
    T.open = T.grin = T.narrow = T.frown = T.tongue = T.dark = 0;
    if (c->creepy) {
        st = c->creepy > 1 ? MOUTH_VOID : MOUTH_SHARK;
    } else switch (c->expr) {
    case EXPR_HAPPY: st = MOUTH_BIG_GRIN; break;
    case EXPR_SAD: st = MOUTH_FROWN; break;
    case EXPR_ANGRY: st = MOUTH_TEETH; break;
    case EXPR_LOVE: st = MOUTH_SMILE; break;
    case EXPR_SHOCK: st = MOUTH_OPEN; break;
    case EXPR_SLEEPY: st = MOUTH_DROOL; break;
    case EXPR_SICK: st = MOUTH_WAVY; break;
    case EXPR_DEAD: st = MOUTH_TONGUE; break;
    case EXPR_TALK: st = (c->anim & 4) ? MOUTH_BEAK : MOUTH_GRIN; break;
    default: break;
    }
#if DOOMO_SHOW
    if (portrait_show_mouth >= 0 && portrait_show_mouth < MOUTH_COUNT) st = portrait_show_mouth;
#endif
    switch (st) {
    case MOUTH_FLAT: case MOUTH_STITCHED: break;                 /* M: pressed shut */
    case MOUTH_SMILE: case MOUTH_BEAK: case MOUTH_CAT: case MOUTH_SMIRK:
    case MOUTH_LIPSTICK:
        T.grin = 1; break;                                      /* rest: goofy grin */
    case MOUTH_TEETH: case MOUTH_FANGY: T.open = 1; T.grin = 1; break;   /* C / F */
    case MOUTH_GRIN: T.open = 2; T.grin = 2; break;                      /* E */
    case MOUTH_BIG_GRIN: T.open = 3; T.grin = 2; break;
    case MOUTH_OPEN: T.open = 3; break;                                  /* A */
    case MOUTH_O: T.open = 3; T.narrow = 1; break;                       /* O */
    case MOUTH_POUT: T.open = 1; T.narrow = 2; break;                    /* U */
    case MOUTH_FROWN: case MOUTH_WAVY: T.frown = 1; break;
    case MOUTH_TONGUE: T.open = 2; T.tongue = 1; break;
    case MOUTH_DROOL: T.open = 1; T.tongue = 2; break;
    case MOUTH_SHARK: case MOUTH_VOID: T.open = 3; T.dark = 1; break;
    default: T.open = 2; break;
    }
}

/* ---- placement (draw step 0): the head rides a long neck under a tall hat -- */
static void place(PCtx *c)
{
    const DoomoLook *l = c->l;
    int top, cy, minneck, ty = pb_torso_top();
    T.tiny = c->view_kind == VK_CHIBI;
    T.yaw = c->h.yaw;
    T.k = c->view_kind == VK_PORTRAIT ? 300 : (T.tiny ? 70 : 168);
    T.hatc = 176 + l->horn_size * 12;                 /* 10 -> 296 = 18.5 bust px */
    if (c->view_kind == VK_PORTRAIT) {
        /* the close-up: a bigger head, the shoulders lower in the frame and
         * the crown foreshortened so the whole hat still fits */
        T.hatc = T.hatc * 5 / 8;
        pb_shift(PX(3));
        ty = pb_torso_top();
    }
    T.wat = l->nose_size;
    T.wob = k_wob[(c->anim >> 3) & 3];
    T.sh = c->bw;
    T.nbx = c->bx;
    T.nby = ty + (T.tiny ? 8 : PX(1));
    /* the hat's top touches row 1; the neck is what is left (at least a bit) */
    top = PX(1) + (T.tiny ? 0 : 4);
    cy = top + S(156 + T.hatc + 22);
    minneck = S(T.tiny ? 40 : 96);
    if (cy + S(110) + minneck > T.nby) cy = T.nby - S(110) - minneck;
    T.ox = c->bx + (T.yaw == 2 ? -S(20) : 0);
    T.oy = cy + (c->breath ? 0 : 0);
    phead_set(&c->h, T.ox, T.oy, S(100), S(116), T.yaw, T.tiny ? LOD_TINY : LOD_BIG);
    pb_set_neck(T.oy + S(100));
    beak_state(c);
}

/* ---- the tail fan (behind the body; in front of it seen from the back) ---- */
static void tail_fan(PCtx *c)
{
    PMat a, b, tip;
    int i, n = T.tiny ? 5 : 9, R, cx, cy, sway;
    if (c->view_kind == VK_PORTRAIT) return;     /* busts: the coat fills the frame */
    R = T.tiny ? PX(7) : PX(19);
    cx = c->bx + (c->byaw == 2 ? PX(T.tiny ? 2 : 4) : 0);
    cy = c->by + (T.tiny ? PX(1) : PX(3));
    sway = c->wag ? 3 : -3;
    pm_paint(&a, PAINT_RUST);
    pm_paint(&b, PAINT_CHOCOLATE);
    pm_paint(&tip, PAINT_BONE);
    for (i = 0; i < n; i++) {
        /* -76..+76 degrees from straight up (256 steps = 360 deg) */
        int ang = -54 + (108 * i) / (n - 1) + sway + T.wob * 2;
        int sx = p_sin(ang & 255), cyy = p_cos(ang & 255);
        int L = R - (i == 0 || i == n - 1 ? R / 6 : 0);
        int ex, ey, mx, my, w = T.tiny ? 12 : 30;
        if (c->byaw == 2) sx = sx / 2 + 40;     /* profile: the fan seen edge-on-ish */
        ex = cx + sx * L / 256; ey = cy - cyy * L / 256;
        mx = cx + sx * L * 6 / 2560; my = cy - cyy * L * 6 / 2560;
        pc_part((i & 1) ? &b : &a, SH_SOFT, PF_NOCAST);
        pc_capsule(cx, cy, ex, ey, 8, w);
        if (!T.tiny) {
            /* a dark band and a cream tip: the turkey's barred feathers */
            pc_decal(&tip, SH_FLAT);
            pc_capsule(ex - sx * w / 512, ey + cyy * w / 512, ex, ey, w * 3 / 4, w * 3 / 4);
            pc_lineq(mx - cyy * w / 256, my - sx * w / 256, mx + cyy * w / 256, my + sx * w / 256, b.c[0]);
        } else {
            pc_px(ex >> 4, ey >> 4, tip.c[2]);
        }
    }
}

/* the frock coat's long tails (behind the legs, figures only) */
static void coat_tails(PCtx *c)
{
    PMat m;
    int x0, y0, y1, w;
    if (c->view_kind != VK_FIGURE) return;
    m = c->top;
    w = T.sh + PX(2);
    x0 = c->bx;
    y0 = c->by - PX(4);
    y1 = c->ground - PX(9);
    pc_part(&m, SH_SOFT, 0);
    pc_quad(x0 - w + PX(1), y0, x0 + w - PX(1), y0, x0 + w + PX(2) + T.wob * 4, y1, x0 - w - PX(2) + T.wob * 4, y1);
}

/* ---- the long neck ------------------------------------------------------- */
static void neck(PCtx *c)
{
    PMat m = c->skin;
    int x0 = HX(0), y0 = HY(60), x2 = T.nbx, y2 = T.nby + S(8);
    int x1 = (x0 + x2) / 2 + S(10) + (c->breath ? S(6) : 0), y1 = (y0 + y2) / 2;
    int r0 = T.tiny ? 10 : S(30), r1 = T.tiny ? 12 : S(40), i;
    if (T.yaw == 2) x1 -= S(30);
    pc_part(&m, SH_SOFT, 0);
    pc_bezier(x0, y0, x1, y1, x2, y2, r0, r1, T.tiny ? 2 : 6);
    if (T.tiny) return;
    /* wrinkles: a turkey's neck is all crinkle */
    for (i = 1; i <= 3; i++) {
        int yy = y0 + (y2 - y0) * i / 4 + S(4), xx = x0 + (x1 - x0) * i / 3 / 1;
        pc_lineq(xx - r0 / 2, yy, xx + r0 / 3, yy + 6, m.c[1]);
    }
}

/* ---- head ---------------------------------------------------------------- */
static void head(PCtx *c)
{
    PMat m = c->skin;
    pc_part(&m, SH_BALL, 0);
    c->head_id = pcv.part;
    pc_frame(HX(0), HY(0), S(104), S(120));
    if (T.yaw == 2) {
        pc_ellipse(HX(0), HY(4), S(92), S(112));
        pc_ellipse(PXX(30), HY(-24), S(70), S(80));
    } else {
        pc_ellipse(HX(0), HY(0), S(100), S(116));
        pc_ellipse(HX(0), HY(-40), S(92), S(86));     /* the dome under the hat */
    }
    pc_noframe();
    /* a few bristles on the crown of the head (a bare-skinned bird) */
    if (!T.tiny && T.yaw < 3) {
        pc_lineq(HX(-30), HY(-100), HX(-38), HY(-118), m.c[0]);
    }
}

/* the wattle (throat) and the snood (over the beak): wobble on every frame */
static void wattle(PCtx *c, int snood)
{
    PMat m;
    int sz = 60 + T.wat * 5, w = T.wob * (c->expr == EXPR_SHOCK ? 2 : 1);
    pm_paint(&m, c->l->marking_color);
    if (T.yaw >= 3) return;
    if (T.tiny) {
        if (snood) return;
        pc_part(&m, SH_SOFT, PF_NOCAST);
        pc_capsule(PXX(T.yaw == 2 ? 50 : 6), HY(70), PXX((T.yaw == 2 ? 46 : 8)) + w * 8, HY(110 + T.wat * 2), 10, 14);
        return;
    }
    if (!snood) {
        /* throat wattle: a fleshy lobe off the beak's base, hanging down one
         * side of the neck; its tip swings a frame behind the head */
        int x = T.yaw == 2 ? 50 : 22, base = 66 + T.open * 9, len = sz;
        pc_part(&m, SH_BALL, PF_NOCAST);
        pc_bezier(PXX(x - 10), HY(base), PXX(x + 14) + S(w * 8), HY(base + len / 2),
                  PXX(x + 8) + S(w * 22), HY(base + len), S(16), S(sz * 3 / 10), 4);
        pc_ellipse(PXX(x + 10) + S(w * 18), HY(base + len * 8 / 10), S(sz / 4 + 4), S(sz / 4));
        pc_decal(&m, SH_FLAT);
        pc_lineq(PXX(x + 4) + S(w * 12), HY(base + len / 2), PXX(x + 10) + S(w * 18), HY(base + len * 8 / 10), m.c[0]);
    } else {
        /* snood: from the beak's root, draped over its left shoulder and
         * dangling a little past the tip */
        int x = T.yaw == 2 ? 70 : -14, L = sz / 2 + 40;
        pc_part(&m, SH_BALL, PF_NOCAST);
        pc_bezier(PXX(x), HY(-2), PXX(x - 58), HY(4 + L / 3),
                  PXX(x - 52) + S(w * 22), HY(48 + L), S(9), S(sz / 8 + 8), 4);
    }
}

/* ---- eyes and brows -------------------------------------------------------- */
static void heart_px(int x, int y, uint8_t col)
{
    pc_px(x - 1, y - 1, col); pc_px(x + 1, y - 1, col);
    pc_px(x - 1, y, col); pc_px(x, y, col); pc_px(x + 1, y, col);
    pc_px(x, y + 1, col);
}

static void eyes(PCtx *c)
{
    const DoomoLook *l = c->l;
    PMat wm, lid = c->skin, iris;
    int s, ex = 46, ey = -36, erx = 40 + l->eye_size * 2, ery = 44 + l->eye_size * 2;
    int e = c->expr, shut = c->blink && e != EXPR_DEAD;
    pm_flat(&wm, WHITE);
    wm.c[1] = PAL_GRAY_L;
    wm.line = INK2;
    pm_paint(&iris, l->eye_color);
    if (e == EXPR_SHOCK) { erx += 12; ery += 14; }
    if (T.yaw >= 3) return;
    if (T.tiny) {
        if (T.yaw == 2) {
            pc_px(PXX(32) >> 4, HY(-30) >> 4, shut ? c->skin.c[0] : INK);
        } else {
            pc_px(HX(-36) >> 4, HY(-30) >> 4, shut ? c->skin.c[0] : INK);
            pc_px(HX(36) >> 4, HY(-30) >> 4, shut ? c->skin.c[0] : INK);
        }
        return;
    }
    for (s = -1; s <= 1; s += 2) {
        int X, Y, rx = S(erx), ry = S(ery), px, py, gx = 0, gy = 0;
        if (T.yaw == 2) {
            if (s > 0) continue;
            X = PXX(40); Y = HY(ey);
        } else {
            X = HX(s * ex); Y = HY(ey);
        }
        pc_part(c->creepy ? &iris : &wm, SH_SOFT, PF_DARKLINE);
        if (c->creepy) { pm_paint(&iris, PAINT_JET); pc_same(&iris, SH_FLAT); }
        pc_ellipse(X, Y, rx, ry);
        px = X >> 4; py = Y >> 4;
        if (c->creepy) {
            pc_px(px, py, PAL_PAINT(PAINT_HELLFIRE, 2));
            pc_px(px + 1, py, PAL_PAINT(PAINT_CHERRY, 2));
            pc_px(px, py + 1, PAL_PAINT(PAINT_CHERRY, 1));
            continue;
        }
        if (shut) {
            pc_decal(&lid, SH_SOFT);
            pc_ellipse(X, Y, rx + 4, ry + 4);
            pc_lineq(X - rx, Y + ry / 3, X + rx, Y + ry / 3, c->skin.line);
            continue;
        }
        switch (e) {
        case EXPR_DEAD:
            pc_line(px - 2, py - 2, px + 2, py + 2, INK);
            pc_line(px - 2, py + 2, px + 2, py - 2, INK);
            continue;
        case EXPR_LOVE:
            heart_px(px - s / 2 * 0, py, PAL_PAINT(PAINT_CHERRY, 1));
            pc_px(px - 1, py - 1, PAL_PAINT(PAINT_CHERRY, 2));
            continue;
        case EXPR_HAPPY: gy = -1; break;
        case EXPR_SAD: gy = 1; gx = -s; break;
        case EXPR_SLEEPY: gy = 1; break;
        case EXPR_SICK: gx = s; gy = 1; break;
        case EXPR_ANGRY: gx = -s; break;
        default: gx = -s * 0; break;
        }
        /* tiny pupils: 2x2 dark with a ring of the iris, a 1 px pinprick in shock */
        if (e == EXPR_SHOCK) {
            pc_px(px, py, INK);
        } else {
            px += gx; py += gy;
            pc_px(px - 1, py - 1, iris.c[1]); pc_px(px, py - 1, INK);
            pc_px(px - 1, py, INK); pc_px(px, py, INK);
            pc_px(px + 1, py, iris.c[1]); pc_px(px, py + 1, iris.c[1]);
            pc_px(px - 1, py - 1, WHITE);
        }
        /* lids: the acting is in how much of the eye they cover */
        if (e == EXPR_SLEEPY || e == EXPR_SICK || e == EXPR_SAD || e == EXPR_ANGRY || e == EXPR_HAPPY) {
            int cut;
            pc_decal(&lid, SH_SOFT);
            pc_clip(0, 0, pcv.w, pcv.h);
            if (e == EXPR_HAPPY) {
                /* a cheek push from below: the manic beam */
                cut = (Y + ry / 3) >> 4;
                pc_clip(0, cut, pcv.w, pcv.h);
                pc_ellipse(X, Y + ry, rx + 6, ry * 3 / 4);
                pc_noclip();
                pc_lineq(X - rx, Y + ry / 3, X + rx, Y + ry / 3 - 4, c->skin.line);
            } else if (e == EXPR_ANGRY) {
                pc_tri(X - rx - 8, Y - ry - 8, X + rx + 8, Y - ry - 8, s < 0 ? X + rx + 8 : X - rx - 8, Y);
                pc_lineq(s < 0 ? X - rx : X + rx, Y - ry, s < 0 ? X + rx : X - rx, Y - 4, INK);
            } else {
                int f = e == EXPR_SLEEPY ? 60 : (e == EXPR_SICK ? 45 : 30);
                cut = (Y - ry + ry * 2 * f / 100) >> 4;
                pc_clip(0, 0, pcv.w, cut);
                pc_ellipse(X, Y, rx + 4, ry + 4);
                pc_noclip();
                pc_line((X - rx) >> 4, cut, (X + rx) >> 4, cut, c->skin.line);
            }
        }
    }
}

static void brows(PCtx *c)
{
    int s, in, out, mid;
    uint8_t col = PAL_PAINT(PAINT_CHARCOAL, 0);
    if (T.yaw >= 3 || c->l->brow_style == BROW_NONE || c->expr == EXPR_DEAD) return;
    switch (c->expr) {
    case EXPR_HAPPY: case EXPR_LOVE: in = -108; out = -100; mid = -122; break;
    case EXPR_SAD: case EXPR_SICK: in = -114; out = -90; mid = -104; break;
    case EXPR_ANGRY: in = -84; out = -114; mid = -96; break;
    case EXPR_POSSESSED: in = -76; out = -116; mid = -96; break;
    case EXPR_SHOCK: in = -140; out = -136; mid = -152; break;
    case EXPR_SLEEPY: in = -88; out = -90; mid = -92; break;
    default: in = -100; out = -96; mid = -112; break;     /* nervous-cheery: up */
    }
    if (c->creepy) { in = -76; out = -116; mid = -96; }
    if (T.tiny) {
        if (c->expr == EXPR_ANGRY || c->expr == EXPR_SHOCK) {
            pc_px(HX(-36) >> 4, (HY(in) >> 4), col);
            pc_px(HX(36) >> 4, (HY(in) >> 4), col);
        }
        return;
    }
    {
        PMat m;
        pm_flat(&m, col);
        pc_decal(&m, SH_FLAT);
        for (s = -1; s <= 1; s += 2) {
            if (T.yaw == 2 && s > 0) continue;
            {
                int x0 = T.yaw == 2 ? PXX(18) : HX(s * 10), x2 = T.yaw == 2 ? PXX(78) : HX(s * 84);
                pc_bezier(x0, HY(in), (x0 + x2) / 2, HY(mid), x2, HY(out), S(12), S(9), 4);
            }
        }
    }
}

/* ---- the beak: upper mandible, the gap, the lower mandible ---------------- */
static void beak(void)
{
    PMat up, lo, in;
    int d = T.open == 1 ? 26 : T.open * 20, w = T.narrow ? 44 : 64, base = 4, tipy = 76;
    uint8_t ink;
    pm_paint(&up, PAINT_TANGERINE);
    pm_paint(&lo, PAINT_HONEY);
    pm_flat(&in, T.dark ? INK : MOUTHIN_C);
    ink = up.c[0];
    if (T.yaw >= 3) return;
    if (T.tiny) {
        int bx = T.yaw == 2 ? PXX(96) : HX(0), by = HY(30);
        pc_part(&up, SH_FLAT, PF_NOCAST);
        if (T.yaw == 2) pc_tri(PXX(70), HY(0), PXX(70), HY(50), PXX(140), HY(40));
        else pc_tri(HX(-26), HY(0), HX(26), HY(0), HX(0), HY(56));
        if (T.open >= 2) pc_px(bx >> 4, (by >> 4) + 1, MOUTHIN_C);
        return;
    }
    if (T.yaw == 2) {
        /* profile: a short hooked cone pointing at the viewer's left */
        if (d) {
            pc_part(&in, SH_FLAT, PF_NOCAST);
            pc_tri(PXX(60), HY(10), PXX(60), HY(46 + d), PXX(130), HY(30 + d / 2));
        }
        pc_part(&lo, SH_BALL, 0);
        pc_tri(PXX(60), HY(30 + d / 2), PXX(60), HY(46 + d), PXX(120), HY(38 + d));
        pc_part(&up, SH_BALL, 0);
        pc_tri(PXX(56), HY(-10), PXX(60), HY(34), PXX(150), HY(30));
        pc_capsule(PXX(140), HY(28), PXX(146), HY(40), 10, 6);     /* the hook */
        return;
    }
    /* mouth cavity behind */
    if (d) {
        pc_part(&in, SH_FLAT, PF_NOCAST);
        if (T.narrow) pc_ellipse(HX(0), HY(42 + d / 2), S(w - 4), S(24 + d / 2));
        else pc_ellipse(HX(0), HY(40 + d / 2), S(w), S(22 + d / 2));
        if (T.tongue || (T.open >= 2 && !T.dark)) {
            PMat tg;
            pm_flat(&tg, TONGUE_C);
            pc_decal(&tg, SH_FLAT);
            pc_ellipse(HX(T.tongue == 1 ? 10 : 0), HY(54 + d * (T.tongue == 1 ? 1 : 1) / 2), S(18), S(T.tongue == 1 ? 26 : 10));
        }
        if (T.dark) {
            /* possessed: a jagged rim */
            int k;
            for (k = -2; k <= 2; k++) pc_px(HX(k * 12) >> 4, (HY(30 + d / 2) >> 4) + (k & 1), WHITE);
        }
    }
    /* lower mandible (drops with the viseme) */
    pc_part(&lo, SH_BALL, 0);
    pc_tri(HX(-w + 10), HY(46 + d), HX(w - 10), HY(46 + d), HX(0), HY(70 + d));
    pc_ellipse(HX(0), HY(50 + d), S(w - 10), S(12));
    /* upper mandible: a wide rounded wedge, the grin line curls at its corners */
    pc_part(&up, SH_BALL, 0);
    pc_frame(HX(0), HY(30), S(w + 6), S(40));
    pc_ellipse(HX(0), HY(base + 18), S(w), S(26));
    pc_tri(HX(-w + 2), HY(base + 18), HX(w - 2), HY(base + 18), HX(0), HY(tipy - (d ? 6 : 0)));
    pc_noframe();
    /* nostril and the grin line */
    pc_px(HX(-12) >> 4, HY(base + 10) >> 4, ink);
    pc_px(HX(12) >> 4, HY(base + 10) >> 4, ink);
    if (!d) {
        int y = HY(46) >> 4, xl = HX(-w + 6) >> 4, xr = HX(w - 6) >> 4;
        pc_line(xl + 1, y, xr - 1, y, ink);
        if (T.grin) { pc_px(xl, y - 1, ink); pc_px(xr, y - 1, ink); if (T.grin > 1) { pc_px(xl - 1, y - 2, ink); pc_px(xr + 1, y - 2, ink); } }
        if (T.frown) { pc_px(xl, y + 1, ink); pc_px(xr, y + 1, ink); }
    }
    if (T.tongue == 2) {
        pc_px((HX(w - 14) >> 4), (HY(80 + d) >> 4), PAL_PAINT(PAINT_SKY, 2));
        pc_px((HX(w - 14) >> 4), (HY(80 + d) >> 4) + 1, PAL_PAINT(PAINT_CYAN, 2));
    }
}

/* ---- the feather tufts: the Hatter's hair, as a bird's -------------------- */
static void tufts(PCtx *c)
{
    PMat m;
    int s, i, w = T.wob * (c->expr == EXPR_SHOCK ? 3 : 1);
    static const int16_t tf[4][4] = {   /* root x, root y, tip x, tip y (right side) */
        { 72, -88, 168, -128 }, { 84, -64, 196, -64 }, { 76, -40, 168, -2 }, { 56, -104, 122, -150 },
    };
    pm_paint(&m, c->l->hair_color);
    for (s = -1; s <= 1; s += 2) {
        if (T.yaw == 2 && s < 0) continue;
        pc_part(&m, SH_SOFT, PF_FUZZ);
        for (i = 0; i < (T.tiny ? 2 : 4); i++) {
            int rx = tf[i][0], ry = tf[i][1], tx = tf[i][2], ty = tf[i][3];
            int bend = (i & 1) ? w : -w;
            if (c->expr == EXPR_SHOCK) ty -= 30;
            if (T.yaw == 2) { rx -= 40; tx -= 30; }
            /* a puff that curls up at its tip: wide at the root, then a hook */
            pc_bezier(HX(s * rx), HY(ry), HX(s * (rx + tx) / 2), HY((ry + ty) / 2 + 16 + bend * 8),
                      HX(s * tx) + S(bend * 10), HY(ty + bend * 6),
                      S(T.tiny ? 30 : 38), T.tiny ? 6 : S(14), T.tiny ? 2 : 4);
            if (!T.tiny)
                pc_capsule(HX(s * tx) + S(bend * 10), HY(ty + bend * 6), HX(s * (tx - 14)) + S(bend * 10),
                           HY(ty - 22 + bend * 6), S(12), S(6));
        }
    }
}

/* ---- the crooked tall top hat with the card in its band ------------------- */
static void hat(PCtx *c)
{
    const DoomoLook *l = c->l;
    PMat hm, band, card;
    int H = T.hatc, by = -150, lean = 30, lean2 = 74 + (c->breath ? 4 : 0);
    int bx0 = 8, w0 = 76, w1 = 70, w2 = 94, tilt = -20;
    int ax0, ay0, ax1, ay1, mx0, my0, mx1, my1, tx0, ty0, tx1, ty1;
    pm_paint(&hm, l->hat_color);
    pm_paint(&band, l->pattern_color);
    pm_paint(&card, l->horn_color);
    if (T.yaw == 2) { bx0 = 16; lean = 20; lean2 = 50; tilt = -34; }
    if (T.yaw >= 3) { tilt = 20; lean = -20; lean2 = -60; }
    /* brim (a tilted ellipse) */
    pc_part(&hm, SH_BALL, PF_DARKLINE);
    pc_ellipse_sh(HX(bx0), HY(by), S(T.tiny ? 124 : 140), S(T.tiny ? 30 : 32), tilt);
    if (!T.tiny) {
        /* the brim's underside in shadow: it reads as a brim, not a disc */
        PMat u = hm;
        u.c[2] = hm.c[1]; u.c[3] = hm.c[1]; u.c[4] = hm.c[2];
        pc_decal(&u, SH_BALL);
        pc_ellipse_sh(HX(bx0), HY(by + 12), S(128), S(18), tilt);
    }
    /* crown: two quads, the upper one leaning further (the crook), flaring */
    ax0 = HX(bx0 - w0); ay0 = HY(by - 6); ax1 = HX(bx0 + w0); ay1 = HY(by - 6);
    mx0 = HX(bx0 - w1 + lean); my0 = HY(by - H / 2); mx1 = HX(bx0 + w1 + lean); my1 = HY(by - H / 2 - 8);
    tx0 = HX(bx0 - w2 + lean2); ty0 = HY(by - H); tx1 = HX(bx0 + w2 + lean2); ty1 = HY(by - H - 14);
    pc_part(&hm, SH_SOFT, 0);
    pc_frame((ax0 + tx1) / 2, (ay0 + ty0) / 2, (ax1 - ax0) / 2 + S(30), (ay0 - ty0) / 2 + S(10));
    pc_quad(ax0, ay0, ax1, ay1, mx1, my1, mx0, my0);
    pc_quad(mx0, my0, mx1, my1, tx1, ty1, tx0, ty0);
    pc_noframe();
    /* the lid */
    pc_decal(&hm, SH_FLAT);
    pc_lineq(tx0, ty0, tx1, ty1, hm.c[4]);
    /* band */
    pc_decal(&band, SH_SOFT);
    pc_quad(ax0, ay0 - S(4), ax1, ay1 - S(4), HX(bx0 + w0 - 2 + lean / 3), HY(by - 44),
            HX(bx0 - w0 + 2 + lean / 3), HY(by - 40));
    if (T.yaw >= 3) return;
    /* the card, tucked into the band on the hat's right, sticking up */
    {
        int cx = bx0 + w0 - 18 + lean / 4, cyb = by - 24, cw = T.tiny ? 40 : 104, ch = T.tiny ? 44 : 92;
        int x0 = HX(cx - cw / 2), x1 = HX(cx + cw / 2), x2 = HX(cx + cw / 2 + 16), x3 = HX(cx - cw / 2 + 16);
        int y0 = HY(cyb), y1 = HY(cyb - 6), y2 = HY(cyb - ch - 6), y3 = HY(cyb - ch);
        pc_part(&card, SH_SOFT, PF_NOCAST);
        pc_quad(x0, y0, x1, y1, x2, y2, x3, y3);
        if (!T.tiny && T.k >= 180) {
            int px = ((x0 + x2) / 2 >> 4) - 3, py = ((y0 + y2) / 2 >> 4) - 3;
            digit(px, py, 1, PAL_PAINT(PAINT_CHERRY, 0));
            digit(px + 4, py, 8, PAL_PAINT(PAINT_CHERRY, 0));
        } else if (!T.tiny) {
            pc_px(((x0 + x2) / 2) >> 4, ((y0 + y2) / 2) >> 4, PAL_PAINT(PAINT_CHERRY, 0));
            pc_px((((x0 + x2) / 2) >> 4) + 1, ((y0 + y2) / 2) >> 4, PAL_PAINT(PAINT_CHERRY, 0));
        }
    }
}

/* ---- coat front: huge lapels, the shirt, the floppy bow tie, the rosette --- */
static void lapels(PCtx *c)
{
    PMat coat = c->top, shirt;
    int s, x = T.nbx, y = T.nby - (T.tiny ? 8 : PX(1)), w = T.sh;
    int big = c->view_kind == VK_PORTRAIT;
    int deep = big ? PX(13) : PX(9);
    if (T.tiny || c->byaw >= 3) return;
    pm_paint(&shirt, PAINT_WHITE);
    pc_part(&shirt, SH_SOFT, 0);
    pc_tri(x - PX(3), y, x + PX(3), y, x, y + deep);
    coat.c[2] = coat.c[3];
    coat.c[3] = coat.c[4];
    for (s = -1; s <= 1; s += 2) {
        pc_part(&coat, SH_SOFT, PF_DARKLINE);
        /* the lapel: from the collar out past the shoulder line, down to a point */
        pc_quad(x + s * PX(2), y - PX(1), x + s * (w - PX(1)), y - PX(2),
                x + s * (w * 2 / 3), y + deep * 2 / 3, x + s * PX(1), y + deep);
        pc_tri(x + s * (w - PX(1)), y - PX(2), x + s * (w + PX(1)), y + PX(1), x + s * (w * 2 / 3), y + deep * 2 / 3);
    }
}

static void bow_tie(PCtx *c)
{
    PMat m, dots;
    int x = T.nbx, y = T.nby + (T.tiny ? 0 : PX(1)), s;
    int W = c->view_kind == VK_PORTRAIT ? PX(8) : (T.tiny ? PX(3) : PX(6));
    int Hh = c->view_kind == VK_PORTRAIT ? PX(4) : (T.tiny ? PX(1) : PX(3));
    int flop = T.wob * 6;
    if (c->byaw >= 3) return;
    pm_paint(&m, c->l->accessory_color);
    pm_paint(&dots, PAINT_WHITE);
    if (T.tiny) {
        pc_part(&m, SH_FLAT, PF_NOCAST);
        pc_tri(x, y, x - W, y - Hh, x - W, y + Hh);
        pc_tri(x, y, x + W, y - Hh, x + W, y + Hh);
        return;
    }
    for (s = -1; s <= 1; s += 2) {
        int ex = x + s * (W - PX(1)), ey = y + PX(1) + flop * s;
        pc_part(&m, SH_SOFT, PF_DARKLINE);
        /* a floppy loop: wider at the end, drooping down and out */
        pc_quad(x, y - Hh / 3, x + s * W, y - Hh + flop * s, x + s * (W + PX(1)), y + Hh + PX(1) + flop * s, x, y + Hh / 3);
        pc_ellipse(ex, ey, PX(2), Hh);
        /* a fold line and two big polka dots */
        pc_lineq(x + s * PX(2), y, ex, ey - Hh / 3, m.c[0]);
        pc_px((x + s * (W / 2)) >> 4, (y - Hh / 3) >> 4, dots.c[2]);
        pc_px((ex) >> 4, (ey + Hh / 2) >> 4, dots.c[2]);
    }
    pc_part(&m, SH_BALL, PF_DARKLINE);
    pc_ellipse(x, y, Hh / 2 + 8, Hh / 2 + 12);
}

static void rosette(PCtx *c)
{
    PMat gold, red;
    int big = c->view_kind == VK_PORTRAIT;
    int x = T.nbx + (T.sh * (big ? 56 : 50)) / 100, y = T.nby + (big ? PX(8) : PX(5));
    int r = big ? PX(3) : PX(2);
    if (T.tiny || c->byaw >= 3) return;
    pm_paint(&gold, PAINT_LEMON);
    pm_paint(&red, PAINT_CHERRY);
    pc_part(&red, SH_SOFT, 0);
    pc_quad(x - r / 2, y, x, y, x - r / 2 - 8, y + r * 2 + 8, x - r - 8, y + r * 2);
    pc_quad(x, y, x + r / 2, y, x + r + 8, y + r * 2, x + r / 2 + 8, y + r * 2 + 8);
    pc_part(&gold, SH_BALL, 0);
    pc_ellipse(x, y, r, r);
    pc_decal(&red, SH_BALL);
    pc_ellipse(x, y, r / 2, r / 2);
}

/* ---- the layer groups (portrait.c draw_step, DL_STEPS = 6) ---------------- */
int show_parts_step(PCtx *c, int step)
{
    if (!show_parts_of(c->l)) return 0;
    switch (step) {
    case 0:
        place(c);
        pf_aura_back(c);
        if (c->byaw < 3) tail_fan(c);
        coat_tails(c);
        return 1;
    case 1:
        pb_draw_body(c);
        return 1;
    case 2:
        if (T.yaw >= 3) {
            neck(c);
            head(c);
            tufts(c);
        } else {
            neck(c);
            wattle(c, 0);
            tufts(c);
            head(c);
        }
        return 1;
    case 3:
        lapels(c);
        bow_tie(c);
        rosette(c);
        eyes(c);
        beak();
        wattle(c, 1);
        return 1;
    case 4:
        hat(c);
        brows(c);
        if (c->byaw >= 3) tail_fan(c);
        pb_draw_front(c);
        return 1;
    default:
        return 0;           /* finish(): the engine's lines, fx and states */
    }
}
