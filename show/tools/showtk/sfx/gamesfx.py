"""The game's own SFX at 48 kHz (SHOW_ENGINE_SPEC.md sec.4.2, 10.5; App. C18).

Two families, both read from the game read-only, never re-authored:

* SynthSeg one-shots (SCRATCH, BOING, POP2, THUD, WHOOSH, KAZOO, BEEP, GLITCH,
  TIK, SHWIP, TINK, FANFARE, CHOIR, PLUCK, CREAK): the tables in
  core/src/audio_fx.c, read by tools/mmlc.py's read_synth_sfx() and stepped
  frame by frame exactly as audio_fx_synth_step() does (C integer arithmetic,
  pitch_hz() from core/src/audio_seq.c). Each 30 Hz frame's (freq, vol, wave)
  per channel is then voiced at 48 kHz: polyBLEP pulses and saw, the 4-bit
  stepped triangle, and the 15-bit LFSR clocked at the channel frequency, the
  same waves the native musrender (core/native/plat_music_native.c) records.
* sfxgen.py's PCM effects (SHOTGUN, CHAINSAW, SPLAT, ...): tools/sfxgen.py is
  imported and its recipes run at 48,000 Hz by setting the module's HI/MID/LO
  rates (no edit). Its one-pole filters and sample-and-hold noise are defined
  per sample, so their coefficients are re-derived for 48 kHz from each
  recipe's native rate: the same cut-offs, the same grit, rendered at 48 kHz.
"""
from __future__ import annotations

import importlib.util
import re
from functools import lru_cache

import numpy as np

from ..music import audiopaths as P

SR = 48000
SPF = 1600
WAVES = ["WAVE_PULSE12", "WAVE_PULSE25", "WAVE_PULSE50", "WAVE_TRIANGLE", "WAVE_SAW", "WAVE_NOISE"]
NZ, ARP, DEC = 8, 16, 32


def _cdiv(a: int, b: int) -> int:
    """C integer division (truncates toward zero)."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


@lru_cache(maxsize=1)
def _mmlc():
    from ..music import mml_legacy
    return mml_legacy.mmlc()


@lru_cache(maxsize=1)
def synth_recipes():
    """[(name, [[t0, n, a, b, wc, vol], ...])] in SFX_SYNTH order (audio_fx.c recipes[])."""
    return _mmlc().read_synth_sfx(str(P.REPO))


@lru_cache(maxsize=1)
def _note_hz64():
    src = (P.REPO / "core" / "src" / "audio_seq.c").read_text(encoding="ascii")
    m = re.search(r"note_hz64\[13\]\s*=\s*\{([^}]*)\}", src)
    return [int(x) for x in m.group(1).replace("\n", " ").split(",") if x.strip()]


def pitch_hz(p16: int) -> int:
    """audio_seq.c pitch_hz(): 1/16 semitones above C0 -> integer Hz."""
    tab = _note_hz64()
    p16 = max(0, min(16 * 12 * 9 - 1, p16))
    semi, frac = p16 >> 4, p16 & 15
    octv = (semi * 5462) >> 16
    s = semi - octv * 12
    a, b = tab[s], tab[s + 1]
    f = a + (((b - a) * frac) >> 4)
    f = (f << octv) >> 10
    return max(20, min(16000, f))


def synth_frames(name: str, semis: int = 0, sfx_vol: int = 10):
    """audio_fx_synth_step() frame by frame -> [ [(freq, vol, wave) for ch2, ch3], ... ]."""
    segs = dict(synth_recipes())[name]
    vol_all = max(1, min(15, _cdiv(sfx_vol * 15 + 5, 10)))
    semis = max(-36, min(36, semis))
    frames = []
    t = 0
    while True:
        f, v, w = [0, 0], [0, 0], [3, 5]
        used, live = 0, False
        for t0, n, a, b, wc, vol in segs:
            k = t - t0
            ch = 1 if wc & NZ else 0
            used |= 1 << ch
            if t < t0 + n:
                live = True
            if k < 0 or k >= n:
                continue
            lvl = vol
            if wc & DEC:
                lvl -= _cdiv((vol * 3 // 4) * k, max(1, n - 1))
            lvl = _cdiv(lvl * vol_all + 7, 15)
            if wc & NZ:
                hz = (a + _cdiv((b - a) * k, n)) * 64
                f[ch] = max(40, min(16000, hz))
            else:
                p16 = a * 16 + _cdiv((b - a) * 16 * k, n) + semis * 16
                if wc & ARP:
                    p16 += (0, 4, 7)[t % 3] * 16
                f[ch] = pitch_hz(p16)
            v[ch] = max(1, min(15, lvl))
            w[ch] = wc & 7
        if not live:
            break
        frames.append([(f[c] if v[c] else 0, v[c], w[c]) if used & (1 << c) else None for c in (0, 1)])
        t += 1
    return frames


def _lfsr(freq: np.ndarray, state: int = 0x7FFF):
    """plat_music_native.c's 15-bit LFSR, clocked at the channel frequency."""
    out = np.empty(len(freq))
    ph = 0.0
    reg = state
    for i, fq in enumerate(freq):
        ph += fq / SR
        while ph >= 1.0:
            ph -= 1.0
            bit = (reg ^ (reg >> 1)) & 1
            reg = (reg >> 1) | (bit << 14)
        out[i] = 1.0 if reg & 1 else -1.0
    return out


