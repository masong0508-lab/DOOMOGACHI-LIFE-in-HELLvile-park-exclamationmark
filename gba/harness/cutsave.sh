#!/bin/sh
# cutsave.sh ROM HARNESS WORKDIR - S2-10: the GBA flash save is cut at several points, like a power cut or a reset.
#
# A valid save of island A (a lived-in Classic island) sits in slot 1. A second save, of island B (the 58 KB worst
# case), is stopped K video frames after it started (K from the list below: during the first encode pass, which
# writes nothing; just after the trailer sector is erased; mid-stream; at the end of it; after it). Then the flash
# file is read back by a fresh boot. The rule (save_flash.c: the trailer is written last): the slot either holds a
# complete save, A or B, that verifies and loads with the right town, or it reads as empty. Never a torn one.
# K = 100, 300: A is untouched (the first pass writes nothing); from about 40% of the save on (K >= 400 on the tree of
# 2026-11-15, a 15.7 s save) the trailer sector is erased, so the slot is empty until B's trailer lands (the GBA keeps
# one copy: unchanged from before S2-10); K = 1100: B. The run fails when the first cut no longer finds A or the last
# one no longer finds B (the save got longer or shorter: move the CUTS).
rom=$1; harness=$2; work=$3
[ -n "$work" ] || { echo "usage: $0 ROM HARNESS WORKDIR" >&2; exit 2; }
scripts=$(dirname "$0")/scripts
mkdir -p "$work"
rm -f "$work"/prev.sav
"$harness" "$rom" --script "$scripts/cutsave_prev.txt" --save "$work/prev.sav" --quiet > "$work/prev.log" 2>&1 || { echo "cutsave: the previous save failed"; tail -5 "$work/prev.log"; exit 1; }
fail=0
first=""; last=""
for k in ${CUTS:-100 300 400 600 850 1100}; do
    sed "s/@K@/$k/" "$scripts/cutsave_cut.txt.in" > "$work/cut_$k.txt"
    cp "$work/prev.sav" "$work/cut_$k.sav"
    "$harness" "$rom" --script "$work/cut_$k.txt" --save "$work/cut_$k.sav" --quiet > "$work/cut_$k.log" 2>&1
    # what does the slot hold now? A: 4242's predecessor (seed 0x80000001), B: 4242
    if "$harness" "$rom" --script "$scripts/cutsave_exists.txt" --save "$work/cut_$k.sav" --quiet > /dev/null 2>&1; then
        state=""
        for seed in 4242 0x80000001; do
            sed "s/@SEED@/$seed/" "$scripts/cutsave_check.txt.in" > "$work/check_${k}_$seed.txt"
            if "$harness" "$rom" --script "$work/check_${k}_$seed.txt" --save "$work/cut_$k.sav" --quiet > "$work/check_${k}_$seed.log" 2>&1; then
                state="complete save of island $seed"
                break
            fi
        done
        if [ -z "$state" ]; then
            echo "cutsave: K=$k: the slot reads as a save but is NOT a complete A or B (torn)"; fail=1
        else
            echo "cutsave: K=$k: $state"
        fi
    else
        state="empty"
        echo "cutsave: K=$k: the slot reads as empty (never torn)"
    fi
    [ -n "$first" ] || first=$state
    last=$state
done
[ "$first" = "complete save of island 0x80000001" ] || { echo "cutsave: the first cut did not find the previous save"; fail=1; }
[ "$last" = "complete save of island 4242" ] || { echo "cutsave: the last cut did not find the new save"; fail=1; }
exit $fail
