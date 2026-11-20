/* studio_play.c - the PLAY tab of the Music Studio (ST-PLAY, docs/design/STUDIO_PLAN.md 2.4; owner: studio).
 *
 * JAM       the pads play the song's own scale and chords: no wrong notes. Notes: A B L R (GBA: four of the eight
 *           scale degrees at a time) and, on the PS1 / Dreamcast / PC pads, X Y L2 R2 for degrees 5..8. Chords:
 *           the d-pad (Left Down Right Up) from the song's own bass line (or I IV V vi). A held pad holds its
 *           note (song_jam_on / song_jam_off: the track's own channel and instrument).
 * RECORD    SELECT+START: a count-in of four clicks, then the chosen part of the pattern loops
 *           (song_play_range) and the pads write into the lead / harmony / bass (or drum) track, quantized to
 *           off / 1/4 / 1/8 / 1/16 (default 1/16). The step is decided from the player's tick position
 *           (song_pos_ticks), a held pad writes tie cells, and the whole take is ONE undo step (the
 *           journal's ed_touch once at the start of the take, ed_commit at its end). Overdub keeps the old
 *           notes; Replace clears the looped part when the take starts.
 * DRUMS     the same pads play the kit (kick, snare, hat, open hat, clap, tom, crash, cowbell) and record
 *           into the drum track with the same quantize.
 * SELECT    held: UP / DOWN octave, A chord set (the song's / I IV V vi), LEFT / RIGHT the high or low four
 *           notes (GBA), START record. A clean tap of SELECT opens the tools menu (every setting by name).
 *
 * The pads' state is 64 bytes of the shared scene scratch (st_tab_state), never a static: EWRAM / IWRAM are full.
 */
#include "studio_int.h"

enum { Q_OFF, Q_4, Q_8, Q_16, Q_COUNT };
enum { REC_IDLE, REC_COUNT, REC_ON };
enum { RANGE_WHOLE, RANGE_BAR1, RANGE_BAR2, RANGE_COUNT };

#if defined(DOOMO_PLATFORM_GBA)
#define PLAY_KEYS 4                  /* the GBA has A B L R; the other pads add X Y L2 R2 */
#else
#define PLAY_KEYS 8
#endif

typedef struct PlayState {
    uint8_t  init;                   /* [0]: managed by st_tab_state */
    uint8_t  mode;                   /* 0 notes, 1 drums */
    int8_t   oct;                    /* -2..+2 octaves */
    uint8_t  row;                    /* GBA: 0 = degrees 1..4, 1 = 5..8 */
    uint8_t  chords;                 /* 0 the song's chords, 1 I IV V vi */
    uint8_t  quant;                  /* Q_* */
    uint8_t  trk;                    /* 0..2 the melodic track a take writes to */
    uint8_t  replace;                /* 0 overdub, 1 replace */
    uint8_t  range;                  /* RANGE_* */
    uint8_t  rec;                    /* REC_* */
    uint8_t  beats;                  /* count-in clicks sounded */
    uint8_t  take_open, take_n, take_cleared;
    uint8_t  last_k;                 /* the melody pad pressed last, +1 (it wins when several are held) */
    uint8_t  hold_step, hold_trk;
    uint16_t hold_btn, hold_t;       /* the pad that holds the latest recorded note, ticks held */
    uint16_t cacc;                   /* count-in beat accumulator */
    uint8_t  stale;                  /* a setting changed: recompute the notes below at the next tick */
    uint8_t  metro, metro_step;      /* optional beat click and last clicked sequencer step */
    uint8_t  roots[4];               /* the song's chord roots (pitch classes) */
    uint8_t  notes[8];               /* the notes the melody pads play now (a few divisions each: kept, not redone per frame) */
    uint8_t  ctri[4][3];             /* the chords' notes: root (low), third, fifth */
} PlayState;
DOOMO_STATIC_ASSERT(sizeof(PlayState) <= STUDIO_TABSTATE, play_state_fits_tab_state);

static const char *const trk_names[3] = { "Lead", "Harmony", "Bass" };
static const char *const q_names[Q_COUNT] = { "off", "1/4", "1/8", "1/16" };
/* The words the draw shows, whole (the header ran doomo_snprintf six times a frame; play_*_text below build the same text
 * without it: pixel-identical, tests/test_studio_play.c checks every combination against the old formats). */
static const char *const q_text[2][Q_COUNT] = {
    { "Q off", "Q 1/4", "Q 1/8", "Q 1/16" },
    { "Quantize off", "Quantize 1/4", "Quantize 1/8", "Quantize 1/16" } };
