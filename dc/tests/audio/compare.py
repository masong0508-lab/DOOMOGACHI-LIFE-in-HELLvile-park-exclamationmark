#!/usr/bin/env python3
"""compare.py - the Dreamcast sound against the native reference (DC5;
owner: dreamcast).

    python3 -I dc/tests/audio/compare.py REF.wav DC.wav [options]

REF.wav is `runner --wav` (musrender, 22,050 Hz), DC.wav the Dreamcast mixer
(dcsnd_runner's DCSND_WAV, or dcrun --wav from Flycast; 44,100 Hz). Both are
mixed to mono. Three numbers:

  lag       the offset of DC against REF that lines their energy envelopes
            up best (10 ms steps, +-max-lag); Flycast's WAV starts at power
            on, the reference at tick 0
  envelope  Pearson r of the two RMS envelopes in 50 ms windows over the
            overlap (the DC5 bar: >= 0.9); --clip-pct P first clips each
            envelope at its P-th percentile, for runs whose one-off effects
            come at different times (game logic, not the sound)
  pitch     the share of 200 ms windows (both loud enough) whose strongest
            spectral peak (60-5,000 Hz) is the same note, within half a
            semitone
  level     RMS of DC over RMS of REF

--drop-gaps N (Flycast runs, DC8) first cuts out of DC every run of exact
digital silence of N samples up to 0.1 s that comes after its first sound
(longer silences are the cue's own, as in REF): the stream's gaps, where the AICA ran dry while the harness held the game
at a sync stop (dc_audio_stall: "no records while held"). Whether a hold
starves the ring depends on emulated timing, so any code change can move a
gap in or out; the game's sound itself is unchanged (the content hash is
the same) but one gap mid-run shifts the rest of the envelope against the
single lag (the combat cue: r 0.92 without, 0.86 with a 1,001-sample gap).

The result line starts with "ok" or "FAIL"; the exit status is 1 on a FAIL.
--plot PNG draws both envelopes (matplotlib, when it is there).
"""
import argparse
import sys
import wave

import numpy as np


def load(path):
    with wave.open(path, "rb") as w:
        rate, ch, sw, n = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit(f"{path}: {sw * 8}-bit WAV, want 16-bit")
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return rate, x


def envelope(x, rate, hop_s):
    """RMS per hop; the hop edges are rounded from exact times, so two rates
    (22,050 and 44,100 Hz) give envelopes on the same time grid"""
    n = int(len(x) / (rate * hop_s))
    if n == 0:
        return np.zeros(0)
    edges = np.round(np.arange(n + 1) * rate * hop_s).astype(np.int64)
    sq = np.add.reduceat(x[: edges[-1]] ** 2, edges[:-1])
    return np.sqrt(sq / np.diff(edges))


def best_lag(er, ed, max_lag):
    """lag (in envelope steps) of ed against er: ed[i + lag] ~ er[i]"""
    best, best_r = 0, -2.0
    for lag in range(-max_lag, max_lag + 1):
        a = er[max(0, -lag):]
        b = ed[max(0, lag):]
        n = min(len(a), len(b))
        if n < 50:
            continue
        a, b = a[:n], b[:n]
        if a.std() == 0 or b.std() == 0:
            continue
        r = float(np.corrcoef(a, b)[0, 1])
        if r > best_r:
            best, best_r = lag, r
    return best, best_r


def drop_gaps(x, min_len, max_len):
    """x without its runs of exact zeros of min_len..max_len samples that
    come after its first nonzero sample; also how many samples went"""
    nz = np.flatnonzero(x)
    if len(nz) == 0:
        return x, 0
    z = np.concatenate([[0], (x[nz[0]:] == 0).astype(np.int8), [0]])
    d = np.diff(z)
    starts, ends = np.flatnonzero(d == 1), np.flatnonzero(d == -1)
    keep = np.ones(len(x), dtype=bool)
    dropped = 0
    for s0, e0 in zip(starts, ends):
        if min_len <= e0 - s0 <= max_len:
            keep[nz[0] + s0: nz[0] + e0] = False
            dropped += int(e0 - s0)
    return x[keep], dropped


