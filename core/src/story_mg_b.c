/* story_mg_b.c - S5's story minigames, the doomo_story_mg_b registry rows
 * (owner: S5; docs/design/story/SPEC.md sec.7 S5, C12, C14; story_final.md
 * sec.14). SCENE_STORY_MG (story_mg.c) frames each one with its title and
 * result cards; `push minigame SMG_B_*` (story_int.h) starts it and the VM
 * reads the result (var 0) after the pop.
 *
 * Four small engines, seven games:
 *   STEER  the pet under the D-pad among drifting things.
 *          S140 "Bubble Test" at the Fun Fair: Vex's rig blows bubbles, the
 *          pet pops the clear ones; the grape ones must float away (three
 *          grape pops and it is over; the first bubble is always grape).
 *          S195 the fireflies: pick the shape (a heart, two faces, the
 *          fountain; it shows dotted in the sky), then lead them with the
 *          pet's glow; one that passes a free point of the shape while lit
 *          settles there. Result 1 + the shape. Past 90 s Glowbug helps.
 *   BEAT   one judge for timed presses and releases.
 *          S140 the Unity Stage: pop each bubble when its ring closes, a
 *          crowd of both sides bobbing on the beat.
 *          S159 the nap-off: hold A to breathe in, let go to breathe out,
 *          on the lullaby's beat; Dozy Doe and Batty Betty (upside down)
 *          breathe it perfectly; Grumbleguts snores in another key.
 *          S184 the duet: two burners, flip the pan whose ball is ready
 *          (B / left, A / right); every third ingredient never lands (he
 *          ate it).
 *   CHALK  S164 Clinker's board: the poem is a 3-line text entry in chalk
 *          words (up / down picks, A writes, B rubs out); result 1 + the
 *          lines with a death in them (she wants exactly one).
 *   SQUEEZE S216 the hug line: hold A to hug, let go in the hugger's zone
 *          (gentle, gentler, firm; Grumbleguts: all the way, rear first,
 *          with the record scratch); the counter reaches 1,000 on the last
 *          good hug.
 *
 * Staging is cartoon-modest (GRUMBLEGUTS_AND_DIRECTOR_NOTES): ui_* / plat_*
 * spans and the citizens' own portraits, no art of its own. The static
 * sets are display lists (scenery(): rects, rounded rects, circles and
 * ellipses in 1/256ths of the screen, so one list serves 240x160 and
 * 320x240) - data instead of code, to stay inside the 8 KB. Every game
 * ends by itself with no input (the runner and the tests rely on it).
 *
 * State lives in the shared scene scratch (0 B of EWRAM), deterministic
 * per game (story_rng), 30 Hz. Story-minigame group code on the PS1 (the
 * "storymg" overlay: the name matches story_mg(_\w+)?).
 */
#include "story_int.h"
#include "doomo/save_ext.h"
#include "doomo/roster.h"

/* Size over speed for this file only (SPEC S5: story_mg_b.c <= 8 KB of
 * code): a few dozen spans a frame at 30 Hz, nothing hot - every toolchain
 * here is GCC. No inlining either: every helper is called from several
 * games, and a call is smaller than a copy. */
#pragma GCC optimize ("Os", "no-inline")

#define MGB_TAG SAVE_EXT_TAG('S', 'M', 'G', 'B')
enum { E_POP, E_STAGE, E_FLY, E_NAP, E_POEM, E_DUET, E_HUG };

typedef struct Obj {
    int16_t x, y;               /* Q4 pixels (BEAT: plain pixels)          */
    int8_t  vx, vy;
    uint8_t kind, age, r, aux;
} Obj;

typedef struct MgB {
    uint16_t id, t, next, idle, pt;
    uint8_t  eng, done, result, end_t;
    uint8_t  n, bad, cur, sel, phase, fx_t, shake, spawned;
    uint8_t  meter, held, flag, cnt;
    int16_t  px, py, fx_x, fx_y;
    uint8_t  word[9];           /* CHALK: the word picked in each slot      */
    uint8_t  slot[16];          /* FLY: 1 + the firefly settled there       */
    int16_t  spt[16][2];        /* FLY: the shape's points on screen        */
    Obj      o[24];
} MgB;
#define G ((MgB *)scene_scratch_block())

static const struct {
    uint8_t pop_goal, pop_grape, pop_gap, pop_secs;
    uint8_t beat, win, stage_n, stage_goal;
    uint8_t fly_n, fly_secs, pick_secs;
    uint8_t nap_n, nap_goal;
    uint8_t poem_idle;
    uint8_t duet_n, duet_goal, duet_miss;
    uint8_t hug_miss, hug_idle, walk;
    uint8_t end;
} VB = { 15, 3, 16, 60,  18, 4, 16, 10,  20, 90, 10,  16, 11,  180,  15, 7, 4,  5, 150, 40,  24 };

/* ---- shared --------------------------------------------------------------- */
static uint32_t rnd(uint32_t k) { return story_rng(((uint32_t)G->id << 16) ^ ((uint32_t)G->t << 3) ^ k); }
static int SX(int v) { return v * plat_screen_w() >> 8; }     /* v / 256 of the screen */
static int SY(int v) { return v * plat_screen_h() >> 8; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
/* -1, 0 or 1 from a pair of buttons (pressed, or held when `held`) */
static int axis(uint32_t neg, uint32_t pos, int pressed)
{
    return pressed ? (btn_pressed(pos) != 0) - (btn_pressed(neg) != 0) : (btn_down(pos) != 0) - (btn_down(neg) != 0);
}

static void finish(int result)
{
    if (G->done) return;
    G->done = 1;
    G->result = (uint8_t)result;
    G->end_t = 0;
}

static void fx(int x, int y, int kind)
{
    MgB *g = G;
    g->fx_x = (int16_t)x;
    g->fx_y = (int16_t)y;
    g->fx_t = 12;
    g->flag = (uint8_t)kind;
}

/* 64 steps a turn */
static int sinw(int a, int r) { return ui_wave(a, 64, r); }
static int cosw(int a, int r) { return ui_wave(a + 16, 64, r); }

/* the scenery display lists: an op and five bytes (x, y, w or rx, h or ry
 * or r, colour) in 1/256ths of the screen width / height; 255 as a size
 * runs to the edge; REP n dx dy repeats the next op n times, stepping */
enum { D_END, D_RECT, D_RRECT, D_CIRC, D_ELL, D_REP };
#define RECT(x, y, w, h, c)  D_RECT, x, y, w, h, c
#define RRECT(x, y, w, h, c) D_RRECT, x, y, w, h, c
#define CIRC(x, y, r, c)     D_CIRC, x, y, 0, r, c
#define ELL(x, y, rx, ry, c) D_ELL, x, y, rx, ry, c
#define REP(n, dx, dy)       D_REP, n, (uint8_t)(dx), (uint8_t)(dy)

static void scenery(const uint8_t *p)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), n = 1, dx = 0, dy = 0, k, x, y, a, b, op;
    while ((op = *p++) != D_END) {
        if (op == D_REP) { n = p[0]; dx = (int8_t)p[1]; dy = (int8_t)p[2]; p += 3; continue; }
        for (k = 0; k < n; k++) {
            x = (p[0] + k * dx) * sw >> 8;
            y = (p[1] + k * dy) * sh >> 8;
            a = p[2] == 255 ? sw - x : DOOMO_MAX(p[2] * sw >> 8, 1);
            b = p[3] == 255 ? sh - y : DOOMO_MAX(p[3] * sh >> 8, 1);
            if (op == D_RECT) plat_rect(x, y, a, b, p[4]);
            else if (op == D_RRECT) ui_round_rect(x, y, a, b, 3, p[4]);
            else if (op == D_CIRC) ui_fill_circle(x, y, b, p[4]);
            else ui_fill_ellipse(x, y, a, b, p[4]);
        }
        p += 5;
        n = 1;
        dx = dy = 0;
    }
}

