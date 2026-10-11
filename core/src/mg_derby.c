/* mg_derby.c - DOOMOGACHI DERBY (owner: minigames).
 *
 * Your Doomogachi races five rival pets down the candy-turf track in
 * front of a cheering grandstand. Mash A to sprint (it burns stamina),
 * press B to jump the candy-cane hurdles - jump right on the beat for a
 * PERFECT boost, miss and you trip. Boost pads give a free burst. Your
 * pet's real stats decide its pace: stage, fed and happy hearts, weight,
 * and its line (feral pets run faster, angels tire slower, demons boost
 * harder, cute ones time their jumps better). No pet yet? Tix lends you a
 * rental Mochiblob. Racing makes your pet happy (pet_play_result).
 *
 * The six racers are rendered once at the start from pet_render() into
 * 2-bit packed sprites (6 x 256 bytes of the arena) and drawn with
 * mg_blit2 in jersey colours, with squash and stretch while running.
 */
#include "mg.h"

#define RACERS 6
#define TRACK_LEN 3200
#define HURDLE_FIRST 360
#define HURDLE_EVERY 420
#define YOU 5

typedef struct Racer {
    int32_t x16;
    int16_t v16;
    uint8_t form, stam, stun, air, paint, fin, tired, pad_hit;
    uint16_t fin_t;
    uint8_t skill, rating;
} Racer;

typedef struct Derby {
    Racer r[RACERS];
    uint8_t spr[RACERS][PET_SPR * PET_SPR / 4];
    uint8_t line, perfects, finished, place, trips, cheer;
    uint16_t race_t, end_t;
    int32_t cam16;
} Derby;
MG_ARENA_FITS(Derby);
#define S (MG_ARENA(Derby))

static const uint8_t jersey[RACERS] = { PAINT_SKY, PAINT_LIME, PAINT_TANGERINE, PAINT_LAVENDER,
                                        PAINT_LEMON, PAINT_BUBBLEGUM };
static const char *const place_text[RACERS] = { "1ST PLACE!", "2ND PLACE!", "3RD PLACE",
                                                "4TH PLACE", "5TH PLACE", "6TH PLACE..." };
static const uint16_t place_pts[RACERS] = { 1500, 1000, 700, 450, 300, 200 };

static int sky_h(void) { return mg_h() * 2 / 9; }
static int stand_h(void) { return mg_h() / 7; }
static int track_top(void) { return sky_h() + stand_h(); }
static int lane_h(void) { return (mg_h() - track_top() - 6) / RACERS; }
static int lane_y(int l) { return track_top() + (l + 1) * lane_h(); }   /* feet line */

int mg_derby_pet_speed(void)
{
    const DoomoPet *p = &doomo_town.pet;
    static const uint8_t stage_base[] = { 75, 62, 74, 86, 98, 75 };
    int r;
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage > PET_STAGE_ADULT) return 75;
    r = stage_base[p->stage] + p->hunger * 2 + p->joy * 2;
    if (p->weight > 40) r -= (p->weight - 40) / 4;
    if (pet_is_sick()) r -= 10;
    return DOOMO_CLAMP(r, 40, 120);
}

static int your_form(void)
{
    const DoomoPet *p = &doomo_town.pet;
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage > PET_STAGE_ADULT) return 1;  /* a rental Puffling */
    return p->form % PET_FORM_COUNT;
}

static const char *your_name(void)
{
    static char buf[10];
    const DoomoPet *p = &doomo_town.pet;
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage > PET_STAGE_ADULT) return "Rental";
    memcpy(buf, p->name, 8);
    buf[8] = 0;
    return buf[0] ? buf : "You";
}

/* Render one form into a packed 2-bit sprite (a 1 KB stack canvas). */
static void pack_form(int form, uint8_t *dst)
{
    uint8_t canvas[PET_SPR * PET_SPR];
    int i;
    pet_render(form, PET_ANIM_IDLE0, canvas);
    for (i = 0; i < PET_SPR * PET_SPR / 4; i++)
        dst[i] = (uint8_t)(((canvas[i * 4] & 3) << 6) | ((canvas[i * 4 + 1] & 3) << 4) |
                           ((canvas[i * 4 + 2] & 3) << 2) | (canvas[i * 4 + 3] & 3));
}

