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
5. Controls and VMU saves (DC4): the controller, analog stick, triggers and
   keyboard; saves and the machine config record on the VMU with an
   atomic replace, the pet's icon and eyecatch, and the card shown while a
   save blocks the game.
6. Sound (DC5): the music (the engine's legacy bridge), the voice babble
   and the sound effects, rendered per logic tick by a software mixer and
   streamed to the AICA, checked against the native runner's `--wav`.
7. The overworld in 3D (DC7): `plat_overworld` as PVR polygons inside the
   2D layer's painter's order (the 3D batches of `dc3d.h`, which DC6 and
   DC8 can use too), checked on Flycast against a software PVR on the host.

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
  (`port`, `device`, `stick`, `trigger`, `key`, `mouse`,
  `expect-rumble`, `vmu-shot`, `expect-audio`). `--sync` / `sync on`
  makes every wait tick-exact (the game holds at the top of its loop,
  as the GBA's "sync on"), `--native-seed N` starts the game in the state
  `runner --seed N` starts in, and `type` feeds the open text entry as the
  native runner does, so a script plays the same ticks on both (DC3).
- `dc_mailbox.h` is the contract with the game: the GBA mailbox's fields
  plus a 4 KB `plat_log` ring and the last scene name, found through the
  `dc_mailbox` symbol (`sh-elf-nm`) and read through the core's
  `RETRO_MEMORY_SYSTEM_RAM`. The game side (`dc/src`) arrives in DC2.
- Runs repeat: the core options are pinned by `dcrun` (threaded rendering
  off, 1x, VGA, NTSC, ...), and the Dreamcast clock boots at a fixed
  time (`--rtc`, with `TZ=UTC0` pinned because Flycast adds the host's
  local-time offset).
- (CL4) dcrun also sets `PLAT_TEST_NO_TIPS` in the mailbox's `test_flags`
  (DC4's word, read by `plat_test_flags()` in `dc/src/hcmd.c`), so the
  first-time tip cards never show in a scripted run and the UI scripts stay
  frame-for-frame with the native runner, which keeps them off too
  (docs/GAME_DESIGN.md 4.2); a script's `tips on` clears it, as the native
  runner's and the GBA harness's do. `--console` leaves the word at 0 (a
  real console never writes the mailbox).

`make -C dc/harness test` checks `dcrun` itself against a stand-in core
(`dc/harness/tests/mockcore.c`): every script command, the mailbox,
PNG output at 640x480 and 320x240, byte-identical PNGs over two runs, and
the exit status for failed `expect-log`/`expect-scene`, timeouts and a
game that never writes the mailbox. It needs no emulator and no
KallistiOS.

Status (2026-11-08): `dcrun` and its self-test are done and pass (also
under ASan/UBSan). The Flycast core is built at the pinned commit (the
owner approved it, DECISIONS.md 2026-11-08) and sits in the toolchain
prefix, `/opt/toolchains/dc/flycast_libretro.so`, where `dc/Makefile`
finds it when `dc/harness/build/` has none. With it, `make -C dc test`
boots the ELF and the CDI with `dc/tests/boot.txt` and runs DC3's UI
comparison (section 4). Still to do: the determinism check on a
KallistiOS example (`dcrun ... plasma.elf --script
dc/harness/scripts/kos_example.txt` twice and `cmp` the PNGs).

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

The controls (controller, analog stick, triggers, keyboard) are section 5
(DC4): D-pad, A, B, X, Y as themselves, L/R triggers past half as L/R,
START, and START+Y as SELECT (DECISIONS.md; the pad has no SELECT).

The skeleton drew only `plat_clear()`'s colour; DC3 draws all of the 2D
(section 4). The placeholders left are in `dc/src/plat_stub.c`:

| group | today | replaced by |
|---|---|---|
| 2D, post effects, held frame | **done** (`dc/src/gfx2d.c`, section 4) | DC3 |
| retained panels, `plat_figure_obj`, the stage sprites | the core's weak defaults (draw every frame; cheap on the PVR) | - |
| saves | **done** (`dc/src/vmusave.c`, section 5: the VMU, a static 136 KB scratch) | DC4 |
| `plat_tone`, `plat_sample`, `plat_sample_ex`, `plat_music_stall` | **done** (`dc/src/audio.c`, `mixer.c`, `sfx.c`, section 6); the music engine stays on the core's weak legacy bridge tier | DC5 |
| `plat_overworld` | **done** (`dc/src/overworld.c` over `dc3d.c` / `pvr3d.c`, section 7) | DC7 |
| `plat_view3d`, `plat_room` | no-ops / weak defaults | DC6, DC8 |

`make -C dc check-elf` lists the core's weak platform defaults still in the
link, so each package can see what it has left.

### Speed tour 2 (SA2, `dc/harness/speed2.sh`)

`sh dc/harness/speed2.sh OUTDIR [groups...]` (after `make -C dc`, the stripped ELF and `doomogachi.nm`) plays the
GBA's speed tour 2 scripts (docs/GBA.md "Speed tour 2") in Flycast: `tools/speed2.py dc` makes each window
`log speed2: begin N NAME`, `mark-ticks`, `frames 2N`, `expect-ticks`, `log speed2: end NAME`, and
`tools/speed2.py table` prints the logic ticks/s and the ticks over 33.3 / 50 ms of work (dcrun's `work:` lines)
per window, with the run's `game drew` and `work over` lines. Flycast is not cycle-accurate: a guide, not a promise.

### The harness mailbox (`dc_mailbox`, `dc/harness/dc_mailbox.h`)

The layout is one header shared by the game and the Flycast harness (DC1):
`dc/harness/dc_mailbox.h`, which `_Static_assert`s its size on both
compilers. The harness finds the `dc_mailbox` symbol in the ELF
(`sh-elf-nm`; the SH ABI prints C names with a leading `_`, so
`dc/src/misc.c` also exports the plain name as an alias) and reads and
writes it through libretro's `RETRO_MEMORY_SYSTEM_RAM` region at
`address & 0xFFFFFF`. `check-elf` checks the size and the alias and prints
the offset. It is 32-byte aligned and 4332 bytes:

| field | what |
|---|---|
| `magic` ... `draw_all` | the GBA's `GbaMailbox`, field for field: magic `"DCMB"` (0x424D4344), logic ticks, drawn frames, fps x 10, cmd/arg/result, the command's time (microseconds), work time sum/count/worst, `name[20]`, the "sync on" `hold_at`, `draw_all` |
| `scene[24]` | the scene on top of the stack (from the core's `scene: set/push/pop` log lines) |
| `log_wr`, `log[4096]` | the log ring: every `plat_log()` line plus `\n`, at `log_wr % 4096`; `log_wr` counts every byte ever written |
| `gfx_prims` ... `gfx_tex_live` | DC3's 2D counters after each drawn frame (section 4, Counters) |
| `test_flags` | DC4: `plat_test_flags()`; dcrun writes `PLAT_TEST_NO_PICKER` and (CL4) `PLAT_TEST_NO_TIPS` when it sees the magic (`--console`: 0; a script's `tips on` clears the tips bit) |
| `snd_pushed` ... `snd_starved` | DC5's sound counters after each tick (section 6, Counters) |
| `g3d_us` ... `g3d_dropped` | DC7's 3D view counters after each drawn frame (section 7, Counters) |

The cable type and the boot time are log lines (`DC: video 640x480 VGA`,
`DC: core up in N us`), not mailbox fields.

Log lines also go to `dbgio` (the serial port on hardware, the emulator's
console). Harness commands (`dc/src/hcmd.c`) keep the GBA's ids and
meaning: 1 save, 2 load, 3 exists, 4 town-fill, 5 town-crc, 6 verify,
7 scene, 8 push, 9 seed (pins `plat_entropy`), 10 story flag, 11/12 the
text probe (what `plat_text` draws), 13 story fingerprint; the
Dreamcast adds 16 text feed (`type`) and 17 the native runner's start
(`--native-seed`), DC3, and DC7 the native runner's story commands: 18
`side`, 19 `talk`, 20 `cell`, 21 the story state (`skip-talk`), 22 "does the
dialogue say" (`expect-text`, `skip-to`; the first 19 characters, the
mailbox's `name`), 23 "is the top scene" (`skip-to-scene`), 24 `clock`. Results follow the GBA: nonzero means success (dc_mailbox.h). The harness writes `arg`
(and `name`), then `cmd`; the game runs it between two logic ticks.
`dc/tests/boot.txt` is the first harness script: boot, `scene: set BOOT`,
`scene: set TITLE`, then 300 video frames (5 s) that must advance the tick count by
149..151 (`expect-ticks`, the 30 Hz check).
`make -C dc test` runs it on the ELF (without debug info) and on the CDI
when the Flycast core is there (`dc/harness/build/` or the toolchain
prefix); without it, it runs the harness's self-test only and says so.

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

`.bss` included the RAM-disk saves (128 KB) and the save scratch (136 KB);
DC4 replaced the RAM disk with two VMU file buffers (92 KB each) and the
card's metadata (8.7 KB): code 1,723,248 B, `.bss` 2,897,492 B, image
7,442,688 B (7.10 MB), 8.84 MB free above `_end` (2026-11-08). With DC3 (2026-11-08): code 1,712,240 B, `.bss`
2,804,724 B (the 2D layer's three prim lists and texture scratch), image
7,335,972 B (7.00 MB), 8.93 MB free above `_end`. With DC5 (2026-11-08): code 1,749,488 B,
`.bss` 3,022,932 B (the PCM ring 94,080 B, the stream's copy buffer 12,288 B), image
7,599,060 B (7.25 MB), 8.69 MB free above `_end`, of which the effects bank takes 95,656 B of heap.
With DC7 (2026-11-08, on the tree with Talk T3/T4 and the clarity work): code 1,772,336 B,
`.bss` 4,277,940 B (the three 3D batches 1.13 MB, the overworld's corner tables 17 KB),
image 8,878,784 B (8.47 MB), 7.46 MB free above `_end`.
The ELF file is 17 MB because of debug info; the scrambled `1ST_READ.BIN` is 4.4 MB.

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

The KallistiOS build: every core file compiles with gcc 15 and `-Werror`,
the ELF links with no undefined symbols, `check-elf` passes (SH
little-endian, entry 0x8c010000, the mailbox present, aligned and of the
right size) and the CDI builds. On Flycast (2026-11-08) the ELF and the
CDI boot with `dc/tests/boot.txt`: BOOT then TITLE, 150 ticks in 300
video frames. The ELF with its debug info is about 17 MB and Flycast
cannot open an ELF over 16 MiB, so the harness boots
`dc/build/doomogachi-nodebug.elf` (`strip --strip-debug`, about 4.5 MB);
the symbols come from `doomogachi.nm`. The 2D layer (DC3): section 4.


## 4. The 2D layer (DC3)

```sh
make dc-host-test        # from the repo root, host gcc only (no KOS, no emulator)
make -C dc ui-compare    # the six UI scripts on Flycast against the native runner
make -C dc test          # the KOS build + check-elf + the host test + the harness (+ ui-compare)
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
the core, shading and blending included. The 3D views go into the same
list at the point the core drew them (DC7, section 7: a marker prim and
real depth among the view's own polygons); the opaque list stays enabled
but nothing uses it.
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
16/255, red not) are written next to the shot. The 3D views are
stand-ins here: the native renderer draws the view and its pixels go to
the list as an image, so the 2D drawn over them is still compared
(`DC7_OW=dc` draws the Dreamcast's own overworld instead, section 7).
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

### On Flycast

`make -C dc ui-compare` (also the last step of `make -C dc test` when the
Flycast core is there) runs `dc/harness/ui_compare.sh`: the six UI
scripts in `dcrun` on the CDI and in `core/build/runner --w 320 --h 240
--pad dc` (C3: the native runner names the buttons as the Dreamcast build
does, START+Y for SELECT; with the host pad the creator's prompt bars
differed in 3% of their pixels), then `dc/harness/compare.py` on each pair of shots (each Dreamcast pixel
block against the native pixel in RGB565; gate 99% exact and 99.9% close).
Shots, diff pictures (grey = exact, yellow = close, red = not) and the
logs go to `dc/build/shots/ui/`, with `summary.txt`.

Both sides play the same ticks: `dcrun --native-seed 0x5EED1234` holds
the game before its first tick and gives it the native runner's start
state (`plat_entropy`, `doomo_rng` and the start town of `runner --seed
0x5EED1234`); every wait is tick-exact (`sync`), `press` is 1 tick down
and 1 up as in the native runner, and `type` feeds the text entry one
character a tick as the native runner does. (The first Flycast runs, made
without these, drifted: dcrun's press was 3 ticks long, its `type` went
through the keyboard at 2 ticks a key, the game booted on real entropy, and
4 of the scripts start with a mailbox command that dcrun refused before
the mailbox was up.)

Measured 2026-11-08 (Flycast `e7085e20`, the CDI):

| script | shots | result | 2D high-water (prims / vertex bytes a frame) | work per tick (mean / max) |
|---|---|---|---|---|
| ui_gallery | 20 | 20 exact | 1,392 / 105,824 | 6.0 / 19.2 ms |
| fp_title | 8 | 8 exact, 5 of them with the platform label masked | 1,647 / 122,880 | 3.5 / 19.2 ms |
| fp_screens | 14 | 14 exact | 2,704 / 222,944 | 10.2 / 253.6 ms (3 loading ticks, below) |
| creator | 36 | 36 exact | 2,681 / 202,432 | 9.5 / 23.1 ms |
| library_lobby | 5 | 5 exact | 1,156 / 93,472 | 7.3 / 16.7 ms |
| pause_options | 11 | 8 exact, the Controls page with its Rumble row masked, 2 pause shots 46.4% (below) | 974 / 69,824 | 4.7 / 19.8 ms |

94 of 94 shots pass; 86 match the native runner on every pixel, and no
prim was dropped (with DC4, 2026-11-08: still 94 of 94, 84 to the pixel;
the two Performance pages gained the Draw distance row, section 5). The
expected differences are listed with their reasons
in `dc/harness/known_diffs.tsv` (compare.py `--known`):

- `plat_name()`: the title's corner label reads DC (native: NATIVE).
- The Controls options page has a Rumble row on the Dreamcast (rumble
  pack); the native runner has none, so the reset row moves down a line.
- The pause menu over the Vape Cellar level: every differing pixel is
  where the native runner shows the 3D level and the Dreamcast draws black,
  because `plat_view3d` on the Dreamcast is DC6. The 2D menu and HUD on
  top are exact.

Frame time against the 30 fps target (DECISIONS.md, FPS TARGET): the work
is the game's own timer from `plat_begin_frame` to the end of the PVR
submit, in Flycast's emulated time (Flycast is not cycle-accurate, so this
is a guide until a real console measures it). Every tick of every screen
in the six scripts stays under 33.3 ms except three loading ticks in
fp_screens: making the new island (tick 11, 96 ms), saving to the VMU
(tick 251, 187 ms) and loading from it into the island (tick 323, 254 ms).
They are one tick each, a pause on a menu action rather than a slow screen.
With DC4's VMU (section 5) the save is a 3.2 s tick and the load a 0.9 s
tick, both with the card on screen. The worst steady tick is 23.1 ms (creator,
the moving-in confirm). The vertex high-water is 223 KB of the 983 KB
budget.

### Owed on hardware

- The frame time on a real console (Flycast's timing is not exact), on
  the heaviest screens (island hub, creator, home rooms); and the three
  loading ticks above (DC4: VMU saves).
- The PVR details an emulator does not prove: sprite 16-bit UVs and
  point sampling at 2x, the presorted translucent list, the TA bins on
  1-row spans (`opb_overflow_count` 8), MODULATEALPHA with a white
  texture, video RAM read-back for the remapped textures (exact shading
  reads textures back with 32-bit uncached reads and tests up to
  thousands of prims for overlap per shade; its speed on the SH4 is
  unmeasured).

## 5. Controls and VMU saves (DC4)

### Controls (`dc/src/pad.c`, `padmap.c`, `dcinput.h`)

Every tick, before `doomo_frame()`, the first controller Maple finds (any
port) and every keyboard are read. The mapping is portable C
(`padmap.c`), checked on the host by `dc/tests/vmu` (test_vmu: every
button, the trigger and stick thresholds, START+Y, the keyboard map and the
on-screen-keyboard rule).

| Dreamcast | the core |
|---|---|
| D-pad, A, B, X, Y | as themselves |
| L / R trigger | L / R when pressed past half (> 127 of 255) |
| START | START |
| START+Y | SELECT, and then neither START nor Y (DECISIONS.md: the pad has no SELECT) |
| C / Z (6-button pads, arcade sticks: no triggers) | L / R |
| D | SELECT |
| second D-pad left / right (twin stick) | L2 / R2 (a standard pad has none, as on the GBA) |
| analog stick | `input_set_analog()` every tick (-128..127, +y down), and past 48 of 128 it also presses the D-pad (the PS1's threshold), so menus follow it |
| second analog stick | `input_set_analog()`'s rx / ry when there is one |

The core's "smooth feel" (`input_set_feel(1)`: eased turning, fire
buffering, aim friction) is on, as on the PS1. One visible consequence: the
Options' Performance page shows the Draw distance row, which the core ties
to `input_feel()` (the PS1's 3D); DC6 is to honour `OPT_DRAW_DIST`. The
DC3 UI comparison lists it in `dc/harness/known_diffs.tsv`.

Keyboard (any port; Dreamcast keyboards send USB HID key codes):

| keys | the core |
|---|---|
| arrows | D-pad |
| Z, Space, Enter | A |
| X, Escape, Backspace | B |
| C / V | X / Y |
| Q / W | L / R |
| 1 / 2 | L2 / R2 |
| P, F1 | START |
| Tab | SELECT |

While the on-screen keyboard is open (`SCENE_TEXT_ENTRY`), the characters
typed go straight into it (`ui_text_entry_feed`, the native runner's `type`
path, from KallistiOS' per-keyboard key queue: printable ASCII, Enter = OK,
Backspace = delete) and only the keys that type nothing (arrows, Escape,
Tab, F1) act as buttons. Accented letters arrive as ISO-8859-1 and are
dropped by the core's text entry today (X5 adds them). `plat_features()`
(now in pad.c) reports `PLATF_CONFIG` always, `PLATF_RUMBLE` while a
Puru Puru pack is in controller 1 and `PLATF_KEYBOARD` while a keyboard is
plugged in. Multi-pad play (`PLATF_MULTIPAD`) is X9's.

On Flycast (`dc/tests/vmu/run_flycast.sh`, "input"): `input_pad.txt` drives
the UI gallery with the D-pad, A, R and the native runner's `type`;
`input_alt.txt` does the same ticks with the analog stick, the analog R
trigger and the keyboard on port 2 (Enter as A, then "\b" x7 + "Candy" + Enter
typed into the on-screen keyboard). All 5 shot pairs are identical to the
byte.

### Saves on the VMU (`dc/src/vmusave.c`, `vms.c`, `save_icon.c`; `vmusave.h`, `vms.h`)

The core's save calls and the machine config record go to the first VMU
Maple finds (A1, A2, B1, ...), one file each:

| file | what | blocks |
|---|---|---|
| `DOOMO0.SYS` | slot 0, story | header 8 + the save (a lived-in story island: 33; at most 88) |
| `DOOMO1.SYS` | slot 1, free play | 8 + the save (a new island 27; the 1000-citizen worst case 124; at most 184) |
| `DOOMOCFG.SYS` | the machine config record (language, volumes, Mature, the adults' code) | 2 |
| `*.NEW` | a save being written (gone when the save ends) | |

Each is a standard VMS file (the layout and CRC-16 of KallistiOS'
`vmu_pkg_build`, built into a static buffer): the descriptions the BIOS
file manager shows ("BOREagotchi" / "BOREagotchi LIFE! - story", "- free
play", "- settings"), application id `BOREAGOTCHI`, an animated 32x32
16-colour icon and a 72x56 16-colour eyecatch of the Boreagotchi as the
save holds it (`save_icon.c`: the core's `pet_render`, the egg when there
is no pet yet, the ghost for a departed one; idle, idle, happy at 12/30 s a
frame; on the tamagotchi's green LCD with a pink rim), then the core's
save. A VMU save of 72 KB (184 blocks) and a 40 KB story save (88 blocks)
together would not fit one VMU's 200 blocks; real saves are far smaller
(above), and a save that does not fit fails cleanly.

The file system code is portable (`vms.c`), so the host test runs it on a
simulated 128 KB VMU (`make -C dc/tests/vmu test`, also in `make
dc-host-test` and CI). It reads and writes blocks through KallistiOS'
`vmu_block_read` / `vmu_block_write` under KallistiOS' VMU lock, and
allocates data files from the top block down, as the BIOS does. Every
update writes the data blocks first, then the FAT, then the directory, so a
card pulled out half way can leak free blocks (allocated, in no file) but
never touches another file.

**A save never leaves a slot half-valid.** `vmu_replace()` writes the new
file as `DOOMOn.NEW`, reads it back and compares it, then removes the old
file (one directory write, then the FAT) and gives the new one its name
(one more directory write). A VMU pulled out at any moment keeps either the
old save or the new one; a load that finds only `DOOMOn.NEW` (the swap was
cut short) loads it, and the next save finishes the swap. The host test
pulls the card out before every single block write of a save (62 points
for a 57-block save): the old save loads whole at 60 of them and the new
one at the last 3, the card stays consistent (at worst 57 leaked blocks
when the pull hits between the FAT and the directory), and the next save
works and leaves no `.NEW` behind. Only when the VMU is too full for two
copies is the old file replaced in place (removed, then written, then read
back); the log says "replaced in place".

**Errors** return 0 to the core (which shows "Couldn't write save slot N")
and are logged (`DC: vmu write DOOMO1.SYS: no VMU`, `... 124 blocks -> VMU
full`); the card shows the reason for a second, once per kind of error: no
VMU, VMU full, too many files, not formatted, a write error. A VMU that is
not formatted is never formatted by the game.

**The card.** A save, a load or an erase blocks the tick, so a picture is
shown in its place: "Saving your game..." (or "Loading your game...",
"Saving settings...", "Erasing..."), "Do not remove the VMU or switch off
the power" and a bar that moves with every block. It is a side frame of
the 2D layer (`dc2d_side_begin/end`: the tick's own list is put aside and
comes back untouched; host-tested in `dc/tests/gfx2d`).
Flycast shot: `dc/build/shots/vmu/saves/shots/saving_card.png`.

**What is known is kept.** A VMU block read costs about a 60th of a second
(Flycast: 15 block reads to learn a card, 262 ms), so `dc_vmu_init()` at
boot reads the root, FAT and directory and the first 64 payload bytes of
the three files once (their headers: what the title screen and the save
menu peek at), and keeps them. The title then never touches the VMU (DC2's
boot.txt: worst tick 10.9 ms; before the cache the title's two peeks took
one 1.12 s tick). Maple's attach and detach callbacks (a VMU pulled out,
put in or swapped), a different first VMU and any error forget it all.
(KallistiOS keeps one attach and one detach callback per Maple driver, and
the VMU's driver also serves the LCD and the clock: X1 must chain to
vmusave.c's callback, not replace it.)

**The config record is written behind.** Since the Refresh the core hands
it over once, when the scene where machine-wide options changed is left
(`opt_config_flush` from `scene_set` / `scene_pop`: leaving the Options,
finishing the picker, the title), and only if something changed; it used
to call `plat_config_write` on every step of a volume slider.
`plat_config_write` keeps the record (reads see it at once) and
`dc_vmu_poll()`, after every tick, writes it when the scene changes or 5 s
after the call. `dc/tests/vmu/options_volume.txt` (vmu-test's `optvol`
check, `dcrun`'s `gaps-reset` / `expect-gap`): ten volume steps, worst
frame 33.4 ms (2 video frames) before and after; leaving the Options, the
one VMU write (0.46 s, the "Saving settings..." card) is a 484 ms frame.

**Test flags.** The language picker shows on the first boot of an empty VMU
(no config record). The harness mailbox has a `test_flags` word
(`plat_test_flags()`); dcrun sets `PLAT_TEST_NO_PICKER` as soon as it sees
the mailbox magic, as the native runner does, and `dcrun --console` leaves
0 (a real console). CL4 adds `PLAT_TEST_NO_TIPS` to the same word (the
first-time tip cards stay off unless a script says `tips on`).

Measured on Flycast (2026-11-08, the CDI, emulated time; Flycast times the
VMU's Maple traffic like the hardware, about 90 ms a block written):

| what | blocks | time |
|---|---|---|
| learn the card at boot (root, FAT, 13 directory blocks, 3 headers) | 15-21 read | 262-322 ms |
| save a new free-play island (fp_screens) | 27 written + 27 read back | 3.05 s |
| save a lived-in story island | 33 + 33 | 3.7 s |
| save the 1000-citizen worst case (58 KB) | 124 + 124 | 13.0 s |
| the same, replaced in place (VMU too full for two copies) | 124 + 124 | 13.1 s |
| load the worst case / a new island | 124 / 27 read | 2.5 s / 0.65 s |
| config record | 2 + 2 | 0.46 s |

So a save is a pause with the card on screen (fp_screens: the VMU save is a
3.2 s tick and the load a 0.9 s tick; every other tick of the six DC3
scripts stays under 33.3 ms). KallistiOS sends each block as Maple frames
that go out once a vblank, which bounds the speed; the read-back costs
about 20% of a save and is what makes the swap safe.

On Flycast (`make -C dc vmu-test`, part of `make -C dc test`;
`dc/tests/vmu/run_flycast.sh`, results in `dc/build/shots/vmu/`):

| check | what |
|---|---|
| saves | `saves1.txt` on an empty VMU, `saves2.txt` on the same image (the power cycle): the worst-case island (124 blocks) and a story island saved, overwritten in RAM and loaded back with the same `town-crc`; slot 0 refuses the 58 KB island; run 2 finds both, verifies, rewrites slot 1 in place. `vmu_check.py` passes on the image after each run (FAT, directory, VMS headers and CRCs, 3 icon frames, no `.NEW` left) and draws the icons |
| full | a VMU with another game's 150-block file (`vmu_check.py --fill 150`): the worst case fails cleanly ("VMU full"), a story save that fits works, the other file's CRC still checks |
| novmu | a keyboard on port 1 and no VMU anywhere: saves fail cleanly, the game runs on to the title |
| console | `--console`: the first boot shows the language picker, the record is written (`DOOMOCFG.SYS`, 2 blocks), the second boot goes straight to the title |
| input | above (stick, trigger, keyboard against the D-pad: identical shots) |

`dc/harness/vmu_check.py IMAGE [--expect NAME] [--json] [--fill N]
[--format] [--icon NAME OUT.png]` reads a Flycast VMU image
(`RUN/system/dc/vmu_save_A1.bin`) with its own Python parser, independent
of `vms.c`; the host test also runs it on the card image it writes.

### Owed on hardware (DC4)

- A real VMU: the write time per block, the read-back, and a card pulled
  out during a save (the host test simulates it at every block write;
  Flycast cannot pull a VMU mid-write).
- A real keyboard and a twin stick / arcade stick (Flycast's keyboard is
  used in the tests; the C/Z/D buttons and the second D-pad are host-tested
  only).
- The BIOS file manager showing the icon, the eyecatch and the descriptions
  (vmu_check.py checks the bytes and draws the icons; no BIOS is run).
- The shooter turning with the stick: the core's analog path is fed and
  the menus follow the stick on Flycast, but the 3D view is DC6.

## 6. Sound (DC5)

```sh
make -C dc audio-test        # on Flycast: title, invaded town, combat, the hash twice
make -C dc/tests/audio test  # on the host: the mixer's units + 4 scripts against musrender
```

### What plays, and how

The core speaks to the platform through `plat_tone()` (four chiptune
channels: the music engine's legacy bridge, the voice babble and the
comedy one-shots of `audio_fx.c`) and `plat_sample()` /
`plat_sample_ex()` (one-shot effects whose 8-bit PCM comes from
`audio_sfx_pcm()`). On the Dreamcast (`dc/src/audio.c`, `mixer.c`,
`sfx.c`, `audio_int.h`):

```
main thread                                   stream thread (prio 9)
  doomo_frame: plat_tone / plat_sample*         every reschedule: snd_stream_poll
    -> the mixer's current state                  when half the AICA buffer played:
  plat_end_frame -> dc_audio_frame                  stream_cb -> dcsnd_render
    -> dcsnd_push: apply the state, render            copies 1,536 frames out of
       1,470 frames (1/30 s at 44,100 Hz)             the ring (gaps: silence)
       into the next slot of a PCM ring  ----->    KOS splits L/R, DMAs to the AICA
```

* **The mixer** (`mixer.c`, portable C99, integer only): four tone
  channels as musrender voices them (12.5 / 25 / 50 % pulses, triangle,
  saw, the 15-bit LFSR noise clocked at the channel frequency), at full
  resolution instead of the GBA's 4-bit wave steps, each at a fixed place
  in the stereo field (lead and bass centre, harmony / babble a little
  left, kit a little right; L + R is the mono reference); eight sample
  voices (16-bit copies of the effects made at boot by `sfx.c`, linear
  interpolation with a 15-bit weight, so `(b - a) * w` fits an int32 for
  any two 16-bit samples (a 16-bit weight overflowed: a one-sample click
  in 28 of the 35 effects, found in review, now `test_audio`'s smooth
  check), volume, pan, pitch in semitones, priority stealing: a
  free voice, else the lowest priority at or below the new one, the
  oldest first); then the native reference's DC block (about 14 Hz), a
  one-pole low-pass (about 12 kHz) against the naive pulses' aliasing, the
  musrender scale (`mix * 24`, so levels and `expect-audio` numbers match
  the native ones), and a soft knee above 24,576 that bends the rare loud
  peak (the invaded town's metal with effects on top reaches past the
  knee; nothing in the measured runs reaches the rail since the
  interpolation fix) towards the rail
  instead of clipping it (`clips` counts the samples that would have
  passed the rail).
* **Ticks, not time.** A slot is exactly one logic tick, rendered when the
  tick ends, so the sound is a pure function of the game's ticks: the
  same ticks give the same samples whatever the stream's chunk sizes, and
  a gap (the game late, a load, a harness hold) is silence that leaves the
  synth untouched. Tones and triggers take effect at the start of their
  tick's frames, as in musrender. The FNV-1a hash of the rendered stream
  is in the mailbox (`snd_hash`, and `snd_hash600` frozen after 600 slots
  = 20 s), so two runs can be compared without a WAV.
* **The ring.** 16 slots (94 KB). Playback starts once 2 slots are ready
  (`DCSND_PREFILL`) and starts again that way after it runs dry (an
  underrun, counted unless a pause was announced: the VMU writes, harness
  commands and holds call `dc_audio_stall()`; the core's
  `plat_music_stall` does the same, though the core does not call it
  yet). More than 4 slots ready at a slot start (the game catching up
  after a long tick) skips the oldest, as the tone chips of the GBA and
  the PS1 simply jump.
* **The stream.** One KOS `snd_stream` (44,100 Hz, 16-bit stereo, 6144 B
  per channel = 3,072 frames = 70 ms in sound RAM), refilled by KOS in
  halves of 1,536 frames from a thread that polls on every reschedule.

**Why the synth is not in the stream callback.** The first version rendered
in `stream_cb` (on the stream thread, priority 9, above the game's 10).
On Flycast the callback took 1.4 ms on average but 11-13.7 ms in about
3 % of the calls (21 of 875 in the title run), and the refills drifted
up to 39.9 ms apart against a 46 ms buffer. Timing the inside of the render
showed at most 1.2 ms; the rest was KallistiOS's scheduler: on its 10 ms
timer tick `thd_schedule()` picks the best thread *in the run queue*,
where the running thread is not, so a timer tick that lands while the
stream thread works hands the CPU to the game thread for a whole slice,
whatever the priorities (KOS `kernel/thread/thread.c`; disabling
interrupts around the render only moves the switch to `irq_restore`).
Rendering on the main thread at the end of the tick (the same CPU time,
now counted in the tick's work) leaves the stream thread a copy of a few
microseconds, which a tick almost never lands in.

**Why 6144 B.** The stream thread wakes on KallistiOS's reschedules, at
most 16.7 ms apart in practice (the game thread's vblank waits and the 10
ms timer), so a refill can come that much after its half became free. With
4096 B (46 ms) the longest measured refill interval was 34-40 ms: 6.5 ms
from the AICA playing stale data. With 6144 B (70 ms) it is 44-46 ms
against 70 ms. The price is latency (below).

**Latency.** A tick's sound waits in the ring until the slots before it
have played (at a slot start 1 to 3 slots were ready in every measured run:
title 215 / 327 / 55 times, invaded town 13 / 242 / 192, combat 9 / 187 /
101), then sits in the AICA buffer for 35-70 ms: about 70-135 ms from the
end of the tick to the speaker, about 100 ms typical, music and effects
alike, so they stay in step with each other as on the other ports. The
visual of a tick shows at the next vblank, so the sound follows the
picture by about 3 ticks. Two ways down were measured and not taken:
4096 B (-17 ms, but the 6.5 ms margin above), and the effects on AICA
channels of their own (`snd_sfx_load_raw_buf` + 8 reserved channels,
started at the end of their tick: under a tick of latency, 47,793 B of
sound RAM): the effects then lead the music by the stream's 100 ms, which
is heard (a rimshot before its sting), and the Flycast comparison with
the native runner fell to envelope r 0.72 on the invaded town. A real cut
needs a shorter refill period: KallistiOS at 1,000 Hz (`thd_set_hz`), or
our own AICA channel loop written from the main thread at a fixed point
of the tick.

**The music engine's tier.** The music engine runs as the legacy bridge
(tier `AT_BRIDGE`, the core's weak `plat_audio_caps`), as on the GBA, the
PS1 and native today (docs/AUDIO.md): its driver runs 8 mt a tick inside
`music_update()` and its four voices reach the mixer as `plat_tone()`. The
plan's IRQ tier (`CAP_DRV_IRQ`, `mus_drv_tick()` at 240 Hz, the voices as
`DoomoVoice` records through `plat_voice`, buses and effects) is not done,
on purpose: no port runs that tier yet (MU2 GBA and MU3 PS1 are open), the
core's driver still leaves the ring updates for the IRQ ports to mask
(`mus_drv.c`), S1's comic effects (mute, duck, tape stop) are applied only
in the bridge's flush, and there is no `DoomoVoice`-level reference render
(musrender works on `plat_tone`), so the Dreamcast would be the one port
whose music differs, with nothing to check it against. The mixer is ready
for it: it renders on the main thread, the thread that runs
`music_update()`, so calling `mus_drv_tick()` every 1,470 / 8 frames inside
`dcsnd_push()` gives the 240 Hz driver at sample-exact positions with no
locking at all. What it still needs: a `plat_audio_caps` with
`CAP_DRV_IRQ`, a voice renderer for `DoomoVoice` (pitch16, vol_l / vol_r,
env, the instrument's timbre, kit hits, the `DVF_*` flags), the S1 effects
in IRQ mode (core, MU1 / S1), the babble on voice 20, a reference render
to compare with, and `plat_audio_bus` / `plat_audio_fx` (reverb, echo).
The tape calls keep the weak defaults (0: unsupported; the bridge caps
have no `CAP_TAPE`). `plat_sample_ex` (pan, priority, pitch) is
implemented and tested on the host; the core does not call it yet, so
`--gc-sections` drops it.

### Cost

On Flycast's clock (not cycle-exact: a guide until a console is timed),
the mixer takes 1.33 ms per tick on the title (max 1.65), 1.41 ms in
combat (max 1.74) and 1.45 ms on the invaded town (max 2.39): 4-5 % of the
33.3 ms tick, counted in the tick's work. Every tick of the three runs
stayed inside the 30 fps budget (title work mean 3.4 ms, worst 12.3 ms;
the 3D view is still DC6's stub). Memory: the ring 94,080 B and the copy
buffer 12,288 B (`.bss`), the effects bank 95,656 B (heap, 35 effects in
16-bit), code 4.7 KB.

### Counters (`dc_mailbox.snd_*`, `dcrun`)

After every tick: slots rendered / begun playing, underruns, overruns (a
tick pushed into a full ring), slots skipped, the fullest ring, frames of
content / of silence, samples over the rail, the effects started, the
content hash (and after 600 slots), the mixer's time per tick (sum, count,
worst), the longest interval between two AICA refills and the refills
later than the whole AICA buffer (`snd_starved`: the AICA played stale
data, heard as a glitch). dcrun prints them at the end of a run (`dcrun:
sound ...`) and adds `audio-stats [LABEL]`, `expect-underruns MAX`
(underruns plus late refills since `audio-reset`) and `expect-sfx MIN`
(dc/harness/README.md).

### The host test (`dc/tests/audio`)

`make -C dc/tests/audio test` (part of `make dc-host-test` and CI; host
gcc and `python3-numpy`, no KallistiOS). `SAN=1 BUILD=...` runs it under
ASan/UBSan, and any UBSan report stops the run and fails it
(`-fno-sanitize-recover=undefined` for this directory's objects,
`UBSAN_OPTIONS=halt_on_error=1` for the core's):

* `test_audio` (2,046 checks): one tick sequence played out in whole
  ticks, in random chunks and in random chunks with empty-ring gaps (with
  and without a stall) gives the same content and the same hash; each wave
  at 110 / 440 / 1,760 Hz has the right zero-crossing rate; a full centred
  pulse peaks at 6,293 (musrender 5,760); every effect lasts len x 44,100 /
  rate frames, +12 semitones halves it, -12 doubles it, pan -64 / 64 is one
  side only; every effect at -24 / -12 / 0 / +12 / +24 semitones matches,
  frame by frame, the same voice and filters computed in double (worst
  27, tolerance 128; the overflowing interpolation was 19,000-23,000 off);
  the 9th voice steals the oldest of the lowest priority and
  never a higher one; prefill, underrun (not during a stall), overrun (a
  full ring renders nothing and keeps the triggers), the lag skip; silence
  is exact zeros; the loudest possible mix (4 tones + 8 explosions at full)
  has 9,342 samples over the rail and none at it (the knee).
* `run_compare.sh`: `dcsnd_runner` is the native runner whose `plat_tone`,
  `plat_sample` and `plat_end_frame` also drive the Dreamcast mixer
  (`sndtee.c`), so one script gives musrender's WAV (22,050 Hz, the
  reference) and the mixer's (44,100 Hz). `compare.py` lines them up and
  measures the RMS envelopes in 50 ms windows (Pearson r, the plan's bar
  0.9), the strongest note in 200 ms windows, and the level:

| script | envelope r | same note | level | notes |
|---|---|---|---|---|
| fp_title | 0.997 | 95.1 % of 82 windows | 0.98 | 590 slots, lag 0, no underrun |
| story_ch1 | 0.999 | 98.4 % of 182 | 0.98 | hash600 0x2b82d6ba |
| action_combat | 0.997 | 98.5 % of 65 | 0.98 | 13 effects |
| story_invasion | 0.973 | 90.0 % of 20 | 0.95 | no sample over the rail (the 90 before the interpolation fix were its clicks) |

### On Flycast (`make -C dc audio-test`)

`dc/tests/audio/run_flycast.sh` plays `title.txt`, `invasion.txt` and
`combat.txt` in dcrun on the CDI with `--wav`, the same lines on the native
runner (`runner --wav`) and compare.py on the pair (Flycast's WAV starts at
power-on, so the lag is searched), then `title.txt` again for the hash.
Results in `dc/build/shots/audio/` (WAVs, logs, `envelope.png`,
`summary.txt`). Measured 2026-11-08 (Flycast `e7085e20`):

| check | result |
|---|---|
| title (15 s of the title cue) | peak-to-peak 24,050; envelope r 0.996, the same note in 85.9 % of 85 windows, level 0.96; 0 underruns, 0 late refills, longest refill interval 45.4 ms (of 70) |
| invasion (chapter 3's invaded town, 15 s) | peak-to-peak 53,438; r 0.945, 93.2 % of 74, level 0.95; 0 underruns, 0 late refills; no sample over the rail (60,874 and 90 over the rail before the interpolation fix: those were its clicks) |
| combat (the Vape Cellar, three pistol shots, 12 s) | peak-to-peak 34,264; 6 effects (at least 3); r 0.922 (envelopes clipped at their 97th percentile: the enemies' noises come at other times than natively while the 3D is a stub), 94.0 % of 67, level 0.97; 0 underruns, 0 late refills |
| repeat | the title's hash after 600 slots, 0xdfa596b6, twice (0x520f170a before the interpolation fix changed the effects' samples) |

`make -C dc test` with DC5 in: boot on the ELF and the CDI, DC3's
ui-compare 94 of 94 shots, DC4's five VMU checks, then the four above.
Outside the measured windows the counters do show underruns: one or two
in runs that boot and change scenes (the slot before a long first tick),
and skipped slots after harness holds (dcrun's `sync` stops the game while
the stream keeps playing). Both are gaps of a few milliseconds at moments
the core makes no sound of its own.

### Owed on hardware (DC5)

- The mixer's cost and the refill timing on a real SH4 and AICA (Flycast
  is not cycle-exact; `dcrun: sound ...` has the numbers to compare).
- The latency, by ear and with a camera and a microphone (the estimate
  above is from the ring and the buffer, not measured end to end).
- The AICA's output with the knee near the rail, through VGA and composite.
- Clock drift. Ticks are paced by the SH4's timer (33,333 us, 30.0003 Hz)
  and the AICA takes 1,470 frames a tick at its own 44,100 Hz crystal; on a
  console the two differ by tens of ppm, so over minutes the ring slowly
  fills (until the lag skip drops a slot: 33 ms of sound, the latency up
  to 67 ms higher before it) or drains (an underrun and a 67 ms re-prime).
  Flycast runs both from one clock and cannot show it. If a console shows
  either every few minutes, a small servo (drop or repeat a few frames per
  slot, keeping the ring at 2) is the fix.
- `snd_starved` is a lower bound: it counts refill intervals longer than
  the whole AICA buffer, but KOS refills a half only after it is free, so
  a refill more than 35 ms after its half was freed already plays stale
  data, which two late refills in a row can do with intervals under 70 ms.
  The Flycast runs are far from that (worst interval 45 ms, polls at most
  about 17 ms apart); on a console, reading the AICA's play position
  against the write position at each refill would count it exactly.

## 7. The overworld in 3D (DC7)

```sh
make dc-host-test        # includes dc3d_test: the 3D batches and the overworld on the host
make -C dc ow-test       # the overworld scripts on Flycast against the host's software PVR
make -C dc test          # ... and everything else (ow-test is its last step)
```

`plat_overworld` draws the cutesy town as real terrain with a tilted
third-person camera, the PS1's design (`ps1/src/overworld.c`) on the PVR
with a depth buffer, in the native runner's colours. Files (`dc/src/`):

| file | what | portable |
|---|---|---|
| `dc3d.h`, `dc3d.c` | 3D batches in the 2D list: segments, markers, camera, near-plane clipping, culling, the TA budget, `plat_hold_frame` copies | yes |
| `overworld.c` | `plat_overworld`: terrain, blocks, sea, billboards, shadows, light, tints | yes |
| `pvr3d.c` | the PVR backend of the batches: headers, strips, the shadows' blob texture | KOS only |

DC6 (`plat_view3d`) and DC8 (`plat_room`) can draw through the same
batches.

### A 3D view inside the painter's order

The 2D layer sends a frame as ONE translucent list in presort mode
(section 4). A view is built into a SEGMENT of the frame's batch: polygons
already projected to the 640x480 frame, z = 64 / camera depth (bigger is
nearer). The 2D list gets a marker prim (`DC2D_VIEW3D`) where the core
called the view, and `pvr2d.c` sends the segment there, in the same list:

- whatever the core drew before the view is under it, whatever it draws
  after (HUD, dialogue, fades, the pause menu) is on top, exactly as on
  the native runner; `story_draw` needs no change;
- inside the segment the depth is real: every polygon compares GEQUAL
  (in presort mode the PVR honours a translucent polygon's depth compare
  and depth write); the opaque terrain writes depth (in any order, so it is
  grouped by texture: one header per texture), the billboards and shadows
  that follow test against it without writing, back to front;
- the 2D prims carry z = `DC2D_Z` = 0.001, just above the background plane
  (0.0001) and under every 3D polygon (z >= 0.0064 out to 10,000 units).
  They compare ALWAYS and their depth write is off, but Flycast writes the
  2D sprites' depth anyway: at z 1.0 a backdrop drawn before the view hid
  it completely (the first Flycast run was black where the host was right).

The marker keeps the segment's viewport; the 2D post effects move or scale
its rect like any prim (screen shake), and the backend maps every vertex
from the built viewport to the rect. A full-screen exact shade (fades)
copies the marker with the shade's fitted per-channel factor (`pal.c`), a
palette lerp switches the view's textures to the lerp bank, and
`plat_hold_frame` copies the shown frame's segments into a third batch
that lives while the hold does (the pause menu's backdrop). A shade over
part of a view is the MUL fallback (the frame times the factor): the exact
path would paint the remapped background over the view. The pause menu
dims the world around its window that way, so on the Dreamcast that dim is
the fitted multiply (a little greyer than the native colormap's
pinker darkening).

A viewport smaller than the screen is not scissored: polygons crossing its
edge draw past it (the story overworld and the credits always use the
whole screen; the PVR's user clip is 32-pixel tiles, so a pixel-exact
viewport would need the polygons clipped to it on the SH4).

Budgets: three batches of 12,288 vertices and 4,096 polygons (1.13 MB of
`.bss`), at most 8 segments a frame, and 576 KB of TA data per segment
(of the 960 KB the frame may send), so the HUD after a view always fits;
past either a view leaves its last polygons out and counts them.

### The town

- **Terrain**: open cells are floor quads at their floor height (doors and
  lifts animated: `map_floor_z`); where a neighbour is lower, a terrace
  face drops to it on the south, east and west sides (the camera looks
  north). Blocks (houses, hedges, fences, trees, the Wall) are boxes with
  a roof: `ceil_h` when above `floor_h`, else a default by wall texture;
  walls are one quad each with the texture repeating up their height,
  roofs use `floor_tex` (else roof tiles on building fronts). Beyond the
  map a sea of water tiles surrounds the island. The world textures have
  no transparent texels (checked: palette index 0 never occurs), so the
  terrain is drawn opaque.
- **What is drawn**: the rows the ground can show in (where the screen's
  top and bottom rays meet the lowest and the highest floor, about 9 rows
  north and 6 south of the focus), each row only as wide as the frustum
  there; north and south of those only the blocks tall enough to rise into
  view (the map's tallest block decides how far: the Wall is 320-480 units
  and shows from 12 rows beyond). About 280-370 polygons a view.
- **Camera**: the PS1's numbers: 58 degrees down, 1,100 units back from
  the focus, tan(hfov/2) = 0.45; the focus `(cx, cy)` (a little above its
  floor) is the screen's centre. No smoothing of its own: the core's
  camera already glides, and the player stays where the core's overlays
  (the iris, spawn markers) expect it.
- **Billboards** (residents, the pet, the player, props): rectangles
  parallel to the screen (they never lean at the screen's sides, which
  upright quads in perspective did), the native runner's size: 3 world
  units per texel at scale 256, about one screen texel per logical pixel
  at the focus (the PS1 draws them at 2). Their depth is that of an
  upright plane through the feet: each row of the rectangle gets the z
  where its pixels' rays meet that plane, so a house in front hides a
  resident's legs pixel by pixel and the wall just behind never cuts a
  head. The rectangle is 4 strips (one strip of 10 vertices), so the
  texture's perspective step between rows stays far below a texel. Heights
  above the floor (jumps, emote bubbles, the quest marks the core sets 108
  units up) lift the rectangle in screen space as the native runner lifts
  them (z / 4 pixels): x and the base row are projected at the ground
  point, so a lifted sprite stays over its feet at the screen's sides.
  (Projecting the lifted point itself, which is nearer the camera, pushed a
  quest mark about 30 of 640 pixels outward there, beside its resident;
  the PS1's `r3_billboard` still does.) Drawn back to front (north
  first).
- **Shadows**: a soft blob under the feet (on the floor, not under a
  jump): one quad with a 32x32 ARGB4444 texture of its own (the palettes
  are ARGB1555: no soft alpha), black at 44 % in the middle.
- **Light**: a grid corner's colour is the average over the open cells
  around it of each cell's light colour (lava, portals, water and exits
  tint it) and brightness as the native runner sees it (light under 8
  darkens a cell, about 6 % a step); a per-map cache, refreshed a row every
  other tick. Daylight is the native runner's: daylight d picks colormap
  level 5 + d * 10 / 15, here that level's fitted per-channel factor, and
  before d = 8 the native runner's lean to blue (the night tintmap over
  (8 - d) / 8 of the pixels) is that tint's fit at that strength. At night
  a corner touching a lamp cell (light >= 13) adds a warm offset colour,
  so a lamp lights a soft pool over its cell that fades across its
  neighbours. The far rows fade toward the sky's horizon colour (45 % at
  the top edge). The view tint (`TINT_*`, the invaded town's red pulse,
  Hellvile's purple) is the native tintmap fitted per channel (c * a + b:
  rms 11-18 per channel for the colour tints, the offset colour can only
  add, and every fitted b is positive) at level / 16, applied to every
  vertex. A per-frame corner cache holds each corner's light, fade and
  tint once.
- **Backdrop**: a 2D rect of the sky's horizon colour under the view (it
  only shows past the sea).

### Counters

After each drawn frame the mailbox holds `g3d_us` (the last view's build
time, microseconds), `g3d_us_max`, `g3d_us_sum` / `g3d_n` (the views
built), `g3d_polys` / `g3d_polys_max` (polygons of a view) and
`g3d_bytes_max` (TA bytes), `g3d_dropped` (polygons left out); `dcrun`
prints them at the end of a run (`dcrun: 3D views ...`).

### The host test

`dc/tests/gfx2d/swpvr.c` rasterises the batches as `pvr3d.c` asks the PVR
to: triangle strips in order, the depth buffer cleared to the background
plane, GEQUAL, depth writes by the opaque mode, perspective-correct UVs and
colours, texel x colour + offset, each mode's blend. `test_dc3d.c` (60
checks, also under ASan/UBSan; part of `make dc-host-test`):

| test | checks |
|---|---|
| camera | the focus projects to the screen's centre; north is farther and higher; east is right; up is nearer |
| clip | a quad from behind the eye to in front is cut at the near plane, every vertex in front with a finite z; one wholly behind adds nothing |
| cover | every pixel has terrain (the depth buffer is written everywhere) at four spots on each of HaHaland, the invaded town, Hellvile and Park United, by day and night (this found the Wall missing at the screen's corner before the rows past the ground's edges were added) |
| order | a rect drawn before the view is under it, one after is on top |
| depth | a billboard behind a 96-unit block is hidden at its feet, one in front of it shows |
| lift | a billboard 108 units up (a quest mark) six cells left and right of the focus covers the same columns as on the ground (within a pixel) and sits 44-60 pixels higher |
| shade | a full-screen shade multiplies the view by the fitted factor (within 12/255); a partial one over it is the multiply |
| hold | the held view is the same picture after two more frames; a full shade over it copies it |
| post | `plat_post_offset` moves the view's pixels with the other prims |
| lerp, tint | the marker is on the lerp bank; the red tint pushes red minus green up by more than 20 |
| repeat | the same view twice gives the same vertices, byte for byte |
| budget | each map with 400 billboards with shadows: nothing dropped (the worst view: 1,088 polygons, 242 KB of the 576 KB) |

`DC7_OW=dc` makes the host runner `dc/build/host/dc2d_runner` draw the
Dreamcast overworld through the software PVR (`NAME_dc.png` beside each
shot) instead of DC3's stand-in, for any runner script.

### On Flycast (`make -C dc ow-test`)

`dc/tests/overworld/run_flycast.sh` plays story_ch1, story_ch2,
story_invasion, fp_island and side_sample_a (`core/tests/scripts`) and two
walks around Hellvile, the busiest hub (`dc/tests/overworld/hellvile.txt`
by day, `hellvile_night.txt` at 21:30), in dcrun on the CDI (tick-exact
from the native runner's start), in the native runner (the top-down
reference, for the contact sheets) and in the host runner with
`DC7_OW=dc`. `compare3d.py` checks every Flycast shot with a 3D view
against the host's picture of the same tick, in RGB565: at least 97 % of
the pixels within 24/255 (both sides draw the same polygons; the PVR's and
the rasteriser's edge rules and Gouraud rounding differ by a level or a
pixel along edges). A Flycast shot without a host shot fails, and every
script but fp_island (the island hub is 2D) must have at least one shot
with a view, so a script whose view went missing cannot pass with nothing
compared. No polygon and no 2D prim may be dropped (a view that does not
fit the held batch counts too), and no tick's work may pass 33.3 ms, the
30 Hz budget (fp_island may have its one known loading tick). `sheet.py`
draws contact sheets (the Dreamcast beside the native top-down reference).
Results in `dc/build/shots/ow/` with `summary.txt`.

side_sample_a needed the native runner's story commands in dcrun (`side`,
`talk`, `cell`, `skip-talk`, `skip-to`, `skip-to-scene`, `expect-text`,
`no-stubs`; section 3, the mailbox), and the night walk `clock`. The
pinned Flycast core sometimes crashes while it unloads, after the script
has passed (SIGILL from its DEBUGBREAK, exit 132; the merge base's CDI
does the same on story_ch1 and the Hellvile walk): the run script takes the
script's own "0 failures" line as the verdict then and says so.

Measured 2026-11-08 (Flycast `e7085e20`, the CDI; times in emulated SH4
time, a guide until a console measures them):

| script | 3D shots (Flycast vs host: worst close / exact) | views built | build mean / max | work per tick mean / worst | ticks over 33.3 ms |
|---|---|---|---|---|---|
| story_ch1 | 8 pass (98.3 % close, 93 % exact) | 667 | 3.7 / 16.3 ms | 7.3 / 28.1 ms | 0 of 1,160 |
| story_ch2 | 3 pass (99.6 %) | 246 | 4.2 / 16.3 ms | 5.3 / 26.7 ms | 0 of 1,291 |
| story_invasion | 2 pass (99.9 %) | 35 | 4.6 / 17.5 ms | 6.8 / 29.0 ms | 0 of 121 |
| fp_island | none (the island hub is 2D) | 0 | - | 18.2 / 96.6 ms | 1 of 522 (making the island, DC3's known loading tick) |
| side_sample_a | 17 pass (99.5 %) | 3,492 | 4.2 / 16.2 ms | 7.1 / 29.2 ms | 0 of 6,100 |
| hellvile (day) | 6 pass (99.98 %) | 250 | 3.6 / 13.7 ms | 6.7 / 18.9 ms | 0 of 250 |
| hellvile_night | 3 pass (99.98 %) | 135 | 3.8 / 13.7 ms | 7.0 / 18.9 ms | 0 of 135 |

39 of 39 shots with a 3D view pass; the 2D-only shots of the same runs
match the host exactly (100 % in RGB565). The busiest hub, Hellvile,
holds 30 fps with room: 6.7 ms of work a tick on average, 18.9 ms at worst,
of which the view takes 3.6 ms (about 290 polygons, 43 KB of TA data).
A view's worst build (13-17 ms) is the first frame on a map (the corner
cache and the world textures' upload). That first tick was 45-56 ms
until DC7 made texture twiddling table-driven (`texcache.c`: about 2 ms a
64x64 texture before); now no overworld tick of these scripts passes
33.3 ms. Against the merge base (no 3D: the overworld drew nothing) the
Hellvile walk's work went from 2.4 to 6.7 ms a tick on average.

Contact sheets of the runs are in the package's evidence (story_ch1,
Hellvile by day and at night): the town in perspective with the native
runner's sprites and colours; the native reference shows more of the map
(it is top-down at 16 pixels a cell), the Dreamcast about 9 rows north and
6 south of the player at about 20.

### Owed on hardware (DC7)

- The view's build time and the frame on a real SH4 and PVR (Flycast's
  timing is not exact; `dcrun: 3D views ...` has the numbers to compare).
- That the PVR honours depth compare and depth write on translucent
  polygons in presort mode as Flycast does (the design rests on it; if a
  console ignores the write, the fallback is to sort the terrain back to
  front, which the row order nearly is already), and whether a console
  writes the 2D sprites' depth as Flycast does (`DC2D_Z` makes it harmless
  either way).
- The ARGB4444 non-twiddled blob texture with bilinear filtering, and the
  offset colour (`specular`) on translucent polygons.
- Fill rate: the terrain is drawn through the translucent list (about 1.5
  times the screen with roofs and faces); the PVR's translucent path is
  slower than its opaque one, unmeasured on a console.

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
