"""showtk.art.cast.host - build and drive the native portrait host (ART track).

The host links the game's portrait engine (core/src/portrait*.c, read-only)
with a build-time COPY of portrait_head.c that carries the show's mouth
override, hunk (d) of SHOW_ENGINE_SPEC.md sec.4.3.  If the ENGINE track's
show/engine/patches/portrait_head.c.patch exists it is applied with
`patch`; otherwise the hunk is inserted by text match (same lines).  A
failed match stops the build with the file name, never the game's build.

    python3 -m showtk.art.cast.host            # build show/build/art_cast/portrait_host
    render(jobs) -> (palette, [ (w, h, bytes) ... ])
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from .catalog import ROOT, CORE

SHOW = os.path.join(ROOT, "show")
BUILD = os.path.join(SHOW, "build", "art_cast")
EXE = os.path.join(BUILD, "portrait_host")
HOST_C = os.path.join(os.path.dirname(os.path.abspath(__file__)), "host", "portrait_host.c")
ENGINE_PATCH = os.path.join(SHOW, "engine", "patches", "portrait_head.c.patch")

HUNK_D = ("#if DOOMO_SHOW\n"
          "    { extern int8_t portrait_show_mouth; if (portrait_show_mouth >= 0) st = portrait_show_mouth; }\n"
          "#endif\n")
ANCHOR_AFTER = "case EXPR_TALK:"
ANCHOR = "    if (phs.h->lod == LOD_TINY) {"

CORE_SRCS = ["palette.c", "resident.c", "catalog.c", "rng.c"]
KIND = {"bust": 0, "figure": 1, "chibi": 2, "fighter": 3, "pet": 4, "gen": 5}
SIZE = {"bust": (48, 56), "figure": (48, 80), "chibi": (16, 24), "fighter": (32, 48), "pet": (32, 32)}


class HostError(Exception):
    pass


def patched_head(dst):
    src = os.path.join(CORE, "src", "portrait_head.c")
    if os.path.exists(ENGINE_PATCH) and shutil.which("patch"):
        r = subprocess.run(["patch", "-s", "-o", dst, src, ENGINE_PATCH],
                           capture_output=True, text=True)
        if r.returncode == 0:
            return "engine patch"
    with open(src) as f:
        text = f.read()
    at = text.find(ANCHOR_AFTER)
    pos = text.find(ANCHOR, at) if at >= 0 else -1
    if pos < 0:
        raise HostError("core/src/portrait_head.c: mouth() no longer matches the show's hunk (d) "
                        "context ('%s' then '%s'); update showtk/art/cast/host.py" % (ANCHOR_AFTER, ANCHOR))
    with open(dst, "w") as f:
        f.write('#line 1 "core/src/portrait_head.c (show copy, hunk d)"\n')
        f.write(text[:pos] + HUNK_D + text[pos:])
    return "text hunk"


def sources():
    src = os.path.join(CORE, "src")
    out = [os.path.join(src, f) for f in sorted(os.listdir(src))
           if f.startswith("portrait") and f.endswith(".c") and f != "portrait_head.c"]
    out += [os.path.join(src, f) for f in CORE_SRCS]
    out += [os.path.join(src, f) for f in sorted(os.listdir(src)) if f.startswith("fixmath") and f.endswith(".c")]
    out.append(os.path.join(CORE, "gen", "art_data.c"))
    return out


def stamp():
    """inputs that force a rebuild when they change"""
    files = sources() + [HOST_C, os.path.join(CORE, "src", "portrait_head.c")]
    inc = os.path.join(CORE, "include", "doomo")
    files += [os.path.join(inc, f) for f in os.listdir(inc)]
    files += [os.path.join(CORE, "src", f) for f in os.listdir(os.path.join(CORE, "src")) if f.endswith(".h")]
    if os.path.exists(ENGINE_PATCH):
        files.append(ENGINE_PATCH)
    return max(os.path.getmtime(f) for f in files)


def build(force=False, quiet=True):
    if not force and os.path.exists(EXE) and os.path.getmtime(EXE) >= stamp():
        return EXE
    gen = os.path.join(BUILD, "gen")
    os.makedirs(gen, exist_ok=True)
    head = os.path.join(gen, "portrait_head.c")
    how = patched_head(head)
    cmd = ["gcc", "-std=c99", "-O2", "-w", "-DDOOMO_SHOW=1",
           "-I" + os.path.join(CORE, "include"), "-I" + os.path.join(CORE, "src"),
           HOST_C, head] + sources() + ["-o", EXE]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise HostError("portrait_host build failed:\n" + r.stderr[-4000:])
    if not quiet:
        print("built %s (%s)" % (os.path.relpath(EXE, ROOT), how))
    return EXE


def job(look_bytes, kind, status=1, expr=0, mouth=-1, anim=0, a=0, b=0, arg0=0):
    if len(look_bytes) != 48:
        raise HostError("a DoomoLook is 48 bytes, got %d" % len(look_bytes))
    rec = bytes(look_bytes) + struct.pack("<BBBBHBBb7x", KIND[kind], status, expr,
                                          0xFF if mouth < 0 else mouth, anim & 0xFFFF,
                                          a & 255, b & 255, arg0)
    assert len(rec) == 64
    return rec


def gen_jobs(seeds, hellborn=False):
    """jobs that return resident_randomize_look(seed) looks (48 bytes each)"""
    return [job(struct.pack("<IB43x", s & 0xFFFFFFFF, 1 if hellborn else 0), "gen") for s in seeds]


def generate(seeds, hellborn=False):
    """the PS1's own crowd look generator, run on the host: [48-byte look]"""
    _, out = render(gen_jobs(seeds, hellborn))
    return [px for (_w, _h, px) in out]


def render(jobs):
    """Render job records; returns (palette [(r,g,b)]*256, [(w, h, bytes)])."""
    exe = build()
    with tempfile.TemporaryDirectory() as td:
        jp, op = os.path.join(td, "jobs.bin"), os.path.join(td, "out.bin")
        with open(jp, "wb") as f:
            f.write(b"".join(jobs))
        r = subprocess.run([exe, jp, op], capture_output=True, text=True)
        if r.returncode != 0:
            raise HostError("portrait_host failed: " + r.stderr)
        with open(op, "rb") as f:
            data = f.read()
    pal = [tuple(data[i * 3:i * 3 + 3]) for i in range(256)]
    pos, out = 768, []
    for _ in jobs:
        w, h = struct.unpack_from("<HH", data, pos)
        pos += 4
        out.append((w, h, data[pos:pos + w * h]))
        pos += w * h
    return pal, out


if __name__ == "__main__":
    build(force="--force" in sys.argv, quiet=False)
