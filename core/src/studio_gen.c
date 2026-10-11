/* studio_gen.c - the Music Studio's composer, lyric writer, templates and
 * choir helpers (owner: studio).
 *
 * COMPOSER. A song is 4 patterns of 2 bars (32 16th steps) with 4 chords
 * each (one per half bar):  A = verse, B = chorus (higher, hookier, crash
 * on the downbeat), C = bridge (the verse motif inverted on a new
 * progression), D = intro / outro (the chorus hook thinned out). The lead
 * melody is built from a one-bar MOTIF (rhythm from a genre-flavoured
 * onset priority list + a contour of scale steps) and its VARIATION in
 * bar 2 (same contour moved to the new chords, an earlier phrase ending
 * held to the bar line). Strong beats land on chord tones, weak ones walk
 * by scale steps. The number of onsets per pattern follows the lyrics
 * already in the song (the longest line sung on it), so templates and
 * Inspire me songs fit their words; shorter lines get "la"s from
 * studio_autofit(). Harmony (arpeggio / pad / stabs / alberti / strum /
 * power fifths / offbeats), bass (roots / octaves / walking / eighths /
 * long / syncopated) and drums (16 genre grooves + fills) follow the
 * genre's style row. Everything is deterministic from (genre, seed).
 *
 * LYRICS come from a citizen: name, favourite food, hobby, genre, fear,
 * catchphrase, best friend, crush, district and job. Every line is
 * checked against the 27-character limit (templates are tried in turn).
 */
#include "studio_int.h"

/* ====================================================================== */
/* genre styles                                                             */
enum { DR_NONE, DR_POP, DR_HIPHOP, DR_CITY, DR_CHIP, DR_LULL, DR_JAZZ, DR_FOLK,
       DR_DISCO, DR_SHANTY, DR_SYNTH, DR_GOTH, DR_PUNK, DR_DOOM, DR_INDUS, DR_HYMN };
enum { BS_ROOTS, BS_OCT8, BS_WALK, BS_EIGHTHS, BS_LONG, BS_SYNC };
enum { HM_ARP, HM_PAD, HM_STAB, HM_ALBERTI, HM_STRUM, HM_POWER, HM_OFFBEAT };

typedef struct GenreStyle {
    uint8_t bpm_lo, bpm_hi, scale, swing;
    uint8_t wave[4], vol[4];
    uint8_t drums, bass, harm, density;
    uint8_t lead_lo;          /* lowest lead note relative to C (MIDI)   */
    uint8_t stacc;            /* 1 = short detached lead notes           */
} GenreStyle;

#define P12 WAVE_PULSE12
#define P25 WAVE_PULSE25
#define P50 WAVE_PULSE50
#define TRI WAVE_TRIANGLE
#define SAW WAVE_SAW
static const GenreStyle styles[MUSIC_COUNT] = {
    /* Bubblegum Pop */ { 118, 132, SONG_SCALE_MAJOR, 0, { P50, P25, TRI, SONG_KIT_TOY },
                          { 12, 7, 12, 10 }, DR_POP, BS_OCT8, HM_ARP, 2, 72, 0 },
    /* Lo-fi Beats   */ { 78, 90, SONG_SCALE_PENTATONIC, 5, { TRI, P25, TRI, SONG_KIT_808 },
                          { 11, 6, 13, 11 }, DR_HIPHOP, BS_SYNC, HM_PAD, 1, 67, 0 },
    /* City Pop      */ { 104, 116, SONG_SCALE_MAJOR, 0, { P25, P50, TRI, SONG_KIT_CLASSIC },
                          { 12, 6, 12, 10 }, DR_CITY, BS_WALK, HM_STAB, 2, 69, 0 },
    /* Chiptune      */ { 140, 160, SONG_SCALE_MAJOR, 0, { P12, P25, TRI, SONG_KIT_TOY },
                          { 12, 7, 12, 9 }, DR_CHIP, BS_OCT8, HM_ARP, 3, 72, 1 },
    /* Lullabies     */ { 66, 80, SONG_SCALE_PENTATONIC, 0, { TRI, P50, TRI, SONG_KIT_PLUSH },
                          { 11, 5, 10, 6 }, DR_LULL, BS_LONG, HM_ARP, 0, 67, 0 },
    /* Jazz          */ { 100, 120, SONG_SCALE_MAJOR, 7, { P25, P50, TRI, SONG_KIT_CLASSIC },
                          { 12, 6, 12, 8 }, DR_JAZZ, BS_WALK, HM_STAB, 2, 67, 0 },
    /* Classical     */ { 90, 108, SONG_SCALE_HARMONIC_MINOR, 0, { P50, P25, TRI, SONG_KIT_PLUSH },
                          { 12, 6, 11, 6 }, DR_NONE, BS_LONG, HM_ALBERTI, 2, 72, 0 },
    /* Folk          */ { 100, 112, SONG_SCALE_MAJOR, 2, { TRI, P25, TRI, SONG_KIT_BONES },
                          { 12, 7, 12, 9 }, DR_FOLK, BS_ROOTS, HM_STRUM, 2, 67, 0 },
    /* Disco         */ { 116, 124, SONG_SCALE_MINOR, 0, { P25, P50, SAW, SONG_KIT_808 },
                          { 12, 6, 12, 11 }, DR_DISCO, BS_OCT8, HM_OFFBEAT, 2, 69, 0 },
    /* Sea Shanties  */ { 96, 108, SONG_SCALE_MINOR, 3, { P50, P25, TRI, SONG_KIT_BONES },
                          { 12, 7, 12, 10 }, DR_SHANTY, BS_ROOTS, HM_STRUM, 2, 64, 0 },
    /* Hymns         */ { 70, 84, SONG_SCALE_MAJOR, 0, { TRI, P50, TRI, SONG_KIT_PLUSH },
                          { 12, 7, 11, 5 }, DR_HYMN, BS_LONG, HM_PAD, 0, 67, 0 },
    /* Synthwave     */ { 96, 110, SONG_SCALE_MINOR, 0, { SAW, P25, P50, SONG_KIT_808 },
                          { 11, 6, 12, 11 }, DR_SYNTH, BS_OCT8, HM_ARP, 2, 69, 0 },
    /* Goth Rock     */ { 120, 136, SONG_SCALE_HARMONIC_MINOR, 0, { P12, SAW, TRI, SONG_KIT_CLASSIC },
                          { 12, 6, 12, 11 }, DR_GOTH, BS_EIGHTHS, HM_PAD, 2, 64, 0 },
    /* Hardcore Punk */ { 168, 196, SONG_SCALE_MINOR, 0, { P50, SAW, P25, SONG_KIT_FORGE },
                          { 13, 7, 12, 12 }, DR_PUNK, BS_EIGHTHS, HM_POWER, 3, 64, 1 },
    /* Doom Metal    */ { 62, 76, SONG_SCALE_HARMONIC_MINOR, 0, { SAW, SAW, TRI, SONG_KIT_FORGE },
                          { 12, 7, 14, 12 }, DR_DOOM, BS_LONG, HM_POWER, 1, 57, 0 },
    /* Industrial    */ { 128, 140, SONG_SCALE_BLUES, 0, { P12, P25, SAW, SONG_KIT_FORGE },
                          { 12, 6, 12, 12 }, DR_INDUS, BS_EIGHTHS, HM_OFFBEAT, 2, 64, 1 },
};
#undef P12
#undef P25
#undef P50
#undef TRI
#undef SAW

