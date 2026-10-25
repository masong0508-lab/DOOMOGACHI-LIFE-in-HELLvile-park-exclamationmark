"""showtk.deliver - `show deliver <ep> web` and `show preview <ep> <range>` (WP12).

    show deliver ep101 web [--range co01..co03] [--out DIR] [--mix WAV] [--masters DIR]
                           [--require human-voice] [--release]
    show preview ep101 co01..co03 [--out DIR] [--mix WAV]

Pipeline (SHOW_ENGINE_SPEC.md sec.12):
  timeline.json -> EDL (per-shot FFV1 masters) -> one H.264 segment per beat
  (cached by content hash in build/<ep>/segments/<recipe>/) -> joined by stream
  copy -> muxed with the mix window (sample-exact: frame f = sample 1,600 f) and
  the chapters -> SRT + VTT -> QC (ffprobe, sampled picture check, loudness) ->
  manifest.json (build/<ep>/deliver/<v>/ and show/<ep>/deliveries/<v>/).
`--release` also runs the standards gate and builds the upload package (WP17).
"""
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

from . import captions as captions_mod
from . import ffmpeg as ff
from .edl import EDL, Masters
from .sidecars import FPS, REPO, SHOW, SPF, build_dir, ep_dir, load_timeline, tc

WEB_LUFS = (-16.5, -13.5)        # web window -14..-16 LUFS (sec.10.6) with the 0.5 tolerance
WEB_TP = -1.0


class DeliverError(RuntimeError):
    pass


