/* sfxref.c - the AT10.3 reference: the game's own SFX through the native
 * platform's musrender (`runner --wav`, 22,050 Hz), with nothing re-implemented.
 *
 * Compiled by showtk.sfx.gamesfx.build_ref() into the scratch work dir from
 * UNCHANGED game sources: core/src/audio_fx.c (the SynthSeg comedy kit and
 * audio_fx_synth_step), core/native/plat_music_native.c (nat_wav_*: the 22,050 Hz
 * PSG model `runner --wav` records with) and core/gen/sfx_data.c (sfxgen.py's
 * PCM). pitch_hz.inc is the note_hz64 table and pitch_hz() copied verbatim, at
 * build time, out of core/src/audio_seq.c (audio_int_hz's body). The stubs below
 * stand in for the rest of the game exactly where the runner's would: options
 * at their defaults (OPT_SFX_VOL 10), no music under the one-shot.
 *
 *   sfxref synth <index 0..14> <out.wav> [semis]   a SynthSeg one-shot (SFX_SYNTH_FIRST + i)
 *   sfxref pcm <SFX_* number> <out.wav>            a sfxgen.py PCM effect through plat_sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_int.h"
#include "plat_music_native.h"

#include "pitch_hz.inc"          /* static uint16_t pitch_hz(int32_t p16), from audio_seq.c */

/* ---- what audio_fx.c needs from the rest of the game ----------------------- */
int opt_get(int opt) { (void)opt; return 10; }
uint16_t audio_int_hz(int32_t p16) { return pitch_hz(p16); }
void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave) { nat_wav_tone(channel, freq_hz, vol, wave); }
void audio_int_release(int c) { plat_tone(c, 0, 0, WAVE_PULSE50); }   /* no song: silence (audio_seq.c) */
void audio_int_dual(int a, int b) { (void)a; (void)b; }
void audio_sfx(int sfx) { (void)sfx; }

const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    if (sfx <= SFX_NONE || sfx >= SFX_COUNT) return NULL;
    *len = doomo_sfx_info[sfx].len;
    *rate = doomo_sfx_info[sfx].rate;
    return doomo_sfx_pcm_data + doomo_sfx_info[sfx].offset;
}

int main(int argc, char **argv)
{
    int i, frames = 60;
    if (argc < 4) { fprintf(stderr, "usage: sfxref synth|pcm <id> <out.wav> [semis]\n"); return 2; }
    if (!nat_wav_open(argv[3])) { fprintf(stderr, "sfxref: cannot write %s\n", argv[3]); return 1; }
    audio_fx_reset();
    if (strcmp(argv[1], "synth") == 0) {
        int id = SFX_SYNTH_FIRST + atoi(argv[2]);
        int semis = argc > 4 ? atoi(argv[4]) : 0;
        if (semis) audio_sfx_pitch(id, semis); else audio_sfx_synth(id);   /* frame 0 sounds now */
        frames = audio_sfx_synth_frames(id) + 4;
    } else {
        nat_wav_sample(atoi(argv[2]), 15);
        frames = 45;
    }
    for (i = 0; i < frames; i++) {
        nat_wav_frame();                 /* the frame's 735 samples, as the runner does */
        audio_fx_tick(0);                /* the next audio_tick                         */
    }
    nat_wav_close();
    return 0;
}
