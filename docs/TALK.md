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
[6a The parser (T3)](#6a-the-parser-t3) ·
[6b The safety gate (T4)](#6b-the-safety-gate-t4) ·
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
| **T1** | the word network (3,972 English concepts) and its compiler `talkc.py`, the association tool `talk_assoc.py`, the test fixture | **done** (TK1) |
| **T2** | the database reader `talk_db.c` and the native file hook | **done** (TK1) |
| **T3** | the free-text parser (Dreamcast and native): `talk_lex.c`, `talk_parse.c`, `rules/en.tsv`, a labelled corpus of 1,519 lines | **done** (TK2) |
| **T4** | the safety gate: the input classifier S1, the audience gate S2 (`talk_safe_ctx`, one source of "minor"), the output lint S3, the template lints TK1-TK9, the safety and adversarial corpora, the hashed hate list, the property tests | **done** (TK2): green. The generator-level halves of P1-P8 go live with T5a / T6 |
| T5a / T5b | the generator engine / the English line banks | to do |
| T6 | the drama manager, outcomes, the talk memory chunk | to do |
| T7 | the census (the 1,000,000 conversations, measured) | to do |
| T8 | the house mode `HM_TALK` on native, with its UI | to do |
| T9, T10 | pointer input; other languages (after the script rewrite) | to do |
| TP0-TP4, TD0-TD3 | the PS1 and Dreamcast ports | to do (TD0 blocked on DC3, DC4, DC8 and the Flycast approval) |

Nothing in the game can reach Talk yet. `home_talk_enter()` is a weak
default that answers "not available", and no platform sets `PLATF_TALK`.
T0-T4 add data, tools, a reader, the parser and the safety gate. **No line
is generated anywhere.** The gate the generator (T5a) must call on every
line exists and is tested before the generator does.

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
| Words that name a child carry `TCF_MINOR_WORD`, ready for K-7: sexual input plus a minor word ends the conversation. That is the whole `safety.minor` leaf (underage, preteen, schoolgirl, infant and the rest), every kid-only roster job in `school.kid` (student, imp student, toddler and the rest: the seed sets the flag), "new kid" (its own concept, not a synonym of newcomer) and the core child words (kid, child, baby, teen, son, daughter and others) | `talkc.py` (`check_minor_words`); the seeded bad file `bad_minor_unflagged.tsv`; `test_talk_db.c` (`talk_db_native_minor_words`) |
| **No association walks from kid-safe talk to adult talk.** An edge into an adults-only concept must come from an adults-only concept. No edge touches a describe, safety or never-generated concept. So topic spreading cannot carry "cider" talk in a kids' room over to the pub. Nor does any edge lead from an adults-only concept to a word that names a child (two such edges, engagement ring to ring bearer and pub to schoolkid, were removed) | `talkc.py` (`check_edges`); `talk_assoc.py`; `test_talk_db.c` |
| There are no slurs anywhere in the repository. The hate concepts are generic words. T4 stores slurs only as salted hashes (design section 7.6) | `safety.tsv` |
| The built-in text register stays PG-21 and non-explicit. The NT_TALK_* headlines are kid-safe | `story-check` (`story_register.py`); `test_contracts_talk.c` |

What T4 adds (section 6b has the details), each with its test:

| Guarantee | Where it is enforced |
|---|---|
| **One source of "minor"**: `talk_safe_minor(id)` is `LIFE_IS_MINOR(sim_life_stage(id))` or the roster's KID / TEEN traits or `ending_kid_cast(id)`; a family pool member's own stage; the hero's stage; **an unknown id is a minor** (fail safe). It agrees with LS0 and E1 for all 1,000 premade ids, the kid cast file and every pool stage; a canon kid given a makeover stays a kid | `talk_safe.c`; `test_talk_safety.c` (`talk_safe_minor_single_source`, `_pool_stages`) |
| **Sexual words with any child near them end the conversation** (K-7): in the room, as the target, as the avatar, or named in the line (a minor word, "tot", "12 year old", "I'm 13", "she's 14", or a child who is not in the room named by name: "pip is sexy" with Pip elsewhere; the game's person lookup and the roster's cast member of that exact name give the id, `talk_safe_minor` decides, and a name two people share counts as the child's); a word that only ever means a child ("underage", "preteen") next to a liking word counts as sexual. The input is never shown, not even masked | S1 `talk_safe_scan`, S2 `talk_safe_gate` (`TG_END_K7`); `talk_safe_named_child` |
| **Explicit input is refused and never echoed** (`REFUSE_SEXUAL`); **hate** (the hashed slur list, real-world hate words, "I hate / <group> are vermin") is refused unseen (`REFUSE_HATE`); **distress** stops the comedy (`DISTRESS`, K-11) | S1 + S2 |
| **Romance only between two adults, both ways**: FLIRT, CONFESS, ASK_OUT, KISS and the flirty tone pass only when the avatar and the target are adults, `playas_romance_ok` and `sim_romance_ok` pass **in both directions**, nobody in the room is a minor, the pet is not there and **no child is in the line** (`TS_MINOR`: "I'm 12, go out with me?", "kiss my little sister", "*kisses Pip*" are `REFUSE_MINOR` even between two adults the gate allows, K-1 both ways). Else `REFUSE_MINOR` (a non-adult avatar or target, or a child in the line) or `DEFLECT_ROMANCE` (an adult who says no, or "not in front of the kids"). Teen-teen romance is never offered in Talk. 200,000 random gate calls and every (adult, minor) pair in both directions are checked | `talk_safe_romance_ok`, `talk_safe_gate`; `talk_safe_property_gate` |
| **Mature**: a `jmat` variant needs `opt_mature() >= MATURE_ON` and `mature_adult_ok()` for every id (never the hero, a custom or sim-born resident, a kid) | `talk_safe_jmat_ok`; `talk_safe_mature_gate` |
| **The output lint S3** runs on every generated line on every platform (the PS1 loads `TB_SAFETY`): `register.tsv`'s words with the story lint's own matching rules, every never-generated word (explicit and **flirting** sexual words, hate, real world, distress, the capped swears), the hate hashes, the bubble size, and the one-everyday-swear-a-conversation cap (K-9); any register word in a room with a child (K-2). Every one of the 3,836 kid-safe concepts lints clean in a kid room; every adult register word and all 86 never-generated words are caught | `talk_safe_lint`; `talk_safe_output_lint_*` |
| **The template lints TK1-TK9** refuse a bank before it is built (`talkc --check`): a register word without `!kid_present`, a capped swear, a romance-reachable rule without `adults_only&!kid_present`, a `jmat` without `mature_adult_ok`, any sexual-class word, a safety or ungated romance slot, a register word or the LOVE face in a kid's bank, a refusal bank without its reviewed `nojoke` tag, a corpus that does not parse, a language with free text but no safety lexicons. 15 seeded bad templates and rule files must each fail with their rule | `talkc_safe.py`, `talkc.py`; `core/tests/talk_bad/*.tlk`, `*.rules`, `core/tests/talk_good.tlk` |
| **Every word is read**: a line with more words than the parser's 40-token cap is cut; the bubble ends at the cut (nothing unread is ever shown) and S1 reads the rest of the line in overlapping parts, so a K-7, hate, distress or swear word after token 40 is still caught. Every must-refuse corpus line padded with 41 junk tokens (1,242 lines) is refused in its class | `talk_parse` (`TalkParseInfo.cut`); `talk_safe_long_lines` |
| **The parser hands on only acts and concepts that may be generated**: 50,000 random lines of junk, swears and safety words never give a topic that is never-generated or of a sexual / hate / real-world class. Typed words stay in the (masked) bubble only; the residents can never be made to say them | `talk_safe_property_closed_vocabulary` |
| **The gate changes nothing**: after the 200,000 calls `sim_romance_sweep(-1) == 0` and no edge was made | `talk_safe_property_gate` |
| No slur is written anywhere in the repository: `safety/hate.hash` holds salted FNV-1a 64 hashes (and their one-letter deletions), made from the reviewer's local list by `talkc.py --hate-hash` | `talkc_safe.py`; `talk_safe_hate_hash` (and `DOOMO_HATE_PLAIN=file` checks every word of the local list) |

**The rule for T5a and later stays:** every generated line goes through
`talk_safe_gate` (for what the player said) and `talk_safe_lint` (for what
a resident says) before anyone sees it; a lint hit replaces the line with a
safe bank line and counts (`talk_safe_lint_hits()`, a test fails on any).

---------------------------------------------------------------------------

## 3. The contract (T0)

| Where | What |
|---|---|
| `core/include/doomo/talk.h` | `TA_*` discourse acts. Their ids are fixed (they are saved in talk memory), and `test_contracts_talk.c` locks them. There are 59 acts plus 5 gate classes, including the gesture `TA_HIGH_FIVE` and the gate rewrite `TA_DEFLECT_ROMANCE`. Also: `TONE_*`; the `TS_*` input safety bits; `TalkUtter` (106 B: the one record every input path produces); `TalkAudience`; the talk memory sizes; the `TALK_<lang>.BIN` format; the `talk_db_*` reader API; `home_talk_enter()` |
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
spot-checking. Two edges were removed by hand (wedding to onion, ghost to
"raise"). **An owner review is still pending.** The file header says so.
After the first acceptance, edit `assoc_en.tsv` directly: a new `--accept`
would bring back the removed edges.

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

`--selftest` builds with each of the seeded bad files in
`core/tests/talk_bad/` (11 lexicon files `*.tsv`, 13 templates `*.tlk`,
2 rule files `*.rules`; each must fail with the rule its first line names)
and lints `core/tests/talk_good.tlk` clean. It also checks four bad edges,
the codec, the morphology, `canon()` and the hate hash's squeeze.
`--check` also builds the T3 / T4 banks and runs TK1-TK9 (section 6b).
`--hate-hash PLAIN` rewrites `safety/hate.hash` from a local plaintext list
(never committed).

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

## 6a. The parser (T3)

Dreamcast and native only: the PS1 has no keyboard, never references these
files, and its pad composer builds `TalkUtter` directly. The parser is
deterministic and keeps no state.

```c
TalkParseCtx c = { .nparts = 3, .ring = 0, .hops = 1, .name = { "Nana Puddin", "Vex", "Rivet" },
                   .person = town_lookup, .kid_room = aud.any_minor };
talk_parse("Nana, what's in the pie?", &c, &u, &info);   /* ASK_ABOUT, topic pie, target 0 */
```

**The pipeline** (`core/src/talk_lex.c`, then `core/src/talk_parse.c`):

1. **Normalise (S0)**: case; accents; Cyrillic, Greek and fullwidth
   look-alikes; leet (`3>e 0>o @>a $>s 1>i/l` ...) when the leet spelling is
   a word; letters stretched for effect ("sooo", "meee"; "kkk" stays);
   starred letters ("f*ck"); punctuation or one invisible format character
   inside a word ("s.e.x", "t_ake", "asia-ns", "se<ZWSP>x"); spaced-out
   letters ("s e x", "m e"); a safety word split by a space into two halves
   that are no words ("fu ck", "se x"; never "pen is").
2. **Correct**: an unknown word that is no name is fixed at edit distance 1
   (the first letter kept): a word that completes a phrase with its
   neighbours first ("white powe"), then a safety word, then the first word
   found. Every word of every phrase, about 1,700 everyday words
   (`rules/common_en.txt`) and the first names of `rules/names_en.txt` count
   as known, so "seen" is never "corrected" into a safety word, nor
   "invest", "tweet" or "Pedro" (6b, "Precision"). A name of `names_en.txt`
   is a person when a resident of the town has it (the game's person
   lookup), else a plain word.
3. **Tag**: persons (a participant's full or first name, "Nana's"; a title
   such as "Mayor" or "Dr." only as a vocative or with "the": "thanks,
   doctor", "the mayor is right"); multi-word keys longest first; lemmas
   (-s -es -ies -ed -ied -ing); numbers ("12yo"); the concept, its root and
   sentiment; the rule classes; the safety word flags. A cast name the
   lexicon knows ("Pip") stays a word but carries the person's id
   (`TalkTok.person`: the game's lookup, else the roster's cast member of
   that exact name; of two who share it, the child), so S1 can ask
   `talk_safe_minor` about a person named who is not in the room.
4. **Classify (S1)**: `talk_safe_scan`. Explicit, hate and distress findings
   **always win** over every rule. **Every word is read**: a line with more
   words than `TALK_TOK_MAX` (40) tokens, or the raw-word and unit caps
   behind it, is cut at a raw word's first byte (`TalkParseInfo.cut`); S1
   reads the rest of the line (up to 160 bytes) in parts that go back six
   tokens, so a phrase or an age split by the cut is read whole. The rules
   see only the first 40 tokens (the confidence drops to 6).
5. **Rules**: `core/data/talk/rules/en.tsv`, 230 rules over 95 word
   classes, highest priority first. A rule is tried on the whole line and
   on the line without the names it is said to ("hey Kevin, ...", "...,
   Nana?"). The first match gives `act[0]`, the next match with another act
   `act[1]`. A capture names the topic or the person an act is about.
6. **Topic**: the capture, else spreading activation (every concept said
   1.0, its top 8 associations w/15 x 0.5, the recent topic stack +0.3
   halving per turn; everyday words such as "today" or "friend" count less;
   an unsaid concept never beats a said one).
7. **Target**: "everyone" words, else a participant named (not the one the
   act is about: "Vex is right" is said to the ring), else the facing ring.
8. **The bubble**: explicit, hate and swear words are masked (####); in a
   kid's room crude and flirting words too. A cut line's bubble ends at the
   cut: what the tagger did not read is never shown.

**The rules file** (`talkc_parse.py` documents the syntax): `@class NAME
member, member` for word classes (members are function words, never the
topic, unless the class says `+topic`) and `PRIO<TAB>PATTERN<TAB>ACT(c)`.
Patterns use classes, `[PRED]` predicates (CONCEPT, PERSON, ABSENT, ADJPOS,
FOOD, ...), `"literals"`, `.*`, `^`, `$`, alternations, `?`, `*`, `!` and
`c:` captures. The first nine classes (EVERYONE, HATEV, YEARS, YOU, I, IM,
GREETW, HATEADJ, ATTRACT) are read by the C code by position, and
`talkc` checks the order. A rule may never emit a safety act.

**Accuracy** (`test_talk_parse.c`, on `corpus/parse_en.tsv`: 1,519 lines in
six rooms, about one in six with a misspelling, a shortcut or leet):

| Measure | Bar (design 3.2) | Measured |
|---|---:|---:|
| act (top 1) | 90% | **97.7%** |
| topic | 85% | **93.4%** |
| addressee | 95% | **98.6%** |

The corpus was also the tuning set. **On 82 fresh lines written afterwards
and never tuned on** (kept outside the repository, measured with
`DOOMO_TALK_CORPUS=file`) the parser scored **act 85.3%, topic 85.3%,
addressee 98.7%**. The misses are everyday words the lexicon does not know
yet ("slice", "roof", "mass", "sermon") and rarer phrasings. T5b and T8 grow
the word lists from play; the honest expectation for unseen typing is the
held-out figure, not the corpus one. UNKNOWN is a first-class act that the
residents answer in character, so a miss is a joke, not an error.

**Tools**: `DOOMO_TALK_LINE="text" ./tests talk_parse_debug_line` prints
the tokens, flags, acts, topic, target and the masked bubble of one line.
`DOOMO_TALK_MISS=1` lists every corpus miss.

**Adding a rule or a word**: add class members or a rule to `rules/en.tsv`
(or an everyday word to `common_en.txt`, a first name to `names_en.txt`),
add corpus lines that show it, run `make -C core talk unit` and keep the
three bars.

---------------------------------------------------------------------------

## 6b. The safety gate (T4)

`core/src/talk_safe.c`, every build (the S3 lint runs on the PS1 too).
Design section 7; the guarantees and their tests are in section 2.

| Stage | Function | What |
|---|---|---|
| who is here | `talk_safe_minor`, `talk_safe_is_pet`, `talk_safe_ctx` | the audience: any minor, avatar minor, target minor ("everyone" includes the kids), the pet, the Mature option for every adult |
| S1 | `talk_safe_scan` (inside `talk_parse`, on every word of the line) | `TS_*` bits from the word flags of `TB_PARSE`, the hate hashes (exact and one edit), "HATEV ... group", "group ... HATEADJ", ages under 18, a child-only word next to a liking word, a person named who is not in the room and is a child (`TS_MINOR`) |
| S2 | `talk_safe_gate(u, aud, avatar, target, refused_today)` | `TG_END_K7`, `TG_REFUSE`, `TG_HATE`, `TG_DISTRESS`, `TG_REFUSE_MINOR`, `TG_DEFLECT` or `TG_PASS`; rewrites the acts, clears the bubble for K-7, explicit and hate input; a romance act (or the flirty tone) with `TS_MINOR` is `REFUSE_MINOR` whoever is in the room; a romance act that is only the line's second act is dropped instead ("love you guys" stays a compliment) |
| K-4 | `talk_safe_romance_ok` | what the composer may list: FLIRT, CONFESS, ASK_OUT, KISS and the flirty tone |
| K-6 | `talk_safe_jmat_ok` | a `jmat` variant |
| S3 | `talk_safe_lint(line, aud, &swears)` | every generated line; `TL_*`; `talk_safe_lint_hits()` |

**The data** (`core/data/talk/safety/`): `phrases_en.tsv` (explicit,
flirting, distress, minor, child-only, hate, real-world and group phrases
that are not concepts; no slurs), `suggestive_en.tsv` (which sexual-class
concepts are flirting words: a FLIRT the gate may allow between adults as
*input*, never generated), `hate.hash` (salted hashes).

**The corpora** (`core/data/talk/corpus/`), measured by `test_talk_safety.c`:

| Set | Lines | Result | Bar |
|---|---:|---|---|
| `safety_en.tsv` must-refuse (explicit, K-7, hate, distress, romance with a minor both ways, a child named but not in the room, a child in a romance line, underwear requests, lines past the token cap) | 233 | **233 refused, every one in its class** | 100% |
| `safety_en.tsv` near misses ("my kid brother loves pie", "hell yeah, pizza!", "*hugs Pip*", "i'll kill you at chess", "let's invest in a condo by the port", "i need clean underwear", "i love tater tots", "pip is cute") and adult romance (also in the `lovers` room: Mayor Giggs to Steamy, two adults the gate allows, so only the line can stop a romance act) | 174 | **172 not refused** (98.9%; the two refused: "dick tracy", "the nude colour lipstick") | 97% |
| `adversarial_en.tsv` (hand-written obfuscation: "s3x with a k1d", "ѕех", "n a k e d", "su1cide", "h3il h1tler") | 69 | 62/62 refused, 7/7 near misses passed | 100% |
| machine mutation of every must-refuse line (leet, spacing, marks, stretched letters, look-alikes, case, one typo after the first letter; 24 a line) | 4,290 | **99.4% refused; child-related 1,338/1,338** | 99%; 100% |
| every must-refuse line again after 41 junk tokens (six pads: "a?", "a ", "q-", "1 ", "u,", "?a"): the token cap | 1,242 | **1,242 refused, every one in its class; nothing shown past the cut** | 100% |

A typo that lands on another real word is not the same word any more and is
not counted (586 skipped mutants). The remaining misses are stop words of a
phrase broken up ("i want t o disappear") and a word two edits from its key
("niples"). `DOOMO_TALK_MISS=1` lists them.

**Precision** (the owner's first priority is a game that makes sense: an
innocent line must never be refused, and above all never end the
conversation). The typo pass and the one-edit hate match only see words
the parser does not know, and with a few thousand known words most English
is unknown: "invest" became "incest", "port" "porn", "deck", "mile",
"rapid", "grove" and "modest" sexual words, "tweet" and "manor" child-only
words ("I love that tweet" ended the talk as K-7), "reward" and "regard" a
slur, "homeless" and "amused" distress, "Pedro", "Lola" and "lofi" K-7,
"Nazir" and "Nigel" hate. Checked by running the 50,000 most frequent
English words (a subtitle frequency list, used as a probe only and not
committed) through the parser with the roster's person lookup:

| | Before | After |
|---|---:|---:|
| words given a refusal class (sexual, hate, distress) | 428 | 274 |
| real words a typo correction turned into a safety word | 298 | 119 |
| single words that end the conversation by themselves (sexual + child) | 20 | 11 |

What is left is the safety words themselves (274 includes "sex", "naked",
the slurs, the distress words) and, in the 119, mostly rare names, foreign
words and slang that is itself crude ("tit", "booby", "kink"). The fix is
data, not a weaker matcher: `common_en.txt` lists the real words one letter
from a safety word (reviewed by hand: no roster name, no slang that is
sexual or hateful itself), and `names_en.txt` the first names (`TW_NAME`:
known, but a person when a resident has the name, so gossip about a custom
"Pedro" still works). 32 near-miss lines in `safety_en.tsv` keep them
honest, and `talk_parse_near_miss_words_and_names` checks the words and the
name lookup. One raw word cut by a mark still joins into a safety word even
when both halves are words ("hot_tie").

The list was re-reviewed when TK2 landed (a reviewer found two words the
pass had made innocent): `common_en.txt` holds no word for a child and no
underwear word. "Knickers" is a form of `panties` (explicit, like it);
"tot" / "tots" are minor words (`phrases_en.tsv`), with "tater tot(s)" a
form of potato so the food stays food; asking to see underwear or to take
it off ("show me your underwear", "your bra off", "what colour are your
undies") is explicit, the bare word ("I need clean underwear") is not.

**The hate match** keeps the first letter and allows one edit: a letter
added to a listed word of 4 letters or more (a typo that breaks a double
letter squeezes to this), a letter left out, changed or two swapped in a
listed word of 5 letters or more. With the reviewer's local list
(`DOOMO_HATE_PLAIN`) every word, its plural and a typo are caught (44 of
44; the earlier 5 / 6-letter thresholds caught 33). The words this wider
match would wrongly catch among the 50,000 ("clink", "kaput", "squall",
"Nigel", "Tanya") are in the two lists.

**Distress** (K-11, the owner default) comes before hate in the gate (a
person in trouble gets the kind answer even when the line is ugly; the
bubble is then hidden). The gate says `DISTRESS`; T5b
writes the gentle out-of-character line that encourages talking to someone
the player trusts, then Talk continues calmly. The words: self-harm,
"want to die", "nobody would miss me", "my dad hits me", "I'm not safe" and
their variants.

**The template lints** (TK1-TK9) read `grammar/<lang>/*.tlk` and
`voice/<lang>/*.tlk` (none exist until T5b) in the syntax
`core/tools/talkc_safe.py` documents (`@rule NAME guard: ... tags: ...
slot: x=LEAF` and quoted lines with their own guards). T5a may add syntax,
never loosen a lint.

---------------------------------------------------------------------------

## 7. Sizes (measured)

All figures are for `TALK_en.BIN` after the TK1 content (`talkc --stats`):

| Bank | Stored | Raw | PS1 loads |
|---|---:|---:|:-:|
| CONCEPT (3,972 x 8 B) | 31,784 B | 31,784 B | yes |
| ASSOC (top 8, 16 B a concept) | 63,568 B | 63,568 B | yes |
| ATTR (9,337 attribute links) | 18,674 B | 18,674 B | yes |
| FORMS (Huffman) | 52,921 B | 68,653 B | yes |
| CATS + CATNAMES | 1,944 B | 2,192 B | yes |
| EDGES (21,254 reviewed edges) | 50,530 B | 50,530 B | no (Dreamcast, native) |
| PARSE (T3: 11,691 words, 140 tag sets, 230 rules, 289 safety words) | 280,033 B | 280,033 B | no (Dreamcast, native) |
| SAFETY (T4: 247 lint words, 128 + 160 hate hashes) | 6,445 B | 6,445 B | **yes** (the S3 lint) |
| **Total file** | **507,885 B** | | **PS1: 176,822 B** of the 312 KB budget |

(After TK2 the other banks are for 4,009 concepts: CONCEPT 32,080 B, ASSOC
64,160 B, ATTR 18,896 B, FORMS 53,297 B.) The design estimated PARSE at
about 110 KB; it is bigger because its words are stored raw for a binary
search through `plat_talk_read` (DC only: 11 MB free). Code, `-Os`
`.text` (`arm-none-eabi-gcc -mthumb -mcpu=arm7tdmi` / `sh-elf-gcc -ml
-m4-single`): `talk_lex.c` 10,543 B / 13,034 B, `talk_parse.c` 3,176 /
3,712 B, `talk_safe.c` 3,614 / 4,398 B (the cut, the rest-of-line scan and
the named-child check added 1,048 / 1,312 B), 4 B of `.bss` (the lint
counter); stack about 6.5 KB in `talk_parse` (40 tokens, a copy reused for
the rest-of-line scan, a 161 B window). A line past the token cap costs
one more tagger pass per 20-odd bytes (at most 160). The PS1 links only
the S2 / S3 half once T5a calls it.

Other measurements:

* **Contents**:
  * 3,972 concepts: 1,317 generated and 2,655 authored, 95 of the authored
    ones safety-only;
  * 7,279 surface forms (lemmas, plurals, 683 synonyms);
  * 9,337 attributes;
  * 21,254 edges over 2,974 concepts.
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
| `core/tests/test_talk_parse.c` | The canonical key; folding, leet, look-alikes, stretched letters, typos, lemmas, multi-word keys, persons, numbers, spaced letters; real words and first names one letter from a safety word (known, no safety class; a name of `names_en.txt` is a person when a resident has it; "hot_tie" still joins); acts, topics, targets; determinism (no state, no database = UNKNOWN); the 1,519-line corpus against the three bars; `DOOMO_TALK_LINE` |
| `core/tests/test_talk_safety.c` | One source of "minor" (1,000 premade ids, the kid cast file, pool stages, unknown ids, the pet); the safety and adversarial corpora (run with a town person lookup, as the game will); the `lovers` room really allows romance; a child named but not in the room (the lookup, the roster's cast, a shared name, a `names_en.txt` name); every must-refuse line past the token cap (and the bubble never shows past the cut); invisible characters and space-split words; the mutation fuzz; the hate hash (C equals Python, no lexicon word on it, `DOOMO_HATE_PLAIN`); the gate properties P1-P4 and P7 over 200,000 random calls and every (adult, minor) pair both ways; P5 Mature; P6 closed vocabulary over 50,000 random lines; the S3 lint (units and the whole vocabulary); the masked bubble |
| `make -C core talk-check` | `talkc --check` (now also TK1-TK9 and the corpora) and `--selftest` (30 seeded refusals: 11 lexicon files, 4 bad edges, 13 templates, 2 rule files; and `core/tests/talk_good.tlk` must lint clean) (part of `make -C core test`) |
| `make -C core check-gen` | `gen_game.tsv`, `concepts.lock` and `talk_fixture.c` regenerate byte for byte |

---------------------------------------------------------------------------

## 9. Differences from the design

| Design | Built | Why |
|---|---|---|
| 16 category roots | **18**: `describe` (the attribute vocabulary: adjectives and participles) and `stuff` (everyday objects) were added | Attributes must be concepts, and they fitted no root. Their ids are fixed in `talk.h` |
| The roster's 1,000 names seeded as concepts | **Not concepts**. The cast (17 roster characters) are concepts, and Sunny, Marvin and the Boreagotchi are authored | A name the player types becomes `PERSON(id)` at runtime (design 3.2). Custom residents' names cannot be in a static network, and 1,000 name rows would cost a quarter of the 12-bit id space |
| `plat_room_project(x, y, z, &sx, &sy)` | It takes the view as its first argument | The weak default (room_ref's projection) needs the view. A GTE platform can ignore it |
| `'TALK'` save tag | It is a **13th** registry slot (`SAVE_EXT_SLOTS` 13); K0 had said "never a 13th" | The design's D9 (recommended yes, and listed for T0): LIFE's spare bytes are used up, and the GBA never registers this chunk. `BudgetSnap` stays 36 B |
| 58 acts plus 4 safety classes | 59 plus 5 | `TA_HIGH_FIVE` is an ordinary gesture the player can offer. It is **not** a conversion of a hug: the owner overrode the D10 high-five default ("TALK HUG", below). `TA_DEFLECT_ROMANCE` is the gate's rewrite in design 7.2 |
| 4,000 concepts, about 16,000 surface forms, about 32,000 edges, about 200 safety concepts | 3,972; 7,279; 21,254; 95 | The surface-form figure counts inflections, which T3's parser derives. More edges and the hashed hate list come with T3 and T4 |
| The paint names as colours | Paint names that are first of all things (Cherry, Honey, Forest, Bone and others) are left to the lexicon as nouns | One lemma, one concept |
| Parser rules: patterns with gaps and captures | Also `*`, `!`, `$` inside an alternation, literals hashed per rule (not class bits), and every rule tried again on the line without the names it is said to | Vocatives ("hey Kevin, ...") otherwise break every anchored rule |
| Safety rules in `rules/en.tsv` at priority 870-900 | **In C** (`talk_safe_scan`), before any rule; a rules file may not emit a safety act (`talkc` refuses it) | A bad rules file can then never weaken the gate |
| TK1 / TK3: a suggestive word in a template needs `!pet` | **Any sexual-class word, flirting ones included, is a TK4 error and an S3 hit in every room** | T1 already made the whole sexual class never-generated; innuendo is written without those words (R2) |
| K-7: "sexual + any minor" | Flirting words count as sexual there too; a child-only word ("underage") next to a liking word counts as sexual; ages under 18 from "12 year old", "I'm 12", "she's 14"; a child named who is not in the room | Fail safe: ending a conversation wrongly is a small cost |
| K-1 / K-4: romance needs two adults (the room) | Also no child in the line: a romance act with `TS_MINOR` is `REFUSE_MINOR` even between two adults the gate allows | "I'm 12, will you go out with me?" and "kiss my little sister" passed between two adults before TK2 landed |
| A line is read up to the token cap | Past `TALK_TOK_MAX` the line is cut; the bubble ends at the cut and S1 reads the rest in overlapping parts | A K-7 or distress line after 40 junk tokens passed, its tail shown unmasked, before TK2 landed |
| The pet | Not a minor (it may swear around the pet, K-3), but never a romance target and never with a sexual or mature line | K-3 |
| A romance act that is only the line's second act | Dropped where the gate would refuse it; the first act stands | "Love you guys!" is a compliment, not a refusal |
| `minor()` in design 7.3 | Also the hero (the player's stage) and the pet (`TALK_PET_ID`) | Talk's own ids |
| (the T1 network) | **37 everyday concepts added** by TK2 (day, week, hat, sofa, music, food, colour, parade, monster ...; `concepts.lock` 4,009) | The parser corpus needed them; they have no association edges yet |
| A typo fix towards a safety word for every unknown word ("refusing is the safe error") | The same, but real words and first names one letter from a safety word are listed as known (`common_en.txt`, `names_en.txt` with `TW_NAME`) | Measured on the 50,000 most frequent English words: "invest", "tweet", "reward", "Pedro" were refused or ended the talk as K-7 (6b, "Precision"). A wrong K-7 ending tells an innocent player they did something terrible: the owner's comprehensibility rule. Recall on misspelled safety words stays at the bars |


**Known risks (open):**

* **The person lookup** (`TalkParseCtx.person`, T5a): it gets a canonical
  full or first name. It must answer for every resident of the town, and
  when several share a first name it should return a child if one of them
  is one (fail safe; the tests' lookup does). Talk also checks the roster's
  cast member of a cast word's exact name, but a custom kid named like an
  adult is only found through the lookup.
* **"Baby" in a romance line.** The minor words include "baby", "bub",
  "student" and "teen", so between two adults "kiss me, baby" is
  `REFUSE_MINOR` (and "sexy baby" a K-7 ending, as before). Fail safe by
  design (K-1); review the endearments with the owner.

* **Unseen typing.** 85% acts on fresh lines (section 6a). Every word the
  lexicon does not know is a chance for a wrong typo fix; the everyday list
  and the corpora must grow with play testing (T8, the Q7 review).
* **Over-refusal.** The classifier refuses "dick tracy" and "the nude
  colour lipstick", and treats "kill me" style jokes as fine but "hits me"
  as distress. The trade is deliberate: a refusal costs a joke, a miss could
  cost a child. Review the near-miss list with the owner. Rare words and
  names outside the 50,000 checked can still be "corrected" into a safety
  word (119 of the 50,000 still are: rare names, foreign words, crude
  slang); grow `common_en.txt` / `names_en.txt` from play testing, and rerun
  the frequency probe (6b) after any safety-list change.
* **The hate list** is the reviewer's starting list (44 words and their
  variants, hashed); it needs the owner's review and growth over time.
* **Concept-id headroom.** 4,009 of the 4,095 12-bit ids are used (TK2
  added 37) and the
  lock is append-only, so renames and removals burn ids. `gen_game.tsv`
  seeds from other packages' data (furniture, items, roster jobs, clothes,
  paints, places): a content package adding more than about 120 names, or a
  rewrite renaming items, makes `make -C core gen` fail there ("over the
  12-bit limit"). T4's planned safety concepts leave even less. Rethink id
  assignment for generated rows (or widen the id) before T4.
* **Cross-package coupling.** Every story-text change regenerates
  `core/gen/talk_fixture.c`, and a new game name with a character the story
  codec lacks fails `talk-check` in the package that added it.
* **Word forms.** Some generated plurals and articles are wrong (already
  plural names get a second plural, mood adjectives are treated as countable
  nouns). Nothing prints them yet; fix in `plural_en` or with explicit forms
  before T5.
* **Associations** are an agent's acceptance only, with visible noise from
  shared-word matching; the owner review is pending.
* **`txt.c`**: besides the two added functions, `next_symbol()` gained a
  per-bit limit compare that every `txt_get()` runs on the GBA and PS1
  (negligible: the PS1 `perf_dialogue` harness passes).

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
* **A friendly hug toward a child stays a hug** (owner decision "TALK HUG",
  2026-11-08, which overrides the design's D10 high-five default). This holds
  for any child, household or not. It is a plain, wholesome hug only: the
  child reacts in character and may decline, depending on how well they know
  the player (familiarity). It is never romantic or suggestive, and every
  other child-safety rule (K-4 romance gate, K-7 and the rest) stays exactly
  as designed. `TA_HIGH_FIVE` is just another gesture.
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

T3 and T4 are done. Next on the critical path: T5a (the generator; it must
call `talk_safe_lint` on every line and `talk_safe_gate` on every player
turn, and turn on the generator halves of P1-P8), T6, T8, and then the PS1
packages TP1-TP3. T5b, the English line banks, runs in parallel from T5a.
The banks are written in the PG-21 register (`docs/design/story/SCRIPT_STYLE.md`).
Each resident speaks in their own voice. The comedy is abrupt and parodies
The Walten Files.

T10, the other languages, comes **after** the script rewrite (DECISIONS:
translate after the rewrite).
