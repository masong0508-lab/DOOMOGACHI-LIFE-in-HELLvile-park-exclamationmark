/* mus_data.h - the music bank's binary formats, the event rings and the
 * platform-facing voice records (owner: MU0; maintainer MU1, add-only;
 * docs/design/music/SPEC.md sec.5, 9.4 and the critique fixes).
 *
 * THE BANK is one position-independent little-endian blob (offsets only):
 * `doomo_mus_bank[]` in core/gen/mus_data.c (GBA ROM, native), the same
 * bytes as MUSIC.BIN on the PS1 from M3. tools/mmlc.py writes it; nothing
 * edits it by hand. Every record below is read in place, so every size is
 * asserted at the end of this file (gcc, arm-none-eabi and mipsel agree:
 * only naturally aligned u8/u16/u32 fields).
 *
 *   MusBank header (52 B)
 *   directory      MusDirEnt[n_dir], sorted by (kind, id)  (binary search)
 *   instruments    MusInst[n_inst]
 *   comps, grooves, misc tables (M4)
 *   cue blobs      MusCue | [MusStingHdr] | MusRole[] | MusNode[] | MusEdge[]
 *                  | MusBind[] | common | node packs
 *
 * Units: 48 per quarter note (MUS_UPQ), i.e. 192 per 4/4 bar, the same
 * 1/192-whole unit as the classic MML dialect (audio_int.h). Master time
 * (event positions) counts these units mod 2^28.
 *
 * Integer only, no floats; C99. */
#ifndef DOOMO_MUS_DATA_H
#define DOOMO_MUS_DATA_H

#include "types.h"

#define MUS_UPQ          48u        /* units per quarter note                    */
#define MUS_MT_HZ        240u       /* driver ticks (mt) per second              */
#define MUS_POS_MASK     0x0FFFFFFFu
#define MUS_BANK_MAGIC   0x534Du    /* 'M','S' little endian                     */
#define MUS_BANK_VERSION 1u

/* ---- bank header --------------------------------------------------------- */
typedef struct MusBank {
    uint16_t magic, version, n_cues, n_stings, n_inst, n_comp, n_groove, n_fill,
             n_motif, n_vinyl, n_trans, n_reharm;
    uint32_t dir_off, inst_off, comp_off, groove_off,
             misc_off,      /* u32[8]: fills motifs vinyl trans reharm params+arcs turnarounds genspecs */
             total,         /* bytes in the bank                                 */
             fnv;           /* FNV-1a 32 of bytes [52, total)                    */
} MusBank;                                                            /* 52 */

/* Directory entry. The directory holds n_cues + n_stings entries sorted
 * by (kind, id) and is binary-searched (MU0 decision: a sparse id space
 * - classic cues live at MC 200.. - costs no empty entries). */
enum { MDK_CUE, MDK_STING, MDK_LEGACY, MDK_SFX };           /* MusDirEnt.kind  */
enum { MDF_HUB = 1, MDF_JUKEBOX = 2, MDF_GEN = 4 };          /* MusDirEnt.flags */
typedef struct MusDirEnt {
    uint32_t off;          /* cue blob, from the bank start                     */
    uint16_t len;          /* cue blob bytes                                    */
    uint8_t  id;           /* MC_* / STG_* id                                   */
    uint8_t  kind_flags;   /* kind << 6 | MDF_* (bits 0-5)                      */
} MusDirEnt;                                                           /* 8 */
#define MUS_DIR_KIND(e)  ((e)->kind_flags >> 6)
#define MUS_DIR_FLAGS(e) ((e)->kind_flags & 0x3F)

/* ---- cue blob ---------------------------------------------------------------- */
enum { MCF_LOOP = 1, MCF_ONCE = 2, MCF_EXACT = 4, MCF_LOGIC_CLOCK = 8, MCF_NO_QUOTES = 16,
       MCF_NO_VINYL = 32, MCF_DJ = 64, MCF_STREAM_OK = 128 };            /* MusCue.flags */
