/* portrait.c - public entry points of the resident renderer: bust
 * portraits, overworld chibis, creator figures, third-person fighters and
 * the cached DoomoImage accessors (owner: portrait).
 *
 * Every image is drawn from scratch from the DoomoLook (no stored art),
 * integer-only, into the caller's buffer. The cache keeps rendered images
 * in a small block pool (48 x 384 bytes, LRU) and keys them by resident,
 * a hash of the look, status, expression and animation phase, so a uid
 * only changes when the pixels do.
 */
#include "portrait_int.h"
#include "doomo/resident.h"
#include "doomo/town.h"
#include "doomo/game.h"

/* ====================================================================== */
/* context                                                                 */
static void sanitize(DoomoLook *l)
{
#define FIXE(f, n) if (l->f >= (n)) l->f = 0
#define FIXS(f, m) if (l->f > (m)) l->f = (m)
    FIXE(species, SPECIES_COUNT); FIXE(body, BODY_COUNT); FIXE(head_shape, HEAD_COUNT);
    FIXE(eye_style, EYE_COUNT); FIXE(brow_style, BROW_COUNT); FIXE(nose_style, NOSE_COUNT);
    FIXE(mouth_style, MOUTH_COUNT); FIXE(hair_style, HAIR_COUNT); FIXE(facial_hair, FHAIR_COUNT);
    FIXE(ears, EARS_COUNT); FIXE(horns, HORNS_COUNT); FIXE(tail, TAIL_COUNT);
    FIXE(wings, WINGS_COUNT); FIXE(halo, HALO_COUNT); FIXE(marking, MARK_COUNT);
    FIXE(top, TOP_COUNT); FIXE(bottom, BOTTOM_COUNT); FIXE(shoes, SHOES_COUNT);
    FIXE(hat, HAT_COUNT); FIXE(accessory, ACC_COUNT); FIXE(aura, AURA_COUNT);
    FIXE(pattern, PATTERN_COUNT);
    FIXS(height, 15); FIXS(build, 15); FIXS(head_size, 15); FIXS(eye_size, 15);
    FIXS(eye_spacing, 15); FIXS(eye_height, 15); FIXS(eye_tilt, 15);
    FIXS(brow_height, 15); FIXS(nose_size, 15); FIXS(mouth_size, 15);
    FIXS(fangs, 3); FIXS(horn_size, 15);
    if (l->eye_count < 1) l->eye_count = 1;
    if (l->eye_count > 3) l->eye_count = 3;
    if (l->skin >= 8 + PAL_PAINT_COUNT) l->skin = SKIN_FAIR;
    l->eye_color %= PAL_PAINT_COUNT; l->hair_color %= PAL_PAINT_COUNT;
    if (l->hair_highlight != LOOK_NO_COLOR) l->hair_highlight %= PAL_PAINT_COUNT;
    l->horn_color %= PAL_PAINT_COUNT; l->marking_color %= PAL_PAINT_COUNT;
    l->top_color %= PAL_PAINT_COUNT; l->bottom_color %= PAL_PAINT_COUNT;
    l->shoes_color %= PAL_PAINT_COUNT; l->hat_color %= PAL_PAINT_COUNT;
    l->accessory_color %= PAL_PAINT_COUNT; l->pattern_color %= PAL_PAINT_COUNT;
#undef FIXE
#undef FIXS
}

static void creepy_skin(PMat *m, uint8_t base, int level)
{
    int lum = palette_luma(base), s;
    s = lum < 60 ? 2 : (lum < 100 ? 3 : (lum < 150 ? 4 : 5));
    if (level > 1) s--;
    pm_ramp(m, RAMP_STONE, s);
}

