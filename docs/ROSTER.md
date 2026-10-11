# The 1000 premade citizens: modding guide

Hellvile Park ships with **1000 hand-made citizens**:

| ids | district | who |
|---|---|---|
| 0..499 | **Hillvile**, the good side | bakers, nurses, kids, grannies, sweet goths, cherubs, plushies, frogling mail carriers, robot crossing guards, ghost librarians... About 60% are fluffy critter-folk (cat, fox, bunny, bear, pup, mouse, sheep, raccoon, owl, frog and moth folk). |
| 500..999 | **Hellvile**, the bad side | imp accountants, ghoul landlords, skeleton bureaucrats, hellhound bouncers, cacoblob bookies, vape lords, edgy teens, cultists, raccoon bandits, black cats... They're demonic and edgy but still drawn cute, and about 15% are secretly lovely. |

Every citizen has a unique name, job, one-line bio and look. Each also has
personality, voice, pronouns, romance preference, birthday, favourite food,
hobby, music and colour, a hated food, a fear, a catchphrase and starting
stats. About 1500 relationships come pre-seeded: households, couples, exes,
best friends, coworkers, neighbours, rivals, nemeses and crushes across the
Wall.

Everything is plain text in `core/data/roster/`. A modder can change any
citizen with a text editor.

## The pipeline

```
core/data/roster/hillvile_000.txt ... hillvile_400.txt   citizens 0..499
core/data/roster/hellvile_500.txt ... hellvile_900.txt   citizens 500..999
core/data/roster/edges.txt                               relationships
        |
        |  tools/roster_build.py   (python3, stdlib only; validates everything)
        v
core/gen/roster_data.c   const DoomoResident records, bios, jobs, traits,
                         edges, doomo_story_npc[]  (committed, ~160 KB ROM)
        |
        v
core/src/roster.c        roster_premade() / roster_bio() / roster_job() ...
```

```
make -C core gen                       # regenerate every generated file
python3 tools/roster_build.py          # just the roster
python3 tools/roster_build.py --check  # validate only
python3 tools/roster_build.py --audit  # validate + the shipped-roster quality audit + stats
make -C core check-gen                 # CI: generated files are up to date
make -C core test                      # includes core/tests/test_roster.c
```

The generator is deterministic and gives byte-identical output on every run.
After editing the text, regenerate and commit `core/gen/roster_data.c`.

## A citizen record

Each citizen is a block that starts with an `[id] Name` header. Blank lines
and lines starting with `#` are ignored. The `# archetype:` comment notes
which family of citizens the record was designed from.

```
[42] Pip
# archetype: kids (Hillvile)
job: Student
bio: Lost their bunny plush, Mr. Flops. Very brave about it.
story=PIP traits=KID flags=STORY_NPC
pronoun=THEY romance=NONE birthday=MAR-21
species=BUNNYKIN body=TINY height=1 build=8 skin=WHITE head=MOCHI head_size=15
eyes=ANIME eye_color=SKY eye_size=14 eye_spacing=9 eye_height=8 eye_tilt=7 eye_count=2
brows=WORRIED brow_height=8 nose=BUTTON nose_size=5 mouth=POUT mouth_size=10 fangs=0 cheeks=BLUSH,FRECKLES
hair=MESSY hair_color=WHITE highlight=NONE facial_hair=NONE
ears=BUNNY horns=NONE horn_color=WHITE horn_size=9 tail=BUNNY wings=NONE halo=NONE
marking=FUR_TIPPED_EARS marking_color=BUBBLEGUM aura=NONE
top=OVERALLS top_color=SKY pattern=SOLID pattern_color=WHITE bottom=NONE bottom_color=SKY
shoes=SNEAKERS shoes_color=LEMON hat=NONE hat_color=WHITE accessory=BACKPACK accessory_color=LEMON
movement=12 speech=2 expression=14 attitude=9 quirk=7
pitch=31 speed=31 quality=25 intonation=17 accent=27
food="Carrot Cake" hates="Pickles" hobby="Bug Collecting" music="Bubblegum Pop" color=SKY fear="The Dark"
phrase="Bun-believable!"
hunger=85 happy=93 hygiene=69 energy=98 health=95 sanity=99 corruption=5 social=61
mood=HAPPY activity=PARK
```

