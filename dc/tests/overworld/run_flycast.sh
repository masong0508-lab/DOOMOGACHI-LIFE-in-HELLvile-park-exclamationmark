#!/bin/sh
# run_flycast.sh - DC7's checks on Flycast (owner: dreamcast): the overworld
# (plat_overworld on the PVR) in the story scripts, in dcrun.
#
#   sh dc/tests/overworld/run_flycast.sh FLYCAST.so DCRUN IMAGE NMFILE RUNNER HOSTRUN OUTDIR
#
# For each script (story_ch1, story_ch2, story_invasion, fp_island,
# side_sample_a from core/tests/scripts; hellvile and hellvile_night here):
#   dcrun     the script on the CDI, tick-exact from the native runner's
#             start (--native-seed); it must pass (its expect-* lines), the
#             3D views must drop no polygon and the 2D no prim
#   native    the same script in the native runner (the top-down reference
#             shots, for the contact sheet; not compared pixel by pixel:
#             the Dreamcast draws the town in 3D)
#   host      the same script in the host build of the Dreamcast layers
#             (HOSTRUN = dc/build/host/dc2d_runner, DC7_OW=dc): the same
#             polygons through the software PVR. compare3d.py checks every
#             Flycast shot against it: at least 97% of the pixels within
#             24/255 per channel (the PVR's and the software rasteriser's
#             edge rules and colour rounding differ; a wrong camera, depth
#             test, texture or colour fails it by far); a Flycast shot
#             without a host shot fails, and every script but fp_island
#             (the 2D island hub) must have at least one shot with a view
#   timing    the game's work per tick (dcrun: worst, ticks over 33.3 ms)
#             and the 3D views' build time (mailbox g3d_*), from the run;
#             no tick may pass 33.3 ms (the 30 Hz budget), except fp_island's
#             one known loading tick (making the island, 2D only)
# OUTDIR/<script>/{dc,nat,host}/ the shots, OUTDIR/<script>/*.log the logs,
# OUTDIR/summary.txt one block per script. Exit status 1 when one fails.
set -u
[ $# -eq 7 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN IMAGE NMFILE RUNNER HOSTRUN OUTDIR" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 RUNNER=$5 HOSTRUN=$6 OUT=$7
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
SCRIPTS=${OW_SCRIPTS:-"story_ch1 story_ch2 story_invasion fp_island side_sample_a hellvile hellvile_night"}
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0
for s in $SCRIPTS; do
    src=$root/core/tests/scripts/$s.txt
    [ -f "$src" ] || src=$here/$s.txt
    d=$OUT/$s
    rm -rf "$d"
    mkdir -p "$d/dc" "$d/nat" "$d/host"
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$d/run" \
        --script "$src" --shots "$d/dc" --timeout 1200 > "$d/dcrun.log" 2>&1
    rc=$?
    # the pinned Flycast core can crash while it unloads, after the script
    # has passed (SIGILL from its DEBUGBREAK; it does so on the merge base's
    # CDI too): the script's own verdict is the "N failures" line
    note=
    if [ $rc -eq 132 ] && grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log"; then
        rc=0
        note=" (Flycast crashed while unloading, after the script passed)"
    fi
    "$RUNNER" --w 320 --h 240 --script "$src" --shots "$d/nat" --saves "$d/saves" > "$d/native.log" 2>&1
    nrc=$?
    rm -f "$d/host/report.txt"
    DC7_OW=dc DC3_PNG=1 DC3_REPORT="$d/host/report.txt" "$HOSTRUN" --w 320 --h 240 --script "$src" \
        --shots "$d/host" --saves "$d/hsaves" > "$d/host.log" 2>&1
    hrc=$?
    rm -rf "$d/run" "$d/saves" "$d/hsaves"
    # the shots with a 3D view each script must have, and the ticks over
    # the 30 Hz budget it may have (fp_island: the 2D hub, and its known
    # loading tick when the island is made)
    case $s in
        fp_island) mingated=0 overmax=1 ;;
        *) mingated=1 overmax=0 ;;
    esac
    python3 -I "$here/compare3d.py" "$d/dc" "$d/host" --min-gated $mingated > "$d/compare.txt" 2>&1
    crc=$?
    drop3=$(grep -o 'polygons left out [0-9]*' "$d/dcrun.log" | grep -o '[0-9]*$')
    drop2=$(grep -o 'dropped [0-9]*, texture' "$d/dcrun.log" | grep -o '[0-9]*' | head -1)
    over=$(sed -n 's/^dcrun: work over [0-9]* us on \([0-9]*\) of .*/\1/p' "$d/dcrun.log")
    st=ok
    [ $rc -eq 0 ] && [ $nrc -eq 0 ] && [ $hrc -eq 0 ] && [ $crc -eq 0 ] || st=FAIL
    [ "${drop3:-0}" = 0 ] && [ "${drop2:-0}" = 0 ] || st=FAIL
    tnote=
    if [ -z "$over" ] || [ "$over" -gt $overmax ]; then
        st=FAIL
        tnote=" (ticks over 33.3 ms: ${over:-no timing line}, allowed $overmax)"
    fi
    {
        echo "== $s: $st (dcrun $rc, native runner $nrc, host runner $hrc, compare $crc)$note$tnote"
        tail -1 "$d/compare.txt"
        grep '^dcrun: game drew\|^dcrun: 3D views\|^dcrun: work over' "$d/dcrun.log"
        grep '^FAIL\|fail' "$d/dcrun.log" | head -5
    } >> "$sum"
    [ "$st" = ok ] || bad=1
done
cat "$sum"
exit $bad
