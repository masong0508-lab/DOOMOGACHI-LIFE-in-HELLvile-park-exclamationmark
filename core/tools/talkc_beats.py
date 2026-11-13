"""talkc_beats.py - the Talk drama manager's beat table for talkc.py (owner:
TK, T6; docs/TALK.md "The drama manager"; design 5.2, 5.3). Imported by
talkc.py; stdlib only.

Input: core/data/talk/beats.tlk. The beats' LINES are ordinary grammar
rules (grammar/<lang>/beats_lines.tlk: beat.<name>, guarded beat=<name>);
this file is the table core/src/talk_dm.c reads: when a beat may start,
what it does to the room's tension, how long it runs, which of the
player's acts play along with it.

SYNTAX
  # comment
  @beat NAME            one of talk.h TBEAT_* (lower case); every beat once
    pre: P              its precondition (talk.h TBP_*, lower case): default
                        start like house news problem rivals clash story
                        lull kid romance late
    tension: N          added to the room's tension when the beat starts
                        (-3..3), or `zero`: the tension goes to 0
    after: N            added when its goals are done or it is resolved
    goals: N            goal lines the residents try to get in (1..4); a
                        player act the beat handles counts as one too
    turns: N            player turns it runs at most (1..6)
    bonus: N            the score of its precondition holding (0..12)
    involves: W         all | a | pair  (who gets the "beat involves me"
                        engagement: everyone, the one it is about, the two)
    handles: C ...      act classes (talk.h TAC_*, lower case) that play
                        along; any other act is a mix-in: answered by the
                        global reply and the beat goes on
    hurt: C ...         act classes that hurt the one the beat is about
                        (PROBLEM: an insult; the others turn on the player)
    lean: C ... +N      act classes of the player that raise its score by N
    lean_act: A ...     up to 2 single acts that raise it too (insult)
    resolve: A ...      up to 4 acts that resolve it at once (PROBLEM:
                        comfort, encourage, offer, sympathize)
    flags: F ...        kidsafe (the room is a kids' room for the rest of
                        the talk), adults (never with a minor or the pet:
                        talk_dm.c checks that itself whatever is written)
  @affinity TYPE favours: ROOT dull: ROOT
                        the persona type (0..15, catalog.c order) and the
                        topic roots (talk.h TCR_*, lower case) it warms to
                        or finds dull (design 5.3: +1 / -1 affinity)

SAFETY (refused here; talk_dm.c enforces the same in C whatever the table
says): romance_adult must carry `pre: romance` and `flags: adults`;
kid_moment `pre: kid` and `flags: kidsafe`; no beat but romance_adult may
lean on or handle the romance class; an affinity root may not be romance,
safety or describe.

COMPILED (the TB_BEATS bank, little endian):
  header 12 B: u32 'TBT1', u16 nbeats (TBEAT_COUNT; row 0 unused), u16 row
    size (24), u16 ntypes (16), u16 0
  rows   nbeats x 24 B {u8 pre, i8 tension (127 = to zero), i8 after, u8
         goals, u8 turns, u8 bonus, u8 involves, u8 lean_bonus, u8 flags,
         u8 0, u16 handles, u16 hurt, u16 lean (bit = a TAC_*), u8
         resolve[4] (TA_*), u8 lean_act[2] (TA_*), u16 0}
  types  ntypes x 2 B {u8 favours (TCR_*), u8 dull (TCR_*)}
"""
import os
import re

BEATS_MAGIC = 0x31544254             # "TBT1"
ROW = 24
TO_ZERO = 127
INVOLVES = {"all": 0, "a": 1, "pair": 2}
FLAGS = {"kidsafe": 1, "adults": 2}
NTYPES = 16
BAD_ROOTS = ("romance", "safety", "describe", "none")


def _u16(v):
    return (v & 0xFFFF).to_bytes(2, "little")


def _u32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def parse(text, where, die):
    beats, order, aff = {}, [], {}
    cur = None
    for ln, raw in enumerate(text.split("\n"), 1):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        w = "%s:%d" % (where, ln)
        m = re.match(r"^@beat\s+([a-z_]+)\s*$", line)
        if m:
            if m.group(1) in beats:
                die(w, "beat %s twice" % m.group(1))
            cur = beats[m.group(1)] = {"where": w}
            order.append(m.group(1))
            continue
        m = re.match(r"^@affinity\s+(\d+)\s+favours:\s*([a-z]+)\s+dull:\s*([a-z]+)\s*$", line)
        if m:
            t = int(m.group(1))
            if t in aff:
                die(w, "affinity %d twice" % t)
            aff[t] = (m.group(2), m.group(3), w)
            cur = None
            continue
        m = re.match(r"^\s+([a-z_]+):\s*(.*\S)\s*$", line)
        if not m or cur is None:
            die(w, "not @beat, @affinity or an indented `key: value` of a beat")
        if m.group(1) in cur:
            die(w, "%s twice" % m.group(1))
        cur[m.group(1)] = m.group(2)
    return beats, order, aff


