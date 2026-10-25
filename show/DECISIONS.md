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

## ENGINE lane 1: player core and capture (WP2, WP3)

- **Plug-in API = weak-referenced module table.** `show_registry.c` names every planned module
  (`show_mod_cam`, `show_mod_overworld`, ...) as a weak reference; an unlinked one is NULL and skipped. Lanes
  plug in by adding their own `show_<name>.c` and never edit a shared file. Data references (not calls) to
  weak symbols link to 0 on the R3000 (the `jal` to a weak function is what fails, spec 15.1).
- **Records, not a fixed ShotChunk struct.** A chunk is a 32 B `ShwShotHdr` plus tagged records
  (`ShwRec{type, n, bytes}`), at most one per type; record types and show-op ranges are pre-allocated per
  lane in `shw_format.h`. The player skips unknown types, so packages add data without a format bump.
- **base= / replay= are resolved by the compiler**: it emits a self-contained chunk plus `preroll` (logic
  frames run undrawn before frame 0). The player only knows "pre-roll N frames", which is also what a seek is.
- **The shake is a camera offset only**: `-Wl,--wrap=plat_post_offset` turns camfx_apply's whole-frame E5
  offset into a no-op, so the frame edge and the matte bars never move (spec 2.1, 2.3); the stage camera
  (lane 2) adds `camfx_offset()` to the planes.
- **TINT** (a host kind) is a `plat_post_blend` of the tint's colour at level16, drawn just before
  `camfx_apply` (off-palette by design, like a fade). **IRIS** from a beat event is centred on
  `g_show_anchor_hook(actor)` when the actor module installs it, else the picture centre; `SOP_IRIS` takes an
  explicit centre.
- **One chunk buffer, synchronous CD read at shot prep.** The lockstep capture does not care how long prep
  takes; the second (prefetch) buffer is only for the deferred disc edition. The `needs 440 KB` line of the
  RAM report therefore over-counts by 160 KB.
- **Stats are microseconds** (`timer_us`), not raw root-counter cycles; they never reach a pixel or P1.
- **P1** = CRC32 (zlib polynomial) of a fixed 152 B core record (shot, frame, beat_now, 32 label ages,
  8 actor clocks, camfx read-backs, RNG, plane visibility) + show_ext's host-kind state + every module's
  `hash()`.
## ENGINE lane 3: deliver, review, standards (WP12, WP13, WP17), 2026-10-25

