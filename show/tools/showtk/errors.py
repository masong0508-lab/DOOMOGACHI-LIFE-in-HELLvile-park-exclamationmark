"""errors.py - the show's one diagnostic format (spec 13.4).

Every tool reports problems the same way, so Claude can jump to the line
and the creator can read it:

    show/ep101/shots/co.shw:88: co15 +153 [AUD-OVERRUN] line co15_perfect (36 f) ends at +189, after the shot's
      tail hold starts (+180, `button`). Move it to +141, use a shorter take, or set retime="take" in takes.toml.

    <file>:<line>: <shot> +<frame> [<RULE>] <message>      (each part optional)
    warnings put "warning: " before the message; long messages wrap with a
    two-space indent.

Use:
    from showtk.errors import Diag, ShowError
    raise ShowError(Diag("AUD-OVERRUN", "line ... ends at +189 ...", file=p, line=88, shot="co15", frame=153))
    print(Diag("TIME-READ", "...", severity="warning").format())
A ShowError reaching the CLI is printed in this format and exits 1.
"""
import sys
import textwrap

WIDTH = 110


class Diag:
    __slots__ = ("rule", "msg", "file", "line", "shot", "frame", "severity")

    def __init__(self, rule, msg, file=None, line=None, shot=None, frame=None, severity="error"):
        self.rule = rule
        self.msg = msg
        self.file = file
        self.line = line
        self.shot = shot
        self.frame = frame
        self.severity = severity

    @property
    def is_error(self):
        return self.severity == "error"

    def head(self):
        parts = []
        if self.file is not None:
            from . import paths
            loc = paths.rel(self.file)
            if self.line is not None:
                loc += ":%d" % self.line
            parts.append(loc + ":")
        if self.shot is not None:
            parts.append(str(self.shot))
        if self.frame is not None:
            parts.append("+%d" % self.frame)
        if self.rule:
            parts.append("[%s]" % self.rule)
        return " ".join(parts)

    def format(self):
        text = self.head()
        msg = ("warning: " if self.severity == "warning" else "") + str(self.msg)
        text = (text + " " + msg) if text else msg
        lines = []
        for k, para in enumerate(text.split("\n")):
            lines += textwrap.wrap(para, WIDTH, initial_indent="" if k == 0 else "  ",
                                   subsequent_indent="  ", break_long_words=False,
                                   break_on_hyphens=False) or [""]
        return "\n".join(lines)

    def __str__(self):
        return self.format()

    def __repr__(self):
        return "Diag(%r)" % self.format()


class ShowError(Exception):
    """one or more diagnostics that stop a command (exit code 1)"""

    def __init__(self, *diags):
        flat = []
        for d in diags:
            if isinstance(d, (list, tuple)):
                flat += list(d)
            elif isinstance(d, Diag):
                flat.append(d)
            else:
                flat.append(Diag("SHOW", str(d)))
        self.diags = flat
        super().__init__("\n".join(d.format() for d in flat))


def report(diags, out=None):
    """print diagnostics; return the number of errors among them"""
    out = out or sys.stderr
    n = 0
    for d in diags:
        print(d.format(), file=out)
        n += d.is_error
    return n


def fail(rule, msg, **where):
    """raise a one-diagnostic ShowError"""
    raise ShowError(Diag(rule, msg, **where))