void pctx_init(PCtx *c, const DoomoLook *l, int status, int expr, int anim)
{
    memset(c, 0, sizeof(*c));
    c->l = l;
    c->status = status;
    c->expr = (expr >= 0 && expr < EXPR_COUNT) ? expr : EXPR_NEUTRAL;
    c->anim = anim < 0 ? -anim : anim;
    c->blink = (c->anim % 96) >= 92;
    c->breath = (c->anim >> 5) & 1;
    c->twitch = (c->anim % 200) >= 190;
    c->wag = (c->anim >> 3) & 1;
    c->creepy = status == RES_SOUL_LOST ? 2 : ((status == RES_POSSESSED || c->expr == EXPR_POSSESSED) ? 1 : 0);
    c->ghost = status == RES_GHOST;
    c->dead = status == RES_DEAD;
    c->critter = ph_is_critter(l->species);
    pm_skin(&c->skin, l);
    if (c->creepy) creepy_skin(&c->skin, look_skin(l, 1), c->creepy);
    pm_paint(&c->hair, l->hair_color);
    if (l->skin >= 8 && l->skin - 8 == l->hair_color) {
        /* hair the same colour as the fur: a shade deeper so the tuft reads */
        uint8_t d = PAL_PAINT(l->hair_color, 0);
        pm_from3(&c->hair, palette_darker(d) ? palette_darker(d) : d, d, PAL_PAINT(l->hair_color, 1));
        c->hair.c[4] = PAL_PAINT(l->hair_color, 2);
    }
    if (l->hair_highlight != LOOK_NO_COLOR) pm_paint(&c->hairhi, l->hair_highlight);
    else c->hairhi = c->hair;
    pm_paint(&c->eye, l->eye_color);
    pm_paint(&c->horn, l->horn_color);
    pm_paint(&c->mark, l->marking_color);
    pm_paint(&c->top, l->top_color);
    pm_paint(&c->bot, l->bottom_color);
    pm_paint(&c->shoe, l->shoes_color);
    pm_paint(&c->hat, l->hat_color);
    pm_paint(&c->acc, l->accessory_color);
    pm_paint(&c->patm, l->pattern_color);
    if (c->creepy > 1) {
        /* soul lost: the colour drains out of everything */
        PMat *ms[9];
        int i, k;
        ms[0] = &c->hair; ms[1] = &c->top; ms[2] = &c->bot; ms[3] = &c->shoe;
        ms[4] = &c->hat; ms[5] = &c->acc; ms[6] = &c->patm; ms[7] = &c->hairhi; ms[8] = &c->horn;
        for (i = 0; i < 9; i++) {
            for (k = 0; k < 5; k++) ms[i]->c[k] = doomo_tintmap[TINT_NIGHT][ms[i]->c[k]];
            ms[i]->line = pm_line_of(ms[i]->c[1]);
        }
    }
}

/* ====================================================================== */
/* layered drawing shared by every view                                     */
static int g_wframe = -1, g_wview, g_wanim, g_whx, g_why;

static void weapon_layer(PCtx *c, int front)
{
    if (g_wframe < 0) return;
    if ((g_wview <= 1) == (front == 0))
        pg_draw_weapon(c, g_wframe, g_wview, g_wanim, g_whx, g_why, front);
}

static void finish(PCtx *c)
{
    pc_finish_lines();
    ph_halo(c);
    pf_status(c);
    pf_expression_fx(c);
    pf_aura_front(c);
    if (g_wframe >= 0 && g_wanim == 4) pg_draw_flash(g_wanim);
    if (c->ghost) {
        pf_recolor_ghost();
    } else if (c->dead) {
        int i, n = pcv.w * pcv.h;
        for (i = 0; i < n; i++) if (pcv.px[i]) pcv.px[i] = doomo_tintmap[TINT_NIGHT][pcv.px[i]];
    }
}

