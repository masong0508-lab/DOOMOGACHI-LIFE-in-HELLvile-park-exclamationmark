#!/bin/sh
# run_screens.sh - TD2: Talk on the Dreamcast, on Flycast against the native runner (owner: dreamcast).
#
#   sh dc/tests/talk/run_screens.sh FLYCAST.so DCRUN IMAGE NMFILE RUNNER OUTDIR [script ...]
#
# talk_pad, talk_typed and the four home_talk_* (or the named scripts; TD3) are played
#   - twice in dcrun on the CDI, tick-exact from the native runner's start (--native-seed), with a Maple
#     keyboard on port 2 (--device 2 keyboard; the pad on port 1 is the player): both runs must end with
#     "0 failures", and every shot must be byte for byte the same in both;
#   - once in the native runner at 320x240 (talk-key is played there as talk-type: same words), and
#     compare.py checks each Dreamcast shot against the native one (exact >= 98 %: the Dreamcast has 40-column lines, so a list behind a box can differ a little).
# The game's frame time is gated: no logic tick over 33.3 ms in the Talk screen.
# OUTDIR/<script>/{1,2,nat,diff}/ the shots, OUTDIR/summary.txt one line per script. Exit status 1 on any failure.
set -u
[ $# -ge 6 ] || { echo "usage: run_screens.sh FLYCAST DCRUN IMAGE NMFILE RUNNER OUTDIR [script ...]" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 RUNNER=$5 OUT=$6; shift 6
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
if [ $# -gt 0 ]; then SCRIPTS=$*; else SCRIPTS='talk_pad talk_typed home_talk_basic home_talk_group home_talk_safety home_talk_visit'; fi
mkdir -p "$OUT"
# The same scenarios run on native and Dreamcast. Their startup waits
# and screenshot ticks differ, but the gameplay and expected outcomes may not.
# Catch drift before the expensive Flycast runs. home_talk_basic intentionally
# includes additional platform-specific assertions and is checked separately.
strip_parity_ops() {
    awk '
        /^[[:space:]]*($|#)/ { next }
        /^wait[[:space:]]/ { next }
        /^shot[[:space:]]/ { next }
        /^talk-ready([[:space:]]|$)/ { next }
        { gsub(/\r$/, ""); print }
    ' "$1"
}
for parity in home_talk_group home_talk_safety home_talk_visit; do
    strip_parity_ops "$root/core/tests/scripts/$parity.txt" > "$OUT/${parity}_native.ops"
    strip_parity_ops "$here/$parity.txt" > "$OUT/${parity}_dreamcast.ops"
    if ! cmp -s "$OUT/${parity}_native.ops" "$OUT/${parity}_dreamcast.ops"; then
        echo "Talk journey $parity diverged (native vs Dreamcast):" >&2
        diff -u "$OUT/${parity}_native.ops" "$OUT/${parity}_dreamcast.ops" >&2 || :
        exit 1
    fi
done
sum=$OUT/summary.txt
: > "$sum"
bad=0
play() {                                   # play SCRIPT N: dcrun into OUT/SCRIPT/N
    d=$OUT/$1/$2
    rm -rf "$d"; mkdir -p "$d/dc"
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --device 2 keyboard --run-dir "$d/run" \
        --script "$here/$1.txt" --shots "$d/dc" --timeout 2400 > "$d/dcrun.log" 2>&1
    rc=$?
    rm -rf "$d/run"
    if [ $rc -eq 132 ] && grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log"; then rc=0; fi
    grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log" || rc=1
    (cd "$d/dc" && md5sum *.png 2>/dev/null | sort) > "$d/result.txt"
    return $rc
}
for s in $SCRIPTS; do
    play "$s" 1; r1=$?
    play "$s" 2; r2=$?
    st=pass
    cmp -s "$OUT/$s/1/result.txt" "$OUT/$s/2/result.txt" || st=DIFFERENT
    [ $r1 -eq 0 ] && [ $r2 -eq 0 ] || st=FAIL
    # the native twin
    n=$OUT/$s/nat; rm -rf "$n" "$OUT/$s/diff"; mkdir -p "$n" "$OUT/$s/diff"
    wt=$(sed -n 's/^talk-ready: \([0-9]*\) ticks$/\1/p' "$OUT/$s/1/dcrun.log" | head -1)   # the ticks the disc took: the title art moves with them
    sed -e 's/^talk-key /talk-type /' -e "s/^talk-ready.*/wait ${wt:-0}/" "$here/$s.txt" > "$OUT/$s/native.txt"
    "$RUNNER" --w 320 --h 240 --pad dc --script "$OUT/$s/native.txt" --shots "$n" --saves "$OUT/$s/saves" > "$OUT/$s/native.log" 2>&1 || { st=FAIL; echo "native runner failed: $s" >&2; }
    python3 -I "$root/dc/harness/compare.py" "$OUT/$s/1/dc" "$n" --diff "$OUT/$s/diff" --gate 98 --close-gate 98 > "$OUT/$s/compare.txt" 2>&1 || st=FAIL
    over=$(sed -n 's/^dcrun: work over [0-9]* us on \([0-9]*\) of \([0-9]*\) ticks; worst \([0-9]*\) us.*/\1 of \2 ticks over 33.3 ms, worst \3 us/p' "$OUT/$s/1/dcrun.log")
    case "$over" in 0\ of*) ;; *) st=FAIL ;; esac
    shots=$(ls "$OUT/$s/1/dc" | grep -c '\.png$')
    low=$(awk '/^(ok|FAIL)/ {v=$4; gsub("%","",v); if (m==""||v+0<m+0) m=v} END {print m}' "$OUT/$s/compare.txt")
    echo "$s: $st (runs $r1 $r2), $shots shots, lowest exact $low %, ${over:-no timing line}" >> "$sum"
    [ "$st" = pass ] || bad=1
done
cat "$sum"
exit $bad
