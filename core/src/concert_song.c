/* concert_song.c - the Concert Hall's procedural composer and its
 * 4-channel sequencer (owner: concert).
 *
 * A song is composed from (singer, genre, take): key, tempo, two chord
 * progressions (verse / chorus), a melody whose rhythm follows the
 * syllables of the lyrics, a bass line, a chord part and a drum pattern,
 * all in the genre's style. It plays directly on the platform's tone
 * channels (audio_music(SONG_NONE) first, the scene restores the music):
 *   ch0 lead (the singer: wave, octave, vibrato and articulation follow
 *       their voice sliders; audio_voice_char babbles each syllable)
 *   ch1 chords   ch2 bass   ch3 drums (noise)
 * Everything is integer, deterministic and a few hundred cycles a tick.
 */
#include "concert_int.h"

/* drum patterns: 16-step masks (bit i = step i) */
enum { DRUMS_NONE, DRUMS_POP, DRUMS_LOFI, DRUMS_CITY, DRUMS_CHIP, DRUMS_JAZZ,
       DRUMS_FOLK, DRUMS_DISCO, DRUMS_SHANTY, DRUMS_SYNTH, DRUMS_GOTH,
       DRUMS_PUNK, DRUMS_METAL, DRUMS_INDUSTRIAL, DRUMS_COUNT };
typedef struct { uint16_t kick, snare, hat, kick_chorus; uint8_t open_hat, metal_hat; } DrumPat;
static const DrumPat drum_pats[DRUMS_COUNT] = {
    { 0x0000, 0x0000, 0x0000, 0x0000, 0, 0 },     /* none (lullaby, hymn)   */
    { 0x0501, 0x1010, 0x5555, 0x0505, 0, 0 },     /* pop                    */
    { 0x0481, 0x1010, 0x5555, 0x0481, 0, 0 },     /* lo-fi lazy kick        */
    { 0x0901, 0x1010, 0xFFFF, 0x0909, 0, 0 },     /* city pop 16ths         */
    { 0x0101, 0x1010, 0x5555, 0x0505, 0, 0 },     /* chiptune               */
    { 0x0001, 0x8080, 0x9191, 0x0101, 0, 1 },     /* jazz swing ride        */
    { 0x0101, 0x1010, 0x0000, 0x0505, 0, 0 },     /* folk stomp             */
    { 0x1111, 0x1010, 0x4444, 0x1111, 1, 0 },     /* disco four on the floor*/
    { 0x0505, 0x1010, 0x0000, 0x0505, 0, 0 },     /* shanty boom-boom CLAP  */
    { 0x1111, 0x1010, 0x4444, 0x1111, 0, 0 },     /* synthwave              */
    { 0x0141, 0x1010, 0x5555, 0x0145, 0, 0 },     /* goth rock              */
    { 0x4141, 0x1010, 0x5555, 0x5555, 0, 0 },     /* punk                   */
    { 0x4949, 0x0100, 0x0101, 0xFFFF, 1, 1 },     /* doom metal (dbl kick)  */
    { 0x0909, 0x1010, 0xFFFF, 0x4949, 0, 1 },     /* industrial clank       */
};

#define G(bpm, sc, pent, root, lw, cw, bw, cs, bs, dr, crowd, show, edgy, c0, c1, c2, ad) \
    { bpm, sc, pent, root, lw, cw, bw, cs, bs, dr, crowd, show, edgy, c0, c1, c2, ad }
