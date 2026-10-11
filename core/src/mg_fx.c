/* mg_fx.c - juice for the Fun Fair minigames (owner: minigames): a small
 * fixed pool of particles (sparks, confetti, hearts, stars, puffs, crumbs,
 * rings, drops, embers), rising score popups, screen shake, flashes and
 * hit-stop. Bounded: 40 particles + 6 popups, a few plat_rects each. */
#include "mg.h"

#define FX_MAX 40
#define POP_MAX 6

typedef struct MgFx {
    int16_t x, y;            /* 1/16 px */
    int8_t  vx, vy;          /* 1/16 px per tick */
    uint8_t kind, color, life, max;
} MgFx;

typedef struct MgPop {
    int16_t x, y, value;
    uint8_t life, color;
    const char *text;
} MgPop;

static MgFx fx[FX_MAX];
static MgPop pops[POP_MAX];
static uint8_t fx_next;

static const uint8_t confetti_cols[8] = {
    PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_MINT, 2),
    PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_LAVENDER, 2), PAL_PAINT(PAINT_TANGERINE, 2),
    PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE
};

void mg_fx_clear(void)
{
    memset(fx, 0, sizeof(fx));
    memset(pops, 0, sizeof(pops));
    fx_next = 0;
}

int mg_fx_live(void)
{
    int i, n = 0;
    for (i = 0; i < FX_MAX; i++) n += fx[i].life != 0;
    return n;
}

void mg_fx(int kind, int x, int y, int vx16, int vy16, uint8_t color, int life)
{
    MgFx *f;
    int i, slot = -1;
    for (i = 0; i < FX_MAX; i++) {
        int k = (fx_next + i) % FX_MAX;
        if (!fx[k].life) { slot = k; break; }
    }
    if (slot < 0) slot = fx_next;              /* steal the oldest-ish */
    fx_next = (uint8_t)((slot + 1) % FX_MAX);
    f = &fx[slot];
    f->x = (int16_t)DOOMO_CLAMP(x * 16, -4000, 30000);
    f->y = (int16_t)DOOMO_CLAMP(y * 16, -4000, 30000);
    f->vx = (int8_t)DOOMO_CLAMP(vx16, -127, 127);
    f->vy = (int8_t)DOOMO_CLAMP(vy16, -127, 127);
    f->kind = (uint8_t)kind;
    f->color = kind == MGFX_CONFETTI && !color ? confetti_cols[rng_range(&doomo_rng, 8)] : color;
    f->life = f->max = (uint8_t)DOOMO_CLAMP(life, 1, 255);
}

void mg_burst(int kind, int x, int y, int n, int speed16, uint8_t color)
{
    int i;
    for (i = 0; i < n; i++) {
        int a = (i * DOOMO_ANG_FULL / (n > 0 ? n : 1) + rng_range(&doomo_rng, 300)) & DOOMO_ANG_MASK;
        int sp = speed16 / 2 + rng_range(&doomo_rng, speed16 / 2 + 1);
        mg_fx(kind, x, y, FIX2INT(fx_cos(a) * sp), FIX2INT(fx_sin(a) * sp) - (kind == MGFX_CONFETTI ? 12 : 0),
              color, 14 + rng_range(&doomo_rng, 12));
    }
}

void mg_confetti_rain(int n)
{
    int i, w = mg_w();
    if (opt_get(OPT_CONFETTI_AMOUNT) == 2) n = n * 3 / 2;
    for (i = 0; i < n; i++)
        mg_fx(MGFX_CONFETTI, rng_range(&doomo_rng, w), -rng_range(&doomo_rng, 30),
              rng_between(&doomo_rng, -10, 10), rng_between(&doomo_rng, 6, 20), 0,
              40 + rng_range(&doomo_rng, 40));
}

void mg_splat(int x, int y, int n)
{
    if (mg.hell && opt_get(OPT_GORE) == GORE_BRUTAL) {
        int i;
        for (i = 0; i < n; i++)
            mg_fx(MGFX_DROP, x, y, rng_between(&doomo_rng, -28, 28), rng_between(&doomo_rng, -40, -6),
                  PAL_RAMP(RAMP_BLOOD, 3 + rng_range(&doomo_rng, 3)), 16 + rng_range(&doomo_rng, 10));
    } else {
        mg_burst(MGFX_CONFETTI, x, y, n, 40, 0);
        mg_burst(MGFX_HEART, x, y, n / 3 + 1, 26, PAL_PAINT(PAINT_ROSE, 2));
        mg_burst(MGFX_PUFF, x, y, 2, 12, PAL_WHITE);
    }
}

void mg_popup(int x, int y, const char *text, int value, uint8_t color)
{
    int i, slot = 0, oldest = 999;
    for (i = 0; i < POP_MAX; i++) {
        if (!pops[i].life) { slot = i; break; }
        if (pops[i].life < oldest) { oldest = pops[i].life; slot = i; }
    }
    pops[slot].x = (int16_t)x;
    pops[slot].y = (int16_t)y;
    pops[slot].value = (int16_t)DOOMO_CLAMP(value, -32000, 32000);
    pops[slot].text = text;
    pops[slot].color = color;
    pops[slot].life = 34;
}

