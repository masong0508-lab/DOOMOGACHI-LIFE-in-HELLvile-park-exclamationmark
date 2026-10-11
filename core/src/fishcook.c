/* fishcook.c - shared glue of the Lake of Fire fishing and the Brimstone
 * Bites kitchen (owner: fishcook): the save chunk 'FISH', the activities
 * registry table, the code-granted unlocks, one particle pool for both
 * scenes and a few drawing helpers. See fishcook_int.h. */
#include "fishcook_int.h"

DOOMO_STATIC_ASSERT(PROG_COUNT <= 32, progress_fits_town);
DOOMO_STATIC_ASSERT(ITEM_COUNT <= DOOMO_MAX_ITEMS, items_fit_inventory2);

FcData fc;
DoomoRng fc_rng;

int fc_rand(int n) { return n > 0 ? rng_range(&fc_rng, n) : 0; }

/* ====================================================================== */
/* the activities (activities.h)                                           */
static const DoomoActivity fc_activities[] = {
    { "Go Fishing", "Cast a line into the bubbly lake!", ACTLOC_LAKE,
      ACTF_FREEPLAY | ACTF_STORY, ICON_DROP, 1, ACT_ALWAYS, &SCENE_FISHING, 0 },
    { "Fishdex", "Every fish you caught, and records.", ACTLOC_LAKE,
      ACTF_FREEPLAY | ACTF_STORY, ICON_STAR, 1, ACT_ALWAYS, &SCENE_FISHING, FISH_ARG_DEX },
    { "Cook with Granny", "Chop, stir and bake in the kitchen.", ACTLOC_CAFE,
      ACTF_FREEPLAY | ACTF_STORY, ICON_FOOD, 1, ACT_ALWAYS, &SCENE_COOKING, 0 },
    { "Serve Customers", "Feed today's hungry regulars.", ACTLOC_CAFE,
      ACTF_FREEPLAY | ACTF_STORY, ICON_HEART, 1, ACT_ALWAYS, &SCENE_COOKING, COOK_ARG_COUNTER },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_extra = fc_activities;

/* ====================================================================== */
/* the save chunk 'FISH'                                                    */
#define FCX_FIXED 32                            /* counters + customers    */
#define FCX_SIZE (FISH_COUNT * 4 + RECIPE_COUNT + FCX_FIXED)

static uint32_t fcx_size(void) { return FCX_SIZE; }

static uint8_t *put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    return p + 2;
}

static uint8_t *put32(uint8_t *p, uint32_t v)
{
    p = put16(p, v & 0xFFFF);
    return put16(p, v >> 16);
}

static uint32_t get16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t get32(const uint8_t *p) { return get16(p) | (get16(p + 2) << 16); }

static uint32_t fcx_write(uint8_t *dst, uint32_t cap)
{
    uint8_t *p = dst;
    int i;
    if (cap < FCX_SIZE) return 0;
    for (i = 0; i < FISH_COUNT; i++) p = put16(p, fc.record[i]);
    memcpy(p, fc.caught, FISH_COUNT); p += FISH_COUNT;
    memcpy(p, fc.cooler, FISH_COUNT); p += FISH_COUNT;
    memcpy(p, fc.grade, RECIPE_COUNT); p += RECIPE_COUNT;
    *p++ = fc.rod;
    *p++ = fc.bait;
    p = put16(p, fc.cooked);
    p = put16(p, fc.perfects);
    p = put16(p, fc.burnt);
    p = put16(p, fc.released);
    p = put16(p, fc.sold);
    p = put16(p, fc.served);
    p = put32(p, fc.cust_day);
    for (i = 0; i < FC_CUSTOMERS; i++) p = put16(p, (uint16_t)fc.cust[i]);
    *p++ = fc.cust_served;
    *p++ = fc.feats;
    while (p < dst + FCX_SIZE) *p++ = 0;        /* reserved               */
    return FCX_SIZE;
}

