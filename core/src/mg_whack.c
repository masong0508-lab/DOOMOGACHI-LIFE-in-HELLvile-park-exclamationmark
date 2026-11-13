/* mg_whack.c - WHACK-AN-IMP (owner: minigames).
 *
 * Nine candy-dirt holes in three mowed rows. Imps pop up to blow
 * raspberries; bonk them with the squeaky lollipop mallet. Aim with the
 * D-pad like a numpad (UP+LEFT is the top-left hole, nothing held is the
 * middle one) and press A. Golden imps are worth a lot, sneaky vape
 * wraiths need a double tap (the first bonk only makes them cough), and
 * Father Fuzz pops up to bless the field: do NOT bonk the priest (it costs
 * a heart). Never bonking him in a whole round earns a trophy.
 */
#include "mg.h"

enum { C_NONE, C_IMP, C_GOLD, C_WRAITH, C_FUZZ };
enum { H_DOWN, H_RISE, H_UP, H_SINK, H_BONK };

typedef struct WHole {
    uint8_t kind, st, t, hits, stay, dazed;
} WHole;

#define WK_FL_PUTS 80
#define WK_SIGN_PUTS 32
#define WK_RING_PUTS 24
typedef struct Whack {
    WHole hole[9];
    uint8_t aim, swing, cool, hit_hole, fuzz_bonks, misses;
    int16_t mx16, my16;         /* mallet position (smoothed) */
    uint16_t spawn_t;
    /* (S2-4b) made once a round: the rows of a hole's front ring (outer and inner half widths), and,
     * for the frames over the kept picture, the boxes to put back (mg.h "undo"): where a hole lies on
     * a flower, and where the sign board lies on each of its two imps */
    uint8_t front[2][12];
    int8_t puts_ok, n_fl, n_sign[2], n_ring;
    MgBox fl_put[WK_FL_PUTS], sign_put[2][WK_SIGN_PUTS], ring_put[WK_RING_PUTS];   /* (ring_put: hole 0's front ring) */
    struct { int16_t x, y; uint8_t pink; } flower[14];      /* where the flowers stand (flower_at) */
} Whack;
MG_ARENA_FITS(Whack);
#define S (MG_ARENA(Whack))

static int field_top(void) { return mg_h() * 2 / 5 - (mg_big() ? 2 : 4); }
static int row_dy(void) { return (mg_h() - field_top() - (mg_big() ? 18 : 12)) / 3; }
static int col_dx(void) { return mg_w() / 4 + (mg_big() ? 4 : 2); }
static int crit_r(void) { return mg_big() ? 14 : 10; }
static int hole_x(int i) { return mg_w() / 2 + (i % 3 - 1) * col_dx(); }
static int hole_y(int i) { return field_top() + 6 + (i / 3) * row_dy() + row_dy() / 2; }
static int hole_rx(void) { return crit_r() + 5; }
static int hole_ry(void) { return hole_rx() / 3 + 1; }

static void front_rows(int rx, int ry, uint8_t *o, uint8_t *in);
static uint32_t flower_at(int k, int *fx2, int *fy);

static void wh_start(void)
{
    int k;
    front_rows(hole_rx(), hole_ry(), S->front[0], S->front[1]);
    for (k = 0; k < 14; k++) {
        int fx2, fy;
        S->flower[k].pink = (uint8_t)(flower_at(k, &fx2, &fy) >> 20 & 1);
        S->flower[k].x = (int16_t)fx2;
        S->flower[k].y = (int16_t)fy;
    }
    S->aim = 4;
    S->mx16 = (int16_t)(hole_x(4) * 16);
    S->my16 = (int16_t)(hole_y(4) * 16);
    S->spawn_t = 15;
}

static int rise_of(const WHole *h)
{
    int full = crit_r() * 2 + 2;
    switch (h->st) {
    case H_RISE: return full * h->t / 6;
    case H_UP: return full;
    case H_SINK: return full * (6 - h->t) / 6;
    case H_BONK: return h->t < 10 ? full * 2 / 3 : full * 2 / 3 * (16 - h->t) / 6;
    default: return 0;
    }
}

