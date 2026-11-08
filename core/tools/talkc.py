#!/usr/bin/env python3
"""talkc.py - the Talk word network compiler (owner: TK, T1; docs/TALK.md
"The word network" and "The database"; the binary format is in
core/include/doomo/talk.h).

Inputs (core/data/talk/, UTF-8, '#' comments):
  categories.tsv           the category tree (18 roots, talk.h TCR_*)
  concepts.lock            id<TAB>key: every concept id ever handed out
                           (ids are saved in talk memory: append only, a
                           removed key stays as "retired" and its id is
                           never reused). Written by this tool (gen mode).
  lexicon/<lang>/*.tsv     the authored concepts, one a row:
      KEY<TAB>CAT<TAB>SENT<TAB>REG<TAB>FORMS<TAB>SYNONYMS<TAB>ATTRS<TAB>FLAGS
    KEY       lowercase a-z 0-9 _ (unique over every file)
    CAT       a leaf of categories.tsv ("food.dish"), or "." for the file's
              last "@cat LEAF" line
    SENT      -3..3, or "." for the leaf's default
    REG       clean crude strong sexual hate realworld distress, or "."
    FORMS     "." (the key with spaces), "lemma", "lemma/plural" or
              "/plural"; a proper name keeps its capitals ("Mayor Giggs")
    SYNONYMS  other surface strings that mean it, comma separated, or "."
    ATTRS     concept keys said of it (adjectives, verbs), comma separated,
              at most 8, or "." for the leaf's defaults
    FLAGS     space separated: mass proper the pl a an gerund minor adult
              never, or "." (missing trailing columns are ".")
  lexicon/<lang>/gen_game.tsv   GENERATED (--seed-from-game, part of gen):
                           the catalog lists, the furniture, roster jobs,
                           items, venues and story places, the cast,
                           persona types, moods, colours and clothes; a
                           `# gen:` column says where each row came from
  lexicon/<lang>/override.tsv   hand edits of generated rows: same columns,
                           KEY must be a generated key; every column that
                           is not "." replaces the generated one
  assoc_<lang>.tsv         the REVIEWED association edges:
                           KEY<TAB>OTHER:W OTHER:W ... (W 1..15, at most 16,
                           strongest first). core/tools/talk_assoc.py writes
                           suggestions; a reviewer accepts lines into this
                           file, so no unreviewed edge ships.
  fixture.txt              the concept keys of the unit-test fixture
Also read: core/data/story/register.tsv (the register words; the classes
must agree with the lexicon's), core/gen/txt_dict.c (the story codec:
every string is encoded with it), the game sources the seed reads.

Outputs:
  gen mode (make -C core gen; --out-root DIR):
    core/data/talk/lexicon/en/gen_game.tsv, core/data/talk/concepts.lock,
    core/gen/talk_fixture.c (a small compiled database for the unit tests)
  --bin [--lang en] [--out FILE] [--align N]   TALK_<lang>.BIN
                                               (make -C core talk)
  --check     every lint, nothing written (make -C core talk-check)
  --selftest  the seeded bad inputs in core/tests/talk_bad/ must each
              fail with the rule their first line names
  --stats     counts and bank sizes

Checks (any failure exits 1): the tree's shape and the roots' order;
every row's columns; no key twice; no generated row duplicating an
authored lemma; every form drawable by the font AND encodable by the story
codec (a character the story text never uses has no code); every
attribute and association a concept; register agreement with
register.tsv (a concept registered crude / strong / sexual has a form the
register list matches with that class, a form the list matches carries
that class, and every register.tsv word has a concept); the lock (no key
without an id: run gen); byte-identical output; the codec round trip.

stdlib only, deterministic.
"""
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CORE = os.path.join(ROOT, "core")
DATA = os.path.join(CORE, "data", "talk")
sys.path.insert(0, HERE)

MAGIC = 0x4B4C4154
VERSION = 1
HDR = 20
DIRENT = 20
ALIGN = 64
MAX_CONCEPTS = 4095
TB = {"CONCEPT": 1, "ASSOC": 2, "ATTR": 3, "FORMS": 4, "CATS": 5, "CATNAMES": 6, "EDGES": 7}
TBK_RAW, TBK_TXT = 0, 1
TBF_DC_ONLY = 1
ROOTS = ["people", "places", "food", "music", "hobbies", "fears", "creatures", "house", "lore",
         "hellvile", "feelings", "work", "time", "school", "romance", "safety", "describe", "stuff"]
REGS = ["clean", "crude", "strong", "sexual", "hate", "realworld", "distress"]
AGES = ["any", "adult", "never"]
CAT_FLAGS = {"talk": 1, "gated": 2, "safety": 4, "describe": 8}
TCF = {"countable": 0x10, "mass": 0x20, "proper": 0x40, "pl": 0x80, "an": 0x100, "the": 0x200,
       "generated": 0x400, "minor": 0x800, "multiword": 0x1000, "gerund": 0x2000}
ROW_FLAGS = ("mass", "proper", "the", "pl", "a", "an", "gerund", "minor", "adult", "never", "any")
FORM_SEP = "/"
BLOCK_SHIFT = 5
FONT_EXTRA = set(range(0xA0, 0xB0))      # font.h: the glyph page letters


class Fail(Exception):
    pass


def die(where, msg):
    raise Fail("%s: %s" % (where, msg))


def rel(p):
    return os.path.relpath(p, ROOT)


# ---------------------------------------------------------------------------
# English morphology (the forms a row does not spell out)
# ---------------------------------------------------------------------------
IRREGULAR = {
    "man": "men", "woman": "women", "child": "children", "person": "people", "mouse": "mice",
    "tooth": "teeth", "foot": "feet", "goose": "geese", "sheep": "sheep", "fish": "fish",
    "deer": "deer", "ox": "oxen", "knife": "knives", "wife": "wives", "life": "lives",
    "leaf": "leaves", "loaf": "loaves", "wolf": "wolves", "half": "halves", "shelf": "shelves",
    "elf": "elves", "calf": "calves", "scarf": "scarves", "thief": "thieves", "potato": "potatoes",
    "tomato": "tomatoes", "hero": "heroes", "echo": "echoes", "volcano": "volcanoes",
    "mosquito": "mosquitoes", "cactus": "cacti", "fungus": "fungi", "moose": "moose",
    "series": "series", "species": "species", "louse": "lice", "die": "dice", "squid": "squid",
    "salmon": "salmon", "shrimp": "shrimp", "aircraft": "aircraft", "bison": "bison",
    "tuna": "tuna", "trout": "trout", "cod": "cod", "werewolf": "werewolves", "hoof": "hooves",
    "domino": "dominoes", "torpedo": "torpedoes", "buffalo": "buffaloes", "mango": "mangoes",
    "jalapeno": "jalapenos", "onigiri": "onigiri", "sushi": "sushi", "mochi": "mochi",
    "takoyaki": "takoyaki", "kimono": "kimonos", "ghoul": "ghouls", "loaf of bread": "loaves of bread",
    "cup of tea": "cups of tea", "slice of pizza": "slices of pizza", "bowl of soup": "bowls of soup",
    "mother-in-law": "mothers-in-law", "father-in-law": "fathers-in-law",
}


def plural_en(w):
    if w in IRREGULAR:
        return IRREGULAR[w]
    parts = w.rsplit(" ", 1)
    head, last = (parts[0] + " ", parts[1]) if len(parts) == 2 else ("", parts[0])
    low = last.lower()
    if low in IRREGULAR:
        p = IRREGULAR[low]
        return head + (p if last == low else p[:1].upper() + p[1:])
    if re.search(r"(s|x|z|ch|sh)$", low):
        return w + "es"
    if re.search(r"[^aeiou]y$", low):
        return w[:-1] + "ies"
    return w + "s"


AN_EXCEPT_A = ("uni", "use", "usu", "uten", "ukulele", "eu", "one", "once", "ewe", "ufo", "uranium",
               "urinal", "utopia", "ute", "uber")
AN_EXCEPT_AN = ("hour", "honest", "honour", "honor", "heir", "herb")


def wants_an(lemma):
    w = lemma.lower()
    if w.startswith(AN_EXCEPT_AN):
        return True
    if w.startswith(AN_EXCEPT_A):
        return False
    return w[:1] in "aeiou"


