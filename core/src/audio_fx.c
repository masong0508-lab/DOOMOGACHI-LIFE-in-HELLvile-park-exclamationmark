/* audio_fx.c - the comic-timing audio hooks and the comedy SFX kit
 * (audio.h; owner: S1, docs/design/story/SPEC.md sec.7 S1 M2, C11;
 * comic_timing.md sec.3.8, 4, 7.5; story_final.md sec.15.10).
 *
 * MUSIC. A global gain (Q8) and pitch multiplier (Q12) sit on top of the
 * sequencer, applied in audio.c's mixer (one marked hunk). The sequencer
 * keeps running underneath a mute, so the score re-enters on the grid
 * ("the vamp sneaks back in like nothing happened", CT sec.3.8):
 *   audio_music_mute(n)        gain 0 for n frames (n <= 0: unmute now)
 *   audio_music_duck(g, n)     gain g/256 for n frames (CT sec.4: 0.4-0.75)
 *   audio_tapestop(n)          pitch slides down 15 semitones over n
 *                              frames, then the music stays muted until a
 *                              new song, a resume or a timed mute
 *   audio_music_resume_on_beat() unmute on the next quarter-note downbeat
 *   audio_music_dual(a, b)     song a on channels 0 + 2, song b's lead on 1
 *                              and its drums on 3 (Ep 4.6), a's tempo
 * SFX never mute: only the music is gated (CT sec.4 rule 1).
 *
 * THE COMEDY KIT (C11). 15 synth one-shots, ids SFX_SYNTH_FIRST.., ported
 * from the trailer's recipes (trailer/tk.c tk_sfx) and squeezed to <= 12
 * frames each: short segment tables played on core tone channel 2 (tones)
 * and 3 (noise), which they borrow from the music the way the voice blip
 * borrows channel 1. 0 B of PCM; the tables are about 0.4 KB of ROM.
 * audio_sfx(id) routes these ids here. audio_sfx_pitch() shifts the tones
 * by semitones (the kazoo stepping down after each failure).
 *
 * GURGLE. audio_voice_preset(VOICE_GURGLE): the belly's bass babble
 * (lowest pitch, slow, round triangle, wobbly intonation).
 *
 * RAM: 16 B of .bss. Integer only; everything advances on audio_tick().
 */
#include "audio_int.h"

/* ---- music gain / pitch -------------------------------------------------- */
#define Q12 4096
/* 2^(-s/12) in Q12 for s = 0..15 semitones (the tape-stop slide) */
static const uint16_t semi_down_q12[16] = {
    4096, 3866, 3649, 3444, 3251, 3069, 2896, 2734,
    2580, 2435, 2299, 2170, 2048, 1933, 1825, 1722
};

enum { FX_HOLD_MUTE = 1, FX_RESUME = 2 };

static struct {
    int16_t mute;          /* frames of timed mute left                    */
    int16_t duck;          /* frames of duck left                          */
    uint8_t duck_gain;     /* Q8 (0..255; 256 = none, stored as 0 = none)  */
    uint8_t tape_n, tape_t;/* tape-stop length and progress (0 = off)      */
    uint8_t flags;         /* FX_*                                         */
    uint8_t live;          /* this tick: bit 0 timed mute, bit 1 duck      */
    /* the synth one-shot */
    uint8_t sfx, t;        /* recipe (0 = none), frame                     */
    int8_t  semis;
    uint8_t owns;          /* bit c = core channel c is borrowed           */
    uint8_t vol;           /* 0..15 after OPT_SFX_VOL                      */
} fx;

void audio_music_mute(int frames)
{
    if (frames <= 0) { fx.mute = 0; fx.flags &= (uint8_t)~FX_HOLD_MUTE; return; }
    fx.mute = (int16_t)DOOMO_MIN(frames, 30000);
}

void audio_music_duck(int gain_q8, int frames)
{
    if (frames <= 0 || gain_q8 >= 256) { fx.duck = 0; return; }
    fx.duck = (int16_t)DOOMO_MIN(frames, 30000);
    fx.duck_gain = (uint8_t)DOOMO_CLAMP(gain_q8, 0, 255);
}