static void draw_layers(PCtx *c)
{
    pb_set_neck(c->h.cy + PCT(c->h.ry, 86));
    pf_aura_back(c);
    weapon_layer(c, 0);
    pb_draw_behind(c);
    ph_hair_back(c);
    ph_ears(c, 0);
    pb_draw_body(c);
    ph_head_base(c);
    ph_face(c);
    pb_draw_neck(c);
    ph_hair_front(c);
    ph_face_over(c);
    ph_ears(c, 1);
    ph_hat(c);
    ph_species_over(c);
    ph_horns(c);
    ph_face_acc(c);
    pb_draw_front(c);
    weapon_layer(c, 1);
    finish(c);
}

static void float_shadow(PCtx *c, int cx, int y, int w)
{
    int x;
    int sp = c->l->species;
    if (!(sp == SPECIES_CACOBLOB || sp == SPECIES_GHOST || c->ghost)) return;
    for (x = -w; x <= w; x++) {
        pc_fx(cx + x, y, PAL_RAMP(RAMP_STONE, 4));
        if (p_abs(x) < w - 1) pc_fx(cx + x, y - 1, PAL_RAMP(RAMP_STONE, 5));
    }
}

/* ====================================================================== */
/* bust portrait                                                            */
static void render_portrait_ctx(PCtx *c, uint8_t *out)
{
    const DoomoLook *l = c->l;
    int rx, ry, cy, top, shift = 0;
    pc_begin(out, PORTRAIT_W, PORTRAIT_H);
    pb_layout(c, VK_PORTRAIT, 0, 0, c->anim);
    rx = PX(14) + l->head_size * 4 + 4;
    ry = PCT(rx, 90);
    cy = pb_torso_top() - PCT(ry, 84);
    if (l->species == SPECIES_CACOBLOB) cy -= c->breath ? 8 : 0;
    top = cy - ph_top_extent(l) * ry / 256;
    if (top < PX(1)) shift = DOOMO_MIN(PX(1) - top, PX(6));
    if (shift) {
        /* move the whole bust down so tall ears / hats stay in frame */
        cy += shift;
        pb_shift(shift);
    }
    phead_set(&c->h, PX(24), cy, rx, ry, 0, LOD_BIG);
    draw_layers(c);
}

void portrait_render(const DoomoLook *l, int status, int expr, int anim, uint8_t *out)
{
    DoomoLook L;
    PCtx c;
    if (!l || !out) return;
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, expr, anim);
    render_portrait_ctx(&c, out);
}

/* ====================================================================== */
/* creator figure                                                           */
static void render_figure_ctx(PCtx *c, uint8_t *out)
{
    const DoomoLook *l = c->l;
    int rx, ry, cy, k;
    int te = ph_top_extent(l);
    pc_begin(out, FIGURE_W, FIGURE_H);
    pb_layout(c, VK_FIGURE, 0, 0, c->anim);
    rx = PX(12) + 8 + l->head_size * 5;
    for (k = 0; k < 40; k++) {
        ry = PCT(rx, 90);
        cy = pb_head_y(c, ry);
        if (cy - te * ry / 256 >= PX(1) || rx <= PX(10)) break;
        rx -= 4;
    }
    ry = PCT(rx, 90);
    cy = pb_head_y(c, ry);
    if (pb_nolegs()) cy += c->breath ? -8 : 0;
    phead_set(&c->h, PX(24), cy, rx, ry, 0, LOD_BIG);
    if (pb_nolegs()) float_shadow(c, 24, FIGURE_H - 2, 7);
    draw_layers(c);
}

void figure_render(const DoomoLook *l, int status, int expr, int anim, uint8_t *out)
{
    DoomoLook L;
    PCtx c;
    if (!l || !out) return;
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, expr, anim);
    render_figure_ctx(&c, out);
}

