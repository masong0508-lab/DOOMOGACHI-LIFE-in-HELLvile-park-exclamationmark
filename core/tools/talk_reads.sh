#!/bin/sh
# TKP0: reads of the Talk database per exchange and line, by bank (what a SpuRead would carry on the PS1).
# usage: sh core/tools/talk_reads.sh [CONTEXTS]   (from the repo root; builds core/build/census_reads)
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
N=${1:-5000}
make -C "$R/core" -j2 talk "$R/core/build/talk_census" >/dev/null
LINK=$(make -C "$R/core" -n -B "$R/core/build/talk_census" 2>/dev/null | grep -- "-o $R/core/build/talk_census " | tail -1)
CMD=$(echo "$LINK" | sed "s# -o $R/core/build/talk_census # -o $R/core/build/census_reads $R/core/tools/talk_reads.c -Wl,--wrap=plat_talk_read -Wl,--wrap=talk_gen_exchange #")
eval "$CMD"
cd "$R/core" && ./build/census_reads --grid data/talk/census_en.tsv --contexts "$N" --seed 1 2>&1 | grep "talk_reads\|talk_census:"
