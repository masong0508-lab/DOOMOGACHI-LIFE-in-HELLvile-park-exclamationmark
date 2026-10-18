/* audio_seq.c - audio_music*() on the music engine, the legacy bridge's
 * flush, and the classic 30 Hz interpreter as T1's reference (owner: MU1 =
 * CS's "audio" team; docs/design/music/SPEC.md D13, D15, sec.9.3, 18).
 *
 * M1. Every call keeps its signature and its timing (CF-18): audio_music()
 * starts the song's cue NOW (mus_bind_cue: its classic cue, imported by
 * tools/mmlc.py from the same MML), audio_music_once() plays it once and
 * restarts the song before it, audio_music_dual() puts one song's lead and
 * kit over the other's lead and bass. The engine (mus_*.c) writes events
 * into rings; the driver turns them into DoomoVoice records; on the weak
 * legacy bridge (all three targets at M1) this file turns voices 0-3 into
 * plat_tone() calls once per tick - after S1's mute / duck / tape-stop
 * gains, around the voice babble and the comedy kit - exactly where and how
 * the interpreter did. test_music_classic.c (T1) proves the result equals
 * the interpreter tick for tick on all 28 songs.
 *
 * The interpreter below (moved here by AUD-SPLIT) only runs as that
 * reference now: nothing in the game calls it, so the console links drop
 * it and songs.o with --gc-sections. M5 deletes it.
 */
#include "audio_int.h"
#include "mus_int.h"

/* ======================================================================
 * The classic interpreter (T1's reference; native tests only)
 * ====================================================================== */
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
    oct = (semi * 5462) >> 16;            /* = semi / 12 for semi < 108, no divide */
    s = semi - oct * 12;
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
static int cur_song;
static uint8_t once_mode;           /* always 0: the reference loops */
static int32_t rate256 = 1000;   /* 1/256 duration units per tick              */
static uint32_t notes_started;

static int clamp_i(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

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
    (void)idx;                            /* the reference: no gains, no plat_tone */
    c->out.freq = v ? f : 0;
    c->out.vol = (uint8_t)v;
    c->out.wave = (uint8_t)w;
}

