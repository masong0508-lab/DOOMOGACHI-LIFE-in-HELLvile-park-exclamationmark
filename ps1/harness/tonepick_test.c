/* tonepick_test.c - host test of the tone pitch / cycle choice (ps1/src/tonepick.h).
 *   cc -O2 -I ps1/src ps1/harness/tonepick_test.c -o tonepick_test && ./tonepick_test */
#include <stdio.h>
#include "tonepick.h"
static int fails;
#define CHECK(c) do { if (!(c) && fails++ < 20) printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

/* the pitch the old code meant, without the 32 bit wrap */
static uint32_t ref_pitch(uint32_t hz, uint32_t cyc)
{
    uint64_t p = ((uint64_t)hz * cyc * 4096u + 22050u) / 44100u;
    return p > PS1_PITCH_MAX ? PS1_PITCH_MAX : (uint32_t)p;
}

/* the old rule: the cycle follows the pitch, a sounding note is keyed on again when it changes */
static int old_variant(uint32_t hz) { return hz * 112u > 176000u ? 1 : 0; }

int main(void)
{
    static const uint32_t cycs[3] = { 4, PS1_CYC_SHORT, PS1_CYC_LONG };   /* noise, short, long */
    uint32_t hz, i, last;
    int cur, rekeys_old, rekeys_new, v, vold;

    /* 1. pitch: equals the exact value for every hz, never past the register, never falls with hz */
    for (i = 0; i < 3; i++) {
        last = 0;
        for (hz = 0; hz <= 65535; hz++) {
            uint32_t p = ps1_tone_pitch(hz, cycs[i]);
            CHECK(p == ref_pitch(hz, cycs[i]));
            CHECK(p <= PS1_PITCH_MAX);
            CHECK(p >= last);
            last = p;
        }
    }
    /* the old 32 bit form really did wrap on the short cycle (what this fixes) */
    CHECK((uint32_t)(40000u * 28u * 4096u) / 44100u != ref_pitch(40000, 28));
    CHECK(ps1_tone_pitch(40000, 28) == PS1_PITCH_MAX);

    /* 2. a new note takes the cycle the old code took, whatever was playing before */
    for (hz = 0; hz <= 65535; hz++) {
        CHECK(ps1_tone_variant(hz, 0, 0) == old_variant(hz));
        CHECK(ps1_tone_variant(hz, 0, 1) == old_variant(hz));
        CHECK(ps1_tone_variant(hz, 1, 0) == old_variant(hz));
    }

    /* 3. the cycle always reaches the pitch: long only up to its register end, short up to 6299 Hz (6300 needs 16384) */
    for (hz = 1; hz <= 6299; hz++)
        for (cur = 0; cur < 2; cur++) {
            v = ps1_tone_variant(hz, 1, cur);
            CHECK(((uint64_t)hz * (v ? PS1_CYC_SHORT : PS1_CYC_LONG) * 4096u + 22050u) / 44100u <= PS1_PITCH_MAX);
        }

    /* 4. one sounding note: how often it would have to be keyed on again */
    /*    a slide down from 3000 Hz to 100 Hz */
    cur = 1; rekeys_old = rekeys_new = 0; vold = 1;
    for (hz = 3000; hz >= 100; hz--) {
        int o = old_variant(hz);
        v = ps1_tone_variant(hz, 1, cur);
        rekeys_old += o != vold; vold = o;
        rekeys_new += v != cur; cur = v;
    }
    printf("slide 3000 -> 100 Hz: re-keys old %d, new %d\n", rekeys_old, rekeys_new);
    CHECK(rekeys_old == 1 && rekeys_new == 1);
    /*    a fast arpeggio over the old boundary: 1700 / 1400 / 1700 ... (every note is one update) */
    cur = 1; vold = 1; rekeys_old = rekeys_new = 0;
    for (i = 0; i < 100; i++) {
        uint32_t f = (i & 1) ? 1400u : 1700u;
        int o = old_variant(f);
        v = ps1_tone_variant(f, 1, cur);
        rekeys_old += o != vold; vold = o;
        rekeys_new += v != cur; cur = v;
    }
    printf("arpeggio 1700/1400 Hz x100: re-keys old %d, new %d\n", rekeys_old, rekeys_new);
    CHECK(rekeys_old == 99 && rekeys_new == 0);
    /*    a vibrato across the new 700 Hz edge flips once (short -> long) and then stays on the long cycle */
    cur = 1; rekeys_new = 0;
    for (i = 0; i < 100; i++) {
        v = ps1_tone_variant(i & 1 ? 690u : 710u, 1, cur);
        rekeys_new += v != cur; cur = v;
    }
    printf("vibrato 690/710 Hz x100: re-keys new %d\n", rekeys_new);
    CHECK(rekeys_new == 1);

    printf("tone pick: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
