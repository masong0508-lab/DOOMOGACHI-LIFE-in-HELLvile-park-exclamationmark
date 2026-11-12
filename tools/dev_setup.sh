#!/bin/sh
# dev_setup.sh [STAGE...] - install every build tool and emulator the tests use, on a fresh
# Ubuntu 24.04 machine (a new cloud container, a new VM). Owner: ci.
#
#   tools/dev_setup.sh                 every stage, in order (about an hour on 4 cores)
#   tools/dev_setup.sh apt psn00bsdk   only these stages
#   JOBS=3 tools/dev_setup.sh redux    parallel make jobs for the stage (default 2)
#
# Stages (each one is skipped when its result is already there):
#   apt        host packages: GBA compiler + libmgba, MIPS and SH4 compilers, ffmpeg libraries, xvfb, ...
#   psn00bsdk  PSn00bSDK at the commit CI pins -> /opt/psn00bsdk            (PS1 disc; about 3 min)
#   psxavenc   the boot FMV encoder at the commit CI pins -> /usr/local/bin (PS1 disc with the ident)
#   redux      PCSX-Redux + OpenBIOS -> /opt/ps1emu                         (PS1 harness; the long one)
#   kos        KallistiOS + sh-elf gcc -> /opt/toolchains/dc                (Dreamcast build; about 40 min)
#   flycast    the Flycast libretro core -> /opt/toolchains/dc              (Dreamcast harness)
#
# Logs go to $DEV_SETUP_LOGS (default /tmp/dev_setup). The versions are the ones in
# .github/workflows/ci.yml, dc/tools/kos_pins.env and dc/harness/build_flycast.sh; docs/BUILDING.md
# explains each tool. Nothing here is needed to run the core tests (make -C core test).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
JOBS=${JOBS:-2}
LOGS=${DEV_SETUP_LOGS:-/tmp/dev_setup}
SUDO=""; [ "$(id -u)" -eq 0 ] || SUDO=sudo
mkdir -p "$LOGS"

say() { printf 'dev_setup: %s\n' "$*"; }

# The pins live in the CI workflow: read them from there so there is one source of truth.
pin() { sed -n "s/^  $1: *\\([0-9a-f]\\{40\\}\\).*/\\1/p" "$root/.github/workflows/ci.yml" | head -n 1; }

stage_apt() {
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update -q
    # shellcheck disable=SC2086
    $SUDO apt-get install -y -q --no-install-recommends \
        build-essential gcc g++ clang make git curl ca-certificates pkg-config cmake ninja-build meson \
        python3 python3-numpy \
        gcc-arm-none-eabi libnewlib-arm-none-eabi libmgba-dev \
        gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu \
        gcc-sh-elf binutils-sh-elf libnewlib-sh-elf-dev \
        libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libswscale-dev ffmpeg \
        xvfb xauth genisoimage libegl-dev libgl-dev libegl-mesa0 libgl1-mesa-dri \
        libcapstone-dev libfreetype-dev libcurl4-openssl-dev libglfw3-dev libuv1-dev zlib1g-dev \
        gawk patch bzip2 xz-utils tar texinfo flex bison \
        libelf-dev libgmp-dev libmpfr-dev libmpc-dev libpng-dev libjpeg-dev
}

stage_psn00bsdk() {
    prefix=/opt/psn00bsdk
    if [ -f "$prefix/lib/libpsn00b/cmake/sdk.cmake" ] && [ -x "$prefix/bin/mkpsxiso" ]; then
        say "psn00bsdk: already in $prefix"; return 0
    fi
    commit=$(pin PSN00BSDK_COMMIT); [ -n "$commit" ] || { say "psn00bsdk: no pin in ci.yml"; return 1; }
    src=$(mktemp -d)
    git init -q "$src"
    git -C "$src" remote add origin https://github.com/Lameguy64/PSn00bSDK
    git -C "$src" fetch -q --depth 1 origin "$commit"
    git -C "$src" checkout -q FETCH_HEAD
    git -C "$src" submodule update -q --init --recursive --depth 1
    cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKIP_EXAMPLES=ON \
        -DPSN00BSDK_TARGET=mipsel-linux-gnu -DCMAKE_INSTALL_PREFIX="$prefix"
    cmake --build "$src/build" -j "$JOBS"
    $SUDO cmake --install "$src/build"
    rm -rf "$src"
    say "psn00bsdk: installed in $prefix (PSN00BSDK_LIBS=$prefix/lib/libpsn00b)"
}

