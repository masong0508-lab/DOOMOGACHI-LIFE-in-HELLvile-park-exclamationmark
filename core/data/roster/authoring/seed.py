#!/usr/bin/env python3
"""seed.py - the roster authoring tool (owner: roster).

This is how the 1000 premade citizens were *made*. The canonical roster is
the set of text files one directory up (hillvile_*.txt, hellvile_*.txt,
edges.txt), which tools/roster_build.py compiles into core/gen/roster_data.c.
This tool seeds those files from:

  * archetypes (cast_hill_*.py, cast_hell_*.py): ~90 per side, each a family
    of citizens with a shared job, a coherent palette and outfit vocabulary,
    personality ranges and favourite things;
  * the hand-written cast of every archetype: a unique name, pronouns and a
    one-line bio per citizen, plus hand-picked look overrides ("hand-vary")
    and relationships (households, marriages, best friends, rivals, crushes).

Looks are built with the same species-coherence rules as resident_generate()
(catfolk get cat ears and tails, owlkin beaks, cacoblobs one eye, ...) and a
deterministic RNG seeded from each citizen's name, so reruns are stable.

    python3 core/data/roster/authoring/seed.py           dry run + report
    python3 core/data/roster/authoring/seed.py --write   rewrite the roster

WARNING: --write regenerates the .txt files from scratch, so hand edits made
directly to them are lost. Once a roster ships, edit the .txt files instead.
"""
import os
import sys
sys.dont_write_bytecode = True
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
DATA = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, HERE)
import roster_build as rb  # noqa: E402

SCHEMA = rb.Schema(rb.Contract(ROOT))
E = SCHEMA.enums
PAINTS = list(SCHEMA.paints)

M64 = (1 << 64) - 1


class Rng:
    """splitmix64: tiny, stable across python versions."""

    def __init__(self, seed):
        self.s = seed & M64

    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & M64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)

    def below(self, n):
        return self.next() % n

    def chance(self, pct):
        return self.below(100) < pct

    def between(self, lo, hi):
        return lo + self.below(hi - lo + 1)

    def choice(self, seq):
        return seq[self.below(len(seq))]

    def wpick(self, wl):
        total = sum(w for _, w in wl)
        x = self.below(total)
        for v, w in wl:
            if x < w:
                return v
            x -= w
        return wl[-1][0]

    def shuffle(self, seq):
        for i in range(len(seq) - 1, 0, -1):
            j = self.below(i + 1)
            seq[i], seq[j] = seq[j], seq[i]


def fnv(s):
    h = 0xCBF29CE484222325
    for ch in s.encode("ascii"):
        h ^= ch
        h = (h * 0x100000001B3) & M64
    return h


# =============================================================================
# vocabulary parsing
# =============================================================================
def wl(spec, sep=None):
    """'A:3 B C:2' -> [(A,3),(B,1),(C,2)]; '|' separated when sep='|'."""
    if isinstance(spec, list):
        return spec
    out = []
    items = spec.split(sep) if sep else spec.split()
    for it in items:
        it = it.strip()
        if not it:
            continue
        m = re.match(r"^(.*?):(\d+)$", it)
        if m:
            out.append((m.group(1).strip(), int(m.group(2))))
        else:
            out.append((it, 1))
    return out


def rng_range(spec):
    a, b = spec.split("-") if "-" in spec[1:] else (spec, spec)
    return int(a), int(b)


# =============================================================================
# species knowledge (mirrors core/src/resident.c and catalog.c)
# =============================================================================
FUR = {
    "CATFOLK": "CARAMEL/WHITE CHOCOLATE/CARAMEL CHARCOAL/WHITE SILVER/WHITE HONEY/WHITE "
               "TANGERINE/WHITE JET/WHITE PEACH/WHITE LAVENDER/WHITE BUBBLEGUM/WHITE",
    "FOXKIN": "TANGERINE/WHITE HONEY/WHITE RUST/BONE SILVER/WHITE WHITE/SILVER "
              "BUBBLEGUM/WHITE LAVENDER/WHITE SKY/WHITE",
    "BEARKIN": "CHOCOLATE/CARAMEL CARAMEL/BONE HONEY/BONE WHITE/SILVER JET/WHITE "
               "PEACH/WHITE LAVENDER/WHITE MINT/WHITE",
    "PUPKIN": "HONEY/WHITE CARAMEL/WHITE CHOCOLATE/HONEY WHITE/CARAMEL SILVER/WHITE "
              "JET/CARAMEL PEACH/WHITE SKY/WHITE",
    "MOUSEKIN": "SILVER/WHITE WHITE/BUBBLEGUM CARAMEL/WHITE CHARCOAL/SILVER PEACH/WHITE "
                "LAVENDER/WHITE",
    "SHEEPKIN": "WHITE/BONE BONE/CHOCOLATE CHARCOAL/WHITE LAVENDER/WHITE BUBBLEGUM/WHITE "
                "LEMON/WHITE",
    "RACCOONKIN": "SILVER/CHARCOAL CHARCOAL/WHITE CARAMEL/CHOCOLATE LAVENDER/GRAPE",
    "OWLKIN": "CARAMEL/BONE CHOCOLATE/HONEY WHITE/SILVER SILVER/WHITE HONEY/WHITE "
              "LAVENDER/SKY",
    "BUNNYKIN": "WHITE/BUBBLEGUM CARAMEL/WHITE CHOCOLATE/WHITE SILVER/WHITE PEACH/WHITE "
                "BUBBLEGUM/WHITE LAVENDER/WHITE HONEY/WHITE",
}
FURRY = set(FUR)            # species with a fur pair
SKINS = {
    "IMP": "CHERRY ROSE MAGENTA TANGERINE LAVENDER BUBBLEGUM BLOOD HELLFIRE GRAPE",
    "GHOUL": "MINT ECTOPLASM LIME TEAL SILVER LAVENDER",
    "SKELETON": "BONE WHITE SILVER",
    "SLIME": "SLIME MINT BUBBLEGUM CYAN LAVENDER LEMON MAGENTA",
    "HELLHOUND": "CHARCOAL JET RUST BLOOD CHOCOLATE",
    "CACOBLOB": "CHERRY MAGENTA ROSE GRAPE BLOOD TANGERINE",
    "BATLING": "GRAPE CHARCOAL LAVENDER VAPE_PURPLE JET",
    "ROBOT": "SILVER COBALT SKY MINT LEMON WHITE CHARCOAL",
    "PLUSHIE": "BUBBLEGUM LAVENDER SKY MINT PEACH LEMON CARAMEL",
    "GHOST": "WHITE ECTOPLASM LAVENDER SKY MINT",
    "MUMMY": "BONE WHITE CARAMEL",
    "MOTHKIN": "LAVENDER WHITE BONE PEACH SKY LEMON",
    "FROGLING": "LIME MINT CHARTREUSE TEAL FOREST SKY",
    "HUMAN": "SKIN_PORCELAIN SKIN_FAIR SKIN_PEACH SKIN_TAN SKIN_OLIVE SKIN_BROWN SKIN_UMBER SKIN_EBONY",
    "CHERUB": "SKIN_PORCELAIN SKIN_FAIR SKIN_PEACH SKIN_TAN SKIN_OLIVE SKIN_BROWN",
}
SPECIES_INFO = {   # hair, horn, tail, wing chance, base corruption (catalog.c)
    "HUMAN": (95, 3, 2, 2, 5), "IMP": (60, 95, 90, 30, 40), "CHERUB": (90, 0, 0, 95, 0),
    "GHOUL": (70, 10, 5, 5, 35), "SKELETON": (10, 5, 5, 5, 30), "SLIME": (20, 5, 5, 5, 20),
    "HELLHOUND": (50, 40, 95, 5, 45), "CACOBLOB": (5, 60, 10, 10, 50),
    "BATLING": (60, 10, 10, 95, 30), "ROBOT": (10, 60, 5, 5, 10), "PLUSHIE": (50, 5, 30, 10, 0),
    "GHOST": (30, 2, 0, 5, 20), "MUMMY": (30, 5, 5, 5, 25), "MOTHKIN": (60, 90, 0, 95, 10),
    "FROGLING": (30, 5, 5, 5, 5), "BUNNYKIN": (70, 2, 100, 2, 3), "CATFOLK": (70, 2, 100, 2, 8),
    "FOXKIN": (70, 2, 100, 2, 10), "BEARKIN": (50, 2, 100, 2, 5), "PUPKIN": (60, 2, 100, 2, 3),
    "MOUSEKIN": (60, 2, 100, 2, 5), "SHEEPKIN": (85, 2, 100, 2, 2), "RACCOONKIN": (60, 2, 100, 2, 15),
    "OWLKIN": (30, 2, 100, 40, 5),
}
EARS = {"CATFOLK": "CAT", "FOXKIN": "FOX", "BEARKIN": "BEAR", "MOUSEKIN": "MOUSE",
        "SHEEPKIN": "SHEEP", "RACCOONKIN": "RACCOON", "OWLKIN": "OWL_TUFTS",
        "BUNNYKIN": "BUNNY", "IMP": "POINTY", "HELLHOUND": "DOG", "BATLING": "BAT",
        "SKELETON": "NONE", "SLIME": "NONE", "CACOBLOB": "NONE", "ROBOT": "NONE",
        "GHOST": "NONE", "MOTHKIN": "NONE"}
