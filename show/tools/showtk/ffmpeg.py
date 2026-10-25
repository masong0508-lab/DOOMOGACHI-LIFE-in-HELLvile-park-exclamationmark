"""showtk.ffmpeg - ffmpeg/ffprobe wrappers and the sec.12.1 recipes (WP12).

Input is always the per-shot FFV1 masters (320x240, the 320x180 picture on
lines 30-209), joined by an ffconcat EDL.  Every scale is nearest-neighbour
and even (x3 preview, x6 HD), so an art pixel is a whole block and 4:2:0
chroma never straddles one.

    RECIPES["preview"]   x3 960x540, H.264 veryfast CRF 20, burn-ins, AAC 192k
    RECIPES["web"]       x6 1920x1080, H.264 medium CRF 16 tune animation,
                         keyint=60 scenecut=0 (closed GOP per segment), BT.709, AAC 320k

Encodes are checked by decoding (framemd5 / sampled frames), never by bytes.
"""
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

import numpy as np

FFMPEG = os.environ.get("SHOW_FFMPEG", "ffmpeg")
FFPROBE = os.environ.get("SHOW_FFPROBE", "ffprobe")
THREADS = int(os.environ.get("SHOW_FFMPEG_THREADS", "2"))   # 4 CPUs shared: 2 by default
FPS = 30
RATE = 48000

FONTS = ["/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
         "/usr/share/fonts/truetype/freefont/FreeMono.ttf"]

RECIPES = {
    "preview": {
        "scale": 3, "vcodec": ["-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
                               "-x264-params", "keyint=60:scenecut=0"],
        "acodec": ["-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2"],
        "burnins": True, "suffix": "preview",
    },
    "web": {
        "scale": 6, "vcodec": ["-c:v", "libx264", "-preset", "medium", "-crf", "16",
                               "-tune", "animation", "-x264-params", "keyint=60:scenecut=0"],
        "acodec": ["-c:a", "aac", "-b:a", "320k", "-ar", "48000", "-ac", "2"],
        "burnins": False, "suffix": "web_hd",
    },
    # optional overnight (sec.12.1); not used by the pilot's release
    "web_uhd": {
        "scale": 12, "vcodec": ["-c:v", "libx264", "-preset", "medium", "-crf", "16",
                                "-tune", "animation", "-x264-params", "keyint=60:scenecut=0"],
        "acodec": ["-c:a", "aac", "-b:a", "320k", "-ar", "48000", "-ac", "2"],
        "burnins": False, "suffix": "web_uhd",
    },
}
COLOR = ["-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709",
         "-color_range", "tv"]


class FFmpegError(RuntimeError):
    pass


def font():
    for f in FONTS:
        if os.path.exists(f):
            return f
    return ""


def run(args, input_bytes=None, capture=True):
    cmd = [FFMPEG, "-hide_banner", "-nostdin", "-v", "error", "-y"] + [str(a) for a in args]
    if input_bytes is not None:
        cmd.remove("-nostdin")
    p = subprocess.run(cmd, input=input_bytes, stdout=subprocess.PIPE if capture else None,
                       stderr=subprocess.PIPE)
    if p.returncode != 0:
        raise FFmpegError("ffmpeg failed (%d): %s\n%s" % (
            p.returncode, " ".join(cmd)[:600], p.stderr.decode("utf-8", "replace")[-2000:]))
    return p.stdout


def probe(path, count_frames=False):
    import json
    cmd = [FFPROBE, "-v", "error", "-print_format", "json", "-show_format", "-show_streams",
           "-show_chapters"]
    if count_frames:
        cmd.append("-count_frames")
    cmd.append(str(path))
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        raise FFmpegError("ffprobe failed: %s" % p.stderr.decode("utf-8", "replace")[-800:])
    return json.loads(p.stdout)


def _rate(s):
    if not s or s == "0/0":
        return 0.0
    a, _, b = s.partition("/")
    return float(a) / float(b or 1)


