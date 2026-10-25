"""The singer: Kokoro-82M speaks the lyric with the song's syllable timing, WORLD
re-pitches it onto the melody and stretches the vowels to the note lengths.

Why two stages: Kokoro's decoder only follows a forced F0 inside the voice's
speaking range (it folds 440 Hz down to 220 for af_heart) and its vowels go dull
when held for more than ~0.3 s. So Kokoro renders a *spoken* phrase (natural
consonants, 100-200 ms vowels) on a melody-shaped pitch curve transposed into its
range; WORLD (CheapTrick envelope + D4C aperiodicity, analysed with the known
F0) then re-synthesises it frame by frame on the exact target pitch, holding each
vowel's steady part for as long as the note lasts. Formants are kept, so the voice
keeps its identity when it is shifted up.

    notes: [SNote(t0, t1, midi, syl, onset, nucleus, coda, word_end, melisma)]
    y48, t_offset = Singer().phrase(notes, voice="af_heart")
"""
from __future__ import annotations

import dataclasses as dc
import math

import numpy as np
import pyworld as pw
from scipy.signal import resample_poly

from kokoro_voice import HOP, SR_K, UNVOICED, KokoroVoice

FP = 5.0                      # WORLD frame period (ms)
DIPH = set("AIOWY")           # misaki diphthong letters (eɪ aɪ oʊ aʊ ɔɪ)
# Kokoro's audio runs ~65 ms ahead of its token frames (measured: fricative bursts vs
# token starts, -60..-75 ms for natural and forced durations alike)
OUT_OFFSET = -0.065
RANGES = {"af_heart": (170.0, 300.0), "af_bella": (165.0, 290.0), "jf_tebukuro": (260.0, 470.0),
          "jf_alpha": (220.0, 400.0), "jf_nezumi": (200.0, 360.0), "jf_gongitsune": (200.0, 380.0)}


@dc.dataclass
class SNote:
    t0: float                 # seconds (note on)
    t1: float                 # seconds (note off)
    midi: float
    syl: str = ""             # display text
    onset: str = ""           # consonants before the vowel (Kokoro symbols)
    nucleus: str = ""         # the vowel, with its stress mark ("ˈu")
    coda: str = ""            # consonants after the vowel
    word_end: bool = True     # a space follows in the Kokoro text
    melisma: bool = False     # continues the previous note's syllable (no new consonants)
    vib: float = 1.0          # vibrato amount
    scoop: float = None       # semitones below at onset (None = auto)
    fall: float = 0.0         # semitones of fall at the end (phrase end)


def _midi_hz(m):
    return 440.0 * 2 ** ((np.asarray(m, dtype=float) - 69) / 12)


