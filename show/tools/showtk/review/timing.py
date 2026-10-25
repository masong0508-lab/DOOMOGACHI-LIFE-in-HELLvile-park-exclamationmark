"""showtk.review.timing - `show timing <shot|range>`: the event-to-frame report (WP13, AT13.1).

For every event of every shot in the range it prints the scripted frame, the
picture change measured on the rendered master (a frame-diff spike above the
running median, so drifts and walks do not count), the audio onset measured on
the mix (when there is one) against the event's sample, the ladder rung of each
gap, and each line's mouth track.  `--static` skips the masters and the mix.

    ep101 co02 [00:00:03:00 f90..269, 180 f] set=park_storybook px=1
      +0    f90   iris_in  at=prop:bandstand over=13      picture f90 ok; ends f103 ok      co.shw:15
      +0    f90   sfx      FX_SHWIP (k3 smp 144000)        audio onset smp 144012 (+0.3 ms) ok
      +30   f120  super    s_co02 ...                       52/52 glyph frames f120..f222 ok
      gaps: 0 R2 R3 ...
Every line ends in ok, OFF (with the measured frame) or ? (nothing measured).
"""
import wave
from pathlib import Path

import numpy as np

from ..sidecars import SPF, load_audio_events, rung, tc
from .pictures import Pictures, baseline, change_signal

FLOOR = 16          # changed pixels above the running median that count as a picture change
SEARCH = 3          # frames either side searched when the change is not on its frame
VISIBILITY_OPS = ("super", "sign", "card")
AUDIO_OP_NAMES = ("sfx", "fol", "amb", "mx", "walla", "say", "hum", "stop", "tapestop")


def _args_text(e):
    a = e.args
    if isinstance(a, dict):
        return " ".join("%s=%s" % kv for kv in a.items())
    return str(a or "")


class Measure:
    """The change signal of one shot, in shot frames.  A frame 'changes' when its
    changed-pixel count beats the quiet level around it (the 25th percentile of its
    neighbours: continuous drifts and walks are the floor, dense type-on glyphs are not);
    a span starts when it beats the frames before and ends when it beats the frames after."""

    def __init__(self, pics, shot, prev_last=None):
        fr = pics.shot_frames(shot.id)
        self.d = change_signal(fr, prev_last).astype(float)
        self.last = fr[-1]
        self.n = len(fr)

    def _q(self, lo, hi, skip=None):
        idx = [i for i in range(max(0, lo), min(self.n, hi)) if i != skip]
        return float(np.percentile(self.d[idx], 25)) if idx else 0.0

    def at(self, t):
        if not 0 <= t < self.n:
            return False
        if t == 0:
            return self.d[0] > FLOOR                       # the cut itself
        return self.d[t] - self._q(t - 7, t + 8, skip=t) > FLOOR

    def starts(self, t):
        if t <= 0:
            return self.d[0] > FLOOR
        return 0 <= t < self.n and self.d[t] - self._q(t - 8, t) > FLOOR

    def ends(self, t):
        if not 0 <= t < self.n:
            return False
        if t == self.n - 1:
            return self.d[t] > FLOOR
        return self.d[t] - self._q(t + 1, t + 9) > FLOOR

    def near(self, t, test=None):
        test = test or self.at
        for k in range(1, SEARCH + 1):
            for s in (t - k, t + k):
                if test(s):
                    return s
        return None


def audio_onset(mix, smp, rate=48000):
    """The first 1 ms block from 2 f before the event whose RMS is 3x the mean of the
    10 ms before it (and above -50 dBFS), refined to the first sample over half that
    block's peak.  Silence is assumed before the file starts."""
    if mix is None:
        return None
    pre = 30 * 48 + 2 * SPF
    a = smp - pre
    x = mix[max(0, a):smp + 6 * SPF]
    if a < 0:
        x = np.concatenate([np.zeros(-a), x])
    blk = 48
    nb = len(x) // blk
    if nb < 40:
        return None
    rms = np.sqrt((x[:nb * blk].reshape(nb, blk) ** 2).mean(axis=1) + 1e-12)
    # the same on the first difference: a transient over a loud tonal bed (a cue already
    # playing) shows there even when the plain level barely moves
    dx = np.diff(x, prepend=x[:1])
    hf = np.sqrt((dx[:nb * blk].reshape(nb, blk) ** 2).mean(axis=1) + 1e-12)
    for i in range(30, nb):
        raw_jump = rms[i] > 3 * rms[i - 10:i].mean()
        hf_jump = hf[i] > 2.5 * hf[i - 10:i].mean()
        if (raw_jump or hf_jump) and rms[i] > 10 ** (-50 / 20):
            sig, pre_lvl = (np.abs(x), rms[i - 10:i].mean()) if raw_jump else \
                (np.abs(dx), hf[i - 10:i].mean())
            seg = sig[(i - 1) * blk:(i + 1) * blk]
            thr = max(0.5 * sig[i * blk:(i + 1) * blk].max(), 4 * pre_lvl)
            j = int(np.argmax(seg > thr)) if (seg > thr).any() else blk
            return a + (i - 1) * blk + j
    return None


