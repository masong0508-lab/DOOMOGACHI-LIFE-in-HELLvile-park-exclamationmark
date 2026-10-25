# show/DECISIONS.md - decisions taken without waiting for the creator

Every workflow appends here (one line or a short block each, newest last).
Each names the default taken and where to change it.

## ART: the cast (show/series/cast.shw, looks/*.shw, acting.shw), 2026-10-25

- **Look text format.** `look <id> base=roster:<N>|look:<id>|SPECIES_* ... end` with the roster
  text's keys (docs/ROSTER.md: `hair=`, `eyes=`, `hat=`, ...) and full engine names as values
  (`HAIR_POMPADOUR`, `PAINT_ROSE`, `SKIN_PEACH`, `CHEEK_BLUSH,CHEEK_DIMPLES`).  `char <id>
  look=...` in cast.shw; a line that starts with `key=value` continues the statement above
  (the spec's sec.9.1 excerpt).  Parser, validator and 48-byte packer:
  `show/tools/showtk/art/cast/lookdata.py` (`Bible().char_look_bytes(id)`).  Change: that module.
- **The cast starts from the game's own citizens.** Every named character with a roster record
  uses it (`base=roster:<N>`) and lists only what the show changes, so the show and the game
  draw the same people.  Built from parts: Sunny, Kevin, the storybook kids, Fluffy, the
  insert silhouettes.
- **Mayor: bow tie, no sash.** The pilot script pins a JUDGE rosette on his bow tie and has
  him clutch the clipboard to it, so the roster bow tie stays; the "festival sash" is not used.
- **Nana's apron = TOP_MAID** (the catalogue's dress with a white apron), peach, polka dots.
  Her roster heart aura is off in the show (it fought her R8 looks at bust size).
- **Pip's JUNIOR FESTIVAL PATROL sash = a tangerine cap and hi-vis bib** (no sash part in the
  catalogue); **Mr. Flops = ACC_TEDDY in white** (the only hugged-plush part; it reads as a
  bear: a one-floppy-ear flipbook is deferred).
- **Bonejangles bares his ribs** (TOP_NONE on a skeleton draws the ribcage) and wears a bow tie;
  possession is a state (`state=possessed` / RES_POSSESSED), never a separate look.
- **Kevin is a bald human with stubble** and `eye_count=3`: critter ears, hats and any hair line
  hide a forehead eye at bust size; a bare brow shows it at every size, the stubble reads 30s.
- **Dr. Bloom: lavender scrubs, EYE_DROOPY**; **Miss Lulla: EYE_SLEEPY**: their roster eyes
  vanished at bust size (into the ghoul's dark sockets / into white wool).
- **Mascotty = her roster bunnykin with a costume head**: EYE_WIDE, MOUTH_STITCHED, no brows or
  hair, a one-piece rose suit.
- **Rivet (MT-2 pop-in) wears a gas-mask respirator under a welding helmet**: her face is never
  shown before 108.
- **Fluffy = SPECIES_SLIME in white** (the round, legless body reads as a cloud); heart eyes
  are EXPR_LOVE.  The crayon look is the shot's PAL_CRAYON lerp, not other looks.
- **Lip flap look-dev.** Speakers' `mouth_size` raised 2-5 steps so the 8 visemes read at
  `px=2`; Miss Lulla's rest mouth is MOUTH_SMILE (her roster MOUTH_OPEN reads as mid-word).
  The sheets use the spec's sec.9.3 viseme defaults until AUDIO's visemes.shw lands.
- **Crowds are K-sets of real citizens** (`looks/crowds.shw`): adult sets hold no KID/TEEN
  citizen; the daycare kids are 11 roster kids who pass the kid-look check (Wobbles and Puff Jr
  wear MOUTH_LIPSTICK in the roster, Wafer a TOP_PRIEST: not cast).  Storybook kids are
  explicit looks (`looks/storybook.shw`), never random generation.
- **Kid looks are checked, not trusted** (`lookdata.kid_problems`): height <= 4, a kid's body,
  no facial hair, no adult-coded top/shoes/mouth/marking/accessory/aura/eyes.  An error, never
  a waiver.
- **Grumbleguts has no 101 image.** His cast entry carries `first=ep102`; the sheets and the
  lineup skip him.  The MT-5 shape behind the shut grille is a featureless set silhouette.
- **Model sheets render on the host** (`showtk.art.cast.sheet`: the game's portrait engine compiled
  natively with a patched copy of portrait_head.c carrying hunk (d)) until show.exe can draw
  them; they are review material, never master pixels.
- **A KID's model sheet never draws the possessed, ghost or dead states** (blank "never: KID"
  panels), and `lookdata` refuses `state=` other than normal on an age=child char.
