/* music_rep.h - the non-repetition metric of T3 (owner: MU6;
 * docs/design/music/SPEC.md sec.8.5, CF-37, CF-38). Host test code only.
 *
 * A RepRun is a list of per-bar records of one deck-A stream: what each
 * role plays (the decision stream, CF-33), reduced to the CF-38 signatures
 *
 *   S  structural full mix: pitched roles as (role, onset on the 16th
 *      grid, pitch, length class) + drum CORE steps, fills and crashes +
 *      the stems + the dial band. No velocity, no ghost or probabilistic
 *      drum steps, no microtiming, no VIB / TIMBRE / ECHO, no GRACE /
 *      MORDENT / TURN ornaments (D9);
 *   P  the pitched roles only, same fields;
 *   L  LEAD only (a bar without lead never matches; an anchor's first 4
 *      bars are masked out: they are the identity);
 *   F  fuzzy: per role class a 16-step onset bitmap and a 12-bit pitch-
 *      class set; similarity = the weighted mean of Jaccard(onsets) x
 *      Jaccard(pitch classes), LEAD 3, BASS 2, HARM 1, COUNTER 1, KIT 1
 *      (core steps only); a match is >= 0.85;
 *
 * plus an 8-bar texture vector (stems, dial band, key, tempo band, LEAD
 * register, density band). rep_gates() measures the CF-38 gates on it.
 *
 * Producers: the engine (test_music_rep.c drives a private deck, like
 * music_hash), the build-time composer (tools/musgen's prototype, linked
 * into the tests by music_rep_proto.c) and the synthetic loops of T18. */
#ifndef DOOMO_MUSIC_REP_H
#define DOOMO_MUSIC_REP_H

#include <stdint.h>

/* role classes of the fuzzy signature (and their CF-38 weights) */
enum { RC_LEAD, RC_COUNTER, RC_HARM, RC_BASS, RC_KIT, RC_N };

/* one sounding note of a bar */
enum {
    RNF_DRUM  = 1,     /* pitch = KH_* hit | kit << 3                        */
    RNF_CORE  = 2,     /* a drum that always plays here (p = 15), a fill or a
                        * crash; drums without it are dice / ghosts           */
    RNF_ORN   = 4,     /* GRACE / MORDENT / TURN: ornament-blind             */
    RNF_SURF  = 8,     /* ECHO / VIB / TIMBRE surface ops: never variation   */
    RNF_CHORD = 16     /* a chord (the chip chord): pitch = root, q = quality */
};
typedef struct RepNote {
    uint16_t at;       /* units from the bar start (48 per quarter)          */
    uint16_t len;      /* units it sounds                                     */
    uint8_t  role;     /* the cue's role index (0..15)                       */
    uint8_t  cls;      /* RC_*, or RC_N for a role the metric ignores (beds) */
    uint8_t  pitch;    /* semitones (MIDI-like); drums see RNF_DRUM          */
    uint8_t  flags;    /* RNF_*                                               */
    uint8_t  q;        /* chord quality (RNF_CHORD), else 0                  */
    uint8_t  vel;      /* carried for the velocity check of T18(d): IGNORED  */
    uint8_t  pad[2];
} RepNote;

/* what the producer knows about a bar */
enum {
    RBF_HEAD    = 1,   /* the theme head sounds in this bar                  */
    RBF_ANCHOR4 = 2,   /* one of an anchor's first 4 bars (masked out of L)  */
    RBF_CAD     = 4,   /* a cadence point (a section / node end)             */
    RBF_CAD_OK  = 8,   /* ... and it is legal                                */
    RBF_SEAM    = 16,  /* a node boundary with neither a legal cadence nor a
                        * turnaround                                         */
    RBF_LITERAL = 32,  /* a lead bar with bar_op 0 and ALT 0 (identity)      */
    RBF_ANCHOR  = 64   /* the bar belongs to an anchor node / section        */
};
typedef struct RepMeta {
    uint8_t dial;      /* the dial band 0..4 (S0..S4)                        */
    uint8_t key;       /* tonic pitch class | mode << 4                       */
    uint8_t bpm;       /* tempo (the band is bpm / 16)                       */
    uint8_t flags;     /* RBF_*                                               */
} RepMeta;

