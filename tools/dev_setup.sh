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
#   redux      SDL3 + PCSX-Redux + OpenBIOS -> /opt/ps1emu                  (PS1 harness; about 20 min)
#   kos        KallistiOS + sh-elf gcc -> /opt/toolchains/dc                (Dreamcast build; about 40 min)
#   flycast    the Flycast libretro core -> /opt/toolchains/dc              (Dreamcast harness)
#
# Logs go to $DEV_SETUP_LOGS (default /tmp/dev_setup). The versions are the ones in
# .github/workflows/ci.yml, dc/tools/kos_pins.env, dc/harness/build_flycast.sh and the two pins below;
# docs/BUILDING.md explains each tool. Nothing here is needed to run the core tests (make -C core test).
set -eu

# PCSX-Redux moves fast (and its timing model with it): the PS1 numbers in docs/PS1.md were measured on
# the version of about this date. Move the pin on purpose, then re-measure.
REDUX_COMMIT=ee9be68de5a482e7fea8eff7f4b1ddbc0f43b81e   # 2026-10-31
# PCSX-Redux needs SDL3; Ubuntu 24.04 has no libsdl3-dev, so it is built from source.
SDL3_TAG=release-3.4.20

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
JOBS=${JOBS:-2}
LOGS=${DEV_SETUP_LOGS:-/tmp/dev_setup}
SUDO=""; [ "$(id -u)" -eq 0 ] || SUDO=sudo
mkdir -p "$LOGS"

say() { printf 'dev_setup: %s\n' "$*"; }

# The CI pins live in the workflow: read them from there so there is one source of truth.
pin() { sed -n "s/^  $1: *\\([0-9a-f]\\{40\\}\\).*/\\1/p" "$root/.github/workflows/ci.yml" | head -n 1; }

stage_apt() {
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update -q
    $SUDO apt-get install -y -q --no-install-recommends \
        build-essential gcc g++ clang make git curl ca-certificates pkg-config cmake ninja-build meson \
        python3 python3-numpy \
        gcc-arm-none-eabi libnewlib-arm-none-eabi libmgba-dev \
        gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu \
        gcc-sh-elf binutils-sh-elf libnewlib-sh-elf-dev \
        libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libswscale-dev ffmpeg \
        xvfb xauth genisoimage libegl-dev libgl-dev libgles-dev libegl-mesa0 libgl1-mesa-dri \
        libcapstone-dev libfreetype-dev libcurl4-openssl-dev libuv1-dev zlib1g-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libasound2-dev libudev-dev libdbus-1-dev \
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
    if [ -x "$dst/pcsx-redux" ] && [ -f "$dst/openbios.bin" ] \
       && [ "$(cat "$dst/redux.commit" 2>/dev/null)" = "$REDUX_COMMIT" ]; then
        say "redux: already in $dst"; return 0
    fi
    if ! pkg-config --exists sdl3; then
        s=$(mktemp -d)
        git clone -q --depth 1 --branch "$SDL3_TAG" https://github.com/libsdl-org/SDL "$s"
        cmake -S "$s" -B "$s/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
            -DSDL_WAYLAND=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
        cmake --build "$s/build" -j "$JOBS"
        $SUDO cmake --install "$s/build"
        $SUDO ldconfig
        rm -rf "$s"
        pkg-config --exists sdl3
    fi
    $SUDO mkdir -p "$dst"; $SUDO chown "$(id -u)" "$dst"
    src=$dst/src
    if [ "$(git -C "$src" rev-parse -q --verify HEAD 2>/dev/null)" != "$REDUX_COMMIT" ]; then
        rm -rf "$src"
        git init -q "$src"
        git -C "$src" remote add origin https://github.com/grumpycoders/pcsx-redux
        git -C "$src" fetch -q --depth 1 origin "$REDUX_COMMIT"
        git -C "$src" checkout -q FETCH_HEAD
        git -C "$src" submodule update -q --init --recursive --depth 1
    fi
    make -C "$src" -j "$JOBS"
    # mipsel-linux-gnu binutils call the little-endian MIPS ELF format "tradlittlemips"
    make -C "$src/src/mips/openbios" -j "$JOBS" PREFIX=mipsel-linux-gnu FORMAT=elf32-tradlittlemips
    test -x "$src/pcsx-redux"
    ln -sf "$src/pcsx-redux" "$dst/pcsx-redux"
    cp "$src/src/mips/openbios/openbios.bin" "$dst/openbios.bin"
    echo "$REDUX_COMMIT" > "$dst/redux.commit"
    # the objects are not needed again (about 2 GB); the binary and its resources stay
    find "$src" -name '*.o' -delete 2>/dev/null || true
    say "redux: $dst/pcsx-redux and $dst/openbios.bin (commit $REDUX_COMMIT)"
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

# One stage in this process, so that `set -e` stops it at the first failing command (a function
# called from an `if` would run with errexit off).
if [ "${1:-}" = --run-stage ]; then
    "stage_$2"
    exit 0
fi

[ $# -gt 0 ] || set -- apt psn00bsdk psxavenc redux kos flycast
rc=0
for s in "$@"; do
    case $s in
        apt|psn00bsdk|psxavenc|redux|kos|flycast) ;;
        *) say "unknown stage: $s (apt psn00bsdk psxavenc redux kos flycast)"; exit 2 ;;
    esac
    t0=$(date +%s)
    say "$s: start (log $LOGS/$s.log)"
    src_rc=0
    JOBS="$JOBS" sh "$0" --run-stage "$s" >"$LOGS/$s.log" 2>&1 || src_rc=$?
    if [ "$src_rc" -eq 0 ]; then
        say "$s: ok in $(( $(date +%s) - t0 )) s"
    else
        say "$s: FAILED (exit $src_rc) after $(( $(date +%s) - t0 )) s; last lines of $LOGS/$s.log:"
        tail -n 25 "$LOGS/$s.log" | sed 's/^/    /'
        rc=1
    fi
done
exit $rc
