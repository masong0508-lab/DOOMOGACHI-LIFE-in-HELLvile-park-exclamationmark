# The game's script style guide

**Status: DRAFT for the user's approval (P0, 1 November 2026; revised for
the user's correction: the game's indie-game parody stays the backbone,
and "the quiet part out loud" is one tool, §0, §1.0, §1.1).** Every
package that rewrites game text (W1-W19, C1, L7, X7) writes to this guide.
Locked lines, ids and test pins are in [`SCRIPT_LOCKS.md`](SCRIPT_LOCKS.md);
the tools are in [`docs/STORY_ENGINE.md`](../../STORY_ENGINE.md#script-tools).

**What wins when documents disagree.** The user's decisions
(`DECISIONS.md`, 1 November 2026) beat this guide, and this guide beats the
older notes for game text: `story_final.md` §7, `comic_timing.md`,
`bible_heart.md`, `bible_horror.md`, the three `GRUMBLEGUTS_*` notes and the
show's `VOICE_BIBLE.md` and `GAG_BIBLE.md`. Those older documents are still
the reference for timing codes, set pieces and character history. They are
not the reference for register, canon or tone. Where they differ, this
guide says so explicitly.

---------------------------------------------------------------------------

## 0. The brief in one breath

The whole game is being rewritten to be **sharper and funnier**, and it
stays **the game it already is: an indie game that parodies its own
genres**. It is a tamagotchi, a Tomodachi Life and DOOM sewn into one plush
toy, and **the mash-up is the joke**. A pastel life sim plays its pet care,
its neighbours and its furniture ratings completely straight, then turns
into a metal shooter that plays its gibs completely straight too. Most of
the laughs come from that collision (§1.0): **the cute-vs-brutal switch**,
**sight gags and signs**, **game-y jokes** about meters, ratings, menus,
tutorials and pixels, Grumbleguts' **gag book**, and the running gags. They
land as **abrupt, laugh-out-loud comedy**: hard cuts, absurd escalation,
and punchlines that arrive from nowhere and leave before you can recover.
It is **edgy but PG-13**: mean, quick, a little filthy around the edges,
never explicit.

On top of that backbone the rewrite adds the user's three references (§1):
a **faster, cruder comic register**, a **cute surface that cracks for a
beat**, and one new **outside parody target**: the
haunted-animatronic-restaurant, lore-tape, frame-decoding genre, and
specifically *The Walten Files*. We make fun of that genre. We never become
it, and it does not take over: it lives mostly in Bonejangles' thread.
**A character saying the quiet part out loud** is one tool in the kit
(§2.1), not the game's engine. Now and then, and always optionally,
something is **just slightly wrong**, and nobody explains it. The game
keeps **its own story** (Sunny lives), told in **the show's canon** (the
Mayor is a turkey, ratings "need salt", Bonejangles is a possessed
animatronic).

**The order of priorities for every line:** clear on first read, then funny,
then (sometimes) creepy. A line that is only creepy is a line that needs a
joke next to it, or a very good reason (§3).

---------------------------------------------------------------------------

## 1. Where the jokes come from

**The game's own comedy comes first (§1.0).** The user's three references
(the table below, §1.1-1.3) sharpen it; they do not replace it. If a
scene's only jokes are a Walten parody or a character blurting the truth,
it has lost the game's own voice: add a game-y joke, a sight gag or a
cute/brutal switch.

### 1.0 The backbone: the indie-game parody

