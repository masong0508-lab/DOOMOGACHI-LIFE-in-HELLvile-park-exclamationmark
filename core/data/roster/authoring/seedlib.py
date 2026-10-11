"""seedlib.py - archetype definition helpers for the roster authoring tool
(owner: roster). See seed.py and docs/ROSTER.md.

An archetype is a family of citizens that share a job, a coherent palette,
an outfit vocabulary, personality ranges and favourite things. Its cast is
a block of lines, one hand-written citizen each:

    Name | pronoun | One-line bio. | key=value; key=value ...

pronoun is one of s (she), h (he), t (they), x (xe), i (it). The options
may override any record key (hat=CHEF, species=HUMAN, job=Head Baker, ...)
and add relationships: fam=TAG (household), wed=, date=, bff=, friend=,
rival=, nem=, ex=, crush= (names, comma separated), id=N (fixed id),
trait=SOFTIE,KID ... (extra ROSTER_T_* traits).
"""

PRONOUN_CODES = {"s": "SHE", "h": "HE", "t": "THEY", "x": "XE", "i": "IT"}


class CastLine:
    __slots__ = ("name", "pronoun", "bio", "opts", "arch", "src")

    def __init__(self, name, pronoun, bio, opts, src):
        self.name = name
        self.pronoun = pronoun
        self.bio = bio
        self.opts = opts
        self.arch = None
        self.src = src


def parse_cast(block, where):
    out = []
    for n, line in enumerate(block.strip("\n").split("\n"), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        parts = [p.strip() for p in s.split("|")]
        if len(parts) < 3 or len(parts) > 4:
            raise ValueError("%s cast line %d: expected 'Name | p | Bio [| opts]': %r" % (where, n, s))
        name, pcode, bio = parts[0], parts[1], parts[2]
        if pcode not in PRONOUN_CODES:
            raise ValueError("%s cast line %d: pronoun code %r" % (where, n, pcode))
        opts = {}
        if len(parts) == 4 and parts[3]:
            for kv in parts[3].split(";"):
                kv = kv.strip()
                if not kv:
                    continue
                if "=" not in kv:
                    raise ValueError("%s cast line %d: option %r needs key=value" % (where, n, kv))
                k, v = kv.split("=", 1)
                opts[k.strip()] = v.strip()
        out.append(CastLine(name, PRONOUN_CODES[pcode], bio, opts, "%s:%d" % (where, n)))
    return out


def A(key, side, job, cast, **kw):
    """Define an archetype. `job` is 'Job|Job2|...' (members cycle through
    them unless a cast line sets job=). All other keyword arguments are
    look/personality vocabularies, see seed.py (ARCH_KEYS)."""
    if side not in ("hill", "hell"):
        raise ValueError("archetype %s: side must be hill or hell" % key)
    d = dict(kw)
    d["key"] = key
    d["side"] = side
    d["jobs"] = [j.strip() for j in job.split("|") if j.strip()]
    d["cast"] = parse_cast(cast, key)
    for c in d["cast"]:
        c.arch = d
    return d
