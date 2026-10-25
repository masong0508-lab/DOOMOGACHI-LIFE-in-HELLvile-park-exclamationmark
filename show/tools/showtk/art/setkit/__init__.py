"""showtk.art.setkit - read the set files, bake their generators, compose previews.

This is the ART package's own tooling (sheets, PIC-PAL checks, VRAM
estimates). The compiler (showc stage resolve + bake) is the ENGINE's; it
reads the same files and calls the same generators (show/art/gen).

    python3 -m showtk.art.setkit list                 # every set and variant
    python3 -m showtk.art.setkit sheet [SET ...]      # contact sheets
    python3 -m showtk.art.setkit check                # parse + bake + PIC-PAL + budgets
    python3 -m showtk.art.setkit bake SET OUTDIR      # indexed PNGs of every plane

(run from show/tools, or through `show sets ...` once the CLI loads commands/sets.py)
"""
import importlib
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SHOW = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
GEN_DIR = os.path.join(SHOW, "art", "gen")
SETS_DIR = os.path.join(SHOW, "series", "sets")


def gen():
    """The plate generators (show/art/gen), imported under a unique module name."""
    name = "show_art_gen"
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(name, os.path.join(GEN_DIR, "__init__.py"),
                                                  submodule_search_locations=[GEN_DIR])
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    for sub in ("pal", "canvas", "font", "light"):
        importlib.import_module(name + "." + sub)
    mod._load()
    return mod
