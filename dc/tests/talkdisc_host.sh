#!/bin/sh
# TD1: build TALK_en.BIN and read it through dc/src/talkdisc.c (KOS file calls mocked) and the core's
# real talk_db.c. Host cc only: `sh dc/tests/talkdisc_host.sh`. Part of `make -C dc check-talkdisc`.
set -eu
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
python3 -I "$root/core/tools/talkc.py" --bin --lang en --out "$t/TALK_en.BIN" >/dev/null
${CC:-cc} -std=gnu99 -Wall -Wextra -Werror -DDOOMO_PLATFORM_DC=1 -DDOOMO_HAS_TALK=1 \
    -I"$root/core/include" -I"$root/dc/src" -I"$root/dc/harness" -I"$here/mock" \
    "$here/talkdisc_host.c" "$here/packdisc_mock.c" "$root/dc/src/talkdisc.c" \
    "$root/core/src/talk_db.c" "$root/core/src/lang.c" "$root/core/src/weak_talk.c" \
    "$root/core/gen/lang_registry.c" "$root/core/gen/txt_dict.c" "$root/core/src/txt.c" -o "$t/t" 2>"$t/cc.log" || { cat "$t/cc.log" >&2; exit 1; }
TALK_FILE="$t/TALK_en.BIN" "$t/t"
