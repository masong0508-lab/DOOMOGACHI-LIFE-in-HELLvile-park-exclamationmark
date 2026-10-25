"""render.py - render an episode file to per-shot masters through the cache
(owner: ENGINE lane 1; spec 11, WP3).

    from showtk import render
    rep = render.render(shw_path, jobs=2)                 # every dirty shot
    rep = render.render(shw_path, shots=[3, 4], encode=False)
    h = render.seek(shw_path, shot=1, frame=150)          # one frame alone (AT3.3)

render() builds a disc (show.exe + SHOW/EP###.SHW), keys every shot
(cache.py), splits the dirty ones between `jobs` emulators (scheduler.py),
captures them (capture.py) and writes, in <out>/ (default
build/ep<nnn>/render/):
  frames.sha256  one line per frame of the selected shots, episode order
  plog.tsv       the P1/P4/stats log, with the shot id in column 1
  shots.tsv      shot index, id, key, frames, cache entry
  render.json    timings: wall seconds, frames, ms/frame, jobs, hits
A CaptureError names the shot and frame (AT3.4).
"""
from __future__ import annotations

import json
import threading
import time
from pathlib import Path

from . import cache, capture, disc, errors, paths, scheduler
from . import shw_format as F


def _exe(preset):
    d = paths.build_dir(preset)
    exe, mp = d / "show.exe", d / "show.map"
    if not exe.exists() or not mp.exists():
        errors.fail("SHOW-RENDER", f"{paths.rel(exe)} not built (run `show build`)")
    return d, exe, mp


def _disc(shw: Path, ep: dict, work: Path, preset: str):
    d, exe, mp = _exe(preset)
    cue = disc.make(d, {"EP%03d.SHW" % ep["episode"]: shw}, work / "disc")
    return cue, exe, mp


