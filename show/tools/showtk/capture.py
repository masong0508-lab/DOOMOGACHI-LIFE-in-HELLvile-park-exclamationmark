"""capture.py - run show.exe in PCSX-Redux and turn its frames into per-shot
masters (owner: ENGINE; spec 11.1, WP3).

One call = one emulator boot serving a list of RUNS (seek to shot/frame,
capture N frames). The forked Lua (emu/harness_show.lua) streams, per frame,
the 176-byte g_show_mbox record + 240x320 BGR555 into a FIFO; this process
(on another CPU) reads it and for each frame:
  1. expands BGR555 to exact RGB24 with the palette's (v<<3)|(v>>2) (numpy);
  2. maps the 15-bit values through the palette LUT and counts off-palette
     pixels (picture rows only);
  3. SHA-256 of the raw 240x320 BGR555 frame (STP bit masked);
  4. pipes RGB24 to ffmpeg FFV1 (-level 3 -g 1, intra-only, pix_fmt bgr0),
     one master.mkv per shot;
and writes frames.sha256 + plog.tsv (P1, P4, stats per frame) per shot.

    from showtk import capture
    res = capture.run(cue, mapfile, runs=[capture.Run(shot=0, frame=0, count=600, last=4)],
                      shots=index_from_read_episode, out=dir_per_shot_fn)

A hang or crash raises CaptureError naming the shot and frame (AT3.4).
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from . import errors, paths

HARNESS_VERSION = "show-harness-1"
LUA = Path(__file__).resolve().parent / "emu" / "harness_show.lua"
W, H, Y0, Y1 = 320, 240, 30, 210
FRAME_BYTES = W * H * 2
MBOX = struct.Struct("<IHHHHiiiiII H14H H III IIIII I iiI I II II II 8s I I 4I")
assert MBOX.size == 176, MBOX.size
MBOX_KEYS = ("magic abi canvas_w canvas_h y_off req_shot req_frame cur_shot cur_frame p1 p2 "
             "p4_n " + " ".join(f"p4_{i}" for i in range(14)) + " pad0 cyc_update cyc_draw "
             "cyc_sync prims prim_bytes uploads evictions pool_blocks seq req_episode req_last "
             "state ep_frame nshots total_frames warn_n err_n sprites user0 shot_id load_us "
             "p4_overflow r0 r1 r2 r3").split()
assert len(MBOX_KEYS) == len(MBOX.unpack(bytes(176)))

PLOG_HEADER = ("frame\tep_frame\tp1\tp4\tcyc_update\tcyc_draw\tprims\tprim_bytes\tuploads\t"
               "evictions\tsprites\toffpal")


class CaptureError(errors.ShowError):
    pass


# ---- the palette LUT ------------------------------------------------------
_PAL15 = None


def palette15() -> np.ndarray:
    """The 256 master palette entries as BGR555 (core/src/palette.c, read-only)."""
    global _PAL15
    if _PAL15 is None:
        src = (paths.REPO / "core" / "src" / "palette.c").read_text()
        body = src[src.index("doomo_palette[256] = {"):]
        body = body[:body.index("};")]
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        vals = []
        for m in re.finditer(r"\bC(3?)\s*\(([^)]*)\)", body):
            nums = [int(x, 0) for x in m.group(2).replace("\n", " ").split(",")]
            for i in range(0, len(nums), 3):
                r, g, b = nums[i:i + 3]
                vals.append(((r >> 3) & 31) | (((g >> 3) & 31) << 5) | (((b >> 3) & 31) << 10))
        if len(vals) != 256:
            raise CaptureError(errors.Diag("CAP-PAL", f"parsed {len(vals)} palette entries, not 256"))
        _PAL15 = np.array(vals, dtype=np.uint16)
    return _PAL15


_ONPAL = None


def onpal_lut() -> np.ndarray:
    """bool[32768]: True when a BGR555 value is a palette colour."""
    global _ONPAL
    if _ONPAL is None:
        _ONPAL = np.zeros(32768, dtype=bool)
        _ONPAL[palette15()] = True
    return _ONPAL


def expand_lut() -> np.ndarray:
    """uint8[32768, 3]: BGR555 -> exact RGB24 ((v << 3) | (v >> 2) per channel)."""
    v = np.arange(32768, dtype=np.uint32)
    ch = np.stack([v & 31, (v >> 5) & 31, (v >> 10) & 31], axis=1)
    return ((ch << 3) | (ch >> 2)).astype(np.uint8)


_EXP = None


def to_rgb(frame15: np.ndarray) -> np.ndarray:
    global _EXP
    if _EXP is None:
        _EXP = expand_lut()
    return _EXP[frame15]


# ---- per-shot writer -------------------------------------------------------
class ShotWriter:
    """One shot's master.mkv (FFV1), frames.sha256 and plog.tsv."""

    def __init__(self, out: Path, shot_id: str, encode=True):
        self.out = Path(out)
        self.out.mkdir(parents=True, exist_ok=True)
        self.id = shot_id
        self.n = 0
        self.sha = open(self.out / "frames.sha256.part", "w")
        self.plog = open(self.out / "plog.tsv.part", "w")
        self.plog.write(PLOG_HEADER + "\n")
        self.ff = None
        if encode:
            self.ff = subprocess.Popen(
                ["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                 "-s", f"{W}x{H}", "-r", "30", "-i", "-", "-c:v", "ffv1", "-level", "3", "-g", "1",
                 "-pix_fmt", "bgr0", "-threads", "1", str(self.out / "master.part.mkv")],
                stdin=subprocess.PIPE)
        self.offpal_frames = 0
        self.t0 = time.time()

    def add(self, mb: dict, frame15: np.ndarray, raw_sha: str, offpal: int):
        self.sha.write(f"{raw_sha}  {self.id}+{mb['cur_frame']:04d}\n")
        p4 = ",".join(str(mb[f"p4_{i}"]) for i in range(min(mb["p4_n"], 14)))
        self.plog.write(f"{mb['cur_frame']}\t{mb['ep_frame']}\t{mb['p1']:08x}\t{p4}\t"
                        f"{mb['cyc_update']}\t{mb['cyc_draw']}\t{mb['prims']}\t{mb['prim_bytes']}\t"
                        f"{mb['uploads']}\t{mb['evictions']}\t{mb['sprites']}\t{offpal}\n")
        if offpal:
            self.offpal_frames += 1
        if self.ff:
            self.ff.stdin.write(to_rgb(frame15).tobytes())
        self.n += 1

    def close(self, ok: bool, extra: dict | None = None):
        self.sha.close()
        self.plog.close()
        if self.ff:
            self.ff.stdin.close()
            rc = self.ff.wait()
            ok = ok and rc == 0
        if ok:
            os.replace(self.out / "frames.sha256.part", self.out / "frames.sha256")
            os.replace(self.out / "plog.tsv.part", self.out / "plog.tsv")
            if self.ff:
                os.replace(self.out / "master.part.mkv", self.out / "master.mkv")
            perf = dict(frames=self.n, seconds=round(time.time() - self.t0, 3),
                        offpal_frames=self.offpal_frames, harness=HARNESS_VERSION)
            perf.update(extra or {})
            (self.out / "perf.json").write_text(json.dumps(perf, indent=1) + "\n")
        return ok