# ---------------------------------------------------------------------------
# the story codec (core/gen/txt_dict.c, textpack.py)
# ---------------------------------------------------------------------------
class Codec:
    def __init__(self, path):
        src = open(path, encoding="latin-1").read()

        def arr(name):
            m = re.search(r"\b%s\[\d*\]\s*=\s*\{([^}]*)\}" % re.escape(name), src)
            if not m:
                die(rel(path), "no %s" % name)
            return [int(x) for x in re.findall(r"\d+", m.group(1))]

        def scalar(name):
            m = re.search(r"\b%s\s*=\s*(\d+)\s*;" % re.escape(name), src)
            if not m:
                die(rel(path), "no %s" % name)
            return int(m.group(1))

        self.counts = arr("doomo_txt_huff_count")
        self.nsym = scalar("doomo_txt_huff_nsym")
        self.order = arr("doomo_txt_huff_sym")[:self.nsym]
        self.dict_n = scalar("doomo_txt_dict_n")
        self.dict_off = arr("doomo_txt_dict_off")
        m = re.search(r"doomo_txt_dict_chars\[\d+\]\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", src)
        if not m:
            die(rel(path), "no doomo_txt_dict_chars")
        chars = bytearray()
        for lit in re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)):
            i = 0
            while i < len(lit):
                c = lit[i]
                if c == "\\":
                    n = lit[i + 1]
                    if n in "01234567":
                        j = i + 1
                        while j < len(lit) and j < i + 4 and lit[j] in "01234567":
                            j += 1
                        chars.append(int(lit[i + 1:j], 8))
                        i = j
                        continue
                    if n == "x":
                        j = i + 2
                        while j < len(lit) and lit[j] in "0123456789abcdefABCDEF":
                            j += 1
                        chars.append(int(lit[i + 2:j], 16))
                        i = j
                        continue
                    chars.append({"n": 10, "t": 9, "\\": 92, "\"": 34, "'": 39}.get(n, ord(n)))
                    i += 2
                    continue
                chars.append(ord(c))
                i += 1
        self.dict_chars = bytes(chars)
        self.dic = [self.dict_chars[self.dict_off[k]:self.dict_off[k + 1]].decode("latin-1")
                    for k in range(self.dict_n)]
        # canonical codes from the counts and the symbol order
        self.codes = {}
        code = idx = 0
        for length in range(1, 17):
            for _ in range(self.counts[length]):
                self.codes[self.order[idx]] = (code, length)
                code += 1
                idx += 1
            code <<= 1
        self.by_first = {}
        for k, w in sorted(enumerate(self.dic), key=lambda kw: (-len(kw[1]), kw[0])):
            if w:
                self.by_first.setdefault(w[0], []).append((k, w))
        self.hash = self.fnv()

    def fnv(self):
        """txt.c txt_codec_hash(): FNV-1a 32 over the codec tables"""
        b = bytearray(self.counts[:17])
        b += self.nsym.to_bytes(2, "little")
        for s in self.order:
            b += s.to_bytes(2, "little")
        b += self.dict_n.to_bytes(2, "little")
        for o in self.dict_off[:self.dict_n + 1]:
            b += o.to_bytes(2, "little")
        b += self.dict_chars[:self.dict_off[self.dict_n]]
        h = 0x811C9DC5
        for x in b:
            h = ((h ^ x) * 0x01000193) & 0xFFFFFFFF
        return h

    def missing(self, s):
        """the characters of s with no code (the story text never uses them)"""
        return sorted(set(c for c in s if ord(c) not in self.codes))

    def tokens(self, s):
        out, i = [], 0
        while i < len(s):
            best = None
            for k, w in self.by_first.get(s[i], ()):
                if s.startswith(w, i):
                    best = (k, w)
                    break
            if best:
                out.append(256 + best[0])
                i += len(best[1])
            else:
                if ord(s[i]) not in self.codes:
                    raise Fail("character %r has no code in the story codec" % s[i])
                out.append(ord(s[i]))
                i += 1
        out.append(0)
        return out

    def encode(self, s):
        bits = []
        for t in self.tokens(s):
            c, n = self.codes[t]
            bits.extend((c >> k) & 1 for k in range(n - 1, -1, -1))
        out = bytearray()
        for i in range(0, len(bits), 8):
            v = 0
            for k, bit in enumerate(bits[i:i + 8]):
                v |= bit << (7 - k)
            out.append(v)
        return bytes(out)

    def decode(self, data, pos=0):
        out = []
        p = pos * 8
        while True:
            code = first = index = 0
            for length in range(1, 17):
                bit = (data[p >> 3] >> (7 - (p & 7))) & 1
                p += 1
                code |= bit
                n = self.counts[length]
                if code - first < n:
                    sym = self.order[index + code - first]
                    break
                index += n
                first += n
                first <<= 1
                code <<= 1
            else:
                raise Fail("bad stream")
            if sym == 0:
                return "".join(out)
            out.append(chr(sym) if sym < 256 else self.dic[sym - 256])


def codec_path(out_root=None):
    if out_root:
        p = os.path.join(out_root, "core", "gen", "txt_dict.c")
        if os.path.exists(p):
            return p
    return os.path.join(CORE, "gen", "txt_dict.c")


def drawable(c):
    o = ord(c)
    return 0x20 <= o <= 0x7E or o in FONT_EXTRA


# ---------------------------------------------------------------------------
# categories
# ---------------------------------------------------------------------------
class Cat:
    def __init__(self, key, flags, sent, reg, age, attrs, where):
        self.key, self.flags, self.sent, self.reg, self.age = key, flags, sent, reg, age
        self.attrs, self.where = attrs, where
        self.id = 0
        self.parent = 0
        self.root = key.split(".")[0]


def read_lines(path):
    if not os.path.exists(path):
        die(rel(path), "missing")
    for i, raw in enumerate(open(path, encoding="utf-8").read().split("\n")):
        line = raw.rstrip("\r")
        yield i + 1, line


def load_categories(path=None):
    path = path or os.path.join(DATA, "categories.tsv")
    cats, by_key = [], {}
    for n, line in read_lines(path):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cols = [c.strip() for c in line.split("\t")]
        if len(cols) != 6:
            die(where, "expected KEY<TAB>FLAGS<TAB>SENT<TAB>REG<TAB>AGE<TAB>ATTRS")
        key, fl, sent, reg, age, attrs = cols
        if not re.match(r"^[a-z]+(\.[a-z_]+)?$", key):
            die(where, "a category key is root or root.leaf (lowercase)")
        flags = 0
        if fl != "-":
            for f in fl.split(","):
                if f not in CAT_FLAGS:
                    die(where, "unknown flag %r" % f)
                flags |= CAT_FLAGS[f]
        if not re.match(r"^-?[0-3]$", sent):
            die(where, "SENT is -3..3")
        if reg not in REGS or age not in AGES:
            die(where, "REG is one of %s, AGE one of %s" % (" ".join(REGS), " ".join(AGES)))
        if key in by_key:
            die(where, "%s twice" % key)
        c = Cat(key, flags, int(sent), reg, age, [] if attrs == "-" else attrs.split(","), where)
        if "." in key:
            if c.root not in by_key:
                die(where, "leaf %s before its root" % key)
            prev = [x for x in cats if "." in x.key]
            if prev and prev[-1].root != c.root and any(x.root == c.root for x in prev):
                die(where, "leaf %s is not in its root's block" % key)
        cats.append(c)
        by_key[key] = c
    roots = [c.key for c in cats if "." not in c.key]
    if roots != ROOTS:
        die(rel(path), "the roots must be, in this order: %s (talk.h TCR_*)" % " ".join(ROOTS))
    if len(cats) > 255:
        die(rel(path), "%d categories: at most 255 (u8 ids)" % len(cats))
    # ids: the roots are 1..18 in TCR order, then the leaves in file order
    nid = 1
    for c in cats:
        if "." not in c.key:
            c.id = nid
            nid += 1
    for c in cats:
        if "." in c.key:
            c.id = nid
            nid += 1
            c.parent = by_key[c.root].id
    return sorted(cats, key=lambda c: c.id), by_key


# ---------------------------------------------------------------------------
# the game seed (--seed-from-game)
# ---------------------------------------------------------------------------
def c_array(path, name):
    src = open(path, encoding="latin-1").read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    m = re.search(r"\b%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\};" % re.escape(name), src, re.S)
    if not m:
        die(rel(path), "no array %s" % name)
    return re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1))


def c_struct_names(path, name):
    """the first string of each { "Name", ... } row of a struct array"""
    src = open(path, encoding="latin-1").read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    m = re.search(r"\b%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};" % re.escape(name), src, re.S)
    if not m:
        die(rel(path), "no array %s" % name)
    return re.findall(r"\{\s*\"((?:[^\"\\]|\\.)*)\"", m.group(1))


