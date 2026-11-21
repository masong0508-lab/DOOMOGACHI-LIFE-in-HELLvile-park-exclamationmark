"""Every gsKit one-shot frame must free its DMA command queue.

The GS queue has a finite fixed-size pool. Retaining commands causes
replayed sprites, stale frames, and eventually exhaustion. Reset only
AFTER the DMA execution and the buffer flip, never before GPU reads it.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FRAME_END = re.compile(
    r"gsKit_queue_exec\(gs\);\s*"
    r"gsKit_sync_flip\(gs\);\s*"
    r"(?:/\*.*?\*/\s*)?"
    r"gsKit_queue_reset\(gs->Os_Queue\);",
    re.DOTALL,
)


def test_queue_lifetime():
    for name, minimum in (("game_main.c", 2),
                          ("main.c", 2), ("gs_only.c", 1)):
        code = (ROOT / "src" / name).read_text(encoding="utf-8")
        executes = len(re.findall(r"gsKit_queue_exec\(gs\);", code))
        flips = len(re.findall(r"gsKit_sync_flip\(gs\);", code))
        resets = len(re.findall(r"gsKit_queue_reset\(gs->Os_Queue\);", code))
        ordered = len(FRAME_END.findall(code))
        assert executes >= minimum, f"{name}: missing drawing path"
        assert executes == flips == resets == ordered, (
            f"{name}: {executes} exec / {flips} flips / {resets} "
            f"resets / {ordered} correctly ordered frame endings"
        )


if __name__ == "__main__":
    test_queue_lifetime()
    print("PS2 GS one-shot queue is reset after every display flip")
