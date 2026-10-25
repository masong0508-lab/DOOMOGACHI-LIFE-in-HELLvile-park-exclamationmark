"""The game's own cues, voiced by the kit at 48 kHz (SHOW_ENGINE_SPEC.md sec.4.2,
10.4; WP10).

The legacy MML (core/src/songs.c rows and the core/src/mml_<name>.c story
songs) is read by IMPORTING tools/mmlc.py read-only: `read_songs()` finds
every song's four channel strings and `simulate()` mirrors the game's 30 Hz
interpreter (audio_seq.c) item for item. Nothing here parses MML itself.

The result becomes a kit Score: channel 0..3 -> one track each, the kit
preset per track from series/music.shw (`track N inst=musicbox gain=-9dB`).

Timing (`timing=` on the cue):
  grid  (default) every onset on the song's own tempo grid: a quarter is
        48 units; at t96 a bar is exactly 75 frames = 120,000 samples.
  game  every onset on the 30 Hz tick the game's sequencer plays it on:
        tick = ceil(units * 256 / rate256), rate256 = (bpm*12288+900)/1800
        (audio_seq.c), i.e. what `runner --wav` (22,050 Hz) records.

Pitch: the interpreter's key is semitones above C0 (pitch = key*16 in
1/16 semitones, audio_seq.c note_on), so MIDI = key + 12 (o5 c = C5 = 72).
Chord instruments (@4 major, @5 minor ... AudioInst.arp) are voiced as
chords (`arp=chord`, the default on pads) or as the game's per-tick
arpeggio (`arp=arp`, the default on chip_* presets).
"""
from __future__ import annotations

import importlib.util
import math
from functools import lru_cache

from . import audiopaths as P

# AudioInst.arp -> chord intervals (audio_seq.c arp_tab, as sets)
ARP_CHORDS = {0: [0], 1: [0, 4, 7], 2: [0, 3, 7], 3: [0, 12], 4: [0, 7, 12], 5: [0, 5, 7],
              6: [0, 3, 6], 7: [0, 4, 7, 11], 8: [0, 2]}
# the interpreter's 4-step per-tick arpeggio (audio_seq.c arp_tab)
ARP_TICKS = {0: [0, 0, 0, 0], 1: [0, 4, 7, 4], 2: [0, 3, 7, 3], 3: [0, 12, 0, 12],
             4: [0, 7, 12, 7], 5: [0, 5, 7, 5], 6: [0, 3, 6, 3], 7: [0, 4, 7, 11],
             8: [0, 2, 0, 2]}
# MML drum letters (mmlc.DRUMS: K S H O C T P) -> GM keys
DRUM_GM = {1: 36, 2: 38, 3: 42, 4: 46, 5: 49, 6: 45, 7: 39}
UNITS_PER_BEAT = 48