const ConcertGenre concert_genres[MUSIC_COUNT] = {
    /* Bubblegum Pop */
    G(128, SCALE_MAJOR, 0, 64, WAVE_PULSE25, WAVE_PULSE12, WAVE_TRIANGLE, CH_ARP, BS_OCTAVES,
      DRUMS_POP, CROWD_JUMP, SHOW_HEARTS, 0, PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_SKY, "Bubble pop!"),
    /* Lo-fi Beats */
    G(82, SCALE_DORIAN, 1, 62, WAVE_TRIANGLE, WAVE_PULSE50, WAVE_TRIANGLE, CH_PAD, BS_ROOTS,
      DRUMS_LOFI, CROWD_SWAY, SHOW_NOTES, 0, PAINT_LAVENDER, PAINT_PEACH, PAINT_TEAL, "Mmm... chill."),
    /* City Pop */
    G(108, SCALE_MAJOR, 0, 62, WAVE_PULSE50, WAVE_PULSE25, WAVE_SAW, CH_OFFBEAT, BS_WALK,
      DRUMS_CITY, CROWD_SWAY, SHOW_SKYLINE, 0, PAINT_MAGENTA, PAINT_CYAN, PAINT_SKY, "Midnight drive~"),
    /* Chiptune */
    G(140, SCALE_MAJOR, 0, 67, WAVE_PULSE12, WAVE_PULSE25, WAVE_TRIANGLE, CH_ARP, BS_EIGHTHS,
      DRUMS_CHIP, CROWD_JUMP, SHOW_PIXELS, 0, PAINT_LIME, PAINT_SKY, PAINT_MAGENTA, "Bleep bloop!"),
    /* Lullabies */
    G(72, SCALE_MAJOR, 1, 64, WAVE_TRIANGLE, WAVE_TRIANGLE, WAVE_TRIANGLE, CH_ALBERTI, BS_LONG,
      DRUMS_NONE, CROWD_SWAY, SHOW_STARS, 0, PAINT_SKY, PAINT_LEMON, PAINT_LAVENDER, "Hush now..."),
    /* Jazz */
    G(120, SCALE_DORIAN, 0, 62, WAVE_PULSE50, WAVE_PULSE25, WAVE_TRIANGLE, CH_STAB, BS_WALK,
      DRUMS_JAZZ, CROWD_BOB, SHOW_SPOTS, 0, PAINT_NAVY, PAINT_HONEY, PAINT_PEACH, "Doo-be-doo!"),
    /* Classical */
    G(92, SCALE_HARMONIC, 0, 65, WAVE_PULSE50, WAVE_TRIANGLE, WAVE_TRIANGLE, CH_ALBERTI, BS_LONG,
      DRUMS_NONE, CROWD_WAVE, SHOW_GLASS, 0, PAINT_CHERRY, PAINT_HONEY, PAINT_WHITE, "La la laaa!"),
    /* Folk */
    G(104, SCALE_MAJOR, 1, 62, WAVE_PULSE25, WAVE_PULSE50, WAVE_TRIANGLE, CH_STAB, BS_ROOTS,
      DRUMS_FOLK, CROWD_WAVE, SHOW_FIELDS, 0, PAINT_FOREST, PAINT_HONEY, PAINT_PEACH, "Hey ho!"),
    /* Disco */
    G(118, SCALE_DORIAN, 0, 62, WAVE_PULSE25, WAVE_PULSE12, WAVE_SAW, CH_OFFBEAT, BS_OCTAVES,
      DRUMS_DISCO, CROWD_JUMP, SHOW_DISCO, 0, PAINT_GRAPE, PAINT_HONEY, PAINT_MAGENTA, "Get down!"),
    /* Sea Shanties */
    G(100, SCALE_DORIAN, 1, 60, WAVE_PULSE50, WAVE_PULSE25, WAVE_TRIANGLE, CH_STAB, BS_ROOTS,
      DRUMS_SHANTY, CROWD_SWAY, SHOW_WAVES, 0, PAINT_NAVY, PAINT_TEAL, PAINT_WHITE, "Yo ho!"),
    /* Hymns */
    G(80, SCALE_MAJOR, 0, 62, WAVE_TRIANGLE, WAVE_PULSE50, WAVE_TRIANGLE, CH_PAD, BS_LONG,
      DRUMS_NONE, CROWD_WAVE, SHOW_GLASS, 0, PAINT_LEMON, PAINT_SKY, PAINT_WHITE, "Hallelujah!"),
    /* Synthwave */
    G(110, SCALE_MINOR, 0, 62, WAVE_SAW, WAVE_PULSE25, WAVE_SAW, CH_ARP, BS_EIGHTHS,
      DRUMS_SYNTH, CROWD_BOB, SHOW_GRID, 0, PAINT_MAGENTA, PAINT_CYAN, PAINT_GRAPE, "Neon nights!"),
    /* Goth Rock */
    G(126, SCALE_HARMONIC, 0, 60, WAVE_PULSE50, WAVE_PULSE25, WAVE_SAW, CH_ARP, BS_EIGHTHS,
      DRUMS_GOTH, CROWD_SWAY, SHOW_BATS, 1, PAINT_GRAPE, PAINT_ROSE, PAINT_LAVENDER, "Darkness, darling~"),
    /* Hardcore Punk */
    G(176, SCALE_MIXO, 0, 60, WAVE_PULSE12, WAVE_PULSE50, WAVE_SAW, CH_POWER, BS_EIGHTHS,
      DRUMS_PUNK, CROWD_MOSH, SHOW_SKULLS, 1, PAINT_LIME, PAINT_CHERRY, PAINT_LEMON, "OI! OI! OI!"),
    /* Doom Metal */
    G(96, SCALE_PHRYGIAN, 0, 57, WAVE_SAW, WAVE_PULSE12, WAVE_SAW, CH_POWER, BS_GALLOP,
      DRUMS_METAL, CROWD_HEADBANG, SHOW_FLAMES, 1, PAINT_HELLFIRE, PAINT_BLOOD, PAINT_TANGERINE, "BLEGH!"),
    /* Industrial */
    G(124, SCALE_PHRYGIAN, 0, 57, WAVE_PULSE12, WAVE_PULSE25, WAVE_SAW, CH_STAB, BS_EIGHTHS,
      DRUMS_INDUSTRIAL, CROWD_HEADBANG, SHOW_GEARS, 1, PAINT_RUST, PAINT_CHARCOAL, PAINT_SLIME, "CLANG CLANG!"),
};
#undef G

