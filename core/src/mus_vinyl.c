/* mus_vinyl.c - the record-scratch DSP: patterns, the platter speed and
 * DECK scrubbing (owner: MU1; docs/design/music/SPEC.md sec.7, D6, D11,
 * CF-19, CF-20, CF-40).
 *
 * A pattern is MU4d's (core/music/common/vinyl.dms, the MusVinyl layout of
 * mus_data.h): segments of a length in units (or 30 Hz frames with
 * MVF_FRAMES, converted at the pattern's start at the master tempo), a
 * platter speed v0 -> v1 in half-semitone log steps (|v| = 2^((L - 64) /
 * 24), MVS_REV = backwards, L = 0 stopped) along a curve - lin (linear in
 * speed), ease (smoothstep), exp (linear in pitch; towards or from a stop
 * it decays like a platter running down) or sin (a hand stroke: v0 -> v1
 * -> v0) - a fader action and a seeded hand jitter (2 % steps).
 *
 * mus_vinyl_speed(p, t, seed) is a pure function of the unit t since the
 * pattern's start, called by the driver every control frame (pitch and
 * gain) and by the arranger once per unit (where the playhead P is), so
 * the two agree bit for bit (T9). P = start + sum of the speeds / 256.
 *
 * DECK (everywhere, 0 B): ring A keeps its look-ahead under a mute window
 * (the ghost keeps time), the driver bends deck A's voices, and each
 * music_update() looks a window of clamp(gap + 8, 16, 48) mt ahead (CF-19):
 * every 3 units it asks the scrub decoder (deck B's storage, CF-20's bar
 * log) which note each pitched voice sounds at P and posts a re-key on ring
 * B whenever it changes, so scratching backwards plays the earlier notes in
 * reverse with fresh attacks. A looping pattern (WOBBLE) only bends: no
 * mute, no re-keys, until VINYL_NONE or another pattern replaces it. TAPE
 * is M7: VSRC_TAPE falls back to DECK. */
#include "mus_int.h"

MusVinylRt mus_vy;

int mus_vinyl_get(int pattern, MusVinylView *out)
{
    const uint8_t *p, *tab = mus_bank_misc(MUS_MISC_VINYL);
    unsigned o;
    int i;
    uint32_t len = 0;
    if (pattern <= 0 || !tab || pattern >= mus_bank()->n_vinyl || !(o = mus_rd16(tab + 2 * pattern))) return 0;
    p = tab + o;
    if (!p[0]) return 0;                                  /* CRACKLE: a toggle, no segments */
    out->seg = p + 4;
    out->n_seg = p[0];
    out->flags = p[1];
    out->after = p[3];
    out->pad = 0;
    /* frame-timed segments run at the master tempo of the pattern's start:
     * units per 30 Hz frame = the driver's units per mt x 8 */
    out->fr_q8 = (p[1] & MVF_FRAMES) ? (uint16_t)DOOMO_CLAMP((mus_drv_tempo() * 8u + 128u) >> 8, 1u, 0xFFFFu) : 256;
    for (i = 0; i < p[0]; i++) len += (uint32_t)p[4 + 4 * i] * out->fr_q8 >> 8;
    out->len = (uint16_t)DOOMO_MIN(len, 0xFFFFu);
    return out->len > 0;
}

static int seg_units(const MusVinylView *p, const uint8_t *s) { return (int)((uint32_t)s[0] * p->fr_q8 >> 8); }

/* the segment holding unit t: *t becomes the unit inside it (looping
 * patterns repeat; others hold their last segment's end) */
static const uint8_t *seg_at(const MusVinylView *p, int *t, int *k)
{
    const uint8_t *s = p->seg;
    int i;
    if (*t < 0) *t = 0;
    if (p->len && *t >= p->len) {
        if (!(p->flags & MVF_LOOP)) { *k = p->n_seg - 1; s += 4 * *k; *t = seg_units(p, s); return s; }
        *t %= p->len;
    }
    for (i = 0; i < p->n_seg; i++, s += 4) {
        int u = seg_units(p, s);
        if (*t < u || i == p->n_seg - 1) { *k = i; return s; }
        *t -= u;
    }
    *k = 0;
    return p->seg;
}

