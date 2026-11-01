#!/bin/sh
# run_journeys.sh RUNNER JOURNEYS_DIR OUT_DIR - the journey and tap meter
# (M0, docs/GAME_DESIGN.md "UX budgets"). Plays every JOURNEYS_DIR/*.txt
# at 240x160 (GBA) and 320x240 (PS1) with JOURNEYS_DIR/budgets.tsv, prints
# one table row per journey (presses, ticks, menu depth, screens, typed
# letters, the budget) and fails when
#   * a script fails (a journey over its budget fails in the runner),
#   * a journey takes a different number of presses at the two sizes,
#   * a journey has no budget row, or a budget row (not "-") no journey,
#   * the meter's own checks fail: j1_boot_menu replayed against a copy of
#     the budgets with its row lowered by one press must fail, and so must
#     a journey that uses a setup command (scene) between journey-begin and
#     journey-end (a teleport past presses the meter would not count).
# Logs and shots: OUT_DIR/<w>x<h>/<journey>/. (owner: M0)
RUNNER=$1
DIR=$2
OUT=$3
BUD="$DIR/budgets.tsv"
fail=0
mkdir -p "$OUT"
res_all="$OUT/journeys.tsv"
: > "$res_all"
for s in "$DIR"/*.txt; do
    [ -f "$s" ] || continue
    name=$(basename "$s" .txt)
    for res in 240x160 320x240; do
        w=${res%x*}
        h=${res#*x}
        o="$OUT/$res/$name"
        rm -rf "$o"
        mkdir -p "$o"
        if "$RUNNER" --quiet --script "$s" --shots "$o" --saves "$o/saves" \
                --w "$w" --h "$h" --budgets "$BUD" > "$o/log.txt" 2>&1; then
            :
        else
            echo "FAIL  journey script $name @ $res (see $o/log.txt)"
            grep -v "^shot:" "$o/log.txt" | tail -10
            fail=1
        fi
        grep "^journey: " "$o/log.txt" | sed "s/^journey: /$res /" >> "$res_all"
    done
done
# the table, the press parity between sizes, rows without journeys
awk -v budfile="$BUD" '
BEGIN {
    while ((getline line < budfile) > 0) {
        if (line ~ /^#/ || line ~ /^[ \t]*$/) continue
        n = split(line, f, /[ \t]+/)   # blanks or tabs, as the runner reads it
        bp[f[1]] = f[2]; bt[f[1]] = f[3]; order[++nb] = f[1]
    }
}
{
    res = $1; nm = $2
    for (i = 3; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
    key = nm SUBSEP res
    P[key] = v["presses"]; T[key] = v["ticks"]; D[key] = v["depth"]; S[key] = v["screens"]; C[key] = v["typed"]
    seen[nm] = 1
}
END {
    bad = 0
    printf "%-20s %8s %8s %6s %6s %6s %8s %8s  %s\n", "journey", "presses", "ticks", "depth", "screens", "typed", "budget", "b.ticks", "(240x160 | 320x240 presses)"
    for (k = 1; k <= nb; k++) {
        nm = order[k]
        a = nm SUBSEP "240x160"; b = nm SUBSEP "320x240"
        if (bp[nm] == "-") {
            if (nm in seen) { print "FAIL  journey " nm " has a reserved (-) budget row"; bad = 1 }
            else printf "%-20s %8s %8s %6s %6s %6s %8s %8s  reserved\n", nm, "-", "-", "-", "-", "-", "-", "-"
            continue
        }
        if (!(nm in seen)) { print "FAIL  budget row " nm " has no journey"; bad = 1; continue }
        printf "%-20s %8s %8s %6s %6s %6s %8s %8s  %s | %s\n", nm, P[a], T[a], D[a], S[a], C[a], bp[nm], bt[nm], P[a], P[b]
        if (P[a] != P[b]) { print "FAIL  journey " nm ": " P[a] " presses at 240x160 but " P[b] " at 320x240"; bad = 1 }
    }
    for (nm in seen) if (!(nm in bp)) { print "FAIL  journey " nm " has no budget row in budgets.tsv"; bad = 1 }
    exit bad
}' "$res_all" || fail=1
# the meter checks itself: one press under today's j1 must fail
neg="$OUT/neg"
rm -rf "$neg"
mkdir -p "$neg"
awk 'BEGIN { FS = OFS = "\t" } $1 == "j1_boot_menu" { for (i = 2; i <= NF && $i == ""; i++) {} $i = $i - 1 } { print }' \
    "$BUD" > "$neg/budgets.tsv"
if "$RUNNER" --quiet --script "$DIR/j1_boot_menu.txt" --shots "$neg" --saves "$neg/saves" \
        --budgets "$neg/budgets.tsv" > "$neg/log.txt" 2>&1; then
    echo "FAIL  the meter passed j1_boot_menu with its budget one press lower"
    fail=1
elif ! grep -q "over budget" "$neg/log.txt"; then
    echo "FAIL  the lowered j1_boot_menu budget failed for another reason (see $neg/log.txt)"
    fail=1
fi
printf 'journey-begin j1_boot_menu\nscene TITLE\njourney-end j1_boot_menu\n' > "$neg/teleport.txt"
if "$RUNNER" --quiet --script "$neg/teleport.txt" --shots "$neg" --saves "$neg/saves" \
        --budgets "$BUD" > "$neg/teleport.log" 2>&1; then
    echo "FAIL  the meter passed a journey that teleports with 'scene' inside it"
    fail=1
elif ! grep -q "setup command inside a journey" "$neg/teleport.log"; then
    echo "FAIL  the teleporting journey failed for another reason (see $neg/teleport.log)"
    fail=1
fi
if [ $fail -ne 0 ]; then echo "journeys: FAILED"; exit 1; fi
echo "journeys: $(($(grep -c . "$res_all") / 2)) journeys OK at 240x160 and 320x240 (budgets $BUD)"
