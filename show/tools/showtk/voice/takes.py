"""Takes: which audio a line uses, and making it (spec §5.6, §10.2).

Order per line id: rec: (a recording) > scratch: (a guide read) > lib: (a library take) >
tts. takes.toml (creator-owned) can pin any of them; a line it does not list uses that
order, so a recording dropped into the episode's vo/ folder wins over TTS on its own.

Recordings: <ep>/vo/<line_id>_take<n>.wav (also accepted: _t<n>; scratch reads _s<n>).
takes.toml picks one with `fav = <n>` (or `take = "rec:<n>"` / `take = "rec:<path>"`);
with no pick the highest take number wins (usually the last, best read).

Every take, whatever its source, is finished the same way: 48 kHz mono, dialogue level
-23 LUFS, exactly 30 ms (1,440 smp) before the first sound, a 60 ms tail, then stored in
the take store by the sha256 of its samples.
"""
from __future__ import annotations

import dataclasses as dc
import hashlib
import os
import re
import zlib

import numpy as np

from . import env, g2p as g2pm, kokoro, shwio, world

DX_LUFS = -23.0
TAIL = int(0.060 * env.SR)
ONSET_DB = -40.0


@dc.dataclass
class Take:
    kind: str                 # tts | rec | scratch | lib
    y: np.ndarray             # float32 48 kHz mono, starts PREROLL before the onset
    phones: list              # [(ch, t0, t1)] samples rel onset
    words: list               # [(text, t0, t1)] samples rel onset
    src: str                  # provenance string
    params: dict              # what made it (for the lock and re-renders)
    path: str = ""            # the stored file
    sha: str = ""             # sha256 of the stored PCM
    align: str = "tts"        # tts | whisper | energy | lib


# ----------------------------------------------------------------------------- voices
def load_cast():
    voices = shwio.read_voices(shwio.series_path("voices.shw"))
    cast = shwio.read_cast_voices(shwio.series_path("cast.shw"))
    return voices, cast


def voice_for(line, voices, cast):
    """-> (voice id, Voice). A line's `mode=` (possessed) picks the variant <voice>.<mode>."""
    vid = line.voice_ref
    if not line.walla and vid not in voices and vid in cast:
        vid = cast[vid]
    mode = line.attrs.get("mode")
    if mode and f"{vid}.{mode}" in voices:
        vid = f"{vid}.{mode}"
    if vid not in voices:
        raise KeyError(f"{env.rel(line.path)}:{line.lineno}: line {line.id}: no voice {vid!r} in series/voices.shw")
    return vid, voices[vid]


def _hash01(*parts):
    h = zlib.crc32("|".join(str(p) for p in parts).encode())
    return (h & 0xFFFFFFFF) / 2 ** 32


def _range(v, u):
    """'1.10..1.20' / '+4..+6st' / '1.0' -> a value inside the range picked by u in [0,1)."""
    v = str(v).replace("st", "")
    if ".." in v:
        a, b = v.split("..", 1)
        a, b = float(a), float(b)
        return round(a + (b - a) * u, 3)
    return float(v)


def tts_params(line, vid, voice, take_no=0):
    """The deterministic TTS recipe for one line: blend, speed, WORLD shape."""
    a = voice.attrs
    seed = int(a.get("seed", 0))
    if "pool" in a:
        pool = a["pool"].split(",")
        blend = pool[int(_hash01(seed, line.id, take_no, "v") * len(pool))]
    else:
        blend = a.get("kokoro", "af_heart")
    speed = _range(a.get("speed", 1.0), _hash01(seed, line.id, take_no, "s"))
    pitch = _range(a.get("pitch", 0), _hash01(seed, line.id, take_no, "p"))
    form = _range(a.get("formant", 0), _hash01(seed, line.id, take_no, "f"))
    flat = float(a.get("flatten_f0", 0))
    utt_flags = set()
    if "{d}" in line.text:
        utt_flags.add("deadpan")
        flat = max(flat, float(a.get("deadpan_f0", 0.6)))
    wavy = 0.0
    if "{w}" in line.text:
        wavy = float(a.get("wavy", 0.8))
    if "speed" in line.attrs:
        speed = float(line.attrs["speed"])
    if "pitch" in line.attrs:
        pitch += float(str(line.attrs["pitch"]).replace("st", ""))
    shp = world.Shape(pitch=pitch, formant=form, flatten=flat, contour=a.get("contour", ""),
                      contour_amt=float(a.get("contour_amt", 1.0)), wavy=wavy,
                      double=float(a.get("double", 0)), breath=float(a.get("breath", 0)),
                      seed=seed + _seed_of(line.id))
    say = line.attrs.get("say", line.text)
    if a.get("babble") in ("1", "on", "yes"):
        say = babble(say, seed + _seed_of(line.id))
    return {"voice": vid, "kokoro": kokoro.blend_str(kokoro.parse_blend(blend)), "speed": round(speed, 3),
            "shape": dc.asdict(shp), "say": say}