# words that keep their capital letters in a lowercased game name
PROPER_WORDS = {"Hellvile", "Hillvile", "HaHa", "HaHaland", "Cuddles", "St.", "Nana", "Grumbleguts",
                "Vex", "Bonejangles", "Giggs", "Rivet", "Bloom", "Fuzz", "Hattie", "Haha", "Pip",
                "Wisp", "Vapula", "Doomogachi", "Boreagotchi", "Nox", "Anvil", "Mascotty", "Puddin",
                "Clinker", "Charlotte", "Hellsteel", "Lulla", "Kevin", "Snoozula", "Huggins"}
# (name, lemma/plural, flags) the seed cannot guess
FOOD_FORMS = {
    "Onigiri": ("onigiri/onigiri", ""), "Soul Soup": ("soul soup", "mass"),
    "Pudding": ("pudding", "mass"), "Angel Cake": ("angel cake", "mass"), "Salad": ("salad", ""),
    "Milk": ("milk", "mass"), "Coffee": ("coffee", "mass"), "Ramen": ("ramen", "mass"),
    "Mochi": ("mochi/mochi", ""), "Pizza": ("pizza", ""), "Sushi": ("sushi", "mass"),
    "Curry": ("curry", "mass"), "Ice Cream": ("ice cream", "mass"), "Bubble Tea": ("bubble tea", "mass"),
    "Honey Toast": ("honey toast", "mass"), "Melon Bread": ("melon bread", "mass"),
    "Takoyaki": ("takoyaki", "mass"), "Omurice": ("omurice", "mass"), "Cotton Candy": ("cotton candy", "mass"),
    "Carrot Cake": ("carrot cake", "mass"), "Popcorn": ("popcorn", "mass"),
    "Fried Chicken": ("fried chicken", "mass"), "Spaghetti": ("spaghetti", "mass"),
    "Cereal": ("cereal", "mass"), "Lava Cake": ("lava cake", ""), "Black Licorice": ("black licorice", "mass"),
    "Burnt Toast": ("burnt toast", "mass"), "Sulfur Soda": ("sulfur soda", "mass"),
    "Eyeball Jelly": ("eyeball jelly", "mass"), "Grave Dirt Cake": ("grave dirt cake", "mass"),
    "Bone Broth": ("bone broth", "mass"), "Worm Gummies": ("worm gummy/worm gummies", "pl"),
    "Brussels Sprouts": ("brussels sprout/brussels sprouts", "pl"),
    "Meat Slabs": ("meat slab/meat slabs", "pl"), "Pickles": ("pickle/pickles", "pl"),
}
FEAR_FORMS = {
    "The Dark": ("dark", "the mass"), "Thunder": ("thunder", "mass"), "Heights": ("heights", "mass"),
    "Deep Water": ("deep water", "mass"), "Being Alone": ("being alone", "mass gerund"),
    "Public Speaking": ("public speaking", "mass gerund"), "Bath Time": ("bath time", "mass"),
    "Silence": ("silence", "mass"), "Holy Water": ("holy water", "mass"), "Sunlight": ("sunlight", "mass"),
    "Tax Season": ("tax season", "mass"), "Cute Things": ("cute thing/cute things", "pl"),
    "Popping Balloons": ("popping balloons", "mass gerund"),
}
MUSIC_FORMS = {"Lullabies": ("lullaby/lullabies", "pl"), "Sea Shanties": ("sea shanty/sea shanties", "pl"),
               "Hymns": ("hymn/hymns", "pl"), "Lo-fi Beats": ("lo-fi beats", "mass")}
HOBBY_FORMS = {"Video Games": ("video game/video games", "pl"), "Board Games": ("board game/board games", "pl"),
               "Seances": ("seance/seances", "pl"), "Pranks": ("prank/pranks", "pl")}
CLOTHES_FORMS = {
    "T-Shirt": "t-shirt", "Priest": "priest robe", "Nurse": "nurse outfit", "Maid": "maid outfit",
    "Armor": "armor", "Plate Armor": "plate armor", "Hazmat": "hazmat suit", "Cargo": "cargo pants",
    "Sweats": "sweatpants", "Cowboy": "cowboy hat", "Propeller": "propeller hat", "Bucket": "bucket hat",
    "Platforms": "platform shoes", "Marine Helmet": "marine helmet", "Teddy": "teddy charm",
    "Chains": "chain/chains", "Robe": "robe", "Kilt": "kilt", "Tutu": "tutu",
}
CLOTHES_SKIP = {"None", "Barefoot", "Hooves", "Claws"}
PAIRS = {"Pants": "pants", "Shorts": "shorts", "Leggings": "leggings", "Ripped Jeans": "ripped jeans",
         "Cargo": "cargo pants", "Sweats": "sweatpants", "Glasses": "glasses", "Sunglasses": "sunglasses",
         "Headphones": "headphones", "Bunny Ears": "bunny ears", "Cat Ears": "cat ears", "Overalls": "overalls",
         "Pajamas": "pajamas"}
CLOTHES_PLURAL = {"Pants", "Shorts", "Leggings", "Ripped Jeans", "Sneakers", "Boots", "Sandals",
                  "Heels", "Clogs", "Combat Boots", "Slippers", "Rollerskates", "Platforms", "Cargo",
                  "Sweats", "Glasses", "Sunglasses", "Earrings", "Bandages", "Bunny Ears", "Cat Ears",
                  "Headphones", "Chains"}
PLACE_FORMS = {
    "Your Cottage": None, "Town Hall": ("town hall", ""), "Brimstone Bites": ("Brimstone Bites", "proper"),
    "HaHa Mart": ("HaHa Mart", "proper"), "Church of St. Cuddles": ("St. Cuddles", "proper"),
    "HaHaland Hospital": ("hospital", ""), "Rivet's Workshop": ("Rivet's workshop", "proper"),
    "Hillvile Towers": ("Hillvile Towers", "proper"), "Concert Hall": ("concert hall", ""),
    "Library": ("library", ""), "Fun Fair Big Top": ("fun fair", ""), "Cloud Nine Vapor": ("Cloud Nine Vapor", "proper adult"),
    "The Pier": ("pier", ""), "The Wall Gate": ("wall gate", ""), "Graveyard": ("graveyard", ""),
    "Hellvile Pits": ("Hellvile Pits", "proper"), "Anvil's Forge": ("Anvil's forge", "proper"),
    "The Sealed Gate": ("Sealed Gate", "proper the"), "Hellvile Arena": ("Hellvile Arena", "proper"),
    "Mother Nox's Chapel": ("Mother Nox's chapel", "proper"), "Vape Factory Ruins": ("vape factory", "adult"),
    "Bone Pier": ("Bone Pier", "proper"), "Imp Market": ("imp market", ""), "Unity Stage": ("Unity Stage", "proper"),
    "Festival Stalls": ("festival stall/festival stalls", "pl"),
}
VENUE_FORMS = {"home": None, "bed": None, "the park": ("park", ""), "Brimstone Bites": ("Brimstone Bites", "proper"),
               "the shop": ("shop", ""), "St. Cuddles": ("St. Cuddles", "proper"), "the workshop": ("workshop", ""),
               "the beach": ("beach", ""), "the concert hall": ("concert hall", ""),
               "Cloud Nine Vapor": ("Cloud Nine Vapor", "proper adult"), "the hospital": ("hospital", ""),
               "the streets": None, "the night": None}
MOOD_FORMS = {"Okay": None, "Lovesick": ("lovesick", "adult"), "Possessed": ("possessed", ""),
              "Corrupted": ("corrupted", "")}
ITEM_SKIP = {"Nothing"}
# paint names that are things first (food, nature, lore): the lexicon has them as nouns
PAINT_NOUNS = {"Cherry", "Blood", "Rose", "Bubblegum", "Peach", "Rust", "Honey", "Lemon", "Lime", "Mint",
               "Forest", "Sky", "Grape", "Chocolate", "Caramel", "Bone", "Slime", "Jet", "Hellfire", "Ectoplasm"}
ITEM_FORMS = {"Gears": ("gear/gears", "pl"), "Flowers": ("flower/flowers", "pl"), "Scrap": ("scrap", "mass"),
              "Glitter": ("glitter", "mass"), "Hellsteel": ("Hellsteel", "mass proper"),
              "Plush Stuffing": ("plush stuffing", "mass"), "Soap": ("soap", "mass"),
              "Medicine": ("medicine", "mass"), "Holy Water": ("holy water", "mass"),
              "Blessed Salt": ("blessed salt", "mass"), "Perfume": ("perfume", "mass"),
              "Vape Pen": ("vape pen", "adult"), "Energy Drink": ("energy drink", ""),
              "Lost Bunny": None, "Gate Sigil": ("gate sigil", ""), "Doomogachi Egg": ("Boreagotchi egg", ""),
              "Boreagotchi Egg": ("Boreagotchi egg", "")}
