"""showcase: every kit instrument in turn (150 BPM, Ab major), mixed and mastered.

Each segment is a small Score rendered + mixed on its own (instrument-type
channel strip, shared plate/hall), then the segments are laid end to end (tails
overlap into the next segment) and the whole reel is mastered to -14 LUFS /
-1 dBTP. Also writes a per-preset audio tuning table (A = 440 Hz).
"""
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import numpy as np  # noqa: E402

import kit  # noqa: E402
from kit import qa, theory  # noqa: E402
from kit.core import note as N  # noqa: E402

BPM = 150
HOOK = "C5:.5 C5:.5 Eb5:.5 C5:.5 Ab5:1 Eb5:1 F5:.5 F5:.5 Eb5:.5 C5:.5 Bb4:1 C5:1"
PROG = [(0, 4, "Dbmaj7"), (4, 4, "Eb"), (8, 4, "Cm7"), (12, 4, "Fm7")]


def ph_drums(tr, kind):
    if kind == "pop":
        tr.pattern("x.....x.x.....x.", 36, bars=2, v=0.9)
        tr.pattern("....x..g....x...", 38, bars=2, v=0.88, ghost_v=0.3)
        tr.pattern("XxxxXxxxXxxxXxxo", 42, bars=1.75, v=0.5, accent_v=0.7)
        for i, (t, p) in enumerate([(7, 48), (7.25, 47), (7.5, 45), (7.75, 43)]):
            tr.add(t, 0.25, p, 0.85)
        tr.add(0, 2, 49, 0.8)
        tr.add(8, 2, 57, 0.9)
        tr.add(8, 1, 36, 0.95)
    elif kind == "rock":
        tr.pattern("x.x...x.x.x...x.", 36, bars=1, v=0.9)
        tr.pattern("....X.......X...", 38, bars=1, v=0.95)
        tr.pattern("x.x.x.x.x.x.x.x.", 51, bars=1, v=0.65)
        tr.add(4, 1, 49, 0.9)
    elif kind == "808":
        tr.pattern("x......x..x.....", 36, bars=1, v=0.95)
        tr.pattern("....x.......x...", 39, bars=1, v=0.9)
        tr.pattern("xxxxxxxxxxxxxxox", 42, bars=1, v=0.55)
        tr.pattern("..x...x...x...x.", 56, bars=1, v=0.5)
    else:
        tr.pattern("x...x...x..x.x..", 36, bars=1, v=0.9)
        tr.pattern("....x.......x...", 38, bars=1, v=0.9)
        tr.pattern("x.x.x.x.x.x.x.xo", 42, bars=1, v=0.7)


def ph_bass(tr, key):
    if key == "slap_bass":
        roots = [N("Db2"), N("Eb2"), N("C2"), N("F2")]
        for b, R in enumerate(roots[:2]):
            for (a, d, iv, art, v) in [(0, .5, 0, "slap", .95), (.5, .25, 12, "pop", .85),
                                       (.75, .25, 0, "ghost", .5), (1, .5, 0, "slap", .85),
                                       (1.5, .5, 12, "pop", .8), (2, .5, 0, "slap", .9),
                                       (2.5, .25, 12, "pop", .8), (2.75, .25, 7, "slap", .7),
                                       (3, .5, 0, "slap", .85), (3.5, .5, 12, "pop", .8)]:
                tr.add(b * 4 + a, d * 0.9, R + iv, v, art)
    elif key in ("bass808", "reese", "sub_bass"):
        tr.add(0, 1.5, N("Ab1"), 0.9)
        tr.add(1.5, 1.0, N("Ab1"), 0.85)
        tr.add(2.5, 1.0, N("Eb2"), 0.85, glide=0.25)
        tr.add(3.5, 0.5, N("F2"), 0.85, glide=0.12)
    else:
        line = [("Db2", .5), ("Db2", .5), ("Db3", .25), ("Db2", .25), ("Db2", .5), ("Ab1", .5),
                ("Db2", .5), ("Eb2", .5), ("F2", .5), ("Eb2", .5), ("Eb2", .5), ("Eb3", .25),
                ("Eb2", .25), ("Eb2", .5), ("Bb1", .5), ("C2", .5), ("Db2", .5)]
        t = 0
        for nm, d in line:
            art = "pop" if nm.endswith("3") and key == "pick_bass" else ""
            tr.add(t, d * 0.9, N(nm), 0.85, art)
            t += d


