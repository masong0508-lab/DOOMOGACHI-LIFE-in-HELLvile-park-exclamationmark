#!/bin/sh
# speed2.sh OUTDIR [groups...] - speed tour 2 (SA2) on the PS1 game disc (ps1/build), run from the repo
# root after a game build. Each gba/harness/scripts/speed2_GROUP.txt is turned into a run.py script by
# tools/speed2.py (every window: fps / fpsend, so the vsyncs-per-frame histogram), played in PCSX-Redux,
# and printed as a table (fps, frames by vsyncs, frames over 33 / 50 ms, the worst frame, the overlay
# loads before the window). PROF=1 samples the PC instead (prof / profend; harness/prof.py reads
# OUT/GROUP/prof_*.txt). JOBS=N runs N groups at once (default 1: Redux is CPU-bound and timing is
# taken from the game's own vsync count, so a busy host slows the run but does not change the numbers).
out=$1; shift
[ -n "$out" ] || { echo "usage: $0 OUTDIR [groups...]" >&2; exit 2; }
[ $# -gt 0 ] || set -- $(ls gba/harness/scripts/speed2_*.txt | sed 's/.*speed2_\(.*\)\.txt/\1/')
mkdir -p "$out"
jobs=${JOBS:-1}
rc=0
run_one() {
  g=$1
  python3 tools/speed2.py ps1 gba/harness/scripts/speed2_$g.txt ${PROF:+--prof} > "$out/speed2_$g.txt" || return 1
  python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue ps1/build/doomogachi.cue \
    --script "$out/speed2_$g.txt" --out "$out/$g" --timeout 1800 > "$out/$g.log" 2>&1
}
n=0
for g in "$@"; do
  run_one "$g" &
  n=$((n + 1))
  if [ $n -ge "$jobs" ]; then wait; n=0; fi
done
wait
for g in "$@"; do
  echo "### $g ($out/$g/tty.log)"
  grep -q "FAIL" "$out/$g.log" && { echo "run.py FAILED:"; grep "FAIL" "$out/$g.log"; rc=1; }
  [ -n "$PROF" ] || python3 tools/speed2.py table "$out/$g/tty.log" || rc=1
done
exit $rc
