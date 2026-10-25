/* mus_int.h - internals shared by the music engine's files (mus_*.c,
 * audio_seq.c) (owner: MU0 skeleton, MU1 maintainer; docs/design/music/
 * SPEC.md sec.4, 6). Not part of the contract: only music.h, mus_data.h
 * and platform.h are.
 *
 * Layout (docs/AUDIO.md has the RAM ledger):
 *   mus_lane.c   the arranger: lane tokens -> events, bar lines, stems,
 *                twin, GEN variants, the scrub decoder
 *   mus_core.c   the bank, music_update(), params and intensity, requests
 *                and the matrix, stingers, the comic bridge, beat events,
 *                windows, quotes, beds, session memory, the hash
 *   mus_graph.c  the walk (DG sec.4.5), GEN bags, chapters, pools, lifts,
 *                the transition matrix lookup, the decision hash mus_h
 *   mus_vinyl.c  vinyl patterns, mus_vinyl_speed, DECK scrubbing
 *   mus_drv.c    the 240 Hz driver: rings, clock, dispatch, voices, macros
 *   mus_bind.c   SONG_* -> cue
 *   audio_seq.c  audio_music*() on the engine, the legacy bridge flush,
 *                the classic interpreter (T1's reference until M5)
 */
#ifndef DOOMO_MUS_INT_H
#define DOOMO_MUS_INT_H

#include "doomo/doomo.h"
#include "doomo/music.h"

/* ---- sizes (the RAM ledger, SPEC sec.5, CF-12, CF-19) ------------------------------ */
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_PLATFORM_PS1)
#define MUS_CONSOLE 1
#endif
#ifdef MUS_CONSOLE
/* GBA: 40 (CF-19). PS1: 40 while the bridge runs the driver in-thread (M1,
 * a 2-tick look-ahead); MU3 raises it to 128 with the IRQ driver at M3 (D22). */
#define MUS_RING_A 40
#else
#define MUS_RING_A 128          /* native: the PS1 size, so T4 proves it (D22)  */
#endif
#define MUS_RING_B 32
#define MUS_DRV_VOICES 4        /* the bridge's four (M2/M3: 16 GBA, 20 PS1)     */
#define MUS_A_VOICES 4          /* deck A's rendered voices on the bridge         */
#define MUS_A_ROLES 12          /* deck A role runtimes (MusCue.n_roles <= 12)    */
#define MUS_B_VOICES 4          /* deck B: the dual song, stingers, the scrub     */
#define MUS_WINDOWS 4           /* driver mute windows                           */
#define MUS_LOOKAHEAD_U 192     /* >= 1 bar of 4/4 ahead of the driver (D5)      */
#define MUS_BAR_LOG 4           /* CF-20                                          */
#define MUS_BEATQ 6             /* the polled beat ring (SPEC sec.20 fallback)    */
#define MUS_BEAT_CBS 4          /* resident beat callbacks                        */
#define MUS_ALL_A ((1u << MUS_A_VOICES) - 1u)

/* ---- the bank (mus_core.c) ------------------------------------------------------------ */
typedef struct MusCueView {
    const uint8_t  *blob;
    const MusCue   *cue;
    const MusRole  *roles;
    const MusNode  *nodes;
    const MusEdge  *edges;
    const MusBind  *binds;
    const MusStingHdr *sting;   /* stingers only                                 */
    uint16_t        len;
    uint8_t         id, kind;
} MusCueView;
const MusBank *mus_bank(void);
/* tests: play another bank (NULL = the generated one); mus_core_init() after */
void mus_bank_use(const uint8_t *bank);
int  mus_bank_cue(int id, MusCueView *out);       /* 1 = found */
int  mus_bank_sting(int id, MusCueView *out);     /* 1 = found */
const MusInst *mus_bank_inst(int i);              /* never NULL (row 0 / a silent row) */
/* misc table i (MUS_MISC_*), NULL if the bank has none */
const uint8_t *mus_bank_misc(int i);
/* the same, cached (set by mus_core_init; hot paths) */
extern const MusInst *mus_inst_tab;
extern uint16_t mus_inst_n;
#define MUS_INST(i) ((unsigned)(i) < mus_inst_n ? &mus_inst_tab[i] : mus_bank_inst(i))
/* the classic tempo law: 1/256 units per 30 Hz tick (today's sequencer) */
int32_t mus_rate256(int bpm);
/* frames a note of `units` lasts (the classic note_ticks) */
int  mus_note_frames(int units, int32_t rate256);
static inline unsigned mus_rd16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static inline int32_t mus_pdiff(uint32_t a, uint32_t b) { return (int32_t)((a - b) << 4) >> 4; }

