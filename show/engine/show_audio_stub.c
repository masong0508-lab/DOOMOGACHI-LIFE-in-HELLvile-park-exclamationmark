/* show_audio_stub.c - the core audio API as strong no-ops (owner: show; spec 4.1).
 *
 * The PS1 never plays sound in a show: every sound is rendered on the
 * host at 48 kHz from audio_events.json (spec 10), and P4 proves each
 * event fired on its frame. audio.c / the sequencer are not linked, so
 * these strong definitions replace core/src/stubs.c's weak placeholders:
 * the harness's "core weak stubs hit" report stays empty (the probe hit
 * audio_sfx_pcm through spu.c's SFX upload). Calls are only counted.
 * Unreferenced ones cost nothing (--gc-sections).
 */
#include "doomo/doomo.h"

volatile uint32_t g_show_audio_calls;   /* how often the engine asked for sound */

#define CALLED() (g_show_audio_calls++)

void audio_init(void) { CALLED(); }
void audio_tick(void) { }
void audio_music(int song) { (void)song; CALLED(); }
void audio_sfx(int sfx) { (void)sfx; CALLED(); }

const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    (void)sfx;
    CALLED();
    if (len) *len = 0;          /* spu.c then marks the slot empty */
    if (rate) *rate = 0;
    return NULL;
}

void audio_voice_char(const uint8_t *voice, char c) { (void)voice; (void)c; }
void audio_music_mute(int frames) { (void)frames; CALLED(); }
void audio_music_duck(int gain_q8, int frames) { (void)gain_q8; (void)frames; CALLED(); }
void audio_tapestop(int frames) { (void)frames; CALLED(); }
void audio_music_resume_on_beat(void) { CALLED(); }
void audio_music_dual(int song_a, int song_b) { (void)song_a; (void)song_b; CALLED(); }
void audio_sfx_pitch(int sfx, int semis) { (void)sfx; (void)semis; CALLED(); }
void audio_sfx_synth(int sfx) { (void)sfx; CALLED(); }
const uint8_t *audio_voice_preset(int preset) { (void)preset; return NULL; }
