/* adpcm_test.c - host test of the PS1 SPU ADPCM encoder (ps1/src/adpcm.c).
 *
 *   cc -O2 -I ps1/src ps1/harness/adpcm_test.c ps1/src/adpcm.c -lm -o adpcm_test
 *   ./adpcm_test
 *
 * Encodes the chiptune waveforms the SPU driver builds at boot (pulse,
 * triangle, saw, noise) plus synthetic 8-bit sound effects, decodes them
 * with the reference SPU decoder and checks the signal-to-noise ratio.
 * Exit code 0 = pass.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "adpcm.h"

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

static int check(const char *name, const int16_t *pcm, int n, int fast, double min_db)
{
    return check_from(name, pcm, n, fast, min_db, 0);
}

int main(void)
{
    static int16_t pcm[MAXN];
    int i, ok = 1, len;
    uint32_t rng = 1234567;

    /* the SPU driver's waves: two 112-sample cycles, the loop plays the
     * second one (see build_waves in spu.c) */
    len = 112;
    for (i = 0; i < len * 2; i++) pcm[i] = (int16_t)(i % len < len / 8 ? 12000 : -12000);
    ok &= check_from("pulse 12.5% (loop)", pcm, len * 2, 0, 20, len);
    for (i = 0; i < len * 2; i++) pcm[i] = (int16_t)(i % len < len / 2 ? 12000 : -12000);
    ok &= check_from("pulse 50% (loop)", pcm, len * 2, 0, 20, len);
    for (i = 0; i < len * 2; i++) {
        int ph = ((i % len) * 4 * 14000) / len;
        pcm[i] = (int16_t)(ph < 14000 ? ph : ph < 42000 ? 28000 - ph : ph - 56000);
    }
    ok &= check_from("triangle (loop)", pcm, len * 2, 0, 30, len);
    for (i = 0; i < len * 2; i++) pcm[i] = (int16_t)(-12000 + ((i % len) * 24000) / (len - 1));
    ok &= check_from("saw (loop)", pcm, len * 2, 0, 16, len);
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
