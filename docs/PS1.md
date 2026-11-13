# BOREagotchi LIFE! on the PlayStation

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
* **Boot FMV** (`DOOMO_INTRO_FMV`, default ON; `src/fmv.c`): `psxavenc`
  encodes `trailer/assets/dippinn_logo.mp4` into `INTRO.STR` (mixed-mode
  `.STR`: MDEC v2 video + 37.8 kHz stereo XA, 15 fps, 2x) and the disc plays
  it before the first game frame (START / Cross skip it; no `INTRO.STR`, or
  no frame for 3 s, boots the game as before). Without `psxavenc` on `PATH`
  the intro is left out with a warning (CI: `-DDOOMO_INTRO_REQUIRED=ON`
  fails instead; `-DDOOMO_INTRO_STR=file` uses a ready-made one). Build
  `psxavenc` like CI does (`.github/actions/setup-psxavenc`: meson + the
  libav*/libsw* dev packages, commit `82f3871`).
  - The encode is `-s 320x176 -I`, centred at y 32 with black bars. Do
    not drop `-I` or go back to a 4:3 `-s`: that psxavenc scales the frames
    to the SOURCE's aspect (16:9 at `-s 320x240` -> 320x192) but encodes
    them as the `-s` size, so every frame comes out as garbage (the picture
    squashed into the top 96 rows over two green bands; ffmpeg decodes the
    same garbage on the host, so it is the stream, not the player). A new
    source with another aspect needs `-s` = its fit to 320x240 rounded to
    16 pixels.
  - Checking it: `ffmpeg -f psxstr -i X.str` decodes a `-t strcd` encode
    on the host; in PCSX-Redux a Lua `GPU::Vsync` listener that calls
    `PCSX.GPU.takeScreenShot()` every 4 vsyncs captures the ident (the
    harness's lockstep only starts at the game loop, after the intro).
    Fixed (fut-fmv): mean PSNR against the source 39.8 dB (was 9.5 dB).
  - Stream limits in `fmv.c`: at most 10 sectors (20,160 B) of bitstream
    and 0x3000 words of MDEC codes per frame, frames at most 320x240; the
    current encode peaks at 9 sectors / 13,820 B and 8,832 words.

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
| L3 / R3  | L3 / R3 (the stick clicks; analog mode only) | | | |

Digital pads, Dual Analog and DualShock in either mode are read through the
BIOS pad driver (port 1). Engine-test controls: section 11.

**Sticks (ST1).** No ANALOG button press is needed: when a pad shows up
(boot, or plugged in / swapped later) and reads as digital, `src/pad.c`
sends it the DualShock config sequence over SIO0 itself, between two BIOS
polls with interrupts off (43h enter config, 44h 01 03 analog + lock, 43h
exit; three tries at most, then a plain digital pad is left alone). The
pad comes up red-LED analog with the ANALOG button locked. The sticks then
drive, with no setup:

| where | left stick | right stick |
|---|---|---|
| shooter / levels (Options > Controls **Auto**, the default) | walk + strafe (analog speed) | turn + look (eased, invert and look speed honoured); R3 = lock on (as L2) |
| overworld / island maps | walk any direction, creep at a small push, walk, run at the rim | - |
| house (build, live, photo) | the D-pad as before | a flick turns the view one 90-degree step (as L1 / R1), held it repeats |
| creator | the D-pad as before | spins the citizen (8 views; a full push one view every 3 ticks): the turntable opens as a peek and closes a second after the stick is let go |

Options > Controls is Classic / **Auto** / Dual stick / Modern. Auto plays
Dual stick while an analog pad is plugged in and Modern otherwise, live
(pull the pad mid-level and the buttons take over); an explicit Classic or
Modern pick wins. Saves from before ST1 stored the old default Modern as 1,
which now reads as Auto (that is the migration; an explicit Modern is stored
as 3). The prompts and the shooter's control card follow the scheme in
effect ("L stick Walk", "R stick Look"); Dual stick picked on a digital pad
says "Analog sticks: use a DualShock". The harness feeds the sticks with
`stick LX LY RX RY` / `stick off` (`harness/run.py` writes `g_pad_force`, `src/pad.c`; the
native runner has the same line); `run.py --analog-pad` makes port 1 a
DualShock that powers up digital, and `scripts/sticks_auto.txt` checks that
the game switches it (`pad: analog` on the TTY) with the card still saving.
PS1 resident cost: +1.6 KB (65,952 B below the stack top with the FMV).

## 3. Hardware usage and memory maps

| unit | used for |
|------|----------|
| R3000A CPU | the core (30 Hz logic), display lists, visibility, caches |
| GTE | every 3D vertex: rotation, perspective (RTPT/RTPS), depth for the ordering tables |
| GPU | 320x240 15-bit, double buffered, dithering on; TILE / SPRT / POLY_FT4 for 2D, POLY_GT4/GT3 for 3D |
| SPU | 4 tone voices (chiptune music), 20 sample voices (sound effects) |
| CD-ROM | the EXE, scene overlays (`OVL.BIN`), text / SFX / pack files; the boot FMV streams `INTRO.STR` (MDEC + XA) once |
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
x 320..1023  y 256..511   image cache: 5 256x256 8bpp pages + one 128x160 strip,
                          shelf packed, LRU per page
x 960..991   y 416..511   dilated font for plat_text_outline: 128 glyphs 7x11 in
                          8x12 cells, 4bpp (the bottom of the strip)
```

Colours: the core's 256-entry `doomo_palette` (15-bit BGR) is converted
once; every palette entry gets the STP bit so that colour 0x0000 (black)
is still opaque, and index 0 maps to 0x0000 (transparent) in the CLUTs.

### Main RAM (2 MB)

| range | what |
|-------|------|
| `0x80000000-0x8000FFFF` | BIOS / kernel |
| `0x80010000-...` | the EXE: resident code, read-only data (art, roster, packed textures / sprites / map cells), data |
| after it | BSS: the two display lists (2 x 56 KB prims + 2 OTs of 1536 entries per view), core state (`doomo_town` 107 KB, `doomo_map` 32 KB), caches |
| after the BSS (game disc) | the **scene overlay slot** (116 KB): the code + const data of the scene that runs, read from `OVL.BIN`; what a group leaves after it is that scene's scratch (`plat_scene_scratch`, SP3: the island's recorded rect lists, 20 KB, `ovl_plan.py` SCRATCH) |
| `...-0x801FFFF0` | stack (grows down) |

Everything - code, const data and BSS - is loaded into these 2 MB, and
`tools/elf2exe.py` refuses an image that leaves less than 64 KB for the
stack. The build prints the numbers (`elf2exe: ... ends 0x..., N KB
left below the stack top`). With every module of the game landed the
disc no longer fitted (`0x802ec0b4`, 1.1 MB over the limit), so:

* **Scene overlays** (game disc only; `tools/ovl_plan.py`,
  `tools/ovl_pack.py`, `src/overlay.c`, option `DOOMO_OVERLAYS`). Most of
  the game's code only runs while its own scene is on screen. The build
  compiles everything into an object library first; `ovl_plan.py` then
  reads the relocations of all objects (`-ffunction-sections
  -fdata-sections`) and decides, section by section, what may leave RAM:
  - candidates are the `.text.*` / `.rodata.*` sections of the scene
    modules (15 groups: combat + levels, concert, creator, dream,
    exorcism, fishing + cooking, free play screens, island, library,
    minigames, pet, story overworld + cutscenes, studio, workshop, UI
    gallery); `.data` / `.bss` always stay resident;
  - a section is RESIDENT if resident code or data can reach it, if its
    address escapes into memory from code (a callback that resident code
    may call later), or if two groups reach it. The only edges not
    followed are the four function pointers of a `const DoomoScene`
    (`SCENE_*`): the core's `scene.c` calls those through
    `plat_scene_code()` (`scene.h`), which loads the scene's group first;
    and (S4) the edges out of a group-local registry pointer
    (`LOCAL_REGISTRIES`: `doomo_story_mg_a` / `_b`, small data and so
    resident), which only its own group's code may read: the planner
    checks every reader and keeps the cut only if all of them are in the
    group and none is resident (the S4 story minigames: 11 KB out of
    resident RAM into the `storymg` group);
  - (G0) the callbacks a scene hands to the resident modals (`ui_confirm`,
    `ui_text_entry`, `ui_choice`, `ui_say_done`; `MODAL_CALLBACKS`, each
    entry reviewed against its call sites) do not escape: the modals call
    them through `scene_callback_begin()` / `scene_callback_end()`
    (scene.h), which load the group of the scene the modal was opened
    over. The same holds for the callbacks the studio keeps in its own
    state (`st_popup`, `st_pick_citizen`). Callbacks given to `fp_pick()`
    stay resident (the picker is a freeplay scene, so the caller's group
    is not in the slot when it calls back), and so do the combat group's
    (combat is the biggest group). This took 60 KB of the studio, concert,
    free play, creator, exorcism, workshop and gallery code out of
    resident RAM;
  - (2026-11-13, "No 3D models in houses" below) a platform file can
    belong to a group (`PLAT_GROUPS`: `model.c`, the third-person chibi, in
    the combat group). Its resident caller is listed in `GUARDED` and asks
    `ovl_code_in()` (`overlay.c`) before every call; that call is not
    followed either. The plan fails if anything else would call such a
    file (`tools/test_ovl_guard.py`);
  - it writes the linker script (every group is an output section of one
    `OVERLAY` at the slot, `NOCROSSREFS`) and the scene -> group table.

  `ovl_pack.py` writes the groups' bytes to `OVL.BIN` on the disc
  (`elf2exe.py` leaves them out of the EXE; their RAM slot counts). At
  boot `overlay_init()` initialises the CD and reads its header; a scene
  of another group costs one CD read (~0.3 s for 70 KB at 2x; the studio
  group, 74 KB since G0, loads in 241 ms). When a
  scene switches scenes from inside its own update (enter / leave run
  inside the call), the caller's group is read back before the call
  returns, so the caller's code is always there: a switch from one
  overlaid scene to another costs three reads. The scene's code is the
  only thing that may live in the slot: anything a resident module keeps
  a pointer to across scenes must be in a resident module. The harness's
  `--exe` runs need the disc too (`--exe ... --cue ...`).
* **Packed art** (`tools/packart.py`, `src/packart.c`, option
  `DOOMO_PACK_ART`, default ON): the core's 60 world-texture frames, 5
  sky frames and 194 sprite frames (585 KB of 8bpp pixels that only this
  platform reads, to fill its VRAM caches) are linked LZ-compressed
  (195 KB), every frame on its own. `core/gen/art_data.c` is compiled
  without those arrays and `doomo_art_tex_pix` / `doomo_art_sky_pix` /
  `doomo_art_spr_pix_v` are defined as virtual addresses (`--defsym`,
  outside RAM): the core keeps computing frame pointers from them,
  `vram.c` / `r3d.c` map such a pointer (interior pointers of cropped
  sprite images included) back to its frame and decode it on a cache
  miss (a texture or sprite frame into a small buffer, a sky frame into
  the frame's display-list memory). After the first frames of a scene
  everything is in VRAM and the cost is zero. Core code must never read
  through texture / sky / sprite pixel pointers (ARCHITECTURE.md,
  textures.h, sprites.h).
* **Packed map cells** (`tools/packmaps.py`): the 12 maps' cell grids
  (266 KB) are linked LZ-compressed (14 KB); `map_def_cells()` (map.h,
  overridden in `src/packart.c`) decodes a map straight into the live map
  when `map_load()` runs - nothing else in the core reads
  `DoomoMapDef.cells`.
* **-Os for cold core code**: the core is compiled for size except the
  files that run every frame (`DOOMO_CORE_HOT`: the UI rasteriser, fixed
  point, map, actors, combat, pet drawing), which keep -O2.
* **Smaller display lists**: 56 KB per frame instead of 103 KB (frames
  have needed at most ~40 KB; a packed sky's first upload needs 52 KB at
  the start of a 3D view). Both lists together are the save scratch
  (section 9), so saves still never stream on the PS1.
* **The engine-test disc leaves the game out and vice versa**: the
  engine-test build compiles out the game loop and the harness's scene /
  save commands, so `--gc-sections` drops the game's scenes (no overlays
  needed there); the game disc does not link the engine test's fallback
  assets (`etest_assets.c`).

Measured (all core modules, fishing / cooking and Hell Breach included):

| build | before this pass | after |
|-------|------------------|-------|
| game disc | ends `0x802ec0b4`: 1.1 MB over the limit | EXE 1,224 KB + BSS 511 KB + overlay slot 108 KB, ends `0x801dd310`: **139 KB below the stack top** (75 KB more than the 64 KB stack reserve); `OVL.BIN` 778 KB in 15 groups |
| engine-test disc | ends `0x80178b1c`, 541 KB below the stack top | ends `0x801088d0`, 989 KB below the stack top |

**G0 memory pass (task #79; landed as H0, 2026-11-01).** The story,
house, side-mission, music and Spanish-text work had grown resident RAM
until the game disc no longer linked: at `959acc8` it ended at
`0x801f08c0`, **63,280 B below the stack top, 2,240 B into `elf2exe.py`'s
64 KB stack reserve**, with a 126,976 B slot. G0 (paused as
`docs/wip/g0_m2_wip.patch` on 2026-10-25, applied unchanged by H0, the
patch then deleted):

| step | below the stack top | how |
|---|---:|---|
| `959acc8` | 63,280 B (does not link) | slot 126,976 B, set by the house group's end (it loads 45,572 B lower, over BORROW); resident text + rodata 1,128 KB |
| modal callbacks stay in their groups | +60,336 B | `MODAL_CALLBACKS` + `scene_callback_begin/end` (above): resident text + rodata 1,128 -> 1,068 KB |
| the house borrows the card's block buffer | +8,192 B | `memcard.c`'s 8 KB `s_blk` joins BORROW (`memcard.c` calls `ovl_borrow_take()` before each card read / write): the house loads 53,764 B lower and ends 8 KB lower, the slot is the combat / cutscene / story groups' 118,784 B |
| **after G0** | **131,808 B (128.7 KB)**, ends `0x801dfd10` | EXE 1,190 KB + BSS 549 KB + slot 116 KB; 66,288 B above the 64 KB stack reserve; `OVL.BIN` 1,002 KB packed (1,278 KB) in 19 groups + 4 house parts |

**The creator borrows too (CRB, CR7/CR9).** The creator's packed part icons (`core/gen/part_icons_rle.c`,
58 KB) and the part catalog (`core/gen/part_meta_data.c`, 14 KB) are generated files, and
`ovl_plan.py`'s `group_of()` kept every `core/gen` object resident: the disc ended 62,000 B over the
stack reserve. They now join the creator group (like `furn_data` joins the house), and the creator is
a second `BORROW_GROUPS` member: it loads at `__ovl_borrow` over the same borrowed `.bss` (the world's
tables, `view3d`'s lists, the combat bot's search, the card block buffer), so the 150 KB group needs
99,828 B of slot and the slot stays the house's. Nothing of the creator draws a 3D view, syncs the
world or does card I/O; a scene under it that does rebuilds what it overwrote (`ovl_borrow_take`).
Measured on `7729ada`: `_end` 0x801ed1d0, 11,824 B above the reserve (CL6's tree before it: 12,240 B).
Landed on `5bb55e7`, whose own disc ends at `0x801edfa0` (73,824 B below the stack top, 96 B above
the 72 KB plan gate), CRB first cost 832 B of resident code and data, which put the disc 736 B under
the gate. Four changes take that back to 80 B, all with the same picture: the creator's panel keys
are only worked out where there are panels (`UI_PANELS`; the PS1 linked `ui_panel_key` /
`ui_key_int` for nothing, 372 B); `CR_CALM` is a constant 0 on the PS1 (no `cr_calm` byte, no glint
hold); the action rows' help lines are named arrays in the creator's overlay (a switch table's
literals went to the resident `.rodata.str1.4`, 156 B); and `ui_draw.c` keeps its one-hole backdrop
code on the PS1, where `ui_backdrop_holes()` uses the first hole only (the GPU fills the rest for
next to nothing): `_end` 0x801edff0, 73,744 B below the stack top, 16 B above the gate. CL4 then
freed resident RAM; on `696ffa6` (its own disc ends at `0x801ec8e0`, 79,648 B below the stack top)
CRB ends the disc at `0x801ec930`: **79,568 B below the stack top** (80 B of resident cost),
14,032 B above the 64 KB reserve and 5,840 B above the 72 KB plan gate; the creator group needs
100,040 B of slot.

| overlay group (bytes) | `959acc8` | G0 | | overlay group (bytes) | `959acc8` | G0 |
|---|---:|---:|---|---|---:|---:|
| cutscene | 118,444 | 118,444 | | island | 56,212 | 56,644 |
| story | 117,992 | 118,172 | | creator | 51,972 | 55,696 |
| combat | 117,760 | 117,760 | | dream | 39,956 | 39,956 |
| home (from `__ovl_borrow`) + biggest part | 159,968 + 11,984 | same, 8 KB lower | | storymg | 37,860 | 37,884 |
| fishcook | 93,052 | 93,052 | | exorcism | 31,852 | 34,820 |
| minigame | 88,136 | 88,136 | | pet | 23,876 | 24,020 |
| freeplay | 74,340 | 80,256 | | workshop | 15,012 | 15,468 |
| library | 71,432 | 71,432 | | cutx | 14,024 | 14,024 |
| studio | 45,524 | 76,648 | | gallery | 5,096 | 5,352 |
| concert | 56,336 | 71,860 | | **slot** | **126,976** | **118,784** |

Checked on the G0 disc (PCSX-Redux harness): `game_smoke`, the core's
`boot` / `ui_gallery` / `saves`, `home.txt` (parity and fps gates) and,
for the modal callbacks of each moved group, the core scripts
`fp_screens`, `studio`, `life_exorcism`, `action_workshop` and `creator`
(no `ovl: ERROR` in any TTY log); the engine-test disc's `memcard.txt`
self test; and, for the borrowed `s_blk`, a save and a load with the
house group in RAM (scene HOME, harness `save 0` / `load 0`, back into
the house: the group is evicted, read again and draws correctly). Only
the engine-test disc runs `memcard.txt`, where overlays are off, so a
save-from-the-house step in `home.txt` is still to add.

The slot is now exactly the three biggest groups (cutscene 118,444,
story 118,172, combat 117,760 B, rounded up to whole sectors): any growth
of them moves the slot again, 2 KB at a time (story SPEC C4). The house
group must stay under it too: from `__ovl_borrow`, its common code plus
its biggest part rounded up to whole sectors (`mart`, 11,984 -> 12,288 B)
may end at most at the slot's end. H6 (furniture animation frames and the
haunt eye, gingham crate and defend props: 69 prims, +552 B; the
`FF_MIRROR` decals below) would have ended it 376 B past the slot (slot
120,832 B, 129,760 B free); `room_ref_sprite()` now compiles to `return 0`
on the PS1 (it links no iso sprites, SPEC C3: 1,088 B of dead decoder), so
the common code ends 308 B below that limit and the disc ends at
`0x801dfd00`, **131,824 B below the stack top** with the 118,784 B slot. C1
(the show canon: `portrait_turkey.c`, which is resident because every
scene draws portraits, plus the animatronic's parts in `portrait_body.c` /
`portrait_head.c`) adds 15,264 B of resident code and data (`resident
text+rodata` 1,068 -> 1,083 KB; the turkey file is 11,960 B at `-Os`
after its coordinate helpers went out of line, from 17,060 B inlined):
the disc ends at `0x801e38a0`, **116,560 B below the stack top** (51,024 B
above the 64 KB reserve; measured on `6714f43`). Rebased on L1
(`f010771`) the disc ends at `0x801e3fe0`, **114,704 B below the stack
top** (-15,264 B against L1's 129,968 B; 49,168 B above the 64 KB
reserve, still above the 72 KB plan gate). That leaves L1's "six more
resident language tables" estimate at about 67 KB, under the 72 KB gate,
so L6 is now required before even the first L10-* language adds a
`ui_main.tsv`. The slot and every group's size are unchanged
(the prologue's crayon turkey adds 88 B to the cutscene group).
H4 (the themed door / window fittings: 19 pieces, 274 prims, the Walls
panel's skin rows and previews, the designed Towers 7B and Pits flat)
added 7,096 B to the common code, which would have moved the slot four
sectors (+8 KB, 123,632 B free). Instead the Build-mode operations moved
out of `home_edit.c` into `home_build.c`, a fifth part (`build`, 8,656 B;
`PLAT_PART_HOME_BUILD`, asked for by `home_draw.c` `want_build()` when
the owner opens Walls, Themes, Blueprints or a group): the common code is
158,868 B (from 159,964), the house ends 1,404 B under the slot (from
308) and the resident cost is the overlay directory's fifth part entry
(16 B; measured on `6714f43`: the disc then ended at `0x801dfd10`,
131,808 B below the stack top). The story
group is far above its 75 KB cap (C4); the levers are content-level
(story_script.c's legacy glue, story_talk.c), parts for SCENE_QUESTLOG /
SCENE_CREDITS (PARTS work only for BORROW groups today) and, for the
cutscene group (cap 100 KB), the Feast's 77 KB (story_feast.c +
grumbleguts_gfx.c) as a part of its own.

**L1 (the N-language lookup, `doomo/lang.h`).** The PS1 links no full
UI table of any language (`core/gen/lang_tab_<id>.c`, about 110 KB each
once a language is complete; `CMakeLists.txt` leaves them out): it links
each language's resident table `core/gen/lang_res_<id>.c` instead, the
strings of `core/lang/<code>/ui_main.tsv` of at most 48 bytes, and
`src/lang_ps1.c` hands those to `lang_tr()` (`plat_lang_table()`).
Spanish: 362 strings in 7,848 B (the old `lang_ui_es.c` was 7,768 B; LNG2
tables under 64 KB keep 16-bit string offsets). `lang.c` keeps 528 B of
state and cache, and only text in the resident `.text` / `.rodata`
(`[__text_start, __CTOR_LIST__)`) is cached by address: strings in the
overlay slot are looked up each time, so a group load never leaves a
stale entry. The disc ends at `0x801e0420`, **130,000 B below the stack
top** (-1,824 B against `6714f43`; slot unchanged; `0x801e0440`, 129,968 B,
once rebased on `883cf3b`). That is under the 128 KiB (131,072 B) the G0
pass reached, though the 64 KB `elf2exe` reserve and the 72 KB plan gate
still pass. Every further language with a `ui_main.tsv` adds its resident
table (about 8 KB) until L6 reads the tables from the disc per overlay
group: six more would leave about 82 KB, close to the 72 KB gate, so **L6
must land before the L10-* packages add `ui_main.tsv` rows**.

**L2a (one text path).** `src/gfx2d.c`'s `plat_text` and
`plat_text_outline` no longer call `lang_tr()`: they draw the bytes they
are given, like the GBA's, and the core translates each string once in
its text layer (`doomo/lang.h` "LOOKUP"). Every outline glyph pass and
every run of a hint used to be looked up again here.

HART (roofs and redrawn furniture, measured against `3706744` built the
same way): the house's common code grew from 156 to 164 KB (+263
furniture prims, 2,104 B of `doomo_furn_prims`; the roof's polygons in
`room.c`, -Os, about 1.9 KB with `draw_roof`; `room_roof_setup()` 1.7 KB;
the 2D roof rows are dead code here and dropped by the linker), so the
house now sets the slot's end: the disc ends at `0x801e2c40` instead of
`0x801e0440`, **116 KB below the stack top** (126 KB before; 10,240 B
less, still 52 KB above the 64 KB stack reserve and 44 KB above the
72 KB plan gate). No VRAM: the roof is
flat-shaded polygons (no texture page, no CLUT row). Home fps (harness
`home.txt`, PASS): plain 30.17, steady 29.88, orbit 27.38 (28.46),
cellar_full 28.80 (30.00), defend_stress 19.89 (20.75), modal 29.83,
defend_w3 30.13, visit 30.17; parity live / backdrop 0 px. With L1's
estimate of about 8 KB per further resident language table, six more
would now leave about 72 KB, right at the plan gate: L6 (tables read
from the disc per group) matters even more after HART.

G7 (retained panels, `platform.h` `plat_panel_begin`): the PS1 keeps the
weak default (every panel drawn every frame, `ui.h` `UI_PANELS` 0, so no
panel keys are built and the pictures are unchanged); the restructured
dialogue drawing, `ui_dialog.c`'s page-length cache and the small
`ui.c` helpers add 1,040 B of resident code: measured against
`17d385b` built the same way, the disc ends at `0x801e6c00` instead of
`0x801e67f0`, **103,424 B below the stack top** (104,464 B before; still
39 KB above the 64 KB reserve and 31 KB above the 72 KB plan gate). No
overlay group changes size.

H8 (lamp pools, glow prims, night panes: `room.c` decodes a prim's light
class from its `mat`, `furn_prim_mat()`; the house group's `room_ref.c`
gains the shared light code): the house's common code still rounds to
166 KB but the slot grows one sector, so the disc ends at `0x801e7400`
instead of G7's `0x801e6c00`, **101,376 B below the stack top** (-2,048 B
against `1e8a97b`, the same on H5's `17d385b`; 35 KB above the 64 KB
reserve, 27 KB above the 72 KB plan gate). Home fps (harness `home.txt`, PASS, parity live / backdrop 0 px):
plain 30.17, steady 29.75, orbit 27.91, cellar_full 28.35, defend_stress
19.51, modal 29.83, defend_w3 30.13, visit 30.17.

LS0 (saved life stage, both-direction romance gate): the gate, the
romance sweep, the version-1 save migration and the save codec's stage
field are resident (sim, sim_social, resident, town, save); the creator's
Age row is in the free-play group. Measured against `226179f` built the
same way, the disc ends at `0x801e7c60` instead of `0x801e7400`
(re-checked after the rebase onto `d642736`: still `0x801e7c60`),
**99,232 B below the stack top** (-2,144 B; 34 KB above the 64 KB
reserve, 25 KB above the 72 KB plan gate; the LIFESIM plan's "< 1 KB"
target for LS0 was missed by 1.1 KB, all of it child-safety code: sweep
0.5 KB, gate + stage helpers 0.6 KB, migration 0.3 KB, codec 0.2 KB).
The overlay slot keeps its size (166 KB, now from `0x801c7c60`).

K0 (the shared registry, LIFESIM_PLAN R1-R7 and section 6): resident
+544 B, all of it resident by design (no overlay group owns these files):
the six placeholder scenes (`weak_scenes.c`: 6 `const DoomoScene` + names
+ the two tiny entry points, about 450 B of text and rodata), the five
weak NULL save-chunk pointers in `.sdata` (20 B) and `save_budget.c`'s
snapshot growing with the registry (7 -> 12 slots: `.bss` +24 B). The
`plat_ext.h` defaults are discarded by `--gc-sections` until something
calls them. Measured against LS0's tree (`a29e375`) built the same way,
the disc ends at `0x801e7e80` instead of `0x801e7c60`, **98,688 B below
the stack top** (-544 B; 32 KB above the 64 KB reserve, 24 KB above the
72 KB plan gate and 2,432 B above the 94 KB combined K0 gate); the slot
keeps its size (166 KB, it only moves up by the same 544 B) and `OVL.BIN`
keeps its 24 groups (1,302 KB unpacked; 948 -> 950 KB packed, the groups
relinked at the moved slot). On `d642736` (before LS0) the same K0 cost
528 B. This is 288 B above the plan's 256 B estimate for K0 (the
placeholder scenes' code and names).

LS1 (news as 8-byte records, the `LIFE` chunk): the feed's `.bss` falls
from 2,050 B to 388 + 128 B (`sim_s` 7,084 -> 5,032 B; `news_s`, the two
rotating buffers), but the formatter (756 B at -Os), the record codec and
`life.c` add 2,272 B of resident code and 776 B of `.rodata` (the
template table), all resident (sim). Measured against the wave-1 tree
(K0 + LS0 + E5 on `d642736`) built the same way, the disc ends at
`0x801e8420` instead of `0x801e7e80`, **97,248 B below the stack top**
(98,688 B on the wave-1 tree; -1,440 B; 31 KB above the 64 KB reserve,
23 KB above the 72 KB plan gate). The plan's "resident delta at most 0 B" is missed by 1.4 KB: it
assumed story lines kept as text ids (no free-text pool) and did not
count the code; the LS area's combined allowance (+8 KB code) holds. The
harness (`game_smoke` memory card write + `memcard_check`, the core's
`boot`, `ui_gallery` and `saves` scripts) passes.
E0 (the endings / content pack / VM contracts, LIFESIM_PLAN E0): resident
+256 B, all of it in resident files by design (`scriptvm.c`, `txt.c`,
`weak_endings.c`): `.bss` +64 B (`txt.c`'s table of the 16 user text
packs) and +196 B of text and rodata (the JMAT rule, `txt_user_set`, the raw-string
branch in `txt_get`, and the weak ending / pack / `opt_mature` /
`mature_adult_ok` defaults that something references). The `TXF_RAW`
test costs one header load and a branch per `txt_get` call, never per
decoded symbol, so the Huffman hot loop is unchanged. Measured against
`d81b95e` built the same way, the disc ends at `0x801e7fa0` instead of
`0x801e7ea0`, **98,400 B below the stack top**
(-256 B; 33 KB above the 64 KB reserve, 26 KB above the 72 KB plan gate,
the plan's E0 budget is +1.5 KB); `OVL.BIN` keeps its 24 groups and the
slot its size (166 KB).
PA0 (the play-as contract, `playas.h`, and the save header's byte 47):
resident +32 B (`save.c` writes `playas_profile_flags()` and reads
`DoomoSaveInfo.flags`, the weak default is linked; the other weak
`playas_*` defaults are discarded by `--gc-sections` until called).
Measured against `16b6929` built the same way, the disc ends at
`0x801e7ea0` instead of `0x801e7e80`, **98,656 B below the stack top**
(within the plan's 64 B); the slot keeps its 166 KB (now from
`0x801c7ea0`) and `OVL.BIN` its 24 groups (950 KB packed).
SP1 (speed package, 2026-11-08; section 12 "Speed package SP1"): resident +0 B as landed. The first build cost
+640 B (`combat_bb_sort`, resident with `combat_build_view`, and the news keywords' strings, made resident by a
table of their addresses); on `fc5f87f` the disc had only 112 B above the 72 KB plan gate, so the PS1 keeps the
old `fx_dist()` billboard sort and the keywords are fixed-width rows: the EXE ends at `0x801edf80` with and without
SP1 (73,840 B below the stack top).

CL4 (first-time tip cards, the Field Guide, the Tips option): `tips.c` is
resident by design (a card shows over any scene and is drawn from
`doomo_frame`): its code (~2.0 KB: `tips_draw` 944 B, the input, show and
drop paths) and the 13 cards' text (~2.7 KB of string literals, the
`tip_text` table 156 B), 5,096 B; with `options_store.c` (the Tips byte
and the seen word in the record and the 'OPT2' chunk, `opt_config_flush`),
`game.c`, the scene and activity tables and the lobby's Field Guide entry
the feature costs about 5.9 KB resident, and on `5bb55e7` (73,824 B below
the stack top, 96 B above the 72 KB plan gate) that put the disc 1.4 KB
under the gate. The Field Guide (`lib_help.c`, ~10 KB of code and text)
is in the library overlay group: its list callback is in `ovl_plan.py`'s
`LOCAL_CALLBACKS` and its scene name is a named array, so nothing resident
points into its string section. CL4 pays for itself with the same trick in
four more scenes: a resident `SCENE_*` struct whose name is a literal keeps
its file's whole generic string section (every literal of the file's
static tables) in resident RAM, so `SCENE_OPTIONS` (3,412 B),
`SCENE_ISLAND` (1,736 B), `SCENE_LIB_BESTIARY` (4,552 B) and
`SCENE_LIB_ITEMS` (2,168 B) now take named arrays and those strings load
with their overlay groups (freeplay, island, library; none of them sets the
slot, the house does). Also: the Options page's "Show all tips again" words
are named arrays (as `opt_draw` literals they shared a section with the
resident `pause_menu_draw`), `cb_tips_reset` is a `MODAL_CALLBACKS` entry,
and the Field Guide's lobby book keeps its words out of `lib_common.c`'s
resident string section. Not done that way: `SCENE_HOME`'s strings
(888 B) would grow the house group, which sets the slot, and the slot grows
in 2 KB steps. Measured against `5bb55e7` built the same way, the disc ends
at `0x801ec8e0` instead of `0x801edfa0`, **79,648 B below the stack top**
(73,824 B before: CL4 is 5,824 B under the base; 15,712 B above the 64 KB
reserve, 5,920 B above the 72 KB plan gate); `OVL.BIN` keeps its 24 groups
(968 -> 984 KB packed, 1,335 -> 1,358 KB unpacked) and the slot its size
(167 KB; house 130,144 B). The seen word is in the machine config record
(`BASLUS-DOOMOC`, bytes 11..15 of the 48 B record): no new card block, and
it is written with the next save (`save_game` -> `opt_config_flush`), not
when a card closes (a card write takes about a second). The harness (in
lockstep) reports `PLAT_TEST_NO_TIPS`, so its scripts and journeys see no
card; the harness target also plays `core/tests/scripts/library_help.txt`
(the Field Guide's contents and every page).

CL3 (the shared prompt bar, the pet screen's bar, workshop / studio /
house legends): measured on `696ffa6` (after CL4) built the same way,
79,648 B left below the stack top before and 75,952 B with CL3 (-3,696
B; 74.2 KB, 2,224 B above the 72 KB plan gate, 10,416 B above the 64 KB
reserve). First built, CL3 took 11 KB and did not link (2,752 B into the
reserve, on `5bb55e7`); three changes, the same pixels: `ui_width_raw`
is not inlined (with the bar's new callers GCC -O2 inlined the width loop
into the toasts, titles, buttons and tabs: `ui.c` 25,272 -> 32,148 B, now
25,704 B); the bar's text builder is in `ui_prompt.c`, compiled -Os (it
runs only when a bar changes: 2,384 B instead of 3,084 B at -O2); and the
house's bar is const tables (`home_draw.c` -328 B in the home group, the
biggest: its slot demand 131,168 -> 130,596 B, against 130,144 B before
CL3, fits the same whole sectors again, so the slot does not grow). The rest of CL3's resident cost: `input.c`'s pad names (+524
B), `fp_options.c`'s per-pad help rows (+816 B), the `unlock_find` memo
(+340 B), the hub badge, the pet's per-tick geometry. The harness: 19 of 19
`run.py` PASS, the pet's bar with the PS1's names (L1 / R1, Square /
Triangle).

**No 3D models in houses (PS1-FLAT, owner 2026-11-12, `OWNER_DECISIONS.md`
"HOUSE CHARACTERS FLAT").** The house (`room.c`) draws the player as a
billboard like everyone else (section 7), so `model.c` is called by
`plat_view3d()` alone, and only for combat's third-person views. It moved
out of resident RAM into the combat overlay group: `ovl_plan.py`
`PLAT_GROUPS` puts the file there, and `GUARDED` names the one resident
call (`view3d.c`), which asks `ovl_code_in(OVL_CODE_MODEL)` first and
draws the player's billboard if the group were ever not in the slot. The
hooks the house had in the 3D code are gone (`g_model_hold`, the
per-polygon drawing area `g_r3_area` in `r3d.c` and `prims.c`).

| | base `ce2d7e8` | PS1-FLAT |
|---|---:|---:|
| EXE image (code, data) | 1,287,360 B | 1,282,852 B (-4,508) |
| `.bss` | 547,232 B | 547,212 B (-20) |
| `_end` | `0x801efe60` | `0x801eecb0` |
| **below the stack top** (boot FMV linked) | **65,952 B** | **70,480 B (+4,528)** |
| combat group (slot demand) | 124,056 B | 128,188 B (`model.c` 4,132 B) |
| home group (slot demand) | 130,856 B | 129,420 B (-1,436) |
| slot | 131,072 B (home) | 131,072 B (story, 129,980 B) |

70,480 B is still 3,248 B under the 72 KB plan gate: PS1-RAM closes that.
Not moved: `prims.c` (4,044 B, the chibi's boxes and blobs, -O2). With it
the combat group would need 132,232 B and the slot grows in 2 KB steps
(+2,048 B here, +4,096 B on PS1-RAM's tree, where the combat group is
126,696 B before this). When the combat group has 4.1 KB to spare under
the slot, add `prims` to `PLAT_GROUPS` (one word). After this package the
combat group has 2,884 B left under the slot (about 200 B on PS1-RAM's
tree): growth of `combat*.c` / `actor.c` / `levels*.c` beyond that moves
the slot.

The biggest remaining consumers of RAM: resident core code (portraits,
UI, sim, story engine, ...) ~650 KB, merged string literals of resident
code ~110 KB, `doomo_town` 107 KB, the display lists 112 KB + OTs 24 KB,
the roster 164 KB, packed art 195 KB, SFX PCM 47 KB (customisation P3
moves it to SPU RAM), the overlay slot 116 KB.

`plat_save_scratch()` is the display-list memory of both frames (one
contiguous 112 KB block): after `DrawSync()` the previous frame's list is
unused until the current frame is presented, and the current frame's list
is empty while the core runs its update (where saves and loads happen;
one call, inside one frame). If something was already drawn this frame,
`plat_save_scratch()` draws that much right away (queued VRAM uploads are
not lost) and starts the list over. 112 KB holds `DOOMO_SAVE_SCRATCH_MIN`
(save.h: a whole slot plus the codec's work area), so the PS1 never needs
the streaming save path. The core's static default buffer is dropped by
`--gc-sections`.

**Creator parts (CRA, 2026-11-08).** The part catalog's names and meta are linked only where the
creator calls them (none of it today: 0 B resident); the age table and the age gate are resident
(~700 B). The part-script VM is built with `DOOMO_PARTS_VM=0` on the PS1: with the painters' slot
hooks calling it, the VM (5.5 KB of code), the 2-slot block LRU and the 4 KB per-render arena came to
about 22 KB resident and the disc ended 7,184 B into the stack reserve. With the VM off the game disc
ends at `0x801ecce0`, 13,088 B above the 64 KB reserve (was 14,976 B at `4851147`: +1,888 B). The VM
comes on with CR10 (SPEC P3 reclaims, part data in SPU RAM through `part_lz_src()`).

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
* The cache's 512-entry hash is kept under 3/4 full (S2-12): before an
  insert it is rebuilt without stale entries and tombstones, and when the
  live tiles alone still fill 384 entries (many small images across the
  six pages: the Fun Fair after Candy Catch's long session and
  Whack-a-Mole), whole pages are evicted, least recently used first and
  never one used in the frame being built, until there is room. Before
  S2-12 it rebuilt and retried forever there: the disc hung entering Vape
  Cloud Dodge (frame 2671 of `speed2_fair`). `harness/scripts/fair_cache.txt`
  (in the harness target) plays that sequence and gates Dodge at 29 fps.

### Frame pacing

`plat_begin_frame()` starts a new prim buffer; `plat_end_frame()` waits for
the GPU to finish the previous frame (`DrawSync`), waits for the 30 Hz slot
(`VSync(2)`, PAL 2-2-1), shows that finished frame and starts the GPU on
the new list. The CPU builds frame N+1 while the GPU draws frame N. Game
logic runs once per frame at 30 Hz; when a frame takes longer than two
vblanks the game slows down (no frame skipping), like the native runner.

**Portrait slices (SP2).** Between the frame's CPU work and `DrawSync`,
`gfx_end` runs the portrait cache's sliced jobs (`ps1/src/pslice.c`:
`portrait_idle_next` / `portrait_idle_run`) while the next slice's measured
cost still ends inside the frame's two vblanks, and `plat_portrait_slice_ok`
lets a slice into a frame only where the last frame's own CPU time leaves
room for it (docs/PORTRAIT.md "Idle time and stand-ins"). With stand-ins
(a new expression shows the old picture for up to 4 frames) this took the
Redux dialogue scene from 24.1 to 29.7 fps, the exorcism from 23.8 to 30.0,
the concert band from 28.8 to 29.9 and the town hall menu from 21.1 to 25.0
(the rest there is the island's drawing, P-I1/P-I2). Cost: ~4 KB of the
image and 156 B of RAM. Landed on `e4d7064`, whose own disc ends at
`0x801edf80` (73,856 B below the stack top, 128 B above the 72 KB plan gate),
SP2 first cost 5,424 B of resident code and data. Three changes take back
2,944 B with the same pixels: the stand-in and prefetch code shares one path
(`standin(..., none)`, `pf_queue`); the PS1 keeps the plain divisions in the
life sim (its R3000 divides in hardware; X4's multiplies are for the GBA);
and the platform files off the per-frame path are compiled -Os
(`DOOMO_PS1_COLD`: memcard, hcmd, overlay, txtload, coreref, main, pslice,
model, spu; 2,816 B). `_end` 0x801ee9b0: **71,248 B below the stack top,
2,480 B under the 72 KB plan gate** (5,712 B above the 64 KB `elf2exe`
reserve, so the disc links). Still open: about 2.5 KB of resident RAM to
find (candidates: `vram.c` -Os 432 B, the roster bio / pack text to disc).

### Loading fades (SP5, G-LD; `gfx.c` `gfx_load_fade`, `overlay.c`)

A scene whose code group is not in the overlay slot waits for the drive with the last frame on screen. While
`overlay.c` waits for sectors it calls `gfx_load_fade()`: counted from the frame's first load (a scene that starts
another loads up to three groups in a row), after 4, 7 and 10 vblanks the newest finished frame is darkened by a
50% black TILE pass (semi-transparency mode 0), drawn straight into the picture on screen like the busy banner,
three passes at most (to 1/8). Every game frame is cleared and redrawn in full, so nothing is put back; the next
frame shows normally. The tty logs `ovl: load fade N/3 after V vblanks`. Measured in PCSX-Redux (game disc, the
harness's `scene` / `push`): story group 121 KB in 341 ms, combat 115 KB 328 ms, house 167 KB 482 ms, free play
91 KB 267 ms, concert 70 KB 241 ms (each three passes), pet 23 KB 121 ms (one pass). The core's
`plat_loading()` hook (plat_ext.h) is not used on the PS1: its freezes are these disc reads.

The PS1 has no `plat_hold_band` (the GBA's held dialogue backdrop): its VRAM has no spare area for the band and
its dialogue cost is the portrait renders (`pc_capsule` and friends, ~32%), which SP2's slices address (24.0 ->
29.7 fps measured on SP2's branch). Dialogue on this tree: 24.0 fps (2:55 3:23 4:10 5:1 vsyncs per frame); on a
scratch merge of SP2 (`fut-sp2` 920d280) with this package: 29.67 fps (2:86 3:3), 68 KB below the stack top.

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

### Combat detail and the subdivision levels (SP8, P-C1)

Options "Combat detail" (`OPT_3D_FAST`, `town.h` `OPT3D_*`; the row is on
every platform) picks how hard `r3d.c` subdivides against the affine warp
(`g_r3_sub`, `r3d.h`; `judge()`'s table `k_sub`): a piece is split when it
is bigger than `big_px`, or bigger than `min_px` with a depth range wide
enough (`size * (zmax - zmin) > k * (zmax + zmin)`), down to `depth`
levels. Far pieces have a small depth range for their size, so the larger
`k` / `min_px` leave far and small quads whole and still split the big,
near ones (the floor under the camera, a wall beside it).

| level | big_px / min_px / k / depth | used by |
|---|---|---|
| 0 | 128 / 20 / 7 / 3 (the rule before SP8) | Max detail; the overworld and the house (reset after every view) |
| 1 | 128 / 32 / 10 / 3 | Balanced (the default) |
| 2 | 160 / 48 / 14 / 2 | Smooth |

`view3d.c` `adapt_sub()` sets it every frame: Balanced and Max detail go
one level down for at least 8 frames after a frame whose CPU time crowded
the 33 ms budget (over 31 ms), so the frames that would drop below 30 fps
lose detail and the rest keep it (the adaptive view distance next to it
works the same way). Smooth also keeps the drop shadow under the player
only (`combat_view_detail`, core). Drawing only: the game never sees it.
The HUD face's new expressions are drawn in slices with the old face as a
stand-in (`combat_hud.c` `face_crop`, `portrait_hold`), which took the
worst third-person frame (frame 269, 83 ms: a portrait render) away.

Measured (PCSX-Redux, Vape Cellar, 240 frames after 200, scripts like
`harness/scripts/perf_combat.txt`; vsyncs-per-frame histogram from the
audit's harness copy: 2 = 33 ms, 3 = 50 ms, 4 = 67 ms):

| mode | third person fps | histogram | first person fps |
|---|---:|---|---:|
| base `4114eb7` | 28.07 | 2:209 3:27 4:2 5:1 | 29.81 |
| Balanced (default) | **29.39** | 2:229 3:9 4:1 | **30.00** |
| Smooth | 29.39 | 2:229 3:9 4:1 | 30.00 |
| Max detail | 29.15 | 2:224 3:15 | 29.94 |
| Balanced + SP1's X5 (not landed then) | 29.57 | 2:233 3:4 4:2 | 29.94 |

Polygons a frame (third person, `perf:` line): 520 -> 441 (Balanced). On
Redux every combat frame's CPU time is over 31 ms (39 ms), so Balanced
runs at level 2 and Max detail at level 1 there; on a console (Redux
overstates CPU time 1.5-2x, section 12) they stay at 1 and 0. What is left
in third person is the geometry itself (`quad_rec` 18.5% and `emit_gt4`
13% of the samples, the rest idle 41%). Rebased on `696ffa6` the same
script gives 29.39 / 30.00 / 29.45 / 29.21 (Balanced third / first person,
Smooth, Max detail third person).

RAM: +816 B resident (the governor, the level table, the `opt` harness
command): the disc ends 78,832 B below the stack top on `696ffa6` + SP8
(79,648 B before), 5,104 B above the 72 KB plan gate.

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

The chibi's boxes, plush blobs and pyramids live in `prims.c`. Each one
transforms its vertices once (RTPT per three)
and emits the same triangles `r3_tri_g()` would; an in-emulator A/B build
that recorded both paths for every call (10,240 calls of `model.txt`)
found no difference.

The model is for the 3D view only. Inside a house nobody is a 3D model
(owner, 2026-11-12; "The dream house" below). On the game disc `model.c`
is part of the combat overlay group (section 3, "No 3D models in
houses"): `plat_view3d()` asks `ovl_code_in(OVL_CODE_MODEL)` before it
calls `model_player_draw()`. The engine-test disc links it resident.

### The dream house (`room.c`, H3)

`plat_room()` builds the house's `DoomoRoomView` as a small GTE diorama
(the header of `src/room.c` has the details): the floor and wall slabs
from the grid, CLUT-recoloured wallpaper and floor, the furniture from
its `DoomoFurnPrim` lists (round prims get 8 / 6 / 4 sides by their size
on screen), every character as a camera-facing billboard,
lamp pools and contact shadows, a camera on a `TW_BOING` spring with the
4-frame drop-and-pop of the cut-away walls.

* **No 3D models in houses (PS1-FLAT, owner 2026-11-12).** Until
  2026-11-13 the player was `model.c`'s 3D chibi here. Now the player
  (`RA_PLAYER`) is drawn by the same `billboard()` as every resident,
  visitor, guest and demon, from the body sprite the core hands over (its
  direction and walk frame, the sit squash): the same picture the GBA and
  the Dreamcast show, in every mode (Build, Live, Photo, Score, Hold the
  Door, a visit; HomeMart draws no room). Same ordering slot and day light
  as any actor. The player keeps the contact shadow it always had, now the
  billboard's. It also shows the avatar being played (`playas_avatar()`),
  where the chibi showed the combat player's look.

* **The backdrop.** While the camera and the furniture rest, the static
  layer (floor, walls, every resting piece) is drawn once into the first
  three world-texture pages (the house uses no world textures) and every
  frame starts by copying it back with two 15-bit sprites. Only what moves
  is drawn again, in one clipped pass where each element carries its own
  drawing area. `room-test 6..9` compares this path with the single full
  pass pixel for pixel (1 pixel differs per layout, where two equal-depth
  polygons meet at a clip edge).
* **Mirrored decals (H6).** A furngen `mirror` wall decal (`FF_MIRROR`,
  the haunt's wallpaper eye) is drawn upright on screen: its prims counter
  wall A's shear. Where its local +x runs away from the camera (the key
  light, which turns with the camera, says so: `plx < 0` in
  `piece_setup()`) that counter-shear would double into a diagonal sliver,
  so `prim_mir()` mirrors the prims' x across the footprint, as the sprite
  ports draw facing 0 flipped there: the same choice in every view.
* **The resting player (H4).** While the player's billboard has not
  changed (picture, place, squash) for two frames it is drawn into the
  backdrop with the furniture and again only inside the clips of what
  moves over it, without its contact shadow. A translucent player never
  rests (drawn again over its own picture in the backdrop it would blend
  twice); `home.txt`'s second parity pair has one (91 pixels differ
  without the rule, 0 with it). A moving element at the back
  of a full room has everything in front of it drawn again every frame:
  the Hell-Cellar ran at 18 fps without this and 28 with it (PS1-FLAT;
  17 and 30 with the 3D chibi, H4).
* **Memory.** `room.c` belongs to the `home` overlay group. That group is
  linked 53,764 B *below* the slot, over the `.bss` tables of `world.c`
  and `view3d.c`, the arena bot's path-search scratch (`combat_bot.c`:
  cleared and refilled by every search) and, since G0, `memcard.c`'s
  8 KB card block buffer `s_blk` (`ovl_plan.py` BORROW, `OVL.BIN`
  v3 with a load address per group): it can be 53 KB bigger than the slot
  allows without moving the slot. `world_sync()`, `plat_view3d()` and
  every card read / write call `ovl_borrow_take()` before they touch
  those tables. It marks the group gone (it is read again when a house
  scene runs next); `world_sync()` then rebuilds its tables and
  `plat_view3d()` clears its visibility stamps. So house-group code
  (`home*.c`, `room_ref.c`, `furn_data.c`) must never itself call
  `save_game()`, `load_game()`, `plat_save_read()` or `plat_save_write()`:
  the card I/O would overwrite the house code still on the stack
  (`ovl_borrow_take()` only logs `ovl: ERROR`). A "save from the house" feature first moves `s_blk`
  out of BORROW (8 KB more resident RAM). `overlay_init()` refuses a
  group that loads more than 65,536 B below the slot, so 11,772 B are
  left for further BORROW entries. The resting pieces' boxes (5 KB) go in
  `DoomoRoomView.plat_cache`.
* **Parts (H4).** The group's mode-only code is linked into PARTS that
  share one range after its common code (`ovl_plan.py` PARTS, `OVL.BIN`
  entries after the groups): `visit` (`home_visit.c`, 8 KB), `defend`
  (`home_defend.c`, 7 KB), `mart` (`home_mart.c`, 11 KB, `SCENE_HOME_MART`'s
  entry points: loaded with the scene) and `roomtest` (the harness layouts,
  `room_test.c`, 2 KB) and (H4) `build` (`home_build.c`, 8 KB: walls,
  themes, door / window skins, groups, the clipboard, blueprints; asked
  for lazily, so the harness's room-test layouts never trade places with
  it). The house's common code asks for a part with
  `plat_code_part()` (scene.h; `overlay.c`) before it calls into it; a
  part's section that another part reaches becomes common, the parts are
  `NOCROSSREFS`, and `--report` (with `OVL_PART_EDGES=1`) lists every call
  from the common code into a part for review. A part loads in 70-95 ms.
  The group costs 153 KB common + 11 KB (the biggest part) instead of
  189 KB. Furniture names and descriptions are Huffman-packed
  (`doomo_furn_text()`), and `gfx_upload_now()` stages its rows in the
  other frame's display list instead of 4 KB of its own `.bss`.
* **Harness.** `harness/scripts/home.txt`: `room-test N` (hcmd.c) swaps in
  fixed layouts (H5: 10..13 the fenced Garden, 14..17 the fenced Park Plot,
  in views 0..3: shots `garden_v*`, `plot_v*`), `seed N` pins `plat_entropy()`, `expect-fps`, and
  `expect-same A B MAX` (the parity pairs); then the real Hold the Door
  wave 3, a visit and HomeMart, each checking its part loaded
  (`expect-log ovl: home.defend`). `clock H M` and `money N` play the
  core's house scripts (`home-room` stays native-only).

| scene (Redux, game disc) | patch `room.c` (single pass) | H3 | H4 | gate |
|---|---:|---:|---:|---:|
| `home_steady` (6x6, 3 animated pieces, 3 walkers) | 19.3 | 29.9 | 29.9 | 28 |
| `home_orbit` (the camera turning) | 17.3 | 28.7 | 28.6 | 22 |
| `home_cellar_full` (8x8, 40 pieces, dim) | 11.3 | 25.7 | 30.0 | 24 |
| `home_modal` (Score panel over the room) | 18.8 | 29.2 | 29.8 | 26 |
| `home_defend_stress` (8x8, 24 lit pieces, 10 walkers) | - | 21.1 | 20.8 | 19 (floor) |
| `home_defend_w3` (Hold the Door, the siege's wave 3) | - | - | 30.1 | 22 |
| `home_visit` (Nana Puddin's flat) | - | - | 30.2 | 28 |

PS1-FLAT (the player as a billboard), `home.txt` on this machine before
and after, fps and the CPU time of one sampled frame (`perf:` line):

| scene | base `ce2d7e8` | PS1-FLAT |
|---|---:|---:|
| `home_plain` (the own cottage, Build) | 30.17, 3.1 ms | 30.17, 3.1 ms |
| `home_steady` | 29.39, 37.1 ms | 30.00, 28.1 ms |
| `home_orbit` | 27.27, 35.0 ms | 30.00, 26.3 ms |
| `home_cellar_full` | 27.80, 35.8 ms | 27.80, 35.9 ms |
| `home_defend_stress` | 19.25, 53.6 ms | 23.00, 45.0 ms |
| `home_modal` | 29.19, 21.0 ms | 30.00, 21.0 ms |
| `home_defend_w3` | 30.13, 14.9 ms | 30.13, 6.4 ms |
| `home_visit` | 30.17, 9.9 ms | 30.17, 9.9 ms |

The parity pairs still differ by 0 pixels (the second pair's resting
player is translucent now). No file of `core/`, `gba/`, `dc/` or `tools/`
changed: built clean in one directory, the native runner, `doomogachi.gba`
and the Dreamcast `doomogachi.elf` have the same sha256 on the base and on
this branch. The speed tour's house group
(`speed2.sh OUT house`, 19 windows: Build, the catalog, a held piece, the
turn, Photo, Live, Score, Hold the Door, a visit, HomeMart, the Black
Market) holds 30 in every window; its one miss, the busy Live layout
(28.89, 9 frames over 33 ms), is 30.00 now (one frame of 50 ms).
Third person is as it was: `perf_combat.txt`'s four shots are
byte-identical (29.88 / 30.00 / 29.94 / 29.51 fps, base 29.81 / 30.00 /
29.94 / 29.45), `fast_cellar_tp.txt` with the entropy pinned gives 183
of 184 identical shots (the other frame differs by the adaptive view
distance, which follows frame time) at 28.94 fps (base 28.86).

Since H6 the cellar layout (`room_test.c`) holds the bunny beanbag on frame
0, as it was before H6 (one frame): the beanbag now sinks only while
someone sits on it and sighs for `HT_SIGH` after (`home_draw.c`
`anim_frame()`), never on the layout's loop. The other looping pieces
still flip every 8 ticks. With H6: cellar 30.0, defend stress 20.7 (the
other rows as H4).

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
             title in Shift-JIS ("BOREagotchi LIFE! Story" / "Free Play"),
             16-colour icon CLUT
frames 1-3   16x16 4bpp icon: a pink Boreagotchi egg whose critter blinks
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
* Saves and loads show a full-screen loading screen (`gfx_loading_screen`)
  with a progress bar that moves after every 8 KB block; the one-block menu
  peeks keep the small corner banner. While the BIOS blocks the loop the SPU
  is silenced (`audio_hw_hush`: all voices keyed off, master volume muted;
  the sounding tone voices restart afterwards) - otherwise the tone voices
  sustain their last note as a frozen drone for the whole save. An in-place
  save writes only the blocks its payload needs, not the whole old file.
* Boot: the pad and card drivers are started before the SDK's interrupt
  handler is installed, and the directory scan (`_bu_init`, ~3 s for both
  slots) runs behind a pink "Checking MEMORY CARD" splash.

### Machine config file (U1p)

`bu00:BASLUS-DOOMOC` (title "BOREagotchi LIFE! Settings") holds the core's
machine config record (`plat_config_read` / `plat_config_write`, at most
`PLAT_CONFIG_MAX` = 48 B: U0's 32 B settings + the 16 B adults' code). It
lives outside both save slots so language, volumes and the code survive
erasing or replacing a save. `plat_features()` now reports `PLATF_CONFIG`.

* Same header and info frame as a save (slot index 2), always exactly one
  8 KB block, so it never grows: it is created once and then rewritten in
  place. The CRC in the info frame makes a torn write read back as "no
  record" and the core falls back to its defaults.
* It costs one of the card's 15 blocks; the save code's room check counts
  it like any other file, so a nearly full card still fails cleanly.
* Read and write take about a second (one block), behind the usual corner
  banner / audio hush. A write retries once after re-reading the directory.
* (Refresh) The core writes the record once, when the scene where settings
  changed is left (`opt_config_flush` from `scene_set` / `scene_pop`), not on
  every value change, and never reads it first (the RAM copy is the record).
  Before, every step of the Options' volume slider read the file ("Checking
  MEMORY CARD...") and rewrote its whole block: the game froze for over four
  seconds per press. A config write now sends only the header and payload
  frames (768 B of the block, `write_file(.., 0)`) behind a "Saving
  settings..." corner banner. `harness/scripts/options_volume.txt` (run.py's
  `expect-gap MS NAME` / `expect-count N TEXT`), PCSX-Redux:

  | | 10 volume steps: worst frame | fps | card writes | leaving the Options |
  |---|---|---|---|---|
  | before | 4,450 ms | 2.4 | 10 (+10 reads) | nothing written |
  | after | 33.3 ms | 34.9 (30 + catch-up) | 1, on leaving | one 483 ms frame (file created), 317 ms (rewritten in place) |

  Resident RAM unchanged (`_end` 0x801ef100 before and after, boot FMV on).
* The engine-test card self test (SELECT+START) also writes 48 B, reads it
  back, rewrites 32 B in place, and erases it (`memcard selftest: config ...`).

## 10. Test harness (`ps1/harness`)

`harness/run.py` boots the EXE (`--exe`) or the disc (`--cue`) headless in
PCSX-Redux (`-cli`, under `xvfb-run`, always with a wall-clock timeout) and
drives it with `harness/harness.lua`.

```sh
python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue ps1/build/doomogachi.cue \
    --script core/tests/scripts/ui_gallery.txt --out /tmp/shots   # game EXE: needs the disc (OVL.BIN)
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
`overworld.txt`, `model.txt` (3D citizens), `memcard.txt` (card self test),
`creator_speed.txt` (CR9's floors), `fair_cache.txt` (S2-12: the image
cache under pressure, the old Dodge hang).

`plat_test_flags()` (plat_ext.h) returns `PLAT_TEST_NO_PICKER` while the
harness holds the lockstep `lock` in `g_hcmd` (every `run.py` script run),
so first-boot steps that wait for a person, such as U5's language picker,
are skipped exactly as in the native runner. A console never writes the
mailbox and gets 0.

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

### Speed pass 2 (game disc, PCSX-Redux, no overclock)

The title screen had regressed to **11.6 fps** (CPU 81 ms): its ten
scale-4 logo letters alone are ~700 rects, so the UI shape cache (sized
for the GBA's 240x160 title, 640 rects) overflowed and was reset every
frame, re-rasterising every big glyph twice per frame (`text_emit` was 60%
of the frame). The cache is now 4096 rects / 160 entries on the PS1 and
native builds (16 KB; the GBA keeps 640), and the title no longer runs
`roster_find()` twice per frame for the waving pair. Measured with the
harness `fps` command over 60-120 frames per scene (`prof` samples the PC
once per vsync; `harness/prof.py` tabulates them):

| scene | before | after | notes |
|-------|--------|-------|-------|
| title, logo drop | 14.9 | **29.5** | |
| title, idle | 11.6 | **29.2** | CPU 81 ms -> ~25 ms |
| title, menu open | 11.9 | **28.6** | |
| new island | 22.5 | | 910 rects, 43 shade rects / frame |
| island hub | 22.0 | | 2,030 rects; `rect_slow` + `plat_rect` 25% |
| island hub, town hall menu | 16.2 | | + chibi re-renders (`pc_*` 15%) |
| apartments / roster / resident / shop / news / options | 30 | 30 | idle 50-80% of the frame |
| creator | 25.7 | | 2,730 rects |
| library, gallery, bestiary | 29.5-30 | | |
| minigame gallery (play) | 29.9 | | 2,390 rects, `fx_isqrt` 10% |
| whack | 29.2 | | |
| studio | 30 | | |
| concert (band) | 19.9 | | 4,600 rects + 247 `plat_checker` rows + 100 lines; display list 48 KB of 56 KB |
| pet | 30 | | |
| dialogue | 20.2 | | chibi re-renders + hearts / ellipses |
| exorcism | 25.2 | | |
| dream | 30 | | |
| combat FP / TP (cellar) | 30 / 28.6 | | 3D 9-27 ms |
| Demon Arena | 29.5 | 28.6 | sky now drawn (below) |
| Hell Breach levels 2-6, FP | 28.8-29.8 | 28.4 | |
| story overworld | 29.3 | | |

Where the remaining 2D time goes (profiles of the slow scenes, all CPU
bound, GPU wait 0): the platform's per-rect cost (`rect_slow`,
`plat_rect`, `gfx2d_flush`: 15-40% of a frame at 2,000-4,600 rects),
`fx_isqrt` from `ui_fill_ellipse` / `nested_bands` (4-14%: the bit-serial
square root per ellipse row), chibi / portrait re-renders for animation
phases (`pc_capsule`, `sh_span`, `pc_finish_lines`, `scan_transitions`:
up to 30% in the concert and dialogue - the 18 KB portrait pool holds ~4
figures, fewer than the phases cycling on screen), and `plat_checker`
(one `DR_TPAGE` + one semi-transparent TILE per row). Next steps, in
order of payoff: image-backed shape cache entries on the PS1 (a cached
shape drawn two frames running becomes ONE `SPRT` from the VRAM image
cache instead of tens of TILEs), a bigger portrait pool here, an
incremental ellipse rasteriser, and whole-rect checker tiles.

### Speed pass 3 (rendering, both consoles)

Same harness, same tour scripts (60-120 frames per scene; `new island`
is measured after the boot-time sound effect conversion, one effect per
frame for the first ~60 frames, has finished - the 22.5 of pass 2 was
that conversion, not the scene). Every 2D screen that was below 30 is
faster, pixel for pixel the same (the harness smoke screenshots, the UI
gallery pages, the title and every static screen of the tour are
byte-identical before and after; the native raster tests check the new
arithmetic against the old formulas):

| scene | pass 2 | pass 3 | what changed |
|-------|-------:|-------:|--------------|
| concert (band) | 19.9 | **21.8-22.2** | checker tiles, one texpage per run, chibi lookup memo (two runs: the crowd is random) |
| dialogue | 20.2 | **21.1-22.1** | hearts / ellipses without `fx_isqrt`, shape images |
| island hub | 21.8 | **23.4-23.8** | shape images for the ellipses (trees, patches), rect path |
| island, town hall menu | 17.6 | **18.7-19.0** | |
| exorcism | 25.2 | **26.7** | |
| title, menu open | 27.6 | **28.9-29.4** | logo letters, pills, hearts as shape images: 54% idle now |
| title, idle / drop | 29.3 / 29.8 | 29.5 / 29.8 | |
| creator | 25.7 | 25.8 | bound by `unlock_find` per field (creator module) |
| new island | (22.5) 30.3 | 30.3 | |
| whack, library, gallery, studio | 29.2-29.5 | **30** | |
| everything else | 30 | 30 | |

What changed (all exact):

* **Shape images** (`ui_raster.c`): a cached UI shape (big glyphs, pills,
  hearts, stars, cursors, and now small ellipses) that is drawn two
  frames running also gets a bitmap, painted once from its rect list, and
  is drawn as ONE `plat_image` (one `SPRT` from the VRAM image cache)
  from then on; a shape whose rects use colour 0 (transparent in images)
  or that was clipped keeps its rects. 16 KB pool; the GBA keeps replaying
  rects (no RAM, and its rects are cheap). The native runner's `stats`
  line shows the replayed rects and images built / drawn per frame.
* **Ellipses and flat hearts without a square root per row**: the
  half-width of row `ay` is `isqrt(floor(t * rx^2 / ry^2)) / 2`; the
  quotient is now stepped from row to row with its remainder (the
  parabola's differences) and the half-width walked up or down by whole
  pixels - the same integers, no division or `fx_isqrt` per row
  (`fx_isqrt` was 4-14% of these scenes). `ui_heart_flat` walks its lobe
  width the same way. `fx_dist` takes the 32-bit root when both legs are
  below 46341 (the 64-bit loop is twice as long).
* **`plat_text_outline`** (platform.h, weak core default = the old ten
  passes): the PS1 draws the outline from a dilated glyph page (7x11
  glyphs at VRAM (960, 416), carved out of the image cache's 128-wide
  strip) as one sprite per glyph, then the text: 2 sprites per glyph
  instead of 10.
* **Checker tiles**: a checker of 2+ rows is one semi-transparent TILE
  for the columns every row covers plus the alternating edge pixels (the
  concert's beams and seams were one `DR_TPAGE` + one TILE per row), and
  a `DR_TPAGE` equal to the one the chain last set is skipped
  (`g_tpage_cur`, reset after a 3D view).
* **Per-rect cost**: `gfx2d_flush()` is an inline check, the per-rect
  statistics counters are gone, the `perf:` line reports prims only.
* **Portrait cache**: a direct-mapped memo in front of the cache scan
  (`cache_get`) and a word-wise look hash: the ~45 chibi lookups a
  concert frame were 7% of it. (A bigger pool, 80 blocks, was tried and
  measured no gain worth its 12 KB; the pool stays 48 blocks.)
* **RAM**: the image cache's hash is 512 entries (8 KB less), the boot-time
  font / CLUT staging uses the cache's staging union (12 KB of statics
  gone), the SPU's PCM -> ADPCM staging is shared (4.5 KB). With the 16 KB
  image pool and the bigger cache entries (4 KB) the pass is a net 4.5 KB
  less resident RAM than before it.

Where the remaining time goes: the island hub still issues ~1,900
`plat_rect` calls at 320x240 (land, roads, decorations: `rect_slow` +
`plat_rect` ~25%), the concert ~4,500 (LED wall, seats, fringe) plus
its chibi re-renders (the lead singer's mouth flaps every 4 ticks), and
the dialogue's hearts; a full chibi render for a *new* expression still
runs in one frame (~30 ms here, one dropped frame when the title's pair
changes expression). Next steps: a sprite for the island's static land
layer (needs ~67 KB, i.e. a smaller display list), a stand-in image
while a new expression renders in slices, fewer rects in the concert's
wall shows.

**Outdoor skies were black in every 3D view** on the game disc (Demon
Arena, the Hell Breach levels). The packed sky is decoded into the frame's
display-list memory and uploaded from there, 52 KB in all; `r3_sky()` runs
after the cells' polygons, by which point less than that is free, so the
upload failed silently every frame. `view3d.c` now fetches the sky
(`vram_sky`) at the top of `plat_view3d()`, while the list is empty, once
a map has shown sky cells; the upload then happens once per sky frame as
designed (verified with a harness screenshot of the arena).

### The concert (SP6, P-K1)

Three concert shows on the game disc in PCSX-Redux, 240 frames each after they settle (the GBA's
`concert_profile.txt` windows: Nana's solo, Rivet's band = `perf_concert.txt`'s keys, a 12-voice
choir); fps and vsyncs-per-frame histogram, `7729ada` -> SP6:

| show | `7729ada` | SP6 |
|------|-----------|-----|
| solo | 30.00 (2:238 3:1) | 30.00 |
| band | 28.97 (2:222 3:16 4:1), cpu 36.4 ms | **29.57 (2:232 3:6 4:1), cpu 35.0 ms** |
| choir | 30.00 (2:238 3:1) | 30.00 |
| `perf_concert.txt` (CI's row) | | 29.51 |

What helped here is the core half of SP6 (docs/GBA.md "The concert (SP6)"): the LED wall's waves
and fields draw only the visible pieces and merge neighbouring columns, the flames merge row pairs
(fewer `plat_rect` calls, so fewer prims), and the stage's per-fan, lamp, beam and confetti
divisions are stepped. Same pixels: the three shows' harness shots are byte-identical before and
after. The two new hooks (`plat_ui_loops`, `plat_beam`, `plat_ext.h`) are the GBA's; here they are
the weak no-ops and the beams stay checker tiles.

Not done: P-K1's replay of the LED wall's dots as one image. Every busy show is a function of the
tick (waves, fields, flames move every frame), so a cached image of the wall would be rebuilt each
frame; the static shows are already cheap. The band's last 0.4 fps is mostly the lead's figure
(its portrait job re-renders a talk / breath / blink phase, about 30% of the frame in the
profile: SP2's slices, X1) and the checkers.

### Life tour (M0, `harness/scripts/perf_life.txt`)

The free-play screens the life sim lives on, on the game disc (made by
`harness/mkperf.py` like the other `perf_*.txt`; the same screens as the
GBA's `make -C gba test-life`, docs/GBA.md "Life tour"): a Hellvile Park
Classic island's hub, the news (L), the Town Hall menu (A), Citizens (R),
a citizen and Hillvile Towers, each measured with `fps` / `fpsend` over 90
frames. `python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue
ps1/build/doomogachi.cue --script ps1/harness/scripts/perf_life.txt --out
DIR` (or `ps1/harness/perfall.sh DIR life`). The press budgets of the same
journeys are native (docs/GAME_DESIGN.md 4.1).

| screen | fps | CPU per frame (the nearest `perf:` line, every 150 frames) |
|---|---|---|
| island hub | 25.0 | 40 ms |
| news | 30.2 | 10 ms |
| island menu (Town Hall) | 25.6 | 61 ms |
| Citizens (roster) | 30.2 | 12 ms |
| a citizen (resident) | 30.2 | 11 ms |
| Hillvile Towers (apartments) | 30.2 | 10 ms |

Like on the GBA, the island map and its location menu are the only life
screens under 30 fps; everything pushed over the island holds 30.

### Speed package SP1 (2026-11-08)

The exact steps of the 2026-11-08 speed audit that reach the PS1 (game disc, PCSX-Redux, against its merge base
`b080562` built the same way; the audit's tour scripts):

| scene | fps | frames by vsyncs (2 = 33 ms, 3 = 50 ms, 4 = 67 ms, 5 = 83 ms) | base fps | base frames by vsyncs |
|---|---|---|---|---|
| combat third person (Vape Cellar), first build (not landed on the PS1, see RAM) | 29.09 | 2:225 3:12 4:2 | 28.07 | 2:209 3:27 4:2 5:1 |
| combat first person | 29.88 | 2:236 3:3 | 29.81 | 2:235 3:4 |
| life tour (hub, news, menu, roster, resident, apartments) | unchanged | unchanged | | |

* **Combat billboards (X5):** not on the PS1 as landed. `combat_bb_sort()` (culls and orders the billboards by squared
  distance with the old `fx_dist` order kept exactly) took `fx_dist` (5.5% of the third-person profile) out and
  measured 28.07 -> 29.09 fps in the first build, but it is resident here (with `combat_build_view`) and cost
  +320..480 B (even at -Os with a smaller near-tie test) against 112 B of room above the 72 KB plan gate. The PS1
  keeps the old insertion sort (`#if DOOMO_PLATFORM_PS1` in `combat_build_view`) until resident RAM is freed;
  then it is a one-line switch.
* **News icons (X2)** are one pass instead of up to 33 string scans per row; the PS1 news already held 30 fps.
* **The unlock key index (X3) is not on the PS1:** its 3.9 KB would be EXE, i.e. RAM, and the PS1 creator and
  options already hold 30; `unlock_find` scans as before here.
* **RAM:** +0 B resident as landed (the EXE ends at `0x801edf80`, base `fc5f87f` the same). The news keywords are
  fixed-width rows (`char [27][9]`), not a table of string pointers: the pointer table made `ovl_plan` keep the
  177 B of strings resident.
* **Harness (T1):** `fpsend NAME` also logs `gaps NAME vsyncs-per-frame histogram 2:N 3:N ... max M (ms) at frame F`
  (the table above), so a window's hitches show next to its average. `harness/prof.py MAP PROF --group NAME` names
  scene-slot addresses by the overlay group that was loaded (the group's `.ovl_NN` sections from `ovl.ld`, each
  symbol's section from the ELF): the audit's exorcism profile read `cook_serve` 10.6% (the fishcook group's
  symbol at that address); with `--group exorcism` it reads `curtain` 18.2%.
* **Exactness note:** the perf scripts have no `seed`, so `town_new()` takes `plat_entropy()` (the CPU timer), and
  any build whose boot takes a different number of cycles makes another town: the life tour's roster then shows
  other activities. With `seed 1` first, every screenshot of the life tour is identical to the base's.

### Speed tour 2 (SA2, `ps1/harness/speed2.sh`)

`sh ps1/harness/speed2.sh OUTDIR [groups...]` (from the repo root, after a game build) plays the GBA's speed tour 2
scripts (`gba/harness/scripts/speed2_*.txt`, docs/GBA.md "Speed tour 2") on the game disc: `tools/speed2.py ps1`
turns each window into `fps` / `fpsend` (GBA-only lines such as `sync`, `profile` and `tips` are dropped), run.py
plays it, and `tools/speed2.py table OUT/GROUP/tty.log` prints fps, the frames by vsyncs, frames over 33 / 50 ms, the
worst frame and the overlay loads before the window (the `ovl: GROUP (N KB) in M ms` lines: the PS1's scene-change
freeze). A single `3` at a window's first frame is the lockstep resume and is not counted. `JOBS=2` runs two groups
at once; `PROF=1` writes PC profiles instead (`prof.py --group`).

A hang (no new frame for `--hang` vsyncs) now also logs `HANG: pc=... ra=... sp=... stack-code ...` (the stack words
that look like code addresses; `mipsel-linux-gnu-addr2line -f -e build/doomogachi.elf ADDR` names them). S2-12 found
the Dodge hang this way: `memcpy` <- `hash_rebuild` <- `vram_image` <- `plat_image`, the image cache retrying forever.
After the fix the whole `fair` group plays in one run (the Dodge title card 30.17 fps, play 30.12, no frame over 33 ms).

**S2-2 (the action cards).** The one action window under 30 was the Hell Breach intro card (22.9 fps: 34 of 90 frames
at 2 vsyncs, 55 at 3): `text_emit` 24% + its run emitter ~17%, because the banner's title at scale 3 is 273 px wide and
a cached shape stores 8-bit x and widths, so it was rasterised again on every frame. `ur_text_big` now caches a block
wider than 255 px as column windows of 255 px, each its own shape (the runs are only cut at the window edge: the same
pixels), and the card holds 30.00 fps (2:88 3:1, the lockstep resume) like every other card, menu and results window
of `speed2_action`. `text_emit` is compiled `-Os` on the PS1 (it runs on cache misses only), which pays for the windows'
code: resident RAM 96 B smaller than before, 66 KB below the stack top with the boot FMV. The GBA's held 3D view
(`plat_view_hold`) and S2-1's modal hold are not built here (`combat_int.h`, `levels_select.c`, `ui.h`).

**S2-3 (the Prize Wheel).** 22.6 / 22.7 fps (intro / spin: 56 and 79 frames of 3 vsyncs): the pie's per-span
`fx_atan2`. With the generated octant table (`core/gen/mg_wheel_tab.c`, in the minigame overlay: `ovl_plan.py` puts
the generated file in that group, 87 -> 96 KB, the slot is the house's 168 KB) and the rim as ring rects, every wheel
window holds 30.00 (intro, spin, the result card, done: 2:58-2:128 with the one 3-vsync lockstep frame), exact (the
native 320x240 shots are byte-identical). Resident RAM byte-identical, 66 KB below the stack top with the boot FMV
(`unlocks.c` keeps its old walk here). The GBA's held frame is not built here (`ui_modal_hold` is compiled out).

**S2-6 (the Unlocks gallery).** The PS1 already held 30 there (30.00 / 30.00 / 30.25: the GPU fills the tiles for
next to nothing); after S2-6 the gallery, a cursor move, the new row-moves window and the Paints tab play at 30.00 /
30.00 / 30.00 / 30.25 (one 50 ms frame each, the lockstep resume). The grid panel and the owned-count fast path are
not built here (`UI_PANELS` 0, `unlocks.c` is resident): `unlock_at` walks the table lengths only, 12 B less resident
code, 66 KB below the stack top with the boot FMV.
**S2-7 (the credits parade).** 10.88 fps (credits) and 2.01 fps (parade: 129 frames over 50 ms, worst 550 ms): the
portrait cache re-drew 8-10 chibis a tick (four walk frames for ~44 marchers in a cull range wider than the 3D view).
With one pose per marcher, the range cut to what the view shows (~485 units either side plus the camera's lead) and
the body job (`bodyspr_lazy(3)`, sliced over ticks, `pslice.c` timing it) both windows hold 30.00 (one 3-vsync frame
each); the PS1 shots of the parade show the marchers to both screen edges. Resident: no data (the body memo
stays 40 slots here), +836 B of `portrait.c` code (the body job path, until now built for the GBA only); 65 KB below
the stack top with the boot FMV (was 66 KB).

**S2-8 (text pages as kept panels).** Nothing is built here (`UI_PANELS` 0: the Field Guide's and the Almanac page's
text panels compile out, `page_text` keeps its old path; the Field Guide's preview scroll byte sits in the wing's
scene scratch). The library already held 30 and still does, numbers as in the audit: Field Guide contents / page /
turn / scroll 30.17 / 30.17 / 30.77 / 30.77, Almanac profile and face 30.00 (one 50 ms frame each, the lockstep
resume), the Almanac list move 29.51 (one 67 ms frame, not text). 66 KB below the stack top with the boot FMV.

**S2-5 (scaled images, the Derby, the welding bench).** The PS1 already held 30 in all of these (the GPU scales
sprites for free); only the welding bench's drawing changed here (the seam's y per column made once per weld, seam
and bead drawn as runs of same-y columns: the same pixels, ~200 fewer `plat_rect` calls a frame). Measured after
(PCSX-Redux, the speed2 windows): weld intro 30.17, welding 30.12, Derby title / race sweet / mid / hellfire 30.17 /
30.00 / 30.00 / 30.00, weapons frames / plan 30.17 / 30.00, home / fog / eruption / Vapula vignettes 30.17 and after a
press 30.25. `plat_ui_loops` is the weak no-op here and the Derby's 8bpp racer pictures are GBA-only; resident RAM
unchanged, 65 KB below the stack top with the boot FMV.

**S2-4 (the fair's kept playfield).** Nothing is built here: `mg.h` turns the kept-playfield mode into constants (`MGK_OFF`), so Whack draws its whole booth as before (the restore helpers and `ui_round_rect_inset` compile out); only the drawing order was regrouped (`mg_tent_sway`, `hole_back`), same pixels. 65 KB below the stack top with the boot FMV on the rebased tree (`_end` the same as without S2-4); the minigame overlay group grows by a few hundred bytes of code, the slot is unchanged (168 KB, the house's). PCSX-Redux `speed2_fair`: the Whack title, Sweet and Hellfire windows 30.17 / 30.12 / 30.12 fps (all 2-vsync frames), as before.

**S3-3 (the creator's part grid).** `part_icon_unpack` was 30% of the hat grid's try-on frames: every visible cell
(10 to 40 of them) was unpacked again on every draw. `grid_icon` now keeps each cell's unpacked icon in the unused tail
of the overlay slot (`plat_scene_scratch`, 48 cells x 400 B = 19,200 B; the creator group needs about 100 KB of the
131 KB slot, so no `ovl_plan.py` `SCRATCH` entry: the call returns NULL when the room is not there and the old unpack
runs; the generation is the overlay load count, a changed one clears the cell tags). A cursor that moves over the page
unpacks nothing (`part_icon_image_kept`, tested on the host). The GBA parts of S3-3 (held frame, panels drawn first)
are not built here (`UI_PANELS` 0). Measured (PCSX-Redux, `creator_speed.txt`, `speed2_creator`): species grid scrolled
29.39 -> 30.00, hat grid try-on 28.57 -> 29.03 (2:27 3:1 4:1), a cursor move in the species grid 30.00 -> 29.51 (one
67 ms frame where there was a 50 ms one: the slices of the new look land on other ticks now that the frames are
shorter), idle 30.13, rows 30.13, a value held 29.88 as before. The try-on's profile after: `rect_slow` 13%, the pane's
ellipses 10%, `lock_compose` 8%, `out_rect` 7% (`part_icon_unpack` is gone). The disc ends at 0x801efe60 both
before and after (64 KB below the stack top with the boot FMV; the 72 KB gate is the SPEED stream's PS1-RAM package).

## 13. Notes for the core

* The 2D layer is fastest with few, large rectangles: one `plat_rect` per
  span costs ~4 us of platform CPU per call on top of the core's own work.
  Vertical stacks of same-width spans and stride-2 dot rows are merged
  automatically; anything else is one GPU primitive per call.
* `plat_shade_rect` is a blend approximation on the PS1 (see section 4).
* `save_game()` blocks for about a second per 8 KB block on a PS1 (BIOS);
  a "Saving..." banner is shown by the platform.
* `plat_save_scratch()` returns both display-list buffers (section 3):
  valid only until the end of the current frame, as `platform.h` allows;
  take it from update(), not halfway through draw().
* Scene code runs from an overlay slot (section 3): never call a scene's
  functions except through scene.c, and do not leave pointers to a
  scene module's const data (strings, tables, callbacks) in a resident
  module's state after the scene has gone.
* Texture, sky and sprite pixels are not CPU readable here (packed,
  section 3): pass `texture_pixels()` / `sky_pixels()` / `texture_image()`
  / `sprite_image()` results to `plat_*` only; use `texture_avg()` /
  `sky_horizon()` for colours. Map cells are read only through
  `map_load()`.
* RAM is shared by code, const data and state: prefer small tables,
  share buffers, and keep scene state small.
* Platform-specific cutscene detail: the Feast of Rebuilding "rear push"
  close shot (`push_shot()` in `core/src/story_feast.c`, panel 3) checks
  `plat_name()` through `on_ps1()`. On the PS1 the brass valve in the crease
  is left out and the hose simply disappears into it, which saves three
  `plat_rect` calls per frame on the 2D layer. GBA and native builds keep
  the valve. The close shot is otherwise identical on all platforms, and
  any new detail added to it should follow the same pattern.

### SFX PCM on the disc (patch 42)

`core/gen/sfx_data.c`'s 47,793 B PCM array is not linked into the game EXE
(`DOOMO_SFX_ON_DISC`, set by `ps1/CMakeLists.txt`; `tools/sfxgen.py` wraps the
array in `#ifndef`). `ps1/tools/sfxbin.py` writes it to `SFX.BIN`; `src/sfxdisc.c`
reads one effect's sectors into a 6 KB window (`plat_sfx_pcm`, called by the core's
`audio_sfx_pcm`), and `audio_hw_sfx_preload()` converts every effect to SPU RAM at
boot, after `overlay_init()`. Net: -41 KB resident; the disc ends at `0x801e8290`
(about 95 KB below the stack top). Simpler than the ADPCM + wave table plan in
design_grow.md (the jukebox waveform reads the PCM through the window).


## RAM fix 2026-11-08: disc over the stack reserve with the boot FMV

CI on 18be42b failed: with `INTRO.STR` and `fmv.c` linked the EXE ended at `0x801f0220`, 544 B into the 64 KB stack reserve. `pet_gfx.c` (16.5 KB resident, it was in `DOOMO_CORE_HOT` at -O2) is now compiled -Os and `fmv.c` joined the -Os cold list: resident 6.7 KB smaller, the EXE ends at `0x801ee790`, 70 KB below the stack top. The pet screen still runs 30.00 fps on PCSX-Redux (`perf_pet.txt`, CPU 24.5 ms per frame).

## CR3 (2026-11-13): the creator's string pool leaves the resident image

The planner (`tools/ovl_plan.py`) moves `.text` and `.rodata` sections of a group's files into the overlay,
but a merged string section (`.rodata.str1.4`) is one section per object: if anything resident points into it,
the whole pool stays. `creator_data.c` kept about 6 KB of row labels and help lines resident that way (two
tables read by resident code, and its 8-byte `n_*` name lists in `.sdata`). Now `unlocks_creator.c` holds the two
tables and `creator_data.c` is compiled `-G0` (`ps1/CMakeLists.txt`). Measured on `ee686c7` (clean builds, `cmake
--preset ps1 -DDOOMO_INTRO_REQUIRED=ON`): `_end` 0x801efe60 -> 0x801ef4e0 with the gene fields live, 65,936 B ->
68,368 B below the stack top. Check the link map for the same trap whenever a creator-group file gains a pointer
table: `grep "creator_data.c.obj(" ps1/build/doomogachi.ldmap` must show its `.rodata.str1.4` only in the overlay part.
## LV0 (levels registry): resident delta -112 B

The 24 new levels are registered on the host, GBA and Dreamcast builds only. The PS1 keeps the old seven
(`LV_TABLE_N` and `LV_HAS_NEW` in `core/src/levels_int.h`): no new table rows, no `BRCH` chunk, no page code,
until the LEVELS share of PS1 memory is released (flip the two macros then). Measured with the boot FMV on
`0df72845` (after CR3, clean builds both sides): the EXE ends at 0x801ef4e0, 68,368 B below the stack top;
with LV0 at 0x801ef450, 144 B lower, so 68,512 B below the stack top. LV0 does not shrink the margin; the
tree is still under the 72 KB rule until PS1-RAM lands.
Gain: the save registry is a pointer table instead of a 13-case switch, `LvRun.size_cls` is packed to two
bits an actor (96 B). `game_smoke` and the level select script PASS; every level-select screenshot is
byte-identical to the base except the autopilot demo shot (`breach_demo.png`). That shot is not evidence of
a behaviour change: the demo's first 100 ticks are byte-identical on both builds, two adjacent demo frames of
one build already differ in 87 % of their pixels (the camera turns fast), the HUD reads the same state in
both (90 HP, 0/17, 111 candy), and a semantics-preserving edit alone (the `size_cls` pack applied to the base
on its own) moves the shot in the same way. Each build is deterministic by itself.

## CR4 A2 (2026-11-13): hair batch

Measured on `b63abec` and `b63abec` + A2, then again on the rebased tree (after LV0; clean builds, `cmake --preset
ps1 -DDOOMO_INTRO_REQUIRED=ON`): `_end` 0x801ef4e0 both times on `b63abec`, the overlay slot ends 0x801ef3b4 both
times. On the rebased tree `_end` is 0x801ef450 (the same as LV0 alone), 68,512 B below the stack top with the boot
FMV, so A2 adds no resident byte; the tree is still under the 72 KB rule until PS1-RAM lands. The 40 new parts (32 hair styles, 8 beards) are part scripts the PS1 does not read yet (the part
VM is off until CR10); their catalog rows (names, sets, tags) are overlay data, no new age row (age class ALL, so
the resident age table is unchanged). The creator offers the PS1 only the painted hair and facial hair
(`part_live_drawable`), as for every scripted part since CR3.

## CR4 A3 (2026-11-13): spooky batch

Measured on `0c1fc51` and `0c1fc51` + A3 (clean builds, `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`, `PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b`):
`_end` 0x801ef450 both times, the overlay slot ends 0x801ef324 both times, 68,512 B below the stack top (on the rebased tree after A2, STKOPT and Talk: `_end` 0x801ef580, 68,208 B below the stack top, identical to origin before A3; the elf2exe line prints "66 KB left"), the disc image and
`OVL.BIN` the same size (the EXE bytes differ: the resident part age table is rebuilt from the catalog). The 60 new parts
(10 horns, 16 tails, 14 wings, 4 halos, 16 auras) are part scripts the PS1 does not read yet (the part VM is off until CR10):
their catalog rows are creator-overlay data and all are age class ALL (no new age row). The creator offers the PS1 only the
painted choices (`part_live_drawable`: Tail 1/16, Aura 1/8), as for every scripted part since CR3. Harness (PCSX-Redux):
`game_smoke`, `creator`, `creator_grid` and the new `creator_parts_spooky` PASS; `creator_speed` idle 30.13, grid scroll
29.39, rows 30.13, value 29.88 (the S3-3 numbers: nothing resident changed).
