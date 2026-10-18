/* plat_weak_music.c - weak defaults of the music platform contract
 * (platform.h "music" block; owner: MU0; docs/design/music/SPEC.md sec.9.4).
 *
 * THE LEGACY BRIDGE. A platform that does not implement the music hooks yet
 * (all three targets at M1) gets these: tier AT_BRIDGE, no IRQ driver (the
 * core runs mus_drv_tick() 8 times per tick inside music_update(), with
 * 30 Hz control frames), and plat_voice() keeps the latest record of deck
 * A's voices 0-3 (lead, harmony, bass, kit). audio_seq.c turns those four
 * records into plat_tone() calls once per tick (mus_bridge_voice()), exactly
 * where the classic sequencer called it, so the game sounds as before.
 * GBA (MU2, M2) and PS1 (MU3, M3) override every function here.
 */
#include "doomo/doomo.h"
#include "doomo/music.h"

#define WEAK __attribute__((weak))

static DoomoVoice bridge[4];

WEAK void plat_audio_caps(DoomoAudioCaps *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->tier = AT_BRIDGE;
}

WEAK uint32_t plat_music_plan(const DoomoVoicePlan *p, int n)
{
    uint32_t mask = 0;
    int i;
    for (i = 0; p && i < n; i++)
        if (p[i].voice < 4) mask |= 1u << p[i].voice;
    return mask;
}

WEAK void plat_voice(int voice, const DoomoVoice *v)
{
    if (voice >= 0 && voice < 4 && v) bridge[voice] = *v;
}

/* the bridge's view of voice `ch` (0..3), NULL outside */
WEAK const DoomoVoice *mus_bridge_voice(int ch)
{
    return (ch >= 0 && ch < 4) ? &bridge[ch] : NULL;
}

WEAK void plat_audio_bus(int bus, uint8_t gain_q8) { (void)bus; (void)gain_q8; }
WEAK void plat_audio_fx(int fx, int a, int b) { (void)fx; (void)a; (void)b; }
WEAK void plat_music_stall(int kind) { (void)kind; }
WEAK int  plat_tape_begin(uint16_t rate, uint16_t samples) { (void)rate; (void)samples; return 0; }
WEAK void plat_tape_render(const DoomoVoiceChange *c, int n, uint16_t s_from, uint16_t s_to)
{ (void)c; (void)n; (void)s_from; (void)s_to; }
WEAK void plat_tape_play(int16_t speed_q8, uint16_t pos, uint8_t gain) { (void)speed_q8; (void)pos; (void)gain; }
WEAK void plat_tape_end(void) { }
WEAK void plat_sample_ex(int sfx, uint8_t vol, int8_t pan, uint8_t prio, int8_t semis)
{
    (void)pan; (void)prio; (void)semis;
    plat_sample(sfx, vol);
}
