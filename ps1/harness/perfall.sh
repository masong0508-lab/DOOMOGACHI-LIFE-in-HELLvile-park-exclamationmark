#!/bin/sh
# perfall.sh OUTDIR [names...] - run the perf_*.txt scripts on the game disc
# (ps1/build) in parallel and print one "name fps" line per measurement.
# Run from the repo root after a game build.
out=$1; shift
[ $# -gt 0 ] || set -- concert island dialogue title creator pet exorcism
mkdir -p "$out"
for n in "$@"; do
  python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue ps1/build/doomogachi.cue \
    --script ps1/harness/scripts/${PFX:-perf}_$n.txt --out "$out/$n" --timeout 600 > "$out/$n.log" 2>&1 &
done
wait
for n in "$@"; do
  grep -i "fps" "$out/$n.log" | sed "s/^/$n: /"
done
