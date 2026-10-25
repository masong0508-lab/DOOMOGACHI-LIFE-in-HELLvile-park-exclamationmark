"""Where everything the voice tools need lives, and the audio constants.

Nothing here writes into git. Binaries (models, the CMU dictionary data, rendered takes)
are found in this order:
  1. the environment: SHOW_MODELS (a dir laid out like the vault's vault-models),
     KOKORO_SRC / KOKORO_MODELS / KOKORO_VOICES / WHISPER_MODEL / SHOW_CMUDICT;
  2. show/build/models/ (gitignored; what `show vault fetch` restores);
  3. the scratchpad the kit was vendored from (theme/), found by glob.
Rendered and ingested takes go to the take store, SHOW_LIB (default: the scratchpad's
show_lib/, else show/build/lib/), keyed by content hash.
"""
from __future__ import annotations

import glob
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SHOWTK = os.path.dirname(HERE)
TOOLS = os.path.dirname(SHOWTK)
SHOW = os.path.dirname(TOOLS)
REPO = os.path.dirname(SHOW)
AUDIO = os.path.join(SHOW, "audio")
BUILD = os.path.join(SHOW, "build")

SR = 48000            # the one sample clock (spec §10.1)
FPS = 30
SPF = SR // FPS       # 1600 samples per frame
SR_K = 24000          # Kokoro's rate
SPF_K = SR_K // FPS   # 800
PREROLL = 1440        # 30 ms of pre-roll before the first sound of every take (spec §10.2)
R = {"R3": 3, "R5": 5, "R8": 8, "R13": 13, "R21": 21, "R26": 26, "R34": 34}

_SCRATCH_GLOBS = ["/tmp/claude-*/*/*/scratchpad", "/tmp/claude-*/*/scratchpad"]


def _first(paths):
    for p in paths:
        if p and os.path.exists(p):
            return p
    return None


def scratchpads():
    out = []
    env = os.environ.get("SHOW_SCRATCH")
    if env:
        out.append(env)
    for g in _SCRATCH_GLOBS:
        out.extend(sorted(glob.glob(g)))
    return [p for p in out if os.path.isdir(p)]


def _theme(rel):
    return [os.path.join(s, "theme", rel) for s in scratchpads()]


def models_root():
    return os.environ.get("SHOW_MODELS") or os.path.join(BUILD, "models")


def kokoro_src():
    return _first([os.environ.get("KOKORO_SRC"),
                   os.path.join(models_root(), "kokoro", "kokoro-fp32.onnx")]
                  + _theme("final_dl/npm_kokoro_fp32/kokoro-fp32.onnx"))


def kokoro_models():
    """Dir holding kokoro_analysis.onnx + kokoro_sing.onnx (made by kokoro_patch.py)."""
    c = [os.environ.get("KOKORO_MODELS"), os.path.join(models_root(), "kokoro")] + _theme("final/work/models")
    for p in c:
        if p and os.path.exists(os.path.join(p, "kokoro_analysis.onnx")):
            return p
    return None


def kokoro_voices():
    c = [os.environ.get("KOKORO_VOICES"), os.path.join(models_root(), "kokoro", "voices")] \
        + _theme("final_dl/npm_kokorojs/x/package/voices")
    for p in c:
        if p and os.path.exists(os.path.join(p, "af_heart.bin")):
            return p
    return None


def whisper_model():
    c = [os.environ.get("WHISPER_MODEL"), os.path.join(models_root(), "whisper_small")] \
        + _theme("final_dl/npm_whisper_small/m")
    for p in c:
        if p and os.path.exists(os.path.join(p, "model.bin")):
            return p
    return None


def show_lib():
    env = os.environ.get("SHOW_LIB")
    if env:
        return env
    for s in scratchpads():
        p = os.path.join(s, "show_lib")
        if os.path.isdir(p):
            return p
    return os.path.join(BUILD, "lib")


CMUDICT_SHA256 = "81917843c7f44ce2b094ac63873c2c7a4cf802040792c455ba3ca406891c3d22"  # cmudict 1.1.3 wheel data
CMUDICT_PIN = "cmudict==1.1.3"


