/* cutstage.c - SCENE_CUTX: the S3 cutscenes on beat.h stages (owner: S3;
 * SPEC sec.7 S3, story_final.md sec.12-13a, comic_timing.md sec.6.1):
 *   CUT_F3      "The Plumber's Order"   (Ep 3.4, sepia, b_f3 in ch3/ep34.dsc)
 *   CUT_F2      "Mingle Night"          (Ep 3.7, sepia, b_f2 in ch3/ep36.dsc)
 *   CUT_F4      "Cupcakes and Bone Soup" (Ep 4.6, b_f4 in ch4/ep46.dsc)
 *   CUT_SUMMIT  the summit (b_summit, or b_summit_dry when Lady Smog's soul
 *               was lost: the jar stays dry)
 *   CUT_SAYIT   "Say It": the walk along the Wall, eight friends and their
 *               unsaid things (story_summit.c picks them)
 *   CUT_E1      the post-credits button (b_e1 in epi/epi.dsc)
 * Like cutscene.c: the script owns all timing (shots, captions, music,
 * SFX, shakes, the sepia, the iris); the stage below draws the current
 * shot (`panel`) from its age (`pt`). START skips, A / B move a shot on
 * (never inside a HOLD). The captions are text ids of the episode's pack,
 * which is live: the script that pushed the cutscene is in it (CUT_E1
 * loads the epilogue's pack itself). Cartoon-modest props: rects, circles
 * and the residents' own chibis.
 */
#include "story_int.h"
#include "story_ch34.h"
#include "doomo/portrait.h"
#include "doomo/save_ext.h"
#include "doomo/camfx.h"
#include "doomo/beat.h"

/* The state lives in the shared scene scratch (scene.h; GBA EWRAM): the
 * cutscene pushes nothing, so the block stays ours until we leave. */
typedef struct {
    int16_t cut;
    uint8_t panel, ended;
    uint16_t shot_t0, cap_t0;
    uint32_t ct, pt;
    char cap[160];                 /* the shot's caption, decoded and cleaned */
    char line[112];                /* Say It: the friend's line              */
} CxState;
#define CX_TAG SAVE_EXT_TAG('C', 'U', 'T', 'X')
static CxState *cxp;
#define cx (*cxp)
static int cx_live(void) { return cxp && scene_scratch_mine(CX_TAG); }

/* ---- text: a pack line without its box codes, on one wrapped page ---------------- */
static void clean(unsigned tid, char *out, int cap)
{
    char raw[200], tmp[200];
    int i, n = 0;
    out[0] = 0;
    if (txt_get((uint16_t)tid, raw, (int)sizeof(raw)) < 0) return;
    for (i = 0; raw[i] && n < (int)sizeof(tmp) - 1; i++) {
        if (raw[i] == '{') {                       /* {d} {..} {r} {big} ... */
            while (raw[i] && raw[i] != '}') i++;
            if (!raw[i]) break;
            continue;
        }
        if (raw[i] == '/') {                       /* the box's line and page breaks */
            if (n && tmp[n - 1] != ' ') tmp[n++] = ' ';
            continue;
        }
        if (raw[i] == ' ' && n && tmp[n - 1] == ' ') continue;
        tmp[n++] = raw[i];
    }
    tmp[n] = 0;
    story_fmt(out, cap, tmp);
}

/* ---- the scene --------------------------------------------------------------------- */
static int script_of(int cut, BeatScript *bs)
{
    switch (cut) {
    case CUT_F2: return txt_beat(BSCR_B_F2, bs);
    case CUT_F3: return txt_beat(BSCR_B_F3, bs);
    case CUT_F4: return txt_beat(BSCR_B_F4, bs);
    case CUT_SUMMIT: return txt_beat(story_flag(SF_SMOG_LOST) ? BSCR_B_SUMMIT_DRY : BSCR_B_SUMMIT, bs);
    case CUT_SAYIT: return txt_beat(BSCR_B_SAYIT, bs);
    case CUT_E1: return txt_beat(BSCR_B_E1, bs);
    default: return 0;
    }
}