- **Segments are per beat**, video-only H.264, cached by a hash of their masters' frame hashes + recipe + burn-ins; audio is encoded once over the whole range (no AAC priming seams). Change: `showtk/edl.py`.
- **EDL in/out points sit 0.6 ms inside frame boundaries** with an explicit `duration` per entry (Matroska stores ms timestamps); frame-exact, tested on cut ranges.
- **BT.709 conversion is explicit** in the filter graph (swscale's default would be BT.601 under bt709 tags: measured max error 26 levels vs 3).
- **AT12.2 reading:** H.264 is lossy, so "framemd5 matches the masters x6" is checked on the deliver chain before the encoder (exact md5 per sampled frame) plus the decoded encode at art-pixel centres within mean error 3 and closer to its own master frame than to its neighbours.
- **Threads:** ffmpeg runs with 2 threads (`SHOW_FFMPEG_THREADS`); the machine is shared.
- **Chapters = the acts** (the eyecatches sit on the act breaks) unless the timeline gives `chapters`; folded to YouTube's >= 10 s rule.
- **The slur list ships as SHA-256 hashes and starts empty**: no slur is ever spelled in the repo; the pilot script is written clean and checklist item 5 is the human gate. The STD-HATE fixture uses an invented word.
- **The sign-off binds to the manifest's `render_hash`** (frame hashes, mix hash + window, captions, chapters, recipe), not to MP4 bytes, so a re-encode of the same render stays signed.
- **Thumbnail** = a master frame x4 (1280x720), default the middle of the longest shot in a scene with no child; `release.thumb` overrides and is refused in a child's scene.
- **--release needs the compiler's lint report** (`build/<ep>/lint.txt` or `lint.json`) with no errors.
## ART: the sets (show/series/sets/, show/art/gen/), 2026-10-25

- **One set per location, cheapest route first** (table in `show/series/sets/SETS.md`): the game's
  own `hahaland` overworld for MT-1/MT-3, its `vape_cellar` 3D map for every first-person beat
  (MT-5, 3-03, 3-05: the pastel back room and the tiled room under the candy-stripe canopy), its
  house (`HOME_COTTAGE`, living room, `HP_EMPTY`) for the tag; new generated plates for the rest.
  Change: the `set` blocks in `series/sets/*.shw`.
- **Time of day is a CLUT swap** (`show/art/gen/light.py`): dawn, sunset, dusk, night, dark, sepia,
  drained as 256-entry index remaps (family-preferring nearest, artgen's rule); night keeps the
  emissive indices lit. Our `night` is a little lighter than the game's `TINT_NIGHT` (which ships
  as `game_night`) so pastel reads at 320x180. Skies are BANDS lists per time of day.
- **The bandstand**: north half live (candy posts, the GIGGLE banner with the cupcake balloon over
  its third letter, lanterns), south half boarded against the Wall, the MINGLE crate in the hollow
  under the boarded deck (skirt boards are props: 2-03 tears them), the sealed gate in the Wall
  beside it. One 800x240 TILEMAP at figure scale with named framings (wide/north/steps/boards/
  gate/fair). The Wall is drawn face-on at the right (the side-scroller convention); the x = 160
  Wall line belongs to the crayon co09 frame and to `wall_split`.
- **The cold open's red** is `PAINT_CHERRY` (candy red), never the BLOOD ramp (D16; a test checks
  the drip). The pink drip (A/B for the animatic) is the drip prop's `pink` state.
- **The crayon/pixel banner match**: both banners put their six letters at x = 42 + 40k, 35x42,
  baseline show y 104 (co11 -> co12). GIGGLE is wet pink over MINGLE, which shows through.
- **Grain density 4 %**, not 12: at 320x180 a 12 % fleck tile read as snow over the night sky;
  the spec's "12 %" was an overlay strength. Change: `paper_grain(density=)` in storybook.shw.
- **Signs are blank boards with a text box** (`text=x,y,w,h` on the prop); every readable word is a
  string (STAGING's strings.shw). Baked lettering only where it is paint or print: the banner,
  the act cards' handwriting (the brief's own lines), the TV-MA card, the town sign's painted-over
  1000, the calendar's print, the logo (drawn the way `fp_title.c` draws it).
- **Previews are host composites** (`show sets sheet`), never master pixels; the player draws the
  masters. PIC-PAL is checked on every composed frame of every variant (100 %).
- ART review (art director): Mr. Flops is a prop bunny (gen:mr_flops, one floppy ear) in Pip's paws, not ACC_TEDDY (a bear); launch_hill redrawn to the script (Wall down the frame's centre line, park below, iron launch stand prop, mark wall_x 160); joke_graveyard fills its sky with a yew/tree line (4-07 board note: sky out of frame).

