"""Reports and lints on a voice.lock (spec §10.2 provenance, §13.5 AUD-*).

  report(ep, require="human-voice")  every line's source; with --require, lists every line
                                      still synthetic and exits 1 if there is one (AT9.4)
  lint(ep)                            AUD-ASR (WER > 0.34 with a script word not heard and the
                                      letters < 90 % alike: an extra word Whisper adds, or
                                      "the Weatherside" for "the weather side", does not
                                      count), G2P-OOV, STD-AGE
                                      (voices.shw), VOX-MOUTH (AT9.2 closures, measured from
                                      the audio), AUD-HEEHEE (<= 3 library "Hee. Hee."s)
  mouth(ep, line)                     the track, its frames and the closure check
"""
from __future__ import annotations

import os

from . import build, env, lock, mouth as mouthm, shwio, takes

WER_MAX = 0.34


def _msg(path, lineno, lid, rule, level, text):
    return f"{env.rel(path)}:{lineno}: {lid} [{rule}] {level} {text}"


def report(ep, require=None, log=print):
    d, lines, beds, cfg, vo = build.load_ep(ep)
    lk = lock.read(d)
    rows = lk.get("lines", {})
    kinds = {}
    for lid in lines:
        r = rows.get(lid, {})
        k = r.get("kind", "unlocked")
        kinds[k] = kinds.get(k, 0) + 1
        if not require:
            log(f"{lid:22s} {k:8s} {r.get('src', '')}")
    synth = lock.tts_lines(lk)
    log(f"{ep}: {len(lines)} lines: " + ", ".join(f"{v} {k}" for k, v in sorted(kinds.items()))
        + f"; TEMP TTS: {len(synth)} lines")
    if require == "human-voice":
        for lid, spk, text, kind in synth:
            ln = lines.get(lid)
            where = f"{env.rel(ln.path)}:{ln.lineno}" if ln else env.rel(lock.path(d))
            log(f"{where}: {lid} [AUD-HUMAN] synthetic ({kind}) {spk}: {text!r}")
        unl = [lid for lid in lines if lid not in rows]
        for lid in unl:
            log(f"{env.rel(lines[lid].path)}:{lines[lid].lineno}: {lid} [AUD-HUMAN] not locked yet")
        return 1 if (synth or unl) else 0
    return 0


def lint(ep, log=print):
    d, lines, beds, cfg, vo = build.load_ep(ep)
    lk = lock.read(d)
    rows = lk.get("lines", {})
    voices, cast = takes.load_cast()
    errs = warns = 0
    out = []
    # STD-AGE on voices
    for vid, v in voices.items():
        if v.age not in ("adult", "teen", "child"):
            out.append(_msg(v.path, v.lineno, vid, "STD-AGE", "ERROR", "voice has no age=adult|teen|child"))
            errs += 1
    # VOX-CAST: a role's voice row is its audition option 1 (what CASTING.md calls the default)
    opts, _ = shwio.read_auditions(shwio.series_path("voices.shw"))
    for role, o in opts.items():
        v = voices.get(role)
        if v and 1 in o:
            diff = {k: (v.attrs.get(k), x) for k, x in o[1].items() if v.attrs.get(k) != x}
            if diff:
                out.append(_msg(v.path, v.lineno, role, "VOX-CAST", "WARN",
                                f"voice row differs from audition 1: {diff}"))
                warns += 1
    heehee = 0
    n_bil = n_ok = 0
    for lid, ln in lines.items():
        r = rows.get(lid)
        if r is None:
            out.append(_msg(ln.path, ln.lineno, lid, "AUD-LOCK", "WARN", "not in voice.lock (run `show voice`)"))
            warns += 1
            continue
        if (ln.attrs.get("take", "").startswith("lib:mayor/heehee")) and ln.attrs.get("lint_skip") != "AUD-HEEHEE":
            heehee += 1
        if r.get("kind") == "pending":
            out.append(_msg(ln.path, ln.lineno, lid, "AUD-LOCK", "WARN", f"pending: {r.get('status', '')}"))
            warns += 1
            continue
        a = r.get("asr")
        if isinstance(a, dict):
            if a["wer"] > WER_MAX and a.get("recall", 0.0) < 1.0 and a.get("char", 0.0) < 0.9:
                out.append(_msg(ln.path, ln.lineno, lid, "AUD-ASR", "WARN",
                                f"WER {a['wer']:.2f}: heard {a['heard']!r} for {ln.text!r}"))
                warns += 1
        elif a == "pending":
            out.append(_msg(ln.path, ln.lineno, lid, "AUD-ASR", "WARN", "not checked yet"))
            warns += 1
        if r.get("oov"):
            out.append(_msg(ln.path, ln.lineno, lid, "G2P-OOV", "WARN",
                            f"guessed {r['oov']}: add a row to series/lexicon.shw"))
            warns += 1
        if r.get("mouth"):
            res = closures(r)
            n_bil += len(res)
            bad = [f for f, ok in res if not ok]
            n_ok += len(res) - len(bad)
            if bad:
                out.append(_msg(ln.path, ln.lineno, lid, "VOX-MOUTH", "ERROR",
                                f"no M within 1 f of the bilabial closure at frame(s) {bad}"))
                errs += 1
    if heehee > 3:
        out.append(f"{env.rel(os.path.join(d, 'lines.shw'))}: [AUD-HEEHEE] ERROR {heehee} library \"Hee. Hee.\"s (cap 3)")
        errs += 1
    for m in out:
        log(m)
    log(f"{ep} voice lint: {errs} errors, {warns} warnings; bilabial closures {n_ok}/{n_bil} with an M "
        f"within 1 f; AUD-HEEHEE {heehee}/3")
    return 1 if errs else 0


def closures(row):
    """[(onset frame, ok)] for every m/b/p of a locked line, the onsets measured from the
    take's audio (mouth.closure_onsets), the M looked up in the locked track."""
    y = takes.load(env.unrel(row["take"]))
    on = row["onset"]
    ph = [tuple(p) for p in row["phones"]]
    ons = mouthm.closure_onsets(y[on:], ph)
    return mouthm.check_closures(row["mouth"], row["mouth_from"], ons)


def mouth(ep, lid, log=print):
    d = env.ep_dir(ep)
    r = lock.read(d)["lines"][lid]
    log(f"{lid}: {r['text']!r} ({r['kind']}, {r['frames']} f, mouth from frame {r['mouth_from']:+d})")
    m = r["mouth"]
    ruler = "".join(str((i + r["mouth_from"]) % 10) if (i + r["mouth_from"]) >= 0 else "-" for i in range(len(m)))
    log("  frame " + ruler)
    log("  mouth " + m)
    for w, a, b in r["words"]:
        log(f"  word {w!r:16s} f {a / env.SPF:5.1f} - {b / env.SPF:5.1f}")
    if r.get("mouth"):
        for f, ok in closures(r):
            log(f"  closure at frame {f:3d}: {'M ok' if ok else 'NO M within 1 f'}")
    return 0