ITEM_LORE = {"Holy Water", "Sage Bundle", "Plush Cross", "Rosary", "Blessed Salt", "Hymn Book", "Gate Sigil",
             "Demon Horn", "Cellar Key", "Doomogachi Egg", "Boreagotchi Egg"}
# the story cast: concept lemma, roster id (its roster name must match), flags
CAST = [("Mayor Giggs", 0), ("Nana Puddin", 1), ("Bonejangles", 5), ("Rivet", 13), ("Pip", 42),
        ("Hattie Haha", 71), ("Father Fuzz", 77), ("Mascotty", 106), ("Dr. Bloom", 111), ("Wisp", 563),
        ("Charlotte", 566), ("Anvil", 595), ("Vex", 666), ("Grumbleguts", 729), ("Clinker", 753),
        ("Mother Nox", 778), ("Lord Vapula", 999)]


NAME_WORDS = set()     # every word of a roster name (seed_rows fills it)
TITLES = ("Mr.", "Mrs.", "Ms.", "Dr.", "St.", "Father", "Mother", "Lord", "Lady", "Nana", "Sir", "Miss")


def lower_name(name):
    """a game name in running text: lowercase, but names keep their capitals
    (a cast or title word, a possessive "Nana's", a title and the word after
    it, ALLCAPS); "X (Y)" becomes "Y X"; a leading "The " goes"""
    m = re.match(r"^(.*\S)\s*\(([^)]*)\)$", name)
    if m:
        name = m.group(2) + " " + m.group(1)
    name = name.rstrip(".").replace(": ", " ").replace(":", "")
    if name.startswith("The ") or name.startswith("the "):
        name = name[4:]
    words = name.split(" ")
    out = []
    keep_next = False
    for w in words:
        base = re.sub(r"'s$", "", w)
        keep = (keep_next or w in TITLES or base in PROPER_WORDS or
                (w.endswith("'s") and base in NAME_WORDS) or re.match(r"^[A-Z]{2,}", w))
        keep_next = w in TITLES
        out.append(w if keep else w.lower())
    return " ".join(out)


def keyify(s):
    k = re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")
    return k


def roster_entries():
    d = os.path.join(CORE, "data", "roster")
    names, jobs, traits, cur = {}, {}, {}, None
    for f in sorted(os.listdir(d)):
        if not f.endswith(".txt") or f == "edges.txt":
            continue
        for line in open(os.path.join(d, f), encoding="utf-8"):
            m = re.match(r"^\[(\d+)\]\s+(.*\S)", line)
            if m:
                cur = int(m.group(1))
                names[cur] = m.group(2)
                continue
            m = re.match(r"^job:\s*(.*\S)", line)
            if m and cur is not None:
                jobs[cur] = m.group(1)
            m = re.search(r"(?:^|\s)traits=(\S+)", line.split("#", 1)[0])
            if m and cur is not None:
                traits[cur] = m.group(1)
    return names, jobs, traits


def furniture_names():
    path = os.path.join(CORE, "data", "furn", "catalog.txt")
    out = []
    for line in open(path, encoding="utf-8"):
        m = re.match(r"^\[(\w+)\]\s+\w+\s+-?\d+\s+\"([^\"]*)\"", line)
        if m:
            out.append((m.group(1), m.group(2)))
    return out


def seed_rows():
    """the generated rows: (key, cat, forms, flags, src) in a fixed order"""
    rows = []
    seen = {}

    def add(name, cat, forms, flags, src, key=None):
        if forms is None:
            return
        fs = forms.split(FORM_SEP)
        lemma = fs[0]
        for f in fs:
            if f.lower() in seen:
                seen[f.lower()][4] += "; " + src
                return
        k = key or keyify(lemma)
        if any(r[0] == k for r in rows):
            k = keyify(cat.split(".")[1] + " " + lemma)
        r = [k, cat, forms, flags, src]
        for f in fs:
            seen[f.lower()] = r
        rows.append(r)

    cat_c = os.path.join(CORE, "src", "catalog.c")
    sim_c = os.path.join(CORE, "src", "sim.c")
    # the cast first: their names are the strongest claim on a lemma
    names, jobs, traits = roster_entries()
    NAME_WORDS.clear()
    for nm in names.values():
        NAME_WORDS.update(nm.split(" "))
    for lemma, rid in CAST:
        if names.get(rid) != lemma:
            die("talkc CAST", "roster id %d is %r, not %r" % (rid, names.get(rid), lemma))
        add(lemma, "lore.cast", lemma, "proper", "roster %d" % rid)
    for n in c_array(cat_c, "doomo_species_names"):
        add(n, "creatures.species", n.lower(), "", "catalog.c species")
    for n in c_array(cat_c, "doomo_food_names"):
        if n in FOOD_FORMS:
            f, fl = FOOD_FORMS[n]
        elif n.endswith("s") and not n.endswith("ss"):
            low = lower_name(n)
            sing = low[:-3] + "y" if low.endswith("ies") else low[:-1]
            f, fl = sing + FORM_SEP + low, "pl"
        else:
            f, fl = lower_name(n), ""
        add(n, "food.catalog", f, fl, "catalog.c foods")
    for n in c_array(cat_c, "doomo_hobby_names"):
        if n in HOBBY_FORMS:
            f, fl = HOBBY_FORMS[n]
        elif n.endswith("ing"):
            f, fl = lower_name(n), "mass gerund"
        else:
            f, fl = lower_name(n), "mass"
        add(n, "hobbies.catalog", f, fl, "catalog.c hobbies")
    for n in c_array(cat_c, "doomo_music_names"):
        f, fl = MUSIC_FORMS.get(n, (lower_name(n), "mass"))
        add(n, "music.genre", f, fl, "catalog.c music")
    for n in c_array(cat_c, "doomo_fear_names"):
        if n in FEAR_FORMS:
            f, fl = FEAR_FORMS[n]
        elif n.endswith("s"):
            low = lower_name(n)
            sing = low[:-3] + "y" if low.endswith("ies") else (low[:-2] if low.endswith("ses") else low[:-1])
            f, fl = sing + FORM_SEP + low, "pl"
        else:
            f, fl = lower_name(n), ""
        add(n, "fears.catalog", f, fl, "catalog.c fears")
    for n in c_array(sim_c, "doomo_mood_names"):
        f = MOOD_FORMS.get(n, (n.lower(), ""))
        if f:
            add(n, "feelings.mood", f[0], f[1], "sim.c moods")
    for n in c_array(cat_c, "doomo_persona_type_names"):
        add(n, "people.persona", n.lower(), "", "catalog.c persona types")
    for n in c_array(sim_c, "venue_names"):
        v = VENUE_FORMS.get(n, (n, ""))
        if v:
            add(n, "places.venue", v[0], v[1], "sim.c venues")
    for n in c_array(os.path.join(CORE, "src", "story_places.c"), "place_names"):
        if not n:
            continue
        v = PLACE_FORMS.get(n, (n, "proper"))
        if v:
            add(n, "places.story", v[0], v[1], "story_places.c")
    for n in c_array(os.path.join(CORE, "src", "palette.c"), "doomo_paint_names"):
        if n in PAINT_NOUNS:
            continue
        add(n, "describe.colour", n.lower(), "", "palette.c paints")
    for arr, cat in (("doomo_top_names", "people.clothes"), ("doomo_bottom_names", "people.clothes"),
                     ("doomo_shoes_names", "people.clothes"), ("doomo_hat_names", "people.accessory"),
                     ("doomo_acc_names", "people.accessory")):
        for n in c_array(cat_c, arr):
            if n in CLOTHES_SKIP:
                continue
            f = CLOTHES_FORMS.get(n, lower_name(n))
            fl = ""
            if n in PAIRS:
                f, fl = PAIRS[n] + FORM_SEP + PAIRS[n], "pl"
            elif n in CLOTHES_PLURAL:
                if FORM_SEP not in f:
                    low = f
                    if low.endswith("es") and low[:-2].endswith(("ss", "sh", "ch")):
                        sing = low[:-2]
                    elif low.endswith("s"):
                        sing = low[:-1]
                    else:
                        sing = low
                    f = sing + FORM_SEP + low
                fl = "pl"
            add(n, cat, f, fl, "catalog.c " + arr[6:-6])
    for n in c_struct_names(os.path.join(CORE, "src", "items.c"), "doomo_items"):
        if n in ITEM_SKIP:
            continue
        v = ITEM_FORMS.get(n, (lower_name(n), ""))
        if v:
            add(n, "lore.item" if n in ITEM_LORE else "stuff.item", v[0], v[1], "items.c")
    for key, n in furniture_names():
        add(n, "house.furniture", lower_name(n), "", "furn/catalog.txt [%s]" % key)
    # roster jobs: a job only kids and teens hold is a kid thing, not work
    holders = {}
    for rid, j in sorted(jobs.items()):
        holders.setdefault(j, []).append(rid)
    for j in sorted(holders):
        kid = all(re.search(r"\b(KID|TEEN)\b", traits.get(r, "")) for r in holders[j])
        add(j, "school.kid" if kid else "work.job", lower_name(j), "", "roster job x%d" % len(holders[j]))
    return rows


