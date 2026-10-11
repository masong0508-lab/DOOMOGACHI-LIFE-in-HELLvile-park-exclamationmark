/* song_player.c - the shared song player and song helpers (owner: studio).
 *
 * Plays a DoomoSong (song.h) on the four tone channels at 30 Hz:
 *   ch 0  lead track (track 0), plus a little vibrato on sung notes
 *   ch 1  harmony track (track 1); on choir lines (2+ singers) it doubles
 *         the sung melody a third / fifth / octave away (rotating through
 *         the sections present) with a chorus wobble that widens with the
 *         number of singers. The voice babble owns ch 1 while a blip
 *         sounds (audio.c) - we just step aside and resend afterwards.
 *   ch 2  bass track
 *   ch 3  drums: one swept noise / pitched blip per SONG_DRUM_*, 6 kits
 * Timing is exact: an integer accumulator in units of 1/(16*bpm) ticks,
 * a straight 16th = 7200 units (450 / bpm ticks), swing moves 450*w units
 * from the odd to the even step of each pair.
 *
 * Singing: each lead-track onset sings the next syllable of the current
 * lyric line with audio_voice_char(). The singer's voice is TUNED to the
 * note: the octave nearest the singer's natural pitch, then the PITCH and
 * ACCENT sliders are searched (through audio_voice_freq, so it follows
 * audio.c exactly) for the blip closest to the note, intonation off. The
 * singer's SPEED and QUALITY stay theirs. Choir lines rotate the lead
 * voice through the line's singers per syllable and per sustain retrigger.
 *
 * Cost per tick: at most a few steps (4 cells each), one envelope pass,
 * <= 4 plat_tone calls (cached), and at most one voice tuning search
 * (~130 audio_voice_freq calls) - bounded.
 */
#include "doomo/doomo.h"

DOOMO_STATIC_ASSERT(SONG_WAVES == (int)WAVE_COUNT, song_waves_match);
DOOMO_STATIC_ASSERT((int)SONG_KIT_CLASSIC == (int)WAVE_NOISE, kit_classic_is_noise);
DOOMO_STATIC_ASSERT(SONG_SINGERS <= 32, singers_fit_a_mask);
DOOMO_STATIC_ASSERT(SONG_LYRIC_LEN <= 64, lyric_offsets_fit);

/* audio.h functions without a weak stub in stubs.c: fallbacks so the
 * player links (and stays quiet) while audio.c has not landed. */
__attribute__((weak)) int audio_voice_busy(void) { return 0; }
__attribute__((weak)) int audio_music_current(void) { return SONG_NONE; }
__attribute__((weak)) uint16_t audio_voice_freq(const uint8_t *voice, char c)
{
    (void)c;
    return song_note_hz(45 + (voice ? voice[VOICE_PITCH] & 31 : 16) * 36 / 31);
}

/* ====================================================================== */
/* names                                                                    */
const char *const doomo_song_scale_names[SONG_SCALE_COUNT] = {
    "Major", "Minor", "Pentatonic", "Blues", "Harmonic", "Chromatic"
};
const char *const doomo_song_drum_names[SONG_DRUM_COUNT] = {
    "-", "Kick", "Snare", "Hat", "Open Hat", "Clap", "Tom", "Crash", "Cowbell"
};
const char *const doomo_song_kit_names[SONG_KIT_COUNT] = {
    "Toy Box", "Bone Rattle", "Boom 808", "Plush Pillow", "Hell Forge", "Classic"
};
const char *const doomo_song_wave_names[SONG_WAVES] = {
    "Tiny Pulse", "Pulse", "Square", "Flute", "Buzzy Saw", "Static"
};
const char *const doomo_song_key_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
const char *const doomo_choir_layout_names[CHOIR_LAYOUT_COUNT] = {
    "Risers", "Semicircle", "Sections", "Pyramid"
};
const char *const doomo_section_names[SECTION_COUNT] = {
    "Soprano", "Alto", "Tenor", "Bass"
};

/* ====================================================================== */
/* pitch helpers                                                            */
/* semitone sets of the scales, bit n = n semitones above the key */
static const uint16_t scale_mask[SONG_SCALE_COUNT] = {
    0x0AB5,   /* major          0 2 4 5 7 9 11 */
    0x05AD,   /* natural minor  0 2 3 5 7 8 10 */
    0x0295,   /* major pent.    0 2 4 7 9      */
    0x04E9,   /* blues          0 3 5 6 7 10   */
    0x09AD,   /* harmonic minor 0 2 3 5 7 8 11 */
    0x0FFF    /* chromatic                     */
};

/* C8..B8 (MIDI 108..119) in Hz */
static const uint16_t note_top[12] = { 4186, 4435, 4699, 4978, 5274, 5588, 5920,
                                       6272, 6645, 7040, 7459, 7902 };

uint16_t song_note_hz(int midi)
{
    int oct, sh;
    if (midi <= 0) return 0;
    if (midi > 119) midi = 119;
    oct = midi / 12;
    sh = 9 - oct;
    return (uint16_t)((note_top[midi % 12] + ((1 << sh) >> 1)) >> sh);
}

void song_note_name(int midi, char *buf)
{
    const char *k;
    int i = 0, oct;
    if (!buf) return;
    if (midi < 2 || midi > 127) { buf[0] = '-'; buf[1] = '-'; buf[2] = 0; return; }
    k = doomo_song_key_names[midi % 12];
    oct = midi / 12 - 1;
    while (*k) buf[i++] = *k++;
    if (oct < 0) { buf[i++] = '-'; oct = -oct; }
    buf[i++] = (char)('0' + (oct % 10));
    buf[i] = 0;
}