void mg_shake_add(int amount)
{
    mg.shake = (uint8_t)DOOMO_MIN(mg.shake + amount, 14);
}

void mg_flash(uint8_t color, int ticks)
{
    mg.flash_c = color;
    mg.flash_t = (uint8_t)ticks;
}

void mg_hitstop(int ticks)
{
    if (mg.freeze < ticks) mg.freeze = (uint8_t)ticks;
}

void mg_fx_update(void)
{
    int i, h16 = (mg_h() + 20) * 16;
    for (i = 0; i < FX_MAX; i++) {
        MgFx *f = &fx[i];
        if (!f->life) continue;
        f->life--;
        f->x = (int16_t)(f->x + f->vx);
        f->y = (int16_t)(f->y + f->vy);
        switch (f->kind) {
        case MGFX_SPARK: case MGFX_STAR:
            if (f->vy < 60) f->vy = (int8_t)(f->vy + 2);
            f->vx = (int8_t)(f->vx * 7 / 8);
            break;
        case MGFX_CONFETTI:
            if (f->vy < 14) f->vy = (int8_t)(f->vy + 1);
            f->vx = (int8_t)(f->vx * 15 / 16 + ((f->life >> 3) & 1 ? 1 : -1));
            break;
        case MGFX_HEART:
            f->vy = (int8_t)DOOMO_MAX(f->vy - 1, -18);
            f->vx = (int8_t)(f->vx * 7 / 8);
            break;
        case MGFX_PUFF:
            f->vx = (int8_t)(f->vx * 3 / 4);
            f->vy = (int8_t)(f->vy * 3 / 4 - 1);
            break;
        case MGFX_CRUMB: case MGFX_DROP:
            if (f->vy < 80) f->vy = (int8_t)(f->vy + 4);
            break;
        case MGFX_EMBER:
            f->vy = (int8_t)DOOMO_MAX(f->vy - 1, -24);
            f->vx = (int8_t)(((f->life >> 2) & 1) ? 6 : -6);
            break;
        default:
            break;
        }
        if (f->y > h16) f->life = 0;
    }
    for (i = 0; i < POP_MAX; i++)
        if (pops[i].life) {
            pops[i].life--;
            if (pops[i].life > 20) pops[i].y--;
            else if (pops[i].life & 1) pops[i].y--;
        }
}

void mg_fx_draw(void)
{
    int i, ox = mg.ox, oy = mg.oy;
    if (plat_draw_skipped()) return;
    for (i = 0; i < FX_MAX; i++) {
        const MgFx *f = &fx[i];
        int x, y, age;
        if (!f->life) continue;
        x = (f->x >> 4) + ox;
        y = (f->y >> 4) + oy;
        age = f->max - f->life;
        switch (f->kind) {
        case MGFX_SPARK:
            plat_rect(x, y, f->life > 6 ? 2 : 1, f->life > 6 ? 2 : 1,
                      f->life > 4 ? f->color : ui_darker(f->color));
            break;
        case MGFX_CONFETTI:
            if ((f->life >> 1) & 1) plat_rect(x, y, 3, 1, f->color);
            else plat_rect(x + 1, y - 1, 1, 3, f->color);
            break;
        case MGFX_HEART:
            ui_heart(x, y, f->life > 8 ? 3 : 2, f->color);
            break;
        case MGFX_STAR:
            ui_sparkle(x, y, (age * 2) & 15, f->color);
            break;
        case MGFX_PUFF:
            ui_fill_circle(x, y, 2 + age / 3, f->life > 6 ? f->color : PAL_GRAY_L);
            break;
        case MGFX_CRUMB:
            plat_rect(x, y, 2, 2, f->color);
            break;
        case MGFX_RING: {
            int r = 3 + age * 2, k;
            for (k = 0; k < 8; k++) {
                int a = k * 512 + age * 40;
                plat_rect(x + FIX2INT(fx_cos(a) * r), y + FIX2INT(fx_sin(a) * r), 2, 2, f->color);
            }
            break;
        }
        case MGFX_DROP:
            plat_rect(x, y, 2, 3, f->color);
            plat_rect(x, y, 1, 1, ui_lighter(f->color));
            break;
        case MGFX_EMBER:
            plat_rect(x, y, f->life & 2 ? 2 : 1, f->life & 2 ? 2 : 1, f->color);
            break;
        default:
            break;
        }
    }
    for (i = 0; i < POP_MAX; i++) {
        const MgPop *p = &pops[i];
        char buf[24];
        const char *s = buf;
        int w;
        if (!p->life || (p->life < 6 && (p->life & 1))) continue;
        if (p->text) s = p->text;
        else doomo_snprintf(buf, sizeof(buf), p->value >= 0 ? "+%d" : "%d", p->value);
        w = ui_text_width(s);
        ui_text_outline(p->x - w / 2 + ox, p->y + oy, s, p->color, PAL_RAMP(RAMP_STONE, 0));
    }
}
