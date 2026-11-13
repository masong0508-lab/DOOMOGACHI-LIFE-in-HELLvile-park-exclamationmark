#!/usr/bin/env python3
"""levelcheck.py - check level maps: budgets, sizes, reachability (owner: levels, LV1).

    python3 tools/levelcheck.py --mapreach core/build/mapreach [FILE.txt ...]

With no FILE: every core/data/maps/*.txt. For each map it

  * compiles the map with tools/mapc.py (every refusal of mapc fails the map:
    zones and paths outside the map, swaps on walls that never open, a
    `dark yes` map without a note, a `gba yes` map that breaks the GBA budgets
    of the levels plan, section 7.2);
  * checks the ROM size (at most 300 KB per level, plan 7.2);
  * runs the reachability search (core/tools/mapreach.c, the code that
    core/tests/test_maps.c runs) and fails if
      - a dungeon's exit cannot be reached from the start,
      - a pickup (health ... candy) cannot be reached,
      - the search overflowed its swap-state limit (the map has too many
        independent swap sets or hooks to prove),
      - the player can get into a state from which no exit is reachable (a
        soft lock: a one-way drop, a swap that walls the way). Format v1
        maps cannot do that; only v2 maps are checked for it.

Prints one line per map. Exit status 0 if every map passes, 1 if not, 2 for
a usage error or a missing mapreach.
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import mapc  # noqa: E402

ROM_MAX = 300 * 1024      # plan 7.2: map, script, text, level art
CELL_BYTES = 8
THING_BYTES = 6


def rom_estimate(m, cells, things):
    """Bytes of the generated map's tables (cells, things and the v2 side tables)."""
    n = len(cells) * CELL_BYTES + len(things) * THING_BYTES
    if m["v2"]:
        n += 24 + 7 * len(m["zones"])
        for p in m["paths"]:
            n += 6 + 2 * len(p["pts"])
        for sw in m["ext"]["swaps"]:
            n += 6 + 10 * len(sw["cells"])
        n += mapc.TEXBANK_NAME * len(m["ext"]["bank"])
        if any(t.flags2 for t in things):
            n += len(things)
    return n


def parse_reach(text):
    """mapreach output -> {name: dict}."""
    out, cur = {}, None
    rows = None
    for line in text.splitlines():
        tok = line.split()
        if not tok:
            continue
        if tok[0] == "map":
            cur = {"w": int(tok[2]), "h": int(tok[3]), "start": None, "exit": (0, 0),
                   "states": 0, "overflow": 0, "stuck": 0, "reach": []}
            out[tok[1]] = cur
            rows = None
        elif cur is None:
            continue
        elif tok[0] == "start":
            cur["start"] = None if tok[1] == "none" else (int(tok[1]), int(tok[2]))
        elif tok[0] == "exit":
            cur["exit"] = (int(tok[1]), int(tok[2]))
        elif tok[0] == "search":
            cur["states"], cur["overflow"] = int(tok[1]), int(tok[3])
        elif tok[0] == "stuck":
            cur["stuck"] = int(tok[1])
            if cur["stuck"]:
                cur["stuck_at"] = (int(tok[2]), int(tok[3]))
        elif tok[0] == "reach":
            rows = cur["reach"]
        elif tok[0] == "end":
            rows = None
        elif rows is not None:
            rows.append(line.strip())
    return out


def check_map(name, m, cells, things, r):
    """The list of plain sentences about what is wrong with the map, empty if it passes."""
    bad = []
    w = r["w"]
    if m["kind"] == "dungeon" and r["start"] is not None:
        if r["exit"][1] == 0:
            bad.append("no exit can be reached from the start")
    if r["overflow"]:
        bad.append("the reachability search overflowed (too many swap sets to prove)")
    if m["v2"] and r["stuck"]:
        bad.append("soft lock: %d reachable states can no longer reach an exit (first at %d,%d)" % (
            r["stuck"], r["stuck_at"][0], r["stuck_at"][1]))
    first, n = None, 0
    for t in things:
        if mapc.ENUMS["THING_HEALTH_SMALL"] <= t.type <= mapc.ENUMS["THING_CANDY"]:
            if r["reach"] and r["reach"][t.y][t.x] != "1":
                n += 1
                first = first or t
    if n:
        bad.append("%d pickups cannot be reached (first: type %d at %d,%d)" % (n, first.type, first.x, first.y))
    return bad


def main(argv):
    mapreach = None
    files = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--mapreach" and i + 1 < len(argv):
            mapreach = argv[i + 1]
            i += 2
            continue
        if a in ("-h", "--help"):
            print(__doc__)
            return 0
        if a.startswith("-"):
            sys.stderr.write("levelcheck: unknown option %s\n" % a)
            return 2
        files.append(os.path.abspath(a))
        i += 1
    if not mapreach or not os.path.isfile(mapreach):
        sys.stderr.write("levelcheck: --mapreach PATH (built by `make -C core levelcheck`) is needed\n")
        return 2
    if not files:
        files = sorted(os.path.join(mapc.MAPS_DIR, f) for f in os.listdir(mapc.MAPS_DIR) if f.endswith(".txt"))
    failed = 0
    maps = {}
    with tempfile.TemporaryDirectory() as tmp:
        exports = []
        for f in files:
            try:
                m = mapc.parse_map(f)
                w, h, cells, things = mapc.build(m)
            except mapc.MapError as e:
                print("FAIL  %s: %s" % (os.path.basename(f), e))
                failed += 1
                continue
            p = os.path.join(tmp, m["name"] + ".mapx")
            with open(p, "w", encoding="ascii", newline="\n") as fh:
                fh.write(mapc.export_text(m, w, h, cells, things))
            exports.append(p)
            maps[m["name"]] = (f, m, cells, things)
        res = {}
        if exports:
            p = subprocess.run([mapreach] + exports, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True)
            if p.returncode != 0:
                sys.stderr.write("levelcheck: mapreach failed: %s\n" % p.stderr.strip())
                return 2
            res = parse_reach(p.stdout)
    for name in sorted(maps):
        f, m, cells, things = maps[name]
        bad = []
        rom = rom_estimate(m, cells, things)
        if rom > ROM_MAX:
            bad.append("%d bytes of tables, at most %d" % (rom, ROM_MAX))
        if name not in res:
            bad.append("mapreach printed nothing for it")
        else:
            bad += check_map(name, m, cells, things, res[name])
        tag = "v2" if m["v2"] else "v1"
        if bad:
            failed += 1
            print("FAIL  %-22s %s: %s" % (name, tag, "; ".join(bad)))
        else:
            r = res[name]
            print("ok    %-22s %s %dx%d  %d B  %d states" % (name, tag, r["w"], r["h"], rom, r["states"]))
    print("levelcheck: %d maps, %d failed" % (len(maps), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
