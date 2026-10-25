/* show_crowd.c - crowds (WP6; SHOW_ENGINE_SPEC.md sec.8.5).
 *
 * A crowd is up to 256 chibi citizens drawn from K <= 48 unique looks
 * (LOOK slots look_base .. look_base + k - 1). Every citizen is computed from
 * the record and the shot frame alone, so a seek's pre-roll and a full run
 * agree and the crowd costs no per-citizen RAM beyond a sort order.
 *
 *   form lane     a line walking along x: the first citizen starts at (x, y),
 *                 the others trail it by `spacing`; y jitters by 0..h px.
 *   form grid     rows x columns over w x h (an audience), odd rows staggered.
 *   form scatter  a jittered grid over w x h (a fair crowd milling about).
 *
 * Ripples. Citizen i has a ripple r_i in 0..`ripple` frames from a seeded
 * hash of its id, and lives r_i frames behind the crowd: it starts walking,
 * stops at a freeze, starts again at a thaw, hops at a pop and turns at a
 * face op r_i frames late (TIME-RIPPLE: 0-3 f). Its clock is the crowd's own
 * (frames minus the frozen frames), or a beat.c actor clock the crowd follows
 * (clock= 0..7: `freeze -1` then stops the whole frame). A special member
 * with clock none (Mascotty) never moves at all.
 *
 * K-look reuse. Citizen i wears look (i * stride + seed) % k, so every image
 * (look, dir, walk frame) is shared by about n / k citizens and drawn many
 * times with the stock plat_image (mirrored at random for standing crowds).
 * K looks x 4 walk frames = at most 192 portrait-pool blocks of the show's
 * 240, and the VRAM image cache uploads each image once. The stats count
 * every fetch that renders an image again after it had been cached: an
 * eviction (AT6.4 wants 0 per frame after the first).
 */
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/beat.h"
#include "doomo/tween.h"
#include "show/show_portrait.h"
#include "show/show_actor.h"

#define MAX_SEG 6                    /* speed changes per crowd              */
#define MAX_CLK 8                    /* freeze / thaw events per crowd       */

typedef struct Crowd {
    ShowCrowdRec r;
    uint8_t  on, hidden, stride, nseg, nclk;
    int8_t   pop_tw;
    int16_t  pop_t0;
    int8_t   dir_from, dir_to;
    int16_t  dir_t0;
    int16_t  clk_t[MAX_CLK];         /* shot frame of each freeze / thaw     */
    uint8_t  clk_frz[MAX_CLK];       /* 1 freeze, 0 thaw                     */
    int32_t  seg_c[MAX_SEG];         /* crowd clock where a speed starts     */
    int32_t  seg_v[MAX_SEG];         /* speed Q16 px per clock frame         */
    uint16_t order[SHOW_CROWD_MAX_N];
} Crowd;

static Crowd s_crowd[SHOW_MAX_CROWDS];
static int s_ncrowd;
static int s_t;
static ShowCrowdStats s_stats;

/* re-render detection: (look, dir, frame, flip) -> last uid seen */
#define KEYS 1024
typedef struct KeySeen { uint32_t key, uid; int32_t frame; } KeySeen;
static KeySeen s_keys[KEYS];