/* 4096 x 2^(i/24): |v| = 2^((L - 64) / 24) in half-semitone log steps */
static const uint16_t exp24[25] = { 4096, 4216, 4340, 4467, 4598, 4733, 4871, 5014, 5161, 5312, 5468, 5628,
                                    5793, 5962, 6137, 6317, 6502, 6693, 6889, 7091, 7298, 7512, 7732, 7959, 8192 };

/* the speed (Q8) of a log code in 1/16 steps: e16 = (L - 64) x 16 */
static int speed_of_e16(int e16)
{
    int q = e16 >= 0 ? e16 / 384 : -((-e16 + 383) / 384), r = e16 - q * 384, i = r >> 4, f = r & 15;
    int32_t v = exp24[i] + (((int32_t)exp24[i + 1] - exp24[i]) * f >> 4);   /* Q12 */
    v = q >= 0 ? v << q : v >> -q;
    return (int)((v + 8) >> 4);                                              /* Q8 */
}

static int speed_of(int code)
{
    int v = (code & 127) ? speed_of_e16(((code & 127) - 64) * 16) : 0;
    return (code & MVS_REV) ? -v : v;
}

int mus_vinyl_speed(const MusVinylView *p, int t, uint32_t seed)
{
    int k, f, c, v0, v1, v, jit, u;
    const uint8_t *s = seg_at(p, &t, &k);
    u = seg_units(p, s);
    f = u ? (t * 256) / u : 256;
    if (f > 256) f = 256;
    v0 = speed_of(s[1]);
    v1 = speed_of(s[2]);
    switch (s[3] & 3) {
    case MVC_EASE: c = (f * f * (768 - 2 * f)) >> 16; v = v0 + (((v1 - v0) * c) >> 8); break;
    case MVC_SIN: {                                   /* a hand stroke: v0 -> v1 -> v0 */
        int a = f * 8;
        c = (int)(doomo_sin_q[a <= 1024 ? a : 2048 - a] >> 8);
        v = v0 + (((v1 - v0) * c) >> 8);
        break; }
    case MVC_EXP:
        if ((s[1] & 127) && (s[2] & 127) && !((s[1] ^ s[2]) & MVS_REV)) {   /* linear in pitch */
            int e0 = ((s[1] & 127) - 64) * 16, e1 = ((s[2] & 127) - 64) * 16;
            v = speed_of_e16(e0 + (((e1 - e0) * f) >> 8));
            if (s[1] & MVS_REV) v = -v;
        } else {                                       /* to / from a stop: a platter running down */
            c = 256 - (((256 - f) * (256 - f)) >> 8);
            v = v0 + (((v1 - v0) * c) >> 8);
        }
        break;
    default: v = v0 + (((v1 - v0) * f) >> 8); break;
    }
    jit = ((s[3] >> 5) * 41) >> 3;                     /* 2 % steps, Q8 */
    if (jit) v += (v * ((int)(mus_h(seed, S_VINYL, (uint32_t)k, 0) % (uint32_t)(2 * jit + 1)) - jit)) >> 8;
    return v;
}

int mus_vinyl_fader(const MusVinylView *p, int t)
{
    int k, u;
    const uint8_t *s = seg_at(p, &t, &k);
    u = seg_units(p, s);
    switch ((s[3] >> 2) & 7) {
    case MVD_CLOSED: return 0;
    case MVD_CHIRP:  return 2 * t < u;                 /* closes as the stroke slows */
    case MVD_GATE32: return (t / 6) % 2 == 0;          /* chopped at 32nds */
    case MVD_FLARE:  return t < u / 2 - 1 || t > u / 2; /* one 1/64 click at the middle */
    default:         return 1;
    }
}

