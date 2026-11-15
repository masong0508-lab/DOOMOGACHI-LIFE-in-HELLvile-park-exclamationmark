#!/usr/bin/env python3
"""mkfmv.py - build the Dreamcast boot FMV, INTRO.DMV (DC9C; owner: dreamcast).

    mkfmv.py SOURCE.mp4 OUT.dmv [--fps 15] [--size 320x176] [--rate 22050]
                                [--quality 2.0] [--stats]

The format is dc/src/dmv.h (an intra-only 8x8 DCT codec with 4:2:0 chroma,
byte tokens and the audio interleaved per frame); dc/src/dmv.c decodes it,
dc/src/fmv.c plays it before the game. SOURCE is read with ffmpeg (scaled to
--size, resampled to mono --rate). `make -C dc disc` calls this for
trailer/assets/dippinn_logo.mp4; without ffmpeg the disc is built without an
intro (INTRO_REQUIRED=1 makes that an error).

Also a module: encode(frames, audio, ...) takes RGB frames (numpy uint8
H x W x 3) and mono s16 audio, and reconstruct() decodes a quantised frame the
way an ideal float IDCT would (dc/tests/fmv compares dmv.c against it).
"""
import argparse
import math
import shutil
import struct
import subprocess
import sys

import numpy as np

ZZ = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
      12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
      35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
      58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]
QL = np.array([
    [16, 11, 10, 16, 24, 40, 51, 61], [12, 12, 14, 19, 26, 58, 60, 55],
    [14, 13, 16, 24, 40, 57, 69, 56], [14, 17, 22, 29, 51, 87, 80, 62],
    [18, 22, 37, 56, 68, 109, 103, 77], [24, 35, 55, 64, 81, 104, 113, 92],
    [49, 64, 78, 87, 103, 121, 120, 101], [72, 92, 95, 98, 112, 100, 103, 99]], float)
QC = np.full((8, 8), 20.0)
DCT = np.array([[(math.sqrt(1 / 8) if u == 0 else 0.5) * math.cos((2 * x + 1) * u * math.pi / 16)
                 for x in range(8)] for u in range(8)])
HDR_SIZE = 276


def tables(quality):
    """dequantisers in zigzag order, luma then chroma (u16 each)"""
    out = []
    for q in (QL, QC):
        t = np.clip(np.rint(q * quality), 1, 255).astype(int).reshape(64)
        out.append([int(t[ZZ[i]]) for i in range(64)])
    return out


def rgb_to_planes(rgb):
    """JPEG full-range BT.601, chroma averaged 2x2"""
    f = rgb.astype(float)
    r, g, b = f[..., 0], f[..., 1], f[..., 2]
    y = 0.299 * r + 0.587 * g + 0.114 * b
    cb = -0.168736 * r - 0.331264 * g + 0.5 * b + 128
    cr = 0.5 * r - 0.418688 * g - 0.081312 * b + 128

    def sub(c):
        return (c[0::2, 0::2] + c[1::2, 0::2] + c[0::2, 1::2] + c[1::2, 1::2]) / 4
    cl = lambda p: np.clip(np.rint(p), 0, 255).astype(np.uint8)
    return cl(y), cl(sub(cb)), cl(sub(cr))


