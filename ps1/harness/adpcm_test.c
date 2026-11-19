/* adpcm_test.c - host test of the PS1 SPU ADPCM encoder (ps1/src/adpcm.c).
 *
 *   cc -O2 -I ps1/src -I core/include ps1/harness/adpcm_test.c ps1/src/adpcm.c -lm -o adpcm_test
 *   ./adpcm_test
 *
 * Builds the chiptune loops the SPU driver builds at boot (waveform.h: pulse, triangle, saw; the
 * very same functions), checks their shape (no DC, exact duty, harmonic levels, fade-in) and plays
 * the encoded loops the way the SPU does (the decoder history carried round the loop jump) to check
 * that every pass decodes the same and close to the source. Plus synthetic sine and 8-bit sound
 * effects through the encoder and the reference decoder (signal-to-noise).
 * Exit code 0 = pass.
 */
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "adpcm.h"
#include "waveform.h"
#include "instrument_bank.h"

#define MAXN (28 * 512)

static double snr(const int16_t *a, const int16_t *b, int n)
{
    double s = 0, e = 0;
    int i;
    for (i = 0; i < n; i++) {
        double d = (double)a[i] - b[i];
        s += (double)a[i] * a[i];
        e += d * d;
    }
    if (e < 1e-9) return 99.0;
    return 10.0 * log10(s / e);
}

/* skip: measure from this sample on (looped waves: the second cycle) */
static int check_from(const char *name, const int16_t *pcm, int n, int fast, double min_db,
                      int skip)
{
    static uint8_t enc[MAXN / 28 * 16 + 16];
    static int16_t dec[MAXN + 28];
    int blocks = adpcm_encode(pcm, n, enc, fast);
    double db;
    adpcm_decode(enc, blocks, dec);
    db = snr(pcm + skip, dec + skip, n - skip);
    printf("%-22s %5d samples %3d blocks  SNR %6.2f dB (min %4.1f)  %s\n", name, n, blocks, db,
           min_db, db >= min_db ? "ok" : "FAIL");
    if ((enc[0] >> 4) != 0) {
        printf("  first block must use filter 0 (clean loops)\n");
        return 0;
    }
    return db >= min_db;
}

/* The SPU decoder (adpcm_decode's maths) with the history kept between blocks, so a loop can be
 * played round: the state at the jump back is what the last block of the loop left. */
static const int k_f0[5] = { 0, 60, 115, 98, 122 };
static const int k_f1[5] = { 0, 0, -52, -55, -60 };
typedef struct Spu { int old, older; } Spu;

static void spu_block(const uint8_t *in, Spu *st, int16_t *pcm)
{
    int shift = in[0] & 15, filter = (in[0] >> 4) & 7, i;
    if (filter > 4) filter = 0;
    for (i = 0; i < 28; i++) {
        int nib = (in[2 + i / 2] >> ((i & 1) * 4)) & 15;
        int s = (int16_t)(nib << 12) >> shift;
        s += (st->old * k_f0[filter] + st->older * k_f1[filter] + 32) >> 6;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        pcm[i] = (int16_t)s;
        st->older = st->old;
        st->old = s;
    }
}

/* Key on and play: the first cycle once (it is blocks [0, first)), then the loop (blocks
 * [first, 2 first)) 'passes' times. first_out gets the first cycle, prev / last the last two
 * passes of the loop. */
static void spu_play(const uint8_t *enc, int first, int passes, int16_t *first_out,
                     int16_t *prev, int16_t *last)
{
    Spu st = { 0, 0 };
    int b, p;
    for (b = 0; b < first; b++) spu_block(enc + b * 16, &st, first_out + b * 28);
    for (p = 0; p < passes; p++)
        for (b = 0; b < first; b++) {
            int16_t *dst = p == passes - 1 ? last : prev;
            spu_block(enc + (first + b) * 16, &st, dst + b * 28);
        }
}

/* amplitude of harmonic k of one cycle */
static double harmonic(const int16_t *c, int len, int k)
{
    double re = 0, im = 0;
    int i;
    for (i = 0; i < len; i++) {
        re += c[i] * cos(2 * M_PI * k * i / len);
        im += c[i] * sin(2 * M_PI * k * i / len);
    }
    return 2.0 * sqrt(re * re + im * im) / len;
}