stage_psxavenc() {
    if command -v psxavenc >/dev/null 2>&1; then say "psxavenc: already on PATH"; return 0; fi
    commit=$(pin PSXAVENC_COMMIT); [ -n "$commit" ] || { say "psxavenc: no pin in ci.yml"; return 1; }
    src=$(mktemp -d)
    git init -q "$src"
    git -C "$src" remote add origin https://github.com/WonderfulToolchain/psxavenc
    git -C "$src" fetch -q --depth 1 origin "$commit"
    git -C "$src" checkout -q FETCH_HEAD
    meson setup "$src/build" "$src" --buildtype=release
    ninja -C "$src/build" -j "$JOBS"
    $SUDO install -D "$src/build/psxavenc" /usr/local/bin/psxavenc
    rm -rf "$src"
    say "psxavenc: installed in /usr/local/bin"
}

stage_redux() {
    dst=/opt/ps1emu
    if [ -x "$dst/pcsx-redux" ] && [ -f "$dst/openbios.bin" ]; then say "redux: already in $dst"; return 0; fi
    $SUDO mkdir -p "$dst"; $SUDO chown "$(id -u)" "$dst"
    src=$dst/src
    if [ ! -d "$src/.git" ]; then
        git clone -q --depth 1 --recursive --shallow-submodules https://github.com/grumpycoders/pcsx-redux "$src"
    fi
    make -C "$src" -j "$JOBS"
    make -C "$src/src/mips/openbios" -j "$JOBS" PREFIX=mipsel-linux-gnu
    ln -sf "$src/pcsx-redux" "$dst/pcsx-redux"
    cp "$src/src/mips/openbios/openbios.bin" "$dst/openbios.bin"
    # the objects are not needed again (about 2 GB); the binary and its resources stay
    find "$src" -name '*.o' -delete 2>/dev/null || true
    say "redux: $dst/pcsx-redux and $dst/openbios.bin (commit $(git -C "$src" rev-parse --short HEAD))"
}

stage_kos() {
    bash "$root/dc/tools/setup_kos.sh" --jobs "$JOBS"
}

stage_flycast() {
    out=/opt/toolchains/dc/flycast_libretro.so
    [ -d /opt/toolchains/dc ] || out=$HOME/dc/flycast_libretro.so
    if [ -f "$out" ]; then say "flycast: already at $out"; return 0; fi
    sh "$root/dc/harness/build_flycast.sh" --jobs "$JOBS"
    mkdir -p "$(dirname "$out")"
    cp "$root/dc/harness/build/flycast_libretro.so" "$out"
    say "flycast: $out"
}

[ $# -gt 0 ] || set -- apt psn00bsdk psxavenc redux kos flycast
rc=0
for s in "$@"; do
    case $s in
        apt|psn00bsdk|psxavenc|redux|kos|flycast) ;;
        *) say "unknown stage: $s (apt psn00bsdk psxavenc redux kos flycast)"; exit 2 ;;
    esac
    t0=$(date +%s)
    say "$s: start (log $LOGS/$s.log)"
    if ( "stage_$s" ) >"$LOGS/$s.log" 2>&1; then
        say "$s: ok in $(( $(date +%s) - t0 )) s"
    else
        say "$s: FAILED after $(( $(date +%s) - t0 )) s; last lines of $LOGS/$s.log:"
        tail -n 25 "$LOGS/$s.log" | sed 's/^/    /'
        rc=1
    fi
done
exit $rc