static const uint8_t scale_steps[SCALE_COUNT][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   /* major          */
    { 0, 2, 3, 5, 7, 8, 10 },   /* natural minor  */
    { 0, 2, 3, 5, 7, 9, 10 },   /* dorian         */
    { 0, 1, 3, 5, 7, 8, 10 },   /* phrygian       */
    { 0, 2, 3, 5, 7, 8, 11 },   /* harmonic minor */
    { 0, 2, 4, 5, 7, 9, 10 },   /* mixolydian     */
};

/* chord progressions (scale degree per bar) */
static const uint8_t progs[][4] = {
    { 0, 4, 5, 3 },   /*  0 I V vi IV       */
    { 5, 3, 0, 4 },   /*  1 vi IV I V       */
    { 0, 5, 3, 4 },   /*  2 I vi IV V       */
    { 0, 3, 4, 0 },   /*  3 I IV V I        */
    { 0, 3, 0, 4 },   /*  4 I IV I V        */
    { 1, 4, 0, 5 },   /*  5 ii V I vi       */
    { 0, 5, 2, 6 },   /*  6 i VI III VII    */
    { 0, 6, 5, 4 },   /*  7 i VII VI V      */
    { 0, 0, 5, 6 },   /*  8 i i VI VII      */
    { 0, 1, 0, 6 },   /*  9 i bII i VII     */
    { 0, 3, 5, 4 },   /* 10 I IV vi V       */
    { 0, 3, 6, 0 },   /* 11 i iv VII i      */
};
static const uint8_t prog_major[] = { 0, 1, 2, 3, 4, 10 };
static const uint8_t prog_minor[] = { 6, 7, 8, 11, 1 };
static const uint8_t prog_phryg[] = { 9, 8, 7 };
static const uint8_t prog_jazz[]  = { 5, 1, 2 };

/* melody rhythms: onset masks by onset count (3..10) */
static const uint16_t rhythm_bank[8][3] = {
    { 0x0111, 0x0141, 0x1101 },   /* 3  */
    { 0x1111, 0x0115, 0x1141 },   /* 4  */
    { 0x1115, 0x1151, 0x1449 },   /* 5  */
    { 0x1155, 0x1515, 0x1199 },   /* 6  */
    { 0x1555, 0x115D, 0x1157 },   /* 7  */
    { 0x5555, 0x1557, 0x5157 },   /* 8  */
    { 0x1D5D, 0x5557, 0x5D55 },   /* 9  */
    { 0x5577, 0x5D5D, 0x7755 },   /* 10 */
};
/* phrase-ending bars: last onset by step 8, then the note is held */
static const uint16_t rhythm_end[7] = { 0x0111, 0x0115, 0x0155, 0x0157, 0x015D, 0x015F, 0x017F };

static int popcount16(unsigned v)
{
    int n = 0;
    while (v) { n += (int)(v & 1u); v >>= 1; }
    return n;
}

/* C8..B8 (MIDI 108..119) in Hz */
static const uint16_t note_top[12] = { 4186, 4435, 4699, 4978, 5274, 5588, 5920,
                                       6272, 6645, 7040, 7459, 7902 };

uint16_t concert_note_hz(int midi)
{
    int oct;
    if (midi <= 0) return 0;
    if (midi > 119) midi = 119;
    oct = midi / 12;                     /* 9 = the top octave */
    return (uint16_t)((note_top[midi % 12] + ((1 << (9 - oct)) >> 1)) >> (9 - oct));
}

static int deg_midi(int key, int scale, int deg)
{
    int oct = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
    int idx = deg - oct * 7;
    return key + oct * 12 + scale_steps[scale][idx];
}

/* is degree d one of chord c's tones? */
static int chord_tone(int d, int c)
{
    int r = ((d - c) % 7 + 7) % 7;
    return r == 0 || r == 2 || r == 4;
}

/* nearest chord tone to d (ties: up if rnd) */
static int nearest_chord(int d, int c, int rnd)
{
    int k;
    for (k = 0; k < 4; k++) {
        if (rnd) {
            if (chord_tone(d + k, c)) return d + k;
            if (chord_tone(d - k, c)) return d - k;
        } else {
            if (chord_tone(d - k, c)) return d - k;
            if (chord_tone(d + k, c)) return d + k;
        }
    }
    return d;
}

