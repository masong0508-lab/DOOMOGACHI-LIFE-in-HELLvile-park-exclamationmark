#!/usr/bin/env python3
"""part_budget.py - what the creator's parts cost, per slot, today and at the plan's target (CR0).

    python3 tools/part_budget.py                 the per-slot table and the totals
    python3 tools/part_budget.py --slot hat      one slot's parts (value, name, state, method, cost)
    python3 tools/part_budget.py --csv           the table as CSV

Reads the catalog (core/data/parts/catalog/, tools/partcat_build.py), the part scripts
(core/data/parts/<dir>/, tools/parts_build.py: real raw and LZ sizes) and the plan's targets
(slot headers' target= and planned.txt). Prints, per slot:

    kind     gene (a field of the 384-bit gene: 0 B per citizen), deep (a sparse LOOK-pool entry:
             2 B per citizen who wears one, once P0/CR5 land) or species
    width    values the field can hold (gene: 2^bits; deep: 127), and the headroom left
    live / hidden / planned        catalogue rows by state (+ target: new parts the plan budgets)
    script   bytes of part scripts in the tree (raw, as the GBA reads them from ROM)
    ROM+     projected ROM for the target: scripts by method (measured average per method where the
             tree has >= 5 samples, else the plan's estimate) + catalog meta (16 B row + name + index)
             + a 20x20 4bpp icon (200 B)
    PS1+     the same scripts LZ-packed at the tree's measured ratio (disc or SPU RAM, not resident),
             + meta (creator overlay) ; icons go to ICONS.BIN on the disc
    pool     deep slots: bytes per citizen wearing one (2) ; gene slots: 0

Estimates, not gates: the gates are the build checks (parts_build.py, partcat_build.py), the unit
tests (test_part_render.c: distinct, shape gate, render budget, costs) and the console builds'
memory reports (gba memreport, PS1 elf2exe / ovl_plan). Stdlib only.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parts_build      # noqa: E402
import partcat_build    # noqa: E402

PL_DEEP0 = partcat_build.PL_DEEP0
PL_SPECIES = partcat_build.PL_SPECIES
METHODS = partcat_build.METHODS
STATES = partcat_build.STATES
# bytes of ROM per new part by method when the tree has too few samples (CREATOR_PLAN.md 4.2):
# S hand-written script, F family member (macro ops + shared templates), T tool data (a stamp or a
# fabric tile and a short script), P new painter code in C, C a code variant of a painter
PLAN_BYTES = {"S": 120, "F": 48, "T": 64, "P": 400, "C": 40}
META_ROW = 16 + 2          # PartMeta + the name index entry
ICON = 20 * 20 // 2        # a 20x20 4bpp icon


def script_sizes():
    """{(list, value): raw bytes} of every part script, and the tree's LZ ratio"""
    fields = parts_build.field_ids()
    sizes, raw_total, z_total = {}, 0, 0
    for d in parts_build.part_dirs():
        text, n, size, zsize, nblk, minv = parts_build.emit(d, fields)
        raw_total += size
        z_total += zsize
        # per entry: re-parse the dir to get the entry sizes
        p = os.path.join(parts_build.ROOT, "core", "data", "parts", d)
        templates = {}
        for f in sorted(os.listdir(p)):
            if f.endswith(".txt"):
                _, entries, _ = parts_build.parse(os.path.join(p, f), fields, templates)
                for k, blocks in entries.items():
                    sizes[k] = sum(len(b) for b in blocks) + 4
    ratio = (z_total / raw_total) if raw_total else 0.8
    return sizes, ratio


def planned():
    out = []
    path = os.path.join(partcat_build.CAT, "planned.txt")
    for ln, line in partcat_build.lines_of(path):
        t = partcat_build.toks(line)
        a = partcat_build.kv(t[2:], "planned.txt:%d" % ln)
        out.append({"key": t[1][1], "kind": a["kind"], "target": int(a["target"]), "method": a["method"],
                    "name": a.get("name", t[1][1])})
    return out


