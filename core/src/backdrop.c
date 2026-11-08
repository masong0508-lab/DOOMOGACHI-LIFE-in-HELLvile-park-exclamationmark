/* backdrop.c - the backdrop families, the weak plat_stage_* defaults and the retained stage
 * (owner: SP7; backdrop.h, plat_ext.h; docs/EXORCISM_SPEED.md).
 *
 * The stage keeps, for the last two frames a scene drew with it, the box and key of every
 * layer. A frame drawn into a buffer that holds one of them (plat_stage_begin's info.held)
 * starts from that record: the layers whose key or box changed, the boxes 2D calls covered
 * after that frame's stage_end (info.dirt: dialogue, toasts, hints) and, closed over, every
 * layer that lies on any of those boxes, are the dirty set. The backdrop is put back on the
 * union of their old and new boxes and exactly those layers are drawn, in order. Every other
 * pixel of the buffer already shows what drawing everything would show: same backdrop, same
 * layers in the same places, nothing drawn over them since. A buffer that holds nothing the
 * stage knows, or another backdrop key, is drawn in full (the whole backdrop put back).
 *
 * RAM: two records of STAGE_MAX_LAYERS layers (12 B each), the frame being built and the dirty
 * boxes, ~1.1 KB, in memory the caller binds (stage_bind: the exorcism's scene scratch, a cache
 * it re-binds when another scene took the scratch); 16 B of .bss here.
 */
#include "doomo/backdrop.h"
#include "doomo/plat_ext.h"
#include "doomo/platform.h"
#include "doomo/fixmath.h"
#include <string.h>

#define WEAK __attribute__((weak))

/* ---- the families --------------------------------------------------------------------- */
extern const DoomoBackdrop exo_church_family;            /* exorcism_gfx.c */

int backdrop_family_count(void) { return 1; }

const DoomoBackdrop *backdrop_family(int i)
{
    return i == 0 ? &exo_church_family : NULL;
}