def ph_keys(tr, key):
    for (t, d, c) in PROG[:2]:
        v = theory.voice(c, center=64, n=4)
        for a in (0, 1.5, 2.5, 3.5) if key != "organ" else (0,):
            tr.chord(t + a, 0.45 if key != "organ" else d * 0.95, v, 0.75, strum=0.01)
        tr.add(t, d * 0.9, theory.bass_note(c, 3), 0.7)
    tr.seq("Ab5:.5 Bb5:.5 C6:1", start=6, v=0.8)


def ph_mallet(tr, key):
    tr.seq("Eb5:.5 Ab5:.5 C6:.5 Eb6:.5 Db6:.5 C6:.5 Bb5:.5 Ab5:.5", v=0.8,
           transpose=12 if key in ("glock_real",) else 0)


def ph_lead(tr, key):
    if key == "saw_lead":
        tr.add(0, 1, N("Eb5"), 0.85)
        tr.add(1, 1, N("Ab5"), 0.85, glide=0.12)
        tr.add(2, 0.5, N("G5"), 0.8, "legato")
        tr.add(2.5, 0.5, N("F5"), 0.8, "legato", glide=0.06)
        tr.add(3, 1, N("Eb5"), 0.85, bend=[(0, -1.0), (0.15, 0.0)])
        tr.seq("C5:.5 Eb5:.5 F5:.5 Ab5:.5 Bb5:2", start=4, v=0.85)
    elif key == "pluck":
        for (t, d, c) in PROG[:2]:
            v = theory.voice(c, center=72, n=4)
            for k in range(16):
                tr.add(t + k * .25, .22, [v[0], v[2], v[1], v[3]][k % 4], .75 if k % 2 else .85)
    else:
        tr.seq(HOOK, v=0.85, gate=0.92)


def ph_chip(tr, key):
    if key == "chip_tri":
        tr.seq("Ab2:.5 Ab3:.5 Eb2:.5 Eb3:.5 F2:.5 F3:.5 Db2:.5 Eb2:.5", v=0.9)
    elif key == "chip_noise":
        for k in range(16):
            tr.add(k * .25, .2, 70 if k % 4 else 60, .9 if k % 4 == 0 else .6)
    elif key == "chip_arp":
        for (t, d, c) in PROG[:1]:
            tr.add(t, 2, N("Db5"), 0.85, arp=[0, 4, 7, 11])
            tr.add(t + 2, 2, N("Eb5"), 0.85, arp=[0, 4, 7, 12])
    else:
        tr.seq("Ab5:.25 C6:.25 Eb6:.25 r:.25 Ab6:.5 r:.5 " + " ".join(HOOK.split()[:6]), v=0.85,
               transpose=0 if key != "chip_p50" else -12)


def ph_pad(tr, key):
    for (t, d, c) in PROG[:2]:
        center = {"warm_pad": 62, "string_pad": 66, "strings": 70, "violas": 60, "celli": 52,
                  "supersaw_chords": 72, "brass": 68}.get(key, 64)
        if key == "brass":
            for a in (0, 1.5, 3):
                tr.chord(t + a, 0.45, theory.voice(c, center=center, n=4), 0.85)
        elif key == "pizz":
            v = theory.voice(c, center=64, n=4)
            for k in range(8):
                tr.add(t + k * .5, .3, v[k % 4], .8)
        else:
            tr.chord(t, d * 0.97, theory.voice(c, center=center, n=4), 0.75)


def ph_orch(tr, key):
    lines = {"horn": "Eb4:1 Ab4:1 C5:2", "trumpet": "Eb5:.5 F5:.5 Ab5:1 G5:2", "trombone": "Ab3:1 Eb3:1 Ab3:2",
             "flute": "C6:.5 Eb6:.5 Ab6:1 G6:.5 F6:.5 Eb6:1", "timpani": "Ab2:1 Eb2:1 Ab2:.5 Ab2:.5 Eb2:1",
             "harp": "Ab3:.25 C4:.25 Eb4:.25 Ab4:.25 C5:.25 Eb5:.25 Ab5:.25 C6:.25 Eb6:2",
             "contrabass": "Ab1:2 Eb2:2"}
    tr.seq(lines[key], v=0.8)


def ph_guitar(tr, key):
    for k in range(8):
        tr.add(k * .5, .45, N("Ab2"), .85 if k % 2 == 0 else .75, "mute")
    tr.add(4, 1.9, N("F2"), .95, "open")
    tr.add(6, .45, N("Db2"), .9, "mute")
    tr.add(6.5, .45, N("Db2"), .9, "mute")
    tr.add(7, .95, N("Eb2"), .95, "open")


