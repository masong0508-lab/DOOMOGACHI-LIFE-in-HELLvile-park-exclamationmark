"""showtk.art.cast.acting - the acting library, show/series/acting.shw (ART).

    macro <name> <actor param> [<param>=<default> ...]
      <time> <op> <args> [; <op> <args> ...]
    end

Times are relative to the frame a shot's `take <macro> <actor> [k=v ...]`
fires: `+N`, `+R<rung>` (R2 R3 R5 R8 R13 R21 R26) or `+$param`.  Ops are
the picture ops of SHOW_ENGINE_SPEC.md sec.5.5 (motion, acting, effects);
sound ops are refused (the shot owns every sound).  Engine constants
(EXPR_*, TWT_*, SHK_*, PAL_*, TINT_*) are checked against the game's headers.

    Library().expand("freeze", "mayor", n="21") -> [(0, "freeze mayor"), (21, "thaw mayor")]

`python3 -m showtk.art.cast.acting` checks the file and prints every macro
expanded on a sample actor.
"""
import os
import re
import sys

from .catalog import catalog
from .lookdata import SERIES, LookError, parse_file, tokens

RUNGS = {"R2": 2, "R3": 3, "R5": 5, "R8": 8, "R13": 13, "R21": 21, "R26": 26}
# op -> (minimum args incl. the actor, which args are symbols of which prefix)
OPS = {
    "expr": (2, {1: "EXPR_"}),
    "pop": (2, {1: "TWT_"}),
    "headturn": (3, {}),
    "squash": (2, {}),
    "move": (2, {}),
    "walk": (2, {}),
    "face": (2, {}),
    "pose": (2, {}),
    "blink": (1, {}),
    "still": (1, {}),
    "freeze": (1, {}),
    "thaw": (1, {}),
    "panic": (1, {}),
    "twitch": (2, {}),
    "state": (2, {}),
    "shake": (1, {0: "SHK_"}),
    "punch": (2, {}),
    "flash": (3, {0: "PAL_"}),
    "hitstop": (1, {}),
    "zoom": (2, {}),
    "tint": (2, {0: "TINT_"}),
    "pal": (2, {0: "PAL_"}),
}
ACTOR_FIRST = {"expr", "pop", "headturn", "squash", "move", "walk", "face", "pose", "blink",
               "still", "freeze", "thaw", "panic", "twitch", "state"}
SOUND = {"sfx", "fol", "amb", "mx", "tapestop", "say", "hum", "walla", "cc", "music", "mute", "duck"}
STATES = {"possessed", "ghost", "dead", "normal"}
TIME = re.compile(r"^\+(\d+|R\d+|\$\w+)$")
PARAM = re.compile(r"\$(\w+)")


class Macro:
    def __init__(self, name, where):
        self.name = name
        self.where = where
        self.actor = None
        self.params = {}
        self.lines = []         # [(where, time text, [op text, ...])]


