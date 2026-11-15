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
   2D layer's painter's order (the 3D batches of `dc3d.h`, which DC6 can
   use too), checked on Flycast against a software PVR on the host.
8. The dream house (DC8): `plat_room` on the PVR, drawing exactly what the
   native reference `room_ref.c` draws (checked frame by frame on the host
   and on Flycast), with the room's shell as one cached texture, real
   translucent ghosts, a pulsing selection and the zoom; the house at about
   3 ms a tick instead of 28-50 ms.

9. The disc image, the boot FMV and the VMU saves end to end (DC9C): the CDI
   with `INTRO.DMV`, the movie that plays before the game, and `make -C dc
   disc-test`.

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
| `plat_tone`, `plat_sample`, `plat_sample_ex`, `plat_music_stall` | **done** (`dc/src/audio.c`, `mixer.c`, `sfx.c`, section 6) | DC5 |
| `plat_audio_caps`, `plat_voice`, `plat_audio_bus`, `plat_audio_fx` | **done**: the music engine's IRQ tier (`dc/src/musvoice.c`, section 6; `MUSIC_IRQ=0` keeps the weak bridge) | DC5b |
| `plat_overworld` | **done** (`dc/src/overworld.c` over `dc3d.c` / `pvr3d.c`, section 7) | DC7 |
| `plat_room`, `plat_room_project` | **done** (`dc/src/room.c`, section 8) | DC8 |
| `plat_view3d` | **part 1 written** (`dc/src/view3d.c`: walls, floors, ceilings, sky; section 9); billboards, weapon, effects, HUD are part 2; draw distance and the governor are part 3. Not yet built with KallistiOS or run on Flycast | DC6 |

`make -C dc check-elf` lists the core's weak platform defaults still in the
link, so each package can see what it has left.

### Speed tour 2 (SA2, `dc/harness/speed2.sh`)

`sh dc/harness/speed2.sh OUTDIR [groups...]` (after `make -C dc`, the stripped ELF and `doomogachi.nm`) plays the
GBA's speed tour 2 scripts (docs/GBA.md "Speed tour 2") in Flycast: `tools/speed2.py dc` makes each window
`log speed2: begin N NAME`, `mark-ticks`, `frames 2N`, `expect-ticks`, `log speed2: end NAME`, and
`tools/speed2.py table` prints the logic ticks/s and the ticks over 33.3 / 50 ms of work (dcrun's `work:` lines)
per window, with the run's `game drew` and `work over` lines. Flycast is not cycle-accurate: a guide, not a promise.

(S2-11, 2026-11-13, a measurement note: the house group after the GBA's house work, on the tip `ee4e053` + S2-11: all 19
windows 30.0, 0 of 2,461 ticks over 33.3 ms, work mean 3.4 ms and worst 20.6 ms at the house's entry, the same as before. The Dreamcast takes
the new core hooks with their weak defaults: `plat_build_frame(PLAT_BUILD_BUSY)` returns 0 and `plat_ui_loops()` does
nothing, so `plat_room` (DC8) is untouched and a house frame is one weak call heavier.)
**S2-9 (the workshop).** Nothing changes on the Dreamcast: the weapon picture is composed in the asking call as before
(`weld_view_image_sliced` is `weld_view_image` here: no slices, nothing composed ahead), the workshop keeps no panels
(`WS_KEPT` is 0 where `DOOMO_PLATFORM_DC` is set: the screen is drawn as it always was, the stat bars with their shine and
the backdrop with its dots). `speed2.sh action` after the package: every window 30.0, no tick over 33.3 ms, the worst
tick of the group 22.7 ms (the workshop's entry; 23.9 ms in speed audit 3). Again on the tip it landed on (`d530565` +
S2-9): every window 30.0, 0 of 3,254 ticks over 33.3 ms, work mean 13.0 ms, the group's worst tick 27.5 ms (the arena
modes, not the workshop).

**S3-1 (the sim's 15-minute pass).** The core change (docs/GBA.md "Speed wave 3: S3-1": one resident a tick over the 15 game
minutes instead of 32 residents in 32 ticks every 15 minutes) is shared. The SH4 never had a long frame from the pass: the new
`sim` group (`speed2_sim.txt`: the island hub, the shop's gifts tab, the citizen picker and the pet, 1,200 ticks each, a pass
inside every window) reads 30.1 ticks/s with no tick over 33.3 ms of work in all four windows both before (origin
`f91bd3e4`) and after (Flycast, `sh dc/harness/speed2.sh OUT sim`, the stripped ELF and `doomogachi.nm` rebuilt first); the
work mean over the run went 10,054 -> 10,077 us a tick (the steady cost of the update, ~20 us) and the worst tick of the group
is the same 125 ms dialog entry in both (tick 53). Again on the trees it landed on (`cb7d48d9` and `3d9905df`, each + S3-1): every window
30.1, 1 of 5,783 ticks over 33.3 ms (the same dialog entry, tick 53), work mean 10,079 us; `.bss` 4,581,012 B on `3d9905df`. The halfword edge move is
GBA-only. `make dc-host-test` passes.