SEGMENTS = [
    # (label, preset, phrase fn, arg, bars, strip)
    ("drums: pop kit (layered synth + CC0 samples)", "drums_pop", ph_drums, "pop", 2, "drums"),
    ("drums: rock kit", "drums_rock", ph_drums, "rock", 1, "drums"),
    ("drums: TR-808 kit", "drums_808", ph_drums, "808", 1, "drums"),
    ("drums: GB/NES noise kit", "drums_chip", ph_drums, "chip", 1, "drums"),
    ("bass: finger (Karplus-Strong)", "pick_bass", ph_bass, "pick_bass", 2, "bass"),
    ("bass: slap / pop / ghost", "slap_bass", ph_bass, "slap_bass", 2, "bass"),
    ("bass: analogue synth bass", "synth_bass", ph_bass, "synth_bass", 1, "bass"),
    ("bass: sub", "sub_bass", ph_bass, "sub_bass", 1, "bass"),
    ("bass: 808 with glides", "bass808", ph_bass, "bass808", 1, "bass"),
    ("bass: reese", "reese", ph_bass, "reese", 1, "bass"),
    ("keys: Salamander grand piano", "piano", ph_keys, "piano", 2, "keys"),
    ("keys: FM electric piano", "fm_epiano", ph_keys, "fm_epiano", 2, "keys"),
    ("keys: tonewheel organ + rotary", "organ", ph_keys, "organ", 2, "keys"),
    ("mallets: celesta", "celesta", ph_mallet, "celesta", 1, "bells"),
    ("mallets: glockenspiel (synth)", "glock", ph_mallet, "glock", 1, "bells"),
    ("mallets: glockenspiel (VSCO-2 CE)", "glock_real", ph_mallet, "glock_real", 1, "bells"),
    ("mallets: music box", "musicbox", ph_mallet, "musicbox", 1, "bells"),
    ("mallets: vibraphone", "vibes", ph_mallet, "vibes", 1, "bells"),
    ("mallets: marimba (VSCO-2 CE)", "marimba", ph_mallet, "marimba", 1, "bells"),
    ("mallets: FM bell", "fm_bell", ph_mallet, "fm_bell", 1, "bells"),
    ("mallets: tubular bells", "tubular", ph_mallet, "tubular", 1, "bells"),
    ("harp (VSCO-2 CE)", "harp", ph_orch, "harp", 1, "bells"),
    ("lead: supersaw (HOOK)", "supersaw_lead", ph_lead, "supersaw_lead", 2, "lead"),
    ("lead: mono saw, glides + bends", "saw_lead", ph_lead, "saw_lead", 2, "lead"),
    ("lead: square", "square_lead", ph_lead, "square_lead", 2, "lead"),
    ("pluck arps", "pluck", ph_lead, "pluck", 2, "lead"),
    ("chip: pulse 12.5% (BEEP + HOOK)", "chip_p12", ph_chip, "chip_p12", 1, "chip"),
    ("chip: pulse 25%", "chip_p25", ph_chip, "chip_p25", 1, "chip"),
    ("chip: pulse 50%", "chip_p50", ph_chip, "chip_p50", 1, "chip"),
    ("chip: 4-bit triangle bass", "chip_tri", ph_chip, "chip_tri", 1, "chip"),
    ("chip: LFSR noise", "chip_noise", ph_chip, "chip_noise", 1, "chip"),
    ("chip: frame-rate arpeggio", "chip_arp", ph_chip, "chip_arp", 1, "chip"),
    ("pad: warm analogue", "warm_pad", ph_pad, "warm_pad", 2, "pad"),
    ("pad: string machine ensemble", "string_pad", ph_pad, "string_pad", 2, "pad"),
    ("strings: VSCO-2 CE violins", "strings", ph_pad, "strings", 2, "pad"),
    ("strings: VSCO-2 CE celli", "celli", ph_pad, "celli", 2, "pad"),
    ("strings: pizzicato", "pizz", ph_pad, "pizz", 2, "pad"),
    ("supersaw chords", "supersaw_chords", ph_pad, "supersaw_chords", 2, "pad"),
    ("synth brass stabs", "brass", ph_pad, "brass", 2, "pad"),
    ("orchestral: horn", "horn", ph_orch, "horn", 1, "pad"),
    ("orchestral: trumpet", "trumpet", ph_orch, "trumpet", 1, "pad"),
    ("orchestral: trombone", "trombone", ph_orch, "trombone", 1, "pad"),
    ("orchestral: flute", "flute", ph_orch, "flute", 1, "pad"),
    ("orchestral: timpani", "timpani", ph_orch, "timpani", 1, "drums"),
    ("DOOM guitar: drop-tuned, palm mutes, double-tracked", "doom_guitar", ph_guitar, None, 2, "guitar"),
]
FX_SEGMENTS = [
    ("fx: riser -> impact", [("riser", 0, 8, dict(pitched=N("Ab3"))), ("impact", 8, 1, {})], 3),
    ("fx: downlifter", [("downlifter", 0, 4, {})], 1),
    ("fx: reverse cymbal -> crash", [("revcymbal", 0, 4, {})], 1),
    ("fx: scratch (baby)", [("scratch", 0, 4, dict(pattern="baby"))], 1),
    ("fx: scratch (chirp)", [("scratch", 0, 4, dict(pattern="chirp"))], 1),
    ("fx: scratch (transformer)", [("scratch", 0, 4, dict(pattern="transformer"))], 1),
    ("fx: scratch (flare / tear / scribble)", [("scratch", 0, 1.5, dict(pattern="flare")),
                                               ("scratch", 1.5, 1.5, dict(pattern="tear")),
                                               ("scratch", 3, 1, dict(pattern="scribble"))], 1),
    ("fx: vinyl crackle", [("vinyl", 0, 4, {})], 1),
    ("fx: gang shouts (formant synth)", [("hey", 0, .5, {}), ("hey", 1, .5, dict(seed=5)),
                                         ("ho", 2, .5, {}), ("hey", 3, .5, dict(seed=9))], 1),
]

