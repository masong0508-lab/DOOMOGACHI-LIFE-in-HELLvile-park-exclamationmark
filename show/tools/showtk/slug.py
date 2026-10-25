"""slug.py - the slug animatic: one slug card per beat of PILOT_BEATSHEET.md
(owner: ENGINE lane 1; WP2, M0-T3, AT2.2).

    from showtk import slug
    beats = slug.beats()                      # 60 beats, frames summing to 39,600
    data = slug.episode(beats).build()        # EP101.SHW bytes

Each beat becomes one shot: a BANDS sky in the segment's ramp, a GRAIN plane
(boil on twos) and a SHWR_SLUG card (act/beat, title, two lines). The beat
sheet is the timing source (read-only); its headings are
`### <ID> · <mm:ss> to <mm:ss> · <m:ss>`.
"""
from __future__ import annotations

import re
import textwrap
import zlib
from dataclasses import dataclass

from . import paths
from . import shw_format as F

BEATSHEET = paths.DOCS_SHOW / "PILOT_BEATSHEET.md"
_HEAD = re.compile(r"^### (\S+) · (\d+):(\d\d) to (\d+):(\d\d) · (\d+):(\d\d)\s*$")
# segment -> (act number on the card, ramp for the sky)
_SEG = {"CO": (0, 4), "MT": (0, 6), "1": (1, 2), "EC1": (0, 7), "2": (2, 3), "EC2": (0, 7),
        "3": (3, 11), "EC3": (0, 7), "4": (4, 10), "T": (0, 8), "CR": (0, 0), "PC": (0, 13)}


@dataclass
class Beat:
    id: str
    start: int          # frame
    frames: int
    segment: str
    title: str
    what: str


def beats(path=BEATSHEET) -> list[Beat]:
    out, seg_title = [], ""
    lines = path.read_text().splitlines()
    for i, line in enumerate(lines):
        if line.startswith("## ") and not line.startswith("### "):
            seg_title = line[3:].strip()
        m = _HEAD.match(line)
        if not m:
            continue
        bid = m.group(1)
        a = (int(m.group(2)) * 60 + int(m.group(3))) * 30
        b = (int(m.group(4)) * 60 + int(m.group(5))) * 30
        what = ""
        for nxt in lines[i + 1:i + 12]:
            if nxt.startswith("| **What happens**"):
                what = nxt.split("|")[2].strip()
                break
        seg = bid.split("-")[0]
        out.append(Beat(bid, a, b - a, seg, seg_title, re.sub(r"[*_`]", "", what)))
    return out


def _card_lines(text, width=46):
    w = textwrap.wrap(text.encode("latin-1", "replace").decode("latin-1"), width)
    l1 = w[0] if w else ""
    l2 = (w[1] if len(w) > 1 else "")
    if len(w) > 2:
        l2 = l2[:width - 3] + "..."
    return l1, l2


def chunk_for(b: Beat, n: int) -> F.Chunk:
    act, ramp = _SEG.get(b.segment, (0, 0))
    ch = F.Chunk(bg=F.PAL_BLACK)
    ch.planes.append(F.plane_bands([(0, F.pal_ramp(ramp, 1)), (60, F.pal_ramp(ramp, 2)),
                                    (120, F.pal_ramp(ramp, 3))], order=0))
    ch.planes.append(F.plane_grain(F.pal_ramp(ramp, 4), 8, zlib.crc32(b.id.encode()), boil=2,
                                   order=1))
    l1, l2 = _card_lines(b.what)
    title = b.title.replace("“", '"').replace("”", '"')
    ch.add_slug(f"{b.id}  {title}"[:52], l1, l2, beat=n + 1, act=act, bg=F.PAL_BLACK,
                fg=F.PAL_WHITE)
    ch.add_beat([])
    return ch


def episode(bs: list[Beat] | None = None, first=0, count=None) -> F.Episode:
    bs = bs if bs is not None else beats()
    ep = F.Episode(episode=101, toolchain=b"showtk-slug-1")
    sel = bs[first:first + count if count else None]
    for n, b in enumerate(sel, start=first):
        ep.add_shot(b.id.lower(), b.frames, chunk_for(b, n), seed=zlib.crc32(b.id.encode()),
                    beat=n + 1)
    return ep