static void fcx_reset(void)
{
    int i;
    memset(&fc, 0, sizeof(fc));
    for (i = 0; i < FC_CUSTOMERS; i++) fc.cust[i] = -1;
}

static int fcx_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    const uint8_t *p = src;
    int i;
    if (version < 1 || len < FCX_SIZE) return 0;
    fcx_reset();
    for (i = 0; i < FISH_COUNT; i++, p += 2) fc.record[i] = (uint16_t)get16(p);
    memcpy(fc.caught, p, FISH_COUNT); p += FISH_COUNT;
    memcpy(fc.cooler, p, FISH_COUNT); p += FISH_COUNT;
    memcpy(fc.grade, p, RECIPE_COUNT); p += RECIPE_COUNT;
    fc.rod = *p++;
    fc.bait = *p++;
    fc.cooked = (uint16_t)get16(p); p += 2;
    fc.perfects = (uint16_t)get16(p); p += 2;
    fc.burnt = (uint16_t)get16(p); p += 2;
    fc.released = (uint16_t)get16(p); p += 2;
    fc.sold = (uint16_t)get16(p); p += 2;
    fc.served = (uint16_t)get16(p); p += 2;
    fc.cust_day = get32(p); p += 4;
    for (i = 0; i < FC_CUSTOMERS; i++, p += 2) {
        int id = (int16_t)get16(p);
        fc.cust[i] = (int16_t)((id >= 0 && id < DOOMO_MAX_RESIDENTS) ? id : -1);
    }
    fc.cust_served = *p++;
    fc.feats = *p++;
    /* sanitise */
    for (i = 0; i < FISH_COUNT; i++) if (fc.cooler[i] > 99) fc.cooler[i] = 99;
    for (i = 0; i < RECIPE_COUNT; i++) if (fc.grade[i] > GRADE_PERFECT) fc.grade[i] = GRADE_PERFECT;
    if (fc.rod >= ROD_COUNT) fc.rod = ROD_TWIG;
    if (fc.bait >= BAIT_COUNT) fc.bait = BAIT_NONE;
    return 1;
}

static const DoomoSaveExt fc_ext = {
    SAVE_EXT_TAG('F', 'I', 'S', 'H'), 1, fcx_size, fcx_write, fcx_read, fcx_reset
};
const DoomoSaveExt *const doomo_save_ext_fishcook = &fc_ext;

/* ====================================================================== */
/* code-granted unlocks                                                     */
typedef struct { uint16_t id; uint8_t kind, arg; } FcRule;
enum { RULE_COOKED, RULE_PERFECTS, RULE_BURNT, RULE_DEX, RULE_RECIPE_DONE };
static const FcRule fc_rules[] = {
    { FC_UL_RECIPE0 + 0, RULE_COOKED, 3 },               /* Candy Eyeballs   */
    { FC_UL_RECIPE0 + 3, RULE_RECIPE_DONE, 4 },          /* Fish Ramen       */
    { FC_UL_RECIPE0 + 4, RULE_DEX, FISH_CURSED_COD },    /* Cod Chowder      */
    { FC_UL_RECIPE0 + 5, RULE_DEX, FISH_LAVA_EEL },      /* Eel Steak        */
    { FC_UL_RECIPE0 + 6, RULE_COOKED, 5 },               /* Hellfire Curry   */
    { FC_UL_RECIPE0 + 7, RULE_COOKED, 8 },               /* Pickle Pizza     */
    { FC_UL_RECIPE0 + 8, RULE_COOKED, 12 },              /* Grave Dirt Cake  */
    { FC_UL_RECIPE0 + 9, RULE_DEX, FISH_SCREAMING_CARP },/* Eyeball Jelly    */
    { FC_UL_RECIPE0 + 10, RULE_PERFECTS, 2 },            /* Lava Cake        */
    { FC_UL_RECIPE0 + 11, RULE_PERFECTS, 5 },            /* Angel Cake       */
    { FC_UL_RECIPE0 + 12, RULE_BURNT, 1 },               /* Charred Mystery  */
    { FC_UL_RECIPE0 + 13, RULE_DEX, FISH_MARSHMALLOW_MINNOW },
    { FC_UL_RECIPE0 + 14, RULE_DEX, FISH_STARLIGHT_SUNFISH },
    { FC_UL_RECIPE0 + 15, RULE_DEX, FISH_VAPULA_SOCK },  /* Sock Stew        */
    { FC_UL_GOLD_STAR, RULE_PERFECTS, 1 },
    { FC_UL_DISASTER, RULE_BURNT, 3 },
};