static const char *const oct_text[5] = { "Oct -2", "Oct -1", "Oct 0", "Oct +1", "Oct +2" };    /* oct is -2..+2 */
static const char *const count_text[5] = { "Count 0", "Count 1", "Count 2", "Count 3", "Count 4" };   /* beats is 0..4 */
static const char *const range_names[RANGE_COUNT] = { "whole pattern", "bar 1", "bar 2" };
static const int8_t trk_center[3] = { 72, 62, 48 };
static const uint16_t melody_keys[8] = { BTN_A, BTN_B, BTN_L, BTN_R, BTN_X, BTN_Y, BTN_L2, BTN_R2 };
static const uint16_t chord_keys[4] = { BTN_LEFT, BTN_DOWN, BTN_RIGHT, BTN_UP };
static const uint16_t drum_keys[8] = { BTN_A, BTN_B, BTN_L, BTN_R, BTN_LEFT, BTN_DOWN, BTN_RIGHT, BTN_UP };
static const uint8_t drum_of_key[8] = { SONG_DRUM_KICK, SONG_DRUM_SNARE, SONG_DRUM_HAT, SONG_DRUM_OPEN_HAT,
                                        SONG_DRUM_CLAP, SONG_DRUM_TOM, SONG_DRUM_CRASH, SONG_DRUM_COWBELL };

/* ====================================================================== */
/* pure helpers (unit tested: core/tests/test_studio_play.c)                */

/* The nearest MIDI note with pitch class pc to `center` (within -6..+5). */
static int near_pc(int pc, int center)
{
    int lo = center - 6;
    return lo + (((pc - lo) % 12) + 12) % 12;
}

/* n folded by octaves into the editor's note range (the pitch class stays: a clamp would leave the scale) */
static int fold_octaves(int n)
{
    while (n < SONG_NOTE_MIN) n += 12;
    while (n > SONG_NOTE_MAX) n -= 12;
    return n;
}

/* Scale degree `degree` (0 = the tonic) of the song's key / scale on track trk (0..2), `oct` octaves up. Always in
 * the scale (song_scale_snap(s, n) == n). */
int play_degree_note(const DoomoSong *s, int trk, int oct, int degree)
{
    int tonic = near_pc(s->key % 12, trk_center[DOOMO_CLAMP(trk, 0, 2)]) + 12 * oct;
    tonic = song_scale_snap(s, fold_octaves(tonic));
    return fold_octaves(song_scale_shift(s, tonic, degree));
}

/* The triad of the chord whose root has pitch class pc: out[0] root (low), out[1] third, out[2] fifth (the thirds are
 * stacked in scale steps, so every note is in the scale). */
void play_chord_notes(const DoomoSong *s, int pc, int oct, int out[3])
{
    int up = song_scale_snap(s, fold_octaves(near_pc(pc, 62) + 12 * oct));
    out[0] = song_scale_snap(s, fold_octaves(near_pc(pc, 48) + 12 * oct));
    out[1] = fold_octaves(song_scale_shift(s, up, 2));
    out[2] = fold_octaves(song_scale_shift(s, up, 4));
}

/* The four chord roots (pitch classes) the d-pad plays: the first notes of the bars of the song's bass track in
 * the order they come (snapped into the scale), filled up with I IV V vi. basic != 0: just I IV V vi. */
void play_song_roots(const DoomoSong *s, int basic, uint8_t out[4])
{
    static const uint8_t deg[4] = { 0, 3, 4, 5 };
    int n = 0, i, o, b, k, j;
    if (!basic)
        for (o = 0; o < s->order_len && n < 4; o++) {
            int p = s->order[o] % SONG_PATTERNS;
            for (b = 0; b < 2 && n < 4; b++)
                for (k = b * 16; k < b * 16 + 16; k++) {
                    int v = s->notes[p][2][k], pc, dup = 0;
                    if (v < 2) continue;
                    pc = song_scale_snap(s, v) % 12;
                    for (j = 0; j < n; j++) if (out[j] == pc) dup = 1;
                    if (!dup) out[n++] = (uint8_t)pc;
                    break;                        /* the first note of the bar only */
                }
        }
    for (i = 0; i < 4 && n < 4; i++) {
        int pc = song_scale_shift(s, song_scale_snap(s, 60 + s->key % 12), deg[i]) % 12, dup = 0;
        for (j = 0; j < n; j++) if (out[j] == pc) dup = 1;
        if (!dup) out[n++] = (uint8_t)pc;
    }
    for (i = 0; n < 4; i++)                       /* a scale so small that the defaults repeat */
        out[n++] = (uint8_t)((s->key + i * 7) % 12);
}