* `[id] Name`: id is 0..999 and every id appears exactly once. A name is
  at most **11 characters** of printable ASCII and must be unique, ignoring
  case. (So "Mayor Gigglesworth" is `Mayor Giggs` and "Nana Pudding" is
  `Nana Puddin` in the roster.)
* `job:` free text, at most **20** characters.
* `bio:` free text, at most **60** characters. It must be unique, start with
  a capital (or a quote or digit) and end with punctuation. Bios describe
  the citizen, so any pronoun in a bio must match their `pronoun`.
* Everything else is `key=value` tokens, in any order, on any lines.
  Values with spaces are written in double quotes.

### Keys

Enum values are the **names from `core/include/doomo/catalog.h` without
their prefix**: `species=CATFOLK` means `SPECIES_CATFOLK`, and
`marking=FUR_TABBY` means `MARK_FUR_TABBY`. The builder reads the headers,
so new catalog options work in the text right away.

| key | values |
|---|---|
| `species` (required) | `SPECIES_*`: HUMAN IMP CHERUB GHOUL SKELETON SLIME HELLHOUND CACOBLOB BATLING ROBOT PLUSHIE GHOST MUMMY MOTHKIN FROGLING BUNNYKIN CATFOLK FOXKIN BEARKIN PUPKIN MOUSEKIN SHEEPKIN RACCOONKIN OWLKIN |
| `body` `head` | `BODY_*`, `HEAD_*` |
| `height` `build` `head_size` | 0..15 |
| `skin` | a paint name for fur or skin (`CARAMEL`, `SLIME`, ...), or a natural tone `SKIN_PORCELAIN` .. `SKIN_EBONY` (humans and cherubs) |
| `eyes` `eye_color` | `EYE_*`, paint |
| `eye_size` `eye_spacing` `eye_height` `eye_tilt` | 0..15 (tilt 8 = level) |
| `eye_count` | 1..3 (cacoblobs have 1) |
| `brows` `brow_height` | `BROW_*`, 0..15 |
| `nose` `nose_size` | `NOSE_*`, 0..15 |
| `mouth` `mouth_size` `fangs` | `MOUTH_*`, 0..15, 0..3 |
| `cheeks` | comma list of `CHEEK_*` (BLUSH, FRECKLES, SCAR, BANDAGE, BEAUTY_MARK, TEAR_TATTOO, DIMPLES, SPARKLES) or `NONE` |
| `hair` `hair_color` `highlight` `facial_hair` | `HAIR_*`, paint, paint or `NONE`, `FHAIR_*` |
| `ears` `horns` `horn_color` `horn_size` | `EARS_*`, `HORNS_*`, paint, 0..15 |
| `tail` `wings` `halo` `aura` | `TAIL_*`, `WINGS_*`, `HALO_*`, `AURA_*` |
| `marking` `marking_color` | `MARK_*` (fur patterns are `FUR_*`, and their colour is the second fur colour), paint |
| `top` `top_color` `pattern` `pattern_color` | `TOP_*`, paint, `PATTERN_*` (printed on the top), paint |
| `bottom` `bottom_color` `shoes` `shoes_color` | `BOTTOM_*`, paint, `SHOES_*`, paint |
| `hat` `hat_color` `accessory` `accessory_color` | `HAT_*`, paint, `ACC_*`, paint |
| `pronoun` | THEY SHE HE IT XE |
| `romance` | ANY SHE HE THEY NONE: who they can fall for (THEY covers they/xe/it folks) |
| `birthday` | `MON-DD`, e.g. `APR-01`. Days run 1..28 |
| `movement` `speech` `expression` `attitude` `quirk` | personality sliders 0..15 (slow..quick, polite..blunt, flat..expressive, serious..easygoing, normal..unhinged) |
| `pitch` `speed` `quality` `intonation` `accent` | voice sliders 0..31 |
| `food` `hates` | an exact `doomo_food_names` string from `core/src/catalog.c`, quoted. They must differ |
| `hobby` `music` `fear` `phrase` | exact `doomo_hobby_names` / `doomo_music_names` / `doomo_fear_names` / `doomo_catchphrases` strings, quoted |
| `color` | favourite paint |
| `hunger` `happy` `hygiene` `energy` `health` `sanity` `corruption` `social` | starting stats 0..100. Corruption must stay **below 100** (100 = possessed) |
| `mood` `activity` | `MOOD_*`, `ACT_*` from `sim.h`: where they are at 8:00 on day one |
| `flags` | comma list of `RF_*` names or `NONE`. Hellvile ids **must** have `HELLBORN`. `HAS_PET`, `TRAVELER` and `STORY_NPC` are allowed. `CUSTOM`, `PLAYER`, `FAVORITE` and `MONSTER_KILL` are player state and are rejected |
| `traits` (optional) | comma list of roster traits, see below |
| `story` (optional) | `NPC_*` without the prefix (MAYOR, FATHER_FUZZ, RIVET, DR_BLOOM, NANA_PUDDING, PIP, VEX, LORD_VAPULA). It goes together with `flags=...STORY_NPC` |