STRIPS = {
    "drums": dict(eq=[("hp", 30, 2), ("peak", 350, -2, 1.0)],
                  comp=dict(thresh=-14, ratio=2.5, attack=10, release=100), sends={"room": -14}),
    "bass": dict(eq=[("hp", 30, 2), ("peak", 250, -2.5, 1.0), ("lp", 7000, 2)],
                 comp=dict(thresh=-18, ratio=4, attack=8, release=80), sat=2.5, mono_below=150),
    "keys": dict(eq=[("hp", 70, 2), ("peak", 300, -2, 1.0), ("highshelf", 9000, 1.0)],
                 comp=dict(thresh=-20, ratio=2.5, attack=12, release=120), sends={"plate": -14}),
    "bells": dict(eq=[("hp", 200, 2)], sends={"hall": -10, "delay": -16}),
    "lead": dict(eq=[("hp", 150, 2), ("peak", 2600, 1.5, 0.9)],
                 comp=dict(thresh=-18, ratio=3, attack=5, release=90), sends={"plate": -12, "delay": -12}),
    "chip": dict(eq=[("hp", 60, 2), ("lp", 12000, 2)], sends={"delay": -12, "room": -16}),
    "pad": dict(eq=[("hp", 90, 2), ("peak", 400, -2, 1.0), ("highshelf", 8000, 1.0)],
                sends={"hall": -9}, width=1.15),
    "guitar": dict(eq=[("hp", 80, 2), ("lp", 10000, 2)], sends={"room": -16}),
    "fx": dict(eq=[("hp", 35, 2)], sends={"plate": -16}),
}


ALL_SCORES = []


def render_segment(label, preset, fn, arg, bars, strip, tail_beats=4.0):
    s = kit.Score(bpm=BPM, key="Ab", bars=bars, tail_beats=tail_beats, title=label)
    tr = s.track(preset, inst=kit.make(preset), drum=preset.startswith("drums"),
                 role="drums" if preset.startswith("drums") else "music")
    if fn is ph_guitar:
        fn(tr, arg)
    else:
        fn(tr, arg)
    s.humanize(timing_ms=3.0, vel=0.04, seed=3)
    stem = kit.render_track(tr, s)
    ALL_SCORES.append(s)
    return s, stem, STRIPS[strip]


def render_fx_segment(label, items, bars):
    s = kit.Score(bpm=BPM, key="Ab", bars=bars, tail_beats=6, title=label)
    tr = s.track("fx", inst=kit.make("fx"), role="fx")
    for kind, t, d, kw in items:
        tr.add(t, d, N("Ab1") if kind == "impact" else 60, 0.85, fx=kind, src="ahh", **kw)
    if kind == "revcymbal":
        tr2 = s.track("crash", inst=kit.make("drums_pop"), drum=True)
        tr2.add(4, 2, 49, 0.95)
        tr2.add(4, 1, 36, 0.95)
    stems = {name: kit.render_track(t, s) for name, t in s.tracks.items()}
    return s, sum(stems.values()), STRIPS["fx"]


