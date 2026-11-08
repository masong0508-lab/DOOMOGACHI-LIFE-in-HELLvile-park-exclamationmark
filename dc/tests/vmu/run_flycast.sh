#!/bin/sh
# run_flycast.sh - DC4's checks on Flycast (owner: dreamcast): the VMU saves
# and the controls of the real game, in dcrun.
#
#   sh dc/tests/vmu/run_flycast.sh FLYCAST.so DCRUN IMAGE NMFILE OUTDIR
#
#   saves      saves1.txt on an empty VMU, then saves2.txt on the same VMU
#              image (the power cycle); vmu_check.py checks the image after
#              each run (FAT, directory, VMS headers and CRCs, icons) and
#              draws the files' icons
#   full       full.txt on a VMU with 50 blocks free (vmu_check.py --fill)
#   novmu      novmu.txt with a keyboard on port 1 and no VMU anywhere
#   console    console1.txt + console2.txt with --console: the first-boot
#              language picker, the config record on the VMU, the second boot
#   input      input_pad.txt and input_alt.txt (stick, analog trigger,
#              keyboard): every shot pair must be the same picture
#   optvol     options_volume.txt (Refresh): the music volume changed 10
#              times in the Options, no frame over 50 ms, the config record
#              written to the VMU once, when the Options are left
# OUTDIR/<check>/ gets the logs, shots and VMU images; OUTDIR/summary.txt one
# line per check. Exit status 1 when one fails.
set -u
[ $# -eq 5 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN IMAGE NMFILE OUTDIR" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 OUT=$5
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
CHECK="python3 -I $root/dc/harness/vmu_check.py"
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0
res() {   # name status detail
    printf '%-8s %s %s\n' "$1" "$2" "$3" | tee -a "$sum"
    [ "$2" = ok ] || bad=1
}
dcrun() {  # dir script [args]
    d=$1; s=$2; shift 2
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --run-dir "$OUT/$d/run" --shots "$OUT/$d/shots" \
        --script "$here/$s" --timeout 600 "$@" >>"$OUT/$d/$s.log" 2>&1
}
vmu() { echo "$OUT/$1/run/system/dc/vmu_save_A1.bin"; }

for d in saves full novmu console input optvol; do rm -rf "$OUT/$d"; mkdir -p "$OUT/$d"; done

# ---- saves: two runs on one VMU ----
dcrun saves saves1.txt --fresh; r1=$?
$CHECK "$(vmu saves)" --expect DOOMO0.SYS --expect DOOMO1.SYS >"$OUT/saves/vmu1.txt" 2>&1; c1=$?
cp "$(vmu saves)" "$OUT/saves/vmu_after_run1.bin" 2>/dev/null
dcrun saves saves2.txt; r2=$?
$CHECK "$(vmu saves)" --expect DOOMO0.SYS --expect DOOMO1.SYS >"$OUT/saves/vmu2.txt" 2>&1; c2=$?
$CHECK "$(vmu saves)" --icon DOOMO1.SYS "$OUT/saves/icon_DOOMO1.png" >/dev/null 2>&1
$CHECK "$(vmu saves)" --icon DOOMO0.SYS "$OUT/saves/icon_DOOMO0.png" >/dev/null 2>&1
w=$(grep -o 'write DOOMO1.SYS [0-9]* blocks -> [^(]*([0-9]* free, [0-9]* us)' "$OUT/saves/saves1.txt.log" | head -1)
st="ok"; [ $r1 -eq 0 ] && [ $c1 -eq 0 ] && [ $r2 -eq 0 ] && [ $c2 -eq 0 ] || st="FAIL"
res saves "$st" "(run1 $r1, vmu $c1, run2 $r2, vmu $c2) $w"

# ---- full ----
mkdir -p "$OUT/full/run/system/dc"
$CHECK "$(vmu full)" --fill 150 >/dev/null 2>&1
dcrun full full.txt; r=$?
$CHECK "$(vmu full)" --expect FILLER.BIN --expect DOOMO0.SYS >"$OUT/full/vmu.txt" 2>&1; c=$?
st="ok"; [ $r -eq 0 ] && [ $c -eq 0 ] || st="FAIL"
res full "$st" "(run $r, vmu $c) $(grep -o 'write DOOMO1.SYS [0-9]* blocks -> VMU full' "$OUT/full/full.txt.log" | head -1)"

# ---- no VMU ----
dcrun novmu novmu.txt --fresh --device 1 keyboard --device 2 none --device 3 none --device 4 none; r=$?
st="ok"; [ $r -eq 0 ] || st="FAIL"
res novmu "$st" "(run $r)"

# ---- first boot as a console ----
dcrun console console1.txt --fresh --console; r1=$?
dcrun console console2.txt --console; r2=$?
$CHECK "$(vmu console)" --expect DOOMOCFG.SYS >"$OUT/console/vmu.txt" 2>&1; c=$?
st="ok"; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ $c -eq 0 ] || st="FAIL"
res console "$st" "(run1 $r1, run2 $r2, vmu $c)"

# ---- input: the same pictures from the stick, the trigger and the keyboard ----
mkdir -p "$OUT/input/pad" "$OUT/input/alt"
"$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$OUT/input/run_pad" \
    --device 2 keyboard --script "$here/input_pad.txt" --shots "$OUT/input/pad" --timeout 600 \
    >"$OUT/input/pad.log" 2>&1; r1=$?
"$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$OUT/input/run_alt" \
    --device 2 keyboard --script "$here/input_alt.txt" --shots "$OUT/input/alt" --timeout 600 \
    >"$OUT/input/alt.log" 2>&1; r2=$?
same=0 diff=0 list=""
for f in "$OUT/input/pad"/*.png; do
    n=$(basename "$f")
    if cmp -s "$f" "$OUT/input/alt/$n"; then same=$((same + 1)); else diff=$((diff + 1)); list="$list $n"; fi
done
st="ok"; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ $diff -eq 0 ] && [ $same -gt 0 ] || st="FAIL"
res input "$st" "(pad $r1, alt $r2) $same shot pairs identical, $diff differ$list"

# ---- Refresh: the Options' volume steps never wait for the VMU ----
dcrun optvol options_volume.txt --fresh; r=$?
n=$(grep -c 'write DOOMOCFG.SYS' "$OUT/optvol/options_volume.txt.log")
st="ok"; [ $r -eq 0 ] && [ "$n" -eq 1 ] || st="FAIL"
res optvol "$st" "(run $r, $n config writes) $(grep -o '^gaps optvol.*' "$OUT/optvol/options_volume.txt.log" | head -1)"

cat "$sum" >/dev/null
[ $bad -eq 0 ]
