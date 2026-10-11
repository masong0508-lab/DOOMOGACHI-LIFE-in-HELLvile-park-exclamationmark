/* mg_memory.c - MEMORY MATCH (owner: minigames).
 *
 * Madame Moth's card table. Flip two cards at a time and pair up the
 * island's citizens (their real portraits). A flipped card lifts toward
 * you and shows the full portrait; a found pair turns into a little stage
 * where both chibis hop and cheer. Hellvile citizens' cards BITE when you
 * mismatch them (it eats seconds off the clock), and on the Hellfire tier the table
 * shuffles two face-down cards after every miss. Clearing the table with
 * no misses is PERFECT RECALL. On an empty island the deck falls back to
 * treats, imps and Father Fuzz (imps, wraiths and bagels bite too).
 *
 * Portrait cache note: only the (at most two) face-up cards ever ask for
 * portraits; matched cards use the cheap chibis, so the shared portrait
 * cache never thrashes.
 */
#include "mg.h"

#define MAXC 20
enum { CS_DOWN, CS_FLIP_UP, CS_UP, CS_FLIP_DOWN, CS_MATCHED };
enum { F_CUPCAKE = -1, F_DONUT = -2, F_COOKIE = -3, F_CANDY = -4, F_STAR = -5, F_HEART = -6,
       F_BAGEL = -7, F_IMP = -8, F_WRAITH = -9, F_FUZZ = -10 };
#define FALLBACKS 10

typedef struct MCard {
    int16_t face;
    uint8_t st, t;
} MCard;

typedef struct Memory {
    MCard c[MAXC];
    uint8_t n, cols, rows, cur, first, second, wait, misses, pairs_left, bite_t, bite_card;
    uint8_t shuffle_a, shuffle_b, shuffle_t;
} Memory;
MG_ARENA_FITS(Memory);
#define S (MG_ARENA(Memory))

static const uint8_t tier_cols[MG_TIERS] = { 4, 4, 5 };
static const uint8_t tier_rows[MG_TIERS] = { 3, 4, 4 };

static int is_biter(int face)
{
    return face >= DOOMO_DISTRICT_SIZE || face == F_BAGEL || face == F_IMP || face == F_WRAITH;
}

int mg_memory_pick_faces(int pairs, int16_t *out, uint32_t seed)
{
    DoomoRng r;
    int n = 0, i, want_hell = pairs / 3, tries;
    rng_seed(&r, seed);
    /* a few Hellvile citizens (they bite), then Hillvile ones */
    for (tries = 0; tries < 400 && n < want_hell; tries++) {
        int id = DOOMO_DISTRICT_SIZE + rng_range(&r, DOOMO_DISTRICT_SIZE), k, dup = 0;
        if (!town_alive(id)) continue;
        for (k = 0; k < n; k++) if (out[k] == id) dup = 1;
        if (!dup) out[n++] = (int16_t)id;
    }
    for (tries = 0; tries < 800 && n < pairs; tries++) {
        int id = rng_range(&r, DOOMO_MAX_RESIDENTS), k, dup = 0;
        if (!town_alive(id)) continue;
        for (k = 0; k < n; k++) if (out[k] == id) dup = 1;
        if (!dup) out[n++] = (int16_t)id;
    }
    /* sweep in order if random picks missed a sparse island */
    for (i = 0; i < DOOMO_MAX_RESIDENTS && n < pairs; i++) {
        int k, dup = 0;
        if (!town_alive(i)) continue;
        for (k = 0; k < n; k++) if (out[k] == i) dup = 1;
        if (!dup) out[n++] = (int16_t)i;
    }
    /* an empty island: treats, imps and the priest (distinct: a walk of
     * consecutive fallbacks from a seeded offset) */
    for (i = 0; n < pairs && i < FALLBACKS; i++) out[n++] = (int16_t)(-1 - (int)((i + seed) % FALLBACKS));
    return n;
}

static void mm_start(void)
{
    int16_t faces[MAXC / 2];
    int pairs, i;
    S->cols = tier_cols[mg.tier];
    S->rows = tier_rows[mg.tier];
    S->n = (uint8_t)(S->cols * S->rows);
    pairs = S->n / 2;
    mg_memory_pick_faces(pairs, faces, rng_next(&mg_rng));
    for (i = 0; i < S->n; i++) {
        S->c[i].face = faces[i / 2];
        S->c[i].st = CS_DOWN;
    }
    for (i = S->n - 1; i > 0; i--) {                 /* shuffle */
        int j = mg_rand(i + 1);
        int16_t f = S->c[i].face;
        S->c[i].face = S->c[j].face;
        S->c[j].face = f;
    }
    S->pairs_left = (uint8_t)pairs;
    S->first = S->second = 0xFF;
    S->cur = 0;
}