/* Where in the looped steps from..to-1 the player is `ticks` after song_play_range() (song_pos_ticks): the step, and
 * how far into it, 0..255. Exact with swing: the loop is periodic, so the ticks are reduced to one period first. */
int play_loop_pos(const DoomoSong *s, int from, int to, uint32_t ticks, int *frac)
{
    uint32_t g = 16u * (uint32_t)DOOMO_CLAMP(s->bpm, 60, 220), sw = (uint32_t)DOOMO_MIN(s->swing, 8) * 450u;
    uint32_t len = 0, a, b, u;
    int i;
    for (i = from; i < to; i++) len += (i & 1) ? 7200u - sw : 7200u + sw;
    a = len; b = g;
    while (b) { uint32_t t = a % b; a = b; b = t; }                       /* gcd */
    u = ((ticks % (len / a)) * g) % len;
    for (i = from; i < to; i++) {
        uint32_t su = (i & 1) ? 7200u - sw : 7200u + sw;
        if (u < su) { if (frac) *frac = (int)(u * 256u / su); return i; }
        u -= su;
    }
    if (frac) *frac = 0;
    return from;
}

/* The step a note pressed at (step, frac/256 into it) is written to: Q_OFF the step you were in; 1/16 the nearest step;
 * 1/8 the nearest even step; 1/4 the nearest beat. Rounding up past the end of the loop wraps to its start. */
int play_quantize_step(int step, int frac, int q, int from, int to)
{
    int r = step, unit, cnt = to - from;
    if (q != Q_OFF) {
        unit = q == Q_4 ? 1024 : q == Q_8 ? 512 : 256;
        r = ((step * 256 + frac + unit / 2) / unit) * (unit / 256);
    }
    if (r < from) r = from;
    if (r >= to) r = from + (r - from) % cnt;
    return r;
}

/* ====================================================================== */
/* state                                                                    */
static void refresh(PlayState *p);

static PlayState *ps(void)
{
    int fresh = 0;
    PlayState *p = (PlayState *)st_tab_state(&fresh);
    if (p && fresh) {
        if (studio.undo_open) ed_commit();               /* the scratch was taken by another scene mid-take: close its step */
        p->quant = Q_16;
        p->metro_step = 0xFF;
        refresh(p);
    }
    return p;
}

static void range_steps(const PlayState *p, int *from, int *to)
{
    *from = p->range == RANGE_BAR2 ? 16 : 0;
    *to = p->range == RANGE_BAR1 ? 16 : SONG_STEPS;
}

static uint8_t *take_row(const PlayState *p)
{
    return SS->notes[studio.pat & 3][p->mode ? 3 : p->trk];
}

/* One answer to "what do the pads play": the chord roots follow the song as it is edited (a scan of at most 16 x 32
 * bass cells), and the notes cost a few divisions each. Redone when a setting changes, when the tab opens and eight
 * times a second; never per frame. */
static int pad_note_calc(const PlayState *p, int k)
{
    int deg = k;
#if defined(DOOMO_PLATFORM_GBA)
    deg = p->row * 4 + k;
#endif
    return play_degree_note(SS, p->trk, p->oct, deg);
}

static void refresh(PlayState *p)
{
    int k, c;
    p->stale = 0;
    play_song_roots(SS, p->chords, p->roots);
    for (k = 0; k < PLAY_KEYS; k++) p->notes[k] = (uint8_t)pad_note_calc(p, k);
    for (c = 0; c < 4; c++) {
        int t[3];
        play_chord_notes(SS, p->roots[c], p->oct, t);
        p->ctri[c][0] = (uint8_t)t[0]; p->ctri[c][1] = (uint8_t)t[1]; p->ctri[c][2] = (uint8_t)t[2];
    }
}

/* The eight-times-a-second refresh, spread over four ticks instead of one (PLAY-32: the refresh was about 1.1 ms in a
 * single tick, and it landed on ticks where the player starts a note, which is the loop's one slow frame). Same
 * values as refresh(), a few ticks later; a setting change, the tab opening and a chord press still call the whole
 * refresh() at once. */
static void refresh_part(PlayState *p, int phase)
{
    int k, c;
    switch (phase) {
    case 0:
        play_song_roots(SS, p->chords, p->roots);
        break;
    case 1:
        for (k = 0; k < PLAY_KEYS / 2; k++) p->notes[k] = (uint8_t)pad_note_calc(p, k);
        break;
    case 2:
        for (k = PLAY_KEYS / 2; k < PLAY_KEYS; k++) p->notes[k] = (uint8_t)pad_note_calc(p, k);
        break;
    case 3:
        for (c = 0; c < 4; c++) {
            int t[3];
            play_chord_notes(SS, p->roots[c], p->oct, t);
            p->ctri[c][0] = (uint8_t)t[0]; p->ctri[c][1] = (uint8_t)t[1]; p->ctri[c][2] = (uint8_t)t[2];
        }
        break;
    default:
        break;
    }
}