TAILS = {"CATFOLK": "CAT", "FOXKIN": "FOX", "BEARKIN": "BEAR_NUB", "PUPKIN": "PUP_WAGGY",
         "MOUSEKIN": "MOUSE", "SHEEPKIN": "SHEEP_PUFF", "RACCOONKIN": "RACCOON_RINGED",
         "OWLKIN": "FEATHER_FAN", "BUNNYKIN": "BUNNY"}
NOSES = {"CATFOLK": "CAT", "FOXKIN": "CAT", "MOUSEKIN": "CAT", "BEARKIN": "BUTTON",
         "PUPKIN": "BUTTON", "SHEEPKIN": "BUTTON", "BUNNYKIN": "BUTTON", "RACCOONKIN": "BUTTON",
         "HELLHOUND": "BUTTON", "OWLKIN": "NONE", "SLIME": "NONE", "GHOST": "NONE",
         "CACOBLOB": "NONE", "FROGLING": "NONE", "ROBOT": "NONE", "SKELETON": "SKULL"}
CRITTER = rb.CRITTERS

# =============================================================================
# side defaults (every archetype key falls back to these)
# =============================================================================
DEFAULTS = {
    "hill": dict(
        sp="CATFOLK:12 BUNNYKIN:10 PUPKIN:10 FOXKIN:9 BEARKIN:9 MOUSEKIN:8 SHEEPKIN:7 "
           "RACCOONKIN:6 OWLKIN:6 PLUSHIE:6 HUMAN:5 CHERUB:4 FROGLING:4 MOTHKIN:3",
        body="AVERAGE:8 ROUND:8 TINY:6 PEAR:4 STOCKY:2 LANKY:2 TOP_HEAVY:1",
        height="3-12", build="5-12", head_size="8-14",
        head="ROUND:12 MOCHI:8 OVAL:5 BEAN:4 HEART:3 PEAR:2 STAR:1",
        eyes="ROUND:14 ANIME:12 DOT:10 HAPPY_ARC:8 SPARKLE:8 LASHES:8 WIDE:6 SLEEPY:4 "
             "STAR:3 HEART:3 DROOPY:4 CAT:3",
        eye_col="HONEY:3 LEMON:2 SKY:3 MINT:2 CHOCOLATE:3 JET:2 COBALT:2 FOREST:2 LAVENDER:2 ROSE:2 "
                "TEAL:2 CARAMEL:2 BUBBLEGUM PEACH NAVY GRAPE CHARTREUSE TANGERINE CYAN LIME",
        brows="NONE:10 THIN:10 DOTS:6 RAISED:4 WORRIED:3 ARCHED:3 THICK:2",
        nose="DOT:8 BUTTON:8 ROUND:5 POINTY:3 NONE:2 WIDE:2",
        mouth="SMILE:14 OPEN:6 GRIN:5 O:4 BIG_GRIN:4 POUT:3 TONGUE:3 WAVY:2 LIPSTICK:2",
        fangs="0:14 1:1",
        hair="SHORT:10 MESSY:8 BOB:8 CURLY:6 PIGTAILS:5 TWINTAILS:5 BUN:5 PONYTAIL:6 ODANGO:3 "
             "WAVY:5 HIME:3 LONG:6 BOWL:3 AFRO:3 BRAIDS:3 CROWN_BRAID:2 POMPADOUR:2 BUZZ:2 LOCS:3",
        hair_col="CHOCOLATE:3 CARAMEL:3 HONEY:3 JET:2 LEMON:2 RUST:2 SILVER WHITE CHARCOAL "
                 "BUBBLEGUM:2 LAVENDER:2 SKY:2 MINT:2 PEACH:2 ROSE:2 COBALT NAVY TEAL FUR:6 FUR2:2",
        hl="NONE:78 BUBBLEGUM:3 LAVENDER:3 SKY:3 MINT:3 PEACH:3 LEMON:3 ROSE:2 WHITE:2",
        fhair="NONE:96 STUBBLE MUSTACHE GOATEE FULL_BEARD",
        horns="NONE", wings="NONE", halo="NONE",
        mark="FUR:75 NONE:15 CHEEK_HEARTS:4 STAR_FRECKLES:4 SPOTS:2 STRIPES:2",
        cheeks="BLUSH:75 FRECKLES:15 DIMPLES:15 SPARKLES:8 BEAUTY_MARK:4",
        top="TSHIRT:10 HOODIE:8 SWEATER:8 DRESS:7 OVERALLS:6 SWEATER_VEST:4 PAJAMAS:2 "
            "KIMONO:2 CROP_TOP:3 TANK:2",
        bottom="SHORTS:8 SKIRT:8 PANTS:8 LEGGINGS:4 LONG_SKIRT:3 TUTU:1 SWEATS:2 CARGO:2",
        shoes="BARE:6 SNEAKERS:10 SLIPPERS:5 BOOTS:5 SANDALS:4 CLOGS:2",
        hat="NONE:70 BERET:5 BEANIE:5 FLOWER_CROWN:5 CAP:4 BUCKET:3 HEADPHONES:2 PARTY:1",
        acc="NONE:68 BOWTIE:6 SCARF:6 GLASSES:6 BACKPACK:4 EARRINGS:4 TEDDY:2",
        pat="SOLID:70 POLKA:8 STRIPES:8 HEARTS:7 PLAID:5",
        aura="NONE:92 SPARKLES:3 HEARTS:3 BUBBLES:2",
        scheme="BUBBLEGUM/MINT/LEMON LAVENDER/PEACH/SKY SKY/LEMON/WHITE MINT/ROSE/WHITE "
               "PEACH/TEAL/HONEY ROSE/LAVENDER/WHITE LEMON/SKY/BUBBLEGUM",
        pers="0-15 0-9 5-15 6-15 0-9",
        food="Cupcakes|Onigiri|Pudding|Angel Cake|Salad|Milk|Coffee|Pancakes|Waffles|Ramen|"
             "Mochi|Dumplings|Pizza|Tacos|Sushi|Curry|Donuts|Macarons|Ice Cream|Bubble Tea|"
             "Honey Toast|Melon Bread|Takoyaki|Omurice|Cotton Candy|Carrot Cake|Popcorn|"
             "Fried Chicken|Spaghetti|Cereal|Gumdrops|Pickles",
        hate="Hell Peppers|Brussels Sprouts|Pickles|Black Licorice|Burnt Toast|Sulfur Soda|"
             "Eyeball Jelly|Worm Gummies|Grave Dirt Cake|Bone Broth|Coffee|Curry|Sushi",
        hobby="Baking|Knitting|Gardening|Stargazing|Skateboarding|Video Games|Painting|Singing|"
              "Dancing|Reading|Fishing|Napping|Cosplay|Karaoke|Board Games|Bug Collecting|"
              "Birdwatching|Yoga|Pottery|Origami|Plush Making|Cloud Watching|Bubble Blowing|Poetry",
        music="Bubblegum Pop|Lo-fi Beats|City Pop|Chiptune|Lullabies|Jazz|Classical|Folk|Disco|"
              "Sea Shanties|Hymns|Synthwave",
        fear="Spiders|The Dark|Thunder|Clowns|Ghosts|Heights|Deep Water|Vacuum Cleaners|Bees|"
             "Needles|Mirrors|Being Alone|Public Speaking|Bath Time|Dentists|Popping Balloons|Silence",
        phrase=None,   # side pool chosen in code
        act="HOME:4 PARK:2 CAFE:1 SHOP:1 WANDERING:1",
        romance="ANY:72 SHE:8 HE:8 THEY:6 NONE:6",
    ),
    "hell": dict(
        sp="IMP:12 GHOUL:9 SKELETON:9 HELLHOUND:9 BATLING:7 CACOBLOB:6 RACCOONKIN:6 SLIME:5 "
           "MUMMY:5 GHOST:5 CATFOLK:5 FOXKIN:5",
        body="AVERAGE:6 LANKY:6 HUNCHED:4 STOCKY:4 BUFF:3 NOODLE:3 TOP_HEAVY:3 ROUND:3 TINY:3 PEAR:2",
        height="3-13", build="3-12", head_size="8-14",
        head="ROUND:6 OVAL:6 TRIANGLE:4 SQUARE:3 DIAMOND:3 BEAN:3 MOCHI:3 PEAR:2",
        eyes="ANGRY:10 SLEEPY:8 GOAT:7 DEMON_SLIT:8 SQUINT:7 GLOW:6 ROUND:6 ANIME:5 CAT:5 "
             "LASHES:4 DROOPY:4 HOLLOW:2 VOID:2 X:1 SPIRAL:1 CROSS:1",
        eye_col="CHERRY:3 HELLFIRE:3 LEMON:3 SLIME:2 VAPE_PURPLE:2 MAGENTA:2 ECTOPLASM:2 BLOOD:2 JET:2 "
                "RUST BONE SILVER WHITE CHARCOAL GRAPE TANGERINE",
        brows="ANGRY:10 THICK:6 ARCHED:6 NONE:5 SCARRED:4 UNIBROW:3 FLAT:4 BUSHY:3",
        nose="DOT:6 POINTY:6 HOOK:4 BUTTON:4 NONE:4 LONG:2 WIDE:2 PIG:2 ROUND:2",
        mouth="SMIRK:10 TEETH:6 SHARK:6 FANGY:8 GRIN:6 FLAT:6 FROWN:4 TONGUE:4 STITCHED:3 "
              "DROOL:2 LIPSTICK:3 SMILE:4",
        fangs="0:3 1:3 2:3 3:2",
        hair="EMO_FRINGE:10 MOHAWK:7 SPIKY:8 SIDE_SHAVE:7 LIBERTY_SPIKES:4 MULLET:5 WILD:6 "
             "SLICKED:6 LONG:6 MESSY:5 BUZZ:3 HIME:3 DRILLS:2 FLAME:2 TWINTAILS:3 LOCS:3",
        hair_col="JET:4 CHARCOAL:3 BLOOD:2 GRAPE:2 VAPE_PURPLE:2 MAGENTA:2 HELLFIRE:2 SLIME "
                 "WHITE CHERRY:2 FUR:4 FUR2",
        hl="NONE:72 HELLFIRE:3 MAGENTA:3 SLIME:3 VAPE_PURPLE:3 CHERRY:3 WHITE:2 ECTOPLASM:2",
        fhair="NONE:88 STUBBLE:3 GOATEE:3 MUSTACHE:2 CHOPS:2 FULL_BEARD:1 HANDLEBAR:1",
        horns="NONE", wings="NONE", halo="NONE",
        mark="FUR:45 NONE:30 SKULL_PAINT:4 TRIBAL:3 STITCHES:3 CRACKS:3 EYELINER:6 "
             "PENTAGRAM:2 TEARS:2 SCALES:2",
        cheeks="BLUSH:30 SCAR:15 BANDAGE:10 TEAR_TATTOO:10 BEAUTY_MARK:8",
        top="LEATHER_JACKET:10 BAND_TEE:9 HOODIE:8 CLOAK:6 ROBE:5 SUIT:6 ARMOR:2 TANK:4 "
            "CROP_TOP:4 DRESS:4 TSHIRT:3",
        bottom="RIPPED_JEANS:10 CARGO:6 PANTS:6 LEGGINGS:5 KILT:3 SKIRT:5 LONG_SKIRT:3 SWEATS:2",
        shoes="COMBAT:10 BOOTS:8 PLATFORM:6 SNEAKERS:4 BARE:3 HEELS:2",
        hat="NONE:74 SKULL_CAP:5 BEANIE:5 WITCH:3 TOP_HAT:3 BANDANA:4 HEADPHONES:3 CAP:3",
        acc="NONE:62 CHAINS:6 SKULL_NECKLACE:6 SUNGLASSES:5 EYEPATCH:3 NOSE_RING:4 MASK:2 "
            "EARRINGS:4 SCARF:2",
        pat="SOLID:68 SKULLS:9 FLAMES:8 STRIPES:6 CAMO:4 PLAID:5",
        aura="NONE:88 FLAMES:3 SMOKE:4 FLIES:3 GLITCH:2",
        scheme="JET/CHERRY/SILVER CHARCOAL/VAPE_PURPLE/BONE BLOOD/JET/HELLFIRE GRAPE/SLIME/JET "
               "NAVY/MAGENTA/SILVER RUST/CHARCOAL/HONEY JET/ECTOPLASM/GRAPE "
               "CHARCOAL/HELLFIRE/BONE VAPE_PURPLE/JET/LEMON FOREST/BLOOD/BONE "
               "JET/BUBBLEGUM/SILVER MAGENTA/JET/CYAN",
        pers="0-15 6-15 0-15 0-12 4-15",
        food="Soul Soup|Brimstone Bagels|Candy Eyeballs|Hell Peppers|Meat Slabs|Coffee|"
             "Gumdrops|Lava Cake|Bat Wing Chips|Worm Gummies|Black Licorice|Burnt Toast|"
             "Sulfur Soda|Eyeball Jelly|Grave Dirt Cake|Blood Oranges|Bone Broth|Pizza|Tacos|"
             "Ramen|Fried Chicken|Popcorn|Pickles|Curry|Spaghetti",
        hate="Salad|Angel Cake|Milk|Cupcakes|Macarons|Cotton Candy|Pudding|Brussels Sprouts|"
             "Carrot Cake|Honey Toast|Mochi|Cereal|Bubble Tea",
        hobby="Cosplay|Karaoke|Board Games|Bug Collecting|Poetry|Tarot Reading|Seances|"
              "Ghost Hunting|Metal Drumming|Pranks|Graffiti|Welding|Skull Collecting|"
              "Video Games|Skateboarding|Napping",
        music="Sea Shanties|Hymns|Synthwave|Goth Rock|Hardcore Punk|Doom Metal|Industrial|Chiptune",
        fear="Public Speaking|Bath Time|Dentists|Popping Balloons|Silence|Holy Water|Sunlight|"
             "Church Bells|Exorcisms|Hugs|Cute Things|Tax Season|Spiders|Clowns",
        phrase=None,
        act="HOME:5 PARK:1 CAFE:1 SHOP:1 WANDERING:2",
        romance="ANY:70 SHE:8 HE:8 THEY:6 NONE:8",
    ),
}
SWEET_PHRASES = list(range(0, 36))
SASSY_PHRASES = list(range(36, 44))
SPOOKY_PHRASES = list(range(44, 64))