/* one bar (16 steps) of drums: bit n = step n */
typedef struct { uint16_t kick, snare, hat, ohat, clap, tom, cow; uint8_t fill; } Groove;
static const Groove grooves[] = {
    /* NONE   */ { 0, 0, 0, 0, 0, 0, 0, 0 },
    /* POP    */ { 0x0501, 0x1010, 0x5555, 0x0000, 0x1000, 0, 0, 1 },
    /* HIPHOP */ { 0x0481, 0x1010, 0x5555, 0x4000, 0, 0, 0, 0 },
    /* CITY   */ { 0x0101, 0x1010, 0x5555, 0x4040, 0, 0, 0, 1 },
    /* CHIP   */ { 0x0101, 0x1010, 0xFFFF, 0, 0, 0, 0, 1 },
    /* LULL   */ { 0x0001, 0, 0x1100, 0, 0, 0, 0, 0 },
    /* JAZZ   */ { 0x0001, 0x0400, 0x5151, 0, 0, 0, 0, 0 },
    /* FOLK   */ { 0x0101, 0, 0x4444, 0, 0x1010, 0, 0, 1 },
    /* DISCO  */ { 0x1111, 0, 0xAAAA, 0x4444, 0x1010, 0, 0, 1 },
    /* SHANTY */ { 0x0101, 0, 0, 0, 0x1010, 0x4000, 0, 1 },
    /* SYNTH  */ { 0x0901, 0x1010, 0x5555, 0, 0, 0, 0, 1 },
    /* GOTH   */ { 0x0141, 0x1010, 0x5555, 0, 0, 0x4400, 0, 1 },
    /* PUNK   */ { 0x0505, 0x5050, 0xAAAA, 0, 0, 0, 0, 1 },
    /* DOOM   */ { 0x0001, 0x0100, 0, 0x1010, 0, 0x8800, 0, 1 },
    /* INDUS  */ { 0x1111, 0, 0xAAAA, 0, 0x4040, 0, 0x0404, 1 },
    /* HYMN   */ { 0x0001, 0, 0, 0, 0, 0, 0, 0 },
};

/* chord progressions (scale degrees, one per half bar) */
static const uint8_t prog_major[][4] = {
    { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 5, 3, 0, 4 }, { 0, 3, 4, 0 }, { 0, 2, 3, 4 },
    { 3, 4, 0, 5 }, { 0, 3, 0, 4 }, { 3, 0, 4, 4 },
};
static const uint8_t prog_minor[][4] = {
    { 0, 5, 2, 6 }, { 0, 3, 4, 0 }, { 0, 6, 5, 4 }, { 0, 0, 5, 6 }, { 5, 6, 0, 0 },
    { 0, 3, 6, 2 }, { 3, 4, 0, 0 },
};

/* ====================================================================== */
/* helpers                                                                  */
static int deg_note(const DoomoSong *s, int base, int deg)
{
    return song_scale_shift(s, base, deg);
}

static int is_minorish(int scale)
{
    return scale == SONG_SCALE_MINOR || scale == SONG_SCALE_HARMONIC_MINOR ||
           scale == SONG_SCALE_BLUES;
}

static int line_syls(const DoomoSong *s, int line)
{
    return s->lyrics[line][0] ? song_syllables(s->lyrics[line], NULL, 0) : 0;
}

int studio_line_pos(const DoomoSong *s, int line)
{
    int p;
    for (p = 0; p < s->order_len && p < SONG_ORDER_MAX; p++)
        if (SONG_LINE_OF(p) == line) return p;
    return -1;
}

/* the most syllables of a lyric line sung over pattern `pat` (0 = none) */
static int pattern_words(const DoomoSong *s, int pat)
{
    int p, best = 0;
    for (p = 0; p < s->order_len && p < SONG_ORDER_MAX; p++)
        if (s->order[p] == pat) best = DOOMO_MAX(best, line_syls(s, SONG_LINE_OF(p)));
    return best;
}

/* onset positions of one bar: the first k of a genre-flavoured priority
 * list (beats, then 8ths, then 16ths), limited to < `last` */
