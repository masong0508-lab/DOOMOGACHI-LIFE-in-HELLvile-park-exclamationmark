"""setfile.py - a reader for show/series/sets/*.shw (the grammar is in SETS.md).

Top level:   set ... end | bands <id> rows=... | nib <id> ... | palette ... (ignored)
Inside set:  plane, prop, strokes ... end (with stroke lines), path, mark, frame,
             variant, cam3d, obj (ROOM furniture), note, waive
Errors name file:line.
"""
import glob
import os
import re

from . import SETS_DIR


class SetError(Exception):
    pass


def tokenize(line):
    """Split on blanks outside quotes and parentheses; drop a # comment."""
    toks, cur, depth, q = [], "", 0, False
    for ch in line:
        if q:
            cur += ch
            if ch == '"':
                q = False
            continue
        if ch == '"':
            q = True
            cur += ch
        elif ch == "#" and depth == 0:
            break
        elif ch in "([":
            depth += 1
            cur += ch
        elif ch in ")]":
            depth -= 1
            cur += ch
        elif ch.isspace() and depth == 0:
            if cur:
                toks.append(cur)
                cur = ""
        else:
            cur += ch
    if cur:
        toks.append(cur)
    return toks


def kv(toks):
    args, pos = {}, []
    for t in toks:
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_.]*)=(.*)$", t)
        if m:
            v = m.group(2)
            if v.startswith('"') and v.endswith('"'):
                v = v[1:-1]
            args[m.group(1)] = v
        else:
            pos.append(t[1:-1] if t.startswith('"') and t.endswith('"') else t)
    return args, pos


class Stmt:
    def __init__(self, kw, ident, args, pos, where):
        self.kw, self.id, self.args, self.pos, self.where = kw, ident, args, pos, where

    def get(self, k, d=None):
        return self.args.get(k, d)

    def __repr__(self):
        return "<%s %s %s>" % (self.kw, self.id, self.args)


class SetDef:
    def __init__(self, ident, args, where):
        self.id, self.args, self.where = ident, args, where
        self.kind = args.get("kind", "2d")
        self.px = int(args.get("px", "1"))
        self.planes, self.props, self.strokes, self.paths = [], [], [], []
        self.marks, self.frames, self.variants, self.cams, self.notes = [], [], [], [], []
        self.objs = []
        self.ambs = []

    def plane(self, pid):
        for p in self.planes:
            if p.id == pid:
                return p
        return None

    def variant(self, vid):
        for v in self.variants:
            if v.id == vid:
                return v
        return None

    def __repr__(self):
        return "<set %s %s planes=%d props=%d variants=%s>" % (
            self.id, self.kind, len(self.planes), len(self.props), [v.id for v in self.variants])


class Library:
    def __init__(self):
        self.sets, self.bands, self.nibs, self.files = {}, {}, {}, []

    def load_dir(self, d=SETS_DIR):
        for f in sorted(glob.glob(os.path.join(d, "*.shw"))):
            self.load(f)
        return self

    def load(self, path):
        self.files.append(path)
        rel = os.path.relpath(path, os.path.dirname(os.path.dirname(os.path.dirname(path))))
        cur, group = None, None
        for n, raw in enumerate(open(path), 1):
            where = "%s:%d" % (rel, n)
            toks = tokenize(raw)
            if not toks:
                continue
            kw = toks[0]
            if kw == "end":
                if group is not None:
                    group = None
                elif cur is not None:
                    cur = None
                else:
                    raise SetError("%s: 'end' without a block" % where)
                continue
            ident = toks[1] if len(toks) > 1 and "=" not in toks[1] else None
            args, pos = kv(toks[2:] if ident else toks[1:])
            st = Stmt(kw, ident, args, pos, where)
            if cur is None:
                if kw == "set":
                    if not ident:
                        raise SetError("%s: set needs an id" % where)
                    if ident in self.sets:
                        raise SetError("%s: set '%s' already defined at %s" % (
                            where, ident, self.sets[ident].where))
                    cur = SetDef(ident, args, where)
                    cur.file = path
                    self.sets[ident] = cur
                elif kw == "bands":
                    self.bands[ident] = st
                elif kw == "nib":
                    self.nibs[ident] = st
                elif kw in ("palette", "note"):
                    pass
                else:
                    raise SetError("%s: unknown top-level statement '%s'" % (where, kw))
                continue
            if group is not None:
                if kw != "stroke":
                    raise SetError("%s: only 'stroke' lines go inside strokes ... end" % where)
                group.strokes.append(st)
                continue
            if kw == "plane":
                cur.planes.append(st)
            elif kw == "prop":
                cur.props.append(st)
            elif kw == "strokes":
                st.strokes = []
                cur.strokes.append(st)
                cur.planes.append(st)
                group = st
            elif kw == "path":
                cur.paths.append(st)
            elif kw == "mark":
                cur.marks.append(st)
            elif kw == "frame":
                cur.frames.append(st)
            elif kw == "variant":
                cur.variants.append(st)
            elif kw == "cam3d":
                cur.cams.append(st)
            elif kw == "obj":
                cur.objs.append(st)
            elif kw == "amb":
                cur.ambs.append(st)
            elif kw in ("note", "waive", "lock"):
                cur.notes.append(st)
            else:
                raise SetError("%s: unknown statement '%s' in set %s" % (where, kw, cur.id))
        if cur is not None:
            raise SetError("%s: set '%s' is missing its 'end'" % (path, cur.id))
        return self


def xy(s, default=(0, 0)):
    if s is None:
        return default
    a, _, b = s.partition(",")
    return (float(a), float(b or 0))


def wh(s):
    a, _, b = s.lower().partition("x")
    return int(a), int(b)


def split_list(s):
    return [t for t in (s or "").split(",") if t]
