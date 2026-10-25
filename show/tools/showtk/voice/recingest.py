"""`show voice ingest <wav> --line <id>` (spec §10.2 step 6): copy the creator's recording
into the episode's vo/ folder under the next take number and re-lock that one line.

The drop folder works without this command too: any <line_id>_take<n>.wav placed in
show/<ep>/vo/ is picked up by the next `show voice <ep>` (the highest n wins unless
takes.toml says `fav = <n>`). vo/ is gitignored: recordings go to the vault, the lock
keeps their hashes.
"""
from __future__ import annotations

import os
import shutil

from . import build, env, shwio, takes


def next_take(ep_dir, line_id, scratch=False):
    files = takes.vo_files(ep_dir).get(line_id, {}).get("scratch" if scratch else "rec", {})
    return (max(files) + 1) if files else 1


def ingest_file(wav, line_id, ep="ep101", scratch=False, log=print):
    d = env.ep_dir(ep)
    lines, _ = shwio.read_lines(os.path.join(d, "lines.shw"))
    if line_id not in lines:
        raise KeyError(f"{ep}: no line {line_id} in lines.shw")
    n = next_take(d, line_id, scratch)
    vo = os.path.join(d, "vo")
    os.makedirs(vo, exist_ok=True)
    ext = os.path.splitext(wav)[1].lower() or ".wav"
    dst = os.path.join(vo, f"{line_id}_{'s' if scratch else 'take'}{n}{ext}")
    shutil.copyfile(wav, dst)
    log(f"{wav} -> {env.rel(dst)}")
    build.build(ep, only={line_id}, log=log)
    return dst
