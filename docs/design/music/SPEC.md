# SPEC: Dynamic Music, Chip Sound and Record Scratches (GBA + PS1)

DOOMOGACHI LIFE in HELLvile Park! This is the final music spec, v1, dated 2026-10-18, for parallel agents working in one shared tree. It was written read-only against branch `claude/nice-allen-gr6czm` and the build outputs of 19:02-19:04. Nothing in the repo was edited, built or git-touched.

**The asks (verbatim):** "Make good flowing dynamic music as well That's as non repetitive/monotonous as possible, while still looping" / "Make the audio chiptuney yet still limit pushing on gba" / "Complete with simulated record scratches" / "But still work on the p s one too".

| Ask | Answer | § |
|---|---|---|
| flowing, non-repetitive, looping | **phrase graphs** of authored nodes + stateless per-pass variation, comp/groove rotation, reharm, stems, the cute→metal dial, lifts, arcs; **build-time generated development nodes (GEN)**; a real-note repetition gate in CI | 6, 8, 15, 19 |
| chiptune that pushes the GBA | 4 PSG channels with every trick + an ARM soft mixer on both FIFOs (packed stereo, true PWM, sync saw, wavetables, PCM kits); a governor buys ≤ 11 more voices at 21 kHz from measured headroom; 3D never pays more than today | 10, 13 |
| record scratches | **vinyl**: DECK (scrubs the sequencer itself, everywhere), TAPE (dry re-render of the last 0.5 s, scrubbed), synthesised crackle and friction; 16 patterns as comic stings, synced transitions, DJ moments, Studio cells | 7, 17 |
| also on PS1 | one core, one composition; the whole SPU (24 voices, ADPCM chip banks, ADSR, PMON, NON, reverb with freeze, stereo, a score bookshelf in SPU RAM); plays through CD loads | 11, 12 |

