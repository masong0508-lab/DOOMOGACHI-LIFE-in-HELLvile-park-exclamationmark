/* instrument_bank.h - compact hybrid instrument bank for the PlayStation SPU.
 *
 * The PS1 does not read desktop SF2 files. This bank builds short, loop-clean
 * harmonic samples at boot, encodes them with the project's SPU ADPCM encoder,
 * and uploads them to the existing 0x1000-0x1fff waveform area. It deliberately
 * reuses the six stable core waveform IDs so the GBA / Dreamcast data formats,
 * songs and saves do not change.
 *
 * PS1 patch map:
 *   WAVE_PULSE12  glass / celesta bell
 *   WAVE_PULSE25  tine electric piano
 *   WAVE_PULSE50  warm drawbar organ
 *   WAVE_TRIANGLE round bass / mellow flute
 *   WAVE_SAW      dark string / analog pad
 *   WAVE_NOISE    existing noise percussion
 *
 * Integer-only, boot-time synthesis. Every partial is an integer harmonic so
 * the cycle closes exactly; partials above each zone's Nyquist are discarded.
 * The existing encoder adds the key-on fade and a loop encoded for SPU history.
 */
#ifndef PS1_INSTRUMENT_BANK_H
#define PS1_INSTRUMENT_BANK_H
#include <stdint.h>
#include "doomo/platform.h"
#include "waveform.h"

typedef struct Ps1Partial {
    uint8_t harmonic;
    uint16_t gain_q12;
} Ps1Partial;

typedef struct Ps1Patch {
    const char *name;
    const Ps1Partial *partials;
    uint16_t adsr1;             /* SPU attack / decay / sustain level */
    uint16_t adsr2;             /* SPU sustain hold / release */
    uint8_t count;
} Ps1Patch;

/* ADSR2 sustain-rate bits 12..6 set to 0x7f hold the chosen sustain level.
 * The low five bits select the release rate; bit 5 keeps it linear. */
#define PS1_ADSR_SUSTAIN_HOLD 0x1fc0u

/* A bell-like spectrum with strong upper partials, kept harmonic for a clean loop. */
static const Ps1Partial ps1_patch_bell[] = {
    { 1, 4096 }, { 2, 2500 }, { 3, 1500 }, { 4, 760 }, { 6, 460 }, { 8, 220 },
};
/* A bright tine / electric piano spectrum. */
static const Ps1Partial ps1_patch_ep[] = {
    { 1, 4096 }, { 2, 1900 }, { 3, 1050 }, { 4, 520 }, { 6, 260 }, { 8, 120 },
};
/* Drawbar-ish reed / organ: a firm fundamental and useful octave / fifth stops. */
static const Ps1Partial ps1_patch_organ[] = {
    { 1, 4096 }, { 2, 2450 }, { 3, 1500 }, { 4, 1050 }, { 5, 430 }, { 6, 330 },
};
/* Rounded low-register bass and breathy upper notes. */
static const Ps1Partial ps1_patch_bass[] = {
    { 1, 4096 }, { 2, 560 }, { 3, 180 }, { 4, 70 },
};
/* Dark bowed-string / analog pad with a slow spectral roll-off. */
static const Ps1Partial ps1_patch_pad[] = {
    { 1, 4096 }, { 2, 1750 }, { 3, 900 }, { 4, 520 }, { 5, 290 },
    { 6, 170 }, { 7, 90 }, { 8, 45 },
};

static const Ps1Patch ps1_patches[WAVE_NOISE] = {
    /* Fast bell attack, short exponential decay, low held tail. */
    { "Glass Bell", ps1_patch_bell, 0x0050u, PS1_ADSR_SUSTAIN_HOLD | 8u,
      (uint8_t)(sizeof(ps1_patch_bell) / sizeof(ps1_patch_bell[0])) },
    /* A little tine bloom, then a piano-like mid-level decay. */
    { "Tine Electric Piano", ps1_patch_ep, 0x0474u, PS1_ADSR_SUSTAIN_HOLD | 9u,
      (uint8_t)(sizeof(ps1_patch_ep) / sizeof(ps1_patch_ep[0])) },
    /* Slightly rounded attack, full sustain, very slow decay. */
    { "Warm Organ", ps1_patch_organ, 0x10ffu, PS1_ADSR_SUSTAIN_HOLD | 10u,
      (uint8_t)(sizeof(ps1_patch_organ) / sizeof(ps1_patch_organ[0])) },
    /* Firm bass onset and a steady, controlled sustain. */
    { "Round Bass", ps1_patch_bass, 0x0869u, PS1_ADSR_SUSTAIN_HOLD | 6u,
      (uint8_t)(sizeof(ps1_patch_bass) / sizeof(ps1_patch_bass[0])) },
    /* Slow exponential swell and a longer, soft release. */
    { "Dark String Pad", ps1_patch_pad, 0xbc0cu, PS1_ADSR_SUSTAIN_HOLD | 15u,
      (uint8_t)(sizeof(ps1_patch_pad) / sizeof(ps1_patch_pad[0])) },
};

static uint16_t ps1_instrument_adsr1(uint8_t wave)
{
    return wave < WAVE_NOISE ? ps1_patches[wave].adsr1 : 0x000fu;
}

static uint16_t ps1_instrument_adsr2(uint8_t wave)
{
    return wave < WAVE_NOISE ? ps1_patches[wave].adsr2 : (PS1_ADSR_SUSTAIN_HOLD | 8u);
}

/* Make one cycle for the patch. Returns 1 for a melodic patch, 0 for noise / invalid. */
static int ps1_instrument_cycle(uint8_t wave, int16_t *out, int len)
{
    const Ps1Patch *patch;
    int peak = 0, i, h;
    if (wave >= WAVE_NOISE || !out || len < 7 || len > PS1_WAVE_MAX_CYCLE ||
        (112 % len) != 0)
        return 0;
    patch = &ps1_patches[wave];
    /* Measure this exact band-limited cycle, then normalize its peak.  This
     * keeps every patch at a useful SPU level without clipping or making a
     * bright patch louder just because it has more partials. */
    for (i = 0; i < len; i++) {
        int sum = 0;
        for (h = 0; h < patch->count; h++) {
            int harmonic = patch->partials[h].harmonic;
            if (harmonic <= (len - 1) / 2) {
                int phase = (i * (112 / len) * harmonic) % 112;
                int s = ps1_wave_sin112(phase);
                sum += (s * (int)patch->partials[h].gain_q12 + 2048) >> 12;
            }
        }
        if (sum > peak) peak = sum;
        if (-sum > peak) peak = -sum;
    }
    if (peak < 1)
        return 0;
    for (i = 0; i < len; i++) {
        int sum = 0;
        for (h = 0; h < patch->count; h++) {
            int harmonic = patch->partials[h].harmonic;
            if (harmonic <= (len - 1) / 2) {
                int phase = (i * (112 / len) * harmonic) % 112;
                int s = ps1_wave_sin112(phase);
                sum += (s * (int)patch->partials[h].gain_q12 + 2048) >> 12;
            }
        }
        out[i] = (int16_t)((sum * PS1_WAVE_AMP) / peak);
    }
    return 1;
}

#endif
