#!/usr/bin/env python3
# doomo-generator
"""roster_build.py - compile the 1000 premade citizens (owner: roster).

Reads the human-editable roster in core/data/roster/:

    hillvile_*.txt   citizens 0..499   (the good side of Hellvile Park)
    hellvile_*.txt   citizens 500..999 (the bad side)
    edges.txt        pre-seeded relationships

validates every record against the C contract (enum NAMES are parsed from
core/include/doomo/*.h and the like/fear/catchphrase tables from
core/src/catalog.c, so the text never contains magic numbers) and writes
core/gen/roster_data.c with const DoomoResident records, bios, jobs,
traits, edges and doomo_story_npc[]. The record format is documented in
docs/ROSTER.md.

    python3 tools/roster_build.py [--out-root DIR]   compile (make -C core gen)
    python3 tools/roster_build.py --check            validate only
    python3 tools/roster_build.py --audit            validate + shipped-roster
                                                     quality audit + stats
Exit status 1 on any error. Output is byte-identical on every run.
Only the python3 standard library is used.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(ROOT, "core", "include", "doomo")
DATA = os.path.join(ROOT, "core", "data", "roster")
OUT_REL = os.path.join("core", "gen", "roster_data.c")

MONTHS = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN",
          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]

# ROSTER_T_* (roster.h) by name
TRAIT_NAMES = ["SOFTIE", "KID", "ELDER", "VAPER", "HOLY", "LOYALIST",
               "INFILTRATOR", "NIGHT_OWL"]

# critter-folk species (the vaguely furry heart of the cast)
CRITTERS = {"CATFOLK", "FOXKIN", "BUNNYKIN", "BEARKIN", "PUPKIN", "MOUSEKIN",
            "SHEEPKIN", "RACCOONKIN", "OWLKIN", "FROGLING", "MOTHKIN"}
DEMONIC = {"IMP", "GHOUL", "SKELETON", "HELLHOUND", "CACOBLOB", "BATLING",
           "MUMMY", "SLIME", "GHOST"}


class RosterError(Exception):
    pass


# =============================================================================
# C contract parsing
# =============================================================================
def _strip_comments(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    return re.sub(r"//[^\n]*", " ", src)


def _eval_int(expr, names):
    expr = expr.strip()
    if not expr:
        raise ValueError("empty")
    tokens = re.findall(r"[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|\d+|<<|>>|[-+*/|&()~]", expr)
    out = []
    for t in tokens:
        if re.match(r"[A-Za-z_]", t):
            if t not in names:
                raise ValueError("unknown name " + t)
            out.append(str(names[t]))
        elif t.lower().startswith("0x"):
            out.append(str(int(t, 16)))
        elif t == "/":
            out.append("//")
        else:
            out.append(t)
    return int(eval("".join(out), {"__builtins__": {}}, {}))  # digits/operators only


class Contract:
    """Every enum constant and #define value from the doomo headers, plus the
    name tables in core/src/catalog.c."""

    def __init__(self, root=ROOT):
        self.values = {}
        inc = os.path.join(root, "core", "include", "doomo")
        for fn in sorted(os.listdir(inc)):
            if fn.endswith(".h"):
                with open(os.path.join(inc, fn), encoding="ascii") as f:
                    self._parse(f.read())
        with open(os.path.join(root, "core", "src", "catalog.c"), encoding="ascii") as f:
            cat = _strip_comments(f.read())
        self.tables = {}
        for m in re.finditer(r"const\s+char\s*\*\s*const\s+(doomo_\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\};",
                             cat, re.S):
            strs = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2))
            self.tables[m.group(1)] = [bytes(s, "ascii").decode("unicode_escape") for s in strs]

    def _parse(self, src):
        src = _strip_comments(src)
        for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+([^\n]+)$", src, re.M):
            try:
                self.values.setdefault(m.group(1), _eval_int(m.group(2), self.values))
            except Exception:
                pass
        for m in re.finditer(r"\benum\b\s*\w*\s*\{(.*?)\}", src, re.S):
            nxt = 0
            for item in m.group(1).split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, expr = item.split("=", 1)
                    name = name.strip()
                    nxt = _eval_int(expr, self.values)
                else:
                    name = item
                if not re.match(r"^[A-Za-z_]\w*$", name):
                    continue
                self.values[name] = nxt
                nxt += 1

    def enum(self, prefix, count_name=None):
        """{SUFFIX: value} for PREFIX_* constants below the list's _COUNT."""
        count = self.values.get(count_name or (prefix + "COUNT"))
        out = {}
        for k, v in self.values.items():
            if not k.startswith(prefix) or k.endswith("_COUNT") or k == prefix + "COUNT":
                continue
            suffix = k[len(prefix):]
            if count is not None and not (0 <= v < count):
                continue
            out[suffix] = v
        return out

    def table(self, name, count_name):
        t = self.tables.get(name)
        if t is None:
            raise RosterError("catalog.c: table %s not found" % name)
        if len(t) != self.values[count_name]:
            raise RosterError("catalog.c: %s has %d entries, %s = %d"
                              % (name, len(t), count_name, self.values[count_name]))
        return t