Paint names are `PAINT_*` from `palette.h`: WHITE SILVER CHARCOAL JET
CHERRY BLOOD ROSE BUBBLEGUM PEACH TANGERINE RUST HONEY LEMON CHARTREUSE LIME
MINT FOREST TEAL CYAN SKY COBALT NAVY LAVENDER GRAPE VAPE_PURPLE MAGENTA
CHOCOLATE CARAMEL BONE SLIME HELLFIRE ECTOPLASM.

Every record is written out in full, and `--audit` / `--strict` insist on
it. When you write a **new** record you only need `[id] Name`, `job:`,
`bio:` and `species=`. Every other key gets a species-aware default: a
catfolk gets cat ears, a cat tail, a kitty nose and caramel fur; a
cacoblob gets one eye; a skeleton gets a skull. Then add what makes them
*them*.

### Roster traits (`roster_traits(id)`)

Traits describe who a premade citizen *is*. They are not stored in
`DoomoResident` or in saves; the story and the life sim read them.

| trait | meaning |
|---|---|
| `SOFTIE` | Secretly lovely. Hellborn only. Their bios hint at it, and Chapter 4 redeems them first |
| `KID` | A child. No romance at all |
| `TEEN` | A teenager. Romance and crushes only with other teens, never with adults; teens do not marry |
| `ELDER` | Grannies, grandpas and ancient things |
| `VAPER` | Already vapes, so they're the first to turn weird in Chapter 2 |
| `HOLY` | Church folk who can help with exorcisms |
| `LOYALIST` | A die-hard Vapula loyalist who is hard to redeem |
| `INFILTRATOR` | A Hellvile agent working among Hillvile (Vex's crew) |
| `NIGHT_OWL` | Keeps night hours (night shift, ghosts, gamers) |

## The story cast

Story NPCs sit at fixed, memorable ids. `roster.h` exposes these as
`ROSTER_ID_*`, and `doomo_story_npc[]` is generated from the `story=` keys.

| NPC | id | name | who |
|---|---|---|---|
| `NPC_MAYOR` | 0 | Mayor Giggs | cherub mayor of Hillvile, laughs at his own speeches |
| `NPC_NANA_PUDDING` | 1 | Nana Puddin | sheepkin head baker, gives you the Doomogachi egg |
| `NPC_RIVET` | 13 | Rivet | imp welder who ran away from Hellvile: lives in Hillvile but `HELLBORN`, with her family (Anvil, Sprocket, Bolt, Nana Clank) across the Wall |
| `NPC_PIP` | 42 | Pip | bunnykin kid (they/them) who lost their bunny plush, Mr. Flops |
| `NPC_FATHER_FUZZ` | 77 | Father Fuzz | plushie priest of St. Cuddles, banishes demons with hugs |
| `NPC_DR_BLOOM` | 111 | Dr. Bloom | gentle ghoul doctor at the hospital |
| `NPC_VEX` | 666 | Vex | foxkin owner of Cloud Nine Vapor, a Hellvile infiltrator |
| `NPC_LORD_VAPULA` | 999 | Lord Vapula | cacoblob Duke of the Ninth Cloud, the final boss |

To move a story NPC, change its `[id]`, update the matching
`ROSTER_ID_*` in `roster.h`, and regenerate. The builder refuses to build
if they disagree.

## Relationships: `edges.txt`

```
Name A | Name B | KIND | value [| crush: NAME[, NAME]]
Muffin    | Crumpet  | MARRIED      |   93
Mopey Mae | Grimbert | ACQUAINTANCE |   21 | crush: Grimbert
```

* Names may also be written `#id`. Order inside a pair doesn't matter (the
  generated graph always stores `a < b`), and each pair may appear once.
