/* music.c - the soundtrack of the DOOMOGACHI LIFE teaser.
 *
 * tk_music() schedules every note of the score on the toolkit synth
 * (tk_note / tk_slide / tk_kick / tk_snare / tk_hat). The score is written as
 * little tracker-style strings - "E5:2 E5:2 G5:2 E5:2 C6:4 G5:4" - lengths in
 * 16th notes, and every section is pinned to the scene list in timeline.c
 * with tk_scene_start(), so re-timing a scene drags its music along.
 *
 * Map of the score (times for the current 78 s timeline):
 *
 *  0.47  s01  ident: music-box bells over a C-augmented shimmer that resolves
 *             to C major ... and freezes mid-arpeggio with the heart (1.53).
 *  2.50  s02  drum pickup -> the HOOK slams in with "LIFE" (3.13, 140 bpm,
 *             C major). When HELL starts to drip the top note droops
 *             C6 -> B5 -> Bb5 and the chords sour to C minor, then thin out
 *             to a nervous Ab whisper so the scene's "uh-oh" + jitter
 *             carry it (the wrong note). Dead stop for the fling + SPLAT.
 *  5.64       ...vamp sneaks back in like nothing happened (F, under the
 *             whistle), into
 *  6.50  s03  the full cute theme for "Welcome to HaHaland!" - cut by the
 *             record scratch at the Wall (9.70, with a tape-stop droop).
 * 11.84  s04  Doomogachi: LCD-toy pet song (pulse + sine), hatch on the
 *             downbeat (12.70), a snare roll while it strains, a "ta-da!" on
 *             the poop (14.84). Muted for the four frames of hell (15.77),
 *             then it carries on... in C minor, wobbling. "it's fine."
 * 17.00  s05  creator: royal-road IV-V-iii-vi groove (F G Em Am), arps go
 *             double-time when the cycling speeds up; stops dead when it
 *             lands on Kevin (20.33). Two-beat pickup back in at 21.14.
 * 22.00  s06  "Make friends! Fall in love!" hook + a button on C (25.43),
 *             then silence for the guy in the dark room.
 * 27.00  s07  grand opening: the hook again, but its last note slips to a
 *             tritone and a low Db saw creeps in under the C; Vex gets a
 *             chromatic lament bass + minor line; the music holds its
 *             breath on a diminished swell during the inhale and is blown
 *             away by the FWOOMP (30.67). Silence.
 * 32.50  s08  the party: the hook bursts in at full tilt for half a second
 *             - RECORD SCRATCH (32.97) tape-stop - 0.47 s of silence -
 * 33.43       DOOM: a low E power-chord BWAAM as the sky cracks, a chromatic
 *             tremolo + snare roll build into the ROAR (34.75), then the
 *             main riff (E minor, 120 bpm, double-kick 16ths, snare
 *             backbeat) from 35.00, with hits on the two caption slams.
 * 38.50  s09  gallop riff for the workshop, twin-guitar harmony in bar 2,
 *             a big fill... into
 * 43.00  s10  CUTE MODE: the very same riff, transposed to C major on a
 *             toy pulse + music box with claps. BRUTAL MODE (45.50): the
 *             real riff. Dead silence for "(We don't judge.)" (47.50).
 *             Third person (49.50): riff + screaming lead (E harmonic
 *             minor run up to a wide whammy wobble).
 * 51.50  s11  half-time riff E-C-D with a choir (TRI/SINE triads). Dead stop
 *             for the close-up (53.50); the sad-trombone gags play alone.
 *             A snare roll as the cursor sneaks down (57.30), then MONSTER
 *             MODE (57.70): blast beat, tremolo chugs, evil choir with a
 *             tritone on top, a dive-bomb squeal.
 * 59.00  s12  census: a G tremolo + roll winds up during the pull-back, then
 *             a hit on every caption, metal drums under cute bells:
 *             C (1000 CITIZENS, 59.73) - F (500 GOOD, 60.53) - Ab gallop
 *             (500 BAD, 61.27) - Bb + a squeeze roll (1 PARK, 62.00) - and
 *             a C "ta-da" for the cork (62.60).
 * 63.00  s13  the hook, shy, on a music box ("...and maybe, just maybe...")
 *             over the scene's C pad; B trembles under the rumble; the wall
 *             CRUMBLES (64.20) = KEY CHANGE to E major: the ANTHEM - the cute
 *             hook on lead + saw, twin harmony, choir, open power chords,
 *             metal drums - cut dead when the camera finds the pile of shell
 *             casings (66.27). Silence. tink.
 * 68.00  s14  the cute theme returns (C major, 120 bpm, light, music box +
 *             echo) - stops dead for "Yes, the actual PlayStation." (70.47),
 *             back in on the HIGH FIVE (71.73) with a pickup into
 * 72.50  s15  the hook in C while the logo drops; HELL ignites (73.50) = the
 *             anthem in E with the metal drums (split screen, split band);
 *             final E major chord on 74.50, held and fading, its third
 *             slipping flat as it dies; silent from 76.11, before the cut to
 *             black (76.17) and the post-credits "beep." (the scene's).
 *
 * Levels: music voices sit around vol 0.1-0.5 so the scenes' sfx stay on
 * top; the score leaves holes (mute windows) for every comic beat and ducks
 * under the little sfx the jokes hang on (see ducks()).
 *
 * Synced to scene frames (if a scene re-times one of these, update FR(n)):
 *   s01 F_LAND 14, tail fwip 44..46   s02 LIFE slam 19, F_FLING 68
 *   s03 F_STOP 96                     s04 F_HATCH 36, F_EVIL 128..131
 *   s05 F_FAST 58, F_FINAL 100        s06 starer from f ~110
 *   s07 CUT_B 40..64, CUT_C 92, FWOOMP 110
 *   s08 F_SCRATCH 14, F_CRACK 28, F_ROAR 68, F_CAP1/2 98/104
 *   s10 F_SWAP 74, JUDGE 138 .. F_TP 195     s11 F_CLOSE 60, F_MROW 174, F_FLIP 186
 *   s12 F_1000 22, F_GOOD 46, F_BAD 68, F_PARK 90, F_POP 108
 *   s13 F_CRUMBLE 36, F_TILT 98       s14 F_YES 74, F_CLAP 112
 *   s15 F_IGNITE 30, F_OUT 110
 */
#include "tk.h"
#include "timeline.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FR(n) ((n) / (double)TK_FPS)

/* ======================================================================== */
/* mute / duck windows: the score is written continuously and then carved   */
/* ======================================================================== */
#define MAXW 32
static double mw_a[MAXW], mw_b[MAXW];
static int nmw;
static double dw_a[MAXW], dw_b[MAXW];
static float dw_g[MAXW];
static int ndw;
static float G = 1.0f; /* current section gain */
/* The score runs twice: pass 0 only collects the windows (so a note from
 * an earlier section still respects a later window), pass 1 plays. */
static int pass;

static void mute(double a, double b) {
    if (pass == 0 && nmw < MAXW) { mw_a[nmw] = a; mw_b[nmw] = b; nmw++; }
}
static void duck(double a, double b, float g) {
    if (pass == 0 && ndw < MAXW) { dw_a[ndw] = a; dw_b[ndw] = b; dw_g[ndw] = g; ndw++; }
}
/* Notes starting inside a mute window are dropped, notes running into one
 * are cut at its start. */
static int clip(double t, double *dur) {
    double b = t + *dur;
    for (int i = 0; i < nmw; i++) {
        if (t >= mw_a[i] - 0.002 && t < mw_b[i]) return 0;
        if (t < mw_a[i] && b > mw_a[i]) b = mw_a[i];
    }
    *dur = b - t;
    return *dur > 0.006;
}
static float gain(double t) {
    float g = G;
    for (int i = 0; i < ndw; i++) if (t >= dw_a[i] && t < dw_b[i]) g *= dw_g[i];
    return g;
}
static int muted(double t) {
    for (int i = 0; i < nmw; i++) if (t >= mw_a[i] - 0.03 && t < mw_b[i]) return 1;
    return 0;
}