static uint16_t bar_rhythm(DoomoRng *r, int k, int last, int sync)
{
    static const uint8_t prio[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
    uint8_t order[16];
    uint16_t m = 0;
    int i, n = 0;
    memcpy(order, prio, sizeof(order));
    /* a little shuffle among equals keeps motifs from sounding the same */
    for (i = 2; i < 15; i++)
        if (rng_chance(r, 30 + sync * 15)) { uint8_t t = order[i]; order[i] = order[i + 1]; order[i + 1] = t; }
    if (sync && rng_chance(r, 50)) {                                 /* syncopation */
        int a = 0, b = 0;
        for (i = 0; i < 16; i++) { if (order[i] == 4) a = i; if (order[i] == 6) b = i; }
        order[a] = 6; order[b] = 4;
    }
    for (i = 0; i < 16 && n < k; i++) {
        if (order[i] >= last) continue;
        m |= (uint16_t)(1u << order[i]);
        n++;
    }
    return m | 1u;                         /* phrases start on the downbeat */
}

static int popc16(unsigned v) { int n = 0; while (v) { v &= v - 1; n++; } return n; }

/* ====================================================================== */
/* the lead melody of one pattern                                           */
typedef struct {
    int8_t  contour[16];      /* scale-step moves of the motif               */
    uint16_t rhythm;
} Motif;

static void make_motif(Motif *m, DoomoRng *r, int k, int sync)
{
    int i;
    m->rhythm = bar_rhythm(r, k, 16, sync);
    for (i = 0; i < 16; i++) {
        static const int8_t moves[8] = { 1, -1, 2, -1, 0, 1, -2, 3 };
        m->contour[i] = moves[rng_range(r, 8)];
    }
}

static int nearest_chord_tone(int prev, int chord, int lo, int hi)
{
    static const int8_t tones[7] = { 0, 2, 4, 7, 9, 11, -3 };
    int i, best = prev, bd = 99;
    for (i = 0; i < 7; i++) {
        int d = chord + tones[i], dist;
        while (d < lo) d += 7;
        while (d > hi) d -= 7;
        dist = d > prev ? d - prev : prev - d;
        if (dist < bd) { bd = dist; best = d; }
    }
    return best;
}

static void write_lead(DoomoSong *s, int pat, const uint8_t *chords, const Motif *mo,
                       int onsets, int lift, int invert, int stacc, int base, DoomoRng *r)
{
    uint8_t *c = s->notes[pat][0];
    uint16_t bars[2];
    int b, i, deg, k1, k2, lo = lift, hi = 9 + lift, prev_on = -1, mi = 0;
    memset(c, 0, SONG_STEPS);
    onsets = DOOMO_CLAMP(onsets, 3, 16);
    k1 = (onsets + 1) / 2;
    k2 = onsets - k1;
    if (k2 > 8) { k1 += k2 - 8; k2 = 8; }
    if (k1 > 9) k1 = 9;
    /* bar 1: the motif's rhythm trimmed / grown to k1 onsets */
    bars[0] = mo->rhythm;
    while (popc16(bars[0]) > k1) {
        for (i = 15; i > 0; i--) if (bars[0] & (1u << i)) { bars[0] &= (uint16_t)~(1u << i); break; }
    }
    if (popc16(bars[0]) < k1) bars[0] |= bar_rhythm(r, k1, 16, 0);
    while (popc16(bars[0]) > k1) {
        for (i = 15; i > 0; i--) if (bars[0] & (1u << i)) { bars[0] &= (uint16_t)~(1u << i); break; }
    }
    /* bar 2: the variation ends early and holds (a phrase ending) */
    bars[1] = (uint16_t)(mo->rhythm & 0x07FF);
    while (popc16(bars[1]) > k2) {
        for (i = 15; i > 0; i--) if (bars[1] & (1u << i)) { bars[1] &= (uint16_t)~(1u << i); break; }
    }
    if (popc16(bars[1]) < k2) bars[1] |= bar_rhythm(r, k2, 12, 0);
    while (popc16(bars[1]) > k2 && bars[1] > 1) {
        for (i = 15; i > 0; i--) if (bars[1] & (1u << i)) { bars[1] &= (uint16_t)~(1u << i); break; }
    }
    if (k2 == 0) bars[1] = 0;
    deg = nearest_chord_tone(lift + 2, chords[0], lo, hi);
    for (b = 0; b < 2; b++) {
        mi = 0;
        for (i = 0; i < 16; i++) {
            int step = b * 16 + i, chord = chords[step / 8], strong;
            if (!(bars[b] & (1u << i))) continue;
            strong = (i % 8) == 0 || ((i % 4) == 0 && rng_chance(r, 50));
            if (prev_on >= 0) {
                int mv = mo->contour[mi % 16];
                if (invert) mv = -mv;
                deg += mv;
                if (deg < lo) deg = lo + (lo - deg) % 3;
                if (deg > hi) deg = hi - (deg - hi) % 3;
                if (strong) deg = nearest_chord_tone(deg, chord, lo, hi);
            }
            mi++;
            /* the very last note of the phrase resolves home-ish */
            if (b == 1 && !(bars[1] >> (i + 1)))
                deg = nearest_chord_tone(deg, chords[3], lo, DOOMO_MIN(hi, lift + 7));
            c[step] = (uint8_t)deg_note(s, base, deg);
            prev_on = step;
        }
    }
    /* durations: hold until the next onset (staccato genres breathe) */
    {
        int last = -1;
        for (i = 0; i < SONG_STEPS; i++) {
            if (c[i] >= 2) { last = i; continue; }
            if (last < 0) continue;
            {
                int next = i, gap;
                while (next < SONG_STEPS && c[next] < 2) next++;
                gap = next - last;
                if (next >= SONG_STEPS) {
                    /* phrase end: hold, then breathe for the last 2 steps */
                    c[i] = (uint8_t)(i < SONG_STEPS - 2 ? SONG_HOLD : SONG_REST);
                } else if (stacc && gap >= 2 && i == next - 1) {
                    c[i] = SONG_REST;
                } else if (stacc && gap > 3 && i - last >= 2) {
                    c[i] = SONG_REST;
                } else {
                    c[i] = SONG_HOLD;
                }
            }
        }
    }
}

/* ---- harmony / bass / drums ------------------------------------------------ */
static void write_harmony(DoomoSong *s, int pat, const uint8_t *chords, int style, int base)
{
    uint8_t *c = s->notes[pat][1];
    int h, i;
    memset(c, 0, SONG_STEPS);
    for (h = 0; h < 4; h++) {
        int ch = chords[h], o = h * 8;
        int r0 = deg_note(s, base, ch), r3 = deg_note(s, base, ch + 2), r5 = deg_note(s, base, ch + 4);
        int r8 = DOOMO_MIN(r0 + 12, 127);
        switch (style) {
        case HM_ARP: {
            const int seq[4] = { r0, r3, r5, r3 };
            for (i = 0; i < 4; i++) { c[o + i * 2] = (uint8_t)seq[i]; c[o + i * 2 + 1] = SONG_HOLD; }
            break; }
        case HM_PAD:
            c[o] = (uint8_t)r3;
            for (i = 1; i < 8; i++) c[o + i] = SONG_HOLD;
            break;
        case HM_STAB:
            c[o + 2] = (uint8_t)r3; c[o + 6] = (uint8_t)r5;
            break;
        case HM_ALBERTI: {
            const int seq[4] = { r0, r5, r3, r5 };
            for (i = 0; i < 8; i++) c[o + i] = (uint8_t)seq[i & 3];
            break; }
        case HM_STRUM:
            c[o] = (uint8_t)r3; c[o + 1] = SONG_HOLD; c[o + 3] = (uint8_t)r5;
            c[o + 4] = (uint8_t)r8; c[o + 5] = SONG_HOLD; c[o + 6] = (uint8_t)r5;
            break;
        case HM_POWER:
            c[o] = (uint8_t)DOOMO_MIN(r0 + 7, 127);
            for (i = 1; i < 8; i++) c[o + i] = (uint8_t)(i == 4 ? DOOMO_MIN(r0 + 7, 127) : SONG_HOLD);
            break;
        default: /* HM_OFFBEAT */
            c[o + 2] = (uint8_t)r5; c[o + 6] = (uint8_t)r3;
            break;
        }
    }
}

static void write_bass(DoomoSong *s, int pat, const uint8_t *chords, int style, int base)
{
    uint8_t *c = s->notes[pat][2];
    int h, i;
    memset(c, 0, SONG_STEPS);
    for (h = 0; h < 4; h++) {
        int ch = chords[h], o = h * 8;
        int r0 = deg_note(s, base, ch), r5 = deg_note(s, base, ch + 4);
        int r8 = DOOMO_MIN(r0 + 12, 127);
        int next = deg_note(s, base, chords[(h + 1) & 3]);
        switch (style) {
        case BS_ROOTS:
            c[o] = (uint8_t)r0; c[o + 1] = SONG_HOLD; c[o + 2] = SONG_HOLD;
            c[o + 4] = (uint8_t)r5; c[o + 5] = SONG_HOLD; c[o + 6] = SONG_HOLD;
            break;
        case BS_OCT8:
            for (i = 0; i < 4; i++) c[o + i * 2] = (uint8_t)(i & 1 ? r8 : r0);
            break;
        case BS_WALK:
            c[o] = (uint8_t)r0; c[o + 1] = SONG_HOLD;
            c[o + 2] = (uint8_t)deg_note(s, base, ch + 2); c[o + 3] = SONG_HOLD;
            c[o + 4] = (uint8_t)r5; c[o + 5] = SONG_HOLD;
            c[o + 6] = (uint8_t)DOOMO_MAX(next + (next > r5 ? -1 : 1), 2); c[o + 7] = SONG_HOLD;
            break;
        case BS_EIGHTHS:
            for (i = 0; i < 4; i++) { c[o + i * 2] = (uint8_t)r0; c[o + i * 2 + 1] = SONG_HOLD; }
            break;
        case BS_LONG:
            c[o] = (uint8_t)r0;
            for (i = 1; i < 8; i++) c[o + i] = SONG_HOLD;
            break;
        default: /* BS_SYNC */
            c[o] = (uint8_t)r0; c[o + 1] = SONG_HOLD; c[o + 2] = SONG_HOLD;
            c[o + 3] = (uint8_t)r0; c[o + 6] = (uint8_t)r8;
            break;
        }
    }
}

static void write_drums(DoomoSong *s, int pat, int groove, int role)
{
    const Groove *g = &grooves[groove];
    uint8_t *c = s->notes[pat][3];
    int i;
    memset(c, 0, SONG_STEPS);
    for (i = 0; i < SONG_STEPS; i++) {
        int b = i & 15, d = 0;
        uint16_t bit = (uint16_t)(1u << b);
        if (role == 3 && i < 16 && !(g->kick & bit)) continue;    /* intro: build up */
        if (g->cow & bit) d = SONG_DRUM_COWBELL;
        if (g->hat & bit) d = SONG_DRUM_HAT;
        if (g->ohat & bit) d = SONG_DRUM_OPEN_HAT;
        if (g->tom & bit) d = SONG_DRUM_TOM;
        if (g->clap & bit) d = SONG_DRUM_CLAP;
        if (g->snare & bit) d = SONG_DRUM_SNARE;
        if (g->kick & bit) d = SONG_DRUM_KICK;
        c[i] = (uint8_t)d;
    }
    if (g->fill && role != 3) {
        /* a fill into the next pattern */
        c[28] = SONG_DRUM_TOM; c[29] = SONG_DRUM_TOM;
        c[30] = SONG_DRUM_SNARE; c[31] = SONG_DRUM_SNARE;
    }
    if (role == 1 && groove != DR_NONE) c[0] = SONG_DRUM_CRASH;    /* chorus! */
    if (groove == DR_HYMN && role == 1) c[0] = SONG_DRUM_CRASH;
}

/* ====================================================================== */
/* compose                                                                  */
void studio_compose(DoomoSong *s, int genre, uint32_t seed)
{
    const GenreStyle *st;
    DoomoRng r;
    const uint8_t *pa, *pb, *pc;
    Motif mv, mc;
    int base, bass, harm_base, words[4], i, k, keep_order = s->order_len > 1;
    genre = DOOMO_CLAMP(genre, 0, MUSIC_COUNT - 1);
    st = &styles[genre];
    rng_seed(&r, seed * 2654435761u + (uint32_t)genre * 977u + 13u);
    s->genre = (uint8_t)genre;
    if (!s->bpm || !(s->flags & SONGF_CUSTOM) || !keep_order)
        s->bpm = (uint8_t)rng_between(&r, st->bpm_lo, st->bpm_hi);
    s->scale = st->scale;
    s->swing = st->swing;
    memcpy(s->wave, st->wave, sizeof(s->wave));
    memcpy(s->vol, st->vol, sizeof(s->vol));
    s->seed = seed ? seed : 1;
    if (!keep_order) {
        static const uint8_t ord_std[8] = { 3, 0, 0, 1, 0, 1, 2, 1 };
        memcpy(s->order, ord_std, sizeof(ord_std));
        s->order_len = 8;
    }
    /* chords: three different progressions */
    {
        int minor = is_minorish(st->scale);
        int n = minor ? DOOMO_COUNTOF(prog_minor) : DOOMO_COUNTOF(prog_major);
        int ia = rng_range(&r, n), ib = (ia + 1 + rng_range(&r, n - 1)) % n, ic = rng_range(&r, n);
        pa = minor ? prog_minor[ia] : prog_major[ia];
        pb = minor ? prog_minor[ib] : prog_major[ib];
        pc = minor ? prog_minor[ic] : prog_major[ic];
    }
    /* registers: lead base near the genre's range in the song's key */
    base = song_scale_snap(s, st->lead_lo + s->key % 12 - (s->key % 12 > 5 ? 12 : 0));
    while ((base - s->key) % 12) base--;
    bass = base - 24 >= 28 ? base - 24 : base - 12;
    harm_base = base - 12;
    /* onsets: follow the words, else the genre density */
    for (i = 0; i < 4; i++) {
        words[i] = pattern_words(s, i);
        if (!words[i]) words[i] = 5 + st->density * 2 + rng_range(&r, 3);
        words[i] = DOOMO_CLAMP(words[i], 4, 16);
    }
    k = DOOMO_CLAMP((words[0] + 1) / 2, 2, 9);
    make_motif(&mv, &r, k, st->density >= 2);
    make_motif(&mc, &r, DOOMO_CLAMP((words[1] + 1) / 2, 2, 9), st->density >= 1);
    /* A verse, B chorus (+2 degrees), C bridge (inverted), D intro hook */
    write_lead(s, 0, pa, &mv, words[0], 0, 0, st->stacc, base, &r);
    write_lead(s, 1, pb, &mc, words[1], 2, 0, st->stacc, base, &r);
    write_lead(s, 2, pc, &mv, words[2], 1, 1, st->stacc, base, &r);
    write_lead(s, 3, pb, &mc, DOOMO_MIN(words[3], 8), 2, 0, 1, base, &r);
    write_harmony(s, 0, pa, st->harm, harm_base);
    write_harmony(s, 1, pb, st->harm, harm_base);
    write_harmony(s, 2, pc, st->harm, harm_base);
    write_harmony(s, 3, pb, st->harm == HM_PAD ? HM_ARP : st->harm, harm_base);
    write_bass(s, 0, pa, st->bass, bass);
    write_bass(s, 1, pb, st->bass, bass);
    write_bass(s, 2, pc, st->bass, bass);
    write_bass(s, 3, pb, st->bass == BS_LONG ? BS_LONG : BS_ROOTS, bass);
    write_drums(s, 0, st->drums, 0);
    write_drums(s, 1, st->drums, 1);
    write_drums(s, 2, st->drums, 2);
    write_drums(s, 3, st->drums, 3);
    /* the intro hook is the instrument's moment: thinner on lyrics */
    for (i = 0; i < SONG_LYRIC_LINES; i++) studio_autofit(s, i);
}

/* ====================================================================== */
/* lyrics                                                                   */
static void lower_into(char *dst, int cap, const char *src)
{
    int i;
    for (i = 0; i < cap - 1 && src[i]; i++)
        dst[i] = (src[i] >= 'A' && src[i] <= 'Z') ? (char)(src[i] + 32) : src[i];
    dst[i] = 0;
}

/* expand a line template: {N} name, {F} food, {H} hobby, {M} music genre,
 * {R} fear, {C} catchphrase, {B} best friend, {L} crush / love, {D}
 * district, {J} job. Returns 1 if it fits SONG_LYRIC_LEN. */
typedef struct { char n[DOOMO_NAME_LEN], b[DOOMO_NAME_LEN], l[DOOMO_NAME_LEN];
                 char f[24], h[24], m[24], r[24], j[24]; const char *c, *d; } Muse;

static int expand(char *out, int cap, const char *tpl, const Muse *m)
{
    int n = 0;
    memset(out, 0, (size_t)cap);
    while (*tpl) {
        const char *sub = NULL;
        if (tpl[0] == '{' && tpl[1] && tpl[2] == '}') {
            switch (tpl[1]) {
            case 'N': sub = m->n; break;
            case 'F': sub = m->f; break;
            case 'H': sub = m->h; break;
            case 'M': sub = m->m; break;
            case 'R': sub = m->r; break;
            case 'C': sub = m->c; break;
            case 'B': sub = m->b; break;
            case 'L': sub = m->l; break;
            case 'D': sub = m->d; break;
            case 'J': sub = m->j; break;
            default: sub = ""; break;
            }
            if (!sub[0]) return 0;
            while (*sub) { if (n >= cap - 1) return 0; out[n++] = *sub++; }
            tpl += 3;
            continue;
        }
        if (n >= cap - 1) return 0;
        out[n++] = *tpl++;
    }
    out[n] = 0;
    return 1;
}

static const char *const verse_tpl[] = {
    "I love {F} a lot", "{F} for breakfast", "{N} loves {H}",
    "we go {H} all day", "scared of {R} at night", "{R}? not tonight!",
    "my best friend {B}", "{B} holds my paw", "{D} is my home",
    "I'm a little {J}", "humming {M} all day", "I dream of {L}",
    "{L}, look at me", "snack on {F} with {B}", "sunny {D} skies",
    "no more {R} for me",
};
static const char *const verse_tpl_hell[] = {
    "{F} in my cauldron", "I fear {R}, mwahaha", "{N} likes {H}",
    "{B} is my minion", "{D} after dark", "I'm the {J} of doom",
    "blasting {M} loud", "{L} stole my heart", "{H} in the gloom",
    "no {R} in my lair",
};
static const char *const chorus_tpl[] = {
    "{C}", "{N}, {N}, sing along!", "hearts up, Hellvile Park!",
    "la la la, {N}!", "{F} forever!", "oh {N}, oh {N}",
};
static const char *const bridge_tpl[] = {
    "even when I'm scared", "of {R}, I still sing", "{B}, sing with me",
    "hold my paw, {L}", "the park is ours tonight",
};
static const char *const title_tpl[] = {
    "{N}'s Song", "Ode to {F}", "{F} Forever", "{R} Blues", "Sing, {N}!", "{D} Nights",
};

static void title_case(char *t)
{
    int i, up = 1;
    for (i = 0; t[i]; i++) {
        if (up && t[i] >= 'a' && t[i] <= 'z') t[i] = (char)(t[i] - 32);
        up = t[i] == ' ';
    }
}

static int first_rel(int id, int want_love)
{
    uint16_t ids[24];
    int i, n = rel_list(id, ids, 24);
    for (i = 0; i < n; i++) {
        int k = rel_kind(id, ids[i]);
        if (!st_alive(ids[i])) continue;
        if (want_love) {
            if (k == REL_DATING || k == REL_MARRIED || rel_has_crush(id, ids[i])) return ids[i];
        } else if (k == REL_BEST_FRIEND || k == REL_FRIEND || k == REL_FAMILY) {
            return ids[i];
        }
    }
    return -1;
}

static void copy_name(char *dst, const char *src)
{
    int i;
    for (i = 0; i < DOOMO_NAME_LEN - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static void muse_of(int id, Muse *m)
{
    const DoomoResident *r = town_res(id);
    int b = first_rel(id, 0), l = first_rel(id, 1);
    memset(m, 0, sizeof(*m));
    if (!r) r = town_res(DOOMO_PLAYER_ID);
    copy_name(m->n, r->name[0] ? r->name : "Fluffy");
    lower_into(m->f, sizeof(m->f), doomo_food_names[r->like_food % FOOD_COUNT]);
    lower_into(m->h, sizeof(m->h), doomo_hobby_names[r->like_hobby % HOBBY_COUNT]);
    lower_into(m->m, sizeof(m->m), doomo_music_names[r->like_music % MUSIC_COUNT]);
    lower_into(m->r, sizeof(m->r), doomo_fear_names[r->fear % FEAR_COUNT]);
    m->c = doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT];
    m->d = (id < DOOMO_MAX_RESIDENTS && RES_DISTRICT(id)) ? "Hellvile" : "Hillvile";
    copy_name(m->b, b >= 0 ? st_name(b) : "my plushie");
    copy_name(m->l, l >= 0 ? st_name(l) : "someone");
    if (id >= 0 && id < DOOMO_MAX_RESIDENTS && !(r->flags & RF_CUSTOM))
        lower_into(m->j, sizeof(m->j), roster_job(id));
    if (!m->j[0]) memcpy(m->j, "dreamer", 8);
}

static void pick_line(char *out, const char *const *tpl, int n, const Muse *m, DoomoRng *r,
                      uint32_t *used)
{
    int tries, k = rng_range(r, n);
    memset(out, 0, SONG_LYRIC_LEN);
    for (tries = 0; tries < n; tries++, k = (k + 1) % n) {
        if (*used & (1u << k)) continue;
        if (expand(out, SONG_LYRIC_LEN, tpl[k], m)) { *used |= 1u << k; return; }
    }
    memcpy(out, "la la la", 9);
}

void studio_write_lyrics(DoomoSong *s, int muse, uint32_t seed)
{
    Muse m;
    DoomoRng r;
    char chorus[SONG_LYRIC_LEN], bridge[SONG_LYRIC_LEN];
    uint32_t used_v = 0, used_c = 0, used_b = 0, used_t = 0;
    int hell = muse >= DOOMO_DISTRICT_SIZE && muse < DOOMO_MAX_RESIDENTS, p;
    rng_seed(&r, seed ^ 0x5EEDu ^ (uint32_t)muse * 31u);
    muse_of(muse, &m);
    pick_line(chorus, chorus_tpl, DOOMO_COUNTOF(chorus_tpl), &m, &r, &used_c);
    pick_line(bridge, bridge_tpl, DOOMO_COUNTOF(bridge_tpl), &m, &r, &used_b);
    memset(s->lyrics, 0, sizeof(s->lyrics));
    for (p = 0; p < s->order_len && p < SONG_LYRIC_LINES; p++) {
        char *line = s->lyrics[p];
        switch (s->order[p]) {
        case 1: memcpy(line, chorus, SONG_LYRIC_LEN); break;
        case 2: memcpy(line, bridge, SONG_LYRIC_LEN); break;
        case 3: line[0] = 0; break;                      /* the intro hook */
        default:
            if (hell && rng_chance(&r, 60))
                pick_line(line, verse_tpl_hell, DOOMO_COUNTOF(verse_tpl_hell), &m, &r, &used_v);
            else
                pick_line(line, verse_tpl, DOOMO_COUNTOF(verse_tpl), &m, &r, &used_v);
            break;
        }
        studio_clean_text(line, SONG_LYRIC_LEN);
    }
    {
        char t[SONG_LYRIC_LEN];
        pick_line(t, title_tpl, DOOMO_COUNTOF(title_tpl), &m, &r, &used_t);
        title_case(t);
        memset(s->title, 0, sizeof(s->title));
        memcpy(s->title, t, SONG_TITLE_LEN - 1);
        /* never cut a title mid-word */
        if (strlen(t) >= SONG_TITLE_LEN) {
            int k = SONG_TITLE_LEN - 1;
            while (k > 4 && s->title[k - 1] != ' ') k--;
            s->title[k > 4 ? k - 1 : SONG_TITLE_LEN - 1] = 0;
        }
    }
}

/* ====================================================================== */
/* choir                                                                    */
int st_alive(int id)
{
    const DoomoResident *r;
    if (id == DOOMO_PLAYER_ID) return 1;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    r = &doomo_town.res[id];
    return r->status != RES_EMPTY && r->status != RES_DEAD;
}

static int has_singer(const DoomoSong *s, int id)
{
    int i;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(s, i) == id) return 1;
    return 0;
}

static int add_singer(DoomoSong *s, int id)
{
    int i;
    if (!st_alive(id) || has_singer(s, id)) return 0;
    for (i = 0; i < SONG_SINGERS; i++)
        if (s->singer[i] == SONG_NO_SINGER) {
            s->singer[i] = (uint16_t)id;
            song_set_section(s, i, -1);
            return 1;
        }
    return 0;
}

static void fill_district(DoomoSong *s, int lo, int hi, int want, DoomoRng *r)
{
    int tries, got = 0;
    for (tries = 0; tries < 900 && got < want; tries++)
        got += add_singer(s, rng_between(r, lo, hi));
    /* small islands: sweep in order */
    for (tries = lo; tries <= hi && got < want; tries++) got += add_singer(s, tries);
}

int studio_choir_fill(DoomoSong *s, int mode, int anchor, uint32_t seed)
{
    DoomoRng r;
    int i;
    rng_seed(&r, seed + 0xC401u);
    for (i = 0; i < SONG_SINGERS; i++) s->singer[i] = SONG_NO_SINGER;
    memset(s->section, 0, sizeof(s->section));
    memset(s->line_singers, 0, sizeof(s->line_singers));
    switch (mode) {
    case FILL_FRIENDS: {
        uint16_t ids[SONG_SINGERS * 2];
        int n;
        if (anchor < 0) anchor = DOOMO_PLAYER_ID;
        add_singer(s, anchor);
        n = rel_list(anchor, ids, DOOMO_COUNTOF(ids));
        for (i = 0; i < n; i++)
            if (rel_value(anchor, ids[i]) > 0) add_singer(s, ids[i]);
        break; }
    case FILL_HILLVILE:
        fill_district(s, 0, DOOMO_DISTRICT_SIZE - 1, SONG_SINGERS, &r);
        break;
    case FILL_HELLVILE:
        fill_district(s, DOOMO_DISTRICT_SIZE, DOOMO_MAX_RESIDENTS - 1, SONG_SINGERS, &r);
        break;
    case FILL_BOTH:
        fill_district(s, 0, DOOMO_DISTRICT_SIZE - 1, 13, &r);
        fill_district(s, DOOMO_DISTRICT_SIZE, DOOMO_MAX_RESIDENTS - 1, 25, &r);
        break;
    case FILL_RANDOM:
        fill_district(s, 0, DOOMO_MAX_RESIDENTS - 1, SONG_SINGERS, &r);
        break;
    default:
        break;
    }
    if (mode != FILL_CLEAR && song_singer_count(s) == 0) add_singer(s, DOOMO_PLAYER_ID);
    studio_auto_parts(s);
    return song_singer_count(s);
}

/* ---- line parts ------------------------------------------------------------ */
static int valid_list(const DoomoSong *s, uint8_t *slots)
{
    int i, n = 0;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(s, i) >= 0) slots[n++] = (uint8_t)i;
    return n;
}

