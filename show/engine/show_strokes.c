/* show_strokes.c - the STROKES plane (show_strokes.h). Owner: ENGINE lane 2. */
#include <string.h>
#include "doomo/doomo.h"
#include "show/show_l2.h"
#include "show/show_cam.h"
#include "show/show_fx.h"
#include "show/show_strokes.h"

typedef struct {
    const ShowStrokeSet *set;
    const ShowStroke    *st;
    const ShowStrokePt  *pt;
    uint8_t              hidden[64];       /* SOP show/hide overrides: 0 as set, 1 hide, 2 show */
} Plane;

static Plane s_pl[SHOW_STROKE_PLANES];
static int   s_npl;

void show_strokes_reset(void)
{
    memset(s_pl, 0, sizeof(s_pl));
    s_npl = 0;
}

int show_strokes_bind(const void *payload, uint32_t bytes)
{
    const ShowStrokeSet *set = (const ShowStrokeSet *)payload;
    Plane *p;
    uint32_t need;
    if (!payload || bytes < sizeof(ShowStrokeSet) || s_npl >= SHOW_STROKE_PLANES) return -1;
    need = sizeof(ShowStrokeSet) + set->nstrokes * sizeof(ShowStroke) + set->npts * sizeof(ShowStrokePt);
    if (need > bytes) return -1;
    p = &s_pl[s_npl];
    memset(p, 0, sizeof(*p));
    p->set = set;
    p->st = (const ShowStroke *)(set + 1);
    p->pt = (const ShowStrokePt *)(p->st + set->nstrokes);
    return s_npl++;
}

void show_strokes_show(int h, int s, int on)
{
    if (h < 0 || h >= s_npl || s < 0 || s >= 64) return;
    s_pl[h].hidden[s] = on ? 2 : 1;
}

/* ---- the rasteriser ---------------------------------------------------------
 * walk() visits the stroke's path pixels in order (Bresenham per segment, the
 * shared endpoint once) and calls plot() for each with its path index. */
typedef struct {
    const ShowXform *xf;
    const ShowStroke *s;
    uint32_t id;
    int limit;              /* draw path pixels [0, limit)                   */
    int n;                  /* path pixels seen                              */
    int draw;
} Walk;

static void put(const Walk *w, int u, int v, uint8_t c)
{
    int x0 = show_xf_x(w->xf, u), x1 = show_xf_x(w->xf, u + 1);
    int y0 = show_xf_y(w->xf, v), y1 = show_xf_y(w->xf, v + 1);
    if (x1 <= x0 || y1 <= y0) return;
    plat_rect(x0, y0 + SHOW_CANVAS_Y, x1 - x0, y1 - y0, c);
}

static void plot(Walk *w, int u, int v, int horiz)
{
    const ShowStroke *s = w->s;
    int i = w->n++, per = s->gap_on + s->gap_off;
    uint8_t c;
    if (!w->draw || i >= w->limit) return;
    if (s->gap_on && s->gap_off && (i % per) >= s->gap_on) return;      /* the tooth */
    c = s->color;
    if (s->color2 && (show_hash3(w->id, (uint32_t)i, 7u) & 7) == 0) c = s->color2;
    put(w, u, v, c);
    if (s->width >= 2) {
        if (horiz) put(w, u, v + 1, c); else put(w, u + 1, v, c);
    }
    if (s->width >= 3) {
        if (horiz) put(w, u, v - 1, c); else put(w, u - 1, v, c);
    }
}

static void segment(Walk *w, int x0, int y0, int x1, int y1, int first)
{
    int dx = x1 - x0, dy = y1 - y0, sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1, err, horiz;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    horiz = dx >= dy;
    err = dx - dy;
    if (first) plot(w, x0, y0, horiz);
    while (x0 != x1 || y0 != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
        plot(w, x0, y0, horiz);
    }
}

static void walk(Plane *p, int si, Walk *w, int every)
{
    const ShowStroke *s = &p->st[si];
    int i, have = 0, px = 0, py = 0, first = 1;
    for (i = 0; i < s->npts; i++) {
        const ShowStrokePt *q = &p->pt[s->first + i];
        int x, y, jx = 0, jy = 0;
        if (q->x == SK_PEN_UP) { have = 0; first = 1; continue; }
        x = q->x;
        y = q->y;
        if (every && !(s->flags & SK_NOBOIL)) {
            show_fx_boil(w->id, i, every, &jx, &jy);
            x += jx;
            y += jy;
        }
        if (have) {
            segment(w, px, py, x, y, first);
            first = 0;
        } else if (i + 1 >= s->npts || p->pt[s->first + i + 1].x == SK_PEN_UP) {
            plot(w, x, y, 1);                      /* a lone dot */
        }
        px = x;
        py = y;
        have = 1;
    }
}

static int visible(const Plane *p, int si)
{
    uint8_t o = si < 64 ? p->hidden[si] : 0;
    if (o == 1) return 0;
    if (o == 2) return 1;
    return !(p->st[si].flags & SK_HIDDEN) || p->st[si].t0 != SK_ALWAYS;
}

/* path pixels to draw at t, given the stroke's total n */
static int reveal_limit(const ShowStroke *s, int t, int n)
{
    int span;
    if (s->t0 == SK_ALWAYS) return n;
    if (t < s->t0) return 0;
    if (t >= s->t1) return n;
    span = s->t1 - s->t0 + 1;
    return (int)(((int32_t)n * (t - s->t0 + 1) + span - 1) / span);
}

static void stroke_pass(Plane *p, int si, int t, const ShowXform *xf, int draw, int *drawn, int *total)
{
    Walk w;
    int every = show_fx_boil_every(p->set->boil);
    memset(&w, 0, sizeof(w));
    w.xf = xf;
    w.s = &p->st[si];
    w.id = p->set->id * 131u + (uint32_t)si;
    w.limit = 0x7fffffff;
    walk(p, si, &w, every);                          /* count */
    *total = w.n;
    *drawn = reveal_limit(w.s, t, w.n);
    if (!draw || *drawn <= 0) return;
    w.n = 0;
    w.limit = *drawn;
    w.draw = 1;
    walk(p, si, &w, every);
}

void show_strokes_draw(int h, int t, const ShowXform *xf)
{
    Plane *p;
    int si, d, n;
    if (h < 0 || h >= s_npl || show_preroll()) return;
    p = &s_pl[h];
    for (si = 0; si < p->set->nstrokes; si++)
        if (visible(p, si)) stroke_pass(p, si, t, xf, 1, &d, &n);
}

int show_strokes_revealed(int h, int s, int t, int *total)
{
    ShowXform xf = { 0, 0, 0, 0, 65536 };
    int d = 0, n = 0;
    if (h < 0 || h >= s_npl || s < 0 || s >= s_pl[h].set->nstrokes) { if (total) *total = 0; return 0; }
    stroke_pass(&s_pl[h], s, t, &xf, 0, &d, &n);
    if (total) *total = n;
    return d;
}
