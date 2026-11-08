/* panel.c - the native platform's retained panels (G7, platform.h
 * plat_panel_begin; owner: foundation).
 *
 * Off by default (nat_panel_cache = 0): every panel is drawn on every
 * frame, exactly like the weak default, so the runner's screenshots and
 * the goldens never depend on it. With nat_panel_cache = 1 (core/tests/
 * test_panel.c) it keeps panels the way the GBA does (gba/src/panel.c,
 * docs/GBA.md section 4b), with plain memory instead of the front page:
 *   - a panel's body is what plat_rect / straight plat_line fill while it
 *     is drawn; everything else it draws (text, images, checkers, other
 *     lines) must land on body drawn earlier in it, and inside its rect,
 *     or it is not kept (counted in nat_panel_stats.rule);
 *   - on a kept frame the core draws only the panel's shading (its drop
 *     shadows), and the body is put back over it in plat_panel_end();
 *   - a kept panel is put back only on the frame right after the one it
 *     was last shown on, if nothing drawn later on that frame touched its
 *     rect outside its hole;
 *   - the body's final pixels (at plat_panel_end, minus the hole) are what
 *     comes back.
 * So a key that misses something the pixels depend on, or a panel that
 * breaks the rules, shows up as a frame that differs from the same run
 * with the cache off (test_panel.c compares every frame).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"

int nat_panel_cache;
NatPanelStats nat_panel_stats;

typedef struct {
    uint8_t  live;                 /* has a capture                    */
    uint8_t  keep;                 /* followed the rules               */
    uint8_t  spoiled;              /* drawn over later on its frame    */
    uint8_t  solid;                /* plat_panel_solid: the whole rect comes back */
    uint32_t key;
    int      x, y, w, h;           /* as passed                        */
    int      hx0, hy0, hx1, hy1;   /* hole, screen coordinates         */
    uint32_t frame;                /* plat_ticks() it was last shown on */
    uint8_t *pix, *mask;           /* nat_w x nat_h, allocated once    */
} NatPanel;

static NatPanel pn[PANEL_COUNT];
static int cap = -1;               /* the panel being drawn, -1 none  */
static int put = -1;               /* the kept panel whose shading is drawn */
static int depth;                  /* nested 2D calls (plat_image's rects) */
static int nest;                   /* panels begun inside a panel (drawn as part of it, as gba/src/panel.c) */
static uint32_t shown_mask;        /* panels shown this frame          */
static uint32_t shown_frame = 0xFFFFFFFFu;

void nat_panel_reset(void)
{
    int i;
    for (i = 0; i < PANEL_COUNT; i++) {
        pn[i].live = 0;
        pn[i].keep = 0;
    }
    cap = -1;
    put = -1;
    depth = 0;
    nest = 0;
    shown_mask = 0;
    memset(&nat_panel_stats, 0, sizeof(nat_panel_stats));
}

static void shown_now(int id)
{
    if (shown_frame != plat_ticks()) { shown_frame = plat_ticks(); shown_mask = 0; }
    shown_mask |= 1u << id;
}

static void rule(const char *why)
{
    NatPanel *p = &pn[cap];
    if (p->keep) {
        nat_panel_stats.rule++;
        if (!nat_panel_stats.first_rule) nat_panel_stats.first_rule = why;
        nat_panel_stats.rule_panel = cap;
    }
    p->keep = 0;
}

/* every native 2D call, before it draws: kind NP_*, the screen box it
 * touches (exclusive, unclipped) */
int nat_panel_op(int kind, int x0, int y0, int x1, int y1)
{
    int i;
    nat_stage_note(x0, y0, x1, y1);              /* (SP7) the stage's overlay box */
    if (!nat_panel_cache) return 0;
    if (depth++ > 0) return 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > nat_w) x1 = nat_w;
    if (y1 > nat_h) y1 = nat_h;
    /* drawn over a panel shown earlier on this frame (by anything, another
     * panel's drawing included): that panel is not kept next frame */
    if (x0 < x1 && y0 < y1 && shown_frame == plat_ticks()) {
        for (i = 0; i < PANEL_COUNT; i++) {
            NatPanel *p = &pn[i];
            if (i == cap || i == put || !(shown_mask & (1u << i)) || p->spoiled) continue;
            if (x1 <= p->x || x0 >= p->x + p->w || y1 <= p->y || y0 >= p->y + p->h) continue;
            if (x0 >= p->hx0 && x1 <= p->hx1 && y0 >= p->hy0 && y1 <= p->hy1) continue;   /* its hole */
            p->spoiled = 1;
            nat_panel_stats.spoiled++;
        }
    }
    if (put >= 0 && kind != NP_SHADE) {
        /* a kept panel's frame draws its shading only */
        nat_panel_stats.rule++;
        if (!nat_panel_stats.first_rule) nat_panel_stats.first_rule = "a kept panel drew more than its shading";
        nat_panel_stats.rule_panel = put;
    }
    if (cap >= 0) {
        NatPanel *p = &pn[cap];
        if (kind == NP_CLEAR) { rule("plat_clear"); return 1; }
        if (x0 >= x1 || y0 >= y1) return 1;
        if (x0 < p->x || y0 < p->y || x1 > p->x + p->w || y1 > p->y + p->h) {
            static char why2[160];
            snprintf(why2, sizeof(why2), "drawn outside the panel's rect: kind %d at %d,%d..%d,%d, rect %d,%d %dx%d",
                     kind, x0, y0, x1, y1, p->x, p->y, p->w, p->h);
            rule(why2);
            return 1;
        }
        if (kind == NP_RECT) {
            int y;
            for (y = y0; y < y1; y++) memset(p->mask + y * nat_w + x0, 1, (size_t)(x1 - x0));
        } else if (kind != NP_SHADE && kind != NP_SHAPE) {
            int y, x;
            for (y = y0; y < y1; y++)
                for (x = x0; x < x1; x++)
                    if (!p->mask[y * nat_w + x]) {
                        static char why[160];
                        snprintf(why, sizeof(why), "text / image / checker off the body: kind %d at %d,%d..%d,%d (%d,%d)",
                                 kind, x0, y0, x1, y1, x, y);
                        rule(why);
                        return 1;
                    }
        }
    }
    return 1;
}

