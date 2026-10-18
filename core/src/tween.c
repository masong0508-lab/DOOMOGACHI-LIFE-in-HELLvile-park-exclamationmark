/* tween.c - comic-timing curves and springs (tween.h; owner: S1,
 * docs/design/story/SPEC.md sec.7 S1, comic_timing.md sec.3.1-3.3, 7.1).
 *
 * Resident on the PS1 (the name matches no overlay group, SPEC R4): the
 * dialogue, the cutscene stages, the overworld and the house all use it.
 * Integer only: fix16 time, Q8.8 scales, Q4 spring positions, Q14 spring
 * coefficients. No 64-bit products: every multiply below fits 32 bits.
 * The keyframe tables live in tween_tables.c.
 */
#include "doomo/doomo.h"
#include "doomo/tween.h"

fix16 tw_t(int age, int dur)
{
    if (dur <= 0 || age >= dur) return FIX_ONE;
    if (age <= 0) return 0;
    if (dur < 32768) return (fix16)(((uint32_t)age << 16) / (uint32_t)dur);
    return (fix16)(((uint32_t)age << 8) / ((uint32_t)dur >> 8));
}

/* t in 0..1 reduced to Q12 so products of two stay inside 32 bits */
static int32_t q12(fix16 t)
{
    if (t <= 0) return 0;
    if (t >= FIX_ONE) return 4096;
    return t >> 4;
}

fix16 tw_smooth(fix16 t)                      /* t*t*(3-2t)  (tk_ease) */
{
    int32_t u = q12(t), uu = (u * u) >> 12;
    return (fix16)((uu * (3 * 4096 - 2 * u)) >> 8);
}

fix16 tw_in2(fix16 t)
{
    int32_t u = q12(t);
    return (fix16)((u * u) >> 8);
}

fix16 tw_out2(fix16 t)
{
    int32_t u = 4096 - q12(t);
    return FIX_ONE - (fix16)((u * u) >> 8);
}

/* ease-out-back, UI slides only (CT sec.3.1 rule): 1 + (s+1)(t-1)^3 + s(t-1)^2,
 * s in fix16 (0x13333 = 1.2 -> about 5% overshoot). */
fix16 tw_back(fix16 t, fix16 s)
{
    int32_t u = q12(t) - 4096;                /* -4096..0 (Q12)      */
    int32_t u2 = (u * u) >> 12;               /* Q12                 */
    int32_t u3 = (u2 * u) >> 12;              /* Q12, <= 0           */
    int32_t s12 = s >> 4;                     /* Q12                 */
    int32_t v = 4096 + (((s12 + 4096) * u3) >> 12) + ((s12 * u2) >> 12);
    return (fix16)(v << 4);
}

/* tk_bounce sampled at t = i/32 in fix16 (CT Appendix B), with the tail
 * clamped to 1.0 from i = 30 (CT sec.10: the trailer's 1.4% end snap would
 * show on big PS1 sprites; every caller snapped to 1.0 anyway). */
static const int32_t TW_BOUNCE_FX[33] = {
    0, 9069, 18637, 28148, 37179, 45425, 52689, 58863, 63916, 67874, 70809, 72821, 74033,
    74573, 74572, 74155, 73439, 72524, 71500, 70439, 69398, 68422, 67539, 66768, 66121,
    65596, 65191, 64897, 64700, 64588, 65536, 65536, 65536 };

fix16 tw_bounce(fix16 t)
{
    int i, fr;
    if (t <= 0) return 0;
    if (t >= FIX_ONE) return FIX_ONE;
    i = t >> 11;                              /* 32 segments */
    fr = t & 0x7FF;
    return TW_BOUNCE_FX[i] + (((TW_BOUNCE_FX[i + 1] - TW_BOUNCE_FX[i]) * fr) >> 11);
}

int tw_lerp(int a, int b, fix16 t)
{
    int32_t d = (int32_t)b - (int32_t)a;
    if (t <= 0) return a;
    if (t >= FIX_ONE) return b;
    /* split t so |d| up to 2^20 stays inside 32 bits */
    return a + (int)((d * (t >> 8)) >> 8) + (int)((d * (t & 0xFF)) >> 16);
}

int tw_tab(const TwTab *t, int age)
{
    if (!t || !t->v || !t->n) return 0;
    if (age < 0) return t->v[0];
    if (age >= t->n) return t->v[t->n - 1];
    return t->v[age];
}