static void cx_end(void)
{
    if (cx_live()) cx.ended = 1;
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

static void cx_enter(int arg)
{
    BeatScript bs;
    cxp = (CxState *)scene_scratch(CX_TAG, (uint32_t)sizeof(CxState));
    if (!cxp) return;                            /* cx_update ends it */
    memset(&cx, 0, sizeof(cx));
    cx.cut = (int16_t)arg;
    camfx_reset();
    if (arg == CUT_SAYIT) summit_begin();
    if (!script_of(arg, &bs)) { cx.ended = 1; return; }    /* no pack: nothing to show */
    beat_start(&bs);
}

static void cx_leave(void)
{
    if (beat_running()) beat_skip();             /* popped from outside: no beat left behind */
    camfx_reset();                               /* no sepia on the overworld */
}

static void cx_event(const BeatEv *e)
{
    switch (e->op) {
    case BOP_SHOT:
        cx.panel = (uint8_t)e->a;
        cx.shot_t0 = (uint16_t)beat_now();
        cx.pt = 0;
        if (cx.cut == CUT_SAYIT && cx.panel >= 2 && cx.panel < 2 + SAYIT_FRIENDS) {
            char raw[112];
            cx.cap[0] = 0;
            summit_line(cx.panel - 2, raw, (int)sizeof(raw));
            {   /* the same cleaning as a caption */
                int i, n = 0;
                for (i = 0; raw[i] && n < (int)sizeof(cx.line) - 1; i++) {
                    if (raw[i] == '{') { while (raw[i] && raw[i] != '}') i++; if (!raw[i]) break; continue; }
                    if (raw[i] == '/') { if (n && cx.line[n - 1] != ' ') cx.line[n++] = ' '; continue; }
                    if (raw[i] == ' ' && n && cx.line[n - 1] == ' ') continue;
                    cx.line[n++] = raw[i];
                }
                cx.line[n] = 0;
            }
        }
        if (cx.panel > 0) audio_sfx(SFX_BLIP);
        if (cx.cut == CUT_E1 && cx.panel == 4) camfx_reset();     /* the iris opens on the end card */
        break;
    case BOP_CAPTION:
        clean((unsigned)(uint16_t)e->b, cx.cap, (int)sizeof(cx.cap));
        cx.cap_t0 = (uint16_t)beat_now();
        break;
    case BOP_IRIS_OUT:
        camfx_iris_out(plat_screen_w() / 2, plat_screen_h() / 2 - 16, plat_screen_h() / 4, (e->b >> 6) & 1, e->b & 63);
        break;
    case BOP_IRIS_IN:
        camfx_iris_in(plat_screen_w() / 2, plat_screen_h() / 2 - 16, plat_screen_h() / 4, (e->b >> 6) & 1, e->b & 63);
        break;
    default: break;
    }
}

static void cx_update(void)
{
    int r = BT_RUN, guard = 64;
    if (!cx_live()) { if (beat_running()) beat_skip(); cx_end(); return; }   /* the scratch went: stop */
    camfx_tick();
    cx.ct++;
    if (cx.ended) { cx_end(); return; }
    if (btn_pressed(BTN_START)) { audio_sfx(SFX_CANCEL); beat_skip(); cx_end(); return; }
    if (btn_pressed(BTN_A) || btn_pressed(BTN_B)) beat_press_a();
    do {
        r = beat_tick();
        if (r == BT_EXT) cx_event(beat_ext_event());
    } while (r == BT_EXT && guard-- > 0);
    cx.pt = (uint32_t)(uint16_t)(beat_now() - cx.shot_t0);
    if (r == BT_DONE) cx_end();
}

/* ---- drawing helpers ------------------------------------------------------------------ */
static int big(void) { return plat_screen_h() >= 200; }
static int ground(void) { return plat_screen_h() * 2 / 3 + (big() ? 4 : 0); }

static void sky(uint8_t top, uint8_t mid, uint8_t bot, int y1)
{
    int sw = plat_screen_w(), h3 = y1 / 3;
    plat_rect(0, 0, sw, h3, top);
    plat_checker(0, h3, sw, 2, mid);
    plat_rect(0, h3 + 1, sw, h3, mid);
    plat_checker(0, 2 * h3, sw, 2, bot);
    plat_rect(0, 2 * h3 + 1, sw, y1 - 2 * h3, bot);
}

static void floor_band(int gy, uint8_t c, uint8_t seam)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    plat_rect(0, gy, sw, sh - gy, c);
    plat_checker(0, gy, sw, 2, seam);
}

/* a resident's chibi, feet at (x, y), scale s; flags IMG_*; jitter = shake px */
static void chibi(int id, int x, int y, int dir, int s, int frame, int flags)
{
    const DoomoImage *im = bodyspr_image(id, dir, frame);
    if (!im) return;
    plat_image(x - im->w * s / 2, y - im->h * s, im, s, flags, PAL_RAMP(RAMP_STONE, 1));
}

static int cs(void) { return big() ? 3 : 2; }   /* the chibis' scale */