/* the reference API (audio_int.h; test_music_classic.c) */
void audio_legacy_start(int song)
{
    int i;
    const AudioSong *s;
    memset(ch, 0, sizeof(ch));
    cur_song = song;
    notes_started = 0;
    once_mode = 0;
    set_tempo(120);
    if (song <= SONG_NONE || song >= SONG_COUNT) { cur_song = SONG_NONE; return; }
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

/* Song a on channels 0 and 2 (and its tempo), song b's lead on channel 1
 * and b's drums on channel 3 (the interpreter's audio_int_dual). */
void audio_legacy_dual(int song_a, int song_b)
{
    const AudioSong *b;
    const char *p;
    audio_legacy_start(song_a);
    if (cur_song == SONG_NONE || song_b <= SONG_NONE || song_b >= SONG_COUNT) return;
    b = &audio_songs[song_b];
    p = b->ch[0];
    if (p) {                                      /* skip b's tempo: a's wins */
        while (*p && is_skip(*p)) p++;
        if (*p == 't') { p++; read_num(&p, 120); }
    }
    ch[1].str = ch[1].p = p;
    ch[1].ended = p == NULL;
    ch[1].loop_pt = NULL;
    ch[3].str = ch[3].p = b->ch[3];
    ch[3].ended = b->ch[3] == NULL;
    ch[3].loop_pt = NULL;
}

void audio_legacy_tick(void)
{
    int i;
    if (cur_song == SONG_NONE) return;
    for (i = 0; i < AUDIO_CH; i++) {
        Chan *c = &ch[i];
        if (!c->ended) {
            c->wait -= rate256;
            chan_advance(c);
        }
        chan_shape(c, i);
    }
}

void audio_legacy_state(int c, DoomoToneState *out)
{
    if (!out) return;
    if (c < 0 || c >= AUDIO_CH) { memset(out, 0, sizeof(*out)); return; }
    *out = ch[c].out;
}

uint32_t audio_legacy_notes(void) { return notes_started; }

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

static int song_tempo(const AudioSong *s) __attribute__((unused));
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

/* ======================================================================
 * audio_music*() on the engine
 * ====================================================================== */
static struct {
    int song, prev;                /* SONG_* playing / to resume after a once     */
    uint8_t once;
    int32_t rate256;               /* the song's tempo law (the beat grid)        */
    uint32_t ticks, pos256;        /* ticks since the start; position, 1/256 units */
    uint32_t qnext;                /* the next quarter line, units                 */
    uint32_t notes0;               /* the driver's key-on count at the start     */
    DoomoToneState out[AUDIO_CH];  /* what the bridge last computed per channel  */
} E;

static int song_cue(int song) { return mus_bind_cue(song, -1, -1, 0); }

static void start_song(int song, int once)
{
    int i;
    MusCueView cv;
    E.song = song;
    E.ticks = 0;
    E.pos256 = 0;                         /* --- S1: the beat grid restarts --- */
    E.qnext = 48;
    audio_fx_song_started();
    memset(E.out, 0, sizeof(E.out));
    E.notes0 = mus_drv_keyons();
    E.rate256 = mus_rate256(120);
    if (song <= SONG_NONE || song >= SONG_COUNT) {
        E.song = SONG_NONE;
        mus_core_start(MC_NONE, 0);
        for (i = 0; i < AUDIO_CH; i++) {
            if (i == AUDIO_VOICE_CHANNEL && audio_voice_busy()) continue;
            if (audio_fx_owns(i)) continue;      /* --- S1 --- */
            audio_int_unsend(i);
            audio_int_send(i, 0, 0, WAVE_PULSE50);
        }
        return;
    }
    if (mus_bank_cue(song_cue(song), &cv)) E.rate256 = mus_rate256(cv.cue->bpm);
    mus_core_start(song_cue(song), once);
}

/* audio_init() (audio.c): the sequencer part */
void audio_seq_init(void)
{
    E.prev = SONG_NONE;
    E.once = 0;
    mus_core_init();
}

/* audio_init() (audio.c), after audio_fx_reset(): silence */
void audio_seq_reset(void)
{
    start_song(SONG_NONE, 0);
}

void audio_music(int song)
{
    if (!audio_int_inited()) audio_init();
    if (song < 0 || song >= SONG_COUNT) song = SONG_NONE;
    if (song == E.song && !E.once) return;
    E.once = 0;
    start_song(song, 0);
}

void audio_music_once(int song)
{
    if (!audio_int_inited()) audio_init();
    if (song <= SONG_NONE || song >= SONG_COUNT) return;
    if (!E.once) E.prev = E.song;
    start_song(song, 1);
    E.once = 1;
}

int audio_music_current(void) { return E.once ? E.prev : E.song; }
uint32_t audio_music_ticks(void) { return E.ticks; }
uint32_t audio_note_count(void) { return mus_drv_keyons() - E.notes0; }
int audio_seq_song(void) { return E.song; }

void audio_channel_state(int c, DoomoToneState *out)
{
    if (!out) return;
    if (c < 0 || c >= AUDIO_CH) { memset(out, 0, sizeof(*out)); return; }
    *out = E.out[c];
}

static uint8_t legacy_wave(const MusInst *in)
{
    switch (in->timbre) {
    case TIMBRE_PULSE: return (uint8_t)(in->tparam <= 4 ? WAVE_PULSE12 : in->tparam <= 8 ? WAVE_PULSE25 : WAVE_PULSE50);
    case TIMBRE_TRI: return WAVE_TRIANGLE;
    case TIMBRE_SAW: return WAVE_SAW;
    case TIMBRE_NOISE15: case TIMBRE_NOISE7: return WAVE_NOISE;
    default: return WAVE_PULSE50;
    }
}

/* 0..63 -> the classic 0..15 (the inverse of mus_vol63_of15[]) */
static const uint8_t vol15_of63[64] = {
    0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
    8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15
};

/* The legacy tone of one driver voice record: Hz (before S1's gains), the
 * classic 0..15 volume and the wave. The weak bridge (plat_weak_music.c)
 * calls it once per changed record; bridge_flush() then only applies the
 * gains each tick. */
void mus_bridge_tone(const DoomoVoice *dv, DoomoToneState *out)
{
    uint16_t f = 0;
    int v = 0, w = WAVE_PULSE50;
    if (dv->pitch16) {
        v = vol15_of63[dv->vol_l & 63];             /* the driver's exact 0..15 */
        if (dv->inst >= MUS_INST_KIT0) {
            const MusKitHit *d = &mus_kit_classic[dv->pitch16 & 7];
            int t = dv->tparam;
            w = WAVE_NOISE;
            f = (t >= d->ticks_sweep || d->ticks_sweep == 0) ? d->f1 :
                (uint16_t)(d->f0 + ((int32_t)d->f1 - d->f0) * t / d->ticks_sweep);
        } else {
            w = legacy_wave(MUS_INST(dv->inst));
            f = audio_int_hz(dv->pitch16);
        }
    }
    out->freq = f;
    out->vol = (uint8_t)v;
    out->wave = (uint8_t)w;
}

/* THE LEGACY BRIDGE, once per tick: voices 0-3 -> plat_tone, through S1's
 * gains and around the babble and the comedy kit, like the interpreter's
 * chan_shape() tail. */
static void bridge_flush(void)
{
    int c;
    for (c = 0; c < AUDIO_CH; c++) {
        const DoomoToneState *t = &mus_bridge_tones[c];
        uint16_t f = t->freq;
        int v = t->vol;
        DoomoToneState *o = &E.out[c];
        audio_fx_music(c, &f, &v);        /* --- S1: mute / duck / tape-stop gain + pitch --- */
        o->freq = v ? f : 0;
        o->vol = (uint8_t)v;
        o->wave = t->wave;
        if (c == AUDIO_VOICE_CHANNEL && audio_voice_busy()) continue;   /* the voice owns it */
        if (audio_fx_owns(c)) continue;   /* --- S1: a comedy one-shot borrowed it --- */
        audio_int_send(c, o->freq, o->vol, o->wave);
    }
}

/* audio_tick() (audio.c), after the voice babble: the music part */
void audio_seq_tick(void)
{
    {   /* --- S1: comic-timing hooks; a quarter note is 48 units --- */
        int quarter = E.song == SONG_NONE;
        if (!quarter) {                   /* a tick moves < 48 units: <= 1 new quarter */
            E.pos256 += (uint32_t)E.rate256;
            if ((E.pos256 >> 8) >= E.qnext) { E.qnext += 48; quarter = 1; }
        }
        audio_fx_tick(quarter);
    }   /* --- end S1 --- */
    music_update();
    if (E.song == SONG_NONE) return;
    E.ticks++;
    if (mus_core_bridge()) bridge_flush();
    if (mus_core_take_end() && E.once) {
        int back = E.prev;
        E.once = 0;
        start_song(back, 0);
    }
}

uint32_t audio_song_length(int song)
{
    if (song <= SONG_NONE || song >= SONG_COUNT) return 0;
    return music_cue_length(song_cue(song));
}

const char *audio_song_name(int song)
{
    const char *n;
    if (song <= SONG_NONE || song >= SONG_COUNT) return "Silence";
    n = music_cue_name(song_cue(song));
    return n[0] ? n : "Silence";
}

/* --- S1: comic-timing hooks (audio_fx.c; SPEC sec.6.3 "S1 first") ------------ */
uint16_t audio_int_hz(int32_t p16) { return pitch_hz(p16); }

void audio_int_release(int c)
{
    if (c < 0 || c >= AUDIO_CH) return;
    audio_int_unsend(c);
    if (c == AUDIO_VOICE_CHANNEL && audio_voice_busy()) return;
    if (E.song == SONG_NONE) audio_int_send(c, 0, 0, WAVE_PULSE50);
    else audio_int_send(c, E.out[c].freq, E.out[c].vol, E.out[c].wave);
}

/* Song a on channels 0 and 2 (and its tempo), song b's lead on channel 1
 * (the harmony pulse) and b's drums on channel 3. */
void audio_int_dual(int song_a, int song_b)
{
    if (!audio_int_inited()) audio_init();
    if (song_a <= SONG_NONE || song_a >= SONG_COUNT) { audio_music(SONG_NONE); return; }
    E.once = 0;
    start_song(song_a, 0);
    if (song_b <= SONG_NONE || song_b >= SONG_COUNT) return;
    mus_core_dual(song_cue(song_a), song_cue(song_b));
}
/* --- end S1 --- */
