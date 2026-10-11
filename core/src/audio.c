/* audio.c - the 30 Hz chiptune sequencer, SFX playback and Tomodachi-style
 * voice babble (owner: story).
 *
 * Four channels are interpreted from MML strings (the dialect is described
 * in audio_int.h, the songs live in songs.c). Each tick a channel consumes
 * bytes until its next note starts in the future (bounded per tick), then
 * the playing note is shaped (envelope, vibrato, arpeggio, sweep) and sent
 * to plat_tone() - only when something audible changed.
 */
#include "audio_int.h"

/* ---- frequency table --------------------------------------------------- */
/* C4..C5 in Hz * 64 (13 entries: the last one for interpolation). */
static const uint16_t note_hz64[13] = {
    16744, 17740, 18795, 19912, 21096, 22351, 23680, 25088, 26580, 28160,
    29834, 31609, 33488
};

/* Pitch in 1/16 semitones above C0 -> Hz. */
static uint16_t pitch_hz(int32_t p16)
{
    int32_t semi, frac, oct, s;
    uint32_t a, b, f;
    if (p16 < 0) p16 = 0;
    if (p16 > 16 * 12 * 9 - 1) p16 = 16 * 12 * 9 - 1;
    semi = p16 >> 4;
    frac = p16 & 15;
    oct = semi / 12;
    s = semi % 12;
    a = note_hz64[s];
    b = note_hz64[s + 1];
    f = a + ((b - a) * (uint32_t)frac >> 4);
    f = (f << oct) >> 10;
    if (f < 20) f = 20;
    if (f > 16000) f = 16000;
    return (uint16_t)f;
}

static const int8_t arp_tab[ARP_COUNT][4] = {
    { 0, 0, 0, 0 }, { 0, 4, 7, 4 }, { 0, 3, 7, 3 }, { 0, 12, 0, 12 },
    { 0, 7, 12, 7 }, { 0, 5, 7, 5 }, { 0, 3, 6, 3 }, { 0, 4, 7, 11 },
    { 0, 2, 0, 2 }
};

/* ---- drums ---------------------------------------------------------------- */
enum { DR_NONE, DR_KICK, DR_SNARE, DR_HAT, DR_OPEN, DR_CRASH, DR_TOM, DR_CLAP, DR_COUNT };
typedef struct { uint16_t f0, f1; uint8_t vol, decay16, ticks_sweep; } Drum;
static const Drum drums[DR_COUNT] = {
    { 0, 0, 0, 0, 0 },
    { 1800, 120, 15, 40, 3 },     /* kick: a noisy thump falling fast     */
    { 5200, 3000, 13, 30, 2 },    /* snare                                */
    { 16000, 16000, 8, 80, 0 },   /* closed hat                           */
    { 14000, 12000, 8, 14, 0 },   /* open hat                             */
    { 11000, 7000, 12, 7, 8 },    /* crash                                */
    { 1100, 520, 13, 36, 4 },     /* tom                                  */
    { 3600, 2600, 12, 34, 1 },    /* clap                                 */
};

/* ---- state ------------------------------------------------------------------ */
typedef struct {
    const char *str;           /* MML text (NULL = silent channel)            */
    const char *p;             /* read pointer                                 */
    const char *loop_pt;       /* 'L' marker                                   */
    int32_t wait;              /* time left until the next event, 1/256 units  */
    uint16_t deflen;           /* default length in units                      */
    uint8_t octave, vol, inst, gate_q;
    int8_t transpose;
    uint8_t depth;
    struct { const char *pos; int8_t left; } loops[4];
    uint8_t ended;
    /* the sounding note */
    uint8_t on;                /* 1 note, 2 drum                               */
    uint8_t drum;
    uint8_t stage;             /* 0 attack 1 decay 2 sustain 3 release        */
    int16_t level;             /* 0..240                                       */
    int32_t pitch;             /* 1/16 semitones                               */
    int32_t sweep;             /* accumulated sweep                            */
    uint16_t t;                /* ticks since note on                          */
    uint16_t gate_t;           /* release after this many ticks                */
    DoomoToneState out;
} Chan;

static Chan ch[AUDIO_CH];
static int cur_song, prev_song;
static uint8_t once_mode;
static int32_t rate256 = 1000;   /* 1/256 duration units per tick              */
static uint32_t song_t, notes_started;
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