The pitch is the comic premise: *Tamagotchi × Tomodachi Life × DOOM*
(`README.md`; `story_final.md` §0's tone target is "Shaun of the Dead,
Gravity Falls, Undertale and DOOM, sewn into a plush toy"). Each genre in
the mash-up takes itself completely seriously, and the joke is that they
have to share one cartridge.

| Tool | How it plays in text | Example (style sample) |
|---|---|---|
| **The mash-up** | One genre's problem handled by another genre's rules, straight-faced: an invasion as a life-sim chore, a possession as a care-meter alert, a funeral as a furniture rating | *"Neighbour POSSESSED! / Mood: spooky. Hunger: / souls. Try a gift!"* |
| **Cute vs. brutal** | The same thing twice: once through the CUTE filter, once as it is. The switch is the punchline (`GAG_BIBLE.md` §1 "the switch", §4.15). The game's own tag: "We don't judge. (We judge a little.)" Weapon names flip with it (PARTY PUMP / SLAUGHTER) | *"It's confetti. {.}It's / mostly confetti."* |
| **Sight gags and signs** | A sign or a short narrator line sets up a picture, and the picture is the joke (`GAG_BIBLE.md` §8.4). A sign is the cheapest laugh a 3 × 27 box can buy | *"(A sign: 'NOTHING INSIDE! / (we checked) -Mayor')"*; the crate that slides one pixel to the left, in shame |
| **Game-y jokes** | The game knows it is a game, and so does everyone who works in it: meters, ratings ("COSY: B+"), tutorials, quest markers, trophies, save points, unlock pop-ups, the HUD, the box itself. Absurd game logic, played straight | *"(The tutorial arrow points / at the demon. Then, / slowly, at you.)"* |
| **The indie-art joke** | The game's own look is material: plush critter-folk, chunky LCD pixels, a palette that rots to hell red, "premium, not cheap pixel art" said a little too proudly | *"(The demon is drawn in / four colours. Three of / them are panic.)"* |
| **The gag book** | Grumbleguts' appetite, belly and rating gags (`GAG_BIBLE.md` §7.4), inside R2 | *"Six. {.}Needs salt."* |
| **Running gags** | "beep.", "...it's fine.", "Hee. Hee.", Population: 500 (and the "1"), Mr. Huggins the Nth, DAYS SINCE LAST FIRE (`story_final.md` §5, `GAG_BIBLE.md` §6). Each comes back changed by one word | *"(The sign: 'Population / 501.' The 1 falls off. / {.}Tink.)"* |

* **Most jokes come from here.** A rough mix for a chapter: about two
  jokes in three from the tools above, the rest from the references, the
  quiet-part-out-loud line and the eerie seasoning. The Walten parody is
  concentrated in Bonejangles' thread and the restaurant's leftovers
  (§1.3), not sprinkled over every scene.
* **The text sets up, the picture lands.** A sight gag in text is a
  short, concrete parenthetical (C3) that the screen can show. It never
  explains itself.
* **Parody genres, not a real game.** No real game, studio or person is
  named in game text (J7). "DOOM", "Tamagotchi" and "Tomodachi Life" are
  pitch words for this guide; the game's own words (Doomogachi, Hellvile,
  HaHaland) are the script's.

### The three references: what we take and what we leave

The user named three references. Two of them set **how the game feels**.
One is **what the game makes fun of**. Do not mix up the roles.

| Reference | Its role here | Take | Leave |
|---|---|---|---|
| **Princess Robot Bubblegum** (the in-universe anime parody from the GTA series) | **The comic register.** How fast, how crude, how cynical, applied to the game's own jokes (§1.0) first | Rapid-fire, deadpan-stupid sincerity. Merch and fandom gags. Melodrama played completely straight until it collapses. Each joke lasts one box and then it is over. Now and then a character says the quiet part out loud (one tool, §2.1) | Mockery of a real nationality or culture. Cruelty with no joke shape. It parodies anime fandom; we parody the analog-horror ARG fandom |
| **Poochee & Pansy** (a cute 2000s web cartoon that was a front for a horror ARG) | **The shape.** A cute surface with something wrong underneath | Sugary kids'-show surfaces (jingles, mascots, pastel signs) that crack for one beat. A smile held one frame too long. A cheerful song that ends one note off | The ARG machinery (a recruitment front, hidden messages, a hunt). Real gore. The user also asked us not to call it "gross": the "gross-looking show" is the next row |
| **The Walten Files** (an analog-horror ARG: found VHS training tapes from an animatronic family-restaurant company, grief and trauma lore, a frame-decoding fandom) | **The parody target. The butt of the jokes.** | Its haunted restaurant. Its solemn VHS-tape lore. Its fans who freeze-frame everything. Its famously ugly art. Bonejangles carries most of it (§1.3) | Any real mechanic: no ciphers, no frame-decoding, no clue you must find, no lore you must assemble. Its real names, characters and text. Its real creator: parody the work, never the person (rule J7) |

> **Not verified.** The sandbox could not open the fandom wikis. The
> descriptions above come from the user's notes and secondary summaries.
> Write to the *register* described here, and never quote, reproduce or name
> anything from those works in game text.

### 1.1 The comic register (Princess Robot Bubblegum)

The register is **how** the game's jokes are told, not **what** they are
about. It makes the mash-up, the cute/brutal switch, the sight gags and
the game-y jokes of §1.0 faster and cruder; it does not swap them for
truth-telling. A character blurting the uncomfortable truth is one tool of
the register (§2.1), rationed like any other: about one a scene at most,
and never a scene's only joke.

* **Speed.** One box, one idea. A setup and its turn fit in one box
  (3 × 28 at 240×160), or across three boxes at most for a rule of three.
