#!/bin/sh
# Build the Flycast libretro core used by the headless Dreamcast harness.
#
#   dc/harness/build_flycast.sh [--jobs N] [--clean]
#
# Fetches libretro/flycast at the pinned commit (git, shallow) into
# dc/harness/build/flycast and builds flycast_libretro.so there (OpenGL
# renderer, no OpenMP so frames repeat). Output:
#   dc/harness/build/flycast_libretro.so
# The source and object trees are deleted afterwards unless --keep is given
# (about 15 MB stays). Running it again with the .so present is a no-op.
set -eu

FLYCAST_REPO=https://github.com/libretro/flycast.git
# 2026-10-31 "Vulkan: fix the 32-bit builds"
FLYCAST_COMMIT=e7085e20fe8d81257e11aea5f19a9c1ca2b1bd55

here=$(cd "$(dirname "$0")" && pwd)
build=$here/build
src=$build/flycast
out=$build/flycast_libretro.so
stamp=$build/flycast_libretro.commit
jobs=2
keep=0
while [ $# -gt 0 ]; do
    case $1 in
        --jobs) jobs=$2; shift 2 ;;
        --keep) keep=1; shift ;;
        --clean) rm -rf "$src" "$out" "$stamp"; exit 0 ;;
        *) echo "usage: $0 [--jobs N] [--keep] [--clean]" >&2; exit 2 ;;
    esac
done

if [ -f "$out" ] && [ "$(cat "$stamp" 2>/dev/null)" = "$FLYCAST_COMMIT" ]; then
    echo "flycast_libretro.so is up to date ($FLYCAST_COMMIT)"
    exit 0
fi

free_mb=$(df -Pm "$here" | awk 'NR==2 {print $4}')
if [ "$free_mb" -lt 1500 ]; then
    echo "only $free_mb MB free; the flycast build needs about 600 MB (plus margin)" >&2
    exit 1
fi

mkdir -p "$build"
if [ ! -d "$src/.git" ]; then
    git init -q "$src"
    git -C "$src" remote add origin "$FLYCAST_REPO"
fi
if [ "$(git -C "$src" rev-parse -q --verify HEAD 2>/dev/null)" != "$FLYCAST_COMMIT" ]; then
    git -C "$src" fetch -q --depth 1 origin "$FLYCAST_COMMIT"
    git -C "$src" checkout -q --force FETCH_HEAD
fi

t0=$(date +%s)
# HAVE_OPENMP=0: texture decoding in one thread, so runs are repeatable.
make -C "$src" -j"$jobs" platform=unix HAVE_OPENMP=0 HAVE_LTCG=0 >"$build/flycast_build.log" 2>&1 || {
    tail -n 40 "$build/flycast_build.log" >&2
    echo "flycast build failed; full log: $build/flycast_build.log" >&2
    exit 1
}
t1=$(date +%s)
cp "$src/flycast_libretro.so" "$out"
echo "$FLYCAST_COMMIT" >"$stamp"
peak=$(du -sm "$src" | cut -f1)
echo "built $out in $((t1 - t0)) s; source+objects $peak MB"
if [ "$keep" = 0 ]; then
    rm -rf "$src"
fi
