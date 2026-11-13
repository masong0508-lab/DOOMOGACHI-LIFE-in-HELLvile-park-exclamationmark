/* musvoice.c - the music engine's IRQ tier on the Dreamcast (DC5b; owner:
 * dreamcast): plat_audio_caps, plat_voice, plat_audio_bus and plat_audio_fx
 * on the software mixer (mixer.c). Portable C99 (no KOS): the host test
 * dc/tests/audio/test_tier links this very file.
 *
 * The caps say CAP_DRV_IRQ, so the core's music_update() only writes events
 * ahead into the driver's rings; the mixer calls mus_drv_tick() 8 times per
 * logic tick, right before it renders each mt's frames (dcsnd_set_driver,
 * audio_int.h). The driver answers with DoomoVoice records (plat_voice, on
 * change only), which become mixer voices here:
 *
 *   pitch16        the phase step, 1/16 semitone exact (the bridge rounded
 *                  to whole Hz: up to 30 cents off in the bass)
 *   vol_l / vol_r  0..63 per side, on top of the voice's place in the
 *                  stereo field (the core sends both alike today: its
 *                  driver stores MEV_PAN but does not apply it yet)
 *   env            not used: vol_l / vol_r already carry the soft envelope
 *   inst, tparam   the instrument's timbre: pulse with a duty of tparam / 32
 *                  (PULSE, PWM), saw (SAW, SYNC_SAW), triangle (TRI, TRI4,
 *                  SINE), noise (NOISE15, NOISE7); every other timbre is a
 *                  50 % pulse, as on the bridge (the bank has none today)
 *   kit hits       inst >= MUS_INST_KIT0: pitch16 is the hit, tparam the
 *                  frames since key-on; the classic kit's noise sweep
 *                  (mus_kit_classic), as the bridge plays it
 *   DVF_KEYON      a noise voice restarts (every hit sounds the same), a
 *                  silent pitched voice starts at phase 0
 *   DVF_CUT        silent at once
 *   DVF_NOISE7     the 7-bit noise
 *   DVF_KEYOFF, DVF_LEGATO, DVF_REVERSE_ENV, DVF_SWEEP_HW
 *                  hints for hardware envelopes: nothing to do, the soft
 *                  envelope is in the volume
 *   DVF_ECHO       ignored: no echo yet (below)
 *
 * S1's mute, duck and tape stop (audio_fx.c) are applied by the core only in
 * the bridge's flush, so under this tier dc_music_tick() asks the same
 * function (audio_fx_music) once per logic tick, before the first mt: a
 * probe with volume 256 gives the bus gain (0 mute / the duck gain / 255),
 * a probe at 60,000 Hz gives the tape stop's pitch in sixteenths of a
 * semitone. Both go to the mixer (dcsnd_music_gain, 255 = full;
 * dcsnd_music_tape), only when they change. plat_audio_bus(0, g) does the
 * same for a core that sends the gain itself. MFX_ECHO, MFX_REVERB,
 * MFX_FREEZE, MFX_CRACKLE, MFX_PSG_DUCK and MFX_STEMS are accepted and do
 * nothing: the core never sends the first three and defines no parameters
 * for them (docs/DREAMCAST.md section 6).
 *
 * latency_mt is 0, as on the bridge: music and effects share the one
 * stream, so the game's timing calls answer as they did.
 *
 * DC_MUSIC_IRQ=0 (make -C dc MUSIC_IRQ=0) builds none of this: the core's
 * weak functions then make the legacy bridge, as before DC5b.
 */
#include "doomo/doomo.h"
#include "doomo/music.h"
#include <math.h>
#include "../../core/src/mus_int.h"     /* MUS_INST, mus_kit_classic */
#include "../../core/src/audio_int.h"   /* audio_fx_music */
#include "audio_int.h"

/* where S1's bus gain and tape stop go (the host test taps them) */
#ifndef DC_TAPE_OUT
#define DC_TAPE_OUT dcsnd_music_tape
#else
void DC_TAPE_OUT(int down16);
#endif
#ifndef DC_BUS_OUT
#define DC_BUS_OUT dcsnd_music_gain
#else
void DC_BUS_OUT(uint8_t gain);
#endif
#ifndef DC_MUSIC_IRQ
#define DC_MUSIC_IRQ 1
#endif

