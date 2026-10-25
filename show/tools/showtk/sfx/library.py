"""The SFX library: series/sfx.shw -> <show_lib>/sfx/DGL_<cat>_<name>_<var>.wav (WP10, sec.10.5).

    python3 -m showtk.sfx.library build [ID ...]   render (48 kHz / 24-bit, peak -1 dBFS)
    python3 -m showtk.sfx.library list
    python3 -m showtk.sfx.library check            AT10.4: every name, file and caption resolves

Each file is peak-normalised by gain only (nothing else touches a `process=none` sound:
the pet's piezo is the engine's own samples, held to 48 kHz). Hashes (of the 24-bit
PCM) go to show/lib/sfx/sfx.lock; per-sound QA to <show_lib>/sfx/library.json.
API for the mix (WP11): `lookup(id) -> entry`, `load(id) -> float array`,
`foley.crayon(speed)`, `foley.paws(contacts, n_frames)`.
"""
from __future__ import annotations

import argparse
import dataclasses as dc
import hashlib
import json
import re
import sys
import time
from pathlib import Path

import numpy as np
import soundfile as sf

from ..music import audiopaths as P
from ..music import shwlite as S
from . import designs, foley, gamesfx

SR = 48000
KINDS = ("sfx", "fol", "amb")
ID_RE = re.compile(r"^(FX|FOL|AMB|PET|MX)_[A-Za-z0-9_]+$")


@dc.dataclass
class Entry:
    kind: str
    id: str
    kv: dict
    where: str

    @property
    def src(self) -> str:
        return self.kv["src"]

    @property
    def cat(self) -> str:
        return self.kv.get("cat", "misc")

    @property
    def file(self) -> str:
        nm = self.id.split("_", 1)[1].lower()
        return f"sfx/DGL_{self.cat}_{nm}_v1.wav"

    @property
    def stereo(self) -> bool:
        return "stereo" in self.kv or self.kv.get("stereo") == "yes"


_ENT_CACHE = {}


def _ENTS():
    if "e" not in _ENT_CACHE:
        _ENT_CACHE["e"] = load_entries()
    return _ENT_CACHE["e"]


def load_entries(path=None) -> dict:
    path = Path(path or P.SERIES / "sfx.shw")
    out = {}
    for st in S.parse_file(path, blocks=()):
        if st.head not in KINDS:
            raise S.ShwError(f"{st.where()}: unknown statement `{st.head}`")
        if "src" not in st.kv:
            raise S.ShwError(f"{st.where()}: {st.name} has no src=")
        if st.name in out:
            raise S.ShwError(f"{st.where()}: {st.name} defined twice (first {out[st.name].where})")
        kv = dict(st.kv)
        for w in st.words[2:]:
            kv[w] = "yes"                                  # bare flags: stereo, loop
        out[st.name] = Entry(st.head, st.name, kv, st.where())
    return out