# =============================================================================
# record format
# =============================================================================
class Schema:
    """Field definitions for one citizen record, derived from the contract."""

    def __init__(self, c):
        self.c = c
        v = c.values
        self.paint_count = v["PAL_PAINT_COUNT"]
        self.name_len = v["DOOMO_NAME_LEN"] - 1
        self.max_edges = v["DOOMO_MAX_EDGES"]
        self.count = v["DOOMO_MAX_RESIDENTS"]
        self.district = v["DOOMO_DISTRICT_SIZE"]
        self.bio_max = v.get("ROSTER_BIO_MAX", 60)
        self.job_max = v.get("ROSTER_JOB_MAX", 20)
        P = {}
        P["species"] = c.enum("SPECIES_")
        P["body"] = c.enum("BODY_")
        P["head"] = c.enum("HEAD_")
        P["eyes"] = c.enum("EYE_")
        P["brows"] = c.enum("BROW_")
        P["nose"] = c.enum("NOSE_")
        P["mouth"] = c.enum("MOUTH_")
        P["hair"] = c.enum("HAIR_")
        P["facial_hair"] = c.enum("FHAIR_")
        P["ears"] = c.enum("EARS_")
        P["horns"] = c.enum("HORNS_")
        P["tail"] = c.enum("TAIL_")
        P["wings"] = c.enum("WINGS_")
        P["halo"] = c.enum("HALO_")
        P["marking"] = c.enum("MARK_")
        P["top"] = c.enum("TOP_")
        P["bottom"] = c.enum("BOTTOM_")
        P["shoes"] = c.enum("SHOES_")
        P["hat"] = c.enum("HAT_")
        P["accessory"] = c.enum("ACC_")
        P["aura"] = c.enum("AURA_")
        P["pattern"] = c.enum("PATTERN_")
        P["pronoun"] = c.enum("PRONOUN_")
        P["romance"] = c.enum("ROMANCE_")
        P["mood"] = c.enum("MOOD_")
        P["activity"] = c.enum("ACT_")
        self.enums = P
        self.paints = {k: val for k, val in c.enum("PAINT_", "PAL_PAINT_COUNT").items()}
        self.skins = {k: val for k, val in c.enum("SKIN_", "PAL_SKIN_COUNT").items()}
        self.cheeks = {k[len("CHEEK_"):]: val for k, val in v.items() if k.startswith("CHEEK_")}
        self.rflags = {k[len("RF_"):]: val for k, val in v.items() if k.startswith("RF_")}
        self.npcs = c.enum("NPC_")
        self.rel = c.enum("REL_", "REL_KIND_COUNT")
        self.rel.pop("KIND_MASK", None)
        self.rel.pop("CRUSH_AB", None)
        self.rel.pop("CRUSH_BA", None)
        self.foods = c.table("doomo_food_names", "FOOD_COUNT")
        self.hobbies = c.table("doomo_hobby_names", "HOBBY_COUNT")
        self.music = c.table("doomo_music_names", "MUSIC_COUNT")
        self.fears = c.table("doomo_fear_names", "FEAR_COUNT")
        self.phrases = c.table("doomo_catchphrases", "CATCHPHRASE_COUNT")
        self.status_alive = v["RES_ALIVE"]
        self.npc_ids = {}
        for npc in self.npcs:
            key = "ROSTER_ID_" + npc
            if key in v:
                self.npc_ids[npc] = v[key]
        # key -> (kind, arg). Kinds: enum, int, paint, paint_or_none, skin,
        # cheeks, text-table, flags, traits, story, birthday.
        self.fields = {}
        for k in ("species", "body", "head", "eyes", "brows", "nose", "mouth", "hair",
                  "facial_hair", "ears", "horns", "tail", "wings", "halo", "marking", "top",
                  "bottom", "shoes", "hat", "accessory", "aura", "pattern", "pronoun",
                  "romance", "mood", "activity"):
            self.fields[k] = ("enum", k)
        for k, hi in (("height", 15), ("build", 15), ("head_size", 15), ("eye_size", 15),
                      ("eye_spacing", 15), ("eye_height", 15), ("eye_tilt", 15),
                      ("brow_height", 15), ("nose_size", 15), ("mouth_size", 15),
                      ("horn_size", 15)):
            self.fields[k] = ("int", (0, hi))
        self.fields["eye_count"] = ("int", (1, 3))
        self.fields["fangs"] = ("int", (0, 3))
        for k in ("eye_color", "hair_color", "horn_color", "marking_color", "top_color",
                  "pattern_color", "bottom_color", "shoes_color", "hat_color",
                  "accessory_color", "color"):
            self.fields[k] = ("paint", None)
        self.fields["highlight"] = ("paint_or_none", None)
        self.fields["skin"] = ("skin", None)
        self.fields["cheeks"] = ("cheeks", None)
        self.fields["flags"] = ("flags", None)
        self.fields["traits"] = ("traits", None)
        self.fields["story"] = ("story", None)
        self.fields["birthday"] = ("birthday", None)
        for k in ("pitch", "speed", "quality", "intonation", "accent"):
            self.fields[k] = ("int", (0, 31))
        for k in ("movement", "speech", "expression", "attitude", "quirk"):
            self.fields[k] = ("int", (0, 15))
        for k in ("hunger", "happy", "hygiene", "energy", "health", "sanity", "corruption",
                  "social"):
            self.fields[k] = ("int", (0, 100))
        self.fields["food"] = ("table", self.foods)
        self.fields["hates"] = ("table", self.foods)
        self.fields["hobby"] = ("table", self.hobbies)
        self.fields["music"] = ("table", self.music)
        self.fields["fear"] = ("table", self.fears)
        self.fields["phrase"] = ("table", self.phrases)

    # canonical key order of a fully written record (also the --strict list)
    KEY_LINES = [
        ["story", "traits", "flags"],
        ["pronoun", "romance", "birthday"],
        ["species", "body", "height", "build", "skin", "head", "head_size"],
        ["eyes", "eye_color", "eye_size", "eye_spacing", "eye_height", "eye_tilt", "eye_count"],
        ["brows", "brow_height", "nose", "nose_size", "mouth", "mouth_size", "fangs", "cheeks"],
        ["hair", "hair_color", "highlight", "facial_hair"],
        ["ears", "horns", "horn_color", "horn_size", "tail", "wings", "halo"],
        ["marking", "marking_color", "aura"],
        ["top", "top_color", "pattern", "pattern_color", "bottom", "bottom_color"],
        ["shoes", "shoes_color", "hat", "hat_color", "accessory", "accessory_color"],
        ["movement", "speech", "expression", "attitude", "quirk"],
        ["pitch", "speed", "quality", "intonation", "accent"],
        ["food", "hates", "hobby", "music", "color", "fear"],
        ["phrase"],
        ["hunger", "happy", "hygiene", "energy", "health", "sanity", "corruption", "social"],
        ["mood", "activity"],
    ]
    OPTIONAL_KEYS = {"story", "traits"}   # may be omitted even in --strict

    def all_keys(self):
        return [k for line in self.KEY_LINES for k in line]

    # ---- value parsing ------------------------------------------------------
    def parse_value(self, key, raw, where):
        kind, arg = self.fields[key]
        def bad(msg):
            raise RosterError("%s: %s=%s: %s" % (where, key, raw, msg))
        if kind == "enum":
            t = self.enums[arg]
            if raw not in t:
                bad("unknown value (expected one of %s)" % ", ".join(sorted(t, key=t.get)))
            return t[raw]
        if kind == "int":
            if not re.match(r"^-?\d+$", raw):
                bad("not a number")
            n = int(raw)
            if not arg[0] <= n <= arg[1]:
                bad("out of range %d..%d" % arg)
            return n
        if kind == "paint":
            if raw not in self.paints:
                bad("unknown paint colour")
            return self.paints[raw]
        if kind == "paint_or_none":
            if raw == "NONE":
                return 0xFF
            if raw not in self.paints:
                bad("unknown paint colour (or NONE)")
            return self.paints[raw]
        if kind == "skin":
            if raw.startswith("SKIN_"):
                if raw[5:] not in self.skins:
                    bad("unknown natural skin tone")
                return self.skins[raw[5:]]
            if raw not in self.paints:
                bad("unknown paint colour (natural tones are SKIN_*)")
            return 8 + self.paints[raw]
        if kind == "cheeks":
            if raw == "NONE":
                return 0
            bits = 0
            for part in raw.split(","):
                if part not in self.cheeks:
                    bad("unknown cheek flag " + part)
                bits |= self.cheeks[part]
            return bits
        if kind == "flags":
            if raw == "NONE":
                return 0
            bits = 0
            for part in raw.split(","):
                if part not in self.rflags:
                    bad("unknown flag " + part)
                bits |= self.rflags[part]
            return bits
        if kind == "traits":
            if raw == "NONE":
                return 0
            bits = 0
            for part in raw.split(","):
                if part not in TRAIT_NAMES:
                    bad("unknown trait " + part)
                bits |= 1 << TRAIT_NAMES.index(part)
            return bits
        if kind == "story":
            if raw not in self.npcs:
                bad("unknown story NPC")
            return self.npcs[raw]
        if kind == "birthday":
            m = re.match(r"^([A-Z]{3})-(\d\d?)$", raw)
            if not m or m.group(1) not in MONTHS:
                bad("expected MON-DD, e.g. APR-01")
            d = int(m.group(2))
            if not 1 <= d <= 28:
                bad("day must be 1..28")
            return (MONTHS.index(m.group(1)), d)
        if kind == "table":
            if raw not in arg:
                bad("not in the catalog table")
            return arg.index(raw)
        bad("internal: unknown field kind")
        return None

    # ---- species-aware defaults for keys a modder leaves out ----------------
    def defaults(self, species_name, hell):
        E, P = self.enums, self.paints
        d = {
            "flags": self.rflags["HELLBORN"] if hell else 0, "traits": 0,
            "pronoun": E["pronoun"]["THEY"], "romance": E["romance"]["ANY"],
            "body": E["body"]["AVERAGE"], "height": 7, "build": 8,
            "skin": 8 + P["CARAMEL"], "head": E["head"]["ROUND"], "head_size": 11,
            "eyes": E["eyes"]["ROUND"], "eye_color": P["HONEY"], "eye_size": 10,
            "eye_spacing": 7, "eye_height": 8, "eye_tilt": 8, "eye_count": 2,
            "brows": E["brows"]["THIN"], "brow_height": 7, "nose": E["nose"]["DOT"],
            "nose_size": 6, "mouth": E["mouth"]["SMILE"], "mouth_size": 6, "fangs": 0,
            "cheeks": self.cheeks["BLUSH"], "hair": E["hair"]["MESSY"],
            "hair_color": P["CARAMEL"], "highlight": 0xFF,
            "facial_hair": E["facial_hair"]["NONE"], "ears": E["ears"]["NORMAL"],
            "horns": E["horns"]["NONE"], "horn_color": P["BONE"], "horn_size": 7,
            "tail": E["tail"]["NONE"], "wings": E["wings"]["NONE"], "halo": E["halo"]["NONE"],
            "marking": E["marking"]["NONE"], "marking_color": P["WHITE"],
            "aura": E["aura"]["NONE"], "top": E["top"]["HOODIE"], "top_color": P["BUBBLEGUM"],
            "pattern": E["pattern"]["SOLID"], "pattern_color": P["WHITE"],
            "bottom": E["bottom"]["SHORTS"], "bottom_color": P["SKY"],
            "shoes": E["shoes"]["SNEAKERS"], "shoes_color": P["WHITE"],
            "hat": E["hat"]["NONE"], "hat_color": P["LEMON"],
            "accessory": E["accessory"]["NONE"], "accessory_color": P["MINT"],
            "movement": 8, "speech": 8, "expression": 8, "attitude": 8, "quirk": 8,
            "pitch": 16, "speed": 16, "quality": 16, "intonation": 16, "accent": 16,
            "food": 0, "hates": 46, "hobby": 0, "music": 0, "color": P["BUBBLEGUM"],
            "fear": 0, "phrase": 0,
            "hunger": 80, "happy": 70 if hell else 85, "hygiene": 80, "energy": 80,
            "health": 95, "sanity": 70 if hell else 90, "corruption": 45 if hell else 5,
            "social": 70, "mood": E["mood"]["OK"], "activity": E["activity"]["HOME"],
            "birthday": (0, 1),
        }
        coherent = {  # species: (ears, tail, nose, mouth, skin paint)
            "CATFOLK": ("CAT", "CAT", "CAT", "CAT", "CARAMEL"),
            "FOXKIN": ("FOX", "FOX", "CAT", "CAT", "TANGERINE"),
            "BEARKIN": ("BEAR", "BEAR_NUB", "BUTTON", "SMILE", "CHOCOLATE"),
            "PUPKIN": ("FLOPPY_PUP", "PUP_WAGGY", "BUTTON", "SMILE", "HONEY"),
            "MOUSEKIN": ("MOUSE", "MOUSE", "CAT", "CAT", "SILVER"),
            "SHEEPKIN": ("SHEEP", "SHEEP_PUFF", "BUTTON", "SMILE", "WHITE"),
            "RACCOONKIN": ("RACCOON", "RACCOON_RINGED", "BUTTON", "SMILE", "SILVER"),
            "OWLKIN": ("OWL_TUFTS", "FEATHER_FAN", "NONE", "BEAK", "CARAMEL"),
            "BUNNYKIN": ("BUNNY", "BUNNY", "BUTTON", "CAT", "WHITE"),
            "IMP": ("POINTY", "DEVIL", "DOT", "FANGY", "CHERRY"),
            "HELLHOUND": ("DOG", "FLAME", "BUTTON", "FANGY", "CHARCOAL"),
            "BATLING": ("BAT", "NONE", "DOT", "FANGY", "GRAPE"),
            "SKELETON": ("NONE", "NONE", "SKULL", "TEETH", "BONE"),
            "CACOBLOB": ("NONE", "NONE", "NONE", "SHARK", "CHERRY"),
            "GHOST": ("NONE", "NONE", "NONE", "O", "WHITE"),
            "SLIME": ("NONE", "NONE", "NONE", "SMILE", "SLIME"),
            "ROBOT": ("NONE", "NONE", "NONE", "FLAT", "SILVER"),
            "MOTHKIN": ("NONE", "NONE", "NONE", "SMILE", "LAVENDER"),
            "FROGLING": ("NONE", "NONE", "NONE", "WAVY", "LIME"),
            "PLUSHIE": ("BEAR", "BEAR_NUB", "BUTTON", "STITCHED", "BUBBLEGUM"),
            "GHOUL": ("ELF", "NONE", "DOT", "TEETH", "MINT"),
            "MUMMY": ("NORMAL", "NONE", "DOT", "FLAT", "BONE"),
        }
        if species_name in coherent:
            ears, tail, nose, mouth, skin = coherent[species_name]
            d["ears"] = E["ears"][ears]
            d["tail"] = E["tail"][tail]
            d["nose"] = E["nose"][nose]
            d["mouth"] = E["mouth"][mouth]
            d["skin"] = 8 + P[skin]
        elif species_name == "CHERUB":
            d["skin"] = self.skins["PEACH"]
            d["wings"] = E["wings"]["ANGEL"]
            d["halo"] = E["halo"]["HALO"]
        else:
            d["skin"] = self.skins["FAIR"]
        if species_name == "CACOBLOB":
            d["eye_count"] = 1
            d["head"] = E["head"]["BLOB"]
        if species_name == "SKELETON":
            d["head"] = E["head"]["SKULL"]
            d["hair"] = E["hair"]["BALD"]
        if species_name == "MUMMY":
            d["accessory"] = E["accessory"]["BANDAGES"]
        if species_name in ("GHOST", "SLIME"):
            d["shoes"] = E["shoes"]["BARE"]
        return d


