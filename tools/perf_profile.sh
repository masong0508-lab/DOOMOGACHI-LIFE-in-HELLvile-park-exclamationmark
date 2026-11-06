#!/bin/sh
# perf_profile.sh - where the shared core spends host time over every runner script (gprof).
#   sh tools/perf_profile.sh [N]    top N functions by self time (default 30)
# Host numbers rank the work; the native raster functions (plat_*, tile_*, blit_*) are host-only,
# so look at the core callers (ur_cache_draw, pc_*, *_draw) and their call counts, not at plat_*.
set -e
cd "$(dirname "$0")/.."
N=${1:-30}
B=${TMPDIR:-/tmp}/doomo-pg
rm -rf "$B" "$B-run"; mkdir -p "$B-run"
make -s -j4 -C core runner BUILD="$B" CC="gcc -pg" >/dev/null
n=0
for f in core/tests/scripts/*.txt; do
  n=$((n + 1)); rm -rf "$B-run/sv"; mkdir "$B-run/sv"
  ( cd "$B-run" && "$B/runner" --script "$OLDPWD/$f" --shots sh --saves sv --quiet >/dev/null 2>&1 ) || true
  [ -f "$B-run/gmon.out" ] && mv "$B-run/gmon.out" "$B-run/gmon.$n"
done
gprof -b -s "$B/runner" "$B-run"/gmon.[0-9]* 2>/dev/null
mv gmon.sum "$B-run/" 2>/dev/null || true
gprof -b -p "$B/runner" "$B-run/gmon.sum" 2>/dev/null | head -$((N + 5))