static int pent_avoid(int d, int scale)
{
    int r = ((d % 7) + 7) % 7;
    if (scale == SCALE_MAJOR || scale == SCALE_MIXO) return r == 3 || r == 6;
    return r == 1 || r == 5;
}

static uint16_t pick_rhythm(DoomoRng *rng, int syl, int end)
{
    int n = DOOMO_CLAMP(syl, 3, 10), i, best = 0, bd = 99;
    if (end) {
        for (i = 0; i < 7; i++) {
            int d = popcount16(rhythm_end[i]) - n;
            if (d < 0) d = -d;
            if (d < bd || (d == bd && rng_chance(rng, 40))) { bd = d; best = i; }
        }
        return rhythm_end[best];
    }
    {
        uint16_t m = rhythm_bank[n - 3][rng_range(rng, 3)];
        return m;
    }
}

/* one 4-bar section of melody */
static void make_melody(ConcertSong *s, int sec, DoomoRng *rng, const ConcertGenre *g,
                        const uint8_t *syl)
{
    uint8_t *mel = s->mel[sec];
    int bar, lo = sec ? 2 : -1, hi = sec ? 9 : 7;
    int d = sec ? 4 : 2, dir = 1;
    memset(mel, 0, SONG_SECTION_BARS * SONG_STEPS_BAR);
    for (bar = 0; bar < SONG_SECTION_BARS; bar++) {
        int c = s->prog[sec][bar], step, last_on = -1, end = bar == 3;
        uint16_t rh;
        if (sec == 1 && bar == 2) {
            /* the hook comes back (re-harmonised on the strong beats) */
            for (step = 0; step < SONG_STEPS_BAR; step++) {
                uint8_t v = mel[step];
                if (v >= 2 && (step & 3) == 0) {
                    /* find its degree again: search near */
                    int k;
                    for (k = lo - 3; k <= hi + 3; k++)
                        if (deg_midi(s->key, g->scale, k) == v) {
                            if (!chord_tone(k, c)) v = (uint8_t)deg_midi(s->key, g->scale,
                                                                       nearest_chord(k, c, k < d));
                            break;
                        }
                }
                mel[2 * SONG_STEPS_BAR + step] = v;
            }
            continue;
        }
        rh = pick_rhythm(rng, syl[bar], end);
        if (bar == 0 && sec == 0) d = nearest_chord(2, c, 1);
        for (step = 0; step < SONG_STEPS_BAR; step++) {
            int idx = bar * SONG_STEPS_BAR + step;
            if (!(rh & (1u << step))) {
                mel[idx] = (uint8_t)(last_on >= 0 ? 1 : 0);
                continue;
            }
            if ((step & 3) == 0) {
                d = nearest_chord(d + (rng_range(rng, 3) - 1), c, dir > 0);
            } else {
                static const int8_t moves[8] = { -2, -1, -1, 1, 1, 2, 0, 1 };
                int mv = moves[rng_range(rng, 8)];
                if (dir < 0) mv = -mv;
                d += mv;
                if (g->pent && pent_avoid(d, g->scale)) d += (mv >= 0) ? 1 : -1;
            }
            if (d > hi) { d = hi - rng_range(rng, 2); dir = -1; }
            if (d < lo) { d = lo + rng_range(rng, 2); dir = 1; }
            if (rng_chance(rng, 18)) dir = -dir;
            mel[idx] = (uint8_t)deg_midi(s->key, g->scale, d);
            last_on = step;
        }
        if (end) {
            /* land on the chord root (or its third) */
            int k, lastidx = -1;
            for (k = SONG_STEPS_BAR - 1; k >= 0; k--)
                if (mel[bar * SONG_STEPS_BAR + k] >= 2) { lastidx = k; break; }
            if (lastidx >= 0) {
                int t = c + (rng_chance(rng, 30) ? 2 : 0);
                while (t < lo) t += 7;
                while (t > hi) t -= 7;
                d = t;
                mel[bar * SONG_STEPS_BAR + lastidx] = (uint8_t)deg_midi(s->key, g->scale, d);
            }
        }
        /* stop holding over a gap of more than a beat at the bar's end */
    }
}

