/* portrait_fx.c - auras, expression effects, possession / ghost looks
 * (owner: portrait). All effects are pixel sprites drawn after the
 * outline pass, animated by the context's anim counter. */
#include "portrait_int.h"
#include "doomo/resident.h"

/* character bounds for particles (pixels) */
static int BX0, BY0, BX1, BY1, HCX, HCY, HRX, HRY;

static void bounds(PCtx *c)
{
    HCX = c->h.cx >> 4; HCY = c->h.cy >> 4;
    HRX = DOOMO_MAX(c->h.rx >> 4, 3); HRY = DOOMO_MAX(c->h.ry >> 4, 3);
    BX0 = 1; BX1 = pcv.w - 2;
    BY0 = DOOMO_MAX(HCY - HRY * 2, 1);
    BY1 = pcv.h - 2;
}

static void fxp(int x, int y, uint8_t c) { pc_fx(x, y, c); }

/* only draws on transparent pixels (behind the character) */
static void fxb(int x, int y, uint8_t c)
{
    if (x < 0 || y < 0 || x >= pcv.w || y >= pcv.h) return;
    if (!pcv.px[y * pcv.w + x]) pc_fx(x, y, c);
}

static void heart_fx(int x, int y, int big, uint8_t c, uint8_t hi)
{
    if (big) {
        fxp(x - 1, y - 1, c); fxp(x + 1, y - 1, c);
        fxp(x - 2, y, c); fxp(x - 1, y, hi); fxp(x, y, c); fxp(x + 1, y, c); fxp(x + 2, y, c);
        fxp(x - 1, y + 1, c); fxp(x, y + 1, c); fxp(x + 1, y + 1, c);
        fxp(x, y + 2, c);
        fxp(x - 2, y - 1, palette_darker(c)); fxp(x + 2, y - 1, palette_darker(c));
    } else {
        fxp(x - 1, y, c); fxp(x + 1, y, c); fxp(x, y + 1, c); fxp(x, y, c);
    }
}

static void sparkle_fx(int x, int y, int ph, uint8_t c)
{
    fxp(x, y, PAL_WHITE);
    if (ph & 1) {
        fxp(x - 1, y, c); fxp(x + 1, y, c); fxp(x, y - 1, c); fxp(x, y + 1, c);
        if (ph & 2) { fxp(x - 2, y, c); fxp(x + 2, y, c); fxp(x, y - 2, c); fxp(x, y + 2, c); }
    }
}

/* deterministic particle k position around the character */
static void part_pos(PCtx *c, int k, int salt, int *x, int *y)
{
    uint32_t h = p_hash((uint32_t)(k * 7919 + salt * 104729 + c->l->species));
    int w = BX1 - BX0, hh = BY1 - BY0;
    *x = BX0 + (int)(h % (uint32_t)(w > 0 ? w : 1));
    *y = BY0 + (int)((h >> 12) % (uint32_t)(hh > 0 ? hh : 1));
}

/* ---- auras --------------------------------------------------------------- */
void pf_aura_back(PCtx *c)
{
    int au = c->l->aura, k, x, y;
    int t = c->anim;
    bounds(c);
    if (c->creepy) {
        /* dark smoke billowing behind the possessed */
        for (k = 0; k < 10 + c->creepy * 4; k++) {
            int r;
            part_pos(c, k, 7, &x, &y);
            y -= ((t >> 2) + k * 3) % 8;
            r = 1 + (k % 3);
            {
                int dx, dy;
                for (dy = -r; dy <= r; dy++)
                    for (dx = -r; dx <= r; dx++)
                        if (dx * dx + dy * dy <= r * r)
                            fxb(x + dx, y + dy, (dx + dy < 0) ? PAL_RAMP(RAMP_VAPE, 1) : PAL_RAMP(RAMP_VAPE, 0));
            }
        }
    }
    switch (au) {
    case AURA_FLAMES:
        for (k = 0; k < 9; k++) {
            int fx = BX0 + 2 + k * (BX1 - BX0 - 4) / 8, h = 4 + ((k * 5 + (t >> 2)) % 4) * 2;
            int base = HCY + HRY + 2 - (p_abs(fx - HCX) * 1) / 3;
            for (y = 0; y < h; y++) {
                int wdt = (h - y) / 3;
                for (x = -wdt; x <= wdt; x++)
                    fxb(fx + x + ((y + k + (t >> 2)) & 1), base - y,
                        y > h - 3 ? PAL_RAMP(RAMP_LAVA, 6) : (p_abs(x) == wdt ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_LAVA, 5)));
            }
        }
        break;
    case AURA_SMOKE:
        for (k = 0; k < 8; k++) {
            int r = 2 + (k & 1);
            part_pos(c, k, 3, &x, &y);
            y = BY1 - ((y + (t >> 1) + k * 5) % (BY1 - BY0));
            {
                int dx, dy;
                for (dy = -r; dy <= r; dy++)
                    for (dx = -r; dx <= r; dx++)
                        if (dx * dx + dy * dy <= r * r)
                            fxb(x + dx, y + dy, dx + dy < 0 ? PAL_GRAY_L : PAL_GRAY);
            }
        }
        break;
    case AURA_GLITCH:
        for (k = 0; k < 5; k++) {
            part_pos(c, k + (t >> 3), 11, &x, &y);
            {
                int len = 3 + (k % 4) * 2, i;
                for (i = 0; i < len; i++)
                    fxb(x + i, y, (k & 1) ? PAL_PAINT(PAINT_CYAN, 2) : PAL_PAINT(PAINT_MAGENTA, 2));
            }
        }
        break;
    default:
        break;
    }
}

