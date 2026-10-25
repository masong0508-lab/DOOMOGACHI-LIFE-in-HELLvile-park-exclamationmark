"""The pilot's score (101 "Mingle Night"; owner AUDIO, WP10, SHOW_ENGINE_SPEC.md sec.10.4).

Every cue is written in the game's own musical language: the leitmotifs come out of
core/music/common/motifs.dms (scorekit.motif -> tools/mmlc.py), keys and tempi are the
game's (HaHaland Day = the HOOK in C at 140; the RIFF in E minor at 140; the theme's 150
grid for everything that touches "Doomtastic Times"), and the instruments are the
game's palette re-voiced on the kit (music box, celesta, chip pulses, rib xylophone =
marimba, DOOM guitar). series/music.shw names each cue: `src=score:pilot.<fn>` plus
string params. Every function returns a scorekit.Cue.
"""
from showtk.music.scorekit import Cue, P, bassline, chords, drums, motif, place  # noqa: F401


def _f(x, d=0.0):
    return float(x) if x not in (None, "") else d


def _i(x, d=0):
    return int(x) if x not in (None, "") else d


# The HOOK as HaHaland Day's 8-bar tune (C major): HOOK, HOOK answer, TITLE, HOOK cadence.
HAHA_CHART = "C | Am | F | G | C | Am | Dm G | C"


def _haha_melody(tr, start_bar, c, v=0.8, octave=0, transpose=0, key="C"):
    for k, name in enumerate(("HOOK", "TITLE", "HOOK", "TITLE")):
        place(tr, motif(name, key=key, octave=octave), at=c.b(start_bar + 2 * k), v=v,
              transpose=transpose)


# ===================================================================== HaHaland Day
def hahaland_day(form="band", reps="1", **_):
    """The town song: HOOK in C, 140 bpm. form: dawn | radio | fair | band | tent | ribs | distant."""
    reps = _i(reps, 1)
    bars = 8 * reps
    c = Cue("hahaland_day", bpm=140, key="C", bars=bars, tail_beats=4,
            desc=f"HaHaland Day ({form})")
    c.section("A", 1, bars)
    if form == "dawn":
        mb = c.track("mb", "musicbox")
        ce = c.track("ce", "celesta")
        pad = c.track("pad", "warm_pad")
        for r in range(reps):
            _haha_melody(mb, 1 + 8 * r, c, v=0.55)
            chords(ce, HAHA_CHART, start_bar=8 * r, center=72, n=3, v=0.3,
                   rhythm=[(0, 0.5), (1.5, 0.5), (2.5, 0.5)])
            chords(pad, HAHA_CHART, start_bar=8 * r, center=60, n=3, v=0.35)
        c.mix = {"mb": dict(gain=-2, sends={"room": -10}), "ce": dict(gain=-9, sends={"room": -10}),
                 "pad": dict(gain=-14)}
        return c
    if form == "ribs":
        mar = c.track("mar", "marimba")
        for r in range(reps):
            _haha_melody(mar, 1 + 8 * r, c, v=0.8)
            bassline(mar, HAHA_CHART, start_bar=8 * r, octave=3, pattern=((0, 0.5), (2, 0.5)), v=0.5)
        c.mix = {"mar": dict(gain=0, sends={"room": -12})}
        return c
    lead = c.track("lead", "trumpet" if form == "band" else "square_lead")
    gl = c.track("gl", "glock")
    pno = c.track("pno", "piano")
    bass = c.track("bass", "pick_bass")
    dr = c.track("dr", "drums_pop", drum=True)
    for r in range(reps):
        s = 8 * r
        _haha_melody(lead, 1 + s, c, v=0.75)
        _haha_melody(gl, 1 + s, c, v=0.45, octave=1)
        chords(pno, HAHA_CHART, start_bar=s, center=64, n=3, v=0.45,
               rhythm=[(0.5, 0.4), (1.5, 0.4), (2.5, 0.4), (3.5, 0.4)])
        bassline(bass, HAHA_CHART, start_bar=s, octave=2, pattern=((0, 1), (1, 1), (2, 1), (3, 1)),
                 fifth=True, v=0.75)
        drums(dr, "x...x...x...x...", 36, 8, start_bar=s, v=0.8)
        drums(dr, "....x.......x...", 38, 8, start_bar=s, v=0.7)
        drums(dr, "x.x.x.x.x.x.x.x.", 42, 8, start_bar=s, v=0.4)
    c.mix = {"lead": dict(gain=-4), "gl": dict(gain=-12), "pno": dict(gain=-9),
             "bass": dict(gain=-6, sends={"room": -30}), "dr": dict(gain=-8, sends={"room": -24})}
    if form in ("radio", "fair", "tent", "distant"):
        c.post = [{"fair": "speaker"}.get(form, form)]
    return c


