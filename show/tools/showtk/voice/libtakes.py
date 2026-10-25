"""Library takes: `lib:<voice>/<name>_v<n>`, the same samples every time (spec §10.2).

The library lives in the take store; show/lib/takes/index.json (committed) pins each
take's sha256, how it was made and whether it is synthetic. `show voice lib make <name>`
renders the synthetic ones from the recipes below; a creator's recording replaces one
with `show voice lib ingest <wav> <name>` (index entry synthetic=false).

  mayor/heehee_v1   "...Hee. Hee.": sung (singer.py), two falling notes a major third
                    apart (E4 -> C4, "mi... do"), each 12 f, 8 f apart (BREAKDOWN §2.3)
  grum/purr_v1      Grumbleguts' purr: a low, pleased "Mmmh", vocal fry, rising at the end
  grum/purr_v2      the shorter alternate
Not voice (WP10's SFX library, listed so lines that name them resolve): pet/beep_v1 (the
game's raw piezo, process=none). The two Mayor hums (mayor/hum_dawn_v1, hum_plusone_v1)
need the MINGLE cue's notes from WP10's mml_legacy.py; until then they stay pending.
"""
from __future__ import annotations

import json
import os

import numpy as np

from . import env, kokoro, takes, world

INDEX = os.path.join(env.SHOW, "lib", "takes", "index.json")

PENDING = {
    "pet/beep_v1": "SFX, not a voice: the game's raw piezo from WP10's sfx library (process=none)",
    "mayor/hum_dawn_v1": "needs the MINGLE cue's held/muted notes (WP10 mml_legacy); sung by singer.py once they land",
    "mayor/hum_plusone_v1": "needs the MINGLE cue's notes (WP10 mml_legacy); the hum one note further",
}


def index():
    if not os.path.exists(INDEX):
        return {}
    with open(INDEX, encoding="utf-8") as fh:
        return json.load(fh)


def _write_index(ix):
    os.makedirs(os.path.dirname(INDEX), exist_ok=True)
    with open(INDEX, "w", encoding="utf-8") as fh:
        json.dump(ix, fh, indent=2, sort_keys=True, ensure_ascii=False)
        fh.write("\n")


def lookup(name):
    return index().get(name, {})


def pending_reason(name):
    return PENDING.get(name, f"library take {name} not made yet (`show voice lib make {name}`)")


def take(name, line):
    e = lookup(name)
    if not e:
        return None
    p = env.unrel(e["take"])
    if not os.path.exists(p):
        return None
    y = takes.load(p)
    tk = takes.Take("lib", y, [tuple(x) for x in e.get("phones", [])], [tuple(x) for x in e.get("words", [])],
                    f"lib:{name}@{e['sha256'][:12]}", {"synthetic": e.get("synthetic", True), "lib": name},
                    align="lib")
    return tk


# ----------------------------------------------------------------------------- recipes
def _mayor_blend():
    from . import shwio
    v = shwio.read_voices(shwio.series_path("voices.shw")).get("mayor")
    return v.attrs.get("kokoro", "am_puck:0.6+am_michael:0.4") if v else "am_puck:0.6+am_michael:0.4"


def _sung_syllable(y24, sr, hz, n_frames_out, fall_st=0.3):
    """One spoken syllable (unvoiced onset + a voiced vowel) re-sung by WORLD on a held
    note: the onset keeps its own timing, the vowel is stretched to fill the note, F0 is
    the note with a small fall. -> (y, onset length in samples)."""
    import pyworld as pw
    x = np.asarray(y24, np.float64)
    f0, t = pw.harvest(x, sr, f0_floor=60, f0_ceil=600, frame_period=world.FP)
    sp = pw.cheaptrick(x, f0, t, sr)
    ap = pw.d4c(x, f0, t, sr)
    v = np.nonzero(f0 > 0)[0]
    a, b = int(v[0]), int(v[-1]) + 1
    a0 = max(0, a - int(60 / world.FP))                       # 60 ms of the 'h' before the vowel
    n_on = a - a0
    n_v = max(4, n_frames_out - n_on)
    src = np.concatenate([np.arange(a0, a), np.linspace(a, b - 1, n_v).round().astype(int)])
    xs = np.linspace(0, 1, n_v)
    f0n = np.concatenate([np.zeros(n_on), hz * 2 ** (-fall_st * xs ** 2 / 12)])
    out = pw.synthesize(np.ascontiguousarray(f0n), np.ascontiguousarray(sp[src]), np.ascontiguousarray(ap[src]), sr,
                        world.FP)
    fade = int(0.03 * sr)
    out[-fade:] *= np.linspace(1, 0, fade)
    return out.astype(np.float32), int(n_on * world.FP / 1000 * sr)