static void glitch_rows(PCtx *c, int n, int strength)
{
    int k, i;
    uint8_t row[PMAXW];
    for (k = 0; k < n; k++) {
        uint32_t h = p_hash((uint32_t)(k * 31 + (c->anim >> 3) * 977 + 5));
        int y = BY0 + (int)(h % (uint32_t)DOOMO_MAX(BY1 - BY0, 1));
        int sh = (int)((h >> 8) % (uint32_t)(strength * 2 + 1)) - strength;
        int w = pcv.w;
        if (sh == 0) sh = 1;
        if (y < 0 || y >= pcv.h) continue;
        memcpy(row, pcv.px + y * w, (size_t)w);
        for (i = 0; i < w; i++) {
            int src = i - sh;
            uint8_t v = (src >= 0 && src < w) ? row[src] : 0;
            pcv.px[y * w + i] = v;
        }
        /* chromatic fringe */
        for (i = 0; i < w; i++) {
            if (pcv.px[y * w + i] && (i == 0 || !pcv.px[y * w + i - 1]))
                pcv.px[y * w + i] = PAL_PAINT(PAINT_CYAN, 1);
        }
    }
}

void pf_aura_front(PCtx *c)
{
    int au = c->l->aura, k, x, y;
    int t = c->anim;
    bounds(c);
    switch (au) {
    case AURA_SPARKLES:
        for (k = 0; k < 6; k++) {
            part_pos(c, k, 1, &x, &y);
            if (!pc_get(x, y) || (k & 1)) sparkle_fx(x, y, ((t >> 3) + k) & 3, (k % 3) ? PAL_UI_GOLD : PAL_PAINT(PAINT_SKY, 2));
        }
        break;
    case AURA_FLAMES:
        for (k = 0; k < 4; k++) {
            part_pos(c, k, 2, &x, &y);
            y -= (t >> 2) % 5;
            fxp(x, y, PAL_RAMP(RAMP_LAVA, 6));
            fxp(x, y + 1, PAL_RAMP(RAMP_LAVA, 4));
        }
        break;
    case AURA_FLIES:
        for (k = 0; k < 5; k++) {
            int ph = ((t >> 2) + k * 3) & 7;
            part_pos(c, k, 4, &x, &y);
            x += (ph < 4 ? ph : 8 - ph) - 2;
            y += ((ph & 3) == 1) - ((ph & 3) == 3);
            fxp(x, y, PAL_PAINT(PAINT_JET, 1));
            fxp(x + 1, y, PAL_PAINT(PAINT_JET, 2));
            fxp(x, y - 1, (ph & 1) ? PAL_GRAY_L : PAL_WHITE);
            fxp(x + 1, y - 1, (ph & 1) ? PAL_WHITE : PAL_GRAY_L);
        }
        break;
    case AURA_HEARTS:
        for (k = 0; k < 4; k++) {
            part_pos(c, k, 5, &x, &y);
            y = BY1 - ((BY1 - y + (t >> 2) + k * 4) % (BY1 - BY0));
            heart_fx(x, y, k & 1, (k & 2) ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_WHITE);
        }
        break;
    case AURA_SMOKE:
        for (k = 0; k < 3; k++) {
            part_pos(c, k, 6, &x, &y);
            fxp(x, y, PAL_GRAY_L); fxp(x + 1, y, PAL_GRAY_L); fxp(x, y - 1, PAL_WHITE);
        }
        break;
    case AURA_BUBBLES:
        for (k = 0; k < 6; k++) {
            int r = 1 + (k % 3 == 0);
            part_pos(c, k, 8, &x, &y);
            y = BY1 - ((BY1 - y + (t >> 2) + k * 6) % (BY1 - BY0));
            if (r == 1) {
                fxp(x, y - 1, PAL_PAINT(PAINT_SKY, 1)); fxp(x - 1, y, PAL_PAINT(PAINT_SKY, 1));
                fxp(x + 1, y, PAL_PAINT(PAINT_SKY, 1)); fxp(x, y + 1, PAL_PAINT(PAINT_SKY, 1));
                fxp(x - 1, y - 1, PAL_WHITE);
            } else {
                int a;
                for (a = 0; a < 256; a += 32)
                    fxp(x + (p_cos(a) * 2 + 128) / 256, y + (p_sin(a) * 2 + 128) / 256, PAL_PAINT(PAINT_CYAN, 1));
                fxp(x - 1, y - 1, PAL_WHITE);
            }
        }
        break;
    case AURA_GLITCH:
        glitch_rows(c, 3, 2);
        break;
    default:
        break;
    }
}