class Citizen:
    __slots__ = ("id", "name", "job", "bio", "v", "where", "comments", "explicit")

    def __init__(self, cid, name, where):
        self.id = cid
        self.name = name
        self.job = None
        self.bio = None
        self.v = {}
        self.where = where
        self.comments = []
        self.explicit = set()


TOKEN_RE = re.compile(r'\s*([a-z_]+)=("(?:[^"\\]|\\.)*"|[^\s"]+)')


def parse_citizen_file(path, schema, errors):
    rel = os.path.relpath(path, ROOT)
    out = []
    cur = None
    with open(path, "rb") as f:
        raw = f.read()
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError:
        errors.append("%s: not plain ASCII" % rel)
        return out
    for ln, line in enumerate(text.split("\n"), 1):
        where = "%s:%d" % (rel, ln)
        s = line.rstrip("\r").rstrip()
        if not s.strip():
            continue
        if s.lstrip().startswith("#"):
            if cur is not None:
                cur.comments.append(s.strip())
            continue
        m = re.match(r"^\[(\d+)\]\s+(.*\S)\s*$", s)
        if m:
            cur = Citizen(int(m.group(1)), m.group(2), where)
            out.append(cur)
            continue
        if cur is None:
            errors.append("%s: text before the first [id] Name header" % where)
            continue
        m = re.match(r"^\s*(job|bio):\s?(.*)$", s)
        if m:
            k, val = m.group(1), m.group(2).strip()
            if getattr(cur, k) is not None:
                errors.append("%s: %s given twice for [%d]" % (where, k, cur.id))
            setattr(cur, k, val)
            continue
        pos = 0
        while pos < len(s):
            if not s[pos:].strip():
                break
            m = TOKEN_RE.match(s, pos)
            if not m:
                errors.append("%s: cannot parse %r (expected key=value)" % (where, s[pos:].strip()))
                break
            key, val = m.group(1), m.group(2)
            pos = m.end()
            if val.startswith('"'):
                val = bytes(val[1:-1], "ascii").decode("unicode_escape")
            if key not in schema.fields:
                errors.append("%s: unknown key %r" % (where, key))
                continue
            if key in cur.explicit:
                errors.append("%s: %s given twice for [%d]" % (where, key, cur.id))
                continue
            try:
                cur.v[key] = schema.parse_value(key, val, where)
                cur.explicit.add(key)
            except RosterError as e:
                errors.append(str(e))
    return out