def write_gen(rows, path):
    lines = ["# GENERATED by core/tools/talkc.py --seed-from-game (make -C core gen) - do not edit;",
             "# hand edits of these rows go in override.tsv (docs/TALK.md). Columns as the",
             "# authored lexicon: KEY CAT SENT REG FORMS SYNONYMS ATTRS FLAGS, then # gen: where.",
             ""]
    for k, cat, forms, flags, src in rows:
        default_forms = forms == k.replace("_", " ")
        lines.append("\t".join([k, cat, ".", ".", "." if default_forms else forms, ".", ".",
                                flags if flags else ".", "# gen: " + src]))
    text = "\n".join(lines) + "\n"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


# ---------------------------------------------------------------------------
# the lexicon
# ---------------------------------------------------------------------------
class Concept:
    __slots__ = ("key", "cat", "sent", "reg", "age", "lemma", "plural", "syn", "attrs", "flags",
                 "where", "generated", "id", "attr_ids", "fl", "explicit_forms")


def parse_row(cols, where, cats, cur_cat, override=False):
    while len(cols) < 8:
        cols.append(".")
    key, cat, sent, reg, forms, syn, attrs, flags = [c.strip() or "." for c in cols[:8]]
    if not re.match(r"^[a-z0-9_]+$", key):
        die(where, "KEY %r: lowercase a-z 0-9 _" % key)
    if cat == ".":
        cat = cur_cat
    if override and cat is None:
        pass
    elif not cat or cat not in cats or "." not in cat:
        die(where, "CAT %r is not a leaf of categories.tsv" % cat)
    if sent != "." and not re.match(r"^-?[0-3]$", sent):
        die(where, "SENT is -3..3 or .")
    if reg != "." and reg not in REGS:
        die(where, "REG is one of %s" % " ".join(REGS))
    fl = [] if flags == "." else flags.split()
    for f in fl:
        if f not in ROW_FLAGS:
            die(where, "unknown flag %r" % f)
    return dict(key=key, cat=cat, sent=sent, reg=reg, forms=forms, syn=syn, attrs=attrs, flags=fl)


def read_lexicon_file(path, cats, generated=False, override=False):
    rows, cur = [], None
    for n, line in read_lines(path):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line.startswith("@cat"):
            parts = line.split()
            if len(parts) != 2 or parts[1] not in cats or "." not in parts[1]:
                die(where, "@cat LEAF")
            cur = parts[1]
            continue
        cols = line.split("\t")
        if cols and cols[-1].lstrip().startswith("#"):
            cols = cols[:-1]
        r = parse_row(cols, where, cats, cur, override)
        r["where"] = where
        r["generated"] = generated
        rows.append(r)
    return rows


def make_concept(r, cats):
    c = Concept()
    cat = cats[r["cat"]]
    c.key, c.cat, c.where, c.generated = r["key"], cat, r["where"], r["generated"]
    c.sent = cat.sent if r["sent"] == "." else int(r["sent"])
    c.reg = cat.reg if r["reg"] == "." else r["reg"]
    fl = set(r["flags"])
    c.age = "never" if "never" in fl else "adult" if "adult" in fl else "any" if "any" in fl else cat.age
    if c.reg in ("sexual", "hate", "realworld", "distress"):
        c.age = "never"
    forms = r["forms"]
    c.explicit_forms = forms != "."
    base = c.key.replace("_", " ")
    if forms == ".":
        lemma, plural = base, None
    elif forms.startswith(FORM_SEP):
        lemma, plural = base, forms[1:]
    elif FORM_SEP in forms:
        lemma, plural = forms.split(FORM_SEP, 1)
    else:
        lemma, plural = forms, None
    if not lemma or (plural is not None and not plural) or FORM_SEP in (plural or ""):
        die(c.where, "FORMS: lemma or lemma/plural")
    flags = 0
    proper = "proper" in fl
    mass = "mass" in fl or "gerund" in fl
    if proper:
        flags |= TCF["proper"]
    if "the" in fl:
        flags |= TCF["the"]
    if mass:
        flags |= TCF["mass"]
    if "gerund" in fl:
        flags |= TCF["gerund"]
    if "minor" in fl:
        flags |= TCF["minor"]
    if r["generated"]:
        flags |= TCF["generated"]
    if " " in lemma:
        flags |= TCF["multiword"]
    if not proper and not mass and cat.flags & (CAT_FLAGS["talk"] | CAT_FLAGS["safety"]) and \
            not (cat.flags & CAT_FLAGS["describe"]):
        if plural is None:
            plural = plural_en(lemma)
        flags |= TCF["countable"]
    elif plural is not None:
        flags |= TCF["countable"]
    if "pl" in fl:
        if plural is None:
            die(c.where, "flag pl needs a plural")
        flags |= TCF["pl"] | TCF["countable"]
    if "an" in fl or ("a" not in fl and wants_an(plural if "pl" in fl else lemma)):
        flags |= TCF["an"]
    if not (flags & TCF["countable"]):
        plural = None
    c.lemma, c.plural = lemma, plural
    c.syn = [] if r["syn"] == "." else [s.strip() for s in r["syn"].split(",") if s.strip()]
    c.attrs = list(cat.attrs) if r["attrs"] == "." else [a.strip() for a in r["attrs"].split(",") if a.strip()]
    c.fl = flags
    return c


def apply_override(gen_rows, path, cats):
    if not os.path.exists(path):
        return gen_rows
    by_key = {r["key"]: r for r in gen_rows}
    for o in read_lexicon_file(path, cats, override=True):
        if o["key"] not in by_key:
            die(o["where"], "override of %s: not a generated key" % o["key"])
        g = by_key[o["key"]]
        for col in ("sent", "reg", "forms", "syn", "attrs"):
            if o[col] != ".":
                g[col] = o[col]
        if o["cat"] is not None:
            g["cat"] = o["cat"]
        if o["flags"]:
            g["flags"] = o["flags"]
        g["where"] = g["where"] + " + " + o["where"]
    return gen_rows


def load_lexicon(lang, cats, gen_rows=None, extra=()):
    d = os.path.join(DATA, "lexicon", lang)
    if gen_rows is None:
        gen_rows = read_lexicon_file(os.path.join(d, "gen_game.tsv"), cats, generated=True)
    gen_rows = apply_override(gen_rows, os.path.join(d, "override.tsv"), cats)
    authored = []
    for f in sorted(os.listdir(d)):
        if f.endswith(".tsv") and not f.startswith("gen_") and f != "override.tsv":
            authored += read_lexicon_file(os.path.join(d, f), cats)
    for f in extra:
        authored += read_lexicon_file(f, cats)
    concepts, by_key, by_lemma, dups = [], {}, {}, []
    for r in authored + gen_rows:
        c = make_concept(r, cats)
        if c.key in by_key:
            dups.append("%s: %s is also %s" % (c.where, c.key, by_key[c.key].where))
            continue
        low = c.lemma.lower()
        if low in by_lemma:
            o = by_lemma[low]
            if c.generated and not o.generated:
                dups.append("%s: authored %r duplicates the generated %s (%s): drop the authored row or "
                            "edit the generated one in override.tsv" % (o.where, c.lemma, c.key, c.where))
            else:
                dups.append("%s: lemma %r is also %s (%s)" % (c.where, c.lemma, o.key, o.where))
            continue
        by_key[c.key] = c
        by_lemma[low] = c
        concepts.append(c)
    # a synonym is one concept's: it may not be another's lemma or synonym
    owner = {}
    for c in concepts:
        owner.setdefault(c.lemma.lower(), c)
    for c in concepts:
        for sy in c.syn:
            o = owner.get(sy.lower())
            if o is c:
                dups.append("%s: %s: synonym %r is its own lemma or listed twice" % (c.where, c.key, sy))
            elif o is not None:
                dups.append("%s: %s: synonym %r is also %s (%s)" % (c.where, c.key, sy, o.key, o.where))
            else:
                owner[sy.lower()] = c
    return concepts, by_key, dups