/* ---- the voices everything goes through -------------------------------- */
/* Narrow pulse waves sit off-centre (SQ12 averages -0.75, SQ25 -0.5), so a
 * busy pulse score drifts the mix and thumps at every note edge. Each pulse
 * note gets a partner that cancels it: a 0.13 Hz SQ50 "note" starts on its
 * flat +1 half-cycle (3.9 s long) and shares the note's exact envelope. */
static void dc_cancel(double t, double d, int w, float v) {
    float dc = w == TKV_SQ12 ? 0.75f : w == TKV_SQ25 ? 0.5f : 0.0f;
    if (dc > 0) tk_note(t, d, -72, TKV_SQ50, v * dc);
}
static void N(double t, double d, int m, int w, float v) {
    if (pass == 0 || m < 0 || v <= 0 || !clip(t, &d)) return;
    tk_note(t, d, m, w, v * gain(t));
    dc_cancel(t, d, w, v * gain(t));
}
static void SL(double t, double d, int a, int b, int w, float v) {
    double d0 = d;
    if (pass == 0 || v <= 0 || !clip(t, &d)) return;
    if (d < d0 * 0.6) return; /* a slide cut short would race - drop it */
    tk_slide(t, d, a, b, w, v * gain(t));
    dc_cancel(t, d, w, v * gain(t));
}
static void KK(double t, float v) { if (pass && !muted(t)) tk_kick(t, v * gain(t)); }
static void SN(double t, float v) { if (pass && !muted(t)) tk_snare(t, v * gain(t)); }
static void HH(double t, float v) { if (pass && !muted(t)) tk_hat(t, v * gain(t)); }
/* raw voices that ignore the windows (tape stops, held breaths) */
static void RN(double t, double d, int m, int w, float v) {
    if (pass) { tk_note(t, d, m, w, v); dc_cancel(t, d, w, v); }
}
static void RS(double t, double d, int a, int b, int w, float v) {
    if (pass) { tk_slide(t, d, a, b, w, v); dc_cancel(t, d, w, v); }
}
/* crash: three noise layers of different lengths = a decaying wash */
static void CR(double t, float v) {
    N(t, 0.12, 118, TKV_NOISE, v * 0.55f);
    N(t, 0.45, 116, TKV_NOISE, v * 0.3f);
    N(t, 1.0, 113, TKV_NOISE, v * 0.18f);
}
static void OH(double t, float v) { N(t, 0.16, 120, TKV_NOISE, v * 0.5f); } /* open hat */
static void TOM(double t, int m, float v) {
    SL(t, 0.16, m + 7, m, TKV_TRI, v);
    SL(t, 0.08, m + 12, m + 2, TKV_SINE, v * 0.5f);
}

/* ---- envelopes from phase-aligned layers ------------------------------- */
/* The synth has a fixed 4 ms attack / 30 ms release. Stacking layers that
 * start on the same sample (or whole periods apart) keeps them in phase,
 * so they simply add: different lengths = a decay, staggered starts = a
 * swell. */
static double period(int m) { return 1.0 / (440.0 * pow(2.0, (m - 69) / 12.0)); }
/* decaying note: n layers, envelope falls from v to 0 over dur
 * (curve 1 = linear, >1 = pluckier) */
static void dn(double t, double dur, int m, int w, float v, int n, float curve) {
    for (int k = 0; k < n; k++) N(t, dur * pow((k + 1.0) / n, curve), m, w, v / n);
}
/* swelling note: n layers sharing an end, starts staggered over att */
static void sw(double t, double dur, double att, int m, int w, float v, int n) {
    double P = period(m);
    for (int k = 0; k < n; k++) {
        double off = floor(att * k / n / P + 0.5) * P;
        N(t + off, dur - off, m, w, v / n);
    }
}

/* ======================================================================== */
/* score notation                                                            */
/* ======================================================================== */
/* "E5:2 r:2 /G5! C6~"  note[:len16] ; r = rest ; len persists.
 *  /  scoop up from a whole tone below     !  accent
 *  .  staccato     ~  wobble (warped, wrong)     v  droop a semitone */
typedef struct { double t, d; int m; uint8_t acc, stac, wob, droop, scoop; } Ev;
#define MAXEV 96
static int parse(const char *s, double t0, double st, Ev *ev) {
    static const int PC[7] = { 9, 11, 0, 2, 4, 5, 7 };
    int n = 0, len = 2;
    double t = t0;
    while (*s && n < MAXEV) {
        if (*s == ' ') { s++; continue; }
        Ev e;
        memset(&e, 0, sizeof e);
        e.m = -1;
        if (*s == '/') { e.scoop = 1; s++; }
        if (*s == 'r') s++;
        else if (*s >= 'A' && *s <= 'G') {
            int p = PC[*s++ - 'A'];
            if (*s == '#') { p++; s++; } else if (*s == 'b') { p--; s++; }
            int oct = 4;
            if (*s >= '0' && *s <= '9') oct = *s++ - '0';
            e.m = 12 * (oct + 1) + p;
        }
        for (; *s && *s != ' '; s++) {
            if (*s == ':') { char *end; len = (int)strtol(s + 1, &end, 10); s = end - 1; }
            else if (*s == '!') e.acc = 1;
            else if (*s == '.') e.stac = 1;
            else if (*s == '~') e.wob = 1;
            else if (*s == 'v') e.droop = 1;
        }
        e.t = t;
        e.d = len * st;
        t += e.d;
        ev[n++] = e;
    }
    return n;
}
static int notes(const char *s, int *out) {
    Ev ev[MAXEV];
    int n = parse(s, 0, 1, ev);
    for (int i = 0; i < n; i++) out[i] = ev[i].m;
    return n;
}

/* one voice of a melody note, with the event's ornaments */
static void tone(double t, double d, int m, int w, float v, const Ev *e) {
    if (e->scoop && d > 0.07) { SL(t, 0.045, m - 2, m, w, v); t += 0.04; d -= 0.04; }
    if (e->wob) {
        double h = d * 0.3;
        N(t, h + 0.012, m, w, v);
        int k = 0;
        for (double u = t + h; u < t + d - 0.03; u += 0.07, k++) {
            double sd = fmin(0.082, t + d - u);
            if (k & 1) SL(u, sd, m - 1, m, w, v); else SL(u, sd, m, m - 1, w, v);
        }
        return;
    }
    if (e->droop) {
        double h = d * 0.35;
        N(t, h + 0.012, m, w, v);
        SL(t + h, d - h, m, m - 1, w, v * 0.9f);
        return;
    }
    N(t, d, m, w, v);
}

/* ---- instruments -------------------------------------------------------- */
enum { I_LEAD, I_LCD, I_BASS, I_SCREAM, I_GTR, I_BELL, I_TWIN, I_ANTHEM, I_TOY, I_TREM };
static float echo_v;           /* GBA-style echo on the leads */
static double echo_dt;
static float gtr_bass = 0.42f; /* triangle bass locked to the guitar */

/* palm-muted chug or open power chord (root, fifth, octave) */
static void gtr(const Ev *e, int m, float v) {
    int bm = m - 12;
    while (bm < 28) bm += 12;
    if (e->acc) {
        double d = e->d * 0.97;
        dn(e->t, d, m, TKV_SAW, v, 3, 1.0f);
        dn(e->t, d, m + 7, TKV_SQ50, v * 0.5f, 3, 1.0f);
        dn(e->t, d, m + 12, TKV_SAW, v * 0.38f, 3, 1.0f);
        N(e->t, 0.03, 96, TKV_NOISE, v * 0.4f);
        if (gtr_bass > 0) dn(e->t, d, bm, TKV_TRI, gtr_bass, 2, 1.0f);
    } else {
        double d = fmin(e->d * 0.8, 0.085);
        N(e->t, d, m, TKV_SAW, v * 0.85f);
        N(e->t, d, m + 7, TKV_SQ50, v * 0.38f);
        N(e->t, 0.02, 88, TKV_NOISE, v * 0.3f);
        if (gtr_bass > 0) N(e->t, fmin(e->d * 0.9, 0.11), bm, TKV_TRI, gtr_bass);
    }
}
/* tremolo-picked power chord: 32nd-note chugs across the whole event */
static void trem(const Ev *e, int m, float v) {
    double st = 0.0625;
    for (double u = 0; u < e->d - 0.01; u += st) {
        N(e->t + u, 0.05, m, TKV_SAW, v);
        N(e->t + u, 0.05, m + 7, TKV_SQ50, v * 0.45f);
        N(e->t + u, 0.05, m + 12, TKV_SAW, v * 0.3f);
    }
    int bm = m - 12;
    while (bm < 28) bm += 12;
    N(e->t, e->d * 0.95, bm, TKV_TRI, gtr_bass);
}

