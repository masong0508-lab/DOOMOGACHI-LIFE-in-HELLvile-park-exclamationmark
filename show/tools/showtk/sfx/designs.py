"""The show's own SFX designs (WP10, sec.10.5): every comedy, gore and foley sound
the pilot calls for (docs/show/PILOT_BEATSHEET.md, COLD_OPEN_SHOTLIST.md,
GAG_BIBLE.md sec.4.8, 4.15, 9), synthesised at 48 kHz from primitives and,
where the game already has the sound, built on the game's own (sfxgen.py PCM,
audio_fx.c SynthSeg) so the show and the game share a vocabulary.

Every design is `def name(r) -> mono or stereo float array`, deterministic
(r is seeded from the name). The library normalises peaks and writes files.

Two registers (GAG_BIBLE.md sec.4.15): the FILTER (HaHaland: squeaks, sprinkles,
bike bells, confetti, plush) and the TRUTH (wet, heavy, DOOM). Gore lands on
demons only; nothing here is a cute citizen's or the pet's harm.
"""
from __future__ import annotations

import numpy as np

from . import gamesfx as G
from .synth import (SR, at, bell, bp, brown, bubbles, click, crackle, drive, env_ar, env_lin, fade,
                    fm, gust, hp, ks, lp, mix, n_of, pink, pitch_shift_resample, place, reson,
                    reverb, rng, saw, sine, square, squeak, stereo, sweep, svf_sweep, t_of, thump,
                    tri, wet, white)

DESIGNS = {}


def design(fn):
    DESIGNS[fn.__name__] = fn
    return fn


def _game(name):
    y, _r0, _pcm = G.sfxgen(name)
    return y


def _kit(path):
    from ..music.kitenv import kit
    x = kit.load(path)
    return x.mean(axis=1) if x.ndim == 2 else x


# =============================================================== the FILTER: cute comedy
@design
def boing_big(r):
    """The gate bulging outward once (PC-1), the suitcase's bigger cousin: a low
    spring boing with a metal shiver."""
    n = n_of(0.9)
    t = t_of(n)
    f = 70 * (1 + 0.9 * np.exp(-t / 0.05)) * (1 + 0.18 * np.sin(2 * np.pi * 9 * t) * np.exp(-t / 0.4))
    y = sine(f) * env_ar(n, 0.003, 0.7) + 0.35 * tri(f * 2) * env_ar(n, 0.003, 0.4)
    metal = bell(410, 0.9, ((1, 1, 1), (2.43, 0.6, 0.5), (3.9, 0.4, 0.3)), r, 0.1) * 0.25
    return drive(y + metal, 1.4)


@design
def suitcase_land(r):
    """The suitcase lands 8 f after Sunny and bounces (1-01)."""
    y = np.zeros(n_of(0.6))
    place(y, thump(160, 70, 0.18, 0.6, r), 0.0)
    place(y, reson(click(4, r, 2500), [180, 420, 760], 10) * 0.6, 0.0)
    place(y, thump(180, 90, 0.12, 0.4, r) * 0.45, 0.22)
    return y


@design
def duck_squeak(r):
    """A rubber duck: the unknown duck squeaks by itself (1-04)."""
    return squeak(1150, 980, 0.22, r, vib=11, vib_depth=0.02)


@design
def duck_squeak_answer(r):
    """Steamy squeaks one back at it, nervously (higher, shorter, twice)."""
    y = np.zeros(n_of(0.32))
    place(y, squeak(1350, 1250, 0.09, r), 0.0)
    place(y, squeak(1400, 1200, 0.12, r, vib=14), 0.14)
    return y


def _roller(r, f):
    n = n_of(0.45)
    sq = squeak(f, f * 0.92, 0.45, r, vib=6, vib_depth=0.015, bright=0.8)
    wetroll = bp(pink(n, r), 900, 1.2) * env_lin(n, [(0, 0), (0.03, 0.4), (0.4, 0.3), (0.45, 0)])
    return sq + 0.5 * wetroll


@design
def roller_squeak_1(r):
    """The paint roller's squeak, first of three, each lower (CO-4, CO-5)."""
    return _roller(r, 880)


@design
def roller_squeak_2(r):
    return _roller(r, 740)


@design
def roller_squeak_3(r):
    return _roller(r, 587)


@design
def roller_squeak_long(r):
    """CO-4's roller crossing the banner in 30 f (+15..+45)."""
    n = n_of(1.05)
    a = squeak(820, 760, 1.0, r, vib=5, vib_depth=0.02, bright=0.8)
    wetroll = bp(pink(n, r), 900, 1.2) * env_lin(n, [(0, 0), (0.05, 0.4), (1.0, 0.3), (1.05, 0)])
    return mix(a, 0.5 * wetroll)


@design
def balloon_creak(r):
    """Rubber creak 2 f after the cut back (co18): the balloon has turned."""
    n = n_of(0.35)
    t = t_of(n)
    f = 95 + 25 * np.sin(2 * np.pi * 3 * t)
    stick = (np.sin(2 * np.pi * np.cumsum(f) / SR * 1.0) > 0.92).astype(float)
    y = reson(stick * 0.8 + 0.1 * white(n, r), [620, 1300, 2100], q=9)
    return y * env_lin(n, [(0, 0), (0.02, 1), (0.3, 0.6), (0.35, 0)])


@design
def balloon_squeak(r):
    """He turns it back: a rubbery squeak (co18 +21)."""
    return squeak(700, 900, 0.18, r, vib=16, vib_depth=0.04, bright=0.9)


@design
def balloon_pop_far(r):
    """Far off, a balloon pops (1-13): distant, small, lonely."""
    n = n_of(0.08)
    y = hp(white(n, r), 800) * np.exp(-t_of(n) / 0.008)
    return reverb(lp(y, 3000), 1.8, 0.75, "hall")


@design
def pat_pat(r):
    """Two pats on the balloon (co15 +140)."""
    y = np.zeros(n_of(0.35))
    for i, s in enumerate((0.0, 0.16)):
        n = n_of(0.06)
        p = lp(white(n, r), 1600) * np.exp(-t_of(n) / 0.012) + 0.4 * sine(220, n) * np.exp(-t_of(n) / 0.02)
        place(y, p * (1.0 - 0.15 * i), s)
    return y


@design
def ladder_clack(r):
    """Ladder clacks in threes (co18 +62 +67 +72)."""
    y = np.zeros(n_of(0.45))
    for i, s in enumerate((0.0, 0.167, 0.333)):
        place(y, reson(click(3, r, 3000), [760, 1240, 2300], 14) * (1 - 0.1 * i), s)
    return y


@design
def drip_tick(r):
    """The wet paint drip (co15 +0): a tiny wet tick."""
    n = n_of(0.08)
    t = t_of(n)
    return np.sin(2 * np.pi * np.cumsum(sweep(1400, 2600, n)) / SR) * np.exp(-t / 0.015)


@design
def wet_blip_rise(r):
    """A wet blip on each bloom, rising (co15 +15 +45 +76 +107): 4 takes in one file,
    0.5 s apart (cut them by onset, or play the take you need)."""
    y = np.zeros(n_of(2.0))
    for i, f in enumerate((500, 640, 820, 1050)):
        n = n_of(0.12)
        t = t_of(n)
        b = np.sin(2 * np.pi * np.cumsum(sweep(f * 0.6, f * 1.4, n)) / SR) * np.exp(-t / 0.03)
        place(y, b + 0.3 * bubbles(0.12, 30, f, f * 1.5, r), i * 0.5)
    return y


@design
def bowl_clink(r):
    """Two small bowls set down, 15 f apart (CO-2): crayon-soft ceramic clinks."""
    y = np.zeros(n_of(0.9))
    for i, (s, f) in enumerate(((0.0, 1850), (0.5, 1720))):
        b = bell(f, 0.6, ((1, 1, 1), (2.32, 0.4, 0.5), (4.25, 0.2, 0.3)), r, 0.2)
        place(y, b * 0.8, s)
    return lp(y, 7000)