/* ====================================================================== */
/* the jam: pads -> held notes                                              */
static int pad_note(const PlayState *p, int k)
{
    return p->notes[k & 7];
}

static int chord_root_note(const PlayState *p, int c)
{
    return fold_octaves(near_pc(p->roots[c & 3], trk_center[p->trk]) + 12 * p->oct);
}

/* make the held notes follow the pads held now */
static void jam_sync(PlayState *p)
{
    uint8_t want[3] = { 0, 0, 0 };
    int c, k, ch;
    if (p->mode) return;
    for (c = 0; c < 4; c++)
        if (btn_down(chord_keys[c])) {
            want[0] = p->ctri[c][2]; want[1] = p->ctri[c][1]; want[2] = p->ctri[c][0];
        }
    for (k = 0; k < PLAY_KEYS; k++)
        if (btn_down(melody_keys[k])) want[p->trk] = (uint8_t)pad_note(p, k);
    if (p->last_k && p->last_k <= PLAY_KEYS && btn_down(melody_keys[p->last_k - 1]))
        want[p->trk] = (uint8_t)pad_note(p, p->last_k - 1);
    for (ch = 0; ch < 3; ch++) {
        int cur = song_jam_note(ch);
        if (want[ch] == cur) continue;
        if (want[ch]) song_jam_on(SS, ch, want[ch]);
        else song_jam_off(ch);
    }
}

/* ====================================================================== */
/* recording                                                                */
static void take_begin(PlayState *p)
{
    if (p->take_open) return;
    ed_touch(take_row(p), SONG_STEPS);
    p->take_open = 1;
    p->take_n = 0;
}

static void hold_release(PlayState *p)
{
    int n, i, from, to;
    uint8_t *row;
    uint32_t units = (uint32_t)p->hold_t * 16u * (uint32_t)DOOMO_CLAMP(SS->bpm, 60, 220);
    p->hold_btn = 0;
    if (!p->take_open || p->rec != REC_ON) return;
    range_steps(p, &from, &to);
    n = (int)(p->quant == Q_OFF ? units / 7200u : (units + 3600u) / 7200u);
    row = SS->notes[studio.pat & 3][p->hold_trk];
    for (i = 1; i < n; i++) {                         /* the note and n - 1 tie cells */
        int st = p->hold_step + i;
        if (st >= to || row[st] != SONG_REST) break;
        row[st] = SONG_HOLD;
    }
}

static void rec_event(PlayState *p, int cell, uint16_t key)
{
    int from, to, frac, step, trk = p->mode ? 3 : p->trk;
    uint8_t *row;
    range_steps(p, &from, &to);
    step = play_loop_pos(SS, from, to, song_pos_ticks(), &frac);
    step = play_quantize_step(step, frac, p->quant, from, to);
    if (p->hold_btn) hold_release(p);                 /* a new note ends the held one */
    take_begin(p);
    row = take_row(p);
    row[step] = (uint8_t)cell;
    p->take_n++;
    if (trk < 3) {
        studio.last[trk] = (uint8_t)cell;
        p->hold_btn = key;
        p->hold_step = (uint8_t)step;
        p->hold_trk = (uint8_t)trk;
        p->hold_t = 0;
    }
}

static void count_click(PlayState *p)
{
    song_jam_on(SS, 3, p->beats == 0 ? SONG_DRUM_KICK : SONG_DRUM_HAT);
}

static void rec_begin_loop(PlayState *p)
{
    int from, to;
    range_steps(p, &from, &to);
    p->rec = REC_ON;
    p->take_cleared = 0;
    if (p->replace) {
        uint8_t *row;
        take_begin(p);
        row = take_row(p);
        memset(row + from, 0, (size_t)(to - from));
        p->take_cleared = 1;
    }
    song_play_range(SS, studio.pat, from, to);
    audio_sfx(SFX_SELECT);
}

/* finish the take: its notes are one undo step */
static void take_end(PlayState *p)
{
    if (p->hold_btn) hold_release(p);
    if (p->rec == REC_COUNT) {
        st_msg("Count-in stopped");
    } else if (p->rec == REC_ON) {
        if (p->take_open && p->take_n == 0 && p->take_cleared) {
            ed_commit();
            ed_undo();                                /* a Replace that recorded nothing: as it was */
            st_msg("No notes: nothing changed");
        } else if (p->take_open) {
            ed_commit();
            st_msg("Take saved: %d notes", p->take_n);
            audio_sfx(SFX_SELECT);
        } else {
            st_msg("No notes: nothing changed");
        }
    }
    p->take_open = 0;
    p->take_n = 0;
    p->take_cleared = 0;
    p->rec = REC_IDLE;
    p->beats = 0;
    song_stop();
}

