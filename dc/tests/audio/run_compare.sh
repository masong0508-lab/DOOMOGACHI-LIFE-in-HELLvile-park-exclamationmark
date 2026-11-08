#!/bin/sh
# run_compare.sh RUNNER SCRIPTS_DIR OUT_DIR [script ...] - DC5's host
# comparison (owner: dreamcast): each runner script plays once on
# dcsnd_runner (sndtee.c), which writes musrender's WAV (the native
# reference) and the Dreamcast mixer's WAV from the same plat_tone /
# plat_sample calls; compare.py then gates the pair: envelope r >= 0.9
# (DC5's bar) and the same note in >= DCSND_PITCH_GATE of the loud 200 ms
# windows (default 0.9). The two must line up at lag 0 (same ticks). WAVs,
# envelope plots and report.txt land in OUT_DIR/<script>/.
# Default scripts: the title, a story chapter's overworld, a combat level and
# the invaded town (the loudest cue: the soft knee at work).
RUNNER=$1
DIR=$2
OUT=$3
shift 3
SCRIPTS=${*:-"fp_title story_ch1 action_combat story_invasion"}
PGATE=${DCSND_PITCH_GATE:-0.9}
here=$(cd "$(dirname "$0")" && pwd)
python3 -I -c 'import numpy' 2>/dev/null || {
    echo "FAIL  dc5 compare: compare.py needs numpy (Debian/Ubuntu: sudo apt install python3-numpy)"; exit 1; }
fail=0
mkdir -p "$OUT"
: > "$OUT/report.txt"
for name in $SCRIPTS; do
    s="$DIR/$name.txt"
    o="$OUT/$name"
    rm -rf "$o"
    mkdir -p "$o"
    if DCSND_WAV="$o/dc.wav" DCSND_STATS="$o/stats.txt" "$RUNNER" --quiet --script "$s" \
            --shots "$o/shots" --saves "$o/saves" --w 320 --h 240 --wav "$o/ref.wav" \
            > "$o/log.txt" 2>&1; then
        :
    else
        echo "FAIL  dc5 $name: the runner failed (see $o/log.txt)"
        tail -20 "$o/log.txt"
        fail=1
        continue
    fi
    rm -rf "$o/shots" "$o/saves"
    line=$(python3 -I "$here/compare.py" "$o/ref.wav" "$o/dc.wav" --label "$name" --max-lag 0.1 \
           --min-pitch "$PGATE" --plot "$o/envelope.png")
    st=$?
    echo "$line   [$(cat "$o/stats.txt")]" | tee -a "$OUT/report.txt"
    case "$line" in *"lag +0 ms"*) ;; *) echo "FAIL  dc5 $name: not at lag 0"; fail=1 ;; esac
    grep -q "underruns 0 overruns 0 dropped 0" "$o/stats.txt" || { echo "FAIL  dc5 $name: queue trouble"; fail=1; }
    [ $st -eq 0 ] || fail=1
done
[ $fail -eq 0 ] && echo "dc5 host compare: ok ($SCRIPTS)"
exit $fail