static void lcd(int x, int y, const char *fmt, int a, int b)
{
    char s[16];
    doomo_snprintf(s, sizeof(s), fmt, a, b);
    ui_round_rect(x, y, ui_text_width(s) + 10, 14, 4, PAL_BLACK);
    ui_text(x + 5, y + 3, s, PAL_UI_LCD);
}

/* outlined white text centred on x */
static void say(int x, int y, const char *s) { ui_text_outline(x - ui_text_width(s) / 2, y, s, PAL_WHITE, PAL_BLACK); }

static void lives(int left, int max)
{
    int k;
    for (k = 0; k < max; k++) ui_heart(plat_screen_w() - 12 * max + k * 12, SY(56), 4, k < left ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_GRAY_D);
}

static void eyes(int x, int y, int gap, int r, int shut)
{
    int k;
    for (k = -gap; k <= gap; k += 2 * gap) {
        if (shut) plat_rect(x + k - r, y, 2 * r + 1, 1, PAL_BLACK);
        else {
            ui_fill_circle(x + k, y, r, PAL_WHITE);
            ui_fill_circle(x + k, y, r / 2 + 1, PAL_BLACK);
        }
    }
}

/* the pet: a round bubblegum blob with little ears and blush */
static void pet(int x, int y, int r, int glow)
{
    uint8_t c = PAL_PAINT(PAINT_BUBBLEGUM, 1 + glow);
    int k;
    for (k = -1; k <= 1; k += 2) {
        ui_fill_circle(x + k * r / 2, y - r + 1, r / 3 + 1, c);
        ui_fill_circle(x + k * (r / 2 + 1), y + r / 3 + r, 1 + r / 6, c);
    }
    ui_fill_circle(x, y, r, c);
    ui_fill_circle(x - r / 3, y - r / 3, r / 4 + 1, PAL_WHITE);
    eyes(x, y - 1, r / 3 + 1, 1 + r / 5, glow);
    ui_fill_circle(x - r / 2 - 1, y + r / 3, 1 + r / 6, PAL_PAINT(PAINT_ROSE, 2));
    ui_fill_circle(x + r / 2 + 1, y + r / 3, 1 + r / 6, PAL_PAINT(PAINT_ROSE, 2));
}

static void pet_move(int sp)
{
    MgB *g = G;
    g->px = (int16_t)clampi(g->px + axis(BTN_LEFT, BTN_RIGHT, 0) * sp, 10 * 16, (plat_screen_w() - 10) * 16);
    g->py = (int16_t)clampi(g->py + axis(BTN_UP, BTN_DOWN, 0) * sp, 24 * 16, (plat_screen_h() - 30) * 16);
}

/* a soap bubble: a pale disc with a rim, a sheen and a glint; grape ones
 * are purple */
