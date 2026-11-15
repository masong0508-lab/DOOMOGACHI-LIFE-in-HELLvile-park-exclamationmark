#!/bin/sh
# run_flycast.sh - TD1: Talk's bank on the disc, on Flycast (owner: dreamcast).
#
#   sh dc/tests/talk/run_flycast.sh FLYCAST.so DCRUN CDI NMFILE OUTDIR
#
# Plays dc/tests/talk/talk_disc.txt twice on the CDI (the disc carries TALK_EN.BIN). It passes when both
# runs end with "0 failures" (the pinned core can crash on unload after the script passed: exit 132), the
# game's log shows the bank opened and verified from the disc and one greeting generated from it, and both
# runs' shots are byte-identical. OUTDIR/{1,2}: logs and shots. Exit status 1 on any failure.
set -u
[ $# -eq 5 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN CDI NMFILE OUTDIR" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 OUT=$5
here=$(cd "$(dirname "$0")" && pwd)
bad=0
for n in 1 2; do
    d=$OUT/$n
    rm -rf "$d"; mkdir -p "$d/dc"
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$d/run" \
        --script "$here/talk_disc.txt" --shots "$d/dc" --timeout 600 > "$d/dcrun.log" 2>&1
    rc=$?
    rm -rf "$d/run"
    if [ $rc -eq 132 ] && grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log"; then rc=0; fi
    grep -q '^dcrun: .* 0 failures$' "$d/dcrun.log" || rc=1
    grep -q '^hcmd: talk-bank open 0 verify 0 concepts [1-9]' "$d/dcrun.log" || rc=1
    grep -q '^hcmd: talk-bank line from .*: ..' "$d/dcrun.log" || rc=1
    (cd "$d/dc" && md5sum *.png | sort) > "$d/shots.md5"
    [ -s "$d/shots.md5" ] || rc=1
    [ $rc -eq 0 ] || { echo "talk_disc run $n: FAIL (see $d/dcrun.log)"; bad=1; }
done
cmp -s "$OUT/1/shots.md5" "$OUT/2/shots.md5" || { echo "talk_disc: the two runs' shots differ"; bad=1; }
[ $bad -eq 0 ] && { echo "talk_disc: pass (bank read from the disc, a Talk line generated, shots identical twice)"; grep '^hcmd: talk-bank' "$OUT/1/dcrun.log"; }
exit $bad