TwSquash tw_squash(const TwTab *sx, const TwTab *sy, const TwTab *dy, int age, int amount256)
{
    TwSquash q;
    int x = sx && sx->n ? tw_tab(sx, age) : 256;
    int y = sy && sy->n ? tw_tab(sy, age) : 256;
    int d = dy && dy->n ? tw_tab(dy, age) : 0;
    amount256 = DOOMO_CLAMP(amount256, 0, 256);
    q.sx = (int16_t)(256 + (((x - 256) * amount256) >> 8));
    q.sy = (int16_t)(256 + (((y - 256) * amount256) >> 8));
    d = (d * amount256) >> 8;
    q.dy = (int8_t)DOOMO_CLAMP(d, -128, 127);
    return q;
}

int tw_bounce_amount(void)
{
    int b = opt_get(OPT_UI_BOUNCE);           /* off / subtle / boingy */
    return b <= 0 ? 0 : b == 1 ? 128 : 256;
}

/* ---- springs ---------------------------------------------------------------
 * Exact sampled damped oscillators (CT sec.3.2): e[n+1] = a1 e[n] + a2 e[n-1],
 * pos = target + e. c is e[-1]/e[0] of a release from rest, so a step or a
 * punch from rest follows the continuous curve from its very first frame
 * (core/tools/trailer_curves.c derives all three; test_tween.c checks them
 * against core/tests/data/trailer_curves.h). Positions stay below 4096 px
 * (Q4 65536) so a1*e + a2*ep fits 32 bits. */
typedef struct { int16_t a1, a2, c; } TwPreset;
static const TwPreset TW_PRESET[TW_SPRING_COUNT] = {
    { -1287,  -3705, -18279 },   /* TW_JELLY  3.8 f, zeta 0.41  letter SX table   */
    {   903,  -2226, -26253 },   /* TW_SLAM4  4.2 f, 0.555      bounce(a/4) "!"   */
    { 12029,  -5196,   4247 },   /* TW_SLAM7  7.3 f, 0.555      slam captions     */
    { 14583,  -6039,   7536 },   /* TW_SLAM8  8.4 f, 0.555      polaroids, lids   */
    { 16477,  -7710,   9322 },   /* TW_BRAKE  8.4 f, 0.45       s03 brake         */
    { 15498,  -5178,   8631 },   /* TW_BOING 11 f,   0.71       s05 window boing  */
    { 19409, -11497,  10711 },   /* TW_WOBBLE 8 f,   0.22       furniture wiggle  */
    { 20746,  -7039,  13706 },   /* TW_FOLLOW 24 f,  0.85       camera, cursor    */
};

static int32_t q14mul(int32_t v, int32_t k) { return (v * k + 8192) >> 14; }

/* the release ratio of the preset this spring was built with (1.0 for a
 * hand-made one: then ep = e, the "snappier first frame" variant) */
static int32_t spring_c(const TwSpring *s)
{
    int i;
    for (i = 0; i < TW_SPRING_COUNT; i++)
        if (TW_PRESET[i].a1 == s->a1 && TW_PRESET[i].a2 == s->a2) return TW_PRESET[i].c;
    return 16384;
}

void tw_spring_init(TwSpring *s, int preset, int32_t pos_q4)
{
    if (!s) return;
    if (preset < 0 || preset >= TW_SPRING_COUNT) preset = TW_FOLLOW;
    s->e = s->ep = 0;
    s->target = pos_q4;
    s->a1 = TW_PRESET[preset].a1;
    s->a2 = TW_PRESET[preset].a2;
}

/* A displacement d released from rest, added on top of the current motion
 * (the recurrence is linear, so motions superpose). */
static void spring_release(TwSpring *s, int32_t d)
{
    s->e += d;
    s->ep += q14mul(d, spring_c(s));
}

void tw_spring_target(TwSpring *s, int32_t target_q4)
{
    if (!s) return;
    spring_release(s, s->target - target_q4);
    s->target = target_q4;
}

void tw_spring_punch(TwSpring *s, int32_t d_q4)
{
    if (s) spring_release(s, d_q4);
}

void tw_spring_shove(TwSpring *s, int32_t dv_q4)
{
    if (s) s->ep -= dv_q4;
}

int32_t tw_spring_step(TwSpring *s)
{
    int32_t n;
    if (!s) return 0;
    n = (s->a1 * s->e + s->a2 * s->ep + 8192) >> 14;
    /* the rounded recurrence can idle at +-1/16 px forever: settle it */
    if (n >= -1 && n <= 1 && s->e >= -1 && s->e <= 1) n = 0;
    s->ep = s->e;
    s->e = n;
    return s->target + n;
}

int tw_spring_resting(const TwSpring *s)
{
    return !s || (s->e == 0 && s->ep == 0);
}