/* kind: 0 saw, 1 pulse (duty_eighths), 2 triangle. min_db: loop signal-to-noise floor. */
static int check_wave(const char *name, int kind, int duty_eighths, int len, double min_db)
{
    static int16_t pcm[PS1_WAVE_MAX_CYCLE * 2], ideal[PS1_WAVE_MAX_CYCLE];
    static int16_t first_out[PS1_WAVE_MAX_CYCLE], prev[PS1_WAVE_MAX_CYCLE], last[PS1_WAVE_MAX_CYCLE];
    static uint8_t enc[PS1_WAVE_MAX_CYCLE / 28 * 2 * 16];
    int ok = 1, i, k, peak = 0, diff = 0, blocks, cyc = len, first = len < 28 ? 1 : len / 28;
    long sum = 0;
    double dc, worst = 0, db;
    double duty = duty_eighths / 8.0;

    if (kind == 0) ps1_wave_saw(pcm, len);
    else if (kind == 1) ps1_wave_pulse(pcm, len, duty_eighths);
    else ps1_wave_triangle(pcm, len);
    memcpy(ideal, pcm, (size_t)len * sizeof(pcm[0]));
    if (cyc < 28) {                          /* the cycle is repeated to fill the block */
        for (i = cyc; i < 28; i++) ideal[i] = ideal[i - cyc];
        len = 28;
    }
    for (i = 0; i < cyc; i++) {
        sum += pcm[i];
        if (abs(pcm[i]) > peak) peak = abs(pcm[i]);
    }
    dc = (double)sum / cyc;
    ok &= fabs(dc) < 2.0 && peak < 30000;

    if (kind != 2) {                         /* harmonic levels against the closed-form series */
        for (k = 1; k <= (cyc - 1) / 2 && k <= 10; k++) {          /* the first ten: the rest are small, rounding shows */
            double resp, t;
            if (cyc != 28)
                resp = (6 + 2 * cos(2 * M_PI * k / cyc)) / 8;
            else
                resp = k <= 3 ? 1.0 : 0.5 * (1 + cos(M_PI * (k - 3.5) / 10.5));
            t = (kind == 0 ? PS1_WAVE_AMP * 2 / (M_PI * k)
                           : PS1_WAVE_AMP * 4 / (M_PI * k) * fabs(sin(M_PI * k * duty))) * resp;
            if (t < 150) continue;           /* the zero harmonics of a 50 % pulse, deep in the roll-off */
            if (fabs(20 * log10(harmonic(ideal, cyc, k) / t)) > worst)
                worst = fabs(20 * log10(harmonic(ideal, cyc, k) / t));
        }
        ok &= worst < 0.15;
    }

    blocks = ps1_wave_encode_cycle(pcm, cyc, enc);
    ok &= blocks == first * 2;
    ok &= (enc[first * 16 + 1] & 4) != 0 && (enc[(blocks - 1) * 16 + 1] & 3) == 3;
    spu_play(enc, first, 12, first_out, prev, last);
    db = snr(ideal, last, len);
    for (i = 0; i < len; i++)
        if (abs(last[i] - prev[i]) > diff) diff = abs(last[i] - prev[i]);
    ok &= db >= min_db && diff <= 10;   /* 9 on the 25 % pulse of 56: a quantiser step or so */
    printf("%-18s %3d  DC %5.1f peak %5d  harmonics within %.2f dB  loop SNR %5.1f dB (min %4.1f)  pass diff %2d  %s\n",
           name, cyc, dc, peak, worst, db, min_db, diff, ok ? "ok" : "FAIL");
    return ok;
}

/* the key-on fade: the first cycle starts at silence, the loop cycle is untouched */
static int check_fade(void)
{
    static int16_t plain[PS1_WAVE_MAX_CYCLE], pcm[PS1_WAVE_MAX_CYCLE * 2];
    static uint8_t enc[8 * 16];
    int ok = 1, i;
    ps1_wave_pulse(plain, 112, 1);                /* 12.5 %: sample 0 sits on the edge, far from zero */
    memcpy(pcm, plain, sizeof plain);
    ps1_wave_encode_loop(pcm, 112, enc);
    ok &= pcm[0] == 0;
    for (i = 28; i < 112; i++) ok &= pcm[i] == plain[i];
    for (i = 0; i < 112; i++) ok &= pcm[112 + i] == plain[i];
    for (i = 1; i < 28; i++) ok &= abs(pcm[i]) <= abs(plain[i]) + 1;
    printf("%-18s first sample %d of %d, cycle 2 untouched  %s\n", "fade-in", pcm[0], plain[0],
           ok ? "ok" : "FAIL");
    return ok;
}