/* ====================================================================== */
/* overworld chibi                                                           */
static void render_body_ctx(PCtx *c, int dir, int frame, uint8_t *out)
{
    static const int8_t steps[4] = { 0, -1, 0, 1 };
    const DoomoLook *l = c->l;
    int yaw, rx, ry, cy;
    dir &= 3;
    frame &= 3;
    yaw = dir == 0 ? 0 : (dir == 3 ? 4 : 2);
    c->breath = frame & 1;
    c->wag = (frame >> 1) & 1;
    c->blink = 0;
    pc_begin(out, BODYSPR_W, BODYSPR_H);
    pb_layout(c, VK_CHIBI, yaw, steps[frame], frame);
    rx = PX(6) + 8 + l->head_size;
    ry = PCT(rx, 88);
    cy = pb_head_y(c, ry);
    if (cy - ry < PX(4) && ph_top_extent(l) > 400) cy = DOOMO_MAX(cy, ry + PX(2));
    phead_set(&c->h, PX(8), cy, rx, ry, yaw, LOD_TINY);
    draw_layers(c);
    if (dir == 2) pc_flipx();
}

void bodyspr_render(const DoomoLook *l, int status, int dir, int frame, uint8_t *out)
{
    DoomoLook L;
    PCtx c;
    if (!l || !out) return;
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, EXPR_NEUTRAL, frame * 8);
    render_body_ctx(&c, dir, frame, out);
}

/* ====================================================================== */
/* third-person fighter                                                      */
static void render_fighter_ctx(PCtx *c, int view, int anim, int wframe, uint8_t *out)
{
    /* run cycle: left stride, left passing (bob up), right stride, right passing */
    static const int8_t steps[4] = { -1, -2, 1, 2 };
    const DoomoLook *l = c->l;
    int yaw, rx, ry, cy, step = 0, hx, hy, tt, H;
    view = DOOMO_CLAMP(view, 0, 4);
    anim = DOOMO_CLAMP(anim, 0, 6);
    yaw = 4 - view;
    if (anim < 4) step = steps[anim];
    c->breath = (anim & 1) && anim < 4;
    c->wag = anim & 1;
    c->blink = 0;
    if (anim == 6) c->expr = EXPR_SHOCK;
    else if (anim == 4) c->expr = c->creepy ? c->expr : EXPR_ANGRY;
    pc_begin(out, FIGHTER_W, FIGHTER_H);
    if (anim == 5) {
        /* dodge roll: a plush ball tumbling, seen from the camera's angle */
        PMat m = c->top;
        int sp = l->top == TOP_NONE ? 1 : 0;
        int dx = (2 - view) * 2;                 /* rolls toward the facing side */
        pb_layout(c, VK_FIGHTER, yaw, 0, anim);
        if (sp) m = c->skin;
        pc_part(&m, SH_BALL, 0);
        if (l->pattern != PATTERN_SOLID && !sp) pc_pattern(p_pattern_tiles[l->pattern], &c->patm);
        pc_ellipse(PX(16 + dx / 2), PX(37), PX(9), PX(8));
        pc_pattern(NULL, NULL);
        pc_part(&c->skin, SH_BALL, c->critter ? PF_FUZZ : 0);
        pc_ellipse(PX(9 + dx), PX(42), PX(3), PX(2) + 8);
        pc_ellipse(PX(23 + dx / 2), PX(42), PX(3), PX(2) + 8);
        rx = PX(8) + l->head_size;
        ry = PCT(rx, 90);
        phead_set(&c->h, PX(15 - dx), PX(30), rx, ry, yaw, LOD_MID);
        if (yaw < 4) pb_draw_tail(c, 0);
        ph_hair_back(c);
        ph_ears(c, 0);
        ph_head_base(c);
        ph_face(c);
        ph_hair_front(c);
        ph_ears(c, 1);
        ph_hat(c);
        ph_horns(c);
        g_wframe = -1;
        finish(c);
        /* motion swoosh */
        for (tt = 0; tt < 6; tt++) {
            pc_fx(4 + tt + dx, 46 - tt / 2, PAL_WHITE);
            pc_fx(26 - tt + dx, 25 + tt / 3, PAL_GRAY_L);
        }
        if (view == 2) pc_flipx();
        return;
    }
    pb_layout(c, VK_FIGHTER, yaw, step, anim);
    H = (c->bh * 2);
    tt = pb_torso_top();
    /* both hands on the weapon */
    switch (view) {
    case 0: hx = c->bx + PX(5); hy = tt + PCT(H, 50); break;
    case 1: hx = c->bx - PX(3); hy = tt + PCT(H, 55); break;
    case 2: hx = c->bx - PX(3); hy = tt + PCT(H, 60); break;
    case 3: hx = c->bx - PX(2); hy = tt + PCT(H, 65); break;
    default: hx = c->bx + PX(1); hy = tt + PCT(H, 62); break;
    }
    if (anim == 6) { hx += PX(1); hy -= PX(2); }
    pb_set_hand(0, hx - PX(2), hy + 8);
    pb_set_hand(1, hx + PX(2), hy - 8);
    if (view == 2) { pb_set_hand(0, hx - PX(3), hy); pb_set_hand(1, hx + PX(1), hy + PX(1)); }
    rx = PX(9) + l->head_size * 2;
    ry = PCT(rx, 90);
    cy = pb_head_y(c, ry);
    if (anim == 6) cy -= PX(1);
    if (anim == 4) cy += 8;
    phead_set(&c->h, PX(16) + (anim == 6 ? PX(1) : 0), cy, rx, ry, yaw, LOD_MID);
    g_wframe = wframe;
    g_wview = view;
    g_wanim = anim;
    g_whx = hx;
    g_why = hy;
    if (pb_nolegs()) float_shadow(c, 16, FIGHTER_H - 1, 5);
    draw_layers(c);
    g_wframe = -1;
}