@design
def firework_thump_soft(r):
    """A crayon firework: a soft thump with a fizzy bloom, never a bang (CO-2)."""
    n = n_of(1.4)
    t = t_of(n)
    th = thump(70, 38, 0.5, 0.15, r) * 0.9
    fizz = hp(pink(n, r), 2500) * env_lin(n, [(0, 0), (0.08, 0.0), (0.25, 0.35), (1.4, 0)])
    sparks = crackle(1.4, 40, r, 5000, (0.05, 0.3)) * env_lin(n, [(0, 0), (0.2, 1), (1.4, 0)])
    return reverb(mix(th, fizz * 0.5, sparks * 0.6), 1.5, 0.35, "hall")


@design
def firework_heart_bloom(r):
    """4-08's pink heart: 'a soft thump on frame 0 (no bang)' and a shimmering bloom."""
    n = n_of(2.6)
    th = thump(60, 34, 0.7, 0.05, r)
    shimmer = sum(bell(f, 2.4, ((1, 1, 1), (2.0, 0.3, 0.6)), r, 0.0) * 0.18
                  for f in (1318.5, 1567.98, 2093.0, 2637.0))
    sh = np.pad(shimmer, (0, n))[:n] * env_lin(n, [(0, 0), (0.15, 1), (2.6, 0)])
    return reverb(mix(th, sh), 2.2, 0.4, "hall")


@design
def firework_far_answer(r):
    """From the south, a small purple firework goes up in answer: a far whistle and a
    tiny pop (4-08)."""
    n = n_of(2.2)
    t = t_of(n)
    wh = sine(sweep(900, 1800, n_of(0.9)), None) * env_lin(n_of(0.9), [(0, 0), (0.1, 0.4), (0.9, 0.2)])
    y = np.zeros(n)
    place(y, wh * 0.4, 0.0)
    place(y, thump(110, 60, 0.3, 0.2, r) * 0.5, 0.92)
    place(y, crackle(0.9, 30, r, 4500, (0.05, 0.2)), 0.95)
    return reverb(lp(y, 5000), 2.5, 0.65, "hall")


@design
def rocket_climb(r):
    """The rocket climbs over 26 f (4-08): a fuse hiss into a soft rising whistle."""
    n = n_of(0.95)
    t = t_of(n)
    hiss = hp(white(n, r), 3000) * 0.25 * env_lin(n, [(0, 0.2), (0.85, 0.6), (0.95, 0)])
    wh = sine(sweep(700, 2400, n)) * 0.35 * env_lin(n, [(0, 0), (0.1, 0.5), (0.85, 0.7), (0.95, 0)])
    return hiss + wh


@design
def fuse_light(r):
    """He lights it: a match strike and a fizzing fuse (4-08)."""
    n = n_of(1.6)
    strike = hp(white(n_of(0.08), r), 1500) * np.exp(-t_of(n_of(0.08)) / 0.02)
    fz = bp(white(n, r), 4200, 1.5) * (0.4 + 0.2 * np.sin(2 * np.pi * 23 * t_of(n))) \
        * env_lin(n, [(0, 0), (0.1, 0.5), (1.6, 0.4)])
    y = np.zeros(n)
    place(y, strike, 0.0)
    return y + crackle(1.6, 25, r, 5000, (0.05, 0.25)) + fz * 0.6


@design
def bus_horn_far(r):
    """A far bus horn: the first two notes of the theme's chorus hook, B3 E4 (CO-6, shot 21 +15).
    The pitches are the master's ("Doom-tas-": B, E): swap at conform if the hook moves."""
    return _horn(r, [("B3", 0.0, 0.32), ("E4", 0.4, 0.55)], far=True)


@design
def bus_horn_near(r):
    """The horn again, nearer: the first three notes, B3 E4 E4 (shot 21 +105)."""
    return _horn(r, [("B3", 0.0, 0.3), ("E4", 0.38, 0.28), ("E4", 0.7, 0.55)], far=False)


def _horn(r, notes, far):
    from ..music.kitenv import kit
    tot = max(s + d for _, s, d in notes) + 0.6
    y = np.zeros(n_of(tot))
    for nm, s, d in notes:
        f = float(kit.mtof(kit.note(nm)))
        n = n_of(d)
        t = t_of(n)
        src = 0.6 * square(f, n, 0.45) + 0.4 * saw(f * 1.004, n)
        src = reson(src, [f * 2, 820, 1450], q=4, gains=[0.5, 1.0, 0.7]) + 0.3 * src
        e = env_lin(n, [(0, 0), (0.02, 1), (d - 0.04, 0.9), (d, 0)])
        place(y, drive(src * e, 1.8), s)
    if far:
        y = reverb(lp(y, 1800), 2.4, 0.7, "hall") * 0.6
    else:
        y = reverb(lp(y, 4200), 1.2, 0.35, "hall")
    return y


@design
def egg_wobble(r):
    """The egg wobbles in its basket: a ceramic-ish rock and a TIK (shot 21; 2-04)."""
    y = np.zeros(n_of(0.5))
    for i, s in enumerate((0.0, 0.1, 0.2)):
        place(y, reson(click(2, r, 3000), [1100 - 120 * i, 2600], 12) * (0.8 - 0.2 * i), s)
    place(y, G.synth("tik") * 0.35, 0.02)
    place(y, lp(white(n_of(0.3), r), 900) * 0.08 * env_ar(n_of(0.3), 0.01, 0.2), 0.0)
    return y


@design
def egg_wobble_low(r):
    """The second, bigger wobble: the TIK a step lower."""
    return pitch_shift_resample(DESIGNS["egg_wobble"](r), 0.84) * 1.0


@design
def basket_creak(r):
    """Shot 21 +0: the wicker basket creaks."""
    n = n_of(0.5)
    t = t_of(n)
    k = (np.sin(2 * np.pi * np.cumsum(60 + 30 * np.sin(2 * np.pi * 2 * t)) / SR) > 0.95).astype(float)
    return reson(k + 0.05 * white(n, r), [900, 1700, 3100], 7) * env_lin(n, [(0, 0), (0.05, 1), (0.5, 0)])


@design
def lamp_tink(r):
    """Nana sets the small lamp down (shot 20 +110)."""
    return bell(2350, 0.7, ((1, 1, 1), (2.6, 0.5, 0.5), (5.1, 0.3, 0.3)), r, 0.4)


@design
def oven_tik(r):
    """The bakery oven ticks (shot 20 +0)."""
    return mix(reson(click(1.5, r, 4000), [2900, 5100], 18) * 0.8, np.zeros(n_of(0.12)))


@design
def towel_rustle(r):
    """Nana's woolly paw tucks the tea towel round the egg."""
    n = n_of(1.4)
    g = crackle(1.4, 160, r, 2600, (0.05, 0.4))
    return lp(g, 5000) * env_lin(n, [(0, 0), (0.2, 1), (1.2, 0.8), (1.4, 0)])


@design
def lantern_fwip(r):
    """A lantern string is strung up (shot 19 +0): a soft rope fwip."""
    n = n_of(0.25)
    return bp(white(n, r), 1800, 1.0) * env_lin(n, [(0, 0), (0.05, 1), (0.25, 0)])


@design
def card_pop(r):
    """The act card's clipboard lines pop in, 8 f apart: a soft paper POP."""
    return mix(G.synth("pop") * 0.6, 0.4 * lp(click(4, r, 1200), 3000))


@design
def ticket_flutter(r):
    """A pink ticket flutters onto Sunny's head and lands on a 4-f squash (1-02)."""
    n = n_of(0.9)
    t = t_of(n)
    fl = bp(white(n, r), 2200, 1.5) * (0.5 + 0.5 * np.sin(2 * np.pi * 14 * t)) ** 2
    y = fl * env_lin(n, [(0, 0), (0.1, 0.6), (0.75, 0.5), (0.8, 0)])
    place(y, lp(click(5, r, 800), 2500) * 0.7, 0.8)
    return y


@design
def number_fall(r):
    """The '1' falls off the town sign: a small wooden clatter (1-02)."""
    y = np.zeros(n_of(0.7))
    for i, s in enumerate((0.0, 0.11, 0.19, 0.25)):
        place(y, reson(click(3, r, 2500), [520, 1150, 1980], 12) * (1.0 - 0.2 * i), s)
    return y


