/* studio_tools.c - ST-TOOLS (owner: studio): the editing tools and the seven helper tools of the Music Studio.
 *
 * Everything here is a pure function on a DoomoSong (no input, no drawing, no undo): the tabs call ed_touch() on the
 * cells, call one of these, then ed_commit(), so every tool is one undo step. Each returns the number of cells (or
 * tracks) it changed, 0 = nothing to do, so the caller can say so instead of pretending.
 *
 *   range tools     transpose by scale step / octave, repeat to the end, reverse, twice / half speed, clear
 *   chord suggester the next chord from the key and the chord before it (a diatonic table, no search)
 *   melody helper   one-bar phrases on the chords (the composer's motif code)
 *   groove picker   the composer's drum grooves by name
 *   auto-harmony    a thirds / fifths / octave line under the lead
 *   section copier  copy a pattern (transposed, other drums)
 *   mix helper      balance the track volumes by role, and flag tracks that hide the lead
 *
 * The chord lane lives in the bass + harmony tracks (the classic format has no fifth lane: see docs/STUDIO.md). */
#include "studio_int.h"

/* ---- helpers ------------------------------------------------------------------------------------------------ */
static uint8_t *row(DoomoSong *s, int p, int t) { return s->notes[p & 3][t & 3]; }

/* semitone sets of the scales (the same as song_player.c), bit n = n semitones above the key */
static const uint16_t tl_scale_mask[SONG_SCALE_COUNT] = { 0x0AB5, 0x05AD, 0x0295, 0x04E9, 0x09AD, 0x0FFF };

/* the semitone offsets of degree d of the song's scale (chromatic: the major scale, a chord needs seven names) */
static int deg_semis(const DoomoSong *s, int d)
{
    uint16_t m = tl_scale_mask[s->scale < SONG_SCALE_COUNT ? s->scale : SONG_SCALE_MAJOR];
    int oct, k = 0, i;
    if (s->scale == SONG_SCALE_CHROMATIC) m = tl_scale_mask[SONG_SCALE_MAJOR];
    {
        int n = 0;
        for (i = 0; i < 12; i++) n += (m >> i) & 1;
        oct = d / n;
        d %= n;
    }
    for (i = 0; i < 12; i++) {
        if (!((m >> i) & 1)) continue;
        if (k == d) return oct * 12 + i;
        k++;
    }
    return 0;
}

static int deg_count(const DoomoSong *s)
{
    uint16_t m = tl_scale_mask[s->scale < SONG_SCALE_COUNT ? s->scale : SONG_SCALE_MAJOR];
    int i, n = 0;
    if (s->scale == SONG_SCALE_CHROMATIC) m = tl_scale_mask[SONG_SCALE_MAJOR];
    for (i = 0; i < 12; i++) n += (m >> i) & 1;
    return n;
}

/* the note of scale degree d (0 = the tonic) above the tonic note `tonic` */
static int deg_note(const DoomoSong *s, int tonic, int d)
{
    return DOOMO_CLAMP(tonic + deg_semis(s, d), 2, 127);
}

/* the tonic in a register: key pc, in the octave starting at `lo` (MIDI) */
static int tonic_at(const DoomoSong *s, int lo) { return lo + s->key % 12; }

/* ---- range tools ------------------------------------------------------------------------------------------------ */
void tl_range(int mark, int stp, int *from, int *to)
{
    if (mark >= 0 && mark < SONG_STEPS) {
        *from = DOOMO_MIN(mark, stp);
        *to = DOOMO_MAX(mark, stp) + 1;
    } else {
        *from = (stp >= 16 ? 16 : 0);            /* no mark: the bar under the cursor */
        *to = *from + 16;
    }
}

static int clamp_range(int *from, int *to)
{
    *from = DOOMO_CLAMP(*from, 0, SONG_STEPS);
    *to = DOOMO_CLAMP(*to, 0, SONG_STEPS);
    return *to > *from;
}