void concert_song_make(ConcertSong *s, int singer, int genre, int take)
{
    DoomoRng rng;
    const ConcertGenre *g;
    const DoomoResident *r = town_res(singer);
    const uint8_t *set;
    int nset, i, sec;
    uint8_t syl[2][4];
    uint32_t seed;
    memset(s, 0, sizeof(*s));
    genre = DOOMO_CLAMP(genre, 0, MUSIC_COUNT - 1);
    take &= 3;
    s->singer = (uint16_t)(singer < 0 ? 0 : singer);
    s->genre = (uint8_t)genre;
    s->take = (uint8_t)take;
    g = &concert_genres[genre];
    seed = doomo_hash32((uint32_t)s->singer * 7919u + (uint32_t)genre * 104729u +
                        (uint32_t)take * 15485863u + 0xC0C0u);
    rng_seed(&rng, seed);
    s->key = (uint8_t)(g->root + rng_range(&rng, 5));
    s->bpm = (uint8_t)(g->bpm + rng_range(&rng, 9) - 4);
    s->edgy = (uint8_t)((r && (RES_DISTRICT(singer) || (r->flags & RF_HELLBORN))) ? 1 : 0);
    if (singer >= DOOMO_MAX_RESIDENTS) s->edgy = 0;           /* the player */
    if (g->bass_style == BS_WALK && genre == 5) { set = prog_jazz; nset = (int)sizeof(prog_jazz); }
    else if (g->scale == SCALE_PHRYGIAN) { set = prog_phryg; nset = (int)sizeof(prog_phryg); }
    else if (g->scale == SCALE_MAJOR || g->scale == SCALE_MIXO) { set = prog_major; nset = (int)sizeof(prog_major); }
    else { set = prog_minor; nset = (int)sizeof(prog_minor); }
    {
        int a = rng_range(&rng, nset), b = rng_range(&rng, nset);
        if (b == a && nset > 1) b = (a + 1) % nset;
        for (i = 0; i < 4; i++) {
            s->prog[0][i] = progs[set[a]][i];
            s->prog[1][i] = progs[set[b]][i];
        }
    }
    /* lyrics first: the melody's rhythm follows their syllables */
    concert_lyrics_make(s, seed ^ 0x5A17u);
    for (sec = 0; sec < 2; sec++)
        for (i = 0; i < 4; i++) {
            uint8_t st[CONCERT_SYL_MAX];
            syl[sec][i] = (uint8_t)concert_syllables(s->lines[sec * 4 + i], st, CONCERT_SYL_MAX);
        }
    make_melody(s, 0, &rng, g, syl[0]);
    make_melody(s, 1, &rng, g, syl[1]);
}

uint32_t concert_song_hash(const ConcertSong *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(*s); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* ====================================================================== */
/* the sequencer                                                            */
static int music_vol(int v)
{
    int mv = opt_get(OPT_MUSIC_VOL);
    v = v * mv / 7;
    return DOOMO_CLAMP(v, 0, 15);
}

static void tone(int ch, int hz, int vol, int wave)
{
    vol = music_vol(vol);
    if (hz <= 0 || vol <= 0) plat_tone(ch, 0, 0, (uint8_t)wave);
    else plat_tone(ch, (uint16_t)DOOMO_CLAMP(hz, 20, 20000), (uint8_t)vol, (uint8_t)wave);
}

void concert_play_silence(void)
{
    int ch;
    for (ch = 0; ch < 4; ch++) plat_tone(ch, 0, 0, WAVE_PULSE50);
}

static int sec_of_bar(int bar, int encore, int *bar_in)
{
    static const uint8_t len[SEC_END] = { 2, 4, 4, 4, 4, 2 };
    int s;
    if (encore) {
        if (bar < 4) { *bar_in = bar; return SEC_CHORUS2; }
        *bar_in = bar - 4;
        return bar - 4 < 2 ? SEC_OUTRO : SEC_END;
    }
    for (s = 0; s < SEC_END; s++) {
        if (bar < len[s]) { *bar_in = bar; return s; }
        bar -= len[s];
    }
    *bar_in = 0;
    return SEC_END;
}

int concert_play_section(const ConcertPlayer *p)
{
    int b;
    return sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &b);
}

int concert_play_bar(const ConcertPlayer *p)
{
    int b;
    sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &b);
    return b;
}

int concert_play_progress(const ConcertPlayer *p)
{
    return p->total ? (int)((uint32_t)p->step * 255u / p->total) : 0;
}

/* chord degree for (section, bar) */
static int chord_at(const ConcertSong *s, int sec, int bar)
{
    switch (sec) {
    case SEC_INTRO: return s->prog[0][bar & 3];
    case SEC_VERSE1: case SEC_VERSE2: return s->prog[0][bar & 3];
    case SEC_CHORUS1: case SEC_CHORUS2: return s->prog[1][bar & 3];
    case SEC_OUTRO: return bar == 0 ? s->prog[1][3] : 0;
    default: return 0;
    }
}

static const uint8_t *melody_at(const ConcertSong *s, int sec)
{
    if (sec == SEC_VERSE1 || sec == SEC_VERSE2) return s->mel[0];
    if (sec == SEC_CHORUS1 || sec == SEC_CHORUS2) return s->mel[1];
    return NULL;
}

