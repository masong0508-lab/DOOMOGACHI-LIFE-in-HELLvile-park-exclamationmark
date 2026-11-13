#!/bin/sh
# run_tier.sh IRQ BRIDGE OUT_DIR - DC5b's host gate (owner: dreamcast): every
# song plays on both tiers (test_tier.c). The IRQ build compares its stream
# with the DoomoVoice reference render sample by sample (ref maxdiff <= 1, no
# driver under-run: test_tier exits 1 otherwise); the pair must also play the
# same notes: the driver's key-on count after the run is equal.
IRQ=$1
BRIDGE=$2
OUT=$3
TICKS=${DC5B_TICKS:-300}
mkdir -p "$OUT"
fail=0
"$IRQ" list > "$OUT/songs.txt" || exit 1
n=0
while read -r id len name; do
    n=$((n + 1))
    t=$TICKS
    [ "$len" -gt 0 ] 2>/dev/null && [ "$len" -lt "$t" ] && t=$len
    a=$("$IRQ" song "$id" "$t" "$OUT/irq.raw" "$OUT/irq.keys") || { echo "FAIL  dc5b song $id $name: $a"; fail=1; continue; }
    b=$("$BRIDGE" song "$id" "$t" "$OUT/br.raw" "$OUT/br.keys") || { echo "FAIL  dc5b song $id $name (bridge)"; fail=1; continue; }
    ka=$(tail -1 "$OUT/irq.keys"); kb=$(tail -1 "$OUT/br.keys")
    if [ "$ka" != "$kb" ]; then echo "FAIL  dc5b song $id $name: key-ons irq $ka bridge $kb"; fail=1; fi
    echo "$a" >> "$OUT/report.txt"
done < "$OUT/songs.txt"
rm -f "$OUT/irq.raw" "$OUT/br.raw"
[ $fail = 0 ] && echo "dc5b tiers: ok ($n songs)"
exit $fail
