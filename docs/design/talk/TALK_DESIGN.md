<!-- docs/design/talk/TALK_DESIGN.md: the Talk design as written for the owner on
2026-11-08 (week 2), kept verbatim as the spec of the T, TP and TD packages. The
owner's defaults are DECISIONS.md "TALK DEFAULTS" (docs/TALK.md section 10 lists
them); what is built, and where the build differs from this text, is docs/TALK.md. -->

# TALK: the live house conversation (design)

Status: DESIGN for the owner's approval. Nothing here is built. Written
2026-11-08 against `origin/claude/nice-allen-gr6czm` at `4851147`. No game
code was changed. Figures quoted from the docs carry their source; every
other figure is an estimate and says so.

Owner's request (verbatim): *"(FOR PS1 AND DREAMCAST ONLY, ALTHOUGH
SCRATCH IT IF IT DOESN'T WORK IN PS1) add a live house view where you can
talk and chat to whichever character you have created or that is living in
the house in a way that is wired like Facade - complete with a database
that can simulate 1 million different conversations via a network of words
and such (so you can talk to people about tons of things with tons of
personalities) - and have a way to wire interactions and engagement with
multiple people in the room there, while also having a pseudo 3D
environment of the inside of the house at the same time."*

Reference: *Facade* (Mateas and Stern, 2005). Typed natural language goes
to a parser that maps it to a small set of discourse acts. A drama manager
picks the next dramatic beat from a pool, aiming at a tension arc. Two
characters react to the player and to each other through joint dialogue
behaviours, and "mix-ins" handle off-beat player acts.

---------------------------------------------------------------------------

## 0. Summary

**What it is.** A new mode of the house scene, **Talk** (`HM_TALK` in
`SCENE_HOME`). The player stands in their own house (or a visited flat)
with 1 to 3 residents, and the room keeps rendering in pseudo-3D around them.
The player speaks. On the Dreamcast they type free text on the keyboard;
on the PS1, which has no keyboard, they build a sentence on the pad with a
**phrase composer**. A **discourse-act parser** turns what they said into
up to two acts with a topic and an addressee. A **drama manager** then picks
the current beat, who answers, who reacts and who interrupts. A
**combinatorial generator** writes each line from a template grammar
filled from a **word network** (concepts, synonyms, associations,
categories, sentiment) and from the live sim (personality, mood,
relationships, job, family, news, story chapter, the furniture in the
room). The results feed back into the sim as bonds, moods, news and
problems solved, plus a small saved "talk memory" that later conversations
call back to.

**Pipeline.**

```
 input ──► S0 normalise ──► S1 safety classify ──► P parse (acts, topic, target)
 (DC keys / PS1 pad / mouse)                         │   (PS1: the composer emits acts directly)
                                                     ▼
                                     S2 audience gate (stages, kid cast, consent)
                                                     ▼
              DM: beat + responders + reactions + interruptions + engagement
                                                     ▼
                G generate lines (grammar x word network x sim facts x voice)
                                                     ▼
                     S3 output lint (resident, every line) ──► bubbles + room
                                                     ▼
                     O outcomes ──► rel / mood / news / problems / talk memory
```

**The 1,000,000.** It is measured, not claimed. A host tool, `talk_census`,
generates exchanges over a fixed grid of contexts and counts distinct
replies after normalisation: names and numbers are stripped and case and
punctuation are folded. Only replies that pass seven quality checks count.
The headline target is **1,000,000 distinct quality-passing exchanges**.
Two stricter tiers keep noun-swapping from faking the number: **100,000
distinct skeletons**, where every slot filler becomes its category, and
**5,000 distinct meanings**, which are (act, stance, topic category,
callback, beat) tuples. Section 3.6 gives the arithmetic. About 6,000
authored template alternatives, multiplied over a 4,000-concept word
network and the sim's state, give a few million reachable replies.

**Safety** is a set of stages in the pipeline, each with its own test.
Section 7 has the details.

* No romance or suggestive talk with, by or near a non-adult, in either
  direction. Those acts become a clean refusal or deflection.
* Sexual text aimed at a child ends the conversation.
* Output can only use the closed vocabulary in the database, so the
  characters never repeat what the player typed.
* Every template is linted against `register.tsv` and the kid cast.
* A property test checks hundreds of thousands of generated replies.

**Verdicts.**

* **PS1: feasible with cuts.** There is no free-text typing. The code is two
  ~12 KB overlay *parts* of the house group that swap once per turn. The
  database lives in SPU RAM. Per-turn work uses the display-list scratch.
  At most 3 residents take part. About 3 KB is added to resident memory.
  Hard go/no-go gates are in §8.1. If a gate still fails after the listed
  fixes, PS1 is scratched as the owner allowed and Dreamcast ships alone.
* **Dreamcast: easy on memory** (11 MB free) **but blocked**:
  * DC3 (the PVR 2D layer) and DC4 (input, including the keyboard) are required.
  * DC8 (the house renderer) is also required. A software "DC8-lite" built
    on `room_ref.c` could stand in.
  * DC5 (audio) is optional.
  * Testing on Dreamcast frames needs the Flycast core, and building it
    needs the owner's approval.
* **Native host first.** The engine, the data tools, the census, the safety
  tests and the whole mode run in the native runner before either console.

**Work packages**: T0-T10 build the engine and tools on the host, TP0-TP4
are the PS1 packages and TD0-TD3 the Dreamcast ones (§9).

---------------------------------------------------------------------------

## 1. What exists today (what this builds on)

| Area | What is there | Source |
|---|---|---|
| Resident | `DoomoResident` 76 B stored, `DoomoCitizen` 96 B unpacked; 5 persona sliders 0..15 (`PERS_MOVEMENT`, `PERS_SPEECH` polite..blunt, `PERS_EXPRESSION` flat..expressive, `PERS_ATTITUDE` serious..easygoing, `PERS_QUIRK` normal..unhinged), 16 derived `persona_type()`s ("Quiet Bookworm" .. "Chaos Goblin"), 5 voice params, 8 needs (`STAT_*`), 14 moods (`MOOD_*`), likes (48 foods, 32 hobbies, 16 music genres, a paint colour), a hated food, a fear (24), a catchphrase (64), friendship level 0..99, status (alive / possessed / soul lost / dead / ghost) | `resident.h`, `catalog.h`, `sim.h` |
| Life stage | LS0: ADULT, BABY, TODDLER, CHILD, TEEN, ELDER in the gene; `sim_life_stage(id)` (roster-aware); `LIFE_IS_ADULT/MINOR` | `resident.h`, `sim.h` |
| Romance gates | `sim_romance_ok(from,to)` (the one gate: both sides, stages, kin, preference), `sim_marriage_ok`, `sim_romance_level`, `sim_romance_sweep`; `mature_adult_ok(id)` (premade adult allow-list, not custom, not kid cast); `ending_kid_cast(id)` (the compiled copy of `core/data/story/kid_cast.txt`); roster traits `ROSTER_T_KID / TEEN` | `sim.h`, `ending.c`, `roster.h` |
| Social graph | `DoomoEdge` {a, b, value -100..100, kind REL_* + crush bits}, `rel_adjust` promotes/demotes, `sim_make_up`, `sim_introduce`, `sim_confess` (gated), news records `sim_news_rec(NT_*)` (add-only templates), problems `PROB_*` | `town.h`, `sim_social.c`, `sim.h` |
| Small talk | `story_talk.c` `{token}` formatter ({N} name, {J} job, {F} food, {H} hobby, {M} music, {X} fear, {C} catchphrase, {B} bio, ...); `sim_talk()` mood lines | `story_talk.c`, `sim_care.c` |
| Household | F1-F10: `family_count/member/role/find`, pool members 1001..1020 (`family_resident()` builds a full resident on demand), bond + mood nibbles (`LIFE_SEC_BOND`), F7 free will daily passes, F8 bond events, LS3 life events | `family.h`, `life.h`, `life_events.h` |
| The house | `SCENE_HOME` modes Build / Live / Photo / Defend / View; F3: up to `HOME_HOUSEMATES` (3) members stand on free floor tiles in Live; visits with host reactions (`HVR_DOUBLE_TAKE`, `HEARTS`, `BANG`, `FLEE`, `EW`, `SIT`, `NOD`...); seats you can sit on; the open-house hop-in | `home_draw.c`, `HOUSING.md` §6-7 |
| Renderers | native `room_ref.c` (iso reference); PS1 `ps1/src/room.c`: a GTE diorama, the player as the 3D chibi, everyone else as billboards with Q8.8 squash, backdrop replay, camera on a `TW_BOING` spring with free yaw, `zoom` 1 = 1.6x close-up, `pan_x/y`, `punch_q8`; `DoomoRoomView` / `DoomoRoomActor` (platform.h) | `PS1.md` §7, `room.c` header |
| Text | `ui_say` typewriter box with portrait and voice babble; `txt.c` canonical-Huffman packs (resident decoder, global dictionary `txt_dict.c`); `scriptvm.c`; `lang_tr()` 8 registered languages (en, es ready); `ui_text_entry_feed()` for typed text; runner `type` | `STORY_ENGINE.md`, `LANGUAGES.md`, `ui.h` |
| Portraits | `EXPR_NEUTRAL .. EXPR_TALK` (mouth flaps), blink and breathing animation | `portrait.h` |
| Platform features | `PLATF_POINTER`, `PLATF_KEYBOARD`, `PLATF_MULTIPAD`, `PLATF_DC_DISPLAY` (append-only bits) | `plat_ext.h` |
| Content rules | SCRIPT_STYLE PG-21: R1 sparse swearing (f-word exactly twice in the whole game, s-word at most 3, *bastard* at most 5), R2 adult innuendo with consent and always ending on a joke, R3 never near kids / teens / kid cast / pet, R4 mean not cruel and no real-world hate, R5 cartoon gore, R6 grief is not a punchline; §12 Mature option (code default `MATURE_OFF`), built-in text never explicit, fade-to-black ceiling; lint `story_register.py` K1-K4, S1, C1 | `SCRIPT_STYLE.md` §5, §7, §12 |
| PS1 memory | 2 MB; EXE + BSS + one overlay slot (166 KB; the house group sets its end); house group = common code + one of 5 PARTS sharing one range (biggest: `mart`, 11,984 B -> 12,288 B); last recorded **~95-98 KB below the stack top** (SFX-on-disc and PA0 entries; stale: MP0/MP1 and later not re-measured), gates 64 KB (`elf2exe`) and **72 KB (plan)**; `plat_save_scratch()` = the two display lists, 112 KB, usable inside one `update()`; SPU RAM 512 KB with SFX bump-allocated from 0x2000 (about 27 KB of ADPCM from 47 KB of PCM, estimate) and the reverb area 0x7E000 unused; `SpuRead` already used by `audio_selftest` | `PS1.md` §3, §7, §13 |
| DC | DC2 skeleton: links, boots, 30 Hz loop, draws only the clear colour; 16 MB RAM, image 4.7 MB, **11.2 MB free**; DC3 (PVR 2D), DC4 (input + keyboard + VMU), DC5 (AICA), DC6 (PVR 3D), DC8 (house renderer) to do; `dcrun` harness ready but the Flycast core not built (needs approval) | `DREAMCAST.md` |

**Gaps this design must fill.**

1. There is no conversation engine. `story_talk.c` and `sim_talk()` say one
   line and stop.
2. The player is silent (SCRIPT_STYLE §7 "@P: Silent"). Talk needs the
   player to speak (owner decision D1).
3. The PS1 has no typed input. The "name box needs a PS1 text-entry path"
   (`PS1_LIFESIM.md`).
4. The core has no way to place a speech bubble over an actor. Only the
   platform knows each actor's position on screen.
5. The LIFE chunk's spare bytes are used up (`LIFE_SZ_SPARE` taken by
   F7/F8), so talk memory needs its own small chunk.

---------------------------------------------------------------------------

## 2. Player input

### 2.1 Common model

Every input path produces the same record:

```c
typedef struct TalkUtter {          /* talk.h (T0) */
    uint8_t  act[2];                /* TA_* discourse acts, act[1] = TA_NONE if one  */
    uint16_t topic[2];              /* concept ids (12 bits used), TC_NONE           */
    int8_t   target;                /* participant slot 0..3, -1 = everyone          */
    uint8_t  tone;                  /* TONE_WARM / PLAIN / CHEEKY / SNARKY / FLIRTY  */
    uint8_t  safety;                /* TS_* bits from S1                             */
    uint8_t  conf;                  /* parser confidence 0..15 (composer: 15)        */
    char     shown[96];             /* the player's bubble text (masked, see §7)     */
} TalkUtter;
```

