# ps1/ - DOOMOGACHI LIFE in HELLvile Park! for the PlayStation

A real PlayStation disc running the shared C99 core: PSn00bSDK, GTE 3D
engine, SPU ADPCM audio, memory card saves. Full documentation (build,
emulators, real hardware, controls, VRAM / RAM / SPU maps, the 3D engine,
saves, harness, measured performance): **[docs/PS1.md](../docs/PS1.md)**.

## Build

```sh
export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b   # PSn00bSDK install
export PATH=$PATH:/opt/psn00bsdk/bin                 # mkpsxiso
cd ps1
cmake --preset ps1                  # configure -> build/
cmake --build --preset ps1          # build/doomogachi.exe, doomogachi.bin + .cue

cmake --preset ps1-enginetest && cmake --build --preset ps1-enginetest
                                    # build-enginetest/: boots into the 3D engine showcase
cmake --preset ps1-pal && cmake --build --preset ps1-pal
                                    # build-pal/: PAL video
```

Needs CMake >= 3.21, Ninja, Python 3, PSn00bSDK and `mipsel-linux-gnu-gcc`
(apt `gcc-mipsel-linux-gnu`; the `ps1-elf` preset uses `mipsel-none-elf`).
All of `ps1/src` plus every core source (`core/src/*.c core/gen/*.c`, the
same wildcards as `core/sources.mk`) is compiled with `-O2` and linked as
objects; platform code builds with `-Werror`. The build also checks that
the binary only uses R3000 (MIPS I) instructions and fits in 2 MB of RAM.

## Run

```sh
pcsx-redux -iso ps1/build/doomogachi.cue -run       # PCSX-Redux (disc)
pcsx-redux -exe ps1/build/doomogachi.exe -run       # PCSX-Redux (side-load)
duckstation-qt ps1/build/doomogachi.cue             # DuckStation
nops /fast /exe ps1/build/doomogachi.exe /dev/ttyUSB0   # real console + Unirom
```

The disc is license-free: on a console it needs a modchip, a softmod
(FreePSXBoot, tonyhax, Unirom) or an ODE. Saves go to the memory card in
slot 1 (`BASLUS-DOOMO0` story, `BASLUS-DOOMO1` free play).

Pad: Cross = A, Circle = B, Square = X, Triangle = Y, L1 = L, R1 = R,
L2, R2, START, SELECT; D-pad or the left analog stick.

## Test (headless PCSX-Redux)

```sh
cmake --build --preset ps1 --target harness             # disc boot, smoke, core scripts
cmake --build --preset ps1-enginetest --target harness  # engine tour, card + audio self tests
python3 harness/run.py --exe build/doomogachi.exe \
    --script ../core/tests/scripts/ui_gallery.txt --out /tmp/shots
```

`harness/run.py` plays the core native runner's scripts (`wait`, `press`,
`hold`, `shot`, `scene`, `save`, `type`, ...) frame-exact on the PS1 build,
writes PNG screenshots and the TTY log, detects hangs and crashes and
reports the frame rate. See docs/PS1.md section 10.

## Layout

| path | what |
|------|------|
| `CMakeLists.txt`, `CMakePresets.json` | build, disc image, `harness` target |
| `iso.xml`, `system.cnf` | mkpsxiso disc layout, boot config |
| `src/main.c` | boot, main loop (30 Hz), log, ticks, entropy |
| `src/gfx.c`, `src/gfx2d.c` | display lists, double buffering, the 2D layer |
| `src/vram.c` | texture / image / CLUT caches in VRAM |
| `src/packart.c`, `tools/packart.py` | the core's textures and skies, linked LZ-compressed (RAM) and decoded on a VRAM cache miss |
| `src/r3d.c`, `src/r3d.h` | GTE pipeline: subdivision, clipping, fog, sky, billboards |
| `src/view3d.c`, `src/world.c` | `plat_view3d()`: the map as 3D polygons |
| `src/overworld.c` | `plat_overworld()`: 3D town terrain |
| `src/model.c` | third-person 3D citizen from the resident's look |
| `src/spu.c`, `src/adpcm.c` | SPU tones and samples, our ADPCM encoder |
| `src/memcard.c` | memory card saves (BIOS file API, icon, Shift-JIS title) |
| `src/pad.c`, `src/timer.c` | controller input, CPU timing |
| `src/hcmd.c` | test-harness mailbox and lockstep |
| `src/coreref.c` | weak references to optional core symbols |
| `src/libgcc_r3000.c` | 64-bit integer helpers without MIPS32 instructions |
| `src/etest*.c` | engine-test mode: showcase maps, tour, self tests |
| `tools/` | `elf2exe.py` (PS-X EXE), `check_isa.py` (R3000 check), `packart.py` (packed art) |
| `harness/` | `run.py` + `harness.lua` (PCSX-Redux), scripts, card / ADPCM checks |