/* ---- expression effects ---------------------------------------------------- */
static void tear(int x, int y, int len)
{
    int k;
    for (k = 0; k < len; k++) fxp(x, y + k, k == len - 1 ? PAL_PAINT(PAINT_SKY, 1) : PAL_PAINT(PAINT_CYAN, 2));
    fxp(x - 1, y + len - 1, PAL_PAINT(PAINT_SKY, 1));
    fxp(x, y + len, PAL_PAINT(PAINT_SKY, 0));
}

static void sweat(int x, int y)
{
    fxp(x, y, PAL_PAINT(PAINT_SKY, 2));
    fxp(x, y + 1, PAL_PAINT(PAINT_SKY, 1));
    fxp(x - 1, y + 2, PAL_PAINT(PAINT_SKY, 1));
    fxp(x, y + 2, PAL_PAINT(PAINT_CYAN, 2));
    fxp(x + 1, y + 2, PAL_PAINT(PAINT_SKY, 0));
    fxp(x, y + 3, PAL_PAINT(PAINT_SKY, 0));
}

static void eye_px(PCtx *c, int s, int *x, int *y)
{
    int X, Y, ery;
    ph_eye_geom(c, c->l->eye_count == 1 ? 0 : s, &X, &Y, NULL, &ery);
    *x = X >> 4;
    *y = (Y + ery) >> 4;
}