int tl_shift(DoomoSong *s, int p, int t, int from, int to, int steps)
{
    uint8_t *c = row(s, p, t);
    int i, n = 0;
    if (t == 3 || !clamp_range(&from, &to)) return 0;
    for (i = from; i < to; i++) {
        int v, nv;
        if (c[i] < 2) continue;
        v = c[i];
        nv = song_scale_shift(s, v, steps);
        if (nv < SONG_NOTE_MIN || nv > SONG_NOTE_MAX || nv == v) continue;
        c[i] = (uint8_t)nv;
        n++;
    }
    return n;
}

int tl_octave(DoomoSong *s, int p, int t, int from, int to, int dir)
{
    uint8_t *c = row(s, p, t);
    int i, n = 0, d = dir < 0 ? -12 : 12;
    if (t == 3 || !clamp_range(&from, &to)) return 0;
    for (i = from; i < to; i++) {
        int nv = c[i] + d;
        if (c[i] < 2 || nv < SONG_NOTE_MIN || nv > SONG_NOTE_MAX) continue;
        c[i] = (uint8_t)nv;
        n++;
    }
    return n;
}

/* the range [from,to) copied again and again to the end of the pattern; returns the cells written */
int tl_repeat(DoomoSong *s, int p, int t, int from, int to)
{
    uint8_t *c = row(s, p, t);
    int i, len, n = 0;
    if (!clamp_range(&from, &to) || to >= SONG_STEPS) return 0;
    len = to - from;
    for (i = to; i < SONG_STEPS; i++) {
        uint8_t v = c[from + (i - to) % len];
        if (c[i] != v) n++;
        c[i] = v;
    }
    return n;
}

int tl_clear(DoomoSong *s, int p, int t, int from, int to)
{
    uint8_t *c = row(s, p, t);
    int i, n = 0;
    if (!clamp_range(&from, &to)) return 0;
    for (i = from; i < to; i++) if (c[i]) { c[i] = 0; n++; }
    return n;
}

/* Reverse the order of the notes. A held note (note + its ties) moves as one piece, so a long note stays long. */
int tl_reverse(DoomoSong *s, int p, int t, int from, int to)
{
    uint8_t *c = row(s, p, t), old[SONG_STEPS], seg_start[SONG_STEPS + 1];
    int i, n, segs = 0, w, changed = 0;
    if (!clamp_range(&from, &to)) return 0;
    n = to - from;
    memcpy(old, c + from, (size_t)n);
    if (t == 3) {                                  /* drums have no ties: mirror the cells */
        for (i = 0; i < n; i++) { if (c[from + i] != old[n - 1 - i]) changed++; c[from + i] = old[n - 1 - i]; }
        return changed;
    }
    /* pieces: a note with its holds, or a run of rests / leading holds */
    for (i = 0; i < n; i++)
        if (i == 0 || old[i] >= 2 || (old[i] == SONG_REST && old[i - 1] != SONG_REST) ) seg_start[segs++] = (uint8_t)i;
    seg_start[segs] = (uint8_t)n;
    w = 0;
    for (i = segs - 1; i >= 0; i--) {
        int a = seg_start[i], b = seg_start[i + 1], k;
        for (k = a; k < b; k++, w++) {
            if (c[from + w] != old[k]) changed++;
            c[from + w] = old[k];
        }
    }
    return changed;
}