class Library:
    def __init__(self, path=os.path.join(SERIES, "acting.shw")):
        self.path = path
        self.macros = {}
        self.errors = []
        try:
            stmts = parse_file(path, keep_raw_blocks=("macro",))
        except LookError as e:
            self.errors.append(str(e))
            return
        for st in stmts:
            if st.kw != "macro":
                self.errors.append("%s: acting.shw holds `macro` blocks (got `%s`)" % (st.where, st.kw))
                continue
            self._load(st)

    def _load(self, st):
        if len(st.words) != 2:
            self.errors.append("%s: `macro <name> <actor param> [k=default ...]`" % st.where)
            return
        m = Macro(st.words[0], st.where)
        if m.name in self.macros:
            self.errors.append("%s: macro %s already defined at %s" % (st.where, m.name, self.macros[m.name].where))
            return
        m.actor = st.words[1]
        for k, v, _w in st.kv:
            m.params[k] = v
        for where, text in st.raw:
            parts = text.split(None, 1)
            if len(parts) != 2:
                self.errors.append("%s: `<time> <op> <args>`" % where)
                continue
            m.lines.append((where, parts[0], [p.strip() for p in parts[1].split(";") if p.strip()]))
        self.macros[m.name] = m
        self._check(m)

    def _check(self, m):
        if not m.lines:
            self.errors.append("%s: macro %s is empty" % (m.where, m.name))
        names = set(m.params) | {m.actor}
        last = -1
        for where, t, ops in m.lines:
            for p in PARAM.findall(t + " " + " ".join(ops)):
                if p not in names:
                    self.errors.append("%s: macro %s uses $%s, not a parameter (%s)" %
                                       (where, m.name, p, ", ".join(sorted(names))))
            try:
                f = self.frame(m, t, dict(m.params))
            except LookError as e:
                self.errors.append(str(e).replace("?", where, 1))
                continue
            if f < last:
                self.errors.append("%s: macro %s times must not go backwards (+%d after +%d)" % (where, m.name, f, last))
            last = f
            for op in ops:
                err = self.check_op(m, op, dict(m.params))
                if err:
                    self.errors.append("%s: macro %s: %s" % (where, m.name, err))

    def frame(self, m, t, params):
        if not TIME.match(t):
            raise LookError("?", "time %r: +N, +R<rung> or +$param" % t)
        v = t[1:]
        if v.startswith("$"):
            v = params.get(v[1:], "")
            v = v[1:] if v.startswith("+") else v
        if v.isdigit():
            return int(v)
        if v in RUNGS:
            return RUNGS[v]
        raise LookError("?", "time %r resolves to %r: not a frame count or rung" % (t, v))

    def subst(self, m, text, params, actor):
        def rep(mo):
            k = mo.group(1)
            return actor if k == m.actor else params.get(k, mo.group(0))
        return PARAM.sub(rep, text)

    def check_op(self, m, op, params):
        toks = [t[1] if t[0] != "kv" else "%s=%s" % (t[1], t[2]) for t in tokens(self.subst(m, op, params, "$" + m.actor))]
        name = toks[0]
        if name in SOUND:
            return "sound op `%s` in a macro (the shot owns every sound)" % name
        if name == "eyes":
            return "`eyes` is deferred with gaze (sec.4.3); use headturn"
        if name not in OPS:
            return "unknown op `%s` (%s)" % (name, " ".join(sorted(OPS)))
        nmin, syms = OPS[name]
        args = toks[1:]
        if len(args) < nmin:
            return "`%s` needs %d argument(s)" % (name, nmin)
        if name in ACTOR_FIRST and args[0] != "$" + m.actor:
            return "`%s` acts on the macro's actor $%s (got %s)" % (name, m.actor, args[0])
        cat = catalog()
        for i, pre in syms.items():
            a = args[i]
            if not a.startswith(pre) or cat.value(a) is None:
                return "`%s`: %s is not a %s* value of the game's headers" % (name, a, pre)
        if name == "state" and args[1] not in STATES:
            return "state %s: %s" % (args[1], "|".join(sorted(STATES)))
        if name == "twitch" and args[1] not in ("on", "off"):
            return "twitch on|off"
        if name == "headturn":
            for a in args[1:3]:
                if not a.isdigit():
                    return "headturn A <brad 0..255> <frames>"
            if int(args[1]) > 255:
                return "headturn brad is 0..255 (64 = a quarter turn)"
        if name == "squash" and (not args[1].isdigit() or not any(a.startswith("over=") for a in args)):
            return "squash A <q8 scale> over=<frames>"
        return None

    def expand(self, name, actor, **kw):
        """[(frame, op text)] for `take <name> <actor> k=v ...`"""
        m = self.macros[name]
        params = dict(m.params)
        for k, v in kw.items():
            if k not in m.params:
                raise LookError(m.where, "macro %s has no parameter %s" % (name, k))
            params[k] = str(v)
        out = []
        for where, t, ops in m.lines:
            f = self.frame(m, t, params)
            for op in ops:
                out.append((f, self.subst(m, op, params, actor)))
        return out


def main(argv):
    lib = Library()
    for e in lib.errors:
        print("ERROR", e)
    for name in sorted(lib.macros):
        if "-v" in argv:
            print("%s: %s" % (name, " | ".join("+%d %s" % fo for fo in lib.expand(name, "mayor"))))
    print("%d macros, %d errors" % (len(lib.macros), len(lib.errors)))
    return 1 if lib.errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