# ---------------------------------------------------------------------------
# the lock (concept ids)
# ---------------------------------------------------------------------------
def load_lock(path):
    ids, retired = {}, set()
    if not os.path.exists(path):
        return ids, retired
    for n, line in read_lines(path):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) not in (2, 3) or not cols[0].isdigit() or (len(cols) == 3 and cols[2] != "retired"):
            die(where, "expected ID<TAB>KEY[<TAB>retired]")
        i = int(cols[0])
        if i in ids.values() or cols[1] in ids or i < 1 or i > MAX_CONCEPTS:
            die(where, "id %d / key %s twice or out of range" % (i, cols[1]))
        ids[cols[1]] = i
        if len(cols) == 3:
            retired.add(cols[1])
    return ids, retired


def update_lock(ids, retired, keys):
    """append the new keys (sorted, after the highest id); mark the gone ones
    retired; returns the new (ids, retired)"""
    ids = dict(ids)
    keyset = set(keys)
    retired = set(k for k in ids if k not in keyset)
    nxt = max(ids.values()) + 1 if ids else 1
    for k in sorted(k for k in keys if k not in ids):
        ids[k] = nxt
        nxt += 1
    if nxt - 1 > MAX_CONCEPTS:
        raise Fail("concepts.lock: %d ids, over the 12-bit limit %d" % (nxt - 1, MAX_CONCEPTS))
    return ids, retired


def write_lock(ids, retired, path):
    lines = ["# GENERATED and APPEND ONLY: core/data/talk/concepts.lock (core/tools/talkc.py,",
             "# make -C core gen). Concept ids are saved in talk memory (docs/TALK.md 6.8):",
             "# a key keeps its id forever, a removed key is marked retired and its id is",
             "# never handed out again. Edit the lexicon, never this file.", ""]
    for k, i in sorted(ids.items(), key=lambda kv: kv[1]):
        lines.append("%d\t%s%s" % (i, k, "\tretired" if k in retired else ""))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------------------
# associations
# ---------------------------------------------------------------------------
def load_assoc(path, by_key):
    edges = {}
    if not os.path.exists(path):
        return edges
    for n, line in read_lines(path):
        where = "%s:%d" % (rel(path), n)
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) != 2:
            die(where, "expected KEY<TAB>OTHER:W ...")
        k = cols[0].strip()
        if k not in by_key:
            die(where, "%s is not a concept" % k)
        if k in edges:
            die(where, "%s twice" % k)
        out = []
        for t in cols[1].split():
            m = re.match(r"^([a-z0-9_]+):(\d+)$", t)
            if not m or not 1 <= int(m.group(2)) <= 15:
                die(where, "%r: OTHER:W with W 1..15" % t)
            if m.group(1) not in by_key:
                die(where, "%s -> %s: not a concept" % (k, m.group(1)))
            if m.group(1) == k or any(o == m.group(1) for o, _ in out):
                die(where, "%s -> %s: itself or twice" % (k, m.group(1)))
            out.append((m.group(1), int(m.group(2))))
        if len(out) > 16:
            die(where, "%s: %d edges, at most 16" % (k, len(out)))
        if any(out[i][1] < out[i + 1][1] for i in range(len(out) - 1)):
            die(where, "%s: strongest first" % k)
        edges[k] = out
    return edges


def is_adult(c):
    return c.age == "adult" or bool(c.cat.flags & CAT_FLAGS["gated"])


def check_edges(edges, by_key):
    """no edge touches a describe or safety concept or a never-generated one;
    an edge INTO an adults-only concept comes only FROM an adults-only one
    (topic activation never walks from kid-safe talk to adult talk)"""
    out = []
    for k, es in sorted(edges.items()):
        a = by_key[k]
        for o, w in es:
            b = by_key[o]
            for c in (a, b):
                if not c.cat.flags & CAT_FLAGS["talk"] or c.age == "never":
                    out.append("assoc: %s -> %s: %s is not a talkable concept" % (k, o, c.key))
                    break
            else:
                if is_adult(b) and not is_adult(a):
                    out.append("assoc: %s -> %s: an edge into an adults-only concept from one that is not" % (k, o))
    return out


# ---------------------------------------------------------------------------
# lints
# ---------------------------------------------------------------------------
def surfaces(c):
    s = [c.lemma] + ([c.plural] if c.plural else []) + c.syn
    return s


def check_concepts(concepts, by_key, codec, register_words):
    problems = []
    for c in concepts:
        if len(c.attrs) > 8:
            problems.append("%s: %s has %d attributes, at most 8" % (c.where, c.key, len(c.attrs)))
        for a in c.attrs:
            if a not in by_key:
                problems.append("%s: %s: attribute %r is not a concept" % (c.where, c.key, a))
            elif a == c.key:
                problems.append("%s: %s is its own attribute" % (c.where, c.key))
        for s in surfaces(c):
            bad = [ch for ch in s if not drawable(ch)]
            if bad:
                problems.append("%s: %s: %r has characters the font cannot draw: %r" % (c.where, c.key, s, bad))
            miss = codec.missing(s)
            if miss and c.age != "never":
                problems.append("%s: %s: %r uses %r, which the story codec has no code for" % (c.where, c.key, s, miss))
            if FORM_SEP in s and s != c.lemma:
                problems.append("%s: %s: %r contains the form separator '/'" % (c.where, c.key, s))
            if s != s.strip() or "  " in s or "\t" in s:
                problems.append("%s: %s: %r has stray spaces" % (c.where, c.key, s))
        if FORM_SEP in c.lemma:
            problems.append("%s: %s: the lemma contains '/'" % (c.where, c.key))
        if c.cat.flags & CAT_FLAGS["safety"] and c.age != "never":
            problems.append("%s: %s is in a safety leaf but may be generated" % (c.where, c.key))
        if c.cat.flags & CAT_FLAGS["gated"] and c.age == "any":
            problems.append("%s: %s is romance but not adults-only" % (c.where, c.key))
    # register agreement (register.tsv <-> the lexicon)
    rank = {"clean": 0, "crude": 1, "strong": 2, "sexual": 3}
    matched_words = set()
    for c in concepts:
        hit = None
        for s in surfaces(c):
            for w in register_words:
                if w.rx.search(s):
                    matched_words.add(w.text)
                    if hit is None or rank[w.cls] > rank[hit.cls]:
                        hit = w
        if c.reg in ("crude", "strong", "sexual"):
            if hit is None:
                problems.append("%s: %s is %s but register.tsv lists none of %r (add the word there)"
                                % (c.where, c.key, c.reg, surfaces(c)))
            elif hit.cls != c.reg:
                problems.append("%s: %s is %s, register.tsv says %s (%s)" % (c.where, c.key, c.reg, hit.cls, hit.text))
        elif hit is not None and c.reg in ("clean",):
            problems.append("%s: %s is clean but register.tsv lists %r as %s" % (c.where, c.key, hit.text, hit.cls))
    for w in register_words:
        if w.text not in matched_words:
            problems.append("%s: register word %r has no concept in the talk lexicon (the parser must know it)"
                            % (w.where, w.text))
    return problems


# ---------------------------------------------------------------------------
# the binary
# ---------------------------------------------------------------------------
def u16(v):
    return (v & 0xFFFF).to_bytes(2, "little")


def u32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def txt_bank(strings, codec):
    streams = [codec.encode(s) for s in strings]
    for s, st in zip(strings, streams):
        if codec.decode(st) != s:
            raise Fail("codec round trip failed for %r" % s)
    nblocks = (len(strings) + (1 << BLOCK_SHIFT) - 1) >> BLOCK_SHIFT
    bases, offs, data = [], [], b""
    for i, st in enumerate(streams):
        if i % (1 << BLOCK_SHIFT) == 0:
            bases.append(len(data))
        off = len(data) - bases[-1]
        if off > 0xFFFF:
            raise Fail("a string block over 64 KB")
        offs.append(off)
        data += st
    head = u16(len(strings)) + bytes((BLOCK_SHIFT, 0)) + u32(nblocks)
    hdr_len = len(head) + 4 * nblocks + 2 * len(strings)
    body = head + b"".join(u32(hdr_len + b) for b in bases) + b"".join(u16(o) for o in offs) + data
    raw = sum(len(s.encode("latin-1")) + 1 for s in strings)
    return body, raw


