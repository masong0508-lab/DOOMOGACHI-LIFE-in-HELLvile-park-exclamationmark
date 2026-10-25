"""Core constants and helpers shared by every kit module.

Audio convention: float64 numpy arrays, shape (n, 2) for stereo, (n,) for mono,
sample rate SR (48 kHz, the broadcast / animation standard).
"""
from __future__ import annotations

import functools
import os
import subprocess
import tempfile

import numpy as np
import soundfile as sf
from scipy import signal

SR = 48000
A4 = 440.0
KIT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(KIT_DIR)
# show/audio vendoring: the samples (168 MB) live in the vault, not in git, and the
# cache must not land inside the repo. KIT_ASSETS / KIT_CACHE override both.
ASSETS = os.environ.get("KIT_ASSETS") or os.path.join(ROOT_DIR, "assets")
CACHE = os.environ.get("KIT_CACHE") or os.path.join(ROOT_DIR, "..", "build", "cache", "kit")

NOTE_NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
_PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def limit_threads(n: int = 2) -> None:
    """Cap BLAS / numba / OpenMP threads (the render box is shared)."""
    for k in ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS",
              "NUMBA_NUM_THREADS", "VECLIB_MAXIMUM_THREADS"):
        os.environ.setdefault(k, str(n))


def mtof(m):
    """MIDI note (float ok) -> Hz at A4 = 440 (12-TET)."""
    return A4 * 2.0 ** ((np.asarray(m, dtype=float) - 69.0) / 12.0)


def ftom(f):
    return 69.0 + 12.0 * np.log2(np.asarray(f, dtype=float) / A4)


def note(name: str) -> int:
    """'Ab4' / 'C#5' / 'Eb2' / 'Bb-1' -> MIDI number (C4 = 60)."""
    s = name.strip()
    pc = _PC[s[0].upper()]
    i = 1
    while i < len(s) and s[i] in "#b":
        pc += 1 if s[i] == "#" else -1
        i += 1
    octave = int(s[i:])
    return pc + 12 * (octave + 1)


def note_name(m: int) -> str:
    m = int(round(m))
    return f"{NOTE_NAMES[m % 12]}{m // 12 - 1}"


def db2a(db):
    return 10.0 ** (np.asarray(db, dtype=float) / 20.0)


def a2db(a, floor=1e-12):
    return 20.0 * np.log10(np.maximum(np.abs(a), floor))


def stereo(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=float)
    if x.ndim == 1:
        return np.stack([x, x], axis=1)
    if x.shape[1] == 1:
        return np.repeat(x, 2, axis=1)
    return x


def mono(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=float)
    return x if x.ndim == 1 else x.mean(axis=1)


def pan_stereo(x: np.ndarray, pan: float = 0.0) -> np.ndarray:
    """Constant-power pan of a mono signal (pan -1 = L, +1 = R)."""
    x = mono(x)
    th = (pan + 1.0) * np.pi / 4.0
    return np.stack([x * np.cos(th), x * np.sin(th)], axis=1) * np.sqrt(2.0)


def balance(x: np.ndarray, pan: float) -> np.ndarray:
    """Stereo balance (keeps the image, tilts it)."""
    x = stereo(x).copy()
    if pan > 0:
        x[:, 0] *= np.cos(pan * np.pi / 2) ** 0.5
    elif pan < 0:
        x[:, 1] *= np.cos(-pan * np.pi / 2) ** 0.5
    return x


def place(buf: np.ndarray, x: np.ndarray, start: int) -> None:
    """Add x into buf at sample `start`, clipping to the buffer bounds."""
    if start >= len(buf):
        return
    if start < 0:
        x = x[-start:]
        start = 0
    n = min(len(x), len(buf) - start)
    if n > 0:
        buf[start:start + n] += x[:n]


def fade(x: np.ndarray, fin: int = 0, fout: int = 0) -> np.ndarray:
    x = x.copy()
    if fin > 0:
        fin = min(fin, len(x))
        r = np.sin(np.linspace(0, np.pi / 2, fin)) ** 2
        x[:fin] *= r if x.ndim == 1 else r[:, None]
    if fout > 0:
        fout = min(fout, len(x))
        r = np.cos(np.linspace(0, np.pi / 2, fout)) ** 2
        x[-fout:] *= r if x.ndim == 1 else r[:, None]
    return x


def resample(x: np.ndarray, sr_in: int, sr_out: int = SR) -> np.ndarray:
    if sr_in == sr_out:
        return x
    g = np.gcd(int(sr_in), int(sr_out))
    return signal.resample_poly(x, sr_out // g, sr_in // g, axis=0)


def _decode_with_ffmpeg(path: str):
    os.makedirs(CACHE, exist_ok=True)
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False, dir=CACHE) as tf:
        tmp = tf.name
    try:
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-c:a", "pcm_f32le", tmp],
                       check=True)
        return sf.read(tmp, dtype="float64", always_2d=True)
    finally:
        os.unlink(tmp)


@functools.lru_cache(maxsize=1024)
def _load_cached(path: str, sr: int):
    try:
        data, fsr = sf.read(path, dtype="float64", always_2d=True)
    except Exception:
        data, fsr = _decode_with_ffmpeg(path)
    data = resample(data, fsr, sr)
    # MP3 decoders add encoder delay; we trim leading near-silence in the samplers
    data.setflags(write=False)
    return data


def load(path: str, sr: int = SR, as_mono: bool = False) -> np.ndarray:
    """Load any audio file (wav/flac/aiff/mp3/ogg) at `sr`. Returns a copy.

    Relative paths are tried against the working directory first, then assets/.
    """
    if not os.path.isabs(path):
        path = os.path.abspath(path) if os.path.exists(path) else os.path.join(ASSETS, path)
    x = np.array(_load_cached(path, sr))
    return mono(x) if as_mono else x


def write_wav(path: str, x: np.ndarray, sr: int = SR, subtype: str = "PCM_24") -> str:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    sf.write(path, np.asarray(x, dtype=np.float64), sr, subtype=subtype)
    return path


def trim_silence(x: np.ndarray, thresh_db: float = -60.0, pad: int = 0) -> np.ndarray:
    """Trim leading silence (MP3 encoder delay, slow sample starts)."""
    m = np.abs(mono(x))
    th = db2a(thresh_db) * max(m.max(), 1e-9)
    idx = np.argmax(m > th)
    return x[max(0, idx - pad):]


def rng(*keys, seed: int = 1234) -> np.random.Generator:
    """Deterministic per-purpose random stream: every render is reproducible."""
    import zlib
    h = zlib.crc32(("|".join([str(seed)] + [str(k) for k in keys])).encode())
    return np.random.default_rng(h)
