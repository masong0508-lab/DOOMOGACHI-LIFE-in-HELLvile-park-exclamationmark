#!/bin/sh
# ram_ledger.sh - the largest mutable statics (.bss/.data) of the shared core, in bytes (decimal).
#   sh tools/ram_ledger.sh [N]      N rows (default 25); builds core objects natively if needed.
# Host sizes (pointers are 8 B here, 4 B on GBA/PS1), so read it as a RANKING of where RAM goes,
# not as the GBA/PS1 numbers: those come from `make -C gba` / the CI link maps.
set -e
cd "$(dirname "$0")/.."
N=${1:-25}
make -s -C core lib >/dev/null
find core/build/obj/src core/build/obj/gen -name '*.o' | while read -r o; do
  nm -S -t d "$o" 2>/dev/null | awk -v f="$o" '$3 ~ /^[bBdD]$/ { printf "%d %s %s\n", $2, $4, f }'
done | sort -rn | head -"$N"