* **Played straight.** Every genre in the game believes in itself. The
  pet-care screen is sincere about hunger while the town burns; the
  shooter is sincere about gibs at a bake sale. Nobody winks.
* **Sincere stupidity.** The funniest characters believe every word. The
  Mayor really thinks a tablecloth makes him invisible. Fuzz really thinks
  a hug can fix a demon. The joke is that they mean it.
* **Cynicism in small doses.** Somebody always notices the scam: the
  merch stand, the gift shop, the mascot's contract, the "limited edition"
  plush with a number on it. Vex is the house cynic; anyone can be cynical
  for one box.
* **The hard cut.** The scene does not wind down. It stops. `[SCRATCH]`,
  `{r}`, `[MUTE]`, a smash to the next box. The button is the last thing
  said and nobody reacts to it.

### 1.2 The cute surface (Poochee & Pansy)

* Hillvile is a sugary life sim that has not been told it is also a DOOM
  game (and, now and then, a horror game). Signs are pastel, songs are
  jingles, everyone is "sweet pea".
* The wrongness is **one detail**, not a mood: a mascot's wave that is too
  slow, a jingle missing its last note, a birthday banner for a party that
  was forty years ago. Then the cartoon carries on.
* The surface cracks **for a beat, not for a scene**. In Ch1-2 every crack
  is either undercut by a joke or doubled by a character reacting to it
  (rule C9). From Ch3 on, a chosen scare may stand with `[NO GAG]`.

### 1.3 The parody target (The Walten Files) and its vehicle, Bonejangles

What we mock, and how. Each entry is a **gag shape**, with an example
written to the box. The examples are style samples, not final lines.

1. **The haunted family restaurant.** A defunct animatronic pizza-and-
   music chain, now a "historic site", where everything is cursed and
   also still has a birthday-party package. *"Our party package includes /
   pizza, cake, and one (1) / unexplained disappearance."*
2. **The solemn training tape.** Lore delivered by a laminated VHS with
   corporate clip art and a narrator who is far too calm. *"(A VHS tape. 'EMPLOYEE /
   ORIENTATION, TAPE 3 OF 9.' / Tapes 4 to 9 are also 3.)"*
3. **The lore guy.** A character who has a theory, a corkboard and red
   string, and is wrong in a way that is somehow worse than the truth.
   *"I paused it on frame 2,041. / There's a SECRET MESSAGE." /
   "That's a smudge." / "A SECRET smudge."*
4. **Grief as a gift-shop item.** The tragedy has merch. *"Commemorative plush of /
   the night it all went / wrong. {.}$12.99."*
5. **Portentous lore with no payoff.** A long, trembling build-up that
   lands on something stupid. *"Some say the band still / plays when the
   lights go / out. {..}That's the fridge."*
6. **The ugly art.** The chain's mascot costumes, menus and tape graphics
   are hideous, and the characters say so. *"(The menu art looks drawn / in a moving car by someone /
   who'd only heard of faces.)"* Mock
   the in-world chain's art, never a real artist.
7. **The fandom.** People who treat the lore like homework. *"I've
   watched the tape four / hundred times." / "Why?" / "...I don't
   remember anymore."*
8. **Animatronic logic.** Bonejangles runs on a servo, a curse and a
   cassette. He still has a "dance routine 4" he cannot turn off, a jingle
   he sings when someone says "birthday", and a maintenance schedule. *"Mostly
   bones. Partly / servo. {.}One hundred / percent under warranty."*
9. **The tape that explains nothing.** The one cryptic tape is played.
   Everyone leans in. It is a cooking segment. *"(Tape 7: 'How To Clean /
   The Ball Pit.' It does / not say how. It sobs.)"*
10. **"It's an ARG."** Someone insists the town is a puzzle. Nobody else
    cares. *"What if the REAL / exorcism was the clues we / found along the way?" /
    "It wasn't."*

**Bonejangles is the main vehicle** (W15 owns his thread). Canon (§6): a
semi-living animatronic skeleton, built as a tribute to an eccentric,
death-obsessed visual artist. The artist was possessed, the exorcism
failed, and the possession went into the animatronic. He still teaches
xylophone on his own ribs. Use him for the restaurant, the tapes, the
routines and the maintenance jokes. **His origin is a joke he tells and
everybody else finds horrifying, not a mystery the player solves.**

**The proposed in-world stand-in** (confirm with the user before any line
names it): the chain was **"Jangles' Jamboree Family Fun Feedery"** (or
"Boneyard Buffet & Bandstand"), its tapes are **"the Jamboree Tapes"** and
its fans are **"tapeheads"**. Never use the real work's names, its
characters' names or its locations.