@design
def number_roll(r):
    """...and rolls away downhill toward the lanterns (1-02)."""
    n = n_of(2.4)
    t = t_of(n)
    rate = 9 * np.exp(-t / 1.5) + 2
    ticks = (np.sin(2 * np.pi * np.cumsum(rate) / SR) > 0.97).astype(float)
    y = reson(ticks, [600, 1300], 9) * env_lin(n, [(0, 0.9), (2.4, 0.0)])
    return y + 0.05 * lp(brown(n, r), 300) * env_lin(n, [(0, 1), (2.4, 0)])


@design
def pencil_scratch(r):
    """In pencil, someone has added 'and Sunny.' (1-02): a graphite scratch."""
    n = n_of(1.1)
    t = t_of(n)
    strokes = (0.5 + 0.5 * np.sin(2 * np.pi * 5.5 * t)) ** 3
    return hp(bp(white(n, r), 5200, 1.8), 1500) * strokes * env_lin(n, [(0, 0), (0.05, 1), (1.0, 1), (1.1, 0)])


@design
def page_soggy(r):
    """A soggy page turns in the fountain basin (1-03)."""
    n = n_of(0.8)
    y = wet(0.8, r, 160, 900, 0.6, 0.6) * 0.6
    return y + 0.3 * lp(crackle(0.8, 60, r, 1500, (0.05, 0.3)), 2500)


def _fountain(r, n_hee):
    """The Giggling Fountain: two stone faces giggling water, the HEE HEE bell under it."""
    dur = 0.32 * n_hee + 0.5
    y = np.zeros(n_of(dur))
    for k in range(n_hee):
        s = k * 0.32
        n = n_of(0.24)
        t = t_of(n)
        f0 = 620 - 30 * k
        burble = bubbles(0.24, 70, f0, f0 * 1.8, r) * env_lin(n, [(0, 0), (0.02, 1), (0.2, 0.6), (0.24, 0)])
        voiced = reson(saw(f0 * 0.5 * (1 + 0.04 * np.sin(2 * np.pi * 7 * t)), n), [f0 * 1.4, 2400], 6) \
            * env_lin(n, [(0, 0), (0.03, 0.4), (0.18, 0.2), (0.24, 0)])
        place(y, burble + 0.5 * voiced, s)
        place(y, bell(1318.5 if k % 2 == 0 else 1108.7, 0.5, r=r, strike=0.2) * 0.25, s)
    return reverb(y, 1.0, 0.25, "room")


@design
def fountain_giggle(r):
    """The fountain giggles back, late (CO-5 shot 14; 1-03; 1-08): exactly R8 (267 ms)
    after its cue, placed sample-exact by the mix. Caption [the fountain giggles, late]."""
    return _fountain(r, 3)


@design
def fountain_hee(r):
    """...and once on time: '...Hee.' (1-03)."""
    return _fountain(r, 1)


@design
def tik_blink(r):
    """Kevin's third eye blinks: a tiny TIK (1-05)."""
    return G.synth("tik") * 0.7


@design
def spotlight_on(r):
    """A spotlight finds Sunny (1-08): a stage-lamp clunk and hum."""
    n = n_of(1.2)
    y = np.zeros(n)
    place(y, reson(click(6, r, 1500), [180, 410, 900], 8), 0.0)
    hum = (sine(120, n) * 0.2 + sine(240, n) * 0.08) * env_lin(n, [(0, 0), (0.05, 1), (1.2, 0)])
    return y + hum


@design
def bite_crunch_1(r):
    """The bite test, crunch 1 of 3 (1-09): each bigger, the pitch stepping down."""
    return _crunch(r, 1.0, 1.0)


@design
def bite_crunch_2(r):
    return _crunch(r, 0.86, 1.35)


@design
def bite_crunch_3(r):
    """...the third: 'that's a button,' then a CRUNCH."""
    return _crunch(r, 0.72, 1.9)


def _crunch(r, pitch, size):
    dur = 0.18 * size
    n = n_of(dur)
    c = crackle(dur, 380 * size, r, 2600 * pitch, (0.2, 1.0))
    body = lp(pink(n, r), 1200 * pitch) * env_ar(n, 0.002, dur * 0.6) * 0.5
    return drive((c + body) * env_lin(n, [(0, 0), (0.003, 1), (dur, 0)]), 1.5)


@design
def xray_tone(r):
    """The 4-f x-ray insert's tone (1-09): owns the audio for its 133 ms."""
    n = n_of(0.134)
    return (square(1760, n, 0.25) * 0.5 + sine(880, n) * 0.5) * env_lin(n, [(0, 0), (0.004, 1), (0.12, 1), (0.134, 0)])


@design
def clipboard_clip(r):
    """He clips the pages in behind his schedule (1-12); the clipboard insert."""
    y = np.zeros(n_of(0.3))
    place(y, reson(click(2, r, 4000), [2100, 3800], 15), 0.0)
    place(y, reson(click(2, r, 3000), [1500, 2900], 12) * 0.7, 0.06)
    return y


@design
def clock_strike(r):
    """The town clock strikes nine (1-13 frame 0): one deep bronze stroke that rings."""
    y = bell(130.8, 6.0, ((0.5, 0.6, 1.3), (1, 1, 1), (1.19, 0.5, 0.8), (1.5, 0.45, 0.7),
                          (2.0, 0.6, 0.6), (2.52, 0.3, 0.4), (3.0, 0.2, 0.3)), r, 0.5)
    return reverb(y, 3.0, 0.4, "hall")


@design
def cricket(r):
    """One cricket (1-13)."""
    n = n_of(0.6)
    t = t_of(n)
    chirps = ((np.sin(2 * np.pi * 30 * t) > 0) & (t % 0.3 < 0.12)).astype(float)
    y = sine(4600, n) * chirps * (0.5 + 0.5 * np.sin(2 * np.pi * 60 * t))
    return reverb(lp(y, 9000), 1.6, 0.4, "hall")


@design
def pipe_sigh(r):
    """Beyond the Wall a pipe heaves a purple sigh (the 1-13 insert, 4 f, owns the audio):
    a low exhaling pipe, cut to its 133 ms by the insert."""
    n = n_of(0.5)
    t = t_of(n)
    air = svf_sweep(pink(n, r), 380 - 200 * t / t[-1], 4.0, "bp")
    groan = sine(sweep(68, 52, n)) * 0.6
    return (air + groan) * env_lin(n, [(0, 0), (0.01, 1), (0.4, 0.7), (0.5, 0)])


@design
def wall_thump(r):
    """From the Wall: thump (1-13, x4: three 15 f apart, R26, one). Heavy, far, through brick."""
    y = thump(55, 32, 0.6, 0.1, r) + 0.3 * lp(brown(n_of(0.6), r), 160) * env_ar(n_of(0.6), 0.002, 0.25)
    return reverb(lp(y, 900), 1.4, 0.35, "hall")


@design
def foot_thump(r):
    """Mascotty thumps her foot back (1-13): a plush-padded stomp, closer than the Wall."""
    return thump(95, 55, 0.3, 0.2, r) + 0.2 * lp(pink(n_of(0.3), r), 700) * env_ar(n_of(0.3), 0.002, 0.1)


@design
def lantern_out(r):
    """A paper lantern goes out (1-14, the run of six): a little fwoomp and smoke hiss."""
    n = n_of(0.35)
    t = t_of(n)
    f = svf_sweep(pink(n, r), 700 * np.exp(-t / 0.12) + 120, 1.2, "bp")
    return f * env_lin(n, [(0, 0), (0.008, 1), (0.35, 0)]) + 0.1 * hp(white(n, r), 4000) * env_ar(n, 0.01, 0.2)


@design
def eyes_open(r):
    """Red eyes pop open in the fog (1-14): a wet little tick with a low under-tone."""
    n = n_of(0.2)
    return mix(G.synth("tik") * 0.5, 0.5 * sine(sweep(140, 90, n)) * env_ar(n, 0.001, 0.12))


