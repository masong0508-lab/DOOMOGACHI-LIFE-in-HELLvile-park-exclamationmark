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
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/parts.h"

/* ====================================================================== */
/* context                                                                 */
static void sanitize(DoomoLook *l)
{
#define FIXE(f, n) if (l->f >= (n)) l->f = 0
/* CR2: a value past the painters' range is kept when a part script draws it (ps_has) */
#define FIXP(f, n, gf) if (l->f >= (n) && !ps_has((gf), l->f)) l->f = 0
#define FIXS(f, m) if (l->f > (m)) l->f = (m)
    FIXE(species, SPECIES_COUNT); FIXE(body, BODY_COUNT); FIXP(head_shape, HEAD_COUNT, GF_HEAD_SHAPE);
    FIXP(eye_style, EYE_COUNT, GF_EYE_STYLE); FIXP(brow_style, BROW_COUNT, GF_BROW_STYLE);
    FIXP(nose_style, NOSE_COUNT, GF_NOSE_STYLE); FIXP(mouth_style, MOUTH_COUNT, GF_MOUTH_STYLE);
    FIXP(hair_style, HAIR_COUNT, GF_HAIR_STYLE); FIXP(facial_hair, FHAIR_COUNT, GF_FACIAL_HAIR);
    FIXP(ears, EARS_COUNT, GF_EARS); FIXP(horns, HORNS_COUNT, GF_HORNS); FIXP(tail, TAIL_COUNT, GF_TAIL);
    FIXP(wings, WINGS_COUNT, GF_WINGS); FIXP(halo, HALO_ALL_COUNT, GF_HALO); FIXE(marking, MARK_COUNT);
    FIXP(top, TOP_COUNT, GF_TOP); FIXP(bottom, BOTTOM_COUNT, GF_BOTTOM); FIXP(shoes, SHOES_COUNT, GF_SHOES);
    FIXP(hat, HAT_COUNT, GF_HAT); FIXP(accessory, ACC_COUNT, GF_ACCESSORY); FIXP(aura, AURA_COUNT, GF_AURA);
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
#undef FIXP
#undef FIXS
}