/* faster = 1: every second step (twice as fast); 0: every cell takes two steps (half as fast, cut at the pattern end) */
int tl_stretch(DoomoSong *s, int p, int t, int from, int to, int faster)
{
    uint8_t *c = row(s, p, t), old[SONG_STEPS];
    int i, n, changed = 0, melodic = t != 3;
    if (!clamp_range(&from, &to)) return 0;
    n = to - from;
    memcpy(old, c + from, (size_t)n);
    if (faster) {
        if (n < 2) return 0;
        for (i = 0; i < n; i++) {
            uint8_t v = 0;
            if (i < n / 2) {
                v = old[2 * i];
                if (v < (melodic ? 2 : 1) && 2 * i + 1 < n && old[2 * i + 1] >= (melodic ? 2 : 1))
                    v = old[2 * i + 1];                                  /* a note on the off-beat is kept */
            }
            if (c[from + i] != v) changed++;
            c[from + i] = v;
        }
    } else {
        int end = DOOMO_MIN(SONG_STEPS, from + 2 * n);
        for (i = from; i < end; i++) {
            int k = (i - from) / 2;
            uint8_t v = (i - from) & 1 ? (melodic && old[k] ? SONG_HOLD : 0) : old[k];
            if ((i - from) & 1 && melodic && old[k] == SONG_REST) v = 0;
            if (c[i] != v) changed++;
            c[i] = v;
        }
    }
    return changed;
}

/* ---- chord suggester ------------------------------------------------------------------------------------------- */
/* how many chords the song's scale has (7; 5 for the pentatonic, 6 for the blues) */
int tl_chord_count(const DoomoSong *s) { return deg_count(s); }

/* the degree (0..) of a note in the song's scale; -1 if it is not a scale note */
int tl_degree(const DoomoSong *s, int midi)
{
    int rel = ((midi - s->key % 12) % 12 + 12) % 12, d, n = deg_count(s);
    for (d = 0; d < n; d++) if (deg_semis(s, d) == rel) return d;
    return -1;
}

/* the chord that sounds in half bar h (0..3) of pattern p: the bass note's degree (the harmony's when the bass rests),
 * or -1 when neither track plays a scale note in it */
int tl_chord_at(const DoomoSong *s, int p, int h)
{
    int t, i;
    static const uint8_t order[2] = { 2, 1 };
    for (t = 0; t < 2; t++) {
        const uint8_t *c = s->notes[p & 3][order[t]];
        for (i = h * 8; i < h * 8 + 8; i++)
            if (c[i] >= 2) {
                int d = tl_degree(s, c[i]);
                if (d >= 0) return d % 7;
            }
    }
    return -1;
}

/* what usually follows a chord: degree -> four likely next degrees (a pop / folk / hymn habit) */
static const uint8_t next_of[7][4] = {
    { 3, 4, 5, 1 }, { 4, 6, 3, 0 }, { 5, 3, 1, 4 }, { 4, 0, 1, 5 },
    { 0, 5, 3, 2 }, { 3, 1, 4, 0 }, { 0, 2, 5, 4 },
};

/* the pattern played just before p in the order list (the first time p is played), -1 when p opens the song; a
 * pattern the order never plays has no before: the one numbered before it */
static int pattern_before(const DoomoSong *s, int p)
{
    int i;
    for (i = 0; i < s->order_len; i++)
        if ((s->order[i] & 3) == (p & 3)) return i > 0 ? s->order[i - 1] & 3 : -1;
    return (p + 3) & 3;
}

/* up to n (<= 4) chord degrees that fit after half bar h of pattern p (the half bar before it, in the pattern that
 * plays before it for h = 0; the tonic starts a song). Every degree is a chord of the song's scale. */
int tl_suggest_chords(const DoomoSong *s, int p, int h, int n, uint8_t out[4])
{
    int cnt = deg_count(s), i, k = 0, prev = -1, pb;
    static const uint8_t start[4] = { 0, 3, 4, 5 };
    n = DOOMO_CLAMP(n, 1, 4);
    if (h > 0) prev = tl_chord_at(s, p, h - 1);
    else if ((pb = pattern_before(s, p)) >= 0) prev = tl_chord_at(s, pb, 3);
    for (i = 0; i < 4 && k < n; i++) {
        int d = prev < 0 ? start[i] : next_of[prev % 7][i], j, dup = 0;
        if (cnt < 7) d %= cnt;                      /* a pentatonic song has fewer chords to choose from */
        for (j = 0; j < k; j++) if (out[j] == d) dup = 1;
        if (!dup) out[k++] = (uint8_t)d;
    }
    return k;
}