**Hard rules for the parody** (no exceptions):

* **No ARG mechanic.** No cipher, no hidden-frame clue, no code to type,
  no puzzle chain across scenes, no item you only understand from lore,
  and no ending gated on lore. The player never has to decode anything
  (DECISIONS: the ARG is the butt of jokes, never a mechanic).
* **The parody always lands on a joke** within two boxes (C3 applies).
* **Parody the genre and the work, never a person**: no real names, no
  real creators, no real fans.

---------------------------------------------------------------------------

## 2. Comedy first: abrupt and laugh-out-loud

The game's default is a laugh. Sincere and scary beats are the exception,
are marked, and earn their place.

### 2.1 The shapes that work in a 3 × 28 box

The first four shapes are the game's own (§1.0); the rest are general
craft. "The quiet part, out loud" is last on purpose: one tool, rationed
(§1.1).

| Shape | How | Example (style sample) |
|---|---|---|
| **The switch** | Cute first, then the truth of it (or the reverse), on the turn word (§1.0) | Rivet: *"It shoots hearts. {.}Out / of the demon. Mostly / the demon's own."* |
| **The sign** | A sign or prop line does the joke; nobody comments | *"(A banner: 'GRAND OPENING! / NO REFUNDS! NO EXITS!')"* |
| **The game-y joke** | A game system taken literally by the people inside it | Nana: *"Your hunger meter's / blinking, sweet pea. {.}So / is the demon's."* |
| **The genre swap** | One genre's rules on another genre's problem | *"(You place the shotgun by / the window. COSY: C-. / It clashes with the gore.)"* |
| **The hard cut** | A scene ends on a line nobody answers. Cut, scratch, next | Mayor: *"Nothing bad has ever / happened here!"* `[SCRATCH]` *(A headstone in the / back falls over.)* Mayor: *"Recently!"* |
| **The escalation ladder** | Three steps, each bigger, the last absurd (rule of three, `story_final.md` §7.5) | *"Two balloons." / "Twenty balloons." / "Do you know a guy / with a helium tank AND / no questions?"* |
| **Wrong, with confidence** | A character states a terrible conclusion as fact | Fuzz: *"Demons are just friends / who haven't been hugged / hard enough. {.}Yet."* |
| **The setup and the turn** | One box, one pause, the turn word last (J4) | *"I baked you a map. {..}/ Into a pie."* |
| **The callback** | A refrain from earlier, changed by one word | *"We don't talk about the / other side. {.}We SING about / it. Badly."* |
| **The deflation** | A big moment answered by something tiny | `[MUTE 13]` The pet: *"beep."* |
| **The quiet part, out loud** | Someone says what everybody is avoiding, in a normal voice. One tool: about one a scene at most, never the scene's only joke (§1.1) | Kevin: *"Yes, three eyes. {.}No, / I won't tell you what the / third one has seen."* |

### 2.2 Density and placement (J1-J5)

* **J1. Density.** A punchline every 3-6 boxes (`story_final.md` §7.9).
  Every offer has at least one joke. Every turn-in ends on a button unless
  it is marked `[NO BUTTON]` or `[NO GAG]`.
* **J2. Abruptness.** Prefer the cut to the wind-down. No "anyway" lines,
  no characters agreeing that something was funny, no recap of the joke.
* **J3. Jokes come from character.** A joke must be something only that
  speaker would say (§7 voices). If it could be anyone's, sharpen it.
* **J4. The punch word goes last** in the box. Pause codes go before the
  turn, never after it.
* **J5. A HEART line is sincere.** Its joke, if any, comes after it as the
  button, never inside it.

### 2.3 Fixing the current script's habits (from the scout's grading)

| Habit (today) | Rule | Before | After (style sample) |
|---|---|---|---|
| Bare mood parentheticals (30% of lines, 82% in `ep23`) | C3 | *"(It pulses.)"* | *"(It pulses. Like it's / waiting for applause.)"* |
| Yoda inversions | C6 | *"Love he has. / Buckwheat he hasn't."* | *"He's got love. He hasn't / got buckwheat. {.}Love / doesn't stuff a bear."* |
| Exposition as a definition | C7 | *"A soul going home is an / exorcism run backwards."* | Bloom: *"It's an exorcism in / reverse. Same paperwork. / {.}Less screaming."* |
| Pauses as padding (0.65 a line in `q49a`) | C5 | *"{.}Thanks. {..}Really. {.}You're kind."* | *"Thanks. {.}You're kind. / Weirdly kind. Stop it."* |
| Lore nouns before their episode | C2 | *"Grape."* (Ch1) | *"Grape. {.}Why does EVERYTHING / smell like grape?"* (someone reacts) |
| Fragments in a row | C4 | *"Later." / "Napping." / "To what?"* | One fragment, as the turn of a setup |
| Jokes that need unseen context | C2, J3 | *"Hey, hey. Customers only / by the frame."* | Say the context, then the joke |

