"""Guard correct gsKit GS_ONESHOT DMA queue ownership.

In upstream ps2dev/gsKit ee/gs/src/gsCore.c, gsKit_queue_exec_real()
already flips Queue->dbuf and resets its one-shot pool internally.
A second explicit gsKit_queue_reset() after sync_flip double-toggles
the DMA backing pool and may reuse a buffer still in flight.
The game and diagnostics must submit and sync, but NOT reset again.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FRAME_END = re.compile(
    r"gsKit_queue_exec\(gs\);\s*gsKit_sync_flip\(gs\);",
    re.DOTALL,
)


def test_queue_lifetime():
    for name, minimum in (("game_main.c", 2),
                          ("main.c", 2), ("gs_only.c", 1)):
        code = (ROOT / "src" / name).read_text(encoding="utf-8")
        execs = len(re.findall(r"gsKit_queue_exec\(gs\);", code))
        flips = len(re.findall(r"gsKit_sync_flip\(gs\);", code))
        paired = len(FRAME_END.findall(code))
        extra_resets = len(re.findall(r"gsKit_queue_reset\(", code))
        assert execs >= minimum, f"{name}: missing GS command submission"
        assert execs == flips == paired, (
            f"{name}: {execs} GS submissions, {flips} flips, "
            f"{paired} paired boundaries"
        )
        assert extra_resets == 0, (
            f"{name}: gsKit queue_exec already resets GS_ONESHOT "
            "internally; an explicit second reset double-swaps DMA pools"
        )


if __name__ == "__main__":
    test_queue_lifetime()
    print("PS2 GS queues: one execution/flip pair, no redundant manual resets")