typedef struct MusCue {
    uint8_t n_roles /* <= 12 */, n_nodes /* <= 32 */, n_edges /* <= 96 */, n_binds /* <= 4 */,
            bpm,                 /* whole bpm (the classic tempo law, mus_int.h)       */
            key,                 /* tonic | mode << 4                                  */
            meter,               /* beats per bar                                      */
            swing, dial_home, dial_lo, dial_hi /* S0..S4 = 0,64,128,192,255 */,
            entry, reentry, tag, rest,
            flags,               /* MCF_*                                              */
            var_max, rest_policy, arc,
            fx;                  /* reverb 3 | depth 2 | gba echo 1 | crackle 1 | stem 1 */
    uint16_t common_off, common_len;
} MusCue;                                                             /* 24 */

enum { STM_KEY = 1, STM_DIAL = 2, STM_RESUME_GRID = 4 };
typedef struct MusStingHdr {
    uint16_t hit_units;
    uint8_t  duck_q8, prio, grid, mode /* STM_* */, gap_beats, pad;
} MusStingHdr;                                                         /* 8 */

enum { RK_LEAD, RK_COUNTER, RK_HARM, RK_ARP, RK_BASS, RK_KIT, RK_PERC, RK_PAD, RK_METAL,
       RK_CHOIR, RK_BED, RK_FX, RK_COUNT };                             /* MusRole.kind */
enum { RF_DUCK_DIALOG = 1, RF_ECHO = 2, RF_MODEMAP = 4, RF_SUNG = 8, RF_QUOTE = 16,
       RF_GHOSTABLE = 32, RF_STREAM = 64, RF_SIDECHAIN = 128 };        /* MusRole.flags */
typedef struct MusRole {
    uint8_t kind, inst, inst_hi,
            i_on,                /* enters at intensity i_on, leaves at i_on - 24       */
            prio;                /* 0-3 prio | 4-6 PSG pref | 7 share                  */
    int8_t  pan;                 /* -32..31, 0 = centre                                */
    uint8_t flags,               /* RF_*                                               */
            morph;               /* 0-1 rank | 2-6 VMASK | 7 TWIN                      */
} MusRole;                                                             /* 8 */

enum { MNF_ANCHOR = 1, MNF_INTRO = 2, MNF_TAG = 4, MNF_BREAKDOWN = 8, MNF_REENTRY = 16,
       MNF_TRANSIT = 32, MNF_CADENCE = 64, MNF_PICKUP = 128 };          /* MusNode.flags */
typedef struct MusNode {
    uint8_t  bars, flags, i_min, i_max, chord_in, chord_out, edge0, n_edge;
    uint16_t pack_off, pack_len;  /* from the cue blob start                           */
} MusNode;                                                            /* 12 */

enum { MEC_ALWAYS, MEC_I_GE, MEC_I_LT, MEC_DIAL_GE, MEC_DIAL_LT, MEC_PASS_GE, MEC_PARAM_GE,
       MEC_PARAM_LT, MEC_CHAPTER_GE, MEC_LIFT };                        /* MusEdge.cond */
typedef struct MusEdge { uint8_t to, w, cond, arg; } MusEdge;           /* 4 */
typedef struct MusBind { uint8_t param, w_q4; int8_t bias; uint8_t curve; } MusBind; /* 4 */

/* Node pack: u8 n_lanes, u8 flags, u16 lane_off[n_lanes] (from the pack
 * start, 0 = the role is silent in this node), then (unless MNP_NO_CHART)
 * u8 chord[bars * 2], then the lanes; with MNP_VARIANTS a MusVarDir
 * follows. Lane i belongs to role i. pack <= 1,024 B (base <= 640 with
 * variants), variant <= 384 B (mmlc checks). */
enum { MNP_PICKUP_LANES = 1, MNP_VARIANTS = 2, MNP_NO_CHART = 4 };
typedef struct MusNodePack { uint8_t n_lanes, flags; uint16_t lane_off[1]; } MusNodePack;
typedef struct MusVarDir { uint8_t n_var /* 2..8 */, n_fam /* 1..4 */; uint16_t role_mask, var_off[1]; } MusVarDir;
#define MUS_PACK_MAX     1024u
#define MUS_PACK_BASE    640u
#define MUS_VARIANT_MAX  384u

