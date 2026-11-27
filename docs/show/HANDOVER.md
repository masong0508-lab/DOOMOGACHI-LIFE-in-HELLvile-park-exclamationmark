# Pilot handover: "Mingle Night" (ep101), original master and engine

Written 2026-11-27 for a production handover (to another assistant or studio). It says what the original
pilot is made of, what is in this repository, what is missing and where it last was, and how to
reproduce the original master before changing anything.

Repository: https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark
Branch with everything: `claude/nice-allen-gr6czm` (also `main`).

## 1. What the original pilot is

The pilot is not hand-drawn or keyframed footage. **Every frame is drawn by code**: the game's own PS1 engine,
built as a stand-alone `show.exe`, plays a compiled episode script and is captured frame by frame from the
PCSX-Redux emulator at 320x180 (inside 320x240), 30 fps, scaled x6 to 1080p. The audio (dialogue, music,
sound effects) is rendered on the host at 48 kHz. The "engine" is therefore this repository's code plus a
pinned emulator, BIOS and PS1 SDK; the "assets" are the episode scripts, the game's art and sprite data,
the music bar maps and the voices.

**The original master** (the last delivered cut):

| item | value |
|---|---|
| file | `ep101_web_hd.mp4`, 352,115,737 bytes, 1080p, 30 fps, 22:00 (39,600 frames), 244 shots |
| delivered | 2026-10-26, commit `a26d05e7` "Show FINAL: ep101 rough cut delivered (244 shots re-rendered, full mix, web master)" |
| rendered from | source commit **`b15283495f964fb189680be5c0156f243b891032`** (clean tree) |
| render hash | `d4587b90989f345c2357226d0c68b942a4b5b943e7622a6814ad096f4ae103a2` |
| audio | stereo 48 kHz mix, sha256 `a93a2413f5a62a22a50554e6b2b2aabdd48027751db0672dbdc96424999732f8`, -16.0 LUFS |
| captions | `ep101_web_hd.srt` (sha256 `b23de245...36ad`), `.vtt` (`a82d1095...b502`), 333 cues |
| tool hashes | emulator `56837ef4...ed61`, BIOS `175836cc...74b0`, show.exe `95916960...99ed3`, harness `4d04b9f5...d106`, patches `8893d321...e9af` |
| voices | 142 lines were synthesized (temp TTS); no creator recordings were in that mix |

All of it is recorded in `show/ep101/deliveries/web/manifest.json` (chapters, per-segment keys, per-frame
MD5 spot checks), which is in git. **The MP4 itself was never committed** (it is a build output), and the
machine that rendered it no longer exists, so the file is not recoverable from the repository. It is
**reproducible**: the render is deterministic and the manifest's hashes let a new render be compared frame
for frame (section 3).

## 2. Inventory

### In the repository (complete, buildable)