/* CR2 shape-only renders: every material on one grey ramp (portrait_shape_mode) */
static void shape_materials(PCtx *c)
{
    PMat *ms[14];
    int i;
    ms[0] = &c->skin; ms[1] = &c->skin2; ms[2] = &c->hair; ms[3] = &c->hairhi; ms[4] = &c->eye;
    ms[5] = &c->horn; ms[6] = &c->mark; ms[7] = &c->top; ms[8] = &c->bot; ms[9] = &c->shoe;
    ms[10] = &c->hat; ms[11] = &c->acc; ms[12] = &c->patm; ms[13] = &c->skin;
    for (i = 0; i < 14; i++) pm_ramp(ms[i], RAMP_STONE, 4);
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
    c->d = &doomo_deep_zero;     /* the cache resolves a resident's own set into it */
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
    if (p_shape) shape_materials(c);
    pf_material_init(c);
#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)
    part_arena_begin();                     /* CR2: each worn script decoded once per render */
#endif
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

/* The layers in painter's order, in DL_STEPS groups of roughly equal cost
 * (about 0.1-0.2M cycles each on the GBA) so the cache can spread a
 * re-render over several ticks. The groups: 0 back, 1 body, 2a head base, 2b face, 3 neck and
 * front hair, 4 hat / horns / accessories / front, 5 finish (P1 split the old group 2 in two,
 * SPEC 4.4; the draw order is unchanged, so every image is too).
 * SP2 (X1): every group is a run of pieces (DL_PIECES in all) and a sliced cache job runs one piece per slice, so
 * no slice is more than ~0.1M cycles on the GBA (a whole group was up to 0.45M): the back in three, the body in
 * three (pb_draw_body_part), the head base and the face in two each, the neck and front hair in three, the hat and
 * front in three, and the finishing pass in nine (pc_finish_lines' scan, its marked rows in six bands top to
 * bottom, the rest of finish in two). The pieces of a group are the calls the group made, in the same order, so a
 * whole render (draw_layers) is the same calls as before. */
#define DL_STEPS 7
#define DL_PIECES 25
#define DL_FIN    16           /* the first finishing piece */
#define DL_BANDS  6            /* bands of pc_finish_rows */
static const uint8_t group_first[DL_STEPS + 1] = { 0, 3, 6, 8, 10, 13, DL_FIN, DL_PIECES };
/* the slices of a sliced cache job: the set-up, then the pieces in order */
enum { SL_SETUP = 0, SL_N = 1 + DL_PIECES };

/* the finishing pass: piece k = DL_FIN + f; f 0 the transition scan, 1..DL_BANDS the marked rows, then the halo
 * and status marks, then the expression effects, the front aura and the ghost / dead recolour */
static void finish_piece(PCtx *c, int f)
{
    if (f == 0) {
        pc_finish_scan();
    } else if (f <= DL_BANDS) {
        int h = pcv.h;
        pc_finish_rows(h * (f - 1) / DL_BANDS, h * f / DL_BANDS);
    } else if (f == DL_BANDS + 1) {
        ph_halo(c);
        pf_status(c);
    } else {
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
}

static void draw_piece(PCtx *c, int k)
{
    switch (k) {
    /* group 0: the back */
    case 0:
        pb_set_neck(c->h.cy + PCT(c->h.ry, 86));
        pf_aura_back(c);
        weapon_layer(c, 0);
        break;
    case 1: pb_draw_behind(c); break;
    case 2:
        ph_hair_back(c);
        ph_ears(c, 0);
        break;
    /* group 1: the body */
    case 3: case 4: case 5: pb_draw_body_part(c, k - 3); break;
    /* group 2a: the head silhouette (+ fur tufts) */
    case 6: case 7: ph_head_base_part(c, k - 6); break;
    /* group 2b: species marks, eyes, brows, nose, mouth */
    case 8: case 9: ph_face_part(c, k - 8); break;
    /* group 3 */
    case 10: pb_draw_neck(c); break;
    case 11: ph_hair_front(c); break;
    case 12:
        ph_face_over(c);
        ph_ears(c, 1);
        break;
    /* group 4 */
    case 13:
        ph_hat(c);
        ph_species_over(c);
        break;
    case 14:
        ph_horns(c);
        ph_face_acc(c);
        break;
    case 15:
        pb_draw_front(c);
        weapon_layer(c, 1);
        break;
    default:
        finish_piece(c, k - DL_FIN);
        break;
    }
}

/* SPECIES_TURKEY (portrait_turkey.c): the same seven groups, with the
 * turkey's own neck, head, face and Hatter pieces in place of the engine's
 * head, face and hair; everything else (body, outfit, other hats, hair,
 * ears, horns, face accessories, weapons, effects) is the engine's. The
 * engine draws a look without its turkey-replaced parts (look copies). */
static void turkey_piece(PCtx *c, int k)
{
    const DoomoLook *own = c->l;
    DoomoLook t = *own;
    int tufts = ptk_tufts(own), engine_hair = own->hair_style != HAIR_BALD && !tufts;
    if (t.tail == TAIL_FEATHER_FAN) t.tail = TAIL_NONE;     /* the turkey fan instead */
    switch (k) {
    case 0:
        ptk_place(c);
        pf_aura_back(c);
        weapon_layer(c, 0);
        if (c->byaw < 3) ptk_tail_fan(c);
        break;
    case 1:
        ptk_coat_tails(c);
        c->l = &t;
        pb_draw_behind(c);
        c->l = own;
        break;
    case 2:
        if (engine_hair) ph_hair_back(c);
        if (own->ears != EARS_NONE) ph_ears(c, 0);
        break;
    case 3: case 4: case 5: pb_draw_body_part(c, k - 3); break;
    case 6: case 7: case 8: case 9:   /* the turkey head in four (2b: its face is drawn after the neck, group 3) */
        ptk_head_part(c, k - 6);
        break;
    case 10:
        if (t.top == TOP_SUIT) t.top = TOP_NONE;            /* lapels instead of the collar */
        if (t.accessory == ACC_BOWTIE) t.accessory = ACC_NONE;  /* the floppy bow */
        c->l = &t;
        pb_draw_neck(c);
        c->l = own;
        break;
    case 11: ptk_face(c); break;
    case 12:
        if (engine_hair) ph_hair_front(c);
        if (own->ears != EARS_NONE) ph_ears(c, 1);
        break;
    case 13:
        if (ptk_hatter(own)) ptk_hat(c);
        else ph_hat(c);
        ptk_brows(c);
        break;
    case 14:
        ph_horns(c);
        ph_face_acc(c);
        if (c->byaw >= 3) ptk_tail_fan(c);
        break;
    case 15:
        c->l = &t;
        pb_draw_front(c);
        c->l = own;
        weapon_layer(c, 1);
        break;
    default:
        finish_piece(c, k - DL_FIN);
        break;
    }
}

static void draw_any_piece(PCtx *c, int k)
{
    if (c->l->species == SPECIES_TURKEY) turkey_piece(c, k);
    else draw_piece(c, k);
}

/* the whole finishing pass (the fighter's roll) */
static void finish(PCtx *c)
{
    int f;
    for (f = 0; f < DL_PIECES - DL_FIN; f++) finish_piece(c, f);
}

/* The head alone (P3's head impostor): the head layers of groups 0, 2a, 2b, 3 and 4 in the same
 * order draw_step uses, without the body, outfit, weapons or effects. */
void ph_head_only(PCtx *c, int lod)
{
    c->h.lod = lod;
    ph_hair_back(c);
    ph_ears(c, 0);
    ph_head_base(c);
    ph_face(c);
    ph_hair_front(c);
    ph_face_over(c);
    ph_ears(c, 1);
    ph_hat(c);
    ph_species_over(c);
    ph_horns(c);
    ph_face_acc(c);
}

/* every layer group of any look, piece by piece (a whole render) */
static void draw_layers(PCtx *c)
{
    int k;
    for (k = 0; k < group_first[DL_STEPS]; k++) draw_any_piece(c, k);
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
static void setup_portrait(PCtx *c, uint8_t *out)
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
}

static void render_portrait_ctx(PCtx *c, uint8_t *out)
{
    setup_portrait(c, out);
    draw_layers(c);
}

/* portrait_render with a resolved deep-field set (tests, contact sheets) */
void portrait_render_deep(const DoomoLook *l, const DoomoDeep *d, int status, int expr, int anim,
                          uint8_t *out)
{
    DoomoLook L;
    PCtx c;
    if (!l || !d || !out) return;
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, expr, anim);
    c.d = d;
    render_portrait_ctx(&c, out);
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
static void setup_figure(PCtx *c, uint8_t *out)
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
}

static void render_figure_ctx(PCtx *c, uint8_t *out)
{
    setup_figure(c, out);
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
static void body_setup(PCtx *c, int dir, int frame, uint8_t *out)
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
}

static void render_body_ctx(PCtx *c, int dir, int frame, uint8_t *out)
{
    body_setup(c, dir, frame, out);
    draw_layers(c);
    if ((dir & 3) == 2) pc_flipx();
}

void bodyspr_render(const DoomoLook *l, int status, int dir, int frame, uint8_t *out)
{
    DoomoLook L;
    PCtx c;
    if (!l || !out) return;
    L = *l;
    sanitize(&L);
    pctx_init(&c, &L, status, EXPR_NEUTRAL, frame * 8);
    motion_pose(&c, dir, frame);
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
#define PC_BLOCKS  48                   /* 18 KB (EWRAM on the GBA; the PS1
                                           has no RAM to spare either) */
#define PC_FREE    0xFF
/* Re-renders that only change the animation phase (blink, breathing, aura
 * frames, mouth flaps) run as a sliced job: SL_N slices (the set-up and the
 * DL_PIECES pieces of the layer groups: SP2) of which PC_SLICES run per tick
 * inside the frame (the platform is asked for each: plat_portrait_slice_ok),
 * plus as many as the platform fits into the idle time after a frame
 * (portrait_idle_next / portrait_idle_run). A GBA bust costs
 * ~0.75M cycles (~1.4 logic ticks) in one go, one slice ~0.1M, so an
 * animating portrait never stalls a frame. Meanwhile the phase shown last stays
 * on screen, and the job looks PC_AHEAD ticks into the future so the next phase
 * is usually ready when it is due. On the GBA and the PS1 (PC_STANDIN) brand-new
 * content (a new expression or status of a look that is cached in some other
 * one) is drawn by a job too, the image shown last standing in for at most
 * PC_STANDIN ticks (SP2, X1/P-D1); content with nothing to stand in is drawn at
 * once, as everywhere on native. Aura / effect frames change every
 * 2^PC_FXSHIFT ticks. */
#if defined(DOOMO_PLATFORM_GBA)
#define PC_SLICES  5
#define PC_FXSHIFT 4
#define PC_STANDIN 4
#elif defined(__mips__)
#define PC_SLICES  11
#define PC_FXSHIFT 3
#define PC_STANDIN 4
#else
#define PC_SLICES  SL_N
#define PC_FXSHIFT 3
#define PC_STANDIN 0
#endif
#define PC_AHEAD   9           /* ticks of look-ahead for sliced jobs       */
#define PC_STALE   4           /* a job nobody asked about for this long    */
                               /* may be dropped for another one            */
#define PC_STALE_SI 12         /* ... a stand-in's job (SP2: a face that flaps between two expressions comes back) */
#define PC_PREFETCH 45         /* ticks a portrait_prefetch request lives   */

enum { CK_NONE, CK_PORTRAIT, CK_BODY, CK_FIGURE, CK_FIGHTER, CK_FOREIGN };

/* animation phase of a cached portrait / figure */
typedef struct Phase { int blink, breath, twitch, wag, fx, talk; } Phase;

typedef struct CacheEnt {
    DoomoImage img;
    uint32_t key, look, stamp, tick;
    int16_t rid;
    uint8_t kind, start, nblk, busy;
} CacheEnt;

static uint8_t  pool[PC_BLOCKS * PC_BLOCK];
static CacheEnt ent[PC_BLOCKS];
static uint8_t  owner[PC_BLOCKS];
static uint8_t  cache_ready;
static uint32_t cache_stamp;

DOOMO_STATIC_ASSERT(PORTRAIT_W * PORTRAIT_H == 7 * PC_BLOCK, portrait_is_7_blocks);
DOOMO_STATIC_ASSERT(FIGURE_W * FIGURE_H == 10 * PC_BLOCK, figure_is_10_blocks);
DOOMO_STATIC_ASSERT(FIGHTER_W * FIGHTER_H == 4 * PC_BLOCK, fighter_is_4_blocks);
DOOMO_STATIC_ASSERT(BODYSPR_W * BODYSPR_H == PC_BLOCK, body_is_1_block);

/* clock and per-tick step budget (tests can drive a fake clock) */
static uint8_t  fake_on;
static uint32_t fake_tick;
static int      steps_cfg;
static uint32_t steps_tick;
static int      steps_used;

static uint32_t now(void) { return fake_on ? fake_tick : doomo_frame_count(); }
/* slices a tick may run inside the frame; a test's portrait_cache_clock(tick, steps) counts layer groups (the old
 * unit) and gets the same share of a job */
static int steps_max(void) { return steps_cfg > 0 ? (steps_cfg * SL_N + DL_STEPS - 1) / DL_STEPS : PC_SLICES; }

static int steps_left(void)
{
    uint32_t t = now();
    if (t != steps_tick) { steps_tick = t; steps_used = 0; }
    return steps_max() - steps_used;
}

/* the sliced re-render in progress */
typedef struct Job {
    uint8_t  on, step, kind;
    uint8_t  bdir, bframe;      /* CK_BODY job (V11): the facing and frame being drawn */
    uint8_t  urgent;            /* SP2: new content behind a stand-in: done within PC_STANDIN ticks */
    uint8_t  vac;               /* SP2: an empty slot's "?" silhouette (vacant()) when done */
    uint8_t  pf;                /* SP2: a prefetch (queued again if it is aborted) */
    uint8_t  si;                /* SP2: draws new content behind a stand-in (or nothing: hold 3) */
    uint32_t t0;                /* the tick it started */
    uint32_t last;              /* SP2: the tick of its last slice (the platform's patience) */
    uint32_t fq_tick;           /* SP2: the tick fq was worked out for */
    uint8_t  fq;                /* SP2: slices an urgent job still has to run this tick */
    uint8_t  restarts;          /* times other renders forced a restart      */
    int8_t   e;                 /* cache entry being filled (busy, hidden)   */
    int      status, expr;
    uint32_t gen;               /* canvas generation after our last step     */
    uint32_t want;              /* last tick its resident was asked for      */
    Phase    p;
    DoomoLook L;
    DoomoDeep D;                /* resolved at job start (P1)                */
    PCtx     c;
} Job;
static Job job;
static int job_stale(void) { return job.si ? PC_STALE_SI : PC_STALE; }

/* SP2: the content of the last few finished jobs (kind, resident, look and the key without its phase, hashed).
 * New content that was drawn a moment ago and has fallen out of the pool again (a talking face that flaps between
 * two expressions, each in several animation phases, needs more images than the pool keeps) is a re-render, not a
 * first appearance: its job stands in without the PC_STANDIN deadline (idle time and the platform's room only). */
#define PC_RECENT 8
static uint32_t recent[PC_RECENT];
static uint8_t recent_at;
static uint32_t content_hash(int kind, int rid, uint32_t look, uint32_t base)
{
    uint32_t h = (uint32_t)kind * 0x9E3779B1u ^ (uint32_t)(rid + 3) * 0x85EBCA77u ^ look ^ (base & 0xFFFFFF00u) * 31u;
    return h ? h : 1;
}
static int recent_has(uint32_t h)
{
    int i;
    for (i = 0; i < PC_RECENT; i++) if (recent[i] == h) return 1;
    return 0;
}

static void cache_drop(int e)
{
    int i;
    if (ent[e].kind == CK_NONE) return;
    for (i = 0; i < ent[e].nblk; i++) owner[ent[e].start + i] = PC_FREE;
    ent[e].kind = CK_NONE;
    ent[e].busy = 0;
}

static uint8_t pf_requeue;     /* SP2: an aborted prefetch is queued again (pf, below) */
static void job_abort(void)
{
    if (!job.on) return;
    cache_drop(job.e);
    job.on = 0;
    if (job.pf) pf_requeue = 1;
}

static void cache_init(void)
{
    int i;
    if (cache_ready) return;
    for (i = 0; i < PC_BLOCKS; i++) {
        owner[i] = PC_FREE;
        ent[i].kind = CK_NONE;
        ent[i].busy = 0;
    }
    job.on = 0;
    pf_requeue = 0;
    memset(recent, 0, sizeof recent);
    cache_ready = 1;
}

/* FNV-1a over the look, four bytes a step (12 multiplies instead of 48:
 * the concert asks for ~45 chibi images a frame, and the byte-wise hash
 * was 3% of its PS1 frame). Byte loads, so any alignment / endianness
 * gives the same value. */
DOOMO_STATIC_ASSERT(sizeof(DoomoLook) % 4 == 0, look_is_words);
static uint32_t look_hash(const DoomoLook *l)
{
    const uint8_t *p = (const uint8_t *)l;
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(*l); i += 4) {
        uint32_t v = (uint32_t)p[i] | (uint32_t)p[i + 1] << 8 | (uint32_t)p[i + 2] << 16 |
                     (uint32_t)p[i + 3] << 24;
        h ^= v; h *= 16777619u;
    }
    return h;
}