#if DC_MUSIC_IRQ
void plat_audio_caps(DoomoAudioCaps *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->tier = AT_NATIVE;
    out->soft_voices = DCSND_MVOICES;
    out->flags = CAP_STEREO | CAP_DRV_IRQ;
    out->mix_rate = DCSND_RATE;
}

void plat_voice(int voice, const DoomoVoice *v)
{
    DcSndMV m;
    if (!v) return;
    /* the jukebox's meters read the classic tone (audio_channel_state) */
    if (voice >= 0 && voice < 4) mus_bridge_tone(v, &mus_bridge_tones[voice]);
    memset(&m, 0, sizeof(m));
    m.wave = DCMW_PULSE;
    m.duty = 16;
    m.keyon = (uint8_t)((v->flags & DVF_KEYON) != 0);
    if (v->inst >= MUS_INST_KIT0) {
        /* a kit voice is noise even while silent: its key-on still restarts the LFSR */
        const MusKitHit *d = &mus_kit_classic[v->pitch16 & 7];
        int t = v->tparam;
        m.wave = (uint8_t)((v->flags & DVF_NOISE7) ? DCMW_NOISE7 : DCMW_NOISE15);
        if (v->pitch16 && !(v->flags & DVF_CUT)) {
            m.hz = (t >= d->ticks_sweep || d->ticks_sweep == 0) ? d->f1 :
                   (uint16_t)(d->f0 + ((int32_t)d->f1 - d->f0) * t / d->ticks_sweep);
            if (m.hz) { m.vol_l = v->vol_l; m.vol_r = v->vol_r; }
        }
    } else {
        switch (MUS_INST(v->inst)->timbre) {
        case TIMBRE_PULSE: case TIMBRE_PWM:
            m.duty = (uint8_t)(v->tparam ? v->tparam : 4);   /* 0: the bridge's 12.5 % */
            break;
        case TIMBRE_SAW: case TIMBRE_SYNC_SAW: m.wave = DCMW_SAW; break;
        case TIMBRE_TRI: case TIMBRE_TRI4: case TIMBRE_SINE: m.wave = DCMW_TRI; break;
        case TIMBRE_NOISE15: m.wave = DCMW_NOISE15; break;
        case TIMBRE_NOISE7: m.wave = DCMW_NOISE7; break;
        default: break;
        }
        if ((v->flags & DVF_NOISE7) && m.wave == DCMW_NOISE15) m.wave = DCMW_NOISE7;
        if (v->pitch16 && !(v->flags & DVF_CUT)) {
            m.p16 = v->pitch16;
            m.vol_l = v->vol_l;
            m.vol_r = v->vol_r;
        }
    }
    dcsnd_mvoice(voice, &m);
}

void plat_audio_bus(int bus, uint8_t gain_q8)
{
    if (bus == 0) dcsnd_music_gain(gain_q8);
}

void plat_audio_fx(int fx, int a, int b)
{
    (void)fx; (void)a; (void)b;          /* see the header: nothing to do */
}

/* S1's state as the bridge's flush would apply it (see the header) */
static int sync_bus = 255, sync_tape, mt_k;
void dc_music_sync_reset(void) { sync_bus = 255; sync_tape = 0; mt_k = 0; }
static void music_sync(void)
{
    uint16_t f = 60000;
    int v = 256, bus, tape = 0;
    audio_fx_music(0, &f, &v);
    bus = v > 255 ? 255 : v;
    if (f && f < 60000) tape = (int)(192.0 * log2(60000.0 / f) + 0.5);
    if (bus != sync_bus) { sync_bus = bus; DC_BUS_OUT((uint8_t)bus); }
    if (tape != sync_tape) { sync_tape = tape; DC_TAPE_OUT(tape); }
}

/* the mixer's driver hook: 8 calls per slot, mt k before the k-th frames */
void dc_music_tick(void)
{
    if (mt_k == 0) music_sync();
    mt_k = (mt_k + 1) % DCSND_MT;
    mus_drv_tick();
}

void dc_music_init(void) { dc_music_sync_reset(); dcsnd_set_driver(dc_music_tick); }
#else
void dc_music_init(void) { dcsnd_set_driver(NULL); }
#endif