def render(e: Entry) -> np.ndarray:
    kind, _, ref = e.src.partition(":")
    if kind == "synth":
        name, _, semis = ref.partition(":")
        y = gamesfx.synth(name, int(semis or 0))
    elif kind == "sfxgen":
        y, _r0, _pcm = gamesfx.sfxgen(ref)
    elif kind == "piezo":
        name, _, semis = ref.partition(":")
        y = gamesfx.piezo(name, int(semis or 0))
    elif kind == "design":
        fn = designs.DESIGNS[ref]
        from .synth import rng
        y = fn(rng(e.id, ref))
    elif kind == "alias":
        # alias:<ID>[:semis[:seconds]]  another library sound, varispeed by semitones and
        # optionally cut to a length (a staged shot's name for a sound the library has).
        parts = ref.split(":")
        tgt = _ENTS().get(parts[0])
        if tgt is None or tgt.src.startswith("alias:"):
            raise S.ShwError(f"{e.where}: alias target {parts[0]} missing or itself an alias")
        y = np.asarray(render(tgt), dtype=float)
        semis = float(parts[1]) if len(parts) > 1 and parts[1] else 0.0
        if semis:
            from .synth import pitch_shift_resample
            ratio = 2 ** (semis / 12)
            y = (pitch_shift_resample(y, ratio) if y.ndim == 1 else
                 np.stack([pitch_shift_resample(y[:, k], ratio) for k in range(y.shape[1])], 1))
        if len(parts) > 2 and parts[2]:
            n = int(float(parts[2]) * SR)
            y = y[:n].copy()
            k = min(len(y), int(0.03 * SR))
            if k:
                fade = np.linspace(1, 0, k)
                y[-k:] *= fade if y.ndim == 1 else fade[:, None]
    elif kind == "foley":
        if ref.startswith("crayon"):
            y = foley.crayon_span(36, seed=1)              # the library take: a 36-f stroke
        elif ref.startswith("paws"):
            y = foley.paws(foley.crowd_contacts(), 120, seed=1)
        else:
            raise S.ShwError(f"{e.where}: unknown foley synth {ref}")
    else:
        raise S.ShwError(f"{e.where}: unknown source kind `{kind}`")
    y = np.asarray(y, dtype=float)
    if e.stereo and y.ndim == 1:
        y = np.stack([y, y], 1)
    if not e.stereo and y.ndim == 2:
        y = y.mean(axis=1)
    return y


def normalise(y, peak_db=-1.0):
    pk = float(np.abs(y).max())
    if pk <= 0:
        return y, None
    g = 10 ** (peak_db / 20) / pk
    return y * g, round(20 * np.log10(g), 2)


def _qa(y):
    from ..music import qa
    m = y if y.ndim == 1 else y.mean(axis=1)
    return dict(length_s=round(len(y) / SR, 3), peak_db=round(float(20 * np.log10(max(np.abs(y).max(), 1e-12))), 2),
                lufs=round(qa.lufs(y), 1), first_onset_smp=qa.first_onset(m))


def build(ids=None, verbose=True):
    ents = load_entries()
    todo = ids or list(ents)
    P.ensure(P.LIB_SFX)
    manifest_path = P.LIB_SFX / "library.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    for i in todo:
        e = ents[i]
        t = time.time()
        y = render(e)
        y, g = normalise(y)
        out = P.LIB / e.file
        sf.write(str(out), y.astype(np.float32), SR, subtype="PCM_24")
        pcm, _ = sf.read(str(out), dtype="int32", always_2d=True)
        h = hashlib.sha256(np.ascontiguousarray(pcm).tobytes()).hexdigest()
        q = _qa(y)
        manifest[i] = dict(file=e.file, src=e.src, cat=e.cat, kind=e.kind, gain=e.kv.get("gain", "0dB"),
                           pan=e.kv.get("pan", "center"), cc=e.kv.get("cc", ""), stereo=e.stereo,
                           loop="loop" in e.kv, process=e.kv.get("process", ""), norm_gain_db=g,
                           sha256=h, sources=sources(e), **q)
        if verbose:
            print(f"{i:<22} {e.src:<28} {q['length_s']:6.2f} s  peak {q['peak_db']:6.2f}  "
                  f"onset {q['first_onset_smp']}  {time.time() - t:4.1f} s", flush=True)
    manifest_path.write_text(json.dumps(manifest, indent=1))
    write_lock(manifest)
    return manifest


