"""showtk.voice.ingest - recorded-take ingest + forced-alignment lip sync
(SHOW_ENGINE_SPEC.md sec.9.3 "Recorded takes", sec.10.2 step 6).

Minimal implementation by the Mayor-VO package; the AUDIO track may replace
it.  Formats it fixes:

  * take files   SHOW_LIB/takes/<char>/e101/<line_id>_t<n>.wav (48 kHz, 24-bit, mono)
  * mouth track  one viseme char per 30 fps frame (X M C E A O U F), frame f
                 covers samples [1600 f, 1600 (f+1)) of the take
  * .mouth file  show/ep101/mouth/<line_id>.mouth :
                     # line=<id> take=t<n> sha256=<take sha> frames=<n>
                     XXXCCEEEMM...
                     # phones: <word> <phone>@<frame> ...

Alignment: script text -> phones (CMU dict + show lexicon) ; Whisper word
times (faster-whisper word_timestamps) matched to the script words by edit
alignment, unmatched script words interpolated ; phones spread inside each
word by fixed duration weights (vowel 2, consonant 1) ; every bilabial snapped
to the energy dip within +-50 ms (lips closed = quiet) ; then the sec.9.3
rules: mouth leads the sound by 1 f, each mouth holds >= 2 f, an M shows
>= 1 f on every bilabial, word gaps close only when >= 3 f.

    python3 -m showtk.voice.ingest track take.wav "script text"   # print a track
"""
import os
import re
import sys

import numpy as np

FPS = 30
SR = 48000
SPF = SR // FPS          # 1600

CMUDICT = os.environ.get("CMUDICT") or (
    "/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/"
    "ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/pydeps/cmudict/data/cmudict.dict")

# names and coinages (series/lexicon.shw when the AUDIO track writes it)
LEXICON = {
    "gigglesworth": "G IH1 G AH0 L Z W ER0 TH",
    "giggs": "G IH1 G Z",
    "hellvile": "HH EH1 L V AY0 L",
    "hahaland": "HH AA1 HH AA0 L AE2 N D",
    "hee": "HH IY1",
    "mm": "M",
    "ooh": "UW1",
}

VIS = {}
for p in "AA AE AH AY AW".split():
    VIS[p] = "A"
for p in "AO OW OY".split():
    VIS[p] = "O"
for p in "UW UH W".split():
    VIS[p] = "U"
for p in "EH EY IH IY ER Y".split():
    VIS[p] = "E"
for p in "P B M".split():
    VIS[p] = "M"
for p in "F V".split():
    VIS[p] = "F"
VOWELS = set("AA AE AH AY AW AO OW OY UW UH EH EY IH IY ER".split())
BILABIAL = {"P", "B", "M"}

_DICT = None


def cmu():
    global _DICT
    if _DICT is None:
        _DICT = {}
        with open(CMUDICT, encoding="latin-1") as f:     # own 10-line parser (BSD data, no GPL code)
            for line in f:
                parts = line.split("#")[0].split()
                if len(parts) < 2:
                    continue
                w = re.sub(r"\(\d+\)$", "", parts[0])
                _DICT.setdefault(w, " ".join(parts[1:]))
    return _DICT


NUM = {"400": "four hundred", "8": "eight", "9": "nine"}


def words_of(text):
    t = re.sub(r"\{[^}]*\}", " ", text).lower()
    t = t.replace("-", " ")
    return [w for w in re.findall(r"[a-z']+", t)]


def phones_of(word):
    w = word.lower().strip("'")
    if w in LEXICON:
        p = LEXICON[w]
    else:
        p = cmu().get(w) or cmu().get(w.replace("'", ""))
    if p is None:                       # crude letter fallback
        p = " ".join({"a": "AE", "e": "EH", "i": "IH", "o": "AA", "u": "AH", "b": "B", "p": "P",
                      "m": "M", "f": "F", "v": "V", "w": "W"}.get(c, "T") for c in w)
    return [re.sub(r"\d", "", x) for x in p.split()]


def viseme(ph):
    return VIS.get(ph, "C")


def _norm(w):
    return re.sub(r"[^a-z']", "", w.lower())