def parse_edges(path, by_name, by_id, schema, errors):
    rel = os.path.relpath(path, ROOT)
    edges = []
    if not os.path.exists(path):
        return edges
    with open(path, encoding="ascii") as f:
        lines = f.read().split("\n")

    def resolve(tok, where):
        tok = tok.strip()
        if tok.startswith("#") and tok[1:].isdigit():
            i = int(tok[1:])
            if i not in by_id:
                errors.append("%s: no citizen #%d" % (where, i))
                return None
            return i
        i = by_name.get(tok.lower())
        if i is None:
            errors.append("%s: no citizen named %r" % (where, tok))
        return i

    for ln, line in enumerate(lines, 1):
        where = "%s:%d" % (rel, ln)
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        parts = [p.strip() for p in s.split("|")]
        if len(parts) not in (4, 5):
            errors.append("%s: expected 'A | B | KIND | value [| crush: NAME[, NAME]]'" % where)
            continue
        a, b = resolve(parts[0], where), resolve(parts[1], where)
        kind = parts[2]
        if kind not in schema.rel:
            errors.append("%s: unknown relationship kind %r" % (where, kind))
            continue
        if not re.match(r"^-?\d+$", parts[3]) or not -100 <= int(parts[3]) <= 100:
            errors.append("%s: value must be -100..100" % where)
            continue
        crush = set()
        if len(parts) == 5:
            m = re.match(r"^crush:\s*(.+)$", parts[4])
            if not m:
                errors.append("%s: fifth column must be 'crush: NAME[, NAME]'" % where)
                continue
            for who in m.group(1).split(","):
                w = resolve(who, where)
                if w is not None:
                    if w not in (a, b):
                        errors.append("%s: crusher %r is not in this pair" % (where, who.strip()))
                    crush.add(w)
        if a is None or b is None:
            continue
        if a == b:
            errors.append("%s: a citizen cannot be related to themself" % where)
            continue
        edges.append({"a": a, "b": b, "kind": kind, "value": int(parts[3]),
                      "crush": crush, "where": where})
    return edges