# ---- one emulator run --------------------------------------------------------
@dataclass
class Run:
    shot: int
    frame: int
    count: int
    last: int = -1


@dataclass
class Result:
    frames: int = 0
    seconds: float = 0.0
    boot_seconds: float = 0.0
    boot_vsyncs: int = -1        # emulated vsyncs from reset until the player is up
    boot_cpu: float = -1.0       # emulator CPU seconds (unthrottled) until then
    shots: list = field(default_factory=list)       # (shot index, out dir, frames)
    tty: str = ""
    log: str = ""
    last: tuple = (None, None)      # (shot index, frame) of the last frame received


def find_symbol(map_path, name):
    for line in open(map_path):
        p = line.split()
        if len(p) >= 3 and p[0] == name:
            return int(p[2], 16)
    raise CaptureError(errors.Diag("CAP-MAP", f"{name} not in {paths.rel(Path(map_path))} "
                                   "(rebuild show.exe)"))


def run(cue: Path, mapfile: Path, runs: list[Run], shot_ids: list[str], out_for, *,
        work: Path, encode=True, episode=0, timeout=900, hang_vsyncs=600, keep_frames=None,
        emu=None, bios=None) -> Result:
    """Boot the disc once and capture `runs`. out_for(shot_index, run_index) -> the
    directory for that shot's master (a new ShotWriter per shot per run).
    keep_frames: optional dict {(shot, frame): None} filled with RGB arrays (tests)."""
    work = Path(work)
    work.mkdir(parents=True, exist_ok=True)
    fifo = work / "frames.fifo"
    if fifo.exists():
        fifo.unlink()
    os.mkfifo(fifo)
    log_path = work / "harness_show.log"
    env = dict(os.environ)
    env.update(SHOW_HCMD="%x" % find_symbol(mapfile, "g_hcmd"),
               SHOW_MBOX="%x" % find_symbol(mapfile, "g_show_mbox"),
               SHOW_FRAME="%x" % find_symbol(mapfile, "g_loop_frames"),
               SHOW_FIFO=str(fifo), SHOW_LOG=str(log_path), SHOW_EPISODE=str(episode),
               SHOW_HANG=str(hang_vsyncs),
               SHOW_RUNS=";".join(f"{r.shot}:{r.frame}:{r.count}:{r.last}" for r in runs))
    emu = emu or str(paths.EMU)
    bios = bios or str(paths.BIOS)
    argv = ["xvfb-run", "-a", emu, "-cli", "-stdout", "-run", "-fastboot", "-bios", bios,
            "-iso", str(Path(cue).resolve()), "-dofile", str(LUA)]
    for card in ("card1.mcd", "card2.mcd"):
        p = work / card
        if p.exists():
            p.unlink()
    argv += ["-memcard1", str(work / "card1.mcd"), "-memcard2", str(work / "card2.mcd")]
    tty_path = work / "tty.log"
    res = Result()
    t0 = time.time()
    tty = open(tty_path, "wb")
    proc = subprocess.Popen(argv, stdout=tty, stderr=subprocess.STDOUT, env=env, cwd=work)
    killer = threading.Timer(timeout, proc.kill)
    killer.start()

    onpal = onpal_lut()
    writer, writer_key, err = None, None, None
    expect_shot, expect_frame = None, None
    run_i, left = 0, runs[0].count if runs else 0
    fd = os.open(fifo, os.O_RDWR)      # never blocks; EOF is "the emulator exited"
    reader = _FifoReader(fd, proc)
    try:
        first = True
        while True:
            hdr = reader.read(176)
            if hdr is None:
                break
            if first:
                res.boot_seconds = time.time() - t0
                first = False
            pix = reader.read(FRAME_BYTES)
            if pix is None:
                break
            mb = dict(zip(MBOX_KEYS, MBOX.unpack(hdr)))
            mb["shot_id"] = mb["shot_id"].rstrip(b"\0").decode("latin-1")
            if left == 0:
                run_i += 1
                left = runs[run_i].count
            shot, frm = mb["cur_shot"], mb["cur_frame"]
            if mb["magic"] != 0x424D4853:
                err = (f"the frame stream lost sync after {res.frames} frames "
                       f"(read {mb['magic']:08x}, not 'SHMB')")
                break
            if shot < 0:
                err = f"run {run_i}: the player published no frame (state {mb['state']})"
                break
            if (shot, run_i) != writer_key:
                if writer:
                    res.shots.append((writer_key[0], writer.out, writer.n))
                    writer.close(True)
                writer = ShotWriter(out_for(shot, run_i), shot_ids[shot], encode)
                writer_key = (shot, run_i)
            frame15 = np.frombuffer(pix, dtype="<u2").reshape(H, W) & 0x7FFF
            raw_sha = hashlib.sha256(frame15.astype("<u2").tobytes()).hexdigest()
            offpal = int(np.count_nonzero(~onpal[frame15[Y0:Y1]]))
            writer.add(mb, frame15, raw_sha, offpal)
            if keep_frames is not None and (shot, frm) in keep_frames:
                keep_frames[(shot, frm)] = to_rgb(frame15)
            res.frames += 1
            res.last = (shot, frm)
            left -= 1
            if res.frames == sum(r.count for r in runs):
                break
    finally:
        try:
            proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            proc.kill()
        killer.cancel()
        tty.close()
        res.seconds = time.time() - t0
        res.tty = tty_path.read_bytes().decode("latin-1")
        res.log = log_path.read_text() if log_path.exists() else ""
        m = re.search(r"lockstep on vsyncs=(\d+) clock=([\d.]+)", res.log)
        if m:
            res.boot_vsyncs, res.boot_cpu = int(m.group(1)), float(m.group(2))
        os.close(fd)
        fifo.unlink(missing_ok=True)
    problem = _problem(res, runs, err)
    if writer:
        if not problem:
            res.shots.append((writer_key[0], writer.out, writer.n))
        writer.close(not problem)
    if problem:
        raise CaptureError(errors.Diag("CAP-RUN", problem[0], shot=_sid(shot_ids, problem[1]),
                                       frame=problem[2]) if isinstance(problem, tuple) else
                           errors.Diag("CAP-RUN", problem))
    return res