static void rec_start(PlayState *p)
{
    if (song_playing()) song_stop();
    song_jam_off(-1);
    p->rec = REC_COUNT;
    p->beats = 0;
    p->cacc = 0;
    count_click(p);
    p->beats = 1;
    st_msg("Recording %s: count-in", p->mode ? "Drums" : trk_names[p->trk]);
}

static void loop_toggle(PlayState *p)
{
    int from, to;
    if (p->rec) { take_end(p); return; }
    if (song_playing()) { song_stop(); st_msg("Stopped"); return; }
    range_steps(p, &from, &to);
    song_play_range(SS, studio.pat, from, to);
    st_msg("Looping %s of %c", range_names[p->range], 'A' + studio.pat);
}

/* a tick of the recorder: count-in beats, the loop, a held pad */
static void service(PlayState *p)
{
    int step;
    if (p->stale || studio.tab_t < 2) refresh(p);
    else if ((studio.t & 7) < 4) refresh_part(p, studio.t & 7);       /* roots, notes, notes, chords: one a tick */
    if (p->take_open && !studio.undo_open) {          /* an undo took the open take away */
        p->take_open = 0;
        p->take_n = 0;
        p->take_cleared = 0;
    }
    if (p->rec == REC_COUNT) {
        p->cacc = (uint16_t)(p->cacc + 16 * DOOMO_CLAMP(SS->bpm, 60, 220));
        if (p->cacc >= 28800u) {                      /* a beat: 4 steps of 7200 units */
            p->cacc = (uint16_t)(p->cacc - 28800u);
            if (p->beats < 4) { count_click(p); p->beats++; }
            else rec_begin_loop(p);
        }
    } else if (p->rec == REC_ON && !song_playing()) {
        take_end(p);                                  /* the loop ended under us */
    }
    step = song_playing() ? song_pos_step() : -1;
    if (p->metro && p->rec != REC_COUNT && step >= 0 && step != p->metro_step && (step & 3) == 0)
        audio_sfx(SFX_TIK);
    p->metro_step = (uint8_t)(step < 0 ? 0xFF : step);
    if (p->hold_btn) {
        if (btn_down(p->hold_btn)) { if (p->hold_t < 60000) p->hold_t++; }
        else hold_release(p);
    }
}

/* ====================================================================== */
/* tools menu                                                               */
#if defined(DOOMO_PLATFORM_GBA)
static void say_row(const PlayState *p)
{
    st_msg(p->row ? "Pads A B L R: notes 5-8" : "Pads A B L R: notes 1-4");
}
#endif

static void play_menu_done(int c)
{
    PlayState *p = ps();
    if (!p || c < 0) return;
    switch (c) {
    case 0:
        p->mode ^= 1;
        st_msg(p->mode ? "Drum pads: A B L R + d-pad" : "Note pads: your song's scale");
        break;
    case 1:
        if (p->mode) { st_msg("Drum pads record into Drums"); break; }
        p->trk = (uint8_t)((p->trk + 1) % 3);
        p->stale = 1;
        st_msg("Pads play and record %s", trk_names[p->trk]);
        break;
    case 2:
        p->quant = (uint8_t)((p->quant + 1) % Q_COUNT);
        st_msg("Quantize: %s", q_names[p->quant]);
        break;
    case 3:
        p->replace ^= 1;
        st_msg(p->replace ? "Replace: a take clears the loop" : "Overdub: keeps the old notes");
        break;
    case 4:
        p->range = (uint8_t)((p->range + 1) % RANGE_COUNT);
        st_msg("Loop: %s", range_names[p->range]);
        break;
    case 5:
        studio.pat = (uint8_t)((studio.pat + 1) % SONG_PATTERNS);
        st_msg("Pattern %c", 'A' + studio.pat);
        break;
    case 6:
        p->chords ^= 1;
        p->stale = 1;
        st_msg(p->chords ? "Chords: I IV V vi" : "Chords: from your song");
        break;
    case 7:
#if defined(DOOMO_PLATFORM_GBA)
        p->row ^= 1;
        p->stale = 1;
        say_row(p);
#else
        rec_start(p);                       /* the route for a pad that cannot make SELECT+START (Dreamcast) */
#endif
        break;
    case 8:
        p->metro ^= 1;
        p->metro_step = 0xFF;
        st_msg(p->metro ? "Metronome: on" : "Metronome: off");
        break;
    default: break;
    }
}

