#!/bin/sh
# Optional PCSX2 launch probe. Requires a locally configured emulator
# and the user's own BIOS; neither is bundled in the repository/CI.
# This cannot turn a timed-out process into proof of playable graphics.
set -eu
ISO=${1:-ps2/build/doomogachi_ps2_game.iso}
PCSX2_BIN=${PCSX2_BIN:-pcsx2-qt}
LIMIT=${PCSX2_BOOT_SECONDS:-60}
LOG=${PCSX2_LOG:-ps2/build/pcsx2-boot.log}
if [ ! -s "$ISO" ]; then
  echo "Missing ISO: $ISO (run make -C ps2 disc)" >&2
  exit 2
fi
if ! command -v "$PCSX2_BIN" >/dev/null 2>&1; then
  echo "PCSX2 not installed: $PCSX2_BIN" >&2
  exit 2
fi
if ! command -v timeout >/dev/null 2>&1; then
  echo "GNU timeout is required for an unattended boot probe" >&2
  exit 2
fi
mkdir -p "$(dirname "$LOG")"
echo "Starting PCSX2 for up to $LIMIT seconds, output: $LOG"
set +e
timeout --signal=TERM --kill-after=5 "${LIMIT}s" "$PCSX2_BIN" -batch -nogui -- "$ISO" >"$LOG" 2>&1
status=$?
set -e
if [ "$status" -ne 124 ] && [ "$status" -ne 0 ]; then
  echo "PCSX2 exited with error $status; see $LOG" >&2
  exit 1
fi
if grep -q 'PS2 shared-core prototype: doomo_init complete' "$LOG"; then
  echo "Core initialization marker observed; visual, audio and save tests still needed."
  exit 0
fi
echo "INCONCLUSIVE: no core init marker; a long-running process alone does not prove boot."
echo "Review $LOG and capture a visual walkthrough in PCSX2."
exit 3
