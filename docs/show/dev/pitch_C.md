# Pitch C: "MINGLE NIGHT"

> **Superseded development draft: the winning pitch, before the judges' grafts.** Do not board, record or pitch from this file. The pilot as it now stands is `../PILOT_BEATSHEET.md` (timing) and `../PILOT_TREATMENT.md` (story); the graft ledger in `../PRODUCTION_ROADMAP.md` §1.3 lists every change. Its rating, audience, comps and standards section (TV-PG, a 10-16 family audience, kids' and family comps, sprinkle shells and confetti-only gore, "show-safe" Grumbleguts rules) are superseded: the show is adult animation, TV-MA, bound by four hard limits, with HaHaland's filter and the truth (`../PILOT_ROADMAP.md` §1, `../PRODUCTION_ROADMAP.md` §2). Known places where this draft differs from the rules now in force: Clinker's "...She?" (cut, to protect "The Bottle"), a crowned silhouette trailing a string (no crowned shape in the pilot has a string), a red-eyes insert in the cold open (no OMINOUS cue before the fog), and a different running order (the beat sheet's 22:00 split is the one in force). See `README.md` in this folder.

## DOOMOGACHI LIFE in HELLvile Park!, Episode 101: "Mingle Night"

**Angle.** This is an event-episode pilot that fits the whole show into one night. HaHaland stopped holding the Midsummer Mingle eleven years ago. On the anniversary it throws a half-a-park substitute, the Midsummer Giggle, and this year the festival goes wrong, from sunset to midnight, in front of everybody. The pilot is self-contained. A neighbour is possessed, Father Fuzz runs an exorcism as a set piece, a cute/brutal fight shows off the DOOM side, and the story turns on a heartfelt beat. By the end it has planted the plumbing secret, Fluffy/Vapula and the Wall without paying any of them off.

Sources: canon is `docs/design/story/story_final.md` (which wins every conflict) and the shipped Chapter 1 script, `core/data/story/ch1/*.dsc`. Quoted lines are canon unless the "Canon decisions" section at the end flags them as new.

---

## 1. Logline

At the Midsummer Giggle, the half-a-festival HaHaland throws on the anniversary of the night it walled off the other side, a silent newcomer and a just-hatched pet that only says "beep." must save the town's skeleton bandleader from a rhyming demon. The cure turns out to be a name nobody on this side is allowed to say.

---

## 2. Format

* **Runtime.** A single 22:00 episode: 21:50 of programme plus a 10 s post-credits button.
* **Why one 22 and not two 11s.** An event night needs unbroken escalation, from party to wrong to war to the name, and it needs a running clock the audience can feel. The episode is still built to split. If a buyer wants 2x11, the Act Two out is a full cliffhanger, and the cut is "Part 1: The Giggle" (cold open to the end of Act Two, with a 1:20 trim from the elastic beats) and "Part 2: Say It" (a 30 s recap cold open, a short main title, then Act Three to the end).
* **Shape.** A cold open, the main title, **four acts** with an eyecatch pair at each of three breaks, a tag, end credits and a post-credits button. Four acts suit horror-comedy escalation, because each act out is a step down the ladder.
* **The clock.** The night runs in story time from 18:00 to midnight, and the structure says so in two ways:
  * every act card is the Mayor's festival clipboard;
  * every eyecatch shows the town clock.
* **Frame rate.** 29.97 fps, so the comic-timing ladder (2/3/5/8/13/21/26 frames) carries over from the game unchanged.

### Rundown

| TC in | Segment | Length | Ends on |
|---|---|---|---|
| 00:00 | **Cold open:** "The Last Mingle" (crayon, then pixel) | 1:50 | The egg cracks on the first downbeat of the theme |
| 01:50 | **Main title:** "Doomtastic Times" (main title cut) | 0:50 | Pilot variant: the title card GIGGLE NIGHT, with MINGLE showing through the paint |
| 02:40 | **Act One:** "18:00 Welcome!" | 5:00 | Bonejangles' ribcage lights up purple |
| 07:40 | Eyecatch pair 1 (OUT, break, IN) | 0:10 | |
| 07:50 | **Act Two:** "21:00 (we don't do that)" | 4:30 | Rivet's prize case: "TOP PRIZE. Please don't. —R" |
| 12:20 | Eyecatch pair 2 | 0:10 | |
| 12:30 | **Act Three:** "22:00 ~~Bedtime!~~ weather" | 4:30 | The name is right there, and nobody can say it |
| 17:00 | Eyecatch pair 3 | 0:10 | |
| 17:10 | **Act Four:** "23:00 ________" | 3:30 | An answering firework, and the hum goes one note further. `[NO BUTTON]` |
| 20:40 | **Tag:** "Morning" | 0:40 | "...He's teething." |
| 21:20 | End credits (music-box arrangement of the theme's hook) | 0:30 | The title card has peeled to read MINGLE NIGHT |
| 21:50 | Post-credits: the gate, GURGLE, then the Dippinn Productions card | 0:10 | "gurgle." |

**Elastic beats** for 21:00 or 23:00 territorial cuts, and for the 2x11 trim:
* the Giggle crawl in Act One (±30 s);
* the cute-mode run in Act Three (±20 s);
* the Bake-Off bite test (±10 s).

### Act cards: the Mayor's clipboard

Every act opens on the Mayor's festival schedule, drawn in the CREAM window skin, with his handwriting popping in line by line:

> 18:00 Welcome! · 19:00 Bake-Off! · 20:00 Bandstand! · 21:00 (we don't do that) · 22:00 Bedtime!

* By Act Three, "Bedtime!" has been crossed out and "weather" written over it.
* Act Four's card is the next line down, and it is blank. The Mayor never planned past bedtime.
* The card is the act's only title, and its sound on frame 0 is the Hee-Hee bell.

### Main title: "Doomtastic Times" (the 0:50 main title cut, placed at 01:50)

**Placement.** The main title comes straight off the cold open. The egg's crack is both its frame 0 and the song's first downbeat. Once the master in `docs/show/theme/` is locked, the bus drive at the top of Act One is the elastic shot that absorbs up to ±5 s.

**Board.** The lyric and bar grid are not final, so the opening is boarded by song section and conformed to the master later:

1. **Intro.** White flash off the crack. Crane up over pastel HaHaland (built from the trailer's s02), with the logo letters dropping on the beat.
2. **Verse.** The cast pops in on the beat (4-6 frames, with overshoot), each with a CUTE-skin name plate, in this order:
   * Sunny with the suitcase, then the pet;
   * Mayor Giggs, Nana, Pip with Mr. Flops;
   * Father Fuzz, Dr. Bloom, Steamy, Hattie, Grampy;
   * Bonejangles at his ribs;
   * Rivet, seen only as a welding mask and a shower of sparks.

   Mascotty never pops in. She is simply in every verse shot, perfectly still.
3. **Pre-chorus.** The pan to the Wall (built from s03). The song's own record scratch stops the camera on the gate: seven padlocks, "KEEP OUT. FOREVER."
4. **Chorus.** On the lift, hellify rots the same shots pixel by pixel:
   * the corridor from s10 flips from cute to brutal on the downbeat;
   * three candles go out in rhythm;
   * behind the gate, a round silhouette bounces once.
5. **End pose.** The frame splits at the Wall and the pastel and Hellvile halves meet on the logo. The "nothing to see here! ♥" sticker slaps over the red drip. Then 13 frames of silence, "beep.", and the Chapter 1 card's canon button: the last sparkle is purple, and it sinks.

**Pilot variant.** For this episode only, the end pose dissolves to the Giggle banner and the episode title card reads **GIGGLE NIGHT**, with MINGLE showing through the wet paint. At the head of the end credits the same card has peeled to **MINGLE NIGHT**.

**The TV size (1:30)** is used inside the episode. Its chorus, in the DOOM-side arrangement, becomes the insert song for Act Three's brutal run, the way an anime opening comes back at the climax. If the master has no DOOM-side stem, the trailer's E-minor saw-riff (the RIFF) carries the scene and the theme's hook is quoted on the last blast.

### Eyecatches: "the festival clock"

**The rule** comes from comic_timing.md rule 5, the whiplash insert.
* The **OUT** card runs 5 s. It is pure HaHaland, scored with a 2-bar cut of the theme's hook in C major, and its frame-0 sound is the Hee-Hee bell.
* The **IN** card also runs 5 s. Frame 0 is a 4-frame hellify flash. Then comes the same card with **exactly one persistent change**, scored with the hook turned minor (the RIFF) and resolving on the ITS_FINE sting (a cheery C over a wrong F♯ bass), rearranged into the theme's key.
* Every card has the town clock in a corner, so the breaks tell the time of the night. The IN card's one change works as a "previously on" for viewers coming back from the break.

| Break | OUT card | IN card: the one change |
|---|---|---|
| 1 (07:40) | A paper lantern printed with the logo, the Giggle banner behind it, the clock at 21:00 | The lantern glows purple from the inside, like a ribcage |
| 2 (12:20) | Two eggshell halves hung on the lantern string, with a tiny "beep." bubble | A tiny Puff Demon sits in the second shell, beeping lower |
| 3 (17:00) | Three candles on the bandstand rail, two of them out, the clock at 23:00 | The last flame leans toward the Wall |

**Uses beyond the broadcast.** Each card is a seamless 5 s loop, ready to post as a short. In a streaming version the cards stay in as chapter markers and the main title carries a skip-intro marker.

---

## 3. Target audience and rating

* **Core audience:** 10-16, built for family co-viewing. The strong secondary audiences are adult animation fans and retro-game players.
* **Comparable shows:** Gravity Falls (summer-event episodes over a serial mystery), The Owl House, Amphibia, Over the Garden Wall and Hilda. The tone, per canon, is "Shaun of the Dead, Gravity Falls, Undertale and DOOM, sewn into a plush toy."
* **Series rating: TV-PG (D, V) is the ceiling and is never exceeded.**
* **This episode: TV-PG-V.** It contains the series' signature fantasy violence and no suggestive dialogue. Grumbleguts is present only as a belly gurgle behind a locked gate.

**How the binding boundaries apply in this pilot:**

| Area | In the pilot |
|---|---|
| **Gore** | Confetti, hearts, plush stuffing and grape goo only. The HUD gag (§10, gag 4) turns the rule into a joke on screen. The game's BRUTAL gore setting and Monster Mode are out of the show. In the show, "brutal" names a register (camera, music, palette, HUD), never a gore level. |
| **The weapon** | Rivet's Pump Shotgun is a welded junk contraption (a bath-house bellows, a drainpipe, a bike bell) that fires sprinkle shells, and only at Puff Demons. It is never pointed at a citizen and has no realistic gun silhouette. Its cocking sound is a squeak and a CHK. An alternate "Pump Sprinkler" design is ready if Standards asks. |
| **Possession** | Shown through stillness, red eye sockets, a ribcage lit like a lantern and a doubled voice. There is no body horror and no harm to the host. The one scare left without its joke is a 4-frame insert. |
| **Kids** | Pip and Miss Lulla's daycare kids face mild peril only. The fog never touches them. They are never in romantic or suggestive material, or in any scene that carries it. Pip's big moment is courage. |
| **Fireworks** | An adult lights the one rocket. The kid stands three steps back (then four). |
| **Religion** | Father Fuzz's rite is hugs, a hymn and holy water, shown with affection and never mocked. There is no pentagram imagery in this episode. |
| **Vaping** | Only the Cloud Nine Vapor "COMING SOON" sign, flickering purple in the background. The fog smells of grape candy (canon), but nobody inhales anything. |
| **Grumbleguts** | Heard only, as GURGLE (it), with no innuendo. His show-safe rules and the concept-sheet cuts apply from his debut onward. |
| **Flashes** | Every insert plays for 4 frames, never 1. White flashes last at most 3 frames, at reduced luminance. Everything is checked with a Harding/PSE pass. |

---

## 4. The pilot's story

**A-plot: Sunny's first night.**

**Act One.** At six o'clock a pink bus brakes with a record scratch and throws Sunny, a silent bunnykin with one suitcase, onto the road. The town sign updates to 501, and the "1" falls off. Nobody meets the bus, because the whole town is at the Midsummer Giggle in Hellvile Park. Sunny wanders in:
* lanterns everywhere;
* a Hook-a-Duck where one duck nobody knows faces the Wall;
* a craft tent where every child has drawn the same crowned purple cloud;
* a bandstand whose south half is boarded up against the Wall.

Mayor Giggs welcomes Sunny from the stage. When the camera drifts toward the boards he steps in front of it, then delivers the town's one rule as a joke: "We don't talk about the other side." The fountain giggles, late. At Nana Puddin's Bake-Off stall Sunny bites through raisin, raisin and a button before finding a key with tiny horns. Nana holds on to an egg a beat too long before handing it over: "...Mostly."

At nine, the hour the fireworks used to go up, the whole festival looks at an empty sky. Then fog pours over the boards, red eyes open in it, and the lanterns go out one by one. Bonejangles, the skeleton xylophone teacher playing the bandstand, keeps playing. His tune bends from major into minor, and his ribcage lights up purple.

**Act Two.** Bonejangles is possessed. He speaks in couplets, in "we", with a second, flat voice running eight frames behind his own, and he rates everything: Nana's pie gets "Six. Needs death." Puff Demons spill from his ribs into the Fun Fair. In the middle of the stampede, the egg in Sunny's arms hatches on the downbeat of his minor riff. "beep." The demon gives it a ten.

Father Fuzz arrives with three candles, one for each try. His hymn loses a rhythm duel with the ribs, and the first candle goes out. The clues:
* a chalk poem the bones keep writing and can't finish, "Down by the Wall where the pumps go clink...";
* the rating, which Nana recognises: a girl at the last Mingle gave her pie a six.

The girl's name is in Nana's recipe tin, in the bake tent, across a Fun Fair full of demons. By the Whack-an-Imp booth stands Rivet's welded prize case, holding a Bubble Wand and a Pump Shotgun.

**Act Three.** Sunny and the newborn pet fight through the fair in cute mode, popping demons into hearts. Then the demons merge into one Big Puff, and the bubbles bounce off it. Sunny pumps the shotgun and the show changes register: first person, a DOOM-style HUD with the pet's face in it, metal, a hellified palette. The gore stays confetti. Back at the bandstand, on the anniversary of the hug that failed eleven years ago, Fuzz tries a hug again and loses the second candle.

Sunny gets back with the tin. Inside is the sign-up sheet from the last Mingle, with names from both sides in one column, among them "Clinker: a poem (will rate yours)." On the last try the rhythm holds, with the pet beeping in time, and the holy water lands. Then Fuzz's mouth won't make the name. "Nobody on this side says those names," the Mayor says quietly, and every adult freezes.

**Act Four.** Pip, crying, reads it out: "CLINKER!" The breath leaves as a small, tired puff and stops at the keyhole of the sealed gate. Bonejangles finishes its poem for it: "...and we dream in pink." The puff goes through. From the other side, very small: "...Seven."

**B-plot: the Mayor's perfect Giggle.** In the cold open he paints GIGGLE over MINGLE on the banner, and through the night he runs the festival from a clipboard: "21:00 (we don't do that)." When things go wrong he hides under a cake table, being calm, and when the name is in reach he is the one who stops it. His arc is one firework. Pip has never seen one. The Mayor takes the last candle ("One. A small one. For the patrol.") and lights a rocket from an old crate stencilled "MINGLE — DO NOT OPEN". It opens over the Wall as a pink heart. Beyond the Wall, a vast crowned shape turns toward it, trailing a string. Then a small purple firework goes up from the south in answer. The Mayor hums the Mingle tune and gets one note further than he did at dawn.

**C-plot: Nana's two of everything.** Nana enters two pies, and the second has a blank name card ("Habit, sweet pea."). To save Bonejangles she gives up the tin that holds her Mingle keepsakes. Her thumb rests for a moment on "Charlotte: cookies (sorry)." The next morning she brings two muffins and her second pie dish, which has come back from somewhere scrubbed clean, with a burnt cookie on it. Then she sees the pet's new horn nubs. "...He's teething."

**What the pilot is about.** HaHaland's cure for everything is not saying it. In one night, the thing it won't say comes over the Wall, gets into a friend, and is sent home by a crying child reading a name off a list. The town survives by doing the one thing it has forbidden, and it doesn't notice. The audience does.

---

## 5. The cold open in detail: "The Last Mingle" (1:50)

* **Look.** The first half is children's-book crayon on paper grain (`PAL_CRAYON`, with a 2-frame line boil). It match-cuts into the show's pixel art.
* **Words.** No narrator. Captions are in a child's hand, so the first spoken words of the series are the Mayor's "...Hee. Hee."
* **Music.** MINGLE (F major, the music box).

| TC | Frames | Picture | Sound | Words |
|---|---|---|---|---|
| 00:00 | 0-90 | Crayon. A calendar page; a child's hand circles MIDSUMMER and scribbles fireworks round it. | A crayon scratch on frame 0. At frame 60 the music box starts the Mingle tune. | — |
| 00:03 | 90-450 | Circle iris in on a bandstand at the centre of **one** park. Two tables: a cupcake tower on the north side, a soup cauldron on the south. Kids run across the bridge both ways, staggered 0-3 frames. A small cherub, seen from behind, holds a cloud on a string. A small imp sets down two bowls. | Music box, with soft crowd babble under it. | "Every Midsummer night, the whole park had one party." [R13] "It was called the Mingle." |
| 00:15 | 450-1020 | Crayon fireworks on the beat. The cloud looks up with love-heart eyes. R8 later the string slips; HOLD 13. The camera pans south with the cloud as the fireworks fade. | A single pluck on the slip. The music box **mutes for 29 frames** mid-phrase, on the note where the Mayor will later stop humming. | "It loved fireworks." / "The string slipped. Nobody noticed." / "Not everybody went home." |
| 00:34 | 1020-1440 | **SMASH.** The same bandstand with a Wall through it, and the crayon loses its pink. Two bowls on one side, one cupcake on the other. | A thud on frame 0, then a rumble shake. | A deadpan caption **appears** without typing on: "Nobody talks about why." |
| 00:48 | 1440-1800 | A crayon banner over the bandstand reads MINGLE. A pink paint roller crosses it from frame left and leaves GIGGLE. | The music box returns, thinner, as one voice. | Captions pop on the beat: "Now there's a different party." "Same night." "Half a park." Then the deadpan caption **appears**: "No fireworks." |
| 01:00 | 1800-2190 | **MATCH CUT** to pixel art on the same frame: the real banner at dawn on Midsummer. The Mayor is up a ladder with a roller, humming the same tune, and stops on the same note. R8. | The hum takes over from the music box at exactly its pitch. After his line, the fountain giggles R8 (267 ms) late. | MAYOR: "...Hee. Hee." |
| 01:13 | 2190-2730 | **Rule of three.** The wet N of MINGLE bleeds back through the pink. He rolls it over (15 frames); it bleeds again. He rolls again (16 frames); it bleeds bigger. Topper at R13: he ties a cupcake balloon over the N and pats it. | Three roller squeaks, each pitched lower, then a *tink* on the balloon. | MAYOR: "Perfect." |
| 01:31 | 2730-3000 | He glances south at the boarded half; FREEZE 13, and his wings stop. **INSERT 4:** fog beyond the Wall, and one pair of red eyes. Back to the same shot with **one change**: the cupcake balloon has turned to face the Wall. R8. He turns it back with a squeak and climbs down, briskly. | The insert owns the audio for its 4 frames (a low drone), then silence, then the balloon's squeak. | — |
| 01:40 | 3000-3300 | Across the plaza, in Nana's bakery window, an egg sits in a tea towel with a tag: "for whoever's next". A bus horn sounds far off. The egg wobbles once. Heart iris closes on it (5 frames), then holds. | The bus horn is the first two notes of the theme's hook. A tiny crack on the last frame **is the first downbeat of "Doomtastic Times."** | — |

---

## 6. Beat sheet

R-numbers are rungs of the comic-timing ladder (R8 = 8 frames). `{d}` marks a deadpan line, which appears without typing on and with no music.

**Act One, "18:00 Welcome!" (02:40-07:40)**

| TC | Beat | Length |
|---|---|---|
| 02:40 | Act card. The pink bus crosses the candy hills at a constant speed and stops with a record-scratch brake. Sunny is thrown off the step; the suitcase lands 8 frames later and bounces. | 0:20 |
| 03:00 | The sign: "Population: 500 lovely citizens." It updates to 501, and the "1" falls off (*tink*). Under the 500, someone has painted over a 1000. Nobody is here. A pink ticket flutters onto Sunny's head: "MIDSUMMER GIGGLE: ADMIT ONE (NEW)". | 0:20 |
| 03:20 | **The Giggle crawl** (one micro-gag per stall, 8-12 s each). Steamy's Hook-a-Duck: "That's Duckworth. That's Quackers. That's..." [SHOCK] "I don't know that one." Kevin at face painting: "Yes, three eyes." [R5] Only the third blinks. Grampy's tomatoes: "Sunny... Sonny... Bunny... Toaster." Hattie's tent: "Knock knock. Who's there? See, that's the trick. Nobody ever says." A second laugh comes from the Wall, R8 late. "...Yikes." Miss Lulla's craft tent: every drawing has a crowned purple cloud. "They all drew the same friend. I don't know who he is." Mascotty stands still in the bobbing crowd. In the background by the Wall, Cloud Nine Vapor's "COMING SOON" flickers purple. | 1:10 |
| 04:30 | The bandstand. A spotlight finds Sunny. MAYOR: "Welcome, welcome! Mayor Gigglesworth. Friends call me Giggs. Enemies don't exist!" Sunny's eyes go to the boarded half, the camera drifts after them, and the Mayor steps into the lens. "Hellvile Park has two sides. This one, and..." [FREEZE, HOLD 13] "...the weather side." `{d}`"We don't talk about the other side." [HOLD 13] The fountain giggles, R8 late. "Hee. Hee." Bonejangles plays a fanfare on his ribs: "Clack clack!" | 0:50 |
| 05:20 | 19:00, the Bake-Off. Nana's stall has two of everything, including two pies; the second has a blank card. MAYOR (judging): "Who's the second one for?" NANA: "Habit, sweet pea." "The key's in a muffin. Don't ask which. I forgot. I always forget." Raisin, raisin, "that's a button", then the key, and a 3-frame x-ray insert shows **tiny horns**. The egg in a tea towel: "It's been waiting for someone." She holds on for R13. "The last one hatched into a little demon! Ha! Kidding." [R8] "...Mostly." | 0:55 |
| 06:15 | Pip, in a JUNIOR FESTIVAL PATROL sash, with Mr. Flops under one arm: "When's the fireworks?" MAYOR [FREEZE 13]: "There's a lovely nine o'clock... nothing!" Clipboard insert: "21:00 (we don't do that)". PIP (to Sunny): "I saw them once. I was a baby. So it doesn't count." | 0:30 |
| 06:45 | 21:00. The whole Giggle looks up, rippled 0-3 frames. The sky is empty. MUTE, then R26 of silence, then one cricket. From the Wall: thump, thump, thump... thump. Mascotty, perfectly still, thumps her foot back: 3, then 1. Only Sunny sees. | 0:20 |
| 07:05 | Fog comes over the boards and the lanterns go out from the south, faster and faster. Two red eyes pop open, then more pairs at R8, R5 and R3. MUTE 40. `{w}`"come and PLAY with us" (the second "us" in a child's voice). The fog drifts toward Lulla's tent, and Fuzz, at his hug booth, freezes. **INSERT 4**, from his point of view: the kids' tent eleven years ago, a cloud-shaped lady with glowing eyes, a crowned silhouette trailing a string. Back with one change: his paws are shaking. The fog turns away from the tent, toward the music. Bonejangles plays on, and the fourth note of the hook bends from C to B♭. | 0:35 |

**Act Two, "21:00 (we don't do that)" (07:50-12:20)**

| TC | Beat | Length |
|---|---|---|
| 07:50 | Act card. Bonejangles stops bobbing; his eye sockets go red; his ribcage glows purple from inside like a paper lantern; the ribs keep playing by themselves. Two voices, the second R8 late: "We came on the fog, and we came for the cake. / We came for the party you only half make." | 0:25 |
| 08:15 | **The ratings** (a rule of three, 15-16 frames apart). Nana, by reflex: "...Pie, sweet pea?" "Six." [R8] `{d}`"Needs death." The Mayor shakes skull sprinkles onto a cupcake: "Four. Too much death." He holds up his speech, hopefully: "Two." | 0:35 |
| 08:50 | Panic, accelerating. The Mayor under the cake table: "Everybody stay calm! I'll be under here. Being calm!" Puff Demons spill from the ribs into the Fun Fair. The teacups spin backwards, and the cardboard ghosts in the Absolutely Not Scary House all turn to face the Wall. A gust of fog blows the bandstand boards loose. Underneath is a crate stencilled "MINGLE — DO NOT OPEN". PIP: "FIREWORKS!" EVERYONE: "NOT NOW, PIP." | 0:35 |
| 09:25 | **The hatch.** In Sunny's arms the egg wobbles on a 6-frame period, then 3, and hatches on the RIFF's downbeat: white flash, two glossy eyes. HOLD 21 and MUTE 29 while the chaos plays on silently behind. "beep." R8. `{d}`"...it's fine." The nearest Puff Demons flinch. The possessed Bonejangles turns. [R13] "...Ten." | 0:35 |
| 10:00 | Fuzz and Dr. Bloom push through the crowd. BLOOM (taking his wrist): "No pulse." [R8] "He never has one." FUZZ sets three candles on the rail: "Three parts. The rhythm, the water, the name. Three tries." "A demon is a frightened thing in a loud coat. You take the coat off." | 0:30 |
| 10:30 | **Try 1.** "THE POWER OF CUDDLES COMPELS YOU!" "...One." The prayer-rhythm duel: Fuzz claps his hymn, and the ribs answer with the same notes in minor, dragging him R8 late until his own hymn turns minor. Kazoo notes step down, the first candle goes out: **FAIL x1.** | 0:45 |
| 11:15 | **The clue.** Chalk dust falls from his finger bones as he writes on the boards: "Down by the Wall where the pumps go clink, / we work all night, and we—" [STILL 13] "...Three. Needs an ending." PIP: "It keeps saying clink." NANA: "I know that rating. A girl at the last Mingle gave my pie a six." [R8] "Needs death." "Her name's in my recipe tin." "Where's the tin?" Everyone turns: it's in the bake tent, across a Fun Fair full of red eyes. | 0:45 |
| 12:00 | The Whack-an-Imp booth. Rivet's welded prize case holds a Bubble Wand and a Pump Shotgun on velvet, with a sign: "TOP PRIZE. Please don't. —R". Sunny puts a paw on the glass. "beep." The glass breaks on frame 0 of the OUT card. | 0:20 |

**Act Three, "22:00 ~~Bedtime!~~ weather" (12:30-17:00)**

| TC | Beat | Length |
|---|---|---|
| 12:30 | Act card. **Cute mode**, side view: the Bubble Wand traps Puff Demons, which float up and pop into hearts and stuffing, scored with the hook's cute arrangement. Jokes come only between waves. Steamy hooks a bubbled demon by reflex: "...I don't know that one either." | 1:00 |
| 13:30 | The demons stack into one **Big Puff**, perched on the bake tent. Bubbles bounce off it. Sunny looks at the shotgun. The pet looks at Sunny. Sunny looks at the camera. "beep." *Squeak-CHK.* `[SCRATCH]` | 0:15 |
| 13:45 | **Brutal register.** First person, with the HUD and the insert song, through the Absolutely Not Scary House ("NOTHING INSIDE! (we checked) —Mayor"): cardboard ghosts, and real demons among them. The HUD reads "GORE: BRUTAL" for 8 frames, then the Board of Decency sticker slaps over it. | 0:45 |
| 14:30 | **Whiplash cut** to the bandstand. The insert song drops to silence; the sequencer keeps running underneath. Against Bloom's "Father, don't", Fuzz tries a hug: "Nothing is unhuggable." The ribs play a terrible minor chord. Kazoo, the second candle out: **FAIL x2.** He sits on the steps. "Eleven years ago tonight, I hugged someone who needed it." [R13] "It didn't work then either." HOLD 4 s, no gag. | 0:40 |
| 15:10 | Back in first person, the song re-enters on the grid. The tin sits under the Big Puff. On the HUD, the pet's face grows a horn nub and glares. A "beep." makes the Big Puff flinch; the last shell; POP. The frame is covered by the sticker; it peels back to show hearts raining down and one tuft of stuffing landing on the tin. A REWIND sound takes the show back to cute, with one change: the tent sign now reads "BAKE TEN". | 0:30 |
| 15:40 | The tin opens. Under Nana's Mingle keepsakes is a sheet: "MIDSUMMER MINGLE: WHO'S BRINGING WHAT?" Names from both sides share one column: "Nox: bone soup (2 bowls)"; "Giggs: cupcakes (+1)", which the Mayor folds over, fast; "Charlotte: cookies (sorry)", where Nana's thumb rests for a moment; "Fuzz: hugs"; "Bonejangles: music"; and "Clinker: a poem (will rate yours)". | 0:35 |
| 16:15 | **Try 3.** "All three. From the top." The rhythm: the pet beeps on every beat, the crowd claps in, and the hymn holds in major. The water: Sunny pours a cupped paw of holy water, gently. The name: Fuzz lifts the sheet. "Cl—" [FREEZE]. MAYOR (quietly, from the steps): "Father. `{..}`Nobody on this side says those names." Every adult freezes. The last flame shrinks to a blue bead. POSSESSED BONEJANGLES (both voices): `{d}`"We'll wait." | 0:45 |

**Act Four, "23:00 ________" (17:10-20:40)**

| TC | Beat | Length |
|---|---|---|
| 17:10 | Act card: the blank line. The freeze holds. NANA: "Cl..." Her eyes go to the Mayor. The Mayor looks at the sky. Sunny opens their mouth and nothing comes. The pet looks at Sunny, and Sunny closes it. | 0:25 |
| 17:35 | Pip, crying, holds Mr. Flops in a headlock. "My eyes are just doing a rain." [R13] "...It's just a NAME." MUTE ALL, then Pip's voice alone: "CLINKER! Her name is CLINKER! It says so RIGHT HERE!" The TRUE NAME motif, the CHOIR, and the purple drains from the ribs over 21 frames. | 0:30 |
| 18:05 | The breath leaves as a small, tired puff trailing chalk dust. It drifts to the sealed gate and stops at the keyhole. BONEJANGLES: "...Clack?" [R13] "Someone was in there with me. She kept trying to finish a poem." He plays the hook on his ribs, in major, and finishes it: "Down by the Wall where the pumps go clink, / we work all night, and we dream in pink." The puff waits R13, then goes through. R21. From the other side, small, and for the first time **on time**: "...Seven." | 0:45 |
| 18:50 | FUZZ: "...She?" Nobody answers. He looks at his paws. "It was the name. `{..}`Not the hug." Sunny hugs him anyway. No line. | 0:20 |
| 19:10 | MAYOR (brightly, to everyone): "Right! Everybody home! Lovely weather tonight! Lovely, lovely weather!" A relieved ripple through the crowd. Nana looks at him for exactly R8. | 0:15 |
| 19:25 | Pip sits on the MINGLE crate. "I didn't cry." [R8] "I did a little." "...Not even one firework?" The Mayor looks at the crate, then the Wall, then the last candle in Fuzz's paw. HOLD 21. "One. `{..}`A small one. `{..}`For the patrol." "Patrol: three steps back." Pip takes three steps, then one more for Mr. Flops. | 0:35 |
| 20:00 | He lights it. MUTE. The rocket climbs and opens over the Wall as a pink heart. PIP: "...Oh." Every face is lit: Nana, Fuzz, Bonejangles, Sunny, the pet, and Mascotty at the back. At the edges of the Mayor's shot the pixels bleed into crayon for R8: for a moment he is nine. **INSERT 4:** beyond the Wall, a vast crowned shape turns its face up toward the light, with a string trailing below it. Back with **one change**: the fog over the Wall has thinned. R26. From the south, a small purple firework goes up in answer. Nobody speaks. The Mayor hums the Mingle tune, gets **one note further** than he did at dawn, and stops. HOLD 4 s. Heart iris out. `[NO BUTTON]` | 0:40 |

---

## 7. Act breaks

* **Act One out (07:40): the lantern.** In the dark, with every lantern out, one light is left in the park: Bonejangles' ribcage, glowing purple from the inside. The hook's fourth note has bent from C to B♭ (the same E-E-G cell, flipped from HaHaland to Hellvile). His skull turns to camera, slowly, while everything else in frame still bobs. `{d}`"...Clack." Cut on the click, which is the frame-0 sound of the OUT card.
  *The question:* what is inside him?
* **Act Two out (12:20): the prize case.** One candle out, two left, and the only name that might work is in a tin across a fair full of red eyes. Behind the glass are a Bubble Wand and a Pump Shotgun: "TOP PRIZE. Please don't. —R". Sunny's paw is on the glass. The pet, newborn and on Sunny's head, says "beep." The glass breaks on frame 0 of the eyecatch.
  *The question:* can a silent newcomer, with one day in town and a one-hour-old pet, cross that?
* **Act Three out (17:00): the freeze.** Rhythm and water have both landed, and the name is written right there on the sheet. Fuzz's mouth won't make it. "Father. Nobody on this side says those names." Every adult is frozen, and only the possessed bones keep their idle motion. The last flame shrinks to a blue bead. `{d}`"We'll wait."
  *The question:* who on this side will say it?

---

## 8. The button and the tag

* **The final story beat (20:00-20:40)** is the firework, the shape that turns to look, the answer from the south, and the hum one note further. It gets a 4-second hold and a heart iris, with **no button**. Per comic_timing.md rule 11 and story_final §7 (rule 7), the final sincere beat is never undercut. The tag begins after a fade to black and a time cut, as a separate scene.
* **Tag, "Morning" (0:40).**
  * The Pink Door Cottage at dawn: Sunny's first sight of home. The door wobbles like jelly. The room is empty except for a crate and a window.
  * Sunny sets the suitcase by the door, still packed. The pet curls up in one eggshell half on the sill, facing the Wall.
  * A knock. Nana has two muffins, one each, and her second pie dish, scrubbed clean, with a burnt cookie on it.
  * NANA: "Somebody returned my dish." [R13] She doesn't say who.
  * She sees the pet's two tiny horn nubs. "Ah." [R8] "...He's teething."
  * Smash to the credits on the ITS_FINE sting.
* **End credits (0:30).** Crayon storybook stills of the night, because tonight is history now: the banner, the bubbles, three candles, a pink heart over the Wall, a purple firework answering. The score is a music-box arrangement of the theme's hook. The Mingle tune is kept unfinished for the finale. The title card at the head of the credits has peeled to read **MINGLE NIGHT**.
* **Post-credits button (0:10).**
  * Night. The sealed gate: seven padlocks, KEEP OUT. FOREVER.
  * A stray Giggle lantern drifts over the Wall and sinks out of sight behind the gate. R13.
  * A gurgle so deep that the padlocks rattle, one, two, three, 15 frames apart, each bigger.
  * Topper at R13: the gate bulges outward once, *BOING*, and settles. The lantern does not come back.
  * Smash to the Dippinn Productions card, silent until its last frame: "gurgle."

---

## 9. Characters introduced and their pilot arcs

| Character | Who they are | Pilot arc | Seeds |
|---|---|---|---|
| **Sunny** (they, by show decision; a young adult in their first home) | A bunnykin newcomer who never speaks. Canon gives the protagonist **three lines in the whole story**. They act by looking, pointing, carrying, pouring and pumping. That also makes Sunny the perfect first-person shooter hero: the genre's great leads don't talk either. | Arrives with nobody to meet them and ends the night as the one who carried the tin, poured the water and kept the pet safe. At the freeze Sunny opens their mouth, and nothing comes. The suitcase stays packed. | The countdown to the three spoken lines (Ep 3.7, Ep 4.4, Ep 4.6). The suitcase is unpacked in the epilogue. |
| **The pet** (it) | A Doomogachi: a Mingle egg, half plush and half brimstone, and the only creature that belongs to both sides. Its vocabulary is "beep." | Hatches mid-possession, on the minor downbeat. Its first "beep." gets a ten from a demon. Demons flinch at it. It keeps time for the rite, and it is the face on the HUD. By morning it has horn nubs. **Its name is never said or shown.** | It eats the Mayor's fear and runs over the Wall (Ep 3.7). Sunny's first spoken word is its name. |
| **Mayor Giggs** (he) | A cherub with wings, a top hat and a bow tie, and the sing-song "Hee. Hee.": "the sound of a man not looking at something." | Goes from painting over MINGLE at dawn to lighting one firework at midnight "for the patrol". He says the theme line for the first of its three times, as a joke. He hides, being calm. He is the one who stops the name. His hum gets one note further. | The "(+1)" on the cupcakes, the hum, the crayon bleed at his edges. Later: the confession, "I had a cloud once", "...Hi, Fluffy." |
| **Nana Puddin** (she) | A sheepkin baker who calls everyone "sweet pea", bakes two of everything and hides things in food. | Hands over the key, the egg, and then her Mingle tin, the secret she keeps. Her one look at the Mayor lasts exactly R8. She knows the rating voice. | Charlotte and the Thursday cookies (Chain D). The returned dish. "...He's teething." |
| **Pip** (they; a kid, friendship and courage only) | The tiniest bunnykin, with the highest and fastest voice in the cast, on JUNIOR FESTIVAL PATROL with Mr. Flops (one floppy ear, one not-floppy, a stitched smile). | Thinks brave means not crying. Learns that brave means saying it out loud while your eyes do a rain. Sees a first firework. | Night patrol, Mr. Flops, the paper-boat pen pal (Wisp, in Ep 102-104). Payoff: "Rain's good for stuff." |
| **Father Fuzz** (he) | A plushie bear priest, "stuffed with love and a little bit of buckwheat". "Nothing is unhuggable." | Tonight is the anniversary of his failed first exorcism. He runs his first rite since, and the hug fails again. Then the name works. "It was the name. Not the hug." Sunny hugs him anyway. | The Mr. Huggins practice (Ep 2.4). "What am I FOR?" (Ch3). The canon payoff his last beat sets up: "The hug was never for the demon. It was for the one holding the demon." |
| **Bonejangles** (he; guest lead) | A Hillvile skeleton xylophone teacher who plays his own ribs and says "Clack clack!". He played the last Mingle. | Possessed in front of the whole town. His tune turns minor. He wakes and gives the breath the last line of its poem. | Nothing in canon depends on him, which is why he is the pilot's host. Later he plays at the series-finale festival. |
| **Clinker** (she; Hellvile, heard but never seen) | The real name behind the breath: a skeleton gate labourer who writes poems in chalk and rates everyone else's ("Six. Needs death."). | Her voice runs R8 late under Bonejangles' all night. Once the breath is home, her voice comes **on time**: "...Seven." | Ep 4.2's pump yard, canon: "I write poems on the pumps. Forty, so far." |

**Cameos**, one beat each:
* Steamy (she): "I don't know that one."
* Kevin (he): "Yes, three eyes."
* Grampy (he): "Sunny... Sonny... Bunny... Toaster."
* Hattie (she): "Knock knock... Nobody ever says." / "...Yikes."
* Miss Lulla (she) and her kids: the crowned-cloud drawings.
* Dr. Bloom (she): "No pulse." [R8] "He never has one."
* Mascotty (she): still in the crowd, thumping 3-then-1, lit by the firework.

**Felt but not seen:**
* Rivet (she): the prize case, signed "—R".
* Charlotte (she): the scrubbed dish.
* Fluffy: a cloud on a string in crayon, and later a shape that turns toward a firework.
* Whoever is behind the gate: GURGLE (it).

---

## 10. The five best gags

1. **The ratings.** A rule of three at the possession reveal, 15-16 frames apart, with the numbers stepping down:
   * Nana's pie: "Six." [R8] `{d}`"Needs death."
   * The Mayor's skull-sprinkled cupcake: "Four. Too much death."
   * The Mayor's speech, held up hopefully: "Two."

   Then the scale keeps running all night:
   * the pet's first "beep." gets "...Ten.", the only thing both sides agree on;
   * "THE POWER OF CUDDLES COMPELS YOU!" gets "...One.";
   * the breath's own unfinished poem gets "Three. Needs an ending.";
   * and the ending Bonejangles gives it gets "...Seven.", from the far side of the Wall.

   The last rating in the runner is also the emotional peak of the episode.
2. **"21:00 (we don't do that)."** It is set up twice: the clipboard insert, and Pip asking. At 21:00 the whole festival looks up, rippled 0-3 frames. The sky is empty. MUTE. R26. One cricket. Somewhere far off, a balloon pops. MAYOR: "...Hee. Hee." Then the fog arrives at the exact hour the fireworks used to.
3. **Hatched in hell.** The stampede runs on the accelerating curve, with a sweat drop every third frame. In the middle of it, the egg hatches on the downbeat of the possessed minor riff: a white flash, two glossy eyes. Then HOLD 21 and MUTE 29 while the chaos carries on in silence behind it. "beep.", in the smallest speech bubble in the show. R8 later, the deadpan caption: "...it's fine." The nearest Puff Demons flinch. The demon in the bandleader turns round, and after R13: "...Ten."
4. **GORE: BRUTAL (approved).**
   * *Squeak-CHK*, a record scratch, and the cut to first person.
   * The HUD slams in on the downbeat of the insert song: hearts for health, sprinkles for ammo, the pet's face in the centre slot glancing left and right.
   * The HUD's mode label reads "GORE: BRUTAL" for 8 frames. Then the "HELLVILE PARK BOARD OF DECENCY (approved)" sticker slaps over it, and peels away to show "GORE: CUTE".
   * From then on every blast pops into confetti, hearts and stuffing.

   The show announces its own rating as a joke, then keeps to it.
5. **The banner.** The Mayor paints GIGGLE over MINGLE and the wet N bleeds back through. He rolls; it bleeds. He rolls; it bleeds bigger. Topper at R13: he ties a cupcake balloon over it, pats it, and says "Perfect." Then, after a 4-frame insert, one change: the balloon has turned to face the Wall. Across the episode this one prop states the thesis: a town painting over a word it can still read.

**The bench**, in case one of the five needs swapping:
* "Everybody stay calm! I'll be under here. Being calm!"
* "No pulse." [R8] "He never has one."
* "NOTHING INSIDE! (we checked) —Mayor"
* "TOP PRIZE. Please don't. —R"
* "Patrol: three steps back." Pip takes three, then one more for Mr. Flops.
* The blown-off T: "BAKE TEN".

---

## 11. How the pilot seeds the season

### Season shape

| Season | Game chapters | Episodes | Ends on |
|---|---|---|---|
| **1, "Something in the Vapor"** | P0, Ch1 and Ch2 | 13 episodes:<br>• 101, the pilot;<br>• 102-104 finish Chapter 1 (the cottage, the three neighbours, the housewarming, the boats) and close Arc 1 on the glow blinking back;<br>• 105-113 are Chapter 2: Cloud Nine, "Grape", the haunted house, Steamy's possession (TALLYWORN), and **"The Bottle" at 107 as the mid-season turn**, then Rivet, the dream "Teeth Like Muffins" and "The Cellar". | The gate cracks, Grumbleguts debuts, and "THE DEMONS OF HELL ARE HERE." |
| **2, "Hell Invades"** | Ch3 | 13 | The plumbing reveal (game Ep 3.4) lands at S2E7, the **series midpoint**. The season ends on the pet running over the Wall and the dark night: "I had a cloud once." |
| **3, "Hellvile Park"** | Ch4 and the epilogue | 13 | "...Hi, Fluffy." The Wall falls, and the festival fills the whole park. A special follows: the first real Mingle in eleven years, one year to the night after the pilot. |

**The bookend.** The pilot is half a festival in half a park, and the series ends on a festival in the whole one. The pilot's title is also the title of the special that closes the show.

### Seed table

| Pilot seed | Where in the pilot | Canon payoff |
|---|---|---|
| **The plumbing secret** | | |
| The fountain giggles R8 (267 ms) late | Cold open, Act One | S142, "267 milliseconds late... So who's laughing?" Then Ep 3.4: the fountain cracks and the pipes run under the Wall. |
| The possessor's voice runs **exactly** R8 late, as late as the fountain, and comes on time only once it is home | Acts Two-Four | The breath and the giggle come from the same place: the other side. Ep 3.4 shows the route. |
| "Down by the Wall where the pumps go clink, / we work all night..." | Acts Two and Four | The pump yard (Ep 4.2), the 900-page contract, the workers kept pumping. |
| "Nobody on this side says those names." | Act Three out | Canon, story_final §0: shouting a true name is "the first time in ten years anyone on this side has said that worker's name." It is the same habit of not asking as "Keep giggles IN. Pump glums OUT." and "...I never asked where it went." |
| "We don't talk about the other side." (use 1 of 3, as a joke) | Act One | Use 2 is the confession (Ep 3.4). Use 3 is Hattie's festival opener (Ep 4.7): "Because we're STANDING in it!" Her Giggle tent in the pilot is the seed for that stage. |
| **Fluffy and Vapula** | | |
| A cloud on a string that loved fireworks; "Not everybody went home." | Cold open | Fluffy came home huge and became Lord Vapula, made of every sigh Hillvile swallowed. |
| The music box stops on one note; the Mayor's hum stops on the same note; at midnight he gets one note further | Cold open, Act Four | Mingle in full, and the finale's lift from E to F, the key of MINGLE (Ep 4.6). |
| The crowned purple cloud in every child's drawing | Act One | W12, "Employee of the Month", Vapula. |
| The 4-frame kids'-tent insert, a cloud-shaped lady and a crowned silhouette with a string, from Fuzz's point of view | Act One out | F2 "Mingle Night" (Ch3): Lady Smog, the failed hug, "Close the park." |
| The vast crowned shape turning toward the firework, its string trailing | Act Four | It loved fireworks. "...Hi, Fluffy." |
| "Giggs: cupcakes (+1)", folded over fast | Act Three | The cupcake on Fluffy's grave (W16, Ep 102). Interlude III. |
| **The Wall** | | |
| The bandstand cut in half and boarded up; the MINGLE crate, "DO NOT OPEN" | Throughout | The Wall falls (Ep 4.6), and the bandstand is whole at the closing festival. |
| The breath goes home through the gate's keyhole | Act Four | The gate cracks (Ep 2.7). |
| GURGLE, the rattling padlocks and the BOING | Post-credits | Grumbleguts' debut insert (Ep 2.7) and his Ch4 arc. |
| The 3-then-1 thumps, and Mascotty thumping back | Act One | Thumpkin (Chain J; Ep 4.2: "for my sister"). The head comes off (Ep 4.6). |
| An unexplained purple firework answering from the south | Act Four | Someone over there still keeps Mingle night. Ch4 (Mother Nox's two bowls) is the room's to claim. |
| **Also seeded** | | |
| "She kept trying to finish a poem." / "...She?" / "...Seven." | Act Four | "The Bottle" (Ep 2.5, at 107): "That was a PERSON." |
| "TOP PRIZE. Please don't. —R" | Act Two | Rivet, "Don't Make It Weird" (Ep 2.6), and F1 "The Fountain Gift". |
| "Charlotte: cookies (sorry)" and the scrubbed dish | Act Three, tag | Chain D and S199 "The Loose Brick". |
| The key with tiny horns, and the horn nubs ("He's teething.") | Act One, tag | The pet keeps its horns forever. Post-credits, its poop grows horns. |
| Cloud Nine Vapor, "COMING SOON" | Act One, background | Vex (Ep 2.1) and its payoff: "plain water vapes. They're terrible." |

---

## 12. Why a network or streamer would buy it

* **The whole show in one night.** In 22 minutes the pilot shows every mode the series has: cozy life-sim, small-town comedy, mystery, horror, DOOM action and real heart. Buyers see the format working, not only the premise.
* **A repeatable engine, proven on screen.** The pilot runs the exorcism-of-the-week structure: a neighbour possessed, clue props, a true name, a three-part rite, and three candles as visible stakes. Every week can repeat it with a different neighbour and a different name from the other side, while the serial mystery advances underneath. That is a procedural engine under a serialized mystery.
* **A tentpole built in.** Midsummer is the show's own holiday. The pilot, the closing special and annual "Mingle Night" event episodes give marketing a date to own every summer.
* **Trailer-ready set pieces.** The brutal switch, the hatch in the stampede, the ratings runner and the firework over the Wall each cut down to a 15-second clip that sells the show.
* **An anime-style opening that comes back.** "Doomtastic Times" is a J-pop-leaning main title, and here it returns as the insert song at the climax, which is anime grammar fans already love. The score's leitmotif system has the HaHaland and Hellvile themes as the same tune in major and minor. The pilot uses that as story: a possession is the moment the fourth note bends.
* **A franchise that already exists.** There is a real GBA ROM, a real PS1 disc, a 78 s trailer, a studio ident, model sheets for 1000 citizens, and a story bible written through to the finale. Merchandise suggests itself: the pet and its "beep.", Mr. Flops, the Bubble Wand (never the shotgun), and a lantern-ribcage Bonejangles.
* **A shape that suits every platform.** It fits ad-supported broadcast (three natural breaks), streaming (eyecatch chapter markers, skip-intro) and 2x11 markets (a clean split at the Act Two out).
* **Cheap, fast development.** The 256-colour palette is the show's colour script, and the trailer toolkit turns this beat sheet into a timed animatic in a day (§14).

---

## 13. Risks and mitigations

| Risk | Mitigation |
|---|---|
| **Burning canon order.** The pilot moves Fuzz's first rite since Lady Smog, Rivet's two weapons and the beats of game Ep 1.6 forward into one night. | It uses a **pilot-only host and possessor**: Bonejangles, and the breath of Clinker. Steamy and TALLYWORN, Sprinklepop's scripted SOUL LOST and "The Bottle" all stay where canon puts them. Rivet stays unseen until Ep 2.6, which can open on "You WON that?" [R8] "Nobody wins that." Fuzz's success becomes his problem: "I got lucky. Luck isn't a liturgy." That leads straight into the Mr. Huggins practice. |
| **An overstuffed event pilot.** | The density curves in the bible: one punchline per 3-6 lines of dialogue, a 5-15 s rest after every big beat, and three gag-free holds of 3-5 s (Fuzz on the steps, "Seven.", the firework). Three elastic beats give about 1:00 to cut. The Giggle crawl is the first cut, because all of its cameos return in Ep 102. |
| **A firearm-like prop** in a show for 10-16s. | A junk-welded contraption with sprinkle shells, a squeak in its cocking sound, and confetti outcomes. It is never aimed at a citizen. Standards and practices review the prop at the design stage, and the "Pump Sprinkler" alternate is drawn in parallel. The Bubble Wand is the merchandise. |
| **Possession and exorcism in some markets.** | The rite is hugs, a hymn and water; there is no pentagram in this episode, and the possession is stillness, not body horror. Standards and practices review starts on the series bible, not after delivery. |
| **A kid in peril at the climax.** | Pip is never touched by the fog and stands behind Fuzz the whole time. The scariest beats are made of stillness and silence. Pip's moment is courage, not danger. |
| **Imitable behaviour (fireworks).** | An adult lights the rocket, and the kid is three (four) steps back. That safety staging is also a gag. |
| **Trademarks:** "DOOM" and Tamagotchi in the title, "Doomtastic" in the song, and a DOOM-style HUD. | Clear the series and song titles with counsel before pitching, and prepare a fallback series title. The HUD is our own layout (hearts, sprinkles, the pet), with no id Software assets, fonts or names. |
| **Flashes and inserts.** Broadcast codes forbid subliminal images (Ofcom rule 2.11, for example). | Every insert plays for 4 frames, never 1. White flashes last at most 3 frames, at reduced luminance. Harding/PSE passes on the hatch, the hellify flashes, the muzzle bursts and the firework. |
| **A silent lead** who can't do exposition. | It is a tradition (Keaton, Pingu, every great FPS lead), and here it is a hook. The pet, Pip, Nana and the Mayor carry the dialogue. Sunny's choices are staged as action: the glass, the pump, the pour. |
| **The theme song is not locked**, and the insert-song plan depends on its stems. | The opening is boarded by section and conformed to the master. The bus drive absorbs ±5 s. If no DOOM-side stem exists, the trailer's RIFF covers Act Three and the theme's hook is quoted on the last blast. |
| **Pixel art at broadcast size.** The toolkit is 3:2, has no camera system or lip-sync, and every scene hard-codes its layout. | Re-stage on 384x216. Add a camera and rig layer, mouth-shape lip-sync, an external audio track, and named beats shared by picture and music, replacing the duplicated `FR(n)` constants (§14). |
| **Grumbleguts' content later in the season.** | In the pilot he is sound only. His show-safe rules and the concept-sheet cuts are in the bible from day one: no hose close-up, no gas gag, no crop or unbuckle poses, no "...that's hot", and the harness becomes a gatekeeper uniform. The `docs/design/story/refs/*.webp` images are third-party references and never appear in pitch material. |

---

## 14. Production notes: what "well produced" means for this pilot

### Picture

* **Canvas.** 384x216 native pixels, scaled nearest-neighbour by whole numbers to 1080p (5x) and UHD (10x), at 29.97 fps. Text stays inside title-safe.
* **Graphics as text.** Diegetic captions, signs, the sheet and the clipboard are all graphics, so delivery includes a textless master.
* **Fix the trailer's chibi presets to canon species before any pitch art:**
  * Mayor: a cherub, not a sheep;
  * Nana: a sheepkin, not a bunny;
  * Pip: a bunnykin, not a mouse;
  * Sunny: a bunnykin, not a fox.
* **New presets needed:** Bonejangles (a skeleton, with an emissive ribcage layer for the possession), Steamy, Mascotty, Kevin, Hattie, Grampy, Dr. Bloom and Miss Lulla.
* **Model sheets.** `core/tools/portrait_sheet.c` renders ids 0, 1, 2, 5, 36, 42, 71, 77, 88, 106, 111 and 753, including the possessed state for id 5 (Bonejangles).

**Colour script by the hour**, built from the game's palette ramps:

| Time | Look |
|---|---|
| 18:00 | PASTEL_DAY into the SUNSET sky; CANDY and PASTEL_PINK stalls |
| 19:00-21:00 | NIGHT_STARS sky; warm PEACH lantern pools (the NIGHT tint keeps emissives lit) |
| 21:00 | The VAPE ramp floods in from the south as the lanterns die; red eyes are the only hot colour |
| The brutal register | The HELL, BLOOD and LAVA ramps through hellify. **These ramps appear only here, in the main title and in the IN cards' flashes.** |
| The rite | BONE and holy gold against VAPE |
| 23:00 | A white-pink firework, one purple firework, and crayon bleeding at the Mayor's edges for 8 frames |
| Dawn (tag) | PEACH and WOOD |

### The brutal register (Act Three)

* **View.** First person, built from the trailer's s10 raycaster and re-staged to 16:9.
* **HUD.** A bottom strip 32 px high:
  * hearts for health on the left;
  * sprinkle shells for ammo on the right;
  * in the centre, the pet's face, with five states: idle look-around on an 83-frame cycle; a grin on a pickup; a wince on a hit; a horn nub and glowing eyes at one heart; and a squint at the sticker.
  * A tiny engraved plate reads "RIP AND TEAR... GENTLY".
* **Getting in and out.** In: SCRATCH, then hellify rots the palette over 21 frames. Out: the REWIND sound, back to cute, with one persistent change.
* **Hits.** A hit on Sunny is a grape-goo splat on the "lens" and a heart popping off the HUD. Sunny is never seen hurt. Every blast is a sprinkle burst, with shake profiles POW and STAMP and a sound on frame 0.

### The exorcism

* **Stakes on screen.** Three candles on the rail. Each fail is kazoo notes stepping down, one candle out, a red flash of at most 3 frames, and a FAIL xN slam.
* **The prayer duel** is staged as music. Fuzz's hymn meets the hook bent into the RIFF on the ribs, and the possession drags him R8 late until his hymn turns minor.
* **Try 3:**
  * the rhythm is the pet's piezo "beep." on every beat, with the crowd clapping in a ripple;
  * the water is poured, not thrown;
  * the name: MUTE ALL, Pip's voice alone and unprocessed, R5, then the TRUE NAME motif and the CHOIR, and the purple drains over 21 frames;
  * the GOODBYE motif plays under the puff at the keyhole.

### Sound

* Every cut has a sound on frame 0.
* 20-25% of every act has no score.
* A record scratch always goes to silence, then to a small sting.
* Mutes keep the sequencer running, so the insert song re-enters on the grid after the hug cut.
* **The "we" voice** is Bonejangles' actor with Clinker's actor beneath him, slid exactly 8 frames late. "...Seven." is her track alone, on time. Casting notes from the roster sliders:
  * Bonejangles: low, very expressive, mid-speed (pitch 8, intonation 29);
  * Clinker: low, slow, flat (pitch 7, speed 6, intonation 5).
* **The pet's "beep."** is the game's own piezo motif, the one sound kept raw from the GBA.
* **Mix and delivery:** 48 kHz in stereo and 5.1, delivered at -24 LKFS (ATSC A/85), -23 LUFS (EBU R128), or each streamer's dialogue-gated spec. -14 to -16 LUFS is for web clips only. Delivery includes M&E, the theme stems, closed captions (including "[beep.]") and an **audio description track**, which a silent lead makes essential.

### Music spotting

| Segment | Cue |
|---|---|
| Cold open | MINGLE on the music box. It mutes for 29 frames mid-phrase, and the Mayor's hum takes over at its pitch. The bus horn plays the hook's first two notes. |
| Main title | "Doomtastic Times", main title cut |
| Act One | HaHaland Day (C major, 140 bpm) as the bandstand's diegetic music. At 21:00: MUTE, a cricket, the thumps. The first OMINOUS cue comes with the fog, as canon holds it back until then. |
| Act Two | The hook bends to the RIFF (E minor) on Bonejangles' ribs, and that diegetic music becomes the score. The hatch lands on its downbeat. The fail stings. |
| Act Three | The cute riff, then SCRATCH, then the TV size's chorus in its DOOM-side arrangement as the insert song (about 1:15 across the two first-person segments). Silence for the hug. |
| Act Four | Fuzz's hymn with the beep as metronome. MUTE for the name. TRUE NAME, CHOIR, GOODBYE. The hook in major on the ribs. Silence for the rocket, then the Mingle hum, one note further. |
| Tag and credits | The NANA motif, the ITS_FINE sting, then a music-box arrangement of the theme's hook |
| Post-credits | GURGLE, the comedy kit's bass babble |

### Animatic and asset reuse (week 1)

* **Timed animatic.** List this beat sheet's rows with their durations in `trailer/timeline.c`. Unbuilt beats render as timed placeholder cards, and `--sheet` contact sheets serve as the thumbnail boards.
* **Set pieces to adapt from the trailer:**

  | Trailer scene | Becomes |
  |---|---|
  | s03, the town pan and record-scratch brake | The arrival |
  | s04, the pet hatch | The hatch |
  | s07, the possession inhale (a 10-frame stretch, a 5-frame hold, FWOOMP) | The fog entering the ribs |
  | s08, the party freeze | 21:00 |
  | s10, the raycaster | The Absolutely Not Scary House |
  | s11, the exorcism rule of three | The bandstand rite |
  | s12, the census crowds | The Giggle crowd |
  | s15, the post-credits sting | The gate |

* **Demon sprites from `artgen`:** Puff Demon (the swarm, and the Big Puff at 3x scale) and Impling.

---

## 15. Canon decisions this pitch makes (flagged for the room)

1. **The Midsummer Giggle is invented.** It is a Hillvile-only festival on the old Mingle date, with no fireworks and the bandstand's south half boarded up. It is consistent with canon, which has no Mingle for eleven years and saves "the first Mingle in eleven years" for after the Wall. The banner where GIGGLE is painted over MINGLE twins the painted-over 1000 on the sign (W2).
2. **Sunny arrives on Midsummer.** Canon does not date day 1. Sunny is **they** and a young adult, and is never a target of romance or innuendo. **The pet's name is withheld** until Sunny's first spoken line (canon Ep 3.7).
3. **A pilot-only possession: Bonejangles (5, he) by the breath of Clinker (753, she).**
   * In canon, Clinker is a Hellvile gate labourer and pump-yard poet. Her name is a decoy in Hattie's rite, and she speaks in Ep 4.2 and Chain Q, where "Six. Needs death." and "Four. Too much death." are her ratings.
   * Her breath is an **addition** to the authored possessor table. Every existing row is untouched.
   * Both skeletons fear thunder on the roster, which gives the writers a free "why him" line.
4. **Moved earlier:**
   * Fuzz's first rite since Lady Smog (canon's first true name is Ep 2.5), with his canon battle cry "THE POWER OF CUDDLES COMPELS YOU!" (Ep 2.5) and creed "A demon is a frightened thing in a loud coat." (Ep 2.4).
   * Rivet's Bubble Wand and Pump Shotgun, here as her unseen prizes; the Ep 2.6 menu becomes an upgrade.
   * Ep 1.6's fog beats: red eyes, "come and PLAY with us", and the kids'-tent insert. Ep 104 keeps the thumps at home, the second boat and the candle, and the glow blinking back.
   * Interlude I's horn nubs and "He's teething."
5. **Invented and consistent with canon:**
   * The Mingle sign-up sheet in Nana's recipe tin. Canon already keeps a slip from the last Mingle in that tin: "Charlotte's turn: #9".
   * The MINGLE fireworks crate.
   * The answering purple firework, left unexplained.
   * The town's taboo on Hellvile names, which canon states (story_final §0).
   * The ratings scale beyond Clinker's two canon ratings.
   * The R8-late possessor voice.
   * The Absolutely Not Scary House and the Hook-a-Duck. The Fun Fair and the Whack-an-Imp machine are canon.
6. **Grumbleguts** is sound only (GURGLE, it) in the pilot. Series picks: his crush is **the Gate** (per story_final), and his catchphrase is **"Open late."** ("Sulfur kisses!" is shared with Vex).
7. **Continuity:**
   * The theme line is used once, as the joke, leaving its other two uses intact.
   * The starer is Mascotty; the trailer's "plain human" is the separate figurine gag.
   * Rivet's age is never stated, and Bolt is xe.
   * Nana's "He's teething." is canon text. The pet is "it" everywhere else.
