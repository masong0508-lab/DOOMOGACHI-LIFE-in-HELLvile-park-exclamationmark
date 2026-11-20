#!/usr/bin/env python3
# doomo-generator
"""breach_scripts.py - LV10: the Hell Breach level scripts, generated from one list (LEVELS below).

For every level of the list it writes four scripts, so a level cannot ship without all of them:
  core/tests/scripts/levels2_SLUG.txt   the native runner (run_scripts.sh plays it at 240x160 and 320x240):
                                        intro card, control card, first / third person, the autopilot
  gba/harness/scripts/breach_SLUG.txt   the GBA harness (make -C gba test-breach2): the autopilot (LVF_DEMO)
                                        with `sync on`, three fps windows (start room, middle, late), a
                                        vhash-seq (printed, not gated), shots
  ps1/harness/scripts/breach_SLUG.txt   the same, for ps1/harness/run.py (converted by tools/speed2.py ps1:
                                        fps / fpsend windows, the vsyncs-per-frame histogram)
  dc/harness/scripts/breach_SLUG.txt    the same, for dc/harness/dcrun (tools/speed2.py dc)

A new level is one line in LEVELS (LV0's registry will feed it). Window labels are "breach SLUG: WHAT":
tools/breach_fps.py finds them in the harness logs and builds the fps table.

usage: breach_scripts.py [--out-root DIR]     (the make gen / check-gen convention: default the repo root)
"""
import argparse
import contextlib
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import speed2  # noqa: E402  (conv(): the GBA script -> the PS1 / DC one)

LVF_DEMO = 0x1000                     # core/src/levels_int.h: the autopilot plays
LVM_NORMAL = 0

# (level id = the LV_* enum of core/src/levels_int.h, slug = file name part, title)
LEVELS = [
    (0, "vape_cellar", "Vape Cellar"),
    (1, "cloud_nine", "Cloud Nine"),
    (2, "sewers", "Sugar Rush Sewers"),
    (3, "factory", "Factory"),
    (4, "graveyard", "Graveyard"),
    (5, "cathedral", "Cathedral"),
    (6, "underbelly", "Underbelly"),
]

# GBA-listed pilot maps with a registered MAPID (core/src/levels2.c).
# Keep LEVELS as the seven cross-port legacy maps (LV_OLD_COUNT == 7);
# these extra maps participate in the GBA-wide FPS census before their
# separate all-port autopilot scripts are ready.
GBA_3D_PILOTS = [
    (7, "steam_room_panic", "Steam Room Panic"),
    (8, "the_undercroft", "The Undercroft"),
    (12, "boneyard_buffet", "Boneyard Buffet & Bandstand"),
]


# Levels with a map and a script that the registry still lists as "not built" (LV_NO_MAP), in plan order (LEVELS-3).
# Their scripts are NOT written by default (a script that pushes an unbuilt level would fail every harness): when the
# level is registered with its map, move its line into LEVELS (the LV_OLD_COUNT check then asks for it). Until then
# `breach_scripts.py --pending` writes them, so the generator and the harness op lint are proved on them already.
PENDING = [
    (7, "steam_room_panic", "Steam Room Panic"),
    (8, "the_undercroft", "The Undercroft"),
    (10, "glum_pipes", "The Glum Pipes"),
    (11, "the_wall", "The Wall"),
    (12, "boneyard_buffet", "Boneyard Buffet & Bandstand"),
    (14, "department_of_contracts", "The Department of Contracts"),
]

# The three fps windows of every level, as (what, ticks to wait before it). Each window is 130 ticks after the
# wait (the speed tour's rule; the first wait is past the 80-tick intro card). The autopilot walks the level, so the
# middle and the late window land in other rooms. FLOORS: expect-fps floors per (slug, window name); a
# window without one only reports.
WINDOWS = [("start room", 100), ("middle", 500), ("late", 900)]
# first measurement 2026-11-13 (GBA, sync on, the autopilot; gba/build/breach2_*.log): the drawn fps x 0.85,
# rounded down. The gap to 30 is the speed stream's (combat is the one scene the 30 fps rule excepts).
FLOORS = {
    ("vape_cellar", "start room"): 7,
    ("vape_cellar", "middle"): 7,
    ("vape_cellar", "late"): 14,
    ("cloud_nine", "start room"): 3,
    ("cloud_nine", "middle"): 3,
    ("cloud_nine", "late"): 4,
    ("sewers", "start room"): 5,
    ("sewers", "middle"): 4,
    ("sewers", "late"): 9,
    ("factory", "start room"): 4,
    ("factory", "middle"): 4,
    ("factory", "late"): 5,
    ("graveyard", "start room"): 4,
    ("graveyard", "middle"): 9,
    ("graveyard", "late"): 4,
    ("cathedral", "start room"): 4,
    ("cathedral", "middle"): 6,
    ("cathedral", "late"): 9,
    ("underbelly", "start room"): 5,
    ("underbelly", "middle"): 5,
    ("underbelly", "late"): 7,
}