static void inst(int ins, const Ev *e, float v, int tr) {
    if (e->m < 0) return;
    int m = e->m + tr;
    double d = e->d;
    float a = e->acc ? 1.3f : 1.0f;
    double art = e->stac ? 0.5 : (d < 0.2 ? 0.8 : 0.92);
    double sd = d * art;
    switch (ins) {
    case I_LEAD:   /* the GBA pulse lead (+ echo); long notes warble into an
                    * octave arp on their tail, tracker style */
        if (sd >= 0.36 && !e->wob && !e->droop && !e->scoop) {
            double h = sd * 0.55, st = 1.0 / 32;
            N(e->t, h + 0.012, m, TKV_SQ25, v * a);
            int k = 0;
            for (double u = e->t + h; u < e->t + sd - 0.01; u += st, k++)
                N(u, fmin(st, e->t + sd - u) + 0.012, (k & 1) ? m : m + 12, TKV_SQ25, v * a * ((k & 1) ? 1.0f : 0.8f));
        } else
            tone(e->t, sd, m, TKV_SQ25, v * a, e);
        if (echo_v > 0) tone(e->t + echo_dt, sd, m, TKV_SQ12, echo_v * a, e);
        break;
    case I_LCD:    /* virtual-pet buzzer: square + a faint sine an octave up */
        tone(e->t, sd * 0.85, m, TKV_SQ50, v * a, e);
        N(e->t, sd * 0.5, m + 12, TKV_SINE, v * 0.35f * a);
        break;
    case I_BASS:
        tone(e->t, d * (e->stac ? 0.45 : 0.82), m, TKV_TRI, v * a, e);
        break;
    case I_SCREAM: /* screaming lead: thin pulse + a fatter pulse an octave down */
        tone(e->t, d * 0.96, m, TKV_SQ12, v * a, e);
        tone(e->t, d * 0.96, m - 12, TKV_SQ25, v * 0.32f * a, e);
        break;
    case I_GTR:  gtr(e, m, v * a); break;
    case I_TREM: trem(e, m, v); break;
    case I_BELL: dn(e->t, fmax(d, 0.45), m, TKV_SINE, v * a, 4, 1.7f); break;
    case I_TWIN: tone(e->t, sd, m, TKV_SQ12, v * a, e); break;
    case I_ANTHEM: /* lead + saw an octave down, like a guitar doubling it */
        tone(e->t, sd, m, TKV_SQ25, v * a, e);
        tone(e->t, sd, m - 12, TKV_SAW, v * 0.42f * a, e);
        if (echo_v > 0) tone(e->t + echo_dt, sd, m, TKV_SQ12, echo_v * a, e);
        break;
    case I_TOY:    /* kawaii plinks: staccato pulse + music-box sine */
        N(e->t, fmin(d * 0.6, 0.09), m, TKV_SQ25, v * a);
        dn(e->t, fmax(d, 0.18), m + 12, TKV_SINE, v * 0.55f * a, 3, 1.6f);
        break;
    }
}
static double play(const char *s, double t0, double st, int ins, float v, int tr) {
    Ev ev[MAXEV];
    int n = parse(s, t0, st, ev);
    for (int i = 0; i < n; i++) inst(ins, &ev[i], v, tr);
    return n ? ev[n - 1].t + ev[n - 1].d : t0;
}

/* ---- drums: one lane per piece, '1'..'9' = velocity, '.' = rest -------- */
static void lane(const char *p, double t0, double st, int steps, int kind, float g) {
    int L = (int)strlen(p);
    for (int i = 0; i < steps; i++) {
        char c = p[i % L];
        if (c < '1' || c > '9') continue;
        float v = (c - '0') * 0.1f * g;
        double t = t0 + i * st;
        switch (kind) {
        case 'k': KK(t, v); break;
        case 's': SN(t, v); break;
        case 'h': HH(t, v); break;
        case 'o': OH(t, v); break;
        case 'c': CR(t, v); break;
        }
    }
}
static void drums(double t0, double st, int steps, const char *k, const char *s, const char *h, float g) {
    if (k) lane(k, t0, st, steps, 'k', g);
    if (s) lane(s, t0, st, steps, 's', g);
    if (h) lane(h, t0, st, steps, 'h', g);
}

/* ---- harmony ------------------------------------------------------------- */
/* tracker-style arpeggio stab: cycles the chord every 1/40 s */
static void stab(double t, double len, const int *ch, int n, int w, float v) {
    const double step = 1.0 / 40;
    for (int i = 0; i * step < len - 0.006; i++)
        N(t + i * step, fmin(step, len - i * step) + 0.012, ch[i % n], w, v);
}
/* boom-chick backing for one chord: bass root/fifth on the beat, octave
 * "pah" + an arp stab on the off-beat */
static void comp(double t, double B, int beats, const char *chord, int root, float g) {
    int ch[8], n = notes(chord, ch);
    for (int b = 0; b < beats; b++) {
        double u = t + b * B;
        N(u, B * 0.42, (b & 1) ? root - 5 : root, TKV_TRI, 0.52f * g);
        N(u + B * 0.5, B * 0.36, root + 12, TKV_TRI, 0.4f * g);
        stab(u + B * 0.5, B * 0.32, ch, n, TKV_SQ12, 0.12f * g);
    }
}
/* rolling 16th arpeggio (busier sections) */
static void roll(double t, double S, int steps, const char *chord, int w, float v) {
    int ch[8], n = notes(chord, ch);
    static const int ORD[4] = { 0, 1, 2, 1 };
    for (int i = 0; i < steps; i++) {
        int k = n >= 3 ? ORD[i & 3] : i % n;
        if (n > 3 && (i & 7) == 3) k = 3;
        N(t + i * S, S * 0.7, ch[k], w, v);
    }
}
/* sustained pad: triad on triangles + top note on a sine, swelled in */
static void choir(double t, double dur, const char *chord, float v) {
    int ch[8], n = notes(chord, ch);
    for (int i = 0; i < n; i++)
        sw(t, dur, fmin(0.25, dur * 0.4), ch[i], i == n - 1 && n > 3 ? TKV_SINE : TKV_TRI, v, 4);
}
/* a power chord with a long ring (for hits) */
static void hit(double t, double dur, int root, float v) {
    Ev e;
    memset(&e, 0, sizeof e);
    e.t = t; e.d = dur; e.m = root; e.acc = 1;
    gtr(&e, root, v);
}
static void bells(double t, double step, const char *s, float v) {
    int m[16], n = notes(s, m);
    for (int i = 0; i < n; i++) dn(t + i * step, 0.5, m[i], TKV_SINE, v, 3, 1.6f);
}

/* ======================================================================== */
/* the cute theme (C major, 140 bpm)                                         */
/* ======================================================================== */
#define B1 (60.0 / 140.0)
#define S1 (B1 / 4.0)
/* the hook, phrase by phrase */
#define HOOK1 "E5:2 E5:2 G5:2 E5:2 C6:4 G5:4"   /* C  */
#define HOOK2 "A5:2 A5:2 G5:2 E5:2 D5:4 E5:4"   /* Am */
#define HOOK3 "F5:2 F5:2 A5:2 F5:2 C6:4 A5:2 B5:2" /* F -> G */
#define HOOK7 "F5:2 A5:2 C6:2 A5:2 G5:2 F5:2 E5:2 D5:2" /* F -> G */

static const char *K_CUTE = "6.......5.......";
static const char *S_CUTE = "....4.......4...";
static const char *H_CUTE = "2.3.2.3.2.3.2.3.";