@design
def possess_inhale(r):
    """The possession inhale (1-14): a 10-f stretch, a 5-f hold, FWOOMP into the ribs.
    The FWOOMP lands at 0.50 s (frame 15)."""
    n = n_of(1.3)
    t = t_of(n)
    y = np.zeros(n)
    m = n_of(0.333)
    inh = svf_sweep(pink(m, r), sweep(500, 2800, m), 2.0, "bp") * env_lin(m, [(0, 0), (0.33, 1)])
    place(y, inh * 0.6, 0.0)
    hold = hp(white(n_of(0.167), r), 3000) * 0.08
    place(y, hold, 0.333)
    fw = n_of(0.8)
    tt = t_of(fw)
    whoomp = svf_sweep(pink(fw, r), 900 * np.exp(-tt / 0.1) + 60, 1.4, "bp") * np.exp(-tt / 0.3)
    place(y, mix(whoomp, 0.6 * thump(80, 40, 0.5, 0.0, r)[:fw]), 0.5)
    pos = _game("POSSESS")
    place(y, pos * 0.35, 0.5)
    return y


@design
def bone_click(r):
    """The '...Clack.' click: frame 0 of the OUT card (1-14)."""
    return mix(reson(click(2, r, 5000), [1900, 3300, 5200], 22, [1.0, 0.7, 0.4]), np.zeros(n_of(0.15)))


@design
def bone_clack_pair(r):
    """Bonejangles: 'Clack clack!' bone foley (1-08), and his rib taps."""
    y = np.zeros(n_of(0.4))
    for i, s in enumerate((0.0, 0.14)):
        place(y, reson(click(3, r, 4000), [1500 + 200 * i, 2700, 4600], 16), s)
    return y


@design
def hellify_flash(r):
    """The 4-f hellify flash of every IN eyecatch: a glitched whoosh with a sub drop."""
    n = n_of(0.45)
    t = t_of(n)
    g = G.synth("glitch")
    w = svf_sweep(pink(n, r), sweep(3000, 200, n), 1.5, "bp") * env_lin(n, [(0, 0), (0.01, 1), (0.45, 0)])
    sub = sine(sweep(110, 40, n)) * env_ar(n, 0.003, 0.3) * 0.6
    y = np.zeros(n)
    place(y, g * 0.6, 0.0)
    return y + w * 0.6 + sub


@design
def glass_crack(r):
    """The prize-case glass cracks on frame 0 of the OUT card (2-08)."""
    n = n_of(1.0)
    cr = crackle(0.25, 900, r, 4800, (0.2, 1.0))
    ring = sum(bell(f, 1.0, ((1, 1, 1),), r, 0.0) * 0.08 for f in (3120, 4410, 5230, 6870))
    y = np.zeros(n)
    place(y, cr, 0.0)
    place(y, ring[:n], 0.0)
    place(y, reson(click(3, r, 5000), [2400, 4100], 10), 0.0)
    return y


@design
def glass_paw(r):
    """Sunny puts a paw on the glass (2-08): a soft squeak on glass."""
    n = n_of(0.3)
    return squeak(2100, 1900, 0.3, r, vib=25, vib_depth=0.01, bright=1.3) * 0.5 + \
        0.2 * lp(pink(n, r), 1200) * env_ar(n, 0.005, 0.1)


@design
def lid_shut(r):
    """The Whack-an-Imp imps pull their plastic lids shut, 2 f apart (2-08): one clack."""
    return mix(reson(click(2, r, 3500), [1300, 2400, 3900], 11) * 0.9, np.zeros(n_of(0.1)))


@design
def anvil(r):
    """Rivet's anvil (the RIVET motif's sting; the welded prize case)."""
    return bell(880, 1.6, ((1, 1, 1), (2.76, 0.7, 0.6), (4.07, 0.5, 0.4), (5.4, 0.4, 0.3),
                           (7.1, 0.25, 0.2)), r, 0.8)


@design
def candle_set(r):
    """Fuzz sets a candle on the rail, 15 f apart (2-05): a wax-and-brass TINK."""
    return mix(G.synth("tink") * 0.6, 0.4 * bell(1660, 0.5, ((1, 1, 1), (2.4, 0.4, 0.4)), r, 0.3))


@design
def candle_out(r):
    """A candle goes out (2-06, 3-04): pfft and a curl of smoke."""
    n = n_of(0.6)
    t = t_of(n)
    pf = bp(white(n, r), 1300, 0.8) * env_lin(n, [(0, 0), (0.01, 1), (0.12, 0.2), (0.6, 0)])
    hiss = hp(white(n, r), 5000) * 0.1 * env_ar(n, 0.02, 0.4)
    return pf + hiss


@design
def flame_gutter(r):
    """The last flame gutters: frame 0 of EC3-OUT (3-07)."""
    n = n_of(0.5)
    t = t_of(n)
    flick = bp(pink(n, r), 900, 1.0) * (0.5 + 0.5 * np.sin(2 * np.pi * 17 * t)) * env_lin(n, [(0, 1), (0.5, 0)])
    return flick


@design
def flame_shrink(r):
    """The last flame shrinks to a blue bead over 26 f (3-07)."""
    n = n_of(0.9)
    t = t_of(n)
    return svf_sweep(white(n, r), 2600 - 1800 * t / t[-1], q=2.0, mode="bp") * env_lin(n, [(0, 0.8), (0.87, 0.2), (0.9, 0)]) * 0.6


@design
def holy_pour(r):
    """Sunny pours a cupped paw of holy water, gently, onto the ribs (21 f, 3-07)."""
    n = n_of(0.9)
    w = bubbles(0.9, 60, 500, 1400, r) + 0.4 * bp(pink(n, r), 2200, 1.5)
    return mix(w * env_lin(n, [(0, 0), (0.08, 1), (0.7, 0.8), (0.9, 0)]), 0.3 * _game("HOLY"))


@design
def clap_crowd(r):
    """The crowd claps in on the beat (3-07): one clap of a dozen hands, rippled 0-3 f."""
    n = n_of(0.25)
    y = np.zeros(n)
    for k in range(12):
        s = r.uniform(0, 0.1)
        m = n_of(0.03)
        c = bp(white(m, r), r.uniform(1100, 1900), 1.2) * np.exp(-t_of(m) / 0.008)
        place(y, c * r.uniform(0.4, 1.0), s)
    return y


@design
def fuzz_clap(r):
    """Father Fuzz claps his hymn (2-06): plush paws, one clap."""
    m = n_of(0.08)
    return bp(white(m, r), 900, 1.0) * np.exp(-t_of(m) / 0.012) + 0.2 * lp(white(m, r), 500) * np.exp(-t_of(m) / 0.02)


@design
def chalk_scrape(r):
    """Chalk dust falls from his finger bones as he writes (2-07): constant-speed chalk,
    stops mid-word. 4 s loop-able."""
    n = n_of(4.0)
    t = t_of(n)
    strokes = 0.6 + 0.4 * np.sin(2 * np.pi * 3.2 * t + np.sin(2 * np.pi * 0.7 * t))
    sq = bp(white(n, r), 3400, 3.0) * strokes
    squeal = squeak(2900, 2700, 4.0, r, vib=6, vib_depth=0.01, bright=1.2) * 0.08
    return (sq + squeal) * env_lin(n, [(0, 0), (0.03, 1), (3.97, 1), (4.0, 0)])


@design
def wall_clink(r):
    """After 'clink' in the poem, a faint real clink answers from the Wall (2-07)."""
    y = bell(1975, 0.8, ((1, 1, 1), (2.8, 0.5, 0.5), (5.1, 0.3, 0.3)), r, 0.4)
    return reverb(lp(y, 4000), 2.0, 0.6, "hall") * 0.6


@design
def mayor_dive(r):
    """The Mayor dives under a cake table (2-03): a whoosh and a tablecloth flump."""
    y = np.zeros(n_of(0.7))
    place(y, gust(0.35, r, 500, 2200) * 0.7, 0.0)
    place(y, mix(thump(130, 60, 0.25, 0.3, r) * 0.8, 0.4 * crackle(0.3, 90, r, 1800, (0.1, 0.5))), 0.3)
    return y