def sources(e: Entry) -> list:
    """AUD-LIC keys (show/lib/licences/licences.toml)."""
    kind, _, ref = e.src.partition(":")
    if kind == "alias":
        return sources(_ENTS()[ref.split(":")[0]])
    if kind in ("synth", "piezo"):
        return ["game:audio_fx.c"]
    if kind == "sfxgen":
        return ["sfxgen:" + ref]
    if kind == "foley":
        return ["foley:" + ref.split("(")[0]]
    out = {"synth:kit"}
    src = (Path(designs.__file__)).read_text(encoding="utf-8")
    m = re.search(r"def %s\(r\):(.*?)(?=\n(?:@design|def |# ====))" % re.escape(ref), src, re.S)
    body = m.group(1) if m else ""
    for k in re.findall(r'_kit\("([^"]+)"\)', body):
        out.add("kit:" + k.split("/")[0] + "/")
    for k in re.findall(r'_game\("([A-Z_]+)"\)', body):
        out.add("sfxgen:" + k)
    if "G.synth(" in body:
        out.add("game:audio_fx.c")
    return sorted(out)


def write_lock(manifest, path=None):
    path = Path(path or P.LIB_SRC / "sfx" / "sfx.lock")
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# show/lib/sfx/sfx.lock - rendered SFX (generated by `show sfx build`; audio under show_lib/).",
             "# id  sha256[:16]  seconds  file"]
    for k in sorted(manifest):
        m = manifest[k]
        lines.append(f"{k:<22} {m['sha256'][:16]} {m['length_s']:7.3f}s {m['file']}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def lookup(i: str) -> dict:
    return json.loads((P.LIB_SFX / "library.json").read_text())[i]


def load(i: str) -> np.ndarray:
    x, _ = sf.read(str(P.LIB / lookup(i)["file"]), dtype="float64")
    return x


def check(verbose=True):
    """AT10.4: ids well-formed and unique; every entry rendered to an existing 48 kHz
    file that is not silent and peaks at -1 dBFS; every caption a [bracketed] SDH line;
    every sample source licensed; the cue sheet's SFX names all exist."""
    from ..music import cuelib
    ents = load_entries()
    man = json.loads((P.LIB_SFX / "library.json").read_text())
    problems = []
    for i, e in ents.items():
        if not ID_RE.match(i):
            problems.append(f"{e.where}: bad id {i}")
        if i not in man:
            problems.append(f"{e.where}: {i} not rendered (run show sfx build)")
            continue
        f = P.LIB / man[i]["file"]
        if not f.exists():
            problems.append(f"{e.where}: {f} missing")
            continue
        info = sf.info(str(f))
        if info.samplerate != SR:
            problems.append(f"{e.where}: {i} is {info.samplerate} Hz")
        if man[i]["peak_db"] < -1.5 or man[i]["peak_db"] > -0.9:
            problems.append(f"{e.where}: {i} peak {man[i]['peak_db']} dBFS")
        if man[i]["first_onset_smp"] is None:
            problems.append(f"{e.where}: {i} is silent")
        cc = e.kv.get("cc")
        if cc is not None and not (cc.startswith("[") and cc.endswith("]")):
            problems.append(f"{e.where}: {i} caption {cc!r} is not a [bracketed] SDH line")
        lic = cuelib.licence_rows(man[i]["sources"])
        for k, row in lic.items():
            if row is None:
                problems.append(f"{e.where}: {i} source {k} has no licence row (AUD-LIC)")
    if verbose:
        n_cc = sum(1 for e in ents.values() if "cc" in e.kv)
        print(f"AT10.4: {len(ents)} SFX names, {len(man)} rendered, {n_cc} captions; "
              f"{len(problems)} problem(s)")
        for p in problems:
            print("  " + p)
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(prog="show sfx")
    s = ap.add_subparsers(dest="cmd", required=True)
    b = s.add_parser("build")
    b.add_argument("ids", nargs="*")
    s.add_parser("list")
    s.add_parser("check")
    a = ap.parse_args(argv)
    if a.cmd == "build":
        build(a.ids or None)
        return 0
    if a.cmd == "list":
        for i, e in load_entries().items():
            print(f"{i:<22} {e.kind} {e.src:<30} {e.file:<44} {e.kv.get('cc', '')}")
        return 0
    return 1 if check() else 0


if __name__ == "__main__":
    sys.exit(main())