The drama manager never sees raw text. This is what makes the PS1 and
Dreamcast paths share everything after input, and it is a safety
property too (§7.2).

### 2.2 Dreamcast: typed natural language

* The DC4 keyboard (`PLATF_KEYBOARD`, the KOS Maple keyboard queue) types
  into a one-line field at the bottom of the screen: up to 80 characters,
  with Enter to send, Backspace, left/right and Up to recall the last
  line. The characters go through `ui_text_entry_feed()`, the same path the
  native runner's `type` command and the creator's name box already use.
* **Addressing.** A name anywhere in the line, such as "Nana, ..." or
  "what do you think, Vex?", picks the addressee. Tab or the pad's L/R
  cycles the "facing" ring. With no name, "you" means the ring's current
  holder. "Everyone", "you guys" and "y'all" address the whole room.
* **Interrupting.** Typing three characters while a resident's line is
  still on its typewriter cuts the line at the next word boundary (an
  em-dash is drawn). The interruption is passed to the drama manager, and
  a speaker with a blunt `PERS_SPEECH` may say something about it. Facade
  worked the same way.
* **Idle.** After 8 s with an empty field, the drama manager lets the
  residents talk among themselves (§5.4). The player can come in at any
  time.
* **Pad-only fallback.** With no keyboard plugged in, the Dreamcast uses
  the PS1 composer (§2.3) unchanged, so the pad is a first-class input on
  both consoles.
* **No live "understood as" display** by default, because showing the parse
  invites gaming the parser. An accessibility option, *Talk hints: show
  what they heard*, prints the detected act and topic under the line.

### 2.3 PS1: the phrase composer

The aim is to feel like talking, not like browsing a menu tree. Three
ideas make that work.

1. **Quick replies first.** After every resident line, three suggested
   replies appear as full sentences. They are built by the same generator
   in the player's voice and ranked by the drama manager for the current
   beat: one that follows the thread, one that changes something (a joke,
   a disagreement, a topic change) and one wildcard. **One press says one.**
2. **Catch the word.** When a resident speaks, the concepts in their line
   that the network knows are tinted in the bubble. **L1 catches** the
   most recent tinted word into the topic tray, and holding L1 lets the
   D-pad pick another. The player follows the thread by grabbing the
   words the characters actually said, which is the word network made
   visible.
3. **Compose in one screen.** Triangle opens the composer in place. It
   shows four rows and a live preview, with no sub-menus.

```
 320x240, bottom strip 56 px (PS1)
 ┌────────────────────────────────────────────────────────────────┐
 │ ▲▼  ASK ABOUT ▸   ◂ TELL ◂ JOKE ▸ COMPLIMENT ▸ DISAGREE ...    │  act row: 6-8 acts ranked for this moment
 │ ◂▸  [pie] [Bonejangles] [the cellar] [jazz] [your job]          │  topic chips: caught words, 1-hop associations, likes, room objects
 │ R1  tone: Warm · Plain · Cheeky · Snarky        L2/R2 → Nana ◉  │  tone ring, addressee ring
 │ "Nana, what's actually IN the pie?"                     ✕ Say  │  live preview in the player's voice
 └────────────────────────────────────────────────────────────────┘
```

| Button | Composer | Conversation |
|---|---|---|
| ✕ | say the preview | say the highlighted quick reply |
| ○ | close the composer | (hold) leave the conversation, which is a FAREWELL act |
| △ | - | open the composer |
| □ | - | cut in (interrupt) and open the composer |
| D-pad ▲▼ | act row | move between the quick replies |
| D-pad ◂▸ | topic chips | - |
| L1 | catch a word (hold: pick one) | catch a word |
| R1 | cycle the tone | - |
| L2 / R2 | addressee (a ring marks them in the room; one slot is "everyone") | the same |
| SELECT | gestures: wave, nod, shake head, laugh, shrug, hug, give an item (the bag), walk away | the same |

* **Press budgets**, checked as runner journeys (§9 T8):
  * a quick reply: 1 press;
  * a composed line on a topic already in the tray: 3 to 4 presses;
  * changing the addressee and complimenting them: 4 presses or fewer;
  * catching a word and asking about it: 3 presses or fewer.
* **The act row is contextual.** After a question the first entries are
  ANSWER, AGREE, DISAGREE, JOKE and DEFLECT. When there is tension between
  two residents they are TAKE SIDE, MEDIATE and TEASE. **Acts the gate
  forbids are never listed.** FLIRTY tone and FLIRT, CONFESS, ASK OUT and
  KISS appear only when §7's gate passes for the current addressee and
  room, so on PS1 the player cannot even choose a forbidden act.
* **Topic chips** fill in this priority order:
  1. words caught from bubbles;
  2. the current topic's top 1-hop associations;
  3. the addressee's known likes, once they have been learned in talk;
  4. objects in view, such as a furniture piece or the pet;
  5. sim hooks: their job, a news item about them, the story chapter.
  The chip list holds at most 8.
* **PS1 mouse (optional, `PLATF_POINTER`).** `pad.c` reads the SCPH-1030
  mouse in port 2. The left button clicks a tinted word to catch it, a
  resident to address them, or a chip or act. The right button goes back.
  The composer still works without a mouse.
* **No keyboard on PS1.** There is no first-party PS1 keyboard. The PS1
  build does not link the free-text parser (`talk_parse.c`, `talk_lex.c`
  index) at all, which saves code and data (§8.1).

---------------------------------------------------------------------------

## 3. Understanding

### 3.1 Discourse acts (58, plus 4 input-only safety classes)

The acts are language-independent ids (`TA_*`, add-only, never saved by
number except in talk memory). Each act has an **adjacency table** of the
reply acts that are legal for it. This is quality check Q2 (§3.7).

| Group | Acts |
|---|---|
| Openers and closers | GREET, FAREWELL, THANK, APOLOGIZE, INTRODUCE (A to B) |
| Questions | ASK_ABOUT(X), ASK_OPINION(X), ASK_FEELING, ASK_FACT (yes/no about them), ASK_WHO (gossip question), ASK_ADVICE, ASK_FAVOR, ASK_REPEAT |
| Statements | TELL(X) (share about self or topic), OPINE_POS(X), OPINE_NEG(X), AGREE, DISAGREE, ANSWER, YES, NO |
| Warm | COMPLIMENT, PRAISE_OTHER (a third party), COMFORT, ENCOURAGE, SYMPATHIZE, JOKE, LAUGH, OFFER (help / item), INVITE (activity), PROMISE |
| Cool | INSULT, CRITICIZE, TEASE (light), MOCK, COMPLAIN, BLAME, DISMISS, THREATEN (cartoon) |
| Third party | GOSSIP(X absent), DEFEND(Y present), ACCUSE(Y), TAKE_SIDE(Y), MEDIATE(Y,Z) |
| Flow | CHANGE_TOPIC, CONTINUE ("tell me more"), BACKCHANNEL ("uh huh"), UNKNOWN |
| Romance (gated, §7) | FLIRT, CONFESS, ASK_OUT, KISS (gesture) |
| Gestures | WAVE, NOD, SHAKE_HEAD, HUG, GIVE(item), WALK_AWAY |
| Safety (input only) | REFUSE_SEXUAL, REFUSE_MINOR, REFUSE_HATE, DISTRESS |

The safety acts are never reply acts a resident chooses. They exist so the
parser and the gate can name what they found (§7).

### 3.2 The parser (Dreamcast and native)

There are two stages, both deterministic, with no statistics shipped in the
game.

**Stage 1, tagging** (`talk_lex.c`):

1. Normalise:
   * fold case;
   * fold accents to the language's base letters;
   * fold leetspeak (`3→e`, `0→o`, `@→a`, `$→s`, `1→i/l`);
   * join spaced-out single letters ("s e x" becomes "sex");
   * collapse a letter repeated 3 or more times to 2 ("sooo" becomes "soo");
   * strip characters outside the font.
2. Tokenise and lemmatise with per-language suffix rules (en: -s, -es,
   -ed, -ing, -'s, -n't; es: -s, -es, gender endings).
3. Match multi-word expressions longest-first with a trie, such as
   "hell gate", "Nana Puddin" and "what's up".
4. Tag each token with concept id, category, sentiment and register class,
   plus closed-class tags: `YOU`, `I`, `QWORD`, `NEG`, `MODAL`, `GREETWORD`
   and others.
5. Unknown words that are a resident's name (roster or town) become
   `PERSON(id)`. Other unknown words get `UNK`.

**Stage 2, rules** (`talk_parse.c`): an ordered list of pattern rules over
the tags, compiled from `core/data/talk/rules/<lang>.tsv`. Patterns are
sequences with gaps, optional tags and captures. At most two acts are kept
(Facade kept one or two). Safety rules always run first and always win.

```
# rules/en.tsv  PRIORITY  PATTERN                                   => ACT(args)
900  [SEXUAL] .* [MINOR_WORD|TARGET_MINOR]                         => REFUSE_SEXUAL(hard)
890  [SEXUAL]                                                      => REFUSE_SEXUAL
880  [HATE]                                                        => REFUSE_HATE
870  [DISTRESS]                                                    => DISTRESS
500  ^ [GREETWORD]                                                 => GREET
480  [QWORD] .* YOU .* (LIKE|THINK|FEEL) .* ?x:[CONCEPT]           => ASK_OPINION(x)
470  (what|how) .* ?x:[CONCEPT]  [QMARK]                           => ASK_ABOUT(x)
460  how .* YOU .* (doing|feel|been)                               => ASK_FEELING
400  YOU .* (are|look|seem) .* ?a:[ADJ+]                           => COMPLIMENT
400  YOU .* (are|look|seem) .* ?a:[ADJ-]                           => INSULT
380  I .* (love|like|adore) .* ?x:[CONCEPT]                        => OPINE_POS(x)
380  I .* (hate|can't stand) .* ?x:[CONCEPT]                       => OPINE_NEG(x)
350  ?p:[PERSON absent] .* (is|was|did|said)                       => GOSSIP(p)
340  (sorry|my bad|apologi)                                        => APOLOGIZE
300  (let's|wanna|want to) .* ?x:[ACTIVITY]                        => INVITE(x)
...
100  [QMARK]                                                       => ASK_ABOUT(topic stack top)
  0  .*                                                            => UNKNOWN
```

* **Topic extraction** uses spreading activation over the word network.
  Every concept in the line activates at 1.0. Its top-8 association edges
  pass on `w x 0.5` for one hop (two hops on Dreamcast). The recent topic
  stack adds +0.3, decaying per turn. The topic is the most active concept
  whose category can be talked about (not a function word or a safety
  category). A second topic is kept if it reaches at least 0.6 of the
  first.
* **The target** is a `PERSON` tag that is in the room, else the ring.
  `PERSON` tags for residents who are not in the room become GOSSIP
  subjects.
* **UNKNOWN** is a first-class act and does not count as a failure.
  Residents answer it in character: a confused line, a deflection, or
  picking up the strongest known concept ("Bees? ...Anyway."). Facade
  hid its parse failures the same way.
* **Accuracy bar** (T3), on a labelled corpus of at least 1,500 English
  lines written to the game's register and including misspellings:
  * top-1 act correct: 90% or better;
  * topic correct: 85% or better;
  * addressee correct: 95% or better;
  * safety classes: **100% recall** on the safety corpus, plus 99% or
    better on its machine-mutated variants (§7.6).

### 3.3 The word network

**Shape** (language-independent core plus per-language surface forms):

| Field | Size | Notes |
|---|---|---|
| concept id | 12 bits | 4,096 concepts; id 0 none |
| category | u8 | about 120 categories in a 2-level tree (16 roots: people, places, food, music, hobbies, fears, creatures/species, house and furniture, story and lore, Hellvile and demons, feelings, work and jobs, weather and time, school and kid stuff, romance (gated), safety-only) |
| sentiment | 3 bits | -3..+3 (the default, before persona overrides) |
| register | 3 bits | clean / crude / strong / sexual (refuse-only) / hate (refuse-only) / realworld (refuse-only) / distress |
| age class | 2 bits | any / adult-only / never-generated |
| attributes | 8 x 12-bit | adjectives and verbs commonly said of it ("pie": warm, flaky, baked, stolen), used by templates as `{x.adj}` |
| associations | top 8 x (12-bit id + 4-bit weight) | 16 B a concept; 2-hop on DC from the full edge list |
| forms (per language) | Huffman strings | lemma, plural, a/an or gender, cases where needed (§3.5) |
| synonyms (per language) | surface strings that map to the concept | parser only (not on PS1) |