int32_t mus_vinyl_disp(const MusVinylView *p, int t, uint32_t seed)
{
    int32_t d = 0;
    int i;
    for (i = 0; i < t; i++) d += mus_vinyl_speed(p, i, seed);
    return d;
}

/* 192 log2(|v| / 256) in 1/16 semitones */
int mus_vinyl_pitch(int v)
{
    static const uint8_t lg[17] = { 0, 17, 33, 48, 62, 75, 88, 101, 112, 124, 134, 145, 155, 165, 174, 183, 192 };
    int e = 0, m, i;
    if (v < 0) v = -v;
    if (v < 16) return -4 * 192;
    m = v;
    while (m >= 512) { m >>= 1; e++; }
    while (m < 256) { m <<= 1; e--; }
    i = (m - 256) >> 4;
    return e * 192 + lg[i] + (((lg[i + 1] - lg[i]) * (m & 15)) >> 4);
}

int mus_vinyl_gain(int v)
{
    if (v < 0) v = -v;
    return v < 16 ? 0 : v >= 128 ? 256 : 2 * v;
}

/* ---- DECK: re-keys for the window ahead (CF-19) ---------------------------------- */
void mus_vinyl_update(void)
{
    MusScrubNote nt[MUS_A_VOICES];
    const MusVinylView vw = mus_drv_vy;              /* the view the driver bends with */
    uint32_t until;
    int t_end, win, i;
    if (!mus_vy.on || mus_vy.live || !vw.seg) return;
    /* the window: 16 mt on the bridge (gaps of 8 mt), 48 mt under an IRQ driver */
    win = (int)((mus_drv_tempo() * (mus_core_bridge() ? 16u : 48u)) >> 16) + 1;
    until = (mus_drv_clock() + (uint32_t)win) & MUS_POS_MASK;
    t_end = (int)mus_pdiff(until, mus_vy.start);
    if (t_end > vw.len) t_end = vw.len;
    while (mus_vy.t < t_end) {
        int t = mus_vy.t;
        if (t % 3 == 0 && mus_drv_room(1) >= 2 * MUS_A_VOICES) {
            uint32_t pos = (mus_vy.start + (uint32_t)t) & MUS_POS_MASK;
            uint8_t got;
            mus_vy.P = (mus_vy.start + (uint32_t)((mus_vy.disp + 128) >> 8)) & MUS_POS_MASK;
            got = mus_core_scrub(mus_vy.P, nt);
            for (i = 0; i < MUS_A_VOICES; i++) {
                int has = (got >> i) & 1;
                if (!has && !mus_vy.key[i]) continue;
                if (has && mus_vy.key[i] == nt[i].pitch && mus_vy.kstart[i] == nt[i].start) continue;
                if (has) {
                    mus_drv_push(1, MEV_MAKE_POS(MEV_INST, pos), MEV_INST_ARG(i, nt[i].inst, MUS_INST(nt[i].inst)->tparam, nt[i].vel));
                    mus_drv_push(1, MEV_MAKE_POS(MEV_NOTE, pos), MEV_NOTE_ARG(i, nt[i].pitch, nt[i].vel, 0, 0));
                    mus_vy.key[i] = nt[i].pitch;
                    mus_vy.kstart[i] = nt[i].start;
                } else {
                    mus_drv_push(1, MEV_MAKE_POS(MEV_OFF, pos), MEV_OFF_ARG(i, MOFF_RELEASE));
                    mus_vy.key[i] = 0;
                }
            }
        } else if (t % 3 == 0) break;                           /* ring B full: next tick */
        mus_vy.disp += mus_vinyl_speed(&vw, t, mus_vy.seed);
        mus_vy.t++;
    }
    if (mus_vy.t >= vw.len) mus_core_vinyl_done();
}