static void spawn(void)
{
    static const uint8_t maxup[MG_TIERS] = { 2, 3, 4 };
    static const uint8_t stay[MG_TIERS] = { 42, 30, 22 };
    int i, up = 0, free_[9], nf = 0, roll;
    WHole *h;
    for (i = 0; i < 9; i++) {
        if (S->hole[i].st != H_DOWN) up++;
        else free_[nf++] = i;
    }
    if (up >= maxup[mg.tier] + (mg.play_t > 600) || !nf) return;
    h = &S->hole[free_[mg_rand(nf)]];
    roll = mg_rand(100);
    h->kind = roll < 9 ? C_FUZZ : roll < 15 ? C_GOLD : roll < 15 + 12 + mg.tier * 6 ? C_WRAITH : C_IMP;
    h->st = H_RISE;
    h->t = 0;
    h->hits = 0;
    h->dazed = 0;
    {
        int s = stay[mg.tier] - mg.play_t / 120;
        if (h->kind == C_GOLD) s = s * 2 / 3;
        if (h->kind == C_WRAITH) s += 10;
        if (h->kind == C_FUZZ) s += 6;
        h->stay = (uint8_t)DOOMO_MAX(s, 12);
    }
    if (h->kind == C_FUZZ) audio_sfx(SFX_HOLY);
}

static int aim_from_pad(void)
{
    int c = 1, r = 1;
    if (mg_down(BTN_LEFT)) c = 0;
    else if (mg_down(BTN_RIGHT)) c = 2;
    if (mg_down(BTN_UP)) r = 0;
    else if (mg_down(BTN_DOWN)) r = 2;
    return r * 3 + c;
}

static void whack(int i)
{
    WHole *h = &S->hole[i];
    int x = hole_x(i), y = hole_y(i) - rise_of(h) / 2 - 4;
    S->hit_hole = (uint8_t)i;
    if (h->st == H_DOWN || h->st == H_BONK || (h->st == H_RISE && h->t < 2) || (h->st == H_SINK && h->t > 4)) {
        mg_burst(MGFX_CRUMB, x, hole_y(i), 4, 18, PAL_PAINT(PAINT_CHOCOLATE, 2));
        audio_sfx(SFX_BLIP);
        if (S->misses < 255) S->misses++;
        if (mg.combo >= 3) mg_combo_break();
        return;
    }
    mg_shake_add(3);
    mg_hitstop(2);
    mg_fx(MGFX_RING, x, y, 0, 0, PAL_WHITE, 8);
    switch (h->kind) {
    case C_FUZZ:
        h->st = H_BONK;
        h->t = 0;
        S->fuzz_bonks++;
        mg_popup(x, y - 16, "OW! Bless you anyway", 0, PAL_UI_GOLD);
        mg_add_raw(-50, x, y - 6);
        audio_sfx(SFX_SQUEAK);
        mg_lose_life(x, y);
        break;
    case C_WRAITH:
        if (!h->hits) {
            h->hits = 1;
            h->dazed = 10;
            h->stay = (uint8_t)DOOMO_MAX(h->stay, h->t + 18);
            mg_add_score(5, x, y - 8);
            mg_popup(x, y - 18, "*cough*", 0, PAL_RAMP(RAMP_VAPE, 6));
            mg_burst(MGFX_PUFF, x, y, 4, 16, PAL_RAMP(RAMP_VAPE, 6));
            audio_sfx(SFX_SQUEAK);
            break;
        }
        h->st = H_BONK;
        h->t = 0;
        mg_combo_hit(x, y);
        mg_add_score(30, x, y - 8);
        mg_burst(MGFX_PUFF, x, y, 6, 30, PAL_RAMP(RAMP_VAPE, 5));
        mg_burst(MGFX_STAR, x, y, 4, 30, PAL_UI_GOLD);
        audio_sfx(SFX_DEMON_PAIN);
        break;
    default:
        h->st = H_BONK;
        h->t = 0;
        mg_combo_hit(x, y);
        mg_add_score(h->kind == C_GOLD ? 50 : 10, x, y - 8);
        if (h->kind == C_GOLD) {
            mg_burst(MGFX_STAR, x, y, 10, 40, PAL_UI_GOLD);
            mg_popup(x, y - 20, "GOLDEN BONK!", 0, PAL_UI_GOLD);
            audio_sfx(SFX_SPARKLE);
        } else {
            mg_splat(x, y, 5);
            audio_sfx(SFX_SQUEAK);
        }
        break;
    }
}

