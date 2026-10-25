"""Text -> Kokoro phonemes (spec §10.2 step 1).

Primary: the CMU Pronouncing Dictionary data file, read by a ten-line parser (the PyPI
wrapper is GPL and is never imported), an ARPAbet -> Kokoro/misaki (American) map, and
show/series/lexicon.shw for names and coinages. Inline overrides: "I'm {fʌzz}".
Unknown words are split into dictionary words (Grumbleguts = grumble + guts) and
reported (G2P-OOV) so a lexicon row can be added. espeak is not used (its wheel's data
path is broken here, spec R7).

The game's text codes are read, not spoken: {d} deadpan and {w} wavy become line flags,
{big}..{/} is dropped, and the pause codes {.} {..} {...} (R5, R8, R13) split the line
into segments that the renderer joins with exact silences.
"""
from __future__ import annotations

import dataclasses as dc
import re

from . import env, shwio

ARPA = {"AA": "ɑ", "AE": "æ", "AH": "ʌ", "AO": "ɔ", "AW": "W", "AY": "I", "B": "b", "CH": "ʧ",
        "D": "d", "DH": "ð", "EH": "ɛ", "ER": "ɜɹ", "EY": "A", "F": "f", "G": "ɡ", "HH": "h",
        "IH": "ɪ", "IY": "i", "JH": "ʤ", "K": "k", "L": "l", "M": "m", "N": "n", "NG": "ŋ",
        "OW": "O", "OY": "Y", "P": "p", "R": "ɹ", "S": "s", "SH": "ʃ", "T": "t", "TH": "θ",
        "UH": "ʊ", "UW": "u", "V": "v", "W": "w", "Y": "j", "Z": "z", "ZH": "ʒ"}
ARPA_VOWELS = {"AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER", "EY", "IH", "IY", "OW", "OY", "UH", "UW"}
PAUSES = {"{.}": 5, "{..}": 8, "{...}": 13}
CODES = {"{d}", "{w}", "{big}", "{/}"}
PUNCT = set(".,!?;:…—")
LETTERS = {"A": "ˈA", "B": "bˈi", "C": "sˈi", "D": "dˈi", "E": "ˈi", "F": "ˈɛf", "I": "ˈI", "O": "ˈO",
           "U": "jˈu", "X": "ˈɛks"}

_ONES = "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen " \
        "fifteen sixteen seventeen eighteen nineteen".split()
_TENS = "_ _ twenty thirty forty fifty sixty seventy eighty ninety".split()


