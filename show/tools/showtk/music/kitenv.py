"""Import the vendored production kit (show/audio/kit) with its samples and
cache pointed at the vault copies (SHOW_ENGINE_SPEC.md sec.10.7).

The kit is vendored in show/audio (WP9: text only; `kit/core.py` reads
KIT_ASSETS / KIT_CACHE). Its 168 MB of CC0 / CC BY samples stay out of git:
this module points KIT_ASSETS at audiopaths.KIT_ASSETS and KIT_CACHE at
audiopaths.KIT_CACHE before the first import, and re-points the module
attributes too in case the kit was imported earlier.

It also records every sample file the kit loads (`used()`), so each cue and
SFX lists its sample sources for AUD-LIC (show/lib/licences/licences.toml).

    from showtk.music.kitenv import kit
"""
from __future__ import annotations

import os
import sys

from . import audiopaths as P

os.environ.setdefault("KIT_ASSETS", str(P.KIT_ASSETS))
os.environ.setdefault("KIT_CACHE", str(P.KIT_CACHE))
P.ensure(P.KIT_CACHE)
if str(P.AUDIO) not in sys.path:
    sys.path.insert(0, str(P.AUDIO))

import kit  # noqa: E402  (show/audio/kit)
from kit import core as _core  # noqa: E402
from kit import samplers as _samplers  # noqa: E402

for _m in (_core, _samplers):
    _m.ASSETS = os.environ["KIT_ASSETS"]
    _m.CACHE = os.environ["KIT_CACHE"]

SR = kit.SR
SPF = SR // 30          # 1,600 samples per 30 fps frame (sec.10.1)

_USED: set = set()
_orig_load_cached = _core._load_cached


def _recording_load_cached(path, sr):
    _USED.add(os.path.abspath(path))
    return _orig_load_cached(path, sr)


if getattr(_core._load_cached, "__name__", "") != "_recording_load_cached":
    _core._load_cached = _recording_load_cached


def reset_used():
    _USED.clear()


def used() -> list:
    """Sample sources loaded since reset_used(), as AUD-LIC keys ("kit:<path under assets>")."""
    root = os.path.abspath(os.environ["KIT_ASSETS"])
    out = set()
    for p in _USED:
        if p.startswith(root + os.sep):
            out.add("kit:" + os.path.relpath(p, root))
        else:
            out.add("file:" + p)
    return sorted(out)


__all__ = ["kit", "SR", "SPF", "reset_used", "used"]