static void dy_start(void)
{
    int i, used = 0, me = your_form();
    static const uint8_t skill[MG_TIERS] = { 65, 82, 94 };
    used |= 1 << me;
    for (i = 0; i < RACERS; i++) {
        Racer *r = &S->r[i];
        if (i == YOU) {
            r->form = (uint8_t)me;
            r->rating = (uint8_t)mg_derby_pet_speed();
            r->skill = 100;
        } else {
            int f, tries = 0;
            do { f = mg_rand(PET_FORM_COUNT); } while ((used & (1 << f)) && ++tries < 40);
            used |= 1 << f;
            r->form = (uint8_t)f;
            r->rating = (uint8_t)(66 + mg.tier * 12 + mg_rand(14) + (f % 4) * 2);
            r->skill = (uint8_t)(skill[mg.tier] - mg_rand(12));
        }
        r->paint = jersey[i];
        r->stam = 100;
        pack_form(r->form, S->spr[i]);
    }
    S->line = (uint8_t)(pet_exists() && doomo_town.pet.stage >= PET_STAGE_BABY ? pet_line() : PET_LINE_CUTE);
    mg.quiet_hud = 0;
}

static int cruise16(const Racer *r, int you)
{
    int c = 30 + r->rating / 5;
    if (you && S->line == PET_LINE_FERAL) c += 3;
    if (r->tired) c -= 10;
    return c;
}

static int next_hurdle(int x)
{
    int k;
    if (x < HURDLE_FIRST) return HURDLE_FIRST;
    k = (x - HURDLE_FIRST) / HURDLE_EVERY + 1;
    return HURDLE_FIRST + k * HURDLE_EVERY;
}

static int pad_at(int lane, int k)
{
    return 250 + k * 520 + (int)(doomo_hash32((uint32_t)(lane * 97 + k)) % 200);
}

static void jump(Racer *r, int you, int lane)
{
    int x = r->x16 / 16, h = next_hurdle(x - 8), d = h - x;
    int win = you && S->line == PET_LINE_CUTE ? 8 : 0;
    r->air = 18;
    if (!you) return;
    audio_sfx(SFX_BLIP);
    if (d >= 18 - win && d <= 40 + win && h < TRACK_LEN) {
        int sx = mg_w() * 2 / 5, sy = lane_y(lane) - 20;
        r->v16 = (int16_t)(r->v16 + 14);
        S->perfects++;
        mg_combo_hit(sx, sy);
        mg_add_score(50, sx, sy - 6);
        mg_popup(sx, sy - 18, "PERFECT!", 0, PAL_UI_GOLD);
        mg_burst(MGFX_STAR, sx, sy + 10, 6, 30, PAL_UI_GOLD);
        audio_sfx(SFX_SPARKLE);
    }
}