static void bubble(int x, int y, int r, int grape)
{
    ui_fill_circle(x, y, r, grape ? PAL_PAINT(PAINT_GRAPE, 0) : PAL_RAMP(RAMP_SKY, 4));
    ui_fill_circle(x, y, r - 1, grape ? PAL_PAINT(PAINT_GRAPE, 2) : PAL_RAMP(RAMP_SKY, 7));
    if (!grape) ui_fill_circle(x + r / 2, y + r / 3, r / 4, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    ui_fill_circle(x - r / 3, y - r / 3, r / 4 + 1, PAL_WHITE);
}

/* the last pop / settle / hit: a ring of dots flying out */
static void burst(uint8_t c)
{
    MgB *g = G;
    int k, rr = 28 - g->fx_t * 2;
    if (g->fx_t)
        for (k = 0; k < 64; k += 8) plat_rect(g->fx_x + cosw(k, rr), g->fx_y + sinw(k, rr), 2, 2, c);
}

/* an approach ring of eight dots */
static void ring(int x, int y, int r, uint8_t c)
{
    int k;
    for (k = 0; k < 64; k += 8) plat_rect(x + cosw(k, r) - 1, y + sinw(k, r) - 1, 2, 2, c);
}

/* ---- the sets ------------------------------------------------------------- */
#define SKY(s)    PAL_RAMP(RAMP_SKY, s)
#define WOOD(s)   PAL_RAMP(RAMP_WOOD, s)
#define PNT(p, s) PAL_PAINT(PAINT_##p, s)
static const uint8_t sc_fair[] = {
    RECT(0, 0, 255, 86, SKY(5)), RECT(0, 86, 255, 86, SKY(6)), RECT(0, 172, 255, 255, SKY(7)),
    REP(10, -3, 10), RECT(205, 116, 2, 3, PAL_RAMP(RAMP_STONE, 4)),
    REP(10, 3, 10), RECT(205, 116, 2, 3, PAL_RAMP(RAMP_STONE, 4)),
    REP(10, 26, 0), RECT(1, 30, 9, 5, PNT(BUBBLEGUM, 1)), REP(10, 26, 0), RECT(3, 35, 5, 5, PNT(BUBBLEGUM, 1)),
    REP(10, 26, 0), RECT(14, 30, 9, 5, PNT(LEMON, 1)), REP(10, 26, 0), RECT(16, 35, 5, 5, PNT(LEMON, 1)),
    RECT(0, 220, 255, 255, PAL_RAMP(RAMP_GRASS, 4)), RECT(0, 220, 255, 3, PAL_RAMP(RAMP_GRASS, 6)),
    RRECT(15, 160, 30, 60, PAL_RAMP(RAMP_STEEL, 3)), RECT(45, 168, 7, 10, PAL_RAMP(RAMP_STEEL, 5)),
    CIRC(24, 138, 10, PNT(LAVENDER, 2)), CIRC(38, 138, 10, PNT(LAVENDER, 2)), CIRC(31, 128, 13, PNT(LAVENDER, 2)),
    D_END };
static const uint8_t sc_stage[] = {
    RECT(0, 0, 255, 86, PAL_RAMP(RAMP_VAPE, 0)), RECT(0, 86, 255, 255, PAL_RAMP(RAMP_VAPE, 1)),
    ELL(45, 120, 20, 120, PAL_RAMP(RAMP_VAPE, 2)), ELL(211, 120, 20, 120, PAL_RAMP(RAMP_VAPE, 2)),
    RECT(10, 34, 9, 255, WOOD(2)), RECT(237, 34, 9, 255, WOOD(2)), RECT(10, 34, 236, 20, WOOD(3)),
    REP(13, 18, 0), CIRC(20, 58, 4, PAL_UI_GOLD),
    RECT(0, 200, 255, 24, WOOD(4)),
    D_END };
static const uint8_t sc_night[] = {
    RECT(0, 0, 255, 128, SKY(0)), RECT(0, 128, 255, 255, SKY(1)),
    CIRC(228, 55, 13, PAL_RAMP(RAMP_CANDY, 7)), CIRC(233, 50, 11, SKY(0)),
    D_END };
static const uint8_t sc_park[] = {
    REP(16, 17, 0), CIRC(0, 210, 21, PAL_RAMP(RAMP_GRASS, 0)),
    RECT(0, 218, 255, 255, PAL_RAMP(RAMP_GRASS, 0)),
    ELL(128, 214, 24, 8, PAL_RAMP(RAMP_STONE, 1)), RECT(126, 186, 4, 30, PAL_RAMP(RAMP_STONE, 1)),
    D_END };
static const uint8_t sc_chapel[] = {
    RECT(0, 0, 255, 148, PNT(GRAPE, 0)), RECT(0, 148, 255, 255, WOOD(2)), RECT(0, 148, 255, 3, WOOD(4)),
    CIRC(128, 108, 26, PAL_BLACK),
    CIRC(139, 108, 8, PNT(ROSE, 1)), CIRC(133, 122, 8, PNT(LEMON, 1)), CIRC(123, 122, 8, PNT(SKY, 1)),
    CIRC(117, 108, 8, PNT(ROSE, 1)), CIRC(123, 94, 8, PNT(LEMON, 1)), CIRC(133, 94, 8, PNT(SKY, 1)),
    CIRC(128, 108, 6, PNT(MINT, 2)),
    RECT(190, 88, 54, 6, WOOD(1)),
    D_END };
static const uint8_t sc_board[] = {
    RECT(0, 0, 255, 255, PAL_RAMP(RAMP_STONE, 2)),
    REP(16, 0, 16), RECT(0, 8, 255, 1, PAL_RAMP(RAMP_STONE, 1)),
    RRECT(6, 24, 244, 200, WOOD(2)), RECT(11, 31, 234, 186, PNT(FOREST, 0)),
    RECT(40, 64, 30, 3, PNT(FOREST, 1)), RECT(150, 94, 40, 4, PNT(FOREST, 1)), RECT(70, 154, 36, 3, PNT(FOREST, 1)),
    RECT(186, 176, 30, 3, PNT(FOREST, 1)), RECT(22, 196, 44, 3, PNT(FOREST, 1)),
    RECT(11, 221, 234, 5, WOOD(1)),
    D_END };
static const uint8_t sc_kitchen[] = {
    RECT(0, 0, 255, 158, PAL_RAMP(RAMP_PEACH, 5)),
    REP(21, 12, 0), RECT(12, 0, 1, 158, PAL_RAMP(RAMP_PEACH, 4)),
    REP(12, 0, 13), RECT(0, 13, 255, 1, PAL_RAMP(RAMP_PEACH, 4)),
    RRECT(73, 34, 110, 21, PNT(BLOOD, 1)),
    RECT(0, 158, 255, 255, WOOD(3)), RECT(0, 158, 255, 3, WOOD(5)),
    RRECT(43, 161, 68, 22, PAL_RAMP(RAMP_STEEL, 2)), RRECT(145, 161, 68, 22, PAL_RAMP(RAMP_STEEL, 2)),
    RECT(30, 147, 19, 6, PNT(JET, 0)), RECT(207, 147, 19, 6, PNT(JET, 0)),
    ELL(77, 150, 30, 13, PNT(JET, 1)), ELL(179, 150, 30, 13, PNT(JET, 1)),
    REP(3, 14, 0), ELL(63, 150, 5, 5, PNT(JET, 0)), REP(3, 14, 0), ELL(165, 150, 5, 5, PNT(JET, 0)),
    D_END };
static const uint8_t sc_gate[] = {
    RECT(0, 0, 255, 86, PAL_RAMP(RAMP_PASTEL_PINK, 6)), RECT(0, 86, 255, 86, PAL_RAMP(RAMP_PEACH, 6)),
    RECT(0, 172, 255, 255, PAL_RAMP(RAMP_PEACH, 5)),
    RECT(181, 48, 13, 255, PNT(BLOOD, 0)), RECT(231, 48, 13, 255, PNT(BLOOD, 0)), RECT(181, 48, 63, 16, PNT(BLOOD, 1)),
    REP(5, 8, 0), RECT(197, 64, 2, 255, PNT(JET, 1)),
    RECT(0, 227, 255, 255, PAL_RAMP(RAMP_STONE, 3)),
    D_END };

/* ---- STEER: the Fun Fair bubbles -------------------------------------------- */
static void pop_update(void)
{
    MgB *g = G;
    int k, dx, dy, r, x, live = 0;
    pet_move(32);
    for (k = 0; k < 12; k++) {
        Obj *o = &g->o[k];
        if (!o->kind) continue;
        live++;
        o->age++;
        o->x = (int16_t)(o->x + o->vx);
        o->y = (int16_t)(o->y + o->vy);
        x = (o->x >> 4) + ui_wave(o->age + k * 11, 48, 3);
        if (o->y < -12 * 16 || x > plat_screen_w() + 12) { o->kind = 0; continue; }
        dx = x - (g->px >> 4);
        dy = (o->y >> 4) - (g->py >> 4);
        r = o->r + 7;
        if (o->age > 8 && dx * dx + dy * dy < r * r) {
            fx(x, o->y >> 4, o->kind);
            if (o->kind == 2) { g->bad++; g->shake = 16; audio_sfx(SFX_SPLAT); }
            else { g->n++; audio_sfx(SFX_BUBBLE); }
            o->kind = 0;
        }
    }
    if (g->t >= g->next && live < 12) {
        Obj *o = g->o;
        uint32_t h = rnd(5);
        while (o->kind) o++;
        o->kind = (uint8_t)(g->spawned == 0 || (h & 255) < 56 ? 2 : 1);   /* the first one is grape */
        o->x = (int16_t)(SX(52) << 4);
        o->y = (int16_t)(SY(172) << 4);
        o->vx = (int8_t)(5 + ((h >> 8) & 7));
        o->vy = (int8_t)-(7 + (int)((h >> 12) & 7));
        o->r = (uint8_t)(5 + ((h >> 16) & 3) + SY(3));
        o->age = 0;
        g->spawned++;
        g->next = (uint16_t)(g->t + VB.pop_gap + ((h >> 20) & 7));
    }
    if (g->n >= VB.pop_goal) finish(1);
    else if (g->bad >= VB.pop_grape || g->t >= VB.pop_secs * 30) finish(0);
}

static void pop_draw(int sw, int sh)
{
    MgB *g = G;
    int k, R = SY(54);
    scenery(sc_fair);
    for (k = 0; k < 64; k += 2)                                /* the wheel's rim */
        plat_rect(SX(205) + cosw(k, R), SY(116) + sinw(k, R), 2, 2, PAL_RAMP(RAMP_STONE, 5));
    for (k = 0; k < 12; k++) {                                 /* the wheel's cabins, turning */
        int a = k * 16 / 3 + (int)g->t / 6;
        ui_round_rect(SX(205) + cosw(a, R) - 3, SY(116) + sinw(a, R) - 2, 7, 6, 2, PAL_PAINT(PAINT_CHERRY + (k % 3) * 8, 1));
    }
    ui_fill_circle(SX(30), SY(158), 3, PNT(VAPE_PURPLE, (g->t / 8) & 2));   /* the rig's lamp */
    eyes(SX(31), SY(130), 3, 2 + (g->shake != 0), 0);        /* Vex, a worried little cloud */
    for (k = 0; k < 12; k++) {
        Obj *o = &g->o[k];
        if (o->kind) bubble((o->x >> 4) + ui_wave(o->age + k * 11, 48, 3), o->y >> 4, o->r, o->kind == 2);
    }
    pet(g->px >> 4, g->py >> 4, 7 + SY(3), 0);
    burst(g->flag == 2 ? PNT(GRAPE, 1) : PAL_WHITE);
    if (g->shake) say(g->fx_x, g->fx_y - 18, "Residue.");
    lcd(6, SY(48), "%d/%d", g->n, VB.pop_goal);
    lives(VB.pop_grape - g->bad, VB.pop_grape);
    (void)sw;
    (void)sh;
}

/* ---- STEER: the fireflies ---------------------------------------------------- */
/* 16 points each on a 64 x 40 grid: a heart, two faces, the fountain */
static const uint8_t shape_pt[3][16][2] = {
    { {32,14},{33,11},{39,7},{47,8},{50,15},{47,24},{39,31},{33,37},{32,40},{31,37},{25,31},{17,24},{14,15},{17,8},{25,7},{31,11} },
    { {28,26},{18,32},{8,26},{8,14},{18,8},{28,14},{14,17},{22,17},{56,26},{46,32},{36,26},{36,14},{46,8},{56,14},{42,17},{50,17} },
    { {8,30},{16,32},{24,34},{32,35},{40,34},{48,32},{56,30},{32,26},{32,18},{32,10},{24,8},{17,11},{12,17},{40,8},{47,11},{52,17} },
};

/* the chosen shape's 16 points on screen (once per pick, not per frame) */
static void shape_place(void)
{
    MgB *g = G;
    int k, sw = plat_screen_w(), sh = plat_screen_h(), sc = DOOMO_MIN((sw - 48) / 4, (sh - 76) * 2 / 5);
    for (k = 0; k < 16; k++) {
        g->spt[k][0] = (int16_t)(sw / 2 + ((int)shape_pt[g->sel][k][0] - 32) * sc / 16);
        g->spt[k][1] = (int16_t)(28 + (sh - 76) / 2 + ((int)shape_pt[g->sel][k][1] - 20) * sc / 16);
    }
}

static void settle(int k, int s)
{
    MgB *g = G;
    g->o[k].kind = 2;
    g->o[k].x = (int16_t)(g->spt[s][0] << 4);
    g->o[k].y = (int16_t)(g->spt[s][1] << 4);
    g->slot[s] = (uint8_t)(k + 1);
    g->n++;
    fx(g->spt[s][0], g->spt[s][1], 0);
    audio_sfx(SFX_TINK);
}

static void fly_update(void)
{
    MgB *g = G;
    int k, s, x, y, dx, dy, sw = plat_screen_w(), sh = plat_screen_h(), R = SY(42);
    g->pt++;
    if (g->phase == 0) {                                       /* pick the shape: it shows in the sky */
        k = g->sel + axis(BTN_LEFT, BTN_RIGHT, 1);
        if (k != g->sel && k >= 0 && k <= 2) { g->sel = (uint8_t)k; shape_place(); audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A) || g->pt >= VB.pick_secs * 30) {
            audio_sfx(SFX_SELECT);
            g->phase = 1;
            g->pt = 0;
            for (k = 0; k < VB.fly_n; k++) {
                uint32_t h = rnd(k);
                g->o[k].kind = 1;
                g->o[k].x = (int16_t)((12 + (int)((h & 255) * (uint32_t)(sw - 24) >> 8)) << 4);
                g->o[k].y = (int16_t)((26 + (int)(((h >> 8) & 255) * (uint32_t)(sh - 70) >> 8)) << 4);
            }
        }
        return;
    }
    pet_move(24);
    for (k = 0; k < VB.fly_n; k++) {
        Obj *o = &g->o[k];
        uint32_t h = rnd(k);
        if (o->kind == 2) continue;
        x = o->x >> 4;
        y = o->y >> 4;
        dx = (g->px >> 4) - x;
        dy = (g->py >> 4) - y;
        o->aux = (uint8_t)(dx * dx + dy * dy < R * R);
        if (o->aux) {                                          /* lit: drawn to the glow, wobbling */
            o->vx = (int8_t)clampi(o->vx + (dx > 6) - (dx < -6) + (int)(h & 2) - 1, -14, 14);
            o->vy = (int8_t)clampi(o->vy + (dy > 6) - (dy < -6) + (int)(h & 4) / 2 - 1, -14, 14);
            for (s = 0; s < 16; s++) {
                dx = g->spt[s][0] - x;
                dy = g->spt[s][1] - y;
                if (!g->slot[s] && dx * dx + dy * dy < 64) { settle(k, s); break; }
            }
            if (s < 16) continue;
        } else if ((g->t + k * 5) % 20 == 0) {                 /* dark: wander */
            o->vx = (int8_t)((int)(h & 15) - 7);
            o->vy = (int8_t)((int)((h >> 4) & 15) - 7);
        }
        x = o->x + o->vx;
        y = o->y + o->vy;
        if (x != clampi(x, 6 * 16, (sw - 6) * 16)) o->vx = (int8_t)-o->vx;
        if (y != clampi(y, 22 * 16, (sh - 34) * 16)) o->vy = (int8_t)-o->vy;
        o->x = (int16_t)clampi(x, 6 * 16, (sw - 6) * 16);
        o->y = (int16_t)clampi(y, 22 * 16, (sh - 34) * 16);
    }
    if (g->pt >= VB.fly_secs * 30 && (g->pt & 7) == 0) {      /* Glowbug lends a hand */
        for (k = 0; k < VB.fly_n && g->o[k].kind == 2; k++) ;
        for (s = 0; s < 16 && g->slot[s]; s++) ;
        if (k < VB.fly_n && s < 16) settle(k, s);
    }
    if (g->n >= 16) finish(1 + g->sel);
}