@lru_cache(maxsize=1)
def mmlc():
    """tools/mmlc.py, imported read-only (never edited, never re-implemented)."""
    path = P.REPO / "tools" / "mmlc.py"
    spec = importlib.util.spec_from_file_location("doomo_mmlc", str(path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


@lru_cache(maxsize=1)
def songs():
    m = mmlc()
    return m.read_songs(str(P.REPO))


@lru_cache(maxsize=1)
def instruments():
    return mmlc().read_instruments(str(P.REPO))


def song(key: str) -> dict:
    """'MINGLE' / 'SONG_MINGLE' / 'mml_mingle.c' / 'core/src/mml_mingle.c' -> the song."""
    k = key.strip()
    if "/" in k or k.endswith(".c"):
        k = k.rsplit("/", 1)[-1]
        k = k[4:] if k.startswith("mml_") else k
        k = k[:-2] if k.endswith(".c") else k
    k = k.upper()
    if k.startswith("SONG_"):
        k = k[5:]
    for s in songs():
        if s and s["key"] == k:
            return s
    raise KeyError(f"no game song {key!r} (tools/mmlc.py read_songs)")


def song_bpm(s: dict) -> int:
    return mmlc().song_tempo(s["ch"][0])


def rate256(bpm: int) -> int:
    return mmlc().rate256(bpm)


def onset_beats(units: int, bpm: float, timing: str) -> float:
    """Unit position -> beats at the song's tempo (grid) or on the game's tick."""
    if timing == "grid":
        return units / UNITS_PER_BEAT
    r = rate256(int(round(bpm)))
    tick = math.ceil(units * 256 / r)
    return tick / 30.0 * bpm / 60.0


def channel_events(s: dict, ch: int, passes: int = 1, timing: str = "grid"):
    """One channel through `passes` passes -> (notes, length_beats).

    notes: dicts {t, d, key, midi, vol, inst, arp, gate, drum, units, pos}
    (t, d in beats). Rests, ties and state changes are folded in.
    """
    m = mmlc()
    text = s["ch"][ch]
    if text is None:
        return [], 0.0
    items, ends, _c = m.simulate(text, ch, passes)
    insts = instruments()
    bpm = song_bpm(s)
    vol = 12
    out = []
    total_units = ends[-1][0] if ends else 0
    for it in items:
        k = it["k"]
        if k == "V":
            vol = it["v"]
        elif k == "N":
            units = sum(it["pieces"])
            row = insts[it["inst"]] if it["inst"] < len(insts) else insts[0]
            g = it["q"] or row["gate"]
            t0 = onset_beats(it["pos"], bpm, timing)
            t1 = onset_beats(it["pos"] + units, bpm, timing)
            out.append({"t": t0, "d": max(1e-3, (t1 - t0) * g / 8.0), "span": t1 - t0,
                        "key": it["key"], "midi": it["key"] + 12, "vol": vol,
                        "inst": it["inst"], "arp": row["arp"], "gate": g, "drum": None,
                        "units": units, "pos": it["pos"]})
        elif k == "D":
            t0 = onset_beats(it["pos"], bpm, timing)
            t1 = onset_beats(it["pos"] + it["len"], bpm, timing)
            out.append({"t": t0, "d": t1 - t0, "span": t1 - t0, "key": None,
                        "midi": DRUM_GM.get(it["hit"], 38), "vol": vol, "inst": None, "arp": 0,
                        "gate": 8, "drum": it["hit"], "units": it["len"], "pos": it["pos"]})
    return out, onset_beats(total_units, bpm, timing)


def vel_from_vol(vol: int, vmax: int) -> float:
    """MML volume (0..15, linear amplitude in the game's mixer) -> kit velocity,
    keeping the channel's relative levels exactly (kit: 18 dB over 0..1)."""
    if vol <= 0:
        return 0.05
    return max(0.05, min(1.0, 1.0 + 20.0 * math.log10(vol / max(1, vmax)) / 18.0))


def to_score(s: dict, tracks: dict, passes: int = 1, timing: str = "grid", title=None,
             bars=None, tail_beats=8.0):
    """Build a kit Score from a game song.

    tracks: {ch: {"inst": preset | Instrument, "params": {...}, "arp": chord|arp|root,
                  "transpose": semitones, "drum_note": GM key, "name": track name}}
    Channels not listed are dropped (the cue's own choice, e.g. one voice).
    """
    from .kitenv import kit
    bpm = song_bpm(s)
    lengths = []
    sc = kit.Score(bpm=bpm, key="C", mode="major", bars=1,
                   title=title or s["name"], tail_beats=tail_beats)
    for ch in sorted(tracks):
        spec = tracks[ch]
        evs, L = channel_events(s, ch, passes, timing)
        lengths.append(L)
        if not evs:
            continue
        inst = spec.get("inst", "musicbox")
        is_drum = isinstance(inst, str) and inst.startswith("drums")
        name = spec.get("name") or f"ch{ch}"
        tr = sc.track(name, inst=inst, role="drums" if is_drum else "music", drum=is_drum,
                      params=dict(spec.get("params", {})))
        vmax = max(e["vol"] for e in evs)
        tp = int(spec.get("transpose", 0))
        arp_mode = spec.get("arp") or ("arp" if isinstance(inst, str) and inst.startswith("chip")
                                       else "chord")
        for e in evs:
            v = vel_from_vol(e["vol"], vmax)
            if e["drum"] is not None:
                tr.add(e["t"], e["d"], int(spec.get("drum_note", e["midi"])), v, mml_hit=e["drum"])
                continue
            midi = e["midi"] + tp
            ivs = ARP_CHORDS.get(e["arp"], [0])
            if e["arp"] and arp_mode == "chord":
                for iv in ivs:
                    tr.add(e["t"], e["d"], midi + iv, v * (1.0 if iv == 0 else 0.92), mml_pos=e["pos"])
            elif e["arp"] and arp_mode == "arp":
                tr.add(e["t"], e["d"], midi, v, arp=ARP_TICKS[e["arp"]], mml_pos=e["pos"])
            else:
                tr.add(e["t"], e["d"], midi, v, mml_pos=e["pos"])
    L = max(lengths) if lengths else 4.0
    sc.bars = int(math.ceil(L / sc.beats_per_bar - 1e-9)) if bars is None else int(bars)
    sc.section("A", 0, sc.bars)
    return sc


def melody(s: dict, ch: int = 0, passes: int = 1):
    """The channel's notes as (bar, beat, midi, beats) rows (1-based bar.beat), for
    cue sheets and named notes (e.g. the Mayor's note)."""
    evs, _ = channel_events(s, ch, passes, "grid")
    rows = []
    for e in evs:
        if e["drum"] is not None:
            continue
        bar = int(e["t"] // 4) + 1
        rows.append((bar, e["t"] - (bar - 1) * 4 + 1, e["midi"], e["span"]))
    return rows


def motif(name: str):
    """A leitmotif from core/music/common/motifs.dms -> [(t_beats, d_beats, midi)]
    (the MML through mmlc.simulate, like every other game melody)."""
    path = P.REPO / "core" / "music" / "common" / "motifs.dms"
    for raw in path.read_text(encoding="ascii").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line.startswith("motif "):
            continue
        head, _, mml = line.partition(" : ")
        parts = head.split()
        if parts[1] != name:
            continue
        items, _ends, _c = mmlc().simulate(mml.strip(), 0, 1)
        out = []
        for it in items:
            if it["k"] == "N":
                u = sum(it["pieces"])
                out.append((it["pos"] / UNITS_PER_BEAT, u / UNITS_PER_BEAT, it["key"] + 12))
        meta = dict(p.split("=", 1) for p in parts[2:] if "=" in p)
        return out, meta
    raise KeyError(f"no motif {name} in {path}")