/* ---- the arranger (mus_lane.c) ---------------------------------------------------------- */
enum { MGS_IDLE, MGS_PLAY, MGS_ENDED };
enum { MGF_ONCE = 1, MGF_SCRUB = 2 };             /* MusGen.mode */
enum { MVF_NOTE = 1, MVF_DIRTY = 2, MVF_DONE = 4, MVF_RET = 8, MVF_TWIN = 16 };   /* MusVoiceRt.flags */
typedef void (*MusEmitFn)(void *ctx, uint32_t pos_kind, uint32_t arg);
typedef struct MusSink { MusEmitFn emit; void *ctx; int room; } MusSink;
/* CF-20: one record per deck-A bar line */
typedef struct MusBarLog {
    uint32_t pos;               /* master position of the bar line                */
    uint16_t stems;             /* roles sounding in the bar                      */
    uint8_t  node, pass, bar, dial, var;
    int8_t   ktrans;
} MusBarLog;                                                          /* 12 */
typedef struct MusGen {
    MusDeck     d;              /* d.bar = bar in node, d.vlevel = lift, d.arc_i = lifts */
    MusVoiceRt *v;              /* n_roles runtimes; v[r].role = the cue role     */
    MusCueView  cv;
    const uint8_t *pack;        /* the current node's pack                       */
    uint32_t    base;           /* scaled decks: master position of local 0      */
    uint32_t    next;           /* master position of the next unwritten event   */
    int32_t     rate256;
    uint16_t    node_len;       /* units                                         */
    uint16_t    bar_next;       /* the next bar line, units into the node        */
    uint16_t    scale_q8;       /* master units per local unit (deck B), 256 = 1 */
    uint8_t     n_roles, max_roles, mode, deck;
    uint8_t     vmap[MUS_A_ROLES];  /* runtime -> driver voice, 0xFF = not played */
    uint8_t     var;            /* GEN variant of the node, 0xFF = the base      */
    int8_t      ktrans;         /* transposition of every pitched role           */
    uint8_t     bar_u_meter;    /* beats per bar                                 */
    uint8_t     pad;
} MusGen;
/* start cue `cv` at master position `base`; vmap NULL = the default plan */
void mus_gen_start(MusGen *g, const MusCueView *cv, uint32_t base, int32_t rate256,
                   int mode, const uint8_t *vmap);
void mus_gen_stop(MusGen *g);
/* write every event before master position `until` (while sink->room >= 4);
 * returns the position everything before which has been written */
uint32_t mus_gen_until(MusGen *g, uint32_t until, MusSink *sink);
/* (re)enter node `node` at master position `pos` with a forced pass and
 * GEN variant (the scrub, CONTINUE; 0xFF = choose as usual) */
void mus_gen_enter(MusGen *g, int node, uint32_t pos, int pass, int var);
/* the default voice plan of a cue's roles (CF-34: generated, then filtered) */
void mus_plan(const MusCueView *cv, uint16_t stems, int deck, uint8_t *vmap);
int  mus_twin_pitch(int key, int pitch);          /* a diatonic 3rd below in key */
const uint8_t *mus_scale(int mode);              /* the 7 steps of MMODE_* mode  */
int  mus_scale_snap(int key, int pitch16);

/* ---- the walk (mus_graph.c) ------------------------------------------------------------------ */
enum { S_SEED = 1, S_WALK, S_MEL, S_ALT, S_GEN, S_POOL, S_LIFT, S_HARM, S_BASS, S_DRUM, S_FILL,
       S_QUOTE, S_BED, S_VINYL, S_VEL };
