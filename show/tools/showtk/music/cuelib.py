"""The cue library: series/music.shw -> rendered cues in show_lib/music/ (WP10).

    cue <id> src=<source> [bpm= meter= key=] [k=v ...]
      track <ch> inst=<kit preset>[:<GM key>] [gain=dB] [pan=] [arp=chord|arp|root]
            [transpose=] [send=dB] [eq=...]
      note <name> bar=<B.b> pitch=<note> [hum=<note>]      named notes (sec.10.4)
    end

Sources:
  mml:core/src/mml_<name>.c | mml:SONG_X    a game cue, through tools/mmlc.py
                                             (mml_legacy.py) and the kit presets
  score:<module>.<fn>                       a score cue in show/lib/music/<module>.py
                                             (<fn>(**params) -> scorekit.Cue)
  file:<path>                               an audio master, relative to the theme
                                             dir (docs/show/theme/ or the scratchpad's
                                             theme/final): swappable, the cue names a file

Every cue renders to <lib>/music/<id>.flac (48 kHz / 24-bit, at "unity":
-16 LUFS or the -1 dBTP ceiling) plus <id>.json (bar map, sections, named
notes, loudness, hash). show/lib/music/library.lock pins the hashes.
"""
from __future__ import annotations

import dataclasses as dc
import hashlib
import importlib.util
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

from . import audiopaths as P
from . import barmap as BM
from . import shwlite as S

RESERVED = {"src", "bpm", "meter", "key", "mode", "passes", "timing", "tempo", "from", "to",
            "fade", "desc", "bars", "barmap", "length", "lufs", "file", "loop", "tail"}


@dc.dataclass
class CueDef:
    id: str
    src: str
    kv: dict
    tracks: dict
    notes: dict
    where: str

    @property
    def kind(self) -> str:
        return self.src.split(":", 1)[0]

    @property
    def ref(self) -> str:
        return self.src.split(":", 1)[1] if ":" in self.src else ""

    def params(self) -> dict:
        return {k: v for k, v in self.kv.items() if k not in RESERVED}


def load_defs(path=None) -> dict:
    path = Path(path or P.SERIES / "music.shw")
    defs = {}
    for st in S.parse_file(path, blocks=("cue",)):
        if st.head != "cue":
            continue
        if "src" not in st.kv:
            raise S.ShwError(f"{st.where()}: cue {st.name} has no src=")
        tracks, notes = {}, {}
        for b in st.body:
            if b.head == "track":
                tracks[int(b.name)] = dict(b.kv)
            elif b.head == "note":
                notes[b.name] = dict(b.kv)
            else:
                raise S.ShwError(f"{b.where()}: unknown statement `{b.head}` in cue {st.name}")
        if st.name in defs:
            raise S.ShwError(f"{st.where()}: cue {st.name} defined twice")
        defs[st.name] = CueDef(st.name, st.kv["src"], dict(st.kv), tracks, notes, st.where())
    return defs


# ---- builders ----------------------------------------------------------------------
def _track_spec(t: dict) -> dict:
    inst = t.get("inst", "musicbox")
    spec = {}
    if ":" in inst and inst.split(":")[0].startswith("drums"):
        inst, key = inst.split(":", 1)
        spec["drum_note"] = int(key)
    spec["inst"] = inst
    for k in ("arp", "name"):
        if k in t:
            spec[k] = t[k]
    if "transpose" in t:
        spec["transpose"] = int(t["transpose"])
    params = {k[2:]: float(v) for k, v in t.items() if k.startswith("p.")}
    if params:
        spec["params"] = params
    return spec


def _mml_cue(d: CueDef, bends=None):
    from . import mml_legacy as M
    from . import scorekit as K
    song = M.song(d.ref)
    tracks = {ch: _track_spec(t) for ch, t in d.tracks.items()}
    passes = int(d.kv.get("passes", 1))
    sc = M.to_score(song, tracks, passes=passes, timing=d.kv.get("timing", "grid"),
                    title=d.id, tail_beats=float(d.kv.get("tail", 6)))
    if "tempo" in d.kv:
        sc.bpm = float(d.kv["tempo"])
    if "bars" in d.kv:
        sc.bars = int(d.kv["bars"])
    cue = K.Cue(d.id, bpm=sc.bpm, key=d.kv.get("key", "C"), bars=sc.bars, score=sc,
                desc=d.kv.get("desc", ""))
    for ch, t in d.tracks.items():
        name = tracks[ch].get("name") or f"ch{ch}"
        if "cents" in t and name in sc.tracks:          # a whole-track detune (e.g. "flat")
            st = float(t["cents"]) / 100.0
            for n in sc.tracks[name].notes:
                n.bend = [(0.0, st), (n.d + 4.0, st)]
        strip = {"gain": S.db(t.get("gain", "0"))}
        if "pan" in t:
            strip["pan"] = float(t["pan"])
        strip["sends"] = {"room": S.db(t.get("send", "-16"))}
        cue.mix[name] = strip
    if "verb" in d.kv:
        cue.verbs = {"room": dict(kind=d.kv["verb"], rt60=float(d.kv.get("rt60", 1.2)),
                                  hp=200, lp=9000)}
    if "post" in d.kv:
        cue.post = d.kv["post"].split(",")
    return cue