static uint32_t part_mask(const DoomoSong *s, int part, int who)
{
    uint8_t v[SONG_SINGERS];
    int n = valid_list(s, v), i;
    uint32_t m = 0;
    if (!n) return 0;
    switch (part) {
    case PART_SOLO: return 1u << v[((who % n) + n) % n];
    case PART_DUET:
        if (n < 2) return 1u << v[0];
        who = ((who % n) + n) % n;
        return (1u << v[who]) | (1u << v[(who + 1) % n]);
    case PART_SECTION:
        for (i = 0; i < n; i++) if (song_singer_section(s, v[i]) == who) m |= 1u << v[i];
        return m;
    case PART_HALF:
        for (i = 0; i < n; i++) if ((i < (n + 1) / 2) == !who) m |= 1u << v[i];
        return m ? m : 1u << v[0];
    default:
        for (i = 0; i < n; i++) m |= 1u << v[i];
        return m;
    }
}

int studio_part_choices(const DoomoSong *s, int part)
{
    uint8_t v[SONG_SINGERS];
    int n = valid_list(s, v);
    switch (part) {
    case PART_SOLO: case PART_DUET: return DOOMO_MAX(n, 1);
    case PART_SECTION: return SECTION_COUNT;
    case PART_HALF: return 2;
    default: return 1;
    }
}