static void open_menu(PlayState *p)
{
    static const char *const items[9] = { "Notes / Drums", "Next track", "Quantize", "Overdub / Replace",
                                          "Loop part", "Next pattern", "Chord set",
#if defined(DOOMO_PLATFORM_GBA)
                                          "Notes 1-4 / 5-8", "Metronome on / off" };
#else
                                          "Record a take", "Metronome on / off" }; /* a menu already open while recording stops the take */
#endif
    if (p->rec) take_end(p);
    song_jam_off(-1);
    st_popup("Play tools", items, 9, 0, play_menu_done);
}

/* ====================================================================== */
/* input                                                                    */
void tab_play_leave(void)
{
    PlayState *p = ps();
    if (p && p->rec) take_end(p);
    song_jam_off(-1);
    song_stop();
}

void tab_play_shift(void)
{
    PlayState *p = ps();
    if (!p) return;
    service(p);
    if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) {
        int d = btn_pressed(BTN_UP) ? 1 : -1;
        if (p->mode) {
            st_msg("Drums have no octave");
        } else if (p->oct + d < -2 || p->oct + d > 2) {
            st_msg(d > 0 ? "Highest octave" : "Lowest octave");
            audio_sfx(SFX_ERROR);
        } else {
            p->oct = (int8_t)(p->oct + d);
            p->stale = 1;
            st_msg("Octave %s%d", p->oct > 0 ? "+" : "", p->oct);
            audio_sfx(SFX_CURSOR);
        }
    }
#if defined(DOOMO_PLATFORM_GBA)
    if (btn_pressed(BTN_LEFT) || btn_pressed(BTN_RIGHT)) {
        p->row = (uint8_t)(btn_pressed(BTN_RIGHT) ? 1 : 0);           /* RIGHT: the high four, LEFT: the low four */
        p->stale = 1;
        say_row(p);
        audio_sfx(SFX_CURSOR);
    }
#endif
    if (btn_pressed(BTN_A)) {
        play_menu_done(6);
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_START)) {
        studio.sel_used = 1;
        if (p->rec) take_end(p);
        else rec_start(p);
    }
    /* pads let go of while SELECT is held: no stuck notes */
    jam_sync(p);
}

void tab_play_update(void)
{
    PlayState *p = ps();
    int k;
    if (!p) return;
    service(p);
    if (studio_sel_tap) { open_menu(p); return; }
    if (btn_pressed(BTN_START)) loop_toggle(p);
    if (p->mode) {
        for (k = 0; k < 8; k++)
            if (btn_pressed(drum_keys[k])) {
                song_jam_on(SS, 3, drum_of_key[k]);
                if (p->rec == REC_ON) rec_event(p, drum_of_key[k], drum_keys[k]);
            }
        return;
    }
    if (btn_pressed(BTN_LEFT | BTN_DOWN | BTN_RIGHT | BTN_UP)) refresh(p);       /* a chord: the song as it is now */
    for (k = 0; k < PLAY_KEYS; k++)
        if (btn_pressed(melody_keys[k])) {
            p->last_k = (uint8_t)(k + 1);
            if (p->rec == REC_ON) rec_event(p, pad_note(p, k), melody_keys[k]);
        }
    for (k = 0; k < 4; k++)
        if (btn_pressed(chord_keys[k]) && p->rec == REC_ON)
            rec_event(p, chord_root_note(p, k), chord_keys[k]);       /* a chord records its root */
    jam_sync(p);
}

int play_prompts(UiPrompt *pr)
{
    PlayState *p = ps();
    int n = 0;
#define PR(b, pri, v) do { pr[n].btn = (uint16_t)(b); pr[n].prio = (uint8_t)(pri); pr[n].verb = (v); n++; } while (0)
    if (studio.sel_held) {                              /* SELECT is down: the shift keys */
        if (!p || !p->mode) PR(BTN_UP | BTN_DOWN, 0, "Octave");
#if !defined(DOOMO_PLATFORM_DC)                       /* the Dreamcast pad makes SELECT from START: record is in the menu */
        PR(BTN_START, 1, p && p->rec ? "Stop" : "Record");
#endif
        if (!p || !p->mode) PR(BTN_A, 2, "Chords");
#if defined(DOOMO_PLATFORM_GBA)
        if (!p || !p->mode) PR(BTN_LEFT | BTN_RIGHT, 3, "Notes");
#endif
        PR(BTN_B, 4, "Undo");
        return n;
    }
    if (p && p->rec) {
        PR(BTN_START, 0, "Stop");
        PR(BTN_A, 1, "Note");
        PR(BTN_SELECT | BTN_B, 3, "Undo");
        return n;
    }
    PR(BTN_A, 0, "Note");
    PR(BTN_START, 1, "Play");
#if defined(DOOMO_PLATFORM_DC)
    PR(BTN_SELECT, 2, "Tools, record");
#else
    PR(BTN_SELECT | BTN_START, 2, "Record");
    PR(BTN_SELECT, 3, "Tools");
#endif
#undef PR
    return n;
}

