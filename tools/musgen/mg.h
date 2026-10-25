/* mg.h - musgen's composer (owner: MU1T; docs/design/music/SPEC.md sec.8.3,
 * 16.2, D2, D10). The build-time generative composer: the design study's
 * prototype (design_generative.md) moved to the host verbatim - only its
 * identifiers carry an Mg / mg_ / G prefix, so it can sit beside the core
 * headers - and therefore bit-identical: `musgen fingerprint` must print
 * 37d71b12 8d898ef0 c33cfa26 bdcb0bd2 on gcc and clang (MU1T acceptance).
 * It never ships: musgen writes curated .dms text that mmlc compiles.
 *
 * C99, integer only, no malloc, no stdio, no division in the hot path
 * (the ARM7TDMI has no divide instruction). Deterministic: every bar is a
 * pure function of (piece seed, latched params, section plan, bar index),
 * so any bar can be regenerated (DECK scratch rewind, resume, tests).
 *
 * Time: 1 unit = 1/48 beat (the repo's AUDIO_WHOLE/4 grid); a 16th = 12.
 */
#ifndef MG_H
#define MG_H
#include <stdint.h>
#include <string.h>

/* ---- vocabulary ------------------------------------------------------- */
enum { GMODE_MAJOR, GMODE_MINOR, GMODE_HARM, GMODE_DORIAN, GMODE_PHRYG, GMODE_MIXO, GMODE_COUNT };
enum { GR_LEAD, GR_COUNTER, GR_HARM, GR_BASS, GR_DRUMS, GR_PAD, GR_ECHO, GR_DESCANT, GR_COUNT };
enum { GDR_NONE, GDR_KICK, GDR_SNARE, GDR_HAT, GDR_OPEN, GDR_CRASH, GDR_TOM, GDR_CLAP, GDR_COUNT };
enum { GCQ_MAJ, GCQ_MIN, GCQ_DIM, GCQ_AUG, GCQ_SUS4, GCQ_POW, GCQ_DOM7, GCQ_MAJ7, GCQ_MIN7,
       GCQ_HDIM7, GCQ_DIM7, GCQ_ADD9, GCQ_COUNT };
enum { GFN_T, GFN_S, GFN_D };
enum { GCAD_PAC, GCAD_HC, GCAD_DC, GCAD_PLAGAL, GCAD_AEOL, GCAD_COUNT };
enum { GSEC_INTRO, GSEC_A, GSEC_A2, GSEC_B, GSEC_C, GSEC_BRK, GSEC_BUILD, GSEC_PEAK, GSEC_TAG,
       GSEC_REST, GSEC_COUNT };
/* motif transforms (tail only; the head is protected) */
enum { GXF_LIT, GXF_SEQ, GXF_INV, GXF_RETRO, GXF_RHY, GXF_CONT, GXF_FRAG, GXF_ORN, GXF_COUNT };
/* phrase "ideas" (2-bar units of a section) */
enum { GID_STATE, GID_VAR, GID_SEQ, GID_FRAG, GID_FREE, GID_CAD, GID_REST, GID_COUNT };
/* harmony and bass styles */
enum { GHS_OFFBEAT, GHS_ARP8, GHS_PAD, GHS_POWER, GHS_WALTZ, GHS_COUNT };
enum { GBS_ROOT5, GBS_OCT8, GBS_WALK, GBS_GALLOP, GBS_CHUG, GBS_LONG, GBS_WALTZ, GBS_RIFF, GBS_COUNT };

/* ---- data (ROM) -------------------------------------------------------- */
/* One motif note: scale degree from the tonic (0 = tonic, 7 = octave),
 * chromatic alteration, length in 16th steps, flags. 4 bytes here; the
 * shipped format packs it into 2 (design sec. 3.3). */
typedef struct MgNote { int8_t deg; int8_t acc; uint8_t dur; uint8_t fl; } MgNote;
#define GNF_REST 1
#define GNF_HEAD 2           /* protected: never altered except transposition / mode */

typedef struct MgMotif {
    const MgNote *n;
    uint8_t len;            /* notes                                        */
    uint8_t head;           /* first `head` notes are protected              */
    uint8_t bars;           /* 1 or 2                                        */
    uint8_t fixed;          /* 1 = keeps its own mode (WALL, the Riff's b5)  */
} MgMotif;

/* Anchor progression slot: degree 0..6, alteration, quality. */
#define AC(deg, alt, q) ((uint8_t)((deg) | (((alt) + 1) << 3) | 0)), (uint8_t)(q)

/* Drum DNA: per lane (kick snare hat open clap tom) 16 steps of 0..10
 * (tenths; 10 = always). Masked by energy at runtime. */
typedef struct MgDna { uint8_t p[6][16]; } MgDna;

typedef struct MgStyle {         /* one band of the style dial */
    uint8_t harm_style, bass_style, swing, lead_gate;   /* gate in 1/8   */
    uint8_t chord_rate;           /* chord slots per bar: 1 or 2        */
    uint8_t lead_inst, harm_inst, bass_inst, ctr_inst;
    uint8_t power;                /* 1 = metal: power chords + twin lead */
    const MgDna *dna;
} MgStyle;