static void s01(void) {
    double t = tk_scene_start("s01_splash");
    double land = t + FR(14), cut = t + FR(46);
    mute(cut, tk_scene_start("s02_title"));
    G = 1.0f;
    /* bass bounces on the landing: C2 - G2 - C3 */
    N(land, B1 * 0.9, 36, TKV_TRI, 0.5f);
    N(land + B1, B1 * 0.85, 43, TKV_TRI, 0.42f);
    N(land + 2 * B1, B1 * 2, 48, TKV_TRI, 0.42f);
    /* C augmented shimmer (it agrees with the scene's whole-tone sparkle
     * run) resolving to C major on beat 3 */
    sw(land, 2 * B1 + 0.01, 0.15, 60, TKV_TRI, 0.16f, 3);
    sw(land, 2 * B1 + 0.01, 0.15, 64, TKV_TRI, 0.15f, 3);
    sw(land, 2 * B1 + 0.01, 0.15, 68, TKV_TRI, 0.14f, 3);
    N(land + 2 * B1, 1.0, 60, TKV_TRI, 0.16f);
    N(land + 2 * B1, 1.0, 64, TKV_TRI, 0.15f);
    N(land + 2 * B1, 1.0, 67, TKV_TRI, 0.15f);
    N(land + 2 * B1, 1.0, 72, TKV_SINE, 0.12f);
    /* music box: a ding on the landing, then the resolution arpeggio - which
     * gets frozen mid-air by the cut */
    dn(land, 0.7, 84, TKV_SINE, 0.24f, 4, 1.6f);
    dn(land + B1, 0.4, 79, TKV_SINE, 0.12f, 3, 1.6f);
    bells(land + 2 * B1, S1 / 2, "C6 E6 G6 C7 E7", 0.2f);
}

static void s02(void) {
    double t = tk_scene_start("s02_title");
    double D = t + FR(19);              /* "LIFE" slams down: downbeat */
    double fling = t + FR(68);
    double t03 = tk_scene_start("s03_hahaland");
    double bar0 = t03 + 2 * B1 - 4 * B1; /* the vamp bar, on s03's grid */
    mute(fling, bar0);
    G = 1.0f;
    /* pickup: snare crescendo + a bass glide while the letters drop */
    for (int k = 5; k >= 1; k--) SN(D - k * S1, 0.38f - 0.05f * (k - 1));
    SL(t + 0.1, D - t - 0.1, 36, 48, TKV_TRI, 0.32f);
    /* bar 1: the hook ... */
    CR(D, 0.34f);
    drums(D, S1, 12, K_CUTE, S_CUTE, H_CUTE, 1.0f);
    lane("2.1.", D + 12 * S1, S1, 4, 'h', 1.0f);
    echo_v = 0.08f; echo_dt = 3 * S1;
    play("E5:2 E5:2 G5:2 E5:2", D, S1, I_LEAD, 0.3f, 0);
    /* ... until HELL drips: the high C sags C6 -> B5 -> Bb5 */
    double c6 = D + 8 * S1;
    N(c6, 0.11, 84, TKV_SQ25, 0.3f);
    SL(c6 + 0.1, 0.17, 84, 83, TKV_SQ25, 0.28f);
    SL(c6 + 0.26, 0.17, 83, 82, TKV_SQ25, 0.24f);
    N(c6 + 3 * S1, 0.1, 84, TKV_SQ12, 0.07f); /* the echo, sagging too */
    SL(c6 + 3 * S1 + 0.1, 0.17, 84, 82, TKV_SQ12, 0.06f);
    /* harmony sours: C | C | Cm | (a nervous Ab whisper - the scene's
     * "uh-oh" and jitter carry beat 4) */
    play("C3:2 C4:2 G2:2 C4:2 C3:2", D, S1, I_BASS, 0.5f, 0); /* (rest: the drip) */
    int cmaj[3] = { 72, 76, 79 }, cmin[3] = { 72, 75, 79 }, ab[3] = { 68, 72, 75 };
    for (int b = 0; b < 3; b++)
        stab(D + b * B1 + B1 / 2, B1 * 0.32, b < 2 ? cmaj : cmin, 3, TKV_SQ12, 0.12f);
    stab(D + 3 * B1, B1 * 0.2, ab, 3, TKV_SQ12, 0.06f);
    stab(D + 3.5 * B1, B1 * 0.2, ab, 3, TKV_SQ12, 0.05f);
    /* (the fling and the SPLAT play in silence) */

    /* the vamp sneaks back in after the sticker: F (under the innocent
     * whistle), then C as s03 cuts in with its fanfare */
    double v0 = bar0;
    comp(v0, B1, 2, "A4 C5 F5", 41, 0.85f);
    drums(v0, S1, 8, NULL, NULL, "1.2.1.2.", 1.0f);
}

static void s03(void) {
    double t = tk_scene_start("s03_hahaland");
    double bar1 = t + 2 * B1, bar2 = bar1 + 4 * B1;
    double stop = t + FR(96);
    mute(stop, tk_scene_start("s04_doomogachi") + 0.34);
    G = 0.92f;
    /* last half of the vamp bar: C under the scene's fanfare */
    CR(t, 0.3f);
    comp(t, B1, 2, "C5 E5 G5", 48, 1.0f);
    drums(t, S1, 8, "5.......", "....3.", H_CUTE, 1.0f);
    SN(t + 6 * S1, 0.25f);
    SN(t + 7 * S1, 0.32f);
    /* "Welcome to HaHaland!" */
    echo_v = 0.08f; echo_dt = 3 * S1;
    play(HOOK1, bar1, S1, I_LEAD, 0.31f, 0);
    play(HOOK2, bar2, S1, I_LEAD, 0.31f, 0);
    play("C6:2 C6:2 E6:2 C6:2 G6:4 E6:4 E6:2 E6:2 E6:2 C6:2 B5:4 C6:4", bar1, S1, I_TWIN, 0.07f, 0);
    comp(bar1, B1, 4, "C5 E5 G5", 48, 1.0f);
    comp(bar2, B1, 4, "A4 C5 E5", 45, 1.0f);
    drums(bar1, S1, 32, K_CUTE, S_CUTE, "2.3.2.3.2.3.2.32", 1.0f);
    CR(bar1, 0.25f);
    /* the record scratch: everything droops like a stopped turntable */
    RS(stop, 0.2, 76, 62, TKV_SQ25, 0.2f);
    RS(stop, 0.22, 45, 33, TKV_TRI, 0.35f);
}

static void s04(void) {
    double t = tk_scene_start("s04_doomogachi");
    double bar2 = t + FR(36);             /* the hatch is a downbeat */
    double bar1 = bar2 - 4 * B1, bar3 = bar2 + 4 * B1, bar4 = bar3 + 4 * B1;
    double evil = t + FR(128), back = t + FR(132);
    mute(evil, back);
    G = 1.0f;
    /* intro (beats 3-4): a ticking dominant pedal while the egg wobbles */
    double i0 = bar1 + 2 * B1;
    play("G2:2 G2 G2 G2", i0, S1, I_BASS, 0.36f, 0);
    drums(i0, S1, 8, NULL, NULL, "1.2.1.2.", 1.0f);
    SN(bar2 - 2 * S1, 0.16f);
    SN(bar2 - S1, 0.24f);
    /* bar 2: HATCH. The pet song on an LCD buzzer (rest on beat 1 - the
     * "beep." lives there) */
    CR(bar2, 0.28f);
    play("r:4 G5:2 C6:2 E6:2 C6:2 r:4", bar2, S1, I_LCD, 0.24f, 0); /* beat 4: the hearts answer */
    comp(bar2, B1, 2, "C5 E5 G5", 48, 0.95f);
    comp(bar2 + 2 * B1, B1, 2, "A4 C5 E5", 45, 0.95f);
    drums(bar2, S1, 16, K_CUTE, S_CUTE, H_CUTE, 0.9f);
    /* bar 3: beat 1 = it strains (snare roll over a held G) ... */
    N(bar3, B1 * 0.95, 43, TKV_TRI, 0.45f);
    for (int k = 0; k < 4; k++) SN(bar3 + k * S1, 0.1f + 0.06f * k);
    /* ... beat 2 = the poop. TA-DA! */
    double poop = bar3 + B1;   /* (no bass on the downbeat: that's the pfrrt's) */
    CR(poop, 0.3f);
    bells(poop, 0.035, "C6 E6 G6 C7", 0.16f);
    play("r:4 C6:2! A5:2 G5:2 E5:2 D5:2 G5:2", bar3, S1, I_LCD, 0.24f, 0);
    int f[3] = { 69, 72, 77 };
    stab(poop + B1 / 2, B1 * 0.32, f, 3, TKV_SQ12, 0.12f);
    comp(poop + B1, B1, 1, "A4 C5 F5", 41, 0.95f);
    comp(poop + 2 * B1, B1, 1, "B4 D5 G5", 43, 0.95f);
    drums(poop, S1, 12, "....6...6...", "........4...", "3.2.3.2.3.2.", 0.9f);
    /* four frames of hell (muted) ... and back, a little bit wrong */
    play("F#5:2~", bar4 - 2 * S1, S1, I_LCD, 0.22f, 0);
    N(bar4 - 2 * S1, S1 * 1.8, 44, TKV_TRI, 0.45f);
    /* "...it's fine." - the pet song carries on in C minor, wobbling */
    play("Eb5:2 D5:2 C5:4~", bar4, S1, I_LCD, 0.24f, 0);
    play("C3:2 Eb3:2 Ab2:2 G2:2", bar4, S1, I_BASS, 0.48f, 0);
    int cm[3] = { 72, 75, 79 }, ab[3] = { 68, 72, 75 };
    stab(bar4 + B1 / 2, B1 * 0.32, cm, 3, TKV_SQ12, 0.11f);
    stab(bar4 + B1 * 1.5, B1 * 0.32, ab, 3, TKV_SQ12, 0.11f);
    drums(bar4, S1, 8, "5.......", "....3...", "2.2.2.2.", 1.0f);
}