/* ---- MML parsing helpers ------------------------------------------------- */
static int is_skip(char c) { return c == ' ' || c == '|' || c == '\n' || c == '\t' || c == '\r'; }

static int read_num(const char **pp, int def)
{
    const char *p = *pp;
    int n = 0, any = 0;
    while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; any = 1; }
    *pp = p;
    return any ? n : def;
}

/* Length after a note / rest / drum: digits (a divisor of the whole note)
 * and dots. Returns units. */
static int read_len(const char **pp, int deflen)
{
    const char *p = *pp;
    int n = read_num(&p, -1), u, add;
    u = n > 0 ? AUDIO_WHOLE / n : deflen;
    if (u < 1) u = 1;
    add = u;
    while (*p == '.') { add /= 2; u += add; p++; }
    *pp = p;
    return u;
}

static int semitone_of(char c)
{
    switch (c) {
    case 'c': return 0; case 'd': return 2; case 'e': return 4; case 'f': return 5;
    case 'g': return 7; case 'a': return 9; case 'b': return 11;
    default: return -1;
    }
}

static int drum_of(char c)
{
    switch (c) {
    case 'K': return DR_KICK; case 'S': return DR_SNARE; case 'H': return DR_HAT;
    case 'O': return DR_OPEN; case 'C': return DR_CRASH; case 'T': return DR_TOM;
    case 'P': return DR_CLAP;
    default: return 0;
    }
}

static void set_tempo(int bpm)
{
    bpm = clamp_i(bpm, 30, 300);
    /* units per tick = bpm * 48 / 1800 (a quarter is 48 units) */
    rate256 = (int32_t)((bpm * 12288 + 900) / 1800);
}

static int note_ticks(int units)
{
    int t = (int)(((int32_t)units * 256 + rate256 / 2) / rate256);
    return t < 1 ? 1 : t;
}

/* ---- note on --------------------------------------------------------------- */
static void note_on(Chan *c, int key, int units)
{
    const AudioInst *in = &audio_insts[c->inst < audio_inst_count ? c->inst : 0];
    int g = c->gate_q ? c->gate_q : in->gate;
    int t = note_ticks(units);
    c->on = 1;
    c->drum = 0;
    c->pitch = (int32_t)key * 16;
    c->sweep = 0;
    c->t = 0;
    c->stage = in->attack ? 0 : 1;
    c->level = in->attack ? 0 : (int16_t)(in->level * 16);
    c->gate_t = (uint16_t)DOOMO_MAX(1, t * g / 8);
    notes_started++;
}

static void drum_on(Chan *c, int d, int units)
{
    (void)units;
    c->on = 2;
    c->drum = (uint8_t)d;
    c->t = 0;
    c->level = (int16_t)(drums[d].vol * 16);
    c->stage = 1;
    notes_started++;
}

static void extend_note(Chan *c, int units)
{
    const AudioInst *in = &audio_insts[c->inst < audio_inst_count ? c->inst : 0];
    int g = c->gate_q ? c->gate_q : in->gate;
    if (c->on == 1) c->gate_t = (uint16_t)DOOMO_MIN(60000, c->gate_t + note_ticks(units) * g / 8);
}