@design
def demon_squeak(r):
    """A Puff Demon squeaks (2-03, 3-01): the game's demon pain, pitched up and shortened."""
    return pitch_shift_resample(_game("DEMON_PAIN"), 1.6)


@design
def teacups_reverse(r):
    """The teacups spin backwards (2-03): a fairground ride's motor and creak, reversed. 6 s loop."""
    n = n_of(6.0)
    t = t_of(n)
    motor = saw(55 + 3 * np.sin(2 * np.pi * 0.5 * t), n) * 0.25 + sine(110, n) * 0.15
    clank = np.zeros(n)
    for k in range(int(6.0 / 0.5)):
        place(clank, reson(click(3, r, 2000), [700, 1500], 10) * 0.4, k * 0.5 + 0.1)
    whirl = svf_sweep(pink(n, r), 600 + 300 * np.sin(2 * np.pi * 0.66 * t), 2.0, "bp") * 0.4
    y = lp(motor, 900) + clank + whirl
    return y[::-1].copy()


@design
def confetti_spray(r):
    """A Puff Demon is sucked into a teacup and sprays out as confetti (2-03)."""
    y = _game("CONFETTI")
    pad = np.zeros(n_of(0.9))
    place(pad, y, 0.0)
    place(pad, crackle(0.8, 220, r, 5000, (0.05, 0.4)) * env_lin(n_of(0.8), [(0, 1), (0.8, 0)]) * 0.6, 0.05)
    return pad


@design
def cardboard_turn(r):
    """Every cardboard ghost turns to face the Wall (2-03): a ripple of cardboard flaps."""
    y = np.zeros(n_of(0.8))
    for k in range(6):
        n = n_of(0.07)
        f = bp(white(n, r), r.uniform(500, 800), 1.5) * np.exp(-t_of(n) / 0.02)
        place(y, f * r.uniform(0.6, 1.0), k * 0.09 + r.uniform(0, 0.03))
    return y


@design
def gust_tear(r):
    """A gust of fog tears the bandstand boards loose (2-03)."""
    y = np.zeros(n_of(1.6))
    place(y, gust(1.2, r, 250, 1400), 0.0)
    for i, s in enumerate((0.55, 0.7)):
        crack = reson(crackle(0.12, 400, r, 1800, (0.3, 1.0)), [300, 700, 1600], 6)
        place(y, crack * (1.0 - 0.3 * i), s)
    return y


@design
def crate_thud(r):
    """A crate stencilled MINGLE, DO NOT OPEN is revealed (2-03)."""
    return mix(thump(110, 55, 0.35, 0.6, r), reson(click(5, r, 1200), [210, 470, 880], 8) * 0.6)


@design
def string_snap(r):
    """The gust snaps a lantern string (2-03, background)."""
    n = n_of(0.4)
    tw = ks(620, 0.4, 0.990, 0.3, r) * 0.6
    return mix(tw, hp(click(2, r, 5000), 2000) * 0.5)


@design
def hatch(r):
    """The egg hatches on the RIFF's downbeat (2-04): the game's EGG_CRACK, a shell pop
    and a sparkle (no beep: the pet's "beep." is its own raw sound)."""
    y = np.zeros(n_of(1.0))
    place(y, _game("EGG_CRACK"), 0.0)
    place(y, crackle(0.12, 500, r, 3500, (0.3, 1.0)) * 0.7, 0.0)
    place(y, _game("SPARKLE") * 0.4, 0.05)
    return y


# =============================================================== the TRUTH: DOOM register
@design
def shotgun_blast(r):
    """The Pump Shotgun as it really is (3-03, 3-05): the game's SHOTGUN at 48 kHz under
    a sub kick, a crack and a room tail (the trailer's blast recipe)."""
    n = n_of(1.2)
    y = np.zeros(n)
    place(y, _game("SHOTGUN") * 0.8, 0.0)
    place(y, thump(70, 35, 0.6, 0.9, r) * 0.9, 0.0)
    crack = hp(white(n_of(0.02), r), 1500) * np.exp(-t_of(n_of(0.02)) / 0.004)
    place(y, crack * 0.9, 0.0)
    return drive(reverb(y, 0.9, 0.25, "room"), 1.6)


@design
def shotgun_pump(r):
    """CHK-CHK: the pump racks (the truth's sound, 3-02, 3-03)."""
    y = np.zeros(n_of(0.45))
    for i, (s, f) in enumerate(((0.0, 1800), (0.17, 1400))):
        m = mix(reson(click(4, r, 3000), [f, f * 1.9, 3800], 14), 0.3 * lp(click(6, r, 600), 900))
        place(y, m * (1.0 if i == 0 else 1.1), s)
    return y


@design
def pump_squeak(r):
    """The Pump Shotgun in the filter: a rubber squeak (3-02) - the last cute sound."""
    return squeak(950, 1250, 0.25, r, vib=12, vib_depth=0.03)


@design
def party_pump_filter(r):
    """A Pump Shotgun blast as the town sees it: sprinkles, a sprinkler hiss and a bike-bell
    ting (GAG_BIBLE.md sec.4.15)."""
    n = n_of(0.8)
    y = np.zeros(n)
    place(y, hp(white(n_of(0.4), r), 3500) * 0.25 * env_ar(n_of(0.4), 0.002, 0.3), 0.0)
    place(y, crackle(0.5, 300, r, 6000, (0.05, 0.3)) * 0.6, 0.0)
    place(y, bell(3150, 0.6, ((1, 1, 1), (2.1, 0.4, 0.5)), r, 0.3) * 0.5, 0.06)
    return y


@design
def gore_burst(r):
    """A Puff Demon bursts into red mist and gibs (3-03, MT-5): wet, heavy, on the beat."""
    n = n_of(1.0)
    y = np.zeros(n)
    place(y, thump(85, 40, 0.4, 0.4, r) * 0.8, 0.0)
    place(y, wet(0.6, r, 200, 3000, 1.2, 1.4), 0.0)
    place(y, crackle(0.5, 120, r, 2200, (0.1, 0.6)) * 0.5, 0.03)
    place(y, _game("SPLAT") * 0.5, 0.0)
    return drive(y, 1.3)


@design
def gib_splat(r):
    """A gib slaps the lens (3-03): close, wet, a slide after."""
    n = n_of(0.7)
    y = np.zeros(n)
    place(y, wet(0.25, r, 300, 2600, 1.4, 0.8), 0.0)
    slide = svf_sweep(pink(n_of(0.45), r), sweep(1400, 500, n_of(0.45)), 2.5, "bp") * 0.25
    place(y, slide, 0.22)
    return y


@design
def eyeball_roll(r):
    """The gore button: an eyeball rolls to a stop facing the Wall (3-03, 3-05)."""
    n = n_of(1.2)
    t = t_of(n)
    rate = 14 * np.exp(-t / 0.45) + 1.0
    ticks = (np.sin(2 * np.pi * np.cumsum(rate) / SR) > 0.985).astype(float)
    sq = reson(ticks, [900, 1800], 6) * 0.7 * env_lin(n, [(0, 1), (1.1, 0.3), (1.2, 0)])
    return mix(sq, 0.2 * wet(0.2, r, 400, 1600, 0.5, 0.4))


@design
def tooth_tink(r):
    """A tooth *tink*s off the lens (3-03)."""
    return bell(4200, 0.35, ((1, 1, 1), (2.3, 0.5, 0.4)), r, 0.6)


@design
def sticker_slap(r):
    """The HELLVILE PARK BOARD OF DECENCY (approved) sticker SLAMs over GORE: BRUTAL (3-03)."""
    return mix(thump(160, 90, 0.12, 0.8, r), 0.6 * lp(click(8, r, 900), 2500))


@design
def paper_rip(r):
    """...and the first shell blows it off in a spray of red: a paper rip inside the blast."""
    n = n_of(0.35)
    return bp(crackle(0.35, 900, r, 3000, (0.2, 1.0)), 2500, 0.7) * env_lin(n, [(0, 0), (0.01, 1), (0.3, 0.5), (0.35, 0)])