static void s05(void) {
    double t = tk_scene_start("s05_creator");
    double bar1 = t, bar2 = t + 4 * B1;
    double stop = t + FR(100);                         /* lands on Kevin */
    double t06 = tk_scene_start("s06_life");
    double pick = t06 - 2 * B1;
    mute(stop, pick);
    G = 0.85f;
    /* royal road: F G | Em Am - a bouncy dotted tune, under the blips */
    CR(bar1, 0.22f);
    echo_v = 0.06f; echo_dt = 3 * S1;
    play("A5:3 A5:1 C6:2 A5:2 B5:3 B5:1 D6:2 B5:2 "
         "E6:3 D6:1 B5:2 G5:2 A5:3 B5:1 C6:2 E6:2 D6:4", bar1, S1, I_LEAD, 0.25f, 0);
    comp(bar1, B1, 2, "A4 C5 F5", 41, 0.9f);
    comp(bar1 + 2 * B1, B1, 2, "B4 D5 G5", 43, 0.9f);
    comp(bar2, B1, 2, "G4 B4 E5", 40, 0.9f);
    comp(bar2 + 2 * B1, B1, 2, "A4 C5 E5", 45, 0.9f);
    comp(bar2 + 4 * B1, B1, 2, "A4 D5 F5", 38, 0.9f);
    drums(bar1, S1, 16, K_CUTE, S_CUTE, H_CUTE, 0.9f);
    /* the cycling speeds up: hats and arps go double-time */
    double fast = t + FR(58);
    drums(bar2, S1, 24, K_CUTE, S_CUTE, "2121312121213121", 0.9f);
    roll(fast, S1 / 2, (int)((stop - fast) / (S1 / 2)), "E5 G5 B5 E6", TKV_SQ12, 0.07f);
    /* pickup back in for s06: G7 + snare fill */
    comp(pick, B1, 2, "G4 B4 F5", 43, 0.8f);
    drums(pick, S1, 8, NULL, "......45", "2.2.2.2.", 1.0f);
}

static void s06(void) {
    double t = tk_scene_start("s06_life");
    double bar1 = t, bar2 = t + 4 * B1, button = t + 8 * B1;
    mute(button + 0.2, tk_scene_start("s07_vape"));
    G = 0.85f;
    CR(bar1, 0.26f);
    echo_v = 0.08f; echo_dt = 3 * S1;
    play(HOOK1, bar1, S1, I_LEAD, 0.3f, 0);                 /* "Make friends!" */
    play(HOOK3, bar2, S1, I_LEAD, 0.3f, 0);                 /* "Fall in love!" */
    play(HOOK3, bar2, S1, I_BELL, 0.07f, 12);               /* sparkly double */
    comp(bar1, B1, 4, "C5 E5 G5", 48, 1.0f);
    comp(bar2, B1, 3, "A4 C5 F5", 41, 1.0f);
    comp(bar2 + 3 * B1, B1, 1, "B4 D5 G5", 43, 1.0f);
    drums(bar1, S1, 32, K_CUTE, S_CUTE, H_CUTE, 0.9f);
    SN(button - S1, 0.3f);
    /* the button: one bright C ... and then nothing (the guy in the dark) */
    CR(button, 0.3f);
    KK(button, 0.5f);
    N(button, 0.16, 84, TKV_SQ25, 0.3f);
    N(button, 0.16, 48, TKV_TRI, 0.5f);
    int c[3] = { 72, 76, 79 };
    stab(button, 0.16, c, 3, TKV_SQ12, 0.14f);
    bells(button + 0.03, 0.04, "G6 C7", 0.12f);
}

static void s07(void) {
    double t = tk_scene_start("s07_vape");
    double bar1 = t, bar2 = t + 4 * B1;
    double inhale = t + FR(92), fwoomp = t + FR(110);
    mute(inhale, tk_scene_start("s08_invasion"));
    duck(t + FR(40), t + FR(64), 0.65f); /* Vex talks */
    G = 0.9f;
    /* GRAND OPENING! ...the hook, but its last note slips to a tritone */
    CR(bar1, 0.24f);
    echo_v = 0.08f; echo_dt = 3 * S1;
    play("E5:2 E5:2 G5:2 E5:2 C6:4 G5:2 Gb5:2v", bar1, S1, I_LEAD, 0.3f, 0);
    comp(bar1, B1, 2, "C5 E5 G5", 48, 1.0f);
    comp(bar1 + 2 * B1, B1, 2, "Ab4 C5 Eb5", 44, 1.0f);
    drums(bar1, S1, 16, K_CUTE, S_CUTE, H_CUTE, 0.9f);
    /* the undertone: a low Db saw creeping up under the C */
    sw(bar1, 4 * B1 + 1.2, 1.6, 37, TKV_SAW, 0.2f, 5);
    sw(bar1 + 2 * B1, 4 * B1, 1.0, 25, TKV_TRI, 0.3f, 3);
    /* Vex: a C minor lament (C B Bb A bass: Cm G/B Eb/Bb F/A), a sly
     * chromatic line, swung hats */
    play("C3:4 B2:4 Bb2:4 A2:2 Ab2:2", bar2, S1, I_BASS, 0.5f, 0);
    play("r:2 Eb5:1 G5:1 F#5:3 G5:1 Ab5:2 G5:1 Eb5:1 C5:2 B4:2", bar2, S1, I_TWIN, 0.2f, 0);
    play("r:2 Eb4:1 G4:1 F#4:3 G4:1 Ab4:2 G4:1 Eb4:1 C4:2 B3:2", bar2, S1, I_TWIN, 0.1f, 0);
    int cm[3] = { 72, 75, 79 }, gb[3] = { 71, 74, 79 }, eb[3] = { 70, 75, 79 }, fa[3] = { 69, 72, 77 };
    const int *seq[4] = { cm, gb, eb, fa };
    for (int b = 0; b < 4; b++) stab(bar2 + b * B1 + B1 / 2, B1 * 0.3, seq[b], 3, TKV_SQ12, 0.1f);
    drums(bar2, S1, 16, "6.......5.......", "....4.......4...", "2..32..32..32..3", 0.85f);
    /* the inhale: the band stops, a diminished swell holds its breath */
    for (int i = 0; i < 4; i++) {
        static const int DIM[4] = { 48, 60, 63, 66 };
        double P = period(DIM[i]);
        for (int k = 0; k < 4; k++) {
            double off = floor(0.45 * k / 4 / P + 0.5) * P;
            RN(inhale + off, fwoomp - inhale - off, DIM[i], i ? TKV_TRI : TKV_SAW, (i ? 0.13f : 0.1f) / 4);
        }
    }
    RS(inhale, fwoomp - inhale, 79, 83, TKV_SQ12, 0.05f);
}

