"""timeexpr.py - time expressions (spec 5.3) and the anchor solver.

    +N  +R8  +1.5s          frames from the shot's frame 0
    +N..+M                  a span (ops with a duration take it: over=+30..+132)
    >+N  >+R8               N frames after the previous timed line in the shot
    @LABEL[+N]              from a `label` (or `punch` / `button`) in the shot
    @<line>.start|.end|.w<k>|.p<k>[+N]   a dialogue line's sound (voice.lock,
                            else a provisional per-voice estimate)
    @bar:B.b[+N]            bar B beat b of the shot's cue (`cue=<cue>@bar:B`)
    @<shot>.end[+N]  @<shot>+N   another shot's frame (must land in this shot)
    @tc:HH:MM:SS:FF         (episode-level files) programme time
    @<beat>+N               (episode-level files) frames from a beat's start

parse() turns the text into a small tuple; Solver resolves a shot's timed
statements in dependency order, detects cycles and prints the anchor chain.
"""
import re

from . import docunits
from .errors import Diag, ShowError

_DUR = r"(?:\d+(?:\.\d+)?s|\d+smp|R\d+|\d+)"
_OFF = r"(?P<off>[+-]" + _DUR + r")?"


def _dur(s, where):
    sign = -1 if s.startswith("-") else 1
    return sign * docunits.frames(s.lstrip("+-"), where)


def parse(text, where=None):
    """-> ('rel', n) | ('span', a, b) | ('chain', n) | ('ref', name, field, offset)
          | ('bar', B, b, offset) | ('tc', frame, offset)"""
    w = where or {}
    t = text.strip()
    m = re.fullmatch(r"\+(" + _DUR + r")\.\.\+(" + _DUR + r")", t)
    if m:
        return ("span", docunits.frames(m.group(1), w), docunits.frames(m.group(2), w))
    m = re.fullmatch(r"\+(" + _DUR + ")", t)
    if m:
        return ("rel", docunits.frames(m.group(1), w))
    m = re.fullmatch(r">\+(" + _DUR + ")", t)
    if m:
        return ("chain", docunits.frames(m.group(1), w))
    m = re.fullmatch(r"@bar:(\d+)(?:\.(\d+))?" + _OFF, t)
    if m:
        return ("bar", int(m.group(1)), int(m.group(2) or 1), _dur(m.group("off"), w) if m.group("off") else 0)
    m = re.fullmatch(r"@tc:(\d+:\d+:\d+:\d+)" + _OFF, t)
    if m:
        return ("tc", docunits.parse_tc(m.group(1), w), _dur(m.group("off"), w) if m.group("off") else 0)
    m = re.fullmatch(r"@(\w+):bar:(\d+)(?:\.(\d+))?" + _OFF, t)
    if m:
        return ("cuebar", m.group(1), int(m.group(2)), int(m.group(3) or 1),
                _dur(m.group("off"), w) if m.group("off") else 0)
    m = re.fullmatch(r"@([A-Za-z_]\w*)(?:\.(start|end|[wp]\d+))?" + _OFF, t)
    if m:
        return ("ref", m.group(1), m.group(2), _dur(m.group("off"), w) if m.group("off") else 0)
    raise ShowError(Diag("TIME", "not a time expression: %r (spec 5.3: +N, +R8, +1.5s, >+N, @LABEL+N, "
                                 "@<line>.end, @bar:B.b, @<shot>.end)" % text, **w))


def is_time(text):
    try:
        parse(text)
        return True
    except ShowError:
        return False


class Solver:
    """Resolve the times of a shot's timed statements.

    items: list of objects with .time_ast, .where (dict file/line), .text
    lookups (callables, return None when the name is unknown):
      label(name) -> index of the item that defines it
      line(name, field) -> (index of the `say` item, offset from its frame, provisional)
      shot(name, field) -> episode frame of that shot's frame 0 / end
      bar(B, b) -> shot frame
    shot_start: the shot's episode frame (cross-shot refs are converted)
    """

    def __init__(self, items, label, line, shot, bar, shot_start, shot_len, shot_id):
        self.items = items
        self.label = label
        self.line = line
        self.shot = shot
        self.bar = bar
        self.start = shot_start
        self.len = shot_len
        self.shot_id = shot_id
        self.frames = [None] * len(items)
        self.prov = [False] * len(items)
        self._busy = set()

    def solve(self):
        for i in range(len(self.items)):
            self.frame_of(i, [])
        return self.frames

    def _fail(self, i, msg, chain):
        it = self.items[i]
        if chain:
            msg += "\n  anchor chain: " + " -> ".join(
                "%s:%d `%s`" % (self.items[j].where.get("file", "?").split("/")[-1],
                                self.items[j].where.get("line", 0), self.items[j].text) for j in chain + [i])
        raise ShowError(Diag("TIME-ANCHOR", msg, shot=self.shot_id, **it.where))

    def frame_of(self, i, chain):
        if self.frames[i] is not None:
            return self.frames[i]
        if i in self._busy:
            self._fail(i, "the anchors form a cycle", chain[chain.index(i):] if i in chain else chain)
        self._busy.add(i)
        chain = chain + [i]
        it = self.items[i]
        a = it.time_ast
        kind = a[0]
        prov = False
        if kind in ("rel", "span"):
            f = a[1]
        elif kind == "chain":
            if i == 0:
                self._fail(i, "`>+N` needs a timed line before it in the shot", [])
            f = self.frame_of(i - 1, chain) + a[1]
            prov = self.prov[i - 1]
        elif kind == "bar":
            f = self.bar(a[1], a[2], it)
            if f is None:
                self._fail(i, "@bar:%d.%d: the shot has no cue (add cue=<cue>@bar:B to the header, or an "
                              "`mx <cue> start` before it)" % (a[1], a[2]), [])
            f += a[3]
        elif kind == "tc":
            f = a[1] - self.start + a[2]
        elif kind == "ref":
            name, field, off = a[1], a[2], a[3]
            j = self.label(name) if field is None else None
            if j is not None:
                f = self.frame_of(j, chain) + off
                prov = self.prov[j]
            else:
                ln = self.line(name, field or "start")
                if ln is not None:
                    j, dl, lprov = ln
                    f = self.frame_of(j, chain) + dl + off
                    prov = lprov or self.prov[j]
                else:
                    ef = self.shot(name, field)
                    if ef is None:
                        self._fail(i, "@%s%s: no label, line or shot is called %s" % (
                            name, "." + field if field else "", name), chain[:-1])
                    f = ef - self.start + off
        else:
            self._fail(i, "a %s time cannot be used in a shot" % kind, [])
        if f < 0 or f > self.len:
            self._fail(i, "`%s` resolves to %+d, outside the shot (0..+%d)" % (it.text, f, self.len), chain[:-1])
        self._busy.discard(i)
        self.frames[i] = f
        self.prov[i] = prov
        return f