@design
def hud_slam(r):
    """The HUD slams in on the insert song's downbeat (3-03)."""
    y = mix(thump(120, 50, 0.3, 0.8, r), bell(330, 0.4, ((1, 1, 1), (2.6, 0.5, 0.5)), r, 0.5) * 0.4)
    return drive(y, 1.5)


@design
def plate_flip(r):
    """The weapon plate flips (PARTY PUMP -> SLAUGHTER, PURRING SAW -> MEATCLEAVER)."""
    y = np.zeros(n_of(0.3))
    place(y, reson(click(3, r, 3000), [1200, 2600], 12), 0.0)
    place(y, reson(click(3, r, 3000), [1500, 3100], 12) * 0.8, 0.07)
    return y


@design
def cardboard_fall(r):
    """A cardboard ghost hit by mistake just falls over flat (3-03)."""
    n = n_of(0.5)
    flap = bp(white(n, r), 380, 0.9) * env_lin(n, [(0, 0), (0.25, 0.3), (0.3, 1), (0.5, 0)])
    return mix(flap, at(thump(120, 70, 0.2, 0.2, r) * 0.5, 0.28))


@design
def hud_heart_pop(r):
    """A hit on Sunny: a heart pops off the HUD (demon blood on the lens)."""
    return mix(G.synth("pop") * 0.7, 0.4 * wet(0.12, r, 600, 2000, 0.8, 0.3))


@design
def click_empty(r):
    """CLICK: empty (3-05). Then R13 of silence."""
    return mix(reson(click(2, r, 4500), [2600, 4200], 20), 0.3 * reson(click(3, r, 2000), [900], 10), np.zeros(n_of(0.15)))


@design
def slot_spin(r):
    """The weapon slot spins to the saw (3-05): a ratchet."""
    n = n_of(0.5)
    y = np.zeros(n)
    for k in range(9):
        place(y, reson(click(2, r, 3000), [2000 + 60 * k, 3500], 14) * (0.6 + 0.04 * k), k * 0.05)
    return y


@design
def saw_rev(r):
    """The welded saw revs: it purrs, then screams (3-05). 2.4 s: idle purr 0-0.8 s,
    the rev climbs to a scream by 1.6 s and holds."""
    n = n_of(2.4)
    t = t_of(n)
    rpm = np.interp(t, [0, 0.8, 1.6, 2.4], [24, 26, 95, 100])
    engine = saw(rpm, n) * 0.5 + square(rpm * 2, n, 0.3) * 0.3
    engine = reson(engine, [180, 420, 900, 1800], q=3, gains=[0.6, 1.0, 0.8, 0.5]) + 0.3 * engine
    chain = hp(white(n, r), 2500) * np.clip((t - 0.8) / 0.8, 0, 1) * 0.3
    saw_pcm = _game("CHAINSAW")
    y = drive(engine + chain, 2.0)
    place(y, saw_pcm * 0.4, 1.5)
    return y * env_lin(n, [(0, 0), (0.05, 0.8), (2.35, 1.0), (2.4, 0)])


@design
def saw_gore(r):
    """It saws the Big Puff top to bottom in a geyser (3-05, ~150 f): the screaming saw, wet
    grind and the geyser. 5 s."""
    n = n_of(5.0)
    t = t_of(n)
    rpm = 98 + 6 * np.sin(2 * np.pi * 0.8 * t) - 20 * np.clip((t - 1.0) / 2, 0, 1) * np.sin(2 * np.pi * 3 * t) ** 2
    engine = drive(reson(saw(rpm, n), [200, 450, 950, 1900], 3) + 0.3 * saw(rpm, n), 2.2)
    grind = wet(5.0, r, 300, 2200, 1.0, 2.0) * 0.9
    geyser = svf_sweep(pink(n, r), 600 + 500 * np.sin(np.pi * t / 5.0), 1.2, "bp") * 0.5
    y = engine * 0.6 + grind + geyser
    return y * env_lin(n, [(0, 0), (0.02, 1), (4.6, 0.9), (5.0, 0)])


@design
def boss_pop(r):
    """The boss pops (3-05: hit-stop 8 f + 1 reduced white frame): the biggest wet burst."""
    n = n_of(2.2)
    y = np.zeros(n)
    place(y, thump(60, 28, 1.2, 0.8, r), 0.0)
    place(y, wet(1.4, r, 150, 3200, 1.5, 2.5), 0.0)
    place(y, _game("EXPLODE") * 0.5, 0.0)
    place(y, crackle(1.6, 200, r, 2400, (0.1, 0.7)) * env_lin(n_of(1.6), [(0, 1), (1.6, 0)]) * 0.6, 0.05)
    return drive(reverb(y, 1.4, 0.3, "hall"), 1.3)


@design
def lens_drip(r):
    """The lens painted red, with a slow drip (3-05)."""
    y = np.zeros(n_of(2.0))
    for k, s in enumerate((0.0, 0.7, 1.5)):
        place(y, mix(DESIGNS["drip_tick"](r) * (0.8 - 0.2 * k), 0.2 * wet(0.1, r, 500, 1500, 0.6, 0.2)), s)
    return y


@design
def rewind(r):
    """REWIND out of hellify (3-05, ~660-681): the kit's CC0 vinyl rewind, 21 f."""
    x = _kit("sonicpi/vinyl_rewind.flac")
    n = n_of(0.9)
    x = x[:n] if len(x) >= n else np.pad(x, (0, n - len(x)))
    return x * env_lin(n, [(0, 1), (0.8, 1), (0.9, 0)])


@design
def record_scratch(r):
    """SCRATCH: the record scratch into the truth (3-02, 3-03, 1-01's brake): the game's
    SynthSeg SCRATCH layered on the kit's CC0 vinyl scratch."""
    y = np.zeros(n_of(0.6))
    place(y, G.synth("scratch") * 0.6, 0.0)
    v = _kit("sonicpi/vinyl_scratch.flac")[: n_of(0.45)]
    place(y, v * 0.8, 0.0)
    return y


@design
def heart_beating(r):
    """One cartoon heart on the grass is beating (3-01's one change): lub-dub, twice."""
    y = np.zeros(n_of(1.6))
    for s in (0.0, 0.8):
        place(y, thump(70, 45, 0.15, 0.1, r), s)
        place(y, thump(80, 50, 0.12, 0.1, r) * 0.7, s + 0.18)
    return lp(y, 600)


@design
def wet_shred(r):
    """The 2-03 truth insert (4 f, owns the audio): the demon shredded in a red spiral."""
    n = n_of(0.4)
    y = wet(0.4, r, 400, 3800, 1.4, 1.5) + 0.6 * crackle(0.4, 700, r, 3000, (0.3, 1.0))
    return drive(y * env_lin(n, [(0, 0), (0.002, 1), (0.13, 0.9), (0.4, 0)]), 1.6)


@design
def wet_pop(r):
    """The 3-01 truth inserts (4 f each): a bubbled demon pops, and a heart falls out."""
    n = n_of(0.4)
    y = G.synth("pop") * 0.4
    y = mix(y, wet(0.4, r, 250, 2600, 1.2, 1.0))
    return y * env_lin(n, [(0, 0), (0.002, 1), (0.13, 0.8), (0.4, 0)])


@design
def bubble_pop_cute(r):
    """In the filter: the bubble pops into hearts and plush stuffing (3-01), POP + SPARKLE."""
    y = np.zeros(n_of(0.6))
    place(y, _game("BUBBLE") * 0.6, 0.0)
    place(y, G.synth("pop") * 0.6, 0.02)
    place(y, _game("SPARKLE") * 0.4, 0.06)
    return y


@design
def bubble_bounce_1(r):
    """Three bubbles bounce off the Big Puff, 15 f apart, each bigger (3-02)."""
    return _bounce(r, 1.0)


@design
def bubble_bounce_2(r):
    return _bounce(r, 0.84)


@design
def bubble_bounce_3(r):
    return _bounce(r, 0.7)


def _bounce(r, k):
    n = n_of(0.35 / k)
    t = t_of(n)
    f = 520 * k * (1 + 0.8 * np.exp(-t / 0.03))
    return mix(sine(f) * env_ar(n, 0.002, 0.25 / k), 0.3 * pitch_shift_resample(G.synth("boing"), k))


