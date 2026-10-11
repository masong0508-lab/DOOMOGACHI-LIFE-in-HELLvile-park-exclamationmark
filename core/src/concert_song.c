/* concert_song.c - the Concert Hall's procedural composer and its
 * 4-channel sequencer (owner: concert).
 *
 * A song is composed from (singer, genre, take): key, tempo, two chord
 * progressions (verse / chorus), a melody whose rhythm follows the
 * syllables of the lyrics, a bass line, a chord part and a drum pattern,
 * all in the genre's style. It plays directly on the platform's tone
 * channels (audio_music(SONG_NONE) first, the scene restores the music):
 *   ch0 lead (the singer: wave, octave, vibrato and articulation follow
 *       their voice sliders, each syllable's vowel colours the tone)
 *   ch1 chords, or the choir's harmony (a third below the tune) when 2+
 *       citizens sing a line; audio_voice_char babbles every syllable here
 *       (the player leaves channel 1 alone while a blip sounds)
 *   ch2 bass   ch3 drums (noise)
 * Bands and choirs: concert_cast_make() picks the band mates / up to 25
 * choir members (friends first), sorts them into voice sections by pitch
 * and decides who sings which line (solo, duet, a section, everyone); the
 * voices rotate through the line's singers with a little pitch spread.
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
    G(128, CSCALE_MAJOR, 0, 64, WAVE_PULSE25, WAVE_PULSE12, WAVE_TRIANGLE, CH_ARP, BS_OCTAVES,
      DRUMS_POP, CROWD_JUMP, SHOW_HEARTS, 0, PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_SKY, "Bubble pop!"),
    /* Lo-fi Beats */
    G(82, CSCALE_DORIAN, 1, 62, WAVE_TRIANGLE, WAVE_PULSE50, WAVE_TRIANGLE, CH_PAD, BS_ROOTS,
      DRUMS_LOFI, CROWD_SWAY, SHOW_NOTES, 0, PAINT_LAVENDER, PAINT_PEACH, PAINT_TEAL, "Mmm... chill."),
    /* City Pop */
    G(108, CSCALE_MAJOR, 0, 62, WAVE_PULSE50, WAVE_PULSE25, WAVE_SAW, CH_OFFBEAT, BS_WALK,
      DRUMS_CITY, CROWD_SWAY, SHOW_SKYLINE, 0, PAINT_MAGENTA, PAINT_CYAN, PAINT_SKY, "Midnight drive~"),
    /* Chiptune */
    G(140, CSCALE_MAJOR, 0, 67, WAVE_PULSE12, WAVE_PULSE25, WAVE_TRIANGLE, CH_ARP, BS_EIGHTHS,
      DRUMS_CHIP, CROWD_JUMP, SHOW_PIXELS, 0, PAINT_LIME, PAINT_SKY, PAINT_MAGENTA, "Bleep bloop!"),
    /* Lullabies */
    G(72, CSCALE_MAJOR, 1, 64, WAVE_TRIANGLE, WAVE_TRIANGLE, WAVE_TRIANGLE, CH_ALBERTI, BS_LONG,
      DRUMS_NONE, CROWD_SWAY, SHOW_STARS, 0, PAINT_SKY, PAINT_LEMON, PAINT_LAVENDER, "Hush now..."),
    /* Jazz */
    G(120, CSCALE_DORIAN, 0, 62, WAVE_PULSE50, WAVE_PULSE25, WAVE_TRIANGLE, CH_STAB, BS_WALK,
      DRUMS_JAZZ, CROWD_BOB, SHOW_SPOTS, 0, PAINT_NAVY, PAINT_HONEY, PAINT_PEACH, "Doo-be-doo!"),
    /* Classical */
    G(92, CSCALE_HARMONIC, 0, 65, WAVE_PULSE50, WAVE_TRIANGLE, WAVE_TRIANGLE, CH_ALBERTI, BS_LONG,
      DRUMS_NONE, CROWD_WAVE, SHOW_GLASS, 0, PAINT_CHERRY, PAINT_HONEY, PAINT_WHITE, "La la laaa!"),
    /* Folk */
    G(104, CSCALE_MAJOR, 1, 62, WAVE_PULSE25, WAVE_PULSE50, WAVE_TRIANGLE, CH_STAB, BS_ROOTS,
      DRUMS_FOLK, CROWD_WAVE, SHOW_FIELDS, 0, PAINT_FOREST, PAINT_HONEY, PAINT_PEACH, "Hey ho!"),
    /* Disco */
    G(118, CSCALE_DORIAN, 0, 62, WAVE_PULSE25, WAVE_PULSE12, WAVE_SAW, CH_OFFBEAT, BS_OCTAVES,
      DRUMS_DISCO, CROWD_JUMP, SHOW_DISCO, 0, PAINT_GRAPE, PAINT_HONEY, PAINT_MAGENTA, "Get down!"),
    /* Sea Shanties */
    G(100, CSCALE_DORIAN, 1, 60, WAVE_PULSE50, WAVE_PULSE25, WAVE_TRIANGLE, CH_STAB, BS_ROOTS,
      DRUMS_SHANTY, CROWD_SWAY, SHOW_WAVES, 0, PAINT_NAVY, PAINT_TEAL, PAINT_WHITE, "Yo ho!"),
    /* Hymns */
    G(80, CSCALE_MAJOR, 0, 62, WAVE_TRIANGLE, WAVE_PULSE50, WAVE_TRIANGLE, CH_PAD, BS_LONG,
      DRUMS_NONE, CROWD_WAVE, SHOW_GLASS, 0, PAINT_LEMON, PAINT_SKY, PAINT_WHITE, "Hallelujah!"),
    /* Synthwave */
    G(110, CSCALE_MINOR, 0, 62, WAVE_SAW, WAVE_PULSE25, WAVE_SAW, CH_ARP, BS_EIGHTHS,
      DRUMS_SYNTH, CROWD_BOB, SHOW_GRID, 0, PAINT_MAGENTA, PAINT_CYAN, PAINT_GRAPE, "Neon nights!"),
    /* Goth Rock */
    G(126, CSCALE_HARMONIC, 0, 60, WAVE_PULSE50, WAVE_PULSE25, WAVE_SAW, CH_ARP, BS_EIGHTHS,
      DRUMS_GOTH, CROWD_SWAY, SHOW_BATS, 1, PAINT_GRAPE, PAINT_ROSE, PAINT_LAVENDER, "Darkness, darling~"),
    /* Hardcore Punk */
    G(176, CSCALE_MIXO, 0, 60, WAVE_PULSE12, WAVE_PULSE50, WAVE_SAW, CH_POWER, BS_EIGHTHS,
      DRUMS_PUNK, CROWD_MOSH, SHOW_SKULLS, 1, PAINT_LIME, PAINT_CHERRY, PAINT_LEMON, "OI! OI! OI!"),
    /* Doom Metal */
    G(96, CSCALE_PHRYGIAN, 0, 57, WAVE_SAW, WAVE_PULSE12, WAVE_SAW, CH_POWER, BS_GALLOP,
      DRUMS_METAL, CROWD_HEADBANG, SHOW_FLAMES, 1, PAINT_HELLFIRE, PAINT_BLOOD, PAINT_TANGERINE, "BLEGH!"),
    /* Industrial */
    G(124, CSCALE_PHRYGIAN, 0, 57, WAVE_PULSE12, WAVE_PULSE25, WAVE_SAW, CH_STAB, BS_EIGHTHS,
      DRUMS_INDUSTRIAL, CROWD_HEADBANG, SHOW_GEARS, 1, PAINT_RUST, PAINT_CHARCOAL, PAINT_SLIME, "CLANG CLANG!"),
};
#undef G

