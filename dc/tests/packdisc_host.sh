#!/bin/sh
# E10: build the example pack for port dc and read it through dc/src/packdisc.c (KOS file calls mocked).
# Host cc only: `sh dc/tests/packdisc_host.sh`. Part of `make -C dc check-packdisc`.
set -eu
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
python3 -I "$root/tools/mpack/mpack.py" example --port dc --out "$t/pack.dmpk" >/dev/null
python3 -I "$root/tools/mpack/mpack.py" validate "$t/pack.dmpk" --port dc >/dev/null
${CC:-cc} -std=gnu99 -Wall -Wextra -Werror -DDOOMO_PLATFORM_DC=1 -DDOOMO_PACK_ON_DISC=1 \
    -I"$root/core/include" -I"$root/dc/src" -I"$root/dc/harness" -I"$here/mock" \
    "$here/packdisc_host.c" "$here/packdisc_mock.c" "$root/dc/src/packdisc.c" "$root/core/src/pack.c" -o "$t/t"
PACK_FILE="$t/pack.dmpk" "$t/t"