def match_words(script, asr):
    """script: [w]; asr: [(word, t0, t1)] -> [(w, t0, t1)] (times in s)"""
    a = [_norm(w) for w, _, _ in asr]
    n, m = len(script), len(a)
    D = np.zeros((n + 1, m + 1))
    D[:, 0] = np.arange(n + 1)
    D[0, :] = np.arange(m + 1)
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            c = 0 if script[i - 1] == a[j - 1] else (0.5 if script[i - 1][:2] == a[j - 1][:2] else 1)
            D[i, j] = min(D[i - 1, j] + 1, D[i, j - 1] + 1, D[i - 1, j - 1] + c)
    i, j, pair = n, m, {}
    while i > 0 and j > 0:
        c = 0 if script[i - 1] == a[j - 1] else (0.5 if script[i - 1][:2] == a[j - 1][:2] else 1)
        if D[i, j] == D[i - 1, j - 1] + c:
            pair[i - 1] = j - 1
            i, j = i - 1, j - 1
        elif D[i, j] == D[i - 1, j] + 1:
            i -= 1
        else:
            j -= 1
    out = [None] * n
    for si, aj in pair.items():
        out[si] = [asr[aj][1], asr[aj][2]]
    return out


def speech_span(y):
    e = frame_db(y, 240)
    thr = max(np.percentile(e, 10) + 10, e.max() - 40)
    on = np.where(e > thr)[0]
    return (on[0] * 240 / SR, (on[-1] + 1) * 240 / SR) if len(on) else (0, len(y) / SR)