ARCH_KEYS = set(DEFAULTS["hill"]) | {
    "fur", "skin", "eye_count", "horn_col", "horn_size", "tail", "mark_col", "pet",
    "traits", "stats", "voice", "corr", "eye_tilt", "nose_size", "hair_force", "hat_force",
}

# record keys a cast line may override directly
RECORD_KEYS = set(SCHEMA.all_keys())
REL_OPTS = {"fam", "wed", "date", "bff", "friend", "rival", "nem", "ex", "crush", "sib"}
OTHER_OPTS = {"id", "job", "trait", "scheme", "fur", "skin_pool", "flag", "bio_pronoun_ok"}
OPT_ALIAS = {"acc": "accessory", "mark": "marking", "fhair": "facial_hair", "pat": "pattern",
             "hl": "highlight", "acc_color": "accessory_color", "mark_color": "marking_color"}


# =============================================================================
# generation
# =============================================================================
def vocab(arch, key):
    v = arch.get(key)
    if v is None:
        v = DEFAULTS[arch["side"]].get(key)
    return v


def pick(rng, arch, key, sep=None):
    v = vocab(arch, key)
    if v is None:
        return None
    return rng.wpick(wl(v, sep))


def is_crit(sp):
    return sp in CRITTER


def make_citizen(cl, idx_in_arch):
    """Build the record dict (values as names / ints) for one cast line."""
    arch = cl.arch
    side = arch["side"]
    hell = side == "hell"
    rng = Rng(fnv(cl.name) ^ fnv(arch["key"]))
    o = {OPT_ALIAS.get(k, k): val for k, val in cl.opts.items()}
    v = {}

    sp = o.get("species") or pick(rng, arch, "sp")
    v["species"] = sp
    info = SPECIES_INFO[sp]

    # ---- traits -------------------------------------------------------------
    traits = set()
    srcs = (arch.get("traits"), o.get("trait"))
    if o.get("trait") == "NONE":
        srcs = ()
    for src in srcs:
        if src:
            traits.update(t.strip() for t in src.replace(",", " ").split() if t.strip())
    kid = "KID" in traits
    elder = "ELDER" in traits

    # ---- fur / skin -----------------------------------------------------------
    fur2 = None
    if sp in FURRY:
        pairs = o.get("fur") or arch.get("fur_" + sp) or (arch.get("fur") if arch.get("fur") else None)
        pool = wl(pairs) if pairs else wl(FUR[sp])
        a, b = rng.wpick(pool).split("/")
        v["skin"] = a
        fur2 = b
    else:
        skins = o.get("skin_pool") or arch.get("skin_" + sp)
        if not skins and arch.get("skin"):
            natural = all(s.startswith("SKIN_") for s, _ in wl(arch["skin"]))
            if natural == (sp in ("HUMAN", "CHERUB")):
                skins = arch["skin"]
        if not skins:
            skins = SKINS.get(sp, SKINS["HUMAN"])
        v["skin"] = rng.wpick(wl(skins))
    sch = o.get("scheme") or pick(rng, arch, "scheme")
    s_main, s_acc, s_ext = sch.split("/")
    if fur2 is None:
        fur2 = s_ext
    furmain = v["skin"] if not v["skin"].startswith("SKIN_") else "CARAMEL"

    # ---- body ------------------------------------------------------------------
    v["body"] = pick(rng, arch, "body")
    lo, hi = rng_range(vocab(arch, "height"))
    if kid:
        lo, hi = 0, 4
    v["height"] = rng.between(lo, hi)
    lo, hi = rng_range(vocab(arch, "build"))
    v["build"] = rng.between(lo, hi)
    if v["body"] in ("NOODLE", "LANKY") or sp in ("SKELETON",):
        v["build"] = rng.between(0, 5)
    elif v["body"] in ("ROUND", "TOP_HEAVY", "PEAR", "BUFF"):
        v["build"] = min(15, v["build"] + rng.between(1, 4))
    if v["body"] == "LANKY" and not kid:
        v["height"] = min(15, v["height"] + rng.between(1, 3))
    if sp == "SKELETON":
        v["head"] = "SKULL"
    elif sp == "CACOBLOB":
        v["head"] = "BLOB"
    elif sp == "ROBOT":
        v["head"] = "SQUARE" if rng.chance(65) else "DIAMOND"
    elif sp == "SLIME":
        v["head"] = "BLOB" if rng.chance(60) else "MOCHI"
    else:
        v["head"] = pick(rng, arch, "head")
    lo, hi = rng_range(vocab(arch, "head_size"))
    if kid:
        lo, hi = max(lo, 11), 15
    v["head_size"] = rng.between(lo, hi)

    # ---- eyes ------------------------------------------------------------------
    eyes = None
    if "eyes" not in arch:
        if sp == "PLUSHIE" and rng.chance(70):
            eyes = "BUTTON"
        elif sp == "MOTHKIN" and rng.chance(50):
            eyes = "COMPOUND"
        elif sp == "CATFOLK" and rng.chance(35):
            eyes = "CAT"
        elif sp == "SKELETON" and rng.chance(60):
            eyes = "HOLLOW"
        elif sp == "GHOST" and rng.chance(40):
            eyes = "VOID"
    v["eyes"] = eyes or pick(rng, arch, "eyes")
    v["eye_color"] = pick(rng, arch, "eye_col")
    v["eye_size"] = rng.between(11, 15) if kid else rng.between(7, 13)
    v["eye_spacing"] = rng.between(5, 10)
    v["eye_height"] = rng.between(6, 10)
    lo, hi = rng_range(arch.get("eye_tilt") or ("5-11" if hell else "7-9"))
    v["eye_tilt"] = rng.between(lo, hi)
    if sp == "CACOBLOB":
        v["eye_count"] = 1
    elif arch.get("eye_count"):
        v["eye_count"] = int(rng.wpick(wl(arch["eye_count"])))
    else:
        v["eye_count"] = 3 if rng.chance(4 if hell else 1) else (1 if rng.chance(2) else 2)
    v["brows"] = pick(rng, arch, "brows")
    v["brow_height"] = rng.between(5, 10)
    v["nose"] = NOSES.get(sp) or ("BUTTON" if sp == "PLUSHIE" and rng.chance(50) else None) \
        or pick(rng, arch, "nose")
    lo, hi = rng_range(arch.get("nose_size") or "4-10")
    v["nose_size"] = rng.between(lo, hi)
    mouth = None
    if sp == "OWLKIN":
        mouth = "BEAK"
    elif "mouth" not in arch:
        if sp in ("CATFOLK", "FOXKIN", "BUNNYKIN", "MOUSEKIN") and rng.chance(50):
            mouth = "CAT"
        elif sp == "FROGLING" and rng.chance(50):
            mouth = "WAVY"
        elif sp == "PLUSHIE" and rng.chance(40):
            mouth = "STITCHED"
    v["mouth"] = mouth or pick(rng, arch, "mouth")
    v["mouth_size"] = rng.between(4, 10)
    v["fangs"] = int(pick(rng, arch, "fangs"))

    # ---- hair ------------------------------------------------------------------
    if arch.get("hair_force") or rng.chance(info[0]) or (arch.get("hair") and rng.chance(50)):
        if sp == "SLIME" and rng.chance(50):
            v["hair"] = "SLIME_DRIP"
        elif sp == "SHEEPKIN" and rng.chance(55):
            v["hair"] = "CURLY" if rng.chance(60) else "AFRO"
        else:
            v["hair"] = pick(rng, arch, "hair")
    else:
        v["hair"] = "BALD"
    hc = pick(rng, arch, "hair_col")
    if hc == "FUR":
        hc = furmain
    elif hc == "FUR2":
        hc = fur2
    v["hair_color"] = hc
    hl = pick(rng, arch, "hl")
    v["highlight"] = "NONE" if hl == v["hair_color"] else hl
    fh = pick(rng, arch, "fhair")
    if is_crit(sp) and fh != "NONE" and "fhair" not in arch and rng.chance(70):
        fh = "NONE"
    if kid or (cl.pronoun == "SHE" and "fhair" not in arch):
        fh = "NONE"
    v["facial_hair"] = fh

    # ---- ears, horns, tail, wings, halo ----------------------------------------
    if sp == "PUPKIN":
        v["ears"] = "FLOPPY_PUP" if rng.chance(70) else "DOG"
    elif sp == "GHOUL":
        v["ears"] = "ELF" if rng.chance(50) else "POINTY"
    elif sp == "FROGLING":
        v["ears"] = "FIN" if rng.chance(30) else "NONE"
    elif sp == "PLUSHIE":
        v["ears"] = rng.choice(["BEAR", "BUNNY", "CAT", "MOUSE"])
    elif sp in EARS:
        v["ears"] = EARS[sp]
    else:
        v["ears"] = "ELF" if rng.chance(5) else "NORMAL"
    horns = "NONE"
    if arch.get("horns") and arch["horns"] != "NONE":
        horns = pick(rng, arch, "horns")
    elif rng.chance(info[1]):
        if sp == "IMP":
            horns = rng.choice(["NUBS", "DEVIL", "RAM", "GOAT", "DEVIL_TALL", "NUBS"])
        elif sp == "HELLHOUND":
            horns = rng.choice(["BULL", "DEVIL", "BROKEN"])
        elif sp == "CACOBLOB":
            horns = rng.choice(["DEVIL", "MANY", "SPIRAL", "CROWN_SPIKES"])
        elif sp in ("ROBOT", "MOTHKIN"):
            horns = "ANTENNAE"
        else:
            horns = rng.choice(["DEVIL", "RAM", "ONI", "GOAT", "BROKEN"] if hell
                               else ["UNICORN", "ANTLERS", "NUBS", "SPIRAL"])
    v["horns"] = horns
    hornless = horns == "NONE"
    v["horn_color"] = pick(rng, arch, "horn_col") if arch.get("horn_col") else \
        rng.choice(["BONE", "JET", "CHERRY", "CHARCOAL", "BLOOD", "GRAPE"] if hell else
                   ["BONE", "HONEY", "LAVENDER", "BUBBLEGUM", "WHITE", "LEMON"])
    lo, hi = rng_range(arch.get("horn_size") or "4-12")
    v["horn_size"] = rng.between(lo, hi)
    if not hornless and not arch.get("horn_col"):
        v["horn_color"] = rng.choice(
            ["BONE", "BONE", "JET", "CHERRY", "CHARCOAL", "BLOOD", "GRAPE", "HELLFIRE", "RUST",
             "CHOCOLATE", "SILVER"] if hell else
            ["BONE", "BONE", "HONEY", "LAVENDER", "BUBBLEGUM", "WHITE", "LEMON", "MINT", "SKY",
             "PEACH", "ROSE", "CARAMEL"])
    if sp in TAILS:
        v["tail"] = TAILS[sp]
    elif arch.get("tail"):
        v["tail"] = pick(rng, arch, "tail")
    elif rng.chance(info[2]):
        if sp == "IMP":
            v["tail"] = "DEVIL"
        elif sp == "HELLHOUND":
            v["tail"] = ("FLAME" if rng.chance(50) else "SPIKED") if hell else "PUP_WAGGY"
        elif sp == "SKELETON":
            v["tail"] = "SKELETAL"
        elif sp == "PLUSHIE":
            v["tail"] = "BUNNY" if rng.chance(50) else "BEAR_NUB"
        else:
            v["tail"] = rng.choice(["DEVIL", "LIZARD", "CAT", "TENTACLE"])
    else:
        v["tail"] = "NONE"
    wings = "NONE"
    if arch.get("wings") and arch["wings"] != "NONE":
        wings = pick(rng, arch, "wings")
    elif rng.chance(info[3]):
        if sp in ("CHERUB", "OWLKIN"):
            wings = "ANGEL"
        elif sp == "BATLING":
            wings = "TINY_BAT" if rng.chance(60) else "BAT"
        elif sp == "MOTHKIN":
            wings = "BUTTERFLY"
        elif sp == "IMP":
            wings = "TINY_BAT" if rng.chance(70) else "BAT"
        else:
            wings = rng.choice(["BAT", "BONE", "DRAGON", "FLY", "TINY_BAT"] if hell
                               else ["ANGEL", "BUTTERFLY", "TINY_BAT"])
    v["wings"] = wings
    halo = "NONE"
    if arch.get("halo") and arch["halo"] != "NONE":
        halo = pick(rng, arch, "halo")
    elif sp == "CHERUB":
        halo = "HALO" if rng.chance(80) else "STARS"
    elif hell and rng.chance(3):
        halo = rng.choice(["BROKEN", "FLAME_RING", "THORNS"])
    v["halo"] = halo

    # ---- markings & cheeks -------------------------------------------------------
    mk = pick(rng, arch, "mark")
    if mk == "FUR":
        if is_crit(sp) and sp in FURRY:
            if sp == "RACCOONKIN":
                mk = "FUR_MASK"
            elif sp == "CATFOLK":
                mk = rng.choice(["FUR_TABBY", "FUR_CALICO", "FUR_MUZZLE", "FUR_SOCKS", "FUR_BELLY"])
            elif sp == "FOXKIN":
                mk = rng.choice(["FUR_TIPPED_EARS", "FUR_MUZZLE", "FUR_BELLY", "FUR_SOCKS"])
            else:
                mk = rng.choice(["FUR_MUZZLE", "FUR_BELLY", "FUR_SOCKS", "FUR_TIPPED_EARS"])
            mcol = fur2
        elif sp == "PLUSHIE":
            mk = "STITCHES" if rng.chance(50) else "PATCHES"
            mcol = s_acc
        elif sp == "FROGLING":
            mk = "SPOTS" if rng.chance(60) else "NONE"
            mcol = rng.choice(["FOREST", "TEAL", "LEMON", "CHARTREUSE"])
        else:
            mk = "NONE"
            mcol = s_acc
    else:
        mcol = None
    v["marking"] = mk
    if mcol is None:
        mc = arch.get("mark_col")
        mcol = rng.wpick(wl(mc)) if mc else s_acc
        if mcol == "FUR2":
            mcol = fur2
    v["marking_color"] = mcol
    ch = []
    for nm, pct in wl(vocab(arch, "cheeks")):
        if rng.chance(pct):
            ch.append(nm)
    v["cheeks"] = ch

    # ---- outfit ----------------------------------------------------------------
    def colour(base, pct=88):
        if rng.chance(pct):
            return base
        return rng.choice([s_main, s_acc, s_ext])
    v["top"] = pick(rng, arch, "top")
    v["top_color"] = colour(s_main, 92)
    v["pattern"] = pick(rng, arch, "pat")
    v["pattern_color"] = s_ext if s_ext != v["top_color"] else s_acc
    one_piece = v["top"] in ("DRESS", "ROBE", "KIMONO", "PAJAMAS", "OVERALLS", "HAZMAT",
                             "SPACESUIT", "PRIEST", "NURSE", "MAID", "PLATE_ARMOR")
    if one_piece and "bottom" not in arch and rng.chance(60):
        v["bottom"] = "NONE"
    else:
        v["bottom"] = pick(rng, arch, "bottom")
    v["bottom_color"] = colour(s_acc, 85)
    if sp in ("GHOST", "SLIME") and "shoes" not in arch:
        v["shoes"] = "BARE"
    elif sp == "IMP" and rng.chance(25) and "shoes" not in arch:
        v["shoes"] = "HOOVES"
    elif sp == "HELLHOUND" and rng.chance(35) and "shoes" not in arch:
        v["shoes"] = "CLAWS"
    elif is_crit(sp) and rng.chance(30) and "shoes" not in arch:
        v["shoes"] = "BARE"
    else:
        v["shoes"] = pick(rng, arch, "shoes")
    v["shoes_color"] = colour(s_acc if rng.chance(50) else s_ext, 85)
    v["hat"] = pick(rng, arch, "hat")
    v["hat_color"] = colour(s_main if rng.chance(50) else s_ext, 85)
    v["accessory"] = "BANDAGES" if sp == "MUMMY" and rng.chance(80) else pick(rng, arch, "acc")
    v["accessory_color"] = colour(s_acc if rng.chance(50) else s_ext, 85)
    v["aura"] = pick(rng, arch, "aura")

    # ---- personality & voice ------------------------------------------------------
    pr = vocab(arch, "pers").split()
    for k, spec in zip(("movement", "speech", "expression", "attitude", "quirk"), pr):
        lo, hi = rng_range(spec)
        v[k] = rng.between(lo, hi)
    for k in ("pitch", "speed", "quality", "intonation", "accent"):
        v[k] = rng.between(0, 31)
    if sp in ("MOUSEKIN", "BUNNYKIN", "CHERUB", "BATLING", "PLUSHIE"):
        v["pitch"] = rng.between(18, 31)
    elif sp in ("BEARKIN", "HELLHOUND", "SKELETON", "MUMMY"):
        v["pitch"] = rng.between(0, 14)
    if kid:
        v["pitch"] = rng.between(22, 31)
        v["speed"] = rng.between(16, 31)
    if elder:
        v["speed"] = rng.between(0, 12)
        v["quality"] = rng.between(14, 31)
    if arch.get("voice"):
        for item in arch["voice"].split():
            k, spec = item.split("=")
            lo, hi = rng_range(spec)
            v[k] = rng.between(lo, hi)

    # ---- likes -------------------------------------------------------------------
    def tpick(key, pct, table):
        if arch.get(key) and rng.chance(pct):
            return rng.wpick(wl(arch[key], "|"))
        return rng.wpick(wl(DEFAULTS[side][key], "|"))
    v["food"] = tpick("food", 75, SCHEMA.foods)
    for _ in range(20):
        v["hates"] = tpick("hate", 60, SCHEMA.foods)
        if v["hates"] != v["food"]:
            break
    if v["hates"] == v["food"]:
        v["hates"] = "Brussels Sprouts" if v["food"] != "Brussels Sprouts" else "Pickles"
    v["hobby"] = tpick("hobby", 75, SCHEMA.hobbies)
    v["music"] = tpick("music", 70, SCHEMA.music)
    v["fear"] = tpick("fear", 55, SCHEMA.fears)
    if arch.get("phrase") and rng.chance(80):
        v["phrase"] = rng.wpick(wl(arch["phrase"], "|"))
    else:
        if hell:
            pool = SPOOKY_PHRASES if rng.chance(70) else (SASSY_PHRASES if rng.chance(60) else SWEET_PHRASES)
        else:
            pool = SWEET_PHRASES if rng.chance(88) else SASSY_PHRASES
        v["phrase"] = SCHEMA.phrases[rng.choice(pool)]
    v["color"] = v["top_color"] if rng.chance(55) else (v["hair_color"] if rng.chance(30)
                                                        else rng.choice(PAINTS))

    # ---- stats -------------------------------------------------------------------
    if hell:
        st = dict(hunger=(55, 90), happy=(40, 85), hygiene=(40, 90), energy=(45, 100),
                  health=(80, 100), sanity=(45, 90), social=(30, 85))
    else:
        st = dict(hunger=(60, 92), happy=(62, 96), hygiene=(62, 100), energy=(55, 100),
                  health=(85, 100), sanity=(72, 100), social=(45, 95))
    if kid:
        st["energy"] = (80, 100)
    if elder:
        st["energy"] = (40, 75)
    if "NIGHT_OWL" in traits:
        st["energy"] = (40, 70)
    if arch.get("stats"):
        for item in arch["stats"].split():
            k, spec = item.split("=")
            st[k] = rng_range(spec)
    for k, (lo, hi) in st.items():
        v[k] = rng.between(lo, hi)
    base = info[4]
    if hell:
        corr = rng.between(40, 78) + (base - 30) // 3
        if "SOFTIE" in traits:
            corr = rng.between(18, 38)
        if "LOYALIST" in traits:
            corr = rng.between(80, 95)
        if "VAPER" in traits:
            corr += 8
            v["sanity"] = max(40, v["sanity"] - 10)
    else:
        corr = base // 2 + rng.between(0, 8)
        if "VAPER" in traits:
            corr += 15
        if "HOLY" in traits:
            corr = rng.between(0, 3)
    if arch.get("corr"):
        lo, hi = rng_range(arch["corr"])
        corr = rng.between(lo, hi)
    v["corruption"] = max(0, min(96, corr))
    if "SOFTIE" in traits:
        v["happy"] = max(v["happy"], rng.between(60, 80))
        if "BLUSH" not in v["cheeks"] and rng.chance(60):
            v["cheeks"] = v["cheeks"] + ["BLUSH"]   # a subtle tell: softies blush

    # ---- identity ------------------------------------------------------------------
    v["pronoun"] = cl.pronoun
    v["romance"] = pick(rng, arch, "romance")
    if kid:
        v["romance"] = "NONE"
    v["birthday"] = "%s-%02d" % (rb.MONTHS[rng.below(12)], rng.between(1, 28))
    flags = []
    if hell:
        flags.append("HELLBORN")
    if arch.get("pet") and rng.chance(int(arch["pet"])):
        flags.append("HAS_PET")
    v["flags"] = flags
    v["traits"] = sorted(traits, key=rb.TRAIT_NAMES.index)
    v["activity"] = pick(rng, arch, "act")
    v["job"] = arch["jobs"][0]          # the archetype's main job unless job= says otherwise
    v["bio"] = cl.bio

    # ---- hand-varied overrides ------------------------------------------------------
    for k, val in o.items():
        if k in RECORD_KEYS:
            if k in ("cheeks", "flags"):
                val = [] if val == "NONE" else val.split(",")
            elif k == "traits":
                val = [] if val == "NONE" else val.split(",")
            elif SCHEMA.fields[k][0] == "int":
                val = int(val)
            v[k] = val
        elif k == "job":
            v["job"] = val
        elif k == "flag":
            v["flags"] = sorted(set(v["flags"]) | set(val.split(",")))
        elif k not in REL_OPTS and k not in OTHER_OPTS:
            raise ValueError("%s: unknown option %r" % (cl.src, k))
    if "story" in v and "STORY_NPC" not in v["flags"]:
        v["flags"] = v["flags"] + ["STORY_NPC"]
    v["mood"] = "HAPPY" if v["happy"] >= 75 else "OK"
    if v["horns"] == "NONE" and "horn_color" not in o:
        v["horn_color"] = v["hair_color"]
    if v["highlight"] == v["hair_color"]:
        v["highlight"] = "NONE"
    return v


