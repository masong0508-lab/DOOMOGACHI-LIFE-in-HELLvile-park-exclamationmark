"""showtk.voice.clean - gentle TV-dialogue clean-up of a recorded take
(SHOW_ENGINE_SPEC.md sec.10.2 step 6: high-pass, light denoise, trim, -23 LUFS).

Chain (every step logged in `steps`):
  channel   the creator's files are stereo with little high-frequency coherence
            between L and R (coh < 0.3 above 2 kHz), so a mono sum would comb and
            dull the voice; the channel with the better speech-to-floor ratio is used
  HPF       80 Hz, 4th-order Butterworth, zero-phase
  denoise   STFT spectral attenuation, floor-limited to (45 dB - SNR) clipped to
            0..10 dB, gains smoothed across 5 bins and with a 0.3/0.75 attack/
            release one-pole in time (no musical noise / watery artefacts)
  de-click  ffmpeg adeclick
  de-ess    ffmpeg deesser i=0.35
  EQ        -1.5 dB @250 Hz (mud), +2 dB @3.5 kHz (presence), +1 dB shelf @9 kHz
  comp      2.5:1 @ -20 dBFS, 8 / 140 ms, soft knee
  trim      0.25 s of room tone kept before the first / after the last speech
  level     -23 LUFS-I (pyloudnorm) and a look-ahead true-peak limiter at -2 dBTP
Never time-stretches or pitch-shifts.  `level` = full | light (half denoise) |
minimal (HPF, trim, level only): the ingest falls back when a stage raises WER.
"""
import subprocess
import tempfile

import numpy as np
import soundfile as sf
import soxr
from scipy import signal
from scipy.ndimage import minimum_filter1d, uniform_filter1d

SR = 48000
TARGET_LUFS = -23.0
TP_LIMIT = -2.0


def frames_db(x, n=960):
    fr = x[: len(x) // n * n].reshape(-1, n)
    return 10 * np.log10((fr ** 2).mean(1) + 1e-12)


def noise_floor(x):
    """dBFS of the quietest 10 % of 20 ms frames"""
    return float(np.percentile(frames_db(x), 10))


def speech_level(x):
    return float(np.percentile(frames_db(x), 95))


def true_peak(x):
    return float(20 * np.log10(np.abs(soxr.resample(x, SR, SR * 4)).max() + 1e-12))


def lufs(x):
    import pyloudnorm as pyln
    return float(pyln.Meter(SR).integrated_loudness(x))


def channels(x):
    """candidate mono signals: {'L','R'} for stereo, {'M'} for mono"""
    if x.ndim == 1:
        return {"M": x}
    return {"L": x[:, 0], "R": x[:, 1]}


def snr(x):
    return speech_level(x) - noise_floor(x)


def hpf(x, fc=80):
    sos = signal.butter(4, fc, "highpass", fs=SR, output="sos")
    return signal.sosfiltfilt(sos, x)


def denoise(x, reduce_db):
    if reduce_db <= 0:
        return x
    n, hop = 2048, 512
    _, _, Z = signal.stft(x, SR, nperseg=n, noverlap=n - hop)
    P = np.abs(Z) ** 2
    e = P.sum(0)
    q = e <= np.percentile(e, 15)
    N = np.median(P[:, q], 1, keepdims=True) * 1.5 + 1e-14
    g = np.maximum(1 - 1.0 / np.maximum(P / N, 1e-6), 0) ** 0.5
    g = np.maximum(g, 10 ** (-reduce_db / 20))
    g = signal.convolve2d(g, np.ones((5, 1)) / 5, mode="same", boundary="symm")
    out = np.empty_like(g)
    out[:, 0] = g[:, 0]
    for i in range(1, g.shape[1]):
        a = np.where(g[:, i] > out[:, i - 1], 0.3, 0.75)
        out[:, i] = a * out[:, i - 1] + (1 - a) * g[:, i]
    _, y = signal.istft(Z * out, SR, nperseg=n, noverlap=n - hop)
    y = y[: len(x)]
    return np.pad(y, (0, len(x) - len(y)))


DECLICK = "adeclick=w=55:o=75:a=2:t=8"
DEESS = "deesser=i=0.35:m=0.5:f=0.5:s=o"
EQ = "equalizer=f=250:t=q:w=1.0:g=-1.5,equalizer=f=3500:t=q:w=0.9:g=2.0,highshelf=f=9000:g=1.0"
COMP = "acompressor=threshold=0.1:ratio=2.5:attack=8:release=140:knee=4:makeup=1"


def ffchain(x, chain):
    with tempfile.TemporaryDirectory() as td:
        a, b = td + "/a.wav", td + "/b.wav"
        sf.write(a, x.astype(np.float32), SR, subtype="FLOAT")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", a, "-af", chain, "-ar", str(SR),
                        "-c:a", "pcm_f32le", b], check=True)
        y, _ = sf.read(b, dtype="float64")
    y = y.mean(1) if y.ndim > 1 else y
    return y[: len(x)] if len(y) >= len(x) else np.pad(y, (0, len(x) - len(y)))