def form_string(c):
    return c.lemma + (FORM_SEP + c.plural if c.plural else "")


def build_banks(concepts, n_ids, cats, edges, codec, id_of):
    """concepts: Concept with .id set (1..n_ids; gaps allowed); returns [(name, kind, flags, bytes, raw)]"""
    by_id = {c.id: c for c in concepts}
    attr_list = []
    concept_rows = bytearray(8 * (n_ids + 1))
    forms = [""] * (n_ids + 1)
    for i in range(1, n_ids + 1):
        c = by_id.get(i)
        if c is None:
            continue
        aids = [id_of[a] for a in c.attrs if a in id_of]
        first = len(attr_list)
        attr_list += aids
        sent = c.sent + 3
        b0 = c.cat.id
        b1 = sent | (REGS.index(c.reg) << 3) | (AGES.index(c.age) << 6)
        flags = (c.fl & ~0xF) | len(aids)
        row = bytes((b0, b1)) + u16(first if aids else 0) + u16(i) + u16(flags)
        concept_rows[8 * i:8 * i + 8] = row
        forms[i] = form_string(c)
    assoc = bytearray(16 * (n_ids + 1))
    edge_first, edge_data = [], []
    for i in range(0, n_ids + 2):
        edge_first.append(len(edge_data))
        c = by_id.get(i)
        if c is None or i > n_ids:
            continue
        es = [(id_of[o], w) for o, w in edges.get(c.key, []) if o in id_of]
        for j, (oid, w) in enumerate(es[:8]):
            assoc[16 * i + 2 * j:16 * i + 2 * j + 2] = u16(oid | (w << 12))
        edge_data += [oid | (w << 12) for oid, w in es]
    edges_b = b"".join(u16(x) for x in edge_first) + b"".join(u16(x) for x in edge_data)
    attr_b = b"".join(u16(x) for x in attr_list) or u16(0)
    forms_b, forms_raw = txt_bank(forms, codec)
    cat_rows = u16(len(cats)) + u16(0) + bytes(4)        # row 0: none
    names = [""]
    for k, c in enumerate(cats):
        assert c.id == k + 1
        cat_rows += bytes((c.parent, c.flags)) + u16(k + 1)
        names.append(c.key)
    names_b, names_raw = txt_bank(names, codec)
    return [
        ("CONCEPT", TBK_RAW, 0, bytes(concept_rows), len(concept_rows)),
        ("ASSOC", TBK_RAW, 0, bytes(assoc), len(assoc)),
        ("ATTR", TBK_RAW, 0, attr_b, len(attr_b)),
        ("FORMS", TBK_TXT, 0, forms_b, forms_raw),
        ("CATS", TBK_RAW, 0, cat_rows, len(cat_rows)),
        ("CATNAMES", TBK_TXT, 0, names_b, names_raw),
        ("EDGES", TBK_RAW, TBF_DC_ONLY, edges_b, len(edges_b)),
    ]


def pack_bin(banks, lang_byte, n_ids, codec, align=ALIGN):
    nb = len(banks)
    pos = HDR + DIRENT * nb
    pos = (pos + align - 1) // align * align
    offs = []
    for name, kind, fl, data, raw in banks:
        offs.append(pos)
        pos = (pos + len(data) + align - 1) // align * align
    total = offs[-1] + len(banks[-1][3])
    out = bytearray(total)
    out[0:HDR] = u32(MAGIC) + u16(VERSION) + u16(lang_byte) + u32(codec.hash) + u16(nb) + u16(n_ids) + u32(total)
    for k, (name, kind, fl, data, raw) in enumerate(banks):
        crc = zlib.crc32(data) & 0xFFFFFFFF
        out[HDR + DIRENT * k:HDR + DIRENT * (k + 1)] = u16(TB[name]) + bytes((kind, fl)) + u32(offs[k]) + \
            u32(len(data)) + u32(raw) + u32(crc)
        out[offs[k]:offs[k] + len(data)] = data
    return bytes(out)


def lang_byte(code):
    path = os.path.join(CORE, "lang", "languages.tsv")
    for n, line in read_lines(path):
        if not line.strip() or line.startswith("#"):
            continue
        cols = line.split("\t")
        if cols[0] == code:
            return int(cols[1])
    die(rel(path), "no language %r" % code)


# ---------------------------------------------------------------------------
# the fixture (core/gen/talk_fixture.c)
# ---------------------------------------------------------------------------
def fixture_keys(path, by_key):
    keys = []
    for n, line in read_lines(path):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        for k in line.split():
            if k not in by_key:
                die("%s:%d" % (rel(path), n), "%s is not a concept" % k)
            if k not in keys:
                keys.append(k)
    # the attributes the fixture's concepts use come along
    more = []
    for k in keys:
        for a in by_key[k].attrs:
            if a not in keys and a not in more:
                more.append(a)
    return keys + more


def build_fixture(concepts, by_key, cats, edges, codec, fixture_path):
    keys = fixture_keys(fixture_path, by_key)
    sub = [by_key[k] for k in keys]
    id_of = {k: i + 1 for i, k in enumerate(keys)}
    old = {}
    for c in sub:
        old[c.key] = c.id
        c.id = id_of[c.key]
    try:
        sub_edges = {k: [(o, w) for o, w in edges.get(k, []) if o in id_of] for k in keys}
        banks = build_banks(sub, len(keys), cats, sub_edges, codec, id_of)
        blob = pack_bin(banks, 0, len(keys), codec)
    finally:
        for c in sub:
            c.id = old[c.key]
    return blob, keys


def c_fixture(blob, keys, codec):
    words = []
    b = blob + b"\0" * ((-len(blob)) & 3)
    for i in range(0, len(b), 4):
        words.append(b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24))
    lines = ["/* GENERATED by core/tools/talkc.py from core/data/talk/ (fixture.txt) - do not edit. */",
             "/* The unit tests' Talk database (test_talk_db.c): the concepts fixture.txt names",
             " * and the attributes they use, ids 1..%d in that order, their reviewed" % len(keys),
             " * associations among themselves, every category. Encoded with the story codec",
             " * of core/gen/txt_dict.c (dict_hash 0x%08X): a story text change regenerates it" % codec.hash,
             " * (make -C core gen). Little endian u32 words. Linked only by the tests. */",
             "#include <stdint.h>", "",
             "const uint32_t doomo_talk_fixture_size = %du;" % len(blob),
             "const uint32_t doomo_talk_fixture[%d] = {" % len(words)]
    for i in range(0, len(words), 6):
        lines.append("    " + ", ".join("0x%08Xu" % w for w in words[i:i + 6]) + ",")
    lines.append("};")
    lines.append("const uint16_t doomo_talk_fixture_nkeys = %d;" % len(keys))
    lines.append("const char *const doomo_talk_fixture_keys[%d] = {" % len(keys))
    for i in range(0, len(keys), 6):
        lines.append("    " + " ".join('"%s",' % k for k in keys[i:i + 6]))
    lines.append("};")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# driver