def log(msg):
    print(msg, file=sys.stderr, flush=True)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def canonical_hash(obj):
    return hashlib.sha256(json.dumps(obj, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


# ---- provenance -------------------------------------------------------------------
def git_state():
    try:
        head = subprocess.run(["git", "-C", str(REPO), "rev-parse", "HEAD"], capture_output=True,
                              text=True).stdout.strip()
        dirty = subprocess.run(["git", "-C", str(REPO), "status", "--porcelain", "--", "show"],
                               capture_output=True, text=True).stdout.strip()
        return {"commit": head, "show_dirty": bool(dirty)}
    except OSError:
        return {"commit": "", "show_dirty": None}


_TOOL_CACHE = None


def _hash_cached(path):
    global _TOOL_CACHE
    p = Path(path)
    if not p.exists():
        return None
    st = p.stat()
    cache_file = SHOW / "build" / "cache" / "toolhash.json"
    if _TOOL_CACHE is None:
        try:
            _TOOL_CACHE = json.loads(cache_file.read_text())
        except (OSError, ValueError):
            _TOOL_CACHE = {}
    k = "%s|%d|%d" % (p.resolve(), st.st_size, int(st.st_mtime))
    if k not in _TOOL_CACHE:
        _TOOL_CACHE[k] = sha256_file(p)
        try:
            cache_file.parent.mkdir(parents=True, exist_ok=True)
            cache_file.write_text(json.dumps(_TOOL_CACHE, indent=0))
        except OSError:
            pass
    return _TOOL_CACHE[k]


def _hash_tree(root, pattern):
    files = sorted(Path(root).glob(pattern)) if Path(root).exists() else []
    if not files:
        return None
    h = hashlib.sha256()
    for f in files:
        h.update(f.name.encode() + b"\0" + f.read_bytes())
    return h.hexdigest()


def tool_hashes():
    exe = None
    for c in ("build/ps1-show/show.exe", "build/ps1-show/SHOW.EXE", "build/ps1-show/show/show.exe"):
        if (SHOW / c).exists():
            exe = SHOW / c
            break
    try:
        ver = subprocess.run([ff.FFMPEG, "-version"], capture_output=True, text=True).stdout.split("\n")[0]
    except OSError:
        ver = ""
    return {
        "show_exe": _hash_cached(exe) if exe else None,
        "emulator": _hash_cached(os.environ.get("SHOW_EMU", "/opt/ps1emu/pcsx-redux")),
        "bios": _hash_cached(os.environ.get("SHOW_BIOS", "/opt/ps1emu/openbios.bin")),
        "harness": _hash_tree(SHOW / "tools" / "showtk" / "emu", "*"),
        "patches": _hash_tree(SHOW / "engine" / "patches", "*.patch"),
        "ffmpeg": ver,
    }


# ---- chapters -----------------------------------------------------------------------
def chapters(tl, f0, f1, min_len=10 * FPS):
    """[(start, end, title)] relative to f0: the compiler's chapters, else the acts (the
    eyecatches sit on the act breaks).  YouTube wants the first at 0:00, >= 3 of them and
    each >= 10 s, so short ones fold into the one before."""
    marks = []
    if tl.chapters:
        for c in tl.chapters:
            marks.append((int(c.get("f", c.get("start", 0))), c.get("title", "")))
    else:
        for a in tl.acts:
            shots = [s for s in tl.shots if s.act == a.get("id")]
            st = a.get("start", shots[0].start if shots else None)
            if st is not None:
                marks.append((int(st), a.get("title") or a.get("id")))
    def fold(seq, end):
        merged = []
        for st, t in sorted(seq):
            if merged and st - merged[-1][0] < min_len:
                continue
            merged.append((st, t))
        while len(merged) > 1 and end - merged[-1][0] < min_len:
            merged.pop()
        return merged
    marks = fold(marks, max(tl.length, f1))         # the episode's chapters first
    marks = [m for m in marks if m[0] < f1]
    if not marks:
        return [(0, f1 - f0, tl.title or tl.episode)]
    # the chapter running at f0 starts the file
    first = [m for m in marks if m[0] <= f0]
    rest = [m for m in marks if m[0] > f0]
    seq = [(0, (first[-1][1] if first else marks[0][1]))] + [(m[0] - f0, m[1]) for m in rest]
    merged = fold(seq, f1 - f0)
    out = []
    for i, (st, t) in enumerate(merged):
        end = merged[i + 1][0] if i + 1 < len(merged) else f1 - f0
        out.append((st, end, t))
    return out


# ---- segments -------------------------------------------------------------------------
def _burnin_spec(tl, seg, masters, tts_count):
    shots = []
    for p in seg.pieces:
        a = p.f0 - seg.f0
        shots.append((p.shot.id, p.shot.beat, a, a + p.n, p.shot.start, masters.badge(p.shot.id)))
    lines = []
    for ln in tl.lines_in(seg.f0, seg.f1):
        if ln.walla:
            continue
        lines.append((ln.id, ln.start - seg.f0, min(ln.end, seg.f1) - seg.f0, ln.tts))
    return {"f0": seg.f0, "shots": shots, "lines": lines, "tts_count": tts_count}


def encode_segments(tl, edl, recipe, masters, tts_count=0):
    segdir = build_dir(tl.episode) / "segments" / recipe
    segdir.mkdir(parents=True, exist_ok=True)
    r = ff.RECIPES[recipe]
    out, made, reused = [], 0, 0
    for seg in edl.segments("beat"):
        spec = _burnin_spec(tl, seg, masters, tts_count) if r["burnins"] else None
        extra = json.dumps(spec, sort_keys=True) + json.dumps(tl.canvas, sort_keys=True) + \
            json.dumps(r, sort_keys=True)
        key = seg.key(recipe, masters, extra)
        path = segdir / ("%s_%s.mp4" % (seg.id, key))
        if not path.exists():
            lst = segdir / ("%s_%s.ffconcat" % (seg.id, key))
            ff.write_ffconcat([p.entry() for p in seg.pieces], lst)
            bi = ff.burnin_filters(spec, r["scale"]) if spec else None
            t0 = time.time()
            ff.encode_segment(lst, path, recipe, tl.canvas, burnin=bi, nframes=seg.n)
            lst.unlink()
            log("  segment %-18s %4d f  encoded in %.1fs" % (seg.id, seg.n, time.time() - t0))
            made += 1
        else:
            reused += 1
        out.append((path, seg.n, key, seg.id))
    return out, made, reused


# ---- QC -------------------------------------------------------------------------------
def qc(mp4, recipe, canvas, nframes):
    """ffprobe facts against the recipe (AT12.1)."""
    r = ff.RECIPES[recipe]
    info = ff.media_info(mp4, count_frames=True)
    v, a = info.get("video", {}), info.get("audio", {})
    want_w, want_h = canvas["w"] * r["scale"], canvas["h"] * r["scale"]
    problems = []
    if (v.get("width"), v.get("height")) != (want_w, want_h):
        problems.append("size %sx%s, want %dx%d" % (v.get("width"), v.get("height"), want_w, want_h))
    if abs(v.get("fps", 0) - FPS) > 1e-6 or abs(v.get("avg_fps", 0) - FPS) > 0.01:
        problems.append("fps %s/%s, want 30" % (v.get("fps"), v.get("avg_fps")))
    if v.get("frames") != nframes:
        problems.append("%s video frames, want %d" % (v.get("frames"), nframes))
    if v.get("codec") != "h264" or v.get("pix_fmt") != "yuv420p":
        problems.append("video %s %s, want h264 yuv420p" % (v.get("codec"), v.get("pix_fmt")))
    if a.get("codec") != "aac" or a.get("sample_rate") != 48000 or a.get("channels") != 2:
        problems.append("audio %s %s Hz %s ch, want aac 48000 Hz stereo" % (
            a.get("codec"), a.get("sample_rate"), a.get("channels")))
    want_s = nframes / FPS
    if abs(v.get("duration", 0) - want_s) > 0.5 / FPS:
        problems.append("video lasts %.3f s, want %.3f s" % (v.get("duration", 0), want_s))
    if abs(a.get("duration", 0) - want_s) > 0.03:
        problems.append("audio lasts %.3f s, want %.3f s" % (a.get("duration", 0), want_s))
    return {"video": v, "audio": a, "duration": info["duration"], "chapters": len(info["chapters"]),
            "problems": problems}


def _sample_frames(edl, n=12):
    pts = {0, edl.n - 1}
    for p in edl.pieces:
        pts.add(p.f0 - edl.f0)
        pts.add(p.f0 - edl.f0 + p.n - 1)
    if edl.n > 2:
        for i in range(n):
            pts.add(int((i + 0.5) * edl.n / n))
    return sorted(x for x in pts if 0 <= x < edl.n)


def master_crops(edl, canvas, frames):
    """Range-relative frames -> the masters' 320x180 RGB pictures (exact)."""
    by_piece = {}
    for i in frames:
        for p in edl.pieces:
            if p.f0 - edl.f0 <= i < p.f0 - edl.f0 + p.n:
                by_piece.setdefault(id(p), (p, []))[1].append(i)
                break
    out = {}
    y0 = canvas["y_off"]
    for p, idx in by_piece.values():
        src = [i - (p.f0 - edl.f0) + p.a for i in idx]
        fr = ff.decode_frames(p.master, src, (canvas["w"], canvas["full_h"]), pix_fmt="rgb24")
        for i, s in zip(idx, src):
            out[i] = fr[s][y0:y0 + canvas["h"]]
    return out


def verify_picture(mp4, edl, canvas, scale, samples=12, tol_mean=3.0, tol_p999=24):
    """AT12.2.  (a) exact: the deliver chain (EDL -> crop -> x scale nearest, before the
    lossy encode) gives framemd5s equal to the masters x scale, frame for frame;
    (b) decoded: each sampled frame of the encode, read at the art-pixel centres, matches
    its own master frame (within the CRF tolerance) better than its neighbours."""
    frames = _sample_frames(edl, samples)
    want = master_crops(edl, canvas, sorted(set(frames + [max(0, i - 1) for i in frames] +
                                                [min(edl.n - 1, i + 1) for i in frames])))
    tmpdir = build_dir(edl.tl.episode) / "deliver" / "_verify"
    tmpdir.mkdir(parents=True, exist_ok=True)
    lst = edl.write(tmpdir / "edl.ffconcat")
    vf = ff.picture_filter(canvas, scale, yuv=False)
    W, H = canvas["w"] * scale, canvas["h"] * scale
    got = ff.decode_frames(lst, frames, (W, H), vf=vf, pix_fmt="rgb24",
                           input_args=["-f", "concat", "-safe", "0"])
    exact_bad = []
    md5s = []
    for i in frames:
        up = np.repeat(np.repeat(want[i], scale, 0), scale, 1)
        a = hashlib.md5(np.ascontiguousarray(got[i]).tobytes()).hexdigest()
        b = hashlib.md5(np.ascontiguousarray(up).tobytes()).hexdigest()
        md5s.append((i, a, b))
        if a != b:
            exact_bad.append(i)
    # decoded the way a player does: by the stream's BT.709 tags (measured on all 256
    # palette colours at qp 0: mean error 1.1, max 3 levels; the untagged BT.601 path: max 26)
    dec = ff.decode_frames(mp4, frames, (W, H), pix_fmt="rgb24", vf=["format=rgb24"])
    c = scale // 2
    worst, align_bad, stats = 0.0, [], []
    for i in frames:
        pic = dec[i][c::scale, c::scale].astype(np.int16)
        err = np.abs(pic - want[i].astype(np.int16))
        m, p999 = float(err.mean()), float(np.percentile(err, 99.9))
        stats.append((i, round(m, 3), p999))
        worst = max(worst, m)
        for j in (i - 1, i + 1):
            if 0 <= j < edl.n and not np.array_equal(want[j], want[i]):
                mj = float(np.abs(pic - want[j].astype(np.int16)).mean())
                if mj <= m:
                    align_bad.append((i, j))
        if m > tol_mean or p999 > tol_p999:
            align_bad.append((i, "err"))
    for f in tmpdir.iterdir():
        f.unlink()
    tmpdir.rmdir()
    return {"frames": frames, "exact_mismatch": exact_bad, "decoded_mismatch": align_bad,
            "worst_mean_err": round(worst, 3), "stats": stats,
            "framemd5": md5s, "ok": not exact_bad and not align_bad}


# ---- the commands -----------------------------------------------------------------------
def range_tag(spec):
    if not spec or spec == "all":
        return ""
    t = spec.replace("..", "-")
    return ("f" + t) if t[:1].isdigit() else t


def find_mix(ep, explicit=None, kinds=("mix_web.wav", "mix_web.flac")):
    if explicit:
        return Path(explicit)
    for k in kinds:
        p = build_dir(ep) / "audio" / k
        if p.exists():
            return p
    return None


def _mix_info(path):
    if not path:
        return None
    info = ff.media_info(path, count_frames=False).get("audio", {})
    return {"path": str(path), "sha256": _hash_cached(path), "sample_rate": info.get("sample_rate"),
            "channels": info.get("channels"), "duration": info.get("duration")}


def assemble(ep, recipe, rng=None, out=None, mix=None, masters_dir=None, allow_silent=False,
             verify=True, timeline=None):
    """The common path of deliver and preview.  Returns the manifest dict."""
    t_start = time.time()
    tl = timeline or load_timeline(ep)
    f0, f1 = tl.range(rng)
    masters = Masters(tl, masters_dir)
    edl = EDL(tl, f0, f1, masters)
    r = ff.RECIPES[recipe]
    tag = range_tag(rng)
    base = "%s_%s%s" % (ep, (tag + "_") if tag else "", r["suffix"])
    out = Path(out) if out else build_dir(ep) / ("previews" if recipe == "preview" else "deliver") / (
        "web" + ("_" + tag if tag else "") if recipe != "preview" else "")
    out.mkdir(parents=True, exist_ok=True)
    mixp = find_mix(ep, mix) if recipe != "preview" else find_mix(ep, mix, (
        "mix_web.wav", "mix_temp.wav", "mix_web.flac"))
    if mixp is None and not allow_silent and recipe != "preview":
        raise DeliverError("%s: no mix (build/%s/audio/mix_web.wav); run `show mix %s` or pass --mix"
                           % (ep, ep, ep))
    if mixp is not None:
        mi = ff.media_info(mixp, count_frames=False).get("audio", {})
        if mi.get("sample_rate") != 48000:
            raise DeliverError("%s: the mix is %s Hz; the show runs on one 48 kHz clock" % (
                mixp, mi.get("sample_rate")))
        if mi.get("duration", 0) * 48000 + 0.5 < f1 * SPF:
            raise DeliverError("%s: the mix ends at sample %d, before the range's end (%d)" % (
                mixp, int(mi.get("duration", 0) * 48000), f1 * SPF))
    tts = [ln for ln in tl.lines_in(f0, f1) if ln.tts and not ln.walla]
    log("%s %s %s: %d f (%s), %d shots, %d segments" % (
        "preview" if recipe == "preview" else "deliver", ep, rng or "all", edl.n, tc(edl.n),
        len(edl.pieces), len(edl.segments())))
    segs, made, reused = encode_segments(tl, edl, recipe, masters, len(tts))
    video = out / (base + ".video.mp4")
    ff.join_segments([(p, n) for p, n, _, _ in segs], video)
    chaps = chapters(tl, f0, f1)
    meta = ff.write_ffmetadata(out / (base + ".ffmeta"), ("%s %s" % (tl.episode, tl.title)).strip(),
                               chaps, comment="DOOMOGACHI LIFE in HELLvile Park!")
    mp4 = out / (base + ".mp4")
    ff.mux(video, mp4, recipe, audio=str(mixp) if mixp else None, smp0=f0 * SPF, smp1=f1 * SPF,
           metadata=meta)
    video.unlink()
    meta.unlink()
    srt_p, vtt_p, cues = captions_mod.write(tl, out / base, f0, f1, ep=ep)
    (out / (base + ".edl.txt")).write_text(edl.table())
    q = qc(mp4, recipe, tl.canvas, edl.n)
    q["warnings"] = ["no mix: silent audio"] if mixp is None else []
    loud = ff.ebur128(mp4) if mixp else {}
    pic = verify_picture(mp4, edl, tl.canvas, r["scale"]) if (verify and recipe != "preview") else None
    shots = [{"id": p.shot.id, "start": p.shot.start, "len": p.shot.len, "in": p.a, "out": p.b,
              "frames_sha256": masters.content_hash(p.shot.id), "badge": masters.badge(p.shot.id)}
             for p in edl.pieces]
    badges = {}
    for s in shots:
        badges[s["badge"] or "unbadged"] = badges.get(s["badge"] or "unbadged", 0) + 1
    render = {
        "episode": ep, "recipe": recipe, "frames": [f0, f1], "fps": FPS, "canvas": tl.canvas,
        "shots": [(s["id"], s["in"], s["out"], s["frames_sha256"]) for s in shots],
        "mix": _hash_cached(mixp) if mixp else None, "mix_window": [f0 * SPF, f1 * SPF],
        "captions": [sha256_file(srt_p), sha256_file(vtt_p)], "chapters": chaps,
        "recipe_params": {k: v for k, v in r.items()},
    }
    manifest = {
        "manifest": 1, "episode": ep, "title": tl.title, "version": "preview" if recipe == "preview"
        else ("web" + ("_" + tag if tag else "")), "range": rng or "all",
        "frames": [f0, f1], "n_frames": edl.n, "duration_s": edl.n / FPS, "fps": FPS,
        "audio_rate": 48000, "source": git_state(), "tools": tool_hashes(),
        "shots": shots, "badges": badges, "audio": _mix_info(mixp),
        "recipe": recipe, "segments": [{"id": i, "frames": n, "key": k} for _, n, k, i in segs],
        "segments_encoded": made, "segments_reused": reused,
        "captions": {"cues": len(cues), "srt": srt_p.name, "vtt": vtt_p.name},
        "chapters": [{"start": a, "end": b, "title": t} for a, b, t in chaps],
        "tts_lines": [ln.id for ln in tts],
        "render_hash": canonical_hash(render),
        "outputs": {"mp4": {"name": mp4.name, "bytes": mp4.stat().st_size},
                    "srt": {"name": srt_p.name, "sha256": sha256_file(srt_p)},
                    "vtt": {"name": vtt_p.name, "sha256": sha256_file(vtt_p)}},
        "qc": {"probe": q, "loudness": loud, "picture": pic},
        "elapsed_s": round(time.time() - t_start, 2),
    }
    (out / (base + ".manifest.json")).write_text(json.dumps(manifest, indent=1, sort_keys=True))
    manifest["_paths"] = {"out": str(out), "mp4": str(mp4), "srt": str(srt_p), "vtt": str(vtt_p),
                          "base": base}
    return manifest


def deliver(ep, version="web", rng=None, out=None, mix=None, masters_dir=None, release=False,
            require=None, allow_silent=False, timeline=None, release_out=None):
    if version != "web":
        raise DeliverError("version %r is deferred to 2027 (sec.12.3); the pilot ships `web`" % version)
    tl = timeline or load_timeline(ep)
    if require == "human-voice":
        f0, f1 = tl.range(rng)
        tts = [ln.id for ln in tl.lines_in(f0, f1) if ln.tts and not ln.walla]
        if tts:
            raise DeliverError("%s: --require human-voice: %d TTS line(s) remain: %s" % (
                ep, len(tts), ", ".join(tts)))
    m = assemble(ep, "web", rng, out, mix, masters_dir, allow_silent=allow_silent and not release,
                 timeline=tl)
    problems = list(m["qc"]["probe"]["problems"])
    pic = m["qc"]["picture"]
    if pic and not pic["ok"]:
        problems.append("picture check failed: exact %s decoded %s" % (
            pic["exact_mismatch"][:5], pic["decoded_mismatch"][:5]))
    loud = m["qc"]["loudness"]
    loud_problems = []
    if loud.get("integrated_lufs") is not None and not (
            WEB_LUFS[0] <= loud["integrated_lufs"] <= WEB_LUFS[1]):
        loud_problems.append("loudness %.1f LUFS, the web window is -16..-14" % loud["integrated_lufs"])
    if loud.get("true_peak_dbtp") is not None and loud["true_peak_dbtp"] > WEB_TP + 0.05:
        loud_problems.append("true peak %.1f dBTP > -1" % loud["true_peak_dbtp"])
    m["qc"]["warnings"] = loud_problems + m["qc"]["probe"].get("warnings", [])
    # the committed manifest (sec.3: show/<ep>/deliveries/<version>/manifest.json)
    committed = ep_dir(ep) / "deliveries" / m["version"] / "manifest.json"
    committed.parent.mkdir(parents=True, exist_ok=True)
    clean = {k: v for k, v in m.items() if not k.startswith("_")}
    clean.pop("elapsed_s", None)
    committed.write_text(json.dumps(clean, indent=1, sort_keys=True) + "\n")
    m["_paths"]["committed_manifest"] = str(committed)
    if problems:
        raise DeliverError("%s: QC failed: %s" % (m["_paths"]["mp4"], "; ".join(problems)))
    if release:
        problems = loud_problems
        from .release import build_package, release_gate
        gate = release_gate(ep, m, timeline=tl)
        if gate or problems:
            raise DeliverError("%s --release refused:\n  %s" % (ep, "\n  ".join(gate + problems)))
        m["_paths"]["release"] = str(build_package(ep, m, timeline=tl, out=release_out))
    return m


def preview(ep, rng=None, out=None, mix=None, masters_dir=None, timeline=None):
    return assemble(ep, "preview", rng, out, mix, masters_dir, allow_silent=True, verify=False,
                    timeline=timeline)


def summary(m):
    q = m["qc"]["probe"]
    v, a = q["video"], q["audio"]
    rows = ["%s  %dx%d %s fps %s f %.3f s | %s %s Hz %sch | %d chapters | %d captions" % (
        m["_paths"]["mp4"], v.get("width"), v.get("height"), v.get("fps"), v.get("frames"),
        v.get("duration", 0), a.get("codec"), a.get("sample_rate"), a.get("channels"),
        len(m["chapters"]), m["captions"]["cues"])]
    rows.append("  segments: %d encoded, %d reused; %.1f s" % (
        m["segments_encoded"], m["segments_reused"], m.get("elapsed_s", 0)))
    if m["qc"].get("loudness"):
        rows.append("  loudness: %s" % m["qc"]["loudness"])
    if m["qc"].get("picture"):
        p = m["qc"]["picture"]
        rows.append("  picture: %s (%d sampled frames exact x%d; decoded mean err <= %.2f)" % (
            "ok" if p["ok"] else "FAIL", len(p["frames"]), ff.RECIPES[m["recipe"]]["scale"],
            p["worst_mean_err"]))
    rows.append("  badges: %s" % ", ".join("%s %d" % kv for kv in sorted(m["badges"].items())))
    if m["tts_lines"]:
        rows.append("  TEMP TTS: %d lines" % len(m["tts_lines"]))
    rows.append("  render hash: %s" % m["render_hash"])
    for w in m["qc"].get("warnings", q.get("warnings", [])) + q["problems"]:
        rows.append("  WARN %s" % w)
    return "\n".join(rows)