void studio_set_part(DoomoSong *s, int line, int part, int who)
{
    uint32_t m;
    if (line < 0 || line >= SONG_LYRIC_LINES) return;
    m = part_mask(s, part, who);
    /* a solo of the first singer is the format's default (0) */
    if (part == PART_SOLO && m == 1u && song_singer_id(s, 0) >= 0) m = 0;
    s->line_singers[line] = m;
}

int studio_part_of(const DoomoSong *s, int line, int *who)
{
    uint8_t v[SONG_SINGERS];
    uint32_t m = song_line_mask(s, line);
    int n = valid_list(s, v), i, k;
    if (who) *who = 0;
    if (n <= 1 || !m) return PART_SOLO;
    if (m == part_mask(s, PART_CHOIR, 0)) return PART_CHOIR;
    for (k = 0; k < n; k++) if (m == part_mask(s, PART_SOLO, k)) { if (who) *who = k; return PART_SOLO; }
    for (k = 0; k < n; k++) if (m == part_mask(s, PART_DUET, k)) { if (who) *who = k; return PART_DUET; }
    for (k = 0; k < SECTION_COUNT; k++)
        if (m == part_mask(s, PART_SECTION, k)) { if (who) *who = k; return PART_SECTION; }
    for (k = 0; k < 2; k++) if (m == part_mask(s, PART_HALF, k)) { if (who) *who = k; return PART_HALF; }
    (void)i;
    if (who) *who = -1;
    return PART_CHOIR;
}