static void wh_update(void)
{
    int i;
    static const uint8_t iv[MG_TIERS] = { 26, 18, 12 };
    S->aim = (uint8_t)aim_from_pad();
    {
        int tx = hole_x(S->aim) * 16, ty = hole_y(S->aim) * 16;
        S->mx16 = (int16_t)(S->mx16 + (tx - S->mx16) / 2);
        S->my16 = (int16_t)(S->my16 + (ty - S->my16) / 2);
    }
    if (S->swing) S->swing--;
    if (S->cool) S->cool--;
    if (mg_pressed(BTN_A) && !S->cool) {
        S->swing = 6;
        S->cool = 5;
        S->mx16 = (int16_t)(hole_x(S->aim) * 16);
        S->my16 = (int16_t)(hole_y(S->aim) * 16);
        whack(S->aim);
    }
    for (i = 0; i < 9; i++) {
        WHole *h = &S->hole[i];
        if (h->st == H_DOWN) continue;
        h->t++;
        if (h->dazed) h->dazed--;
        switch (h->st) {
        case H_RISE: if (h->t >= 6) { h->st = H_UP; h->t = 0; } break;
        case H_UP:
            if (h->t >= h->stay) {
                h->st = H_SINK;
                h->t = 0;
                if (h->kind == C_FUZZ) {
                    mg_add_score(5, hole_x(i), hole_y(i) - 20);
                    mg_popup(hole_x(i), hole_y(i) - 30, "bless you!", 0, PAL_UI_GOLD);
                } else {
                    mg_combo_break();
                    if (h->kind != C_GOLD) mg_popup(hole_x(i), hole_y(i) - 26, "nyah!", 0, PAL_PAINT(PAINT_CHERRY, 2));
                }
            }
            break;
        case H_SINK: if (h->t >= 6) h->st = H_DOWN; break;
        case H_BONK: if (h->t >= 16) h->st = H_DOWN; break;
        default: break;
        }
    }
    if (S->spawn_t) S->spawn_t--;
    else {
        spawn();
        S->spawn_t = (uint16_t)DOOMO_MAX(iv[mg.tier] - mg.play_t / 150, 6) + (uint16_t)mg_rand(8);
    }
    if (mg.time_left == 1 && S->fuzz_bonks == 0 && mg.score >= mg.g->medal[0]) mg_trophy(MG_UL_FUZZ_FAN);
}

/* ---- drawing ------------------------------------------------------------------ */
static uint8_t band_col(int row)
{
    if (mg.hell) return row & 1 ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_STONE, 1);
    return row & 1 ? PAL_RAMP(RAMP_GRASS, 5) : PAL_RAMP(RAMP_GRASS, 6);
}

static void critter(const WHole *h, int x, int y, int r, int t)
{
    int bonk = h->st == H_BONK && h->t < 10;
    int sq = bonk ? 256 + 80 : 256, sy = bonk ? 256 - 90 : 256;
    switch (h->kind) {
    case C_FUZZ:
        mg_fuzz(x, y + 2, r - 2, t, bonk);
        break;
    case C_WRAITH:
        mg_wraith(x, y + (bonk ? 4 : 0), r - 2, t, h->hits || bonk);
        if (h->dazed && (t & 2)) ui_sparkle(x + r, y - r, t & 15, PAL_WHITE);
        break;
    default:
        mg_imp(x, y + (bonk ? r / 3 : 0), r * sq / 256, r * sy / 256, t,
               (bonk ? IMP_BONK : (h->t / 20) & 1 ? IMP_SNEAKY : IMP_GRIN) | (h->kind == C_GOLD ? IMP_GOLD : 0));
        break;
    }
}

