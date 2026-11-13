/* portrait_script.c - the part-script VM, materials, regions and stamps (P1.M1; SPEC 4.4).
 * Owner: customisation P1.
 *
 * A script (include/doomo/parts.h, built by tools/parts_build.py) draws parts in normalised head or
 * torso space with the same primitives the painters use, so a part can be added or changed without
 * a code edit. Nothing here keeps state of its own except the 16-entry material memo in the PCtx;
 * every loop is bounded by the script length (<= 255 B a block).
 *
 * CR2: any number of script sets (core/gen/parts_index.c), u16 list ids (gene fields and deep
 * fields), the family macro ops (crown, brim, band, strap, bow, ring, chain, frill, pouch) and the
 * SLOT HOOKS: each painter asks ps_slot() first, and a worn value that has a script is drawn by the
 * script instead of the painter (tools/parts_build.py HOOKS lists the groups each list runs in). A
 * look with no scripted value costs one byte compare per hook (doomo_part_minv). */
#include "doomo/doomo.h"
#include "doomo/gene.h"
#include "doomo/parts.h"
#include "doomo/look_pool.h"
#include "portrait_int.h"

/* ---- pm_of ------------------------------------------------------------------------------- */
static const PMat *base_mat(PCtx *c, int lf)
{
    switch (lf) {
    case GF_HAIR_COLOR:       return &c->hair;
    case GF_HAIR_HIGHLIGHT:   return &c->hairhi;
    case GF_EYE_COLOR:        return &c->eye;
    case GF_HORN_COLOR:       return &c->horn;
    case GF_MARKING_COLOR:    return &c->mark;
    case GF_TOP_COLOR:        return &c->top;
    case GF_BOTTOM_COLOR:     return &c->bot;
    case GF_SHOES_COLOR:      return &c->shoe;
    case GF_HAT_COLOR:        return &c->hat;
    case GF_ACCESSORY_COLOR:  return &c->acc;
    case GF_PATTERN_COLOR:    return &c->patm;
    default:                  return &c->skin;
    }
}

/* a memo slot for `key` (found, or a free / overwritten one); *hit = 1 when it was there */
static PMemo *memo_slot(PCtx *c, uint16_t key, int *hit)
{
    int i, free_slot = -1;
    for (i = 0; i < 16; i++) {
        if (c->memo[i].used && c->memo[i].key == key) { *hit = 1; return &c->memo[i]; }
        if (!c->memo[i].used && free_slot < 0) free_slot = i;
    }
    *hit = 0;
    return &c->memo[free_slot >= 0 ? free_slot : (key & 15)];   /* a full memo overwrites by key */
}

/* CR3: the third fur colour (fur_color3: 0 = the marking colour, p + 1 = paint p) */
static const PMat *fur3_mat(PCtx *c)
{
    int v = c->d->v[DF_FUR_COLOR3], hit;
    PMemo *e;
    if (!v) return &c->mark;
    e = memo_slot(c, (uint16_t)((GF_FUR_COLOR3 << 2) | 3), &hit);
    if (!hit) {
        e->used = 1;
        e->key = (uint16_t)((GF_FUR_COLOR3 << 2) | 3);
        pm_paint(&e->m, (uint8_t)((v - 1) % PAL_PAINT_COUNT));
    }
    return &e->m;
}

const PMat *pm_of(PCtx *c, int lf, int tone)
{
    const PMat *b = lf == GF_FUR_COLOR3 ? fur3_mat(c) : base_mat(c, lf);
    uint16_t key;
    PMemo *e;
    int i, free_slot = -1;
    if (tone != PM_LIGHT && tone != PM_DARK) return b;
    key = (uint16_t)((lf << 2) | tone);
    for (i = 0; i < 16; i++) {
        if (c->memo[i].used && c->memo[i].key == key) return &c->memo[i].m;
        if (!c->memo[i].used && free_slot < 0) free_slot = i;
    }
    e = &c->memo[free_slot >= 0 ? free_slot : (key & 15)];   /* a full memo overwrites by key */
    e->used = 1;
    e->key = key;
    if (tone == PM_LIGHT) {
        e->m.c[0] = b->c[1]; e->m.c[1] = b->c[2]; e->m.c[2] = b->c[3];
        e->m.c[3] = b->c[4]; e->m.c[4] = b->c[4];
    } else {
        e->m.c[0] = b->c[0]; e->m.c[1] = b->c[0]; e->m.c[2] = b->c[1];
        e->m.c[3] = b->c[2]; e->m.c[4] = b->c[3];
    }
    e->m.line = pm_line_of(e->m.c[1]);
    return &e->m;
}