/* ====================================================================== */
/* cache                                                                     */
#define PC_BLOCK   384
#define PC_BLOCKS  48
#define PC_FREE    0xFF
/* phase-only re-renders (blink, breathing, aura frames) allowed per tick;
 * a GBA bust costs about 0.75M cycles (~45 ms), so one per tick there */
#if defined(DOOMO_PLATFORM_GBA)
#define PC_BUDGET  1
#else
#define PC_BUDGET  3
#endif

enum { CK_NONE, CK_PORTRAIT, CK_BODY, CK_FIGURE, CK_FIGHTER };

typedef struct CacheEnt {
    DoomoImage img;
    uint32_t key, look, stamp;
    int16_t rid;
    uint8_t kind, start, nblk;
} CacheEnt;

static uint8_t  pool[PC_BLOCKS * PC_BLOCK];
static CacheEnt ent[PC_BLOCKS];
static uint8_t  owner[PC_BLOCKS];
static uint8_t  cache_ready;
static uint32_t cache_stamp;
static uint32_t budget_tick;
static int      budget_used;

DOOMO_STATIC_ASSERT(PORTRAIT_W * PORTRAIT_H == 7 * PC_BLOCK, portrait_is_7_blocks);
DOOMO_STATIC_ASSERT(FIGURE_W * FIGURE_H == 10 * PC_BLOCK, figure_is_10_blocks);
DOOMO_STATIC_ASSERT(FIGHTER_W * FIGHTER_H == 4 * PC_BLOCK, fighter_is_4_blocks);
DOOMO_STATIC_ASSERT(BODYSPR_W * BODYSPR_H == PC_BLOCK, body_is_1_block);

static void cache_init(void)
{
    int i;
    if (cache_ready) return;
    for (i = 0; i < PC_BLOCKS; i++) { owner[i] = PC_FREE; ent[i].kind = CK_NONE; }
    cache_ready = 1;
}

static void cache_drop(int e)
{
    int i;
    if (ent[e].kind == CK_NONE) return;
    for (i = 0; i < ent[e].nblk; i++) owner[ent[e].start + i] = PC_FREE;
    ent[e].kind = CK_NONE;
}