/* producer knowledge (RepRun.know) */
enum { RK_HEAD = 1, RK_CAD = 2, RK_LITERAL = 4, RK_SEAM = 8 };

typedef struct RepBar {
    uint32_t S, P, L;           /* 0 = an empty bar (never matches)        */
    uint16_t ons[RC_N];         /* onset bitmaps (16th steps)             */
    uint16_t pcs[RC_N];         /* pitch classes; KIT: the hit types       */
    uint16_t stems;             /* the roles sounding                      */
    uint16_t lead_sum;          /* sum of the lead pitches ...            */
    uint8_t  lead_n;            /* ... over this many lead notes           */
    uint8_t  notes;             /* notes counted (capped at 255)          */
    RepMeta  m;
} RepBar;

typedef struct RepRun {
    RepBar  *b;
    int      n, cap;
    int      bar_u;             /* units per bar                           */
    int      know;              /* RK_*                                    */
    int      bpm;               /* for timestamps                          */
    /* the bar being built */
    RepNote  cur[256];
    int      ncur;
    RepMeta  cm;
} RepRun;

void rep_init(RepRun *r, int beats_per_bar, int bpm, int know);
void rep_free(RepRun *r);
void rep_begin_bar(RepRun *r, const RepMeta *m);
void rep_note(RepRun *r, const RepNote *n);
void rep_end_bar(RepRun *r);
/* the 0..1000 fuzzy similarity of bars i and j */
int  rep_sim(const RepRun *r, int i, int j);

/* the measures */
typedef struct RepSpan { int len, at, lag; } RepSpan;     /* bars */
typedef struct RepStats {
    int bars;
    int lrr_s, lrr_p, lrr_l;            /* longest repeated runs (bars)           */
    int lrr_s_at, lrr_s_lag;
    int near_run, near_at, near_lag;    /* gate 2: the longest near-loop (lag < 64) */
    int recency_pm;                     /* gate 3: per mille of 8-bar windows whose
                                         * nearest F-match is >= 24 bars back       */
    int texture_pm;                     /* gate 4: per mille of 32-bar spans with a
                                         * texture change                            */
    int texture_min10;                  /*   the fewest distinct vectors in any
                                         *   10-minute stretch                       */
    int head_gap;                       /* gate 5 (RK_HEAD)                          */
    int cad_n, cad_ok;                  /*        (RK_CAD)                           */
    int seams;                          /*        (RK_SEAM)                          */
    int lit_n, lit_ok;                  /*        (RK_LITERAL)                       */
    RepSpan worst[5];                   /* the 5 worst near-repeat spans (gate 7)    */
} RepStats;
void rep_measure(const RepRun *r, RepStats *st);

/* the CF-38 hub gates (60 min): returns a bit per failed gate (1 << g, g =
 * 1..5); `steady` = the steady script, which also owes 4 distinct texture
 * vectors per 10 minutes. why (may be NULL) gets the first failure. */
int  rep_hub_gates(const RepRun *r, const RepStats *st, int steady, char *why, int why_len);
/* the CF-38/sec.8.5 gate for every adaptive cue with >= 2 nodes (10 min):
 * no identical 8-bar S or 16-bar P window, LRR(P) <= 8 */
int  rep_cue_gates(const RepStats *st, char *why, int why_len);
/* seconds into the stream of bar i (renders are named by seconds) */
int  rep_seconds(const RepRun *r, int bar);

/* longest repeated run of a signature sequence (0 never matches) */
int  rep_lrr(const uint32_t *sig, int n, int *at, int *lag);

/* the prototype composer (tools/musgen/mg_gen.c, music_rep_proto.c):
 * theme 0..3 = hook riff home title; script 0 steady, 1 walk, 2 morph */
enum { RS_STEADY, RS_WALK, RS_MORPH, RS_COUNT };
void rep_proto_run(RepRun *r, int theme, uint32_t seed, int minutes, int script);
const char *rep_proto_name(int theme);
/* the same stream's old full-mix signature (it hashed velocity: CF-37) */
int  rep_proto_old_lrr(int theme, uint32_t seed, int minutes, int script);
/* proto/host_main.c's 10-minute hash: 37d71b12 8d898ef0 c33cfa26 bdcb0bd2 */
uint32_t rep_proto_fingerprint(int theme);

#endif