static void race_tick(Racer *r, int i)
{
    int you = i == YOU, x, c = cruise16(r, you), h, k;
    if (r->fin) { r->v16 = (int16_t)DOOMO_MAX(r->v16 - 2, 8); r->x16 += r->v16; return; }
    if (r->stun) { r->stun--; r->v16 = (int16_t)DOOMO_MAX(r->v16 - 3, 6); }
    else if (r->v16 < c) r->v16 = (int16_t)DOOMO_MIN(r->v16 + 2, c);
    else if (r->v16 > c) r->v16 = (int16_t)(r->v16 - 1);
    if (r->air) r->air--;
    if (r->tired && --r->tired == 0) r->stam = 30;
    /* sprinting */
    if (you) {
        if (mg_pressed(BTN_A) && !r->stun && r->stam && !r->tired) {
            int cost = S->line == PET_LINE_ANGEL ? 4 : 6;
            r->v16 = (int16_t)DOOMO_MIN(r->v16 + 5 + (S->line == PET_LINE_DEMON), c + 30);
            r->stam = (uint8_t)(r->stam > cost ? r->stam - cost : 0);
            mg_fx(MGFX_PUFF, mg_w() * 2 / 5 - 14, lane_y(i) - 2, -20, -4, PAL_WHITE, 10);
            if (!r->stam) { r->tired = 45; mg_popup(mg_w() * 2 / 5, lane_y(i) - 40, "pant pant...", 0, PAL_WHITE); }
        } else if ((mg.play_t & 1) && r->stam < 100 && !r->tired) {
            r->stam++;
        }
        if (mg_pressed(BTN_B | BTN_UP) && !r->air && !r->stun) jump(r, 1, i);
    } else if (!r->stun) {
        int beat = 7 - mg.tier * 2;
        if ((mg.play_t + i * 3) % beat == 0 && r->stam > 20 && mg_rand(100) < r->skill)
            { r->v16 = (int16_t)DOOMO_MIN(r->v16 + 5, c + 26); r->stam -= 6; }
        else if (r->stam < 100 && (mg.play_t & 1)) r->stam++;
        x = r->x16 / 16;
        h = next_hurdle(x - 8);
        if (!r->air && h - x < 30 && h - x > 16 && h < TRACK_LEN) {
            if (mg_rand(100) < r->skill) { r->air = 18; if (mg_rand(100) < r->skill / 2) r->v16 += 10; }
            else r->air = 1;                  /* too late: will trip */
        }
    }
    r->x16 += r->v16;
    x = r->x16 / 16;
    /* hurdles */
    h = next_hurdle(x - 8);
    if (h - 8 <= x && x <= h + 2 && !r->air && !r->stun && h < TRACK_LEN) {
        r->stun = 22;
        r->v16 = 10;
        if (you) {
            S->trips++;
            mg_combo_break();
            mg_shake_add(5);
            mg_popup(mg_w() * 2 / 5, lane_y(i) - 30, "TRIP!", 0, PAL_PAINT(PAINT_CHERRY, 2));
            mg_burst(MGFX_CRUMB, mg_w() * 2 / 5, lane_y(i), 6, 26, PAL_PAINT(PAINT_CHERRY, 1));
            audio_sfx(SFX_SQUEAK);
        }
    }
    /* boost pads */
    for (k = 0; k < 6; k++) {
        int p = pad_at(i, k);
        if (x >= p && x < p + 12 && !(r->pad_hit & (1 << k)) && !r->air) {
            r->pad_hit |= (uint8_t)(1 << k);
            r->v16 = (int16_t)(r->v16 + 16);
            if (you) {
                mg_popup(mg_w() * 2 / 5, lane_y(i) - 30, "BOOST!", 0, PAL_PAINT(PAINT_MINT, 2));
                mg_burst(MGFX_SPARK, mg_w() * 2 / 5, lane_y(i) - 4, 6, 30, PAL_PAINT(PAINT_MINT, 2));
                audio_sfx(SFX_PICKUP);
            }
        }
    }
    if (x >= TRACK_LEN && !r->fin) {
        r->fin = (uint8_t)(++S->finished);
        r->fin_t = S->race_t;
    }
}

static void dy_update(void)
{
    int i, W = mg_w();
    Racer *me = &S->r[YOU];
    S->race_t++;
    if (S->cheer) S->cheer--;
    for (i = 0; i < RACERS; i++) race_tick(&S->r[i], i);
    {
        int32_t target = me->x16 - (W * 2 / 5) * 16;
        S->cam16 += (target - S->cam16) / 3;
    }
    if (me->fin && !S->place) {
        int margin = 9999;
        S->place = me->fin;
        mg_add_raw(place_pts[me->fin - 1] * (2 + mg.tier) / 2, W * 2 / 5, lane_y(YOU) - 40);
        if (me->fin == 1) {
            for (i = 0; i < RACERS; i++) {
                int d = (int)((me->x16 - S->r[i].x16) / 16);
                if (i != YOU && d < margin) margin = d;
            }
            mg_confetti_rain(30);
            if (margin < 10) { mg_popup(W / 2, mg_h() / 3, "PHOTO FINISH!", 0, PAL_UI_GOLD); mg_trophy(MG_UL_PHOTO); }
            audio_sfx(SFX_HAPPY);
        } else {
            audio_sfx(me->fin <= 3 ? SFX_SELECT : SFX_SAD);
        }
        S->cheer = 60;
        if (pet_exists() && doomo_town.pet.stage >= PET_STAGE_BABY && doomo_town.pet.stage <= PET_STAGE_ADULT)
            pet_play_result(me->fin <= 3);
    }
    if (S->place && ++S->end_t > 45) mg_end(place_text[S->place - 1]);
    if (S->race_t > 90 * 30 && !S->place) mg_end("TOO SLOW!");
}

