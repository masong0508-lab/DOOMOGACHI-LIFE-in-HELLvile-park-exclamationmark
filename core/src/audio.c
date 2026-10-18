/* audio.c - SFX playback, Tomodachi-style voice babble, the tone-channel
 * cache and audio_tick() (owner: MU1 = CS's "audio" team; docs/design/
 * music/SPEC.md D13, CF-30; originally story).
 *
 * AUD-SPLIT: the 30 Hz chiptune sequencer and the audio_music*() calls moved
 * verbatim to audio_seq.c. Everything that reaches plat_tone() from the
 * core's music and babble goes through send() here, which skips writes that
 * would not change anything audible.
 */
#include "audio_int.h"

static uint8_t inited;
static DoomoToneState sent[AUDIO_CH];
static uint8_t sent_valid[AUDIO_CH];

/* voice babble */
static struct {
    uint8_t ticks, vol, wave, syll;
    int32_t pitch, slide;
    uint32_t rng;
} vb;

static int clamp_i(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void send(int c, uint16_t f, uint8_t v, uint8_t w)
{
    if (v == 0) f = 0;
    if (f == 0) v = 0;
    if (sent_valid[c] && sent[c].freq == f && sent[c].vol == v && (v == 0 || sent[c].wave == w))
        return;
    sent[c].freq = f;
    sent[c].vol = v;
    sent[c].wave = w;
    sent_valid[c] = 1;
    plat_tone(c, f, v, w);
}

/* --- AUD-SPLIT: the channel cache, shared with audio_seq.c --- */
void audio_int_send(int c, uint16_t f, uint8_t v, uint8_t w) { send(c, f, v, w); }
void audio_int_unsend(int c) { if (c >= 0 && c < AUDIO_CH) sent_valid[c] = 0; }
int  audio_int_inited(void) { return inited; }
/* --- end AUD-SPLIT --- */

void audio_init(void)
{
    memset(&vb, 0, sizeof(vb));
    vb.rng = 0x9E3779B9u;
    memset(sent_valid, 0, sizeof(sent_valid));
    audio_seq_init();
    inited = 1;
    audio_fx_reset();                     /* --- S1 --- */
    audio_seq_reset();
}

static void voice_tick(void);

void audio_tick(void)
{
    if (!inited) return;
    voice_tick();
    audio_seq_tick();
}

/* ---- SFX ------------------------------------------------------------------- */
static const char *const sfx_names[SFX_COUNT] = {
    "none", "cursor", "select", "cancel", "error", "blip", "eat", "happy", "sad",
    "love", "sparkle", "splash", "poop", "egg crack", "evolve", "pistol",
    "shotgun", "chaingun", "rocket", "plasma", "bubble", "squeak", "chainsaw",
    "explode", "splat", "confetti", "demon roar", "demon pain", "demon die",
    "player hurt", "pickup", "door", "weld", "holy", "possess", "bell"
};

const char *audio_sfx_name(int sfx)
{
    return (sfx >= 0 && sfx < SFX_COUNT) ? sfx_names[sfx] : "";
}

uint32_t audio_sfx_total_bytes(void) { return doomo_sfx_pcm_total; }

const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    const DoomoSfxInfo *in;
    if (sfx <= SFX_NONE || sfx >= SFX_COUNT) {
        if (len) *len = 0;
        if (rate) *rate = 0;
        return NULL;
    }
    in = &doomo_sfx_info[sfx];
    if (len) *len = in->len;
    if (rate) *rate = in->rate;
    return in->len ? &doomo_sfx_pcm_data[in->offset] : NULL;
}

void audio_sfx(int sfx)
{
    int v;
    if (sfx >= SFX_SYNTH_FIRST && sfx < SFX_SYNTH_END) { audio_sfx_synth(sfx); return; }  /* --- S1 --- */
    if (sfx <= SFX_NONE || sfx >= SFX_COUNT) return;
    v = opt_get(OPT_SFX_VOL);
    if (v <= 0) return;
    v = (v * 15 + 5) / 10;
    plat_sample(sfx, (uint8_t)clamp_i(v, 1, 15));
}

/* ---- voice babble ---------------------------------------------------------- */
static uint32_t vrand(void)
{
    vb.rng ^= vb.rng << 13;
    vb.rng ^= vb.rng >> 17;
    vb.rng ^= vb.rng << 5;
    return vb.rng;
}