static void fly_draw(int sw, int sh)
{
    MgB *g = G;
    static const char *const shape_name[3] = { "< A heart >", "< Two faces >", "< The fountain >" };
    int k, x, y;
    scenery(sc_night);
    for (k = 0; k < 28; k++)
        plat_rect((k * 73 + 17) % sw, (k * 41 + 9) % (sh - 50) + 4, 1, 1, ((g->t / 8 + k) % 5) ? PAL_GRAY : PAL_WHITE);
    if (g->phase) {
        ui_fill_circle(g->px >> 4, g->py >> 4, SY(42), SKY(2));   /* the pet's glow */
        ui_fill_circle(g->px >> 4, g->py >> 4, SY(28), SKY(3));
    }
    scenery(sc_park);
    for (k = 0; k < 16; k++)                                   /* the shape, dotted */
        if (!g->slot[k]) plat_rect(g->spt[k][0], g->spt[k][1], 2, 2, g->phase ? SKY(5) : PNT(LEMON, (g->t / 4 + k) & 2));
    if (g->phase == 0) {
        say(sw / 2, sh - 44, shape_name[g->sel]);
        return;
    }
    for (k = 0; k < VB.fly_n; k++) {
        Obj *o = &g->o[k];
        x = o->x >> 4;
        y = o->y >> 4;
        if (o->kind == 2 || o->aux) {
            plat_rect(x - 1, y, 4, 2, PNT(LEMON, 0));
            plat_rect(x, y - 1, 2, 4, PNT(LEMON, 0));
            plat_rect(x, y, 2, 2, PNT(LEMON, 2));
        } else if ((g->t + k * 7) % 24 < 16) {
            plat_rect(x, y, 2, 2, PNT(CHARTREUSE, 1));
        }
    }
    pet(g->px >> 4, g->py >> 4, 6 + SY(3), 1);
    burst(PNT(LEMON, 2));
    lcd(6, SY(48), "%d/%d", g->n, 16);
}