def voice_frames(frames, declick_ms: float = 0.7) -> np.ndarray:
    """Per-frame PSG states -> mono float at 48 kHz (both channels summed)."""
    from ..music.kitenv import kit
    from kit import dsp
    n = len(frames) * SPF
    out = np.zeros(n)
    for c in (0, 1):
        fr = np.zeros(n)
        vol = np.zeros(n)
        wav = np.zeros(n, dtype=int)
        for i, st in enumerate(frames):
            if st[c] is None:
                continue
            f, v, w = st[c]
            sl = slice(i * SPF, (i + 1) * SPF)
            fr[sl], vol[sl], wav[sl] = f, v / 15.0, w
        if not vol.any():
            continue
        inc = np.maximum(fr, 1.0) / SR
        y = np.zeros(n)
        for w in np.unique(wav[vol > 0]):
            m = wav == w
            if w in (0, 1, 2):
                duty = {0: 0.125, 1: 0.25, 2: 0.5}[int(w)]
                z = dsp.pulse(inc, np.full(n, duty), 0.0)
            elif w == 3:
                z = dsp.tri(inc, 0.0, 16)
            elif w == 4:
                z = dsp.saw(inc, 0.0)
            else:
                z = _lfsr(fr)
            y[m] = z[m]
        g = dsp.onepole_lp(vol, np.full(n, 1000.0 / (2 * np.pi * declick_ms)), float(SR))
        out += y * g
    return dc_block(out)


def dc_block(x: np.ndarray) -> np.ndarray:
    """The DAC's DC block of plat_music_native.c (y = x - x1 + y1*255/256 at 22,050 Hz,
    about 14 Hz), at the same cut-off for 48 kHz."""
    from scipy.signal import lfilter
    a = (255.0 / 256.0) ** (22050.0 / SR)
    return lfilter([1.0, -1.0], [1.0, -a], x)


def synth(name: str, semis: int = 0) -> np.ndarray:
    """A SynthSeg one-shot at 48 kHz (mono float, peak ~1)."""
    y = voice_frames(synth_frames(name, semis))
    pk = np.abs(y).max()
    return y / pk if pk > 0 else y