/* ---- weak platform defaults: no kept backdrop ------------------------------------------- */
WEAK int plat_stage_begin(int id, uint32_t key, void (*draw)(uint32_t key), PlatStageInfo *info)
{
    (void)id; (void)key; (void)draw; (void)info;
    return 0;
}
WEAK void plat_stage_restore(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
WEAK void plat_stage_end(void) {}

/* ---- the stage ------------------------------------------------------------------------ */
typedef struct { int16_t x0, y0, x1, y1; uint32_t key; } SLayer;     /* box: [x0, x1) x [y0, y1) */
typedef struct {
    uint32_t serial, bkey;
    int16_t  bid;
    uint8_t  n, pad;
    SLayer   l[STAGE_MAX_LAYERS];
} SRec;

#define DIRTY_MAX 40
typedef struct {
    SRec rec[2];                  /* the last two frames drawn with the stage   */
    SRec cur;                     /* this frame                                 */
    PlatStageInfo info;
    int16_t d[DIRTY_MAX][4];      /* boxes put back                             */
    int nd;
} SMem;
/* the bound memory (the caller's: the exorcism's scene scratch) and the per-frame state */
static SMem *M;
static struct {
    int8_t on;                    /* plat_stage_begin said yes                  */
    uint32_t draw_mask;           /* layers to draw this frame                  */
    StageStats st;
} S;
#define SS (*M)

DOOMO_STATIC_ASSERT(STAGE_MAX_LAYERS <= 32, stage_mask_fits);

uint32_t stage_mem_bytes(void) { return (uint32_t)sizeof(SMem); }

void stage_bind(void *mem)
{
    M = (SMem *)mem;
    if (M) memset(M, 0, sizeof(*M));
    memset(&S, 0, sizeof(S));
}

void stage_reset(void)
{
    if (M) memset(M, 0, sizeof(*M));
    memset(&S, 0, sizeof(S));
}

int stage_begin(int id, uint32_t key, void (*draw)(uint32_t key))
{
    S.on = 0;
    S.draw_mask = 0xFFFFFFFFu;
    S.st.drawn = S.st.restored = 0;
    S.st.full = 1;
    S.st.retained = 0;
    if (!M) return 0;
    SS.cur.n = 0;
    SS.cur.bid = (int16_t)id;
    SS.cur.bkey = key;
    SS.cur.serial = 0;
    memset(&SS.info, 0, sizeof(SS.info));
    if (plat_draw_skipped()) return 0;
    if (!plat_stage_begin(id, key, draw, &SS.info)) return 0;
    S.on = 1;
    SS.cur.serial = SS.info.serial;
    S.st.retained = 1;
    return 1;
}

void stage_layer(int i, int x, int y, int w, int h, uint32_t key)
{
    SLayer *l;
    int W = plat_screen_w(), H = plat_screen_h();
    if (!S.on || i < 0 || i >= STAGE_MAX_LAYERS) return;
    while (SS.cur.n <= i) {                     /* layers skipped in between draw nothing */
        l = &SS.cur.l[SS.cur.n++];
        l->x0 = l->y0 = l->x1 = l->y1 = 0;
        l->key = 0;
    }
    l = &SS.cur.l[i];
    l->key = key;
    if (w <= 0 || h <= 0) { l->x0 = l->y0 = l->x1 = l->y1 = 0; return; }
    l->x0 = (int16_t)DOOMO_CLAMP(x, 0, W);
    l->y0 = (int16_t)DOOMO_CLAMP(y, 0, H);
    l->x1 = (int16_t)DOOMO_CLAMP(x + w, 0, W);
    l->y1 = (int16_t)DOOMO_CLAMP(y + h, 0, H);
    if (l->x0 >= l->x1 || l->y0 >= l->y1) l->x0 = l->y0 = l->x1 = l->y1 = 0;
}

static int empty(const SLayer *l) { return l->x0 >= l->x1 || l->y0 >= l->y1; }

static int hits(int x0, int y0, int x1, int y1, const int16_t b[4])
{
    return x0 < b[2] && x1 > b[0] && y0 < b[3] && y1 > b[1];
}

static int hits_dirty(int x0, int y0, int x1, int y1)
{
    int i;
    for (i = 0; i < SS.nd; i++)
        if (hits(x0, y0, x1, y1, SS.d[i])) return 1;
    return 0;
}

/* add a box to the dirty set (a box inside one already there adds nothing; one that overlaps
 * a box so much that their bounding box is barely bigger joins it: a moving layer's old and
 * new boxes; a full set becomes the whole screen) */
static void add_dirty(int x0, int y0, int x1, int y1)
{
    int i;
    if (x0 >= x1 || y0 >= y1) return;
    for (i = 0; i < SS.nd; i++) {
        int16_t *b = SS.d[i];
        int ux0, uy0, ux1, uy1;
        if (b[0] <= x0 && b[1] <= y0 && b[2] >= x1 && b[3] >= y1) return;
        if (x0 > b[2] || x1 < b[0] || y0 > b[3] || y1 < b[1]) continue;   /* apart (touching joins) */
        ux0 = DOOMO_MIN(x0, b[0]); uy0 = DOOMO_MIN(y0, b[1]);
        ux1 = DOOMO_MAX(x1, b[2]); uy1 = DOOMO_MAX(y1, b[3]);
        if ((int32_t)(ux1 - ux0) * (uy1 - uy0) * 4 <=
            ((int32_t)(x1 - x0) * (y1 - y0) + (int32_t)(b[2] - b[0]) * (b[3] - b[1])) * 5) {
            /* (the union is at most a quarter more than the two) */
            b[0] = (int16_t)ux0; b[1] = (int16_t)uy0; b[2] = (int16_t)ux1; b[3] = (int16_t)uy1;
            return;
        }
    }
    if (SS.nd >= DIRTY_MAX) {
        SS.nd = 1;
        SS.d[0][0] = 0; SS.d[0][1] = 0;
        SS.d[0][2] = (int16_t)plat_screen_w(); SS.d[0][3] = (int16_t)plat_screen_h();
        return;
    }
    SS.d[SS.nd][0] = (int16_t)x0; SS.d[SS.nd][1] = (int16_t)y0;
    SS.d[SS.nd][2] = (int16_t)x1; SS.d[SS.nd][3] = (int16_t)y1;
    SS.nd++;
}

/* Put the backdrop back on the union of the dirty boxes, every pixel once: the rows are cut
 * into bands at every box edge, and in each band the spans of the boxes that cover it are
 * merged. Returns the number of restores. */
static int restore_dirty(int dry, int32_t *area)
{
    int16_t ys[2 * DIRTY_MAX], sp[DIRTY_MAX][2];
    int ny = 0, i, j, k, n, calls = 0;
    *area = 0;
    for (i = 0; i < SS.nd; i++) {
        ys[ny++] = SS.d[i][1];
        ys[ny++] = SS.d[i][3];
    }
    for (i = 1; i < ny; i++)                       /* sort the edges */
        for (j = i; j > 0 && ys[j - 1] > ys[j]; j--) { int16_t t = ys[j]; ys[j] = ys[j - 1]; ys[j - 1] = t; }
    for (k = 0; k + 1 < ny; k++) {
        int y0 = ys[k], y1 = ys[k + 1];
        if (y0 >= y1) continue;
        for (i = 0, n = 0; i < SS.nd; i++) {        /* boxes covering the band, by x0 */
            if (SS.d[i][1] > y0 || SS.d[i][3] < y1) continue;
            for (j = n; j > 0 && sp[j - 1][0] > SS.d[i][0]; j--) { sp[j][0] = sp[j - 1][0]; sp[j][1] = sp[j - 1][1]; }
            sp[j][0] = SS.d[i][0];
            sp[j][1] = SS.d[i][2];
            n++;
        }
        for (i = 0; i < n;) {                        /* merged spans */
            int x0 = sp[i][0], x1 = sp[i][1];
            for (i++; i < n && sp[i][0] <= x1; i++) x1 = DOOMO_MAX(x1, sp[i][1]);
            if (!dry) plat_stage_restore(x0, y0, x1 - x0, y1 - y0);
            *area += (int32_t)(x1 - x0) * (y1 - y0);
            calls++;
        }
    }
    return calls;
}

static const SRec *held_record(void)
{
    int i;
    if (!SS.info.held) return NULL;
    for (i = 0; i < 2; i++)
        if (SS.rec[i].serial == SS.info.held && SS.rec[i].bid == SS.cur.bid) return &SS.rec[i];
    return NULL;
}

void stage_resolve(void)
{
    const SRec *old;
    int i, n, changed;
    int W = plat_screen_w(), H = plat_screen_h();
    if (!S.on) return;
    SS.nd = 0;
    old = held_record();
    if (!old || old->bkey != SS.cur.bkey) {
        /* nothing known about this buffer, or another backdrop: all of it */
        plat_stage_restore(0, 0, W, H);
        S.draw_mask = 0xFFFFFFFFu;
        S.st.full = 1;
        S.st.restored = 1;
        S.st.drawn = SS.cur.n;
        return;
    }
    S.st.full = 0;
    n = DOOMO_MAX(SS.cur.n, old->n);
    S.draw_mask = 0;
    add_dirty(SS.info.dx0, SS.info.dy0, SS.info.dx1, SS.info.dy1);
    for (i = 0; i < n; i++) {
        static const SLayer none = { 0, 0, 0, 0, 0 };
        const SLayer *a = i < old->n ? &old->l[i] : &none, *b = i < SS.cur.n ? &SS.cur.l[i] : &none;
        if (a->key == b->key && a->x0 == b->x0 && a->y0 == b->y0 && a->x1 == b->x1 && a->y1 == b->y1)
            continue;
        if (i < SS.cur.n) S.draw_mask |= 1u << i;
        if (!empty(a)) add_dirty(a->x0, a->y0, a->x1, a->y1);
        if (!empty(b)) add_dirty(b->x0, b->y0, b->x1, b->y1);
    }
    /* close over: a layer on a dirty box is drawn again, over a clean backdrop under its
     * whole box (its shading must not land twice) */
    do {
        changed = 0;
        for (i = 0; i < SS.cur.n; i++) {
            const SLayer *b = &SS.cur.l[i];
            if ((S.draw_mask >> i) & 1u || empty(b)) continue;
            if (!hits_dirty(b->x0, b->y0, b->x1, b->y1)) continue;
            S.draw_mask |= 1u << i;
            add_dirty(b->x0, b->y0, b->x1, b->y1);
            changed = 1;
        }
    } while (changed);
    {
        /* when most of the screen is dirty anyway, one full restore (whole rows: the fastest
         * copy) and every layer beat many partial ones */
        int32_t area;
        restore_dirty(1, &area);
        if (area * 20 >= (int32_t)W * H * 17) {
            plat_stage_restore(0, 0, W, H);
            S.draw_mask = 0xFFFFFFFFu;
            S.st.full = 1;
            S.st.restored = 1;
            S.st.drawn = SS.cur.n;
            return;
        }
        S.st.restored = (uint8_t)DOOMO_MIN(restore_dirty(0, &area), 255);
    }
    for (i = 0; i < SS.cur.n; i++) S.st.drawn = (uint8_t)(S.st.drawn + ((S.draw_mask >> i) & 1u));
}

int stage_draw(int i)
{
    if (!S.on || i < 0 || i >= STAGE_MAX_LAYERS) return 1;
    return (int)((S.draw_mask >> i) & 1u);
}

void stage_end(void)
{
    int slot;
    if (!S.on) return;
    plat_stage_end();
    /* keep this frame's record in place of the older of the two */
    slot = SS.rec[0].serial <= SS.rec[1].serial ? 0 : 1;
    SS.rec[slot] = SS.cur;
    S.on = 0;
}

void stage_stats(StageStats *s)
{
    if (s) *s = S.st;
}
