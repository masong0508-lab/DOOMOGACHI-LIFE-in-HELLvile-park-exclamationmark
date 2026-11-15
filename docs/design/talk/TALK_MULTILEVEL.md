<!-- docs/design/talk/TALK_MULTILEVEL.md: the multilevel composer (package TM0-DESIGN, 2026-11-15).
Owner decision: docs/design/held/OWNER_DECISIONS.md "TALK DYNAMIC, MULTILEVEL, PERSONALITY SCREEN".
Design only: no engine code changed. The numbers come from `talkc.py --stats`, the census grid
(core/data/talk/census_en.tsv) and core/tools/talk_layers.py (the scan added with this document). -->

# TALK: the multilevel composer (design and package plan)

Status: DESIGN, written for the Sonnet agents that build it (TM2a to TM2e). Nothing here is built.
Written against `origin/claude/nice-allen-gr6czm` at `da1334bf`. What exists today is
[docs/TALK.md](../../TALK.md) sections 6a to 6g and [TALK_DESIGN.md](TALK_DESIGN.md); this page
says what changes and how to build it in five one-day packages.

---------------------------------------------------------------------------

## 0. The decision, in plain words

The owner wants Talk to answer what the player said, in layers, with no filler and no non
sequiturs, with topics that go deeper, with replies that can make the sim do something, and with a
personality (the Tomodachi sliders plus Sims-like traits) that decides how a resident reacts.

* **The line bank is frozen.** 8,891 alternatives in 3,189 rules in 105 files
  (`talkc --stats`; the "5,500+" of the decision counted the core lines only). No new content
  packages. What may still be added: fixes, safety guards and a small set of **connective lines**
  (section 10: 108 lines, 1.2% of the bank).
* **Composition replaces authoring.** A reply is a plan (what to do), then four parts chosen from
  existing data, then a coherence check. The plan is the new thing. Most parts already exist as
  rules; the plan only chooses *which existing rules may speak*.
* **The 1,000,000 target** is reached by composition (section 8). The reachable space is already
  large (Chao1 1.6M at 400,000 draws, 3.9M in the last full run); what misses is how flat the draw is.
* **Safety does not move.** Section 12 lists the invariants. Every one of them is already enforced
  in C by the gate, the S3 lint and the guards; the plan adds a second lock in front, never a
  replacement.

---------------------------------------------------------------------------

## 1. How a reply is composed today (read from the code)

`core/src/talk_gen.c` (1,564 lines), `talk_dm.c` (1,284), the banks in `core/data/talk/`.

1. The parser (`talk_parse`) gives a `TalkUtter`: up to two acts, up to two topics, a target slot,
   a tone, safety bits, a confidence. An unknown word is a token with `TTF_UNK`; a line with no
   rule becomes `TA_UNKNOWN`. The utterance keeps only the **masked** bubble text (`shown`).
2. The gate (`talk_safe_gate`) may replace the turn: pass, deflect, refuse, distress, end (K-7).
3. The drama manager (`talk_dm_say`) plans the exchange: the answerer (the one addressed, else the
   most interested), up to two reactions of other residents, a beat line, the **follow-up**
   (`DK_FOLLOW`, about 7 turns in 10), at most 4 lines.
4. Each line is `gen_line()`: `[opener 30%] [core] [callback 35%] [tag 40%] [hand-off 25%]`,
   each part one weighted pick among the alternatives of a rule whose **guards** hold. The core's
   rule is the entry `core.<player act>` (so the reply act is *whatever the guards leave*, tagged
   by the rule's `act:`). Fillers (`{x}`, `{x.adj}`, `{food}`, `{mem}`...) read the word network.
   The line must fit the bubble (3 x 26 columns, at most 3 sentences) or it is re-picked.
5. Outcomes (`talk_outcome_end`) apply the summary to the sim under daily caps.

What this means for the owner's complaint:

* The **reply act is not chosen**: it falls out of which guards happen to hold. A "no" and a "yes"
  to the same request are both reachable at random; `stance=` (pro, neutral, con) is the only
  principled input. Relationship, mood and persona only pick *voice*, not *what is said*.
* The **content is fused into the core line** (`core.ask_about.pro` says the opinion and the
  reaction in one string); there is no second, independent content part.
* The **colour** is real (voice guards, 16 tics, 17 cast banks) but it is only voice.
* The **hook** exists (`follow.tlk`: 19 rules, 45 alternatives; callbacks `cb.*`: 27 rules, 32
  alternatives; hand-offs: 12) but is thin and has no depth.
* **Where the filler and the non sequiturs come from:** section 9 (counts).

### 1.1 The bank by layer (measured, `python3 core/tools/talk_layers.py layers`)

| Layer (as the plan will use it) | Rules | Alternatives | What it is today |
|---|---:|---:|---|
| a reaction, resident to resident (`react.*`) | 46 | 95 | reactions between residents (stay as they are) |
| a + b fused core (`core.*.pro/.con/.neutral/.agree/.disagree`) | 158 | 346 | stance answers: reaction and opinion in one string |
| b content (`core.*`, `topic_<root>.tlk`, `ask_*`, `surf_*`) | 2,076 | 6,192 | the answers themselves, with `{x}` fillers |
| c colour: voiced cores (`type=`, `speaker=`, `.voice`) | 553 | 1,350 | persona and cast voices |
| c colour: openers / tags (16 tics) | 32 / 31 | 63 / 64 | the little parts around a core |
| d hook: follow-up / callback / hand-off | 19 / 27 / 6 | 45 / 32 / 12 | keep the thread |
| kept as they are: safety, refusal, grief, pet, K-7, comfort | 180 | 444 | the sincere and safety banks (never composed) |
| kept as they are: drama-manager beats, idle, jab, row, makeup | 56 | 217 | `beats*.tlk` |
| other | 5 | 31 | |
| **total** | **3,189** | **8,891** | |

(The classification is by rule name and guard; the script is read-only and documents its rules.)

### 1.2 The word network (the content layer's raw material)

`talkc --stats`: 4,009 concepts (1,317 generated from game data, 2,692 authored), 7,370 surface
forms, **9,448 attributes** (about 2.4 per concept, at most 8: the `x.adj`, `x.good`, `x.bad`,
`x.is`, `x.has`, `x.does`, `x.was` fields), **21,254 reviewed association edges** over 2,974
concepts (about 7 per concept: `x.assoc`), 132 categories under 18 roots, a sentiment -3..+3 and a
register class per concept. These are the "facts" of the content layer.

### 1.3 The latest census

Baseline on this machine, on this tree (8,891 alternatives), seed 20261108, the fixed grid:

* `core/build/talk_census --quiet --grid core/data/talk/census_en.tsv --contexts 400000
  --conversations 20000` (110 s): quality 400,000 passed, 0 failed; **Surface 254,159 distinct of
  400,000 (63.5% unique), Chao1 1,588,804** (target 1,000,000: missed at this draw count);
  Skeleton 243,043 (Chao1 1,256,514); Meaning 28,196 (Chao1 52,369); conversations 20,000 of 20,000
  distinct; Q5 repeats in 23 conversations; S3 hits 0.
* The last **full** census (2,000,000 contexts, 5,536 alternatives, before GRIEF and SURF): Surface
  775,601 distinct (**missed**), Skeleton 727,337, Meaning 41,760, conversations 1,999,886 of
  2,000,000 distinct.
* A full 2,000,000-context run of the current tree was started and stopped after its contexts phase
  (the report prints only at the end and the conversation phase takes about an hour on the shared
  machine); the lead runs the full census. The numbers above are the baseline TM2b to TM2e repeat.

---------------------------------------------------------------------------

## 2. The four layers and the plan

### 2.1 One new object: `TalkPlan`

Computed once per **answer line** (not per reaction) by `talk_plan()` before `gen_line()`. It is
plain data, 12 bytes, lives on the stack:

```c
typedef struct TalkPlan {
    uint8_t kind;      /* TK_ACCEPT AGREE DENY REFUSE DEFLECT COUNTER ASKBACK (layer a)          */
    uint8_t depth;     /* 0 surface, 1 opinion + reason, 2 story / memory, 3 secret (layer b)    */
    uint8_t content;   /* TCN_* what the b part says: stance, fact, assoc, memory, secret, name  */
    uint8_t hook;      /* TH_* none, ask, callback, offer, call (layer d)                        */
    uint8_t action;    /* TACT_* none, invite, gift, ask_item, call_over, walk_off (section 6)    */
    uint8_t arg;       /* the action's argument: activity, item, slot                            */
    int8_t  lean;      /* -2..+2 the stance as the reply sees it (trait and relation applied)    */
    uint8_t flags;     /* TPL_SINCERE (grief, refusal: layers a and c only), TPL_KIDSAFE, TPL_NOECHO */
    uint8_t eff[5];    /* (13 B in all) the effective persona sliders after the traits (layer c)               */
} TalkPlan;
```

Reactions of other residents (`react.*`) and the drama manager's own lines (`beat.*`, `idle`, `jab`,
`row`, `makeup`) do **not** use the plan. They already answer the previous line and stay as they are.
The grief, comfort, refusal, distress, K-7 and pet banks stay sincere and unplanned except for the
voice (layer c) and the hook rules in section 7 (a sincere line takes no hook).

### 2.2 Layer a: the reaction to the act (accept, agree, deny, refuse, deflect, counter, ask back)

**Chosen from existing data, no new lines.** Every rule already carries a reply act (`act:`; the
adjacency table `acts.tsv` says which are legal after which player act). The plan maps the seven
kinds onto those acts:

| Kind | Reply acts that serve it (`act:` of the rule) |
|---|---|
| accept | yes, thank, agree, offer, encourage |
| agree | agree, opine_pos, sympathize, laugh |
| deny | no, disagree, opine_neg |
| refuse | no (a request or invitation, with a reason), complain, deflect_romance |
| deflect | joke, dismiss, tease, backchannel |
| counter | change_topic, offer, answer, tell |
| ask back | ask_about, ask_opinion, ask_feeling, ask_repeat (via the hook, 2.5) |

