#!/usr/bin/env python3
"""enrich_audio.py - a shared post-mix pass that thickens each scene's audio.

  enrich_audio.py IN.mp4 OUT.mp4 LOCATION MOOD

Adds, under the scene's own mix (dialogue, synth score, sfx):
  * a music bed in the act's colour: pad chords + bass line + arpeggio +
    a countermelody fragment + light percussion with a fill into the cut,
    sweet (C major, HaHaland), cute (glockenspiel), possessed (E minor,
    detuned), sad (F major, slow, no drums) or none;
  * a riser/swell into the scene change and a soft stinger on the downbeat;
  * an ambience bed per location: fair walla + distant rides, night crickets
    + wind, tent canvas flaps, fountain water, bakery hum, hill wind;
  * sweeteners keyed off the scene's own transients: a low sub-thump under
    big hits, a sparkle tinkle on bright pops;
and ducks the new music under the scene's own audio (dialogue first) by
up to 10 dB with a sidechain envelope. The video stream is copied untouched.
"""
import subprocess, sys, tempfile, os
import numpy as np, soundfile as sf

SR = 48000
rng = np.random.default_rng(7)

def tone(f, n, kind="sine", det=0.0):
    t = np.arange(n) / SR
    if kind == "sine": return np.sin(2 * np.pi * f * t)
    if kind == "tri": return 2 / np.pi * np.arcsin(np.sin(2 * np.pi * f * t))
    if kind == "saw": return sum(np.sin(2 * np.pi * f * k * (1 + det) * t) / k for k in range(1, 8)) * 0.6
    if kind == "bell": return np.sin(2 * np.pi * f * t) * np.exp(-t * 5) + 0.3 * np.sin(2 * np.pi * f * 2.76 * t) * np.exp(-t * 9)

def env(n, a=0.01, r=0.1):
    e = np.ones(n); A = max(1, int(a * SR)); R = max(1, int(r * SR))
    e[:A] = np.linspace(0, 1, A); e[-R:] *= np.linspace(1, 0, R); return e

def m2f(m): return 440 * 2 ** ((m - 69) / 12)

def add(buf, at, sig, g):
    i = int(at * SR); j = min(len(buf), i + len(sig))
    if i < len(buf) and j > i: buf[i:j] += sig[: j - i] * g

KEYS = {   # chord roots (midi), scale for arps, tempo, timbres
    "sweet": dict(prog=[60, 65, 67, 60], bpm=140, pad="tri", lead="bell", drums=True, det=0.0),
    "cute": dict(prog=[72, 77, 79, 72], bpm=140, pad="sine", lead="bell", drums=True, det=0.0),
    "possessed": dict(prog=[52, 48, 50, 47], bpm=120, pad="saw", lead="sine", drums=True, det=0.012, minor=True),
    "sad": dict(prog=[53, 58, 60, 53], bpm=80, pad="tri", lead="sine", drums=False, det=0.0),
}

def music(n, mood):
    if mood not in KEYS: return np.zeros(n)
    k = KEYS[mood]; minor = k.get("minor", False); out = np.zeros(n)
    beat = 60 / k["bpm"]; bar = 4 * beat if mood != "sad" else 3 * beat
    third = 3 if minor else 4
    t, i = 0.0, 0
    while t < n / SR:
        r = k["prog"][i % 4]; L = int(bar * SR)
        for iv in (0, third, 7):                       # pad chord
            add(out, t, tone(m2f(r + iv - 12), L, k["pad"], k["det"]) * env(L, 0.3, 0.4), 0.035)
        for b in range(int(bar / beat)):              # bass line: root, fifth walk
            m = r - 24 + (0, 7, 12, 7)[b % 4]
            add(out, t + b * beat, tone(m2f(m), int(beat * 0.9 * SR), "tri") * env(int(beat * 0.9 * SR), 0.005, 0.1), 0.07)
        for s in range(int(bar / beat) * 2):          # arpeggio in eighths
            m = r + (0, third, 7, 12)[s % 4] + 12
            L2 = int(beat * 0.45 * SR); add(out, t + s * beat / 2, tone(m2f(m), L2, k["lead"]) * env(L2, 0.003, 0.15), 0.025)
        if i % 2 == 1:                                 # countermelody fragment, every other bar
            for s, iv in enumerate((7, 5, third, 2)):
                L2 = int(beat * 0.9 * SR); add(out, t + s * beat, tone(m2f(r + iv + 24), L2, "sine") * env(L2, 0.02, 0.3), 0.02)
        if k["drums"]:
            for b in range(int(bar / beat)):
                tt = t + b * beat
                if b % 2 == 0: add(out, tt, np.sin(2 * np.pi * 55 * np.arange(int(0.15 * SR)) / SR * np.exp(-np.arange(int(0.15 * SR)) / SR * 12)) * env(int(0.15 * SR), 0.001, 0.1), 0.12)
                hh = rng.standard_normal(int(0.04 * SR)) * env(int(0.04 * SR), 0.001, 0.03)
                add(out, tt + beat / 2, np.diff(hh, prepend=0), 0.02)
        t += bar; i += 1
    # fill into the cut: a short snare roll in the last beat
    if k["drums"]:
        for s in range(6):
            sn = rng.standard_normal(int(0.06 * SR)) * env(int(0.06 * SR), 0.001, 0.05)
            add(out, n / SR - beat + s * beat / 6, sn, 0.015 + s * 0.006)
    return out