def peak_note(seg, rate):
    w = seg * np.hanning(len(seg))
    nfft = 1 << (len(seg) * 4 - 1).bit_length()
    mag = np.abs(np.fft.rfft(w, nfft))
    f = np.fft.rfftfreq(nfft, 1.0 / rate)
    band = (f >= 60) & (f <= 5000)
    if not band.any():
        return None
    i = int(np.argmax(np.where(band, mag, 0)))
    if mag[i] <= 0:
        return None
    return 69 + 12 * np.log2(f[i] / 440.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("dc")
    ap.add_argument("--max-lag", type=float, default=2.0, help="seconds (default 2)")
    ap.add_argument("--skip", type=float, default=0.0, help="seconds of REF to ignore at the start")
    ap.add_argument("--dur", type=float, default=0.0, help="seconds of REF to compare (0: all)")
    ap.add_argument("--min-corr", type=float, default=0.9)
    ap.add_argument("--min-pitch", type=float, default=0.0)
    ap.add_argument("--clip-pct", type=float, default=0.0,
                    help="clip both 50 ms envelopes at this percentile first (one-off effects)")
    ap.add_argument("--drop-gaps", type=int, default=0,
                    help="cut runs of exact silence of N samples to 0.1 s after DC's first sound")
    ap.add_argument("--label", default="")
    ap.add_argument("--plot", default="")
    a = ap.parse_args()

    rr, xr = load(a.ref)
    rd, xd = load(a.dc)
    dropped = 0
    if a.drop_gaps > 0:
        xd, dropped = drop_gaps(xd, a.drop_gaps, int(0.1 * rd))
    # coarse alignment on 10 ms envelopes
    er = envelope(xr, rr, 0.01)
    ed = envelope(xd, rd, 0.01)
    lag, _ = best_lag(er, ed, int(a.max_lag * 100))
    # the DC signal shifted so that sample 0 lines up with REF's sample 0
    off_d = int(round(lag * 0.01 * rd))
    s0 = a.skip
    s1 = len(xr) / rr if a.dur <= 0 else min(len(xr) / rr, a.skip + a.dur)
    if off_d >= 0:
        xd = xd[off_d:]
    else:
        xd = np.concatenate([np.zeros(-off_d), xd])
    s1 = min(s1, len(xd) / rd)
    if s1 - s0 < 1.0:
        print(f"FAIL {a.label}: less than 1 s overlaps ({s1 - s0:.2f} s)")
        return 1
    seg_r = xr[int(s0 * rr): int(s1 * rr)]
    seg_d = xd[int(s0 * rd): int(s1 * rd)]
    e50r = envelope(seg_r, rr, 0.05)
    e50d = envelope(seg_d, rd, 0.05)
    n = min(len(e50r), len(e50d))
    e50r, e50d = e50r[:n], e50d[:n]
    if a.clip_pct > 0:
        e50r = np.minimum(e50r, np.percentile(e50r, a.clip_pct))
        e50d = np.minimum(e50d, np.percentile(e50d, a.clip_pct))
    if e50r.std() == 0 or e50d.std() == 0:
        corr = 1.0 if np.allclose(e50r, e50d, atol=1.0) else 0.0
    else:
        corr = float(np.corrcoef(e50r, e50d)[0, 1])
    level = float(np.sqrt((seg_d ** 2).mean()) / max(1e-9, np.sqrt((seg_r ** 2).mean())))

    # pitch per 200 ms window
    agree = total = 0
    win = 0.2
    loud_r = np.sqrt((seg_r ** 2).mean()) * 0.25
    t = 0.0
    while t + win <= s1 - s0:
        a_r = seg_r[int(t * rr): int((t + win) * rr)]
        a_d = seg_d[int(t * rd): int((t + win) * rd)]
        t += win
        if len(a_r) < 16 or len(a_d) < 16:
            continue
        if np.sqrt((a_r ** 2).mean()) < loud_r or np.sqrt((a_d ** 2).mean()) < loud_r * 0.5:
            continue
        nr, nd = peak_note(a_r, rr), peak_note(a_d, rd)
        if nr is None or nd is None:
            continue
        total += 1
        if abs(nr - nd) <= 0.5:
            agree += 1
    pitch = agree / total if total else 1.0

    ok = corr >= a.min_corr and pitch >= a.min_pitch
    print(f"{'ok' if ok else 'FAIL'} {a.label}: lag {lag * 10:+d} ms, envelope r={corr:.3f} "
          f"({n} x 50 ms{', clipped at p%g' % a.clip_pct if a.clip_pct else ''}, bar {a.min_corr}), pitch {100 * pitch:.1f}% of {total} windows agree, "
          f"level {level:.2f}, {s1 - s0:.1f} s compared"
          f"{', %d gap samples cut' % dropped if a.drop_gaps > 0 else ''}")
    if a.plot:
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            tt = np.arange(n) * 0.05 + s0
            plt.figure(figsize=(10, 3))
            plt.plot(tt, e50r, label="native musrender", lw=1)
            plt.plot(tt, e50d, label="Dreamcast mixer", lw=1, alpha=0.8)
            plt.xlabel("s")
            plt.ylabel("RMS (50 ms)")
            plt.title(f"{a.label}: envelope r={corr:.3f}, pitch {100 * pitch:.0f}%")
            plt.legend(loc="upper right")
            plt.tight_layout()
            plt.savefig(a.plot, dpi=80)
        except ImportError:
            pass
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
