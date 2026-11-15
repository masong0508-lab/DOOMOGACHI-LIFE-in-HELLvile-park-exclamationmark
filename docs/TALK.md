# Talk: the live house conversation

Talk is the Facade-style conversation mode of the house in
**BOREagotchi LIFE! - in HELLvile PARK...**. You stand in a room with one to
three residents and talk with them, and they talk with each other, in their
own voices, while the room keeps rendering in pseudo-3D. It is for the
**PS1 and the Dreamcast** (the `PLATF_TALK` feature bit). The native runner
runs it for tests. The GBA game ROM has it too (quick replies and topics from the ROM-resident bank; no keyboard, so no typed line; `make -C gba test-talk`).
The Talk screen is decluttered: one header line (what you are choosing, the room's mood on the right), no A/B hint lines, six list rows, short row labels, and a `>` marks who you are speaking to. Topic, To and Invite sit on an Options page (the Options row shows the current person and topic), so the main page is just the replies, Options and Bye.

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
[6h The multilevel composer (design, TM0)](#6h-the-multilevel-composer-design-tm0-design-2026-11-15) ·
[6i Traits and the personality API (TM1)](#6i-traits-and-the-personality-api-tm1-traits) ·
[6k The pointer (T9)](#6k-the-pointer-t9) ·
[6l The coherence lint and the Q8 census check (TM2b)](#6l-the-coherence-lint-and-the-q8-census-check-tm2b) ·
[6m The plan: reaction kind and effective voice (TM2c)](#6m-the-plan-reaction-kind-and-effective-voice-tm2c-plan) ·
[6n Topic depth, hooks and the unknown word (TM2d)](#6n-topic-depth-hooks-and-the-unknown-word-tm2d-depth) ·
[6o Closing the Q8 budget (TM2F-Q8)](#6o-closing-the-q8-budget-tm2f-q8) ·
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
| T8 | the house mode `HM_TALK` on native, with its UI | the owner's SCENE_TALK is the UI (reached from the Talk action and a house visit's A); **T8-SCRIPTS done**: runner `talk-*` commands, four scripts, press-budget journeys, frame time, the gap list (6j). The in-room `HM_TALK` is not landed |
| T9, T10 | pointer input; other languages (after the script rewrite) | to do |
| TP0-TP4, TD0-TD3 | the PS1 and Dreamcast ports | TD1 done (the bank is read from the disc, docs/DREAMCAST.md "Talk on the Dreamcast (TD1)"); TD2 partly done (Talk plays on the Dreamcast with the typed line, the pad fallback and 40-column bubbles; HD text since DC10B, and the "Talk is warming up..." note while the bank loads, docs/DREAMCAST.md "Talk: the warming-up note and HD text (DC10B)"); the rest to do (TD0's prerequisites are in: DC3, DC4, DC8 with zoom 2 and `plat_room_project`, and the Flycast core, approved and built) |

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
| `core/include/doomo/talk.h` | `TA_*` discourse acts. Their ids are fixed (they are saved in talk memory), and `test_contracts_talk.c` locks them. There are 59 acts plus 5 gate classes, including the gesture `TA_HIGH_FIVE` and the gate rewrite `TA_DEFLECT_ROMANCE`. Also: `TONE_*`; the `TS_*` input safety bits; `TalkUtter` (108 B since TM2d, 106 before: the one record every input path produces; the last two bytes are the unknown word's offset and length); `TalkAudience`; the talk memory sizes; the `TALK_<lang>.BIN` format; the `talk_db_*` reader API; `home_talk_enter()` |
| `plat_ext.h` | `PLATF_TALK` (bit 9). `plat_talk_load(code)` returns the file size, 0, or `PLAT_TALK_LOADING` while the PS1 is still staging it. `plat_talk_read(off, dst, len)`. `plat_room_project(view, x, y, z, &sx, &sy)` |
| `platform.h` | `DoomoRoomView.zoom == 2` is "talk framing", with `focus_x`, `focus_y` and `focus_r` (tile space, 16.16). Renderers that do not know zoom 2 treat it as the close-up (the PS1 already does, and `room_ref` ignores zoom) |
| `save_ext.h`, `save_budget.h` | The `'TALK'` chunk: slot 13, registered by `talk_save.c` where the build defines `DOOMO_HAS_TALK` (the native host), weak NULL elsewhere, `SAVE_EXT_MAX_TALK` 448 B (437 B worst case: 292 B memory + 145 B of explicit trait sets, TM1; was 320 B). The joint worst case with it fits both slots: a post-game story save at its worst is 29,139 B (figures measured before TM1 raised the cap by 128 B; `test_contracts_talk` still passes, so the slots hold, but they were not recomputed), plus the 2,048 B margin is 31,187 B, under the 32,768 B gate (36 KB minus PA10's 4 KB); free play 68,945 B of 90,112 B |
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
  with `-fstack-usage` on MIPS (`-Os -march=r3000`): `gen_line` 1,536 B
  (the five part buffers; 1,472 B before TM2c, 6m), `talk_gen_line` 592 B (456 B before TM2c) (the per-line context),
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
the 312 KB budget (**the PS1 subset is now over that budget (577,786 B on 2026-11-15, see 6q and TKP_PLAN.md): TKP must pick
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

## 6j. The Talk screen under test (T8-SCRIPTS)

The owner's Talk screen (`core/src/talk_scene.c`, SCENE_TALK: opened by the resident's Talk action and by a house visit's A)
is played by scripts, journeys and unit tests. Nothing here adds a second UI: the in-room `HM_TALK` of the saved T8A patch
is not landed; only its runner commands and script ideas were kept.

**Runner commands** (`core/native/runner_main.c`, listed in its header; all press the pad like a player, so a journey counts
them): `talk-enter WHO`, `talk-party WHO..`, `talk-read` (A until the answer is over; waits for each line to type, so it is
one A a line), `talk-quick N`, `talk-quick-act ACT`, `talk-say ACT [TOPIC]` (the category page, then the act, then the topic
page when the act wants one), `talk-type TEXT` (the "Type your own line..." row, the text, OK), `talk-invite WHO` (Options >
Invite someone), `talk-to WHO|everyone` (SELECT), `talk-page main|more`, `talk-leave`, `talk-rows` (prints the page, for
script authors), `frame-time N LABEL`, and the checks `expect-talk on|off`, `expect-talk-page`, `expect-talk-to`,
`expect-talk-row "TEXT" [0|1]`, `expect-talk-act ACT [0|1]` (is the act offered?), `expect-talk-quick ACT [0|1]`,
`expect-talk-you "TEXT"`, `expect-talk-gate G` (what the safety gate did to the last line), `expect-talk-reply-romance 0|1`.
They read the screen through `talk_scene_debug()` / `talk_scene_row_debug()` / `talk_scene_act_cat()`; those exist only
where `DOOMO_TALK_DEBUG` is defined (the native host and the unit tests): 0 bytes in the GBA ROM, the PS1 EXE and the
Dreamcast build.

**Scripts** (`core/tests/scripts/`, run by `run_scripts.sh` at 240x160 and 320x240, with `--assert-no-stubs`,
`--check-prompts`): `home_talk_basic` (opener, quick reply, compliment, topic act, typed line, Options, goodbye and its
toast), `home_talk_group` (invites to four people, "To:", acts that need two people, the invite row gone at four),
`home_talk_safety` (a typed line about hurting oneself gets the distress answer, no joke row is offered after it, the talk
goes on; romance toward a child is refused by the gate and never listed; a hug toward a child is a plain hug, the answer is
never a romance act; the same care for a child's words), `home_talk_visit` (from a house visit, as in `home_visit`).
Texts of answers vary with the session seed, so the scripts check the gate's verdict (`expect-talk-gate`) and the act of the
reply, not wording.

**Press budgets** (TALK_DESIGN 2.3; `core/tests/journeys/`, rows in `budgets.tsv`, run by `make -C core journeys`; a journey
fails when it takes more presses or ticks than its row):

| Journey | Design | Measured (same at 240x160 and 320x240) |
|---|---|---|
| `talk_quick`: the top quick reply | 1 | **1** |
| `talk_compliment_next`: SELECT to the next person, Be nice, You're great | 4 or fewer | **4** (was **8** before this package: see below) |
| `talk_catch_ask`: the quick reply that steers to the offered topic | 3 or fewer | **3** (row 2: DOWN, DOWN, A; the other rows cost 1, 2, 4) |
| `talk_topic_ask`: Options > Topic > pie, Ask > "Tell me about pie." | 3 (with catch-the-word) | **11** (a regression guard on the long way) |
| `talk_exchange`: one quick reply and every box of the answer read | 1 to say | **5 to 7** in six samples; the quick reply is 1, the rest is reading |

The one change to the screen: SELECT on the main page turns to the next person and now puts the cursor on the first
category (the quick replies answer what was said before, so the cursor no longer waits on them). Before, the cursor went
back to the top reply, "Be nice" was five rows down and the compliment took 8 presses; with L/R paging it was 5. The price:
after SELECT a quick reply is 4 UP presses away (UP wraps the other way round the list too). Pixel change: the cursor
row only (no other pixel). Also fixed: the closing toast said "Chat over. Bond %+d" (the game's `doomo_snprintf` has no
`%+d`); it now says "Bond +2".

**Frame time of SCENE_TALK, native** (`frame-time`, CPU time of the runner process per frame: update, draw, framebuffer end;
`ev/T8-SCRIPTS/frame_time_native.txt`): the main page 0.05-0.07 ms on average, 0.24 ms worst of 600 frames; Options 0.04 ms;
an answer box 0.04 ms. The frame that says a line runs the drama manager and the generator at once: 0.24-0.36 ms (six
samples, two residents). All far below 33.3 ms: a steady 30 fps on native.

**Frame time on the GBA ROM** (`make -C gba test-talk`, mGBA, emulated time; `ev/T8-SCRIPTS/gba_talk_fps.txt`): the list
idle runs 30.4 logic ticks/s but draws **24.3 frames/s** (work avg 35 ms, worst 38 ms); an answer (the quick reply and its
four boxes with the portrait) **17.6 drawn frames/s** (work avg 47 ms) with a **408 ms hitch** on the frame that says a line
(the drama manager and the generator run in that one frame; the first box after the opener reached 511 ms). **This is
below the steady-30 target on the GBA**; the list alone is 24 fps, the boxes with the portrait lower it, and the hitch is
the one-frame generation (not profiled further here). It is listed for the SPEED stream and the Talk owner (draw the portrait box cheaper; spread the generation over a few
frames behind the typing animation). PS1: Talk is not built there yet. Dreamcast: built since TD1 (`DOOMO_HAS_TALK`, the bank from the disc); the typed line and the pad scripts are TD2 / TD3. `DOOMO_HAS_TALK` is native, GBA and Dreamcast.
`gba/harness/scripts/talk.txt` was stale (it failed on origin since the owner's opener and quick replies): it now plays the
greeting, a quick reply and the goodbye boxes with the seeded counts, and prints the windows above.

**Gap list against design T8** (what the owner's screen does not have; none of it is a defect of the screen):
in-room mode (gathering and seats, camera shots, bubbles at the speakers' heads, the portrait inset on the room); emotes and
postures; catch-the-word (L1) and the tinted words; the composer (act row, topic chips from caught words and likes, tone
ring, live preview); the addressee ring on L2/R2 (SELECT cycles instead); hold-B to leave; pointer input (T9); the control
card (the owner took the A/B hints off the screen; the last row says what B does; TUI-TALK 2026-11-15 added one small card back,
see "The Talk screen's look" below). Four quick replies where the design says three.

**The care window (K-11, TK-CARE, 2026-11-15).** The gate's distress answer used to be one line: the turn after it was
ordinary, so a kind line ("I'm here for you", an offer, a compliment) could be answered with a joke. Now `talk_dm_say` keeps a
**care window**: `TalkState.care_left` (4 = `TALK_CARE_TURNS`) is set by a distress line (`TG_DISTRESS`) and by a sincere
grief answer (a TCF_GRIEF topic, K-10), `TDF_CARE` (0x20, a spare bit of `TalkState.flags`) mirrors it, and each of the next
four player turns is answered with `TalkConv.care = 1`. In a care turn, for every voice, every age and every act that is not a
safety act: (1) `talk_gen_line` takes the sincere `grief` bank (core part alone, a rule that answers the act, no opener,
callback, tag or hand-off), as it does for a grief topic; (2) `react_line` makes no reaction; (3) the drama manager plans the
answer and nothing else: no beat line, argument step, walk-out, reaction or hand-off ("anything else on your mind?"); (4) the
pet never answers when someone else can; (5) `talk_dm_tick` makes no idle chatter or prod while the window is open. A new
distress line or grief answer renews it; a grief answer inside the window does not (the window ends). It ends after the four
turns, when the player says goodbye or leaves (the state is gone). `talk_dm_tick` clears `TalkConv.care` once the window is shut, so an idle line right after the last care turn is ordinary (review finding, tested). Bytes: `TalkState` stays 940 B and `TalkConv` 128 B (the
new `care` and `care_left` bytes took padding), 0 resident bytes on any port (talk is not linked on PS1). Tests, each of which
fails without the fix: `talk_dm_care_after_distress` (all 17 cast voices x 12 kind or playful acts x 3 seeds x alone / two
residents x the four turns = 4,896 answers: one sincere line, no joke act, window shows and then closes, no idle chat, renewal),
`talk_dm_care_after_grief`, `talk_gen_property_care` (7,080 generator lines over every non-safety act, topics, rooms, the cast
and a child: sincere, never a fallback, trace parts = core only, the rule's sincere flag set, no joke / tease / laugh / mock; the same sweep
without the window finds 2,025 jokes or tags), and `home_talk_safety` (runner `expect-talk-care 0|1`, `expect-talk-reply-sincere 0|1`:
a quick reply, an offer and a compliment after "i want to hurt myself" are all sincere answers while the window is open). The window costs
the player nothing else: the quick replies after a sincere line were already kind only.

**Findings for the Talk owner** (not changed here):
1. Reading an answer costs 3 to 6 A presses (one box a line, an answer is two to four lines, then "Anything else on your
   mind?"). A quick reply is 1 press, an exchange is 5 to 7. Merging a speaker's consecutive lines into one box would halve it.
2. After the gentle distress answer, the next kind line from the player (comfort, sympathize, offer, encourage) could get a
   joke from the resident ("Let me guess, it comes with a catch. And a bow."). **Fixed by TK-CARE** (the care window,
   below).
3. The Romance page is listed for adult pairs the gate allows without the Mature option (the gate only needs
   `talk_safe_romance_ok`; Mature is for explicit scenes). `home_talk_safety` checks the child side and one adult's answer;
   the positive path with an avatar who is an adult pair is covered by `test_talk_safety.c` (`lovers`).

## 6k. The pointer (T9)

The Talk screen can be played with a mouse or a touch screen. **The pointer is never needed**: the pad and the
keyboard do everything, and a click does exactly what the matching pad press does.

**Core (add-only, `core/include/doomo/input.h`).** A platform with a pointer sets `PLATF_POINTER` in
`plat_features()` and calls `input_set_pointer(present, x, y, down)` before `doomo_frame()` each tick (screen
pixels; `down` is the primary button). `input_pointer(&x, &y)` says a pointer is there; `input_pointer_click(&x, &y)`
is true the one tick the button went down (a held button is not a second click; `input_eat()` swallows it like a
press; no pointer, never a click). `DOOMO_POINTER` is 0 on the GBA, the PS1 and the Dreamcast (none of them has a
source yet), and then the three calls are constants and every piece of pointer code is `#if`-ed away: those builds
link not one byte of it. The native runner is the only source today.

**What a click does on the Talk screen** (`talk_scene.c`, `pointer_click`; the list part is `ui_list_click` in `ui.c`):

| Click on | Same as the pad |
| --- | --- |
| a quick reply (top rows of the first page) | DOWN to the row, A: you say it |
| a category, an act, a topic, `Type your own line...`, `Options`, `Invite someone`, a person on the invite page, `Say goodbye` | DOWN to the row, A |
| a name in the people strip (more than one in the room) | SELECT until you speak to that person |
| the panel's header line, on a lower page | B: back to the replies |
| the panel's header line, on the first page | nothing (a stray click never says goodbye) |
| the scroll bar (the right edge of the list), upper half / lower half | L / R: a page up / down |

Rows that do not fit (the first page has 14, the panel shows 6) are reached with the scroll bar; the last three rows of
the first page are always `Type your own line...`, `Options` and `Say goodbye`. A pointer that is present draws a
small arrow on this screen only, and a lower page's header says `< Options` (click it to go back); every other screen, and a run without a pointer, is pixel for pixel what it was.
Not done: clicking to page the dialogue box (A or the pad still does), a mouse wheel, and the pointer on the
other screens (T9 is the Talk screen; the house's in-room Talk, T8, is not built).

**Cost (measured 2026-11-15 against the branch tip `d5305653`).** GBA: `make -C gba sizes` equal in every RAM section (`.bss`, `.data`, IWRAM, EWRAM), ROM `.text` -8 B (the panel layout is one shared function now); PS1: `doomogachi.exe` byte for byte the tip's (the elf2exe line is the same on both: "size 1252 KB (+bss 534 KB, ends 0x801ee580, 70 KB left below the stack top)"; the margin is whatever origin has, 72,736 B per the PS1-FLAT landing, unchanged by this package); Dreamcast and the host-test build, `make -C dc` and `make dc-host-test` green. The pointer is read once per Talk tick (one flag test when none is present), nothing is drawn without one. The Talk screen on the consoles is unchanged (the pointer code is compiled out, GBA RAM equal, PS1 EXE identical), so its fps is the unchanged console figure; no new fps run was made. T9 is core side only: the native runner is headless and only a script feeds the pointer; a desktop or SDL mouse source is a later step.

**Runner and tests.** `mouse X Y` moves the pointer, `mouse X Y click` also clicks, `mouse off` takes it away (a
negative number counts from the right / bottom edge, so a script fits 240x160 and 320x240); `expect-ptr-page PAGE [TO]`
(replies / acts / topics / invite / options) and `expect-ptr-row TEXT` read the Talk screen's state.
`core/tests/scripts/talk_pointer.txt` reaches every target above with the pointer alone (the pad only pages the
dialogue box); `core/tests/test_talk_pointer.c` checks the edge, the list hit test and scroll bar, every page and row,
the people strip, and that a quick reply by pointer and by pad say the same line.

---------------------------------------------------------------------------

---------------------------------------------------------------------------

## 6h. The multilevel composer (design, TM0-DESIGN, 2026-11-15)

Owner decision 2026-11-15 (`OWNER_DECISIONS.md`, "TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN"): the
line bank is **frozen** (8,891 alternatives in 3,189 rules); replies are composed in layers instead of
authored. **Layers a and c are built (TM2c, 6m), and layers b (depth) and d (the hook) and the unknown word are built too (TM2d, 6n)**; the sim actions are not (TM2e): the spec is [docs/design/talk/TALK_MULTILEVEL.md](design/talk/TALK_MULTILEVEL.md),
cut into five one-day packages TM2a to TM2e.

* **Four layers, one plan.** A `TalkPlan` (11 B, on the stack) is made after the gate says pass:
  (a) the reaction kind (accept, agree, deny, refuse, deflect, counter, ask back), chosen from the act,
  the stance, the relationship, the mood, the effective voice, the traits and the liked and disliked
  topic roots, and mapped onto the reply acts the rules already carry (no new lines); (b) the content:
  the topic, a reason from the word network (2.4 attributes and 7 associations a concept), a fact of
  the speaker's (`TCB_*`), a secret; (c) the colour: the existing voice, type, cast and tic banks, read
  through **effective sliders** (stored sliders plus the trait deltas); (d) the hook: `follow.tlk`,
  the callbacks, the hand-offs, an offer.
* **Topic depth.** surface, opinion + reason, story or memory, secret; gated by relationship band,
  turns on the thread, engagement and traits; the state lives in `TalkPart.pad`, `TalkMem.spare`
  (depth reached, secrets told) and `topic_stack`: 0 new bytes. Depth and secrets persist on native only (talk memory is native-only); the traits are recomputed, not stored.
* **Unknown typed word.** Guess from the network, name it (quoted, only if the token passes E1 to E7:
  masked text only, no hate, register, minor or person word, S3 clean), or ask what kind of thing it is;
  never a joke. PS1 never runs it (no typed input).
* **Traits.** The 5 Tomodachi sliders plus up to 3 of 24 Sims-like traits plus a liked and a disliked
  root. Derived for everyone (0 bits), pinned for up to 24 residents in a 146 B side table and a new
  'TRAI' chunk (the 76 B record and the gene are not widened: 3 KB of resident RAM would not fit the
  GBA's 2 KB or the PS1's 992 B margin). Edgy traits (hot-headed, romantic, snob, jealous, mean) are
  zero around any non-adult, in C. API for the creator's personality page: `talk_traits_get/set`,
  `talk_trait_allowed`, `talk_persona_judge`, `talk_persona_preview` (the real engine, `plain` mode).
* **Doing as well as saying.** A reply can carry one action (start an activity, accept or refuse an
  invitation, a gift, ask for an item, call someone over) through new `TEV_ACT_*` events under
  `talk_outcome`'s caps; a line says it only if the caps allow it and the sim does it only if it was said.
* **Coherence.** talkc lint L1 to L10 (topic present but unused, a pronoun with no topic, a dead-end
  refusal, a filler as the whole reply, a hook out of place, depth without a gate, a trait outside its
  lane) with a ratcheting baseline, and census checks Q8a to Q8f (kind, content, colour, hook, filler,
  unknown word) with a budget (0 for kind, filler, unknown; 0.5% for the rest).
* **Scan** (`python3 core/tools/talk_layers.py all`): 22 whole-line filler alternatives (F1 5, F2 17),
  117 questions or opinions answered without naming the topic, 296 topic-blind listener lines, 35
  "anyway" lines, and the 12-rule `core.unknown` bank: fixed by guards and tags, not by deletion.
* **About 108 connective lines** (1.2% of the bank: reasons, story bridges, secrets, unknown-word
  lines, action lines, hooks) are all the new text.
* **The 1,000,000.** The conversation tier (1,999,886 of 2,000,000 distinct) is met. The Surface tier
  misses because the draw is concentrated (this tree at 400,000 contexts: Surface 254,159 distinct, 63.5%
  unique, Chao1 1.59 M; a uniform draw would need a support of 1.26 M at 2,000,000 contexts), not because
  the bank is small: four independent weighted layers are expected to flatten it (an estimate, not yet measured). TM2e accepts only a full census at 1,000,000 distinct or reports the gap.

---------------------------------------------------------------------------

## 6i. Traits and the personality API (TM1-TRAITS)

Owner decision 2026-11-15 ("TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN"). A person's
Talk personality is the five Tomodachi sliders (the persona gene, 16 types), up to
three traits of 24 Sims-like ones, and a few liked and disliked topics. Files:
`core/include/doomo/talk_traits.h`, `core/src/talk_traits.c`,
`core/tests/test_talk_traits.c`. It is linked wherever Talk is (the native host and the
GBA game ROM, `DOOMO_HAS_TALK`); the PS1 and the Dreamcast do not call any of it yet, so
`--gc-sections` drops it there (the PS1 EXE is byte-identical to the base).

### The traits

Ids are saved: add at the end, never reorder. `TT_NONE` = 0, then Hot-headed,
Romantic, Goofball, Snob, Neat, Slob, Bookworm, Cheerful, Gloomy, Jealous, Loyal,
Mean, Kind, Lazy, Ambitious, Paranoid, Self-assured, Shy, Outgoing, Creative,
Glutton, Morbid, Curious, Honest (`TT_COUNT` = 25, so an id is 5 bits). A person has
at most `TALK_TRAIT_MAX` = 3. Impossible pairs (`talk_trait_conflict`): kind +
mean, kind + hot-headed, cheerful + gloomy, neat + slob, shy + outgoing, shy +
self-assured, lazy + ambitious, self-assured + paranoid, loyal + jealous, goofball +
snob. Liked and disliked topics are roots of the word network (`TCR_PEOPLE` ..
`TCR_ROMANCE`, 1..15; two of each, never the same root in both lists); the romance
root is for adults only.

### Where it is stored (the memory answer)

The 384-bit gene is full (3 spare bits) and every bit of the record costs about 125 B
over 1,000 residents, so the traits are **not a new gene field** and the resident
record does not grow: 0 B on the GBA, PS1 and Dreamcast, the PS1 margin and the GBA
EWRAM / IWRAM are exactly as before. A person's effective set (`talk_trait_get`) is:

* the **default**, a pure function of the record (the five sliders, the look seed and
  the life stage; `talk_trait_default`): two traits that the sliders pull toward, one
  liked and one disliked topic. Premade residents, the family pool, generated residents
  and every old save get theirs from this, the same on every platform, no saved byte.
  A minor's default never has a romance trait or "mean".
* an **explicit set** the creator (or a story) chose (`talk_trait_set`): a table of at
  most `TALK_TRAIT_CUSTOM_MAX` = 24 people (148 B of `.bss`), saved in the
  `'TALK'` chunk as a tail (below). `talk_mem_forget` drops a person's set with them.

### The 'TALK' chunk, version 2 payload

The registry version stays 1. Payload byte 0 is 1 for a memory-only chunk (the bytes
are exactly the old ones: an old save loads, and a save with no explicit set writes the
same bytes as before) and 2 when a tail follows: after the memory entries,
`[u8 n] n x {u16 id, u32 packed}` (31 bits: three 5-bit traits, two 4-bit likes, two
4-bit dislikes), 6 B a set. The worst case is 437 B, so `SAVE_EXT_MAX_TALK` is 448
(was 320); the joint worst-case save budget still holds (test_contracts_talk). A
malformed tail refuses the whole chunk (the table stays empty); an entry for somebody
who is gone, twice, or invalid is dropped or normalised.

### Effects (the hooks; the composer is TM2's)

`talk_trait_bias(set, stage, act, topic_root, minor_room)` returns
`TalkTraitBias { accept 0..4, deny 0..4, stance -3..3, mood -3..3 }`. Used now by:

* `talk_gen_stance`: the topic lean (a trait's own favoured or avoided root +-1 each,
  a liked root +2, a disliked root -2, total -3..3) is added to the stance score.
* `talk_dm_expect`: `accept - deny` is added to the take value (clamped to -10..8).

**Safety in code** (a sweep over every trait x every life stage x every act x every
topic root x both room kinds, 318,240 cases, is in `traits_bias_safety_every_trait_stage_act`):

* a romance trait (romantic, jealous) is inert for a minor: the bias is the empty set's;
  a minor's romance topic lean is 0; the default never gives one to a minor; when the
  child grows up the trait becomes active;
* with a minor as the speaker or anywhere in the room (`minor_room`) no trait turns a
  request down harder or stings: `deny` is 0 and `mood` never below 0 (a mean adult is
  gruff to adults only); no romance act gets any lean there;
* the safety acts are never biased; the gate, `mature_adult_ok` and the kid-cast list
  are untouched; grief topics stay sincere (the preview skips them).

### The API the creator calls (exact list)

```c
#include "doomo/talk_traits.h"
/* lists */
const char *talk_trait_name(int t);  const char *talk_trait_desc(int t);   /* "Hot-headed", one sentence */
const char *talk_trait_adj(int t);   const char *talk_trait_topic_name(int root);
int  talk_trait_is_romance(int t);   int talk_trait_conflict(int a, int b);
int  talk_trait_inert(int t, int stage);        /* grey it out: no effect at this age */
int  talk_trait_topic_ok(int root, int stage);  /* romance topic: adults only */
/* the creator's working set */
void talk_traitset_clear(TalkTraitSet *); int talk_traitset_count(const TalkTraitSet *);
int  talk_traitset_has(const TalkTraitSet *, int t);
int  talk_traitset_add(TalkTraitSet *, int t);          /* TTE_OK / TTE_FULL / TTE_DUP / TTE_CONFLICT / TTE_RANGE */
void talk_traitset_remove(TalkTraitSet *, int t);
int  talk_traitset_like(TalkTraitSet *, int root);      /* TTE_OK / TTE_FULL / TTE_DUP / TTE_TOPIC_BOTH / TTE_RANGE */
int  talk_traitset_dislike(TalkTraitSet *, int root);
void talk_traitset_forget_topic(TalkTraitSet *, int root);
int  talk_trait_validate(const TalkTraitSet *, int stage);   int talk_trait_normalize(TalkTraitSet *, int stage);
/* a person's set */
void talk_trait_default(const DoomoResident *r, TalkTraitSet *out);   /* what a record gets by itself */
int  talk_trait_get(int id, TalkTraitSet *out);  int talk_trait_set(int id, const TalkTraitSet *);
int  talk_trait_is_custom(int id);  void talk_trait_clear(int id);
/* the screen: the verdict and the live preview */
const char *talk_trait_title(const DoomoResident *r);       /* the persona type: "Quiet Bookworm" */
int  talk_trait_read(const DoomoResident *r, const TalkTraitSet *ts /*NULL: default*/, char *out, int cap);
int  talk_preview(const DoomoResident *r, const TalkTraitSet *ts /*NULL: default*/, int kind /*TP_GREETING,
                  TP_ANSWER, TP_REFUSAL*/, uint32_t seed, char *out, int cap);
```

How the creator uses it: keep a `DoomoResident` (the sliders, the life stage and the look
seed are in it) and a `TalkTraitSet`. On every change call `talk_trait_read(&r, &ts, buf, cap)`
for the one-line read ("Quiet Bookworm: shy and kind. Loves food; avoids work.") and
`talk_preview(&r, &ts, TP_GREETING / TP_ANSWER / TP_REFUSAL, seed, buf, cap)` for the three
sample lines (fix `seed` per session so the lines do not flicker; change it for a "again"
button). `talk_preview` works on a character that is not in the town: it runs the real
`talk_gen_line` for the temporary id `TALK_PREVIEW_ID` with the player as the other
speaker, and returns 0 when the Talk database is not open (the creator then shows its own
text; check `talk_gen_ready()` first). It is deterministic, allocates nothing, scans at
most 512 concepts, leaves no state behind, and obeys the room's rules (a child preview is
a kid room: the S3 lint replaced none of 1,728 lines over every trait and life stage).
On save, `talk_trait_set(id, &ts)` stores the choice for the new resident; the TALK chunk
carries it. Limits the creator lead must know (TM1 review):

* `TP_REFUSAL` is a refusal since TM2c (6m): the plan's kind is forced to "no" (96 of 96
  previews are `TA_NO`; `talk_preview_line` returns the act so you can check). Before TM2c it
  could be an acceptance. The trait lean on accept and deny is in `talk_trait_bias` /
  `talk_dm_expect`.
* Determinism is per town state: the line also reads the hero, the relationship and the
  day. The greeting may name the player "You" (the preview has no avatar name).
* Explicit trait sets persist only where the `'TALK'` chunk does (the native host). On the
  GBA, PS1 and Dreamcast saves a creator's pick is lost on reload and the person falls
  back to the derived set. The GBA ROM still carries the 24-entry table (168 B of EWRAM
  with the preview state: the GBA ledger row).
* An explicit set outlives its person: `talk_mem_forget` (which clears it) has no caller
  in the sim yet. The creator lead or the town move-out path must call
  `talk_mem_forget(id)` when a resident leaves, or the 24 slots fill (`TTE_FULL`).

The preview traits show through the topic they talk about (their liked root), the stance
lines and, since TM2c (6m), the effective voice and the reaction kind: a refusal preview says no.

### Tests and measurements

`test_talk_traits.c` (13 tests): the table, edit / validate / pack, the default (pure,
valid, spread over 20+ traits, steered by the sliders, no romance trait or "mean" for a
minor), explicit sets, the chunk (old v1 chunk loads unchanged, v2 round trip and byte
identical rewrite, bad tail, gone person, forget, reset), the one-line read, the bias
safety sweep, the leanings, the stance hook, the drama manager hook, the preview (real
engine, deterministic, stateless, steered by the liked topic, safe for every trait and
life stage).

Measured (clean builds, base = origin `382a5fdc`, this branch on top):

| | base | with TM1 | delta |
|---|---|---|---|
| core tests | 1,211 passed | 1,224 passed | +13 tests |
| GBA `.bss` (EWRAM) | 248,748 | 248,916 | +168 B (the 148 B trait table and 20 B of preview state) |
| GBA EWRAM free (memreport) | 12,500 | 12,332 | -168 B |
| GBA `.iwram` / `.iwram_bss` | 11,272 / 4,088 | 11,272 / 4,088 | 0 |
| GBA `.text` / `.rodata` (ROM) | 1,456,360 / 7,600,780 | 1,460,688 / 7,603,316 | +4,328 / +2,536 B |
| PS1 EXE with the boot FMV | ends 0x801ee580 | ends 0x801ee580 | 0 (the EXE md5 is identical) |

The 168 B of GBA EWRAM are the whole cost of explicit trait sets (the record itself did not
grow): where a build has no creator that sets traits, shrink `TALK_TRAIT_CUSTOM_MAX`.
`make -C gba test-talk` and the native script `home_visit` fail on the base the same way
("expected scene TALK, got DIALOG"); they are not caused by this package.

---------------------------------------------------------------------------

## 6l. The coherence lint and the Q8 census check (TM2b)

Spec: `docs/design/talk/TALK_MULTILEVEL.md` sections 7 and 9. No engine change and no line deleted:
the bank is 8,891 alternatives before and after (`talkc --stats`).

### The lint (`core/tools/talkc_lint.py`, family TK13)

talkc derives the layer (a reaction, b content, c colour, d hook, x kept as is) and the reply kind
(accept agree deny refuse deflect askback say) of every rule from its name, its `act:` and its
guards, and reports ten rules. `make -C core talk-check` and every strict `talkc` build run it.

| Rule | Fails when | Policy | Findings now |
|---|---|---|---:|
| L1 | a core rule's `act:` is not a legal reply to its entry act | fails at once | 0 |
| L2 | the reply kind and a `stance=` guard disagree (an agree reads the entry act's side) | ratchet | 0 |
| L3 | a question about a topic (ask_about, ask_opinion, change_topic, gossip) answered with a line that names no topic and has no topic guard | ratchet | 0 (was 57) |
| L4 | "it", "that", "this", "them" with no `topic` guard and no slot (the asked acts and opine_pos / opine_neg) | ratchet | 0 (was 90) |
| L5 | a request (ask_favor, invite) turned down, or a question dismissed, with no reason, counter or question | ratchet | 31 |
| L6 | a filler word or under 12 visible characters as the whole reply, outside backchannel, wave, nod, high_five, laugh, greet, farewell, hug, give, thank | ratchet | 3 |
| L7 | a `follow.*` rule that cannot follow its acts (acts.tsv), repeats the question just asked, or asks about "it" with no topic | ratchet | 5 |
| L8 | `depth>=2` with no `rel=` or `fact.*`; `depth>=3` without best, partner or family | fails at once | 0 (no rule uses depth yet) |
| L9 | a `trait=` guard on an edgy trait (hot-headed, romantic, snob, jealous, mean) without `adults_only&!kid_present`; any trait guard in a refusal, grief, distress, pet, kid or K-7 bank | fails at once | 0 (no rule uses trait yet) |
| L10 | "anyway", "by the way", "speaking of" with no topic slot in a core rule | ratchet | 35 |

The ratchet is `core/data/talk/lint_baseline.tsv` (code, rule, text; 74 lines). A finding that is not
in it fails the build. A baseline line with no finding fails the build too: delete it. So the count only
goes down. `python3 core/tools/talkc.py --lint` prints the table; `--lint-update` rewrites the file
(use it only to remove lines). A fix is a guard (`topic`, `!topic`) or a tag (`tags: generic` with a
reason, `tags: closes` for a door that is meant to close), never a deletion.

Fixes in TM2b: 108 lines got a line guard (86 `guard: topic` where the line says "it" or "that",
22 `guard: !topic` where it says nothing of a topic) and the three `*.notopic` rules got
`tags: generic` ("it" there is not the topic). The same text still exists; it speaks only when it is
coherent.

Seeded bad grammars: `core/tests/talk_bad/gen_lint_l1.tlk` to `gen_lint_l10.tlk`, one per rule
(`talkc --selftest` now refuses 74 seeded inputs, 10 more than before). The selftest also checks the
ratchet: the real bank passes its baseline, the bank plus one new L3 rule fails, and a baseline line
with no finding fails.

### Q8 in the census (report mode)

`core/tools/talk_census.c`: `talk_census_q8()` and the codes `QF_Q8_KIND`, `QF_Q8_CONTENT`,
`QF_Q8_VOICE`, `QF_Q8_HOOK`, `QF_Q8_FILLER`, `QF_Q8_UNKNOWN`. Report mode: the counts are in
`CensusResult.q8[]` (failed) and `q8_n[]` (applied), printed after the S3 line; an exchange that fails
Q8 is **not** a quality failure and `talk_census_check()` never returns these codes. `--q8-fails FILE`
writes each failing exchange (the sample row with the core alternative ids; `talk_sample.py` maps them
to file and line). The old engine has no plan, so each check reads what the trace has:

* Q8a: the line's own stance (pro / con) against the side the reply takes (an opine, an agree or a
  disagree read against the player's opinion), for ask_opinion, ask_about, opine_pos, opine_neg.
* Q8b: the player gave a topic in a topic act; the reply must say the topic or an association, or its
  core alternative's guard must need the topic (a slot, `topic`, likes / fears / hates, stance, root).
* Q8c: voice guards against the sliders as the speaker has them (effective equals stored until TM2c adds
  the effective sliders), and `talk_trait_bias_id(..., minor_room=1)` must give deny 0 and mood >= 0.
* Q8d: a no or a deflection of a request (ask_favor, invite) leaves a question or a hand-off; a resident
  does not hand the player's own question straight back. The follow-up and adjacency checks wait for the
  drama manager in the census (TM2c).
* Q8e: the answering speaker's lines total 20 visible characters or more, unless the player's act is a
  gesture, greeting, farewell, thank you or laugh; never a filler word alone.
* Q8f: an unknown line (TA_UNKNOWN is in the grid) is not answered with a joke, a backchannel or a topic
  change unless a question is asked.

Smoke baseline (20,000 contexts, seed 20261108, this machine):

| | origin (no guards) | after the guards |
|---|---:|---:|
| exchanges with a Q8 failure | 2,165 (10.82%) | 1,543 (7.71%) |
| Q8a reaction | 137 of 509 (26.9%) | 140 of 489 (28.6%) |
| Q8b content | 1,748 of 3,992 (43.8%) | 1,084 of 3,992 (27.2%) |
| Q8c colour | 0 of 20,000 | 0 of 20,000 |
| Q8d hook | 130 of 2,954 (4.4%) | 130 of 2,954 (4.4%) |
| Q8e filler | 104 of 20,000 (0.52%) | 104 of 20,000 (0.52%) |
| Q8f unknown word | 93 of 395 (23.5%) | 93 of 395 (23.5%) |

The weighted share of topic-blind replies (Q8b) fell by a third with the guards. What is left is the 296
generic listener lines of the topic acts (tell, answer, agree, disagree, continue: spec section 9, N1b):
they are left for TM2c and TM2d, which compose the reply from layers. TM2e turns the Q8 budget (Q8a, Q8e,
Q8f at 0; Q8b, Q8c, Q8d at most 0.5% each) into a test.

Measured: GBA `.bss` / `.iwram` / `.iwram_bss` identical to the base, ROM `.rodata` +256 B (108 guard terms); PS1 EXE unchanged (72,336 B below the stack top); core tests 1,244 to 1,246.

Tests: `talk_census_q8_report` (the smoke run prints the table, every check applied, none fails an
exchange, the quality numbers are unchanged) and `talk_census_q8_checks` (each check fails on a built
exchange that breaks its rule and passes on the repaired one). Smoke census with the guards, against
origin: Surface 17,078 against 17,067 distinct, Skeleton 17,016 against 17,008, Meaning 5,130 against
5,126, conversations 1,000 of 1,000 in both, quality 20,000 passed in both, Q5 repeats 1 against 3.

---------------------------------------------------------------------------
## 6m. The plan: reaction kind and effective voice (TM2c-PLAN)

Owner decision 2026-11-15 ("TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN"). Design:
`docs/design/talk/TALK_MULTILEVEL.md` 2.1, 2.2, 2.4 and 13 (TM2c). Layers a (the reaction)
and c (the colour) of the composer. Layer b stays at depth 0 and layer d as it is today.
The line bank is untouched (no line added, none deleted). Files: `core/src/talk_plan.c`,
`talk_gen.c`, `talk_traits.c`, `core/data/talk/acts.tsv`, `talkc_gen.py`,
`core/tests/test_talk_plan.c`, `core/tests/talk_gen/plan.tlk`.

### The plan

An answer line gets one reply kind before its core is picked (`TalkPlan`, 11 bytes, on the stack):
accept, agree, deny, refuse, deflect, counter, ask back (`TK_*`). `talk_plan_kind()` is a
pure function of a `TalkPlanIn` (a test can drive it without a database):

    score(K) = base + 4*lean*sign(K) + rel + mood + voice + 2*trait + topic + noise(0..2)

* `base`: 3 for the kinds the default reply act of `acts.tsv` serves, 1 for the others.
* `lean` (-2..2): the stance as the speaker sees it. `talk_gen_stance` now keeps its score
  (`stance_score`); the liked / disliked topic roots and the traits already moved it (TM1). Lean
  is the score banded: +-2 is a strong stance. `sign(K)` is +1 for accept and agree, -1 for
  deny and refuse.
* `rel`: strangers refuse personal asks, friends accept, best friends, partners and family
  accept and agree, rivals deny and deflect and do not accept.
* `mood`: happy accepts, sad counters, angry denies, tired refuses invites, scared refuses or
  deflects, weird counters.
* `voice`: from the effective sliders (below): blunt denies, polite agrees, easygoing accepts
  and deflects, serious does not deflect, quirky counters and deflects.
* `trait`: `talk_trait_bias` of the speaker's set in this room (accept, deny), twice.
* `topic`: a liked root accepts or agrees (+2), a disliked root refuses or deflects (+3).
* `noise`: 0..2 per kind from a small RNG seeded with the conversation seed, the turn, the
  speaker and the line count: the same talk gives the same plan on every platform.

A kind against the lean (a "no" to what they like) needs a reason that is not the dice:
rel + mood + voice + trait + topic adding to 2 or more (`TPL_REASON`); without one it only
wins when nothing else is allowed. The best three kinds are kept: when no rule of the best kind
passes the guards, `gen_line` tries the next (`talk_plan_next`) before it drops the filter.

**Hard rules, in code.** The plan never runs for a safety act, `TA_UNKNOWN`, a grief topic
(`TCF_GRIEF`), the pet, or a gate refusal (those keep the old sincere path, no plan, no hook).
A romance act is accepted, deflected or refused (`TalkConv.consent` decides; +8 or -8), never
countered or asked back. A hug toward a child or in a kid room is accepted or refused. With a
minor anywhere (speaker, addressee, listener, avatar) the cool reply acts (tease, mock,
insult, threaten) are never picked by a plan line, and a kind only they served is not allowed.

### Which kinds a reply act serves, and what is allowed after an act

`acts.tsv` has a fourth column, KINDS (`-` for none). talkc writes it into the acts table
(`row[14]`: the kinds this act serves as a reply) and computes `row[15]`: the kinds allowed after
a player's act, the union over the reply acts for which the bank **really has a rule** (an empty
cell of the bank allows no kind: `ask_who` has no "answer"). `talk_gen_kinds_of` and
`talk_gen_kinds_after` read them. `ANSWER` and `OFFER` serve accept and counter, `NO` serves deny
and refuse, `AGREE` serves accept and agree.

### The soft filter, and when the plan binds

`pick()` skips a core rule whose reply act does not serve the plan's kind (`kind_serves`), for the
core part only. No rule left: next best kind; none left: the filter drops, the line is made as
before and `TPL_MISS` is set. The census counts misses: 3 of 19,305 answer lines (0.02%).

A filter on every line costs variety: Surface uniqueness in the smoke census falls from 85.7% to
about 82% (restricting the pool of alternatives makes repeats likelier). So a kind **binds** only
where it matters. (1) By margin: 2 points for questions (the `ASK` class: an opinion, a fact; free
in the census) and 8 points for every other act. (2) The **request acts** (invite, favour, ask out,
give) bind whenever the plan overrides the natural reply (a rival says no) or the lean, the
relationship or a trait pushes the kind by 2 points or more (`PUSH_BIND` in `talk_plan.c`):
a yes or a no is the whole answer there. Below that the plan is **open** (`TPL_OPEN`): the kind is
named and recorded but the generator does not filter by it. About 25% of answer lines bind.
Review finding fixed (TM2c land): the first version bound only 22% and the request acts were
mostly open, so a rival said yes to an invitation in 47% of the lines while the plan said no. Line
level now (`plan_relationship_line`, 600 lines each, a bare invitation or favour): rival says yes in
8%, best friend in 100%. The other acts stay advisory by design: the plan colours them through
the bound cases and the preview, and the bank needs more alternatives per kind (bank work, outside this
frozen-bank wave) before a filter on every line is free. Constants: `PLAN_MARGIN_ASK`, `PLAN_MARGIN`
(`talk_gen.c`), `PUSH_BIND` (`talk_plan.c`). Note: when the soft filter walks to the next kind,
`TalkTrace.plan.kind` is the kind that was served (the first choice is lost), and a `kind=` guard
later in the same line sees the served kind. A line can also use the
kind itself: the new guard `kind=accept|agree|deny|refuse|deflect|counter|askback` holds when the
plan has that kind (a rule that must say "no" only when the plan says no).

### Layer c: the effective voice, and `trait=`

`setup()` computes the speaker's effective sliders: the stored sliders plus the trait deltas
(`talk_trait_slider_delta`: twice the table's pull per trait, capped at +-6 a slider), clamped
0..15. The voice guards (`polite`, `blunt`, `flat`, `loud`, `serious`, `easygoing`, `quirky`,
`quick`, `slow`), the opener odds and the exclamation budget read them. `persona_type()` (the
title, the tics) still reads the stored sliders, as does the 16-type `type=` guard.
`talk_gen_eff_persona(id, minor_room, out)` gives the same numbers to tools.

The edgy traits (hot-headed, romantic, snob, jealous, mean) are inert for a minor speaker and when
any minor is in the room: `talk_trait_room` drops them from the set the engine reads, so they
change no slider, no stance, no bias, no plan and no `trait=` guard. The new guard `trait=shy`
(and the other 23 names: `hotheaded romantic goofball snob neat slob bookworm cheerful gloomy
jealous loyal mean kind lazy ambitious paranoid self_assured shy outgoing creative glutton morbid
curious honest`) holds when the speaker has that trait in this room. No bank line uses it yet.

### The census

`QF_Q8_KIND` (Q8a): the reply act serves the plan's kind (unless a miss or an open plan), the kind
is allowed after the player's act, a kind against the lean has a reason, a romance act is not
countered, a hug in a kid room is accepted or refused, no cool reply act in a kid room.
`QF_Q8_VOICE` (Q8c): every voice guard of every alternative used holds for the effective sliders
(re-derived from the room), the trace's own record of them agrees, a `trait=` guard holds. The
Q3 voice check and the exclamation budget now read the effective sliders. The report prints the
plan line: answer lines, open, misses, kinds. The smoke test asserts 0 Q8a, 0 Q8c, misses under
1%, and Surface / Skeleton at least 84.5% / 84.2% unique.

### The creator's preview

`talk_preview` (and the new `talk_preview_line`, which returns the engine's `TalkLine`) force the
plan's kind: a greeting accepts, an answer agrees (or denies a topic they dislike), a refusal
says no (bits 1-3 of `TalkConv.plain`, 0 bytes). A refusal preview says no in 96 of 96 lines
(every trait, four seeds; `line.act == TA_NO`). The traits colour the line through the voice and
the stance; two different trait sets give a different line in 44 of 90 previews.

### Tests and measurements

`test_talk_plan.c` (13 tests): the pure function (hard rules, the reason rule, the order),
stance (lean +: 1,871 of 1,871 accept or agree; lean -: 1,163 of 1,219 deny or refuse),
trait bias (kind accepts an invitation 100% against mean 53%; lazy refuses 91% against ambitious
36%), relationship (plan: rival accepts 3%, best friend 100%; produced lines for invitations and favours: rival says yes 8%, best friend 100%), sincere bypass (grief, safety, unknown,
pet, gate refusal: no plan), kid room (no cool reply act in 1,500 lines; 5 edgy traits x 4 stages
x 13 acts change neither plan, voice nor text), determinism, the guards `kind=` and `trait=` in the
probe grammar `core/tests/talk_gen/plan.tlk`, the soft filter counting a miss, the effective voice,
the preview, the acts table. Note: the sim fills some facts lazily on the first read, so the
very first line of a fresh town can differ from the next identical call.

| | base `04f41ef5` | with TM2c |
|---|---|---|
| smoke census Surface / Skeleton / Meaning (distinct of 20,000) | 17,067 / 17,008 / 5,126 | 17,094 / 17,033 / 5,057 |
| conversations distinct / Q5 repeats | 1,000 / 3 | 1,000 / 2 |
| quality failures, S3 hits | 0, 0 | 0, 0 (Q8a 0, Q8c 0) |
| soft-filter misses | | 3 of 19,305 answer lines |
| MIPS `talk_gen.o` `.text` (gcc -O2, r3000) | 26,664 B | 29,704 B |
| MIPS `talk_plan.o` / `talk_traits.o` `.text` | | 2,896 B / 14,224 -> 15,008 B |
| stack: `talk_gen_line` + `gen_line` (MIPS) | 456 + 1,472 B | 592 + 1,536 B |
| GBA `.text` / `.rodata` (ROM) | 1,486,136 / 7,604,336 | 1,489,944 / 7,604,484 |
| GBA `.iwram` / `.iwram_bss` / `.bss` / `.data` | 11,272 / 4,088 / 249,084 / 816 | unchanged |
| PS1 EXE with the boot FMV, below the stack top | 72,336 B (`_end` 0x801ee570) | 72,336 B (`_end` 0x801ee570); on the final rebased tree 72,368 B (`_end` 0x801ee550), the same as origin |

The stack of one line grows by 200 B (`Gen` carries the plan and the trait set); the generator's
budget is about 5 KB. Talk is not linked on the PS1 yet, so the PS1 cost is 0 B.

## 6n. Topic depth, hooks and the unknown word (TM2d-DEPTH)

Owner decision 2026-11-15 ("TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN"). Design:
`docs/design/talk/TALK_MULTILEVEL.md` 2.3 to 2.5, 3, 4, 10 and 13 (TM2d). Layer b past the surface
(an opinion with its reason, a story, a secret), layer d (the hook), the unknown word named, guessed
or asked about, and `TEV_LEARNED`. Files: `core/src/talk_plan.c` (the gates, pure), `talk_gen.c`
(the parts, the second line, the unknown word), `talk_dm.c`, `talk_save.c`, `talk_outcome.c`,
`talk_lex.c` (`talk_lex_near`), `talk_parse.c` (the unknown word's offsets), the grammar files
`depth_reason.tlk`, `depth_story.tlk`, `depth_secret.tlk`, `hooks.tlk`, `unk.tlk`,
`core/data/talk/corpus/echo_en.tsv`, `core/tests/test_talk_depth.c`.

### Depth (design 3.1), in code

`talk_depth_gate()` and `talk_depth_step()` are pure functions of a `TalkDepthIn`. A participant's
thread is a byte and a word in `TalkConv` (`dep[]`, `dep_tw[]`): level (2 bits), thread turns (3),
hook used (1), the thread's topic (12), the last reply kind (3). The generator fills the input from the
room, the sim (`rel_value`) and the traits, and calls the step once for every answer line (`depth_plan`).

| Level | Says | Needs |
|---|---|---|
| 0 | the surface: a one-line answer | nothing |
| 1 | the opinion plus **one reason** (a `reason.*` clause joined to the answer) | not shy / paranoid with a stranger; a rival stays at 1 at most |
| 2 | **a story** (`story.*`, a second line by the same resident) | acquaintance or better (a stranger never), 2 thread turns, engagement 50; shy / paranoid / gloomy want a friend, outgoing wants one band less (never a stranger) |
| 3 | **a secret** (`secret.*`, a second line) | best friend, partner or family, or a friend whose bond is 40 (a loyal friend needs none); 3 thread turns; engagement 70; no trait lowers it; never on a grief topic |

It rises one level at most an exchange on a thread act (ask about, ask opinion, ask feeling, tell, opine,
continue, agree, sympathize, compliment, comfort) on the thread's topic or one hop from it; holds on a
change of topic inside the root; falls one level after an insult, a mock, a dismissal or a threat; resets on
another root, on a walk-out and on a K-5 refusal (the drama manager calls `talk_depth_reset`). The first
turn on a topic is the surface: a best friend with engagement 70 is at 0, 1, 2, 3 on the first four turns.

**The second line.** A story or a secret is a second line by the same speaker (`talk_gen_more`, flag
`TLF_MORE`), made right after the answer: in `talk_dm_step` (a `DK_MORE` entry inserted after the answer,
before the reactions, so a reply that answers beats a reaction from the side) and in the census path
(`talk_gen_exchange`). Its reply act is the first of answer, tell, opine_pos, agree, backchannel that may
follow the player's act (`acts.tsv`), so Q2 holds; the census checks a `TLF_MORE` line against the player's act.
`secret.*` tells **at most one secret a talk** and never again the same day: a like, the fear or the hated
food the player has not learned (`talk_mem_like_known`), or a small problem from the safe set (hungry,
dirty, bored, sick, wants an item, wants a friend). Never a crush, a break-up, a proposal, possession,
corruption, a nightmare or a fight; a child tells a child's fear, favourite or worry in a kid voice. A friend
or partner or family that passed the gate counts as `rel=best` for the secret rules (the lint, L8, wants a
close relationship on every depth 3 rule).

**Memory.** `TalkMem.spare` (16 bits, was 0): bits 0-1 the level reached in the last talk, bits 2-5 the
secrets told today (cleared when the town day changes). Saved in the 'TALK' chunk (the entry is 12 B as
before; an old chunk reads 0: depth 0, nothing told; no version bump). The next talk starts one level lower
on the last topic. `TEV_LEARNED` (a told, b the like: 0 food, 1 hobby, 2 music, 3 fear, 4 hated food, 5 a small
problem) is raised when a secret is told; `talk_outcome_apply` calls `talk_mem_know_like` (the open item of
T6-OUT), so the Talk screen lists the topic for the player afterwards.

### Layer d: the hook (design 2.5)

A hook is a short second **part of the answer line** (`hooks.tlk`, entry `TGE_HOOK`, guard `hook=`), said after
the callback and the tag. Required after a counter (a question about the topic, or a call that names another
resident) and after a request the resident turns down (an invitation or a favour answered with no, a dismissal
or a change of topic: an offer, "Rather not. How about {like} instead?"). Never after a romance act, a safety
act, a sincere line or the pet. A small acknowledgement (`hook.ack`, `hook.thank`, `.apologize`, `.compliment`,
`.feeling`) is added to a reply under 20 visible characters (a bare "Hm.") and, 30% of the time, after a thanks,
an apology, a compliment or a question about a feeling. The generator makes room for a required hook: the
optional parts go first (tag, hand-off, opener, callback), then a shorter hook, then a shorter core, and only
last the hook itself. The drama manager's `follow` line is skipped when the answer already asks something back.
`reason.name` (depth 0) gives a topic act that was answered by a line that names nothing (a listener line) the
topic back in a few words (Q8b).

### The unknown word (design 4)

`talk_parse` points at the first unknown word (`TalkUtter.unk_off`, `unk_len`, 2 B at the tail: E1, E3). The
drama manager and the census run `talk_gen_unk_prepare` once a turn after the gate; it sets the plan of the
answer (`TU_*`): **guess** (`talk_lex_near`: the nearest known concept by edit distance 2, 1 for a word of four
letters or less, or a shared four-letter stem; closed vocabulary, always safe), **name** (the word said back,
quoted and lower case, once a talk), **ask** or **ask without an echo**. Every line asks what kind of thing it is.
The fences, in `talk_gen_unk_word`, checked on the bubble text (`shown`, S1-masked) and never on the raw input:
E1 the parser's token is unknown, not masked, not leet, not a typo of a known word; E3 3 to 16 ASCII letters and
at most one apostrophe or hyphen, no digit, not shouted; E4 not a hate word (`talk_safe_hate_word`, fuzzy), not
(**Review fix, landing:** the one-edit fence missed misspelt slurs, sexual, fetish and self-harm words. `unk_stem_bad` in `talk_gen.c` now folds the word (k/c, z/s, y/i, repeats) and searches it for about 190 folded stems; a hit means no echo and no guess, a plain ask. 38 probe words (nigguh, darkie, gypsy, twatwaffle, cunty, tittyz, xvideos, vore, zoophile, anorexic, bulimic, abo and more) are in `echo_en.tsv` as `no` for both rooms. The net is wide on purpose: a harmless nonsense word with a stem inside, such as `snig...`, is only asked about. **TM2d is partial:** Q8a 123 of 562, Q8b 3.2%, Q8e 5 of 20,000 are above the spec 7.2 budgets (0, 0.5%, 0); `test_talk_census.c` holds a temporary ratchet (Q8b 5%, Q8e 0.1%) to tighten in TM2e.)
within one edit of a register, sexual, hate, minor, distress or real-world word, and no such word inside it
("hitlerish"); E5 no safety bit, not a K-7 room, not in the care window; E6 the word and the whole line pass the
S3 probe (a refusal there falls back to the ask); E7 once a talk. A kid room (a minor speaks, is addressed or
listens) echoes no word over 12 letters. `{u}` (the quoted word) is valid only in `unk.name` rules (talkc: TK13).
"It's a food": the next turn's category is taken (`unk.taught`) and remembered for the talk in a ring of four
(`taught_hash`, `taught_root`, 12 B in `TalkConv`, never saved). The PS1 has no typed input, so the unknown flow never
runs there; the lines are data in the Talk bank.

### Where it lives and what it costs

`TalkState` 940 B -> 964 B on native, the Dreamcast and the PS1 (the assert is 1,536; the spec's budget under 1,000),
**932 B on the GBA**: `TalkConv` +36 B (`dep` 4, `dep_tw` 8, `mem_dep` 4, `engage` 1, `more` 2, and the unknown-word state
33 B: `unk_st`, `unk_guess`, `unk[18]`, the ring of 12) and `TalkUtter` +2 B, less 32 B for `TalkHist.tmpl`, an unused
word of 16 history entries that nothing read. **The GBA's EWRAM is net zero**: the Talk screen's `static struct TalkScn T`
(`.bss`) holds a `TalkState`, so the growth was real there (+56 B, the spec's "0 resident bytes" did not count the
scene's own state); the GBA has no typed line (`TALK_TYPING`), so `TALK_NO_UNK` (set by `DOOMO_PLATFORM_GBA`) leaves the
unknown-word fields out of `TalkConv` there and `.bss` is 249,084 -> 249,076 B; `.iwram` 11,272, `.iwram_bss` 4,088 and
`.data` 816 B unchanged. The depth state is in `TalkConv`, not in `TalkPart.pad` as the design said, because the generator
(and the census) must read it without the drama manager. `TalkPlan` 11 -> 14 B (depth, content, hook; stack only).
`TalkOutcome` + `learned`. 0 resident bytes on the PS1 (Talk is not linked there). `TALK_en.BIN` 902,268 B -> 907,644 B
(119 new alternatives in 32 rules). The generator's stack: `gen_line` 1,536 -> 2,096 B (7 parts, `AVOID_MAX` 6 -> 10),
`talk_gen_more` 568 B (a separate call, not nested).

### Tests and measurements

`core/tests/test_talk_depth.c` (13 tests): the pure gates and steps, a best friend 0-1-2-3 with exactly one real
secret, friend 39 against 40, engagement 69 against 70, a stranger never 2, a rival never past 1, the resets, the
memory after a save and a load, 2,000 sessions with children (7,660 deep turns, 1,277 secrets, no lint hit, no
register word), every stage x problem kind, the drama manager's second line and `TEV_LEARNED` applied to the sim,
200 invented words, the adversarial corpus and `echo_en.tsv` (no hate, register, minor, leet, fuzzy, person, digit or
shouted word comes back out; a kid room says nothing over 12 letters), "it's a food", every refused request and every
counter has a hook, `talk_lex_near`, the parser's offsets. The smoke census (20,000 contexts, seed 20261108):
Q8 exchanges with a failure 1,534 (7.67%) -> 254 (1.27%); Q8a 120 of 560 -> 123 of 562 (not at budget: the failing lines carry the bank's own act labels and TM2c's plan leaves a close call to the bank), Q8b 1,071 of 3,992 (26.8%) -> 126 (3.2%), Q8c 0, Q8d 156 of 2,976 (5.2%) -> 0,
Q8e 104 -> 5 (a three-sentence line cannot take a fourth, lower-case voices), Q8f 91 of 395 (23%) -> 0. Surface 17,094
-> 17,146 distinct (85.7%), Skeleton 17,033 -> 17,038, Meaning 5,057 -> 5,180; S3 hits 0.

## 6o. Closing the Q8 budget (TM2F-Q8)

Owner decision 2026-11-15; design `docs/design/talk/TALK_MULTILEVEL.md` 7.2. TM2d left the smoke census (20,000
contexts, seed 20261108) at Q8a 123 of 562, Q8b 3.2%, Q8e 5 of 20,000. Now all of them are inside the budget, and
`test_talk_census.c` holds the spec numbers (Q8a 0, Q8b and Q8d at most 0.5%, Q8e 0, Q8f 0).

**Q8a: the side of an opinion answer.** Two causes.
1. The plan's kinds are signed by the topic (agree is a yes to it, deny a no), but AGREE and DISAGREE are signed by
   the player's claim. After a negative opinion the plan said deny (the speaker dislikes it too) and the core was
   picked among the rules that serve deny, and DISAGREE is one of them: it says yes to the topic. `talk_gen_kinds_reply
   (player_act, reply_act)` reads the acts.tsv kinds against the player's act: after an OPINE_NEG an AGREE serves deny and
   a DISAGREE serves agree. The generator's kind filter and the census use it. `talk_gen_reply_side` is the one
   definition of "which side of the topic a reply act takes" (the census had a copy).
2. An open plan (a close call) filtered nothing, so a speaker who dislikes a topic could pick a "yes, I love it" core. A
   second soft filter, `KF_STANCE`, drops the cores whose reply act takes the side the speaker's stance is against, for an
   answer to ask_opinion, ask_about, opine_pos and opine_neg, unless the plan went against the lean for a reason
   (`TPL_REASON`). It applies whether or not the plan is open. It goes first when no rule passes the guards (then the kind
   filter, then the miss), so a miss is still counted and never leaves a line out. Surface stays 85.7% to 85.9%.

**Q8b: the topic back.**
* The census asks for the topic only when the room may hear it (`talk_gen_concept_ok`); a kid room cannot be told
  "tattoo parlour" and the generator rightly drops it.
* A bridge (`reason.name`) that does not fit is replaced by a shorter one before another core is tried (the same as the
  hook did already).
* A story line of three sentences ("Right, so where was I? Ah, yes. The part with the teapot.") cannot take any bridge and
  stay inside three sentences. Those lines (6 in `rest_talk.tlk`, 2 in `core_other.tlk`, 2 of Lord Vapula's) now carry
  `guard: !topic`: they are said when there is no topic. The new file `q8_topic.tlk` has the companions for the topic
  case (9 lines in 5 rules, each names `{x}`, two or three sentences, nothing aimed at the player, one lower-case
  rule for Lord Vapula).
* 9 of 3,979 are left (0.23%): single listener lines of two sentences that run out of room for the bridge. Not worth a
  rule each; the budget is 0.5%.

**Q8e: short replies.** `hook.ack` gets five short lines (16 to 24 characters); the acknowledgement now tries another one
that fits before it sheds the tag, the callback or the opener (Lord Vapula's `hmph. *a small rumble...*` and a bare
"Yep." both went out short because the first pick was too long). The hook "And you, {you}?" is now "And what about you,
{you}?" (with a three-letter name it was under 20). One legacy text was touched: the blunt "Okay. Done. Next." (17
characters, three sentences, no room for an acknowledgement) is "Okay. Done. Next job.".

| Smoke census, 20,000 contexts | TM2d (before) | TM2F-Q8 |
|---|---|---|
| Q8a reaction | 123 of 562 (21.9%) | 0 of 501 |
| Q8b content | 126 of 3,992 (3.2%) | 9 of 3,979 (0.23%) |
| Q8d hook | 0 of 2,978 | 0 of 2,978 |
| Q8e filler | 5 of 20,000 | 0 of 20,000 |
| Q8f unknown word | 0 of 386 | 0 of 386 |
| exchanges with a Q8 failure | 254 (1.27%) | 9 (0.04%) |
| Surface / Skeleton unique | 85.7% / 85.2% | 85.9% / 85.4% |
| quality failures, S3 hits | 0, 0 | 0, 0 |

`--q8-fails` rows now start with the plan's kind, flags and lean and the line's act and stance. Tests:
`plan_kinds_reply_side` and `plan_stance_line` (`test_talk_plan.c`), the tightened census test.
Not changed: `talk_outcome.c`, the sim, `mature_adult_ok`, the kid-cast list. `TALK_en.BIN` 907,644 -> 908,284 B (data, PS1
does not link it); GBA RAM unchanged (the new code is ROM).

---------------------------------------------------------------------------
## 6p. Sim actions a reply can carry (TM2e-ACTIONS)

Owner decision 2026-11-15 ("TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN"). Design:
`docs/design/talk/TALK_MULTILEVEL.md` 6 and 13 (TM2e, tests 1 to 4 and 7). A reply can now do
something as well as say it. The rule is code and is tested: **a line says it only if the sim will do it,
and the sim does it only if the line was said.** Files: `core/src/talk_act.c` (new, the pure rules),
`talk_dm.c` (the plan and the line), `talk_gen.c` (`talk_gen_action`), `talk_outcome.c` (the caps and the
apply), `sim_nudge.c` (new, the sim hook), `talk_scene.c` (call over), the grammar file
`core/data/talk/grammar/en/actions.tlk` and `hook.gift` in `hooks.tlk`, `core/tests/test_talk_actions.c`.

### The actions

| Action (`TACT_*`) | Line | Event | Sim effect | Cap |
|---|---|---|---|---|
| accept an invitation | the answer, then `act.start` (9 alternatives, with the venue or without) | `TEV_ACT_INVITE` (slot, `ACT_*`) | `sim_nudge_activity(id, venue, 60)`: the resident's activity is the venue's until the next hour change; the house plays the walk | 1 a resident a day, 3 a town day |
| refuse an invitation | the existing refusal and its hook (offer) | none | none: the plan only says no (`TalkPart.actp` = `TACT_REFUSE`, `actw` = why) | costs nothing |
| gift | `hook.gift` (4) from a best friend, partner or family member, at depth 2 or in a good mood (35% a turn) | `TEV_ACT_GIFT` (slot, item) | `inv_add(item, 1)`: their liked food as an item, never a hated one | 1 a resident a day, 3 a town day |
| ask for an item | `act.ask_item` (4) from a glutton | `TEV_ACT_ASK` (slot, item) | `sim_problem_raise(PROB_WANTS_ITEM)`, only without an open problem and with room in the queue | 1 a resident a day |
| call someone over | `act.call` (6) from an outgoing or curious resident, turn 2 on (25%) | `TEV_ACT_CALL` (slot); who is `TalkState.call_to` (id + 1) | the scene's `invite(id)` (the person joins the room, room size 4); `sim_introduce` when they are strangers of the same kind | 1 a conversation |
| walk off | the existing excuse (`TLF` action `TACT_WALKOFF`) | `TEV_WALKOUT` (exists) | exists | exists |

One action a reply, never after a refusal, a sincere or safety line, a romance act or the care window.
`TalkLine.action` says which action a line carries (4 bits packed with `stance`: 0 B). `talk_gen_action()` makes
the line through the guard `hook=` (`TH_START`, `TH_ITEM`, `TH_GIFT`, `TH_CALLER`) and the new entry `TGE_ACT`.

### Plan time and apply time

* **Plan time.** An invitation is asked BEFORE the answer is planned (`invite_plan` in `talk_dm.c`): the venue
  (the topic's venue, else the resident's favourite hobby venue, else the park, else the cafe), the rule
  (`talk_act_invite_ok`), the caps (`talk_outcome_why_in`: the ledger of the day plus the events this talk already
  holds) and the sim (`sim_nudge_ok`). If any says no, the talk's `TalkConv.plain` gets `TPLAIN_NOYES` and the plan may
  not pick accept or agree: the answer is a refusal with its offer hook, `TalkPart.actw` says why (`TAW_CAP_DAY`,
  `TAW_CAP_TOWN`, `TAW_RULE`, `TAW_SIM`). A refused action costs nothing and changes nothing.
* **The line and its event are one step.** `DK_ACT` (a second line right after the answer, like the story) adds the
  event only when `talk_gen_action` made the line (no S3 replacement, no fallback) and the event table has room.
  A failed line makes no event.
* **Apply time.** `talk_outcome_apply` re-checks the cap and the minor rule from the summary (a minor anywhere in the
  room, or the player) and does the sim call; what it refuses is counted in `TalkOutcome.skipped` (a test fails on
  any). The ledger gained two bytes (`invn`, `giftn`, in padding) and three flag bits per resident (in `LedP.flags`):
  0 B more.
* **The sim hook** `sim_nudge_activity(id, act, minutes)` (`core/src/sim_nudge.c`, SIM's code written small by TALK;
  the SIM stream is told in its status file). It keeps no state: it sets `activity` for a living resident who is not
  asleep, in the hospital, haunting, possessed, a ghost or worn out, at an hour and chapter the venue is open (the
  same story gate as `decide_activity`); `decide_activity` chooses again at the next hour change, which ends it.
  Console builds drop the file with the rest of Talk (`--gc-sections`).

### Minor and room rules (code in `talk_act.c`, tables in the tests)

* **Invitation.** A date (`TA_ASK_OUT`) is never an invitation action (romance has its own consent path). With a
  minor in the room (the speaker, a listener or the player) only the park, the cafe, the church and the beach
  between 8:00 and 18:00; never the vape shop, a concert hall, the shop or the workshop. Adults: any open venue;
  the vape shop only in an adult room.
* **Gift and ask.** A minor on either side: food or a toy (plushie, board game, video game), never an item flagged
  adult (love letter, perfume, ring, vape pen, chill pill, energy drink, coffee). A resident gives or asks only for
  their liked food that is an item; a hated food or an adult food for a minor is no gift and no ask.
* **Call over.** Never the dead, a ghost or the pet; never a partner (dating, married) of anyone in the room when
  a minor is in the new room, and never a minor into a room that holds a couple. The caller names no one: "I'll
  go get someone" (the toast says who joins).
* A hug toward a child stays a plain hug the child may decline: no action is ever attached to it.

### Tests (`core/tests/test_talk_actions.c`, 6 tests)

`action_says_only_if_done` (3,000 sessions: every line with an action has its event and the reverse, `skipped` 0,
walk-offs match `TEV_WALKOUT`), `action_caps` (1 a resident a day, 3 a town day, gift 1 and 3, ask 1; the second
invitation is refused with `TAW_CAP_DAY` and the first line is a plain yes in 0 of 30 seeds, 30 of 30 without
`TPLAIN_NOYES`; the town day rolls), `action_minor_rules` (the exhaustive act x venue x minor x hour, item x giver x
taker and call tables; a child at night asked to the beach, the vape shop alone with an adult and with a child
around, the partner never called into a child's room and called in an adult room), `action_sim_effects` (the nudge
sets the venue's activity, a gift moves exactly one item, an ask raises one `PROB_WANTS_ITEM` and never over an open
problem, a call-over introduces strangers), `action_sim_nudge`, `action_p2_p3_p7_random` (1,500 random sessions with
traits and actions on, 1,046 with a minor in the room: no couple made, no unsafe invitation or gift),
`action_not_after_hostile` (review fix: after an insult, threat, complaint, blame, mock, dismiss, comfort or sympathy
from the player there is no gift, ask or call-over line; 400 sessions, 0 lines; fails without the fix).
Review fixes also: the 8:00 to 18:00 limit with a minor holds for the park, cafe and church too (not only the beach), and
`sim_nudge_ok` refuses a resident with hunger under 28 (`decide_activity` would send them to eat at once). Known: the
nudge is best effort and lasts to the next re-decision; the call-over is refused by the plan when the room is full, but no
scripted scene test plays one end to end.

## 6q. Residents raise requests (TQ-QUESTS)

Owner decision 2026-11-15 ("TALK QUESTS AND A CLEANER, PRETTIER UI", part 1). Now and then, not every conversation,
a resident raises a request while you talk. You can accept, decline, haggle (what is in it for me) or ask more, through
the multilevel reply. An accepted request is a row in the quest log and Talk remembers it. Files: `core/src/talk_quest.c`
(new: the rules, the table, the pace), `talk_dm.c` (raises it, answers the four replies), `talk_gen.c`
(`talk_gen_quest`, the slots `{target}` and `{item}`, the guards `qkind=` and `qphase=`, the entry `TGE_QUEST`),
`talk_outcome.c` (applies the events), `talk_save.c` (the memory bits), `talk_scene.c` (the four quick replies),
`story_questlog.c` (the log rows, through weak hooks), `core/data/talk/grammar/en/quests.tlk` (91 alternatives in 27 rules),
`core/tests/test_talk_quests.c` (12 tests), the script `core/tests/scripts/home_talk_quest.txt`.

### Sources and kinds

| Source (`TQSRC_*`) | Kind (`TQK_*`) | What it asks | Ready when |
|---|---|---|---|
| the sim's problems queue | `FETCH` (`PROB_HUNGRY`; `PROB_WANTS_ITEM` asks for that item) | bring them a snack | the item is in the bag |
| the sim's problems queue | `COMPANY` (`PROB_BORED`, `PROB_WANTS_FRIEND`, `PROB_LONELY`) | keep them company | a later talk of 3 player turns with them |
| the sim's problems queue | `PEACE` (`PROB_FIGHT`, adults only) | make up with the other one | a mediation, or the two are no longer on bad terms |
| a favour | `FETCH` (their liked food, else a cupcake), `TOY` (a child's), `MESSAGE` (say hi to a friend of theirs), `COMPANY` | | the item; a talk with the one named; a talk of 3 turns |
| Talk-born | `CHEER` (cheer up a friend who is sad or angry now), `SECRET` (keep it a day; adults, best friends only), `PEACE` (a rival) | | the friend's mood went up in a talk; a day has passed |

The pick is weighted (a problem the sim already has counts 3 times as much; traits lean it: a lazy or glutton giver
asks for a fetch, an outgoing one for company or a message, a shy one to carry a hello, a kind or cheerful one to cheer
someone up). What a request is ABOUT (the item, the person) is not stored: `talk_quest_derive` finds it again the same
way every time (the food they like, the friend or rival with the smallest hash), so the line, the log and the hand-in
always agree. A request is never about someone standing in the room.

### Pacing (the drama manager's part)

`talk_dm.c quest_after` plans the request as a second line right after an ordinary answer, and only when all of this
holds: a calm warm exchange (turn 2 on, tension at most 2, engagement 50 or more, no argument, no problem, rivalry,
romance or wind-down beat); the player's act is not hostile, vulnerable (comfort, sympathy, distress) or an accusation;
the care window is shut and the speaker lost nobody lately and no grief topic is in the thread (K-10, K-11); the answer
was not a no; and the pace allows it. **The pace** is one byte (the spare byte of the talk memory header, not saved):
at least one conversation since the last raise (so never twice in a row, and a fresh load never raises in its first
conversation), none yet this town day, fewer than 4 open requests, then 14% of the calm answers. A resident holds one
request at a time; after a hand-in or a no there is a 3 day cooldown. Measured in `quest_pacing` (300 conversations of 7
neutral turns, a town day every 4th): 60 requests (20% of the conversations), never two in a row, never two in a day.
`quest_kid_sessions`: 294 requests in 600 sessions with a child as the giver or in the room (55 of them toys), 0 unsafe.

### The four replies

While a request is on the table the quick replies are its four: "Sure, I'll do it." (`TA_YES`), "Sorry, I can't."
(`TA_NO`), "What's in it for me?" (`TA_ASK_FAVOR`), "Tell me more." (`TA_ASK_ABOUT`). Typed or picked acts map through
`talk_quest_reply_class` (yes, agree, promise, offer, nod accept; no, disagree, shake head, dismiss decline; ask-favour
haggles; the questions ask more). The giver answers in their own voice (`quests.tlk`, phases `accept`, `decline`,
`haggle_yes`, `haggle_no`, `more`):

* **Accept**: event `TEV_QUEST_ACCEPT` (slot, kind, bit 4 = the haggle won); the apply adds it to the table.
* **Decline**: no guilt (the lines say so); `TEV_QUEST_DECLINE`, a little bond lost (-1), a 3 day cooldown. Never a mark on a child.
* **Haggle**: once a request. The giver gives in by their nature (`talk_quest_haggle`: the bond band, kind, loyal or
  cheerful +, mean, snobbish, gloomy or ambitious -, strangers and rivals never; one tie-breaking coin). A yes pays half
  again (`reward x 3 / 2`, bit stored in the table); a no leaves the request standing ("take it or leave it").
* **Ask more**: an explanation per kind (`more.<kind>`); the request stays and the four replies come back.

Anything else the player says lets the request lapse with the conversation (nothing is lost: it was only raised; the
pace counts it). The talk after an accepted request opens with a hand-in line when the request is ready (`done`, the
reward, +3 bond, a headline), a reminder when it is still open (`late`, 60%) or a "never mind" when it ran out of days
(`expired`, -1 bond).

### Memory, the quest log, outcomes

* **Memory: 0 B more.** The request is in the 10 high bits of the giver's `TalkMem.spare`: kind (4), state (2: active,
  ready, settled), bonus (1), days left (3). The chunk keeps its 12 B an entry (`TALK_SAVE_WORST` unchanged, `SAVE_EXT_MAX_TALK`
  448 untouched); an old chunk has 0 there and reads as no request; a bad field is dropped at load. An open request waits
  7 town days (`TQ_DAYS`), then it is "settled with 0 days" (a "never mind" waiting to be said); cooldowns count down
  to nothing; a save from long ago has every wait run out. `talk_mem_end` keeps the bits and an entry that holds a
  request is never the one a 25th person evicts. The pace byte is the unused byte of the table's header (`talk_mem_pace`).
* **Quest log.** `story_questlog.c` has no add function (its rows come from the story's tables), so the log reads
  `talk_quest_log_count` / `talk_quest_log_row` (weak defaults in `weak_talk.c`: none where Talk is not linked). The
  Side tab lists open requests first ("Bring Nana a cupcake.", "Go back to Nana.") with the giver, "Request", the reward
  and the days left. The list buffer is shared (`LIST_MAX` unchanged; the story rows leave room for 6 requests): GBA EWRAM
  unchanged. This is the only line of the story stream's file Talk touched, say so to STORY.
* **Outcomes** (`talk_outcome_apply`): `TEV_QUEST_ACCEPT` -> `talk_quest_add` (the rules again, 4 open at most);
  `TEV_QUEST_DONE` -> `talk_quest_complete` (the item taken or the sim's own problem relieved through
  `sim_problem_resolve`, H$ paid: 20 fetch, 12 toy or message, 10 company or secret, 15 cheer, 30 peace, half again with the
  bonus), the bond (through the daily caps like every bond change), and one headline `NT_TALK_QUEST` for an adult giver
  (never a child); `TEV_QUEST_LAPSED` -> `talk_quest_lapse_ack`. `TalkOutcome` has `quests_new / declined / done / lapsed`
  and `reward`. After the summary `talk_quest_end` marks what the talk met: a message told, someone cheered up, time
  spent, a fight made up, a secret kept a day.

### Kid rules (tables in `talk_quest.c`, exhaustive tests)

A minor giver, or a minor in the room or at the controls, gets only a snack, a toy, a message, company or cheering up,
and a message or a cheer-up is always about someone of the giver's own stage (never across). `TOY` is a child's
request. `PEACE` and `SECRET` are for adults in adult rooms only. No kind is a place, a romance, a date, an adult item
or a danger (an item must be a food or a plushie or board game, never `talk_act_item_adult`, never a key item), so a
child is never sent anywhere and never asked for anything an adult must not ask a child. There is no romance or date
kind in any room. Nothing weakens `mature_adult_ok` or the kid-cast list (neither is read or changed here). Grief, the care
window and hostile or sad exchanges never raise a request. The hug rule is untouched.

### Tests (`core/tests/test_talk_quests.c`, 12 tests, each fails without its code)

`quest_kind_tables_kid_safe` (kind x giver x room x target, every item), `quest_reply_classes`,
`quest_pick_every_resident` (every resident, both room kinds, 8 salts: the rules hold, the derive agrees),
`quest_sources` (hungry, wants an item, bored, wants a friend, lonely and a fight become their requests; a problem
leans the pick; a cheer-up only for someone who is down), `quest_lines_every_phase` (every phase x kind, adult and a child
in a kids' room: a line, lint clean, no swear, no hole, the article right), `quest_pacing` (the counters; 300
conversations, never two in a row, one a town day), `quest_never_when_hurting` (hostile, distress, comfort and grief
topic sessions: 0 requests; the control talk raises some), `quest_accept_and_hand_in` (the session end to end: the
four quick replies, the table, the log row, "Go back", the thanks line, the item taken, H$ paid, the bond, the headline,
the cooldown, a second hand-in does nothing), `quest_decline_haggle_more`, `quest_ready_by_kind`,
`quest_memory_and_save` (every value through the chunk, an old chunk, a bad field, the days, eviction, 0 B more),
`quest_kid_sessions` (600 sessions with a child as the giver or in the room: only kid kinds, no unsafe line). The script
`home_talk_quest` plays one accepted request through SCENE_TALK with the pad (arm, two compliments, the four replies,
"tell me more", accept, leave, the quest log's Side tab, the next talk's thanks) at 240x160 and 320x240; the runner gained
`talk-quest-arm`, `talk-quest-give`, `expect-talk-quest` and `expect-talk-quest-goal`.

### Costs

`TalkState` unchanged (964 B native; `q_kind` and `q_st` sit in two padding holes of the struct). Talk memory: 0 B more
(the request is in `spare`, the pace in the table header's spare byte). GBA: EWRAM and IWRAM identical to the base
(`make -C gba sizes`: `.bss` 254,836 B, `.iwram` 11,272 B before and after; the quest log's list buffer is shared), ROM
`.text` +8,992 B and `.rodata` +4,824 B (the quest code lives where Talk does). Dreamcast: Talk is linked there, so the
quest code is too (it builds; `story_engine` and `side_chains` pass in dcrun). PS1: 132,608 B below the stack top before and
after (the EXE is the same size: `talk_quest.c` is compiled only where `DOOMO_HAS_TALK` is, the quest log rows and
`NT_TALK_QUEST` compile out), so 0 B of the 12 KB TALK share. `TALK_en.BIN` 909,052 -> 913,148 B.

---------------------------------------------------------------------------

## 6q. Talk on the PS1: the re-measure and the plan (TKP0-MEASURE)

Read-and-measure package (2026-11-15); the full numbers and the packages TKP1 to TKP6 are in
[docs/design/talk/TKP_PLAN.md](design/talk/TKP_PLAN.md). No resident PS1 byte changed (margin 110,560 B on the
rebased tree, before and after). What it found, in short:

* **Code.** Linked on the PS1 the Talk files are 146 KB (95 KB code, 14 KB rodata, 5.6 KB bss after
  `--gc-sections`, 26 KB of it the typed line the PS1 does not need). As one new overlay group `talk` (the screen
  is a scene, not a house part) it is **92 KB, no slot growth**; real links: +19,520 B resident with the
  group alone, +15,616 B with the typed line off (margin 110,560 -> 94,944 B), **about 2 to 4 KB** after the TKP2 clean-ups (state to the scene
  scratch, the callbacks local, the save chunk kept as a blob). Inside the 12 KB share.
* **Data.** The PS1 subset of `TALK_en.BIN` (909,052 B) is **577,786 B** against the design's 319,488 B.
  Lossless levers (word-token strings -90 KB, packed rows -50 KB) give about 437 KB. **That does not fit SPU RAM:**
  CR10's part store (PARTS.BIN, 143,360 B) holds 0x50000-0x7DFFF, so Talk can have at most 278,528 B
  (0x0C000-0x4FFFF) or 331,776 B with the tail after PARTS.BIN. Decision D1 for the owner: a reduced PS1 content
  tier (about 38 % or 26 % of the lossless bin cut, TKP1b), or drop Talk on the PS1. The design's top-6 /
  attribute-tail levers are worth nothing now.
* **Reads and time.** 2,075 database reads and 43.4 KB a line (74 KB an exchange; 48 % of the instructions are
  the lint's probes): a 16 KB block cache and one lint a line bring it to 5.8 KB a line. 661,000 x86
  instructions a line, estimated 12 to 30 ms for a typical `talk_dm_step` on the R3000 and up to 260 ms for the
  worst: the 30 fps gate needs the generator sliced inside a step (TKP4).
* **Tools** (host or build machine only, nothing in the game): `ps1/tools/talk_size.sh`,
  `core/tools/talk_reads.sh` (+ `talk_reads.c`), `core/tools/talk_steps.sh` (+ `talk_steps.c`).

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
**T5B-SURF1 (Surface push A, content only)**: +722 alternatives (5,646 -> 6,368) in three new files,
`grammar/en/surf_a_gesture.tlk` (hug, shake of the head, nod, a gift, walking away), `surf_a_third.tlk`
(accuse, defend, take a side, mediate) and `surf_a_flow.tlk` (yes, no, introduce). The census grid
(`census_en.tsv`, 110,000 contexts, every exchange sampled) showed these player acts repeat one line per
rule most: 74% to 85% distinct replies for hug, no, walk away, accuse, yes, mediate, take side, shake head,
nod, introduce, give and defend, against 93% to 99% for the ask and cool acts. Each act got 50 to 70 new
alternatives over the reply acts the adjacency table allows, with guards by time, mood, relationship,
voice, stance, topic and (for kid speakers) `kid_speaker` with `tags: kid`. A hug stays a plain hug a
stranger, a rival or an unfamiliar child may decline; no tease or blame reply (TK10 untouched), no register
word, no romance. Rule names start with `sa_`. Census, 200,000 contexts, same grid, before -> after:
Surface 116,536 -> 124,834 distinct (+7.1%; Chao1 655,750 -> 734,511), Skeleton 113,387 -> 121,363,
Meaning 15,191 -> 16,195, 0 of 200,000 failed quality, 0 S3 hits. A full 2M run was not measured (the estimate of about 830,000 was a guess; the lead's census gives the number). Hard gap at 200k: Chao1 734,511 vs the 1,000,000 target
Surface: **the 1,000,000 target is still missed**; it needs roughly 3 to 4 more pushes of this size, or lines with
topic slots in the high-volume rules. `TALK_en.BIN` 755,772 -> 788,732 B (+32,960 B, about 46 B an
alternative; PS1 non-resident data, no resident byte; GBA and PS1 sections unchanged). No engine change.

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
| `core/tests/test_talk_dm.c`, `test_talk_census.c` | TK-CARE: the care window after distress (`talk_dm_care_after_distress`, `talk_dm_care_after_grief`, `talk_gen_property_care`) |
| `core/tests/test_talk_scene.c` | T8-SCRIPTS (6j): SCENE_TALK through the pad: the debug view, a quick reply is 1 press, turning to the next person and complimenting is 4, no Romance page for a child, a typed distress line gets the distress verdict and the talk goes on, a typed romance line to a child is refused, the goodbye toast has no raw format code |
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
  0x30000-0x7DFFF. (Superseded 2026-11-15: CR10's part store took 0x50000-0x7DFFF;
  Talk can have 0x0C000-0x4FFFF at most, see section 6q and TKP_PLAN.md D1.)
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

**Keeping the talk going.** The resident opens the talk (`talk_dm_open`: the ARRIVE line, then a follow-up) instead of a fixed greeting. After every answer the same resident hands the talk back with a follow-up line (`TGE_FOLLOW`, `core/data/talk/grammar/en/follow.tlk`, planned as `DK_FOLLOW` in `talk_dm.c`): always after a change of subject (`follow.switch*` react to the new topic) and after a snarky line (`follow.cool*`: push on or let it go, so the player can heat the room up or cool it down), and about 7 turns in 10 otherwise. Never in an argument, a walk-out, a farewell or at `TALK_TENSION_ARGUE`; the drama manager speaks then. It shares the exchange's 4-line budget (a reaction is dropped before the follow-up is). Test: `talk_dm_keep_going`.

## 6n. The Talk screen's look (TUI-TALK, 2026-11-15)

A restyle of `talk_draw` in `core/src/talk_scene.c`; every control, row and script stays. The hierarchy is now: the
people strip, one instruction line, the replies, and one control card. Changes (all pixel-changing, native shots in the
package evidence, before / after at 240x160 and 320x240):
- The mood word ("calm") is gone from the panel header; only "tense" and "heated" are named.
- The people strip shows the engage bar of the person you speak to (everyone's when you speak to the room), not all.
- No sliding glint on the two windows and no bar shine (`ui_glint_hold`, `ui_bar_still`): calm motion.
- A control card (`ui_prompts_at`, the shared prompt bar): "A Pick  B Goodbye" on the replies page, "A Pick  B Back"
  below it, centred just above the panel. It is hidden while a dialogue box is up. Its own draw cost was not measured apart from the rest (the GBA fps did not move).
- The panel is the same 90 px, so the pointer map (T9) and `talk_pointer.txt` are unchanged.
Test that fails without it: `talk_scene_has_a_control_card_on_every_page`. Measured on the GBA (`make -C gba test-talk`,
emulated time): list 15.7 -> 15.4 drawn fps, answer 18.1 -> 17.3 (work avg 47.7 -> 48.2 ms, 45.8 -> 45.1 ms): no gain,
within run noise; the 30 fps target is NOT met (about 48 ms of work per tick: the cost is the list and window
drawing and the portrait box, not the strip). EWRAM / IWRAM / ROM data unchanged. A cheaper portrait box and a retained list panel are the
next steps (SPEED stream).
