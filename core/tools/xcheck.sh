#!/bin/sh
# core/tools/xcheck.sh - compile EVERY portable core source (core/src/*.c,
# core/gen/*.c) with the three -Werror toolchains from the team rules:
# host gcc, arm-none-eabi-gcc (GBA) and mipsel-linux-gnu-gcc (PS1).
# Usage: sh core/tools/xcheck.sh [file.c ...]   (default: all core sources)
# Exit status is non-zero if any compile fails. Owner: foundation.
CORE=$(cd "$(dirname "$0")/.." && pwd)
INC="$CORE/include"
if [ $# -gt 0 ]; then FILES="$*"; else FILES=$(ls "$CORE"/src/*.c "$CORE"/gen/*.c 2>/dev/null | sort); fi
PSN=/opt/psn00bsdk/include/libpsn00b
JOBS=$(nproc 2>/dev/null || echo 2)
TMP=${TMPDIR:-/tmp}/doomo-xcheck.$$
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

have() { command -v "$1" >/dev/null 2>&1; }
MIPS_INC=""
if have mipsel-linux-gnu-gcc; then MIPS_INC=$(mipsel-linux-gnu-gcc -print-file-name=include); fi

check_one() {
    f="$1"
    b=$(basename "$f" .c)
    ok=1
    if ! gcc -std=c99 -Wall -Wextra -Werror -I"$INC" -c "$f" -o /dev/null 2>"$TMP/$b.gcc"; then
        echo "FAIL gcc    $f"; cat "$TMP/$b.gcc"; ok=0
    fi
    if have arm-none-eabi-gcc; then
        if ! arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -mthumb -mcpu=arm7tdmi -O2 \
                -ffunction-sections -I"$INC" -c "$f" -o /dev/null 2>"$TMP/$b.arm"; then
            echo "FAIL arm    $f"; cat "$TMP/$b.arm"; ok=0
        fi
    fi
    if have mipsel-linux-gnu-gcc; then
        if ! mipsel-linux-gnu-gcc -std=c99 -Wall -Wextra -Werror -O2 -march=r3000 -msoft-float \
                -G0 -fno-pic -mno-abicalls -ffreestanding -fno-builtin -nostdinc \
                -I"$PSN" -isystem "$MIPS_INC" -I"$INC" -c "$f" -o /dev/null 2>"$TMP/$b.mips"; then
            echo "FAIL mipsel $f"; cat "$TMP/$b.mips"; ok=0
        fi
    fi
    [ $ok -eq 1 ] && echo "ok          $f"
    [ $ok -eq 1 ]
}

export INC PSN TMP MIPS_INC
fail=0
n=0
for f in $FILES; do
    n=$((n + 1))
done
# Run in parallel batches of $JOBS.
pids=""
i=0
for f in $FILES; do
    ( check_one "$f" > "$TMP/$(basename "$f" .c).out" 2>&1; echo $? > "$TMP/$(basename "$f" .c).rc" ) &
    pids="$pids $!"
    i=$((i + 1))
    if [ $((i % JOBS)) -eq 0 ]; then wait; fi
done
wait
for f in $FILES; do
    b=$(basename "$f" .c)
    cat "$TMP/$b.out"
    rc=$(cat "$TMP/$b.rc" 2>/dev/null || echo 1)
    [ "$rc" = "0" ] || fail=1
done
have arm-none-eabi-gcc || echo "WARNING: arm-none-eabi-gcc not found (GBA check skipped)"
have mipsel-linux-gnu-gcc || echo "WARNING: mipsel-linux-gnu-gcc not found (PS1 check skipped)"
if [ $fail -ne 0 ]; then echo "xcheck: FAILED"; exit 1; fi
echo "xcheck: $n files OK on all compilers"