/* the 14 and 7 sample cycles come out at the level of the 28 sample one (spu.c build_waves) */
static int check_level_match(void)
{
    static int16_t p[56];
    int ok = 1, w, c;
    for (w = 0; w < 4; w++) {
        double ref = 0;
        for (c = 28; c >= 7; c /= 2) {
            double db;
            uint32_t ms;
            if (w == 0) ps1_wave_pulse(p, c, 1); else if (w == 1) ps1_wave_pulse(p, c, 4);
            else if (w == 2) ps1_wave_triangle(p, c); else ps1_wave_saw(p, c);
            if (c == 28) ref = ps1_wave_ms(p, c);
            else ps1_wave_match(p, c, (uint32_t)ref);
            ms = ps1_wave_ms(p, c);
            db = 10 * log10((double)ms / ref);
            ok &= fabs(db) < 0.6;
            { int i, pk = 0; for (i = 0; i < c; i++) if (abs(p[i]) > pk) pk = abs(p[i]); ok &= pk <= 30000; }
        }
    }
    printf("%-18s 14 / 7 sample cycles within 0.6 dB of the 28 sample level  %s\n", "level match", ok ? "ok" : "FAIL");
    return ok;
}

/* adpcm_encode_loop and ps1_wave_encode_loop refuse lengths they cannot hold, and a loop made
 * from a plain sine plays back the same on every pass */
static int check_encode_loop(void)
{
    static int16_t cyc[28 * 9], pcm[PS1_WAVE_MAX_CYCLE * 2], first[28], prev[28], last[28];
    static uint8_t enc[16 * 9];
    int ok = 1, i;
    for (i = 0; i < 28 * 9; i++) cyc[i] = (int16_t)(9000 * sin(2 * M_PI * i / 28));
    ok &= adpcm_encode_loop(cyc, 27, enc) == 0;
    ok &= adpcm_encode_loop(cyc, 0, enc) == 0;
    ok &= adpcm_encode_loop(cyc, 28 * 9, enc) == 0;
    ok &= ps1_wave_encode_loop(pcm, 140, enc) == 0;
    ok &= ps1_wave_encode_loop(pcm, 30, enc) == 0;
    ok &= adpcm_encode_loop(cyc, 28, enc) == 1;
    {   /* one block: play it round */
        Spu st = { 0, 0 };
        int p, d = 0;
        for (p = 0; p < 14; p++) { memcpy(prev, last, sizeof prev); spu_block(enc, &st, last); }
        for (i = 0; i < 28; i++) if (abs(last[i] - prev[i]) > d) d = abs(last[i] - prev[i]);
        ok &= d <= 8 && snr(cyc, last, 28) > 40.0;
        (void)first;
    }
    printf("%-18s refuses bad lengths, plays back the same each pass  %s\n", "encode_loop",
           ok ? "ok" : "FAIL");
    return ok;
}


/* The PS1 hybrid instrument patches use the same SPU loop path as the legacy waves.
 * Test every patch in every pitch zone, including the repeated 14/7-sample cycles. */
static int check_instrument(uint8_t wave)
{
    static const int zones[] = { 112, 56, 28, 14, 7 };
    static int16_t pcm[PS1_WAVE_MAX_CYCLE * 2], ideal[PS1_WAVE_MAX_CYCLE];
    static int16_t first_out[PS1_WAVE_MAX_CYCLE], prev[PS1_WAVE_MAX_CYCLE], last[PS1_WAVE_MAX_CYCLE];
    static uint8_t enc[PS1_WAVE_MAX_CYCLE / 28 * 2 * 16];
    uint32_t ref_ms = 0;
    int ok = 1, z;
    if ((ps1_patches[wave].adsr2 & 0x1fc0u) != PS1_ADSR_SUSTAIN_HOLD ||
        ps1_instrument_adsr1(wave) != ps1_patches[wave].adsr1 ||
        ps1_instrument_adsr2(wave) != ps1_patches[wave].adsr2)
        return 0;
    for (z = 0; z < 5; z++) {
        int cyc = zones[z], samples = cyc < 28 ? 28 : cyc;
        int first = cyc < 28 ? 1 : cyc / 28;
        int blocks, i, peak = 0, diff = 0;
        double dc = 0.0, db;
        long sum = 0;
        if (!ps1_instrument_cycle(wave, pcm, cyc)) return 0;
        if (cyc == 28) ref_ms = ps1_wave_ms(pcm, cyc);
        if (cyc < 28) ps1_wave_match(pcm, cyc, ref_ms);
        memcpy(ideal, pcm, (size_t)cyc * sizeof(pcm[0]));
        if (cyc < 28)
            for (i = cyc; i < 28; i++) ideal[i] = ideal[i - cyc];
        for (i = 0; i < cyc; i++) {
            sum += pcm[i];
            if (abs(pcm[i]) > peak) peak = abs(pcm[i]);
        }
        dc = (double)sum / cyc;
        blocks = ps1_wave_encode_cycle(pcm, cyc, enc);
        if (blocks != first * 2 || (enc[first * 16 + 1] & 4) == 0 ||
            (enc[(blocks - 1) * 16 + 1] & 3) != 3) ok = 0;
        spu_play(enc, first, 12, first_out, prev, last);
        db = snr(ideal, last, samples);
        for (i = 0; i < samples; i++) {
            if (abs(last[i] - prev[i]) > diff) diff = abs(last[i] - prev[i]);
        }
        if (fabs(dc) > 2.0 || peak > 22000 || db < (cyc >= 28 ? 14.0 : 8.0) || diff > 20)
            ok = 0;
        printf("  %-20s zone %3d  DC %5.1f peak %5d  loop SNR %5.1f dB  pass diff %2d  %s\n",
               ps1_patches[wave].name, cyc, dc, peak, db, diff,
               (fabs(dc) <= 2.0 && peak <= 22000 && db >= (cyc >= 28 ? 14.0 : 8.0) && diff <= 20 && blocks == first * 2) ? "ok" : "FAIL");
    }
    return ok;
}