---------------------------------------------------------------------------

## 3. The eerie seasoning (sparse, optional, never required)

The user wants **some** cryptic moments "here and there". They are
seasoning, not a course.

* **What it is.** One odd detail: a sign, a background prop, a line said a
  beat late, a jingle missing a note, a name nobody should know. Never an
  explanation and never a clue.
* **Optional.** Nothing in the story depends on noticing it. A player who
  skips every one loses nothing but a shiver.
* **Not a chain.** No eerie detail is the setup of a later puzzle. If it
  pays off later, the payoff is a joke or a feeling, not an answer.
* **Rationed.** Ch1-2: at most one eerie beat per scene, always undercut
  or doubled (C9). Ch3-4 and the side missions: at most one per scene; it
  may stand with `[NO GAG]` if it is chosen. The game-wide target is about
  one eerie beat per 25 boxes. `story_stats.py`'s `omin%` (ominous
  keywords) is 8% today; aim for 3-5% per pack.
* **The comedy may undercut it, or leave it hanging.** Both are allowed.
  Leaving it hanging is the rarer, stronger choice.
* Examples (style samples): *"(A birthday banner: 'HAPPY / 9TH, DOTTIE!' The
  date is / forty years ago.)"* with no comment; or Steamy:
  *"That's Duckworth. That's / Quackers. That's... {..}/ I don't know that one."*

---------------------------------------------------------------------------

## 4. Clarity (C1-C9)

* **C1.** The first box of every offer says **who, what and why** in
  plain words. By the accept choice the player knows the task without
  opening the quest log. The quest-log hint repeats the goal plainly.
* **C2.** No unexplained lore noun before the episode that explains it
  (grape, the tally, Mingle, Fluffy, the stone, the Factory, the Huggins
  numbering). If a mystery must appear early, another character reacts to
  it ("Grape? AGAIN?"), so the player knows it is meant to be strange and
  not context they missed.
* **C3.** Narrator parentheticals: **at most 25%** of a script's lines.
  Each describes something visible or an action, as a full sentence with a
  subject. No bare mood fragments. A mood beat is followed within two
  boxes by a joke or by a character noticing it. The narrator is a comic
  voice, not a horror trailer (§7).
* **C4.** Fragments and one-word replies only as the turn of a setup.
  Never two fragments in a row. Never "..." as a whole line unless someone
  comments on the silence.
* **C5.** Pause codes `{.}` `{..}` `{...}`: at most one per box, only
  before a punchline or a turn. Target **at most 0.25 per line** per pack
  (side packs are 0.3-0.65 today).
* **C6.** Natural spoken grammar, contractions, no inversions.
* **C7.** Exposition comes as opinion, complaint or gag, never as a
  definition.
