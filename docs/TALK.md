# Talk: the live house conversation

Talk is the Facade-style conversation mode of the house in
**BOREagotchi LIFE! - in HELLvile PARK...**. You stand in a room with one to
three residents and talk with them, and they talk with each other, in their
own voices, while the room keeps rendering in pseudo-3D. It is for the
**PS1 and the Dreamcast** (the `PLATF_TALK` feature bit). The native runner
runs it for tests. The GBA never has it.

The full design, with the parser, the drama manager, the generator, the
census and the PS1 and Dreamcast budgets, is
[docs/design/talk/TALK_DESIGN.md](design/talk/TALK_DESIGN.md). This page
says **what is built**, how to use and extend it, and where the build
differs from the design.

Contents:
[1 Status](#1-status) ·
[2 Safety](#2-safety-non-negotiable) ·
[3 The contract (T0)](#3-the-contract-t0) ·
[4 The word network (T1)](#4-the-word-network-t1) ·
[5 The compiler](#5-the-compiler-talkcpy) ·
[6 The database and its reader (T2)](#6-the-database-and-its-reader-t2) ·
[7 Sizes](#7-sizes-measured) ·
[8 Tests](#8-tests) ·
[9 Differences from the design](#9-differences-from-the-design) ·
[10 Owner decisions](#10-owner-decisions) ·
[11 What comes next](#11-what-comes-next)

---------------------------------------------------------------------------

## 1. Status

| Package | What | State |
|---|---|---|
| **T0** | the contract: `talk.h`, `PLATF_TALK`, talk framing fields, `plat_room_project`, the `'TALK'` save chunk's tag and budget, `NT_TALK_*` news | **done** (TK1) |
| **T1** | the word network (3,971 English concepts) and its compiler `talkc.py`, the association tool `talk_assoc.py`, the test fixture | **done** (TK1) |
| **T2** | the database reader `talk_db.c` and the native file hook | **done** (TK1) |
| T3 | the free-text parser (Dreamcast and native) | to do |
| T4 | the safety gate (input classifier, audience gate, output lint, property tests) | to do: **must be green before any generated line can be reached in play** |
| T5a / T5b | the generator engine / the English line banks | to do |
| T6 | the drama manager, outcomes, the talk memory chunk | to do |
| T7 | the census (the 1,000,000 conversations, measured) | to do |
| T8 | the house mode `HM_TALK` on native, with its UI | to do |
| T9, T10 | pointer input; other languages (after the script rewrite) | to do |
| TP0-TP4, TD0-TD3 | the PS1 and Dreamcast ports | to do (TD0 blocked on DC3, DC4, DC8 and the Flycast approval) |

Nothing in the game can reach Talk yet. `home_talk_enter()` is a weak
default that answers "not available", and no platform sets `PLATF_TALK`.
T0-T2 add data, tools and a reader. **No line is generated anywhere.**

---------------------------------------------------------------------------

## 2. Safety (non-negotiable)

The rules are the design's section 7 (K-1 to K-11). In short:

* There is no romance, flirting or sexual talk with, by or near anyone who
  is not an adult, **in either direction**.
* "Not an adult" comes from the game's existing sources:
  * `sim_life_stage()` and `LIFE_IS_MINOR`;
  * the roster's `ROSTER_T_KID` and `ROSTER_T_TEEN` traits;
  * `ending_kid_cast()`, the compiled `core/data/story/kid_cast.txt`.
* Talk keeps **no list of its own**.
* Every romance act goes through `sim_romance_ok()` and the sim's own
  `sim_confess()`.
* `mature_adult_ok()` and the kid cast are never weakened.

What T0-T2 already guarantee, with a test for each:

| Guarantee | Where it is enforced |
|---|---|
| Every concept has a register class (clean, crude, strong, sexual, hate, realworld, distress) and an age class (any, adult, never), so the gate (T4) and the generator (T5) can filter by them | `talk.h`; `talkc.py` |
| The register classes agree with `core/data/story/register.tsv`, both ways. A concept marked crude, strong or sexual must have a form the list matches with that class. A form the list matches must carry that class. Every register word has a concept, so the parser will know it | `talkc.py` (`check_concepts`) |
| Sexual, hate, realworld and distress concepts, and everything in a `safety.*` leaf, are **never generated** (age class `never`). The capped swears (the f-word, the s-word, *bastard*, and the other strong words) are never generated either. Only the everyday adult swears are left: *damn*, *crap*, "what the hell". They are adults-only, and T5 caps them at one per conversation (K-9) | `talkc.py`; `test_talk_db.c` |
| Romance words are adults-only (`romance.*` is a gated root) | `talkc.py`; the seeded bad file `bad_romance_any.tsv` |
| Words that name a child carry `TCF_MINOR_WORD` (kid, child, baby, toddler, teen and others), ready for K-7: sexual input plus a minor word ends the conversation | the lexicon; `test_talk_db.c` |
| **No association walks from kid-safe talk to adult talk.** An edge into an adults-only concept must come from an adults-only concept. No edge touches a describe, safety or never-generated concept. So topic spreading cannot carry "cider" talk in a kids' room over to the pub | `talkc.py` (`check_edges`); `talk_assoc.py`; `test_talk_db.c` |
| There are no slurs anywhere in the repository. The hate concepts are generic words. T4 stores slurs only as salted hashes (design section 7.6) | `safety.tsv` |
| The built-in text register stays PG-21 and non-explicit. The NT_TALK_* headlines are kid-safe | `story-check` (`story_register.py`); `test_contracts_talk.c` |

T4 builds the rest: the classifier, `talk_safe_ctx()`, the S2 rewrite, the
S3 output lint and the P1-P8 property tests. **Until T4 is green, no
package may connect the generator to anything the player can reach.**

---------------------------------------------------------------------------

## 3. The contract (T0)

| Where | What |
|---|---|
| `core/include/doomo/talk.h` | `TA_*` discourse acts. Their ids are fixed (they are saved in talk memory), and `test_contracts_talk.c` locks them. There are 59 acts plus 5 gate classes, including `TA_HIGH_FIVE` (D10) and `TA_DEFLECT_ROMANCE`. Also: `TONE_*`; the `TS_*` input safety bits; `TalkUtter` (106 B: the one record every input path produces); `TalkAudience`; the talk memory sizes; the `TALK_<lang>.BIN` format; the `talk_db_*` reader API; `home_talk_enter()` |
| `plat_ext.h` | `PLATF_TALK` (bit 9). `plat_talk_load(code)` returns the file size, 0, or `PLAT_TALK_LOADING` while the PS1 is still staging it. `plat_talk_read(off, dst, len)`. `plat_room_project(view, x, y, z, &sx, &sy)` |
| `platform.h` | `DoomoRoomView.zoom == 2` is "talk framing", with `focus_x`, `focus_y` and `focus_r` (tile space, 16.16). Renderers that do not know zoom 2 treat it as the close-up (the PS1 already does, and `room_ref` ignores zoom) |
| `save_ext.h`, `save_budget.h` | The `'TALK'` chunk: slot 13, weak NULL until T6, `SAVE_EXT_MAX_TALK` 320 B (292 B worst case). The joint worst case with it fits both slots: a post-game story save at its worst is 29,139 B, plus the 2,048 B margin is 31,187 B, under the 32,768 B gate (36 KB minus PA10's 4 KB); free play 68,945 B of 90,112 B |
| `sim.h`, `sim_events.c` | `NT_TALK_ROW`, `NT_TALK_MADE_UP`, `NT_TALK_GREAT_NIGHT`, `NT_TALK_WALKOUT` ("Big night at yours! %s and %s still giggle."). They fit the 64 B feed line with two 11-letter names |
| `core/src/weak_talk.c` | The weak `plat_talk_*` hooks and `home_talk_enter` (all "no Talk here") |
| `core/src/room_ref.c` | `room_ref_project()`. It shares `room_ref_draw()`'s origin, and it is the weak `plat_room_project()`. It is the anchor `draw_actor()` puts a resident's feet on |

---------------------------------------------------------------------------

## 4. The word network (T1)

Everything is under `core/data/talk/`. All of it is text: reviewers read the
diffs.

| File | What |
|---|---|
| `categories.tsv` | The category tree: 18 roots in `TCR_*` order and 114 leaves. Each leaf has flags (`talk`, `gated`, `safety`, `describe`) and default sentiment, register, age and attributes |
| `lexicon/en/*.tsv` | **Authored** concepts, one per row: `KEY CAT SENT REG FORMS SYNONYMS ATTRS FLAGS`, with `.` for a default. The 18 files `describe`, `feelings`, `people`, `places`, `food`, `music`, `hobbies`, `fears`, `creatures`, `house`, `lore`, `hellvile`, `work`, `time`, `school`, `romance`, `safety` and `stuff` hold 2,654 rows |
| `lexicon/en/gen_game.tsv` | **Generated** from the game (`make -C core gen`; `check-gen` keeps it current). It has 1,317 rows: the cast (from the roster), the species, the 48 foods, 32 hobbies, 16 genres and 24 fears, the moods and persona types, the venues and story places, the colours, the clothes, the items, the 349 furniture pieces and the 600-odd roster jobs. Jobs that only kids hold go to `school.kid`. Every row says where it came from (`# gen:`) |
| `lexicon/en/override.tsv` | Hand edits of generated rows: a better category, attributes, flags. Regenerating never loses them |
| `concepts.lock` | **Append-only** concept ids. Talk memory saves topic ids, so a key keeps its id forever. A removed key is marked `retired`, and its id is never reused. `make gen` writes it. `talkc --check` fails when a key has no id |
| `assoc_hand_en.tsv` | Association edges a writer wants regardless of what the game text says (Nana Puddin and pie, Grumbleguts and the gate) |
| `assoc_en.tsv` | The **reviewed** association edges. This is what ships |
| `fixture.txt` | The concepts of the unit tests' small database |

**Adding a concept**:

1. Add a row to the right `lexicon/en/*.tsv`. Use `@cat LEAF` to set the
   category for the rows after it.
2. Run `make -C core gen` (it updates the lock).
3. Run `python3 core/tools/talk_assoc.py` and review its suggestions for
   the new key.
4. Copy the accepted lines into `assoc_en.tsv`.
5. Run `make -C core talk-check`.

Forms are worked out for you: the plural (with an irregular table), a or an,
mass nouns ("some butter"), names ("Nana Puddin") and "the" names ("the
Feast"). Write `lemma/plural` when English does something odd.

**Associations**: `talk_assoc.py` proposes up to 12 edges a concept from
these sources:

* PMI over the game's own text (9,494 lines: the story packs, roster bios,
  furniture descriptions and UI strings);
* attribute similarity (IDF-weighted cosine of each row's own attributes);
* small-leaf siblings;
* shared lemma words;
* the hand edges.

Its suggestions go to `core/build/talk/` and are never committed. The
`--accept` mode is the reviewer's tool. It keeps the top 8 and every hand
edge. The shipped file was accepted by the TK1 agent pass after
spot-checking. **An owner review is still pending.** The file header says
so.

---------------------------------------------------------------------------

## 5. The compiler (`talkc.py`)

```
make -C core gen          # gen_game.tsv, concepts.lock, core/gen/talk_fixture.c
make -C core talk         # core/build/talk/TALK_en.BIN (never committed)
make -C core talk-check   # every lint + the seeded bad inputs (part of make test)
python3 core/tools/talkc.py --stats    # counts and bank sizes
python3 core/tools/talkc.py --bin --align 2048 --out X   # sector-aligned banks for a disc (TP1)
```

What it checks, failing the build on any problem:

* the tree's shape and the order of the roots;
* each row's columns;
* no key twice;
* no lemma twice;
* no synonym shared between concepts;
* no authored row that duplicates a generated one;
* every attribute and every association is a concept, with at most 8
  attributes;
* every form can be drawn by the font **and** encoded by the story codec;
* register agreement (section 2);
* safety leaves and romance never generated or never for kids;
* the edge rules;
* the lock;
* a byte-identical rebuild;
* the codec round trip of every string.

`--selftest` builds with each of the 10 seeded bad files in
`core/tests/talk_bad/` (each must fail with the rule its first line names).
It also checks three bad edges, the codec and the morphology.

---------------------------------------------------------------------------

## 6. The database and its reader (T2)

**The format** is `talk.h`.

* A 20 B header: magic, version, language, **dict_hash**, the number of
  banks and concepts, the total size.
* A directory of banks, each with its CRC-32.
* The banks, 64 B aligned.
* Strings use **the story's own canonical-Huffman code** (`txt.c`).
  * Talk adds no decoder and no dictionary to PS1 RAM.
  * `dict_hash` is `txt_codec_hash()`, so a database built against another
    story text is refused (`TALK_DB_DICT_MISMATCH`).
  * The build makes the database depend on `txt_dict.c`.
* Two consequences for anyone who changes the story text:
  * Run `make -C core gen`, as always. It also re-encodes
    `talk_fixture.c`.
  * A character the story text never uses has no code, so `talkc` reports
    a lexicon form that needs one.

**The reader** (`core/src/talk_db.c`, about 250 B of `.bss` for the
directory, nothing resident):

```c
talk_db_open(lang) / talk_db_open_mem(image, size) / talk_db_close() / talk_db_verify()
talk_db_concept(id, &c)        /* category, sentiment, register, age, flags */
talk_db_form(id, TF_INDEF, buf, cap)   /* "a pie", "an owl", "some butter", "the Feast" */
talk_db_attrs(id, ids)         /* flaky, baked, stolen ... */
talk_db_assoc(id, ids, w)      /* the top 8; talk_db_edges(): all of them (DC, native) */
talk_db_cat / _cat_name / _cat_root
```

Everything is read through `plat_talk_read()`, a few bytes at a time. On
the PS1 the file will live in SPU RAM (TP1). The native platform reads
`$DOOMO_TALK_DIR/TALK_<code>.BIN`, or by default `core/build/talk/`, whole,
into the heap.

---------------------------------------------------------------------------

## 7. Sizes (measured)

All figures are for `TALK_en.BIN` after the TK1 content (`talkc --stats`):

| Bank | Stored | Raw | PS1 loads |
|---|---:|---:|:-:|
| CONCEPT (3,971 x 8 B) | 31,776 B | 31,776 B | yes |
| ASSOC (top 8, 16 B a concept) | 63,552 B | 63,552 B | yes |
| ATTR (9,334 attribute links) | 18,668 B | 18,668 B | yes |
| FORMS (Huffman) | 53,430 B | 69,467 B | yes |
| CATS + CATNAMES | 1,944 B | 2,192 B | yes |
| EDGES (21,424 reviewed edges) | 50,794 B | 50,794 B | no (Dreamcast, native) |
| **Total file** | **220,458 B** | | **PS1: 169,370 B** of the 312 KB budget |

Other measurements:

* **Contents**:
  * 3,971 concepts: 1,317 generated and 2,654 authored, 95 of the authored
    ones safety-only;
  * 7,364 surface forms (lemmas, plurals, 684 synonyms);
  * 9,334 attributes;
  * 21,424 edges over 2,976 concepts.
* **The fixture** (`core/gen/talk_fixture.c`): the 49 concepts
  `fixture.txt` names plus the 89 attributes they use (138 concepts),
  7,300 B. Only the tests link it.
* **RAM**:
  * nothing on the GBA: `--gc-sections` drops the reader, the fixture and
    the weak hooks, and nothing references them;
  * `talk_db.c` is about 250 B of `.bss` and 128 B of stack per string;
  * `txt.c` gained one compare per decoded bit, plus `txt_decode()` and
    `txt_codec_hash()`.

---------------------------------------------------------------------------

## 8. Tests

| Test | What |
|---|---|
| `core/tests/test_contracts_talk.c` | The act ids and groups; `TalkUtter`'s layout; `PLATF_TALK` and the weak hooks; the room projection (origin, tile steps, height, a quarter turn equal to the turned room, pan, zoom 2 ignored by the reference, the platform hook equal to the reference); the `'TALK'` chunk and the joint worst case; the four headlines with the longest names |
| `core/tests/test_talk_db.c` | The fixture opens and verifies; forms and articles (plurals, irregulars, mass nouns, names, "the" names, truncation); categories and roots; the safety classes the gate will read (sexual and capped never, everyday swears adult, romance adult, minor words); attributes; associations (range, order, talkable, **no edge into adult from non-adult**, EDGES begins with ASSOC); refusals (CRC, magic, version, dictionary, truncation, an out-of-range bank, a duplicate bank); CRC-32; the full `TALK_en.BIN` through the native platform |
| `make -C core talk-check` | `talkc --check` and `--selftest` (part of `make -C core test`) |
| `make -C core check-gen` | `gen_game.tsv`, `concepts.lock` and `talk_fixture.c` regenerate byte for byte |

---------------------------------------------------------------------------

## 9. Differences from the design

| Design | Built | Why |
|---|---|---|
| 16 category roots | **18**: `describe` (the attribute vocabulary: adjectives and participles) and `stuff` (everyday objects) were added | Attributes must be concepts, and they fitted no root. Their ids are fixed in `talk.h` |
| The roster's 1,000 names seeded as concepts | **Not concepts**. The cast (17 roster characters) are concepts, and Sunny, Marvin and the Boreagotchi are authored | A name the player types becomes `PERSON(id)` at runtime (design 3.2). Custom residents' names cannot be in a static network, and 1,000 name rows would cost a quarter of the 12-bit id space |
| `plat_room_project(x, y, z, &sx, &sy)` | It takes the view as its first argument | The weak default (room_ref's projection) needs the view. A GTE platform can ignore it |
| `'TALK'` save tag | It is a **13th** registry slot (`SAVE_EXT_SLOTS` 13); K0 had said "never a 13th" | The design's D9 (recommended yes, and listed for T0): LIFE's spare bytes are used up, and the GBA never registers this chunk. `BudgetSnap` stays 36 B |
| 58 acts plus 4 safety classes | 59 plus 5 | `TA_HIGH_FIVE` is the D10 default ("a hug toward a non-household child becomes a high five"). `TA_DEFLECT_ROMANCE` is the gate's rewrite in design 7.2 |
| 4,000 concepts, about 16,000 surface forms, about 32,000 edges, about 200 safety concepts | 3,971; 7,364; 21,424; 95 | The surface-form figure counts inflections, which T3's parser derives. More edges and the hashed hate list come with T3 and T4 |
| The paint names as colours | Paint names that are first of all things (Cherry, Honey, Forest, Bone and others) are left to the lexicon as nouns | One lemma, one concept |

---------------------------------------------------------------------------

## 10. Owner decisions

These are the defaults from DECISIONS.md "TALK DEFAULTS" (2026-11-08). The
owner may override any of them.

* **The player speaks in Talk.** This is an exception to "@P: Silent". The
  story stays silent.
* **PS1 SPU RAM is split**: SFX use 0x2000-0x2FFFF, Talk uses
  0x30000-0x7DFFF.
* **No teen-teen romance acts in Talk.** The life sim keeps its own
  same-age teen rule.
* **Typed self-harm content** gets a gentle line, out of character and not a
  joke, that encourages the player to talk to someone they trust. Then Talk
  continues calmly.
* **A language without a Talk bank** falls back to English with a notice.
* **A hug toward a non-household child** becomes a high five.
* **Dreamcast Talk waits** for DC3, DC4 and DC8, and for the owner's
  approval of the Flycast core build.

The UI will follow the owner's top priority, comprehensibility. T8 must
make the mode obvious. It needs:

* a help line;
* clear prompts;
* the quick replies first;
* the press budgets of design section 2.3.

---------------------------------------------------------------------------

## 11. What comes next

The critical path is T3, then T4 (**safety: the gate and its property tests
before anything else connects**), then T5a, T6, T8, and then the PS1
packages TP1-TP3. T5b, the English line banks, runs in parallel from T5a.
The banks are written in the PG-21 register (`docs/design/story/SCRIPT_STYLE.md`).
Each resident speaks in their own voice. The comedy is abrupt and parodies
The Walten Files.

T10, the other languages, comes **after** the script rewrite (DECISIONS:
translate after the rewrite).