- **register=crayon** marks the storybook cast and K-sets; the lineup previews them through the
  PAL_CRAYON lerp (camfx.c's formula); on the PS1 it is the shot's `look=crayon`.
- **Acting macros are picture-only** (`series/acting.shw`): no sound op in a macro; the shot adds
  the TIK, the creak, the whistle.  `headturn` in brads (64 = a quarter turn, the game's unit),
  `squash` in Q8 (256 = rest).  CLI: `show cast check|sheet|look|macro` (commands/cast.py).

## ENGINE (WP0 + WP1, 2026-10-25)

- **Patch hunk (a) also widens the sliced job's entry index** (`Job.e`, `int8_t` -> `int16_t` under `#if DOOMO_SHOW`). With `PC_BLOCKS=240` a cache entry index can reach 239, which an `int8_t` turns negative (`&ent[job.e]` would then write before the array). Spec §4.3 lists the `PC_FREE` collision but missed this one. The game (48 blocks) is unaffected.
- **New patch hunk (g): a phase that is due now always gets its pool blocks** (`job_start()` claims with `soft = 0` under `#if DOOMO_SHOW`). Found by AT1.3's override check: under one fixed clock every cached image was "drawn in the last two ticks", so the game's soft claim failed and the old blink phase stayed on screen (0/20 blink overrides took effect). Hunk (f) alone does not cover it. Safe on the PS1: `plat_image` copies the pixels into the GPU packet when it is called, so evicting an image drawn earlier in the frame cannot corrupt it. Rule for the player: draw each image right after its fetch.
- **Patches are applied by `show/engine/apply_patch.py`, not GNU patch**: no fuzz (a changed context line stops the build), offsets allowed, the copy is written only when every hunk applied, and the error names the patch, the game file and the hunk (AT1.4).
- **The (c) blink hunk normalises the override** (`p->blink = portrait_show_blink != 0`), so any non-zero value means shut; `show_portrait_overrides()` clamps the mouth to `0..MOUTH_COUNT-1` and (d) ignores anything else.
- **Every `show/engine/show_*.c` is compiled** (CMake glob), so WP2-WP8 add player files without editing the CMakeLists; `show_selftest.c` only with `-DSHOW_SELFTEST=ON`, `show_wrap.c` (P2, deferred) never.
- **`show_audio_stub.c` landed in WP1** (spec lists it under the player): spu.c's SFX upload called the weak `audio_sfx_pcm` stub at boot, so the harness reported "core weak stubs hit". Strong no-ops make the stub report empty from the first boot.
- **The 3D closure is compiled now** (map, maps, home, home_draw, home_edit, furn_* and the packed maps, as measured by the probe); `--gc-sections` drops it until a stage references `plat_view3d`/`plat_overworld`/`plat_room`, so the 2D build costs nothing.
- **`ps1-show-8mb` is the same build with `SHOW_RAM_KB=8192`** (a define the player can size buffers by). elf2exe (the game's, read-only) still enforces 2 MB; the emulator side of M0-T5 (the 8 MB setting) is tested with the harness fork (WP2/WP3).
- **The test chart** shows the 16:9 picture on palette bands, the title-safe box (16 px / 9 px), the Wall line at x = 160, the 256-colour palette, a random look as a bust (px=2) and chibi (px=2) and another as a figure (px=1), cycling the 11 expressions every 45 f, an 8 px ruler and the frame counter; matte bars are drawn last.
- **AT1.3 is measured on the PS1 by a self-test build pair**: the same `show_selftest.c` built with the patched copies (`-DPC_BLOCKS=240`, overrides at -1) and with the game's unpatched files (`-DSHOW_PORTRAIT_ORIGINAL=ON`); both print the CRC32 of 440 busts/figures and 80 chibis (20 roster looks, the roster linked only into these test builds) and the logs must be identical. The patched build also checks that a mouth/blink override changes the image and that -1 brings the original back (no stale cache entry).
- **WP1's emulator runs use the game's `ps1/harness/run.py`, invoked read-only** with `--map show/build/<preset>/show.map`; the show's own fork (unthrottled, Lua crop, mailbox) is WP3.
- **`check-game --full` builds from `git archive`** of the merge-base and of HEAD (committed files only, so a dirty work tree cannot fake a pass), targets `doomogachi` + `textbin` (not the disc), and caches each build's hashes by the tree ids of `core/`, `ps1/`, `tools/` in `show/build/check-game/`. The path half also fails on uncommitted changes outside `show/`.
- **The vault's SDK archive carries the cross toolchain**: `/opt/psn00bsdk` alone cannot build anything, so `vault-sdk` also holds the files of the mipsel gcc/binutils packages (`dpkg -L`) and the shared libraries `ld`, `as` and `cc1` load (libbfd/opcodes/ctf-mipsel, isl, mpc, mpfr, gmp, jansson, sframe, zstd).
- **Vault archives are reproducible** (members sorted, mtime 0, uid/gid 0, Python lzma preset 6, one thread) and split into `<name>.tar.xz.partNN` pieces of 95 MiB only when larger. `fetch` never overwrites a differing file without `--force` and never writes through the `/opt/ps1emu/pcsx-redux` symlink into the scratchpad.
- **No remote vault was created** (no repo, branch or release asset): where the archives go is the creator's decision (spec §10.7 lists the three routes). They are in `<scratch>/vault/` with `SHA256SUMS` and a copy of `vault.lock`.

## AUDIO WP10: music and SFX (showtk/music, showtk/sfx, series/music.shw, sfx.shw, ep101/cues.shw), 2026-10-25

- **Game cues play on the tempo grid, not the 30 Hz tick.** `mml_legacy.py` (over `tools/mmlc.py`) places every
  MML onset on the song's own grid (`timing=grid`: at t96 a bar is exactly 75 f / 120,000 smp). `timing=game`
  on a cue reproduces the sequencer's tick quantisation (onset tick = ceil(units*256/rate256)) for an A/B
  against `runner --wav`. The two differ by under one frame per note. Change: `timing=` in series/music.shw.
- **Chord instruments (@4/@5/... arpeggio insts) are voiced as chords** on pads, and as the game's per-tick
  arpeggio on `chip_*` presets (`arp=chord|arp` per track).
- **Unity.** Every cue in show_lib/music is rendered at -16 LUFS integrated, or lower when the true peak would
  pass -1 dBTP; `gain=` in cues.shw is relative to that. The theme master (-14 LUFS) comes in at -2 dB.
- **The Mayor's note is G5**, MINGLE phrase bar 4 beat 1 (the cadence "g f e g f2"): it is the first onset
  under the cold open's 29-f mute at the shot list's TC 00:00:29:00 (mingle_box bar 12.1). The music box's
  held note into the co12 match cut is A5 (the hum's first pitch); the hum stops on G; 4-08 gets one note
  further, to F5. Hums sound an octave down (A4 / G4 / F4) for the Mayor's tenor. Change: `note` lines in
  series/music.shw.
- **cues.shw carries timed `mx` ops** (`@<beat>+N mx <cue> <op>`) next to the spec's `spot` defaults, so the
  whole episode is spotted before the shots exist; STAGING copies each op into its shot unchanged (the cue
  sheet prints the shot-relative frame). A `<cue>:bar:B.b` time anchors on a playing cue's bar map.
- **The theme cues name files** (`src=file:doomtastic_times_main_title.flac` under docs/show/theme/ once
  published, else the scratchpad's theme/final): a new vocal master drops in with no other change; the bar
  map is re-imported from the master's own `*_full.mid` (tempo + section markers).
## AUDIO: voice (WP9) and the vendored kit (show/audio), 2026-10-25

- **show/audio/ is the kit and the theme's voice sources, text only (364 KB).** The only edits
  make model, sample and cache paths overridable (`KIT_ASSETS`, `KIT_CACHE`, `KOKORO_MODELS`,
  `KOKORO_VOICES`, `KOKORO_SRC`, `WHISPER_MODEL`); the kit cache moved out of the repo to
  `show/build/cache/kit`. Source hashes: `show/audio/README.md`.
- **The CMU dictionary data is not in git** (3.6 MB): spec §10.7 puts the cmudict wheel in
  `vault-models`. `showtk/voice/env.py` reads `$SHOW_LIB/g2p/cmudict.dict` and, if missing,
  extracts it from the pinned PyPI wheel (`cmudict==1.1.3`, sha256 `81917843…` checked); the GPL
  wrapper is never imported. Its BSD licence is in `show/lib/licences/`.
- **voice.lock is JSON** (`show-voice-lock/1`, one row per line, readable diffs). Placement
  contract: `say <line>` on frame F puts take sample `onset` (= 1440, the 30 ms pre-roll) on
  episode sample 1600·F; `mouth` letter i is on frame F + `mouth_from` (-1) + i. Full format:
  `show/tools/showtk/voice/README.md`. Change: `showtk/voice/lock.py`.
- **Takes live in the take store** (`$SHOW_LIB/takes/<voice>/<ep>/<id>_<kind>_<sha12>.flac`,
  default the scratchpad's `show_lib/`), 24-bit FLAC, keyed by the sha256 of the samples, so
  the vault can mirror the folder as is.
- **Every take is finished the same way**: 48 kHz mono, -23 LUFS (pyloudnorm), exactly 30 ms
  before the first sound (-40 dB re peak), a 60 ms tail with a 10 ms fade.
- **Kokoro's audio runs 65 ms ahead of its token grid** (measured here on bilabial closures and
  fricative bursts, and in the theme's singer.py): every token time is corrected by -1,560
  samples at 24 kHz before the mouth track is made. AT9.2 measures closures from the audio
  itself (the 1-6 kHz band dip), not from the token times.
- **Recordings**: drop folder `show/ep101/vo/<line_id>_take<n>.wav` (also `_t<n>`; scratch
  reads `_s<n>`), gitignored; `takes.toml` `fav = <n>` picks one; with no pick the **highest take
  number wins** (the last read is usually the keeper). A recording beats TTS with no edit.
  `docs/show/ep101/RECORDING_GUIDE.md` was not published yet; BREAKDOWN §2.4's naming is accepted too.
- **Casting** (CASTING.md in the scratchpad's `show_out/voices/`): option 1 of each role is the
  spec §10.3 pick, with pitch moved where the measured native pitch missed the voice bible's
  register: Mayor +3 st (not +2: 136 Hz reads "upper-middle"), Bonejangles **bm_lewis** (not
  bm_george: it sits at 139 Hz, and pitched down and flattened for the possessed ratings Whisper
  heard "Six" as "Ziggs" and "Ten" as "Zen"; bm_lewis, 89 Hz, was heard right every time), Kevin am_onyx +4 st (86 Hz is a basso;
  the bible says "middle"), Hattie bf_isabella -3 st, Lulla af_heart -3 st with a little breath,
  Pip af_sky +4 st / formant +2 st at speed 1.25 (1.30 blurred words). Kids are adult voices
  pitched up with the formants raised; the fog's child is af_kore, never Pip's af_sky.
- **Leading "..." is not spoken** (a timing note: "come in a little late, small"); Kokoro read
  it as a stray "a". **Stage parentheticals** in walla ("(laughter)") speak a `say=` text and
  skip ASR. **Possessed lines** carry `mode=possessed` (voices.shw variant `bonejangles.possessed`).
- **Clinker's twins** are rendered in her voice and then time-warped by WORLD word by word onto
  Bonejangles' locked take (same text, same rhythm); `late=R8` stays a placement field
  (`sync.late` = 12,800 smp) for the mixer.
- **"...Hee. Hee." (lib:mayor/heehee_v1)** is the Mayor's voice saying "hee", re-sung by WORLD
  on E4 then C4, 12 f each, the second 20 f after the first; synthetic until the creator records it
  (`show voice lib ingest take.wav mayor/heehee_v1`).
- **dx_proof lines** live in `show/tests/audio/dx_proof/` (lines.shw + voice.lock): they keep
  the ep101 ids where the script has the line (`co15_perfect`, `co18_stay`, `a1_09_nana_01`) and
  `dx_nana_two` for "Two of everything, dear."
