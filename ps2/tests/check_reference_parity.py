#!/usr/bin/env python3
"""PS2 reference renderer fidelity guard: no silent geometry divergence."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
pairs = (
    ("core/native/overworld.c", "ps2/src/ref_overworld.c", True),
    ("core/native/raycast.c", "ps2/src/ref_view3d.c", False),
)
for source, target, has_counter in pairs:
    old = (root / source).read_text()
    expected = old.replace('#include "native.h"', '#include "frame_ref.h"')
    if has_counter:
        expected = expected.replace("    nat_calls[NC_VIEW]++;\n", "")
    got = (root / target).read_text()
    if got != expected:
        raise SystemExit(f"PS2 reference renderer diverged: {target} vs {source}")
    print(f"Parity PASS: {source} -> {target}")
make = (root / "ps2/Makefile").read_text()
screen = (root / "ps2/src/game2d.c").read_text()
for path in ("build/ref_overworld.o", "build/ref_view3d.o"):
    assert path in make, path
for name in ("plat_view3d(", "plat_overworld("):
    assert name not in screen, "duplicate placeholder in game2d: "+name
print("PS2 shared-map renderer wiring PASS")
