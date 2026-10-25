"""parse.py - the .shw reader (spec 5.2).

One statement per line; `;` chains statements on one line (a leading time
applies to every one of them); `#` starts a comment outside quotes;
indentation is ignored; a line whose first token is `key=value` continues the
statement above it (the cast and crowd excerpts write headers over two lines);
a block ends at `end`. Which keywords open a block depends on where they
stand (`look` is a block in looks/*.shw and a setup statement in a shot):

    top level   shot set macro look kset show cue act
    in a shot   crowd
    in a set    strokes

`crowd` and `act` are soft blocks: the `end` of an act is optional (spec 6.2 writes
none; the staging files close it): a new `act` or the end of the file closes
it. A crowd's body is key=value lines then `end`; a crowd line followed by any
other statement was a one-liner and is closed there.

Every Stmt keeps its file and line, so every later diagnostic can name them.
"""
import re

from .errors import Diag, ShowError

BLOCKS = {
    None: {"shot", "set", "macro", "look", "kset", "show", "cue", "act"},
    "act": {"shot", "set", "macro", "look", "kset", "show", "cue"},
    "shot": {"crowd"},
    "set": {"strokes"},
}

WARNINGS = []     # tolerated oddities found while parsing (the compiler reports them)

# a leading time: +N, +R8, +1.5s, +N..+M, >+N, @LABEL[+N], @line.end+3, @bar:2.1, tc:..
TIME_RE = re.compile(r"^(>?\+[\w.:$]+(\.\.\+[\w.$]+)?|@[\w.:]+([+-][\w.$]+)?)$")
DIRECTIVES = {"@episode"}


class Tok:
    __slots__ = ("kind", "key", "value", "quoted")

    def __init__(self, kind, value, key=None, quoted=False):
        self.kind = kind          # 'word' | 'str' | 'kv'
        self.key = key
        self.value = value
        self.quoted = quoted

    def __repr__(self):
        if self.kind == "kv":
            return "%s=%r" % (self.key, self.value)
        return repr(self.value) if self.kind == "str" else self.value


class Stmt:
    """one statement: kw, positional tokens, key=value options, children"""

    def __init__(self, kw, toks, file, line, time=None):
        self.kw = kw
        self.toks = toks
        self.file = file
        self.line = line
        self.time = time
        self.children = []
        self.parent = None
        self.end_line = line

    # positional words and strings, in order
    @property
    def pos(self):
        return [t.value for t in self.toks if t.kind != "kv"]

    @property
    def words(self):
        return [t.value for t in self.toks if t.kind == "word"]

    @property
    def strs(self):
        return [t.value for t in self.toks if t.kind == "str"]

    @property
    def kv(self):
        d = {}
        for t in self.toks:
            if t.kind == "kv":
                d[t.key] = t.value
        return d

    def get(self, key, default=None):
        for t in reversed(self.toks):
            if t.kind == "kv" and t.key == key:
                return t.value
        return default

    def has(self, key):
        return any(t.kind == "kv" and t.key == key for t in self.toks)

    def where(self):
        return {"file": self.file, "line": self.line}

    def diag(self, rule, msg, severity="error", **kw):
        return Diag(rule, msg, file=self.file, line=self.line, severity=severity, **kw)

    def fail(self, rule, msg, **kw):
        raise ShowError(self.diag(rule, msg, **kw))

    def text(self):
        out = ([self.time] if self.time else []) + [self.kw]
        for t in self.toks:
            if t.kind == "kv":
                out.append("%s=%s" % (t.key, ('"%s"' % t.value) if t.quoted else t.value))
            elif t.kind == "str":
                out.append('"%s"' % t.value)
            else:
                out.append(t.value)
        return " ".join(out)

    def __repr__(self):
        return "<%s:%d %s>" % (self.file, self.line, self.text())


def strip_comment(raw):
    q = False
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == "\\" and q:
            i += 2
            continue
        if c == '"':
            q = not q
        elif c == "#" and not q:
            return raw[:i]
        i += 1
    return raw


def split_semis(line):
    out, cur, q, i = [], [], False, 0
    while i < len(line):
        c = line[i]
        if c == "\\" and q and i + 1 < len(line):
            cur.append(line[i:i + 2])
            i += 2
            continue
        if c == '"':
            q = not q
        if c == ";" and not q:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    out.append("".join(cur))
    return out


