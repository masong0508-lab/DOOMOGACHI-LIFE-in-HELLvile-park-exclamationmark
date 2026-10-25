"""cache.py - the per-shot render cache (owner: ENGINE lane 1; spec 11.2).

Shot key = SHA-256 of (the shot chunk, the chunks of its base=/replay=
sources, the shot's index row minus its start frame, show.exe, the emulator,
the BIOS, the harness version, the episode seed). An entry is
build/cache/shots/<key>/{master.mkv, frames.sha256, plog.tsv, perf.json}
(master.mkv is absent for a hash-only render). A shot moved in time but
otherwise unchanged keeps its key.
"""
from __future__ import annotations

import hashlib
import json
import shutil
from functools import lru_cache
from pathlib import Path

from . import capture, paths

ROOT = paths.BUILD / "cache" / "shots"


@lru_cache(maxsize=4)
def _file_sha(p: str, mtime: float) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def file_sha(p) -> str:
    p = Path(p).resolve()
    return _file_sha(str(p), p.stat().st_mtime)


@lru_cache(maxsize=1)
def _env_hashes():
    e = capture.emulator_hashes()
    return e["emu"] + e["bios"] + capture.lua_hash()


def shot_keys(data: bytes, ep: dict, exe: Path) -> list[str]:
    """One key per shot of a parsed episode (shw_format.read_episode)."""
    base = file_sha(exe) + _env_hashes() + "%08x" % ep["seed"]
    chunk = []
    for s in ep["shots"]:
        off = s["chunk_sector"] * 2048
        chunk.append(hashlib.sha256(data[off:off + s["chunk_bytes"]]).hexdigest())
    keys = []
    for i, s in enumerate(ep["shots"]):
        h = hashlib.sha256(base.encode())
        h.update(chunk[i].encode())
        for src in (s["base_shot"], s["replay_shot"]):
            if src != 0xFFFF and src < len(chunk):
                h.update(chunk[src].encode())
        row = {k: v for k, v in s.items() if k not in ("start_frame", "chunk_sector", "records",
                                                         "chunk_hdr")}
        h.update(json.dumps(row, sort_keys=True, default=str).encode())
        keys.append(h.hexdigest()[:32])
    return keys


def entry(key: str) -> Path:
    return ROOT / key


def hit(key: str, need_master=True) -> bool:
    d = entry(key)
    if not (d / "frames.sha256").exists() or not (d / "plog.tsv").exists():
        return False
    return (d / "master.mkv").exists() or not need_master


def drop(key: str):
    d = entry(key)
    if d.is_dir():
        shutil.rmtree(d)
