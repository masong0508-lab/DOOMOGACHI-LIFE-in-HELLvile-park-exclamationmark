"""`show music ...` (WP10): auto-loaded by the CLI (register(subparsers)).

    show music build [cue ...]          render series/music.shw cues into show_lib/music/
    show music list                     the cue library
    show music import-barmap <mid>      a master's bar map (tempo + section markers)
    show music barmap <cue>             a cue's bar table: bar, sample, frame, section
    show music spot ep101 [--mx]        ep101/cues.shw -> events, cue sheet, temp MX track
"""
from showtk.music import render as _render
from showtk.music import spot as _spot


def register(subparsers):
    p = _render.register(subparsers)
    _spot.register_into(p)
    return p
