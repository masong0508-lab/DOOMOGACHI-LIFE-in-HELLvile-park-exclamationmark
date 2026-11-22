#!/bin/sh
# Reproducible EE build from a Linux machine with Docker installed.
# The official ps2dev image supplies the MIPS toolchain, PS2SDK and gsKit.
# Audio/ISO use only PS2SDK's audsrv.irx and the generated Talk bank.
set -eu
if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required. The PS2SDK toolchain is not installed here." >&2
    exit 2
fi
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
echo "Building PS2 shared-game ELF and ISO from $ROOT"
docker run --rm \
  -v "$ROOT:/src" -w /src \
  ps2dev/ps2dev:latest \
  sh -ec '
    apk add --no-cache make python3 xorriso
    make -C ps2 -j2
    make -C ps2 game -j2
    make -C ps2 disc
    test -s ps2/build/doomogachi_ps2_game.elf
    test -s ps2/build/doomogachi_ps2_game.iso
    echo "Produced /src/ps2/build/doomogachi_ps2_game.iso"
  '