/* ====================================================================== */
/* the header and chord texts (pure, no doomo_snprintf: see q_text)         */
const char *play_q_text(int q, int wide)
{
    return q_text[wide ? 1 : 0][DOOMO_CLAMP(q, 0, Q_COUNT - 1)];
}

const char *play_count_text(int beats, char *buf, int cap)
{
    if (beats >= 0 && beats < 5) return count_text[beats];
    doomo_snprintf(buf, (size_t)cap, "Count %d", beats);
    return buf;
}

static int put_text(char *d, int n, int cap, const char *s)
{
    while (*s && n < cap) d[n++] = *s++;
    return n;
}

/* The line under the header: "Oct +1  Notes 5-8  Overdub  Loop bar 1" (notes; "Notes 1-4 / 5-8" only where the pads are
 * four, keys4) or "Overdub  Loop bar 1" (drums). Cut at 43 characters like the 44-byte snprintf buffer it replaces. */
void play_head_text(char *buf, int drums, int oct, int row, int keys4, int replace, int range)
{
    int n = 0, cap = 43;
    if (!drums) {
        n = put_text(buf, n, cap, oct_text[DOOMO_CLAMP(oct + 2, 0, 4)]);
        n = put_text(buf, n, cap, "  ");
        if (keys4) n = put_text(buf, n, cap, row ? "Notes 5-8  " : "Notes 1-4  ");
    }
    n = put_text(buf, n, cap, replace ? "Replace" : "Overdub");
    n = put_text(buf, n, cap, "  Loop ");
    n = put_text(buf, n, cap, range_names[DOOMO_CLAMP(range, 0, RANGE_COUNT - 1)]);
    buf[n] = 0;
}

/* A chord pad's name: the root ("C", "F#") and "m" for a minor third, "o" for a diminished one (buf: 4 bytes) */
void play_chord_text(char *buf, int root_pc, int third)
{
    int n = put_text(buf, 0, 2, doomo_song_key_names[((root_pc % 12) + 12) % 12]);
    if (third == 3) buf[n++] = 'm';
    else if (third == 6) buf[n++] = 'o';
    buf[n] = 0;
}

/* ====================================================================== */
/* drawing                                                                  */
/* A pad is drawn straight: the round rect and its shadow line (~6 rect calls a pad, 8 pads). Do NOT bake it
 * (st_bake): PLAY-30 tried, and on the GBA a ~1,300 px blit of a mostly solid pad costs far more than the few
 * rect fills it replaces (frame +4 ms, vid_blit 3.9% -> 21.6%, loop peak past 33.3 ms). Reverted; see status/STUDIO.md. */
static void pad_cell(int x, int y, int w, int h, uint8_t base, int down, const char *key, const char *label)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    ui_round_rect(x, y, w, h, 4, down ? PAL_PAINT(PAINT_ROSE, 1) : base);
    ST_RECT(x + 3, y + h, w - 6, 1, down ? PAL_PAINT(PAINT_ROSE, 0) : st->outline);
    if (key && key[0] && ui_text_width(key) < w / 2) ui_text(x + 3, y + 2, key, down ? PAL_WHITE : st->text_dim);
    if (label && label[0])
        ui_text(x + (w - ui_text_width(label)) / 2, y + h / 2 - 2, label, down ? PAL_WHITE : st->text);
}

static const char *pad_key_name(uint16_t b)
{
    const char *n = btn_name(b);
    return n ? n : "";
}