static int rule_holds(const FcRule *r)
{
    switch (r->kind) {
    case RULE_COOKED: return fc.cooked >= r->arg;
    case RULE_PERFECTS: return fc.perfects >= r->arg;
    case RULE_BURNT: return fc.burnt >= r->arg;
    case RULE_DEX: return fish_dex_has(r->arg);
    case RULE_RECIPE_DONE: return r->arg < RECIPE_COUNT && fc.grade[r->arg] > GRADE_NONE;
    default: return 0;
    }
}

void fc_check_unlocks(void)
{
    int i, all = 1, night = 1, legend = 1, chef = 1, big = 0;
    for (i = 0; i < DOOMO_COUNTOF(fc_rules); i++)
        if (!unlock_has(fc_rules[i].id) && rule_holds(&fc_rules[i])) unlock_grant(fc_rules[i].id);
    for (i = 0; i < FISH_COUNT; i++) {
        int have = fish_dex_has(i);
        if (!have) all = 0;
        if (fish_info[i].time == FT_NIGHT && !have) night = 0;
        if (fish_info[i].rarity == FR_LEGEND && !have) legend = 0;
        if (fc.record[i] >= 300) big = 1;
    }
    for (i = 0; i < RECIPE_COUNT; i++) if (fc.grade[i] < GRADE_PERFECT) chef = 0;
    if (all) unlock_grant(FC_UL_MASTER);
    if (night) unlock_grant(FC_UL_NIGHT_OWL);
    if (legend) unlock_grant(FC_UL_LEGEND);
    for (i = 0; i < FISH_COUNT; i++)
        if (fish_info[i].rarity == FR_LEGEND && fish_dex_has(i)) unlock_grant(FC_UL_ROD_GOLD);
    if (big) unlock_grant(FC_UL_WHOPPER);
    if (chef) unlock_grant(FC_UL_HEAD_CHEF);
    if (fc.released >= 10) unlock_grant(FC_UL_RELEASE);
    if (fc.sold >= 30) unlock_grant(FC_UL_MONGER);
    if (fc.feats & FEAT_FAV_SERVED) unlock_grant(FC_UL_REGULAR);
    if (fc.feats & FEAT_FULL_HOUSE) unlock_grant(FC_UL_FULL_HOUSE);
}

/* ====================================================================== */
/* particles + popups                                                       */
#define FC_FX_MAX 40
#define FC_POP_MAX 4
typedef struct {
    int16_t x, y, vx, vy;           /* 1/16 px                             */
    uint8_t kind, color, life, max;
} FcFx;
typedef struct {
    int16_t x, y;
    uint8_t t, color;
    char    text[14];
} FcPop;
/* Both live at the start of the scene scratch (FC_SCRATCH_TAG), followed
 * by the running scene's state: no .bss while neither scene runs. */
typedef struct {
    FcFx  fx[FC_FX_MAX];
    FcPop pops[FC_POP_MAX];
} FcFxPool;
DOOMO_STATIC_ASSERT(sizeof(FcFxPool) <= FC_FX_POOL_BYTES, fc_fx_pool_fits);

int fc_scratch_mine(void) { return scene_scratch_mine(FC_SCRATCH_TAG); }