static void puff(int x, int y, int r, int ramp, int bright, uint32_t t)
{
    static const int8_t px[5] = { -9, -4, 2, 8, 0 }, py[5] = { 3, -3, -4, 2, 4 };
    static const uint8_t pr[5] = { 6, 8, 9, 6, 7 };
    int i, w = ui_wave((int)t, 48, 1);
    for (i = 0; i < 5; i++)
        ui_fill_circle(x + px[i] * r / 10, y + py[i] * r / 10 + (i & 1 ? w : -w), pr[i] * r / 10 + 1,
                       PAL_RAMP(ramp, DOOMO_CLAMP(bright - 2, 0, 7)));
    for (i = 0; i < 5; i++)
        ui_fill_circle(x + px[i] * r / 10, y + py[i] * r / 10 + (i & 1 ? w : -w), pr[i] * r / 10,
                       PAL_RAMP(ramp, bright));
    for (i = 1; i < 3; i++)
        ui_fill_circle(x + px[i] * r / 10 - r / 6, y + py[i] * r / 10 - r / 4, DOOMO_MAX(1, pr[i] * r / 30),
                       PAL_RAMP(ramp, DOOMO_CLAMP(bright + 2, 0, 7)));
}

/* ---- small primitives of our own (no gg_*: SCENE_CUTX is its own PS1
 * group, `cutx`, and must not reach into the cutscene group) ---------------- */
static void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    int t, y;
    if (y1 < y0) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 < y0) { t = x0; x0 = x2; x2 = t; t = y0; y0 = y2; y2 = t; }
    if (y2 < y1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    for (y = y0; y <= y2; y++) {
        int a = y2 > y0 ? x0 + (x2 - x0) * (y - y0) / (y2 - y0) : x0, b;
        if (y < y1) b = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        else b = y2 > y1 ? x1 + (x2 - x1) * (y - y1) / (y2 - y1) : x1;
        if (a > b) { t = a; a = b; b = t; }
        plat_rect(a, y, b - a + 1, 1, c);
    }
}

/* a mitten: palm and four fingers up (dir 0) or sideways (dir -1 / 1) */
static void hand(int x, int y, int s, int dir, uint8_t col)
{
    int f;
    uint8_t o = ui_darker(ui_darker(col));
    ui_fill_circle(x, y, 4 * s + 1, o);
    ui_fill_circle(x, y, 4 * s, col);
    for (f = 0; f < 4; f++) {
        int fx = dir ? x + dir * 4 * s : x - 3 * s + f * 2 * s, fy = dir ? y - 3 * s + f * 2 * s : y - 4 * s;
        ui_fill_circle(fx, fy, s + 1, o);
        ui_fill_circle(fx, fy, s, col);
    }
    ui_fill_circle(x - s, y - s, s, ui_lighter(col));
}

enum { CROWD_CALM, CROWD_CHEER, CROWD_SHOCK };
static void crowd(const int16_t *ids, int n, int x0, int y, int dx, int dir, int scale, int mood, uint32_t t)
{
    int i;
    for (i = 0; i < n; i++) {
        const DoomoImage *b = bodyspr_image(ids[i], dir, mood == CROWD_CHEER ? (int)((t / 6 + (uint32_t)i) & 3) : 0);
        int x = x0 + i * dx, hop = 0;
        if (!b) continue;
        if (mood == CROWD_CHEER) { hop = ui_wave((int)t * 2 + i * 11, 8, 4); hop = hop < 0 ? -hop : hop; }
        if (mood == CROWD_SHOCK) hop = ((t / 3 + (uint32_t)i) & 1) ? 1 : 0;
        plat_rect(x - b->w * scale / 3, y - 2, b->w * scale * 2 / 3, 2, PAL_RAMP(RAMP_STONE, 1));
        plat_image(x - b->w * scale / 2, y - b->h * scale + 2 * scale - hop, b, scale, 0, 0);
        if (mood == CROWD_SHOCK && ((t / 5 + (uint32_t)i) & 3) == 0)
            ui_text_outline(x - 2, y - b->h * scale - 10, "!", PAL_UI_GOLD, PAL_BLACK);
        if (mood == CROWD_CHEER && ((t / 4 + (uint32_t)i) & 3) == 0)
            ui_sparkle(x + 8, y - b->h * scale - 6 - (i & 1) * 4, (int)((t + (uint32_t)i * 3) & 15), PAL_UI_GOLD);
    }
}