# ---------------------------------------------------------------------------
class Build:
    def __init__(self, lang="en", out_root=None, gen_rows=None, write_lock_ok=False, extra=()):
        import story_register as sr
        self.lang = lang
        self.cats_list, self.cats = load_categories()
        self.codec = Codec(codec_path(out_root))
        if gen_rows is None and write_lock_ok:
            gen_rows = [self.gen_to_row(r) for r in seed_rows()]
        self.concepts, self.by_key, dups = load_lexicon(lang, self.cats, gen_rows, extra)
        lock_path = os.path.join(DATA, "concepts.lock")
        ids, retired = load_lock(lock_path)
        keys = [c.key for c in self.concepts]
        if write_lock_ok:
            ids, retired = update_lock(ids, retired, keys)
        else:
            missing = [k for k in keys if k not in ids or k in retired]
            if missing:
                raise Fail("%s: %d concepts have no id yet (%s...): run make -C core gen"
                           % (rel(lock_path), len(missing), ", ".join(missing[:5])))
        self.ids, self.retired = ids, retired
        for c in self.concepts:
            c.id = ids[c.key]
        self.n_ids = max(ids.values()) if ids else 0
        self.id_of = {c.key: c.id for c in self.concepts}
        self.edges = load_assoc(os.path.join(DATA, "assoc_%s.tsv" % lang), self.by_key)
        edge_problems = check_edges(self.edges, self.by_key)
        self.register = sr.load_register()
        self.problems = dups + edge_problems + check_concepts(self.concepts, self.by_key, self.codec,
                                                               self.register)

    @staticmethod
    def gen_to_row(r):
        k, cat, forms, flags, src = r
        return dict(key=k, cat=cat, sent=".", reg=".", forms=forms if forms != k.replace("_", " ") else ".",
                    syn=".", attrs=".", flags=flags.split() if flags else [], where="gen_game.tsv (%s)" % src,
                    generated=True)

    def fail_on_problems(self):
        if self.problems:
            for p in self.problems[:2000]:
                print("talkc: " + p, file=sys.stderr)
            if len(self.problems) > 400:
                print("talkc: ... and %d more" % (len(self.problems) - 400), file=sys.stderr)
            raise Fail("%d problem(s)" % len(self.problems))

    def banks(self):
        return build_banks(self.concepts, self.n_ids, self.cats_list, self.edges, self.codec, self.id_of)

    def binary(self, align=ALIGN):
        b = pack_bin(self.banks(), lang_byte(self.lang), self.n_ids, self.codec, align)
        b2 = pack_bin(self.banks(), lang_byte(self.lang), self.n_ids, self.codec, align)
        if b != b2:
            raise Fail("the binary is not byte-identical between two builds")
        return b


def stats(b, blob):
    cs = b.concepts
    gen = sum(1 for c in cs if c.generated)
    saf = sum(1 for c in cs if c.cat.root == "safety")
    surf = sum(len(surfaces(c)) for c in cs)
    syn = sum(len(c.syn) for c in cs)
    ne = sum(len(v) for v in b.edges.values())
    print("talkc: %d concepts (%d generated, %d authored of which %d safety-only), %d categories"
          % (len(cs), gen, len(cs) - gen, saf, len(b.cats_list)))
    print("talkc: %d surface forms (%d synonyms), %d attributes, %d reviewed edges over %d concepts"
          % (surf, syn, sum(len(c.attrs) for c in cs), ne, len(b.edges)))
    roots = {}
    for c in cs:
        roots[c.cat.root] = roots.get(c.cat.root, 0) + 1
    print("talkc: by root: " + ", ".join("%s %d" % (r, roots.get(r, 0)) for r in ROOTS))
    banks = b.banks()
    tot = 0
    for name, kind, fl, data, raw in banks:
        tot += len(data)
        print("talkc:   %-9s %7d B stored, %7d B raw%s" % (name, len(data), raw, " (DC/native only)" if fl & 1 else ""))
    ps1 = sum(len(d) for n, k, f, d, r in banks if not f & 1)
    print("talkc: TALK_%s.BIN %d B (banks %d B; the PS1 loads %d B)" % (b.lang, len(blob), tot, ps1))


def gen_mode(out_root):
    rows = seed_rows()
    gen_path = os.path.join(out_root, "core", "data", "talk", "lexicon", "en", "gen_game.tsv")
    write_gen(rows, gen_path)
    b = Build("en", out_root, gen_rows=[Build.gen_to_row(r) for r in rows], write_lock_ok=True)
    b.fail_on_problems()
    lock_out = os.path.join(out_root, "core", "data", "talk", "concepts.lock")
    os.makedirs(os.path.dirname(lock_out), exist_ok=True)
    write_lock(b.ids, b.retired, lock_out)
    blob, keys = build_fixture(b.concepts, b.by_key, b.cats_list, b.edges, b.codec,
                               os.path.join(DATA, "fixture.txt"))
    fx = os.path.join(out_root, "core", "gen", "talk_fixture.c")
    os.makedirs(os.path.dirname(fx), exist_ok=True)
    with open(fx, "w", encoding="utf-8", newline="\n") as f:
        f.write(c_fixture(blob, keys, b.codec))


def selftest():
    """every core/tests/talk_bad/*.tsv is a seeded bad lexicon file whose first
    line is '# expect: TEXT': building with it added must fail with TEXT"""
    d = os.path.join(CORE, "tests", "talk_bad")
    files = sorted(f for f in os.listdir(d) if f.endswith(".tsv"))
    if not files:
        raise Fail("no seeded bad files in %s" % rel(d))
    n = 0
    for f in files:
        path = os.path.join(d, f)
        first = open(path, encoding="utf-8").readline()
        m = re.match(r"^# expect: (.*\S)", first)
        if not m:
            raise Fail("%s: the first line is # expect: TEXT" % rel(path))
        want = m.group(1)
        try:
            b = Build("en", write_lock_ok=True, extra=[path])
            got = "\n".join(b.problems) or None
        except Fail as e:
            got = str(e)
        msgs = got or ""
        if got is None or want not in msgs:
            # the problems list is printed, the exception names only the count
            raise Fail("%s: expected a failure with %r, got %r" % (rel(path), want, (got or "success")[:300]))
        n += 1
    # the edge rules (talk_assoc.py proposes, this refuses)
    b = Build("en")
    for edges, want in (({"pie": [("booze", 5)]}, "adults-only"), ({"pie": [("sweet", 3)]}, "not a talkable"),
                        ({"pie": [("sex", 3)]}, "not a talkable")):
        got = "\n".join(check_edges(edges, b.by_key))
        if want not in got:
            raise Fail("check_edges(%r): expected %r, got %r" % (edges, want, got))
        n += 1
    # the codec: round trip on every character class it has
    c = Codec(codec_path())
    for s in ("pie", "Mayor Giggs", "the purple fog", "lo-fi beats", "Dr. Bloom", "a/b"):
        if c.decode(c.encode(s)) != s:
            raise Fail("codec round trip: %r" % s)
    # morphology
    for w, p in (("pie", "pies"), ("box", "boxes"), ("pony", "ponies"), ("toy", "toys"), ("knife", "knives"),
                 ("ice cream", "ice creams"), ("bus", "buses"), ("church", "churches"), ("child", "children")):
        if plural_en(w) != p:
            raise Fail("plural_en(%r) = %r, not %r" % (w, plural_en(w), p))
    for w, a in (("owl", True), ("unicorn", False), ("hour", True), ("pie", False), ("umbrella", True)):
        if wants_an(w) != a:
            raise Fail("wants_an(%r)" % w)
    print("talkc: selftest OK (%d seeded bad inputs refused, codec and morphology)" % n)


def main(argv):
    args = argv[1:]
    out_root = None
    lang = "en"
    out = None
    align = ALIGN
    mode = "gen"
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--out-root":
            out_root = args[i + 1]; i += 2; continue
        if a == "--lang":
            lang = args[i + 1]; i += 2; continue
        if a == "--out":
            out = args[i + 1]; i += 2; continue
        if a == "--align":
            align = int(args[i + 1]); i += 2; continue
        if a in ("--bin", "--check", "--selftest", "--stats", "--seed-from-game"):
            mode = a[2:]; i += 1; continue
        print(__doc__)
        return 2
    try:
        if mode == "gen":
            gen_mode(out_root or ROOT)
        elif mode == "seed-from-game":
            write_gen(seed_rows(), os.path.join(out_root or ROOT, "core", "data", "talk", "lexicon", "en",
                                                "gen_game.tsv"))
        elif mode == "selftest":
            selftest()
        else:
            b = Build(lang)
            b.fail_on_problems()
            blob = b.binary(align)
            if mode == "bin":
                out = out or os.path.join(CORE, "build", "talk", "TALK_%s.BIN" % lang)
                os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
                with open(out, "wb") as f:
                    f.write(blob)
                print("talkc: %s %d B, %d concepts, dict_hash 0x%08X" % (rel(os.path.abspath(out)), len(blob),
                                                                         len(b.concepts), b.codec.hash))
            elif mode == "stats":
                stats(b, blob)
            else:
                # --check: also the fixture and the generated rows are what gen would write
                rows = seed_rows()
                gen_now = os.path.join(DATA, "lexicon", "en", "gen_game.tsv")
                tmp = gen_now + ".check"
                write_gen(rows, tmp)
                same = open(tmp, "rb").read() == open(gen_now, "rb").read()
                os.remove(tmp)
                if not same:
                    raise Fail("%s is stale: run make -C core gen" % rel(gen_now))
                print("talkc: check OK (%d concepts, %d B, dict_hash 0x%08X)" % (len(b.concepts), len(blob),
                                                                                b.codec.hash))
    except Fail as e:
        print("talkc: " + str(e), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