# =============================================================================
# relationships
# =============================================================================
BANDS = {"ACQUAINTANCE": (5, 22), "FRIEND": (28, 66), "BEST_FRIEND": (72, 98),
         "DATING": (55, 95), "MARRIED": (70, 100), "RIVAL": (-62, -26),
         "NEMESIS": (-100, -72), "FAMILY": (45, 95), "EX": (-35, 15)}


def accepts(who, other):
    r, p = who["romance"], other["pronoun"]
    if r == "ANY":
        return True
    if r == "NONE":
        return False
    if r in ("SHE", "HE"):
        return p == r
    return p in ("THEY", "XE", "IT")


class Graph:
    def __init__(self, cits, rng):
        self.c = cits
        self.rng = rng
        self.e = {}

    def has(self, a, b):
        return (min(a, b), max(a, b)) in self.e

    def deg(self, a):
        return self._deg.get(a, 0) if hasattr(self, "_deg") else 0

    def add(self, a, b, kind, cat, value=None, crush=(), force=False):
        if a == b:
            return False
        k = (min(a, b), max(a, b))
        if k in self.e and not force:
            if crush and self.e[k]["kind"] not in ("FAMILY",):
                self.e[k]["crush"].update(crush)
                return True
            return False
        if value is None:
            lo, hi = BANDS[kind]
            value = self.rng.between(lo, hi)
        self.e[k] = {"a": k[0], "b": k[1], "kind": kind, "value": value,
                     "crush": set(crush), "cat": cat}
        return True