def num_words(n):
    n = int(n)
    if n < 20:
        return _ONES[n]
    if n < 100:
        return _TENS[n // 10] + ("" if n % 10 == 0 else " " + _ONES[n % 10])
    if n < 1000:
        return _ONES[n // 100] + " hundred" + ("" if n % 100 == 0 else " " + num_words(n % 100))
    if n < 10000:
        return num_words(n // 1000) + " thousand" + ("" if n % 1000 == 0 else " " + num_words(n % 1000))
    return " ".join(_ONES[int(c)] for c in str(n))


_DICT = None


def cmudict():
    """{word: [arpabet list]} (first pronunciation only; variants 'word(2)' skipped)."""
    global _DICT
    if _DICT is None:
        d = {}
        with open(env.cmudict_path(), encoding="utf-8", errors="replace") as fh:
            for ln in fh:
                ln = ln.split("#", 1)[0].strip()
                if not ln:
                    continue
                w, *ph = ln.split()
                if "(" in w or w in d:
                    continue
                d[w] = ph
        _DICT = d
    return _DICT


def arpa_to_kokoro(phs):
    """ARPAbet list -> Kokoro phoneme string, misaki-style: the stress mark sits right
    before its vowel, unstressed AH -> ə, ER0 -> əɹ, and an American flap (t -> ɾ)
    between a vowel (or r) and an unstressed vowel."""
    out = []
    n = len(phs)
    for i, ph in enumerate(phs):
        base = re.sub(r"\d", "", ph)
        st = ph[-1] if ph[-1].isdigit() else ""
        s = ARPA[base]
        if base == "AH" and st == "0":
            s = "ə"
        elif base == "ER" and st == "0":
            s = "əɹ"
        elif base == "T" and 0 < i < n - 1:
            prev = re.sub(r"\d", "", phs[i - 1])
            nxt = phs[i + 1]
            nn = phs[i + 2] if i + 2 < n else ""
            syllabic_n = nxt == "AH0" and nn == "N"          # button, frightened: no flap
            if (prev in ARPA_VOWELS or prev == "R") and nxt[-1:] == "0" \
                    and re.sub(r"\d", "", nxt) in ARPA_VOWELS and not syllabic_n:
                s = "ɾ"
        if base in ARPA_VOWELS and st in ("1", "2"):
            s = ("ˈ" if st == "1" else "ˌ") + s
        out.append(s)
    return "".join(out)


@dc.dataclass
class Word:
    text: str          # as written (for captions / ASR)
    phon: str          # Kokoro phonemes of the word
    tok0: int = 0      # first token index in the segment's phoneme string
    tok1: int = 0      # one past the last token
    oov: bool = False


@dc.dataclass
class Segment:
    phon: str                      # the Kokoro input
    words: list
    pause_after: int = 0           # frames of silence after this segment (pause codes)


@dc.dataclass
class Utterance:
    text: str
    segments: list
    flags: set
    oov: list

    @property
    def phon(self):
        return " | ".join(s.phon for s in self.segments)

    @property
    def words(self):
        return [w for s in self.segments for w in s.words]


class G2P:
    def __init__(self, lexicon=None):
        if lexicon is None:
            lexicon = shwio.read_lexicon(shwio.series_path("lexicon.shw"))
        self.lex = {k.lower(): v for k, v in lexicon.items()}
        self.d = cmudict()

    # ---------------------------------------------------------------- words
    def _dict_word(self, w):
        if w in self.lex:
            return self.lex[w]
        if w in self.d:
            return arpa_to_kokoro(self.d[w])
        return None

    def _compound(self, w):
        """Split an unknown word into the fewest dictionary words (pieces >= 2 letters)."""
        n = len(w)
        best = [None] * (n + 1)
        best[0] = []
        for i in range(n):
            if best[i] is None:
                continue
            for j in range(i + 2, n + 1):
                p = self._dict_word(w[i:j])
                if p is not None and (best[j] is None or len(best[j]) > len(best[i]) + 1):
                    best[j] = best[i] + [p]
        return "".join(best[n]) if best[n] else None

    def word(self, raw):
        """-> (phonemes, oov). Handles possessive 's, hyphens and digits."""
        w = raw.lower().replace("’", "'").strip("'")
        if not w:
            return "", False
        if w.isdigit():
            return " ".join(self.word(x)[0] for x in num_words(w).split()), False
        p = self._dict_word(w)
        if p is not None:
            return p, False
        for suf in ("'s", "s'"):
            if w.endswith(suf):
                b = self._dict_word(w[:-2]) or self._compound(w[:-2])
                if b:
                    last = b.rstrip("ˈˌ")[-1:]
                    if last in "szʃʒʧʤ":
                        return b + "ɪz", False
                    return b + ("s" if last in "ptkfθ" else "z"), False
        if w.endswith("s") and self._dict_word(w[:-1]):
            b = self._dict_word(w[:-1])
            return b + ("s" if b[-1:] in "ptkfθ" else "z"), False
        c = self._compound(w)
        if c:
            return c, True
        # last resort: spell it (never silent)
        letters = [self._dict_word(ch) or "" for ch in w if ch.isalpha()]
        return " ".join(x for x in letters if x), True

    # ---------------------------------------------------------------- lines
    def __call__(self, text):
        flags = set()
        for code in ("{d}", "{w}"):
            if code in text:
                flags.add({"{d}": "deadpan", "{w}": "wavy"}[code])
        t = text.replace("{big}", "").replace("{/}", "").replace("{d}", "").replace("{w}", "")
        t = t.replace("’", "'").replace("“", "").replace("”", "").replace('"', "")
        t = t.replace("...", "…").replace("--", "—")
        # a leading "..." means "come in a little late, small" (BREAKDOWN §3.1): a timing note,
        # not a sound; Kokoro reads a leading ellipsis as a stray "a"
        t = re.sub(r"^\s*…\s*", "", t)
        # split at pause codes (longest first)
        parts = re.split(r"(\{\.\.\.\}|\{\.\.\}|\{\.\})", t)
        segs, oov = [], []
        for i in range(0, len(parts), 2):
            chunk = parts[i]
            pause = PAUSES.get(parts[i + 1], 0) if i + 1 < len(parts) else 0
            seg = self._segment(chunk, oov)
            if seg is None:
                if segs:
                    segs[-1].pause_after += pause
                continue
            seg.pause_after = pause
            segs.append(seg)
        return Utterance(text, segs, flags, oov)

    def _segment(self, chunk, oov):
        toks = re.findall(r"\{[^}]*\}|[A-Za-z0-9']+(?:-[A-Za-z0-9']+)*|[.,!?;:…—]", chunk)
        phon, words = "", []
        for tk in toks:
            if tk.startswith("{"):
                body = tk[1:-1]
                if not body:
                    continue
                ph, wt = body, body
                w = Word(wt, ph)
            elif tk in PUNCT:
                if phon.endswith(" "):
                    phon = phon[:-1]
                phon += tk + " "
                continue
            else:
                pieces = tk.split("-")
                ph_parts = []
                bad = False
                for pc in pieces:
                    if len(pieces) > 1 and len(pc) == 1 and pc.isupper():
                        ph_parts.append(LETTERS.get(pc, self.word(pc)[0]))   # A-plus
                        continue
                    p, o = self.word(pc)
                    bad |= o
                    ph_parts.append(p)
                ph = " ".join(x for x in ph_parts if x)
                w = Word(tk, ph, oov=bad)
                if bad:
                    oov.append(tk)
            if not ph:
                continue
            w.tok0 = len(phon)
            phon += ph
            w.tok1 = len(phon)
            phon += " "
            words.append(w)
        phon = phon.strip()
        if not words:
            return None
        if phon[-1] not in PUNCT:
            phon += "."
        return Segment(phon, words)