/* "Am", "C", "G7"-free: the root's name, "m" when the third is a minor third */
void tl_chord_name(const DoomoSong *s, int deg, char *buf, int cap)
{
    int root = deg_note(s, tonic_at(s, 60), deg), third = deg_note(s, tonic_at(s, 60), deg + 2);
    char nm[6];
    int i;
    if (cap < 1) return;
    song_note_name(root, nm);
    for (i = 0; nm[i] && !(nm[i] >= '0' && nm[i] <= '9') && nm[i] != '-'; i++) {}
    nm[i] = 0;
    for (i = 0; nm[i] && i < cap - 1; i++) buf[i] = nm[i];             /* no printf: the chord screen names 16 cells a frame */
    if ((third - root) % 12 == 3 && i < cap - 1) buf[i++] = 'm';
    buf[i] = 0;
}

/* write chord `deg` into half bar h of pattern p: the bass plays the root (held), the harmony the third above it
 * (held). Returns the cells that changed. */
int tl_chord_write(DoomoSong *s, int p, int h, int deg)
{
    uint8_t *b = row(s, p, 2), *m = row(s, p, 1);
    int root = deg_note(s, tonic_at(s, 36), deg), third = deg_note(s, tonic_at(s, 48), deg + 2), i, n = 0;
    if (h < 0 || h > 3) return 0;
    for (i = 0; i < 8; i++) {
        uint8_t nb = (uint8_t)(i == 0 ? root : SONG_HOLD), nm = (uint8_t)(i == 0 ? third : SONG_HOLD);
        if (b[h * 8 + i] != nb) n++;
        if (m[h * 8 + i] != nm) n++;
        b[h * 8 + i] = nb;
        m[h * 8 + i] = nm;
    }
    return n;
}

/* take the chord out of half bar h of pattern p (the bass and the harmony rest there); cells changed */
int tl_chord_clear(DoomoSong *s, int p, int h)
{
    int i, n = 0;
    if (h < 0 || h > 3) return 0;
    for (i = h * 8; i < h * 8 + 8; i++) {
        if (row(s, p, 2)[i]) { row(s, p, 2)[i] = 0; n++; }
        if (row(s, p, 1)[i]) { row(s, p, 1)[i] = 0; n++; }
    }
    return n;
}

/* the chord of half bar h written into every half bar after it in the pattern; cells changed (0: no chord there) */
int tl_chord_repeat(DoomoSong *s, int p, int h)
{
    int d = tl_chord_at(s, p, h), k, n = 0;
    if (d < 0) return 0;
    for (k = h + 1; k < 4; k++) n += tl_chord_write(s, p, k, d);
    return n;
}

/* ---- drum editor ------------------------------------------------------------------------------------------------ */
/* One drum per step (the format has one drums cell per step). Toggle `drum` (1..8) at step stp of pattern p: set it
 * (replacing another drum there) or, when it is already there, take it out. Returns the new cell value. */
int tl_drum_toggle(DoomoSong *s, int p, int stp, int drum)
{
    uint8_t *c = row(s, p, 3) + (stp & 31);
    if (drum < 1 || drum >= SONG_DRUM_COUNT) return *c;
    *c = (uint8_t)(*c == drum ? 0 : drum);
    return *c;
}

/* every hit of one drum taken out of pattern p; cells changed */
int tl_drum_lane_clear(DoomoSong *s, int p, int drum)
{
    uint8_t *c = row(s, p, 3);
    int i, n = 0;
    for (i = 0; i < SONG_STEPS; i++) if (c[i] == drum && drum > 0) { c[i] = 0; n++; }
    return n;
}

