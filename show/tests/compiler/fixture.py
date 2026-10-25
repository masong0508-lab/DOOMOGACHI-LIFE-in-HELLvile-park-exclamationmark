"""The spec 6 worked example as a project tree for the compiler tests.

at41/ holds the spec's 6.1/6.2 excerpts verbatim (series/show.shw,
sets/storybook.shw, sfx.shw, music.shw, voices.shw; ep101/strings.shw,
lines.shw, cues.shw). The rundown and the shots are the REAL files,
show/ep101/rundown.shw and show/ep101/shots/co.shw, so the test proves what
the creator's files compile to.
"""
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "show", "tools"))
SHOW = os.path.join(ROOT, "show")


def make_tree(dst, shots=("co.shw",), extra=None):
    """dst/series, dst/ep101 (fixture + real rundown and shots); extra: {relpath: text}"""
    shutil.copytree(os.path.join(HERE, "at41"), dst, dirs_exist_ok=True)
    os.makedirs(os.path.join(dst, "ep101", "shots"), exist_ok=True)
    shutil.copy(os.path.join(SHOW, "ep101", "rundown.shw"), os.path.join(dst, "ep101", "rundown.shw"))
    for s in shots:
        shutil.copy(os.path.join(SHOW, "ep101", "shots", s), os.path.join(dst, "ep101", "shots", s))
    for rel, text in (extra or {}).items():
        p = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w") as f:
            f.write(text)
    return dst


class Tree:
    def __init__(self, **kw):
        self.kw = kw

    def __enter__(self):
        self.d = tempfile.mkdtemp(prefix="showc-")
        return make_tree(self.d, **self.kw)

    def __exit__(self, *a):
        shutil.rmtree(self.d, ignore_errors=True)