def cmudict_path(fetch=True):
    """The CMU dictionary data file (BSD-style). Read as data; the GPL PyPI wrapper is
    never imported. If missing, it is extracted from the pinned PyPI wheel."""
    c = [os.environ.get("SHOW_CMUDICT"), os.path.join(show_lib(), "g2p", "cmudict.dict"),
         os.path.join(models_root(), "g2p", "cmudict.dict")]
    p = _first(c)
    if p or not fetch:
        return p
    import subprocess
    import tempfile
    import zipfile
    dst = os.path.join(show_lib(), "g2p", "cmudict.dict")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        subprocess.run([sys.executable, "-m", "pip", "download", "--no-deps", "--only-binary=:all:",
                        "-d", td, CMUDICT_PIN], check=True, capture_output=True)
        whl = glob.glob(os.path.join(td, "cmudict-*.whl"))[0]
        with zipfile.ZipFile(whl) as z:
            data = z.read("cmudict/data/cmudict.dict")
    if hashlib.sha256(data).hexdigest() != CMUDICT_SHA256:
        raise RuntimeError("cmudict.dict from PyPI does not match the pinned sha256")
    with open(dst, "wb") as fh:
        fh.write(data)
    return dst


def setup():
    """Point the vendored modules at the binaries and put show/audio/voice on sys.path."""
    m = kokoro_models()
    if m:
        os.environ.setdefault("KOKORO_MODELS", m)
    v = kokoro_voices()
    if v:
        os.environ.setdefault("KOKORO_VOICES", v)
    s = kokoro_src()
    if s:
        os.environ.setdefault("KOKORO_SRC", s)
    w = whisper_model()
    if w:
        os.environ.setdefault("WHISPER_MODEL", w)
    for k in ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS"):
        os.environ.setdefault(k, "2")
    vd = os.path.join(AUDIO, "voice")
    if vd not in sys.path:
        sys.path.insert(0, vd)


_SHA_CACHE = {}


def file_sha256(path, short=False):
    """sha256 of a file, cached by (path, size, mtime) in show/build/cache/sha.json."""
    import json
    st = os.stat(path)
    key = f"{os.path.abspath(path)}|{st.st_size}|{int(st.st_mtime)}"
    cache_p = os.path.join(BUILD, "cache", "sha.json")
    if not _SHA_CACHE and os.path.exists(cache_p):
        try:
            with open(cache_p) as fh:
                _SHA_CACHE.update(json.load(fh))
        except (OSError, ValueError):
            pass
    h = _SHA_CACHE.get(key)
    if h is None:
        d = hashlib.sha256()
        with open(path, "rb") as fh:
            for blk in iter(lambda: fh.read(1 << 20), b""):
                d.update(blk)
        h = d.hexdigest()
        _SHA_CACHE[key] = h
        os.makedirs(os.path.dirname(cache_p), exist_ok=True)
        with open(cache_p, "w") as fh:
            json.dump(_SHA_CACHE, fh, indent=0, sort_keys=True)
    return h[:16] if short else h


def model_id():
    """The provenance tag of the TTS model: the fp32 source sha plus the patched graph."""
    src = kokoro_src()
    an = os.path.join(kokoro_models() or "", "kokoro_analysis.onnx")
    a = file_sha256(src, True) if src else "nosrc"
    b = file_sha256(an, True) if os.path.exists(an) else "noanalysis"
    return f"{a}+{b[:8]}"


def ep_dir(ep):
    """An episode or line-set directory: 'ep101' -> show/ep101; 'dx_proof' -> the
    dialogue-proof set; an existing path is used as is."""
    if os.path.isdir(ep) and os.path.exists(os.path.join(ep, "lines.shw")):
        return os.path.abspath(ep)
    for p in (os.path.join(SHOW, ep), os.path.join(SHOW, "tests", ep),
              os.path.join(SHOW, "tests", "audio", ep)):
        if os.path.exists(os.path.join(p, "lines.shw")):
            return p
    return os.path.join(SHOW, ep)


def rel(path):
    """A path as stored in committed files: relative to show/ when inside it."""
    ap = os.path.abspath(path)
    if ap.startswith(SHOW + os.sep):
        return os.path.relpath(ap, SHOW)
    lib = os.path.abspath(show_lib())
    if ap.startswith(lib + os.sep):
        return "store:" + os.path.relpath(ap, lib)
    return ap


def unrel(p):
    if p.startswith("store:"):
        return os.path.join(show_lib(), p[6:])
    if os.path.isabs(p):
        return p
    if p.startswith("show/"):
        return os.path.join(REPO, p)
    return os.path.join(SHOW, p)