def quantise(plane, dq):
    """plane (h, w) -> (blocks_y, blocks_x, 64) levels in zigzag order"""
    h, w = plane.shape
    b = plane.reshape(h // 8, 8, w // 8, 8).transpose(0, 2, 1, 3).astype(float) - 128
    d = np.einsum('ux,abxy,vy->abuv', DCT, b, DCT)        # u vertical, v horizontal
    nat = d.reshape(h // 8, w // 8, 64)
    out = np.zeros(nat.shape, int)
    for i in range(64):
        out[..., i] = np.rint(nat[..., ZZ[i]] / dq[i]).astype(int)
    return out


def reconstruct(levels, dq):
    """quantised levels back to a plane with an ideal float IDCT"""
    hb, wb, _ = levels.shape
    nat = np.zeros((hb, wb, 64))
    for i in range(64):
        nat[..., ZZ[i]] = levels[..., i] * dq[i]
    d = nat.reshape(hb, wb, 8, 8)
    p = np.einsum('ux,abuv,vy->abxy', DCT, d, DCT) + 128
    p = np.rint(np.clip(p, 0, 255)).astype(np.uint8)
    return p.transpose(0, 2, 1, 3).reshape(hb * 8, wb * 8)


def clamp_levels(levels):
    """what the bitstream can hold: AC levels within +-127"""
    out = levels.copy()
    out[..., 1:] = np.clip(out[..., 1:], -127, 127)
    return out


def put_block(out, blk, prev_dc):
    d = int(blk[0]) - prev_dc
    v = ((d << 1) ^ (d >> 63)) & 0xFFFFFFFF
    while True:
        b = v & 0x7F
        v >>= 7
        if v:
            out.append(b | 0x80)
        else:
            out.append(b)
            break
    zeros = 0
    for i in range(1, 64):
        c = int(blk[i])
        if c == 0:
            zeros += 1
            continue
        while zeros >= 8:                 # skip tokens: 8 * r zeros, r 1..7
            r = min(zeros // 8, 7)
            out.append(r << 4)
            zeros -= 8 * r
        if abs(c) <= 7:
            out.append((zeros << 4) | ((2 * c - 1) if c > 0 else (-2 * c)))
        else:
            out.append((zeros << 4) | 15)
            out.append(c & 0xFF)
        zeros = 0
    out.append(0)
    return int(blk[0])


def encode_frame(planes_levels):
    out = bytearray()
    for lv in planes_levels:
        prev = 0
        for a in range(lv.shape[0]):
            for b in range(lv.shape[1]):
                prev = put_block(out, lv[a, b], prev)
    while len(out) % 4:
        out.append(0)
    return bytes(out)


def header(w, h, fps, frames, rate, apr, dqs):
    hd = b"DMV1" + struct.pack("<HHBBHIHH", w, h, fps, 0, frames, rate, apr, 0)
    for t in dqs:
        hd += struct.pack("<64H", *t)
    assert len(hd) == HDR_SIZE
    return hd


def encode(frames, audio, fps=15, rate=22050, quality=2.0):
    """frames: list of RGB uint8 (H, W, 3); audio: mono int16 array. -> bytes"""
    h, w = frames[0].shape[:2]
    apr = rate // fps
    dqs = tables(quality)
    out = bytearray(header(w, h, fps, len(frames), rate, apr, dqs))
    pad = np.zeros(apr * len(frames), np.int16)
    n = min(len(audio), len(pad))
    pad[:n] = audio[:n]
    for i, fr in enumerate(frames):
        y, cb, cr = rgb_to_planes(fr)
        lv = [clamp_levels(quantise(y, dqs[0])), clamp_levels(quantise(cb, dqs[1])),
              clamp_levels(quantise(cr, dqs[1]))]
        vb = encode_frame(lv)
        out += struct.pack("<I", len(vb)) + vb + pad[i * apr:(i + 1) * apr].astype("<i2").tobytes()
    return bytes(out)


def read_source(path, fps, w, h, rate):
    if not shutil.which("ffmpeg"):
        sys.exit("mkfmv: ffmpeg not found (apt install ffmpeg)")
    v = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-vf", "fps=%d,scale=%d:%d:flags=lanczos" % (fps, w, h),
         "-pix_fmt", "rgb24", "-f", "rawvideo", "-"], stdout=subprocess.PIPE, check=True).stdout
    a = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-ar", str(rate), "-ac", "1", "-f", "s16le", "-"],
        stdout=subprocess.PIPE, check=True).stdout
    fb = w * h * 3
    frames = [np.frombuffer(v[i * fb:(i + 1) * fb], np.uint8).reshape(h, w, 3) for i in range(len(v) // fb)]
    return frames, np.frombuffer(a, "<i2")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source")
    ap.add_argument("out")
    ap.add_argument("--fps", type=int, default=15)
    ap.add_argument("--size", default="320x176")
    ap.add_argument("--rate", type=int, default=22050)
    ap.add_argument("--quality", type=float, default=2.0)
    ap.add_argument("--stats", action="store_true")
    a = ap.parse_args()
    w, h = (int(x) for x in a.size.split("x"))
    if w % 16 or h % 16 or w > 320 or h > 240 or 30 % a.fps or a.rate % a.fps:
        sys.exit("mkfmv: size must be multiples of 16 within 320x240, fps a divisor of 30, rate a multiple of fps")
    frames, audio = read_source(a.source, a.fps, w, h, a.rate)
    if not frames:
        sys.exit("mkfmv: no video frames in %s" % a.source)
    data = encode(frames, audio, a.fps, a.rate, a.quality)
    with open(a.out, "wb") as f:
        f.write(data)
    print("mkfmv: %s: %d frames %dx%d at %d fps, audio %d Hz mono, %d bytes (%.1f KB per frame)"
          % (a.out, len(frames), w, h, a.fps, a.rate, len(data), len(data) / len(frames) / 1024))


if __name__ == "__main__":
    main()
