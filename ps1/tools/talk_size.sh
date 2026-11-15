#!/bin/sh
# TKP0: what the Talk code would cost on the PS1 if it were linked.
# Compiles core/src/talk*.c (and the files they need) with the PS1 compiler flags taken from the
# build's compile_commands.json (-Os, R3000) plus -DDOOMO_HAS_TALK=1, then prints size(1) for each.
# Nothing is linked and no resident byte changes. Run after: cd ps1 && cmake --preset ps1 && cmake --build --preset ps1
# usage: sh ps1/tools/talk_size.sh [OUTDIR]   (default /tmp/talk_size)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-/tmp/talk_size}
mkdir -p "$OUT"
python3 - "$ROOT" "$OUT" <<'PY'
import json, os, re, subprocess, sys
root, out = sys.argv[1], sys.argv[2]
cc = json.load(open(os.path.join(root, "ps1/build/compile_commands.json")))
ref = [e for e in cc if e["file"].endswith("core/src/talk_db.c")][0]
base = re.sub(r" -o \S+ -c \S+$", "", ref["command"])
files = sorted(f for f in os.listdir(os.path.join(root, "core/src")) if f.startswith("talk") or f in ("story_talk.c", "weak_talk.c"))
tot = [0, 0, 0]
print("%-18s %8s %8s %8s" % ("file", "text", "data", "bss"))
for f in files:
    o = os.path.join(out, f[:-2] + ".o")
    r = subprocess.run(base + " -DDOOMO_HAS_TALK=1 -o %s -c %s" % (o, os.path.join(root, "core/src", f)),
                       shell=True, cwd=ref["directory"], capture_output=True, text=True)
    if r.returncode:
        print("%-18s COMPILE FAILED: %s" % (f, r.stderr.strip().splitlines()[0] if r.stderr else "")); continue
    s = subprocess.run(["mipsel-linux-gnu-size", o], capture_output=True, text=True).stdout.splitlines()[1].split()
    t, d, b = int(s[0]), int(s[1]), int(s[2])
    old = f in ("story_talk.c", "weak_talk.c")        # already linked on the PS1 today: listed, not in the total
    print("%-18s %8d %8d %8d%s" % (f, t, d, b, "   (already linked, not in TOTAL)" if old else ""))
    if not old:
        tot[0] += t; tot[1] += d; tot[2] += b
print("%-18s %8d %8d %8d" % ("TOTAL (new files)", *tot))
# what --gc-sections keeps when the scene and the save chunk are the roots (the lex / parse pair only when
# the scene still calls it: a build with the typed line off does not link them)
objs = [os.path.join(out, f[:-2] + ".o") for f in files if f not in ("story_talk.c", "weak_talk.c")]
for name, drop in (("typing on (native, DC)", ()), ("typing off (GBA, PS1 plan)", ("talk_lex.o", "talk_parse.o"))):
    use = [o for o in objs if os.path.basename(o) not in drop]
    r = os.path.join(out, "gc.o")
    p = subprocess.run(["mipsel-linux-gnu-ld", "-r", "--gc-sections", "-u", "SCENE_TALK", "-u", "doomo_save_ext_talk",
                        "-o", r] + use, capture_output=True, text=True)
    if p.returncode:
        print("gc %s: link failed (%s)" % (name, p.stderr.strip().splitlines()[0] if p.stderr else "")); continue
    a = {"text": 0, "rodata": 0, "data": 0, "bss": 0}
    for ln in subprocess.run(["mipsel-linux-gnu-size", "-A", r], capture_output=True, text=True).stdout.splitlines():
        w = ln.split()
        if len(w) < 2 or not w[1].isdigit(): continue
        n = w[0]
        k = "rodata" if "rodata" in n else "text" if n.startswith(".text") else "bss" if "bss" in n else "data" if "data" in n else None
        if k: a[k] += int(w[1])
    print("after gc, %-28s text %6d  rodata %6d  data %4d  bss %5d  = %d B" % (name, a["text"], a["rodata"], a["data"], a["bss"], sum(a.values())))
PY