# =============================================================================
# validation
# =============================================================================
LOOK_ORDER = ["species", "body", "height", "build", "skin", "head", "head_size", "eyes",
              "eye_color", "eye_size", "eye_spacing", "eye_height", "eye_tilt", "eye_count",
              "brows", "brow_height", "nose", "nose_size", "mouth", "mouth_size", "fangs",
              "hair", "hair_color", "highlight", "facial_hair", "ears", "horns", "horn_color",
              "horn_size", "tail", "wings", "halo", "marking", "marking_color", "cheeks", "top",
              "top_color", "bottom", "bottom_color", "shoes", "shoes_color", "hat", "hat_color",
              "accessory", "accessory_color", "aura", "pattern", "pattern_color"]

# auto relationship kinds and their value bands (town.c rel_adjust)
AUTO_BANDS = {"ACQUAINTANCE": (-24, 24), "FRIEND": (15, 69), "BEST_FRIEND": (70, 100),
              "RIVAL": (-69, -15), "NEMESIS": (-100, -70)}
ROMANTIC = {"DATING", "MARRIED"}


def accepts(schema, who, other):
    """Romance preference of `who` accepts `other`'s pronoun group."""
    R, Pn = schema.enums["romance"], schema.enums["pronoun"]
    r, p = who.v["romance"], other.v["pronoun"]
    if r == R["ANY"]:
        return True
    if r == R["NONE"]:
        return False
    if r == R["SHE"]:
        return p == Pn["SHE"]
    if r == R["HE"]:
        return p == Pn["HE"]
    return p in (Pn["THEY"], Pn["XE"], Pn["IT"])