void audio_tapestop(int frames)
{
    fx.tape_n = (uint8_t)DOOMO_CLAMP(frames, 1, 60);
    fx.tape_t = 1;
    fx.flags &= (uint8_t)~FX_RESUME;
}

void audio_music_resume_on_beat(void) { fx.flags |= FX_RESUME; }

void audio_music_dual(int song_a, int song_b)
{
    audio_int_dual(song_a, song_b);
}

/* audio.c: a new song started (start_song). A tape-stop's open-ended mute
 * ends there; timed mutes and ducks keep running (they are beat-timed). */
void audio_fx_song_started(void)
{
    fx.tape_t = 0;
    fx.flags &= (uint8_t)~FX_HOLD_MUTE;
}

/* audio.c mixer hunk: scale one channel's frequency and volume. */
void audio_fx_music(int c, uint16_t *freq, int *vol)
{
    (void)c;
    if ((fx.live & 1) || (fx.flags & FX_HOLD_MUTE)) { *vol = 0; return; }
    if (fx.live & 2) *vol = (*vol * fx.duck_gain + 128) >> 8;
    if (fx.tape_t && *freq) {
        int s16 = (int)fx.tape_t * 15 * 16 / fx.tape_n;      /* 1/16 semitones */
        int s = DOOMO_MIN(s16 >> 4, 14), fr = s16 & 15;
        int32_t q = semi_down_q12[s] + ((int32_t)(semi_down_q12[s + 1] - semi_down_q12[s]) * fr >> 4);
        uint32_t f = ((uint32_t)*freq * (uint32_t)q) >> 12;
        *freq = (uint16_t)(f < 20 ? 20 : f);
    }
}

int audio_fx_owns(int c) { return c >= 0 && c < AUDIO_CH && (fx.owns & (1 << c)); }

int audio_fx_music_gated(void) { return fx.mute > 0 || (fx.live & 1) || (fx.flags & FX_HOLD_MUTE) != 0; }

/* ---- the comedy kit ------------------------------------------------------ */
/* One segment: frames [t0, t0 + n) on a channel. Tones: a -> b are MIDI
 * notes (60 = middle C), slid linearly. Noise: a -> b are Hz / 64.
 * wc: wave (bits 0-2) | noise channel (bit 3) | chord arpeggio (bit 4:
 * 0, +4, +7 per frame) | decay to a quarter (bit 5). */
typedef struct SynthSeg { uint8_t t0, n, a, b, wc, vol; } SynthSeg;
#define NZ 8
#define ARP 16
#define DEC 32

static const SynthSeg S_SCRATCH[] = { { 0, 4, 47, 6, WAVE_NOISE | NZ, 11 }, { 4, 5, 6, 47, WAVE_NOISE | NZ, 11 } };
static const SynthSeg S_BOING[]   = { { 0, 7, 55, 79, WAVE_PULSE50, 10 }, { 7, 5, 79, 67, WAVE_PULSE50, 7 } };
static const SynthSeg S_POP2[]    = { { 0, 2, 70, 96, WAVE_TRIANGLE, 13 } };
static const SynthSeg S_THUD[]    = { { 0, 3, 28, 2, WAVE_NOISE | NZ, 12 }, { 0, 4, 40, 28, WAVE_TRIANGLE | DEC, 14 } };
static const SynthSeg S_WHOOSH[]  = { { 0, 12, 5, 47, WAVE_NOISE | NZ | DEC, 7 } };
static const SynthSeg S_KAZOO[]   = { { 0, 4, 67, 72, WAVE_SAW, 9 }, { 4, 8, 72, 60, WAVE_SAW, 9 } };
static const SynthSeg S_BEEP[]    = { { 0, 2, 88, 88, WAVE_PULSE50, 7 }, { 3, 2, 88, 88, WAVE_PULSE50, 7 } };
static const SynthSeg S_GLITCH[]  = { { 0, 1, 40, 40, WAVE_PULSE12, 8 }, { 1, 1, 77, 77, WAVE_PULSE12, 8 },
                                      { 2, 1, 64, 64, WAVE_PULSE12, 8 }, { 3, 1, 51, 51, WAVE_PULSE12, 8 },
                                      { 4, 1, 88, 88, WAVE_PULSE12, 8 }, { 5, 1, 75, 75, WAVE_PULSE12, 8 } };