static int cell_w(void) { return DOOMO_MIN((mg_w() - 12) / S->cols, mg_big() ? 72 : 54); }
static int cell_h(void) { return DOOMO_MIN((mg_h() - 24) / S->rows, mg_big() ? 64 : 46); }
static int card_x(int i) { return mg_w() / 2 - S->cols * cell_w() / 2 + (i % S->cols) * cell_w() + cell_w() / 2; }
static int card_y(int i) { return 21 + (mg_h() - 22 - S->rows * cell_h()) / 2 + (i / S->cols) * cell_h() + cell_h() / 2; }

static void flip_up(int i)
{
    S->c[i].st = CS_FLIP_UP;
    S->c[i].t = 0;
    audio_sfx(SFX_CURSOR);
}

static void resolve(void)
{
    MCard *a = &S->c[S->first], *b = &S->c[S->second];
    int x = (card_x(S->first) + card_x(S->second)) / 2, y = (card_y(S->first) + card_y(S->second)) / 2;
    if (a->face == b->face) {
        a->st = b->st = CS_MATCHED;
        a->t = b->t = 0;
        mg_combo_hit(x, y);
        mg_add_score(80, x, y - 10);
        mg_burst(MGFX_HEART, card_x(S->first), card_y(S->first), 5, 30, PAL_PAINT(PAINT_ROSE, 2));
        mg_burst(MGFX_HEART, card_x(S->second), card_y(S->second), 5, 30, PAL_PAINT(PAINT_ROSE, 2));
        audio_sfx(SFX_LOVE);
        if (--S->pairs_left == 0) {
            int bonus = mg.time_left / 30 * (10 + mg.tier * 5);
            mg_add_raw(bonus, mg_w() / 2, mg_h() / 2);
            if (!S->misses) {
                mg_add_raw(500 + mg.tier * 250, mg_w() / 2, mg_h() / 2 + 12);
                mg_popup(mg_w() / 2, mg_h() / 2 - 14, "PERFECT RECALL!", 0, PAL_UI_GOLD);
                mg_trophy(MG_UL_RECALL);
            }
            mg_confetti_rain(30);
            mg_end("ALL MATCHED!");
        }
    } else {
        a->st = b->st = CS_FLIP_DOWN;
        a->t = b->t = 0;
        if (S->misses < 255) S->misses++;
        mg_combo_break();
        audio_sfx(SFX_CANCEL);
        if (is_biter(a->face) || is_biter(b->face)) {
            int bc = is_biter(a->face) ? S->first : S->second;
            S->bite_t = 20;
            S->bite_card = (uint8_t)bc;
            mg_popup(card_x(bc), card_y(bc) - 20, "CHOMP! -5 sec", 0, PAL_PAINT(PAINT_CHERRY, 2));
            mg.time_left = (uint16_t)(mg.time_left > (5 + mg.tier * 2) * 30 ? mg.time_left - (5 + mg.tier * 2) * 30 : 1);
            mg_shake_add(7);
            mg_flash(PAL_UI_RED, 3);
            mg_hitstop(3);
            audio_sfx(SFX_DEMON_ROAR);
        }
        if (mg.tier == MG_TIER_HELLFIRE && S->pairs_left > 2) {   /* the table shuffles two cards */
            int k, tries = 0, a2 = -1, b2 = -1;
            while (tries++ < 60 && (a2 < 0 || b2 < 0 || a2 == b2)) {
                k = mg_rand(S->n);
                if (S->c[k].st != CS_DOWN || k == S->first || k == S->second) continue;
                if (a2 < 0) a2 = k; else if (k != a2) b2 = k;
            }
            if (a2 >= 0 && b2 >= 0) {
                int16_t f = S->c[a2].face;
                S->c[a2].face = S->c[b2].face;
                S->c[b2].face = f;
                S->shuffle_a = (uint8_t)a2;
                S->shuffle_b = (uint8_t)b2;
                S->shuffle_t = 16;
            }
        }
    }
    S->first = S->second = 0xFF;
}