void studio_part_label(const DoomoSong *s, int line, char *buf, int cap)
{
    uint8_t v[SONG_SINGERS];
    int who, n = valid_list(s, v), part = studio_part_of(s, line, &who);
    uint32_t m = song_line_mask(s, line);
    int cnt = 0;
    while (m) { m &= m - 1; cnt++; }
    if (n == 0) { doomo_snprintf(buf, cap, "No singers"); return; }
    switch (part) {
    case PART_SOLO:
        doomo_snprintf(buf, cap, "Solo: %s", st_name(song_singer_id(s, n > 1 ? v[who] : v[0])));
        break;
    case PART_DUET:
        doomo_snprintf(buf, cap, "Duet: %s+%s", st_name(song_singer_id(s, v[who])),
                       st_name(song_singer_id(s, v[(who + 1) % n])));
        break;
    case PART_SECTION:
        doomo_snprintf(buf, cap, "%ss (%d)", doomo_section_names[who], cnt);
        break;
    case PART_HALF:
        doomo_snprintf(buf, cap, "%s half (%d)", who ? "2nd" : "1st", cnt);
        break;
    default:
        if (who < 0) doomo_snprintf(buf, cap, "Custom (%d)", cnt);
        else doomo_snprintf(buf, cap, "Full choir (%d)", cnt);
        break;
    }
}

void studio_auto_parts(DoomoSong *s)
{
    uint8_t v[SONG_SINGERS];
    int n = valid_list(s, v), line, uses[SONG_PATTERNS] = { 0, 0, 0, 0 }, p, best = 0, k = 0;
    memset(s->line_singers, 0, sizeof(s->line_singers));
    if (n <= 1) return;
    for (p = 0; p < s->order_len && p < SONG_ORDER_MAX; p++)
        if (s->lyrics[SONG_LINE_OF(p)][0]) uses[s->order[p] & 3]++;
    for (p = 1; p < SONG_PATTERNS; p++) if (uses[p] > uses[best]) best = p;
    for (line = 0; line < SONG_LYRIC_LINES; line++) {
        int pos = studio_line_pos(s, line);
        if (pos < 0 || !s->lyrics[line][0]) continue;
        if ((s->order[pos] & 3) == best && uses[best] >= 2) {
            studio_set_part(s, line, PART_CHOIR, 0);
        } else if (n >= 8) {
            static const uint8_t secs[4] = { SECTION_SOPRANO, SECTION_BASS, SECTION_ALTO, SECTION_TENOR };
            int sec = secs[k % 4], tries;
            for (tries = 0; tries < 4 && !part_mask(s, PART_SECTION, sec); tries++)
                sec = secs[(k + tries + 1) % 4];
            studio_set_part(s, line, part_mask(s, PART_SECTION, sec) ? PART_SECTION : PART_HALF,
                            part_mask(s, PART_SECTION, sec) ? sec : (k & 1));
            k++;
        } else if (n >= 4 && (k % 3) == 2) {
            studio_set_part(s, line, PART_DUET, k);
            k++;
        } else {
            studio_set_part(s, line, PART_SOLO, k);
            k++;
        }
    }
}

/* ====================================================================== */
/* auto-fit + word filter                                                   */
int studio_autofit(DoomoSong *s, int line)
{
    int pos = studio_line_pos(s, line), pat, m, n, changed = 0, guard;
    char *t = s->lyrics[line];
    if (pos < 0 || !t[0]) return 0;
    pat = s->order[pos] & 3;
    m = song_lead_onsets(s, pat);
    n = song_syllables(t, NULL, 0);
    /* too few words: la la la */
    for (guard = 0; n < m && guard < 16; guard++) {
        int len = (int)strlen(t);
        const char *add = (m - n == 1 && !changed) ? " oh" : " la";
        if (len + 3 > SONG_LYRIC_LEN - 1) break;
        memcpy(t + len, add, 4);
        n++;
        changed = 1;
    }
    /* still too few: merge the last notes into long ones (only when no
     * other lyric line is sung on this pattern) */
    {
        int p2, shared = 0;
        for (p2 = 0; p2 < s->order_len && p2 < SONG_ORDER_MAX; p2++)
            if ((s->order[p2] & 3) == pat && SONG_LINE_OF(p2) != line &&
                s->lyrics[SONG_LINE_OF(p2)][0]) shared = 1;
        if (shared) guard = SONG_STEPS;
        else guard = 0;
    }
    for (; n < m && guard < SONG_STEPS; guard++) {
        int i;
        uint8_t *c = s->notes[pat][0];
        for (i = SONG_STEPS - 1; i > 0; i--) if (c[i] >= 2) break;
        if (i <= 0) break;
        c[i] = SONG_HOLD;
        m--;
        changed = 1;
    }
    /* too many words: split long notes */
    for (guard = 0; n > m && guard < SONG_STEPS; guard++) {
        uint8_t *c = s->notes[pat][0];
        int i, best = -1, blen = 0, run = 0, note = 0, start = -1;
        for (i = 0; i <= SONG_STEPS; i++) {
            int v = i < SONG_STEPS ? c[i] : 0;
            if (v == SONG_HOLD && start >= 0) { run++; continue; }
            if (start >= 0 && run > blen) { blen = run; best = start; }
            run = 0;
            start = v >= 2 ? i : -1;
            if (v >= 2) note = v;
        }
        (void)note;
        if (best < 0 || blen < 1) {
            /* no held notes left: turn a rest after a note into a note */
            int prev = 0;
            for (i = 0; i < SONG_STEPS; i++) {
                if (c[i] >= 2) prev = c[i];
                else if (c[i] == SONG_REST && prev) { c[i] = (uint8_t)prev; break; }
            }
            if (i >= SONG_STEPS) break;
        } else {
            int mid = best + (blen + 1) / 2;
            c[mid] = c[best];
        }
        m++;
        changed = 1;
    }
    return changed;
}