/* A direct-mapped memo of the entry a (kind, rid, look) found last: the
 * scan below runs for every chibi drawn, and a scene draws the same few
 * dozen every frame (the scan over PC_BLOCKS entries was 4% of the PS1
 * concert's frame). An entry that was dropped or reused fails the match
 * and the scan runs as before. */
#define PC_MEMO 128
static uint8_t memo[PC_MEMO];

static unsigned memo_slot(int kind, int rid, uint32_t look)
{
    uint32_t h = (uint32_t)kind * 0x9E3779B1u ^ (uint32_t)(rid + 7) * 0x85EBCA77u ^ look;
    return (h ^ (h >> 16)) & (PC_MEMO - 1);
}

/* the matching ready entry used most recently (mask: which key bits must
 * match); `touch` marks it used */
static CacheEnt *cache_get(int kind, int rid, uint32_t key, uint32_t look, uint32_t mask,
                           int touch)
{
    int i;
    CacheEnt *best = NULL;
    unsigned slot = memo_slot(kind, rid, look);
    CacheEnt *m = &ent[memo[slot] < PC_BLOCKS ? memo[slot] : 0];
    if (m->kind == kind && !m->busy && m->rid == rid && m->look == look &&
        ((m->key ^ key) & mask) == 0) {
        /* the memo entry is the one touched last for this look, i.e. the
         * most recently used of the matches: the answer the scan gives */
        best = m;
    } else {
        for (i = 0; i < PC_BLOCKS; i++) {
            CacheEnt *e = &ent[i];
            if (e->kind == kind && !e->busy && e->rid == rid && e->look == look &&
                ((e->key ^ key) & mask) == 0 && (!best || e->stamp > best->stamp))
                best = e;
        }
    }
    if (best && touch) {
        best->stamp = ++cache_stamp;
        best->tick = now();
        memo[slot] = (uint8_t)(best - ent);
    }
    return best;
}

static CacheEnt *cache_find(int kind, int rid, uint32_t key, uint32_t look, uint32_t mask)
{
    return cache_get(kind, rid, key, look, mask, 1);
}

/* Claim the run of blocks whose most recently used victim is oldest. A
 * `soft` claim (look-ahead jobs) never evicts what was on screen in the
 * last two ticks and fails instead; nothing ever evicts the busy job. */
static CacheEnt *cache_alloc(int kind, int rid, uint32_t key, uint32_t look, int w, int h,
                             int soft)
{
    int n = (w * h + PC_BLOCK - 1) / PC_BLOCK, s, best = -1, k, e;
    uint32_t best_cost = 0xFFFFFFFFu, t = now();
    for (s = 0; s + n <= PC_BLOCKS; s++) {
        uint32_t cost = 0;
        for (k = 0; k < n; k++) {
            int o = owner[s + k];
            if (o == PC_FREE) continue;
            if (ent[o].busy || (soft && t - ent[o].tick < 2u)) { cost = 0xFFFFFFFFu; break; }
            if (ent[o].stamp + 1 > cost) cost = ent[o].stamp + 1;
        }
        if (cost < best_cost) { best_cost = cost; best = s; if (!cost) break; }
    }
    if (best < 0) {
        if (soft || !job.on) return NULL;
        job_abort();                    /* cannot happen with 48 blocks     */
        return cache_alloc(kind, rid, key, look, w, h, 0);
    }
    for (k = 0; k < n; k++) if (owner[best + k] != PC_FREE) cache_drop(owner[best + k]);
    for (e = 0; e < PC_BLOCKS && ent[e].kind != CK_NONE; e++) {}
    if (e >= PC_BLOCKS) e = 0;          /* cannot happen: n >= 1 blocks freed */
    for (k = 0; k < n; k++) owner[best + k] = (uint8_t)e;
    ent[e].kind = (uint8_t)kind;
    ent[e].busy = 0;
    ent[e].rid = (int16_t)rid;
    ent[e].key = key;
    ent[e].look = look;
    ent[e].start = (uint8_t)best;
    ent[e].nblk = (uint8_t)n;
    ent[e].stamp = ++cache_stamp;
    ent[e].tick = t;
    ent[e].img.w = (uint16_t)w;
    ent[e].img.h = (uint16_t)h;
    ent[e].img.pix = pool + best * PC_BLOCK;
    ent[e].img.uid = doomo_image_new_uid();
    return &ent[e];
}