# ===================================================================== motif stings
_MOTIF_FORMS = {
    # id: (motif, key, mode, bpm, lead inst, pad chord chart, bars, octave)
    "home_2bar": ("HOME", "C", "major", 84, "musicbox", "C | C", 2, 0, 3),
    "home_cottage": ("HOME", "C", "major", 84, "musicbox", "C | Am | F | C", 4, 0, 3),
    "nana": ("NANA", "F", "major", 100, "celesta", "F | Bb C", 2, 0, 4),
    "nana_fragment": ("NANA", "F", "major", 100, "celesta", "F | -", 2, 0, 4),
    "pip": ("PIP", "G", "major", 120, "pizz", "G | D", 2, 0, 4),
    "pip_slow": ("PIP", "G", "major", 90, "pizz", "G | Em", 2, 0, 4),
    "mayor_bright": ("MAYOR", "C", "major", 132, "brass", "C | G C", 2, 0, 4),
    "mayor_slow": ("MAYOR", "C", "major", 72, "horn", "Am | F", 2, -1, 4),
    "fuzz_small": ("FUZZ", "F", "major", 80, "organ", "F | Bb F", 2, 0, 4),
    "fuzz_amen": ("FUZZ", "F", "major", 66, "organ", "Bb | F", 2, 0, 4),
    "goodbye": ("GOODBYE", "E", "minor", 72, "celesta", "Em | Am B", 2, 0, 4),
    "wall_flame": ("WALL", "E", "minor", 70, "string_pad", "Em | Em", 2, 0, 4),
    "rivet": ("RIVET", "G", "mixolydian", 140, "doom_guitar", "G | F G", 2, 0, 4),
}


def sting(form="nana", **_):
    """A leitmotif quote, 2-4 bars: the motif on its instrument over a soft pad."""
    m, key, mode, bpm, inst, chart, bars, octv, meter = _MOTIF_FORMS[form]
    c = Cue(form, bpm=bpm, key=key, mode=mode, bars=bars, meter=meter, tail_beats=6,
            desc=f"{m} motif ({form})")
    c.section(m, 1, bars)
    lead = c.track("lead", inst)
    notes = motif(m, key=key, mode=mode, octave=octv)
    place(lead, notes, at=0.0, v=0.7)
    if form == "home_cottage":
        place(lead, motif("HOME_ANSWER", key=key), at=c.b(3), v=0.6)
    pad = c.track("pad", "warm_pad" if inst != "string_pad" else "warm_pad")
    chords(pad, chart, beats_per_bar=meter, center=58, n=3, v=0.35)
    c.mix = {"lead": dict(gain=-3, sends={"room": -10}), "pad": dict(gain=-14)}
    if form == "rivet":
        a = c.track("anvil", "drums_acoustic", drum=True)
        drums(a, "X...x...X...x...", 56, bars, v=0.9)
        b = c.track("bass", "synth_bass")
        place(b, [(t, d, p - 12) for t, d, p in notes], v=0.8)
        c.mix.update(anvil=dict(gain=-6), bass=dict(gain=-8))
    if form == "wall_flame":
        c.verbs = {"room": dict(kind="hall", rt60=2.6, hp=200, lp=8000)}
    return c