/* rude words (ROT13, so this file stays polite) -> cute ones */
static const char *const rude_rot[] = { "qnza", "fuvg", "shpx", "ovgpu", "penc", "cvff",
                                        "qvpx", "onfgneq", "nff", "phag", "juber", "fyhg",
                                        "shpxvat", "fuvgg", "nffubyr" };
static const char *const cute_sub[] = { "darn", "shoot", "fluff", "kitty", "crumbs", "fizz",
                                        "duck", "buster", "butt", "cutie", "snore", "slug",
                                        "fluffing", "shoot", "bun" };

static char rot13(char c)
{
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    return (c >= 'a' && c <= 'z') ? (char)('a' + (c - 'a' + 13) % 26) : c;
}

int studio_clean_text(char *t, int cap)
{
    int i = 0, swaps = 0;
    char out[64];
    int o = 0;
    if (!t || cap <= 1) return 0;
    if (cap > (int)sizeof(out)) cap = (int)sizeof(out);
    while (t[i] && o < cap - 1) {
        int ws = i, we, k, hit = -1;
        unsigned char c = (unsigned char)t[i];
        if (!((c | 32) >= 'a' && (c | 32) <= 'z')) {
            out[o++] = (c >= 0x20 && c <= 0x7E) ? (char)c : ' ';
            i++;
            continue;
        }
        while (t[i] && (((unsigned char)t[i] | 32) >= 'a' && ((unsigned char)t[i] | 32) <= 'z')) i++;
        we = i;
        for (k = 0; k < DOOMO_COUNTOF(rude_rot) && hit < 0; k++) {
            int j, len = (int)strlen(rude_rot[k]);
            if (len != we - ws) continue;
            for (j = 0; j < len && rot13(t[ws + j]) == rude_rot[k][j]; j++) {}
            if (j == len) hit = k;
        }
        if (hit >= 0) {
            const char *sub = cute_sub[hit];
            swaps++;
            while (*sub && o < cap - 1) out[o++] = *sub++;
        } else {
            for (k = ws; k < we && o < cap - 1; k++) out[o++] = t[k];
        }
    }
    out[o] = 0;
    memcpy(t, out, (size_t)o + 1);
    return swaps;
}

/* ====================================================================== */
/* inspire                                                                  */
void studio_inspire(DoomoSong *s, int muse, int genre, uint32_t seed)
{
    const DoomoResident *r = town_res(muse >= 0 ? muse : DOOMO_PLAYER_ID);
    int keep_layout = s->choir_layout;
    if (genre < 0) genre = r ? r->like_music % MUSIC_COUNT : 0;
    song_clear(s);
    s->choir_layout = (uint8_t)keep_layout;
    s->genre = (uint8_t)genre;
    s->key = (uint8_t)(doomo_hash32(seed) % 12);
    {
        static const uint8_t ord_std[8] = { 3, 0, 0, 1, 0, 1, 2, 1 };
        memcpy(s->order, ord_std, sizeof(ord_std));
        s->order_len = 8;
    }
    studio_write_lyrics(s, muse >= 0 ? muse : DOOMO_PLAYER_ID, seed);
    s->flags &= (uint8_t)~SONGF_CUSTOM;
    studio_compose(s, genre, seed);
    s->flags |= SONGF_CUSTOM;
    studio_choir_fill(s, FILL_FRIENDS, muse >= 0 ? muse : DOOMO_PLAYER_ID, seed);
    /* a band, not a stadium: keep the muse + a few friends */
    {
        int i, n = 0;
        for (i = 0; i < SONG_SINGERS; i++)
            if (song_singer_id(s, i) >= 0 && ++n > 6) s->singer[i] = SONG_NO_SINGER;
        studio_auto_parts(s);
    }
    if (muse >= DOOMO_DISTRICT_SIZE && muse < DOOMO_MAX_RESIDENTS) s->flags |= SONGF_HELLISH;
    song_sanitize(s);
}

/* ====================================================================== */
/* templates                                                                */
enum { TC_NONE, TC_CHURCH, TC_PARK, TC_BAND };
typedef struct Template {
    const char *name, *blurb;
    uint8_t genre, key, bpm, loop, choir, layout;
    uint16_t unlock;
    uint32_t seed;
    uint8_t order_len, order[12];
    const char *lyrics[SONG_LYRIC_LINES];
} Template;

