#!/bin/sh
# ui_compare.sh - DC3's verification on Flycast (owner: dreamcast): the six
# UI scripts run in dcrun and in the native runner, and compare.py checks
# the Dreamcast shots against the native 320x240 ones (exact >= 99%, the
# expected differences in known_diffs.tsv, each with its reason).
#
#   sh dc/harness/ui_compare.sh FLYCAST.so DCRUN IMAGE NMFILE RUNNER OUTDIR
#
# IMAGE is the CDI (or an ELF without debug sections: Flycast cannot open
# an ELF over 16 MiB). dcrun runs with --native-seed 0x5EED1234 (the native
# runner's default seed): tick-exact waits from tick 0 (--sync) and the
# start state `runner` has. OUTDIR/SCRIPT/{dc,nat,diff}/ get the shots and
# the diff pictures (grey = exact, yellow = close, red = not),
# OUTDIR/SCRIPT.log the dcrun log (with any tick over the 30 fps budget)
# and OUTDIR/summary.txt one block per script: the compare lines, the 2D
# layer's high-water marks (prims and vertex bytes in one frame) and the
# game's work per tick. UI_SCRIPTS="a b" runs other scripts.
# Exit status 1 when a script fails to run or a shot fails its gate.
set -u
[ $# -eq 6 ] || { echo "usage: ui_compare.sh FLYCAST DCRUN IMAGE NMFILE RUNNER OUTDIR" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 RUNNER=$5 OUT=$6
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
SCRIPTS=${UI_SCRIPTS:-"ui_gallery fp_title fp_screens creator library_lobby pause_options"}
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0
for s in $SCRIPTS; do
    src=$root/core/tests/scripts/$s.txt
    for d in dc nat diff saves; do
        mkdir -p "$OUT/$s/$d"
        rm -f "$OUT/$s/$d"/*
    done
    # --fresh empties the run directory's VMU images and saves first
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$OUT/$s/run" \
        --script "$src" --shots "$OUT/$s/dc" --timeout 900 >"$OUT/$s.log" 2>&1
    rc=$?
    # --pad dc: the native runner names the buttons as the Dreamcast build does (C3, input.h btn_name)
    "$RUNNER" --w 320 --h 240 --pad dc --script "$src" --shots "$OUT/$s/nat" --saves "$OUT/$s/saves" \
        >"$OUT/$s.native.log" 2>&1
    nrc=$?
    python3 -I "$here/compare.py" "$OUT/$s/dc" "$OUT/$s/nat" --diff "$OUT/$s/diff" \
        --known "$here/known_diffs.tsv" --script "$s" >"$OUT/$s.compare.txt" 2>&1
    crc=$?
    {
        echo "== $s (dcrun exit $rc, native runner exit $nrc, compare exit $crc)"
        cat "$OUT/$s.compare.txt"
        grep '^FAIL\|^work:\|^dcrun: game drew\|^dcrun: work over' "$OUT/$s.log"
    } >>"$sum"
    if [ $rc -ne 0 ] || [ $nrc -ne 0 ] || [ $crc -ne 0 ]; then bad=1; fi
done
cat "$sum"
n_ok=$(grep -c '^ok \|^known ' "$sum")
n_fail=$(grep -c '^FAIL \|^MISSING ' "$sum")
echo "ui_compare: $n_ok shots pass, $n_fail fail"
[ $bad -eq 0 ] && [ "$n_fail" -eq 0 ]