/* ---- BEAT: one judge, three games ------------------------------------------- */
/* the Unity Stage's bubbles in half-beats (two quick doubles) */
static const uint8_t stage_at[16] = { 0, 2, 4, 6, 8, 10, 11, 12, 14, 16, 18, 19, 20, 22, 24, 26 };
/* the duet: which burner each ingredient lands on (0 left, 1 right) */
static const uint8_t duet_side[15] = { 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 };
/* events, goal, first event's frame per game - one 9 B table, not three of
 * 3 B: the PS1 keeps every object of 8 B or less in its resident .sdata
 * (gp-relative), whatever overlay group the code is in */
static const uint8_t ev_tab[3][3] = { { 16, 16, 15 }, { 10, 11, 7 }, { 60, 40, 54 } };
#define ev_n    ev_tab[0]
#define ev_goal ev_tab[1]
#define ev_o0   ev_tab[2]
#define EV(e) ((e) == E_STAGE ? 0 : (e) == E_NAP ? 1 : 2)

/* the frame event k is due (the duet: the ball is ready to flip) */
static int ev_t(int k)
{
    int e = G->eng;
    return ev_o0[EV(e)] + (e == E_STAGE ? stage_at[k] * VB.beat / 2 : k * 2 * VB.beat + (e == E_DUET ? VB.beat : 0));
}

static int pan_x(int side) { return SX(side ? 179 : 77); }

static void beat_update(void)
{
    MgB *g = G;
    int e = EV(g->eng), k = g->cur, n = ev_n[e], o0 = ev_o0[e], d, in, duet = e == 2;
    if (g->t >= o0 && (g->t - o0) % VB.beat == 0) audio_sfx(SFX_TIK);
    if (e == 1) g->meter = (uint8_t)clampi((int)g->meter + (btn_down(BTN_A) ? 2 : -2), 0, 30);
    for (d = 0; d < n; d++) if (g->o[d].kind && g->o[d].age < 250) g->o[d].age++;
    if (k >= n) {
        if ((int)g->t > ev_t(n - 1) + 30) finish(g->n >= ev_goal[e]);
        return;
    }
    if (duet && k % 3 == 2) {                                  /* never lands: he ate it */
        if ((int)g->t >= ev_t(k) - VB.beat - 8) {
            g->o[k].kind = 3;
            fx(pan_x(duet_side[k]), SY(100), 3);
            audio_sfx(SFX_EAT);
            g->cur++;
        }
        return;
    }
    d = (int)g->t - ev_t(k);
    if (e == 0) in = btn_pressed(BTN_A) != 0;
    else if (e == 1) in = ((k & 1) ? btn_released(BTN_A) : btn_pressed(BTN_A)) != 0;
    else {                                                     /* the burner pressed: 1 right, -1 wrong */
        in = axis(BTN_LEFT | BTN_B | BTN_L, BTN_RIGHT | BTN_A | BTN_R, 1);
        in = in ? (in > 0) == duet_side[k] ? 1 : -1 : 0;
    }
    if (in > 0 && d >= -VB.win && d <= VB.win) {
        g->o[k].kind = 1;
        g->n++;
        g->cur++;
        if (duet) fx(pan_x(duet_side[k]), SY(150), 1);
        else fx(g->o[k].x, g->o[k].y, 1);
        audio_sfx(e == 0 ? SFX_BUBBLE : e == 1 ? SFX_BLIP : SFX_BOING);
    } else if (d > VB.win || (in && duet)) {                   /* late; the duet: a pan with nothing ready */
        if (d > VB.win) { g->o[k].kind = 2; g->cur++; }
        g->bad++;
        g->shake = 20;
        if (e) audio_sfx(duet ? SFX_SPLAT : SFX_SQUEAK);
        if (duet && g->bad >= VB.duet_miss) finish(0);
    }
}

static void stage_draw(int sw, int sh)
{
    MgB *g = G;
    int k, x, y, ph = g->t >= 60 ? (int)((g->t - 60) % VB.beat) : 9, jump = g->fx_t > 4;
    scenery(sc_stage);
    ui_text_center(sw / 2, SY(37), "UNITY STAGE", PAL_UI_GOLD);
    for (k = g->cur; k < VB.stage_n && k < g->cur + 5; k++) {
        Obj *o = &g->o[k];
        int d = ev_t(k) - (int)g->t;
        if (d > 2 * VB.beat) continue;
        bubble(o->x, o->y, o->r, 0);
        if (d > -VB.win) ring(o->x, o->y, o->r + DOOMO_MAX(d, 0) * o->r / VB.beat + 2, d <= VB.win ? PAL_UI_GOLD : PAL_WHITE);
    }
    for (k = 0; k < VB.stage_n; k++)
        if (g->o[k].kind == 2 && g->o[k].age < 16) say(g->o[k].x, g->o[k].y - g->o[k].age, "oop");
    burst(PAL_UI_GOLD);
    for (k = 0; k < 2 * (sw / 13 + 2); k++) {                  /* the crowd: both sides, mixed */
        x = (k >> 1) * 13 + (k & 1) * 6;
        y = sh - 18 + (k & 1) * 10 - (ph < 4 && (k & 2) ? 2 : 0) - (jump && (k & 2) ? 4 : 0);
        ui_fill_circle(x, y, 6, (k % 3) ? PNT(BUBBLEGUM, 1) : PNT(CHERRY, 1));
    }
    lcd(6, SY(70), "%d/%d", g->n, VB.stage_n);
}

/* the nap-off ------------------------------------------------------------------ */
static void nap_draw(int sw, int sh)
{
    MgB *g = G;
    int k, x, y, fy = SY(148), ty = SY(61), c = 4 * VB.beat, p = g->t < 40 ? 0 : (g->t - 40) % c;
    int ib = (p < c / 2 ? p : c - p) * 60 / c;                 /* the perfect breath, 0..30 */
    scenery(sc_chapel);
    /* the breathing guide: IN (hold) and OUT (let go) sliding to the line */
    plat_rect(8, ty, sw - 16, 12, PAL_BLACK);
    for (k = g->cur > 0 ? g->cur - 1 : 0; k < VB.nap_n; k++) {
        int x0 = 40 + ev_t(k) - (int)g->t, x1 = DOOMO_MIN(x0 + 2 * VB.beat, sw - 10);
        if (x0 > sw - 10) break;
        x0 = DOOMO_MAX(x0, 9);
        if (x1 <= x0) continue;
        plat_rect(x0, ty + 1, x1 - x0 - 1, 10, (k & 1) ? PNT(MINT, 1) : PNT(ROSE, 1));
        if (x1 - x0 > 22) ui_text(x0 + 3, ty + 2, (k & 1) ? "OUT" : "IN", PAL_WHITE);
    }
    plat_rect(39, ty - 3, 2, 18, PAL_UI_GOLD);
    /* the sleepers: Dozy Doe, then the kids (they breathe with you; the one
     * the last miss woke sits up) */
    for (k = 0; k < 5; k++) {
        int awake = k && g->shake && k == 1 + g->bad % 4, br = k ? g->meter : ib;
        x = k ? 64 + (k - 1) * (sw - 128) / 3 : 26;
        y = fy + 8 + (k ? 10 + (k & 1) * 6 : 0);
        ui_round_rect(x - 14, y, 28, 10, 3, PNT(LAVENDER, 2));
        y -= awake * 6;
        ui_fill_circle(x - 8, y, 5, k == 0 ? PNT(CARAMEL, 2) : (k & 1) ? PNT(PEACH, 2) : PNT(CHERRY, 1));
        if (k && !(k & 1)) plat_rect(x - 12, y - 6, 9, 2, PNT(BONE, 1));   /* Hellvile kids have nubs */
        eyes(x - 8, y - 1, 2, 1, !awake);
        ui_fill_ellipse(x + 3, y + 3 + awake * 6, 10, 3 + br / 8, PNT(SKY, 2));   /* the blanket breathes */
    }
    ui_text(10, fy + 21, "DOE", PAL_GRAY_L);
    k = SY(94);                                                                  /* Betty, upside down */
    ui_fill_ellipse(SX(217), k + 10, 7, 10 + ib / 10, PNT(CHARCOAL, 1));
    ui_fill_ellipse(SX(217) - 8, k + 6, 3, 8, PNT(CHARCOAL, 0));              /* her folded wings */
    ui_fill_ellipse(SX(217) + 8, k + 6, 3, 8, PNT(CHARCOAL, 0));
    ui_fill_circle(SX(217), k + 25, 5, PNT(CHARCOAL, 2));
    ui_text(SX(198), fy + 4, "BETTY", PAL_GRAY_L);
    if (!story_flag(SF_GRUMBLE_DEAD)) {                        /* the gatekeeper, three mats deep */
        int gx = sw * 3 / 4, gy = sh - 24, sn = ui_wave(g->t, 52, 3);
        ui_fill_ellipse(gx, gy, 30, 10 + sn, PNT(CHERRY, 1));
        ui_round_rect(gx - 26, gy - 6 - sn, 52, 8, 3, PNT(MINT, 2));
        plat_rect(gx - 32, gy - 12, 4, 6, PNT(JET, 1));       /* horn tips */
        if ((g->t / 26) & 1) ui_text(gx + 18, gy - 26 - (int)(g->t % 26) / 2, "Z", PNT(HELLFIRE, 1));
    }
    if (g->fx_t) ui_text(sw / 2, fy - 4 - (12 - g->fx_t) * 2, "z Z", PAL_WHITE);
    ui_bar(8, ty + 16, 60, 5, g->n, VB.nap_goal, PNT(LEMON, 2));
}

