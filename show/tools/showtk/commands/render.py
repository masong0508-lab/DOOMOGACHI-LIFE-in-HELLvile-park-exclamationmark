"""show render - capture an episode file through the render cache (WP3).

    show render FILE.SHW [--jobs 2] [--shots co-1..co-3] [--force] [--no-encode]
    show render FILE.SHW --seek co-2+150 [--count 1]

Output: build/ep<nnn>/render/{frames.sha256, plog.tsv, shots.tsv, render.json};
masters in build/cache/shots/<key>/master.mkv.
"""
from pathlib import Path

from .. import render
from .. import shw_format as F


def _sel(spec, ids):
    if not spec:
        return None
    out = []
    for part in spec.split(","):
        a, _, b = part.partition("..")
        i = render.parse_shot_ref(a, ids)[0]
        j = render.parse_shot_ref(b, ids)[0] if b else i
        out += range(i, j + 1)
    return out


def _run(a):
    ids = [s["id"] for s in F.read_episode(Path(a.file).read_bytes())["shots"]]
    if a.seek:
        s, f = render.parse_shot_ref(a.seek, ids)
        r = render.seek(a.file, s, f, a.count, keep=False)
        for i, h in enumerate(r["sha"]):
            print(f"{h}  {ids[s]}+{f + i:04d}")
        return 0
    render.render(a.file, jobs=a.jobs, shots=_sel(a.shots, ids), encode=not a.no_encode,
                  force=a.force)
    return 0


def register(sub):
    p = sub.add_parser("render", help="capture an episode file to per-shot masters (cached)")
    p.add_argument("file")
    p.add_argument("--jobs", "-j", type=int, default=2)
    p.add_argument("--shots", help="a..b[,c] by id or index")
    p.add_argument("--force", action="store_true")
    p.add_argument("--no-encode", action="store_true")
    p.add_argument("--seek", help="SHOT+FRAME: capture from there alone (no cache)")
    p.add_argument("--count", type=int, default=1)
    p.set_defaults(func=_run)
