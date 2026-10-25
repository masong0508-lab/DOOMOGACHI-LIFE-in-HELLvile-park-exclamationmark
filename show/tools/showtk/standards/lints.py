"""showtk.standards.lints - the STD-* lints of SHOW_ENGINE_SPEC.md sec.13.5 (WP17).

    diags = run(tl)                 # [errors.Diag], errors first
    python3 -m showtk.standards.lints hash "<word>"     # a slur's sha256 for series/lints.shw

They read the standards tags the compiler carries on shots, lines and events
(sec.5.5 `tag`, sec.5.6 `tags=`), the cast's/crowds'/voices' `age=` and `tags=`,
and the word lists in show/series/lints.shw.  Every hard-limit rule is an error that
blocks `show deliver --release`, and none can be waived.
"""
import hashlib
import re
import sys

from ..errors import Diag
from ..sidecars import SHOW

HARD = ("STD-EXPLICIT", "STD-CONSENT", "STD-AGE", "STD-KIDS", "STD-HATE")
SUGGESTIVE = {"innuendo", "suggestive", "implied", "advance", "contact", "touch", "rear", "gore_truth"}
SEXUAL_OK = {"implied", "offscreen", "innuendo"}
SEXUAL_BAD = {"explicit", "sex", "sexual", "nudity", "nude", "genitals", "anatomy"}
KID_AGES = {"child", "teen"}
FLASH_MAX_F = 3
FLASH_PER_S = 3
FP_MAX_F = 75 * 30


def norm(word):
    return " ".join(re.sub(r"[^a-z0-9' -]", " ", str(word).lower()).split())


def term_hash(word):
    return hashlib.sha256(norm(word).encode()).hexdigest()


def load_lists(path=None):
    path = path or SHOW / "series" / "lints.shw"
    out = {"slur": set(), "real": [], "innuendo": [], "brand": []}
    if not path.exists():
        return out
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0]
        for stmt in line.split(";"):
            m = re.match(r'\s*(slur|real|innuendo|brand)\s+(?:sha256:([0-9a-f]{64})|"([^"]+)")', stmt)
            if not m:
                continue
            kind, h, word = m.groups()
            if kind == "slur":
                out["slur"].add(h or term_hash(word))
            else:
                out[kind].append(word)
    return out


def _tagset(tags):
    return {t["name"] for t in tags}


def _cast(tl):
    """Cast entries from the timeline, else from show/series/cast.shw."""
    if tl.cast:
        return tl.cast
    p = SHOW / "series" / "cast.shw"
    cast, cur = {}, None
    if p.exists():
        for raw in p.read_text().splitlines():
            line = raw.split("#", 1)[0].rstrip()
            if not line.strip():
                continue
            m = re.match(r"\s*char\s+(\S+)(.*)", line)
            if m:
                cur = cast.setdefault(m.group(1), {})
                rest = m.group(2)
            elif cur is not None and re.match(r"\s+\w+=", line):
                rest = line
            else:
                continue
            for k, v in re.findall(r'(\w+)=("[^"]*"|\S+)', rest):
                cur[k] = v.strip('"')
    return cast


def _taglist(v):
    if isinstance(v, str):
        return {t for t in re.split(r"[,\s]+", v) if t}
    return set(v or [])


def _age(entry):
    return str(entry.get("age", "")).lower() if entry else ""


def _is_kid(entry):
    return _age(entry) in KID_AGES or bool({"KID", "TEEN"} & _taglist(entry.get("tags", "")))


class Ctx:
    def __init__(self, tl, lists):
        self.tl, self.lists = tl, lists
        self.cast = _cast(tl)
        self.diags = []

    def err(self, rule, msg, shot=None, frame=None, src=None, sev="error"):
        f, ln = None, None
        if src and ":" in str(src):
            f, _, ln = str(src).rpartition(":")
            ln = int(ln) if ln.isdigit() else None
        self.diags.append(Diag(rule, msg, file=f, line=ln, shot=shot, frame=frame, severity=sev))

    def who(self, cid):
        cid = str(cid or "")
        if cid.startswith("walla:"):
            return self.tl.voices.get(cid[6:], {})
        return self.cast.get(cid, {})


