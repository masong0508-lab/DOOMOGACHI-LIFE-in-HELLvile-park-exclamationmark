# DOOMOGACHI LIFE on the Dreamcast

The Dreamcast port is being built in stages (see the DC packages in the
project plan). This document grows with it. Right now it covers:

1. The toolchain: the sh-elf cross compiler and KallistiOS, installed by
   `dc/tools/setup_kos.sh` at pinned versions.
2. The headless harness: `dc/harness/dcrun`, a libretro frontend for the
   Flycast core (details in `dc/harness/README.md`).
3. The `dc/` build (DC2): the skeleton that links the whole core, boots,
   runs the 30 Hz loop and changes scenes, the harness mailbox, the RAM
   budget and the self-booting CDI.

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
(the vblank counter, also in the mailbox) and calls
`vid_set_mode(DM_640x480, PM_RGB565)`, which gives 480p on a VGA cable and
interlaced 640x480 on RGB/composite, then `pvr_init_defaults()`. The core
runs on a 320x240 logical screen (`plat_screen_w/h`), which DC3 draws at
2x.

The loop is the platform.h protocol. Logic runs at 30 Hz on the SH4's
microsecond timer (`timer_us_gettime64`), not on vblanks, so the game runs
at the same speed on 60 Hz and 50 Hz (PAL) displays. A drawn tick is one
PVR scene: `plat_begin_frame()` waits for the PVR (`pvr_wait_ready`) and
opens a scene, `plat_end_frame()` finishes it (KOS submits blank lists for
lists nothing was drawn into) and the PVR shows it at the next vblank;
then the loop sleeps on vblanks until the next tick is due. When the game
falls more than a tick behind, the next tick skips its drawing
(`plat_draw_skipped()` = 1), never two in a row; more than 6 ticks behind
(loading) forgets the debt. Each drawn tick's work time (begin to end, in
microseconds) goes to the mailbox's `work_sum/work_n/work_max`.

The controller (DC4 adds the analog stick, the keyboard and pads 2-4):
D-pad, A, B, X, Y as themselves, L/R triggers past half as L/R, START,
and START+Y as SELECT (DECISIONS.md; the pad has no SELECT).

What the skeleton draws: only `plat_clear()`'s colour, as the PVR's
background colour. The other placeholders are in `dc/src/plat_stub.c`:

| group | today | replaced by |
|---|---|---|
| 2D (`plat_rect`, `plat_text`, `plat_image`, ...) | no-ops; `plat_text` feeds the harness text probe | DC3 |
| post effects, panels, `plat_figure_obj` | the core's weak software defaults | DC3 |
| saves | a RAM disk (one buffer per slot, lost at power-off) and a static 136 KB scratch | DC4 (VMU) |
| `plat_tone`, `plat_sample` | silent; the core's weak music bridge drives `plat_tone` | DC5 |
| `plat_view3d`, `plat_overworld`, `plat_room` | no-ops / weak defaults | DC6, DC7, DC8 |

`make -C dc check-elf` lists the core's weak platform defaults still in the
link, so each package can see what it has left.

### The harness mailbox (`dc_mailbox`, `dc/src/dc_plat.h`)

The Flycast harness (DC1) finds the `dc_mailbox` symbol in the ELF
(`sh-elf-nm`, shown as `_dc_mailbox`) and reads and writes it through
libretro's `RETRO_MEMORY_SYSTEM_RAM` region at `address & 0xFFFFFF`.
`check-elf` prints that offset. It is 32-byte aligned and 4212 bytes:

| field | what |
|---|---|
| `magic` ... `draw_all` | the GBA's `GbaMailbox`, field for field and at the same offsets: magic `"DMDC"` (0x43444D44), logic ticks, drawn frames, fps x 10, cmd/arg/result, the command's time (microseconds), work time sum/count/worst, `name[20]`, the "sync on" `hold_at`, `draw_all` |
| `version` | 1 (`DC_MAILBOX_VERSION`; `check-elf` checks the size) |
| `vblanks` | vblank interrupts since boot |
| `video` | `vid_check_cable()`: 0 VGA, 2 RGB, 3 composite |
| `boot_us` | microseconds from `main()` to the first loop tick |
| `scene[24]` | the scene on top of the stack (from the core's `scene: set/push/pop` log lines) |
| `log_head`, `log[4096]` | the log ring: every `plat_log()` line plus `\n`, at `log_head % 4096`; `log_head` counts every byte ever written |

Log lines also go to `dbgio` (the serial port on hardware, the emulator's
console). Harness commands (`dc/src/hcmd.c`) keep the GBA's ids and
meaning: 1 save, 2 load, 3 exists, 4 town-fill, 5 town-crc, 6 verify,
7 scene, 8 push, 9 seed (pins `plat_entropy`), 10 story flag, 11/12 the
text probe (expect-text), 13 story fingerprint. The harness writes `arg`
(and `name`), then `cmd`; the game runs it between two logic ticks.
`dc/tests/boot.txt` is the first harness script: boot, `scene: set BOOT`,
`scene: set TITLE`, the text probe on the title.

### RAM

The whole program loads into the 16 MB of main RAM at 0x8c010000; what is
above `_end` is the heap and the thread stacks. `make -C dc` prints the
budget (`dc/tools/elfsize.sh`) and fails if less than 4 MB
(`DC_RAM_MIN_FREE`) would be left. Measured 2026-11-01 (DC2):

| part | bytes | MB |
|---|---|---|
| code (`.text .init .fini`) | 1,580,784 | 1.51 |
| constant data (`.rodata` ...) | 2,815,836 | 2.69 |
| `.data` | 6,272 | 0.01 |
| `.bss` | 537,892 | 0.51 |
| **image** | **4,940,784** | **4.71** |
| free above `_end` (heap, stacks) | 11,770,396 | 11.23 |

`.bss` includes the RAM-disk saves (128 KB) and the save scratch (136 KB)
that DC4 replaces. The ELF file is 16 MB because of debug info; the
scrambled `1ST_READ.BIN` is 4.4 MB.

### Disc image (`dc/tools/mkcdi.sh`)

```
doomogachi.elf --objcopy -O binary--> doomogachi.bin --scramble--> 1ST_READ.BIN
dc/ip.txt --makeip--> IP.BIN
1ST_READ.BIN + IP.BIN --genisoimage -C 0,11702 -G IP.BIN--> doomogachi.iso
doomogachi.iso --cdi4dc--> doomogachi.cdi   (audio/data, self-booting)
```

`dc/ip.txt` holds our own title ("DOOMOGACHI LIFE IN HELLVILE PARK"), maker
("DOOMOGACHI TEAM"), product number T-DOOMO and version. makeip adds the
hardware and maker identifiers the boot ROM requires in every bootstrap
(`SEGA SEGAKATANA`, `SEGA ENTERPRISES`); they are not shown anywhere, and
the disc has no logo or jingle of anyone's. The image is 6.9 MB. A GDI
or CHD release (DECISIONS.md) can be made from the same 1ST_READ.BIN and
IP.BIN, which the build copies next to the CDI.

### Verification so far

Without the Flycast harness (DC1) in the tree, DC2 was checked with the
KallistiOS build only: every core file compiles with gcc 15 and `-Werror`,
the ELF links with no undefined symbols, `check-elf` passes (SH
little-endian, entry 0x8c010000, the mailbox present, aligned and of the
right size) and the CDI builds. Booting the ELF and the CDI in Flycast
(`dc/tests/boot.txt`: BOOT then TITLE, 30 ticks per emulated second) is
the first thing to run once `dc/harness` lands.

