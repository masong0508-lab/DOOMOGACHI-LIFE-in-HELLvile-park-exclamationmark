"""QA tools: loudness / true peak / LRA (pyloudnorm + ffmpeg ebur128), clipping,
crest factor, per-band stereo correlation, spectral balance vs a modern-pop
reference curve, inter-track masking, tuning verifier (symbolic + audio, A=440),
bass-vs-harmony clash detector, spectrogram PNG, loop-seam checker.

    rep = qa.report(master, sr, stems=stems, score=score, png="out/spec.png")
    print(qa.format_report(rep))
"""
from __future__ import annotations

import re
import subprocess

import numpy as np
import pyloudnorm as pyln
from scipy import signal

from .core import SR, a2db, mono, mtof, note_name, stereo
from .theory import scale_pcs

THIRD_OCT = np.array([25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
                      1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500,
                      16000, 20000])

# Long-term 1/3-octave band levels of modern (2015-2025) pop / J-pop / anime
# openings, relative to the 1 kHz band (dB). Built from averaged LTAS of
# commercial masters (Pestana et al. 2013 trend; mainstream-pop tilt
# ~ -4.5 dB/oct above 2 kHz; sub/kick region +6..+9 dB). Tolerance +-4 dB.
POP_REF = {25: -2.0, 31.5: 2.0, 40: 5.5, 50: 7.5, 63: 8.5, 80: 8.5, 100: 7.5, 125: 6.5,
           160: 5.5, 200: 4.5, 250: 3.5, 315: 2.8, 400: 2.0, 500: 1.4, 630: 0.8, 800: 0.4,
           1000: 0.0, 1250: -0.6, 1600: -1.3, 2000: -2.1, 2500: -3.0, 3150: -3.9, 4000: -4.9,
           5000: -6.0, 6300: -7.3, 8000: -8.8, 10000: -10.6, 12500: -12.8, 16000: -16.5,
           20000: -24.0}
# J-pop / anisong band-mix variant (approximation, documented in KIT.md): live
# bass + kick rather than 808 sub, so less 25-45 Hz; a touch more presence/air.
JPOP_REF = dict(POP_REF)
JPOP_REF.update({25: -12.0, 31.5: -6.0, 40: 0.0, 50: 5.0, 63: 8.0})
for _f in (2500, 3150, 4000, 5000):
    JPOP_REF[_f] = POP_REF[_f] + 1.0
for _f in (6300, 8000, 10000, 12500, 16000):
    JPOP_REF[_f] = POP_REF[_f] + 1.5
REFS = {"pop": POP_REF, "jpop": JPOP_REF}


# ---------------------------------------------------------------------------
# loudness and level
# ---------------------------------------------------------------------------
def ebur128_ffmpeg(path):
    """Run ffmpeg ebur128 (true peak) -> dict(I, LRA, TP)."""
    p = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-filter_complex",
                        "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True)
    txt = p.stderr[p.stderr.rfind("Summary:"):]

    def grab(key):
        m = re.search(key + r":\s+(-?[\d.]+|-inf)", txt)
        return float(m.group(1)) if m and m.group(1) != "-inf" else float("-inf")

    return dict(I=grab("I"), LRA=grab("LRA"), TP=grab("Peak"))


def loudness_stats(x, sr=SR):
    x = stereo(x)
    meter = pyln.Meter(sr)
    I = float(meter.integrated_loudness(x))
    # short-term (3 s windows, 0.5 s hop) maximum
    st = []
    n3 = int(3 * sr)
    hop = int(0.5 * sr)
    for s in range(0, max(1, len(x) - n3), hop):
        st.append(_lufs_block(x[s:s + n3], sr))
    up = signal.resample_poly(x, 4, 1, axis=0)
    tp = float(a2db(np.abs(up).max()))
    pk = float(a2db(np.abs(x).max()))
    rms = float(a2db(np.sqrt(np.mean(mono(x) ** 2))))
    clip = int(np.sum(np.abs(x) >= 0.9999))
    # clip runs: >= 3 consecutive full-scale samples
    runs = 0
    a = np.abs(x).max(axis=1) >= 0.999
    if a.any():
        d = np.diff(np.concatenate([[0], a.astype(int), [0]]))
        starts, ends = np.flatnonzero(d == 1), np.flatnonzero(d == -1)
        runs = int(np.sum((ends - starts) >= 3))
    return dict(lufs_i=I, lufs_st_max=float(np.max(st)) if st else I, true_peak_dbtp=tp,
                sample_peak_dbfs=pk, rms_dbfs=rms, crest_db=pk - rms, plr_db=tp - I,
                clipped_samples=clip, clip_runs=runs)