* `KIND` is `REL_*` without the prefix: ACQUAINTANCE FRIEND BEST_FRIEND
  DATING MARRIED RIVAL NEMESIS FAMILY EX.
* `value` runs from -100 (hate) to 100 (adore). The kinds the sim promotes
  and demotes on its own must sit in their band, or they would flip on day
  one:

  | kind | band |
  |---|---|
  | ACQUAINTANCE | -24..24 |
  | FRIEND | 15..69 |
  | BEST_FRIEND | 70..100 |
  | RIVAL | -69..-15 |
  | NEMESIS | -100..-70 |

* `crush:` names who is secretly crushing on the other. It can be one or
  both.
* Romance must fit: DATING, MARRIED and crushes must match each person's
  `romance` preference. Kids have no romance. Teens only date teens and
  never marry. Families have no crushes.
* Keep the total under `DOOMO_MAX_EDGES - 400` (1648). That leaves room for
  the friendships that form during play.

The shipped file groups edges by story: households, couples, exes,
friends, coworkers, next-door neighbours (10 homes per apartment floor),
rivals, secret crushes, cross-Wall friendships and feuds, and cross-border
crushes.

## What the builder checks

Errors stop the build; warnings are printed:

* Every id 0..999 appears once. Every enum value, range, paint, skin and
  table string is valid. Names and bios are unique and within their
  lengths, ASCII only.
* Hellvile ids are `HELLBORN`. No player-state flags. `story=` and
  `STORY_NPC` go together. Every NPC appears once, at its `ROSTER_ID_*`.
* Corruption is below 100, and food differs from hates.
* **No two citizens share an identical look** (all 48 bytes).
* Edge rules as above.

`--audit` adds the quality bar the shipped roster meets. A modded roster
does not have to meet it, but CI checks the shipped one:

* Every option of every catalog list is used at least once: species, body,
  head, eyes, brows, nose, mouth, ears, hair, facial hair, horns, tail,
  wings, halo, marking, top, bottom, shoes, hat, accessory, aura, pattern,
  pronoun, romance and all 8 cheek flags. So is every food, hobby, music,
  fear, catchphrase and birthday month.
* Colour diversity: all 32 paints appear as eye, hair, top and favourite
  colours, and secondary colour fields use at least 26.
* Hillvile is 55-70% critter-folk; Hellvile is mostly demonic with a good
  share of edgy critter-folk.
* 12-18% of Hellvile are `SOFTIE`, and at least 10% of citizens are
  they/them.
* Average Hellvile corruption is at least 25 above Hillvile's.
* Everyone starts healthy, and there are at least 250 distinct jobs.
* 1200-1648 edges, every relationship kind used, at least 20 cross-border
  crushes, and almost nobody without a single relationship.

`core/tests/test_roster.c` runs the same checks on the compiled data in C.
It also checks that every record passes `resident_sanitize()` with zero
fixes and survives `town_new(..., TOWN_POP_PREMADE)` byte for byte with
the whole graph intact.