/* the duet ---------------------------------------------------------------------- */
static void duet_draw(int sw, int sh)
{
    MgB *g = G;
    int k, py = SY(150), tick = g->t >= 54 && (g->t - 54) % VB.beat < 4;
    scenery(sc_kitchen);
    ui_text_center(sw / 2, SY(38), "BRIMSTONE BITES", PAL_UI_GOLD);
    plat_rect(10, SY(130) - tick * 6, 2, 16, PNT(BONE, 2));   /* Taco Yaki's toothpick */
    for (k = 0; k < 6; k++)                                    /* the flames */
        ui_fill_ellipse(pan_x(k / 3) + (k % 3 - 1) * 12, py + 9 + ((g->t + k) & 2), 4, 2,
                        PNT(LEMON + ((g->t / 3 + k) & 1) * 18, 1));
    ui_text(pan_x(0) - 3, SY(166), STR_ICON_B, PAL_WHITE);
    ui_text(pan_x(1) - 3, SY(166), STR_ICON_A, PAL_WHITE);
    for (k = 0; k < VB.duet_n; k++) {
        Obj *o = &g->o[k];
        int ready = ev_t(k), x = pan_x(duet_side[k]), y, d = ready - (int)g->t, age = -d;
        if (d > 2 * VB.beat) break;
        if (o->kind == 3) {                                    /* the mitt from upstage right */
            age += VB.beat + 8;
            if (age < 14) {
                int hx = x + (sw - x) * (14 - age) / 14;
                ui_fill_circle((hx + sw) / 2, py - 40, 6, PNT(CHERRY, 1));
                ui_fill_circle(hx, py - 34, 8, PNT(CHERRY, 2));
            }
            continue;
        }
        if (o->kind == 1) {                                    /* flipped: arcs to the pet */
            if (age >= 20) continue;
            x += (sw / 2 - x) * age / 20;
            y = py - age * (20 - age) / 2 + (sh - 26 - py) * age / 20;
        } else if (o->kind == 2) {                             /* burnt */
            if (age > 24) continue;
            ui_fill_circle(x, py - 2, 5, PNT(CHARCOAL, 0));
            say(x, py - 22 - age / 2, "Burnt!");
            continue;
        } else {
            y = d > VB.beat ? SY(40) + (py - SY(40)) * (2 * VB.beat - d) / VB.beat : py - 2;
            if (d <= VB.beat) ring(x, py - 2, 6 + DOOMO_MAX(d, 0) / 2, d <= VB.win ? PAL_UI_GOLD : PAL_WHITE);
        }
        ui_fill_circle(x, y, 5, PNT(CARAMEL, 2));
        plat_rect(x - 3, y - 3, 2, 2, PAL_WHITE);
    }
    pet(sw / 2, sh - 20, 7 + SY(3), 0);
    if (g->fx_t && g->flag == 3) say(g->fx_x, g->fx_y, "GULP!");
    else burst(PNT(CARAMEL, 2));
    lcd(6, SY(70), "%d/%d", g->n, VB.duet_goal);
    lives(VB.duet_miss - g->bad, VB.duet_miss);
}

/* ---- CHALK: Clinker's board ------------------------------------------------- */
/* three lines of three slots; each slot has four words, one of them death */
static const char *const poem_word[9][4] = {
    { "The moon", "My gate", "A kettle", "The grave" },
    { "is", "hums", "rots", "waits" },
    { "so round", "with bones", "all night", "for tea" },
    { "A skull", "A small bell", "The pump", "Your hand" },
    { "sings", "counts", "shines", "dies" },
    { "at noon", "for you", "in dust", "like rain" },
    { "I keep", "We count", "I bury", "You chalk" },
    { "the dead", "a day", "the hours", "one more" },
    { "on walls.", "in tens.", "and smile.", "in a tomb." },
};
static const uint8_t poem_death[9] = { 3, 2, 1, 0, 3, 2, 2, 0, 3 };

static int line_dead(int l)
{
    const uint8_t *w = G->word + l * 3, *d = poem_death + l * 3;
    return w[0] == d[0] || w[1] == d[1] || w[2] == d[2];
}

static void poem_update(void)
{
    MgB *g = G;
    int c = g->cur, a = axis(BTN_UP, BTN_DOWN, 1);
    if (g->shake) g->shake--;
    if (g->fx_t) g->fx_t--;
    if (a) {
        g->word[c] = (uint8_t)((g->word[c] + a) & 3);
        g->fx_t = 6;
        g->idle = 0;
        audio_sfx(SFX_CURSOR);
    } else if (btn_pressed(BTN_B) && c > 0) {
        g->cur--;
        g->idle = 0;
        audio_sfx(SFX_CANCEL);
    } else if (btn_pressed(BTN_A) || ++g->idle >= VB.poem_idle) {
        g->cur++;
        g->idle = 0;
        g->shake = 10;
        audio_sfx(SFX_SHWIP);
        if (g->cur >= 9) finish(1 + line_dead(0) + line_dead(1) + line_dead(2));
    }
}

static void chalk(int x, int y, const char *s, uint8_t c)
{
    ui_text(x + 1, y + 1, s, PNT(FOREST, 1));
    ui_text(x, y, s, c);
}