static void mm_update(void)
{
    int i, c = S->cols, n = S->n;
    if (S->bite_t) S->bite_t--;
    if (S->shuffle_t) S->shuffle_t--;
    if (mg_pressed(BTN_LEFT)) S->cur = (uint8_t)((S->cur % c) ? S->cur - 1 : S->cur + c - 1);
    if (mg_pressed(BTN_RIGHT)) S->cur = (uint8_t)((S->cur % c) == c - 1 ? S->cur - c + 1 : S->cur + 1);
    if (mg_pressed(BTN_UP)) S->cur = (uint8_t)(S->cur >= c ? S->cur - c : S->cur + n - c);
    if (mg_pressed(BTN_DOWN)) S->cur = (uint8_t)(S->cur + c < n ? S->cur + c : S->cur % c);
    if (mg_pressed(BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) audio_sfx(SFX_BLIP);
    if (mg_pressed(BTN_A) && S->c[S->cur].st == CS_DOWN && S->second == 0xFF && !S->wait) {
        if (S->first == 0xFF) { S->first = S->cur; flip_up(S->cur); }
        else { S->second = S->cur; flip_up(S->cur); S->wait = 34; }
    }
    if (S->wait && --S->wait == 0) resolve();
    for (i = 0; i < n; i++) {
        MCard *m = &S->c[i];
        if (m->t < 255) m->t++;
        if (m->st == CS_FLIP_UP && m->t >= 8) { m->st = CS_UP; m->t = 0; }
        if (m->st == CS_FLIP_DOWN && m->t >= 8) { m->st = CS_DOWN; m->t = 0; }
    }
}

/* ---- drawing ------------------------------------------------------------------ */
static void fallback_face(int face, int x, int y, int r, int t)
{
    switch (face) {
    case F_IMP: mg_imp(x, y, r, r - 1, t, IMP_GRIN); break;
    case F_WRAITH: mg_wraith(x, y - 2, r - 1, t, 0); break;
    case F_FUZZ: mg_fuzz(x, y - 2, r - 1, t, 0); break;
    default: mg_treat(-1 - face, x, y, r, 256, 256, t); break;
    }
}

static void card_back(int x, int y, int w, int h, int t, int i)
{
    uint8_t a = mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_LAVENDER, 1);
    uint8_t b = mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_PAINT(PAINT_GRAPE, 1);
    ui_round_rect(x - w / 2, y - h / 2, w, h, 4, b);
    ui_round_rect(x - w / 2 + 1, y - h / 2 + 1, w - 2, h - 2, 3, a);
    if (w > 10) {
        ui_frame(x - w / 2 + 3, y - h / 2 + 3, w - 6, h - 6, mg.hell ? PAL_RAMP(RAMP_LAVA, 4) : PAL_PAINT(PAINT_LAVENDER, 2));
        if (mg.hell) ui_star(x, y, 5, PAL_RAMP(RAMP_LAVA, 4));
        else ui_heart(x, y + ui_wave(t + i * 7, 60, 1), 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        plat_rect(x - w / 2 + 3, y - h / 2 + 2, w / 3, 1, PAL_WHITE);
    }
}

static void card_face_bg(int x, int y, int w, int h, int biter)
{
    uint8_t f = biter ? PAL_RAMP(RAMP_FLESH, 6) : PAL_UI_CREAM;
    ui_round_rect(x - w / 2, y - h / 2, w, h, 4, biter ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_BONE, 4));
    ui_round_rect(x - w / 2 + 1, y - h / 2 + 1, w - 2, h - 2, 3, f);
}

static void lifted(int i, int t)
{
    const MCard *m = &S->c[i];
    int x = card_x(i) + mg.ox, y = card_y(i) + mg.oy, rows = DOOMO_MIN(cell_h() + 6, mg_big() ? 52 : 44);
    int w = PORTRAIT_W + 6, h = rows + 6, bite = S->bite_t && S->bite_card == i;
    int pop = ui_pop_scale(m->t);
    w = w * pop / 256;
    h = h * pop / 256;
    if (bite) { x += (t & 2) ? 2 : -2; }
    ui_round_rect(x - w / 2 + 2, y - h / 2 + 3, w, h, 4, PAL_RAMP(RAMP_STONE, 1));
    card_face_bg(x, y, w, h, is_biter(m->face));
    if (pop < 240) return;
    if (m->face >= 0) {
        const DoomoImage *img = portrait_image(m->face, is_biter(m->face) ? EXPR_ANGRY : EXPR_HAPPY);
        if (img) {
            DoomoImage c = mg_crop(img, 2, rows);
            plat_image(x - PORTRAIT_W / 2, y - h / 2 + 3, &c, 1, 0, 0);
        }
    } else {
        fallback_face(m->face, x, y, mg_big() ? 14 : 12, t);
    }
    if (bite) {                                  /* fangs snapping shut */
        int k, close = (20 - S->bite_t) < 6 ? (20 - S->bite_t) : 6;
        for (k = -w / 2 + 4; k < w / 2 - 4; k += 6) {
            plat_rect(x + k, y - h / 2 + 1, 4, 2 + close, PAL_WHITE);
            plat_rect(x + k + 3, y + h / 2 - 3 - close, 4, 2 + close, PAL_WHITE);
        }
    }
}

