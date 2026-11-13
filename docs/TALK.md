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
[6c The generator (T5a)](#6c-the-generator-t5a) ·
[6d The census and the review sheet (T7)](#6d-the-census-and-the-review-sheet-t7) ·
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
| **T5a** | the generator: `talk_gen.c` (the grammar VM, guards, fillers, sim-fact callbacks, novelty, re-picks, the bubble fit, the per-conversation RNG, the trace), the `.tlk` grammar compiler `talkc_gen.py`, the adjacency table `acts.tsv`, a plain exchange (the gate first, who answers, reactions between residents) | **done** (TK3); a **starter** English bank (718 alternatives in 12 files, the K-10 grief and comfort banks included) so every act, refusal and part plays |
| T5b | the English line banks (about 6,000 alternatives, design 3.6) and the owner's review | in progress: 6,742 alternatives in grammar/en (starter 718 + T5B-WARM 739: warm acts + T5B-COOL 600: cool and rude acts + T5b-ASK 975: questions and answers + T5B-OPEN 648: openers, parts and the rest + T5B-ROM 492: romance, adults only + T5B-TOPIC 771: topic replies + T5B-VOICE 596: the cast voices + T5B- |
| T6 | the drama manager, outcomes, the talk memory chunk | **memory and chunk done** (T6-MEM, 6e); **drama manager done** (T6-DM, 6f: `talk_dm.c`, `beats.tlk`); **outcomes into the sim done** (T6-OUT, 6g: `talk_outcome.c`) |
| **T7** | the census (`core/tools/talk_census.c`: the three tiers, Chao1, the quality checks Q1-Q6, the conversation tier), the grid `census_en.tsv`, `make -C core talk-census`, the smoke census in `make test`, the Q7 review sheet `talk_sample.py` | **done** (TK3): the tools and the measurement. The full run's numbers are in 6d: Skeleton, Meaning, conversations and quality meet the design's targets; **Surface does not** (it needs T5b's bank); Q7 (a person rating 300 exchanges) has not been done |
| T8 | the house mode `HM_TALK` on native, with its UI | to do |
| T9, T10 | pointer input; other languages (after the script rewrite) | to do |
| TP0-TP4, TD0-TD3 | the PS1 and Dreamcast ports | to do (TD0's prerequisites are in: DC3, DC4, DC8 with zoom 2 and `plat_room_project`, and the Flycast core, approved and built) |

Nothing in the game can reach Talk yet. `home_talk_enter()` is a weak
default that answers "not available", and no platform sets `PLATF_TALK`.
T5a generates lines, but **only the unit tests and the census call it**:
the house mode (T8) is what makes it reachable in play. Every line the
generator writes has already gone through the T4 gate (the player's
utterance) and the S3 output lint (the line), and the property tests over
generated lines (P1-P8, section 2) are green.

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
| **The template lints TK1-TK12** refuse a bank before it is built (`talkc --check`): a topic word followed by a literal apostrophe (TK12: write `{x:s}`), an adult cool-act reply that turns on the listener without `!kid_present` or the reviewed `gentle` tag (TK10), a romance decline whose text ends the relationship (TK11), a register word without `!kid_present`, a capped swear, a romance-reachable rule without `adults_only&!kid_present`, a `jmat` without `mature_adult_ok`, any sexual-class word, a safety or ungated romance slot, a register word or the LOVE face in a kid's bank, a refusal bank without its reviewed `nojoke` tag, a corpus that does not parse, a language with free text but no safety lexicons. 18 seeded bad templates and rule files must each fail with their rule | `talkc_safe.py`, `talkc.py`; `core/tests/talk_bad/*.tlk`, `*.rules`, `core/tests/talk_good.tlk` |
| **Every word is read**: a line with more words than the parser's 40-token cap is cut; the bubble ends at the cut (nothing unread is ever shown) and S1 reads the rest of the line in overlapping parts, so a K-7, hate, distress or swear word after token 40 is still caught. Every must-refuse corpus line padded with 41 junk tokens (1,242 lines) is refused in its class | `talk_parse` (`TalkParseInfo.cut`); `talk_safe_long_lines` |
| **The parser hands on only acts and concepts that may be generated**: 50,000 random lines of junk, swears and safety words never give a topic that is never-generated or of a sexual / hate / real-world class. Typed words stay in the (masked) bubble only; the residents can never be made to say them | `talk_safe_property_closed_vocabulary` |
| **The gate changes nothing**: after the 200,000 calls `sim_romance_sweep(-1) == 0` and no edge was made | `talk_safe_property_gate` |
| No slur is written anywhere in the repository: `safety/hate.hash` holds salted FNV-1a 64 hashes (and their one-letter deletions), made from the reviewer's local list by `talkc.py --hate-hash` | `talkc_safe.py`; `talk_safe_hate_hash` (and `DOOMO_HATE_PLAIN=file` checks every word of the local list) |

**The rule for T5a and later stays:** every generated line goes through
`talk_safe_gate` (for what the player said) and `talk_safe_lint` (for what
a resident says) before anyone sees it; a lint hit replaces the line with a
safe bank line and counts (`talk_safe_lint_hits()`, a test fails on any).

What T5a adds (section 6c), each with its test:

| Guarantee | Where it is enforced |
|---|---|
| **The gate comes first.** `talk_gen_exchange()` runs `talk_safe_gate()` on the player's utterance before anything is written. A K-7 input gets one or two short adult lines from `end.k7` (no joke; a child alone says a kid's line), every line carries `TLF_END`, and the conversation is over (`c->ended`); refusals, hate, distress and deflections get one sincere line and no reactions | `talk_gen.c` (`exchange`); `talk_gen_exchange_gate`, `talk_gen_property_rooms` (P3) |
| **Every line is linted.** `talk_safe_lint()` runs on every line with the room's audience and the conversation's swear count; a hit plays a `safe` line, counts, and logs `talk: lint` (design 7.2). Over the 20,000 property exchanges, the census and every test: **0 hits** | `gen_line`; every test checks `talk_safe_lint_hits()` |
| **Nothing the room may not hear is filled in.** A topic, an association or an attribute is used only when `talk_gen_concept_ok()` passes (never-generated, sexual, hate and real-world classes never; adults-only words, the romance root and register words never with a child here); a filled-in word the lint would refuse (`spice's` squeezes to a hate-list plural) is probed first; a name the player typed is probed strictly (no register word in any room) and replaced by "pal" or the line is re-picked | `fill_concept`, `fill_name` (`talk_safe_lint_probe`); `talk_gen_custom_name` |
| **Romance only where the gate let it through, and only with the target's yes.** Every romance rule carries `adults_only&!kid_present&!pet` (TK3); `consent` is the target's answer, which T6 and the sim decide (Talk never sets romance state); a no marks the target refused for the day (K-5) and the next romance act is a `DEFLECT_ROMANCE` | `core_romance.tlk`; `talk_gen_exchange_romance_consent`; P2, P7 |
| **No child, no romance, no register word, no LOVE face, ever** (P1): 20,000 random exchanges (every act, the romance and safety ones included, random safety bits, Mature on a third of the time, 1-3 residents of whom 40% children, a minor avatar one time in five): every line in a room with a child lints clean under the strict probe, has no LOVE face, no romance act and no rule reachable from a romance act; every romance or sexual act near a child is answered by a refusal; nobody under 18 speaks a romance line and none is said to one (P2); the pet never hears a sexual-class word (P4); a Mature (`jmat`) variant only with the option on and every participant a premade adult (P5); after the run `sim_romance_sweep(-1) == 0` and no edge was made (P7). `DOOMO_TALK_PROPERTY_N=200000` runs the design's size (done once for TK3: green, see the evidence) | `test_talk_census.c` `talk_gen_property_rooms` |
| **Closed vocabulary** (P6): 4,000 typed lines of junk, swears and safety words through the parser and the exchange: nothing typed comes back out | `talk_gen_property_vocabulary_and_swears` |
| **One everyday swear a conversation, from a blunt adult** (K-9, P8): an everyday swear needs `blunt&adults_only&!kid_present` (talkc refuses it otherwise), the generator skips swear lines once the conversation has had one, and a catchphrase with a swear counts too; 600 conversations of blunt adults trading insults: at most one each, the capped words never | `talkc_gen.py`; `pick`; `talk_gen_swear_cap`, P8 |
| **Callbacks are true** (Q4): a `cb.*` line must stand on a fact predicate (talkc refuses one that does not), and the fact is read from the sim's accessors when the line is made; the census re-reads them for every callback line | `talkc_gen.py`; `talk_gen_fact`; `talk_gen_parts_and_callbacks`, the census |
| **The safety banks** (`grammar/en/safety.tlk`): no topic slot (no echo), no register word, no catchphrase, no LOVE face, the reviewed `nojoke` tag; `distress` and `end` banks are refusal banks for TK7 too; the `safe` fallback has no guard, no filler and no register word, so it plays in any room | `talkc_gen.py`, `talkc_safe.py`; 15 seeded bad grammars |
| **Grief is not a punchline** (K-10, SCRIPT_STYLE R6). A grief or loss topic (the lexicon's `grief` flag, `TCF_GRIEF`: grief, mourning, condolences, loss, memorial, funeral) takes the sincere `grief` bank (`grammar/en/grief.tlk`) **whatever the player's act** (the safety acts keep their own banks, which are sincere already): the core line alone, no opener, callback, tag or hand-off, and a reply act the adjacency table allows after the player's act (a joke or a tease about it gets a quiet "not about this", never a laugh). One other resident may add a sincere `comfort` line; the pet stays quiet. A grief word is never filled into any other line (an association of another topic, the talk-memory callback). A household member who lost someone in the last 28 town days (`fact.loss`, read from `family_memorial*`; a move away is not mourned as a death) may name them (`{lost}`). talkc refuses a grief or comfort rule without its `act:` or the reviewed `nojoke` tag, a register word, sub-rule, catchphrase, LOVE face or `[JMAT]` in one, `{lost}` anywhere else, and a full bank that leaves any non-safety act without a grief line that needs no guard but the act's own. Death, ghosts and graves stay the game's jokes | `talk_gen.c` (`grief_line`, `fill_concept`, `assoc_pick`, `loss_of`), `talkc_gen.py`; `talk_gen_grief` (the engine), `talk_gen_property_grief` (every act x the 6 topics x adults', kids' and pet rooms, 4 seeds each: every line sincere and core-only, at most one comfort line, the household loss named, ghost / graveyard / zombie still joked about, a remembered grief topic never in a joke), K-10 in `talk_gen_property_rooms` and the census check `QF_K10_GRIEF`; 5 seeded bad grammars and a bad lexicon flag |

---------------------------------------------------------------------------

## 3. The contract (T0)

| Where | What |
|---|---|
| `core/include/doomo/talk.h` | `TA_*` discourse acts. Their ids are fixed (they are saved in talk memory), and `test_contracts_talk.c` locks them. There are 59 acts plus 5 gate classes, including the gesture `TA_HIGH_FIVE` and the gate rewrite `TA_DEFLECT_ROMANCE`. Also: `TONE_*`; the `TS_*` input safety bits; `TalkUtter` (106 B: the one record every input path produces); `TalkAudience`; the talk memory sizes; the `TALK_<lang>.BIN` format; the `talk_db_*` reader API; `home_talk_enter()` |
| `plat_ext.h` | `PLATF_TALK` (bit 9). `plat_talk_load(code)` returns the file size, 0, or `PLAT_TALK_LOADING` while the PS1 is still staging it. `plat_talk_read(off, dst, len)`. `plat_room_project(view, x, y, z, &sx, &sy)` |
| `platform.h` | `DoomoRoomView.zoom == 2` is "talk framing", with `focus_x`, `focus_y` and `focus_r` (tile space, 16.16). Renderers that do not know zoom 2 treat it as the close-up (the PS1 already does, and `room_ref` ignores zoom) |
| `save_ext.h`, `save_budget.h` | The `'TALK'` chunk: slot 13, registered by `talk_save.c` where the build defines `DOOMO_HAS_TALK` (the native host), weak NULL elsewhere, `SAVE_EXT_MAX_TALK` 320 B (292 B worst case). The joint worst case with it fits both slots: a post-game story save at its worst is 29,139 B, plus the 2,048 B margin is 31,187 B, under the 32,768 B gate (36 KB minus PA10's 4 KB); free play 68,945 B of 90,112 B |
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

## 6c. The generator (T5a)

`core/src/talk_gen.c`, every Talk build (PS1, Dreamcast, native). Design
3.6. It writes only from the database: template strings chosen by the
grammar and concept forms. The player's words never come back out.

```c
TalkConv c;      TalkUtter u;      TalkExchange x;
int room[2] = { NANA, VEX };
talk_gen_begin(&c, DOOMO_PLAYER_ID, room, 2, talk_gen_seed(DOOMO_PLAYER_ID, room, 2));
talk_parse("Nana, what's in the pie?", &pctx, &u, NULL);     /* or the PS1 composer */
talk_gen_exchange(&c, &u, &x, NULL);    /* the gate, then 1-4 lines: x.line[k].who / .text / .face */
```

**A line** has up to five parts (design 3.6), each a pick from the grammar:

```
 [opener] [core] [callback] [tag] [hand-off]
 "Ooh-"   "Pie? I love it. Ask again and I'll say it louder."  "Like at work. I'm a baker."  "Sparkle sparkle!"  "Right, Vex?"
```

* **The core** answers the act replied to: the rules named `core.<act>` (or
  `refuse.sexual`, `refuse.minor`, `refuse.hate`, `distress`,
  `deflect.romance` for what the gate made of it; `react.<act>` for a
  reaction between residents; `pet` for the Boreagotchi, which only says
  "beep."). Its rule's `act:` is the reply act, which `acts.tsv` must allow
  (talkc refuses one it does not: Q2 by construction).
* **Grief and loss** (K-10, section 2): a topic with the lexicon's `grief`
  flag takes the rules `grief` / `grief.*` instead of `core.<act>`,
  whatever the act (not the safety acts): only the rules whose `act:` may
  answer the player's act, the core part alone (`TLF_SINCERE`). A reaction
  to it is a `comfort` line (agree or backchannel) or nothing.
* **The opener** (30%, more for a loud speaker), **the callback** (35%),
  **the tag** (40%: the catchphrase once a conversation, a persona tic or a
  button) and **the hand-off** (25%, to another participant) are optional.
* **The fit**: a line must fit the bubble (3 x `TALK_BUBBLE_COLS`) and have
  1-3 sentences (`TALK_SENTENCES_MAX`; a fragment counts, "Dr." does not).
  The generator re-picks up to 4 times, sets aside a core that cannot fit
  alone, then drops the tag, the callback, the opener and the hand-off;
  when nothing fits it plays a `safe` line (`TLF_FALLBACK`, a Q1 failure:
  the census counts it).
* **An alternative whose filling fails** (a topic the room may not hear, a
  name the lint refuses, an attribute the concept does not have) is set
  aside and another picked, in parts and in sub-rules.
* **Selection**: weighted random over the alternatives whose guards hold.
  A line with a `speaker=` guard (the cast's own line) weighs 8 times as
  much. **Novelty**: an alternative this speaker said in their last 6 lines
  (`TALK_NOVELTY`), or anyone in the room in the last 16
  (`TALK_ROOM_NOVELTY`), is taken only when nothing fresher is possible
  (Q5: no line twice in a conversation).
* **The RNG** is a local `DoomoRng` seeded from the conversation seed (the
  town seed, the day, the participants: `talk_gen_seed`) and the line
  count, so the same talk gives the same text on every platform.
* **Stance** on the topic (`talk_gen_stance`): the concept's sentiment, +3
  for the speaker's liked food, hobby and music, -3 for their fear and
  hated food, holy things flipped for the Hellvile-born, plus a fixed
  personal lean from a hash of the speaker and the topic.
* **The voice**: guards on the persona sliders (`polite`, `blunt`, `flat`,
  `loud`, `serious`, `easygoing`, `quirky`, `quick`, `slow`), the persona
  type (`type=`), the cast (`speaker=`), the mood band, the relationship to
  the one addressed (`rel=`), the life stage (`kid_speaker`, `teen`,
  `elder`); the exclamation budget of `PERS_EXPRESSION` (1, 2 or 3 a line:
  extra ones become full stops); `tags: lower` keeps a lower-case voice
  (Wisp, Lord Vapula) as written.
* **Callbacks** stand on a fact read from the sim when the line is made
  (`talk_gen_fact`): the roster job, the partner, a pet, this week's news
  about them, their problem (`prob=`), a birthday within a week, ghost,
  possessed, Hellvile-born, night, talk memory, a like or a fear on topic,
  the household, the story chapter. Story callbacks are gated by chapter
  (no spoilers).
* **The exchange** (`talk_gen_exchange`) is a plain stand-in for T6's drama
  manager: the gate; the one addressed answers, or the most interested
  (likes the topic, likes the player, a little noise); each other
  participant may react to the line before (design 5.4: 25% + 4% x
  EXPRESSION + 2% x QUIRK, at most 75%; two reactions at most, four lines
  at most) with an act the adjacency table allows after it (agree,
  disagree, laugh, tease, joke, a backchannel) chosen by stance clash and
  persona. Refusals, distress, deflections and K-7 get no reactions; after
  a grief line (K-10) one sincere `comfort` line at most, never the pet; a
  romance act gets its target's answer only (`TalkConv.consent`, which T6
  and the sim decide; a no marks the target refused today).
* **What it costs**: no static RAM; `TalkConv` 128 B (the conversation,
  T6 keeps it), `TalkLine` 104 B, `TalkExchange` 420 B. Stack, measured
  with `-fstack-usage` on MIPS (`-Os -march=r3000`): `gen_line` 1,480 B
  (the five part buffers), `talk_gen_line` 456 B (the per-line context),
  `exchange` 632 B, 264 B per template nesting level (at most 5) and the
  S3 lint's 440 + 304 B: about 5 KB in the worst case. Everything is read
  through `talk_db_read` (SPU RAM on the PS1; section 7 has the count).
* **One exchange runs synchronously** (`talk_gen_exchange`). The design's
  risk note (spread the work over frames, test with the scratch poisoned
  between frames) is T6's and TP2's: `talk_gen_line` makes one line and
  `TalkConv` (128 B) holds everything between lines, so the drama manager
  can step one line a frame with it as the cursor; nothing here keeps a
  pointer into scratch between calls.

**The grammar** (`core/data/talk/grammar/<lang>/` and `voice/<lang>/`,
compiled by `core/tools/talkc_gen.py`, which documents the syntax and the
bank format):

```
@rule core.ask_about.pro guard: stance=pro act: opine_pos
  "{x:cap}? I love {x.it}. Ask again and I'll say it louder."
  "Big fan of {x}. Huge. I'd sign {x.it}." w=2
@rule cb.job guard: fact.job
  "Like at work. I'm {job:indef}. Don't ask."
@rule core.greet.grumb guard: speaker=grumbleguts&adults_only&!kid_present act: greet
  "Well, hello, darlin'. Come on in. Mind the belly."
```

* **Fillers**: `{x}` the topic (`.adj`, `.good`, `.bad`, `.assoc`, `.it`,
  `.is`, `.has`, `.does`, `.was`; `:pl`, `:indef`, `:def`, `:lemma`,
  `:cap`, `:up`, `:s`), the speaker's `{food}`, `{hobby}`, `{music}`,
  `{fear}`, `{hate}`, `{like}`, `{job}`, `{mem}`; the names `{you}`, `{me}`,
  `{them}`, `{other}`, `{partner}`, `{petname}`, `{lost}` (K-10: the one the
  speaker lost lately; grief and comfort banks only); `{catch}` the catchphrase;
  `{rule.name}` a sub-rule; the story's line codes `{.}`, `{..}`, `{r}`...;
  a face `[HAPPY]`, `[LOVE]`... Fillers add their own guards (`{x}` needs a
  topic, `{them}` a resident addressed, `{job}` a job).
* **Guards**: the predicates of `talk.h` `TGP_*` (the room, the speaker,
  the line, the facts (`fact.loss`: K-10), the topic's forms and
  `x.grief`) and their values (`stance=`,
  `speaker=`, `type=`, `mood=`, `rel=`, `root=`, `chapter=`, `time=`,
  `beat=`, `tone=`, `pact=`, `act=`, `prob=`).
* **What talkc refuses** (besides TK1-TK9): an unknown predicate, value,
  slot, field, modifier or rule; a reply act `acts.tsv` does not allow; "a
  {x}" written by hand (use `{x:indef}`); a line that cannot fit the bubble
  with the shortest fillers, or has more than 3 sentences; an everyday
  swear without `blunt&adults_only&!kid_present`; a partner line without
  `adults_only&!kid_present`; a callback that stands on no fact; a word the
  output lint's hate list refuses; a topic slot or a register word in a
  refusal or the pet's bank; a `safe` line with a guard or a filler;
  sub-rules that call themselves or nest deeper than 4; a character the
  font or the story codec cannot do; K-10: a `grief` or `comfort` rule with
  no `act:` (comfort: agree or backchannel) or no `nojoke` tag, a register
  word, sub-rule, catchphrase, LOVE face or `[JMAT]` in one, `{lost}`
  outside them, and a full bank where some act that is not a safety act has
  no grief line it may get with no guard but its own (`pact=` / `act=`).
  20 seeded bad grammars (`core/tests/talk_bad/gen_*.tlk`) must each fail
  with their message.
* **The banks**: `TB_GRAMMAR` (rules, alternatives, guard terms, symbols,
  the entry table, the acts table, the likes / catchphrase / cast / job
  maps) and `TB_STRINGS` (the template texts, the story's Huffman code).
  `TB_VOICE` stays unused: the voice rules are ordinary rules.

**The starter bank** (TK3; T5b grows it to the design's ~6,000): 718
alternatives in 373 rules over 12 files: every act has replies (64 entry
points, the romance and safety ones included), openers by act class, mood
and voice, callbacks for every fact kind, tags and 16 persona tics, hand-
offs, six reaction kinds, kid lines, the refusal / distress / K-7 / safe /
pet banks, the sincere grief and comfort banks (K-10: 50 lines with kid,
elder, loss and cast variants), and 17 of the cast in their SCRIPT_STYLE section 7 voices
(Mayor Giggs' tea-party patter, Nana's "sweet pea", Bonejangles' servo and
Bandstand Tapes, Pip's patrol, Father Fuzz's hugs, Dr. Bloom's dry button,
Hattie's set, Vex's "- V", Grumbleguts' "Needs salt." and "Open late,
darlin'.", Wisp's lower case, Mother Nox's soup, Clinker's numbers,
Vapula's weather). PG-21 for adults only where the guards allow it,
kid-safe everywhere else; no `jmat` (Mature) line: Talk never pushes a
mature slot itself. **The distress lines (K-11) await the owner's approval
of their wording (design D6); the grief lines (K-10) are the agent's too
and go to the owner with the Q7 sheet.**

**T5b-ASK: questions and answers (+975 alternatives).** Three new files
in `core/data/talk/grammar/en/`: `ask_topic.tlk` (360: replies to
ASK_ABOUT and ASK_OPINION by stance, by what the topic is, by voice and by
persona type, with kid lines and the shared `ask.star.*` / `ask.more` /
`ask.less` sub-rules), `ask_feel.tlk` (194: ASK_FEELING by mood, voice, type,
plus day, plans, past and family stories) and `ask_answer.tlk` (421: ASK_FACT,
ASK_WHO, ASK_ADVICE, ASK_FAVOR and ASK_REPEAT: honest, evasive and silly
answers). Same rule names with a `2` / `.voice` / `.type` suffix as the
starter's, so nothing in `core_ask.tlk` changed. Kid-safe everywhere; the
only adults-only guards are the two `blunt&adults_only&!kid_present` swears
in `ask_topic.tlk`. No romance. `TALK_en.BIN` 540,274 -> 582,443 B (about 43
B an alternative; the PS1 subset grows by the same rate, so TKP's lever
choice matters more now). No resident code changed.

**T5B-TOPIC: topics from the word network and more beat lines (+771
alternatives).** Sixteen new files `grammar/en/topic_<root>.tlk` (+654) give
every talkable root of the category tree its own replies, so a word the
player names gets an answer about that kind of thing and not the generic
one: `topic_people.tlk`, `topic_places.tlk`, `topic_food.tlk`,
`topic_music.tlk`, `topic_hobbies.tlk`, `topic_fears.tlk`,
`topic_creatures.tlk`, `topic_house.tlk`, `topic_lore.tlk`,
`topic_hellvile.tlk`, `topic_feelings.tlk`, `topic_work.tlk`,
`topic_time.tlk`, `topic_school.tlk`, `topic_stuff.tlk` and
`topic_describe.tlk` (the network has 406 topic concepts with mostly
generic replies; `ask_topic.tlk` had 2-5 lines a root). Each file follows
the same shape, all guarded `root=<root>`: ASK_ABOUT by stance (pro, con,
neutral) and a joke rule, ASK_OPINION, TELL by stance, and the four opinion
reactions (`core.opine_pos.*_agree` / `_disagree`, `core.opine_neg.*_agree`
/ `_disagree`), so a stance clash between two residents on a food word now
reads differently from one on a place or a job. `topic_fears.tlk` is the
exception: fear answers stay kind (no laughing at the player's fear, the
joke rule carries `!kid_speaker`), and `topic_feelings.tlk` keeps the
comfort register of K-10's neighbours without touching the grief bank.
New rule names only, so nothing in `core_ask.tlk` / `ask_topic.tlk`
changed. Kid-safe everywhere: no register word, no romance, no `jmat`, at
most 3 sentences a line.
`grammar/en/beats_more.tlk` (+117) answers T6-DM's repeats: a `.more` rule
beside every drama-manager rule of `beats_lines.tlk` (the 13 beats' goal
lines, idle chatter, the prod, the walk-out excuse, the grumble, jab, row,
ultimatum, makeup and the child's "are you fighting?"), with the same
guards and acts; `beats_lines.tlk` is untouched. The romance beat's extra
lines keep `consent&adults_only&!kid_present&!pet`, the row and the
ultimatum keep `!kid_present`, the kid lines keep `kid_speaker` and the
`kid` tag. A long session now draws from 2-3 times as many beat lines
(e.g. `beat.smalltalk` 4 -> 11, `excuse` 5 -> 10, `prod` 4 -> 8).
`TALK_en.BIN` 643,516 -> 676,924 B, the PS1 subset 312,185 -> 345,634 B of
the 312 KB budget (**the PS1 subset is now over that budget: TKP must pick
its lever, see 6.4 and TP1**). Data only: no code, no resident byte on any
platform. A 40-line sample is in the evidence directory.

**Writing a line**: add it to the right rule in `grammar/en/*.tlk` (or a new
rule with a guard and an `act:`), run `make -C core talk-check talk`, then
`core/build/talk_census --smoke --grid core/data/talk/census_en.tsv` and
read a few with `--sample FILE --every 50`.

**Tools**: `DOOMO_TALK_LOG=1 core/build/talk_census ...` prints any S3 lint
hit (`talk: lint N: line`); `--fails FILE` writes every exchange that fails
a quality check, with the reason.

---------------------------------------------------------------------------

## 6d. The census and the review sheet (T7)

`core/tools/talk_census.c` (a host program linking the core), the grid
`core/data/talk/census_en.tsv`, and `core/tools/talk_sample.py`. Design 3.7.

**The grid** (fixed, so the numbers are reproducible): 32 residents (the
16 persona types across both districts, 4 of the story cast, 20 adults, 4
elders, 4 teens, 4 children; `talk_census --pick` made the list), 400
topics taken in turn from every talkable category plus the 6 grief and loss
topics (K-10, always in), the 55 player acts that
are neither romance nor safety (the property tests cover those), and per
context a mood band (7), a relationship band to the player (6), the
addressee (a resident 70%, everyone 30%), a talk-memory state (4), the time
of day (3) and the story chapter (3). The census sets that state in a
premade town and generates one exchange per context with the real gate,
generator and lint.

**Counting** (only exchanges that pass Q1-Q6 count):

| Tier | Normalisation |
|---|---|
| Surface | the lines with a role prefix (`R1:`, `R2:`, not the speaker), lower case, punctuation and line codes out, names `<n>`, numbers `<#>` |
| Skeleton | as Surface, every filler replaced by its category root (`<food>`, `<work>`, `<text>` for a catchphrase) |
| Meaning | per line (reply act, stance, topic root, callback kind, beat, responders, role) |

FNV-1a 64 hashes in open-addressing sets with counts, the Chao1 estimate
of the total reachable (`S + f1^2 / 2 f2`), the failure histogram by
reason. **The conversation tier**: bot conversations of 6 player turns (a
GREET, then acts the adjacency table allows after the last line, topics
from the current one's associations or the grid); two are distinct when
their Surface hashes differ in at least 2 of the 6 (kept greedily: a
conversation counts when none kept shares any of its six leave-one-out
keys); Q5 (no line twice in a conversation) is counted there.

**The quality checks** (`talk_census_check`, also used by the property
tests): Q1 well-formed (no stray `{` or `}`, spacing, a capital letter,
end punctuation, a/an agreement, the bubble, 1-3 sentences, no fallback);
Q2 the reply act legal after the act before it (`acts.tsv`) and the topic
the player's, a 1-hop association of it, or (CHANGE_TOPIC) the speaker's
like; Q3 every voice guard of every alternative used re-checked against the
resident's own data, the exclamation budget, a tic from their own type; Q4
every callback re-read from the sim; Q5 above; Q6 the S3 lint (and the
strict probe, no LOVE face, no romance rule in a kid room, no Mature
variant without the option and the adults); K-10 (`QF_K10_GRIEF`): a grief
topic answered with anything but a sincere core line (an opener, tag,
callback or hand-off, a joke, laugh, tease or mock act), or a grief word in
a line that is not sincere. Q7 is the human read.

```
make -C core talk-census                    # the full run (exit 1 if a target is missed)
core/build/talk_census --smoke --grid core/data/talk/census_en.tsv
python3 core/tools/talk_sample.py draw --n 300 --out review.tsv    # Q7: rate, then
python3 core/tools/talk_sample.py score review.tsv                 # mean >= 3.5, <= 2% rated 1
```

**The full run** (the TK3 landing: the starter bank with K-10's grief and
comfort banks and the 6 grief topics in the grid; 2,000,000 contexts and
2,000,000 conversations, seed 20261108; 303 s for the contexts and 947 s
for the conversations, 24 minutes on the shared 4-CPU build machine (31 in
an earlier, busier run); log in the TK3 evidence):

| Measure | Full run | Target (design 3.7) | |
|---|---|---|:-:|
| Contexts | 2,000,000 (856,770 with a child in the room), 3,392,714 lines | | |
| Quality failures (Q1-Q4, Q6, K-10) | **0** | 1% or less | met |
| S3 lint hits | **0** | 0 | met |
| Surface | 438,891 distinct (21.9% unique), Chao1 1,522,818 | 1,000,000 distinct | **missed** |
| Skeleton | 360,151 distinct (18.0% unique), Chao1 997,117 | 100,000 | met |
| Meaning | 42,036 distinct (2.1% unique), Chao1 67,009 | 5,000 | met |
| Conversations | 1,999,886 of 2,000,000 distinct (differ in 2 or more of 6 exchanges) | 1,000,000 | met |
| Q5 (a line twice in one conversation) | 9,683 conversations (0.48%) | inside the 1% failure bar | met |

`make -C core talk-census` therefore exits 1 ("a target was missed"):
Surface. Chao1 estimates about 1.5 million Surface variants reachable
from the starter bank, but 2,000,000 draws see only 438,891 of them (the
run before K-10 saw 443,391: the 50 sincere grief lines have few fillers,
so a grief topic repeats more; that is the point of them): the
same lines come up too often (most residents share the generic
alternatives; a short reaction or a safe line is the same text in every
room). That is what T5b's ~6,000 alternatives are for; the generator does
not need to change for it. Per line, about 80 us on the host with the
census checks.

**The smoke census** (`talk_census_smoke` in `make -C core test`: 20,000
contexts and 1,000 conversations, about 4 s): quality failures 1% or less,
no safety failure and no lint hit (the design's bar), and floors that fail
early when a bank collapses: Surface 50% unique, Skeleton 30% (the
design's), Meaning 4,000 distinct, conversations 99% distinct, Q5 repeats
2% or less. The design's smoke bar of 90% Surface-unique at 100,000
contexts belongs to T5b's full bank; the starter bank makes about 57%.

**Q7**: `talk_sample.py draw` writes a sheet of 300 exchanges spread over
the grid (what the player did, about what, who is in the room, the lines,
an empty rating), with each line's template (file:line) in a last column
for the writer; `score` reports the mean, the share of 1s, PASS or FAIL and
the templates of every line rated 1 or 2. **Not done yet**: a sheet for
the owner is in the TK3 evidence (`q7_review_sheet_k10_seed7.tsv`, drawn
after K-10, with 5 grief exchanges); the review is T5b's release gate.

---------------------------------------------------------------------------

## 6e. Talk memory and the TALK chunk (T6-MEM)

`core/src/talk_save.c`: what a resident remembers of the player's talks.
The table holds at most 24 entries of 12 bytes, most recently used first;
a 25th person evicts the last one. With a 4-byte header that is 292 bytes of
`.bss` (`TALK_MEM_WORST`). Each entry has the person id, the town day of
the last talk, the last two topics, the flags (complimented, insulted,
promised, refused today) and the likes the player has learned.

* **Chunk.** Tag `'TALK'` (slot 13, already reserved by T0), version 1, the
  same bytes written little endian. An empty table is a chunk of 0 bytes.
  A save with no chunk (an old save) loads with an empty table. A chunk that
  `read()` refuses (unknown version, truncated, too long, count over 24)
  also leaves it empty. No other tag or layout changed.
* **Who may have an entry.** A resident or the hero on the island, or a
  family pool member. The pet, anyone who left and a second entry of the
  same id are dropped on load and on lookup.
* **Refused today (K-5).** Cleared when the town day changes; every public
  call checks first, so a save loaded on a later day forgets it too. The
  chunk callbacks never roll the day.
* **Last time stays true.** The flags of a running conversation wait in the
  high nibble (never saved) until `talk_mem_end()`, so a "you were nice to
  me last time" line can never speak of the conversation it is said in.
* **Retired topics.** Topic ids are kept as they are; one the open database
  no longer has is skipped when a conversation begins.
* **Where it lives.** The save registry is link-time, so the descriptor is
  compiled only with `-DDOOMO_HAS_TALK` (the native host in `core/Makefile`;
  a console port adds it when it gets Talk). On the GBA, PS1 and Dreamcast
  nothing references the file and `--gc-sections` drops it: 0 bytes
  resident there, so GBA EWRAM and IWRAM and the PS1 EXE are unchanged.
  On native the 292 bytes are `.bss`.
* **API.** `talk_mem_touch / find / note_topic / set_flags / clear_flags /
  know_like / like_known / refused_today / begin / end / forget / reset`
  in `talk.h`. T6-DM calls `talk_mem_begin` and `talk_mem_end` around a
  conversation.

---------------------------------------------------------------------------

## 6f. The drama manager (T6-DM)

`core/src/talk_dm.c`, the table `core/data/talk/beats.tlk` (compiled by
`talkc_beats.py` into the `TB_BEATS` bank), the lines
`grammar/en/beats_lines.tlk`, tests `core/tests/test_talk_dm.c` (17 tests).
It is the **policy** over the generator. Lines are still made by
`talk_gen_line`, `talk_gen_react` and the new `talk_gen_entry` (a fixed
grammar entry: a beat's goal line, idle chatter, the prod, an excuse, a
grumble, the three argument steps, making up, a child's question). The gate
runs first on every player turn; the S3 lint runs on every line.

* **Use.** `talk_dm_begin`, then per player turn `talk_dm_say` (the gate, the
  engagement, the beat, the plan of the exchange) and `talk_dm_step` until it
  returns 0 (one line a call, so the PS1 can spread an exchange over frames);
  `talk_dm_tick(frames)` for idle chatter; `talk_dm_end` fills a
  `TalkSummary` for T6-OUT. Everything between calls is in `TalkState`
  (940 B, limit 1,536 B, the house's scene scratch); no static RAM; no pointer
  into scratch survives a call. The random stream of a call is seeded from the
  conversation seed, the turn and the exchange: the same text on every
  platform.
* **Beats** (`beats.tlk`, 13 beats as design 5.2): precondition, tension on
  entry (`zero` for WIND_DOWN) and after, goals, turns, bonus, who it involves,
  the act classes it handles (a handled act counts as a goal, but one goal line
  is always said), what hurts and what resolves it, what the player leans on.
  The next beat is the one whose resulting tension is nearest the target for the
  turn (`talk_dm_target_tension`: 0 up to 6 at turn 9, down to 0 at 15, then 2),
  plus the precondition bonus, the lean, minus 3 for a beat of the last two talks
  with these people and 5 for one already played. Tension also moves with the
  player's acts (an insult +1) and drifts down (a warm act, every third turn).
* **Engagement** (5.3): 55 at the start; each turn `+6` addressed, the affinity
  of the topic (`+3` a like, `-3` a hate or fear, `+1` / `-1` a root the persona
  type favours or finds dull: `@affinity` in `beats.tlk`), the reward of the act
  for the persona, `+2` when the beat involves them, `-2` decay, `-4` a repeated
  act, `-3` after three turns ignored. Under 25 they turn away (`TPF_AWAY`);
  under 15 for two turns an excuse and a walk-out (`TEV_WALKOUT`); over 85 they
  lean in (`TPF_LEAN`). When everyone has left the talk is over.
* **Turn-taking** (5.4): the addressee answers, else the best of engagement + 20
  x affinity + bond + noise. Reactions use the design's 25% + 4% EXPRESSION + 2%
  QUIRK (+15% on a stance clash, the second reaction half as likely, 75% at
  most); at most 4 lines an exchange, the mandatory ones (the argument's step,
  the beat's goal line, a walk-out) first, reactions in what room is left and
  said right after the answer. An interruption: SPEECH 11 or more, a stance
  clash or a rival speaking, engagement 50 or more, both adults, at most once in
  3 exchanges; the line before is cut at a word with a dash (`TLF_CUT`), the
  interrupter reacts, the interrupted one grumbles. Idle chatter: after 8 s
  (240 frames) a resident talks to another, twice, then someone prods the
  player, then nothing until the player speaks.
* **Alliances and arguments** (5.4): TAKE_SIDE or DEFEND of Y raises the
  session bond with Y by 3 and lowers it with Y's opponents (Heider: a friend of
  Y rises a little); caps `TALK_REL_D_MAX` 12 and `TALK_PAIR_D_MAX` 8 a session.
  RIVALRY (two adult rivals, REL_RIVAL / NEMESIS or bond under -30) runs jab,
  row, ultimatum (the row needs tension 6 or more). MEDIATE rolls on both
  ATTITUDEs and the bond: `TEV_MEDIATED` (T6-OUT calls `sim_make_up(a, b)`;
  the test calls it on the event); TAKE_SIDE `TEV_SIDE_TAKEN`; ignored it
  fizzles with a walk-out of one of the two. With anyone here who is not an
  adult it stops at the jab, and a child asks "Are you guys fighting?" and the
  room cools.
* **Safety, in C whatever `beats.tlk` says.** ROMANCE_ADULT needs the player's
  own romance act this turn, the target's yes (`talk_dm_consent`: an adult
  room, `talk_safe_romance_ok`, `sim_romance_ok` from the target, the bond),
  and no minor, pet, kid cast or minor avatar in the room; its line is said only
  by that adult. One filter drops every other drama-manager line that is a
  romance act. The room's audience only gets stricter during a talk. The gate's
  refusals, deflections, hate and distress get the answer alone (no beat line, no
  reaction); K-7 gives one or two adult lines and ends the talk. Romance state is
  never written here: events go to `talk_dm_end` for T6-OUT.
* **Tests** (`test_talk_dm.c`): the table and the state budget, the arc (30
  sessions peak in turns 3-13 and end under the peak), a scripted session and
  determinism, idle chatter and the prod, walk-out and the end of the talk, the
  argument steps (rows and ultimatums reached; fizzles), mediation calling
  `sim_make_up` (61 of 120 sessions made up, with failed rolls), a child holding
  an argument at the jab, interruptions (86 cuts, never closer than 3 exchanges),
  alliances and their caps, a problem talked through and a great night, the
  romance beat (yes, no and the K-5 deflection, with the pet or a child never),
  P2 in both directions, P3 (K-7), the gate's stand-alone answers, and P2 / P7
  over 400 random sessions (no romance line in a minor's room, no couple, crush or
  romance-sweep hit made).
* **Not here** (other packages): applying outcomes to the sim (done in 6g), the
  beat lines at full size (T5b: about 5 a beat now), the PS1 build (TP2: the
  object is 22 KB on x86 at -O2; the MIPS size and the stack use of `talk_dm_say`
  (288 B on x86) are to be measured).

## 6g. Outcomes into the sim (T6-OUT)

`core/src/talk_outcome.c`: the only place where Talk writes to the sim. It
applies the `TalkSummary` of `talk_dm_end` under the daily caps of design 5.5.
A small ledger of the town day (keyed by day and town seed, 64 residents and 64
pairs, about 900 bytes `.bss`, not saved: a reload forgets the day) keeps the caps
across conversations, so no number of talks farms more than they allow. The
guarantee holds for 64 residents and 64 pairs a day: past that the oldest entry is
reused and forgets its caps (known, accepted; T8 may save the ledger in the TALK chunk).

| Effect | How | Cap |
|---|---|---|
| Bond with the player | `rel_adjust(avatar slot, id, d)`; friendship `level` +1 per 4 bond gained; `family_care(id)` once a day (the bond nibble of a family member; a pool member gets only that) | net +-12 a resident a day (`TALK_CAP_REL_DAY`), level +3 (`TALK_CAP_LEVEL_DAY`) |
| Resident-resident | `rel_adjust(a, b, d)`; an ultimatum costs the pair 3 more, so a big row can end in `REL_RIVAL` through `rel_adjust`'s own rules | net +-8 a pair a day |
| Moods | `STAT_HAPPY += mood_d`; `STAT_SOCIAL` +4..+12 by engagement (none for one who walked out); then `sim_mood_of` | happy +-12, social 12 a day |
| Problems | `TEV_PROBLEM`: `sim_problem_resolve` (the sim keeps an item or possession problem open and Talk does not count it) | 1 a resident a day |
| Mediation | `TEV_MEDIATED`: the sim's own `sim_make_up(a, b)` (a pair with no bond is skipped) | 1 a pair a day |
| News | `NT_TALK_MADE_UP`, then `NT_TALK_ROW` (an ultimatum), `NT_TALK_WALKOUT`, `NT_TALK_GREAT_NIGHT` | 1 a conversation, 4 a town day (`TALK_CAP_NEWS_DAY`); none when a minor is in the row or walks out, none for a pair of minors |
| Romance | a yes (`TEV_ROMANCE_YES`) to CONFESS or ASK_OUT only **asks** `sim_confess(avatar, id, 10)`; the sim rolls, applies its gate again and owns every crush bit and dating edge. When the avatar is the hero or a family member the sim has no romance edge, so nothing is asked. A flirt asks nothing | 1 a pair a day |

* **Use.** `talk_outcome_begin` (= `talk_dm_begin` + `talk_mem_begin`, and
  `TPF_REFUSED` from "refused today"), the turns as before, then
  `talk_outcome_end(s, &out, &sum)`: it writes the talk memory (the last topic of
  those who spoke, the flags the player earned for those who stayed in the room,
  `refused today`), calls `talk_dm_end`, and applies the summary. `talk_outcome_apply`
  alone applies a summary. `TalkOutcome` says what was really applied.
* **Fix in T6-DM's end.** `talk_dm_end` no longer sets `TalkConv.ended` (that is
  K-7's, and `talk_mem_end` locks the whole room for the day on it: every talk would
  have locked everyone out). `TDF_OVER` is what ends the talk.
* **Not done here.** "Learning likes" (a GAME beat or an ASK_ABOUT answer marks a
  like as known): the summary does not say which like was asked about; it needs a
  `TEV_LEARNED` event from the drama manager (T5B-ASK / T8). `beats_recent` is not
  filled from memory (the table keeps no beats). Hate and the K-8 penalty are already in
  `rel_d_player`.
* **Tests** (`test_talk_outcome.c`, 15): the bond / level / mood / pair caps, the
  next day, a leaver gets no social, mediation once a pair a day and one headline,
  strangers skipped, news rules and none for minors, a problem once a resident a
  day and an item problem left open, romance asked and never forced (hero avatar,
  a flirt, a child, the sweep), 100 sessions by a greedy player stay under every
  cap (the sessions asked for a summed 1,310 bond; headlines 4), the talk memory
  written back (and nobody locked out), refused today across talks and days, and
  P7 over 1,500 random sessions with romance acts (the romance sweep finds nothing).
* **Memory.** Native only (like talk_save.c): 0 B on GBA / PS1 / Dreamcast, GBA
  EWRAM / IWRAM net zero, PS1 margin unchanged.

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

**After TK3 (T5a, T7), before K-10**: GRAMMAR 13,852 B and STRINGS 16,566 B stored
(25,603 B raw) for the starter bank's 355 rules, 668 alternatives, 466
guard terms, 77 symbols and 660 template strings, with the 1,000-entry
roster job map (2,000 B) and the likes, catchphrase and cast maps. Both are
loaded by the PS1 too: `TALK_en.BIN` is 538,358 B and **the PS1 subset
207,240 B** of the 312 KB budget (T5b's ~6,000 alternatives would add
roughly 250 KB at this rate: the PS1 subset would pass the budget, so T5b
must measure it and the levers of design 6.3 apply). Code, `-Os`, `.text`:
`talk_gen.c` 11,853 B (`arm-none-eabi-gcc -mthumb`), 20,972 B
(`mipsel-linux-gnu-gcc -march=r3000`, the PS1) and 14,786 B (`sh-elf-gcc`,
the Dreamcast); no `.data` or `.bss`. `talk_db.c` +76 / +124 / +80 B
(`talk_db_string`), `talk_safe.c` -14 / -100 / -16 B (the lint core shared
with the probe). **The PS1 talkgen part is
12,288 B in the design, for the generator, the drama manager, the outcomes
and the gate together**: the generator alone is over it on MIPS, so TP2
must split it (the design's third part) or trim it (the trace and the
exchange stand-in are host-and-T6 conveniences); measured here so TP2 starts
from the number. `talk_safe.c` grew by the probe (one flag through the lint
core). The census measures about 80 us a line on the host (6d).

**T5B-WARM (the warm acts, content only)**: +739 alternatives in four new files,
`grammar/en/warm_praise.tlk` (222: compliments, praise of a third party, thanks),
`warm_comfort.tlk` (181: comfort, encouragement, sympathy; the light kind, grief
keeps its own bank), `warm_play.tlk` (211: jokes, laughs, high fives, warm greetings)
and `warm_offers.tlk` (125: offers, invitations, promises). 1,457 alternatives in 16
files now. Every reply act the adjacency table allows after a warm act has lines;
voices by guard: polite, flat, blunt, loud, slow, quick, quirky, serious, elder, teen,
kid (tagged `kid`), mood, relationship, time of day, hellborn and ghost. Rules: at most
3 sentences a line (an opener or a tag may add one, so most core lines have 1-2), at most
one exclamation mark except loud and kid lines, no register word, no filler, no romance,
nothing a kid may not hear. `TALK_en.BIN` 540,274 -> 569,233 B (PS1 non-resident data,
no resident byte). No code change.

**T5B-COOL (the cool and rude acts, content only)**: +600 alternatives in five new files,
`grammar/en/cool_insult.tlk` (150: INSULT and MOCK), `cool_tease.tlk` (139: CRITICIZE and
TEASE), `cool_blame.tlk` (134: COMPLAIN and BLAME), `cool_dismiss.tlk` (122: DISMISS and
THREATEN) and `cool_extra.tlk` (55: sarcasm and brush-offs by topic, by the memory of an
insult or compliment, by relationship, night, ghost, hellborn, possessed and stance).
Every reply act the adjacency table allows after a cool act has lines. Voices by guard:
polite, flat, blunt, loud, quirky, serious, easygoing, elder, kid (tagged `kid`), mood,
relationship, night, hellborn, ghost. Rude stays PG-21 comedy (SCRIPT_STYLE R4): the target
is a hat, a walk or a joke, never a body, a family or a loss; threats are cartoon (a duck, a
postcard, a nap); no slur, no register word, no romance. The sharp lines carry `!kid_present`
and a child speaker gets a cute deflection ("I'm telling!", "Not nice!"). At most 3 sentences
a line. `TALK_en.BIN` 540,274 -> 567,450 B on the starter bank; 596,426 B with T5B-WARM too (PS1 non-resident data, no
resident byte). No code change. A 40-line sample is in the evidence directory.

**T5B-ROM (the romance acts, adults only, content only)**: +492 alternatives (target about 400)
in two new files, `grammar/en/rom_flirt.tlk` (226: replies to a flirt, flirting back, laughing,
teasing, a jealous tease, a kind no) and `rom_ask.tlk` (266: ASK_OUT yes / no / deflect / joke,
CONFESS yes / no / joke, a confession in return, the awkward crush, a soft "not tonight" no with
`rel=partner`). 95 rules, all named under the romance roots (`core.flirt.r_*`, `core.ask_out.r_*`,
`core.confess.r_*`) so lint TK3 reads every one. Every rule carries `adults_only&!kid_present&!pet`
exactly like its neighbours in `core_romance.tlk`; a yes carries `consent`, a no
`!consent`; no rule has a teen or kid voice, and no line carries its own guard. Minors still only
get the safety banks. A guard audit of all 95 rules is in the evidence directory. Words: no
sexual-class or flirt-class word (TK4 / TK5 refuse `flirt` and `flirting`, so the lines say
"charm", "woo", "sweet talk"), no topic echo, no catchphrase, no LOVE face on a no. Every line ends
on a joke or a kind close; a no is never at the refused person's expense. At most 3 sentences and
78 characters a line. `TALK_en.BIN` 672,124 -> 694,396 B on the rebased branch (+22,272 B of PS1 non-resident data, about 45 B an alternative; no resident byte on any port; the PS1 EXE with the FMV still ends 0x801ef450, 66 KB below the stack top, and the GBA sections are unchanged).
Review fix: the break-up rules (`core.confess.r_split*`) were removed, because no break-up act exists and a declined confession from a partner must not end the relationship in text; new lint TK11 (`talkc_safe.py`, seeded bad file `core/tests/talk_bad/bad_tk11_breakup.tlk`) refuses break-up wording in any romance decline (act `no` or `deflect_romance` under a romance root). A break-up bank waits for a real act (T6 / sim). A 40-line sample is in the evidence directory.

**T5B-KIDFIX (no insult, mock or blame line from an adult at a present child)**: a review of
T5B-COOL found that most reply voices to INSULT, MOCK, TEASE and BLAME turned on the listener
without `!kid_present`. Now every adult-spoken reply of act insult, mock, tease, blame, threaten
or dismiss in the cool banks (`core.insult`, `mock`, `tease`, `criticize`, `complain`, `blame`,
`dismiss`, `threaten` rules) either carries `!kid_present` (the whole rule or one line) or sits in
a rule tagged `gentle` (reviewed: a silly deflection or a joke on the speaker, fine for a child).
A child speaker keeps its own `kid` banks; every act keeps unguarded lines, so a room with a
child never runs dry. A few lines that came near a body or a family were reworded ("it's
genetic", "closed at birth", "family curse", "nature beat me to it", "you smell like", "a long
face", "better people", "I know where you sleep"). The Vex voice insult now has `!kid_present`.
New lint **TK10** (`talkc_safe.py`, run by `talkc --check`): an adult-spoken cool-act reply
without `!kid_present`, `kid_speaker` or `tags: gentle` is refused; two seeded bad files
(`core/tests/talk_bad/bad_tk10_*.tlk`). Alternative count unchanged (guards only, no line added
or dropped). Data and a lint only: no resident byte on any platform.
Landing pass: the walk, charm, fashion and hat lines in `c_back` rules got `!kid_present`
(the `gentle` tag stays for self-jokes and deflections), and the elder smack line is now "a stern
look". Known gaps: TK10 covers only the acts insult, mock, tease, blame, threaten and dismiss.
Mild listener lines in the acts complain, disagree, laugh and joke ("You're being dramatic",
"you little menace") are not linted, and TK10 trusts the `gentle` tag. Kid-present reachability
of the all-sharp rules (insult.c_mock, threaten, tease.c_x) is for the census, and the owner's
human read (Q7) of the rude lines is still owed.

**T5B-S3FIX (the census's Q6 lint failures and S3 hits)**: the census at 2,057+ alternatives
reported 4 Q6 lint failures in the 2,000,000 contexts and 5 more S3 hits in the 2,000,000
conversations (9 hits in all, one cause). All were the same line: the cool-blame rule "It was
{x}'s fault, not mine! Go and blame {x}!" and `cool_extra`'s "Don't blame {x}! It's not {x}'s
fault." with the topic **spice**. The filler probes the topic word alone ("spice", clean), but the
template put a literal `'s` after it, and the S3 lint reads "spice's" as one word that squeezes
to a hate-list plural. The line was replaced by a safe one ("Anyway.") and counted. Fix, data
only: both lines now say `{x:s}` (the filler adds the `'s` first, then probes the whole word).
New lint **TK12** (`talkc_safe.py`): a topic-slot word (`{x}`, `{food}`, `{hobby}`, `{music}`,
`{fear}`, `{hate}`, `{like}`, `{mem}`, `{job}`) followed by a literal `'` is refused, so the next
one cannot get in (seeded bad files `core/tests/talk_bad/bad_tk12_possessive.tlk` and `bad_tk12_possessive_mod.tlk`,
the second for a slot with a modifier such as `{x:cap}'s`; the old lines fail the build with TK12, which is the
regression guard). Regression test `talk_gen_possessive_hate_plural` (9,000
exchanges with the spice topic, 0 hits and 0 safe lines; it covers the `{x:s}` runtime probe;
also every resident's name with `'s` after it lints clean, since `{them}'s right` still spells
the `'s` out). No code in `core/src`: no resident byte on any platform; this package does not
change `TALK_en.BIN` (755,772 B on the landed tree). The name slots in `parts.tlk` and
`ask_answer.tlk` (`{them:cap}'s`, `{other}'s`) are not covered by TK12; the unit test checks every
alive resident's name instead. A renamed or typed pet is the open gap.

**T5B-SURF2 (Surface push B: persona, mood, time, relationship and stance variation)**: +700
English alternatives in six new files `grammar/en/surf_b_*.tlk` (6,346 alternatives and 74 files in
all, counting talkc's own tally), 350 rules named `sb_*`. The census showed that a line with no topic
slot adds about 2 distinct lines per 300,000 contexts, and a line that names the topic (`{x}`) adds
about 13, so only the first file is topic-free. `surf_b_type_1.tlk` (192: eight persona types answer
twelve plain acts in their own voice: thank, tell, agree, disagree, joke, laugh, compliment, encourage,
invite, offer, apologize, comfort), `surf_b_mood.tlk` (144: happy, sad, angry, scared, tired, weird times
twelve acts, every line names the topic), `surf_b_type_topic.tlk` (192: all 16 persona types times six
acts, topic named), `surf_b_rel.tlk` (72: stranger, acquaintance, friend, best friend, rival, family
times six acts, topic named; a rival stays a grudging rival, never an insult), `surf_b_time.tlk` (48:
four times of day times six acts) and `surf_b_stance.tlk` (52: a fan or no fan of the topic, 13 acts,
some lines with `{x.adj}` or `{x.assoc}`). Rules: at most 3 sentences a line, no register word, no
romance, no cool act (so TK10 has nothing to guard), no kid-unsafe line, no break-up wording, a grief
topic never reaches these (the grief bank takes it). Disjoint from `surf_a` (other rule-name suffixes).
No code change, no resident byte on any port. `TALK_en.BIN` 755,772 -> 790,460 B (+34,688 B, PS1
non-resident, about 50 B an alternative). Census, 300,000 contexts, same grid and seed (before ->
after): Surface 164,193 -> 169,815 distinct (Chao1 862,612 -> 959,114), Skeleton 158,769 -> 163,600,
Meaning 18,354 -> 20,445, quality 0 failed, S3 hits 0. Surface is still short of its 1,000,000 target:
700 alternatives are about +3.4% distinct at this size, nowhere near the +30% the gap asks for. What
moves the number is the count of lines that name the topic, the stance or an association; more
topic-naming alternatives (not persona words) are the lever for the next push. A 40-line sample is
in the evidence directory.

Cool acts with a child in the room (after T5B-KIDFIX): new census mode `talk_census --cool-kid
[--contexts N]` (N kid rooms per act, drawn from the census grid; `talk_census_cool_kid()` in
`talk_census.c`) counts how often a resident still gives a real reply (not the fallback or a
safe line) to insult, mock, tease, blame, threaten and dismiss when a child is in the room.
Before (3,000 kid rooms per act): insult, mock, tease, threaten, dismiss 100.00%, blame 99.97%
(2,999 of 3,000; the one miss was the spice line above). After (20,000 kid rooms per act):
**100.00% for all six**. No rule is all-sharp for a child: every act keeps unguarded or `gentle`
lines, so no deflection line had to be added. A unit test (`talk_census_cool_kid_reply_rate`,
1,500 rooms an act) holds each act to 99% or more.

Census after the fix (2,000,000 contexts and 2,000,000 conversations on the same tree): quality
failures **4 -> 0** (Q6 lint), S3 lint hits **9 -> 0** (4 in the contexts, 5 in the conversations),
Surface 767,799 distinct (Chao1 3,829,245; the 1,000,000 target is still missed, that is the
bank's size, not this package), Skeleton 719,360, Meaning 41,208, conversations 1,999,981
distinct. (The 6d table below is the older starter-bank run.)

Re-measured on the rebased tree (5,536 alternatives, `TALK_en.BIN` 755,772 B, context tier only,
2,000,000 contexts): quality failures 0 of 2,000,000, S3 lint hits 0, Surface 775,604 distinct
(Chao1 3,875,800), Skeleton 727,340, Meaning 41,760; cool-kid 100.00% for all six acts at 20,000
rooms each. The lead's earlier count was 5 Q6 failures and 7 S3 hits on a different tree and
sample; on the tree this package started from it was 4 and 9 (all `spice`, one cause).

**T5B-OPEN (openers, parts and the rest, content only)**: +648 alternatives in seven new files,
`grammar/en/open_greet.tlk` (97: greetings by time of day, first meeting, old friends, family,
acquaintances, strangers, rivals, mood, voice and the 16 persona types), `open_farewell.tlk` (62:
goodbyes and door promises, same axes), `open_manners.tlk` (86: replies to thanks, apologies and
introductions), `rest_flow.tlk` (121: agreeing, disagreeing, yes, no, answers, telling, opinions),
`rest_talk.tlk` (94: new topic, go on, mm-hm, "what?", gossip, defending, accusing, taking
sides, mediating), `rest_gesture.tlk` (97: wave, nod, shake of the head, hug, gift, walking away,
high five) and `rest_parts.tlk` (91: openers by act class, time, mood and voice; tags; hand-offs;
reactions between residents). Every reply act the adjacency table allows after these acts has
more lines than before (greet 75 to 172, farewell 9 to 71, apologize 6 to 31, introduce 4 to
28, hug 6 to 31, and so on). Rules: at most 3 sentences a line, no register word, no romance (a
greeting never flirts), kid voices tagged `kid`. A hug stays a plain hug: a stranger or a rival
(and a child who does not know the player) answers with a polite no or a wave. Rule names end
in `o_` / `r_`. `TALK_en.BIN` 672,124 B on the landed tree, about +28.7 KB for this package (PS1 non-resident data, no resident byte).
No code change. A 40-line sample is in the evidence directory. 3,680 alternatives in 30 grammar files with T5b-ASK.
**T5B-VOICE (the cast's own voices, content only)**: +596 alternatives in 17 new files,
`grammar/en/voice_<name>.tlk`, one a cast member (the 17 `speaker=` ids of `talkc.py`
`CAST`): Mayor Giggs, Nana Puddin, Bonejangles, Rivet, Pip, Hattie Haha, Father Fuzz,
Mascotty, Dr. Bloom, Wisp, Charlotte, Anvil, Vex, Grumbleguts, Clinker, Mother Nox and
Lord Vapula, 33-38 alternatives each over the 18 player acts a resident answers in plain
conversation (greet, farewell, thank, apologize, ask_about, ask_opinion, ask_feeling,
ask_advice, tell, joke, compliment, comfort, offer, invite, high five, hug, wave, give).
A `speaker=` line weighs 8 times a persona line, so these are what the cast says most of
the time. The register is SCRIPT_STYLE section 7: Giggs' lists of three and "Hee hee",
Nana's "sweet pea" and kind fibs, Bonejangles' "Clack!", servo and warranty, Rivet's
tools and "don't make it weird", Pip's Patrol, Fuzz's hug theology, Bloom's clinical
button, Hattie's stand-up structure, Mascotty's thumps and "if you stand still", Wisp's
lowercase "sorry", Charlotte's letters, Anvil's forge, Vex's "- V", Grumbleguts' "Needs
salt.", Clinker's ratings, Nox's soup, Vapula's weather.
Safety: Pip and Wisp are kid banks (`tags: kid`, Wisp also `lower`): no register word, no
LOVE face, nothing aimed at them. Grumbleguts' purr greeting and compliment carry
`adults_only&!kid_present` and he has a grumpy-uncle greeting and thank-you for a room
with a child. No romance act, no cool act, so TK10 does not apply and no line turns on
the listener. New test `core/tests/test_talk_voice.c`: every voice says a phrase only it
says (17 marks, cross-checked against the other 16 of the cast), the kids' banks pass the
S3 lint and wear no LOVE face in a kid room, and none of the five purr lines can come out
with a child in the room. `TALK_en.BIN` 694,396 -> 722,364 B on the rebased tree, +27,968 B (PS1
non-resident data, no resident byte). No engine change.

**T5B-GRIEF (grief, comfort and hard feelings, content only)**: +506 alternatives in 4 new
files, `grammar/en/grief_*.tlk`. The K-10 rule is unchanged: a grief or loss topic (grief,
mourning, condolences, loss, memorial, funeral) takes the sincere `grief` bank whatever was
said, and another resident may add one `comfort` line. The new rules are `grief.more.*`
and `comfort.more.*` and join the same family, so every K-10 lint applies (tags: nojoke,
no register word, no sub-rule, no catchphrase, no LOVE face, no [JMAT]); a plain no to a
laugh at loss, no cure, no lesson, no mechanic.
* `grief_loss.tlk` (169): the generic banks in 21 reply acts (answer, tell, opine_neg,
  sympathize, comfort, encourage, offer, ask_about, continue, backchannel, agree,
  disagree, thank, yes, a no to a hug or a favour, the speaker's own heavy day, a promise
  after a goodbye, greet, farewell, dismiss, deflect_romance, and the `comfort.*`
  reactions). The `*_adult` rules carry `!kid_present`: a grown-up's deeper lines are
  never said with a child in the room.
* `grief_voices.tlk` (127): the persona banks are not used on a grief topic, so all 17 cast
  members get their own sincere lines, a few each (Vex "No post for this one",
  Pip "Patrol's on sad duty", Wisp lowercase and kind, Mascotty's one small thump,
  Grumbleguts' soup, Vapula's soft sky, Clinker's "no score today"); Pip is `kid`, Wisp
  `kid,lower`, Vapula `lower`. The six cast in `grief.tlk` keep theirs and get more. No
  line is a joke and none jokes about the loss.
* `grief_kids.tlk` (69): a child speaks (`kid_speaker`, tags kid): short, honest, never
  graphic, may say no to a hug; and a grown-up with a child present (`!kid_speaker&
  kid_present`): "you're safe", "none of this is your fault", a hug only if wanted, "tell a
  grown-up you trust". Strict kid-room lint, no LOVE face.
* `grief_feelings.tlk` (141): the hard feelings that are not grief (loneliness, worry,
  homesickness, regret, shame, stress; fears), when the speaker does not like the topic
  (`root=feelings&stance=con`, `root=fears&stance=con`): listening, asking, a small step
  for advice, comfort for a complaint; and the speaker's own sad, scared or lonely moment
  (`mood=sad`, `mood=scared`, `prob=lonely`) and thanks for kindness while low. The comic
  banks still joke about these feelings; these lines weigh 4 so the careful side is
  heard. Typed self-harm words keep their existing out-of-character path (`safety.tlk`,
  K-11); nothing here touches it.
New test `core/tests/test_talk_grief.c`: every generic family is reached on all six grief
topics over 22 acts, every line is sincere and never a fallback and S3-lint clean; a cast
voice is said by its own speaker only; with a child in the room none of the grown-up-only
lines come out and the room stays strict-lint clean; a hard feeling is still a plain topic
(never the grief bank). `TALK_en.BIN` 755,772 -> 776,188 B (+20,416 B, PS1/GBA banks are
streamed: no resident byte). No engine change. A sample is in the evidence directory.

**With K-10 (the TK3 landing)**: GRAMMAR 14,536 B and STRINGS 17,650 B
stored (27,439 B raw) for 373 rules, 718 alternatives, 512 guard terms, 79
symbols and 710 template strings; `TALK_en.BIN` 540,274 B and the PS1
subset 209,070 B of the 312 KB budget. `talk_gen.c` `.text`, `-Os`:
12,540 B (Thumb), 22,120 B (MIPS r3000) and 15,736 B (SH4), still no
`.data` or `.bss`; the stack is in 6c (about 5 KB in the worst case).

**Reads per line (measured, and over the design's budget on the PS1).**
Counting `talk_db_read` by bank over 5,000 census contexts (8,428 lines;
inside `talk_gen_exchange` only, the gate and the generator, not the census
checks): per line about 246 reads and
2.4 KB of TB_GRAMMAR, 15 reads and 0.5 KB of TB_STRINGS, a few of the
concept banks, and **1,071 reads and 28.5 KB of TB_SAFETY**: about 1,350
reads and 31.7 KB a line, 53 KB an exchange, and 7,764 reads in the worst
exchange. The SAFETY share is the S3 lint and the per-filler
`talk_safe_lint_probe`, each of which reads the whole register list from
the database. On the PS1 every read is a SpuRead, so this is about three
times the design's 16 KB a turn (6.4). **TP1 / TP2 must fix it before the
PS1 talks**: keep the register rows (6.4 KB) in the 24 KB per-turn scratch
for the turn, or probe less (one lint of the joined line instead of one
probe a filler).

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
| `core/tests/test_talk_gen.c` | T5a on the test grammar `core/tests/talk_gen/gentest.tlk` (`TALK_gentest.BIN`): every modifier and field, names, likes, the job callback for an adult and none for a child, the weights, Q5 novelty, sub-rules, the fit and the safe fallback, K-9 (one swear a conversation, none with a kid), K-6 (a Mature variant only with the option and premade adults), the cast's own lines, faces, the optional parts with every callback true (Q4), the exchange (the gate first: K-7 ends it unseen, refusals, distress, romance at a child both ways; a romance yes, a no remembered for the day), reactions that answer the line before (Q2), a typed name the room may not hear, determinism; K-10 (`talk_gen_grief`): a grief topic takes the sincere bank whatever the act, the core part alone and an act that may answer, a quiet safe line when no grief line may answer, the safety acts keep their banks, the loss fact (a household member, not a move away, 28 days), `{lost}`, one comfort reaction at most and never the pet |
| `core/tests/test_talk_census.c` | T7 and T5a over the English banks: the smoke census (6d) and the generator-level property tests P1-P8 (section 2; `DOOMO_TALK_PROPERTY_N` for the design's 200,000) and K-10 (`talk_gen_property_grief`: every act about every grief topic in adults', kids' and pet rooms) |
| `core/tests/test_talk_save.c` | T6-MEM (6e): the table (find, touch, last two topics, LRU of 24, likes, who may have an entry), the day rollover (refused today only), the flags of a running conversation, the chunk (empty = 0 bytes, worst case 292 B, full save round trip, an old save without the chunk, truncated / too long / wrong version / count over 24, a gone resident, the pet, a double id, a retired concept), and the generator guards and callback through `core/tests/talk_gen/mem.tlk` (K-5 across two conversations) |
| `core/tests/test_talk_outcome.c` | T6-OUT (6g): the daily caps, mediation, problems, news, romance asked and never forced, 100 greedy sessions, the talk memory write-back, P7 over 1,500 random sessions |
| `make -C core talk-check` | `talkc --check` (now also TK1-TK9, the corpora and the grammar compiler) and `--selftest` (51 seeded refusals: 12 lexicon files, 4 bad edges, 13 templates, 2 rule files, 20 grammars; and `core/tests/talk_good.tlk` must lint clean) (part of `make -C core test`) |
| `make -C core talk-census` | the full census (6d; not in `make test`: about 25-30 minutes) |
| `python3 core/tools/talk_sample.py selftest` | the Q7 sheet's scoring |
| `make -C core check-gen` | `gen_game.tsv`, `concepts.lock` and `talk_fixture.c` regenerate byte for byte |

---------------------------------------------------------------------------

## 9. Differences from the design

| Design | Built | Why |
|---|---|---|
| 16 category roots | **18**: `describe` (the attribute vocabulary: adjectives and participles) and `stuff` (everyday objects) were added | Attributes must be concepts, and they fitted no root. Their ids are fixed in `talk.h` |
| The roster's 1,000 names seeded as concepts | **Not concepts**. The cast (17 roster characters) are concepts, and Sunny, Marvin and the Boreagotchi are authored | A name the player types becomes `PERSON(id)` at runtime (design 3.2). Custom residents' names cannot be in a static network, and 1,000 name rows would cost a quarter of the 12-bit id space |
| `plat_room_project(x, y, z, &sx, &sy)` | It takes the view as its first argument | The weak default (room_ref's projection) needs the view. A GTE platform can ignore it |
| `'TALK'` save tag | It is a **13th** registry slot (`SAVE_EXT_SLOTS` 13; LV0 added a 14th, `'BRCH'`); K0 had said "never a 13th" | The design's D9 (recommended yes, and listed for T0): LIFE's spare bytes are used up, and the GBA never registers this chunk. `BudgetSnap` stays 36 B |
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
| `TB_VOICE`: persona tics and cast overrides in their own bank | **In `TB_GRAMMAR`**: a tic is a `tag.*` rule with a `type=` guard, a cast line any rule with a `speaker=` guard (weight x8) | One selection path; the lints and the census see them like any rule |
| Templates compiled to bytecode | **Compiled text**: the template is kept as a Huffman string with its fillers and sub-rules turned into short codes (`{X:n}`, `{#12}`) that `talk_gen.c` reads while it writes | The story codec packs it; no second interpreter; the PS1 decodes about one 100-byte string a part |
| Core rules keyed by (act, reply act) pairs | Keyed by **the act replied to** (`core.<act>.*`); each rule names its reply act (`act:`), which `acts.tsv` must allow | One entry per player act; Q2 is checked at compile time |
| Novelty: weight / 8 for a template used lately | **Taken only when nothing fresher is possible**, with a second ring for the whole room (16 lines) | Q5 counts any repeat as a failure; with weight / 8 a small bank repeats; 4 conversations in 1,000 still repeat a short reaction ("Mm-hm.") |
| 1-3 sentences a line | The same, **a fragment counts** ("Hm."), "Dr." / "St." / "Mme." do not; talkc refuses a line of more than 3 | Clarity first (the owner's priority; SCRIPT_STYLE C4 "fragments in a row") |
| The drama manager picks who answers and who reacts (T6) | `talk_gen_exchange` does it plainly until T6: the one addressed or the most interested answers; reactions by design 5.4's formula, **capped at 75%**; no beats, tension, engagement, interruptions or idle chatter yet | The generator and the census need exchanges now; T6 replaces the policy, not the API |
| The smoke census: 100,000 contexts, 90% Surface-unique, 30% Skeleton-unique, 0 safety failures | **20,000 contexts and 1,000 conversations** (about 4 s), 0 safety failures, quality failures 1% or less, Skeleton 30%; Surface 50% | The unit run's time; the 90% Surface bar is for T5b's full bank (the starter makes about 57%) |
| A census context: 8 mood bands, 1-3 residents, 400 topics | 7 mood bands (`TMB_*`: ok, happy, sad, angry, scared, tired, weird); the rest as designed | The moods the sim has |
| K-10: "a memorial topic (`family_memorial*`) takes the sincere core bank first and no punchline" | A **grief or loss topic** (6 flagged concepts) takes the sincere `grief` bank whatever the act, the core line alone, **and no joke follows at all** (no button after it, though R6 / J5 allow one in authored scenes); the memorial becomes the speaker's fact (`fact.loss`, `{lost}`) | A person is not a topic concept in Talk, so the memorial reaches a line through the speaker; a generator cannot judge when a button after grief lands, so it never adds one |
| P6: 50,000 typed lines | **4,000** in `make test` (`talk_gen_property_vocabulary_and_swears`); T4's own P6 runs 50,000 random lines through the parser | The unit run's time |
| Chao1 `S + f1^2 / 2 f2` | The same; `S + f1 (f1 - 1) / 2` when no value was seen twice | The usual bias-corrected form, defined when f2 = 0 |
| A typo fix towards a safety word for every unknown word ("refusing is the safe error") | The same, but real words and first names one letter from a safety word are listed as known (`common_en.txt`, `names_en.txt` with `TW_NAME`) | Measured on the 50,000 most frequent English words: "invest", "tweet", "reward", "Pedro" were refused or ended the talk as K-7 (6b, "Precision"). A wrong K-7 ending tells an innocent player they did something terrible: the owner's comprehensibility rule. Recall on misspelled safety words stays at the bars |


**Known risks (open):**

* **The 1,000,000 needs T5b.** The starter bank makes the Skeleton,
  Meaning, conversation and quality targets but not Surface (6d); the
  design's arithmetic assumed ~6,000 alternatives. Q7, the human read, has
  not been done: the starter lines are the TK3 agent's, unreviewed.
* **PS1 code size.** `talk_gen.c` alone is over the 12 KB `talkgen` part on
  MIPS (section 7): TP2 must split or trim.
* **The exchange is a stand-in.** Without T6 there is no tension arc, no
  engagement, no walk-outs and no outcomes: a room of three chats a lot
  (most exchanges get a reaction). T6 should tune that for clarity.
* **Distress wording** (K-11) is the agent's; the owner approves it (D6).
  The grief and comfort lines (K-10) are the agent's too; the owner reads
  them with the Q7 sheet.
* **PS1 reads per line** (section 7): about 31.7 KB a line, three times
  the design's 16 KB a turn, mostly the S3 lint re-reading the SAFETY bank:
  TP1 / TP2 cache the register rows or probe less.

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
* **Dreamcast Talk waited** for DC3, DC4 and DC8, and for the owner's
  approval of the Flycast core build. All four are in (2026-11-08): DC8's
  `plat_room` implements zoom 2 (the talk framing, a glide to the circle
  around `focus_x` / `focus_y`) and `plat_room_project` follows it
  (docs/DREAMCAST.md section 8).

The UI will follow the owner's top priority, comprehensibility. T8 must
make the mode obvious. It needs:

* a help line;
* clear prompts;
* the quick replies first;
* the press budgets of design section 2.3.

---------------------------------------------------------------------------

## 11. What comes next

T3, T4, T5a and T7 are done. Next on the critical path: T6 (the drama
manager, outcomes and talk memory; it replaces `talk_gen_exchange`'s plain
turn-taking and sets `TalkConv.consent` through the sim), T8 (the house
mode, with the help line, clear prompts and quick replies first), and then
the PS1 packages TP1-TP3 (TP2 must fit or split `talk_gen.c`, section 7).
T5b, the English line banks, grows the starter bank to the design's ~6,000
alternatives, re-runs `make -C core talk-census` until Surface reaches
1,000,000, raises the smoke census' Surface floor to the design's 90%, and
takes the Q7 review sheet (`talk_sample.py`) to the owner.
The banks are written in the PG-21 register (`docs/design/story/SCRIPT_STYLE.md`).
Each resident speaks in their own voice. The comedy is abrupt and parodies
The Walten Files.

T10, the other languages, comes **after** the script rewrite (DECISIONS:
translate after the rewrite).