@design
def plush_land(r):
    """A tuft of stuffing lands on the tin (3-05); plush things landing."""
    n = n_of(0.3)
    return lp(pink(n, r), 900) * env_lin(n, [(0, 0), (0.01, 1), (0.3, 0)]) * 0.7


# =============================================================== kit-synthesised foley (sec.10.5)
@design
def plush_squash(r):
    """Plush squash: a hug (4-04's 6-f squash, 'no sound but plush'), a pat, a bump."""
    n = n_of(0.45)
    t = t_of(n)
    fab = svf_sweep(pink(n, r), 300 + 900 * np.exp(-t / 0.08), 1.0, "bp")
    return fab * env_lin(n, [(0, 0), (0.03, 1), (0.25, 0.5), (0.45, 0)]) + 0.2 * lp(brown(n, r), 200) * env_ar(n, 0.01, 0.2)


@design
def bone_clack(r):
    """Bone clacks: one rib or finger bone on another."""
    return mix(reson(click(2.5, r, 4000), [1700, 2900, 4400], 18, [1.0, 0.8, 0.5]), np.zeros(n_of(0.12)))


def _padlock(r, size):
    n = n_of(0.5 * size)
    y = np.zeros(n)
    for k in range(int(5 * size)):
        f = r.uniform(1600, 3200) / size ** 0.3
        place(y, bell(f, 0.25, ((1, 1, 1), (2.3, 0.5, 0.4)), r, 0.4) * r.uniform(0.3, 1.0) * 0.4, k * 0.04 + r.uniform(0, 0.02))
    place(y, reson(click(4, r, 2000), [800, 1700], 8) * 0.6 * size, 0.0)
    return y


@design
def padlock_rattle_1(r):
    """Padlock rattles, one, two, three, each bigger (PC-1; MT-5's jump)."""
    return _padlock(r, 1.0)


@design
def padlock_rattle_2(r):
    return _padlock(r, 1.4)


@design
def padlock_rattle_3(r):
    return _padlock(r, 2.0)


@design
def footsteps_small(r):
    """Small footsteps (Pip's three steps back, 8 f apart, then one more; the procession)."""
    y = np.zeros(n_of(0.3))
    place(y, mix(lp(click(6, r, 900), 1800), 0.3 * lp(crackle(0.06, 200, r, 1500, (0.1, 0.4)), 3000)), 0.0)
    return y


@design
def wings_flutter(r):
    """The Mayor's wings fluttering on their idle (co12-co16): 2 s loop."""
    n = n_of(2.0)
    t = t_of(n)
    f = bp(pink(n, r), 700, 0.9) * (0.4 + 0.6 * np.abs(np.sin(2 * np.pi * 6 * t)))
    return f * 0.6


@design
def jelly_door(r):
    """The pink door wobbles like jelly (T-1)."""
    n = n_of(1.2)
    t = t_of(n)
    f = 140 * (1 + 0.25 * np.sin(2 * np.pi * 7 * t) * np.exp(-t / 0.4))
    goo = sine(f) * env_ar(n, 0.01, 0.9) * 0.6 + 0.3 * wet(1.2, r, 150, 600, 0.6, 0.3)
    return goo


@design
def suitcase_set(r):
    """The suitcase set down by the door, still packed: a soft THUD (T-1)."""
    return thump(120, 60, 0.25, 0.3, r) + 0.3 * lp(pink(n_of(0.25), r), 900) * env_ar(n_of(0.25), 0.003, 0.1)


@design
def pet_hop(r):
    """The pet hops into an eggshell half on the windowsill (T-1)."""
    y = np.zeros(n_of(0.4))
    place(y, reson(click(3, r, 2500), [1400, 2900], 10) * 0.6, 0.12)
    place(y, sine(sweep(500, 900, n_of(0.1)), None) * env_ar(n_of(0.1), 0.002, 0.06) * 0.3, 0.0)
    return y


@design
def door_knock(r):
    """A knock (T-2): three knuckles on a cottage door."""
    y = np.zeros(n_of(0.8))
    for k, s in enumerate((0.0, 0.18, 0.34)):
        place(y, reson(click(4, r, 1200), [180, 420, 760, 1300], 9) * (1.0 - 0.1 * k), s)
    return reverb(y, 0.6, 0.2, "room")


@design
def dish_set(r):
    """Her second pie dish, scrubbed clean, set down (T-2)."""
    return mix(bell(1250, 0.8, ((1, 1, 1), (2.41, 0.5, 0.5), (3.9, 0.3, 0.3)), r, 0.5) * 0.7, thump(220, 140, 0.08, 0.3, r) * 0.5)


@design
def tin_open(r):
    """The recipe tin opens (3-06): a tight lid pops and scrapes."""
    y = np.zeros(n_of(0.7))
    place(y, reson(click(3, r, 2500), [900, 2100, 3600], 9), 0.0)
    place(y, bp(white(n_of(0.4), r), 3200, 2.0) * 0.2 * env_lin(n_of(0.4), [(0, 1), (0.4, 0)]), 0.08)
    return y


@design
def paper_fold(r):
    """The Mayor folds it over, fast (3-06 '(+1)', 4-05 'Old draft.'): 4 f."""
    n = n_of(0.2)
    return bp(crackle(0.2, 500, r, 2200, (0.1, 0.6)), 2000, 0.8) * env_lin(n, [(0, 0), (0.01, 1), (0.2, 0)])


@design
def paper_flip(r):
    """The sheet clipped behind flips up (4-05)."""
    n = n_of(0.3)
    return bp(white(n, r), 1500, 0.8) * env_lin(n, [(0, 0), (0.04, 1), (0.3, 0)]) * 0.7


@design
def name_pop(r):
    """Names read down the sign-up sheet at 26 f each, with a small pop (3-06)."""
    return G.synth("pop") * 0.4


@design
def purple_drain(r):
    """The purple drains from the ribs over 21 f (4-02): a descending glassy shimmer."""
    n = n_of(0.9)
    t = t_of(n)
    y = sum(sine(sweep(f, f * 0.5, n)) * 0.2 for f in (1760, 2217, 2637))
    return y * env_lin(n, [(0, 0), (0.05, 1), (0.9, 0)])


@design
def puff_drift(r):
    """The breath leaves as a small, tired puff trailing chalk dust (4-03): 3 s."""
    n = n_of(3.0)
    t = t_of(n)
    return gust(3.0, r, 300, 700) * 0.4 + 0.1 * crackle(3.0, 20, r, 3000, (0.05, 0.2))


@design
def keyhole(r):
    """The puff goes through the keyhole (4-03): air drawn into metal."""
    n = n_of(0.8)
    t = t_of(n)
    return svf_sweep(pink(n, r), sweep(400, 2200, n), 4.0, "bp") * env_lin(n, [(0, 0), (0.6, 0.6), (0.8, 0)])


@design
def brick_wipe(r):
    """The brick wipe down the Wall line: 5 courses, 2 f each (4-08; MT-3): a dry
    masonry slide, one tick per course."""
    y = np.zeros(n_of(0.6))
    for k in range(5):
        place(y, reson(click(5, r, 1500), [350, 800, 1600], 6) * 0.7, k * 0.0667)
    place(y, lp(crackle(0.4, 120, r, 1200, (0.05, 0.3)), 2500) * 0.4, 0.0)
    return y


@design
def mallet_drop(r):
    """The logo letters DROP on the intro hits (MT-1): a toy-block landing."""
    return mix(reson(click(4, r, 1500), [420, 950, 1800], 10), thump(150, 80, 0.12, 0.3, r) * 0.5)


@design
def knock_wall(r):
    """Both sides knock on the Wall on the same beat (MT-3)."""
    return reverb(reson(click(5, r, 900), [140, 330, 700], 7), 0.9, 0.3, "room")


@design
def candle_out_rhythm(r):
    """Three candles go out in rhythm (MT-5): the candle-out, tighter."""
    x = DESIGNS["candle_out"](r)
    return x[: n_of(0.35)]


@design
def gate_rattle(r):
    """Behind the grille a round silhouette presses the gate: the padlocks jump 2 f after
    the BOING (MT-5)."""
    return DESIGNS["padlock_rattle_2"](r)


