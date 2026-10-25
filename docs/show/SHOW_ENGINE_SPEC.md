# DOOMOGACHI LIFE in HELLvile Park!: Show Engine Spec

**Version 1.1, 2026-10-25. Implementation-ready, re-scoped for the New-Year schedule.**

| | |
|---|---|
| **What this is** | The build spec for the pipeline that lets one creator make every episode of the show by talking to Claude Code, with every frame drawn by the game's own PS1 engine. |
| **Schedule** | `SELF_MADE_ROADMAP.md` v2.0: M0 Nov 1, M1 Nov 8, M2 Nov 15, the pilot released Dec 5, then 102-104 on a one-week loop to Dec 31. Work that does not serve a released episode by then is marked **deferred (2027)**. The work packages carry their week in §16 |
| **Built on** | Design A, "On-console authentic". It won both judge panels (8.0 and 7.5). Accepted grafts from designs B and C are folded in and listed in Appendix B. v1.1 adds the review against the real code in Appendix C. |
| **Repo state read** | HEAD `9d4d915` ("Show: re-aim to adult animation (TV-MA)"). The show docs are read here but never edited. The TV-MA re-aim has landed, and the cold open's timing is unchanged by it (3,000 f, 21 shots). |
| **Evidence** | Numbers marked **(m)** were measured in this container on 2026-10-25. Their logs are in `showengine/bench/`, `bench_b/`, `eval_td/`, `verify_eval/`, `spec_bench/` and `critique_nye/`. Numbers marked **(e)** are estimates, and a named acceptance test confirms each one. |
| **Write scope** | Nothing in the repository was edited. Every file this spec names under `show/` is new. **v1.1: the show never edits a file in `core/`, `gba/`, `ps1/`, `tools/` or `docs/show/` either.** It builds stand-alone and compiles patched copies of the two portrait files (§4.3). |

---

## 0. The pipeline on one page

| Question | Answer |
|---|---|
| Who draws the picture? | `show.exe`, a PS1 executable built from `ps1/src` + `core/` + new `show/engine/` sources by **its own stand-alone CMake project** (`show/engine/CMakeLists.txt`, preset `ps1-show`). It links the game's renderers (2D layer, portrait/figure/chibi engine, GTE 3D, overworld, 2.5D room, `camfx`, `tween`, `beat`, fonts). It does not link the game's simulation, scenes or overlays, except the house modules the room renderer needs (§8.3). Measured: such an executable links from the game's own objects and boots in PCSX-Redux (§15.1). |
| Where does it run? | PCSX-Redux (OpenBIOS), headless and unthrottled (`spu.Speed = 0`), in lockstep with a harness forked from `ps1/harness`. Each 30 Hz tick is one captured frame. |
| Picture format | A real NTSC **320x240** display carrying a **320x180** 16:9 picture (lines 30-209, open matte). Even scales give **x6 = 1920x1080** and **x12 = 3840x2160**. A **384x216** "wide" preset is a hatch **deferred to 2027** (§2.1). |
| Frame rate | **30.000 fps master, one frame per game tick.** The 29.97 conform is deferred with the broadcast versions. |
| Source of truth | Plain-text `.shw` scripts in git: rundown, shots, lines, cues, cast, sets. Claude and the creator edit them with small diffs. |
| Compiler | `showc` (Python). It wraps `core/tools/textpack.py`'s `Compiler` class read-only, so the show and the game share symbols, text codes and the beat event format. It emits per-shot `BeatEv` streams that run on the unchanged `beat.c` (show ops ride `BOP_SHOT`), plus stage records, cast looks, tracks, a sample-exact audio event list and captions. |
| Audio | Rendered on the host at 48 kHz/24-bit, with frame *f* starting at sample 1,600·*f*. Voices come from Kokoro-82M TTS (54 voices, blends, WORLD shaping) or the creator's recordings, swappable per line id. Music comes from the production kit plus the game's own cues. SFX are the game's kit re-rendered at 48 kHz plus synthesised foley. The kit's mixer makes the stems and the masters. |
| Masters | Lossless **FFV1** per shot at native 320x240 (about 0.63 GB per episode (m)). Deliverables are made from it with ffmpeg: nearest-neighbour scaling, H.264 for the web. UHD and ProRes are deferred (on request). |
| Review | Claude reads contact sheets, timing text, lint text and per-frame state hashes. The creator watches burned-in preview MP4s and ticks a standards checklist per episode (§13.7). Every fix is a small text diff plus a before/after sheet. |
| Speed here | An episode's picture renders in **about 4-6 min** on one emulator (e; emulation alone is 5-7 ms/f (m)). A three-shot test (390 f) renders, mixes and encodes in **under 2 min** (e). An HD web MP4 of a full episode takes about **15-20 min** to encode (m: 39-48 fps at 3 threads). |
| Game safety | **The show edits no game file.** It has its own CMake project and presets under `show/engine/`, and it compiles copies of `portrait.c` and `portrait_head.c` patched at build time (§4.3). The game's sources, presets and outputs cannot change. A cheap gate checks that every show commit leaves `core/`, `gba/`, `ps1/`, `tools/` and `docs/show/` untouched, and a weekly full build checks the game's EXE, OVL.BIN and TEXT.BIN hashes (§4.4). |

---

## 1. Goals and non-goals

### 1.1 Goals