def mix_segment(s, stem, strip):
    mx = kit.Mixer(s)
    mx.reverb("plate", kind="plate", rt60=1.4, predelay_ms=15, hp=300, lp=9000)
    mx.reverb("hall", kind="hall", rt60=2.2, predelay_ms=25, hp=250, lp=8500)
    mx.reverb("room", kind="room", rt60=0.5, hp=200, lp=11000)
    mx.delay("delay", beats=0.75, beats_r=0.5, feedback=0.28, hp=400, lp=5000, gain_db=-3)
    st = dict(strip)
    mx.channel("x", stem, bus="main", **st)
    return mx.render()


def main():
    t0 = time.time()
    spb = 60.0 / BPM
    parts = []
    for seg in SEGMENTS:
        s, stem, strip = render_segment(*seg)
        y = mix_segment(s, stem, strip)
        # loudness-match segments so the reel is even (-20 LUFS pre-master per part)
        L = kit.lufs(y)
        if np.isfinite(L):
            y = y * kit.db2a(-20.0 - L)
        parts.append((seg[0], seg[4], y))
        print(f"  {seg[0]:<52} {L:6.1f} LUFS raw  ({time.time() - t0:5.1f}s)", flush=True)
    for label, items, bars in FX_SEGMENTS:
        s, stem, strip = render_fx_segment(label, items, bars)
        y = mix_segment(s, stem, strip)
        L = kit.lufs(y)
        y = y * kit.db2a((-30.0 if "vinyl" in label else -21.0) - L)
        parts.append((label, bars, y))
        print(f"  {label:<52} {L:6.1f} LUFS raw", flush=True)
    # lay out: each part starts at the previous part's bar end + 1 beat gap
    total_beats = sum(b * 4 + 1 for _, b, _ in parts) + 8
    n = int(total_beats * spb * kit.SR)
    reel = np.zeros((n, 2))
    cur = 0.0
    sections = []
    for label, bars, y in parts:
        a = int(cur * spb * kit.SR)
        m = min(len(y), n - a)
        reel[a:a + m] += y[:m]
        sections.append((label, cur))
        cur += bars * 4 + 1
    reel = reel[: int((cur + 6) * spb * kit.SR)]
    y, mrep = kit.master(reel, target_lufs=-14.0, tp_max=-1.0,
                         chain=dict(glue=dict(thresh=-18.0, ratio=1.6, attack=30.0, release=200.0,
                                              knee=6.0, detector="rms", sc_hp=90.0)), verbose=True)
    files = kit.export(y, os.path.join(HERE, "showcase"))
    # dummy score for spectrogram section labels
    sc = kit.Score(bpm=BPM, bars=int(cur / 4) + 1)
    for label, beat in sections[::3]:
        sc.section(label.split(":")[0][:14], beat / 4, 1)
    rep = qa.report(y, path_wav=files["wav"], png=os.path.join(HERE, "showcase_spectrogram.png"),
                    title="showcase - every kit instrument (150 BPM, Ab major)", score=sc)
    # tuning table for every pitched preset (A = 440)
    tun = {}
    for key in kit.PRESETS:
        if key.startswith(("drums", "fx", "chip_noise", "doom", "chip_arp", "timpani")):
            continue
        tun[key] = qa.tuning_preset(key)["max_abs_cents"]
    rep["tuning_audio_max_abs_cents"] = tun
    sym = [qa.tuning_symbolic(sc_) for sc_ in ALL_SCORES]
    rep["tuning_symbolic"] = dict(checked=sum(x["checked"] for x in sym), key="Ab major",
                                  out_of_scale=[o for x in sym for o in x["out_of_scale"]])
    rep.pop("clashes", None)  # solo segments: no bass/harmony overlap to check
    rep["tuning_audio_worst"] = max((v for v in tun.values() if v is not None), default=None)
    rep["master"] = mrep
    rep["files"] = files
    rep["sections"] = [dict(label=l, beat=b, time_s=b * spb) for l, b in sections]
    with open(os.path.join(HERE, "showcase_qa.json"), "w") as fh:
        json.dump(rep, fh, indent=1, default=float)
    txt = qa.format_report(rep)
    txt += "\naudio tuning (max |cents| per preset, A=440): " + ", ".join(
        f"{k} {v:.1f}" for k, v in tun.items() if v is not None)
    with open(os.path.join(HERE, "showcase_qa.txt"), "w") as fh:
        fh.write(txt + "\n\nsections:\n" + "\n".join(f"{b * spb:7.2f}s  {l}" for l, b in sections) + "\n")
    print(txt)
    print(f"done in {time.time() - t0:.0f}s")


if __name__ == "__main__":
    main()