static uint32_t look_hash(const DoomoLook *l)
{
    const uint8_t *p = (const uint8_t *)l;
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(*l); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static CacheEnt *cache_find(int kind, int rid, uint32_t key, uint32_t look, uint32_t mask)
{
    int i;
    for (i = 0; i < PC_BLOCKS; i++) {
        CacheEnt *e = &ent[i];
        if (e->kind == kind && e->rid == rid && e->look == look && ((e->key ^ key) & mask) == 0) {
            e->stamp = ++cache_stamp;
            return e;
        }
    }
    return NULL;
}

static CacheEnt *cache_alloc(int kind, int rid, uint32_t key, uint32_t look, int w, int h)
{
    int n = (w * h + PC_BLOCK - 1) / PC_BLOCK, s, best = 0, k, e;
    uint32_t best_cost = 0xFFFFFFFFu;
    /* the window whose most recently used victim is oldest */
    for (s = 0; s + n <= PC_BLOCKS; s++) {
        uint32_t cost = 0;
        for (k = 0; k < n; k++) {
            int o = owner[s + k];
            if (o != PC_FREE && ent[o].stamp + 1 > cost) cost = ent[o].stamp + 1;
        }
        if (cost < best_cost) { best_cost = cost; best = s; if (!cost) break; }
    }
    for (k = 0; k < n; k++) if (owner[best + k] != PC_FREE) cache_drop(owner[best + k]);
    for (e = 0; e < PC_BLOCKS && ent[e].kind != CK_NONE; e++) {}
    if (e >= PC_BLOCKS) e = 0;          /* cannot happen: n >= 1 blocks freed */
    for (k = 0; k < n; k++) owner[best + k] = (uint8_t)e;
    ent[e].kind = (uint8_t)kind;
    ent[e].rid = (int16_t)rid;
    ent[e].key = key;
    ent[e].look = look;
    ent[e].start = (uint8_t)best;
    ent[e].nblk = (uint8_t)n;
    ent[e].stamp = ++cache_stamp;
    ent[e].img.w = (uint16_t)w;
    ent[e].img.h = (uint16_t)h;
    ent[e].img.pix = pool + best * PC_BLOCK;
    ent[e].img.uid = doomo_image_new_uid();
    return &ent[e];
}

static int budget_ok(void)
{
    uint32_t t = doomo_frame_count();
    if (t != budget_tick) { budget_tick = t; budget_used = 0; }
    return budget_used < PC_BUDGET;
}

static const DoomoResident *res_of(int rid, int *status)
{
    static DoomoResident def;
    static uint8_t def_ready;
    const DoomoResident *r = (rid >= 0 && rid <= DOOMO_PLAYER_ID) ? town_res(rid) : NULL;
    if (!def_ready) { resident_default(&def); def.status = RES_ALIVE; def_ready = 1; }
    if (!r || r->status == RES_EMPTY) {
        *status = RES_EMPTY;
        return &def;
    }
    *status = r->status;
    return r;
}

/* animation phase key for cached portraits / figures */
typedef struct Phase { int blink, breath, twitch, wag, fx, talk; } Phase;

static uint32_t phase_of(int rid, int expr, int status, const DoomoLook *l, Phase *p)
{
    uint32_t t = doomo_frame_count();
    uint32_t off = (uint32_t)((rid + 1) * 53) & 1023u;
    int fxon = l->aura != AURA_NONE || status == RES_POSSESSED || status == RES_SOUL_LOST ||
               expr == EXPR_SAD || expr == EXPR_LOVE || expr == EXPR_SLEEPY || expr == EXPR_ANGRY ||
               expr == EXPR_HAPPY || expr == EXPR_DEAD || expr == EXPR_POSSESSED ||
               l->halo == HALO_FLAME_RING || l->halo == HALO_STARS || (l->cheeks & CHEEK_SPARKLES) ||
               l->hair_style == HAIR_FLAME;
    p->blink = ((t + off) % 110u) >= 106u;
    p->breath = (int)(((t + off) >> 5) & 1u);
    p->twitch = ((t + off * 3u) % 270u) >= 262u;
    p->wag = p->breath;
    p->fx = fxon ? (int)((t >> 3) & 3u) : 0;
    p->talk = expr == EXPR_TALK ? (int)((t >> 2) & 1u) : 0;
    return (uint32_t)(p->blink | (p->breath << 1) | (p->twitch << 2) | (p->fx << 3) | (p->talk << 5));
}

static void apply_phase(PCtx *c, const Phase *p)
{
    c->blink = p->blink;
    c->breath = p->breath;
    c->twitch = p->twitch;
    c->wag = p->wag;
    c->anim = p->fx * 8 + p->talk * 4;
}

/* the look of an empty slot: a soft lavender silhouette with a "?" */
static void vacant(uint8_t *pix, int w, int h)
{
    int i, n = w * h, cx = w / 2, cy = h / 2 - 2;
    static const uint8_t q[7] = { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 };
    for (i = 0; i < n; i++)
        if (pix[i]) pix[i] = palette_luma(pix[i]) < 90 ? PAL_RAMP(RAMP_STONE, 4) : PAL_RAMP(RAMP_STONE, 6);
    for (i = 0; i < 7; i++) {
        int b;
        for (b = 0; b < 5; b++)
            if (q[i] & (0x10 >> b)) {
                int x = cx - 2 + b, y = cy - 3 + i;
                if (x >= 0 && y >= 0 && x < w && y < h) pix[y * w + x] = PAL_RAMP(RAMP_STONE, 2);
            }
    }
}

const DoomoImage *portrait_image(int resident_id, int expr)
{
    int status;
    const DoomoResident *r;
    uint32_t lh, key, ph;
    Phase p;
    CacheEnt *e;
    PCtx c;
    DoomoLook L;
    cache_init();
    r = res_of(resident_id, &status);
    if (expr < 0 || expr >= EXPR_COUNT) expr = EXPR_NEUTRAL;
    lh = look_hash(&r->look);
    ph = phase_of(resident_id, expr, status, &r->look, &p);
    key = ph | ((uint32_t)expr << 8) | ((uint32_t)status << 12);
    e = cache_find(CK_PORTRAIT, resident_id, key, lh, 0xFFFFFFFFu);
    if (e) return &e->img;
    if (!budget_ok()) {
        e = cache_find(CK_PORTRAIT, resident_id, key, lh, 0xFFFFFF00u);
        if (e) return &e->img;
    }
    budget_used++;
    e = cache_alloc(CK_PORTRAIT, resident_id, key, lh, PORTRAIT_W, PORTRAIT_H);
    L = r->look;
    sanitize(&L);
    pctx_init(&c, &L, status == RES_EMPTY ? RES_ALIVE : status, expr, 0);
    apply_phase(&c, &p);
    render_portrait_ctx(&c, (uint8_t *)e->img.pix);
    if (status == RES_EMPTY) vacant((uint8_t *)e->img.pix, PORTRAIT_W, PORTRAIT_H);
    return &e->img;
}

const DoomoImage *bodyspr_image(int resident_id, int dir, int frame)
{
    int status;
    const DoomoResident *r;
    uint32_t lh, key;
    CacheEnt *e;
    PCtx c;
    DoomoLook L;
    cache_init();
    r = res_of(resident_id, &status);
    dir &= 3;
    frame &= 3;
    lh = look_hash(&r->look);
    key = (uint32_t)(dir | (frame << 2)) | ((uint32_t)status << 12);
    e = cache_find(CK_BODY, resident_id, key, lh, 0xFFFFFFFFu);
    if (e) return &e->img;
    e = cache_alloc(CK_BODY, resident_id, key, lh, BODYSPR_W, BODYSPR_H);
    L = r->look;
    sanitize(&L);
    pctx_init(&c, &L, status == RES_EMPTY ? RES_ALIVE : status, EXPR_NEUTRAL, frame * 8);
    render_body_ctx(&c, dir, frame, (uint8_t *)e->img.pix);
    return &e->img;
}

const DoomoImage *figure_image(const DoomoLook *l, int status, int expr)
{
    uint32_t lh, key, ph;
    Phase p;
    CacheEnt *e;
    PCtx c;
    DoomoLook L;
    static DoomoResident def;
    cache_init();
    if (!l) { resident_default(&def); l = &def.look; }
    if (expr < 0 || expr >= EXPR_COUNT) expr = EXPR_NEUTRAL;
    status &= 15;
    lh = look_hash(l);
    ph = phase_of(-1, expr, status, l, &p);
    if (l->tail || l->wings) p.wag = (int)((doomo_frame_count() >> 3) & 1u);
    ph |= (uint32_t)p.wag << 6;
    key = ph | ((uint32_t)expr << 8) | ((uint32_t)status << 12);
    e = cache_find(CK_FIGURE, -1, key, lh, 0xFFFFFFFFu);
    if (e) return &e->img;
    if (!budget_ok()) {
        e = cache_find(CK_FIGURE, -1, key, lh, 0xFFFFFF00u);
        if (e) return &e->img;
    }
    budget_used++;
    e = cache_alloc(CK_FIGURE, -1, key, lh, FIGURE_W, FIGURE_H);
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, expr, 0);
    apply_phase(&c, &p);
    render_figure_ctx(&c, (uint8_t *)e->img.pix);
    return &e->img;
}

