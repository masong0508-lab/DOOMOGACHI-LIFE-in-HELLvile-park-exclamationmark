"""textpack_bridge.py - the game's own compiler, used read-only (spec 4.2).

core/tools/textpack.py is imported as a module (never edited, never run as a
script): load_symbols() gives every engine enum (SHK_*, EXPR_*, TWT_*,
PAL_*, BOP_*, SPECIES_*...), and Compiler(syms).compile_beat(pack, name, body)
compiles engine-native beat lines, so the show and the game share one
encoding of BeatEv. compile_beat wants (where, "+N op args") tuples ending in
an `end` line; it raises textpack.Fail("<where>: <msg>"), which is turned
into a spec 13.4 diagnostic here.
"""
import importlib.util
import json
import os
import re
import sys

from . import paths
from .errors import Diag, ShowError

TEXTPACK = paths.CORE / "tools" / "textpack.py"
_mod = None
_syms = None
_comp = None


def module():
    global _mod
    if _mod is None:
        spec = importlib.util.spec_from_file_location("doomo_textpack_ro", str(TEXTPACK))
        m = importlib.util.module_from_spec(spec)
        sys.modules.setdefault("doomo_textpack_ro", m)
        spec.loader.exec_module(m)
        _mod = m
    return _mod


# headers textpack's load_symbols() does not read (the catalogue, looks,
# palette, portraits): SPECIES_*, MOUTH_*, EYE_*, PAINT_*, RES_* ...
EXTRA = ["catalog.h", "resident.h", "palette.h", "portrait.h", "pet.h", "sprites.h", "actor.h", "tween.h",
         "camfx.h"]


def _add_headers(tp, syms, files):
    for path in files:
        if not os.path.exists(path):
            continue
        src = tp.strip_comments(open(path).read())
        for m in re.finditer(r"enum\s*\w*\s*\{(.*?)\}", src, flags=re.S):
            cur = -1
            for item in m.group(1).split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, val = item.split("=", 1)
                    name = name.strip()
                    v = tp.c_eval(val, syms)
                    cur = v if v is not None else cur + 1
                else:
                    name = item
                    cur += 1
                if re.match(r"^[A-Za-z_]\w*$", name):
                    syms.setdefault(name, cur)
        for m in re.finditer(r"#define[ \t]+([A-Z_][A-Z0-9_]*)[ \t]+([^\n(][^\n]*)", src):
            v = tp.c_eval(m.group(2), syms)
            if v is not None:
                syms.setdefault(m.group(1), v)


def _cache_path():
    return paths.BUILD / "cache" / "symbols.json"


def symbols():
    """every engine symbol (cached in show/build/cache/ by the headers' mtimes)"""
    global _syms
    if _syms is not None:
        return _syms
    tp = module()
    hdrs = [os.path.join(tp.INC, h) for h in list(tp.HEADERS) + EXTRA] + list(tp.EXTRA_HEADERS)
    stamp = [[h, os.path.getmtime(h) if os.path.exists(h) else 0] for h in hdrs + [str(TEXTPACK)]]
    cp = _cache_path()
    try:
        d = json.loads(cp.read_text())
        if d.get("stamp") == stamp:
            _syms = d["syms"]
            return _syms
    except (OSError, ValueError, KeyError):
        pass
    _syms = tp.load_symbols()
    _add_headers(tp, _syms, [os.path.join(tp.INC, h) for h in EXTRA])
    try:
        cp.parent.mkdir(parents=True, exist_ok=True)
        cp.write_text(json.dumps({"stamp": stamp, "syms": _syms}))
    except OSError:
        pass
    return _syms


# staged names that are another engine name (the show's synonyms, not new engine constants)
SYNONYMS = {"TWT_POP_SMALL": "TWT_POP_LETTER", "PAL_RED": "PAL_UI_RED", "PAL_RED_D": "PAL_UI_RED_D",
            "PAL_GOLD": "PAL_UI_GOLD", "PAL_PURPLE": "PAL_UI_PURPLE", "PAL_PINK": "PAL_UI_PINK",
            "PAL_CREAM": "PAL_UI_CREAM", "PAL_MINT": "PAL_UI_MINT"}


def sym(name, where=None, prefix=None):
    """an engine constant by name (or a number); unknown -> a file:line error"""
    s = str(name)
    if re.fullmatch(r"-?\d+", s) or re.fullmatch(r"0x[0-9a-fA-F]+", s):
        return int(s, 0)
    S = symbols()
    if s not in S and s in SYNONYMS and SYNONYMS[s] in S:
        s = SYNONYMS[s]
    if s in S:
        if prefix and not s.startswith(prefix):
            raise ShowError(Diag("SYM", "%s is not a %s* constant" % (s, prefix), **(where or {})))
        return S[s]
    if prefix and prefix + s.upper() in S:
        return S[prefix + s.upper()]
    near = sorted(k for k in S if prefix and k.startswith(prefix))[:12]
    hint = (" (the %s* names: %s%s)" % (prefix, ", ".join(near), ", ..." if len(near) == 12 else "")) if near else ""
    raise ShowError(Diag("SYM", "unknown engine name %s%s" % (s, hint), **(where or {})))


class _StubPack:
    """what compile_beat touches on a pack: .path and .text() (SAY/CAPTION only;
    the show never sends text through BOP_SAY)"""

    def __init__(self, path):
        self.path = path
        self.id = 0
        self.strings = []

    def text(self, s, where):
        self.strings.append(s)
        return len(self.strings) - 1


def compiler():
    global _comp
    if _comp is None:
        tp = module()
        _comp = tp.Compiler.__new__(tp.Compiler)       # no push_args file read: not needed here
        _comp.syms = symbols()
        _comp.packs, _comp.script_names, _comp.beat_names = [], {}, {}
        _comp.missions, _comp.possessors, _comp.armed, _comp.spots = [], [], [], []
        _comp.push_args = None
    return _comp


_WHERE = re.compile(r"^(?P<file>[^:]+):(?P<line>\d+): (?P<msg>.*)$", re.S)


def compile_beat(lines, name="shot", path="show"):
    """lines: [(where_dict, '+N op args')] -> [(at, anchor, op, a, b)] sorted.
    An `end` line must be among them (the caller adds `+len end`)."""
    tp = module()
    body = [("%s:%d" % (w.get("file", path), w.get("line", 0)), raw) for w, raw in lines]
    try:
        return compiler().compile_beat(_StubPack(path), name, body)
    except tp.Fail as e:
        m = _WHERE.match(str(e))
        if m:
            raise ShowError(Diag("SYM" if "unknown name" in m.group("msg") else "BEAT",
                                 m.group("msg") + " (core/tools/textpack.py compile_beat)",
                                 file=m.group("file"), line=int(m.group("line"))))
        raise ShowError(Diag("BEAT", str(e)))
    except (ValueError, IndexError) as e:
        w = lines[-1][0] if lines else {}
        raise ShowError(Diag("BEAT", "bad engine op arguments: %s" % e, **w))