def rows():
    cat = partcat_build.read_catalog()
    sizes, ratio = script_sizes()
    # measured bytes per method from the scripted parts
    per = {}
    for s in cat["slots"]:
        for p in s["parts"]:
            k = (s["list"], p["value"])
            if k in sizes and STATES[p["state"]] != "test":
                per.setdefault(METHODS[p["method"]], []).append(sizes[k])
    est = dict(PLAN_BYTES)
    for m, v in per.items():
        if len(v) >= 5:
            est[m] = (sum(v) + len(v) - 1) // len(v)
    names_avg = 12
    out = []
    for s in cat["slots"]:
        kind = "species" if s["list"] == PL_SPECIES else ("deep" if s["list"] >= PL_DEEP0 else "gene")
        n = {st: 0 for st in STATES}
        script = 0
        top = 0
        for p in s["parts"]:
            n[STATES[p["state"]]] += 1
            script += sizes.get((s["list"], p["value"]), 0)
            if STATES[p["state"]] != "test":
                top = max(top, p["value"])
        width = 127 if kind == "deep" else min(s["width"], 256)
        tgt = s["target"]
        each = est.get(s["new"], 64)
        out.append({"key": s["key"], "kind": kind, "width": width, "headroom": width - 1 - top if kind != "species" else 0,
                    "live": n["live"], "hidden": n["hidden"], "planned": n["planned"], "target": tgt,
                    "script": script, "method": s["new"],
                    "rom": tgt * (each + META_ROW + names_avg + ICON),
                    "ps1": int(tgt * each * ratio) + tgt * (META_ROW + names_avg),
                    "pool": 2 if kind == "deep" else 0})
    for p in planned():
        each = est.get(p["method"], 64)
        out.append({"key": p["key"], "kind": p["kind"], "width": 127 if p["kind"] == "deep" else 0, "headroom": "-",
                    "live": 0, "hidden": 0, "planned": 0, "target": p["target"], "script": 0, "method": p["method"],
                    "rom": p["target"] * (each + META_ROW + names_avg + ICON),
                    "ps1": int(p["target"] * each * ratio) + p["target"] * (META_ROW + names_avg),
                    "pool": 2 if p["kind"] == "deep" else 0})
    return out, est, ratio, cat


def main(argv):
    out, est, ratio, cat = rows()
    if "--slot" in argv:
        key = argv[argv.index("--slot") + 1]
        sizes, _ = script_sizes()
        for s in cat["slots"]:
            if s["key"] == key:
                print("%s (%s): %d parts" % (s["name"], key, len(s["parts"])))
                for p in s["parts"]:
                    print("  %3d  %-24s %-7s %s  script %4d B  cost %d" % (
                        p["value"], p["name"], STATES[p["state"]], METHODS[p["method"]],
                        sizes.get((s["list"], p["value"]), 0), p["cost"]))
                return 0
        print("no slot %r" % key, file=sys.stderr)
        return 1
    cols = ["key", "kind", "width", "headroom", "live", "hidden", "planned", "target", "method", "script",
            "rom", "ps1", "pool"]
    if "--csv" in argv:
        print(",".join(cols))
        for r in out:
            print(",".join(str(r[c]) for c in cols))
        return 0
    print("%-12s %-7s %5s %5s %5s %6s %5s %6s %3s %7s %8s %8s %4s" % (
        "slot", "kind", "width", "room", "live", "hidden", "plan", "target", "m", "script", "ROM+", "PS1+", "pool"))
    tot = {"live": 0, "hidden": 0, "target": 0, "script": 0, "rom": 0, "ps1": 0}
    for r in out:
        print("%-12s %-7s %5s %5s %5d %6d %5d %6d %3s %7d %8d %8d %4d" % (
            r["key"], r["kind"], r["width"], r["headroom"], r["live"], r["hidden"], r["planned"], r["target"],
            r["method"], r["script"], r["rom"], r["ps1"], r["pool"]))
        for k in tot:
            tot[k] += r[k]
    print("%-12s %-7s %5s %5s %5d %6d %5s %6d %3s %7d %8d %8d" % (
        "TOTAL", "", "", "", tot["live"], tot["hidden"], "", tot["target"], "", tot["script"], tot["rom"], tot["ps1"]))
    gene = sum(r["target"] for r in out if r["kind"] == "gene")
    deep = sum(r["target"] for r in out if r["kind"] == "deep")
    print()
    print("plan target: %d new parts (%d in the gene: 0 B per citizen; %d deep: 2 B per citizen wearing one)"
          % (tot["target"], gene, deep))
    print("bytes per new part by method (measured where >= 5 scripts, else the plan): %s"
          % ", ".join("%s %d" % (m, est[m]) for m in METHODS))
    print("LZ ratio of the tree's scripts: %.2f ; meta %d B + icon %d B per part" % (ratio, META_ROW + 12, ICON))
    print("GBA ROM: +%.1f KB projected (of ~27.7 MB free) ; PS1: +%.1f KB scripts+meta, none resident"
          % (tot["rom"] / 1024.0, tot["ps1"] / 1024.0))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
