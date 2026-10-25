"""Voice casting (spec §10.3).

  audition(char, text)   the role's voice and five neighbours (pitch -2/+2 st, speed
                         -/+8 %, and a 50/50 blend with its option 2), each with a
                         Whisper read-back, as numbered MP3s plus a text summary
  casting(roles)         for every main role: its 3 numbered `audition` options from
                         voices.shw, each speaking the voice bible's sample reads, as MP3s
                         in show_out/voices/<ROLE>/, and CASTING.md marking option 1 (the
                         voices.shw default)
"""
from __future__ import annotations

import os
import subprocess

import numpy as np

from . import asr, env, kokoro, shwio, takes

MAIN = ["mayor", "nana", "pip", "fuzz", "bonejangles", "clinker", "bloom", "steamy", "kevin", "hattie",
        "lulla", "gurgle", "grumbleguts", "kids_walla"]
GAP = int(0.6 * env.SR)


def out_root():
    for s in env.scratchpads():
        return os.path.join(s, "show_out", "voices")
    return os.path.join(env.BUILD, "show_out", "voices")


SHAPE_KEYS = ("kokoro", "pool", "speed", "pitch", "formant", "flatten_f0", "contour", "contour_amt", "breath",
              "wavy", "double")


def _voice(vid, settings, base=None, inherit=True):
    """A Voice from `settings` on top of `base`. inherit=False (a casting option): the
    option's row is complete, so none of the base's voice or shaping keys carry over."""
    a = dict(base.attrs) if base else {}
    if not inherit:
        for k in SHAPE_KEYS:
            a.pop(k, None)
    if "kokoro" in settings or "pool" in settings:
        a.pop("kokoro", None)
        a.pop("pool", None)
    a.update(settings)
    return shwio.Voice(vid, a)


def speak(text, vid, voice, line_id):
    """Render one text through the full take pipeline (no storing)."""
    ln = shwio.Line(line_id, vid, text, {})
    p = takes.tts_params(ln, vid, voice)
    tk = takes.render_tts(ln, p)
    return tk.y, p


def _f0_med(y):
    import pyworld as pw
    x = np.asarray(y, np.float64)
    f0, _ = pw.dio(x, env.SR, f0_floor=55, f0_ceil=800, frame_period=10)
    v = f0[f0 > 0]
    return float(np.median(v)) if len(v) else 0.0


def mp3(y, path, title=""):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pcm = (np.clip(np.asarray(y, np.float64), -1, 1) * 32767).astype("<i2").tobytes()
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-f", "s16le", "-ar", str(env.SR), "-ac", "1", "-i", "-",
           "-c:a", "libmp3lame", "-b:a", "160k", "-metadata", f"title={title}", "-metadata",
           "artist=DOOMOGACHI LIFE temp TTS (Kokoro)", path]
    subprocess.run(cmd, input=pcm, check=True)
    return path


def _levelled(y, target=-16.0):
    """Auditions are for listening: level them to the web target so options compare fairly."""
    g = target - takes.lufs(y)
    y = np.asarray(y, np.float64) * 10 ** (g / 20)
    pk = np.abs(y).max()
    return y * (0.89 / pk) if pk > 0.89 else y


def render_option(role, n, settings, samples, base):
    v = _voice(role, settings, base, inherit=False)
    parts, heard, wers, params = [], [], [], None
    words = 0
    for i, s in enumerate(samples):
        y, p = speak(s, role, v, f"aud_{role}_{n}_{i}")
        params = params or p
        parts.append(y)
        parts.append(np.zeros(GAP, np.float32))
        if v.attrs.get("babble") not in ("1", "on", "yes"):
            r = asr.check(y, p["say"])
            heard.append(r["heard"])
            wers.append(r["wer"])
        words += len(asr.norm_words(s))
    y = np.concatenate(parts[:-1])
    speech_s = len(y) / env.SR - 0.06 * len(samples)
    return y, {"f0": round(_f0_med(y), 1), "wps": round(words / max(speech_s, 0.1), 2),
               "wer": round(float(np.mean(wers)), 2) if wers else None, "heard": heard,
               "settings": settings, "params": params}


def casting(roles=None, out=None, log=print):
    voices, cast = takes.load_cast()
    opts, samples = shwio.read_auditions(shwio.series_path("voices.shw"))
    out = out or out_root()
    roles = roles or MAIN
    rows = []
    for role in roles:
        base = voices[role]
        smp = samples.get(role) or ["Hello."]
        for n in sorted(opts.get(role, {1: {}})):
            st = opts[role][n]
            y, info = render_option(role, n, st, smp, base)
            desc = st.get("kokoro") or st.get("pool", "pool").split(",")[0]
            desc = desc.replace(":", "").replace("+", "_").replace(".", "")
            path = os.path.join(out, role.upper(), f"{role}_{n}_{desc}.mp3")
            mp3(_levelled(y), path, f"{role} option {n}")
            info.update(role=role, n=n, path=path, default=(n == 1))
            rows.append(info)
            log(f"{role:12s} {n} {'DEFAULT' if n == 1 else '       '} f0 {info['f0']:6.1f} Hz  "
                f"{info['wps']:.2f} w/s  wer {info['wer']}  {os.path.basename(path)}")
    _casting_md(rows, voices, samples, out)
    return rows


