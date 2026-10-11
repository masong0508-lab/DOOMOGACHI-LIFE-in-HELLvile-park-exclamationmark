# DOOMOGACHI LIFE on the PlayStation

`ps1/` turns the shared C99 core into a real PlayStation disc: a PS-X EXE
built with PSn00bSDK, wrapped in a bootable, license-free `.bin/.cue`
(mkpsxiso). Everything in `core/include/doomo/platform.h` is implemented on
the console's own hardware: the GPU draws the 2D layer, the GTE runs a true
3D polygon engine for `plat_view3d()` and `plat_overworld()`, the SPU plays
ADPCM encoded by our own encoder, and saves go to the memory card through
the BIOS file API.

Contents: 1 Building - 2 Running - 3 Hardware usage and memory maps -
4 The 2D layer - 5 The 3D engine - 6 Overworld - 7 Third-person model -
8 Audio - 9 Memory card saves - 10 Test harness - 11 Engine-test build -
12 Performance - 13 Notes for the core.

---------------------------------------------------------------------------

## 1. Building

Requirements: PSn00bSDK (installed in `/opt/psn00bsdk`), a MIPS cross
compiler (`mipsel-linux-gnu-gcc`, the Debian/Ubuntu `gcc-mipsel-linux-gnu`
package, or `mipsel-none-elf-gcc`), CMake >= 3.21, Ninja, Python 3.

```sh
export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b
export PATH=$PATH:/opt/psn00bsdk/bin          # mkpsxiso, elf2x, ...
cd ps1
cmake --preset ps1                   # configure (build/)
cmake --build --preset ps1           # build/doomogachi.exe + doomogachi.bin/.cue
```

| preset           | output dir          | what                                              |
|------------------|---------------------|---------------------------------------------------|
| `ps1`            | `build/`            | the game, NTSC (release, `-O2`)                   |
| `ps1-enginetest` | `build-enginetest/` | boots into the 3D engine showcase (section 11)    |
| `ps1-pal`        | `build-pal/`        | the game with PAL video (50 Hz display, 30 Hz logic) |
| `ps1-elf`        | `build/`            | same as `ps1` with a `mipsel-none-elf` toolchain  |

Options (`-D...`): `DOOMO_ENGINE_TEST=ON`, `DOOMO_PAL=ON`,
`DOOMO_CORE=OFF` (platform bring-up without the core: the engine test with
built-in fallback art, `cmake --preset ps1 -B build-plat -DDOOMO_CORE=OFF
-DDOOMO_ENGINE_TEST=ON`).

What the build does (`ps1/CMakeLists.txt`):

* compiles `ps1/src/*.c` (strict: `-std=gnu99 -Werror -Wall -Wextra`) and
  **every** core source, mirroring `core/sources.mk`
  (`core/src/*.c core/gen/*.c`, sorted, re-globbed on every build so new
  core files are picked up), `-O2`;
* links the objects directly (never through `.a` archives) so the weak
  placeholders in `core/src/stubs.c` are replaced by strong definitions;
* `tools/elf2exe.py` writes the PS-X EXE (the SDK's `elf2x` mis-reads the
  `PT_GNU_STACK` header of the apt toolchain) and refuses images that leave
  less than 64 KB for the stack;
* `tools/check_isa.py` disassembles the ELF and fails the build on any
  instruction the R3000 does not have. The apt `libgcc.a` is built for
  MIPS32R2 (its 64-bit division uses `clz`, a Reserved Instruction
  exception on a PS1), so `src/libgcc_r3000.c` provides the 64-bit helpers;
* writes `doomogachi.map` (symbols, used by the harness);
* `mkpsxiso` builds `doomogachi.bin/.cue` from `iso.xml`: `SYSTEM.CNF`
  (`BOOT=cdrom:\DOOMO.EXE;1`, stack `801FFFF0`), `DOOMO.EXE`, `README.TXT`.
  No license sector data is included.

`cmake --build --preset ps1 --target harness` (or `--preset ps1-enginetest`)
runs the headless test suite (section 10).

## 2. Running

### PCSX-Redux

```sh
pcsx-redux -iso ps1/build/doomogachi.cue -run            # the disc
pcsx-redux -exe ps1/build/doomogachi.exe -run            # side-load the EXE
pcsx-redux -bios openbios.bin -iso ... -run              # without a Sony BIOS
```