`gen_line()` then picks the core only among rules whose `act:` is in the kind's set (a **soft
filter**: if no rule passes the guards, the filter drops, the pick is counted as a Q8a miss and the
census shows it). The matrix of how many alternatives each (player act, kind) cell has today is
`python3 core/tools/talk_layers.py matrix` (evidence `matrix.txt`): of the cells the adjacency table
allows, only 2 have no alternative at all (`ask_who` counter, `kiss` deflect); most have 20 to 300.
The layer a is therefore **selection only**.

**How the kind is chosen** (`talk_plan_kind()`, deterministic, seeded from the conversation seed
and the turn, so the same talk gives the same text on every platform):

```
score(K) = base(A,K) + 3*lean*sign(K) + rel(K) + mood(K) + voice(K) + trait(K) + topic(K) + noise
```

* `A` the player's act (`u.act[0]`), `K` a kind the act allows. `base` = 3 for the first reply act
  of `acts.tsv` for `A`, 1 for the others (talkc emits the table; no hand-written matrix).
* `lean` = the stance (pro +1, neutral 0, con -1) *as the resident sees it*: `talk_gen_stance`
  (word sentiment, likes, fear, hated food, the Hellvile flip, the personal hash) plus the trait
  shifts of 5.3, clamped. `sign(K)` is +1 for accept and agree, -1 for deny and refuse, 0 else.
* `rel(K)` by relationship band to the player (`band_rel`): stranger: refuse +1 on requests,
  invitations and personal questions, deflect +1; acquaintance 0; friend: accept +1; best,
  partner, family: accept +2, agree +1; rival: deny +2, deflect +1, accept -3.
* `mood(K)` by mood band: happy accept +1; sad: counter +1 (changes to something lighter), accept
  on a kind act; angry: deny +2, refuse +1; tired: refuse +1 on invite and favor; scared: refuse +1,
  deflect +1; weird: counter +1.
* `voice(K)` from the **effective** sliders (2.4): blunt deny +2, refuse +1; polite agree +1 and
  refuse stays soft (the line rules already carry the politeness); easygoing accept +1, deflect +1;
  serious deflect -1; quirky counter +1, deflect +1.
* `trait(K)`: the table in 5.2, at most 3 traits, each at most +-3 per kind.
* `topic(K)`: a **liked** topic root (5.1) accept/agree +2; a **disliked** root refuse/deflect +3
  (never on a grief topic, never on a safety act: those bypass the plan).
* `noise`: 0..2 from the plan's RNG.

Hard rules of layer a (in C, whatever the table says):

* The plan never runs for a safety act, a grief topic (`TCF_GRIEF`), or when the gate said anything
  but pass. Those keep today's path (sincere core, no joke, K-10).
* A romance act keeps today's rule: only the target answers, `TalkConv.consent` decides. The kinds
  allowed there are accept, deflect and refuse only; a romance reply is never a counter or an
  ask back, and `deflect_romance` stays the refusal for minors.
* A minor in the room removes tease, mock, insult, threaten from layer a's reply acts (the cool
  banks are already `!kid_present`; the plan makes it a rule of the selector too).
* A hug toward a child is a plain hug the child may decline: kind accept or refuse, nothing else.

### 2.3 Layer b: the content about what was said

**Chosen from existing data**: the topic and its network facts.

| Depth | Content (what the b part says) | Source (existing) |
|---|---|---|
| 0 surface | the core alternative itself: it already names `{x}` and says an opinion or an answer | `core.*`, `ask_topic.tlk`, `topic_<root>.tlk`, `surf_*` |
| 1 opinion + reason | the stance answer **plus one reason** taken from the topic's attributes (`{x.good}` / `{x.bad}` / `{x.adj}`), or from an association (`{x.assoc}`) | word network; a `reason.*` sub-rule (section 10) joins them |
| 2 personal story / memory | one fact about the speaker told as a short story: the job, the family, this week's news, a problem, the last topic they talked about with the player (`{mem}`), a liked thing | `talk_gen_fact()` (`TCB_*`), `cb.*`, talk memory |
| 3 secret | something the player does **not** know yet: a like, a fear, the hated food, a problem; telling it marks it known | `talk_gen_like()`, `talk_mem_like_known`, `TCB_PROBLEM` |

Depth is the subject of section 3. The `content` field of the plan says which fact the part uses
(`TCN_STANCE`, `TCN_ASSOC`, `TCN_FACT_*`, `TCN_MEMORY`, `TCN_SECRET_*`, `TCN_NAME` for an unknown
word, section 4). **A topic act always has a topic fill or a fact in its b part.** If the player
gave no topic, the content layer uses the one the thread is on (`topic_stack`), else a like of the
speaker (`TA_CHANGE_TOPIC`'s existing rule), else it **asks** (layer d: "About what?" exists:
`core.ask_about.notopic`). It never answers a topic act with a line that names nothing (section 7).

### 2.4 Layer c: the speaker's colour (persona, traits, mood, relationship)

Everything that is voice today stays: voice guards on the sliders, `type=` (16 persona types),
`speaker=` (17 cast banks), mood and relationship bands, kid / teen / elder, the exclamation budget,
tics. What changes is **what the guards read**: the plan computes `eff[5]`, the **effective
sliders** = the stored sliders plus the trait deltas (5.2), clamped 0..15, and the generator's
voice guards (`polite`, `blunt`, `flat`, `loud`, `serious`, `easygoing`, `quirky`, `quick`, `slow`)
read `eff` instead of the stored values. So a "kind" resident with blunt sliders gets polite
voice lines; a "mean" one gets blunt lines. **No new voice lines are needed**: the 1,350 voiced
cores, 63 openers and 64 tags are the colour bank, and traits move which of them are live.
`persona_type()` (the 16 titles) keeps reading the *stored* sliders (the creator's title does not
shift under a trait).

Optionally a trait may add a guard, `trait=<name>`, to a connective line of section 10 (for
example a `shy` bridge). The guard checks the speaker's **effective trait set**
(`talk_trait_has`), is valued, and is covered by lint (7.1, rule L9).

### 2.5 Layer d: the hook (follow-up question, callback, offer)

| Hook | Existing source | Added (connective, section 10) |
|---|---|---|
| ask (a follow-up question about the topic or the person) | `follow.tlk` (19 rules, 45 alternatives, by what the player did) | 10 lines for acts that have none (thank, apologize, compliment, ask_feeling...) |
| callback (a remembered fact) | `cb.*` (job, partner, pet, news, problem, birthday, memory, complimented, insulted, like, fear, family, chapter) | the `story.*` bridges (depth 2) |
| offer (an invitation, a gift, a favour) | the offer, invite and give banks (`core.offer`, `core.invite`, `core.give`: 134 + 134 + 94) | 8 offer lines (an offer made by the resident, section 6) |
| call (name another resident) | `handoff` (12 alternatives) | 6 lines for "call someone over" |

The plan picks the hook from the kind and the depth: a **deny** or **refuse** hooks an offer or an
alternative ("I'd rather not, but what about...") when relationship allows; an **accept** hooks a
question or an offer; a **counter** hooks the question it asked; a **deflect** hooks nothing (it is a
deflection) but never ends the thread: it must carry a counter-topic (7.1 rule L5). A hook is
**required** after a counter and after a refuse of a request (there is something to do next), and
optional otherwise (today's 7 in 10 stays as the default). The hook uses the drama manager's
existing slot (`DK_FOLLOW`, shared 4-line budget), so no new exchange mechanics.

### 2.6 Composing the reply

```
player turn -> gate -> talk_plan(kind, depth, content, hook, action, eff)
            -> gen_line(core filtered by kind)     [a + b, one string]
            -> + reason / bridge sub-rule (depth >= 1, a short joined clause)
            -> + opener / tag (c), already optional parts, now read eff
            -> + hook line (d), a second line of the same exchange by the same speaker
            -> S3 lint -> coherence debug assert (native / census only) -> bubbles
```

The **bubble limit** (3 x 26 columns, 3 sentences) stays. A reply that carries a reason or a bridge
is two short lines (the core, then a continuation the DM plays right after): the continuation uses
the exchange's existing line budget and is dropped before the hook, and the hook before the
reaction of another resident, in the order the DM already uses (reactions are dropped before the
follow-up; the new order puts the **continuation and the hook first**: a reply that answers beats a
reaction from the side).

---------------------------------------------------------------------------

## 3. Topic depth

### 3.1 Levels and gates

| Level | Name | Needs (all of them) | What it unlocks |
|---|---|---|---|
| 0 | surface | nothing | a one-line answer about the topic |
| 1 | opinion + reason | not `TRB_RIVAL` (a rival stays at 0 or 1 with a jab, never goes deeper); the resident is not `shy` unless the band is acquaintance or better | a stance and its reason |
| 2 | story / memory | band acquaintance or better; 2 turns on this topic (or 1-hop associations of it) in this talk; engagement >= 50 | one fact of the speaker's as a story |
| 3 | secret | band best, partner or family, **or** friend with `level` >= 40; 3 turns on the thread; engagement >= 70; not a grief topic (grief is sincere, never mined for a "secret") | something the player does not know yet; the resident says it once, then the topic rests |

Trait shifts to the bands (5.2): `shy` and `paranoid` need one band more for 1 and 2; `outgoing`
needs one band less for 1 and 2 (never for 3); `loyal` lets a friend reach 3 without `level` 40;
`gloomy` raises the story depth gate but not the secret gate.

Rises and falls (per participant, one level per exchange at most):

* **Up** when the player's act is a thread act (ask_about, ask_opinion, ask_feeling, tell, opine,
  continue, agree, sympathize, compliment, comfort) **on the same topic or a 1-hop association** and
  the gate holds.
* **Hold** on a topic change (CHANGE_TOPIC) *within* the same root: the level stays, the reply
  bridges ("while we're on {x}"): a conversation that wanders in one area does not forget itself.
