"""showtk.captions - SRT and WebVTT from the line timings and the cc events (WP12).

Sources, in order (SHOW_ENGINE_SPEC.md sec.12.3 "Captions"):
  1. build/<ep>/captions.json when the compiler wrote it (frame-exact in/out);
  2. otherwise: every dialogue line of timeline.json (its start/end frames, `sdh=`
     overriding the text) plus every audio event that carries `cc` (the AUDIO
     lane's SDH captions: walla beds, SFX captions such as "[crayon scratching]").
Walla lines are never captioned one by one (the bed's `cc` speaks for them).

Rules: the game's inline text codes ({d} {w} {big}..{/} {..}) are stripped;
42 characters a row, two rows a cue (a longer line is split by words over its
time); a cue shows at least 20 f unless the next one starts; voice-only and
off-screen speakers get their name in capitals; a cue moves to the top while a
SUPER or sign sits in the bottom third ({\\an8} in SRT, line:10% in VTT).
"""
import re
from pathlib import Path

from .sidecars import FPS, SPF, load_audio_events, load_captions_json

MAX_ROW = 42
MIN_FRAMES = 20
CODE = re.compile(r"\{[^{}]*\}")


def clean(text):
    t = CODE.sub("", str(text or ""))
    t = t.replace("\\n", " ")
    return re.sub(r"\s+", " ", t).strip()


def wrap(text, width=MAX_ROW):
    rows, cur = [], ""
    for w in text.split():
        if cur and len(cur) + 1 + len(w) > width:
            rows.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    if cur:
        rows.append(cur)
    return rows


class Cue:
    def __init__(self, f0, f1, text, kind="dx", top=False, ident=""):
        self.f0, self.f1, self.text, self.kind, self.top, self.id = f0, f1, text, kind, top, ident

    def __repr__(self):
        return "Cue(%d-%d %r)" % (self.f0, self.f1, self.text)


def _split(f0, f1, text):
    """A line longer than two rows -> several cues, time shared by characters."""
    rows = wrap(text)
    if len(rows) <= 2:
        return [(f0, f1, "\n".join(rows))]
    chunks = ["\n".join(rows[i:i + 2]) for i in range(0, len(rows), 2)]
    total = sum(len(c) for c in chunks)
    out, a = [], f0
    for i, c in enumerate(chunks):
        b = f1 if i == len(chunks) - 1 else a + max(1, round((f1 - f0) * len(c) / total))
        out.append((a, b, c))
        a = b
    return out


def _lower_third_spans(tl):
    """Frames where a SUPER or sign sits in the bottom third of the picture."""
    spans = []
    third = tl.canvas["h"] * 2 // 3
    for s in tl.strings:
        at = str(s.get("at", ""))
        y = s.get("y")
        low = at in ("lower", "bottom") or (isinstance(y, (int, float)) and y >= third)
        a, b = s.get("start", s.get("f0")), s.get("end", s.get("f1"))
        if low and a is not None and b is not None:
            spans.append((int(a), int(b)))
    for sh in tl.shots:
        for e in sh.events:
            if e.op in ("super", "sign", "card"):
                args = e.args if isinstance(e.args, str) else " ".join(
                    "%s=%s" % kv for kv in dict(e.args or {}).items())
                if "at=lower" in args or "at=bottom" in args:
                    a, b = e.span if e.span else (e.f, sh.end)
                    spans.append((a, b))
    return spans


