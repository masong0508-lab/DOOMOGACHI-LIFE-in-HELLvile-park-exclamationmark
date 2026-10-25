"""Readers for the audio side of the .shw files (lines, voices, lexicon, visemes) and
takes.toml. A tiny tokenizer for the subset these files use (spec §5.2): one statement
per line, `#` comments, double-quoted strings with backslash escapes, key=value args.

This is deliberately self-contained (the compiler's parse.py is the ENGINE's); it reads
the same grammar, so the files stay one format.
"""
from __future__ import annotations

import dataclasses as dc
import os
import re
import tomllib

from . import env


class ShwError(Exception):
    def __init__(self, path, line, msg):
        super().__init__(f"{env.rel(path) if path else '?'}:{line}: {msg}")
        self.path, self.line, self.msg = path, line, msg


def _quoted(s, i, path, lineno):
    """s[i] == '"': -> (text, index after the closing quote)."""
    j, buf, n = i + 1, [], len(s)
    while j < n and s[j] != '"':
        if s[j] == "\\" and j + 1 < n:
            buf.append(s[j + 1])
            j += 2
            continue
        buf.append(s[j])
        j += 1
    if j >= n:
        raise ShwError(path, lineno, "unterminated string")
    return "".join(buf), j + 1


def tokenize(s, path=None, lineno=0):
    """'line a b "x \\"y\\"" k=v k2="q r"' -> ['line','a','b',('str','x "y"'),('kv','k','v'),...]"""
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c in " \t":
            i += 1
            continue
        if c == "#":
            break
        if c == '"':
            txt, i = _quoted(s, i, path, lineno)
            out.append(("str", txt))
            continue
        j = i
        while j < n and s[j] not in " \t=":
            j += 1
        if j < n and s[j] == "=" and j > i:
            k = s[i:j]
            if j + 1 < n and s[j + 1] == '"':
                v, i = _quoted(s, j + 1, path, lineno)
            else:
                q = j + 1
                while q < n and s[q] not in " \t":
                    q += 1
                v, i = s[j + 1:q], q
            out.append(("kv", k, v))
            continue
        while j < n and s[j] not in " \t":
            j += 1
        out.append(s[i:j])
        i = j
    return out


def statements(path):
    """Yield (lineno, tokens) for every non-empty statement of a .shw file."""
    with open(path, encoding="utf-8") as fh:
        for no, raw in enumerate(fh, 1):
            toks = tokenize(raw.rstrip("\n"), path, no)
            if toks:
                yield no, toks


def _split(toks):
    pos, kv = [], {}
    for t in toks:
        if isinstance(t, tuple) and t[0] == "kv":
            kv[t[1]] = t[2]
        elif isinstance(t, tuple) and t[0] == "str":
            pos.append(t)
        else:
            pos.append(t)
    return pos, kv


# ----------------------------------------------------------------------------- lines
@dc.dataclass
class Line:
    id: str
    speaker: str                 # cast id, or walla:<voice>
    text: str
    attrs: dict
    path: str = ""
    lineno: int = 0

    @property
    def walla(self):
        return self.speaker.startswith("walla:")

    @property
    def voice_ref(self):
        """The voice id this line asks for (before cast.shw / variants)."""
        if "voice" in self.attrs:
            return self.attrs["voice"]
        return self.speaker.split(":", 1)[1] if self.walla else self.speaker

    @property
    def lib_take(self):
        t = self.attrs.get("take", "")
        return t if t.startswith("lib:") else None


@dc.dataclass
class Bed:
    id: str
    lines: list
    attrs: dict
    path: str = ""
    lineno: int = 0


def _expand_range(spec, ids):
    """'w_kids_01..w_kids_06' -> ids in file order between the two (inclusive)."""
    out = []
    for part in spec.split(","):
        if ".." in part:
            a, b = part.split("..", 1)
            if a not in ids or b not in ids:
                raise KeyError(part)
            ia, ib = ids.index(a), ids.index(b)
            out.extend(ids[ia:ib + 1])
        else:
            out.append(part)
    return out