/* a string of paper lanterns sagging across the top of the screen */
static void lanterns(int y, uint32_t t)
{
    int sw = plat_screen_w(), r = plat_screen_h() >= 200 ? 5 : 4, n = sw / (r * 6), i;
    static const uint8_t ramp[3] = { RAMP_CANDY, RAMP_LAVA, RAMP_PASTEL_PINK };
    for (i = 0; i <= n; i++) {
        int x = i * sw / n, xn = (i + 1) * sw / n, sag = (i & 1) ? r : r / 2;
        int sway = ui_wave((int)t + i * 9, 40, 1), ly = y + sag + sway;
        if (i < n) plat_line(x, y + sag, xn, y + ((i + 1) & 1 ? r : r / 2), PAL_RAMP(RAMP_STONE, 2));
        plat_rect(x, ly, 1, r / 2 + 1, PAL_RAMP(RAMP_STONE, 2));
        ui_fill_circle(x, ly + r + 1, r, ui_darker(PAL_RAMP(ramp[i % 3], 6)));
        ui_fill_circle(x, ly + r + 1, r - 1, PAL_RAMP(ramp[i % 3], 6));
        plat_rect(x - 1, ly + r, 2, 2, PAL_RAMP(RAMP_CANDY, 7));
    }
}

/* a word balloon wrapped to w, tail toward (tx, ty) below it */
static void balloon(int x, int y, int w, const char *s, int tx)
{
    int lines = ui_text_wrap(0, 0, w - 8, s, 0, 0), h = lines * FONT_LINE + 6;
    if (!s[0]) return;
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 4, PAL_BLACK);
    ui_round_rect(x, y, w, h, 4, PAL_WHITE);
    tri(tx - 3, y + h - 1, tx + 3, y + h - 1, tx, y + h + 5, PAL_WHITE);
    ui_text_wrap(x + 4, y + 3, w - 8, s, PAL_BLACK, 1);
}

