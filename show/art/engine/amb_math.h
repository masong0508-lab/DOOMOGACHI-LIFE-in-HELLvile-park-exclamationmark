/* amb_math.h - the ambient layers' per-frame math for the PS1 player
 * (owner: ART; a header-only twin of show/tools/showtk/art/motion/ambient.py).
 *
 * Every function is a pure integer function of (parameters, shot frame t,
 * seed): a shot stays pure (spec 8.2) and a seek draws the same pixels.
 * show/tests/art/test_motion.py compiles this header on the host and checks
 * it against the Python reference value for value. The record layout that
 * carries the parameters (SHWR_AMB, proposed) is in SETS.md sec.6.4; a player
 * module includes this header and does only the drawing.
 *
 * Signed right shifts are arithmetic (gcc on MIPS and x86 both).
 */
#ifndef SHOW_ART_AMB_MATH_H
#define SHOW_ART_AMB_MATH_H

#include <stdint.h>

static const int16_t amb_sin_tab[256] = {
    0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104, 109, 115, 121, 126,
    132, 137, 142, 147, 152, 157, 162, 167, 172, 177, 181, 185, 190, 194, 198, 202, 206, 209,
    213, 216, 220, 223, 226, 229, 231, 234, 237, 239, 241, 243, 245, 247, 248, 250, 251, 252,
    253, 254, 255, 255, 256, 256, 256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247,
    245, 243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209, 206, 202, 198, 194,
    190, 185, 181, 177, 172, 167, 162, 157, 152, 147, 142, 137, 132, 126, 121, 115, 109, 104,
    98, 92, 86, 80, 74, 68, 62, 56, 50, 44, 38, 31, 25, 19, 13, 6, 0, -6, -13, -19, -25, -31,
    -38, -44, -50, -56, -62, -68, -74, -80, -86, -92, -98, -104, -109, -115, -121, -126, -132,
    -137, -142, -147, -152, -157, -162, -167, -172, -177, -181, -185, -190, -194, -198, -202,
    -206, -209, -213, -216, -220, -223, -226, -229, -231, -234, -237, -239, -241, -243, -245,
    -247, -248, -250, -251, -252, -253, -254, -255, -255, -256, -256, -256, -256, -256, -255,
    -255, -254, -253, -252, -251, -250, -248, -247, -245, -243, -241, -239, -237, -234, -231,
    -229, -226, -223, -220, -216, -213, -209, -206, -202, -198, -194, -190, -185, -181, -177,
    -172, -167, -162, -157, -152, -147, -142, -137, -132, -126, -121, -115, -109, -104, -98,
    -92, -86, -80, -74, -68, -62, -56, -50, -44, -38, -31, -25, -19, -13, -6
};

static inline int amb_sin8(uint32_t i) { return amb_sin_tab[i & 255]; }
static inline int amb_cos8(uint32_t i) { return amb_sin_tab[(i + 64) & 255]; }

/* show_l2.h's show_hash3, repeated so this header stands alone */
static inline uint32_t amb_hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

/* drift: a plane's own offset (v_q8 = px/f in Q8) */
static inline int32_t amb_drift(int32_t v_q8, int32_t t) { return (v_q8 * t) >> 8; }

/* wave: row y's x offset (strips share their first row's) */
static inline int amb_wave_dx(int amp, int period, int len, int y, int t)
{
    uint32_t i = (uint32_t)(t * 256 / (period > 0 ? period : 1)) + (uint32_t)(y * 256 / (len > 0 ? len : 1));
    return (amp * amb_sin8(i) + 128) >> 8;
}

/* sway: prop k of the list, `lag` frames behind prop k-1 */
static inline int amb_sway_dx(int amp, int period, int t, int k, int lag)
{
    int p = period > 0 ? period : 1;
    int i = (t * 256) / p - (k * lag * 256) / p;
    return (amp * amb_sin8((uint32_t)i) + 128) >> 8;
}

/* flip: the drawing shown at t (holds[] frames each; speed Q8; loops) */
static inline int amb_flip_frame(const uint8_t *holds, int n, int t, int speed_q8, int phase)
{
    int total = 0, k;
    int32_t tt;
    for (k = 0; k < n; k++) total += holds[k];
    if (total <= 0) return 0;
    tt = (((int32_t)(t + phase) * speed_q8) >> 8) % total;
    for (k = 0; k < n; k++) { if (tt < holds[k]) return k; tt -= holds[k]; }
    return 0;
}

/* lamps: 0 normal, 1 dim, 2 bloom for lamp i at t */
static inline int amb_lamp_state(uint32_t seed, int i, int t, int hold_lo, int hold_hi, int dim, int bloom)
{
    uint32_t span = (uint32_t)(hold_hi - hold_lo + 1);
    uint32_t h0 = amb_hash3(seed, (uint32_t)i, 0xA11);
    uint32_t hl = (uint32_t)hold_lo + h0 % span;
    uint32_t h = amb_hash3(seed, (uint32_t)i, ((uint32_t)t + h0 % 97) / hl) % 100;
    if ((int)h < dim) return 1;
    if ((int)h >= 100 - bloom) return 2;
    return 0;
}

static inline int amb_pcycle_step(int t, int every, int n) { return (t / (every > 0 ? every : 1)) % n; }

/* motes: returns 0 when the mote is not drawn this frame */
enum { AMB_FIREFLY, AMB_SPARKLE, AMB_EMBER, AMB_DUST, AMB_PETAL, AMB_SNOW, AMB_BUBBLE, AMB_LEAF };
enum { AMB_ROLE_CORE, AMB_ROLE_HI, AMB_ROLE_DIM };
typedef struct AmbMote { int16_t x, y; uint8_t size, role; } AmbMote;