def _lufs_block(seg, sr):
    meter = pyln.Meter(sr, block_size=0.4)
    try:
        return float(meter.integrated_loudness(seg))
    except Exception:
        return -70.0


# ---------------------------------------------------------------------------
# stereo
# ---------------------------------------------------------------------------
BANDS = [(20, 150), (150, 500), (500, 2000), (2000, 8000), (8000, 20000)]


def band_correlation(x, sr=SR):
    x = stereo(x)
    out = {}
    for lo, hi in BANDS:
        sos = signal.butter(4, (lo, min(hi, sr / 2 - 100)), "band", fs=sr, output="sos")
        y = signal.sosfilt(sos, x, axis=0)
        l, r = y[:, 0], y[:, 1]
        den = np.sqrt(np.sum(l * l) * np.sum(r * r)) + 1e-20
        S = np.sum((l - r) ** 2)
        M = np.sum((l + r) ** 2) + 1e-20
        out[f"{lo}-{hi}"] = dict(corr=float(np.sum(l * r) / den), side_to_mid_db=float(10 * np.log10(S / M + 1e-12)))
    l, r = x[:, 0], x[:, 1]
    out["full"] = dict(corr=float(np.sum(l * r) / (np.sqrt(np.sum(l * l) * np.sum(r * r)) + 1e-20)))
    return out


# ---------------------------------------------------------------------------
# spectrum
# ---------------------------------------------------------------------------
def third_octave(x, sr=SR):
    """Long-term average 1/3-octave band levels (dB, relative to 1 kHz band)."""
    m = mono(x)
    f, P = signal.welch(m, sr, nperseg=16384, noverlap=8192)
    lv = []
    for fc in THIRD_OCT:
        lo, hi = fc / 2 ** (1 / 6), fc * 2 ** (1 / 6)
        sel = (f >= lo) & (f < hi)
        lv.append(10 * np.log10(np.sum(P[sel]) + 1e-20))
    lv = np.array(lv)
    ref = lv[list(THIRD_OCT).index(1000)]
    return lv - ref


def spectral_balance(x, sr=SR, tol=4.0, ref="pop"):
    REF = REFS[ref] if isinstance(ref, str) else ref
    lv = third_octave(x, sr)
    rows = []
    worst = 0.0
    for fc, v in zip(THIRD_OCT, lv):
        r = REF[float(fc) if fc == 31.5 else int(fc)]
        d = float(v - r)
        rows.append((float(fc), float(v), r, d))
        if 40 <= fc <= 16000:
            worst = max(worst, abs(d))
    # broad regions
    def region(lo, hi):
        ds = [d for fc, v, r, d in rows if lo <= fc <= hi]
        return float(np.mean(ds))
    regions = dict(sub_40_63=region(40, 63), low_80_200=region(80, 200),
                   lowmid_250_630=region(250, 630), mid_800_2k=region(800, 2000),
                   presence_2k5_5k=region(2500, 5000), air_6k3_16k=region(6300, 16000))
    ok = all(abs(v) <= tol for v in regions.values())
    return dict(bands=rows, regions=regions, worst_band_dev=worst, within_tol=ok)


# ---------------------------------------------------------------------------
# masking between stems
# ---------------------------------------------------------------------------
ERB_EDGES = np.array([40, 80, 120, 180, 250, 350, 500, 700, 1000, 1400, 2000, 2800, 4000,
                      5600, 8000, 11000, 16000])


def _band_energy(x, sr, hop=0.05):
    m = mono(x)
    nper = 4096
    f, t, Z = signal.stft(m, sr, nperseg=nper, noverlap=nper - int(hop * sr))
    P = np.abs(Z) ** 2
    E = []
    for lo, hi in zip(ERB_EDGES[:-1], ERB_EDGES[1:]):
        sel = (f >= lo) & (f < hi)
        E.append(P[sel].sum(axis=0))
    return 10 * np.log10(np.array(E) + 1e-20)  # (bands, frames)