/* a table standing on y_floor, its top h px up; returns the top's y */
static int table(int x, int y_floor, int w, int h, int s)
{
    int top = y_floor - h;
    plat_rect(x + 2 * s, top, 2 * s, h, PAL_RAMP(RAMP_WOOD, 1));
    plat_rect(x + w - 4 * s, top, 2 * s, h, PAL_RAMP(RAMP_WOOD, 1));
    plat_rect(x, top - 2 * s, w, 3 * s, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(x, top - 2 * s, w, s, PAL_RAMP(RAMP_WOOD, 5));
    return top - 2 * s;
}

static void bowl(int cx_, int y, int s, uint32_t t)
{
    ui_round_rect(cx_ - 5 * s, y - 4 * s, 10 * s, 4 * s, 2 * s, PAL_RAMP(RAMP_BONE, 3));
    plat_rect(cx_ - 4 * s, y - 4 * s, 8 * s, s, PAL_RAMP(RAMP_LAVA, 3));            /* bone soup */
    if (((t / 12) & 1)) plat_rect(cx_ - s, y - 7 * s, s, 2 * s, PAL_RAMP(RAMP_STONE, 6));   /* steam */
}

static void cupcake(int cx_, int y, int s, int k)
{
    plat_rect(cx_ - 3 * s, y - 3 * s, 6 * s, 3 * s, PAL_RAMP(RAMP_WOOD, 4));
    ui_fill_circle(cx_, y - 4 * s, 3 * s, PAL_PAINT((k * 7 + 3) & 31, 2));
    plat_rect(cx_, y - 8 * s, s, s, PAL_UI_RED);
}


/* the Wall across the screen; gap = 1 opens it in the middle (never here) */
static void wall(int y0, int y1)
{
    int sw = plat_screen_w(), x, y, row = 0;
    for (y = y0; y < y1; y += 8, row++)
        for (x = (row & 1) ? -6 : 0; x < sw; x += 12) {
            plat_rect(x, y, 11, 7, PAL_RAMP(RAMP_STEEL, 2 + ((x / 12 + row) % 3)));
            plat_rect(x, y, 11, 1, PAL_RAMP(RAMP_STEEL, 6));
        }
}

static void giggle_bubbles(int n, int x0, int y0, uint32_t t)
{
    int i;
    for (i = 0; i < n; i++) {
        int bx = x0 + (i % 4) * 9 + ((i / 4) & 1) * 4, by = y0 - (i / 4) * 9 + ui_wave((int)t + i * 7, 40, 1);
        ui_fill_circle(bx, by, 4, PAL_RAMP(RAMP_PASTEL_PINK, 4));
        ui_fill_circle(bx, by, 3, PAL_RAMP(RAMP_PASTEL_PINK, 7));
        plat_rect(bx - 1, by - 2, 1, 1, PAL_WHITE);
    }
}

static void signature(int x, int y, int w, uint32_t t)
{
    int i, prev = 0;
    for (i = 0; i < w && (uint32_t)i < t * 4; i++) {
        int yy = ui_wave(i * 6, 40, 3) + ((i % 13) < 3 ? -3 : 0);
        if (i) plat_line(x + i - 1, y + prev, x + i, y + yy, PAL_RAMP(RAMP_STONE, 0));
        prev = yy;
    }
}

static void paper(int x, int y, int w, int h, int lines)
{
    int i;
    plat_rect(x + 2, y + 2, w, h, PAL_RAMP(RAMP_STONE, 2));
    plat_rect(x, y, w, h, PAL_RAMP(RAMP_BONE, 7));
    for (i = 0; i < lines; i++) plat_rect(x + 5, y + 8 + i * 6, w - 10 - (i & 1) * 12, 1, PAL_RAMP(RAMP_STONE, 3));
}

/* ---- the stages ------------------------------------------------------------------------- */
static void stage_f3(void)
{
    int sw = plat_screen_w(), gy = ground(), s = cs(), p = cx.panel;
    uint32_t pt = cx.pt;
    sky(PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6), PAL_RAMP(RAMP_WOOD, 6), gy);     /* Town Hall office */
    floor_band(gy, PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(sw - 70, 12, 44, 34, PAL_RAMP(RAMP_SKY, 6));                                  /* the window */
    plat_rect(sw - 49, 12, 2, 34, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(sw - 70, 28, 44, 2, PAL_RAMP(RAMP_WOOD, 2));
    if (p == 2 || p == 5) {                                                                 /* close-ups on paper */
        int w = sw / 3, h = gy - 20;
        if (p == 2) {
            paper(sw / 2 - w / 2, 10, w, h, 6);
            ui_text(sw / 2 - w / 2 + 6, 14, "RE: FOUNTAIN", PAL_RAMP(RAMP_STONE, 0));
            ui_fill_circle(sw / 2 + w / 2 - 14, 10 + h - 14, 8, PAL_RAMP(RAMP_BLOOD, 4));    /* APPROVED */
            ui_fill_circle(sw / 2 + w / 2 - 14, 10 + h - 14, 6, PAL_RAMP(RAMP_BONE, 7));
        } else {
            paper(sw / 4 - w / 2 + 6, 14, w, h - 4, 4);
            paper(sw * 3 / 4 - w / 2 - 6, 10, w, h, 5);
            signature(sw / 4 - w / 2 + 12, 14 + h - 22, w - 16, pt + 40);
            signature(sw * 3 / 4 - w / 2, 10 + h - 18, w - 16, pt);
            ui_text(sw * 3 / 4 - w / 2, 14, "SPEECH", PAL_RAMP(RAMP_STONE, 0));
        }
        return;
    }
    {
        int top = table(sw / 2 - 44, gy + 4, 88, 16 * s / 2, 1);                            /* the desk */
        if (p >= 1) paper(sw / 2 - 12, top - 6, 24, 6, 1);
    }
    if (p == 0) return;
    if (p == 3) {                                                                           /* the plumber, a stranger */
        chibi(320, sw / 2 + 52, gy + 6, 1, s, 0, IMG_SILHOUETTE);
        plat_rect(sw / 2 + 64, gy - 26, 3, 14, PAL_RAMP(RAMP_STEEL, 3));                    /* a wrench */
        plat_rect(sw / 2 + 61, gy - 28, 9, 4, PAL_RAMP(RAMP_STEEL, 4));
    }
    chibi(ID_MAYOR, sw / 2 - 30, gy + 6, p == 4 ? 0 : 2, p == 4 ? s + 1 : s, 0, 0);
    if (p != 4) {                                                                           /* the too-big sash */
        int x = sw / 2 - 30;
        plat_line(x - 7 * s, gy - 18 * s, x + 7 * s, gy - 4 * s, PAL_RAMP(RAMP_BLOOD, 4));
        plat_line(x - 7 * s, gy - 17 * s, x + 7 * s, gy - 3 * s, PAL_RAMP(RAMP_BLOOD, 4));
        plat_line(x - 6 * s, gy - 17 * s, x + 8 * s, gy - 3 * s, PAL_RAMP(RAMP_BLOOD, 5));
    }
    if (p == 1) signature(sw / 2 - 8, gy + 4 - 8 * s - 6, 16, pt);
}

static void stage_f2(void)
{
    static const int16_t hill[4] = { ID_NANA, ID_LULLA, ID_BLOOM, ID_STEAMY };
    static const int16_t hell[4] = { ID_MOTHER_NOX, ID_ANVIL, ID_SOOTKIN, ID_TALLYWORN };
    int sw = plat_screen_w(), gy = ground(), s = cs(), p = cx.panel, i;
    uint32_t pt = cx.pt, ct = cx.ct;
    sky(PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_SKY, 2), gy);
    floor_band(gy, PAL_RAMP(RAMP_GRASS, 2), PAL_RAMP(RAMP_GRASS, 3));
    lanterns(8, ct);
    if (p <= 1) {                                                                           /* both sides, dancing */
        crowd(hill, 4, 14, gy + 8, 22, 0, 1, CROWD_CHEER, ct);
        crowd(hell, 4, sw - 90, gy + 8, 22, 0, 1, CROWD_CHEER, ct + 7);
        if (p == 1) {
            chibi(ID_PIP, sw / 2 - 14, gy + 4, 0, 1, 0, 0);                                 /* baby Pip */
            chibi(ID_RIVET, sw / 2 + 14, gy + 6, 0, 1, (int)(ct / 8) & 3, 0);               /* Rivet, six, with a torch */
            plat_rect(sw / 2 + 22, gy - 22, 2, 8, PAL_RAMP(RAMP_WOOD, 2));
            ui_fill_circle(sw / 2 + 23, gy - 24, 3 + (int)((ct / 4) & 1), PAL_RAMP(RAMP_LAVA, 6));
        }
        return;
    }
    if (p == 2 || p == 6) {                                                                 /* the purple cloud, with a string */
        int r = p == 2 ? 14 + (int)DOOMO_MIN(pt / 4, 18u) : 30, ccx = sw / 2, ccy = 30;
        plat_line(ccx, ccy + r / 2, ccx - 6 + ui_wave((int)ct, 50, 4), gy - 4, PAL_RAMP(RAMP_STONE, 6));
        puff(ccx, ccy, r, RAMP_VAPE, 4, ct);
        if (p == 6) {                                                                       /* two little eyes: he does not know it */
            plat_rect(ccx - 6, ccy - 2, 3, 3, PAL_BLACK);
            plat_rect(ccx + 4, ccy - 2, 3, 3, PAL_BLACK);
            chibi(ID_MAYOR, sw / 2, gy + 6, 3, s, 0, 0);
        } else {
            crowd(hill, 4, 14, gy + 8, 22, 0, 1, CROWD_SHOCK, ct);
            crowd(hell, 4, sw - 90, gy + 8, 22, 0, 1, CROWD_SHOCK, ct + 7);
        }
        return;
    }
    if (p == 3) {                                                                           /* Lady Smog breathes it in; the tent */
        int tx = sw * 3 / 4;
        tri(tx - 30, gy, tx + 30, gy, tx, gy - 36, PAL_RAMP(RAMP_CANDY, 3));
        tri(tx - 8, gy, tx + 8, gy, tx, gy - 16, PAL_RAMP(RAMP_VAPE, 0));
        for (i = 0; i < 4; i++) puff(sw / 3 + 20 - (int)DOOMO_MIN(pt, 40u) / 2 + i * 6, gy - 46 + i * 4, 6 - i, RAMP_VAPE, 4, ct + (uint32_t)i * 5);
        chibi(ID_LADY_SMOG, sw / 3, gy + 6, 2, s, 0, 0);
        return;
    }
    if (p == 4) {                                                                           /* young Fuzz tries a hug */
        chibi(ID_LADY_SMOG, sw / 2 + 12, gy + 6, 1, s, 0, 0);
        chibi(ID_FUZZ, sw / 2 - 14 + (int)DOOMO_MIN(pt / 4, 10u), gy + 6, 2, s, 0, 0);
        if (pt > 40) {                                                                      /* it fails: she runs */
            ui_heart(sw / 2, gy - 22 * s, 4, PAL_RAMP(RAMP_STONE, 4));
            plat_line(sw / 2 - 4, gy - 22 * s - 4, sw / 2 + 4, gy - 22 * s + 4, PAL_BLACK);
        }
        return;
    }
    if (p == 5) {                                                                           /* young Giggs between the sides, shaking */
        int j = (int)(ct & 2) ? 1 : -1;
        crowd(hill, 4, 8, gy + 8, 18, 2, 1, CROWD_SHOCK, ct);
        crowd(hell, 4, sw - 72, gy + 8, 18, 1, 1, CROWD_SHOCK, ct + 7);
        chibi(ID_MAYOR, sw / 2 + j, gy + 6, 0, s, 0, 0);
        return;
    }
    if (p == 7) {                                                                           /* Nox reaches for him */
        chibi(ID_MAYOR, sw / 3, gy + 6, 2, s, 0, 0);
        chibi(ID_MOTHER_NOX, sw * 2 / 3, gy + 6, 1, s, 0, 0);
        hand(sw * 2 / 3 - 18 - (int)DOOMO_MIN(pt / 3, 14u), gy - 9 * s, 2, -1, PAL_RAMP(RAMP_STONE, 3));
        return;
    }
    plat_rect(0, 0, sw, gy, PAL_RAMP(RAMP_VAPE, 0));                                        /* close-up: "Close the park." */
    chibi(ID_MAYOR, sw / 2, gy + 10, 0, s + 1, 0, 0);
}

static void stage_f4(void)
{
    int sw = plat_screen_w(), gy = ground(), s = cs(), p = cx.panel, x, y;
    uint32_t ct = cx.ct;
    sky(PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), gy);
    floor_band(gy, PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6));
    lanterns(6, ct);
    for (y = 0; y < 3; y++)                                                                 /* the picnic blanket */
        for (x = 0; x < 8; x++)
            plat_rect(sw / 2 - 48 + x * 12, gy + 4 + y * 6, 12, 6, ((x + y) & 1) ? PAL_RAMP(RAMP_PASTEL_PINK, 4) : PAL_WHITE);
    chibi(ID_MOTHER_NOX, sw / 2 - 30, gy + 12, 2, s, 0, 0);                                 /* little Nox, little Giggs */
    chibi(ID_MAYOR, sw / 2 + 30, gy + 12, 1, s, 0, 0);
    if (p == 1 || p == 3) bowl(sw / 2 - 10, gy + 8, 2, ct);
    if (p == 2 || p == 3) cupcake(sw / 2 + 10, gy + 8, 2, 2);
    if (p == 3) {
        int i;
        for (i = 0; i < 3; i++) ui_heart(sw / 2 - 16 + i * 16, gy - 36 - (int)((cx.pt + (uint32_t)i * 9) % 20u), 3, PAL_UI_PINK);
    }
}