uint32_t mus_h(uint32_t seed, uint32_t stream, uint32_t a, uint32_t b);
/* the next node; *end = 1 when a once play / a dead end stops */
int  mus_graph_next(MusGen *g, int node, int once, int *end);
int  mus_bag(uint32_t h, int n, int k);           /* element k of a hashed permutation of n */
int  mus_pool_value(uint32_t seed, int e, int chapter, int n);
int  mus_gen_variant(uint32_t seed, int node, int pass, int n);   /* GEN bag pick */
/* the transition matrix row for (from, to), NULL = none (CUT) */
const MusTrans *mus_trans_find(int from, int to);
int  mus_seam_ok(const MusGen *g, int from, int to);   /* walk rule 5 */
extern uint16_t mus_seams;                          /* illegal seams taken (relaxed)  */
extern uint8_t mus_intensity_now;                  /* the committed intensity     */

/* ---- vinyl (mus_vinyl.c) ---------------------------------------------------------------------- */
/* a pattern: n_seg 4-byte segments (MUS_MISC_VINYL layout) */
/* a pattern (MusVinyl, mus_data.h): flags MVF_*, after = its AFTER_* default,
 * len = units in all, fr_q8 = units per 30 Hz frame (Q8) for MVF_FRAMES
 * patterns, fixed at the pattern's start (the driver and the arranger read
 * this same view, so they agree bit for bit) */
typedef struct MusVinylView { const uint8_t *seg; uint8_t n_seg, flags, after, pad; uint16_t len, fr_q8; } MusVinylView;
int     mus_vinyl_get(int pattern, MusVinylView *out);   /* 1 = a pattern */
/* platter speed (Q8, 256 = play) at unit t of the pattern; the driver and
 * the arranger call exactly this (T9) */
int     mus_vinyl_speed(const MusVinylView *p, int t, uint32_t seed);
int     mus_vinyl_fader(const MusVinylView *p, int t);   /* 0 / 1 */
int32_t mus_vinyl_disp(const MusVinylView *p, int t, uint32_t seed);   /* sum of speeds over [0, t) */
int     mus_vinyl_pitch(int speed_q8);            /* 1/16 st: 192 log2 |v|          */
int     mus_vinyl_gain(int speed_q8);             /* 0..256: min(1, 2|v|), 0 < 1/16 */
/* the scrub: called by music_update while a DECK pattern runs */
void    mus_vinyl_update(void);

/* ---- the driver (mus_drv.c) ------------------------------------------------------------------ */
typedef struct MusKitHit { uint16_t f0, f1; uint8_t vol, decay16, ticks_sweep, pad; } MusKitHit;
extern const MusKitHit mus_kit_classic[KH_COUNT];
#define MUS_INST_KIT0 0xF0      /* DoomoVoice.inst of a drum: 0xF0 + kit         */
#define MUS_INST_BED0 0xE0      /* built-in bed instruments (mus_bank_inst)       */
void     mus_drv_reset(void);
int      mus_drv_room(int ring);
void     mus_drv_push(int ring, uint32_t pos_kind, uint32_t arg);   /* ring B: sorted */
void     mus_drv_truncate(int ring);
void     mus_drv_truncate_at(int ring, uint32_t pos);   /* drop undispatched >= pos */
void     mus_drv_cut(uint32_t voice_mask);
void     mus_drv_set_tempo(uint32_t upm_q16);
uint32_t mus_drv_tempo(void);
uint32_t mus_drv_rebase(void);                    /* drop the clock fraction -> clock */
uint32_t mus_drv_clock(void);
uint16_t mus_drv_clock_frac(void);
uint32_t mus_drv_mt(void);                        /* driver ticks so far            */
void     mus_drv_written(int ring, uint32_t pos);
uint8_t  mus_drv_take_marks(void);
#ifndef MUS_CONSOLE
/* native tests: every event as the driver dispatches it (before mute windows) */
extern void (*mus_drv_trace)(const DoomoMusEv *e, int ring);
#endif
uint32_t mus_drv_keyons(void);
uint32_t mus_drv_underruns(void);
void     mus_drv_set_master(int vol10);           /* OPT_MUSIC_VOL, 0..10        */
void     mus_drv_set_ctl(int mt_per_frame);       /* 8 = 30 Hz (bridge), 4 = 60 Hz */
void     mus_drv_fence(int on, uint32_t pos);
const DrvVoice *mus_drv_voice(int v);
void     mus_drv_run(int mt);                     /* the bridge: mt driver ticks   */
extern const uint8_t mus_vol63_of15[16];
int      mus_drv_ring_used(int ring);
int      mus_drv_muted(int deck, uint32_t pos);   /* a live window of deck covers pos */
/* vinyl in the driver: deck-A voices bend from master position `start` */
int      mus_drv_vinyl_on(void);
int16_t  mus_drv_vinyl_speed(void);               /* the last control frame's (T9) */
void     mus_drv_vinyl_live(int16_t speed_q8, uint8_t fader);   /* live DJ input */
/* the driver's view of a pattern (set by mus_vinyl.c before VINYL_START) */
extern MusVinylView mus_drv_vy;
extern uint32_t mus_drv_vy_seed;