/* a cached shape's bitmap (plat_image): its opaque pixels are body */
void nat_panel_body_image(int x, int y, const DoomoImage *img, int scale, int flags)
{
    NatPanel *p;
    int ix, iy, a, b;
    if (!nat_panel_cache || cap < 0 || depth > 1) return;
    p = &pn[cap];
    for (iy = 0; iy < img->h; iy++)
        for (ix = 0; ix < img->w; ix++) {
            int sx = (flags & IMG_FLIPX) ? img->w - 1 - ix : ix;
            if (!img->pix[iy * img->w + sx]) continue;
            for (b = 0; b < scale; b++)
                for (a = 0; a < scale; a++) {
                    int qx = x + ix * scale + a, qy = y + iy * scale + b;
                    if ((unsigned)qx < (unsigned)nat_w && (unsigned)qy < (unsigned)nat_h)
                        p->mask[qy * nat_w + qx] = 1;
                }
        }
}

void nat_panel_op_end(void)
{
    if (depth > 0) depth--;
}

void nat_panel_bad(const char *why)
{
    if (nat_panel_cache && cap >= 0) rule(why);
}

int plat_panel_begin(int id, int x, int y, int w, int h, uint32_t key)
{
    NatPanel *p;
    int i, j;
    if (!nat_panel_cache) return 1;
    if (cap >= 0 || put >= 0) { nest++; return 1; }   /* no panels inside panels: its end must not close the outer one */
    if (id < 0 || id >= PANEL_COUNT) { nest++; return 1; }
    p = &pn[id];
    nat_panel_stats.begins++;
    if (!p->pix) {
        p->pix = (uint8_t *)malloc(NAT_MAX_W * NAT_MAX_H);
        p->mask = (uint8_t *)malloc(NAT_MAX_W * NAT_MAX_H);
        if (!p->pix || !p->mask) return 1;
    }
    if (p->live && p->keep && !p->spoiled && p->key == key && p->x == x && p->y == y &&
        p->w == w && p->h == h && p->frame + 1 == plat_ticks()) {
        /* kept: the core draws its shading, plat_panel_end puts it back */
        put = id;
        nat_panel_stats.kept++;
        return 0;
    }
    /* draw it, and watch what it draws */
    p->live = 1;
    p->keep = 1;
    p->spoiled = 0;
    p->key = key;
    p->x = x; p->y = y; p->w = w; p->h = h;
    p->hx0 = p->hy0 = p->hx1 = p->hy1 = 0;
    p->solid = 0;
    for (j = DOOMO_MAX(y, 0); j < DOOMO_MIN(y + h, nat_h); j++)
        for (i = DOOMO_MAX(x, 0); i < DOOMO_MIN(x + w, nat_w); i++) p->mask[j * nat_w + i] = 0;
    cap = id;
    nat_panel_stats.drawn++;
    return 1;
}

void plat_panel_hole(int x, int y, int w, int h)
{
    NatPanel *p;
    if (!nat_panel_cache || cap < 0) return;
    p = &pn[cap];
    if (x < p->x || y < p->y || x + w > p->x + p->w || y + h > p->y + p->h) rule("hole outside the rect");
    p->hx0 = x; p->hy0 = y; p->hx1 = x + w; p->hy1 = y + h;
}

void plat_panel_solid(void)
{
    if (!nat_panel_cache || cap < 0) return;
    pn[cap].solid = 1;
}

void plat_panel_end(void)
{
    NatPanel *p;
    int i, j;
    if (!nat_panel_cache) return;
    if (nest) { nest--; return; }
    if (put >= 0) {
        p = &pn[put];
        for (j = DOOMO_MAX(p->y, 0); j < DOOMO_MIN(p->y + p->h, nat_h); j++)
            for (i = DOOMO_MAX(p->x, 0); i < DOOMO_MIN(p->x + p->w, nat_w); i++)
                if (p->mask[j * nat_w + i]) nat_fb[j * nat_w + i] = p->pix[j * nat_w + i];
        p->frame = plat_ticks();
        shown_now(put);
        put = -1;
        return;
    }
    if (cap < 0) return;
    p = &pn[cap];
    for (j = DOOMO_MAX(p->y, 0); j < DOOMO_MIN(p->y + p->h, nat_h); j++)
        for (i = DOOMO_MAX(p->x, 0); i < DOOMO_MIN(p->x + p->w, nat_w); i++) {
            int k = j * nat_w + i;
            if (p->solid) p->mask[k] = 1;   /* CR9: the whole rect comes back */
            if (i >= p->hx0 && i < p->hx1 && j >= p->hy0 && j < p->hy1) p->mask[k] = 0;
            p->pix[k] = nat_fb[k];
        }
    p->frame = plat_ticks();
    shown_now(cap);
    cap = -1;
}