# detail modes (the combat detail option arrives with LV9: its opt ids go here, one run per mode)
DETAIL_MODES = [("", [])]


def level_count(root):
    """LV_OLD_COUNT of core/src/levels_int.h: the levels that are built (ids 0..N-1). The registry also lists
    unbuilt ids (LV_NO_MAP); each gets its scripts when it is built and added to LEVELS. None if the macro is gone."""
    try:
        text = open(os.path.join(root, "core/src/levels_int.h")).read()
    except OSError:
        return None
    m = re.search(r"#define\s+LV_OLD_COUNT\s+(\d+)", text)
    return int(m.group(1)) if m else None


def arg_of(level, demo):
    return level | (LVM_NORMAL << 8) | (LVF_DEMO if demo else 0)


def runner_script(level, slug, title):
    s = ["# generated by tools/breach_scripts.py from its LEVELS list: do not edit (make -C core check-gen).",
         "# %s: intro card, control card, first and third person, then the autopilot (LVF_DEMO) in two rooms." % title,
         "scene TITLE", "wait 2",
         "push BREACH %d" % arg_of(level, False), "expect-scene BREACH", "wait 40",
         "shot %s_intro" % slug, "press A", "wait 20", "shot %s_fp" % slug,
         "press SELECT", "hold RIGHT 6", "wait 4", "shot %s_tp" % slug,
         "scene TITLE", "wait 2",
         "push BREACH %d" % arg_of(level, True), "expect-scene BREACH",
         "wait 300", "shot %s_demo_a" % slug, "wait 600", "shot %s_demo_b" % slug]
    return "\n".join(s) + "\n"


def gba_script(level, slug, title):
    s = ["# generated by tools/breach_scripts.py from its LEVELS list: do not edit (make -C core check-gen).",
         "# %s: the autopilot plays (push BREACH %d = level | LVF_DEMO); sync on makes every run the same ticks." %
         (title, arg_of(level, True)),
         "# Windows rules: speed2_fair.txt. Labels: breach SLUG: WHAT (tools/breach_fps.py).",
         "wait 40", "expect-log doomo: init", "sync on 61"]
    for dname, opts in DETAIL_MODES:
        tag = slug + (" " + dname if dname else "")
        s += ["seed 1", "scene TITLE", "wait 2"]
        s += ["opt %d %d" % o for o in opts]
        s += ["push BREACH %d" % arg_of(level, True)]
        for what, before in WINDOWS:
            s += ["wait %d" % before, "fps start", "wait 130"]
            floor = FLOORS.get((slug, what))
            s += ["%s breach %s: %s" % ("expect-fps %d" % floor if floor else "fps", tag, what),
                  "shot breach_%s_%s" % (slug, what.replace(" ", "_"))]
        s += ["vhash-seq 30 breach %s: hash printed for run-to-run comparison, not gated" % tag]
    return "\n".join(s) + "\n"