typedef struct MgTheme {
    const char *name;
    uint8_t key, mode, bpm, steps;    /* steps per bar: 16 (4/4) or 12 (3/4) */
    uint8_t dial;                     /* home style band 0..4               */
    uint8_t lead_lo, lead_hi;         /* MIDI range of the lead             */
    uint8_t var_max;                  /* cap on the variation level 0..3    */
    uint8_t nmotif;
    const MgMotif *motif;            /* [0] = the theme (head + tail)      */
    uint8_t anchor_n;                 /* slots in the anchor progression    */
    const uint8_t *anchor;            /* pairs (deg|alt<<3, quality)        */
    uint8_t riff;                     /* 1 = motif[1] is a bass riff        */
    uint8_t final_cad;                /* GCAD_PAC or GCAD_AEOL                */
} MgTheme;

/* ---- runtime ----------------------------------------------------------- */
typedef struct MgEv {                /* bar-relative event (8 bytes)       */
    uint16_t at;                      /* units from the bar start           */
    uint16_t dur;                     /* units                              */
    uint8_t  role, note, vel, inst;   /* note: MIDI, or DR_* on GR_DRUMS     */
} MgEv;

#define MG_BAR_EV 96
typedef struct MgBar {
    uint32_t sig;                     /* signature of all events            */
    uint32_t sig_lead;                /* lead only                          */
    uint16_t n;
    uint16_t len;                     /* units                              */
    uint8_t  sec_type, bar_in_sec, energy, key;
    uint8_t  head_stated;             /* 1 if the theme head sounds here    */
    uint8_t  cadence;                 /* CAD_* + 1 at a phrase end, else 0  */
    MgEv    ev[MG_BAR_EV];
} MgBar;

typedef uint16_t MgChord;            /* root semi 0..11 | quality<<4 | fn<<8 | deg<<10 */
#define GCH_ROOT(c) ((c) & 15)
#define GCH_Q(c)    (((c) >> 4) & 15)
#define GCH_FN(c)   (((c) >> 8) & 3)
#define GCH_DEG(c)  (((c) >> 10) & 7)
#define GCH_MAKE(r, q, f, d) ((MgChord)((r) | ((q) << 4) | ((f) << 8) | ((d) << 10)))

#define MG_MAX_BARS 16
#define MG_POOL_PROG 3
#define MG_POOL_MOTIF 2
#define MG_SIGRING 64

typedef struct MgSection {           /* the plan of the running section   */
    uint8_t type, bars, key, mode;
    uint8_t energy, var, dial, cad_mid;
    uint8_t prog_src;                 /* 0 = anchor, 1.. = pool             */
    uint8_t idea[MG_MAX_BARS / 2];   /* ID_* per 2 bars                    */
    uint8_t xf[MG_MAX_BARS / 2];     /* XF_* per idea                      */
    int8_t  shift[MG_MAX_BARS / 2];  /* sequence shift per idea (degrees)  */
    uint8_t motif;                    /* 0 = theme, 1.. = session pool      */
    uint8_t lead_oct;                 /* register offset index              */
    uint8_t stems;                    /* bit per role                       */
    uint8_t fills;                    /* bit per bar: fill at its end       */
    uint16_t lane[6];                 /* drum base pattern for the section  */
    uint16_t opt[6];                  /* steps the DNA leaves optional      */
    MgChord chord[MG_MAX_BARS * 2]; /* slots                              */
    uint32_t seed;
} MgSection;

typedef struct MgPiece {
    const MgTheme *th;
    uint32_t seed;
    uint32_t bar_total;               /* bars played                        */
    uint16_t sec_count;               /* sections started                   */
    uint8_t  bar_in_sec;
    uint8_t  hist[4];                 /* last section types                 */
    uint8_t  since_anchor, chapter, chap_pos, arc;
    uint8_t  type_passes[GSEC_COUNT];
    uint8_t  intensity, dial;         /* latched drivers (0..5, 0..4)       */
    uint8_t  key_lift;                /* semitones above home               */
    MgSection cur;
    /* session pools (rotate one element per chapter) */
    MgNote  pm[MG_POOL_MOTIF][16];
    uint8_t  pm_len[MG_POOL_MOTIF];
    MgChord pprog[MG_POOL_PROG][8];
    uint8_t  pprog_cad[MG_POOL_PROG];
    uint16_t pgroove[2][6];
    uint8_t  pool_next;               /* which element rotates next          */
    /* anti-repeat guard: 4-bar window hashes */
    uint32_t sig[4];
    uint32_t win[MG_SIGRING];
    uint8_t  win_pos, guard_hits;
    uint32_t work;                    /* abstract work units (tests)         */
} MgPiece;

void mg_start(MgPiece *p, const MgTheme *th, uint32_t seed);
void mg_set(MgPiece *p, int intensity, int dial);   /* latched at sections */
void mg_next_bar(MgPiece *p, MgBar *out);
int  mg_key_of(const MgPiece *p);

extern const MgTheme mg_theme_hook, mg_theme_riff, mg_theme_home, mg_theme_title;
extern const MgStyle mg_styles[5];

#endif