static void matched(int i, int x, int y, int w, int h, int t)
{
    const MCard *m = &S->c[i];
    int hop = m->t < 20 ? -ui_wave(m->t * 2, 20, 3) : ((t / 12 + i) % 6 == 0 ? -2 : 0);
    ui_round_rect(x - w / 2, y - h / 2, w, h, 4, PAL_PAINT(PAINT_MINT, 0));
    ui_round_rect(x - w / 2 + 1, y - h / 2 + 1, w - 2, h - 2, 3, mg.hell ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_PAINT(PAINT_MINT, 2));
    ui_fill_ellipse(x, y + h / 2 - 5, w / 3, 2, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_PAINT(PAINT_MINT, 1));
    if (m->face >= 0) {
        const DoomoImage *img = bodyspr_image(m->face, 0, (t / 10 + i) & 1);
        int sc = h >= 52 ? 2 : 1;                    /* big cards: a big chibi */
        if (img) plat_image(x - img->w * sc / 2, y + h / 2 - 4 - img->h * sc + hop, img, sc, 0, 0);
    } else {
        fallback_face(m->face, x, y + hop, mg_big() ? 9 : 8, t);
    }
    if (((t + i * 13) % 50) < 16) ui_sparkle(x + w / 2 - 4, y - h / 2 + 4, ((t + i * 13) % 50), PAL_WHITE);
}

static void mm_draw(void)
{
    int W = mg_w(), H = mg_h(), t = mg.play_t + mg.t, i, cw = cell_w(), ch = cell_h();
    int w = cw - 4, h = ch - 4, ox = mg.ox, oy = mg.oy;
    /* Madame Moth's velvet table with a fringe */
    {
        int tx = W / 2 - S->cols * cw / 2 - 5, ty = card_y(0) - ch / 2 - 5, tw = S->cols * cw + 10, th = S->rows * ch + 10;
        uint8_t bg = mg.hell ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_PAINT(PAINT_NAVY, 1);
        /* the room only around the table (no overdraw under it) */
        plat_rect(0, 0, W, ty + oy + 8, bg);
        plat_rect(0, ty + th + oy - 8, W, H - (ty + th + oy - 8), bg);
        plat_rect(0, ty + oy + 8, tx + ox + 8, th - 16, bg);
        plat_rect(tx + tw + ox - 8, ty + oy + 8, W - (tx + tw + ox - 8), th - 16, bg);
        plat_rect(tx + 8 + ox, ty + th + oy, tw - 16, 2, PAL_RAMP(RAMP_STONE, 0));
        ui_round_rect(tx + ox, ty + oy, tw, th, 8, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_PAINT(PAINT_GRAPE, 0));
        ui_round_rect(tx + 2 + ox, ty + 2 + oy, tw - 4, th - 4, 7, mg.hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_PAINT(PAINT_VAPE_PURPLE, 0));
        for (i = tx + 4; i < tx + tw - 4; i += 4) plat_rect(i + ox, ty + th + oy + 2, 2, 3 + ((i / 4) & 1), PAL_UI_GOLD);
    }
    for (i = 0; i < 10; i++) {                   /* floating moth-dust and stars */
        uint32_t hs = doomo_hash32((uint32_t)i * 6151u + 1u);
        int ph = (int)((t / 2 + (hs >> 20)) & 31);
        if (ph < 16) ui_sparkle((int)(hs % (uint32_t)W), (int)((hs >> 8) % (uint32_t)H), ph, PAL_PAINT(PAINT_LEMON, 2));
    }
    if (S->shuffle_t) {
        int k;
        for (k = 0; k < 2; k++) {
            int c2 = k ? S->shuffle_b : S->shuffle_a;
            mg_fx(MGFX_STAR, card_x(c2), card_y(c2), 0, -6, PAL_RAMP(RAMP_LAVA, 5), 6);
        }
    }
    for (i = 0; i < S->n; i++) {
        const MCard *m = &S->c[i];
        int x = card_x(i) + ox, y = card_y(i) + oy, sq;
        if (S->shuffle_t && (i == S->shuffle_a || i == S->shuffle_b)) y -= ui_wave(S->shuffle_t * 4, 32, 4);
        switch (m->st) {
        case CS_DOWN:
            card_back(x, y, w, h, t, i);
            break;
        case CS_FLIP_UP:
            sq = m->t < 4 ? w * (4 - m->t) / 4 : w * (m->t - 4) / 4;
            if (m->t < 4) card_back(x, y, DOOMO_MAX(sq, 2), h, t, i);
            else card_face_bg(x, y, DOOMO_MAX(sq, 2), h, is_biter(m->face));
            break;
        case CS_FLIP_DOWN:
            sq = m->t < 4 ? w * (4 - m->t) / 4 : w * (m->t - 4) / 4;
            if (m->t < 4) card_face_bg(x, y, DOOMO_MAX(sq, 2), h, is_biter(m->face));
            else card_back(x, y, DOOMO_MAX(sq, 2), h, t, i);
            break;
        case CS_MATCHED:
            matched(i, x, y, w, h, t);
            break;
        default:
            break;
        }
    }
    /* lifted face-up cards go on top */
    for (i = 0; i < S->n; i++) if (S->c[i].st == CS_UP) lifted(i, t);
    /* cursor */
    {
        int x = card_x(S->cur) + ox, y = card_y(S->cur) + oy, b = ui_wave(t, 20, 1);
        ui_frame(x - w / 2 - 2 - b, y - h / 2 - 2 - b, w + 4 + b * 2, h + 4 + b * 2, PAL_UI_GOLD);
        ui_frame(x - w / 2 - 1 - b, y - h / 2 - 1 - b, w + 2 + b * 2, h + 2 + b * 2, PAL_WHITE);
        ui_cursor(x - w / 2 - 10, y - 3);
    }
}