def _casting_md(rows, voices, samples, out):
    L = ["# DOOMOGACHI LIFE ep101: voice casting (temp TTS)", "",
         "Every speaking role, cast from the stock Kokoro-82M voices (Apache-2.0) and shaped with WORLD "
         "(pitch, formant, a flattened or laid-on melody). Each main role has **3 numbered options**; "
         "**option 1 is the default** and is what `show/series/voices.shw` uses now. Every option speaks the "
         "voice bible's two sample reads (ep101 BREAKDOWN §3.2). All files are levelled to -16 LUFS so they "
         "compare fairly.", "",
         "**To pick another option:** reply with the role and the number (for example \"Nana 2, Pip 3\"). "
         "The option's settings are its `audition` row in `show/series/voices.shw`; Claude copies them into the "
         "`voice` row and re-renders only that role's lines (`show voice ep101`).", "",
         "**To use your own voice for a role:** record the lines into `show/ep101/vo/` as "
         "`<line id>_take<n>.wav` (see `show/ep101/vo/README.md`); a recording beats TTS line by line.", "",
         "Columns: median pitch (Hz), pace (words a second, including pauses), Whisper word error rate on the "
         "sample reads (0 = every word heard; QA only).", ""]
    by = {}
    for r in rows:
        by.setdefault(r["role"], []).append(r)
    for role, rs in by.items():
        v = voices[role]
        L.append(f"## {v.attrs.get('role', role)} (`{role}`)")
        L.append("")
        L.append(f"Brief: {v.attrs.get('brief', '')}. Samples: " + " / ".join(f"\"{s}\"" for s in samples.get(role, [])))
        L.append("")
        L.append("| # | File | Voice | Settings | Pitch | Pace | WER | Whisper heard |")
        L.append("|---|---|---|---|---|---|---|---|")
        for r in rs:
            st = dict(r["settings"])
            vv = st.pop("kokoro", st.pop("pool", ""))
            sts = " ".join(f"{k}={x}" for k, x in st.items())
            heard = " / ".join(r["heard"]) if r["heard"] else "(babble: not scored)"
            mark = "**1 (default)**" if r["default"] else str(r["n"])
            L.append(f"| {mark} | `{role.upper()}/{os.path.basename(r['path'])}` | {vv} | {sts} | "
                     f"{r['f0']:.0f} Hz | {r['wps']:.2f} | {r['wer'] if r['wer'] is not None else '-'} | {heard} |")
        L.append("")
    L += ["## Roles without auditions", "",
          "| Role | Cast | Why |", "|---|---|---|",
          "| The pet | `lib:pet/beep_v1` | The game's raw piezo \"beep.\", a sound effect, not a performance (`process=none`) |",
          "| Walla: the Giggle crowd, Hattie's audience, the Wall laugh, the fog stack | pools in `voices.shw` (`giggle`, `tent`, `wall`, `fog`) | Mixed low and stacked; each line draws a voice from its pool |",
          "| Miss Lulla's kids | `lulla_kids` pool, pitched up | Children voiced by adult voices; background walla only |",
          "| The fog's child \"...us\" | `fog_child` = `af_kore` +6 st | Must never be Pip's voice (`af_sky`) |",
          "| AD narrator | `bm_fable` | Audio description is deferred (spec §1.2) |", "",
          "Sunny and Mascotty are silent. Every TTS line is listed by `show voice report ep101 --require human-voice`.", ""]
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "CASTING.md"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(L))


def audition(char, text, out=None, n=6, log=print):
    voices, cast = takes.load_cast()
    vid = char if char in voices else cast.get(char, char)
    base = voices[vid]
    opts, _ = shwio.read_auditions(shwio.series_path("voices.shw"))
    a = base.attrs
    pitch = float(str(a.get("pitch", "0")).replace("st", "").split("..")[0])
    speed = float(str(a.get("speed", "1")).split("..")[0])
    variants = [("as cast", {})]
    variants.append(("pitch -2 st", {"pitch": f"{pitch - 2:+g}st"}))
    variants.append(("pitch +2 st", {"pitch": f"{pitch + 2:+g}st"}))
    variants.append(("8% slower", {"speed": f"{speed * 0.92:.2f}"}))
    variants.append(("8% faster", {"speed": f"{speed * 1.08:.2f}"}))
    o2 = opts.get(vid, {}).get(2, {})
    if "kokoro" in a and "kokoro" in o2:
        b1 = kokoro.parse_blend(a["kokoro"])
        b2 = kokoro.parse_blend(o2["kokoro"])
        b1 = b1 if isinstance(b1, dict) else {b1: 1.0}
        b2 = b2 if isinstance(b2, dict) else {b2: 1.0}
        mix = {}
        for k, w in b1.items():
            mix[k] = mix.get(k, 0) + 0.5 * w / sum(b1.values())
        for k, w in b2.items():
            mix[k] = mix.get(k, 0) + 0.5 * w / sum(b2.values())
        variants.append(("50/50 with option 2", {"kokoro": kokoro.blend_str(mix)}))
    else:
        variants.append(("pitch +4 st", {"pitch": f"{pitch + 4:+g}st"}))
    out = out or os.path.join(out_root(), "audition", vid)
    lines = [f"audition {vid}: {text!r}"]
    for i, (name, st) in enumerate(variants[:n], 1):
        v = _voice(vid, st, base)
        y, p = speak(text, vid, v, f"aud_{vid}_{i}")
        r = asr.check(y, p["say"]) if a.get("babble") not in ("1", "on", "yes") else {"heard": "-", "wer": None}
        path = mp3(_levelled(y), os.path.join(out, f"{i}_{vid}_{name.replace(' ', '_').replace('/', '-')}.mp3"))
        msg = f"  {i}. {name:20s} {p['kokoro']} speed={p['speed']} pitch={p['shape']['pitch']:+g}st  " \
              f"f0 {_f0_med(y):5.1f} Hz  wer {r['wer']}  heard {r['heard']!r}  {path}"
        lines.append(msg)
        log(msg)
    with open(os.path.join(out, "audition.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    return out
