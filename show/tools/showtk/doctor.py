"""doctor.py - `show doctor`: is this machine ready to make the show? (spec 14, WP0)

Checks, fastest first, and prints one line each (OK / WARN / FAIL):
  python and the libraries the tools import; the command modules load;
  cmake, ninja, the mipsel compiler, mkpsxiso, xvfb-run, ffmpeg (libx264,
  aac, flac, ffv1); the emulator, SDL3, BIOS, SDK and toolchain hashes
  against show/vault.lock; free disk (WARN under 3 GB, FAIL under 2 GB);
  whether show.exe is built.  --models also hashes the TTS/ASR models
  (about 1.2 GB), --boot also boots show.exe to the test chart in the
  emulator and times it.
Exit 1 if anything FAILs.
"""
import importlib
import shutil
import subprocess
import sys
import time

from . import errors, paths

FFMPEG_NEEDS = ("libx264", "aac", "flac", "ffv1")


class Report:
    def __init__(self):
        self.fails = 0
        self.warns = 0

    def line(self, state, what, detail=""):
        if state == "FAIL":
            self.fails += 1
        elif state == "WARN":
            self.warns += 1
        print("doctor: %-4s %s%s" % (state, what, (" - " + detail) if detail else ""))


def _which(name, extra=()):
    p = shutil.which(name)
    if p:
        return p
    for d in extra:
        c = d / name
        if c.exists():
            return str(c)
    return None


def check_python(r):
    v = sys.version_info
    r.line("OK" if v >= (3, 11) else "FAIL", "python %d.%d" % (v.major, v.minor),
           "" if v >= (3, 11) else "need 3.11+ (tomllib)")
    for mod, why in (("numpy", "capture, sheets"), ("PIL", "PNG sheets")):
        try:
            m = importlib.import_module(mod)
            r.line("OK", "python module %s %s" % (mod, getattr(m, "__version__", "")))
        except ImportError:
            r.line("WARN", "python module %s missing" % mod, why)


def check_commands(r):
    from . import cli
    import argparse
    ap = argparse.ArgumentParser(prog="show")
    failed = cli.load_commands(ap.add_subparsers())
    if failed:
        for name, why in failed:
            r.line("FAIL", "command module commands/%s.py" % name, why)
    else:
        r.line("OK", "command modules load (%s)" % ", ".join(sorted(ap._subparsers._group_actions[0].choices)))


def check_programs(r):
    sdk_bin = paths.PSN00BSDK / "bin"
    for name, need in (("cmake", True), ("ninja", True), ("mipsel-linux-gnu-gcc", True),
                       ("mkpsxiso", True), ("xvfb-run", True), ("ffmpeg", True), ("xz", False)):
        p = _which(name, (sdk_bin,))
        r.line("OK" if p else ("FAIL" if need else "WARN"), name, p or "not found")
    ff = _which("ffmpeg")
    if ff:
        out = subprocess.run([ff, "-hide_banner", "-encoders"], stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True).stdout
        missing = [e for e in FFMPEG_NEEDS if (" %s " % e) not in out]
        r.line("FAIL" if missing else "OK", "ffmpeg encoders " + ", ".join(FFMPEG_NEEDS),
               ("missing " + ", ".join(missing)) if missing else "")


def check_vault(r, models):
    from . import vault
    try:
        diags, ok = vault.verify(check_models=models, check_archives=False)
    except errors.ShowError as e:
        for d in e.diags:
            r.line("FAIL", d.format())
        return
    for line in ok:
        r.line("OK", line, "matches vault.lock")
    for d in diags:
        r.line("FAIL" if d.is_error else "WARN", d.format())
    if not models:
        r.line("OK", "models", "not hashed (show doctor --models)")


def check_disk(r):
    free = shutil.disk_usage(str(paths.SCRATCH if paths.SCRATCH.exists() else paths.REPO)).free / 1e9
    state = "FAIL" if free < 2 else "WARN" if free < 3 else "OK"
    r.line(state, "disk %.1f GB free" % free, "the brief stops work under 3 GB" if state != "OK" else "")


def check_build(r, boot):
    exe = paths.build_dir() / "show.exe"
    if not exe.exists():
        r.line("WARN", "show.exe not built", "show build")
        return
    ram = paths.build_dir() / "ram.txt"
    left = ""
    if ram.exists():
        for line in ram.read_text().splitlines():
            if line.startswith("left"):
                left = " ".join(line.split()[1:3])
    r.line("OK", "show.exe built", "%s left below the stack top" % left if left else "")
    if not boot:
        return
    out = paths.SHOW_WORK / "doctor-boot"
    t0 = time.time()
    p = subprocess.run([sys.executable, str(paths.GAME_HARNESS), "--cue", str(paths.build_dir() / "show.cue"),
                        "--map", str(paths.build_dir() / "show.map"),
                        "--script", str(paths.SHOW / "tests" / "engine" / "chart.txt"),
                        "--out", str(out), "--timeout", "120", "--emu", str(paths.EMU), "--bios", str(paths.BIOS)],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    dt = time.time() - t0
    r.line("OK" if p.returncode == 0 else "FAIL", "emulator boots show.exe to the test chart",
           "%.1f s for 300 frames (throttled game harness)" % dt if p.returncode == 0 else p.stdout[-400:])


def run(models=False, boot=False):
    r = Report()
    check_python(r)
    check_commands(r)
    check_programs(r)
    check_vault(r, models)
    check_disk(r)
    check_build(r, boot)
    print("doctor: %s (%d fail, %d warn)" % ("FAIL" if r.fails else "PASS", r.fails, r.warns))
    return 1 if r.fails else 0