/* MusChord: 1 B per half bar. Low nibble = root above the key (12 same as
 * before, 13 N.C.), high nibble = quality. */
enum { MCH_MAJ, MCH_MIN, MCH_DIM, MCH_AUG, MCH_SUS2, MCH_SUS4, MCH_DOM7, MCH_MAJ7, MCH_MIN7,
       MCH_HDIM7, MCH_DIM7, MCH_MMAJ7, MCH_POWER, MCH_ADD9, MCH_MIN9, MCH_SIX };
#define MCH_SAME 12
#define MCH_NC   13

enum { MGT_SENTENCE, MGT_PERIOD, MGT_ANSWER, MGT_DEV, MGT_BUILD, MGT_TAG };
typedef struct MusGenSpec {
    uint8_t tmpl, motif, xform /* SEQ INV RETRO RHY CONT ORN */, cad /* PAC HC DC PLAGAL AEOL */;
    int8_t  shift[4];
    uint8_t energy, var_cap, n_var, flags /* TWIN COUNTER KEEP_ONSETS FIT_HEAD */;
} MusGenSpec;                                                         /* 12 */

/* ---- lane tokens (DG sec.3.3; the exact bytes are MU0's) -------------------------
 * A lane is a byte stream that sums to bars x meter x 48 units. Timed
 * tokens advance the lane clock; commands (one argument byte) take effect
 * at the current lane time. Lengths come from deflen or MUS_LEN_TAB; EXT
 * tokens lengthen the timed token just before them (so any length is a
 * short sum). A NOTE followed by TIE tokens is one tied note: its gate is
 * computed from its pieces like the classic sequencer does.               */
#define MUS_LEN_TAB_INIT { 3, 6, 8, 12, 16, 18, 24, 32, 36, 48, 72, 96, 144, 192, 288, 384 }
enum {
    MT_NOTE     = 0x00,   /* 00-5F  note BASE+n, deflen                              */
    MT_NOTE_L   = 0x60,   /* 60-6F  LEN_TAB[i]; next byte n                          */
    MT_REST     = 0x70,   /* 70-7F  LEN_TAB[i]; releases a sounding note             */
    MT_TIE      = 0x80,   /* 80-8F  LEN_TAB[i]; another piece of the note before     */
    MT_HIT      = 0x90,   /* 90-9F  drum hit 0..15 (KIT_*_HIT), deflen               */
    MT_TONE     = 0xA0,   /* A0-AF  reserved (14 = the chip chord, M4)               */
    MT_ACC      = 0xB0,   /* B0-BF  reserved (accents, M4)                           */
    MT_CMD      = 0xC0,   /* C0-D7  commands, one argument byte each                 */
    MT_GEN      = 0xD8,   /*        phase B                                          */
    MT_EXT      = 0xE0,   /* E0-EF  + LEN_TAB[i] units to the previous timed token   */
    MT_EXTS     = 0xF0,   /* F0-F7  + (n + 1) units to the previous timed token      */
    MT_HOLD     = 0xF8,   /*        time passes, nothing sounds or stops; deflen      */
    MT_END      = 0xFF
};
enum {  /* MT_CMD + c */
    MTC_DEFLEN, MTC_VOL /* 0..63 */, MTC_INST, MTC_GATE /* 0 inst, 1..8 eighths */,
    MTC_TRANS /* int8 */, MTC_BASE, MTC_PAN /* int8 */, MTC_GLIDE, MTC_TMB, MTC_ECHO,
    MTC_PROB, MTC_ALT, MTC_HEAD, MTC_QSLOT, MTC_COMP, MTC_DNA, MTC_FILL, MTC_CALL,
    MTC_RET, MTC_MARK, MTC_VMASK, MTC_VINYL, MTC_BED, MTC_KEYSH, MTC_COUNT
};