_BABBLE = ["blub", "glub", "gloop", "blorp", "mub", "burb", "glorg", "wub", "bloop", "gorp"]


def babble(text, seed):
    """GURGLE: the caption's words become wet nonsense syllables, one per syllable, with the
    same punctuation, so the babble carries the caption's rhythm."""
    out = []
    k = 0
    for tok in re.findall(r"[A-Za-z']+|[.,!?…]+|\S", text):
        if tok[0].isalpha():
            n = max(1, len(re.findall(r"[aeiouy]+", tok.lower())))
            syl = []
            for _ in range(n):
                syl.append(_BABBLE[int(_hash01(seed, k) * len(_BABBLE))])
                k += 1
            out.append("-".join(syl))
        else:
            if out:
                out[-1] += tok
    return " ".join(out)


def _seed_of(s):
    return zlib.crc32(s.encode()) & 0xFFFF


def src_tag(p):
    sh = world.Shape(**p["shape"]).tag()
    return f"tts:kokoro@{env.model_id()}:{p['kokoro']},speed={p['speed']:g}" + (f",{sh}" if sh else "")


# ----------------------------------------------------------------------------- audio helpers
def to48(y24):
    import soxr
    return soxr.resample(np.asarray(y24, dtype=np.float64), env.SR_K, env.SR, quality="VHQ").astype(np.float32)