**S2-4b (the Fun Fair, 2026-11-13).** Nothing changes here: stamps, bands and undo lists are GBA-only (`plat_stage_stamp` and
`plat_stage_restore_list` are the weak no-ops, `mg_stamp` returns 0 and the games draw as before). Flycast `speed2_fair` after the
package: all 23 windows 30.0 logic ticks/s, 0 of 6,040 ticks over 33.3 ms of work (worst 15.7 ms, the Derby), as before.
Landing (2026-11-15, on `cb9df1c`, with DC9B's kept church, DC6D and DC9C; the same on `b9e74a5` and `0cce022`): the results card's hold is GBA-only (`ui_modal_hold` holds only where
`ui_modal_hold_enable` is on, the GBA's default), and DC9B's `stage_begin` keeps only the exorcism's church, so the fair games
still draw in full here. `speed2_fair` now has 30 windows (a results card for every game): all 30.0 logic ticks/s, 0 of 14,140
ticks over 33.3 ms of work, worst 17.1 ms (the Gallery); `make dc-host-test` passes.

### The harness mailbox (`dc_mailbox`, `dc/harness/dc_mailbox.h`)

The layout is one header shared by the game and the Flycast harness (DC1):
`dc/harness/dc_mailbox.h`, which `_Static_assert`s its size on both
compilers. The harness finds the `dc_mailbox` symbol in the ELF
(`sh-elf-nm`; the SH ABI prints C names with a leading `_`, so
`dc/src/misc.c` also exports the plain name as an alias) and reads and
writes it through libretro's `RETRO_MEMORY_SYSTEM_RAM` region at
`address & 0xFFFFFF`. `check-elf` checks the size and the alias and prints
the offset. It is 32-byte aligned and 4364 bytes:

| field | what |
|---|---|
| `magic` ... `draw_all` | the GBA's `GbaMailbox`, field for field: magic `"DCMB"` (0x424D4344), logic ticks, drawn frames, fps x 10, cmd/arg/result, the command's time (microseconds), work time sum/count/worst, `name[20]`, the "sync on" `hold_at`, `draw_all` |
| `scene[24]` | the scene on top of the stack (from the core's `scene: set/push/pop` log lines) |
| `log_wr`, `log[4096]` | the log ring: every `plat_log()` line plus `\n`, at `log_wr % 4096`; `log_wr` counts every byte ever written |
| `gfx_prims` ... `gfx_tex_live` | DC3's 2D counters after each drawn frame (section 4, Counters) |
| `test_flags` | DC4: `plat_test_flags()`; dcrun writes `PLAT_TEST_NO_PICKER` and (CL4) `PLAT_TEST_NO_TIPS` when it sees the magic (`--console`: 0; a script's `tips on` clears the tips bit) |
| `snd_pushed` ... `snd_starved` | DC5's sound counters after each tick (section 6, Counters) |
| `g3d_us` ... `g3d_dropped` | DC7's 3D view counters after each drawn frame (section 7, Counters) |
| `room_us` ... `room_prims_max` | DC8's house counters after each drawn frame (section 8, Counters) |

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
mailbox's `name`), 23 "is the top scene" (`skip-to-scene`), 24 `clock`; DC8
the house: 14 `room-parity` (a golden frame of `core/tests/room_shells.h`
drawn by `plat_room` on a screen of its own, the GBA's id), 25 `home-room`
and 26 `money` (the native runner's house commands). Results follow the GBA: nonzero means success (dc_mailbox.h). The harness writes `arg`
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
With DC8 (2026-11-08, `make -C dc sizes` on the tree with CL6, SP1 and SP2):
code 1,820,848 B, rodata 2,997,628 B, `.bss` 4,555,572 B (DC8's +277 KB: the
house's shell canvas 179 KB, its sprite decode buffer 64 KB, the piece
table 12 KB, the harness's `room-parity` scratch 12 KB, the order's edge
matrix 5 KB, the tint maps 2 KB), image
9,380,584 B (8.95 MB), 7,324,876 B (6.99 MB) free above `_end`.
The ELF file is 17 MB because of debug info; the scrambled `1ST_READ.BIN` is 4.4 MB.

### Disc image (`dc/tools/mkcdi.sh`)

```
doomogachi.elf --objcopy -O binary--> doomogachi.bin --scramble--> 1ST_READ.BIN
dc/ip.txt --makeip--> IP.BIN
1ST_READ.BIN + INTRO.DMV (+ PACK.DMP) + IP.BIN --genisoimage -C 0,11702 -G IP.BIN--> doomogachi.iso
doomogachi.iso --cdi4dc--> doomogachi.cdi   (audio/data, self-booting)
```

The disc root also holds Talk's database, `TALK_EN.BIN` (TD1, section "Talk on the Dreamcast (TD1)"): `make -C dc` builds
`core/build/talk/TALK_en.BIN` with `core/tools/talkc.py` and `mkcdi.sh ... PACK TALKDIR` copies every `TALK_<code>.BIN`
of that directory to the disc root in capitals (the test bank `TALK_gentest.BIN` stays off the disc).

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
The disc root holds `1ST_READ.BIN` (the game), `INTRO.DMV` (the boot movie,
DC9C, "Boot FMV" below) and, when built with `PACK=`, `PACK.DMP`. The image is
8.7 MB (7.6 MB before the movie). A GDI
or CHD release (DECISIONS.md) can be made from the same 1ST_READ.BIN and
IP.BIN, which the build copies next to the CDI (the movie goes on the disc
as a file: copy `dc/build/disc/cd/INTRO.DMV` into the GDI's data track).

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


### The disc, the boot FMV and the VMU saves end to end (DC9C)

**Build and run, exact steps** (from the repo root, KallistiOS installed by
`dc/tools/setup_kos.sh`; docs/BUILDING.md has the host packages):

```sh
make -C dc -j2 all-disc          # dc/build/doomogachi.elf + doomogachi.cdi (+ INTRO.DMV, 1ST_READ.BIN, IP.BIN)
make -C dc/harness               # dcrun, once (the Flycast core: make -C dc/harness flycast, or the toolchain prefix's)
make -C dc disc-test             # the CDI on Flycast: boot, movie, skip, a saved game over a restart, an old VMU image
make -C dc vmu-test              # DC4's VMU checks (vmu_check.py on the image after each run)
dc/harness/build/dcrun /opt/toolchains/dc/flycast_libretro.so dc/build/doomogachi.cdi \
    --nm dc/build/doomogachi.nm --fmv --console --fresh --run-dir /tmp/dc --shots /tmp/dc/shots   # watch it boot
```

`make -C dc disc` needs `ffmpeg` and python3 `numpy` for the movie; without
them the disc is built without `INTRO.DMV` and the game boots as before (a
warning; `INTRO_REQUIRED=1` makes it an error; `INTRO=file.dmv` uses a
ready-made file, `INTRO=none` builds none). `make -C dc/harness` must be run
again when `dcrun.c` changes, and `dc/build/doomogachi.nm` must be the one of
the ELF just built (`make -C dc vmu-test` and the other targets rebuild it;
a stale one makes dcrun report "the game never wrote the mailbox magic").

**The boot FMV** (`dc/src/fmv.c`, `dmv.c`, `dmv.h`; `dc/tools/mkfmv.py`).
The Dreamcast has no video decoder in hardware and KallistiOS has none in
the tree, so the Dippinn Productions ident (`trailer/assets/dippinn_logo.mp4`,
the movie the PS1 plays too) is encoded into a small codec of our own:
320x176, 15 fps, 152 frames, YCbCr 4:2:0, 8x8 DCT per block with a
quantiser table, every frame stands alone (no motion; the ident is slow),
byte tokens for the coefficients, and 22,050 Hz mono 16-bit sound
interleaved per frame. The format is written in `dmv.h`. `dmv.c` decodes it
in integer arithmetic (the IJG's islow IDCT), so the host and the SH4 make
the same pixels; `fmv.c` plays it:

- `main()` calls `dc_fmv_play()` after the sound stream is up and before
  `doomo_init()`. START or A skips it (once the pad has been seen without
  them, so a button held from power-on does not); no `/cd/INTRO.DMV` (the ELF
  run from a cable, a disc built without ffmpeg), a short or damaged file,
  or a read error ends it at once and the game boots as before.
- The whole file (960 KB) is read into the heap 16 KB a vblank while it plays,
  with the decode buffers (365 KB); all of it is freed after the movie, so the
  `.bss` is as it was (+800 B of the DC9C code's own variables). Each frame
  is decoded to RGB565 (20 ms mean, 30 ms worst on Flycast, under the 66.7 ms
  a 15 fps frame has), uploaded to one of two 512x256 PVR textures and drawn as
  one bilinear quad at 2x (640x352, black bars). A frame that is late is
  skipped before it is decoded.
- The sound is made into the mixer's PCM slots (1,470 stereo frames, one a
  tick; `dcsnd_push_pcm`, which leaves the synth and the content hash
  alone), at most 3 ahead; the picture waits 3 ticks for the AICA's buffer.
- It is off in every scripted run (`DC_TEST_NO_FMV`, bit 31 of the mailbox's
  `test_flags`, which dcrun sets unless `--fmv` is given; `--console` alone
  leaves it off too). The frame counts and sound hashes of all other scripts
  stay as they were. During the movie no game tick runs: scripts use
  `frames N`, `wait-log FRAMES TEXT` and `print-frames` (new in dcrun, with
  `expect-town-crc 0xCRC`).

Quality of the encode (`make -C dc/tests/fmv test` checks the decoder, the
numbers below are from the real file): 4.9 KB per frame at most (3.4 KB mean),
mean PSNR against the source frames 34.8 dB (worst 32.8 dB) after the RGB565
rounding. On Flycast the movie plays 152 frames, skips none, and its sound
follows the source's loudness over time with an envelope correlation of 0.98.
Before / after pictures: a new screen, so no "before"; the three shots of
`dc/tests/fmv/intro.txt` (`dc/build/shots/disc/intro/1/shots/`) show it.

**Host test** (`make -C dc/tests/fmv test`, in `make dc-host-test`; ASan and
UBSan: `SAN=1 BUILD=../../build/host-fmv-san`; needs numpy): `gen.py` encodes
a synthetic clip with `mkfmv.py` (gradient, colour bars, black, hard stripes
that need the escape code, noise) and `test_dmv` checks that every frame
decodes to within 2 levels of an ideal float IDCT of the same coefficients
(the first version, a plain matrix IDCT, took 74 ms a frame on Flycast; this
one, with the colour conversion done once per 2x2 pixels, 20 ms),
within 28 dB of the source after the colour conversion, that the records walk
exactly to the end of the file with the audio intact, that a frame cut short
is refused, and that 3,000 mutated and truncated frames do no out-of-bounds
read or write and no signed overflow (the decoder clamps what a corrupt
stream could grow). `test_audio` has 22 more checks for `dcsnd_push_pcm`
(the PCM slot comes out unchanged, the synth and the hash are untouched, a
full ring refuses it).

**On Flycast** (`make -C dc disc-test`, `dc/tests/disc/run_flycast.sh`; every
check is played twice and the two runs must be identical: the same log
numbers and the same bytes in every shot):

| check | what | 2026-11-15 |
|---|---|---|
| boot | the CDI boots to the title without the movie | title 7.12 s after power-on (426 video frames: the game's mailbox is up after 2.7 s, then the boot scene's 123 ticks) |
| intro | `tests/fmv/intro.txt --fmv`: the movie from the disc, then the title | starts 3.0 s after power-on, 152 frames shown, 0 skipped, ends 13.3 s after power-on (10,269 ms), the title scene is set 4.1 s later (17.5 s, `scene: set TITLE`); 304 sound slots; one audio underrun in the handover from the movie's PCM slots to the synth (the 20-vblank drain and `doomo_init()` push nothing for about 0.3 s; 1 of 447 slots, heard as a click at worst, left as is) |
| skip | `tests/fmv/skip.txt`: START during the movie | skipped after 3.7 s, the game boots to the title |
| e2e | `tests/disc/e2e1.txt` then `e2e2.txt` on one VMU image: title, Free Play, a new island, the island's menu, Save / Load, A; `vmu_check.py` on the image; a restart; the title's Continue loads it; `load 1` gives the crc `e2e1` printed | the save writes 27 blocks in 2.96-3.00 s, the title's load reads 9,734 bytes in 0.55-0.66 s |
| old | `tests/disc/old_saves_vmu.bin`, the VMU image the build before DC9C made (story and free-play saves of DC4's format), loaded by `tests/disc/old.txt` | both saves found, verified and loaded by the core (its own version and CRC checks); each written back by the current build and loaded again with the same town crc |

The VMU format did not change in DC9C. `old_saves_vmu.bin` is the proof that
an old save loads: `vmu-test`'s `saves1.txt` made it on the base build
(ad060127). The islands in it are not compared with a fresh `town-fill` (the
core's town moves from commit to commit: after a rebase the same fill gave
another crc), only with themselves after a round trip.

**Disc size, boot time, save time** (Flycast's emulated time, 2026-11-15):

| | before DC9C | now |
|---|---|---|
| `doomogachi.cdi` | 7,637,587 B | 8,744,851 B (`INTRO.DMV` 959,692 B, `1ST_READ.BIN` 5,080,652 -> 5,089,572 B); 8,784,563 B on the tree rebased onto the other streams' core changes |
| ELF image (`make -C dc sizes`) | 9,657,580 B, 7,049,996 B free above `_end` | 9,664,376 B, 7,040,428 B free: +6,796 B (code 6 KB, `.bss` +800 B; the 365 KB of decode buffers are heap, for the movie only). After the rebase 9,699,624 B, 7,005,228 B free |
| power-on to the title | 7.12 s | 7.12 s scripted; 17.5 s with the movie |
| save a new free-play island (27 blocks) | 2.96 s | 2.96-3.00 s |
| save a story island (33 blocks) | 3.67 s | 3.67 s |
| save the 1000-citizen worst case (128 blocks; 129 after the rebase onto the other streams' core changes) | 13.4 s | 13.4 s (13.5 s, 129 blocks) |
| load: new island / story island / worst case | 0.55 / 0.74 / 2.6 s | the same |
| settings record (2 blocks) | 0.46 s | 0.46 s |
| learn the card at boot | 245-315 ms | the same |

**What can the Dreamcast side cut from the VMU save?** Measured: nothing that
is safe, so nothing was changed, and the save is still a pause of 3 s (the
screen shows the card with its bar; no tick over 33.3 ms is hidden by it, the
tick is one long one). Where the 2.96 s of the new island go (27 blocks):
every block write is 5 Maple round trips in KallistiOS (`vmu_block_write`:
four 128-byte phases and a sync), and a Maple frame leaves only in the vblank
handler (`maple_vbl_irq_hnd` -> `maple_queue_flush`), one per device per
vblank: 5 x 16.7 = 83.4 ms a block, 27 blocks 2.25 s; the read-back that
makes the swap safe is 27 reads at one round trip each (16.7 ms): 0.45 s;
the FAT and the two directory writes of the replace: 3 more blocks, 0.25 s.
The levers, with what they would give and why they are not taken:

| lever | gain on the 27-block save | why not |
|---|---|---|
| compress the payload again | none: the core's own LZ codec has already compressed it (the new island's 9,734 B re-compressed with zlib -9 are 8,661 B, 2 blocks of the 27; a synthetic story island of 13,123 B, 10,723 B) | a second coder on the save path for 0.3 s |
| no eyecatch (4 blocks) and one icon frame (2 blocks) | 0.5 s | it is what the BIOS file manager shows: the art DC4 chose |
| skip the read-back | 0.45 s | it is what makes a bad write visible before the old save goes |
| queue the 5 frames of a block (or the reads of 8 blocks) in one vblank | 4-5 x on the writes, 8 x on the read-back and on every load (the load of the worst case 2.6 s -> 0.3 s) | not provable here: Flycast would accept it, a real VMU may answer `RESPONSE_AGAIN` while it is busy writing flash, and KallistiOS resends such a frame after the later ones, so the phases and the sync could arrive out of order. Needs a real console. Loads (reads only, any order) are the safe half of it. |
| write behind the game (a thread) | the tick would not block at all | the core's `plat_save_write` must answer "saved" after the read-back; a late failure would be a lie. It is S2-10's design (the core), not a platform change |

So the rest is S2-10 (`save.c` streaming and a different write contract) and a real
console. **Owed on hardware:** the per-block write time of a real VMU, and
whether a batched read of the card is accepted by it (the half that would cut
the loads).

**Gates on the rebased tree** (2026-11-15): `make -C dc test` is green except `ow-test`'s
`side_sample_a` (known, fails on the base too) and `room-test`'s `home_visit` (dcrun and the host
runner both fail its `expect-scene DIALOG` / `TALK` / `HOME` lines while the native runner passes:
a core change of another stream, not seen by anything of DC9C: the host runner links none of it).
Repaired 2026-11-15 (TIP-DC-HOMEVISIT): the Dreamcast has no Talk screen, so `room-test` now runs
`dc/tests/house/home_visit.txt` (the core script minus the live Talk steps; a copy beside
`run_flycast.sh` wins over `core/tests/scripts`). Only dcrun and the host runner take that copy;
the native reference runner always takes the core script (it has the live Talk screen, and the Talk-less
copy fails its scene checks). `dc/tests/families/scripts/home_visit_dc.txt` is a near-duplicate kept for
family-test2. TD2 landing: Talk is live on the DC (TD1), so that copy failed on dcrun (`expect-scene HOME` while the Talk screen is open); the copy is deleted and `home_visit` left `room-test`'s list (the host runner links no Talk, so the three-way compare cannot hold its Talk shots). `family-test2` plays the core script on dcrun with the live Talk screen. `make -C dc room-test` is green again.
`check-elf` expected a `dc_mailbox` of 4,364 B and the ELF's is 4,372 B since DC6B/6C: the check says
4,372 B now (`make -C dc test` stopped at it before).

**Owed on hardware (DC9C):** the disc read of `INTRO.DMV` from a real GD-ROM /
CD-R (the 16 KB steps are about 100 KB/s of the drive's 1.2 MB/s, the first
read must come before the first frame), the AICA's latency against the 3-tick
picture lag, and the colours of the bilinear quad on a CRT (the PVR's RGB565
texture is shown without dithering).


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

### The kept backdrop (DC9B)

The core lets a scene give the platform a full-screen backdrop that depends on a small key
(`plat_stage_begin` / `_restore` / `_end`, `core/include/doomo/plat_ext.h`, `backdrop.h`). Only the
exorcism's church uses it on the Dreamcast (the minigame booths' family is not taken: those scenes are
cheap). The Dreamcast rebuilds the picture every tick, so `plat_stage_begin` answers "yes, but the buffer
holds nothing" (`held` 0); the core then asks for the whole backdrop with `plat_stage_restore` and draws
every moving layer over it.

* **First restore of a key (a bake).** The core's own `draw(key)` runs with the 2D calls pointed at a
  320x240 canvas of palette indices (`rect`, `line` and `shade_rect` as the native runner does them,
  `image` without shadow or silhouette). Any other call (text, checker, a post effect, a palette lerp)
  ends the bake: the key is marked unbakeable and is drawn the old way, prim by prim, from then on. The
  canvas is uploaded as one 512x256 PAL8 texture (3.9 ms) and the backdrop is two prims: a black rect under
  it (index 0 of a texture is clear) and the texture.
* **Later restores.** Two prims. The church was 3,100 to 4,900 prims.
* **Shades over it.** An exact shade (`plat_shade_rect`) re-emits what lies under its rect. For the
  canvas that is the same prim over a copy of the canvas sent through the colormap level (one copy per
  key and level, kept) when the shade is 12,000 logical pixels or more, else the canvas's solid runs under
  the rect (about 130 to 400 prims for the church's panel shadows). Nested shades work (a copy of a copy).
  The full-screen dim of the verdict screen is a copy: two prims.
* **Memory.** 6 keys and 4 shaded copies, 76,800 B of canvas each: 768,000 B of `.bss` (`stg_canvas`), and
  up to 10 textures of 128 KB in video RAM, freed by the texture cache when unused for 120 frames.

Verified: `dc2d_test` plays a stand-in backdrop (stripes, lines, a circle of rects, hearts, flipped and
scaled, shaded bands) through the stage and compares every frame with the native one, with every mix of
eight shades over it (255 frames), another key, the key again, and a backdrop that cannot be baked. On
Flycast `life_exorcism` has 9 of its 11 shots byte-identical to the old path; `result` and `verdict` changed
by about 300 pixels each, and those are now **equal to the native runner's frame** (the old path was off by
up to 26/255 on 19 to 26 logical pixels around the demon's soft shadow).

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
| `dc2d_test` | 593 checks, 0 failed (52 at DC3; DC9B added the kept backdrop in 255 shade mixes and the palette table; also under ASan/UBSan) |
| the six DC3 UI scripts, hardware effects | 94 shots, **100.000% exact** on every shot; at most 2,704 prims and 223 KB of vertices a frame, nothing dropped |
| all 80 runner scripts, `DC3_FX=sw` | 905 shots, 854 exact to the pixel, mean 99.984%; worst 98.59% (home_defend: `room_ref` reuses one uid for translucent actors whose dither changes every frame, so a uid-keyed cache shows the last pattern; the room is DC8's own renderer on the console, section 8, whose translucent residents are GLASS prims) |
| all 80 runner scripts, hardware effects | 905 shots, 810 exact; the rest differ where the post effects do their hardware job (shakes, palette lerps, blends) |
| heaviest frame | home_rooms (`room_ref` through the 2D calls): 10,287 prims, 938 KB of vertices, inside the budget; DC8 draws rooms itself (section 8: 50-190 prims a view) |

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

### The full game's scripts and frame times (DC9A)

`make -C dc family-test` (`dc/tests/families/run_flycast.sh`) plays every
`story_*`, `fp_*` and `side_*` script of `core/tests/scripts` (28 scripts,
about 38,000 ticks) twice in dcrun on the CDI, from the native runner's
start (`--native-seed`). A script passes when both runs end with "0
failures" and the two runs are identical: the same tick and frame counts, the
same work summary, and the same bytes in every shot (md5 of each PNG).
Measured 2026-11-13: 28 of 28 pass, 28 of 28 identical.
"Pass" means the script's own expectations hold and the two runs match each
other. It does not mean parity with the native runner's shots, and it does not
gate sound underruns (story_es logs one). An exit 132 (Flycast SIGILL on
unload) counts as pass when the log ends with "0 failures", as in ow-test.
Four of them failed at first on a command the native runner has and dcrun lacked: `lang` /
`expect-lang` (fp_playas_free, story_es), `expect-hero`
(story_newgame_hero), `story-fp` (story_engine), plus `ending-seen` and
`story-done`. They are mailbox commands now (`DC_HC_LANG`, `_HERO`,
`_ENDING_SEEN`, `_STORY_DONE`; `story-fp` uses the old `DC_HC_STORY_FP`) and
the mock-core self-test covers each (`make -C dc/harness test`).

Ticks over 33.3 ms of work (Flycast's emulated time): none in 19 scripts,
the worst steady tick 31.5 ms (story_invasion, the combat view). The other
nine scripts have the loading ticks of the next table.

| what | ticks | work |
|---|---|---|
| making a new island (New Island screen) | 1 in fp_apartments, fp_care, fp_dark, fp_island, fp_night, fp_playas_free, fp_shop, fp_screens | 125 ms |
| title to a new story (story_newgame_hero) | 1 | 127 ms |
| VMU save and load in fp_screens (the card with its progress bar is on screen) | the other 2 of its 3 | 3.2 s (the save) |

Speed tour 2 on the Dreamcast (`sh dc/harness/speed2.sh OUT`, the ten
groups of `gba/harness/scripts/speed2_*.txt`, 218 windows, every window the
same script as on the GBA and PS1): 215 hold 30 logic ticks a second with no
tick over 33.3 ms; 3 windows do not:

| group | ticks | work mean / worst | ticks over 33.3 ms (worst) |
|---|---:|---|---|
| action | 3,254 | 13.0 / 28.4 ms | 0 |
| creator | 2,335 | 8.2 / 15.6 ms | 0 |
| dream | 1,483 | 8.3 / 16.3 ms | 0 |
| fair (minigames) | 6,040 | 7.5 / 15.8 ms | 0 |
| fishcook | 3,334 | 6.3 / 17.8 ms | 0 |
| house | 2,461 | 3.4 / 20.6 ms | 0 |
| library | 2,632 | 8.8 / 18.9 ms | 0 |
| life | 3,920 | 8.8 ms mean | 2: new island 125 ms, saving 4.54 s |
| story | 3,214 | 6.2 ms mean | 1: title to new story 129 ms (127 ms in the family script above: two different runs) |
| studio | 1,165 | 7.2 / 30.8 ms | 0 |

The credits and the credits parade, which speed audit 2 named, hold 30 on
the Dreamcast now (no tick over 33.3 ms). The three misses are what is left: the two loads are the
core making a town (`town_new` / the story's start, about 100 ms of SH4
compute) and the save is the VMU's block writes (about 40 blocks, each a
Maple round trip). All three belong to speed package S2-10 (saves and loads
without a frozen screen), which is in flight on the SPEED stream and changes
core code; nothing on the Dreamcast side can shorten them without it. The
save already keeps the screen alive: its card redraws every vblank with a
progress bar while the writes run. Raw logs of the runs: `dc/build/shots/families/`
and the speed2 output directory.

### The rest of the full game's scripts and frame times (DC9B)

`make -C dc family-test2` is DC9A's `family-test` (same runner, `dc/tests/families/run_flycast.sh`,
same pass rule: both runs end with "0 failures" and repeat byte for byte) for the other scene families,
from `core/tests/scripts` (`FAMILY_GLOBS` in `dc/Makefile` names them): minigames (`mg_*`), the house
(`home_defend`, `_editor`, `_flats`, `_gallery`, `_haunt`, `_mart`, `_rooms`, `_tutorial`), the creator
(`creator*`), the studio, fishing and cooking, the library (`library_*`), dreams (`dream*`), the concert
(`concert_*`) and the exorcism (`life_exorcism`); `home_visit` was a script of our own
(`home_visit_dc.txt`, without the Talk screen) until TD1, which brought Talk: the core's `home_visit.txt` now passes on dcrun,
twice identical, 0 of 1,141 ticks over 33.3 ms, worst 26.0 ms. Measured 2026-11-15 on Flycast (emulated time):
**44 of 44 pass, 44 of 44 identical**, 586 shots, 43,046 ticks, 25 over 33.3 ms (23 concert, 2 exorcism); the numbers are from the full run on the rebased tip (the builder's first run had 42 scripts, `creator_sliders` and a creator script came with later landings).

| family | scripts | shots | ticks | ticks over 33.3 ms | worst tick |
|---|---:|---:|---:|---:|---|
| minigames | 7 | 44 | 14,069 | 0 | 21.8 ms |
| house | 9 | 126 | 5,884 | 0 | 21.4 ms |
| creator | 10 | 207 | 7,238 | 0 | 22.7 ms |
| studio | 1 | 19 | 1,495 | 0 | 19.4 ms |
| fishing and cooking | 2 | 26 | 2,989 | 0 | 17.2 ms |
| library | 9 | 108 | 2,579 | 0 | 28.1 ms |
| dreams | 2 | 17 | 1,612 | 0 | 16.8 ms |
| concert | 3 | 28 | 5,759 | **23** | 37.5 ms |
| exorcism | 1 | 11 | 1,421 | **2** | 33.9 ms (the game's own largest tick: 35.1 ms) |

Not in the list, and why:

* `home_talk_basic`, `_group`, `_safety`, `_visit` play the Talk screen (`talk-enter`, `talk-read`,
* `home_talk_basic`, `_group`, `_safety`, `_visit` play the Talk screen (`talk-enter`, `talk-read`,
  `expect-talk-*`). Talk is built on the Dreamcast (TD1: the bank is read from the disc; TD2: dcrun has the `talk-*` commands, see "Talk on the Dreamcast"). `home_talk_basic` runs unchanged except `expect-toast`; TD3 puts these four on the list (it needs `expect-talk-act`, `expect-toast`, `talk-rows`).
* `home_visit` expects the live Talk screen after A at the host (greeting box, the Talk list, B, the goodbye box). Until
  TD1 the Dreamcast had a copy without that step (`home_visit_dc.txt`); since TD1 the core script runs as it is (it only
  uses `press` and `expect-scene`), the copy is gone and `home_visit` is in `family-test2`'s list.

What the first run (before any change) found, on the same machine: the two Talk scripts' failures above and
two real frame-time misses:

* **The exorcism** was over the 33.3 ms budget on 1,012 of 1,421 ticks (mean 38.1 ms, worst 51.0 ms; the
  church is 3,100 to 4,900 SOLID prims a tick, 6,941 with the foreground, 31 ms of the tick). Fixed with the
  kept backdrop (section 4, "The kept backdrop"): mean 12.1 ms, 2 ticks over by dcrun's per-tick log (33.9
  and 33.5 ms), the game's own largest tick 35.1 ms, prims at most 2,263. The ticks left are the ones where
  the church's key changes (a bake 14 ms, 3.9 ms to upload the canvas, one shaded copy 6 ms, all in one
  tick with the foreground); the church changes key about every 15 ticks at the most, and a key already
  seen again costs nothing. **Gap: 1.8 ms on the key-change ticks (35.1 against 33.3).** (dcrun's "work
  over" line averages the ticks that end in one video frame, so the game's own `work max` is the safer
  number.)
* **The concert** was over on 63 of 5,759 ticks (worst 40.6 ms): `concert_choir` ticks 57 to 101, the
  Concert Hall's setup page with a choir of 25 (3,000 prims, 19 ms of the core drawing the stage and the
  25 portraits, 9 ms to send them), and 22 ticks of `concert_band` (worst 38.1 ms). Two changes that touch every screen cut it to
  23 ticks (worst 37.5 ms): `dc2d_pal_argb` is a table lookup (`pal.c` `argb_tab`, filled with the banks
  and the palette lerp), and the header test of two rects and the sprite colour of `send_prim` are done
  only when a header is sent. Frames are identical (shots of 8 scripts compared with the old build:
  byte for byte), and the mean work of every speed-tour group fell 5 to 15 % (table below).
  **Gap: 4.2 ms on 23 ticks of the choir setup page.** What is left is the core's drawing of the stage; a
  kept backdrop for the concert would need a backdrop family in core (the church's is `exo_church_*`), which
  is a cross-stream change not made here.

Speed tour 2 (`sh dc/harness/speed2.sh OUT`, the ten groups, 218 windows) before and after, work per tick
(mean / worst), the same windows and scripts, measured on this machine: before is the tree at `17d5e17f`
(the table of DC9A above), after is this package.

| group | before | after | ticks over 33.3 ms |
|---|---|---|---|
| action | 11.4 / 26.0 ms | 10.7 / 25.4 ms | 0 |
| creator | 8.2 / 15.6 ms | 6.5 / 13.8 ms | 0 |
| dream | 8.3 / 16.3 ms | 6.9 / 14.6 ms | 0 |
| fair (minigames) | 7.5 / 15.8 ms | 5.6 / 12.2 ms | 0 |
| fishcook | 6.3 / 17.8 ms | 4.8 / 16.5 ms | 0 |
| house | 3.4 / 20.6 ms | 2.9 / 20.2 ms | 0 |
| library | 8.8 / 18.9 ms | 6.9 / 15.6 ms | 0 |
| life | 8.8 ms mean | 7.2 ms mean | 2: new island 121 ms, saving 4.53 s |
| story | 6.2 ms mean | 5.0 ms mean | 1: title to new story 129 ms |
| studio | 7.2 / 30.8 ms | 5.9 / 29.4 ms | 0 |

215 of 218 windows hold 30; the 3 hitches are the three of DC9A (the new island, the VMU save and the
new story: the core making a town and the card's block writes, speed package S2-10). Studio's worst steady
tick is 29.4 ms (4 ms under the budget). The tour does not play the exorcism; `life_exorcism` above does.

Memory: `make -C dc` prints `.bss` 5,354,740 B (4,581,012 B before this package on the same base, +773,728 B:
the kept backdrop's 10 canvases of 76,800 B, section 4, and the other streams' changes since), 5.99 MB
free above `_end` (6.74 MB before; the floor is 4 MB). Video RAM: up to 10 canvas textures of 128 KB, the
texture cache frees them when unused.

### Owed on hardware

- The frame time on a real console (Flycast's timing is not exact), on
  the heaviest screens (island hub, creator, home rooms, the concert's choir setup page, the
  exorcism's church-key ticks); and the three
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
| (ST1) analog L / R | `input_set_caps(INPUT_CAP_TRIGGERS, l, r)` every tick (0..255); a second stick adds `INPUT_CAP_RSTICK` |

**Stick + triggers (ST1).** With Options > Controls on **Auto** (the
default) the shooter plays a one-stick scheme on the standard pad: the
stick's X turns (eased), its Y walks, the analog triggers strafe with the
pressure as the speed (`combat_trigger_strafe`: a small dead zone, then the
stick curve), both triggers in lock on, the D-pad walks / strafes as well, A
fires, X rolls. A twin stick (a second analog stick) plays the PS1's dual
stick. The house's view still turns on the triggers (L / R past half), the
overworld walks with analog speed off the stick, and the creator's
turntable opens on SELECT as before (no right stick). The control card and
the shooter tip show the one-stick lines ("Stick Walk/turn", "L/R
Strafe").

**Options > Controls > Stick: Turn / Strafe (STKOPT).** A row on the
one-stick Dreamcast pad only (hidden on the GBA, PS1 and a twin stick).
Turn (the default, today's behaviour): the stick's X turns, the triggers
strafe. Strafe: the stick's X strafes and the triggers turn (pressure =
speed, eased like the stick). It is `OPT_STICK_MODE`, the last id of the
second option table (OPT2); it rides in the 'OPT2' chunk (one more byte,
inside `SAVE_EXT_MAX_OPT2`). A chunk from before it (or a save with no chunk)
loads as Turn. A preset and the Quick page's presets leave it alone (it is a
pad preference, like Tips), but "Reset page" on the Controls page does set it
back to Turn. The shooter tip and the Controls help line follow it: in Strafe
the tip reads "Stick: walk and step aside. L/R triggers: turn." The row shows
whatever the Controls scheme is, and its help line says it works on Auto only
(Classic and Modern have their own stick rules). Tests:
`sticks_option_strafe_mode`, `u0_opt2_chunk_round_trip`,
`stkopt_preset_keeps_the_stick_mode`, `stkopt_shooter_tip_follows_the_stick_mode`.
On Flycast (`dc/tests/sticks/stick_option.txt`): the row shows on the Controls
page (below Rumble), RIGHT picks Strafe, the stick's X then strafes and R
turns; `sticks.txt` (the default) runs as before. On the landed tree:
0 of 396 ticks over 33 ms, worst 29.0 ms in COMBAT; `sticks.txt` 0 of 345,
worst 29.4 ms. The new row widens the Controls page's mask in
`dc/harness/known_diffs.tsv` (`pause_options` / `opt_0_controls.png`, the
Rumble row's 24 px grown to 48); `make -C dc ui-compare` is 94 of 94 with it.
One new row on the Dreamcast's Controls page is the only visible change; the
GBA, PS1 and native screens are unchanged. GBA EWRAM / IWRAM net zero (the
value is a bit next to the two option flags); ROM only, .text +144 B,
.rodata +84 B. The one-stick tip lines and the row's help line are inside
`#if DOOMO_TRIGGERS`, so the PS1 and the GBA carry no byte of them; what is
left on the PS1 is the OPT2 table row and its three words (about 40 B of
-Os rodata, every core file here is already -Os). The PS1 EXE was not built
or measured in this package: that machine has no PSn00bSDK.

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

**The music engine's tier (DC5b).** The music engine runs on its IRQ tier
(`CAP_DRV_IRQ`, `AT_NATIVE`, `dc/src/musvoice.c`). The core's
`music_update()` only writes events ahead; the mixer calls `mus_drv_tick()`
8 times per slot from inside `dcsnd_push()`, mt k right before the frames
of that mt (frame 0, 183, 367, 551, 735, 918, 1102, 1286 of the slot), on the
main thread, the one that runs `music_update()`: no lock at all. A record
the driver sends (`plat_voice`, on change only) reaches the mixer as one of
8 music voices (`dcsnd_mvoice`) and is heard from exactly that frame. A push
into a full ring still runs the driver (the music keeps the game's time) but
renders nothing. `make -C dc MUSIC_IRQ=0` (`-DDC_MUSIC_IRQ=0`) builds the
legacy bridge again.

What the renderer does with a `DoomoVoice`: the pitch is `pitch16` to 1/16
semitone (the bridge rounded to whole Hz: up to 30 cents off in the bass);
`vol_l` / `vol_r` are 0..63 per side on top of the voice's place in the
field (lead and bass centre, harmony a little left, kit a little right);
the timbre follows the instrument (pulse with a duty of `tparam` / 32, saw,
triangle, noise 15 / 7 bit; kit hits are the classic noise sweep, a key-on
restarts the noise so every hit sounds the same; a key-on of a silent
pitched voice starts at phase 0, a sounding voice keeps its phase). The
soft envelope is already in the volume, so `env` and the hardware hints
(`DVF_KEYOFF`, `LEGATO`, `REVERSE_ENV`, `SWEEP_HW`) do nothing. Voices 0-3
also leave their classic tone in `mus_bridge_tones[]`, and `musvoice.c`
defines `audio_channel_state` (the jukebox meters) from it. In the core it
is a weak alias of `audio_seq_out()` (`audio_seq.c`: no code and no byte
on any port; the babble's hand-back in `audio.c` calls `audio_seq_out()`,
which stays what the sequencer sent: silence here). The first version of
this answered the meters from the core itself, and the hand-back then put
the harmony on tone channel 1 a second time after 21 of 40 blips; test_tier
checks it now.

S1's effects: the core applies mute, duck and tape stop only in the bridge's
flush, so `dc_music_tick()` asks `audio_fx_music()` once per logic tick (a
probe with volume 256 gives the bus gain, a probe at 60,000 Hz the tape
stop's pitch in sixteenths of a semitone) and sends the music bus
(`dcsnd_music_gain`) and the tape stop (`dcsnd_music_tape`) on change.
The tape stop's last step is 14 semitones, as in the core's table (232
sixteenths, then 224, then the hold mute). `plat_audio_bus(0, g)` does the
same for a core that sends the gain itself. `MFX_ECHO`, `REVERB`, `FREEZE`,
`CRACKLE`, `PSG_DUCK` and `STEMS` are accepted and do nothing (the core
never sends the first three). The voice babble and the comedy one-shots
stay on the four tone channels, and the music no longer gives way to
them: on the bridge a music channel is skipped while the babble
(`audio_voice_busy`) or a comedy one-shot (`audio_fx_owns`) holds it, but
on this tier the music voices are mixer voices of their own and keep
sounding underneath. Not yet listened to on a console for clutter or
clipping under the babble; if it is too much, silence voice 1 / the owned
channel's music voice while those two answer yes. `latency_mt` is 0: music and effects share
the one stream. The tape calls (`plat_tape_*`) keep the weak defaults; the
core has no `CAP_TAPE` use here. `plat_sample_ex` (pan, priority, pitch) is
implemented and tested on the host; the core does not call it yet, so
`--gc-sections` drops it.

Checked on the host (`make -C dc/tests/audio tier`, part of `test`):
`test_tier` runs every song of the game (28) for up to 300 ticks on the
IRQ tier and on the bridge (`DC5B_TICKS`); on the IRQ tier a reference
renderer for `DoomoVoice` records (`voiceref.c`, a separate double-precision
implementation) renders the same records at the same frames and the two
streams must be within 1 LSB, with no driver under-run; both tiers must
play the same number of key-ons. The unit tests (72 checks) cover the
pitch, every record kind, 900 ticks of random records, mute, duck and tape
stop, the babble and the comedy one-shots, the queue and the caps.

**Not the bridge's sound, by design.** With `CAP_DRV_IRQ` the core runs the
driver's control frames at 4 mt (60 Hz), where the bridge runs 8 mt (30 Hz):
the instruments that are not `MIF_ENV30` then step their envelopes twice as
fast, so notes decay and release sooner and rests are exact digital silence
(the bridge never reaches it). The pitch is exact and the duty is any
tparam / 32. Measured against the bridge on the host (6 songs of 20 s,
`dcsnd_runner` against `dcsnd_runner_irq`): envelope r 0.5-0.83, level
0.73-0.77, same note in 50-64 % of the windows. That is the music engine's
own 60 Hz design, not a mixer fault: the key-on counts are equal on both
tiers on every song and tick by tick, and on the host the mixer matches the
`DoomoVoice` reference to 1 LSB. So the Flycast gate for an IRQ image is not
the native runner's WAV (the bridge's sound) but `dcsnd_runner_irq`'s: the
same runner with `musvoice.c` linked (`DCSND_WAV`). The old comparison with
the bridge is printed as "vs bridge" and not gated.

### Cost

On Flycast's clock (not cycle-exact: a guide until a console is timed),
the mixer takes 1.33 ms per tick on the title (max 1.65), 1.41 ms in
combat (max 1.74) and 1.45 ms on the invaded town (max 2.39): 4-5 % of the
33.3 ms tick, counted in the tick's work. Every tick of the three runs
stayed inside the 30 fps budget (title work mean 3.4 ms, worst 12.3 ms;
the 3D view is still DC6's stub). Memory: the ring 94,080 B and the copy
buffer 12,288 B (`.bss`), the effects bank 95,656 B (heap, 35 effects in
16-bit), code 4.7 KB. DC5b added 8 music voices of 36 B (`mv` 288 B), the bus and tape
state, 2 counters and `mus_bridge_tones` (16 B): about 370 B of `.bss`/`.data`
(measured in the ELF's symbols, 2026-11-13).

On Flycast with the IRQ tier (2026-11-13, `make -C dc audio-test`, the
three scripts against `dcsnd_runner_irq`; `--drop-gaps` is off there: the
music's own rests are exact silence too, and the cut took them for stream
gaps, title r 0.99 -> 0.63):

| check | envelope r | same note | level | mixer per tick | work over 33.3 ms |
|---|---|---|---|---|---|
| title | 0.990 (bar 0.9) | 97.6 % of 85 | 0.97 | mean 1.22 ms, max 3.93 | 0 of 611 ticks (worst 13.0 ms) |
| invasion | 0.997 (bar 0.9) | 97.3 % of 74 | 0.98 | mean 1.42 ms, max 1.65 | 0 of 454 (worst 28.9 ms at tick 2, the scene load) |
| combat | 0.822 (bar **0.8**, clipped at p97) | 88.1 % of 67 | 0.98 | mean 1.32 ms, max 1.86 | 0 of 409 (worst 16.2 ms) |
| repeat | title hash600 0xc3e91676 twice | | | | |

The bridge image, same day: title r 0.990, invasion 0.988, combat 0.904 (the
bar 0.9). Combat on the IRQ tier is 0.08 under that bar and is gated at 0.8:
the music lines up with the host render tick for tick (the best lag is 100
ticks to 0.1 ms; per tick levels agree), but the scene's own effects and
enemy noises differ between dcrun and the runner (see `combat.txt`), and the
60 Hz envelopes show a one-tick difference more than the bridge's did. No
other cause was found; a stub 3D view makes it worse. The IRQ tier costs no
more mixer time than the bridge (title mean 1.22 ms against 1.37 ms: fewer
tone channels run). Music works with 0 sound underruns and 0 late refills in
the three windows.

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

(DC8) Whether a hold starves the stream depends on emulated timing, so
unrelated code can move a gap into the middle of a run: DC8's build put a
1,001-sample gap into combat's WAV (the same sound: the content hash
0x664c5844 on both builds), which shifted the rest of the envelope against
the single lag and dropped r from 0.920 to 0.859. `run_flycast.sh` therefore
asks compare.py to cut such gaps first (`--drop-gaps 256`: runs of exact
digital silence of 256 samples to 0.1 s after the first sound; the cues'
own pauses are longer, and are left alone). With it, on DC8's build: title r
0.989, invasion 0.988, combat 0.904 (1,001 samples cut); on the merge base
the cut finds nothing and the numbers do not change (0.998, 0.989, 0.920).
Combat stays the marginal one (its logic diverges from native while the 3D
is DC6's stub).

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

## 8. The dream house (DC8)

```sh
make dc-host-test        # includes dcroom_test: the house against room_ref on the host
make -C dc room-test     # room_ref's golden frames and the house scripts on Flycast
make -C dc test          # ... and everything else (room-test is its last step)
```

`plat_room` (and `plat_room_project`, which Talk's speech bubbles anchor on)
is `dc/src/room.c` (portable C, `room_int.h`). Before DC8 the Dreamcast used
the core's weak default, `room_ref.c` through the 2D calls: correct, but its
floor and walls went to the PVR row by row (up to 8,332 prims, 744 KB of
vertices a frame in the house scripts; 10,287 in DC3's busiest host frame,
near the list's 12,288) and the house cost 28-50 ms of SH4 time a tick on
Flycast: 1,560 of home_defend's 1,593 ticks and every tick of home_flats were
over the 30 fps budget, and the sound underran (214 times in home_defend).

### What it draws

What `core/src/room_ref.c` draws, pixel for pixel at the 2D layer's 2x: the
projection (the view `k` from the yaw, 16 units a tile, the back corner
centred in the viewport, the pan), the shell (the viewport colour or the open
kinds' sky and stars, the roof with its chimney, dormer, skylight, tank or
spikes, the Garden's and the Park Plot's backdrop, the two back walls in all
8 wallpapers with the cornice ramp and the corner post, all 8 floors with
their grout, the plot's lip on both front edges), the light (H8: the daylight
level, up to 4 lamp pools on the floor tiles, the wall columns and the
pieces, glow pixels at full light, a switched-off lamp's glow dark, the
windows' panes and the roof glass by daylight), the furniture art through
`room_ref_sprite()` (every facing with H9's backs, the paints, the door and
window skins, the animation frames, the half-tile smalls, the possessed
jitter), the tints, the residents (flipped, squashed, tinted), the
rubber band and the cursor, and the order (the back-wall pieces, then rugs,
pieces and residents in room_ref's topological order of their footprints,
then the open kinds' front-edge pieces, then the cursor). Where a piece's
sprite does not fit the scene's `plat_cache` (or there is none) it is
room_ref's shaded box, as on native.

Where the Dreamcast does more than the reference (the plan's DC8 list):

| | room_ref (native) | the Dreamcast |
|---|---|---|
| a held / placed ghost (`RO_GHOST`, `RO_SEMI`), a translucent resident (`BB_TRANSLUCENT`) | a 50% checker dither | the piece at 50% (a GLASS prim: texture times ARGB, blended) |
| the blueprint ghost (`RO_GHOST` + `RO_SEMI`, "buy at HomeMart") | a grey dithered silhouette | the grey silhouette at 50% |
| an invalid ghost (`RO_INVALID` on a ghost) | dithered, a shade darker | the silhouette in red at 75% (the blueprint's grey ramp times red), so "not here" reads at a glance |
| a solid invalid piece (`RO_INVALID` alone; no script sets it) | unchanged, a shade darker only while tinted or blinking | the same (no blink: the glow) |
| a selected piece (`RO_SELECTED`) | a shade lighter every other 8 ticks | an additive glow over its shape (an ADD prim through the white bank) that swells and fades every 32 ticks |
| `zoom` 1 / 2 | ignored | 1: a 1.6x close-up on the player (the PS1's); 2: Talk's framing, the circle `focus_r` around `focus_x/y` fills the viewport (scale 1..3); both glide a third of the way a frame, and back to exactly room_ref's picture at zoom 0 |
| a squashed piece or resident | the weak default's rect runs | one quad, as `plat_image_xform` (section 4) |

The two new 2D modes (`dc2d.h`: `DC2D_GLASS`, `DC2D_ADD`) are generic; an
exact shade over a GLASS prim remaps its texture like an image's (its
colour times the level's factor when the remap finds no room), and an ADD
glow under a shade is dimmed by the same factor, so a selected piece does
not shine through a darkening menu or a fade.

### How it is fast

- **The shell is one texture.** Everything static (the roof, the backdrop,
  the walls, the floor, the lip) is drawn once into a PAL8 canvas with
  room_ref's own pixel code (ported to write bytes; a first, cheap pass
  measures its box, so the canvas is as big as the shell, at most 448 x 400
  with 64 px of margin round the screen for pans and zooms) and uploaded as
  one texture under a fresh uid. Every frame draws it as one sprite. It is
  drawn again only when its key changes: the turned size, origin and wall
  height, the patterns, paints, ceiling ramp, kind, roof, light level and
  tint, and the derived `RoomLight`, `RoomBackdrop` and `RoomRoof` (a lamp
  switched, the door moved, the hearth under the chimney). A pan (the view
  brake's shake) only moves it. The background colour and the stars stay 2D
  rects (they do not pan).
- **A piece is one sprite.** Its texture is keyed by everything its pixels
  depend on (piece, frame and facing entry, paints, light level with the
  dark-lamp / dusk / night bits, the grey / invalid recolour, the tint) in a
  table of 1,024 keys with uids of their own (room_ref's xor of those fields
  can collide, and a uid-keyed cache must not). A key whose texture is still
  in video RAM costs no decode at all (`dc2d_tex_find`); otherwise
  `room_ref_sprite()` decodes it once.
- **The order is room_ref's**, found in O(n^2) instead of O(n^3): the same
  items, the same `behind()` and the same pick (the free item with the
  smallest key, the cycle fallback on the plain depth key), as Kahn's
  algorithm with that priority (`dc_room_sort`). The presorted translucent
  list (section 4) draws the prims in that order. A depth buffer cannot
  replace it: an iso sprite has no per-pixel depth, and the art's overlaps
  follow the footprints.
- Residents are their images' textures (the 2D layer's cache), tinted
  through a remapped copy (`dc2d_tex_remap` with `doomo_tintmap`, index 0
  kept clear); lines are the 2D layer's exact walks.

A view at rest costs the key, the sort and 50-190 prims (about 0.2-0.7 ms
of SH4 time on Flycast); the first frame in a room renders the shell (12-17
ms) and decodes its sprites: the worst tick of the house scripts is 26.9 ms
(home_visit, the script that decodes the most sprites: 115 over the run),
and none passes 33.3 ms (before DC8: 28-50 ms on average, table below). RAM: 277 KB of `.bss` (the canvas 179 KB, the
decode buffer 64 KB, the piece table 12 KB, the `room-parity` scratch 12 KB,
the order's matrix 5 KB); video RAM: the shell's texture (64-256 KB) and one
small texture per piece variant.

### Counters

After each drawn frame the mailbox holds `room_us` (the last view's time,
microseconds), `room_us_max`, `room_us_sum` / `room_n`, `room_shells` (shell
textures rendered) and `room_shell_us_max`, `room_decodes` (piece sprites
decoded) and `room_prims_max`; `dcrun` prints them (`dcrun: house views ...`).
`dc_room_stats` (`room_int.h`) has the same and the sorted items.

### The host test (`dc/tests/gfx2d/test_dc_room.c`)

The host runner's tee (section 4) now draws the house with room.c on the
Dreamcast side and room_ref on the native side (`DC8_ROOM=ref`: room_ref on
both, the route before DC8). `dcroom_test` (120 checks, part of
`make dc-host-test`):

| test | checks |
|---|---|
| parity | all 58 golden frames of `core/tests/room_shells.h` (the classic layout in 4 views and at night, every wallpaper, floor, ceiling ramp, tint, room kind, roof, fence, lamp pool) identical to room_ref |
| catalog | every one of the 349 pieces alone in a room, 4 views by day and one at night (back facings, door and window skins, glow, panes): 1,745 frames identical |
| dynamic | residents (plain, flipped, lifted, tinted), the cursor, the rubber band, half-tile smalls, a jitter, a pan, in 4 views: identical; squashes differ only inside their own quads |
| boxes | no `plat_cache`: the shaded boxes, identical |
| effects | a ghost is one 50% GLASS prim whose pixels are a blend; the invalid ghost is one red (red over twice green, green and blue level); a solid invalid piece is room_ref's picture, untinted and tinted; the blueprint ghost grey; a translucent resident GLASS; the selection an ADD glow over the piece's own texture that pulses and brightens |
| sort | `dc_room_sort` equals room_ref's search on 3,000 random layouts (2,089 of them through the cycle fallback) |
| cache | a view at rest decodes nothing and keeps its shell; the same frame twice gives the same prims, byte for byte; a pan keeps the shell; a new light, a lamp switched off and a turn render a new one (and stay identical to room_ref) |
| zoom | zoom 1 settles at 1.6x with the player's feet under the viewport's centre, `plat_room_project` where the sprite stands; zoom 2 puts Talk's focus at the centre; back at zoom 0 the picture is room_ref's again |
| budget | the busiest room (8x8, 40 pieces, 12 residents, roof, night) in 4 views: identical, 108 prims, 8.5 KB of vertices, nothing dropped |
| maps | no colormap or tintmap entry is index 0 (the canvas's transparent texel) |

With room.c in the tee, DC3's six UI scripts still match the native runner
on every shot (`run_compare.sh`).

### On Flycast (`make -C dc room-test`)

`dc/tests/house/run_flycast.sh`:

1. **The golden frames on the console.** `room-parity LINE NAME` (mailbox
   command 14) makes the game draw golden line LINE with its own
   `plat_room` on a screen of its own; `dcrun` shoots it. All 58 are compared
   with room_ref's pictures from the host test (`DC8_PNG`) by
   `dc/harness/compare.py` at a gate of 100%.
2. **The house scripts** (`core/tests/scripts/home_*.txt`, which needed the
   native runner's `home-room`, `money` and `up` in `dcrun`, mailbox 25 and
   26), tick-exact from the native runner's start: each Flycast shot against
   the host runner's software-PVR picture of the same tick (gated:
   `compare_house.py`, exact >= 99%, within 24/255 >= 99.9%) and against the
   native runner (reported); no prim dropped; no tick over 33.3 ms. Both
   host-side runners run with `--pad dc` (C3: the prompt bars name the
   Dreamcast's buttons, as the console does).

Measured 2026-11-08 (Flycast `e7085e20`, the CDI; times in emulated SH4
time, a guide until a console measures them; "worst" is the game's own
`work_max`):

| script | shots vs host (worst exact) / identical to native | house views / mean | work per tick mean / worst | shells (slowest) | sprites decoded | prims max a view | ticks over 33.3 ms |
|---|---|---|---|---|---|---|---|
| home_rooms | 21 pass (100%) / 21 of 21 | 420 / 0.7 ms | 4.3 / 22.0 ms | 20 (17.2 ms) | 29 | 69 | 0 of 420 |
| home_editor | 32 pass (99.61%) / 20 of 32 | 1,025 / 0.3 ms | 4.5 / 21.1 ms | 9 (15.2 ms) | 38 | 190 | 0 of 1,025 |
| home_flats | 8 pass (100%) / 8 of 8 | 228 / 0.5 ms | 5.2 / 21.4 ms | 5 (15.0 ms) | 21 | 65 | 0 of 228 |
| home_gallery | 7 pass (100%) / 5 of 7 | 310 / 0.4 ms | 3.9 / 20.3 ms | 6 (12.6 ms) | 20 | 61 | 0 of 410 |
| home_haunt | 7 pass (100%) / 6 of 7 | 503 / 0.3 ms | 2.9 / 20.4 ms | 5 (12.6 ms) | 33 | 64 | 0 of 544 |
| home_mart | 16 pass (100%) / 14 of 16 | 148 / 0.4 ms | 5.4 / 20.3 ms | 3 (12.2 ms) | 18 | 59 | 0 of 456 |
| home_tutorial | 9 pass (100%) / 8 of 9 | 303 / 0.2 ms | 3.5 / 20.4 ms | 1 (12.4 ms) | 10 | 64 | 0 of 344 |
| home_visit | 17 pass (99.99%) / 15 of 17 | 527 / 0.4 ms | 3.8 / 26.9 ms | 7 (15.9 ms) | 115 | 74 | 0 of 803 |
| home_defend | 9 pass (99.85%) / 4 of 9 | 1,560 / 0.2 ms | 3.2 / 20.9 ms | 1 (12.4 ms) | 4 | 69 | 0 of 1,594 |

The golden frames: 58 of 58 identical to room_ref on Flycast (every pixel).
The house scripts: 126 of 126 shots pass against the host's software PVR (at
worst 99.6% exact, the rest within 24/255: the PVR rounds a GLASS blend a
level apart). Against the native runner 101 of 126 shots are identical to
the pixel; the others differ where the Dreamcast does its own effects:
translucent ghosts and guests, the selection glow, the photo mode's palette
filters (`plat_pal_lerp`), the shakes and the hell insert's punch
(`plat_post_offset` / `plat_post_scale`, section 4). Before DC8 (the same
scripts on the merge base's CDI):

| script | work per tick mean / worst | ticks over 33.3 ms | prims max / vertex bytes |
|---|---|---|---|
| home_defend | 38.6 / 45.7 ms | 1,560 of 1,593 | 5,907 / 511 KB |
| home_gallery | 28.0 / 42.9 ms | 192 of 410 | 5,835 / 505 KB |
| home_tutorial | 35.1 / 46.4 ms | 303 of 344 | 5,872 / 508 KB |
| home_flats | 49.6 / 56.2 ms | 227 of 227 | 8,332 / 744 KB |

### Owed on hardware (DC8)

- The house's frame time on a real SH4 and PVR, above all the first frame
  in a room (the shell render, 12-17 ms on Flycast, and the sprite decodes):
  `dcrun: house views ...` has the numbers to compare.
- The GLASS and ADD blends on a console's PVR (MODULATEALPHA through a
  palette texture, SRCALPHA / ONE), and point sampling of a sprite drawn at
  1.6x and 3x (the zoom).
- The texture upload of the shell (up to 256 KB through the store queues)
  inside one tick.

## 9. The first-person view (DC6, parts 1 and 2: the world, then the sprites)

```sh
make dc-host-test        # includes dcv3d_test
make -C dc v3d-test      # the same test alone (the dc Makefile needs KOS installed)
make -C dc/tests/gfx2d v3d-test   # ... or straight, host gcc only
```

`plat_view3d` draws the live map as PVR polygons through DC7's 3D batches
(`dc/src/view3d.c`, `dc3d.h`): a segment in the 2D list, so the HUD the core
draws afterwards is on top. The picture follows the reference raycaster
(`core/native/raycast.c`): its projection and integer focal length, the
pitch as a vertical shear (so `combat_hud_project` lands on the polygons),
its light and distance fade, its sky mapping. What the code does:

- Visibility: a fan of rays over the grid with the reference's open window
  as slopes; every cell a ray sees is marked (corner cases mark the cells
  around the corner), then each marked cell emits its floor, ceiling and the
  faces on the edges the eye is inside of. Heights come from `map_floor_z`
  and `map_ceil_z`, so doors and lifts move live.
- Light: a face's texture is a copy remapped through the colormap at the
  brightest corner's level; Gouraud does the fraction down to each corner.
  The distance fade scales with `dc_view3d_set_dist()` (part 3 moves it).
- Sky as a backdrop quad strip at the far depth; views that see no sky get
  the reference's dark stone. Tints use the native tintmap fit
  (`dc3d_tint_fit`, shared with the overworld). A view smaller than the
  screen is cut at its edges (`Dc3dCam.clip`). `DC6_V3D=dc` draws it in the
  host runner. Billboards: see "DC6B" below.
- `dc3d_sort_from` found texture kinds by a linear scan; a view has a texture
  per light level, so it uses a small hash (256 kinds), checked by `test_sort`
  (100 and 300 textures: grouping, and the order left alone above 256).

`dc3d_sort_from` is checked by `test_sort` (100 and 300 textures, 3 per texture: grouping, and the order left alone above 256 kinds).

Measured on the host (software PVR, `dcv3d_test`, 2026-11-13): 8 first-person
maps (Hell Breach 1..7 and the arena), 24 eyes each, 4 headings, level and
looking up 120: 1182 frames, 2x2 block mean within 40/255 per channel of the
native pixel. Level views 79.2 % of pixels (gate 75 %), looking up 80.2 %
(gate 60 %); the red tint 66 %; the busiest view about 780 polygons, about 103 KB of TA (moves a little between runs)
data (budget 576 KB). Polygons against columns will never be pixel equal.

**Known gaps (part 1 is not finished):**
- (Fixed in DC6B: 16 of 788 level frames matched under 45 %, eyes by a map edge that is low: the reference
  draws the solid block past the edge. See "DC6B" below.)
- Flycast (2026-11-13, `dcrun` on the nodebug ELF, emulated time): the first
  run drew only the dark stone backdrop with the billboards on it. The
  backdrop was emitted last; on the console the list is drawn in emission
  order and its far z did not keep it behind the walls (the same family as
  the 2D sprites' depth in DC7). It is now emitted first
  (`test_backdrop_first`). After that, Hell Breach 1 shows the walls, floor,
  ceiling and sky next to the HUD (evidence: `breach_b.png` against the native
  `nat/breach_b.png`). Five levels (`push COMBAT 0, 1, 2, 4, 6`, each walked
  120 ticks) in one script (`levels.txt`): 1056 ticks, 1000 views. The view
  costs 4 ms a tick in the first room (mean 13 ms, 22 ms worst in the big
  levels, 800 polygons). Scene-entry ticks: Hell Breach 1 now 29-31 ms;
  story_invasion's combat entry 31 ms (it was 40.7 ms, over the 33.3 ms
  gate, before the budget below); four of the five levels in `levels.txt`
  still enter at 35-38 ms (the level loads and uploads its textures in that
  tick), and no tick after an entry is over 33.3 ms. A view may make only 4
  new textures (uploads and light-level copies; `V3D_TEX_BUDGET`): the rest
  of a level's first frames use the plain texture darkened by the fit, or the
  flat colour, and fill in over the next frames (`test_remap_budget`: 56 % of
  pixels with no new texture, 84 % with them). The entry tick
  and the governor are DC6C's (below). `make -C dc ui-compare` 94 of 94 (the pause menu's 3D backdrop
  gate went from 45 to 30 % exact: polygons against columns, 36 % exact, 66 %
  close; the Performance page's mask grew by one row for the new Effects
  detail row), `make -C dc room-test` passes, `ow-test` passes except
  `side_sample_a`, which fails the same way (3 of 17 shots, worst 92.9 %) on
  the origin tree without this change. Not run: a Flycast-to-host picture
  compare of the view (the host parity numbers are the only pixel measure)
  and a real console.
- No ticks, memory or PS1/GBA effect: all changes are under `dc/`; the
  tint fit moved from `overworld.c` to `dc3d.c` unchanged.

**DC6B: billboards, the weapon, effects and the HUD (2026-11-13)**

The core hands `plat_view3d` the enemies, pickups and props as `DoomoBillboard`s; the weapon in
the player's hands, the muzzle flash, the hit marks and the HUD are 2D calls (`plat_image`,
`plat_rect`, `plat_text`) that the core makes after the view, so they are drawn by the 2D layer on top
of the view's segment and need nothing from `view3d.c` (the segment is the 2D list's earlier entry).
What part 2 changed in `view3d.c`:

- Billboards use the reference's integer rectangle (`raycast.c draw_sprite`: width and height
  `(int)(size x focal / depth)`, the centre column and the top row truncated), doubled onto the 640x480
  frame, and read their texture a quarter logical pixel back (the reference takes a texel at a pixel's
  left edge, a polygon at its centre). Parity of the sprites' own pixels (the pixels the reference drew
  them on; `test_sprites`): 77.9 % before, 90.5 % now. The colours are the reference's: the sprite
  through the colormap of its whole light level (a remapped copy), `BB_FULLBRIGHT`, `BB_FLIPX`, the
  palette's index 0 transparent, `BB_TRANSLUCENT` and the player's own body close to the camera at 50 %
  alpha (the reference dithers every other pixel), `BB_SHADOW` as the blob.
- The HUD over a sprite is equal to the pixel on both sides (97 % gate, measured 100 %): the 2D list is
  drawn after the segment.
- The map's edge: past it the reference has a solid block of texture 1 (`map_cell_safe`). A ray that went
  over a low border block under the open sky hits it and sees a wall 256 units above the block, so an eye
  by the edge of a sky map sees a wall, with the sky above it. `emit_cell` now draws that wall. This was
  both open items of part 1 (the border-cell picture gap and the sky colour when looking up): host
  parity, level views 79.2 -> 85.6 %, looking up 80.2 -> 90.1 %, level frames under 45 % 16 -> 1 of 788
  (`test_edge_wall`: 19 % without it).
- Texture budget for sprites: a sprite the cache does not have is uploaded inside a per-view budget
  (`V3D_SPR_BUDGET`, 3 uploads or colormap copies); the rest wait a tick (`dc3d_stats.deferred`).
  Nine uploads in one tick were a 38 ms tick (`test_sprite_budget`).
  A colormap copy that does not fit the budget falls back to the fitted flat colour for that tick: the
  sprite's brightness can change for one tick (host: remap budget full 84.4 %, no new copies 56.5 %).
- Visibility casts a ray per 4 logical pixels (`V3D_RAY_PX`, it was 2): the casting was 6-8 ms of the
  13 ms a big view cost on Flycast; host parity moved by less than 1 point (numbers below).
- The tick's budget (`view_budget`): the core's own work before the view varies (the demo bot's plan takes
  16 ms on every 12th tick; the 30 fps budget is 33.3 ms, and the HUD, the PVR hand-off and the sound
  after the view take about 5-7). The view costs about `K x distance` (K from the last views, a decaying
  peak, with a 1.3 safety factor), so `dc3d_be_tick_us()` (the time since `plat_begin_frame`) sets how far
  the rays go this tick: never over the draw distance, never under `DC_V3D_DIST_MIN` (10 cells). The fog
  keeps the draw distance's scale, so only the cut moves, where the light is nearly out
  (`test_tick_budget`). dcrun prints `first-person views cut short by the tick's budget N of M, shortest
  ray distance D cells`. This is a first, small governor; DC6C owns `OPT_DRAW_DIST`, the combat detail
  option and the real governor, and replaces it.

Flycast (2026-11-13, `dcrun` on the nodebug ELF, the seven `dc/harness/scripts/breach_*.txt`, about 1935
ticks each, the demo bot plays; emulated time). Ticks over 33.3 ms of work, before -> after (the base
tree, then this package), with the mean work per tick and the 3D view's build mean:

| script | ticks over 33.3 ms | worst tick | work mean | view build mean |
|---|---|---|---|---|
| factory | 3 -> 0 | 34.2 -> 32.7 ms | 13.5 -> 12.0 ms | 7.4 -> 5.8 ms |
| sewers | 2 -> 0 | 33.7 -> 31.9 ms | 14.2 -> 12.5 ms | 8.2 -> 6.5 ms |
| cathedral | 6 -> 3 | 47.7 -> 40.0 ms | 11.6 -> 10.5 ms | 8.0 -> 6.4 ms |
| graveyard | 38 -> 3 | 46.5 -> 35.3 ms | 17.0 -> 14.3 ms | 11.3 -> 8.5 ms |
| underbelly | 1 -> 0 | 33.8 -> 31.6 ms | 12.8 -> 11.2 ms | 7.0 -> 5.3 ms |
| vape cellar | 0 -> 0 | 23.7 -> 23.0 ms | 8.3 -> 7.5 ms | 3.5 -> 2.7 ms |
| cloud nine | 55 -> 1 | 52.9 -> 36.2 ms | 15.9 -> 13.6 ms | 9.4 -> 7.0 ms |

The seven that are left are not the view: three are a scene's entry tick (the level loads and uploads its
textures: 33.5-36.2 ms, DC6C owns them) and the other four are a bot plan (16 ms of the core's own work on
every 12th tick; the demo bot only) that lands on a tick that also uploads textures for the HUD. The
combat scripts of the sticks tests (`dc/tests/sticks`): 0 of 396 and 0 of 346 ticks over, worst 25.8 and
27.9 ms; the five-level walk (`COMBAT 0, 1, 2, 4, 6`, 955 ticks): 1 -> 0 over, worst 36.4 -> 32.4 ms, view
build mean 13.6 -> 10.5 ms. `make -C dc ui-compare` 94 of 94 as before (the pause menu's 3D backdrop 36 %
exact, 66 % close, as in part 1). Screenshots of the same scripts, the native runner's next to the base
and this package's Dreamcast ones: the package's evidence folder (`DC6B/compare/`, not in the repo; the
native runner is not synced with dcrun, so the bot's moves, and so the scenes, differ between the columns).

Host parity (`make -C dc v3d-test`, 2026-11-13): level views 85.6 % of pixels within 40/255 (gate 80 %),
looking up 90.1 % (gate 85 %), the red tint 66 %; 1 of 788 level frames under 45 % (gate 1 %); the sprites'
own pixels 90.5 % (gate 85 %), the sprite rectangles 79.5 % (gate 72 %, they include the world behind
the sprite), the HUD over them 100 % (gate 97 %); the busiest view about 780 polygons, about 103 KB of TA (moves a little between runs) data.

Memory: no resident RAM anywhere (all under `dc/`; the PS1 and GBA are untouched). Still open: a Flycast-to-host picture compare of the view (the host numbers are the only pixel measure),
a real console, the scene-entry tick (DC6C), the real draw-distance option and governor (DC6C), the
HUD's texture uploads in the same tick as a bot plan.

### Draw distance, Combat detail and the governor (DC6C, on top of DC6B)

`make -C dc` then `sh dc/harness/scripts/gen_detail.sh MODE DIST PLAY > script.txt` and `dcrun` (the script
needs `opt`, a new `dcrun` command: hcmd 31, `opt ID VALUE`, like the native runner's and the GBA's).
The view reads the two rows the PS1 reads: Options > Performance > **Combat detail** (`OPT_3D_FAST`:
Balanced, Smooth, Max detail) and **Draw distance** (`OPT_DRAW_DIST`: Near, Normal). Drawing only; the
game never sees them. `dc/src/view3d.c`, "the governor". It sits above DC6B's tick budget: the governor
picks the distance of the view from the last tick's whole work and the modes' caps (`s_view_dist`, also the
fog's scale); DC6B's `view_budget` then still cuts how far this tick's rays go, from the time the core
already spent before the view (a bot plan). The two work together: the governor is slow and steady, the
budget is the fast cut for a spike.

| | Smooth | Balanced (default) | Max detail |
|---|---|---|---|
| most cells seen | 20 | 32 | 32 |
| ray fan (logical columns a ray) | 4 | 4 (DC6B's) | 2 |
| new textures a view may make | 3 | 4 | 6 |
| view loses 3 cells when the last tick's work is over | 25 ms | 29 ms | 31 ms |
| view gains 2 cells when it is under | 20 ms | 24 ms | 27 ms |

Draw distance Near caps the view at 12 cells under any mode (the PS1's cap). The work is the whole last drawn
tick (`plat_begin_frame` to the end of the draw, what `dcrun` prints as `work:`), fed by `main.c` through
`dc_view3d_work`. The fade is scaled to the distance, so a shorter view ends in the dark and the cut does
not pop. The first view after a scene change (a gap of more than 2 drawn frames) starts at 14 cells and
grows 2 cells a tick while the work has room, so the tick that loads a level is lighter; a view with no work
feed (the host tests) uses the caps only, so Balanced at the default is the same picture as DC6B's.
`dc_gov_step` is the rule, tested alone (`make -C dc v3d-test`, `DC6_GOV_ONLY=1` runs only the governor's
tests, seconds): `test_gov_rule`, `test_gov_caps`, `test_gov_options` (the options reach the view; Smooth's
sparse fan loses no cell), `test_gov_feed` (the entry, the growth, the shortening, the clamps).

Measured on Flycast (2026-11-15, emulated time, `dcrun` on the nodebug ELF of the tree with DC6B and DC6C,
Draw distance Normal). Before = DC6B alone, from its table above.

| script | what | DC6B alone | Balanced | Smooth | Max detail |
|---|---|---|---|---|---|
| `combat` | `COMBAT 0 1 2 4 6` entered fresh, 120 ticks walked (822 ticks) | 0 over, worst 32.4 ms | 0 over, worst 26.8 ms, view mean 10.1 ms | 0 over, worst 26.4, view mean 6.2 | 0 over, worst 28.6, view mean 13.2 |
| `walk` | the seven Hell Breach levels, a pad walks, turns and fires, 900 ticks each (6986 ticks) | | 0 over, worst 27.8 ms, view mean 3.2 | 0 over, worst 22.7, view mean 2.5 | 0 over, worst 31.7, view mean 5.2 |
| `walk`, Draw distance Near | the same | | | | 0 over, worst 22.0, view mean 2.4 |
| `demo` | the seven levels played by the autopilot (`push BREACH 4096+N`, 6989 ticks) | | 3 over, worst 36.2 ms | 1 over, worst 36.3 | 8 over, worst 37.3 |
| `breach_*.txt` (the seven DC6B scripts, Balanced) | the demo bot, 1935 ticks each | 7 over, worst 40.0 | 6 over (0 0 2 3 0 0 1), worst 40.0 | | |

The entry tick of the four big levels is solved at every mode (the `combat` run). Before DC6C alone had
`demo` at 82 ticks over at Balanced and Max detail; with DC6B's budget in the same tree it is 3 and 8. **Gap,
not solved:** Max detail has 8 of 6988 ticks over 33.3 ms in `demo` (worst 37.3 ms, 0.1 %), Balanced 3, Smooth 1;
they are the same tick numbers as DC6B's own (the bot's BFS replan before the view, the level entry at tick
1736, a texture upload for the HUD) and the budget cuts the view to 10 cells there (174-225 views cut) but
the core's own 22 ms leave no room. The pad runs (`walk`, `combat`) have 0 over at every mode and Max detail
has the least room (31.7 ms worst). Spreading the planner's search over several ticks (core, the speed
stream's) would close it.

The picture: Balanced is DC6B's picture (the host parity and the sprite tests below are unchanged: level views 85.6 %,
looking up 90.1 %, sprites' own pixels 90.5 %, 176 checks); Max detail differs only by its denser ray fan
(no visible difference on the host parity); Smooth and Near show a nearer fog (the cut ends in the dark at 20 / 12 cells),
and the first ticks of a level start with a nearer fog that opens up. Evidence: `/tmp/claude-0/ev/DC6C/` (not
in the repo; the scripts are `dc/harness/scripts/gen_detail.sh`). Memory: DC `.bss` +32 B, `.text` +472 B;
no core, GBA or PS1 file changed.

### The first-person misses (DC6D, on top of DC6B and DC6C)

Goal: no tick over 33.3 ms in the seven `breach_*` scripts and the autopilot demo. **Not fully reached:
1 tick of the breach scripts and 1 / 1 / 2 ticks of the demo (Balanced / Smooth / Max detail) stay over, the worst
34.1 ms (0.8 ms over).** Measured on Flycast (emulated time, `dcrun` on the nodebug ELF, same machine, same
scripts, before = origin 17d5e17f, after = this package):

| script | ticks over 33.3 ms | worst tick | work mean | 3D view build mean | views cut by the budget |
|---|---|---|---|---|---|
| factory | 0 -> 0 | 32.7 -> 31.9 ms | 12.0 -> 11.2 ms | 5.8 -> 5.0 ms | 3 -> 145 of 1891 |
| sewers | 0 -> 0 | 31.9 -> 27.8 ms | 12.5 -> 11.6 ms | 6.5 -> 5.5 ms | 2 -> 68 |
| cathedral | 2 -> 0 | 40.0 -> 31.1 ms | 10.4 -> 9.7 ms | 6.4 -> 5.4 ms | 28 -> 77 |
| graveyard | 3 -> 0 | 35.3 -> 29.2 ms | 14.3 -> 13.2 ms | 8.5 -> 7.7 ms | 67 -> 19 |
| underbelly | 0 -> 0 | 31.6 -> 30.8 ms | 11.2 -> 10.7 ms | 5.3 -> 4.8 ms | 0 -> 36 |
| vape cellar | 0 -> 0 | 23.0 -> 22.6 ms | 7.5 -> 7.3 ms | 2.7 -> 2.5 ms | 0 -> 0 |
| cloud nine | 1 -> 1 | 36.0 -> 34.1 ms | 13.5 -> 12.7 ms | 6.9 -> 6.5 ms | 85 -> 15 |
| **seven scripts** | **6 -> 1** | **40.0 -> 34.1 ms** | | | |
| `demo`, Balanced | 3 -> 1 | 36.2 -> 34.0 ms | 12.2 -> 11.6 ms | 5.7 -> 5.4 ms | 176 -> 124 of 6654 |
| `demo`, Smooth | 1 -> 1 | 36.3 -> 34.0 ms | | | 54 of 6639 |
| `demo`, Max detail | 8 -> 2 | 37.3 -> 34.0 ms | | | 768 of 6649 |
| `walk`, Balanced (a pad, 6986 ticks) | 0 -> 0 | 27.8 -> 26.7 ms | 7.2 -> 6.8 ms | 3.2 -> 2.8 ms | 0 -> 0 |
| `walk`, Max detail | 0 -> 0 | 31.7 -> 29.3 ms | 9.3 -> 8.4 ms | 5.2 -> 4.3 ms | 1 -> 221 of 6907 |
| `combat` (`COMBAT 0 1 2 4 6`), Balanced | 0 -> 0 | 26.8 -> 26.7 ms | 14.2 -> 13.4 ms | 10.1 -> 9.1 ms | 0 -> 4 of 754 |

(Raw logs and the breakdowns: `/tmp/claude-0/ev/DC6D/`, not in the repo; the scripts are the same ones: `breach_*.txt`,
`gen_detail.sh MODE 1 demo|walk|combat`.)

Where the time of a late tick goes (a profile build that times the parts, about 0.4 ms slower than the plain
build, so it shows 11 late ticks in the three scripts where the plain build shows 6; ms; ticks are numbered as that
build counts them, dcrun's count is one higher):

| tick | core before the view | the view | after the view | late by |
|---|---|---|---|---|
| cathedral 126 | 13.3 | 8.6 | 11.9 | 0.4 |
| cathedral 216 | 14.0 | 10.4 | 15.9 | 7.0 |
| cathedral 229 | 3.3 | 14.0 | 16.6 | 0.6 |
| graveyard 126, 289, 372, 444, 1728 | 16.0, 16.3, 16.6, 17.1, 16.8 | 8.7, 6.0, 8.1, 7.7, 5.7 | 10.6, 11.1, 9.1, 10.8, 13.0 | 0.1 to 2.3 |
| cloud nine 126, 397, 840 | 22.6, 25.6, 23.0 | 2.9, 2.9, 3.8 (the shortest view) | 11.0, 5.3, 6.8 | 0.2 to 3.2 |

"After the view" is the core's weapon and HUD calls (a weapon picture is composed there, `weld_view_image`:
4-8 ms the tick a new frame of it is needed), the sound (1-3 ms) and the PVR hand-off (`dc2d_end`, 1.2-3.9 ms).
Before the view it is the core: the demo bot's path search on every 12th tick (13-25 ms, the biggest on
cloud nine) and a level's load. The view is the only part the Dreamcast code decides. The budget of DC6B
reserved a fixed 7 ms for "after", so on those ticks it built a view the tick could not pay for.

What changed (`dc/src/view3d.c`, `dc3d.c`):

- **The budget leaves 15 ms for what comes after the view** when the work feed runs (the console and the harness;
  without it, the host tests, 7 ms as before). The distance of the view is cut to what the rest allows, as before.
- **A tick that has no room for even the shortest view (10 cells at 1.3 x the measured cost) does not build one.**
  It shows the segment of the last drawn frame again through a marker (`dc3d_seg_repeat`): the HUD, the weapon and
  the effects are the core's 2D calls and stay live, only the world and its sprites are one tick old. If there
  is nothing to show again (the tick before was a repeat, another level or another viewport) it draws the
  backdrop alone (no walls, no sprites). Never two repeats in a row. dcrun prints
  `first-person views shown again from the last tick N, drawn as the backdrop alone M`.
  Counts: shown again 2 (cathedral), 77 (graveyard) and 128 (cloud nine) of about 1,890 ticks, none in the other four
  scripts, and 3 and 5 backdrops (graveyard, cloud nine); the demo (Balanced) 155 repeats and 4 backdrops of 6,986
  ticks. They are the bot's plan ticks (the bot is only in the demo and the breach scripts); `walk` and `combat` (a
  pad plays) have none.
- **A segment's texture list is a stamp per slot** (`seg_textures` scanned up to 64 entries for every polygon: 2.7 ms
  of a 14 ms view). The same list in the same order; the view's build mean is 6-16 % lower in every script.
- No resident RAM on the GBA and the PS1 (no core, GBA or PS1 file changed). Dreamcast `.bss` +8.5 KB (the stamp table).

Pixels: the repeat is byte for byte the last picture (`test_view_hold`); on the 2 % of the ticks of a demo that
show it (4 to 7 % on graveyard and cloud nine) the world is one tick old, the bot's plan ticks (before, those ticks
ran over 33.3 ms and the whole frame came a vblank late). The
15 ms reserve cuts the ray distance a little more often (the fog scale stays, the cut ends in the dark): Max detail
`walk` 221 of 6907 views, `demo` 768 of 6649, the other modes at most 2 %. Flycast's shots are not byte equal
before and after (18 of 21 breach shots are; the rest differ because the bot and the governor depend on the speed of
the tick: another position or fog scale, not another picture). Evidence pairs: `/tmp/claude-0/ev/DC6D/pics/`.

**Gap, not solved:** the ticks left are 22-23 ms of core before the view (the bot's plan on the first tick of a
level in the demo, a plan on cloud nine) plus 11 ms after it with nothing of the view left to take out (the weapon
picture composed, the sound, `dc2d_end`). They are 0.8 ms over (cloud nine tick 126: 34.1 ms; demo tick 1736, the first tick of a level: 34.0 ms
at every mode; one more tick at Max detail, 34.05 ms). What would close them, none of it in this package: the
planner's search spread over several ticks (core, the speed stream's: it is 13-25 ms of one tick), the weapon
picture composed in slices as on the GBA and the PS1 (`weld_view_image_sliced`, weld.h), or the sound mix of a late tick done in the next
(DC5's `audio.c`, about 2 ms). Not tried: a direct-render submission in `pvr3d.c` (the 120 KB of a repeated view cost
1.1 ms in `dc2d_end`). Real hardware is not measured.

Host: `make -C dc v3d-test` 188 checks (new: `test_view_hold`: no feed never holds, 12 ms builds a cut view, 16 ms
shows the last picture byte for byte, twice in a row the backdrop with no cells, the next idle tick builds,
another level the backdrop), `dc3d_test` 593 (new: `test_seg_textures`, `test_seg_repeat`), parity unchanged
(level views, sprites: the same numbers as DC6B). `ui-compare` 94 of 94; `ow-test` as on origin (`side_sample_a`);
`room-test`'s `home_visit` fails on origin 249a0282 as well (`expect-scene DIALOG`, not this package).

## Talk on the Dreamcast: input, display and the harness (TD2)

TD1 puts the word network on the disc (`dc/src/talkdisc.c`); this package makes the Talk screen playable and
testable on the console. The Talk screen itself is the core's (`core/src/talk_scene.c`, owner TALK).

* **The typed line.** Nothing new was needed in `pad.c`: Talk's "Type your own line..." row opens the core's text
  entry (`SCENE_TEXT_ENTRY`), and `read_keyboards` already feeds every key of a Maple keyboard to
  `ui_text_entry_feed` while it is open. `dcrun --device 2 keyboard` plus the script command `talk-key "words"`
  types a line through the real Maple keyboard path (Flycast keyboard, Shift for capitals); `talk-type` feeds the
  characters the way the native runner does. Both end in the same parser and the same safety gate.
* **The pad fallback.** Without a keyboard the player has the quick replies (one press), catch-the-word (the
  quick reply that picks up the offered topic), the category pages, Options > Topic, and the row "Type your
  own line..." opens the on-screen keyboard, which the pad types on. `dc/tests/talk/talk_pad.txt` plays them.
* **One generator step a tick.** A step of the answer's generator costs 3 to 17 ms on the SH4 and an answer is
  three steps: "Ask about" the offered topic took 26.5 ms in one tick, 35.0 ms with the screen, the one tick over
  33.3 ms the first Talk run showed. `talk_scene_slice(1)` (called nowhere on the GBA; the Dreamcast sets it in
  its start-up) runs the steps one a tick from `talk_update`; the pad is dead meanwhile (1 to 3 ticks per answer). The Dreamcast's
  lines are not the native ones (40 columns against 26 wrap differently, so the shots match the native runner's
  to 98.97 % exact at worst, not 100 %). Unit test `talk_scene_slice_runs_the_answer_one_step_a_tick`.
* **Bubble columns.** `TALK_BUBBLE_COLS` is 40 on the Dreamcast (`core/include/doomo/talk.h`): the dialogue
  box holds 41 columns beside a portrait at 320x240 (250 px of 6 px glyphs). The generator can use lines up to
  3 x 40. The census still runs at 26.
* **HD text is NOT built.** The design's "2x font at native resolution" needs a finer glyph set and a font
  advance the dialogue box can read; there is none (the core has one 5x8 font at a 6 px advance and the Dreamcast
  draws it at 2x). Left open; nothing in Talk waits for it.
* **The harness.** `DC_HC_TALKQ` (`dc/src/talkhc.c`, built with `DOOMO_TALK_DEBUG`, a few KB) answers questions
  about the screen; `dcrun` has the native runner's T8 commands (`talk-enter`, `talk-party`, `talk-read`,
  `talk-quick`, `talk-quick-act`, `talk-say`, `talk-type`, `talk-key`, `talk-page`, `talk-to`, `talk-invite`,
  `talk-leave`, `expect-talk`, `-page`, `-to`, `-row`, `-you`, `-quick`, `-gate`, `-reply-romance`, `-care`,
  `-reply-sincere`) built on real button presses and ticks. Not done: `talk-rows`, `expect-talk-act`, `expect-toast`.
* **On Flycast** (`make -C dc talk-test`: `dc/tests/talk/run_screens.sh`, after TD1's `run_flycast.sh`): each script twice in `dcrun` (a keyboard on
  port 2), every shot identical in both runs, then against the native runner's 320x240 shots by `compare.py`
  (exact >= 98 %, the gate in the script), and no tick over 33.3 ms. Results are in the commit message and the status file.

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

## Talk on the Dreamcast (TD1)

Talk (the word network, the generator, the Talk screen: docs/TALK.md) is built into the Dreamcast game since TD1:
`dc/Makefile` defines `DOOMO_HAS_TALK=1` (so `SCENE_TALK`, the house's `home_talk_enter` and the `TALK` save chunk are
linked), `plat_features()` (`dc/src/pad.c`) reports `PLATF_TALK`, and `dc/src/talkdisc.c` is the platform's database
reader.

* **The file.** `core/tools/talkc.py --bin --lang en` writes `TALK_en.BIN` (902,268 B on this tree, the full bank, not the
  PS1's subset). `mkcdi.sh` puts it on the disc root as `TALK_EN.BIN` (ISO 9660 names are capitals).
* **`plat_talk_load(code)`** opens `/cd/TALK_<CODE>.BIN` through KallistiOS's CD file system, `malloc`s the file size and
  answers `PLAT_TALK_LOADING` while the file is read; asking for the language that is already loaded is free, another
  language frees the first. A missing or short file leaves nothing loaded and returns 0 (Talk then says "not here": the
  same as a boot from the ELF, which has no disc).
* **The disc never holds a tick.** The first version read the whole 0.9 MB in one `fs_read` inside the tick of the first A
  press on a housemate: on Flycast that tick took 7.4 s (a sector takes about 17 ms there). So the file is read in
  one-sector slices (`TALK_CHUNK` 2,048 B) by `dc_talk_pump(us_left)`, which `plat_end_frame` (`main.c`) calls in the idle
  time at the end of each tick: it runs as many slices as the slot has time for and none when the last slice would not
  fit (the longest slice measured on Flycast is 19.3 ms, so a tick with 20 ms or more free takes one; on a faster
  disc, several). Before the first slice the pump assumes 20 ms (`TALK_SLICE_GUESS`), so an overloaded tick (slot 0) never
  pays for it. "The disc never holds a tick" is measured on Flycast only: a real GD-ROM seek after `PACK.DMP` or the FMV
  may take longer than a sector, which the last-slice rule does not foresee.
  Ticks over 33.3 ms because of the disc: 0. The pump starts the English bank by itself 90 ticks (3 s)
  after boot, so it is in about 15 s later on Flycast (a slice a tick) and in about a second on a fast disc, long before
  a player reaches a housemate. A press before that finds Talk "not ready" (`talk_db_open` answers `TALK_DB_LOADING`,
  `home_talk_enter` returns 0) and works a moment later; the house shows no "loading" note for it (a TD2 item if the
  owner wants one).
* **`plat_talk_read(off, dst, len)`** is a bounds-checked `memcpy` out of that block (it answers 0 while the load is
  not finished; `off + len` cannot wrap). Talk reads a row or a string at a time, so a disc read per call would stall a
  frame.
* **Memory.** The bank is heap, not BSS: with the code Talk adds (measured on the tree before DC9C landed: `.text`
  +96,800 B, `.rodata` +11,660 B, `.bss` +5,600 B, `make -C dc sizes` 10,492,376 B in the image before, 10,606,436 B
  after; on the rebased tree 10,628,688 B) the heap above `_end` is 5.80 MB before the bank is loaded and about 4.9 MB
  after (the floor is 4 MB).
* **dcrun and the native start.** `dcrun --native-seed` (the start the native runner gets) reads the bank at once, in
  the harness command, so a script that walks to a housemate within seconds finds Talk as the native runner does
  (`home_visit` does). A script without `--native-seed` gets the console's own behaviour.
* **Tests.** `make -C dc check-talkdisc` (host cc only, in `make -C dc test`): the real `talkdisc.c` and `talk_db.c` over a
  mock of `fs_*` (`dc/tests/talkdisc_host.sh`): opening reads nothing, a read answers 0 while loading, one slice per tick
  and never a read over 2,048 B, a short slot gets no slice and a long one several, the bank is readable after exactly
  `ceil(size / 2048)` slices, a second load is free, the capital name is the only one that opens, reads past the end and
  wrapping offsets return 0, another language replaces the first, the prefetch starts by itself after 90 pumps, and the
  database opens and every bank's CRC verifies over the copy. `make -C dc talk-test` (Flycast, in `make -C dc test` when
  the core is there, `dc/tests/talk/run_flycast.sh`): `talk_disc.txt` on the CDI with the new dcrun command
  `talk-bank open | line | enter | ready` (mailbox command `DC_HC_TALK_BANK`): the bank opens from the disc and verifies
  (4,010 concepts), one greeting is generated from it ("Rise and shine, You! Or just rise. Shining is optional."), the
  real Talk screen opens and the opener is drawn; two runs, byte-identical shots, 152 ticks, 0 over 33.3 ms (worst
  16.7 ms, the dialogue tick). `talk_prefetch.txt`, no `--native-seed`: 900 ticks on the title, the bank arrives by itself
  (slice max 19.2 ms), `talk-bank ready` finds it, 0 of 901 ticks over 33.3 ms (worst 11.7 ms). On the ELF (no disc) the
  first script fails at `talk-bank open`, which is how the test is known to look at the disc.
* **`room-test`.** `home_visit` passes in dcrun (the Talk screen is there) but the host runner (`dc2d_runner`) links no Talk; since the TD2 landing it is not in `room-test`'s list (family-test2 plays it) and `room-test` is green.
* **Not yet (TD2, TD3).** The typed line from the keyboard queue, the pad fallback, HD-text bubbles (40 columns) and
  the `home_talk_*` scripts on dcrun with frame-time gates. The Talk screen's list and the opener's dialogue box overlap
  at 640x480 in the TD1 shots (`talk_disc_opener.png`: the dialogue box sits over the list; the native 320x240 screen
  has the same layout): TD2 owns that layout.
