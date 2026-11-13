/* voiceref.c - the reference render at DoomoVoice level (DC5b; owner:
 * dreamcast). What musrender is for plat_tone, this is for plat_voice: a
 * second renderer, written from the rules in docs/DREAMCAST.md section 6
 * and nothing of dc/src, one frame at a time with no blocks, no tables and
 * no state shared with the mixer. test_tier feeds it the very records the
 * core's driver sends (and the bus and tape calls) at the frame they take
 * effect, and compares its output with the Dreamcast mixer's, sample by
 * sample.
 *
 * The rules:
 *   pitch   C0 = 440 * 2^(-57/12) Hz. The phase step of pitch p (1/16
 *           semitones above C0, 1..1727) is the step of the same note in
 *           octave 8, round(C0 * 2^(8 + (p mod 192) / 192) * 2^32 / 44100),
 *           shifted right by 8 - p / 192. A tape stop of d lowers p by d,
 *           not below 56 (20 Hz).
 *   kit     a hit's noise is clocked at f0 + (f1 - f0) * t / sweep Hz (f1
 *           from t = sweep on), t = the record's tparam; the step is
 *           floor(f * 2^32 / 44100); a tape stop multiplies it by
 *           2^(-d / 192) (floor(step * step8(192 - d mod 192) / step8(0)),
 *           halved, then halved per whole octave), not below 20 Hz.
 *   waves   pulse: high while phase < duty / 32 (PULSE and PWM: the
 *           record's tparam, 0 = 4, at most 31; other timbres 16); triangle and saw from the
 *           phase's top 16 bits; noise: the 15-bit LFSR (taps 0 and 1, the
 *           new bit also to bit 6 for the 7-bit one), clocked when the
 *           phase wraps. A sample is +-4096 at most.
 *   gain    round(place * vol * bus * 15 / (63 * 255)) per side; place is
 *           128 / 128 for voices 0 and 2 (and 4 up), 152 / 104 for voice 1,
 *           104 / 152 for voice 3.
 *   key-on  noise: LFSR 0x7FFF, phase 0. Pitched: phase 0 if the voice was
 *           silent (both gains 0), else the phase runs on.
 *   output  the mixer's chain (audio_int.h): the sum / 128 through the
 *           14 Hz DC block and the 12 kHz low-pass, x 24 / 256, the knee.
 */
#include <math.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/music.h"
#include "../../../core/src/mus_int.h"
#include "voiceref.h"

#define NV 8
typedef struct RefVoice {
    DoomoVoice rec;
    uint32_t ph, lfsr;
    int32_t gl, gr;
    int on;
} RefVoice;

static RefVoice rv[NV];
static int bus = 255, tape;
static int64_t dcx[2], dcy[2], lp[2];
static uint32_t ref_clips;

static uint32_t step8(int i)
{
    const double c0 = 440.0 * pow(2.0, -57.0 / 12.0);
    return (uint32_t)llround(c0 * pow(2.0, 8.0 + i / 192.0) * 4294967296.0 / 44100.0);
}

uint32_t ref_pitch_step(int p)
{
    if (p < 1) p = 1;
    if (p > 1727) p = 1727;
    return step8(p % 192) >> (8 - p / 192);
}

static int is_kit(const DoomoVoice *r) { return r->inst >= MUS_INST_KIT0; }

static int wave_of(const DoomoVoice *r)      /* 0 pulse, 1 tri, 2 saw, 3 noise15, 4 noise7 */
{
    int w = 0;
    if (is_kit(r)) w = 3;
    else switch (MUS_INST(r->inst)->timbre) {
    case TIMBRE_SAW: case TIMBRE_SYNC_SAW: w = 2; break;
    case TIMBRE_TRI: case TIMBRE_TRI4: case TIMBRE_SINE: w = 1; break;
    case TIMBRE_NOISE15: w = 3; break;
    case TIMBRE_NOISE7: w = 4; break;
    default: break;
    }
    if (w == 3 && (r->flags & DVF_NOISE7)) w = 4;
    return w;
}

static uint32_t step_of(const DoomoVoice *r)
{
    if (!r->pitch16 || (r->flags & DVF_CUT)) return 0;
    if (is_kit(r)) {
        const MusKitHit *d = &mus_kit_classic[r->pitch16 & 7];
        int t = r->tparam;
        long f = (d->ticks_sweep == 0 || t >= d->ticks_sweep) ? d->f1 :
                 (long)d->f0 + ((long)d->f1 - d->f0) * t / d->ticks_sweep;
        uint64_t s;
        f &= 0xFFFF;
        if (!f) return 0;
        s = ((uint64_t)f << 32) / 44100u;
        if (tape) {
            int r192 = tape % 192;
            if (r192) s = (s * step8(192 - r192) / step8(0)) >> 1;
            s >>= tape / 192;
            if (s < 1947816u) s = 1947816u;
        }
        return (uint32_t)s;
    } else {
        int p = r->pitch16;
        if (tape) { p -= tape; if (p < 56) p = 56; }
        return ref_pitch_step(p);
    }
}