static const DoomoResident *res_of(int rid, int *status)
{
    static DoomoResident def;
    static uint8_t def_ready;
    static DoomoResident fam;
    static int fam_id = -1;
    static uint32_t fam_fr = 0xFFFFFFFFu;
    const DoomoResident *r = (rid >= 0 && rid <= DOOMO_PLAYER_ID) ? town_res(rid) : NULL;
    if (rid >= PLAYAS_ID_FAMILY_FIRST) {             /* MP1: a family pool member / co-op housemate has a face too */
        if (fam_id != rid || fam_fr != doomo_frame_count()) {
            fam_id = family_resident(rid, &fam) ? rid : -1;
            fam_fr = doomo_frame_count();
        }
        if (fam_id == rid) r = &fam;
    }
    if (!def_ready) { resident_default(&def); def.status = RES_ALIVE; def_ready = 1; }
    if (!r || r->status == RES_EMPTY) {
        *status = RES_EMPTY;
        return &def;
    }
    *status = r->status;
    return r;
}

/* Which animation phases a look needs (P1.M1, SPEC 4.4 item 7): one list for the cache (phase_of)
 * and the creator's draft bust (creator_draw.c anim_phase), replacing their hand-written copies.
 * LAM_FX: the look has effect frames (aura, flame, sparkles, an expression or status that emits
 * them). Blink, breathing and twitch are the same for every look. */
uint32_t look_anim_mask(const DoomoLook *l, int status, int expr)
{
    uint32_t m = 0;
    if (l->aura != AURA_NONE || status == RES_POSSESSED || status == RES_SOUL_LOST ||
        expr == EXPR_SAD || expr == EXPR_LOVE || expr == EXPR_SLEEPY || expr == EXPR_ANGRY ||
        expr == EXPR_HAPPY || expr == EXPR_DEAD || expr == EXPR_POSSESSED ||
        l->halo == HALO_FLAME_RING || l->halo == HALO_STARS || (l->cheeks & CHEEK_SPARKLES) ||
        l->hair_style == HAIR_FLAME ||
        l->species == SPECIES_TURKEY)           /* the wattle wobbles (portrait_turkey.c) */
        m |= LAM_FX;
    if (expr == EXPR_TALK) m |= LAM_TALK;
    return m;
}

/* look_anim_mask plus the deep fields of resident/draft `rid` (a DFF_ANIM field asks for effect
 * frames too). Same list for the cache (phase_of) and the creator's draft bust. */
uint32_t look_anim_mask_rid(const DoomoLook *l, int rid, int status, int expr)
{
    DoomoDeep d;
    look_resolve_mask(NULL, rid, LV_ALL, &d);
    return look_anim_mask(l, status, expr) | deep_anim_mask(&d);
}

/* animation phase at tick t (staggered per resident); the low byte of the
 * cache key */
static int hold_on;   /* portrait_hold() */
static uint8_t hold_patient;   /* portrait_hold_patient() (CR9's creator): hold jobs without the SP2 deadline */

