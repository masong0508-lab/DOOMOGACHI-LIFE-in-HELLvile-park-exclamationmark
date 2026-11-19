/* tonepick_test.c - host test of the tone pitch / zone choice (ps1/src/tonepick.h).
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

/* the old two-zone rule: long cycle up to 1571 Hz, short above; a sounding note re-keyed on every change */
static int old_variant(uint32_t hz) { return hz * 112u > 176000u ? 1 : 0; }

static int reaches(uint32_t hz, int z)
{
    return ((uint64_t)hz * ps1_zone_cyc[z] * 4096u + 22050u) / 44100u <= PS1_PITCH_MAX;
}

int main(void)
{
    static const uint32_t cycs[3] = { 4, PS1_CYC_SHORT, PS1_CYC_LONG };   /* noise, short, long */
    uint32_t hz, i, last;
    int cur, z, n, rekeys_old, rekeys_new, v, vold;

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
    CHECK((uint32_t)(40000u * 28u * 4096u) / 44100u != ref_pitch(40000, 28));
    CHECK(ps1_tone_pitch(40000, 28) == PS1_PITCH_MAX);

    /* 2. the tables: cycles halve, edges rise, a note on the way down stays below where it came up,
     *    every zone's reach is the register's end for its cycle */
    for (z = 0; z < PS1_ZONES; z++) {
        CHECK(ps1_zone_cyc[z] == 112 >> z);
        CHECK(reaches(ps1_zone_reach[z], z) && !reaches(ps1_zone_reach[z] * 101u / 100u, z));
        if (z) {
            CHECK(ps1_zone_down[z] < ps1_zone_up[z - 1]);                /* hysteresis gap */
            CHECK(ps1_zone_up[z - 1] <= ps1_zone_reach[z - 1]);          /* a new note's zone reaches it */
            CHECK(ps1_zone_up[z - 1] > ps1_zone_up[z > 1 ? z - 2 : 0] || z == 1);
            /* 1820 is the register value a sounding note may fall to: pitch steps stay about a cent */
            CHECK(ref_pitch(ps1_zone_down[z], ps1_zone_cyc[z]) >= 1780);
        }
    }
    CHECK(ps1_zone_up[2] == PS1_LONG_LIMIT_HZ);                          /* the old 28 / 14 split */

    /* 3. a new note: always a zone that reaches it (to 25 kHz), the zone never falls as hz rises,
     *    and below 1571 Hz it is never longer than... the old long cycle's range is only refined */
    last = 0;
    for (hz = 1; hz <= 25136; hz++) {
        z = ps1_tone_variant(hz, 0, 0);
        CHECK(reaches(hz, z));
        CHECK((uint32_t)z >= last);
        CHECK(ps1_tone_variant(hz, 0, 3) == z && ps1_tone_variant(hz, 1, 99) == z);
        last = (uint32_t)z;
    }
    CHECK(ps1_tone_variant(100, 0, 0) == 0 && ps1_tone_variant(440, 0, 0) == 1 && ps1_tone_variant(1000, 0, 0) == 2 &&
          ps1_tone_variant(2000, 0, 0) == 3 && ps1_tone_variant(4000, 0, 0) == 4);
    /* old rule: short cycle (28) = anything above 1571 Hz; the new zones 3 and 4 are inside that range */
    for (hz = 1; hz <= 1571; hz++) CHECK(old_variant(hz) == 0 && ps1_tone_variant(hz, 0, 0) <= 2);
    for (hz = 1572; hz <= 6299; hz++) CHECK(old_variant(hz) == 1 && ps1_tone_variant(hz, 0, 0) >= 3);

    /* 4. a sounding note: it stays when the register reaches it and its steps are fine, and the zone
     *    it lands in always reaches it */
    for (cur = 0; cur < PS1_ZONES; cur++)
        for (hz = 1; hz <= 30000; hz++) {
            z = ps1_tone_variant(hz, 1, cur);
            CHECK(z == cur || !(hz >= ps1_zone_down[cur] && hz <= ps1_zone_reach[cur]));
            if (hz <= 25136) CHECK(reaches(hz, z));
        }

    /* 5. how often one sounding note would have to be keyed on again */
    /*    a slide down from 6000 Hz to 100 Hz: once per zone edge (4), the old rule once */
    cur = ps1_tone_variant(6000, 0, 0); rekeys_old = rekeys_new = 0; vold = old_variant(6000);
    for (hz = 6000; hz >= 100; hz--) {
        int o = old_variant(hz);
        v = ps1_tone_variant(hz, 1, cur);
        rekeys_old += o != vold; vold = o;
        rekeys_new += v != cur; cur = v;
    }
    printf("slide 6000 -> 100 Hz: re-keys old %d, new %d (zones %d)\n", rekeys_old, rekeys_new, PS1_ZONES);
    CHECK(rekeys_old == 1 && rekeys_new == PS1_ZONES - 1);
    /*    a fast arpeggio over every old and new edge: no key-on (each note is one update) */
    for (n = 0; n < PS1_ZONES - 1; n++) {
        uint32_t hi = ps1_zone_up[n] + 60u, lo = ps1_zone_down[n + 1] + 40u;   /* inside the hysteresis gap */
        cur = ps1_tone_variant(hi, 0, 0); rekeys_new = 0;
        for (i = 0; i < 100; i++) {
            v = ps1_tone_variant((i & 1) ? lo : hi, 1, cur);
            rekeys_new += v != cur; cur = v;
        }
        printf("arpeggio %u / %u Hz x100: re-keys %d\n", (unsigned)hi, (unsigned)lo, rekeys_new);
        CHECK(rekeys_new == 0);
    }
    /*    the old 1700 / 1400 Hz arpeggio: old 99 re-keys, new 0 */
    cur = ps1_tone_variant(1700, 0, 0); vold = 1; rekeys_old = rekeys_new = 0;
    for (i = 0; i < 100; i++) {
        uint32_t f = (i & 1) ? 1400u : 1700u;
        int o = old_variant(f);
        v = ps1_tone_variant(f, 1, cur);
        rekeys_old += o != vold; vold = o;
        rekeys_new += v != cur; cur = v;
    }
    printf("arpeggio 1700/1400 Hz x100: re-keys old %d, new %d\n", rekeys_old, rekeys_new);
    CHECK(rekeys_old == 99 && rekeys_new == 0);

    printf("tone pick: %s\n", fails ? "FAIL" : "ok");
    return fails ? 1 : 0;
}