def musicbox_note(pitch="C6", **_):
    """One music-box note on the wave (1-11)."""
    c = Cue("musicbox_note", bpm=96, key="F", bars=1, tail_beats=4, desc="one music-box note")
    place(c.track("mb", "musicbox"), [(0, 1.5, P(pitch))], v=0.7)
    c.mix = {"mb": dict(gain=0, sends={"room": -9})}
    return c


# ===================================================================== the RIFF (Hell)
def _riff_band(c, start_bar, bars, gtr, bass, dr, swing=False, v=0.9, motifname="RIFF"):
    r = motif(motifname)
    for b in range(0, bars, 2):
        place(gtr, r, at=c.b(start_bar + b), v=v)
        place(bass, [(t, d, p) for t, d, p in r], at=c.b(start_bar + b), v=v * 0.8, transpose=-12)
    pat_k = "x..x..x.x.x..x.." if not swing else "x....x..x....x.."
    drums(dr, pat_k, 36, bars, start_bar=start_bar - 1, v=0.85)
    drums(dr, "....X.......X...", 38, bars, start_bar=start_bar - 1, v=0.85)
    drums(dr, "x.x.x.x.x.x.x.x." if not swing else "x..x.xx..x.xx..x", 42, bars,
          start_bar=start_bar - 1, v=0.4)