def _scene_kids(c):
    """{scene: [reasons]} for every scene with a child or teen in it (on screen or voiced)."""
    out = {}
    for s in c.tl.shots:
        for a in s.actors:
            ent = dict(c.who(a.get("who"))) if a.get("who") else {}
            ent.update({k: v for k, v in a.items() if k in ("age", "tags")})
            if _is_kid(ent):
                out.setdefault(s.scene, []).append("%s: %s" % (s.id, a.get("who") or a.get("id")))
        for cr in s.crowds:
            if _is_kid(cr):
                out.setdefault(s.scene, []).append("%s: crowd %s" % (s.id, cr.get("id")))
    for ln in c.tl.lines:
        if _is_kid(c.who(ln.who)):
            out.setdefault(ln.scene, []).append("%s: voice %s" % (ln.id, ln.who))
    return out


def lint_age(c):
    for cid, ent in c.cast.items():
        if _age(ent) not in ("adult", "teen", "child"):
            c.err("STD-AGE", "cast entry %s declares no age=adult|teen|child" % cid)
    for vid, ent in c.tl.voices.items():
        if _age(ent) not in ("adult", "teen", "child"):
            c.err("STD-AGE", "voice %s declares no age=adult|teen|child" % vid)
    for s in c.tl.shots:
        for cr in s.crowds:
            if _age(cr) not in ("adult", "teen", "child"):
                c.err("STD-AGE", "crowd %s declares no age=adult|teen|child" % cr.get("id"), s.id, 0, s.src)
        for a in s.actors:
            w = a.get("who")
            if w and w not in c.cast and not a.get("age"):
                c.err("STD-AGE", "actor %s (who=%s) is not in the cast and has no age" % (a.get("id"), w),
                      s.id, 0, s.src)


def _all_tags(c):
    """[(tagset, kind, id, shot, frame, src, scene, item)] for shots, lines, events."""
    out = []
    for s in c.tl.shots:
        out.append((_tagset(s.tags), "shot", s.id, s.id, 0, s.src, s.scene, s))
        for e in s.events:
            ts = _tagset(e.tags)
            if e.op == "tag":
                ts |= {t for t in re.split(r"[\s,]+", str(e.args)) if t and "=" not in t
                       and t not in ("shot", "line", "scene")}
            if ts:
                out.append((ts, "event", e.op, s.id, e.t, e.src, s.scene, e))
    for ln in c.tl.lines:
        out.append((ln.tag_names(), "line", ln.id, ln.shot, None, ln.src, ln.scene, ln))
    for a in c.tl.raw.get("assets", []) or []:
        out.append((_taglist(a.get("tags", "")), "asset", a.get("id"), None, None, a.get("src"), None, a))
    return out


def lint_explicit(c):
    stickers = 0
    for ts, kind, ident, shot, fr, src, scene, item in _all_tags(c):
        bad = ts & SEXUAL_BAD
        if bad:
            c.err("STD-EXPLICIT", "%s %s is tagged %s: sexual content may only be implied, "
                  "off-screen or innuendo (hard limit 1)" % (kind, ident, ",".join(sorted(bad))), shot, fr, src)
        if "sticker" in ts:
            stickers += 1
    for s in c.tl.shots:
        for a in s.actors:
            if a.get("who") == "grumbleguts" and str(a.get("modest", c.who("grumbleguts").get("modest", "no"))) != "yes":
                c.err("STD-EXPLICIT", "Grumbleguts appears in a look not marked modest=yes", s.id, 0, s.src)
    if stickers > 1:
        c.err("STD-STICKER", "the censor-sticker gag is used %d times; once a season at most" % stickers)


