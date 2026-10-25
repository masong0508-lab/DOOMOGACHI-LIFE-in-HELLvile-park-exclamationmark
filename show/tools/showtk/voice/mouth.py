"""Mouth tracks: one viseme character per frame (spec §9.3).

Visemes: X rest, M closed (m b p), C slightly open (consonants, teeth), E open (e, i,
schwa), A wide (a, ai, ae, uh, the onset of ay), O round, U pucker (u, w), F teeth on lip (f, v).

Track contract (voice.lock): character i is shown on frame say + mouth_from + i, where the
`say` frame is the frame whose first sample is the take's onset and mouth_from = -lead
(-1: the mouth leads the sound by one frame). Rules (series/visemes.shw `rules`):
  - each frame shows the phone sounding at the centre of the frame `lead` later;
  - stress/length marks take the next phone's viseme; h takes the next vowel's;
  - every mouth holds >= `hold` frames, except an M closure, which shows >= 1 frame on
    every bilabial even when that breaks the hold;
  - silence closes the mouth (X) only when it lasts >= `gap` frames;
  - frames where the take is silent (below `gate_db` of its peak) are X.
"""
from __future__ import annotations

import numpy as np

from . import env

VIS = {}
for _c in "pbm":
    VIS[_c] = "M"
for _c in "fv":
    VIS[_c] = "F"
for _c in "uʊw":
    VIS[_c] = "U"
for _c in "oɔɒ":
    VIS[_c] = "O"
for _c in "aɑæɐʌ":
    VIS[_c] = "A"
for _c in "eɛəɚɜᵊɪiᵻɨ":
    VIS[_c] = "E"
DIPH = {"A": ("A", "E"), "I": ("A", "E"), "O": ("O", "U"), "W": ("A", "U"), "Y": ("O", "E")}
MARKS = set("ˈˌːʰʲ")
SILENT = set("^$ ,.!?;:—…\"()“”")
BILABIAL = set("pbm")

DEFAULT_RULES = {"lead": 1, "hold": 2, "gap": 3, "gate_db": -38.0}


def rules():
    from . import shwio
    r, _ = shwio.read_visemes(shwio.series_path("visemes.shw"))
    out = dict(DEFAULT_RULES)
    for k, v in r.items():
        if k in out:
            out[k] = type(DEFAULT_RULES[k])(v)
    return out


def phone_visemes(phones):
    """[(ch, t0, t1)] -> [(viseme, t0, t1, is_bilabial)] with marks, h and diphthongs resolved."""
    out = []
    n = len(phones)
    for i, (ch, t0, t1) in enumerate(phones):
        if ch in SILENT:
            out.append(("X", t0, t1, False))
            continue
        if ch in MARKS or ch == "h":
            j = i + 1
            while j < n and (phones[j][0] in MARKS or phones[j][0] == "h"):
                j += 1
            nxt = phones[j][0] if j < n else " "
            v = DIPH[nxt][0] if nxt in DIPH else VIS.get(nxt, "X" if nxt in SILENT else "C")
            if ch == "h" and v in ("C", "M", "F"):
                v = "C"
            out.append((v, t0, t1, False))
            continue
        if ch in DIPH:
            a, b = DIPH[ch]
            mid = t0 + int(0.6 * (t1 - t0))
            out.append((a, t0, mid, False))
            out.append((b, mid, t1, False))
            continue
        out.append((VIS.get(ch, "C"), t0, t1, ch in BILABIAL))
    return out


def frame_db(y, n_frames, spf=env.SPF):
    """Per-frame level in dB re the take's loudest frame (y starts at the onset)."""
    y = np.asarray(y, dtype=np.float64)
    e = np.full(n_frames, -120.0)
    lv = []
    for k in range(n_frames):
        seg = y[k * spf:(k + 1) * spf]
        lv.append(np.sqrt(np.mean(seg ** 2)) if len(seg) else 0.0)
    lv = np.array(lv)
    pk = lv.max() if len(lv) else 1.0
    e[:] = 20 * np.log10(np.maximum(lv, 1e-9) / max(pk, 1e-9))
    return e


