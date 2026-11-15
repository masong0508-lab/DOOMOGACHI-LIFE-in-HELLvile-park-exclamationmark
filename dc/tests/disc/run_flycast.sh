#!/bin/sh
# run_flycast.sh - DC9C: the disc image, the boot FMV and the VMU saves end to end on Flycast
# (owner: dreamcast).
#
#   sh dc/tests/disc/run_flycast.sh FLYCAST.so DCRUN IMAGE NMFILE OUTDIR
#
# IMAGE is the CDI (make -C dc disc) and must carry INTRO.DMV. Every check is played twice and
# the two runs must be identical (the same log numbers and the same bytes in every shot):
#   boot    boot.txt: the disc boots to the title without the movie (the scripted default); the
#           video frames to the title are printed
#   intro   tests/fmv/intro.txt with --fmv: INTRO.DMV plays from the disc, 152 frames shown and
#           none skipped, sound on, and the game boots to the title after it
#   skip    tests/fmv/skip.txt: START skips the movie, the game boots on
#   e2e     e2e1.txt then e2e2.txt on the same VMU image (a restart in between): a new free-play island
#           saved through the title, island menu and Save / Load screen, vmu_check.py happy with the
#           image, the title's Continue loads it, and `load 1` gives the crc e2e1 printed
#   old     the VMU image of the build before DC9C (tests/disc/old_saves_vmu.bin: a story and a
#           free-play save made by DC4's code) loads: tests/vmu/saves2.txt on it
# OUTDIR/<check>/{1,2}/ the logs and shots, OUTDIR/summary.txt one line per check with the numbers
# (disc size, boot and movie times, save and load times). Exit status 1 when one fails.
set -u
[ $# -eq 5 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN IMAGE NMFILE OUTDIR" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 OUT=$5
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
CHECK="python3 -I $root/dc/harness/vmu_check.py"
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0

# run SCRIPT into DIR with the given dcrun args; the exit status (a Flycast crash while it unloads,
# SIGILL 132, after a passed script counts as the script's own verdict)
dc() {
    d=$1; s=$2; shift 2
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --run-dir "$d/run" --shots "$d/shots" --script "$s" \
        --timeout 900 "$@" >>"$d/dcrun.log" 2>&1
    rc=$?
    if [ $rc -eq 132 ] && tail -3 "$d/dcrun.log" | grep -q ' 0 failures'; then rc=0; fi
    return $rc
}
# what must repeat between the two runs: the numbers of the log and the bytes of the shots
result() {
    d=$1
    grep -aE '^(DC: (fmv|vmu)|town-crc|frames |dcrun: (game drew|work over|[0-9]+ video))' "$d"/dcrun.log | \
        sed 's/, [0-9.]* s,/,/' > "$d/result.txt"
    (cd "$d/shots" 2>/dev/null && md5sum *.png 2>/dev/null | sort) >> "$d/result.txt"
}
fin() {   # name status detail
    printf '%-6s %s %s\n' "$1" "$2" "$3" | tee -a "$sum"
    [ "$2" = ok ] || bad=1
}
same() { cmp -s "$OUT/$1/1/result.txt" "$OUT/$1/2/result.txt"; }

for c in boot intro skip e2e old; do rm -rf "$OUT/$c"; mkdir -p "$OUT/$c/1" "$OUT/$c/2"; done

# ---- the disc ----
cdi_size=$(wc -c < "$IMG")
intro_size=$(wc -c < "$here/../../build/disc/cd/INTRO.DMV" 2>/dev/null || echo 0)
echo "disc: $(basename "$IMG") $cdi_size bytes, INTRO.DMV $intro_size bytes" | tee -a "$sum"

# ---- boot, twice ----
for n in 1 2; do dc "$OUT/boot/$n" "$here/boot.txt" --fresh; eval "r$n=\$?"; result "$OUT/boot/$n"; done
st=ok; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && same boot || st=FAIL
fin boot $st "(runs $r1 $r2) $(grep -ah 'title_no_fmv' "$OUT/boot/1/dcrun.log" | head -1)"

# ---- the FMV, twice ----
for n in 1 2; do dc "$OUT/intro/$n" "$root/dc/tests/fmv/intro.txt" --fresh --fmv; eval "r$n=\$?"; result "$OUT/intro/$n"; done
st=ok; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && same intro || st=FAIL
fin intro $st "(runs $r1 $r2) $(grep -ah 'DC: fmv: done' "$OUT/intro/1/dcrun.log" | head -1) | $(grep -ah 'fmv_done\|title_after_fmv' "$OUT/intro/1/dcrun.log" | tr '\n' ' ')"

# ---- skipping it, twice ----
for n in 1 2; do dc "$OUT/skip/$n" "$root/dc/tests/fmv/skip.txt" --fresh --fmv; eval "r$n=\$?"; result "$OUT/skip/$n"; done
st=ok; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && same skip || st=FAIL
fin skip $st "(runs $r1 $r2) $(grep -ah 'DC: fmv: skipped' "$OUT/skip/1/dcrun.log" | head -1)"

# ---- a game saved and loaded after a restart, twice ----
for n in 1 2; do
    d=$OUT/e2e/$n
    dc "$d" "$here/e2e1.txt" --fresh --native-seed 0x5EED1234; ra=$?
    $CHECK "$d/run/system/dc/vmu_save_A1.bin" --expect DOOMO1.SYS > "$d/vmu_check.txt" 2>&1; rv=$?
    crc=$(grep -ao 'town-crc 0x[0-9a-f]*' "$d/dcrun.log" | tail -1 | cut -d' ' -f2)
    sed "s/@CRC@/$crc/" "$here/e2e2.txt" > "$d/e2e2.txt"
    dc "$d" "$d/e2e2.txt" --native-seed 0x5EED1234; rb=$?
    result "$d"
    eval "r$n=\$((ra + rv + rb))"
    [ -n "$crc" ] || eval "r$n=9"
done
st=ok; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && same e2e || st=FAIL
d=$OUT/e2e/1
fin e2e $st "(runs $r1 $r2) $(grep -ah 'write DOOMO1.SYS' "$d/dcrun.log" | head -1 | sed 's/DC: vmu A1 //'); $(grep -ah 'vmu read DOOMO1.SYS' "$d/dcrun.log" | head -1 | sed 's/DC: vmu //')"

# ---- the saves of the build before this one load ----
for n in 1 2; do
    d=$OUT/old/$n
    mkdir -p "$d/run/system/dc"
    cp "$here/old_saves_vmu.bin" "$d/run/system/dc/vmu_save_A1.bin"
    dc "$d" "$root/dc/tests/vmu/saves2.txt"; eval "r$n=\$?"
    result "$d"
done
st=ok; [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && same old || st=FAIL
fin old $st "(runs $r1 $r2) $(grep -ah 'DC: vmu A1: .* free play saved' "$OUT/old/1/dcrun.log" | head -1 | sed 's/DC: vmu //')"
exit $bad