static void poem_draw(int sw, int sh)
{
    MgB *g = G;
    int bx = SX(11), by = SY(31), lh = (SY(186) - 26) / 3, l, s, k, cx = bx + 24, cy = by;
    char num[4];
    scenery(sc_board);
    chalk(bx + 8, by + 5, "A POEM. THREE LINES.", PAL_GRAY_L);
    for (l = 0; l < 3; l++) {
        int y = by + 22 + l * lh, x = bx + 24;
        num[0] = (char)('1' + l);
        num[1] = '.';
        num[2] = 0;
        chalk(bx + 8, y, num, PAL_GRAY);
        plat_rect(bx + 22, y + 10, sw - 2 * bx - 30, 1, PNT(FOREST, 1));
        for (s = 0; s < 3; s++) {
            int i = l * 3 + s, w, now = i == g->cur && !g->done;
            const char *word = poem_word[i][g->word[i]];
            if (i > g->cur) break;
            w = ui_text_width(word);
            chalk(x + now * (g->fx_t & 2), y, word, now ? PAL_UI_GOLD : PAL_WHITE);
            if (now) {
                if ((g->t / 8) & 1) plat_rect(x, y + 9, w, 1, PAL_UI_GOLD);
                ui_text(x + w / 2 - 2, y - 8, "^", PAL_GRAY_L);
                ui_text(x + w / 2 - 2, y + 11, "v", PAL_GRAY_L);
                cx = x + w + 2;
                cy = y;
            }
            x += w + 6;
        }
        if (g->cur > l * 3 + 2 && line_dead(l)) {             /* a little skull doodle */
            ui_fill_circle(x + 4, y + 3, 4, PAL_WHITE);
            ui_text(x + 1, y, STR_ICON_SKULL, PNT(FOREST, 0));
        }
    }
    plat_rect(cx, cy - 4, 9, 3, PAL_WHITE);                    /* the chalk, and its dust */
    for (k = 0; k < 4 && g->shake; k++)
        plat_rect(cx + k * 3 - 2, cy + (10 - g->shake) * 2 + k, 1, 1, PAL_GRAY_L);
    if (g->done) chalk(sw - bx - 48, by + SY(186) - 12, "- you", PAL_GRAY_L);
    (void)sh;
}

/* ---- SQUEEZE: the hug line --------------------------------------------------- */
enum { Z_GENTLER, Z_GENTLE, Z_FIRM, Z_ALL };
static const uint16_t hug_id[8] = { 855, 42, 1, 666, 77, 13, 111, ROSTER_ID_GRUMBLEGUTS };
/* each hugger's zone, then the zones' meter bounds: one 16 B table (out of
 * the PS1's resident .sdata, see ev_tab) */
static const uint8_t hug_tab[16] = {
    Z_FIRM, Z_GENTLER, Z_GENTLE, Z_GENTLE, Z_GENTLE, Z_FIRM, Z_GENTLER, Z_ALL,
    12, 32, 56, 88,                                       /* zone_lo */
    28, 52, 76, 100 };                                    /* zone_hi */
#define hug_zone hug_tab
#define zone_lo  (hug_tab + 8)
#define zone_hi  (hug_tab + 12)
static const char *const zone_name[4] = { "GENTLER", "GENTLE", "FIRM", "ALL THE WAY" };
static const char *const hug_say[4] = { "Too soft...", "Aww!", "Too tight!", "A hug anyway." };

static void hug_enter(void)
{
    MgB *g = G;
    int k;
    for (k = 0; k < 8; k++) {                                  /* only the living queue */
        const DoomoResident *r = town_res(hug_id[k]);
        if (!r || r->status != RES_ALIVE || (k == 7 && story_flag(SF_GRUMBLE_DEAD))) continue;
        g->o[g->cnt++].aux = (uint8_t)k;
    }
}

static int hugger(void) { return G->cur < G->cnt ? G->o[G->cur].aux : -1; }

static void judge(int r)
{
    MgB *g = G;
    int h = hugger();
    g->flag = (uint8_t)r;
    g->phase = 2;
    g->pt = 0;
    if (r == 1) {
        g->n++;
        audio_sfx(h == 0 ? SFX_SQUEAK : h == 7 ? SFX_LOVE : SFX_HAPPY);
    } else {
        g->bad++;
        audio_sfx(r == 2 ? SFX_SQUEAK : SFX_CANCEL);
        if (g->bad >= VB.hug_miss) finish(0);
    }
}

static void hug_update(void)
{
    MgB *g = G;
    int h = hugger(), z;
    if (h < 0) { finish(1); return; }
    z = hug_zone[h];
    g->pt++;
    if (g->phase == 0) {
        if (g->pt == 1 && h == 7) audio_sfx(SFX_SCRATCH);      /* he backs in, rear first */
        if (g->pt >= VB.walk + (h == 7) * 20) { g->phase = 1; g->pt = 0; g->meter = 0; g->idle = 0; }
    } else if (g->phase == 1) {
        if (btn_down(BTN_A)) {
            g->held = 1;
            g->idle = 0;
            g->meter = (uint8_t)DOOMO_MIN(g->meter + 2, 100);
            if (g->meter >= 100) judge(z == Z_ALL ? 1 : 2);
        } else if (g->held) {
            judge(g->meter < zone_lo[z] ? 0 : g->meter <= zone_hi[z] ? 1 : 2);
        } else if (++g->idle >= VB.hug_idle) {
            judge(3);                                          /* they hug you anyway */
        }
    } else if (g->pt >= 36) {
        g->held = 0;
        g->pt = 0;
        g->meter = 0;
        if (g->flag & 1) { g->cur++; g->phase = 0; }           /* a good hug, or a hug anyway */
        else g->phase = 1;
    }
}

/* a citizen in the line: a chibi in their side's colour (horns south of the
 * Wall); Grumbleguts big, cherry red, crop top and shorts, rear view while
 * he backs in */
static void body(int x, int gy, int k, int rear, int bob)
{
    int id = hug_id[k], s = hug_zone[k] == Z_GENTLER ? 7 : hug_zone[k] == Z_FIRM ? 12 : 9, y;
    uint8_t skin = k == 0 ? PNT(CHARCOAL, 2) : id >= 500 ? PNT(CHERRY, 1) : PNT(PEACH, 2);
    if (k == 7) {
        y = gy - 22 - bob;
        plat_rect(x - 11, y - 25, 4, 6, PNT(JET, 1));         /* horns */
        plat_rect(x + 7, y - 25, 4, 6, PNT(JET, 1));
        ui_fill_circle(x, y - 12, 10, skin);
        ui_fill_circle(x, y + 6, 18, skin);
        plat_rect(x - 14, y - 6, 28, 5, PNT(JET, 1));         /* the crop top */
        ui_round_rect(x - 17, y + 10, 34, 11, 4, PNT(JET, 1)); /* the shorts */
        if (rear) ui_fill_circle(x + 20, y + 14, 2, skin);     /* the tail's spike */
        else {
            eyes(x, y - 13, 4, 2, 0);
            ui_fill_ellipse(x, y - 7, 5, 2, PAL_BLACK);
        }
        return;
    }
    y = gy - 2 * s - s / 2 - bob;
    ui_fill_ellipse(x, gy - s - bob, s, s, skin);
    ui_fill_circle(x, y, s * 3 / 4 + 2, skin);
    if (id >= 500) {
        plat_rect(x - s / 2 - 1, y - s / 2 - 2, 2, 3, PNT(BONE, 1));
        plat_rect(x + s / 2, y - s / 2 - 2, 2, 3, PNT(BONE, 1));
    }
    eyes(x - 1, y, 3, 1, 0);
}