def media_info(path, count_frames=True):
    """{'video': {...}, 'audio': {...}, 'duration': s, 'chapters': [...]}"""
    p = probe(path, count_frames=count_frames)
    out = {"duration": float(p.get("format", {}).get("duration", 0) or 0),
           "chapters": p.get("chapters", []), "format": p.get("format", {})}
    for st in p.get("streams", []):
        if st.get("codec_type") == "video" and "video" not in out:
            n = st.get("nb_read_frames") or st.get("nb_frames")
            out["video"] = {"codec": st.get("codec_name"), "width": st.get("width"),
                            "height": st.get("height"), "pix_fmt": st.get("pix_fmt"),
                            "fps": _rate(st.get("r_frame_rate")),
                            "avg_fps": _rate(st.get("avg_frame_rate")),
                            "frames": int(n) if n not in (None, "N/A") else None,
                            "duration": float(st.get("duration", 0) or 0),
                            "color_space": st.get("color_space"),
                            "profile": st.get("profile")}
        elif st.get("codec_type") == "audio" and "audio" not in out:
            out["audio"] = {"codec": st.get("codec_name"),
                            "sample_rate": int(st.get("sample_rate", 0) or 0),
                            "channels": st.get("channels"),
                            "duration": float(st.get("duration", 0) or 0),
                            "bit_rate": st.get("bit_rate")}
    return out


# ---- the EDL as an ffconcat file -------------------------------------------------
def write_ffconcat(entries, path, fps=FPS):
    """entries: [(master_path, in_frame, out_frame)] with out exclusive, frames of that
    master.  Matroska stores millisecond timestamps (frame k is within 0.5 ms of k/fps),
    so in/out points sit 0.6 ms past/before the frame boundary and every entry carries
    its exact duration (no drift across 270 shots)."""
    lines = ["ffconcat version 1.0"]
    for src, a, b in entries:
        lines.append("file '%s'" % str(Path(src).resolve()).replace("'", "'\\''"))
        if a > 0:
            lines.append("inpoint %.6f" % (a / fps + 0.0006))
        lines.append("outpoint %.6f" % (b / fps - 0.0006))
        lines.append("duration %.9f" % ((b - a) / fps))
    Path(path).write_text("\n".join(lines) + "\n")
    return path


def crop_filter(canvas):
    return "crop=%d:%d:0:%d" % (canvas["w"], canvas["h"], canvas["y_off"])


def picture_filter(canvas, scale, yuv=True):
    """crop -> x scale nearest -> BT.709 limited 4:2:0 (the conversion is explicit:
    swscale's default RGB->YUV matrix is BT.601, which would not match the bt709 tags)."""
    w, h = canvas["w"] * scale, canvas["h"] * scale
    vf = ["setpts=N/(%d*TB)" % FPS, crop_filter(canvas), "scale=%d:%d:flags=neighbor" % (w, h)]
    if yuv:
        vf.append("scale=out_color_matrix=bt709:out_range=tv:flags=neighbor+accurate_rnd,"
                  "format=yuv420p")
    return vf


def _esc(text):
    """drawtext text= value inside a filter script."""
    t = "".join(c for c in str(text) if c.isalnum() or c in " _.+-/#()[]")
    return t.replace(":", "\\:")


def burnin_filters(seg, scale, font_path=None):
    """drawtext chain for a preview segment: TC, episode frame, beat, shot +N, line id,
    TEMP TTS, badge.  seg: {'f0', 'shots': [(id, beat, a, b, shot_start, badge)],
    'lines': [(id, a, b, tts)], 'tts_count'} with a/b segment-relative, b exclusive."""
    fp = font_path or font()
    fs = 6 * scale                        # 18 px at x3
    base = "fontfile='%s':fontsize=%d:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=3" % (
        fp, fs)
    out = []
    from .sidecars import tc
    out.append("drawtext=%s:x=6:y=6:timecode='%s':rate=%d" % (
        base, tc(seg["f0"]).replace(":", "\\:"), FPS))
    out.append("drawtext=%s:x=w-tw-6:y=6:text='f %%{eif\\:n+%d\\:d}'" % (base, seg["f0"]))
    for sid, beat, a, b, sstart, badge in seg["shots"]:
        en = "between(n\\,%d\\,%d)" % (a, b - 1)
        rel = seg["f0"] - sstart          # shot frame = n + rel
        out.append("drawtext=%s:x=6:y=h-th-6:enable='%s':text='%s  %s +%%{eif\\:n+%d\\:d}'" % (
            base, en, _esc(beat or ""), _esc(sid), rel))
        if badge:
            out.append("drawtext=%s:x=w-tw-6:y=h-th-6:enable='%s':text='%s'" % (
                base, en, _esc(badge)))
    for lid, a, b, tts in seg["lines"]:
        en = "between(n\\,%d\\,%d)" % (max(a, 0), b - 1)
        label = _esc(lid) + ("  TEMP TTS" if tts else "")
        out.append("drawtext=%s:x=(w-tw)/2:y=h-2*th-14:enable='%s':text='%s'" % (base, en, label))
    if seg.get("tts_count"):
        out.append("drawtext=%s:x=(w-tw)/2:y=6:text='TEMP TTS\\: %d lines'" % (
            base, seg["tts_count"]))
    return out