/* ---- shared banks ------------------------------------------------------------- */
enum { TIMBRE_PULSE, TIMBRE_PWM, TIMBRE_SAW, TIMBRE_SYNC_SAW, TIMBRE_TRI, TIMBRE_TRI4,
       TIMBRE_SINE, TIMBRE_WT_ORGAN, TIMBRE_WT_POWER5, TIMBRE_WT_VOWEL_A, TIMBRE_WT_VOWEL_O,
       TIMBRE_WT_VOWEL_E, TIMBRE_WT_GROWL, TIMBRE_WT_BELL, TIMBRE_WT_GROSS, TIMBRE_NOISE15,
       TIMBRE_NOISE7, TIMBRE_KIT_TOY, TIMBRE_KIT_BONES, TIMBRE_KIT_808, TIMBRE_KIT_PLUSH,
       TIMBRE_KIT_FORGE, TIMBRE_KIT_CLASSIC, TIMBRE_CHOIR_AAH, TIMBRE_CHOIR_OOH,
       TIMBRE_CHOP_BABBLE, TIMBRE_CRACKLE, TIMBRE_PIEZO, TIMBRE_STREAM, TIMBRE_COUNT };
#define TIMBRE_IS_KIT(t) ((t) >= TIMBRE_KIT_TOY && (t) <= TIMBRE_KIT_CLASSIC)
/* kits (DRUM event kit field, 3 bits) and the classic kit's hits */
enum { KIT_TOY, KIT_BONES, KIT_808, KIT_PLUSH, KIT_FORGE, KIT_CLASSIC, KIT_COUNT };
enum { KH_NONE, KH_KICK, KH_SNARE, KH_HAT, KH_OPEN, KH_CRASH, KH_TOM, KH_CLAP, KH_COUNT };
/* the classic arpeggio tables (MusInst.arp) */
enum { MARP_NONE, MARP_MAJOR, MARP_MINOR, MARP_OCTAVE, MARP_POWER, MARP_SUS4, MARP_DIM,
       MARP_MAJ7, MARP_TRILL, MARP_COUNT };

/* Instrument. The envelope runs once per control frame of the 30 Hz
 * classic law (mus_drv.c): level 0..240 (16 x 0..15), attack = frames to
 * the peak, decay / release = level drop per frame. */
enum { MIF_ENV30 = 1 };
typedef struct MusInst {
    uint8_t  timbre, timbre_alt,
             attack, decay, sustain /* 0..15 */, release, level /* 0..15 */, gate /* 1..8 */,
             vib_depth /* 1/16 st */, vib_speed /* phase per frame, 256 = cycle */, vib_delay, arp;
    int8_t   sweep;               /* pitch change per frame, 1/16 st                  */
    uint8_t  tparam,              /* PULSE: duty in 1/32                               */
             psg_env,             /* GBA PSG envelope byte (fitted by mmlc; 0 = soft)  */
             flags;               /* MIF_*                                             */
    uint16_t spu_adsr1, spu_adsr2;/* PS1 ADSR (fitted; 0 = soft)                        */
} MusInst;                                                            /* 20 */

typedef struct MusComp { uint8_t n_ev, flags, len_beats, pad; } MusComp;        /* 4 + 2/event */
typedef struct MusGroove { uint8_t kit, flags, steps, swing, hit[6], pad[6];
                           uint8_t prob[48]; /* 6 rows x 16 steps x 4 bit */ } MusGroove; /* 64 */
typedef struct MusFill { uint8_t n_hit, beats; } MusFill;                       /* 2 + 2/hit */
typedef struct MusMotif { uint8_t n_note, head, mode, pad; } MusMotif;          /* 4 + 2/note */
typedef struct MusVinyl { uint8_t n_seg, flags, beats_q4, pad; } MusVinyl;      /* 4 + 4/seg */
enum { MTT_CUT, MTT_CROSS, MTT_FILL, MTT_PICKUP, MTT_BRIDGE, MTT_PIVOT, MTT_VINYL, MTT_SILENCE, MTT_SYNC };
enum { MTF_TEMPO_RAMP = 1, MTF_KEEP_TEMPO = 2, MTF_DUCK = 4, MTF_FAST_MORPH = 8 };
typedef struct MusTrans { uint8_t from, to, grid, type, arg, entry /* 0xFE resume */, max_wait, flags; } MusTrans; /* 8 */
typedef struct MusParamDef { uint8_t rise, fall, def, flags; } MusParamDef;    /* 4 */