def _heehee():
    """"...Hee. Hee.": the Mayor's voice says "hee", and WORLD sings it twice on two falling
    notes a major third apart, E4 then C4 ("mi... do"), 12 f each, the second starting 20 f
    after the first (8 f between), BREAKDOWN §2.3."""
    from . import g2p as g2pm
    blend = _mayor_blend()
    rd = kokoro.render(g2pm.G2P()("Hee."), blend, 1.0)
    a, b = kokoro._edges(rd.y)
    syl = rd.y[a:b]
    f = env.SR_K // env.FPS                                    # 800 samples per frame at 24 kHz
    n1, on1 = _sung_syllable(syl, env.SR_K, 329.63, int(12 * f / (world.FP / 1000 * env.SR_K)) + 12, 0.3)
    n2, on2 = _sung_syllable(syl, env.SR_K, 261.63, int(12 * f / (world.FP / 1000 * env.SR_K)) + 12, 0.8)
    lead = 4 * f
    s1 = lead - on1                                            # note 1's vowel starts at `lead`
    s2 = lead + 20 * f - on2                                   # note 2's vowel 20 f later
    y24 = np.zeros(s2 + len(n2) + 4 * f, np.float32)
    y24[s1:s1 + len(n1)] += n1
    y24[s2:s2 + len(n2)] += n2
    y = takes.to48(y24)
    k = 2                                                      # 24 kHz -> 48 kHz
    v1, v2 = lead * k, (lead + 20 * f) * k
    note = 12 * f * k
    ph = [("h", (s1) * k, v1), ("i", v1, v1 + note), (" ", v1 + note, v2 - on2 * k),
          ("h", v2 - on2 * k, v2), ("i", v2, v2 + note)]
    wd = [("Hee.", s1 * k, v1 + note), ("Hee.", v2 - on2 * k, v2 + note)]
    made = (f"tts+world:kokoro@{env.model_id()}:{kokoro.blend_str(kokoro.parse_blend(blend))},"
            f"\"hˈi\" sung E4:12f then C4:12f, 20 f apart")
    return y, ph, wd, made


def _purr(short=False):
    """Kokoro says "Mmmh." low and slow; WORLD drops it an octave-ish with fry (breath) and
    a slight rise at the end."""
    from . import g2p as g2pm
    blend = "am_fenrir:0.7+am_santa:0.3"
    G = g2pm.G2P()
    utt = G("Mmmh." if short else "Mmmmh... mmh.")
    rd = kokoro.render(utt, blend, 0.7)
    sh = world.Shape(pitch=-7.0, formant=-2.0, contour="lilt", contour_amt=-0.4, breath=0.35, seed=5)
    y24 = world.shape(rd.y, env.SR_K, sh)
    y = takes.to48(y24)
    ph = [("m", t.t0 * 2, t.t1 * 2) for t in rd.toks if t.ch not in "^$ .…"]
    made = f"tts:kokoro@{env.model_id()}:{blend},speed=0.7,{sh.tag()}"
    return y, ph, [("Mmmh.", ph[0][1], ph[-1][2])] if ph else [], made


RECIPES = {"mayor/heehee_v1": _heehee, "grum/purr_v1": lambda: _purr(False), "grum/purr_v2": lambda: _purr(True)}


def make(name, force=False, log=print):
    ix = index()
    if name in ix and not force:
        log(f"{name}: already in the library ({ix[name]['sha256'][:12]})")
        return ix[name]
    if name not in RECIPES:
        raise KeyError(f"no recipe for {name} ({pending_reason(name)})")
    y, ph, wd, made = RECIPES[name]()
    y, ph, wd, _ = takes.finish(np.asarray(y, np.float32), ph, wd)
    voice, nm = name.split("/", 1)
    p, sha = takes.store(y, voice, "lib", nm, "lib")
    ix[name] = {"take": env.rel(p), "sha256": sha, "made": made, "synthetic": True,
                "len": int(len(y)), "phones": [[c, int(a), int(b)] for c, a, b in ph],
                "words": [[w, int(a), int(b)] for w, a, b in wd]}
    _write_index(ix)
    log(f"{name}: made {sha[:12]} ({len(y) / env.SR:.2f} s) <- {made}")
    return ix[name]


def ingest(path, name, log=print):
    """A creator's recording becomes the library take (synthetic=false)."""
    import hashlib

    from . import align
    y = takes.load(path)
    ph = align.energy_phones(y)
    y, ph, wd, _ = takes.finish(y, ph, [])
    voice, nm = name.split("/", 1)
    p, sha = takes.store(y, voice, "lib", nm, "lib")
    with open(path, "rb") as fh:
        fsha = hashlib.sha256(fh.read()).hexdigest()
    ix = index()
    ix[name] = {"take": env.rel(p), "sha256": sha, "made": f"rec:{fsha[:16]}", "synthetic": False,
                "len": int(len(y)), "phones": [[c, int(a), int(b)] for c, a, b in ph], "words": []}
    _write_index(ix)
    log(f"{name}: ingested {sha[:12]} from {path}")
    return ix[name]