static void hug_draw(int sw, int sh)
{
    MgB *g = G;
    int h = hugger(), gy = SY(227), hx = SX(92), hr = SY(29), k, z = h >= 0 ? hug_zone[h] : 0;
    int gh = SY(112), gt = SY(64), wrap = g->phase == 2 && g->flag == 1, px = sw - 58 - SX(4);
    scenery(sc_gate);
    for (k = g->cur + 1; k < g->cnt && k < g->cur + 4; k++)   /* the line, round the corner */
        body(sw - 20 - (k - g->cur) * 14, gy, g->o[k].aux, 0, ui_wave((int)g->t + k * 9, 30, 1));
    if (h >= 0) {
        int walk = VB.walk + (h == 7) * 20, gx = hx + hr + 12;
        body(g->phase ? gx - wrap * 6 : gx + (sw + 20 - gx) * (walk - g->pt) / walk, gy, h,
             h == 7 && !g->phase, g->phase ? 0 : (g->pt & 4) / 2);
        ui_portrait(px, SY(28), hug_id[h], 1, wrap ? (h == 7 ? EXPR_LOVE : EXPR_HAPPY) : EXPR_NEUTRAL);
        say(px + 27, SY(28) + 66, zone_name[z]);
    }
    /* Hugsley: a mint plush with button eyes and a heart patch; arms close in */
    k = wrap ? hr + 2 : g->held ? hr + 4 + (100 - g->meter) / 12 : hr + 8;
    ui_fill_ellipse(hx - k, gy - hr, 6, 10, PNT(MINT, 1));
    ui_fill_ellipse(hx + k, gy - hr, 6, 10, PNT(MINT, 1));
    ui_fill_ellipse(hx, gy - hr, hr + wrap * 2, hr - wrap * 2, PNT(MINT, 2));
    eyes(hx, gy - hr - 6, 6, 3, 0);
    ui_heart(hx, gy - hr + 8, 4, PNT(ROSE, 1));
    /* the gauge: the zone in mint, the squeeze in pink (red past the zone) */
    plat_rect(10, gt, 12, gh, PAL_BLACK);
    plat_rect(11, gt + 1 + gh * (100 - zone_hi[z]) / 100, 10, gh * (zone_hi[z] - zone_lo[z]) / 100, PNT(MINT, 1));
    k = gh * g->meter / 100;
    plat_rect(13, gt + gh - k, 6, k, g->meter > zone_hi[z] ? PAL_UI_RED : PNT(BUBBLEGUM, 2));
    lcd(28, SY(48), "HUGS %04d", 1000 - g->cnt + g->n, 0);
    if (g->phase == 2 && h >= 0) {
        say(hx, gy - 2 * hr - 22, wrap && h == 7 ? "grrrbl." : wrap && h == 0 ? "...Again." : hug_say[g->flag & 3]);
        for (k = 0; wrap && k < 4; k++) ui_heart(hx - 12 + k * 8, gy - 2 * hr - 4 - (int)g->pt + k * 3, 3, PNT(ROSE, 1));
    }
    (void)sh;
}

/* ---- the registry rows ------------------------------------------------------ */
static void enter(int id)
{
    MgB *g = (MgB *)scene_scratch(MGB_TAG, (uint32_t)sizeof(MgB));
    int k;
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->id = (uint16_t)id;
    g->eng = (uint8_t)clampi(id - SMG_B_BUBBLES, E_POP, E_HUG);
    g->px = (int16_t)((plat_screen_w() / 2) << 4);
    g->py = (int16_t)((plat_screen_h() - 46) << 4);
    g->next = 20;
    if (g->eng == E_FLY) shape_place();
    if (g->eng == E_HUG) hug_enter();
    if (g->eng == E_STAGE)                                     /* the stage bubbles' places */
        for (k = 0; k < VB.stage_n; k++) {
            uint32_t h = story_rng(0x5B00u + (uint32_t)k);
            g->o[k].x = (int16_t)(SX(36) + (int)((h & 255) * (uint32_t)SX(184) >> 8));
            g->o[k].y = (int16_t)(SY(84) + (int)(((h >> 8) & 255) * (uint32_t)SY(70) >> 8));
            g->o[k].r = (uint8_t)(8 + SY(3));
        }
}

static int update(void)
{
    MgB *g = G;
    if (!scene_scratch_mine(MGB_TAG)) return 1 + 1;           /* the scratch is gone: count it as played */
    g->t++;
    if (g->done) return ++g->end_t >= VB.end ? 1 + g->result : 0;
    if (g->eng != E_POEM) {
        if (g->fx_t) g->fx_t--;
        if (g->shake) g->shake--;
    }
    switch (g->eng) {
    case E_POP: pop_update(); break;
    case E_FLY: fly_update(); break;
    case E_POEM: poem_update(); break;
    case E_HUG: hug_update(); break;
    default: beat_update(); break;
    }
    return 0;
}

static void draw(void)
{
    static const char *const title[7] = { "Bubble Test", "Unity Stage", "Firefly Night", "The Nap-Off", NULL,
                                          "Line Cook", "The Hug Line" };
    static const char *const hint[7] = {
        "D-pad: pop the clear ones!", STR_ICON_A " Pop it when the ring closes",
        "D-pad: lead them into the shape", "Hold " STR_ICON_A ": in.  Let go: out.",
        "^v Word  " STR_ICON_A " Write  " STR_ICON_B " Rub out", STR_ICON_B " / " STR_ICON_A " Flip that pan!",
        "Hold " STR_ICON_A " to hug. Let go in the zone!" };
    /* [9], not [7]: over 8 B it leaves the PS1's resident .sdata (ev_tab) */
    static const uint8_t style[9] = { WIN_MINT, WIN_CUTE, WIN_DARK, WIN_DARK, WIN_CREAM, WIN_HELL, WIN_CUTE };
    static void (*const draws[7])(int, int) = { pop_draw, stage_draw, fly_draw, nap_draw, poem_draw, duet_draw, hug_draw };
    MgB *g = G;
    int sw = plat_screen_w(), sh = plat_screen_h(), e;
    const char *h;
    if (!scene_scratch_mine(MGB_TAG)) { ui_backdrop(WIN_CUTE, 0); return; }
    e = g->eng;
    h = hint[e];
    draws[e](sw, sh);
    if (e == E_FLY) h = g->phase == 0 ? "< > Pick a shape  " STR_ICON_A " Choose" : g->pt >= VB.fly_secs * 30 ? "Glowbug lends a hand!" : h;
    if (title[e]) ui_title(sw / 2, SY(8), e == E_POP && g->t < 150 ? "Not the grape ones!" : title[e], style[e]);
    ui_hint(h, style[e]);
    if (g->done && g->result)
        ui_sparkle(sw / 2 + ui_wave(g->end_t * 4, 40, 30), sh / 2 - 20, g->end_t & 15, PAL_UI_GOLD);
}

static const StoryMgEntry rows[] = {
    { SMG_B_BUBBLES, "Bubble Test", enter, update, draw },
    { SMG_B_BUBBLES_STAGE, "The Unity Stage", enter, update, draw },
    { SMG_B_FIREFLY, "Firefly Night", enter, update, draw },
    { SMG_B_NAPOFF, "The Nap-Off", enter, update, draw },
    { SMG_B_POEM, "Clinker's Board", enter, update, draw },
    { SMG_B_DUET, "Line Cook", enter, update, draw },
    { SMG_B_HUGS, "Free Hugs", enter, update, draw },
    { 0, NULL, NULL, NULL, NULL },
};
const StoryMgEntry *const doomo_story_mg_b = rows;