static void s08_party(void) {
    double t = tk_scene_start("s08_invasion");
    double scratch = t + FR(14);
    mute(scratch, t + FR(28));
    G = 1.05f;
    /* the party: the hook at full tilt ... */
    CR(t, 0.32f);
    echo_v = 0.0f;
    play("E5:2 E5:2 G5:2", t, S1, I_LEAD, 0.32f, 0);
    play("E5:2 E5:2 G5:2", t, S1, I_BELL, 0.08f, 12);
    comp(t, B1, 2, "C5 E5 G5", 48, 1.0f);
    drums(t, S1, 8, "7...4...", "....5...", "3.3.3.3.", 1.0f);
    /* ... RECORD SCRATCH: tape-stop droop */
    RS(scratch, 0.16, 79, 64, TKV_SQ25, 0.22f);
    RS(scratch, 0.2, 48, 36, TKV_TRI, 0.35f);
}

/* ======================================================================== */
/* DOOM (E minor, 120 bpm: a beat = 0.5 s, so every cut lands on the grid)   */
/* ======================================================================== */
#define S2 0.125
/* the main riff: pedal E chugs, chromatic accents */
#define RIFF_A1 "E2:1 E2 G2!:2 E2:1 E2 A#2!:2 E2:1 E2 A2!:2 E2:1 E2 G2! F#2!"
#define RIFF_A2 "E2:1 E2 G2!:2 E2:1 E2 A#2!:2 E2:1 E2 C3!:2 B2!:2 A#2!:1 A2!"
static const char *K_DBL = "5343434353434343";   /* double kick */
static const char *S_BB  = "....7.......7...";   /* backbeat */
static const char *H_8   = "3.2.3.2.3.2.3.2.";

static void s08_doom(void) {
    double t = tk_scene_start("s08_invasion");
    double crack = t + FR(28), roar = t + 2.25, r0 = t + 2.5;
    G = 1.0f;
    gtr_bass = 0.42f;
    /* the sky cracks: BWAAM */
    hit(crack, 1.3, 40, 0.42f);
    dn(crack, 1.5, 28, TKV_TRI, 0.5f, 3, 1.0f);
    KK(crack, 0.65f);
    N(crack, 0.9, 100, TKV_NOISE, 0.22f);
    N(crack, 0.25, 108, TKV_NOISE, 0.2f);
    /* build: tremolo chugs climbing E F F# G + a snare roll into the roar */
    double b0 = t + 1.5;
    static const int CL[4] = { 40, 41, 42, 43 };
    static const double CT[5] = { 0, 0.25, 0.5, 0.625, 0.75 };
    for (int s = 0; s < 4; s++)
        for (double u = b0 + CT[s]; u < b0 + CT[s + 1] - 0.01; u += 0.0625) {
            float v = 0.12f + 0.28f * (float)((u - b0) / 0.75);
            N(u, 0.05, CL[s], TKV_SAW, v);
            N(u, 0.05, CL[s] + 7, TKV_SQ50, v * 0.45f);
        }
    sw(b0, 0.75, 0.6, 28, TKV_TRI, 0.45f, 4);
    for (int k = 0; k < 4; k++) SN(b0 + k * S2, 0.15f + 0.04f * k);
    for (int k = 0; k < 4; k++) SN(b0 + 0.5 + k * S2 / 2, 0.32f + 0.05f * k);
    RS(b0, 0.75, 52, 64, TKV_SQ12, 0.08f);
    /* THE ROAR: hit + a squeal */
    CR(roar, 0.42f);
    KK(roar, 0.7f);
    hit(roar, 0.24, 40, 0.42f);
    SL(roar, 0.12, 83, 88, TKV_SQ12, 0.25f);
    N(roar + 0.11, 0.14, 88, TKV_SQ12, 0.25f);
    /* the riff: "THE DEMONS OF HELL" (35.75) "HAVE INVADED" (36.0) slam on
     * the accents of bar 1 */
    play("E2:1 E2 G2!:2 E2:1 E2 A#2!:2 E2!:1 E2 A2!:2 E2:1 E2 G2! F#2!", r0, S2, I_GTR, 0.34f, 0);
    play(RIFF_A2, r0 + 2.0, S2, I_GTR, 0.34f, 0);
    double end = tk_scene_start("s09_weld");
    int steps = (int)((end - r0) / S2 + 0.5);
    drums(r0, S2, steps, K_DBL, S_BB, H_8, 1.0f);
    CR(r0, 0.4f);
    CR(r0 + 0.75, 0.38f);
    CR(r0 + 1.0, 0.4f);
    KK(r0 + 0.75, 0.6f);
    CR(r0 + 2.0, 0.35f);
    /* "(wherever you are)": the lead creeps in, chromatic menace */
    play("B5:4 C6:2 B5:2 A#5:4 B5:2~", r0 + 2.0, S2, I_SCREAM, 0.2f, 0);
}

static void s09(void) {
    double t = tk_scene_start("s09_weld");
    G = 0.9f;
    gtr_bass = 0.4f;
    /* the gallop */
    play("E2!:2 E2:1 E2 E2!:2 E2:1 E2 G2!:2 G2:1 G2 F#2!:2 F#2:1 F#2", t, S2, I_GTR, 0.32f, 0);
    play("E2!:2 E2:1 E2 E2!:2 E2:1 E2 A#2!:2 A#2:1 A#2 B2!:2 B2:1 B2", t + 2.0, S2, I_GTR, 0.32f, 0);
    drums(t, S2, 32, "6.4.6.4.6.4.6.4.", S_BB, "3.2.3.2.3.2.3.2.", 1.0f);
    lane("5.55", t, S2, 32, 'k', 0.55f);
    CR(t, 0.4f);
    CR(t + 2.0, 0.32f);
    /* twin-guitar harmony in bar 2 */
    play("B5:3 C6:1 B5:2 A5:2 G5:2 A5:2 F#5:4", t + 2.0, S2, I_SCREAM, 0.18f, 0);
    play("G5:3 A5:1 G5:2 F#5:2 E5:2 F#5:2 D#5:4", t + 2.0, S2, I_TWIN, 0.12f, 0);
    /* fill: snare + toms -> (CUTE MODE) */
    double f = t + 4.0;
    SN(f, 0.4f); TOM(f + S2, 52, 0.45f); TOM(f + 2 * S2, 47, 0.5f); TOM(f + 3 * S2, 40, 0.55f);
    KK(f, 0.5f);
    hit(f, 0.45, 40, 0.3f);
}

static void s10(void) {
    double t = tk_scene_start("s10_shooter");
    double brutal = t + 2.5, judge = t + 4.5, tp = t + 6.5;
    mute(judge, tp);
    G = 1.0f;
    /* CUTE MODE: the riff, kawaii (C major, toy pulse + music box, claps) */
    play("C5:1 C5 E5!:2 C5:1 C5 G5!:2 C5:1 C5 F5!:2 C5:1 C5 E5! D5! C5:1 C5 E5:2",
         t, S2, I_TOY, 0.24f, 0);
    play("C3:2 C4:2 C3:2 C4:2 F2:2 F3:2 G2:2 G3:2 C3:2 C4:2", t, S2, I_BASS, 0.45f, 0);
    int cmaj[3] = { 72, 76, 79 };
    for (int b = 0; b < 5; b++) stab(t + b * 0.5 + 0.25, 0.14, cmaj, 3, TKV_SQ12, 0.09f);
    drums(t, S2, 20, "5.......4.......", "....3.......3...", "2.2.2.2.2.2.2.2.", 1.0f);
    bells(t, 0.05, "C7 G6", 0.12f);
    /* swap: the cute riff winds down like a dying toy */
    RS(brutal - 0.05, 0.12, 76, 60, TKV_SQ25, 0.12f);
    /* BRUTAL MODE: the real thing */
    gtr_bass = 0.45f;
    CR(brutal, 0.45f);
    KK(brutal, 0.7f);
    play(RIFF_A1, brutal, S2, I_GTR, 0.36f, 0);
    drums(brutal, S2, 16, "5444544454445444", S_BB, H_8, 1.0f);
    CR(brutal + 1.0, 0.3f);
    /* (We don't judge.) - silence - (We judge a little.) */
    /* third person: riff + screaming lead + thrash beat */
    CR(tp, 0.45f);
    play(RIFF_A2, tp, S2, I_GTR, 0.32f, 0);
    drums(tp, S2, 16, "5444544454445444", "....6...6...6.66", "3.3.3.3.3.3.3.3.", 1.0f);
    CR(tp + 1.0, 0.3f);
    play("B5:1 C6 B5 A5 G5 A5 B5 C6 D#6 E6 F#6 G6 E6:4~", tp, S2, I_SCREAM, 0.26f, 0);
    SL(tp + 1.5 + 0.38, 0.12, 88, 76, TKV_SQ12, 0.2f);   /* dive into s11 */
}