static const uint8_t scale_steps[CSCALE_COUNT][7] = {
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
    if (scale == CSCALE_MAJOR || scale == CSCALE_MIXO) return r == 3 || r == 6;
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
    if (singer < 0 || singer > DOOMO_PLAYER_ID) singer = 0;
    s->singer = (uint16_t)singer;
    s->genre = (uint8_t)genre;
    s->take = (uint8_t)take;
    g = &concert_genres[genre];
    seed = doomo_hash32((uint32_t)s->singer * 7919u + (uint32_t)genre * 104729u +
                        (uint32_t)take * 15485863u + 0xC0C0u);
    s->seed = seed;
    rng_seed(&rng, seed);
    s->key = (uint8_t)(g->root + rng_range(&rng, 5));
    s->bpm = (uint8_t)(g->bpm + rng_range(&rng, 9) - 4);
    s->edgy = (uint8_t)((r && (RES_DISTRICT(singer) || (r->flags & RF_HELLBORN))) ? 1 : 0);
    if (singer >= DOOMO_MAX_RESIDENTS) s->edgy = 0;           /* the player */
    if (g->bass_style == BS_WALK && genre == 5) { set = prog_jazz; nset = (int)sizeof(prog_jazz); }
    else if (g->scale == CSCALE_PHRYGIAN) { set = prog_phryg; nset = (int)sizeof(prog_phryg); }
    else if (g->scale == CSCALE_MAJOR || g->scale == CSCALE_MIXO) { set = prog_major; nset = (int)sizeof(prog_major); }
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
/* the act: band mates and choir members                                    */
int concert_mask_count(uint32_t m)
{
    int n = 0;
    while (m) { m &= m - 1; n++; }
    return n;
}

static int cast_has(const ConcertCast *c, int id)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (c->ids[i] == id) return 1;
    return 0;
}