## STAGING co_mt (cold open, main title, eyecatches), 2026-10-25
- co_mt: files are `ep101/shots/co.shw` (cold open, owned by co_mt from now on), `shots/co_mt.shw` (main title) and `shots/ec1.shw` (EC1, included after a1; a1 stages 9,600 f without it). EC2 and EC3 were already staged by a2 (a2_56-57) and a3 (a3s38-39) when co_mt landed, so episode.shw does NOT include co_mt's `ec2.shw`/`ec3.shw` (same beat ids `e101_ec2_*`/`e101_ec3_*`, would double 600 f). They stay as the co_mt versions (Mascotty prop, flash level, bell timing); the integrator includes exactly one version of each pair.
- co_mt: the main title is cut to the master's bar map (150 bpm, 48 f/bar, 30 bars + 60-f ring = 1,500 f), not the beat sheet's provisional TCs: MT-1 bars 1-9 (432 f), MT-2 verse 10-13 (192), MT-3 wall window + pre-chorus to "Come out!" (108), MT-4 the stop-time drop 16.2-16.4 (36), MT-5 chorus 17-24 (384), MT-6 post-chorus + button to the last chord (264), MT-7 ring/mute/beep/title (84). Total 1,500, so 1-01 is not stretched.
- co_mt: the mirrored knock lands on the lyric's three "Knock, knock, knock!" syllables (bar 15.1, +6, 15.2), not "two snare hits": the master's pre-chorus is P4 alone.
- co_mt: MT-5 rots a replay of the mt01 crane (no child), never the cast pop-in (Pip is in it).
- co_mt: beat rows (`act`/`beat`) live at the top of each shot file, since episode.shw is a plain include list.
- co_mt: timed prop states are written as `+N show <prop>[:<state>]` / `hide <prop>` (proposed SOP_PROP_STATE); fallback is base= + one change per state.