def track(phones, y_from_onset, r=None):
    """phones: [(ch, t0, t1)] in 48 kHz samples relative to the onset. y_from_onset: the
    take audio starting at the onset. -> (mouth string, mouth_from)."""
    r = r or rules()
    lead, hold, gap, gate = int(r["lead"]), int(r["hold"]), int(r["gap"]), float(r["gate_db"])
    spf = env.SPF
    pv = phone_visemes(phones)
    end = max([t1 for _, _, t1, _ in pv] + [len(y_from_onset)])
    nsound = int(np.ceil(len(y_from_onset) / spf)) + 1
    db = frame_db(y_from_onset, nsound)
    nf = nsound + lead + 1
    s = []
    for i in range(nf):
        k = i - lead                       # mouth frame relative to the say frame
        t = (k + lead) * spf + spf // 2    # the sound it shows: the centre of frame k + lead
        sf = k + lead
        if sf < 0 or sf >= nsound or db[sf] < gate:
            s.append("X")
            continue
        v = "X"
        for vv, t0, t1, _ in pv:
            if t0 <= t < t1:
                v = vv
                break
        if t >= end:
            v = "X"
        s.append(v)
    # an M on every bilabial (>= 1 frame), placed where its closure is, lead applied
    must = set()
    for vv, t0, t1, bil in pv:
        if not bil:
            continue
        frames = [i for i in range(nf) if t0 <= (i * spf + spf // 2) < t1]
        if not frames:
            c = (t0 + t1) / 2.0
            frames = [int(round(c / spf - 0.5))]
        i = min(max(frames[0], 0), nf - 1)
        if not any(s[j] == "M" for j in frames if 0 <= j < nf):
            s[i] = "M"
        must.add(i if s[i] == "M" else next(j for j in frames if 0 <= j < nf and s[j] == "M"))
    s = _close_gaps(s, gap)
    s = _min_hold(s, hold, must)
    # never end open
    while s and s[-1] != "X":
        s.append("X")
    return "".join(s), -lead


def _runs(s):
    out, i = [], 0
    while i < len(s):
        j = i
        while j < len(s) and s[j] == s[i]:
            j += 1
        out.append([s[i], i, j])
        i = j
    return out


def _close_gaps(s, gap):
    """Interior X runs shorter than `gap` frames keep the mouth from the frame before."""
    s = list(s)
    first = next((i for i, c in enumerate(s) if c != "X"), None)
    last = max((i for i, c in enumerate(s) if c != "X"), default=None)
    if first is None:
        return s
    for v, a, b in _runs(s):
        if v == "X" and a > first and b <= last and b - a < gap:
            prev = s[a - 1]
            fill = "C" if prev == "M" else prev
            for i in range(a, b):
                s[i] = fill
    return s


def _min_hold(s, hold, must):
    s = list(s)
    for _ in range(8):
        changed = False
        rr = _runs(s)
        for ri, (v, a, b) in enumerate(rr):
            if b - a >= hold:
                continue
            if v == "M" and any(a <= m < b for m in must):
                continue
            if v == "X" and (ri == 0 or ri == len(rr) - 1):
                continue
            prev = rr[ri - 1][0] if ri > 0 else None
            nxt = rr[ri + 1][0] if ri + 1 < len(rr) else None
            cand = prev if prev not in (None, "M") else nxt
            if cand in (None, "M"):
                cand = prev if prev is not None else nxt
            if cand is None or cand == v:
                continue
            for i in range(a, b):
                s[i] = cand
            changed = True
            break
        if not changed:
            break
    return s


def bilabials(phones):
    """[(t0, t1)] of every m/b/p phone (48 kHz samples relative to the onset)."""
    return [(t0, t1) for ch, t0, t1 in phones if ch in BILABIAL]


def closure_onsets(y_from_onset, phones, sr=env.SR, search=0.06):
    """AT9.2, measured from the audio itself: for each bilabial, the start of its closure
    (the 1-6 kHz band energy dip) within +/-60 ms of the phone time. -> [samples]."""
    from scipy.signal import butter, sosfiltfilt
    y = np.asarray(y_from_onset, dtype=np.float64)
    sos = butter(4, [1000, 6000], btype="band", fs=sr, output="sos")
    b = sosfiltfilt(sos, y) if len(y) > 64 else y
    hop = sr // 400            # 2.5 ms
    win = 2 * hop
    n = max(1, (len(b) - win) // hop)
    e = np.array([np.sqrt(np.mean(b[i * hop:i * hop + win] ** 2) + 1e-20) for i in range(n)])
    edb = 20 * np.log10(e / (e.max() + 1e-20))
    out = []
    for t0, t1 in bilabials(phones):
        c = (t0 + t1) / 2
        lo = int(max(0, (min(t0, c) - search * sr) / hop))
        hi = int(min(n, (max(t1, c) + search * sr) / hop))
        if hi - lo < 2:
            out.append(int(t0))
            continue
        seg = edb[lo:hi]
        mi = int(np.argmin(seg))
        floor = seg[mi]
        a = mi
        while a > 0 and seg[a - 1] < floor + 6.0:
            a -= 1
        out.append(int((lo + a) * hop))
    return out


def check_closures(mouth, mouth_from, onsets, tol=1, spf=env.SPF):
    """-> [(onset_frame, ok)] : an M within +/-tol frames of each closure onset's frame
    (onset frame = the frame the closure starts in, relative to the say frame)."""
    res = []
    for t in onsets:
        f = int(t // spf)
        ok = False
        for k in range(f - tol, f + tol + 1):
            i = k - mouth_from
            if 0 <= i < len(mouth) and mouth[i] == "M":
                ok = True
                break
        res.append((f, ok))
    return res