def _sid(ids, i):
    if isinstance(i, str):
        return i
    return ids[i] if i is not None and 0 <= i < len(ids) else None


def _problem(res: Result, runs, err):
    """None, a message, or (message, shot, frame) when a shot/frame is known."""
    want = sum(r.count for r in runs)
    m = re.search(r"HANG: .*making shot (-?\d+) frame (-?\d+)", res.log)
    if m:
        return ("the player hung (no new frame) while making this frame; " + _tail(res.tty),
                int(m.group(1)), int(m.group(2)))
    m = re.search(r"show: E (\S+) ([+-]\d+) \[(\S+)\] (.*)", res.tty)
    if m:
        return (f"[{m.group(3)}] {m.group(4)}", m.group(1), int(m.group(2)))
    # otherwise: the frame after the last one received is the one being made
    ls, lf = res.last
    nxt = (ls, lf + 1) if ls is not None else (runs[0].shot, runs[0].frame) if runs else (None, None)
    for pat in ("Unrecoverable error", "*** PANIC", "Segmentation fault", "Exception"):
        if pat in res.tty:
            return (f"emulator TTY: '{pat}' while making this frame; " + _tail(res.tty),) + nxt
    if err:
        return (err,) + nxt
    if res.frames != want:
        return (f"captured {res.frames} of {want} frames; " + _tail(res.log + res.tty),) + nxt
    return None