static const Template templates[STUDIO_TEMPLATES] = {
    { "Sugar Rush", "Cute pop with sprinkles on top", 0, 0, 126, 0, TC_BAND, CHOIR_RISERS,
      0xFFFF, 11, 8, { 3, 0, 0, 1, 0, 1, 2, 1 },
      { "", "Sprinkles on my nose", "candy in my paws", "sugar rush, sugar rush!",
        "bunny hop to school", "sugar rush, sugar rush!", "one more cupcake please",
        "sugar rush, sugar rush!" } },
    { "Beep Beep Love", "Tamagotchi chiptune, loops forever", 3, 7, 150, 1, TC_NONE,
      CHOIR_RISERS, 0xFFFF, 23, 6, { 0, 1, 0, 1, 2, 1 },
      { "Feed me, play with me", "beep beep, I love you", "clean my little poop",
        "beep beep, I love you", "pixel heart is full", "beep beep, I love you", "", "" } },
    { "Plushie Lullaby", "Soft and sleepy, for tucking in", 4, 5, 72, 0, TC_BAND,
      CHOIR_SEMICIRCLE, 0xFFFF, 31, 5, { 0, 0, 1, 0, 1 },
      { "Hush now, little plushie", "the stars are fluffy too", "sleep tight, sleep tight",
        "the moon is a pillow", "sleep tight, sleep tight", "", "", "" } },
    { "Mosh Pit Picnic", "Fast hardcore punk, bring a blanket", 13, 4, 184, 0, TC_BAND,
      CHOIR_RISERS, 0xFFFF, 47, 6, { 0, 1, 0, 1, 2, 1 },
      { "Ants in my sandwich", "mosh pit picnic!", "spill the lemonade", "mosh pit picnic!",
        "jump in the potato salad", "mosh pit picnic!", "", "" } },
    { "Doom Cupcakes", "Slow doom metal about dessert", 14, 4, 66, 0, TC_BAND, CHOIR_RISERS,
      0xFFFF, 66, 6, { 3, 0, 1, 0, 1, 1 },
      { "", "The frosting is black", "doom of cupcakes", "the sprinkles all scream",
        "doom of cupcakes", "doom of cupcakes!", "", "" } },
    { "Organ of Gloom", "Gothic organ for candlelit nights", 12, 9, 124, 0, TC_BAND,
      CHOIR_SECTIONS, 0xFFFF, 13, 7, { 3, 0, 0, 1, 2, 1, 3 },
      { "", "Candles in the attic", "the bats knit me a scarf", "gloom, sweet sweet gloom",
        "my heart is a cobweb", "gloom, sweet sweet gloom", "", "" } },
    { "Hellvile Flow", "A laid-back hip-hop beat", 1, 2, 86, 1, TC_BAND, CHOIR_RISERS,
      0xFFFF, 86, 8, { 3, 0, 0, 1, 0, 1, 2, 1 },
      { "", "Yo, it's the Hellvile crew", "imps chill on the corner", "flow, flow, Hellvile flow",
        "brimstone bagels so hot", "flow, flow, Hellvile flow", "drop the beat, drop it",
        "flow, flow, Hellvile flow" } },
    { "Park United!", "Festival anthem: both districts sing", 7, 7, 110, 0, TC_PARK,
      CHOIR_SEMICIRCLE, 0xFFFF, 777, 8, { 3, 0, 0, 1, 0, 1, 2, 1 },
      { "", "Hillvile on the left side", "Hellvile on the right side", "tear the wall down, sing!",
        "imps and bunnies all dance", "tear the wall down, sing!", "one park, one big heart",
        "tear the wall down, sing!" } },
    { "Hymn of Cuddles", "For the Church of St. Cuddles", 10, 0, 76, 0, TC_CHURCH,
      CHOIR_SECTIONS, 0xFFFF, 316, 6, { 0, 1, 0, 1, 2, 1 },
      { "Blessed be the fluffy paws", "hallelujah, hugs!", "holy water, warm milk",
        "hallelujah, hugs!", "keep the demons cozy", "hallelujah, hugs!", "", "" } },
    { "Yo Ho Hellvile", "A sea shanty for the Lake of Fire", 9, 2, 102, 0, TC_BAND,
      CHOIR_RISERS, UL_STUDIO_TPL_SHANTY, 99, 6, { 0, 1, 0, 1, 2, 1 },
      { "We sail the lake of fire", "yo ho, Hellvile ho!", "our boat is made of toast",
        "yo ho, Hellvile ho!", "the captain is a frog", "yo ho, Hellvile ho!", "", "" } },
    { "Neon Nightmare", "Synthwave for midnight drives", 11, 9, 104, 1, TC_BAND, CHOIR_RISERS,
      UL_STUDIO_TPL_SYNTH, 1984, 6, { 3, 0, 1, 0, 1, 1 },
      { "", "Neon on the gate", "drive into the dark", "chrome and purple haze",
        "drive into the dark", "drive into the dark", "", "" } },
    { "Cat Cafe Jazz", "Swingy jazz with a latte", 5, 5, 112, 0, TC_BAND, CHOIR_SEMICIRCLE,
      UL_STUDIO_TPL_JAZZ, 451, 6, { 0, 1, 0, 1, 2, 1 },
      { "Purring by the window", "scoobie doo, meow", "foam art of a fishy",
        "scoobie doo, meow", "the piano has fleas", "scoobie doo, meow", "", "" } },
    { "Disco Inferno", "Four on the floor, in Hellvile", 8, 9, 120, 1, TC_BAND, CHOIR_RISERS,
      UL_STUDIO_TPL_DISCO, 1977, 8, { 3, 0, 0, 1, 0, 1, 2, 1 },
      { "", "The dance floor is lava", "glitter on both my horns", "burn baby, boogie!",
        "imps in their platform shoes", "burn baby, boogie!", "spin me round, demon",
        "burn baby, boogie!" } },
    { "Forge of Clank", "Industrial anvils and chains", 15, 4, 134, 1, TC_BAND, CHOIR_PYRAMID,
      UL_STUDIO_TPL_FORGE, 1313, 6, { 3, 0, 1, 0, 1, 1 },
      { "", "Hammer on the anvil", "clank clank clank clank!", "Rivet welds a big heart",
        "clank clank clank clank!", "clank clank clank clank!", "", "" } },
};

const char *studio_template_name(int t) { return (t >= 0 && t < STUDIO_TEMPLATES) ? templates[t].name : ""; }
const char *studio_template_blurb(int t) { return (t >= 0 && t < STUDIO_TEMPLATES) ? templates[t].blurb : ""; }
int studio_template_genre(int t) { return (t >= 0 && t < STUDIO_TEMPLATES) ? templates[t].genre : 0; }
int studio_template_unlock(int t)
{
    if (t < 0 || t >= STUDIO_TEMPLATES || templates[t].unlock == 0xFFFF) return -1;
    return templates[t].unlock;
}

static void church_choir(DoomoSong *s, DoomoRng *r)
{
    int i, id;
    /* Father Fuzz + church folk, then any Hillvile voices */
    if (st_alive(ROSTER_ID_FATHER_FUZZ)) add_singer(s, ROSTER_ID_FATHER_FUZZ);
    for (i = 0; i < DOOMO_DISTRICT_SIZE && song_singer_count(s) < 16; i++) {
        id = i;
        if (!(doomo_town.res[id].flags & RF_CUSTOM) && (roster_traits(id) & ROSTER_T_HOLY))
            add_singer(s, id);
    }
    fill_district(s, 0, DOOMO_DISTRICT_SIZE - 1, 20 - song_singer_count(s), r);
}

void studio_template_load(DoomoSong *s, int t)
{
    const Template *tp;
    DoomoRng r;
    int i;
    if (t < 0 || t >= STUDIO_TEMPLATES) return;
    tp = &templates[t];
    song_clear(s);
    memset(s->title, 0, sizeof(s->title));
    memcpy(s->title, tp->name, strlen(tp->name) < SONG_TITLE_LEN ? strlen(tp->name) : SONG_TITLE_LEN - 1);
    s->key = tp->key;
    s->order_len = tp->order_len;
    memcpy(s->order, tp->order, tp->order_len);
    for (i = 0; i < SONG_LYRIC_LINES; i++) {
        size_t n = strlen(tp->lyrics[i]);
        memcpy(s->lyrics[i], tp->lyrics[i], n < SONG_LYRIC_LEN ? n + 1 : SONG_LYRIC_LEN - 1);
    }
    s->flags = SONGF_VALID;
    studio_compose(s, tp->genre, tp->seed);
    s->bpm = tp->bpm;
    s->flags = (uint8_t)(SONGF_VALID | SONGF_CUSTOM | (tp->loop ? SONGF_LOOP : 0) |
                         (tp->genre >= 12 ? SONGF_HELLISH : 0));
    s->choir_layout = tp->layout;
    rng_seed(&r, tp->seed);
    switch (tp->choir) {
    case TC_CHURCH: {
        church_choir(s, &r);
        if (!song_singer_count(s)) add_singer(s, DOOMO_PLAYER_ID);
        break; }
    case TC_PARK:
        if (st_alive(ROSTER_ID_MAYOR)) add_singer(s, ROSTER_ID_MAYOR);
        fill_district(s, 0, DOOMO_DISTRICT_SIZE - 1, 12, &r);
        fill_district(s, DOOMO_DISTRICT_SIZE, DOOMO_MAX_RESIDENTS - 1, 25, &r);
        if (!song_singer_count(s)) add_singer(s, DOOMO_PLAYER_ID);
        break;
    case TC_BAND:
        add_singer(s, DOOMO_PLAYER_ID);
        break;
    default:
        add_singer(s, DOOMO_PLAYER_ID);
        break;
    }
    studio_auto_parts(s);
    if (tp->choir == TC_PARK && song_singer_count(s) >= 4) {
        /* call and response: Hillvile asks, Hellvile answers, all together
         * on the chorus */
        uint32_t hill = 0, hell = 0;
        for (i = 0; i < SONG_SINGERS; i++) {
            int id = song_singer_id(s, i);
            if (id < 0) continue;
            if (id < DOOMO_DISTRICT_SIZE) hill |= 1u << i;
            else if (id < DOOMO_MAX_RESIDENTS) hell |= 1u << i;
        }
        s->line_singers[1] = hill ? hill : 0;
        s->line_singers[2] = hell ? hell : 0;
        s->line_singers[4] = hill | hell;
        s->line_singers[6] = hell ? hell : 0;
    }
    song_sanitize(s);
}
