"""Scales, chord symbols and voicings (12-TET, A4 = 440)."""
from __future__ import annotations

import re

from .core import note as note_num

PC = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6,
      "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11, "Cb": 11,
      "Fb": 4, "E#": 5, "B#": 0}

SCALES = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "harmonic_minor": [0, 2, 3, 5, 7, 8, 11],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "major_pent": [0, 2, 4, 7, 9],
    "minor_pent": [0, 3, 5, 7, 10],
}

QUALITY = {
    "": [0, 4, 7], "maj": [0, 4, 7], "m": [0, 3, 7], "min": [0, 3, 7],
    "dim": [0, 3, 6], "aug": [0, 4, 8], "sus2": [0, 2, 7], "sus4": [0, 5, 7], "sus": [0, 5, 7],
    "5": [0, 7], "6": [0, 4, 7, 9], "m6": [0, 3, 7, 9],
    "7": [0, 4, 7, 10], "maj7": [0, 4, 7, 11], "M7": [0, 4, 7, 11], "m7": [0, 3, 7, 10],
    "m7b5": [0, 3, 6, 10], "dim7": [0, 3, 6, 9], "7sus4": [0, 5, 7, 10],
    "add9": [0, 4, 7, 14], "madd9": [0, 3, 7, 14], "9": [0, 4, 7, 10, 14],
    "maj9": [0, 4, 7, 11, 14], "m9": [0, 3, 7, 10, 14], "m11": [0, 3, 7, 10, 14, 17],
    "6/9": [0, 4, 7, 9, 14],
}


def scale_pcs(key="Ab", mode="major"):
    r = PC[key]
    return sorted({(r + i) % 12 for i in SCALES[mode]})


def parse_chord(sym: str):
    """'Dbmaj7' / 'Fm7' / 'Eb/G' / 'Ab' -> (root_pc, [chord pcs], bass_pc, quality)."""
    m = re.match(r"^([A-G][#b]?)([^/]*)(?:/([A-G][#b]?))?$", sym.strip())
    if not m:
        raise ValueError(f"bad chord symbol {sym!r}")
    root, q, bass = m.group(1), m.group(2), m.group(3)
    iv = QUALITY.get(q)
    if iv is None:
        raise ValueError(f"unknown chord quality {q!r} in {sym!r}")
    r = PC[root]
    pcs = [(r + i) % 12 for i in iv]
    return r, pcs, PC[bass] if bass else r, q


def chord_tones(sym: str, octave_root: int = 4):
    r, pcs, b, q = parse_chord(sym)
    base = 12 * (octave_root + 1) + r
    return [base + i for i in QUALITY[q]]


def voice(sym: str, center=64, n=4, spread=False):
    """Close voicing of a chord near `center` (MIDI), n voices (no doubled bass)."""
    r, pcs, b, q = parse_chord(sym)
    tones = list(dict.fromkeys(pcs))
    best, best_cost = None, 1e9
    for start in range(center - 14, center + 8):
        if start % 12 not in tones:
            continue
        v, p = [start], start
        while len(v) < n:
            p += 1
            used = {x % 12 for x in v}
            if p % 12 in tones and (p % 12 not in used or len(used) == len(tones)):
                v.append(p)
        cost = abs(sum(v) / len(v) - center)
        if cost < best_cost:
            best, best_cost = v, cost
    out = best
    if spread and len(out) >= 3:
        out = [out[0] - 12] + out[1:]
    return out


def bass_note(sym: str, octave=2):
    r, pcs, b, q = parse_chord(sym)
    return 12 * (octave + 1) + b


def in_scale(p: int, key="Ab", mode="major") -> bool:
    return p % 12 in scale_pcs(key, mode)


def degree(key: str, deg: int, octave: int = 4, mode="major") -> int:
    """Scale degree (1-based, may exceed 7) -> MIDI note."""
    sc = SCALES[mode]
    d = deg - 1
    return 12 * (octave + 1) + PC[key] + sc[d % 7] + 12 * (d // 7)


def transpose_text(text: str, semis: int) -> str:
    """Transpose a seq() string ('E5:2 G5:1') by semitones (flats spelling)."""
    from .core import note_name
    out = []
    for tok in text.split():
        name, sep, rest = tok.partition(":")
        if name.lower() == "r":
            out.append(tok)
            continue
        if name.startswith("["):
            ns = [note_name(note_num(x) + semis) for x in name.strip("[]").split(",")]
            out.append("[" + ",".join(ns) + "]" + sep + rest)
        else:
            out.append(note_name(note_num(name) + semis) + sep + rest)
    return " ".join(out)