def frame_db(y, n):
    fr = y[: len(y) // n * n].reshape(-1, n)
    return 10 * np.log10((fr ** 2).mean(1) + 1e-12)


def word_times(y, script, asr):
    s0, s1 = speech_span(y)
    t = match_words(script, asr)
    # clip to speech, fill gaps by interpolation weighted by phone count
    known = [i for i, x in enumerate(t) if x]
    if not known:
        t[0] = [s0, s0]
        known = [0]
        t[-1] = t[-1] or [s1, s1]
        known = sorted(set([0, len(t) - 1]))
    w = [len(phones_of(x)) for x in script]
    for i in range(len(t)):
        if t[i]:
            continue
        lo = max([k for k in known if k < i], default=None)
        hi = min([k for k in known if k > i], default=None)
        a = t[lo][1] if lo is not None else s0
        b = t[hi][0] if hi is not None else s1
        lo_i = (lo + 1) if lo is not None else 0
        hi_i = hi if hi is not None else len(t)
        tot = sum(w[lo_i:hi_i]) or 1
        acc = sum(w[lo_i:i])
        t[i] = [a + (b - a) * acc / tot, a + (b - a) * (acc + w[i]) / tot]
    for x in t:
        x[0], x[1] = max(x[0], s0 - 0.02), min(max(x[1], x[0] + 0.06), s1 + 0.02)
    snap_gaps(y, t, s0, s1)
    return t


def silent_gaps(y, min_s=0.08):
    """[(a, b)] silences >= min_s inside the speech span (5 ms frames)"""
    e = frame_db(y, 240)
    thr = max(np.percentile(e, 10) + 10, e.max() - 40)
    on = e > thr
    idx = np.where(on)[0]
    if len(idx) == 0:
        return []
    out, k = [], idx[0]
    while k <= idx[-1]:
        if not on[k]:
            j = k
            while j <= idx[-1] and not on[j]:
                j += 1
            if (j - k) * 240 / SR >= min_s:
                out.append((k * 240 / SR, j * 240 / SR))
            k = j
        k += 1
    return out


def snap_gaps(y, t, s0, s1):
    """Whisper word times drift by 100-300 ms on short exclamations: move each
    word boundary into the nearest real silence when one lies within 0.3 s,
    so no word spans a pause; first word starts at speech onset, last ends at
    speech offset."""
    gaps = silent_gaps(y)
    for i in range(len(t) - 1):
        bnd = (t[i][1] + t[i + 1][0]) / 2
        near = [g for g in gaps if abs((g[0] + g[1]) / 2 - bnd) < 0.3]
        if near:
            g = min(near, key=lambda g: abs((g[0] + g[1]) / 2 - bnd))
            gaps.remove(g)
            t[i][1], t[i + 1][0] = g[0], g[1]
    if t:
        if abs(t[0][0] - s0) < 0.3:
            t[0][0] = s0
        if abs(t[-1][1] - s1) < 0.7:
            t[-1][1] = s1
    for x in t:
        x[1] = max(x[1], x[0] + 0.06)


def phone_times(y, script, wt):
    env = frame_db(y, 80)            # 1.67 ms
    out = []
    for w, (a, b) in zip(script, wt):
        ph = phones_of(w)
        wts = [2.0 if p in VOWELS else 1.0 for p in ph]
        tot = sum(wts)
        acc = 0
        for p, k in zip(ph, wts):
            t0 = a + (b - a) * acc / tot
            t1 = a + (b - a) * (acc + k) / tot
            acc += k
            if p in BILABIAL:       # closure = local energy minimum near the onset
                lo, hi = int((t0 - 0.05) * SR / 80), int((t0 + 0.05) * SR / 80)
                lo, hi = max(lo, 0), min(hi, len(env) - 1)
                if hi > lo:
                    tm = (lo + int(np.argmin(env[lo:hi]))) * 80 / SR
                    d = tm - t0
                    t0, t1 = tm, max(t1 + d * 0.5, tm + 0.03)
            out.append((w, p, t0, t1))
    return out


def mouth_track(phones, nframes):
    tr = ["X"] * nframes
    events = []                       # (frame, viseme, is_bilabial)
    for k, (_w, p, t0, t1) in enumerate(phones):
        f0 = int(round(t0 * FPS)) - 1           # lead the sound by 1 f
        f1 = max(f0 + 2, int(round(t1 * FPS)) - 1)
        events.append((max(0, f0), min(nframes, f1), viseme(p), p in BILABIAL))
    for f0, f1, v, _ in events:
        for f in range(f0, f1):
            tr[f] = v
    # bilabials win their onset frame (closure >= 1 f), but a 1-f vowel next to
    # them is not erased: draw M last on just its onset (+1 f when room)
    for f0, f1, v, bl in events:
        if bl and f0 < nframes:
            tr[f0] = "M"
            if f1 - f0 >= 2 and f0 + 1 < nframes:
                tr[f0 + 1] = "M"
    # word-internal / short gaps (< 3 f of X between shapes) do not close
    s = "".join(tr)
    for m in re.finditer(r"(?<=[^X])X{1,2}(?=[^X])", s):
        prev = s[m.start() - 1]
        s = s[:m.start()] + prev * (m.end() - m.start()) + s[m.end():]
    # every non-M shape holds >= 2 f
    t = list(s)
    i = 0
    while i < len(t):
        j = i
        while j < len(t) and t[j] == t[i]:
            j += 1
        if t[i] not in "XM" and j - i == 1 and j < len(t) and t[j] not in "M":
            t[j] = t[i]          # steal one frame from the next shape (no cascade)
            j += 1
        i = j
    return "".join(t)


def bilabial_onsets(phones):
    return [int(round(t0 * FPS)) for _w, p, t0, _ in phones if p in BILABIAL]


def check_track(track, phones):
    """sec.9.3 acceptance: M within +-1 f of every p/b/m onset; >= 3 visemes"""
    bad = []
    for f in bilabial_onsets(phones):
        win = track[max(0, f - 1): f + 2]
        if "M" not in win:
            bad.append(f)
    distinct = len(set(track) - {"X"}) + (1 if "X" in track else 0)
    return dict(bilabials=len(bilabial_onsets(phones)), missed=bad, distinct=distinct,
                ok=not bad and distinct >= 3)


def align(y, text, asr_words):
    script = words_of(text)
    wt = word_times(y, script, asr_words)
    ph = phone_times(y, script, wt)
    nf = -(-len(y) // SPF)
    tr = mouth_track(ph, nf)
    return tr, ph, wt


def mouth_file(line_id, take_n, sha, track, phones):
    out = ["# line=%s take=t%d sha256=%s frames=%d fps=30" % (line_id, take_n, sha, len(track)),
           track]
    cur, row = None, []
    for w, p, t0, _ in phones:
        row.append("%s@%d" % (p, int(round(t0 * FPS))))
    out.append("# phones: " + " ".join(row))
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    if len(sys.argv) >= 4 and sys.argv[1] == "track":
        import soundfile as sf
        from faster_whisper import WhisperModel
        y, sr = sf.read(sys.argv[2], dtype="float64")
        y = y.mean(1) if y.ndim > 1 else y
        mdl = os.environ.get("WHISPER_MODEL")
        segs, _ = WhisperModel(mdl, compute_type="int8", cpu_threads=2).transcribe(
            sys.argv[2], word_timestamps=True, language="en")
        aw = [(w.word.strip(), w.start, w.end) for s in segs for w in s.words]
        tr, ph, _ = align(y, sys.argv[3], aw)
        print(tr)
        print(check_track(tr, ph))
    else:
        print(__doc__)