/* Consume events until the channel waits again. */
static void chan_advance(Chan *c)
{
    int guard = 256, passes = 0;
    while (c->wait <= 0 && !c->ended && guard-- > 0) {
        char x = *c->p;
        int k, units;
        if (x == 0) {
            if (once_mode) { c->ended = 1; c->on = 0; break; }
            if (++passes > 1) { c->ended = 1; break; }   /* nothing playable */
            c->p = c->loop_pt ? c->loop_pt : c->str;
            c->depth = 0;
            continue;
        }
        c->p++;
        if (is_skip(x)) continue;
        if ((k = semitone_of(x)) >= 0) {
            const char *p = c->p;
            if (*p == '+' || *p == '#') { k++; p++; }
            else if (*p == '-') { k--; p++; }
            units = read_len(&p, c->deflen);
            note_on(c, c->octave * 12 + k + c->transpose, units);
            while (*p == '^') {
                int more;
                p++;
                more = read_len(&p, c->deflen);
                units += more;
                extend_note(c, more);
            }
            c->p = p;
            c->wait += (int32_t)units * 256;
            passes = 0;
            continue;
        }
        if ((k = drum_of(x)) > 0) {
            units = read_len(&c->p, c->deflen);
            drum_on(c, k, units);
            c->wait += (int32_t)units * 256;
            passes = 0;
            continue;
        }
        switch (x) {
        case 'r':
            units = read_len(&c->p, c->deflen);
            if (c->on == 1 && c->stage < 3) c->stage = 3;
            c->wait += (int32_t)units * 256;
            passes = 0;
            break;
        case '^':          /* stray tie: extend the current note */
            units = read_len(&c->p, c->deflen);
            extend_note(c, units);
            c->wait += (int32_t)units * 256;
            break;
        case 't': set_tempo(read_num(&c->p, 120)); break;
        case 'o': c->octave = (uint8_t)clamp_i(read_num(&c->p, 4), 0, 8); break;
        case '<': if (c->octave > 0) c->octave--; break;
        case '>': if (c->octave < 8) c->octave++; break;
        case 'l': c->deflen = (uint16_t)read_len(&c->p, 48); break;
        case 'v': c->vol = (uint8_t)clamp_i(read_num(&c->p, 12), 0, 15); break;
        case '@': c->inst = (uint8_t)clamp_i(read_num(&c->p, 0), 0, audio_inst_count - 1); break;
        case 'q': c->gate_q = (uint8_t)clamp_i(read_num(&c->p, 0), 0, 8); break;
        case 'k': {
            int neg = 0;
            if (*c->p == '-') { neg = 1; c->p++; }
            c->transpose = (int8_t)clamp_i(neg ? -read_num(&c->p, 0) : read_num(&c->p, 0), -48, 48);
            break; }
        case 'L': c->loop_pt = c->p; break;
        case '[':
            if (c->depth < 4) {
                c->loops[c->depth].pos = c->p;
                c->loops[c->depth].left = -1;
                c->depth++;
            }
            break;
        case ']': {
            int n = read_num(&c->p, 2);
            if (c->depth > 0) {
                int i = c->depth - 1;
                if (c->loops[i].left < 0) c->loops[i].left = (int8_t)clamp_i(n - 1, 0, 100);
                if (c->loops[i].left > 0) { c->loops[i].left--; c->p = c->loops[i].pos; }
                else c->depth--;
            }
            break; }
        default: break;     /* unknown bytes are ignored at runtime */
        }
    }
}

/* Shape the sounding note for this tick. */
static void chan_shape(Chan *c, int idx)
{
    int master = opt_get(OPT_MUSIC_VOL);
    uint16_t f = 0;
    int v = 0, w = WAVE_PULSE50;
    if (c->on == 1) {
        const AudioInst *in = &audio_insts[c->inst < audio_inst_count ? c->inst : 0];
        int32_t p = c->pitch;
        int peak = in->level * 16, sus = in->sustain * 16;
        w = in->wave;
        if (c->t >= c->gate_t && c->stage < 3) c->stage = 3;
        switch (c->stage) {
        case 0:
            c->level = (int16_t)(c->level + peak / DOOMO_MAX(1, in->attack));
            if (c->level >= peak) { c->level = (int16_t)peak; c->stage = 1; }
            break;
        case 1:
            c->level = (int16_t)(c->level - in->decay);
            if (c->level <= sus) { c->level = (int16_t)sus; c->stage = 2; }
            break;
        case 2: break;
        default:
            c->level = (int16_t)(c->level - DOOMO_MAX(1, in->release));
            if (c->level <= 0) { c->level = 0; c->on = 0; }
            break;
        }
        if (in->vib_depth && c->t >= in->vib_delay) {
            int ph = (int)((c->t - in->vib_delay) * in->vib_speed);
            p += (int32_t)(((int64_t)fx_sin(ph * 16) * in->vib_depth) >> 16);
        }
        if (in->sweep) {
            c->sweep += in->sweep;
            p += c->sweep;
        }
        if (in->arp) p += arp_tab[in->arp][c->t & 3] * 16;
        f = pitch_hz(p);
        v = c->level;
    } else if (c->on == 2) {
        const Drum *d = &drums[c->drum];
        w = WAVE_NOISE;
        f = (c->t >= d->ticks_sweep || d->ticks_sweep == 0) ? d->f1 :
            (uint16_t)(d->f0 + ((int32_t)d->f1 - d->f0) * c->t / d->ticks_sweep);
        v = c->level;
        c->level = (int16_t)(c->level - d->decay16);
        if (c->level <= 0) { c->level = 0; c->on = 0; }
        if (c->drum == DR_CLAP && c->t == 1) c->level = (int16_t)(d->vol * 16);
    }
    if (c->on) c->t++;
    /* level (0..240) x channel volume (0..15) x master (0..10) -> 0..15 */
    v = v * c->vol * master;
    v = (v + 1200) / 2400;
    if (v > 15) v = 15;
    c->out.freq = v ? f : 0;
    c->out.vol = (uint8_t)v;
    c->out.wave = (uint8_t)w;
    if (idx == AUDIO_VOICE_CHANNEL && vb.ticks) return;   /* the voice owns it */
    send(idx, c->out.freq, c->out.vol, c->out.wave);
}