/* ---- drawing ------------------------------------------------------------------ */
static void grandstand(int y0, int h, int cam, int t)
{
    int W = mg_w(), row, x, rh = h / 3;
    static const uint8_t heads[6] = { PAINT_PEACH, PAINT_BUBBLEGUM, PAINT_SKY, PAINT_LEMON, PAINT_MINT, PAINT_LAVENDER };
    plat_rect(0, y0, W, h, mg.hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_WOOD, 4));
    for (row = 0; row < 3; row++) {
        int y = y0 + row * rh, off = (cam / (3 - row + 1)) % 12;
        plat_rect(0, y + rh - 2, W, 2, mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_WOOD, 2));
        for (x = -off; x < W; x += 12) {
            uint32_t hs = doomo_hash32((uint32_t)((x + cam / (3 - row + 1)) / 12 * 31 + row));
            int jump = S->cheer && ((hs >> 4) & 1) ? ui_wave(t * 3 + (int)(hs & 63), 12, 2) : ui_wave(t + (int)(hs & 63), 40, 1);
            uint8_t c = mg.hell ? PAL_RAMP(RAMP_BLOOD, 3 + (hs & 1)) : PAL_PAINT(heads[hs % 6], 1);
            int hx = x + 6, hy = y + rh - 6 + (jump < 0 ? jump : 0);
            plat_rect(hx - 2, hy - 2, 5, 5, c);
            plat_rect(hx - 1, hy - 1, 1, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(hx + 1, hy - 1, 1, 1, PAL_RAMP(RAMP_STONE, 0));
            if ((hs >> 8) & 1) { plat_rect(hx - 3, hy - 4, 1, 2, c); plat_rect(hx + 3, hy - 4, 1, 2, c); }
        }
    }
    mg_bunting(0, W, y0 - 2, 3, t);
}

