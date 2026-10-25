"""showtk.master - the stereo `web` master and its QA (WP11; SHOW_ENGINE_SPEC.md sec.10.6).

One mix, re-mastered per target by gain and limiter only (no EQ, no glue: the mix is the
mix). `web` = -16 LUFS integrated (pyloudnorm, BS.1770) +/-0.5, true peak <= -1 dBTP
(4x oversampled), through the kit's look-ahead true-peak limiter at -1.3 dBTP. The
r128 / a85 / dlg27 targets are deferred (show/DEFERRED.md).

    y, report, gain_db = web(x)
    qa(y, stems, gain_db, timeline, n) -> clipped samples, clicks, stem loudness,
                                          masking pairs, the unscored share per act
"""
from __future__ import annotations

import numpy as np

from .music.kitenv import kit  # noqa: F401  (puts show/audio on sys.path)
import importlib  # noqa: E402
KM = importlib.import_module("kit.master")   # (kit/__init__ re-exports a function `master`)
KQ = importlib.import_module("kit.qa")

SR, SPF = 48000, 1600
TARGETS = {"web": (-16.0, 0.5, -1.0)}       # integrated LUFS, tolerance, dBTP ceiling
CEILING = -1.3                               # limiter ceiling: 0.3 dB under the TP spec


def lufs(x):
    try:
        v = KM.lufs(x)
    except ValueError:
        return float("-inf")
    return float(v) if np.isfinite(v) else float("-inf")


def web(x, target="web"):
    """(y, report, gain_db): gain to the target, then the true-peak limiter; the gain is
    re-trimmed after limiting until the integrated loudness is inside the window."""
    tgt, tol, tp_max = TARGETS[target]
    x = np.asarray(x, dtype=np.float64)
    l0 = lufs(x)
    if not np.isfinite(l0):
        return x, {"target": target, "lufs": l0, "tp": float("-inf"), "gain_db": 0.0, "ok": False,
                   "why": "silent mix"}, 0.0
    gain = tgt - l0
    y = x
    for _ in range(6):
        y = KM.limiter(x * 10 ** (gain / 20.0), ceiling_db=CEILING, lookahead_ms=1.5, release_ms=80.0)
        l1 = lufs(y)
        if abs(l1 - tgt) <= 0.1:
            break
        gain += tgt - l1
    y = np.clip(y, -1.0, 1.0 - 2 ** -23)
    tp = KM.true_peak(y)
    l1 = lufs(y)
    ok = abs(l1 - tgt) <= tol and tp <= tp_max
    return y, {"target": target, "lufs": round(l1, 2), "tp": round(tp, 2), "gain_db": round(gain, 2),
               "input_lufs": round(l0, 2), "ok": bool(ok), "window": [tgt - tol, tgt + tol],
               "tp_max": tp_max}, gain


def clicks(y, thresh=0.1, sigma=6.0, positions=False):
    """Discontinuities: second differences far outside the local (10 ms) spread of the
    signal's own second differences, so dense noise (crayon, crowd) is not a click."""
    m = np.asarray(y).mean(axis=1) if np.ndim(y) == 2 else np.asarray(y)
    d2 = np.abs(np.diff(m, 2))
    if not len(d2):
        return 0
    w = 480
    rms = np.sqrt(np.convolve(d2 ** 2, np.ones(w) / w, mode="same"))
    hit = (d2 > thresh) & (d2 > sigma * rms)
    starts = np.nonzero(hit[1:] & ~hit[:-1])[0] + 1
    return starts if positions else int(len(starts))


def unscored(mx, timeline, n, thresh_db=-50.0):
    """Share of 0.1 s windows where the MX stem is under thresh (sec.10.6 silence budget)."""
    w = SR // 10
    seg = mx[: n // w * w]
    if not len(seg):
        return 0.0, []
    r = np.sqrt(np.mean(seg.reshape(-1, w, seg.shape[1]) ** 2, axis=(1, 2)) + 1e-20)
    q = 20 * np.log10(r) < thresh_db
    rows = []
    for a in timeline.get("acts", []):
        i0, i1 = a["start"] * SPF // w, (a["start"] + a["len"]) * SPF // w
        if i0 < len(q):
            part = q[i0: min(i1, len(q))]
            rows.append({"act": a["id"], "unscored_pct": round(float(part.mean()) * 100, 1),
                         "covered_s": round(len(part) / 10, 1)})
    return round(float(q.mean()) * 100, 1), rows


def qa(y, stems, gain_db, timeline, n, spans=()):
    """spans: [(smp0, smp1, id, has_own_edges)] of the one-shots placed; a discontinuity
    inside a sound whose library file has the same edges (the game's square-wave SynthSeg
    thuds) is the sound itself, counted as `source_clicks`, not a mix fault."""
    g = 10 ** (gain_db / 20.0)
    st = {}
    for b, x in stems.items():
        xs = x * g
        st[b] = {"lufs": round(lufs(xs), 1) if np.any(xs) else None,
                 "peak_db": round(float(20 * np.log10(max(np.max(np.abs(xs)), 1e-12))), 1)}
    for b in st:
        if st[b]["lufs"] is None or not np.isfinite(st[b]["lufs"]):
            st[b]["lufs"] = float("nan")
    try:
        masking = KQ.masking_report({b: stems[b] for b in stems if np.any(stems[b])}, top=5)
    except Exception as e:  # noqa: BLE001  (QA never blocks the mix)
        masking = [{"error": repr(e)}]
    un, acts = unscored(stems["MX"], timeline, n)
    pos = clicks(y, positions=True)
    own = [p for p in pos if any(a <= p < b and edgy for a, b, _i, edgy in spans)]
    return {"clipped": int(np.sum(np.abs(y) >= 1.0 - 2 ** -22)), "clicks": int(len(pos) - len(own)),
            "source_clicks": len(own), "click_frames": sorted({int(p) // SPF for p in pos})[:20], "stems": st,
            "masking": masking, "unscored_pct": un, "acts": acts}
