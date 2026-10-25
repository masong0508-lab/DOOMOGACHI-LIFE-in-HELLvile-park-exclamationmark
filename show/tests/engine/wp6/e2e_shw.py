#!/usr/bin/env python3
"""e2e_shw.py - WP6 through the REAL player: an EP101.SHW written with
showtk.shw_format + showtk.actors (ShotCast.to_chunk) on a show disc, played
by show.exe's own shot loop (show_game.c -> show_registry -> show_mod_cast /
show_mod_actor / show_mod_crowd), captured with the game's harness.

    python3 show/tests/engine/wp6/e2e_shw.py      (one build + one run, ~2 min)

Three shots: a two-shot with lip flap and a headturn op, a CU bust at px=2
with lip flap, and a 150-citizen crowd (48 looks) with a rippled crowd pop.
Writes <scratch>/show_out/engine/wp6_e2e/e2e_contact.png and returns the TTY.
"""
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "tools"))
sys.path.insert(0, str(HERE))

from showtk import actors as A, paths, shw_format as F      # noqa: E402
import gen_wp6 as G                                          # noqa: E402
import wp6run as W                                           # noqa: E402

BDIR = paths.BUILD / "test-wp6-e2e"
WORK = paths.SHOW_WORK / "wp6" / "e2e"
SKY = [(0, F.pal_ramp(5, 1)), (40, F.pal_ramp(5, 2)), (80, F.pal_ramp(5, 3)), (130, F.pal_ramp(3, 3))]


def shot(chunk_sc, ops, length):
    ch = F.Chunk(bg=F.PAL_BLACK)
    ch.planes.append(F.plane_bands(SKY, id=1, order=0))
    ev = chunk_sc.to_chunk(ch, ops)
    ch.add_beat(ev + [(length, 0, F.BOP_END, 0, 0)])
    return ch


def episode():
    from showtk.art.cast.lookdata import Bible
    G.BIBLE = Bible()
    ep = F.Episode(episode=101, seed=0x101)
    # 1: two-shot, the Mayor says "Perfect." and turns his head
    sc = A.ShotCast(G.BIBLE)
    sc.actor("mayor", "mayor", kind="figure", at=(128, 166), face="R", expr="EXPR_HAPPY")
    sc.actor("nana", "nana", kind="figure", at=(196, 166), face="L")
    sc.mouth("mayor", 6, G.test_mouth("Perfect."))
    ep.add_shot("e2e1", 60, shot(sc, [(30, sc.op("mayor", "headturn", 64, 2))], 60), seed=1)
    # 2: CU Nana, px=2 bust, "Two of everything, dear."
    sc = A.ShotCast(G.BIBLE)
    sc.actor("nana", "nana", kind="bust", at=(160, 180), face="L", px=2, expr="EXPR_HAPPY")
    sc.mouth("nana", 4, G.test_mouth("Two of everything, dear."))
    ep.add_shot("e2e2", 60, shot(sc, [], 60), seed=2)
    # 3: 150 citizens from 48 looks, the Mayor in front; the crowd hops at 20
    sc = A.ShotCast(G.BIBLE)
    sc.crowd("plaza", G.adults48(), 150, form="scatter", seed=7, area=(6, 80, 308, 90), bob=True, flipmix=True)
    sc.actor("mayor", "mayor", kind="figure", at=(160, 176), face="front")
    ep.add_shot("e2e3", 60, shot(sc, [(20, sc.crowd_op("plaza", "pop", "TWT_HOP2"))], 60), seed=3)
    return ep.build()


def build(shw):
    env = paths.env_for_build()
    if BDIR.exists() and not (BDIR / "build.ninja").exists():
        shutil.rmtree(BDIR)
    if not (BDIR / "build.ninja").exists():
        p = subprocess.run(["cmake", "--preset", "ps1-show", "-B", str(BDIR),
                            "-DSHOW_ISO_FILES=EP101.SHW=" + str(shw)], cwd=str(paths.ENGINE), env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if p.returncode:
            raise RuntimeError("configure failed:\n" + p.stdout[-3000:])
    p = subprocess.run(["cmake", "--build", str(BDIR), "-j", "2"], cwd=str(paths.ENGINE), env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError("build failed:\n" + p.stdout[-3000:])


def run(frames=260, step=6):
    WORK.mkdir(parents=True, exist_ok=True)
    shw = WORK / "EP101.SHW"
    shw.write_bytes(episode())
    build(shw)
    out = WORK / "run"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    lines = []
    for f in range(step, frames, step):
        lines += ["wait %d" % step, "shot f%03d" % f]
    (WORK / "e2e.txt").write_text("\n".join(lines) + "\n")
    p = subprocess.run([sys.executable, str(paths.GAME_HARNESS), "--cue", str(BDIR / "show.cue"),
                        "--map", str(BDIR / "show.map"), "--script", str(WORK / "e2e.txt"), "--out", str(out),
                        "--timeout", "600"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    tty = (out / "tty.log").read_text(errors="replace") if (out / "tty.log").exists() else ""
    if p.returncode:
        raise RuntimeError("harness failed:\n" + p.stdout[-2000:] + "\n--- tty ---\n" + tty[-2000:])
    pngs = sorted(out.glob("f*.png"))
    W.contact([(pp, pp.stem) for pp in pngs], paths.SHOW_OUT / "engine" / "wp6_e2e" / "e2e_contact.png",
              cols=6, scale=1, title="WP6 through show.exe's shot loop: EP101.SHW (two-shot, CU px=2, crowd 150)")
    return tty


if __name__ == "__main__":
    t = run()
    errs = [l for l in t.splitlines() if l.startswith("show: E")]
    print("\n".join(l for l in t.splitlines() if l.startswith("show:"))[-3000:])
    sys.exit(1 if errs else 0)