static int in_scale(const DoomoSong *s, int midi)
{
    int sc = s ? s->scale : SONG_SCALE_MAJOR, key = s ? s->key % 12 : 0;
    if (sc >= SONG_SCALE_COUNT) sc = SONG_SCALE_CHROMATIC;
    return (scale_mask[sc] >> ((midi - key + 120) % 12)) & 1;
}

int song_scale_snap(const DoomoSong *s, int midi)
{
    static const int8_t tries[5] = { 0, -1, 1, -2, 2 };
    int i;
    midi = DOOMO_CLAMP(midi, 2, 127);
    for (i = 0; i < 5; i++) {
        int m = midi + tries[i];
        if (m >= 2 && m <= 127 && in_scale(s, m)) return m;
    }
    return midi;
}

int song_scale_shift(const DoomoSong *s, int midi, int steps)
{
    int dir = steps < 0 ? -1 : 1, n = steps < 0 ? -steps : steps, k;
    midi = DOOMO_CLAMP(midi, 2, 127);
    for (; n > 0; n--) {
        int m = midi;
        for (k = 0; k < 12; k++) {
            m += dir;
            if (m < 2 || m > 127) return midi;
            if (in_scale(s, m)) break;
        }
        midi = m;
    }
    return midi;
}

/* ====================================================================== */
/* song helpers                                                             */
void song_clear(DoomoSong *s)
{
    int i;
    static const uint8_t def_wave[SONG_TRACKS] = { WAVE_PULSE50, WAVE_PULSE25, WAVE_TRIANGLE,
                                                   SONG_KIT_CLASSIC };
    static const uint8_t def_vol[SONG_TRACKS] = { 12, 8, 12, 10 };
    if (!s) return;
    memset(s, 0, sizeof(*s));
    memcpy(s->title, "New Song", 9);
    s->flags = SONGF_VALID | SONGF_CUSTOM;
    s->bpm = 120;
    s->scale = SONG_SCALE_MAJOR;
    memcpy(s->wave, def_wave, sizeof(def_wave));
    memcpy(s->vol, def_vol, sizeof(def_vol));
    s->order_len = 1;
    for (i = 0; i < SONG_SINGERS; i++) s->singer[i] = SONG_NO_SINGER;
    s->choir_layout = CHOIR_RISERS;
}

int song_singer_id(const DoomoSong *s, int i)
{
    int id;
    if (!s || i < 0 || i >= SONG_SINGERS) return -1;
    id = s->singer[i];
    if (id == SONG_NO_SINGER || id > DOOMO_PLAYER_ID) return -1;
    return id;
}

int song_singer_count(const DoomoSong *s)
{
    int i, n = 0;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(s, i) >= 0) n++;
    return n;
}

uint32_t song_line_mask(const DoomoSong *s, int line)
{
    uint32_t m, valid = 0;
    int i;
    if (!s || line < 0 || line >= SONG_LYRIC_LINES) return 0;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(s, i) >= 0) valid |= 1u << i;
    m = s->line_singers[line];
    if (!m) m = 1;
    return m & valid;
}

int song_section_of_voice(const uint8_t *voice)
{
    int p = voice ? voice[VOICE_PITCH] : 16;
    return p >= 22 ? SECTION_SOPRANO : p >= 15 ? SECTION_ALTO : p >= 8 ? SECTION_TENOR
                                                                       : SECTION_BASS;
}

static const uint8_t *voice_of(int id)
{
    static const uint8_t def_voice[VOICE_COUNT] = { 16, 14, 18, 12, 8 };
    const DoomoResident *r = id >= 0 ? town_res(id) : NULL;
    if (!r || (id < DOOMO_MAX_RESIDENTS && r->status == RES_EMPTY)) return def_voice;
    return r->voice;
}

int song_singer_section(const DoomoSong *s, int i)
{
    int v, id = song_singer_id(s, i);
    if (id < 0) return SECTION_ALTO;
    v = (s->section[i >> 1] >> ((i & 1) * 4)) & 15;
    if (v >= 1 && v <= SECTION_COUNT) return v - 1;
    return song_section_of_voice(voice_of(id));
}

void song_set_section(DoomoSong *s, int i, int section)
{
    int v = (section >= 0 && section < SECTION_COUNT) ? section + 1 : 0;
    if (!s || i < 0 || i >= SONG_SINGERS) return;
    s->section[i >> 1] = (uint8_t)((s->section[i >> 1] & ~(15 << ((i & 1) * 4))) |
                                   (v << ((i & 1) * 4)));
}

int song_lead_onsets(const DoomoSong *s, int pattern)
{
    int i, n = 0;
    if (!s || pattern < 0 || pattern >= SONG_PATTERNS) return 0;
    for (i = 0; i < SONG_STEPS; i++) if (s->notes[pattern][0][i] >= 2) n++;
    return n;
}

uint32_t song_length_ticks(const DoomoSong *s)
{
    uint32_t bpm, n;
    if (!s) return 0;
    bpm = DOOMO_CLAMP(s->bpm, 60, 220);
    n = DOOMO_CLAMP(s->order_len, 1, SONG_ORDER_MAX);
    /* a pattern = 16 step pairs of 14400 units, 16 * bpm units per tick */
    return (n * 14400u + bpm - 1) / bpm;
}

static int printable(char c) { return c >= 0x20 && c <= 0x7E; }

