#!/bin/sh
# run_flycast.sh - DC8's checks on Flycast (owner: dreamcast): the house
# (plat_room on the PVR, dc/src/room.c) in dcrun.
#
#   sh dc/tests/house/run_flycast.sh FLYCAST.so DCRUN IMAGE NMFILE RUNNER HOSTRUN ROOMTEST OUTDIR
#
# 1. parity: every golden frame of core/tests/room_shells.h (58 frames: the
#    classic layout in 4 views and at night, every wallpaper, floor,
#    ceiling ramp, tint, room kind, roof, fence and lamp pool) drawn by the
#    game's plat_room on Flycast (`room-parity LINE NAME`, mailbox 14), and
#    by room_ref.c in the host test (ROOMTEST = dc/build/host/dcroom_test,
#    DC8_PNG): compare.py, every pixel identical (gate 100 %).
# 2. the house scripts (core/tests/scripts/home_*.txt, or $HOUSE_SCRIPTS):
#    dcrun     the script on IMAGE, tick-exact from the native runner's start
#              (--native-seed); it must pass, and drop no prim
#    host      the same script in the host build of the Dreamcast layers
#              (HOSTRUN = dc/build/host/dc2d_runner: room.c through the
#              software PVR); compare_house.py gates every Flycast shot
#              against it (exact >= 99 %, within 24/255 >= 99.9 %)
#    native    the same script in the native runner (room_ref.c): reported
#              per shot (the Dreamcast's own effects differ by design)
#    timing    the game's work per tick (dcrun): no tick over 33.3 ms (the
#              30 fps budget), and the house view's own time (mailbox room_*)
# OUTDIR/parity/{dc,nat}, OUTDIR/<script>/{dc,host,nat}, the logs, and
# OUTDIR/summary.txt. Exit status 1 when anything fails.
set -u
[ $# -eq 8 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN IMAGE NMFILE RUNNER HOSTRUN ROOMTEST OUTDIR" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 RUNNER=$5 HOSTRUN=$6 ROOMTEST=$7 OUT=$8
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
SCRIPTS=${HOUSE_SCRIPTS:-"home_rooms home_editor home_flats home_gallery home_haunt home_mart home_tutorial home_visit home_defend"}
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0

# the pinned Flycast core can crash while it unloads, after the script has
# passed (SIGILL from its DEBUGBREAK; DC7): the script's own verdict then
verdict() {
    if [ "$1" -eq 132 ] && grep -q '^dcrun: .* 0 failures$' "$2"; then echo 0; else echo "$1"; fi
}

# ---- 1. the golden frames ------------------------------------------------------
p=$OUT/parity
rm -rf "$p"
mkdir -p "$p/dc" "$p/nat" "$p/host"
DC8_PNG="$p/host" "$ROOMTEST" > "$p/dcroom_test.log" 2>&1
trc=$?
lines=0
for f in "$p"/host/room_*_nat.png; do
    [ -f "$f" ] || continue
    n=$(basename "$f" _nat.png)
    mv "$f" "$p/nat/$n.png"
    lines=$((lines + 1))
done
{
    echo "# DC8: the room_ref golden frames drawn by the game's plat_room (generated)"
    echo "wait 10"
    i=0
    while [ $i -lt $lines ]; do echo "room-parity $i room_$i"; i=$((i + 1)); done
} > "$p/parity.txt"
"$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --run-dir "$p/run" --script "$p/parity.txt" \
    --shots "$p/dc" --timeout 1200 > "$p/dcrun.log" 2>&1
prc=$(verdict $? "$p/dcrun.log")
rm -rf "$p/run"
python3 -I "$root/dc/harness/compare.py" "$p/dc" "$p/nat" --gate 100 --close-gate 100 --diff "$p/diff" \
    > "$p/compare.txt" 2>&1
pcrc=$?
nshots=$(ls "$p/dc" 2>/dev/null | grep -c '\.png$')
st=ok
[ $trc -eq 0 ] && [ $prc -eq 0 ] && [ $pcrc -eq 0 ] && [ "$lines" -gt 0 ] && [ "$nshots" -eq "$lines" ] || st=FAIL
{
    echo "== parity: $st ($lines golden frames, $nshots Flycast shots; host test $trc, dcrun $prc, compare $pcrc)"
    tail -1 "$p/compare.txt"
    grep '^dcroom tests' "$p/dcroom_test.log"
} >> "$sum"
[ "$st" = ok ] || bad=1

# ---- 2. the house scripts ------------------------------------------------------
for s in $SCRIPTS; do
    src=$root/core/tests/scripts/$s.txt
    [ -f "$src" ] || src=$here/$s.txt
    d=$OUT/$s
    rm -rf "$d"
    mkdir -p "$d/dc" "$d/nat" "$d/host"
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$d/run" \
        --script "$src" --shots "$d/dc" --timeout 1200 > "$d/dcrun.log" 2>&1
    rc=$(verdict $? "$d/dcrun.log")
    # --pad dc: both runners name the buttons as the Dreamcast build does (C3)
    "$RUNNER" --w 320 --h 240 --pad dc --script "$src" --shots "$d/nat" --saves "$d/saves" > "$d/native.log" 2>&1
    nrc=$?
    DC3_PNG=1 DC3_REPORT="$d/host/report.txt" "$HOSTRUN" --w 320 --h 240 --pad dc --script "$src" \
        --shots "$d/host" --saves "$d/hsaves" > "$d/host.log" 2>&1
    hrc=$?
    rm -rf "$d/run" "$d/saves" "$d/hsaves"
    python3 -I "$here/compare_house.py" "$d/dc" "$d/host" "$d/nat" > "$d/compare.txt" 2>&1
    crc=$?
    drop2=$(grep -o 'dropped [0-9]*, texture' "$d/dcrun.log" | grep -o '[0-9]*' | head -1)
    over=$(sed -n 's/^dcrun: work over [0-9]* us on \([0-9]*\) of .*/\1/p' "$d/dcrun.log")
    st=ok
    [ $rc -eq 0 ] && [ $nrc -eq 0 ] && [ $hrc -eq 0 ] && [ $crc -eq 0 ] || st=FAIL
    [ "${drop2:-0}" = 0 ] || st=FAIL
    tnote=
    if [ -z "$over" ] || [ "$over" -gt 0 ]; then
        st=FAIL
        tnote=" (ticks over 33.3 ms: ${over:-no timing line})"
    fi
    {
        echo "== $s: $st (dcrun $rc, native runner $nrc, host runner $hrc, compare $crc)$tnote"
        tail -1 "$d/compare.txt"
        grep '^dcrun: game drew\|^dcrun: house views\|^dcrun: work over' "$d/dcrun.log"
        grep '^FAIL\|fail' "$d/dcrun.log" | grep -v ' 0 failures' | head -5
    } >> "$sum"
    [ "$st" = ok ] || bad=1
done
cat "$sum"
exit $bad