static const SynthSeg S_TIK[]     = { { 0, 1, 91, 91, WAVE_PULSE50, 8 } };
static const SynthSeg S_SHWIP[]   = { { 0, 5, 96, 72, WAVE_TRIANGLE, 10 } };
static const SynthSeg S_TINK[]    = { { 0, 1, 100, 100, WAVE_TRIANGLE, 12 }, { 1, 4, 100, 100, WAVE_TRIANGLE | DEC, 7 } };
static const SynthSeg S_FANFARE[] = { { 0, 2, 72, 72, WAVE_PULSE25, 10 }, { 2, 2, 76, 76, WAVE_PULSE25, 10 },
                                      { 4, 2, 79, 79, WAVE_PULSE25, 10 }, { 6, 6, 84, 84, WAVE_PULSE25 | DEC, 11 } };
static const SynthSeg S_CHOIR[]   = { { 0, 12, 64, 64, WAVE_TRIANGLE | ARP, 9 } };
static const SynthSeg S_PLUCK[]   = { { 0, 1, 67, 67, WAVE_PULSE25, 12 }, { 1, 7, 67, 67, WAVE_PULSE25 | DEC, 9 } };
static const SynthSeg S_CREAK[]   = { { 0, 6, 40, 44, WAVE_SAW, 7 }, { 6, 6, 44, 39, WAVE_SAW | DEC, 7 } };

typedef struct SynthRecipe { const SynthSeg *seg; uint8_t n; const char *name; } SynthRecipe;
#define R(a, nm) { a, (uint8_t)(sizeof(a) / sizeof(a[0])), nm }
static const SynthRecipe recipes[SFX_SYNTH_COUNT] = {
    R(S_SCRATCH, "scratch"), R(S_BOING, "boing"), R(S_POP2, "pop"), R(S_THUD, "thud"),
    R(S_WHOOSH, "whoosh"), R(S_KAZOO, "kazoo"), R(S_BEEP, "beep"), R(S_GLITCH, "glitch"),
    R(S_TIK, "tik"), R(S_SHWIP, "shwip"), R(S_TINK, "tink"), R(S_FANFARE, "fanfare"),
    R(S_CHOIR, "choir"), R(S_PLUCK, "pluck"), R(S_CREAK, "creak")
};
#undef R

static int seg_ch(const SynthSeg *s) { return (s->wc & NZ) ? 3 : 2; }

/* frames a recipe lasts (tests: <= 12) */
int audio_sfx_synth_frames(int sfx)
{
    const SynthRecipe *r;
    int i, end = 0;
    if (sfx < SFX_SYNTH_FIRST || sfx >= SFX_SYNTH_END) return 0;
    r = &recipes[sfx - SFX_SYNTH_FIRST];
    for (i = 0; i < r->n; i++) end = DOOMO_MAX(end, r->seg[i].t0 + r->seg[i].n);
    return end;
}

const char *audio_sfx_synth_name(int sfx)
{
    if (sfx < SFX_SYNTH_FIRST || sfx >= SFX_SYNTH_END) return "";
    return recipes[sfx - SFX_SYNTH_FIRST].name;
}

int audio_sfx_synth_busy(void) { return fx.sfx != 0; }

static void synth_release(void)
{
    int c;
    uint8_t o = fx.owns;
    fx.owns = 0;
    fx.sfx = 0;
    for (c = 0; c < AUDIO_CH; c++)
        if (o & (1 << c)) audio_int_release(c);
}

static void synth_play(int sfx, int semis)
{
    int v = opt_get(OPT_SFX_VOL);
    if (sfx < SFX_SYNTH_FIRST || sfx >= SFX_SYNTH_END || v <= 0) return;
    if (fx.sfx) synth_release();
    fx.sfx = (uint8_t)(sfx - SFX_SYNTH_FIRST + 1);
    fx.t = 0;
    fx.semis = (int8_t)DOOMO_CLAMP(semis, -36, 36);
    fx.vol = (uint8_t)DOOMO_CLAMP((v * 15 + 5) / 10, 1, 15);
    audio_fx_synth_step();                 /* frame 0 sounds on this tick */
}

