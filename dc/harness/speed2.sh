#!/bin/sh
# speed2.sh OUTDIR [groups...] - speed tour 2 (SA2) on the Dreamcast build in Flycast, run from the repo root
# after `make -C dc` (the ELF, build/doomogachi-nodebug.elf and build/doomogachi.nm) and `make -C dc/harness`.
# Each gba/harness/scripts/speed2_GROUP.txt is turned into a dcrun script by tools/speed2.py (every window:
# `log speed2: begin N NAME`, mark-ticks, `frames 2N`, expect-ticks, `log speed2: end NAME`) and printed as a
# table: logic ticks/s over the window, ticks over 33.3 ms and over 50 ms of work (dcrun prints every tick
# over the 30 fps budget), the worst one. The run's last lines give the worst tick of the whole group.
# FLYCAST=path overrides the core (default: dc/harness/build/flycast_libretro.so, else the toolchain's).
out=$1; shift
[ -n "$out" ] || { echo "usage: $0 OUTDIR [groups...]" >&2; exit 2; }
[ $# -gt 0 ] || set -- $(ls gba/harness/scripts/speed2_*.txt | sed 's/.*speed2_\(.*\)\.txt/\1/')
core=${FLYCAST:-$(ls dc/harness/build/flycast_libretro.so /opt/toolchains/dc/flycast_libretro.so 2>/dev/null | head -n 1)}
[ -n "$core" ] || { echo "$0: no Flycast core (make -C dc/harness flycast)" >&2; exit 2; }
mkdir -p "$out"
rc=0
for g in "$@"; do
  python3 tools/speed2.py dc gba/harness/scripts/speed2_$g.txt > "$out/speed2_$g.txt" || exit 1
  dc/harness/build/dcrun "$core" dc/build/doomogachi-nodebug.elf --nm dc/build/doomogachi.nm --fresh \
    --run-dir "$out/run" --script "$out/speed2_$g.txt" --shots "$out/$g" --timeout 1800 > "$out/$g.log" 2>&1
  echo "### $g ($out/$g.log)"
  # the pinned core may crash while unloading after the script (dc/harness/README.md "Limits"): the
  # verdict is dcrun's "N failures" line
  grep -q ", 0 failures" "$out/$g.log" || { echo "dcrun FAILED"; tail -n 3 "$out/$g.log"; rc=1; }
  python3 tools/speed2.py table "$out/$g.log" || rc=1
  grep -E "^dcrun: (game drew|work over)" "$out/$g.log"
done
exit $rc