def render(shw, *, jobs=2, shots=None, encode=True, force=False, out=None, preset="ps1-show",
           timeout=None, quiet=False) -> dict:
    shw = Path(shw).resolve()
    data = shw.read_bytes()
    ep = F.read_episode(data)
    ids = [s["id"] for s in ep["shots"]]
    lens = [s["len"] for s in ep["shots"]]
    sel = list(range(len(ids))) if shots is None else sorted(set(shots))
    out = Path(out) if out else paths.episode_build("ep%03d" % ep["episode"]) / "render"
    # one work dir per .SHW path (and process), so agents rendering different files at once
    # never share a disc image or a FIFO
    import hashlib
    import os
    work = paths.SHOW_WORK / "render" / ("ep%03d-%s-%d" % (
        ep["episode"], hashlib.sha1(str(shw).encode()).hexdigest()[:8], os.getpid()))
    work.mkdir(parents=True, exist_ok=True)
    cue, exe, mp = _disc(shw, ep, work, preset)
    keys = cache.shot_keys(data, ep, exe)
    dirty = [s for s in sel if force or not cache.hit(keys[s], need_master=encode)]
    for s in dirty:
        cache.drop(keys[s])
    groups = scheduler.boots(dirty, lens, jobs)
    t0 = time.time()
    results, fails = [], []

    def job(gi, boots):
        for bi, group in enumerate(boots):
            if fails:
                return
            runs = scheduler.runs_for(group, lens)
            n = sum(r.count for r in runs)
            try:
                results.append(capture.run(
                    cue, mp, runs, ids, lambda s, ri: cache.entry(keys[s]),
                    work=work / f"job{gi}", encode=encode, episode=ep["episode"],
                    timeout=timeout or max(120, n // 30)))
            except errors.ShowError as e:
                fails.append(e)

    th = [threading.Thread(target=job, args=(gi, g)) for gi, g in enumerate(groups)]
    for t in th:
        t.start()
    for t in th:
        t.join()
    wall = time.time() - t0
    if fails:
        raise fails[0]
    out.mkdir(parents=True, exist_ok=True)
    nfr = 0
    with open(out / "frames.sha256", "w") as fs, open(out / "plog.tsv", "w") as fp, \
            open(out / "shots.tsv", "w") as ft:
        fp.write("shot\t" + capture.PLOG_HEADER + "\n")
        for s in sel:
            e = cache.entry(keys[s])
            sha = (e / "frames.sha256").read_text().splitlines()
            if len(sha) != lens[s]:
                errors.fail("SHOW-RENDER", f"{ids[s]}: cache has {len(sha)} of {lens[s]} frames")
            fs.write("\n".join(sha) + "\n")
            for line in (e / "plog.tsv").read_text().splitlines()[1:]:
                fp.write(f"{ids[s]}\t{line}\n")
            ft.write(f"{s}\t{ids[s]}\t{keys[s]}\t{lens[s]}\t{e}\n")
            nfr += lens[s]
    made = sum(lens[s] for s in dirty)
    rep = dict(shw=str(shw), shots=len(sel), frames=nfr, rendered_shots=len(dirty),
               rendered_frames=made, hits=len(sel) - len(dirty), jobs=len(groups),
               wall_s=round(wall, 2), ms_per_frame=round(1000 * wall / made, 2) if made else 0,
               boot_s=[round(r.boot_seconds, 2) for r in results if r], encode=encode,
               out=str(out))
    (out / "render.json").write_text(json.dumps(rep, indent=1) + "\n")
    import shutil
    shutil.rmtree(work, ignore_errors=True)       # disc image + FIFOs; the cache keeps the shots
    # The .SHW keeps 8-byte shot ids; timeline.json beside it has the full ones (by index).
    # render_index.json maps those to cache dirs so preview/deliver (edl.Masters) find them.
    full = list(ids)
    tlp = shw.parent / "timeline.json"
    if tlp.exists():
        try:
            tsh = json.loads(tlp.read_text()).get("shots", [])
            if len(tsh) == len(ids):
                full = [t.get("id", i) for t, i in zip(tsh, ids)]
        except (ValueError, AttributeError):
            pass
    (out / "render_index.json").write_text(json.dumps(
        {"shw": str(shw), "shots": {full[s]: str(cache.entry(keys[s])) for s in sel}}, indent=1) + "\n")
    if not quiet:
        print(f"render: {len(sel)} shots, {nfr} frames ({len(dirty)} shots / {made} frames "
              f"rendered on {len(groups)} job(s) in {wall:.1f} s, "
              f"{rep['ms_per_frame']} ms/frame; {rep['hits']} cache hits) -> {paths.rel(out)}")
    return rep


def seek(shw, shot: int, frame: int, count=1, *, preset="ps1-show", keep=True) -> dict:
    """Boot, seek to (shot, frame) and capture `count` frames alone. Returns
    {"sha": [...], "rgb": {(shot, frame): array}} (nothing is cached)."""
    shw = Path(shw).resolve()
    data = shw.read_bytes()
    ep = F.read_episode(data)
    ids = [s["id"] for s in ep["shots"]]
    work = paths.SHOW_WORK / "seek"
    cue, exe, mp = _disc(shw, ep, work, preset)
    keepd = {(shot, frame + i): None for i in range(count)} if keep else None
    outd = work / "out"
    capture.run(cue, mp, [capture.Run(shot, frame, count, shot)], ids, lambda s, ri: outd / ids[s],
                work=work / "emu", encode=False, keep_frames=keepd, timeout=120)
    sha = [l.split()[0] for l in (outd / ids[shot] / "frames.sha256").read_text().splitlines()]
    return dict(sha=sha, rgb=keepd)


def parse_shot_ref(ref: str, ids: list[str]) -> tuple[int, int]:
    """'co-2+150' or '1+150' -> (shot index, frame)."""
    name, _, fr = ref.partition("+")
    if name.isdigit():
        i = int(name)
    elif name.lower() in ids:
        i = ids.index(name.lower())
    else:
        errors.fail("SHOW-RENDER", f"no shot {name!r} (have {', '.join(ids[:6])}, ...)")
    return i, int(fr or 0)
