# BOREagotchi LIFE! on the Dreamcast

The Dreamcast port is being built in stages (see the DC packages in the
project plan). This document grows with it. Right now it covers:

1. The toolchain: the sh-elf cross compiler and KallistiOS, installed by
   `dc/tools/setup_kos.sh` at pinned versions.
2. The headless harness: `dc/harness/dcrun`, a libretro frontend for the
   Flycast core (details in `dc/harness/README.md`).
3. The `dc/` build (DC2): the skeleton that links the whole core, boots,
   runs the 30 Hz loop and changes scenes, the harness mailbox, the RAM
   budget and the self-booting CDI.
4. The 2D layer (DC3): every 2D call and post effect on the PVR, the core's
   320x240 screen drawn at 2x, with a host test that compares it pixel for
   pixel with the native runner.

The SH4 compile check that runs today, with no KallistiOS at all, is the
`sh-elf-gcc` leg of `core/tools/xcheck.sh` (docs/BUILDING.md, "Tests").

---------------------------------------------------------------------------

## 1. Toolchain

The port uses [KallistiOS](https://github.com/KallistiOS/KallistiOS) (KOS),
the free Dreamcast development library, and the GCC toolchain built by its
own builder, `utils/kos-chain`. Nothing comes from Sega: no SDK, no BIOS,
no boot ROM.

### Pinned versions

Everything is pinned in `dc/tools/kos_pins.env`:

| part | version |
|---|---|
| KallistiOS | commit `d81aa3fda0520b6ca64098ce9d1d43e7562142d9` (2026-10-27) |
| kos-chain profile | `stable` |
| binutils / gcc / newlib (sh-elf) | 2.45.1 / 15.2.0 / 4.6.0.20260123 |
| gcc languages | C only |
| SH4 float ABIs (multilibs) | `m4-single` (default) and `m4-single-only` |
| gcc thread model | `kos` |
| img4dc (`cdi4dc`, `mds4dc`) | commit `d28df15db97db1b410e52191eeb45534dcbba252` (Kazade/img4dc) |

`m4-single` is the KallistiOS default ABI: functions start in single
precision and switch to double precision where 64-bit `double` is used.
`m4-single-only` (all floating point is 32-bit) is built too, so the port
can switch with `KOS_SH4_PRECISION` if the core's few `double`s ever
matter for speed.

### Installing

```sh
dc/tools/setup_kos.sh --install-deps      # Ubuntu/Debian: apt-get the host packages first
dc/tools/setup_kos.sh                     # or install them yourself (list below)
source /opt/toolchains/dc/kos/environ.sh  # in every shell that builds for the Dreamcast
```

Host packages (Ubuntu 24.04 names):

```sh
sudo apt install build-essential git curl gawk patch bzip2 xz-utils texinfo flex bison \
                 libelf-dev libgmp-dev libmpfr-dev libmpc-dev libpng-dev libjpeg-dev \
                 cmake genisoimage
```

The prefix is `/opt/toolchains/dc` (the KallistiOS convention) when it is
writable, otherwise `$HOME/dc`. Pick another with `--prefix DIR` or
`DC_PREFIX=DIR`. The script never uses `sudo` except for
`--install-deps`. To use `/opt` without root, create the directory once:
`sudo mkdir -p /opt/toolchains/dc && sudo chown "$USER" /opt/toolchains/dc`.

What ends up in the prefix:

| path | what |
|---|---|
| `sh-elf/` | the cross toolchain (`KOS_CC_BASE`): `sh-elf-gcc`, binutils, newlib |
| `kos/` | KallistiOS, built (`KOS_BASE`): `lib/dreamcast/libkallisti.a`, addons, `environ.sh` |
| `bin/` | host tools (`DC_TOOLS_BASE`): `makeip`, `scramble`, `bin2c`, `bin2o`, `genromfs`, `cdi4dc`, `mds4dc` |
| `.setup_kos/` | the script's stamps (and logs while a stage is running) |

`kos/environ.sh` is KallistiOS' `doc/environ.sh.sample` with this prefix
filled in. It also puts `sh-elf/bin` and `bin/` at the **front** of `PATH`,
so after sourcing it `sh-elf-gcc` is the KallistiOS compiler and not the
apt `gcc-sh-elf` that the xcheck SH4 leg uses. With it sourced, `xcheck.sh`
checks the core against the KallistiOS compiler and `$KOS_SH4_PRECISION`.

### What the script does

1. Checks the host tools and headers, then fetches KallistiOS at the pinned
   commit with `git fetch --depth 1` (git, because plain `curl` to
   github.com is refused by some proxies).
2. Writes kos-chain's `Makefile.cfg` from `Makefile.dreamcast.cfg`:
   profile `stable`, `toolchain_path=<prefix>/sh-elf`, `makejobs` from
   `--jobs` (default 2), `erase=1`, C only, both precision multilibs,
   `thread_model=kos`, and `use_custom_dependencies=1` so that kos-chain
   fetches GMP, MPFR, MPC and ISL itself over https (gcc's own
   `contrib/download_prerequisites` uses plain http, which an https-only
   proxy refuses). It refuses to build if the profile's binutils, gcc
   or newlib version differs from the pins.
3. Runs kos-chain one stage at a time (binutils, gcc pass 1, newlib, gcc
   pass 2), deletes binutils' and newlib's sources as soon as they are
   installed, and deletes `gcc/testsuite` (except `selftests/`, which the
   build reads) right after unpacking gcc: about 110,000 files and 0.9 GB
   that only `make check` uses. kos-chain downloads the GNU sources from
   ftpmirror.gnu.org (fallback mirrors.kernel.org), newlib from sourceware.org and GMP, MPFR,
   MPC and ISL (versions from the kos-chain profile) from gcc.gnu.org, all
   over https.
4. Deletes every kos-chain download, archive, build tree and log.
5. Writes `kos/environ.sh` and builds KallistiOS with `make -j<jobs>`
   (kernel, addons, and the host utilities in `utils/`). With no ARM
   toolchain installed, KallistiOS uses its prebuilt AICA sound driver
   (`stream.drv.prebuilt`), which is what `snd_stream` needs.
6. Copies `makeip`, `scramble`, `bin2c`, `bin2o` and `genromfs` to `bin/`,
   then builds img4dc at its pinned commit and installs `cdi4dc` and
   `mds4dc` there.
7. Checks the result (gcc version, `--enable-threads=kos`, the
   `m4-single-only` multilib, every host tool) and prints the size.

Each stage writes a stamp in `.setup_kos/` with the pins it was built
from. Running the script again with the same pins is a no-op. Changing
`KOS_COMMIT` rebuilds KallistiOS, and also the toolchain if the
`utils/kos-chain` tree differs at the new commit. An interrupted run
resumes at the toolchain stage it stopped in.

Other options: `--check` (verify only; exit 1 if anything is missing or
out of date), `--verify` (also build `examples/dreamcast/hello` in a
temporary copy), `--force` (rebuild everything), `--keep-sources` (keep
kos-chain's downloads and logs), `--min-free-gb N` (default 4),
`--print-profile` (the versions kos-chain's profile uses at the pinned
commit, for updating the pins).

### Measured cost

Measured on the development container (4 CPUs shared with another busy
build, `-j2`, Ubuntu 24.04, host gcc 13.3), 2026-11-01, with
`dc/tools/setup_kos.sh --force --verify` from an empty toolchain:

| | |
|---|---|
| wall time, whole install | 2348 s (39 min) |
| binutils / gcc pass 1 / newlib / gcc pass 2 | 74 s / 1066 s / 92 s / 1101 s |
| KallistiOS (kernel, addons, utils) / img4dc | about 30-40 s / 3 s |
| peak disk use under the prefix | 3.40 GB (sampled every 10-15 s) |
| installed size | 278 MB: `sh-elf/` 194 MB, `kos/` 85 MB, `bin/` 0.3 MB |
| second run (nothing to do) | 0.06 s |

Without the `gcc/testsuite` cut the peak was 3.98 GB. The peak is during
gcc pass 2: the pruned gcc source tree (about 770 MB with GMP/MPFR/MPC/ISL),
the pass 2 build tree and the toolchain installed so far. Downloads are
about 140 MB (gcc 101 MB, binutils, newlib, GMP/MPFR/MPC/ISL).

### Updating the pins

1. Change `KOS_COMMIT` in `dc/tools/kos_pins.env`.
2. Run `dc/tools/setup_kos.sh --print-profile`. It prints the binutils,
   gcc and newlib versions of the `stable` profile at the new commit. Copy
   them into the `KOS_*_VER` lines.
3. Run the script, then `dc/tools/setup_kos.sh --check --verify`.
4. Update the table above and the versions table in docs/BUILDING.md.

CI uses the same pins through `.github/actions/setup-kos`, whose cache key
hashes `kos_pins.env` and `setup_kos.sh`, so any change there rebuilds the
cached toolchain.

### CI

`.github/actions/setup-kos` installs the host packages, restores the
prefix from `actions/cache` (key: the pins file, the setup script, the
runner image and the host gcc version) or runs `setup_kos.sh` on a miss,
then runs `setup_kos.sh --check --verify` and exports everything
`environ.sh` sets (`KOS_*`, `DC_*`) plus the two `bin` directories for the
later steps of the job. No workflow uses it yet: the Dreamcast CI job
arrives with the `dc/` build.

### The core under gcc 15

With `kos/environ.sh` sourced, `core/tools/xcheck.sh` compiles the core
with the KallistiOS gcc 15.2.0 instead of apt's gcc 13.2. On 2026-11-01
that leg passed 345 of 346 files: gcc 15 reported a false
`-Wmaybe-uninitialized` on the `EllQ e` quadrant state in
`ellipse_draw()` (`core/src/ui_raster.c`), which is only read when `fast`
is set and is always initialised then. DC2 initialises it
(`EllQ e = {0};`), since the `dc/` build compiles the core with gcc 15 and
`-Werror`; every core file now compiles warning-free there.

### Troubleshooting (toolchain)

| symptom | fix |
|---|---|
| `missing host tools/headers: makeinfo ...` | install the host packages above (or `--install-deps`) |
| `only N MB free on <prefix>` | the gcc build peaks at about 3.4 GB; free space or use `--prefix` on another disk |
| `kos-chain profile 'stable' builds gcc X, but kos_pins.env pins Y` | the KallistiOS commit moved the profile: update the pins (`--print-profile`) |
| `kos-chain failed: make build-...` | the last lines of each kos-chain log are printed; the full logs stay in `<prefix>/.setup_kos/kos-chain.log` and `<prefix>/kos/utils/kos-chain/logs/`. Rerun to resume |
| `<prefix>/kos has local changes to tracked files` | the KallistiOS tree was edited; commit or discard the edits, or `--force` |
| `sh-elf-gcc --version` says 13.2 after sourcing `environ.sh` | an older shell already had another `sh-elf-gcc` first in `PATH`; open a new shell, or run `$KOS_CC_BASE/bin/sh-elf-gcc` |

---------------------------------------------------------------------------

## 2. Headless harness

`dc/harness/` holds the test harness the port's scripts will run on, the
Dreamcast counterpart of `gba/harness` and the PS1 harness:

- `build_flycast.sh` builds the Flycast libretro core
  (`libretro/flycast` at `e7085e20fe8d81257e11aea5f19a9c1ca2b1bd55`,
  2026-10-31; `make platform=unix`, whose output `parallel_dc_libretro.so`
  is copied to `dc/harness/build/flycast_libretro.so`), then deletes its source tree.
- `dcrun` loads the core, boots an `.elf`/`.cdi`/`.gdi`/`.chd` with
  Flycast's HLE BIOS (no Sega files), renders through an EGL surfaceless
  GL context (Mesa llvmpipe, GL 4.5 core) into an FBO, and plays the
  runner's script language on 4 ports with the Dreamcast extensions
  (`port`, `device`, `stick`, `trigger`, `type`, `mouse`,
  `expect-rumble`, `vmu-shot`, `expect-audio`).
- `dc_mailbox.h` is the contract with the game: the GBA mailbox's fields
  plus a 4 KB `plat_log` ring and the last scene name, found through the
  `dc_mailbox` symbol (`sh-elf-nm`) and read through the core's
  `RETRO_MEMORY_SYSTEM_RAM`. The game side (`dc/src`) arrives in DC2.
- Runs repeat: the core options are pinned by `dcrun` (threaded rendering
  off, 1x, VGA, NTSC, ...), and the Dreamcast clock boots at a fixed
  time (`--rtc`, with `TZ=UTC0` pinned because Flycast adds the host's
  local-time offset).

`make -C dc/harness test` checks `dcrun` itself against a stand-in core
(`dc/harness/tests/mockcore.c`): every script command, the mailbox,
PNG output at 640x480 and 320x240, byte-identical PNGs over two runs, and
the exit status for failed `expect-log`/`expect-scene`, timeouts and a
game that never writes the mailbox. It needs no emulator and no
KallistiOS.

Status (2026-11-01): `dcrun` and its self-test are done and pass (also
under ASan/UBSan). The Flycast core has not been built in the development
sandbox yet (building third-party code there needs the user's approval),
so booting KallistiOS examples and the determinism check on real
Dreamcast frames are still to do: `make -C dc/harness flycast`, then
`dcrun build/flycast_libretro.so $KOS_BASE/examples/dreamcast/pvr/plasma/plasma.elf --script dc/harness/scripts/kos_example.txt` twice and `cmp` the PNGs.

## 3. The dc/ build (DC2: the skeleton)

```sh
make dc                 # from the repo root: dc/build/doomogachi.elf + .cdi
make dc-test            # + check-elf (+ the Flycast harness scripts once dc/harness exists)
make -C dc              # the ELF only (+ .map and the RAM report)
make -C dc disc         # the CDI (+ 1ST_READ.BIN, IP.BIN)
make -C dc check-elf    # ELF checks, no emulator
make -C dc clean
```

`dc/Makefile` sources `<prefix>/kos/environ.sh` itself when `KOS_BASE` is
not set (prefix: `DC_PREFIX`, else `/opt/toolchains/dc`, else `~/dc`) and
prints how to install KallistiOS when there is none. The root Makefile's
`make dc` / `make dc-test` check the toolchain, `cdi4dc` and mkisofs first
(friendly message, as for the PS1), and `make doctor` lists them with the
Flycast libretro core of the harness. `make all` and `make test` do not
build the Dreamcast yet.

### Compile and link

Every core source from `core/sources.mk` and every `dc/src/*.c` is compiled
by KallistiOS' `kos-cc` wrapper, which adds `KOS_CFLAGS` (`-O2 -m4-single
-ml -mfsrra -mfsca -ffunction-sections -fdata-sections -g -Wall`, the KOS
include paths) to our `-std=gnu99 -Wextra -Werror -fno-strict-aliasing`.
The core keeps to C99; gnu99 is for the KallistiOS headers. Unlike the
GBA build, core files are built with `-Werror` too. The objects are linked
directly (never through an archive, so the core's weak stubs are
overridden) by `kos-cc`, which adds KOS' linker script, `--gc-sections`
and `-lkallisti -lm -lc -lgcc`; the link writes `dc/build/doomogachi.map`.

### Boot and the frame loop (`dc/src/main.c`)

`KOS_INIT_FLAGS(INIT_DEFAULT)`: IRQs, threads, the file systems, Maple and
the CD drive. `plat_init_hw()` sets up the mailbox, adds a vblank handler
(a vblank counter the loop idles on) and calls
`vid_set_mode(DM_640x480, PM_RGB565)`, which gives 480p on a VGA cable and
interlaced 640x480 on RGB/composite, then `dc2d_be_init()` (section 4:
one presorted translucent list, ARGB1555 palettes, dithering off) and
`dc2d_init()`. The core runs on a 320x240 logical screen
(`plat_screen_w/h`), which DC3 draws at 2x.

The loop is the platform.h protocol. Logic runs at 30 Hz on the SH4's
microsecond timer (`timer_us_gettime64`), not on vblanks, so the game runs
at the same speed on 60 Hz and 50 Hz (PAL) displays. A drawn tick is one
PVR scene: the tick's drawing collects in the 2D layer's list, and
`plat_end_frame()` submits it (`dc2d_end()`: wait for the PVR, open a
scene, send the list, finish; KOS submits blank lists for the lists
nothing was drawn into) and the PVR shows it at the next vblank while the
CPU runs the next tick; then the loop sleeps on vblanks until the next
tick is due. When the game
falls more than a tick behind, the next tick skips its drawing
(`plat_draw_skipped()` = 1), never two in a row; more than 6 ticks behind
(loading) forgets the debt. Each drawn tick's work time (begin to end, in
microseconds) goes to the mailbox's `work_sum/work_n/work_max`.

The controller (DC4 adds the analog stick, the keyboard and pads 2-4):
D-pad, A, B, X, Y as themselves, L/R triggers past half as L/R, START,
and START+Y as SELECT (DECISIONS.md; the pad has no SELECT).

The skeleton drew only `plat_clear()`'s colour; DC3 draws all of the 2D
(section 4). The placeholders left are in `dc/src/plat_stub.c`:

| group | today | replaced by |
|---|---|---|
| 2D, post effects, held frame | **done** (`dc/src/gfx2d.c`, section 4) | DC3 |
| retained panels, `plat_figure_obj`, the stage sprites | the core's weak defaults (draw every frame; cheap on the PVR) | - |
| saves | a RAM disk (one buffer per slot, lost at power-off) and a static 136 KB scratch | DC4 (VMU) |
| `plat_tone`, `plat_sample` | silent; the core's weak music bridge drives `plat_tone` | DC5 |
| `plat_view3d`, `plat_overworld`, `plat_room` | no-ops / weak defaults | DC6, DC7, DC8 |

`make -C dc check-elf` lists the core's weak platform defaults still in the
link, so each package can see what it has left.

### The harness mailbox (`dc_mailbox`, `dc/harness/dc_mailbox.h`)

The layout is one header shared by the game and the Flycast harness (DC1):
`dc/harness/dc_mailbox.h`, which `_Static_assert`s its size on both
compilers. The harness finds the `dc_mailbox` symbol in the ELF
(`sh-elf-nm`; the SH ABI prints C names with a leading `_`, so
`dc/src/misc.c` also exports the plain name as an alias) and reads and
writes it through libretro's `RETRO_MEMORY_SYSTEM_RAM` region at
`address & 0xFFFFFF`. `check-elf` checks the size and the alias and prints
the offset. It is 32-byte aligned and 4228 bytes:

| field | what |
|---|---|
| `magic` ... `draw_all` | the GBA's `GbaMailbox`, field for field: magic `"DCMB"` (0x424D4344), logic ticks, drawn frames, fps x 10, cmd/arg/result, the command's time (microseconds), work time sum/count/worst, `name[20]`, the "sync on" `hold_at`, `draw_all` |
| `scene[24]` | the scene on top of the stack (from the core's `scene: set/push/pop` log lines) |
| `log_wr`, `log[4096]` | the log ring: every `plat_log()` line plus `\n`, at `log_wr % 4096`; `log_wr` counts every byte ever written |
| `gfx_prims` ... `gfx_tex_live` | DC3's 2D counters after each drawn frame (section 4, Counters) |

The cable type and the boot time are log lines (`DC: video 640x480 VGA`,
`DC: core up in N us`), not mailbox fields.

Log lines also go to `dbgio` (the serial port on hardware, the emulator's
console). Harness commands (`dc/src/hcmd.c`) keep the GBA's ids and
meaning: 1 save, 2 load, 3 exists, 4 town-fill, 5 town-crc, 6 verify,
7 scene, 8 push, 9 seed (pins `plat_entropy`), 10 story flag, 11/12 the
text probe (dcrun has no expect-text yet), 13 story fingerprint.
Results follow the GBA: nonzero means success (dc_mailbox.h). The harness writes `arg`
(and `name`), then `cmd`; the game runs it between two logic ticks.
`dc/tests/boot.txt` is the first harness script: boot, `scene: set BOOT`,
`scene: set TITLE`, then 300 video frames (5 s) that must advance the tick count by
149..151 (`expect-ticks`, the 30 Hz check).
`make -C dc test` runs it on the ELF and on the CDI once
`dc/harness/build/flycast_libretro.so` is built; until then it runs the
harness's self-test only and says so.

### RAM

The whole program loads into the 16 MB of main RAM at 0x8c010000; what is
above `_end` is the heap and the thread stacks. `make -C dc` prints the
budget (`dc/tools/elfsize.sh`) and fails if less than 4 MB
(`DC_RAM_MIN_FREE`) would be left. Measured 2026-11-01 (DC2):

| part | bytes | MB |
|---|---|---|
| code (`.text .init .fini`) | 1,580,720 | 1.51 |
| constant data (`.rodata` ...) | 2,815,780 | 2.69 |
| `.data` | 6,272 | 0.01 |
| `.bss` | 537,892 | 0.51 |
| **image** | **4,940,664** | **4.71** |
| free above `_end` (heap, stacks) | 11,770,428 | 11.23 |

`.bss` includes the RAM-disk saves (128 KB) and the save scratch (136 KB)
that DC4 replaces. With DC3 (2026-11-08): code 1,712,240 B, `.bss`
2,804,724 B (the 2D layer's three prim lists and texture scratch), image
7,335,972 B (7.00 MB), 8.93 MB free above `_end`. The ELF file is 16 MB because of debug info; the
scrambled `1ST_READ.BIN` is 4.4 MB.

### Disc image (`dc/tools/mkcdi.sh`)

```
doomogachi.elf --objcopy -O binary--> doomogachi.bin --scramble--> 1ST_READ.BIN
dc/ip.txt --makeip--> IP.BIN
1ST_READ.BIN + IP.BIN --genisoimage -C 0,11702 -G IP.BIN--> doomogachi.iso
doomogachi.iso --cdi4dc--> doomogachi.cdi   (audio/data, self-booting)
```

`dc/ip.txt` holds our own title ("BOREAGOTCHI LIFE! IN HELLVILE PARK..."),
maker ("BOREAGOTCHI TEAM"), product number T-DOOMO and version. makeip adds the
hardware and maker identifiers the boot ROM requires in every bootstrap
(`SEGA SEGAKATANA`, `SEGA ENTERPRISES`). The IP.BIN bootstrap that makeip
writes also contains the standard license text ("PRODUCED BY OR UNDER
LICENSE FROM SEGA ENTERPRISES, LTD."), which the console's boot screen
draws when a disc starts: it is part of what makes a disc self-boot, not
something the game draws, but it does mean the boot shows Sega's name.
Whether that is acceptable for a release (trademark question) is open for
the user to decide; the game itself draws no logo or jingle of anyone's.
The image is 6.9 MB. A GDI
or CHD release (DECISIONS.md) can be made from the same 1ST_READ.BIN and
IP.BIN, which the build copies next to the CDI.

### Verification so far

DC3 (the 2D layer) is checked on the host: see section 4. Without the
Flycast harness (DC1) in the tree, DC2 was checked with the
KallistiOS build only: every core file compiles with gcc 15 and `-Werror`,
the ELF links with no undefined symbols, `check-elf` passes (SH
little-endian, entry 0x8c010000, the mailbox present, aligned and of the
right size) and the CDI builds. Booting the ELF and the CDI in Flycast
(`dc/tests/boot.txt`: BOOT then TITLE, 30 ticks per emulated second) is
the first thing to run once `dc/harness` lands.


## 4. The 2D layer (DC3)

```sh
make dc-host-test        # from the repo root, host gcc only (no KOS, no emulator)
make -C dc test          # the KOS build + check-elf + the host test + the harness
```

The core draws a 320x240 logical screen; the PVR draws it at 2x into the
640x480 video mode, so logical pixel (x, y) is the 2x2 block at (2x, 2y).
Files (`dc/src/`):

| file | what | portable |
|---|---|---|
| `dc2d.h` | the prim list, the encoding rules, the backend interface | yes |
| `gfx2d.c` | every platform.h 2D call, the post effects, `plat_hold_frame` | yes |
| `texcache.c` | DoomoImages as twiddled PAL8 textures in video RAM, LRU | yes |
| `font.c` | the glyph page and the outline page; the checker texture | yes |
| `pal.c` | the four palette banks; the shade factors of the fallbacks | yes |
| `pvr2d.c` | the PVR: init, textures, palettes, the submit | KOS only |

"Portable" files are also built on the host by `dc/tests/gfx2d` (below),
which is how this package was verified without Flycast.

### How a frame is drawn

Every 2D call appends axis-aligned prims (`DcPrim`: a rect of the 640x480
frame, texture coordinates, a mode) to the tick's list, in the core's
order. `plat_end_frame()` sends the list to the PVR as ONE translucent
list with autosort off (`pvr_init_params_t.autosort_disabled`), depth
compare ALWAYS and no depth writes: the PVR then draws translucent
polygons in submission order, which is exactly the painter's order of
the core, shading and blending included. The opaque list stays enabled
for the 3D packages (DC6-DC8), which the PVR draws first, under the 2D.
A prim goes as a **sprite** (one 32 B header that also holds the colour,
then 64 B with 16-bit UVs) when its UVs survive the 16-bit truncation,
else as a 4-vertex polygon (128 B). A header is only sent when the mode,
texture, palette bank or (sprite) colour change.

| call | on the PVR | exact? |
|---|---|---|
| `plat_clear` | the background plane colour; the list so far is dropped | yes |
| `plat_rect` | an untextured sprite; a rect that continues the previous one (same colour, next to it in a row or a column) grows it instead, so the UI's thousands of 1-row spans cost few prims; an opaque full-screen rect drops the list before it | yes |
| `plat_line` | the native runner's Bresenham walk, gathered into straight runs (one rect each) | yes |
| `plat_checker` | an 8x8 checker texture repeating at one texel per logical pixel, times the colour (phase at the rect's corner) | yes |
| `plat_text` | one sprite per inked glyph from the glyph page (an 8x8 cell per byte value, so no lookup), through the white palette bank times the colour | yes |
| `plat_text_outline` | first the outline page (each glyph stamped at the 8 neighbours and at +1,+2: the ten passes of the weak default) for every character, then the glyphs: 2 sprites per character instead of 10 | yes |
| `plat_image` | the image's texture at scale 1..4; FLIPX swaps U; SILHOUETTE is the white bank times the colour | yes |
| `plat_image` IMG_SHADOW | the image's opaque runs merged into rects (at most 48), each an exact shade at colormap level 5; a noisier shape (or a list over 6,000 prims) gets a DARK copy (the frame times one fitted grey) | yes / close |
| `plat_shade_rect` | see below | yes |
| `plat_image_xform` | one quad at any Q8.8 scale (the weak default rounds to rect runs) | by design not the native look |
| `plat_post_offset`, `plat_post_scale` | move / scale the prims drawn so far (the picture; the HUD drawn after `camfx_apply` stays put) | the native runner does not do them |
| `plat_post_blend` | a translucent sprite at level/16 (16: solid) | the native runner uses a checker or a darken |
| `plat_post_iris` | the weak default's exact black runs and rim, merged | yes |
| `plat_pal_lerp` | palette bank 2 or 3 (alternating by frame, so the frame on the PVR keeps its palette) = `camfx_pal_entry()`; the prims drawn so far switch to it | the native runner does not do it |
| `plat_hold_frame` (U4) | the last submitted list is kept and submitted under every new frame until released (it keeps no palette lerp) | yes |

**Exact shading.** The native runner shades by remapping every pixel
through `doomo_colormap[level]` (each colour walks down its own shade
family). The PVR cannot read the frame back, but the list holds
everything under the rect, so `plat_shade_rect` draws the rect's part of
every earlier prim again with its colour remapped, over the remapped
background colour: SOLID and TINT prims take `colormap[level][colour]`,
images take a remapped COPY of their texture (`dc2d_tex_remap`: the texels
are read back from video RAM, sent through the colormap and cached under
the texture's key and the level); blends are drawn again as they are. A
full-screen shade replaces the list by its copy. Prims are found through
bounding boxes of 64-prim chunks, and anything drawn before an earlier
shade's copy and lying wholly inside its rect is skipped (the copy hides
it), so nested and growing shades (vignettes) stay linear. If the list
would pass 9,000 prims the shade is a MUL sprite instead: the frame times
the level's per-channel factor fitted to the colormap (mean error about
15-24/255 per colour; `pal.c`).

**Palettes** (ARGB1555, which holds the BGR555 master palette exactly and
keeps the texture fetch at full speed): bank 0 the master palette (index
0 transparent), bank 1 all white (TINT and DARK prims multiply it by the
header colour), banks 2 and 3 the lerped palette. Dithering is off, so a
palette colour is stored exactly in the RGB565 frame buffer.

**Textures** (`texcache.c`): one power-of-two PAL8 texture (8..1024 per
side, up to 512 K texels) per image, twiddled as the PVR reads it
(Morton order, V in the low bit; a rectangle is a row of squares; checked
against KOS' `pvr_txr_load_ex` by the unit test), uploaded at the draw
call. The key is the image's uid (the core gives an image a new uid when
its pixels change); an image without a uid is keyed by its pixel pointer
plus a checksum of its pixels, so a buffer rebuilt in place or reused
within a frame is never drawn stale. When video RAM or the 2,048-entry
table is full, textures unused for 120 frames go first, then any not used
by this frame or the last one (which the PVR may still be drawing). The
font pages and the checker are never evicted; the pages are rebuilt when
the language's glyph page (`DoomoLangInfo.page`) changes. Today every
page reads the one table of `font.h` (the native runner does the same);
`font.c`'s `glyph_cols()` is where L4's per-language charset plugs in.

**Budgets.** 12,288 prims per frame (three lists of 590 KB in `.bss`: the
frame, the last frame, the held frame) and a 1 MB vertex buffer (twice:
the PVR keeps two), of which 960 KB is used for prims; a frame over
either drops its last prims and counts them. The `.bss` grew by 2.1 MB
(the three lists and a 512 KB texture scratch); 8.93 MB of RAM stays free.

### Counters

After each drawn frame the mailbox holds `gfx_prims`, `gfx_vbytes` (last
frame), `gfx_prims_max`, `gfx_vbytes_max`, `gfx_dropped`, `gfx_uploads`,
`gfx_evictions` and `gfx_tex_live`; `dcrun` prints them with the game's
work time at the end of a run.

### The host test (`dc/tests/gfx2d`)

`dc2d_runner` is the native runner with its 2D calls renamed to a
reference: `tee.c` defines the platform.h 2D calls so that each one draws
into the native 8bpp frame AND into the Dreamcast list. At every `shot`
the last submitted Dreamcast list is rendered by `swpvr.c`, a software
PVR (the twiddled textures and banks as uploaded, point sampling at pixel
centres, the sprite UV truncation, each mode's blend, an RGB565 frame
buffer without dithering, a 4 MB texture budget), and compared with the
native frame, all four screen pixels of each logical pixel, in RGB565.
`NAME_dc.png` (640x480) and `NAME_diff.png` (grey equal, yellow within
16/255, red not) are written next to the shot. The 3D views (DC6, DC7)
are stand-ins: the native renderer draws the view and its pixels go to
the list as an image, so the 2D drawn over them is still compared.
`DC3_FX=sw` draws the post effects in software on both sides (the native
runner does not do offset, scale and palette lerps, and blends with a
checker), so every other pixel of those shots can be compared too.

`make dc-host-test` = `dc2d_test` (twiddle layout vs `pvr_txr_load_ex`,
16-bit UVs, rect merging, a 120-frame random fuzz of rects, lines,
checkers, text, outlined text, images and irises that must match the
native frame on every pixel, exact shades at every colormap level and
over images, nested shades and a held backdrop, the shade and shadow
fallbacks, the texture cache under video-RAM pressure, held frames, the
vertex budget, the post effects) plus `run_compare.sh` on DC3's UI
scripts (ui_gallery, fp_title, fp_screens, creator, library_lobby,
pause_options) with a gate of 99% exact and 99.9% close per shot. CI runs
it in the core job. `SAN=1 BUILD=...` runs it under ASan/UBSan.

Measured 2026-11-08 (host, this tree):

| what | result |
|---|---|
| `dc2d_test` | 52 checks, 0 failed (also under ASan/UBSan) |
| the six DC3 UI scripts, hardware effects | 94 shots, **100.000% exact** on every shot; at most 2,704 prims and 223 KB of vertices a frame, nothing dropped |
| all 80 runner scripts, `DC3_FX=sw` | 905 shots, 854 exact to the pixel, mean 99.984%; worst 98.59% (home_defend: `room_ref` reuses one uid for translucent actors whose dither changes every frame, so a uid-keyed cache shows the last pattern; the room is DC8's own renderer on the console) |
| all 80 runner scripts, hardware effects | 905 shots, 810 exact; the rest differ where the post effects do their hardware job (shakes, palette lerps, blends) |
| heaviest frame | home_rooms (`room_ref` through the 2D calls): 10,287 prims, 938 KB of vertices, inside the budget; DC8 draws rooms itself |

`dc/harness/compare.py` does the same comparison on real Flycast shots:
`python3 -I dc/harness/compare.py DC_SHOTS NATIVE_SHOTS --diff DIR` (shots
of the same script from `dcrun` and from `core/build/runner --w 320 --h 240`).

### Owed on Flycast and hardware

The Flycast core is not built in this tree (its build needs the owner's
permission rule in the sandbox), so the PVR side itself is unverified:

- the six UI scripts in `dcrun` against the native runner with
  `compare.py` (gate 99% exact), the shots looked at by eye;
- the frame time of the 2D layer (the mailbox's `work_max`, and the
  `gfx_*` counters) on the heaviest screens (island hub, creator,
  home rooms) against the 33 ms tick, on Flycast and on a real console;
- the PVR details the software PVR cannot prove: sprite 16-bit UVs and
  point sampling at 2x, the presorted translucent list, the TA bins on
  1-row spans (`opb_overflow_count` 8), MODULATEALPHA with a white
  texture, video RAM read-back for the remapped textures.

## Rumble (X2)

`plat_rumble(strength 0..255, ticks)` (plat_ext.h) is the core's rumble hook.
`camfx_shake()` calls it for every shake profile with the profile's strongest
frame (amp 1..7 -> 36..252) and its own length at 30 Hz, both scaled by
`OPT_SCREEN_SHAKE`, so "Screen shake: Off" silences the motor as well. The
Dreamcast driver (`dc/src/rumble.c`) sends one KOS Puru Puru effect to the
pack in controller 1's first slot (intensity 1..7, duration from the tick
count; a new pulse replaces the running one) and logs `DC: rumble strength N
ticks N` for the harness. `plat_features()` reports `PLATF_RUMBLE` while a
pack is plugged in. No pack, no effect. The core's other platforms keep the
weak no-op (the PS1 DualShock motor is not wired yet). The option table is
full, so a rumble-only on/off row waits for U0's ids >= 64.

## Boot easter egg (X8)

Hold A + B + X + Y + START on controller 1 while the console boots and the
game opens with a one-second rainbow (`dc/src/egg.c`, `dc_boot_egg()`,
called from `main()` right after `plat_init_hw()`): the PVR background steps
through eight colours every four vblanks, over empty scenes, and the log
shows `DC: boot easter egg`. It touches no saves and no options. Without the
buttons it only waits (at most 90 vblanks) for the controller to appear.

## Mature content pack on the disc (E10)

An optional DMPK pack built for port dc (`python3 tools/mpack/mpack.py build manifest.txt --out pack.dmpk`,
manifest line `port dc`) goes on the disc as `PACK.DMP`:

    make -C dc PACK=/path/pack.dmpk disc

The build validates the pack (`mpack.py validate --port dc`), defines `DOOMO_PACK_ON_DISC`, links
`dc/src/packdisc.c` and copies the pack to the disc root (`mkcdi.sh ... PACK`). Without `PACK=` the disc is the
stock one and links none of it. `plat_pack_read` reads `/cd/PACK.DMP` with a 32 KB window; the mslot scene reads a
slot's script into a 32 KB buffer (`PACK_SCRIPT_CAP_DC`; mpack.py refuses a bigger one) and a still into a
320x240 buffer (not drawn until the 2D layer, DC3, lands). Cost with a pack: about 140 KB BSS. Test without
KallistiOS: `sh dc/tests/packdisc_host.sh` (the real reader over a mocked `fs_*`).