/* ---- melody helper ----------------------------------------------------------------------------------------------- */
void studio_phrase(const DoomoSong *s, int base, int chord0, int chord1, uint32_t seed, uint8_t out[16]);

/* candidate `variant` (0..3) of a one-bar lead phrase for bar (0 / 1) of pattern p, on the chords that bar has
 * (the tonic where it has none); the same song + variant always gives the same phrase */
void tl_phrase(const DoomoSong *s, int p, int bar, int variant, uint8_t out[16])
{
    int c0 = tl_chord_at(s, p, bar * 2), c1 = tl_chord_at(s, p, bar * 2 + 1), base = tonic_at(s, 60);
    if (c0 < 0) c0 = 0;
    if (c1 < 0) c1 = c0;
    if (base - s->key % 12 + 12 < 80 && s->key % 12 > 5) base -= 12;
    studio_phrase(s, base, c0, c1, s->seed * 31u + (uint32_t)(p * 8 + bar * 4 + variant) + 1u, out);
}

int tl_phrase_write(DoomoSong *s, int p, int bar, const uint8_t ph[16])
{
    uint8_t *c;
    int i, n = 0;
    if (bar < 0 || bar > 1) return 0;
    c = row(s, p, 0) + bar * 16;
    for (i = 0; i < 16; i++) { if (c[i] != ph[i]) n++; c[i] = ph[i]; }
    return n;
}

/* ---- groove picker ---------------------------------------------------------------------------------------------- */
void        studio_groove_write(DoomoSong *s, int pat, int g, int with_fill);

int tl_groove_write(DoomoSong *s, int p, int g, int with_fill)
{
    uint8_t before[SONG_STEPS], *c = row(s, p, 3);
    int i, n = 0;
    memcpy(before, c, SONG_STEPS);
    studio_groove_write(s, p, g, with_fill);
    for (i = 0; i < SONG_STEPS; i++) if (before[i] != c[i]) n++;
    return n;
}

/* ---- choir auto-harmony ------------------------------------------------------------------------------------------ */
/* Write a line under the lead of pattern p into the harmony track: kind 0 = a third below, 1 = a fifth below,
 * 2 = an octave below (all in the song's scale). Ties follow the lead. Returns the cells that changed. */
int tl_harmonize(DoomoSong *s, int p, int kind)
{
    const uint8_t *l = s->notes[p & 3][0];
    uint8_t *h = row(s, p, 1);
    int i, n = 0, any = 0;
    for (i = 0; i < SONG_STEPS; i++) if (l[i] >= 2) any = 1;
    if (!any) return 0;
    for (i = 0; i < SONG_STEPS; i++) {
        uint8_t v = l[i];
        if (v >= 2) {
            int d = kind == 1 ? -4 : -2;
            v = (uint8_t)(kind == 2 ? DOOMO_MAX(l[i] - 12, SONG_NOTE_MIN)
                                    : DOOMO_MAX(song_scale_shift(s, l[i], d), SONG_NOTE_MIN));
        }
        if (h[i] != v) n++;
        h[i] = v;
    }
    return n;
}

/* ---- section copier ---------------------------------------------------------------------------------------------- */
/* Copy pattern `src` into `dst`, the melodic tracks moved `shift` scale steps; `groove` >= 0 swaps the drums for that
 * groove. Returns the cells that changed. */
int tl_section_copy(DoomoSong *s, int src, int dst, int shift, int groove)
{
    int t, i, n = 0;
    uint8_t before[SONG_TRACKS][SONG_STEPS];
    src &= 3; dst &= 3;
    if (src == dst) return 0;
    memcpy(before, s->notes[dst], sizeof(before));
    memcpy(s->notes[dst], s->notes[src], sizeof(s->notes[dst]));
    if (shift) for (t = 0; t < 3; t++) tl_shift(s, dst, t, 0, SONG_STEPS, shift);
    if (groove >= 0) studio_groove_write(s, dst, groove, 1);
    for (t = 0; t < SONG_TRACKS; t++)
        for (i = 0; i < SONG_STEPS; i++) if (before[t][i] != s->notes[dst][t][i]) n++;
    return n;
}