void *fc_scratch_claim(uint32_t size)
{
    uint8_t *b;
    if (size > FC_STATE_MAX) return NULL;
    b = (uint8_t *)scene_scratch(FC_SCRATCH_TAG, FC_FX_POOL_BYTES + size);
    if (!b) return NULL;
    memset(b, 0, FC_FX_POOL_BYTES + size);
    return b + FC_FX_POOL_BYTES;
}

/* The pool, or NULL while the scratch is someone else's. */
static FcFxPool *pool(void)
{
    return fc_scratch_mine() ? (FcFxPool *)scene_scratch_block() : NULL;
}

void fc_fx_clear(void)
{
    FcFxPool *p = pool();
    if (p) memset(p, 0, sizeof(*p));
}

void fc_fx(int kind, int x, int y, int vx16, int vy16, uint8_t color, int life)
{
    FcFxPool *p = pool();
    FcFx *fx;
    int i, best = 0;
    if (!p) return;
    fx = p->fx;
    for (i = 0; i < FC_FX_MAX; i++) {
        if (!fx[i].life) { best = i; break; }
        if (fx[i].life < fx[best].life) best = i;
    }
    fx[best].x = (int16_t)(x * 16);
    fx[best].y = (int16_t)(y * 16);
    fx[best].vx = (int16_t)vx16;
    fx[best].vy = (int16_t)vy16;
    fx[best].kind = (uint8_t)kind;
    fx[best].color = color;
    fx[best].life = fx[best].max = (uint8_t)DOOMO_CLAMP(life, 1, 255);
}

void fc_burst(int kind, int x, int y, int n, int speed16, uint8_t color)
{
    int i;
    for (i = 0; i < n; i++) {
        int a = (i * DOOMO_ANG_FULL / DOOMO_MAX(n, 1) + fc_rand(300)) & DOOMO_ANG_MASK;
        int s = speed16 / 2 + fc_rand(speed16 / 2 + 1);
        fc_fx(kind, x, y, FIX2INT(fx_cos(a) * s), FIX2INT(fx_sin(a) * s) - speed16 / 3,
              color, 14 + fc_rand(12));
    }
}

void fc_popup(int x, int y, const char *text, uint8_t color)
{
    FcFxPool *p = pool();
    FcPop *pops;
    int i, best = 0;
    if (!p) return;
    pops = p->pops;
    for (i = 0; i < FC_POP_MAX; i++) {
        if (!pops[i].t) { best = i; break; }
        if (pops[i].t > pops[best].t) best = i;
    }
    pops[best].x = (int16_t)x;
    pops[best].y = (int16_t)y;
    pops[best].t = 1;
    pops[best].color = color;
    for (i = 0; i < (int)sizeof(pops[0].text) - 1 && text[i]; i++) pops[best].text[i] = text[i];
    pops[best].text[i] = 0;
}

void fc_fx_update(void)
{
    FcFxPool *p = pool();
    int i;
    if (!p) return;
    for (i = 0; i < FC_FX_MAX; i++) {
        FcFx *f = &p->fx[i];
        if (!f->life) continue;
        f->life--;
        f->x = (int16_t)(f->x + f->vx);
        f->y = (int16_t)(f->y + f->vy);
        switch (f->kind) {
        case FCX_STEAM: case FCX_SMOKE: case FCX_BUBBLE:
            f->vx = (int16_t)(f->vx + (fc_rand(5) - 2));
            if (f->vy > -12) f->vy--;
            break;
        case FCX_HEART: case FCX_STAR:
            if (f->vy > -10) f->vy--;
            break;
        case FCX_RING:
            break;
        default:
            f->vy = (int16_t)(f->vy + 2);            /* gravity             */
            break;
        }
    }
    for (i = 0; i < FC_POP_MAX; i++)
        if (p->pops[i].t && ++p->pops[i].t > 40) p->pops[i].t = 0;
}

int fc_fx_live(void)
{
    FcFxPool *p = pool();
    int i, n = 0;
    if (!p) return 0;
    for (i = 0; i < FC_FX_MAX; i++) n += p->fx[i].life != 0;
    return n;
}