Memory cards: `-memcard1 card1.mcd`. The PCSX-Redux TTY console shows the
game's log (`plat_log`, `perf:` lines). Note PCSX-Redux charges about two
cycles per instruction plus one per memory access, so CPU-bound code runs
roughly 1.5-2x slower than on a real console (section 12).

### DuckStation

Open `ps1/build/doomogachi.cue` with "Start Disc" (or run
`duckstation-qt ps1/build/doomogachi.cue`); "Start File" side-loads
`doomogachi.exe`. DuckStation needs a PlayStation BIOS image. Memory card type "Shared between all games" or "per
game" both work; the saves are `BASLUS-DOOMO0` / `BASLUS-DOOMO1`.

### Real hardware

* **CD-R:** burn `doomogachi.bin/.cue` (any burner that writes raw
  MODE2/2352 cue sheets). The disc has no license data and no wobble, so
  it needs a modchip, a softmod (FreePSXBoot memory card exploit, tonyhax
  International, Unirom) or a swap trick.
* **Serial / USB cable:** with Unirom or Caetla on the console,
  `nops /fast /exe ps1/build/doomogachi.exe /dev/ttyUSB0` (or PSXSerial).
  The TTY output appears on the same link.
* **ODE (XStation, PSIO):** copy the `.bin/.cue` to the SD card.

Video is NTSC 320x240 by default; the `ps1-pal` preset makes a PAL disc
(the same 320x240 picture centred in the 256-line PAL frame, game logic
still at 30 Hz with a 2-2-1 vblank cadence).

### Controls

The core's buttons (`core/include/doomo/input.h`) on a PlayStation pad:

| core     | PlayStation | | core   | PlayStation |
|----------|-------------|-|--------|-------------|
| A        | Cross       | | L      | L1          |
| B        | Circle      | | R      | R1          |
| X        | Square      | | L2     | L2          |
| Y        | Triangle    | | R2     | R2          |
| START    | START       | | SELECT | SELECT      |
| D-pad    | D-pad, or the left stick of a Dual Analog / DualShock |

Digital pads, Dual Analog and DualShock in either mode are read through the
BIOS pad driver (port 1). Engine-test controls: section 11.

## 3. Hardware usage and memory maps

| unit | used for |
|------|----------|
| R3000A CPU | the core (30 Hz logic), display lists, visibility, caches |
| GTE | every 3D vertex: rotation, perspective (RTPT/RTPS), depth for the ordering tables |
| GPU | 320x240 15-bit, double buffered, dithering on; TILE / SPRT / POLY_FT4 for 2D, POLY_GT4/GT3 for 3D |
| SPU | 4 tone voices (chiptune music), 20 sample voices (sound effects) |
| CD-ROM | boot only (the EXE is loaded once; no streaming) |
| memory card | saves, slot 1, BIOS `bu00:` file API |
| root counter 1 | hblank counter for CPU timing (`perf:` numbers) |

### VRAM (1024 x 512 halfwords)

```
x 0..319     y   0..239   framebuffer 0
x 0..319     y 240..479   framebuffer 1
x 0..255     y 480        master CLUT (doomo_palette, STP bit set, index 0 = transparent)
x 0..255     y 481..495   silhouette CLUTs (15, LRU: all opaque pixels -> one colour)
x 0..255     y 496..511   font CLUTs: 256 colours x 16 entries (4bpp)
x 256..319   y 480..511   misc CLUTs (shadow blob)
x 320..959   y   0..255   world textures: 5 8bpp texture pages x 16 slots of 64x64 (80 slots, LRU)
x 960..1023  y   0..191   sky: 256x96 8bpp in two 128x96 halves
x 960..991   y 192..255   font: 128 glyphs 8x8, 4bpp
x 992..999   y 192..223   soft shadow blob (32x32 4bpp)
x 320..1023  y 256..511   image cache: 5 256x256 8bpp pages + one 128x256 strip,
                          shelf packed, LRU per page
```

Colours: the core's 256-entry `doomo_palette` (15-bit BGR) is converted
once; every palette entry gets the STP bit so that colour 0x0000 (black)
is still opaque, and index 0 maps to 0x0000 (transparent) in the CLUTs.

### Main RAM (2 MB)