static int check(const char *name, const int16_t *pcm, int n, int fast, double min_db)
{
    return check_from(name, pcm, n, fast, min_db, 0);
}

int main(void)
{
    static int16_t pcm[MAXN];
    int i, ok = 1, len;
    uint32_t rng = 1234567;

    /* the SPU driver's waves (waveform.h): [first cycle, fades in][loop cycle], played the way the
     * SPU plays them */
    ok &= check_wave("pulse 12.5%", 1, 1, 112, 20);
    ok &= check_wave("pulse 12.5%", 1, 1, 28, 15);
    ok &= check_wave("pulse 25%", 1, 2, 112, 20);
    ok &= check_wave("pulse 25%", 1, 2, 28, 15);
    ok &= check_wave("pulse 50%", 1, 4, 112, 20);
    ok &= check_wave("pulse 50%", 1, 4, 28, 15);
    ok &= check_wave("triangle", 2, 0, 112, 30);
    ok &= check_wave("triangle", 2, 0, 28, 20);
    ok &= check_wave("saw", 0, 0, 112, 20);
    ok &= check_wave("saw", 0, 0, 28, 15);
    ok &= check_wave("pulse 12.5%", 1, 1, 56, 18);
    ok &= check_wave("pulse 12.5%", 1, 1, 14, 12);
    ok &= check_wave("pulse 12.5%", 1, 1, 7, 8);
    ok &= check_wave("pulse 25%", 1, 2, 56, 18);
    ok &= check_wave("pulse 25%", 1, 2, 14, 12);
    ok &= check_wave("pulse 25%", 1, 2, 7, 8);
    ok &= check_wave("pulse 50%", 1, 4, 56, 18);
    ok &= check_wave("pulse 50%", 1, 4, 14, 12);
    ok &= check_wave("pulse 50%", 1, 4, 7, 8);
    ok &= check_wave("triangle", 2, 0, 56, 25);
    ok &= check_wave("triangle", 2, 0, 14, 12);
    ok &= check_wave("triangle", 2, 0, 7, 8);
    ok &= check_wave("saw", 0, 0, 56, 18);
    ok &= check_wave("saw", 0, 0, 14, 12);
    ok &= check_wave("saw", 0, 0, 7, 8);
    ok &= check_fade();
    ok &= check_level_match();
    ok &= check_encode_loop();
    /* PS1 hybrid instrument bank: these are the timbres uploaded by spu.c. */
    for (i = 0; i < WAVE_NOISE; i++)
        ok &= check_instrument((uint8_t)i);
    /* smooth tones */
    len = 28 * 100;
    for (i = 0; i < len; i++) pcm[i] = (int16_t)(16000 * sin(i * 2 * M_PI * 440 / 22050.0));
    ok &= check("sine 440 Hz", pcm, len, 0, 30);
    ok &= check("sine 440 Hz (fast)", pcm, len, 1, 24);
    /* 8-bit sound effects as the core makes them: decaying chirp + noise */
    len = 28 * 300;
    for (i = 0; i < len; i++) {
        double env = exp(-i / 2500.0);
        double f = 200 + 2000 * exp(-i / 1500.0);
        int8_t v = (int8_t)(100 * env * sin(i * 2 * M_PI * f / 11025.0));
        pcm[i] = (int16_t)(v * 256);
    }
    ok &= check("sfx chirp (fast)", pcm, len, 1, 18);
    for (i = 0; i < len; i++) {
        rng = rng * 1103515245u + 12345u;
        pcm[i] = (int16_t)((int8_t)(((rng >> 16) & 255) * exp(-i / 3000.0) - 0) * 256);
    }
    ok &= check("sfx noise burst (fast)", pcm, len, 1, 6);
    printf("adpcm_test: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