void fc_fx_draw(int ox, int oy)
{
    FcFxPool *p = pool();
    int i;
    if (plat_draw_skipped() || !p) return;
    for (i = 0; i < FC_FX_MAX; i++) {
        const FcFx *f = &p->fx[i];
        int x, y, age;
        if (!f->life) continue;
        x = f->x / 16 + ox;
        y = f->y / 16 + oy;
        age = f->max - f->life;
        switch (f->kind) {
        case FCX_SPARK:
            ui_sparkle(x, y, (age * 16 / f->max + 2) & 15, f->color);
            break;
        case FCX_STAR:
            ui_star(x, y, f->life > 8 ? 3 : 2, f->color);
            break;
        case FCX_HEART:
            ui_heart(x, y, f->life > 10 ? 3 : 2, f->color);
            break;
        case FCX_COIN:
            ui_fill_circle(x, y, 2, PAL_UI_GOLD);
            plat_rect(x - 1, y - 1, 1, 1, PAL_WHITE);
            plat_rect(x, y - 2, 1, 5, ui_darker(PAL_UI_GOLD));
            break;
        case FCX_STEAM: case FCX_SMOKE: {
            int r = 1 + age / 6;
            if (f->life < 4) plat_checker(x - r, y - r, r * 2, r * 2, f->color);
            else ui_fill_circle(x, y, r, f->color);
            break; }
        case FCX_BUBBLE:
            ui_fill_circle(x, y, 2, f->color);
            plat_rect(x - 1, y - 1, 1, 1, PAL_WHITE);
            break;
        case FCX_RING: {
            int r = 2 + age / 2, ry = r / 3 + 1;
            ui_frame(x - r, y - ry, r * 2, ry * 2, f->color);
            break; }
        case FCX_CONFETTI:
            plat_rect(x, y, ((age >> 2) & 1) ? 2 : 1, ((age >> 2) & 1) ? 1 : 2, f->color);
            break;
        case FCX_EMBER:
            plat_rect(x, y, 1, 1, f->life > 6 ? f->color : ui_darker(f->color));
            if (f->life > 10) plat_rect(x, y + 1, 1, 1, ui_darker(f->color));
            break;
        default:                                    /* DROP, CRUMB          */
            plat_rect(x, y, 2, 2, f->color);
            plat_rect(x, y, 1, 1, ui_lighter(f->color));
            break;
        }
    }
    for (i = 0; i < FC_POP_MAX; i++) {
        const FcPop *q = &p->pops[i];
        int rise, s;
        if (!q->t) continue;
        rise = q->t < 10 ? q->t * 2 : 20 + (q->t - 10) / 3;
        s = ui_pop_scale(q->t);
        if (s < 200 && q->t < 4) continue;
        fc_textc(q->x + ox, q->y - rise + oy, q->text, q->color);
    }
}

/* ====================================================================== */
/* drawing helpers                                                          */
int fc_w(void) { return plat_screen_w(); }
int fc_h(void) { return plat_screen_h(); }
int fc_big(void) { return plat_screen_w() >= 300; }

void fc_text(int x, int y, const char *s, uint8_t c)
{
    ui_text_outline(x, y, s, c, PAL_RAMP(RAMP_STONE, 0));
}

void fc_textc(int cx, int y, const char *s, uint8_t c)
{
    fc_text(cx - ui_text_width(s) / 2, y, s, c);
}

void fc_pill(int x, int y, int w, int h, uint8_t fill)
{
    int r = h / 2;
    ui_round_rect(x, y, w, h, r, ui_darker(ui_darker(fill)));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, r - 1, fill);
    plat_rect(x + r, y + 1, w - 2 * r, 1, ui_lighter(fill));
}