def build(text, where, env, die):
    """env: tbeat {name: id}, nbeats, tbp {name: id}, tac [names], acts {NAME: id}, tcr {name: id}"""
    beats, order, aff = parse(text, where, die)
    want = sorted((k for k in env["tbeat"] if k != "none"), key=lambda k: env["tbeat"][k])
    for b in order:
        if b not in want:
            die(beats[b]["where"], "beat %s is not a talk.h TBEAT_* (%s)" % (b, " ".join(want)))
    miss = [b for b in want if b not in beats]
    if miss:
        die(where, "no @beat for: %s" % ", ".join(miss))
    rows = bytearray(ROW * env["nbeats"])
    keys = {"pre", "tension", "after", "goals", "turns", "bonus", "involves", "handles", "hurt", "lean",
            "lean_act", "resolve", "flags", "where"}

    def num(b, k, lo, hi, dflt=None):
        v = beats[b].get(k)
        if v is None:
            if dflt is None:
                die(beats[b]["where"], "beat %s needs %s:" % (b, k))
            return dflt
        if not re.match(r"^[+-]?\d+$", v) or not lo <= int(v) <= hi:
            die(beats[b]["where"], "beat %s: %s: %s (a number %d..%d)" % (b, k, v, lo, hi))
        return int(v)

    def classes(b, k, words):
        m = 0
        for c in words:
            if c not in env["tac"]:
                die(beats[b]["where"], "beat %s: %s: %s is not an act class (%s)" % (b, k, c, " ".join(env["tac"])))
            m |= 1 << env["tac"].index(c)
        return m

    def acts(b, k, cap):
        out = []
        for a in beats[b].get(k, "").split():
            if a.upper() not in env["acts"]:
                die(beats[b]["where"], "beat %s: %s: %s is not a talk.h TA_* act" % (b, k, a))
            out.append(env["acts"][a.upper()])
        if len(out) > cap:
            die(beats[b]["where"], "beat %s: %s: at most %d acts" % (b, k, cap))
        return out + [0] * (cap - len(out))

    rom = 1 << env["tac"].index("romance")
    saf = 1 << env["tac"].index("safety")
    for b in want:
        d = beats[b]
        for k in d:
            if k not in keys:
                die(d["where"], "beat %s: unknown key %s" % (b, k))
        pre = d.get("pre")
        if pre not in env["tbp"]:
            die(d["where"], "beat %s: pre: %s (one of %s)" % (b, pre, " ".join(sorted(env["tbp"], key=env["tbp"].get))))
        tv = d.get("tension")
        tension = TO_ZERO if tv == "zero" else num(b, "tension", -3, 3)
        after = num(b, "after", -3, 3, 0)
        goals = num(b, "goals", 1, 4)
        turns = num(b, "turns", 1, 6)
        bonus = num(b, "bonus", 0, 12)
        inv = d.get("involves", "all")
        if inv not in INVOLVES:
            die(d["where"], "beat %s: involves: %s (all, a, pair)" % (b, inv))
        handles = classes(b, "handles", d.get("handles", "").split())
        hurt = classes(b, "hurt", d.get("hurt", "").split())
        lw = d.get("lean", "").split()
        lean_bonus = 0
        if lw:
            if not re.match(r"^\+\d+$", lw[-1]) or not 1 <= int(lw[-1]) <= 12:
                die(d["where"], "beat %s: lean: classes then +N (1..12)" % b)
            lean_bonus = int(lw[-1])
            lw = lw[:-1]
        lean = classes(b, "lean", lw)
        lean_act = acts(b, "lean_act", 2)
        if any(lean_act) and not lean_bonus:
            die(d["where"], "beat %s: lean_act needs lean: ... +N (or lean: +N alone)" % b)
        resolve = acts(b, "resolve", 4)
        fl = 0
        for f in d.get("flags", "").split():
            if f not in FLAGS:
                die(d["where"], "beat %s: flags: %s (kidsafe, adults)" % (b, f))
            fl |= FLAGS[f]
        # safety (talk_dm.c checks the same in C)
        romance_act = [a for a in lean_act + resolve if a and env["first_romance"] <= a <= env["last_romance"]]
        safety_act = [a for a in lean_act + resolve if a and env["first_safety"] <= a <= env["last_safety"]]
        if b == "romance_adult":
            if pre != "romance" or not fl & FLAGS["adults"]:
                die(d["where"], "safety: romance_adult needs pre: romance and flags: adults")
            if fl & FLAGS["kidsafe"]:
                die(d["where"], "safety: romance_adult is never a kids' beat")
        else:
            if pre == "romance":
                die(d["where"], "safety: only romance_adult may have pre: romance")
            if (handles | lean | hurt) & rom or romance_act:
                die(d["where"], "safety: only romance_adult may handle or lean on a romance act (beat %s)" % b)
        if (handles | lean | hurt) & saf or safety_act:
            die(d["where"], "safety: no beat handles, leans on or resolves with a safety act (beat %s)" % b)
        if b == "kid_moment" and (pre != "kid" or not fl & FLAGS["kidsafe"]):
            die(d["where"], "safety: kid_moment needs pre: kid and flags: kidsafe")
        i = env["tbeat"][b] * ROW
        rows[i:i + ROW] = bytes((env["tbp"][pre], tension & 0xFF, after & 0xFF, goals, turns, bonus, INVOLVES[inv],
                                 lean_bonus, fl, 0)) + _u16(handles) + _u16(hurt) + _u16(lean) + \
            bytes(resolve) + bytes(lean_act) + _u16(0)
    types = bytearray(2 * NTYPES)
    for t in range(NTYPES):
        if t not in aff:
            die(where, "no @affinity for persona type %d" % t)
        fav, dull, w = aff[t]
        for r in (fav, dull):
            if r not in env["tcr"] or r in BAD_ROOTS:
                die(w, "affinity %d: %s is not a topic root a type may warm to (not %s)" % (t, r, ", ".join(BAD_ROOTS)))
        if fav == dull:
            die(w, "affinity %d: favours and dull are the same root" % t)
        types[2 * t:2 * t + 2] = bytes((env["tcr"][fav], env["tcr"][dull]))
    for t in aff:
        if not 0 <= t < NTYPES:
            die(aff[t][2], "affinity %d: persona types are 0..%d" % (t, NTYPES - 1))
    head = _u32(BEATS_MAGIC) + _u16(env["nbeats"]) + _u16(ROW) + _u16(NTYPES) + _u16(0)
    return bytes(head + rows + types), {"beats": len(want), "bytes": len(head) + len(rows) + len(types)}


