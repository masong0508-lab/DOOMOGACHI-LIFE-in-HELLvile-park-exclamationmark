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

`DOOMO_PS1_TALK` defaults to `OFF` to keep the normal game disc within
the console's RAM budget. The experimental `ON` build places
`TALK_EN.BIN` at the ISO root and reads it through a 2 KB sector buffer.
CI linked that build and rejected it at disc packaging because its
resident image left no stack space. The engine-test build always omits
Talk. See the [Talk port notes](LIVING_PEOPLE_TALK.md#ps1-disc-backed-talk-integration).

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
PS1 resident cost: +1.6 KB (65,952 B below the stack top with the FMV on 2026-11-12; PS1-FLAT's number is in section 3).

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
left below the stack top`; under the 72 KB plan gate it also prints a
WARNING). The numbers of 2026-11-12 (131,312 B below the stack top with the
boot FMV) and the ledger of where the 2 MB go are in "RAM headroom
2026-11-12" at the end of this file. With every module of the game landed the
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

70,480 B was 3,248 B under the 72 KB plan gate. On the tip it landed on
(origin `19d01fe`, after the Talk, LV0, LV1, CR3, S3-3, LV10 and STKOPT
landings; the same overlay plan, the combat group 128,232 B) the EXE ends
at `0x801ef580` before and `0x801ee3e0` after: the margin went from
68,224 B to **72,736 B** (+4,512 B; `0x80200000` minus `_end`, as above):
992 B under the 72 KB plan gate (73,728 B), 7,200 B above the 64 KB
reserve. (On `c827410`, a few landings earlier, it was 68,528 -> 73,056 B,
672 B under: every landing since eats into the margin.) PS1-RAM closes
the rest.

Not moved: `prims.c` (3,612 B in the plan, the chibi's boxes and blobs,
-O2). With it the combat group would need 131,800 B, 728 B over the slot,
and the slot grows in 2 KB steps: +2,048 B here, a net gain of 1,564 B
(the margin on the tip would be about 74,300 B, past the 72 KB gate). On
PS1-RAM's tree the combat group is 126,696 B before this and 134,440 B
with `model.c` and `prims.c`: the slot would grow by 4,096 B, a net loss
of 484 B. Growing the slot costs resident memory and needs the
coordinator (`PARALLEL_WAVES.md` section 3), so it was left: whoever
lands second, with the numbers of both trees, adds `prims` to
`PLAT_GROUPS` (one word) when the combat group has 3.7 KB to spare under
the slot. After this package the combat group has 2,840 B left under the
slot (about 200 B on PS1-RAM's tree): growth of `combat*.c` / `actor.c` /
`levels*.c` beyond that moves the slot.

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
comes on with CR10 (done 2026-11-15, see "CR10" below: the part data is in SPU RAM).

### SPU RAM (512 KB)

```
0x00000-0x00fff  capture buffers (hardware)
0x01000-0x01fff  silent loop block + the chiptune waveforms (1.8 KB)
0x02000-0x4ffff  sound effects (ADPCM; all converted at boot, about 35 KB used)
0x50000-0x7dfff  PARTS.BIN (CR10): the part scripts and the packed part icons, 143,360 B of the 184,320 B (file on disc 145,408 B)
0x7e000-0x7ffff  reverb work area (MS-S4: Studio Small at 0x7e0c0, only while the studio is open)
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
  wrapping once per 360 degrees and following the pitch. The edge fills
  take the same lighting factor and clip to the viewport. A camera pointed
  past the panorama uses a fill without uploading the sky texture.
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
and after, on the tip it landed on (`c827410`), fps and the CPU time of
one sampled frame (`perf:` line). On `ce2d7e8` the same scenes gave the
same within 0.2 fps (steady 29.39 -> 30.00, orbit 27.27 -> 30.00, defend
stress 19.25 -> 23.00):

| scene | base `c827410` | PS1-FLAT |
|---|---:|---:|
| `home_plain` (the own cottage, Build) | 30.17, 3.1 ms | 30.17, 3.1 ms |
| `home_steady` | 29.39, 37.2 ms | 30.00, 28.2 ms |
| `home_orbit` | 27.17, 35.5 ms | 30.00, 26.4 ms |
| `home_cellar_full` | 27.80, 35.9 ms | 27.80, 36.0 ms |
| `home_defend_stress` | 19.10, 53.6 ms | 22.78, 45.4 ms |
| `home_modal` | 29.19, 21.0 ms | 30.00, 21.1 ms |
| `home_defend_w3` | 30.13, 15.1 ms | 30.13, 6.5 ms |
| `home_visit` | 30.17, 9.9 ms | 30.17, 9.9 ms |

The parity pairs still differ by 0 pixels (the second pair's resting
player is translucent now). The numbers above were measured three times
on three trees (`ce2d7e8`, `32257f3` after the Talk landings, and the tip
`c827410`, with the harness runs repeated on `19d01fe`); the CPU time of a sampled frame is about 0.13 ms higher on
the tip, base and head alike (not this package), and the fps agree within
0.2. Besides the player's place, the `home_cellar` shot differs on the
room's outline by 1 to 2 px: it now has the outline of the other 8x8
layout (`home_defend`), which did not change. No file of `core/`, `gba/`, `dc/` or `tools/`
changed: built clean in one directory, the native runner, `doomogachi.gba`
and the Dreamcast `doomogachi.elf` have the same sha256 on the base and on
this branch. The speed tour's house group
(`speed2.sh OUT house`, 19 windows: Build, the catalog, a held piece, the
turn, Photo, Live, Score, Hold the Door, a visit, HomeMart, the Black
Market) holds 30 in every window; its one miss, the busy Live layout
(28.78, 10 frames over 33 ms), is 30.00 now (one frame of 50 ms).
`home_cellar_full` and `home_defend_stress` are stress layouts that were
below 30 before and are below 30 now (the second is cheaper).
Third person is as it was: `perf_combat.txt`'s four shots are
byte-identical (29.81 / 30.00 / 29.94 / 29.51 fps, base 29.81 / 30.00 /
29.88 / 29.45). `fast_cellar_tp.txt` picks its spawn from the entropy,
which differs from build to build (28.64 base, 27.57 here, each
repeatable), so it is played with `seed N` (the same game on both discs):
seeds 1, 2, 3, 7 give 28.90 / 28.45 / 28.86 / 28.42 fps on the base and
28.86 / 28.34 / 29.01 / 28.42 on PS1-FLAT (both average 28.66), and
184, 170, 175, 178 of 184 identical shots. The other shots differ by the
adaptive view distance (it follows frame time) or, in seed 2's shots 61
to 70, by the 3D chibi's run cycle (it follows the frames drawn): the
same PS1-FLAT disc started one vsync later reproduces the base's pixels at
those shots, so it is pacing, not drawing.

Since H6 the cellar layout (`room_test.c`) holds the bunny beanbag on frame
0, as it was before H6 (one frame): the beanbag now sinks only while
someone sits on it and sighs for `HT_SIGH` after (`home_draw.c`
`anim_frame()`), never on the layout's loop. The other looping pieces
still flip every 8 ticks. With H6: cellar 30.0, defend stress 20.7 (the
other rows as H4).

## 8. Audio

* **Tones / hybrid patch bank** (`plat_tone`, channels 0-3 -> SPU voices 0-3;
  `src/instrument_bank.h`). The PS1 build uses the six stable core wave IDs as
  instrument selectors without changing the shared song/save format: pulse12 =
  glass/celesta bell, pulse25 = tine electric piano, pulse50 = warm organ,
  triangle = round bass / mellow flute, saw = dark string pad, noise = the
  existing noise percussion voice. The Music Studio shows these PS1-specific
  names instead of the shared waveform labels. Each patch also selects a PS1
  SPU ADSR envelope: bell and tine decay, organ and bass sustain, and the pad
  swells and releases slowly. The hardware handles these envelopes, with no
  per-frame CPU work. The five melodic patches are additive, integer-harmonic
  spectra made at boot (no floating point); partials above each cycle's
  Nyquist are omitted. Integer
  harmonics make every cycle close exactly, avoiding a click at the loop point.
  Each patch is encoded to SPU ADPCM by our encoder (`src/adpcm.c`) as a faded
  first cycle plus a loop cycle encoded for the SPU's real predictor history.
  Five cycles per patch (112, 56, 28, 14 and 7 samples) cover the full pitch
  range using the same zone picker as before. The 155-block waveform area remains
  2,480 B at `0x1000-0x19b0`, inside the existing `0x1000-0x1fff` allocation;
  no extra SPU RAM or music voices are used. Pitch = `freq_hz x cycle x 4096 /
  44100`; the voice is only re-keyed when the patch or pitch zone changes, so
  ordinary note slides stay click-free.
* **Cycle choice** (`src/tonepick.h`, host-tested by `harness/tonepick_test.c`). A new note takes the long
  cycle up to 1571 Hz and the short one above, as always. A note that is already sounding keeps its
  cycle while the pitch register can reach it: it moves up to the short cycle only above 1571 Hz and
  back to the long one only below 700 Hz. Before, every crossing of 1571 Hz keyed the voice on again,
  which cuts the note (a click): a 1700 / 1400 Hz arpeggio re-keyed on all 99 changes, now on none.
  The pitch sum is also safe from a 32 bit wrap (the short cycle wrapped above 37,449 Hz; the core
  never sends more than 20,000). Not listened to on a console. Ceiling: the short cycle tops out at about 6300 Hz
  (register 0x3fff), not the ~10 kHz written above.
* **Samples** (`plat_sample`, voices 4-23). The core's 8-bit PCM
  (`audio_sfx_pcm`) is converted to ADPCM (fast encoder mode) the first
  time an effect plays, or in the background one effect per frame after
  boot; a free voice (ENDX) or the oldest one is used.
* **Loop preparation and pitch zones** (`src/waveform.h`, `src/tonepick.h`).
  Each patch has five cycle lengths (112, 56, 28, 14, 7 samples). A shorter
  cycle pushes the SPU's 44.1 kHz sample-rate ceiling higher before skipped
  samples fold upper partials back into the note; the hysteretic zone picker
  keeps a sounding note in its current cycle while the pitch register can reach
  it, preventing clicks on glides. The 14- and 7-sample cycles are repeated to
  fill a 28-sample ADPCM block and level-matched to the patch's 28-sample zone.
  The first cycle fades in from silence; the second is encoded with the SPU's
  actual two-sample decoder history carried across the loop jump. All generation
  and encoding happen once at boot, with no per-frame synthesis or new staging
  buffers. The original pulse / triangle / saw waveform vectors remain in the
  host test suite as encoder regression references.
* **Encoder quality** (`harness/adpcm_test.c`, host): the legacy pulse, triangle and saw loops
  (`waveform.h`) and all five hybrid patches in every pitch zone (`instrument_bank.h`) are checked
  for DC, peak level, loop markers, repeated-pass stability and loop SNR using the SPU decoder
  history across the loop jump. Sine, chirp and noise are also SNR-tested after ADPCM conversion.
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
  (section 4), apart from screens dimmed by a modal. The harness sets
  `lock` as soon as the mailbox's magic word is in RAM, which can be while
  the BIOS is still loading the EXE: when `lock` sits in the next 2 KB
  sector (`g_hcmd` at `0x8013f7fc`, the PS1-RAM tree of 2026-11-15), that
  sector's load wrote it back to 0 and the game ran free (the language
  picker, no shot, a timeout). `harness.lua` now sets it again on every
  vsync until the game has stopped once.
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

G2-CO (2026-11-15, the GBA concert at 30; docs/GBA.md "G2-CO: the concert at 30") leaves the PS1 as
it was: its trades and kept paths (the baked stage, the 15 Hz rows, the flame trade, the smaller
choir, the held curtain, `look_run_hash`'s early return, the new weak hooks) are GBA only or compiled
out here; the concert's code moved into `stage_parts` with the same drawing (`test_concert_gfx.c`'s
golden frames pass), and the setup menu's rows stay where they were (the one-pixel nudge is GBA only).
One exact change reaches the PS1: Start keeps the song the setup made instead of composing it again.
The EXE built with the boot FMV ends 110,608 B below the stack top with and without G2-CO
(0 B resident; the concert overlay group grew by the restructured code). `perf_concert.txt` on
PCSX-Redux: 29.88 fps with G2-CO, as before; `game_smoke` passes.

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

**S2-11 (the house's view turn, the busy Live layout, HomeMart).** Nothing is built here: the turn is built over 2-3
ticks in the GBA's hidden page and its row loops, checksum pass and cursor live in the GBA's IWRAM; the two core hooks
(`plat_build_frame(PLAT_BUILD_BUSY)` in `home_draw`, `plat_ui_loops()` in `mart_draw`) are `!DOOMO_PLATFORM_PS1`, so the
home group is unchanged: on the PS1-FLAT tip (`ee4e053`) and that tree + S2-11, `doomogachi.exe`, `OVL.BIN`,
`TEXT.BIN` and `SFX.BIN` are byte for byte the same (sha256 of both clean builds; `elf2exe` prints image 1,280,680 B,
bss 547,128 B, `_end` 0x801ee3e0, 72,736 B below the stack top with the boot FMV: delta 0 B). The PS1 turns its 3D
camera and never recomposed a room. Measured (PCSX-Redux, `speed2.sh house`, `game_smoke` passes): every house window
30.0-30.5, the busy layout (room-test 1, 4 chibis, 3 animated pieces) **30.00** (2:128 3:1, one 50 ms frame). On the
base before PS1-FLAT it was 28.78-28.89 (8-9 frames of 50 ms, one of 67), and its profile was the 3D room's polygons
(`draw_piece` 15.6%, `g4` 12.2%, `tri` 5.9%, `g3` 5.6%, `room_render` 5.2%, `lit` 4.8%, `tone` 4.8%), not a restore:
the player's 3D model went with the PS1-FLAT package ("HOUSE CHARACTERS FLAT"). `plat_ui_loops` stays the weak
no-op.

**S2-4c (the kitchen as layers over kept pictures).** Nothing of it is built here (`COOK_KEPT` is 0 on the PS1: no
kept picture, no layers, no baked poses; the core's new box and key helpers and the poses' lookup `cook_pose_find` are
`!DOOMO_PLATFORM_PS1`), and the kitchen draws its whole frame as before. On the tips `5aa25cd1` (PS1-RAM in),
`366ee6c1`, `9dc5e39a` and `167c08ba` (S2-4b in), and each + S2-4c: `_end` 0x801df9f0 both times (132,608 B below the stack top with the boot FMV: delta 0 B; on `262bf2b7`, S3-2 in, 0x801dfb10 both times, 132,320 B), `TEXT.BIN`
and `SFX.BIN` the same, `OVL.BIN` and the EXE's overlay table differ (the fishcook group's code: the screens are split
into parts). Measured (PCSX-Redux, `speed2.sh fishcook`, `game_smoke` passes): the kitchen's table is the tip's, row
for row (book, chop, stir, fry, bake, simmer 30.12-30.17 with every frame two vsyncs; the result card 30.00 with one
50 ms frame, its first: the dish's unlocks granted that tick; the counter and serving 30.00 with one 50 ms frame each:
whole faces rendered in one tick, S3-2), and so is the fishcook group's overlay read (313 + 315 ms); on `262bf2b7` (S3-2 in)
the table is again the tip's, row for row.

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

**S2-9 (the workshop).** `weld_gfx` composed a weapon picture in one tick (100 ms: three frames of 100 ms in the
`action_workshop` window, 28.78 fps). `weld_slice.c` (docs/GBA.md "Speed wave 2: S2-9") composes it in slices, one of
350 units (1.5-5 ms) a tick, inside the frame, while the picture shown before stays up, and composes the next ones
ahead. The gate is the core's one slice a tick, not `plat_portrait_slice_ok` alone: its base is the frame's drawing
(`g_perf.cpu_us`), which leaves out the logic tick before it (11 ms in the workshop), so with room for "one more" three
slices ran in one frame and it took 3 vsyncs (cpu 21-25 ms, 32-36 ms from the vsync). A picture takes ~14 ticks. Measured
on the tip it landed on (`d530565`) and that tree + S2-9 (PCSX-Redux, the workshop windows of `speed2_action`): workshop
28.78 (2:126 6:3) -> **30.12** (2:129, every frame 33 ms), part change
30.00 (2:28 3:1: the one 50 ms frame has 18.6 ms of update and draw and exactly 33,433 us (two vblanks) between the
previous vsync and `gfx_begin`: `hcmd_poll`'s lockstep stop (`hcmd.c`: `DrawSync`, serve the harness, one more vblank),
not the game's code; it follows the press when the script moves it, and with the window opened 12 ticks after the press
all 30 frames are 33 ms; on `d530565` most windows of the group have one such frame, with and without this package),
armory 30.17, weld intro 30.17, welding 30.12, as before. The PS1 has no panels (`UI_PANELS` 0) and draws the screen as before: no kept panels, the stat bars keep
their shine, the backdrop its dots. Memory: `weld_slice.c` is in the workshop overlay group (`ovl_plan.py`), the job's
state (~100 B) in the overlay slot's scene scratch (`SCRATCH` 256 B for the group, `WS_SCRATCH` in `weld_slice.c`
asserts that it fits: a bigger state would get no scratch and quietly compose in the call again). No resident byte:
`_end` 0x801ee580 -> 0x801ee570 (-16 B of `.bss`; 72,320 -> 72,336 B below the stack top with the boot FMV; the tip is
1,408 B under the 72 KB gate until PS1-RAM lands). **Cost:** `weld_slice.c` builds `weld_gfx.c`'s recipes a second
time, so the workshop group grows 16 -> 50 KB (85 sections; `OVL.BIN` 1,090 -> 1,114 KB packed) and the workshop's
entry reads its overlay 78 ms longer (`action_workshop` overlay load 315+127 -> 315+205 ms). That read is part of the
scene change, before the windows: left for S2-10's fade over the loads; a smaller copy (only the sliced primitives, or
the plain row code on the PS1) would win most of it back.