static int line_at(int sec, int bar)
{
    switch (sec) {
    case SEC_VERSE1: return bar;
    case SEC_CHORUS1: case SEC_CHORUS2: return 4 + bar;
    case SEC_VERSE2: return 8 + bar;
    default: return -1;
    }
}

void concert_play_start(ConcertPlayer *p, const ConcertSong *s, const uint8_t *voice,
                        int encore)
{
    int bpm = s->bpm ? s->bpm : 120;
    memset(p, 0, sizeof(*p));
    p->song = s;
    p->voice = voice;
    p->encore = (uint8_t)(encore ? 1 : 0);
    if (encore) bpm += bpm / 10;                 /* the encore is faster */
    p->step_len = (uint16_t)(450u * 256u / (unsigned)bpm);
    p->total = (uint16_t)((encore ? SONG_ENCORE_BARS : SONG_BARS) * SONG_STEPS_BAR);
    p->phase = p->step_len;                      /* step 0 fires at once */
    p->line = -1;
    p->step = 0xFFFF;
}

/* the singer's voice: wave from quality, octave from pitch, vibrato from
 * intonation, articulation from speed */
static void lead_tick(ConcertPlayer *p, int fresh)
{
    const ConcertGenre *g = &concert_genres[p->song->genre];
    int wave = g->lead_wave, oct = 0, vib = 3, legato = 1, hz, vol;
    if (p->voice) {
        int q = p->voice[VOICE_QUALITY], pi = p->voice[VOICE_PITCH];
        if (q < 8) wave = WAVE_TRIANGLE;
        else if (q > 26) wave = WAVE_PULSE12;
        oct = pi >= 24 ? 12 : pi <= 5 ? -12 : 0;
        vib = 1 + p->voice[VOICE_INTONATION] / 6;
        legato = p->voice[VOICE_SPEED] < 22;
    }
    if (!p->lead_note) { tone(0, 0, 0, wave); return; }
    if (fresh) { p->lead_age = 0; p->lead_vol = 13; }
    else if (p->lead_age < 255) p->lead_age++;
    vol = p->lead_vol;
    if (p->lead_age > 1) vol -= DOOMO_MIN(p->lead_age - 1, legato ? 3 : 7);
    if (vol < 3) vol = 3;
    hz = concert_note_hz(p->lead_note + oct);
    if (p->lead_age > 4)                         /* delayed vibrato */
        hz += ui_wave(p->ticks, 6, hz * vib / 220);
    tone(0, hz, vol, wave);
}

static void drum_tick(ConcertPlayer *p)
{
    int a = p->drum_age;
    switch (p->drum_kind) {
    case 1:     /* kick: a thump sweeping down */
        if (a < 4) tone(3, 140 - a * 30, 14 - a * 4, WAVE_PULSE50);
        else tone(3, 0, 0, WAVE_NOISE);
        break;
    case 2:     /* snare */
        if (a < 4) tone(3, 2200 - a * 200, 12 - a * 3, WAVE_NOISE);
        else tone(3, 0, 0, WAVE_NOISE);
        break;
    case 3:     /* closed hat */
        if (a < 1) tone(3, 9000, 6, WAVE_NOISE);
        else tone(3, 0, 0, WAVE_NOISE);
        break;
    case 4:     /* open hat */
        if (a < 3) tone(3, 8000, 6 - a * 2, WAVE_NOISE);
        else tone(3, 0, 0, WAVE_NOISE);
        break;
    case 5:     /* metallic clank / ride */
        if (a < 2) tone(3, 6000, 7 - a * 3, WAVE_PULSE25);
        else tone(3, 0, 0, WAVE_NOISE);
        break;
    default:
        tone(3, 0, 0, WAVE_NOISE);
        break;
    }
    if (p->drum_age < 255) p->drum_age++;
}

static void karaoke_bar(ConcertPlayer *p, int sec, int bar)
{
    const uint8_t *mel = melody_at(p->song, sec);
    int k;
    p->line = (int8_t)line_at(sec, bar);
    p->sung = 0;
    p->onsets = 0;
    p->line_syl = 0;
    if (p->line < 0 || !mel) { p->line = -1; return; }
    for (k = 0; k < SONG_STEPS_BAR; k++)
        if (mel[bar * SONG_STEPS_BAR + k] >= 2) p->onsets++;
    p->line_syl = (uint8_t)concert_syllables(p->song->lines[(int)p->line], p->syl_start,
                                             CONCERT_SYL_MAX);
}