static int fix_text(char *t, int cap)
{
    int i, fixed = 0, end = 0;
    for (i = 0; i < cap; i++) {
        if (end) { if (t[i]) { t[i] = 0; fixed++; } continue; }
        if (!t[i]) { end = 1; continue; }
        if (!printable(t[i])) { t[i] = ' '; fixed++; }
    }
    if (t[cap - 1]) { t[cap - 1] = 0; fixed++; }
    return fixed;
}

#define FIX(cond, stmt) do { if (cond) { stmt; n++; } } while (0)

int song_sanitize(DoomoSong *s)
{
    int n = 0, p, t, i;
    if (!s) return 0;
    FIX(s->genre >= MUSIC_COUNT, s->genre = 0);
    FIX(s->bpm < 60, s->bpm = 60);
    FIX(s->bpm > 220, s->bpm = 220);
    FIX(s->key > 11, s->key = (uint8_t)(s->key % 12));
    FIX(s->scale >= SONG_SCALE_COUNT, s->scale = SONG_SCALE_MAJOR);
    FIX(s->swing > 8, s->swing = 8);
    for (t = 0; t < SONG_TRACKS; t++) {
        FIX(s->wave[t] >= SONG_WAVES, s->wave[t] = t == 3 ? SONG_KIT_CLASSIC : WAVE_PULSE50);
        FIX(s->vol[t] > 15, s->vol[t] = 15);
    }
    FIX(s->order_len < 1, s->order_len = 1);
    FIX(s->order_len > SONG_ORDER_MAX, s->order_len = SONG_ORDER_MAX);
    for (i = 0; i < SONG_ORDER_MAX; i++) FIX(s->order[i] >= SONG_PATTERNS, s->order[i] = 0);
    for (p = 0; p < SONG_PATTERNS; p++)
        for (t = 0; t < SONG_TRACKS; t++)
            for (i = 0; i < SONG_STEPS; i++) {
                uint8_t *c = &s->notes[p][t][i];
                if (t == 3) FIX(*c >= SONG_DRUM_COUNT, *c = 0);
                else FIX(*c > 127, *c = 0);
            }
    n += fix_text(s->title, SONG_TITLE_LEN);
    for (i = 0; i < SONG_LYRIC_LINES; i++) n += fix_text(s->lyrics[i], SONG_LYRIC_LEN);
    for (i = 0; i < SONG_SINGERS; i++)
        FIX(s->singer[i] != SONG_NO_SINGER && s->singer[i] > DOOMO_PLAYER_ID,
            s->singer[i] = SONG_NO_SINGER);
    for (i = 0; i < SONG_LYRIC_LINES; i++)
        FIX(s->line_singers[i] >> SONG_SINGERS, s->line_singers[i] &= (1u << SONG_SINGERS) - 1u);
    FIX(s->choir_layout >= CHOIR_LAYOUT_COUNT, s->choir_layout = CHOIR_RISERS);
    for (i = 0; i < SONG_SINGERS; i++) {
        int v = (s->section[i >> 1] >> ((i & 1) * 4)) & 15;
        FIX(v > SECTION_COUNT, song_set_section(s, i, -1));
    }
    if (SONG_SINGERS & 1) FIX(s->section[SONG_SINGERS >> 1] & 0xF0,
                              s->section[SONG_SINGERS >> 1] &= 0x0F);
    FIX(!(s->flags & SONGF_VALID), s->flags |= SONGF_VALID);
    return n;
}