/* ---- events: rings A (deck A) and B (deck B, SFX deck, urgent) ------------------- */
/* pos = master units (bits 0-27) | kind << 28; arg packed per kind,
 * low bits first (MEV_* helpers below). */
typedef struct DoomoMusEv { uint32_t pos, arg; } DoomoMusEv;            /* 8 */
enum { MEV_NOTE, MEV_CHORD, MEV_DRUM, MEV_OFF, MEV_INST, MEV_PAN, MEV_GAIN, MEV_TEMPO,
       MEV_MARK, MEV_PITCH, MEV_MUTE, MEV_FX, MEV_KINDS };
#define MEV_POS(e)   ((e)->pos & MUS_POS_MASK)
#define MEV_KIND(e)  ((int)((e)->pos >> 28))
#define MEV_MAKE_POS(kind, pos) ((((uint32_t)(kind)) << 28) | ((uint32_t)(pos) & MUS_POS_MASK))
#define MEV_VOICE(a) ((int)((a) & 31u))
/* NOTE: voice 5 | pitch 7 | vel 6 | dur 11 (gate, control frames; 0 = hold) | flags 3 */
enum { MEVN_LEGATO = 1, MEVN_SLIDE = 2, MEVN_ACCENT = 4 };
#define MEV_NOTE_ARG(v, pitch, vel, dur, fl) ((uint32_t)(v) | ((uint32_t)(pitch) << 5) | \
    ((uint32_t)(vel) << 12) | ((uint32_t)(dur) << 18) | ((uint32_t)(fl) << 29))
#define MEV_NOTE_PITCH(a) ((int)(((a) >> 5) & 127u))
#define MEV_NOTE_VEL(a)   ((int)(((a) >> 12) & 63u))
#define MEV_NOTE_DUR(a)   ((int)(((a) >> 18) & 2047u))
#define MEV_NOTE_FLAGS(a) ((int)(((a) >> 29) & 7u))
#define MEV_DUR_MAX 2047
/* CHORD: voice 5 | root 7 | quality 4 | vel 6 | dur 10 (M4) */
/* DRUM: voice 5 | hit 3 | kit 3 | vel 6 | variant 4 | dur 11 */
#define MEV_DRUM_ARG(v, hit, kit, vel, var, dur) ((uint32_t)(v) | ((uint32_t)(hit) << 5) | \
    ((uint32_t)(kit) << 8) | ((uint32_t)(vel) << 11) | ((uint32_t)(var) << 17) | ((uint32_t)(dur) << 21))
#define MEV_DRUM_HIT(a) ((int)(((a) >> 5) & 7u))
#define MEV_DRUM_KIT(a) ((int)(((a) >> 8) & 7u))
#define MEV_DRUM_VEL(a) ((int)(((a) >> 11) & 63u))
/* OFF: voice 5 | mode 2 */
enum { MOFF_RELEASE, MOFF_CUT, MOFF_CUT_ALL };
#define MEV_OFF_ARG(v, mode) ((uint32_t)(v) | ((uint32_t)(mode) << 5))
#define MEV_OFF_MODE(a) ((int)(((a) >> 5) & 3u))
/* INST: voice 5 | inst 8 | tparam 8 | vel 6 (the channel volume, MU0 addition) */
#define MEV_INST_ARG(v, inst, tp, vel) ((uint32_t)(v) | ((uint32_t)(inst) << 5) | \
    ((uint32_t)(tp) << 13) | ((uint32_t)(vel) << 21))