void tab_play_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    PlayState *p = ps();
    int wide = st_wide(), gap = 3, cw, ch, x0, y, rows, i, k, trk, from, to, rx;
    char buf[44], nm[6];
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    if (!p) return;
    trk = p->mode ? 3 : p->trk;
    range_steps(p, &from, &to);
    /* header: what the pads play, where it goes, the state of the recorder */
    {
        const char *tn = p->mode ? "Drums" : trk_names[p->trk];
        int tw = ui_text_width(tn) + 10;
        ui_round_rect(r.x + 6, r.y + 4, tw, 11, 4, st_track_color(trk, 1));
        ui_text(r.x + 11, r.y + 6, tn, PAL_WHITE);
        ui_round_rect(r.x + 10 + tw, r.y + 4, 16, 11, 4, st_pattern_color(studio.pat, 1));
        nm[0] = (char)('A' + studio.pat); nm[1] = 0;
        ui_text(r.x + 15 + tw, r.y + 6, nm, PAL_WHITE);
        ui_text(r.x + 34 + tw, r.y + 6, play_q_text(p->quant, wide), st->text);
    }
    rx = r.x + r.w - 6;
    if (p->rec == REC_COUNT) {
        const char *ct = play_count_text(p->beats, buf, (int)sizeof(buf));
        rx -= ui_text_width(ct);
        ui_text(rx, r.y + 6, ct, PAL_PAINT(PAINT_ROSE, 0));
    } else if (p->rec == REC_ON) {
        rx -= ui_text_width("REC");
        ui_text(rx, r.y + 6, "REC", PAL_PAINT(PAINT_CHERRY, 0));
        if (ui_wave((int)studio.t, 16, 1) > 0) ui_fill_circle(rx - 6, r.y + 10, 3, PAL_PAINT(PAINT_CHERRY, 1));
    } else if (song_playing()) {
        rx -= ui_text_width("Loop");
        ui_text(rx, r.y + 6, "Loop", PAL_PAINT(PAINT_MINT, 0));
    }
    play_head_text(buf, p->mode, p->oct, p->row, PLAY_KEYS == 4, p->replace, p->range);
    ui_text(r.x + 8, r.y + 18, buf, st->text_dim);
    /* the pads */
    rows = p->mode ? 2 : (PLAY_KEYS == 8 ? 3 : 2);
    cw = (r.w - 16 - 3 * gap) / 4;
    ch = (r.h - 30 - 20 - (rows - 1) * gap) / rows;
    if (ch > (wide ? 34 : 26)) ch = wide ? 34 : 26;
    x0 = r.x + 8;
    y = r.y + 30;
    if (p->mode) {
        for (i = 0; i < 8; i++) {
            int px = x0 + (i & 3) * (cw + gap), py = y + (i >> 2) * (ch + gap), d = drum_of_key[i];
            pad_cell(px, py, cw, ch, st_track_color(3, 2), btn_down(drum_keys[i]), pad_key_name(drum_keys[i]), "");
            st_drum_icon(d, px + cw / 2, py + ch / 2 - 1, (int)studio.t);
            if (cw >= 40) ui_text(px + cw - 4 - ui_text_width(doomo_song_drum_names[d]), py + ch - 9,
                                  doomo_song_drum_names[d], st->text_dim);
        }
    } else {
        for (i = 0; i < PLAY_KEYS; i++) {
            int px = x0 + (i & 3) * (cw + gap), py = y + (i >> 2) * (ch + gap);
            song_note_name(pad_note(p, i), nm);
            pad_cell(px, py, cw, ch, st_track_color(p->trk, 2), btn_down(melody_keys[i]), pad_key_name(melody_keys[i]), nm);
        }
        y += ((PLAY_KEYS + 3) / 4) * (ch + gap);
        for (i = 0; i < 4; i++) {
            int px = x0 + i * (cw + gap), third;
            third = (p->ctri[i][1] % 12 + 12 - p->ctri[i][0] % 12) % 12;
            play_chord_text(nm, p->roots[i], third);
            pad_cell(px, y, cw, ch, PAL_PAINT(PAINT_SKY, 2), btn_down(chord_keys[i]), pad_key_name(chord_keys[i]), nm);
        }
    }
    /* the 32 steps of the pattern for the track the pads write to: the loop, the playhead and what is there */
    {
        int sw = r.w - 16, cs = sw / SONG_STEPS, gx = r.x + 8 + (sw - cs * SONG_STEPS) / 2, gy = r.y + r.h - 16;
        const uint8_t *row = SS->notes[studio.pat & 3][trk];
        int head = song_playing() ? song_pos_step() : -1;
        plat_rect(gx, gy, cs * SONG_STEPS, 11, st->fill_lo);
        plat_rect(gx + from * cs, gy - 2, (to - from) * cs, 1, st_track_color(trk, 0));
        for (k = 0; k < SONG_STEPS; k++) {
            int v = row[k];
            if (v == SONG_REST) continue;
            plat_rect(gx + k * cs, gy + 1, cs - 1, 9, st_track_color(trk, v == SONG_HOLD ? 2 : 0));
        }
        if (head >= 0) plat_rect(gx + head * cs, gy, cs, 11, PAL_PAINT(PAINT_LEMON, 1));
        for (k = 0; k < SONG_STEPS; k += 4) plat_rect(gx + k * cs, gy + 11, 1, 2, st->text_dim);
    }
}