| range | what |
|-------|------|
| `0x80000000-0x8000FFFF` | BIOS / kernel |
| `0x80010000-...` | the EXE: code, read-only data (art, roster, maps), data |
| after it | BSS: two display-list buffers (2 x 103 KB prims + 2 ordering tables of 1536 entries per view), core state (`doomo_town` 107 KB, `doomo_map` 32 KB), caches |
| `...-0x801FFFF0` | stack (grows down) |

Everything - code, const data and BSS - is loaded into these 2 MB, and
`tools/elf2exe.py` refuses an image that leaves less than 64 KB for the
stack. The build prints the numbers (`elf2exe: ... ends 0x..., N KB
left below the stack top`). Three measures keep the discs inside:

* **Packed art** (`tools/packart.py`, `src/packart.c`, option
  `DOOMO_PACK_ART`, default ON): the core's 60 world-texture frames and 5
  sky frames (360 KB of 8bpp pixels that only this platform reads, to
  fill its VRAM caches) are linked LZ-compressed (80 KB), every frame on
  its own. `core/gen/art_data.c` is compiled without those two arrays and
  `doomo_art_tex_pix` / `doomo_art_sky_pix` are defined as virtual
  addresses (`--defsym`, outside RAM): the core's `textures.c` keeps
  computing frame pointers from them, `vram.c` / `r3d.c` map such a
  pointer back to its frame and decode it on a cache miss (a texture
  frame into a 4 KB buffer, a sky frame into the frame's display-list
  memory). After the first frames of a scene everything is in VRAM and
  the cost is zero; the engine tour's frames are identical to an unpacked
  build (only the HUD's CPU-time digits differ). Core code must never
  read through texture / sky pixel pointers (ARCHITECTURE.md, textures.h).
* **-Os for cold core code**: the core is compiled for size except the
  files that run every frame (`DOOMO_CORE_HOT`: the UI rasteriser, fixed
  point, map, actors, combat, pet drawing), which keep -O2 - about 120 KB
  less code at the same frame times (harness: UI, combat, workshop,
  exorcism unchanged; the pet scene needed its two files back at -O2).
* **The engine-test disc leaves the game out and vice versa**: the
  engine-test build compiles out the game loop and the harness's scene /
  save commands, so `--gc-sections` drops the game's scenes; the game
  disc does not link the engine test's fallback assets (`etest_assets.c`,
  only reachable when a core accessor returns NULL, which the full core
  never does).

Measured (all core modules up to the life / action / save-extension
work; the disc used to overflow):

| build | before | after |
|-------|--------|-------|
| game disc | ends `0x80226864`: 218 KB over the limit (64 KB stack reserve) | ends `0x801c2b50`: 181 KB under the limit (245 KB below the stack top) |
| engine-test disc | ends `0x8023f190`: 316 KB over the limit | ends `0x80135724`: 746 KB under the limit (810 KB below the stack top) |

The biggest remaining consumers of the game disc: art other than
textures / skies 247 KB (sprites 225 KB - the core reads sprite pixels,
so they stay unpacked), the display lists 206 KB + OTs 24 KB, roster
164 KB, `doomo_town` 107 KB, core code ~540 KB.