def make_env(core):
    import talkc_gen as tg
    th = os.path.join(core, "include", "doomo", "talk.h")
    tbeat = tg.c_enum(th, "TBEAT_")
    nbeats = tbeat.pop("count")
    tbp = tg.c_enum(th, "TBP_")
    tbp.pop("count")
    tac = tg.c_enum(th, "TAC_")
    tac.pop("count")
    acts = {k.upper(): v for k, v in tg.c_enum(th, "TA_", "TA_NONE").items()}
    acts.pop("COUNT")
    tcr = tg.c_enum(th, "TCR_")
    tcr.pop("count")
    return {"tbeat": tbeat, "nbeats": nbeats, "tbp": tbp, "tac": sorted(tac, key=tac.get), "acts": acts, "tcr": tcr,
            "first_romance": acts["FLIRT"], "last_romance": acts["KISS"],
            "first_safety": acts["REFUSE_SEXUAL"], "last_safety": acts["DEFLECT_ROMANCE"]}


def build_file(path, core, die, rel):
    return build(open(path, encoding="utf-8").read(), rel(path), make_env(core), die)


# seeded bad tables: (what to change in the real beats.tlk, the message it must fail with)
BAD = [
    (("@beat romance_adult\n  pre: romance", "@beat romance_adult\n  pre: default"), "romance_adult needs pre: romance"),
    (("flags: adults", "flags: kidsafe"), "romance_adult needs pre: romance and flags: adults"),
    (("@beat kid_moment\n  pre: kid", "@beat kid_moment\n  pre: default"), "kid_moment needs pre: kid"),
    (("@beat smalltalk\n  pre: default", "@beat smalltalk\n  pre: romance"), "only romance_adult may have pre: romance"),
    (("lean: third +3", "lean: romance +3"), "only romance_adult may handle or lean on a romance act"),
    (("@beat game", "@beat game\n  resolve: refuse_minor"), "no beat handles, leans on or resolves with a safety act"),
    (("@beat story", "@beat storey"), "is not a talk.h TBEAT_*"),
    (("@affinity 0 favours: lore", "@affinity 0 favours: romance"), "is not a topic root a type may warm to"),
]


def selftest(path, core, die_cls, rel):
    """every seeded bad table must fail with its message; returns how many did"""
    src = open(path, encoding="utf-8").read()
    env = make_env(core)

    class Stop(Exception):
        pass

    def die(where, msg):
        raise Stop("%s: %s" % (where, msg))
    n = 0
    for (old, new), expect in BAD:
        if old not in src:
            raise die_cls("talkc_beats selftest: %r is not in %s (update BAD)" % (old, rel(path)))
        try:
            build(src.replace(old, new, 1), "seeded", env, die)
        except Stop as e:
            if expect not in str(e):
                raise die_cls("talkc_beats selftest: expected %r, got %r" % (expect, str(e)))
            n += 1
            continue
        raise die_cls("talkc_beats selftest: a bad beat table built (expected %r)" % expect)
    return n
