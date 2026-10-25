"""AT10.4: every SFX name in series/sfx.shw resolves to a rendered 48 kHz file at
-1 dBFS peak, not silent, captioned [like this], with every source licensed."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))

from showtk.sfx import library as L  # noqa: E402


def test_every_sfx_name_resolves():
    if not (L.P.LIB_SFX / "library.json").exists():
        L.build(verbose=False)
    assert L.check() == []


if __name__ == "__main__":
    test_every_sfx_name_resolves()
    print("AT10.4 PASS")