def _score_cue(d: CueDef):
    mod_name, _, fn = d.ref.partition(".")
    path = P.LIB_SRC / "music" / f"{mod_name}.py"
    key = f"showlib_music_{mod_name}"
    if key in sys.modules:
        mod = sys.modules[key]
    else:
        spec = importlib.util.spec_from_file_location(key, str(path))
        mod = importlib.util.module_from_spec(spec)
        sys.modules[key] = mod
        spec.loader.exec_module(mod)
    params = d.params()
    cue = getattr(mod, fn or "build")(**params)
    cue.name = d.id
    return cue


def apply_bend(score, points):
    """Pitch automation into a score: points [(beat, semis)] (piecewise linear,
    holding the last value) become Note.bend curves on every pitched note."""
    if not points:
        return score
    bt = np.array([p[0] for p in points], float)
    bv = np.array([p[1] for p in points], float)
    for tr in score.tracks.values():
        if tr.drum:
            continue
        for n in tr.notes:
            t = np.linspace(n.t, n.t + n.d + 1.0, 9)
            v = np.interp(t, bt, bv, left=0.0, right=bv[-1])
            if np.any(np.abs(v) > 1e-4):
                n.bend = [(float(tt - n.t), float(vv)) for tt, vv in zip(t, v)]
    return score


def _file_cue(d: CueDef):
    path = (P.theme_dir() / d.ref) if not d.ref.startswith("/") else Path(d.ref)
    x, sr = sf.read(str(path), dtype="float64", always_2d=True)
    if sr != 48000:
        from .kitenv import kit
        x = kit.core.resample(x, sr, 48000)
    return x, path


def build(d: CueDef, bends=None, write=True):
    """Render one cue -> (audio at unity, meta). bends: [(cue_smp, semis)] for a
    re-render with pitch automation (score and mml cues)."""
    from . import scorekit as K
    from . import kitenv
    meta = dict(id=d.id, src=d.src, where=d.where)
    kitenv.reset_used()
    if d.kind in ("mml", "score"):
        cue = _mml_cue(d) if d.kind == "mml" else _score_cue(d)
        if bends:
            spb_smp = 48000 * 60.0 / cue.score.bpm
            apply_bend(cue.score, [(s / spb_smp, st) for s, st in bends])
        y, info = K.render(cue)
        bm = BM.BarMap(bpm=cue.score.bpm, meter=cue.score.beats_per_bar, bars=cue.score.bars,
                       sections=info["sections"], source=d.src)
        meta.update(info)
        meta["named_notes"] = {**cue.notes, **{k: v for k, v in d.notes.items()}}
    elif d.kind == "file":
        y, path = _file_cue(d)
        meta["file"] = str(path)
        if "barmap" in d.kv:
            src = d.kv["barmap"]
            if src.startswith("midi:"):
                bm = BM.import_midi(P.theme_dir() / src[5:])
            else:
                bm = BM.load(P.theme_dir() / src)
        else:
            bm = BM.BarMap(bpm=float(d.kv.get("bpm", 120)),
                           meter=int(str(d.kv.get("meter", "4")).split("/")[0]),
                           bars=int(d.kv.get("bars", 1)), source=d.src)
        meta.update(bpm=bm.bpm, meter=bm.meter, bars=bm.bars, sections=bm.sections,
                    named_notes=d.notes, desc=d.kv.get("desc", ""))
        if bends:
            from . import automation as A
            curve = np.zeros(len(y))
            pts = sorted(bends)
            curve[:] = np.interp(np.arange(len(y)), [p[0] for p in pts], [p[1] for p in pts],
                                 left=0.0, right=pts[-1][1])
            y = A.varispeed(y, 2.0 ** (curve / 12.0))
    else:
        raise S.ShwError(f"{d.where}: unknown cue source {d.src}")
    # crop / length / fade
    if "from" in d.kv:
        y = y[bm.at(d.kv["from"]):]
    if "length" in d.kv:
        n = S.frames(d.kv["length"]) * 1600
        y = np.pad(y, ((0, max(0, n - len(y))), (0, 0)))[:n]
    if "fade" in d.kv:
        from .kitenv import kit
        y = kit.core.fade(y, 0, int(float(d.kv["fade"]) * 48000))
    y, lvl = K.unity(y, float(d.kv.get("lufs", -16.0)))
    meta.update(level=lvl, length_smp=len(y), barmap=bm.to_json(), sources=sources(d, kitenv.used()))
    if write and not bends:
        P.ensure(P.LIB_MUSIC)
        out = P.LIB_MUSIC / f"{d.id}.flac"
        sf.write(str(out), y.astype(np.float32), 48000, subtype="PCM_24")
        # hash the 24-bit PCM as stored (not the FLAC container, which carries
        # encoder details): re-rendering the same source gives the same hash
        pcm, _ = sf.read(str(out), dtype="int32", always_2d=True)
        h = hashlib.sha256(np.ascontiguousarray(pcm).tobytes()).hexdigest()
        meta["sha256"] = h
        meta["path"] = f"music/{d.id}.flac"
        with open(P.LIB_MUSIC / f"{d.id}.json", "w", encoding="utf-8") as f:
            json.dump(_jsonable(meta), f, indent=1)
        # the bar map is source-side data too (small text, in git): show/lib/music/barmaps/
        bdir = P.LIB_SRC / "music" / "barmaps"
        P.ensure(bdir)
        bj = dict(cue=d.id, **_jsonable(bm.to_json()))
        bj["named_notes"] = _jsonable(meta.get("named_notes", {}))
        with open(bdir / f"{d.id}.json", "w", encoding="utf-8") as f:
            json.dump(bj, f, indent=1)
            f.write("\n")
    return y, meta