def side_of(i):
    return 0 if i < 500 else 1


def build_edges(cits, by_name, arch_members, rng):
    g = Graph(cits, rng)
    fams = {}
    for i, c in cits.items():
        tag = c["_opts"].get("fam")
        if tag:
            for t in tag.split(","):
                fams.setdefault(t.strip(), []).append(i)

    def names(i, key):
        s = cits[i]["_opts"].get(key)
        if not s:
            return []
        out = []
        for n in s.split(","):
            n = n.strip()
            if n.lower() not in by_name:
                raise ValueError("%s: %s=%s: no such citizen" % (cits[i]["name"], key, n))
            out.append(by_name[n.lower()])
        return out

    # 1. hand-written romance first (so families do not shadow spouses)
    for i in sorted(cits):
        for j in names(i, "wed"):
            g.add(i, j, "MARRIED", "couples", force=True)
        for j in names(i, "date"):
            g.add(i, j, "DATING", "couples")
        for j in names(i, "ex"):
            g.add(i, j, "EX", "exes")
    # 2. households
    for tag in sorted(fams):
        m = sorted(fams[tag])
        for x in range(len(m)):
            for y in range(x + 1, len(m)):
                g.add(m[x], m[y], "FAMILY", "families")
    # 3. the rest of the hand-written links
    for i in sorted(cits):
        for j in names(i, "sib"):
            g.add(i, j, "FAMILY", "families")
    for i in sorted(cits):
        for j in names(i, "bff"):
            g.add(i, j, "BEST_FRIEND", "friends")
        for j in names(i, "friend"):
            g.add(i, j, "FRIEND", "friends")
        for j in names(i, "rival"):
            g.add(i, j, "RIVAL", "rivals")
        for j in names(i, "nem"):
            g.add(i, j, "NEMESIS", "rivals")
        for j in names(i, "crush"):
            if not g.add(i, j, "ACQUAINTANCE", "crushes", value=rng.between(8, 22), crush={i}):
                pass
    # make hand-written romance compatible with romance prefs
    for e in g.e.values():
        a, b = cits[e["a"]], cits[e["b"]]
        if e["kind"] in ("DATING", "MARRIED"):
            for x, y in ((a, b), (b, a)):
                if not accepts(x, y):
                    x["romance"] = "ANY"
        for who in e["crush"]:
            x = cits[who]
            y = b if who == e["a"] else a
            if not accepts(x, y):
                x["romance"] = "ANY"

    # 4. coworkers: each archetype is a little social circle
    for key in sorted(arch_members):
        m = arch_members[key]
        if len(m) < 2:
            continue
        for x in range(len(m) - 1):
            if rng.chance(70):
                g.add(m[x], m[x + 1], "FRIEND" if rng.chance(65) else "ACQUAINTANCE", "coworkers")
        if len(m) >= 3 and rng.chance(55):
            x, y = rng.below(len(m)), rng.below(len(m))
            g.add(m[x], m[y], "BEST_FRIEND", "coworkers")
        if len(m) >= 4 and rng.chance(40):
            x, y = rng.below(len(m)), rng.below(len(m))
            g.add(m[x], m[y], "RIVAL", "coworkers")

    ids = sorted(cits)
    by_side = [[i for i in ids if side_of(i) == s] for s in (0, 1)]

    def rand_pair(side_a, side_b):
        return rng.choice(by_side[side_a]), rng.choice(by_side[side_b])

    def romance_ok(a, b):
        ca, cb = cits[a], cits[b]
        if "KID" in ca["traits"] or "KID" in cb["traits"]:
            return False
        return accepts(ca, cb) and accepts(cb, ca)

    def single(a):
        for e in g.e.values():
            if (e["a"] == a or e["b"] == a) and e["kind"] in ("DATING", "MARRIED"):
                return False
        return True

    def spray(n, kind, cat, side_a, side_b, romantic=False, crush=False, tries=40000):
        made = 0
        if not by_side[side_a] or not by_side[side_b]:
            return 0
        t = 0
        while made < n and t < tries:
            t += 1
            a, b = rand_pair(side_a, side_b)
            if a == b or g.has(a, b):
                continue
            if romantic and not (romance_ok(a, b) and single(a) and single(b)):
                continue
            if crush:
                ca, cb = cits[a], cits[b]
                if "KID" in ca["traits"] or "KID" in cb["traits"] or not accepts(ca, cb):
                    continue
                if g.add(a, b, "ACQUAINTANCE" if rng.chance(60) else "FRIEND", cat,
                         crush={a} if not (accepts(cb, ca) and rng.chance(25)) else {a, b}):
                    made += 1
                continue
            if kind in ("MARRIED",) and ("ELDER" in cits[a]["traits"]) != ("ELDER" in cits[b]["traits"]):
                continue
            if g.add(a, b, kind, cat):
                made += 1
        return made

    # 5. neighbours (same floor of the apartments: 10 homes per floor)
    for i in ids:
        j = i + 1
        if j in cits and i // 10 == j // 10 and side_of(i) == side_of(j) and rng.chance(28):
            g.add(i, j, "ACQUAINTANCE" if rng.chance(55) else "FRIEND", "neighbours")

    # 6. town-wide social web
    for s in (0, 1):
        spray(70 if s == 0 else 55, "FRIEND", "friends", s, s)
        spray(28 if s == 0 else 22, "BEST_FRIEND", "friends", s, s)
        spray(14 if s == 0 else 26, "RIVAL", "rivals", s, s)
        spray(4 if s == 0 else 14, "NEMESIS", "rivals", s, s)
        spray(18 if s == 0 else 18, "DATING", "couples", s, s, romantic=True)
        spray(10 if s == 0 else 8, "MARRIED", "couples", s, s, romantic=True)
        spray(12 if s == 0 else 16, "EX", "exes", s, s)
        spray(14 if s == 0 else 12, None, "crushes", s, s, crush=True)
    # 7. across the Wall
    spray(26, None, "cross-border crushes", 0, 1, crush=True)
    spray(18, None, "cross-border crushes", 1, 0, crush=True)
    spray(22, "FRIEND", "cross-border", 0, 1)
    spray(8, "RIVAL", "cross-border", 0, 1)
    spray(3, "NEMESIS", "cross-border", 1, 0)
    spray(5, "EX", "cross-border", 0, 1)
    return g