static void s11(void) {
    double t = tk_scene_start("s11_exorcism");
    double close = t + FR(60), sneak = t + FR(174), flip = t + FR(186);
    double t12 = tk_scene_start("s12_census");
    mute(close, sneak);
    G = 1.0f;
    gtr_bass = 0.4f;
    /* "THE POWER OF CUDDLES COMPELS YOU!" - half-time E . C . D + choir */
    CR(t, 0.42f);
    play("E2!:4 E2:1 E2 E2!:2 C3!:4 D3!:4", t, S2, I_GTR, 0.32f, 0);
    drums(t, S2, 16, "5434543454345434", "........7.......", "3.3.3.3.3.3.3.3.", 0.95f);
    CR(t + 1.0, 0.3f);
    choir(t, 1.0, "E4 G#4 B4 E5", 0.16f);
    choir(t + 1.0, 0.5, "E4 G4 C5 C6", 0.16f);
    choir(t + 1.5, 0.5, "F#4 A4 D5 D6", 0.16f);
    play("E5:8 G5:4 F#5:4", t, S2, I_TWIN, 0.1f, 0);
    /* (close-up: silence. the fails play alone) */

    /* the cursor sneaks down to MONSTER MODE: roll + a low swell */
    for (double u = sneak + 0.03; u < flip - 0.01; u += 0.0625)
        SN(u, 0.08f + 0.32f * (float)((u - sneak) / (flip - sneak)));
    sw(sneak, flip - sneak, 0.35, 28, TKV_TRI, 0.4f, 4);
    /* MONSTER MODE: blast beat, tremolo chugs, the evil choir */
    double st = (t12 - flip) / 10.0;
    gtr_bass = 0.45f;
    CR(flip, 0.5f);
    KK(flip, 0.7f);
    play("E2:4 F2:2 E2:2 A#2:2", flip, st, I_TREM, 0.3f, 0);
    lane("5", flip, st, 10, 'k', 0.8f);
    lane("6.5.6.5.6.", flip, st, 10, 's', 1.0f);
    lane("4...4...4.", flip, st, 10, 'c', 1.0f);
    choir(flip, t12 - flip, "E4 G4 B4 A#5", 0.15f);
    SL(flip, 0.14, 88, 100, TKV_SQ12, 0.2f);
    SL(flip + 0.14, 0.7, 100, 76, TKV_SQ12, 0.2f);
    SL(flip + 0.14, 0.7, 88, 64, TKV_SQ25, 0.08f);
}

/* ======================================================================== */
/* census, anthem, cute reprise, logo                                        */
/* ======================================================================== */
/* census: a hit on every caption, IV - bVI - bVII - I (F Ab Bb C) after a
 * big C, metal drums under cute bells - and a "ta-da" for the cork */
static void s12(void) {
    double t = tk_scene_start("s12_census");
    double c1k = t + FR(22), good = t + FR(46), bad = t + FR(68);
    double park = t + FR(90), pop = t + FR(108), t13 = tk_scene_start("s13_ending");
    G = 1.0f;
    gtr_bass = 0.42f;
    /* the pull-back: a G tremolo + snare roll winding up to the count */
    for (double u = t; u < c1k - 0.03; u += 0.0625) {
        float v = 0.08f + 0.22f * (float)((u - t) / (c1k - t));
        N(u, 0.05, 43, TKV_SAW, v);
        N(u, 0.05, 50, TKV_SQ50, v * 0.45f);
    }
    sw(t, c1k - t, 0.5, 31, TKV_TRI, 0.42f, 4);
    for (double u = t + 0.25; u < c1k - 0.03; u += 0.0625)
        SN(u, 0.06f + 0.3f * (float)((u - t) / (c1k - t)));
    lane("4.3.4.3.", t, S2, 6, 'k', 1.0f);
    /* 1000 CITIZENS. - C, the whole band (the scene's fanfare rides on it) */
    double st = (good - c1k) / 6;
    CR(c1k, 0.45f);
    KK(c1k, 0.7f);
    hit(c1k, good - c1k, 48, 0.32f);
    choir(c1k, good - c1k, "C4 E4 G4 C5", 0.11f);
    lane("5343434", c1k, st, 6, 'k', 1.0f);
    lane("...6..", c1k, st, 6, 's', 1.0f);
    lane("3.3.3.", c1k, st, 6, 'h', 1.0f);
    /* 500 GOOD. - F, cute: bells, boom-chick, claps */
    st = (bad - good) / 6;
    CR(good, 0.22f);
    KK(good, 0.45f);
    bells(good, 0.05, "F5 A5 C6 F6", 0.13f);
    comp(good, st * 2, 3, "A4 C5 F5", 41, 0.9f);
    lane("...4..", good, st, 6, 's', 1.0f);
    lane("2.2.2.", good, st, 6, 'h', 1.0f);
    /* 500 BAD. - Ab gallop chugs (+ the scene's dim stab = Ab7) */
    st = (park - bad) / 6;
    CR(bad, 0.42f);
    play("Ab2!:1 Ab2 Ab2 Ab2! Ab2 Ab2", bad, st, I_GTR, 0.34f, 0);
    lane("644544", bad, st, 6, 'k', 1.0f);
    lane("...6..", bad, st, 6, 's', 1.0f);
    /* 1 PARK. - Bb, both sides; the fence squeezes (roll) ... */
    CR(park, 0.4f);
    KK(park, 0.6f);
    hit(park, pop - park, 46, 0.28f);
    bells(park, 0.05, "Bb5 D6 F6", 0.12f);
    choir(park, pop - park, "D4 F4 A#4", 0.11f);
    for (double u = park + 0.0625; u < pop - 0.02; u += 0.0625)
        SN(u, 0.12f + 0.3f * (float)((u - park) / (pop - park)));
    lane("5343", park, 0.125, (int)((pop - park) / 0.125), 'k', 1.0f);
    /* ... POP: C. cozy. (rings into s13's own C pad) */
    CR(pop, 0.28f);
    bells(pop, 0.04, "C6 E6 G6 C7", 0.15f);
    dn(pop, t13 - pop + 0.5, 36, TKV_TRI, 0.42f, 4, 1.0f);
    dn(pop, t13 - pop + 0.3, 72, TKV_SQ25, 0.12f, 3, 1.0f);
}

/* ending: the hook, shy, on a music box ("...and maybe, just maybe...");
 * the wall crumbles = KEY CHANGE to E major and the ANTHEM (cute hook, lead
 * + saw, twin harmony, choir, open power chords, metal drums) ... cut dead
 * when the camera finds the pile of shell casings. */