static uint32_t phase_of(int kind, int rid, int expr, int status, const DoomoLook *l,
                         uint32_t t, Phase *p)
{
    uint32_t off = (uint32_t)((rid + 1) * 53) & 1023u, ph;
    int fxon = (look_anim_mask_rid(l, rid, status, expr) & LAM_FX) != 0;
    memset(p, 0, sizeof(*p));
    if (status == RES_EMPTY) return 0;
    /* portrait_hold() (title figures): one still pose per expression, so
     * nothing re-renders between expression changes */
    if (hold_on == 1 || hold_on == 2) return 0;     /* the "?" stays still */
    p->blink = ((t + off) % 110u) >= 106u;
    p->breath = (int)(((t + off) >> 5) & 1u);
    p->twitch = ((t + off * 3u) % 270u) >= 262u;
    p->wag = p->breath;
    p->fx = fxon ? (int)((t >> PC_FXSHIFT) & 3u) : 0;
    p->talk = expr == EXPR_TALK ? (int)((t >> 2) & 1u) : 0;
    ph = (uint32_t)(p->blink | (p->breath << 1) | (p->twitch << 2) | (p->fx << 3) | (p->talk << 5));
    if (kind == CK_FIGURE && (l->tail || l->wings)) {
        p->wag = (int)((t >> 3) & 1u);
        ph |= (uint32_t)p->wag << 6;
    }
    return ph;
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

/* ---- sliced jobs --------------------------------------------------------- */
static int job_for(int kind, int rid, uint32_t look, uint32_t key, uint32_t mask)
{
    const CacheEnt *e = &ent[job.e];
    return job.on && e->kind == kind && e->rid == rid && e->look == look &&
           ((e->key ^ key) & mask) == 0;
}

static int job_hold;   /* job_hold: the job is a portrait_hold one */
/* SP2: ticks a stand-in may show before its job is forced to finish (0: no stand-ins, new content is drawn at
 * once); portrait_cache_standin() sets it in tests. A portrait_hold() job is due after PC_STANDIN_HOLD. */
#define PC_STANDIN_HOLD 4
static uint8_t si_ticks = PC_STANDIN;

/* claim a hidden entry and set the job up (step SL_SETUP next); soft: a look-ahead / prefetch claim */
static int job_begin(int kind, int rid, uint32_t key, uint32_t look, const DoomoLook *l, int status, int expr,
                     const Phase *p, int soft)
{
    CacheEnt *e = cache_alloc(kind, rid, key, look, kind == CK_FIGURE ? FIGURE_W : PORTRAIT_W,
                              kind == CK_FIGURE ? FIGURE_H : PORTRAIT_H, soft);
    if (!e) return 0;
    e->busy = 1;
    job_hold = 0;
    job.on = 1;
    job.step = SL_SETUP;
    job.restarts = 0;
    job.urgent = 0;
    job.vac = 0;
    job.pf = 0;
    job.si = 0;
    job.fq_tick = now() - 1;
    job.kind = (uint8_t)kind;
    job.e = (int8_t)(e - ent);
    job.want = job.t0 = job.last = now();
    job.status = status;
    job.expr = expr;
    job.p = *p;
    job.L = *l;
    sanitize(&job.L);
    look_resolve_mask(NULL, rid, LV_ALL, &job.D);
    return 1;
}

static void job_start(int kind, int rid, uint32_t key, uint32_t look, const DoomoLook *l,
                      int status, int expr, const Phase *p)
{
    if (job.on) {
        if ((int32_t)(now() - job.want) <= job_stale()) return;   /* still wanted: wait */
        job_abort();
    }
    job_begin(kind, rid, key, look, l, status, expr, p, 1);
}

/* drop a job nobody asks about any more (frees its blocks for chibis) */
static void job_reap(void)
{
    if (job.on && (int32_t)(now() - job.want) > job_stale()) job_abort();
}

/* SP2: the platform's say on slices inside a frame (weak default: as many as PC_SLICES allows). `cls` is
 * slice_class() of the slice, `waited` the ticks since the job's last slice, or -1 for a slice that runs anyway
 * (a job behind a stand-in that is due, a job's third try, an idle-time slice): the answer is ignored then, the
 * call only marks the start of the slice for plat_portrait_slice_done, which may time it. The GBA answers from
 * the last frame's work and the slice's measured cost (gba/src/pslice.c). */
__attribute__((weak)) int plat_portrait_slice_ok(int cls, int waited)
{
    (void)cls; (void)waited;
    return 1;
}
__attribute__((weak)) void plat_portrait_slice_done(int cls) { (void)cls; }

static int slice_class(void)
{
    int k = job.kind == CK_FIGURE ? 1 : job.kind == CK_BODY ? 2 : 0;
    int st = (job.step && pcv.gen != job.gen) ? SL_SETUP : job.step;   /* a clobbered canvas restarts */
    return k * SL_N + st;
}

/* run the job's next slice; restart it from the top if anything else drew on the shared canvas since its last
 * slice */
static void job_slice(void)
{
    CacheEnt *e = &ent[job.e];
    PCtx *c = &job.c;
    int st;
    if (job.step && pcv.gen != job.gen) {
        job.step = SL_SETUP;
        if (job.restarts < 255) job.restarts++;
        if (job.restarts == 2 && !job.urgent) {
            /* clobbered twice: it now has PC_STANDIN_HOLD ticks to finish (forced slices) */
            job.urgent = 1;
            job.t0 = now();
            job.fq_tick = now() - 1;
        }
    }
    st = job.step;
    if (st == SL_SETUP) {
        if (job.kind == CK_BODY) {
            pctx_init(c, &job.L, job.status, job.expr, job.bframe * 8);
            c->d = &job.D;
            body_setup(c, job.bdir, job.bframe, (uint8_t *)e->img.pix);
        } else {
            pctx_init(c, &job.L, job.status, job.expr, 0);
            c->d = &job.D;
            apply_phase(c, &job.p);
            if (job.kind == CK_FIGURE) setup_figure(c, (uint8_t *)e->img.pix);
            else setup_portrait(c, (uint8_t *)e->img.pix);
        }
    } else {
        draw_any_piece(c, st - 1);
        if (st == SL_N - 1) {
            if (job.kind == CK_BODY && (job.bdir & 3) == 2) pc_flipx();
            if (job.vac) vacant((uint8_t *)e->img.pix, e->img.w, e->img.h);
        }
    }
    job.gen = pcv.gen;
    job.last = now();
    steps_used++;
    if (++job.step >= SL_N) {
        /* ready; a hair older than what is on screen, so it is not
         * picked as the stand-in before its phase is due */
        if (job.kind != CK_BODY) {
            recent[recent_at] = content_hash(job.kind, e->rid, e->look, e->key);
            recent_at = (uint8_t)((recent_at + 1) % PC_RECENT);
        }
        e->busy = 0;
        e->stamp = cache_stamp ? cache_stamp - 1 : 0;
        e->tick = now();
        job.on = 0;
    }
}

/* Run the slices this tick still allows. A job that keeps getting interrupted gets a deadline on its third try (it
 * is urgent from then on) and finishes in one go on its sixth (one whole render in a tick, as without slicing), so
 * it always completes. A job behind a stand-in (urgent) runs
 * at least its share of what is left so that it is done PC_STANDIN ticks after it started, whatever the platform
 * says; the rest of the budget asks the platform. */
static void job_pump(void)
{
    if (job.on && job.urgent && job.fq_tick != now()) {
        /* its share of this tick, once a tick however often it is asked for (idle-time slices shrink it) */
        int left = SL_N - job.step, ticks = (int)(job.t0 + (si_ticks ? si_ticks : PC_STANDIN_HOLD) - now());
        job.fq_tick = now();
        job.fq = (uint8_t)(ticks <= 0 ? left : (left + ticks - 1) / ticks);
    }
    while (job.on) {
        int cls = slice_class(), force = job.restarts >= 5 || (job.urgent && job.fq > 0);
        if (force && job.fq) job.fq--;
        else if (!force && steps_left() <= 0) break;
        if (!plat_portrait_slice_ok(cls, force ? -1 : (int)(now() - job.last)) && !force) break;
        job_slice();
        plat_portrait_slice_done(cls);
    }
}

#if defined(DOOMO_PLATFORM_GBA)
extern uint32_t gba_work_now(void);   /* cycles of the frame's work so far (gba/src/main.c) */
#endif
/* CR9 (portrait.h): more slices of a portrait_hold() job this tick, while the frame has room */
void portrait_pump(int ms)
{
#if defined(DOOMO_PLATFORM_GBA)
    uint32_t limit = (uint32_t)ms * 16780u;
    while (job.on && job_hold && job.kind != CK_BODY && gba_work_now() < limit) {
        int cls = slice_class();
        (void)plat_portrait_slice_ok(cls, -1);
        job_slice();
        plat_portrait_slice_done(cls);
    }
#else
    (void)ms;
#endif
}

/* ---- SP2: idle time and prefetch ---------------------------------------------------------------------- */
/* One render-ahead request (portrait_prefetch / figure_prefetch): drawn in the idle time after frames (or in
 * slices the platform lets into a frame) once the job slot is free, so it is cached before it is asked for. */
static struct {
    uint8_t  on, kind, still;   /* still: asked under portrait_hold() (one still pose, phase 0) */
    int8_t   expr, status;
    int16_t  rid;
    uint32_t until;
    DoomoLook L;               /* a figure's look (prefetch of a resident reads theirs at start) */
} pf;

static const DoomoResident *res_of(int rid, int *status);
static int pf_start(void);

DOOMO_STATIC_ASSERT(3 * SL_N == PORTRAIT_SLICE_CLASSES, slice_classes_match_portrait_h);

int portrait_idle_next(void)
{
    if (!cache_ready) return -1;
    if (!job.on && !pf_start()) return -1;
    return slice_class();
}

void portrait_idle_run(void)
{
    int cls;
    if (!cache_ready || !job.on) return;
    cls = slice_class();
    (void)plat_portrait_slice_ok(cls, -1);
    job_slice();
    plat_portrait_slice_done(cls);
}

/* portrait_image / figure_image: the phase due now; while a sliced job
 * draws it, the phase shown last */
void portrait_hold(int on) { hold_on = on; }
void portrait_hold_patient(int on) { hold_patient = (uint8_t)(on != 0); }

#ifdef DOOMO_HITCHECK
static uint32_t hit_n, hit_bad;
/* Re-render an exact hit into a scratch buffer and compare. The shared canvas is clobbered, so it is
 * a sliced job in flight simply restarts (pixels are unaffected). */
static void hit_verify(int kind, int rid, const DoomoLook *l, int st, int expr, const Phase *p,
                       int status, const CacheEnt *e, int w, int h)
{
    static uint8_t buf[FIGURE_W * FIGURE_H];
    DoomoLook L = *l;
    DoomoDeep D;
    PCtx c;
    sanitize(&L);
    pctx_init(&c, &L, st, expr, 0);
    look_resolve_mask(NULL, rid, LV_ALL, &D);
    c.d = &D;
    apply_phase(&c, p);
    if (kind == CK_FIGURE) render_figure_ctx(&c, buf);
    else render_portrait_ctx(&c, buf);
    if (status == RES_EMPTY) vacant(buf, w, h);
    hit_n++;
    if (memcmp(buf, e->img.pix, (size_t)(w * h)) != 0) hit_bad++;
}
uint32_t portrait_hitcheck_count(void) { return hit_n; }
uint32_t portrait_hitcheck_fails(void) { return hit_bad; }
#else
uint32_t portrait_hitcheck_count(void) { return 0; }
uint32_t portrait_hitcheck_fails(void) { return 0; }
#endif

/* SP2: new content (look, expression or status) of something with no exact image: draw it as an urgent job and
 * show a stand-in meanwhile (an image of the same look: the same status first; any of its expressions). Returns
 * the image to show, or NULL when there is nothing to stand in: then the caller draws at once, or with `none`
 * (portrait_hold(3)) the job is started anyway (urgent) and the caller shows nothing meanwhile. */
static CacheEnt *standin(int kind, int rid, uint32_t key, uint32_t base, uint32_t lh, const DoomoLook *l, int st,
                         int expr, const Phase *p, int vac, int none)
{
    CacheEnt *o = cache_get(kind, rid, base, lh, 0x0000F000u, 0);
    if (!o) o = cache_get(kind, rid, base, lh, 0, 0);
    if (!o && !none) return NULL;
    if (!job_for(kind, rid, lh, base, 0xFFFFFF00u)) {
        if (job.on && (job.urgent || job_hold || job.si) && (int32_t)(now() - job.want) <= job_stale()) {
            /* another stand-in's job is under way: this one waits its turn (two at once would double the cost) */
        } else {
            job_abort();
            if (o) {
                o->stamp = ++cache_stamp;       /* keep the stand-in out of the claim's reach */
                o->tick = now();
            }
            if (job_begin(kind, rid, key, lh, l, st, expr, p, 0)) {
                job.urgent = !o || !recent_has(content_hash(kind, rid, lh, base));
                job.si = 1;
                job.vac = (uint8_t)vac;
            }
        }
    }
    if (job_for(kind, rid, lh, base, 0xFFFFFF00u)) job.want = now();
    if (!o) return NULL;
    o = cache_find(kind, rid, base, lh, 0x0000F000u);
    if (!o) o = cache_find(kind, rid, base, lh, 0);
    return o;
}

static const DoomoImage *animated(int kind, int rid, const DoomoLook *l, int status, int expr)
{
    uint32_t lh = look_hash(l) ^ look_run_hash(rid, 0), t = now(), base, key;
    int w = kind == CK_FIGURE ? FIGURE_W : PORTRAIT_W;
    int h = kind == CK_FIGURE ? FIGURE_H : PORTRAIT_H;
    int st = status == RES_EMPTY ? RES_ALIVE : status;
    Phase p, q;
    CacheEnt *e;
    int exact;
    base = ((uint32_t)expr << 8) | ((uint32_t)(status & 15) << 12);
    key = base | phase_of(kind, rid, expr, status, l, t, &p);
    if (job_for(kind, rid, lh, base, 0xFFFFFF00u)) job.want = t;
    if (pf.on && kind == CK_PORTRAIT && pf.kind == kind && pf.rid == rid && pf.expr == expr)
        pf.on = 0;                                          /* asked for: no longer ahead */
    e = cache_find(kind, rid, key, lh, 0xFFFFFFFFu);
    exact = e != NULL;
    if (!e && cache_get(kind, rid, key, lh, 0xFFFFFF00u, 0)) {
        /* only the phase differs: draw it in slices */
        if (!job_for(kind, rid, lh, key, 0xFFFFFFFFu)) job_start(kind, rid, key, lh, l, st, expr, &p);
        job_pump();
        e = cache_find(kind, rid, key, lh, 0xFFFFFFFFu);
        if (!e) e = cache_find(kind, rid, key, lh, 0xFFFFFF00u);
    }
    if (!e && (hold_on == 1 || hold_on == 2)) {
        /* portrait_hold(): draw the new expression / look as a sliced job
         * (it starts once the job slot is free) and meanwhile show the
         * image of this look used last, or nothing (hold 2) */
        CacheEnt *o;
        /* any phase of the new content will do: it is a few ticks old */
        if (!job_for(kind, rid, lh, base, 0xFFFFFF00u)) {
            if (job.on && !job_hold) job_abort();    /* a look-ahead can wait */
            job_start(kind, rid, key, lh, l, st, expr, &p);
            if (job_for(kind, rid, lh, key, 0xFFFFFFFFu)) { job_hold = 1; job.urgent = !hold_patient; }
        }
        if (job_for(kind, rid, lh, base, 0xFFFFFF00u)) job.want = t;
        job_pump();
        e = cache_find(kind, rid, base, lh, 0xFFFFFF00u);
        if (e) return &e->img;
        o = cache_find(kind, rid, base, lh, 0x0000F000u);
        if (o) return &o->img;
        if (hold_on == 2) return NULL;
    }
    if (!e && (hold_on == 3 || (!hold_on && si_ticks))) {
        /* SP2: a new expression / status of a look on screen: a few ticks of the old picture instead of a
         * whole render in this frame; portrait_hold(3) with nothing to stand in: a few ticks of nothing */
        CacheEnt *o = standin(kind, rid, key, base, lh, l, st, expr, &p, status == RES_EMPTY, hold_on == 3);
        if (o || hold_on == 3) {
            job_pump();
            e = cache_find(kind, rid, base, lh, 0xFFFFFF00u);
            return e ? &e->img : o ? &o->img : NULL;
        }
    }
    if (!e) {
        /* new content: draw it now */
        PCtx c;
        DoomoLook L;
        if (job_for(kind, rid, lh, key, 0xFFFFFFFFu)) job_abort();
        e = cache_alloc(kind, rid, key, lh, w, h, 0);
        L = *l;
        sanitize(&L);
        DoomoDeep D;
        pctx_init(&c, &L, st, expr, 0);
        look_resolve_mask(NULL, rid, LV_ALL, &D);
        c.d = &D;
        apply_phase(&c, &p);
        if (kind == CK_FIGURE) render_figure_ctx(&c, (uint8_t *)e->img.pix);
        else render_portrait_ctx(&c, (uint8_t *)e->img.pix);
        if (status == RES_EMPTY) vacant((uint8_t *)e->img.pix, w, h);
        steps_left();
        steps_used = steps_max();       /* that was this tick's budget      */
    }
#ifdef DOOMO_HITCHECK
    if (exact && !hold_on) hit_verify(kind, rid, l, st, expr, &p, status, e, w, h);
#endif
    (void)exact;
    /* look ahead: start on the next phase before it is due */
    if (!job.on && steps_max() < SL_N) {
        uint32_t k2 = base | phase_of(kind, rid, expr, status, l, t + PC_AHEAD, &q);
        if (k2 != key && !cache_get(kind, rid, k2, lh, 0xFFFFFFFFu, 0))
            job_start(kind, rid, k2, lh, l, st, expr, &q);
    }
    job_pump();
    return &e->img;
}

/* start the queued prefetch, if it is still wanted and not cached in any phase */
static int pf_start(void)
{
    const DoomoLook *l;
    DoomoLook al;
    uint32_t lh, base, key;
    int status, st;
    Phase p;
    if (pf_requeue) { pf_requeue = 0; pf.on = 1; }
    if (!pf.on || job.on) return 0;
    if ((int32_t)(now() - pf.until) > 0) { pf.on = 0; return 0; }
    if (pf.kind == CK_FIGURE) {
        l = &pf.L;
        status = pf.status;
    } else {
        const DoomoResident *r = res_of(pf.rid, &status);
        look_for_stage(RES_LOOK_VIEW(r), RES_STAGE(r), &al);
        l = &al;
    }
    lh = look_hash(l) ^ look_run_hash(pf.rid, 0);
    base = ((uint32_t)pf.expr << 8) | ((uint32_t)(status & 15) << 12);
    key = base | phase_of(pf.kind, pf.rid, pf.expr, status, l, now() + PC_AHEAD, &p);
    if (pf.still) { memset(&p, 0, sizeof p); key = base; }
    pf.on = 0;
    if (cache_get(pf.kind, pf.rid, key, lh, 0xFFFFFF00u, 0)) return 0;        /* already there */
    st = status == RES_EMPTY ? RES_ALIVE : status;
    if (!job_begin(pf.kind, pf.rid, key, lh, l, st, pf.expr, &p, 1)) return 0;
    job.vac = (uint8_t)(status == RES_EMPTY);
    job.pf = 1;
    job.want = pf.until;
    return 1;
}

/* is (kind, rid, look, status, expr) cached in some phase, or the job drawing it? */
static int have_any(int kind, int rid, const DoomoLook *l, int status, int expr)
{
    uint32_t lh = look_hash(l) ^ look_run_hash(rid, 0);
    uint32_t base = ((uint32_t)expr << 8) | ((uint32_t)(status & 15) << 12);
    return cache_get(kind, rid, base, lh, 0xFFFFFF00u, 0) != NULL || job_for(kind, rid, lh, base, 0xFFFFFF00u);
}

/* queue one render-ahead request (a figure keeps its look; a resident's is read when the job starts) */
static int pf_queue(int kind, int rid, const DoomoLook *l, int status, int expr)
{
    if (have_any(kind, rid, l, status, expr)) return 1;
    pf.on = 1;
    pf_requeue = 0;
    pf.kind = (uint8_t)kind;
    pf.still = (uint8_t)(hold_on != 0);
    pf.rid = (int16_t)rid;
    pf.expr = (int8_t)expr;
    pf.status = (int8_t)status;
    if (kind == CK_FIGURE) pf.L = *l;
    pf.until = now() + PC_PREFETCH;
    return 0;
}

int portrait_prefetch(int resident_id, int expr)
{
    int status;
    DoomoLook al;
    const DoomoResident *r;
    if (expr < 0 || expr >= EXPR_COUNT) return 1;
    cache_init();
    r = res_of(resident_id, &status);
    look_for_stage(RES_LOOK_VIEW(r), RES_STAGE(r), &al);
    return pf_queue(CK_PORTRAIT, resident_id, &al, status, expr);
}

int figure_prefetch(const DoomoLook *l, int status, int expr)
{
    if (!l || expr < 0 || expr >= EXPR_COUNT) return 1;
    if (status < 0 || status > 15) status = RES_ALIVE;
    cache_init();
    return pf_queue(CK_FIGURE, -1, l, status, expr);
}

const DoomoImage *portrait_image(int resident_id, int expr)
{
    int status;
    const DoomoResident *r;
    DoomoLook al;
    cache_init();
    r = res_of(resident_id, &status);
    if (expr < 0 || expr >= EXPR_COUNT) expr = EXPR_NEUTRAL;
    look_for_stage(RES_LOOK_VIEW(r), RES_STAGE(r), &al);       /* F4: the face of their age */
    return animated(CK_PORTRAIT, resident_id, &al, status, expr);
}

/* CR9: the cached bust of any look (the creator's draft), like figure_image */
const DoomoImage *portrait_image_look(const DoomoLook *l, int status, int expr)
{
    cache_init();
    if (!l) return NULL;
    if (expr < 0 || expr >= EXPR_COUNT) expr = EXPR_NEUTRAL;
    if (status < 0 || status > 15) status = RES_ALIVE;
    return animated(CK_PORTRAIT, -1, l, status, expr);
}

const DoomoImage *figure_image(const DoomoLook *l, int status, int expr)
{
    static DoomoResident def;
    static DoomoLook def_look;
    static uint8_t def_ready;
    cache_init();
    if (!l) {
        if (!def_ready) { resident_default(&def); res_get_look(&def, &def_look); def_ready = 1; }
        l = &def_look;
    }
    if (expr < 0 || expr >= EXPR_COUNT) expr = EXPR_NEUTRAL;
    if (status < 0 || status > 15) status = RES_ALIVE;
    return animated(CK_FIGURE, -1, l, status, expr);
}

/* B (docs/STUDIO_30FPS_PLAN.md): the 48-byte gene compare of the look-hash memo, word by word. DoomoResident has no
 * member wider than a byte, so the compiler cannot assume a record is word aligned: the test is at run time and the
 * byte-wise memcmp stays as the fallback (an unaligned word load traps on the R3000 and misreads on the ARM7). Same
 * answer as memcmp(a, b, 48) == 0 on every input. */
typedef uint32_t __attribute__((may_alias)) gene_w_t;
static inline int gene_same(const uint8_t *a, const uint8_t *b)
{
    if (((uintptr_t)a | (uintptr_t)b) & 3u) return memcmp(a, b, 48) == 0;
    {
        const gene_w_t *x = (const gene_w_t *)a, *y = (const gene_w_t *)b;
        return ((x[0] ^ y[0]) | (x[1] ^ y[1]) | (x[2] ^ y[2]) | (x[3] ^ y[3]) | (x[4] ^ y[4]) | (x[5] ^ y[5]) |
                (x[6] ^ y[6]) | (x[7] ^ y[7]) | (x[8] ^ y[8]) | (x[9] ^ y[9]) | (x[10] ^ y[10]) |
                (x[11] ^ y[11])) == 0;
    }
}

/* V8 (docs/INVADED_SPEED.md): the look hash of a body, memoised on the resident's raw record. A citizen's body
 * is asked for every tick (7 a tick on the invaded map), and unpacking the 40-odd look genes, ageing the look and
 * hashing it was ~3% of the GBA frame. The hash is a pure function of the gene bytes and `species` (look_for_stage
 * and look_hash read nothing else), so a hit needs both unchanged: any edit to the look re-computes it. The deep
 * run (look_run_hash) is not memoised; it is a walk of a pool that is empty or tiny. 40 entries (fully
 * associative since MS-P4), 60 B each; a miss only costs the old path. */
#define BLH_N 40
typedef struct BodyLH {
    uint8_t  gene[48];
    uint32_t h;
    int32_t  id;
    uint8_t  species, ok;
    uint8_t  ei;                    /* B: the cache entry this body was last found in; a hint, checked on every use */
} BodyLH;
static BodyLH blh[BLH_N];

/* V11 (docs/INVADED_SPEED.md): a body the cache does not have is ~20 ms of one frame on the GBA, and the invaded
 * map asks for one every few seconds (a citizen turning to a facing it has not shown yet). With bodyspr_lazy(1)
 * (the overworld sets it for that map only) such a miss is drawn as a sliced job, one layer group a tick through
 * the portrait job slot, and meanwhile the citizen shows a facing it already has (or is not drawn, if it has
 * none). Pixels are the same once drawn, nothing but the picture is delayed; logic never sees it. GBA only. */
#if defined(DOOMO_PLATFORM_GBA)
static uint8_t body_lazy;
void bodyspr_lazy(int on) { body_lazy = (uint8_t)on; }   /* 1: a citizen may be skipped, 2: never skip */
static int body_job_start(int rid, uint32_t key, uint32_t lh, const DoomoLook *L, int status, int dir, int frame)
{
    CacheEnt *e = cache_alloc(CK_BODY, rid, key, lh, BODYSPR_W, BODYSPR_H, 1);
    if (!e) return 0;
    e->busy = 1;
    job_hold = 0;
    job.on = 1;
    job.step = SL_SETUP;
    job.restarts = 0;
    job.urgent = 0;
    job.vac = 0;
    job.pf = 0;
    job.si = 0;
    job.kind = CK_BODY;
    job.e = (int8_t)(e - ent);
    job.want = job.t0 = job.last = now();
    job.status = status == RES_EMPTY ? RES_ALIVE : status;
    job.expr = EXPR_NEUTRAL;
    job.bdir = (uint8_t)dir;
    job.bframe = (uint8_t)frame;
    job.L = *L;
    sanitize(&job.L);
    look_resolve_mask(NULL, rid, LV_CHIBI, &job.D);
    return 1;
}
#else
void bodyspr_lazy(int on) { (void)on; }
#endif

const DoomoImage *bodyspr_image(int resident_id, int dir, int frame)
{
    int status, have_look = 0;
    const DoomoResident *r;
    uint32_t lh, key, lhl;
    CacheEnt *e;
    PCtx c;
    DoomoLook L;
    BodyLH *m;
    cache_init();
#if defined(DOOMO_PLATFORM_GBA)
    if (job.on && job.kind == CK_BODY) job_pump();            /* V11: one slice a tick */
#endif
    r = res_of(resident_id, &status);
    dir &= 3;
    frame &= 3;
    /* MS-P4: fully associative (a 25-voice choir collided in the old 32 direct-mapped slots and redid the
     * look + hash for most singers every frame); a miss replaces slots round-robin */
    {
        static uint8_t blh_next, blh_hint[64];
        int bi, hi = resident_id & 63;
        m = NULL;
        /* MS-P2: the slot this id used last is tried first (a choir asks for the same 25 every frame) */
        bi = blh_hint[hi];
        if (bi < BLH_N && blh[bi].ok && blh[bi].id == resident_id) m = &blh[bi];
        else {
            for (bi = 0; bi < BLH_N; bi++)
                if (blh[bi].ok && blh[bi].id == resident_id) { m = &blh[bi]; break; }
        }
        if (!m) { bi = blh_next; m = &blh[bi]; blh_next = (uint8_t)((blh_next + 1) % BLH_N); m->ok = 0; }
        blh_hint[hi] = (uint8_t)bi;
    }
    if (m->ok && m->id == resident_id && m->species == r->species &&
        gene_same(m->gene, r->gene)) {
        lhl = m->h;
    } else {
        look_for_stage(RES_LOOK_VIEW(r), RES_STAGE(r), &L);    /* F4: the body of their age */
        have_look = 1;
        lhl = look_hash(&L);
        memcpy(m->gene, r->gene, sizeof m->gene);
        m->species = r->species;
        m->id = resident_id;
        m->h = lhl;
        m->ok = 1;
    }
    lh = lhl ^ look_run_hash(resident_id, 1);             /* chibi sees only DFF_CHIBI fields */
    key = (uint32_t)(dir | (frame << 2)) | ((uint32_t)status << 12);
    /* B: try the entry this body was found in last time before the general lookup. It is accepted only when it
     * passes the very test cache_get applies (kind, ready, resident, look hash including the deep run, key), so a
     * look, species, status, run or facing change, an eviction or a reuse of its blocks all fail it and fall through
     * to the old path; the touch is the one cache_get makes (stamp and tick, so eviction order is unchanged). What a
     * hit skips is the memo hash and the scan of cache_find. */
    e = &ent[m->ei < PC_BLOCKS ? m->ei : 0];
    if (e->kind == CK_BODY && !e->busy && e->rid == resident_id && e->look == lh && e->key == key) {
        e->stamp = ++cache_stamp;
        e->tick = now();
        return &e->img;
    }
    e = cache_find(CK_BODY, resident_id, key, lh, 0xFFFFFFFFu);
    if (e) {
        m->ei = (uint8_t)(e - ent);
        return &e->img;
    }
    if (!have_look) look_for_stage(RES_LOOK_VIEW(r), RES_STAGE(r), &L);
#if defined(DOOMO_PLATFORM_GBA)
    if (body_lazy) {
        const uint32_t smask = 0xFFFFF000u;                   /* same look and status, any facing / frame */
        CacheEnt *st = cache_get(CK_BODY, resident_id, key, lh, smask, 0);
        if (job.on && job.kind == CK_BODY && job_for(CK_BODY, resident_id, lh, key, 0xFFFFFFFFu)) {
            job.want = now();
        } else if (!job.on) {
            body_job_start(resident_id, key, lh, &L, status, dir, frame);   /* its first slice runs next tick */
            if (st) st = cache_get(CK_BODY, resident_id, key, lh, smask, 0);   /* the claim may have evicted it */
        }
        if (st || body_lazy == 1) return st ? &st->img : NULL;   /* NULL: not drawn for a few ticks */
        /* body_lazy == 2 (the player) with nothing cached to show: draw it now, as before */
    }
#endif
    if (si_ticks && job.on && job.kind != CK_BODY && job.step > SL_SETUP &&
        (job.urgent || now() - job.last <= 1)) {
        /* SP2: a portrait / figure job is half way through the shared canvas: a facing this body has stands in
         * (a few ticks at most: the job is due or moving) instead of a render that would restart the job */
        CacheEnt *st = cache_get(CK_BODY, resident_id, key, lh, 0xFFFFF000u, 1);
        if (st) return &st->img;
    }
    job_reap();
    e = cache_alloc(CK_BODY, resident_id, key, lh, BODYSPR_W, BODYSPR_H, 0);
    sanitize(&L);
    {
        DoomoDeep D;
        pctx_init(&c, &L, status == RES_EMPTY ? RES_ALIVE : status, EXPR_NEUTRAL, frame * 8);
        look_resolve_mask(NULL, resident_id, LV_CHIBI, &D);
        c.d = &D;
        render_body_ctx(&c, dir, frame, (uint8_t *)e->img.pix);
    }
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
    look_for_stage(RES_LOOK_VIEW(r), RES_STAGE(r), &L);        /* F4: the body of their age */
    lh = look_hash(&L) ^ look_run_hash(resident_id, 0);
    key = (uint32_t)(view | (anim << 3) | (weapon_frame << 6)) | ((uint32_t)status << 12);
    e = cache_find(CK_FIGHTER, resident_id, key, lh, 0xFFFFFFFFu);
    if (e) return &e->img;
    job_reap();
    e = cache_alloc(CK_FIGHTER, resident_id, key, lh, FIGHTER_W, FIGHTER_H, 0);
    sanitize(&L);
    {
        DoomoDeep D;
        pctx_init(&c, &L, status == RES_EMPTY ? RES_ALIVE : status, EXPR_NEUTRAL, anim * 8);
        look_resolve_mask(NULL, resident_id, LV_ALL, &D);
        c.d = &D;
        render_fighter_ctx(&c, view, anim, weapon_frame, (uint8_t *)e->img.pix);
    }
    return &e->img;
}

/* other modules' small images (portrait.h): rid -2, the size in `look` */
const DoomoImage *portrait_pool_image(uint32_t key, int w, int h, int *fresh)
{
    uint32_t sz = (uint32_t)w | ((uint32_t)h << 16);
    CacheEnt *e;
    *fresh = 0;
    if (w <= 0 || h <= 0 || w * h > PORTRAIT_POOL_IMAGE_MAX) return NULL;
    cache_init();
    e = cache_find(CK_FOREIGN, -2, key, sz, 0xFFFFFFFFu);
    if (e) return &e->img;
    job_reap();
    e = cache_alloc(CK_FOREIGN, -2, key, sz, w, h, 0);
    if (!e) return NULL;
    *fresh = 1;
    return &e->img;
}

void portrait_invalidate(int resident_id)
{
    int i;
    if (!cache_ready) return;
    for (i = 0; i < PC_BLOCKS; i++)
        if (ent[i].kind != CK_NONE && ent[i].kind != CK_FIGURE && ent[i].rid == resident_id) {
            if (job.on && i == job.e) job_abort();
            else cache_drop(i);
        }
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
    fake_on = 0;
    steps_cfg = 0;
    steps_used = 0;
    si_ticks = PC_STANDIN;
    hold_patient = 0;
    pf.on = 0;
    cache_init();
}

void portrait_cache_standin(int ticks)
{
    si_ticks = (uint8_t)(ticks < 0 ? 0 : ticks > 30 ? 30 : ticks);
}

void portrait_cache_clock(uint32_t tick, int steps)
{
    fake_on = 1;
    fake_tick = tick;
    steps_cfg = steps > 0 ? steps : 0;
}