| material | where |
|---|---|
| The show engine (episode compiler, renderer driver, mixer, delivery, QC; Python `showtk`) | `show/tools/showtk/`, command `show/tools/show` |
| `show.exe` (the PS1 player of episodes, CMake project reading the game's sources) | `show/engine/`, `show/include/`, built by `show build` |
| The game engine it draws with (characters, portraits, sprites, sets, 3D, palette, text) | `core/`, `ps1/` |
| Episode 101: scripts, shots, lines, cues, rundown, takes choice, mouth sync data | `show/ep101/` (`episode.shw`, `shots/`, `lines.shw`, `cues.shw`, `rundown.shw`, `takes.toml`, `mouth/*.mouth`, `voice.lock`, `strings/`) |
| Shot lists, continuity | `show/ep101/shotlists/` (`co_mt.md`, `a1.md`-`a4.md`, `CONTINUITY.md`), `docs/show/COLD_OPEN_SHOTLIST.md` |
| Screenplays and series bible | `docs/show/ep101/PILOT_SCRIPT.md`, `PILOT_SCRIPT_v2.md`, `docs/show/PILOT_BEATSHEET.md`, `PILOT_TREATMENT.md`, `SERIES_OVERVIEW.md`, `GAG_BIBLE.md`, `PRODUCTION_ROADMAP.md`, `docs/show/dev/` (pitches, digests) |
| Five-season and series planning | `docs/show/SERIES_OVERVIEW.md`, `PRODUCTION_ROADMAP.md`, `show/series/` (cast, acting, lexicon, sets) |
| Voice direction | `docs/show/ep101/VOICE_BIBLE.md`, `RECORDING_GUIDE.md`, `DIALOGUE_NOTES.md`, `lines.csv`, `sides/` |
| Music: bar maps, cue layout; theme song plan | `show/lib/music/barmaps/*.json`, `show/audio/`, `docs/show/INTRO_REDO.md` (the planned new opening) |
| Art used by the show | `show/art/`, the game's art in `core/art/`, `core/gen/` (generated sprite and part data) |
| Engine spec and decisions | `docs/show/SHOW_ENGINE_SPEC.md` (v1.1, binding), `show/DECISIONS.md`, `show/DEFERRED.md`, `show/README.md` |
| Pins of everything outside git | `show/vault.lock` (emulator, SDL3, BIOS, SDK, toolchain, TTS / ASR models, archive hashes) |
| The 70-second cold-open proof (crayon storybook + live-action look) | `pilot_proof/` (C, built with the teaser toolkit), `pilot_proof/README.md` |
| The teaser trailer engine, the production logo bumper | `trailer/` (`main.c`, `tk*.c`, `music.c`, `assemble.sh`), `trailer/assets/dippinn_logo.mp4` (also the games' boot video) |
| Licences of third-party data | `show/lib/licences/` |

### Not in the repository

| material | status | last known location | how to get it back |
|---|---|---|---|
| The rendered master `ep101_web_hd.mp4` (and its FFV1 per-shot masters and render cache) | **missing** | `show/build/ep101/` on the render machine of 2026-10-26 (gone) | Re-render from `b1528349` (section 3); compare with the manifest's hashes |
| `vault-emu.tar.xz` (stripped PCSX-Redux, SDL3, BIOS, licences; 5,949,892 B) and `vault-sdk.tar.xz` (PSn00bSDK + mipsel toolchain; 36,037,332 B) | **missing** (never pushed: `show/DEFERRED.md` says the vault location was left for the creator to choose) | `<SHOW_SCRATCH>/vault/` on the render machine | Rebuild them: `sh tools/dev_setup.sh apt psn00bsdk redux` with PCSX-Redux at commit **`c45db3b5372edb4c4479ea17130cf0e6c33cecb3`** (the master's emulator; today's `dev_setup.sh` pins a newer `ee9be68`), then `show vault pack`; check the hashes in `show/vault.lock` |
| TTS and ASR models (Kokoro ONNX + voices, Whisper small; about 1.2 GB) | **missing**, public | `<SHOW_SCRATCH>/theme/...` | Download the public packages named in `show/vault.lock` (`[models]`: kokoro-js voices, kokoro fp32 ONNX, whisper small) and check each SHA-256 there |
| Soundfonts (FluidR3 Mono, GeneralUser GS, MuseScore General Lite) | **missing**, public | the show library on the render machine | Download the public files whose licences are in `show/lib/licences/` |
| The creator's own voice recordings (for example the hummed melody `co12_hum`) | **not used in the master** (it used TTS for all 142 lines); any recordings live only with the creator | the creator's machine | Drop WAVs into `show/ep101/vo/` as `show/ep101/vo/README.md` says |
| A recorded theme-song master (vocals, stems) | **not found**; the main title in the master is engine-rendered | `<SHOW_SCRATCH>/theme/` (work files) | Use the bar maps and `docs/show/INTRO_REDO.md`; a new arrangement is planned there |

## 3. Reproduce the original master first

On an x86-64 Ubuntu 24.04 machine (a Claude Code cloud session works):

```sh
git clone https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark repo
cd repo                                                  # the branch tip has the setup script
sed -i 's/^REDUX_COMMIT=.*/REDUX_COMMIT=c45db3b5372edb4c4479ea17130cf0e6c33cecb3/' tools/dev_setup.sh  # the master's emulator, not today's pin
sh tools/dev_setup.sh apt psn00bsdk psxavenc redux       # host packages, PS1 SDK, FMV encoder, PCSX-Redux + BIOS
git checkout tools/dev_setup.sh
git worktree add ../master b15283495f964fb189680be5c0156f243b891032   # the master's exact source
cd ../master
cp ../repo/show/ep101/deliveries/web/manifest.json ../original_manifest.json   # the original's record (committed after b1528349)
# models and soundfonts: fetch the public files listed in show/vault.lock, place them under $SHOW_SCRATCH
export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b
show/tools/show doctor                 # checks tools and hashes against show/vault.lock
show/tools/show build                  # show.exe
show/tools/show compile ep101
show/tools/show render build/ep101/EP101.SHW --jobs 2     # hours: 244 shots captured in the emulator
show/tools/show deliver ep101 web      # the MP4, captions, manifest
```

Then compare with the original: the new `manifest.json` against `../original_manifest.json` (the committed
`show/ep101/deliveries/web/manifest.json` of commit `a26d05e7`) (render hash, per-segment keys, the per-frame MD5 spot checks, the
mix's SHA-256, loudness). A match means the reproduction is the original. Any difference: record it (which
shot, which frame, which tool hash differs) instead of substituting anything.

## 4. Where the pilot was going next (for the improver)

The direction already written down, which a new production should start from: `docs/show/ep101/REBUILD_PLAN.md`
(the planned rebuild of the pilot), `docs/show/INTRO_REDO.md` (a new 1:30 anime-style opening with the
huggle hook, one clean song form at 176 BPM), `docs/show/GAG_BIBLE.md` (comic timing rules in frames),
`pilot_proof/` (the look target: crayon storybook and live-action proof), `show/DEFERRED.md`. The show's
content rules are the game's (CLAUDE.md): PG-21 adult comedy, no sexual content, nothing suggestive with
minors, no harm shown to children, no real-world hate.

## 5. Access

To let another assistant read and build this, share the repository with it the normal way (for example
connect its GitHub integration to this repository, or give it a clone or the archive below). Do not put
passwords or tokens in files or archives.