static void racer_draw(int i, int x, int t)
{
    const Racer *r = &S->r[i];
    int y = lane_y(i), sy = 256, hop = 0;
    uint8_t pal[3];
    pal[0] = ui_darker(ui_darker(PAL_PAINT(r->paint, 0)));
    pal[1] = PAL_PAINT(r->paint, 1);
    pal[2] = PAL_PAINT(r->paint, 2);
    if (r->air) hop = (18 - r->air) * r->air / 4;
    else if (r->stun) sy = 180 + (r->stun & 3) * 10;
    else { int ph = ((int)(r->x16 / 16) / 6 + i) & 3; hop = ph == 1 || ph == 2 ? 2 : 0; sy = ph == 0 ? 236 : 262; }
    mg_shadow(x, y, 9 - hop / 4);
    mg_blit2(x - PET_SPR / 2, y - PET_SPR - hop + 2, S->spr[i], PET_SPR, PET_SPR, pal, sy);
    if (r->stun && (t & 4)) ui_sparkle(x + 8, y - PET_SPR + 4, t & 15, PAL_UI_GOLD);
    if (r->tired && (t & 8)) mg_text(x + 10, y - PET_SPR - 4, STR_ICON_DROP, PAL_PAINT(PAINT_SKY, 2));
    if (i == YOU) {
        int b = ui_wave(t, 20, 2);
        ui_heart(x, y - PET_SPR - hop - 4 + b, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }
    if (r->fin && r->fin <= 3) {
        static const uint8_t mc[3] = { MG_MEDAL_GOLD, MG_MEDAL_SILVER, MG_MEDAL_BRONZE };
        mg_medal_icon(x - 12, y - PET_SPR + 6, mc[r->fin - 1], t);
    }
}

static void dy_draw(void)
{
    int W = mg_w(), H = mg_h(), t = mg.play_t + mg.t, i, l, k, ox = mg.ox, oy = mg.oy;
    int cam = (int)(S->cam16 / 16), tt = track_top(), lh = lane_h();
    mg_fair_sky(0, sky_h() + oy, t);
    grandstand(sky_h() + oy, stand_h(), cam, t);
    /* lanes */
    for (l = 0; l < RACERS; l++) {
        int y = tt + l * lh + oy;
        uint8_t c = mg.hell ? PAL_RAMP(RAMP_STONE, 1 + (l & 1)) : (l & 1 ? PAL_RAMP(RAMP_GRASS, 5) : PAL_RAMP(RAMP_GRASS, 6));
        plat_rect(0, y, W, lh, c);
        plat_rect(0, y + lh - 1, W, 1, mg.hell ? PAL_RAMP(RAMP_LAVA, 3) : PAL_WHITE);
    }
    plat_rect(0, tt + RACERS * lh + oy, W, H - tt - RACERS * lh, mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_PEACH, 5));
    plat_rect(0, tt + oy, W, 2, mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_WOOD, 2));
    /* distance posts, finish line */
    for (k = (cam / 200) * 200; k < cam + W + 200; k += 200) {
        int x = k - cam + ox;
        char buf[8];
        if (k <= 0 || k >= TRACK_LEN) continue;
        plat_rect(x, tt + oy, 2, RACERS * lh, mg.hell ? PAL_RAMP(RAMP_STONE, 3) : PAL_RAMP(RAMP_GRASS, 7));
        doomo_snprintf(buf, sizeof(buf), "%d", (TRACK_LEN - k) / 10);
        mg_text(x - ui_text_width(buf) / 2, tt + oy + 2, buf, PAL_WHITE);
    }
    {
        int fx2 = TRACK_LEN - cam + ox, r2;
        if (fx2 > -20 && fx2 < W + 20)
            for (r2 = 0; r2 < RACERS * lh; r2 += 4) {
                plat_rect(fx2, tt + r2 + oy, 4, 4, ((r2 / 4) & 1) ? PAL_WHITE : PAL_RAMP(RAMP_STONE, 0));
                plat_rect(fx2 + 4, tt + r2 + oy, 4, 4, ((r2 / 4) & 1) ? PAL_RAMP(RAMP_STONE, 0) : PAL_WHITE);
            }
    }
    /* boost pads + hurdles, then racers, lane by lane (back to front) */
    for (l = 0; l < RACERS; l++) {
        int y = lane_y(l) + oy;
        for (k = 0; k < 6; k++) {
            int px = pad_at(l, k) - cam + ox;
            if (px < -20 || px > W + 20) continue;
            plat_rect(px, y - lh / 2 - 1, 12, 4, mg.hell ? PAL_RAMP(RAMP_LAVA, 4) : PAL_PAINT(PAINT_MINT, 2));
            plat_rect(px + 2 + (t / 2) % 4, y - lh / 2, 3, 2, PAL_WHITE);
        }
        for (k = next_hurdle(cam - 40); k < cam + W + 40 && k < TRACK_LEN; k += HURDLE_EVERY) {
            int hx = k - cam + ox + (RACERS - 1 - l), hh = lh * 2 / 3 + 2, s;
            plat_rect(hx - 6, y - hh, 2, hh, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(hx + 5, y - hh, 2, hh, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(hx - 7, y - 1, 4, 1, PAL_RAMP(RAMP_STONE, 3));
            plat_rect(hx + 4, y - 1, 4, 1, PAL_RAMP(RAMP_STONE, 3));
            plat_rect(hx - 7, y - hh - 1, 15, 5, PAL_PAINT(PAINT_CHERRY, 0));
            for (s = 0; s < 13; s += 3)
                plat_rect(hx - 6 + s, y - hh, 2, 3, (s / 3) & 1 ? PAL_WHITE : PAL_PAINT(PAINT_CHERRY, 1));
        }
        {
            int rx = (int)(S->r[l].x16 / 16) - cam + ox;
            if (rx > -30 && rx < W + 30) racer_draw(l, rx, t + l * 9);
            else {
                int ex = rx < 0 ? 4 : W - 10;
                mg_text(ex, y - 12, rx < 0 ? STR_ICON_ARROW_L : STR_ICON_ARROW_R, PAL_PAINT(S->r[l].paint, 1));
            }
        }
    }
    /* race strip (progress) + stamina */
    {
        int bw = W / 3, bx = W - bw - 8, by = 24;
        ui_round_rect(bx - 2, by - 2, bw + 4, 8, 3, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(bx, by + 1, bw, 2, PAL_RAMP(RAMP_STONE, 3));
        plat_rect(bx + bw - 2, by - 1, 2, 6, PAL_WHITE);
        for (i = 0; i < RACERS; i++) {
            int px = bx + (int)DOOMO_MIN(S->r[i].x16 / 16, TRACK_LEN) * bw / TRACK_LEN;
            plat_rect(px - 1, by - (i == YOU ? 1 : 0), 3, i == YOU ? 6 : 4, PAL_PAINT(S->r[i].paint, i == YOU ? 2 : 1));
        }
    }
    {
        const Racer *me = &S->r[YOU];
        int sw = mg_big() ? 70 : 50, sx = 6, sy = 22;
        uint8_t c = me->tired ? PAL_GRAY : me->stam > 30 ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_TANGERINE, 1);
        ui_round_rect(sx - 1, sy - 1, sw + 2, 8, 3, PAL_RAMP(RAMP_STONE, 0));
        ui_bar(sx, sy, sw, 6, me->stam, 100, c);
        mg_text(sx + sw + 4, sy - 1, your_name(), PAL_WHITE);
        if (!S->place && (t / 8) & 1 && S->race_t < 200) mg_textc(W / 2, 32, "MASH " STR_ICON_A "!  " STR_ICON_B " jump", PAL_UI_GOLD);
        if (S->place) {
            static const char *const ord[RACERS] = { "1st", "2nd", "3rd", "4th", "5th", "6th" };
            mg_textc(W / 2, 32, ord[S->place - 1], PAL_UI_GOLD);
        }
    }
}

static void dy_art(int cx, int cy, int t)
{
    int hop = ((t / 4) & 1) ? 2 : 0, k;
    for (k = 0; k < 3; k++) plat_rect(cx - 18 - k * 6 - (t % 6), cy + 10 - k, 3, 1, PAL_WHITE);
    mg_shadow(cx, cy + 12, 10);
    ui_fill_ellipse(cx, cy + 2 - hop, 10, 9 - hop / 2, PAL_PAINT(PAINT_SKY, 0));
    ui_fill_ellipse(cx, cy + 2 - hop, 9, 8 - hop / 2, PAL_PAINT(PAINT_SKY, 2));
    plat_rect(cx - 4, cy - hop, 2, 3, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(cx + 3, cy - hop, 2, 3, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(cx - 1, cy + 4 - hop, 3, 1, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_circle(cx - 8, cy + 3 - hop, 2, PAL_PAINT(PAINT_ROSE, 2));
    mg_medal_icon(cx + 12, cy - 10, MG_MEDAL_GOLD, t);
}

static uint32_t dy_bot(void)
{
    const Racer *me = &S->r[YOU];
    int x = (int)(me->x16 / 16), h = next_hurdle(x - 8), d = h - x;
    uint32_t b = 0;
    if (d >= 22 && d <= 34 && !me->air && h < TRACK_LEN) b |= BTN_B;
    if (me->stam > 25 && !me->tired && (mg.play_t % 4) < 2) b |= BTN_A;
    return b;
}

const MgGame mg_game_derby = {
    "DOOMOGACHI DERBY", "Hellvile Park Downs",
    "Mash A to sprint, tap B to hop hurdles on the beat. Care makes pets fast!",
    STR_ICON_A " sprint  " STR_ICON_B " jump",
    MG_DERBY, PROG_HS_DERBY, SONG_FESTIVAL, 0, 742,       /* Eyeballer, bookie */
    { MG_DERBY_B, MG_DERBY_S, MG_DERBY_G }, 20, 0,
    { PAL_PAINT(PAINT_LIME, 1), PAL_WHITE, PAL_PAINT(PAINT_FOREST, 1) },
    dy_start, dy_update, dy_draw, dy_art, dy_bot
};

MG_DEFINE_SCENE(SCENE_MG_DERBY, "MG_DERBY", mg_game_derby);