## How the roster was made: `core/data/roster/authoring/`

The text files were seeded by `authoring/seed.py` from **193
archetypes**: 98 Hillvile and 95 Hellvile, in `cast_hill_*.py` and
`cast_hell_*.py`. Each archetype is a family of citizens with:

* a shared job vocabulary;
* species weights;
* a coherent colour scheme (main / accent / extra);
* outfit, hat, accessory, pattern and face vocabularies;
* personality ranges and favourite things.

Under each archetype is its hand-written cast, one line per citizen:

```
Muffin | s | Bakes muffins shaped like everyone in town. | job=Muffin Artist; species=BUNNYKIN; fam=bunbury
```

The name, pronoun code (s/h/t/x/i) and bio are hand-written. The options
hand-vary any record key (`hat=CHEF`, `species=HUMAN`) and add
relationships: `fam=` (household), `wed=`, `date=`, `ex=`, `bff=`,
`friend=`, `rival=`, `nem=`, `crush=`, `sib=`, plus `id=` for a fixed id
and `trait=`. Looks follow the same species-coherence rules as
`resident_generate()`, and a deterministic RNG is seeded from each name.

On top of the hand-written links, the tool generates the rest:

* households: kids get same-species parents, and elders move in with a
  grown-up;
* coworker circles inside each archetype;
* next-door neighbours;
* a town-wide web of friends, rivals, couples and exes;
* cross-Wall friendships and crushes.

Households are placed next door to each other in the apartments. The
tool also checks that bio pronouns, gendered titles ("Sir", "Granny") and
jobs that name a species ("Ghoul Landlord", "Tea Bot") match each citizen.

```
python3 core/data/roster/authoring/seed.py           # dry run + report
python3 core/data/roster/authoring/seed.py --write   # rewrite the .txt files
```

**`--write` regenerates the text from scratch.** Use it to reseed or
restyle whole archetypes before a release. To change individual citizens,
edit the `.txt` files directly and don't rerun the seeder.

## Recipes

* **Restyle a citizen**: edit their block, run `python3 tools/roster_build.py`,
  and commit the `.txt` and `core/gen/roster_data.c`.
* **Replace a citizen**: rewrite the block at that id (same `[id]`, new
  name/job/bio/look), then fix any lines in `edges.txt` that use the old
  name.
* **Marry two citizens**: add `A | B | MARRIED | 90` to `edges.txt`. Make
  sure their `romance=` accepts each other's `pronoun=`.
* **Make someone secretly lovely**: add `traits=SOFTIE` (hellborn only),
  give them a bio that hints at it, and lower their corruption.

## Runtime API (`core/include/doomo/roster.h`)

```c
const DoomoResident *roster_premade(int id);  /* NULL outside 0..999          */
const char *roster_bio(int id);               /* "" outside 0..999            */
const char *roster_job(int id);
const char *roster_name(int id);
int  roster_traits(int id);                   /* ROSTER_T_* bits              */
int  roster_find(const char *name);           /* case-insensitive, -1 if none */
int  roster_story_npc(int npc);               /* NPC_* -> id                  */
int  roster_npc_of(int id);                   /* id -> NPC_* or -1            */
int  roster_count_traits(int traits, int district);
extern const DoomoEdge doomo_roster_edges[];  /* sorted (a, b), a < b         */
extern const uint16_t  doomo_roster_edge_count;
extern const uint16_t  doomo_story_npc[NPC_COUNT];
```

All the data is `const`: about 160 KB of ROM on GBA (96 KB of records,
45 KB of bios, 9 KB of edges, 8 KB of jobs), and nothing in RAM. Every
lookup costs O(1), except `roster_find` and `roster_count_traits`, which
scan the roster and are meant for menus and setup.
`town_new(..., TOWN_POP_PREMADE)` copies the records and the graph into
the town, and saves store residents as deltas against these records. An
untouched premade island saves in about 7 KB.