/* the rows of a hole's front ring: per row y (0..ry + 3) the outer and the inner half width */
static void front_rows(int rx, int ry, uint8_t *o, uint8_t *in)
{
    int y, orx = rx + 4, ory = ry + 3;
    for (y = 0; y <= ory && y < 12; y++) {
        o[y] = (uint8_t)(orx * fx_isqrt((uint32_t)((ory * ory - y * y) * 256 / (ory * ory))) / 16);
        in[y] = (uint8_t)(y < ry ? rx * fx_isqrt((uint32_t)((ry * ry - y * y) * 256 / (ry * ry))) / 16 : 0);
    }
}

/* the front half of the hole's dirt ring + the ground in front of it (mg_rect: over the kept picture
 * the same spans are put back, mg.h "undo"). tab: front_rows' rows. */
static void hole_front(int cx, int hy, int rx, int ry, int row, int cover, uint8_t tab[2][12])
{
    int y, orx = rx + 4, ory = ry + 3;
    uint8_t dirt = mg.hell ? PAL_RAMP(RAMP_WOOD, 1) : PAL_PAINT(PAINT_CARAMEL, 1);
    uint8_t dirt_l = mg.hell ? PAL_RAMP(RAMP_WOOD, 2) : PAL_PAINT(PAINT_CARAMEL, 2);
    for (y = 0; y <= ory; y++) {
        int o = tab[0][y], in = tab[1][y];
        if (in > 0) {
            mg_rect(cx - o, hy + y, o - in, 1, y < 2 ? dirt_l : dirt);
            mg_rect(cx + in, hy + y, o - in, 1, y < 2 ? dirt_l : dirt);
        } else {
            mg_rect(cx - o, hy + y, o * 2, 1, y == ory ? ui_darker(dirt) : dirt);
        }
    }
    mg_rect(cx - orx - 2, hy + ory + 1, (orx + 2) * 2, cover, band_col(row));
}