def encode_segment(ffconcat, out, recipe, canvas, burnin=None, nframes=None):
    """One video-only H.264 segment from an EDL piece (closed GOP: x264's default with
    keyint=60 and scenecut=0, and every segment starts on an IDR)."""
    r = RECIPES[recipe]
    vf = picture_filter(canvas, r["scale"])
    if burnin is not None:
        vf = vf[:-1] + burnin + [vf[-1]]
    script = Path(str(out) + ".vf")
    script.write_text(",".join(vf))
    tmp = Path(str(out) + ".part.mp4")
    args = ["-f", "concat", "-safe", "0", "-i", ffconcat, "-filter_script:v", script,
            "-r", FPS, "-fps_mode", "cfr"]
    if nframes:
        args += ["-frames:v", nframes]
    args += r["vcodec"] + COLOR + ["-threads", THREADS, "-video_track_timescale", "30000",
                                   "-an", "-map_metadata", "-1", "-fflags", "+bitexact",
                                   "-flags:v", "+bitexact", tmp]
    run(args)
    os.replace(tmp, out)
    script.unlink()
    return out


def join_segments(segments, out):
    """segments: [(path, nframes)] -> one video-only MP4 by stream copy."""
    lst = Path(str(out) + ".lst")
    lines = ["ffconcat version 1.0"]
    for p, n in segments:
        lines.append("file '%s'" % str(Path(p).resolve()))
        lines.append("duration %.9f" % (n / FPS))
    lst.write_text("\n".join(lines) + "\n")
    tmp = Path(str(out) + ".part.mp4")
    run(["-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", "-video_track_timescale", "30000",
         "-map_metadata", "-1", tmp])
    os.replace(tmp, out)
    lst.unlink()
    return out


def write_ffmetadata(path, title, chapters, comment=None):
    """chapters: [(start_frame, end_frame, title)] relative to the file start."""
    def esc(s):
        return str(s).replace("\\", "\\\\").replace("=", "\\=").replace(";", "\\;") \
            .replace("#", "\\#").replace("\n", "\\\n")
    lines = [";FFMETADATA1", "title=" + esc(title)]
    if comment:
        lines.append("comment=" + esc(comment))
    for a, b, t in chapters:
        lines += ["[CHAPTER]", "TIMEBASE=1/%d" % FPS, "START=%d" % a, "END=%d" % b,
                  "title=" + esc(t)]
    Path(path).write_text("\n".join(lines) + "\n")
    return path


def mux(video, out, recipe, audio=None, smp0=0, smp1=None, metadata=None):
    """video.mp4 + the mix window [smp0, smp1) -> the deliverable.  No mix -> silence
    of the exact length (previews only; deliver refuses it for a release)."""
    r = RECIPES[recipe]
    nsmp = smp1 - smp0
    args = ["-i", video]
    if audio:
        args += ["-i", audio]
        af = "atrim=start_sample=%d:end_sample=%d,asetpts=N/SR/TB,aresample=48000" % (smp0, smp1)
    else:
        args += ["-f", "lavfi", "-i", "anullsrc=r=48000:cl=stereo"]
        af = "atrim=end_sample=%d,asetpts=N/SR/TB" % nsmp
    if metadata:
        args += ["-f", "ffmetadata", "-i", metadata]
    args += ["-map", "0:v", "-map", "1:a", "-af", af]
    if metadata:
        args += ["-map_metadata", "2", "-map_chapters", "2"]
    else:
        args += ["-map_metadata", "-1"]
    tmp = Path(str(out) + ".part.mp4")
    args += ["-c:v", "copy"] + r["acodec"] + ["-movflags", "+faststart", "-threads", THREADS, tmp]
    run(args)
    os.replace(tmp, out)
    return out