# =============================================================================
# writing
# =============================================================================
HEADER = """\
# core/data/roster/%(file)s - %(what)s
#
# THE 1000 PREMADE CITIZENS of Hellvile Park. This text is the source of
# truth: tools/roster_build.py validates it and compiles it into
# core/gen/roster_data.c (make -C core gen). Format: docs/ROSTER.md.
#
#   [id] Name             11 characters max, unique
#   job: ...              20 characters max
#   bio: ...              60 characters max, unique
#   key=value ...         enum values are the catalog.h names without their
#                         prefix (species=CATFOLK means SPECIES_CATFOLK);
#                         colours are paint names (PAINT_*), natural skin
#                         tones are SKIN_*; food/hobby/music/fear/phrase are
#                         the exact catalog.c strings in quotes.
#
# Lines starting with '#' are comments ('# archetype:' notes which
# family of citizens a record was designed from).
"""


def fmt_val(k, v):
    if isinstance(v, list):
        return ",".join(v) if v else "NONE"
    if k in ("food", "hates", "hobby", "music", "fear", "phrase"):
        return '"' + v.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return str(v)


def record_text(i, c):
    out = ["[%d] %s" % (i, c["name"]), "# archetype: %s (%s)" % (c["_arch"], "Hillvile" if c["_side"] == "hill" else "Hellvile"),
           "job: " + c["job"], "bio: " + c["bio"]]
    for line in rb.Schema.KEY_LINES:
        toks = []
        for k in line:
            if k == "story" and "story" not in c:
                continue
            if k == "traits" and not c["traits"]:
                continue
            toks.append("%s=%s" % (k, fmt_val(k, c[k])))
        if toks:
            out.append(" ".join(toks))
    return "\n".join(out) + "\n"


