"""A small reader for the audio .shw files (series/music.shw, series/sfx.shw,
epNNN/cues.shw), following SHOW_ENGINE_SPEC.md sec.5.2:

* one statement per line; `;` chains statements on one line;
* `#` starts a comment (outside double quotes);
* a block (here: `cue`) ends at `end`;
* a statement is words followed by key=value pairs; values may be
  double-quoted strings.

The engine's own parser (showtk/parse.py) reads the same files; this one
exists so the music and SFX tools run stand-alone (and before parse.py lands).
Every statement keeps its file and line for error messages.
"""
from __future__ import annotations

import dataclasses as dc
import re
import shlex
from pathlib import Path


class ShwError(Exception):
    pass


@dc.dataclass
class Stmt:
    words: list
    kv: dict
    file: str
    line: int
    body: list = dc.field(default_factory=list)     # statements inside a block

    @property
    def head(self) -> str:
        return self.words[0] if self.words else ""

    @property
    def name(self) -> str:
        return self.words[1] if len(self.words) > 1 else ""

    def where(self) -> str:
        return f"{self.file}:{self.line}"

    def get(self, k, default=None):
        return self.kv.get(k, default)


def _strip_comment(line: str) -> str:
    out, q = [], False
    for c in line:
        if c == '"':
            q = not q
        if c == "#" and not q:
            break
        out.append(c)
    return "".join(out)


def _split_semis(line: str) -> list:
    parts, cur, q = [], [], False
    for c in line:
        if c == '"':
            q = not q
        if c == ";" and not q:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append("".join(cur))
    return [p.strip() for p in parts if p.strip()]


_KV = re.compile(r"^([A-Za-z_][A-Za-z0-9_.:-]*)=(.*)$")


def parse_stmt(text: str, file: str, line: int) -> Stmt:
    try:
        toks = shlex.split(text, posix=True)
    except ValueError as e:
        raise ShwError(f"{file}:{line}: {e}") from None
    words, kv = [], {}
    for t in toks:
        m = _KV.match(t)
        if m and words:
            kv[m.group(1)] = m.group(2)
        else:
            words.append(t)
    return Stmt(words, kv, file, line)


def parse_text(text: str, file: str = "<text>", blocks=("cue",)) -> list:
    """-> top-level statements; block statements carry `.body`."""
    top, stack = [], []
    for ln, raw in enumerate(text.splitlines(), 1):
        line = _strip_comment(raw).strip()
        if not line:
            continue
        for part in _split_semis(line):
            st = parse_stmt(part, file, ln)
            if not st.words:
                continue
            if st.head == "end":
                if not stack:
                    raise ShwError(f"{file}:{ln}: `end` without a block")
                stack.pop()
                continue
            (stack[-1].body if stack else top).append(st)
            if st.head in blocks:
                stack.append(st)
    if stack:
        raise ShwError(f"{stack[-1].where()}: block `{stack[-1].head} {stack[-1].name}` has no `end`")
    return top


def parse_file(path, blocks=("cue",)) -> list:
    p = Path(path)
    return parse_text(p.read_text(encoding="utf-8"), str(p), blocks)


# ---- values ------------------------------------------------------------------
def db(v, default=0.0) -> float:
    """'-12dB' / '-12' / '+3 dB' -> float dB."""
    if v is None:
        return float(default)
    s = str(v).strip().lower().replace(" ", "")
    if s.endswith("db"):
        s = s[:-2]
    return float(s)


def boolv(v, default=False) -> bool:
    if v is None:
        return default
    return str(v).lower() in ("1", "yes", "on", "true", "y")


def frames(v, fps=30) -> int:
    """'29' / '29f' / '1.5s' / 'R8' (the ladder) -> frames."""
    s = str(v).strip()
    ladder = {"R2": 2, "R3": 3, "R5": 5, "R8": 8, "R13": 13, "R21": 21, "R26": 26}
    if s.upper() in ladder:
        return ladder[s.upper()]
    if s.endswith("s") and not s.endswith("smp"):
        f = float(s[:-1]) * fps
        if abs(f - round(f)) > 1e-6:
            raise ShwError(f"{v}: seconds must be a whole number of frames")
        return int(round(f))
    if s.endswith("f"):
        s = s[:-1]
    return int(s)