class Singer:
    def __init__(self, kv=None):
        self.kv = kv or KokoroVoice()

    # ------------------------------------------------------------------ text
    @staticmethod
    def syllables(notes):
        """Group notes into syllables (a melisma note joins the previous syllable)."""
        out = []
        for i, n in enumerate(notes):
            if n.melisma and out:
                out[-1]["notes"].append(i)
            else:
                out.append(dict(notes=[i], onset=n.onset, nucleus=n.nucleus, coda=n.coda,
                                word_end=n.word_end))
        # the coda of a melisma'd syllable is the coda of its LAST note when given there
        for s in out:
            last = notes[s["notes"][-1]]
            if last.melisma and last.coda:
                s["coda"] = last.coda
            s["word_end"] = notes[s["notes"][-1]].word_end
        return out

    # ------------------------------------------------------------- render
    def phrase(self, notes, voice="af_heart", shift=None, f0_lo=None, f0_hi=None, seed=0,
               formant=1.0, vmax=14, hold_energy=False, leadf=12, mark_fr=1, cmax=3, sp_fr=1,
               raw_only=False, formant_track=0.0, formant_ref=69.0):
        """Render one phrase. Returns (audio at 48 kHz mono, time in seconds of sample 0,
        info). The Kokoro render is laid out on the song's own 25 ms grid: every vowel
        starts on its note (consonants before the beat, codas before the next onset);
        vowels longer than `vmax` frames are rendered `vmax` long and WORLD holds their
        steady middle for the rest of the note. `shift`: semitones the render sits
        below the melody (None = fit the voice's comfortable range)."""
        kv = self.kv
        r = np.random.default_rng(seed)
        syls = self.syllables(notes)
        midis = np.array([n.midi for n in notes])
        vr = RANGES.get(voice if isinstance(voice, str) else max(voice, key=voice.get), (170.0, 300.0))
        lo_hz, hi_hz = (f0_lo or vr[0]), (f0_hi or vr[1])
        if shift is None:
            mid_hz = math.sqrt(lo_hz * hi_hz)
            centre = 0.5 * (midis.max() + midis.min())
            shift = max(0.0, round(centre - (69 + 12 * math.log2(mid_hz / 440.0))))
            while _midi_hz(midis.max() - shift) > hi_hz * 1.12 and shift < 36:
                shift += 1
        FR = HOP / SR_K                                   # 25 ms
        LEADF = leadf
        T0 = notes[0].t0

        def fr_of(t):
            return LEADF + int(round((t - T0) / FR))

        # ---- 1. tokens and roles
        toks, role = [], []
        for si, s in enumerate(syls):
            for ch in s["onset"]:
                toks.append(ch); role.append((si, "on"))
            for ch in s["nucleus"]:
                toks.append(ch); role.append((si, "mark" if ch in "ˈˌ" else "nuc"))
            for ch in s["coda"]:
                toks.append(ch); role.append((si, "coda"))
            if si < len(syls) - 1:
                toks.append(" " if s["word_end"] else "")
                role.append((si, "sp"))
        # drop empty separators but keep a gap slot per syllable boundary
        phon = "".join(toks)
        _, info = kv.speak(phon.replace("", "") , voice, speed=1.0)
        nat = info["dur"][1:-1]
        # map natural durations onto our token list (empty separators have none)
        nat_full, j = [], 0
        for t in toks:
            if t == "":
                nat_full.append(0)
            else:
                nat_full.append(int(nat[j])); j += 1
        # ---- 2. musical frame plan per syllable
        plan = {}                                         # token index -> musical frames
        hold = {}                                         # token index -> extra frames to hold
        on_len = []
        for si, s in enumerate(syls):
            on = [k for k, (sj, kind) in enumerate(role) if sj == si and kind == "on"]
            d = [int(np.clip(nat_full[k], 1, cmax)) for k in on]
            if si > 0:
                room = fr_of(notes[s["notes"][0]].t0) - fr_of(notes[syls[si - 1]["notes"][0]].t0)
                while sum(d) > max(1, room // 2) and max(d, default=0) > 1:
                    d[int(np.argmax(d))] -= 1
            on_len.append(d)
        for si, s in enumerate(syls):
            ks = [k for k, (sj, _) in enumerate(role) if sj == si]
            on = [k for k in ks if role[k][1] == "on"]
            mk = [k for k in ks if role[k][1] == "mark"]
            nu = [k for k in ks if role[k][1] == "nuc"]
            co = [k for k in ks if role[k][1] == "coda"]
            sp = [k for k in ks if role[k][1] == "sp"]
            n0, n1 = notes[s["notes"][0]], notes[s["notes"][-1]]
            vf = fr_of(n0.t0)
            ef = fr_of(n1.t1)
            if si + 1 < len(syls):
                lim = fr_of(notes[syls[si + 1]["notes"][0]].t0) - sum(on_len[si + 1])
            else:
                lim = ef + 2
            cend = min(ef, lim)
            co_d = [int(np.clip(nat_full[k], 1, cmax)) for k in co]
            while sum(co_d) > max(1, (cend - vf) // 2) and max(co_d, default=0) > 1:
                co_d[int(np.argmax(co_d))] -= 1
            vend = cend - sum(co_d)
            for k, d in zip(co, co_d):
                plan[k] = d
            nvow = max(1, vend - vf)
            for k in mk:                                  # stress marks need a frame
                plan[k] = min(mark_fr, max(0, nvow - 2))
                nvow -= plan[k]
            gap = max(0, lim - cend) if si + 1 < len(syls) else 0
            sp_extra = 0
            if sp and gap < sp_fr and toks[sp[0]] == " " and nvow >= 3 + sp_fr:
                sp_extra = sp_fr - gap                    # a word boundary needs a frame
                nvow -= sp_extra
            if len(nu) == 1:
                plan[nu[0]] = nvow
            else:                                         # two-letter nucleus (e.g. 'aɪ')
                plan[nu[0]] = max(1, nvow - max(1, nvow // 4))
                for k in nu[1:]:
                    plan[k] = max(1, nvow // 4) if k == nu[-1] else 1
            for k in sp:
                plan[k] = gap + sp_extra
            for k, d in zip(on, on_len[si]):
                plan[k] = d
            plan[("start", si)] = vf - sum(on_len[si])
            plan[("vf", si)] = vf
        # first syllable onset sets the lead
        first_start = plan[("start", 0)]
        lead = max(4, first_start)
        tok_m = [plan[k] for k in range(len(toks))]
        # every vowel must land on its note: check the cumulative plan
        pos = lead
        for k, (si, kind) in enumerate(role):
            if kind in ("mark", "nuc") and (k == 0 or role[k - 1][1] == "on" or role[k - 1][1] == "sp"
                                            or role[k - 1][0] != si):
                assert pos == plan[("vf", si)], (si, pos, plan[("vf", si)], toks)
            pos += tok_m[k]
        # vowels longer than vmax are rendered shorter and held
        tok_r = list(tok_m)
        for k, (si, kind) in enumerate(role):
            if kind == "nuc" and tok_m[k] > vmax:
                hold[k] = tok_m[k] - vmax
                tok_r[k] = vmax
        # drop empty tokens for the model
        keep = [k for k in range(len(toks)) if toks[k] != ""]
        ids_phon = "".join(toks[k] for k in keep)
        durs = [lead] + [tok_r[k] for k in keep] + [10]
        nfr = int(sum(durs))
        # ---- 3. musical time of every render frame (the inverse map) + Kokoro F0
        # render frame boundaries per token
        rb = np.cumsum([lead] + [tok_r[k] for k in keep])
        mb = np.cumsum([lead] + [tok_m[k] for k in keep])
        seg = []                                          # (r0, r1, m0, m1, hold?)
        for i, k in enumerate(keep):
            r0, r1, m0, m1 = rb[i], rb[i + 1], mb[i], mb[i + 1]
            if k in hold:
                h = (r1 - r0) // 2 if toks[k] not in DIPH else max(2, int(0.3 * (r1 - r0)))
                seg.append((r0, r0 + h, m0, m0 + h, False))
                seg.append((r0 + h, r0 + h, m0 + h, m0 + h + hold[k], True))
                seg.append((r0 + h, r1, m0 + h + hold[k], m1, False))
            else:
                seg.append((r0, r1, m0, m1, False))
        mel_m = np.array([n.midi for n in notes])
        mel_t = np.array([n.t0 for n in notes])

        def midi_at(t):
            i = np.clip(np.searchsorted(mel_t, t, side="right") - 1, 0, len(notes) - 1)
            return mel_m[i]

        f0k = np.zeros(2 * nfr)
        for hf in range(2 * nfr):
            rf = hf / 2.0
            mf = rf
            for (r0, r1, m0, m1, hh) in seg:
                if not hh and r0 <= rf < r1:
                    mf = m0 + (rf - r0) * (m1 - m0) / max(r1 - r0, 1e-9)
                    break
            else:
                if rf >= rb[-1]:
                    mf = mb[-1] + (rf - rb[-1])
            t = T0 + (mf - LEADF) * FR
            if lead - 2 <= rf < rb[-1] + 4:
                f0k[hf] = _midi_hz(midi_at(t) - shift)
        v = f0k > 0
        sm = np.convolve(np.where(v, np.log(np.maximum(f0k, 1)), 0), np.ones(7) / 7, "same")
        wt = np.convolve(v.astype(float), np.ones(7) / 7, "same")
        f0k = np.where(v, np.exp(sm / np.maximum(wt, 1e-6)), 0.0)
        n_fn = None
        if hold_energy:
            def n_fn(n):
                for i, k in enumerate(keep):
                    if role[k][1] == "nuc" and tok_r[k] >= 6:
                        a, b = 2 * rb[i], 2 * rb[i + 1]
                        pk = n[a:a + 6].max()
                        n[a + 4:b - 2] = np.maximum(n[a + 4:b - 2], pk - 1.0)
                return n
        y = kv.sing(ids_phon, durs, f0k, voice, n_fn=n_fn)
        if raw_only:
            return None, None, dict(raw=y, durs=durs, phon=ids_phon, shift=shift)
        # ---- 4. WORLD analysis (known pitch protects against octave errors)
        x = np.concatenate([y.astype(np.float64), np.zeros(int(0.05 * SR_K))])
        f0_h, t_h = pw.harvest(x, SR_K, f0_floor=max(60.0, float(_midi_hz(midis.min() - shift - 7))),
                               f0_ceil=min(1000.0, float(_midi_hz(midis.max() - shift + 7))), frame_period=FP)
        f0_known = np.interp(t_h, (np.arange(2 * nfr) + 0.5) * FR / 2 + OUT_OFFSET, f0k, left=0, right=0)
        voiced = f0_h > 0
        ratio = np.where(voiced & (f0_known > 0), f0_h / np.maximum(f0_known, 1), 1)
        good = voiced & (np.abs(12 * np.log2(np.maximum(ratio, 1e-3))) < 1.5)
        f0_a = np.where(good, f0_h, np.where(voiced & (f0_known > 0), f0_known, 0.0))
        f0_a = pw.stonemask(x, f0_a, t_h, SR_K)
        spx = pw.cheaptrick(x, f0_a, t_h, SR_K)
        apx = pw.d4c(x, f0_a, t_h, SR_K)
        # ---- 5. musical time -> source time (seconds of the Kokoro audio)
        g0 = T0 - LEADF * FR                               # time of musical frame 0
        t_start = T0 - 0.4
        t_end = notes[-1].t1 + 0.3
        T = np.arange(int((t_end - t_start) * 1000 / FP)) * FP / 1000 + t_start
        mfT = (T - g0) / FR                                 # musical frame position
        src = np.full(len(T), np.nan)
        for (r0, r1, m0, m1, hh) in seg:
            sel = (mfT >= m0) & (mfT < m1)
            if not sel.any():
                continue
            if hh:
                span_k = 0.3 * vmax
                # hold: slow ping-pong within +-35 % of the rendered vowel's middle
                tm = (mfT[sel] - m0) * FR
                per = 0.8
                ph = (tm / per) % 2.0
                tri = np.where(ph < 1, ph, 2 - ph)
                span = 0.3 * vmax
                rf = r0 - span / 2 + tri * span
                ramp = np.clip(tm / 0.15, 0, 1)
                rf = r0 + (rf - r0) * ramp
            else:
                rf = r0 + (mfT[sel] - m0) * (r1 - r0) / max(m1 - m0, 1e-9)
            src[sel] = rf * FR
        pre = mfT < mb[0]
        src[pre] = (rb[0] + (mfT[pre] - mb[0])) * FR
        post = mfT >= mb[-1]
        src[post] = (rb[-1] + (mfT[post] - mb[-1])) * FR
        # the audio content sits OUT_OFFSET earlier than its frames
        src = src + OUT_OFFSET
        fi = np.clip(src / (FP / 1000), 0, len(t_h) - 1)
        i0 = np.floor(fi).astype(int)
        i1 = np.minimum(i0 + 1, len(t_h) - 1)
        w = (fi - i0)[:, None]
        sp_o = np.exp((1 - w) * np.log(spx[i0] + 1e-16) + w * np.log(spx[i1] + 1e-16))
        ap_o = (1 - w) * apx[i0] + w * apx[i1]
        vsrc = (f0_a[i0] > 0) & (f0_a[i1] > 0)
        out_of = (src < 0) | (src > (len(x) / SR_K))
        # ---- 6. target pitch and synthesis
        f0_t = self._target_f0(T, notes, r)
        f0_o = np.where(vsrc & ~out_of, f0_t, 0.0)
        sp_o[out_of] = 1e-16
        if formant != 1.0 or formant_track:
            # singers open up (F1 rises) on high notes: the formant scale follows the pitch
            m_t = 69 + 12 * np.log2(np.maximum(f0_t, 1.0) / 440.0)
            k_t = formant * 2 ** (formant_track * (m_t - formant_ref) / 12)
            sp_o = self._formant(sp_o, k_t)
        yo = pw.synthesize(np.ascontiguousarray(f0_o), np.ascontiguousarray(sp_o), np.ascontiguousarray(ap_o),
                           SR_K, FP)
        y48 = resample_poly(yo, 2, 1)
        return y48, t_start, dict(shift=shift, phon=ids_phon, f0_target=f0_t, T=T, voiced=f0_o > 0,
                                  raw=y, durs=durs)

    # ------------------------------------------------------------ helpers
    @staticmethod
    def _lin(T, src, a, b, s0, s1):
        sel = (T >= a) & (T < b)
        if b > a:
            src[sel] = s0 + (T[sel] - a) / (b - a) * (s1 - s0)

    @staticmethod
    def _vowel(T, src, a, b, s0, s1, diph, r):
        """Map a target vowel span [a, b) onto the source vowel [s0, s1): keep the
        consonant->vowel transition (first 35 ms) at speed, hold the steady part
        (slow drift inside it), and land the end (the glide of a diphthong, or the
        last 30 ms) at speed just before b."""
        L = s1 - s0
        tin = min(0.035, 0.3 * L)
        if diph:
            hold_a, hold_b = s0 + 0.18 * L, s0 + 0.42 * L
            tout = min(0.14, 0.5 * (b - a), s1 - hold_b)
        else:
            hold_a, hold_b = s0 + 0.3 * L, s0 + 0.7 * L
            tout = min(0.03, 0.2 * L, 0.3 * (b - a))
        sel = (T >= a) & (T < b)
        t = T[sel] - a
        D = b - a
        out = np.empty(len(t))
        p_in = t < tin
        out[p_in] = s0 + t[p_in] / tin * (hold_a - s0) if tin > 0 else hold_a
        p_out = t >= D - tout
        out[p_out] = hold_b + (t[p_out] - (D - tout)) / max(tout, 1e-6) * (s1 - hold_b)
        mid = ~p_in & ~p_out
        if mid.any():
            tm = t[mid] - tin
            Dm = max(D - tin - tout, 1e-6)
            if Dm <= (hold_b - hold_a) * 1.3:
                out[mid] = hold_a + tm / Dm * (hold_b - hold_a)
            else:
                # slow ping-pong inside the steady part (keeps micro-variation alive)
                per = 0.9
                ph = (tm / per) % 2.0
                tri = np.where(ph < 1, ph, 2 - ph)
                out[mid] = hold_a + tri * (hold_b - hold_a)
        src[sel] = out

    @staticmethod
    def _target_f0(T, notes, r):
        m = np.full(len(T), np.nan)
        vib = np.zeros(len(T))
        for i, n in enumerate(notes):
            prev = notes[i - 1] if i else None
            nxt = notes[i + 1] if i + 1 < len(notes) else None
            a = n.t0 - (0.04 if prev is not None and prev.t1 >= n.t0 - 0.02 else 0.12)
            b = nxt.t0 - 0.04 if nxt is not None and n.t1 >= nxt.t0 - 0.02 else n.t1 + 0.12
            sel = (T >= a) & (T < b)
            tt = T[sel] - n.t0
            base = np.full(len(tt), float(n.midi))
            legato = prev is not None and prev.t1 >= n.t0 - 0.02
            if legato:
                # portamento from the previous pitch (70 ms, eased), small overshoot
                d = prev.midi - n.midi
                x = np.clip((tt + 0.04) / 0.075, 0, 1)
                ease = x * x * (3 - 2 * x)
                base = base + d * (1 - ease)
                if abs(d) >= 2:
                    base += -np.sign(d) * 0.08 * np.exp(-((tt - 0.06) / 0.04) ** 2)
            else:
                sc = n.scoop if n.scoop is not None else (0.6 if n.t1 - n.t0 > 0.3 else 0.3)
                base = base - sc * np.exp(-np.maximum(tt, 0) / 0.045) * (tt > -0.2)
            # vibrato on long notes
            L = n.t1 - n.t0
            if L > 0.32 and n.vib > 0:
                ramp = np.clip((tt - 0.2) / 0.25, 0, 1)
                ph = r.random() * 6.28
                rate = 5.5 + 0.3 * r.standard_normal()
                vib[sel] = 0.28 * n.vib * ramp * np.sin(2 * np.pi * rate * tt + ph)
            if n.fall and nxt is None:
                base = base - n.fall * np.clip((tt - (L - 0.08)) / 0.1, 0, 1) ** 2
            m[sel] = base
        # fill gaps by holding
        idx = np.flatnonzero(~np.isnan(m))
        m = np.interp(np.arange(len(T)), idx, m[idx])
        # slow natural drift (a few cents)
        drift = np.convolve(r.standard_normal(len(T)), np.ones(60) / 60, "same") * 0.03
        return _midi_hz(m + vib + drift)

    @staticmethod
    def _formant(sp, k):
        """Scale the spectral envelope along frequency (k > 1 = smaller tract); k may be
        a scalar or one value per frame."""
        nb = sp.shape[1]
        f = np.arange(nb)[None, :]
        kk = np.broadcast_to(np.asarray(k, dtype=float).reshape(-1, 1), (sp.shape[0], 1))
        src = np.clip(f / kk, 0, nb - 1)
        i0 = np.floor(src).astype(int)
        i1 = np.minimum(i0 + 1, nb - 1)
        w = src - i0
        ls = np.log(sp + 1e-16)
        rows = np.arange(sp.shape[0])[:, None]
        return np.exp((1 - w) * ls[rows, i0] + w * ls[rows, i1])