static void hole_back(int x, int hy, int rx, int ry)
{
    ui_fill_ellipse(x, hy, rx + 4, ry + 3, mg.hell ? PAL_RAMP(RAMP_WOOD, 0) : PAL_PAINT(PAINT_CARAMEL, 0));
    ui_fill_ellipse(x, hy - 1, rx + 3, ry + 2, mg.hell ? PAL_RAMP(RAMP_WOOD, 2) : PAL_PAINT(PAINT_CARAMEL, 2));
    ui_fill_ellipse(x, hy, rx, ry, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_ellipse(x, hy + 1, rx - 2, ry - 1, mg.hell ? PAL_RAMP(RAMP_LAVA, 1) : PAL_PAINT(PAINT_CHOCOLATE, 0));
}

/* flower k (or its bones): where it stands, and the box of every pixel it can paint */
static uint32_t flower_at(int k, int *fx2, int *fy)
{
    uint32_t h = doomo_hash32((uint32_t)k * 2654435761u + 7u);
    int ft = field_top();
    *fx2 = (int)(h % (uint32_t)mg_w());
    *fy = ft + 8 + (int)((h >> 10) % (uint32_t)(mg_h() - ft - 12));
    return h;
}
static void flower_box(int k, int *x0, int *y0, int *x1, int *y1)
{
    int fx2, fy;
    flower_at(k, &fx2, &fy);
    if (mg.hell) { *x0 = fx2 - 1; *x1 = fx2 + 5; *y0 = fy - 1; *y1 = fy + 2; }
    else { *x0 = fx2 - 2; *x1 = fx2 + 3; *y0 = fy - 3; *y1 = fy + 3; }
}

/* (S2-4, S2-4b) the kept playfield (mg.h): two bands. The tent at one of its five sways with the sign
 * board at one of its three bulb phases (rows above the field: 15 keys a tier), and the field with its
 * grass bands and nine empty holes (one key). Live: the sign's imps, the flowers, the critters, the
 * mallet. */
#define WK_BULBS 8       /* tent key: sway + 2 (0..4) | the sign's bulb phase (0..2) << 3 */
#define WK_FIELD 0x40    /* the field's key */
static uint32_t wh_kept_key(void)
{
    int t = mg.play_t + mg.t;
    return (uint32_t)(ui_wave(t, 150, 2) + 2) | (uint32_t)((t / 4) % 3) * WK_BULBS;
}
static int wh_kept_keys(uint32_t *out, int cap, int hell)
{
    int i, b, n = 0;
    (void)hell;
    for (b = 0; b < 3; b++)
        for (i = 0; i < 5; i++) {
            if (n < cap) out[n] = (uint32_t)i | (uint32_t)b * WK_BULBS;
            n++;
        }
    if (n < cap) out[n] = WK_FIELD;
    return n + 1;
}
static int wh_kept_part(uint32_t key, int16_t rows[2])
{
    int ft = field_top();
    rows[0] = (int16_t)(key == WK_FIELD ? ft : 0);
    rows[1] = (int16_t)(key == WK_FIELD ? mg_h() : ft);
    return 1;
}

/* the painted sign board with its marquee bulbs (and its words, which no imp reaches: left out when
 * the board is put back over the imps behind it) */
static void sign_board(int sx, int sy, int sw, int sh, int tb, int cx)
{
    ui_round_rect(sx + 2, sy + 2, sw, sh, 5, PAL_RAMP(RAMP_STONE, 1));
    ui_round_rect(sx, sy, sw, sh, 5, PAL_RAMP(RAMP_WOOD, 1));
    ui_round_rect(sx + 2, sy + 2, sw - 4, sh - 4, 4, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_WOOD, 5));
    mg_bulbs(sx + 1, sy + 1, sw - 2, sh - 2, tb);
    if (mg_kept_mode < MGK_UNDO)
        mg_textc(cx, sy + sh / 2 - 3, "BONK 'EM!", mg.hell ? PAL_RAMP(RAMP_LAVA, 6) : PAL_WHITE);
}

/* (S2-4b) the first frame of a round over the kept picture lists what it must put back every frame:
 * each hole's ring, front and ground inside the box of every flower they are painted over (the full
 * frame paints the flowers first), and the sign board inside the box its imp peeks in. Nothing here
 * moves during a round, so the lists hold. -1: a list was too short (the frame then replays them). */
static void holes_over_flowers(uint8_t tab[2][12])
{
    int i, k, r = crit_r(), rx = hole_rx(), ry = hole_ry();
    for (i = 0; i < 9; i++) {
        int x = hole_x(i), y = hole_y(i);
        for (k = 0; k < 14; k++) {
            int x0, y0, x1, y1;
            flower_box(k, &x0, &y0, &x1, &y1);
            if (x1 <= x - rx - 6 || x0 >= x + rx + 6 || y1 <= y - ry - 4 || y0 >= y + ry + r * 2 + 13) continue;
            mg_undo(x0, y0, x1, y1);
            hole_back(x, y, rx, ry);
            hole_front(x, y, rx, ry, i / 3, r * 2 + 8, tab);
            mg_undo_end();
        }
    }
}
static void sign_over_imp(int k, int sx, int sy, int sw, int sh)
{
    int ix = k ? sx + sw - 12 : sx + 12;
    mg_undo(ix - 14, sy - 12, ix + 14, sy + 14);          /* (the imp: 11 px to each side, 3 rows of peek) */
    sign_board(sx, sy, sw, sh, 0, 0);
    mg_undo_end();
}
static void wh_puts(int sx, int sy, int sw, int sh, uint8_t tab[2][12])
{
    int k;
    mg_undo_rec(S->fl_put, WK_FL_PUTS);
    holes_over_flowers(tab);
    S->n_fl = (int8_t)mg_undo_rec_end();
    for (k = 0; k < 2; k++) {
        mg_undo_rec(S->sign_put[k], WK_SIGN_PUTS);
        sign_over_imp(k, sx, sy, sw, sh);
        S->n_sign[k] = (int8_t)mg_undo_rec_end();
    }
    mg_undo_rec(S->ring_put, WK_RING_PUTS);        /* hole 0's front ring (no ground rows) */
    mg_undo(0, 0, mg_w(), mg_h());
    hole_front(hole_x(0), hole_y(0), hole_rx(), hole_ry(), 0, 0, tab);
    mg_undo_end();
    S->n_ring = (int8_t)mg_undo_rec_end();
    S->puts_ok = 1;
}

