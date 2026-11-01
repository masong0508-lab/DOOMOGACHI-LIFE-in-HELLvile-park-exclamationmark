# DOOMOGACHI LIFE on the Dreamcast

The Dreamcast port is being built in stages (see the DC packages in the
project plan). This document grows with it. Right now it covers:

1. The toolchain: the sh-elf cross compiler and KallistiOS, installed by
   `dc/tools/setup_kos.sh` at pinned versions.

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

### Known issue: the core under gcc 15

With `kos/environ.sh` sourced, `core/tools/xcheck.sh` compiles the core
with the KallistiOS gcc 15.2.0 instead of apt's gcc 13.2. On 2026-11-01
that leg passed 345 of 346 files: gcc 15 reports a false
`-Wmaybe-uninitialized` on the `EllQ e` quadrant state in
`ellipse_draw()` (`core/src/ui_raster.c`), which is only read when `fast`
is set and is always initialised then. Initialising it (`EllQ e = {0};`)
silences it. The default xcheck leg (apt gcc 13.2, what CI runs) passes.
The `dc/` build will compile the core with gcc 15, so this needs fixing
before or in DC2.

### Troubleshooting

| symptom | fix |
|---|---|
| `missing host tools/headers: makeinfo ...` | install the host packages above (or `--install-deps`) |
| `only N MB free on <prefix>` | the gcc build peaks at about 3.4 GB; free space or use `--prefix` on another disk |
| `kos-chain profile 'stable' builds gcc X, but kos_pins.env pins Y` | the KallistiOS commit moved the profile: update the pins (`--print-profile`) |
| `kos-chain failed: make build-...` | the last lines of each kos-chain log are printed; the full logs stay in `<prefix>/.setup_kos/kos-chain.log` and `<prefix>/kos/utils/kos-chain/logs/`. Rerun to resume |
| `<prefix>/kos has local changes to tracked files` | the KallistiOS tree was edited; commit or discard the edits, or `--force` |
| `sh-elf-gcc --version` says 13.2 after sourcing `environ.sh` | an older shell already had another `sh-elf-gcc` first in `PATH`; open a new shell, or run `$KOS_CC_BASE/bin/sh-elf-gcc` |