/* ---- regions ----------------------------------------------------------------------------- */
void pc_region(PCtx *c, int region)
{
    const PHead *h = &c->h;
    int X, Y, erx, ery;
    switch (region) {
    case REG_HEAD:
        pc_clip((h->cx - h->rx) >> 4, (h->cy - h->ry) >> 4, ((h->cx + h->rx) >> 4) + 1,
                ((h->cy + h->ry) >> 4) + 1);
        break;
    case REG_EYE_L:
    case REG_EYE_R:
        if (ph_eye_geom(c, region == REG_EYE_L ? -1 : 1, &X, &Y, &erx, &ery) <= 0) {
            pc_clip(0, 0, 0, 0);                       /* turned away: nothing to draw into */
            break;
        }
        pc_clip((X - erx * 3 / 2) >> 4, (Y - ery * 3 / 2) >> 4, ((X + erx * 3 / 2) >> 4) + 1,
                ((Y + ery * 3 / 2) >> 4) + 1);
        break;
    case REG_MUZZLE:                                   /* the lower half of the face */
        pc_clip((h->cx - h->rx * 3 / 4) >> 4, (h->cy + h->ry / 8) >> 4,
                ((h->cx + h->rx * 3 / 4) >> 4) + 1, ((h->cy + h->ry) >> 4) + 1);
        break;
    case REG_TORSO:
        pc_clip((c->bx - c->bw) >> 4, (c->by - c->bh) >> 4, ((c->bx + c->bw) >> 4) + 1,
                ((c->by + c->bh) >> 4) + 1);
        break;
    default:
        pc_noclip();
        break;
    }
}

/* ---- stamps ------------------------------------------------------------------------------ */
void pc_stamp(PCtx *c, const uint8_t *m1bpp, int w, int h, int x, int y, int q4, const PMat *m)
{
    int dw, dh, dx, dy, x0, y0, rb;
    (void)c;
    if (!m1bpp || !m || w < 1 || h < 1 || w > 16 || h > 16 || q4 < 1) return;
    dw = w * q4 / 16; dh = h * q4 / 16;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > 48) dw = 48;                              /* the largest canvas is 48 wide */
    if (dh > 80) dh = 80;
    x0 = (x - dw * 8) >> 4;
    y0 = (y - dh * 8) >> 4;
    rb = (w + 7) >> 3;
    for (dy = 0; dy < dh; dy++) {
        int sy = dy * h / dh;
        for (dx = 0; dx < dw; dx++) {
            int sx = dx * w / dw;
            if (m1bpp[sy * rb + (sx >> 3)] & (0x80 >> (sx & 7))) pc_px(x0 + dx, y0 + dy, m->c[2]);
        }
    }
}