def envelope_db(y, hop=120):
    y = np.asarray(y, dtype=np.float64)
    n = max(1, len(y) // hop)
    fr = y[: n * hop].reshape(n, hop)
    e = np.sqrt((fr ** 2).mean(axis=1) + 1e-20)
    return 20 * np.log10(e / (e.max() + 1e-20)), hop


def onset_end(y, thr=ONSET_DB):
    db, hop = envelope_db(y)
    idx = np.nonzero(db > thr)[0]
    if not len(idx):
        return 0, len(y)
    return int(idx[0] * hop), int(min(len(y), (idx[-1] + 1) * hop))


def lufs(y):
    import pyloudnorm as pyln
    x = np.asarray(y, dtype=np.float64)
    if len(x) < int(0.5 * env.SR):
        x = np.concatenate([x, np.zeros(int(0.5 * env.SR) - len(x))])
    m = pyln.Meter(env.SR)
    v = m.integrated_loudness(x)
    if not np.isfinite(v):
        # a very short word: fall back to active-speech RMS as LUFS-ish
        a = x[np.abs(x) > 1e-4]
        v = 20 * np.log10(np.sqrt(np.mean(a ** 2)) + 1e-12) - 0.7 if len(a) else -70.0
    return float(v)


def finish(y48, phones, words, level=DX_LUFS):
    """Normalise, cut to PREROLL before the onset and TAIL after the last sound.
    phones/words come in samples relative to y48's start; they leave relative to the onset."""
    on, end = onset_end(y48)
    a = max(0, on - env.PREROLL)
    lead_pad = env.PREROLL - (on - a)
    seg = y48[a:min(len(y48), end + TAIL)].astype(np.float64)
    if lead_pad > 0:
        seg = np.concatenate([np.zeros(lead_pad), seg])
    nf = min(len(seg), int(0.010 * env.SR))
    seg[-nf:] *= np.linspace(1, 0, nf)
    g = level - lufs(seg)
    seg = seg * 10 ** (g / 20)
    pk = np.abs(seg).max() if len(seg) else 0
    if pk > 0.89:   # -1 dBFS guard
        seg *= 0.89 / pk
    last = end - on

    def clip(t):
        return int(min(max(0, t - on), last))

    ph = [(c, clip(t0), clip(t1)) for c, t0, t1 in phones]
    wd = [(w, clip(t0), clip(t1)) for w, t0, t1 in words]
    return seg.astype(np.float32), ph, wd, last


def phones_from_render(rd):
    """Kokoro tokens -> phones at 48 kHz (render coordinates): marks merged into the next
    phone, pads/spaces/punctuation kept as silence tokens for the mouth."""
    out = []
    pend = None
    for t in rd.toks:
        t0, t1 = t.t0 * 2, t.t1 * 2
        if t.ch in "ˈˌː":
            pend = t0 if pend is None else pend
            continue
        if pend is not None:
            t0, pend = pend, None
        out.append((t.ch, t0, t1))
    return out


def quantize(y):
    """float -> the 24-bit sample values a take is stored as (libsndfile's own scale, so a
    stored take read back as float re-quantizes to the same integers)."""
    return np.clip(np.round(np.asarray(y, np.float64) * 8388608.0), -8388608, 8388607).astype("<i4")


def pcm_sha(y):
    """sha256 of the samples as stored: 24-bit PCM values in little-endian int32."""
    return hashlib.sha256(quantize(y).tobytes()).hexdigest()


def store(y, voice, ep, line_id, kind, tag=""):
    """Write the take to the store as 24-bit FLAC, named by its content hash."""
    import soundfile as sf
    q = quantize(y)
    sha = hashlib.sha256(q.tobytes()).hexdigest()
    d = os.path.join(env.show_lib(), "takes", voice.split(".")[0], ep)
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, f"{line_id}_{kind}{tag}_{sha[:12]}.flac")
    if not os.path.exists(p):
        sf.write(p, (q.astype(np.int64) << 8).astype(np.int32), env.SR, subtype="PCM_24", format="FLAC")
    chk = pcm_sha(load(p))
    if chk != sha:
        raise RuntimeError(f"take store round trip changed the samples: {p}")
    return p, sha


def load(path):
    import soundfile as sf
    y, sr = sf.read(path, dtype="float64", always_2d=True)
    y = y.mean(axis=1)
    if sr != env.SR:
        import soxr
        y = soxr.resample(y, sr, env.SR, quality="VHQ")
    return y.astype(np.float32)


# ----------------------------------------------------------------------------- TTS
_G2P = None


def g2p():
    global _G2P
    if _G2P is None:
        _G2P = g2pm.G2P()
    return _G2P


def render_tts(line, params):
    """-> Take (unstored). Kokoro speaks, WORLD shapes, the take is finished."""
    utt = g2p()(params["say"])
    rd = kokoro.render(utt, params["kokoro"], params["speed"])
    y24 = world.shape(rd.y, env.SR_K, world.Shape(**params["shape"]))
    y48 = to48(y24)
    ph = phones_from_render(rd)
    wd = [(w, t0 * 2, t1 * 2) for w, t0, t1 in rd.words]
    y, ph, wd, end = finish(y48, ph, wd)
    p = dict(params)
    p["phon"] = utt.phon
    p["oov"] = utt.oov
    return Take("tts", y, ph, wd, src_tag(params), p, align="tts")


# ----------------------------------------------------------------------------- recordings
TAKE_RE = re.compile(r"^(?P<id>[a-z][a-z0-9_]*?)_(?P<kind>take|t|s)(?P<n>\d+)\.(wav|flac|aiff?|ogg)$", re.I)


def vo_files(ep_dir):
    """{line_id: {'rec': {n: path}, 'scratch': {n: path}}} from <ep>/vo/."""
    out = {}
    dirs = [os.path.join(ep_dir, "vo")]
    # docs/show/ep101/RECORDING_GUIDE.md §5: show/lib/takes/<voice>/e101/<id>_t<n>.wav
    epn = "e" + os.path.basename(os.path.normpath(ep_dir)).lstrip("ep")
    lib = os.path.join(env.SHOW, "lib", "takes")
    if os.path.isdir(lib):
        for v in sorted(os.listdir(lib)):
            dirs.append(os.path.join(lib, v, epn))
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            m = TAKE_RE.match(f)
            if not m:
                continue
            kind = "scratch" if m.group("kind").lower() == "s" else "rec"
            slot = out.setdefault(m.group("id"), {}).setdefault(kind, {})
            n = int(m.group("n"))
            p = os.path.join(d, f)
            # the raw .wav beats the guide's ingested .flac beside it (we finish it ourselves)
            if n not in slot or not p.lower().endswith(".flac"):
                slot[n] = p
    return out