`plat_save_scratch()` is the idle half of the display-list memory: after
`DrawSync()` the prim buffer of the previous frame is unused until the
current frame is presented, and a save or load runs inside one frame.
`PRIM_BYTES` (103 KB) is sized for `DOOMO_SAVE_SCRATCH_MIN` (save.h: a
whole slot plus the codec's work area), so the PS1 never needs the
streaming save path; a frame itself has needed at most ~36 KB so far.
The core's static default buffer is dropped by `--gc-sections`.

### SPU RAM (512 KB)

```
0x00000-0x00fff  capture buffers (hardware)
0x01000-0x01fff  silent loop block + the chiptune waveforms (1.8 KB)
0x02000-0x7dfff  sound effects (ADPCM, bump allocated on first use)
0x7e000-0x7ffff  reverb work area (unused)
```

## 4. The 2D layer

Every frame is **one** linked GPU display list in painter's order
(`gfx.c`):

```
[uploads] -> [2D prims ...] -> [view clip][3D OT 1535..0][full clip] -> [2D prims ...] -> ...
```

* `plat_rect` -> `TILE`. The core draws its UI as thousands of small spans
  per frame, so `gfx2d.c` merges them while they are issued: a rect
  directly below or right of the previous one with the same colour and
  width/height extends that TILE in place, and a row of single pixels with
  stride 2 (the core's 50% checker seams and shadows) becomes ONE 50%
  semi-transparent TILE (a true blend, which is what the checker stands
  for). The title screen goes from 4,475 calls to 1,840 GPU primitives.
  The common case is a frameless leaf function (no stack frame, ~25
  instructions).
* `plat_shade_rect` -> semi-transparent TILEs: the GPU has no multiply
  blend, so the level is approximated with 0-3 halving passes
  (`B/2 + 0`) followed by one subtractive pass (`B - F`). This is the
  least-squares best fit of that family to the art colormap; the 8bpp
  colormap's palette-quantisation colour shifts are not reproduced
  (`platform.h`: "PS1: semi-transparent quad").
* `plat_line` -> clipped `LINE_F2` plus the end pixel.
* `plat_text` -> one `SPRT` per glyph from the 4bpp font page; each palette
  colour has its own 16-entry font CLUT, so text is exact and never
  dithered.
* `plat_image` -> the VRAM image cache (keyed by the image's `uid`, or its
  pixel pointer); images larger than 128x128 are split into 128-texel
  tiles. Scale 1 uses `SPRT`, scale 2-4 raw-textured `POLY_FT4` (nearest
  neighbour); `IMG_FLIPX` flips the texture coordinates; `IMG_SHADOW`
  draws the image first through an all-dark silhouette CLUT offset by one
  pixel; `IMG_SILHOUETTE` uses a silhouette CLUT of the requested colour.
* Uploads (cache misses) are `GP0(A0h)` packets in their own chain at the
  head of the frame, so they run strictly after the previous frame
  finished drawing and never overwrite a texture still in use.

### Frame pacing

`plat_begin_frame()` starts a new prim buffer; `plat_end_frame()` waits for
the GPU to finish the previous frame (`DrawSync`), waits for the 30 Hz slot
(`VSync(2)`, PAL 2-2-1), shows that finished frame and starts the GPU on
the new list. The CPU builds frame N+1 while the GPU draws frame N. Game
logic runs once per frame at 30 Hz; when a frame takes longer than two
vblanks the game slows down (no frame skipping), like the native runner.

## 5. The 3D engine (`plat_view3d`)

`view3d.c` turns the live `doomo_map` into real polygons every frame; the
GTE pipeline itself is `r3d.c`.

* **Camera.** Eye position, yaw and a real pitch rotation (the full
  3x3 matrix, no y-shearing). World space: x east, y south, z up, 64 units
  per cell. Vertices are sent to the GTE relative to the integer camera
  position so they fit its 16-bit inputs; the rotation is pre-scaled by
  ~7.9 (`R3_SCALE_Q12`) so the near plane can be ~12 world units in front
  of the eye while staying above the GTE's H/2 projection limit. The field
  of view follows `OPT_FOV` (narrow / normal / wide).
* **Visibility.** A fan of 2D rays (36 per 45 degrees, 18 cells deep) is
  cast through the grid from the eye; rays stop at walls and closed doors
  and continue over open-air blocks lower than the eye. Only the cells it
  reaches are drawn, so rooms behind walls cost nothing. The fan widens
  with the pitch so looking up/down never shows holes.
* **Geometry per visible cell.** Floor and ceiling quads at the cell's
  floor/ceiling height; on each edge the vertical faces that face into the
  cell: full walls of solid neighbours, step faces where a neighbour's
  floor is higher, upper faces where its ceiling is lower (lintels, doors),
  and facades / roof caps for solid blocks under open sky (the same rules
  as the reference raycaster: a block is as tall as its `ceil_h`, roofed
  with `floor_tex`). Door and lift cells read `map_floor_z()` /
  `map_ceil_z()`, so they move live. Textures are cached per texture id and
  animation frame; animated textures are separate cache entries.
* **Pipeline per quad.** `RTPT`/`RTPS` -> near/far/frustum rejection ->
  recursive 2x2 subdivision while the affine texture error would be
  visible (screen size x depth ratio), or while a vertex is outside the
  GPU's coordinate range -> near-plane clipping of the leaves that still
  cross it -> `POLY_GT4`/`GT3` into a 1536-entry ordering table. Subdivided
  quads get a "backing" polygon one depth bucket behind them that hides
  T-junction cracks between neighbours at different subdivision levels.
* **Light and fog.** Every grid corner has a smoothed light colour (the
  average of the open cells around it, tinted by lava, water and portals;
  `world.c` caches it per map and refreshes a few rows per frame). Walls
  get directional contrast and a contact shadow at the bottom. A fog table
  (quadratic in depth, rebuilt only when the view distance changes) fades
  vertex colours towards black from 4 cells away in dungeons and from 10
  cells away on outdoor maps, up to the 18-cell view distance.
* **Sky** is a cylinder of strips behind everything, the 256x96 sky texture
  wrapping once per 360 degrees and following the pitch.
* **Billboards** are textured quads at their feet position, depth sorted in
  the same ordering table as the walls (so they hide correctly behind
  pillars), lit by their cell, flipped / semi-transparent as requested,
  with soft subtractive blob shadows on the floor. `player_sprite` is drawn
  semi-transparent when the camera is closer than 40 units.
* **Tint** (damage, holy, vape...) is a full-viewport additive or
  subtractive TILE after the 3D, and `v->vx/vy/vw/vh` viewports clip the
  view anywhere on screen; 2D drawn after `plat_view3d()` lands on top.

## 6. Overworld (`plat_overworld`)

The same cells as the GBA's top-down map, as real 3D terrain seen by a
tilted (~58 degrees), smoothly following camera looking north: floor
quads at their heights, terrace and cliff faces down to lower neighbours,
solid cells extruded as blocks (houses with roofs, hedges, fences, trees;
height from `ceil_h` or a default by texture), and a sea of water tiles
around the island. Residents, the pet, props and the player are
camera-facing billboards with blob shadows. `daylight` (0..15) colours
every vertex (warm sunrise/sunset, blue night) while cells with light
>= 13 keep glowing at night (street lamps, windows); then the tint.

## 7. Third-person model

In third person (`CAM_THIRD_PERSON`) the player is not a billboard but a
low-poly chibi citizen built from `town_res(doomo_player.resident_id)->look`
(`model.c`): round head in the skin colour with eyes, glints, blush and
mouth; hair (with twin-tails), species ears, horns, halo; top, bottom and
shoes in their paint colours; tail and wings; a toy gun in the right hand
that kicks back with the weapon recoil. It runs (legs/arms swing, body
bob), turns to the aim angle, is lit by the sun and its cell, and shares
the level's ordering table. When no resident is set up yet (engine test),
it wears a premade roster citizen's look. If the camera is closer than 24
units it is not drawn (first-person-like close-ups).

## 8. Audio

* **Tones** (`plat_tone`, channels 0-3 -> SPU voices 0-3). At boot,
  single-cycle pulse 12.5/25/50 %, triangle, saw and a noise loop are
  synthesised and encoded to SPU ADPCM by our encoder (`src/adpcm.c`), as
  two cycles per wave with the loop starting at the second one (the first
  ADPCM block must use filter 0; the loop then replays blocks encoded with
  the real predictor history). A long (112-sample) and a short (28-sample)
  cycle per waveform keep the pitch register in range from bass to
  ~10 kHz. Pitch = `freq_hz x cycle x 4096 / 44100`; the voice is only
  re-keyed when the waveform changes, so notes glide without clicks.
* **Samples** (`plat_sample`, voices 4-23). The core's 8-bit PCM
  (`audio_sfx_pcm`) is converted to ADPCM (fast encoder mode) the first
  time an effect plays, or in the background one effect per frame after
  boot; a free voice (ENDX) or the oldest one is used.
* **Encoder quality** (`harness/adpcm_test.c`, host): pulse waves 22-23 dB
  SNR, triangle 44.7 dB, saw 17.8 dB, sine 40.7 dB, 8-bit chirp 24.9 dB,
  noise 19.7 dB.
* The engine-test build runs `audio_selftest()` at boot: SPU RAM read-back
  of the uploaded waveforms (DMA), a tone voice's envelope rising after
  key-on and falling after release, and a synthetic one-shot that plays and
  then ends on its own (ENDX). It prints `audio selftest: PASS`.

## 9. Memory card saves

`platform.h` slots map to one file each on the card in slot 1:
`bu00:BASLUS-DOOMO0` (story) and `bu00:BASLUS-DOOMO1` (free play), 1-12
blocks (8 KB each) depending on the save size.

```
frame 0      "SC", icon flag 0x13 (3 frames), block count,
             title in Shift-JIS ("DOOMOGACHI LIFE Story" / "Free Play"),
             16-colour icon CLUT
frames 1-3   16x16 4bpp icon: a pink Doomogachi egg whose critter blinks
             and grows horns (3-frame animation in the BIOS / emulator
             card manager)
frame 4      info frame: magic "DMPS", payload size, CRC32
frames 5..   the core's save stream, zero padded
```

* The BIOS cannot grow a file, so a save that needs more blocks than the
  existing file is written as a second file (`BASLUS-DOOMO<n>B`) first and
  only then is the old one erased: a failed write never destroys the old
  save (unless the card has no room for two copies; then the old file is
  replaced). Reads pick whichever copy is valid (magic + CRC).
* All transfers are whole, block-aligned 8 KB chunks (OpenBIOS
  miscomputes partial writes that do not start on a block boundary).
* Every error - no card, unformatted card, card full, I/O error - returns
  0 to the core, which shows its own message. A card inserted after boot is
  picked up by re-reading the directory once when a save fails.
* The BIOS card driver moves one 128-byte sector per vblank, about a
  second per block: a full 88 KB free-play save (12 blocks) takes ~12 s on
  a console; the core's save menus only peek at the 48-byte header, which
  reads a single block (partial reads are supported). While the BIOS blocks the game loop, a "Saving... do not remove
  MEMORY CARD" banner is drawn over the last frame (and removed again
  afterwards).
* Boot: the pad and card drivers are started before the SDK's interrupt
  handler is installed, and the directory scan (`_bu_init`, ~3 s for both
  slots) runs behind a pink "Checking MEMORY CARD" splash.

## 10. Test harness (`ps1/harness`)

`harness/run.py` boots the EXE (`--exe`) or the disc (`--cue`) headless in
PCSX-Redux (`-cli`, under `xvfb-run`, always with a wall-clock timeout) and
drives it with `harness/harness.lua`.

```sh
python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe \
    --script core/tests/scripts/ui_gallery.txt --out /tmp/shots
python3 ps1/harness/run.py --cue ps1/build/doomogachi.cue \
    --script ps1/harness/scripts/game_smoke.txt --out /tmp/smoke --expect "memcard: wrote"
cmake --build --preset ps1 --target harness             # the whole game suite
cmake --build --preset ps1-enginetest --target harness  # the engine suite
```

* **Same scripts as the native runner** (`core/build/runner --script`):
  `wait`, `press`, `hold`, `down`/`up`, `release`, `shot`, `scene`, `push`,
  `expect-scene`, `save`/`load`/`expect-save`, `type`, `log`, `no-stubs`;
  PS1 extensions `fps`/`fpsend NAME`, `vram NAME` (full VRAM dump), `dshot
  NAME` (the video output as displayed), `expect-log TEXT`.
* **Frame-exact lockstep.** The game exposes a small mailbox (`g_hcmd`,
  `src/hcmd.c`). The harness makes the game stop at the top of its loop
  (between two frames, before the pad is read) whenever it reaches the
  frame the script is at; there it changes pad buttons, captures the frame
  that was just drawn straight from VRAM, and runs `scene`/`save`/`type`...
  inside the game, exactly where the native runner runs them. Between those
  points the game runs at full speed, so frame rates stay measurable (time
  spent stopped is excluded). Shots therefore show the same game frame as
  the native runner's: comparing `core/tests/scripts/boot.txt` and
  `ui_gallery.txt` against the native PNGs gives 0.4-4 % differing pixels,
  all from the checker-seam blending and the shade approximation
  (section 4), apart from screens dimmed by a modal.
* **Checks:** hang detection (no new frame for 15 s while running; long
  memory card I/O counts as alive), crash patterns on the TTY, `--expect
  TEXT`, `--assert-no-stubs`, failing mailbox commands. Writes `tty.log`,
  PNG screenshots and `harness_results.txt`; prints the frame rates, the
  last `perf:` line and the core stubs that ran. Exit code 0 = pass.
* `harness/memcard_check.py card.mcd --icons icons.png --expect
  'BASLUS-DOOMO1*'` lists the card directory, decodes the Shift-JIS titles
  and our info frame, and renders the icon animation frames.
* `harness/sheet.py out.png a.png b.png ...` makes a contact sheet.
* `harness/adpcm_test.c` checks the ADPCM encoder on the host.

Scripts in `harness/scripts/`: `game_smoke.txt` (boot, title, launcher,
UI gallery, save + load on the card), `enginetest.txt` (the full engine
tour with screenshots and its frame rate), `thirdperson.txt`,
`overworld.txt`, `model.txt` (3D citizens), `memcard.txt` (card self test).

## 11. Engine-test build

`cmake --preset ps1-enginetest && cmake --build --preset ps1-enginetest`
boots straight into a deterministic walk-through that exercises the whole
3D engine, using the core's art (textures, sprites, sky) and the core's
maps when the action/story modules provide them (`MAPID_VAPE_CELLAR`,
`MAPID_CLOUD_NINE_SPIRE`, `MAPID_HAHALAND`; the TTY says which: at the
time of writing the core's map table is still the foundation stub, so the
built-in maps are used), else built-in showcase maps:
a hall with pillars, stairs to a balcony, a lintel doorway, an open-sky
courtyard with a pond and a flower terrace, a low corridor, a door, a lift,
a hell cavern with a lava pit, stepping stones and chains; then the 3D
overworld town through a day/night cycle; then the 3D citizen showcase
(the story cast from the roster, standing and jogging). First person and
third person, looking up and down. A HUD shows fps, CPU and 3D time,
polygons, cells and sprites; the same numbers go to the TTY (`perf:`).

Pad: any button switches to manual control. D-pad move/turn, L1/R1
strafe, L2/R2 look down/up, SELECT first/third person, START next scene,
TRIANGLE HUD on/off, SELECT+START memory card self test (write, read
back, grow, second slot, erase: `memcard selftest: PASS`).

## 12. Performance

Measured in PCSX-Redux (interpreter-accurate timing that is roughly
1.5-2x slower than a real console for CPU-bound code; GPU time is not the
limit anywhere). 30 fps is the cap (30 Hz logic).

| scene | fps | CPU / 3D per frame | load |
|-------|-----|--------------------|------|
| engine tour (FP + TP, core art), average | **29.9** | | |
| first person | 28-30 | CPU 11-21 ms, 3D 9-19 ms | 130-260 polys, 40-120 cells |
| third person (3D model) | 30 | 3D 13-22 ms | 240-470 polys |
| overworld, 296 cells | 29.5-30 | 3D 20-22 ms | 290-316 polys, 12-14 sprites |
| 3D citizen close-up | 24-30 | 3D 17-28 ms | 370-550 polys |
| boot animation (core 2D UI) | 30.2 (was 13.5) | CPU 11.8 ms (was 48.8 ms) | 885 `plat_rect` (was 4,459) |
| title (core 2D UI) | 30.2 (was 20.5) | | |
| UI gallery pages 1-8 | 30.2 (was 18.2-29.8) | CPU 7-11 ms (was 31-55 ms) | 530-800 `plat_rect` (was 2,300-4,500) |

The 2D screens used to be CPU bound in the core's own drawing code (per
pixel shape rasterising, thousands of tiny `plat_rect` calls); the perf
pass rewrote the UI kit as span rasterising (`core/src/ui_raster.c`,
pixel-identical output), see ARCHITECTURE.md. Other core scenes measured
with the core scripts: combat 25 fps (3D 27 ms), workshop 30, pet 23
(CPU 43 ms), exorcism 19-27 (CPU 33-58 ms): the last two are bound by
those modules' own drawing.

## 13. Notes for the core

* The 2D layer is fastest with few, large rectangles: one `plat_rect` per
  span costs ~4 us of platform CPU per call on top of the core's own work.
  Vertical stacks of same-width spans and stride-2 dot rows are merged
  automatically; anything else is one GPU primitive per call.
* `plat_shade_rect` is a blend approximation on the PS1 (see section 4).
* `save_game()` blocks for about a second per 8 KB block on a PS1 (BIOS);
  a "Saving..." banner is shown by the platform.
* `plat_save_scratch()` returns the idle display-list buffer (section 3):
  valid only until the end of the current frame, as `platform.h` allows.
* Texture and sky pixels are not CPU readable here (packed, section 3):
  pass `texture_pixels()` / `sky_pixels()` / `texture_image()` results to
  `plat_*` only; use `texture_avg()` / `sky_horizon()` for colours.
* RAM is shared by code, const data and state: prefer small tables,
  share buffers, and keep scene state small.