static int step_event(ConcertPlayer *p)
{
    const ConcertSong *s = p->song;
    const ConcertGenre *g = &concert_genres[s->genre];
    const DrumPat *dp = &drum_pats[g->drums];
    int ev = PEV_STEP, bar, sec, st = p->step % SONG_STEPS_BAR, c, root, third, fifth;
    const uint8_t *mel;
    sec = sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &bar);
    if (sec == SEC_END) return PEV_END;
    if ((st & 3) == 0) ev |= PEV_BEAT;
    if (st == 0) {
        ev |= PEV_BAR;
        if (bar == 0) ev |= PEV_SECTION;
        karaoke_bar(p, sec, bar);
    }
    c = chord_at(s, sec, bar);
    root = deg_midi(s->key, g->scale, c);
    third = deg_midi(s->key, g->scale, c + 2);
    fifth = deg_midi(s->key, g->scale, c + 4);

    /* melody + lyrics */
    mel = melody_at(s, sec);
    if (mel) {
        uint8_t v = mel[bar * SONG_STEPS_BAR + st];
        if (v >= 2) {
            int syl0;
            p->lead_note = v;
            lead_tick(p, 1);
            ev |= PEV_ONSET;
            p->onset_flash = 6;
            /* which syllable starts on this onset (spread over the bar) */
            if (p->line >= 0 && p->line_syl) {
                int on_idx = 0, k;
                for (k = 0; k < st; k++)
                    if (mel[bar * SONG_STEPS_BAR + k] >= 2) on_idx++;
                syl0 = p->onsets ? on_idx * p->line_syl / p->onsets : 0;
                p->sung = (uint8_t)(p->onsets ? ((on_idx + 1) * p->line_syl + p->onsets - 1) / p->onsets
                                              : p->line_syl);
                if (p->sung > p->line_syl) p->sung = p->line_syl;
                if (p->voice && opt_get(OPT_VOICE_BABBLE)) {
                    const char *ln = s->lines[(int)p->line];
                    audio_voice_char(p->voice, ln[p->syl_start[DOOMO_MIN(syl0, p->line_syl - 1)]]);
                }
            }
        } else if (v == 0) {
            p->lead_note = 0;
        }
    } else {
        p->lead_note = 0;
    }

    /* bass */
    {
        int bn = 0, broot = root - 24, v = 0;
        while (broot > 52) broot -= 12;
        while (broot < 36) broot += 12;
        switch (g->bass_style) {
        case BS_ROOTS:   if (st == 0 || st == 8) bn = broot; else if (st == 12) bn = fifth - 24; break;
        case BS_OCTAVES: if (!(st & 1)) bn = (st & 2) ? broot + 12 : broot; break;
        case BS_WALK:
            if (!(st & 3)) {
                int nb = sec_of_bar(p->step / SONG_STEPS_BAR + 1, p->encore, &v) == SEC_END
                         ? root : deg_midi(s->key, g->scale, chord_at(s, sec, (bar + 1) & 3));
                bn = st == 0 ? broot : st == 4 ? third - 24 : st == 8 ? fifth - 24 : nb - 25;
                while (bn > 55) bn -= 12;
                while (bn < 34) bn += 12;
            }
            break;
        case BS_EIGHTHS: if (!(st & 1)) bn = broot; break;
        case BS_LONG:    if (st == 0) bn = broot; break;
        case BS_GALLOP:  if ((st & 3) != 1) bn = broot; break;
        }
        if (sec == SEC_OUTRO && bar == 1 && st > 0) bn = 0;
        if (bn) {
            p->bass_vol = 12;
            p->bass_note = (uint8_t)bn;
            tone(2, concert_note_hz(bn), p->bass_vol, g->bass_wave);
        } else if (g->bass_style != BS_LONG && g->bass_style != BS_ROOTS) {
            p->bass_vol = 0;
            tone(2, 0, 0, g->bass_wave);
        }
    }

    /* chords (per step part; arps/pads also move per tick) */
    {
        int cn = 0;
        switch (g->chord_style) {
        case CH_STAB:    if (st == 4 || st == 12 || (st == 10 && sec >= SEC_CHORUS1)) cn = third; break;
        case CH_OFFBEAT: if ((st & 3) == 2) cn = (st & 4) ? fifth : third; break;
        case CH_ALBERTI: if (!(st & 1)) { static const int8_t pat[4] = { 0, 2, 1, 2 };
                             int w = pat[(st >> 1) & 3]; cn = w == 0 ? root : w == 1 ? third : fifth;
                             cn -= 12; } break;
        case CH_POWER:   if (((dp->kick | dp->snare) & (1u << st)) || !(st & 1)) cn = root - 12; break;
        default: break;
        }
        if (sec == SEC_INTRO && bar == 0 && g->chord_style != CH_PAD && g->chord_style != CH_ARP) cn = 0;
        if (cn) {
            p->chord_vol = 9;
            p->chord_note = (uint8_t)cn;
            tone(1, concert_note_hz(cn), 9, g->chord_wave);
        }
    }

    /* drums */
    {
        uint16_t kick = (sec == SEC_CHORUS1 || sec == SEC_CHORUS2) ? dp->kick_chorus : dp->kick;
        int fill = (sec == SEC_VERSE1 || sec == SEC_VERSE2) && bar == 3 && st >= 12;
        if (sec == SEC_INTRO && bar == 0) kick &= 0x0101;
        if (sec == SEC_OUTRO && bar == 1) kick = st == 0 ? 1 : 0;
        if (kick & (1u << st)) { p->drum_kind = 1; p->drum_age = 0; ev |= PEV_KICK; p->kick_flash = 5; }
        else if ((dp->snare & (1u << st)) || (fill && g->drums != DRUMS_NONE && (st & 1) == 0)) {
            if (!(sec == SEC_OUTRO && bar == 1)) {
                p->drum_kind = 2; p->drum_age = 0; ev |= PEV_SNARE; p->snare_flash = 5;
            }
        } else if ((dp->hat & (1u << st)) && !(sec == SEC_INTRO && bar == 0)) {
            p->drum_kind = (uint8_t)(dp->metal_hat ? 5 : dp->open_hat ? 4 : 3);
            p->drum_age = 0;
        }
    }
    return ev;
}