* **Reset to 0** on a topic from another root.
* **Down one** after an insult, a mock, a dismissal or a threat from the player; to 0 after a
  walk-out or a K-5 refusal. The resident says a cool-act line as today (the cool banks) and the
  next turn starts one level lower.
* **Never up** while the room has a child and the content would be adult (the secret kinds are
  kid-safe by construction, 3.3, so a child may tell one to a friend).

### 3.2 Where the state lives and what it costs

| State | Where | Bytes | Saved |
|---|---|---:|---|
| the current level per participant (2 bits), the exchanges on this thread (3 bits), the hook used (1 bit), the last kind (3 bits) | `TalkPart.pad[2]` (already in `TalkState`, 2 spare bytes a participant) | **0** new | no |
| the thread's root (5 bits) and the topic | `TalkState` already has `topic_stack[8]` | 0 | no |
| the unknown-word echo (one word, section 4) | `TalkState` (a `char[18]` + length) | 18 | no |
| the level reached on the last topic (2 bits), the secrets told (4 bits: like, fear, hate, problem) | `TalkMem.spare` (16 bits, today 0): bits 0-1 depth of `topic_a`, bits 2-5 secrets told | **0** new (the entry is 12 B and stays 12 B) | yes, in the 'TALK' chunk; an old chunk reads spare = 0: depth 0, no secrets told; no version bump |
| "the player knows this like" | `TalkMem.likes_known` (exists) | 0 | yes |

`TalkState` is 940 B of the 1,536 B budget; the plan adds about 13 B on the stack and 18 B in the
state: **under 1,000 B**, inside the house's scene scratch, **0 resident bytes** on every platform.
The talk memory table (24 entries) keeps its 292 B `.bss` and its native-only status.

### 3.3 Secrets are kid-safe by construction

A secret is always one of: a like, a fear, the hated food, a problem. **`secret.problem` fires only for the safe `cb.problem.*` set** (hungry, dirty, bored, sick, wants_item, wants_friend; lonely only as a sincere line, never a joke). It never fires for PROB_CRUSH, PROB_BREAKUP, PROB_CONFESS_HELP, PROB_PROPOSAL, PROB_POSSESSED, PROB_CORRUPTION or nightmare; TM2d's `depth_never_adult_for_kids` test covers every stage x PROB_* pair. It reuses `{like}`, `{fear}`,
`{hate}` and the `prob=` guard. **No secret is a romance, a body or a harm.** A child's secret is a
child's fear, favourite or worry ("I'm scared of the dark"): the reply is the same plain sincere
sentence with a kid voice, and the player's next act can only comfort (the quick replies already
restrict to comfort after a sincere line). A grief or distress topic never counts as a secret.

---------------------------------------------------------------------------

## 4. An unknown typed word is named and asked about

Today: `talk_parse` marks the token `TTF_UNK`, the line may fall to `TA_UNKNOWN`, and the reply is
one of 24 generic alternatives (12 rules) ("Hm? Say that again in human." / "Bees? ...Anyway."). The word never
comes back out (design principle: *output uses only the closed vocabulary*).

**New rule.** The reply names the word, or guesses, or asks, and says what it understood. Three
steps, in this order, each with its own safety lock:

1. **Guess from the network** (always safe: the output is a real concept). `talk_lex_near(word)`
   finds the nearest known concept by edit distance 2 (the parser already does 1: `TTF_FUZZY`) or by
   a shared stem. "Did you mean {x}?" says a closed-vocabulary word, never the player's. Lines:
   `unk.guess` (8).
2. **Name it** (the echo, the one new exception). The word is said back only if **all** hold:
   * E1 the token is `TTF_UNK`, kind word, not `TTF_MASK`, `TTF_LEET`, `TTF_FUZZY`;
   * E2 it comes from `TalkUtter.shown` (the **S1-masked** bubble text), never from the raw input,
     so anything S1 masked is gone before the word is read;
   * E3 3 to 16 letters, ASCII letters plus one apostrophe or hyphen, no digits, no all-caps shout;
   * E4 `talk_safe_hate_word(word, fuzzy=1)` is 0 and the word is not within edit distance 1 of any
     of the 247 register words, the minor words (`TW_MINOR`, `TW_MINORX`), a person (`TTK_PERSON`),
     or a hate-hash entry (the same fuzzy check the parser already runs);
   * E5 the room is not in a K-7 state and the player's safety bits are 0 (`TS_*`);
   * E6 the line with the word in it passes **S3** (the lint runs on every line anyway; a refusal
     there falls back to step 3);
   * E7 at most once per talk, and the word is said in quotes, lower case: `"florp"`.
   The word fills a new slot `{u}` (`TFK_TEXT`), valid only in `unk.name` rules (talkc: TK13).
   Lines: `unk.name` (8), kid variants (4).
3. **Ask** without echo (`TPL_NOECHO`: the word failed E1 to E6, or the room is a kid room and the
   word is longer than 12 letters). "That's a word I don't know. What kind of thing is it?" Lines:
   `unk.askmeaning` (6) and `unk.noecho` (4).

Every one of these lines **asks for the kind of thing**, not for the word to be repeated. If the
player answers "it's a food", the next turn parses a category (`talk_lex` already knows category
words) and the plan makes the answer a normal topic answer about that root, saying
"Okay, {x}: so a kind of food." The word is remembered for the rest of the **talk** only: a ring of 4
(hash16 + category root, 12 B in `TalkState`, never saved: no unknown word is stored in a save).

Platforms: **the PS1 has no typed input** (the composer emits known words only), so steps 2 and 3
never run there; the lines are data in the Talk bank (about 30 alternatives, about 1.4 KB, in the
non-resident subset). The Dreamcast and native run all three. On the GBA Talk has no typed line.

Census and tests: the adversarial corpus (`core/data/talk/corpus/adversarial_en.tsv`) gets a column
"echo allowed?" and a property test feeds every entry through `talk_plan` and asserts that no
masked, hate, register, minor or person word is ever echoed (acceptance TM2d).

---------------------------------------------------------------------------

## 5. The trait model (the personality screen's engine)

TALK builds it first; CREATOR (CR-PERS) calls it.

### 5.1 What a personality is

* The five Tomodachi sliders that exist (`PERS_MOVEMENT .. PERS_QUIRK`, 0..15, `persona_type()`
  = 16 types). **Unchanged.**
* **Up to three traits** from 24 (5.2).
* **Likes and dislikes**: one liked topic root and one disliked topic root (the 18 roots of the
  word network: people, places, food, music, hobbies, fears, creatures, house, lore, hellvile,
  feelings, work, time, school, romance (never picked: filtered), safety (never picked), describe,
  stuff). The existing specific likes (a food, a hobby, a music genre, a fear, a hated food) stay and
  are *more* specific than the roots.

```c
typedef struct TalkTraits {
    uint8_t t[3];          /* TRAIT_* 1..24, 0 none, no repeats, no exclusive pair   */
    uint8_t like_root;     /* TCR_* 0 none                                            */
    uint8_t dislike_root;  /* TCR_* 0 none, never equal to like_root                  */
} TalkTraits;              /* 5 B; packed 3x5 + 5 + 5 = 25 bits in the save           */
```

### 5.2 The 24 traits

Deltas are to the raw sliders (movement M, speech S (polite 0 .. blunt 15), expression E, attitude A
(serious 0 .. easygoing 15), quirk Q), summed over the three traits, **capped at +-6 in total** per
slider. "Edgy" traits are inert around children (5.4). Exclusive pairs cannot be picked together.

| # | Trait | M | S | E | A | Q | Bias (layer a and the plan) | Edgy |
|--:|---|--:|--:|--:|--:|--:|---|:-:|
| 1 | hot-headed | +1 | +3 | +2 | -3 | +1 | deny +2, refuse +1; patience -3 (engagement falls twice as fast on a cool act); picks fights with adults only | yes |
| 2 | romantic | 0 | -1 | +2 | +2 | 0 | accept +2 to warm acts; flirt replies (adults only, the gate decides); likes the romance-free roots feelings, music | yes |
| 3 | goofball | +1 | 0 | +2 | +3 | +3 | deflect +2 (a joke), counter +1 | |
| 4 | snob | -1 | +1 | 0 | -2 | 0 | deny +2 on food, stuff, house roots; refuse +1 for strangers; never mocks a person | yes |
| 5 | neat | 0 | -1 | -1 | -2 | -2 | likes house; dislikes dirt (filthy mood); accept cleaning | |
| 6 | slob | -1 | +1 | 0 | +3 | +1 | likes food, stuff; refuse +1 on activities | |
| 7 | bookworm | -1 | -2 | -2 | -1 | 0 | likes school, lore, hobbies; story depth gate -0; ask back +1 | |
| 8 | cheerful | +1 | -1 | +3 | +3 | 0 | accept +1, agree +1; mood floor ok | |
| 9 | gloomy | -2 | 0 | -2 | -3 | +1 | deflect +1, deny +1; accepts comfort; story depth gate +1 | |
| 10 | jealous | 0 | +1 | +1 | -2 | +1 | mentions "someone else" in hooks; deny +1 on a third person's praise; adults only | yes |
| 11 | loyal | 0 | -1 | 0 | -1 | -1 | defend / take side +2; friend reaches depth 3 without `level` 40 | |
| 12 | mean | 0 | +4 | 0 | -1 | +1 | deny +2, deflect +1, tease replies; **adults only** | yes |
| 13 | kind | 0 | -4 | +1 | +1 | -1 | accept +2, agree +1; comfort first | |
| 14 | lazy | -3 | +1 | -1 | +3 | 0 | refuse +2 on activities and favours | |
| 15 | ambitious | +2 | 0 | +1 | -3 | 0 | likes work; accept +1 on work and money; refuse +1 on idle activities | |
| 16 | paranoid | 0 | +1 | -1 | -3 | +3 | ask back +2; depth gates +1; refuse +1 for strangers | |
| 17 | self-assured | +1 | +2 | +1 | +1 | -2 | counter +2; never deflects a compliment | |
| 18 | shy | -2 | -2 | -3 | -1 | 0 | depth gates +1; refuse +1 in a room of 3 or more; short answers (voice `slow`, `flat`) | |
| 19 | outgoing | +2 | +1 | +3 | +2 | 0 | depth gates -1 (1 and 2); hook `call` +2 (calls others over) | |
| 20 | creative | 0 | -1 | +1 | +2 | +3 | likes hobbies, music; counter +1 | |
| 21 | glutton | -1 | +1 | +1 | +3 | 0 | likes food; accept +2 on food gifts and invitations | |
| 22 | curious | +1 | 0 | +1 | +1 | +1 | ask back +2; asks about the unknown word first | |
| 23 | stubborn | 0 | +2 | 0 | -2 | -1 | deny +1; keeps the stance against a counter; harder to change topic | |
| 24 | generous | 0 | -1 | +1 | +2 | -1 | accept +2 on gifts; offers a gift hook at depth 2 | |