**Precedence:**
1. the **Critique fixes** (CF-1 … CF-46) at the end of this file, then §2;
2. the rest of this SPEC;
3. `design_graph.md` ("DG", the backbone, judged 8.0/35.5);
4. `design_generative.md` + `proto/` ("DGen", the build-time composer and metric code);
5. `direction.md` ("DIR", musical intent);
6. `engine_map.md` ("EM", today's engine).

**Coordination:**
- `../story/SPEC.md` ("SS") is **binding** for ownership and the hunk rule (§6), the PS1 ledger (C5), overlays (C4, C6), the GBA ROM (C21) and fps (C22). Its S1 owns `beat.h`, `audio_fx.c`, `ui_dialog.c`, `cutscene.c` and `overworld*.c`; S2 owns `story_vm.h`, `scriptvm.c` and `textpack.py`.
- `../custom/SPEC.md` ("CS", final, 19:54) supersedes `design_hybrid.md`, so read every "CH" below as CS. Music needs CS P3's **SFX.BIN move** (**−43,413 B** net PS1 resident, CS §2), AU01-AU24 and `audio_bus_gain()`. CS cuts AU21. CS's "audio" team is the music packages (CF-1, CF-30).
- Story content is being written now. Music never edits `.dsc` files.

**Contents:** 1 What ships · 2 Decisions · 3 Gates · 4 Architecture · 5 Formats · 6 Runtime · 7 Vinyl · 8 Variation and metric · 9 API · 10 GBA · 11 PS1 · 12 Parity · 13 Instruments · 14 Motifs · 15 Cues · 16 Authoring · 17 Player songs and DJ · 18 Migration · 19 Tests · 20 Budgets · 21 Work packages · 22 Order · 23 Decisions and risks · Appendices A, B · **Critique fixes (CF-1 … CF-46)**

---

## 1. What ships

1. **Author what the player should hum; generate what should hide the loop.** Each `MC_*` cue is 6-12 nodes of 1-16 bars, with weighted edges, intensity stems and a transition matrix.
   - Melodies are literal lanes.
   - Harmony, bass and arps are comp patterns over a 1 B-per-half-bar chord chart.
   - Drums are groove DNA plus fills.
2. **GEN nodes** are written at build time by `tools/musgen` (the prototype composer, moved to the host) as 4-8 variants per slot. The runtime walks them as a no-repeat bag, at 0 B of runtime code.
3. **Every decision is a stateless hash** `mus_h(seed, stream, node·pass, role·bar·step)`. That is what makes resume, seeks, TAPE, tier skipping and the in-ROM determinism test work.
4. **Two-layer timing.** The 30 Hz arranger writes note events up to 1 bar ahead into two rings. `mus_drv` plays them at 240 Hz in the IRQ (60 Hz arps, ADSR, vibrato, PWM, slides, vinyl speed). Tempo holds at 8.7 fps; the PS1 plays through loads.
5. **Decks:** A (the cue), B (stingers, crossfades, fills, scratch re-keys, dual) and a 2-voice SFX deck.
6. **Vinyl:** DECK everywhere, TAPE in 2D, crackle everywhere, landed on the frame by the comic engine.
7. **GBA:** tier L's mixer idles like today's with no SFX and is cheaper with ≥ 1. The whole audio path (arranger + driver + mixer) costs about +0.1-0.5 % CPU over today with no SFX (and less than today with any SFX), gated per scene; "edge" scenes keep today's 30 Hz control cadence (CF-6). L+/M/S/S+ add 3/4/8/11 soft voices at 10.5/13.4/18.2/21.0 kHz, and a tier stays only while the *measured* drawn fps holds (CF-7). The ROM drum stem comes after I9.
8. **PS1:** 16 music voices + beds, babble, tape and SFX; pre-encoded banks; reverb per cue; a 4.6 KB paging window; KON batched at 240 Hz.
9. **Cost:**
   - GBA: **+2,320 B EWRAM** (≤ 2,350 B, CF-12), **+86 B IWRAM**, ROM ≤ 60 KB in phase 1 (floor +36 KB, CF-14);
   - PS1: **+13.5 KB resident with PS1 TAPE, or +9.1 KB DECK-only** (CF-15), after SFX.BIN or with an I9 allowance.
10. **The gate** (hubs, 60 min, 8 seeds × 3 scripts; the velocity-free signatures, fuzzy, recency, texture and sensitivity gates of CF-38):
    - full mix never repeats > 4 bars;
    - pitched content never repeats > 8 bars (ornament-blind);
    - the head every ≤ 32 bars;
    - cadences ≥ 95 % legal; seams 0.
11. **Compatibility.** The 28 songs import as exact classic cues (T1); `audio_music_mute/duck`, `audio_tapestop`, `audio_music_resume_on_beat` and `audio_music_dual` keep their signatures.
12. **Fixed on the way:**
    - `ASONG_ONCE`;
    - 30 Hz arps and GBA bursts;
    - PS1 slowdown, mono and load drones;
    - babble stealing the harmony;
    - PSG zips;
    - the GBA's 25 %-pulse "saw";
    - three drum implementations and two volume curves.

---

## 2. Binding decisions

| # | Decision (why) |
|---|---|
| D1 | `design_graph.md` is the base: formats (DG §3), hashing (§5.5), walk (§4.5), matrix (§4.7), GBA (§8), PS1 (§9), tests (§13). This SPEC changes only what it says. |
| D2 | The generative composer runs **at build time only** (`tools/musgen`). Runtime use is phase B, D21. (Its 18.6 KB of MIPS cannot fit the PS1.) |
| D3 | **Two-layer timing:** `DoomoMusEv` rings A (deck A) and B (deck B, SFX deck, urgent), consumed by `mus_drv` in the IRQ. This replaces DG §4.3-4.4's 2-4-tick queue. (139 ms GBA ticks; CD loads.) |
| D4 | **Never rewrite the past.** Only CUT truncates ring A, without regeneration. Everything else goes on ring B, including driver **mute windows** over deck-A roles. (No snapshot RAM.) |
| D5 | **Look-ahead:** ≥ 1 bar or until the ring is full. Ring A = 40 entries (ring B 32) on GBA, 128 on PS1. `music_prefill` before CD reads. During vinyl, ring A keeps its look-ahead, and ring B carries re-keys in windows of clamp(gap + 8, 16, 48) mt (CF-19). (Measured 12-70 events per bar.) |
| D6 | **DECK everywhere.** TAPE (on-demand dry re-render) only in GBA 2D scenes with `gba_big` free and in PS1 2D scenes. **No always-armed ring, ghost PSG or shadow render.** |
| D7 | GBA tier L adds no soft music voices in 3D (3D runs 8.7-12.9 fps). |
| D8 | **The ROM stem:** unpitched KIT/PERC; DMA restarts on beats; muted during DECK vinyl, bends > 2 % and tempo ramps (falls back to the PSG kit at the next beat); **only after I9's 8 MiB decision**. |
| D9 | The repetition gate runs on real note events (DGen §6.3, ported). `graphsim2` is a design aid. VIB±, TIMBRE, ECHO and velocity are not variation. |
| D10 | The anti-repeat guard runs only in musgen and the tests, **never at runtime**, so bars stay pure functions. |
| D11 | The code says **vinyl** (`music_vinyl`, `VINYL_*`); "scratch" already names memory buffers (EM 10.11). |
| D12 | The engine tick is `music_update()`: `static music_tick` exists in `overworld.c:724` and `levels.c:540`. |
| D13 | **AUD-SPLIT:** MU1, as CS's "audio" team and `audio.c`'s owner from G-A (CF-30), moves the sequencer and `audio_music*` verbatim into `audio_seq.c` (music-owned). `audio.c` keeps SFX, babble, buses and `audio_tick()`. |
| D14 | **Synth SFX** are S1's `SynthSeg` tables in `audio_fx.c` (about 0.3 KB, not MML strings; CF-3). `mmlc` imports them from their initialisers as `MDK_SFX` cues on the SFX deck (`MB_SFX`, outside mute windows). No caller changes: at **M3**, one S1 hunk makes `audio_sfx_synth()`'s body forward to `music_sfx_synth()` (−0.3 KB PS1). |
| D15 | Native links both sequencers until M5 (T1). **The GBA switches at M2 and the PS1 at M3, one PR each,** dropping `songs.o` and the interpreter. So interim RAM = final RAM. |
| D16 | PS1: SFX ADPCM stays at `0x02000` (CS P3) and ends below `0x0A000` (about 27.6 KB used; a P3 hunk). Music sits above it; reverb is reserved from `0x67FC0` (CF-16). |
| D17 | The house holds 15,488 of `gba_big`'s 17,408 B (SS C7). Audio uses only the 1,408 B **audio tail** there, for the M+ FIFO double buffers (CF-11). The overworld's tile cache uses 17,152 B, so the overworld stays mono (≤ L+). TAPE and echo fall back to DECK, the crackle loop and echo-by-repeat. |
| D18 | Exorcism, Concert/Studio play and DJ scoring use `MCF_LOGIC_CLOCK` (exact); everything else uses the real clock. |
| D19 | Comic sync is exact at 30 Hz logic. Slower GBA logic lands within the presenting tick. `BOP_MUS_SYNC` anchors visuals to *audible* music events. |
| D20 | Music reads CH getters in its own files. **AU21 is cut in CH**: the governor alone sets the tier (if AU21 returns, it only caps). |
| D21 | **Phase B** (runtime GEN lead + harmony, ≈ 7 KB MIPS) is optional after M8: both consoles at once, with ≥ 8 KB of PS1 slack and a listening win; off by default. |
| D22 | **The PS1 ring is 128 entries:** 1.5 s of the densest measured cue = 81 events. T4/T7 prove it; the fallback is 256. |
| D23 | The 9 story songs import as classic cues onto the bookshelf. The story ledger's "+5.5 KB resident MML" (SS C5) is booked unspent by I9. |
| D24 | `trailer/music.c` stays a host score and the motif canon. It never ships, and its `FR(n)` coupling is never copied. |

---

## 3. Gates and measured baselines (2026-10-18 19:02-19:04 builds; I9 re-measures at G0)

| Resource | Now (measured) | Music asks | Precondition / gate |
|---|---:|---|---|
| GBA ROM (4 MiB file) | **3,076,700 B** | phase 1 ≤ **60 KB** | SS C21 projects ≈ 4.07 MB with story and custom. Full kits, the full score and stems wait for **I9's 8 MiB decision**. |
| GBA EWRAM free | **7,104 B** (#77 targets 24 KB; CS: 19,773 B after customisation, 16,341 B after `HOME`) | **≤ +2,350 B** (CF-12) | free ≥ 12,352 B (the CS gate) after the merge and after H1's `HOME` (3,432 B), with #77 sign-off |
| GBA IWRAM | **6,320 B** for the 6 KB main stack (176 spare) | **+86 B** (mixer 1,378 B replaces 1,292 B) | main stack ≥ 6,208 B; rich kernels wait for #78's flash-to-stack move (−1,092 B) |
| PS1 below the stack top | **110,624 B** (image ends `0x801E4FD0`; `doomo_sfx_pcm_data` 47,796 B is still resident; CS P3 nets −43,413 B) | **+13.5 KB (TAPE) / +9.1 KB (DECK-only)** (CF-15) | ≥ 72 KB below the stack top after every merge (SS C5); M3 lands after CS P3's SFX.BIN move, or with an I9 allowance |
| PS1 overlay slot | 118,896 B (combat) | 0 | no group grows |
| GBA fps (speed pass 3: drawn fps / work per drawn tick) | title logo 21.8 / 34.4 ms · new island 23.4 / 35.8 · studio play 27.8 / 34.1 · apartments 30 / 33.4 · shop, roster, resident 21-27 / 35-41 · library 17.3 · creator 13.4 · minigames 12-14 · island hub 5.6-7.5 · pet, exorcism, concert, dialogue 6.6-8.9 / 90-120 · 3D 8.7-12.9 / 70-96 | — | T5: each scene ≥ its G0 fps − 0.3 in tier L; promoted scenes ≥ 20 (≥ 28 if protected); UI scenes at ≥ 20 stay ≥ 20 |
| PS1 fps | dialogue 21-22, overworld 29.3, exorcism 26.7 | — | no regression > 1 fps |

---

## 4. Architecture

```
game: audio_music(SONG) | music_set_cue/param/sting/vinyl/land | BOP_MUS_* | OP_MUS_* | {r}
core, 30 Hz, music_update():  mus_core (params→intensity, clocks, decks A/B/S, buses, windows, beat
  events, ring writer) · mus_graph (walk, GEN bags, chapters, matrix, stingers, rests, session) ·
  mus_lane (tokens, comp, DNA, fills, beds) · mus_var (ops, ALT, reharm, mode maps, quotes, twin) ·
  mus_vinyl (patterns, DECK re-parse, TAPE feed, rules) · mus_harm (chord_pcs/fit_deg guard)
        │ ring A (deck A)                         │ ring B (deck B, SFX deck, urgent)
mus_drv, 240 Hz in IRQ (GBA 4 mt/VBlank, PS1 root counter 2): master clock, dispatch, mute windows,
  60 Hz macros (ADSR, vibrato, arps, duty/PWM, slides), vinyl speed, buses → plat_voice() on change
        │                                   │                                  │
GBA audio.c + mixer.iwram.c          PS1 spu_music.c                     native plat_music_native.c
(PSG + ARM mixer, governor, TAPE)    (24 voices, reverb, bookshelf, TAPE) (hash + WAV via mus_synth)
```

| File | THUMB (ROM) | PS1 |
|---|---:|---|
| `mus_core.c` | 3.0 KB | resident |
| `mus_lane.c` | 2.5 KB | resident |
| `mus_var.c` (V3 ops pre-realised) | 1.5 KB | resident |
| `mus_graph.c` | 2.2 KB | resident |
| `mus_vinyl.c` | 1.0 KB | resident |
| `mus_drv.c` (≈ 600 lines) | 2.5 KB | resident |
| `mus_harm.c` | 0.4 KB | resident |
| `mus_bind.c` (song→cue map, scene profiles, `doomo_sfx_prio[]`, babble snap) | 0.6 KB | resident |
| `mus_synth.c` (reference synth: native WAV and hash; PS1 TAPE) | not on GBA | resident from M7 |
| `mus_song.c` (DoomoSong/ConcertSong → RAM cue, LIVE, chord inference) | 2.0 KB | resident from M6 (several groups play songs; SS C6) |
| `audio_seq.c` (wrappers; the interpreter until M5) | −4 KB at M5 | resident |
| `plat_weak_music.c` (the legacy bridge onto `plat_tone`) | 0.6 KB | dropped when overridden |

The resident core is ≈ **13.8 KB THUMB**, which is ≈ **17.9 KB MIPS -Os** (×1.30, between the measured 1.27 for `audio.o` and 1.40 for `song_player.o`). M1 re-measures it.

---

## 5. Data formats (exact; sizes asserted in `core/include/doomo/mus_data.h`)

The bank is one position-independent LE blob (offsets only):
- **GBA and native:** `const uint8_t doomo_mus_bank[]` in `core/gen/mus_data.c`;
- **PS1:** the same bytes as `MUSIC.BIN`, uploaded to the SPU-RAM bookshelf and paged (§11.3).

Ids go in the generated `core/gen/mus_ids.h`, locked per area (§16.1).

```c
typedef struct MusBank { uint16_t magic /*'MS'*/, version, n_cues, n_stings, n_inst, n_comp, n_groove, n_fill,
    n_motif, n_vinyl, n_trans, n_reharm; uint32_t dir_off, inst_off, comp_off, groove_off,
    misc_off /* u32[8]: fills motifs vinyl trans reharm params+arcs turnarounds genspecs */, total, fnv; } MusBank; /* 52 */
typedef struct MusDirEnt { uint32_t off; uint16_t len; uint8_t kind /*CUE STING LEGACY SFX*/, flags /*HUB JUKEBOX GEN*/; } MusDirEnt; /* 8 */
/* cue blob: MusCue | MusStingHdr | MusRole[] | MusNode[] | MusEdge[] | MusBind[] | common | packs */
typedef struct MusCue { uint8_t n_roles /*<=12*/, n_nodes /*<=32*/, n_edges /*<=96*/, n_binds /*<=4*/, bpm,
    key /* tonic | mode<<4 */, meter, swing, dial_home, dial_lo, dial_hi /* S0..S4 = 0,64,128,192,255 */,
    entry, reentry, tag, rest, flags /* LOOP ONCE EXACT LOGIC_CLOCK NO_QUOTES NO_VINYL DJ STREAM_OK */,
    var_max, rest_policy, arc, fx /* reverb 3 | depth 2 | gba echo 1 | crackle 1 | stem 1 */;
    uint16_t common_off, common_len; } MusCue;                                                   /* 24 */
typedef struct MusStingHdr { uint16_t hit_units; uint8_t duck_q8, prio, grid, mode /*KEY DIAL RESUME_GRID*/, gap_beats, pad; } MusStingHdr; /* 8 */
typedef struct MusRole { uint8_t kind /* LEAD COUNTER HARM ARP BASS KIT PERC PAD METAL CHOIR BED FX */, inst, inst_hi,
    i_on /* leave at i_on-24 */, prio /* 0-3 prio | 4-6 PSG pref | 7 share */; int8_t pan;
    uint8_t flags /* DUCK_DIALOG ECHO MODEMAP SUNG QUOTE GHOSTABLE STREAM SIDECHAIN */,
    morph /* 0-1 rank | 2-6 VMASK | 7 TWIN */; } MusRole;                                        /* 8 */
typedef struct MusNode { uint8_t bars, flags /* ANCHOR INTRO TAG BREAKDOWN REENTRY TRANSIT CADENCE PICKUP */,
    i_min, i_max, chord_in, chord_out, edge0, n_edge; uint16_t pack_off, pack_len; } MusNode;   /* 12 */
typedef struct MusEdge { uint8_t to, w, cond /* ALWAYS I_GE I_LT DIAL_GE DIAL_LT PASS_GE PARAM_GE PARAM_LT CHAPTER_GE LIFT */, arg; } MusEdge;
typedef struct MusBind { uint8_t param, w_q4; int8_t bias; uint8_t curve; } MusBind;          /* 4, 4 */
typedef struct MusNodePack { uint8_t n_lanes, flags /* PICKUP_LANES VARIANTS */; uint16_t lane_off[];
    /* then u8 chord[bars*2], lanes, [MusVarDir] */ } MusNodePack;   /* pack <= 1,024 B; base <= 640 with variants */
typedef struct MusVarDir { uint8_t n_var /*2..8*/, n_fam /*1..4*/; uint16_t role_mask, var_off[]; } MusVarDir;
/* variant <= 384 B: u8 vflags (CHART | family<<4); [chord[bars*2]]; u16 lane_off[popcount]; lanes */
typedef struct MusGenSpec { uint8_t tmpl /* SENTENCE PERIOD ANSWER DEV BUILD TAG */, motif,
    xform /* SEQ INV RETRO RHY CONT ORN */, cad /* PAC HC DC PLAGAL AEOL */; int8_t shift[4];
    uint8_t energy, var_cap, n_var, flags /* TWIN COUNTER KEEP_ONSETS FIT_HEAD */; } MusGenSpec; /* 12 */
```

**MusChord (1 B per half bar):** low nibble = root above the key (12 same, 13 N.C.); high nibble = MAJ MIN DIM AUG SUS2 SUS4 DOM7 MAJ7 MIN7 HDIM7 DIM7 MMAJ7 POWER ADD9 MIN9 SIX.

**Tokens:** DG §3.3 verbatim:

| Bytes | Tokens |
|---|---|
| `00-5F` | NOTE |
| `60` | NOTE_L |
| `70` | REST |
| `80` | TIE |
| `90-9F` | HIT |
| `A0` | TONE (14 = the chip chord) |
| `B0` | ACC |
| `C0-D7` | DEFLEN VOL INST GATE TRANS BASE PAN GLIDE TMB ECHO PROB ALT HEAD QSLOT COMP DNA FILL CALL RET MARK VMASK VINYL BED KEYSH |
| **`D8`** | **GEN (phase B)** |
| `FF` | END |

- `LEN_TAB` = 3 6 8 12 16 18 24 32 36 48 72 96 144 192 288 384.
- **Every lane of every node and variant sums to bars × meter × 48** (the compiler checks it).

**Shared banks** (DG §3.4):

| Bank | Size and fields |
|---|---|
| `MusInst` | 20 B: timbre/alt, ADSR, level, vibrato, arp, sweep, gate, tparam, `psg_env` and `spu_adsr1/2` (fitted) |
| `MusComp` | 4 + 2 B per event |
| `MusGroove` | 64 B: 6 × 16 steps × 4-bit probability |
| `MusFill` | 2 + 2 B per hit |
| `MusMotif` | 4 + 2 B per note |
| `MusVinyl` | 4 + 4 B per segment |
| `MusTrans` | 8 B: from, to, grid, type (CUT CROSS FILL PICKUP BRIDGE PIVOT VINYL SILENCE SYNC), arg, entry (0xFE resume), max_wait, flags (TEMPO_RAMP KEEP_TEMPO DUCK FAST_MORPH) |
| `MusParamDef` | 4 B × 32 |
| misc | reharm 240 B, arcs 64 B, arp tables 128 B, turnarounds ≈ 400 B, cadence bitmaps 18 B per family |

**Timbres (28):** PULSE PWM SAW SYNC_SAW TRI TRI4 SINE WT_ORGAN WT_POWER5 WT_VOWEL_A/O/E WT_GROWL WT_BELL WT_GROSS NOISE15 NOISE7 KIT_TOY/BONES/808/PLUSH/FORGE/CLASSIC CHOIR_AAH/OOH CHOP_BABBLE CRACKLE PIEZO STREAM.

```c
typedef struct MusVoiceRt { uint16_t lane, ret, next_u, gate_u, bar_off; int16_t pitch16, glide16;
    uint8_t role, inst, base, deflen, vol, gate_q, accent, vmask, nx_pitch, nx_len; int8_t trans;
    uint8_t flags, var, bar_op; } MusVoiceRt;                                                     /* 28 */
typedef struct MusDeck { uint8_t cue, node, next_node, state, bar, beat, pass, vlevel, n_voices, key, dial, dial_target;
    uint16_t stems, stems_next, node_u, picks /* chapter = picks/5 */; uint32_t acc; uint16_t bpm_q4; int16_t speed_q8;
    uint32_t seed, ghost_u; uint8_t hist[8], passes[32], dwell, nonanchor, anchors, arc_i; MusVoiceRt v[]; } MusDeck; /* 80 + 28/voice */
typedef struct DoomoMusEv { uint32_t pos /* 0-27 MASTER units mod 2^28 | kind<<28 */, arg; } DoomoMusEv;     /* 8 */
typedef struct DoomoVoice { uint16_t pitch16 /* 0 silent; kit: hit|kit<<3 */; uint8_t vol_l, vol_r /* 0..63 */,
    env /* soft env, ignored by HW-envelope renderers */, inst, tparam,
    flags /* KEYON KEYOFF CUT LEGATO REVERSE_ENV ECHO NOISE7 SWEEP_HW */, mt /* 0..3 */, prio, pad[2]; } DoomoVoice; /* 12 */
typedef struct DoomoVoicePlan { uint8_t voice, prio, timbre, psg_pref; } DoomoVoicePlan;                          /* 4 */
typedef struct DoomoAudioCaps { uint8_t tier /* L LP M S SP SPU NATIVE */, soft_voices,
    flags /* STEREO TAPE REVERB PMON ECHO STREAM HWENV */, latency_mt; uint16_t mix_rate, tape_rate; uint32_t tape_samples; } DoomoAudioCaps; /* 12 */
typedef struct DrvVoice { int16_t pitch16, glide_to; uint8_t inst, vel, env, stage, arp_i, arp_t, vib_t, chord_q, pan, gain, flags, age; } DrvVoice; /* 16 */
```

**Event kinds** (`arg`, bits low → high):

| Kind | Fields |
|---|---|
| NOTE | voice 5 · pitch 7 · vel 6 · dur 11 · flags 3 (LEGATO SLIDE ACCENT) |
| CHORD | voice 5 · root 7 · quality 4 · vel 6 · dur 10 (the driver arps it at 60 Hz) |
| DRUM | voice 5 · hit 3 · kit 3 · vel 6 · variant 4 · dur 11 |
| OFF | voice 5 · mode 2 |
| INST | voice 5 · inst 8 · tparam 8 |
| PAN | voice 5 · pan 6 |
| GAIN | target 5 (voice or bus 24-31) · gain 8 · ramp 12 |
| TEMPO | upm_q16 24 · bend% 8 |
| MARK | flags 8 (BEAT BAR PHRASE NODE MARKER CADENCE STING_HIT VINYL_END) · chord_pcs 12 · root 4 · key 4 |
| PITCH | voice 5 · offset int11 · glide 8 |
| **MUTE** | role_mask 12 · deck 1 · len 15 units (a driver mute window) |
| FX | fx 4 (VINYL_START/LIVE/END ECHO REVERB FREEZE CRACKLE PSG_DUCK STEMS) · a 12 · b 12 |

**One master clock.** All events are stamped in master units (deck A's ghost timeline at its tempo).
- The platter never moves it; vinyl only scales deck-A pitch and gain and moves the re-parse playhead.
- Deck B and the SFX deck convert their positions through deck A's already-generated tempo map.
- 120 bpm with no cue. Bends and the LOGIC fence apply to all.
- Each ring is nondecreasing; the driver merges the two heads.

| RAM | GBA | PS1 / native |
|---|---:|---:|
| core state: deck A 416 + B 192 + SFX deck 136 + session (2 cues) 92 + params 64 + windows, buses, vinyl, sting, quotes, beat ring 8 × 12, callbacks, babble log 222 | 1,122 | 1,122 |
| ring A / ring B | 40 / 32 entries (576 B; CF-19) | 128 / 32 (1,280 B) |
| driver voices | 16 × 16 | 20 × 16 |

**Voices:** 0-11 deck A, 12-15 deck B, 16-17 SFX deck, 18-19 beds, 20 babble (direct), 21 tape/stream.

**Score size:**
- `MC_HAHALAND`: 48 authored bars = 1.55 KB; + 3 GEN × 8 variants × 150 B ≈ 5.2 KB.
- **Whole game ≈ 175 KB:** hubs 18, Breach 21, bosses 21, activities 30, cutscenes 15, ≈ 70 stingers 8.4, classic 6.4, SFX 1.7, banks 10.3, GEN 43.
- **Wave 1 ≈ 30 KB.**

---

## 6. Runtime

### 6.1 Clocks

- **Units.** 48 per quarter; mt = 1/240 s; 8 mt = 1 tick; 4 mt = 1 control frame.
- **Accumulator.** The driver adds `upm_q16 = bpm_q4 × 65,536 × 48 / (16 × 14,400)` per mt (× any bend), integer only.
- **REAL** (default): GBA 4 mt per vblank (238.9 Hz, sample-locked); PS1 240.0 Hz (root counter 2). Tempo holds when logic slows. The GBA runs 0.45 % slow against the PS1, which is presentation only.
- **LOGIC** (`MCF_LOGIC_CLOCK`): `fence = pos_at(tick + 1)`; the driver stops there.
- **Native:** 8 driver calls per tick.
- **Audible position** = `drv_pos − latency_mt` (GBA 4, PS1 ≈ 1).

### 6.2 `music_update()` (from `audio_tick()`) and the driver

1. Params (rise/fall), intensity (binds → arc → hysteresis → `stems_next`), windows.
2. Read `drv_pos` and the driver's MARKs → resident callbacks + the poll ring.
3. Resolve requests to grid positions (§6.5). **Urgent ones go on ring B.**
4. **Fill ring A** to `drv_pos` + look-ahead (1 bar or a full ring, or the prefill target; vinyl does not shorten it, CF-19), ≤ 2 bars of decode per call.
5. Fill ring B (deck B and the SFX deck: ≤ 1 beat; vinyl re-keys in windows of clamp(gap + 8, 16, 48) mt; live DJ input 16 mt; CF-19).
6. **At bar lines:** stem commits (4-bar dwell), a dial rank step, quotes, the hashed comp/groove/fill/`bar_op` choices. **At a node's last bar:** pick `next_node` (the PS1 prefetches its page; lane setup is spread over the bar).
7. **Never rewrite the past:** CUT truncates ring A (IRQ masked ≈ 30 cycles); everything else adds ring-B events, including `MEV_MUTE`.

**Driver, per mt:**
- advance the clock or stop at the fence;
- dispatch due events from both rings, honouring mute windows;
- every 4th mt, per voice: ADSR (soft voices), delayed vibrato, arp step, duty/PWM, slide, vinyl speed (Δpitch 192·log2|v|, gain min(1, 2|v|)), buses;
- `plat_voice()` only on change.

Cost ≈ 40 c/mt + 60 c/event + 50 c per changed voice, **plus about 120-200 c per modulated voice per control frame** (THUMB from ROM; this term was missing). **Gate ≤ 1.0 % GBA CPU** (CF-6).

**Long gaps and blocking I/O:**

| Situation | Behaviour |
|---|---|
| GBA tick gaps ≤ 139 ms | ring A holds ≥ 1 s of the densest metal (T4: no under-run in 30 min at 15 Hz logic) |
| PS1 CD load | `music_prefill(1500)` before `CdRead`; the IRQ plays through `CdReadSync` |
| memory card / GBA flash | `STALL_CARD` → `audio_hw_hush()`; `STALL_SAVE` → a 4-frame PSG fade and DMA stop; both resume on the grid |
| ring full / empty | pause generation / hold state (T4 fails on any under-run) |

### 6.3 Walk (DG §4.5) and GEN bags

Candidates are edges whose `cond` holds and whose target's [i_min, i_max] contains the intensity. Then:
1. no A→A (A→A' is fine);
2. **soft recency:** last 2 picks w/4, last 3 w/2 (hard zeroing locks graphs: 1,834 of 1,860 bars measured);
3. **bag:** unheard for 2 × n picks → w × 4;
4. **anchors:** after 2 non-anchors only anchors; never 3 anchors in a row;
5. **cadence** legality by family bitmap, else a 1-bar turnaround from the table (V7, ii-V, ♭VI-♭VII-I, chromatic) and a re-pick with rule 2 relaxed;
6. **arc** bias when params are static for 8 bars.

The pick is `mus_h(seed, S_WALK, node, picks) % Σw`.

**GEN:** family = `pool_value(e)` (§8.4); variant = `bag(mus_h(seed, S_GEN, node, pass/n), pass % n)` (a hashed Fisher-Yates over ≤ 8, with the epoch-boundary repeat swapped).

### 6.4 Intensity, stems, dial, journeys

- **Intensity:** `clamp(Σ curve·w >> 4 + bias)` → arc → hysteresis (`i_on` / `i_on − 24`) → bar commits with a 4-bar dwell.
- **`MPF_SPIKE`** (danger) acts at the next beat: ring B carries the spike fill + `MEV_MUTE` KIT + catch-up events for the entering roles (re-derived statelessly); ring A carries the new stems from the next bar.
- **Fades:** PSG by velocity and density; soft and SPU voices by `MEV_GAIN` (PS1 hardware sweep).
- **Dial:** one rank per bar, drums → bass → harmony → lead (4 bars from cute to metal). `MTF_FAST_MORPH` = all ranks on one downbeat behind NEEDLE + a silence.
- **Journeys:** `KEYSH`/`LIFT` give +1/+2 at most twice, then pivot home; tempo ramps ±8 % per phrase; combat prefers half/double-time grooves.

### 6.5 Transitions, stingers, rests, beds

Grids NOW 16TH BEAT BAR PHRASE NODE_END MARKER, with `max_wait` falling back to a finer grid. Matrix lookup: (from, to) → (from, \*) → (\*, to) → (\*, \*) by binary search.

| Type | Mechanics |
|---|---|
| CUT | truncate ring A at the target; the new cue starts there |
| FILL | ring B: a fill over the last 1-2 beats + mute the old KIT; then CUT + crash |
| PICKUP | ring B: the entry pickup from beat (meter − pickup); CUT at the bar |
| CROSS n | the new cue on deck B from the next bar; deck A's stems ramp down by priority; swap at a bar (truncate A) |
| BRIDGE / PIVOT | a TRANSIT node / the E-E-G node, with the key gliding |
| VINYL | §7 |
| SILENCE b | `MEV_MUTE` b beats, then a downbeat |
| SYNC | deck B joins at deck A's bar and beat, then swap |

- **Resume, not restart:** `entry 0xFE` restores node/passes/hist from session memory (2 cues) at the next phrase or REENTRY; INTROs heard within 10 min are skipped.
- **Stingers:** the return value = ticks until the hit; pre-rolled by `hit_units`; one at a time on deck B; a lower prio never cuts a higher; `STM_KEY`/`STM_DIAL`; deck A returns on the beat after `gap_beats`; **`MCF_ONCE` is honoured** (EM fact 6).
- **Rests:** after `rest_policy` minutes, the rest node or RESTING for 16-32 bars (never in combat); idle 20 s → −1 level; 90 s → rest.
- **Beds:** seeded pentatonic emitters gated by hour, weather and zone (birds, crickets, wind, rain, river, lava, vape, crowd, clock, machinery, embers, fireworks). GBA: PSG noise while the music rests, else the lowest soft voice. PS1: voices 16-17.

### 6.6 Cost (GBA THUMB from ROM; T5 checks)

- **Per tick:** params 550 + events 1,000 + bar decisions 100 + beat events 200 = **≈ 2.0k typical** (today 2.5-3k).
- **Node switch:** ≤ 2k peak, split over 2 ticks.
- **DECK re-parse:** +3.6k per control frame (≈ 1.3 %), only during patterns.
- **Gates:** ≤ 6k average, ≤ 12k peak. PS1: arranger 0.3 % + IRQ 0.3 %.

---

## 7. Vinyl: the record-scratch DSP (both consoles)

### 7.1 The turntable and its three sources

**State:** playhead P; platter `speed_q8` (256 = 1.0, negative = backwards); fader (open, closed, chirp, gate32); `ghost_u` = the never-stopping master clock; friction and crackle density.

**Patterns:** 4 B segments in beats, each v0→v1 with a curve (lin, ease, sine, exp), a fader action and seeded 0-15 % hand jitter.

**`mus_vinyl_speed(pattern, t, seed)`** runs identically in the driver (pitch and gain per control frame) and the arranger (where P is), so a pattern needs only one `MEV_FX VINYL_START`. Live DJ input writes a volatile `live_speed_q8` + fader.

**Rendering:**
- +192·log2|v| (1/16 st);
- gain min(1, 2|v|) × fader; silent below 1/16;
- highs cut below 0.5 (GBA one-pole + PSG duty 50 %; PS1 muffled bank).

| Source | How | Where | Cost |
|---|---|---|---|
| **DECK**: scrubs the sequencer itself | The driver applies the speed to all deck-A voices without retriggering. On each arranger tick, `mus_vinyl` re-parses each audible role's bar from `bar_off` to P (stateless given the bar log, CF-20). Whenever the sounding note changes, it posts re-keys on ring B for the next window (CF-19). Only the 4 highest-priority pitched roles re-key; the others only bend. Ring A is muted over the span. Backwards, earlier notes return in reverse, with short-attack re-keys. | **everywhere, 3D included** | GBA ≈ 1.3 % while scratching, PS1 ≈ 0.6 %; 0 B |
| **TAPE**: the real recent music (exact PSG parts) + the babble log ("scratch the singer") | With ≥ 3 ticks of pre-roll, deck B seeks statelessly to P − `tape_q` and runs **dry** through a dry driver into `plat_tape_render()` over the pre-roll. From P, `plat_tape_play(speed, pos, gain)` runs each control frame. Deck A is muted, or DECK at −9 dB underneath. | GBA 2D with `gba_big` free; PS1 2D scenes | GBA ≈ 200k cycles once + 0.5 % playback, 4 KB borrowed. PS1: `mus_synth` 8 kHz + streaming ADPCM forward and reversed ≈ 26 ms, 8 KB SPU RAM, voice 19 (a direction change re-keys in the other copy). |
| **CRACKLE** | Poisson clicks at 2-60/s (1 in 40 a big pop), hiss, a friction **zip** on every direction change, drag noise while \|v−1\| > 0.3, NEEDLE groove clicks + a pop, a 33⅓ rpm tick | everywhere | GBA: an 8-bit loop through the PCM kernel in every tier (synth in the rich set). PS1: loop + NON (voice 17) + zips. |

**No live mix ring.**
- The GBA PSG bypasses the mixer, so a ring needs a ghost PSG (0.6-1.2 % always on).
- The PS1 needs a 6.5 % shadow render.
- SPU capture sees only CD audio and voices 1 and 3 (11.6 ms rings).

The re-render costs **0 until a scratch fires** and is identical on both consoles. SFX are not taped: the world keeps playing while the music is scratched.

**Source choice.** `VSRC_AUTO` = TAPE if `CAP_TAPE` and `MVF_TAPE`, else DECK. Combat, 3D, exorcism and the GBA house use DECK + crackle.

**After a pattern:**
- **RESUME** (default): jump to `ghost_u`; the following ring-A events are still valid.
- **CONTINUE** (REWIND): a stateless re-seek (≤ 30k cycles once), truncate ring A, regenerate.
- **STOP:** mute until `music_resume_on_beat()`; generation runs underneath.
- **NEXT_CUE:** `MT_VINYL`.

### 7.2 Patterns (16; ≈ 320 B)

| Id | Length | Platter path (1.0 = play) | Fader | Use |
|---|---|---|---|---|
| BABY | ½ beat | 0→+2.4→0→−2.4→0 sine | open | basics, quick stings |
| FORWARD | ¼ | 0→+3→0, back stroke | open, then closed | punctuation |
| CHIRP | ¼ | ±2.6 | chirp | cute gags, the pet |
| TRANSFORMER | 1 | ±1.2 | gate32 | robots, Rivet, the factory |
| TEAR | ½ | two humps forward, one back | open | rips |
| FLARE | ½ | baby | 1/64 click | flair |
| SCRIBBLE | ½ | ±1.3 every 1/48 | open | nervous "brrrr" |
| WIKKA | 1 | 4 rising babies | open | hype |
| REWIND | 1 + snap | +1→−4 exp; P 1-2 bars back; +1 on the downbeat | open | transitions, retries, flashbacks |
| SPINBACK | 1 bar | −6 decaying | open | boss phases, endings |
| STOP | ½ | +1→0 exp (≈ −2 octaves) | open, then silence | deaths, freezes |
| NEEDLE | ¼ | +1→+3→0 + clicks + pop | open, then silence | **"wait, WHAT?"** |
| STUTTER | ½ | back 1/16 × 4 | open | spit takes, coughs |
| WOBBLE | continuous | 1 + 0.06·sin(1 Hz) + drift | open | haunted, dreams, the sick pet 3 % |
| SLOWMO | n beats | 1→0.5, hold, →1 | open | slow motion |
| TAPESTOP | n frames | to 2^(−15/12), then mute | open, then silence | `audio_tapestop(n)` exactly |

### 7.3 Uses and house rules

| Use | Mechanism |
|---|---|
| **Comic** | RECORD_SCRATCH = NEEDLE + STOP; FREEZE = STOP on the frame + reverb/echo freeze; SLOWMO/REWIND/STUTTER |
| **Dialogue** | `{r}`/`[SCRATCH]` = NEEDLE, resume 14 frames later (trailer s08) |
| **Menus** ("wait, WHAT?") | Monster Mode confirms, the first Lay to Rest, deleting a save, the title's idle corruption |
| **Deaths** | STOP → silence → the defeat sting |
| **Synced transitions** | invasion NEEDLE + silence + FAST_MORPH; boss SPINBACK; Boss Rush and retries REWIND; recaps REWIND → `MC_FLASHBACK`; cliffhangers STOP; house flips REWIND; the border = TAPESTOP 5 + 14 frames of silence + a downbeat |
| **Diegetic** | the radio (crackle + tick), dreams and the haunted house (WOBBLE), credits fast-forward 1.5× |
| **DJ** | the Studio lane (§17), the booth and battles, the Jukebox "Scratch it!", generated DJ moments (§8.3) |
| **Gentle audio** | fades, capped stingers |

**House rules** (`mus_vinyl.c`): ≤ 1 sting per bar; ≤ 1 vinyl per 8 bars outside `MCF_DJ`; "beep." only after ≥ 1 beat of silence. Refusals return −1.

---

## 8. Variation and the non-repetition metric

**The loop is the grammar, not the audio.** Every op is an O(1) hashed filter.

### 8.1 Melodic ops

**Never on:** `HEAD` blocks, an anchor's first bar, motif heads, V0.

| Level | Ops († = onset-safe; *surface* = ignored by M2) |
|---|---|
| V1 | GRACE · SLIDE_IN†/FALL† · VIB†* · ECHO†* · TIMBRE†* (the `inst_hi` sibling) |
| V2 | ANTICIPATE†/LAYBACK† · DOTSWAP†/SPLIT/MERGE · OCTAVE† (never > 10th) · MORDENT/TURN/PASSING |
| V3 (pre-realised as ALT bars by mmlc, L1) | TAIL_INVERT† RETRO† SEQUENCE† TRUNCATE CALL_RESPONSE |
| any | ALT (no immediate repeat) · QUOTE (QSLOT, ×2/×½) · DROOP† (p = INFECT/255) |
| dial ≥ S3 | **TWIN†**: COUNTER = a diatonic 3rd under LEAD, same rhythm, `fit_deg` refit (GBA SQ1 25 % vs SQ2 75 %; PS1 2 voices ±6 c panned) |

**Guards:** strong beats stay chord tones (`chord_pcs`/`fit_deg`); scale snap; role ranges; ≤ 1 op per role per bar (≤ 2 per 4 bars at V3); `RF_SUNG` gets † only.

| Pass | Level | Op chance | Comp/bass | Reharm | Drums |
|---|---|---:|---|---|---|
| 0 | V0 | 0 % | first pattern | — | core steps |
| 1 | V1 | 25 % | rotate | — | full DNA |
| 2-3 | V2 | 35 % | rotate | 20 % per half bar | + ghosts |
| 4+ | V3 | 45 % | rotate | 20 % | + ghosts |

`var_max` caps the level (bosses V2); `MCF_EXACT` forces V0 and a linear walk.

### 8.2 Harmony, bass, drums

- **Comp:** rotation per pass with no repeat (pools per band: ARP STAB ALBERTI PAD CHARLESTON chip-chord); voicing = the nearest inversion.
- **Reharm** (V2+, 20 % per half bar): from `pool[band][strong-beat degree]` (DIR §5.3), checked by `fit_deg` (9/13 allowed in S0-S1, none in metal); never on an anchor's first bar or a cadence's last; a clashing literal lane cancels it.
- **Mode maps:** major → natural/harmonic minor; hellify (♭5 on the peak); redeem (G→G♯, B♭→B by TRUST).
- **Bass:** root, root-5, octave, walk + chromatic approach, pedal, Charleston, gallop, chugs.
- **Drums:** a hit iff `mus_h(seed, S_DRUM, node·pass, role·bar·step) % 15 < p` (V0 = the p = 15 core); accents ± 1; ghosts at I3+; fills sized by the intensity jump with no repeat; crash only after a fill or rise; ±1 mt microtiming; round robin (GBA divider × 3, PS1 2-3 samples ±20 c).

### 8.3 GEN nodes (phase A, build time)

- **Hubs** = 6-8 authored nodes + **2-3 GEN slots** (C development, B answer, BUILD).
- **musgen** realises each slot from the motifs (heads protected), the anchor chart and its `MusGenSpec` (DGen §4.4.2-4.4.4):
  - sentence/period plans; ideas STATE VAR SEQ FRAG FREE CAD; transforms INV RETRO RHY CONT ORN with leap and range guards;
  - cadence-first T/S/D harmony with applied dominants and dial colour; the `fit_deg` melody-first refit; answers sharing the theme rhythm.
- **Output:** 8 variants (4 in phase 1) in families. HARM, ARP, BASS and KIT stay graph pools and DNA.
- **Offline guard (D10):** reject variants whose 4-bar M2 windows collide with another variant, the authored nodes or another family.
- **Curation:** the committed `gen/<area>/*.dms` **is the source**. Noodling is deleted by ear; the knobs are `var_max`, weights and the spec.
- **DJ moments** (LIVE songs, DJ cues): VINYL on the last beat of 4-bar lines in BRK/BUILD by the genre's `fill_rate`; ≤ 1 per 8 bars elsewhere.

### 8.4 Chapters, pools, quotes, seeds (all stateless)

- **Chapter** = picks / 5. **Pools:** P_ANSWER, P_DEV, P_PROG, P_GROOVE, P_COMP (groove and comp pick a max(2, n/2) window inside the pool).
  - `pool_value(e) = mus_h(seed, S_POOL, e, (chapter + e)/5) % n`: **exactly one element rotates per chapter**, so a session's B and C return before they drift.
- **Lifts:** from chapter 2, LIFT is live if `mus_h(seed, S_LIFT, chapter) % 100 < 35`: +1/+2 on PEAK/A', home the next chapter.
- **Arcs:** DG §5.4, for picking only.
- **Quotes:**
  - priority: story beat > speaker > quest giver > proximity > pet;
  - cooldowns: 1 per 4 bars, the same motif per 16;
  - infection: DROOP + detune INFECT/16 c + a ♭5 pedal at ≥ 192;
  - hot/cold: TENSION detunes a HARM voice ≤ 40 c;
  - TRUE NAMEs: 4 degrees of E harmonic minor from the resident id (0 B).
- **Seeds:**
  - `mus_h = mix(seed ^ mix(stream·0x9E3779B9 ^ a·0x85EBCA77 ^ b·0xC2B2AE3D))` (DG §5.5);
  - `seed = mus_h(doomo_town.seed, S_SEED, cue<<8 | chapter, day)`;
  - streams WALK MEL ALT GEN POOL LIFT HARM BASS DRUM FILL QUOTE BED VINYL VEL;
  - never `doomo_rng` or `story_rng`, and in no gameplay hash.

### 8.5 The metric (T3, `test_music_rep.c`, ported from `proto/host_main.c` + `lr60.c`)

**Per deck-A bar signatures** from the native dispatch:
- **M1** = FNV over notes, chords and drums as (role, onset on the 16th grid, semitone, log2 length), plus stems and dial band. **No velocity, no ghost or probabilistic drum steps, no microtiming** (D9). Only core steps (p = 15), fills and crashes count. CF-38 calls this signature S and adds the fuzzy F;
- **M2** = pitched roles only, without velocity or VIB/TIMBRE/ECHO;
- **ML** = LEAD (rest bars never match).

**Measures:** LRR (longest repeated run), Gap_W, HEAD_GAP, the anchor literal ratio, the GEN share, CAD_OK, STRONG_CT, seams, coverage.

| Scope (8 seeds × steady/walk/morph) | Gate |
|---|---|
| every cue ≥ 2 nodes, 10 min | no identical 8-bar M1 or 16-bar M2 window; LRR(M2) ≤ 8 |
| hubs (`MDF_HUB`), 60 min | **LRR(M1) ≤ 4, LRR(M2) ≤ 8**, no identical 32-bar M2 window, LRR(ML) ≤ 16 |
| identity | **HEAD_GAP ≤ 32**; anchor literal ≥ 50 %; anchor spacing ≤ 3; coverage |
| GEN | ≥ 30 % of non-anchor hub bars; **STRONG_CT ≥ 95 %**; leaps ≤ 9 st outside heads |
| form | **CAD_OK ≥ 95 %; seams = 0** |
| baseline (recorded) | today's HAHALAND_DAY: LRR(M1) = 294 bars in 10 min; 5 % distinct 8-bar windows |

≈ 2.7 M ticks: seconds on the host, and `SAN=1` too.

---

## 9. API and platform contract

### 9.1 `core/include/doomo/music.h` (MU0 writes it; MU1 maintains it add-only)

```c
#include "types.h"
#include "mus_ids.h"
enum { MQ_NOW, MQ_16TH, MQ_BEAT, MQ_BAR, MQ_PHRASE, MQ_NODE_END, MQ_MARKER, MQ_COUNT };
/* cues */
void music_set_cue(int cue, int grid);                       /* via the matrix; no-op if playing/armed */
typedef struct MusCueReq { uint8_t cue, grid, type, arg, entry, max_wait, flags, pad; } MusCueReq;
void music_set_cue_ex(const MusCueReq *r);
void music_set_cue_dual(int cue_a, int cue_b);  int music_cue(void);  void music_resume(int cue);
/* params: 0..255 targets, smoothed */
enum { MP_INTENSITY, MP_DANGER, MP_ENEMIES, MP_HP, MP_COMBO, MP_BOSS_PHASE, MP_HELL, MP_INFECT, MP_HOUR,
       MP_WEATHER, MP_SEASON, MP_CROWD, MP_ACTIVITY, MP_WALL_DIST, MP_ZONE, MP_SANITY, MP_COMFORT, MP_SPOOK,
       MP_FORT, MP_PET_STAGE, MP_PET_LINE, MP_TENSION, MP_HEAT, MP_TIMER, MP_HYPE, MP_TRUST, MP_LOVE,
       MP_ALTITUDE, MP_DIALOG, MP_MENU, MP_HARMONY /* house Style, Ch4 */, MP_QUEST /* family */, MP_COUNT /* 32 */ };
void music_set_param(int p, int v);  void music_set_param_now(int p, int v);  int music_param(int p);
void music_pulse_duck(int gain_q8, int ticks);  int music_intensity(void);
/* stingers, quotes, vinyl */
enum { STF_OVER, STF_REPLACE, STF_NO_DUCK = 2 };
int  music_sting(int sting, int grid, int flags);            /* -> ticks until the hit is audible */
int  music_sting_at(int sting, uint32_t land_tick, int flags);
void music_quote(int motif, int prio);
enum { VSRC_AUTO, VSRC_DECK, VSRC_TAPE, VSRC_HYBRID };
enum { AFTER_DEFAULT, AFTER_RESUME, AFTER_CONTINUE, AFTER_STOP, AFTER_NEXT_CUE };
int  music_vinyl(int pattern, int grid, int source, int after);   /* -> ticks until it lands; -1 refused */
int  music_vinyl_at(int pattern, uint32_t land_tick, int source, int after);
void music_vinyl_live(int16_t speed_q8, uint8_t fader);      /* DJ booth; 0x7FFF releases */
void music_resume_on_beat(void);  void music_sfx_synth(int sfx);
/* timing and the comic bridge */
typedef struct MusPos { uint32_t mt; uint16_t bar; uint8_t beat, sub48, cue, node, pass, beats_per_bar;
                        uint16_t bpm_q4; uint8_t key, chord_root; uint16_t chord_pcs, pad; } MusPos;   /* 20 B, audible */
void music_pos(MusPos *out);  int music_ticks_to(int grid);
int  music_land(int grid, int frames, int max_bend_q8);      /* <= ±12 %; returns the residual */
enum { MB_BEAT = 1, MB_BAR = 2, MB_PHRASE = 4, MB_NODE = 8, MB_MARKER = 16, MB_STING_HIT = 32,
       MB_VINYL_END = 64, MB_CUE = 128 };
typedef struct MusBeatEv { uint8_t kind, node; uint16_t bar; uint8_t beat, marker; int8_t due; uint8_t cue;
                           uint8_t key, chord_root; uint16_t chord_pcs; } MusBeatEv;           /* 12 B */
typedef void (*MusBeatFn)(const MusBeatEv *ev);
int  music_on_beat(MusBeatFn fn, uint8_t mask);              /* RESIDENT callers only (SS C6) */
void music_off_beat(int slot);  int music_beat_events(MusBeatEv *out, int max);  /* scenes poll */
void music_mute_window(int from, int to);  void music_duck_window(int from, int to, int gain_q8);
int  music_gated(void);  uint16_t music_snap_pitch(uint16_t pitch16);           /* babble in tune */
/* system */
enum { STALL_NONE, STALL_LOAD, STALL_SAVE, STALL_CARD };
void music_stall(int kind);  void music_prefill(int ms);
enum { AP_3D, AP_OVERWORLD, AP_UI, AP_CUTSCENE, AP_MUSIC, AP_HOUSE };
void music_profile(int scene_class);
void music_update(void);                                     /* audio_tick() calls it (D12) */
uint32_t music_hash(int cue, int minutes);                   /* T2 in-ROM, AT_NATIVE caps */
```

### 9.2 The comic-timing bridge

**Three ways to land a joke** (beat.c picks one after `music_ticks_to()`):
1. **Hold the picture.** `BOP_MUS_SYNC` holds the script clock until the next grid point is *audible* (`BOP_WAIT_BEAT` semantics, ≤ b frames).
2. **Bend the music.** `BOP_MUS_LAND` → `music_land()`, ≤ ±12 %. Events are in units, so ring B just carries a `MEV_TEMPO` bend at now + 2 ticks and a restore at the landing. Nothing is regenerated.
3. **Ignore the grid.** `music_sting_at` / `music_vinyl_at` hit the frame; the groove re-enters on the grid.

**Pre-roll.** `beat.c` looks 12 frames ahead (scripts are sorted by anchor and `at`) and issues stingers and vinyl hit offset + 2 ticks early. Events after `WAIT_A` land ≤ 67 ms late; the R3/R5 gaps absorb it.

**New `beat.h` ops** (S1 appends after `BOP_HEADTURN`):

| Op | a | b |
|---|---|---|
| `BOP_MUS_CUE` | MC | grid \| type<<4 |
| `BOP_MUS_STING` | STG | grid \| flags<<4 (`at` = the hit frame) |
| `BOP_MUS_VINYL` | VINYL | source \| after<<4 |
| `BOP_MUS_PARAM` | MP | value |
| `BOP_MUS_SYNC` | grid | max hold |
| `BOP_MUS_LAND` | grid | bend Q8 |

`BOP_MUSIC`, `_MUTE`, `_DUCK` and `BOP_TAPESTOP` keep working.

**Comic events:**

| Event | Music |
|---|---|
| SETUP | intensity −1 |
| PAUSE / DEADPAN | a mute window (beds off) |
| PUNCH | `sting_at(flavour, reaction frame)` |
| SMASH_CUT | CUT armed ≥ 2 ticks ahead |
| FREEZE | STOP on the frame + `AFX_FREEZE` |
| RECORD_SCRATCH | NEEDLE + STOP |
| SLOWMO / REWIND / STUTTER | the patterns |
| RESUME | `music_resume_on_beat` |
| IMPACT | kick + crash + `music_pulse_duck` |
| RUNNING_GAG | BEEP, ITS_FINE, the starer's 2-beat cut |

**Flavour → sting** (DIR §5.9): pratfall SLIDE/BOING · awkward ITS_FINE · reveal REVEAL/DUN_DUN_DUNN · fail SAD_TROMBONE · win TADA · gross SPLAT · holy HALLELUJAH · evil EVIL_LAUGH.

**Silence.** `music_gated()` feeds S2's `silence_budget` (20-25 % of cutscene frames; Interlude III exempt). The house: 20-25 % in scripted beats, 0 % while decorating.

**VM and dialogue** (S2/S1 hunks):
- `story_vm.h` gets `OP_MUS_CUE/STING/VINYL/PARAM`.
- `textpack.py`'s closed list gets `[CUE x] [STING x] [VINYL x] [PARAM p v] [SYNC grid n] [LAND grid]`.
  - `[SCRATCH]` keeps its timing → NEEDLE + `SHK_BRAKE` + a resume R13 later;
  - `[SILENCE]` = MUTE until the next MUSIC;
  - `[NO MUSIC]` = `MC_NONE` + beds.
- `ui_dialog.c`:
  - `{r}` = NEEDLE + a resume 14 frames later;
  - an open box = DIALOG 255 (`RF_DUCK_DIALOG` leads drop at the next phrase, −6 dB, AU22);
  - `{d}` = duck 0.5.

### 9.3 Compatibility (`audio_seq.c`), hook-ups and options

| Today | Becomes |
|---|---|
| `audio_music(SONG_x)` | `music_set_cue(mus_bind_cue(x, scene, map, arg), MQ_NOW)` with a transition shape that starts now (CF-18, CF-27, CF-29); `SONG_NONE` → `MC_NONE`; `audio_music_current()` returns the last requested song at once |
| `audio_music_once` | an `MCF_ONCE` cue or sting; the previous cue resumes from session memory |
| `audio_music_mute/duck` | windows; the deck keeps running |
| `audio_tapestop(n)` | `VINYL_TAPESTOP` |
| `resume_on_beat` | the next quarter downbeat |
| `audio_music_dual(a, b)` | deck A = a LEAD + BASS; deck B = b LEAD on HARM + KIT |
| `current` / `ticks` | same meaning |
| `audio_sfx_synth` | `music_sfx_synth` (D14) |
| `audio_voice_char` babble | voice 20 + `music_snap_pitch` (an `audio.c` hunk) |

**Who sets what** (each row is one hunk, §21.3). **Tier 0** needs none: existing `audio_music(SONG_x)` calls map to adaptive cues, and resident `doomo_town`/`doomo_map` supply hour, weather, chapter, map and beat.

| System (owner) | New calls |
|---|---|
| `overworld.c` (S1) | the 90-tick poll goes; `music_set_cue(cue_for(map, beat), MQ_PHRASE)` on change; HOUR WEATHER ZONE CROWD WALL_DIST INFECT TRUST; the border is a VINYL matrix row |
| `fp_*.c` (free play) | `MC_ISLAND` + HOUR/WEATHER; places → cues |
| `levels*.c`, `arena_modes.c`, `exorcism_scene.c`, `pet_scene.c`, `credits.c` (S3); `combat_scene.c` (custom) | `MC_HBn_*`; HELL = hell × 255 / hell_at; DANGER ENEMIES HP COMBO BOSS_PHASE TIMER; phases SPINBACK; death STOP + DEFEAT; `MC_RITUAL` logic clock + CHOIR_AH per hit; `MC_PET` + PET_*; credits step on `MB_BEAT` (`RANK_TICKS` need not match a tempo) |
| `home*.c` (H1/H4) | `MC_HOME`; COMFORT SPOOK FORT HARMONY, ZONE = room; `home_phase()` → nodes; cursed piece = TAPESTOP |
| studio/concert | RAM cues (§17); `PEV_*` from beat events; HYPE |
| menus; `overlay.c` (H3); saves and card | MENU = 255 (AU20); `music_prefill(1500)`; STALL_* |

**Options** (CH owns them; music reads its getters):

| Option | Effect |
|---|---|
| AU01-08 | `audio_bus_gain()`: music × `OPT_MUSIC_VOL`; stingers `BUS_JINGLE`; beds `BUS_AMBIENCE`; heartbeat `BUS_HEART`; piezo `BUS_PET`. One curve everywhere (EM issue 7). |
| AU09 | stereo / mono / swapped |
| AU10 | speaker: bass `timbre_alt` + mono fold |
| AU11/12 | PS1 reverb preset and depth caps |
| AU13 | playlists = LIVE player songs |
| AU14 | invasion music: Metal / Cute remix (toy metal) / Keep |
| AU15 | dynamic layers Off = I3 |
| AU20, AU22, AU24 | as named |
| AU21 | cut (D20) |
| Gentle audio | fades instead of vinyl |

### 9.4 Platform contract (`platform.h`: one appended block; weak defaults = the legacy bridge)

```c
#define MUS_MT_PER_TICK 8
void     plat_audio_caps(DoomoAudioCaps *out);
uint32_t plat_music_plan(const DoomoVoicePlan *p, int n);   /* -> rendered-voice mask (main thread) */
void     plat_voice(int voice, const DoomoVoice *v);        /* IRQ, from mus_drv, on change only   */
void     plat_audio_bus(int bus, uint8_t gain_q8);
void     plat_audio_fx(int fx, int a, int b);               /* REVERB ECHO FREEZE FADE PROFILE CRACKLE PSG_DUCK */
void     plat_music_stall(int kind);
int      plat_tape_begin(uint16_t rate, uint16_t samples);
typedef struct DoomoVoiceChange { uint32_t mt; uint8_t voice, pad[3]; DoomoVoice v; } DoomoVoiceChange;   /* 20 B */
void     plat_tape_render(const DoomoVoiceChange *c, int n, uint16_t s_from, uint16_t s_to);
void     plat_tape_play(int16_t speed_q8, uint16_t pos, uint8_t gain);
void     plat_tape_end(void);
void     plat_sample_ex(int sfx, uint8_t vol, int8_t pan, uint8_t prio, int8_t semis);
void     mus_drv_tick(void);                                /* core; the platform calls it per mt  */
```

- **Planning:** the core asks `plat_music_plan()` at cue start, at stem commits and on tier changes, and skips unrendered roles; no decision changes.
- **Envelopes and modulation:** hardware envelopes where they exist (`psg_env`, `spu_adsr`), `DoomoVoice.env` otherwise; pitch modulation runs in `mus_drv` at 60 Hz on both consoles.
- **Weak bridge:** 4 voices onto `plat_tone` channels 0-3 (lead, harm, bass, kit), so the engine runs everywhere from M1.
- **Native:** 8 driver calls per tick, an FNV hash of every `DoomoVoice`, and `--wav` via `mus_synth` at 22,050 Hz stereo (closes EM gap 13).

---

## 10. GBA rendering: chiptune at the limit, inside an honest budget

### 10.1 Allocation, PSG tricks, write discipline

**Allocation:**
- 4 PSG channels in every tier (0 CPU).
- FIFO A + B on timer 0 (DMA1/DMA2): **L/L+** = A mono soft mix, B idle (the stem after I9); **M+** = A left, B right, packed stereo.
- Timer 1 (v2) cascades into a 238.9 Hz PSG micro-queue IRQ.
- **VBlank:** apply this frame's PSG writes → `mus_drv` × 4 mt for the next frame → mix → DMA.

| Timbre | SQ1/SQ2 | WAVE | NOISE |
|---|---|---|---|
| PULSE, PWM | the nearest of 12.5/25/50/75 %; 25 ↔ 75 across squares = width and the twin; duty switching per control frame = pseudo-PWM | any duty as 32 steps; bank morph | — |
| SAW, TRI, SINE | `timbre_alt` | 32-step tables (TRI4 = NES staircase) | — |
| ORGAN, POWER5, VOWEL, GROWL, BELL, GROSS | — | **64-step: a one-channel power chord** (root + fifth an octave down), vowels, organ. 64-step mode plays both banks, so these timbres cannot bank-morph (CF-39) | — |
| KIT; slide whistle, boing, birds | **SQ1 hardware-sweep kick** and sweeps | toms | snare/hats/crash; 15/7-bit LFSR; divider × 3 round robin; shift-sweep crashes |

**Write discipline** (ends EM issue 5's zipper):
- frequency writes never retrigger;
- envelopes only at note-on (`psg_env` fitted by mmlc);
- wave volume without retrigger;
- **double-buffered wave banks** (write the idle one, flip on a control frame = morphs);
- `SOUNDCNT_L` L/R for hard pan and **ping-pong arps per 8th**; its master ducks the PSG;
- no zombie mode.

**Tier-L metal:** SQ1 lead (25 % + duty sweep), SQ2 twin a 3rd under (75 %), WAVE POWER5 (chord + bass), NOISE kit + the sweep kick.

### 10.2 Mixer v2 (`gba/src/mixer.iwram.c` + `mixer_loops.s`)

- **Packed stereo:** `acc += s × (volL | volR<<16)` (one MLA, with the sample as the multiplier operand; a panned voice costs the same as a mono one). Headroom: |s × vol| ≤ 2,047 per voice, so 16 voices never carry across the two halves (CF-9).
- 32-sample `int32` blocks (128 B).
- Sample-accurate starts at `mt × N/4`.
- **Priority:** SFX > tape > LEAD/BASS > KIT > COUNTER > PAD > BED; over budget, the lowest soft music voices are skipped (rendering only).

**Kernels** (estimated cycles per sample; T5 measures them before any promotion):

| Set | Kernels |
|---|---|
| **lean** (IWRAM ≤ 1,378 B) | output stereo 12 / mono 7; PCM from ROM 14; true-PWM pulse 7; saw 7 / hard-sync 10; tables 9; stem `ldm` 5 (after I9) |
| **rich** (after #78, or THUMB at ≈ 2× in tier S) | noise 10, tape interpolation 20, crackle synth 5, echo 7, one-pole +4, FM-lite 16 |

Per voice per vblank: 60 + 32 per block.

### 10.3 Voices per rate, tiers, governor

Frame = 280,896 cycles; 1 % = 2,809. Chip voice = 8N + 60 + 32⌈N/32⌉; PCM voice = 14N + …; stereo output = 12N.

| Rate | N | Output | Chip voice | PCM voice | Echo | Soft voices that fit (2 SFX reserved): 12.7k / 8 % / 12 % / 20 % |
|---:|---:|---:|---:|---:|---:|---|
| 10,512 | 176 | 2,112 | 1,660 (0.59 %) | 2,716 (0.97 %) | 1,232 | 3 / 8 / 15 / 29 |
| 13,379 | 224 | 2,688 | 2,076 (0.74 %) | 3,420 (1.22 %) | 1,568 | 1 / 6 / 11 / 22 |
| 18,157 | 304 | 3,648 | 2,812 (1.00 %) | 4,636 (1.65 %) | 2,128 | 0 / 3 / 7 / 15 |
| 21,024 | 352 | 4,224 | 3,228 (1.15 %) | 5,340 (1.90 %) | 2,464 | 0 / 2 / 5 / 12 |

Today's C mixer: 0.3k / 8.1k / 12.7k / 17.2k / 21.8k cycles for 0-4 SFX (measured).

| Tier | Rate | Music voices + extras | Cycles per vblank | Δ per drawn frame |
|---|---:|---|---|---|
| **L** | 10,512 mono | 4 PSG (+ stem after I9); the crackle loop only while a cue's `fx` or a vinyl asks for it | idle 0.3k (today's short-circuit kept, CF-6); otherwise 1,232 + n × 2,716 = **3.9k / 6.7k / 9.4k / 12.1k for n = 1-4, below today's 8.1k-21.8k** | 0 for the mixer; the driver's cost is in CF-6 |
| **L+** | 10,512 | + ≤ 3 soft voices inside 12.7k, yielding to SFX | ≤ 12.7k | ≤ +0.7 ms |
| **M** | 13,379 stereo | + 4 soft | 17.8k (2 SFX) | +1.3 ms |
| **S** | 18,157 | + 8 soft, echo, TAPE | 37.5k | +3.7 ms |
| **S+** | 21,024 | + 9 chip + 2 PCM, echo, TAPE, crackle synth | 57.1k | +6.0 ms |

**The governor** (every 32 drawn frames, W = `gba_mailbox.work_sum / work_n`):
1. protected ≥ 28 fps scenes rise only while W + Δ ≤ 32 ms;
2. others while W + 1.5Δ ≤ 46 ms;
3. drop a tier when W > 48 ms (4 s hysteresis, at phrase boundaries; the rate is fixed per scene);
4. caps: 3D and the overworld L+ (mono, because `gba_big` has no room there, CF-11); UI/cutscene S; house S without TAPE/echo; Studio/Concert/Jukebox S+.
5. **CF-7 supersedes the decision step.** A promotion is a trial, judged by the *measured* drawn fps over 64 drawn frames, because vblank quantisation makes W-based predictions wrong. Rules 1-3 only pre-filter, and they use windowed deltas of `work_sum`/`work_n`, never `work_max`, which the harness zeroes.

**Predicted today (corrected, CF-7).** Every UI scene at 20-28 fps already averages 33.4-41 ms of work, at or just past the 2-vblank line (33.5 ms). Its fps is a mix of 2- and 3-vblank frames, and adding S or S+ (+3.7 or +6.0 ms) moves nearly every frame to 3 vblanks (19.9 fps).
- Title logo, new island, shop, roster and resident start at L+ and *trial* M.
- Studio play (34.1 ms) trials M.
- Apartments (33.4 ms, 0.08 ms of headroom) is an edge scene (`AP_UI_EDGE`, CF-6).
- The house stays L+ (C22 `home_steady` ≥ 28).
- Slow 2D and all 3D stay L.

**No promotion survives a measured fps loss**, and scenes promote themselves when the perf tasks land.

### 10.4 Timing, ducking, TAPE/echo, stem

- **Timing:** PSG writes are delayed 4 mt to land with the soft voices. v1 quantises the PSG to vblanks (twice today's resolution, no bursts). v2 adds timer-1 sub-vblank writes. They need no nesting: the VBlank handler (≤ 64k cycles) ends before timer 1's first IRQ at sample N/4 (CF-10). Flash saves = `STALL_SAVE`.
- **Ducking:** bus gains in the packed volume; PSG master + velocity. Dialogue drops `RF_DUCK_DIALOG` leads at −6 dB.
- **Babble = voice 20:** SQ2 when free, else a soft pulse (M+); it takes HARM's channel only in tier L with all 4 PSG busy.
- **SFX classes** (`doomo_sfx_prio[]`): UI 1 voice, never steals music · GAMEPLAY steals music priority < 8 · CRITICAL steals anything but LEAD/BASS/STING · JINGLE is never stolen. GAMEPLAY+ `music_pulse_duck` sidechain pads −3 dB for 6 frames.
- **TAPE/echo:**
  - 4 + 4 KB borrowed from `gba_big` when free (2D except the overworld and the house);
  - a reclaim falls back to DECK + echo-by-repeat;
  - the M+ FIFO A + B double buffers (896 / 1,216 / 1,408 B for M / S / S+) live in the `gba_big` audio tail, released synchronously on any claim (CF-11);
  - echo at half rate, 2 taps, ≤ 75 % feedback (≈ 0.76 % in S); **freeze** = 100 % feedback with the input cut.
- **ROM stem (after I9):** KIT/PERC pre-rendered by `mus_synth` (8-bit, 10,512 Hz, 4 grooves + 4 fills per cue, ≤ 120 KB per cue, ≤ 1.2 MB); DMA2 restarts at the vblank nearest each beat; ≈ 0.06 % CPU and +352 B. The PS1 equivalent is ADPCM loops of the same PCM.

---

## 11. PS1 rendering: the whole SPU, a super-chiptune

### 11.1 Voices, banks, rendering

| Voices | Use |
|---|---|
| 0-11 | deck A (today music uses 4) |
| 12-15 | deck B, lent to SFX when idle |
| 16-17 | beds (**17 owns NON**) |
| 18 | babble |
| 19 | TAPE/stream |
| 20-23 | SFX + the SFX deck; with lending, 6-10 SFX; GAMEPLAY may steal PAD/BED |

**Richer than the GBA:** all 12 stems at once, the twin as 2 voices at ±6 c, the doubled power-chord stack, an 8-voice choir, constant-power stereo.

**`SPUBANK.BIN`** (host `tools/spubank`; boot encoding would take 1.4 s):
- pulses at **15 duties** in 112- and 28-sample cycles; saw, sync loops, TRI/TRI4/SINE;
- the shared 64-step tables + PIEZO;
- baked PWM/filter morphs and 2-op FM (≈ 2.3 KB each); 3 noise colours; muffled variants (the "filter");
- kits and choir from **the GBA's own 8-bit PCM**;
- ≈ 40 + 48 + 24 + 8 KB.

**Rendering rules:**
- **key-on:** `spu_adsr` fitted (sustain-increase for swells); bank + cycle by range; **KON batched per mt**.
- **pitch:** a 192-entry 2^(i/192) table → the 4.12 register (short cycle above 0x3FFF).
- **volume:** vel × bus × a constant-power pan; **true stereo**; fades and ducks by **hardware sweep**.
- **optional, *verify on hardware*:** PMON FM pairs with a silent modulator; repeat-address PWM morphs; inverted saw pairs. Baked loops are the default.
- **reverb:** per cue (`MusCue.fx`), EON from `RF_ECHO`; presets change only in silence; **freeze** = inputs cut + feedback below unity.

| Reverb area | Preset (work area) | Send |
|---|---|---|
| day, shops, living rooms | Room 0x26C0 | low |
| rain | Studio Medium 0x4840 | — |
| snow, night, church, graveyard, Cathedral, bathroom | Hall 0xADE0 | mid |
| Hellvile, Crypt Row, Vape Ruins | Space Echo 0xF6C0 | — |
| combat | Room | low |
| dreams, haunted | Echo 0x18040 / Half Echo 0x3C00 | high |
| Concert stages | Studio Small / Hall / Space Echo / Room / Echo | per stage |

### 11.2 SPU RAM map (512 KB; D16)

```
0x00000 capture (unused) 4 KB | 0x01000 silent + core waves 4 KB | 0x02000-0x09FFF SFX ADPCM (CS P3; ~27.6 KB used) 32 KB
0x0A000 chip banks 40 KB | 0x14000 kits 48 KB | 0x20000 choir/babble/"aah" 24 KB | 0x26000 crackle 8 KB
0x28000 TAPE fwd+rev 8 KB | 0x2A000 area bank 48 KB | 0x36000-0x66FFF BOOKSHELF (MUSIC.BIN incl. GEN <= 44 KB) 196 KiB = 200,704 B
0x67000 spare 4,032 B | 0x67FC0-0x7FFFF reverb (fits Echo 0x18040) 96 KB
(CF-16: the old 172 KiB shelf could not hold the ~179,000 B score; CS's AU11 work area at 0x7e000 is withdrawn)
```

### 11.3 Paging, locking, timing

- **Window (4,608 B):** deck A header + bank 1,024 · deck B 512 · 2 deck-A pages 2,048 (current + next, prefetched a bar ahead; a page = base ≤ 640 + this pass's variant ≤ 384, two DMAs) · deck-B page 512 · 2 common packs 512.
  - `SpuRead` ≤ 1 KB takes < 0.5 ms (e); a bar at 200 bpm is 1.2 s.
  - The GBA reads the same records in place.
- **Locking:** the IRQ never touches `SPUCNT` transfer bits or the transfer address; main-thread transfers mask the IRQ only around the mode sequence; SFX key-ons go through a 16-entry queue drained into the batched KON.
- **M3 starts with the SpuRead-during-playback hardware check.** Fallback **F-SPU**: an 8 KB window with the whole cue loaded by the group's CD read (+3.4 KB, from the §20 levers).
- **Timing:** root counter 2 at **240.0 Hz** (33,868,800/8/17,640) runs `mus_drv_tick()` (≈ 0.3 %). **CD loads play through:** `music_prefill(1500)` → `CdRead`/`CdReadSync` with IRQs on (music is resident and calls no overlay, SS C6). Memory card → `audio_hw_hush()`.
- **Boot:** `MUSIC.BIN` (≤ 200,704 B) and `SPUBANK.BIN` (about 120 KB) are read in ≤ 64 KB chunks through the free overlay slot during `SCENE_BOOT`, which is resident. This happens after P3's `SFX.BIN` and before the first group loads, and takes about 1.1 s (CF-17). Never during the title: `fp_title` is overlay code that occupies the slot. Area banks ride the group read (+0.16 s).

---

## 12. Parity matrix

| Feature | GBA | PS1 |
|---|---|---|
| graphs, variation, GEN, transitions, stingers, quotes, beds | core (identical decisions) | core (identical decisions) |
| timing | 4 mt per vblank; sample-accurate soft voices; PSG at vblank or timer 1 | 240 Hz root counter, batched KON; plays through loads |
| PWM, saw/sync | duty switching + soft true PWM; wave tables + soft sync | 15-duty banks + morph loops; ADPCM saw + baked sync |
| power chord, vowels | wave 64-step + bank morph | the same tables as ADPCM |
| drums | LFSR + divider round robin + sweep kick; ROM PCM kits | noise loops at pitch, NON; the same PCM as ADPCM |
| FM, space | FM-lite (rich); soft echo + echo-by-repeat; echo freeze | PMON + baked FM; reverb per cue; reverb freeze |
| stereo | packed soft mix, PSG L/R, ping-pong arps | per-voice pan + sweeps |
| voices beyond SFX | 4 PSG + 0-11 soft (governed) + stem (I9) | 12 + 4 + 2 beds + babble + tape |
| choir (25 on screen) | babble + ≤ 3 vowel chops + PSG harmony | 8 voices + babble |
| vinyl | DECK every tier; TAPE in 2D with `gba_big` free; PCM crackle | DECK; TAPE in 2D; crackle loop + NON |
| DJ | D-pad + buttons | the same, plus **the DualShock stick as the platter** |

---

## 13. Instruments and samples

| Instrument (`common/inst.dms`, ≈ 64 rows) | GBA L (PSG) | GBA soft (M+) | PS1 |
|---|---|---|---|
| `box_lead`, `celesta_arp` (S0) | SQ 12.5 %, fast HW decay, echo-by-repeat | soft 12.5 % | pulse 4/32, fast ADSR, Hall |
| `toy_lead` (S1) | SQ1 25 % + delayed vibrato | soft PWM | pulse 8/32 |
| `chip_chord` | SQ2 50 % chord at 60 Hz | soft chord, ping-pong | pulse 16/32, ping-pong |
| `tri_bass`, `walk_tri` | WAVE TRI | — | TRI |
| `wobble_lead`, `hm_arp` (S2) | SQ1 12.5 % deep vibrato / SQ2 25 % | soft | pulse 4/32, 8/32 |
| `organ_wave`, `hymn_organ` | WAVE ORGAN / SQ1 + SQ2 stacked | table | ORGAN × 1-4 |
| `sync_lead` (S3-4) | SQ1 25 % + duty sweep | **hard-sync saw** | baked sync loop |
| `power5` | **WAVE POWER5** | saw pair L/R | POWER5 × 2, detuned, panned |
| `saw_bass`, `twin_lead` | WAVE SAW / SQ2 75 % | saw + sub / pulse | SAW + sub / pulse 24/32 ±6 c |
| `lounge_lead`, `pwm_pad` (L) | SQ1 50 % + glide / — | pulse + glide / true PWM | pulse 16/32 / PWM loop |
| `piezo` | SQ2 50 %, no envelope | — | PIEZO cycle |
| `choir_aah/ooh` | — | PCM chops 1-3 | SATB × 2 loops |
| `fm_bell`, `growl_bass`, `gross` | SQ1 + noise / WAVE morph | FM-lite | PMON or baked FM / loops |
| 6 kits | NOISE + SQ1 sweep kick + WAVE toms | PCM | ADPCM, round robins |
| `kazoo`, `slide_whistle`, `boing` | SQ1 sweep | saw + vibrato | pitch glides |
| `crackle`, `bed_*` | PCM loop / NOISE while resting | synth | loop + NON / voices 16-17 |

| Samples | GBA phase 1 | GBA after I9 | PS1 (SPU RAM) |
|---|---:|---:|---:|
| kits | **≈ 8 KB minimal** (SFX kick/snare; hats, clap, crash on PSG noise) | ≈ 120 KB | ≈ 48 KB, all 6 from M3 |
| choir, babble formants, "aah" | ≈ 6 KB (S/S+) | ≈ 100 KB | ≈ 24 KB |
| crackle, zips, needle, reversed swell | ≈ 6 KB | ≈ 12 KB | 10 KB |
| area one-shots / stems | — | ≈ 60 KB / ≤ 1.2 MB | ≤ 48 KB per group / loops |

The PS1's full kits are richer rendering of the same drums; the GBA plays the same hits on PSG noise until I9.

---

## 14. Leitmotifs (`common/motifs.dms`; canon: `trailer/music.c`, DIR §2)

Heads are quoted literally (only transposed or mode-mapped); tails may bend.

| Motif | Notation / shape | Uses |
|---|---|---|
| **MINGLE** — *canon: S2's shipped `mml_mingle.c` (CF-2)* | F major, tempo 96, on the @12 music box: `o5 l8 c f a f >c4< a4 \| b- a g f g4 c4 \| …`, head **C-F-A-F** (the F triad climbing to C'). The song both sides sang before the Wall; the finale's last chorus lands in F, MINGLE's own key. | P0 music box; the Mayor's hummed fragment (Ep 1.6, bar 1); F1 in sepia; the festival's last chorus; the rite forwards and retrograded |
| **HOOK** | C: `t140 o5 l8 \| e e g e >c4< g4 \| a a g e d4 e4 \|` + bar-3 alternatives (LOVE `f f a f >c4< a b`; `f a >c< a g f e d`) and bar-4 alternatives (PAC, HC, button); head E-E-G-C' | HaHaland, the title, quests, the anthem |
| **RIFF** | Em: `o2 l16 \| e e g8 e e b-8 e e a8 e e g f+ \| e e g8 e e b-8 e e >c8< b8 b- a \|`, head E-E-G-B♭; **redeemed** `e e g+8 e e b8 …` | folk swing, war march, redemption |
| **WALL** | `e2 f2 \| e2 b-2` (fixed mode) → `e f+ e b` at C4_GATE | dread, fog, the gate |
| **COMMINGLING** | E major: HOOK over the redeemed RIFF + BEEP descant + FUZZ/NOX canon; the last chorus +1 to F; amen + "beep." | finale, credits |
| **BEEP** | piezo o6 l16: egg `>c<` → adult `c e g r >c8<` + "beep."; lines: ANGEL `c g >e< r >b<`, FERAL `c c+ g r >c<`, DEMON `c e- g- r >c<` + "bweep." | pet, unlocks, finale top line |
| **VEX** | Am, every interval a glide: `o4 a2 >e-4 d8 d-8 \| c2.<` (Am9 Fmaj7 Dm9 E7♭9) | Cloud Nine, the droop, credits in A major |
| **VAPULA** | Em `e2 b-4 a8 a-8 \| g2.`; P1 augmented, P2 diminished with twins, P3 inverted `e2 b-4 b8 >c8 \| c+2.<` +1 | Breach 2, the boss, Spire muzak |
| **FLUFFY** — *proposal* (the monster was a pet) | VAPULA redeemed on the music box: `c2 g4 f8 e8 \| d2.` | the jar, "...Hi, Fluffy.", Interlude III, the credits cameo |
| **HEE HEE** — *canon: S2's shipped `mml_heehee_jingle.c` (CF-2)* | a bell (@3, o6) plinks `e r e r` and, unanswered, `c+ r c+ r`, over A → G♯ (tempo 120, 2 bars) | the Wall's giggle and Hattie's laugh coming back late (Ep 2.2); **the Fountain echoes it a beat late** until the mission whose Later makes it "exactly on time, forever" |
| **FUZZ / NOX / BONEBOTTOM** | F `o5 l4 f a g f \| b- a8 g8 a2` + **plagal amen** / mirror `f d- e- f` (F minor) / `f a- g- f` over B, organ metal | church, exorcism / Chapel Hill, the canon / Bone Cathedral |
| **RIVET** | the redeemed Riff first, G mixolydian `o3 l16 \| g g b8 g g >d8< g g >c8< g g b a \|` + anvil | workshop, weld, the Forge duet |
| **HOME** | C 3/4 84 `\| g4 >c4 e4 \| g2 e4 \| e2. \|` (Hook bar 1 reversed); answer `\| d4 e4 g4 \| >c2.< \|`; doorbell 3 notes, save = the full phrase | the house's phases |
| **LOVE · MAYOR · NANA · PIP · BLOOM** | `f f a f >c4< a b` · `c12 c12 c12 e4 g4 >c4<` · `c8 f8 f8 a8 g4 f4` · `d8 g8 b16 a16 g8 d4` + squeak · `a8. b16 >c8< b8 a4` + beeps | romance · quests · bakery · the bunny · hospital |
| **TRUE NAME · GOODBYE** | 4 degrees of E harmonic minor by resident id · `e2 d4 c4 \| <b2 a2>` | exorcism, windows, credits · loss |
| mini-bosses | WRAITH = Vex on power chords + STUTTER; GUTTERKING sticky slides; FLUFFNUTS whistle + punch clock; WIDOW broken box waltz → D major; GRUMBLEGUTS 7/8 gate engine | Breach 1, 3-7 |

**The shared cell.** Hook and Riff share E-E-G: sunny in C, a tritone push in E minor. A PIVOT through it flips worlds. The finale's E major is the 3rd of C and the root of E minor, so both themes agree on beat 1. MINGLE (F major) is not built on the cell; it is the *destination*: the finale's +1 lift from E lands in MINGLE's key (CF-2).

**Dial forms** (DIR §2.11): HOOK S0 lullaby → S1 HaHaland → S2 Em swing → S3/S4 E-major anthem; RIFF S0 detuned box → S2 folk → S4 march; BEEP is native piezo; FUZZ is native hymn.

---

## 15. Cue list with leitmotif usage

**Key:** W1 = wave 1 (M4), W2 = wave 2 (M8); G = GEN slots; V: = vinyl; (SONG) = the A-material and the classic cue.

### 15.1 World and system (MU4a)

| Cue | Motifs · tempo · key · dial → behaviour | W |
|---|---|---|
| `MC_BOOT` | BEEP egg, S0. The shimmer freezes (V:STOP) → "beep." | 1 |
| `MC_TITLE` (TITLE) | HOOK, RIFF in the bass; 140 C S1. Idle 60 s → "corruption creeps" (Em S3); a button → V:NEEDLE → A. A A' B C + G BUILD/TAG; Cute/Spooky/Metal/Lullaby variants. | 1 |
| menus | MENU: −9 dB, LP, mono. Save = HOME; load = the doorbell. | 1 |
| `MC_NEW_ISLAND`, `MC_CREATOR` (CREATOR), `MC_NEWS` | HOOK. Hover/tab stems; the babble choir; hellish creation → dial; V:SCRIBBLE; gloomy news → F minor. | 2 |
| `MC_ISLAND` (HAHALAND_DAY/NIGHT) | HOOK; HOME/BEEP at night; 72-128, F/G, S0-1. HOUR bands (dawn opens on the box); WEATHER (rain brushes iv/♭VI, snow sleigh half-time, petals glock); INFECT droop and ♭5; the starer gag; CROWD. Day A A' B C BR BRK TAG + G ×3; night A B INTRO. | 1 |
| `MC_HAHALAND` | HOOK + MAYOR, FUZZ, WALL bleed, VEX leak; 124 F S1. ZONE nodes: plaza, Town Hall, Towers bossa, St. Cuddles plagal, Fun Fair calliope, Park + WALL ≤ 12 cells, East Bank 3/4, surf beach. Ch2 INFECT 32/96/192; Rivet's redeemed counter. + G ×3. | 1 |
| `MC_VAPE_LOUNGE`, `MC_OMINOUS` | VEX 88 Am L (a door emitter); WALL 70 (the scream = V:STOP + 1 bar + DUN_DUN_DUNN). | 1 |
| `MC_HELLVILE` (HELLVILE, retuned to Em) | RIFF folk 108 S2; TRUST redeems it. ZONE: Gate drone, Pits lo-fi, Forge RIVET duet, Imp busker, Chapel NOX, Vape Ruins broken box, Sealed Gate (later HOME in E), Crypt harpsichord, Arena lobby. + G ×3. | 1 |
| `MC_PRISONERS`, `MC_HOPE` | RIFF slow 3/4 (each prisoner helped = one more redeemed note); HOOK in the church with the Riff muffled outside (CROSS 1 bar). | 2 |
| `MC_FESTIVAL`/`PARK` (FESTIVAL) | COMMINGLING + MINGLE in the last chorus; 144 E→F. ZONE stage, meadow, river L/R duet, garden, parade, stalls. A A' B(HOME) C(canon) cameos +1; G ×2. | 1 |
| `MC_MINGLE` (MINGLE) | box, hummed, full; the rite forwards on A + **reversed** on B. | 1 |
| `MC_HOME` | HOME 84 C 3/4. **Rooms = stems** (ZONE = room; neighbours muffled): Living radio (jukebox pick, LP mono + crackle + tick), Kitchen percussion, Bedroom box, Bathroom reverb, Cellar dim pad + the Riff through the floor, Garden birds, Annex detuned. **Phases:** COSY (COMFORT), HAUNTED (C minor, V:WOBBLE; SPOOK), BARRICADED (Hold the Door: tom march + Riff; FORT, DANGER), EMPTY (clock), OPEN (disco), DREAM. HARMONY adds the redeemed Riff. Bubbles duck 0.6; inserts mute; a cursed piece = TAPESTOP; the sealed room is silent. + G C. | 1 |
| `MC_HOME_VISIT`, `MC_DREAM` | the host's genre + TRUE NAME; HOME in the sleeper's genre. Dial +2 per vignette; NIGHTMARE = a melting record + heartbeat + the fear sting; a hug = V:STOP. | 2 |
| `MC_FLASHBACK` | any cue on a memory bus (LP, mono, crackle, WOBBLE 2 %), entered by V:REWIND. | 1 |
| `MC_CREDITS` (CREDITS) | COMMINGLING + cameos (MAYOR in E, NANA, FUZZ, RIVET, PIP, NOX, VEX in A, VAPULA → FLUFFY); 120 E→F. Steps on `MB_BEAT`; hold A = 1.5× + a scratch; B = HOME for the lost; ends amen + "beep." | 1 |

### 15.2 Cutscenes and story (MU4d)

| Cue | Direction |
|---|---|
| `STG_CH1..4_CARD` | Hook → toy pop; the Hook drooping into the lounge; Riff octaves + V:SPINBACK; the lone Riff ending on the redeemed B |
| `STG_EP_CARD` | 38 beats (P0, 27 episodes, 8 specials, E1-E2): chapter motif + per-episode reharm (beat read from `doomo_town`) |
| recap / cliffhanger | V:REWIND → `MC_FLASHBACK` / V:STOP + one WALL or VAPULA note |
| `MC_ARRIVAL`, `CUT_HOME`, `CUT_FOG` | dawn bloom + MAYOR; doorbell → BEEP; WALL + a far-left VEX |
| `MC_ERUPTION` | **trailer s08:** Hook 2 beats → V:NEEDLE → silence → "beep." → BWAAM → the Riff full metal (FAST_MORPH) |
| `MC_VAPULA_DOWN` | VAPULA deflates (V:STOP), a squeak, VICTORY in E major |
| `MC_WALL_FALLS` | **trailer s13:** the shy box Hook → the crumble = E-major COMMINGLING; never undercut |
| `MC_SUMMIT` (Ep 4.6) | FLUFFY under "...Hi, Fluffy."; HEE HEE **on the beat**; "the music comes back on the grid" (a SYNC pickup) |
| C4_GATE | WALL resolves; the crossing = TAPESTOP 5 + mute 14 + the Hellvile downbeat |
| `MC_FEAST` (S186) | a grand NANA polka (tuba pulse 132 F); fullness → HEAT tempo; straps = BOING; IRIS |
| Interlude III | `[NO MUSIC]`: clock bed only |
| 9 story songs (D23) | classic cues at M1. Suggested: WALL_LULLABY = WALL redeemed 3/4; TWO_CHOIRS = FUZZ/NOX canon; THIRTEEN_BELLS = FM bells → F; HELL_LULLABY = BEEP + the redeemed Riff |

### 15.3 Side missions as families (MU4d; QUEST = the family tag)

| Family | Layer · stingers |
|---|---|
| fetch / deliver | — · QUEST_NEW (Hook head), STEP, DONE (small MAYOR) |
| find / lost pet | BEEP counter + **hot/cold** detune ≤ 40 c · adult BEEP |
| photo | shutter hats · sparkle |
| matchmaking | LOVE quotes · LOVE duet |
| exorcism request | FUZZ quotes · church cue |
| cross-border | Hook ↔ Riff call and response · redeemed unison (TRUST +) |
| mini-boss hunt | combat-lite stems · defeat |
| timed | clock hats, TIMER tempo · SAD_TROMBONE if late |
| chase / escort | double-time drums · V:STOP when caught |
| comedy (open mic) | logic-clock beat · rimshot / cricket |
| secrets | — · a box arpeggio; all secrets unlock a Jukebox cue |

### 15.4 Combat (MU4b)

**Every Breach:** cute half → hell half; HELL drives the dial, and `hell_at` = S4. Third person = wider stereo. Gore CUTE = toy metal; BRUTAL = real; BOTH alternates every 4 bars.

| Cue | Behaviour | W |
|---|---|---|
| `MC_HB1` Cellar | Vex's lounge muffled through the ceiling → HELL_BREACH; 88→150 Am→Em; more LP per floor | 1 |
| `MC_HB2` Spire | half-time → BOSS pool; 120→150, Em **+1 semitone per floor**; lifts play VAPULA bossa | 1 |
| `MC_HB3`-`MC_HB7` | Sewers: soda calypso → wet GROSS morph. Factory: conveyor + whistle → 3+3+2 industrial. Graveyard: harpsichord, Dm **−1 semitone per loop**, the Widow's box by distance. Cathedral: organ → organ metal, Fm, an organ register per floor. Underbelly: busker → Invasion + a 7/8 piston ostinato. | 2 |
| `MC_INVASION` (INVASION) | RIFF war march 168 Em S4; VAPULA slot; a rescue's Hook head; TRUE NAME + ♭5 near the possessed. DANGER, ENEMIES, HP (< 25 % = heartbeat stem, lead out, muffled), COMBO; an encounter = fill + crash → I4; a nest cleared = breakdown + metal TADA. A B drone BRK FILL + G ×2; twin; stem (I9). | 1 |
| `MC_BOSS_VAPULA` (BOSS) | P1 half-time doom (augmented, evil choir, NON hiss) → P2 thrash (diminished, twins) → P3 +1, inverted, double-time, **the Hook as counter-melody**; Em→Fm; V:STOP → `MC_VAPULA_DOWN` | 1 |
| other bosses | WRAITH (Vex metal, STUTTER coughs; long-echo chord). GUTTERKING (sticky slides, WOBBLE; slow STOP + drips). FLUFFNUTS (+16 bpm per phase, 7/8 conveyor; whistle down). WIDOW (tragic box waltz, S2 max; → D major + GOODBYE, **no fanfare**). BONEBOTTOM (anti-hymn → organ metal → bells; sour cluster → Fuzz's amen). GRUMBLEGUTS (7/8 engine, STUTTER, the GURGLE warning on the beat; **the Riff in major for the first time**). **Grammar:** 2-bar intro, phases by BOSS_PHASE, V:SPINBACK between phases, < 10 % = half-time + choir, defeat = STOP + silence + sting. | 2 |
| combat misc | W1: `MC_COMBAT` (map cue → stems), `MC_LEVEL_SELECT` (hover = cute motif), `STG_RANK_S/A/B/C`, `MC_DEATH` (STOP, GOODBYE; retry REWIND), `MC_ARENA` (+1 semitone per 5 waves, +2 bpm per wave, cap +24). W2: HORDE_SHOP (metal lounge + countdown), BOSS_RUSH (REWIND chains), TIME_TRIAL (pace click), dial-locked Cute/Brutal runs, MAYHEM (200 bpm, all inverted, a seeded vinyl every 8 bars). | 1/2 |

### 15.5 Activities (MU4c)

| Cue | Behaviour | W |
|---|---|---|
| `MC_CHURCH` (CHURCH), `MC_RITUAL` (EXORCISM) | FUZZ 72 F H (SANITY → organ voices; amen TAG). **Logic clock** ritual: Fuzz calls, the RIFF answers; hit = CHOIR_AH; miss = laugh + detune; a streak strips the demon S4→S1; holy water = accelerating hats. | 1 |
| `MC_TRUE_NAME`, `MC_EXORCISED`, `MC_SOUL_LOST` (SAD) | note-by-note reveal → **resolves into the amen**; the biggest amen; GOODBYE; Lay to Rest = organ + amen | 1 |
| `MC_PET` (PET) | **piezo on purpose** (GBA SQ2, PS1 one buzzy voice); BEEP by stage and line; **hatch = 1 bar of the full band**; needs → minor / tuba / WOBBLE 3 % / slower; sleep 60 bpm; the demon's 4th loop glitches; the pet-care boss leaks the band in | 1 |
| `MC_WORKSHOP`, `MC_WELD` (WORKSHOP) | RIVET; brutal → S3; the bead in the zone holds the groove; HEAT → tempo; out of zone = V:STUTTER; overheat = siren + rimshot | 1 |
| `MC_MG_*` (7) | Catch NANA (catches climb the scale), Whack galop, Dodge VAPULA circus (near miss V:CHIRP), Gallery RIVET, Derby (photo = STOP), Memory celesta (tuned cards), Wheel (arp on the ticks); tiers S1/S2-3/S4; last 10 s +15 % +1; TIME UP = STOP | 2 |
| `MC_FISH`, `MC_COOK` | fishing blues; a bite = pizzicato Hook head; TENSION → intensity · ragtime; CHOP = the knife drums; STIR = DECK 0.8-1.2×; FRY = HEAT | 2 |
| Jukebox, `MC_LIB_*` | any cue × Cute/Spooky/Rock/Metal/Lullaby/Piezo/**Generative**/**Classic** + **"Scratch it!"** + open in the Studio; lobby "shhh" I1 | 1/2 |
| free-play screens | SHOP (black market Em); APT_HILL bossa / APT_HELL lo-fi + window loops by `like_music` (possessed = retrograde, −5th); RESIDENT moods + care stings; ROMANCE (sus4→3; rejected = STOP + trombone); WEDDING (LOVE 3/4 organ + amen + TRUE NAMEs); BABY; ARGUMENT (L/R → unison); GRAVEYARD (ghost rite); places | 2 |
| `MC_STUDIO`, lobby, stages | Studio = silence or the edited song (EXACT); tuning noodle; Sugar Bowl, Disco Inferno, Graveyard, Hellfire, Dreamland; HYPE claps 128+, choir 192+, encore +1; DJ booth §17 | 1 |

### 15.6 Stingers (≈ 70, `common/stings.dms`; duck 6-9 dB; Q:beat; `STM_DIAL`/`STM_KEY`)

QUEST_NEW/STEP/DONE · CHAPTER_CLEAR · UNLOCK · FRIEND · LOVE · HEARTBREAK · TADA · FAIL · RIMSHOT · SAD_TROMBONE · SLIDE_UP/DOWN (GBA SQ1 sweep, PS1 glide) · BOING · HONK · KAZOO · BEEP (after ≥ 1 beat of silence) · **ITS_FINE** (cheery C with a wrong F♯ bass that decays last) · GASP · REVEAL · DUN_DUN_DUNN · HALLELUJAH · EVIL_LAUGH · SPLAT · IRIS · VICTORY (`MCF_ONCE`: the fix) · DEFEAT · CHOIR_AH · RANK_* · cards · `STG_VINYL_*`.

**24 fear stings:** Spiders crawl · Dark (heartbeat only) · Thunder · Clowns (minor calliope) · Ghosts · Heights slide · Deep Water · Vacuum (an endless riser) · Bees 7-bit · Needles · Mirrors (retrograde) · Alone (one voice, far left) · Public Speaking (feedback) · Bath · Dentists (saw drill) · Balloons · **Silence** (a cut + one heartbeat) · Holy Water · **Sunlight** (full-sugar HaHaland) · Bells · Exorcisms (a loud amen) · Hugs · **Cute Things** (the sugar-max Hook) · Tax Season (adding machine).

---

## 16. Authoring pipeline

### 16.1 Sources (`core/music/`, one directory per owner)

| Directory | Owner | Holds | Id ranges |
|---|---|---|---|
| `common/` | MU4d | inst, comps, grooves, fills, motifs, vinyl, trans, stings, fears, sfx_synth, story_cards | MC 176-199; STG 0-127; VINYL/MOTIF/INST |
| `world/` | MU4a | | MC 1-63 |
| `combat/` | MU4b | | MC 64-111 |
| `activity/` | MU4c | | MC 112-175 |
| `classic/` | MU1T | generated | MC 200-231 |
| SFX synth | — | | MC 232-255 |
| `gen/<area>/` | that area | curated musgen output | |
| `ids/<area>.lock` | that area | append-only locks | |

- **Lanes are today's MML dialect, unchanged.**
- **Blocks** (DG §11.1): `cue role node chords alt -> trans sting vinyl motif comp groove fill inst`, plus **`gen`**.
- **Lane extensions:** `{head}` `y` `j` `D` `E` `?` `%` `Q` `M` `X` `!` `K`; comp tones `R 3 5 7 r8 8va ~`.

```
cue MC_HAHALAND "HaHaland" bpm=124 key=F:major meter=4 dial=S1 range=S0..S2 hub
  fx reverb=room depth=1   bind MP_CROWD*0.5 +I2   rest every=4min bars=16
  role LEAD inst=toy_lead/sync_lead psg=SQ1 prio=12 on=I3 duck_dialog modemap quote
  role COUNTER inst=flute twin prio=6 pan=+10 on=I4
  role HARM inst=chip_chord comp psg=SQ2 on=I2   role BASS inst=tri_bass comp psg=WAVE on=I1
  role KIT kit=TOY dna psg=NOISE on=I2           role BIRDS bed=birds prio=1 on=I0
  node A anchor bars=8 i=I2..I5 in=F out=C
    chords | F . Dm . | Bb . C . | ...
    LEAD t124 o5 l8 {head c4 <a> c} f4 e f | a4 g f d4. r | ...
    HARM comp=pool:major_stabs  BASS comp=pool:root5  KIT dna=pool:toy_pop fill=1
    -> A2:55 B:30 GEN_C:25 BRK:15
  gen GEN_C tmpl=DEV motif=HOOK cad=HC shift=-1,-2 n=8 fam=2 roles=lead,counter energy=4
  trans -> MC_INVASION grid=bar type=vinyl:NEEDLE entry=A silence=1 fast_morph
  trans -> * grid=phrase type=cross:2
```

### 16.2 Tools (MU1T) and CI

**`tools/mmlc.py`** (Python 3, stdlib, integer-only like `sfxgen.py`):
1. **parse;** `--import` reads `songs.c` (`/*SONG:x*/`), the 9 `mml_<name>.c` files and the S1 synth strings (`/*SFXSYNTH:x*/`);
2. **validate:** lanes and variants sum to bars × meter × 48; locks; packs ≤ 1,024 (base ≤ 640, variant ≤ 384); headers ≤ 512; common ≤ 256; stingers ≤ 512;
3. **lint:** an anchor exists; all reachable; edges cadence-legal or `turn`; every intensity pickable; sung lanes onset-safe; heads in `{head}`; stingers ≤ 4 bars;
4. **expand** V3 ops into ALT bars (lever L1);
5. **compile:** tokens, CALL dedupe, PS1 per-cue banks, fitted `psg_env` and `spu_adsr`;
6. **emit:**
   - `core/gen/mus_data.c`, `mus_ids.h`, `mus_stems.c` (after I9);
   - `MUSIC.BIN` + `SPUBANK.BIN` as uncommitted build products in `ps1/build/` (like `TEXT.BIN`);
   - `build/mus_report.txt`.

**Other tools:**
- **`tools/musgen/`** (host C99 from `proto/`): run by an author through `mmlc --gen CUE`, never in CI. It realises the variants, applies the offline guard, writes `gen/<area>/<cue>.dms` and renders a 3-min morph WAV. The committed `.dms` is the source.
- **`tools/spubank/`**: host `adpcm.c` → `SPUBANK.BIN`, from the GBA's PCM.
- **`musrender`**: `plat_music_native.c --wav`.

**CI:**
- `check-gen` byte-identical (it never re-runs musgen);
- `xcheck.sh` builds all `mus_*.c` at `-Werror` on 3 compilers;
- a `ps1/CMakeLists.txt` hunk: the custom command + the filter `"/core/gen/mus_(data|stems)\\.c$"` + 2 `iso.xml` lines.

### 16.3 Listening renders (required by every content and backend PR)

- **Root:** `build/renders/` at the repo root, which is git-ignored (CF-36). WAVs are never committed; PRs attach them. The R0 references are committed as 64 kbps Opus under `docs/design/music/renders/R0/`. While this session lives, the scratchpad copy at `/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/music/renders/` mirrors them.
- **Naming:** `<package>/<cue>_<script>_<s>s_<target>.wav`, where target is:
  - `ref` (musrender);
  - `gba` (the mGBA harness `audio-dump`);
  - `ps1` (a PCSX-Redux SPU capture, *verified at MU3 start*), or until then `ref-ps1` (the `mus_synth` PS1 model).
- **Scripts:** steady, walk, morph (cute → spooky → metal → home), comic (stingers + vinyl).
- **Every wave-1 cue:** morph 180 s + comic 60 s.
- **MP4:** comic beats are muxed with harness shots by `ffmpeg` (present here) as `<cue>_comic_<target>.mp4`.
- **PR note:** a listening paragraph covering loop audibility, seams, identity, mix and scratch feel.
- **Reference:** `renders/R0/` holds the prototype's 4 dial-morph previews.

---

## 17. Player songs, Concert, DJ booth (MU5b)

**`DoomoSong` → a cue *view* (`mus_song.c`).** The resident `doomo_song_book[]` is read in place through a ≤ 32 B view, so player songs play anywhere, the overworld and the house radio included, with no scene scratch (CF-24). 4 patterns = 4 nodes; roles LEAD (`RF_SUNG`), HARM, BASS, KIT (`SONG_KIT_*` → `TIMBRE_KIT_*`).
- **EXACT** (default; the editor always previews it): the order list at V0, ±1 tick of today (T10).
- **LIVE** (`SONGF_LIVE = 32`; the FILE-tab "Live arrangement"; default in the Jukebox, the house radio, AU13 playlists and encores):
  - order adjacencies become edges; V0-V2;
  - **the lyric rule:** sung passes keep onset counts and positions, so only † ops;
  - chord charts from a 12-bit pitch-class histogram against 7 triads (≈ 200 cycles per slot);
  - the genre comp pools add COUNTER, PAD, ECHO, fills and DJ moments; stems by intensity; encore +1.
- **Choir:** GBA ≤ 3 voices (M+), PS1 ≤ 8, following the syllables at 3rds, 5ths and octaves (`choir_double()` moves here).

**The scratch lane (0 new bytes).** Drum-cell bits 0-3 = `SONG_DRUM_*` (0-8); **bits 4-7 = vinyl id**: 1 BABY, 2 FORWARD, 3 CHIRP, 4 TRANSFORMER, 5 TEAR, 6 FLARE, 7 SCRIBBLE, 8 WIKKA, 9 REWIND, 10 SPINBACK, 11 STOP, 12 NEEDLE, 13 STUTTER, 14 crackle toggle, 15 SLOWMO.
- `song_sanitize()` (`song_player.c:258`) → `FIX((*c & 15) >= SONG_DRUM_COUNT, *c &= 0xF0)`; the drum player reads `cell & 15`.
- `DoomoSong` stays 892 B. An old build zeroes any whole drum cell ≥ 9 in its sanitizer, so it loses a scratch *and* the hit under it. No save crosses builds, so this is accepted (CF-24).
- **In the cue:** a VINYL token on the step; TAPE where available (a song *is* a record); RESUME after, except REWIND (CONTINUE) and STOP (until the next note).
- **UI:** SELECT toggles the layer on the drum track; A cycles. `studio_compose` adds 1-2 scratches per pattern to Lo-fi, Punk, Industrial and Synthwave ("Hellvile Flow" by default). Unlock "Wikka Wikka" for placing 10.

**Concert.**
- `ConcertSong` progressions → chord charts over the genre pools (**one shared genre table** replaces the 3 copies).
- Melodies → LEAD; the 20-bar form → a linear graph with MARKs; encore = `KEYSH +1`, bpm × 1.1.
- `PEV_*` from beat events (SECTION = MARKER, END = CUE; ONSET/KICK/SNARE from a lane mask).
- The logic clock in rhythm modes; HYPE drives claps, choir and the encore.

**DJ booth** (`CMODE_DJ`, `concert_dj.c`):
- an AI DJ scratches by personality and genre; **battles** = alternating 4-bar solos;
- **"Take the decks"** drives `music_vinyl_live()`:
  - GBA: R hand, D-pad push/pull, A fader, B cut, L backspin;
  - PS1: the same + **the left stick as the platter** (position → offset, rate → speed), R1/L1/R2;
- scoring times the direction changes and fader clicks against the prompts;
- audio = TAPE of the song's last 0.5 s + SAMPLE (the chip "aah", the singer's chop).

---

## 18. Migration of the 28 songs

- **M1 (automatic):** `mmlc --import` makes the 19 + 9 songs `MCF_EXACT` classic cues (8-bar nodes, linear edges, 4 literal roles, the 24 instruments fitted, the K S H O C T P kit). `audio_music(SONG_x)` → the adaptive cue if one exists, else the classic. **T1 proves it.** Tokens are 3.7 KB against 10.2 KB of MML for the 19.
- **M4/M8:** each song is the A-material of its adaptive cue (§15; DG §12 + GEN on hubs).

| Step | Change |
|---|---|
| before M1 | AUD-SPLIT |
| M1 | call sites unchanged; the engine plays everything on the legacy bridge |
| M2 / M3 | the GBA, then the PS1, drop `songs.o` + the interpreter (D15); the D14 swap with M3 |
| M4 | the §9.3 hunks; `overworld.c` loses its 90-tick poll |
| M5 | the interpreter and `songs.c` are deleted; `audio_song_check/length` answer from the bank; T1 frozen |
| M6 | `song_player.c` → a thin layer (lyrics, choir rotation, `song_beat_age()` stay); `concert_song.c` keeps its generator; one drum implementation, one volume curve |

---

## 19. Tests (owner MU6 unless noted)

| # | Test | Asserts |
|---|---|---|
| T1 | `test_music_classic.c` | 28 songs × 1,500 ticks: engine = interpreter (onset ±1 tick, Hz ±1, wave class); frozen at M5 |
| T2 | `test_music_hash.c` + harness `music-hash CUE MIN` (MU2/MU3) | the ring-event hash of every cue (2 min, scripted) = `core/tests/data/music_golden.h` on gcc and clang; **in-ROM GBA and PS1 = native**; plus a native `DoomoVoice` golden |
| T3 | `test_music_rep.c` | the §8.5 gates on real events |
| T4 | `test_music_data.c` | checksum, bounds, lints; a 30-min random-param fuzz per cue (no crash, voices ≤ caps, every key-on closed within 16 bars, no overflow); **no under-run** at 15 Hz logic for 30 min, nor in a 0.9 s load after `music_prefill(1500)` with the densest cue and 128 entries; every `MC_*` used exists |
| T5 | GBA `profile`, `music_budget.txt` (MU2) | heaviest cue + 4 SFX, with soft-voice eviction (CF-8): L ≤ 12.1k (idle ≤ 0.4k), L+ ≤ 12.7k, M ≤ 25k, S ≤ 47k, S+ ≤ 60k cycles per vblank including TAPE; the VBlank handler ≤ 64k; `music_update` ≤ 6k avg / 12k peak; `mus_drv` ≤ 1.0 % (CF-6). New tests T15-T19 are listed in CF-35. **Kernels measured before any promotion.** fps tour: tier L ≥ G0 − 0.3; promoted ≥ 20 (≥ 28 protected); UI ≥ 20 stays ≥ 20. |
| T6 | GBA `expect-audio` / `audio-dump` (MU2) | both FIFOs in M+ with L ≠ R; silent in STALL_SAVE; no PSG restart on volume (monotonic decays); stem audible (I9) |
| T7 | PS1 `audio_selftest()` (MU3) | bookshelf checksum; **page DMA during playback**; one KON per mt; reverb area in the map; TAPE SNR ≥ 15 dB; 240 ± 1 IRQs per second; **the driver advances during `CdReadSync`** |
| T8 | `test_music_comic.c` | pre-rolled stingers and vinyl land on frame × 8 exactly (native); `music_land` residual 0 within ±12 %; `{r}`: `music_gated()` for exactly 14 frames, then a quarter-downbeat resume; mute keeps the grid; the 2nd sting in a bar is refused |
| T9 | `test_music_vinyl.c` | BABY + RESUME returns to `ghost_u`; seek equivalence (1,000 random P per cue); TAPE dry events = deck A's events; pattern integrals = displacement; driver and arranger speed agree bit for bit |
| T10 | `test_studio.c` additions (applied by the studio owner) | EXACT = today ±1 tick; LIVE keeps sung onsets; the scratch nibble round-trips at the same size; one deliberate golden refresh at M6 |
| T11 | hooks | `music_gated()` feeds S2's `silence_budget`; the house's scripted beats are 20-25 % silent |
| T12 | memory gates (lines to the `memreport.py`/`elf2exe.py` owners via I9) | GBA: EWRAM ≤ +2,250 B with free ≥ 8,192; IWRAM ≤ +86 B with the stack ≥ 6,208; ROM ≤ 60 KB (phase 1). PS1: ≤ +9.5 KB at every milestone (≤ +7.5 between M6 and M7); ≥ 72 KB below the stack top; no group > 118,896 B |
| T13 | `test_music_drv.c` | arp step = 4 mt; ADSR; vibrato delay; slides; mute windows drop exactly [pos, pos+len); gain ramps; fence never passed; stable merge |
| T14 | `music_tour.txt` (native, GBA, PS1) | island + weather; an invasion spike; a cutscene with SYNC + NEEDLE; a shop round trip (resume); exorcism; the house (stems + cursed piece); the DJ platter; credits fast-forward; renders to `renders/MU6/` |

---

## 20. Budgets (m = measured, e = estimated, x = exact)

**GBA CPU** (1 % = 2,809 cycles per vblank):

| Item | Cost |
|---|---|
| arranger | ≈ 2.0k per tick, ≤ 12k peak (e); today 2.5-3k (m) |
| driver | ≤ 0.6 % (e) |
| mixer | per tier; L ≤ today (e, on the measured 0.3k-21.8k) |
| timer-1 | 0.17 % (e) |
| DECK | +1.3 % while scratching (e) |
| TAPE | ≈ 200k per scratch + 0.5 % (e) |
| stem | 0.06 % (e) |
| composer | **0** (phase B ≤ 0.13 %, m) |

**GBA EWRAM:**

| Item | Bytes |
|---|---:|
| core state | 1,122 (x) |
| rings | 576 (x) |
| driver voices | 256 (x) |
| soft voices | 240 (x) |
| FIFO A mono (from IWRAM) | 352 (x) |
| PSG shadow, governor | 48 (e) |
| retire `Chan ch[4]` etc. at M2 | −378 (m) |
| **M2** | **+2,216** |
| `song_player` state at M6 | −120 (m) |
| timer-1 queue at M7 | +128 (x) |
| **final** | **+2,224 → +2,320 after CF-12 (gate ≤ 2,350)** |
| stems (I9) | +360 (≤ 2,610, #77 sign-off) |

Borrowed in 2D only: FIFO growth, tape 4 KB, echo 4 KB. Fallbacks: ring A 32 (−128), 1-cue session (−46), beat ring 6 (−24), deck B 3 slots (−28).

**GBA IWRAM:** `irq.iwram.o` today is 1,292 B (484 code + 352 acc + 352 mixbuf + 104 voices; m) → 1,250 code + 128 acc = **1,378 (+86)**. The rich kernels (≈ 1.0 KB) wait for #78 (−1,092 B).

**GBA ROM, phase 1:**

| Item | KB |
|---|---:|
| `mus_*` 13.8 + `mus_song` 2.0 | +15.8 (e) |
| GBA audio v2 | +5.3 (e) |
| score | +30 (e) |
| samples | +20 (e) |
| `songs.o` 11.5 (m) + interpreter 3.5 | −15 |
| **net** | **≈ +56 (≤ 60)** |

After I9's 8 MiB: score + GEN ≈ 175 KB, samples ≈ 330 KB, stems ≤ 1.2 MB. If 4 MiB stays: ≤ +60 KB more, GEN at 4 variants, no stems.

**PS1 CPU:** ≈ 0.6 % typical; DECK +0.6 %; TAPE 26 ms per scratch; paging 0.5 ms per node; prefill 2-3 ms per load (e).

**PS1 resident (the C5 ledger line):**

| Item | KB |
|---|---:|
| `mus_*` MIPS -Os (13.8 × 1.30) | +17.9 (e) |
| `spu_music.c` over today | +2.1 (e) |
| state (1,122 + 1,280 + 320) | +2.7 (x) |
| window | +4.6 (x) |
| `songs.c.obj` | −11.9 (m: 11,856 B) |
| interpreter (60 % of `audio.c.obj` 9,358 B) | −5.6 (e) |
| D14 strings | −1.5 (e) |
| **M3** | **+8.3 (≤ 9.5)** |
| `mus_song.c` | +2.6 (e) |
| `song_player` renderer (13,144 B resident, m) | −3.0 (e) |
| `concert_song` renderer (3,716 B, m) | −1.0 (e) |
| **after M6** | **+6.9 (≤ 7.5)** |
| PS1 TAPE (lean `mus_synth` 2.0 + encoder 0.6) at M7 | +2.6 (e) |
| **final** | **+9.5 (≤ 9.5), superseded by CF-15: +13.5 with PS1 TAPE / +9.1 DECK-only (M3 +11.3, after M6 +9.9)** |

- TAPE cannot live in overlays: the resident `mus_vinyl.c` triggers it (SS C6). Only `concert_dj.c`/`studio_scratch.c` are group code.
- **Levers:** pre-realise SPLIT/MERGE/MORDENT (−0.8); ring 96 (−0.25); **drop PS1 TAPE** (−2.6: DECK + the running reverb's smeared tails + crackle; the brief allows sequencer scrubbing on the PS1). The D23 credit goes to the joint ledger.
- **Precondition:** SFX.BIN or an I9 allowance.

**SPU RAM:** §11.2, ≈ 4 KB spare at full content. **Disc:** `MUSIC.BIN` ≤ 176 KB + `SPUBANK.BIN` ≈ 120 KB; capacity is not a constraint (SS C20).

---

## 21. Work packages and file ownership (one shared tree, disjoint files)

### 21.1 Rules and common acceptance (MCA)

**Rules:**
- One owner per file. A **hunk** is one marked block `/* --- music: <purpose> --- */ … /* --- end music --- */`, in its owner's review, in §21.3's order.
- Hand-overs happen only at the named step, with sign-off: `audio_seq.c`, `gba/src/audio.c`, `ps1/src/spu.c`, `song_player.c`, `concert_song.c`. Their teams' tasks (#6, #13, #41-48) are complete.
- Generated files belong to MU1T's tool, and their content to whoever wrote the input. Fix lints in the input.

**MCA, for every merge:**
1. `make -C core test` passes, at 240×160 and 320×240, and under `SAN=1`; `xcheck` (`-Werror`) and `check-gen` pass.
2. `make -C gba` and `make -C gba test` pass, with the T12 GBA gates.
3. The PS1 disc builds; `ovl_plan` shows no group > 118,896 B; ≥ 72 KB below the stack top; the music delta ≤ its §20 line.
4. **fps:** UI scenes at ≥ 20 fps at G0 stay ≥ 20 with music running; tier L ≥ G0 − 0.3 fps (PS1 −1); 3D ≥ G0 − 0.3.
5. GitHub CI (`core`, `gba`, `ps1`) is green.
6. Listening renders under `renders/<package>/` (WAV; MP4 for comic beats) + a listening note.
7. New shots are looked at by eye at both resolutions; no floats, `malloc` or stdio in `core/`; integer only; the 30 Hz tick.
8. Owned files + listed hunks only.

### 21.2 Packages

**MU0: contracts.**
- **Owns:** `core/include/doomo/music.h`, `mus_data.h`; `core/src/plat_weak_music.c`; the `mus_int.h` skeleton; the `core/music/ids/*.lock` seeds; `core/tests/test_music_contracts.c`.
- **Hunks:** rows 2-6. AUD-SPLIT (row 1) runs alongside.
- **Accept:**
  - every size asserted: Bank 52, DirEnt 8, Cue 24, StingHdr 8, Role 8, Node 12, Edge 4, Bind 4, Inst 20, Groove 64, Trans 8, ParamDef 4, GenSpec 12, MusEv 8, Voice 12, VoicePlan 4, Caps 12, VoiceChange 20, MusPos 20, BeatEv 12, VoiceRt 28, DrvVoice 16;
  - `MP_COUNT == 32`; no `music_tick` symbol;
  - **no behaviour change.**

**MU1: core runtime.**
- **Owns:** `core/src/mus_{core,lane,var,graph,vinyl,drv,harm,bind,synth}.c`; `mus_int.h` and `music.h` (maintainer); `audio_seq.c`; `core/native/plat_music_native.c`; `docs/AUDIO.md`.
- **Hunks:** rows 7-8.
- **Milestones:**
  - **M1:** engine, rings, driver, legacy bridge, classic cues;
  - **M4:** matrix, stingers, comic bridge, DECK vinyl, GEN bags, chapters, twin, quotes, beds;
  - **M5:** retire the interpreter;
  - **M7:** TAPE feed.
- **Depends:** MU0, AUD-SPLIT, MU1T M1.
- **Accept:**
  - T1, T2-native, T4 and T13 at M1; T8 and T9 at M4;
  - `music_update` ≤ 6k/12k and `mus_drv` ≤ 0.6 % (via T5);
  - `size` reported in the PR: **PS1 MIPS resident ≤ 18.5 KB**, or the §20 levers.

**MU1T: toolchain.**
- **Owns:** `tools/mmlc.py`; `tools/musgen/*`; `tools/spubank/*`; `core/music/classic/`; the generated `core/gen/mus_*`.
- **Hunks:** row 21.
- **Milestones:** **M1** import, compile, check-gen, musrender; **M1.5** musgen + guard + turnarounds, with GEN graphs for TITLE, HAHALAND, INVASION and HOME.
- **Accept:** a lossless import (T1); **musgen reproduces `37d71b12 8d898ef0 c33cfa26 bdcb0bd2` on gcc and clang**; `mus_report.txt`; 4 renders.

**MU2: GBA backend.**
- **Owns:** `gba/src/audio.c`, `audio_int.h` (handed over); new `mixer.iwram.c`, `mixer_loops.s`; `gba/harness/scripts/music_budget.txt`.
- **Hunks:** rows 16-18, 22.
- **Milestones:**
  - **M2:** PSG v2, the lean mixer ≤ 1,378 B IWRAM, packed stereo, tiers L/L+/M, the governor, the VBlank driver, legacy off on GBA;
  - **M7:** rich kernels (after #78), TAPE/echo, crackle synth, S/S+, timer-1;
  - **after I9:** stems and kits.
- **Depends:** MU1 M1; #77 sign-off.
- **Accept:** T5, T6, T12-GBA, T2 in-ROM; `gba` vs `ref` renders of the 6 wave-1 hubs.

**MU3: PS1 backend.**
- **Owns:** new `ps1/src/spu_music.c` (planner, voice map, banks, KON batching, tables, reverb + freeze, NON, PMON, bookshelf + paging, SFX queue, TAPE voice 19, prefill, and the strong `plat_tone`/`plat_sample`/`plat_sample_ex`); `ps1/harness/scripts/music_tour.txt`. **`ps1/src/spu.c` stays CS P3's file** (CF-30, CF-42), and MU3 asks P3 for hunks.
- **Hunks:** rows 15, 19-20, 22.
- **Milestones:**
  - **M3:** first the SpuRead-during-playback check (else F-SPU); then the map, banks, bookshelf, root counter, prefill, reverb, locking, legacy off on PS1;
  - **M7:** TAPE + encoder, crackle, PMON options.
- **Depends:** MU1 M1; **SFX.BIN or an I9 allowance.**
- **Accept:** T7, T2 in-ROM, T12-PS1 (M3 ≤ +9.5 KB), the SPU map asserted at boot; renders.

**MU4a/b/c/d: the score, by area.** Each owns `core/music/<area>/`, `gen/<area>/` and `ids/<area>.lock`.

| Package | Wave-1 cues (M4) | Wave-1 budget |
|---|---|---:|
| **MU4d** common + story | banks (instruments, comps, grooves, fills, motifs, 16 vinyl, default transitions); ≈ 70 stingers + 24 fears; cards; ERUPTION, VAPULA_DOWN, WALL_FALLS, SUMMIT, FLASHBACK; the synth-SFX import; side-mission layers | ≤ 6 KB |
| **MU4a** world | TITLE, ISLAND, HAHALAND, HELLVILE, FESTIVAL/PARK, HOME, CREDITS, MINGLE, VAPE_LOUNGE, OMINOUS, BOOT (GEN on TITLE, ISLAND, HAHALAND, HOME) | ≤ 12 KB |
| **MU4b** combat | INVASION (GEN), COMBAT, HB1, HB2, BOSS_VAPULA, LEVEL_SELECT, ARENA, DEATH, RANK | ≤ 8 KB |
| **MU4c** activities | CHURCH, RITUAL, TRUE_NAME, EXORCISED, SOUL_LOST, PET, WORKSHOP, WELD, Jukebox, stages, STUDIO | ≤ 4 KB |

- **M8:** the rest of §15.
- **Depends:** MU1T M1; MU4d's banks; MU1 M4.
- **Accept:** lint clean; **T3 for all their cues**; morph + comic renders and a listening note per wave-1 cue; rejected-GEN counts in the PR.

**MU5a: game hookups.**
- **Owns:** only hunks, rows 9-14.
- **When:** M4, after each owner's milestone (S1 M3/M5; S3; H4 M2; custom's waves).
- **Accept:** T8, T11, T14; S1/S2/S3 runners unchanged apart from re-approved timing shots; `L_PUNCH` ticks equal on all three; Tier 0 still works where a hunk is pending.

**MU5b: Studio, Concert, DJ.**
- **Owns:** new `core/src/mus_song.c`, `concert_dj.c`, `studio_scratch.c`; `song_player.c` and `concert_song.c` (handed over).
- **Hunks:** rows 23-24.
- **When:** M6, after MU1 M4 and MU4d.
- **Accept:** T10; the T14 DJ path; the PS1 M6 line (net −1.4 KB); renders of a LIVE song, a scratch song and a DJ battle.

**MU6: tests and harness.**
- **Owns:** `core/tests/test_music_{classic,hash,rep,data,comic,vinyl,drv}.c`, `music_rep_lib.c` (the ported proto), `core/tests/data/music_golden.h` (record mode), `core/tests/scripts/music_tour.txt`, `gba/harness/scripts/music_tour.txt`.
- **When:** lands with the milestone each test gates (early behind `MUS_TEST_PENDING` if needed).
- **Accept:** deterministic; `SAN=1`; the 60-min T3 set < 30 s in CI; T12 lines sent to I9.

### 21.3 Hunks and hand-overs, in order

| # | File (owner) | Music change | When | Applied by |
|---:|---|---|---|---|
| 1 | `core/src/audio.c` → `audio_seq.c` (MU1 = CS's "audio" team, CF-30) | **AUD-SPLIT** (verbatim move) | before M1 | MU1, with I9 confirming the ownership |
| 2-6 | `platform.h` (S0/S1), `beat.h` (S1), `story_vm.h` (S2), `song.h` (studio), `stubs.c` (all) | the §9.4 block; `BOP_MUS_*`; `OP_MUS_*`; `SONGF_LIVE` + the nibble comment; weak `music_*` | M0 | MU0 / S1 / S2 / studio / MU0 |
| 7 | `audio_fx.c` (S1) | hooks → `music_*` + SFXSYNTH markers (M1); the D14 swap (M3) | M1, M3 | S1 |
| 8 | `audio.c` (MU1) | babble voice 20 + snap; `music_update` from `audio_tick`. CS P5's `send()` bus hunk and P3's `audio_sfx_pcm()` NULL-after-boot hunk land here too | M1 | MU1 |
| 9-10 | `beat.c` (S1); `scriptvm.c`, `textpack.py` (S2) | `BOP_MUS_*` + the 12-frame pre-roll; `OP_MUS_*` + notation | M4 | S1, S2 (MU5a supplies code + T8) |
| 11-14 | `ui_dialog.c`, `overworld.c` (S1); `levels*.c`, `arena_modes.c`, `exorcism_scene.c`, `pet_scene.c`, `credits.c` (S3); `combat_scene.c`, `fp_*.c` (custom/free play); `home*.c` (H1/H4) | the §9.3 rows | M4, after each owner's milestone | MU5a |
| 15 | `ps1/src/overlay.c`, `hcmd.c`, `harness/run.py` (H3) | prefill/stall; `music-hash`; capture | M3 | MU3 |
| 16-18 | `gba/src/irq.iwram.c`, `audio.c`, `audio_int.h` (GBA platform: **hand-over**); `save_flash.c` (custom C1); `harness.c`, `hcmd.c`, `gba/Makefile` (H2) | mixer out; STALL_SAVE; `audio-dump`, `music-hash`, tests | M2 | MU2 |
| 19-20 | `ps1/src/spu.c` (**CS P3**, no hand-over: weak `plat_tone`/`plat_sample` at M3, `SPU_SFX_END` = `0x0A000`, export `spu_sfx_addr()`, no reverb code); `timer.c`, `adpcm.c` (PS1 platform) | root counter 2; the streaming encoder | M3, M7 | P3 for `spu.c`, MU3 for the rest |
| 21 | `core/Makefile` (foundation); `ps1/CMakeLists.txt`, `ps1/iso.xml` (S2, SS C3) | a gen rule (`mmlc` before `textpack`, CF-44); build products, and filters for `songs.c`, `mml_*.c` and the `mus_*` generated files; 2 files | M1, M3 | MU1T supplies; foundation and S2 apply |
| 22 | `gba/tools/memreport.py`, `ps1/tools/elf2exe.py` | T12 lines | M2, M3 | I9 on MU6's request |
| 23-24 | `song_player.c`, `concert_song.c` (**hand-over**); `concert_scene.c`, `concert_int.h`, `studio_edit.c`, `studio_gen.c`, `lib_jukebox.c`, `test_studio.c`, `docs/STUDIO.md` | §17 hunks | M6 | MU5b (the test: studio owner) |
| 25 | `docs/GBA.md`, `docs/PS1.md`, `README.md` (I9) | audio sections | M7 | I9 |
| 26-39 | the further hunks of **CF-31**: `dream.c`, `arena.c`, `workshop.c`/`weld_game.c`, `mg*.c`, fish/cook, `lib_*.c`, freeplay `fp_*.c`, CS `fp_apartments`/`fp_resident`/`creator.c`, foundation `boot.c` + `scene.c` (`scene_top_arg()`), `txtload.c`/`rostxt.c`/`cdread.c` prefill, `memcard.c`, `bigbuf.c`, `unlocks.c`, `runner_main.c` | see CF-31 | see CF-31 | see CF-31 |

---

## 22. Order of work

```
Gates: G-A AUD-SPLIT · G-B CH SFX.BIN · G-C #77 sign-off · G-D #78 flash-to-stack · G-E I9 8 MiB
W1  MU0 ─► MU1 M1 + MU1T import + MU6 T1/T2/T4/T13   (engine on the legacy bridge, all 3 targets)
W2  MU1T M1.5 + T3 │ MU4d banks │ MU2 M2 (G-C) │ MU3 M3 (G-B + SpuRead check) │ MU4a/b/c authoring on native
W3  MU1 M4 │ MU5a hunks per owner milestone │ MU4 wave-1 merges │ T8 T9 T14
W4  M5 retire (MU1) │ M6 Studio/Concert/DJ (MU5b)
W5  M7 TAPE, crackle synth, rich kernels (G-D), S/S+, timer-1 │ stems + full kits (G-E)
W6  M8 wave-2 content │ optional phase B (D21)
```

- **Critical path:** MU0 → MU1 M1 → (MU2 ∥ MU3) → MU1 M4 → MU5a → M5.
- **Content** runs from W2 on native.
- **The game stays playable throughout:** M1 = everything plays through the engine; M2/M3 = native console rendering; M4 = adaptive cues via Tier 0 even without hunks.
- **One PR per package milestone.** Nobody edits another package's files to unblock.

---

## 23. Decisions needed, and risks

**Decisions needed:**

| From | Decision |
|---|---|
| custom | AUD-SPLIT (D13); SFX.BIN before M3 at `0x02000` (D16); bus ids; AU21 stays cut |
| C5/I9 | PS1 allowance ≤ +9.5 KB; the D23 credit; **the 8 MiB GBA ROM** |
| #77/#78 | EWRAM ≤ 2,250 B; the flash-to-stack move |
| S1 | beat ops; the `audio_fx.c` hooks and D14; dialogue and overworld hunks |
| S2 | VM ops and notation; **MINGLE and HEE HEE** (§14) |
| S3, H1/H4 | hunk slots; `MP_HARMONY` from Style |
| H2/H3 | harness commands; prefill; the house leaves ≥ 864 B of `gba_big` |
| studio, concert | the M6 hand-overs |

**Risks:**

| # | Risk | Mitigation |
|---|---|---|
| 1 | kernel cycles are estimates | T5 measures first; the governor adapts; tier L's margin is PCM 14 vs 26 c/s |
| 2 | PS1 code above ×1.30 | measure at M1; the levers; drop PS1 TAPE; F-SPU priced in |
| 3 | `SpuRead` during playback | checked first in M3; F-SPU |
| 4 | GEN noodling | head protection, V0 anchors, guard, curation, STRONG_CT/CAD_OK gates |
| 5 | hooks too literal | GEN share ≥ 30 %; ornament-blind M2; pool rotation |
| 6 | PS1 load under-run | prefill, T4/T7, the 256 fallback |
| 7 | comic sync on slow GBA logic | `BOP_MUS_SYNC` on audible events; exact at 30 Hz |
| 8 | real-clock divergence | presentation only; logic clock for rhythm; fixed look-ahead in hash mode |
| 9 | ROM budget | phase 1 ≤ 60 KB; I9 gates the rest; PSG tricks meanwhile |
| 10 | many cross-owner hunks | Tier 0; one block + one test each; the §21.3 order |
| 11 | unverified hardware tricks (PMON, repeat address, reverb freeze, nested IRQ) | optional, safe defaults, T6/T7 checks |
| 12 | ≈ 175 KB of score | classic import day one; GEN; wave 1 first |

---

## Appendix A: formulas

| Quantity | Formula |
|---|---|
| tempo | `upm_q16 = bpm_q4 × 65,536 × 48 / (16 × 14,400)` per mt |
| GBA cost | output 12N (stereo) or 7N (mono) + Σ (kN + 60 + 32⌈N/32⌉), k = 8 chip / 14 PCM, N = 176 / 224 / 304 / 352; 1 % = 2,809 per vblank |
| Δ per drawn frame | Δ per 2 vblanks × vblanks per frame / 2 |
| PS1 pitch | hz × cycle × 4096 / 44,100 (cycle 112 below 1.57 kHz, else 28) |
| root counter 2 | 33,868,800 / 8 / 17,640 = 240.0 Hz |
| vinyl | Δpitch16 = 192 log2\|v\|; gain = min(1, 2\|v\|) |
| chapter | picks / 5 |
| pool | `mus_h(seed, S_POOL, e, (chapter + e) / 5) % n` |
| GEN pick | `bag(mus_h(seed, S_GEN, node, pass / n), pass % n)` |

## Appendix B: evidence (`scratchpad/music/`)

| Source | Result |
|---|---|
| `proto/` composer on GBA (mGBA, THUMB -O2 from ROM) | **15.7k-29.6k cycles per bar** (58k worst); 0.04-0.13 % CPU; 1,060 B stack; 19.1 KB THUMB; 18.6 KB MIPS -Os |
| `proto/` determinism | hashes `37d71b12 8d898ef0 c33cfa26 bdcb0bd2` equal on GBA, x86 gcc and clang |
| `proto/out/metrics.txt` (10 min × 8 seeds × 4 themes × 3 scripts) | full-mix repeat ≤ 4 bars (today 294); 100 % distinct 8-bar windows; head gap 20-24; cadences 97-100 %; 60 min: full ≤ 4, pitched ≤ 5. **Caveat (CF-37):** the proto's full-mix and lead signatures hash `vel >> 2` with ±2 lead-velocity jitter (`mus_gen.c:562-566`, `:738-739`), so those figures must be re-measured without velocity. The pitched figure (`lr60.c:11`: no drums, no velocity) stands |
| `renders/R0/*_morph_120s.wav` | 4 dial-morph listening references |
| `build/tokproto.py` | tokens = 0.43 × MML |
| `build/bardedupe.py` | lead bars 86-88 % unique, harmony 42-63 %, bass 34-56 %, drums 22 % (so bytes go to melodies) |
| `build/graphsim2.py` | M1 ≤ 3, M2 ≤ 8 over 60 min (symbolic) |
| `build/graphsim*.py` | hard no-repeat rules lock graphs into cycles (1,834 of 1,860 bars) |

---

## Critique fixes (adversarial review, 2026-10-18 20:15-20:50)

**Scope.** This review was done read-only. It checked the SPEC against the working tree at 20:15 (HEAD `8cc7910` plus S2's uncommitted work), against `../custom/SPEC.md` (final, 19:54, "CS") with its two appendices, and against `../story/SPEC.md` ("SS"). Nothing in the repo was edited, built or git-touched.

**Status.** Each fix below is **binding** and outranks every section it names (see Precedence). Text that was edited in place carries its `CF-n` number. When an earlier section and a CF disagree, the CF wins.

### CF-A. Stale coordination facts

**CF-1. The custom SPEC is final, and some numbers changed.** `../custom/SPEC.md` exists (final, 19:54). It supersedes `design_hybrid.md`, so read every "CH" in this file as **CS**. The SFX move changes:
- **Who:** CS **P3** moves the SFX PCM to the SPU.
- **Net PS1 effect:** **−43,413 B** (CS §2 table: +2,880 B wave table and +1,500 B loader). The −47,796 B figure is out of date.

CS also changes who owns these files:
- **P3:** `ps1/src/{model,prims,spu,rostxt,cdread}.c`, `tools/sfxgen.py`, `core/gen/sfx_{info,pcm}.c`.
- **P4:** `creator*.c`.
- **P5:** `combat{,_hud,_scene}.c`, `actor.c`, `options.c`, `opt_rules.c`, `audio_bus.c`, `fp_options.c`, `input.c`, `ps1/src/pad.c`.

CS names an **"audio" team** (CS §5.4, C10 rule 2) that owns `audio.c` and receives the hunks for GR11, AU01, AU09, AU10, AU13, AU15, AU23, CH21 and CH22. **That team is the music packages** (CF-30).

CS baselines for GBA EWRAM: 19,773 B free after customisation, 16,341 B after `HOME`, gate 12,352 B.

**CF-2. S2 has shipped MINGLE and HEE HEE, so they are canon.** Both files landed at about 19:30: `core/src/mml_mingle.c` and `core/src/mml_heehee_jingle.c`. §14's two "proposals" are void.
- **MINGLE** is in F major, tempo 96, on the @12 music box. Its head is **C-F-A-F** (`c f a f >c4< a4 | b- a g f g4 c4 | …`), and the file shows its uses: P0, the Mayor's hum in Ep 1.6, and F1 in sepia.
- **HEE HEE** is a bell, `e r e r | c+ r c+ r`, over A → G♯, at tempo 120.
- **The "shared cell" story is narrowed (§14 edited).** E-E-G still links HOOK and RIFF. MINGLE is not built on that cell; it is the *destination*: the finale's +1 lift from E lands in F, which is MINGLE's key.
- **What changes for MU4:** MU4a/MU4d quote the shipped tunes. The Mayor's hummed fragment is MINGLE's bar 1. The rite reversed is MINGLE retrograded.

**CF-3. The synth SFX are tables, not strings.** In `audio_fx.c:122-139` the 15 recipes are `SynthSeg` tables of `{t0, n, a, b, wc, vol}`. The file's own header says they take about 0.4 KB of ROM. No `/*SFXSYNTH:x*/` markers exist, and none are needed.
- **What D14 now means (edited):** `mmlc --import` reads the C initialisers. `audio_sfx_synth()` keeps its signature and every caller stays as it is (`beat.c`, `scriptvm.c`, `ui_dialog.c`, `overworld*.c`, `story_timing.c`, `story_verbs.c`). At M3, one S1 hunk makes its body forward to `music_sfx_synth()`.
- **PS1 credit:** −0.3 KB, not −1.5 KB.

**CF-4. Seven of the nine story songs are still silent stubs.** `stubs.c:1267-1274` holds them; each is `"t120 v0 l8 [c]16"`. Only MINGLE and HEE HEE are real.
- **T1** imports all 28 songs; 7 of them import as silent classic cues, and that is allowed.
- **After M5,** each `mml_<name>.c` stays an owner input (S2, S4, S5) that `mmlc` compiles.
- **The console links drop them from M2/M3.** The PS1 CMake filter and the `gba/Makefile` filter list `songs.c` and `mml_*.c` (rows 16-18 and 21). The `doomo_mml_*` strings then cost 0 B, and SS C5's "+5.5 KB resident MML" line returns to the joint ledger in full (D23).

**CF-5. On the PS1, resident music code may not call group code (SS C6, `ps1/tools/ovl_plan.py:43-72`).** The groups that matter here:
- **story group:** `story.c`, `story_*.c`, `overworld*.c`, `credits.c`;
- **island group:** `fp_island_gfx.c`;
- **pet group:** `pet.c`;
- **combat group:** `levels*.c`, `combat*.c`, `arena*.c`.

So `mus_bind.c` must **never call** `story_beat()`, `story_chapter()`, `ow_music_for_map()`, `fp_island_weather()`, `pet_*()` or `lv_*()`. It reads **resident data and functions only**:
- `doomo_town` fields (`story.chapter`, `story.map`, `day`, `minute`, `seed`);
- `town_hour()`, `town_is_night()`, `sim_date()`, `opt_get()`, `scene_top()`, `map_current_id()`;
- the new `scene_top_arg()` (one foundation hunk in `scene.c`/`scene.h`, row 35).

**"Tier 0 supplies weather" was false.** `fp_island_weather()` (`fp_island_gfx.c:1258`) is island-group code, and `doomo_town` has no weather field. The freeplay team is asked to move the function into resident `fp_common.c` (row 32). Until then, MP_WEATHER comes from the `fp_island` hunk and from S1's `overworld_fx` only.

### CF-B. Budgets recomputed from the code

**CF-6. GBA CPU: the whole audio path, not just the mixer.**

The figures below are per vblank (1 % = 2,809 cycles). "m" means measured, "e" estimated.

| Item | Today | New, tier L |
|---|---:|---:|
| arranger (main thread) | 30 Hz sequencer 2.5-3k per tick ≈ 1.25-1.5k (m) | 2.0k per tick ≈ 1.0k (e) |
| driver (IRQ, THUMB from ROM) | 0 | **0.8-1.6k** in L (4 mt × 40 c, events, and **4-6 modulated voices × 120-200 c per control frame**; that last term was missing from the old formula); ≤ 2.8k in S+ (e) |
| mixer, 0 SFX | 0.3k (m; `irq.iwram.c` short-circuits when nothing is active) | **0.3k**: today's short-circuit is kept; crackle runs only while a cue `fx` or a vinyl asks for it |
| mixer, 1-4 SFX | 8.1 / 12.7 / 17.2 / 21.8k (m) | 3.9 / 6.7 / 9.4 / 12.1k (e) |
| **total, 0 SFX** | 1.55-1.8k | **2.1-2.9k (+0.1-0.5 %)** |
| total, 1 SFX | 9.35-9.6k | 5.7-6.5k (lower than today; the gap widens with each SFX) |

What this means for fps:
- **3D** pays +0.3-1.35k per vblank × 5.5 vblanks ≈ +0.1-0.45 ms of a 70-96 ms frame, which is −0.07 fps or less and passes the T5 gate of "≥ G0 − 0.3".
- **Edge scenes** are scenes whose G0 work per drawn tick sits within 0.4 ms below a vblank multiple. Apartments is one: 30 fps at 33.4 ms of work, with the 2-vblank line at 33.48 ms. For those scenes +0.1 ms costs a whole vblank, 30 → 20 fps.
- **Fix for edge scenes:** a new profile `AP_UI_EDGE` runs control frames every 8 mt, which is today's 30 Hz cadence for PSG arps and envelopes, with 4 PSG voices only. The driver then costs ≤ 0.6k, and the total is at or below today's. I9's G0 fps tour marks which scenes are edge scenes, and `mus_bind` reads that list from a table.
- **Gates:** `mus_drv` ≤ **1.0 %** (was an unbudgeted 0.6 %, §6.2 edited); and every scene must stay ≥ G0 − 0.3 fps.

**CF-7. The governor must judge by measured fps.** A GBA frame takes at least 2 vblanks (33.48 ms) and flips only on a vblank (`main.c video_flip()`). A frame that goes over waits for the 3rd vblank, which means 19.9 fps.

The old predictions are therefore wrong. Two examples:
- **Title logo:** 34.4 ms + 3.7 ms (S) = 38.1 ms, so almost every frame takes 3 vblanks and the scene runs at 19.9 fps. That breaks "UI ≥ 20".
- **Studio play:** S+ gives 40.1 ms, which is also 19.9 fps.

**How the governor works now:**
1. **Pre-filter.** Rules 1-3 still decide which scenes may *try* a higher tier.
2. **Trial.** It promotes one tier at a phrase boundary.
3. **Measure.** It measures drawn fps over the next 64 drawn frames as Δ`gba_mailbox.drawn` × 59.73 / Δ`gba_vblanks`. It uses only **windowed deltas**: `work_sum` is a u32 that wraps, and the harness zeroes `work_max` (`harness.c:517`).
4. **Keep or revert.** It keeps the tier only if fps ≥ max(20.5, G0 − 1.0) for UI-class scenes, ≥ 28 for protected scenes, and ≥ G0 − 0.3 for the rest. Otherwise it reverts and locks that tier for the rest of the scene visit.

This needs no `main.c` hunk. §10.3 is edited, and the predicted table is replaced there.

**CF-8. The S+ figures did not add up.** The tier table assumes 2 SFX, but T5 tests with 4. S+ with 4 SFX is 4,224 + 29,052 + 10,680 + 2,464 + 21,360 = **67.8k**, which is over the old 58k gate. TAPE adds 7.5k on top.

**How tiers work now:**
- A tier is a **cycle cap**.
- Each PCM SFX beyond 2 evicts the lowest-priority soft music voices first. One PCM voice costs about 1.65 chip voices.
- Deck-A voices that are muted are **not rendered**, so TAPE replaces them rather than adding to them.

**New T5 caps** (heaviest cue + 4 SFX, eviction on):

| Case | Cap (cycles) |
|---|---:|
| L | ≤ 12.1k |
| L idle | ≤ 0.4k |
| L+ | ≤ 12.7k |
| M | ≤ 25k |
| S | ≤ 47k |
| S+, TAPE included | ≤ 60k |
| **VBlank handler** (driver + mix) | **≤ 64k** |

The VBlank-handler cap exists because VBlank lasts 83,776 cycles. The handler must leave at least 19k cycles for `postfx_vblank()`'s OAM DMA and palette writes, and it must end before timer 1's first IRQ (CF-10).

**CF-9. Packed stereo needs headroom.** The mixer packs both channels into one 32-bit accumulator as `acc += s × (volL | volR << 16)`. That is exact only while |Σ s·volL| < 32,768. With 8-bit samples (±127) and volumes 0..63, that limit holds for just 4 voices; beyond it the left sum carries into the right half and both channels distort.

The rule:
- every voice keeps **|s × vol| ≤ 2,047**;
- 8-bit PCM uses volumes 0..15;
- chip waves use ±31 amplitude with volumes 0..63;
- bus gains are applied when the packed volume is built.

Sixteen voices then stay under 32,768. **T6 adds a crosstalk check:** a hard-left voice must leave the right channel at exactly 0.

**CF-10. The IRQ path has its own small budget.**
- **Stack.** `mus_drv` runs in IRQ mode on the **672 B IRQ stack** (`gba.ld:13`; the BIOS pushes 24 B and `gba_irq_master` 8 B). The driver (THUMB from ROM) plus the mixer must fit in **≤ 480 B**, measured with `-fstack-usage` and checked by a canary at run time (T16).
- **Timer 1.** The timer-1 PSG micro-queue needs **no nesting and no `crt0.s` change**. The handler runs with IRQs disabled (`crt0.s:139-143`), and that stays as it is. Timer 1 cascades off timer 0 and fires at samples N/4, N/2 and 3N/4, the first about 4.19 ms (70k cycles) after the vblank. The VBlank handler (≤ 64k, CF-8) has finished by then.
- **Setup.** `irq.iwram.c` (MU2, handed over) sets `REG_IE |= IRQ_TIMER1` and re-arms the timer at each vblank. The sentence "IME re-enabled for timer 1 inside the mix" is removed (§10.4 edited).

**CF-11. `gba_big` space is tighter than assumed.** The overworld's lit-tile cache already uses **17,152 of 17,408 B** (`OwTile` is 268 B × 64 entries; `overworld.c:31-43`). There is no room for stereo buffers in the overworld.
- **Overworld and 3D: mono only.** They stay mono (≤ L+). The old "overworld M" cap is corrected in §10.3.
- **M+ needs whole buffers.** The tiers need *contiguous* FIFO A + B double buffers, not "growth" on top of the 352 B buffer: **896 / 1,216 / 1,408 B** for M / S / S+. These live in an **audio tail**, the last 1,408 B of `gba_big`.
- **Who leaves the tail free:** the house (15,488 B used) and r3d (≤ 13,312 B). TAPE and echo (8 KB) run only while `gba_big` is free or owned by audio.
- **Hunks in `bigbuf.c` (GBA platform, row 16-18):**
  - a `GBA_BIG_BY_AUDIO` owner;
  - every `gba_big_claim()` first calls `mixer_big_release()` *synchronously*: stop DMA2, echo and tape, then play mono from the 352 B resident FIFO A buffer starting at the next vblank;
  - `plat_save_scratch()` calls `music_stall(STALL_SAVE)` before it hands the block out, because the save codec uses the block's **last** bytes (`bigbuf.c` header), which are the audio tail.
- **Why the hook sits in `gba_big_claim()`:** `doomo_frame()` runs `audio_tick()` *before* `scene_update()` (`game.c:48-55`). A newly entered scene can claim the block before music sees the scene change, so the release cannot wait for `music_update()`.

**CF-12. GBA EWRAM line.** The changes from the old +2,224 B:

| Change | Bytes |
|---|---:|
| ring A 48 → 40 entries | −64 |
| ring B 24 → 32 entries | +64 |
| 4-entry bar log (CF-20) | +64 |
| player-song view (CF-24) | +32 |
| **final** | **+2,320 B** |

The gate becomes **≤ 2,350 B**, with #77's sign-off. After CS and `HOME`: 16,341 − 2,320 = 14,021 B, which is ≥ CS's 12,352 B gate. The −378 B retire splits in two: −358 B at M2 (the sequencer's `Chan` state) and −20 B at M6 (the GBA's `tone[4]`, still used by `plat_tone` clients until then; CF-21).

**CF-13. GBA IWRAM.** Music is the only package asking for IWRAM: SS C7 asks +0 and CS asks +0. The +86 B leaves the main stack at 6,234 B, still ≥ `GBA_STACK_SIZE` 0x1800 = 6,144 B (`gba.ld:26`). It needs #78's sign-off. **Fallback:** a lean set without the hard-sync kernel, with sync done in THUMB at about 2× cost, in S+ only, which gives +0 B.

**CF-14. GBA ROM, joint total.** SS C21 puts story plus customisation at about 4.07 MB. CS adds +180 KB, and music's phase 1 adds ≤ 60 KB. That makes about **4.13 MB of 4,194,304 B**, already past the 4,063,232 B warning line. I9 decides. Music's **phase-1 floor** is score 20 KB (GEN at 2 variants on 4 hubs) plus samples 8 KB, about **+36 KB** net.

**CF-15. PS1 resident ledger, recomputed.** The changes from §20 (KB):

| Line | KB |
|---|---:|
| §20 M3 as written | +8.3 |
| D14 credit −1.5 → −0.3 (CF-3) | +1.2 |
| **resident bank core** (e): vinyl patterns 0.32 (the IRQ driver reads them); the `(*, to)`/`(*, *)` transition rows ≤ 0.26; reharm 0.24, arp 0.13, turnarounds 0.40, cadence bitmaps 0.10, params + arcs 0.19. (The directory, about 300 × 8 B, and each cue's `(from, *)` rows are paged with the cue header page and cost nothing resident. This core was missing.) | +1.7 |
| bar log + player-song view | +0.1 |
| **M3** | **+11.3** |
| M6: `mus_song.c` +2.6; `song_player` −3.0; `concert_song` −1.0 | −1.4 |
| **after M6** | **+9.9** |
| M7 PS1 TAPE: `mus_synth` 2.0 + encoder 0.6 + render staging in 448-sample chunks 1.0 (0 if P3 exports `spu.c`'s `s_pcm` staging) | +3.6 |
| **final with PS1 TAPE** | **+13.5** |
| **final DECK-only** (no PS1 TAPE, pre-realise lever −0.8) | **+9.1** |

Two options for I9:
- **(A) Keep PS1 TAPE: +13.5 KB.** The joint headroom is 110,624 + 7,817 (CS net) − 11,264 (worst SS net) − 13,824 = **93,353 B**. That clears the 72 KB gate, but it sits below SS's "G0 − 12 KB" line (98,336 B), so I9 has to allow it.
- **(B) PS1 DECK-only vinyl: +9.1 KB.** This fits the original cap. For parity, the PS1 counterpart of GBA TAPE is DECK plus a reverb smear/freeze tail plus NON crackle. The brief explicitly allows "sequencer-level scrubbing" on PS1, and GBA TAPE itself runs only in 2D scenes where `gba_big` is free.

**Default: (B), unless I9 grants (A) by M7.** The PS1's richer layering (§11) is unaffected either way.

**CF-16. SPU RAM: the bookshelf overflowed.** The whole score is about 174.8 KB, roughly 179,000 B. The old 172 KiB shelf holds 176,128 B, so it overflowed with no margin. Meanwhile SFX ADPCM needs only about **27.6 KB**: 47,793 B of PCM × 16/28, plus padding; CS's own design says "about 27 KB at 0x02000".

The fix (§11.2 edited):
- the SFX region becomes `0x02000-0x09FFF` (32 KB), a P3 hunk setting `SPU_SFX_END` to `0x0A000`;
- every bank moves down 24 KB;
- the **bookshelf becomes `0x36000-0x66FFF` = 200,704 B**.

New CI lints: `MUSIC.BIN` ≤ 200,704 B, and SFX ADPCM ≤ 32,768 B (P3's `sfx_boot` asserts it).

**CS's reverb plan does not fit and is replaced.** CS's appendix line for AU11 places the reverb work area at `0x7e000`, which is 8,192 B. Room alone needs 0x26C0 = 9,920 B. Reverb registers are written only by `spu_music.c`, with the base at `0x67FC0`; AU11 and AU12 become caps (CF-23).

**CF-17. PS1 boot load.** `MUSIC.BIN` (≤ 196 KiB) and `SPUBANK.BIN` (about 120 KB) are each bigger than the overlay slot (118,896 B). They cannot be staged during the title either, because `fp_title` is overlay code that sits in the slot.
- **When:** during `SCENE_BOOT`, which is resident.
- **How:** in ≤ 64 KB chunks through the free slot, after P3's `SFX.BIN` and before the first group loads.
- **Time:** about 1.1 s, which is 316 KB at 300 KB/s plus seeks.

§11.3 is edited.

### CF-C. Runtime corrections

**CF-18. `audio_music()` keeps today's timing.** Today the switch is immediate (`audio.c:411`, "restarts only if different"). Two callers are authored to the frame: `beat.c:226` (`BOP_MUSIC`) and `scriptvm.c:453` (`OP_MUSIC`). For example, `cutscene.c` ERUPTION starts INVASION exactly at `L_PUNCH`. An `MQ_BAR` default would land punchlines up to 2 s late.

The rules:
- **Shape.** Every compatibility call is `music_set_cue(cue, MQ_NOW)`. The matrix may only pick a shape that *starts now*: CUT, VINYL, or a CROSS or FILL of ≤ 1 beat.
- **Longer grids** need an explicit `music_set_cue`, such as the overworld hunk's `MQ_PHRASE`.
- **Current song.** `audio_music_current()` returns the *last requested* `SONG_*` immediately.

Eleven prev-song save/restore sites rely on that last rule: `mg.c:228`, `mg_wheel.c:130`, `lib_jukebox.c:109`, `lib_common.c:661`, `studio.c:696`, `dream.c:290`, `fish_scene.c:231`, `cook_scene.c:415`, `concert_scene.c:292`, `song_player.c:832` and `levels.c:288`.

**CF-19. Look-ahead during vinyl.** "16 mt ahead during vinyl" under-runs on slow logic: in GBA 3D a tick gap reaches 14-33 mt.
- **Ring A** keeps its normal look-ahead during vinyl; a MUTE window silences it. CONTINUE still truncates it.
- **Ring B re-keys for patterns.** Patterns are deterministic, so their re-keys are precomputed in windows of clamp(gap + 8 mt, 16, 48 mt). Here gap is the largest `drv_pos` step seen over the last 16 `music_update()` calls.
- **Live DJ input** keeps 16 mt. It runs only in the Concert, where gaps are ≤ 12 mt.
- **Which roles re-key:** DECK re-keys only the **4 highest-priority pitched roles**; the others only bend.
- **Ring sizes:** GBA ring B grows from 24 to 32 entries and ring A shrinks from 48 to 40 (CF-12). T4 proves that 40 entries hold ≥ 0.8 s of the densest bar (70 events at 168 bpm), far more than the 139 ms worst gap.
- **Edits:** D5, §6.2 and §7.1.

**CF-20. Seeking needs a bar log.** The walk depends on its history (soft recency, bags, the anchor rules and chapters all read `hist[]` and `passes[]`), so it is deterministic but not stateless. Only the lanes are stateless, and only *given the bar header*.
- **What the log holds:** the last 4 bars, about 16 B each: node, pass, bar, stems, dial rank, key, `bpm_q4`, `bar_op`, and the comp, groove and fill picks. That is 64 B.
- **What it makes exact:** DECK running backwards across a bar line, TAPE at P − 0.5 s, and REWIND/CONTINUE up to 2 bars back.
- **Test:** T9's seek-equivalence test must change params at random bar lines, not hold them fixed.

**CF-21. Legacy `plat_tone` clients until M6.** Three callers drive `plat_tone()` directly today: `song_player.c:511`, `concert_song.c:460` and `audio_fx.c:235` (the synth). Babble uses channel 1.
- **Lifetime:** `plat_tone` stays on GBA and PS1 until M6, as a shim onto the PSG channels and SPU voices 0-3.
- **Hold:** a legacy client holds its channels from `audio_music(SONG_NONE)` until the next non-NONE request, and audio_fx's synth holds the channels in its `owns` mask.
- **Exclusion:** while a channel is held, the engine renders **nothing** on PSG channels 0-3 or SPU voices 0-3, beds included. PS1 beds live on voices 16-17 and keep playing; GBA beds pause.
- **Test:** T19 checks that no channel is written by two owners in the same frame.

**CF-22. Keep the introspection APIs working after `songs.o` goes.** These calls must keep answering after M2/M3:
- `audio_song_name`, `audio_song_length`, `audio_song_check`;
- `audio_channel_state`, `audio_note_count`, `audio_music_ticks`.

They are used by `lib_jukebox.c:194,241,260,282` and the tests. From M2 they read the bank:
- **length:** a classic cue reports its T1 pass length; an adaptive cue reports the current node's length, so the Jukebox progress bar loops once per node;
- **channel state:** `audio_channel_state(ch)` maps LEAD, HARM, BASS and KIT through `DoomoVoice` to Hz.

**The audio tests stay green through the shim** at every milestone: `test_audio.c` (MU6, as the audio team) and `test_audio_fx.c` (S1's). Tests that read MML internals are ported by MU6 at M5, with S1 reviewing.

**CF-23. Options, reconciled with CS's appendix.** Source: `SPEC_appendix_options.md:330-352, 381, 525-526`.

| Option | Consumer, binding |
|---|---|
| AU01 master | `audio_bus_gain(BUS_MASTER)` in `mus_drv`'s bus stage, *and* CS's `send()` hunk (babble, legacy) |
| AU09 output | GBA `audio.c` (MU2), `spu_music.c` (MU3), and `spu.c` SFX voices (P3) |
| AU10 speaker | CS's high-shelf cut **plus** this SPEC's bass `timbre_alt` and mono fold (the GBA speaker drops sound below ~200 Hz); MU2 |
| AU11 / AU12 | **`spu_music.c` only**, read as caps (Off / Room / Studio / Hall / Space echo; depth 0-3). CS's "work area 0x7e000" line is withdrawn (CF-16) |
| AU13 location music | `mus_bind`: *Jukebox playlist* = unlocked Jukebox cues + LIVE player songs; *Silence* = `MC_NONE` + beds. Not `audio.c` callers |
| AU14 invasion music | `mus_bind` dial lock: Metal, Cute remix = toy metal (S1 cap), Keep location theme. No `combat_scene.c` hunk |
| AU15 dynamic layers | `mus_core`: Off = I3. CS's "audio.c combat layer channel mute" line is obsolete |
| AU20 | the foundation scene manager sets `MP_MENU` |
| AU22 | S1: a duck window + `RF_DUCK_DIALOG` |
| AU24 | P5's `combat.c` delays the cue request |
| CH21 voice pitch | `audio.c` babble: the offset is applied *before* `music_snap_pitch()` |
| DS26 title theme | `OPT_TITLE_THEME`, 10 values → the MC_TITLE variant and dial (`mus_bind` reads `opt_get`; no hunk) |
| GR11, AU23, CH22 | S1's `audio_fx.c`, not music |

**CF-24. Player songs need no scene scratch.** `doomo_song_book[]` is resident (`song.h:81`, `song_book.c:27`: 6 × 892 B). `mus_song.c` therefore plays a `DoomoSong` *in place*, through a ≤ 32 B view in the core state, decoding its lanes from `notes[][][]` one bar at a time.

This makes AU13 playlists work in the overworld, whose scratch is S1's `OWLD`, and lets the house radio play without borrowing `SCENE_HOME`'s scratch. §17 is edited.

**The scratch nibble breaks old builds a little.** An old build's sanitizer zeroes any drum cell ≥ 9 (`song_player.c`, after line 258), so it loses a scratch *and the hit underneath it*. No save crosses builds, so this is accepted.

**CF-25. Rests and the silence budget.**
- `music_gated()` reports **scripted** silence only: mute windows, `[SILENCE]`, `[NO MUSIC]`, `MC_NONE`, and the STOP/TAPESTOP holds.
- The engine's own rests (`rest_policy`, idle rests) are reported by a new `music_resting()`.
- Engine rests are disabled while `beat_running()` or the VM holds the scene.

So S2's `silence_budget` test (20-25 % per chapter, SS §7) measures only authored silence.

### CF-D. Cue coverage: every scene, map and cutscene

**CF-26. Scene table.** `mus_bind.c` gets one row for each of the **56 scenes** in `scenes.h`. T15 walks `doomo_all_scenes[]`, so a new scene without a row fails CI.

In the table:
- **KEEP** means the playing cue continues.
- **W** means W1 (M4) or W2 (M8); until the cue exists, its classic fallback plays.
- **Owner** is who owns the hook-up, and "—" means none is needed.

| Scene | Cue / profile | W | Owner |
|---|---|---|---|
| BOOT | MC_BOOT | 1 | foundation, `boot.c:57` (row 35) |
| CONFIRM | KEEP, MP_MENU; a NEEDLE sting for destructive confirms | 1 | caller (freeplay `fp_saveload`) |
| TEXT_ENTRY, PICKER, UNLOCKS, OPTIONS, QUESTLOG | KEEP + MP_MENU (AU20); OPTIONS volume preview = a 1-bar loop of the playing cue | 1 | foundation, P5 |
| DIALOG | KEEP + MP_DIALOG | 1 | S1 |
| UI_GALLERY | KEEP (debug) | — | — |
| TITLE | MC_TITLE + DS26 variants; idle corruption | 1 | freeplay |
| NEW_ISLAND | MC_NEW_ISLAND (`SONG_CREATOR` in this scene) | 2 | freeplay |
| ISLAND | MC_ISLAND | 1 | freeplay |
| APARTMENTS | APT_HILL / APT_HELL | 2 | CS (`fp_apartments`) |
| ROSTER | KEEP + MP_MENU | 1 | — |
| RESIDENT | RESIDENT moods, ROMANCE, WEDDING, BABY, ARGUMENT; SAD → MC_SOUL_LOST variant | 2 | CS (`fp_resident`) |
| SHOP | MC_SHOP (lounge / black-market Em node) | 2 | freeplay |
| GRAVEYARD | MC_GRAVEYARD (ghost rite) | 2 | freeplay |
| NEWS | MC_NEWS | 2 | freeplay |
| SAVELOAD | KEEP; save = HOME sting, load = doorbell | 1 | freeplay |
| CREATOR | MC_CREATOR | 1 | CS P4 |
| EXORCISM | MC_CHURCH → MC_RITUAL (LOGIC) → TRUE_NAME / EXORCISED / SOUL_LOST | 1 | S3 |
| PET | MC_PET | 1 | S3 |
| COMBAT | by map (CF-27) | 1 | CS P5 |
| WORKSHOP / WELD_GAME | MC_WORKSHOP / MC_WELD | 1 | action (row 28) |
| ARENA | MC_ARENA | 1 | action (row 27) |
| STORY | by map and beat (CF-27) | 1 | S1 |
| CUTSCENE | by CUT id (CF-28) | 1/2 | S1, S2, S3 data |
| CREDITS | MC_CREDITS | 1 | S3 |
| MG_CATCH … MG_WHEEL (7) | MC_MG_* | 2 | minigames (row 29) |
| CONCERT | stages, LIVE songs, DJ | 1 | MU5b |
| DREAM | MC_DREAM (dial +2 per vignette; NIGHTMARE node) | 2 | S2 (row 26) |
| STUDIO | silence or the edited song (EXACT) | 1 | MU5b |
| LIBRARY + 6 wings | lobby "shhh" I1, KEEP in wings | 1 | library |
| LIB_JUKEBOX | Jukebox modes + "Scratch it!" | 1 | library (row 31) |
| FISHING / COOKING | MC_FISH / MC_COOK | 2 | fishcook (row 30) |
| LEVEL_SELECT | MC_LEVEL_SELECT | 1 | S3 |
| BREACH | MC_HB1-7 by map + boss cues | 1/2 | S3 |
| ARENA_MODE | BOSS_RUSH / HORDE_SHOP / TIME_TRIAL / MAYHEM | 2 | S3 |
| HOME | MC_HOME (`home_phase` → node; ZONE = room; home id → key/dial: Cottage and Towers 7B in C S0-1, Pits Flat in E minor S2, Park Plot in E major) | 1 | H4 (row 14) |
| HOME_MART | MC_SHOP: HomeMart muzak node / Black Market Em node (arg 0/1 via `scene_top_arg()`) | 2 | H4 |
| STORY_MG | KEEP + per-game MP_* from the registry row | 2 | S2 |
| STORY_TIMING | `AP_MUSIC`: deck A silent, crowd bed only. The metronome (15 frames per beat = 120 bpm at 30 Hz) must be the only pulse, and TIMING_DRIFT must stay audible | 1 | — (scene row only) |

**CF-27. Map table (12 MAPIDs, `map.h:90-97`).** Music is chosen by `map_current_id()` in SCENE_STORY, SCENE_COMBAT and SCENE_BREACH, not by the song alone.

The song alone gives the wrong answer. The maps' `music` lines (sewers HAHALAND_DAY, factory WORKSHOP, graveyard OMINOUS, cathedral CHURCH, underbelly HELLVILE) would resolve to the island, workshop, ominous, church and Hellvile hub cues.

**Breach songs are absorbed into the level's cue.** In BREACH, `lv_info[].hell_song` (`levels.c:18-48`: HELL_BREACH, INVASION, HELLVILE) means MP_HELL → 255 on the same MC_HBn, not a cue switch. `SONG_BOSS` means the level's boss cue. Its boss id comes from the S3 hunk; until then the classic BOSS plays.

| MAPID | Context | Cue | W |
|---|---|---|---|
| HAHALAND | story overworld | MC_HAHALAND (ZONE nodes; OMINOUS beats → MC_OMINOUS; night by HOUR) | 1 |
| HAHALAND_INVADED | story | MC_INVASION (overworld node set; DANGER from spawns) | 1 |
| HELLVILE | story and free-play visits | MC_HELLVILE | 1 |
| VAPE_CELLAR | Breach 1 | MC_HB1 → WRAITH | 1 |
| CLOUD_NINE_SPIRE | Breach 2 | MC_HB2 → MC_BOSS_VAPULA | 1 |
| ARENA | ARENA, ARENA_MODE | MC_ARENA (+ mode variants) | 1/2 |
| PARK_UNITED | story ending | MC_FESTIVAL / PARK | 1 |
| SUGAR_RUSH_SEWERS | Breach 3 | MC_HB3 → GUTTERKING | 2 |
| PLUSHIE_FACTORY | Breach 4 | MC_HB4 → FLUFFNUTS | 2 |
| GRAVEYARD_SHIFT | Breach 5 | MC_HB5 → WIDOW | 2 |
| BONE_CATHEDRAL | Breach 6 | MC_HB6 → BONEBOTTOM | 2 |
| HELLVILE_UNDERBELLY | Breach 7 | MC_HB7 → GRUMBLEGUTS | 2 |

**CF-28. Cutscene table (all 24 `CUT_*` ids, `story.h:120-131`).** Resident code cannot see which cutscene is playing until `scene_top_arg()` exists. So the cue rides in each stage's own `BeatEv` data as `BOP_MUS_CUE`, authored by the stage owners S1, S2 and S3. Until then, Tier 0 is the stage's own `BOP_MUSIC` song.

| CUT | Cue |
|---|---|
| CHAPTER1-4 | STG_CH1..4_CARD |
| ARRIVAL | MC_ARRIVAL |
| FOG | the FOG cue |
| ERUPTION | MC_ERUPTION (trailer s08) |
| VAPULA_DOWN | MC_VAPULA_DOWN |
| WALL_FALLS | MC_WALL_FALLS |
| FREEPLAY | MC_CREDITS "parade" node |
| HOME | the HOME cue (doorbell → BEEP) |
| FEAST_POP, FEAST_SAVE | MC_FEAST (pop / save branches) |
| P0 | MC_MINGLE (box) |
| F1-F4 | MC_FLASHBACK; F1 = MINGLE in sepia per `mml_mingle.c` |
| INT3_KNOCKS | `[NO MUSIC]` clock bed (exempt) |
| SUMMIT | MC_SUMMIT |
| SAYIT | **MC_SAYIT** (new, W2: HOME/MINGLE tender, per-friend motif quotes) |
| E1 | **MC_EPILOGUE** (new, W2: COMMINGLING in F + HOME) |
| BOTTLE, BALLOON | classic via the stage's `BOP_MUSIC` until S2/S3 name the material; MU4d authors them in W2 |

**CF-29. Song → cue resolution in `mus_bind`.** The key is (song, scene, map, arg), and an unmatched key falls back to the song's classic cue.
- **Minigames:** every MG_* with `SONG_FESTIVAL` or `VAPE_LOUNGE` → its MC_MG_* (`mg_*.c`). The `mg.hell` HELL_BREACH request → that game's cue at dial S4.
- **Fishing / cooking:** `SONG_HAHALAND_DAY` or `NIGHT` in FISHING → MC_FISH; `SONG_SHOP` in COOKING → MC_COOK.
- **Library lobby:** `SONG_HAHALAND_NIGHT` in LIBRARY → the lobby node.
- **Dream:** `SONG_HAHALAND_NIGHT` in DREAM → MC_DREAM; `SONG_OMINOUS` in DREAM → its NIGHTMARE node.
- **Concert stages:** `concert_song.c:987-989` (INVASION / HAHALAND) → the stage's cue.
- **Victory:** `SONG_VICTORY` → STG_VICTORY with `MCF_ONCE`.
- **Sad:** `SONG_SAD` in a death context → STOP + DEFEAT.

### CF-E. Ownership and hunks

**CF-30. Ownership conflicts, resolved.**

| File | This SPEC said | Other spec | Resolution |
|---|---|---|---|
| `ps1/src/spu.c` | handed to MU3 after SFX.BIN | CS P3 owns it (P3 file list) | **No hand-over.** P3 keeps `spu.c`. MU3's `spu_music.c` holds the strong `plat_tone`, `plat_sample`, `plat_sample_ex` and the reverb. P3 hunks at M3: make `plat_tone`/`plat_sample` weak, set `SPU_SFX_END` = `0x0A000`, export `spu_sfx_addr(id, &pitch)`, keep reverb code out, and optionally export the staging buffer (CF-42) |
| `core/src/audio.c` | "(custom)" | SS line 226: custom owns it after S1's hunk. CS: "audio" is another team and gets a `send()` hunk | **MU1 = CS's "audio" team.** It owns `audio.c` from G-A and applies S1's existing hunk, P5's `send()` bus hunk and P3's `audio_sfx_pcm()` NULL-after-boot hunk. I9 confirms in writing |
| `gba/src/audio.c`, `audio_int.h`, `irq.iwram.c` | handed to MU2 | CS gives AU09/AU10 to the "audio" team | consistent |
| `ps1/CMakeLists.txt`, `ps1/iso.xml` | MU1T applies | SS C3: S2 applies CMake/iso hunks (CS P3 asks S2 too) | **MU1T supplies; S2 applies** |
| `core/Makefile` | MU1T | SS: foundation owns it; S2 and H4 add a rule each | MU1T supplies one rule; foundation applies |
| `combat.c`, `combat_scene.c` | "custom" | CS P5 owns `combat{,_hud,_scene}.c`; SS gives S3 ≤ 3 hook hunks | MU5a asks P5; S3's hunks come first |
| `creator.c` | not listed | CS P4 | MU5a asks P4 (row 34) |
| `song_player.c`, `concert_song.c` | handed to MU5b at M6 | CS routes its hunks to "their own teams (wave 3)" | after M6, CS requests for these two files go to MU5b |
| `lib_jukebox.c` | MU5b hunks | CS P3: a `build_wave()` hunk (2,880 B wave table) | both are hunks; the library team applies them in order P3 (CS M1), then MU5b (M6) |
| `tools/sfxgen.py`, `core/gen/sfx_*.c` | read | CS P3 | read-only; `tools/spubank` reads CS's new `core/gen/sfx_pcm.c` |
| `core/src/stubs.c` | "all" | SS: the S0 block | MU0 adds its own marked music block |
| options AU* | "CH owns them" | CS P5: `options.c`, `opt_rules.c`, `audio_bus.c` | music reads getters only; CS's appendix consumer lines change per CF-23 (CS doc edit requested) |

**CF-31. Every audio call site and its owner.** This list comes from `grep -rn "audio_music\|audio_tapestop\|audio_sfx_synth\|audio_voice_char\|audio_song_"` over core, gba and ps1 sources. Each row below continues §21.3 under the same hunk rule. "✓" means Tier 0 keeps the file working unchanged through `audio_seq.c` and CF-18/27/29.

| # | File (owner) | Today | New calls (hunk) | When |
|---:|---|---|---|---|
| 9 | `beat.c` (S1) | `BOP_MUSIC`/`MUTE`/`DUCK`/`TAPESTOP`, synth SFX ✓ | `BOP_MUS_*`, the pre-roll | M4 |
| 11 | `ui_dialog.c` (S1) | duck; `{r}` = SCRATCH + tapestop 5 + mute 4 (`:352-360`); babble ✓ | NEEDLE, MP_DIALOG, AU22 | M4 |
| 11 | `overworld.c` (S1) | `ow_music_for_map` ×6, the border, the 90-tick poll ✓ | `cue_for`, the MP_* of CF-32 | M4 |
| — | `overworld_npc.c`, `overworld_spawn.c` (S1) | synth SFX, duck ✓ | none (D14 body swap) | — |
| — | `cutscene.c`, `cutscene_ch12.c` (S1, S2) | `BeatEv MUSIC` data ✓ | `BOP_MUS_CUE` in the stage tables (content) | M4+ |
| 10 | `scriptvm.c`, `textpack.py` (S2) | `OP_MUSIC`, `MUTE`, `DUCK`, `TAPESTOP`, `RESUME` ✓ | `OP_MUS_*` appended before `OP_COUNT` (CF-44) | M4 |
| 26 | `dream.c` (S2) | NIGHT/OMINOUS + prev restore ✓ | MC_DREAM, dial per vignette | W2 |
| — | `story_timing.c`, `story_verbs.c`, `story_mg.c` (S2) | synth SFX ✓ | STORY_MG MP_* per registry row (W2) | W2 |
| 12 | `credits.c`, `levels*.c`, `arena_modes.c`, `exorcism_scene.c`, `pet_scene.c` (S3) | ✓ | §9.3 rows; the boss cue id | M4 |
| 13 | `combat.c`, `combat_scene.c` (CS P5) | map music, VICTORY/SAD ✓ | DANGER, ENEMIES, HP, COMBO, TIMER; death STOP; AU24 | M4 |
| 27 | `arena.c` (action) | INVASION/SAD ✓ | MC_ARENA waves (+1 semitone per 5 waves, +2 bpm per wave) | W1 |
| 28 | `workshop.c`, `weld_game.c` (action) | WORKSHOP ✓ | MC_WELD: HEAT, in-zone groove, STUTTER, overheat | W1 |
| 29 | `mg.c`, `mg_wheel.c`, `mg_*.c` (minigames) | per-game song, hell, prev restore, once VICTORY ✓ | TIMER, catch steps, near-miss CHIRP, TIME UP STOP, last 10 s | W2 |
| 30 | `fish_scene.c`, `cook_scene.c` (fishcook) | ✓ | TENSION, the bite, CHOP, STIR = `music_vinyl_live` 0.8-1.2×, HEAT | W2 |
| 31 | `lib_common.c`, `lib_jukebox.c` (library) | lobby, Jukebox, `channel_state`, `song_length` ✓ (CF-22) | variants, "Scratch it!", open in Studio | W1, M6 |
| 32 | `fp_title.c`, `fp_newisland.c`, `fp_island.c`, `fp_shop.c`, `fp_graveyard.c`, `fp_saveload.c` (freeplay) | ✓ | title idle corruption + NEEDLE; save/load stings; move `fp_island_weather()` to `fp_common.c` (CF-5) | W1 |
| 33 | `fp_apartments.c`, `fp_resident.c` (CS) | ✓ | window loops by `like_music`; care stings; ROMANCE/WEDDING | W2 |
| 34 | `creator.c` (CS P4) | CREATOR, babble ✓ | hover/tab stems, dial on hellish creation, SCRIBBLE | W2 |
| 23-24 | `studio.c`, `concert_scene.c` (studio, concert); `song_player.c`, `concert_song.c` (→ MU5b) | ✓ (CF-21) | §17 | M6 |
| 35 | `boot.c`, `scene.c`/`scene.h` (foundation) | `SONG_NONE` | MC_BOOT; `int scene_top_arg(void)`; MP_MENU for AU20 | W1 |
| 36 | `ps1/src/txtload.c` (S2); `ps1/src/rostxt.c`, `cdread.c` (CS P3) | CD reads | `music_prefill(1500)` before each read | M3 |
| 37 | `ps1/src/memcard.c` (CS) | card I/O | `music_stall(STALL_CARD)` | M3 |
| 16-18 | `gba/src/bigbuf.c` (GBA platform) | `gba_big` claims | audio tail, `mixer_big_release()`, the `STALL_SAVE` in `plat_save_scratch()` (CF-11) | M2 |
| 38 | `unlocks.c` (integration) | toast | the UNLOCK sting | W1 |
| 39 | `core/native/runner_main.c` (foundation; S2 edits it) | — | `--wav`, `music-hash` | M1 |
| — | `gba/src/cutview.c`, `ps1/src/cutview.c` (platform) | `SONG_NONE` ✓ | none | — |
| — | tests: `test_audio.c` (MU6), `test_audio_fx.c`, `test_ow_script.c` (S1), `test_studio.c` (studio), `test_library.c` (library) | must stay green (CF-22) | ports at M5 only | M5 |

**CF-32. Who sets each MP_* parameter.** Every new `music_set_param` call has a named owner.

| Parameter | Setter |
|---|---|
| INTENSITY | any scene, through its row |
| DANGER, ENEMIES, HP, COMBO, TIMER | CS P5 `combat*.c`; S3 `levels*.c`, `arena_modes.c`; action `arena.c`; minigames (TIMER) |
| BOSS_PHASE, HELL, ALTITUDE | S3 `levels*.c` (hell × 255 / `hell_at`; Spire floors); P5 for single-map combat |
| INFECT, CROWD, WALL_DIST, ZONE (overworld), TRUST | S1 `overworld.c`, from `doomo_town.story` data that S2 owns |
| HOUR, SEASON | `mus_bind` (resident) |
| WEATHER | `mus_bind` after the CF-5 move; until then the freeplay `fp_island` hunk and S1 `overworld_fx` |
| ACTIVITY | freeplay `fp_island` |
| SANITY | S3 `exorcism_scene.c` |
| COMFORT, SPOOK, FORT, HARMONY, ZONE (house) | H4 `home*.c`, from H1's cached `HOME` results |
| PET_STAGE, PET_LINE | S3 `pet_scene.c` (`pet.c` is group code) |
| TENSION | fishcook `fish_scene.c` |
| HEAT | action `weld_game.c`; fishcook `cook_scene.c`; S3 `story_feast.c` (fullness) |
| HYPE | MU5b `concert_scene.c`, `concert_dj.c` |
| LOVE | CS `fp_resident.c`; S2 VM `OP_MUS_PARAM` |
| DIALOG | S1 `ui_dialog.c` |
| MENU | foundation scene manager (AU20) |
| QUEST | S2 VM (`story_side` family tag) |

### CF-F. Determinism and tests

**CF-33. "Hash mode" is defined.** `music_hash()` and T2 run with these settings:
- the LOGIC clock forced on;
- generation driven by logic ticks with a fixed 1-bar look-ahead, whatever the ring size;
- plan = ALL roles;
- no governor, caps or STALLs.

The hash is taken **at generation time, over the decision stream of every deck plus the MARKs, sorted by (pos, deck, voice)**, not at dispatch. GBA, PS1 and native therefore agree by construction.

REAL-clock play on the consoles is not reproducible against logic time, and that is accepted as presentation only. T8 and T9 run natively with exact clocks.

**CF-34. The plan does not change decisions.** Unrendered roles are *generated and then filtered*, never skipped during generation. T2b checks this: native decision hashes are identical under plan masks {ALL, GBA-L with 4 PSG, PS1 map}.

**CF-35. New tests (owner MU6 unless noted).**

| Test | What it checks |
|---|---|
| **T15** `test_music_coverage.c` | every scene in `doomo_all_scenes[]` has a row; every MAPID, every `CUT_*` and every `SONG_*` resolves (CF-26 to CF-29); every MP_* has a declared setter; every MC used by data exists |
| **T16** IRQ budget | GBA IRQ-stack canary ≤ 480 B; VBlank handler ≤ 64k cycles (`profile`); PS1 root-counter IRQ ≤ 2 % per tick |
| **T17** `gba_big` | a save during S+ Studio playback; Studio → overworld with M+ active; the house at S. No DMA reads a reclaimed buffer (canary pattern + `audio-dump` glitch detector) |
| **T18** metric sensitivity | CF-38 rule 6 |
| **T19** legacy coexistence | `song_player`, concert, synth SFX and babble running beside cues and beds: no PSG channel or SPU voice is written by two owners in one frame (native trace) |
| T9 (extended) | bar-log seeks under random param changes |
| T11 (extended) | `music_gated()` vs `music_resting()` (CF-25) |
| T6 (extended) | crosstalk (CF-9) |

**CF-36. Where renders go.** A `/tmp` scratchpad path cannot ship in a repo document. Renders go to `build/renders/<package>/` at the repo root, which is git-ignored (`.gitignore` covers `build/`); PRs attach them. The R0 references are committed as 64 kbps Opus files (about 1 MB each, made with `ffmpeg`) under `docs/design/music/renders/R0/`. **MU0 installs this SPEC as `docs/design/music/SPEC.md`**, as CS and SS do (CF-45).

### CF-G. The non-repetition metric, made meaningful

**CF-37. What was wrong with the old metric.**
1. **Velocity counted, against D9.** M1 hashed `vel >> 2`, although D9 says velocity is not variation. The proto does the same: its bar signature mixes `inst` and `vel >> 2` into every event, LEAD included (`proto/mus_gen.c:562-566`), and lead velocity is jittered by ±2 per bar (`:738-739`). So **Appendix B's full-mix and lead figures ("full ≤ 4 bars, 100 % distinct 8-bar windows") are partly velocity noise.** The pitched figure stands: `lr60.c:11` hashes pitched roles without drums or velocity, and gets ≤ 5 bars.
2. **Drum dice beat M1.** Probabilistic drum steps (`% 15 < p`) and ghost notes change M1 on almost every bar. A graph whose music truly loops still passes.
3. **Exact hashing is brittle.** One grace note in 32 bars defeats "no identical 32-bar M2 window".
4. **LRR has no sense of time.** It counts the longest repeat anywhere in 60 minutes. It treats a repeat 50 minutes later like a repeat 8 bars later, and it penalises anchors that are *meant* to recur.
5. **Texture is not measured.** Nothing catches the same stems, dial, key and tempo for ten minutes.
6. **There is no proof the metric catches a loop.**

**CF-38. The new metric (T3; replaces §8.5's signature list; §1 item 10 and §8.5 M1 edited).** Signatures are taken per deck-A bar from the hash-mode decision stream.

**Signatures:**
- **S (structural full mix).** Pitched roles as (role, onset on the 16th grid, pitch, length class), plus drum **core** steps (p = 15), fills, crashes, stems and dial band. It excludes velocity, ghost and probabilistic steps, microtiming, VIB, TIMBRE, ECHO and the GRACE, MORDENT and TURN ornaments.
- **P.** Pitched roles only, same fields.
- **L.** LEAD only.
- **F (fuzzy).** For each role, a 16-step onset bitmap and a 12-bit pitch-class set. Bar similarity is the weighted mean of Jaccard(onsets) × Jaccard(pitch classes), with weights LEAD 3, BASS 2, HARM 1, COUNTER 1, KIT 1 (core steps only). Two bars match when similarity ≥ 0.85.
- **Texture.** A vector per 8 bars: stem set, dial band, key, tempo band, LEAD register (mean pitch / 6 semitones), density band.

**Gates** (hubs, 60 min, 8 seeds × steady / walk / morph):
1. **Literal repeats:** LRR(S) ≤ 4; LRR(P) ≤ 8; LRR(L) ≤ 16. An anchor's first 4 bars are masked out of L only, because they are the identity.
2. **Near-loops:** for every lag d < 64 bars, no run longer than 8 bars in which each bar F-matches the bar d earlier.
3. **Recency:** in ≥ 95 % of 8-bar windows, the nearest earlier window with mean F ≥ 0.85 is ≥ 24 bars back (about 45 s at 124 bpm).
4. **Texture:** the texture vector changes at least once in ≥ 90 % of 32-bar spans, and the steady script reaches ≥ 4 distinct vectors per 10 minutes.
5. **Identity and form** (unchanged): HEAD_GAP ≤ 32; anchor literal ≥ 50 %; CAD_OK ≥ 95 %; seams = 0.
6. **Sensitivity (T18).** The metric must fail on known loops:
   - (a) today's classic HAHALAND_DAY must fail gates 1-3;
   - (b) a hub with GEN off and comp/groove rotation off, but drum dice on, must fail gate 1 or 2;
   - (c) A-B-A-B with only V1/V2 ornaments must fail gate 2;
   - (d) a 4-bar loop varied only in velocity must fail gate 1;
   - (e) the proto reference, re-measured with velocity removed, must pass.

   Any gate change that lets (a)-(d) pass is rejected.
7. **Listening still decides (MCA 6).** T3 lists the 5 worst near-repeat spans per cue with timestamps into the render, so reviewers listen exactly there.

### CF-H. Other technical corrections

**CF-39. WAVE 64-step mode and bank morphing are exclusive.** In 64-step mode both 32-sample banks play, so neither can be rewritten while the channel plays. The 64-step timbres (POWER5, VOWEL, ORGAN, GROWL, BELL, GROSS) therefore **cannot bank-morph**. Morphing timbres use the double-buffered 32-step mode, and the two modes switch only at a note-on that restarts the channel. §10.1 is edited.

**CF-40. PSG pitch floors under vinyl.** At slow platter speeds a note can fall below what the hardware can play: SQ1 and SQ2 bottom out at 64 Hz (`sq_rate`), and WAVE at 32 Hz in 32-step mode or 16 Hz in 64-step mode. Below its floor, a voice's gain ramps to 0 over the rest of the pattern instead of clamping at the floor pitch, so STOP and TAPESTOP do not "buzz" at the bottom. PS1 voices have no practical floor.

**CF-41. TAPE renders are spread out.**
- **GBA:** about 200k cycles per scratch, spread over the ≥ 3-tick pre-roll at ≤ 70k cycles per tick.
- **PS1:** about 26 ms, spread at ≤ 9 ms per tick.
- **Fallback:** if a scene's measured frame time plus that slice would cross a vblank multiple (CF-6/7), the scratch falls back to DECK.

**CF-42. PS1 symbol ownership.** The strong `plat_tone`, `plat_sample` and `plat_sample_ex` live in `spu_music.c`; `spu.c`'s copies become weak (a P3 hunk at M3, CF-30). SFX key-ons go through MU3's 16-entry queue into the batched KON. P3 keeps SFX upload and the address table, exported as `spu_sfx_addr()`. `audio_selftest()`'s music checks (T7) live in `spu_music.c`.

**CF-43. Story tests stay meaningful.** Covered by CF-25 (`silence_budget`) and CF-22 (`test_audio_fx.c`).

**CF-44. Build order and VM opcodes.**
- `textpack.py` must resolve `MC_*`, `STG_*`, `VINYL_*` and `MP_*` from the generated `core/gen/mus_ids.h`. So `mmlc` runs **before** `textpack` in `core/Makefile` (foundation) and in the PS1 CMake custom commands (S2).
- `OP_MUS_*` is **appended before `OP_COUNT`** (`story_vm.h:88`), because opcodes are bytecode inside committed packs.
- After the ops land, regenerating `core/gen/text_*.c` is S2's `check-gen` job.

**CF-45. Install the SPEC.** MU0's first commit copies this file to `docs/design/music/SPEC.md`.

**CF-46. Native platform.** `core/native/plat_native.c` (foundation) keeps `plat_tone` and `plat_sample` for the legacy path. `plat_music_native.c` (MU1) adds the music hooks. `runner_main.c` gets row 39.

### CF-I. Decisions needed (added to §23)

| From | Decision |
|---|---|
| I9 | **PS1 ledger option (A) or (B)** (CF-15); the joint GBA ROM figure and music's phase-1 floor (CF-14); MU1 as `audio.c`'s owner (CF-30) |
| CS P3 | `spu.c` hunks: weak symbols, `SPU_SFX_END` = `0x0A000`, `spu_sfx_addr()`, no reverb code, optionally share the staging buffer; the `rostxt.c`/`cdread.c` prefill calls; withdraw AU11's `0x7e000` |
| CS P5 | the consumer lines for AU01, AU13, AU14 and AU15 (CF-23); the `send()` hunk goes to MU1 |
| GBA platform | `bigbuf.c`: the audio tail and claim release (CF-11); the hand-over of `irq.iwram.c` with `IRQ_TIMER1` (CF-10) |
| foundation | `scene_top_arg()`; MC_BOOT in `boot.c`; the `mmlc` rule in `core/Makefile`; `runner_main.c` |
| freeplay | move `fp_island_weather()` into `fp_common.c` (CF-5) |
| S2 | apply the CMake/iso hunks; `mmlc` before `textpack`; `OP_MUS_*` append-only; the `dream.c` and `txtload.c` hunks |
| #77 / #78 | EWRAM ≤ 2,350 B (CF-12); IWRAM +86 B (CF-13) |

*End of critique fixes.*

*End of SPEC.*