static void mm_art(int cx, int cy, int t)
{
    int ph = (t / 3) % 16, w = ph < 8 ? 18 * (8 - ph) / 8 : 18 * (ph - 8) / 8;
    if (w < 2) w = 2;
    if ((t / 24) & 1) {
        ui_round_rect(cx - w / 2, cy - 12, w, 24, 3, PAL_PAINT(PAINT_GRAPE, 1));
        ui_round_rect(cx - w / 2 + 1, cy - 11, w - 2, 22, 2, PAL_PAINT(PAINT_LAVENDER, 1));
    } else {
        ui_round_rect(cx - w / 2, cy - 12, w, 24, 3, PAL_RAMP(RAMP_BONE, 4));
        ui_round_rect(cx - w / 2 + 1, cy - 11, w - 2, 22, 2, PAL_UI_CREAM);
        if (w > 12) ui_heart(cx, cy, 4, PAL_PAINT(PAINT_ROSE, 1));
    }
}

static uint32_t mm_bot(void)
{
    /* perfect memory, a little slow: remember every face that was shown */
    int i, j, target = -1;
    if (S->wait) return 0;
    if (S->first != 0xFF) {
        for (i = 0; i < S->n; i++)
            if (i != S->first && S->c[i].st == CS_DOWN && S->c[i].face == S->c[S->first].face) { target = i; break; }
        if (target < 0 || mg_rand(100) < 20 + mg.tier * 10) {
            for (j = 0; j < S->n; j++) if (j != S->first && S->c[j].st == CS_DOWN) { target = j; break; }
        }
    } else {
        for (i = 0; i < S->n && target < 0; i++) if (S->c[i].st == CS_DOWN) target = i;
    }
    if (target < 0) return 0;
    if (target != S->cur) {
        int tc = target % S->cols, cc = S->cur % S->cols;
        if ((mg.play_t & 3) != 0) return 0;
        if (tc < cc) return BTN_LEFT;
        if (tc > cc) return BTN_RIGHT;
        return target / S->cols < S->cur / S->cols ? BTN_UP : BTN_DOWN;
    }
    return (mg.play_t & 3) == 0 ? BTN_A : 0;
}

const MgGame mg_game_memory = {
    "MEMORY MATCH", "Madame Moth's table",
    "Find the pairs! Careful: Hellvile cards bite when you miss.",
    STR_ICON_ARROW_U STR_ICON_ARROW_D STR_ICON_ARROW_L STR_ICON_ARROW_R " move  " STR_ICON_A " flip",
    MG_MEMORY, PROG_HS_MEMORY, SONG_FESTIVAL, 0, 201,      /* Madame Moth */
    { MG_MEMORY_B, MG_MEMORY_S, MG_MEMORY_G }, 12, 90 * 30,
    { PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_NAVY, 2), PAL_PAINT(PAINT_GRAPE, 1) },
    mm_start, mm_update, mm_draw, mm_art, mm_bot
};

MG_DEFINE_SCENE(SCENE_MG_MEMORY, "MG_MEMORY", mg_game_memory);