Exclusive pairs: cheerful-gloomy, kind-mean, neat-slob, shy-outgoing, lazy-ambitious,
shy-self-assured, generous-snob. The list is data (`core/data/talk/traits.tsv`, compiled by
`talkc` into a 24-row ROM table of about 24 x 12 B = 288 B for the numbers plus 24 names and short
reads of about 600 B: **under 1 KB of ROM**, non-resident, in the Talk bank on PS1 and Dreamcast and
in the creator's ROM data on the GBA).

### 5.3 What the traits drive (and nothing else)

1. **Effective sliders** (layer c): `eff[k] = clamp(slider[k] + sum(delta), 0, 15)`.
2. **Reaction scores** (layer a): the "Bias" column, through `trait(K)` in 2.2.
3. **Stance**: `talk_gen_stance` gets `+2` for a topic in the liked root, `-2` in the disliked root
   (an exact like or dislike still wins).
4. **Accept or deny** a request, invitation, gift: the plan's score (accept vs refuse), plus the
   daily caps of section 6; a lazy resident refuses an activity, a generous one accepts a gift.
5. **What they bring up**: `talk_dm_idle` and the follow-up pick a topic from the liked root first;
   a disliked root is never brought up by the speaker.
6. **Depth** gates (3.1) and the **hook** choice (2.5).

Traits never touch the gate, the S3 lint, the stage rules, `mature_adult_ok`, the kid-cast list or
the romance conditions (`sim_romance_ok`): they run *inside* what the gate lets through.

### 5.4 Safety of traits (hard)

* **Edgy traits are inert around children.** For hot-headed, romantic, snob, jealous and mean:
  if the speaker is not an adult, or any non-adult is in the room (`aud.any_minor`,
  `aud.avatar_minor`, `aud.target_minor`, or the avatar is a minor), the trait's deltas and bias are
  **zero** and `lean` is not shifted by it. The check is in `talk_trait_effective()` in C, not in
  data. The creator also greys them out for a minor resident (`talk_trait_allowed(stage, trait)`),
  but the engine ignores the pick regardless.
* No trait adds a line, a guard value or a callback that harms, mocks or frightens a child. The
  cool banks keep `!kid_present` and the TK10 lint holds.
* **Romance is never a trait of a minor** and the romantic trait changes *tone only*: the act still
  needs `sim_romance_ok`, the gate and `TalkConv.consent`.
* Grief, distress and the K-7 path never read traits except for the voice's gentleness (a kind
  resident says the same sincere line; a mean one does **not** get a different line there). Grief
  is never a joke whatever the traits (TK K-10 lint holds).
* Likes and dislikes: a disliked root is "refused" with a polite deflection plus a counter-topic
  (7.1 rule L5), never an insult to the player, never for a safety or grief topic.

### 5.5 Where the traits live (the memory question)

The gene has 3 spare bits (`GF_SPARE`) and the record is 76 B, all in saves, goldens and the
roster ROM. Traits at the owner's estimate (20 bits a resident) would be 2.5 KB of RAM for 1,000
residents on every port: **more than the GBA's 2 KB of free EWRAM and 2.5 times the PS1's
992 B of margin above its gate.** So the traits do **not** widen the record. Three layers:

1. **Derived, 0 bits.** `talk_trait_derive(r)` gives every resident traits from the stored data:
   the sliders (each trait has a slider affinity: its deltas, 5.2, the closer the resident's sliders
   are to the trait's signature the higher it scores), plus one hash of the id for the tie break,
   exclusive pairs removed, best three kept, and a liked and disliked root from the specific likes
   (like_food -> food, like_hobby -> hobbies, like_music -> music, fear -> fears; the disliked root is
   the root of the hated food or the fear). Deterministic, the same on every platform, **0 B**.
   Every existing resident and every old save has traits this way ("new fields default from the
   sliders").
2. **Pinned, a small side table.** When the player picks traits in the creator the choice is stored
   as an override entry: `{uint16 id (10 bits) + flag; uint32 packed traits (25 bits)}` = **6 B an
   entry**, a table of **`TRAIT_PIN_MAX` = 24** entries = **144 B + 2 B count = 146 B** of `.bss`
   (`talk_traits.c`, **every** platform: the creator runs on all three consoles). It is saved in a
   new chunk 'TRAI' (146 B worst case, `save_budget.h` row; an old save has no chunk = no pins =
   all derived). A 25th pin refuses with "pinned personalities are full: release one" (the creator
   shows the count). The 3 spare gene bits are **not** used (the gene is shipped, SPEC 1.3).
3. **The talk state.** The effective set for a conversation is computed when it begins and kept in
   not stored: the traits are recomputed on demand from `talk_traits_get()` (a ROM table lookup plus the
   derivation), so no per-participant byte is used. `TalkPart.pad[2]` (16 bits) holds only the depth, thread, hook and kind
   bits of section 3.2 (9 bits).

**Budget (what must be reported by TM2a):**

| Platform | New resident bytes | Where from |
|---|---:|---|
| GBA EWRAM | **146 B** (`.bss`) of about 2 KB free | pin table; `make -C gba sizes` before and after |
| GBA IWRAM | 0 | the derive and effective functions are ROM code |
| PS1 | **146 B** of `.bss` against a margin of 992 B above the 72 KB gate; **the package must offset it** (for example by taking it from the scratch the house already owns, or by 146 B of dead code removed by `--gc-sections`) or report that it cannot land | `elf2exe.py` margin line, before and after |
| Dreamcast / native | 146 B | no budget issue |
| Save | +146 B worst case in the free-play slot (88 KB cap) | `save_budget.h` must still pass its static asserts |

If the owner does want every resident to own free picks (up to 1,000 x 3.1 B = about 3.1 KB), it
cannot land on the GBA or the PS1 without first freeing that RAM in another stream: say so in the
status file and keep the 24-pin table (the default shipped here).

### 5.6 The API CREATOR calls

All in `core/include/doomo/talk_traits.h` (new header, no Talk bank needed for the first four, so
the creator links without the generator):

```c
enum { TRAIT_NONE = 0, TRAIT_HOT_HEADED, TRAIT_ROMANTIC, TRAIT_GOOFBALL, TRAIT_SNOB, TRAIT_NEAT, TRAIT_SLOB,
       TRAIT_BOOKWORM, TRAIT_CHEERFUL, TRAIT_GLOOMY, TRAIT_JEALOUS, TRAIT_LOYAL, TRAIT_MEAN, TRAIT_KIND,
       TRAIT_LAZY, TRAIT_AMBITIOUS, TRAIT_PARANOID, TRAIT_SELF_ASSURED, TRAIT_SHY, TRAIT_OUTGOING,
       TRAIT_CREATIVE, TRAIT_GLUTTON, TRAIT_CURIOUS, TRAIT_STUBBORN, TRAIT_GENEROUS, TRAIT_COUNT };

/* the resident's traits: the pinned entry, else derived from the sliders; never fails */
void talk_traits_get(int id, TalkTraits *out);
int  talk_traits_pinned(int id);                    /* 1 if the player picked them          */
int  talk_traits_set(int id, const TalkTraits *t);  /* pin; 0 if illegal (see below) or full */
void talk_traits_unpin(int id);                     /* back to derived                      */
int  talk_traits_pin_count(void);
/* the legality the creator uses: a pick the engine would accept (no repeats, no exclusive pair,
 * no edgy trait for a minor, like != dislike, no romance or safety root) */
int  talk_trait_allowed(int stage, const TalkTraits *t, int trait);
/* creator rows: names and the one-line read */
const char *talk_trait_name(int trait);             /* "hot-headed"                          */
const char *talk_trait_blurb(int trait);            /* "loses their temper fast"             */
/* the judgement: title = the persona type's name ("Quiet Bookworm"), read = one line built from
 * the three traits and the likes ("Kind and shy, loves music, won't talk about work.") */
void talk_persona_judge(const DoomoResident *r, const TalkTraits *t, char *title, int tcap,
                        char *read, int rcap);
/* the live preview: one line of how this person talks to the player (needs the Talk bank, i.e. PS1
 * and Dreamcast and native; returns 0 where there is none) */
enum { TPV_GREET, TPV_ANSWER, TPV_OPINION, TPV_REFUSE, TPV_COUNT };
int  talk_persona_preview(const DoomoResident *r, const TalkTraits *t, int which, uint32_t salt,
                          char *out, int cap);
```

The preview is the **real engine** with `TalkConv.plain = 1` (the "short UI preview" flag that
exists today): a throwaway `TalkConv` (128 B) and `TalkLine` (104 B) on the creator's stack, the
resident record passed in (`talk_gen.c` gets one file-static `const DoomoResident *preview_res`, 4
B, read by `res_of()` for the id `TALK_PREVIEW_ID` = `TALK_PET_ID - 1`), the avatar's real stage for
the audience (a minor avatar previews a kid-safe line), the traits passed in so the player sees the
**unsaved** pick. `TPV_GREET` plays `core.greet`, `TPV_ANSWER` an ASK_ABOUT on a fixed neutral topic
("pie"), `TPV_OPINION` an ASK_OPINION on the liked root, `TPV_REFUSE` a request the personality
refuses (the plan with the action forced to a request, so a shy or lazy one says no). Four calls a
page, each about 80 us on the host. **GBA: the judge and the names only** (no generator on the GBA;
the creator may show a canned line per type: CREATOR's decision).

---------------------------------------------------------------------------

## 6. Doing as well as saying: sim actions a reply can carry

A reply can carry **one action** (`TalkPlan.action`). The rule, enforced in code and tested:
**the line says it only if the sim will do it, and the sim does it only if the line was said.**

### 6.1 The actions and the existing calls they use

| Action (`TACT_*`) | Triggered by | Text (existing + new) | Sim effect (existing call) | Cap |
|---|---|---|---|---|
| accept / refuse an invitation (to an activity) | player `TA_INVITE` + a venue or activity topic ("Want to do {x}?") | `core.invite` accept / decline banks (134 alternatives) + `act.start` (8, new) | accept: a resident **nudge** to the matching `ACT_*` (park, cafe, beach, concert, church, shop, workshop) for 1 game hour through one new sim hook `sim_nudge_activity(id, act, minutes)`; the house plays the walk | 1 a resident a day; 3 a town day |
| accept / refuse a favour or a request | player `TA_ASK_FAVOR` | `core.ask_favor` (122) | the problem path that exists: `TEV_PROBLEM` -> `sim_problem_resolve` | 1 a resident a day (exists) |
| give a gift | the resident's own offer hook at depth 2 or in a good mood, best band or better; player `TA_GIVE` (the player gives) | `core.offer`, `core.give` (134, 94) + `hook.gift` (4, new) | resident gives: `inv_add(item, 1)` for the resident's liked food (`talk_gen_like(id, 0)` mapped to an item); the player gives: the UI already calls `sim_gift(id, item)` and the reaction line reads its result | `TALK_CAP_GIFT_DAY` = 1 a resident, 3 a town day |
| ask for an item | the resident's problem is `PROB_WANTS_ITEM` or a `generous`/`glutton` hook | `cb.problem.item` + `act.ask_item` (4, new) | `sim_problem_raise(id, PROB_WANTS_ITEM, -1, item)` (exists) only if the resident has no open problem | 1 a resident a day |
| walk off | engagement under 15 for 2 turns; or a **refuse** with a reason ("I have to go") at kind rooms | `excuse` (10) exists | `TEV_WALKOUT` (exists) | exists |
| call someone over | the hook `call` (outgoing, curious, a rival or friend of the player) | `handoff` (12) + `act.call` (6, new) | the scene's `invite(id)` for a nearby resident; `sim_introduce(a, b)` when they are strangers | 1 a conversation; room size 4 |
| change mood or relationship | any accept, refuse, deny | the line itself | `rel_adjust`, `STAT_HAPPY` through `talk_outcome` | the existing caps (+-12 bond, +-12 mood a day) |

### 6.2 Mapping to `talk_outcome` and its caps

`TalkEvent` is `{kind, a, b, turn}` (4 B, 8 slots in `TalkState`); the new kinds go after
`TEV_HATE` (`TEV_COUNT` 11 becomes 15, still a `uint8_t`): `TEV_ACT_INVITE` (a = slot, b = ACT_*),
`TEV_ACT_GIFT` (a = slot, b = item index into the resident's liked items), `TEV_ACT_ASK` (a, b = item),
`TEV_ACT_CALL` (a = caller slot, b = the resident called). `talk_outcome_apply` handles them under a
new ledger row group next to the existing one (the ledger is about 900 B of `.bss`, native only: add
`gift`, `invite`, `ask` counters per resident, 3 bytes x 64 = 192 B native). **0 resident bytes on
the consoles**: `talk_outcome.c` is native (the house on a console reaches Talk through the same
`home_talk_*` hooks, TP packages).

**Plan-time cap check.** `talk_outcome_can(kind, id)` is asked when the plan is made. If the cap is
spent, the plan changes the kind to **refuse** with the reason "not today" (a counter hook offered),
so the text never promises what the caps will deny. A refused action costs nothing.

**Stage and safety rules for actions:**

* No action with a minor involves a romance partner, a date, alcohol or the vape shop (the sim's
  `ACT_VAPE_SHOP` and any item flagged adult are never a Talk invitation target for a minor, in
  either direction); a minor is never invited out of a kid-safe venue and never goes anywhere "alone
  with" an adult by a Talk action: the invitation action needs `!aud.any_minor` unless the venue is
  the house itself or a public kid-safe one (park, cafe, beach in daytime, church).
* A gift from or to a minor is a food or a toy only; never an item flagged adult or mature.
* "Call someone over" never pulls a romance partner into a room with a minor and never names a
  resident who is dead, a ghost the player has not met, or the pet.
* A hug toward a child stays a plain hug the child may decline (the existing rule): the plan's kind
  is accept or refuse; no action is attached.

---------------------------------------------------------------------------

## 7. The coherence lint and the census check

"The layers do not match the act and the topic" is a **measurable rule**. It has two enforcement
points: talkc (compile time, on the grammar) and the census (run time, on generated replies).

### 7.1 The compile-time lint (`talkc_gen.py`, rules L1 to L10, new `TK13` family)

talkc already knows each rule's `act:`, guards, fillers, `needs:` and the acts table; it also
derives the **layer** and the **reply kind** of every rule (a table in `acts.tsv` extended with a
`KIND` column, written by TM2c; the Python of `core/tools/talk_layers.py` is the prototype).

| Rule | Fails when | Today (measured) |
|---|---|---:|
| L1 | a core rule's `act:` is not a legal reply to its entry act (exists as Q2 by construction) | 0 |
| L2 | the reply kind from `act:` and the rule's `stance=` guard disagree (a `stance=pro` rule whose act is `no`, `opine_neg` or `disagree`; a `stance=con` rule whose act is `yes`, `agree`, `opine_pos`) | not yet checked |
| L3 | **topic present, topic unused.** A core rule that answers a **question about a topic** (entries `ask_about`, `ask_opinion`, `change_topic`, `gossip`) and has no topic slot (`{x}`, `{x.*}`, `{like}`, `{food}`...) and no `!topic` / `notopic` / `likes_topic` guard and no `tags: generic`. The only escape is an explicit `tags: generic` (a recorded decision) | **57** alternatives (ask_about 10, ask_opinion 15, change_topic 10, gossip 22) |
| L4 | **pronoun with nothing to point at.** A rule whose line says "it", "that", "this", "them" (about the topic) and has no `topic` guard and no slot | **51** alternatives (of the 60 topic-blind `opine_pos` / `opine_neg` lines, 51 say "it", "that" or "this") |
| L5 | **dead-end reply.** A rule with reply kind `deflect`, `deny` or `refuse` to a *request or question* that carries no counter, no reason and no hook in the same rule (the line only closes the door). The bank gets a `tags: closes` for the few intended | measured by TM2b |
| L6 | **filler as the whole reply.** The visible text (line codes, faces and `*stage directions*` removed) is a filler word from the list (hm, mm, uh, oh, sure, ok, fine, yeah, right, well, wow, cool, whatever, i see, if you say so, go on...) or under 12 visible characters, in an act that is not backchannel, wave, nod, high_five, laugh, greet, farewell, hug, give | **22** alternatives (F1 5, F2 17) |
| L7 | **hook out of place.** A `follow.*` rule whose question act is not adjacent after the acts it follows (`acts.tsv`), or that asks about a topic but has no `topic` guard, or repeats the question class the player just asked | measured by TM2b |
| L8 | **depth without gate.** A rule with `depth>=2` and no `rel=` or `fact.*` guard; a rule with `depth>=3` and no `rel=` of best, partner or family | new rules only (0 existing) |
| L9 | **trait outside its lane.** A `trait=` guard on an edgy trait without `adults_only&!kid_present`; any trait guard in a refusal, grief, distress, pet or K-7 bank | new rules only |
| L10 | **a non sequitur marker.** A core rule in a topic act whose text carries a *fixed* concrete noun phrase and **no** topic or fact slot (the "Needs salt." family): flagged for review, fixable only by a slot or `tags: generic`. It is a **warning with a budget** (count in the baseline) not a failure | **35** (the N2 scan; heuristic) |

Failure policy in talkc: L1, L8, L9 fail the build at once (they only apply to new data and to
safety). **L2 to L7 start as a warning table with a frozen baseline** (`core/data/talk/lint_baseline.tsv`:
rule name -> finding) and **fail on any finding not in the baseline**; every package that fixes a
finding removes it from the baseline (a ratchet, so the count only goes down). By TM2e the baseline
for L3, L4, L6 is empty. Fixing is **a guard or a tag, not a deletion**: `needs: topic` / `!topic`,
or `tags: generic` with the owner's reviewed reason; an unfixable line moves to a rule only used
when no topic exists.

### 7.2 The census check (Q8, in `talk_census_check`, new `QF_Q8_*` failures)

The census already checks Q1 to Q6 plus K-10 on every generated exchange from the **trace**
(`TalkTrace`: alternatives used, fills, `said[]` concepts, callback, parts, `core_alt`). Q8 adds
five checks, each a function of data the trace already has plus the plan (the generator writes
`kind`, `depth`, `content`, `hook` into a new `TalkTrace.plan` byte block, 6 B, native and census
only: `TalkTrace` is a tool struct, not resident):

| Check | Failure code | Rule (measurable) |
|---|---|---|
| Q8a reaction | `QF_Q8_KIND` | the line's reply act is in the kind set the plan chose **and** the kind is allowed after the player's act by `acts.tsv` **and** it is consistent with the plan's `lean`: lean +1 or +2 and kind in {deny, refuse} (a "no" to something the resident likes) is a failure unless the plan recorded a reason (refuse of a request: tired, lazy, stranger, cap); lean -1 or -2 and kind in {accept, agree} likewise |
| Q8b content | `QF_Q8_CONTENT` | the player gave a topic (or the thread has one) and the reply is a topic act reply: `said[]` must contain the topic **or** a 1-hop association of it **or** the line must hold a pronoun alternative whose guard required the topic. A fill of any other concept that is not an association (a stray "tape 3", "taxes") in the *core* part of a topic reply fails; a fact callback is allowed |
| Q8c colour | `QF_Q8_VOICE` | the voice guards of every alternative used still hold for the **effective** sliders (Q3 against `eff`, not against the stored sliders); an edgy trait never shaped a line when a minor was in the room (re-derived from the room); no persona's tic from another type |
| Q8d hook | `QF_Q8_HOOK` | a hook line's act is adjacent after the reply's act (`acts.tsv`); its topic is the reply's topic or an association; it does not ask the question the player just asked (same act and topic as the player's turn); after a **counter** or a **refuse of a request** the exchange has a hook (the "dead end" check) |
| Q8e filler | `QF_Q8_FILLER` | the **whole exchange** (all of one speaker's lines for this turn) has at least 20 visible characters, **or** the player's act is a gesture, a greeting, a farewell, a thank you or a laugh; and it never consists of a filler word alone (the L6 list) |
| Q8f unknown | `QF_Q8_UNKNOWN` | a turn parsed to `TA_UNKNOWN` or with `TTF_UNK` tokens gets a reply with the `unk.*` plan (names, guesses or asks); a reply to it that is a joke, a backchannel or a change of topic **without** having asked is a failure |

**Failure budget.**

* Smoke census (`make -C core test`, 20,000 contexts): **0** `Q8a`, `Q8e` and `Q8f` failures; `Q8b`,
  `Q8c`, `Q8d` at most 0.5% each and 1% together. (The same 1% bar as the other quality checks.)
* Full census (2,000,000 contexts): `Q8` total at most 0.5%; **Q8a, Q8e, Q8f 0**.
* The Q5 repeat rule, S3 hits 0 and K-10 0 are unchanged.
* Any exchange that fails Q8 is written to the `--fails` file with the template file and line, so
  the fix is a guard or a tag, never a guess.
* TM2b lands the check in **report mode** (counted, not failing) and fixes what it finds; TM2e turns
  the budget into a test. The baseline of Q8 failures on the **old** engine (no plan) is measured
  by TM2b's first run and recorded in the status file; the multilevel composer is accepted only if
  it brings every Q8 class at or under budget.

---------------------------------------------------------------------------

## 8. The 1,000,000 conversations, by composition

### 8.1 What the target means (three tiers, unchanged from design 3.7)

| Tier | Target | Last full run, 2M draws (5,536 alt.) | This tree, 400,000 draws (8,891 alt.) |
|---|---|---|---|
| **Surface** (distinct exchanges after normalising names and numbers) | 1,000,000 | 775,601: **missed** | 254,159 (63.5% unique), Chao1 1,588,804 |
| Skeleton (fillers replaced by their category) | 100,000 | 727,337: met | 243,043: met |
| Meaning (act, stance, topic root, callback, beat) | 5,000 | 41,760: met | 28,196: met |
| **Conversations** (6 turns, differ in at least 2 turns) | 1,000,000 | 1,999,886 of 2,000,000: met | 20,000 of 20,000 |

The owner's "1,000,000 different conversations" is met at the conversation tier today. The Surface
tier is the open one and the design's headline.

### 8.2 Why composition fixes it (the arithmetic)

The census draws 2,000,000 contexts; the distinct count of D draws from a support of N equally
likely replies is `N (1 - e^(-D/N))`. For **1,000,000 distinct out of 2,000,000 draws** a uniform
draw needs N of about **1.26 M** (solve `(1-e^-x)/x = 0.5`, x = 1.59). Two measurements say how far
the bank is: the old 2M run (775,601 distinct) is an **effective uniform support of about 0.86 M**
(x = 2.33); the current tree at 400,000 draws (254,159 distinct, 63.5%) is about **0.41 M** at that
draw count (x = 0.97; the effective support of a lopsided draw grows with the number of draws, so
the 2M figure is higher, which is why the lead's full run is the real gate). The Chao1 estimate of
what is reachable is much larger (1.6 M at 400k draws, 3.9 M in the old run): the bank is not too
small, **the draw is concentrated on a few high-weight alternatives**. So the target needs the
**effective support to grow by about 1.5 to 3 times**, not the bank to grow.

Composition does that because the four layers are **independent picks**. The number of distinct
texts of a turn is the product of the layers' effective choices:

```
N_turn = (kinds the act allows) x (b: core pool x reason/fact pick) x (c: voice, opener, tag) x (d: hook) x (topic fills)
```

Real numbers for the factors (from `talk_layers.py`, `talkc --stats`):

| Factor | Measured pool | Live after guards (estimate, TM2e measures it) |
|---|---|---|
| kinds an act allows (a) | 2 to 5 per act (`acts.tsv`) | 2.5 |
| b: core alternatives of an act's `core.*` rules | 36 to 714 per act (ask_about 714, tell 408, opine_pos 269, joke 239...) | 15 to 40 |
| b: the reason pick (depth 1) | attributes 2.4 a concept (at most 8), associations 7 a concept | 5 |
| c: opener x tag x voiced core | 63 x 64 x 1,350 voiced | 3 to 6 combinations used (30% opener, 40% tag) |
| d: hook | follow 45 + callbacks 32 + hand-offs 12 + the 120 new lines | 4 to 8 |
| topic fills | 4,009 concepts (about 400 in the census grid) | 400 |

For one (act, resident cohort) cell: `2.5 x 20 x 5 x 4 x 6 = 6,000` text variants per topic against
today's roughly `1 x 20 x 1 x 2 x 3 = 120`: **about 50 times the per-topic fan-out for the acts
that take a topic** (ask_about, ask_opinion, tell, opine: together 27% of the 7,217 core
alternatives of the acts the census draws: 714 + 319 + 408 + 269 + 247 = 1,957, and the most
varied ones). Even if the live pools come out at **a fifth** of these estimates, the
effective support grows by far more than the 1.46 times needed, and **the draw flattens** because
each part is an independent weighted pick (the product of four independent distributions is much
less concentrated than one).

For the acts that take **no** topic (greet, thank, farewell, gestures: about half of the 55 census
acts) the fan-out comes from kinds x voice x hook only (about `2.5 x 20 x 4 x 4` = 800 against about
200 today) and from the trait-shifted voice: a resident's effective sliders move across the same
persona cohorts, so two residents of the same type with different traits no longer share a
distribution.

**Conversations (the 1M conversation target)** are the same multiplication over 6 turns with the
depth and the thread state as extra memory: two conversations differ whenever the plan's kind,
depth or hook differ even if the topic repeats. The tier is already met; the plan only adds
margin and keeps it met under the Q8 filter (the filter removes replies; the check must be run on
the full census after the filter is on).

### 8.3 Baselines on this machine (same commands the packages use)

* `core/build/talk_census --quiet --grid core/data/talk/census_en.tsv --contexts 400000
  --conversations 20000` (the medium run, about 110 s) and `make -C core talk-census` (the full
  gate): the baseline is in 1.3. A package reports the same numbers before and after.
* **Acceptance for TM2e** (the end of the series): Surface >= 1,000,000 distinct in 2,000,000
  contexts (the existing `make -C core talk-census` already exits 1 when a target is missed);
  Q8 inside the budget of 7.2; the other tiers not lower than the baseline.

---------------------------------------------------------------------------

## 9. Which banks make filler non-responses and non sequiturs (counts)

Scan: `python3 core/tools/talk_layers.py fillers --list 10 --tsv fillers.tsv` (the sheet lists every
finding with `file:line`, rule, guard and text; the evidence copy is `fillers.tsv`, 470 rows).
Classes, all counted over the 8,891 alternatives:

| Class | Meaning | Alternatives | Where |
|---|---|---:|---|
| F1 | the whole line is a filler word ("Hm.", "Yep.", "Heh.", "hmph.") | **5** | `react.laugh.flat` (parts.tlk), `react.backchannel`, `core.backchannel.r_back`, `core.joke.lord_vapula_v1`, `core.high_five.mother_nox_v1` |
| F2 | under 12 visible characters (stage directions removed) | **17** | `core.backchannel` 3, `core.laugh` 3, `core.wave` 2, `core.high_five` 2, `core.greet` 2, `core.thank` 2, `core.tell.r_back` ("Oh? Go on."), the cast's one-word lines ("Clack!", "UP TOP!") |
| N1a | a **question about a topic** (ask_about, ask_opinion, change_topic, gossip) or an opinion (opine_pos, opine_neg) answered by a line that names no topic and whose guards do not need one | **117** | `core.opine_neg` 31, `core.opine_pos` 29, `core.gossip` 22, `core.ask_opinion` 15, `core.ask_about` 10, `core.change_topic` 10 |
| N1b | a topic act (tell, answer, agree, disagree, continue) answered topic-blind: a generic listener line ("Interesting. I'm picturing it now...") | **296** | `core.tell` 139, `core.answer` 65, `core.disagree` 45, `core.agree` 34, `core.continue` 13 |
| N2 | a topic-change phrase ("anyway", "by the way", "speaking of") with no topic slot | **35** | `core.apologize` 4, `core.give` 3, `core.walk_away` 3, `core.ask_fact`, `core.ask_advice`, `core.criticize`... |
| (unknown) | the `core.unknown.*` bank answers a line it did not understand with a joke or a backchannel ("Bees? ...Anyway.", "Whatever that was.", "I'm going to take that as a compliment.") | 12 rules, 24 alternatives, to be scanned for Q8f by TM2b (no count yet) | `core_other.tlk:66-75`, `rest_talk.tlk:73-98` |
| (fused wrong-kind risk) | `core.*.pro/.con/.neutral` where the stance guard and the reply act could disagree | 346 | checked by L2 (0 known failures; the guard is `stance=` already) |

Reading the numbers:

* **True filler (F1, F2) is small: 22 alternatives (0.25% of the bank).** These are the "Hm." and
  "Mm-hm." lines. Most are legitimate answers to a gesture or a backchannel (`core.wave`,
  `core.high_five`, `core.backchannel`); the lint allows them only for those acts (L6) and flags
  the rest (`core.laugh.plain` "Ha! Right?", `core.tell.r_back` "Oh? Go on.", the Vapula and Nox
  one-word lines) for a guard that keeps them as an **opener or tail**, not the whole reply.
* **The non sequitur problem is not short lines, it is topic-blind lines**: 117 + 296 = 413 core
  alternatives (4.6%) answer a topic act without naming the topic. Many are fine as an *agree* to an
  opinion ("You're right, it's a good one" says `it` and works only when a topic exists: that is
  L4, 51 of them): they need a `topic` guard. The rest are generic listeners that are right when
  the player **told** something with no topic and wrong when a topic is on the table.
* **Fixing without deleting** (the bank is frozen): a topic guard (`topic`) on the pronoun lines;
  `!topic` on the generic listeners, so a topic act with a topic falls to the topic banks; and for
  the 35 N2 and the unknown-bank lines a `generic` tag with the baseline. The expected result is
  that the **same text still exists** and speaks only when it is coherent.
* The weighted share (how often the generator actually *says* one of these) is the first number TM2b
  produces (`QF_Q8_CONTENT` and `QF_Q8_FILLER` in report mode on the old engine).

---------------------------------------------------------------------------

## 10. The minimal connective lines still needed

The bank is frozen, so the new data is only what the composer cannot do with an existing line.
**About 108 alternatives in about 40 rules** (1.2% of 8,891; about 5.4 KB in the Talk bank at the
bank's 45 B an alternative, **non-resident** on the PS1, which loads a subset). Every line follows the
house register (PG-21 for adults, kid-safe wherever a child may be in the room: the guards say which),
at most 3 sentences, fits the bubble, no register word in a kid-safe rule, no romance, no `jmat`.

| Group | Rules | Lines | What they are | Guards |
|---|---|---:|---|---|
| `reason.pro`, `reason.con`, `reason.neutral` | 3 | 16 | a clause that joins to the stance core at depth 1: "...because {x.it} {x.is} {x.adj}." / "...{x.assoc:cap} will do that to a person." Uses `{x.good}`, `{x.bad}`, `{x.adj}`, `{x.assoc}` | `depth=1`, `stance=`, `x.adj` etc. needs |
| `story.job`, `story.family`, `story.news`, `story.problem`, `story.mem`, `story.like` | 6 | 14 | the bridge into a depth 2 story: "That takes me back. I was {job:indef} when..." followed by the existing `cb.*` fact line (no invented backstory: only the fact the sim has) | `depth=2`, `fact.*` |
| `secret.like`, `secret.fear`, `secret.hate`, `secret.problem` | 4 | 16 | the depth 3 reveal: "Don't tell anyone, but I'm scared of {fear}." / "Okay. I secretly love {like}." Kid variants for a child speaker (the same facts) | `depth=3`, `rel=`, `kid_speaker` variants |
| `unk.guess`, `unk.name`, `unk.askmeaning`, `unk.noecho` (+ kid) | 5 | 30 | section 4 | `act=unknown` / `plan` |
| `act.start`, `act.ask_item`, `act.call`, `hook.gift` | 4 | 22 | the lines that propose or announce an action (start an activity, ask for an item, call someone over, hand over a gift) | `kind=accept` / `hook=offer` / `hook=call`; adults-only where the venue is |
| `hook.thank`, `hook.apologize`, `hook.compliment`, `hook.feeling` | 4 | 10 | the follow-up for acts `follow.tlk` has no rule for | `pact=`, `topic` |
| `fix.*` | 0 | 0 | the repairs of section 9 are guards and tags on existing rules, not lines | |
| **total** | **26** | **108** + 12 spare | | |

Not needed (they exist): accept and refuse of an invitation or a request (`core.invite`,
`core.offer`, `core.ask_favor`, `core.yes`, `core.no`: 134, 134, 122, 86, 86 alternatives), a
walk-off (`excuse`), a callback (`cb.*`), the sincere banks, the cast voices, the persona tics.

New guard predicates (talkc and `talk_gen.c`, TM2c; the term byte has room: 71 predicates today):
`kind=` (TK_*), `depth=` (0 to 3), `hook=` (TH_*), `trait=` (TRAIT_*). Existing rules get **no**
edit for them: talkc derives `kind` from `act:`.

---------------------------------------------------------------------------

## 11. Memory budget (summary)

| Item | Where | Bytes | Resident on |
|---|---|---:|---|
| `TalkPlan` | stack | 13 | transient |
| depth, kind, hook state | `TalkPart.pad[2]` | 0 new | in `TalkState`, house scratch |
| unknown word echo + taught ring | `TalkState` | 18 + 12 = 30 | house scratch (TalkState stays under 1,000 B of 1,536 B) |
| depth and secrets told | `TalkMem.spare` | 0 new | native only today (talk memory is native-only): on the Dreamcast depth and secrets are not kept between sessions, so the persistence test is native-only |
| trait pins | `.bss`, new chunk 'TRAI' | 146 | **all** platforms |
| trait ROM table + names + reads | ROM | about 1 KB | Talk bank / creator data |
| action events and the ledger | native `talk_outcome` | about 190 | native |
| preview pointer | `talk_gen.c` static | 4 | where the generator links |
| new connective lines | `TALK_en.BIN` | about 5.4 KB (108 lines x 45 B) | PS1 non-resident (disc), DC / native |
| new code | native, DC, PS1 overlay | est. 6 to 9 KB of MIPS code (the generator part is 22 KB today) | PS1 overlay: **TM2c must measure and offset**; the PS1 margin is 992 B above the gate |

**The PS1 rule of this wave: add 0 resident bytes.** The pin table (146 B) and the planner code are
the only candidates; the packages must either keep the planner inside the existing Talk overlay (it
is not resident), place the pin table where `--gc-sections` or a smaller existing table frees the
bytes, or **say that they cannot land** with the measured numbers (`elf2exe.py` margin line before
and after). The design of the composer itself (the plan, the lint, the census) costs 0 resident
bytes on the GBA: Talk is not linked there.

---------------------------------------------------------------------------

## 12. Safety invariants (hard; each has a test in section 13)

1. The gate, the S1 classifier, the S2 audience rules, the S3 output lint and K-7 / K-5 / K-8 /
   K-10 are untouched and **run first and last**: the plan is created after the gate says pass and
   every composed line goes through S3.
2. Adults-only romance both ways; no romance with, by or near a minor; no teen-teen romance; the
   romance acts keep the target-only answer and `TalkConv.consent`. The `romantic` trait changes
   tone, never the conditions (`sim_romance_ok`).
3. Never weaken `mature_adult_ok` or the kid-cast list. No trait or plan reads or writes either.
4. A hug toward a child stays a plain hug the child may decline.
5. No trait makes anyone harm, mock, frighten or pressure a child. The edgy traits (hot-headed,
   romantic, snob, jealous, mean) are **zero** around any non-adult (`talk_trait_effective`, in C).
6. Grief is never a joke: grief topics bypass the plan; sincere banks take no hook that jokes; no
   trait changes a sincere line; a grief word is never a "secret". Distress and self-harm talk stay
   the gate's: care, never a mechanic or a joke.
7. No real-world hate. The unknown-word echo (section 4) is guarded by E1 to E7; a word within one
   edit of a hate or register word is never echoed; kids' rooms echo less. Output stays in the
   closed vocabulary apart from the one quoted word.
8. Gore only on adults and demons: unchanged (Talk has no gore; cartoon violence jokes stay in
   `!kid_present` banks).
9. A reply that carries an action says it only if the caps allow it and the sim does it only if the
   line was said (6.2).
10. A kid room is a kid room for the rest of the talk (`TDF_KIDSAFE`); the plan's selector removes the
    cool reply acts there.
11. Old saves load: no chunk, no pins, depth 0, nothing told.
12. Determinism: the plan reads only the conversation seed, the turn, the sim and the talk memory;
    the same talk gives the same text on every platform.

---------------------------------------------------------------------------

## 13. The packages (TM2a to TM2e) and their acceptance tests

One day each for a Sonnet agent, in this order. **Each package is its own commit series on the shared
branch; the fast tests and `talk-check` pass at every commit.** TALK lands the model first (TM2a), so
the CREATOR stream can start CR-PERS on the API of 5.6 as soon as it lands.

### TM2a: the trait model, the API, the save

*Build.* `core/include/doomo/talk_traits.h`, `core/src/talk_traits.c`, `core/data/talk/traits.tsv`
(talkc compiles the 24-row table; or a C table if talkc is not needed by the creator build),
the 'TRAI' save chunk (registered on all platforms, `save_budget.h`), `talk_traits_derive`,
`talk_trait_effective` (the edgy rule), the judge. The preview waits for TM2c (it needs the plan);
the function exists and returns 0.

*Acceptance tests* (`core/tests/test_talk_traits.c`, each fails without the code):
1. `traits_derive_deterministic`: every id 0..999 derives the same set twice and on a fresh process;
   no repeats, no exclusive pair, 3 at most, like != dislike.
2. `traits_derive_follows_sliders`: a resident with blunt, loud, serious sliders gets hot-headed or
   mean more often than kind over 1,000 residents (threshold: at least 3 times).
3. `traits_pin_roundtrip`: pin, save, load: equal; an old save (no chunk) loads with 0 pins and
   every resident derived; a 25th pin refuses.
4. `traits_edgy_inert_for_minors`: for every edgy trait and every minor stage, `talk_trait_effective`
   returns zero deltas and bias; with a minor in the audience the same for an adult speaker.
5. `traits_allowed`: `talk_trait_allowed` refuses an edgy trait for a minor, an exclusive pair,
   a romance or safety root as like or dislike.
6. `traits_effective_sliders`: deltas sum, cap at +-6, clamp 0..15; `persona_type` reads the stored
   sliders.
7. `traits_judge_text`: the title is the persona type name; the read line has no register word and
   fits one 26-column line for every legal pick (exhaustive over 24 choose 3 minus exclusions).
*Measurements:* GBA `make -C gba sizes` before and after (EWRAM delta must be 146 B or less),
PS1 margin before and after (**must be offset to 0 net or reported as unable to land**), `check-gen`,
save budget asserts.

### TM2b: the coherence lint, the filler scan, Q8 in report mode

*Build.* `talkc_gen.py`: the layer and kind derivation, lint L1 to L10 with the baseline file
`lint_baseline.tsv` (the 470 findings of section 9 are the starting baseline), the seeded bad grammars
for each rule (`core/tests/talk_bad/gen_*.tlk`, selftest count grows by 10); `talk_census.c`: the
`QF_Q8_*` codes and the report; `core/tools/talk_layers.py` stays as the analysis tool. **No engine
change and no bank change** except guard/tag fixes of section 9 that need no engine support
(`topic` and `!topic` guards on the L4 pronoun lines and the generic listeners, `generic` tags).

*Acceptance tests:* (1) each seeded bad grammar fails with its message (talk-check selftest);
(2) `make -C core talk-check` passes on the real bank with the baseline and **fails** when one new
rule violates L3 (a test adds a rule to a copy of the bank); (3) `talk_census_q8_report`: smoke run
prints the Q8 table and counts on the old engine (the baseline is written to the status file);
(4) L4: after the guard fixes the L4 baseline is 0 (the 51 pronoun lines now need a topic) and the
smoke census shows no regression in quality failures, Surface or conversation tiers (same numbers
within 1%); (5) no line deleted: `talkc --stats` alternative count equal or higher.

### TM2c: the plan, layer a and layer c

*Build.* `talk_plan.c` (`TalkPlan`, `talk_plan_kind`, the effective sliders into the voice
guards), the new guard predicates `kind=` and `trait=` in talkc and `talk_gen.c`, the soft filter in
`gen_line()`, `acts.tsv` `KIND` column, the preview (`talk_persona_preview`), the stance shift
for liked and disliked roots, the `TalkTrace.plan` block. Layers b depth 0 and d as today.

*Acceptance tests* (`core/tests/test_talk_plan.c`, plus the census):
1. `plan_kind_follows_stance`: over 5,000 random (resident, topic, ASK_OPINION) the share of
   accept/agree kinds when `lean` is +1 or +2 is at least 80%, and the share of deny/refuse when
   `lean` is -1 or -2 is at least 80%.
2. `plan_trait_bias`: a kind resident accepts at least 20 points more often than a mean one on
   the same invitation set (adults only); a lazy resident refuses an activity at least 20 points
   more than an ambitious one.
3. `plan_relationship`: a rival accepts under 15%; a best friend or partner over 70% of the same
   accept-able requests.
4. `plan_sincere_bypass`: grief, safety acts, gate refusals never get a plan (no kind, no hook).
5. `plan_kid_room`: with a child anywhere, no plan chooses tease, mock, insult, threaten, no edgy
   trait has effect (8 edgy-trait x 4 stage cases).
6. `plan_deterministic`: same seed, same plan and text; different on a different turn.
7. `plan_soft_filter_counts_misses`: when no rule passes, the line is still produced and the miss is
   counted (the census shows it); the smoke census shows the soft-filter misses under 1%.
8. `preview_four_lines`: `talk_persona_preview` returns a non-empty, S3-clean, in-voice line for
   every trait set tested; a minor avatar gets a kid-safe line; the same call twice gives the same
   line; unsaved traits change the line.
9. `census_q8a_q8c_zero`: the smoke census has 0 `QF_Q8_KIND` and 0 `QF_Q8_VOICE` failures and no
   other quality failure; Surface, Skeleton and conversation tiers no lower than the baseline.
*Measurements:* the MIPS object size and stack of `talk_plan` and `gen_line` (the generator's budget
is about 5 KB of stack), GBA unchanged (not linked), PS1 margin line and `game_smoke`, `dc-host-test`.

### TM2d: layer b depth, layer d hooks, the unknown word

*Build.* the depth state and gates (3.1) in `talk_dm.c` and `talk_plan.c`; `TalkMem.spare` bits;
`reason.*`, `story.*`, `secret.*`, `hook.*`, `unk.*` rules (the connective lines of section 10, 108
lines, written to the house register) and their lints (L8); `talk_lex_near`; the `{u}` slot (TK13);
the `TEV_LEARNED` event that finally records "the player learned a like" (the open item of T6-OUT);
the unknown-word flow in the scene (DC and native: the parse info carries the first unknown token's
offsets into `shown`; one new pair of `uint8_t` in `TalkUtter`'s tail, 2 B).

*Acceptance tests:*
1. `depth_gates`: a stranger never reaches 2; a rival never past 1; a friend without `level` 40 never
   reaches 3; a best friend with 3 turns, engagement 70 reaches 3 and tells exactly one secret that
   is a real unknown like or fear of that resident (the test reads `talk_gen_like`); the secret is
   then marked known and not told again the same day.
2. `depth_resets`: a topic of another root resets to 0; an insult lowers by 1; a walk-out to 0;
   the memory remembers the reached level: after a save and load the next talk starts one level lower.
3. `depth_never_adult_for_kids`: over 2,000 sessions with children, every depth 2 and 3 line is
   kid-safe (S3 strict probe and no register word) and no secret is romance, body or harm.
4. `unknown_word_named`: 200 invented words ("florp", "zibbleduck"...) get a reply with the
   `unk.name` or `unk.guess` plan; the word is said quoted and lower case at most once per talk.
5. `unknown_word_never_echoed_when_unsafe`: every entry of the adversarial corpus (hate, register,
   minor, leet, fuzzy, person, digits, all-caps) gets `TPL_NOECHO` and the output contains none of the
   input's tokens (substring test on the bubble text), S3 clean, and a kid room never echoes a word
   over 12 letters.
6. `hook_after_counter_and_refuse`: 100% of counters and request refusals in the smoke census have a
   hook (Q8d); no hook after a sincere line.
7. `census_q8_budget`: smoke census within the section 7.2 budget (all Q8 at 0 for a, e, f; at most
   0.5% each for b, c, d).
*Measurements:* `TalkState` `sizeof` still under 1,000 B and the 1,536 B assert, GBA and PS1 as
before, `TALK_en.BIN` growth (about 5 KB), the PS1 subset size line.

### TM2e: sim actions, the full census, the ratchet

*Build.* the `TACT_*` actions (6.1), the new `TEV_ACT_*` events and ledger rows, `talk_outcome_can`,
`sim_nudge_activity` (a small sim-side hook; the SIM owner is told in the status file; if the SIM
owner has not landed it, the action falls back to "no effect, the line says 'maybe later'" and the
test is skipped with a visible note), the scene wiring (invite, call over), the lint baseline for
L2 to L7 emptied or each remaining line `generic`-tagged with a reason, Q8 failing in the smoke
census, docs.

*Acceptance tests:*
1. `action_says_only_if_done`: over 3,000 sessions every `TLF_ACTION` line has a matching event that
   `talk_outcome_apply` applies, and every applied event has a line.
2. `action_caps`: invitation 1 a resident a day and 3 a town day; gift 1 and 3; ask 1; after a cap
   the plan refuses with a reason and the sim state is unchanged.
3. `action_minor_rules`: no invitation to the vape shop or a date for or with a minor, no adult item
   as a gift to or from a minor, no call-over that puts a partner with a child (an exhaustive stage x
   venue x item table).
4. `action_sim_effects`: an accepted invitation makes `sim_nudge_activity` run once and the resident's
   `activity` becomes the venue's for the hour; a gift moves one item in the inventory; an ask raises
   one `PROB_WANTS_ITEM` only without an open problem.
5. `lint_baseline_empty` for L3, L4, L6; no new `generic` tag without a reason column.
6. **`make -C core talk-census` exits 0 on the full run**: Surface >= 1,000,000 distinct, Skeleton,
   Meaning and conversations not below the baseline, Q8 total <= 0.5% with Q8a, Q8e, Q8f 0, S3 hits 0,
   K-10 0, Q5 repeats under 1%. If Surface is short, the package reports the measured number and the
   gap and does not call it done.
7. P2 / P3 / P7 (the safety property tests over 1,500 random sessions) pass with traits and actions
   on.
*Gates for every package:* `make -C core -j2 test` ("N passed, 0 failed"), `make -C core test SAN=1
BUILD=build-san -j2` (no new report), `make -C core check-gen`, `sh core/tools/xcheck.sh`, GBA build,
test and sizes, the PS1 build with the margin line and `game_smoke`, `make dc-host-test JOBS=2`;
`docs/TALK.md`, `ROADMAP.md` and `status/TALK.md` updated.

---------------------------------------------------------------------------

## 14. Open items for the owner and the lead

* **The pin table is 24 residents, not 1,000.** The owner's estimate (20 bits for every resident,
  3 KB or more) does not fit the GBA's 2 KB of EWRAM or the PS1's 992 B of margin. The default here is
  *derived for everyone, pinned for up to 24 chosen by the player* (146 B). If every resident must
  own free picks, RAM has to be freed elsewhere first. **Owner decision wanted** (default: 24).
* **The unknown-word echo is the first exception to "output uses only the closed vocabulary".** It is
  fenced by E1 to E7 and a property test; if the owner prefers no echo at all, section 4 steps 1 and 3
  (guess and ask) still work and `unk.name` is dropped (30 lines become 22).
* **`sim_nudge_activity`** is a sim-side hook the SIM owner must accept (or TM2e ships the fallback).
* **PS1 resident bytes:** the planner code and the pin table must be offset (section 11); if they
  cannot be, the package says so with the margin line and does not land.
* **The line fixes of section 9 are guards and tags, not deletions**, so Q7's human read of the
  affected lines (the owner's) is still owed after TM2b.
* **GBA:** the personality screen has the judge and the names, no live preview, unless the GBA gets
  the generator (it has not): CREATOR's decision.
