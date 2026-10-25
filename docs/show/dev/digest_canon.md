# Canon digest for the pilot: DOOMOGACHI LIFE in HELLvile Park!

> **Development reference.** A reading digest of the repo canon made for the pitch round. `docs/design/story/story_final.md` wins every conflict with it, and the show's own decisions (Sunny's pronoun, the pilot's inventions, the standards) are in `../PILOT_TREATMENT.md` §9 and `../SERIES_OVERVIEW.md` §5.6. See `README.md` in this folder.

Purpose: the canon a pilot-episode roadmap needs, read from the repo. Repo root: `/home/user/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark`.

**Precedence.** `docs/design/story/story_final.md` is the merged final story and wins every conflict. Use `bible_heart.md` and `bible_horror.md` only for texture they add that story_final does not contradict. The roster (`core/data/roster/*.txt`, `docs/ROSTER.md`) is canon for species and pronouns, except where the task's canon rules override it (see §16). The shipped Chapter 1 script (`core/data/story/ch1/*.dsc`) supplies the exact on-screen lines for P0 and Ch1. It adds some extra lines and props, which are marked below.

---

## 1. Premise

**One breath (story_final §0).** You arrive in HaHaland on a pink bus with one suitcase and nowhere you belong. The town gives you a cottage with a pink door, a muffin with a key in it, and an egg that hatches into a tiny creature that says "beep." Everyone is lovely, everyone giggles, and nobody talks about the other side of the Wall. Then the fountain giggles half a second too late.

**The engine.** HaHaland's endless happiness is **plumbing**. Ten years ago the town learned to swallow every sad and scary feeling and flush it through the Giggling Fountain, under the Wall and into Hellvile. There it fed a cloud, and the cloud became **Lord Vapula, Duke of the Ninth Cloud**: he is made of every sigh Hillvile never let out. The "demons" that possess your friends are breath. Each one is named after the Hellvile pump worker whose lungs it passed through.

**The face under the twist.** The cloud was **Fluffy**, nine-year-old Giggs's pet cloud. Fluffy drifted away on a Mingle night and came home thirty years later, huge and purple and lonely. The town was too scared to recognise him.

**The cure.** The cure is **"say it out loud,"** not a bigger gun. The Wall falls when 1000 critter-folk each say the one thing they were scared to say, and the Mayor says "...Hi, Fluffy." to a cloud the size of a cupcake.

**Game frame.** "GBA tamagotchi x Tomodachi Life x DOOM." The game is cute first. Its horror and gore are funny *because* the first hour is a sugary daydream. It ships as a real GBA ROM and a real PS1 disc, and the teaser trailer is a 78-second pixel animation (`trailer/README.md`).

---

## 2. World rules

* **Geography.** HaHaland is the land, a candy-coloured town of cottages, cafes, flower beds and a big pastel park. **Hellvile Park** is the giant park at its heart, split by a river and a big ugly Wall:
  * **Hillvile** is the north, "good" side: 500 citizens, ids 0-499, about 60% fluffy critter-folk.
  * **Hellvile** is the south, "bad" side: 500 citizens, ids 500-999. They are imps, ghouls, skeleton bureaucrats, hellhounds and cacoblobs. All are drawn cute, and 12-18% of them are secretly lovely (`SOFTIE`).

  The game's title names the park because the ending is everyone commingling in it.
* **The Wall and the gate.** One sealed gate stands in the Wall. The shipped Ch1 sign reads: "A huge iron gate, seven padlocks and one very stern sign: KEEP OUT. Underneath, smaller: FOREVER." Rivet welded its lock at age ten. Gatekeeper Grumbleguts sits on it from the Hellvile side.
* **Landmarks.** The Giggling Fountain: two laughing faces, originally two-way; you laugh here and it bubbles up over there. The Population sign says "500", with a scratched-out "1000" under it. Also:
  * the graveyard of joke graves;
  * Brimstone Bites, a Hillvile cafe "named for a joke. It's ironic!";
  * Church of St. Cuddles, Steamy's bath house ("the pond"), Nana's bakery, the daycare, Hillvile Towers, the beach;
  * the Pink Door Cottage at Hillvile Lane (4,7).
* **Species and materials.** Plush critter-folk (cat, fox, bunny, bear, pup, mouse, sheep, raccoon, owl, frog, moth) live alongside imps, ghouls, ghosts, skeletons, slimes, cherubs and plushies. Materials move differently:
  * **plush squashes and wobbles**, up to 34%;
  * hardware is stiff, 18-22%;
  * **hell drips and never wobbles**;
  * **Grumbleguts alone bounces** like a balloon.

  Nothing is ever static: characters breathe, blink and twitch ears on co-prime cycles. Possessed characters freeze, with ears twitching too fast.
* **Possession and exorcism.** Corruption rises at night (from vapor, nightmares and so on). At 100 a citizen is POSSESSED: red hollow eyes, `bob = 0`, and dialogue in "we". Father Fuzz's rite has three parts:
  1. prayer rhythm;
  2. holy water;
  3. the **true name**, which is a real Hellvile citizen's name, picked from clue props.

  Three failures lose the soul (SOUL LOST). The soul wanders as a ghost and can later be guided home by a reverse rite ("EXORCISM UNFAILED x1"). A saved exorcism sends the breath home. After "the Bottle" (Ep 2.5) every exorcism carries the knowledge that **"That was a PERSON."**