/* ====================================================================== */
/* syllables                                                                */
static char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int is_alpha(char c) { c = lc(c); return c >= 'a' && c <= 'z'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_wordc(char c) { return is_alpha(c) || is_digit(c) || c == '\''; }
static int vowel_at(const char *w, int i, int ws)
{
    char c = lc(w[i]);
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return 1;
    return c == 'y' && i > ws;            /* y is a vowel except word-initially */
}
static int blend2(char a, char b)
{
    static const char *const bl = "blbrclcrdrflfrglgrplprscskslsmsnspsttrtwchshthphwh";
    int i;
    a = lc(a); b = lc(b);
    for (i = 0; bl[i]; i += 2) if (bl[i] == a && bl[i + 1] == b) return 1;
    return 0;
}

/* Two vowels that are sung apart: pi-a-no, li-on, ne-on, du-al (but not
 * na-tion, spe-cial, peo-ple, guard, quick). i is the second vowel. */
static int hiatus(const char *w, int i, int ws)
{
    char a = lc(w[i - 1]), b = lc(w[i]), p = i - 2 >= ws ? lc(w[i - 2]) : 0;
    if (a == 'i' && (b == 'a' || b == 'o' || b == 'u'))
        return p && p != 'c' && p != 't' && p != 's' && p != 'g' && p != 'x';
    if (a == 'e' && b == 'o') return p != 'p' && p != 'g';
    if (a == 'u' && (b == 'a' || b == 'o')) return p && p != 'q' && p != 'g';
    return 0;
}

/* Syllables of one word [ws, we): appends starts (the first `max`), returns
 * the new count. */
#define PUT(x) do { if (n < max) starts[n] = (uint8_t)(x); n++; } while (0)
static int word_syllables(const char *w, int ws, int we, uint8_t *starts, int n, int max)
{
    int gs[16], ge[16], ng = 0, i, k;
    if (is_digit(w[ws])) {
        for (i = ws; i < we; i++)
            if (is_digit(w[i])) PUT(i);
        return n;
    }
    for (i = ws; i < we && ng < 16;) {
        if (vowel_at(w, i, ws)) {
            gs[ng] = i;
            i++;
            while (i < we && vowel_at(w, i, ws) && !hiatus(w, i, ws)) i++;
            ge[ng++] = i;
        } else {
            i++;
        }
    }
    if (ng >= 2) {
        char last = lc(w[we - 1]);
        int gl = ng - 1, len = ge[gl] - gs[gl];
        /* silent final e (cake, fire) - but not consonant + "le" (bubble) */
        if (last == 'e' && len == 1 && gs[gl] == we - 1 && gs[gl] > ge[gl - 1]) {
            int le = lc(w[we - 2]) == 'l' && !vowel_at(w, we - 3, ws) && lc(w[we - 3]) != 'l';
            if (!le) ng--;
        } else if (len == 1 && gs[gl] == we - 2 && lc(w[gs[gl]]) == 'e' &&
                   gs[gl] - 1 >= ge[gl - 1]) {
            /* -es / -ed endings are usually silent (cakes, loved) */
            char end = lc(w[we - 1]), pre = lc(w[we - 3]);
            int cle = pre == 'l' && we - 4 >= ws && !vowel_at(w, we - 4, ws) && lc(w[we - 4]) != 'l';
            if (cle) { /* bubbles, candles, bubbled: the -le is sung */ }
            else if (end == 'd' && pre != 't' && pre != 'd') ng--;
            else if (end == 's' && pre != 's' && pre != 'x' && pre != 'z' && pre != 'c' &&
                     pre != 'g' && !(pre == 'h' && (lc(w[we - 4]) == 'c' || lc(w[we - 4]) == 's')))
                ng--;
        }
    }
    PUT(ws);
    if (ng == 0) return n;
    for (k = 1; k < ng; k++) {
        int a = ge[k - 1], b = gs[k], c = b - a, split;
        if (c <= 1) split = a;                                   /* ba|by    */
        else if (k == ng - 1 && lc(w[b]) == 'e' && b == we - 1 && lc(w[b - 1]) == 'l')
            split = b - 2 >= a ? b - 2 : a;                      /* ta|ble   */
        else if (c >= 3 && blend2(w[b - 2], w[b - 1])) split = b - 2;  /* mon|ster */
        else if (c == 2 && blend2(w[b - 2], w[b - 1]) &&
                 (lc(w[b - 1]) == 'h')) split = b - 2;          /* mo|ther  */
        else split = b - 1;                                      /* hap|py   */
        PUT(split);
    }
    return n;
}
#undef PUT

int song_syllables(const char *line, uint8_t *starts, int max)
{
    int i = 0, n = 0;
    if (!line) return 0;
    if (!starts) max = 0;
    while (line[i] && i < 255) {
        int ws, we;
        while (line[i] && !is_wordc(line[i])) i++;
        if (!line[i]) break;
        ws = i;
        while (line[i] && is_wordc(line[i])) i++;
        we = i;
        if (we - ws == 1 && line[ws] == '\'') continue;   /* a lone apostrophe */
        n = word_syllables(line, ws, we, starts, n, max);
    }
    return n;
}

/* ====================================================================== */
/* drum kits                                                                */
typedef struct {
    uint16_t f0, f1;     /* start / end frequency (Hz)                   */
    uint8_t vol;         /* 0..15                                         */
    uint8_t decay;       /* volume * 16 lost per tick                     */
    uint8_t sweep;       /* ticks of the f0 -> f1 sweep                   */
    uint8_t wave;        /* WAVE_NOISE, or a pulse = pitched (periodic)   */
} DrumDef;

#define N_ WAVE_NOISE
#define T_ WAVE_PULSE50
#define Q_ WAVE_PULSE25
#define E_ WAVE_PULSE12
static const DrumDef kits[SONG_KIT_COUNT][SONG_DRUM_COUNT] = {
    { /* TOY BOX: tiny, bright, plinky */
        { 0, 0, 0, 0, 0, 0 },
        { 900, 220, 13, 60, 2, T_ },     { 7000, 5000, 11, 48, 1, N_ },
        { 16000, 16000, 7, 96, 0, N_ },  { 15000, 13000, 7, 28, 0, N_ },
        { 5200, 4000, 10, 40, 1, N_ },   { 1600, 900, 11, 40, 3, E_ },
        { 12000, 9000, 10, 12, 6, N_ },  { 1300, 1250, 11, 44, 0, Q_ } },
    { /* BONE RATTLE: clacky wood blocks and xylobones */
        { 0, 0, 0, 0, 0, 0 },
        { 1200, 150, 15, 52, 2, N_ },    { 2400, 2200, 12, 60, 0, E_ },
        { 9000, 9000, 8, 96, 0, E_ },    { 7000, 6500, 8, 30, 0, E_ },
        { 3000, 2800, 12, 56, 0, Q_ },   { 1400, 700, 13, 44, 3, E_ },
        { 10000, 6000, 11, 10, 8, N_ },  { 1800, 1700, 12, 48, 0, E_ } },
    { /* BOOM 808: long sub kick, big clap */
        { 0, 0, 0, 0, 0, 0 },
        { 320, 48, 15, 18, 6, T_ },      { 4800, 2600, 13, 22, 3, N_ },
        { 15000, 15000, 7, 70, 0, N_ },  { 12000, 11000, 8, 10, 0, N_ },
        { 3400, 2400, 13, 20, 1, N_ },   { 600, 260, 13, 22, 6, T_ },
        { 10000, 6500, 12, 6, 10, N_ },  { 820, 800, 12, 26, 0, Q_ } },
    { /* PLUSH PILLOW: soft, round, quiet */
        { 0, 0, 0, 0, 0, 0 },
        { 1400, 100, 11, 44, 3, N_ },    { 4200, 2800, 9, 40, 2, N_ },
        { 13000, 13000, 5, 80, 0, N_ },  { 12000, 11000, 5, 20, 0, N_ },
        { 3000, 2400, 9, 40, 1, N_ },    { 700, 380, 10, 40, 4, T_ },
        { 9000, 7000, 8, 10, 8, N_ },    { 900, 880, 8, 40, 0, T_ } },
    { /* HELL FORGE: anvils, chains, metal */
        { 0, 0, 0, 0, 0, 0 },
        { 2200, 90, 15, 36, 3, N_ },     { 6400, 3600, 15, 26, 2, N_ },
        { 6000, 6000, 9, 70, 0, Q_ },    { 5200, 4800, 9, 12, 0, Q_ },
        { 3800, 3000, 14, 30, 1, N_ },   { 2100, 1100, 14, 30, 4, Q_ },
        { 12000, 5000, 14, 5, 12, N_ },  { 2600, 2500, 13, 22, 0, E_ } },
    { /* CLASSIC */
        { 0, 0, 0, 0, 0, 0 },
        { 1800, 120, 15, 40, 3, N_ },    { 5200, 3000, 13, 30, 2, N_ },
        { 16000, 16000, 8, 80, 0, N_ },  { 14000, 12000, 8, 14, 0, N_ },
        { 3600, 2600, 12, 34, 1, N_ },   { 1100, 520, 13, 36, 4, T_ },
        { 11000, 7000, 12, 7, 8, N_ },   { 800, 780, 11, 30, 0, Q_ } },
};
#undef N_
#undef T_
#undef Q_
#undef E_

/* ====================================================================== */
/* player state                                                             */
typedef struct { uint16_t freq; uint8_t vol, wave, valid; } Sent;

static struct {
    const DoomoSong *s;
    int16_t  over;            /* singer override (resident id) or -1        */
    int16_t  prev_music;      /* background song to restore                  */
    uint8_t  on, solo, solo_pat, paused_music;
    uint8_t  ord, step;
    uint32_t acc;             /* units into the current step                 */
    uint32_t ticks;
    uint8_t  beat_age;
    /* tone channels 0..2: sounding note (0 = off), age, onset volume */
    uint8_t  note[3], age[3], base[3], wave[3];
    /* drum channel */
    uint8_t  drum, dkit, dage;
    int16_t  dlevel;
    /* lyrics */
    int8_t   line;            /* lyric line of this pass, -1 = none          */
    int8_t   syl;             /* syllable sounding (index), -1 = none yet    */
    uint8_t  nsyl, next;
    uint8_t  starts[SONG_LYRIC_LEN];
    uint32_t mask;            /* singers of the line                         */
    uint8_t  rot;
    int8_t   lead;            /* choir slot of the lead voice                */
    uint8_t  singing;         /* a sung note is sounding                     */
    uint8_t  vowel;
    uint8_t  sing_age;
    uint8_t  harm;            /* choir doubling note on ch1 (0 = none)       */
    uint8_t  harm_vol;
    uint8_t  voice[VOICE_COUNT];
    /* preview */
    uint8_t  pv_ticks, pv_ch;
    Sent     sent[4];
} P;

static void send(int ch, int freq, int vol, int wave)
{
    Sent *c = &P.sent[ch];
    int mv = opt_get(OPT_MUSIC_VOL);
    vol = vol * mv / 7;
    vol = DOOMO_CLAMP(vol, 0, 15);
    if (freq <= 0 || vol <= 0) { freq = 0; vol = 0; }
    freq = DOOMO_MIN(freq, 20000);
    if (c->valid && c->freq == freq && c->vol == vol && c->wave == wave) return;
    plat_tone(ch, (uint16_t)freq, (uint8_t)vol, (uint8_t)wave);
    c->freq = (uint16_t)freq;
    c->vol = (uint8_t)vol;
    c->wave = (uint8_t)wave;
    c->valid = 1;
}

/* an onset that would send exactly what is playing: retrigger it */
static void retrigger(int ch, int freq, int vol, int wave)
{
    Sent *c = &P.sent[ch];
    int mv = opt_get(OPT_MUSIC_VOL), v = DOOMO_CLAMP(vol * mv / 7, 0, 15);
    if (ch == 1 && audio_voice_busy()) return;
    if (c->valid && c->freq == freq && c->vol == v && c->wave == wave && v) {
        plat_tone(ch, 0, 0, (uint8_t)wave);
        c->valid = 0;
    }
}

static void silence_all(void)
{
    int ch;
    for (ch = 0; ch < 4; ch++) {
        P.sent[ch].valid = 0;
        send(ch, 0, 0, ch == 3 ? WAVE_NOISE : WAVE_PULSE50);
    }
}

static int cur_pattern(void)
{
    const DoomoSong *s = P.s;
    if (P.solo) return P.solo_pat;
    return s->order[DOOMO_MIN(P.ord, SONG_ORDER_MAX - 1)] % SONG_PATTERNS;
}

static int order_len(void)
{
    return DOOMO_CLAMP(P.s->order_len, 1, SONG_ORDER_MAX);
}

static uint32_t step_units(int step)
{
    uint32_t sw = (uint32_t)DOOMO_MIN(P.s->swing, 8) * 450u;
    return (step & 1) ? 7200u - sw : 7200u + sw;
}

/* ---- singing ------------------------------------------------------------- */
static uint32_t hz_diff(int a, int b) { return (uint32_t)(a > b ? a - b : b - a); }

/* Tune a copy of voice `v` so its blip for `vowel` sounds note `midi` in
 * the octave nearest the voice's own pitch. */
static void tune_voice(const uint8_t *v, int midi, char vowel, int spread, uint8_t *out)
{
    uint8_t t[VOICE_COUNT];
    int nat = 45 + v[VOICE_PITCH] * 36 / 31, target = midi, p, a, bp = 0, ba = 0, hz;
    uint32_t best = 0xFFFFFFFFu, d;
    while (target < nat - 6) target += 12;
    while (target > nat + 6) target -= 12;
    target = DOOMO_CLAMP(target, 45, 81);
    hz = song_note_hz(target);
    memcpy(t, v, VOICE_COUNT);
    t[VOICE_INTONATION] = 0;
    t[VOICE_ACCENT] = 0;
    for (p = 0; p < 32; p++) {
        t[VOICE_PITCH] = (uint8_t)p;
        d = hz_diff(audio_voice_freq(t, vowel), hz);
        if (d < best) { best = d; bp = p; }
    }
    /* the accent bends the vowel: fine tuning between two pitch steps */
    best = 0xFFFFFFFFu;
    for (p = DOOMO_MAX(bp - 1, 0), a = bp; p <= DOOMO_MIN(bp + 1, 31); p++) {
        int k;
        t[VOICE_PITCH] = (uint8_t)p;
        for (k = 0; k < 32; k++) {
            t[VOICE_ACCENT] = (uint8_t)k;
            d = hz_diff(audio_voice_freq(t, vowel), hz);
            if (d < best) { best = d; a = p; ba = k; }
        }
    }
    memcpy(out, v, VOICE_COUNT);
    out[VOICE_PITCH] = (uint8_t)a;
    out[VOICE_ACCENT] = (uint8_t)DOOMO_CLAMP(ba + spread, 0, 31);
    out[VOICE_INTONATION] = 0;
}

static char syllable_vowel(int syl)
{
    const char *t = P.s->lyrics[P.line];
    int i = P.starts[syl], end = syl + 1 < P.nsyl ? P.starts[syl + 1] : SONG_LYRIC_LEN;
    char first = 0;
    for (; i < end && t[i]; i++) {
        char c = lc(t[i]);
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y') return c;
        if (!first && is_alpha(c)) first = c;
    }
    return first ? first : 'a';
}

static int popcount32(uint32_t v)
{
    int n = 0;
    while (v) { v &= v - 1; n++; }
    return n;
}

/* the singer (choir slot) after `from` in the line's mask */
static int next_singer(int from)
{
    int k;
    if (!P.mask) return -1;
    for (k = 1; k <= SONG_SINGERS; k++) {
        int i = (from + k + SONG_SINGERS) % SONG_SINGERS;
        if (P.mask & (1u << i)) return i;
    }
    return -1;
}

static void voice_blip(void)
{
    const uint8_t *v;
    int spread = 0;
    if (P.over >= 0) {
        v = voice_of(P.over);
    } else {
        int id;
        P.lead = (int8_t)next_singer(P.lead);
        id = song_singer_id(P.s, P.lead);
        if (id < 0) return;
        v = voice_of(id);
        /* a choir is never perfectly in unison */
        if (popcount32(P.mask) > 1) spread = (int)(P.rot % 3) - 1;
    }
    P.rot++;
    tune_voice(v, P.note[0], (char)P.vowel, spread, P.voice);
    audio_voice_char(P.voice, (char)P.vowel);
}

/* choir doubling for the sung note: an interval per section present */
static void choir_double(void)
{
    static const int8_t sec_steps[SECTION_COUNT] = { 2, -2, -4, -7 };
    int secs[SECTION_COUNT], ns = 0, i, n = popcount32(P.mask), sec;
    P.harm = 0;
    if (P.over >= 0 || n < 2) return;
    for (i = 0; i < SECTION_COUNT; i++) secs[i] = 0;
    for (i = 0; i < SONG_SINGERS; i++)
        if (P.mask & (1u << i)) secs[song_singer_section(P.s, i)] = 1;
    for (i = 0; i < SECTION_COUNT; i++) if (secs[i]) ns++;
    /* rotate through the sections present (unison choirs double an octave) */
    sec = -1;
    if (ns) {
        int k = P.rot % ns;
        for (i = 0; i < SECTION_COUNT; i++) if (secs[i] && k-- == 0) { sec = i; break; }
    }
    if (ns == 1 && sec == SECTION_SOPRANO) P.harm = (uint8_t)DOOMO_MIN(P.note[0] + 12, 119);
    else if (sec == SECTION_BASS) P.harm = (uint8_t)DOOMO_MAX(P.note[0] - 12, 24);
    else P.harm = (uint8_t)song_scale_shift(P.s, P.note[0], sec >= 0 ? sec_steps[sec] : -2);
    P.harm_vol = (uint8_t)DOOMO_MIN(15, 5 + n / 3 + (P.s->vol[1] > 8));
}

static void sing_onset(void)
{
    P.singing = 0;
    P.harm = 0;
    if (P.line < 0 || P.next >= P.nsyl) return;
    if (P.over < 0 && !P.mask) return;
    P.syl = (int8_t)P.next++;
    P.vowel = (uint8_t)syllable_vowel(P.syl);
    P.singing = 1;
    P.sing_age = 0;
    voice_blip();
    choir_double();
}

/* new pass of the order list (or of the solo pattern): its lyric line */
static void begin_line(void)
{
    const DoomoSong *s = P.s;
    int line = -1, k;
    if (P.solo) {
        for (k = 0; k < order_len(); k++)
            if (s->order[k] % SONG_PATTERNS == P.solo_pat) { line = SONG_LINE_OF(k); break; }
    } else {
        line = SONG_LINE_OF(P.ord);
    }
    P.line = (int8_t)line;
    P.syl = -1;
    P.next = 0;
    P.nsyl = 0;
    P.singing = 0;
    P.harm = 0;
    P.mask = 0;
    if (line < 0 || !s->lyrics[line][0]) { P.line = -1; return; }
    P.nsyl = (uint8_t)DOOMO_MIN(song_syllables(s->lyrics[line], P.starts, SONG_LYRIC_LEN),
                                SONG_LYRIC_LEN);
    P.mask = P.over >= 0 ? 1u : song_line_mask(s, line);
    if (P.over < 0 && P.mask && !(P.mask & (1u << DOOMO_MAX(P.lead, 0))))
        P.lead = -1;
}

/* ---- steps ------------------------------------------------------------- */
static void drum_on(int d)
{
    P.drum = (uint8_t)d;
    P.dage = 0;
    P.dkit = (uint8_t)(P.s->wave[3] < SONG_KIT_COUNT ? P.s->wave[3] : SONG_KIT_CLASSIC);
    P.dlevel = (int16_t)(kits[P.dkit][d].vol * 16 * DOOMO_MIN(P.s->vol[3] + 1, 16) / 16);
    P.sent[3].valid = 0;               /* always restart the hit */
}

static void do_step(void)
{
    const DoomoSong *s = P.s;
    int pat = cur_pattern(), t, v;
    if (P.step == 0) begin_line();
    if ((P.step & 3) == 0) P.beat_age = 0;
    for (t = 0; t < 3; t++) {
        v = s->notes[pat][t][P.step];
        if (v == SONG_REST) {
            P.note[t] = 0;
            if (t == 0) { P.singing = 0; P.harm = 0; }
        } else if (v >= 2) {
            P.note[t] = (uint8_t)v;
            P.age[t] = 0;
            P.base[t] = s->vol[t];
            P.wave[t] = (uint8_t)(s->wave[t] < SONG_WAVES ? s->wave[t] : WAVE_PULSE50);
            if (t == 0) {
                if (P.mask && P.over < 0) {
                    int n = popcount32(P.mask);
                    P.base[0] = (uint8_t)DOOMO_MIN(15, P.base[0] + (n >= 4) + (n >= 12));
                }
                retrigger(0, song_note_hz(v), P.base[0], P.wave[0]);
                sing_onset();
            } else {
                retrigger(t, song_note_hz(v), P.base[t], P.wave[t]);
            }
        }
    }
    v = s->notes[pat][3][P.step];
    if (v > 0 && v < SONG_DRUM_COUNT) drum_on(v);
}

/* advance to the next step; 0 = the song ended */
static int advance(void)
{
    P.step++;
    if (P.step < SONG_STEPS) return 1;
    P.step = 0;
    if (P.solo) return 1;
    P.ord++;
    if (P.ord < order_len()) return 1;
    if (!(P.s->flags & SONGF_LOOP)) return 0;
    P.ord = 0;
    return 1;
}

/* ---- output -------------------------------------------------------------- */
static int env(int base, int age)
{
    int drop = age < 3 ? 0 : (age - 3) / 5;
    if (drop > base / 3) drop = base / 3;
    return base - drop;
}

static void render_tones(void)
{
    int t;
    for (t = 0; t < 3; t++) {
        int f = 0, v = 0, w = P.wave[t];
        if (t == 1 && P.harm && P.singing) {
            int n = popcount32(P.mask);
            f = song_note_hz(P.harm);
            /* chorus: a wobble that widens with the number of singers */
            if (n >= 3) f += (P.ticks & 1) ? 1 + n / 6 : -(1 + n / 6);
            v = env(P.harm_vol, P.age[0]);
            w = P.s->wave[1] < SONG_WAVES ? P.s->wave[1] : WAVE_PULSE25;
        } else if (P.note[t]) {
            f = song_note_hz(P.note[t]);
            v = env(P.base[t], P.age[t]);
            /* a sung note gets vibrato once it is held */
            if (t == 0 && P.singing && P.age[0] > 6)
                f += ((P.ticks >> 1) & 1) ? f / 160 : -(f / 160);
        }
        if (P.note[t] && P.age[t] < 255) P.age[t]++;
        if (t == 1 && audio_voice_busy()) { P.sent[1].valid = 0; continue; }
        send(t, f, v, w);
    }
    if (P.drum) {
        const DrumDef *d = &kits[P.dkit][P.drum];
        int f = (P.dage >= d->sweep || !d->sweep) ? d->f1 :
                d->f0 + ((int)d->f1 - (int)d->f0) * P.dage / d->sweep;
        int v = (P.dlevel + 15) / 16;
        if (P.drum == SONG_DRUM_CLAP && P.dage == 2)
            P.dlevel = (int16_t)(d->vol * 14);      /* the second slap */
        send(3, f, v, d->wave);
        P.dlevel = (int16_t)(P.dlevel - d->decay);
        P.dage++;
        if (P.dlevel <= 0) { P.drum = 0; P.dlevel = 0; }
    } else {
        send(3, 0, 0, WAVE_NOISE);
    }
}

/* sustain: long sung notes keep babbling (choirs pass the line around) */
static void sustain(void)
{
    if (!P.singing) return;
    P.sing_age++;
    if (P.sing_age % 5 == 0 && P.sing_age < 40 && !audio_voice_busy()) {
        voice_blip();
        if (P.harm) choir_double();
    }
}

/* ====================================================================== */
/* public API                                                               */
static void start(const DoomoSong *s, int over, int solo, int pat, int ord, int step)
{
    int keep_music = P.on;
    if (!s) return;
    if (!keep_music) {
        P.prev_music = (int16_t)audio_music_current();
        if (P.prev_music != SONG_NONE) audio_music(SONG_NONE);
    }
    memset(P.note, 0, sizeof(P.note));
    memset(P.age, 0, sizeof(P.age));
    P.drum = 0;
    P.s = s;
    P.over = (int16_t)(over >= 0 && over <= DOOMO_PLAYER_ID ? over : -1);
    P.solo = (uint8_t)solo;
    P.solo_pat = (uint8_t)(pat & (SONG_PATTERNS - 1));
    P.on = 1;
    P.pv_ticks = 0;
    P.ticks = 0;
    P.acc = 0;
    P.rot = 0;
    P.lead = -1;
    P.beat_age = 0;
    P.ord = (uint8_t)DOOMO_CLAMP(ord, 0, order_len() - 1);
    P.step = (uint8_t)DOOMO_CLAMP(step, 0, SONG_STEPS - 1);
    silence_all();
    if (P.step) begin_line();             /* do_step only does it at step 0 */
    if (P.step && P.line >= 0) {
        /* skip the syllables of the onsets before the start step */
        int i, p = cur_pattern();
        for (i = 0; i < P.step; i++) if (s->notes[p][0][i] >= 2 && P.next < P.nsyl) P.next++;
    }
    do_step();
    render_tones();
}

void song_play_at(const DoomoSong *s, int singer_override, int order_pos, int step)
{
    start(s, singer_override, 0, 0, order_pos, step);
}

void song_play(const DoomoSong *s, int singer_override)
{
    start(s, singer_override, 0, 0, 0, 0);
}

void song_play_pattern(const DoomoSong *s, int pattern, int step)
{
    start(s, -1, 1, pattern, 0, step);
}

void song_stop(void)
{
    int was = P.on || P.pv_ticks;
    P.on = 0;
    P.pv_ticks = 0;
    P.singing = 0;
    P.harm = 0;
    P.syl = -1;
    P.line = -1;
    P.mask = 0;
    P.lead = -1;
    if (!was) return;
    silence_all();
    if (P.prev_music != SONG_NONE) audio_music(P.prev_music);
    P.prev_music = SONG_NONE;
}

int song_playing(void) { return P.on; }

void song_tick(void)
{
    int guard = 0;
    if (!P.on) {
        if (P.pv_ticks) {
            P.ticks++;
            if (--P.pv_ticks == 0) {
                P.note[0] = P.note[1] = P.note[2] = 0;
                P.drum = 0;
                P.sent[P.pv_ch].valid = 0;
                send(P.pv_ch, 0, 0, P.pv_ch == 3 ? WAVE_NOISE : WAVE_PULSE50);
            } else {
                render_tones();
            }
        }
        return;
    }
    P.ticks++;
    if (P.beat_age < 255) P.beat_age++;
    P.acc += 16u * DOOMO_CLAMP(P.s->bpm, 60, 220);
    while (P.acc >= step_units(P.step) && guard++ < 4) {
        P.acc -= step_units(P.step);
        if (!advance()) { song_stop(); return; }
        do_step();
    }
    sustain();
    render_tones();
}

void song_preview(const DoomoSong *s, int track, int cell)
{
    if (!s || P.on || track < 0 || track >= SONG_TRACKS) return;
    P.s = s;
    P.over = -1;
    P.singing = 0;
    P.harm = 0;
    P.note[0] = P.note[1] = P.note[2] = 0;
    P.drum = 0;
    P.pv_ch = (uint8_t)track;
    P.pv_ticks = (uint8_t)(track == 3 ? 10 : 8);
    if (track == 3) {
        if (cell > 0 && cell < SONG_DRUM_COUNT) drum_on(cell);
        else P.pv_ticks = 0;
    } else if (cell >= 2) {
        P.note[track] = (uint8_t)cell;
        P.age[track] = 0;
        P.base[track] = (uint8_t)DOOMO_MAX(s->vol[track], 6);
        P.wave[track] = (uint8_t)(s->wave[track] < SONG_WAVES ? s->wave[track] : WAVE_PULSE50);
        retrigger(track, song_note_hz(cell), P.base[track], P.wave[track]);
    } else {
        P.pv_ticks = 0;
    }
    if (P.pv_ticks) render_tones();
}

int song_pos_order(void) { return P.on ? P.ord : -1; }
int song_pos_step(void) { return P.on ? P.step : -1; }
int song_pos_line(void) { return P.on ? P.line : -1; }
int song_pos_syllable(void) { return P.on ? P.syl : -1; }
uint32_t song_pos_singers(void) { return (P.on && P.singing && P.over < 0) ? P.mask : 0; }
int song_pos_lead(void) { return (P.on && P.singing && P.over < 0) ? P.lead : -1; }
int song_beat_age(void) { return P.on ? P.beat_age : 255; }
uint32_t song_pos_ticks(void) { return P.on ? P.ticks : 0; }
