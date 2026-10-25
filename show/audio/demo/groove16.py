"""groove16: a 16-bar J-pop groove at 150 BPM in Ab major, built with the kit.

Bars 1-8  verse: royal-road progression (IVmaj7 V iii vi), EP comping, celesta
          topline, finger bass, the BEEP motif on a GB pulse, a vinyl-filtered intro.
Bar  8    Hellvile peeks in: power-chord stab, riser, reverse cymbal, "Hey!".
Bars 9-16 chorus: the game's HOOK (canon: trailer/music.c HOOK1/2/3/7, moved
          from C to Ab) on a supersaw lead, slap bass, four-on-the-floor,
          supersaw chords pumping from the kick, piano, real strings, glock,
          GBA arps, palm-muted DOOM guitar underneath, scratches and gang shouts.
Outputs: demo/out/groove16.{wav,flac,mp3}, stems, MIDI, QA report, spectrogram.
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

OUT = os.path.join(HERE, "out", "groove16")
os.makedirs(OUT, exist_ok=True)


def build():
    s = kit.Score(bpm=150, key="Ab", mode="major", bars=16, title="groove16", tail_beats=8)
    s.section("intro", 0, 2)
    s.section("verse", 2, 6)
    s.section("chorus", 8, 8)
    chords = [(0, 4, "Dbmaj7"), (4, 4, "Eb"), (8, 4, "Cm7"), (12, 4, "Fm7"),
              (16, 4, "Dbmaj7"), (20, 4, "Eb"), (24, 2, "Cm7"), (26, 2, "Fm7"),
              (28, 2, "Bbm7"), (30, 2, "Eb7"),
              (32, 4, "Ab"), (36, 4, "Fm7"), (40, 3, "Dbmaj7"), (43, 1, "Eb"),
              (44, 2, "Cm7"), (46, 2, "Fm7"), (48, 4, "Bbm7"), (52, 4, "Eb7"),
              (56, 2, "Dbmaj7"), (58, 2, "Eb"), (60, 4, "Ab")]
    for c in chords:
        s.chord_at(*c)

    # ---------------- drums ----------------------------------------------
    kit_pop = kit.make("drums_pop", kick_tune=51.9,  # kick sub tuned to Ab1
                       pans={42: 0.25, 44: 0.25, 46: 0.3, 41: -0.5, 43: -0.35, 45: -0.15,
                             47: 0.1, 48: 0.3, 50: 0.45, 49: -0.45, 57: 0.5, 51: 0.4, 54: 0.35,
                             70: -0.35})
    kick = s.track("kick", inst=kit_pop, role="drums", drum=True)
    snare = s.track("snare", inst=kit_pop, role="drums", drum=True)
    clap = s.track("clap", inst=kit_pop, role="drums", drum=True)
    hats = s.track("hats", inst=kit_pop, role="drums", drum=True)
    toms = s.track("toms", inst=kit_pop, role="drums", drum=True)
    cym = s.track("cymbals", inst=kit_pop, role="drums", drum=True)
    perc = s.track("perc", inst=kit_pop, role="drums", drum=True)

    # intro (bars 1-2): light
    kick.pattern("x.......x.......", 36, start=0, bars=2, v=0.8)
    snare.pattern("....x.......x...", 38, start=0, bars=2, v=0.7)
    hats.pattern("x.x.x.x.x.x.x.x.", 42, start=0, bars=2, v=0.55)
    # verse (bars 3-7)
    kick.pattern("x.....x.x.....x.", 36, start=8, bars=5, v=0.88)
    snare.pattern("....x..g....x..g", 38, start=8, bars=5, v=0.82, ghost_v=0.3)
    hats.pattern("XxxxXxxxXxxxXxxo", 42, start=8, bars=5, v=0.5, accent_v=0.7)
    perc.pattern("..x...x...x...x.", 54, start=16, bars=4, v=0.45)   # tambourine
    # bar 8: build + fill
    kick.pattern("x.......x.x.x.x.", 36, start=28, bars=1, v=0.9)
    for i in range(8):
        snare.add(30 + i * 0.25, 0.25, 38, 0.45 + i * 0.06)
    snare.add(28, 0.5, 38, 0.85)
    snare.add(29, 0.5, 38, 0.8)
    hats.pattern("x.x.x.x.", 42, start=28, bars=0.5, v=0.6)
    toms.add(31.0, 0.25, 48, 0.85); toms.add(31.25, 0.25, 47, 0.85)
    toms.add(31.5, 0.25, 45, 0.9); toms.add(31.75, 0.25, 43, 0.95)
    cym.add(0, 4, 49, 0.75)
    cym.add(8, 4, 57, 0.8)
    # chorus (bars 9-16)
    kick.pattern("x...x...x...x...", 36, start=32, bars=7, v=0.95)
    kick.pattern("x...x...x.x.x.xx", 36, start=60, bars=0.75, v=0.95)
    snare.pattern("....x.......x...", 38, start=32, bars=7, v=0.95)
    clap.pattern("....x.......x...", 39, start=32, bars=7, v=0.9)
    hats.pattern("..o...o...o...o.", 46, start=32, bars=7, v=0.62)
    hats.pattern("x.xgx.xgx.xgx.xg", 42, start=32, bars=7, v=0.42, ghost_v=0.25)
    perc.pattern("xxxxxxxxxxxxxxxx", 70, start=32, bars=7, v=0.38)  # shaker 16ths
    for b in (44.5, 47.5):
        snare.add(b, 0.25, 38, 0.6, "ghost")
    # bar 15 beat 3-4 tom fill, bar 16 build to the button
    for i, (t, p) in enumerate([(58, 50), (58.5, 48), (59, 47), (59.25, 45), (59.5, 43), (59.75, 41)]):
        toms.add(t, 0.25, p, 0.82 + 0.03 * i)
    snare.pattern("x.x.xxxx", 38, start=62, bars=0.5, v=0.7)
    cym.add(32, 4, 49, 0.95)
    cym.add(32, 4, 57, 0.7)
    cym.add(48, 4, 57, 0.85)
    cym.add(60, 4, 49, 0.85)
    cym.add(64, 4, 49, 1.0)
    cym.add(64, 4, 57, 0.85)
    kick.add(64, 1, 36, 1.0)

    # ---------------- bass ---------------------------------------------------
    roots = {"Dbmaj7": N("Db2"), "Eb": N("Eb2"), "Cm7": N("C2"), "Fm7": N("F2"), "Bbm7": N("Bb1"),
             "Eb7": N("Eb2"), "Ab": N("Ab1"), "Fm": N("F2")}
    bass = s.track("bass", inst=kit.make("pick_bass", t60=2.0, bright=0.5), role="bass", program=33)
    for (t, d, c) in chords[:10]:
        R = roots[c]
        if t < 8:  # intro: long notes
            bass.add(t, d * 0.95, R, 0.75)
            continue
        half = [(0, .5, 0, "", .85), (.5, .5, 0, "", .7), (1, .25, 12, "pop", .8),
                (1.25, .25, 0, "ghost", .5), (1.5, .5, 0, "", .78)]
        pat = half + [(2 + a, b, c_, e, v) for (a, b, c_, e, v) in
                      [(0, .5, 0, "", .85), (.5, .25, 7, "", .7), (.75, .25, 12, "pop", .75),
                       (1, .5, 0, "", .8), (1.5, .5, 10 if c in ("Cm7", "Fm7", "Bbm7") else 7, "", .7)]]
        for (a, b, iv, art, v) in pat:
            if a < d:
                bass.add(t + a, b * 0.92, R + iv, v, art)
    slap = kit.make("slap_bass", gain=0.55)
    bass2 = s.track("slap", inst=slap, role="bass", program=36)
    for (t, d, c) in chords[10:]:
        R = roots[c]
        pat = [(0, .5, 0, "slap", .95), (.5, .25, 12, "pop", .85), (.75, .25, 0, "ghost", .5),
               (1, .5, 0, "slap", .85), (1.5, .5, 12, "pop", .8),
               (2, .5, 0, "slap", .92), (2.5, .25, 12, "pop", .82), (2.75, .25, 7, "slap", .7),
               (3, .5, 0, "slap", .85), (3.5, .5, 12, "pop", .8)]
        for (a, b, iv, art, v) in pat:
            if a < d:
                bass2.add(t + a, b * 0.9, R + iv, v, art)
    bass2.add(64, 2.0, N("Ab1"), 1.0, "slap")

    # ---------------- keys / pads -------------------------------------------
    ep = s.track("epiano", inst=kit.make("fm_epiano", gain=0.3), role="harmony", program=4)
    comp_rhythm = [(0, 0.45, 0.75), (1.5, 0.4, 0.62), (2.5, 0.45, 0.7), (3.5, 0.4, 0.6)]
    for (t, d, c) in chords[:10]:
        v = theory.voice(c, center=64, n=4)
        for (a, l, vel) in comp_rhythm:
            if a < d:
                ep.chord(t + a, l, v, vel, strum=0.012)
    pad = s.track("pad", inst=kit.make("warm_pad", gain=0.12, cutoff=1300.0), role="harmony", program=89)
    for (t, d, c) in chords[:10]:
        pad.chord(t, d, theory.voice(c, center=60, n=4), 0.6)
    strings = s.track("strings", inst=kit.make("violins", gain=0.8), role="harmony", program=48)
    violas = s.track("violas", inst=kit.make("violas", gain=0.7), role="harmony", program=41)
    celli = s.track("celli", inst=kit.make("celli", gain=0.7), role="harmony", program=42)
    for (t, d, c) in chords[4:]:
        v = theory.voice(c, center=70, n=3)
        vel = 0.55 if t < 32 else 0.75
        strings.chord(t, d, [p for p in v if p >= 60], vel)
        violas.add(t, d, theory.voice(c, center=60, n=1)[0], vel)
        celli.add(t, d, roots[c] + 12, vel * 0.9)
    piano = s.track("piano", inst=kit.make("piano"), role="harmony", program=0)
    for (t, d, c) in chords[10:]:
        v = theory.voice(c, center=67, n=4)
        for k in range(int(d * 2)):
            a = k * 0.5
            vel = 0.78 if k % 2 == 0 else 0.6
            if k % 4 == 3:
                vel = 0.7
            piano.chord(t + a, 0.42, v, vel)
        piano.add(t, d * 0.9, roots[c] + 12, 0.7)
    piano.chord(64, 4, [N("Ab3"), N("Eb4"), N("Ab4"), N("C5"), N("Eb5")], 0.85)
    sawc = s.track("saw_chords", inst=kit.make("supersaw_chords", gain=0.1), role="harmony", program=90)
    for (t, d, c) in chords[10:]:
        sawc.chord(t, d * 0.98, theory.voice(c, center=72, n=4), 0.75)
    sawc.chord(64, 3, theory.voice("Ab", center=72, n=4), 0.85)
    brass = s.track("brass", inst=kit.make("brass", gain=0.13), role="harmony", program=61)
    for t, c in ((31.0, "Eb7"), (47.5, "Fm7"), (63.5, "Ab")):
        brass.chord(t, 0.4, theory.voice(c, center=70, n=3), 0.85)
    brass.chord(64, 1.5, theory.voice("Ab", center=70, n=4), 0.95)

    # ---------------- melody ---------------------------------------------------
    cel = s.track("celesta", inst=kit.make("celesta", gain=0.24), role="lead", program=8)
    cel.seq("Eb5:.5 G5:.5 Bb5:1 G5:.5 Eb5:.5 C5:1", start=8, v=0.75)        # Cm7
    cel.seq("Ab5:1.5 G5:.5 F5:1", start=12, v=0.72)                          # Fm7
    cel.seq("F5:.5 Ab5:.5 C6:1 Ab5:.5 F5:.5 Eb5:1", start=16, v=0.75)        # Dbmaj7
    cel.seq("G5:.5 Bb5:.5 Eb6:1 Db6:.5 Bb5:.5 G5:1", start=20, v=0.72)       # Eb
    cel.seq("Eb5:.5 G5:.5 Bb5:.5 G5:.5 Ab5:.5 C6:.5 Eb6:.5 C6:.5", start=24, v=0.75)  # Cm7 Fm7
    cel.seq("Db6:.5 Bb5:.5 F5:.5 Db5:.5 Eb5:.5 G5:.5 Bb5:.5 Db6:.5", start=28, v=0.78)  # Bbm7 Eb7
    beep = s.track("beep", inst=kit.make("chip_p12", gain=0.12), role="lead", program=80)
    # BEEP motif (pet, adult form: c e g r >c<), on each chord's own root
    beep.seq("Ab5:.25 C6:.25 Eb6:.25 r:.25 Ab6:.5", start=14.5, v=0.8)       # over Fm7
    beep.seq("Eb5:.25 G5:.25 Bb5:.25 r:.25 Eb6:.5", start=22.5, v=0.8)       # over Eb
    lead = s.track("lead", inst=kit.make("supersaw_lead", gain=0.16), role="lead", program=81)
    lead2 = s.track("lead_saw", inst=kit.make("saw_lead", gain=0.2, vib_depth=16.0), role="lead", program=81)
    hook = [
        (32, "C5:.5 C5:.5 Eb5:.5 C5:.5 Ab5:1 Eb5:1"),          # HOOK1 (I)
        (36, "F5:.5 F5:.5 Eb5:.5 C5:.5 Bb4:1 C5:1"),           # HOOK2 (vi)
        (40, "Db5:.5 Db5:.5 F5:.5 Db5:.5 Ab5:1 F5:.5 G5:.5"),  # HOOK3 (IV -> V)
        (44, "G5:1.5 Eb5:.5 C5:1 Eb5:.5 F5:.5"),               # answer (iii -> vi)
        (48, "Db5:.5 Db5:.5 F5:.5 Db5:.5 Bb5:1 F5:1"),         # HOOK head on ii
        (52, "C5:.5 C5:.5 Bb4:.5 G4:.5 F4:1 G4:1"),            # HOOK2 shape on V7
        (56, "Db5:.5 F5:.5 Ab5:.5 F5:.5 Eb5:.5 Db5:.5 C5:.5 Bb4:.5"),  # HOOK7 (IV -> V)
        (60, "Ab4:2 r:1"),
    ]
    for t0, txt in hook:
        lead.seq(txt, start=t0, v=0.85, gate=0.92)
        lead2.seq(txt, start=t0, v=0.8, gate=0.92, transpose=-12)
    lead.add(64, 2.5, N("Ab5"), 0.9)
    lead2.add(64, 2.5, N("Ab4"), 0.85)
    glock = s.track("glock", inst=kit.make("glock_real", gain=0.55), role="lead", program=9)
    for t0, txt in hook[4:7]:
        glock.seq(txt, start=t0, v=0.6, transpose=12)
    glock.add(64, 2, N("Ab6"), 0.7)

    # ---------------- arps --------------------------------------------------------
    pluck = s.track("pluck", inst=kit.make("pluck", gain=0.16), role="harmony", program=84)
    for (t, d, c) in chords[2:10]:
        v = theory.voice(c, center=72, n=4)
        seqn = [v[0], v[2], v[1], v[3]]
        for k in range(int(d * 4)):
            pluck.add(t + k * 0.25, 0.22, seqn[k % 4], 0.62 if k % 2 else 0.78)
    arp = s.track("chip_arp", inst=kit.make("chip_p25", gain=0.06, env=(12, -1, 4)), role="harmony", program=80)
    for (t, d, c) in chords[10:]:
        v = theory.voice(c, center=78, n=4)
        seqn = [v[0], v[1], v[2], v[3], v[2], v[1]]
        for k in range(int(d * 4)):
            arp.add(t + k * 0.25, 0.2, seqn[k % 6], 0.8 if k % 4 == 0 else 0.65)

    # ---------------- DOOM guitar (Hellvile underneath) -----------------------------
    gtr = s.track("guitar", inst=kit.make("doom_guitar", level=0.2), role="harmony", program=30)
    gr = {"Ab": N("Ab2"), "Fm7": N("F2"), "Dbmaj7": N("Db2"), "Eb": N("Eb2"), "Cm7": N("C2") + 12,
          "Bbm7": N("Bb2"), "Eb7": N("Eb2")}
    gtr.add(28, 1.9, gr["Bbm7"], 0.95, "open")
    for k in range(4):
        gtr.add(30 + k * 0.5, 0.45, gr["Eb7"], 0.8 + 0.04 * k, "mute")
    for (t, d, c) in chords[10:]:
        for k in range(int(d * 2)):
            art = "open" if k == 0 and d >= 4 else "mute"
            gtr.add(t + k * 0.5, 0.48 if art == "mute" else 0.95, gr[c], 0.85 if k % 2 == 0 else 0.72, art)
    gtr.add(64, 3.0, gr["Ab"], 1.0, "open")

    # ---------------- FX ---------------------------------------------------------------
    fx = s.track("fx", inst=kit.make("fx"), role="fx")
    fx.add(0, 8, 60, 0.9, fx="vinyl")
    fx.add(24, 8, 56, 0.55, fx="riser", pitched=N("Ab3"))
    fx.add(30, 2, 60, 0.6, fx="revcymbal")
    fx.add(32, 1, N("Ab1"), 0.75, fx="impact")
    fx.add(64, 1, N("Ab1"), 0.6, fx="impact", seed=3)
    scr = s.track("scratch", inst=kit.make("fx"), role="fx")
    scr.add(15, 1, 60, 0.7, fx="scratch", pattern="chirp", src="ahh", length=0.2)
    scr.add(47, 1, 60, 0.7, fx="scratch", pattern="transformer", src="ahh", length=0.25)
    scr.add(55.5, 0.5, 60, 0.6, fx="scratch", pattern="flare", src="ahh", length=0.2)
    hey = s.track("hey", inst=kit.make("fx"), role="fx")
    for t0 in (31.5, 39.5, 55.5, 63.5):
        hey.add(t0, 0.5, 60, 0.8, fx="hey", voices=12, seed=int(t0))
    return s


def loop_score(full):
    """Chorus-only loop: bars 9-16 played twice (render-twice-keep-second gives
    a seamless wrap with real reverb/delay tails); no intro, no ending button."""
    import copy
    s = kit.Score(bpm=full.bpm, key=full.key, mode=full.mode, bars=16, title="groove16_loop",
                  tail_beats=4)
    s.section("chorus A", 0, 8)
    s.section("chorus B (loop)", 8, 8)
    s.chords = [(t - 32 + k, d, c) for (t, d, c) in full.chords if t >= 32 for k in (0, 32)]
    for name, tr in full.tracks.items():
        if name == "fx":
            continue
        t2 = s.track(name, inst=tr.inst, role=tr.role, drum=tr.drum, program=tr.program)
        for n in tr.notes:
            if 32 <= n.t < 64:
                for k in (0, 32):
                    m = copy.deepcopy(n)
                    m.t = n.t - 32 + k
                    t2.notes.append(m)
    return s


def mix(s, stems, loop=False):
    mx = kit.Mixer(s)
    kick_beats = [n.t for n in s.tracks["kick"].notes]
    mx.triggers("kick", kick_beats)
    ch_kick = [n.t for n in s.tracks["kick"].notes if n.t >= 32]
    mx.triggers("kick_chorus", ch_kick)
    sc_soft = dict(source="kick", depth=2.5, attack=3, hold=8, release=120)
    sc_pump = dict(source="kick_chorus", depth=6.0, attack=3, hold=10, release=170)
    sc_bass = dict(source="kick", depth=5.0, attack=2, hold=15, release=90)

    mx.reverb("plate", kind="plate", rt60=1.5, predelay_ms=18, hp=320, lp=9000, gain_db=-1.0,
              duck=dict(source="kick", depth=3, release=200))
    mx.reverb("hall", kind="hall", rt60=2.3, predelay_ms=28, hp=280, lp=8500, gain_db=-2.0)
    mx.reverb("room", kind="room", rt60=0.5, predelay_ms=2, hp=200, lp=11000, gain_db=-3.0)
    mx.delay("delay", beats=0.75, beats_r=0.5, feedback=0.3, hp=450, lp=5200, gain_db=-4.0,
             duck=dict(source="kick", depth=4, release=180))

    D = "drums"
    mx.channel("kick", stems.get("kick"), bus=D, gain_db=1.5,
               eq=[("hp", 28, 2), ("peak", 56, 3.0, 1.1), ("peak", 330, -4.0, 1.2), ("peak", 3800, 2.5, 1.0)],
               comp=dict(thresh=-12, ratio=3, attack=6, release=70, detector="peak"))
    mx.channel("snare", stems.get("snare"), bus=D, gain_db=-3.0,
               eq=[("hp", 110, 2), ("peak", 220, 2.0, 1.2), ("peak", 800, -2.0, 1.0), ("peak", 5500, 2.5, 0.8),
                   ("highshelf", 11000, 1.5)],
               comp=dict(thresh=-14, ratio=3.5, attack=8, release=90, detector="peak"),
               sends={"plate": -12, "room": -10})
    mx.channel("clap", stems.get("clap"), bus=D, gain_db=-7.0, eq=[("hp", 300, 2), ("peak", 1300, 1.5, 1.0)],
               sends={"plate": -9}, width=1.2)
    mx.channel("hats", stems.get("hats"), bus=D, gain_db=-4.5,
               eq=[("hp", 450, 2), ("peak", 2500, -1.5, 1.0), ("highshelf", 10000, 1.5)],
               sends={"room": -14})
    mx.channel("toms", stems.get("toms"), bus=D, gain_db=-4.0, eq=[("hp", 60, 2), ("peak", 400, -3, 1.0)],
               sends={"room": -8, "plate": -16})
    mx.channel("cymbals", stems.get("cymbals"), bus=D, gain_db=-9.0,
               eq=[("hp", 320, 2), ("peak", 4000, -1.5, 0.8), ("lp", 16000, 2)], sends={"room": -14}, width=1.2)
    mx.channel("perc", stems.get("perc"), bus=D, gain_db=-11.0, eq=[("hp", 500, 2)], sends={"room": -10})
    mx.bus(D, comp=dict(thresh=-16, ratio=2.2, attack=12, release=110, knee=6, detector="rms", sc_hp=80),
           parallel=dict(thresh=-32, ratio=8, mix_db=-10), sat=1.5,
           automation=None if loop else {"lpf": [(0, 1400), (7.5, 2500), (8, 20000)]})

    B = "bass"
    mx.channel("bass", stems.get("bass"), bus=B, gain_db=-0.5,
               eq=[("hp", 32, 2), ("peak", 52, -2.0, 1.4), ("peak", 90, 1.5, 0.9), ("peak", 260, -3.0, 1.2),
                   ("peak", 900, 2.0, 1.0), ("lp", 6000, 2)],
               comp=dict(thresh=-18, ratio=4, attack=8, release=80), sat=3.0, sidechain=sc_bass,
               mono_below=150)
    mx.channel("slap", stems.get("slap"), bus=B, gain_db=-1.0,
               eq=[("hp", 32, 2), ("peak", 52, -2.0, 1.4), ("peak", 85, 1.5, 0.9), ("peak", 280, -3.5, 1.1),
                   ("peak", 550, -2.5, 1.0), ("peak", 2200, 2.5, 1.0), ("lp", 8000, 2)],
               comp=dict(thresh=-18, ratio=4.5, attack=6, release=70, detector="peak"), sat=3.0,
               sidechain=sc_bass, mono_below=150)
    mx.bus(B, automation=None if loop else {"lpf": [(0, 900), (7.5, 1600), (8, 20000)]},
           mono_below=120)

    M = "music"
    mx.channel("epiano", stems.get("epiano"), bus=M, gain_db=-4.0,
               eq=[("hp", 140, 2), ("peak", 350, -3, 1.0), ("peak", 2800, 1.5, 0.9)],
               comp=dict(thresh=-20, ratio=2.5, attack=10, release=120),
               autopan=dict(rate_beats=2.0, depth=0.35), sends={"plate": -14, "delay": -24},
               sidechain=sc_soft)
    mx.channel("pad", stems.get("pad"), bus=M, gain_db=-6.0, eq=[("hp", 220, 2), ("peak", 450, -2, 1.0)],
               sends={"hall": -10}, width=1.3, sidechain=sc_soft)
    for nm, g, pn in (("strings", -3.0, 0.15), ("violas", -6.0, -0.1), ("celli", -7.0, -0.25)):
        mx.channel(nm, stems.get(nm), bus=M, gain_db=g, pan=pn,
                   eq=[("hp", 160 if nm != "celli" else 90, 2), ("peak", 400, -2.5, 1.0),
                       ("highshelf", 8000, 2.0)],
                   sends={"hall": -8}, sidechain=sc_soft)
    mx.channel("piano", stems.get("piano"), bus=M, gain_db=-6.0,
               eq=[("hp", 120, 2), ("peak", 280, -3, 1.0), ("peak", 3000, 1.5, 0.8), ("highshelf", 9000, 1.5)],
               comp=dict(thresh=-20, ratio=3, attack=12, release=120), sends={"plate": -16},
               sidechain=sc_soft, width=1.15)
    mx.channel("saw_chords", stems.get("saw_chords"), bus=M, gain_db=-5.0,
               eq=[("hp", 280, 2), ("peak", 600, -2.5, 1.0), ("peak", 3500, 1.0, 0.8)],
               sends={"hall": -12}, width=1.35, sidechain=sc_pump)
    mx.channel("brass", stems.get("brass"), bus=M, gain_db=-5.0, eq=[("hp", 180, 2), ("peak", 1500, 1.5, 1)],
               sends={"plate": -10}, width=1.2)
    mx.channel("pluck", stems.get("pluck"), bus=M, gain_db=-9.0, eq=[("hp", 300, 2), ("peak", 4000, 1.0, 1)],
               sends={"delay": -12, "plate": -18}, autopan=dict(rate_beats=4.0, depth=0.4), sidechain=sc_soft)
    mx.channel("chip_arp", stems.get("chip_arp"), bus=M, gain_db=-11.0, eq=[("hp", 350, 2), ("lp", 9000, 2)],
               sends={"delay": -10}, pan=-0.35)
    mx.channel("guitar", stems.get("guitar"), bus=M, gain_db=-7.0,
               eq=[("hp", 95, 2), ("peak", 250, -2.0, 1.0), ("peak", 2600, -2.0, 1.2), ("lp", 9000, 2)],
               sidechain=dict(source="kick", depth=2.0, release=100))
    mx.bus(M, comp=dict(thresh=-20, ratio=1.8, attack=20, release=150, knee=8, sc_hp=120),
           automation=None if loop else {"lpf": [(0, 1000), (7.0, 2200), (8, 20000)]})

    L = "lead"
    mx.channel("lead", stems.get("lead"), bus=L, gain_db=0.5,
               eq=[("hp", 200, 2), ("peak", 450, -2.0, 1.0), ("peak", 2600, 2.0, 0.9), ("highshelf", 10000, 1.5)],
               comp=dict(thresh=-18, ratio=3, attack=5, release=90), sends={"plate": -12, "delay": -10})
    mx.channel("lead_saw", stems.get("lead_saw"), bus=L, gain_db=-5.0,
               eq=[("hp", 150, 2), ("peak", 350, -2.5, 1.0), ("lp", 7000, 2)],
               comp=dict(thresh=-18, ratio=3, attack=5, release=90), sends={"plate": -16})
    mx.channel("celesta", stems.get("celesta"), bus=L, gain_db=-3.0, eq=[("hp", 400, 2), ("highshelf", 9000, 1.5)],
               sends={"hall": -9, "delay": -14}, pan=0.1)
    mx.channel("beep", stems.get("beep"), bus=L, gain_db=-8.0, eq=[("hp", 400, 2)], sends={"delay": -8},
               pan=0.3)
    mx.channel("glock", stems.get("glock"), bus=L, gain_db=-6.0, eq=[("hp", 700, 2)],
               sends={"hall": -10, "delay": -14}, pan=-0.2)
    mx.bus(L, comp=dict(thresh=-20, ratio=2, attack=15, release=120))

    F = "fx"
    mx.channel("fx", stems.get("fx"), bus=F, gain_db=-4.0, eq=[("hp", 40, 2)], sends={"hall": -14})
    mx.channel("scratch", stems.get("scratch"), bus=F, gain_db=-6.0, eq=[("hp", 150, 2), ("peak", 2500, 2, 1)],
               sends={"plate": -16}, pan=0.2)
    mx.channel("hey", stems.get("hey"), bus=F, gain_db=-7.0,
               eq=[("hp", 160, 2), ("peak", 300, -2, 1), ("peak", 2800, 2.5, 1)],
               comp=dict(thresh=-16, ratio=4, attack=3, release=60), sends={"plate": -8, "room": -10}, width=1.3)
    return mx


def main():
    t0 = time.time()
    s = build()
    s.humanize(timing_ms=5.0, vel=0.05, seed=11,
               tracks=[n for n in s.tracks if n not in ("kick", "fx", "scratch", "hey")])
    s.swing(0.08, grid=0.25, tracks=["hats", "perc", "pluck", "chip_arp"])
    midi = s.export_midi(os.path.join(OUT, "midi"))
    cache = os.path.join(OUT, "raw_stems.npz")
    if "--remix" in sys.argv and os.path.exists(cache):
        z = np.load(cache)
        stems = {k: z[k].astype(np.float64) for k in z.files}
    else:
        stems = kit.render_score(s)
        np.savez(cache, **{k: v.astype(np.float32) for k, v in stems.items()})
    mx = mix(s, stems)
    pre = mx.render()
    if "--levels" in sys.argv:
        print(qa.format_levels(qa.stem_levels(mx.stems)))
    kit.write_wav(os.path.join(OUT, "groove16_premaster.wav"), pre / max(1e-9, np.abs(pre).max()) * 0.5)
    chain = dict(eq=[("hp", 26.0, 2), ("lowshelf", 75.0, 1.5), ("peak", 320.0, -0.8, 0.8),
                     ("peak", 3200.0, -0.3, 0.7), ("highshelf", 11000.0, -0.5)])
    y, mrep = kit.master(pre, target_lufs=-14.0, tp_max=-1.0, chain=chain, verbose=True)
    files = kit.export(y, os.path.join(HERE, "groove16"))
    os.makedirs(os.path.join(OUT, "stems"), exist_ok=True)
    for k, v in mx.bus_out.items():
        kit.write_wav(os.path.join(OUT, "stems", f"bus_{k}.wav"), v * 0.5)
    rep = qa.report(y, stems={k: v for k, v in mx.stems.items()}, score=s,
                    path_wav=files["wav"], png=os.path.join(HERE, "groove16_spectrogram.png"),
                    title="groove16 - J-pop groove, 150 BPM, Ab major (kit demo)", ref="jpop")
    rep["spectral_balance_vs_pop"] = qa.spectral_balance(y, ref="pop")["regions"]
    rep["master"] = mrep
    rep["files"] = files
    rep["midi"] = midi
    # ---- seamless 8-bar chorus loop (game / show bumper use) + seam check
    ls = loop_score(s)
    lstems = kit.render_score(ls, verbose=False)
    lmx = mix(ls, lstems, loop=True)
    lpre = lmx.render()
    lm = kit.master(lpre, target_lufs=-14.0, tp_max=-1.0, chain=chain)[0]
    a, b = ls.b2n(32), ls.b2n(64)
    loop = lm[a:b]
    kit.export(loop, os.path.join(OUT, "groove16_chorus_loop"), mp3=False)
    seam = qa.loop_seam(loop, reference=(lm, a))      # wrap vs. pass1 -> pass2 transition
    naive = qa.loop_seam(lm[:a], reference=(lm, a))   # control: pass 1 alone (dry start)
    rep["loop_seam"] = dict(loop=seam, first_pass_without_wrap=naive)
    txt = qa.format_report(rep)
    txt += (f"\nloop seam (8-bar chorus loop, render-twice): jump {seam['sample_jump']:.4f} "
            f"(music's own p99.9 step {seam['typical_step_p999']:.4f}), band-envelope diff vs continuous "
            f"{seam['wrap_vs_continuous_db']:.2f} dB mean / {seam['wrap_vs_continuous_p95_db']:.1f} dB p95, "
            f"start tail deficit {seam['start_tail_deficit_db']:+.2f} dB -> seamless={seam['seamless']}"
            f"\n   control (pass 1 cut naively, dry start): diff {naive['wrap_vs_continuous_db']:.2f} dB, "
            f"tail deficit {naive['start_tail_deficit_db']:+.2f} dB -> seamless={naive['seamless']}")
    with open(os.path.join(HERE, "groove16_qa.json"), "w") as fh:
        json.dump(rep, fh, indent=1, default=float)
    with open(os.path.join(HERE, "groove16_qa.txt"), "w") as fh:
        fh.write(txt + "\n")
    print(txt)
    print(f"done in {time.time() - t0:.0f}s")


if __name__ == "__main__":
    main()