* **Violence.** Cartoon violence is aimed at demons and hell-stuff only: cloud spiders, imps, Breach bosses. They pop into confetti, hearts, plush stuffing and grape goo (the game's CUTE setting). **No cute character is ever shown hurt in the cute half of a whiplash cut.** Nothing mocks real people.
* **Time.** Story days pass by sleeping at home. Chapter 1 covers days 1-4. Night beats are real nights (Ep 1.6 runs after 21:00).
* **The Doomogachi.** A Mingle egg, half plush and half brimstone, from the days of one park. It is the only creature that belongs to both sides. It grows horns under stress. Its whole vocabulary is "beep.", a horned "beep." and "bep?".

---

## 3. Secret history (the player digs it up in pieces)

| When | What | Who knows | Revealed |
|---|---|---|---|
| ~40 yrs ago | Hellvile Park is one park. The **Midsummer Mingle** potluck is held at the central bandstand. Little Giggs (cherub) brings cupcakes; little Nox (imp) brings bone soup; they are best friends. | Nox, Mayor, Nana, Grampy | F4 |
| ~31 yrs ago | Giggs, nine, has a pet cloud, Fluffy, who rains on bullies ("only bullies; he was very precise"). On Mingle night Fluffy chases the fireworks south, the string slips, and nobody notices. Giggs digs a joke grave, "Fluffy the cloud. Drifted.", and never says what it is for. | Mayor; Nana suspects | P0, W16, S196, Int III |
| 12 yrs ago | Anvil, Rivet's mum, welds the two-way Giggling Fountain as a Mingle gift. Rivet, six, holds the torch. | Anvil, Rivet | F1 |
| 11 yrs ago, Mingle night | A huge purple cloud rolls over the festival. It is Fluffy, grown on nobody's attention and on sulfur. Lady Smog breathes it in (the first possession) and walks into the Hillvile kids' tent saying the children's fears in their own voices. Young Fuzz tries a hug, and it fails. Young Mayor Giggs does not recognise the cloud and says "Close the park." | Fuzz, Smog (no memory), Mayor (half) | Ep 1.6 insert, F2 |
| 11-10 yrs ago | The Wall goes up. Hellvile labourers are paid to build it, then **kept** under a 900-page contract (by Signhere) to pump the cloud's vapor. Tallyworn starts counting. | Tallyworn, Signhere | Ch4 |
| 10 yrs ago | The Mayor has the fountain re-plumbed: "Keep giggles IN. Pump glums OUT." The plumber asks "Out where?" Young Giggs answers "Not here." He never asked where it went. Fed on the town's fear, Fluffy learns to talk and calls himself Lord Vapula. | Mayor (half), Vapula | Ep 3.4 / F3 |
| Now | Vapula wants to be **breathed in**, felt by the people who made him. Vex opens Cloud Nine Vapor by the Wall. Grumbleguts sits on the sealed gate, waiting for a feast promised ten years ago. | Vex, Haze, Grumbleguts | Ch2 |

**The midpoint twist (Ep 3.4):** the fountain cracks, and the pipes beneath it carry Anvil's hammer mark under the Wall. *We* made the monster. **The secret under the twist:** the monster was a pet.

**True names in the story:**

| Possessed | True name |
|---|---|
| Steamy | TALLYWORN |
| Sprinklepop | WHEEZLE, never offered (scripted fail) |
| Hattie | HAZE |
| Grampy | SOOTKIN |
| Lady Smog | FLUFFY (VAPULA is a near miss; GIGGLESWORTH is a strike) |
| The leftover breath | GIGGLESWORTH (the Mayor's own fear) |

---

## 4. Theme line and its three uses (plus the thesis)

**"We don't talk about the other side."**

1. **Ch1, Ep 1.1, as a joke.** The Mayor says it deadpan, `{d}`, followed by "Hee. Hee.", while the fountain giggles late.
2. **Ch3, Ep 3.4, as a confession.** "We don't talk about the other side. ..That was the whole point. ... I never asked where it went."
3. **Ch4, Ep 4.7, as a punchline.** Hattie Haha opens her festival set with it ("Because we're STANDING in it!"), and both sides laugh. The Mayor lets out a "Ha!", everyone freezes, he says "...Ha?", and they laugh harder.

The shipped Ep 1.1 script even winks at this structure on NG+: "(He will say it twice more.)"

**The thesis line is "say it out loud".** It is seeded in Ch1: everyone avoids the Wall, and the player's menu can ask "What's on the other side?", which freezes the whole town. It pays off in Bloom's confession (Ep 3.4), the Mayor's "I had a cloud once" (Interlude III), and the "Say It" finale.

---

## 5. Tone references

* **Main reference (story_final §0):** "Shaun of the Dead, Gravity Falls, Undertale and DOOM, sewn into a plush toy."
* **How the world goes wrong.** The pastel world goes wrong first in the background, then in your house, then in the sky. Every scare is undercut or doubled by a joke, except a few chosen ones that are not.
* **Game references:** Tamagotchi (an LCD-green pet screen), Tomodachi Life (a citizen sim with apartments and thought bubbles), DOOM (Hell Breach FPS levels, saw-riff metal).
* **Grumbleguts' design references:** a pot-bellied cartoon devil, Baloo's belly-forward posture, a gym-demon build, and the Red Guy (Cow and Chicken), whose rear is always in frame. The user's own words: "Animal Crossing mixed with DOOM and Hazbin Hotel sympathies." For the show, the Hazbin influence stays at **energy** level only, and the content limits in §6 apply.
* **Art.** Premium plush chibis with soft hue-shifted shading, coloured outlines and rim light. The cute set is pastel candy-land; the hell set is "grotesque Doom" (flesh, bone, lava, rust, vape clouds), still drawn cute. Special styles:
  * crayon/children's-book for history (P0, and edges bleeding in during the Mayor's confessions);
  * sepia for flashbacks;
  * LCD-pixel for the pet's dreams and its 4-frame hell inserts.
* **Studio.** The trailer opens with a **Dippinn Productions** logo bumper.

---

## 6. Content boundaries for the show (binding) and the rating

* **Recommended rating: TV-PG (D, V), and never higher.** The core audience is about 10-16 plus adult animation fans, safe for families to watch together. The pilot (P0 plus Ch1, no Grumbleguts) sits at the soft end: mild fantasy peril, scares that come in the same beat as their joke, and no on-screen violence beyond a hint.
* **Gore** is cartoon "confetti-gibs" only (confetti, hearts, plush stuffing, grape goo). The game's BRUTAL setting (blood, gibs, red showers) and Monster Mode (killing citizens) are **excluded**.
* **Grumbleguts.** His butt, belly and feast humour stay at innuendo and slapstick and are never explicit. Use the story_final §3 "hard rules" version (see §8 below).
* **Excluded from the show as game-only material:**
  * the S186 Feast **outcome cutscene** ("What an Ass": the inflation, the pop and death, and every staging note for it in `GRUMBLEGUTS_AND_DIRECTOR_NOTES.md`);
  * the "adult humour / very suggestive / distasteful" passes in `GRUMBLEGUTS_TONE.md`: swearing, crude gag-bank lines, body-language escalation, gross-out close-ups.
* **Kids and teens are fully out of romance and innuendo, and out of any scene that carries it.** That covers Pip, Wisp, Bolt, Tantrum, Pokefork, Wobbles, Miss Lulla's daycare kids and the "edgy teens". With kids, Grumbleguts is only ever the grumpy uncle ("Go home. Eat something."). Kid flashbacks (F4: little Giggs and Nox) are **friendship only**. Romance appears only between adults and stays tender and PG: the Mayor and Nox's "Hold my hand?", and kindly rejections of Grumbleguts.
* **Vaping.** Cloud Nine Vapor is the villain's lure. Keep it clearly negative and unglamorous (the payoff: "plain-water vapes... they're terrible"), with no real-looking products. It is not in the pilot's A-plot. It is planted only as a "COMING SOON" sign in the shipped Ch1.
* **Horror ceiling.** Hold scenes, red eyes in the fog, a 3-4-frame insert, stillness. No cute character is ever harmed on screen.

---

## 7. Cast

Each entry gives species, pronoun, voice, personality, relationships and arc. "Voice" combines the roster's voice sliders (0-31) with the bibles' voice notes.

### 7a. On screen in the prologue and Chapter 1

* **Sunny the bunnykin (@P), the player and protagonist.**
  * Wants a place with room for them; wound: arrived with one suitcase they never unpack.
  * Silent in the game: the name pill appears only three times. Those lines are "@E!" at the closed gate (Ep 3.7), the answer to Wisp (Ep 4.4) and the "Say It" line (Ep 4.6).
  * Grampy calls them "Sunny... Sonny... Bunny... Toaster".
  * Arc: make a home, the home turns on them, defend it, it empties, open it. In the epilogue the suitcase is unpacked: it holds an empty photo frame, and the festival polaroid goes in it.
  * **Pronouns are not set in canon** (it is a player avatar), so the show must choose.
* **The Doomogachi (@E), "it".** Hatches in Ep 1.3; the player names it.
  * Wants to be near you. Says only "beep." (a tiny bubble at first), with "bep?" as a variant.
  * Arc: horn nubs (Interlude I), a horn in its sleep (Ch2), it eats the Mayor's fear and runs over the Wall (Ep 3.7), it is found and turns its back on the fear (Ep 4.5). It keeps the horns forever. Post-credits, its poop grows horns.
* **Mayor Giggs ("Gigglesworth"; id 0), he.**
  * Look: cherub with angel wings (he "flaps over"), halo, top hat, pompadour, handlebar moustache, bow tie.
  * Voice: sing-song (intonation 29), maximally expressive.
  * Tic: nervous "Hee. Hee." Wants everyone happy. Roster catchphrase: "Oh my stars!"; fear: public speaking.
  * Wound: lost a cloud at nine, closed the park at thirty, plumbed the fountain at thirty-one.
  * Arc: joke, then denial, then confession, then "I had a cloud once", then he crosses the gate. Payoffs: "I LOVE soup." / "...Hi, Fluffy." / his first real laugh / "Hold my hand?" / "Population: 1,001."
  * In Ch1 he points at everything except the Wall, steps in front of the camera, hums the Mingle song and stops when he notices, and toasts "To fluffy— ...cupcakes."
* **Nana Puddin (1), she.** Sheepkin head baker in a chef's hat.
  * Voice: low, slow, warm. Calls you "sweet pea". Catchphrase: "Sweet as pie!"; fear: being alone.
  * Bakes **two of everything** (habit) and **bakes clues into food**: a key in a muffin (Ch1), a map in a pie, a relic in a bundt, a key in the Unity Loaf.
  * Her Hellvile pen pal Charlotte trades cookies through a loose brick in the Wall every Thursday (S199).
  * She kept the Hellvile egg. Ch1 lines: "The last one hatched into a little demon! Ha! Kidding. ...Mostly." and, a day later, "He's teething." She suspects the Mayor's secret: she holds a look on him for exactly R8.
* **Pip (42), they.** A tiny bunnykin kid (KID: no romance, ever).
  * Voice: highest and fastest in the cast.
  * Lines: "My eyes are just doing a rain." / "Bun-believable!"
  * Wants to be brave and thinks brave means not crying. Their bunny plush is **Mr. Flops** (one floppy ear, one not-floppy, a stitched smile).
  * Keeps Wisp's paper-boat drawing "safe like Mr. Flops" and becomes Wisp's pen pal by paper boat.
  * Arc: lost bunny, night patrol, Mr. Flops kidnapped, then lends Mr. Flops to Wisp. Payoff: "Rain's good for stuff."
* **Mascotty, "the Starer" (106), she.** The town mascot in a giant bunny head ("Nobody knows who").
  * Perfectly still in a crowd that bobs. Waves, very slowly, only if you stand still. Thumps 3-then-1 back at the Wall.
  * Secretly a Hellvile bunny trapped in Hillvile's mascot suit for ten years. Never speaks until late (S136); never takes the head off until the rubble (Ep 4.6). **Thumpkin** (he) is her brother.
  * Running line: "Ignore that one."
* **Steamy (2), she.** Frogling bath-house keeper.
  * Voice: lilting accent. Catchphrase: "Cozy vibes only!"
  * Wants everyone warm. Every tub has a duck; one unknown duck faces the Wall (it becomes "Guest").
  * The first possession (Ch2), true name TALLYWORN.
* **Hattie Haha (71), she.** Plushie comedian.
  * Voice: slow and precise. Catchphrase: "Yikes."
  * Her act is not talking about things. Her open mic is the timing minigame. Her closer: "So I said, 'Mayor, what's on the other side?' He said, 'Hee. Hee.'"
  * Shipped line: "Knock knock. Who's there? See, that's the trick. Nobody ever says."
  * Possessed in Ch3 (HAZE). She delivers the theme line's third use.
* **Grampy Bean (88), he.** Human elder; grows the town's tomatoes "and the town's beard".
  * Voice: very slow. Catchphrase: "Big mood." / "...where was I?"
  * Forgets names. His seeds "came over the fence years back. Never asked who threw 'em." The thrower was Sootkin.
  * Possessed in Ch3 (SOOTKIN).
* **Kevin, he.** A creator preset with three eyes: "Yes, three eyes." Only the third eye blinks.
  * Delivers a bust of himself to the housewarming and leaves before you can ask.
  * Shipped line: "Nobody ever asks about the third one. It's lonely."
* **Bonejangles (5), he.** Skeleton xylophone teacher who plays his own ribs; hugging him plays a chord. "Clack clack!"
* **Miss Lulla (36), she.** Daycare sheepkin who can nap any baby in under a minute.
  * Voice: low and soothing. Catchphrase: "Hug o'clock!"
  * Arc: tells kids monsters aren't real, hears giggles from the Wall, ends with "Monsters are real. They also nap."
  * Her kids yell "BEEP!" at the pet. In Ep 1.5 they all drew the same crowned purple cloud.
* **Father Fuzz (77), he.** Plushie bear priest of St. Cuddles, "stuffed with love and a little bit of buckwheat".
  * Voice: high and quick. Catchphrase: "Hug o'clock!"
  * Believes "nothing is unhuggable". Wound: his first exorcism, Lady Smog eleven years ago, failed.
  * In Ch1 he only appears at the housewarming ("A cushion. Cuddle-proofed. I tested it. Extensively."). Mr. Huggins the 1st's grave is in the graveyard: "Rest in pieces".
  * Arc: the Huggins dummies keep dying (a runner); in Ch3 a hymnal works where a hug didn't ("What am I FOR?"). Payoff: "The hug was never for the demon. It was for the one holding the demon."
* **Dr. Bloom (111), she.** Ghoul doctor in pastel scrubs.
  * Voice: high and soft. Catchphrase: "Bless your heart!" / "Sorry. Ghoul."
  * Hellvile-born, trapped on this side when the Wall went up, hiding it; keeps cold hands warm in water.
  * At the housewarming: "A thermometer. Decorative. ..Mostly decorative."
  * Runs the Ch2 investigation and is the first to say it out loud (Ep 3.4).
* **Rivet (13), she.** Adult imp welder who fled Hellvile, glasses and bandana.
  * Voice: blunt and fast. Catchphrases: "Rip and tear... gently." / "Don't make it weird." (x3, escalating).
  * **Unseen in Ch1**: a welded coat hook left on the step with the note "don't make it weird".
  * Ch2: the weapon, and the DAYS SINCE LAST FIRE sign, which flips 7, then "-", then 0. Payoff: "It's a REALLY good spoon."
  * Family across the Wall: Anvil (mum, she), Sprocket (he) and Bolt (xe, KID).

### 7b. Seeded offscreen in P0 and Ch1

* **Young Giggs and young Nox**: tables and bowls only, no faces needed.
* **Fluffy**: the cloud on a string in P0, and the grave.
* **Charlotte** (566, she, Hellvile charcoal baker): the burnt cookies in P0.
* **Wisp** (563, they, KID imp): the crayon paper boats ("ARE YOU THERE", with a candle), the purple glow that blinks back, and the chair on the Ch2 haunting.
* **Thumpkin**: the 3-then-1 thumps.
* **Vapula/Fog**: red eyes, "come and PLAY with us", and the crowned-cloud drawings.
* **Lady Smog**: the 4-frame kids'-tent insert.
* **Cloud Nine**: the "COMING SOON" sign (shipped script).
* **Grumbleguts**: only the gate, unseen and unheard.

### 7c. Introduced later

**Ch2:**
* **Vex** (666, he, foxkin): smile 2 px too wide, held R8 too long. "First puff's free... that's the business model." Later reformed: "plain water vapes. They're terrible."
* **Haze** (709, she): waters the plants. "The plants hate it too."
* **Sprinklepop** (81, she): sneezes glitter; a scripted SOUL LOST, later "UNFAILED".
* **Grumbleguts and Vapula**: Ep 2.7.

**Ch3:** Wisp becomes visible.

**Ch4:**
* **Mother Nox** (778, she, imp high priestess): deadpan, sets two bowls every Mingle. "You're late." "I let you."
* **Tallyworn** (712, he): "3,652 days". "Habit." The last mark is a heart.
* **Lady Smog** (502, she): "It's DELICIOUS."
* **Sootkin** (525, xe): the sunflower under the gate.
* **Signhere** (567): the 900-page contract.
* **Anvil and family.**

### 7d. Key relationships

* Giggs and Nox (childhood friends, then estranged co-signers).
* Giggs and Fluffy.
* Nana and Charlotte.
* Pip and Mr. Flops, and Pip and Wisp.
* Rivet and Anvil, Sprocket and Bolt.
* Fuzz and Lady Smog, and Fuzz and Mr. Huggins.
* Mascotty and Thumpkin (siblings).
* Grampy and Sootkin; Steamy and Tallyworn; Hattie and Haze.
* Bloom and Graveley (sibling).
* Vex and Vapula ("the only one who ever listened").
* Grumbleguts and the Gate.

---

## 8. Gatekeeper Grumbleguts as written (show-safe)

* **Status.** Not in Ch1. He debuts in Ep 2.7, "The Gate Cracks", as a 25-second insert, and his big chapter is Ch4.
* **Role.** Keeper of the Sealed Gate ("Also of a very large seat."). He has waited ten years for a promised feast. Vapula leans on him to crack the gate, which he does with a belly-flop.
* **Look.** Glossy red pot-bellied imp devil with stubby horns, fangs, a huge grin, bat wings and a spiked tail. Cartoon hell costume: spiked collar, harness, studded belt, the gate key on a chain. The belly and the smooth cartoon rear are always on show. He **bounces** (`TW_BOING`).
* **Voice.** Two speakers share his box:
  * **GRUMBLEGUTS** (he): a warm, oily baritone, roster pitch 6;
  * **GURGLE** (it), his belly: bass babble that is always hungry, always honest, and sides with the player about half the time.
* **Personality.** Hell's biggest flirt **with adults only**: winks, pet names ("sugar", "padre", "new kid"), "Come up and see my gate sometime. This is my gate." Everyone kindly turns him down; running gag: nobody is interested, except possibly the Gate. Every rear reveal gets a record scratch.
* **Gag ladder.**
  * Mid-meal on Brimstone Bagels: "I only had a light snack—" as a forklift beeps in with the next course.
  * Sucks his gut in `[HOLD 13]`; it springs out `{r}`.
  * Squeezes through the gate rear-first, gets stuck, and pops through with a BOING.
  * "Evening, sugar. Nice buns." NANA: "They're MUFFINS." "I'll allow it."
  * To kids: "Go home. Eat something. {d}Not THAT."
  * Fuzz: `{d}No.` "Playing hard to get. I respect it."
  * "Mayor. Love the hat." "It's my head." "I know."
* **Fight.** The belly is the weak point. Feeding him (a thrown dish) stuns him while he rates it ("Six. Needs salt."); three dishes and no hits wins pacifist. Defeated, he sits: "...Is there dessert?"
* **Heart.** "Kept this gate ten years. Nobody came up to see it." He becomes a line cook at Brimstone Bites ("You'd have to wear a shirt." "...A TIGHT shirt?"), via chain T "Feed the Gate".
* **Edgy antihero streak (tone note).** He drops the act to save his friends, then claims he "totally didn't do that for you".
* **Hard rules.** Innuendo only; no acts, genitals or nudity beyond cartoon cheeks; kids are off limits; consensual and played for laughs; the joke is his vanity and gluttony, never the target; never mean.
* **Show exclusions:** the S186 outcome, the adult-humour passes and swearing (see §6).

---

## 9. Running gags (Ch1 form, then how they mutate)

* **"beep."**
  * Ch1: the smallest bubble in the game, at the hatch.
  * Then: with horns, beeping at nothing; mid-invasion, so that demons flinch and the fog beeps back lower; far across the Wall; post-credits, the poop grows horns.
* **"...it's fine."**
  * Ch1: a deadpan caption after the first beep. Nana also says it when the oven smokes.
  * Then: Sprinklepop, possessed; "IT'S FINE" painted on the church barricade; the Mayor about crossing the gate; finally the title logo on fire, "(it's fine.)".
* **"Hee. Hee."**
  * Ch1: the sound of a man not looking at something.
  * Then: the fountain giggles it late; the fog says it in his voice; "...Hi, Fluffy." and his first real laugh.
* **Mr. Huggins.**
  * Ch1: the 1st's grave, "Rest in pieces".
  * Then: the 2nd falls apart in practice (head rolls three times and stops facing the Wall); the 3rd-6th; the 9th survives and hugs Fuzz.
* **Nana bakes it into food.** Ch1: the key in the muffin.
* **Mascotty the Starer.** "Ignore that one." At your window. In Ch4 the head comes off.
* **The crowned cloud.** Ch1: the chapter card's last sparkle is purple and sinks, and the daycare drawings show it. Then "Employee of the Month", then it is Vapula.
* **Paper boats.** Ch1: Wisp's crayon house and the candle. Ch4: "That's MY house. You KEPT it?"
* **"My eyes are just doing a rain."** Pip, then Steamy, the Mayor, Tallyworn.
* **Mailbox.** Junk mail "from The Other Side" (S121).
* **DAYS SINCE LAST FIRE.** From Ch2.
* **Grumbleguts eating, and the record scratch on the rear.** From Ch2.

---

## 10. Comic timing: "spastic, elastic, deadpan" (comic_timing.md)

The timing is reverse-engineered from the 78-second trailer, which runs about one gag every 1.7 s.

1. **Entrances take 4-6 frames and overshoot.** POP grows from 0.15-0.4x up to 1.12-1.35x. SLAM lands from 1.6-2.4x. DROP squashes to 1.22-1.38 wide. Squash anchors at the feet. Use springs that start from rest, not back-ease.
2. **The beat ladder (Fibonacci).**

   | Rung | Name | 30 fps frames | ms | About at 24 fps |
   |---|---|---|---|---|
   | R2 | BLINK | 2 | 67 | 2 f |
   | R3 | REACT | 3 | 100 | 2-3 f |
   | R5 | BEAT | 5 | 167 | 4 f |
   | R8 | TAKE | 8 | 267 | 6-7 f |
   | R13 | HOLD | 13 | 433 | 10 f |
   | R21 | PAUSE | 21 | 700 | 17 f |
   | R26 | DEADPAN | 26 | 870 | 21 f |

   An instant reaction lands at R2-R3, a dawning one at R5-R8, a deadpan reply at R18-R26.
3. **Silence is an instrument.** About 23% of the trailer has no music; aim for 20-25% inside story scenes. A mute keeps the tempo and the music re-enters on the grid. A record scratch is a tape-stop (lead down 15 semitones over 5 frames), then silence (14-64 frames), then a **small** sting first ("beep.", a tink).
4. **Stillness is the scariest and funniest thing in a world that never stops moving.** Freezing an actor's idle clock is the joke (or the scare).
5. **Whiplash inserts last 3-4 frames.** The insert owns the audio for its length, then cuts back to the same shot with exactly **one** persistent change. A small sound follows 2 frames later, and the deadpan line comes at R8.
6. **Rule of three.** Constant interval (15-16 frames), escalating magnitude in every channel at once, pitch stepping down, then a topper R13 later. Variants: list-and-break, accelerating three, escalating to the personal.
7. **Deadpan** means no bounce, no music and no idle motion; the text just appears. **Setup types, the punch pops, deadpan appears.** Allow at most one deadpan per conversation.
8. **Every joke gets a button** 12-25 frames after the punch: a callback smaller than the punch, often inside the 3-5-frame hold of an iris-out (heart iris for cute, circle for creepy).
9. **Crowds ripple, never switch.** Stagger crowd reactions by 0-3 frames.
10. **Shake has a profile** (TAP, POW, STAMP, FWOOMP, ROAR, RUMBLE). Every cut has a sound on frame 0.
11. **Scary is comedy timing with the release removed. Heartfelt is the same with the button withheld:** hold 3-5 s with no gag. Undercut at most half the sincere beats, never two in a row, and **never undercut the final story beat**. Put that button after the credits.
12. **Density.**
    * Cutscene: 1 gag per 1.5-3 s.
    * Dialogue: 1 punchline per 3-6 lines.
    * Ambient world: 1 micro-gag per 20-40 s.
    * After a big beat, leave 5-15 s with no gag.

    Running gags: establish small, repeat soon, change the context not the words, break the frame last, and send one-shot gags back for the ending.

---

## 11. Prologue P0, "The Egg Crossing" (~2 min crayon cutscene before the title)

* **Style.** Children's-book crayon. No portraits; captions in a child's hand.
* **Music.** The Mingle/festival song as a slow music-box arrangement (`SONG_MINGLE` in the shipped script).
* **Purpose.** Plant the Mingle, the cloud on a string, Nana's boats and the egg.

| Time | Beat |
|---|---|
| 0:00 | Circle iris in on the bandstand. Two tables: a cupcake tower and a soup cauldron. Caption: "Every year, the park had a potluck." |
| ~0:03 | Kids run across the bridge both ways (a ripple). A small cherub holds a cloud on a string. "Everybody came." |
| ~0:06 | Fireworks. The cloud tugs and looks up, in love with the fireworks ("It loved fireworks."). R8 later the string slips. HOLD 13: "The string slipped. Nobody noticed." |
| ~0:08 | Pan south with the cloud as the fireworks fade. The music mutes for 29 frames. "Not everybody went home." |
| ~0:10 | SMASH (thud) to the same bandstand eleven years ago, with a Wall through it (rumble). Two tables, one on each side. Two bowls on Nox's, one cupcake on Giggs's. Deadpan caption *appears*: "Nobody talks about why." |
| ~0:13 | Covered cut through fog. Nana, a sheepkin, kneels at the river and pushes a paper boat from north to south; inside is an egg. "Except Nana. Nana sends eggs." R13 later a boat comes back the other way carrying burnt cookies. Button +21: she eats one, looks sick, then in love. |
| ~0:16 | Heart iris out on the egg. In the hold the egg wobbles once with a tiny crack sound, into the **title card** "DOOMOGACHI LIFE in HELLvile Park!" |

This is the natural cold open into the main title ("Doomtastic Times").

---

## 12. Chapter 1, "Welcome to HaHaland" (2.5 h of game, days 1-4)

* **Purpose.** Make the audience love this town and this house before anything goes wrong. Horror stays in the background only.
* **Music.** The HaHaland day and night themes. Scare-adjacent beats duck the music. No OMINOUS cue until Ep 1.6.
* **Chapter card.** Pink polka dots, "Chapter 1" pops in, the title follows 18 frames later. The last sparkle is purple and sinks.

### Ep 1.1 "Population: 500": the first laugh, the first wrongness, the refrain, the grave

1. The pink bus drives over candy hills at constant speed and stops with a record-scratch brake. The player is thrown off the step; the suitcase lands 8 frames later and bounces.
2. The sign reads "HaHaland! Population: 500 lovely citizens." It updates to "...and now, @P!" (501). R8 later the "1" falls off (*tink*). The shipped version: "Someone has added, in pencil: and @P." A pink ticket flutters onto your head.
3. Fountain plaza. The Mayor flaps over, and the crowd waves in a ripple of hearts. Mascotty does not.
4. The Mayor points at everything except the Wall ("The bakery! The fountain! The flowers! The lovely, lovely sky!"). The camera creeps toward the Wall anyway at 0.04 px per frame, and he steps in front of the lens.
5. Key lines:
   * "Welcome, welcome! Mayor Gigglesworth. Friends call me Giggs. Enemies don't exist!"
   * "Ooh, a new face! We get so few. We had a whole fence of them once. ..Not a fence OF faces. Faces AT a fence. Hee. Hee."
   * "Try the Brimstone Bites! Named for a joke. It's ironic!"
   * "Hellvile Park has two sides. This one, and..." [FREEZE, HOLD 13] "...the weather side."
   * `{d}`"We don't talk about the other side." [HOLD 13; the fountain giggles late]
   * "Your key is with Nana! Follow the smell of nutmeg!"
6. The player's menu has no name pill:
   * "Nice to meet you!" gets "Manners! We'll keep you."
   * "What's on the other side?" stops everything: freeze, tape-stop, and every citizen turns to look at you (HOLD 21). The music comes back on the downbeat with "Hee. Hee."
   * "Why is that mascot staring?" gets "Ignore that one.", and the button is Mascotty waving, very slowly.
7. Optional graveyard shortcut:
   * A row of joke graves ("Rest in pieces, Mr. Huggins the 1st").
   * A small stone in a child's hand: "FLUFFY THE CLOUD. DRIFTED." Someone has left a cupcake on it.
   * If the Mayor is following, his face goes SAD for exactly R8, then HAPPY. Shipped line: "Behind you, a pair of wings stops flapping. ..Then starts again."
8. Shipped props: the Wall ("Something on the other side giggles. Nobody on this side looks at it."); the gate (seven padlocks, KEEP OUT / FOREVER); Cloud Nine Vapor "COMING SOON" with a purple flicker.

### Ep 1.2 "The Key in the Muffin": Nana, the egg, the first room

1. Nana's bakery. Every tray has two of everything.
   * "You're the new one! You look hungry. Everyone looks hungry to me."
   * "The key's in a muffin. Don't ask which. I forgot. I always forget."
2. Bite test, rule of three: raisin, raisin, "that's a button". The fourth crunches on the key. A 3-frame x-ray insert shows **the key has tiny horns**.
3. The egg in a tea towel. "..And this. An egg. It's been waiting for someone." She **hesitates** for R13 before letting go. "The last one hatched into a little demon! Ha! Kidding." [R8] "Mostly."
4. The cottage: heart iris in, the door wobbles like jelly. It is empty except for a crate and a window.
5. Decorating tutorial: the bed drops in with a THUD and BOING; the window gets a "nope" shake. The rating stamp reads "COSY: B+", followed by three imaginary visitor bubbles: "♥", "♥♥", "...is that a crate?" The button: the crate slides 1 px "in shame".
6. Nana at the door: "You put a lamp by the egg. That's exactly where my mother put hers."
7. Night. Through the window, a faint purple glow at the Wall pulses R21 apart. Sleep. No button.

### Ep 1.3 "Hatch Day": the pet, "beep.", Pip, the first paper boat

1. The egg wobbles with a 6-frame period, then 3, and hatches **on the musical downbeat**: white flash, two glossy eyes pop open.
2. Back home the pet hops to the lamp and sits in the light. HOLD 21, music mute 29, then **"beep."**, the smallest bubble in the game. R8 later the caption appears: `{d}`"...it's fine." Then the naming, and a 1-frame insert of a pentagram on the LCD.
3. Pip at the fountain, crying with a sweat emote x3:
   * "I lost Mr. Flops. He's a bunny. I'm a bunny. We match."
   * "My eyes are just doing a rain. It's fine."
   * "The sea goes all the way to the Wall."
4. The beach. Mr. Flops bobs in the surf, **face down, staring at the Wall**. Turn him over: the stitched smile, and a "FOUND!" sticker slams in.
5. A second shape in the surf: **a paper boat**. Inside is a crayon drawing of a house with a candle in the window, a small imp and a bigger imp, and "ARE YOU THERE" in backwards letters. HOLD and mute for 18 frames. The pet sits on it, in love.
6. Pip:
   * "MR. FLOPS! You're damp! Were you scared?" [R8] `{d}`"...He says he saw the Wall breathe."
   * "That's not my drawing. That's somebody's HOUSE. [R13] I'll keep it safe. Like Mr. Flops."
   * Pip gives you a skull sticker ("Skulls are cute.") and the advice "make friends with EVERYONE". No button.

### Ep 1.4 "Three Neighbours": meet the future possessed so it hurts later (any order)

* **Steamy.** Filling the tubs: "That's Duckworth. That's Quackers. That's..." [SHOCK] "I don't know that one." The unknown duck squeaks by itself, facing the Wall. Button: Steamy squeaks a duck back, nervously. "Everybody needs to be warm."
* **Hattie's open mic** (the timing minigame: perfect presses get a snort-laugh ripple, late ones a cricket). "Tough crowd. You ARE the crowd." Her closer gets a big laugh. Button: Mascotty in the back row doesn't laugh.
* **Grampy's tomatoes.** Five picks, five names ("Sunny, Sonny, Bunny, Toaster"). "These seeds came over the fence years back. Never asked who threw 'em." [SLEEPY] "...where was I?"
* **Two-beat gags:**
  * Kevin: "Yes, three eyes." [R5] Only the third eye blinks.
  * Bonejangles: hugging him plays a chord.
  * Miss Lulla's eleven kids shout "BEEP!", and the pet answers, very quietly, "beep."
* Nana texts: "You've made friends! Come get a cupcake to share. xo"

### Ep 1.5 "Housewarming": the house becomes yours, the toast slips, the first clear scare at the window

1. The cupcake goes to Miss Lulla ("I'm crying. Thank you."). She says "the fog giggles at night" and shows eleven drawings, every one with a crowned purple cloud: "They all drew the same friend. I don't know who he is." Nana: "And be home at six! ..No reason! Shoo!"
2. "SURPRISE!" Nine guests drop in 8 frames apart, each with a gift tagged "gift from X":
   * Nana, a rug: "Every home needs one thing that's been walked on with love."
   * Pip, a drawing: "I drew you a bee! It's you."
   * Fuzz, a cuddle-proofed cushion.
   * Kevin, a bust of himself.
   * Bonejangles, a xylophone end-table.
   * Lulla, a nightlight: "For you, not the egg."
   * The Mayor, a framed photo of himself: "For when you miss me. Which will be constantly!"
   * Bloom, a thermometer: "Decorative. Mostly."
   * Rivet, unseen: a welded coat hook on the step with the note "don't make it weird".
3. Live placement: every gift you place gets a crowd ripple, three reactions 15 frames apart and a topper (the pet climbs onto Kevin's bust "like it was always the plan"). The loft bedroom unlocks.
4. The toast. "To @P! To new friends! To fluffy—" [FREEZE 13, MUTE 13] "—cupcakes. Fluffy cupcakes." Nana looks at him for exactly R8.
5. The goodbye ripple. The door closes. R21. Mascotty stands perfectly still at the window. Mute 41. Caption: `{d}`"Ignore that one." R21 later she waves, slowly. Heart iris out on the window.

### Ep 1.6 "The Giggling Fog": the first real scare (night)

1. Woken by thumping from the Wall: 3, then 1. A purple tint creeps in, the music ducks, and the crickets stop one at a time.
2. Menu: Peek, or Back away (the shipped script: "The fog takes one step after you."). The fog smells of grape candy.
3. The fog cutscene:
   * Two red eyes pop, then more pairs at R8, R5 and R3 (an accelerating three plus one).
   * Mute 40; all the eyes go still.
   * The caption `{w}`"come and PLAY with us" types; the second "us" is a child's voice.
   * A 4-frame insert: a Hillvile kids' tent eleven years ago, a cloud-shaped lady with glowing eyes, and behind her a crowned silhouette trailing a string. This is the first flash of F2.
   * SMASH to the Mayor's panting face filling the frame.
4. The Mayor:
   * "NOPE. Nope! Home! It's bedtime! It's always bedtime!"
   * "It's just.. weather. Hellvile weather."
   * Walking you home, he hums the Mingle song and stops when he notices. `{d}`"Please don't go near there again."
5. Home. The pet stares at the Wall from the window. Turn it round; R13 later it is facing the Wall again. "beep."
6. Under the door: a **second paper boat** with Pip's note: "ANOTHER ONE CAME. IT SAYS HELLO. I DIDN'T OPEN IT. (I opened it.)" Inside are an unlit candle and the same house, now with a light in the window. Place the candle and light it.
7. Heart iris out on the window. In the hold, the far purple glow **blinks once in reply**. Day 4, then Chapter 2.

### Interlude I "Night Shift" (2 min tag)

Feed the pet a cupcake: it eats, strains for 8 frames, plop. A 4-frame insert: the LCD fills the screen with a pentagram, then a demon face, with the music muted for exactly 4 frames. Back to the same shot with one change: **two horn nubs**. "beep." [R8] `{d}`"...it's fine." Next day, Nana: "Ah. .. He's teething." She knows.

### Ch1 wrongness catalogue

Each wrongness is noticed once and never explained until Ch3.

| # | Wrongness |
|---|---|
| W1 | The fountain giggles R8 (267 ms) late. Stop, and R5 later it giggles again on time: "...Hee." |
| W2 | 1000 is scratched out under the 500 on the sign. |
| W3 | Flowers lean toward the Wall; pick one and it re-leans in your hand. |
| W4 | Mascotty is still in a bobbing crowd. |
| W5 | A lamp post faces the Wall. The next time, it faces you. |
| W6 | Thumps at night, 3 then 1. |
| W16 | Fluffy's grave. |

W12 (the crowned-cloud drawings) is already shown in Ep 1.5.

### Ch1 side missions (B-plot or tag material)

* **S100 "A Bed for Mr. Flops"** (Pip). Pip inspects it three times, rule of three: "Too close to the door / window / ...floor." Heart: "He sleeps better when someone else is in the room. ..So do I."
* **S107 "Speech Notes"** (Mayor). His whole speech is one sentence: "Welcome to HaHaland, where... hee. hee." The last page reads "You never asked where it went."
* **S110 "The Secret Ingredient"** (Nana). The missing ingredient is bone flour. "Charlotte and I used to bake together."
* **S121 "Return to Sender"** (Lilypad Lou). Mail addressed to "THE SNACK, Pink Door Cottage."
* **S124 "Where Was I?"** (Grampy). The pet finds his glasses (he puts them on over his other glasses: "Better.") and a Hellvile seed packet.
* **S127 "Every Duck Has a Name"** (Steamy). The duck "Guest" squeaks back at the Wall 3-then-1.
* **S130 "Party Supplies"** (Sprinklepop). The third sneeze is purple and R8 late.
* **S134 "Ignore That One"** (Hugsley). Mascotty thumps back.
* **S142 "The Fountain Is Late"** (Prof. Fizz). "267 milliseconds late... So who's laughing?"
* **S144** Gerald the fish. **S145** "The Very Hungry Demon" bedtime story.
* **S151 "Cold Hands"** (Bloom). "...I'm a ghoul."
* **S154 "Open Mic"** (Hattie). Your third punchline is "...Hee. Hee."
* **S157 "Naptime Monsters"** (Lulla).
* **S196 "Flowers for a Cloud"** (Mayor). "I'm inspecting the... gravel."
* **S199 "The Loose Brick"** (Nana, at night). A sooty hand takes the muffin. "Every Thursday. She never missed one. ..Neither did I."
* **S208** Hauntessa, the ghost housekeeper.

---

## 13. The dream house (series spine)

The house changes phase every chapter:

| Phase | When | What it means |
|---|---|---|
| COSY | Ch1 | The tutorial. The egg hatches only in a cosy room. |
| HAUNTED | Ch2 | Wisp moves the chair; the chair ends up tucked into your bed, breathing. |
| BARRICADED | Ch3 | Welded plates and a toaster turret; the siege. |
| EMPTY | Interlude III | Decorate can only remove. Seven visitors knock, and the music comes back one instrument per visitor. |
| OPEN | Ch4 | The Hellvile Annex is welded on. |
| DREAM | Post-game | The Park Plot. |

Gifts stay tagged "gift from X" forever. Gag furniture: Kevin's bust (the third eye blinks), the Plain Human figurine (never animates), the DAYS SINCE LAST FIRE sign, the horned-poop plushie.

---

## 14. Music canon useful for the show

* **Game songs.** Title, HaHaland Day/Night, Hellvile, Church, Pet, Exorcism, Workshop, Invasion (metal), Hell Breach, Boss, Sad, Festival (the Mingle song, music-box in P0), Credits, Vape Lounge, Ominous.
* **The key musical secret.** **HAHALAND_DAY and HELLVILE are the same tune in major and minor.** They resolve into one chord when the Wall falls, which makes them a natural leitmotif or quote for "Doomtastic Times".
* **Two more tunes.** The Mayor hums the Mingle song and stops himself. "Lullaby for the Wall" (S148) is the tune Wisp's mum hums and Steamy hums wrong.
* **Placement.** P0 ends on the title card, which is the hand-off to the ~45-55 s main title. The TV-size theme (~1:30) suits finales and specials.
* **Eyecatch seeds (from canon gags).** The tiny "beep." bubble, "...it's fine.", the sign flipping 7 to "-" to 0, Mascotty's slow wave in an iris hold, the 4-frame LCD pentagram flash, and the Plain Human figurine.

---

## 15. Series-arc seeds

* **Structure.**
  * The game's 27 episodes and 8 specials map naturally onto a ~26-episode season, or onto three seasons: Ch1-2, Ch3, Ch4.
  * Chapter-ending cliffhangers: the glow blinking back (Ch1); the gate cracking, Grumbleguts' debut and "THE DEMONS OF HELL ARE HERE." (Ch2); the pet running over the Wall and the dark night (Ch3); the Wall falling (Ch4).
* **Big turns to place.** "The Bottle" (the demons are people, Ep 2.5) is a mid-season gut punch. The plumbing reveal (Ep 3.4) is the series midpoint. "Hi, Fluffy" is the finale.
* **Pilot mysteries.** All planted in P0 and Ch1, and all have canon payoffs: the late giggle, the 1000 on the sign, Fluffy's grave, the paper boats and candle (Wisp), the 3-then-1 thumps (Thumpkin and Mascotty), the egg's horned key and the LCD pentagram, the "fluffy— cupcakes" slip, the hummed Mingle song, the crowned-cloud drawings, Cloud Nine "COMING SOON", the gate "KEEP OUT. FOREVER.", Nana's two of everything, and Rivet's unseen coat hook.
* **Character engines.**
  * The Mayor's "Hee. Hee." as a wound.
  * Fuzz's faith, with the Mr. Huggins succession running the whole show (the 9th survives).
  * Rivet's family; Nana and Charlotte (Thursday cookies).
  * Bloom's secret; Pip and Wisp as pen pals; Mascotty and Thumpkin.
  * Vex's redemption.
  * The pet's horns.
  * Grumbleguts' show-safe chains: Feed the Gate (S183-S185), the little black book (S172), the profile picture (S173).
* **Episodic formats.**
  * Exorcism-of-the-week, built around true-name clue props.
  * Side missions as A/B plots, each with a "Later" callback button in the tag or a later episode.
  * Sepia flashback cold opens (F1-F4).
  * LCD-style dream episodes (D1 "Teeth Like Muffins", D2 "The Ninth Cloud").
  * Crayon history inserts.
* **Season after the Wall ("Park Life").** Population: 1000, the Two-Door Clinic, Mixed Bathing kept wholesome, the Unity Loaf, the first Mingle in eleven years, and Vapula as a plush cloud in a jar who rains on sunflowers.

---

## 16. Continuity flags and open decisions

1. **Bolt's pronoun.** The roster file says THEY; the canon rule and story_final say **xe**. Use xe.
2. **Squeaker's pronoun.** The roster says she, but story_final Ep 3.1 says "he honks". Use she (not pilot-relevant).
3. **Grumbleguts details that conflict:**
   * his crush: story_final says **the Gate**, while GRUMBLEGUTS_ROMANCE.md says "Gatekeep the robot";
   * his exes: S174 lists Fangela, Cloudzilla and Kodiak, while the romance doc lists Shriekra and Spearmint;
   * his catchphrase: the roster's "Sulfur kisses!" is shared with Vex, while the tone notes give "Open late, darlin'."

   story_final wins on the crush and the exes. The show must pick one catchphrase.
4. **Superseded heart-bible variants.** Do not use: both mayors signing the Wall (story_final has Giggs alone say "Close the park"), Wisp's Ma Gloom backstory, Hattie and Grampy possessed by Honkers and Marrowmere, young Giggs as a bunnykin (he is a cherub), and "Potluck of the Park" (the canon name is the Midsummer Mingle). Fluffy's stone has **no year** on it.
5. **Rivet's age math is loose.** She held the torch at 6, twelve years ago; welded the lock at 10; ran at 14; yet the letters and the place at dinner span "ten years". She is an adult (no KID or TEEN trait). The show should avoid exact numbers.
6. **Cloud Nine's first appearance.** The shipped Ch1 has the "COMING SOON" sign, while story_final has the shop appear in Ep 2.1. Both are compatible.
7. **Sunny's pronouns** are not set in canon (player avatar). The show must decide.
8. **The trailer's starer** is a "plain human". In story canon the starer is **Mascotty**. The Plain Human is a separate figurine gag.