def read_lines(path):
    """-> (dict id->Line in file order, dict bed id->Bed)."""
    lines, beds = {}, {}
    if not os.path.exists(path):
        return lines, beds
    pending = []
    for no, toks in statements(path):
        head = toks[0]
        if head == "line":
            pos, kv = _split(toks[1:])
            if len(pos) < 3 or not isinstance(pos[2], tuple):
                raise ShwError(path, no, 'expected: line <id> <speaker> "<text>" [k=v...]')
            lid, spk, text = pos[0], pos[1], pos[2][1]
            if not re.fullmatch(r"[a-z][a-z0-9_]*", lid):
                raise ShwError(path, no, f"bad line id {lid!r} (lowercase, digits, _; starts with a letter)")
            if lid in lines:
                raise ShwError(path, no, f"duplicate line id {lid}")
            lines[lid] = Line(lid, spk, text, kv, path, no)
        elif head == "walla":
            pos, kv = _split(toks[1:])
            pending.append((no, pos[0], kv))
    ids = list(lines)
    for no, bid, kv in pending:
        try:
            members = _expand_range(kv.get("lines", ""), ids)
        except KeyError as e:
            raise ShwError(path, no, f"walla {bid}: unknown line range {e}")
        beds[bid] = Bed(bid, members, kv, path, no)
    return lines, beds


# ----------------------------------------------------------------------------- voices
@dc.dataclass
class Voice:
    id: str
    attrs: dict
    path: str = ""
    lineno: int = 0

    def get(self, k, d=None):
        return self.attrs.get(k, d)

    @property
    def engine(self):
        return self.attrs.get("engine", "kokoro")

    @property
    def age(self):
        return self.attrs.get("age")


def read_voices(path):
    out = {}
    for no, toks in statements(path):
        if toks[0] != "voice":
            continue
        pos, kv = _split(toks[1:])
        vid = pos[0]
        if vid in out:
            raise ShwError(path, no, f"duplicate voice {vid}")
        out[vid] = Voice(vid, kv, path, no)
    # variants inherit: voice bonejangles.possessed base=bonejangles flatten_f0=0.7
    for vid, v in out.items():
        b = v.attrs.get("base")
        seen = {vid}
        while b:
            if b in seen or b not in out:
                raise ShwError(path, v.lineno, f"voice {vid}: bad base {b}")
            seen.add(b)
            merged = dict(out[b].attrs)
            merged.update(v.attrs)
            merged["base"] = out[b].attrs.get("base", "")
            v.attrs = {k: x for k, x in merged.items() if not (k == "base" and not x)}
            b = v.attrs.get("base")
    return out


def read_auditions(path):
    """-> ({role: {n: settings dict}}, {role: [sample texts]}) from voices.shw."""
    opts, samples = {}, {}
    for no, toks in statements(path):
        if toks[0] == "audition":
            pos, kv = _split(toks[1:])
            opts.setdefault(pos[0], {})[int(pos[1])] = kv
        elif toks[0] == "sample":
            pos, kv = _split(toks[1:])
            samples.setdefault(pos[0], []).append(pos[1][1])
    return opts, samples


def read_cast_voices(path):
    """cast.shw (ART's file): char -> voice id, when it exists."""
    out = {}
    if not os.path.exists(path):
        return out
    for no, toks in statements(path):
        if toks[0] != "char":
            continue
        pos, kv = _split(toks[1:])
        if "voice" in kv:
            out[pos[0]] = kv["voice"]
    return out


# ----------------------------------------------------------------------------- lexicon
def read_lexicon(path):
    """lex <Word> "<kokoro phonemes>" [arpa="..."] [say="HELL-vyle"] -> {word.lower(): phon}"""
    out = {}
    if not os.path.exists(path):
        return out
    for no, toks in statements(path):
        if toks[0] != "lex":
            continue
        pos, kv = _split(toks[1:])
        if len(pos) < 2 or not isinstance(pos[1], tuple):
            raise ShwError(path, no, 'expected: lex <word> "<phonemes>"')
        out[pos[0].lower()] = pos[1][1]
    return out


# ----------------------------------------------------------------------------- visemes
def read_visemes(path):
    """-> (rules dict, {family: {viseme: MOUTH_*}})"""
    rules, fams = {}, {}
    if not os.path.exists(path):
        return rules, fams
    for no, toks in statements(path):
        pos, kv = _split(toks)
        if pos[0] == "rules":
            rules.update(kv)
        elif pos[0] == "family":
            fams[pos[1]] = kv
    return rules, fams


# ----------------------------------------------------------------------------- takes.toml
def read_takes(path):
    if not os.path.exists(path):
        return {}
    with open(path, "rb") as fh:
        return tomllib.load(fh)


def series_path(name):
    return os.path.join(env.SHOW, "series", name)
