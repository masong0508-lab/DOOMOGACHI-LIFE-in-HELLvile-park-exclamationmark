#!/bin/sh
# run_compare.sh RUNNER SCRIPTS_DIR SHOTS_DIR [script ...] - play the UI
# scripts of DC3's verification on the dual-render runner (tee.c) at
# 320x240 and gate every shot: the Dreamcast frame (software PVR, 640x480)
# must match the native frame on at least DC3_GATE % of the logical pixels
# exactly (default 99) and on DC3_CLOSE_GATE % within 16/255 per channel
# (default 99.9). Shots, *_dc.png, *_diff.png and report.txt land in
# SHOTS_DIR/<script>/. (owner: dreamcast)
RUNNER=$1
DIR=$2
SHOTS=$3
shift 3
SCRIPTS=${*:-"ui_gallery fp_title fp_screens creator library_lobby pause_options"}
GATE=${DC3_GATE:-99}
CGATE=${DC3_CLOSE_GATE:-99.9}
fail=0
mkdir -p "$SHOTS"
: > "$SHOTS/report.txt"
for name in $SCRIPTS; do
    s="$DIR/$name.txt"
    out="$SHOTS/$name"
    rm -rf "$out"
    mkdir -p "$out"
    if DC3_REPORT="$out/report.txt" "$RUNNER" --quiet --script "$s" --shots "$out" \
            --saves "$out/saves" --w 320 --h 240 > "$out/log.txt" 2>&1; then
        :
    else
        echo "FAIL  dc3 $name: the runner failed (see $out/log.txt)"
        tail -20 "$out/log.txt"
        fail=1
        continue
    fi
    bad=$(awk -v g="$GATE" -v c="$CGATE" '$1 == "shot" { e = $4; sub("%", "", e); k = $6; sub("%", "", k);
          if (e + 0 < g + 0 || k + 0 < c + 0) print $2 " exact " $4 " close " $6 }' "$out/report.txt")
    sum=$(grep '^summary:' "$out/report.txt")
    cat "$out/report.txt" >> "$SHOTS/report.txt"
    if [ -n "$bad" ]; then
        echo "FAIL  dc3 $name: shots under the gate (exact >= $GATE%, close >= $CGATE%):"
        echo "$bad"
        fail=1
    else
        echo "ok    dc3 $name: $sum"
    fi
done
if [ $fail -ne 0 ]; then echo "dc3 compare: FAILED"; exit 1; fi
echo "dc3 compare: OK"