def riser_stinger(n, mood):
    out = np.zeros(n); L = int(1.2 * SR)
    if n > 2 * L:
        t = np.arange(L) / SR; f = 200 + 1800 * (t / t[-1]) ** 2
        noise = rng.standard_normal(L); sweep = np.sin(2 * np.pi * np.cumsum(f) / SR)
        add(out, n / SR - 1.2, (noise * 0.25 + sweep * 0.5) * np.linspace(0, 1, L) ** 2, 0.03)   # riser into the cut
        root = 60 if mood != "possessed" else 52
        add(out, 0.0, sum(tone(m2f(root + iv), int(0.8 * SR), "bell") for iv in (0, 4 if mood != "possessed" else 3, 7)), 0.03)   # downbeat stinger
    return out

def ambience(n, loc):
    t = np.arange(n) / SR; out = np.zeros(n)
    pink = np.cumsum(rng.standard_normal(n)); pink -= np.convolve(pink, np.ones(4800) / 4800, mode="same"); pink /= (np.abs(pink).max() + 1e-9)
    if loc in ("fair", "plaza"):
        out += pink * 0.04 * (1 + 0.3 * np.sin(2 * np.pi * 0.13 * t))                         # crowd walla bed
        for k in range(int(n / SR / 3)):                                                         # distant ride: a whirr swell
            L = int(1.5 * SR); add(out, k * 3 + rng.random(), tone(180 + rng.random() * 60, L, "saw") * np.hanning(L), 0.006)
    if loc in ("night", "hill", "gate", "bandstand"):
        for k in range(int(n / SR * 2)):                                                         # crickets
            L = int(0.05 * SR); add(out, rng.random() * n / SR, tone(4200 + rng.random() * 400, L) * np.hanning(L), 0.012)
        out += pink * 0.025 * (0.6 + 0.4 * np.sin(2 * np.pi * 0.07 * t))                       # wind
    if loc == "tent":
        out += pink * 0.02
        for k in range(int(n / SR / 2)):
            L = int(0.25 * SR); add(out, k * 2 + rng.random(), rng.standard_normal(L) * np.hanning(L), 0.01)   # canvas flaps
    if loc == "fountain":
        out += np.diff(rng.standard_normal(n + 1)) * 0.012 * (1 + 0.2 * np.sin(2 * np.pi * 1.3 * t))
    if loc == "bakery":
        out += tone(100, n) * 0.008 + tone(150, n) * 0.004 + pink * 0.01                         # warm oven hum
    return out

def sweeteners(x):
    out = np.zeros(len(x)); hop = 480
    n = len(x) // hop; e = np.sqrt((x[: n * hop].reshape(n, hop) ** 2).mean(1))
    d = np.diff(e, prepend=0); thr = np.percentile(d, 99.5)
    last = -99
    for i in np.nonzero(d > max(thr, 0.05))[0]:
        if i - last < 15: continue
        last = i; at = i * hop / SR
        L = int(0.35 * SR); tt = np.arange(L) / SR
        add(out, at, np.sin(2 * np.pi * (60 - 25 * tt / tt[-1]) * tt) * np.exp(-tt * 9), 0.22)        # sub-thump under big hits
        add(out, at + 0.02, sum(tone(m2f(96 + k * 3), int(0.25 * SR), "bell") for k in range(3)), 0.012)   # sparkle tinkle
    return out

def main():
    src, dst, loc, mood = sys.argv[1:5]
    with tempfile.TemporaryDirectory() as td:
        a = os.path.join(td, "a.wav"); subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-vn", "-ac", "2", "-ar", str(SR), a], check=True)
        x, _ = sf.read(a); mono = x.mean(1); n = len(mono)
        mus = music(n, mood) + riser_stinger(n, mood)
        # duck the new music under the scene's own audio (dialogue first)
        hop = 1200; m = n // hop + 1
        e = np.sqrt(np.convolve(np.pad(mono ** 2, (0, m * hop - n)).reshape(m, hop).mean(1), np.ones(5) / 5, mode="same"))
        duck = np.clip(1 - e / (np.percentile(e, 90) + 1e-9), 0.32, 1.0)
        duck = np.repeat(duck, hop)[:n]
        add_l = mus * duck + ambience(n, loc) + sweeteners(mono)
        y = x + add_l[:, None] * np.array([[1.0, 0.92]])
        y /= max(1.0, np.abs(y).max() / 0.95)
        b = os.path.join(td, "b.wav"); sf.write(b, y, SR)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-i", b, "-map", "0:v", "-map", "1:a", "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", dst], check=True)

if __name__ == "__main__":
    main()