def load_mix(path):
    if path is None or not Path(path).exists():
        return None
    with wave.open(str(path), "rb") as w:
        n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(n)
    if sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = (b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16))
        v = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
    elif sw == 2:
        v = np.frombuffer(raw, "<i2") / 32768.0
    else:
        v = np.frombuffer(raw, "<i4") / float(1 << 31)
    return v.reshape(-1, ch).mean(axis=1)


def report(tl, rng=None, masters_dir=None, mix=None, static=False):
    """Returns (text, results) with one result dict per event."""
    f0, f1 = tl.range(rng)
    shots = tl.shots_in(f0, f1)
    pics = None if static else Pictures(tl, masters_dir)
    evs = load_audio_events(tl.episode)
    by_k = {e.get("k"): e for e in evs}
    mixv = None if static else load_mix(mix)
    out, results = [], []
    prev_last = None
    if pics is not None and shots and shots[0].start > 0:
        p = tl.shot_at(shots[0].start - 1)
        if p is not None and pics.has(p.id):
            prev_last = pics.shot_frames(p.id)[-1]
    for s in shots:
        m = None
        if pics is not None and pics.has(s.id):
            m = Measure(pics, s, prev_last)
            prev_last = m.last
        out.append("%s %s [%s f%d..%d, %d f] set=%s px=%d scene=%s%s" % (
            tl.episode, s.id, tc(s.start), s.start, s.end - 1, s.len, s.set, s.px, s.scene,
            "" if m is not None or static else "  (no master: static report)"))
        frames_seen = []
        for e in s.events:
            r = {"shot": s.id, "op": e.op, "f": e.f, "t": e.t, "src": e.src, "status": "info"}
            msg = ""
            if e.op in AUDIO_OP_NAMES or e.k is not None:
                ae = by_k.get(e.k) if e.k is not None else None
                smp = int(ae["smp"]) if ae else e.f * SPF
                r["smp"] = smp
                onset = audio_onset(mixv, smp) if mixv is not None else None
                if onset is not None:
                    ms = (onset - smp) / 48.0
                    r["onset"], r["onset_ms"] = onset, ms
                    r["status"] = "ok" if abs(ms) <= 1.0 + 1e-9 else "OFF"     # M1-T5: +-1 ms
                    msg = "k%s smp %d: audio onset smp %d (%+.1f ms) %s" % (
                        e.k, smp, onset, ms, r["status"])
                else:
                    msg = "k%s smp %d%s" % (e.k, smp, "" if mixv is None else ": no onset found ?")
                    r["status"] = "info" if mixv is None else "?"
            elif m is not None and e.visual:
                st = e.t
                glyphs = e.raw.get("glyphs")
                if glyphs:
                    hit = [g for g in glyphs if m.at(g - s.start)]
                    r["glyphs"] = (len(hit), len(glyphs))
                    r["status"] = "ok" if len(hit) == len(glyphs) else "OFF"
                    msg = "%d/%d glyph frames changed, f%d..f%d %s" % (
                        len(hit), len(glyphs), glyphs[0], glyphs[-1], r["status"])
                    if len(hit) != len(glyphs):
                        miss = [g for g in glyphs if g not in hit]
                        msg += " (no change on f%s)" % ",".join(str(x) for x in miss[:8])
                else:
                    parts, ok = [], True
                    test0 = m.starts if e.span else m.at
                    if test0(st):
                        parts.append("picture f%d ok" % e.f)
                        r["measured"] = e.f
                    else:
                        nb = m.near(st, test0)
                        ok = False
                        parts.append("picture %s OFF" % ("f%d (%+d)" % (s.start + nb, nb - st)
                                                         if nb is not None else "no change"))
                        r["measured"] = None if nb is None else s.start + nb
                    if e.span:
                        a, b = e.span
                        tb = b - s.start
                        if e.op in VISIBILITY_OPS:
                            tb += 1                       # on screen through b: gone on b+1
                        if tb < s.len:
                            if m.ends(tb):
                                parts.append("ends f%d ok" % (s.start + tb))
                                r["end_measured"] = s.start + tb
                            else:
                                nb = m.near(tb, m.ends)
                                ok = False
                                parts.append("ends %s OFF (want f%d)" % (
                                    "f%d" % (s.start + nb) if nb is not None else "with no change",
                                    s.start + tb))
                                r["end_measured"] = None if nb is None else s.start + nb
                    r["status"] = "ok" if ok else "OFF"
                    msg = "; ".join(parts)
            elif not e.visual:
                msg = "marker"
            frames_seen.append(e.f)
            r["msg"] = msg
            results.append(r)
            out.append("  +%-4d f%-6d %-9s %-44s %s   %s" % (
                e.t, e.f, e.op, _args_text(e)[:44], msg, e.src))
        fs = sorted(set(frames_seen))
        if len(fs) > 1:
            out.append("  gaps: " + " ".join("%d(%s)" % (b - a, rung(b - a)) for a, b in zip(fs, fs[1:])))
        for ln in tl.lines:
            if ln.shot == s.id and ln.mouth:
                out.append("  mouth %-14s %s" % (ln.id, ln.mouth))
    bad = [r for r in results if r["status"] in ("OFF", "?")]
    out.append("%s %s: %d events, %d ok, %d off/unmeasured" % (
        tl.episode, rng or "all", len(results), sum(r["status"] == "ok" for r in results), len(bad)))
    return "\n".join(out), results
