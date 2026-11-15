#!/usr/bin/env python3
"""combat_kit.py - the COMBAT stream's measuring kit: every combat window x every Combat detail mode x every port.

Windows (labels "combat WHERE MODE: WHAT", so tools/speed2.py's tables read them):
  vape fp / vape tp   the fixed Vape Cellar fight (`push COMBAT 3`), first / third person (SELECT toggles it)
  SLUG start / mid / late   a Hell Breach level (tools/breach_scripts.py LEVELS) with the autopilot (LVF_DEMO)
Modes: Balanced (option 63 = 0, the default), Smooth (1), Max detail (2).
Every window is 130 ticks after a settle, like the speed tour. Reports: drawn fps, logic ticks/s, work ms,
frames over 33 ms (tools/speed2.py verdicts). Reports only, never gates.

usage:
  combat_kit.py gen OUTDIR [--levels a,b] [--modes 0,1,2] [--no-levels] [--plats gba,ps1,dc]
      writes OUTDIR/gba/*.txt (harness scripts), OUTDIR/ps1/*.txt, OUTDIR/dc/*.txt (converted by speed2.conv)
  combat_kit.py run gba|ps1|dc OUTDIR [same options]
      gen, then play every script on the platform's harness (run from the repo root after that
      platform's build; GBA needs `make -C gba harness rom`, PS1 ps1/build, DC dc/build + dc/harness)
      and print the speed2 table of each log. GBA also takes --profile N: a second pass with
      `profile N` after the window (needs build/doomogachi.nm; GBA only).
  combat_kit.py table OUTDIR   the tables of all logs under OUTDIR (one per script)
Stdlib only. The generated scripts are build products: do not commit them.
"""
import argparse
import contextlib
import io
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import speed2  # noqa: E402
import breach_scripts  # noqa: E402

MODES = {0: "balanced", 1: "smooth", 2: "max"}
LEVELS = [(lid, slug) for lid, slug, _ in breach_scripts.LEVELS]
WIN = [("start", 100), ("mid", 500), ("late", 900)]


def head(mode):
    return ["wait 40", "expect-log doomo: init", "sync on 61", "seed 1", "opt 63 %d" % mode, "scene TITLE", "wait 2"]


def window(label, settle, shot):
    return ["wait %d" % settle, "fps start", "wait 130", "fps " + label, "shot " + shot]


def vape(mode, tp):
    where = "vape tp" if tp else "vape fp"
    s = head(mode) + ["push COMBAT 3"]
    if tp:
        s += ["wait 2", "press SELECT"]
    s += window("combat %s %s: fight" % (where, MODES[mode]), 200, "combat_%s_%s" % (where.replace(" ", "_"), MODES[mode]))
    return "vape_%s_%s" % ("tp" if tp else "fp", MODES[mode]), s


def level(lid, slug, mode):
    s = head(mode) + ["push BREACH %d" % (lid | breach_scripts.LVF_DEMO)]
    for what, w in WIN:
        s += window("combat %s %s: %s" % (slug, MODES[mode], what), w if what == "start" else (500 if what == "mid" else 900),
                    "combat_%s_%s_%s" % (slug, MODES[mode], what))
    return "%s_%s" % (slug, MODES[mode]), s


def scripts(a):
    modes = [int(m) for m in a.modes.split(",")]
    out = []
    for m in modes:
        out.append(vape(m, False))
        out.append(vape(m, True))
        if not a.no_levels:
            for lid, slug in LEVELS:
                if a.levels and slug not in a.levels.split(","):
                    continue
                out.append(level(lid, slug, m))
    return out


def gen(a):
    plats = a.plats.split(",")
    names = []
    for name, lines in scripts(a):
        names.append(name)
        for p in plats:
            d = os.path.join(a.outdir, p)
            os.makedirs(d, exist_ok=True)
            gba = os.path.join(d, name + ".gba.txt") if p != "gba" else os.path.join(d, name + ".txt")
            with open(gba, "w") as f:
                f.write("\n".join(lines) + "\n")
            if p != "gba":
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    speed2.conv(gba, p)
                os.remove(gba)
                with open(os.path.join(d, name + ".txt"), "w") as f:
                    f.write(buf.getvalue())
    return names


def table(path):
    speed2.main.__globals__["sys"].argv = ["speed2", "table", path]
    speed2.main()


def run(a):
    names = gen(a)
    d = os.path.join(a.outdir, a.plat)
    rc = 0
    for n in names:
        src, log = os.path.join(d, n + ".txt"), os.path.join(d, n + ".log")
        if a.plat == "gba":
            cmd = [os.path.join(ROOT, "gba/build/harness"), os.path.join(ROOT, "gba/build/doomogachi.gba"), "--script", src,
                   "--shots", os.path.join(d, "shots"), "--no-fps-gates", "--worklog", "--quiet"]
            with open(log, "w") as f:
                rc |= subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
            if a.profile:
                prof_script = os.path.join(d, n + ".prof.txt")
                with open(src) as f, open(prof_script, "w") as g:
                    for l in f:
                        g.write(l)
                        if l.startswith("fps combat"):
                            g.write("profile %d\n" % a.profile)
                plog = os.path.join(d, n + ".prof.log")
                with open(plog, "w") as f:
                    subprocess.call(cmd[:3] + [prof_script, "--nm", os.path.join(ROOT, "gba/build/doomogachi.nm")] + cmd[4:],
                                    stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
        elif a.plat == "ps1":
            cmd = ["python3", "ps1/harness/run.py", "--exe", "ps1/build/doomogachi.exe", "--cue", "ps1/build/doomogachi.cue",
                   "--script", src, "--out", os.path.join(d, n), "--timeout", "1800"]
            with open(log, "w") as f:
                rc |= subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
            log = os.path.join(d, n, "tty.log")
        else:
            core = os.environ.get("FLYCAST") or next((p for p in ("dc/harness/build/flycast_libretro.so",
                                                                  "/opt/toolchains/dc/flycast_libretro.so") if os.path.exists(os.path.join(ROOT, p)) or os.path.exists(p)), "")
            cmd = ["dc/harness/build/dcrun", core, "dc/build/doomogachi-nodebug.elf", "--nm", "dc/build/doomogachi.nm", "--fresh",
                   "--run-dir", os.path.join(d, "run"), "--script", src, "--shots", os.path.join(d, n + "_shots"), "--timeout", "1800"]
            with open(log, "w") as f:
                subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
        print("### %s %s (%s)" % (a.plat, n, log))
        sys.stdout.flush()
        if os.path.exists(log):
            table(log)
    return rc


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("cmd", choices=["gen", "run", "table"])
    p.add_argument("args", nargs="+")
    p.add_argument("--levels", default="")
    p.add_argument("--modes", default="0,1,2")
    p.add_argument("--no-levels", action="store_true")
    p.add_argument("--plats", default="gba,ps1,dc")
    p.add_argument("--profile", type=int, default=0)
    a = p.parse_args()
    if a.cmd == "gen":
        a.outdir = a.args[0]
        names = gen(a)
        print("combat_kit: %d scripts x %s in %s" % (len(names), a.plats, a.outdir))
    elif a.cmd == "run":
        a.plat, a.outdir = a.args[0], a.args[1]
        a.plats = a.plat
        return run(a)
    else:
        for root, _, files in os.walk(a.args[0]):
            for f in sorted(files):
                if f.endswith(".log") and not f.endswith(".prof.log") or f == "tty.log":
                    print("### " + os.path.join(root, f))
                    table(os.path.join(root, f))
    return 0


if __name__ == "__main__":
    sys.exit(main())