def load(strict=False):
    """Parse + validate everything. Returns (schema, citizens by id, edges,
    errors, warnings)."""
    errors, warnings = [], []
    schema = Schema(Contract())
    files = sorted(fn for fn in os.listdir(DATA)
                   if fn.endswith(".txt") and (fn.startswith("hillvile") or fn.startswith("hellvile")))
    if not files:
        errors.append("no core/data/roster/{hillvile,hellvile}*.txt files")
    cits = []
    for fn in files:
        cits.extend(parse_citizen_file(os.path.join(DATA, fn), schema, errors))

    by_id = {}
    for c in cits:
        if not 0 <= c.id < schema.count:
            errors.append("%s: id %d out of range 0..%d" % (c.where, c.id, schema.count - 1))
            continue
        if c.id in by_id:
            errors.append("%s: id %d already used at %s" % (c.where, c.id, by_id[c.id].where))
            continue
        by_id[c.id] = c
    missing = [i for i in range(schema.count) if i not in by_id]
    if missing:
        errors.append("missing citizen ids: %s%s" % (", ".join(map(str, missing[:20])),
                                                     " ..." if len(missing) > 20 else ""))

    sp_names = {v: k for k, v in schema.enums["species"].items()}
    by_name = {}
    bios = {}
    for i in sorted(by_id):
        c = by_id[i]
        hell = i >= schema.district
        w = "%s [%d] %s" % (c.where, i, c.name)
        # name
        if len(c.name) > schema.name_len:
            errors.append("%s: name longer than %d chars" % (w, schema.name_len))
        if not all(0x20 <= ord(ch) <= 0x7E for ch in c.name) or c.name != c.name.strip():
            errors.append("%s: name must be printable ASCII without edge spaces" % w)
        if "|" in c.name or c.name.startswith("#"):
            errors.append("%s: name may not contain '|' or start with '#'" % w)
        key = c.name.lower()
        if key in by_name:
            errors.append("%s: name already used by [%d]" % (w, by_name[key]))
        by_name.setdefault(key, i)
        # job / bio
        for k, mx in (("job", schema.job_max), ("bio", schema.bio_max)):
            t = getattr(c, k)
            if not t:
                errors.append("%s: missing %s:" % (w, k))
                setattr(c, k, "")
                continue
            if len(t) > mx:
                errors.append("%s: %s longer than %d chars (%d): %r" % (w, k, mx, len(t), t))
            if not all(0x20 <= ord(ch) <= 0x7E for ch in t):
                errors.append("%s: %s must be printable ASCII" % (w, k))
        if c.bio:
            if c.bio in bios:
                errors.append("%s: bio identical to [%d]" % (w, bios[c.bio]))
            bios.setdefault(c.bio, i)
            if not c.bio[0].isupper() and not c.bio[0].isdigit() and c.bio[0] not in "\"'":
                warnings.append("%s: bio should start with a capital letter" % w)
            if c.bio[-1] not in ".!?~\"')":
                warnings.append("%s: bio should end with punctuation" % w)
        # fill defaults
        if "species" not in c.v:
            errors.append("%s: missing species=" % w)
            c.v["species"] = 0
        d = schema.defaults(sp_names[c.v["species"]], hell)
        for k in schema.all_keys():
            if k not in c.v:
                if strict and k not in schema.OPTIONAL_KEYS:
                    errors.append("%s: --strict: missing %s=" % (w, k))
                if k == "story":
                    continue
                c.v[k] = d[k]
        # rules
        v = c.v
        F = schema.rflags
        if hell and not v["flags"] & F["HELLBORN"]:
            errors.append("%s: Hellvile citizens (ids >= %d) need flags=HELLBORN" % (w, schema.district))
        for bad in ("CUSTOM", "PLAYER", "FAVORITE", "MONSTER_KILL"):
            if v["flags"] & F[bad]:
                errors.append("%s: flag %s is player state, not allowed on premade citizens" % (w, bad))
        if ("story" in v) != bool(v["flags"] & F["STORY_NPC"]):
            errors.append("%s: story= and flags=STORY_NPC must go together" % w)
        if v["corruption"] >= 100:
            errors.append("%s: corruption must be < 100 (100 = possessed)" % w)
        if v["food"] == v["hates"]:
            errors.append("%s: food and hates are the same" % w)
        if v["highlight"] == v["hair_color"]:
            warnings.append("%s: highlight equals hair_color (use NONE)" % w)
        if v["health"] < 50:
            warnings.append("%s: premade citizens should start healthy" % w)

    # story cast
    story = {}
    for i, c in by_id.items():
        if "story" in c.v:
            n = c.v["story"]
            if n in story:
                errors.append("%s: story NPC used twice ([%d] and [%d])" % (c.where, story[n], i))
            story[n] = i
    npc_names = {v: k for k, v in schema.npcs.items()}
    for n in range(len(schema.npcs)):
        if n not in story:
            errors.append("story NPC %s has no citizen (add story=%s)" % (npc_names[n], npc_names[n]))
        elif npc_names[n] in schema.npc_ids and schema.npc_ids[npc_names[n]] != story[n]:
            errors.append("story NPC %s is [%d] but roster.h says ROSTER_ID_%s = %d"
                          % (npc_names[n], story[n], npc_names[n], schema.npc_ids[npc_names[n]]))

    # looks
    looks = {}
    for i in sorted(by_id):
        lk = tuple(by_id[i].v[k] for k in LOOK_ORDER)
        if lk in looks:
            errors.append("[%d] %s: look identical to [%d]" % (i, by_id[i].name, looks[lk]))
        looks.setdefault(lk, i)

    # edges
    edges = parse_edges(os.path.join(DATA, "edges.txt"), by_name, by_id, schema, errors)
    seen = {}
    for e in edges:
        a, b = min(e["a"], e["b"]), max(e["a"], e["b"])
        e["a"], e["b"] = a, b
        if (a, b) in seen:
            errors.append("%s: pair already related at %s" % (e["where"], seen[(a, b)]))
            continue
        seen[(a, b)] = e["where"]
        if a not in by_id or b not in by_id:
            continue
        ca, cb = by_id[a], by_id[b]
        k = e["kind"]
        if k in AUTO_BANDS:
            lo, hi = AUTO_BANDS[k]
            if not lo <= e["value"] <= hi:
                errors.append("%s: %s value should be %d..%d" % (e["where"], k, lo, hi))
        if k in ROMANTIC:
            if not (accepts(schema, ca, cb) and accepts(schema, cb, ca)):
                errors.append("%s: %s couple %s & %s does not match their romance prefs"
                              % (e["where"], k, ca.name, cb.name))
            if e["value"] < 30:
                warnings.append("%s: a %s couple with value %d" % (e["where"], k, e["value"]))
        if k == "FAMILY" and e["value"] < 0:
            warnings.append("%s: a family edge with a negative value" % e["where"])
        for w_ in e["crush"]:
            other = cb if w_ == a else ca
            if not accepts(schema, by_id[w_], other):
                errors.append("%s: %s's crush on %s does not match their romance pref"
                              % (e["where"], by_id[w_].name, other.name))
        if k == "FAMILY" and e["crush"]:
            errors.append("%s: no crushes inside a family" % e["where"])
    limit = schema.max_edges - 400
    if len(edges) > limit:
        errors.append("edges.txt: %d edges, more than DOOMO_MAX_EDGES - 400 = %d" % (len(edges), limit))
    edges.sort(key=lambda e: (e["a"], e["b"]))
    return schema, by_id, edges, story, errors, warnings


