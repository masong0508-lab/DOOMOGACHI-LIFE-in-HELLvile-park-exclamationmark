"""DOOMOGACHI production kit: score engine, instruments, mixer, master, QA.

    import kit
    s = kit.Score(bpm=150, key="Ab", bars=16)
    ...
    stems = kit.render_score(s)
    mx = kit.Mixer(s); ...; pre = mx.render()
    y, rep = kit.master(pre)
    kit.export(y, "out/song")
    print(kit.qa.format_report(kit.qa.report(y, stems=mx.stems, score=s)))
"""
from .core import limit_threads

limit_threads(2)

from . import core, dsp, drums, fx, master as mastering, mixer, qa, reverb, samplers, score, synths, theory  # noqa: E402
from .core import SR, db2a, load, mtof, note, note_name, write_wav  # noqa: E402
from .master import export, limiter, lufs, master, true_peak  # noqa: E402
from .mixer import Mixer, compress, eq, mono_below, width  # noqa: E402
from .render import PRESETS, make, render_score, render_track  # noqa: E402
from .score import Note, Score, Track, import_midi  # noqa: E402

__all__ = ["Score", "Track", "Note", "import_midi", "make", "PRESETS", "render_score",
           "render_track", "Mixer", "master", "export", "qa", "SR", "note", "note_name", "mtof",
           "db2a", "load", "write_wav", "eq", "compress", "width", "mono_below", "limiter",
           "lufs", "true_peak", "theory", "synths", "samplers", "drums", "fx", "reverb", "dsp"]