static void wh_draw(void)
{
    int W = mg_w(), H = mg_h(), t = mg.play_t + mg.t, i, r = crit_r(), rx = hole_rx(), ry = hole_ry();
    int ft = field_top(), ox = mg.ox, oy = mg.oy, dy = row_dy();
    int bake = mg_kept_mode == MGK_BAKE;
    uint8_t tab_[2][12], (*tab)[12] = bake ? tab_ : S->front;
    if (bake) front_rows(rx, ry, tab_[0], tab_[1]);
    /* booth backdrop: tent wall + a painted sign board */
    if (mg_still()) {
        uint8_t a = mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_LEMON, 1);
        uint8_t b = mg.hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_WHITE;
        uint8_t trim = mg.hell ? PAL_RAMP(RAMP_LAVA, 4) : PAL_PAINT(PAINT_TANGERINE, 1);
        if (bake) mg_tent_sway(0, ft + oy, a, b, trim, (int)(mg_bake_key % WK_BULBS) - 2);
        else mg_tent(0, ft + oy, a, b, trim, t);
    }
    {
        int sw = mg_big() ? 150 : 112, sh = mg_big() ? 24 : 18, sx = W / 2 - sw / 2 + ox, sy = ft - sh - 6 + oy;
        int tb = bake ? (int)(mg_bake_key / WK_BULBS % 3) * 4 : t;
        if (mg_live()) {
            /* two cheeky imps peeking over the sign's corners; over the kept picture the board is put
             * back over each (its round rects and bulbs in the imp's box) */
            int pk = ui_wave(t, 90, 3), k;
            for (k = 0; k < 2; k++) {
                int ix = k ? sx + sw - 12 : sx + 12, iy = sy - 1 + (k ? (pk < 0 ? -pk : 0) : (pk > 0 ? pk : 0));
                if (mg_kept() && !S->puts_ok) wh_puts(sx, sy, sw, sh, tab);
                mg_imp(ix, iy, 7, 6, k ? t + 33 : t, k ? IMP_GRIN : IMP_SNEAKY);
                if (S->n_sign[k] >= 0) mg_kept_puts(S->sign_put[k], S->n_sign[k], 0, 0);
                else if (mg_kept()) sign_over_imp(k, sx, sy, sw, sh);
            }
        }
        if (mg_still()) sign_board(sx, sy, sw, sh, tb, W / 2 + ox);
    }
    /* mowed grass bands, one per row of holes */
    if (mg_still()) {
        for (i = 0; i < 4; i++) {
            int y0 = i == 0 ? ft : ft + 6 + i * dy - dy / 4;
            int y1 = i == 3 ? H + 8 : ft + 6 + (i + 1) * dy - dy / 4;
            plat_rect(0, y0 + oy, W, y1 - y0, band_col(i == 0 ? 0 : i - 1));
        }
        plat_rect(0, ft + oy, W, 2, mg.hell ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_GRASS, 7));
    }
    if (mg_kept()) mg_kept_band(WK_FIELD);         /* the field's band; the holes' restores come from it */
    /* flowers (or bones and embers) between the holes */
    for (i = 0; i < 14 && mg_live(); i++) {
        int fx2 = S->flower[i].x + ox, fy = S->flower[i].y + oy;
        if (mg.hell) {
            plat_rect(fx2, fy, 4, 1, PAL_RAMP(RAMP_BONE, 5));
            plat_rect(fx2 - 1, fy - 1, 1, 3, PAL_RAMP(RAMP_BONE, 6));
            plat_rect(fx2 + 4, fy - 1, 1, 3, PAL_RAMP(RAMP_BONE, 6));
        } else {
            uint8_t c = S->flower[i].pink ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_WHITE;
            int sway = ui_wave(t + i * 11, 70, 1);
            plat_rect(fx2, fy, 1, 3, PAL_RAMP(RAMP_GRASS, 3));
            plat_rect(fx2 - 1 + sway, fy - 2, 3, 1, c);
            plat_rect(fx2 + sway, fy - 3, 1, 3, c);
            plat_rect(fx2 + sway, fy - 2, 1, 1, PAL_UI_GOLD);
        }
    }
    if (!mg_kept()) {}                             /* the holes over the flowers (wh_puts) */
    else if (S->n_fl >= 0) mg_kept_puts(S->fl_put, S->n_fl, 0, 0);
    else holes_over_flowers(tab);
    for (i = 0; i < 9; i++) {
        const WHole *h = &S->hole[i];
        int x = hole_x(i) + ox, hy = hole_y(i) + oy, rise;
        int row = i / 3, up;
        if (bake) {                               /* the still hole: its dirt ring and front */
            hole_back(x, hy, rx, ry);
            hole_front(x, hy, rx, ry, row, r * 2 + 8, tab);
            continue;
        }
        rise = rise_of(h);
        up = h->st != H_DOWN && rise > 0;
        /* back of the dirt ring + the hole (over the kept picture it is there already) */
        if (!mg_kept()) hole_back(x, hy, rx, ry);
        if (up) critter(h, x, hy + r + 3 - rise, r, t + i * 17);
        else if (((t / 40 + i * 5) % 9) == 0 && (t % 40) > 6) {   /* glowing eyes in the dark */
            uint8_t ec = mg.hell ? PAL_RAMP(RAMP_LAVA, 6) : PAL_PAINT(PAINT_LEMON, 2);
            if ((t % 40) != 20) {
                plat_rect(x - 4, hy, 2, 2, ec);
                plat_rect(x + 3, hy, 2, 2, ec);
            }
        }
        if (!mg_kept()) hole_front(x, hy, rx, ry, row, r * 2 + 8, tab);
        else if (up) {
            /* (the ground in front hides the critter's body only as low as it reaches: a wraith's
             * tail, 3r + 12 below the rim at rise 0) */
            int cover = DOOMO_MIN(r * 2 + 8, r * 3 + 12 - rise - (ry + 4));
            if (S->n_ring >= 0) {
                mg_kept_puts(S->ring_put, S->n_ring, x - hole_x(0), hy - hole_y(0));
                mg_kept_put(x - rx - 6, hy + ry + 4, (rx + 6) * 2, cover);
            } else {
                mg_undo(0, hy, W, hy + ry + 4 + cover);
                hole_front(x, hy, rx, ry, row, r * 2 + 8, tab);
                mg_undo_end();
            }
        }
        if (i == S->aim) {                     /* aim marker under the hole */
            int b = ui_wave(t, 20, 1);
            ui_heart(x, hy + ry + 8 + b, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        }
    }
    /* the squeaky lollipop mallet */
    {
        int mx = S->mx16 / 16 + ox, my = S->my16 / 16 + oy;
        int sw = S->swing, hx, hy2, k;
        if (sw > 3) { hx = mx + 2; hy2 = my - r - 4; }
        else if (sw) { hx = mx + 6; hy2 = my - r - 12; }
        else { hx = mx + 14; hy2 = my - r * 2 - 10 + ui_wave(t, 30, 2); }
        if (mg_live()) {
            for (k = 0; k < 3; k++)
                plat_line(hx + 2 + k, hy2 + 4, hx + 14 + k, hy2 + 22, k == 1 ? PAL_WHITE : PAL_PAINT(PAINT_SKY, 1));
            ui_fill_ellipse(hx, hy2, 9 + (sw > 3 ? 2 : 0), 7 - (sw > 3 ? 2 : 0), PAL_PAINT(PAINT_ROSE, 0));
            ui_fill_ellipse(hx, hy2, 8 + (sw > 3 ? 2 : 0), 6 - (sw > 3 ? 2 : 0), PAL_PAINT(PAINT_BUBBLEGUM, 1));
            plat_line(hx - 5, hy2 - 2, hx + 5, hy2 + 2, PAL_WHITE);
            plat_line(hx - 5, hy2 + 2, hx + 5, hy2 - 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            plat_rect(hx - 4, hy2 - 4, 3, 1, PAL_WHITE);
            if (sw > 3) {
                mg_text(hx - 12, hy2 - 16, "BONK", PAL_UI_GOLD);
            }
        }
    }
}

static void wh_art(int cx, int cy, int t)
{
    int ph = t % 60, rise = ph < 10 ? ph : ph < 40 ? 10 : ph < 50 ? 50 - ph : 0;
    ui_fill_ellipse(cx, cy + 10, 16, 6, PAL_PAINT(PAINT_CARAMEL, 1));
    ui_fill_ellipse(cx, cy + 10, 12, 4, PAL_RAMP(RAMP_STONE, 0));
    if (rise) mg_imp(cx, cy + 14 - rise * 2, 9, ph > 30 && ph < 40 ? 6 : 8, t, ph > 30 && ph < 40 ? IMP_BONK : IMP_GRIN);
    ui_fill_ellipse(cx, cy + 14, 16, 4, PAL_PAINT(PAINT_CARAMEL, 1));
    plat_rect(cx - 16, cy + 14, 32, 6, PAL_RAMP(RAMP_GRASS, 5));
    ui_fill_ellipse(cx + 12, cy - 14 + (ph > 28 && ph < 34 ? 10 : 0), 7, 5, PAL_PAINT(PAINT_BUBBLEGUM, 1));
}

static uint32_t wh_bot(void)
{
    int i, best = -1, bestv = 0;
    uint32_t b = 0;
    for (i = 0; i < 9; i++) {
        const WHole *h = &S->hole[i];
        int v;
        /* a human-ish reaction time: only critters that have been up a while */
        if (h->kind == C_FUZZ || h->st != H_UP || h->t < 7 + (i * 3 + mg.play_t / 64) % 6) continue;
        v = h->kind == C_GOLD ? 5 : h->kind == C_WRAITH ? 3 : 2;
        if (v > bestv) { bestv = v; best = i; }
    }
    if (best < 0) return 0;
    if (best % 3 == 0) b |= BTN_LEFT;
    if (best % 3 == 2) b |= BTN_RIGHT;
    if (best / 3 == 0) b |= BTN_UP;
    if (best / 3 == 2) b |= BTN_DOWN;
    if (!(mg.bot_now & BTN_A) && S->aim == best && mg_rand(100) < 70) b |= BTN_A;
    return b;
}

const MgGame mg_game_whack = {
    "WHACK-AN-IMP", "a Bonk-a-Demon booth",
    "D-pad aims like a numpad, A bonks. Wraiths take two. Spare Father Fuzz!",
    STR_ICON_ARROW_U STR_ICON_ARROW_D STR_ICON_ARROW_L STR_ICON_ARROW_R " aim  " STR_ICON_A " bonk",
    MG_WHACK, PROG_HS_WHACK, SONG_FESTIVAL, 3, 337,       /* Whackamole */
    { MG_WHACK_B, MG_WHACK_S, MG_WHACK_G }, 24, 45 * 30,
    { PAL_PAINT(PAINT_LEMON, 1), PAL_WHITE, PAL_PAINT(PAINT_TANGERINE, 1) },
    wh_start, wh_update, wh_draw, wh_art, wh_bot,
    wh_kept_key, wh_kept_keys, wh_kept_part, NULL, NULL
};

MG_DEFINE_SCENE(SCENE_MG_WHACK, "MG_WHACK", mg_game_whack);