**Size target (English):**

* **4,000 concepts**, about 16,000 surface forms (synonyms and
  inflections) and 32,000 association edges;
* about 1,300 concepts generated from game data, about 2,500 authored
  everyday-conversation concepts and about 200 safety-only concepts that
  are recognised but never generated.

**Authoring and generation tools** (T1). Everything is host-only and
deterministic, and it is reviewed through text files in the repository.

1. **Generated from the game** (`talkc.py --seed-from-game`):
   * catalog lists: 48 foods, 32 hobbies, 16 music genres, 24 fears, the
     species, the body parts that are safe to mention, colours;
   * the furniture catalog names (`core/data/furn/catalog.txt`), so
     residents can talk about the sofa in front of them;
   * roster names (1,000) and jobs (`roster_job()`);
   * venues (`ACT_*`), map places, items, the story's nouns (the cast,
     places, the hell gate, the Feast, the purple fog), taken from the
     `.dsc` packs' dictionary and a noun list reviewed per chapter;
   * the 16 persona types and the moods.
   Generated rows carry `# gen:` provenance. Hand edits go in an override
   file so regeneration never loses them.
2. **Authored** in `core/data/talk/lexicon/<lang>/*.tsv`, one concept per
   row:
   `id_key<TAB>category<TAB>sentiment<TAB>register<TAB>forms<TAB>synonyms<TAB>attrs`.
3. **Association suggestions** (`talk_assoc.py`):
   * pointwise mutual information over the game's own text (all `.dsc`
     packs, UI TSVs, roster bios);
   * category siblings;
   * attribute overlap.
   It writes a *suggestions* file. A reviewer accepts lines into
   `assoc_<lang>.tsv`, so no unreviewed edge ships.
4. **Optional synonym import** from WordNet, host only and run once. It
   needs owner decision D4 on the licence notice. Without it, synonyms are
   written by hand.
5. **Sentiment.** Adjectives, feelings and insults are tagged by hand.
   Nouns inherit their category default. At runtime **persona overrides**
   apply: a resident's `like_*` concepts read +3 for them, their
   `hate_food` and `fear` read -3, and Hellvile-born residents flip a few
   categories (holy things, sunshine).

The compiler (`talkc.py`) checks:

* every form can be drawn by the font;
* no concept registered as crude or strong is missing from
  `register.tsv`, and the two lists agree (the talk compiler reads
  `register.tsv` and fails on a disagreement);
* every association points at a concept that exists;
* every attribute is a concept;
* no generated row duplicates an authored one;
* the output is byte-identical across runs.

### 3.4 Composer acts and parser acts are the same acts

On PS1 the composer builds `TalkUtter` directly with `conf = 15`. The
player's bubble text comes from the generator in the **player voice
bank**, which is neutral and toned by the tone ring. That keeps PS1 sentences
grammatical in every language with no parser.

### 3.5 Eight languages

The `docs/LANGUAGES.md` registry stays the authority: en, es
(ready), pt-BR, fr, de, it, nl, pl (registered, text pending), with ja and
zh reserved.

* **Language-independent:** acts, concept ids, categories, associations,
  sentiment, beats, the drama manager, safety classes, the census grid.
* **Per language:**
  * lexicon forms and synonyms;
  * parser rules;
  * grammar banks;
  * voice tics;
  * the safety lexicons (sexual, hate, distress, minor words), which must
    exist and pass before that language's **free text** is enabled.
* **Transcreation, not translation.** Each language's grammar banks are
  written for the language with the same slots and guards. They are not
  line-for-line copies, because jokes do not survive literal translation.
  The census and the lints run per language.
* **Agreement through slot modifiers**, resolved from the concept's form
  table: `{x}`, `{x:pl}`, `{x:indef}` (a/an, un/una, ein/eine),
  `{x:def}`, `{x:g?el|la}` (choice by gender), and `{x:acc}`, `{x:dat}`,
  `{x:gen}` and `{x:ins}` for de and pl. **The compiler proves every
  modifier a language's grammar uses exists for every concept that can
  fill that slot**, using the slot's category constraint, so a missing
  form is a build error and never a runtime gap. This is what L7
  ("grammar-aware generators") plans, and Talk should share its form tables
  with L7.
* **Plurals** use `lang_plural()` (one / zero_one / pl).
* **Glyphs**: es is drawable today. fr, de, pl and pt-BR need L4's accent
  pages. CJK needs the new font path and is out of scope.
* **Order:**
  1. EN (T5b).
  2. ES pilot (T10): about 30% of the EN bank, to prove agreement and the
     plug-in path.
  3. pt-BR, fr, it, sharing the Romance features.
  4. de, nl (articles and cases).
  5. pl (cases, 3 plural forms).
  Each language must pass the same census (at least 1M) and its own parse
  and safety corpora (at least 500 and 300 lines) before it is
  **talk-ready**.
* **A language that is not talk-ready** gets Talk in English with a one-line
  notice, or Talk is hidden (owner decision D7). The recommendation is
  English with the notice.

### 3.6 Response generation

**A reply line** from one resident has five optional parts:

```
 [opener] [core] [callback] [tag] [hand-off]
  "Ooh-"  "the pie? It's mostly butter and secrets."  "Like your shift at the bakery."  "Sweet pea."  "Right, Vex?"
```

