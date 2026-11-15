#!/bin/sh
# run_flycast.sh - DC9A: the story, free-play and side-mission script families on Flycast (owner: dreamcast).
#
#   sh dc/tests/families/run_flycast.sh FLYCAST.so DCRUN IMAGE NMFILE OUTDIR [script ...]
#
# Every core/tests/scripts/{story,fp,side}_*.txt (FAMILY_GLOBS overrides the patterns: DC9B's family-test2; or the named scripts) is played twice in dcrun on the CDI,
# tick-exact from the native runner's start (--native-seed). A script passes when
#   - both runs end with dcrun's "0 failures" (the pinned Flycast core can crash while it unloads, after the
#     script has passed: SIGILL, exit 132; the script's own verdict then counts),
#   - the two runs are identical: the same ticks, frames, per-tick work summary, 3D / 2D counters and the
#     same bytes in every shot (md5 of each PNG).
# The ticks over 33.3 ms are listed, not gated: the known ones are loading ticks (a new island or story, the
# VMU save: docs/DREAMCAST.md "Frame time of the story, free play and side missions").
# dc/tests/families/scripts/NAME.txt is played in place of (or in addition to) the core script of that name.
# OUTDIR/<script>/{1,2}/ the logs and shots, OUTDIR/summary.txt one line per script. Exit status 1 when one fails.
set -u
[ $# -ge 5 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN IMAGE NMFILE OUTDIR [script ...]" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 OUT=$5; shift 5
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
if [ $# -gt 0 ]; then SCRIPTS=$*
else
    SCRIPTS=$(cd "$root/core/tests/scripts" && ls ${FAMILY_GLOBS:-story_*.txt fp_*.txt side_*.txt} | sed 's/\.txt$//')
    # DC9B: scripts of our own (a core script that needs something the Dreamcast does not have yet, written for it)
    [ -n "${FAMILY_GLOBS:-}" ] && SCRIPTS="$SCRIPTS $(cd "$here/scripts" 2>/dev/null && ls *.txt 2>/dev/null | sed 's/\.txt$//')"
fi
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0
script_path() { if [ -f "$here/scripts/$1.txt" ]; then echo "$here/scripts/$1.txt"; else echo "$root/core/tests/scripts/$1.txt"; fi; }
play() {                                   # play SCRIPT N: runs it into OUT/SCRIPT/N, prints the exit status
    d=$OUT/$1/$2
    rm -rf "$d"; mkdir -p "$d/dc"
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$d/run" \
        --script "$(script_path "$1")" --shots "$d/dc" --timeout 2400 > "$d/dcrun.log" 2>&1
    rc=$?
    rm -rf "$d/run"
    if [ $rc -eq 132 ] && grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log"; then rc=0; fi
    grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log" || rc=1
    # what must repeat: everything but the wall-clock seconds
    grep -E '^(dcrun: (game drew|work over|3D views|[0-9]+ video)|work: tick)' "$d/dcrun.log" | sed 's/, [0-9.]* s,/,/' > "$d/result.txt"
    (cd "$d/dc" && md5sum *.png 2>/dev/null | sort) >> "$d/result.txt"
    return $rc
}
for s in $SCRIPTS; do
    play "$s" 1; r1=$?
    play "$s" 2; r2=$?
    st=pass
    cmp -s "$OUT/$s/1/result.txt" "$OUT/$s/2/result.txt" || st=DIFFERENT
    [ $r1 -eq 0 ] && [ $r2 -eq 0 ] || st=FAIL
    over=$(sed -n 's/^dcrun: work over [0-9]* us on \([0-9]*\) of \([0-9]*\) ticks; worst \([0-9]*\) us.*/\1 of \2 ticks over 33.3 ms, worst \3 us/p' "$OUT/$s/1/dcrun.log")
    shots=$(ls "$OUT/$s/1/dc" | grep -c '\.png$')
    echo "$s: $st (runs $r1 $r2), $shots shots, ${over:-no timing line}" >> "$sum"
    [ "$st" = pass ] || bad=1
done
cat "$sum"
exit $bad