/* ---- public: music ------------------------------------------------------- */
static void start_song(int song)
{
    int i;
    const AudioSong *s;
    memset(ch, 0, sizeof(ch));
    cur_song = song;
    song_t = 0;
    notes_started = 0;
    set_tempo(120);
    if (song <= SONG_NONE || song >= SONG_COUNT) {
        cur_song = SONG_NONE;
        for (i = 0; i < AUDIO_CH; i++) {
            if (i == AUDIO_VOICE_CHANNEL && vb.ticks) continue;
            sent_valid[i] = 0;
            send(i, 0, 0, WAVE_PULSE50);
        }
        return;
    }
    s = &audio_songs[song];
    for (i = 0; i < AUDIO_CH; i++) {
        Chan *c = &ch[i];
        c->str = s->ch[i];
        c->p = c->str;
        c->ended = c->str == NULL;
        c->octave = 4;
        c->vol = 12;
        c->deflen = AUDIO_WHOLE / 8;
        c->inst = (uint8_t)(i == 3 ? 0 : i);
    }
    /* channel 0 usually carries the tempo: read it before anything plays */
    if (ch[0].str) {
        const char *p = ch[0].str;
        while (*p && is_skip(*p)) p++;
        if (*p == 't') { p++; set_tempo(read_num(&p, 120)); }
    }
}

void audio_init(void)
{
    memset(&vb, 0, sizeof(vb));
    vb.rng = 0x9E3779B9u;
    memset(sent_valid, 0, sizeof(sent_valid));
    prev_song = SONG_NONE;
    once_mode = 0;
    inited = 1;
    start_song(SONG_NONE);
}

void audio_music(int song)
{
    if (!inited) audio_init();
    if (song < 0 || song >= SONG_COUNT) song = SONG_NONE;
    if (song == cur_song && !once_mode) return;
    once_mode = 0;
    start_song(song);
}

void audio_music_once(int song)
{
    if (!inited) audio_init();
    if (song <= SONG_NONE || song >= SONG_COUNT) return;
    if (!once_mode) prev_song = cur_song;
    start_song(song);
    once_mode = 1;
}

int audio_music_current(void) { return once_mode ? prev_song : cur_song; }
uint32_t audio_music_ticks(void) { return song_t; }
uint32_t audio_note_count(void) { return notes_started; }

void audio_channel_state(int c, DoomoToneState *out)
{
    if (!out) return;
    if (c < 0 || c >= AUDIO_CH) { memset(out, 0, sizeof(*out)); return; }
    *out = ch[c].out;
}

static void voice_tick(void);

void audio_tick(void)
{
    int i, alive = 0;
    if (!inited) return;
    voice_tick();
    if (cur_song == SONG_NONE) return;
    song_t++;
    for (i = 0; i < AUDIO_CH; i++) {
        Chan *c = &ch[i];
        if (!c->ended) {
            c->wait -= rate256;
            chan_advance(c);
        }
        if (!c->ended || c->on) alive = 1;
        chan_shape(c, i);
    }
    if (!alive && once_mode) {
        int back = prev_song;
        once_mode = 0;
        start_song(back);
    }
}

/* ---- song measuring / validation ------------------------------------------ */
/* Total units of one pass of a channel (loops expanded). Returns -1 and sets
 * *err on a parse error. */