def masking_report(stems: dict, sr=SR, top=10, active_db=-55.0, close_db=6.0):
    """Pairs of stems competing in the same band at similar level at the same time.

    score = fraction of frames where both stems are active in a band and within
    `close_db` of each other, weighted by how loud the band is in the mix.
    """
    names = [k for k in stems if not k.startswith("ret:")]
    E = {k: _band_energy(stems[k], sr) for k in names}
    mixE = 10 * np.log10(sum(10 ** (E[k] / 10) for k in names) + 1e-20)
    gmax = mixE.max()
    out = []
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = E[names[i]], E[names[j]]
            T = min(a.shape[1], b.shape[1])
            a, b, mx = a[:, :T], b[:, :T], mixE[:, :T]
            act = (a > gmax + active_db) & (b > gmax + active_db) & (mx > gmax - 40)
            close = np.abs(a - b) < close_db
            hit = act & close
            for bi in range(a.shape[0]):
                frac = hit[bi].mean()
                if frac > 0.12:
                    loud = float(np.mean(mx[bi][hit[bi]]) - gmax) if hit[bi].any() else -99
                    out.append(dict(a=names[i], b=names[j],
                                    band=f"{ERB_EDGES[bi]}-{ERB_EDGES[bi + 1]} Hz",
                                    overlap=float(frac), mix_level_db=loud,
                                    score=float(frac * 10 ** (loud / 20))))
    out.sort(key=lambda d: -d["score"])
    return out[:top]


# ---------------------------------------------------------------------------
# tuning
# ---------------------------------------------------------------------------
def tuning_symbolic(score, extra_ok=None):
    """Every pitched note must be in the score's key/mode (unless chromatic_ok)."""
    pcs = set(scale_pcs(score.key, score.mode))
    bad = []
    n_checked = 0
    for name, tr in score.tracks.items():
        if tr.drum or tr.role in ("fx",):
            continue
        for n in tr.notes:
            n_checked += 1
            if n.p % 12 not in pcs and not tr.chromatic_ok and not n.meta.get("chromatic"):
                bad.append(dict(track=name, beat=round(n.t, 3), note=note_name(n.p)))
    return dict(checked=n_checked, out_of_scale=bad, key=f"{score.key} {score.mode}")


def _f0_yin(x, sr, fmin=40.0, fmax=2000.0):
    """YIN f0 of a mono segment (single estimate, with parabolic refinement)."""
    x = x - x.mean()
    W = len(x) // 2
    tau_min, tau_max = int(sr / fmax), min(int(sr / fmin), W - 2)
    if tau_max <= tau_min + 2:
        return None
    d = np.zeros(tau_max + 1)
    cs = np.cumsum(x ** 2)
    e0 = cs[W - 1]
    for tau in range(1, tau_max + 1):
        d[tau] = e0 + (cs[W - 1 + tau] - cs[tau - 1]) - 2 * np.dot(x[:W], x[tau:tau + W])
    dn = np.ones_like(d)
    dn[1:] = d[1:] * np.arange(1, tau_max + 1) / (np.cumsum(d[1:]) + 1e-20)
    cand = np.flatnonzero(dn[tau_min:] < 0.15)
    if len(cand) == 0:
        tau = tau_min + int(np.argmin(dn[tau_min:]))
    else:
        tau = tau_min + cand[0]
        while tau + 1 <= tau_max and dn[tau + 1] < dn[tau]:
            tau += 1
    if 1 <= tau < tau_max:
        a, b, c = dn[tau - 1], dn[tau], dn[tau + 1]
        tau = tau + 0.5 * (a - c) / (a - 2 * b + c + 1e-20)
    return sr / tau


def _f0_peak(x, sr, f_expect, search_semis=0.8):
    """Frequency of the strongest spectral peak within +-search_semis of f_expect
    (zero-padded FFT + parabolic interpolation on log magnitude)."""
    x = x - x.mean()
    N = 1 << int(np.ceil(np.log2(len(x) * 8)))
    S = np.abs(np.fft.rfft(x * np.hanning(len(x)), N))
    fr = np.fft.rfftfreq(N, 1 / sr)
    lo, hi = f_expect * 2 ** (-search_semis / 12), f_expect * 2 ** (search_semis / 12)
    sel = np.flatnonzero((fr >= lo) & (fr <= hi))
    if len(sel) < 3:
        return None
    k = sel[np.argmax(S[sel])]
    a, b, c = np.log(S[k - 1] + 1e-20), np.log(S[k] + 1e-20), np.log(S[k + 1] + 1e-20)
    d = 0.5 * (a - c) / (a - 2 * b + c + 1e-20)
    if S[k] < 1e-3 * S.max():
        return None
    return (k + d) * sr / N