**S2-4d (the dreams).** The exact steps of docs/GBA.md "Speed wave 2: S2-4d" are built here: no drawing on a
catch-up tick, the sky's 20 border waves a frame and its covered rows skipped, the rim's bumps made once and its band
rows not filled twice, the caption's cloud box drawing only the bump rows its rects leave showing, the bedroom left out
under the bubble's full disc. `plat_hshapes` and `plat_tile_rows` are the weak defaults, so the circles, ellipses and
the floor take the core's own fills (the same pixels), and none of the GBA-only changes is built (the straight sky
bands, the held choice, the wipe's hardware fade, the vignettes' still faces and lazy chibis, the mirror's ARM blit).
`speed2.sh dream` (its windows now cover every vignette of three dreams, docs/GBA.md) on the landing tip `e597703e`
+ S2-4d against that tip (the same tables on `5aa25cd1`, `249ae551`, `167c08ba`): the 13 shots are byte for byte the tip's; bedroom 30.17, bubble 30.38, Endless Doors cute / twist 30.00 (one 50 ms frame each) -> **30.18 /
30.18** (none), nightmare vignette 2 30.00 (one) -> **30.09** (none), plush dream vignette 1 30.00 (one) -> **30.09**
(none), The Giant Plush 29.92 (two 50 ms frames) -> **30.00** (one); vignette 2, The Mirror, nightmare 1 and 3, the
choice and Cloud Concert 30.00 on both, with one 50 ms frame at the window's start (`hcmd_poll`'s lockstep stop, see
S2-9 above); the choice's first ticks 28.24 on both (two 50 ms frames). No frame over 50 ms in any window. Earlier, on
`ad06012` with the old windows: the two choice windows 29.85 / 29.75 (a 67 ms frame each) -> 30.00, the last
vignette's start one 67 ms frame on both builds (the red nightmare is 29-31 ms of CPU a frame here, `perf:` lines).
`game_smoke` passes. Memory: no resident data; the EXE's code 32 B smaller (`_end` 0x801e0610 -> 0x801e05f0,
129,504 -> 129,536 B below `elf2exe`'s stack top with the boot FMV, on the landing tip `8fe2134f`; the same
32 B on `5aa25cd1` and `e597703e`); the dream overlay group 39 -> 42 KB (its load 182
-> 186 ms, measured on `ad06012`; 181 -> 195 ms before the bedroom window in this run).

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
**S3-1 (the sim's 15-minute pass).** The core change (docs/GBA.md "Speed wave 3: S3-1": the resident pass is one resident a
tick over the 15 game minutes, not 32 residents in 32 ticks every 15 minutes) is shared by the PS1. The R3000 hid most of
the old pass: in the new `sim` group (`speed2_sim.txt`, four 1,200-tick windows with a pass inside each; PCSX-Redux, the
disc before and after on `cb7d48d9`) the shop gifts tab, the citizen picker and the pet already held 30.0 with no frame over
33 ms; the island hub had 4 frames of 50 ms (29.96 fps), now **30.00 with none over 33 ms**. The `life` group on the first cut
of the package: every window 30.0 but the new island load, the save freeze (S2-10) and the care reaction (2 frames of
50 ms). `game_smoke` passes. **PS1 resident: 16 B less.** The first cut's +4 B of text cost 16 B of image (the base's text
ended on the 16-byte boundary where `.rodata` starts). The package's code is now -12 B (`.text` 711,332 -> 711,320 B on
`cb7d48d9`): `minute_step` reads the event-rate option once, the activity is stored without the compare, `sim_edge_remove`
was a copy of `town.c`'s `remove_edge` and is `rel_remove_at` now, and `update_resident_ex` takes the fewest game minutes an
update may cover (`SIM_PASS_MIN` for the pass, `SIM_MIN_ELAPSED` for a fast-forward chunk, which also chooses the routine
again) instead of a flag and a conditional line (-16 B). The EXE with the boot FMV ends at `0x801ee560` instead of
`0x801ee570` on `cb7d48d9`: 72,336 -> 72,352 B below the stack top; on the tip it landed on (`ad060127`, after CR-BODY's own
-16 B) the same 16 B: `_end` 0x801ee560 -> 0x801ee550, 72,352 -> **72,368 B** (the 72 KB gate, 73,728 B, is the PS1-RAM
package's; this package takes 16 B off the shortfall). The halfword edge move is GBA-only (`DOOMO_PLATFORM_GBA`); the PS1 keeps `memmove`.

**S2-4b (the other fair games, 2026-11-13).** Nothing is built here either: stamps, bands and undo lists are GBA-only (`mg.h`:
`MG_BAKED(f)` is NULL, the baked-only functions are under `#if !defined(DOOMO_PLATFORM_PS1)`, the macros are `(void)` stubs), so
every game draws as before; `MgBox` is declared everywhere, and `mg_mod6` (the Derby crowd's `x % 6` without a division, the
same values) does the crowd's arithmetic here too. **Resident RAM: +0 B**, found the hard way: the first build of the package was
16 B over the base (`_end` 0x801efe70 against 0x801efe60), because `mg_mod6` kept a 6-byte `static const` table, and a static
const in a minigame file lands in the resident `.data` (the overlay group moves code, not the data). It is now arithmetic;
`_end` is 0x801efe60 as on the base (`411d9812`), 64 KB below the stack top with the boot FMV, the overlay slot unchanged (168 KB).
PCSX-Redux `speed2_fair` (23 windows): the fps and the vsync histograms are those of speed audit 3 in every window, i.e. the
title cards, Whack, Dodge, Memory and Candy Catch at 30.12-30.17 with every frame at 2 vsyncs, the Gallery, the Derby and the
Wheel at 30.00 with one 3-vsync frame each (as in speed audit 3); `game_smoke.txt` passes. **Landing (2026-11-15, on `41df852`, with PS1-RAM and the Talk and creator packages since; the same on `366ee6c`, `5aa25cd` and `cb9df1c`):**
`_end` 0x801df9f0, the same as the tip's (both built, the image 1,247,068 B in both): **132,608 B below the stack top with the boot FMV,
+0 B** (before PS1-RAM, on `b9e74a5`, `2bc6130` and `0cce022`: 0x801ee550 = the tip's each time, 72,368 B). The results card's hold that came with the landing is GBA-only (`mg.c` builds it under `#if
!defined(DOOMO_PLATFORM_PS1)`; `ui_modal_hold` is a macro that returns 0 here). `speed2_fair` now has 30 windows (a results window for
every game, Candy Catch's moved onto its card): every one 30.00-30.17, the six results cards 30.00-30.17 (the Gallery's and the
Derby's with the one 3-vsync frame their other windows have), `memory play sweet late` 30.15; `game_smoke.txt` passes.

**S2-10 (saves, loads and town builds with a live screen; built 2026-11-13, landed 2026-11-15, partial).** The PS1's
free-play save was one long frame: the memory card write (PCSX-Redux's OpenBIOS: about 40 ms a 128-byte sector) plus
a header peek of a whole 8 KB block after it. Title -> New story was frames of 2.4 s and 1.2 s (the town build, seven
overlay reads of 314-370 ms), a new island one 1.9 s frame. The work still runs in one tick; the screen no longer
sits still (`plat_ext.h` "S2-10"):

* **The loading screen follows the work.** `plat_busy(kind)` / `plat_busy_step(done, total)` (`gfx.c`) redraw the
  full-screen loading screen with its bar at done / total, at most every other vblank and only after the work has run
  three vblanks (a quick job shows nothing). The core calls them per citizen in `save_game`'s encode, `load_game`'s
  decode, `town_new`, `sim_init`, and per pass of `rel_normalize` (that step and the relationship runs pass
  `PLAT_BUSY_KEEP`: the bar stays where it is, so it only moves forward within a pass of the work; a load checks, then
  applies, so its bar fills twice). A town build says "Getting everything ready..." without the memory card warning.
* **Card calls two sectors at a time** (`memcard.c`, `CALL_FRAMES`): a write or read with the loading screen up moves
  two 128-byte frames per BIOS call and shows a picture after each (the bar used to move once per 8 KB block, once a
  second). One sector a call was 34% slower overall (the BIOS's per-call cost); two sectors a call make the save
  window **14% longer in total** than one 8 KB block a call (883 vsyncs against 775 on `ee686c74`, +1.8 s), with
  pictures 70-100 ms apart. A **peek** (the 48-byte save header a menu asks for, the config record) reads the 6-7
  frames it needs, not the 64 of a block: the 2.3 s the Save/Load screen's refresh took after a save is about 10x
  less (and every title and menu peek with it). The line under the title says "Writing..." / "Reading..." now
  ("block N of M" went). **Unverified on a console:** 32 times more BIOS card calls per block, measured on
  OpenBIOS only.
* **A spinner over the overlay reads** (`gfx_load_fade`): once the SP5 fade's three darkening passes are done and the
  drive is still reading (a scene change loads up to three groups; a New story reads seven, 2.4 s), a ring of eight
  dots turns on the dark picture every other vblank.
* The harness counts the pictures the game shows outside its loop (`g_busy_pics`: fade passes, spinner steps, loading
  screens) with the loop's frames (`run.py` `HARNESS_PICS`, `harness.lua` `read_shown`), so `fps` and `gaps` see a live
  screen. The "before" column counts loop frames only (the old disc has no `g_busy_pics`).
* The busy banner (`gfx_busy_banner`, the config record's "Saving settings...") keeps its pixels in the other
  framebuffer since PS1-RAM step 3; the loading screen and the spinner draw into the framebuffer shown, never the
  other one, so the two do not meet.

| window (`speed2.sh busy`, PCSX-Redux; before on `d3e63e9d`, after on `81542c47`, the same as on `d3e63e9d`) | before | after |
|---|---|---|
| title -> New story | 8.4 fps, frames of 2,483 and 1,200 ms | 23.8 fps, worst picture gap **383 ms** |
| new island -> hub | 16.4 fps, one 1,900 ms frame | 26.6-26.7 fps, worst gap **183-200 ms** |
| save, a Classic island (16 KB, 3 blocks) | 3.8 fps, one 9,667 ms frame | 16.9 fps, worst gap **367 ms**, 64 pictures 100 ms apart through the write |
| load, a Classic island | 6.0 fps, one 8,383 ms frame | 21.1 fps, pictures 67 ms apart, worst gap **767 ms** |
| save / load, the "worst-case" windows (the same island: `town-fill` is GBA-only) | 9,467 / 8,367 ms | 400 / 767 ms |

**Not met** (the package's goal is no gap over ~100 ms): what is left are single blocking calls the game cannot
slice: the config record's write after a save (367-400 ms, the "Saving settings..." banner), the directory scan of an
`open` at the start of a load plus the scene's enter after it (767 ms; 683 ms on `ee686c74`), and the scene-enter
work after a build (183-383 ms). They are one call each in OpenBIOS; on a console the BIOS is faster but not
measured. A VSync-callback spinner (drawing from the vblank IRQ while the BIOS call blocks) would cover them; it is not
built (the card driver is touchy about its interrupts, `memcard.c` top). "fps" counts the pictures: the sim is
stopped for the whole job. `speed2.sh story life`: the same New story and new island numbers (383 / 200 ms), the save
freeze 400 ms, Save & Load and its cursor 30.2 / 30.5 fps.

**Same bytes.** The 128 KB memory card image of a deterministic island save (seed 1, a tick-exact script, `run.py`) was
sha256-identical (`5b89b9d2...`) between the old and the new disc on `ee686c74`; `memcard_check.py` reads it
(`BASLUS-DOOMO1B`, 2 blocks, payload 9,718 B, CRC `1fc29482`). The card writes the same bytes in the same order; only
the call sizes differ. On the landed tree `game_smoke` writes and `memcard_check.py` reads its card. **Power cut:** not
tested on the PS1; `write_file` rewrites a file in place when it is big enough (unchanged by S2-10, not atomic).

**Resident RAM: +496 B.** On the tip it landed on (`91479d4c`, after S2-4c; the same on `6e8363ab`) the disc with the boot FMV ends at
`0x801e0420`, 130,000 B below the stack top; with S2-10 at `0x801e0610`, **129,504 B** (image +480 B, BSS +16 B),
55,776 B above the 72 KB plan gate. The same +496 B on `167c08ba`, `61728c76`, `81542c47` and `d3e63e9d` (132,608 ->
132,112 B); +512 B on `262bf2b7` (132,320 -> 131,808 B: the image's 16-byte alignment). (On
`ee686c74` the package was 320 B *smaller* than its base: it merged the banner's and the loading screen's glyph arrays,
which PS1-RAM step 3 did on its own since.) The cold busy code is compiled `-Os` by attribute (`COLD` in `gfx.c`).
**G2-EX (the exorcism toward 30; built 2026-11-13, landed 2026-11-15).** The PS1 held 30 before and holds it after:
`perf_exorcism_phases.txt` reads 30.00 fps in the intro, tools, prayer, holy water, true name, verdict and result phases on
the tip and on the tip + G2-EX (every frame 2 vsyncs but one 3-vsync frame per window, the lockstep resume; the histograms
identical), and the seven phase shots are byte-identical to the tip's. That is why the PS1 does not take the package's two
pixel-changing steps (the wall drips' hold, the host's and Fuzz's held pose: `EXO_HOLD_STILL` 0 under
`DOOMO_PLATFORM_PS1`, `core/src/exorcism_int.h`; SPEED DEFAULTS allow a pixel change only where the exact steps cannot
reach 30). The shared changes (clipped redraw in `backdrop.c`, the verdict and result in layers) cost nothing here: the
platform hooks `plat_clip` / `plat_clip_end` and `plat_stage_*` are the weak no-ops (no retained stage on the disc), the
aura's list is not built (`HOST_HOLES` 0: the shapes are drawn whole), and the exorcism overlay group is 51 KB (was 46; the
slot is 180 KB). `perf_exorcism.txt` 30.00, `game_smoke` and the core's `life_exorcism` pass. **Resident +0 B.** The
first build was +912 B: the host and demon layers chose between two static declarer functions by a pointer, which made
their addresses escape and the planner (`ovl_plan.py`: "a section whose address escapes is resident") kept both,
`stage_layer`, `stage_draw`, `stage_layer_clip` and `layer_set` (792 B of code) resident. `LX(clip, ...)` is one function
that every caller calls directly (`OVL_WHY_OBJ=exorcism_scene` shows the chain if it comes back). On the landing tips
`7cda6362` and `cf5e9f27` the disc with the boot FMV ends at `0x801e0610` with and without G2-EX: **129,504 B below the
stack top, delta 0 B** (built with `-DDOOMO_INTRO_REQUIRED=ON`; the image and bss sections the same size, `OVL.BIN`
1,144 -> 1,146 KB), 55,776 B above the 72 KB plan gate.

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
## LV4 (mechanic kit B, environment and light): compiled out

`core/src/levels_mech_env.c` (fog, tide, lantern, candles, faces, showtime, collapse, dissolving rooms) is empty on
the PS1 (`#if !defined(DOOMO_PLATFORM_PS1)`), and `lv_runtime_view()` does not fill `DoomoView3D.view_dist` there
(`LV_HAS_NEW` is 0): the PS1 EXE is unchanged (no resident byte). `ps1/src/view3d.c` ignores the new field and
keeps its governor's `s_view_dist`. When the LEVELS share of PS1 memory is released: drop the `#if`, take
`min(s_view_dist, view_dist)` in `cast()`, and put `levels_mech_env` in the combat overlay group
(`ps1/tools/ovl_plan.py` matches `levels_*` already).

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
(10 horns, 16 tails, 14 wings, 4 halos, 16 auras) are part scripts the PS1 does not read yet (the part VM was off until CR10 (history: it is on since CR10, they now draw and are offered)):
their catalog rows are creator-overlay data and all are age class ALL (no new age row). The creator offers the PS1 only the
painted choices (`part_live_drawable`: Tail 1/16, Aura 1/8), as for every scripted part since CR3. Harness (PCSX-Redux):
`game_smoke`, `creator`, `creator_grid` and the new `creator_parts_spooky` PASS; `creator_speed` idle 30.13, grid scroll
29.39, rows 30.13, value 29.88 (the S3-3 numbers: nothing resident changed).

## CR-BODY (2026-11-13): body types and the slider test

Measured on the tip `9f52eb14` and the tip + CR-BODY, 2026-11-15 (`cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`,
`cmake --build --preset ps1 -j2`): `_end` is 0x801ee560 with CR-BODY, 72,336 B below the stack top (`0x1FFFF0` minus `_end`) with the boot FMV;
the elf2exe line prints "70 KB left". CR-BODY itself is 16 B LESS resident (measured twice: on `ee4e053` 72,720 -> 72,736 B, on `d5305653`
72,304 -> 72,320 B). The 72 KB plan gate (73,728 B) is NOT met: the margin is 1,392 B short of it, inherited from the tip (other
streams moved it; it stays so until SPEED PS1-RAM). The first
version of the step (a function plus a seven column table) was +96 B; the head size step is now a 16 byte array read inline
in `sanitize` before its existing clamp, and the body table is back to six one-byte columns (96 B, was 120 B as int16).
The 6 new body types are table rows (method P, age class ALL: no new age row); their catalog rows and icons are overlay
data. Harness (PCSX-Redux): `game_smoke`, `creator`, `creator_grid`, `creator_parts_spooky`, `creator_speed` (idle 30.13,
grid scroll 29.39, rows 30.13, value 29.88: the S3-3 numbers) and the new `creator_sliders` (11 sliders, 16 bodies) PASS.

## CR10 (2026-11-15): the part VM and the part store

The PS1 now draws and offers every scripted part (A1a faces, A2 hair, A3 spooky, A4 fur and marks, A5 outfit): `DOOMO_PARTS_VM`
is 1 (it was 0 since CR2), `part_live_drawable` is the live count, and the deep look pool is on (`LOOK_POOL_SAVED` 1, the 384 B pool).
A look made on the GBA shows the same on the PS1 (same gene, same scripts, the same renderer).

**Where the data lives.** Nothing of the scripts or the icons is linked into the EXE (`DOOMO_PARTS_SPU`):
* `ps1/tools/partsbin.py` writes **`PARTS.BIN`** (71 sectors, 145,408 B): sector 0 is a header (`"PRTS"`, the number of
  sets, the data size, then an offset and a length per set and for the icons), then every set's raw scripts
  (`core/gen/parts_<dir>.c` `ops[]`, 19,639 B) and the packed part icons (`doomo_part_icon_rle[]`, 121,577 B), each
  starting on a 64-byte boundary. No LZ: the PS1 keeps no decoder and no block slots.
* `src/partstore.c` (`partstore_boot()`, called after `audio_hw_sfx_preload()`) reads it in 32 KB chunks through the
  display lists' scratch (idle until the game loop, like the effects' staging) and uploads it to SPU RAM at **0x50000**
  (`SPU_PARTS_BASE`; the effects stop below it, `SPU_SFX_END`). It reads the first bytes back at an odd offset and logs
  `parts: loaded, 9 sets + icons, 143360 bytes at spu 50000` (`game_smoke` expects `parts: loaded`; a missing or damaged
  PARTS.BIN logs `parts: MISSING` and every scripted part draws nothing, the grid shows "?").
* **The audio lock.** After boot nothing writes SPU RAM (every effect is converted before the first game frame, spu.c
  "staging"), so a part read never races an upload; the engine-test build does not load the store.
* `plat_part_read(set, ofs, dst, n)` is one SPU DMA read of at most 320 B (the 256 B entry window, aligned down to
  8 bytes, rounded up to the 64-byte blocks). Measured on PCSX-Redux: 84 us a read, about 10 reads for a render of a
  many-part look, so under 1 ms of a render; `core/src/part_lru.c` `part_store_ops()` keeps each entry in a **2 KB
  per-render arena** (20 entries; a worst look, the longest script of every list worn at once, fits with no spill: a
  test) and falls back to a 256 B window, valid until the next read, when the arena is full.
* `plat_part_icon_read(ofs, dst, n)`: `part_icon_unpack()` reads one packed icon (at most 256 B, `PART_ICON_PACK_MAX`,
  the icon tool refuses a longer one) onto the stack and unpacks it into the grid's cell buffer as before. The
  creator group keeps only the icon tables (`doomo_part_icon_rle_rows` / `_ofs`, 769 icons). **The packed set is now
  every icon** (it was the 404 painted parts' icons): the grid shows every part's picture, no "?".
* The host tests build the same code (`DOOMO_PARTS_SPU_TEST`): `PART_OPS` and the icon read go through the store (its
  "SPU RAM" is the linked raw array), so the whole portrait and icon suite of `make -C core test` is the proof that
  drawing through the store is the same picture; three tests check the store itself (entries equal the raw scripts,
  the arena keeps an entry once per render and spills when full, a missing store draws nothing).

**Memory** (clean builds with the boot FMV, `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`; base = the tip
`e587f9c1`):

| | base `366ee6c1` | with CR10 | change |
|---|---:|---:|---:|
| EXE ends (`_end`) | 0x801df9f0 | 0x801e4c10 | +21,024 B |
| **below the stack top** (elf2exe) | **132,608 B** | **111,584 B** | **-21,024 B** |
| overlay slot | 131,072 B | 131,072 B | 0 |
| creator group (`.ovl_03`) | 184,248 B, 600 B under the slot's end | 121,824 B, **63,024 B** under it | -62,424 B |
| `OVL.BIN` | 1,234,944 B | 1,134,592 B | -100,352 B |
| `PARTS.BIN` (new, on the disc) | - | 145,408 B | +145,408 B |

The resident +21,024 B, by object (map diff): the part VM `portrait_script.c` 9,380 B (code, `-Os`), the painters' scripted
paths in `portrait_head` / `portrait_hair` / `portrait_body` / `portrait_fx` 3,560 B, the PartRef tables (refs, lists,
stamps, the registry) 2,150 B, `part_lru.c` 3,088 B (the 2 KB arena, the 256 B window, 20 entry slots, 400 B of code),
`partstore.c` 1,836 B (a 320 B read buffer, the set bases, the boot loader), the look pool's put / load / raw glue
(`look_pool.c` 596 B and the resident part of `creator.c`, together about 970 B), the rest (`cutview.c`, `part_icons.c`)
130 B. **CREATOR's PS1 share was 4 KB; the coordinator approved 21 KB (21,504 B) for CR10
(PARALLEL_WAVES.md section 3): this package uses 21,024 B**, 480 B of the share left (224 B since CR-SLIDERX added 256 B); the margin is 111,584 B below the stack
top, 37,856 B above the 72 KB gate (73,728 B).
**Why none of it is in an overlay group.** The part VM, the painters' scripted paths and the PartRef tables are used by the
portrait renderer, and `ps1/tools/ovl_plan.py` leaves `portrait*.c` and `part_lru.c` outside every group (so they are resident),
for two reasons checked in the code: (1) portraits are drawn in almost every scene, not in one: callers are in `combat_hud.c`,
`concert_scene.c`, `cook_scene.c`, `dream_gfx.c`, `exorcism_scene.c`, `mg_draw.c`, `studio.c`, `story_feast.c`, `cutscene_ch12.c`
and in resident code (`ui.c`, `ui_dialog.c`, `fp_common.c`, `fp_resident.c`), so the code would have to be in every group or
in the slot all the time; (2) the slot (131,072 B) holds one group at a time and a scene entry reads the group from the disc,
so a renderer that sits in a group would be swapped out by every scene change and reloaded just to draw a face in the HUD
or a dialog. The VM's code and the painters' hooks alone are 12.7 KB. **What could move later:** only the creator-only pieces (the resident part of `creator.c` and the look pool glue, about
970 B, and any PartRef table that only the creator's lists read, to be checked first); the VM, the painters and the arena
stay resident. Anything moved goes back to CREATOR's reserve in PARALLEL_WAVES section 3. What this package did to keep the rest small: the scripts
and icons are raw bytes in SPU RAM, not in the EXE (linking only the LZ blocks of the scripts was +14.7 KB, the
LRU 9.5 KB: the first, naive build with the VM on was 37,088 B); the arena is 2 KB, not the LZ design's 8 KB.
**Every later part batch adds 4 B of PartRef to the resident tables per part** (the scripts and the icons cost no RAM:
SPU RAM holds 184,320 B (0x50000 to the reverb area), 143,360 B used, so **only about 190 more parts of script and icon fit**
(a part is about 55 B of script and 155 B of packed icon): the icons, 86 % of the file, need their own disc file read
per section (`ICONS.BIN`, CREATOR_PLAN CR7) before the next big batches; that is the next PS1 creator package).

**Speed** (PCSX-Redux, 30 Hz): `creator_speed` idle 30.13, grid scroll 30.00, rows 30.13, value 29.88 (the base's: 30.13 / 29.39 / 30.13 / 29.88); the speed tour's `creator` group (25 windows) all the same fps as the base (the creator's load 314+685 -> 313+362 ms: its group is 62 KB smaller);
`game_smoke` 7 of 7 shots byte-identical. The new `creator_parts.txt` holds the cursor on scripted parts of six grids
(100 frames each): head shapes 29.85, eyes 29.85, horns 30.00, tail 30.00, outer layer 30.00, **hair 27.52** (14 frames
at 50 ms; the same moves on the base disc, painted hair only, were 21.9 with 70 frames at 50 ms). A hair is the
biggest picture the creator draws; lifting it to 30 is the whole-portrait-in-a-frame work of S3-2.

**Visible changes** (the speed tour's shots that differ: the lists offer more parts, e.g. "Shape Round 1/12" is now
"1/24", "Horns None 1/16" "1/26"; the part grids show the pictures of the new parts; the citizen in the preview wears
scripted parts); every other shot is byte-identical. Contact sheets: the harness shots of `creator_parts_*` against the
native runner's (the same parts, the same look; the PS1's shading differs by the 2-4 % of section 10).

**Harness:** `creator_parts.txt` (PS1: held cursor on scripted parts, fps floors 28, hair 26), the runner's
`creator_parts_face_a|hair|spooky|fur|outfit`, `creator_deep`, and the new `creator_deep_save` (a Lashes value through
Move in, a save and a load on the memory card, the citizen reopened: it still wears it) are in the `harness` target.

**Left:** the deep batches (hair page 2, spooky accessories, outfit page 2, A6-A9) are scripts plus a `DF_` id and a
painter hook each; the PS1 pool is 384 B (about 40 short runs): a save written by a console with a bigger pool loads here
with an empty pool (the nicknames and gene bits stay); SPU RAM has room for about 190 more parts (above); the 21,024 B against the 4 KB share (the coordinator releases the
rest of the reserve or CR-UI / SPEED find it elsewhere); hair try-on 27.5.

## RAM headroom 2026-11-12 (PS1-RAM)

**Tip 2026-11-17 (CI on `5a08784`):** the disc ends at `0x801e86e0`, **96,528 B below the stack top**, overlay slot 133,120 B (studio group 131,488 B, the biggest), 22,800 B above the plan gate. The 128 KB figures below are older than that tip; read the margin from the latest ledger. A trial with 6 Advanced song pages on the PS1 read 32,768 B (docs/STUDIO.md "ST-ADV: PS1 pages").

On `89a28b56` the game disc with the boot FMV ended **65,936 B below the stack top** (65,952 B on `e89f329`):
416 B above the build's hard limit and 7.8 KB under the 72 KB plan gate. Two packages had already hit that
wall. This package gives resident memory back without touching the game's logic or a pixel: the disc now ends
at `0x801dff00`, **131,312 B below the stack top** (128.2 KB, +65,376 B). That is 57,584 B above the plan gate,
33,008 B above the 96 KB aim and 240 B above the old 128 KB goal (131,072 B). The overlay slot is unchanged
(131,072 B). Every PS1 window that held 30 fps still does (tables below). Steps 1 to 6 alone ended 161,312 B
below the stack top; step 7 gives 30,000 B of that back on purpose, because they would have made three scene
loads longer.

The ledger, the steps and the checks below were measured on the tree of `4c0f369e` (the base plus this package).
Rebased on `0df72845` (CR3's gene code and its creator string pool, Talk T5B-OPEN, DC6A) the disc ends at
`0x801e0640`, **129,456 B below the stack top**: those packages add 1,856 B of resident code and data
net (mainly `look_gene` 1,544 B, `unlocks_creator` 1,388, `portrait` 412 and `resident` 216, less the 1,916 B of
`creator_data` that CR3's `-G0` takes out of the resident image: its `.sdata` name lists, which no string
section can move). That is 55,728 B above the plan gate and 1,616 B short of the old 128 KB goal. On the
newest tip, `f91bd3e4` (LV0, STKOPT and the Talk and DC work on top of that), the disc ends at `0x801e0a20`:
**128,464 B below the stack top** (core tests 1153 passed, `game_smoke` passes with the same SPU line).

**Landed (2026-11-15) on `b9e74a51`** (PS1-FLAT, S2-11, S2-9, S3-1, CR-BODY, CR5, the CR4, LV, Talk and DC landings
and the owner's Talk and GBA commits of 2026-11-14; builds of both, `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`):
the tip alone ends at `0x801ee550`, 72,352 B below the stack top (1,376 B under the plan gate); with the package it
ends at `0x801df9f0`, **132,608 B below the stack top** (+60,256 B). That is 58,880 B above the plan gate, 34,304 B
above the 96 KB aim and 1,536 B above the old 128 KB goal. The overlay slot is still 131,072 B. The package was
checked on `d5305653` (72,304 -> 132,592 B), on `3d9905df` (S2-9: 72,320 -> 132,608 B), on `249a0282` (the same
layout), on `ad060127` (CR-BODY: 72,336 -> 132,608 B; its 16 B are 8 B of `.rodata` and alignment in the package
build), on `5879aa3f` (S3-1, CR4 A5, TM2c: 72,352 -> 132,624 B) and last on `b9e74a51` (DC9B, TUI-TALK, DC6D and
CR5 on top: CR5's 8 B of code cost the package build 16 B of alignment). The layout of the landed tree is the last table under "Ledger", its checks the last items under "Checks".

### The gate and the ledger tools

* `tools/elf2exe.py` prints the exact margin on every build: `elf2exe: N B left below the stack top 0x801ffff0
  (fails under 65520 B, plan gate 73728 B)`. Under the 72 KB plan gate it also prints `elf2exe: WARNING: only N B
  left below the stack top: M B under the 72 KB plan gate ...` (on GitHub Actions a `::warning` annotation too)
  and still passes. Under 64 KB it fails as before. `--gate BYTES` moves the plan gate.
* `tools/ram_ledger.py build/doomogachi.ldmap --ld build/ovl.ld --readelf mipsel-linux-gnu-readelf --elf
  build/doomogachi.elf [--objs N] [--syms N] [--md] [--gate BYTES]` reads the linker map and prints the ledger
  below: the layout, the biggest resident objects (code, const data, string literals, `.data`, `.bss` apart;
  the linker merges all resident string literals into one block, an object's "strings" are its share of it),
  the overlay groups, and the biggest resident symbols.
* CI (the PS1 job): "PS1 build tool tests" runs `tools/test_tools.py` (the gate, the ledger, the string splitter
  and the planner rules below on made-up inputs) and the host test `harness/tilerow_test.c`; "PS1 RAM ledger"
  writes the ledger into the job summary, next to "Biggest PS1 statics".

### Ledger

Layout, `89a28b56` against this package (the game disc with the boot FMV):

| what (bytes) | before | after | change |
|---|---:|---:|---:|
| resident `.text` (code) | 713,292 | 704,320 | -8,972 |
| resident `.rodata` (const data) | 569,920 | 539,772 | -30,148 |
| of it: the merged string literals | 108,748 | 81,304 | -27,444 |
| `.data` + `.sdata` + ctors / dtors | 4,136 | 4,140 | +4 |
| `.sbss` + `.bss` | 493,448 | 467,180 | -26,268 |
| `.borrow` (the `.bss` the house and creator groups load over) | 53,776 | 53,776 | 0 |
| overlay slot | 131,072 | 131,072 | 0 |
| the EXE ends at | `0x801efe60` | `0x801dff00` | |
| **left below the stack top** | **65,936** | **131,312** | **+65,376** |

(The rows add up to 65,384 B; 8 B are the sections' alignment.) Where the freed bytes came from, by object
(resident total before -> after; an object's total counts its share of the merged strings, so the objects add
up to a little more than the layout does): `ps1/src/vram` 35,217 -> 26,297, `ps1/src/gfx` 152,386 -> 143,852,
`ps1/src/spu` 12,556 -> 4,116, `fishcook` 7,152 -> 1,596, `catalog` 12,756 -> 7,284, `creator_data` 6,772 -> 1,916,
`lib_weapons` 4,269 -> 61, `fish_data` 3,184 -> 8, `ui` 24,280 -> 21,420, `levels` 2,949 -> 897, `song_player`
14,842 -> 13,194, `lib_common` 2,476 -> 904, `dream` 1,768 -> 224, `cook_data` 1,500 -> 0, and 35 more files
with under 1 KB each (9 KB together, mostly their table strings). The ledger of the final build
(`tools/ram_ledger.py`, 24 objects and 24 symbols):

**The 24 biggest resident objects**

| object | total | .text | .rodata | strings | .data | .bss | borrowed .bss |
|---|---:|---:|---:|---:|---:|---:|---:|
| build/packed/packed_art | 193658 | 0 | 185816 | 0 | 14 | 7828 | 0 |
| ps1/src/gfx | 143852 | 7524 | 0 | 196 | 6 | 136126 | 0 |
| core/gen/roster_data | 137698 | 0 | 137696 | 0 | 2 | 0 | 0 |
| core/src/town | 95844 | 5776 | 124 | 184 | 0 | 89760 | 0 |
| core/src/ui_raster | 60736 | 22536 | 1240 | 32 | 12 | 36916 | 0 |
| core/src/portrait | 43662 | 19760 | 128 | 0 | 29 | 23745 | 0 |
| core/src/portrait_head | 41496 | 40404 | 984 | 0 | 24 | 84 | 0 |
| core/src/portrait_body | 39324 | 38528 | 592 | 0 | 8 | 196 | 0 |
| core/src/map | 37704 | 3916 | 72 | 220 | 36 | 33460 | 0 |
| core/src/portrait_hair | 32312 | 31476 | 804 | 0 | 28 | 4 | 0 |
| core/src/weld_gfx | 28412 | 27344 | 964 | 48 | 4 | 52 | 0 |
| ps1/src/world | 26356 | 1228 | 12 | 0 | 0 | 24 | 25092 |
| ps1/src/vram | 26297 | 5620 | 0 | 0 | 4 | 20673 | 0 |
| ps1/src/view3d | 26209 | 9372 | 20 | 0 | 14 | 419 | 16384 |
| ps1/src/txtload | 24852 | 1392 | 0 | 148 | 12 | 23300 | 0 |
| core/src/weld | 22480 | 10932 | 4748 | 6760 | 40 | 0 | 0 |
| core/src/ui | 21420 | 20732 | 148 | 248 | 10 | 282 | 0 |
| core/src/family | 21058 | 18524 | 72 | 1580 | 54 | 828 | 0 |
| core/src/portrait_gfx | 20805 | 13400 | 2184 | 0 | 0 | 5221 | 0 |
| core/src/fp_common | 18054 | 13680 | 592 | 1788 | 76 | 1918 | 0 |
| core/src/sim | 17604 | 11028 | 716 | 824 | 4 | 5032 | 0 |
| core/src/actor | 17121 | 8820 | 724 | 784 | 4 | 6789 | 0 |
| ps1/src/r3d | 16149 | 15172 | 24 | 0 | 0 | 953 | 0 |
| core/gen/mus_data | 15784 | 0 | 15784 | 0 | 0 | 0 | 0 |
| (276 more) | 645438 | 377156 | 105016 | 73916 | 3676 | 73386 | 12288 |

Layout of the landed tree, `b9e74a51` against `b9e74a51` with this package (`tools/ram_ledger.py`, boot FMV
linked; its 24 biggest symbols are the ones above and its biggest objects within a few hundred bytes of them,
`ps1/src/gfx` 152,386 -> 143,852 and `ps1/src/vram` 35,217 -> 26,297 as on `89a28b56`):

| what (bytes) | before | after | change |
|---|---:|---:|---:|
| resident `.text` (code) | 711,328 | 702,356 | -8,972 |
| resident `.rodata` (const data) | 565,740 | 540,696 | -25,044 |
| of it: the merged string literals | 104,244 | 81,916 | -22,328 |
| `.data` + `.sdata` + ctors / dtors | 3,984 | 3,988 | +4 |
| `.sbss` + `.bss` | 493,332 | 467,072 | -26,260 |
| `.borrow` | 53,776 | 53,776 | 0 |
| overlay slot | 131,072 | 131,072 | 0 |
| the EXE ends at | `0x801ee550` | `0x801df9f0` | |
| **left below the stack top** | **72,352** | **132,608** | **+60,256** |

(The rows add up to 60,272 B; 16 B go to the sections' alignment here, on `d5305653` and `3d9905df` 16 B came from it.) Fewer strings move than on `89a28b56`
(-22,328 B against -27,444 B): CR3 already took the creator's string pool out of the resident image.

**The 24 biggest resident symbols**

| symbol | bytes | kind | object |
|---|---:|---:|---:|
| s_prim | 108544 | bss | ps1/src/gfx |
| ps1_pack_spr | 108543 | rodata | build/packed/packed_art |
| doomo_town | 89504 | bss | core/src/town |
| ps1_pack_tex | 64428 | rodata | build/packed/packed_art |
| doomo_roster_pack | 58000 | rodata | core/gen/roster_data |
| doomo_roster_bio_text | 44931 | rodata | core/gen/roster_data |
| doomo_map | 32780 | bss | core/src/map |
| s_fb | 24936 | bss | ps1/src/gfx |
| pool | 18432 | bss | core/src/portrait |
| g_world_corner | 16900 | borrow | ps1/src/world |
| si_pool | 16384 | bss | core/src/ui_raster |
| doomo_mus_bank | 15784 | rodata | core/gen/mus_data |
| pl_bandlut | 12675 | rodata | core/src/portrait_lut |
| pb_draw_body_part | 12608 | text | core/src/portrait_body |
| compose | 12468 | text | core/src/weld_gfx |
| block.0 | 12288 | bss | core/src/scene_scratch |
| win_e | 12288 | bss | ps1/src/txtload |
| ph_face_part | 11232 | text | core/src/portrait_head |
| ps1_pack_sky | 11001 | rodata | build/packed/packed_art |
| sc_pool | 10240 | bss | core/src/ui_raster |
| doomo_roster_name_text | 9431 | rodata | core/gen/roster_data |
| doomo_roster_edges | 9258 | rodata | core/gen/roster_data |
| doomo_lang_es_res | 8260 | rodata | core/gen/lang_res_es |
| g_world_wtop | 8192 | borrow | ps1/src/world |

### The steps

The margin of each row is the `elf2exe` line of that commit's build. The last column is PCSX-Redux on the scenes
the step could slow (`ps1/harness/scripts/perf_*.txt`, `options_volume.txt`, `fair_cache.txt`; the whole tour
and the whole harness are in "Checks"): steps 1 and 2, 3 and 3b, and 4 and 5 were measured together, on the
builds of steps 2, 3b and 5, and every number is the base's unless a difference is named.

| step | what | freed (B) | left below the stack top (B) | scenes it could slow, base -> this build (PCSX-Redux) |
|---|---|---:|---:|---|
| 0 | `89a28b56`, before the package | | 65,936 | |
| 1 | island and free-play callbacks go to their groups (`ovl_plan.py`) | +21,808 | 87,744 | measured with step 2 (below) |
| 2 | table strings in sections of their own (`strsec.py`) | +29,376 | 117,120 | island 29.88 / 30.00, title 30.13, dialogue 29.67, creator 30.13: all as before; CPU per frame 21.1 ms against 21.0. Loads: free play 314 -> 340 ms, island 207 -> 229, cutscenes 348 -> 354, creator 416 -> 422 |
| 3 | the busy banner keeps its pixels in VRAM (`gfx.c`) | +8,640 | 125,760 | measured with the next row |
| 3b | the boot log says what went into SPU RAM (`spu.c`: one more log line, a checksum) | -144 | 125,616 | options volume windows 34.92 / 26.41 / 27.88 (the saving banner), creator 30.13, fair cache 30.17 / 30.12, pet 30.00: all as before |
| 4 | the SPU's conversion buffers are the idle display lists at boot (`spu.c`) | +8,640 | 134,256 | boot only: the SPU line is the same (`sum 50d935f7`), see the next row |
| 5 | image-cache tiles go straight into the upload packets (`vram.c`, `gfx.c`) | +8,656 | 142,912 | after steps 4 and 5: creator 30.13, fair cache 30.17 / 30.12, pet 30.00 (CPU 24.4 ms, as before), options volume 34.92 / 26.41 / 27.88: all as before |
| 6 | shared core files give their one-group parts to that group (`SHARED`) | +18,448 | 161,360 | every speed-tour window: two load windows 0.4-0.9% slower (new island 16.52 -> 16.45, title to New story 8.54 -> 8.46): the reason for step 7 |
| 6b | a host test for step 5 and the boot-time SPU check (48 B of log text) | -48 | 161,312 | |
| 7 | HOLD: free play, the island and the cutscenes take nothing new (`ovl_plan.py`) | -30,000 | **131,312** | every speed-tour window and every harness script ("Checks"): none slower; the loads of the three groups are as before |

No source file of the game changes in steps 1, 2, 6 and 7: the same code and data run from the overlay slot
instead of from resident RAM. Steps 3 to 5 change three platform buffers in `ps1/src`.

**Step 1: callbacks that never leave their group (`ovl_plan.py`).** (In the final build the tables are tested but
not used: step 7's `HOLD` keeps every group they name, and their callbacks, as before, so step 1's bytes are
given back there. What the final build frees is the strings, the three buffers and the shared files' parts.
The tables act as soon as a group leaves `HOLD`.) The planner keeps a function resident as
soon as its address is taken, because resident code may call it later. 22 KB were resident for addresses that
never leave their own group:

* `LOCAL_CALLBACKS`: `fp_island_gfx.c`'s six `draw_*_body` functions go to its own `lr_run()`, which runs them
  at once; `lr_tap` sits in `ui_raster.c`'s `ur_tap` only around one `ui_fill_circle` / `ui_fill_ellipse` call.
  `fp_title.c`'s `pc_pill` / `pc_in` / `pc_vile` go to its own `logo_snap()`.
* `MODAL_CALLBACKS` (called through `scene_callback_begin()`, section 3): the citizen page's `cb_manage` (with
  `do_action` and all it opens, 6 KB), the memorial's `mem_flowers`, the title's `finished_pick` /
  `fresh_world_go`, the profile row's `pf_leave` / `pf_renamed` / `pf_join`.
* `PICK_CALLBACKS` (new): a callback a free-play screen gives to the citizen picker (`fp_pick()`) is called by
  the picker's own update (`finish_pick()`: `scene_pop()`, then the callback), with the free-play group in the
  slot. `wz_neighbour` and `cb_introduce` are listed. The planner checks that the function's group is the
  picker's; the island's picker callbacks (`cb_treat`, `cb_exorcise`, `cb_open`) are not listed and stay resident.

**Step 2: table strings in sections of their own (`tools/strsec.py`, `DOOMO_STRING_SECTIONS`, default ON).**
With `-fdata-sections` every function's string literals get a section of their own, but all literals a file's
tables point to share one (`.rodata.str1.4`), and the planner places whole sections. One resident pointer into
it (a scene's name, two tab labels in `.sdata`) kept every table string of the file resident: 40 KB in 61
files. `strsec.py` is a compiler launcher: it compiles to assembly, gives each `$LC<n>` of `.rodata.str1.4`
its own mergeable section (`.rodata.str1.4.LC<n>`), and assembles. Without the rewrite the two steps give the
one-step object byte for byte (`STRSEC_VERIFY=1` checks that per file; all 316 objects passed). The planner
then keeps resident exactly the strings resident code or data can reach; the linker still merges equal
strings and now drops unread ones. `STR_WHOLE` (`story`, `home`): the groups that set the slot's size keep an
object's table strings together as before, so the slot does not grow.

**Step 3: the busy banner keeps its pixels in VRAM (`gfx.c`).** `gfx_busy_banner()` saved the 252 x 16 pixels
under "Checking MEMORY CARD..." / "Saving settings..." into 8,064 B of `.bss` (`StoreImage`). They are now
copied VRAM to VRAM (`MoveImage`) into the same rows of the other framebuffer and back. That buffer is the next
frame's target, which is cleared before it is drawn (`isbg`). The banner's glyphs and the loading screen's lines
share one array of 40 sprites. One thing differs on screen, and no capture shows it: when the banner goes up
over a frame that was not shown yet, the picture on screen has that frame's pixels in the strip for the few
hundred microseconds until the banner's frame is shown (the pixels that are shown next anyway).

**Step 4: the SPU's conversion buffers are the idle display lists at boot (`spu.c`).** The PCM chunk, its ADPCM
blocks and the DMA's word-aligned copy (8.7 KB of `.bss`) are only used while a waveform or an effect is
converted. On the game disc that is boot only (`audio_hw_init()`, `audio_hw_sfx_preload()`): `stage_open()`
takes the three buffers from the display lists (`plat_save_scratch`, idle until the game loop starts) and
`stage_close()` drops them. A conversion without buffers fails like a full SPU RAM (the effect stays silent,
nothing is written). The boot log says what is in SPU RAM: `spu: 35 of 35 effects (all), top 08be0, sum
50d935f7` (an FNV-1a sum of every byte uploaded; the same sum before and after this step, see "Checks"), and
`game_smoke.txt` expects `effects (all)`. The engine test also converts at run time (its self test) and keeps
the statics. **For Talk on the PS1 (TP1):** code that uploads to SPU RAM after boot must take its staging
buffer the same way, from the game's update (`stage_open()` uses the save scratch: both display lists, idle
until the frame's drawing starts), never from an interrupt.

**Step 5: image-cache tiles go straight into the upload packets (`vram.c`, `gfx.c`, `tilerow.h`).**
`vram_image()` built every tile with its transparent border in a 130 x 130 staging buffer (16.9 KB) and
`gfx_upload_strided()` copied it into the frame's upload packets. `gfx_upload_tile()` writes the bordered rows
straight into the packets (`tile_row()`: one transparent texel, the tile's texels, read backwards for a
mirrored image, transparent up to the even width, one transparent row below). The same packets with the same
bytes, one copy less per cache miss. The staging union is now only the hash table's rebuild copy (8,192 B),
which the boot-time font / CLUT uploads also use. `harness/tilerow_test.c` compares the rows on the host with
the old staging code (full, narrow and short tiles, plain and mirrored).

**Step 6: shared core files give their one-group parts to that group (`ovl_plan.py` `SHARED`).** A core file
outside the planner's `GROUPS` was resident as a whole. For the files named in `SHARED` (`fishcook`, `catalog`,
`ui`, `song_player`, `fp_common`, `audio`) every `.text` / `.rodata` section is a candidate like a group's
own: resident when resident code or data reach it or its address escapes; when the scenes of exactly one group
reach it, it joins that group; reached by two groups it stays resident. `SHARED_NOT` (`story`, `home`,
`combat`, `freeplay`) take none: they set the slot's size or are close to it. Moved: the fishing / cooking
effects and unlock checks (5.6 KB, which also frees the fish and recipe tables only they read), the creator's
option names (4.0 KB), the paint picker and slider only the UI gallery draws (2.5 KB), the studio's song
helpers (1.6 KB), the jukebox's effect names (0.5 KB). The placement follows the call graph on every build: if
a second group starts to call one of these, it is resident again by itself (and costs that much resident RAM:
the build's `ovl_plan: shared code and data in GROUP: N B (...)` lines show what each group holds).

**Step 7: HOLD, the groups on the hub's path take nothing new (`ovl_plan.py` `HOLD`).** What leaves resident
RAM is read with its group from then on: about 2 ms of a load per KB at the drive's 2x speed. With steps 1 to 6
alone the disc ended 161,312 B below the stack top, and three groups took 30,000 B of it: free play (the title and
every free-play screen; 52 loads in the speed tour) loaded in 340 ms instead of 314, the island hub in 227
instead of 206, the cutscenes in 356 instead of 347. The two speed-tour windows that hold those reads were
0.4-0.9% slower (new island: 16.52 -> 16.45 fps; title to New story: 8.54 -> 8.46). The player crosses those
groups all the time, so `HOLD = ("freeplay", "island", "cutscene")` keeps them as they were: their callbacks of
step 1 (`HELD_CALLBACKS`, still in their reviewed tables) are resident, their objects' table strings stay
together, and they take no shared code. The three groups are byte for byte as big as on `89a28b56` (106,248 /
58,324 / 118,768 B). Taking a group off `HOLD` releases it: free play 13.5 KB, the island 13.1 KB, the
cutscenes 3.7 KB (30,000 B in all). That is the lever to pull when the room is needed more than the loads.

### What it costs: bigger groups, longer loads

Code and strings that leave resident RAM are read with their group. The slot does not grow (the house group with
its biggest part ends 552 B and the story group 1,192 B below the slot's end, which is a whole number of 2 KB
sectors), but the groups that took something load a little longer. A load is a scene change (the loading fade,
section 4) that happens before the scene's first frame, so it is not a frame time; the only speed-tour windows
that contain one are the two `..._the_load` windows named in step 7. Bytes are the group's size in `OVL.BIN`
before packing; load ms are PCSX-Redux's own (`ovl: GROUP (N KB) in M ms`) in the speed tour and the harness
runs (a load takes 1-3 ms more or less when its group sits at another offset in `OVL.BIN`: seek time):

| group | bytes before | bytes after | change | load ms before | after | change |
|---|---:|---:|---:|---:|---:|---:|
| home | 172,404 | 172,152 | -252 | 502 | 502 | 0 |
| creator | 153,600 | 161,976 | +8,376 | 416 | 429 | +13 |
| story | 129,880 | 129,880 | 0 | 366 | 368 | +2 |
| combat | 123,532 | 126,088 | +2,556 | 361 | 368 | +7 |
| cutscene | 118,768 | 118,768 | 0 | 348 | 346 | -2 |
| freeplay | 106,248 | 106,248 | 0 | 314 | 314 | 0 |
| fishcook | 93,196 | 103,816 | +10,620 | 289 | 315 | +26 |
| minigame | 98,768 | 100,648 | +1,880 | 314 | 314 | 0 |
| library | 90,572 | 99,116 | +8,544 | 288 | 307 | +19 |
| studio | 83,824 | 85,636 | +1,812 | 276 | 282 | +6 |
| concert | 75,140 | 75,300 | +160 | 268 | 267 | -1 |
| island | 58,324 | 58,324 | 0 | 207 | 208 | +1 |
| exorcism | 47,228 | 47,768 | +540 | 194 | 200 | +6 |
| dream | 40,068 | 41,616 | +1,548 | 182 | 181 | -1 |
| storymg | 38,056 | 38,148 | +92 | - | - | - |
| pet | 27,116 | 27,216 | +100 | 148 | 147 | -1 |
| workshop | 16,596 | 17,084 | +488 | 125 | 128 | +3 |
| cutx | 14,024 | 14,024 | 0 | - | - | - |
| home.mart | 12,144 | 12,144 | 0 | 114 | 113 | -1 |
| home.visit | 8,824 | 9,076 | +252 | 106 | 107 | +1 |
| home.build | 8,916 | 8,916 | 0 | - | - | - |
| gallery | 5,352 | 8,276 | +2,924 | 102 | 108 | +6 |
| home.defend | 7,308 | 7,308 | 0 | 100 | 99 | -1 |
| home.roomtest | 5,296 | 5,296 | 0 | 98 | 101 | +3 |
| all groups | 1,535,184 | 1,574,824 | +39,640 | | | |

The groups that load longer than before are fishing and cooking (+26 ms of 289), the library (+19 ms of 288),
the creator (+13 ms of 416) and combat (+7 ms of 361): once per scene change, each under one frame. The three
HOLD groups (free play, the island, the cutscenes) load as before. To make one of the others as fast as before
too, add it to `HOLD`: it keeps about the group's growth resident (fishing and cooking 10.6 KB, the library 8.5 KB, the
creator 8.4 KB of the 131,312 B).

### Checks (PCSX-Redux, base `89a28b56` against the final build)

Base is the build of `89a28b56` (65,936 B below the stack top); the final build is the tip of the package
(131,312 B). Both disc images were played by PCSX-Redux (`ps1/harness/run.py`, `ps1/harness/speed2.sh`), one
emulator at a time; the fps numbers are the game's own frame counts, so a busy host does not change them.

* **Boot and SPU.** `game_smoke` passes on both and its 7 screenshots are byte for byte the same. The SPU line
  of the boot log is `spu: 35 of 35 effects, top 08be0, sum 50d935f7` before and after step 4 (the buffers'
  move; `(all)` was added to it later), so the same bytes went into SPU RAM. `memcard_check.py` finds the game's
  save blocks on the smoke run's card. The engine-test disc (918,848 B below the stack top) passes its audio
  self test, its memory card self test (and `memcard_check.py` on that card) and the model script.
* **Frame rate.** `speed2.sh`, all 10 groups, 218 windows: 213 have the same fps (212 the same frame histogram
  too; the dream wake-up window has a 3-vsync frame where it had a 4). Four are one frame better (the kitchen
  counter 29.83 -> 30.00, the library shelf move 29.51 -> 30.00, the new-island load window 16.52 -> 16.65, the
  care popup 30.00 -> 30.25) and one is 0.03 fps lower: `story_title_new_story_the_load`, 8.54 -> 8.51. That
  window is 2.4 s of overlay reads; they add up to 2,428 ms against 2,429 ms before, and one frame after the
  last read ends at 3 vsyncs instead of 2 because the reads' seek times moved by 1-3 ms (the groups sit at other
  offsets in `OVL.BIN`). No window has a worse worst frame. The perf scripts and `home`, `fair_cache`,
  `options_volume`, `creator_speed`, ... (53 fps numbers): 51 equal; the two others are the third-person combat
  views, 29.81 -> 29.88 and 29.45 -> 29.51 (slightly faster). Combat did not get slower.
* **Scripts.** The 44 scripts played on both builds (`game_smoke`, 25 of `core/tests/scripts` (boot, UI gallery, saves,
  library, free play, studio, exorcism, workshop, creator, ...), 7 of `ps1/harness/scripts`, the 10 `perf_*` and
  `es_perf`) all
  pass; so do the 15 press journeys on the final build. No `ovl: ERROR` line anywhere. The old diagnostic
  `gfx: save scratch taken mid-frame, list drawn early` appears as often in every script as before.
* **Pixels.** The speed tour's 182 screenshots: 177 byte for byte the same. The harness scripts' 428: 346 the
  same, and every shot in 25 of the 44 scripts. The rest are animated sprites whose picture depends on when a
  time-sliced job ran (`pslice.c` budgets the portrait cache's slices in microseconds of CPU time) and on the
  global random generator (`plat_entropy()` is the VSync counter and two hardware timers, so any change of boot
  time re-rolls the title's pair of citizens, a chaotic house's pits, a ghost's sparkles). The scripts with a
  differing shot were played again on both builds with `seed 1` as their first line (the generator pinned): 309
  of those 320 shots are the same. The 11 left are the 3D view behind the combat pause menus (combat's own
  timing, which this package does not touch) and its arenas, the almanac's animated portraits, the exorcist's
  ghost and a 10 x 10 px sparkle. Six of them are the same picture a few game frames later (shots one game frame
  apart: base and final are identical from the 1st to the 8th frame on, the arena pause menu after a shift of two
  frames); five are never in step within 12 frames (random sparkles and particles, the arena's 3D floor). A
  control: the build with every move of steps 1-6 (161,312 B) differs from the final build by the placement of
  30,000 B of unchanged code only, and one of the 182 tour shots differs between them (the same 10 x 10 px
  sparkle). No UI panel, text, tile or static picture differs anywhere.
* **The tool tests.** `ps1/tools/test_tools.py` (28 tests): each rule of the planner (callbacks, pick callbacks,
  strings, shared files, HOLD), the gate's WARNING and the string splitter fail a test when its line is
  removed; `harness/tilerow_test.c` (56 tiles) fails on a wrong row; with `STRSEC_VERIFY=1` the whole PS1
  build (316 objects) compiles every file the same in two steps as in one, and the EXE it links is the same
  file as the normal build's.
* **Rebased tree.** The package on `0df72845` against `0df72845` itself (68,368 B below the stack top; with the
  package 129,456 B), PCSX-Redux again: the speed tour (218 windows) has the same fps in 212, a better fps in
  6 (the care and kitchen windows, the dream windows, the library shelf move, the new-island load) and a
  lower one in none; one window (`library_weapons_plan`) shows a 3-vsync frame, which is the window's first
  frame (the lockstep resume, 0 frames over 33 ms in its table row). The loads are as in the table above
  (free play, the island and the cutscenes +0-2 ms). 179 of the 182 tour shots are byte for byte the same (the
  arena's 3D view and one animated portrait differ). 23 harness scripts on both builds all pass, with the same
  fps (two windows slightly better: third-person combat 29.81 -> 29.88, the options exit 26.41 -> 26.54);
  `game_smoke` 7 of 7 shots the same and the same SPU line (`sum 50d935f7`). The shots that differ are the
  random / time-sliced ones named above (title, island, life, concert).
* **Gates** (on the rebased tree). `make -C core -j2 test`: 1119 passed, 0 failed (1067 before the rebase; also
  with `SAN=1`: only the four old UBSan reports of `concert_gfx.c` / `fixmath.c` and their tests);
  `check-gen`; `core/tools/xcheck.sh` (408 files on gcc, arm, mipsel, sh4, no warning); `make -C gba` and
  `make -C gba test` (nothing under `core/`, `gba/` or `dc/` changes in this package); `make dc-host-test`.
* **First landing tree, `d5305653` (2026-11-15).** `d5305653` against `d5305653` with the package, clean builds, PCSX-Redux,
  one emulator at a time (the harness of this package on both builds). Speed tour, all 10 groups, 218
  windows: 213 the same fps, 5 better (the kitchen counter 29.83 -> 30.00, the nightmare vignette 29.85 ->
  30.00, the dream wake-up 29.75 -> 30.00, the new-island load 16.25 -> 16.39, title to New story 8.40 ->
  8.43), none lower; no window has more frames over 33 ms or a worse worst frame. 180 of the 182 tour shots
  are byte for byte the same (the arena's 3D floor in `act_rush_round1`, 2,509 px in its bottom band, and
  the 218 px choir sparkle, both named above). Loads (the tour's `ovl:` lines, mean ms): free play 313.7 ->
  313.7 (52 loads), the island 206.5 -> 206.8, the cutscenes 348.2 -> 347.7, story 367.6 -> 367.8, home
  500.2 -> 500.7; fishing / cooking 287 -> 315, the library 288 -> 307, the creator 448 -> 456, combat
  367.4 -> 374.8, studio 276 -> 282. `perf_combat`: 29.75 / 30.00 / 29.94 / 29.51 -> 29.88 / 30.00 / 29.94 /
  29.51 fps, its 4 shots the same. `game_smoke` passes with 7 of 7 shots the same as the tip's and the same
  SPU line (`sum 50d935f7`); `memcard_check.py` finds the save. `harness/scripts/busy_banner.txt` (the
  review's probe: VRAM and displayed shots 0, 1, 2, 4 and 24 frames after the "Saving settings..." banner,
  twice): 16 of 16 shots the same.
* **The harness on the landed trees.** `game_smoke` first timed out on the package build (twice): `g_hcmd`
  sits at `0x8013f7fc` there, its magic word in one 2 KB sector of the EXE and `lock` in the next, so the
  BIOS's load of that sector wrote `lock` back to 0 after the harness had set it; the game ran free into the
  first-boot language picker and never stopped. `harness.lua` now sets `lock` again on every vsync until the
  game has stopped once (section 10). The tip's own build is not affected (its mailbox is inside one sector).
* **Gates on `d5305653`.** `make -C core -j2 test`: 1211 passed, 0 failed, also with `SAN=1` (only the four
  old UBSan reports). The runner script `home_visit` and the GBA `test-talk` failed on `d5305653` itself in the
  same way (the owner's Talk follow-ups and quick replies of 2026-11-14); the S2-9 lander repaired both
  (`tip repair:` commits). Every other GBA test passed, the GBA ROM was byte for byte the tip's.
* **Rebased onto S2-9, `3d9905df`.** Clean builds: 72,320 -> 132,608 B. The whole speed tour again on both:
  213 windows the same fps, the same 5 better, none lower, no window with more frames over 33 ms or a worse
  worst frame, and all 182 shots byte for byte the same; the loads as above (the workshop 205 -> 206 ms).
  `perf_combat` 29.75 / 30.00 / 29.94 / 29.51 -> 29.88 / 30.00 / 29.94 / 29.51 (4 of 4 shots the same),
  `game_smoke` and `busy_banner.txt` (16 of 16) the same, the game harness suite (`cmake --build --preset ps1
  --target harness`: `game_smoke` + `memcard_check`, boot, UI gallery, saves, Field Guide, `creator_speed`,
  `fair_cache` and the 15 journeys) 22 of 22 PASS. Gates: core 1,224 passed and 206 script runs OK (also
  `SAN=1`, the four old reports), `check-gen`, `xcheck.sh` (417 files), `make -C gba` + `make -C gba test` +
  `make -C gba sizes` (EWRAM `.bss` 248,924 B, IWRAM 11,272 B: the package changes no GBA byte), `make
  dc-host-test`, `tools/test_tools.py` (29) and `harness/tilerow_test.c`.
* **`249a0282`** (CR4 A4, Talk T8 / T9 and DC6C on top of `3d9905df`). The resident layout is
  the same as on `3d9905df` (72,320 -> 132,608 B); the creator group grew with CR4 A4 (planner estimate 123,484
  B, inside the slot). Played again on both builds, the parts those commits touch: `game_smoke` (7 of 7 shots
  the same) and the creator's 25 speed-tour windows (all the same fps, 16 of 16 shots the same, the creator's
  load 455 -> 463 ms); on the package build the game harness suite, 22 of 22 PASS (`creator_speed` 30.13 /
  29.39 / 30.13). Gates: core 1,256 passed and 218 script runs OK (also `SAN=1`, the four old reports), `check-gen`, `xcheck.sh` (419
  files, no warning), `make -C gba` + `make -C gba sizes` (EWRAM `.bss` 249,084 B, IWRAM 11,272 B) +
  `make -C gba test`, `make dc-host-test`, `tools/test_tools.py` (29).
* **`ad060127`** (CR-BODY, Talk care and TM2b on top of `249a0282`). Clean builds: 72,336 ->
  132,608 B. `game_smoke` 7 of 7 shots the same, the game harness suite 23 of 23 PASS (`creator_speed` 30.13 /
  29.39 / 30.13 / 29.88, as before), the creator's 16 speed-tour shots the same. One creator window changed:
  `creator_home_tab` 30.17 -> 30.00, one 50 ms frame at frame 1402 (two runs, both builds deterministic; the
  creator group has 15 such frames on both builds, this is a 16th: 2,890 -> 2,891 vsyncs over 1,450 frames).
  It is not more work. Builds that log every frame's CPU time there show the frame before it, a portrait-cache
  slice frame, at 32,480 us on the package and 32,479 us without step 5, and the four tile-upload frames 1.3 to
  1.6 ms CHEAPER with step 5 (24,280 against 25,869 us, ...). `pslice.c` packs slice frames up to 700 us
  (`MARGIN_US`) under two vblanks, so a frame that lands a little differently can cross the line: the package
  with step 5 reverted has no such frame (a phase change, at 8,640 B), the creator group on `HOLD` still has it.
  The margin of the slice scheduler is where that frame is fixed (a speed package), not here. Gates: core 1,274
  passed and 220 script runs OK (also `SAN=1`, the four old reports), `check-gen`, `xcheck.sh` (419 files, no
  warning), `make -C gba` + `make -C gba test` + `make -C gba sizes` (EWRAM `.bss` 249,084 B, IWRAM 11,272 B),
  `make dc-host-test`, `tools/test_tools.py` (29).
* **`5879aa3f`** (S3-1, CR4 A5 and TM2c on top of `ad060127`). Clean builds: 72,352 -> 132,624
  B. `game_smoke` 7 of 7 shots the same; the creator's and S3-1's new `sim` speed-tour groups on both builds: 29
  windows, all the same fps (the home tab's extra frame of `ad060127` is gone again), 20 of 20 shots the same,
  the creator's load 460 -> 469 ms; the game harness suite 23 of 23 PASS (`creator_speed` 30.13 / 29.39 / 30.13 / 29.88). Gates: core 1,302 passed and 222
  script runs OK (also `SAN=1`, the four old reports), `check-gen`, `xcheck.sh` (421 files, no warning), `make -C gba` + `make -C gba test` + `make -C gba sizes` (EWRAM `.bss` 249,084 B, IWRAM 11,272 B), `make
  dc-host-test`, `tools/test_tools.py` (29).
* **The final tip, `b9e74a51`** (DC9B, TUI-TALK, DC6D and CR5 on top of `5879aa3f`). Builds: 72,352 -> 132,608 B.
  `game_smoke` 7 of 7 shots the same as on `5879aa3f`; the creator's 25 speed-tour windows on both builds all the
  same fps, 16 of 16 shots the same, the creator's load 469 -> 475 ms. Gates: core 1,315 passed and 224 script runs OK
  (also `SAN=1`, the four old reports), `check-gen`, `xcheck.sh` (421 files, no warning), `make -C gba` + `make -C
  gba test` + `make -C gba sizes` (EWRAM `.bss` 254,844 B with CR5's pool, IWRAM 11,272 B), `make dc-host-test`,
  `tools/test_tools.py` (29).

### Keeping the room (for the packages that follow)

* The shared budget is in `docs/design/held/PARALLEL_WAVES.md` section 3. Every landing reports the margin
  line of its own build; the WARNING and the CI summary show it long before the hard limit.
* New scene code belongs in a group, not in a resident file. A new callback handed to `ui_confirm` /
  `ui_choice` / `ui_text_entry` or to `fp_pick()` from a free-play screen goes into `MODAL_CALLBACKS` /
  `PICK_CALLBACKS` (after a look at its call sites), or the planner keeps it and all it reaches resident
  (`OVL_WHY=1` on the build lists resident code by file; `OVL_WHY_OBJ=name` explains one object).
* A resident module must not keep a pointer to a group's string or table across scenes (section 3). That now
  also holds for the table strings of a group's files and for the `SHARED` files' name tables: they are in the
  slot only while their group is.
* The slot is 131,072 B (64 sectors of 2 KB); a group that outgrows it makes it 2 KB bigger, which is 2 KB less
  resident RAM. Room on the landed tree (`b9e74a51` with this package, from `build/doomogachi.ldmap`): the story
  group ends 1,216 B below the slot's end, combat 1,692 B (PS1-FLAT put `model.c` in it), the house's common part
  1,816 B (its parts follow it in whole 2 KB sectors: HomeMart, the biggest at 12,144 B, has 144 B before it
  takes one more), the creator **600 B** on the rebased tree (CR10 moved the packed icons out of it: 63,024 B now; it loads over borrowed `.bss` and ends in the slot, in whole 2 KB
  sectors; 183,000 B with this package, which gives it about 3.8 KB of catalog parts and table strings; every
  creator batch adds to it: CR5 took 1,932 B of the room), the cutscenes 12,344 B, free play 24,752 B. A group that reaches the slot can keep its table
  strings resident instead (`STR_WHOLE`) or take no shared code (`SHARED_NOT`).
* The held groups (`HOLD`) keep their loads as long as they were; a new callback of theirs is resident, too.
* Levers not used here (the ledger above): `weld_gfx` (28 KB resident, reached by the workshop and by combat),
  the portrait files (130 KB of code every scene uses), `s_prim` (the display lists, 108 KB: frames have needed
  at most 40 KB of the 54 KB each), `doomo_town` (89 KB), the resident Spanish table (8 KB, L6), `model.c` once
  the house draws no 3D figure (4.3 KB, with what only it uses of `prims.c` 8.4 KB; owner decision 2026-11-12, "HOUSE
  CHARACTERS FLAT": a change of what the house shows, so not part of this package), and the 30,000 B of `HOLD`.

## CR4 A1b (2026-11-15): noses, mouths, ears, blush styles

Measured with clean `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON` builds on origin `5879aa3` and on A1b rebased onto `abc4f66`: `_end` 0x801ee550
both times (72,368 B below the stack top): 0 B resident added. On the final landing tip (after PS1-RAM, CR5-FIX, TD1) the A1b tree ends at 0x801df9f0, 132,624 B (129 KB) below the stack top, above the 72 KB rule; the parts are still not read by the PS1. The 51 new parts (12 noses, 12 mouths,
16 ears, 11 blush styles) are part scripts the PS1 does not read yet (the part VM was off until CR10 (history: on since CR10)), age class ALL; the creator offers the PS1
only the painted choices (`part_live_drawable`), as for every scripted part since CR3. `mat blush` is in the VM only (`portrait_script.c`,
compiled out here). Harness (PCSX-Redux): `game_smoke`, `creator_grid`, `creator_parts_face_b` and `creator_speed` PASS (idle 30.13, grid
scroll 29.39, rows 30.13, value 29.88: nothing resident changed).


## S3-2 (2026-11-15): no whole portrait or chibi inside a frame

UI previews (`portrait_image_ui`, `figure_image_ui`, `figure_image_res_ui`, `bodyspr_image_ui`; docs/PORTRAIT.md "UI
previews") are patient jobs: a face or chibi with nothing to stand in is drawn in idle time and the frame's room and the
call returns NULL meanwhile. `ps1/src/pslice.c` shares the forced-slice rule with the GBA (`doomo/pslice_policy.h`: a forced
slice only when it costs at most one more vblank). **Resident cost: +304 B** (`_end` 0x801ee550 -> 0x801ee680; the one
wrapper `portrait_ui_image` is 180 B, the rest is the call sites and the rule): **72,368 -> 72,064 B below the stack top**
with the FMV on the tree it was built on; after the rebase onto the PS1-RAM landing (132,608 B) it measures **132,320 B**
(`_end` 0x801dfb10, `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`), so the thin margin is gone. `game_smoke` PASS. `sh ps1/harness/speed2.sh OUT
life studio fishcook library` (PCSX-Redux, not run before the change on this machine; the audit listed these windows at
65-325 ms single frames): every window of the shop, Black Market, cafe, picker, picker move, choir, cook counter, lobby and
Almanac list reads 30.0-30.5 fps with no frame over 50 ms (a single 50 ms frame in several of the studio, cook and library windows); the known misses are the
new island's load, the save freeze and the care reaction (2 frames of 50 ms).
## CR-SLIDERX (2026-11-15): the cheat "Slider range x1..x4"

Measured on the tip `5879aa3f` and the tip + CR-SLIDERX, 2026-11-15 (`cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`,
`cmake --build --preset ps1 -j2`, `PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b`): `_end` 0x801ee550 -> 0x801ee660, **+272 B resident**;
margin below the stack top with the boot FMV **72,352 -> 72,080 B** (the elf2exe line still prints "70 KB left"; the hard gate is 64 KB, the
plan gate stays as CR-BODY described). CREATOR's share is 4 KB (PARALLEL_WAVES section 3). Where the bytes are (map diff): the slider loop
`look_sliders_fix` 216 B (one body for the scaling and for `sanitize`'s clamp, `noinline`), its table `look_sliders` 44 B, the option rows
(`doomo_option2_info` +16 B, the value names 16 B, `apply2` and the OPT2 reader / reset +40 B), the hash memo reset 20 B, `look_for_stage`
+68 B (the adult call, the stages' maths now on zero-extended bytes), `pb_layout`, `ptk_place`, `face_layout` and `gen_look` +48 B together
(signed loads of the 11 look fields, now `int8_t`), and `sanitize` -260 B (its 11 compares became the call). The first version was +608 B: the
loop was inlined twice and the stages' signed divisions cost 150 B; the option now lives in a byte of the OPT2 array instead of bits. The creator
and the Options page are overlay code. **Linker fix on the landing tree (origin `e597703e`, with S3-2):** the tip links at `_end` 0x801dfb10 (132,320 B below the stack top). With the package the home overlay group (`.ovl_03`, 0x2d008 B) passed 90 sectors by 8 bytes, and `ovl_pack` refused it ("last sector passes the slot"): `ovl_plan.py` kept the slot's end for the LAST borrowing group only (`.` moves back at each group's start), so the home group's sector-rounded end was lost. It now keeps the running end in `__ovl_hi`. Result: `_end` 0x801e0420, **130,000 B below the stack top** (the extra sector of the home group is 2,048 B of it; the 72 KB plan gate is met with 56,272 B to spare); `game_smoke`, `creator_speed` (30.13 / 29.39 / 30.13 / 29.88 fps) and `creator_sliders` with x4 pass. **Earlier on `5aa25cd1`** (the numbers below), with PS1-RAM, TM2d, DC9C and CR5 under it; the package rebased on it): `_end` 0x801dfaf0, **132,352 B below the stack top** with the boot FMV (the tip's last number was 132,608 B, so the package costs 256 B there; the 72 KB plan gate, 73,728 B, is met with 58,624 B to spare). Harness (PCSX-Redux): `game_smoke`, `creator_speed` (idle 30.13, grid scroll 29.39, rows 30.13, value 29.88 fps, unchanged) and `creator_sliders` with x2, x3, x4 pass. Harness (PCSX-Redux): `creator_sliders` and its x2, x3, x4 variants (the numbers are in the ROADMAP Done line).

## LV7a (level art kit part A): resident delta 0 B

Measured 2026-11-15 on `fe410024` and the same + LV7a (clean builds, boot FMV, `-DDOOMO_INTRO_REQUIRED=ON`): `doomogachi.exe` and
`OVL.BIN` are byte-identical (`cmp`), `elf2exe` says 130,000 B below the stack top both times. The 7 new textures and 3 skies sit behind
`#ifndef DOOMO_PLATFORM_PS1` in `art_data.c` (`TEX_COUNT` / `SKY_COUNT` stay 48 and 5 there; `packart.py` stops at that line, tested by
`tools/tests/test_packart.py`); the texture bank reader, its hook in `textures.c` and the sample bank are compiled out
(`DOOMO_TEXBANK_ACTIVE` 0). The new art and the bank blobs reach the PS1 as pack data on the disc with the level packs (LV8). The LEVELS
share of 8 KB resident: 0 B used by LV7a. `game_smoke` PASS.

## Studio ST-EASY (2026-11-15): Easy mode, resident delta 0 B

Measured on `d979c93` and the same + ST-EASY (clean builds, boot FMV, `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`,
`PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b`): `elf2exe` says **110,608 B below the stack top both times** (plan gate 73,728 B). The new code
(`studio_start.c`, `studio_easy.c`, `studio_help.c` and the redo / card hooks in `studio*.c`) is in the `studio` overlay group, whose load
grows by 27 ms (the second part of the group: 283 -> 310 ms, once per scene entry; the first part, 313 ms, is the same). `Studio` did not grow
(`nm`: 0x738 B before and after) and the state `StEasy` (448 B) is in the scene scratch (`EZ_TAG`, claimed through `scene_scratch()`, no
static pointer). A first version had four small `static const uint8_t` tables (5, 5, 8 and 3 bytes) and cost 16 B of resident `.sdata` (the
compiler's small-data threshold is 8 B: anything that small goes to `.sdata`, which is resident, even in an overlay group's file); the tables
are now one 10-byte and one 36-byte array (overlay `.rodata`). Rule for the next package: a `static const` table of 8 bytes or less is resident.
Speed (PCSX-Redux, the demo song, `-107`; the same script on both builds): SONG idle / playing, NOTES idle / playing, ORDER, LYRICS and CHOIR
all 30.13 fps (120 frames in 239 vsyncs); the screens ST-EASY added (the Start card, the control card, the made song playing, FILE at both
ends) 30.13 - 30.17; `sh ps1/harness/speed2.sh OUT studio`: all nine windows identical to the base. `harness/run.py`: `game_smoke` PASS,
`core/tests/scripts/studio.txt` and `studio_easy.txt` PASS (the control card names the pad's buttons: "Triangle Undo a step", "Square Redo a
step", "SEL+L1/R1 Switch tab").

## ST-PLAY (the Music Studio's PLAY tab): resident +256 B, margin 110,352 B

Measured 2026-11-15 on the clean tip `d979c93` and the same + ST-PLAY (clean builds, boot FMV, `-DDOOMO_INTRO_REQUIRED=ON`, `PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b`):
`elf2exe` says 110,608 B below the stack top before and **110,352 B** after (`_end` 0x801e4fe0 -> 0x801e50e0, +256 B resident; the 72 KB gate has
36 KB to spare). All of it is in `song_player.c` (shared, resident): `song_jam_on` / `song_jam_off` / `song_play_range` and the range loop in
`advance` (the plan asked for share 0: this is the number to ask the coordinator for). The PLAY tab itself (`studio_play.c`, about 7 KB of
code) is in the studio overlay group (`OVL.BIN` 1,114 -> 1,122 KB, the studio group still far under the slot; its entry load 283 -> 310 ms), its
callback `play_menu_done` is in `MODAL_CALLBACKS` (`tools/ovl_plan.py`), and its state is 64 B of the scene scratch, not `.bss`. PCSX-Redux speed
tour (`sh ps1/harness/speed2.sh OUT studio_play studio`): every PLAY window 30.13-30.25 fps (the take 32.20 with the loop start), every other studio
window as before (30.00-30.51); `game_smoke` PASS.
