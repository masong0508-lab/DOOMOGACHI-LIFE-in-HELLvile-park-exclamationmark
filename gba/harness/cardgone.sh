#!/bin/sh
# cardgone.sh ROM HARNESS WORKDIR - S2-10: the busy card does not stay on a held frame (docs/GBA.md "Speed wave 2:
# S2-10"). scripts/cardgone.txt puts the island hub on screen, which holds its frame in both Mode 4 pages (I7);
# this adds a wait of K ticks (K = 0..7: the hold's cycle is 8 ticks), the fastest clock and an hourly autosave.
# Run 1 finds the autosave's tick; run 2 replays it with a shot in the middle of the save and one at every tick
# from the second after it. cardgone.py wants the card in the first and in none of the others (before the flip copied the card's
# rows back, it stayed on every other picture). A K whose save runs on a tick that builds the next frame (no card,
# the fade only) is skipped; the first K with a card decides.
rom=$1; harness=$2; work=$3
[ -n "$work" ] || { echo "usage: $0 ROM HARNESS WORKDIR" >&2; exit 2; }
dir=$(dirname "$0")
mkdir -p "$work"
for k in 0 1 2 3 4 5 6 7; do
    rm -f "$work"/*.png
    { cat "$dir/scripts/cardgone.txt"; [ $k -gt 0 ] && echo "wait $k"
      printf 'opt 9 4\nopt 8 3\nopt 10 2\nvhash t0\n'; } > "$work/head.txt"
    { cat "$work/head.txt"; echo "wait 400"; } > "$work/find.txt"
    "$harness" "$rom" --script "$work/find.txt" --no-fps-gates --worklog --quiet > "$work/find.log" 2>&1 ||
        { echo "cardgone: run 1 failed"; tail -5 "$work/find.log"; exit 1; }
    t0=$(sed -n 's/.*vhash t0: .*(tick \([0-9]*\)).*/\1/p' "$work/find.log" | tail -1)
    # the autosave: the first tick after t0 with over a second of work
    t=$(awk -v t0="$t0" '/^harness: work / && $3 > t0 && $4 > 1000 { print $3 }' "$work/find.log" | head -1)
    [ -n "$t0" ] && [ -n "$t" ] || { echo "cardgone: no autosave found after tick ${t0:-?}"; exit 1; }
    {
        cat "$work/head.txt"
        echo "wait $((t - t0 - 1))"
        # 60 video frames into the save (sync off: the frames run inside the long tick), then from the tick after
        # the save's own (whose picture may still be on screen when its tick count moves) one shot a tick
        printf 'sync off\nframes 60\nshot cardgone_mid\nwait 1\n'
        i=10
        while [ $i -lt 34 ]; do i=$((i + 1)); echo "wait 1"; echo "shot cardgone_$i"; done
    } > "$work/shots.txt"
    "$harness" "$rom" --script "$work/shots.txt" --shots "$work" --no-fps-gates --quiet > "$work/shots.log" 2>&1 ||
        { echo "cardgone: run 2 failed"; tail -5 "$work/shots.log"; exit 1; }
    python3 "$dir/cardgone.py" "$work/cardgone_mid.png" "$work"/cardgone_[0-9]*.png
    rc=$?
    [ $rc -eq 3 ] && continue
    [ $rc -eq 0 ] && echo "cardgone: K=$k: the card was up during the autosave (tick $t) and is gone from the 24 pictures after it"
    exit $rc
done
echo "cardgone: no K put the autosave on a tick with the card"
exit 1