static inline int amb_mote(int typ, uint32_t seed, int i, int t, int x0, int y0, int w, int h,
                           int speed_q8, AmbMote *m)
{
    uint32_t h1 = amb_hash3(seed, (uint32_t)i, 1), h2 = amb_hash3(seed, (uint32_t)i, 2),
             h3 = amb_hash3(seed, (uint32_t)i, 3);
    int32_t ts = (t * speed_q8) >> 8;
    if (typ == AMB_FIREFLY) {
        int bx = x0 + (int)(h1 % (uint32_t)w), by = y0 + (int)(h2 % (uint32_t)h);
        int rx = 6 + (int)(h3 % 10), ry = 3 + (int)((h3 >> 8) % 6);
        int fx = 1 + (int)((h1 >> 12) % 3), fy = 2 + (int)((h2 >> 12) % 3);
        int cyc = 70 + (int)(h3 % 60), ph;
        m->x = (int16_t)(bx + ((rx * amb_sin8((uint32_t)(ts * fx / 2) + (h3 >> 4))) >> 8));
        m->y = (int16_t)(by + ((ry * amb_cos8((uint32_t)(ts * fy / 3) + (h2 >> 4))) >> 8));
        ph = (int)(((uint32_t)ts + h1) % (uint32_t)cyc);
        if (ph < 10) return 0;
        if (ph < 16 || ph > cyc - 8) { m->size = 0; m->role = AMB_ROLE_DIM; return 1; }
        m->size = (ph % 20 < 10) ? 1 : 0; m->role = AMB_ROLE_CORE; return 1;
    }
    if (typ == AMB_SPARKLE) {
        static const uint8_t sz[10] = { 0, 1, 3, 3, 1, 1, 0, 0, 0, 0 };
        int cyc = 40 + (int)(h3 % 50);
        int ph = (int)(((uint32_t)ts + h3) % (uint32_t)cyc);
        if (ph > 9) return 0;
        m->x = (int16_t)(x0 + (int)(h1 % (uint32_t)w)); m->y = (int16_t)(y0 + (int)(h2 % (uint32_t)h));
        m->size = sz[ph]; m->role = ph < 6 ? AMB_ROLE_HI : AMB_ROLE_DIM; return 1;
    }
    if (typ == AMB_EMBER || typ == AMB_BUBBLE) {
        int vy = 64 + (int)(h3 % 96);
        int life = h * 256 / vy + 1;
        int ph = (int)(((uint32_t)ts + h1) % (uint32_t)life);
        m->y = (int16_t)(y0 + h - ((ph * vy) >> 8));
        m->x = (int16_t)(x0 + (int)(h2 % (uint32_t)w) + ((3 * amb_sin8((uint32_t)(ph * 4) + h3)) >> 8));
        if (typ == AMB_EMBER) {
            m->size = 0;
            m->role = ph < life / 2 ? AMB_ROLE_CORE : (ph < life * 3 / 4 ? AMB_ROLE_HI : AMB_ROLE_DIM);
        } else { m->size = (h3 & 3) == 0 ? 1 : 0; m->role = AMB_ROLE_HI; }
        return 1;
    }
    {
        static const uint8_t bvx[8] = { 0, 0, 0, 40, 70, 20, 0, 90 }, bvy[8] = { 0, 0, 0, 10, 60, 70, 0, 80 };
        int vx = bvx[typ] + (int)(h3 % 48), vy = bvy[typ] + (int)((h3 >> 6) % 40);
        int x = x0 + (int)((((h1 % (uint32_t)(w * 256)) + (uint32_t)(ts * vx)) >> 8) % (uint32_t)w);
        int y = y0 + (int)((((h2 % (uint32_t)(h * 256)) + (uint32_t)(ts * vy)) >> 8) % (uint32_t)h);
        x += (2 * amb_sin8((uint32_t)(ts * 3) + h3)) >> 8;
        m->x = (int16_t)x; m->y = (int16_t)y;
        m->size = ((typ == AMB_PETAL || typ == AMB_LEAF) && (h1 & 1)) ? 2 : 0;
        m->role = (h2 & 3) ? AMB_ROLE_CORE : AMB_ROLE_HI;
        return 1;
    }
}

/* crowd walkers: member i of a lane wrapping over [x0-20, x1+20] (odd members walk left) */
typedef struct AmbWalker { int16_t x, y; uint8_t dir, frame; } AmbWalker;
static inline void amb_walker(int i, int t, uint32_t seed, int x0, int x1, int y, int speed_q8,
                              int step, int jitter, int n, AmbWalker *w)
{
    uint32_t h1 = amb_hash3(seed, (uint32_t)i, 7), h2 = amb_hash3(seed, (uint32_t)i, 8);
    int span = (x1 - x0) + 40;
    int v = speed_q8 + (int)(h2 % 64) - 32;
    int left = (i & 1) == 1;
    uint32_t slot = (uint32_t)(span * 256) / (uint32_t)(n > 0 ? n : 1);
    int pos = (int)(((uint32_t)i * slot + h1 % slot + (uint32_t)(t * v)) >> 8);
    pos %= span;
    w->x = (int16_t)(left ? (x1 + 20 - pos) : (x0 - 20 + pos));
    w->y = (int16_t)(y + (int)((h2 >> 8) % (uint32_t)(jitter + 1)) - jitter / 2);
    w->dir = left ? 1 : 2;
    w->frame = (uint8_t)((((uint32_t)t + h1 % 16) / (uint32_t)(step > 0 ? step : 1)) % 4);
}

#endif