def gba_player_census_script():
    """Real player input, no LVF_DEMO: all mapped GBA Breach levels, both cameras.

    An explicit arena-map COMBAT pass also covers the free-play raycaster.
    Each 130-tick window uses identical seed, option and forward input.
    The legacy four-room benchmark remains untouched for trend continuity.
    """
    s = [
        "# Generated by tools/breach_scripts.py: do not edit (core check-gen).",
        "# Balanced quality, actual player movement; legacy + mapped GBA pilot levels, both cameras and arena.",
        "# Report-only FPS: 22 drawn FPS interim, ~30 final, logic must remain >=29.",
        "wait 40", "expect-log doomo: init", "sync on 61", "opt 63 0",
    ]
    for level, slug, _title in LEVELS + GBA_3D_PILOTS:
        for camera in ("first", "third"):
            s += ["seed 1", "scene TITLE", "wait 2", "push BREACH %d" % level,
                  "wait 40", "expect-scene BREACH", "press A", "wait 12"]
            if camera == "third":
                s += ["press SELECT", "wait 12"]
            s += ["fps start", "down UP", "wait 130",
                  "fps census %s %s" % (slug, camera),
                  "expect-logic 29 census %s %s" % (slug, camera),
                  "shot census_%s_%s" % (slug, camera)]
            # Profile AFTER the FPS interval and screenshot: no profiler
            # sampling overhead contaminates the player-controlled window.
            # Focus on the slowest measured cases to keep CI time bounded.
            if (slug, camera) in {
                ("cathedral", "first"), ("cathedral", "third"),
                ("graveyard", "first"), ("sewers", "first"),
                ("cloud_nine", "first"),
            }:
                s.append("profile 9000000")
            s.append("release UP")
    # Start a REAL Arena wave rather than opening the bare MAPID_ARENA
    # through SCENE_COMBAT: the latter never calls arena.c's wave spawner.
    for camera in ("first", "third"):
        s += ["seed 1", "scene TITLE", "wait 2", "push ARENA 0",
              "wait 40", "expect-scene ARENA", "press A", "wait 20"]
        if camera == "third":
            s += ["press SELECT", "wait 12"]
        s += ["fps start", "down UP", "wait 130",
              "fps census arena %s" % camera,
              "expect-logic 29 census arena %s" % camera,
              "shot census_arena_%s" % camera, "release UP"]
    return "\n".join(s) + "\n"


# the commands each harness understands (gba/harness/harness.c, ps1/harness/run.py, dc/harness/dcrun.c,
# core/native/runner_main.c): a generated line outside them is a bug in this file, caught here
OPS = {
    "core": {"scene", "push", "expect-scene", "wait", "shot", "press", "hold", "down", "up", "release"},
    "gba": {"wait", "expect-log", "sync", "seed", "scene", "push", "opt", "fps", "expect-fps", "shot", "vhash-seq", "expect-scene", "press", "down", "release", "expect-logic", "profile"},
    "ps1": {"wait", "seed", "scene", "push", "opt", "fps", "fpsend", "shot"},
    "dc": {"wait", "seed", "scene", "push", "opt", "log", "mark-ticks", "gaps-reset", "frames", "expect-ticks",
           "expect-gap", "shot"},   # (S2-10: speed2.py's dc windows measure the gaps between pictures)
}


def lint(text, target, name):
    for n, line in enumerate(text.splitlines(), 1):
        w = line.split()
        if w and not w[0].startswith("#") and w[0] not in OPS[target]:
            sys.exit("breach_scripts: %s line %d: %r is not a %s harness command" % (name, n, w[0], target))


def convert(gba_text, name, target, tmp):
    """tools/speed2.py's conversion of the GBA script, as text"""
    p = os.path.join(tmp, name)
    with open(p, "w") as f:
        f.write(gba_text)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        speed2.conv(p, target)
    return buf.getvalue()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-root", default=os.path.dirname(HERE))
    ap.add_argument("--pending", action="store_true", help="also write the scripts of the PENDING (unbuilt) levels")
    args = ap.parse_args()
    root = args.out_root
    n = level_count(os.path.dirname(HERE))
    if n is not None and n != len(LEVELS):
        sys.exit("breach_scripts: LEVELS has %d levels but core/src/levels_int.h has LV_OLD_COUNT = %d (built levels): add the new "
                 "level(s) to LEVELS so they get their scripts" % (len(LEVELS), n))
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        for level, slug, title in LEVELS + (PENDING if args.pending else []):
            out = {
                "core/tests/scripts/levels2_%s.txt" % slug: runner_script(level, slug, title),
            }
            g = gba_script(level, slug, title)
            name = "breach_%s.txt" % slug
            out["gba/harness/scripts/" + name] = g
            out["ps1/harness/scripts/" + name] = convert(g, name, "ps1", tmp)
            out["dc/harness/scripts/" + name] = convert(g, name, "dc", tmp)
            for rel, text in out.items():
                lint(text, rel.split("/")[0] if rel[0] != "c" else "core", rel)
                path = os.path.join(root, rel)
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", newline="\n") as f:
                    f.write(text)
        # One player-controlled census grows alongside the built-level registry.
        # Keep it out of breach_*.txt so the autopilot tour cannot ingest it.
        rel = "gba/harness/scripts/3d_census.txt"
        census = gba_player_census_script()
        lint(census, "gba", rel)
        dest = os.path.join(root, rel)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", newline="\n") as f:
            f.write(census)


if __name__ == "__main__":
    main()