def lint_consent(c):
    lines = c.tl.lines
    by_id = {ln.id: (i, ln) for i, ln in enumerate(lines)}
    for i, ln in enumerate(lines):
        ts = {t["name"]: t for t in ln.tags}
        if "consent_gag" in ts:
            c.err("STD-CONSENT", "line %s is tagged consent_gag: consent is never the punchline" % ln.id,
                  ln.shot, None, ln.src)
        for name in ("advance", "contact", "touch"):
            if name not in ts:
                continue
            tgt = ts[name].get("to") or ln.to
            if not tgt:
                c.err("STD-CONSENT", "line %s: %s names no target (to=<char>)" % (ln.id, name), ln.shot, None, ln.src)
                continue
            for who in (ln.who, tgt):
                if _age(c.who(who)) != "adult":
                    c.err("STD-CONSENT", "line %s: %s between %s and %s, but %s is not age=adult"
                          % (ln.id, name, ln.who, tgt, who), ln.shot, None, ln.src)
            if name in ("contact", "touch"):
                ref = ts[name].get("consent") or ln.consent
                ok = False
                if ref and ref in by_id:
                    j, yes = by_id[ref]
                    ok = (j < i and yes.who == tgt and "yes" in yes.tag_names()
                          and yes.scene == ln.scene and bool(str(yes.text).strip(" .…")))
                if not ok:
                    c.err("STD-CONSENT", "line %s: %s with %s needs consent=<line> pointing at an earlier "
                          "spoken line from %s tagged yes in the same scene" % (ln.id, name, tgt, tgt),
                          ln.shot, None, ln.src)
    # a no ends it: the advancer's next line steps back; nothing aimed at the target after
    for i, ln in enumerate(lines):
        if "no" not in ln.tag_names():
            continue
        prev = [p for p in lines[:i] if p.scene == ln.scene and "advance" in p.tag_names()
                and ({t.get("to") for t in p.tags if t["name"] == "advance"} | {p.to}) & {ln.who}]
        for adv in {p.who for p in prev}:
            after = [p for p in lines[i + 1:] if p.scene == ln.scene and p.who == adv]
            if after and "stepback" not in after[0].tag_names():
                c.err("STD-CONSENT", "line %s: after %s's no, %s's next line must be tagged stepback"
                      % (after[0].id, ln.who, adv), after[0].shot, None, after[0].src)
            for p in after:
                hit = {t["name"] for t in p.tags if t["name"] in ("advance", "innuendo", "contact", "touch")
                       and (t.get("to") or p.to) == ln.who}
                if hit:
                    c.err("STD-CONSENT", "line %s: %s aimed at %s after their no (%s)" % (
                        p.id, ",".join(sorted(hit)), ln.who, ln.id), p.shot, None, p.src)


def lint_kids(c):
    kids = _scene_kids(c)
    for ts, kind, ident, shot, fr, src, scene, item in _all_tags(c):
        bad = ts & SUGGESTIVE
        if scene in kids and bad:
            c.err("STD-KIDS", "%s %s is tagged %s in scene %s, which has a child or teen (%s): hard limit 3"
                  % (kind, ident, ",".join(sorted(bad)), scene, kids[scene][0]), shot, fr, src)
        if kind == "line" and (ts & (SUGGESTIVE - {"gore_truth"})) and item.ref and _is_kid(c.who(item.ref)):
            c.err("STD-KIDS", "line %s is suggestive and refers to %s, a child" % (ident, item.ref), shot, fr, src)
    for s in c.tl.shots:
        if s.scene in kids:
            if s.gore == "BRUTAL":
                c.err("STD-KIDS", "shot %s is gore=BRUTAL in scene %s with a child (%s): the filter stays on"
                      % (s.id, s.scene, kids[s.scene][0]), s.id, 0, s.src)
            if s.insert and "gore_truth" in _tagset(s.tags):
                c.err("STD-KIDS", "truth insert %s sits in scene %s with a child" % (s.id, s.scene), s.id, 0, s.src)


def _words(text, phrase):
    return re.search(r"(?<![a-z])%s(?![a-z])" % re.escape(phrase.lower()), " " + norm(text) + " ") is not None


def _texts(c):
    for ln in c.tl.lines:
        yield "line", ln.id, ln.text, ln.shot, ln.src, ln.tag_names()
    for st in c.tl.strings:
        yield "string", st.get("id"), st.get("text", ""), st.get("shot"), st.get("src"), _taglist(st.get("tags", ""))
    for ch in c.tl.chapters:
        yield "chapter", ch.get("title"), ch.get("title", ""), None, None, set()
    if c.tl.title:
        yield "title", "title", c.tl.title, None, None, set()


def lint_hate_scan(c):
    slurs = c.lists["slur"]
    for kind, ident, text, shot, src, ts in _texts(c):
        toks = norm(text).split()
        grams = set(toks) | {" ".join(toks[i:i + 2]) for i in range(len(toks) - 1)}
        for g in grams:
            if term_hash(g) in slurs:
                c.err("STD-HATE", "%s %s contains a word on the slur list (hard limit 4)" % (kind, ident), shot, None, src)
        for r in c.lists["real"]:
            if _words(text, r):
                c.err("STD-HATE", "%s %s names %r (a real person, group or faith): it may mention, never "
                      "target; listed for the creator's check" % (kind, ident, r), shot, None, src, sev="warning")
        for b in c.lists["brand"]:
            if _words(text, b):
                c.err("STD-BRAND", "%s %s names the real brand %r" % (kind, ident, b), shot, None, src, sev="warning")
        if not (ts & (SUGGESTIVE | SEXUAL_OK)):
            for w in c.lists["innuendo"]:
                if _words(text, w):
                    c.err("STD-TAGSCAN", "%s %s is untagged but matches %r: tag it (innuendo/implied/...) "
                          "or note why it is not" % (kind, ident, w), shot, None, src, sev="warning")
                    break