static void s13(void) {
    double t = tk_scene_start("s13_ending");
    double crumble = t + FR(36), tilt = t + FR(98);
    mute(tilt, tk_scene_start("s14_platforms"));
    G = 1.0f;
    gtr_bass = 0.4f;
    play("E5:2 E5:2 G5:2 E5:2 B5:4", t, 0.1, I_BELL, 0.17f, 0);
    dn(t, 0.85, 36, TKV_TRI, 0.32f, 3, 1.0f);
    /* the rumble: B (the new key's dominant) trembles, a snare roll leans in */
    double r0 = t + 0.8;
    for (double u = r0; u < crumble - 0.02; u += 0.0625) {
        float v = 0.1f + 0.2f * (float)((u - r0) / (crumble - r0));
        N(u, 0.05, 47, TKV_SAW, v);
        N(u, 0.05, 54, TKV_SQ50, v * 0.45f);
    }
    sw(r0, crumble - r0, 0.3, 35, TKV_TRI, 0.42f, 4);
    for (double u = r0; u < crumble - 0.02; u += 0.0625)
        SN(u, 0.1f + 0.3f * (float)((u - r0) / (crumble - r0)));
    /* CRUMBLE: E major, the anthem */
    double st = (tilt - crumble) / 16;
    echo_v = 0.07f;
    echo_dt = 3 * st;
    CR(crumble, 0.5f);
    KK(crumble, 0.75f);
    play("G#5:2 G#5:2 B5:2 G#5:2 E6:4 F#6:2 G#6:2", crumble, st, I_ANTHEM, 0.3f, 0);
    play("E5:2 E5:2 G#5:2 E5:2 C#6:4 D#6:2 E6:2", crumble, st, I_TWIN, 0.12f, 0);
    play("G#5:2 G#5:2 B5:2 G#5:2 E6:4 F#6:2 G#6:2", crumble, st, I_BELL, 0.06f, 12);
    play("E2!:2 E2! E2! E2! A2! A2! B2! B2!", crumble, st, I_GTR, 0.26f, 0);
    choir(crumble, 8 * st, "E4 G#4 B4 E5", 0.12f);
    choir(crumble + 8 * st, 4 * st, "E4 A4 C#5 A5", 0.12f);
    choir(crumble + 12 * st, 4 * st, "D#4 F#4 B4 B5", 0.12f);
    drums(crumble, st, 16, K_DBL, S_BB, H_8, 0.9f);
    CR(crumble + 8 * st, 0.35f);
    /* (the pile. silence. tink.) */
}

/* platforms: the cute theme returns in C - and stops dead for the deadpan
 * "Yes, the actual PlayStation." - back in on the HIGH FIVE */
static void s14(void) {
    double t = tk_scene_start("s14_platforms");
    double yes = t + FR(74), clap = t + FR(112), t15 = tk_scene_start("s15_logo");
    double B = 0.5, S = 0.125;
    mute(yes, clap);
    G = 0.9f;
    echo_v = 0.08f;
    echo_dt = 3 * S;
    play(HOOK1, t, S, I_LEAD, 0.28f, 0);
    play(HOOK2, t + 2.0, S, I_LEAD, 0.28f, 0);
    play(HOOK1 " " HOOK2, t, S, I_BELL, 0.07f, 12);
    comp(t, B, 4, "C5 E5 G5", 48, 0.9f);
    comp(t + 2.0, B, 4, "A4 C5 E5", 45, 0.9f);
    drums(t, S, 32, "5.......4.......", "....3.......3...", "2.2.2.2.2.2.2.2.", 1.0f);
    CR(t, 0.24f);
    /* *clap* */
    CR(clap, 0.32f);
    KK(clap, 0.55f);
    int c[3] = { 72, 76, 79 };
    stab(clap, 0.2, c, 3, TKV_SQ12, 0.13f);
    dn(clap, 0.26, 48, TKV_TRI, 0.48f, 2, 1.0f);
    /* pickup into the logo */
    play("C5:2 D5:2", t15 - 0.5, S, I_LEAD, 0.28f, 0);
    comp(t15 - 0.5, B, 1, "B4 D5 G5", 43, 0.9f);
    SN(t15 - 2 * S, 0.25f);
    SN(t15 - S, 0.32f);
}

/* logo: the hook in C while the letters drop; HELL ignites = the anthem in
 * E with the metal drums (the screen is split, so is the band); a final E
 * major chord, held and fading - its third slips flat as it dies - silent
 * before the cut to black and the post-credits "beep." */
static void s15(void) {
    double t = tk_scene_start("s15_logo");
    double S = 0.125, ignite = t + FR(30), fin = ignite + 1.0, out = t + FR(110);
    double hold = out - 0.06 - fin;
    G = 1.0f;
    gtr_bass = 0.4f;
    echo_v = 0.07f;
    echo_dt = 3 * S;
    /* the letters drop: C */
    CR(t, 0.22f);
    play("E5:2 E5:2 G5:2 E5:2", t, S, I_LEAD, 0.27f, 0);
    play("E5:2 E5:2 G5:2 E5:2", t, S, I_BELL, 0.07f, 12);
    comp(t, 0.5, 2, "C5 E5 G5", 48, 0.9f);
    drums(t, S, 8, "5...4...", "....3...", "2.2.2.2.", 1.0f);
    /* HELL ignites: E, metal */
    CR(ignite, 0.45f);
    KK(ignite, 0.7f);
    play("G#5:2 G#5:2 B5:2 G#5:2", ignite, S, I_ANTHEM, 0.29f, 0);
    play("E5:2 E5:2 G#5:2 E5:2", ignite, S, I_TWIN, 0.11f, 0);
    play("E2!:2 E2! E2! E2!", ignite, S, I_GTR, 0.25f, 0);
    choir(ignite, 1.0, "E4 G#4 B4", 0.1f);
    drums(ignite, S, 8, K_DBL, S_BB, H_8, 0.9f);
    TOM(fin - 2 * S, 52, 0.4f);
    TOM(fin - S, 45, 0.45f);
    /* the final chord */
    CR(fin, 0.42f);
    KK(fin, 0.65f);
    dn(fin, hold, 88, TKV_SQ25, 0.28f, 6, 1.0f);              /* E6 lead   */
    dn(fin, hold, 76, TKV_SAW, 0.08f, 6, 1.0f);
    dn(fin, hold, 28, TKV_TRI, 0.5f, 6, 1.0f);                /* E1 bass   */
    dn(fin, hold, 40, TKV_SAW, 0.12f, 6, 1.0f);               /* power chord */
    dn(fin, hold, 47, TKV_SQ50, 0.06f, 6, 1.0f);
    dn(fin, hold, 64, TKV_TRI, 0.15f, 6, 1.0f);               /* pad E B   */
    dn(fin, hold, 71, TKV_TRI, 0.14f, 6, 1.0f);
    dn(fin, hold - 0.5, 68, TKV_TRI, 0.15f, 5, 1.0f);         /* the third ... */
    bells(fin, 0.045, "E6 G#6 B6 E7 G#7", 0.12f);
    dn(fin + 0.3, hold - 0.3, 100, TKV_SINE, 0.07f, 4, 1.4f);
    /* ... slips to G as it dies. (silence. cut to black. "beep.") */
    RS(fin + hold - 0.62, 0.56, 68, 67, TKV_TRI, 0.07f);
    RS(fin + hold - 0.62, 0.56, 80, 79, TKV_SINE, 0.045f);
}

/* make room for the little sfx the jokes hang on */
static void ducks(void) {
    double t04 = tk_scene_start("s04_doomogachi"), t06 = tk_scene_start("s06_life");
    double t10 = tk_scene_start("s10_shooter"), t11 = tk_scene_start("s11_exorcism");
    double t13 = tk_scene_start("s13_ending"), t14 = tk_scene_start("s14_platforms");
    duck(t04 + FR(40), t04 + FR(46), 0.55f);   /* "beep." (hatched)          */
    duck(t04 + FR(134), t04 + FR(140), 0.5f);  /* "beep." (it's fine)        */
    duck(t06 + FR(19), t06 + FR(25), 0.4f);    /* the distant "beep."        */
    duck(t10 + FR(9), t10 + FR(24), 0.7f);     /* imp gibbering              */
    duck(t11 + FR(8), t11 + FR(33), 0.6f);     /* Father Fuzz yelling        */
    duck(t13 + FR(46), t13 + FR(52), 0.55f);   /* "!" beep                   */
    duck(t14 + FR(14), t14 + FR(30), 0.75f);   /* the handheld's power-on    */
}

void tk_music(void) {
    static void (*const SECTIONS[])(void) = {
        ducks, s01, s02, s03, s04, s05, s06, s07, s08_party,
        s08_doom, s09, s10, s11, s12, s13, s14, s15,
    };
    nmw = ndw = 0;
    for (pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < sizeof SECTIONS / sizeof SECTIONS[0]; i++) {
            G = 1.0f;
            echo_v = 0.0f;
            gtr_bass = 0.42f;
            SECTIONS[i]();
        }
}