# ---- sfxgen.py at 48 kHz -------------------------------------------------------------
def _load_sfxgen():
    path = P.REPO / "tools" / "sfxgen.py"
    spec = importlib.util.spec_from_file_location("doomo_sfxgen_48k", str(path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


@lru_cache(maxsize=1)
def sfxgen_names():
    return _load_sfxgen().read_sfx_names()[1:]          # without NONE


def _k48(k: int, r0: int) -> int:
    """A one-pole coefficient (1..256 per sample at r0) with the same cut-off at 48 kHz."""
    if k is None or k >= 256:
        return k
    a = 1.0 - k / 256.0
    return max(1, int(round(256.0 * (1.0 - a ** (r0 / SR)))))


def sfxgen(name: str):
    """(48 kHz mono float in -1..1 at its peak, native rate, native int8 PCM)."""
    g = _load_sfxgen()
    recipe = g.RECIPES[name]
    r0, nat = recipe()                           # the recipe at its own rate (HI/MID/LO)
    if name in g.CAP_MS and len(nat) > r0 * g.CAP_MS[name] // 1000:       # sfxgen.build()
        nat = g.fade_out(nat[: r0 * g.CAP_MS[name] // 1000], 60, r0)
    native_pcm = g.to_pcm8(g.fade_out(nat, 6, r0))
    orig_noise, orig_lowpass = g.noise, g.lowpass

    def noise(rate, ms, seed, vol=100, hold_hz=None, hold_hz1=None, lp0=256, lp1=None, attack=1,
              decay=None, start_ms=0, total_ms=None, hp=0):
        if hold_hz is None:
            hold_hz = r0                        # new random value per native sample = S&H at r0
        return orig_noise(rate, ms, seed, vol, hold_hz, hold_hz1, _k48(lp0, r0),
                          _k48(lp1, r0) if lp1 is not None else None, attack, decay, start_ms,
                          total_ms, _k48(hp, r0) if hp else 0)

    def lowpass(t, k):
        return orig_lowpass(t, _k48(k, r0))

    g.noise, g.lowpass = noise, lowpass
    g.HI = g.MID = g.LO = SR
    rate, data = recipe()
    assert rate == SR
    if name in g.CAP_MS and len(data) > rate * g.CAP_MS[name] // 1000:
        data = g.fade_out(data[: rate * g.CAP_MS[name] // 1000], 60, rate)
    data = g.fade_out(data, 6, rate)
    y = np.asarray(data, dtype=float)
    pk = np.abs(y).max()
    return (y / pk if pk > 0 else y), r0, np.asarray(native_pcm, dtype=np.int8)


# ---- the engine's own render (the raw piezo; the AT10.3 reference) --------------------
def build_ref():
    """Compile sfxref.c with the game's unchanged audio_fx.c, plat_music_native.c and
    sfx_data.c (and pitch_hz() copied verbatim from audio_seq.c) into the work dir."""
    import os
    import subprocess
    build = P.WORK / "sfxref"
    build.mkdir(parents=True, exist_ok=True)
    exe = build / "sfxref"
    srcs = [P.SHOWTK / "sfx" / "sfxref.c", P.REPO / "core" / "src" / "audio_fx.c",
            P.REPO / "core" / "native" / "plat_music_native.c", P.REPO / "core" / "gen" / "sfx_data.c"]
    seq = (P.REPO / "core" / "src" / "audio_seq.c").read_text(encoding="ascii")
    tab = re.search(r"static const uint16_t note_hz64\[13\] = \{.*?\};", seq, re.S).group(0)
    fn = re.search(r"static uint16_t pitch_hz\(int32_t p16\)\n\{.*?\n\}", seq, re.S).group(0)
    inc = "/* copied verbatim from core/src/audio_seq.c */\n" + tab + "\n" + fn + "\n"
    incp = build / "pitch_hz.inc"
    if not incp.exists() or incp.read_text(encoding="ascii") != inc:
        incp.write_text(inc, encoding="ascii")
    newest = max(os.path.getmtime(s) for s in srcs + [incp])
    if not exe.exists() or os.path.getmtime(exe) < newest:
        cmd = ["gcc", "-std=c99", "-O1", "-w", "-I", str(build), "-I", str(P.REPO / "core" / "include"),
               "-I", str(P.REPO / "core" / "src"), "-I", str(P.REPO / "core" / "native")] + \
              [str(s) for s in srcs] + ["-o", str(exe)]
        subprocess.run(cmd, check=True)
    return exe


def engine_render(kind: str, ident, semis: int = 0):
    """The engine's own 22,050 Hz render of a SynthSeg one-shot (kind='synth', ident =
    recipe name) or a sfxgen PCM (kind='pcm', ident = SFX_* name)."""
    import subprocess
    import soundfile as sf
    exe = build_ref()
    if kind == "synth":
        idx = [n for n, _ in synth_recipes()].index(ident)
        out = P.WORK / "sfxref" / f"synth_{ident}_{semis}.wav"
        subprocess.run([str(exe), "synth", str(idx), str(out), str(semis)], check=True)
    else:
        idx = sfxgen_names().index(ident) + 1
        out = P.WORK / "sfxref" / f"pcm_{ident}.wav"
        subprocess.run([str(exe), "pcm", str(idx), str(out)], check=True)
    y, sr = sf.read(str(out))
    return (y.mean(axis=1) if y.ndim == 2 else y), sr


def piezo(name: str = "beep", semis: int = 0) -> np.ndarray:
    """The pet's raw piezo, exactly as the engine renders it (`runner --wav`, 22,050 Hz),
    carried to 48 kHz by sample-and-hold only (no filter, no interpolation): the one
    sound kept unprocessed (`process=none`, sec.10.2). Trailing silence trimmed."""
    y, sr = engine_render("synth", name, semis)
    nz = np.nonzero(np.abs(y) > 1e-6)[0]
    y = y[: nz[-1] + 1] if len(nz) else y
    idx = (np.arange(int(len(y) * SR / sr)) * sr) // SR
    return y[idx]