static int can_perform(int id)
{
    const DoomoResident *r = town_res(id);
    return r && (r->status == RES_ALIVE || r->status == RES_GHOST ||
                 r->status == RES_POSSESSED);
}

static uint32_t section_mask(const ConcertCast *c, int sec)
{
    uint32_t m = 0;
    int i;
    for (i = 0; i < c->n; i++)
        if (c->section[i] == sec) m |= 1u << i;
    return m;
}

void concert_cast_make(ConcertCast *c, int lead, int mode, int choir_n, int layout)
{
    uint16_t rel[24];
    int nrel, i, want, start, k;
    uint32_t all;
    memset(c, 0, sizeof(*c));
    for (i = 0; i < BAND_COUNT; i++) c->band[i] = CONCERT_NOBODY;
    for (i = 0; i < SONG_SINGERS; i++) c->ids[i] = CONCERT_NOBODY;
    mode = DOOMO_CLAMP(mode, 0, CMODE_COUNT - 1);
    c->mode = (uint8_t)mode;
    c->layout = (uint8_t)DOOMO_CLAMP(layout, 0, CHOIR_LAYOUT_COUNT - 1);
    c->ids[0] = (uint16_t)lead;
    c->n = 1;
    want = mode == CMODE_CHOIR ? DOOMO_CLAMP(choir_n, 2, SONG_SINGERS) :
           mode == CMODE_BAND ? 1 + BAND_COUNT : 1;
    /* friends first (best relationships), then neighbours */
    nrel = rel_list(lead, rel, DOOMO_COUNTOF(rel));
    for (i = 0; i < nrel && c->n < want; i++)
        if (rel_value(lead, rel[i]) > 0 && rel[i] != lead && rel[i] < DOOMO_MAX_RESIDENTS &&
            can_perform(rel[i]) && !cast_has(c, rel[i]))
            c->ids[c->n++] = rel[i];
    start = (int)(doomo_hash32((uint32_t)lead * 2654435761u + 77u) % DOOMO_DISTRICT_SIZE);
    for (k = 0; k < DOOMO_MAX_RESIDENTS && c->n < want; k++) {
        /* the lead's own district first, then across the wall */
        int half = k < DOOMO_DISTRICT_SIZE ? 0 : 1;
        int d = (lead >= 0 && lead < DOOMO_MAX_RESIDENTS ? RES_DISTRICT(lead) : 0) ^ half;
        int id = d * DOOMO_DISTRICT_SIZE + (start + k * 37) % DOOMO_DISTRICT_SIZE;
        if (id != lead && can_perform(id) && !cast_has(c, id))
            c->ids[c->n++] = (uint16_t)id;
    }
    if (mode == CMODE_BAND) {
        for (i = 1; i < c->n && i <= BAND_COUNT; i++) c->band[i - 1] = c->ids[i];
        c->nband = (uint8_t)(c->n - 1);
    }
    /* voice sections by pitch: sopranos are the squeakiest quarter */
    {
        uint8_t order[SONG_SINGERS];
        int a, b;
        for (i = 0; i < c->n; i++) order[i] = (uint8_t)i;
        for (a = 1; a < c->n; a++)
            for (b = a; b > 0; b--) {
                const DoomoResident *x = town_res(c->ids[order[b - 1]]);
                const DoomoResident *y = town_res(c->ids[order[b]]);
                int px = x ? x->voice[VOICE_PITCH] : 16, py = y ? y->voice[VOICE_PITCH] : 16;
                if (px >= py) break;
                { uint8_t t = order[b]; order[b] = order[b - 1]; order[b - 1] = t; }
            }
        for (i = 0; i < c->n; i++)
            c->section[order[i]] = (uint8_t)(c->n >= 4 ? i * SECTION_COUNT / c->n
                                                       : (i < 2 ? SECTION_SOPRANO : SECTION_ALTO));
    }
    /* who sings which line */
    all = c->n >= 32 ? 0xFFFFFFFFu : (1u << c->n) - 1u;
    for (i = 0; i < CONCERT_LINES; i++) c->line_mask[i] = 1u;
    if (mode == CMODE_BAND) {
        for (i = 4; i < 8; i++) c->line_mask[i] = (i & 1) ? all : 1u;   /* backing vocals */
        c->line_mask[11] = all;
    } else if (mode == CMODE_CHOIR) {
        uint32_t m;
        c->line_mask[2] = c->n > 1 ? 3u : 1u;                    /* a duet  */
        m = section_mask(c, SECTION_BASS);
        c->line_mask[3] = m ? m : 1u;                            /* deep!   */
        for (i = 4; i < 8; i++) c->line_mask[i] = all;           /* chorus  */
        m = section_mask(c, SECTION_SOPRANO);
        c->line_mask[8] = m ? m : 1u;
        m = section_mask(c, SECTION_ALTO);
        c->line_mask[9] = m ? m : 1u;
        m = section_mask(c, SECTION_TENOR) | section_mask(c, SECTION_BASS);
        c->line_mask[10] = m ? m : 1u;
        c->line_mask[11] = all;
    }
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
    if (p->step == 0xFFFF) return SEC_INTRO;
    return sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &b);
}