def choose(line, takes_cfg, vo):
    """-> (kind, ref, retime). kind: rec | scratch | lib | tts; ref: a path, a lib name or None."""
    cfg = takes_cfg.get(line.id, {})
    retime = cfg.get("retime", "fit")
    files = vo.get(line.id, {})
    pick = cfg.get("take")
    fav = cfg.get("fav")
    if pick is None and fav is not None:
        pick = f"rec:{fav}"
    if isinstance(pick, str):
        kind, _, ref = pick.partition(":")
        if kind == "tts":
            return "tts", None, retime
        if kind in ("rec", "scratch"):
            if ref.isdigit():
                p = files.get(kind, {}).get(int(ref))
                if p is None:
                    raise FileNotFoundError(f"takes.toml [{line.id}] {pick}: no {kind} take {ref} in vo/")
                return kind, p, retime
            return kind, env.unrel(ref), retime
        if kind == "lib":
            return "lib", ref, retime
        raise ValueError(f"takes.toml [{line.id}]: unknown take {pick!r}")
    for kind in ("rec", "scratch"):
        if files.get(kind):
            n = max(files[kind])
            return kind, files[kind][n], retime
    if line.lib_take:
        return "lib", line.lib_take[4:], retime
    return "tts", None, retime


def ingest(path, line, params, kind="rec", denoise=True):
    """A recording -> a finished Take: high-pass 80 Hz, light denoise from the head's room
    tone, trim, -23 LUFS, then align (Whisper words + the TTS render's phone ratios)."""
    from scipy.signal import butter, sosfiltfilt
    from . import align
    y = load(path).astype(np.float64)
    sos = butter(2, 80, btype="high", fs=env.SR, output="sos")
    y = sosfiltfilt(sos, y)
    if denoise and line.attrs.get("fx") != "dry":
        y = _denoise(y)
    on, end = onset_end(y.astype(np.float32))
    # alignment on the recording (relative to its own start), then finish() rebases to the onset
    ph, wd, how = align.align_recording(y.astype(np.float32), line, params)
    yf, ph, wd, _ = finish(y.astype(np.float32), ph, wd)
    with open(path, "rb") as fh:
        fsha = hashlib.sha256(fh.read()).hexdigest()
    p = {"file": env.rel(path), "file_sha256": fsha, "say": params.get("say", line.text), "voice": params.get("voice")}
    return Take(kind, yf, ph, wd, f"{kind}:{fsha[:16]}", p, align=how)


def _denoise(y, nfft=1024, hop=256):
    """Spectral gate: the quietest 0.25 s is the noise print; bins within 6 dB of it are
    attenuated by 12 dB. Light on purpose: dialogue, not restoration."""
    from scipy.signal import istft, stft
    f, t, Z = stft(y, fs=env.SR, nperseg=nfft, noverlap=nfft - hop)
    mag = np.abs(Z)
    fe = mag.mean(axis=0)
    w = max(1, int(0.25 * env.SR / hop))
    if mag.shape[1] <= w:
        return y
    csum = np.convolve(fe, np.ones(w) / w, mode="valid")
    i = int(np.argmin(csum))
    noise = mag[:, i:i + w].mean(axis=1, keepdims=True)
    gain = np.where(mag < noise * 2.0, 10 ** (-12 / 20), 1.0)
    from scipy.ndimage import uniform_filter
    gain = uniform_filter(gain, size=(3, 5))
    _, yo = istft(Z * gain, fs=env.SR, nperseg=nfft, noverlap=nfft - hop)
    return yo[: len(y)] if len(yo) >= len(y) else np.concatenate([yo, np.zeros(len(y) - len(yo))])