const DoomoImage *fighter_image(int resident_id, int view, int anim, int weapon_frame)
{
    int status;
    const DoomoResident *r;
    uint32_t lh, key;
    CacheEnt *e;
    PCtx c;
    DoomoLook L;
    cache_init();
    r = res_of(resident_id, &status);
    view = DOOMO_CLAMP(view, 0, 4);
    anim = DOOMO_CLAMP(anim, 0, 6);
    if (weapon_frame < 0 || weapon_frame >= 64) weapon_frame = 0;
    lh = look_hash(&r->look);
    key = (uint32_t)(view | (anim << 3) | (weapon_frame << 6)) | ((uint32_t)status << 12);
    e = cache_find(CK_FIGHTER, resident_id, key, lh, 0xFFFFFFFFu);
    if (e) return &e->img;
    e = cache_alloc(CK_FIGHTER, resident_id, key, lh, FIGHTER_W, FIGHTER_H);
    L = r->look;
    sanitize(&L);
    pctx_init(&c, &L, status == RES_EMPTY ? RES_ALIVE : status, EXPR_NEUTRAL, anim * 8);
    render_fighter_ctx(&c, view, anim, weapon_frame, (uint8_t *)e->img.pix);
    return &e->img;
}

void portrait_invalidate(int resident_id)
{
    int i;
    if (!cache_ready) return;
    for (i = 0; i < PC_BLOCKS; i++)
        if (ent[i].kind != CK_NONE && ent[i].kind != CK_FIGURE && ent[i].rid == resident_id)
            cache_drop(i);
}

/* ---- extras for tools and tests (portrait.h additions) ------------------ */
void fighter_render(const DoomoLook *l, int status, int view, int anim, int weapon_frame,
                    uint8_t *out)
{
    DoomoLook L;
    PCtx c;
    if (!l || !out) return;
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, EXPR_NEUTRAL, anim * 8);
    render_fighter_ctx(&c, view, anim, weapon_frame, out);
}

uint32_t portrait_last_ops(void) { return pcv.ops; }
uint32_t portrait_last_prims(void) { return pcv.prims; }

void portrait_cache_reset(void)
{
    cache_ready = 0;
    cache_init();
}