def trim(x, pad=0.25):
    db = frames_db(x, 240)
    thr = max(np.percentile(db, 10) + 10, db.max() - 45)
    on = np.where(db > thr)[0]
    if len(on) == 0:
        return x, 0
    s = max(0, on[0] * 240 - int(pad * SR))
    e = min(len(x), (on[-1] + 1) * 240 + int(pad * SR))
    y = x[s:e].copy()
    fade = int(0.01 * SR)
    y[:fade] *= np.linspace(0, 1, fade)
    y[-fade:] *= np.linspace(1, 0, fade)
    return y, s


def limit(x, ceil_db=TP_LIMIT):
    ceil = 10 ** ((ceil_db - 0.2) / 20)
    for _ in range(4):
        up = np.abs(soxr.resample(x, SR, SR * 4)).reshape(-1)
        env = np.maximum.reduceat(up, np.arange(0, len(up), 4))[: len(x)]
        env = np.pad(env, (0, max(0, len(x) - len(env))))
        g0 = np.minimum(1, ceil / np.maximum(env, 1e-9))
        if g0.min() >= 1:
            break
        la = int(0.003 * SR)
        g = uniform_filter1d(minimum_filter1d(g0, 2 * la + 1), la)
        x = x * np.minimum(g, minimum_filter1d(g0, 3))
    return x


def process(x, level="full"):
    """x: mono float64 at 48 kHz -> (y, steps, info)"""
    s = snr(x)
    steps = ["HPF 80 Hz (4th-order Butterworth, zero-phase)"]
    y = hpf(x)
    red = float(np.clip(45 - s, 0, 10))
    red = {"full": red, "light": red / 2, "minimal": 0.0}[level]
    if red > 0:
        y = denoise(y, red)
        steps.append("spectral denoise max %.1f dB (raw SNR %.1f dB)" % (red, s))
    y = y * 10 ** ((-6 - 20 * np.log10(np.abs(y).max() + 1e-9)) / 20)
    if level != "minimal":
        y = ffchain(y, ",".join([DECLICK, DEESS, EQ, COMP]))
        steps += ["de-click", "de-ess", "EQ -1.5@250 +2@3.5k +1 shelf@9k", "comp 2.5:1 @-20 dBFS"]
    y, off = trim(y)
    steps.append("trim 0.25 s room tone")
    y = y * 10 ** ((TARGET_LUFS - lufs(y)) / 20)
    for _ in range(3):
        y = limit(y)
        y = y * 10 ** ((TARGET_LUFS - lufs(y)) / 20)
        if true_peak(y) <= TP_LIMIT + 0.05:
            break
    steps.append("%.0f LUFS-I, TP limit %.0f dBTP" % (TARGET_LUFS, TP_LIMIT))
    return y, steps, dict(denoise_db=red, trim_offset=int(off), level=level)


def write_take(path, y):
    """48 kHz / 24-bit / mono WAV (and FLAC when path ends .flac)"""
    sf.write(path, np.clip(y, -1, 1), SR, subtype="PCM_24")