int concert_play_bar(const ConcertPlayer *p)
{
    int b;
    if (p->step == 0xFFFF) return 0;
    sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &b);
    return b;
}

int concert_play_progress(const ConcertPlayer *p)
{
    if (p->step == 0xFFFF) return 0;
    return p->total ? (int)((uint32_t)p->step * 255u / p->total) : 0;
}

int concert_beat_len256(const ConcertPlayer *p) { return p->step_len * 4; }

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

/* scale degree of a MIDI note (or -99) */
static int midi_deg(const ConcertSong *s, int scale, int midi)
{
    int d;
    for (d = -14; d <= 21; d++)
        if (deg_midi(s->key, scale, d) == midi) return d;
    return -99;
}

/* ---- the parts, shared by the player and the Studio export ------------- */
/* bass note starting at (sec, bar, st), 0 = none */
static int bass_at(const ConcertSong *s, const ConcertGenre *g, int sec, int bar, int st, int encore)
{
    int c = chord_at(s, sec, bar), root = deg_midi(s->key, g->scale, c);
    int third = deg_midi(s->key, g->scale, c + 2), fifth = deg_midi(s->key, g->scale, c + 4);
    int bn = 0, broot = root - 24, v = 0;
    while (broot > 52) broot -= 12;
    while (broot < 36) broot += 12;
    switch (g->bass_style) {
    case BS_ROOTS:   if (st == 0 || st == 8) bn = broot; else if (st == 12) bn = fifth - 24; break;
    case BS_OCTAVES: if (!(st & 1)) bn = (st & 2) ? broot + 12 : broot; break;
    case BS_WALK:
        if (!(st & 3)) {
            int nsec = sec, nbar = bar + 1, nb;
            if (sec == SEC_INTRO || sec == SEC_OUTRO) { if (nbar > 1) nbar = 0; }
            else if (nbar > 3) nbar = 0;
            nb = deg_midi(s->key, g->scale, chord_at(s, nsec, nbar));
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
    (void)v;
    (void)encore;
    return bn;
}

/* stepped chord note at (sec, bar, st), 0 = none (arp / pad parts are per
 * tick in the player; `arp_steps` renders them per step for the export) */
static int chord_step_at(const ConcertSong *s, const ConcertGenre *g, int sec, int bar, int st,
                         int arp_steps)
{
    const DrumPat *dp = &drum_pats[g->drums];
    int c = chord_at(s, sec, bar), root = deg_midi(s->key, g->scale, c);
    int third = deg_midi(s->key, g->scale, c + 2), fifth = deg_midi(s->key, g->scale, c + 4);
    int cn = 0;
    switch (g->chord_style) {
    case CH_STAB:    if (st == 4 || st == 12 || (st == 10 && sec >= SEC_CHORUS1)) cn = third; break;
    case CH_OFFBEAT: if ((st & 3) == 2) cn = (st & 4) ? fifth : third; break;
    case CH_ALBERTI: if (!(st & 1)) { static const int8_t pat[4] = { 0, 2, 1, 2 };
                         int w = pat[(st >> 1) & 3]; cn = w == 0 ? root : w == 1 ? third : fifth;
                         cn -= 12; } break;
    case CH_POWER:   if (((dp->kick | dp->snare) & (1u << st)) || !(st & 1)) cn = root - 12; break;
    case CH_ARP:
        if (arp_steps) { static const int8_t arp[4] = { 0, 2, 4, 7 };
            int k = st & 3;
            cn = arp[k] == 7 ? root : deg_midi(s->key, g->scale, c + arp[k]) - 12; }
        break;
    case CH_PAD:
        if (arp_steps && (st & 7) == 0) cn = (st & 8) ? third - 12 : root - 12;
        break;
    default: break;
    }
    if (sec == SEC_INTRO && bar == 0 && g->chord_style != CH_PAD && g->chord_style != CH_ARP) cn = 0;
    return cn;
}

/* drum hit at (sec, bar, st): 0 none, 1 kick, 2 snare, 3 hat, 4 open hat,
 * 5 metallic ride */
static int drum_at(const ConcertGenre *g, int sec, int bar, int st)
{
    const DrumPat *dp = &drum_pats[g->drums];
    uint16_t kick = (sec == SEC_CHORUS1 || sec == SEC_CHORUS2) ? dp->kick_chorus : dp->kick;
    int fill = (sec == SEC_VERSE1 || sec == SEC_VERSE2) && bar == 3 && st >= 12;
    if (sec == SEC_INTRO && bar == 0) kick &= 0x0101;
    if (sec == SEC_OUTRO && bar == 1) kick = st == 0 ? 1 : 0;
    if (kick & (1u << st)) return 1;
    if ((dp->snare & (1u << st)) || (fill && g->drums != DRUMS_NONE && (st & 1) == 0))
        return (sec == SEC_OUTRO && bar == 1) ? 0 : 2;
    if ((dp->hat & (1u << st)) && !(sec == SEC_INTRO && bar == 0))
        return dp->metal_hat ? 5 : dp->open_hat ? 4 : 3;
    return 0;
}

/* the singer's voice for this syllable: rotate through the line's singers,
 * with a little pitch spread so a crowd of voices sounds like a crowd */
static const uint8_t *syllable_voice(ConcertPlayer *p, uint32_t mask, uint8_t *tmp)
{
    const ConcertCast *c = p->cast;
    const DoomoResident *r;
    int cnt = concert_mask_count(mask), pick, i, k = -1;
    if (!c || !mask) {
        r = town_res(p->song->singer);
        p->voice_k = 0;
        return r ? r->voice : NULL;
    }
    pick = (p->ticks / 3 + p->sung) % cnt;
    for (i = 0; i < c->n; i++)
        if (mask & (1u << i)) { if (pick-- == 0) { k = i; break; } }
    if (k < 0) k = 0;
    p->voice_k = (int8_t)k;
    r = town_res(c->ids[k]);
    if (!r) return NULL;
    memcpy(tmp, r->voice, VOICE_COUNT);
    if (cnt > 1) {
        int sp = (int)((p->ticks + k * 5) % 5) - 2;
        tmp[VOICE_PITCH] = (uint8_t)DOOMO_CLAMP(tmp[VOICE_PITCH] + sp, 0, 31);
    }
    return tmp;
}

static const uint8_t *lead_voice(const ConcertPlayer *p)
{
    const DoomoResident *r = town_res(p->cast ? p->cast->ids[p->voice_k > 0 ? p->voice_k : 0]
                                              : p->song->singer);
    return r ? r->voice : NULL;
}

/* vowels colour the sung tone: "aah" round, "eee" thin, "ooo" soft */
static int vowel_wave(int base, char v)
{
    v = (char)(v | 0x20);
    if (base == WAVE_SAW || base == WAVE_TRIANGLE) return base;
    if (v == 'e' || v == 'i' || v == 'y') return base == WAVE_PULSE50 ? WAVE_PULSE25 : WAVE_PULSE12;
    if (v == 'o' || v == 'u') return base == WAVE_PULSE12 ? WAVE_PULSE25 : WAVE_PULSE50;
    return base;
}

/* the singer's voice: wave from quality, octave from pitch, vibrato from
 * intonation, articulation from speed */
static void lead_tick(ConcertPlayer *p, int fresh)
{
    const uint8_t *voice = lead_voice(p);
    int wave = p->lead_wave, oct = 0, vib = 3, legato = 1, hz, vol;
    if (voice) {
        int pi = voice[VOICE_PITCH];
        oct = pi >= 24 ? 12 : pi <= 5 ? -12 : 0;
        vib = 1 + voice[VOICE_INTONATION] / 6;
        legato = voice[VOICE_SPEED] < 22;
    }
    if (!p->lead_note) { tone(0, 0, 0, wave); return; }
    if (fresh) { p->lead_age = 0; p->lead_vol = 13; }
    else if (p->lead_age < 255) p->lead_age++;
    vol = p->lead_vol;
    if (p->lead_age > 1) vol -= DOOMO_MIN(p->lead_age - 1, legato ? 3 : 7);
    if (vol < 3) vol = 3;
    hz = concert_note_hz(p->lead_note + oct);
    if (p->lead_age == 0) hz -= hz / 24;            /* a little scoop up   */
    if (p->lead_age > 4)                            /* delayed vibrato     */
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
    p->singing = 0;
    if (p->line < 0 || !mel) { p->line = -1; return; }
    for (k = 0; k < SONG_STEPS_BAR; k++)
        if (mel[bar * SONG_STEPS_BAR + k] >= 2) p->onsets++;
    p->line_syl = (uint8_t)concert_syllables(p->song->lines[(int)p->line], p->syl_start,
                                             CONCERT_SYL_MAX);
    p->singing = p->cast ? p->cast->line_mask[(int)p->line] : 1u;
}

static int step_event(ConcertPlayer *p)
{
    const ConcertSong *s = p->song;
    const ConcertGenre *g = &concert_genres[s->genre];
    int ev = PEV_STEP, bar, sec, st = p->step % SONG_STEPS_BAR, d;
    const uint8_t *mel;
    sec = sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &bar);
    if (sec == SEC_END) return PEV_END;
    if ((st & 3) == 0) ev |= PEV_BEAT;
    if (st == 0) {
        ev |= PEV_BAR;
        if (bar == 0) ev |= PEV_SECTION;
        karaoke_bar(p, sec, bar);
    }

    /* melody + lyrics */
    mel = melody_at(s, sec);
    if (mel) {
        uint8_t v = mel[bar * SONG_STEPS_BAR + st];
        if (v >= 2) {
            p->lead_note = v;
            ev |= PEV_ONSET;
            p->onset_flash = 6;
            p->harm_note = 0;
            /* which syllable starts on this onset (spread over the bar) */
            if (p->line >= 0 && p->line_syl) {
                int on_idx = 0, k, syl0;
                uint8_t vtmp[VOICE_COUNT];
                const uint8_t *voice;
                const char *ln = s->lines[(int)p->line];
                for (k = 0; k < st; k++)
                    if (mel[bar * SONG_STEPS_BAR + k] >= 2) on_idx++;
                syl0 = p->onsets ? on_idx * p->line_syl / p->onsets : 0;
                syl0 = DOOMO_MIN(syl0, p->line_syl - 1);
                p->sung = (uint8_t)(p->onsets ? ((on_idx + 1) * p->line_syl + p->onsets - 1) / p->onsets
                                              : p->line_syl);
                if (p->sung > p->line_syl) p->sung = p->line_syl;
                voice = syllable_voice(p, p->singing, vtmp);
                /* the syllable's vowel colours the sung tone */
                {
                    int base = g->lead_wave, j = p->syl_start[syl0];
                    const uint8_t *lv = lead_voice(p);
                    if (lv) {
                        int q = lv[VOICE_QUALITY];
                        if (q < 8) base = WAVE_TRIANGLE;
                        else if (q > 26) base = WAVE_PULSE12;
                    }
                    while (ln[j] && ln[j] != ' ' && !strchr("aeiouyAEIOUY", ln[j])) j++;
                    p->lead_wave = (uint8_t)vowel_wave(base, ln[j] ? ln[j] : 'a');
                }
                if (voice && opt_get(OPT_VOICE_BABBLE))
                    audio_voice_char(voice, ln[p->syl_start[syl0]]);
                /* choir harmony: a third below the tune */
                if (concert_mask_count(p->singing) >= 2) {
                    d = midi_deg(s, g->scale, v);
                    if (d > -99) p->harm_note = (uint8_t)deg_midi(s->key, g->scale, d - 2);
                }
            } else {
                p->lead_wave = g->lead_wave;
            }
            lead_tick(p, 1);
        } else if (v == 0) {
            p->lead_note = 0;
            p->harm_note = 0;
        }
    } else {
        p->lead_note = 0;
        p->harm_note = 0;
    }

    /* bass */
    {
        int bn = bass_at(s, g, sec, bar, st, 0);
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
        int cn = chord_step_at(s, g, sec, bar, st, 0);
        if (cn) {
            p->chord_vol = 9;
            p->chord_note = (uint8_t)cn;
            if (!p->harm_note && !audio_voice_busy())
                tone(1, concert_note_hz(cn), 9, g->chord_wave);
        }
    }

    /* drums */
    {
        int dk = drum_at(g, sec, bar, st);
        if (dk) {
            p->drum_kind = (uint8_t)dk;
            p->drum_age = 0;
            if (dk == 1) { ev |= PEV_KICK; p->kick_flash = 5; }
            if (dk == 2) { ev |= PEV_SNARE; p->snare_flash = 5; }
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
    /* per-tick parts; channel 1 belongs to the voice babble while it talks */
    {
        const ConcertSong *s = p->song;
        int bar, sec = sec_of_bar(p->step / SONG_STEPS_BAR, p->encore, &bar);
        int c = chord_at(s, sec, bar);
        int r = deg_midi(s->key, g->scale, c) - 12;
        if (audio_voice_busy()) {
            /* the singer is babbling a syllable on channel 1 */
        } else if (p->harm_note && p->lead_note) {
            int n = concert_mask_count(p->singing);
            int hz = concert_note_hz(p->harm_note);
            if (p->lead_age > 4) hz += ui_wave(p->ticks + 3, 7, hz / 90);
            tone(1, hz, DOOMO_MIN(6 + n / 3, 11) - DOOMO_MIN(p->lead_age, 3),
                 n >= 8 ? WAVE_PULSE50 : WAVE_PULSE25);
        } else if (g->chord_style == CH_ARP && sec != SEC_END) {
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
        } else {
            tone(1, 0, 0, g->chord_wave);
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

void concert_play_start(ConcertPlayer *p, const ConcertSong *s, const ConcertCast *cast,
                        int encore)
{
    int bpm = s->bpm ? s->bpm : 120;
    memset(p, 0, sizeof(*p));
    p->song = s;
    p->cast = cast;
    p->encore = (uint8_t)(encore ? 1 : 0);
    if (encore) bpm += bpm / 10;                 /* the encore is faster */
    p->step_len = (uint16_t)(450u * 256u / (unsigned)bpm);
    p->total = (uint16_t)((encore ? SONG_ENCORE_BARS : SONG_BARS) * SONG_STEPS_BAR);
    p->phase = p->step_len;                      /* step 0 fires at once */
    p->line = -1;
    p->step = 0xFFFF;
    p->lead_wave = concert_genres[s->genre].lead_wave;
}

void concert_restore_music(void)
{
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter >= 3 &&
        doomo_town.story.chapter < 4)
        audio_music(SONG_INVASION);
    else
        audio_music(town_is_night() ? SONG_HAHALAND_NIGHT : SONG_HAHALAND_DAY);
}

/* ====================================================================== */
/* the Music Studio export                                                  */
void concert_song_export(const ConcertSong *s, const ConcertCast *c, DoomoSong *out)
{
    const ConcertGenre *g = &concert_genres[s->genre];
    int p, st, i;
    static const uint8_t drum_map[6] = { SONG_DRUM_NONE, SONG_DRUM_KICK, SONG_DRUM_SNARE,
                                         SONG_DRUM_HAT, SONG_DRUM_OPEN_HAT, SONG_DRUM_COWBELL };
    memset(out, 0, sizeof(*out));
    memcpy(out->title, s->title, SONG_TITLE_LEN - 1);
    out->flags = (uint8_t)(SONGF_VALID | (s->edgy || g->edgy ? SONGF_HELLISH : 0));
    out->genre = s->genre;
    out->bpm = (uint8_t)DOOMO_CLAMP(s->bpm, 60, 220);
    out->key = (uint8_t)(s->key % 12);
    switch (g->scale) {
    case CSCALE_MAJOR: case CSCALE_MIXO:
        out->scale = g->pent ? SONG_SCALE_PENTATONIC : SONG_SCALE_MAJOR; break;
    case CSCALE_HARMONIC: out->scale = SONG_SCALE_HARMONIC_MINOR; break;
    default: out->scale = g->pent ? SONG_SCALE_BLUES : SONG_SCALE_MINOR; break;
    }
    out->swing = (uint8_t)(s->genre == 5 ? 3 : 0);       /* jazz swings */
    out->wave[0] = g->lead_wave;
    out->wave[1] = g->chord_wave;
    out->wave[2] = g->bass_wave;
    out->wave[3] = WAVE_NOISE;
    out->vol[0] = 13; out->vol[1] = 9; out->vol[2] = 12; out->vol[3] = 12;
    /* verse a, verse b, chorus a, chorus b; played verse-chorus twice */
    out->order_len = 8;
    for (i = 0; i < 8; i++) out->order[i] = (uint8_t)(i & 3);
    for (p = 0; p < SONG_PATTERNS; p++) {
        int sec = p < 2 ? SEC_VERSE1 : SEC_CHORUS1;
        const uint8_t *mel = s->mel[p < 2 ? 0 : 1];
        int last_bass = 0, last_ch = 0;
        for (st = 0; st < SONG_STEPS; st++) {
            int bar = (p & 1) * 2 + st / SONG_STEPS_BAR, sb = st % SONG_STEPS_BAR, n;
            out->notes[p][0][st] = mel[bar * SONG_STEPS_BAR + sb];
            if (st == 0 && out->notes[p][0][st] == SONG_HOLD) out->notes[p][0][st] = SONG_REST;
            n = chord_step_at(s, g, sec, bar, sb, 1);
            if (n) { out->notes[p][1][st] = (uint8_t)n; last_ch = n; }
            else out->notes[p][1][st] = (uint8_t)((last_ch && (g->chord_style == CH_PAD ||
                                                  g->chord_style == CH_ALBERTI)) ? SONG_HOLD : SONG_REST);
            n = bass_at(s, g, sec, bar, sb, -1);
            if (n) { out->notes[p][2][st] = (uint8_t)n; last_bass = n; }
            else out->notes[p][2][st] = (uint8_t)((last_bass && (g->bass_style == BS_LONG ||
                                                   g->bass_style == BS_ROOTS)) ? SONG_HOLD : SONG_REST);
            out->notes[p][3][st] = drum_map[drum_at(g, sec, bar, sb)];
        }
    }
    for (i = 0; i < SONG_LYRIC_LINES && i < CONCERT_LINES; i++)
        memcpy(out->lyrics[i], s->lines[i], SONG_LYRIC_LEN - 1);
    for (i = 0; i < SONG_SINGERS; i++)
        out->singer[i] = (uint16_t)(c && i < c->n ? c->ids[i] : (i == 0 ? s->singer : SONG_NO_SINGER));
    for (i = 0; i < SONG_LYRIC_LINES; i++)
        out->line_singers[i] = c ? c->line_mask[i] : 1u;
    out->choir_layout = (uint8_t)(c ? c->layout : CHOIR_RISERS);
    out->seed = s->seed;
}

/* ====================================================================== */
/* singles: 16-bit codes, two per progress counter (stored inverted so an
 * untouched counter reads as empty slots)                                   */
uint16_t concert_single_pack(int singer, int genre, int take)
{
    return (uint16_t)((singer & 1023) | ((genre & 15) << 10) | ((take & 3) << 14));
}

int concert_single_unpack(uint16_t code, ConcertSingle *out)
{
    int singer = code & 1023;
    if (singer > DOOMO_PLAYER_ID) return 0;
    if (out) {
        out->singer = (int16_t)singer;
        out->genre = (uint8_t)((code >> 10) & 15);
        out->take = (uint8_t)((code >> 14) & 3);
    }
    return 1;
}

static uint16_t slot_code(int i)
{
    uint32_t v = progress_get(i < 2 ? PROG_HIT_SINGLES : PROG_HIT_SINGLES2);
    return (uint16_t)(((i & 1) ? (v >> 16) : v) ^ 0xFFFFu);
}

static void slot_store(const uint16_t *codes)
{
    uint32_t a = (uint32_t)(codes[0] ^ 0xFFFFu) | ((uint32_t)(codes[1] ^ 0xFFFFu) << 16);
    uint32_t b = (uint32_t)(codes[2] ^ 0xFFFFu) | ((uint32_t)(codes[3] ^ 0xFFFFu) << 16);
    if (progress_get(PROG_HIT_SINGLES) != a) progress_set(PROG_HIT_SINGLES, a);
    if (progress_get(PROG_HIT_SINGLES2) != b) progress_set(PROG_HIT_SINGLES2, b);
}

int concert_single_get(int i, ConcertSingle *out)
{
    if (i < 0 || i >= CONCERT_SINGLES) return 0;
    return concert_single_unpack(slot_code(i), out);
}

int concert_single_count(void)
{
    int i, n = 0;
    for (i = 0; i < CONCERT_SINGLES; i++) n += concert_single_get(i, NULL);
    return n;
}

int concert_single_save(int singer, int genre, int take)
{
    uint16_t codes[CONCERT_SINGLES], code = concert_single_pack(singer, genre, take);
    int i, k = 1;
    codes[0] = code;
    for (i = 0; i < CONCERT_SINGLES && k < CONCERT_SINGLES; i++) {
        uint16_t c = slot_code(i);
        if (!concert_single_unpack(c, NULL) || c == code) continue;
        codes[k++] = c;
    }
    while (k < CONCERT_SINGLES) codes[k++] = 0xFFFFu;
    slot_store(codes);
    return 0;
}

void concert_single_remove(int i)
{
    uint16_t codes[CONCERT_SINGLES];
    int j, k = 0;
    for (j = 0; j < CONCERT_SINGLES; j++) {
        uint16_t c = slot_code(j);
        if (j == i || !concert_single_unpack(c, NULL)) continue;
        codes[k++] = c;
    }
    while (k < CONCERT_SINGLES) codes[k++] = 0xFFFFu;
    slot_store(codes);
}