| # | Goal | How this spec meets it |
|---|---|---|
| G1 | **Every master pixel is drawn by a PS1 executable built from this repo** (REQ-GOAL-1) | `show.exe` in PCSX-Redux is the only master renderer. Host code prepares data the way the game's build does (plates, packed art), but never draws a master pixel. |
| G2 | **One creator, working through Claude Code, writes, stages, renders, reviews and fixes scenes conversationally** (REQ-LOOP-1/2/6) | Text sources, one `show <verb>` command per step, errors that name file, line, shot, frame and rule, and review outputs Claude can read: PNG sheets, timing text, hash logs. |
| G3 | **Video files out** | `show preview` and `show deliver` produce MP4/MOV with audio, captions and chapters. |
| G4 | **Deterministic and reproducible** (REQ-LOOP-5) | Pinned emulator, BIOS, toolchain, harness and Python dependencies. Pure shots, seeded RNG, no wall clock. Frame hashes go in a manifest, and approved TTS takes are locked by hash. |
| G5 | **Frame-accurate timing** | 30 fps master, one frame per engine tick, audio at 1,600 samples per frame. Every event is resolved to a frame (and a sample for audio) at compile time and verified on the rendered frames (P1/P4 logs, §8.9). |
| G6 | **16:9** | 320x180 inside the real 320x240 mode, with integer scales to 1080p and 2160p. A wide 384x216 hatch is specified but deferred to 2027. |
| G7 | **Stay within PS1 limits by default and name every hatch** | The default `ps1-show` build has 2 MB RAM, 1 MB VRAM, the game's VRAM map and the NTSC display (measured headroom in §8.7). Each shot gets a badge: PS1-LEGAL or EMU-ONLY (§8.10; PS1-REALTIME is deferred). |
| G8 | **The game build is never destabilised** (REQ-GOAL-2) | No game file is edited: a stand-alone show project, patched copies of two portrait files, its own binary dirs under `show/build/`. The `show check-game` gate (§4.4). |
| G11 | **Finished episodes by 2027-01-01** (the creator's milestone) | The pilot plus 102-104 on the roadmap's dates; every deliverable that is not needed for an online release is deferred (roadmap Appendix A). |
| G12 | **The four hard limits are enforced on every episode** (`SERIES_OVERVIEW.md` §2) | Scene-scoped lints that block delivery (§13.5) plus the creator's signed checklist (§13.7). |
| G9 | **Audio from the project's own tools** | Production kit (`theme/kit`), Kokoro + WORLD (`theme/final/src`), the game's MML cues and `sfxgen.py`, or the creator's recordings. |
| G10 | **Works within this container's limits** | 4 CPUs, no GPU, PyPI and npm only, about 4.9 GB of free disk (m, falling; §12.2). |

### 1.2 Non-goals

- **A second master renderer.** The native runner, `runner-show` and any software PS1 GPU are diagnostics only. They are never used for master pixels and never trusted for them. The pixel-parity calibration suite of design C is dropped.
- **Real-time playback on a PlayStation (the "disc edition").** This is optional, after M3. The CD-XA soundtrack and realtime mode are off the critical path.
- **PS1 SPU audio in the master.** PCSX-Redux has no capture tap and drops audio when unthrottled (m). Audio is rendered on the host, locked to frames.
- **Editing the docs in `docs/show/`.** They are the content and timing source, owned by other workflows.
- **Hand animation as the main acting channel.** Acting is scripted data on the engine's own channels: expression, mouth, blink, gaze, clocks, tween tables, walks. Hand-drawn frames are imported only for the beats that need them.
- **Studio features.** No multi-user review server, no render farm, no proprietary formats.
- **A guaranteed broadcast QC pass.** The flash and loudness analyses are honest tools, not certified Harding or PSE tests.
- **Deferred to 2027 (v1.1)** because none of it is needed to release an episode online by New Year: the broadcast version (29.97 conform, elastic trims, breaks, `r128`/`a85`/`dlg27`), 5.1 and M&E, audio description, TTML/SCC captions, ProRes and UHD masters (UHD stays an optional overnight encode), the 2x11/textless/4:3/9:16 versions, the wide canvas, the disc edition and PS1-REALTIME badge, `runner-show` (WP14), the P2 draw trace, GP0 attribution, gaze control, the crowd atlas with recolour CLUTs, MFCC-DTW take alignment and Git LFS. Each section below marks its deferred parts.

---

## 2. The format decisions

### 2.1 Resolution and aspect

| | Default: `ps1-show` (fixed for 2026) | Hatch: `ps1-show-wide` (deferred to 2027) |
|---|---|---|
| PS1 display | NTSC 320x240, 15-bit, double-buffered at VRAM x 0-319, y 0-479: **the game's layout, `ps1.h` unchanged** | Single-buffered 384x216 at VRAM (0,0). Not displayable on hardware. Emulator capture only |
| 16:9 picture | **320x180**, screen lines 30-209 (show y + 30 = screen y) | 384x216, the whole buffer |
| Matte | **Open matte**: lines 0-29 and 210-239 carry picture when the set provides it. Otherwise black bars are drawn last, outside post-FX, so a shake never moves the frame edge | None |
| Delivery scales | HD **x6** = 1920x1080; UHD **x12** = 3840x2160 (even, so 4:2:0 chroma blocks never straddle an art pixel). Bonus: 4:3 x9 = 2880x2160 from the full frame; 9:16 = a 135x240 crop x8 = 1080x1920 | HD x5 (odd: chroma straddles art pixels in 4:2:0), UHD x10 |
| The Wall line | x = **160** | x = 192 (the docs' number) |
| Title-safe (90 %) | 16 px left/right, 9 px top/bottom | 19 / 11 |
| VRAM map | The game's | Page-0 world-texture slots move into the area the second framebuffer frees (§8.8) |
| Badge | PS1-LEGAL possible | Always EMU-ONLY |

**Why 320x180 is the default.**
- It is a real PS1 mode, so the disc boots on hardware and shows exactly what was captured.
- The VRAM map, image cache and CLUT rows stay exactly where the shipped renderers expect them.
- The art density, shake amplitudes and portrait sizes were tuned at 320 wide. A 48x56 bust at `px=2` (96x112) is 62 % of frame height, which makes a natural close-up.

**What it costs.**
- The picture has 69 % of the docs' pixel area.
- Every coordinate in the docs needs a 5/6 remap. The script handles this with a `doc:` prefix: `doc:192` compiles to 160 on the default canvas and to 192 on the wide one.

**Decision procedure (v1.1).** The 2026 schedule fixes the canvas at **320x180**. The wide hatch needs guarded changes in three game files (`ps1.h`, `vram.c`, `gfx.c`) and a VRAM slot move, which the zero-touch rule (§4.3) and the dates rule out. M0-T6 is now a one-minute look: the test chart and a slug-art co12 at 320x180, shown at 1080p. If the creator still wants the wide canvas after four episodes, the procedure below is the 2027 path:
1. Render the same test chart, and the cold open's shot 12 blocked with slug art, at 320x180 and at 384x216.
2. Show the creator both at 1080p.
3. Record the choice in `show/series/show.shw` (`canvas=`).

Every script number written with `doc:` survives either choice.

**Options rejected.**
- **368x240.** No integer scale to 1080p, non-square pixels on a CRT, and every engine layout is 320 wide.
- **640x480i.** Interlaced. The art draws tiny at 1x or doubles back to 320x240, and fill rate halves.
- **Anamorphic 16:9.** No integer pixel multiplier exists for it.

### 2.2 Frame rate: 30 fps

- **Every engine timing table is in 30 Hz ticks:** the R2-R26 rungs, the `TW_*` tweens, the beat scripts, the dialogue holds, the 14 `camfx` shake profiles and the music tempo law. The shot list is also written in 30 fps frames, including a 4 f insert and 31/40/44 f shots. 24 fps would mean re-timing every table, and 4 f inserts would not survive.
- **Audio lands on whole samples per frame:** 1,600 at 48 kHz, 800 at 24 kHz (Kokoro) and 735 at 22.05 kHz (the runner).
- **The 29.97 delivery conform** (deferred with the broadcast version) relabels the picture as 30000/1001 and resamples the audio by 1001/1000 with `soxr`. The broadcast version trims 40 f from elastic beats so the episode fits a 22:00;00 drop-frame slot. The web release is 30p and needs neither.

### 2.3 Pixel rules

- **Per-shot pixel scale `px=1|2|3`.** Everything in a shot draws at one scale. A `px=2` shot uses plates authored at 160x120 (open matte) and drawn x2 with nearest-neighbour `POLY_FT4`, which the PS1 image cache already does for scales 2-4 (m: `gfx2d.c`). All positions in scripts are **show pixels**, and the compiler snaps them to multiples of `px` (lint `PIC-SNAP`). 3D set kinds are `px=1` only.
- **Dithering: no hunk is needed.** The 2D layer already draws raw-textured SPRTs and quads (`setShadeTex(p, 1)`) and flat TILEs, which the PS1 GPU never dithers. Dithering applies only to the Gouraud and textured-modulated polygons of the 3D, overworld and room renderers, and the show keeps it there as part of the PS1 look. The show's own 2D planes never use Gouraud: gradients are drawn as palette bands.
- **Palette lint by primitive class.** The capture LUT maps all 32,768 BGR555 values to a palette index or OFF (0.29 ms per frame (m)). Off-palette pixels have three legitimate sources:
  1. GTE Gouraud and fog in 3D, overworld and room planes;
  2. the game's semi-transparent 2D blends (the 50 % checker TILE, the `plat_shade_rect` passes);
  3. `camfx` flash and fade.

  The lint (`PIC-PAL`) reports them per frame, with a heat map. In v1 it fails only on 2D-only shots that use none of these. In v2 it masks the allowed regions using the P2 draw trace.
- **Scale moves are done per layer, inside the engine.** Shot 11's 4 % push, shot 12's 1.10→1.00 and shot 20's 1.00→1.08 draw each plane as a nearest-sampled quad at the camera scale. The finished frame is never scaled. Each non-integer span is logged as `CAM-SCALE`.
- **Shake is a camera offset.** The `camfx` shake and punch offsets are applied to the stage camera, and planes carry an 8 px overscan, so no uncovered edge ever shows.

---

## 3. Architecture: compile, render, assemble

```
 TEXT (git)                    COMPILE (host, Python)              RENDER (PS1 show.exe, PCSX-Redux)       ASSEMBLE (host)
 ------------------------      ---------------------------------   ------------------------------------    ---------------------------------
 show/series/*.shw         ┐   showc                               build/ep101/disc/show.cue               capture.py (2nd CPU, overlapped):
 show/ep101/episode.shw    │    1 parse + macros                     show.exe + SHOW/EP101.SHW               BGR555 -> exact RGB24 -> FFV1
 show/ep101/shots/*.shw    ├──> 2 voice resolve (voice.lock)   ──>  harness_show.lua (lockstep, unthrottled)   per shot; frames.sha256;
 show/ep101/lines.shw      │    3 time resolve (frames/samples)      - Lua crop 240 rows -> FIFO              off-palette counts; P-logs
 show/ep101/cues.shw       │    4 stage resolve (sets, looks)        - g_show_mbox: seek, P1/P4, stats      audio: voice (Kokoro/takes)
 show/ep101/takes.toml     │    5 plan (RAM/VRAM/badge)              - one boot serves a run of shots           + music (kit, MML cues)
 show/ep101/voice.lock     ┘    6 lint                                                                         + SFX/foley -> mix -> stems,
                                7 emit: EP101.SHW, timeline.json,                                              masters (LUFS targets)
                                  audio_events.json, captions.json,                                         ffmpeg: crop 320x180, x6/x12
                                  plan.txt, lint.txt                                                           neighbor, mux, chapters, TC
                                                                                                            QC: P4 audio-event check,
 review <──────── sheets (PNG), strips, `show timing`, lint text, preview MP4, before/after ────────────────  loudness, flash, captions
```

**Data contracts between the stages (all new, all versioned):**

| Artifact | Producer → consumer | Format |
|---|---|---|
| `build/ep101/EP101.SHW` | showc → show.exe | Little-endian binary, 4-byte aligned, one chunk per shot (§7.3) |
| `build/ep101/timeline.json` | showc → every host tool | Shots, beats and events in absolute frames, with file:line provenance |
| `build/ep101/audio_events.json` | showc → voice/mix/QC | Every sound: absolute sample, source, gain, pan, bus, event index *k* |
| `build/ep101/captions.json`, `ad.json` | showc → captions/deliver | Frame-exact in/out |
| `build/cache/shots/<key>/` | capture → deliver/review | `master.mkv` (FFV1), `frames.sha256`, `plog.tsv` (P1/P4/stats per frame; P2 deferred), `perf.json`, `badge.json` |
| `build/ep101/audio/` | voice/music/mix → deliver | 48 kHz/24-bit WAV or FLAC stems: DX, MX, FX, AD; `mix_<target>.wav` |
| `show/ep101/voice.lock` | `show voice` → showc (committed) | Per line: take source and hash, onset and length in samples, word and phoneme times, mouth track |
| `show/ep101/deliveries/<v>/manifest.json` | deliver (committed) | Source commit, tool, emulator and BIOS hashes, frame and audio hashes, badges, lints |

**Three loops at three speeds:**
1. **Edit, then check** (sub-second): `show compile` + `show lint` + `show timing --static`.
2. **Edit, then look** (seconds): render the dirty shots and build sheets and strips.
3. **Edit, then watch** (a minute or so): preview MP4 of a beat with its temp mix.

---

## 4. File layout

### 4.1 New tree (the game never reads it)

```
show/
  README.md                         every command, with examples
  docs/                             this spec, SELF_MADE_ROADMAP.md, the WP checklists
  vault.lock                        hashes + archive locations of everything not in git (§10.7)
  engine/                           C99: the show player (ps1-show; runner-show is deferred)
    CMakeLists.txt                  a STAND-ALONE project (v1.1): PSn00bSDK toolchain, the game's exact compile
                                    flags, its own targets; reads ps1/src and core/ by path, writes only show/build/
    CMakePresets.json               ps1-show; ps1-show-8mb (the M0-T5 RAM hatch test); ps1-show-wide deferred;
                                    binaryDir show/build/<preset>
    core_sources.cmake              curated core list, as measured by the link probe (§8.7): portrait*, beat, camfx,
                                    tween*, ui, ui_draw, ui_raster, ui_fmt, font, palette, fixmath*, txt, txt_dict,
                                    resident, catalog, rng, stubs, textures, sprites, options, town (doomo_town),
                                    art_data_ps1 + packed_art; + map, maps, map data for OVERWORLD/VIEW3D; + home,
                                    home_draw, home_edit, furn_* for ROOM. Never game.c, scene.c or other scenes
    ps1_sources.cmake               gfx gfx2d vram view3d r3d overworld room room_test postfx xform prims model world
                                    packart timer pad hcmd main coreref libgcc_r3000 adpcm spu memcard, plus cutview,
                                    overlay and txtload compiled WITHOUT -DDOOMO_CUTSCENE_VIEWER/-DDOOMO_OVERLAYS
                                    (main.c calls them unconditionally; their #ifdef bodies compile to stubs)
    patches/portrait.c.patch        the show's portrait hunks (§4.3), applied by CMake to copies in
    patches/portrait_head.c.patch   show/build/<preset>/gen/; a failed apply stops the show build, never the game's
    iso_show.xml                    mkpsxiso layout: SYSTEM.CNF, SHOW.EXE, SHOW/EP###.SHW
    include/show/shw_format.h       the binary format (mirrored by tools/showtk/shw_format.py, checked by a test)
    show_game.c                     doomo_init / doomo_frame for the show: boot, index, shot loop, mailbox. Also
                                    defines what game.c and scene.c would: doomo_frame_count, doomo_image_new_uid
                                    (the VRAM image cache keys on it), doomo_initialized, and no-op scene_set /
                                    scene_push / scene_top / scene_by_name (hcmd.c's weak references to them do
                                    NOT resolve to NULL at link time: measured, §15.1)
    show_load.c                     loaders: disc (default), Lua inject (fallback); PCdrv only if M0-T5 passes
    show_mbox.c                     g_show_mbox: seek requests, P1/P4, stats, badge counters (Lua-visible)
    show_purity.c                   shot-start reset (§8.2)
    show_stage.c                    planes: BANDS GRAD, IMAGE, TILEMAP, STROKES, GRAIN, VIEW3D, OVERWORLD, ROOM
                                    (CUTSTAGE removed in v1.1, §8.3)
    show_cast.c                     fills doomo_town.res[0..255] (town.c's own storage and town_res()) from the
                                    LOOK chunk at shot prep; pins doomo_town.opt[] (via opt_set) to the show profile
                                    (screen shake 100 %, flash cap per §13.5) so camfx/opt_get are deterministic
    show_actor.c                    actors: bust/figure/chibi/fighter/model3d; clocks; expr/mouth/blink tracks (gaze deferred)
    show_crowd.c                    crowds: formations, lanes, ripples, stoppable clocks, K-look reuse (§8.5;
                                    the atlas with recolour CLUTs is deferred)
    show_cam.c                      camera keys, drift, pans, creeps, whips, cranes, per-plane scale, cam3d rails
    show_fx.c                       particles, crayon register (grain, boil, stroke reveal), brick wipe, Split, dissolve
    show_text.c                     SUPERs (type/pop/deadpan), signs, cards, show fonts (child's hand, .pxt)
    show_hud.c                      first-person HUD
    show_ext.c                      BT_EXT dispatcher: BOP_SHOT a >= 0x4000 show ops, and the host kinds beat.c
                                    hands back (IRIS, TINT, POP, EXPR, CAM, NPC, HEADTURN, INSERT; §4.2)
    show_audio_stub.c               the core audio API as strong no-op loggers (the PS1 never plays sound in a show;
                                    strong, so the harness's weak-stub report stays empty: the probe hit audio_sfx_pcm)
    show_hash.c                     P1 state CRC, P4 audio-event ids (P2 draw-call CRC: deferred)
    show_wrap.c                     deferred: __wrap_plat_* for P2 (depth-0 calls only)
    show_stats.c                    root-counter cycles: update, draw, DrawSync; prims; prim bytes; uploads; evictions
    ps1/show_crowd_ps1.c            deferred: SPRT emission with per-citizen recolour CLUTs (the atlas)
  native/                           deferred to 2027: runner-show (diagnostics only)
    Makefile                        core + core/native/plat_native.c + show/engine; SAN=1 for UBSan
    main_native.c                   --shw FILE --shot ID [--frames N] [--plog OUT] [--png DIR] [--look-gen ...]
  tools/
    show                            CLI entry point (python3 -m showtk "$@")
    requirements.lock               pinned PyPI packages with hashes
    showtk/
      cli.py  errors.py  paths.py  cache.py  vault.py  doctor.py
      parse.py  macros.py  timeexpr.py  textpack_bridge.py  stage.py  looks.py  plan.py  lint/  emit.py
      shw_format.py  docunits.py  ids.py
      emu/harness_show.lua  emu/pcsx_settings.json  emu/run.py      (a fork of ps1/harness/run.py)
      capture.py  render.py  scheduler.py  verify.py  plog.py  gp0.py (optional, §13.6)
      voice/ g2p.py lexicon.py kokoro.py world.py takes.py align.py mouth.py asr.py audition.py
      music/ mml_legacy.py cues.py barmap.py render.py
      sfx/ library.py foley.py
      mix.py  master.py  captions.py  ad.py (deferred)  deliver.py  ffmpeg.py  release.py (upload sheet, §13.7)
      standards/ tags.py lints.py report.py signoff.py   (the hard limits, §13.5 and §13.7)
      review/ sheet.py timing.py frame.py status.py notes.py   (strip.py, diff heat map: deferred)
  audio/                            vendored from the scratchpad (text; see §10.7): kit/ (theme/kit/kit),
                                    voice/ (kokoro_voice.py, kokoro_patch.py, singer.py, asr.py)
  series/                           the bible as data
    show.shw cast.shw voices.shw lexicon.shw visemes.shw acting.shw sfx.shw music.shw lints.shw
    looks/*.shw  sets/*.shw  ratings/tvma.shw  templates/*.shw
  art/                              gen/ (plate generators, Python), pxt/ (pixel-text sprites and fonts),
                                    hand/ (creator's indexed PNGs and flipbooks; binaries in the vault, hashes in git)
  lib/                              takes/<char>/ and sfx/ (binaries in the vault, hashes in git; names in
                                    series/sfx.shw), music/ (cue scores as Python + bar maps), licences/ (§10.8)
  tests/                            pytest for showtk; golden plogs for the player; dx_proof shot set
  ep101/
    episode.shw lines.shw strings.shw cues.shw ad.shw takes.toml
    shots/co.shw mt.shw a1.shw a2.shw a3.shw a4.shw tag.shw
    voice.lock                      generated, committed
    notes/round-NN.md               review rounds
    standards_signoff.toml          the creator's ticked checklist, bound to the render hash (§13.7)
    deliveries/<version>/manifest.json
  ep102/ ep103/ ep104/              the same layout, made from series/templates by `show new-episode`
  build/                            gitignored (the repo's `build/` rule): ps1-show/ (the CMake binary dir),
                                    EP###.SHW, disc/, cache/shots/<key>/, audio/, segments/, previews/
  ai_usage.jsonl                    appended by the tools (TTS lines, generated art)
  hooks/session_start.sh            OPT-IN restore hook (v1.1): checks for /opt/ps1emu/pcsx-redux and, only if it
                                    is missing, fetches the ~10 MB emulator + BIOS. Models (~1 GB) are fetched
                                    lazily by the first `show voice`. The creator enables it in their own
                                    .claude/settings.local.json, never in a committed .claude/settings.json, so
                                    game sessions in this repo never pay for it
(no .gitattributes / LFS in 2026: binaries live in the vault, §10.7)
```

### 4.2 What is reused from the repo, read-only

| Existing piece | Reused as |
|---|---|
| `core/include/doomo/beat.h`, `core/src/beat.c` | The per-shot runtime. `BeatEv` is 8 B: `at` u16, `anchor` u8, `op` u8, `a` and `b` i16. Show ops are `op = BOP_SHOT`, `a = 0x4000 \| show_op`, `b` = arg-table index. `beat_script_valid()` checks `op < BOP_COUNT`, `anchor < 32` **and that events are sorted by (anchor, at)** (m: beat.c:73-84), so show ops pass once the compiler sorts them, and `BOP_SHOT` comes back as `BT_EXT`. **`beat.c` itself executes only** shake, punch, zoom, flash, fade, pal-lerp, hitstop, the clock ops (freeze, thaw, panic, still), labels, holds, flags and the audio ops. **IRIS, TINT, POP, EXPR, SAY, CAPTION, CUT, INSERT, ACT, TWITCH, NPC, CAM, HEADTURN and SHOT are host kinds** returned as `BT_EXT` (m: beat.c `exec()` default case), so `show_ext.c` implements them, calling `camfx_iris_in/out()` and `camfx_pal_lerp_to()` itself. 8 actor clocks (`BEAT_ACTORS`). The show never emits `BOP_FLAG` (it writes `doomo_town.story.flags`) or the `BOP_SFX`/`BOP_MUSIC*` audio ops (lint `SHW-OPS`). |
| `core/tools/textpack.py` | `load_symbols()` (line 134; 1,439 symbols in 0.05 s (m)) for every engine enum. `Compiler(syms).compile_beat(pack, name, body)` (line 888) compiles engine-native beat lines. It is a method, not a free function, so `textpack_bridge.py` builds a `Compiler`, passes a stub `pack` with a `.path` and a `.text()`, and feeds it `(where, "+N op args")` tuples **ending with an `end` line** (it refuses a script without one). Measured: `+0 iris_in 1 5`, `+8 shake SHK_TAP`, `+30 end` compile to `(0,0,19,1,5) (8,0,13,0,0) (30,0,0,0,0)` with nothing edited. The packer is used for show strings. |
| `camfx`, `tween` (`TW_*`), `ui`/font, portrait engine | Effects, eases, text and every character image |
| `resident_randomize_look(r, seed)` (resident.h:141) | Crowd and kid look generation **on the PS1 at shot prep** (resident.c is in the measured link closure), from seeds in the LOOK chunk; the script's `species=`/`tags=`/`age=` are applied after it, because `gen_look()` may pick its own species. (`runner-show --look-gen` is deferred) |
| `ps1/harness/run.py`, `harness.lua`, `hcmd.c` | Forked, never edited. `hcmd.c` is linked unchanged, but **its weak references to `scene_set`, `scene_push` and `scene_top` fail to link without `scene.c`** (m, §15.1), so `show_game.c` defines them as no-ops. `hcmd.c` also needs `doomo_town` (a strong reference), which links `town.c`. The show's own commands live in `g_show_mbox`. The harness's `--map` file is `nm -f posix` output (name, type, address), as the game's CMake writes it. |
| `core/src/mml_*.c` (for example `mml_mingle.c`: t96, F major, 8 bars) | Game cue sources, parsed by **importing `tools/mmlc.py`** (`read_songs()`, `simulate()`: it already mirrors the 30 Hz sequencer tick for tick), never by a new parser; voiced by the kit at 48 kHz |
| `tools/sfxgen.py`, `core/src/audio_fx.c` | `sfxgen.py` synthesises the PCM `SFX_*` effects with integer `tone()`/`noise()` functions that take a `rate`, so `sfx/library.py` imports them and calls them at 48,000 Hz (no edit). **The synth one-shots (SCRATCH, BOING, POP2, THUD, WHOOSH, KAZOO, BEEP, GLITCH, TIK, SHWIP, TINK, …) are not in `sfxgen.py`**: they are `SynthSeg` tables played by `audio_sfx_synth()` in `audio_fx.c`. The library parses those tables read-only and synthesises them at 48 kHz; `runner --wav` (22,050 Hz) is the reference |
| `tools/mapc.py` | Show-only 3D maps (the funhouse), compiled into `build-show/gen`, never into `core/gen` |
| `trailer/` | Gag references only. It is not a renderer for the show. |

### 4.3 The portrait hunks, as a build-time patch (the complete list)

**v1.1: no existing file is edited.** v1.0 put presets in `ps1/CMakePresets.json`, a guarded `include()` in `ps1/CMakeLists.txt` and guarded blocks in `core/src/portrait.c` and `portrait_head.c`. All of those are active game files that other workflows are changing right now (the repo has live `.claude/worktrees`), and every one of them can be avoided:

| v1.0 hunk | v1.1 replacement |
|---|---|
| Four presets in `ps1/CMakePresets.json` | `show/engine/CMakePresets.json` (`ps1-show`; `ps1-show-8mb` for the RAM hatch test; `ps1-show-wide` deferred) |
| A guarded `include()` in `ps1/CMakeLists.txt` | `show/engine/CMakeLists.txt`, a stand-alone project with the PSn00bSDK toolchain file and the game's exact flags, listing `ps1/src/*.c` and the curated core sources by path. Measured: a probe linked this way from the game's own objects, passed `elf2exe`, and booted in PCSX-Redux (§15.1) |
| Guarded blocks in `portrait.c` / `portrait_head.c` | `show/engine/patches/*.patch`, applied by CMake to **copies** in `show/build/<preset>/gen/`; the show compiles the copies instead of the originals. If a game change breaks a patch's context, the show build stops with the file name; the game build is never involved |
| `ps1.h`, `vram.c`, `gfx.c` guards for the wide canvas | Deferred with the wide canvas (2027). If it comes back, it is patched copies too |

The patches keep the `#if DOOMO_SHOW` form, so they could be upstreamed one day if the game's owners want them. The complete list:

| File (copy) | Hunk | Milestone |
|---|---|---|
| `portrait.c` | (a) the pool size from the command line: **`-DPC_BLOCKS=240`** (90 KB), with `DOOMO_STATIC_ASSERT(PC_BLOCKS < PC_FREE)` | M1 |
| `portrait.c` | (b) the mouth override folded into the image key (the blink is already in its low byte); (b') the mouth into the chibi key | M1 |
| `portrait.c` | (c) blink override in `phase_of()` | M1 |
| `portrait.c` | **(f) new:** no per-tick render budget in the show (`steps_left()` always grants the full `DL_STEPS`) | M1 |
| `portrait_head.c` | (d) mouth override in `mouth()` | M1 |
| `portrait_head.c` | (e) gaze: **deferred to 2027** (see below) | — |

```c
/* portrait.c (a): around the existing pool size (line ~392) */
#ifndef PC_BLOCKS                       /* the show passes -DPC_BLOCKS=240 (90 KB) */
#define PC_BLOCKS  48                   /* 18 KB (EWRAM on the GBA; the PS1
                                           has no RAM to spare either) */
#endif
/* ... after PC_FREE is defined: owner[] is uint8_t and 0xFF means "free", so an
   entry index of 255 would read as free. 256 blocks (v1.0) corrupts the pool. */
#if DOOMO_SHOW
DOOMO_STATIC_ASSERT(PC_BLOCKS < PC_FREE, pool_ids_fit_owner_bytes);
#endif

/* portrait.c, near the top, after the includes */
#if DOOMO_SHOW
extern int8_t portrait_show_mouth, portrait_show_blink;   /* -1 = engine default */
#endif

/* (c) in phase_of(), right after p->blink is computed (line ~638) */
#if DOOMO_SHOW
    if (portrait_show_blink >= 0) p->blink = portrait_show_blink;   /* blink is already in the key */
#endif

/* (b) in animated(), right after base is computed (line ~759) */
#if DOOMO_SHOW
    base |= (uint32_t)(portrait_show_mouth + 1) << 16;   /* MOUTH_COUNT = 20: fits bits 16-20 */
#endif
/* (b') in bodyspr_image(), right after key is computed (line ~833), so chibis can flap open/closed */
#if DOOMO_SHOW
    key |= (uint32_t)(portrait_show_mouth + 1) << 16;
#endif

/* (f) in steps_left() (line ~451): the sliced re-render budget is a real-time device. In the
   show every fetch must finish its phase on the frame it is due, whatever else was drawn
   that tick (see "Why (f)" below). */
#if DOOMO_SHOW
    return steps_max();
#endif

/* portrait_head.c (d): in mouth(), after the expression switch, before the LOD_TINY block (line ~1393) */
#if DOOMO_SHOW
    { extern int8_t portrait_show_mouth; if (portrait_show_mouth >= 0) st = portrait_show_mouth; }
#endif
```

**Why the key fold matters.** Design C proposed only the mouth override. But the portrait cache keys an image by expression, status and animation phase only (portrait.c:759, `base = expr<<8 | status<<12`). Without folding the override into the key, a changed mouth would be served from a stale cached image. The key mask `0xFFFFFF00` already compares bits 16-31, so the fold is all that is needed. A changed mouth is then "new content" and renders at once.

**Why (f).** `animated()` marks the tick's whole budget as spent after any new-content render (`steps_used = steps_max()`), and a phase-only change (a blink, a breath) is drawn as a sliced job only while budget is left. With v1.0's per-actor clocks, two actors whose clocks hold the same value in one tick (two frozen actors, or two figures with the same phase) share one budget, so the second one shows its **previous** phase: a missed blink on screen, and a seek that renders differently from a full run. `portrait_cache_clock(t, 6)` alone does not prevent this. (f) removes the budget in the show only; the lockstep capture does not care how long a frame takes.

**Gaze (e) is deferred.** v1.0 proposed one offset "at the pupil centre in `eyes()`". The real code has no such point: `eyes()` places whole eyes, and `one_eye()` hands off to about 20 eye styles (`EYE_COUNT`), each drawing its own pupil (or none). Shifting `(ex, ey)` moves the white and the lid with the pupil, which does not read as a glance. A real gaze control needs a per-style pupil offset, which is too much code for 2026. The cold open's glances are staged with `headturn`, expressions, blinks and cutaways instead.

**How the override variables are used.** They are defined in `show/engine/show_actor.c`. The player sets them around each actor's image fetch and resets them to -1 after. With (f), every fetch completes inside the call, so no sliced job ever crosses an actor boundary.

### 4.4 The game-unchanged gate: `show check-game`

v1.1 makes the gate cheap, because the show cannot change a game file:

1. **Every show commit:** `git diff --quiet <merge-base> -- core gba ps1 tools docs/show trailer` must be empty, and the commit may add files only under `show/`. This takes milliseconds and runs as a pre-commit check.
2. **Weekly, and before each release:** build the `ps1` preset of the game into a scratch directory and compare the SHA-256 of `doomogachi.exe`, `OVL.BIN` and `TEXT.BIN` with a build of the merge-base. **Do not compare the `.bin` disc image:** mkpsxiso stamps the directory records with the build time, so two builds of identical inputs differ in about 70 bytes (m: `critique/iso/a.bin` vs `b.bin`, the date fields plus sector EDC). Measured: the EXE, OVL.BIN and TEXT.BIN are byte-identical across two builds in different directories (c0c19b66…, 982bdfbb…, 50d371f0…), and a full build takes 83 s on 4 CPUs.
3. `make -C core test` and the native runner's test scripts, on the same weekly schedule.

Any difference fails the show change. The show shares the game's working branch (`claude/nice-allen-gr6czm`), so game changes reach it on every merge; a patch that no longer applies is fixed in `show/engine/patches/` the same day.

---

## 5. The script format (`.shw`)

### 5.1 Files

| File | Holds | Owner |
|---|---|---|
| `series/show.shw` | Canvas, fps, doc canvas, safe areas, rating profile, series seed, lint thresholds | Creator + Claude |
| `series/cast.shw` | Characters: look (catalogue values) or roster id, image kinds, idle phase, squash material, voice id, tags (KID, TEEN, SILENT) | Creator + Claude |
| `series/voices.shw` | Voices: engine, Kokoro voice or blend, speed, WORLD shaping, take policy | Creator |
| `series/lexicon.shw` | Pronunciations of names and coinages (Giggs, Hellvile, Bonejangles, Mascotty, MINGLE…) | Claude |
| `series/visemes.shw` | The 8 visemes mapped to `MOUTH_*` styles per look family; mouth timing rules | Claude (look-dev with the creator) |
| `series/looks/*.shw` | Show-only looks (Mascotty, the crayon young cherub, kids, Fluffy) as `DoomoLook` text | Claude |
| `series/sets/*.shw` | Sets: kind, planes, props, marks, paths, locks, variants by look and light | Claude |
| `series/acting.shw` | Acting and gag macros (double take, rule of three, freeze 13, slow burn, whiplash insert) | Claude |
| `series/sfx.shw`, `music.shw` | SFX and foley names → sources, gains, pans, SDH captions; cues → scores, bar maps, stems | Claude |
| `series/ratings/tvma.shw`, `lints.shw` | Content switches and the hard-limit rules | Creator |
| `epNNN/episode.shw` | Rundown: acts, beats, lengths, elastic and protected flags, breaks, versions | Creator + Claude |
| `epNNN/lines.shw` | Dialogue: stable id, speaker, text, direction, SDH overrides | Creator + Claude |
| `epNNN/takes.toml` | Which take each line id uses, and `retime = "fit" \| "take"` | **Creator** |
| `epNNN/strings.shw` | On-screen text: SUPERs, signs, cards (localisable; textless is a switch) | Creator + Claude |
| `epNNN/cues.shw`, `ad.shw` | Music spotting; audio-description lines | Claude |
| `epNNN/shots/*.shw` | Shots grouped by act | Claude, edited with the creator |
| `epNNN/voice.lock` | **Generated, committed.** Take hashes, onsets, word and phoneme times, mouth tracks | `show voice` |

### 5.2 Lexical rules

- One statement per line. `;` chains statements at the same time. `#` starts a comment. Indentation is ignored.
- A block (`shot`, `set`, `macro`, `crowd …`) ends at `end`.
- Engine constants use the repo's own names (`SHK_RUMBLE`, `TWT_POP_SMALL`, `EXPR_HAPPY`, `PAL_CRAYON`, `SPECIES_CHERUB`). They are validated through `load_symbols()`, so the show and the game can never disagree on an enum.
- Strings are double-quoted and may carry the game's inline text codes (`{d}`, `{w}`, `{big}`…`{/}`, `{..}`).
- **Units.**
  - Positions and speeds are show pixels (px and px/f).
  - `doc:` marks a number in the docs' 384x216 units. It is converted ×5/6 on the default canvas and ×1 on the wide one, and both directions are printed by `show doc2px`.
  - Times are frames unless suffixed: `s` for seconds (must be a whole frame count), `smp` for samples, `R8` for rungs.

### 5.3 Time expressions

| Form | Meaning |
|---|---|
| `+N`, `+R8`, `+1.5s` | N frames, a rung, or seconds after the shot's frame 0 (the shot list's own notation) |
| `+N..+M` | A span. Ops with a duration take it (`over=+30..+132`) |
| `>+N`, `>+R8` | N frames after the previous timed line in this shot (comic chains) |
| `@LABEL[+N]` | From a label in this shot. `L_PUNCH` and `L_BUTTON` are emitted as real `beat.h` labels |
| `@<line>.start / .end / .w<k> / .p<k>` `[+N]` | A dialogue line's first sound, last sound, k-th word or k-th phoneme, read from `voice.lock`. Until a take exists, a per-voice rate estimate is used and the event is marked **provisional** |
| `@bar:B.b[+N]` | Bar B, beat b of the shot's active cue (`cue=`), from its bar map in samples, snapped to the nearest frame. The error is at most ½ f, counted from the bar line, so it never accumulates |
| `@<shot>.end[+N]`, `@<shot>+N` | Another shot's frame. The compiler checks that the result lands inside this shot |

The compiler resolves every expression to a frame inside its shot (and to a sample for audio). It fails with the anchor chain printed if an event leaves the shot.

### 5.4 Shot header

```
shot <id> beat=<beat> len=<N | bar:N | @<line>.end+N | auto> set=<set>[:<variant>] [px=1|2|3]
     [look=pixel|crayon|sepia|lcd|hellify] [cut=CUT|SMASH|MATCH:<shot>|FADE|IRIS|WIPE:brick]
     [matte=open|black] [cue=<cue>@bar:B] [gore=CUTE|BRUTAL] [text=on|off] [seed=N] [v916=x|keys]
     [base=<shot>@<frame>] [replay=<shot>@<from>] [insert]
```

- **`base=<shot>@<frame>`** starts the shot from another shot's exact state at that frame. The player re-derives it by running the source shot silently, so no state carries over by accident. Used for shot 18 = shot 16@+39 and shot 9 = shot 2@+90.
- **`change <obj>.<field>=<value>`** (setup lines only, with `base=`) declares each difference from the base. Lint `COMP-ONECHANGE` counts them: the whiplash rule allows exactly one.
- **`replay=<shot>@<from>`** re-runs another shot's frames from a frame (shot 17 = shot 7 from +8). With `insert` it is linted for ≥ 4 f and owning its audio.
- **`len=auto`** with `retime = "take"` lets a recorded take drive the shot's length. Every later event in the beat that is anchored to it moves with it, and the compiler prints the new cut next to the shot list's number.

### 5.5 Statements

**Setup statements** (no time; they define frame 0):

| Statement | Example |
|---|---|
| `look <register> k=v…` | `look crayon grain=12 boil=2` |
| `cam at=x,y [scale=s]` · `cam drift dx= dy= [zero=+N]` | `cam drift dx=doc:1.0 zero=+90` (camera x is 0 at +90) |
| `actor <id> who=<char> kind=bust\|figure\|chibi\|fighter\|model3d at=x,y [face=L\|R] [expr=EXPR_*] [idle=on\|still\|frozen\|none] [pose=<flipbook>]` | `actor mayor who=mayor kind=figure at=196,96 face=L idle=on` |
| `prop <id> art=<src> at=x,y [plane=] [face=]` | `prop balloon art=pxt:cupcake_balloon at=160,52 face=front` |
| `crowd <id> … end` | §6.4 |
| `change <obj>.<field>=<v>` | `change balloon.face=wall` |
| `assert <expr> == <expr> [@<time>]` | `assert bandstand.x == 160 @+90` |
| `waive <RULE> "<reason>"` | `waive CANON-HV107 "storybook flashback: crayon imp kids, no faces"` |
| `note "<text>"` | Carried into sheets and the dialogue list |

**Timed ops** (`<time> <op> <args>`). Engine-native ops compile through `compile_beat`. Ops marked † are show ops on `BOP_SHOT`.

| Group | Ops |
|---|---|
| Motion | `move† A to=x,y over=F tw=TW_*`, `walk† A to=x over=F`, `pop A TWT_*`, `squash† A <q> over=F`, `headturn A <brad> <F>`, `face† A L\|R`, `pose† A <flipbook> [fps=]` |
| Acting | `expr A EXPR_*`, `blink† A [F]`, `eyes† A L\|R\|U\|D\|UL… over=F` (deferred with gaze, §4.3), `still A`, `freeze A`, `thaw A`, `panic A`, `twitch A on\|off`, `state† A possessed\|ghost\|dead\|normal`, `take† <macro> A k=v…` |
| Standards | `tag† <shot\|line\|scene> innuendo\|implied\|offscreen\|suggestive\|advance\|yes\|no\|stepback\|gore_truth\|rear` and `scene <id>` (§13.5): required wherever they apply; the lints read them |
| Dialogue | `say† <line> [A] [mouth=auto\|none\|"<track>"]`, `hum† A on\|off note=<cue>.<note>`, `walla† <bed> on\|off\|to gain= [over=]`, `stop† <line>` |
| Text | `super† <string> style=type\|pop\|deadpan at=title\|lower\|x,y [over=+a..+b \| cps=N] [until=<time>\|end]`, `sign† <string> on=<prop>`, `card† <template> k=v…` |
| Camera | `cam† to x= y= scale= over=F ease=IN\|OUT\|INOUT\|LINEAR`, `cam† pan dx=`, `cam† creep dx=`, `cam† whip L\|R over=F`, `cam† crane dy= over=`, `cam3d† rail=<path> from= to=`, `zoom <q8> <F>` |
| Effects | `shake SHK_*`, `punch dx dy`, `flash PAL_* <lvl> <F>`, `fade in\|out over=F`, `iris_in\|iris_out at=prop:<id>\|x,y over=F shape=circle\|heart [hold=F]`, `pal PAL_* <lvl> over=F`, `tint TINT_* <lvl>`, `hitstop F`, `reveal† <stroke> over=+a..+b`, `fx† <kind> at= n= seed=`, `boil† on\|off [every=2]` |
| Audio | `sfx† <id> [gain=] [pan=auto\|L\|R\|x]`, `fol† <id> [follow=<stroke>\|from=crowd:<ids>]`, `amb† <id> on\|off`, `mx† <cue> start\|stop\|mute F\|duck <g> F\|bend <a>-><b> over=F`, `tapestop† F`, `cc† "<SDH text>"` |
| Markers | `label NAME`, `punch` (= `label L_PUNCH`), `button` (= `label L_BUTTON`), `nobutton`, `ad† <ad id>\|"<text>"` |

`waita` is forbidden (lint `SHW-WAITA`), because a show never waits for a button. `flag`, `sfx`, `music`, `mute`, `duck` and `tapestop` in their engine-native form are forbidden too (lint `SHW-OPS`): `flag` writes the game's story state, and sound is a show op (below).

**Scenes.** A shot header may carry `scene=<id>`; consecutive shots share the previous shot's scene unless they declare a new one. A scene is one continuous time and place, as the docs define it (`SERIES_OVERVIEW.md` §2, hard limit 3), and it is the unit the standards lints work on (§13.5).

**Audio ops never become `BOP_SFX`.** Every sound is a show op carrying its event index *k* in `audio_events.json`. The PS1 publishes the *k* values it fired each frame (P4), so the host proves the mix is in sync with the master picture.

### 5.6 Dialogue lines and takes

```
# ep101/lines.shw
line co13_heehee  mayor  "...Hee. Hee."   take=lib:mayor/heehee_v1 lock=identical asr=skip
line co15_perfect mayor  "Perfect."       dir="pleased with himself, quiet"
line co18_stay    mayor  "Stay."          dir="firm, to the balloon"
```

```toml
# ep101/takes.toml  (creator-owned; a line not listed uses its default order)
[co15_perfect]
take = "rec:show/lib/takes/mayor/co15_perfect_t3.flac"   # the creator's own read
retime = "fit"         # keep the written timing; lint AUD-OVERRUN if the take overruns its slot
[co18_stay]
take = "tts"           # the speaker's voice from series/voices.shw
retime = "take"        # the take's length drives len=auto and every anchor after it
```

**Line tags.** Every line in `lines.shw` may carry standards tags (`tags=innuendo,advance to=<char>` or `tags=yes|no|stepback`, and `ref=<char>` when the line is about someone). Claude tags every line when it writes the script; the lints in §13.5 read the tags, and `STD-TAGSCAN` warns when an untagged line matches the innuendo word list.

**Take order per line id:**
1. `rec:` (a recording);
2. `scratch:` (the creator's guide read);
3. `lib:` (a library take);
4. `tts`.

Line ids are stable forever, and `show ids new ep101 co18` hands out the next free one.

---

## 6. Worked example: the cold open's first three shots

The source is `COLD_OPEN_SHOTLIST.md` shots 1-3 (CO-1 and the start of CO-2, TC 00:00:00:00 to 00:00:13:00, 390 f). Every number traces to the shot list. Numbers the shot list does not give are marked `# choice:` so the creator can see and change them.

**Note on dialogue.** Shots 1-3 have **no spoken lines**: the storybook has "no narrator", and the series' first spoken words are the Mayor's in shot 13. The only voices are the "soft crowd babble" of shots 2-3. They are written below as a kids' walla bed of scripted lines, voiced through the same TTS pipeline as dialogue (adult voices, pitched; the bible says every child is voiced by an adult). M1 therefore also renders a **dialogue proof**: the Mayor and Nana talking in a two-shot and close-ups (§17, M1-T10).

### 6.1 Series files these shots need

```
# show/series/show.shw
show doomogachi fps=30 audio=48000 canvas=320x180@320x240 doc_canvas=384x216 rating=tvma seed=0xD00D
  safe title=16,9 action=8,5
  palette core                       # doomo_palette, 256 entries, index 0 transparent
end
```

```
# show/series/sets/storybook.shw   (crayon register; everything palette-exact)
set calendar_page kind=2d px=2 matte=open look=crayon
  plane paper  image=gen:paper_sheet(seed=1)                      size=160x120 par=0
  plane page   image=gen:calendar_page(month=JUN,mark=MIDSUMMER,seed=3) par=0
  strokes ink  par=0 boil=2                                       # live GPU lines, jittered ±1 px on twos
    stroke circle_midsummer   nib=crayon:CRAYON_RED  w=2 path=ellipse(194,96,46,18,turns=1.15,wobble=1,seed=11)
    stroke fireworks_scribble nib=crayon:CRAYON_PINK w=2 path=gen:scribble_bursts(cx=194,cy=96,n=5,r=28..52,seed=12) outside=on
  end
  plane grain  tile=gen:paper_grain(density=12,seed=2) size=64x64 par=0 order=top   # "12 %": a fixed density, never alpha
end

set park_storybook kind=2d px=1 matte=open look=crayon
  plane sky    image=gen:crayon_sky(night=1,stars=40,seed=21)        size=352x240 par=0.05
  plane park   image=gen:storybook_park(river=1,bridge=1,seed=22)   size=368x240 par=0.10
  plane near   image=gen:lantern_strings(seed=23)                   size=400x240 par=0.20
  prop  bandstand art=gen:bandstand_round(lanterns=12,seed=24) plane=park at=160,92 anchor=center
  prop  table_n   art=gen:table(cupcake_tower=1,seed=25)       plane=park at=116,70
  prop  table_s   art=gen:table(cauldron=1,seed=26)            plane=park at=206,128
  plane grain  tile=gen:paper_grain(density=12,seed=2) par=0 order=top
  mark  wall_x 160                                               # doc:192; every brick wipe and Split uses it
end

set bridge_storybook kind=2d px=2 matte=open look=crayon
  plane sky    image=gen:crayon_sky(night=1,stars=12,seed=31)  par=0
  plane water  image=gen:crayon_river(seed=32)                 par=0 boil=2
  plane bridge image=gen:crayon_bridge(side=1,seed=33)         par=0
  plane rail   image=gen:crayon_bridge_rail(seed=34)           par=0 order=front    # kids walk behind the rail
  path  deck   y=118 from=-32 to=352
  plane grain  tile=gen:paper_grain(density=12,seed=2) par=0 order=top
end
```

```
# show/series/sfx.shw (excerpt)
sfx FX_SHWIP          src=sfxgen:SHWIP@48k            gain=-10dB pan=center
fol FOL_crayon_scratch src=foley:crayon(stroke_speed) gain=-14dB cc="[crayon scratching]"
fol FOL_paw_patter    src=foley:paws(steps=crowd)     gain=-18dB cc="[small feet patter]"

# show/series/music.shw (excerpt)
cue mingle_box src=mml:core/src/mml_mingle.c bpm=96 meter=4/4 key=F       # parsed by importing tools/mmlc.py
  track 0 inst=musicbox ; track 1 inst=warm_pad gain=-9dB ; track 2 inst=chip_tri gain=-8dB ; track 3 inst=drums_pop:70 gain=-20dB
  # kit preset names as they exist in theme/kit/kit/render.py (v1.0's pad_soft/tri/shaker are not kit presets);
  # GM note 70 is the kit's synthesised shaker
  # 96 bpm: one beat = 18.75 f = 30,000 smp; one bar = 75 f = 120,000 smp (bars land on frames)
end

# show/series/voices.shw (excerpt): the storybook kids' walla
voice kids_walla engine=kokoro pool=af_sky,af_river,am_puck,bf_lily pitch=+4..+6st speed=1.10..1.20 seed=7
```

### 6.2 Episode files

```
# show/ep101/episode.shw (excerpt)
@episode e101 "Mingle Night" shape=B fps=30 matte=open rating=tvma seed=101
act CO "Cold open"
  beat e101_co_1 len=90   shots=co01
  beat e101_co_2 len=900  shots=co02..co08
  beat e101_co_3 len=240  shots=co09
  beat e101_co_4 len=450  shots=co10..co11
  beat e101_co_5 len=630  shots=co12..co18
  beat e101_co_6 len=690  shots=co19..co21
```

```
# show/ep101/strings.shw (excerpt)
string s_co02 "Every Midsummer night, the whole park had one party." font=child_hand   # M1: game font; child_hand lands in M2
string s_co03 "It was called the Mingle."                             font=child_hand

# show/ep101/lines.shw (excerpt): walla is scripted, so it can be swapped, re-voiced and logged like dialogue
line w_kids_01 walla:kids_walla "Cupcakes!"
line w_kids_02 walla:kids_walla "Wait for me!"
line w_kids_03 walla:kids_walla "Soup! There's soup!"
line w_kids_04 walla:kids_walla "Look, lanterns!"
line w_kids_05 walla:kids_walla "Race you to the bridge!"
line w_kids_06 walla:kids_walla "Hi! Hi! Hi!"
walla kids_far lines=w_kids_01..w_kids_06 density=1.4/s pan=spread seed=7   # captioned by the shot's `cc`

# show/ep101/cues.shw (excerpt): defaults for the cue; the start time lives in the shot (co01 +60)
spot mingle_box from=bar:1.1 bus=MX gain=-12dB               # enters at 00:00:02:00 (shot list)
```

### 6.3 The shots

```
# show/ep101/shots/co.shw  (COLD_OPEN_SHOTLIST.md; show px on 320x180; doc: = 384x216 units)

shot co01 beat=e101_co_1 len=90 set=calendar_page px=2 look=crayon cut=FADE matte=open scene=co_storybook
  look crayon grain=12 boil=2
  +0    fade in over=R8                                       # choice: FADE IN length not in the shot list
  +0    fol FOL_crayon_scratch follow=circle_midsummer ; reveal circle_midsummer over=+0..+24
  +30   reveal fireworks_scribble over=+30..+60 ; fol FOL_crayon_scratch follow=fireworks_scribble gain=-3dB
  +60   mx mingle_box start                                   # sample 96,000 of the episode
  ad "A crayon circles Midsummer on a calendar, then scribbles fireworks round it."
end

shot co02 beat=e101_co_2 len=180 set=park_storybook px=1 look=crayon cut=CUT matte=open
  cam at=0,0 ; cam drift dx=doc:1.0 zero=+90                   # planes ride it at x0.05/0.10/0.20 (doc px/f)
  assert bandstand.x == 160 @+90                              # composition lock; co09 is base=co02@+90
  +0    iris_in at=prop:bandstand over=13 shape=circle ; sfx FX_SHWIP
  +0    walla kids_far on gain=-28dB ; cc "[soft crowd babble]"
  +30   super s_co02 style=type over=+30..+132 at=lower until=end
  ad "One park, no wall: a river, a bridge, a round bandstand strung with lanterns."
end

shot co03 beat=e101_co_2 len=120 set=bridge_storybook px=2 look=crayon cut=CUT matte=open
  crowd bunnies gen=randomize seed=301 n=6 species=SPECIES_BUNNYKIN tags=KID age=child kind=chibi
    lane=deck dir=R from=-32 spacing=26 speed=doc:2.2 ripple=0..3 cycle=walk4
  end
  crowd imps gen=randomize seed=302 n=6 species=SPECIES_IMP tags=KID age=child kind=chibi
    lane=deck dir=L from=352 spacing=26 speed=doc:2.2 ripple=0..3 cycle=walk4 dy=+6
  end
  waive CANON-HV107 "storybook flashback: crayon imp kids crossing, no featured faces (shot list co03)"
  +0    fol FOL_paw_patter from=crowd:bunnies,imps ; walla kids_far to gain=-20dB over=30   # "babble swells"
  +13   super s_co03 style=type at=lower until=+63
end
```

### 6.4 What the compiler resolves (printed by `show compile ep101 --shots co01..co03`)

| Shot | Episode frames | TC in | Len | Set / px | Sound on frame 0 | Key events (episode frame / sample) |
|---|---|---|---|---|---|---|
| co01 | 0-89 | 00:00:00:00 | 90 | calendar_page / 2 | `FOL_crayon_scratch` (smp 0) | fade 0-7; reveal 0-24 and 30-60; MINGLE bar 1.1 at f 60 = **smp 96,000** |
| co02 | 90-269 | 00:00:03:00 | 180 | park_storybook / 1 | `FX_SHWIP` (smp 144,000) | iris 90-103; SUPER glyph 1 at f 120, glyph 52 at f 222 (15.3 cps); camera x = 0 at f 180 |
| co03 | 270-389 | 00:00:09:00 | 120 | bridge_storybook / 2 | `FOL_paw_patter` (smp 432,000) | walla to −20 dB over 270-300; SUPER 283-333 |

**Camera.** 1.0 doc px/f becomes 0.8333 px/f, so the planes move at 0.0417, 0.0833 and 0.1667 px/f (the docs' 0.05, 0.10 and 0.20 ×5/6). They accumulate in Q16, so the fractional rates are exact. The near plane travels 30 px over the shot, inside its 400 px width with an 8 px overscan.

**The co02 SUPER.** 52 characters typed over +30..+132 is 15.3 cps. The compiler emits one `BOP_SHOT` text event per glyph frame, and `show timing` lists each glyph's frame.

**What `compile_beat` receives for co02.** These are the engine-native ops only, all anchored at the shot start:

```
+0 iris_in 1 5           # centre = the bandstand's prop slot 1; b = hold 5 | heart<<6 | (radius/2)<<7
+180 end                 # compile_beat refuses a script without `end`
```

**The iris encoding (fixed in v1.1).** `BOP_IRIS_IN`'s `b` is `hold | heart<<6 | (radius/2)<<7` (beat.h), not a duration, and `camfx_iris_in()` always takes R3 to snap plus `hold` plus R5 to open (camfx.c `IRIS_SNAP + iris_hold + IRIS_CLOSE`). So `over=13` compiles to **hold 5** (3 + 5 + 5 = 13 f), and the compiler rejects an `over=` below 8. v1.0's `iris_in 1 13` would have run 21 f. The op is a host kind (§4.2), so `show_ext.c` resolves prop slot 1 to the bandstand's screen position and calls `camfx_iris_in()`. `fade in over=R8` likewise needs the player to set `camfx_fade(16)` at the shot's setup, because `BOP_FADE` only ramps toward a level from the current one.

The show ops become `BeatEv{at, 0, BOP_SHOT, 0x4000|op, argidx}` and are merged into the same sorted stream: `SOP_CAM_DRIFT`, `SOP_AUDIO k=3` (SHWIP), `SOP_AUDIO k=4` (walla on), `SOP_SUPER`, and the per-glyph `SOP_TEXT_STEP`.

### 6.5 What `show lint ep101 --shots co01..co03` prints

```
show/ep101/shots/co.shw:30: co03 +13 [TIME-READ] WARN SUPER s_co03 "It was called the Mingle." is on screen 50 f
  (+13..+63); 5 words need 60 f (9 f/word + 15). The shot list sets +13..+63. Keep it (`waive TIME-READ "..."`)
  or end it at +73.
show/ep101/shots/co.shw:14: co02 [COMP-LOCK] ok bandstand.x == 160 at +90 (episode frame 180).
show/ep101/shots/co.shw:28: co03 [CANON-HV107] waived: storybook flashback: crayon imp kids crossing, no featured faces.
ep101 co01..co03 [AUD-FRAME0] ok: every cut has a sound on frame 0 (smp 0, 144000, 432000).
ep101 co01..co03 [STD-AGE] ok: 12 crowd members age=child; no cast without an age.
ep101 scene co_storybook (co01..co03) [STD-KIDS] ok: children in the scene; no innuendo/suggestive/advance tags,
  no truth insert, gore=CUTE throughout; STD-TAGSCAN: 0 untagged matches in 6 walla lines and 2 SUPERs.
ep101 co01..co03 [PLAN] co01 38 KB chunk, co02 61 KB, co03 44 KB; VRAM set pool 2/5 pages; badge PS1-LEGAL (est.).
```

The TIME-READ warning is real arithmetic on the docs' numbers: 9 × 5 + 15 = 60 > 50. The creator decides, and this is exactly the kind of note the loop is built for.

### 6.6 Audio events (excerpt of `audio_events.json`)

```json
[{"k":0,"smp":0,"src":"FOL_crayon_scratch","follow":"co01:circle_midsummer","bus":"FX","gain_db":-14},
 {"k":1,"smp":48000,"src":"FOL_crayon_scratch","follow":"co01:fireworks_scribble","bus":"FX","gain_db":-17},
 {"k":2,"smp":96000,"src":"cue:mingle_box","from":"bar:1.1","bus":"MX","gain_db":-12},
 {"k":3,"smp":144000,"src":"FX_SHWIP","bus":"FX","gain_db":-10,"pan":0.0},
 {"k":4,"smp":144000,"src":"walla:kids_far","op":"on","bus":"DX","gain_db":-28,"cc":"[soft crowd babble]"},
 {"k":5,"smp":432000,"src":"FOL_paw_patter","from":"crowd:co03.bunnies,co03.imps","bus":"FX","gain_db":-18},
 {"k":6,"smp":432000,"src":"walla:kids_far","op":"to","gain_db":-20,"over_smp":48000}]
```

---

## 7. The compiler, `showc`

### 7.1 Phases (`show compile ep101`)

1. **Parse and expand.** Read the series, episode and shot files and expand macros. Every node keeps its file and line.
2. **Voice resolve.** Read `voice.lock` and `takes.toml`. Lines whose text, voice parameters or take changed are flagged. `show compile --voice` runs `show voice --changed` and loops once more (the two-pass dependency: provisional times, then final times).
3. **Time resolve.** Build the anchor graph (labels, chains, words, bars, cross-shot) and solve it to frames and samples. Check `len=` against the rundown, check that each beat equals the sum of its shots, and check the episode total (39,600 f, `FMT-RUNTIME`).
4. **Stage resolve.** Bind sets, planes, props and actors. Record crowd look seeds (the PS1 generates the looks at prep, §4.2), resolve `base=` and `replay=` sources and check them for cycles, and snap positions to `px`.
5. **Bake.** Run the plate generators (`show/art/gen/*.py`) to indexed PNGs (cache). Then tile and pack them into 8bpp pages with the game's own LZ format, so the PS1 needs no new decompressor, and convert `.pxt` sprites and fonts.
6. **Plan.** Per shot: chunk bytes, VRAM pools, image-pool blocks, prim-byte estimate, actor and crowd counts, a badge estimate.
7. **Lint** (§13.5).
8. **Emit.** `EP101.SHW`, the JSON sidecars, `plan.txt`, `lint.txt`, and a content hash of every chunk, which is the render-cache key input.

### 7.2 Speeds (targets, measured in WP4)

- Compiling one shot after an edit: under 1 s.
- A full episode with a warm bake cache: under 20 s.
- A cold bake: minutes, cached per generator input hash.

### 7.3 `EP###.SHW` (PS1 side; `include/show/shw_format.h`)

```
EpHeader  { magic 'SHW1', abi u16, nshots u16, fps u8=30, px_canvas {w,h,y_off}, seed u32,
            toolchain_hash[8], strtab_off u32 }
ShotIndex [nshots] { start_frame u32, len u16, px u8, look u8, chunk_sector u32, chunk_bytes u32,
                     base_shot u16, base_frame u16, replay_shot u16, replay_from u16, flags u16, seed u32 }
ShotChunk { ShotHdr { set_kind, plane_n, actor_n, prop_n, crowd_n, ev_n, track_n, vram_plan[], badge_est }
            LOOK[]    48 B DoomoLook per cast member / crowd citizen (no roster linked: saves 164 KB)
            Planes[]  { kind BANDS|IMAGE|TILEMAP|STROKES|GRAIN|VIEW3D|OVERWORLD|ROOM|CUTSTAGE, par_q16, order, … }
            Actors[]  { kind, look_idx, x, y, face, expr, idle_mode, phase, px }
            Crowds[]  { look_base, n, lane, dir, spacing_q8, speed_q16, ripple_seed, clock_mask }
            CamKeys[], BeatEv[ev_n], ArgTable[], Tracks[] { actor, MOUTH|EXPR|EYES|BLINK, RLE per frame },
            Strokes[] (polyline points, nib, width, reveal spans), Strings (textpack format),
            TexPages[] (8bpp, LZ), CLUTs[] }
```

- A shot chunk is ≤ 160 KB. A bigger shot gets a second, texture-only chunk that is loaded during prep.
- A mouth track costs about 0.5 B per speaking actor per frame.

---

## 8. The show player (`ps1-show`)

### 8.1 Main loop (capture mode)

For each tick:
1. `hcmd_poll()` (the game's lockstep stop, unchanged).
2. `show_mbox_poll()`: seek requests from Lua.
3. If *t* = 0: bind the chunk, reset (§8.2), and pre-roll `base=`/`replay=` logic with drawing skipped.
4. `beat_tick()` until `BT_RUN`/`BT_DONE`, with `BT_EXT` going to `show_ext`.
5. `camfx` update; tracks at *t*.
6. Draw: planes → actors → props and crowds → FX → text → matte bars.
7. P1 and P2 are final. Publish P1, P2, P4 and the stats to `g_show_mbox`.
8. End the frame.

### 8.2 Purity: what resets at every shot start

`camfx_reset()`, the beat script, actor slots, crowds, the text state and frame counters reset. The RNG is seeded from `episode seed ⊕ shot seed`. Then:

```c
portrait_cache_reset();                         /* portrait.c:915: drop every cached image          */
/* every image fetch, every tick: */
portrait_cache_clock(actor_clock(a), 6);        /* steps = DL_STEPS: every phase complete on its frame */
```

`portrait.c:408` sets `PC_STEPS = 3` under `__mips__` and `DL_STEPS` (6) on the host. Without this graft, the blink, breath and wing phase on screen would depend on where a render started and on the platform. Passing 6 steps makes each phase render whole on the frame it is due **only if no other fetch already spent that tick's budget**; patch hunk (f) (§4.3) removes the budget, which is what actually guarantees it when several actors share a clock value.

Each actor's clock is `shot_frame + phase(actor id)`, held while FROZEN or STILL (driven by `beat.c` clocks) and doubled under PANIC. It is set with `portrait_cache_clock` before that actor's fetch. This gives frame-exact blinks (`(t + off) % 110 >= 106`) and wing flutter (`(t >> 3) & 1` for winged figures), and lets FREEZE stop the wings.

Seeking runs the shot from frame 0 with drawing skipped, so any frame can be rendered alone. On the PS1 the `plat_*` calls do not check `plat_draw_skipped()` (only the GBA's do; its weak default in `ui_raster.c` returns 0), so the show's own draw loop skips the draw step itself during pre-roll, and still fetches portrait images so the cache state matches a full run.

### 8.3 Stage kinds

| Kind | Draws with | First used |
|---|---|---|
| `BANDS` | Palette-step gradient as TILE bands (never Gouraud) | M1 skies |
| `IMAGE` | Baked plates through `plat_image` (SPRT at px 1, nearest quads at px 2-3), parallax in Q16 | M1 |
| `TILEMAP` | 16x16 tiles from 8bpp pages plus a map (wide parallax plates) | M2 |
| `STROKES` | Live `plat_line` polylines with reveal by arc length and ±1 px boil on twos | M1 (crayon; drops to flat plates at CP1) |
| `GRAIN` | A 64x64 two-tone paper tile at fixed density | M1 |
| `VIEW3D` | `plat_view3d` on **the game's existing maps** (`core/gen/map_*.c`: arena, vape cellar, bone cathedral, hellvile underbelly, …) with a `cam3d` rail and the HUD. New show maps through `mapc.py` are deferred | M3 (W5: 3-03, 3-05) |
| `OVERWORLD` | `plat_overworld` on **the game's existing overworld maps** (hahaland, hahaland_invaded, park_united, hellvile) at a show camera, `daylight` 0-15; `doomo_town.res[]` filled from LOOK (town.c is linked anyway) | M3 (W4) |
| `ROOM` | `plat_room` (ps1/src/room.c; 2.5D house; `room.c:2314 c.vw <= 320` keeps the backdrop cache on the default canvas). **Needs the house modules** `home.c`, `home_draw.c`, `home_edit.c` and `furn_*` linked read-only for `home_room_template`, `home_room_defs`, `home_obj_view`, `home_furn` and `home_furn_by_role` (m: the link probe, §15.1) | M3 (W4: the cottage) |
| `CUTSTAGE` | **Not available (v1.1).** The 24 `CUT_*` stages are static functions inside scene files (`cutscene*.c`, `cutstage.c`) driven by scene state and the scene scratch, so using one means linking the story scenes. The crayon prologue's drawing (`cutscene_ch12.c` `draw_p0()`: paper, hills, bridge, kids, cherub, fireworks, the Wall) is the reference for the show's own crayon generators instead | — |

### 8.4 Actors

- **Resident slots.** Busts (`portrait_image(rid, expr)`) and chibis (`bodyspr_image(rid, dir, frame)`) take resident ids, which `portrait.c`'s `res_of()` resolves through `town_res()`. v1.0 replaced `town_res()` and dropped `town.c`; that cannot link, because `beat.c` (`set_flag`) and `hcmd.c` both reference `doomo_town` directly (m: the link probe). v1.1 links `town.c` (its `doomo_town` is 107 KB of .bss (m)) and `show_cast.c` writes the shot's looks into `doomo_town.res[0..255]` at prep, so the game's own `town_res()` serves them. Figures (`figure_image(look, …)`) take the look directly.
- **The portrait pool** grows to 90 KB (`-DPC_BLOCKS=240`; **not** 256, which collides with the `PC_FREE` marker in the `uint8_t` owner table, §4.3). One block is 384 B: a chibi frame takes 1 block, a 48x56 bust 7, and a 48x80 figure 10.
- **Each mouth and blink state is its own cached image.** A talking bust cycling 6 mouths × 2 blink states needs about 84 blocks (32 KB). Images render at prep or on first use. The 30 ms PS1 render cost is invisible in lockstep capture.
- **Poses the engine lacks** (ladder climb, roller stroke, reach, pat, the cloud's heart eyes) are **flipbooks**: hand-drawn by the creator or generated on the host, palette-exact, and drawn as `plat_image` frames layered on the figure. The show needs no rotation primitive.

### 8.5 Crowds

- Each citizen has its own idle phase from its id, a 0-3 f ripple from a seeded hash of the id, a clock that can be stopped by name, lanes or formations, and walk cycles.
- **M1** (co03, 12 kids) fits the enlarged pool directly.
- **M2 and later wides** (co19, 150-250 citizens) use **K-look reuse (v1.1)**: K unique looks (K ≤ 48) × the frames a lane needs (4 for a walk in one direction) stay in the pool (≤ 192 blocks), and the crowd draws each of those `DoomoImage`s many times at different positions and flips with the stock `plat_image`. The VRAM image cache uploads each image once; 200 citizens cost about 200 SPRTs (a few KB of the 56 KB display list). This needs no new platform code.
- **Deferred: the atlas with recolour CLUTs** (more visual variety than K looks, a show-owned VRAM region, `ps1/show_crowd_ps1.c`). The planner already counts pool blocks, so the atlas can drop in later without changing scripts.
- Lint `TIME-RIPPLE` checks the 0-3 f staggers. Mascotty is a crowd member with clock `none`.

### 8.6 Camera, FX, text

- **Camera:** keys, drift (`zero=`), pans, creeps, whips and cranes in Q16; per-plane scale for scale moves; `cam3d` rails keyed per frame.
- **FX:** `camfx` for shakes (14 profiles), punch, flash, fade, irises (circle and heart) and the SEPIA, CRAYON, HELLIFY and LILAC palette lerps. `show_fx` adds seeded palette particles, the crayon register, the brick wipe (5 courses × 2 f) and Split along x = 160, the credits dither dissolve (13 f, 4x4 ordered masks) and the sticker slap.
- **Text:** SUPER styles are `type` (per-glyph schedule, letter hop), `pop` (4-6 f overshoot) and `deadpan` (appears whole). Signs sit on props, and cards come from templates. Show fonts are `.pxt` glyph sets (the child's hand: 8x12, 1-bit plus CLUT), loaded as font pages next to the game's 8x8 font.

### 8.7 Memory plan, `ps1-show` (v1.1: measured by a link probe)

**The link probe (m, 2026-10-25, `critique_nye/probe/`).** A `show_probe.c` that defines what `game.c` defines and calls the APIs the player uses (busts, figures, chibis, beat, camfx, tween, text, and in the 3D variant `plat_view3d`, `plat_overworld` and `plat_room`) was linked with `--gc-sections` against a fresh build of the game's own objects, with no game file changed, and run through the game's `elf2exe` RAM gate:

| Probe | Code + data | .bss | `elf2exe` |
|---|---:|---:|---|
| 2D (portraits, beat, camfx, ui, town, options, packed art) | 412 KB | 389 KB | **1,182 KB left** below the stack top |
| 2D + VIEW3D + OVERWORLD + ROOM (+ map, maps, home, home_draw, home_edit, furn_*) | 556 KB | 465 KB | **963 KB left** below the stack top |

The biggest pieces: packed art 182 KB, the portrait engine about 150 KB of code and tables, `gfx.c`'s display lists and OTs 138 KB of .bss, `doomo_town` 107 KB of .bss (needed, §8.4), room.c 43 KB, furniture data 38 KB. Map cell data for the shots that use a map comes on top (each packed map is small; `doomo_map` is 32 KB of .bss already counted).

| Main RAM (2,048 KB) | KB |
|---|---:|
| BIOS/kernel | 64 |
| Linked game code, data and .bss, 2D build / with 3D, overworld and room **(m)** | 801 / 1,021 |
| Show player (e) | 80-120 |
| Portrait pool growth, 18 → 90 KB | 72 |
| Two shot-chunk buffers (play one, prefetch the next) | 320 |
| Stack reserve (`elf2exe` gate) | 64 |
| **Total, 2D build / with 3D** | **~1,400-1,440 / ~1,620-1,660** |

That leaves **~600 KB (2D) and ~390 KB (3D) of headroom**, more than v1.0 estimated, and enough to keep everything in one executable. The game build, for comparison, sits at 72 KB below its stack top (m: a fresh build today). It is unaffected, because the show is a separate executable built from untouched sources.

**VRAM (the game's map is kept on the default canvas):**

| Region | Show use |
|---|---|
| x 0-319, y 0-479 | Framebuffers 0/1 (double-buffered, the game's) |
| x 320-959, y 0-255 (5 × 8bpp pages) | 3D/overworld/room sets: world textures as in the game. 2D sets: the **set pool** for plates and tile pages |
| x 960-1023 | Sky, game font, blob, plus the show font pages |
| x 320-1023, y 256-511 | Image cache: busts, figures, chibis, props, HUD; the crowd atlas carved from it |
| y 480-511 | CLUTs: master, silhouettes, a trimmed font bank, recolour CLUTs |

### 8.8 The wide hatch, `ps1-show-wide` (deferred to 2027; kept here as the design)

- **The frame.** A single-buffered 384x216 frame at VRAM (0,0). The lockstep stop captures finished frames, so tearing cannot appear.
- **Texture slots.** The 384-wide buffer covers the top 216 rows of world-texture page 0 (x 320-447). Page 0's 16 slots move into the area the second framebuffer frees (x 0-255, y 256-479: two 8bpp pages, 12 slots each above the CLUT rows).
- **Hunks:** `#ifndef` guards on `SCR_W`/`SCR_H` in `ps1.h`, a guarded slot table in `vram.c`, and a guarded single buffer in `gfx.c`. `room.c:2314`'s `c.vw <= 320` already sends a 384-wide view down the live path. The harness crop size comes from `g_show_mbox`.
- **Badge:** EMU-ONLY on every shot, because hardware cannot display it.

### 8.9 Mailbox and hashes (`g_show_mbox`, located by Lua from `show.ldmap`)

```c
typedef struct ShowMbox {
    uint32_t magic;            /* 'SHMB' */
    uint16_t abi, canvas_w, canvas_h, y_off;
    volatile int32_t req_shot, req_frame;      /* seek (Lua writes while paused; -1 = none) */
    int32_t  cur_shot, cur_frame;
    uint32_t p1_state_crc;     /* CRC32 of a fixed-layout record: shot, frame, beat_now, label ages, actor
                                  channels, camera, camfx read-backs, text cursor, crowd frozen mask, RNG */
    uint32_t p2_draw_crc;      /* CRC of depth-0 core->platform calls (name + args) via ld --wrap */
    uint16_t p4_n, p4_k[14];   /* audio event indices fired this frame */
    uint32_t cyc_update, cyc_draw, cyc_sync;   /* root counter */
    uint32_t prims, prim_bytes, uploads, evictions, pool_blocks_used;
} ShowMbox;
```

- **v1.1:** P1 and P4 ship in M0-M1. **P2 is deferred**: frame hashes plus P1 catch the regressions the schedule needs. `p2_draw_crc` stays in the struct as 0 so the ABI does not change later.
- **P2** wraps `plat_clear`, `plat_rect`, `plat_line`, `plat_text`, `plat_text_outline`, `plat_image`, `plat_image_xform`, `plat_checker`, `plat_shade_rect`, `plat_view3d`, `plat_overworld`, `plat_room`, `plat_post_*` and `plat_pal_lerp` with `-Wl,--wrap=…`, counting only depth-0 calls. Calls inside one file to a weak default (for example `plat_text_outline` inside `ui.c`) are not wrapped. That is a minor blind spot, and it is the same on every run.
- **Images** are identified by cache uid plus a content CRC logged once per uid.
- **P1, P2 and P4 are appended per frame to `plog.tsv`.** `show verify` compares them against the last approved render of the same shot and names the **first differing frame and channel**. This catches dropped primitives, image-cache thrash and VRAM collisions without a second renderer. P4 proves every audio event *k* fired on its scheduled frame.

### 8.10 Badges (computed per shot from the stats; they warn, never block)

| Badge | Means | Checked from |
|---|---|---|
| **PS1-LEGAL** | Retail RAM (link map + chunks + pools ≤ 2 MB with the stack gate); VRAM within the map; ≤ 56 KB display list per frame; 0 same-frame evictions; NTSC 320x240 display; built by `ps1-show` | `elf2exe`, planner, `prim_bytes`, `evictions` |
| **PS1-REALTIME** (deferred with the disc edition) | Also: every frame's update + draw + sync ≤ 2 vblanks in emulated time; each shot's prep fits the previous shot's slack | `cyc_*` stats (PCSX-Redux timing is not cycle-exact: hardware check before claiming it) |
| **EMU-ONLY** | Uses a hatch: `-8mb`, the wide canvas, or multi-pass | Manifest |

- **Masters may carry any badge.** The data path (disc, PCdrv or Lua inject) does not change pixels, so it does not affect the badge.
- `show deliver` prints a badge summary per version.
- Only the optional disc edition requires PS1-REALTIME.

### 8.11 Escape hatches

| Limit hit | Hatch | Cost |
|---|---|---|
| RAM in a heavy shot | `ps1-show-8mb` (PCSX-Redux dev-kit 8 MB; its strings show the option; tested in M0-T5). With ~390-600 KB of measured headroom (§8.7) it is not expected to be needed | EMU-ONLY |
| Iteration speed | Disc images rebuild in seconds (default). `ps1-show-dev` PCdrv is optional (only if M0-T5 shows it works); Lua inject as fallback | None to pixels |
| VRAM in one shot | Multi-pass: capture plates and characters separately and composite them on the host by palette index (exact, no blending) | EMU-ONLY; logged |
| Canvas | `ps1-show-wide` (deferred to 2027) | EMU-ONLY |
| Anything else | Re-stage the shot to fit; the full 320x240 frame is always valid | — |

---

## 9. Characters, assets, expressions and lip flap

### 9.1 Cast as data (the catalogue already has the species fixes)

```
# show/series/cast.shw (excerpt): every value is a real catalogue enum
char mayor   species=SPECIES_CHERUB wings=WINGS_ANGEL halo=HALO_HALO hat=HAT_TOP_HAT hair=HAIR_POMPADOUR
             facial_hair=FHAIR_HANDLEBAR accessory=ACC_BOWTIE kinds=bust,figure,chibi,model3d voice=mayor
             squash=plush:34 phase=39 age=adult
char nana    species=SPECIES_SHEEPKIN kinds=bust,figure,chibi voice=nana squash=plush:34 age=adult
char pip     look=show:pip kinds=bust,figure,chibi voice=pip age=child tags=KID       # hard limit 3
char mascotty look=show:mascotty kinds=figure,chibi idle=none tags=SILENT age=adult
char sunny   look=show:sunny kinds=figure,bust,chibi,model3d voice=none foley=sunny tags=SILENT age=adult
```

- **Every character and every crowd declares `age=adult|teen|child`** (lint `STD-AGE`: a missing age is an error). The chibi style makes age impossible to infer from a look, so it is always data. The KID/TEEN list starts from `SERIES_OVERVIEW.md` §2 hard limit 3 (Pip; the young cherub and young imp of the storybook; Miss Lulla's daycare kids; Bolt, Pokefork, Wisp, Tantrum, Wobbles, Homework; every KID- or TEEN-flagged citizen) and is re-read from the docs at each episode's start.

- The Mayor is a cherub and Nana a sheepkin, from the catalogue, not the trailer's wrong presets.
- `show art sheet mayor` renders a model sheet **on the PS1**: every kind × expression × viseme. Look-dev picks the exact hair style and paints there.

### 9.2 Image kinds and shot sizes

| Image | Size | `px=1` | `px=2` | `px=3` |
|---|---|---|---|---|
| Bust | 48x56 | Two-shot heads | **CU** (96x112) | ECU (144x168) |
| Figure | 48x80 | MS/MLS | MS full figure (96x160) | — |
| Chibi | 16x24 (4 dirs × 4-frame walk) | Wides, crowds | Kids at MS (co03) | — |
| Fighter | 32x48 (5 views) | Action | — | — |
| 3D chibi | `model.c` low-poly | Room, overworld, 3D sets | — | — |

**Expressions** are the engine's 11 `EXPR_*`. **States** are possessed (red sockets, ear twitch, glitch aura), ghost and dead. The `emissive` layer (Bonejangles' ribs) is drawn after the night tint.

### 9.3 Lip flap

- **Visemes.** There are 8 mouth shapes: `X` rest, `M` closed (M/B/P), `C` slightly open (consonants, teeth), `E` open (E, I, schwa), `A` wide (A, AI, æ, ʌ), `O` round, `U` pucker (U, W), `F` teeth on lip (F, V).
- **They map onto the engine's own mouth styles** in `series/visemes.shw`: rest = the look's own style, `M` = `MOUTH_FLAT`, `C` = `MOUTH_TEETH`, `E` = `MOUTH_GRIN`, `A` = `MOUTH_OPEN`/`MOUTH_BIG_GRIN`, `O` = `MOUTH_O`, `U` = `MOUTH_POUT`, `F` = `MOUTH_TEETH` + lip. These are tunable at look-dev. Beaked species keep `MOUTH_BEAK` and flap open and closed only.
- **The override.** Hunk (d) puts the chosen style straight into `mouth()`, and hunk (b) keys the cache by it. There are no render-diff mouth patches.
- **Mouth tracks are text: one character per frame.** `show timing` prints them, and Claude reads and edits them. A per-line override `mouth="XXMMEEEFF…"` is allowed.
- **TTS lines.** Kokoro's `speak()` returns `dur_pred` per token (one unit = 600 samples at 24 kHz = 25 ms = 0.75 frame).
  - Token start times become frame centres (`f × 800 + 400` at 24 kHz).
  - The mouth leads the sound by 1 f, every mouth holds for at least 2 f, and an `M` closure shows for at least 1 f on each bilabial.
  - Stress and length marks take the next phone's viseme. Word gaps close only when they are 3 f or longer.

  Measured (m), `am_puck`, "Stay.":

  ```
  stˈA.  ->  XXXXXXCCCXXXXAAAAAAAAXXXXXXXXXX   (31 f)
  ```

  The probe (`spec_bench/tts_probe2.py`) shows the format works. It also found the stress-mark rule above is needed (the probe wrote `X` mid-word).
- **Recorded takes** (optional in 2026; every line defaults to TTS).
  1. faster-whisper word times (`word_timestamps=True`).
  2. Inside each word, phonemes are spread by the TTS render's own duration ratios for the same text (cheap, good enough for a flap at 30 fps). **Deferred:** DTW of MFCCs against the TTS render (librosa).
  3. Fallback: the energy envelope mapped to `X`/`C`/`E`/`A`.
- **Chibis** flap `A`/`X` from the same track. Crowds never flap.
- **Acceptance (WP9):** on the `dx_proof` set, each `p`, `b` and `m` onset shows an `M` within ±1 f, and the creator approves the look on a CU at `px=2`.

### 9.4 New art the Claude way

1. **Scenery as data.** Strokes, prims and bands in set files are drawn live by the GPU and diff cleanly.
2. **Procedural plate generators** (`show/art/gen/*.py`). They write indexed PNGs (open matte, 240 tall at `px=1`). Claude writes and edits the generators, and their output is cache.
3. **Pixel-text sprites and fonts (`.pxt`).** One character per palette index, so Claude can edit pixels with Edit diffs.
4. **The creator's hand art** (Aseprite, LibreSprite, GIMP). `show art import file.png --to hand/…` quantises to the master palette, reports the worst colour error and stores the file in the vault (hash in git). Flipbooks get per-frame timing.
5. **Review in-engine.** `show art sheet <set|char>` renders on the PS1.

**The crayon register** is all palette-exact:
- a paper-grain tile at a fixed density;
- strokes 2 px wide with a gap pattern;
- boil of ±1 px on twos by `hash(stroke, frame/2)`;
- stroke reveal by arc length;
- the child's-hand font;
- the `CRAYON` palette lerp from `camfx`;
- chibis in crayon shots drawn through the lerp plus the boil jitter (judged at M1 look-dev).

---

## 10. Audio pipeline

### 10.1 One sample clock

- 48,000 Hz. Frame *f* starts at sample 1,600·*f*. A 22:00 episode is 63,360,000 samples.
- Music grids that do not land on frames are scheduled in samples, and picture events snap to the nearest frame from the bar line. 140 bpm is 12.857 f per beat. 96 bpm is 18.75 f per beat, but its bars are a whole 75 f.
- Fixed offsets are written in samples: `late=R8` is 12,800 smp (frame-locked, the default); `late=12816smp` is exactly 267 ms where a doc insists.

### 10.2 Dialogue: text → phonemes → take → lock

1. **G2P.**
   - Primary: the CMU Pronouncing Dictionary (loads in 0.54 s, 126,052 words (m)) + an ARPAbet→Kokoro-IPA map + `series/lexicon.shw` for names and coinages. **Licence note (v1.1):** the PyPI `cmudict` wrapper is **GPL-3.0-or-later** (m: its METADATA), while the dictionary file itself is CMU's BSD-style licence (m: `cmudict/data/LICENSE`). `g2p.py` reads the `cmudict.dict` data file directly with its own 10-line parser and vendors it with its licence under `show/lib/licences/`, so the show's tools never import GPL code. (Using GPL tools privately would not affect the episodes either way; this keeps the tools themselves freely licensable.)
   - Fallback: espeak-ng. **Caution (m):** the downloaded `espeakng_loader 0.2.4` wheel ignores its data path here (it looks for `phontab` on its build machine's path). WP9 either fixes the init path or skips espeak.
   - Inline phoneme overrides are allowed: `"I'm {fʌzz}…"`.
2. **TTS.** Kokoro-82M through `show/audio/voice/kokoro_voice.py` (`speak()` for speech, `sing()` with forced durations and an F0 curve for hums and sung lines). It renders at 24 kHz on 2 threads and is resampled to 48 kHz with `soxr`. Measured here: model load plus the first line 2.6 s, RTF 0.64 over five lines (m).
3. **WORLD** (pyworld) shaping: pitch, formant, a flattened F0 for possessed and deadpan voices, a doubled voice.
4. **QA with faster-whisper small (int8, 2 threads).** Load 0.8 s, about 3-4 s per short line (m). It read back "Perfect.", "Stay.", "Two of everything dear." and "Look fireworks, come on, come on." exactly. It heard "...Hee. Hee." as "Hey, hey", so that line is `asr=skip` (it is a sung library take anyway).
5. **Lock.** `voice.lock` gets the source, the take hash, the 30 ms pre-roll, onset, length in samples, word and phoneme times and the mouth track. **Approved TTS takes are stored in the vault (not LFS, v1.1) and locked by hash**, because ONNX output is not guaranteed identical across machines. Measured on this container: the same line rendered three times in one process and again in a new process gave identical samples (sha `1a8e91f0…`, 2 threads), so re-rendering is safe here; the lock matters after a container or library change.
6. **Recorded takes** (`show voice ingest take.wav --line co15_perfect`): high-pass, light denoise, trim, level the dialogue to −23 LUFS (pyloudnorm), then align (§9.3). `retime=fit` time-stretches with WORLD only within ±15 %. Beyond that the lint refuses and suggests `retime=take`.

**Special rules as data:**
- The Mayor's "...Hee. Hee." is `lib:` + `lock=identical`: the same samples every time, at most 3 per episode (`AUD-HEEHEE`).
- Pip's "CLINKER!" is `fx=dry solo=true` under `mx mute all`.
- Clinker is `sync=<bonejangles line> late=R8`, and "...Seven." is `late=0`.
- The pet's "beep." is three raw piezo files, `process=none` enforced.
- The {d} deadpan lines duck the score.

**Provenance.**
- Every lock row has `src=tts:kokoro@<model sha>:<voice params>` or `src=rec:<file sha>`.
- `ai_usage.jsonl` is appended.
- Preview burn-ins say `TEMP TTS: n lines`.
- `show deliver --require human-voice` lists every TTS line still in a version (REQ-AUD-3).

### 10.3 Voice casting (temp TTS, auditioned with `show voice audition <char>`)

**2026 default: every role is TTS** unless the creator records a take (roadmap §8); the "Final voice" column is a recommendation for later. The Kokoro picks are **starting points**. `show voice audition mayor "…Hee. Hee. Perfect."` renders the line in the listed voice and five neighbours (blends ± pitch), with a Whisper read-back, and the creator picks by ear. The pilot's 12 speaking roles come from beat sheet Appendix E. All voices are adults (bible).

| Role (pronoun) | Brief from the docs | Temp TTS: Kokoro voice / blend, speed | Shaping and rules | Final voice (recommendation) |
|---|---|---|---|---|
| Mayor Giggs (he) | Musical-theatre tenor | `am_puck` 0.6 + `am_michael` 0.4, 1.00 | WORLD +2 st. "...Hee. Hee." is a **sung library take** (`singer.py`: two falling notes a major third apart, locked) | Creator's voice if a bright tenor is comfortable; otherwise TTS |
| Nana Puddin (she) | Low, slow, warm alto | `bf_emma`, 0.88 | −2 st | TTS or creator |
| Pip (they; a child, voiced by an adult) | Highest and fastest | `af_sky`, 1.30 (the 1.3x cast tempo) | +3 st; "CLINKER!" dry, solo, unprocessed | **Creator's voice** recommended (the crying beat is a performance) |
| Father Fuzz (he) | High, quick, sincere | `am_liam`, 1.10 | +2 st | TTS or creator |
| Bonejangles (he) | Low, expressive | `bm_george`, 0.95 | −2 st; possessed couplets with `flatten_f0=0.7` and Clinker under him | TTS or creator |
| Clinker (she; voice only) | Low, slow, flat | `af_nicole`, 0.85 | −3 st, `flatten_f0=1.0`, `late=R8`; "...Seven." on time | TTS |
| Dr. Bloom (she) | High, soft | `af_aoede`, 0.95 | +1 st | TTS |
| Steamy (she) | A fictional lilt, never a caricature | `bf_lily`, 1.00 | A defined intonation contour via WORLD. **Never a non-English Kokoro voice used to fake an accent** | Creator's voice, or a performer's own lilt (the docs' rule) |
| Kevin (he) | Deadpan | `am_onyx`, 0.92 | `flatten_f0=0.5` | TTS |
| Hattie Haha (she) | Slow and precise | `bf_isabella`, 0.85 | Her second laugh lands R8 late | TTS |
| Miss Lulla (she) | Low, soothing | `af_heart`, 0.90 | −2 st; −6 dB diegetic filtering inside her tent | TTS |
| GURGLE (it) | Sound design | none | `VOICE_GURGLE` (game) + bass babble + tuba + draining sink | The same performer as Grumbleguts |
| Grumbleguts (he; a purr only in 101, speaks from 102) | Flamboyant, raunchy | `am_fenrir` 0.7 + `am_santa` 0.3, 1.05 | −2 st | **Creator's voice** recommended (a lead performance) |
| Walla, adults (4-6) | Crowd | `am_eric`, `af_river`, `am_adam`, `af_kore`, `bm_daniel`, `bf_alice` | ±2 st by seed, −18 to −28 dB, panned | TTS |
| Storybook kids' walla (co02-co03) | Children, voiced by adults | `af_sky`, `af_river`, `am_puck`, `bf_lily` | +4..+6 st, 1.10-1.20 | TTS |
| Fog voice "come and PLAY with us" | Walla stacked with Clinker; the second "us" a child's voice, never Pip's actor | Walla stack + `af_nicole`; "us" = `af_kore` +6 st (not `af_sky`, which is Pip) | `{w}` wave | TTS |
| AD narrator | Neutral, clear | `bm_fable` | AD bus, ducks the programme | **Creator's voice** (default) |
| The pet (it) | "beep." | none | Raw piezo from the engine (`runner --wav`) in 3 readings, `process=none` | — |
| Sunny (they), Mascotty (she) | Silent | — | Foley only | — |

### 10.4 Music

- **Game cues** (for example `mml_mingle.c`). `music/mml_legacy.py` **imports `tools/mmlc.py`** (`read_songs()` and `simulate()`, which already mirror the game's 30 Hz sequencer tick for tick, MU1T) to read the legacy MML (`t96 @12 o5 l8 v12 c f a f >c4< …`, `[…]N` loops, `H` shaker), and converts the result into a kit `Score`. It writes no parser of its own. It is voiced at 48 kHz by kit presets (`musicbox`, `pad_soft`, `tri`, `shaker`), so it stays the game's tune. `runner --wav` (22,050 Hz, 735 smp per frame) remains the sync reference.
- **The theme** ("Doomtastic Times"). The masters are in `theme/final/` (main title 50.35 s, TV size, eyecatch, stems). `show music import-barmap` reads the bar map (150 bpm = 12 f per beat, 48 f per bar), and the main title is boarded with `len=bar:N` and `@bar:` times only. The CRACK is the main title's frame 0, which is bar 1, beat 1.
- **Score cues** (HOOK and RIFF at 140 bpm, motifs, about 70 stingers) are kit scores in `show/lib/music/*.py`: deterministic float64, seeded per note, with stems.
- **Automation in the score:**
  - the possession bend (C→B♭ over 13 f) is pitch automation;
  - MINGLE's 29 f mid-phrase mute is bus-gain automation over a continuous render, so the music re-enters on the grid as the game's sequencer does;
  - the Mayor's hum takes the muted note's exact pitch through `hum … note=mingle_box.mayors_note`, sung by Kokoro `sing()` on that F0.

### 10.5 SFX, foley, walla

- **Library.** `DGL_<cat>_<name>_<var>` at 48 kHz/24-bit: the game's PCM effects re-synthesised at 48 kHz by importing `sfxgen.py`'s generators, the synth one-shots from `audio_fx.c`'s `SynthSeg` tables (§4.2), kit-synthesised foley (plush squash, bone clacks, roller squeaks in three falling pitches, padlock rattles) and the creator's recordings (LFS, with licence files).
- **Data-driven foley.**
  - `FOL_crayon_scratch follow=<stroke>` shapes filtered noise by the stroke's reveal speed, frame by frame.
  - `FOL_paw_patter from=crowd:…` places one paw tick per walk-cycle contact of each citizen (computed by showc from the crowd data), panned by x.
- **Rules as lints:** one sound per visual beat, a sound on frame 0 of every cut, an insert owns its audio, ducks of 0.4-0.75 for 5-20 f.

### 10.6 Mix and master

- **Buses** (the kit's `Mixer`: sends, sidechain, automation from `audio_events.json`):
  - DX, with pan from the actor's x;
  - MX;
  - FX (with ambience);
  - AD.
- **Outputs (2026):** 48 kHz/24-bit stereo mix and the DX/MX/FX stems (FLAC, kept until the episode is released, then moved to the vault, §12.2). **Deferred:** the AD stem and mix, a filled M&E, and 5.1 (DX → C; MX → L/R with a surround spread; FX by `pan=`; ambience → Ls/Rs; the low-passed sum → LFE; a Lo/Ro downmix check).
- **Loudness** (pyloudnorm; true peak at 4x oversampling). One mix, re-mastered per target by gain and limiter only:

  | Target | Integrated | True peak | Use |
  |---|---|---|---|
  | `web` | **−16 LUFS** (window −14 to −16) | −1 dBTP | YouTube/Vimeo, every 2026 preview and release |
  | `r128` (deferred) | −23 LUFS | −1 dBTP | EBU |
  | `a85` (deferred) | −24 LKFS ±2 | −2 dBTP | ATSC |
  | `dlg27` (deferred) | −27 LKFS dialogue-gated | −2 dBTP | Streamers that ask for it |

- **QA** (`kit.qa`): clicks, clashes, masking, tuning, plus the silence budget per act (20-25 % unscored; the cold open allows 26 %).

### 10.7 What must be preserved first (the vault)

The audio and voice kit, the TTS models and the emulator **live only in this session's scratchpad today**. `/opt/ps1emu/pcsx-redux` is a symlink into `scratchpad/redux/` (m).

| Item | Size here (m) | Where it goes |
|---|---|---|
| `theme/kit/kit/` (1.9 MB), `theme/final/src/` (0.6 MB) | 2.5 MB | **git**, as `show/audio/` (text) |
| PCSX-Redux binary (453 MB debug; **21.5 MB stripped, 4.8 MB xz**) + `/usr/local/lib/libSDL3.so.0.2.10` (3.5 MB, its only non-system library) + `openbios.bin` (512 KB) + GPL licence | ~9 MB xz | Vault release `vault-emu` |
| PCSX-Redux source at `c45db3b` (`src` 32 MB; `third_party` 787 MB) | ~32 MB (+ third_party optional) | Vault release `vault-emu-src` (rebuildable) |
| PSn00bSDK (`/opt/psn00bsdk`, 17 MB) + the `gcc-mipsel-linux-gnu` files (`dpkg -L`) | ~60 MB (e) | Vault release `vault-sdk` |
| Kokoro fp32 ONNX + 54 voices (28 MB) + patched analysis/sing models (621 MB, or regenerate with `kokoro_patch.py`) + Whisper small int8 (244 MB) + the espeak and cmudict wheels | ~0.6-1.2 GB | Vault release `vault-models` |
| Kit assets (168 MB, CC0/CC-BY, npm-sourced), theme masters and stems (~370 MB) | ~0.5 GB | Vault `vault-audio` (route per the list below) |

- **The vault, v1.1.** v1.0 said "release assets, created with the preconfigured `gh`". **Measured today: `gh` is not logged in in this container** (`gh auth status`: "The token in GH_TOKEN is invalid"), and the GitHub connector available to the session can create a repository and push files but has no release-asset upload. So WP0 tries, in order:
  1. a private `doomo-vault` repo (created through the GitHub connector, or by the creator in one click) holding the archives as ordinary git files, split with `split -b 95M` to stay under GitHub's 100 MB per-file limit, pushed over the session's git credentials;
  2. if the session's git credentials do not cover a new repo: an orphan branch `vault` in this repo, pushed with the existing credentials and never merged. Clones made with `--single-branch` never download it, but a full clone of the game repo would, so this is the fallback, not the default;
  3. release assets, once the creator runs `gh auth login` with a token (the 2 GB-per-file path v1.0 described).
  The emulator, BIOS and SDK (about 70 MB) go first, the models and kit second.
- **Pins.** `show/vault.lock` pins every hash: the emulator `56837ef4…` (original) plus the stripped hash, and OpenBIOS `175836cc…`.
- **Restore.** `show vault fetch` on a fresh container, run by hand or by the **opt-in** hook (§4.1), which fetches only the emulator and BIOS; models come on first use. This is **WP0, day 1**.
- **Episode archives.** After each release, that episode's FFV1 masters and FLAC stems go to the vault (one archive per episode) and leave the disk (§12.2).

### 10.8 Licences of everything that reaches a frame or a sample

Checked against the files in this container (m):

| Source | Licence | Obligation for a YouTube release |
|---|---|---|
| The game's art, palette, fonts, cue melodies (`mml_*.c`) and procedural SFX (`sfxgen.py`, `audio_fx.c`) | The project's own | None |
| The theme ("Doomtastic Times") and every score cue, rendered by the kit | The project's own | None |
| Kit samples: Sonic Pi set (CC0), VSCO-2 CE (CC0), TR-808 set (free, ISC wrapper), Stomachache and Karman Lyne samples (CC0) | As listed in `theme/kit/KIT.md` §1 and each `assets/*/LICENSE` | None (VSCO-2 credit optional) |
| **Salamander Grand Piano V3** (Alexander Holm) | **CC BY 3.0** | **Credit** in the description and end credits whenever the piano is in the mix |
| **DWSD "Deep House Drum Kit"** (freesound, via `@teropa/drumkit`) | **CC BY** | **Credit** "DWSD / freesound.org" whenever those samples are in the mix |
| GeneralUser GS 1.471; MuseScore General Lite and FluidR3 Mono (SF3) | GeneralUser GS License v2.0 (music production incl. commercial); MIT | None for audio (keep the licence files with the kit) |
| TimGM6mb (inside `pretty_midi`) | GPL-2.0 | **Never in a mix** (comparison only); lint `AUD-LIC` refuses it |
| Kokoro-82M weights and voices | Apache-2.0 | None; the show credits "Voices: Kokoro TTS" anyway (YouTube honesty, roadmap §7) |
| faster-whisper / Whisper small (QA only, never in the audio) | MIT | None |
| CMU Pronouncing Dictionary data (G2P only) | BSD-style | Keep the notice with the vendored file |
| PCSX-Redux, OpenBIOS, PSn00bSDK, ffmpeg/x264, pyworld, soxr, pyloudnorm | GPL / MPL-2.0 / LGPL-GPL / MIT / LGPL / MIT | Tools only: no terms on the rendered episodes |

`AUD-LIC` (new lint): every sample source that a mix uses must resolve to a row of `show/lib/licences/licences.toml`; the release package (§13.7) prints the credit lines the mix log requires. **Content ID:** CC0 and royalty-free samples are sometimes claimed by mistake on YouTube; the provenance log is the dispute evidence.

---

## 11. Capture and render

### 11.1 Harness (`show/tools/showtk/emu/`)

- **A fork of `ps1/harness/run.py` and `harness.lua`**, never an edit.
- **Settings.** `PCSX.settings.spu.Speed = 0` (unthrottled; boot 1.07-1.1 s (m)). **Never set `UseNullSync`** (m: it hung).
- **Lua crop.** At each lockstep stop, Lua `ffi.copy`s 240 rows × 640 B from `getVRAM()` at the mailbox's `shot_y` into one buffer and writes it to a FIFO with `File:write(ptr, n)`. Measured: 600 frames in 4.04 s including boot (4.9 ms per frame), with byte-identical frames across 4 runs (m).
- **Mailbox log.** Lua also reads `g_show_mbox` and appends the P-log line.
- **`capture.py`** (overlapped, on another CPU):
  1. expands BGR555 to RGB24 with the palette's defined `(v<<3)|(v>>2)` (numpy, 0.55 ms (m));
  2. maps the LUT and counts off-palette pixels (0.29 ms (m));
  3. takes a SHA-256 of the raw frame;
  4. pipes RGB24 to ffmpeg FFV1 (`-pix_fmt bgr0 -level 3 -g 1`, intra-only).

  Measured: 300 f in 0.54 s from a raw stream, 4.8 MB per 300 f (m). Every frame ffmpeg ever sees is exact palette RGB.
- **Seek.** `req_shot`/`req_frame` binds the chunk and pre-rolls from the shot start with drawing skipped, so a single frame takes boot plus under 1 s (e).
- **Checks.** Hang detection, TTY crash patterns and `expect` assertions are inherited. A crash names the shot and frame.

### 11.2 Render cache and scheduler

- **Shot key:** SHA-256 of (shot chunk, the chunks of `base`/`replay` sources, `show.exe`, emulator, BIOS, harness version, seed).
- **Cache entry:** `build/cache/shots/<key>/{master.mkv, frames.sha256, plog.tsv, perf.json, badge.json}`.
- **Scheduling.** `show render ep101 --jobs N` gives each emulator a contiguous run of dirty shots, so one boot serves many. Measured scaling: 3 instances give **1.6x** throughput, not 3x (m). The default is `--jobs 2`, with one CPU for capture and encoding and one for the conversation and tools.
- **A one-beat edit** recompiles in under 1 s, re-renders only the changed keys (seconds), re-mixes the affected window and re-encodes only the dirty segments (§12.2).

### 11.3 Determinism

- **Measured:** 150/150 frames identical across runs, throttled = unthrottled (120/120), and 4 parallel and solo runs identical (m).
- **Inputs:** the show build reads no wall clock, and seeds come from the episode and shot.
- **Checks:**
  - `show verify ep101 --sample 20` re-renders 20 random shots and compares the frame hashes and P-logs.
  - Encodes are checked with `ffmpeg -f framemd5` on decoded output (300 f in 0.38 s (m)), never by file bytes.
- **Audio** is float64 with seeded noise, with the mix hash in the manifest.
- **Python dependencies** are pinned with hashes in `requirements.lock`.

---

## 12. Assembly and deliverables

### 12.1 ffmpeg recipes (input: the per-shot FFV1 masters, concatenated by the deliver EDL)

```sh
# preview: x3, H.264 veryfast, burn-ins (TC, beat, shot, frame, line id, TEMP TTS, badge), temp mix    156 fps (m)
ffmpeg -f concat -i edl.txt -i mix_web.wav -vf "crop=320:180:0:30,scale=960:540:flags=neighbor,drawtext=…" \
  -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p -c:a aac -b:a 192k preview.mp4

# web HD master (x6): H.264 CRF 16, tune animation, closed GOP per segment                           33-48 fps (m)
# (v1.1 re-measure, 3 threads: veryfast 45 fps, faster 49 fps, medium 39 fps on the same 300 f; the
#  nearest-neighbour x6 scale and RGB->YUV conversion dominate, so the preset barely matters: ~15-20 min/episode)
… -vf "crop=320:180:0:30,scale=1920:1080:flags=neighbor,format=yuv420p" -colorspace bt709 -color_primaries bt709 \
  -color_trc bt709 -c:v libx264 -preset medium -crf 16 -tune animation -x264-params keyint=60:scenecut=0 \
  -c:a aac -b:a 320k -movflags +faststart ep101_web_hd.mp4

# web UHD (x12), optional overnight in 2026                                                           10 fps (m)
… scale=3840:2160:flags=neighbor …

# deferred: HD ProRes 422 HQ per shot (intra-only), prores_aw; joined with concat -c copy             87 fps (m)
… -vf "crop=320:180:0:30,scale=1920:1080:flags=neighbor,format=yuv422p10le" -c:v prores_aw -profile:v 3 seg_coNN.mov
# UHD 4444: prores_aw 19-21 fps (m). prores_ks (2.5 fps UHD (m)) only on a buyer's request.

# 29.97 conform: -r 30000/1001 on input; audio resampled by 1001/1000 with soxr beforehand
```

### 12.2 Segments and storage

- **Segments.** Encodes are made per shot (ProRes) or per beat (H.264, closed GOP) and joined with the concat demuxer by stream copy, so a one-shot change re-encodes one segment.
- **Measured sizes:**
  - FFV1 master: 4.8 MB per 300 f, about 0.63 GB per episode;
  - H.264 HD: 0.58 MB per 300 f, about 77 MB per episode;
  - H.264 UHD: about 110 MB per episode;
  - ProRes 422 HQ HD: 89.5 MB per 300 f, about **11.8 GB per episode** (m).
- **Audio sizes (arithmetic):** a 22:00 stereo 48 kHz/24-bit WAV is about 380 MB, so the mix plus three stems is about 1.5 GB as WAV and about 0.8 GB as FLAC.
- **Disk (v1.1).** Free space was 6.8 GB earlier this month, 5.6 GB when v1.0 was written, **4.9 GB** at the low point of this review and 7.7 GB an hour later (m): other sessions share this disk, so plan for the low point. One episode's working set (FFV1 masters 0.63 GB, FLAC stems about 0.8 GB, temp WAVs during the mix, caches, the HD MP4) is about 2-2.5 GB, so **one episode is in progress at a time**: after its release, its masters and stems go to the vault and leave the disk; only the MP4 (about 80 MB) and captions stay. `show doctor` warns below 2 GB. ProRes is never stored locally.
- **The archive of record** is the FFV1 masters plus the manifest, in the vault. PNGs are made on demand for sheets only, and the cache is purged least-recently-used.

### 12.3 Versions from data

`show deliver ep101 <version>` composes the EDL from `episode.shw`, renders any shots the version needs (textless shots are separate cache keys), mixes, encodes and runs QC.

| Version | How it is made | 2026? |
|---|---|---|
| **Web (default)** | 30p, `web` loudness, chapters (eyecatches), the TV-MA card at the head, no burn-ins | **Yes: the only version** |
| Broadcast | 29.97 conform, 40 f trimmed from elastic beats, breaks, `a85`/`r128` | Deferred |
| 2x11, 21:00 | Beat sheet Appendix G orders and trims | Deferred |
| Textless | `text=off` | Deferred |
| 4:3 (bonus) | Full 320x240, x9 in UHD | Deferred |
| 9:16 Shorts | 135x240 crop with per-shot `v916` keys | Deferred (first back, for promotion) |
| Disc edition (optional) | XA soundtrack plus realtime mode; PS1-REALTIME shots only | Deferred |

**Runtime (v1.1).** The docs time every episode to 22:00 for a broadcast slot. For the web release, `FMT-RUNTIME` accepts **20:00-23:00** and the beat sums are a warning, not an error, so no time goes into elastic trims.

**Captions:** SRT and WebVTT (M1), which YouTube accepts. TTML and SCC are deferred. Text comes from `lines.shw`, the `cc` events and the SFX captions. Captions move to the top when a SUPER or sign sits in the bottom third.

**Paperwork (2026):** the standards report and signed checklist (§13.7), the provenance/AI log and the credits/licence lines (§10.8). The dialogue list, music cue sheet, AD script and QC report are deferred with the broadcast version.

---

## 13. The review loop

### 13.1 What Claude reads

| Command | Output | Why it works for Claude |
|---|---|---|
| `show sheet <shot\|range>` | A PNG grid of every event frame plus every Nth frame at 2x, each tile captioned with shot, frame, TC and event | Timing bugs live on event frames |
| `show strip <shot>` (deferred) | A timing strip: one column per frame, thumbnails at events, the audio waveform with onsets, bar lines and the mouth track | Picture against sound in one image |
| `show standards <ep>` | The standards report (§13.7): every scene with a child, every tagged moment, every advance with its yes/no line, as text plus contact sheets | What the creator ticks |
| `show timing <shot>` | Text: each event's frame, the measured picture change (frame-diff spike), the audio onset, the R-rung of each gap, and the mouth track string | Exact, diffable, greppable |
| `show frame <shot> +N --overlay` | PNG x3 with safe areas, the Wall line, actor anchors and off-palette pixels in magenta | "Why is this pixel wrong" |
| `show lint` | `file:line: shot +frame [RULE] problem; fix` | Fixable without asking |
| `show verify <shot>` | First frame and channel where the frame hashes, P1 or P4 differ from the approved render (P2 deferred) | Logic regressions without a second renderer |
| `show asr <beat>` | The Whisper transcript of the DX stem next to the script | Intelligibility |
| `show plan`, `show perf` | Per-shot RAM/VRAM/prim budgets and badges | Limits |
| `show status ep101` | Shots by state (slug, blocked, voiced, final), open lints, cache hits | Where the episode stands |

### 13.2 What the creator watches

`show preview <shot|beat|act>` gives an MP4 with burn-ins (TC, beat, shot, frame, line id, `TEMP TTS`, badge) and the temp mix. `show diff <shot> --against <ref>` gives a before/after sheet and the lint delta (the heat map is deferred). In 2026 the creator watches **an act or an episode at a time**, not shots (roadmap §4-§5), so previews are made per act.

### 13.3 A note becomes a fix

1. **Creator:** "In co02 the SUPER types too slowly, and the bandstand should be dead centre when the iris opens."
2. **Claude** opens `ep101/shots/co.shw`, runs `show timing co02` and `show sheet co02`, and sees the bandstand at x ≈ 166 at +13: the park plane is still 77 f × 0.0833 px/f off centre, because the composition lock is set for +90.
3. **Claude** explains the trade-off. The shot list types the SUPER over +30..+132, and the lock at +90 is what makes shot 9 work. It proposes two options: `zero=+13` with `co09 base=co02@+13`, or keep the shot list's timing. The creator picks.
4. **Claude** edits one line, then runs `show render co02 co09`, `show diff co02`, `show lint co02 co09` and `show preview co01..co03`, and returns the MP4 path and the sheet.
5. **Claude** logs the note in `ep101/notes/round-02.md` and commits: `ep101 co02: bandstand centred at the iris (+13); co09 base moved to match (note 2.1)`.

### 13.4 Error format

```
show/ep101/shots/co.shw:88: co15 +153 [AUD-OVERRUN] line co15_perfect (36 f) ends at +189, after the shot's
  tail hold starts (+180, `button`). Move it to +141, use a shorter take, or set retime="take" in takes.toml.
```

### 13.5 Lint catalogue (each traces to a requirement)

| Id | Checks |
|---|---|
| `FMT-RUNTIME` | Beat = the sum of its shots; episode within the web window 20:00-23:00 (v1.1; exactly 39,600 f only for the deferred broadcast version); protected holds untouched |
| `TIME-RUNG`, `TIME-INSERT`, `TIME-FLASH`, `TIME-IRIS`, `TIME-WIPE`, `TIME-SPLIT`, `TIME-READ`, `TIME-RIPPLE` | Ladder gaps; inserts ≥ 4 f owning their audio, with one persistent change after; flashes ≤ 3 f and ≤ 3/s with luminance ramps ≥ 8 f; wipe 5 × 2 f; Split ≥ 60 f at x = 160; reading time ≥ 9 f/word + 15; ripples 0-3 f |
| `COMP-LOCK`, `COMP-ONECHANGE` | `assert`s; `base=` with exactly the declared changes (shots 2 = 9, 16 = 18, 7 → 17; the banner baseline across 11/12) |
| `AUD-FRAME0`, `AUD-SILENCE`, `AUD-LATE`, `AUD-HEEHEE`, `AUD-ASR`, `AUD-OVERRUN`, `AUD-P4`, `AUD-LIC` | A sound on frame 0 of each cut; 20-25 % unscored per act; 267 ms rules sample-exact; identical take ≤ 3 per episode; transcripts; takes fit slots; every event *k* fired on its frame; every sample source in the mix has a licence row and its credit line is in the release package (§10.8) |
| `PIC-PAL`, `PIC-SAFE`, `PIC-SNAP`, `CAM-SCALE` | Off-palette pixels by class; text in title-safe; positions on the `px` grid; logged scale spans |
| **`STD-EXPLICIT`** (hard limit 1) | **Error.** No shot, line, string, look or asset may carry the tag `explicit`; sexual content may only be tagged `implied`, `offscreen` or `innuendo`; an art import flagged `anatomy` is refused; Grumbleguts appears only in looks marked `modest=yes` (his front smooth, nothing drawn, per the docs); `STD-STICKER`: the censor-sticker gag at most once a season (it is in 101) |
| **`STD-CONSENT`** (hard limit 2) | **Error.** Every `advance` tag names `to=<char>`, and both characters are `age=adult`. Any `touch` or `contact` tag between them needs a prior line or scripted beat tagged `yes` **from the target, in the same scene**, referenced by `consent=<line id>` (a pause, a look or silence can never be the referenced yes). After a line tagged `no` from the target, the advancer's next line must carry `stepback`, and no further `advance`, `innuendo` or `contact` tag aimed at that target may follow in the scene. `consent_gag` is a forbidden tag (consent is never the punchline); the human check covers the rest |
| **`STD-AGE`**, **`STD-KIDS`** (hard limit 3) | **Error.** `STD-AGE`: every cast entry, crowd and voice declares `age=adult\|teen\|child`. `STD-KIDS` works **per scene** (`scene=`, one continuous time and place, §5.5), not per shot: if any KID/TEEN actor, crowd member, walla voice or `ref=` in the scene is a child, then no shot or line in the scene may carry `innuendo`, `suggestive`, `implied`, `advance`, `contact`, `rear` or `gore_truth`, no `gore=BRUTAL` shot or truth insert may sit in it, the gore filter is forced on, and no suggestive line anywhere may `ref=` a child. Off-screen children count when they are voiced in the scene |
| **`STD-HATE`** (hard limit 4) | **Error** on a match in any line, string, sign, caption or title against `series/lints.shw`'s slur list; **warning** on a match against its list of real people, groups and faiths (a joke may mention them; it may not target them, which the human check decides) |
| `STD-TAGSCAN` | **Warning.** An untagged line, SUPER or sign that matches the innuendo/sexual word list, so a missed tag is caught before review |
| `STD-FPS`, `STD-LOCK`, `STD-FLASH`, `STD-*` | First person ≤ 75 s per episode; the Lock List of what Hellvile may show and when; flashes per `TIME-FLASH` (and `show_cast.c` pins the game's REDUCE_FLASH/SCREEN_SHAKE options so camfx's own caps match the lint); a cute citizen never gored; Sunny never shown hurt; the pet never harmed; no real brands; no third-party reference art |
| `CANON-*` | Species, pronouns in captions and AD, no Hellvile citizens before 107 (waivable with a reason) |
| `PLAN-RAM`, `PLAN-VRAM`, `PLAN-PRIM` | The budgets behind the badges (warnings) |
| `SHW-WAITA`, `SHW-OPS`, `SHW-PROV` | No button waits; no engine-native `flag`/`sfx`/`music*`/`tapestop` ops (§5.5); a provisional time left in a final |

**Error vs warning.** Every `STD-*` error blocks `show deliver --release`. There is no `waive` for hard limits 1-4 (the `waive` statement refuses `STD-EXPLICIT`, `STD-CONSENT`, `STD-AGE`, `STD-KIDS` and `STD-HATE`). The lints test tags, so their strength depends on tagging: Claude tags every line and shot as it writes them, `STD-TAGSCAN` catches what was missed, and the creator's checklist (§13.7) is the final, human check. Each rule has a failing fixture in `show/tests/standards/` (a child in a scene with an innuendo line; an advance with no yes; a `no` followed by another advance; an untagged suggestive line; a slur), and M1-T9 requires every fixture to fail.

### 13.6 Later, optional: GP0 attribution

- At the lockstep stop, Lua copies the frame's primitive buffers (2 × `PRIM_BYTES` = 112 KB) out of PS1 RAM.
- `gp0.py` decodes the OT chain into primitives (type, tpage, CLUT, vertices).
- `show frame --why X,Y` names the last primitive covering a pixel.

No GPU re-implementation is involved. A software PS1 GPU is a deferred option only if a 9:16 re-render or gdb on the renderer ever becomes necessary. If it does, it is built by porting the local PCSX-Redux `src/gpu/soft` (GPL, private tool), never as a master renderer.

### 13.7 The review checklist: standards and release (every episode)

The hard limits (`SERIES_OVERVIEW.md` §2) are checked twice: by the lints in §13.5, which block delivery, and by the creator, because no lint can judge whether a joke is suggestive or whether consent is the punchline. `show standards ep10N` builds the report; it lists only what needs a human eye, so the check takes about 5 minutes.

**The standards report** (`build/ep10N/standards.html` plus a text copy Claude reads):
1. **Every scene with a child, teen or KID-tagged voice** (hard limit 3): one contact-sheet row per scene, its shots and lines, and the lint result ("clean: no innuendo, suggestive, advance, contact, rear or truth-insert tags; filter on").
2. **Every tagged sexual or suggestive moment** (hard limits 1 and 2): the frame ranges tagged `implied`, `offscreen` or `innuendo`, as contact sheets at 2x, so the creator sees that nothing explicit is drawn.
3. **Every advance** (hard limit 2): a two-column list of the advance line and the target's `yes` or `no` line, with the `stepback` line after each `no`.
4. **Jokes about groups, beliefs or real people** (hard limit 4): every `STD-HATE` warning, and every line tagged `satire`.
5. **The gore register**: every truth insert and BRUTAL shot with its frame range (and the proof that no child shares its scene), the first-person total against 75 s, and the flash report.
6. **Untagged matches** (`STD-TAGSCAN` warnings) with Claude's resolution for each.

**The creator's checklist** (`ep10N/standards_signoff.toml`, written by Claude from the creator's one-line reply and bound to the render's manifest hash, so any later change re-opens it):

| # | Check | Hard limit |
|---|---|---|
| 1 | Nothing explicit is visible anywhere; sex is only implied, off-screen or innuendo | 1 |
| 2 | Every sexual or suggestive moment is between adults, and each one asked and got a yes first | 2 |
| 3 | Every no ended the advance on the spot; no silence was treated as a yes; consent is never the joke | 2 |
| 4 | No child or teen is in a scene with innuendo, a suggestive line, or graphic gore; no suggestive line is about a child | 3 |
| 5 | No joke targets a real person, group or faith; no slurs or real hate symbols | 4 |
| 6 | Gore rules: the filter is on with children; gore lands on demons and monsters only; Sunny is never shown hurt; the pet is never harmed | docs §2 |
| 7 | Release: the TV-MA card is at the head; the title, thumbnail and description are plainly adult (not child-appealing); the YouTube settings are set to "not made for kids" and 18+ | release |
| 8 | Credits: the CC BY credit lines from the mix log are in the description and end credits; voices are credited as synthetic where they are | release |

`show deliver ep10N web --release` refuses to build the upload package unless every lint in §13.5 is green and the sign-off matches the current manifest.

**The release package** (`release.py`, WP17): the HD MP4, `.srt` and `.vtt`, a title, a description with the credits and the voices line, chapters from the eyecatches, tags, a thumbnail rendered on the engine, and an upload sheet that lists the YouTube settings above. The container cannot reach the creator's YouTube account, so the creator uploads it (about 10 minutes); the package is handed over through the session or the vault.

---

## 14. Commands

The creator talks, and Claude runs these. Every command is headless, exits non-zero on failure, and prints `file:line: shot +frame [RULE]`.

| Command | Does |
|---|---|
| `show doctor` | Checks the vault, tool hashes, disk space, emulator boot and Python dependencies |
| `show vault pack\|push\|fetch\|verify` | Builds, uploads, restores and checks the vault (§10.7) |
| `show check-game [--full]` | The game-unchanged gate (§4.4): the path check every commit, the full hash build with `--full` |
| `show build [--preset ps1-show\|ps1-show-8mb]` | Builds `show.exe` and the disc layout from the stand-alone project (`ps1-show-dev`, `ps1-show-wide` and `--native` are deferred) |
| `show new-episode ep102 --shape A\|B\|C [--from ep101]` | An episode skeleton from a template |
| `show compile ep101 [--shots a..b] [--voice]` | §7 |
| `show slug ep101` | The timed slug animatic from the rundown, rendered by the PS1 |
| `show voice ep101 [--changed\|<line>\|<beat>]` · `show voice audition <char> "<text>"` · `show voice ingest <wav> --line <id>` | Dialogue |
| `show music ep101 [<cue>]` · `show music import-barmap <master>` · `show sfx build` | Music and SFX |
| `show mix ep101 [--range a..b] [--target web\|r128\|a85\|dlg27]` | Stems and masters |
| `show render ep101 <shot\|beat\|range\|all> [--jobs N] [--frame N]` | Frames into the cache |
| `show sheet\|strip\|timing\|frame\|diff\|lint\|asr\|plan\|perf\|status\|verify …` | Review (§13) |
| `show preview ep101 <range>` | x3 MP4 with burn-ins and the temp mix |
| `show deliver ep101 web [--range a..b] [--out DIR] [--require human-voice] [--tail fade=R13] [--release]` | Final files and QC; `--release` also builds the upload package and requires the signed checklist (§13.7) |
| `show standards ep101` · `show signoff ep101 "<creator's reply>"` | The standards report; the sign-off bound to the manifest hash |
| `show vault archive ep101` | Moves a released episode's masters and stems to the vault and frees the disk (§12.2) |
| `show art import\|sheet\|build …` · `show doc2px <n>` · `show ids new ep101 <shot>` · `show notes add …` | Art, units, ids, notes |

---

## 15. Performance on this machine (4 CPUs, no GPU, 15 GB RAM, ~4.9 GB free disk)

### 15.1 Measured (m)

| Step | Result | Source |
|---|---|---|
| Emulator boot, unthrottled | 1.07-1.1 s | `bench/log_boot0.txt` |
| 600 f, 2D cutscene, Lua crop, unthrottled | 4.04 s incl. boot (**4.9 ms/f**) | `eval_td/results.txt` |
| 3D cellar, 300 f with 1 MB dumps | 3.9 s (≤ 13 ms/f, dumps included) | `bench/log_combat.txt` |
| 3 emulators in parallel | 1,800 f in 8.24 s = **1.6x** | `eval_td/results.txt` |
| Determinism | 4 runs byte-identical (sha `f23a33ae…`); 150/150; 120/120 | `eval_td`, engine_map |
| BGR555 → RGB24 / LUT lint | 0.55 ms / 0.29 ms per frame | engine_map, `spec_bench` |
| FFV1 from a raw stream | 300 f in 0.54 s (~555 fps), 4.8 MB | `spec_bench` |
| Decode + framemd5 | 300 f in 0.38 s | `spec_bench` |
| H.264 x3 preview (veryfast) | 156 fps | `spec_bench` |
| H.264 x6 HD (medium / slow, CRF 16) | 33 / 35 fps, 0.58 MB per 300 f | `spec_bench` |
| H.264 x12 UHD (medium) | 10 fps, 0.84 MB per 300 f | `spec_bench` |
| ProRes 422 HQ HD, `prores_aw` | 87 fps, 89.5 MB per 300 f | `spec_bench` |
| ProRes 4444 UHD, `prores_aw` / `prores_ks` | 19-21 / 2.5 fps | `bench_b/results.txt` |
| Kokoro: load + first line; RTF | 2.6 s; 0.64 (5 lines, 2 threads) | `spec_bench/tts_probe2.py` |
| Whisper small int8 | load 0.8 s; 3.1-4.2 s per short line | `spec_bench` |
| cmudict load | 0.54 s, 126,052 words | `spec_bench` |
| Native runner, 300 f at 384x216 with PNGs | 0.20-0.29 s | `bench_b`, `verify_eval` |
| **v1.1 review (`critique_nye/`):** | | |
| Game PS1 build, fresh, 4 CPUs | 83 s; 1,240 KB + 549 KB .bss, 72 KB left below the stack top | `critique_nye/timeA.txt`, `buildA.log` |
| Game build reproducibility across directories | EXE, OVL.BIN, TEXT.BIN identical (c0c19b66…, 982bdfbb…, 50d371f0…); the `.bin` image differs (mkpsxiso timestamps) | `critique_nye`, `critique/iso` |
| **Show link probe** (no game file changed) | 2D: 412 KB + 389 KB .bss, 1,182 KB left; with VIEW3D/OVERWORLD/ROOM: 556 KB + 465 KB, 963 KB left | `critique_nye/probe/p2d.map`, `p3d.map` |
| Show probe link closure | Needs `options.c` (camfx and ui call `opt_get`), `town.c` (`doomo_town`, from beat.c and hcmd.c), no-op `scene_set/push/top` (hcmd.c's weak refs fail to link), and for ROOM `home.c`, `home_draw.c`, `home_edit.c`, `furn_*` | `critique_nye/probe/*.err` |
| **Show probe boots in PCSX-Redux** | 300 frames, 30 fps, 0.38 ms CPU per frame; a vacant bust at `px=2`, a figure and a chibi drawn by the game's portrait engine | `critique_nye/probe/o2d/f300.png` |
| Emulation alone, unthrottled, no capture | 2D concert 5.7 ms/f, island 4.9 ms/f, 3D overworld 6.7 ms/f (300 f each, boot subtracted) | `critique/emu/*.log` |
| HD x6 H.264 CRF 16, 3 threads | veryfast 45 fps, faster 49 fps, medium 39 fps (300 f) | `critique_nye/enc_*.mp4` |
| Kokoro determinism | 3 renders in one process and 1 in a new process: identical samples (sha `1a8e91f0…`); 1.25 s of audio in 0.75-0.80 s warm | `critique_nye/tts/det.py` |
| Kit render, a MINGLE-sized cue (8 bars at 96 bpm; musicbox, warm_pad, chip_tri, drums) | 22.5 s of audio in 5.5 s (4.1x real time), first run | `critique_nye/kit/mingle_time.py` |
| `textpack.Compiler.compile_beat` read-only | Works with a stub pack; 1,439 symbols load in 0.05 s | §4.2 |
| Free disk | 6.8 GB → 6.3 GB → 5.6 GB → **4.9 GB** (low point of this review) → 7.7 GB an hour later; other sessions share the disk | `df` |
| `gh` authentication | **Not logged in** ("The token in GH_TOKEN is invalid"); api.github.com reachable | §10.7 |

### 15.2 Estimates built on them (e; confirmed by the named tests)

| Job | Estimate | Test |
|---|---|---|
| **M1: 390 frames** picture | ~3 s of emulator + boot | M1-T1 |
| M1 end to end (compile, TTS walla, music, mix, render, preview + HD encode) | **under 2 min** cold, under 30 s warm | M1-T1 |
| Cold open, 3,000 f picture | ~20 s (2D) | M2-T1 |
| **Episode picture, 39,600 f** | **~4-6 min** on 1 instance (85 % 2D at ~5 ms, 15 % 3D at ~11 ms, plus ~270 shot preps); ~3-4 min with `--jobs 3` | M3-T1 |
| Episode TTS (~200 lines, ~2 s each) + Whisper QA | ~4 min + ~11 min, changed lines only | M3-T3 |
| Episode score render (cached per cue) | ~3-10 min cold (v1.1: the kit renders at ~4x real time (m), and about half an episode is scored) | M3-T3 |
| Episode HD web encode / UHD web (optional) | ~15-20 min / ~66 min (ProRes deferred) | M3-T6 |
| **Episode, cold, to an HD web MP4 with audio** | **~45-75 min** | M3-T6 |
| **Four episodes, all machine time** | **~4-5 hours** in total, across ten weeks | M4 |
| One-shot fix to a new preview | seconds to render + under 1 min to re-encode its segment | M2-T7 |

The requirement's ceiling (8 h overnight for an episode) is met by a factor of about 8. **The New-Year schedule is bound by Claude's working time and the creator's review latency, never by machine time** (roadmap §2).

---

## 16. Work packages

Each package lists its 2026 scope, its week on the roadmap, files, dependencies and acceptance tests (AT). "Claude-days" is a rough measure of focused sessions; the plan needs about 36 for the engine and 8-12 per episode (roadmap §2). **Ownership:** every package writes only under `show/` (and, for WP0, the vault repo). Nothing writes to `core/`, `gba/`, `ps1/`, `tools/`, `trailer/` or `docs/show/`; those are read, imported or compiled from, never edited (§4.3, §4.4).

| WP | 2026 scope | Week | Key files | Needs | Acceptance tests | Claude-days |
|---|---|---|---|---|---|---|
| **WP0 Vault and environment** | Preserve the emulator, BIOS, SDK, models, kit and theme, with the §10.7 fallback order; the opt-in restore hook; `show doctor` (disk, tools, hashes) | W1 Mon | `showtk/vault.py`, `doctor.py`, `show/vault.lock`, `show/hooks/` | — | **AT0.1** On a fresh container, `show vault fetch && show doctor` is all green. **AT0.2** The stripped emulator renders `CUTSCENE 256` × 600 f byte-identical to the original (sha `f23a33ae…`). **AT0.3** Every vault hash matches `vault.lock`. **AT0.4** `show/audio/kit` imports and renders `demo/groove16.py` identically to the scratch copy | 1 |
| **WP1 Zero-touch build and gate** | `show/engine/CMakeLists.txt` + presets, curated sources (the measured closure, §8.7), the portrait patches (a)-(d), (f), `check-game` | W1 | `engine/*.cmake`, `engine/patches/` | WP0 | **AT1.1** `show check-game` passes, including `--full` (EXE, OVL.BIN, TEXT.BIN identical to the merge-base build). **AT1.2** `cmake --preset ps1-show` builds `show.exe`; `elf2exe` passes; the RAM report is within §8.7. **AT1.3** With overrides at -1, show-built portrait images equal the game's for 20 roster looks × 11 expressions (CRC, captured on the PS1). **AT1.4** A patch that no longer applies stops the show build with the file name | 1.5 |
| **WP2 Player skeleton and slug stage** | `show_game` (incl. the defs of §4.1), `show_load` (disc), `show_mbox`, purity reset, `show_ext` (host kinds incl. IRIS, TINT, POP, EXPR), slug cards | W1 | `engine/show_*.c` | WP1 | **AT2.1** Boots to the test chart in ≤ 1.2 s unthrottled. **AT2.2** `show slug ep101` gives 39,600 frames. **AT2.3** Two renders give identical `frames.sha256` and P1 logs | 1.5 |
| **WP3 Harness and capture** | Forked harness, Lua crop to FIFO, `capture.py` (exact RGB, LUT, FFV1, hashes), P1/P4 logs, seek, `--jobs 2`, cache | W1 | `emu/*`, `capture.py`, `render.py`, `scheduler.py`, `cache.py` | WP2 | **AT3.1** 600 f ≤ 5 s including boot. **AT3.2** `--jobs 2/3` throughput recorded. **AT3.3** A seek to co02+150 alone equals that frame from a full run (hash). **AT3.4** A hang or crash names the shot and frame | 1.5 |
| **WP4 showc** | v0 (W1): rundown → slug. v1 (W2): parse, macros, time solver, `textpack_bridge`, stage resolve, plan, `.SHW` emit, sidecars, `doc:` units, scenes and standards tags, error format | W1-W2 | `showtk/*.py` | WP2 | **AT4.1** The §6 example compiles to the frames and samples in §6.4 and §6.6 exactly (with the v1.1 iris encoding). **AT4.2** `shw_format.py` round-trips with `shw_format.h`. **AT4.3** A bad anchor, unknown symbol or cycle gives `file:line` errors. **AT4.4** One-shot recompile < 1 s | 3 |
| **WP5 Stage interpreter** | BANDS, IMAGE, STROKES (reveal, boil), GRAIN (W2); TILEMAP (W3); Q16 parallax; per-plane scale; matte; generators and the bake | W2-W3 | `show_stage.c`, `show_fx.c`, `art/gen/*.py` | WP4 | **AT5.1** Planes drift at 0.0417/0.0833/0.1667 px/f with no accumulated error over 10,000 f (P1). **AT5.2** The reveal reaches 100 % at the scripted frame. **AT5.3** 2D frames are 100 % on-palette (PIC-PAL) | 3 |
| **WP6 Actors and crowds** | `show_cast` (fills `doomo_town.res[]`), busts, figures, chibis, clocks, expr/mouth/blink tracks, flipbook poses (W2); crowds with lanes, ripples and K-look reuse (W2-W3). Gaze and the atlas deferred | W2-W3 | `show_cast.c`, `show_actor.c`, `show_crowd.c` | WP5 | **AT6.1** The Mayor model sheet renders on the PS1 with 11 expressions × 8 visemes. **AT6.2** `freeze` stops the wings for exactly 13 f. **AT6.3** 12 kids cross with ripples of 0-3 f. **AT6.4** (W3) a 150-citizen crowd from ≤ 48 looks at 0 evictions per frame. **AT6.5** Two actors with equal clock values both show their due blink phase on the same frame (patch (f)) | 3.5 |
| **WP7 Text and cards** | SUPER styles (W2, game font); the child's-hand `.pxt` font, signs, cards, the TV-MA card (W3) | W2-W3 | `show_text.c`, `art/pxt/*` | WP5 | **AT7.1** The co02 SUPER's 52 glyphs land on frames 120-222. **AT7.2** TIME-READ fires on co03 as in §6.5 | 1.5 |
| **WP8 Camera and FX** | camfx host kinds: irises (circle, heart), fades, flashes, shakes, palette lerps (W2); whips, cranes, brick wipe, Split, dissolve (W3) | W2-W3 | `show_cam.c`, `show_fx.c` | WP5 | **AT8.1** The co02 iris opens over frames 90-103 centred on the bandstand (hold 5). **AT8.2** Flash lint catches a 4 f full-screen flash. **AT8.3** A shake never shows an uncovered edge | 2 |
| **WP9 Voice** | G2P (CMU dict data + lexicon), Kokoro, WORLD, audition, `voice.lock`, mouth tracks, Whisper QA, simple take ingest, provenance | W2 | `voice/*` | WP4 | **AT9.1** The walla lines render, lock and reproduce from the vault. **AT9.2** The `dx_proof` closures are ±1 f. **AT9.3** Swapping a line TTS → recording changes only that line's audio, mouth track and anchored events. **AT9.4** `--require human-voice` lists every TTS line | 3 |
| **WP10 Music and SFX** | `mml_legacy.py` over `tools/mmlc.py` → kit; cue spotting; theme bar map; mute/duck/bend; SFX from `sfxgen.py` at 48 kHz and from `audio_fx.c` SynthSeg tables; 2 foley synths | W2-W3 | `music/*`, `sfx/*` | WP4 | **AT10.1** MINGLE's first onset at sample 96,000 ±48. **AT10.2** The theme bar map gives 48 f per bar. **AT10.3** SHWIP at 48 kHz matches `runner --wav`'s SHWIP envelope (resampled) within 1 dB. **AT10.4** SFX names and captions resolve | 2 |
| **WP11 Mix and master** | Kit mixer from `audio_events.json`, DX/MX/FX buses, stereo `web` master, QA, P4 cross-check, `AUD-LIC` | W2 | `mix.py`, `master.py` | WP9, WP10 | **AT11.1** `web` −16 LUFS ±0.5, ≤ −1 dBTP. **AT11.2** P4 shows every event *k* on its frame. **AT11.3** Re-mixing gives the same hash | 1 |
| **WP12 Assemble and deliver** | EDL, segments, HD web recipe, x3 previews, SRT/VTT, chapters, manifests | W1 (v0) - W2 | `deliver.py`, `ffmpeg.py`, `captions.py` | WP3, WP11 | **AT12.1** The M1 MP4 checks out (1920x1080, 30 fps, 390 f, AAC 48 kHz). **AT12.2** framemd5 of the decoded HD crop matches the masters ×6 sampled | 1.5 |
| **WP13 Review tools** | sheet, timing, frame overlay, preview, status, notes (strip, heat-map diff deferred) | W1-W2 | `review/*` | WP3 | **AT13.1** `show timing co01..co03` reports every event's picture change on its frame. **AT13.2** A before/after sheet shows the changed tiles | 1.5 |
| WP14 runner-show | **Deferred to 2027** | — | `show/native/*` | — | — | 0 |
| **WP15 Episode scale** | OVERWORLD and ROOM on the game's maps and house (W4); VIEW3D first person on the game's maps with HUD and the filter switch (W5); main title on the TV-size bar map, eyecatch, credits (W4-W6); runtime proof | W4-W6 | many | WP5-WP13 | M3 tests | 5 |
| **WP16 Season tooling** | `show new-episode` templates for shape A, the series libraries (sets, looks, macros, crowds, cues, voices), the reuse report | W6 | `series/templates`, `showtk/season.py` | WP15 | M4 tests | 2 |
| **WP17 Standards and release** (new) | The hard-limit lints and fixtures (W2); `show standards`, the signed checklist, the release package and `vault archive` (W6) | W2, W6 | `showtk/standards/*`, `release.py` | WP4, WP12 | **AT17.1** Every fixture in `tests/standards/` fails its lint. **AT17.2** `--release` refuses an unsigned or stale sign-off. **AT17.3** The release package has every item in §13.7 | 2 |
| WP-opt (2027) | GP0 attribution; P2 trace; gaze; crowd atlas; the wide canvas; the disc edition; broadcast/5.1/AD/TTML; psxsoft | — | — | — | Each when needed | — |

**Totals:** about 36 Claude-days of engine work, front-loaded into W1-W3 (about 7, 17 and 5), then about 8 (WP15-WP17) in W4-W6 alongside the pilot's content.

---

## 17. Milestones

### M0, "First light" (W1: Mon Oct 26 - Sun Nov 1)

**Builds:** WP0, WP1, WP2, WP3, WP4 v0 (rundown only), WP12 v0, WP13 v0.

| Test | Pass when |
|---|---|
| M0-T1 | The tools are vaulted and restorable (AT0.1-AT0.3), by whichever §10.7 route worked |
| M0-T2 | `show check-game --full` is green, and `git diff` shows nothing outside `show/` |
| M0-T3 | **The 22:00 slug animatic, rendered by `show.exe`**, as a burned-in x3 preview MP4 (960x540, 156 fps encode (m)) in ≤ 15 min with 39,600 frames; a second render's frame hashes are identical. The HD version is optional (about 15-20 min more) |
| M0-T4 | Measured and recorded in `show/README.md`: `elf2exe` RAM against §8.7, ms per frame with the Lua crop, the `--jobs 2/3` speed-up |
| M0-T5 | The 8 MB setting is tested on the installed emulator (works or is struck from the hatch list). PCdrv is tested only if time allows |
| M0-T6 | The canvas look: the test chart and a slug-art co12 at 320x180, shown at 1080p, confirmed by the creator (1 minute). The wide comparison is deferred |

### M1, first scenes (W2: Mon Nov 2 - Sun Nov 8; checkpoint CP1 on Wed Nov 11)

**Builds:** WP4 v1, WP5 (no TILEMAP), WP6 (M1 part), WP7 (game font), WP8 (iris, fade), WP9, WP10 (MINGLE, SFX, 2 foley), WP11, WP12, WP13 v1, WP17 lints. **v1.1 re-scope:** v1.0's M1 needed about 27 Claude-days by the spec's own estimates (three weeks in v1.0's calendar); this M1 has one week. Cut from it: the child's-hand font (M2), the stroke-speed-following foley (now noise bursts over each reveal span), `runner-show`, the timing strip, P2. Added to it: the **dialogue proof**, because two characters talking is most of every episode and the single word "Perfect." proved too little. **Achievability:** this is still the plan's peak, about 17 Claude-days of work in 7 calendar days. It is achievable only as three parallel streams that share nothing but `timeline.json` and `audio_events.json`: (1) the C player (WP5-WP8), (2) the compiler, review and standards tools (WP4, WP13, WP17), (3) the Python voice, music, mix and deliver tools (WP9-WP12). M0 already de-risks the hardest unknown (the stand-alone build boots and draws busts, §15.1). If M1 is not green by **Wed Nov 11 (CP1)**, the roadmap's cut order applies instead of a slip.

**Deliverables:** `show deliver ep101 web --range co01..co03` writes `ep101_co01-co03_web_hd.mp4` (13.000 s) + `.srt` + `manifest.json`; `show deliver dx_proof web` writes the dialogue proof (about 20 s); `show preview` writes the burned-in previews.

| Test | Pass when |
|---|---|
| M1-T1 | ffprobe shows 1920x1080, 30/1 fps, 390 frames, 13.000 s, AAC stereo 48 kHz. Cold end-to-end ≤ 2 min, warm ≤ 30 s |
| M1-T2 | The manifest names `show.exe`, emulator, BIOS, harness and patch hashes. Each shot carries a badge (expected PS1-LEGAL) |
| M1-T3 | A second render gives identical `frames.sha256`, P1/P4 logs and mix hash |
| M1-T4 | Event frames: fade 0-7; reveal ends at f 24 and f 60; iris 90-103; the co02 SUPER's first glyph at f 120 and last at f 222; the co03 SUPER from f 283 to f 333 (or as the creator re-times it) |
| M1-T5 | Audio: SHWIP at smp 144,000 and MINGLE's first onset at smp 96,000, each ±1 ms (onset detection on the stems); P4 confirms each *k* on its frame |
| M1-T6 | **Walla:** the kids' walla lines are on the DX stem in co02-co03 at the scripted gains, locked in `voice.lock` with provenance, and the SRT carries "[soft crowd babble]" from 00:00:03,000 |
| M1-T7 | **Music:** MINGLE from `mml_mingle.c` (through `tools/mmlc.py`) voiced by the kit's `musicbox`, entering at 00:00:02:00 |
| M1-T8 | Loudness `web`: −16 LUFS ±0.5, ≤ −1 dBTP |
| M1-T9 | Lints: no errors on co01-co03; the TIME-READ warning on co03 is decided (default: keep the shot list's timing); **every standards fixture fails its lint** (AT17.1), and the co_storybook scene passes `STD-KIDS` |
| M1-T10 | **The dialogue proof** (`tests/dx_proof`): the Mayor and Nana in a two-shot (`px=1` figures) and two close-ups (`px=2` busts), four lines (for example "Perfect." / "Two of everything, dear." / "Stay." and one more from the pilot's Act One), TTS. Each `p`, `b` and `m` onset shows an `M` within ±1 f; at least 3 distinct visemes per line; blinks continue under the mouth override; the cut points follow the lines |
| M1-T11 | **The creator signs off the look** (crayon weight, grain, boil, the kids, the Mayor and Nana) from numbered options, or accepts the defaults |

### M2, the full 1:40 cold open (W3: Mon Nov 9 - Sun Nov 15; CP2 on Wed Nov 18)

**Adds:** the pixel-art bandstand set (TILEMAP); the Mayor (figure and bust; ladder, roller and balloon flipbooks; glances by `headturn`, expressions and cutaways, since gaze is deferred); the fountain insert; the banner bloom with the red drip; the 4 f insert (`replay=co07@+8`) and `base=` shots 9 and 18; the Wall plane and the brick-wipe line; fireworks with luminance ramps; the plaza crowd (K-look reuse, 150+ citizens); Mascotty; Nana's paw; the egg and the heart iris; the bus-horn notes from the theme master; MINGLE's 29 f mute and the hum at the Mayor's note (Kokoro `sing()`); the child's-hand font. In parallel: **the pilot's script is locked**, with every line tagged for standards, and a TTS table read rendered. (AD is deferred.)

| Test | Pass when |
|---|---|
| M2-T1 | 3,000 frames, 21 shots, rendered by `show.exe`; the runtime proof equals the shot list's beat map |
| M2-T2 | Composition locks: co09 = co02@+90 (P1 equal except the Wall layer); co18 = co16@+39 + one change; co17 = co07 frames +8..+11 (hash equal) |
| M2-T3 | Every event in the shot list's tables lands on its frame (`show timing` report attached); the fountain at exactly R8 after `co13_heehee.end` |
| M2-T4 | "...Hee. Hee." is the identical library take; "Perfect." and "Stay." have approved mouth tracks |
| M2-T5 | The standards report is clean: no flash frames, firework ramps ≥ 8 f, the insert is 4 f, no OMINOUS cue, the filter on with children in frame, `STD-KIDS` green per scene |
| M2-T6 | Two renders are bit-identical; all lints are green or waived with reasons (never a hard-limit lint) |
| M2-T7 | A one-shot note round trip (edit → render → preview) takes under 2 min |
| M2-T8 | The creator watches the cold open at 1080p and says "fine" or gives notes, and approves the pilot script ("lock it") |

### M3, the pilot released (W4-W6: Mon Nov 16 - Sat Dec 5; CP3 on Wed Dec 9)

**Adds:** Act One (W4), Acts Two and Three (W5), Act Four, the tag and the credits (W6); the main title on the TV-size bar map and the eyecatch; the OVERWORLD wides and the ROOM cottage on the game's maps and house (W4); VIEW3D first person on the game's existing 3D maps, with the HUD, rails, hit-stop and the filter switch (W5); about 270 shots and 150-250 lines; the standards report, the signed checklist and the release package (WP17). **Deferred:** 5.1, TTML/SCC, AD, the broadcast, textless and 2x11 versions, new 3D maps.

| Test | Pass when |
|---|---|
| M3-T1 | The whole episode is rendered by `show.exe` in ≤ 15 min of picture time (the estimate is 4-6 min) |
| M3-T2 | The runtime is inside the web window (20:00-23:00); protected holds are intact |
| M3-T3 | Every line has a take; `voice.lock` is complete; the ASR lint is green; the provenance report lists every TTS line |
| M3-T4 | The main title's crack is on frame 0 = the downbeat; cast pops on 12 f beats (`@bar:` resolved, ≤ ½ f error) |
| M3-T5 | Clinker's slip is sample-exact (12,800 smp, or 12,816 where set) and "...Seven." is on time |
| M3-T6 | `show deliver ep101 web --release` produces the HD MP4, SRT, VTT, chapters, the loudness report and the upload package. Cold, the HD web MP4 takes ≤ 90 min |
| M3-T7 | `show verify ep101 --sample 20` is identical. The manifest is tagged `ep101-v1.0-web` |
| M3-T8 | **Standards:** every `STD-*` lint is green; the creator's checklist (§13.7) is signed against this manifest; every shot's badge is listed and any EMU-ONLY shot carries its reason |
| M3-T9 | The creator has the package in hand and uploads it (target Sat Dec 5) |

### M4, the season loop: 102, 103, 104 (W6-W10)

**Builds:** WP16 (W6). The libraries grown in the pilot (acting macros, sets, crowds, SFX, cues, voices) become the series kit, and each episode runs the roadmap's one-week loop:
1. `show new-episode ep10N --shape A --from ep101` (Monday).
2. Claude re-reads `docs/show/` for the episode, writes the one-page outline, the beat sheet, the rundown and the script with tagged line ids; the creator approves the outline.
3. TTS of every line, the table read and the slug animatic on the PS1, the same day.
4. Claude stages the four acts from the libraries (Tue-Thu), renders, lints and self-reviews; an Act One preview on Wednesday.
5. The full cut with score and mix (Fri); the creator's notes.
6. Fixes, the standards report, the signed checklist, `--release` (Sat); the creator uploads.
7. `show vault archive ep10N` (Sun).

| Test | Pass when |
|---|---|
| M4-T1 | Each episode compiles from its template with a slug animatic of its full length on its first day (102: by Mon Dec 7, drafted in W6; 103: Mon Dec 14; 104: Mon Dec 21) |
| M4-T2 | Reuse report: ≥ 60 % of each episode's sets, looks, macros and SFX come from the series libraries (`show status --reuse`) |
| M4-T3 | Each episode passes M3-T1 to M3-T8 (with its own manifest) |
| M4-T4 | Dates: 102 released by Sat Dec 12, 103 by Sat Dec 19, 104 by Thu Dec 31 (CP3/CP4 in the roadmap say what moves if not) |
| M4-T5 | The creator's time per episode stays near an hour (tracked from the timestamps of their replies in `notes/`); if a note recurs, it becomes a library macro before the next episode |

---

## 18. Risks

| # | Risk | Effect | Mitigation |
|---|---|---|---|
| R1 | **The emulator, SDK, kit and models exist only in this container's scratchpad** | Losing the container loses the master renderer | WP0 on day 1, by the §10.7 route that works (`gh` is not logged in here (m)); pinned hashes; the opt-in restore hook. The emulator can be rebuilt from its local source |
| R2 | PCdrv and 8 MB are untested | Dev hatches missing | Not on the critical path: disc images rebuild in seconds; the measured RAM headroom (§8.7) makes the 8 MB hatch unlikely to be needed (M0-T5) |
| R3 | Parallel emulators scale only 1.6x | Slower full renders | Even one instance renders an episode's picture in minutes |
| R4 | Data-driven acting can look stiff | Weak performances | Close-ups at `px=2/3`, head turns, blink and clock tracks (gaze is deferred), macros from the gag bible, flipbooks for hero beats, a real-time review of every act |
| R5 | Lip sync from TTS durations and Whisper is approximate | Mouths look off | The 2 f hold, 1 f lead and M-closure rules; per-line mouth overrides as text; creator sign-off in M1-T10 |
| R6 | Mouth and blink variants multiply cached images | Pool or VRAM pressure in talky shots | 90 KB pool (240 blocks, the most the `uint8_t` owner table allows); the planner counts blocks; `PLAN-*` warnings; prep-time renders |
| R7 | The espeak wheel misbehaves here (m) | G2P gaps | cmudict + lexicon as the primary path (works (m)); inline phonemes |
| R8 | ONNX drift across machines | TTS not reproducible | Approved takes locked in the vault by hash (identical across runs on this container (m)) |
| R9 | Disk: as low as **4.9 GB** (m; shared with other sessions) | Cache, masters and stems squeezed; one episode's working set is 2-2.5 GB | One episode in progress at a time; `show vault archive` after each release; FFV1 masters only; ProRes never; LRU purge; `show doctor` warns below 2 GB |
| R10 | Game workflows change shared code (portrait.c, ps1/CMakeLists.txt and the PS1 platform are active; the repo has live worktrees) | The show build breaks | **No game file is edited** (§4.3): the show compiles patched copies and its own project, so a game change can only break the show build, loudly, and is fixed in `show/engine/patches/`. Game changes arrive by a weekly merge, never mid-episode |
| R11 | Content rules keep moving (TV-MA re-aim) | Lints drift | Rating profile and standards as data; hard limits as hard lints |
| R12 | The 320x180 canvas feels cramped | Composition | The M0 look; `doc:` units keep both canvases one switch apart, so the wide canvas can come back in 2027 without rewriting scripts |
| R13 | The TTS-versus-human policy conflicts with the docs' studio-era rule | Disputes later | Provenance on every line, labelled renders, `--require human-voice`; the creator decides (§SELF_MADE_ROADMAP) |
| R14 | PCSX-Redux is not cycle-exact | The PS1-REALTIME badge could be optimistic | That badge is deferred with the disc edition; a hardware check before any claim |
| R15 | **The schedule is bound by Claude's working time** (about 80 Claude-days in 67 days) | Dates slip | The roadmap's checkpoints CP1-CP4 cut scope in a fixed order; the floor is 101 + 102 by Dec 31 |
| R16 | **Hard-limit lints depend on tags** | An untagged suggestive line slips past the lints | Claude tags as it writes; `STD-TAGSCAN` catches untagged matches; the creator's signed checklist is the final gate; hard-limit lints cannot be waived (§13.5, §13.7) |
| R17 | **YouTube removes or misclassifies a cute-looking adult cartoon** ("misleading family content") | An episode taken down or a strike | Not made for kids + 18+ on every upload, the TV-MA card at the head, adult titles and thumbnails (§13.7, roadmap §7) |
| R18 | A false Content ID claim on CC0 or royalty-free samples | A claim on a release | The provenance log and §10.8 licence table as the dispute evidence |
| R19 | The two portrait patches drift from the game's files | A failed patch stops the show build | The weekly merge refreshes them the same day (AT1.4) |

---

## Appendix A. Numbers used

| Fact | Value |
|---|---|
| Samples per frame | 1,600 at 48 kHz; 800 at 24 kHz; 735 at 22.05 kHz |
| Episode | 39,600 f = 1,320 s = 63,360,000 smp |
| 16:9 scales | 320x180 x6 = 1920x1080; x12 = 3840x2160; 135x240 x8 = 1080x1920; 320x240 x9 = 2880x2160 |
| 96 bpm (MINGLE) | 18.75 f per beat; 75 f = 120,000 smp per bar |
| 150 bpm (theme) | 12 f per beat; 48 f per bar |
| Kokoro duration unit | 600 smp at 24 kHz = 25 ms = 0.75 f |
| `DoomoLook` | 48 B (static-asserted in resident.h:124) |
| Portrait pool | 48 × 384 B blocks = 18 KB in the game; 240 blocks = 90 KB in the show (255 is the hard maximum: `owner[]` is `uint8_t` and `PC_FREE` is 0xFF) |
| Display list | `PRIM_BYTES` 56 KB × 2; `OT_LEN` 1,536 |
| Tool hashes | pcsx-redux (debug) `56837ef4b2805fb6…`; OpenBIOS `175836cc20365609…` |

## Appendix B. Grafts folded into design A (from the two judge panels)

| Graft | Where |
|---|---|
| Shot purity: `portrait_cache_reset()` plus `portrait_cache_clock(…, DL_STEPS)` on every fetch; RNG from episode + shot seed | §8.2 |
| P1 state hash, P2 depth-0 draw trace (ld `--wrap`), P4 audio-event ids; `show verify` names the first differing frame and channel | §8.9, §13.1 |
| `runner-show` native build for logic only (UBSan, gdb); never pixels; no parity suite (deferred to 2027 in v1.1) | WP14, §1.2 |
| A LOOK chunk instead of the 164 KB roster | §7.3, §8.7 |
| Per-shot FFV1 masters; per-shot ProRes with `prores_aw`, joined with `concat -c copy` | §12 |
| The wide hatch as a single-buffered 384x216 with relocated page-0 slots and `#ifndef` guards | §8.8 |
| The canvas decided by eye at M0, with `doc:` units in both directions (v1.1: fixed at 320x180 for 2026; the wide comparison returns in 2027) | §2.1 |
| Mouth timing from Kokoro `dur_pred`; Whisper + MFCC-DTW for recordings; mouth tracks as one character per frame; the guarded override from day one (and its cache-key fold, added here) | §9.3, §4.3 |
| Creator-owned `takes.toml` with `retime=fit\|take`; take order rec > scratch > lib > tts; approved TTS locked by hash (vault, not LFS, in v1.1) | §5.6, §10.2 |
| C's base/replay/change grammar with the one-change lint | §5.4 |
| Dithering and Gouraud kept in 3D set kinds; the palette lint by primitive class (and the finding that the 2D layer is already dither-free, so no dither hunk is needed) | §2.3 |
| C0 hatches legal for masters; PS1-LEGAL/REALTIME badges per shot; the planner warns, never blocks; the disc edition off the critical path | §8.10 |
| The crowd atlas with recolour CLUTs and an enlarged pool | §8.5 |
| showc wraps textpack's `Compiler` class (no standalone `compile_beat` exists) | §4.2 |
| The vault, from M0 step 0 | §10.7, WP0 |
| The speed table updated from measurements (1.6x parallel, 4.9 ms/f with Lua crop) | §15 |
| GP0 trace attribution as an optional later debugger; a software GPU only as a deferred port of the local `src/gpu/soft` | §13.6 |

## Appendix C. Critique fixes (v1.1, 2026-10-25)

An adversarial review of v1.0 against the real code at `9d4d915`, re-aimed at the creator's New-Year milestone. Evidence is in `showengine/critique_nye/` (a fresh game build twice, a show link probe that boots in the emulator, encode, TTS and kit timings) and `showengine/critique/` (emulator timings, disc-image comparison). Every item below is fixed in the section named.

**Schedule and scope**

| # | Problem in v1.0 | Fix |
|---|---|---|
| C1 | The milestones ran to a June 2027 pilot; the creator wants several finished episodes by 2027-01-01 with minutes of their own time | Dated milestones (M0 Nov 1, M1 Nov 8, M2 Nov 15, M3 Dec 5, M4 Dec 12/19/31); a week on every WP; everything not needed for an online release deferred (§1.2, §16, §17; roadmap v2.0) |
| C2 | **M1 was not achievable in its new slot.** Its packages add up to about 27 Claude-days by v1.0's own estimates | M1 re-scoped to one week: no child's-hand font, simpler foley, no `runner-show`, no strip, no P2. A **dialogue proof** (two characters, four lines, two-shot and close-ups) replaces the single-word lip-sync test, because dialogue is most of every episode (§17 M1) |
| C3 | M0-T3 asked for a 39,600-frame HD MP4 in ≤ 15 min; the HD encode alone runs at 39-48 fps (m), about 14-17 min, plus the render | M0-T3 renders the animatic as an x3 preview (156 fps (m)); HD optional (§17 M0) |
| C4 | M3 required the broadcast version, TTML/SCC, AD and 5.1 | Deferred; M3 is the web release plus standards and the upload package (§12.3, §17 M3) |
| C5 | `FMT-RUNTIME` demanded exactly 39,600 f | Web window 20:00-23:00; exact length only for the deferred broadcast version (§12.3, §13.5) |

**Code-level errors (the spec relied on behaviour the code does not have)**

| # | Problem in v1.0 | Fix |
|---|---|---|
| C6 | **`-DPC_BLOCKS=256` corrupts the portrait cache.** `owner[]` is `uint8_t` and `PC_FREE` is 0xFF, so the blocks of cache entry 255 read as free and get overwritten while still in use | `-DPC_BLOCKS=240` (90 KB) plus a static assert `PC_BLOCKS < PC_FREE` (§4.3, §8.4, App. A) |
| C7 | **Missed phases.** `animated()` spends the whole tick's render budget after any new-content render, so a second actor whose clock has the same value that tick keeps its old blink/breath phase; seeks then differ from full runs. `portrait_cache_clock(t, 6)` does not prevent it | New patch hunk (f): no render budget in the show (§4.3, §8.2; AT6.5) |
| C8 | The gaze hunk assumed one pupil centre in `eyes()`; there are about 20 eye styles (`EYE_COUNT`), each drawing its own pupil | Gaze deferred; glances staged with head turns, expressions and cutaways (§4.3, §5.5, §17 M2) |
| C9 | Replacing `town_res()` and dropping `town.c` cannot link: `beat.c` (`set_flag`) and `hcmd.c` reference `doomo_town` directly | Link `town.c` (107 KB .bss (m)); `show_cast.c` fills `doomo_town.res[]` (§4.1, §8.4, §8.7) |
| C10 | "hcmd.c's scene commands are weak and resolve to nothing": **measured false**, the link fails on `scene_set`, `scene_push` and `scene_top` | `show_game.c` defines them as no-ops (§4.1, §4.2) |
| C11 | `camfx` and `ui` call `opt_get()` (screen-shake and reduce-flash options) | Link `options.c` and pin the show's option values so shakes and flash caps are deterministic and match `TIME-FLASH` (§4.1, §13.5) |
| C12 | `show_game.c` must replace everything `game.c` defines, including `doomo_image_new_uid()` (the VRAM image cache keys on it) and `doomo_frame_count()` | Listed in §4.1 |
| C13 | "`beat.c` handles … tint, iris": IRIS, TINT, POP, EXPR and others are host kinds returned as `BT_EXT` | `show_ext.c` implements them (§4.2) |
| C14 | **The co02 iris was mis-encoded.** `iris_in 1 13` is hold 13 (21 f in total), because `b` packs `hold \| heart<<6 \| (radius/2)<<7` and camfx adds R3 + R5 | `over=13` compiles to hold 5; fade-in needs a `camfx_fade(16)` setup (§6.4) |
| C15 | `beat_script_valid()` also requires sorted events; `compile_beat()` takes `(where, raw)` tuples and refuses a script without `end` | Stated, and verified by compiling a sample read-only (§4.2, §6.4) |
| C16 | The `CUTSTAGE` set kind cannot exist without linking the story scenes (the stages are static functions on scene state) | Removed; the game's crayon prologue (`draw_p0()`) is the look reference (§8.3) |
| C17 | The ROOM kind needs the house modules (`home.c`, `home_draw.c`, `home_edit.c`, `furn_*`), measured by the link probe | Listed in §4.1 and §8.3 |
| C18 | The synth one-shots (SHWIP, TINK, BOING …) are `SynthSeg` tables in `audio_fx.c`, not `sfxgen.py` output | SFX library parses `audio_fx.c` read-only; `sfxgen.py`'s generators are imported at 48 kHz (§4.2, §10.5) |
| C19 | A new MML parser was planned although `tools/mmlc.py` already imports the legacy MML tick for tick; the cue used kit presets that do not exist (`pad_soft`, `tri`, `shaker`) | Import `mmlc.py`; presets `warm_pad`, `chip_tri`, `drums_pop:70` (§4.2, §6.1, §10.4) |
| C20 | `main.c` calls `cutview_enter()` and `overlay_init()` unconditionally | Compile `cutview.c`, `overlay.c` and `txtload.c` without the game's mode macros (§4.1) |
| C21 | Seeking relied on `plat_draw_skipped()`, which the PS1's `plat_*` calls ignore | The player skips its draw step itself during pre-roll (§8.2) |
| C22 | M1 crowds depended on `runner-show --look-gen` (a deferred native build) | Looks are generated on the PS1 at prep with `resident_randomize_look()`, then the script's species, tags and age are applied (§4.2) |

**Game-build safety and ownership**

| # | Problem in v1.0 | Fix |
|---|---|---|
| C23 | Guarded hunks in `ps1/CMakePresets.json`, `ps1/CMakeLists.txt`, `portrait.c` and `portrait_head.c`, all files other workflows are actively changing (live `.claude/worktrees`) | **Zero-touch:** a stand-alone show CMake project and patched copies built under `show/build/`. Measured feasible: the probe links from the game's own objects with nothing changed and boots (§4.3, §15.1) |
| C24 | The gate compared the `.bin` disc image, which differs between two builds of identical inputs (mkpsxiso timestamps, measured) | Compare EXE, OVL.BIN, TEXT.BIN (measured identical across build directories); a path check on every commit, the full 83 s build weekly (§4.4) |
| C25 | A committed SessionStart hook would make every game session in the repo download about 1 GB | Opt-in hook in the creator's local settings, emulator and BIOS only (§4.1) |
| C26 | Repo-wide `.gitattributes` LFS rules for show binaries | No LFS in 2026; binaries in the vault, hashes in git (§4.1, §10.2) |
| C27 | The show's binary dirs were under `ps1/` | `show/build/<preset>/` (§4.1) |

**Performance and limits, re-measured**

| # | v1.0 claim | Measured now |
|---|---|---|
| C28 | RAM: "~1,430-1,620 KB, ~290-480 KB headroom" (the headroom line was the 3D case; `doomo_town` was not counted) | Linked closure 801 KB (2D) / 1,021 KB (3D) incl. `doomo_town`; with the player, pool and chunk buffers about 600 / 390 KB of headroom (§8.7) |
| C29 | HD encode 33 fps, about 20 min per episode | 39-48 fps at 3 threads, preset barely matters; about 15-20 min (§12.1) |
| C30 | Episode score 10-30 min cold | The kit renders a 4-track cue at about 4x real time: minutes (§15.2) |
| C31 | Kokoro output may drift, so takes need LFS | Identical across runs and processes on this container; takes still locked by hash in the vault for container changes (§10.2) |
| C32 | 4-6 min per episode picture | Consistent with emulation alone at 4.9-6.7 ms/f (m) |
| C33 | Disk 5.6 GB | As low as 4.9 GB (it moves by gigabytes; other sessions share the disk); one episode in progress at a time (§12.2) |

**Vault, release and licences**

| # | Problem in v1.0 | Fix |
|---|---|---|
| C34 | The vault relied on "the preconfigured `gh`", which is not logged in here | A three-route order (connector-created repo with split archives, an orphan branch, or `gh` after the creator logs in) (§10.7) |
| C35 | Nothing said how an episode reaches YouTube; the container cannot reach the creator's account | The release package and the creator's 10-minute upload (§13.7; roadmap §7) |
| C36 | The PyPI `cmudict` wrapper is GPL-3.0-or-later | Read CMU's BSD-licensed data file directly (§10.2) |
| C37 | No licence audit of what reaches the mix | §10.8: the table, the two CC BY credits (Salamander piano, DWSD), TimGM6mb never in a mix, `AUD-LIC`, Content ID dispute evidence |

**The hard limits**

| # | Problem in v1.0 | Fix |
|---|---|---|
| C38 | One rule (`STD-KIDS`) per **shot**, while the docs define hard limit 3 per **scene** and include dialogue about children | Scenes in the script format; `STD-KIDS` per scene, covering lines that refer to a child and voices off-screen (§5.5, §13.5) |
| C39 | No age data: a chibi-style cast cannot be judged by look | `age=` mandatory on every character, crowd and voice (`STD-AGE`, §9.1) |
| C40 | Hard limits 1, 2 and 4 (explicitness, consent, hate) had no rule at all | `STD-EXPLICIT`, `STD-CONSENT` (yes before contact, a no ends it, a step-back line, never consent as the gag), `STD-HATE`, `STD-TAGSCAN`; none waivable; failing fixtures for each (§13.5) |
| C41 | No human review step for what lints cannot judge | The standards report and the creator's checklist, bound to the render hash; `--release` refuses without it (§13.7) |
| C42 | No release-side safeguards for an adult cartoon on YouTube | Not made for kids, 18+, adult thumbnails and titles (the misleading-family-content risk), the TV-MA card (§13.7, R17) |
