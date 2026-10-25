"""showtk.review.synthetic - a synthetic co01..co03 (390 f) for the delivery and review
tools while no PS1 render exists, and for their tests (AT12.1, AT12.2, AT13.1).

It writes what the compiler and the capture will write, in their formats:
  <root>/build/<ep>/timeline.json, audio_events.json   (sec.6.4 / 6.6 numbers)
  <root>/masters/<shot>.mkv                              (FFV1, 320x240 bgr0, -g 1)
  <root>/masters/<shot>.frames.sha256                    (one SHA-256 per raw frame)
  <root>/build/<ep>/audio/mix_web.wav                    (48 kHz, 24-bit stereo)
Every picture change sits on its scripted frame (the fade 0-7, the reveals ending
on f 24 and f 60, the iris 90-103, the co02 SUPER's 52 glyphs on f 120-222, the
co03 SUPER on f 283-333), the cuts are colour changes, co02 has a 1 px/f drifting
plane and co03 twelve walking kids, and a 1-pixel frame counter makes every frame
unique.  Pixels are palette-exact except the fade ramp (a camfx class, sec.2.3).

    python3 -m showtk.review.synthetic OUT_ROOT
"""
import hashlib
import json
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

W, H, Y0, PH = 320, 240, 30, 180


def palette():
    """doomo_palette (core/src/palette.c, read-only) as RGB888, (v<<3)|(v>>2)."""
    import re
    src = (Path(__file__).resolve().parents[4] / "core" / "src" / "palette.c").read_text()
    i = src.index("doomo_palette[256] = {")
    body = re.sub(r"/\*.*?\*/", "", src[i:src.index("};", i)], flags=re.S)
    vals = []
    for _, a in re.findall(r"\b(C3|C)\s*\(([^()]*)\)", body):
        v = [int(x, 0) for x in a.split(",")]
        for k in range(0, len(v), 3):
            vals.append(tuple(((c >> 3) << 3) | ((c >> 3) >> 2) for c in v[k:k + 3]))
    assert len(vals) == 256, len(vals)
    return np.array(vals, np.uint8)


GLYPHS_CO02 = 52
SUPER_CO02 = (120, 222)
SUPER_CO03 = (283, 333)


def glyph_frames(f0, f1, n):
    return [f0 + round(g * (f1 - f0) / (n - 1)) for g in range(n)]