static void stage_summit(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = ground(), s = cs(), p = cx.panel;
    int dry = story_flag(SF_SMOG_LOST), tx = sw / 2;
    uint32_t pt = cx.pt, ct = cx.ct;
    sky(PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_WOOD, 2), gy);      /* Nox's chapel kitchen */
    floor_band(gy, PAL_RAMP(RAMP_WOOD, 1), PAL_RAMP(RAMP_WOOD, 2));
    ui_fill_circle(sw - 40, 26, 10, PAL_RAMP(RAMP_LAVA, 5));                                 /* a bone lamp */
    if (p == 8 || p == 9 || p == 10) {                                                      /* the jar, close */
        plat_rect(0, gy - 4, sw, 4, PAL_RAMP(RAMP_WOOD, 3));
        ss_jar(tx, gy - 2, s, !dry && p >= 9, ct);
        if (p == 8) chibi(ID_MAYOR, sw / 5, gy + 6, 2, s, 0, 0);
        return;
    }
    {
        int top = table(tx - 54, gy + 6, 108, 10 * s, 2), ps = big() ? 2 : 1;
        bowl(tx - 32, top, ps + 1, ct);
        bowl(tx + 32, top, ps + 1, ct);
        cupcake(tx - 8, top, ps + 1, 1);
        cupcake(tx + 8, top, ps + 1, 5);
        if (p >= 7) ss_jar(tx, top, ps, !dry && p >= 9, ct);
        if (p >= 13) {                                                                      /* she takes his hand */
            hand(tx - 8, top - 10, 2, 1, PAL_RAMP(RAMP_PEACH, 5));
            hand(tx + 8, top - 10, 2, -1, PAL_RAMP(RAMP_STONE, 3));
            if (pt > 10) ui_heart(tx, top - 24, 4, PAL_UI_PINK);
        }
    }
    chibi(ID_MAYOR, tx - 70, gy + 6, 2, s, p == 11 ? (int)(ct / 6) & 3 : 0, 0);
    chibi(ID_MOTHER_NOX, tx + 70, gy + 6, 1, s, 0, 0);
    if (p == 7) chibi(ID_VEX, tx + 20, gy + 6, 0, s - 1, 0, 0);
    if (p == 3) ui_heart(tx - 70, gy - 26 * s, 4, PAL_UI_PINK);
    if (p == 11) {
        int i;
        for (i = 0; i < 4; i++) ui_sparkle(tx - 40 + i * 26, gy - 40 - (i & 1) * 8, (int)((ct / 2 + (uint32_t)i * 4) & 15), PAL_WHITE);
    }
    (void)sh;
}