def edge_text(g, cits):
    cats = ["families", "couples", "exes", "friends", "coworkers", "neighbours", "rivals",
            "crushes", "cross-border", "cross-border crushes"]
    titles = {"families": "households (FAMILY)", "couples": "couples (DATING / MARRIED)",
              "exes": "exes", "friends": "friends and best friends",
              "coworkers": "coworkers (same archetype)", "neighbours": "next-door neighbours",
              "rivals": "rivals and nemeses", "crushes": "secret crushes",
              "cross-border": "friendships and feuds across the Wall",
              "cross-border crushes": "cross-border crushes (Hillvile <-> Hellvile)"}
    w = max(len(c["name"]) for c in cits.values())
    lines = ["# core/data/roster/edges.txt - pre-seeded relationships between the premade",
             "# citizens (compiled by tools/roster_build.py; format in docs/ROSTER.md).",
             "#",
             "#   Name A | Name B | KIND | value [| crush: NAME[, NAME]]",
             "#",
             "# KIND is REL_* without the prefix: ACQUAINTANCE FRIEND BEST_FRIEND DATING",
             "# MARRIED RIVAL NEMESIS FAMILY EX. value is -100 (hate) .. 100 (adore);",
             "# the friend/rival kinds must sit in their sim band (FRIEND 15..69,",
             "# BEST_FRIEND 70..100, ACQUAINTANCE -24..24, RIVAL -69..-15,",
             "# NEMESIS -100..-70). 'crush: X' marks who is secretly crushing.",
             "# Names can also be written #id. Keep the total under DOOMO_MAX_EDGES - 400.",
             ""]
    for cat in cats:
        es = sorted((e for e in g.e.values() if e["cat"] == cat), key=lambda e: (e["a"], e["b"]))
        if not es:
            continue
        lines.append("# ---- %s (%d) %s" % (titles[cat], len(es), "-" * max(0, 60 - len(titles[cat]))))
        for e in es:
            row = "%-*s | %-*s | %-12s | %4d" % (w, cits[e["a"]]["name"], w, cits[e["b"]]["name"],
                                                 e["kind"], e["value"])
            if e["crush"]:
                row += " | crush: " + ", ".join(cits[x]["name"] for x in sorted(e["crush"]))
            lines.append(row.rstrip())
        lines.append("")
    return "\n".join(lines)


# =============================================================================
def load_archetypes():
    mods = sorted(fn[:-3] for fn in os.listdir(HERE) if fn.startswith("cast_") and fn.endswith(".py"))
    archs = []
    for m in mods:
        mod = __import__(m)
        archs.extend(mod.ARCHETYPES)
    return archs


HE_WORDS = re.compile(r"\b(he|him|his|himself)\b", re.I)
SHE_WORDS = re.compile(r"\b(she|her|hers|herself)\b", re.I)
XE_WORDS = re.compile(r"\b(xe|xem|xyr|xemself)\b", re.I)
PRONOUN_OK = set()   # "Name" whose bio deliberately talks about someone else


FEMALE_TITLES = {"Miss", "Ms.", "Mrs.", "Madame", "Mme.", "Lady", "Dame", "Sr.", "Sister", "Ma",
                 "Mama", "Mother", "Granny", "Gran", "Nana", "Oma", "Auntie", "Princess", "Duchess",
                 "Countess", "Baroness", "Queen", "Viscountess", "Mistress"}
MALE_TITLES = {"Mr.", "Sir", "Lord", "Duke", "Pa", "Papa", "Gramps", "Grampy", "Grandpa", "Pops",
               "Brother", "Bro.", "King", "Baron", "Marquis", "Earl", "Father", "Uncle"}


JOB_SPECIES = [  # a job that names a species belongs to that species
    (r"\bImp\b", {"IMP"}), (r"\bGhoul\b", {"GHOUL"}), (r"\bBat\b", {"BATLING"}),
    (r"\bOwl\b", {"OWLKIN"}), (r"\bBunny\b", {"BUNNYKIN"}), (r"\bCacoblob\b", {"CACOBLOB"}),
    (r"(Hellhound|Doorhound|Yard Hound)", {"HELLHOUND", "PUPKIN"}), (r"\bHellpup\b", {"HELLHOUND", "PUPKIN"}),
    (r"\b(Bot|Robot)\b", {"ROBOT"}), (r"\bPlush\b", {"PLUSHIE"}), (r"\bSkeleton\b", {"SKELETON"}),
    (r"\bGhost\b", {"GHOST"}), (r"\bSlime\b", {"SLIME"}), (r"\bMoth\b", {"MOTHKIN"}),
    (r"\b(Toad|Frog)\b", {"FROGLING"}), (r"\bRat\b", {"MOUSEKIN", "RACCOONKIN"}),
    (r"\bFox\b", {"FOXKIN"}), (r"\bCat\b", {"CATFOLK"}), (r"\bSheep\b", {"SHEEPKIN"}),
    (r"\b(Grizzly|Bear)\b", {"BEARKIN"}), (r"\bCherub\b", {"CHERUB"}), (r"\bMummy\b", {"MUMMY"}),
]
JOB_HANDLERS = re.compile(r"(Keeper|Rescuer|Spotter|Walker|Groomer|Herder|Courier|Gazer|Scarer|Repairer|"
                          r"Bouncer|Doctor|Fryer|Brat)$")


def job_check(cl, v):
    """A job naming a species ("Ghoul Landlord", "Tea Bot") fits the citizen."""
    out = []
    job = v["job"]
    for rx, ok in JOB_SPECIES:
        if re.search(rx, job) and v["species"] not in ok and not JOB_HANDLERS.search(job):
            out.append("%s: %s is a %s but works as %r" % (cl.src, cl.name, v["species"], job))
    if re.search(r"\bits\b", cl.bio) and v["pronoun"] != "IT" and not cl.opts.get("bio_pronoun_ok"):
        out.append("%s: %s is %s but the bio says 'its': %r" % (cl.src, cl.name, v["pronoun"], cl.bio))
    return out


def title_check(cl, v):
    words = cl.name.split()
    out = []
    if any(w in FEMALE_TITLES for w in words) and v["pronoun"] != "SHE":
        out.append("%s: %s has a she/her title but is %s" % (cl.src, cl.name, v["pronoun"]))
    if any(w in MALE_TITLES for w in words) and v["pronoun"] != "HE":
        out.append("%s: %s has a he/him title but is %s" % (cl.src, cl.name, v["pronoun"]))
    return out


def pronoun_check(cl, v):
    """Bio pronouns must match the citizen's own pronoun (bios are written
    about the citizen)."""
    if cl.name in PRONOUN_OK or cl.opts.get("bio_pronoun_ok"):
        return []
    out = []
    for rx, want in ((HE_WORDS, "HE"), (SHE_WORDS, "SHE"), (XE_WORDS, "XE")):
        if rx.search(cl.bio) and v["pronoun"] != want:
            out.append("%s: %s is %s but the bio says %r" % (cl.src, cl.name, v["pronoun"], cl.bio))
    return out


def auto_families(archs, rng):
    """Give households to people who do not have one yet:
    * a kid gets one or two grown-ups of their own species (two grown-ups
      who can love each other are married, otherwise they are exes who
      co-parent);
    * an elder moves in with a grown-up of their own species (grandparent);
    * same-species coworkers are now and then siblings."""
    tagged = set()
    for a in archs:
        for cl in a["cast"]:
            if cl.opts.get("fam") or cl.opts.get("wed"):
                tagged.add(cl.name)
    n = 0
    for side in ("hill", "hell"):
        cast = [cl for a in archs if a["side"] == side for cl in a["cast"]]
        free = {}
        for cl in cast:
            t = cl.v["traits"]
            if cl.name in tagged or "KID" in t or "ELDER" in t or "id" in cl.opts:
                continue
            free.setdefault(cl.v["species"], []).append(cl)
        for sp in free:
            rng.shuffle(free[sp])

        def take(sp):
            lst = free.get(sp) or []
            return lst.pop() if lst else None

        def tag(cl, t):
            cl.opts["fam"] = (cl.opts["fam"] + "," + t) if cl.opts.get("fam") else t
            tagged.add(cl.name)

        for cl in cast:
            if "KID" not in cl.v["traits"] or cl.name in tagged:
                continue
            p1 = take(cl.v["species"])
            if not p1:
                continue
            n += 1
            t = "auto%d" % n
            tag(cl, t)
            tag(p1, t)
            if rng.chance(70):
                p2 = take(cl.v["species"])
                if p2:
                    tag(p2, t)
                    if accepts(p1.v, p2.v) and accepts(p2.v, p1.v):
                        p1.opts["wed"] = p2.name
                    else:
                        p1.opts["ex"] = p2.name
        for cl in cast:
            if "ELDER" not in cl.v["traits"] or cl.name in tagged:
                continue
            kid = take(cl.v["species"])
            if not kid:
                continue
            n += 1
            t = "auto%d" % n
            tag(cl, t)
            tag(kid, t)
    # siblings in the same trade
    for a in archs:
        by_sp = {}
        for cl in a["cast"]:
            if "id" not in cl.opts:
                by_sp.setdefault(cl.v["species"], []).append(cl)
        for sp, lst in sorted(by_sp.items()):
            if len(lst) >= 2 and rng.chance(30):
                x, y = lst[0], lst[1]
                if "KID" in x.v["traits"] or "KID" in y.v["traits"]:
                    continue
                x.opts["sib"] = ((x.opts.get("sib") + ",") if x.opts.get("sib") else "") + y.name