# =============================================================================
# audit (quality of the shipped roster) and stats
# =============================================================================
def audit(schema, by_id, edges, story):
    problems, lines = [], []
    hill = [by_id[i] for i in sorted(by_id) if i < schema.district]
    hell = [by_id[i] for i in sorted(by_id) if i >= schema.district]
    sp = {v: k for k, v in schema.enums["species"].items()}

    # every catalog option is used somewhere
    for key in ("species", "body", "head", "eyes", "brows", "nose", "mouth", "hair",
                "facial_hair", "ears", "horns", "tail", "wings", "halo", "marking", "top",
                "bottom", "shoes", "hat", "accessory", "aura", "pattern", "pronoun", "romance"):
        used = {c.v[key] for c in by_id.values()}
        names = {v: k for k, v in schema.enums[key].items()}
        miss = [names[n] for n in sorted(names) if n not in used]
        if miss:
            problems.append("option never used: %s = %s" % (key, ", ".join(miss)))
    for key, table in (("food", schema.foods), ("hates", schema.foods), ("hobby", schema.hobbies),
                       ("music", schema.music), ("fear", schema.fears),
                       ("phrase", schema.phrases)):
        used = {c.v[key] for c in by_id.values()}
        miss = [table[n] for n in range(len(table)) if n not in used]
        if miss and key != "hates":
            problems.append("%s never used: %s" % (key, ", ".join(miss)))
    cheek_used = 0
    for c in by_id.values():
        cheek_used |= c.v["cheeks"]
    for nm, bit in schema.cheeks.items():
        if not cheek_used & bit:
            problems.append("cheek flag never used: " + nm)
    # colour diversity: every paint shows up in the main visible colours, and
    # the secondary colour fields use most of the palette
    names = {v: k for k, v in schema.paints.items()}
    for key in ("eye_color", "hair_color", "top_color", "bottom_color", "shoes_color",
                "hat_color", "accessory_color", "marking_color", "pattern_color", "horn_color",
                "color"):
        used = {c.v[key] for c in by_id.values()}
        need = schema.paint_count if key in ("eye_color", "hair_color", "top_color", "color") \
            else schema.paint_count * 13 // 16
        if len(used) < need:
            problems.append("%s uses %d paints (want %d); missing %s" % (key, len(used), need, ", ".join(
                names[p] for p in range(schema.paint_count) if p not in used)))
    skins = {c.v["skin"] for c in by_id.values()}
    if len(skins) < 8 + schema.paint_count - 4:
        problems.append("skin uses only %d of %d values" % (len(skins), 8 + schema.paint_count))
    months = {c.v["birthday"][0] for c in by_id.values()}
    if len(months) < 12:
        problems.append("birthdays do not cover all 12 months")
    for k, lo, hi in (("height", 0, 15), ("build", 0, 15), ("eye_count", 1, 3), ("fangs", 0, 3)):
        used = {c.v[k] for c in by_id.values()}
        if len(used) < hi - lo + 1:
            problems.append("%s does not use every value %d..%d" % (k, lo, hi))

    # cast make-up
    crit = sum(1 for c in hill if sp[c.v["species"]] in CRITTERS)
    pct = 100 * crit // max(1, len(hill))
    if not 55 <= pct <= 70:
        problems.append("Hillvile critter-folk share %d%% (want ~60%%)" % pct)
    dem = sum(1 for c in hell if sp[c.v["species"]] in DEMONIC)
    hcrit = sum(1 for c in hell if sp[c.v["species"]] in CRITTERS)
    if dem < len(hell) * 50 // 100:
        problems.append("Hellvile demonic share only %d" % dem)
    if hcrit < len(hell) * 15 // 100:
        problems.append("Hellvile edgy critter-folk share only %d" % hcrit)
    soft_bit = 1 << TRAIT_NAMES.index("SOFTIE")
    soft = sum(1 for c in hell if c.v["traits"] & soft_bit)
    if not len(hell) * 12 // 100 <= soft <= len(hell) * 18 // 100:
        problems.append("Hellvile softies: %d (want ~15%%)" % soft)
    for c in by_id.values():
        if c.v["traits"] & soft_bit and not c.v["flags"] & schema.rflags["HELLBORN"]:
            problems.append("[%d] SOFTIE is for hellborn citizens" % c.id)
    pr = {}
    for c in by_id.values():
        pr[c.v["pronoun"]] = pr.get(c.v["pronoun"], 0) + 1
    they = pr.get(schema.enums["pronoun"]["THEY"], 0)
    if they < len(by_id) * 10 // 100:
        problems.append("only %d they/them citizens" % they)
    ch = sum(c.v["corruption"] for c in hill) // max(1, len(hill))
    cb = sum(c.v["corruption"] for c in hell) // max(1, len(hell))
    if cb < ch + 25:
        problems.append("Hellvile corruption (avg %d) not clearly above Hillvile (avg %d)" % (cb, ch))
    for c in by_id.values():
        if c.v["health"] < 60 or c.v["hunger"] < 40:
            problems.append("[%d] %s does not start healthy" % (c.id, c.name))
    jobs = {c.job for c in by_id.values()}
    if len(jobs) < 250:
        problems.append("only %d distinct jobs" % len(jobs))

    # relationships
    kinds = {}
    for e in edges:
        kinds[e["kind"]] = kinds.get(e["kind"], 0) + 1
    for k in schema.rel:
        if not kinds.get(k):
            problems.append("relationship kind never used: " + k)
    cross_crush = sum(1 for e in edges if e["crush"] and (e["a"] < schema.district) != (e["b"] < schema.district))
    if cross_crush < 20:
        problems.append("only %d cross-border crushes" % cross_crush)
    if not 1200 <= len(edges) <= schema.max_edges - 400:
        problems.append("%d edges (want ~1500, max %d)" % (len(edges), schema.max_edges - 400))
    lonely = [c for c in by_id.values()
              if not any(e["a"] == c.id or e["b"] == c.id for e in edges)]
    if len(lonely) > len(by_id) // 10:
        problems.append("%d citizens have no relationships at all" % len(lonely))

    # ---- stats -------------------------------------------------------------
    def hist(cs, key, names):
        h = {}
        for c in cs:
            h[c.v[key]] = h.get(c.v[key], 0) + 1
        return ", ".join("%s %d" % (names[k], n) for k, n in sorted(h.items(), key=lambda kv: -kv[1]))
    spn = {v: k for k, v in schema.enums["species"].items()}
    lines.append("Hillvile species: " + hist(hill, "species", spn))
    lines.append("Hellvile species: " + hist(hell, "species", spn))
    lines.append("critter-folk: Hillvile %d%%, Hellvile %d%%; demonic Hellvile %d%%"
                 % (pct, 100 * hcrit // max(1, len(hell)), 100 * dem // max(1, len(hell))))
    pn = {v: k for k, v in schema.enums["pronoun"].items()}
    lines.append("pronouns: " + hist(list(by_id.values()), "pronoun", pn))
    rn = {v: k for k, v in schema.enums["romance"].items()}
    lines.append("romance: " + hist(list(by_id.values()), "romance", rn))
    lines.append("avg corruption: Hillvile %d, Hellvile %d; Hellvile softies %d" % (ch, cb, soft))
    lines.append("distinct jobs %d, distinct skins %d" % (len(jobs), len(skins)))
    lines.append("edges %d: %s; cross-border crushes %d; lonely citizens %d"
                 % (len(edges), ", ".join("%s %d" % kv for kv in sorted(kinds.items())),
                    cross_crush, len(lonely)))
    names = {v: k for k, v in schema.npcs.items()}
    lines.append("story cast: " + ", ".join("%s=[%d] %s" % (names[n], story[n], by_id[story[n]].name)
                                           for n in sorted(story)))
    return problems, lines


# =============================================================================
# C emission
# =============================================================================
def c_str(s):
    out = []
    for ch in s:
        if ch in '"\\':
            out.append("\\" + ch)
        elif ch == "?":
            out.append("\\?")      # never form a trigraph
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def emit(schema, by_id, edges, story):
    v_status = schema.status_alive
    L = []
    w = L.append
    w("/* roster_data.c - GENERATED by tools/roster_build.py from core/data/roster/.")
    w(" * DO NOT EDIT: edit the text files and run `make -C core gen`.")
    w(" *")
    w(" * The 1000 premade citizens of Hellvile Park: ids 0..499 Hillvile (the good")
    w(" * side), 500..999 Hellvile (the bad side). All data is const (ROM on GBA).")
    w(" * Record field order: see DoomoResident in resident.h.")
    w(" */")
    w('#include "doomo/roster.h"')
    w("")
    w("DOOMO_STATIC_ASSERT(ROSTER_COUNT == %d, roster_count_matches_data);" % schema.count)
    w("")
    w("const DoomoResident doomo_roster_premade[ROSTER_COUNT] = {")
    for i in range(schema.count):
        c = by_id[i]
        v = c.v
        look = ",".join(str(v[k]) for k in LOOK_ORDER)
        voice = ",".join(str(v[k]) for k in ("pitch", "speed", "quality", "intonation", "accent"))
        pers = ",".join(str(v[k]) for k in ("movement", "speech", "expression", "attitude", "quirk"))
        stats = ",".join(str(v[k]) for k in ("hunger", "happy", "hygiene", "energy", "health",
                                             "sanity", "corruption", "social"))
        rest = [v_status, v["flags"], v["pronoun"], v["romance"], v["birthday"][0],
                v["birthday"][1], v["food"], v["hobby"], v["music"], v["color"], v["hates"],
                v["fear"], v["phrase"], 0, 0, v["mood"], v["activity"], 0]
        w("    /* %3d */ { {%s}, %s, {%s}, {%s}, {%s}, %s }," % (
            i, look, c_str(c.name), voice, pers, stats, ",".join(str(x) for x in rest)))
    w("};")
    w("")
    # bios
    ofs, pos = [], 0
    w("const char doomo_roster_bio_text[] =")
    for i in range(schema.count):
        b = by_id[i].bio
        ofs.append(pos)
        pos += len(b) + 1
        w("    %s \"\\0\"  /* %d */" % (c_str(b), i))
    w("    ;")
    assert pos < 65536
    w("const uint16_t doomo_roster_bio_ofs[ROSTER_COUNT] = {")
    for k in range(0, schema.count, 12):
        w("    " + ",".join(str(o) for o in ofs[k:k + 12]) + ",")
    w("};")
    w("")
    # jobs (deduplicated)
    jobs, job_pos, pos = [], {}, 0
    for i in range(schema.count):
        j = by_id[i].job
        if j not in job_pos:
            job_pos[j] = pos
            jobs.append(j)
            pos += len(j) + 1
    assert pos < 65536
    w("const char doomo_roster_job_text[] =")
    for j in jobs:
        w("    %s \"\\0\"" % c_str(j))
    w("    ;")
    w("const uint16_t doomo_roster_job_ofs[ROSTER_COUNT] = {")
    jo = [job_pos[by_id[i].job] for i in range(schema.count)]
    for k in range(0, schema.count, 12):
        w("    " + ",".join(str(o) for o in jo[k:k + 12]) + ",")
    w("};")
    w("")
    w("const uint8_t doomo_roster_traits[ROSTER_COUNT] = {")
    tr = [by_id[i].v["traits"] for i in range(schema.count)]
    for k in range(0, schema.count, 20):
        w("    " + ",".join(str(t) for t in tr[k:k + 20]) + ",")
    w("};")
    w("")
    w("/* %d relationships, sorted by (a, b), a < b. kind = REL_* | REL_CRUSH_* */" % len(edges))
    w("const DoomoEdge doomo_roster_edges[%d] = {" % max(1, len(edges)))
    ab = schema.c.values["REL_CRUSH_AB"]
    ba = schema.c.values["REL_CRUSH_BA"]
    for e in edges:
        kind = schema.rel[e["kind"]]
        if e["a"] in e["crush"]:
            kind |= ab
        if e["b"] in e["crush"]:
            kind |= ba
        w("    { %d, %d, %d, %d }," % (e["a"], e["b"], e["value"], kind))
    if not edges:
        w("    { 0, 0, 0, 0 }")
    w("};")
    w("const uint16_t doomo_roster_edge_count = %d;" % len(edges))
    w("")
    names = {v: k for k, v in schema.npcs.items()}
    w("const uint16_t doomo_story_npc[NPC_COUNT] = {")
    for n in range(len(schema.npcs)):
        w("    %d,  /* NPC_%s: %s */" % (story[n], names[n], by_id[story[n]].name.replace("*/", "")))
    w("};")
    return "\n".join(L) + "\n"


# =============================================================================
def main(argv):
    out_root = ROOT
    mode = "build"
    strict = False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--out-root" and i + 1 < len(argv):
            out_root = argv[i + 1]
            i += 1
        elif a == "--check":
            mode = "check"
        elif a == "--audit":
            mode = "audit"
            strict = True
        elif a == "--strict":
            strict = True
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        else:
            print("roster_build: unknown argument %r" % a, file=sys.stderr)
            return 2
        i += 1
    try:
        schema, by_id, edges, story, errors, warnings = load(strict)
    except RosterError as e:
        print("roster_build: error: %s" % e, file=sys.stderr)
        return 1
    for wmsg in warnings[:50]:
        print("roster_build: warning: " + wmsg, file=sys.stderr)
    if len(warnings) > 50:
        print("roster_build: ... %d more warnings" % (len(warnings) - 50), file=sys.stderr)
    if errors:
        for e in errors[:200]:
            print("roster_build: error: " + e, file=sys.stderr)
        if len(errors) > 200:
            print("roster_build: ... %d more errors" % (len(errors) - 200), file=sys.stderr)
        return 1
    if mode == "audit":
        problems, lines = audit(schema, by_id, edges, story)
        for ln in lines:
            print(ln)
        for p in problems:
            print("roster_build: audit: " + p, file=sys.stderr)
        if problems:
            return 1
        print("roster_build: audit OK (%d citizens, %d edges)" % (len(by_id), len(edges)))
        return 0
    if mode == "check":
        print("roster_build: OK (%d citizens, %d edges)" % (len(by_id), len(edges)))
        return 0
    src = emit(schema, by_id, edges, story)
    path = os.path.join(out_root, OUT_REL)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write(src)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
