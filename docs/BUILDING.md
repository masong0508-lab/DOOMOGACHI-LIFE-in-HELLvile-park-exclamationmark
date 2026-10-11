# Building and playing DOOMOGACHI LIFE in HELLvile Park!

This guide covers toolchain setup on Ubuntu/Debian, macOS and Windows
(WSL2), the build commands, the test harnesses, emulators and real
hardware. You don't need to build anything just to play: every push builds
the ROM and discs on GitHub (see [Downloads](#downloads)).

Contents:
[What you need](#what-you-need) ·
[Ubuntu / Debian](#ubuntu--debian) ·
[PSn00bSDK](#psn00bsdk-from-source) ·
[macOS](#macos-homebrew) ·
[Windows (WSL2)](#windows-wsl2) ·
[Building](#building) ·
[Tests](#tests) ·
[Playing the builds](#playing-the-builds) ·
[Real hardware](#real-hardware) ·
[CI and releases](#ci-and-releases) ·
[Troubleshooting](#troubleshooting)

---------------------------------------------------------------------------

## What you need

| You want to... | You need |
|---|---|
| run the core tests (`make core`) | a host C compiler (`gcc`), GNU make, `python3` |
| build the **GBA ROM** (`make gba`) | `arm-none-eabi-gcc` + **newlib**, `python3` |
| run the GBA harness (`make test`) | the above plus `libmgba-dev` 0.10 |
| build the **PS1 disc** (`make ps1`) | `mipsel-linux-gnu-gcc` (or `mipsel-none-elf-gcc`), **PSn00bSDK** built from source, CMake ≥ 3.21, Ninja, `python3` |
| run the PS1 harness (`make test`) | the above plus PCSX-Redux, OpenBIOS and `xvfb-run` |
| render the **trailer** (`make trailer`) | `gcc`, `python3`, `ffmpeg` (with libx264) |

There are no devkitPro, Sony SDK or BIOS dependencies. Everything is free
software and builds from source.

`make doctor` shows what was found:

```
$ make doctor
DOOMOGACHI LIFE build doctor
  host gcc            found   /usr/bin/gcc
  arm-none-eabi-gcc   found   /usr/bin/arm-none-eabi-gcc
  newlib (arm)        found   /usr/lib/gcc/arm-none-eabi/13.2.1/../../../arm-none-eabi/lib/libc.a
  ...
  make gba : ready
  make ps1 : ready
```

Versions that are known to work (Ubuntu 24.04 packages, the same as the CI runners):

| tool | version |
|---|---|
| gcc (host) | 13.3 |
| arm-none-eabi-gcc / newlib | 13.2.1 / 4.4.0 |
| mipsel-linux-gnu-gcc | 12.4 |
| PSn00bSDK | commit `5d9aa2d3dfc7d6e51c2eb942ab4cdbae5571a40a` (0.24) |
| CMake / Ninja | 3.28 / 1.11 |
| libmgba | 0.10.2 |
| Python | 3.12 / 3.13 (stdlib only) |
| ffmpeg | 6.1 |

---------------------------------------------------------------------------

## Ubuntu / Debian

Tested on Ubuntu 24.04. Debian 12 and Ubuntu 22.04 use the same package
names. The GBA harness is written against libmgba 0.10, and Ubuntu 22.04
ships 0.9, so on 22.04 skip the harness. The ROM still builds.

```sh
sudo apt update
# core + GBA
sudo apt install build-essential python3 gcc-arm-none-eabi libnewlib-arm-none-eabi
# GBA headless test harness (optional)
sudo apt install libmgba-dev
# PS1 toolchain (then build PSn00bSDK, next section)
sudo apt install gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu \
                 cmake ninja-build git
# trailer (optional)
sudo apt install ffmpeg
```

`libnewlib-arm-none-eabi` is easy to miss, and without it the GBA link
fails with `cannot find -lc`.

### PSn00bSDK from source

The PS1 build uses [PSn00bSDK](https://github.com/Lameguy64/PSn00bSDK), the
open source PlayStation SDK, built at a pinned commit with the Debian/Ubuntu
`mipsel-linux-gnu` toolchain. Its examples are skipped
(`-DSKIP_EXAMPLES=ON`): one C++ example does not build with that
toolchain, and the game doesn't need them.

```sh
git clone https://github.com/Lameguy64/PSn00bSDK
cd PSn00bSDK
git checkout 5d9aa2d3dfc7d6e51c2eb942ab4cdbae5571a40a
git submodule update --init --recursive          # mkpsxiso, tinyxml2, ...

cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DSKIP_EXAMPLES=ON \
      -DPSN00BSDK_TARGET=mipsel-linux-gnu \
      -DCMAKE_INSTALL_PREFIX=/opt/psn00bsdk       # or $HOME/psn00bsdk (no sudo)
cmake --build build                               # about 30 s on 4 cores
sudo cmake --install build                        # drop sudo for a $HOME prefix
```

Then point the build at it:

```sh
export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b   # <prefix>/lib/libpsn00b
export PATH="$PATH:/opt/psn00bsdk/bin"               # mkpsxiso, elf2x, ...
```

The top-level Makefile finds the SDK by itself in `/opt/psn00bsdk`,
`/usr/local`, `~/psn00bsdk` and `~/.local`. Set `PSN00BSDK_LIBS` for any
other location. `core/tools/xcheck.sh` reads the same variable for its
PlayStation compile check.

### PCSX-Redux and OpenBIOS (only for the PS1 harness)

The automated PS1 tests (`make test`, or `cmake --build --preset ps1
--target harness`) run the disc headless in
[PCSX-Redux](https://github.com/grumpycoders/pcsx-redux) under `xvfb-run`,
with PCSX-Redux's free OpenBIOS. You need these only for the tests, not to
play.

```sh
sudo apt install xvfb
git clone --recursive https://github.com/grumpycoders/pcsx-redux
cd pcsx-redux
# install the build dependencies listed in PCSX-Redux's README, then:
make -j"$(nproc)"
make -C src/mips/openbios PREFIX=mipsel-linux-gnu      # -> src/mips/openbios/openbios.bin
sudo mkdir -p /opt/ps1emu
sudo ln -s "$PWD/pcsx-redux" /opt/ps1emu/pcsx-redux
sudo cp src/mips/openbios/openbios.bin /opt/ps1emu/
```

Other locations work too:
`make test PCSX_REDUX=/path/to/pcsx-redux PS1_BIOS=/path/to/openbios.bin`.

---------------------------------------------------------------------------

## macOS (Homebrew)

The core and the GBA ROM build natively. The PS1 disc needs a MIPS cross
compiler, which Homebrew does not ship. The simplest way to get one is to
use Linux in a VM or container (OrbStack, Lima, UTM or Docker with Ubuntu
24.04) and follow the Ubuntu steps there.

```sh
xcode-select --install                     # clang, git
brew install make python cmake ninja ffmpeg
brew install --cask gcc-arm-embedded       # arm-none-eabi-gcc with newlib
brew install --cask mgba                   # to play the ROM
```

* Use **`gmake`** (GNU make 4.x from Homebrew). The system `/usr/bin/make`
  is GNU make 3.81 from 2006, and the Makefiles are only tested with 4.x.
* On macOS, `gcc` is Apple clang. The core is plain C99 and is meant to
  build with it, but CI only checks GCC.
* The headless GBA harness needs the libmgba development library, which
  Homebrew doesn't package. Build mGBA 0.10 from source with
  `-DBUILD_SHARED=ON` and install it, or let CI run the harness. The ROM
  builds without it.
* If you do want the PS1 disc natively, build a `mipsel-none-elf` GCC
  (binutils + gcc, `--target=mipsel-none-elf --with-float=soft`) following
  PSn00bSDK's toolchain guide, build PSn00bSDK with
  `-DPSN00BSDK_TARGET=mipsel-none-elf`, and run
  `gmake ps1 PS1_TARGET=mipsel-none-elf`. The `ps1-elf` CMake preset does
  the same.

```sh
gmake doctor
gmake            # core tests + GBA ROM
```

---------------------------------------------------------------------------

## Windows (WSL2)

Use WSL2 with Ubuntu 24.04. Everything then works exactly as on
[Ubuntu](#ubuntu--debian).

```powershell
wsl --install -d Ubuntu-24.04
```

Inside the Ubuntu shell, clone the repository into the Linux file system
(`~/src/...`, not `/mnt/c/...`, which is much slower), then follow the
Ubuntu steps. To play, open the outputs with the Windows builds of mGBA or
DuckStation through `\\wsl$\Ubuntu-24.04\home\<you>\src\...`, or copy them
to Windows: `cp gba/build/doomogachi.gba /mnt/c/Users/<you>/Desktop/`.

---------------------------------------------------------------------------

## Building

From the repository root:

| command | what it does |
|---|---|
| `make` / `make all` | core tests, then the GBA ROMs, then the PS1 discs. The PS1 part is skipped with a note if PSn00bSDK or a MIPS compiler is missing. |
| `make core` | `make -C core test`: native unit tests + scripted playthroughs at 240×160 and 320×240 |
| `make gba` | `gba/build/doomogachi.gba` (game) and `gba/build/doomogachi_et.gba` (engine test) |
| `make ps1` | `ps1/build/doomogachi.{exe,bin,cue}` and `ps1/build-enginetest/` |
| `make trailer` | `trailer/build/trailer.mp4` (960×640, 78 s) |
| `make dist` | builds everything, then copies the ROMs and zips the discs into `dist/` with the release file names |
| `make test` | every test this machine can run (next section) |
| `make doctor` | which tools were found |
| `make clean` | removes every build directory |

Variables: `JOBS=N` (parallel jobs, default: CPU count),
`PSN00BSDK_LIBS=...`, `PS1_TARGET=mipsel-none-elf`, `PCSX_REDUX=...`,
`PS1_BIOS=...`.

The top-level Makefile only delegates. You can always use the module builds
directly:

```sh
make -C core test                 # core/Makefile, see core/native/README.md
make -C core runner               # core/build/runner (headless, scripted, PNG shots)
make -C gba                       # gba/Makefile, see docs/GBA.md
make -C gba engine-test
cd ps1 && cmake --preset ps1 && cmake --build --preset ps1        # see docs/PS1.md
cd ps1 && cmake --preset ps1-pal && cmake --build --preset ps1-pal    # PAL disc
make -C trailer                   # mp4 + gif, see trailer/README.md
trailer/assemble.sh trailer/build/trailer out.mp4    # 1080p cut with the Dippinn Productions logo
```

---------------------------------------------------------------------------

## Tests

`make test` runs:

1. `make -C core test`: native unit tests, then every script in
   `core/tests/scripts/` played by the headless runner at the GBA
   (240×160) and PS1 (320×240) resolutions.
2. The same tests under AddressSanitizer + UBSan (`SAN=1`, in
   `core/build-san/`).
3. `core/tools/xcheck.sh`: every core source compiled with `-Werror` by
   gcc, arm-none-eabi-gcc and mipsel-linux-gnu-gcc against the PSn00bSDK
   headers.
4. `make -C core check-gen`: regenerates every generated file (art,
   roster, maps) into a temporary directory and fails if the committed copy
   is stale.
5. `make -C gba test`: the ROMs in headless mGBA (libmgba, no window): the
   engine tour, a flash save written in one run and read back in the next,
   and a game smoke test. Screenshots go to `gba/build/shots/`.
6. The PS1 harness, if PCSX-Redux is installed: disc boot, memory card
   save checks, the core's scripts on the PS1 build, the engine tour, the
   ADPCM encoder test and the 3D citizen model. Screenshots and TTY logs go
   to `ps1/build*/shots/`.

Parts whose tools are missing are skipped with a note. Steps 1-5 run in CI
on every push.

---------------------------------------------------------------------------

## Playing the builds

### Downloads

* **Releases:** <https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/releases>
  (`doomogachi.gba`, `doomogachi-ps1.zip`, `doomogachi-ps1-pal.zip`,
  engine-test builds, `SHA256SUMS.txt`).
* **Every commit:** Actions → CI → a green run → *Artifacts*
  (`doomogachi-gba`, `doomogachi-ps1`, `doomogachi-ps1-enginetest`). You
  need to be signed in to GitHub.

### Game Boy Advance emulators

* **[mGBA](https://mgba.io) (recommended, and what the tests use).** Open
  `doomogachi.gba`. The ROM uses a 128 KB flash save and mGBA detects it
  automatically. If an old `.sav` of a different size is lying around,
  delete it, or set *Tools → Game overrides → Save type: Flash 1Mb*. Debug
  output (`plat_log`) appears under *Tools → View logs*.
* Other accurate emulators (NanoBoyAdvance, SkyEmu) should work but are
  not tested. Set the save type to Flash 128K if they ask. No BIOS file is
  needed.

### PlayStation emulators

* **[DuckStation](https://www.duckstation.org):** *Start Disc* with
  `doomogachi.cue`, or *Start File* with `doomogachi.exe` to side-load. It
  needs a PlayStation BIOS image dumped from your own console. Saves go to
  the memory card in slot 1 (`BASLUS-DOOMO0` story, `BASLUS-DOOMO1` free
  play).
* **[PCSX-Redux](https://pcsx-redux.consoledev.net) (what the tests use):**
  `pcsx-redux -iso doomogachi.cue -run`, or `-exe doomogachi.exe`. It also
  runs with the free OpenBIOS (`-bios openbios.bin`), so no Sony BIOS is
  needed. PCSX-Redux charges about two cycles per instruction, so CPU-heavy
  scenes run up to about 2x slower than on a real console.
* Use the PAL disc (`doomogachi-ps1-pal.zip`) for PAL consoles or emulators
  set to a PAL BIOS.

---------------------------------------------------------------------------

## Real hardware

### Game Boy Advance

* The ROM has a valid header (logo + complement check), so it boots on any
  GBA, GBA SP, Game Boy Micro, DS or DS Lite (GBA slot).
* **Flash carts:** EverDrive GBA, EZ-Flash Omega / Omega DE, and similar.
  Set the save type to **FLASH 128K** (1 Mbit). Most carts detect it from
  the `FLASH1M_V103` tag in the ROM. The ROM is 1 MB and uses the ROM wait
  states every cartridge supports (WAITCNT 3/1 + prefetch).
* Cheap reproduction carts work if they have a 128 KB flash chip. Write the
  ROM with a GBxCart RW or similar. Carts with only SRAM or EEPROM won't
  keep saves.
* The game never needs a BIOS file and uses only `IntrWait`/`Halt` BIOS
  calls.

### PlayStation

The disc has **no license data** (it's homebrew), so an unmodified console
rejects a burned CD-R. You need one of these:

* **A modchip** (any model that boots imports / backups).
* **A softmod:** [Unirom](https://unirom.github.io) (from a memory card or
  cheat cart), [tonyhax International](https://github.com/socram8888/tonyhax)
  (game-save exploit), or **FreePSXBoot** (memory card exploit, no game
  needed). These let the console boot an unlicensed CD-R.
* **An optical drive emulator** (XStation, PSIO, PicoStation...): copy
  `doomogachi.bin` + `doomogachi.cue` to the SD card.
* **Serial / USB side-loading** with Unirom or Caetla on the console:
  `nops /fast /exe doomogachi.exe /dev/ttyUSB0`. The game's TTY log comes
  back on the same cable.

Burning a CD-R: write the `.cue` in raw **MODE2/2352** (CDRDAO / ImgBurn /
Brasero all handle it), at a low speed, on good media. Use the NTSC disc
for NTSC consoles and the PAL disc for PAL consoles. The NTSC disc on a
PAL TV may show in black and white or roll.

---------------------------------------------------------------------------

## CI and releases

The workflows are in `.github/workflows/`. Everything runs on
`ubuntu-24.04` with apt packages, plus PSn00bSDK built from source.

**`ci.yml`**: on every push, pull request and manual run (*Actions → CI →
Run workflow*):

| job | steps | artifacts |
|---|---|---|
| core | `make -C core test`, the same with `SAN=1`, `xcheck.sh` (with the pinned PSn00bSDK headers), `make -C core check-gen` | `core-native-screenshots` |
| gba | `make -C gba`, `make -C gba engine-test`, `make -C gba test` (headless libmgba, no display needed) | `doomogachi-gba`, `gba-harness-screenshots` |
| ps1 | PSn00bSDK (cached), `cmake --preset ps1` + build, `cmake --preset ps1-enginetest` + build | `doomogachi-ps1`, `doomogachi-ps1-enginetest` |
| trailer | only when `trailer/`, the game palette (`palette.c` / `palette.h`) or `ci.yml` changed, or on a manual run: `make -C trailer build/trailer.mp4`. A manual run with *trailer_1080p* also builds the 1080p logo cut, which adds about 3-6 minutes. | `doomogachi-trailer` |

The PSn00bSDK build lives in the local action
`.github/actions/setup-psn00bsdk`. It builds the pinned commit with
`-DSKIP_EXAMPLES=ON -DPSN00BSDK_TARGET=mipsel-linux-gnu` into
`~/psn00bsdk` and caches it with `actions/cache`. The cache key includes
the commit, the target, the runner image and the gcc, g++ and cmake
versions, so a toolchain update rebuilds it. To move to a newer SDK,
change `PSN00BSDK_COMMIT` in both workflows.

Pushes to the same branch, and to the same pull request, cancel older runs
that are still in progress, except on the default branch.

**`release.yml`**: push a tag starting with `v`:

```sh
git tag v0.1.0
git push origin v0.1.0
```

It builds both ROMs, runs the GBA harness, builds the NTSC, PAL and
engine-test discs, zips them, and creates the GitHub Release with the files
and `SHA256SUMS.txt` attached. If the release already exists, it replaces
the files. A tag with a hyphen (`v0.2.0-beta.1`) is published as a
pre-release. *Run workflow* with an existing tag rebuilds that release.

---------------------------------------------------------------------------

## Troubleshooting

| symptom | fix |
|---|---|
| `arm-none-eabi/bin/ld: cannot find -lc` | install `libnewlib-arm-none-eabi` |
| `make doctor` says PSn00bSDK MISSING | `export PSN00BSDK_LIBS=<prefix>/lib/libpsn00b` (the directory with `cmake/sdk.cmake`) |
| PSn00bSDK build fails in `examples/` (C++) | configure it with `-DSKIP_EXAMPLES=ON` |
| CMake: `Could not find toolchain file: /cmake/sdk.cmake` | `PSN00BSDK_LIBS` is not set in the shell that runs `cmake --preset` |
| CMake complains about a different generator or toolchain | remove the stale build directory (`rm -rf ps1/build`) and configure again |
| `mkpsxiso` not found | add `<prefix>/bin` to `PATH` (the Makefile does this for you) |
| `check_isa: ... not R3000 clean` | something pulled in a MIPS32 instruction (for example `libgcc`'s 64-bit division). See docs/PS1.md section 1 |
| GBA harness: `mgba/core/core.h: No such file` | install `libmgba-dev` (0.10) |
| ASan aborts at start with `DEADLYSIGNAL` | newer kernels randomise mmap more than ASan supports: `sudo sysctl -w vm.mmap_rnd_bits=28` (CI does this) |
| `xcheck: WARNING: PSn00bSDK headers not found` | the PS1 leg was skipped. Set `PSN00BSDK_LIBS` or `PSN00BSDK_INCLUDE` |
| mGBA does not keep saves | delete the stale `.sav` or force *Save type: Flash 1Mb* |
| PS1 disc boots to the BIOS screen on a console | the console can't boot unlicensed discs. See [Real hardware](#playstation) |