# =============================================================== Grumbleguts' belly (PC-1)
@design
def wet_crunch(r):
    """A wet crunch (PC-1 ~103): something on the other side eats the LOST PROPERTY lantern."""
    return mix(drive(_crunch(r, 0.6, 2.4), 1.2), wet(0.5, r, 160, 1200, 1.0, 0.8) * 0.8)


@design
def gurgle_deep(r):
    """A gurgle so deep that the padlocks rattle (PC-1 ~110-160): the belly as a tuba
    and a draining sink, 2.5 s."""
    n = n_of(2.5)
    t = t_of(n)
    f = 41 + 6 * np.sin(2 * np.pi * 1.7 * t) + 3 * np.sin(2 * np.pi * 5.3 * t)
    tuba = drive(saw(f, n) * 0.6 + sine(f, n), 2.0)
    tuba = lp(tuba, 380) * env_lin(n, [(0, 0), (0.15, 1), (2.2, 0.8), (2.5, 0)])
    sink = bubbles(2.5, 30, 90, 300, r, rise=0.8) * 0.8 + svf_sweep(brown(n, r), 120 + 80 * np.sin(2 * np.pi * 2.3 * t), 3.0, "bp") * 0.6
    return tuba + sink


@design
def gurgle_babble(r):
    """GURGLE: '...needs salt.' - the belly's bass babble behind the caption box (PC-1 ~180),
    after the game's VOICE_GURGLE preset (lowest pitch, slow, round triangle, wobbly
    intonation): three slow syllables, the last one falling."""
    n = n_of(1.5)
    y = np.zeros(n)
    for k, (s, d, f0, f1) in enumerate(((0.05, 0.28, 68, 74), (0.45, 0.22, 72, 66), (0.8, 0.5, 70, 52))):
        m = n_of(d)
        tt = t_of(m)
        f = sweep(f0, f1, m) * (1 + 0.06 * np.sin(2 * np.pi * 6.5 * tt))
        syl = lp(tri(f) * 0.7 + 0.3 * saw(f), 500) * env_lin(m, [(0, 0), (0.03, 1), (d - 0.05, 0.8), (d, 0)])
        place(y, syl + 0.25 * bubbles(d, 25, 100, 260, r), s)
    return drive(y, 1.5)


@design
def gurgle_tiny(r):
    """'gurgle.' on the Dippinn card's last frame (PC-1, 300): small."""
    n = n_of(0.5)
    return bubbles(0.5, 26, 180, 420, r, rise=1.0) * env_lin(n, [(0, 0), (0.05, 1), (0.5, 0)]) * 0.8


@design
def purr_low(r):
    """A low, satisfied purr behind the gate (PC-1 ~225-250): temp sound until the
    Grumbleguts performer records it (D8)."""
    n = n_of(1.6)
    t = t_of(n)
    am = 0.55 + 0.45 * np.sin(2 * np.pi * 23 * t) ** 2
    rumble = lp(brown(n, r), 260) * am + 0.3 * lp(saw(46 + 2 * np.sin(2 * np.pi * 0.8 * t), n), 300) * am
    return rumble * env_lin(n, [(0, 0), (0.25, 1), (1.3, 0.9), (1.6, 0)])


@design
def lantern_sink(r):
    """The LOST PROPERTY lantern drifts over the Wall and sinks out of sight (PC-1 0-90)."""
    n = n_of(3.0)
    return gust(3.0, r, 200, 500) * 0.25 + 0.15 * sine(sweep(600, 300, n)) * env_lin(n, [(0, 0), (0.5, 0.3), (3, 0)])


# =============================================================== ambiences (stereo loops)
@design
def amb_dawn_birds(r):
    """Dawn birds over HaHaland (CO-5, CO-6): 20 s stereo bed."""
    dur = 20.0
    n = n_of(dur)
    L = np.zeros(n)
    R = np.zeros(n)
    for k in range(70):
        s = r.uniform(0, dur - 0.6)
        f = r.uniform(2400, 5200)
        m = n_of(r.uniform(0.05, 0.18))
        tt = t_of(m)
        chirp = np.sin(2 * np.pi * np.cumsum(f * (1 + r.uniform(-0.3, 0.4) * tt / tt[-1])) / SR) * \
            np.sin(np.pi * tt / tt[-1]) ** 2
        for j in range(r.integers(1, 4)):
            p = r.uniform(-1, 1)
            g = r.uniform(0.1, 0.5)
            place(L, chirp * g * np.cos((p + 1) * np.pi / 4), s + j * 0.09)
            place(R, chirp * g * np.sin((p + 1) * np.pi / 4), s + j * 0.09)
    air = lp(pink(n, r), 1200) * 0.02
    return np.stack([L + air, R + air[::-1]], 1)


@design
def amb_fountain(r):
    """The Giggling Fountain's water (CO-6, 1-03): 15 s stereo bed."""
    dur = 15.0
    n = n_of(dur)
    w = bp(pink(n, r), 1800, 0.6) * 0.3 + bubbles(dur, 30, 300, 1100, r) * 0.25
    w2 = bp(pink(n, rng("fountainR")), 2000, 0.6) * 0.3 + bubbles(dur, 30, 300, 1100, rng("fbR")) * 0.25
    return np.stack([w, w2], 1)


@design
def amb_night_fair(r):
    """The Midsummer Giggle at night (1-09 ... 2-08): ride motors, far tinkles, no voices
    (the crowd is walla, a voice bed). 20 s stereo."""
    dur = 20.0
    n = n_of(dur)
    t = t_of(n)
    hum = lp(saw(50 + 0.5 * np.sin(2 * np.pi * 0.1 * t), n), 300) * 0.05
    L = hum + lp(pink(n, r), 900) * 0.03
    R = hum + lp(pink(n, rng("fairR")), 900) * 0.03
    for k in range(25):
        s = r.uniform(0, dur - 1)
        b = bell(r.uniform(1500, 3000), 0.6, r=r, strike=0.1) * 0.03
        place(L if k % 2 else R, b, s)
    return np.stack([L, R], 1)


@design
def amb_night_crickets(r):
    """Crickets and a still night (1-13, 4-07, 4-08): 20 s stereo."""
    dur = 20.0
    n = n_of(dur)
    L = np.zeros(n)
    R = np.zeros(n)
    c = DESIGNS["cricket"](r)
    for k in range(40):
        s = r.uniform(0, dur - 1)
        g = r.uniform(0.05, 0.25)
        place(L if k % 2 else R, c * g, s)
    return np.stack([L, R], 1)


@design
def amb_fog(r):
    """The fog rolling over the boards (1-14, 2-01): wind and a low drone, 20 s stereo."""
    dur = 20.0
    n = n_of(dur)
    t = t_of(n)
    a = svf_sweep(pink(n, r), 300 + 150 * np.sin(2 * np.pi * 0.07 * t), 1.0, "bp") * 0.4
    b = svf_sweep(pink(n, rng("fogR")), 320 + 150 * np.sin(2 * np.pi * 0.05 * t + 1), 1.0, "bp") * 0.4
    drone = sine(41.2, n) * 0.1 + sine(58.3, n) * 0.06
    return np.stack([a + drone, b + drone], 1)


@design
def amb_funhouse(r):
    """Inside the Absolutely Not Scary House, hellified (3-03): a low room drone, 15 s."""
    dur = 15.0
    n = n_of(dur)
    t = t_of(n)
    d = lp(saw(55 + 0.3 * np.sin(2 * np.pi * 0.2 * t), n), 220) * 0.1
    a = d + bp(pink(n, r), 500, 2.0) * 0.08
    b = d + bp(pink(n, rng("funR")), 520, 2.0) * 0.08
    return np.stack([a, b], 1)


@design
def amb_cottage_dawn(r):
    """Pink Door Cottage at dawn (T-1, T-2): room tone and birds through the window, 15 s."""
    birds = DESIGNS["amb_dawn_birds"](r)[: n_of(15.0)]
    birds = lp(birds, 3500) * 0.4
    room = lp(pink(n_of(15.0), r), 400)[:, None] * 0.015
    return birds + room