def frame(f, pal):
    """Episode frame f -> 240x320x3 RGB."""
    img = np.zeros((H, W, 3), np.uint8)
    pic = img[Y0:Y0 + PH]
    if f < 90:                                  # co01 calendar page, crayon
        t = f
        pic[:] = pal[47]
        rows = PH if t >= 7 else (PH * (t + 1)) // 8   # fade over R8: complete on +7
        pic[rows:] = 0
        r = min(t, 24) / 24.0                     # reveal circle_midsummer over +0..+24
        n = int(round(r * 160))
        if n:
            pic[60:64, 80:80 + n] = pal[19]
        if t >= 30:                               # reveal fireworks over +30..+60
            m = int(round(min(t - 30, 30) / 30.0 * 120))
            if m:
                pic[100:104, 100:100 + m] = pal[21]
    elif f < 270:                               # co02 storybook park
        t = f - 90
        pic[:] = pal[86]
        x = np.arange(W)
        band = ((x + t) // 4) % 2 == 0            # the near plane drifts 1 px/f
        pic[150:170][:, band] = pal[90]
        if t < 13:                                # iris_in over 13 f (90..102), open on 103
            rad = int(10 + t * 15)
            yy, xx = np.ogrid[:PH, :W]
            mask = (yy - 90) ** 2 + (xx - 160) ** 2 > rad * rad
            pic[mask] = 0
        gf = glyph_frames(*SUPER_CO02, GLYPHS_CO02)
        shown = sum(1 for g in gf if g <= f)
        for g in range(shown):                     # SUPER s_co02, type-on, until=end
            gx, gy = 12 + (g % 26) * 11, 124 + (g // 26) * 14
            pic[gy:gy + 12, gx:gx + 8] = pal[2]
    else:                                       # co03 bridge, 12 kids crossing
        t = f - 270
        pic[:] = pal[100]
        pic[118:122] = pal[80]
        for k in range(12):
            ripple = (k * 7) % 4
            x = (-32 + k * 26 + max(0, t - ripple)) % 352 - 16 if k < 6 else \
                (352 - (k - 6) * 26 - max(0, t - ripple)) % 352 - 16
            if 0 <= x < W - 16:
                pic[94 + (k >= 6) * 6:118 + (k >= 6) * 6, x:x + 16] = pal[120 + k]
        if SUPER_CO03[0] <= f <= SUPER_CO03[1]:
            pic[140:152, 40:280] = pal[2]
    for b in range(12):                         # 1-px frame counter (row 1 of the picture)
        if (f >> b) & 1:
            pic[1, 300 + b] = pal[2]
    return img


def timeline(ep="ep101"):
    gl = glyph_frames(*SUPER_CO02, GLYPHS_CO02)
    shots = [
        {"id": "co01", "beat": "e101_co_1", "act": "CO", "start": 0, "len": 90, "px": 2,
         "look": "crayon", "set": "calendar_page", "set_kind": "2d", "scene": "co_storybook",
         "cut": "FADE", "gore": "CUTE", "badge": "PS1-LEGAL", "src": "show/ep101/shots/co.shw:3",
         "events": [
             {"f": 0, "op": "fade", "args": "in over=R8", "span": [0, 7], "src": "co.shw:5"},
             {"f": 0, "op": "reveal", "args": "circle_midsummer over=+0..+24", "span": [0, 24],
              "src": "co.shw:6"},
             {"f": 0, "op": "fol", "args": "FOL_crayon_scratch follow=circle_midsummer", "k": 0,
              "src": "co.shw:6"},
             {"f": 30, "op": "reveal", "args": "fireworks_scribble over=+30..+60", "span": [30, 60],
              "src": "co.shw:7"},
             {"f": 30, "op": "fol", "args": "FOL_crayon_scratch follow=fireworks_scribble", "k": 1,
              "src": "co.shw:7"},
             {"f": 60, "op": "mx", "args": "mingle_box start", "k": 2, "src": "co.shw:8"}]},
        {"id": "co02", "beat": "e101_co_2", "act": "CO", "start": 90, "len": 180, "px": 1,
         "look": "crayon", "set": "park_storybook", "set_kind": "2d", "cut": "CUT",
         "gore": "CUTE", "badge": "PS1-LEGAL", "src": "show/ep101/shots/co.shw:12",
         "actors": [{"id": "bandstand", "kind": "prop", "x": 160, "y": 92}],
         "events": [
             {"f": 90, "op": "iris_in", "args": "at=prop:bandstand over=13 shape=circle",
              "span": [90, 103], "src": "co.shw:15"},
             {"f": 90, "op": "sfx", "args": "FX_SHWIP", "k": 3, "src": "co.shw:15"},
             {"f": 90, "op": "walla", "args": "kids_far on gain=-28dB", "k": 4, "src": "co.shw:16"},
             {"f": 90, "op": "cc", "args": "[soft crowd babble]", "src": "co.shw:16"},
             {"f": 120, "op": "super", "args": "s_co02 style=type over=+30..+132 at=lower until=end",
              "span": [120, 222], "glyphs": gl, "src": "co.shw:17"}]},
        {"id": "co03", "beat": "e101_co_2", "act": "CO", "start": 270, "len": 120, "px": 2,
         "look": "crayon", "set": "bridge_storybook", "set_kind": "2d", "cut": "CUT",
         "gore": "CUTE", "badge": "PS1-LEGAL", "src": "show/ep101/shots/co.shw:21",
         "crowds": [{"id": "bunnies", "n": 6, "age": "child", "tags": ["KID"], "kind": "chibi"},
                    {"id": "imps", "n": 6, "age": "child", "tags": ["KID"], "kind": "chibi"}],
         "events": [
             {"f": 270, "op": "fol", "args": "FOL_paw_patter from=crowd:bunnies,imps", "k": 5,
              "src": "co.shw:29"},
             {"f": 270, "op": "walla", "args": "kids_far to gain=-20dB over=30", "k": 6,
              "src": "co.shw:29"},
             {"f": 283, "op": "super", "args": "s_co03 style=type at=lower until=+63",
              "span": list(SUPER_CO03), "src": "co.shw:30"}]},
    ]
    lines = [{"id": "w_kids_%02d" % i, "who": "walla:kids_walla", "text": t, "walla": True,
              "shot": "co02" if i <= 3 else "co03", "start": 90 + 40 * i, "len": 30, "take": "tts"}
             for i, t in enumerate(["Cupcakes!", "Wait for me!", "Soup! There's soup!",
                                    "Look, lanterns!", "Race you to the bridge!", "Hi! Hi! Hi!"], 1)]
    return {
        "abi": 1, "episode": ep, "title": "Mingle Night", "fps": 30, "audio_rate": 48000,
        "canvas": {"w": 320, "h": 180, "y_off": 30, "full_h": 240},
        "acts": [{"id": "CO", "title": "Cold open"}],
        "beats": [{"id": "e101_co_1", "act": "CO", "start": 0, "len": 90},
                  {"id": "e101_co_2", "act": "CO", "start": 90, "len": 300}],
        "shots": shots, "lines": lines,
        "strings": [{"id": "s_co02", "text": "Every Midsummer night, the whole park had one party.",
                     "at": "lower", "start": 120, "end": 270},
                    {"id": "s_co03", "text": "It was called the Mingle.", "at": "lower",
                     "start": 283, "end": 334}],
        "cast": {"mayor": {"age": "adult", "name": "Mayor Giggs"}},
        "voices": {"kids_walla": {"age": "child", "tags": ["KID"]}},
    }


AUDIO_EVENTS = [
    {"k": 0, "smp": 0, "src": "FOL_crayon_scratch", "follow": "co01:circle_midsummer", "bus": "FX",
     "gain_db": -14},
    {"k": 1, "smp": 48000, "src": "FOL_crayon_scratch", "follow": "co01:fireworks_scribble",
     "bus": "FX", "gain_db": -17},
    {"k": 2, "smp": 96000, "src": "cue:mingle_box", "from": "bar:1.1", "bus": "MX", "gain_db": -12},
    {"k": 3, "smp": 144000, "src": "FX_SHWIP", "bus": "FX", "gain_db": -10, "pan": 0.0},
    {"k": 4, "smp": 144000, "src": "walla:kids_far", "op": "on", "bus": "DX", "gain_db": -28,
     "cc": "[soft crowd babble]"},
    {"k": 5, "smp": 432000, "src": "FOL_paw_patter", "from": "crowd:co03.bunnies,co03.imps",
     "bus": "FX", "gain_db": -18},
    {"k": 6, "smp": 432000, "src": "walla:kids_far", "op": "to", "gain_db": -20, "over_smp": 48000},
]


def write_master(path, frames_rgb):
    """FFV1 exactly as capture.py writes it (bgr0, level 3, -g 1) + frames.sha256."""
    p = subprocess.Popen(["ffmpeg", "-hide_banner", "-v", "error", "-y", "-f", "rawvideo",
                          "-pix_fmt", "bgr0", "-s", "%dx%d" % (W, H), "-r", "30", "-i", "-",
                          "-c:v", "ffv1", "-level", "3", "-g", "1", str(path)],
                         stdin=subprocess.PIPE)
    hashes = []
    for img in frames_rgb:
        bgr0 = np.zeros((H, W, 4), np.uint8)
        bgr0[..., 0], bgr0[..., 1], bgr0[..., 2] = img[..., 2], img[..., 1], img[..., 0]
        b = bgr0.tobytes()
        hashes.append(hashlib.sha256(b).hexdigest())
        p.stdin.write(b)
    p.stdin.close()
    if p.wait() != 0:
        raise RuntimeError("ffmpeg failed writing %s" % path)
    Path(str(path)[:-4] + ".frames.sha256").write_text(
        "".join("%s  %06d\n" % (h, i) for i, h in enumerate(hashes)))


def write_mix(path, nframes, seed=7):
    """A temp mix with an onset on every audio event: noise for foley, a tone for MINGLE,
    a chirp for SHWIP, a babble bed (48 kHz, 24-bit stereo)."""
    rng = np.random.default_rng(seed)
    n = nframes * 1600
    x = np.zeros(n)
    t = np.arange(n) / 48000.0
    x += 10 ** (-40 / 20) * rng.standard_normal(n) * 0.3

    def burst(s0, length, sig):
        e = min(n, s0 + length)
        x[s0:e] += sig[:e - s0]
    L = 24000
    env = np.exp(-np.arange(L) / 6000.0)
    burst(0, L, 0.25 * rng.standard_normal(L) * env)
    burst(48000, L, 0.2 * rng.standard_normal(L) * env)
    burst(96000, n - 96000, 0.18 * np.sin(2 * np.pi * 349.2 * t[:n - 96000]))   # F4
    ch = np.sin(2 * np.pi * (800 + 4000 * np.arange(9600) / 9600.0) * np.arange(9600) / 48000.0)
    burst(144000, 9600, 0.4 * ch * np.hanning(9600))
    burst(144000, n - 144000, 0.05 * rng.standard_normal(n - 144000))
    burst(432000, L, 0.2 * rng.standard_normal(L) * env)
    x = np.clip(x, -0.99, 0.99)
    pcm = (x * (2 ** 23 - 1)).astype(np.int32)
    st = np.stack([pcm, pcm], 1).reshape(-1)
    b = np.zeros((st.size, 3), np.uint8)
    b[:, 0], b[:, 1], b[:, 2] = st & 255, (st >> 8) & 255, (st >> 16) & 255
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(3)
        w.setframerate(48000)
        w.writeframes(b.tobytes())


def make(root, ep="ep101"):
    root = Path(root)
    b = root / "build" / ep
    (b / "audio").mkdir(parents=True, exist_ok=True)
    md = root / "masters"
    md.mkdir(parents=True, exist_ok=True)
    tl = timeline(ep)
    (b / "timeline.json").write_text(json.dumps(tl, indent=1))
    (b / "audio_events.json").write_text(json.dumps(AUDIO_EVENTS, indent=1))
    pal = palette()
    for s in tl["shots"]:
        write_master(md / (s["id"] + ".mkv"), (frame(f, pal) for f in range(s["start"], s["start"] + s["len"])))
    write_mix(b / "audio" / "mix_web.wav", 390)
    return {"root": root, "build": b, "masters": md, "timeline": b / "timeline.json"}


if __name__ == "__main__":
    out = make(sys.argv[1] if len(sys.argv) > 1 else "synthetic")
    print(json.dumps({k: str(v) for k, v in out.items()}, indent=1))
