/* mus_int.h - internals shared by the music engine's files (mus_*.c,
 * audio_seq.c) (owner: MU0 skeleton, MU1 maintainer; docs/design/music/
 * SPEC.md sec.4, 6). Not part of the contract: only music.h, mus_data.h
 * and platform.h are.
 *
 * M1 layout (docs/AUDIO.md has the RAM ledger):
 *   mus_lane.c   the arranger: lane tokens -> events, decks
 *   mus_core.c   the bank, music_update(), cue requests, the hash, the API
 *   mus_graph.c  the node walk (M1: linear, MCF_EXACT)
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
/* GBA: 40 (CF-19). PS1: 40 while the bridge runs the driver in-thread (M1,
 * a 2-tick look-ahead); MU3 raises it to 128 with the IRQ driver at M3 (D22). */
#define MUS_RING_A 40
#else
#define MUS_RING_A 128          /* native: the PS1 size, so T4 proves it (D22)  */
#endif
#define MUS_RING_B 32
#define MUS_DRV_VOICES 4        /* M1: the bridge's four (M2/M3: 16 GBA, 20 PS1) */
#define MUS_A_VOICES 4          /* deck A roles at M1 (classic: 4)               */
#define MUS_B_VOICES 4          /* deck B: the dual song (roles 0 and 3 sound)  */
#define MUS_WINDOWS 4           /* driver mute windows                           */
#define MUS_LOOKAHEAD_U 192     /* >= 1 bar of 4/4 ahead of the driver (D5)      */

/* ---- the bank (mus_core.c) ------------------------------------------------------------ */
typedef struct MusCueView {
    const uint8_t  *blob;
    const MusCue   *cue;
    const MusRole  *roles;
    const MusNode  *nodes;
    const MusEdge  *edges;
    uint16_t        len;
    uint8_t         id, kind;
} MusCueView;
const MusBank *mus_bank(void);
int  mus_bank_cue(int id, MusCueView *out);       /* 1 = found */
const MusInst *mus_bank_inst(int i);              /* never NULL (row 0 / a silent row) */
/* the same, cached (set by mus_core_init; hot paths) */
extern const MusInst *mus_inst_tab;
extern uint16_t mus_inst_n;
#define MUS_INST(i) ((unsigned)(i) < mus_inst_n ? &mus_inst_tab[i] : mus_bank_inst(i))
/* the classic tempo law: 1/256 units per 30 Hz tick (today's sequencer) */
int32_t mus_rate256(int bpm);
/* frames a note of `units` lasts (the classic note_ticks) */
int  mus_note_frames(int units, int32_t rate256);

/* ---- the arranger (mus_lane.c) ---------------------------------------------------------- */
enum { MGS_IDLE, MGS_PLAY, MGS_ENDED };
enum { MGF_ONCE = 1 };                            /* MusGen.mode */
enum { MVF_NOTE = 1, MVF_DIRTY = 2, MVF_DONE = 4, MVF_RET = 8 };   /* MusVoiceRt.flags */
typedef void (*MusEmitFn)(void *ctx, uint32_t pos_kind, uint32_t arg);
typedef struct MusSink { MusEmitFn emit; void *ctx; int room; } MusSink;
typedef struct MusGen {
    MusDeck     d;
    MusVoiceRt *v;              /* n_roles runtimes                              */
    MusCueView  cv;
    const uint8_t *pack;        /* the current node's pack                       */
    uint16_t    node_len;       /* units                                         */
    uint8_t     n_roles, max_roles, mode, deck;
    uint8_t     vmap[MUS_A_VOICES];   /* role -> driver voice, 0xFF = not played */
    int32_t     rate256;
    uint32_t    next;           /* master position of the next unwritten event   */
} MusGen;
/* start cue `cv` at master position `base`; vmap NULL = role i -> voice i */
void mus_gen_start(MusGen *g, const MusCueView *cv, uint32_t base, int32_t rate256,
                   int mode, const uint8_t *vmap);
void mus_gen_stop(MusGen *g);
/* write every event before master position `until` (while sink->room >= 4);
 * returns the position everything before which has been written */
uint32_t mus_gen_until(MusGen *g, uint32_t until, MusSink *sink);

/* ---- the driver (mus_drv.c) ------------------------------------------------------------------ */
typedef struct MusKitHit { uint16_t f0, f1; uint8_t vol, decay16, ticks_sweep, pad; } MusKitHit;
extern const MusKitHit mus_kit_classic[KH_COUNT];
#define MUS_INST_KIT0 0xF0      /* DoomoVoice.inst of a drum: 0xF0 + kit         */
void     mus_drv_reset(void);
int      mus_drv_room(int ring);
void     mus_drv_push(int ring, uint32_t pos_kind, uint32_t arg);
void     mus_drv_truncate(int ring);
void     mus_drv_cut(uint32_t voice_mask);
void     mus_drv_set_tempo(uint32_t upm_q16);
uint32_t mus_drv_rebase(void);                    /* drop the clock fraction -> clock */
uint32_t mus_drv_clock(void);
void     mus_drv_written(int ring, uint32_t pos);
uint8_t  mus_drv_take_marks(void);
uint32_t mus_drv_keyons(void);
uint32_t mus_drv_underruns(void);
void     mus_drv_set_master(int vol10);           /* OPT_MUSIC_VOL, 0..10        */
void     mus_drv_set_ctl(int mt_per_frame);       /* 8 = 30 Hz (bridge), 4 = 60 Hz */
void     mus_drv_fence(int on, uint32_t pos);
const DrvVoice *mus_drv_voice(int v);
void     mus_drv_run(int mt);                     /* the bridge: mt driver ticks   */
extern const uint8_t mus_vol63_of15[16];
int      mus_drv_ring_used(int ring);

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

/* the legacy bridge (plat_weak_music.c, audio_seq.c): the classic tone of
 * a record, and the latest tone of voice ch */
void mus_bridge_tone(const DoomoVoice *v, DoomoToneState *out);
const DoomoToneState *mus_bridge_voice(int ch);
/* the four tones; the driver's bridge renderer (mus_drv.c) writes them
 * directly when the platform is the weak bridge */
extern DoomoToneState mus_bridge_tones[4];
void mus_drv_set_bridge(int on);

#endif
