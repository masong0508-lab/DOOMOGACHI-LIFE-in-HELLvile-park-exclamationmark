/* portrait_script.c - the part-script VM, materials, regions and stamps (P1.M1; SPEC 4.4).
 * Owner: customisation P1.
 *
 * A script (include/doomo/parts.h, built by tools/parts_build.py) draws parts in normalised head or
 * torso space with the same primitives the painters use, so a part can be added or changed without
 * a code edit. Nothing here keeps state of its own except the 16-entry material memo in the PCtx;
 * every loop is bounded by the script length (<= 255 B a block). Nothing calls ps_run from the
 * painters yet: M2 wires the live lists in (core/data/look/live_p1.txt). */
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

const PMat *pm_of(PCtx *c, int lf, int tone)
{
    const PMat *b = base_mat(c, lf);
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
static const PartRef *find_ref(const PartSet *set, int list, int value)
{
    int lo = 0, hi = (int)set->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const PartRef *r = &set->ref[mid];
        int cmp = r->list != list ? (r->list < list ? -1 : 1) : (r->value != value ? (r->value < value ? -1 : 1) : 0);
        if (cmp == 0) return r;
        if (cmp < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static const PartSet *find_set(int list, int value, const PartRef **ref)
{
    const PartRef *r = find_ref(&doomo_parts_face, list, value);
    if (r) { *ref = r; return &doomo_parts_face; }
    r = find_ref(&doomo_parts_head, list, value);
    if (r) { *ref = r; return &doomo_parts_head; }
    return NULL;
}

int ps_has(int list, int value)
{
    const PartRef *r;
    return find_set(list, value, &r) != NULL;
}

typedef struct VM {
    PCtx *c;
    const PartSet *set;
    int anchor, scale, mirror, shade;
    const PMat *m;
    int decal;
} VM;

static int i8(const uint8_t *p, int k) { return (int8_t)p[k] * 2; }

/* normalised -> Q4 for the anchor; s = +1 / -1 mirror side */
static int vx(const VM *v, int n, int s)
{
    PCtx *c = v->c;
    n = n * v->scale / 64 * s;
    if (v->anchor == PA_TORSO) return c->bx + n * c->bw / 256;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = 0;
    return hx(n);                                       /* yaw handled by the head space */
}

static int vy(const VM *v, int n)
{
    PCtx *c = v->c;
    n = n * v->scale / 64;
    return v->anchor == PA_TORSO ? c->by + n * c->bh / 256 : c->h.cy + n * c->h.ry / 256;
}

static int vrx(const VM *v, int r)
{
    r = r * v->scale / 64;
    return v->anchor == PA_TORSO ? r * v->c->bw / 256 : r * v->c->h.rx / 256;
}

static int vry(const VM *v, int r)
{
    r = r * v->scale / 64;
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
        case PO_ANCHOR:  v->anchor = a[0] == PA_TORSO ? PA_TORSO : PA_HEAD; i += 1; break;
        case PO_MAT:     v->m = pm_of(v->c, a[0], a[1]); i += 2; break;
        case PO_SHADE:   v->shade = a[0] < SH_COUNT ? a[0] : SH_BALL; i += 1; break;
        case PO_SOLID:   v->decal = 0; pc_part(v->m, v->shade, a[0]); i += 1; break;
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
        default:
            return n;                                   /* unknown op: stop this block */
        }
    }
    return n;
}

int ps_run_group(PCtx *c, int list, int value, int anchor, int group)
{
    const PartRef *r;
    const PartSet *set = find_set(list, value, &r);
    const uint8_t *p;
    int ops = 0;
    if (!set) return 0;
    p = set->ops + r->ofs;
    while (p[0] != PH_END) {
        PartHdr h;
        h.group = p[0]; h.anchor = p[1]; h.lod_views = p[2]; h.len = p[3];
        if ((anchor < 0 || h.anchor == anchor) && (group < 0 || h.group == group) &&
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