void fc_money_pill(int right_x, int y)
{
    char buf[16];
    int w;
    doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN "%u", (unsigned)doomo_town.money);
    w = ui_text_width(buf) + 10;
    fc_pill(right_x - w, y, w, 12, PAL_RAMP(RAMP_CANDY, 6));
    ui_text(right_x - w + 5, y + 2, buf, PAL_RAMP(RAMP_WOOD, 1));
}

void fc_stars(int x, int y, int n, int max, int t)
{
    int i;
    for (i = 0; i < max; i++) {
        int b = (i < n) ? ui_wave(t * 2 + i * 10, 40, 1) : 0;
        ui_star(x + i * 9, y - b, 3, i < n ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 3));
    }
}

void fc_shadow(int cx, int cy, int rx)
{
    int ry = rx / 4 + 1, y;
    for (y = -ry; y <= ry; y++) {
        int hw = rx * (ry * 2 - (y < 0 ? -y : y)) / (ry * 2);
        if (hw > 0) plat_checker(cx - hw, cy + y, hw * 2, 1, PAL_RAMP(RAMP_STONE, 1));
    }
}

void fc_bubble(int x, int y, int w, const char *s, int tx, int ty, int chars)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int lines = ui_text_wrap(0, 0, w - 10, s, 0, 0), h = lines * FONT_LINE + 7, k;
    uint8_t fill = PAL_WHITE, edge = PAL_RAMP(RAMP_PASTEL_PINK, 3);
    if (plat_draw_skipped()) return;
    if (lines < 1) h = FONT_LINE + 7;
    /* tail: a stepped triangle from the bubble edge toward (tx, ty) */
    for (k = 0; k < 6; k++) {
        int px = x + w / 2 + (tx - (x + w / 2)) * k / 6;
        int py = (ty > y + h ? y + h - 1 : y) + (ty > y + h ? k : -k);
        int hw = 3 - k / 2;
        plat_rect(px - hw - 1, py, hw * 2 + 3, 1, edge);
        if (hw > 0) plat_rect(px - hw, py, hw * 2 + 1, 1, fill);
    }
    ui_round_rect(x, y, w, h, 4, edge);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, fill);
    plat_rect(x + 4, y + h - 2, w - 8, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    ui_text_wrap_n(x + 5, y + 4, w - 10, s, st->text, chars);
}

void fc_grade_stamp(int cx, int cy, int grade, int t)
{
    static const uint8_t col[5] = { 0, PAL_RAMP(RAMP_STONE, 1), PAL_PAINT(PAINT_SKY, 1),
                                    PAL_PAINT(PAINT_MINT, 0), PAL_PAINT(PAINT_CHERRY, 1) };
    const char *s = cook_grade_names[DOOMO_CLAMP(grade, 0, 4)];
    int scale = fc_big() ? 3 : 2, s256 = ui_pop_scale(t), w, r;
    uint8_t c = col[DOOMO_CLAMP(grade, 0, 4)];
    if (grade <= GRADE_NONE || plat_draw_skipped()) return;
    if (s256 < 160) scale = 1;
    w = ui_text_big_width(s, scale);
    r = w / 2 + 8;
    /* the stamp: a ring with a ribbon behind the word */
    ui_fill_ellipse(cx, cy + 1, r + 2, 9 * scale / 2 + 4, ui_darker(c));
    ui_fill_ellipse(cx, cy, r, 9 * scale / 2 + 2, PAL_WHITE);
    ui_fill_ellipse(cx, cy, r - 2, 9 * scale / 2, ui_lighter(ui_lighter(c)));
    ui_text_big(cx - w / 2, cy - 4 * scale, s, scale, c, PAL_WHITE);
    if (grade == GRADE_PERFECT) {
        ui_sparkle(cx - r, cy - 6, (t / 2) & 15, PAL_UI_GOLD);
        ui_sparkle(cx + r, cy + 4, (t / 2 + 7) & 15, PAL_UI_GOLD);
        ui_star(cx, cy - 9 * scale / 2 - 6, 4, PAL_UI_GOLD);
    }
}