* **C8.** Options (27 characters at most) name the action ("Fetch
  buckwheat"). A jokey option is fine when its meaning is obvious ("Talk to
  a carrot").
* **C9.** Ch1-2: at most one ominous beat per scene, always undercut or
  doubled (`bible_horror.md`). From Ch3 on, chosen scares may stand, with
  `[NO GAG]`.

---------------------------------------------------------------------------

## 5. Register: edgy, PG-13

DECISIONS: **edgy but PG-13** (sharp and mean, mild swearing, innuendo
limited to Grumbleguts and a few adults). This **overrides** the
"PG-13-to-R, the odd f-bomb on BRUTAL" line in `GRUMBLEGUTS_TONE.md`: there
are no f-bombs, on any setting.

* **R1. Swearing.** Allowed, rarely and for rhythm: *damn, hell, ass,
  crap, sucks, screw it, pissed, bastard* (adults only, and *bastard* at
  most a handful of times in the game). Not allowed: the f-word, the
  s-word, slurs of any kind, sexual swear words. Hellvile's own oaths are
  better jokes anyway ("brimstone and BISCUITS").
* **R2. Innuendo.** Grumbleguts is the game's innuendo character
  (`GRUMBLEGUTS_TONE.md`: teasing first, implication only, cartoon-modest,
  consensual, every suggestive beat ends on a joke or a deflation). A few
  other adults may land **one** innuendo line a scene: Vex, Hattie Haha,
  Kevin, Dr. Bloom and the Hellvile bar crowd. Nobody else.
* **R3. Never with, near or about kids or teens.** Pip, Wisp, the
  daycare and imp kids, Miss Lulla's kids, the storybook kids: no
  innuendo, no adult swearing, no adult joke in their scenes or about
  them. `test_side_table.c` checks part of this; reviewers check the rest
  by reading.
* **R4. Mean is fine; cruel is not.** Characters can insult each other,
  the town and the player. Punch up or sideways. No jokes at real groups,
  real people or real tragedies (J7). No slurs. No jokes that need the
  player to agree with something hateful.
* **R5. Gore stays cartoon in text.** The CUTE/BRUTAL filter owns what is
  shown. Text describes it the way a cartoon would: "blood to the elbows"
  is the ceiling, said once.
* **R6. Death is a joke subject, grief is not a punchline.** Death,
  ghosts, graves and exorcisms are the game's material. A character's real
  grief gets a sincere line first (J5), and the joke goes after it.

---------------------------------------------------------------------------

## 6. Canon: the show's canon, the game's own story

DECISIONS: **adopt the show's canon** in the game. The game keeps **its own
story**: its chapters, missions, flags and endings. The show's seasons are
not the game's plot.

| Topic | Game text now says | Write from now on |
|---|---|---|
| **Mayor Giggs** | A cherub with busy little wings | A **Mad-Hatter turkey**: top hat, feather fan, bow tie, tea-party patter. "Gobble gobble, welcome!" His wings **flap** (never buzz). No halo, no cherub. His story stays: the showman who won't look at the sad thing, the lost pet cloud, "I had a cloud once." Art and data: package C1 |
| **Ratings** | Clinker rates poems "Six. Needs death." | **"Needs salt."** is the rating catchphrase (Grumbleguts, Clinker, GURGLE's "...needs salt."). S164's death-counting scoring is a mechanic: package C1 moves its verdicts to salt; the text never says "Needs death" again |
| **Bonejangles** | "Skeleton xylophone teacher. Plays his own ribs." | A **semi-living animatronic skeleton**, built as a tribute to an eccentric visual artist obsessed with death. The artist was possessed; the exorcism **failed**; the possession passed into the animatronic. Still the xylophone teacher, still plays his own ribs, still "Clack!". **Who performed the failed exorcism is open** (§11 Q3): W15 decides it with the user, consistently with Lady Smog (eleven years ago) being Father Fuzz's established failed rite and wound. Until then no line names the exorcist. He is the Walten-parody vehicle (§1.3) |
| **Sunny** | Grampy Bean calls the player "Sunny" | In the show, Sunny is the lead and dies in Season 4. **In the game Sunny lives.** No game text treats Sunny's death as fact. Light foreshadowing or an easter egg is allowed (at most a line or two, post-game, played as a joke) |
| **Everything else** | | The game's plot, its 27 episodes and 8 specials, the 109 side missions, flags, endings and the house. Show gags may be borrowed as callbacks where they fit (`SERIES_OVERVIEW.md` "Show nods in the game") |

Where the show and the old game text disagree on a fact not listed here,
**ask before writing it** (add it to the open questions in §11).

---------------------------------------------------------------------------

## 7. Voices

One line per character: how they sound, what only they would say. Jokes
come from these (J3).

| Character | Voice |
|---|---|
| **Mayor Giggs** (he; turkey) | Mad-Hatter showman: tour-guide patter, lists of three, exclamation marks, non sequiturs about tea and hats. "Hee. Hee." whenever he is not looking at something. Dead-stop freezes. "Gobble gobble" when flustered. Avoids the sad thing until Ch3 |
| **Nana Puddin** (she) | Slow and cosy, "sweet pea", food as love, hides things in bakes, kind fibs that slip ("...Mostly.") |
| **Pip** (they; a child) | Fast and official ("Patrol rules."), narrates their own bravery, euphemises crying ("My eyes are doing a rain."). Never the target of an adult joke (R3) |
| **Father Fuzz** (he) | Earnest youth-group energy, hug theology, a sincere battle cry ("THE POWER OF CUDDLES COMPELS YOU!"), doubt underneath. The jokes come from his literalness and the dummies. Never ironic, never mocked for his faith |
| **Rivet** (she) | Gruff, shows love through tools and threats. "Don't make it weird" (rationed: at most 3 more uses in the rewrite) |
| **Dr. Bloom** (she) | Clinical fact, then a dry button ("No pulse." / "He never has one."). One of the few adults allowed one innuendo line a scene |
| **Steamy** (she) | Lilting host who names every duck. "I don't know that one." |
| **Hattie Haha** (she) | Stand-up structure, subverted knock-knocks, waits for the laugh. Adult; may do one blue line a scene |
| **Vex** (she) | Influencer fox: brand-speak, "sweetie", signs off "- V". The house cynic: merch, sponsorships, engagement. One innuendo line a scene at most |
| **Vapula / Fluffy** | Lowercase, small, sulky weather puns; menace as a tantrum |
| **Grumbleguts** (he) | Slow purr, pet names (darlin', sugar, hon), vain about belly and butt, innuendo (R2). Every sincere moment is undercut by a smirk. Sign-off "Open late, darlin'." Ratings: "Six. {.}Needs salt." |
| **GURGLE** | Plain text, hungry and honest, sides with the player about half the time |
| **Wisp** (they; a kid ghost) | Lowercase, apologetic. Never the target of an adult joke (R3) |
| **Mother Nox** | Terse, soup, withholding |
| **Tallyworn, Clinker** | Tired labourers who rate things with numbers. Clinker writes chalk poems |
| **Kevin** | "Yes, three eyes." Says the quiet part out loud, cheerfully |
| **Bonejangles** (he; animatronic) | Musical, "Clack!", rhythm and bone puns, and now the animatronic layer: servo noises, a dance routine he can't stop, a cassette jingle, a warranty. Tells his horrifying origin like a funny anecdote. The Walten-parody voice (§1.3) |
| **The narrator** | Present tense, concrete, wry ("The crate slides one pixel to the left, in shame."). A comic voice, not a horror trailer. At most 25% of a script (C3) |
| **@E, the pet** | "beep." / "bep?" only |
| **@P, the player** | Silent. The player's three spoken lines (`story_final.md` §8.4) stay. Every other choice is an action |

---------------------------------------------------------------------------

## 8. The box

| Limit | Value | Checked by |
|---|---|---|
| A page | 3 lines × 28 characters at 240×160; `/` breaks a line, `//` a page | `textpack.py` |
| **With a portrait** | The box is **27 characters** wide at 240×160 (`ui_dialog.c`: 162 px / 6 px a glyph). A 28-character line wraps and pushes the page's last line onto an extra page. **Write to 27** | `story_stats.py`'s `box27` column (a count, report only: 89 lines in the game today; drive your pack's to 0). The lint still allows 28 |
| A line | 199 visible characters | `textpack.py` |
| A choice option | 27 characters | `textpack.py` |
| A choice question (`choice`, `player_line`) | **No `/` or `//`**: `ui_choice` takes the question as plain text, so a `/` prints as a slash. Write one sentence run and let the box word-wrap it (at most 3 lines at 27 columns). W2 measured it; W1 fixed Ch1's (`ep12`, `ep14`, `ep16`); 4 questions outside Ch1-2 still have one (`ep34`, `int3`, `ep44`, `ep46`) | reading the shots |
| An idle line (`string IDLE_*`) | **No `/` or `//`** either: `ch12_idle` hands it to `story_say` -> `ui_say`, which takes plain text, so a `/` prints as a slash. One run, at most 3 lines at 27 columns (W1 measured it on Grampy's line; Ch1's are fixed, the 14 in `ch2/common.dsc` still have one) | reading the shots |
| A quest-log hint | 95 characters (the HUD objective) | `textpack.py` |
| A trophy name | 27 characters | `textpack.py` |
| Characters | ASCII plus the icon glyphs 0x80-0x9F | `textpack.py` |
| Line codes | Only `{.} {..} {...} {d} {!} {big}..{/} {w} {r} {punch} {p n} {cut n}` | `textpack.py` |
| `{d}` | At most one a script; fewer than 80 in the game | `textpack.py`, `test_story_lint.c` |
| "Ten years I" | Only in `ep41`, `s139`, `s179`, `s187` (4 in the game) | `textpack.py`, `test_story_lint.c` |
| Pack bytes (English) | episode 12 KB, common 6 KB, side 8 KB; **keep 5% headroom** (translations run longer) | `textpack.py`, `--sizes` |
| PS1 slots | common pack ≤ 2,048 B, side pack ≤ 8,192 B, or it falls back to the episode slot and evicts the running episode's pack | `--check-es`: the **English** out of its slot fails; the Spanish only warns. `--sizes` flags a pack within 10% of its slot (today `ch1` and `quests`) |
| Herald ticker (`story_grumble.c`) | 63 characters | C |

---------------------------------------------------------------------------

## 9. Mechanical rules for every rewrite package

* **M1. Words are free; structure is deliberate.** Reword any quoted
  text, and add or cut whole `say` lines, freely. Restructuring a scene
  (moving, adding or cutting ops, waits, beats, choices, scripts) is
  **allowed** where it makes a joke land (DECISIONS), as long as:
  * the **save lock** still passes. It never changes in a rewrite: pack
    ids, the ids of `later` targets, mission rows, possessor rows, and
    the names the C code uses (`SCRIPT_LOCKS.md` §1). Add new scripts at
    the **end** of a pack; never delete or rename a `later` target;
  * the **flow lock** is regenerated (`python3 core/tools/story_opslock.py
    --write`) in the same commit, and the commit message names each
    restructured scene and says why (one line each);
  * every test still passes. `silence_budget` and `story_style_rations`
    may be retuned only with the reason written in the commit, never just
    to make a rewrite pass.
* **M2. Tags stay with their line**: HEART, PUNCH, DEADPAN, GURGLE, the
  `[EXPR]` and expression brackets.
* **M3. The limits in §8.**
* **M4. Do NOT translate.** DECISIONS: Spanish (and every new language)
  is redone **after** the rewrite. Leave `core/lang/story/*.tsv` untouched.
  A rewritten line's old Spanish row goes stale and the game shows the new
  English in Spanish mode until the L8 batches retranslate it (the default
  `make -C core story-check` reports stale and missing rows and does not
  fail on them).
* **M5. Update every runner pin you break** (`SCRIPT_LOCKS.md` §5) and
  every C-pinned word (`SCRIPT_LOCKS.md` §4) in the same commit.
* **M6. Regenerate `core/gen` with `make -C core gen`.** On a merge
  conflict in `core/gen`, take either side and regenerate. Never
  hand-merge. A conflict in `flow.lock` is different: never re-run
  `--write` blind after a merge; confirm that `--check` names only your
  own restructures first (`SCRIPT_LOCKS.md` §2).
* **M7. Measure before and after** with `python3 core/tools/story_stats.py
  PACK...` and put the table in the commit message.
* **M8. Review from the reading script**: `python3 core/tools/story_read.py
  PACK` prints the pack as a reader meets it. Read it aloud once.

---------------------------------------------------------------------------

## 10. The checklist before a commit

1. Every offer's first box says who, what and why (C1).
2. A joke every 3-6 boxes; every turn-in ends on a button or a marker (J1).
   Most of them are the game's own (§1.0: the mash-up, cute vs. brutal,
   sight gags, game-y jokes, the gag book, running gags); the quiet part
   out loud is one tool, about once a scene at most (§1.1).
3. Parentheticals ≤ 25%, pauses ≤ 0.25 a line, no fragment pairs (C3-C5).
4. At most one eerie beat a scene, optional, not a clue (§3).
5. Swearing and innuendo inside R1-R3; nothing adult near a kid.
6. Turkey Mayor, "Needs salt", animatronic Bonejangles, Sunny lives (§6).
7. The Walten parody lands on a joke and asks the player to decode nothing (§1.3).
8. Lines written to 27 characters where a portrait shows (§8).
9. `make -C core story-check` passes (save lock untouched; flow lock
   regenerated with a reason if the structure changed).
10. `make -C core gen && make -C core check-gen`, `make -C core -j2 test`.
11. `story_stats.py` before/after table in the commit message.

---------------------------------------------------------------------------

## 11. Open questions for the user (answer before the guide is locked)

1. **The in-world names** for the Walten-parody chain, its tapes and its
   fans (§1.3 proposes "Jangles' Jamboree Family Fun Feedery", "the
   Jamboree Tapes", "tapeheads"). Keep, change or pick another?
2. **How hard to hit the ugly-art jokes.** Proposed: a running gag in
   Bonejangles' thread and the restaurant's leftovers (menus, costumes,
   tape graphics), about one joke a scene there and none elsewhere.
3. **Bonejangles' failed exorcist.** Unnamed, a new character, or an
   existing one? Father Fuzz is possible only if it fits Lady Smog
   staying his established failed rite and wound (§6; W15 owns it).
4. **Sunny's show death in the game.** Proposed: one post-game easter
   egg at most, played as a joke (a "Season 4" poster nobody wants to
   talk about). Or none?
5. **The swear list** in R1 (*bastard* included or not).
6. **Innuendo cast** in R2 (Vex, Hattie, Kevin, Dr. Bloom and the
   Hellvile bar crowd, one line a scene). Add or remove anyone?
