"""macros.py - `take <macro> <actor> k=v...` (spec 5.5; series/acting.shw).

    macro <name> <actor param> [<param>=<default> ...]
      <time> <op> <args> [; <op> <args>]      times relative to the take: +N, +R8, +$param
    end

A shot's `+40 take freeze mayor n=21` expands in place: every op of the macro
lands at take frame + its own offset, with `$A` (the actor param) and every
`$param` substituted. Macros may call macros (`take` inside a macro), up to
depth 4. Every expanded statement keeps the shot line it came from, and its
text says which macro line made it.
"""
import copy
import re

from . import docunits, parse
from .errors import Diag, ShowError

MAX_DEPTH = 4


class MacroMissing(ShowError):
    """an unknown macro: reported as a warning, the take does nothing"""


class Macro:
    def __init__(self, st):
        if len(st.words) < 2:
            st.fail("MACRO", "`macro <name> <actor param> [k=v...]`")
        self.name = st.words[0]
        self.actor = st.words[1]
        self.defaults = dict(st.kv)
        self.stmt = st
        self.body = []                      # [(time text, Stmt)]
        for c in st.children:
            t = c.time
            kw, toks = c.kw, c.toks
            if t is None and re.match(r"^[+>@]", kw):
                # `+$n thaw $A`: the parser saw `+$n` as the keyword
                if not toks or toks[0].kind != "word":
                    c.fail("MACRO", "a macro line is `<time> <op> <args>`")
                t, kw, toks = kw, toks[0].value, toks[1:]
            if t is None:
                c.fail("MACRO", "macro %s: every line needs a time (+N, +R8, +$param)" % self.name)
            c2 = copy.copy(c)
            c2.kw, c2.toks, c2.time = kw, toks, t
            self.body.append((t, c2))


def load(stmts, diags=None):
    out = {}
    for st in stmts:
        if st.kw == "macro":
            try:
                m = Macro(st)
            except ShowError as e:
                if diags is None:
                    raise
                diags += e.diags
                continue
            if m.name in out:
                st.fail("MACRO", "macro %s is already defined at %s:%d" % (m.name, out[m.name].stmt.file,
                                                                         out[m.name].stmt.line))
            out[m.name] = m
    return out


def _sub(text, env, where):
    def rep(m):
        k = m.group(1)
        if k not in env:
            raise ShowError(Diag("MACRO", "$%s is not a parameter of the macro" % k, **where))
        return str(env[k])
    return re.sub(r"\$(\w+)", rep, text)


def expand(take, macros, at_frame, depth=0):
    """take: the shot's `take` Stmt (time already resolved to at_frame).
    -> [(frame, Stmt)] with the macro's ops, substituted."""
    w = take.where()
    if depth >= MAX_DEPTH:
        raise ShowError(Diag("MACRO", "macros nest deeper than %d (a macro calls itself?)" % MAX_DEPTH, **w))
    if not take.words:
        take.fail("MACRO", "`take <macro> <actor> [k=v...]`")
    name = take.words[0]
    m = macros.get(name)
    if m is None:
        near = ", ".join(sorted(macros)[:20])
        raise MacroMissing(Diag("MACRO", "unknown macro %s (series/acting.shw has: %s): the take is skipped" % (
            name, near or "none"), severity="warning", **w))
    if len(take.words) < 2:
        take.fail("MACRO", "take %s needs the actor (`take %s <actor>`)" % (name, name))
    env = dict(m.defaults)
    for k, v in take.kv.items():
        if k not in m.defaults:
            take.fail("MACRO", "macro %s has no parameter %s (it takes: %s)" % (
                name, k, ", ".join(sorted(m.defaults)) or "only the actor"))
        env[k] = v
    env[m.actor] = take.words[1]
    out = []
    for t, st in m.body:
        tt = _sub(t, env, w)
        mm = re.fullmatch(r"\+([\w.]+)", tt)
        if not mm:
            raise ShowError(Diag("MACRO", "macro %s line %d: time %r must be +N, +R8 or +$param" % (
                name, st.line, t), **w))
        f = at_frame + docunits.frames(mm.group(1), w)
        toks = []
        for tk in st.toks:
            nt = parse.Tok(tk.kind, _sub(tk.value, env, w), key=tk.key, quoted=tk.quoted)
            toks.append(nt)
        kw = _sub(st.kw, env, w)
        ns = parse.Stmt(kw, toks, take.file, take.line, time="+%d" % f)
        ns.parent = take.parent
        ns.from_macro = "%s (%s:%d)" % (name, st.file.split("/")[-1], st.line)
        if kw == "take":
            out += expand(ns, macros, f, depth + 1)
        else:
            out.append((f, ns))
    return out