/* ---- the core (mus_core.c) ---------------------------------------------------------------------- */
void mus_core_init(void);
void mus_core_start(int cue, int once);           /* MQ_NOW; MC_NONE stops */
void mus_core_dual(int cue_a, int cue_b);
int  mus_core_take_end(void);                     /* 1 once: a once cue ended */
int  mus_core_bridge(void);                       /* 1 = the legacy bridge renders */
/* tests (T4): run as if an IRQ drove the driver - music_update() only
 * generates (>= 1 bar ahead) and the caller ticks mus_drv_tick() */
void mus_core_test_irq(int on);
int  mus_drv_vol15(int level, int vel, int master);
/* deck A at a bar line (mus_lane.c): stems, dial, the bar log */
void mus_core_bar(MusGen *g, uint32_t pos, int node_start);
/* the bar log entry whose bar holds master position pos (NULL = none) */
const MusBarLog *mus_core_log_at(uint32_t pos);
MusGen *mus_core_deck(int d);                     /* 0 = A, 1 = B */
uint32_t mus_core_lookahead(void);                /* units the arranger writes ahead */
uint32_t mus_core_pos_in(int ticks);              /* master position `ticks` ahead */
int  mus_core_ticks_until(uint32_t pos);          /* ceil, >= 0                     */
/* re-key deck A's pitched voices in `mask` at pos (sting end, vinyl RESUME) */
void mus_core_catchup(uint32_t pos, uint8_t mask);
/* the scrub decoder (deck B's storage): the note of each pitched deck-A
 * voice sounding at master position P; returns the voices found */
typedef struct MusScrubNote { uint32_t start, end; uint8_t pitch, vel, inst, pad; } MusScrubNote;
uint8_t mus_core_scrub(uint32_t P, MusScrubNote out[MUS_A_VOICES]);
/* the B deck's owner (stingers, the scrub and the dual song share it) */
enum { BM_IDLE, BM_DUAL, BM_STING, BM_SCRUB };
int  mus_core_b_mode(void);
void mus_core_b_claim(int mode);
/* vinyl state shared by mus_core.c and mus_vinyl.c */
typedef struct MusVinylRt {
    uint32_t start, end;        /* master span of the pattern                    */
    uint32_t seed;
    int32_t  disp;              /* displacement so far, units Q8                 */
    uint32_t P;                 /* the playhead at `t`, master units             */
    uint16_t t;                 /* units of the pattern scrubbed so far          */
    uint8_t  pattern, after, live, on;
    uint8_t  key[MUS_A_VOICES]; /* the scrubbed note per voice (pitch, 0 = none) */
    uint32_t kstart[MUS_A_VOICES];
    uint8_t  next_cue, pad[3];
} MusVinylRt;
extern MusVinylRt mus_vy;
void mus_core_vinyl_done(void);                   /* the pattern ended: AFTER_*   */
/* a vinyl pattern from master position S (lanes, transitions); -1 refused */
int  mus_core_vinyl_pos(int pattern, uint32_t S, int source, int after);
/* lane commands of deck A: MTC_VINYL, MTC_BED */
void mus_core_lane_cmd(MusGen *g, int cmd, int a, uint32_t pos);

/* the legacy bridge (plat_weak_music.c, audio_seq.c): the classic tone of
 * a record, and the latest tone of voice ch */
void mus_bridge_tone(const DoomoVoice *v, DoomoToneState *out);
const DoomoToneState *mus_bridge_voice(int ch);
/* the four tones; the driver's bridge renderer (mus_drv.c) writes them
 * directly when the platform is the weak bridge */
extern DoomoToneState mus_bridge_tones[4];
void mus_drv_set_bridge(int on);

#endif
