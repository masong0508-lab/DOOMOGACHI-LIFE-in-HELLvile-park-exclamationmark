#!/bin/sh
# fastall.sh OUTDIR [EXE] - fast-movement 3D stress runs (scripts from mkfast.py)
cd "$(dirname "$0")/.." || exit 1
OUT=$1; EXE=${2:-build/doomogachi.exe}
CUE=$(dirname "$EXE")/doomogachi.cue
mkdir -p "$OUT" harness/scripts
python3 harness/mkfast.py harness/scripts/fast_cellar.txt COMBAT 3 --prefix c
python3 harness/mkfast.py harness/scripts/fast_cellar_tp.txt COMBAT 3 --tp --prefix t
python3 harness/mkfast.py harness/scripts/fast_arena.txt COMBAT 5 --prefix a
python3 harness/mkfast.py harness/scripts/fast_cathedral.txt COMBAT 10 --prefix b
for m in cellar cellar_tp arena cathedral; do
    python3 harness/run.py --exe "$EXE" --cue "$CUE" --script harness/scripts/fast_$m.txt \
        --out "$OUT/$m" --timeout 600 > "$OUT/$m.log" 2>&1 &
done
wait
grep -h "fps\|FAIL\|PASS" "$OUT"/*/harness_results.txt