#define MEV_INST_INST(a) ((int)(((a) >> 5) & 255u))
#define MEV_INST_TP(a)   ((int)(((a) >> 13) & 255u))
#define MEV_INST_VEL(a)  ((int)(((a) >> 21) & 63u))
/* PAN: voice 5 | pan 6 (signed -32..31) */
#define MEV_PAN_ARG(v, pan) ((uint32_t)(v) | (((uint32_t)(pan) & 63u) << 5))
/* GAIN: target 5 (voice, or bus 24-31) | gain 8 | ramp 12 (mt) */
#define MEV_GAIN_ARG(t, g, ramp) ((uint32_t)(t) | ((uint32_t)(g) << 5) | ((uint32_t)(ramp) << 13))
#define MEV_GAIN_GAIN(a) ((int)(((a) >> 5) & 255u))
#define MEV_GAIN_RAMP(a) ((int)(((a) >> 13) & 4095u))
/* TEMPO: upm_q16 24 | bend % 8 */
#define MEV_TEMPO_ARG(upm, bend) (((uint32_t)(upm) & 0xFFFFFFu) | ((uint32_t)(bend) << 24))
/* MARK: flags 8 | chord_pcs 12 | root 4 | key 4 */
enum { MMK_BEAT = 1, MMK_BAR = 2, MMK_PHRASE = 4, MMK_NODE = 8, MMK_MARKER = 16, MMK_CADENCE = 32,
       MMK_STING_HIT = 64, MMK_END = 128 /* VINYL_END / the end of a once cue */ };
#define MEV_MARK_ARG(fl, pcs, root, key) ((uint32_t)(fl) | ((uint32_t)(pcs) << 8) | \
    ((uint32_t)(root) << 20) | ((uint32_t)(key) << 24))
/* PITCH: voice 5 | offset int11 | glide 8 */
/* MUTE: role_mask 12 | deck 1 | len 15 units (a driver mute window from pos) */
#define MEV_MUTE_ARG(mask, deck, len) (((uint32_t)(mask) & 0xFFFu) | ((uint32_t)(deck) << 12) | \
    (((uint32_t)(len) & 0x7FFFu) << 13))
#define MEV_MUTE_MASK(a) ((int)((a) & 0xFFFu))
#define MEV_MUTE_DECK(a) ((int)(((a) >> 12) & 1u))
#define MEV_MUTE_LEN(a)  ((uint32_t)(((a) >> 13) & 0x7FFFu))
/* FX: fx 4 | a 12 | b 12 */
enum { MFX_VINYL_START, MFX_VINYL_LIVE, MFX_VINYL_END, MFX_ECHO, MFX_REVERB, MFX_FREEZE,
       MFX_CRACKLE, MFX_PSG_DUCK, MFX_STEMS };

/* ---- the platform's voice records (platform.h sec.9.4) --------------------------- */
/* One rendered voice, sent by mus_drv to plat_voice() on change.
 * Voices: 0-11 deck A, 12-15 deck B, 16-17 SFX deck, 18-19 beds, 20 babble,
 * 21 tape/stream. */
enum { DVF_KEYON = 1, DVF_KEYOFF = 2, DVF_CUT = 4, DVF_LEGATO = 8, DVF_REVERSE_ENV = 16,
       DVF_ECHO = 32, DVF_NOISE7 = 64, DVF_SWEEP_HW = 128 };
typedef struct DoomoVoice {
    uint16_t pitch16;      /* 1/16 semitones above C0, 0 = silent; kit: hit | kit << 3  */
    uint8_t  vol_l, vol_r; /* 0..63, every gain applied                                   */
    uint8_t  env;          /* soft envelope level 0..240 (HW-envelope renderers ignore)   */
    uint8_t  inst, tparam; /* bank instrument; timbre param (kit: frames since key-on)    */
    uint8_t  flags;        /* DVF_*                                                       */
    uint8_t  mt;           /* 0..3: mt offset inside the control frame                    */
    uint8_t  prio, pad[2];
} DoomoVoice;                                                          /* 12 */
#define MUS_VOICE_KIT(hit, kit) ((uint16_t)((hit) | ((kit) << 3)))
typedef struct DoomoVoicePlan { uint8_t voice, prio, timbre, psg_pref; } DoomoVoicePlan; /* 4 */
enum { AT_L, AT_LP, AT_M, AT_S, AT_SP, AT_SPU, AT_NATIVE,
       AT_BRIDGE /* the weak legacy bridge: plat_tone channels 0-3, core-driven 30 Hz */ };