/* ---- the VM ------------------------------------------------------------------------------ */
#if DOOMO_PARTS_VM
static const PartRef *find_ref(const PartSet *set, int list, int value)
{
    int i, lo, hi;
    for (i = 0; i < set->nlists; i++)                   /* a set holds one list, or a few */
        if (set->lists[i].list == list) break;
    if (i == set->nlists) return NULL;
    lo = set->lists[i].first;
    hi = lo + set->lists[i].n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const PartRef *r = &set->ref[mid];
        if (r->value == value) return r;
        if (r->value < value) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static const PartSet *find_set(int list, int value, const PartRef **ref)
{
    int lo = 0, hi = (int)doomo_part_script_nlists - 1;
    if (list < 128 && value < doomo_part_minv[list]) return NULL;   /* the common case: one compare */
    while (lo <= hi) {                                  /* the set that holds the list */
        int mid = (lo + hi) >> 1;
        const PartListSet *e = &doomo_part_script_lists[mid];
        if (e->list == list) {
            const PartSet *set = doomo_part_scripts[e->set];
            const PartRef *r;
            if (value < e->minv) return NULL;
            r = find_ref(set, list, value);
            if (!r) return NULL;
            *ref = r;
            return set;
        }
        if (e->list < list) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

int ps_has(int list, int value)
{
    const PartRef *r;
    return find_set(list, value, &r) != NULL;
}

int ps_top(int list, int value)
{
    const PartRef *r;
    return find_set(list, value, &r) ? r->top * 8 : 0;
}

typedef struct VM {
    PCtx *c;
    const PartSet *set;
    int anchor, scale, mirror, shade;
    const PMat *m;
    int decal;
} VM;

static int i8(const uint8_t *p, int k) { return (int8_t)p[k] * 2; }

/* CR3: the frame of PA_FRAME blocks (ps_frame): origin, position units and radius units, Q4 per 256 */
static struct { int16_t ox, oy, px, py, rx, ry; } fr;   /* int16: these are screen units, no EWRAM to spare */

void ps_frame(int ox, int oy, int px, int py, int rx, int ry)
{
    fr.ox = ox; fr.oy = oy; fr.px = px; fr.py = py;
    fr.rx = rx < 0 ? -rx : rx; fr.ry = ry < 0 ? -ry : ry;
}

/* normalised -> Q4 for the anchor; s = +1 / -1 mirror side */
static int vx(const VM *v, int n, int s)
{
    PCtx *c = v->c;
    n = n * v->scale / 64 * s;
    if (v->anchor == PA_TORSO) return c->bx + n * c->bw / 256;
    if (v->anchor == PA_FRAME) return fr.ox + n * fr.px / 256;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = 0;
    return hx(n);                                       /* yaw handled by the head space */
}

static int vy(const VM *v, int n)
{
    PCtx *c = v->c;
    n = n * v->scale / 64;
    if (v->anchor == PA_FRAME) return fr.oy + n * fr.py / 256;
    return v->anchor == PA_TORSO ? c->by + n * c->bh / 256 : c->h.cy + n * c->h.ry / 256;
}

static int vrx(const VM *v, int r)
{
    r = r * v->scale / 64;
    if (v->anchor == PA_FRAME) return r * fr.rx / 256;
    return v->anchor == PA_TORSO ? r * v->c->bw / 256 : r * v->c->h.rx / 256;
}

static int vry(const VM *v, int r)
{
    r = r * v->scale / 64;
    if (v->anchor == PA_FRAME) return r * fr.ry / 256;
    return v->anchor == PA_TORSO ? r * v->c->bh / 256 : r * v->c->h.ry / 256;
}

static int cond_ok(const VM *v, int op, int mask)
{
    const PCtx *c = v->c;
    switch (op) {
    case PO_IFVIEW: return (mask >> c->view_kind) & 1;
    case PO_IFYAW:  return (mask >> (v->anchor == PA_TORSO ? c->byaw : c->h.yaw)) & 1;
    default:
        return ((mask & 1) && c->blink) || ((mask & 2) && c->breath) ||
               ((mask & 4) && c->twitch) || ((mask & 8) && c->wag);
    }
}

/* ---- CR2 macro ops: the part families' shape grammar ----------------------------------------
 * Every macro draws with the VM's current material and shade, in its current solid / decal mode,
 * through vx / vy / vrx / vry, so it follows the anchor, the scale, the yaw and the sliders exactly
 * like the primitives. A macro that needs a second tone (a band, a strap edge, a knot) takes it from
 * the same material (c[0] / c[1] / c[3]), so colour stays in the colour picker. */
static void vm_part(VM *v)                              /* a new solid part, or keep decal mode */
{
    if (v->decal) pc_decal(v->m, v->shade);
    else pc_part(v->m, v->shade, 0);
}

static void vm_restore(VM *v)                           /* back to the VM's material and mode */
{
    if (v->decal) pc_decal(v->m, v->shade);
    else pc_same(v->m, v->shade);
}

static void macro_hat(VM *v, int op, int kind, int x, int y, int w, int h)
{
    int X = vx(v, x, 1), Y = vy(v, y), W = vrx(v, w), H = vry(v, h);
    if (W < 1 || H < 1) return;
    if (op == PO_CROWN) {
        switch (kind) {
        case PCROWN_DOME:                               /* a half ellipse standing on y */
            pc_clip(0, 0, pcv.w, (Y >> 4) + 1);
            pc_ellipse(X, Y, W, H);
            pc_noclip();
            break;
        case PCROWN_CONE:
            pc_tri(X - W, Y, X + W, Y, X, Y - 2 * H);
            break;
        case PCROWN_CYLINDER:
            pc_rrect(X, Y - H, W, H, W / 5 + 1);
            break;
        case PCROWN_FLAT:                               /* a pillbox: short and wide */
            pc_rrect(X, Y - H / 2, W, H / 2, H / 3 + 1);
            break;
        case PCROWN_SLOUCH:                             /* a dome that sags to one side */
            pc_clip(0, 0, pcv.w, (Y >> 4) + 1);
            pc_ellipse(X, Y, W, H);
            pc_noclip();
            pc_capsule(X + W / 2, Y - H / 2, X + W + W / 3, Y + H / 3, H / 3 + 1, H / 5 + 1);
            break;
        case PCROWN_PEAKED:                             /* a cone with a rounded base */
            pc_ellipse(X, Y - H / 3, W, H / 2);
            pc_tri(X - W * 3 / 4, Y - H / 3, X + W * 3 / 4, Y - H / 3, X + W / 4, Y - 2 * H);
            break;
        default:                                        /* PCROWN_BERET: a tilted disc */
            pc_ellipse_sh(X + W / 6, Y - H / 2, W, H / 2, 40);
            break;
        }
        return;
    }
    switch (kind) {
    case PBRIM_BILL:                                    /* a cap's bill, in front of the face */
        pc_quad(X - W, Y - H, X + W, Y - H, X + W * 5 / 4, Y + H, X - W * 5 / 4, Y + H);
        break;
    case PBRIM_SMALL:
        pc_ellipse(X, Y, W, H);
        break;
    case PBRIM_WIDE:
        pc_ellipse(X, Y, W * 3 / 2, H);
        break;
    case PBRIM_FLOPPY:                                  /* wide, the sides drooping */
        pc_ellipse(X, Y, W * 3 / 2, H);
        pc_capsule(X - W * 3 / 2 + H, Y, X - W * 3 / 2, Y + H * 2, H, H / 2 + 1);
        pc_capsule(X + W * 3 / 2 - H, Y, X + W * 3 / 2, Y + H * 2, H, H / 2 + 1);
        break;
    case PBRIM_COCKED:                                  /* the sides turned up */
        pc_ellipse(X, Y, W, H);
        pc_tri(X - W, Y + H / 2, X - W * 3 / 4, Y - H * 2, X - W / 3, Y);
        pc_tri(X + W, Y + H / 2, X + W * 3 / 4, Y - H * 2, X + W / 3, Y);
        break;
    default:                                            /* PBRIM_NONE */
        break;
    }
}

static void macro_box(VM *v, int op, int x, int y, int w, int h)
{
    int X = vx(v, x, 1), Y = vy(v, y), W = vrx(v, w), H = vry(v, h);
    const PMat *m = v->m;
    if (W < 1 || H < 1) return;
    if (op == PO_BAND) {                                /* a band of its own: the contour shows it */
        PMat b;
        pm_from3(&b, m->c[0], m->c[1], m->c[2]);
        if (v->decal) pc_decal(&b, SH_FLAT); else pc_part(&b, SH_FLAT, PF_NOCAST);
        pc_rect(X - W, Y - H, X + W, Y + H);
        vm_restore(v);
        return;
    }
    vm_part(v);                                         /* PO_POUCH */
    pc_rrect(X, Y, W, H, (W < H ? W : H) / 3 + 1);
    pc_decal(m, SH_FLAT);
    pc_lineq(X - W + 8, Y - H / 3, X + W - 8, Y - H / 3, m->c[0]);
    pc_px(X >> 4, (Y - H / 3 + 16) >> 4, m->c[4]);
    vm_restore(v);
}

static void macro_strap(VM *v, int x0, int y0, int x1, int y1, int w)
{
    int X0 = vx(v, x0, 1), Y0 = vy(v, y0), X1 = vx(v, x1, 1), Y1 = vy(v, y1), W = vrx(v, w);
    if (W < 1) W = 1;
    vm_part(v);
    pc_capsule(X0, Y0, X1, Y1, W, W);
    pc_decal(v->m, SH_FLAT);
    pc_lineq(X0, Y0 + W / 2, X1, Y1 + W / 2, v->m->c[1]);
    vm_restore(v);
}

static void macro_bow(VM *v, int x, int y, int s)
{
    int X = vx(v, x, 1), Y = vy(v, y), S = vrx(v, s), T = vry(v, s);
    PMat k;
    if (S < 2) S = 2;
    if (T < 2) T = 2;
    vm_part(v);
    pc_tri(X, Y, X - S, Y - T * 2 / 3, X - S, Y + T * 2 / 3);
    pc_tri(X, Y, X + S, Y - T * 2 / 3, X + S, Y + T * 2 / 3);
    pm_from3(&k, v->m->c[1], v->m->c[2], v->m->c[3]);
    if (v->decal) pc_decal(&k, SH_BALL); else pc_part(&k, SH_BALL, PF_NOCAST);
    pc_ellipse(X, Y, S / 3 + 4, T / 3 + 4);
    vm_restore(v);
}

static void macro_ring(VM *v, int x, int y, int r, int t)
{
    int X = vx(v, x, 1), Y = vy(v, y), RX = vrx(v, r), RY = vry(v, r), T = vrx(v, t), k;
    if (T < 6) T = 6;
    vm_part(v);
    for (k = 0; k < 8; k++) {                           /* eight capsules round the ellipse */
        int a0 = k * 32, a1 = a0 + 32;
        pc_capsule(X + RX * p_cos(a0) / 256, Y + RY * p_sin(a0) / 256,
                   X + RX * p_cos(a1) / 256, Y + RY * p_sin(a1) / 256, T, T);
    }
}

static void macro_chain(VM *v, int x0, int y0, int x1, int y1, int n, int sag)
{
    int X0 = vx(v, x0, 1), Y0 = vy(v, y0), X1 = vx(v, x1, 1), Y1 = vy(v, y1), S = vry(v, sag), k;
    if (n < 1) n = 1;
    if (n > 24) n = 24;
    vm_part(v);
    for (k = 0; k <= n; k++) {                          /* a parabola from end to end */
        int X = X0 + (X1 - X0) * k / n, Y = Y0 + (Y1 - Y0) * k / n;
        int d = n * n / 4 - (k - n / 2) * (k - n / 2);
        Y += n > 1 ? S * d * 4 / (n * n) : 0;
        pc_ellipse(X, Y, 12 + (k & 1) * 4, 12 + ((k + 1) & 1) * 4);
    }
}

static void macro_frill(VM *v, int x0, int x1, int y, int n, int r)
{
    int X0 = vx(v, x0, 1), X1 = vx(v, x1, 1), Y = vy(v, y), R = vrx(v, r), RY = vry(v, r), k;
    if (n < 1) n = 1;
    if (n > 24) n = 24;
    vm_part(v);
    for (k = 0; k < n; k++) {
        int X = n > 1 ? X0 + (X1 - X0) * k / (n - 1) : (X0 + X1) / 2;
        pc_ellipse(X, Y, R, RY);
    }
}

/* one block; returns the ops executed */
static int run_block(VM *v, const uint8_t *p, int len)
{
    int i = 0, n = 0, sides, k;
    while (i < len) {
        int op = p[i++];
        const uint8_t *a = p + i;
        sides = v->mirror ? 2 : 1;
        n++;
        switch (op) {
        case PO_END:
            return n;
        case PO_ANCHOR:  v->anchor = a[0] == PA_TORSO || a[0] == PA_FRAME ? a[0] : PA_HEAD; i += 1; break;
        case PO_MAT:     v->m = pm_of(v->c, a[0], a[1]); i += 2; break;
        case PO_SHADE:   v->shade = a[0] < SH_COUNT ? a[0] : SH_BALL; i += 1; break;
        case PO_SOLID:
            v->decal = 0;
            if (a[0] & PSF_SAME) pc_same(v->m, v->shade); else pc_part(v->m, v->shade, a[0]);
            i += 1;
            break;
        case PO_DECAL:   v->decal = 1; pc_decal(v->m, v->shade); break;
        case PO_MIRROR:  v->mirror = a[0] != 0; i += 1; break;
        case PO_SCALE:   v->scale = a[0] ? a[0] : 64; i += 1; break;
        case PO_REGION:  pc_region(v->c, a[0]); i += 1; break;
        case PO_ELLIPSE:
            for (k = 0; k < sides; k++)
                pc_ellipse(vx(v, i8(a, 0), k ? -1 : 1), vy(v, i8(a, 1)), vrx(v, a[2]), vry(v, a[3]));
            i += 4;
            break;
        case PO_CAPSULE:
            for (k = 0; k < sides; k++)
                pc_capsule(vx(v, i8(a, 0), k ? -1 : 1), vy(v, i8(a, 1)), vx(v, i8(a, 2), k ? -1 : 1),
                           vy(v, i8(a, 3)), vrx(v, a[4]), vrx(v, a[5]));
            i += 6;
            break;
        case PO_TRI:
            for (k = 0; k < sides; k++) {
                int s = k ? -1 : 1;
                pc_tri(vx(v, i8(a, 0), s), vy(v, i8(a, 1)), vx(v, i8(a, 2), s), vy(v, i8(a, 3)),
                       vx(v, i8(a, 4), s), vy(v, i8(a, 5)));
            }
            i += 6;
            break;
        case PO_LINE:
            for (k = 0; k < sides; k++) {
                int s = k ? -1 : 1;
                pc_lineq(vx(v, i8(a, 0), s), vy(v, i8(a, 1)), vx(v, i8(a, 2), s), vy(v, i8(a, 3)),
                         v->m->c[a[4] < 5 ? a[4] : 2]);
            }
            i += 5;
            break;
        case PO_DOT:
            for (k = 0; k < sides; k++)
                pc_px(vx(v, i8(a, 0), k ? -1 : 1) >> 4, vy(v, i8(a, 1)) >> 4, v->m->c[a[2] < 5 ? a[2] : 2]);
            i += 3;
            break;
        case PO_STAMP:
            if (a[0] < v->set->nst) {
                const StampRef *s = &v->set->st[a[0]];
                pc_stamp(v->c, v->set->bits + s->ofs, s->w, s->h, vx(v, i8(a, 1), 1), vy(v, i8(a, 2)),
                         a[3] ? a[3] : 16, v->m);
            }
            i += 4;
            break;
        case PO_IFVIEW: case PO_IFYAW: case PO_IFANIM:
            i += 2;
            if (!cond_ok(v, op, a[0])) i += a[1];
            break;
        case PO_CROWN: case PO_BRIM:
            for (k = 0; k < sides; k++) macro_hat(v, op, a[0], i8(a, 1) * (k ? -1 : 1), i8(a, 2), a[3], a[4]);
            i += 5;
            break;
        case PO_BAND: case PO_POUCH:
            for (k = 0; k < sides; k++) macro_box(v, op, i8(a, 0) * (k ? -1 : 1), i8(a, 1), a[2], a[3]);
            i += 4;
            break;
        case PO_STRAP:
            for (k = 0; k < sides; k++) {
                int s = k ? -1 : 1;
                macro_strap(v, i8(a, 0) * s, i8(a, 1), i8(a, 2) * s, i8(a, 3), a[4]);
            }
            i += 5;
            break;
        case PO_BOW:
            for (k = 0; k < sides; k++) macro_bow(v, i8(a, 0) * (k ? -1 : 1), i8(a, 1), a[2]);
            i += 3;
            break;
        case PO_RING:
            for (k = 0; k < sides; k++) macro_ring(v, i8(a, 0) * (k ? -1 : 1), i8(a, 1), a[2], a[3]);
            i += 4;
            break;
        case PO_CHAIN:
            for (k = 0; k < sides; k++) {
                int s = k ? -1 : 1;
                macro_chain(v, i8(a, 0) * s, i8(a, 1), i8(a, 2) * s, i8(a, 3), a[4], a[5]);
            }
            i += 6;
            break;
        case PO_FRILL:
            macro_frill(v, i8(a, 0), i8(a, 1), i8(a, 2), a[3], a[4]);   /* spans x0..x1: never mirrored */
            i += 5;
            break;
        default:
            return n;                                   /* unknown op: stop this block */
        }
    }
    return n;
}

/* anchor: PA_HEAD / PA_TORSO / PA_FRAME runs only the blocks of that anchor; -1 every block but the
 * frame ones (they need their hook's frame: ps_slot_frame) */
int ps_run_group(PCtx *c, int list, int value, int anchor, int group)
{
    const PartRef *r;
    const PartSet *set = find_set(list, value, &r);
    const uint8_t *p;
    int ops = 0;
    if (!set) return 0;
    p = PART_OPS(set, r->ofs);
    while (p[0] != PH_END) {
        PartHdr h;
        h.group = p[0]; h.anchor = p[1]; h.lod_views = p[2]; h.len = p[3];
        if ((anchor < 0 ? h.anchor != PA_FRAME : h.anchor == anchor) && (group < 0 || h.group == group) &&
            ((PLV_LOD(h.lod_views) >> c->h.lod) & 1) && ((PLV_VIEW(h.lod_views) >> c->view_kind) & 1)) {
            VM v;
            v.c = c; v.set = set; v.anchor = h.anchor; v.scale = 64; v.mirror = 0; v.decal = 0;
            v.shade = SH_BALL; v.m = &c->skin;
            ops += run_block(&v, p + 4, h.len);
            pc_noclip();                                /* a block never leaves a clip behind */
        }
        p += 4 + h.len;
    }
    return ops;
}

int ps_run(PCtx *c, int list, int value, int anchor)
{
    return ps_run_group(c, list, value, anchor, -1);
}

/* The slot hook (CR2): 1 if the worn `value` of `list` is a scripted part, after drawing its blocks
 * of layer group `group` (-1: every block, for painters that run once per image). The painter then
 * draws nothing of its own for the slot. */
int ps_slot(PCtx *c, int list, int value, int group)
{
    const PartRef *r;
    if (list < 128 && value < doomo_part_minv[list]) return 0;
    if (!find_set(list, value, &r)) return 0;
    ps_run_group(c, list, value, -1, group);
    return 1;
}

/* CR3: the frame blocks of the worn value in group `group`, in the frame ps_frame() set; 1 if the value
 * is scripted. A hook that sets frames calls ps_slot() once (head / torso blocks) and this per frame. */
int ps_slot_frame(PCtx *c, int list, int value, int group)
{
    const PartRef *r;
    if (list < 128 && value < doomo_part_minv[list]) return 0;
    if (!find_set(list, value, &r)) return 0;
    ps_run_group(c, list, value, PA_FRAME, group);
    return 1;
}

#else  /* !DOOMO_PARTS_VM: the PS1 until CR10 frees the RAM (portrait_int.h): no script draws */
int ps_has(int list, int value) { (void)list; (void)value; return 0; }
int ps_top(int list, int value) { (void)list; (void)value; return 0; }
int ps_run_group(PCtx *c, int list, int value, int anchor, int group)
{
    (void)c; (void)list; (void)value; (void)anchor; (void)group;
    return 0;
}
int ps_run(PCtx *c, int list, int value, int anchor) { (void)c; (void)list; (void)value; (void)anchor; return 0; }
int ps_slot(PCtx *c, int list, int value, int group) { (void)c; (void)list; (void)value; (void)group; return 0; }
void ps_frame(int ox, int oy, int px, int py, int rx, int ry) { (void)ox; (void)oy; (void)px; (void)py; (void)rx; (void)ry; }
int ps_slot_frame(PCtx *c, int list, int value, int group) { (void)c; (void)list; (void)value; (void)group; return 0; }
#endif

/* ---- look_key (SPEC 4.5) ------------------------------------------------------------------
 * The cache key of a resident's look: the gene plus the run bytes that change it (the deep set,
 * content-addressed). 0 for an id with no record. P1.M2 adds the style runs. */
uint32_t look_key(int rid)
{
    const DoomoResident *r = town_res(rid);
    const uint8_t *run;
    uint32_t h = 2166136261u;
    int i, len = 0;
    if (!r || r->status == RES_EMPTY) r = roster_premade(rid);
    if (!r) return 0;
    h = (h ^ r->species) * 16777619u;
    for (i = 0; i < DOOMO_GENE_BYTES; i++) h = (h ^ r->gene[i]) * 16777619u;
    run = look_pool_find(rid, RUN_DEEP, &len);
    if (run) for (i = 0; i < len; i++) h = (h ^ run[i]) * 16777619u;
    return h ? h : 1;
}

/* ---- hooks (declared in portrait_int.h) -------------------------------------------------- */
void pf_material_init(PCtx *c) { (void)c; }
void motion_pose(PCtx *c, int dir, int frame) { (void)c; (void)dir; (void)frame; }
