#!/bin/sh
# TKP0: time talk_dm_say / talk_dm_step on the host. usage: sh core/tools/talk_steps.sh [TURNS [SESSIONS]]
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
make -C "$R/core" -j2 talk "$R/core/build/talk_census" >/dev/null
LINK=$(make -C "$R/core" -n -B "$R/core/build/talk_census" 2>/dev/null | grep -- "-o $R/core/build/talk_census " | tail -1)
CMD=$(echo "$LINK" | sed "s# -o $R/core/build/talk_census # -o $R/core/build/talk_steps #; s#$R/core/tools/talk_census.c#$R/core/tools/talk_steps.c#")
eval "$CMD"
cd "$R/core" && ./build/talk_steps "$@"