/* the first pattern the order list does not use (a free slot for a copy), else the one after `src` */
int tl_free_pattern(const DoomoSong *s, int src)
{
    int p, i;
    for (p = 0; p < SONG_PATTERNS; p++) {
        int used = 0;
        for (i = 0; i < s->order_len; i++) if (s->order[i] == p) used = 1;
        if (!used && p != src) return p;
    }
    return (src + 1) & 3;
}

/* ---- mix helper ------------------------------------------------------------------------------------------------- */
/* the balance by role: the lead on top, the bass under it, the drums and the harmony behind. The reference is the
 * lead's volume (raised to the loudest track's, so the song keeps its level); a song without a lead takes the first
 * track it has in the order bass, drums, harmony and keeps that one's volume. Either way the reference never moves,
 * so a second run changes nothing. Returns the number of tracks whose volume changed. */
int tl_mix_balance(DoomoSong *s)
{
    static const uint8_t role[SONG_TRACKS] = { 12, 8, 10, 9 };      /* of 12 */
    static const uint8_t ref_order[SONG_TRACKS] = { 0, 2, 3, 1 };
    uint8_t used[SONG_TRACKS];
    int t, i, ref = -1, top, n = 0;
    for (t = 0; t < SONG_TRACKS; t++) {
        used[t] = 0;
        for (i = 0; i < SONG_PATTERNS * SONG_STEPS; i++)
            if (s->notes[i / SONG_STEPS][t][i % SONG_STEPS] >= (t == 3 ? 1 : 2)) { used[t] = 1; break; }
    }
    for (i = 0; i < SONG_TRACKS && ref < 0; i++) if (used[ref_order[i]]) ref = ref_order[i];
    if (ref < 0) return 0;                          /* nothing is played: nothing to balance */
    if (ref == 0) {
        top = 0;
        for (t = 0; t < SONG_TRACKS; t++) if (used[t]) top = DOOMO_MAX(top, s->vol[t]);
        top = DOOMO_CLAMP(top, 8, 15);
    } else {
        top = (s->vol[ref] * 12 + role[ref] / 2) / role[ref];       /* the level the lead would have */
    }
    for (t = 0; t < SONG_TRACKS; t++) {
        int v;
        if (!used[t] || (ref > 0 && t == ref)) continue;            /* an empty track keeps its volume; the reference stays */
        v = DOOMO_CLAMP((role[t] * top + 6) / 12, 1, 15);
        if (s->vol[t] != v) { s->vol[t] = (uint8_t)v; n++; }
    }
    return n;
}

/* bit t = track t hides the lead: the harmony or bass as loud as the lead (or louder), or the drums louder than it */
int tl_mix_flags(const DoomoSong *s)
{
    int t, f = 0;
    for (t = 1; t < SONG_TRACKS; t++)
        if ((t == 3 ? s->vol[t] > s->vol[0] : s->vol[t] >= s->vol[0]) && s->vol[t] > 6) f |= 1 << t;
    return f;
}

/* ---- the lyric syllable fitter ----------------------------------------------------------------------------------- */
/* How the words of lyric line `line` sit on the notes it is sung over: words, notes, and the verdict
 * 0 = fits, 1 = spare notes (la la), 2 = spare words (they are dropped). -1 when the line is empty or not sung. */
int tl_fit_status(const DoomoSong *s, int line, int *words, int *notes)
{
    int pos = studio_line_pos(s, line), w, n;
    if (pos < 0 || !s->lyrics[line][0]) return -1;
    w = song_syllables(s->lyrics[line], NULL, 0);
    n = song_lead_onsets(s, s->order[pos] & 3);
    if (words) *words = w;
    if (notes) *notes = n;
    return w == n ? 0 : (w < n ? 1 : 2);
}