int concert_play_tick(ConcertPlayer *p)
{
    const ConcertGenre *g;
    int ev = 0;
    if (!p->song) return PEV_END;
    g = &concert_genres[p->song->genre];
    p->ticks++;
    if (p->kick_flash) p->kick_flash--;
    if (p->snare_flash) p->snare_flash--;
    if (p->onset_flash) p->onset_flash--;
    p->phase = (uint16_t)(p->phase + 256);
    if (p->phase >= p->step_len) {
        p->phase = (uint16_t)(p->phase - p->step_len);
        p->step = (uint16_t)(p->step + 1);          /* 0xFFFF -> 0 at start */
        if (p->step >= p->total) {
            concert_play_silence();
            p->song = NULL;
            return PEV_END;
        }
        ev = step_event(p);
        if (ev & PEV_END) {
            concert_play_silence();
            p->song = NULL;
            return PEV_END;
        }
        if (!(ev & PEV_ONSET)) lead_tick(p, 0);
    } else {
        lead_tick(p, 0);
    }
    /* per-tick parts */
    {
        const ConcertSong *s = p->song;
        int bar, sec = sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &bar);
        int c = chord_at(s, sec, bar);
        int r = deg_midi(s->key, g->scale, c) - 12;
        if (g->chord_style == CH_ARP && sec != SEC_END) {
            static const int8_t arp[4] = { 0, 2, 4, 7 };
            int fast = s->genre == 3, k = fast ? p->ticks & 3 : (p->step + 0) & 3;
            int n = arp[k] == 7 ? r + 12 : deg_midi(s->key, g->scale, c + arp[k]) - 12;
            tone(1, concert_note_hz(n), 7 - (p->phase * 3 / (p->step_len ? p->step_len : 1)),
                 g->chord_wave);
        } else if (g->chord_style == CH_PAD && sec != SEC_END) {
            static const int8_t pad[3] = { 0, 2, 4 };
            int n = deg_midi(s->key, g->scale, c + pad[(p->ticks >> 1) % 3]) - 12;
            tone(1, concert_note_hz(n), 6, g->chord_wave);
        } else if (g->chord_style == CH_POWER && p->chord_vol) {
            int n = (p->ticks & 1) ? r + 7 : r;
            tone(1, concert_note_hz(n), p->chord_vol, g->chord_wave);
            p->chord_vol = (uint8_t)(p->chord_vol > 2 ? p->chord_vol - 2 : 0);
        } else if (p->chord_vol) {
            int dec = g->chord_style == CH_ALBERTI ? 1 : g->chord_style == CH_STAB ? 3 : 2;
            int floor_v = g->chord_style == CH_ALBERTI ? 4 : 0;
            if (p->chord_vol > floor_v)
                p->chord_vol = (uint8_t)DOOMO_MAX(p->chord_vol - dec, floor_v);
            tone(1, p->chord_vol ? concert_note_hz(p->chord_note) : 0, p->chord_vol, g->chord_wave);
        }
        if ((g->bass_style == BS_LONG || g->bass_style == BS_ROOTS) && p->bass_vol > 6 &&
            (p->ticks & 3) == 0) {
            p->bass_vol--;
            tone(2, concert_note_hz(p->bass_note), p->bass_vol, g->bass_wave);
        }
        drum_tick(p);
    }
    return ev;
}

void concert_restore_music(void)
{
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter >= 3 &&
        doomo_town.story.chapter < 4)
        audio_music(SONG_INVASION);
    else
        audio_music(town_is_night() ? SONG_HAHALAND_NIGHT : SONG_HAHALAND_DAY);
}
