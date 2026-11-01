#!/usr/bin/env python3
"""story_opslock.py - the script rewrite's two locks (P0; docs/STORY_ENGINE.md
"Script tools", docs/design/story/SCRIPT_LOCKS.md).

It compiles core/data/story/**/*.dsc with textpack.py's own compiler (nothing
is written to core/gen) and compares what the rewrite must not break with two
committed lock files next to the .dsc sources:

  save.lock   THE HARD LOCK. Everything a save file or the C code holds by id:
              every pack's id (script ids are pack << 9 | index), the id of
              every script a `later` op can park in STRY.later (old saves
              carry those numbers), the mission rows S100-S217 (STRY.side2
              keeps 2 bits per row), the @possessor rows (the POSSESS op's
              true-name ids) and every SCR_/BSCR_/TXT_/PACK_ name the C code
              uses. Never regenerate it to make a rewrite pass; only a
              change that also bumps STRY (story.h) or the C side may, with
              --write-save and the reason in the commit.

  flow.lock   THE FLOW LOCK. The op stream of every script with the text
              taken out: every op except `say` lines (SAY plus its line/who
              data), text ids masked in choices, stashes and captions (the
              number of options stays), jump targets as positions in that
              stream (labels may be renamed, lines added or cut), the beat
              scripts with their captions masked, the @mission, @armed and
              @possessor rows with their text masked, the named-string NAMEs
              and the pack attributes (kind, chapter, minutes). A rewrite
              that changes only words passes untouched. A rewrite that
              restructures a scene on purpose (DECISIONS: restructure allowed)
              regenerates it with --write and says why in the commit.

    python3 core/tools/story_opslock.py --check            (make -C core story-check)
    python3 core/tools/story_opslock.py --write            (flow.lock only)
    python3 core/tools/story_opslock.py --write-save       (save.lock; see above)
    options: --data DIR (another .dsc tree; its locks live in DIR too),
             --lock-dir DIR (read/write the locks elsewhere)

--check exits 1 and prints, per script, the first op that differs.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textpack as tp  # noqa: E402

C_DIRS = ("core/src", "core/native", "core/tests", "gba/src", "ps1/src")
C_REF = re.compile(r"\b((?:SCR|BSCR|TXT|PACK)_[A-Z0-9_]+)\b")


class TextId(int):
    """a text id handed out by Pack.text: masked in the flow lock"""


_text, _text_new = tp.Pack.text, tp.Pack.text_new
tp.Pack.text = lambda self, s, where: TextId(_text(self, s, where))
tp.Pack.text_new = lambda self, s, where: TextId(_text_new(self, s, where))


class LockCompiler(tp.Compiler):
    def __init__(self, syms):
        super().__init__(syms)
        self.raw = {}                 # script name -> (ops, labels) before resolution
        self.raw_beats = {}

    def compile_script(self, pack, name, body):
        ops, labels = super().compile_script(pack, name, body)
        self.raw[name] = ([list(o) for o in ops], dict(labels))
        return ops, labels

    def compile_beat(self, pack, name, body):
        evs = super().compile_beat(pack, name, body)
        self.raw_beats[name] = list(evs)
        return evs


def operand(v):
    if isinstance(v, TextId):
        return "T"
    if isinstance(v, tuple):
        kind, name = v
        return "%s:%s" % (kind, name)
    return str(v)


def flow_script(ops, labels):
    """the op stream without say lines; jump targets as stream positions"""
    keep = []                          # (raw index, op)
    i = 0
    while i < len(ops):
        op = ops[i]
        if op[0] == "OP_SAY":
            i += 1
            while i < len(ops) and ops[i][0] == "OP_DATA" and ops[i][1] in (tp.D_LINE, tp.D_WHO):
                i += 1
            continue
        keep.append((i, op))
        i += 1
    # raw position -> stream position (a label on a say line points at the next kept op)
    pos = {}
    k = len(keep)
    for j in range(len(ops), -1, -1):
        while k > 0 and keep[k - 1][0] >= j:
            k -= 1
        pos[j] = k
    out = []
    for _, op in keep:
        a, b = op[1], op[2]
        if isinstance(b, tuple) and b[0] == "label":
            b = "@%d" % pos[labels[b[1]]] if b[1] in labels else "@?" + b[1]
        else:
            b = operand(b)
        out.append("%s %s %s" % (op[0][3:], operand(a), b))
    return out


def masked_attrs(a, text_keys, list_keys=()):
    out = []
    for k in sorted(a):
        v = a[k]
        if k in text_keys:
            v = "T" if v else ""
        elif k in list_keys:
            v = "T*%d" % (v.count("|") + 1) if v else ""
        out.append("%s=%s" % (k, v))
    return " ".join(out)


def compile_tree():
    syms = tp.load_symbols()
    comp = LockCompiler(syms)
    files = []
    for d, _, fs in os.walk(tp.DATA):
        for f in fs:
            if f.endswith(".dsc"):
                files.append(os.path.join(d, f))
    for f in sorted(files):
        comp.read(f)
    comp.build()
    return comp


def c_refs(names):
    """the story_ids.h names the C code uses (only names the compiler defines)"""
    found = set()
    for d in C_DIRS:
        root = os.path.join(tp.ROOT, d)
        for dp, _, fs in os.walk(root):
            for f in fs:
                if f.endswith((".c", ".h")):
                    for m in C_REF.finditer(open(os.path.join(dp, f), errors="replace").read()):
                        if m.group(1) in names:
                            found.add(m.group(1))
    return found


def id_names(comp):
    names = {}
    for p in comp.packs:
        names["PACK_%s" % tp.c_ident(p.name).upper()] = p.id
    for n, v in comp.script_names.items():
        if not n.startswith("__"):
            names["SCR_%s" % n.upper()] = v
    for n, v in comp.beat_names.items():
        names["BSCR_%s" % n.upper()] = v
    for n, v in comp.string_names.items():
        names["TXT_%s" % n.upper()] = int(v)
    return names


def save_records(comp, crefs):
    """the hard lock: {key: value} (the key names the thing, the value is what must not change)"""
    rec = {}
    for p in comp.packs:
        rec["pack %s" % p.name] = "id=%d" % p.id
    later = set()
    for name, (ops, _) in comp.raw.items():
        for op in ops:
            if op[0] == "OP_LATER" and isinstance(op[2], tuple):
                later.add(op[2][1])
    for name in sorted(later):
        if name in comp.script_names:
            rec["later %s" % name] = "id=%d" % comp.script_names[name]
    for where, a, pack in comp.missions:
        rec["mission %s" % a.get("id", "?")] = "retired" if a.get("kind") == "none" else "live"
    nxt = 0
    for where, a, pack in comp.possessors:
        r = int(a["row"]) if "row" in a else nxt
        nxt = r + 1
        rec["possessor row=%d" % r] = "host=%s" % a.get("host", "")
    names = id_names(comp)
    for n in sorted(crefs):
        rec["c-ref %s" % n] = "defined"
    return rec


def flow_records(comp):
    rec = {}
    for p in comp.packs:
        rec["pack %s" % p.name] = "kind=%s chapter=%d minutes=%d" % (p.kind, p.chapter, p.minutes)
        rec["strings %s" % p.name] = " ".join(sorted(n for n, _, _ in p.named)) or "-"
        for s in p.scripts:
            if s[0].startswith("__"):
                continue
            ops, labels = comp.raw[s[0]]
            rec["script %s" % s[0]] = flow_script(ops, labels)
        for b in p.beats:
            evs = comp.raw_beats[b[0]]
            rec["beat %s" % b[0]] = ["%d %d %d %s %s" % (e[0], e[1], e[2], operand(e[3]), operand(e[4])) for e in evs]
    for where, a, pack in comp.missions:
        rec["mission %s" % a.get("id", "?")] = masked_attrs(a, ("title", "hint", "trophy"))
    for i, (where, a, pack) in enumerate(comp.possessors):
        rec["possessor %d" % i] = masked_attrs(a, (), ("offer", "clue"))
    for i, (where, a, pack) in enumerate(comp.armed):
        rec["armed %d" % i] = masked_attrs(a, ())
    return rec


def write_lock(path, head, rec):
    with open(path, "w") as f:
        f.write(head)
        for k in sorted(rec):
            v = rec[k]
            if isinstance(v, list):
                f.write("%s {\n" % k)
                for line in v:
                    f.write("  %s\n" % line)
                f.write("}\n")
            else:
                f.write("%s = %s\n" % (k, v))


def read_lock(path):
    rec = {}
    cur = None
    for raw in open(path).read().split("\n"):
        if not raw.strip() or raw.startswith("#"):
            continue
        if cur is not None:
            if raw == "}":
                cur = None
            else:
                rec[cur].append(raw[2:])
            continue
        if raw.endswith(" {"):
            cur = raw[:-2]
            rec[cur] = []
            continue
        k, _, v = raw.partition(" = ")
        rec[k] = v
    return rec


SAVE_HEAD = """# save.lock - THE HARD LOCK (generated by core/tools/story_opslock.py --write-save).
# What a save file or the C code holds by id: pack ids, the ids of scripts a
# `later` op parks in STRY.later, the mission rows, the possessor rows and the
# story_ids.h names the C code uses. Do NOT regenerate it to make a rewrite
# pass: keep the ids (add new scripts at the END of a pack, new packs with a
# name that sorts after the old ones, never delete a later target or a
# mission row). See docs/design/story/SCRIPT_LOCKS.md.
"""
FLOW_HEAD = """# flow.lock - THE FLOW LOCK (generated by core/tools/story_opslock.py --write).
# Every script's ops with the words taken out. A pure text rewrite never
# changes it. A deliberate restructure (DECISIONS: allowed) regenerates it
# with --write and names the scenes and the reason in the commit message.
# See docs/design/story/SCRIPT_LOCKS.md.
"""


def diff(kind, old, new, hard):
    """print the differences; returns the number of failing records"""
    bad = 0
    for k in sorted(set(old) | set(new)):
        o, n = old.get(k), new.get(k)
        if o == n:
            continue
        if hard and o is None:
            continue                  # new things are fine for the save lock
        bad += 1
        if o is None:
            print("%s: new %s (not in the lock)" % (kind, k))
        elif n is None:
            print("%s: %s is gone (the lock has it)" % (kind, k))
        elif isinstance(o, list):
            i = 0
            while i < min(len(o), len(n)) and o[i] == n[i]:
                i += 1
            print("%s: %s: first difference at op %d: lock '%s', now '%s' (%d ops in the lock, %d now)" % (
                kind, k, i, o[i] if i < len(o) else "(end)", n[i] if i < len(n) else "(end)", len(o), len(n)))
        else:
            print("%s: %s: lock '%s', now '%s'" % (kind, k, o, n))
    return bad


def main(argv):
    mode = None
    lock_dir = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("--check", "--write", "--write-save"):
            mode = a[2:]
            i += 1
        elif a == "--data":
            tp.DATA = os.path.abspath(argv[i + 1])
            i += 2
        elif a == "--lock-dir":
            lock_dir = os.path.abspath(argv[i + 1])
            i += 2
        else:
            print(__doc__)
            return 2
    if mode is None:
        print(__doc__)
        return 2
    lock_dir = lock_dir or tp.DATA
    save_path = os.path.join(lock_dir, "save.lock")
    flow_path = os.path.join(lock_dir, "flow.lock")
    comp = compile_tree()
    if mode == "write":
        write_lock(flow_path, FLOW_HEAD, flow_records(comp))
        print("opslock: wrote %s" % os.path.relpath(flow_path, tp.ROOT))
        return 0
    if mode == "write-save":
        write_lock(save_path, SAVE_HEAD, save_records(comp, c_refs(id_names(comp))))
        print("opslock: wrote %s" % os.path.relpath(save_path, tp.ROOT))
        return 0
    status = 0
    for path, kind, hard in ((save_path, "SAVE LOCK", True), (flow_path, "FLOW LOCK", False)):
        if not os.path.exists(path):
            print("%s: %s is missing (run --write%s)" % (kind, path, "-save" if hard else ""))
            status = 1
            continue
        old = read_lock(path)
        if hard:
            # only the lock's own C names are checked: a new C reference to a
            # missing name fails the C build anyway
            new = save_records(comp, [k[6:] for k in old if k.startswith("c-ref ")])
            names = id_names(comp)
            for k in list(new):
                if k.startswith("c-ref ") and k[6:] not in names:
                    del new[k]
        else:
            new = flow_records(comp)
        bad = diff(kind, old, new, hard)
        n = len(old)
        if bad:
            status = 1
            if hard:
                print("%s: %d of %d record(s) broken. Saves and C code hold these by id: restore them "
                      "(docs/design/story/SCRIPT_LOCKS.md); never regenerate save.lock to pass." % (kind, bad, n))
            else:
                print("%s: %d of %d record(s) differ. If the restructure is deliberate, run "
                      "'python3 core/tools/story_opslock.py --write' and name the scenes and the reason "
                      "in the commit message." % (kind, bad, n))
        else:
            print("%s: ok (%d records)" % (kind, n))
    return status


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except tp.Fail as e:
        print("opslock: error: %s" % e, file=sys.stderr)
        sys.exit(1)
