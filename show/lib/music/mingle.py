"""MINGLE v2: the creator's phrase (pad1.mid / pad1interrupt.mid, Oct 25), 150 bpm, C minor.

The notes and rhythm are the creator's MIDI exactly (beats: MIDI note, duration). The
music box plays them two octaves up (+24: the kit's music box has no body below C4);
the Mayor hums them at the written pitch (start = middle C, C4).
  form=full       pad1: ... C4 Bb3 Ab3 Eb3 (the whole phrase)
  form=interrupt  pad1interrupt: ... C4, Bb3 late (19.25), Ab3 later (20.75), cut off
"""
from showtk.music.scorekit import Cue, place

HEAD = [(0, 1, 60), (1, 1, 58), (2, 1, 55), (3, 1, 60), (4, 1, 58), (5, 1, 55),
        (6, 1, 58), (7, 1, 56), (8, 1, 53), (9, 1, 58), (10, 1, 56), (11, 1, 53),
        (12, 1, 56), (13, 1, 55), (14, 1, 51), (15, 3, 48)]
TAIL = {"full": [(18, 1, 60), (19, 1, 58), (20, 1, 56), (21, 2.25, 51)],
        "interrupt": [(18, 1, 60), (19.25, 1.75, 58), (20.75, 3.25, 56)]}
BOX_OCTAVES = 2


def notes(form="full"):
    return HEAD + TAIL["interrupt" if form == "interrupt" else "full"]


def mingle(form="full", reps="1", octaves=str(BOX_OCTAVES), **_):
    """reps: the phrase back to back every 24 beats (6 bars); the interrupt only ever
    plays its cut-off on the last pass (earlier passes are the full phrase)."""
    reps = int(reps or 1)
    c = Cue(f"mingle_{form}", bpm=150, key="C", mode="minor", bars=6 * reps, tail_beats=4,
            desc=f"MINGLE v2 ({form})")
    mb = c.track("mb", "musicbox")
    for r in range(reps):
        c.section(f"phrase{r + 1}", 1 + 6 * r, 6)
        last = r == reps - 1
        place(mb, notes(form if last else "full"), at=24 * r, v=0.7, transpose=12 * int(octaves))
    c.mix = {"mb": dict(gain=0, sends={"room": -11})}
    return c
