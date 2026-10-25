# Audio and music

Owner: MU1 (the music engine). The design is `docs/design/music/SPEC.md`;
its Critique fixes (CF-*) are binding. This file says what is built today,
how it fits together, and what it costs.

Status: **M1** (engine, rings, driver, legacy bridge, classic cues) and
**M1.5** (the shared banks, the build-time composer, the GEN material of
the four hubs, the T3 metric). The game sounds exactly as it did before
the engine: all 28 classic songs are compiled into the music bank and
played by the engine through the legacy bridge on native, GBA and PS1 (T1
proves it tick for tick). The shared banks sit in the bank, ready for M4's
runtime and MU4's cues; nothing plays them yet.

## Files

| File | What |
|---|---|
| `core/include/doomo/music.h` | the music API (SPEC sec.9.1); the contract |
| `core/include/doomo/mus_data.h` | the bank format, events, voices, tiers (sec.4, 6) |
| `core/include/doomo/platform.h` | the music block: caps, voice plan, `plat_voice`, buses (sec.9.4) |
| `core/src/audio.c` | SFX, babble, the plat_tone send cache, `audio_init/tick` |
| `core/src/audio_seq.c` | `audio_music*()` on the engine, the bridge flush, the classic interpreter (T1's reference until M5) |
| `core/src/audio_fx.c` | S1's music fx (mute, duck, tape stop, resume-on-beat) |
| `core/src/mus_core.c` | the bank directory, `music_update`, cue requests, `music_hash` |
| `core/src/mus_lane.c` | the arranger: lane tokens to events |
| `core/src/mus_graph.c` | the node walk (M1: linear, `MCF_EXACT`) |
| `core/src/mus_drv.c` | the driver: rings, clock, dispatch, envelopes and macros, voices |
| `core/src/mus_bind.c` | `SONG_*` to `MC_CLASSIC_*` |
| `core/src/mus_int.h` | engine internals and sizes |
| `core/src/plat_weak_music.c` | weak platform defaults: the legacy bridge tier |
| `core/native/plat_music_native.c` | musrender: `runner --wav` |
| `tools/mmlc.py` | the music compiler (a `doomo-generator`) |
| `tools/musgen/` | the build-time composer (host only, never in CI): GEN variants, the offline guard, renders |
| `core/music/common/*.dms` | MU4d's shared banks: instruments, comps, grooves, fills, motifs, vinyl, transitions, turnarounds |
| `core/music/gen/<area>/*.dms` | curated musgen output: the GEN slots of TITLE, HAHALAND, INVASION, HOME |
| `core/music/ids/*.lock` | the cue / sting / vinyl / motif / instrument id locks (append-only) |
| `core/gen/mus_data.c`, `mus_ids.h` | generated: the bank and the ids |

## How a note gets out

```
audio_music(SONG_x)                       (game code, unchanged)
  -> mus_bind_cue -> MC_CLASSIC_x
  -> mus_core_start: deck A arranger at the master clock
audio_tick (30 Hz) -> audio_seq_tick
  -> audio_fx_tick (quarter-note grid)
  -> music_update
       arranger: lanes -> DoomoMusEv into ring A (deck A) / ring B (deck B)
       mus_drv_run(8): 8 driver ticks (mt) = one 30 Hz frame on the bridge
         dispatch due events -> voices -> control frame (envelope, gate,
         arp, vibrato, sweep) -> plat_voice records
  -> bridge_flush: mus_bridge_tones[4] + S1 fx gains -> plat_tone
```

* **Bank.** One position-independent little-endian blob
  (`doomo_mus_bank[]`): a 52 B header, a sorted directory (cues by id, then
  stingers), per cue its roles, nodes, edges and node packs (a lane per
  role, 4-aligned CALL subroutines shared by repeated bars), then the
  instrument table (20 B rows; the classic `AudioInst` fields plus timbre).
  The classic songs are `MDK_LEGACY` cues named in `common`.
* **Tokens.** One byte each: `00-5F` note at the default length, `60-6F`
  note with a length index, `70-7F` rest, `80-8F` tie, `90-9F` drum hit,
  `C0-D7` commands (length, volume 0..63, instrument, gate, transpose, base,
  pan, CALL / RET, MARK ...), `E0-EF` an extended length (the 16-entry
  table `3 6 8 12 16 18 24 32 36 48 72 96 144 192 288 384`), `F0-F7` a short
  extension, `F8` hold, `F9`/`FA` long rest / tie, `FF` end. 48 units per
  quarter note.
* **Tempo.** The classic law, kept exactly: `rate256 = (bpm*12288+900)/1800`
  units/256 per 30 Hz tick; the driver clock advances `rate256*32` (Q16) per
  mt, 8 mt per tick.
* **Events.** `DoomoMusEv {pos | kind<<28, arg}`, 8 B: NOTE, CHORD, DRUM,
  OFF, INST, PAN, GAIN, TEMPO, MARK, PITCH, MUTE, FX. Gates are computed by
  the arranger in control frames exactly as the old interpreter did (first
  piece `max(1, frames*gate/8)`, ties add).
* **Decks.** Deck A plays the cue; deck B plays the second song of
  `audio_music_dual` (roles mapped A: 0,2 -> voices 0,2; B: 0 -> 1, 3 -> 3).
  `audio_music_once` plays pass 1 (the cue's `tag` node is its last), then
  the arranger emits OFF/CUT per voice and `MARK END`, and `audio_seq`
  restarts the previous song.

## The legacy bridge (tier `AT_BRIDGE`)

Until a console backend lands (MU2 GBA at M2, MU3 PS1 at M3), the weak
defaults in `plat_weak_music.c` report tier `AT_BRIDGE` with 4 voices.
`music_update` then runs the driver in-thread (`mus_drv_run(8)`) and the
driver writes `DoomoToneState mus_bridge_tones[4]` directly (no record
round trip); `audio_seq`'s `bridge_flush` applies S1's fx gains and sends
`plat_tone` through `audio_int_send` exactly where the interpreter used to,
around babble and the comedy kit. The arranger keeps only 2 ticks of
look-ahead on the bridge (`(rate256>>7)+2` units) so the decode work is
spread evenly instead of a one-bar spike at song start.

## Tools

```
python3 tools/mmlc.py                       # regenerate core/gen/mus_*.{c,h}
python3 tools/mmlc.py --report build/mus_report.txt
python3 tools/mmlc.py --dump MC_CLASSIC_BOSS
make -C core check-gen                      # byte-identical check (CI)
core/build/runner --script S --wav out.wav  # musrender (22,050 Hz stereo)
```

`mmlc.py` imports `core/src/songs.c` and the `mml_*.c` strings, simulates
the classic interpreter to find each song's loop (up to 3 passes), splits
it into 8-bar nodes, encodes lanes, dedupes repeated bars into CALL
subroutines and writes the bank. Today: **8,784 B for 28 cues and 24
instruments** (the classic MML text was 10,813 B).

Runner script commands for listening: `music N`, `music-once N`,
`music-dual A B`, `mute N`, `duck Q8 N`, `tapestop N`, `resume`,
`music-hash CUE MIN`. Renders go to `build/renders/<package>/` (git-ignored,
CF-36).

### Shared banks and the build-time composer (M1.5)

`core/music/common/*.dms` (MU4d) compile into sections between the
instruments and the cue blobs (layouts: the M1.5 block of `mus_data.h`):

| Bank | Today | Bytes |
|---|---|---:|
| `inst.dms` | 58 instruments (rows 24-81 after the 24 classic ones; leads S0-S4, harmony, pads, basses, 6 kits, comedy, crackle, 12 beds), `psg_env` and `spu_adsr1/2` fitted | 1,160 |
| `comps.dms` | 29 comp patterns in families (arp, stab, Alberti, pad, root-5, walk, gallop, chug, pedal, octave, Charleston, waltz) | 486 |
| `grooves.dms` | 21 grooves (64 B: 6 rows x 16 steps x a 4-bit probability; 15 = a core step) | 1,344 |
| `fills.dms` | 17 fills | 280 |
| `motifs.dms` | the 32 leitmotifs of SPEC sec.14 (heads protected; MINGLE and HEE HEE as shipped, CF-2) | 644 |
| `vinyl.dms` | the 16 turntable patterns + the crackle toggle (sec.7.2), segments in half-semitone log speed | 294 |
| `trans.dms` | the default transition rows (`(*,*)` CROSS 2 at a phrase, `MC_NONE` in/out) | 24 |
| `turns.dms` | 16 turnarounds + the cadence bitmaps of the 3 families | 204 |
| synth SFX | the 15 `SynthSeg` recipes of `audio_fx.c`, imported verbatim as `MDK_SFX` cues (D14, CF-3) | 966 |

The bank is 14,428 B (was 8,980 B). The GEN slots (`core/music/gen/`)
are linted (lane sums, chart length, STRONG_CT, the offline guard: no 4-bar
pitched window twice in a file) but take no bank bytes until MU4a/b's cues
reference them: TITLE 2 slots x 4 variants (410 B), HAHALAND 3 x 4 (1,253
B), INVASION 2 x 4 (1,282 B), HOME 2 slots, 5 variants (408 B).

```
make -C tools/musgen check     # gcc AND clang print 37d71b12 8d898ef0 c33cfa26 bdcb0bd2
make -C tools/musgen renders   # build/renders/MU1T/<cue>_morph_180s_ref.wav, <cue>_genreel_ref.wav
python3 tools/mmlc.py --gen MC_HAHALAND   # re-realise a cue's GEN slots (never in CI)
```

`tools/musgen` is the design study's composer moved to the host bit for
bit (only its identifiers carry an `Mg` prefix). For each slot it plays the
theme at the cue's dial over a fixed list of seeds, takes every section of
the slot's form type as a candidate (the pitched roles the variants own and
the chord chart) and keeps 4 variants in 2 chart families, rejecting thin,
off-key and non-chord-tone candidates, duplicates, any 4-bar M2 window that
collides with the theme's V0 statement or another kept variant (D10), and
near-copies (mean fuzzy F >= 0.85). The rejection counts are in each
`.dms` header. The committed `.dms` is the source: curate by ear.

Listening (the 8 M1.5 renders, `musgen` reference synth, 22,050 Hz): no
clipping (peaks 13.3k-15.8k), no drop-outs (no silence over 0.3 s), level
steady per 10 s (rms 1.6k HOME's waltz to 2.8k INVASION); the morph renders
walk home -> spooky -> metal -> home on schedule, and each GEN reel plays A
(V0) between the kept variants, so every variant can be heard against the
theme.

## Tests

| Test | File | Proves |
|---|---|---|
| T1 | `test_music_classic.c` | engine == interpreter per tick (freq, vol, wave), 28 songs x 1,500 ticks; quiet volumes, dual, once + resume, lengths, names |
| T2-native | `test_music_hash.c` + `data/music_golden.h` | per cue: blob FNV, `music_hash(cue,2)`, 600 ticks of bridge tones |
| T3 | `test_music_rep.c`, `music_rep_lib.c`, `music_rep_proto.c` | the CF-38 metric on real events: every adaptive cue (>= 2 nodes, 10 min) and hub (60 min, 8 seeds x steady/walk/morph) against gates 1-5; the 5 worst near-repeat spans with timestamps |
| T18 | `test_music_rep.c` | the metric FAILS classic HAHALAND_DAY (gates 1-3), a static hub with drum dice (gate 1), an ornamented A-B-A-B (gate 2) and a velocity-only 4-bar loop (gate 1), and PASSES the prototype composer (96 streams of 60 min) |
| T4 | `test_music_data.c` | bank walk (lanes sum to node length, CALL/RET), the shared banks in bounds (indexes, records, locks, sorted transitions, turnarounds, the synth SFX), 30 min random play (no stuck voice), IRQ-mode 15 Hz 30 min without under-run, prefill |
| T13 | `test_music_drv.c` | driver: merge order, batched == single ticks, envelope, rest/cut, vibrato delay, arp, sweep, mute windows, gain ramp, fence, under-runs, volume table |
| MU0 | `test_music_contracts.c` | sizes, enums, packing, bank header, weak defaults |

**The T3 metric (CF-38).** Per deck-A bar of the hash-mode decision
stream: S (pitched roles as role, 16th onset, pitch, length class + drum
core steps, fills, crashes + stems + dial band; no velocity, no dice or
ghost drums, no surface ops or GRACE / MORDENT / TURN), P (pitched only), L
(lead only, an anchor's first 4 bars masked), F (per role class a 16-step
onset bitmap and a pitch-class set; weighted Jaccard product, LEAD 3, BASS
2, HARM 1, COUNTER 1, KIT 1; a match is >= 0.85) and an 8-bar texture
vector. Two choices CF-38 left open, both needed for T18(e) and neither
lets T18(a)-(d) pass: the lead takes part in every F comparison (two bars
without a lead score 0 there, as a rest never matches in L), and the
texture's stem set is the pair (roles sounding throughout the 8 bars,
roles sounding in any of them), so a role dropping out inside a block is a
texture change. The prototype then passes every gate with margins: worst
of 96 streams LRR S 4 / P 5 / L 10, near-loop 8, recency >= 99.7 %,
texture >= 94.2 %, HEAD_GAP 24, cadences >= 99 %. Re-measured without
velocity (CF-37) its full-mix LRR is 2 bars, not 1. T3 + T18 take 1.4 s.

Re-record T2 after a deliberate bank change:
`core/build/tests --update-golden music_t2` (it rewrites
`core/tests/data/music_golden.h`).

Also checked at M1: the `plat_tone` / `plat_sample` traces of 20 runner
scripts (story chapters, cutscenes, jukebox, concert, studio, title,
island, dream ...) are byte-identical to the pre-engine runner.

## RAM and CPU ledger (M1)

| | before (AUD-SPLIT) | after (M1) | budget |
|---|---|---|---|
| GBA EWRAM free | 6,616 B | 5,640 B (+976 B used) | +2,350 B |
| GBA IWRAM | 6,320 B stack | unchanged | +86 B |
| GBA ROM | 3,158,428 B | 3,161,012 B | - |
| PS1 image end | 0x801e7000 (99 KB below stack top) | 0x801e8530 (94 KB) | >= 72 KB |

M1.5 (the shared banks and the synth SFX in the bank; no code), measured
on the 2026-10-25 tree: GBA EWRAM 5,632 B free and IWRAM 6,320 B stack,
both unchanged; GBA ROM 3,287,856 -> 3,293,304 B (+5,448 B: the bank);
PS1 image end 0x801e7370 -> 0x801e88c0 (+5,456 B, the bank is resident
until MU3's MUSIC.BIN at M3), 93 KB below the stack top. No scene or
per-frame code changed, so no fps moved.

* GBA music RAM: driver 748 B, core (decks, voice runtimes) 502 B,
  `audio_seq` 48 B, bridge tones 16 B; the interpreter's state and
  `songs.o` are dropped by the linker on consoles.
* PS1 resident code: `mus_*` 11.7 KB + `audio_seq` 2.2 KB = 14.0 KB
  (budget 18.5 KB). The bank (8.8 KB) is resident at M1; MU3 moves it to
  SPU RAM at M3. Ring A is 40 events on PS1 while the driver runs
  in-thread; MU3 raises it to 128 with the IRQ driver (saves 704 B today).
* CPU (GBA, THUMB from ROM): no divisions in the hot paths (ring indices
  without `%`, multiply-shift volumes and octaves), a cached instrument
  table, one batched driver run per frame, the bridge's direct tone path and
  a 2-tick look-ahead. `make -C gba test` work averages match the
  pre-engine tree within 0.1 ms (P0 31.3 vs 31.2 ms; P0b 37.9 / 23.1 fps;
  summit 15.7 fps; cutscene_wall 17.9 fps). Audio path per video frame:
  island 2,387 -> 2,846 cycles, title 933 -> 1,473 (about 1 % of a frame).

## Next

* **M2 (MU2):** GBA PSG v2 + lean mixer, the VBlank driver; legacy off on GBA.
* **M3 (MU3):** PS1 SPU backend, IRQ driver, ring A 128, bank in SPU RAM.
* **M4:** matrix, stingers, comic bridge, DECK vinyl, GEN bags, chapters.
* **M5:** retire the interpreter (`audio_legacy_*`, `songs.c`).
* **M7:** TAPE feed (PS1 scratching uses TAPE unless the ledger can't hold it, CF-I).