def sources(d: CueDef, kit_files) -> list:
    """AUD-LIC keys for a cue: its own source plus every kit sample folder it loaded."""
    own = {"mml": "mml:" + d.ref, "score": "synth:kit", "file": "theme:" + d.ref}[d.kind]
    out = {own}
    if d.kind == "mml":
        out.add("synth:kit")
    for k in kit_files:
        parts = k.split("/")
        out.add("/".join(parts[:2]) + "/" if len(parts) > 2 else k)
    return sorted(out)


def licence_rows(keys, path=None):
    """keys -> {key: (row name, credit)} by the longest matching `match` prefix
    (show/lib/licences/licences.toml); a key with no row maps to None (AUD-LIC fails)."""
    import tomllib
    path = Path(path or P.LIB_SRC / "licences" / "licences.toml")
    rows = tomllib.loads(path.read_text(encoding="utf-8"))
    out = {}
    for k in keys:
        best = None
        for name, r in rows.items():
            for m in r.get("match", []):
                if k.startswith(m) and (best is None or len(m) > best[2]):
                    best = (name, r, len(m))
        out[k] = None if best is None or not best[1].get("in_mix", True) else \
            (best[0], best[1].get("credit", ""))
    return out


def _jsonable(o):
    if isinstance(o, dict):
        return {str(k): _jsonable(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_jsonable(v) for v in o]
    if isinstance(o, (np.floating, np.integer)):
        return o.item()
    return o


def load_meta(cue_id: str) -> dict:
    with open(P.LIB_MUSIC / f"{cue_id}.json", encoding="utf-8") as f:
        return json.load(f)


def load_audio(cue_id: str) -> np.ndarray:
    x, sr = sf.read(str(P.LIB_MUSIC / f"{cue_id}.flac"), dtype="float64", always_2d=True)
    return x


def barmap_of(cue_id: str) -> BM.BarMap:
    return BM.BarMap.from_json(load_meta(cue_id)["barmap"])


def write_lock(metas: list, path=None):
    """show/lib/music/library.lock: one row per rendered cue (hashes in git,
    audio in the vault / show_lib)."""
    path = Path(path or P.LIB_SRC / "music" / "library.lock")
    rows = {}
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.startswith("#"):
                parts = line.split()
                rows[parts[0]] = line
    for m in metas:
        lv = m["level"]
        rows[m["id"]] = (f"{m['id']:<26} {m['sha256'][:16]} smp={m['length_smp']:<9} "
                         f"lufs={lv['lufs']} tp={lv['tp']} bpm={m['bpm']} bars={m['bars']} "
                         f"{m['path']}")
    head = ("# show/lib/music/library.lock - rendered cues (generated by `show music build`).\n"
            "# id  sha256[:16]  samples  unity loudness  true peak  tempo  bars  file (under show_lib/)\n")
    path.write_text(head + "\n".join(rows[k] for k in sorted(rows)) + "\n", encoding="utf-8")