void audio_sfx_synth(int sfx) { synth_play(sfx, 0); }

void audio_sfx_pitch(int sfx, int semis)
{
    if (sfx >= SFX_SYNTH_FIRST && sfx < SFX_SYNTH_END) synth_play(sfx, semis);
    else audio_sfx(sfx);                   /* PCM: plat_sample has no pitch */
}

/* One frame of the running one-shot (called by audio_tick and on start). */
void audio_fx_synth_step(void)
{
    const SynthRecipe *r;
    int i, c, live = 0;
    uint16_t f[2] = { 0, 0 };
    uint8_t v[2] = { 0, 0 }, w[2] = { WAVE_TRIANGLE, WAVE_NOISE }, used = 0;
    if (!fx.sfx) return;
    r = &recipes[fx.sfx - 1];
    for (i = 0; i < r->n; i++) {
        const SynthSeg *s = &r->seg[i];
        int k = fx.t - s->t0, ch = seg_ch(s) - 2, lvl;
        used |= (uint8_t)(1 << ch);
        if (fx.t < s->t0 + s->n) live = 1;
        if (k < 0 || k >= s->n) continue;
        lvl = s->vol;
        if (s->wc & DEC) lvl -= (s->vol * 3 / 4) * k / DOOMO_MAX(1, s->n - 1);
        lvl = (lvl * fx.vol + 7) / 15;
        if (s->wc & NZ) {
            int hz = (s->a + (s->b - s->a) * k / s->n) * 64;
            f[ch] = (uint16_t)DOOMO_CLAMP(hz, 40, 16000);
        } else {
            int32_t p16 = ((int32_t)s->a * 16 + (int32_t)(s->b - s->a) * 16 * k / s->n) + fx.semis * 16;
            if (s->wc & ARP) { static const int8_t arp[3] = { 0, 4, 7 }; p16 += arp[fx.t % 3] * 16; }
            f[ch] = audio_int_hz(p16);
        }
        v[ch] = (uint8_t)DOOMO_CLAMP(lvl, 1, 15);
        w[ch] = (uint8_t)(s->wc & 7);
    }
    if (!live) { synth_release(); return; }
    for (c = 0; c < 2; c++) {
        if (!(used & (1 << c))) continue;
        fx.owns |= (uint8_t)(1 << (c + 2));
        plat_tone(c + 2, v[c] ? f[c] : 0, v[c], w[c]);
    }
    fx.t++;
}

/* audio_tick(): timers, the resume-on-beat check and the one-shot. */
void audio_fx_tick(int on_quarter)
{
    /* a mute or duck of n frames covers exactly the next n ticks */
    fx.live = (uint8_t)((fx.mute > 0 ? 1 : 0) | (fx.duck > 0 ? 2 : 0));
    if (fx.mute > 0) fx.mute--;
    if (fx.duck > 0) fx.duck--;
    if (fx.tape_t) {
        if (fx.tape_t >= fx.tape_n) { fx.tape_t = 0; fx.flags |= FX_HOLD_MUTE; }
        else fx.tape_t++;
    }
    if (fx.flags & FX_RESUME) {
        /* a quarter note is 48 units: resume on the tick a new one starts */
        if (on_quarter) {
            fx.flags &= (uint8_t)~(FX_RESUME | FX_HOLD_MUTE);
            fx.mute = 0;
            fx.live &= (uint8_t)~1;
            fx.tape_t = 0;
        }
    }
    audio_fx_synth_step();
}

void audio_fx_reset(void)
{
    if (fx.owns) synth_release();
    memset(&fx, 0, sizeof(fx));
}

/* ---- voice presets ----------------------------------------------------------- */
const uint8_t *audio_voice_preset(int preset)
{
    /* PITCH, SPEED, QUALITY, INTONATION, ACCENT (0..31) */
    static const uint8_t gurgle[VOICE_COUNT] = { 0, 4, 30, 27, 22 };
    return preset == VOICE_GURGLE ? gurgle : NULL;
}