def build_cues(tl, f0=0, f1=None, audio_events=None, captions_json=None):
    """Cues in episode frames, clipped to [f0, f1)."""
    f1 = tl.length if f1 is None else f1
    cues = []
    if captions_json:
        for c in captions_json:
            a = int(c.get("in", c.get("f0", c.get("start", 0))))
            b = int(c.get("out", c.get("f1", c.get("end", a + 30))))
            txt = clean(c.get("text", ""))
            if txt:
                for a2, b2, t2 in _split(a, b, txt):
                    cues.append(Cue(a2, b2, t2, c.get("kind", "dx"), bool(c.get("top")),
                                    c.get("id", "")))
    else:
        for ln in tl.lines:
            if ln.walla or ln.start is None:
                continue
            txt = clean(ln.sdh if ln.sdh else ln.text)
            if not txt:
                continue
            ent = tl.cast_entry(ln.who)
            tags = set(str(ent.get("tags", "")).replace(",", " ").split()) if isinstance(
                ent.get("tags", ""), str) else set(ent.get("tags", []))
            names = ln.tag_names()
            if ("VOICE_ONLY" in tags or "offscreen" in names or "vo" in names) and not ln.sdh:
                name = ent.get("name") or ln.who
                txt = "%s: %s" % (clean(name).upper(), txt)
            for a, b, t in _split(ln.start, ln.end, txt):
                cues.append(Cue(a, b, t, "dx", ident=ln.id))
        evs = audio_events if audio_events is not None else []
        for i, e in enumerate(evs):
            cc = e.get("cc")
            if not cc:
                continue
            a = int(e.get("f", int(e.get("smp", 0)) // SPF))
            if "cc_len" in e:
                b = a + int(e["cc_len"])
            else:
                b = a + 3 * FPS
                for e2 in evs[i + 1:]:      # the bed's own off/stop ends it sooner
                    if e2.get("src") == e.get("src") and e2.get("op") in ("off", "stop"):
                        b = min(b, int(e2.get("f", int(e2.get("smp", 0)) // SPF)))
                        break
            txt = clean(cc)
            cues.append(Cue(a, b, txt if txt.startswith("[") else "[%s]" % txt.strip("[]"),
                            "sdh", ident="k%s" % e.get("k", i)))
    # clip, order, minimum length, placement
    out = []
    for c in sorted(cues, key=lambda c: (c.f0, c.kind != "sdh")):
        a, b = max(c.f0, f0), min(c.f1, f1)
        if b <= a:
            continue
        c.f0, c.f1 = a, b
        out.append(c)
    for i, c in enumerate(out):
        if c.f1 - c.f0 < MIN_FRAMES:
            nxt = min([d.f0 for d in out[i + 1:] if d.f0 > c.f0 and d.kind == c.kind] + [f1])
            c.f1 = max(c.f1, min(c.f0 + MIN_FRAMES, nxt))
    spans = _lower_third_spans(tl)
    for c in out:
        if any(a < c.f1 and b > c.f0 for a, b in spans):
            c.top = True
    return out


def _ts(frame, sep):
    ms = int(round(frame * 1000.0 / FPS))
    return "%02d:%02d:%02d%s%03d" % (ms // 3600000, ms // 60000 % 60, ms // 1000 % 60, sep, ms % 1000)


def srt(cues, f0=0):
    out = []
    for i, c in enumerate(cues, 1):
        text = ("{\\an8}" if c.top else "") + c.text
        out.append("%d\n%s --> %s\n%s\n" % (i, _ts(c.f0 - f0, ","), _ts(c.f1 - f0, ","), text))
    return "\n".join(out)


def vtt(cues, f0=0, title=""):
    out = ["WEBVTT" + (" - " + title if title else ""), ""]
    for i, c in enumerate(cues, 1):
        setting = " line:10%" if c.top else ""
        out.append("%s\n%s --> %s%s\n%s\n" % (c.id or i, _ts(c.f0 - f0, "."), _ts(c.f1 - f0, "."),
                                              setting, c.text.replace("-->", "->")))
    return "\n".join(out)


def write(tl, out_base, f0=0, f1=None, ep=None):
    """Writes <out_base>.srt and .vtt; returns (srt_path, vtt_path, cues)."""
    ep = ep or tl.episode
    evs = load_audio_events(ep)
    cj = load_captions_json(ep)
    cues = build_cues(tl, f0, f1, audio_events=evs, captions_json=cj)
    sp, vp = Path(str(out_base) + ".srt"), Path(str(out_base) + ".vtt")
    sp.write_text(srt(cues, f0), encoding="utf-8")
    vp.write_text(vtt(cues, f0, tl.title), encoding="utf-8")
    return sp, vp, cues


def parse_srt(text):
    """[(start_ms, end_ms, text)] (used by the tests and the QC check)."""
    out = []
    for block in re.split(r"\n\s*\n", text.strip()):
        rows = block.strip().splitlines()
        if len(rows) < 3:
            continue
        m = re.match(r"(\d+):(\d+):(\d+),(\d+) --> (\d+):(\d+):(\d+),(\d+)", rows[1])
        g = [int(x) for x in m.groups()]
        a = ((g[0] * 60 + g[1]) * 60 + g[2]) * 1000 + g[3]
        b = ((g[4] * 60 + g[5]) * 60 + g[6]) * 1000 + g[7]
        out.append((a, b, "\n".join(rows[2:])))
    return out
