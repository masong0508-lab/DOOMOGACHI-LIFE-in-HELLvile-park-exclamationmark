/* host_stubs.c - what game.c, scene.c and audio.c would define, for the
 * hosts' test player only (owner: show, ENGINE lane 3). show.exe gets the
 * same from show/engine/show_game.c and show_audio_stub.c; the test links
 * neither, so it carries its own copies.
 */
#include "doomo/doomo.h"
#include "doomo/scene.h"

volatile uint32_t g_host_audio_calls;
#define CALLED() (g_host_audio_calls++)

void audio_init(void) { CALLED(); }
void audio_tick(void) { }
void audio_music(int song) { (void)song; CALLED(); }
void audio_sfx(int sfx) { (void)sfx; CALLED(); }
const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    (void)sfx;
    if (len) *len = 0;
    if (rate) *rate = 0;
    return 0;
}
void audio_voice_char(const uint8_t *voice, char c) { (void)voice; (void)c; }
void audio_music_mute(int frames) { (void)frames; CALLED(); }
void audio_music_duck(int gain_q8, int frames) { (void)gain_q8; (void)frames; CALLED(); }
void audio_tapestop(int frames) { (void)frames; CALLED(); }
void audio_music_resume_on_beat(void) { CALLED(); }
void audio_music_dual(int song_a, int song_b) { (void)song_a; (void)song_b; CALLED(); }
void audio_sfx_pitch(int sfx, int semis) { (void)sfx; (void)semis; CALLED(); }
void audio_sfx_synth(int sfx) { (void)sfx; CALLED(); }
const uint8_t *audio_voice_preset(int preset) { (void)preset; return 0; }

/* the scene entry points hcmd.c references (the test selects shots with
 * `scene <name> <arg>`: host_driver.c's host_select) */
int host_select(const char *name, int arg);
static const DoomoScene s_fake = { 0 };
void scene_set(const DoomoScene *s, int arg) { (void)s; (void)arg; }
void scene_push(const DoomoScene *s, int arg) { (void)s; (void)arg; }
const DoomoScene *scene_top(void) { return &s_fake; }
const DoomoScene *scene_by_name(const char *name) { return host_select(name, 0) ? &s_fake : 0; }