def tuning_audio(inst, score_cls=None, pitches=(45, 52, 57, 60, 64, 69, 72, 76), dur=1.6, sr=SR,
                 skip=0.1, frame=0.1):
    """Render isolated notes through an instrument; measure the pitch vs 12-TET
    at A=440. Per note: median of frame-wise peak-frequency estimates over the
    held part (robust to vibrato / ensemble chorus). Returns cents per note.
    """
    from .score import Score
    rows = []
    for p in pitches:
        s = Score(bpm=60, bars=1, tail_beats=1)
        t = s.track("t", inst=inst)
        t.add(0, dur, p, 0.8)
        y = mono(inst.render(t, s, sr))
        cs, ws = [], []
        nf = int(max(frame, 20.0 / float(mtof(p))) * sr)  # >= 20 periods (low notes)
        for a in range(int(skip * sr), int(dur * sr) - nf, nf // 2):
            seg = y[a:a + nf]
            e = float(np.sqrt(np.mean(seg ** 2)))
            if e < 1e-5:
                continue
            f0 = _f0_peak(seg, sr, float(mtof(p)))
            if f0:
                cs.append(1200 * np.log2(f0 / mtof(p)))
                ws.append(e)
        if not cs:
            continue
        o = np.argsort(cs)
        cw = np.cumsum(np.array(ws)[o])
        c = float(np.array(cs)[o][np.searchsorted(cw, cw[-1] / 2)])  # energy-weighted median
        rows.append(dict(note=note_name(p), target_hz=float(mtof(p)),
                         f0_hz=float(mtof(p) * 2 ** (c / 1200)), cents=c,
                         spread_cents=float(np.percentile(cs, 90) - np.percentile(cs, 10))))
    errs = [abs(r["cents"]) for r in rows]
    return dict(notes=rows, max_abs_cents=float(max(errs)) if errs else None,
                median_abs_cents=float(np.median(errs)) if errs else None)


def tuning_preset(key, pitches=None, dry=True):
    """Audio tuning check of a kit preset. dry=True disables unison detune and
    vibrato (verifies the oscillator / sample tuning itself)."""
    from . import render
    cls, base = render.PRESETS[key]
    over = {}
    if dry:
        for k in ("detune", "vib_depth"):
            if k in cls.defaults or k in base:
                over[k] = 0.0
    inst = render.make(key, **over)
    if pitches is None:
        lo = 36 if ("bass" in key or key in ("reese", "contrabass", "celli")) else 60
        lo = 79 if key == "glock_real" else lo
        pitches = (lo, lo + 4, lo + 7, lo + 12)
    return tuning_audio(inst, None, pitches=pitches)


def global_tuning(x, sr=SR, lo=80, hi=1200):
    """Global tuning offset of a mix (cents vs A=440) from spectral peak positions."""
    m = mono(x)
    N = 32768
    hop = 8192
    win = np.hanning(N)
    fr = np.fft.rfftfreq(N, 1 / sr)
    sel = np.flatnonzero((fr >= lo) & (fr <= hi))
    cents = []
    wts = []
    for i in range(0, len(m) - N, hop):
        s = np.abs(np.fft.rfft(m[i:i + N] * win))
        seg = s[sel]
        pk = signal.find_peaks(seg, height=seg.max() * 0.2, distance=6)[0]
        for k in pk[:12]:
            kk = sel[k]
            a, b, c = np.log(s[kk - 1] + 1e-12), np.log(s[kk] + 1e-12), np.log(s[kk + 1] + 1e-12)
            d = 0.5 * (a - c) / (a - 2 * b + c + 1e-12)
            f = (kk + d) * sr / N
            mm = 69 + 12 * np.log2(f / 440)
            cents.append((mm - round(mm)) * 100)
            wts.append(s[kk])
    if not cents:
        return None
    cents = np.array(cents)
    w = np.array(wts)
    # circular mean on the semitone circle
    ang = cents / 100 * 2 * np.pi
    c = np.angle(np.sum(w * np.exp(1j * ang))) / (2 * np.pi) * 100
    return float(c)


# ---------------------------------------------------------------------------
# bass vs harmony clashes
# ---------------------------------------------------------------------------
def clash_report(score, bass_roles=("bass",), harmony_roles=("harmony", "lead", "music"),
                 strong="beats"):
    """Flag bass notes a semitone (b9 / maj7) or tritone against sounding chord
    tones on strong beats (every beat, or 'downbeats' = 1 and 3)."""
    bass = [n for t in score.tracks.values() if t.role in bass_roles and not t.drum for n in t.notes]
    harm = [(t.name, n) for t in score.tracks.values() if t.role in harmony_roles and not t.drum
            for n in t.notes]
    step = 1.0 if strong == "beats" else 2.0
    issues = []
    beat = 0.0
    end = score.length_beats
    while beat < end:
        sb = [n for n in bass if n.t <= beat + 0.02 and n.end > beat + 0.05]
        if sb:
            b = min(sb, key=lambda n: n.p)
            for name, h in harm:
                if not (h.t <= beat + 0.02 and h.end > beat + 0.05):
                    continue
                ic = (h.p - b.p) % 12
                if ic in (1, 11, 6):
                    kind = {1: "b9 (semitone above bass)", 11: "maj7 (semitone below bass)",
                            6: "tritone"}[ic]
                    sev = "high" if ic == 1 else ("low" if ic == 11 else "medium")
                    chord = next((c[2] for c in score.chords if c[0] <= beat < c[0] + c[1]), "")
                    if ic == 11 and "maj7" in chord or ic == 11 and "M7" in chord:
                        sev = "ok"
                    if ic == 6 and ("m7b5" in chord or "dim" in chord):
                        sev = "ok"
                    if sev != "ok":
                        issues.append(dict(beat=beat, bar=int(beat // score.beats_per_bar) + 1,
                                           bass=note_name(b.p), track=name, note=note_name(h.p),
                                           interval=kind, severity=sev, chord=chord))
        beat += step
    # de-duplicate
    seen, out = set(), []
    for i in issues:
        k = (i["beat"], i["bass"], i["note"])
        if k not in seen:
            seen.add(k)
            out.append(i)
    return dict(checked_beats=int(end / step), issues=out,
                high=sum(1 for i in out if i["severity"] == "high"),
                medium=sum(1 for i in out if i["severity"] == "medium"),
                low=sum(1 for i in out if i["severity"] == "low"))


# ---------------------------------------------------------------------------
# clicks / discontinuities
# ---------------------------------------------------------------------------
def click_detect(x, sr=SR, ratio=10.0, fc=7000.0, min_level_db=-45.0, isolation=0.12):
    """Find isolated broadband spikes (render bugs: truncated voices, buffer seams,
    discontinuities) while ignoring musical attacks.

    A click = a high-passed peak > `ratio` x the local 30 ms HF RMS, whose HF
    energy does NOT continue: the energy 1-6 ms after the spike is below
    `isolation` x the energy within +-0.4 ms of it (a pluck/hit keeps ringing).
    Returns (count, times_s[:20]).
    """
    m = mono(x)
    sos = signal.butter(4, fc, "high", fs=sr, output="sos")
    h = signal.sosfilt(sos, m)
    w2 = int(0.03 * sr)
    bg = np.sqrt(np.convolve(h * h, np.ones(w2) / w2, mode="same") + 1e-20)
    floor = 10 ** (min_level_db / 20) * (np.abs(m).max() + 1e-12)
    cand = np.flatnonzero((np.abs(h) > ratio * bg) & (np.abs(h) > floor))
    times = []
    last = -10 ** 9
    a0, a1 = int(0.0004 * sr), int(0.001 * sr)
    b1 = int(0.006 * sr)
    near_lo, near_hi = int(0.002 * sr), int(0.04 * sr)
    for k, i in enumerate(cand):
        if i - last <= int(0.01 * sr):
            last = i
            continue
        last = i
        spike = np.mean(h[max(0, i - a0):i + a0 + 1] ** 2)
        post = np.mean(h[i + a1:i + b1] ** 2) if i + b1 < len(h) else 0.0
        if post >= isolation * spike:
            continue
        # periodic edges (saw / pulse resets of a low note) recur within 40 ms: not clicks
        j0 = np.searchsorted(cand, i - near_hi)
        j1 = np.searchsorted(cand, i + near_hi)
        nb = cand[j0:j1]
        if np.any((np.abs(nb - i) > near_lo)):
            continue
        times.append(i / sr)
    return len(times), [round(float(t), 3) for t in times[:20]]


# ---------------------------------------------------------------------------
# loop seam
# ---------------------------------------------------------------------------
def loop_seam(x, sr=SR, win_ms=40.0, reference=None):
    """Check a loop's wrap point (end -> start).

    * jump: |x[0] - x[-1]| compared with the loop's own 99.9th-percentile
      sample step (a seam must not be a bigger step than the music makes);
    * reference=(continuous_audio, seam_index): the same musical transition in
      a continuous render (e.g. pass 1 -> pass 2 of a render-twice loop). The
      wrapped audio around the seam must match it: err_db <= -20 dB means the
      wrap carries the right tails and dynamics (a dry start or a cut tail
      shows up as a large error);
    * level step and spectral flux at the seam vs. the loop's typical flux are
      reported for information (a downbeat seam is legitimately a transient).
    """
    x = stereo(x)
    w = int(win_ms * 1e-3 * sr)
    steps = np.abs(np.diff(x, axis=0)).max(axis=1)
    p999 = float(np.percentile(steps, 99.9))
    jump = float(np.max(np.abs(x[0] - x[-1])))
    end_rms = float(a2db(np.sqrt(np.mean(x[-w:] ** 2))))
    start_rms = float(a2db(np.sqrt(np.mean(x[:w] ** 2))))

    def spec(seg):
        S = np.abs(np.fft.rfft(mono(seg) * np.hanning(len(seg))))
        return np.log(S + 1e-9)

    flux_seam = float(np.mean(np.maximum(spec(x[:w]) - spec(x[-w:]), 0)))
    fl = []
    for k in range(1, 40):
        i = int(len(x) * k / 40)
        fl.append(np.mean(np.maximum(spec(x[i:i + w]) - spec(x[i - w:i]), 0)))
    flux_med = float(np.median(fl))
    out = dict(sample_jump=jump, typical_step_p999=p999, end_rms_db=end_rms, start_rms_db=start_rms,
               seam_flux=flux_seam, median_flux=flux_med)
    ok = jump <= max(p999, 1e-4) * 1.5
    if reference is not None:
        # compare band-energy envelopes (not waveforms: per-note randomness such
        # as round robins / oscillator phases legitimately differs between passes)
        ref, k = reference
        ref = stereo(ref)
        W = int(0.3 * sr)
        wrap = mono(np.concatenate([x[-W:], x[:W]]))
        cont = mono(ref[k - W:k + W])

        def bands(sig):
            f, t, Z = signal.stft(sig, sr, nperseg=1024, noverlap=1024 - 240)
            P = np.abs(Z) ** 2
            edges = np.geomspace(40, 16000, 25)
            E = np.array([P[(f >= lo) & (f < hi)].sum(axis=0) for lo, hi in zip(edges[:-1], edges[1:])])
            return 10 * np.log10(E + 1e-12)

        A, B = bands(wrap), bands(cont)
        top = max(A.max(), B.max())
        A, B = np.maximum(A, top - 70), np.maximum(B, top - 70)
        d = np.abs(A - B)
        out["wrap_vs_continuous_db"] = float(np.mean(d))
        out["wrap_vs_continuous_p95_db"] = float(np.percentile(d, 95))
        # the 'tail' check: energy in the first 150 ms after the seam vs continuous
        n0 = A.shape[1] // 2
        n1 = n0 + int(0.15 * sr / 240)
        out["start_tail_deficit_db"] = float(np.mean(B[:, n0:n1] - A[:, n0:n1]))
        ok = ok and out["wrap_vs_continuous_db"] <= 1.0 and abs(out["start_tail_deficit_db"]) <= 1.0
    out["seamless"] = bool(ok)
    return out


def make_loopable(x, loop_len, sr=SR):
    """Fold everything after `loop_len` samples (reverb/delay tails) onto the start."""
    x = stereo(x)
    y = x[:loop_len].copy()
    tail = x[loop_len:]
    k = 0
    while len(tail):
        seg = tail[:loop_len]
        y[:len(seg)] += seg
        tail = tail[loop_len:]
        k += 1
    return y


# ---------------------------------------------------------------------------
# spectrogram
# ---------------------------------------------------------------------------
def spectrogram_png(x, path, sr=SR, title="", score=None, fmax=20000, ref="pop"):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    m = mono(x)
    f, t, Z = signal.stft(m, sr, nperseg=4096, noverlap=4096 - 512)
    S = 20 * np.log10(np.abs(Z) + 1e-9)
    S -= S.max()
    fig = plt.figure(figsize=(16, 9), dpi=100)
    gs = fig.add_gridspec(3, 2, height_ratios=[3.2, 1, 1.2], width_ratios=[4, 1.3])
    ax = fig.add_subplot(gs[0, :])
    sel = (f > 20) & (f < fmax)
    ax.pcolormesh(t, f[sel], S[sel], vmin=-90, vmax=0, cmap="magma", shading="auto")
    ax.set_yscale("log")
    ax.set_ylim(25, fmax)
    ax.set_ylabel("Hz")
    ax.set_title(title)
    if score is not None:
        for s in score.sections:
            ts = score.bar(s.bar) * score.spb
            ax.axvline(ts, color="cyan", lw=0.6, alpha=0.6)
            ax.text(ts + 0.1, 30, s.name, color="cyan", fontsize=8)
    ax2 = fig.add_subplot(gs[1, :], sharex=ax)
    st = stereo(x)
    tt = np.arange(len(st)) / sr
    dec = max(1, len(st) // 6000)
    ax2.fill_between(tt[::dec], st[::dec, 0].clip(-1, 1), -np.abs(st[::dec, 1]).clip(0, 1), lw=0,
                     color="#4a90d9")
    ax2.set_ylim(-1.05, 1.05)
    ax2.set_ylabel("L / -|R|")
    # short-term loudness
    n3 = int(3 * sr)
    hop = int(0.25 * sr)
    stl = []
    tl = []
    for s in range(0, max(1, len(st) - n3), hop):
        stl.append(_lufs_block(st[s:s + n3], sr))
        tl.append((s + n3 / 2) / sr)
    ax2b = ax2.twinx()
    ax2b.plot(tl, stl, color="orange", lw=1)
    ax2b.set_ylim(-40, -4)
    ax2b.set_ylabel("ST LUFS", color="orange")
    ax3 = fig.add_subplot(gs[2, 0])
    lv = third_octave(x, sr)
    REF = REFS[ref] if isinstance(ref, str) else ref
    refv = [REF[float(fc) if fc == 31.5 else int(fc)] for fc in THIRD_OCT]
    ax3.semilogx(THIRD_OCT, lv, "o-", label="this mix", ms=3)
    ax3.semilogx(THIRD_OCT, refv, "--", label=f"{ref if isinstance(ref, str) else 'custom'} ref", color="gray")
    ax3.fill_between(THIRD_OCT, np.array(refv) - 4, np.array(refv) + 4, color="gray", alpha=0.15)
    ax3.set_ylim(-30, 15)
    ax3.legend(fontsize=7)
    ax3.set_ylabel("dB re 1k (1/3 oct)")
    ax3.grid(alpha=0.3)
    ax4 = fig.add_subplot(gs[2, 1])
    bc = band_correlation(x, sr)
    ks = [k for k in bc if k != "full"]
    ax4.bar(range(len(ks)), [bc[k]["corr"] for k in ks], color="#7bc96f")
    ax4.set_xticks(range(len(ks)))
    ax4.set_xticklabels(ks, fontsize=6, rotation=30)
    ax4.set_ylim(-1, 1)
    ax4.set_title("L/R correlation per band", fontsize=8)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
    return path


# ---------------------------------------------------------------------------
# stem levels
# ---------------------------------------------------------------------------
def stem_levels(stems, sr=SR):
    """Per-stem loudness (LUFS over its active part), peak, and band energy split."""
    meter = pyln.Meter(sr)
    rows = {}
    for k, v in stems.items():
        v = stereo(v)
        e = np.sqrt(np.mean(v ** 2, axis=1))
        act = e > (e.max() * 0.01 + 1e-12)
        idx = np.flatnonzero(act)
        seg = v[idx.min():idx.max() + 1] if len(idx) else v
        try:
            L = float(meter.integrated_loudness(seg)) if len(seg) > sr * 0.5 else float("nan")
        except Exception:
            L = float("nan")
        f, P = signal.welch(mono(v), sr, nperseg=8192)
        def band(lo, hi):
            return float(10 * np.log10(np.sum(P[(f >= lo) & (f < hi)]) + 1e-20))
        rows[k] = dict(lufs_active=L, peak_db=float(a2db(np.abs(v).max())),
                       sub=band(20, 80), low=band(80, 250), mid=band(250, 2000),
                       high=band(2000, 8000), air=band(8000, 20000))
    return rows


def format_levels(rows):
    out = [f"{'stem':<14}{'LUFS':>7}{'peak':>7}{'sub':>7}{'low':>7}{'mid':>7}{'high':>7}{'air':>7}"]
    for k, r in sorted(rows.items(), key=lambda kv: -np.nan_to_num(kv[1]["lufs_active"], nan=-99)):
        out.append(f"{k:<14}{r['lufs_active']:7.1f}{r['peak_db']:7.1f}{r['sub']:7.1f}{r['low']:7.1f}"
                   f"{r['mid']:7.1f}{r['high']:7.1f}{r['air']:7.1f}")
    return "\n".join(out)


# ---------------------------------------------------------------------------
# full report
# ---------------------------------------------------------------------------
def report(x, sr=SR, stems=None, score=None, path_wav=None, png=None, title="", ref="pop"):
    rep = dict(duration_s=len(x) / sr)
    rep["loudness"] = loudness_stats(x, sr)
    if path_wav:
        rep["ffmpeg_ebur128"] = ebur128_ffmpeg(path_wav)
    rep["stereo"] = band_correlation(x, sr)
    sb = spectral_balance(x, sr, ref=ref)
    rep["spectral_balance"] = dict(ref=ref if isinstance(ref, str) else "custom",
                                   regions=sb["regions"], worst_band_dev=sb["worst_band_dev"],
                                   within_tol=sb["within_tol"])
    rep["global_tuning_cents"] = global_tuning(x, sr)
    rep["clicks"] = click_detect(x, sr)
    if stems:
        rep["masking_top"] = masking_report(stems, sr)
    if score is not None:
        rep["tuning_symbolic"] = tuning_symbolic(score)
        rep["clashes"] = clash_report(score)
    if png:
        spectrogram_png(x, png, sr, title or "", score, ref=ref)
        rep["spectrogram"] = png
    return rep


def format_report(rep):
    L = rep["loudness"]
    lines = [f"duration {rep['duration_s']:.2f} s",
             f"LUFS-I {L['lufs_i']:.2f} | ST max {L['lufs_st_max']:.2f} | TP {L['true_peak_dbtp']:.2f} dBTP"
             f" | peak {L['sample_peak_dbfs']:.2f} dBFS | crest {L['crest_db']:.1f} dB | PLR {L['plr_db']:.1f} dB"
             f" | clipped {L['clipped_samples']} (runs {L['clip_runs']})"]
    if "ffmpeg_ebur128" in rep:
        e = rep["ffmpeg_ebur128"]
        lines.append(f"ffmpeg ebur128: I {e['I']:.1f} LUFS, LRA {e['LRA']:.1f} LU, TP {e['TP']:.1f} dBFS")
    st = rep["stereo"]
    lines.append("stereo corr: " + ", ".join(f"{k} {v['corr']:+.2f}" for k, v in st.items()))
    sb = rep["spectral_balance"]
    lines.append(f"spectral vs {sb.get('ref', 'pop')} ref (dB): " + ", ".join(f"{k} {v:+.1f}" for k, v in sb["regions"].items())
                 + f" | worst band {sb['worst_band_dev']:.1f} | within tol {sb['within_tol']}")
    if "clicks" in rep:
        lines.append(f"click detector: {rep['clicks'][0]} suspicious spikes {rep['clicks'][1][:8]}")
    lines.append(f"global tuning offset: {rep['global_tuning_cents']:+.1f} cents vs A440"
                 if rep.get("global_tuning_cents") is not None else "global tuning: n/a")
    if "tuning_symbolic" in rep:
        ts = rep["tuning_symbolic"]
        lines.append(f"tuning (symbolic, {ts['key']}): {ts['checked']} notes, {len(ts['out_of_scale'])} out of scale")
    if "clashes" in rep:
        c = rep["clashes"]
        lines.append(f"bass/harmony clashes: high {c['high']}, medium {c['medium']}, low {c['low']} "
                     f"over {c['checked_beats']} beats")
    if rep.get("masking_top"):
        lines.append("masking (top): " + "; ".join(
            f"{m['a']}~{m['b']} @{m['band']} {m['overlap'] * 100:.0f}%" for m in rep["masking_top"][:5]))
    return "\n".join(lines)