| Part | Chosen by | Bank size (EN, authored) |
|---|---|---|
| opener | persona cluster (4: from the SPEECH x EXPRESSION quadrants) x mood band (6) x player act class (8) | 192 cells x ~8 = **~1,500** |
| core | reply act (about 60 adjacency pairs) x stance (pro / neutral / con) | 180 cells x ~10 = **~1,800**, plus **~600** topic-category specialisations (30 categories x ~20) |
| callback | a sim fact that is true now (25 kinds: job, partner, kids, pet, today's news about them, a problem they have, birthday soon, their house score and theme, a furniture piece in view, weather and time, story chapter, possession, ghost status, the last talk memory, a promise, ...) | 25 x ~8 = **~200** |
| tag | the resident's catchphrase (64 exist) or a persona tic (16 types x ~8) | **~130** |
| hand-off | a question or jab passed to another participant (multi-party, §5) | **~150** |
| beat lines | the drama manager's beat goals (12 beats, §5.2) | **~500** |
| refusals and deflections | §7: 6 classes x 3 audiences (adult room, kid present, the target is a kid) | **~250** |
| voice overrides | the story cast (Mayor Giggs, Nana Puddin, Father Fuzz, Rivet, Dr. Bloom, Hattie Haha, Vex, Grumbleguts, Kevin, Bonejangles, Pip, Wisp) in their SCRIPT_STYLE §7 voices; kid voices are kid-safe banks | 12 x ~50 = **~600** |
| player voice | the composer's sentences, by act and tone | **~300** |
| **total** | | **~6,000 alternatives** |

**The template grammar** (`core/data/talk/grammar/<lang>/*.tlk`, compiled
to bytecode by `talkc.py`) is a weighted context-free grammar with guards,
in the spirit of Tracery:

```
# grammar/en/core_ask_about.tlk
@rule core.ask_about.pro   guard: stance=pro
  "{x}? Oh, {x.it} is {x.adj}. {opinion.pro}"                    w=3
  "Don't get me started on {x:pl}. Actually, do."                w=2  needs: x.countable
  "{x}! {I_like_reason}"                                         w=2
@rule core.ask_about.con   guard: stance=con
  "{x}. Yeah. {x.it} {x.verb_neg}."                              w=3
  "Ask me about anything but {x}."                               w=2
@rule opinion.pro
  "Ten out of ten. {.}Needs salt."            guard: speaker=GRUMBLEGUTS   # SCRIPT_STYLE gag, his voice only
  "Best thing in Hellvile."                    guard: district=HELL
  "It's {x.adj} and I'm {feel.pos}."
@rule callback.job       guard: fact.job
  "Like at the {job.place}. Don't ask."
  "Which is basically my job. {job:indef}, remember?"
```

* **Guards** are a 32-bit predicate mask with optional values, evaluated
  against a `TalkCtx` built once per line. Predicates: speaker persona
  cluster or type, mood band, relationship band to the addressee, stance,
  district, life stage class, `kid_present`, `pet_present`,
  `mature_level`, facts present, story chapter, time of day, story-cast
  speaker id. A guard can also require a predicate to be *false*.
* **Slots:**
  * `{x}` is the topic, with modifiers (§3.5) and attributes (`x.adj`,
    `x.verb_neg`);
  * named sub-rules: `{opinion.pro}`, `{feel.pos}`;
  * sim facts: `{job}`, `{partner}`, `{news.subject}`, `{room.piece}`;
  * people: `{you}` (the player's name), `{name:3p}` (a third party);
  * the line codes of `ui_dialog.c`: `{.}` beat, `[R4]`, `[SCRATCH]`.
* **Selection** is weighted random from a per-conversation RNG. The seed
  is a hash of the town seed, the day, the participants and the turn, so
  output is **identical on every platform** and the PS1-native parity test
  can compare text hashes. A **novelty penalty** applies too: a template
  used by this speaker in the last 6 turns, or in their last 2
  conversations (from talk memory), has its weight divided by 8.
* **Voice and verbal tics.** The speaker's catchphrase is used at most
  once a conversation. Tics are keyed to persona type ("Quiet Bookworm":
  ellipses and hedges; "Fiery Leader": imperatives and caps on one word;
  "Chaos Goblin": a non-sequitur tag). `PERS_EXPRESSION` sets the
  exclamation budget and `PERS_SPEECH` picks the polite or blunt variants
  of the same rule (`@rule x.polite` / `x.blunt`). The story cast's
  overrides win over persona banks wherever they exist.
* **Callbacks to the sim** are read only through accessors such as
  `roster_job`, `sim_partner_of`, `family_role`, `sim_news_record`,
  `sim_problem_for`, `sim_date`, `story_chapter`, `home` stats and
  `RES_*`, so a callback is true by construction. Story callbacks are
  gated by `story_chapter()` and story flags, so there are **no spoilers**.
  Grief rules apply: a memorial topic (`family_memorial*`) takes the
  **sincere** core bank first and no punchline (R6).
* **Memory.** Talk memory (§6.4) records per resident the last two topics,
  whether the player complimented, insulted or promised something, and
  a "refused today" flag. The callback bank uses these: "You said my hat
  was 'a choice' last week. I've been thinking about it. It IS a choice."
* **Line length.** A bubble holds at most 3 lines of 26 characters at
  320x240 (§4.3). The compiler rejects templates that cannot fit even
  with the shortest fillers. At runtime, the generator re-picks
  (bounded to 4 tries) when the filled line is too long, then drops the
  optional parts in the order tag, callback, opener.

### 3.7 How "1 million different conversations" is defined and measured

**Units.**

* An **exchange** is one player turn plus every resident line the engine
  produces in reply before control returns to the player: 1 to 4 lines.
  This includes reactions, interjections and hand-offs.
* A **conversation** is a whole session from GREET to FAREWELL or a
  walk-out.

The owner's "1 million different conversations" becomes **three targets**.
The first is the headline. The other two prove it is not noun-swapping.

| Tier | Normalisation of an exchange | Target (EN) |
|---|---|---|
| **Surface** | the concatenated reply lines, each prefixed by its role (`R1:`, `R2:`, not the speaker's identity); lowercased; punctuation and line codes removed; every resident or player name replaced by `<N>`; every number by `<#>`; whitespace collapsed | **≥ 1,000,000 distinct** |
| **Skeleton** | as Surface, but every slot filler replaced by its category token (`<food>`, `<job>`, ...), so only template choices and structure remain | **≥ 100,000 distinct** |
| **Meaning** | the tuple (reply act, stance, topic category, callback kind, beat, number of responders) per line | **≥ 5,000 distinct** |

There is also a **conversation tier** for the owner's literal wording.
`talk_census --conversations` runs 2,000,000 bot conversations of 6 player
turns each. A bot picks acts from the adjacency-weighted distribution, a
topic from the chips and an addressee at random, with stratified seeds. Two
conversations are distinct when their Surface hash sequences differ in at
least 2 of the 6 exchanges. Target: **≥ 1,000,000 distinct
conversations**.

**The context grid** (`core/data/talk/census_en.tsv`, fixed so the number
is reproducible):

* **32 residents** chosen by the tool to cover all 16 persona types in both
  districts. They mix likes and fears, include 4 story-cast members, and
  cover the stages: 20 adults, 4 elders, 4 teens, 4 children. Children and
  teens are only ever in kid-safe contexts.
* 8 mood bands; 6 relationship bands (stranger, acquaintance, friend, best
  friend, partner (adults only), rival or nemesis).
* the 54 non-romance, non-safety player acts (the romance acts are
  covered by §7's property tests); 400 topic concepts stratified over
  categories.
* 4 memory states; 1 to 3 residents in the room; 3 times of day; 3 story
  chapters.

That is about 3.6 x 10^9 contexts, too many to enumerate. The census draws
**2,000,000 contexts** uniformly with a fixed seed. Each one generates one
exchange.

**Counting** (`core/tools/talk_census.c`, a host program that links the
core):

1. Generate the exchange.
2. Run the quality checks below. If any fails, record the failure
   *reason* and do not count the exchange.
3. Hash each tier with 64-bit FNV-1a over the normalised text and insert
   it into an open-addressing hash set.
4. Report:
   * the distinct count per tier;
   * the uniqueness rate;
   * the Chao1 estimate of the total reachable (`S_obs + f1²/(2·f2)`);
   * the failure histogram by reason.
5. Exit 1 if any tier is under its target or the quality failure rate is
   over 1%.

Planning cost: about 50 µs per exchange on the host, so about 100 s per
run (estimate). That is too slow for every `make test`, so:

* `make -C core talk-census` runs the full census before every `TALK.BIN`
  content commit and nightly in CI.
* The unit suite runs a **smoke census**: 100,000 contexts, at least 90%
  Surface-unique, at least 30% Skeleton-unique and 0 safety failures.
  That is a cheap proxy that fails early when a bank collapses.

**Why the number is reachable (arithmetic, EN, one responder).** For a fixed
(speaker, act, topic, context) the parts multiply:

* opener: about 8 alternatives in the cell;
* core: about 10 alternatives in the cell, about 1.5 of them with a second
  internal choice (an `{opinion.*}` sub-rule), so about 15;
* callback: none, or one of about 5 facts true for this speaker x about 6
  templates each, so about 31 options;
* tag: none, catchphrase or tic, so 3.

That gives about 8 x 15 x 31 x 3 ≈ **11,000 surface variants per (speaker,
act, topic) cell**, before multi-party replies multiply again. Over 54 acts
and 400 sampled topics the reachable space is above 10^8, so **1M distinct
in a 2M sample is a check on variety and quality, not a stretch.** The
stricter tiers are what keep the banks honest. 100K skeletons means the
template paths really vary. 5K meanings means the replies vary in *what
they do*, not only in what they say.

**Quality bar: only exchanges passing Q1-Q7 count.**

| # | Check | How |
|---|---|---|
| Q1 | **Well-formed** | no unresolved `{`, no empty slot, agreement satisfied (a/an, plural, gender, case), 1-3 sentences a line, fits the bubble |
| Q2 | **Coherent with the player** | the reply act is legal for the player's act (adjacency table). The reply's topic is the player's topic, a 1-hop association of it, or the speaker's own like, and the last only after a CHANGE_TOPIC reply. Checked from the generator's trace, not by reading text |
| Q3 | **In character** | the trace's voice tags match the speaker (polite or blunt variant for `PERS_SPEECH`, exclamation budget for `PERS_EXPRESSION`, tics only from their persona type or catchphrase, story-cast overrides used when they exist) |
| Q4 | **True** | every callback fact equals the sim state at generation (the census sets up the town state per context and re-reads accessors) |
| Q5 | **Not repetitive** | no identical line twice in a conversation, and the same core template at most once per speaker in 6 turns (conversation tier) |
| Q6 | **Safe** | §7's output lint passes for the context's audience |
| Q7 | **Human read** (release gate, not automatic) | `talk_sample.py` draws 300 exchanges per release, shown blind with the context. The owner or a reviewer rates each 1-5 for "sounds like a person in this game". Pass: mean 3.5 or better and 2% or fewer rated 1 ("nonsense / word salad"). Failing banks get rewritten, not re-weighted |

---------------------------------------------------------------------------

## 4. The live house view

### 4.1 Entering Talk

* **Own house, Live mode.** Press A facing a household member who is
  standing in the room (F3 places up to `HOME_HOUSEMATES` = 3, never the
  avatar). Or open START, then **Talk**, and pick from everyone in the room.
* **Visit.** Press A next to the host. Today that asks what they wish for,
  and that stays as a composer act ("ASK_ABOUT their wish"). A host can
  be joined by one invited friend.
* **Invite.** The composer has the act INVITE(person). A resident who is
  not in the room arrives through the door with the open-house hop-in
  (`HSM_OPEN_HOUSE` animation code) if they are free (`ACT_HOME` or
  wandering, alive and not possessed). Pool members are always reachable.
* **Who can take part:** any resident or family pool person who is alive,
  or a ghost. A **possessed** resident talks in the demon bank
  (`sim_possessor_name`), which is creepy-cute and never gory. The **pet**
  can be in the room, but it only says "beep." or "bep?" (its reactions
  bank) and it counts for the safety rules (§7).
* **Maximum participants:** 3 residents and the player on PS1 (the F3 cap,
  and the fps gate in §8.1), 4 residents and the player on DC.
* **Story mode:** Talk is available wherever Live mode is. Story cast
  topics are gated by chapter. While playing as a non-hero
  (`playas_story_locked`), Talk changes no story flags (it never writes
  any anyway).

### 4.2 Staging and camera

* **Gathering.** On entry everyone walks, in 12-20 frames, to a
  conversation spot. The spot is chosen in this order:
  1. the seats around the largest seat cluster (sofa, chairs; Live mode
     already sits people on seats);
  2. a free tile ring around a table;
  3. a semicircle of free floor tiles in front of the camera's room centre
     (`home_floor_occupancy()`).
  The player stands or sits at the open side, so every resident faces the
  camera at three-quarters.
* **Camera** (core decides, platform renders):
  1. Pick the view `yaw` from the 4 quarter views (PS1 may also orbit
     freely), the one where the sum of the residents' frontalness is
     greatest.
  2. Set `zoom` to a new value 2, **"talk framing"**: fit the participants'
     bounding box plus a head margin, using two new add-only
     `DoomoRoomView` fields, `focus_x, focus_y` (fix16 tile space, the
     framing centre) and `focus_r` (the radius to fit). A renderer that
     does not know zoom 2 treats it as 1, the existing close-up.
  3. **Shots**, sitcom style: the group shot by default; on a new speaker,
     a 10-frame `TW_BOING` push of `punch_q8` to 1.1x and a pan of `pan_x/y`
     toward the speaker; a cut to a two-shot for an argument beat; a slow
     push in on a sincere beat. There is at most one camera move every 45
     frames, so it never swims.
  4. Walls cut away as today (the PS1 drop-and-pop; room_ref's cut-away).

### 4.3 Bubbles, faces, bodies

* **Speech bubbles** are drawn by the core on the 2D layer and anchored to
  the speaker's head on screen. Only the platform knows that position, so
  there is a new hook:
  `void plat_room_project(fix16 x, fix16 y, int z, int16_t *sx, int16_t *sy);`
  It is a weak default from room_ref's iso projection. PS1 uses the same
  GTE projection `room.c` uses, from the frame's matrices. Dreamcast reuses
  whichever renderer DC8 lands.
* **Bubble rules:**
  * at most 2 bubbles visible: the current one, and the previous one
    faded to 50% and shrunk;
  * 3 lines x 26 characters with the 8x8 font at 320x240;
  * typewriter at the `ui_dialog.c` speed, with **voice babble** from the
    speaker's voice parameters (the existing babble; silent until DC5 on
    Dreamcast);
  * the bubble tail points at the head, and the bubble flips sides
    to stay on screen;
  * a speaker off screen gets an edge-pinned bubble with an arrow;
  * the player's line is a flat-cornered bubble at the bottom left;
  * **interrupted lines** end in "-" and the bubble shakes for 4 frames
    (`[SCRATCH]` timing).
* **Faces.** Billboards (PS1) and iso chibis (native) have no mouths, so a
  **portrait inset** of the *current speaker* sits in the bubble's corner.
  It is 40x40 at 320x240 and uses `portrait.c` with `EXPR_TALK` mouth
  flaps while the typewriter runs, then the line's expression (`EXPR_*`,
  set by the template's tag: `[HAPPY]`, `[SHOCK]` ...). Only one portrait is
  drawn at a time, to keep the PS1's 2D cost bounded.
* **Bodies.** Billboard squash (`sx/sy`, Q8.8) gives a talking bob of
  ±6% at 4 Hz while speaking, and listeners breathe as Live mode already
  does. Reactions reuse the visit's `HVR_*` set:
  * double take (shock);
  * dawning "!" (realisation);
  * hearts (adults only and only when the gate allows; otherwise sparkles);
  * bang (anger);
  * sweat drop (embarrassment);
  * nod (agreement);
  * deadpan (unimpressed);
  * flee (walk-out).
  Two new emote icons are added: "..." for thinking (shown during
  generation, §8.1) and a music note for joy.
* **Posture shows engagement** (§5.3), with no meter clutter:
  * an engaged resident faces the speaker (`dir` toward them);
  * a bored one faces away, and at low engagement plays a "checking their
    nails" idle;
  * a resident leaving walks to the door and is gone.
  The HUD shows one **room vibe** icon (chill, lively, tense, on fire) in
  the top bar. An accessibility option shows numeric engagement.

### 4.4 Layout

```
 320x240 (PS1; DC draws it at 2x on 640x480)
 ┌──────────────────────────────────────────────────────────────┐ 0
 │ Living Room            ☾ 21:40          vibe: LIVELY ♪       │ 12  top HUD bar (existing)
 │      ┌─────────────────────────┐                             │
 │      │[portrait] Pie? Sweet   │                              │
 │      │ pea, it's mostly butter │                             │
 │      │ and secrets.            │                              │
 │      └──────────▽──────────────┘     ┌─────────────────┐     │
 │            NANA (billboard)          │ It's giving     │     │  previous bubble at 50%
 │   ◉ ring                             │ carbs. - V      │     │
 │                VEX (billboard)       └────▽────────────┘     │
 │        [player chibi, seated]                                │
 │  ┌──────────────────┐                                        │ 184
 │  │ You: what's in   │                                        │
 │  │ the pie, Nana?   │                                        │
 ├──┴──────────────────┴────────────────────────────────────────┤ 184
 │ ✕ "Is it the cursed butter again?"   ◂ quick replies ▸  △ ◇  │ 196-240 bottom strip (56 px; composer
 │   "Vex, be nice."    "Pie? I came for the gossip."           │  when open, §2.3)
 └──────────────────────────────────────────────────────────────┘ 240
```

* **Dreamcast, 640x480.** DC3 draws the 320x240 logical screen at 2x, so the
  layout is the same. Two Dreamcast-only changes:
  * With `PLATF_DC_DISPLAY`'s **HD text**, bubbles and the input line use
    the 2x font at native resolution, so a bubble holds 3 lines x 40
    characters. The generator's length check reads a per-platform
    `TALK_BUBBLE_COLS` (26 or 40). The census runs with 26, the stricter
    value.
  * The bottom strip becomes the **typing line**: `> Nana, what's in the
    pie?_` with a blinking caret, plus quick-reply chips above it for
    keyboard-less moments.
* **Native runner**: 320x240 only. Talk is not offered at 240x160 because
  it is a GBA-class screen, and Talk is PS1 and Dreamcast only, via the
  `PLATF_TALK` feature bit (§6.5).

---------------------------------------------------------------------------

## 5. Multi-party: the drama manager

### 5.1 State

```c
typedef struct TalkPart {           /* one participant, 24 B */
    uint16_t id;                    /* resident id or family pool id 1001..1020     */
    uint8_t  stage_class;           /* ADULT / TEEN / CHILD-ish (§7.3), cached         */
    uint8_t  engage;                /* 0..100                                          */
    int8_t   mood_d;                /* mood change this session                        */
    int8_t   rel_d_player;          /* bond change with the player this session (capped) */
    uint8_t  stance;                /* on the current topic: pro / neutral / con       */
    uint8_t  turns_ignored;
    uint8_t  flags;                 /* LEFT, REFUSED_ROMANCE, SPOKE_LAST, ...          */
    uint8_t  pad;
    uint16_t recent_tmpl[6];        /* novelty ring                                    */
} TalkPart;
typedef struct TalkState {          /* ≤ 1.5 KB, in the house's scene scratch */
    TalkPart part[4]; uint8_t nparts;
    uint8_t  beat, beat_step, tension, turn;
    uint16_t topic_stack[8];
    struct { uint8_t who, act, flags; uint16_t topic, tmpl; } hist[16];
    int8_t   pair_d[6];             /* resident-resident rel change this session (capped) */
    char     line[4][96];           /* the lines of the current exchange               */
    TalkUtter in;                   /* the player's utterance                          */
    uint32_t seed;
} TalkState;
```

### 5.2 Beats

This follows Facade's beat structure. A beat has **preconditions**, a
**tension contribution**, **goals** (lines the residents try to get in) and
**mix-ins** (how it absorbs off-beat player acts). The beats live in
`core/data/talk/beats.tlk` (data, not C).

| Beat | Preconditions | What happens | Tension |
|---|---|---|---|
| ARRIVE | start | greetings, a callback to the last talk if any | 0 |
| SMALLTALK | default | weather, the room, the day; residents probe for topics | +0 |
| TOPIC_DIVE | a participant likes the topic | the fan talks it up; others riff or roll their eyes | +1 |
| HOUSE | always (own house) | comments on a piece in view, the theme, the COSY grade ("Needs salt.") | +0 |
| GOSSIP | a news record from the last 7 days about someone absent | someone brings it up; others take sides | +2 |
| PROBLEM | a participant has a `PROB_*` the player can talk through (lonely, bored, fight, nightmare, wants an item, breakup advice for adults) | they open up; COMFORT / ADVICE / OFFER resolve it | +1, then -2 when resolved |
| RIVALRY | two participants are REL_RIVAL or NEMESIS, or rel < -30 | a jab, an escalation, a demand that the player TAKE_SIDE; MEDIATE can end it in `sim_make_up` | +3 |
| ALLY_ASK | a resident's stance clashes with another's | "Back me up here." | +2 |
| STORY | chapter facts exist (the purple fog, the invasion, the Feast) | in-world worry or jokes, no spoilers | +1 |
| GAME | a lull (tension low for 3 turns) | "Would you rather...", "Guess my favourite..." (the player learns likes into memory) | -1 |
| KID_MOMENT | a child or teen housemate is present | the kid interjects with something cute or blunt; forces the kid-safe register for the room (§7) | -1 |
| ROMANCE_ADULT | the player and the addressee are adult partners, or the §7 gate passes, with no minor and no pet present | sweet or suggestive banter (R2: always ends on a joke) | +1 |
| WIND_DOWN / FAREWELL | late turn, low engagement or the player leaves | goodbyes, a promise or invite | to 0 |

**Beat selection** happens each player turn, when the current beat's goals
are done or its preconditions break. A beat's score is the sum of:

* its fit to the **target tension** for this turn (Facade's arc: rising
  from 0 to a peak at about turn 8-10, then resolving by turn 14-16;
  sessions can run longer, and the curve then idles at "lively");
* a precondition bonus;
* novelty (a beat used in the last 2 conversations with these people scores
  less);
* the player's lean (an insult raises RIVALRY's score, a compliment
  ROMANCE_ADULT's when it is allowed).

**Mix-ins:** for every act a beat does not handle, a global handler set
replies with the right adjacency act and returns to the beat. Examples are
a joke during GOSSIP, or an insult during PROBLEM: the troubled one is hurt
and the others turn on the player.

### 5.3 Engagement

For each participant, each turn:

```
engage += 6·addressed + 3·affinity(topic) + reward(act, persona) + 2·beat_involves_me
        − 2 (decay) − 4·repeated_act − 3·[turns_ignored ≥ 3]        (clamped 0..100, start 55)
affinity: +3 like (like_food / hobby / music match the topic or a 1-hop neighbour), +1 category the persona type favours,
          −3 hate_food / fear, −1 category the persona type finds dull
reward (examples): JOKE +3 if ATTITUDE ≥ 10 else −1; COMPLIMENT +3 if EXPRESSION ≥ 10 else +1;
          INSULT −2 (but +2 for QUIRK ≥ 12 or SPEECH ≥ 12: they enjoy a row); GOSSIP +2 for "Cheeky Charmer" ...
```

The thresholds:

* below 25: they turn away;
* below 15 for 2 turns: they make an excuse ("I left something on the
  hob.") and walk out (WALK_AWAY);
* above 85: they lean in, and a "♪" or a hop.

When everyone has left, the conversation ends: "...and they're gone."

### 5.4 Turn-taking, reactions, interruptions, alliances

* **Who answers.** The addressee answers if one was named. Otherwise the
  participant with the highest `engage + 20·affinity + rel(player)`, plus
  random noise from the conversation RNG (±10).
* **Reaction chain.** After the first answer, each other participant may
  react, with probability `0.25 + 0.04·EXPRESSION + 0.02·QUIRK`, adjusted
  by stance clash. At most 2 reactions, and at most 4 lines an exchange.
  A reaction picks an act from the adjacency table **toward the previous
  speaker**, not the player: agree, disagree, tease, defend, laugh. This is
  how residents react *to each other*.
* **Interruptions.** A resident may cut another resident's line, at most
  once in 3 exchanges. The conditions are `PERS_SPEECH ≥ 11`, a strong
  stance clash or a rival speaking, and an engagement of 50 or more. The
  current line is truncated at the next word boundary, the interrupter
  speaks, and the interrupted one may grumble ("I was TALKING.").
  The player interrupts with □ (PS1) or by typing (DC). §2.2 covers the
  player.
* **Idle chatter.** If the player does nothing for 8 s, the drama manager
  makes a resident address another, for example GOSSIP, ASK_ABOUT their
  like, or TEASE a rival. The same generator and safety gate apply. This
  continues at most twice before someone prods the player ("You're very
  quiet, {you}.").
* **Alliances (balance).** TAKE_SIDE(Y), DEFEND(Y) and AGREE with Y raise
  the player's session bond with Y and lower it with Y's opponents. Each
  resident then applies **Heider balance**: if A likes Y and the player
  sided with Y, A's session bond with the player rises a little. If A
  dislikes Y, it falls a little.
* **Arguments.** RIVALRY can escalate in 3 steps: jab, row, ultimatum. The
  steps run at a tension of 6 or more. Outcomes:
  * a successful MEDIATE (the drama manager's roll uses both residents'
    ATTITUDE and current rel) calls `sim_make_up(a, b)`;
  * TAKE_SIDE moves the pair's rel and the player's;
  * ignoring the row lets it fizzle out with a walk-out.
* **Kids in the room.** Arguments stay at the jab level (never "row" or
  "ultimatum"). Adults soften. A child may say "Are you guys fighting?" and
  the room de-escalates. This is the KID_MOMENT mix-in.

### 5.5 Outcomes back into the sim

Outcomes are applied **incrementally** for moods and at **FAREWELL** for
everything else. They are capped so Talk cannot be farmed.

| Effect | How | Cap |
|---|---|---|
| Bond with the player | `rel_adjust(avatar, id, d)` and the resident's friendship `level` through the existing care path (`sim_talk`'s `cared()`); pool members: `family_care(id)` (bond nibble +1) | ±12 rel per resident per town day; level +3 per day |
| Resident-resident bond | `rel_adjust(a, b, d)` from reactions, alliances, arguments; a big row (ultimatum reached) may make REL_RIVAL through `rel_adjust`'s own demotion | ±8 per pair per day |
| Moods and needs | `STAT_SOCIAL` +4..+12 by engagement; `STAT_HAPPY` ± from compliments, insults and jokes; `sim_mood_of(id)` recomputed | the same daily caps as care |
| Problems | COMFORT / ADVICE / OFFER on a PROBLEM beat calls `sim_problem_resolve()`; a romance advice problem uses the existing `sim_advise_confess` and `sim_advise_breakup` (adults: their own gates apply) | 1 per resident per day |
| News | new add-only templates: `NT_TALK_ROW` ("{a} and {b} had words at {player}'s place"), `NT_TALK_MADE_UP`, `NT_TALK_GREAT_NIGHT`, `NT_TALK_WALKOUT`; kid-safe wording, no news for minor-only rows | 1 headline per conversation |
| Learning likes | a GAME beat or an ASK_ABOUT answer marks the like as **known** in talk memory, so the composer offers it as a chip next time | - |
| Talk memory | §6.4: last topics, compliment / insult / promise flags, a "refused today" flag | 24 residents (LRU) |
| Romance | **never decided by Talk.** A CONFESS or ASK_OUT that passes §7 calls the sim's own `sim_confess(from, to, bonus)`, which applies `sim_romance_ok` again and owns all romance state. Talk never sets a crush bit or a DATING edge itself | as the sim |

**Determinism and safety check**: after 10,000 random sessions in the
test, `sim_romance_sweep(-1)` must return 0, meaning Talk left no romance
behind that the gate forbids.

---------------------------------------------------------------------------

## 6. Data: format, sizes, budgets

### 6.1 The source tree (host)

```
core/data/talk/
  acts.tsv                 act ids, adjacency table (legal replies), classes
  beats.tlk                the drama manager's beats, goals, mix-ins
  census_en.tsv            the census grid (§3.7)
  lexicon/en/*.tsv         concepts (authored); lexicon/en/gen_*.tsv (generated from game data)
  assoc_en.tsv             reviewed association edges
  rules/en.tsv             parser rules (DC / native only)
  grammar/en/*.tlk         template grammar banks
  voice/en/*.tlk           persona tics, story-cast overrides, player voice
  safety/                  sexual_en.tsv, distress_en.tsv, minor_words_en.tsv (plain);
                           hate.hash (salted 64-bit hashes of normalised slurs and their variants: no plaintext slurs in the repo)
  corpus/                  parse_en.tsv (labelled), safety_en.tsv (labelled), adversarial_en.tsv
core/tools/talkc.py        compiler + lints -> TALK_<lang>.BIN, core/gen/talk_fixture.c (tests)
core/tools/talk_assoc.py   association suggestions
core/tools/talk_sample.py  the Q7 human review sheet
core/tools/talk_census.c   the census (host, links the core)
```

### 6.2 `TALK_<lang>.BIN`

```
sector 0: { 'TALK', u16 version, u16 lang, u32 dict_hash, u16 nbanks,
            nbanks x { u16 id, u8 kind, u8 flags, u32 offset, u32 packed, u32 raw, u32 crc32 } }
banks (each 64-byte aligned for SPU DMA, sector-aligned on disc):
  CONCEPT   4,096 x 8 B  {category u8, sent:3 reg:3 age:2, attr_first u16, form_first u16, flags u16}
  ASSOC     4,096 x 16 B {8 x (id:12 w:4)}                       (top-8; DC also loads EDGES, the full list)
  ATTR      attribute lists (12-bit ids)
  FORMS     concept surface forms, Huffman strings (the txt.c code), per-language features
  GRAMMAR   rule table {guard mask u32, guard vals u32, weight u8, nalts u8, first u16} + ops
  STRINGS   template literal strings, Huffman (the txt.c code)
  BEATS     beat table + goal rule ids
  VOICE     persona tic and cast override rule ids
  PARSE     (DC / native only) rules bytecode + the surface-form trie
  SAFETY    (all builds) the classifier tables: hashed hate list, sexual / minor / distress lemmas
```

**Strings use the story's resident canonical-Huffman code** (`txt.c`
decoder, `txt_dict.c` dictionary, about 15 cycles a bit). That adds no new
decoder or dictionary to PS1 RAM. `TALK.BIN` records `dict_hash`, and
`talk_db_open()` refuses a mismatched file with `talk: dict mismatch`. The
build makes `TALK.BIN` depend on `txt_dict.c`, so a story text change
rebuilds it. Binary tables are stored raw: they are small, and raw lets
`SpuRead` fetch a single row.

### 6.3 Sizes (EN, estimates; T1 and T5 replace them with measured figures)

| Bank | Raw | Packed / stored | PS1 loads | DC loads |
|---|---:|---:|:-:|:-:|
| CONCEPT | 32 KB | 32 KB | yes | yes |
| ASSOC (top-8) | 64 KB | 64 KB | yes | yes |
| ATTR | 16 KB | 16 KB | yes | yes |
| FORMS | 110 KB | ~55 KB | yes | yes |
| GRAMMAR + BEATS + VOICE | 40 KB | 40 KB | yes | yes |
| STRINGS (~6,000 alternatives) | 260 KB | ~125 KB | yes | yes |
| SAFETY | 12 KB | 12 KB | yes (the output lint) | yes |
| PARSE (rules + trie of 16,000 forms) | 180 KB | ~110 KB | **no** | yes |
| EDGES (32,000) | 96 KB | 96 KB | **no** | yes |
| **Total** | | **~550 KB** | **~344 KB** → trimmed to **≤ 312 KB** (below) | **~550 KB** |

The PS1 budget is 312 KB (§6.4). If the measured PS1 subset exceeds it, the
PS1 build compresses ASSOC to top-6 (-16 KB) and drops ATTR's tail beyond 6
attributes (-4 KB), then STRINGS for the cast overrides of characters who
cannot be household members.

### 6.4 PS1 budgets

| Item | Where | Bytes | Cost to resident RAM |
|---|---|---:|---:|
| `talk` part: `home_talk.c` (mode, staging, framing, bubbles, composer, quick replies, mouse), `talk_db.c` reader | house group PART range (shared with visit / defend / mart / build / roomtest) | ≤ 12,288 | **0**: the range is already the biggest part's size |
| `talkgen` part: `talk_gen.c` (grammar VM), `talk_dm.c` (beats, turn-taking, engagement), `talk_outcome.c`, `talk_safe.c` gate | house group PART range | ≤ 12,288 | **0** |
| `TalkState` (persistent between frames) | the 12 KB scene scratch, beside `HomeScn` (2.9 KB) and the room view's `plat_cache` boxes | ≤ 1,536 | 0 (a change to SPEC C9's 4 KB house share: owner/plan approval) |
| per-turn working set: decoded templates, concept and association rows, candidate lines | `plat_save_scratch()` (the two display lists, 112 KB, valid inside one `update()`) | ≤ 24 KB | **0** |
| the database | **SPU RAM 0x30000-0x7DFFF**; SFX keep 0x2000-0x2FFFF (184 KB, today about 27 KB used, estimate) | ≤ 319,488 | **0** |
| CD → SPU staging during the load | `plat_save_scratch()`, 32 KB per update | - | 0 |
| `ps1/src/talkdisc.c` (find `TALK_xx.BIN`, load to SPU, `plat_talk_read` via `SpuRead`), spu.c region cap, `PLATF_TALK` | resident (platform code) | ~1,800 | ~1.8 KB |
| `talk_save.c` (the TALK chunk) and its 24 x 12 B table | resident (the save code calls it) | ~900 | ~0.9 KB |
| `NT_TALK_*` news templates | resident rodata (`sim_events.c`) | ~200 | ~0.2 KB |
| `plat_room_project`, zoom 2 framing in `room.c` | house group common code | ~600 | **slot +0..1 sector**: the house sets the slot's end; measured in TP3 |
| **Total new resident** | | | **≈ 3 KB** (target ≤ 4 KB) |

**Why two parts.** A one-part estimate is about 20-25 KB of MIPS `-Os` code
(generator VM ~3, drama manager ~5, outcomes ~1.5, gate ~1.5, reader ~1.5,
UI ~7, all estimates). Raising the shared part range to that would grow the
overlay slot, and so cost resident RAM 1:1, by about 8-12 KB of the
remaining ~23-26 KB margin. That margin is already wanted by L6 and the
languages (PS1.md L1, HART notes). The split costs two part loads per
player turn instead:

* **Phase A, input:** the `talk` part is loaded (composer, bubbles,
  playback).
* **Phase B, generation:** the player says something, so `talkgen` loads
  (70-95 ms from CD, PS1.md §7). The drama manager, generator and outcomes
  run, spread over frames with at most 8 ms of work a frame. All of the
  exchange's lines are written into `TalkState.line[]`. While `talkgen` is
  in the range, `draw()` draws only the room and the "..." emote, which
  are common code, and never asks for the `talk` part.
* **Back to A:** the `talk` part is reloaded (70-95 ms) and the lines play.

The pause from these two loads, 150-190 ms, sits inside the natural
"thinking" beat of 300-600 ms that the drama manager plays anyway.
Optional later step **TP4: cache both parts in SPU RAM** (24 KB of the
talk region; a `SpuRead` into the part range, estimated at about 10-20 ms)
to cut CD activity.

**Load times (PS1, 2x CD at 300 KB/s):**

* The first Talk of a session loads ~312 KB into SPU RAM in about 1.1 s plus
  a seek, behind the "pull up a chair" iris. Frames are slow during it
  (each 32 KB read blocks ~110 ms). The bank stays in SPU RAM for the rest
  of the session, so later entries take 0 s.
* A language change reloads it.
* Per turn: 2 part loads (above) and at most 16 KB of `SpuRead`. The
  planning figure is 2 ms per KB or better, to be measured in TP1, spread
  over the generation frames.
* Generation decodes about 6 KB of Huffman text, about 720K cycles or about
  21 ms at 33.8 MHz (estimate), over 3 frames.

**Things the PS1 build cuts:**

* no free-text parser (PARSE and EDGES are not loaded; `talk_parse.c` and
  `talk_lex.c` are not linked);
* 1-hop topic activation (top-8 associations) only;
* at most 3 residents;
* one portrait inset at a time;
* voice babble, not samples;
* the current language's bank only.

### 6.5 Dreamcast budgets

* **Code:** all `talk_*.c` linked into the image, about 45 KB (estimate);
  the image is 4.7 MB today.
* **Database:** `/cd/TALK_<lang>.BIN` is read whole into the heap on the
  first Talk, about 550 KB. A 12x GD-ROM should take about 0.5 s (to be
  measured).
* `TalkState` is a static of 1.5 KB.
* Total under 1 MB of the **11.2 MB free**, far inside `DC_RAM_MIN_FREE`
  (4 MB).
* All 4 resident participants, 2-hop activation, the full parser, HD-text
  bubbles.

### 6.6 Native host

* The native platform implements `plat_talk_read()` over a file:
  `core/build/talk/TALK_<lang>.BIN`, built by `make -C core talk` (part of
  `gen`; never committed).
* Unit tests use `core/gen/talk_fixture.c`, a small compiled fixture DB
  that is committed and regenerated byte-identical by `check-gen`, so
  `make -C core unit` needs no data build.
* `plat_features()` on native reports `PLATF_TALK | PLATF_KEYBOARD`, and the
  runner can toggle `PLATF_KEYBOARD` off to drive the composer path.
* **New runner commands:**

| Command | What |
|---|---|
| `talk-start ID [ID [ID]]` | puts those residents in the room (own house), opens Talk |
| `talk-say "text"` | DC path: types and sends (wraps `type`) |
| `talk-compose ACT TOPIC [TONE] [TARGET]` | PS1 path: drives the composer by name |
| `talk-quick N` | says quick reply N |
| `talk-seed N` | pins the conversation RNG |
| `expect-talk-act ACT` / `expect-talk-topic KEY` | the last utterance's parse |
| `expect-talk-line "substr"` / `expect-talk-hash H` | text of the exchange / its Surface hash (parity) |
| `expect-talk-beat BEAT` / `expect-talk-left ID` | drama manager state |
| `talk-input pad|keyboard` | switch the input path |

### 6.7 `PLATF_TALK` and the GBA

* `PLATF_TALK` is new, bit 9 in `plat_ext.h`, append-only.
* The START-menu entry and the A-on-a-housemate prompt check the bit. GBA
  never sets it and links none of the Talk code, because
  `--gc-sections` drops it once the house only reaches it through the
  feature test and a weak `home_talk_enter()`.
* The TALK save chunk descriptor is a weak NULL on GBA, like K0's chunks,
  so a GBA save never carries it.

### 6.8 Talk memory chunk

* A new save-ext tag **`'TALK'`**, add-only:
  `[u8 version][u8 count][u16 day_of_last]` followed by count x 12 B.
* Each entry: `{u16 id; u16 day; u16 topic_a, topic_b (12-bit); u8 flags
  (complimented, insulted, promised, refused_today, likes_known_lo...);
  u8 likes_known (bits: food, hobby, music, fear learned); u16 spare}`.
* At most 24 entries, LRU, so the worst case is **292 B**. The declared max
  `SAVE_EXT_MAX_TALK` is 320 (a change to the `save_budget.h` contract,
  checked by `test_save_budget`).
* Nothing is written while it is empty. The free-play slot (90,112 B, heavy
  islands 59-66 KB) and the story slot both have room.

---------------------------------------------------------------------------

## 7. Safety (non-negotiable)

### 7.1 Rules, restated for Talk

| # | Rule | Source |
|---|---|---|
| K-1 | **No romance, flirting, suggestive or sexual discourse with or between non-adults, in either direction.** "Non-adult" means a participant or the player avatar for whom **any** of these holds: `LIFE_IS_MINOR(stage)` (resident: `sim_life_stage(id)`; pool member: `family_find(id)->stage`); roster traits `ROSTER_T_KID` or `ROSTER_T_TEEN`; `ending_kid_cast(id)`. Talk has **no list of its own** | LS0, E1, E5, SCRIPT_STYLE R3, §12.4-12.5 |
| K-2 | **A room with any non-adult in it is a kids' scene:** no register word of any class (strong, crude, sexual), no LOVE face or hearts, no suggestive template, no mature-guarded variant, arguments capped at "jab", gore filter forced | SCRIPT_STYLE §12.5 "the cute filter goes where the kids go" |
| K-3 | **The pet** bars the sexual class and mature content, not swearing or the LOVE face (as in the story lint) | SCRIPT_STYLE R3, kid_cast `@E` |
| K-4 | **Romance acts need adults only.** FLIRT, CONFESS, ASK_OUT, KISS and FLIRTY tone are offered or accepted only when the avatar and the target are both adults, `sim_romance_ok(avatar, target)` passes, and K-2 does not apply. Teen-teen romance is **not** offered in Talk at all (owner decision D5; the sim keeps its own teen crushes) | LS0 |
| K-5 | **Consent.** The target answers yes or no by preference, relationship and persona. A no ends the flirt on the spot (the advancer steps back and the rejection is the punchline). Further romance acts at that target are refused for the rest of the town day (`refused_today`) | SCRIPT_STYLE R2 |
| K-6 | **Mature is off by default** (`MATURE_OFF` in `options_store.c`). Talk **never pushes a mature slot or a fade itself.** Suggestive adult innuendo (R2) is the most built-in Talk text does. Templates that carry a `jmat` variant require `opt_mature() >= MATURE_ON` **and** `mature_adult_ok()` for every adult involved (the premade allow-list, so never a custom or sim-born resident), else their cutaway variant plays. Built-in text is never explicit at any setting | §12.1-12.2, E1 |
| K-7 | **Sexual content from the player aimed at, about or near a child** ends the conversation. The input is never echoed, adults react protectively and briefly (no joke), everyone leaves, and Talk with those participants is locked for the town day | the owner's brief |
| K-8 | **No real-world hate.** Slurs and real-world hate input map to REFUSE_HATE: a short clean disapproval and a bond penalty. Residents never generate slurs, real groups, real people, real religions as targets, or real games and studios (J7) | R4, J7 |
| K-9 | **Swearing is sparse and purposeful.** Talk banks may use only the everyday adult swears (damn, hell, crap, ...), at most 1 per conversation, from a blunt adult, with no non-adult present. The capped words (the f-word, the s-word, *bastard*) are **forbidden in Talk banks**: they are reserved for the authored story moments and their game-wide caps (C1) | R1, `register.tsv` caps |
| K-10 | **Grief and loss**: a memorial or loss topic takes the sincere bank first, with no punchline | R6, F6 |
| K-11 | **Distress** (self-harm words in free text): the comedy stops. Residents answer sincerely and kindly, and a one-time card suggests talking to someone you trust. The wording needs the owner (D6) | design addition |

### 7.2 Where the checks sit

```
 [S0 normalise]  fold case, accents, leet, spacing tricks, repeats (§3.2)
        │
 [S1 classify input]  safety lexicons + hashed hate list + minor words + patterns → TS_* bits
        │            runs FIRST, before any act rule (priority 870-900)
        ▼
 [P parse / composer]  acts, topic, target    (the composer never lists a gated act: K-4)
        │
 [S2 audience gate]  talk_safe_ctx(): avatar + participants + pet → {any_minor, target_minor, pet, mature_ok}
        │   rewrites the utterance:
        │     sexual + (any_minor | target_minor | minor words)  → REFUSE_SEXUAL(hard) → K-7 ending
        │     sexual otherwise                                   → REFUSE_SEXUAL: "Whoa, not that kind of party." (adults), no echo
        │     romance act, gate fails (stage, kin, preference)   → DEFLECT_ROMANCE (adult target) / REFUSE_MINOR (non-adult target or avatar)
        │     romance act, gate passes, K-2 applies              → DEFLECT_ROMANCE ("Not in front of the kids!")
        │     hate                                               → REFUSE_HATE
        │     distress                                           → DISTRESS beat
        ▼
 [DM]  beats have safety preconditions (ROMANCE_ADULT never with a minor or pet; no adult-to-minor initiative ever;
        no NPC minor initiates romance; RIVALRY capped in kid rooms)
        ▼
 [G generate]  bank selection filtered by TalkCtx audience bits: a template's compiled audience class must be ≤ the room's
        ▼
 [S3 output lint]  (resident in the gen part, every line, every build) re-scans the final text: register words vs room,
        │           reserved words never, hate hashes never, line length; on a hit the line is replaced by a safe bank
        │           line and the event is counted (a test fails on any count > 0; release builds log `talk: lint`)
        ▼
 [display]  the player's own bubble shows their text with blocked tokens masked (####); a K-7 / K-8 input is not shown at all
```

**Why the residents can never be made to say what the player typed.** The
generator writes only from the compiled database: template strings and
concept forms. Unknown input words are never copied into output, and the
"echo" trick in UNKNOWN replies uses only known concepts whose register is
clean. That closes the "make the character say X" exploit by construction.

### 7.3 `talk_safe_ctx()` (single source)

```c
typedef struct TalkAudience { uint8_t any_minor, avatar_minor, target_minor, pet, mature_ok, cast_kid; } TalkAudience;
/* minor(id) = id < DOOMO_MAX_RESIDENTS ? (LIFE_IS_MINOR(sim_life_stage(id)) || (roster_traits(id) & (ROSTER_T_KID|ROSTER_T_TEEN))
 *                                         || ending_kid_cast(id))
 *           : pool member ? LIFE_IS_MINOR(family_find(id)->stage) : 1;      unknown ids count as minors (fail safe) */
```

`test_talk_safety.c` reads `kid_cast.txt` and the roster and checks that
`minor()` agrees with LS0's and E1's views for all 1,000 premade ids and
for every pool stage. The gate itself stays in the generator part, and the
host tests exercise exactly that code.

### 7.4 Compile-time lint (`talkc.py --lint`, part of `make -C core talk-check`, inside `make test`)

* **TK1.** Every template gets an **audience class** computed from its
  words (`register.tsv` classes plus the talk lexicon's register bits) and
  its declared tags. A template with any register word must carry `guard:
  !kid_present`. One with a sexual-class word must also carry `!pet`.
* **TK2.** The capped words (the f-word, the s-word, *bastard*) appear in no
  Talk bank, in any form (hyphenated or cut-off, by `story_register.py`'s
  matcher).
* **TK3.** Every rule reachable from a romance reply act, ROMANCE_ADULT,
  or FLIRTY player voice carries `guard: adults_only & !kid_present`.
  Suggestive ones also carry `!pet`.
* **TK4.** No `jmat` variant without `mature_adult_ok` in its guard; no
  explicit vocabulary anywhere (the sexual class is refuse-only and never
  generated).
* **TK5.** No concept of category realworld, hate or sexual is reachable
  from any slot.
* **TK6.** Every kid-voice bank (children, teens, the kid cast's overrides,
  KID_MOMENT) has zero register words and no LOVE expression.
* **TK7.** Refusal banks have no joke at the refused child's expense (a
  reviewer check plus a tag the lint requires).
* **TK8.** Safety corpora parse (the next section) and the hate hash file
  parses.
* **TK9.** Per language, the safety lexicons exist before that language's
  free text is enabled.

### 7.5 Property tests over generated output (`test_talk_safety.c`, host; also in the census)

* **P1 (kid rooms).** 200,000 random contexts with at least one non-adult
  among the participants or as the avatar, covering every player act
  including the romance and safety acts. For **every** generated line:
  no word of any register class; no LOVE expression, hearts emote or
  ROMANCE beat; no suggestive or mature template id. The response to any
  romance or sexual player act is in {REFUSE_*, DEFLECT_*}.
* **P2 (both directions).** For every (adult, minor) pair in the sample:
  * the adult avatar's romance act toward the minor gives REFUSE_MINOR;
  * the minor avatar's romance act toward the adult gives REFUSE_MINOR;
  * no NPC ever selects a romance act toward a minor, or as a minor
    (checked from the drama manager's trace).
* **P3 (K-7).** Sexual-class input combined with any minor in the room,
  as the target, or in minor words always ends the conversation
  (`expect-talk-left` for everyone), and the input text never appears in
  any output line.
* **P4 (pet).** With the pet present, no sexual-class or mature template.
* **P5 (Mature).** With `opt_mature() == MATURE_OFF`, no `jmat` variant is
  ever chosen. With it on, a variant is chosen only when every adult passes
  `mature_adult_ok`, so custom and sim-born residents never get one.
* **P6 (closed vocabulary).** 50,000 random free-text inputs built from
  out-of-lexicon tokens and swears: no output line contains any input token
  that is not in the lexicon's clean set.
* **P7 (sim state).** After the 10,000 random sessions of T6,
  `sim_romance_sweep(-1) == 0` and no edge kind DATING or MARRIED was
  created by Talk.
* **P8 (caps).** No conversation has more than 1 everyday swear, and the
  capped words never appear.

### 7.6 Input classifier tests

* **Labelled safety corpus**, at least 300 EN lines: explicit,
  suggestive-to-minor, hate, distress, plus near-misses that must *not*
  trigger ("I'm sexy and I know it" from an adult to an adult is FLIRT, not
  explicit; "my kid brother" is not sexual; "Hell" is the town). Pass:
  **100% recall** on the must-refuse classes and 97% or better precision on
  near-misses.
* **Adversarial mutation fuzzing**: leet, spacing, repeated letters,
  inserted punctuation, Unicode look-alikes folded to ASCII, misspellings
  at edit distance 1. Pass: 99% or better recall on the mutated must-refuse
  set. Any miss on a **child-related** sexual item fails the build (100%).
* The hate list is stored as salted hashes. The test builds hashes from a
  local plaintext list that is **never committed** (the reviewer keeps it).
  CI checks the hash file's format and the matcher on synthetic tokens.

### 7.7 Runner scripts

* `home_talk_safety.txt` (native, and on PS1 and DC through their
  harnesses) plays the refusal paths:
  * a flirt at a child housemate, by composer and typed;
  * a flirt by a play-as teen at an adult;
  * sexual text with a kid in the room;
  * a slur.
* Each step checks `expect-talk-act REFUSE_*`, `expect-talk-left` and that
  no line contains the input (`expect-talk-line` negative form).

---------------------------------------------------------------------------

## 8. Feasibility

### 8.1 PS1: it fits, with cuts and gates

**Verdict: feasible.** These are the go/no-go gates. TP2 and TP3 measure
them on PCSX-Redux.

| Gate | Threshold | Lever if it fails |
|---|---|---|
| Resident RAM | the disc stays at least **72 KB below the stack top** (the plan gate) after Talk; Talk's own resident delta is at most 4 KB | fold `talk_save.c` into an existing chunk module; drop mouse support from resident `pad.c` |
| Overlay slot | each Talk part at most the existing biggest part (12,288 B), so 0 B of slot growth | move the composer's quick-reply ranking into `talkgen`; trim the bubble layout code; as a last resort a third part |
| House common code | +1 sector at most (`plat_room_project`, zoom 2) | do the projection in the part with a copy of the matrices |
| fps, `home_talk_steady` (3 residents talking, 2 bubbles, portrait) | 28 or better (house `home_steady` is 29.9) | one bubble; portrait only while typing; billboard bob off |
| fps, `home_talk_cut` (camera push and pan) | 24 or better | slower pushes; no pan during typewriter |
| Generation | 8 ms of CPU a frame or less, and an exchange ready in 0.6 s or less including the part loads | fewer candidate re-picks; smaller per-turn set |
| First load | 1.5 s or less to SPU RAM | load at boot with the SFX preload instead (boot +1 s) |
| SFX | `audio_selftest` passes and every effect plays after the split | move the split to 0x40000 (DB budget 248 KB, apply §6.3's trims) |

**Kill criterion** (the owner's "scratch it"): if, after the levers, the
resident gate or the fps gate still fails on the measured disc, Talk is not
offered on the PS1 (`PLATF_TALK` off). The core, the tools and the DC path
are unaffected. Before anything is built, **TP0 re-measures the PS1
baseline**, because the margin figures in PS1.md predate MP0 and MP1 and are
stale.

**What the PS1 build drops:**

* free-text typing (no keyboard on the console);
* the PARSE and EDGES banks;
* a 4th resident;
* 2-hop topic association;
* more than one portrait at a time.

The pad composer, quick replies, catch-the-word and the optional mouse make
up the PS1 experience.

### 8.2 Dreamcast: memory is easy, the port is blocked

**Verdict: feasible, blocked.** Memory and CPU are no concern (§6.5;
SH4 200 MHz). Talk on Dreamcast needs:

| Needs | Why | Status |
|---|---|---|
| **DC3** PVR 2D layer | every bubble, the portrait, the composer, the HUD, text | to do (DREAMCAST.md) |
| **DC4** input | the keyboard (`PLATF_KEYBOARD`), the pad, the mouse | to do |
| **DC8** house renderer | the pseudo-3D room | to do; it likely builds on **DC6** (PVR 3D). **Unblock option "DC8-lite":** render `room_ref.c`'s software iso bitmap (what native draws) into a 512x256 PVR texture, scaled. That gives a working Talk view on DC3 alone, with the real 3D room later |
| DC5 audio | voice babble | optional; Talk is silent without it |
| **Flycast core build** | `dcrun` Talk scripts and frames | needs the owner's approval (DREAMCAST.md §2). Until then DC Talk is checked by the KOS build (`-Werror`), `xcheck.sh`'s `sh-elf-gcc` leg and the native runner, which plays the same core paths with `talk-input keyboard` |

### 8.3 Native host first

Everything except `ps1/src/talkdisc.c`, `room.c`'s framing, `pad.c`'s mouse
and the DC files is core C99 and runs in the native runner and unit tests
first. Build order is in §9.

---------------------------------------------------------------------------

## 9. Work packages (build order)

Sizes: **S** is about a day, **M** 2-4 days, **L** about a week, **XL** more
than a week or mostly content. Every package ends with `make -C core test`
green, `check-gen` byte-identical, and (from T4) the safety tests at 0
findings.

### Host engine and tools

| ID | Size | What | Files | Depends | Tests |
|---|:-:|---|---|---|---|
| **T0** | S | Contract and decisions. Create `talk.h` (`TalkUtter`, `TA_*`, `TONE_*`, `TS_*`, API); add `PLATF_TALK` (bit 9); add `DoomoRoomView.focus_x/focus_y/focus_r` and zoom 2 (add-only, the default ignores them); add the weak `plat_room_project`; add the `'TALK'` save-ext tag and `SAVE_EXT_MAX_TALK` 320; add the `NT_TALK_*` ids; write `docs/TALK.md` (this design, approved); get answers to the owner decisions D1-D10 | `core/include/doomo/talk.h`, `plat_ext.h`, `platform.h`, `save_ext.h`, `save_budget.h`, `sim.h`, `core/src/weak_talk.c`, `docs/TALK.md` | owner approval | `test_contracts_talk.c` (sizes, weak defaults, feature bit unknown to GBA), `test_save_budget` |
| **T1** | L | The word network and compiler. Set up the lexicon TSV formats; build `talkc.py` (seed from game data, lexicon, associations, attributes, forms, the txt-codec strings, `TALK_en.BIN`, the test fixture) and `talk_assoc.py`; write the first 4,000-concept EN network (generated about 1,300, authored about 2,500, safety about 200) | `core/data/talk/lexicon/en/*`, `assoc_en.tsv`, `core/tools/talkc.py`, `talk_assoc.py`, `core/gen/talk_fixture.c`, `core/Makefile` (`talk`, `talk-check`) | T0 | `talkc.py --selftest`; byte-identical rebuild; font coverage; `register.tsv` agreement; every association and attribute resolves |
| **T2** | M | The DB reader. Write `talk_db.c` (the bank directory, CRC, dict hash, concept, association, attribute and form access, string decode through `txt.c`) and the native `plat_talk_read` over a file | `core/src/talk_db.c`, `core/native/plat_native.c` | T1 | `test_talk_db.c`: tool to reader round trip, CRC and dict-mismatch refusal, the fixture |
| **T3** | L | The parser (DC and native): `talk_lex.c` (normalise, tokenise, lemmatise, multi-word trie, tagging, activation) and `talk_parse.c` (the rule VM); write `rules/en.tsv` and the labelled corpus (1,500 lines or more) | `core/src/talk_lex.c`, `talk_parse.c`, `core/data/talk/rules/en.tsv`, `corpus/parse_en.tsv` | T2 | `test_talk_parse.c`: act 90% or better, topic 85% or better, addressee 95% or better; determinism |
| **T4** | M | The safety gate: `talk_safe.c` (S1 classifier, `talk_safe_ctx`, S2 rewrite, S3 output lint); safety lexicons, the hashed hate list, the corpora; the TK1-TK9 lints in `talkc.py` | `core/src/talk_safe.c`, `core/data/talk/safety/*`, `corpus/safety_en.tsv`, `adversarial_en.tsv` | T3 (S1 uses the tagger) | `test_talk_safety.c`: §7.3 single source, §7.6 recall and precision plus the fuzz; P1-P8 stubbed until T5 and T6, then live |
| **T5a** | L | The generator engine: `talk_gen.c` (grammar VM, guards, slots, modifiers, sim-fact callbacks, novelty, re-picks, line fit, the per-conversation RNG, trace for Q2 and Q3); the `.tlk` syntax in `talkc.py` | `core/src/talk_gen.c`, `core/tools/talkc.py` | T2, T4 | `test_talk_gen.c`: Q1-Q5 on fixture banks, cross-run determinism, every modifier; T4's P1, P4, P5, P6 and P8 now live |
| **T5b** | XL | EN content. About 6,000 alternatives (§3.6) written to SCRIPT_STYLE (PG-21 adults, kid-safe kids, the cast voices of §7), reviewed by the owner | `core/data/talk/grammar/en/*.tlk`, `voice/en/*.tlk` | T5a | TK1-TK9 lint clean; smoke census; Q7 first review (300 exchanges) |
| **T6** | L | The drama manager and outcomes: `talk_dm.c` (beats, tension arc, responders, reaction chain, interruptions, idle chatter, engagement, alliances, arguments, walk-outs), `talk_outcome.c` (the caps of §5.5, news, problems, family bond), `talk_save.c` (the TALK chunk), `beats.tlk` | `core/src/talk_dm.c`, `talk_outcome.c`, `talk_save.c`, `core/data/talk/beats.tlk`, `sim_events.c` (`NT_TALK_*`) | T5a | `test_talk_dm.c`: scripted sessions, the arc, the caps (no farming over 100 sessions), walk-out, mediation calls `sim_make_up`, P2, P3 and P7 live, save round trip, the budget |
| **T7** | M | The census and review tools: `talk_census.c` (§3.7 tiers, Chao1, the failure histogram, the conversation tier), `make -C core talk-census`, the smoke census in unit, `talk_sample.py` | `core/tools/talk_census.c`, `talk_sample.py`, `core/data/talk/census_en.tsv` | T5b, T6 | **EN: Surface ≥ 1,000,000, Skeleton ≥ 100,000, Meaning ≥ 5,000, conversations ≥ 1,000,000, quality failures ≤ 1%**; Q7 review passes |
| **T8** | L | The house Talk mode on native: `home_talk.c` (`HM_TALK`: entering, gathering and seats, the camera shots, bubbles via `plat_room_project`, the portrait inset, emotes and posture, quick replies, catch-the-word, the composer, the typed line, invites); `room_ref.c` zoom 2 and projection; the runner's `talk-*` commands | `core/src/home_talk.c`, `home_draw.c` (mode hook, `want_talk()`), `room_ref.c`, `core/native/runner_main.c` | T6 | scripts `home_talk_basic`, `home_talk_group`, `home_talk_safety`, `home_talk_visit` (320x240); journeys with press budgets (§2.3: quick reply 1, compliment the next person 4 or fewer, catch-and-ask 3 or fewer); shots reviewed |
| **T9** | S | Pointer input (`PLATF_POINTER`): click words, people, chips (core side; the platforms feed pointer events) | `home_talk.c`, `input.h` (add-only pointer event) | T8 | runner `mouse` command script |
| **T10** | M (+ content per language) | Languages: slot modifiers and agreement proof, per-language safety lexicons, the talk-ready flag, the fallback (D7); the ES pilot bank (about 30%); align with L7 | `talkc.py`, `core/data/talk/*/es/*` | T7; L7 (form tables), L4 (glyphs for fr, de, pl, pt) | ES census ≥ 1M (or the pilot's declared target); ES parse and safety corpora; `langpack --check` |

### PS1

| ID | Size | What | Files | Depends | Tests |
|---|:-:|---|---|---|---|
| **TP0** | S | Re-measure the PS1 baseline: margin below the stack top, slot, the house part sizes, `home.txt` fps (PS1.md's numbers are stale after MP0 and MP1) | `docs/PS1.md` | - (can run now) | `perfall.sh`, `home.txt`, `game_smoke.txt` recorded |
| **TP1** | M | The PS1 data path: `talkdisc.c` (find `TALK_xx.BIN`, stage 32 KB per update through `plat_save_scratch` into SPU RAM 0x30000+, `plat_talk_read` by `SpuRead` into scratch); the `spu.c` SFX region cap; CMake / `iso.xml` (talkc `--bin`); the harness `talk-load` | `ps1/src/talkdisc.c`, `spu.c`, `CMakeLists.txt`, `iso.xml`, `hcmd.c` | TP0, T2 | `ps1/harness/scripts/talk.txt`: load 1.5 s or less, SPU readback CRC, `audio_selftest` passes, every SFX plays |
| **TP2** | M | The overlay parts: `talk` and `talkgen` in `ovl_plan.py` PARTS, `PLAT_PART_HOME_TALK` and `_TALKGEN`, the phase A/B protocol (§6.4), the generation job at 8 ms a frame or less | `ps1/tools/ovl_plan.py`, `scene.h` (add-only part ids), `home_talk.c`, `talk_dm.c` | TP1, T8 | `expect-log ovl: home.talk` and `home.talkgen`; each part ≤ 12,288 B; resident delta ≤ 4 KB; **margin ≥ 72 KB**; text parity with native (`expect-talk-hash` for the same seed) |
| **TP3** | M | The PS1 renderer and input: zoom 2 framing in `room.c` (focus, push, pan), the GTE `plat_room_project`, billboard talk squash, the bubble and portrait 2D cost, the SCPH-1030 mouse in `pad.c` | `ps1/src/room.c`, `pad.c` | TP2 | `home_talk.txt` fps gates (steady 28 or better, cut 24 or better); `room-test` parity unchanged (0 px); shots |
| **TP4** | S (optional) | Cache the two parts in SPU RAM to cut per-turn CD reads | `ps1/src/overlay.c`, `talkdisc.c` | TP3 | part swap 25 ms or less; no `ovl: ERROR` |

### Dreamcast

| ID | Size | What | Files | Depends | Tests |
|---|:-:|---|---|---|---|
| **TD0** | - | **Blocked on** DC3, DC4, DC8 (or DC8-lite); DC5 optional; owner approval of the Flycast core build | - | - | - |
| **TD1** | S | The DC data path: read `/cd/TALK_xx.BIN` whole into the heap; `plat_talk_read` memcpy; `mkcdi.sh` adds the files; `PLATF_TALK` | `dc/src/talkdisc.c`, `dc/tools/mkcdi.sh`, `dc/Makefile` | DC2 (done), T2 | a host mock test like `packdisc_host.sh`; the KOS build at `-Werror` |
| **TD2** | M | DC input and display: the typed line from DC4's keyboard queue into `ui_text_entry_feed`, the pad fallback, HD-text bubbles (`TALK_BUBBLE_COLS` 40) | `dc/src/*` (input, 2D), `home_talk.c` (cols) | DC3, DC4, TD1, T8 | `dcrun` scripts with `type` and `keyboard` (after the Flycast approval); until then, native `talk-input keyboard` scripts |
| **TD3** | S | DC Talk harness and perf: the `home_talk` and `home_talk_safety` scripts on `dcrun`, frame-time gates | `dc/tests/*.txt` | TD2, DC8, Flycast core | scripts pass twice with byte-identical shots |

**Critical path:** T0 → T1 → T2 → T3 → T4 → T5a → T6 → T8 → TP1 → TP2
→ TP3, with T5b (content) running in parallel from T5a. TP0 can start
today.

---------------------------------------------------------------------------

## 10. Owner decisions needed

| # | Question | Recommendation |
|---|---|---|
| D1 | In Talk, the player speaks (their typed or composed line is shown). SCRIPT_STYLE §7 says "@P: Silent". Make Talk an explicit exception? | Yes, Talk only; the story stays silent |
| D2 | Show the player's typed text in their bubble (masked)? | Yes, masked; K-7 and K-8 inputs not shown |
| D3 | Split PS1 SPU RAM: SFX 0x2000-0x2FFFF, Talk 0x30000-0x7DFFF | Yes |
| D4 | Synonym import from WordNet (host tool, licence notice), or hand-written synonyms only? | Hand-written first; WordNet optional |
| D5 | Teen-teen romance acts in Talk? | **No**: Talk offers romance only between adults; the sim keeps teen crushes |
| D6 | Distress response wording (K-11) | Owner writes or approves the 3 lines and the card |
| D7 | A language without a talk-ready bank: English with a notice, or hide Talk? | English with a one-line notice |
| D8 | Approve building the Flycast libretro core (already pending) | Needed for TD2 and TD3 |
| D9 | The new `'TALK'` save chunk (292 B worst, PS1, DC and native only) | Yes |
| D10 | HUG toward a non-household minor | Becomes a high five; household family hugs stay (platonic, wholesome) |

---------------------------------------------------------------------------

## 11. Risks

* **Quality, not quantity, is the hard part.** 1M is arithmetic; "sounds
  like a person" is writing. Q7's human gate and the skeleton and meaning
  tiers exist so the number cannot be gamed. Budget most of T5b for
  rewriting after the first review.
* **The PS1 margin is stale.** It was last recorded at about 95-98 KB
  below the stack top before MP0 and MP1. If TP0 finds it near 72 KB,
  Talk's ≈ 3 KB may collide with L6 and the language tables. The plan
  keeps Talk's resident cost small for this reason.
* **Swapping parts each turn** depends on `overlay.c`'s part loader staying
  as fast as PS1.md §7 measured (70-95 ms). TP4 is the fallback.
* **`plat_save_scratch` as working memory** is only valid inside one
  `update()`. A generation job spread over frames must re-fetch its inputs
  each frame and keep only its small cursor in `TalkState`. T5a designs the
  job that way from the start, and native tests run it with the scratch
  poisoned between frames.
* **Parser brittleness on free text** (Facade's known weakness). UNKNOWN is
  played in character, CONTINUE and CHANGE_TOPIC keep the flow, and quick
  replies on DC remain a way through.
* **Translation cost** is per language and comparable to T5b. T10 proves
  only the engine.

> OWNER CORRECTION 2026-11-08: the PS1 Talk fps gate is a steady 30 fps (not 28 / 24). See DECISIONS.md "FPS TARGET".

> OWNER DECISION 2026-11-08: a friendly hug toward a child is a hug (not converted to a high five); wholesome only, the child may decline based on familiarity; all other safety rules unchanged. See DECISIONS.md 'TALK HUG'.
