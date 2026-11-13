#!/usr/bin/env python3
"""breach_fps.py - LV10: one fps table for the Hell Breach levels from the harness logs.

usage: breach_fps.py [--strict] [gba=LOG ...] [ps1=tty.log ...] [dc=LOG ...]
  gba=LOG   a GBA harness log (gba/build/harness ... --worklog --quiet > LOG; make -C gba test-breach2 writes one)
  ps1=LOG   a PS1 run.py OUT/tty.log of a ps1/harness/scripts/breach_*.txt run
  dc=LOG    a dcrun log of a dc/harness/scripts/breach_*.txt run
Prints, per platform, tools/speed2.py's window table cut down to the "breach SLUG: WHAT" windows (the labels
tools/breach_scripts.py writes), so a level's report is: platform x level x window (drawn fps, logic ticks/s,
worst frame). A level is done at a steady 30 or with its gap reported. --strict exits 1 when any window is
MISS (under 29.5 fps) or "hitch" (a frame over 33.4 ms): the autopilot fights, and combat is the one
exception to the 30 fps rule, so only the report matters there. Stdlib only.
"""
import contextlib
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import speed2  # noqa: E402

TABLES = {"gba": speed2.table_gba, "ps1": speed2.table_ps1, "dc": speed2.table_dc}


def rows_of(plat, path):
    """(header lines, [row lines]) of the breach windows of one log"""
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        TABLES[plat](path)
    lines = buf.getvalue().splitlines()
    # a PS1 window's tag is the label with non-letters turned into "_" (breach_sewers_start_room)
    keep = [l for l in lines[2:] if re.match(r"\| breach[ _]", l)]
    return lines[:2], keep


def main():
    strict = "--strict" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--strict"]
    if not args:
        print(__doc__)
        return 2
    bad = 0
    for a in args:
        plat, _, path = a.partition("=")
        if plat not in TABLES or not path:
            print("breach_fps: bad argument %r (gba=LOG, ps1=LOG, dc=LOG)" % a, file=sys.stderr)
            return 2
        head, rows = rows_of(plat, path)
        print("### %s (%s)\n" % (plat, path))
        if not rows:
            print("(no breach windows in this log)\n")
            bad += 1
            continue
        print("\n".join(head + rows) + "\n")
        bad += sum(1 for r in rows if r.rstrip().endswith(("| MISS |", "| hitch |")))
    return 1 if strict and bad else 0


if __name__ == "__main__":
    sys.exit(main())
