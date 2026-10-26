#!/usr/bin/env python3
"""mayor_voice.py - character processing for the creator's Mayor Giggs takes.

  mayor_voice.py IN_TAKES_ROOT OUT_TAKES_ROOT [--lines lines.csv] [--ab DIR]

For every recorded Mayor take (*_rec_*): trim dead air inside the line, then
a WORLD (pyworld) re-synthesis with
  * pitch up PITCH_ST semitones (turkey-ish, Mad-Hatter lift),
  * formants raised slightly (FORMANT, spectral-envelope warp) so it doesn't
    chipmunk, and a gentle nasal/beak EQ bump later in the chain,
  * soft pitch correction: sung/hummed lines are pulled toward the C-minor
    Mingle scale (strength SUNG_STR, slow retune by smoothing the target);
    speech is barely nudged (SPEECH_STR) - never the hard robotic snap,
  * a formant-safe time-stretch (rubberband via ffmpeg) of at most +-8% when
    the take overruns its lines.csv target.
Every other speaker's folder is linked through unchanged. The dialogue chain
(denoise, de-ess, presence, compression) runs afterwards in assemble*.sh.
"""
import csv, os, sys, subprocess, tempfile
import numpy as np, soundfile as sf, pyworld as pw

PITCH_ST, FORMANT = 3.0, 1.05
SUNG_STR, SPEECH_STR = 0.6, 0.15
SUNG = ("co12_hum", "co13_heehee", "a1_08_mayor_09", "a1_13_mayor_01", "a4_08_mayor_01", "a1_12_mayor_02")
CMINOR = [0, 2, 3, 5, 7, 8, 10]   # C natural minor (the Mingle's key family)
SR = 48000

def load(p):
    with tempfile.NamedTemporaryFile(suffix=".wav") as t:
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", p, "-ac", "1", "-ar", str(SR), t.name], check=True)
        x, _ = sf.read(t.name)
    return x.astype(np.float64)

def trim_gaps(x, keep=0.15, min_gap=0.25):
    hop = 480; n = len(x) // hop
    e = np.sqrt((x[: n * hop].reshape(n, hop) ** 2).mean(1)); thr = max(e.max() * 0.03, 1e-4)
    voiced = e > thr
    nz = np.nonzero(voiced)[0]
    if len(nz) == 0: return x
    a, b = max(nz[0] - 3, 0), min(nz[-1] + 6, n)
    out, run = [], 0
    for i in range(a, b):
        if voiced[i]: run = 0; out.append(x[i * hop:(i + 1) * hop])
        else:
            run += 1
            if run * hop / SR <= keep: out.append(x[i * hop:(i + 1) * hop])
    return np.concatenate(out)

def snap(f0, strength, scale):
    out = f0.copy(); v = f0 > 0
    if not v.any() or strength <= 0: return out
    midi = 69 + 12 * np.log2(f0[v] / 440.0)
    if scale:
        pc = np.array(scale)
        base = np.floor(midi / 12) * 12
        cands = base[:, None] + np.concatenate([pc - 12, pc, pc + 12])[None, :]
        tgt = cands[np.arange(len(midi)), np.abs(cands - midi[:, None]).argmin(1)]
    else: tgt = np.round(midi)
    # slow retune: smooth the correction so it glides, never snaps
    corr = tgt - midi
    k = 9; corr = np.convolve(corr, np.ones(k) / k, mode="same")
    out[v] = 440.0 * 2 ** ((midi + strength * corr - 69) / 12)
    return out

def character(x, sung):
    f0, t = pw.harvest(x, SR, f0_floor=60, f0_ceil=900, frame_period=5.0)
    sp = pw.cheaptrick(x, f0, t, SR); ap = pw.d4c(x, f0, t, SR)
    f0 = f0 * 2 ** (PITCH_ST / 12)
    f0 = snap(f0, SUNG_STR if sung else SPEECH_STR, CMINOR if sung else None)
    # formant lift: read the envelope at freq/FORMANT
    nb = sp.shape[1]; src = np.clip(np.arange(nb) / FORMANT, 0, nb - 1)
    lo = np.floor(src).astype(int); hi = np.minimum(lo + 1, nb - 1); w = src - lo
    sp2 = sp[:, lo] * (1 - w) + sp[:, hi] * w
    y = pw.synthesize(np.ascontiguousarray(f0), np.ascontiguousarray(sp2), np.ascontiguousarray(ap), SR, frame_period=5.0)
    return y / (np.abs(y).max() + 1e-9) * 0.9

def stretch(path, ratio):
    if abs(ratio - 1) < 0.01: return
    tmp = path + ".tmp.wav"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-af", f"rubberband=tempo={ratio:.4f}:formant=preserved", tmp], check=True)
    os.replace(tmp, path)

def main():
    src, dst = sys.argv[1], sys.argv[2]
    lines = sys.argv[sys.argv.index("--lines") + 1] if "--lines" in sys.argv else None
    ab = sys.argv[sys.argv.index("--ab") + 1] if "--ab" in sys.argv else None
    target = {}
    if lines:
        for r in csv.DictReader(open(lines)):
            try: target[r["line_id"]] = float(r["target_seconds"])
            except Exception: pass
    os.makedirs(dst, exist_ok=True)
    for spk in os.listdir(src):
        d = os.path.join(dst, spk)
        if spk != "mayor" and not os.path.exists(d): os.symlink(os.path.abspath(os.path.join(src, spk)), d)
    sd, dd = os.path.join(src, "mayor/ep101"), os.path.join(dst, "mayor/ep101"); os.makedirs(dd, exist_ok=True)
    for f in sorted(os.listdir(sd)):
        lid = f.split("_rec_")[0] if "_rec_" in f else None
        if not lid:   # lib/tts takes pass through
            if not os.path.exists(os.path.join(dd, f)): os.symlink(os.path.abspath(os.path.join(sd, f)), os.path.join(dd, f))
            continue
        x = load(os.path.join(sd, f))
        sung = lid.startswith(SUNG)
        y = character(trim_gaps(x) if not lid.startswith("co12_hum") else x, sung)
        out = os.path.join(dd, f.replace(".flac", ".wav"))
        sf.write(out, y, SR)
        tg = target.get(lid)
        dur = len(y) / SR
        if tg and dur > tg: stretch(out, min(1.08, dur / tg))
        print(f"{lid}: {len(x)/SR:.2f}s -> {sf.info(out).duration:.2f}s{' sung' if sung else ''}")
        if ab and lid in ("a1_08_mayor_01", "a1_08_mayor_08", "co12_hum"):
            os.makedirs(ab, exist_ok=True)
            sf.write(os.path.join(ab, lid + "_A_before.wav"), x / (np.abs(x).max() + 1e-9) * 0.9, SR)
            subprocess.run(["cp", out, os.path.join(ab, lid + "_B_after.wav")])

if __name__ == "__main__":
    main()