def assign_ids(archs, rng):
    problems = []
    lines = {"hill": [], "hell": []}
    for a in archs:
        for c in a["cast"]:
            lines[a["side"]].append(c)
    out = {}
    for side, base in (("hill", 0), ("hell", 500)):
        cl = lines[side]
        if len(cl) != 500:
            problems.append("%s has %d citizens (need 500)" % (side, len(cl)))
        fixed = {}
        for c in cl:
            if "id" in c.opts:
                fixed[int(c.opts["id"])] = c
        # households: union of fam tags and spouses
        parent = {}

        def find(x):
            while parent.get(x, x) != x:
                x = parent[x]
            return x

        def union(x, y):
            parent[find(x)] = find(y)
        byname = {c.name.lower(): c for c in cl}
        tags = {}
        for c in cl:
            for t in (c.opts.get("fam") or "").split(","):
                t = t.strip()
                if t:
                    if t in tags:
                        union(c.name.lower(), tags[t])
                    tags[t] = c.name.lower()
            for n in (c.opts.get("wed") or "").split(","):
                n = n.strip().lower()
                if n in byname:
                    union(c.name.lower(), n)
        groups = {}
        for c in cl:
            if "id" in c.opts:
                continue
            groups.setdefault(find(c.name.lower()), []).append(c)
        order = [groups[k] for k in sorted(groups)]
        rng.shuffle(order)
        free = [i for i in range(base, base + 500) if i not in fixed]
        k = 0
        for grp in order:
            for c in grp:
                if k < len(free):
                    out[free[k]] = c
                k += 1
        for i, c in fixed.items():
            out[i] = c
    return out, problems


VOCAB_ENUM = {"sp": "species", "body": "body", "head": "head", "eyes": "eyes",
              "brows": "brows", "nose": "nose", "mouth": "mouth", "hair": "hair",
              "fhair": "facial_hair", "horns": "horns", "wings": "wings", "halo": "halo",
              "tail": "tail", "mark": "marking", "top": "top", "bottom": "bottom",
              "shoes": "shoes", "hat": "hat", "acc": "accessory", "pat": "pattern",
              "aura": "aura", "act": "activity", "romance": "romance"}
VOCAB_TABLE = {"food": SCHEMA.foods, "hate": SCHEMA.foods, "hobby": SCHEMA.hobbies,
               "music": SCHEMA.music, "fear": SCHEMA.fears, "phrase": SCHEMA.phrases}


def validate_arch(a):
    out = []
    key = a["key"]
    paints = set(PAINTS)
    for k, val in a.items():
        if k in ("key", "side", "jobs", "cast"):
            continue
        if k not in ARCH_KEYS and not k.startswith("fur_") and not k.startswith("skin_"):
            out.append("archetype %s: unknown key %s" % (key, k))
            continue
        try:
            if k in VOCAB_ENUM:
                names = set(E[VOCAB_ENUM[k]])
                if k == "mark":
                    names.add("FUR")
                for n, w in wl(val):
                    if n not in names or w <= 0:
                        out.append("archetype %s: %s has bad entry %s:%d" % (key, k, n, w))
            elif k in VOCAB_TABLE:
                for n, w in wl(val, "|"):
                    if n not in VOCAB_TABLE[k] or w <= 0:
                        out.append("archetype %s: %s has bad entry %r" % (key, k, n))
            elif k in ("eye_col", "hair_col", "hl", "horn_col", "mark_col"):
                extra = {"FUR", "FUR2"} if k in ("hair_col",) else ({"NONE"} if k == "hl" else
                                                                     ({"FUR2"} if k == "mark_col" else set()))
                for n, w in wl(val):
                    if n not in paints | extra or w <= 0:
                        out.append("archetype %s: %s has bad colour %s" % (key, k, n))
            elif k == "scheme":
                for n, w in wl(val):
                    parts = n.split("/")
                    if len(parts) != 3 or any(p not in paints for p in parts):
                        out.append("archetype %s: bad scheme %s" % (key, n))
            elif k == "fur" or k.startswith("fur_"):
                for n, w in wl(val):
                    parts = n.split("/")
                    if len(parts) != 2 or any(p not in paints for p in parts):
                        out.append("archetype %s: bad fur pair %s" % (key, n))
            elif k == "skin" or k.startswith("skin_"):
                for n, w in wl(val):
                    if not (n in paints or (n.startswith("SKIN_") and n[5:] in SCHEMA.skins)):
                        out.append("archetype %s: bad skin %s" % (key, n))
            elif k == "cheeks":
                for n, w in wl(val):
                    if n not in SCHEMA.cheeks or not 0 <= w <= 100:
                        out.append("archetype %s: bad cheek %s" % (key, n))
            elif k == "traits":
                for n in val.replace(",", " ").split():
                    if n not in rb.TRAIT_NAMES:
                        out.append("archetype %s: bad trait %s" % (key, n))
            elif k in ("height", "build", "head_size", "eye_tilt", "nose_size", "horn_size", "corr"):
                lo, hi = rng_range(val)
                if lo > hi:
                    out.append("archetype %s: bad range %s=%s" % (key, k, val))
            elif k == "pers":
                if len(val.split()) != 5:
                    out.append("archetype %s: pers needs 5 ranges" % key)
            elif k in ("voice", "stats"):
                for item in val.split():
                    kk, spec = item.split("=")
                    if kk not in SCHEMA.fields:
                        out.append("archetype %s: %s has bad key %s" % (key, k, kk))
                    rng_range(spec)
            elif k in ("fangs", "eye_count"):
                for n, w in wl(val):
                    int(n)
        except (ValueError, KeyError) as e:
            out.append("archetype %s: %s=%r: %s" % (key, k, val, e))
    return out


def main(argv):
    write = "--write" in argv
    archs = load_archetypes()
    problems = []
    keys = set()
    for a in archs:
        if a["key"] in keys:
            problems.append("archetype key %s used twice" % a["key"])
        keys.add(a["key"])
        problems += validate_arch(a)
    hill = [a for a in archs if a["side"] == "hill"]
    hell = [a for a in archs if a["side"] == "hell"]
    # 1. every citizen's record (independent of where they live)
    seen = {}
    for a in archs:
        for idx, cl in enumerate(a["cast"]):
            if cl.name.lower() in seen:
                problems.append("duplicate name %s (%s and %s)" % (cl.name, cl.src, seen[cl.name.lower()]))
            seen[cl.name.lower()] = cl.src
            v = make_citizen(cl, idx)
            v["name"] = cl.name
            v["_opts"] = cl.opts
            v["_arch"] = a["key"]
            v["_side"] = a["side"]
            cl.v = v
            if len(cl.name) > 11:
                problems.append("%s: name %r is %d chars" % (cl.src, cl.name, len(cl.name)))
            if len(cl.bio) > 60:
                problems.append("%s: bio is %d chars: %r" % (cl.src, len(cl.bio), cl.bio))
            if len(v["job"]) > 20:
                problems.append("%s: job %r is %d chars" % (cl.src, v["job"], len(v["job"])))
            if v["skin"].startswith("SKIN_") and v["species"] not in ("HUMAN", "CHERUB"):
                problems.append("%s: natural skin tone on a %s" % (cl.src, v["species"]))
            problems += pronoun_check(cl, v)
            problems += title_check(cl, v)
            problems += job_check(cl, v)
    # 2. automatic households (decided before ids so families live together)
    auto_families(archs, Rng(0xFA417135))
    # 3. homes: households next door to each other, floors mixed
    ids, p = assign_ids(archs, Rng(0xD00D5EED))
    problems += p
    cits = {}
    arch_members = {}
    for i in sorted(ids):
        cl = ids[i]
        cits[i] = cl.v
        arch_members.setdefault(cl.arch["key"], []).append(i)
    by_name = {c["name"].lower(): i for i, c in cits.items()}
    g = build_edges(cits, by_name, arch_members, Rng(0x5EED0ED6))

    # bios that mention another citizen should be related to them
    name_re = {}
    for i, c in cits.items():
        name_re[i] = re.compile(r"(?<![A-Za-z])" + re.escape(c["name"]) + r"(?![A-Za-z])")
    for i, c in cits.items():
        for j, rx in name_re.items():
            if j != i and rx.search(c["bio"]) and not g.has(i, j):
                problems.append("bio of %s mentions %s but they are not related"
                                % (c["name"], cits[j]["name"]))

    for a in argv:
        if a.startswith("--show="):
            for nm in a[7:].split(","):
                i = by_name.get(nm.lower())
                if i is not None:
                    print(record_text(i, cits[i]))
    print("archetypes: %d Hillvile, %d Hellvile; citizens %d; edges %d"
          % (len(hill), len(hell), len(cits), len(g.e)))
    for pmsg in problems:
        print("seed: " + pmsg)
    if not write:
        return 1 if problems else 0
    if problems:
        print("seed: not writing (fix the problems above)")
        return 1
    for fn in os.listdir(DATA):
        if (fn.startswith("hillvile_") or fn.startswith("hellvile_")) and fn.endswith(".txt"):
            os.remove(os.path.join(DATA, fn))
    for start in range(0, 1000, 100):
        side = "hillvile" if start < 500 else "hellvile"
        fn = "%s_%03d.txt" % (side, start)
        what = ("Hillvile (the good side), citizens %d..%d" if start < 500 else
                "Hellvile (the bad side), citizens %d..%d") % (start, start + 99)
        with open(os.path.join(DATA, fn), "w", encoding="ascii", newline="\n") as f:
            f.write(HEADER % {"file": fn, "what": what})
            for i in range(start, start + 100):
                f.write("\n" + record_text(i, cits[i]))
    with open(os.path.join(DATA, "edges.txt"), "w", encoding="ascii", newline="\n") as f:
        f.write(edge_text(g, cits))
    print("seed: wrote core/data/roster/*.txt")
    schema, by_id, edges, story, errors, warnings = rb.load(strict=True)
    for w_ in warnings:
        print("roster_build warning: " + w_)
    for e in errors:
        print("roster_build error: " + e)
    if errors:
        return 1
    probs, lines = rb.audit(schema, by_id, edges, story)
    for ln in lines:
        print(ln)
    for pmsg in probs:
        print("audit: " + pmsg)
    return 1 if probs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