_TOK = re.compile(r'\s*(?:([A-Za-z_][\w.\-]*)=("((?:[^"\\]|\\.)*)"|\S*)|"((?:[^"\\]|\\.)*)"|(\S+))')


def _unesc(s):
    return s.replace('\\"', '"').replace("\\\\", "\\")


def tokenize(text, file="?", line=0):
    toks = []
    pos = 0
    text = text.rstrip()
    while pos < len(text):
        m = _TOK.match(text, pos)
        if not m or m.end() == pos:
            raise ShowError(Diag("PARSE", "cannot read %r" % text[pos:], file=file, line=line))
        pos = m.end()
        if m.group(1) is not None:
            if m.group(3) is not None:
                toks.append(Tok("kv", _unesc(m.group(3)), key=m.group(1), quoted=True))
            else:
                v = m.group(2)
                if v.startswith('"'):
                    raise ShowError(Diag("PARSE", "unterminated string in %s=" % m.group(1), file=file, line=line))
                toks.append(Tok("kv", v, key=m.group(1)))
        elif m.group(4) is not None:
            toks.append(Tok("str", _unesc(m.group(4)), quoted=True))
        elif m.group(5) is not None:
            w = m.group(5)
            if w.startswith('"'):
                raise ShowError(Diag("PARSE", "unterminated string", file=file, line=line))
            toks.append(Tok("word", w))
    return toks


def parse_text(text, file="?"):
    """-> [Stmt] (top level, children nested)"""
    root = Stmt("<root>", [], file, 0)
    stack = [root]
    last = None
    for ln, raw in enumerate(text.split("\n"), 1):
        body = strip_comment(raw).strip()
        if not body:
            continue
        pieces = split_semis(body)
        time = None
        for k, piece in enumerate(pieces):
            toks = tokenize(piece, file, ln)
            if not toks:
                continue
            if k == 0 and toks[0].kind == "kv" and last is not None:
                last.toks += toks          # continuation of the statement above
                last.end_line = ln
                continue
            if toks[0].kind == "word" and TIME_RE.match(toks[0].value) and toks[0].value not in DIRECTIVES:
                if k == 0:
                    time = toks[0].value
                    toks = toks[1:]
                else:
                    # `+60 sfx A ; +70 sfx B`: the piece keeps its own time (spec 5.2 means `;` for
                    # ops at the same time; tolerated, with a warning, so the shot still compiles)
                    WARNINGS.append(Diag("PARSE", "a new time after `;` (%s): put it on its own line" % toks[0].value,
                                         file=file, line=ln, severity="warning"))
                    time = toks[0].value
                    toks = toks[1:]
            if not toks:
                raise ShowError(Diag("PARSE", "a time with no statement", file=file, line=ln))
            if toks[0].kind != "word":
                raise ShowError(Diag("PARSE", "a statement starts with a keyword, not %r" % toks[0], file=file, line=ln))
            kw = toks[0].value
            parent = stack[-1]
            if parent.kw == "crowd" and kw != "end":
                stack.pop()                    # a one-line crowd (no body, no end)
                parent = stack[-1]
            if kw == "end" and time is None and len(toks) == 1:
                if len(stack) == 1:
                    raise ShowError(Diag("PARSE", "`end` with no open block", file=file, line=ln))
                stack[-1].end_line = ln
                stack.pop()
                last = None
                continue
            if kw == "act" and time is None and parent.kw == "act":
                stack.pop()                    # a new act closes the open one
                parent = stack[-1]
            st = Stmt(kw, toks[1:], file, ln, time=time)
            st.parent = parent if parent is not root else None
            parent.children.append(st)
            last = st
            ctx = parent.kw if parent is not root else None
            if time is None and kw in BLOCKS.get(ctx, ()):
                stack.append(st)
    if len(stack) == 2 and stack[-1].kw == "act":
        stack.pop()
    if len(stack) > 1:
        b = stack[-1]
        raise ShowError(Diag("PARSE", "`%s %s` has no `end`" % (b.kw, " ".join(map(str, b.pos[:1]))),
                             file=file, line=b.line))
    return root.children


def flatten(stmts):
    """top-level statements with every `act` block's children spliced in after the act"""
    out = []
    for st in stmts:
        out.append(st)
        if st.kw == "act":
            out += st.children
    return out


def parse_file(path):
    from pathlib import Path
    p = Path(path)
    return parse_text(p.read_text(encoding="utf-8"), file=str(p))