void pf_expression_fx(PCtx *c)
{
    int t = c->anim, x, y, k;
    int big = c->h.lod == LOD_BIG, mid = c->h.lod == LOD_MID;
    bounds(c);
    if (c->h.yaw >= 3) return;
    switch (c->expr) {
    case EXPR_SAD:
        eye_px(c, -1, &x, &y);
        tear(x, y + 1, big ? 2 + ((t >> 2) & 3) : 1);
        if (big) { eye_px(c, 1, &x, &y); tear(x, y + 1, 1 + ((t >> 2) + 2) % 3); }
        break;
    case EXPR_ANGRY: {
        /* anger vein */
        int ax = HCX + HRX * 6 / 10, ay = HCY - HRY * 6 / 10;
        uint8_t r = PAL_PAINT(PAINT_CHERRY, 1), r2 = PAL_PAINT(PAINT_CHERRY, 2);
        if (!big) { fxp(ax, ay, r); fxp(ax + 1, ay - 1, r); break; }
        fxp(ax - 1, ay - 2, r); fxp(ax - 1, ay - 1, r2); fxp(ax - 2, ay - 1, r);
        fxp(ax + 1, ay - 2, r); fxp(ax + 1, ay - 1, r2); fxp(ax + 2, ay - 1, r);
        fxp(ax - 1, ay + 2, r); fxp(ax - 1, ay + 1, r2); fxp(ax - 2, ay + 1, r);
        fxp(ax + 1, ay + 2, r); fxp(ax + 1, ay + 1, r2); fxp(ax + 2, ay + 1, r);
        /* steam puffs */
        k = (t >> 3) & 1;
        fxp(HCX - HRX - 1 - k, HCY - 1, PAL_WHITE); fxp(HCX - HRX - 2 - k, HCY - 2, PAL_GRAY_L);
        fxp(HCX + HRX + 1 + k, HCY - 1, PAL_WHITE); fxp(HCX + HRX + 2 + k, HCY - 2, PAL_GRAY_L);
        break; }
    case EXPR_LOVE:
        for (k = 0; k < (big ? 3 : 1); k++) {
            int hx = HCX + (k - 1) * HRX * 8 / 10, hy = HCY - HRY - 2 - ((t >> 2) + k * 3) % 5;
            heart_fx(hx, hy, big && k != 1, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
        }
        break;
    case EXPR_SHOCK:
        sweat(HCX + HRX - 1, HCY - HRY / 2);
        if (big) {
            fxp(HCX - HRX - 2, HCY - HRY, c->skin.line); fxp(HCX - HRX - 3, HCY - HRY - 1, c->skin.line);
            fxp(HCX + HRX + 2, HCY - HRY, c->skin.line); fxp(HCX + HRX + 3, HCY - HRY - 1, c->skin.line);
            fxp(HCX, HCY - HRY - 3, c->skin.line); fxp(HCX, HCY - HRY - 4, c->skin.line);
        }
        break;
    case EXPR_SLEEPY: {
        int zx = HCX + HRX - 1 + ((t >> 3) & 1), zy = HCY - HRY - ((t >> 3) & 3);
        uint8_t zc = PAL_PAINT(PAINT_LAVENDER, 0);
        if (big || mid) {
            fxp(zx, zy, zc); fxp(zx + 1, zy, zc); fxp(zx + 2, zy, zc);
            fxp(zx + 1, zy + 1, zc);
            fxp(zx, zy + 2, zc); fxp(zx + 1, zy + 2, zc); fxp(zx + 2, zy + 2, zc);
            if (big) {
                zx += 4; zy -= 4;
                fxp(zx, zy, zc); fxp(zx + 1, zy, zc); fxp(zx + 1, zy + 1, zc); fxp(zx, zy + 2, zc); fxp(zx + 1, zy + 2, zc);
            }
        } else {
            fxp(zx, zy, zc); fxp(zx + 1, zy, zc);
        }
        /* snot bubble */
        if (big) {
            int bx = HCX + 3, by = HCY + HRY / 3, r = 1 + ((t >> 3) & 1);
            int a;
            for (a = 0; a < 256; a += 32)
                fxp(bx + (p_cos(a) * r + 128) / 256, by + (p_sin(a) * r + 128) / 256, PAL_PAINT(PAINT_SKY, 2));
            fxp(bx - 1, by - 1, PAL_WHITE);
        }
        break; }
    case EXPR_SICK: {
        /* sickly green face */
        int i, n = pcv.w * pcv.h;
        for (i = 0; i < n; i++)
            if (pcv.id[i] == c->head_id && pcv.px[i])
                pcv.px[i] = doomo_tintmap[TINT_GREEN][pcv.px[i]];
        sweat(HCX - HRX + 1, HCY - HRY / 2);
        break; }
    case EXPR_DEAD: {
        /* a little soul wisp floats away */
        int sx = HCX + HRX / 2, sy = HCY - HRY - 3 - ((t >> 3) & 1);
        if (big) {
            fxp(sx, sy, PAL_WHITE); fxp(sx + 1, sy, PAL_WHITE); fxp(sx - 1, sy, PAL_WHITE);
            fxp(sx, sy - 1, PAL_WHITE); fxp(sx, sy + 1, PAL_PAINT(PAINT_SKY, 2)); fxp(sx + 1, sy + 1, PAL_WHITE);
            fxp(sx + 2, sy + 2, PAL_PAINT(PAINT_SKY, 2));
            fxp(sx - 1, sy, PAL_PAINT(PAINT_JET, 1)); fxp(sx + 1, sy, PAL_PAINT(PAINT_JET, 1));
        }
        break; }
    case EXPR_HAPPY:
        if (big) {
            sparkle_fx(HCX - HRX - 1, HCY - HRY + 2, (t >> 3) & 3, PAL_UI_GOLD);
            sparkle_fx(HCX + HRX + 1, HCY - HRY / 2, ((t >> 3) + 2) & 3, PAL_UI_GOLD);
        }
        break;
    case EXPR_TALK:
        if (big && (t & 4)) {
            fxp(HCX + HRX + 2, HCY + HRY / 3, c->skin.line);
            fxp(HCX + HRX + 3, HCY + HRY / 3 - 1, c->skin.line);
            fxp(HCX + HRX + 2, HCY + HRY / 3 + 2, c->skin.line);
            fxp(HCX + HRX + 3, HCY + HRY / 3 + 3, c->skin.line);
        }
        break;
    default:
        break;
    }
}

/* ---- possession / ghost ----------------------------------------------------- */
void pf_status(PCtx *c)
{
    int k, x, y;
    bounds(c);
    if (c->creepy && c->h.yaw < 3) {
        int big = c->h.lod == LOD_BIG;
        uint8_t vein = c->creepy > 1 ? PAL_PAINT(PAINT_JET, 1) : PAL_PAINT(PAINT_GRAPE, 0);
        int s;
        for (s = -1; s <= 1; s += 2) {
            int ex, ey;
            eye_px(c, s, &ex, &ey);
            if (c->l->eye_count == 1 && s > 0) break;
            /* drippy black tears */
            {
                int len = big ? 3 + ((c->anim >> 3) + (s > 0)) % 3 : 1;
                for (k = 0; k < len; k++) fxp(ex, ey + k, PAL_PAINT(PAINT_JET, 0));
                if (big) { fxp(ex, ey + len, PAL_PAINT(PAINT_JET, 1)); fxp(ex - 1, ey + len - 1, PAL_PAINT(PAINT_JET, 1)); }
            }
            /* veins crawling from the eyes */
            if (big) {
                int vx = ex + s * 3, vy = ey - 3;
                pc_mask_none();
                pc_mask_add(c->head_id);
                pc_line(vx, vy, vx + s * 3, vy - 2, vein);
                pc_line(vx + s * 3, vy - 2, vx + s * 4, vy - 5, vein);
                pc_line(vx + s * 3, vy - 2, vx + s * 6, vy - 1, vein);
                pc_line(vx, vy + 3, vx + s * 3, vy + 5, vein);
                pc_mask_none();
            }
        }
        if (c->creepy > 1 && big) {
            /* the soul is cracking */
            pc_mask_none();
            pc_mask_add(c->head_id);
            pc_line(HCX - 2, HCY - HRY + 1, HCX + 1, HCY - HRY / 2, PAL_PAINT(PAINT_JET, 1));
            pc_line(HCX + 1, HCY - HRY / 2, HCX - 1, HCY - 1, PAL_PAINT(PAINT_JET, 1));
            pc_line(HCX + 1, HCY - HRY / 2, HCX + 4, HCY - HRY / 2 + 2, PAL_PAINT(PAINT_JET, 1));
            pc_mask_none();
        }
        /* sputtering glitch + red eye glow particles */
        glitch_rows(c, c->creepy > 1 ? 4 : 2, c->creepy > 1 ? 2 : 1);
        for (k = 0; k < 3; k++) {
            part_pos(c, k + (c->anim >> 3), 21, &x, &y);
            fxb(x, y, PAL_PAINT(PAINT_BLOOD, 2));
        }
    }
}

/* map everything to a pale translucent cyan */
void pf_recolor_ghost(void)
{
    static const uint8_t ramp[6] = {
        PAL_PAINT(PAINT_TEAL, 0), PAL_PAINT(PAINT_TEAL, 2), PAL_PAINT(PAINT_CYAN, 2),
        PAL_PAINT(PAINT_ECTOPLASM, 2), PAL_PAINT(PAINT_WHITE, 1), PAL_WHITE
    };
    int i, n = pcv.w * pcv.h, h = pcv.h;
    for (i = 0; i < n; i++) {
        uint8_t c = pcv.px[i];
        int lum, y;
        if (!c) continue;
        lum = palette_luma(c);
        c = ramp[lum < 50 ? 0 : (lum < 100 ? 1 : (lum < 150 ? 2 : (lum < 190 ? 3 : (lum < 228 ? 4 : 5))))];
        y = i / pcv.w;
        /* fade out toward the bottom: translucent wisp */
        if (y > h * 78 / 100 && (((i % pcv.w) + y) & 1)) c = 0;
        else if (y > h * 90 / 100 && ((i % pcv.w) & 1)) c = 0;
        pcv.px[i] = c;
    }
}

void pf_recolor_creepy(int level)
{
    /* used for SOUL_LOST: drain the colour from the whole figure */
    int i, n = pcv.w * pcv.h;
    for (i = 0; i < n; i++) {
        uint8_t c = pcv.px[i];
        if (!c || pcv.id[i] == PID_FX) continue;
        if (level > 1) c = doomo_tintmap[TINT_NIGHT][c];
        pcv.px[i] = c;
    }
}