# ---- decoding for checks and review ----------------------------------------------
def decode_frames(path, indices, size, vf=None, pix_fmt="rgb24", input_args=None):
    """Decode the given frame numbers (0-based in decode order) -> {i: HxWx3 uint8}."""
    idx = sorted(set(int(i) for i in indices))
    if not idx:
        return {}
    w, h = size
    sel = "select='%s'" % "+".join("eq(n\\,%d)" % i for i in idx)
    chain = [sel] + (vf or [])
    script = Path(str(path)).parent / (".dec_%s.vf" % hashlib.sha1(
        (str(path) + ",".join(chain)).encode()).hexdigest()[:12])
    try:
        script.write_text(",".join(chain))
    except OSError:
        import tempfile
        script = Path(tempfile.mkstemp(suffix=".vf")[1])
        script.write_text(",".join(chain))
    try:
        raw = run((input_args or []) + ["-i", path, "-filter_script:v", script, "-fps_mode", "passthrough",
                                        "-f", "rawvideo", "-pix_fmt", pix_fmt, "-"])
    finally:
        script.unlink()
    ch = 3 if pix_fmt in ("rgb24", "bgr24") else 4
    arr = np.frombuffer(raw, np.uint8).reshape(-1, h, w, ch)
    if len(arr) != len(idx):
        raise FFmpegError("decoded %d frames, wanted %d from %s" % (len(arr), len(idx), path))
    return {i: arr[k] for k, i in enumerate(idx)}


def decode_all(path, size, vf=None, pix_fmt="rgb24", input_args=None, every=1):
    w, h = size
    chain = list(vf or [])
    if every > 1:
        chain = ["select='not(mod(n\\,%d))'" % every] + chain
    args = (input_args or []) + ["-i", path]
    if chain:
        args += ["-vf", ",".join(chain)]
    raw = run(args + ["-fps_mode", "passthrough", "-f", "rawvideo", "-pix_fmt", pix_fmt, "-"])
    ch = 3 if pix_fmt in ("rgb24", "bgr24") else 4
    return np.frombuffer(raw, np.uint8).reshape(-1, h, w, ch)


def framemd5(path, vf=None, pix_fmt="rgb24", input_args=None):
    """[(frame, md5)] of the decoded frames (sec.11.3: encodes are checked this way)."""
    args = (input_args or []) + ["-i", path]
    if vf:
        args += ["-vf", ",".join(vf)]
    txt = run(args + ["-map", "0:v", "-fps_mode", "passthrough", "-pix_fmt", pix_fmt,
                      "-f", "framemd5", "-"]).decode()
    out = []
    for ln in txt.splitlines():
        if ln.startswith("#") or not ln.strip():
            continue
        parts = [p.strip() for p in ln.split(",")]
        out.append((len(out), parts[-1]))
    return out


def ebur128(path):
    """Integrated loudness and true peak of a file's audio (ffmpeg's ebur128 filter)."""
    p = subprocess.run([FFMPEG, "-hide_banner", "-nostdin", "-i", str(path), "-map", "0:a",
                        "-af", "ebur128=peak=true", "-f", "null", "-"],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    txt = p.stderr.decode("utf-8", "replace")
    tail = txt[txt.rfind("Summary:"):]
    res = {}
    import re
    m = re.search(r"I:\s+(-?[\d.]+|-inf)\s+LUFS", tail)
    if m:
        res["integrated_lufs"] = float(m.group(1)) if m.group(1) != "-inf" else float("-inf")
    m = re.search(r"Peak:\s+(-?[\d.]+|-inf)\s+dBFS", tail)
    if m:
        res["true_peak_dbtp"] = float(m.group(1)) if m.group(1) != "-inf" else float("-inf")
    return res


def have_ffmpeg():
    return shutil.which(FFMPEG) is not None and shutil.which(FFPROBE) is not None