static int is_vowel(char c)
{
    c = (char)(c | 0x20);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

static int is_alpha(char c) { c = (char)(c | 0x20); return c >= 'a' && c <= 'z'; }

/* Base pitch (1/16 semitones) of a voice for one character. */
static int32_t voice_pitch(const uint8_t *voice, char c)
{
    int pitch = voice ? clamp_i(voice[VOICE_PITCH], 0, 31) : 16;
    int accent = voice ? clamp_i(voice[VOICE_ACCENT], 0, 31) : 8;
    int lc = (c | 0x20) - 'a';
    /* A2 (110 Hz) .. ~A5: three octaves over the slider */
    int32_t p = (2 * 12 + 9) * 16 + pitch * 36 * 16 / 31;
    if (is_alpha(c)) {
        /* each letter has its own little vowel-ish lilt, scaled by accent */
        static const int8_t shape[26] = { 3, -2, 1, -1, 4, 0, -3, 2, 6, -2, 1, -1, -4,
                                           0, -1, 2, -3, 1, 2, -2, -5, 3, -4, 0, 5, -1 };
        p += shape[lc] * (4 + accent) / 2;
    }
    return p;
}

uint16_t audio_voice_freq(const uint8_t *voice, char c) { return audio_int_hz(voice_pitch(voice, c)); }

int audio_voice_busy(void) { return vb.ticks != 0; }

void audio_voice_char(const uint8_t *voice, char c)
{
    int speed, quality, inton, vol;
    int32_t p;
    if (!inited) audio_init();
    if (!opt_get(OPT_VOICE_BABBLE)) return;
    vol = opt_get(OPT_VOICE_VOL);
    if (vol <= 0) return;
    if (!is_alpha(c) && !(c >= '0' && c <= '9') && c != '!' && c != '?') {
        vb.syll = 0;                       /* a pause: next letter speaks */
        return;
    }
    if (c == '!' || c == '?') {
        /* punctuation bends the running blip */
        if (vb.ticks) vb.slide = c == '?' ? 10 : -6;
        return;
    }
    /* consonants in the middle of a syllable only colour the blip */
    if (vb.ticks && !is_vowel(c) && vb.syll) {
        vb.pitch += (vrand() & 7) - 3;
        return;
    }
    if (vb.ticks && vb.syll && (vrand() & 1)) return;   /* keep it babbly, not machine-gun */
    speed = voice ? clamp_i(voice[VOICE_SPEED], 0, 31) : 16;
    quality = voice ? clamp_i(voice[VOICE_QUALITY], 0, 31) : 16;
    inton = voice ? clamp_i(voice[VOICE_INTONATION], 0, 31) : 16;
    p = voice_pitch(voice, c);
    /* intonation: each syllable wanders around the base pitch */
    p += (int32_t)((int)(vrand() % (unsigned)(inton * 3 + 1)) - inton * 3 / 2);
    /* sing-song voices rise and fall over a sentence */
    p += (vb.syll & 4 ? 1 : -1) * inton / 2;
    vb.pitch = p;
    vb.slide = (int32_t)(inton > 20 ? ((vrand() & 1) ? 3 : -3) : 0);
    vb.ticks = (uint8_t)(speed > 22 ? 1 : speed > 9 ? 2 : 3);
    vb.wave = (uint8_t)(quality < 8 ? WAVE_PULSE12 : quality < 18 ? WAVE_PULSE25 :
                        quality < 27 ? WAVE_PULSE50 : WAVE_TRIANGLE);
    vb.vol = (uint8_t)clamp_i((vol * 15 + 5) / 10 - (is_vowel(c) ? 0 : 2), 1, 15);
    vb.syll++;
    sent_valid[AUDIO_VOICE_CHANNEL] = 0;
    send(AUDIO_VOICE_CHANNEL, audio_int_hz(vb.pitch), vb.vol, vb.wave);
}

static void voice_tick(void)
{
    if (!vb.ticks) return;
    vb.ticks--;
    if (vb.ticks) {
        vb.pitch += vb.slide;
        send(AUDIO_VOICE_CHANNEL, audio_int_hz(vb.pitch), (uint8_t)DOOMO_MAX(1, vb.vol - 2), vb.wave);
        return;
    }
    /* the blip ended: give channel 1 back to the music (or silence) */
    sent_valid[AUDIO_VOICE_CHANNEL] = 0;
    if (audio_seq_song() == SONG_NONE) send(AUDIO_VOICE_CHANNEL, 0, 0, WAVE_PULSE50);
    else {
        DoomoToneState st;
        audio_channel_state(AUDIO_VOICE_CHANNEL, &st);
        send(AUDIO_VOICE_CHANNEL, st.freq, st.vol, st.wave);
    }
}