static void gains(int v)
{
    static const int place[4][2] = { { 128, 128 }, { 152, 104 }, { 128, 128 }, { 104, 152 } };
    RefVoice *o = &rv[v];
    const int *pl = place[v < 4 ? v : 0];
    int vl = o->rec.vol_l > 63 ? 63 : o->rec.vol_l, vr = o->rec.vol_r > 63 ? 63 : o->rec.vol_r;
    if (!step_of(&o->rec)) { o->gl = o->gr = 0; return; }
    o->gl = (int32_t)lround((double)pl[0] * vl * bus * 15.0 / 16065.0);
    o->gr = (int32_t)lround((double)pl[1] * vr * bus * 15.0 / 16065.0);
}

void ref_reset(void)
{
    int i;
    memset(rv, 0, sizeof(rv));
    for (i = 0; i < NV; i++) rv[i].lfsr = 0x7FFF;
    bus = 255;
    tape = 0;
    memset(dcx, 0, sizeof(dcx));
    memset(dcy, 0, sizeof(dcy));
    memset(lp, 0, sizeof(lp));
    ref_clips = 0;
}

void ref_voice(int v, const DoomoVoice *r)
{
    RefVoice *o;
    if (v < 0 || v >= NV) return;
    o = &rv[v];
    if (r->flags & DVF_KEYON) {
        if (wave_of(r) >= 3) { o->lfsr = 0x7FFF; o->ph = 0; }
        else if (!o->gl && !o->gr) o->ph = 0;
    }
    o->rec = *r;
    o->on = 1;
    gains(v);
}

void ref_bus(int g) { int v; bus = g; for (v = 0; v < NV; v++) if (rv[v].on) gains(v); }
void ref_tape(int d) { int v; tape = d < 0 ? 0 : d > 768 ? 768 : d; for (v = 0; v < NV; v++) if (rv[v].on) gains(v); }

static int64_t fdiv(int64_t a, int64_t b)   /* floor(a / b), b > 0 */
{
    int64_t q = a / b;
    return (a % b < 0) ? q - 1 : q;
}

static int16_t out_chain(int ch, int64_t mix)
{
    int64_t x = fdiv(mix, 128), v = dcy[ch], d, y, a, e;
    /* the leak: v / 512 rounded away from zero */
    v -= v >= 0 ? fdiv(v + 511, 512) : -fdiv(511 - v, 512);
    dcy[ch] = x - dcx[ch] + v;
    dcx[ch] = x;
    d = dcy[ch] - lp[ch];
    lp[ch] += fdiv(d * 210, 256);
    y = lp[ch] * 3;
    y = y < 0 ? -((-y) / 32) : y / 32;        /* toward zero */
    a = y < 0 ? -y : y;
    if (a > 24576) {
        if (a > 32767) ref_clips++;
        e = a - 24576;
        a = 24576 + e * 8191 / (e + 8191);
        y = y < 0 ? -a : a;
    }
    return (int16_t)y;
}

void ref_render(int16_t *out, int frames)
{
    int i, v;
    for (i = 0; i < frames; i++) {
        int64_t l = 0, r = 0;
        for (v = 0; v < NV; v++) {
            RefVoice *o = &rv[v];
            uint32_t step;
            int w, s;
            if (!o->on || (!o->gl && !o->gr)) continue;
            step = step_of(&o->rec);
            w = wave_of(&o->rec);
            if (w >= 3) {
                uint32_t p0 = o->ph;
                o->ph += step;
                if (o->ph < p0) {
                    uint32_t bit = (o->lfsr ^ (o->lfsr >> 1)) & 1u;
                    o->lfsr = (o->lfsr >> 1) | (bit << 14);
                    if (w == 4) o->lfsr = (o->lfsr & ~0x40u) | (bit << 6);
                }
                s = (o->lfsr & 1) ? 4096 : -4096;
            } else {
                uint32_t ph = o->ph;
                long u = (long)(ph >> 16);
                o->ph += step;
                if (w == 1) s = (int)fdiv((u < 32768 ? u : 65535 - u) - 16384, 4);
                else if (w == 2) s = (int)fdiv(u - 32768, 8);
                else {
                    /* PULSE / PWM: tparam / 32 (0: 4, at most 31); any other timbre 16 */
                    int tb = MUS_INST(o->rec.inst)->timbre, duty = 16;
                    if (tb == TIMBRE_PULSE || tb == TIMBRE_PWM) duty = o->rec.tparam ? o->rec.tparam : 4;
                    if (duty > 31) duty = 31;
                    s = ((uint64_t)ph < ((uint64_t)duty << 27)) ? 4096 : -4096;
                }
            }
            l += (int64_t)s * o->gl;
            r += (int64_t)s * o->gr;
        }
        out[i * 2] = out_chain(0, l);
        out[i * 2 + 1] = out_chain(1, r);
    }
}