enum { CAP_STEREO = 1, CAP_TAPE = 2, CAP_REVERB = 4, CAP_PMON = 8, CAP_ECHO = 16,
       CAP_STREAM = 32, CAP_HWENV = 64, CAP_DRV_IRQ = 128 /* the platform calls mus_drv_tick() */ };
typedef struct DoomoAudioCaps {
    uint8_t  tier, soft_voices, flags /* CAP_* */, latency_mt;
    uint16_t mix_rate, tape_rate;
    uint32_t tape_samples;
} DoomoAudioCaps;                                                      /* 12 */
typedef struct DoomoVoiceChange { uint32_t mt; uint8_t voice, pad[3]; DoomoVoice v; } DoomoVoiceChange; /* 20 */

/* ---- runtime records (mus_core.c / mus_drv.c; sizes are part of the RAM ledger) ---- */
typedef struct MusVoiceRt {
    uint16_t lane, ret, next_u, gate_u, bar_off;
    int16_t  pitch16, glide16;
    uint8_t  role, inst, base, deflen, vol, gate_q, accent, vmask, nx_pitch, nx_len;
    int8_t   trans;
    uint8_t  flags, var, bar_op;
} MusVoiceRt;                                                         /* 28 */
typedef struct MusDeck {
    uint8_t  cue, node, next_node, state, bar, beat, pass, vlevel, n_voices, key, dial, dial_target;
    uint16_t stems, stems_next, node_u, picks;    /* chapter = picks / 5            */
    uint32_t acc;
    uint16_t bpm_q4;
    int16_t  speed_q8;
    uint32_t seed, ghost_u;
    uint8_t  hist[8], passes[32], dwell, nonanchor, anchors, arc_i;
    /* MusVoiceRt v[n_voices] follows in the owner's storage */
} MusDeck;                                                            /* 80 */
typedef struct DrvVoice {
    int16_t  pitch16;      /* base pitch + accumulated sweep, 1/16 st                 */
    uint16_t gate;         /* control frames until release (0xFFFF hold)               */
    uint8_t  inst, vel, env, stage, arp_i, vib_t, flags, age, pan, gain, gain_to, chord_q;
} DrvVoice;                                                           /* 16 */

/* ---- size asserts (MU0 acceptance) ---------------------------------------------------- */
#define MUS_ASSERT_SIZE(T, n) typedef char mus_size_##T[(sizeof(T) == (n)) ? 1 : -1]
MUS_ASSERT_SIZE(MusBank, 52);
MUS_ASSERT_SIZE(MusDirEnt, 8);
MUS_ASSERT_SIZE(MusCue, 24);
MUS_ASSERT_SIZE(MusStingHdr, 8);
MUS_ASSERT_SIZE(MusRole, 8);
MUS_ASSERT_SIZE(MusNode, 12);
MUS_ASSERT_SIZE(MusEdge, 4);
MUS_ASSERT_SIZE(MusBind, 4);
MUS_ASSERT_SIZE(MusInst, 20);
MUS_ASSERT_SIZE(MusGroove, 64);
MUS_ASSERT_SIZE(MusTrans, 8);
MUS_ASSERT_SIZE(MusParamDef, 4);
MUS_ASSERT_SIZE(MusGenSpec, 12);
MUS_ASSERT_SIZE(DoomoMusEv, 8);
MUS_ASSERT_SIZE(DoomoVoice, 12);
MUS_ASSERT_SIZE(DoomoVoicePlan, 4);
MUS_ASSERT_SIZE(DoomoAudioCaps, 12);
MUS_ASSERT_SIZE(DoomoVoiceChange, 20);
MUS_ASSERT_SIZE(MusVoiceRt, 28);
MUS_ASSERT_SIZE(MusDeck, 80);
MUS_ASSERT_SIZE(DrvVoice, 16);

/* the bank (core/gen/mus_data.c, generated by tools/mmlc.py) */
extern const uint8_t doomo_mus_bank[];
extern const uint32_t doomo_mus_bank_size;

#endif