## WP4 compiler: the .SHW, and the ep101 compile report (2026-10-25, lane 1)
- The .SHW is the PLAYER's format (shw_format.h + shw_format.py from the player package, record based). The compiler's earlier fixed-section format was dropped before anyone used it; showc now resolves into an in-memory IR (`showtk/shw_ir.py`) and `emit.py` writes the player's records. README "Compiler outputs" lists what goes in each record; SHWR_ACTOR/TRACK/CROWD payloads and the actor/crowd show-op numbers are the compiler's PROPOSAL for lane 2's actor modules (add-only from now on, or change both sides in one push).
- Props and prop states are IMAGE planes (one per state, hidden unless the variant's `state=` picks it), so `+T show <prop>[:<state>]`, `hide <prop>`, `prop.set <prop> state=` and `change <prop>.state=` (STAGING's proposed SOP_PROP_STATE) compile to `SOP_PLANE_SHOW/HIDE` with no new op. An untimed `show`/`hide` is the frame-0 state.
- Lane 2 data: camera, fx setup, wipes, text and lane-2 events go in the PIC1 blob (record SHWR_PRIV_L2) built with lane 2's own packers (`showtk.picture`); TILEMAP/STROKES planes go in SHWR_PLANE with lane 2's payloads (a TILEMAP over a plane's 64 KB `bytes` field is drawn as an IMAGE plate, warning PLAN). The player binds the blob with `show_l2_bind(show_chunk_find(SHWR_PRIV_L2, ...))`.
- Op-range conflict for the player/lane-2 owners: shw_format.h gives lane 3 the ops 0x100-0x17F and lane 2 0x040-0x0DF, but show_l2.h numbers lane 2's ops 0x100.. (SOP_IRIS 0x100, SOP_BOIL 0x101). The compiler sends lane-2 ops only inside the PIC1 EVTS section, never in the beat stream, so nothing collides today; CAM3D is proposed at 0x140.
- `base=`/`replay=` shots inherit the source shot's `actor`/`crowd` setup lines (same slots); a source cycle is a STAGE error. `preroll` is 0: the base shot starts from its own setup, not from the source's state at +N (DEFERRED).
- Plates over a 160 KB chunk are dropped to a 1x1 image with warning PLAN-CHUNK (none in ep101 today); the texture-only second chunk is deferred.
- `show compile ep101` (16-18 s warm, 31 s cold bake): 211 shots, 40,566 f; `show slug ep101` = 39,600 f exactly. ~1,060 errors, all in content owned by other lanes, reported here for them:
  - AUDIO: 260 AUD-LIB unknown sounds and 71 AUD-MX unknown cues (series/sfx.shw, music.shw not complete yet); 103 LINE unknown line ids (show/ep101/lines.shw not on the branch yet); 7 AUD-WALLA.
  - STAGING: ~137 ACTOR "no actor X" where shots address set props as actors (co07 hand_hold, co20 paw, mt02 logo*, mt05 hh/hv...): use `show`/`hide`/`prop.set` for props, or declare an `actor`; ~96 SHW-OPS unknown ops: `hud`, `mon`, `lens`, `door` (lane 3's grammar is not in the compiler yet), and a few timed setup statements (`change`, `actor`, `look`, `set`, `prop`, `plane`); 56 STAGE `at=` forms the compiler does not read (`at=x0,y0..x1,y1`, `at=mark:...`); 13 FMT-RUNTIME beats whose shots exceed the rundown's `len=` (co_mt's main title follows the master's bar map; the rundown keeps the beat sheet).
- WP6: crowd image keys include the flip bit (VRAM caches flips apart); AT6.4 checks 0 portrait-pool rerenders every frame and 0 VRAM uploads after warm-up (t>=24), allowing deferred first uploads.
- WP6: until lane 1's plug-in API lands, WP6 is driven by show/tests/engine/wp6 (self-test hook build); the frame protocol is documented at the top of show/include/show/show_actor.h.
- WP10.5 (AUDIO): ep101/cues.shw spots all 60 beats (99 mx ops). Where the beat sheet gives a rule, not a frame, the time is a marked `choice:`. Level ladder vs unity: theme 0 dB, stings -3..-6, underscore -6..-10, diegetic -8..-14, beds -16. The cute HOOK in 3-01 is restarted every 8 bars (384 f) on the 150 grid; insert_doom is one 61-bar render with bars 26-50 muted for 3-04 (re-entry bar 51.1 = 3-05 frame 0). Theme is still from scratchpad theme/final (docs/show/theme/ not on origin yet); swapping is a file drop.
- lane 2 (picture): text/strokes/camera/fx shot data travels as one PIC1 blob per shot (show_l2_bind) until lane 1's chunk API lands; ops 0x0100-0x01FF are lane 2's.
- WP6 plug-in: show_mod_cast resets with gore on (TV-MA; DOOM-style gore on adults/demons only, per the brief); flipbooks ride a private lane-2 record 0x160 (SHWR_PROP stays for props).

## CONTINUITY (ep101 integration), 2026-10-25
- continuity D1: one design for all three eyecatch pairs = co_mt's ec1/ec2/ec3.shw (Mascotty `gen:mascotty_tiny(h=8)` by the banner pole, same depth). a2 drops a2_56-57 + `s_ec2out_bubble`, a3 drops a3s38-39 + `s_ec3out`/`s_ec3in`; then episode.shw includes ec2.shw/ec3.shw and strings/co_mt.shw. Until then the a2/a3 versions stay (no doubled frames).
- continuity D2: every lantern stays dark from 2-01 to the end of Act Four (script 3-01 "dark since nine"); the rite candles and the 4-08 firework are the light. ART's `bandstand:torn/rite/night` need the lanterns-out states.
- continuity: strings files not included by their own shots file are included from episode.shw ahead of the shots (a3, a4 now; co_mt after D1).
- ENGINE lane 1 (WP3): the emulator boots with `-fastboot` (skips the BIOS shell; frames unchanged, AT3.3 hash identical). AT2.1 "1.2 s unthrottled" is measured as the emulator's CPU time to the player's lockstep handshake (load-independent on the shared box); vsyncs and wall are recorded beside it. One boot captures <= 10,000 frames (`scheduler.MAX_BOOT_FRAMES`) because Redux's Vsync listener overflows the Lua stack at ~15,600 frames.
- ENGINE lane 1: `ram_report.py` no longer double-counts the player. The WP1 gate demanded 320 KB (two chunk buffers) + 120 KB (player) free beyond the image when nothing was linked; now the player and its 160 KB chunk buffer are in .bss (no heap) and prefetch is deferred, so the gate keeps a 128 KB margin for modules still to land. With lane 3's hosts linked: 686 KB image + 889 KB bss, 410 KB left.

## ART motion pass, 2026-10-25
- ART motion: every set animates through `amb` statements (SETS.md sec.6), pure integer math with a C twin (show/art/engine/amb_math.h) so ENGINE ports it line for line; proposed record SHWR_AMB = 36. No hand-keyed frames.
- ART motion: character motion = the engine's own figure render cut into parts (hat/ears, paws, feet) and moved (show/art/gen/motion.py); 229 flipbooks in series/acting.shw (`cycles` + `flip`), macros enter_walk/exit_run/take_big/react_small/laugh_full/hit_react/... A one-shot pose hands back to its `next` (idle) by itself.
- ART motion: fun_fair's ferris wheel is 4 drawings (4.88 of 5 set-pool pages); pcycle is never used on a plane whose ramp also paints big areas (it flashed fountain_faces and lulla_tent: replaced by jets and waves).
- ART motion: "make him a turkey too" (creator) -> gen:turkey(kind=roast|live|walk), flips turkey_roast/turkey_live/turkey_strut; "him" was not named, so the roast is a prop any cast member can hold (OVER) or Nana's table can carry.
- **Mayor v2 = a turkey Mad-Hatter (creator, Oct 25).** Drawn by a show-only part set (show/art/parts/show_parts.c), called from portrait.c patch hunks (h)/(i) for looks with the turkey signature (OWLKIN+BEAK+TOP_HAT+FEATHER_FAN+BOWTIE+FHAIR_WIZARD; the game's randomizer never makes it, tested). Default = option 1 (lavender hat, peach head, teal coat, magenta bow tie, card "18"); options 2/3 in show_out/art/mayor_v2/mayor_v2_variants.png. No wings, so the dx3 white box on his bust is gone. The fighter view is not built (the mayor's kinds exclude it).
- ART mayor final (Oct 25, creator: "1 with no paper number on the hat"): Mayor Giggs is LOCKED to v2 variant 1 (lavender hat, peach head, teal coat, magenta bow tie). The card/paper and its "18" were removed from the hat band in every image kind (bust, figure, chibi, all expressions); the grape band stays. horn_color is no longer used by the mayor part set; the variant alternates 2/3 were dropped from showtk/art/cast/mayor_v2.py, which now also writes mayor_final.png.
- **ENGINE lane 1 integration (M0, Oct 25): the compiler writes lane 2's WP6 records.** `emit.py` wrote its own 16/36-byte ACTOR/CROWD proposals while `show_wp6_mod.c` reads `ShowActorRec` (32 B) / `ShowCrowdRec` (48 B) / the TRAK blob; co03's crowds drew as a full-screen smear. emit now builds them through `showtk.actors.ShotCast` (looks from the cast, `looks=a_1..a_6` ranges resolved through the look bible, mouth tracks from `mouth_raw`), and the timed actor ops become one `SOP_ACTOR_BASE` row `[actor, SHOP_*, v0..v3]` each (move, walk, squash, face, blink, state, show/hide). `SOP_PROP`/`SOP_FX` are no longer emitted at 0x0C0 (that is the crowd op; nothing reads them). `pose` (no FLIP records from the compiler yet) and `eyes` are dropped with no op.
- **Slug cards (M0-T3)** come from `showc`'s rundown again with the segment's BANDS sky and boiling grain (as `slug.py`'s); a slug shot's 8-byte .SHW id is its beat code (`co_1`, `1_12`), timeline.json keeps `slug:<beat>`.
- **`show render` writes `render_index.json`** (timeline id -> cache dir) beside its outputs; `edl.Masters` reads it beside the timeline, in its `render/`, and in `build/<ep>/`, so preview/deliver find masters of a slug or `--out` compile. Render work dirs are per .SHW path and process and are removed after a good render (two agents rendering at once no longer share a disc image or FIFO).
- **M0-T5:** the 8 MB setting works on the installed PCSX-Redux (`PCSX.settings.emulator['8Megs']` + reset); kept as a hatch, not used.