def flash_events(tl):
    out = []
    for s in tl.shots:
        for e in s.events:
            if e.op == "flash":
                a = str(e.args).split() if not isinstance(e.args, dict) else []
                pal = a[0] if a else ""
                n = int(a[2]) if len(a) > 2 and a[2].lstrip("+").isdigit() else int(e.raw.get("frames", 1))
                full = "local" not in str(e.args)
                out.append((s, e, pal, n, full))
    return out


def lint_flash(c):
    fl = flash_events(c.tl)
    for s, e, pal, n, full in fl:
        if n > FLASH_MAX_F:
            c.err("STD-FLASH", "flash %s lasts %d f; at most %d (TIME-FLASH)" % (pal, n, FLASH_MAX_F), s.id, e.t, e.src)
        if full and "RED" in pal.upper():
            c.err("STD-FLASH", "a full-screen red flash (%s); red flashes are local only" % pal, s.id, e.t, e.src)
    fr = sorted(e.f for _, e, _, _, _ in fl)
    for i, f in enumerate(fr):
        n = sum(1 for g in fr[i:] if g < f + 30)
        if n > FLASH_PER_S:
            s = c.tl.shot_at(f)
            c.err("STD-FLASH", "%d flashes within 1 s from frame %d; at most %d a second" % (n, f, FLASH_PER_S),
                  s.id if s else None, f - s.start if s else None)
            break


def lint_misc(c):
    fp = sum(s.len for s in c.tl.shots if s.fp)
    if fp > FP_MAX_F:
        c.err("STD-FPS", "first person runs %.1f s; the cap is 75 s an episode" % (fp / 30.0))
    m = re.search(r"(\d+)", c.tl.episode)
    epn = int(m.group(1)) if m else 0
    for s in c.tl.shots:
        for a in s.actors:
            first = str(c.who(a.get("who")).get("first", ""))
            fm = re.search(r"(\d+)", first)
            if fm and int(fm.group(1)) > epn:
                c.err("STD-LOCK", "%s is not seen before %s (Lock List)" % (a.get("who"), first), s.id, 0, s.src)
    for ts, kind, ident, shot, fr, src, scene, item in _all_tags(c):
        for t in (item.tags if hasattr(item, "tags") and isinstance(getattr(item, "tags"), list) else []):
            if t.get("name") not in ("hurt", "gore", "gore_truth", "harm"):
                continue
            tgt = t.get("to")
            if tgt == "sunny":
                c.err("STD-SUNNY", "%s %s shows Sunny hurt: a hit reads on the lens only" % (kind, ident), shot, fr, src)
            elif tgt == "pet":
                c.err("STD-PET", "%s %s harms the pet" % (kind, ident), shot, fr, src)
            elif tgt and (_is_kid(c.who(tgt)) or "CUTE" in _taglist(c.who(tgt).get("tags", ""))):
                c.err("STD-GORE", "%s %s gores %s: gore lands on demons and monsters only" % (kind, ident, tgt),
                      shot, fr, src)
        if kind == "asset" and ("third_party" in ts or "reference" in ts):
            c.err("STD-REFART", "asset %s is third-party reference art" % ident, None, None, src)


def run(tl, lists=None):
    lists = lists or load_lists()
    extra = tl.raw.get("lints") or {}
    if extra:          # a fixture's own lists (the slur fixture uses an invented word)
        lists = {k: (set(v) if isinstance(v, set) else list(v)) for k, v in lists.items()}
        lists["slur"] |= {term_hash(w) for w in extra.get("slur_terms", [])}
        for k in ("real", "innuendo", "brand"):
            lists[k] += extra.get(k, [])
    c = Ctx(tl, lists)
    for f in (lint_age, lint_explicit, lint_consent, lint_kids, lint_hate_scan, lint_flash, lint_misc):
        f(c)
    seen, out = set(), []
    for d in c.diags:
        k = d.format()
        if k not in seen:
            seen.add(k)
            out.append(d)
    return sorted(out, key=lambda d: (not d.is_error, d.rule))


def kid_scenes(tl):
    c = Ctx(tl, load_lists())
    return _scene_kids(c)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "hash":
        print("slur sha256:%s" % term_hash(sys.argv[2]))
    else:
        print(__doc__)
