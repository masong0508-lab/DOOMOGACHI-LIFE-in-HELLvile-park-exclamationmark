#!/bin/sh
# run_scripts.sh RUNNER SCRIPTS_DIR SHOTS_DIR - play every runner script
# at 240x160 (GBA) and 320x240 (PS1). Screenshots land in
# SHOTS_DIR/<w>x<h>/<script>/. A script whose first line is
# "# assert-no-stubs" also fails if any weak stub ran. (owner: foundation)
RUNNER=$1
DIR=$2
SHOTS=$3
fail=0
n=0
for s in "$DIR"/*.txt; do
    [ -f "$s" ] || continue
    name=$(basename "$s" .txt)
    extra=""
    if head -n 1 "$s" | grep -q "assert-no-stubs"; then extra="--assert-no-stubs"; fi
    for res in 240x160 320x240; do
        w=${res%x*}
        h=${res#*x}
        out="$SHOTS/$res/$name"
        rm -rf "$out"
        mkdir -p "$out"
        if "$RUNNER" --quiet --script "$s" --shots "$out" --saves "$out/saves" \
                --w "$w" --h "$h" $extra > "$out/log.txt" 2>&1; then
            echo "ok    script $name @ $res"
        else
            echo "FAIL  script $name @ $res (see $out/log.txt)"
            cat "$out/log.txt" | grep -v "^shot:" | tail -20
            fail=1
        fi
        n=$((n + 1))
    done
done
if [ $fail -ne 0 ]; then echo "scripts: FAILED"; exit 1; fi
echo "scripts: $n runs OK"