def _tail(s, n=600):
    s = s.strip()
    return s[-n:].replace("\n", " | ")


class _FifoReader:
    """Reads exact sizes from the FIFO; None once the emulator has exited and
    nothing is left (we hold a write end ourselves, so EOF never comes)."""

    def __init__(self, fd, proc):
        self.fd, self.proc, self.buf = fd, proc, bytearray()

    def read(self, n):
        import select
        while len(self.buf) < n:
            r, _, _ = select.select([self.fd], [], [], 0.5)
            if r:
                self.buf += os.read(self.fd, max(n - len(self.buf), 1 << 20))
            elif self.proc.poll() is not None:
                return None
        out = bytes(self.buf[:n])
        del self.buf[:n]
        return out


def _read_exact(f, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = f.read(n - len(buf))
        if not chunk:
            return None if not buf else None
        buf += chunk
    return bytes(buf)


def emulator_hashes() -> dict:
    """sha256 of the emulator binary and the BIOS (the render cache key)."""
    out = {}
    for k, p in (("emu", paths.EMU), ("bios", paths.BIOS)):
        h = hashlib.sha256()
        with open(Path(p).resolve(), "rb") as f:
            for blk in iter(lambda: f.read(1 << 20), b""):
                h.update(blk)
        out[k] = h.hexdigest()
    return out


def lua_hash() -> str:
    return hashlib.sha256(LUA.read_bytes() + HARNESS_VERSION.encode()).hexdigest()