static void stage_sayit(void)
{
    static const int16_t walkers[6] = { ID_NANA, ID_MOTHER_NOX, ID_PIP, ID_SOOTKIN, ID_FUZZ, ID_TALLYWORN };
    int sw = plat_screen_w(), gy = ground(), s = cs(), p = cx.panel, wy = gy - 52 - (big() ? 30 : 0);
    uint32_t ct = cx.ct;
    sky(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_VAPE, 5), wy);
    wall(wy, gy);
    floor_band(gy, PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5));
    if (p >= 2) giggle_bubbles(p - 1 + (p >= 10 ? 4 : 0), 14, gy - 10, ct);
    if (p == 0) {
        crowd(walkers, 6, 20 + (int)((ct / 2) % 40u), gy + 12, 26, 2, 1, CROWD_CALM, ct);
        return;
    }
    if (p == 1) {                                                                           /* the fountain, two-way again */
        int fx = sw / 2;
        ui_round_rect(fx - 20, gy - 14, 40, 14, 4, PAL_RAMP(RAMP_STONE, 4));
        plat_rect(fx - 2, gy - 34, 4, 20, PAL_RAMP(RAMP_STONE, 5));
        plat_rect(fx - 30, gy - 6, 60, 3, PAL_RAMP(RAMP_STEEL, 3));                         /* the pipes */
        ui_fill_circle(fx, gy - 36, 5, PAL_RAMP(RAMP_SKY, 6));
        if ((ct / 3) & 1) ui_sparkle(fx - 26, gy - 8, (int)(ct & 15), PAL_RAMP(RAMP_LAVA, 7));
        chibi(ID_RIVET, fx - 44, gy + 6, 2, s, (int)(ct / 6) & 3, 0);
        chibi(ID_ANVIL, fx + 44, gy + 6, 1, s, 0, 0);
        return;
    }
    if (p >= 2 && p < 2 + SAYIT_FRIENDS) {                                                  /* one friend, one unsaid thing */
        int id = summit_friend(p - 2), fx = sw * 2 / 3;
        chibi(id, fx, gy + 6, 1, s, 0, 0);
        chibi(DOOMO_PLAYER_ID, sw / 3, gy + 6, 2, s, (int)(ct / 8) & 3, 0);
        balloon(10, 6, sw - 20, cx.line, fx - 4);
        return;
    }
    chibi(DOOMO_PLAYER_ID, sw / 2, gy + 6, 3, s, 0, 0);                                     /* your turn */
}