def riff(form="possessed", bars="8", **_):
    """The RIFF, E minor, 140. form: pivot | possessed | swing | low | duel | redeem."""
    bars = _i(bars, 8)
    bpm = 112 if form == "swing" else 140
    c = Cue(f"riff_{form}", bpm=bpm, key="E", mode="minor", bars=bars, tail_beats=4,
            desc=f"the RIFF ({form})")
    gtr = c.track("gtr", "doom_guitar")
    bass = c.track("bass", "reese")
    dr = c.track("dr", "drums_rock", drum=True)
    c.mix = {"gtr": dict(gain=-4, sends={"room": -22}), "bass": dict(gain=-10, sends={"room": -40}),
             "dr": dict(gain=-6, sends={"room": -24})}
    if form == "pivot":
        # HaHaland Day's HOOK on the ribs, then the shared E-E-G cell turns it into the RIFF
        mar = c.track("mar", "marimba")
        place(mar, motif("HOOK", key="C"), at=0, v=0.8)
        place(mar, [(0, 0.25, P("E5")), (0.25, 0.25, P("E5")), (0.5, 0.5, P("G5"))], at=c.b(3), v=0.85)
        c.section("hook", 1, 2)
        c.section("riff", 3, bars - 2)
        _riff_band(c, 3, bars - 2, gtr, bass, dr)
        pad = c.track("pad", "string_pad")
        chords(pad, " | ".join(["Em"] * bars), center=52, n=3, v=0.4)
        c.mix.update(mar=dict(gain=-2, sends={"room": -12}), pad=dict(gain=-14))
        c.markers["pivot"] = "3.1"
        return c
    c.section("riff", 1, bars)
    if form == "low":
        place(bass, sum([[(t + 8 * k, d, p) for t, d, p in motif("RIFF")] for k in range(bars // 2)], []),
              v=0.6, transpose=-12)
        pad = c.track("pad", "string_pad")
        chords(pad, " | ".join(["Em"] * bars), center=48, n=3, v=0.35)
        c.mix = {"bass": dict(gain=-8), "pad": dict(gain=-12)}
        return c
    if form == "redeem":
        mar = c.track("mar", "marimba")
        for b in range(0, bars, 2):
            place(mar, motif("RIFF_REDEEM", octave=2), at=c.b(1 + b), v=0.75)
        c.mix = {"mar": dict(gain=0, sends={"room": -12})}
        return c
    _riff_band(c, 1, bars, gtr, bass, dr, swing=(form == "swing"))
    if form in ("possessed", "duel"):
        pad = c.track("pad", "string_pad")
        chords(pad, " | ".join(["Em"] * bars), center=46, n=3, v=0.45)
        c.mix["pad"] = dict(gain=-12, sends={"room": -8})
    if form == "duel":
        org = c.track("org", "organ")
        for b in range(0, bars, 2):
            place(org, motif("FUZZ", key="E", mode="minor"), at=c.b(1 + b), v=0.7)
        c.mix["org"] = dict(gain=-6)
    return c


# ===================================================================== eyecatches + stings
def eyecatch_in(**_):
    """IN eyecatch: the hook turned minor (the RIFF) resolving on ITS_FINE in E (150)."""
    c = Cue("eyecatch_in", bpm=150, key="E", mode="minor", bars=3, tail_beats=4,
            desc="RIFF -> ITS_FINE in E (eyecatch IN)")
    gtr, bass, dr = c.track("gtr", "doom_guitar"), c.track("bass", "reese"), c.track("dr", "drums_rock", drum=True)
    _riff_band(c, 1, 2, gtr, bass, dr)
    _its_fine(c, 3, "E")
    c.section("riff", 1, 2)
    c.section("its_fine", 3, 1)
    c.mix.update(gtr=dict(gain=-4), bass=dict(gain=-10), dr=dict(gain=-6))
    return c


def _its_fine(c, bar, key):
    """ITS_FINE: a bright major button whose bass sits a tritone off (it is not fine)."""
    bell = c.track("fine", "glock")
    tonic = P(key + "5")
    place(bell, [(0, 0.5, tonic), (0.5, 0.5, tonic + 4), (1, 2, tonic + 7)], at=c.b(bar), v=0.8)
    lo = c.track("finebass", "pick_bass")
    place(lo, [(1, 2, tonic - 24 + 6)], at=c.b(bar), v=0.7)          # the wrong bass
    c.mix.update(fine=dict(gain=-2, sends={"room": -12}), finebass=dict(gain=-8))


def its_fine(key="C", **_):
    c = Cue("its_fine", bpm=140, key=key, bars=1, tail_beats=4, desc="ITS_FINE sting")
    _its_fine(c, 1, key)
    return c


def fail(times="1", **_):
    """The FAIL sting: a kazoo-ish brass wah-wah down (x1, or x2 a step lower)."""
    n = _i(times, 1)
    c = Cue(f"fail_x{n}", bpm=120, key="C", bars=n, tail_beats=3, desc=f"FAIL sting x{n}")
    br = c.track("br", "square_lead")
    for k in range(n):
        base = P("G4") - 2 * k
        place(br, [(0, 0.5, base), (0.5, 0.5, base - 1), (1, 0.5, base - 2), (1.5, 2, base - 3)],
              at=c.b(1 + k), v=0.8)
    c.mix = {"br": dict(gain=-2, sends={"room": -16})}
    return c


def ribs_fanfare(**_):
    """Bonejangles' rib-xylophone FANFARE (1-10): a C arpeggio up and a trill."""
    c = Cue("ribs_fanfare", bpm=140, key="C", bars=2, tail_beats=4, desc="rib-xylophone fanfare")
    m = c.track("mar", "marimba")
    arp = [(i * 0.25, 0.25, P(n)) for i, n in enumerate(["C5", "E5", "G5", "C6", "E6", "G6"])]
    place(m, arp, v=0.85)
    place(m, [(1.5 + i * 0.125, 0.125, P("C7") if i % 2 == 0 else P("D7")) for i in range(12)], v=0.6)
    place(m, [(4, 2, P("C6")), (4, 2, P("E6")), (4, 2, P("G6"))], v=0.9)
    c.mix = {"mar": dict(gain=0, sends={"room": -12})}
    return c


def ribs_minor_chord(**_):
    """The minor chord on the detuned rib xylophone (3-04)."""
    c = Cue("ribs_minor_chord", bpm=60, key="C", mode="minor", bars=1, tail_beats=4, desc="detuned minor chord")
    m = c.track("mar", "marimba")
    place(m, [(0, 3, P("C5")), (0.02, 3, P("Eb5")), (0.04, 3, P("G5"))], v=0.8)
    c.mix = {"mar": dict(gain=0, sends={"room": -10})}
    return c


# ===================================================================== Act Three/Four cute + DOOM
CUTE_CHART = "E | C#m | A | B"


def hook_cute(bars="8", **_):
    """The cute arrangement of the HOOK (dial S1), in the theme's E major at 150."""
    bars = _i(bars, 8)
    c = Cue("hook_cute", bpm=150, key="E", bars=bars, tail_beats=4, desc="cute HOOK (S1)")
    gl, pz = c.track("gl", "glock"), c.track("pz", "pizz")
    dr = c.track("dr", "drums_chip", drum=True)
    for b in range(0, bars, 2):
        place(gl, motif("HOOK", key="E"), at=c.b(1 + b), v=0.7)
    chords(pz, " | ".join([CUTE_CHART] * (bars // 4)), center=64, n=3, v=0.45,
           rhythm=[(0, 0.5), (1, 0.5), (2, 0.5), (3, 0.5)])
    drums(dr, "x.......x.......", 36, bars, v=0.5)
    drums(dr, "....x.......x...", 39, bars, v=0.45)
    c.section("cute", 1, bars)
    c.mix = {"gl": dict(gain=-3), "pz": dict(gain=-8), "dr": dict(gain=-12)}
    return c


DOOM_CHART = "E5 | C5 | A5 | B5"


def insert_doom(**_):
    """The insert song's DOOM-side arrangement at 150 (3-03..3-05): pickup bar 1, the
    shoot-out 2-25 (HUD on bar 2), the held mute 26-50 is the spotting's, re-entry 51-52,
    the final chorus 53-60, the last hit on bar 61. The theme's HOOK is quoted on top over
    the final chorus (the fallback: no DOOM-side vocal stem is published; DEFERRED)."""
    bars = 61
    c = Cue("insert_doom", bpm=150, key="E", mode="minor", bars=bars, tail_beats=6,
            desc="Doomtastic Times, DOOM side (insert song)")
    gtr, bass, dr = c.track("gtr", "doom_guitar"), c.track("bass", "reese"), c.track("dr", "drums_rock", drum=True)
    lead = c.track("lead", "supersaw_lead")
    c.section("pickup", 1, 1)
    c.section("shootout", 2, 24)
    c.section("held", 26, 25)
    c.section("reentry", 51, 2)
    c.section("final_chorus", 53, 8)
    c.section("last_hit", 61, 1)
    drums(dr, "........X.X.XXXX", 38, 1, v=0.9)                       # pickup fill
    _riff_band(c, 2, 24, gtr, bass, dr)
    _riff_band(c, 26, 25, gtr, bass, dr, v=0.7)                        # runs under the mute
    _riff_band(c, 51, 2, gtr, bass, dr)
    for b in range(53, 61, 2):
        place(lead, motif("HOOK", key="E"), at=c.b(b), v=0.85)
    pw = c.track("pw", "supersaw_chords")
    chords(pw, " | ".join([DOOM_CHART] * 2), start_bar=52, center=60, n=3, v=0.6,
           rhythm=[(0, 0.75), (1, 0.75), (2, 0.75), (3, 0.75)])
    drums(dr, "x.x.x.x.x.x.x.x.", 36, 8, start_bar=52, v=0.9)
    drums(dr, "....X.......X...", 38, 8, start_bar=52, v=0.9)
    hit = c.track("hit", "brass")
    place(hit, [(0, 2, P("E3")), (0, 2, P("B3")), (0, 2, P("E4"))], at=c.b(61), v=1.0)
    drums(dr, "X...............", 49, 1, start_bar=60, v=1.0)
    drums(dr, "X...............", 36, 1, start_bar=60, v=1.0)
    c.markers.update(hud="2.1", mute="26.1", reentry="51.1", last_hit="61.1")
    c.mix = {"gtr": dict(gain=-4, sends={"room": -24}), "bass": dict(gain=-10, sends={"room": -40}),
             "dr": dict(gain=-5, sends={"room": -26}), "lead": dict(gain=-7), "pw": dict(gain=-12),
             "hit": dict(gain=-4)}
    return c


def cute_bed(bars="4", **_):
    """A cute bed (hearts after the REWIND, 3-05): celesta and pad, E major."""
    bars = _i(bars, 4)
    c = Cue("cute_bed", bpm=150, key="E", bars=bars, tail_beats=6, desc="cute bed")
    ce, pad = c.track("ce", "celesta"), c.track("pad", "warm_pad")
    chords(ce, " | ".join([CUTE_CHART] * (bars // 4 or 1)), center=76, n=3, v=0.35,
           rhythm=[(0, 0.5), (1, 0.5), (2, 0.5), (3, 0.5)], strum=0.06)
    chords(pad, " | ".join([CUTE_CHART] * (bars // 4 or 1)), center=60, n=3, v=0.35)
    c.mix = {"ce": dict(gain=-6, sends={"room": -10}), "pad": dict(gain=-12)}
    return c


def hymn_try3(**_):
    """FUZZ hymn in major + BEEP metronome + claps (3-07), cut by the spotting's mute on "Cl-"."""
    bars = 4
    c = Cue("hymn_try3", bpm=96, key="F", bars=bars, tail_beats=4, desc="FUZZ hymn, major, claps")
    org = c.track("org", "organ")
    for b in (1, 3):
        place(org, motif("FUZZ", key="F"), at=c.b(b), v=0.7)
    chords(c.track("pad", "warm_pad"), "F | Bb F | F | C F", center=58, n=3, v=0.35)
    beep = c.track("beep", "chip_p50")
    place(beep, [(i, 0.1, P("C7")) for i in range(bars * 4)], v=0.4)
    dr = c.track("dr", "drums_pop", drum=True)
    drums(dr, "....x.......x...", 39, bars, v=0.7)
    c.mix = {"org": dict(gain=-4), "pad": dict(gain=-14), "beep": dict(gain=-20), "dr": dict(gain=-10)}
    return c


def true_name(**_):
    """TRUE NAME (4-01): the HOOK_LOVE head, slow, then a soft choir (not triumphant)."""
    c = Cue("true_name", bpm=72, key="F", bars=4, tail_beats=8, desc="true name + soft choir")
    ce = c.track("ce", "celesta")
    place(ce, motif("HOOK_LOVE", key="F"), v=0.6)
    ch = c.track("ch", "string_pad")
    chords(ch, "- | F | Dm | Bb C", center=62, n=4, v=0.4)
    c.verbs = {"room": dict(kind="hall", rt60=3.0, hp=200, lp=8000)}
    c.mix = {"ce": dict(gain=-3, sends={"room": -8}), "ch": dict(gain=-8, sends={"room": -6})}
    return c


def credits_musicbox(**_):
    """End credits: the theme's chorus hook on the music box, 120 bpm, 15 bars, left
    unresolved (the Mingle stays unfinished for the finale)."""
    bars = 15
    c = Cue("credits_musicbox", bpm=120, key="E", bars=bars, tail_beats=6, desc="credits music box")
    mb, pad = c.track("mb", "musicbox"), c.track("pad", "warm_pad")
    for b in range(1, 14, 2):
        place(mb, motif("TITLE" if (b // 2) % 2 else "HOOK", key="E"), at=c.b(b), v=0.6)
    place(mb, [(0, 4, P("F#5"))], at=c.b(15), v=0.5)                   # ends on the 2nd: open
    chords(pad, " | ".join([CUTE_CHART] * 3 + ["E | C#m | A"]) + " | B", center=60, n=3, v=0.3)
    c.section("credits", 1, bars)
    c.mix = {"mb": dict(gain=-2, sends={"room": -9}), "pad": dict(gain=-15)}
    return c