static int32_t measure(const char *s, const char **err)
{
    const char *p = s, *lstart[4];
    int32_t total = 0;
    int left[4], depth = 0, deflen = AUDIO_WHOLE / 8, guard = 200000;
    if (!s) return 0;
    while (*p && guard-- > 0) {
        char x = *p++;
        int k;
        if (is_skip(x)) continue;
        if (semitone_of(x) >= 0 || drum_of(x) || x == 'r') {
            if (semitone_of(x) >= 0 && (*p == '+' || *p == '#' || *p == '-')) p++;
            total += read_len(&p, deflen);
            while (semitone_of(x) >= 0 && *p == '^') { p++; total += read_len(&p, deflen); }
            continue;
        }
        switch (x) {
        case 't': case 'o': case 'v': case '@': case 'q':
            if (!(*p >= '0' && *p <= '9')) { if (err) *err = "command without a number"; return -1; }
            read_num(&p, 0);
            break;
        case 'k': if (*p == '-') p++; read_num(&p, 0); break;
        case 'l': deflen = read_len(&p, 48); break;
        case '<': case '>': case 'L': break;
        case '^': total += read_len(&p, deflen); break;
        case '[':
            if (depth >= 4) { if (err) *err = "loops nested too deep"; return -1; }
            lstart[depth] = p;
            left[depth] = -1;
            depth++;
            break;
        case ']':
            k = read_num(&p, 2);
            if (depth == 0) { if (err) *err = "']' without '['"; return -1; }
            if (left[depth - 1] < 0) left[depth - 1] = k - 1;
            if (left[depth - 1] > 0) { left[depth - 1]--; p = lstart[depth - 1]; }
            else depth--;
            break;
        default:
            if (err) *err = "unknown MML character";
            return -1;
        }
    }
    if (depth) { if (err) *err = "unclosed '['"; return -1; }
    return total;
}

static int song_tempo(const AudioSong *s)
{
    const char *p = s->ch[0];
    if (!p) return 120;
    while (*p && is_skip(*p)) p++;
    if (*p == 't') { p++; return read_num(&p, 120); }
    return 120;
}

int audio_song_check(int song, const char **err)
{
    const AudioSong *s;
    int32_t len = -1;
    int i, any = 0;
    if (song <= SONG_NONE || song >= SONG_COUNT) { if (err) *err = "no such song"; return 0; }
    s = &audio_songs[song];
    if (!s->name || !s->name[0]) { if (err) *err = "no name"; return 0; }
    for (i = 0; i < AUDIO_CH; i++) {
        int32_t l;
        if (!s->ch[i]) continue;
        l = measure(s->ch[i], err);
        if (l < 0) return 0;
        if (l == 0) continue;
        any = 1;
        if (len < 0) len = l;
        else if (l != len) { if (err) *err = "channel lengths differ"; return 0; }
    }
    if (!any) { if (err) *err = "song is empty"; return 0; }
    return 1;
}

uint32_t audio_song_length(int song)
{
    const AudioSong *s;
    int32_t len;
    int bpm, r;
    if (song <= SONG_NONE || song >= SONG_COUNT) return 0;
    s = &audio_songs[song];
    len = measure(s->ch[0] ? s->ch[0] : s->ch[3], NULL);
    if (len <= 0) return 0;
    bpm = clamp_i(song_tempo(s), 30, 300);
    r = (bpm * 12288 + 900) / 1800;
    return (uint32_t)(((int64_t)len * 256 + r - 1) / r);
}

const char *audio_song_name(int song)
{
    if (song <= SONG_NONE || song >= SONG_COUNT) return "Silence";
    return audio_songs[song].name;
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

uint16_t audio_voice_freq(const uint8_t *voice, char c) { return pitch_hz(voice_pitch(voice, c)); }

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
    send(AUDIO_VOICE_CHANNEL, pitch_hz(vb.pitch), vb.vol, vb.wave);
}

static void voice_tick(void)
{
    if (!vb.ticks) return;
    vb.ticks--;
    if (vb.ticks) {
        vb.pitch += vb.slide;
        send(AUDIO_VOICE_CHANNEL, pitch_hz(vb.pitch), (uint8_t)DOOMO_MAX(1, vb.vol - 2), vb.wave);
        return;
    }
    /* the blip ended: give channel 1 back to the music (or silence) */
    sent_valid[AUDIO_VOICE_CHANNEL] = 0;
    if (cur_song == SONG_NONE) send(AUDIO_VOICE_CHANNEL, 0, 0, WAVE_PULSE50);
    else send(AUDIO_VOICE_CHANNEL, ch[1].out.freq, ch[1].out.vol, ch[1].out.wave);
}