static void stage_e1(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = ground(), s = cs(), p = cx.panel;
    int punch = beat_age(L_PUNCH);
    uint32_t ct = cx.ct;
    if (p >= 4) {                                                                           /* the end card */
        int k = big() ? 3 : 2;
        const char *t = p == 4 ? "THE END" : "";
        ui_backdrop(WIN_CUTE, ct);
        if (t[0]) ui_text_big((sw - ui_text_big_width(t, k)) / 2, sh / 3 - 10, t, k, PAL_WHITE, PAL_UI_PINK_D);
        return;
    }
    sky(PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_PASTEL_PINK, 3), gy); /* home, later: night */
    floor_band(gy, PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(30, 14, 34, 26, PAL_RAMP(RAMP_SKY, 1));
    ui_fill_circle(40, 22, 4, PAL_RAMP(RAMP_CANDY, 7));                                      /* the moon */
    ui_round_rect(sw - 80, gy - 10, 50, 10, 4, PAL_RAMP(RAMP_PASTEL_PINK, 4));              /* the pet bed */
    {
        const DoomoImage *pet = sprite_image(SPR_PET, (int)(ct / 8) & 3);
        int px = p >= 1 ? sw / 2 + 24 : sw - 56;
        if (pet) {
            plat_image(px - pet->w * s / 2, gy - pet->h * s, pet, s, 0, 0);
            ss_horns(px, gy - pet->h * s + 2 * s, s);
        }
    }
    if (p >= 2 || punch >= 0) ss_poop(sw / 2 - 20, gy, s, p >= 3);
}

static void cx_draw(void)
{
    char buf[168];
    int sw, sh, lines, h, y, age;
    if (plat_draw_skipped()) return;
    if (!cx_live()) { plat_clear(PAL_BLACK); return; }
    switch (cx.cut) {
    case CUT_F3: stage_f3(); break;
    case CUT_F2: stage_f2(); break;
    case CUT_F4: stage_f4(); break;
    case CUT_SUMMIT: stage_summit(); break;
    case CUT_SAYIT: stage_sayit(); break;
    case CUT_E1: stage_e1(); break;
    default: plat_clear(PAL_BLACK); break;
    }
    camfx_apply();                               /* the picture tints and shakes, the caption does not */
    if (!cx.cap[0]) return;
    sw = plat_screen_w();
    sh = plat_screen_h();
    doomo_snprintf(buf, sizeof(buf), "%s", cx.cap);
    lines = ui_text_wrap(0, 0, sw - 32, buf, 0, 0);
    h = lines * FONT_LINE + 12;
    y = sh - h - 6;
    age = (uint16_t)(beat_now() - cx.cap_t0);
    if (!ui_window_pop(8, y, sw - 16, h, WIN_CREAM, age)) return;
    ui_text_wrap_n(16, y + 6, sw - 32, buf, ui_style_text(WIN_CREAM), age * 2);
}

const DoomoScene SCENE_CUTX = {
    "CUTX", cx_enter, cx_leave, cx_update, cx_draw, 0
};