static uint32_t hash32(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

static int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

void show_crowd_reset(void)
{
    memset(s_crowd, 0, sizeof(s_crowd));
    memset(s_keys, 0, sizeof(s_keys));
    memset(&s_stats, 0, sizeof(s_stats));
    s_ncrowd = 0;
    s_t = 0;
}

int show_crowd_load(const ShowCrowdRec *recs, int n)
{
    int i, j;
    if (!recs || n < 0) n = 0;
    if (n > SHOW_MAX_CROWDS) n = SHOW_MAX_CROWDS;
    for (i = 0; i < n; i++) {
        Crowd *c = &s_crowd[i];
        static const uint8_t strides[] = { 7, 11, 13, 5, 17, 3, 19, 1 };
        memset(c, 0, sizeof(*c));
        c->r = recs[i];
        if (c->r.n > SHOW_CROWD_MAX_N) c->r.n = SHOW_CROWD_MAX_N;
        if (c->r.k < 1) c->r.k = 1;
        if (c->r.k > SHOW_CROWD_MAX_K) c->r.k = SHOW_CROWD_MAX_K;
        if (c->r.ripple > 3) c->r.ripple = 3;
        if (c->r.step < 1) c->r.step = 4;
        if (c->r.px < 1) c->r.px = 1;
        if (c->r.px > 4) c->r.px = 4;
        if (c->r.clock >= BEAT_ACTORS) c->r.clock = 0xFF;
        if (c->r.nspecial > 4) c->r.nspecial = 4;
        c->on = 1;
        c->pop_tw = -1;
        c->dir_from = c->dir_to = (int8_t)(c->r.dir & 3);
        c->dir_t0 = -1000;
        c->stride = 1;
        for (j = 0; j < (int)sizeof(strides); j++)
            if (gcd(strides[(j + c->r.seed) % sizeof(strides)], c->r.k) == 1) {
                c->stride = strides[(j + c->r.seed) % sizeof(strides)];
                break;
            }
        c->nseg = 1;
        c->seg_c[0] = 0;
        c->seg_v[0] = c->r.speed_q16;
        for (j = 0; j < c->r.n; j++) c->order[j] = (uint16_t)j;
    }
    s_ncrowd = n;
    return n;
}

/* the crowd's own clock at shot frame tau: frames minus frozen frames */
static int32_t own_clock(const Crowd *c, int tau)
{
    int32_t clk = 0;
    int last = 0, frozen = 0, k;
    if (tau <= 0) return 0;
    for (k = 0; k < c->nclk && c->clk_t[k] < tau; k++) {
        if (!frozen) clk += c->clk_t[k] - last;
        last = c->clk_t[k];
        frozen = c->clk_frz[k];
    }
    if (!frozen) clk += tau - last;
    return clk;
}

static int crowd_clock_now(const Crowd *c)
{
    if (c->r.clock < BEAT_ACTORS) return beat_clock(c->r.clock);
    return (int)own_clock(c, s_t);
}

void show_crowd_op(int ci, int op, int a)
{
    Crowd *c;
    if (ci < 0 || ci >= s_ncrowd) return;
    c = &s_crowd[ci];
    switch (op) {
    case SHCO_FREEZE:
    case SHCO_THAW:
        if (c->nclk < MAX_CLK) {
            int fz = op == SHCO_FREEZE;
            int was = c->nclk ? c->clk_frz[c->nclk - 1] : 0;
            if (fz != was) {
                c->clk_t[c->nclk] = (int16_t)s_t;
                c->clk_frz[c->nclk] = (uint8_t)fz;
                c->nclk++;
            }
        }
        break;
    case SHCO_POP:
        c->pop_tw = (int8_t)(a >= 0 && a < TWT_COUNT ? a : -1);
        c->pop_t0 = (int16_t)s_t;
        break;
    case SHCO_FACE:
        c->dir_from = c->dir_to;
        c->dir_to = (int8_t)(a & 3);
        c->dir_t0 = (int16_t)s_t;
        break;
    case SHCO_SPEED:
        if (c->nseg < MAX_SEG) {
            c->seg_c[c->nseg] = crowd_clock_now(c);
            c->seg_v[c->nseg] = (int32_t)a * 256;          /* Q8 -> Q16 */
            c->nseg++;
        }
        break;
    case SHCO_SHOW:
        c->hidden = a ? 0 : 1;
        break;
    default: break;
    }
}

/* distance travelled (Q16) by clock value clk */
static int32_t travelled(const Crowd *c, int32_t clk)
{
    int32_t d = 0;
    int s;
    for (s = 0; s < c->nseg; s++) {
        int32_t from = c->seg_c[s], to = s + 1 < c->nseg ? c->seg_c[s + 1] : 0x7FFFFFFF;
        if (clk <= from) break;
        if (clk < to) to = clk;
        d += c->seg_v[s] * (to - from);
    }
    return d;
}

static int32_t speed_at(const Crowd *c, int32_t clk)
{
    int s, v = c->seg_v[0];
    for (s = 1; s < c->nseg; s++) if (clk >= c->seg_c[s]) v = c->seg_v[s];
    return v;
}

static int special_of(const Crowd *c, int i)
{
    int j;
    for (j = 0; j < c->r.nspecial; j++) if (c->r.special_idx[j] == i) return j;
    return -1;
}

/* member i at shot frame s_t (pure: record + frame + the ops so far) */
static void member(const Crowd *c, int i, ShowMember *m)
{
    const ShowCrowdRec *r = &c->r;
    uint32_t h = hash32((uint32_t)r->seed, (uint32_t)i);
    int sp = special_of(c, i);
    int still = (r->flags & SHCF_NOCLOCK) || (sp >= 0 && (r->special_flags[sp] & SHCF_NOCLOCK));
    int rip = r->ripple ? (int)(h % (uint32_t)(r->ripple + 1)) : 0;
    int32_t clk, v;
    int x, y, dir, frame = 0, flip = 0;
    memset(m, 0, sizeof(*m));
    if (still) clk = 0;
    else if (r->clock < BEAT_ACTORS) { clk = beat_clock(r->clock) - rip; if (clk < 0) clk = 0; }
    else clk = own_clock(c, s_t - rip);
    /* the base position */
    if (r->form == SHC_LANE) {
        int32_t sp0 = (int32_t)r->spacing_q4 * i;          /* Q4 */
        int sgn = r->speed_q16 < 0 ? -1 : 1;
        x = r->x - sgn * (int)(sp0 >> 4);
        y = r->y + (r->h > 0 ? (int)((h >> 8) % (uint32_t)(r->h + 1)) : 0);
        x += (int)(travelled(c, clk) >> 16);
        if ((r->flags & SHCF_WRAP) && r->w > 0) {     /* ambient walkers loop over [x, x + w) */
            int d = (x - r->x) % r->w;
            x = r->x + (d < 0 ? d + r->w : d);
        }
    } else if (r->form == SHC_GRID) {
        int pitch = r->spacing_q4 > 0 ? r->spacing_q4 : 16 * 18;
        int rowp = r->row_q4 > 0 ? r->row_q4 : 16 * 12;
        int cols = (r->w * 16) / pitch + 1, row, col;
        if (cols < 1) cols = 1;
        row = i / cols;
        col = i % cols;
        x = r->x + (col * pitch + ((row & 1) ? pitch / 2 : 0)) / 16 + (int)((h >> 4) % 5) - 2;
        y = r->y + (row * rowp) / 16 + (int)((h >> 12) % 3) - 1;
    } else {                                                /* scatter */
        int cols = 1, rows, cw, ch;
        while (cols * cols * (r->h > 0 ? r->h : 1) < r->n * (r->w > 0 ? r->w : 1)) cols++;
        rows = (r->n + cols - 1) / cols;
        if (rows < 1) rows = 1;
        cw = (r->w > 0 ? r->w : 1) / cols;
        ch = (r->h > 0 ? r->h : 1) / rows;
        x = r->x + (i % cols) * cw + (cw > 0 ? (int)((h >> 4) % (uint32_t)cw) : 0);
        y = r->y + (i / cols) * ch + (ch > 0 ? (int)((h >> 14) % (uint32_t)ch) : 0);
    }
    /* direction and walk frame */
    v = speed_at(c, clk);
    dir = (s_t - rip >= c->dir_t0) ? c->dir_to : c->dir_from;
    if (r->form == SHC_LANE && v != 0 && !still) dir = v < 0 ? 1 : 2;
    if (v != 0 && !still) frame = (int)(((clk + (int32_t)((h >> 20) & 15)) / r->step) & 3);
    else if ((r->flags & SHCF_FLIPMIX) && (dir == 0 || dir == 3)) flip = (int)((h >> 24) & 1);
    if (!still && v == 0 && (r->flags & SHCF_BOB))
        y -= (int)(((clk + (int32_t)((h >> 16) & 31)) >> 4) & 1);
    m->x = (int16_t)x;
    m->y = (int16_t)y;
    m->dir = (int8_t)dir;
    m->frame = (int8_t)frame;
    m->flip = (int8_t)flip;
    m->ripple = (int8_t)rip;
    m->clock = clk;
    m->look = (uint8_t)(sp >= 0 ? r->special_look[sp]
                                : r->look_base + (i * c->stride + r->seed) % r->k);
}

int show_crowd_member(int ci, int i, ShowMember *out)
{
    if (ci < 0 || ci >= s_ncrowd || i < 0 || i >= s_crowd[ci].r.n || !out) return 0;
    member(&s_crowd[ci], i, out);
    return 1;
}

void show_crowd_tick(int t)
{
    s_t = t;
}

static void note_fetch(uint32_t key, const DoomoImage *im)
{
    uint32_t h = hash32(key, 0x5EED) & (KEYS - 1);
    int probes;
    s_stats.fetched++;
    if (!im) return;
    for (probes = 0; probes < KEYS; probes++, h = (h + 1) & (KEYS - 1)) {
        KeySeen *k = &s_keys[h];
        if (k->key == key + 1) {
            if (k->uid != im->uid) { s_stats.rerenders++; k->uid = im->uid; }
            if (k->frame != s_t + 1) { k->frame = s_t + 1; s_stats.keys++; }
            return;
        }
        if (!k->key) {
            k->key = key + 1;
            k->uid = im->uid;
            k->frame = s_t + 1;
            s_stats.keys++;
            s_stats.newkeys++;
            return;
        }
    }
}

void show_crowd_draw(int ox, int oy, int layer, int draw)
{
    int ci;
    static ShowMember mem[SHOW_CROWD_MAX_N];
    if (layer == SHOW_LAYER_BACK) {
        s_stats.drawn = 0;
        s_stats.fetched = 0;
        s_stats.rerenders = 0;
        s_stats.keys = 0;
        s_stats.newkeys = 0;
    }
    portrait_cache_clock((uint32_t)s_t, 6);
    portrait_show_mouth = -1;                         /* crowds never flap */
    portrait_show_blink = -1;
    for (ci = 0; ci < s_ncrowd; ci++) {
        Crowd *c = &s_crowd[ci];
        int i, n = c->r.n, px = c->r.px;
        if (!c->on || ((c->r.flags & SHCF_FRONT) ? 1 : 0) != layer) continue;
        for (i = 0; i < n; i++) member(c, i, &mem[i]);
        /* back to front: insertion sort of last frame's order (nearly sorted) */
        for (i = 1; i < n; i++) {
            uint16_t v = c->order[i];
            int j = i - 1;
            while (j >= 0 && (mem[c->order[j]].y > mem[v].y ||
                              (mem[c->order[j]].y == mem[v].y && c->order[j] > v))) {
                c->order[j + 1] = c->order[j];
                j--;
            }
            c->order[j + 1] = v;
        }
        for (i = 0; i < n; i++) {
            const ShowMember *m = &mem[c->order[i]];
            const DoomoImage *im;
            int sx = 256, sy = 256, dy = 0, x, y;
            x = ox + m->x;
            y = oy + m->y;
            if (x < -BODYSPR_W * px || x > 320 + BODYSPR_W * px || y < 0 || y > 240 + BODYSPR_H * px)
                continue;                                  /* off screen: not fetched */
            im = bodyspr_image(m->look, m->dir, m->frame);
            note_fetch((uint32_t)m->look | ((uint32_t)m->dir << 8) | ((uint32_t)m->frame << 10) |
                       ((uint32_t)(m->flip ? 1 : 0) << 14), im);   /* VRAM keeps flips apart */
            if (!draw || c->hidden || !im) continue;
            if (c->pop_tw >= 0) {
                const TwTab *tt = tw_table(c->pop_tw);
                int age = s_t - c->pop_t0 - m->ripple;
                if (tt && age >= 0 && age < tt->n) {
                    int v = tw_tab(tt, age);
                    if (c->pop_tw <= TWT_SLAM_STICK2) sx = sy = v;
                    else if (c->pop_tw == TWT_CLAP || c->pop_tw == TWT_CLUNK) { sy = v; sx = 512 - v; }
                    else if (c->pop_tw != TWT_BRAKE_DX) dy = v;
                }
            }
            y += dy * px;
            if (sx == 256 && sy == 256)
                plat_image(x - (BODYSPR_W * px) / 2, y - BODYSPR_H * px, im, px,
                           m->flip ? IMG_FLIPX : 0, 0);
            else
                plat_image_xform(x, y, im, (m->flip ? -sx : sx) * px, sy * px, 0, 0);
            s_stats.drawn++;
        }
    }
}

const ShowCrowdStats *show_crowd_stats(void) { return &s_stats; }

uint32_t show_crowd_hash(uint32_t h)
{
    int ci, i;
    for (ci = 0; ci < s_ncrowd; ci++) {
        const Crowd *c = &s_crowd[ci];
        for (i = 0; i < c->r.n; i++) {
            ShowMember m;
            member(c, i, &m);
            h ^= (uint32_t)(uint16_t)m.x | ((uint32_t)(uint16_t)m.y << 16); h *= 16777619u;
            h ^= (uint32_t)m.clock; h *= 16777619u;
            h ^= (uint32_t)(m.dir | (m.frame << 4) | (m.flip << 8) | (m.look << 16)); h *= 16777619u;
        }
        h ^= c->hidden; h *= 16777619u;
    }
    return h;
}
