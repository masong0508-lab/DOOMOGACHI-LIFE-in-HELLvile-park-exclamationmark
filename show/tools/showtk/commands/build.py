"""show build - build show.exe and the show disc (spec 4.1, 14; AT1.2).

    show build [--preset ps1-show|ps1-show-8mb] [--jobs 2] [--clean]
    show build --variant selftest|selftest-game      (AT1.3 test builds)

Configures show/engine (a stand-alone CMake project) into
show/build/<preset>/ and builds show.exe, show.map (nm, for the harness),
show.ldmap, ram.txt (the spec 8.7 RAM report) and show.bin/.cue.
Variants configure the same preset into show/build/test-<variant>/ with
-DSHOW_SELFTEST=ON (and -DSHOW_PORTRAIT_ORIGINAL=ON for selftest-game).
Python API: build(preset, variant=None, jobs=2) -> the binary dir (Path).
"""
import shutil
import subprocess
import sys

from .. import errors, paths

VARIANTS = {
    "selftest": ["-DSHOW_SELFTEST=ON"],
    "selftest-game": ["-DSHOW_SELFTEST=ON", "-DSHOW_PORTRAIT_ORIGINAL=ON"],
}


def bindir_of(preset="ps1-show", variant=None):
    return paths.BUILD / ("test-" + variant if variant else preset)


def build(preset="ps1-show", variant=None, jobs=2, clean=False, quiet=False, target=None):
    """configure (if needed) and build; raise ShowError on failure"""
    if preset not in paths.PRESETS:
        errors.fail("SHOW-BUILD", "unknown preset %s (have: %s)" % (preset, ", ".join(paths.PRESETS)))
    bdir = bindir_of(preset, variant)
    if clean and bdir.exists():
        shutil.rmtree(bdir)
    env = paths.env_for_build()
    log = []

    def run(cmd, cwd):
        p = subprocess.run(cmd, cwd=str(cwd), env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
        out = p.stdout
        log.append(out)
        if not quiet:
            for line in out.splitlines():
                if ("abicalls" in line or "-msoft-float" in line or "build-id" in line
                        or "Building C object" in line):
                    continue
                print(line)
        return p.returncode, out

    if not (bdir / "build.ninja").exists():
        cmd = ["cmake", "--preset", preset]
        if variant:
            cmd += ["-B", str(bdir)] + VARIANTS[variant]
        rc, out = run(cmd, paths.ENGINE)
        if rc:
            errors.fail("SHOW-BUILD", "cmake configure failed for %s:\n%s" % (paths.rel(bdir), out[-3000:]))
    cmd = ["cmake", "--build", str(bdir), "-j", str(jobs)]
    if target:
        cmd += ["--target", target]
    rc, out = run(cmd, paths.ENGINE)
    if rc:
        # surface the patch failure (AT1.4) or the first compiler error
        lines = [l for l in out.splitlines() if l.startswith("show: ") or ": error:" in l
                 or "FAILED:" in l]
        errors.fail("SHOW-BUILD", "build failed in %s:\n%s" % (paths.rel(bdir), "\n".join(lines[:12]) or out[-3000:]))
    return bdir


def run(args):
    bdir = build(args.preset, args.variant, args.jobs, args.clean)
    ram = bdir / "ram.txt"
    print("show: built %s" % paths.rel(bdir / "show.exe"))
    if ram.exists() and not args.variant:
        sys.stdout.write(ram.read_text())
    return 0


def register(sub):
    p = sub.add_parser("build", help="build show.exe and the show disc (stand-alone CMake project)")
    p.add_argument("--preset", default="ps1-show", choices=paths.PRESETS)
    p.add_argument("--variant", choices=sorted(VARIANTS), help="a test build (AT1.3) in show/build/test-<variant>")
    p.add_argument("--jobs", "-j", type=int, default=2, help="parallel jobs (default 2: the CPUs are shared)")
    p.add_argument("--clean", action="store_true", help="delete the binary dir first")
    p.set_defaults(func=run)
