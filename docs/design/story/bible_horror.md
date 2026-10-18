# STORY BIBLE (horror-comedy-first): "Don't Talk About the Other Side"

DOOMOGACHI LIFE in HELLvile Park! is the same story on both the GBA and the PS1. The cast and the 4-chapter backbone are the canon ones from `story_map.md`. The timing vocabulary comes from `comic_timing.md`.

> **Tone target:** Shaun of the Dead, Gravity Falls, Undertale and DOOM, sewn into a plush toy.
> The pastel world slowly goes **wrong**: first in the background, then in your house, then in the sky. Every scare is undercut or doubled by a joke. Every running gag comes back at the end as something you feel.
> Violence is cartoon violence against demons. Cute characters are never shown hurt in the cute half of a whiplash cut. Nothing here mocks real people.

Cast pronouns follow `roster_list.txt` (the roster is canon). **Rivet is she, Steamy is she, Pip is they, Sootkin is xe.** Some current story lines say "him" for Steamy; §12a lists that and other continuity fixes. The user's notes in `user_notes.md` (Gatekeeper Grumbleguts) are folded in at §3a, Ep 2.7, Ep 4.5, Ep 4.7, chain T and mission S186.

**Map of this document:** §0-§7 premise, cast, house, gags, wrongness, shape. §8-§12 the main story as beat sheets with sample dialogue. §12a continuity fixes. **§13 the 96 new side missions** (21 chains, 13 of them cross-border, plus 27 stand-alone missions). **§14 gameplay integration** (Breach levels, invasions, exorcisms, house defence, decorating goals, free play). §15 engine budget notes.

---------------------------------------------------------------------------------------------------
## 0. The pitch in one breath

You arrive in HaHaland on a pink bus with one suitcase and nowhere you belong. The town gives you a cottage with a pink door, a muffin with a key in it, and an egg that hatches into a tiny creature that says "beep."

Everyone is lovely. Everyone giggles. Nobody talks about the other side of the Wall. Then the fountain giggles half a second too late.

Over four chapters (about 26 episodes, 12-14 hours) you learn that HaHaland's endless happiness is **plumbing**. Eleven years ago the town learned to swallow every sad and scary feeling and flush it through the Giggling Fountain, under the Wall, into Hellvile. There it became vapor, the vapor became a cloud, and the cloud became **Lord Vapula, Duke of the Ninth Cloud**. Vapula is literally made of every sigh Hillvile never let out. He is not evil so much as **un-felt**.

The demons that possess your friends are breath. Each one is named after the Hellvile pump worker whose lungs it passed through. Shouting a demon's true name at an exorcism is the first time in ten years anyone on this side has said that worker's name.

The cure is not a bigger gun (although there are bigger guns). The cure is the thing the town forgot how to do: **say it out loud**. In the end the Wall comes down because 1000 critter-folk each say the one thing they were scared to say. Then everybody commingles in Hellvile Park.

**Theme line, said three times in the game:** *"We don't talk about the other side."*
1. Ch1: the Mayor says it as a joke ("Hee. Hee.").
2. Ch3: the Mayor says it as a confession ("...That was the whole point. I never asked where it went.").
3. Ch4: Hattie Haha says it as the opening of a stand-up bit at the festival, and both sides laugh.

---------------------------------------------------------------------------------------------------
## 1. Notation used in the beat sheets

Every number is in frames at 30 Hz, using the ladder in `comic_timing.md` §2. Tags compile to `BeatEv` rows (§7.2 of the timing bible) or to dialogue codes (§6.2).

| tag | meaning | tag | meaning |
|---|---|---|---|
| `[R2]` `[R3]` `[R5]` `[R8]` `[R13]` `[R21]` `[R26]` | gap on the beat ladder | `[HOLD n]` | unskippable hold (the joke or scare lives here) |
| `[MUTE n]` | music muted n f, SFX keep playing, the sequencer keeps running | `[TAPESTOP]` | 5 f tape-stop, then mute |
| `[DUCK g n]` | music to gain g for n f | `[SILENCE]` | mute until the next music cue |
| `[FREEZE x]` / `[THAW]` | clamp actor clocks (x = actor or ALL) | `[PANIC x]` | actor clock at 2x |
| `[STILL x]` | idle clock = 0 (deadpan or horror: no breath, no blink) | `[TWITCH x]` | possessed ear twitch at 1.7 rad/f |
| `[INSERT n: what]` | tonal-whiplash insert of n frames (3-4); owns the audio | `[ONE CHANGE: what]` | the single persistent detail after an insert |
| `[SMASH]` | hard cut with SFX on frame 0 | `[COVER n]` | covered cut: cloud, paint roller or iris hides a change for n f |
| `[FLASH w10,5]` | flash levels per frame (w white, r red, l lava, v vape purple) | `[IRIS♥ out h]` / `[IRIS○ out h]` | heart (cute) or circle (creepy) iris out, hold h f |
| `[POP]` `[SLAM]` `[DROP]` | entrance tables (`TW_POP_*`, `TW_SLAM_*`, `TW_DROP_*`) | `[JELLY]` `[BOING]` `[BRAKE]` `[WOBBLE]` | spring presets |
| `[SQUASH a/b]` | sx/sy at the impact frame | `[SHK_x]` | shake profile (TAP, POW, IMPACT, STAMP, STAMP3, SLAM, RUMBLE, ROAR, CRACK, FWOOMP...) |
| `[!RIPPLE]` | crowd "!" bangs staggered 0-3 f by id hash | `[ZOOM]` | snap zoom (GBA BG2 affine for 2 f under a flash, then a close-up render; PS1 GTE H change) |
| `[3x@15]` | rule of three at a 15 f interval, escalating | `[BUTTON +n: what]` | tiny after-beat n f after the punch |
| `[NO BUTTON]` | sincere or horror beat: the release is withheld on purpose | `[DAY n]` | calendar card: a sleep in the dream house advances the story day |

Dialogue codes inside lines: `{.}` `{..}` `{...}` = pauses of R5 / R8 / R13; `{d}` = deadpan (instant text, no hop, idle off); `{!}` = sting; `{big}…{/}` = the word pops whole; `{w}` = wavy text; `{r}` = record scratch; `{punch}` = A cannot skip the hold that follows.
`[face X]` sets the portrait expression for the line (NEUTRAL, HAPPY, SAD, ANGRY, LOVE, SHOCK, SLEEPY, SICK, POSSESSED, DEAD, TALK).

**Dialogue box budget:** at most 3 lines of about 28 characters per page at 240x160. Every sample line below fits in one page unless it is marked `//` (page break).

---------------------------------------------------------------------------------------------------
## 2. The secret history (the truth the player digs up)

A timeline the game reveals in pieces (flashbacks F1-F4, §5). None of it is said outright before the midpoint.

| When | What happened | Who knows |
|---|---|---|
| ~40 years ago | Hellvile Park is one park. The **Midsummer Mingle** festival is held every year at its centre. Little Giggs, a cherub kid, and little Nox, an imp kid, are best friends. Their thing: he brings cupcakes, she brings bone soup. | Nox, the Mayor, Nana, Grampy |
| 12 years ago | Anvil, Rivet's mum and Hellvile's best smith, welds **the Giggling Fountain** as a Mingle gift. Its magic: a laugh on one side bubbles up on the other side. | Anvil, Rivet (aged 6, holding the torch) |
| 11 years ago, Mingle night | A strange purple cloud rolls over the festival. **Lady Smog**, a Hellvile courtier, breathes it in. She is the first person ever possessed. She stumbles into the Hillvile kids' tent, eyes glowing, saying the kids' worst fears in their own voices. Father Fuzz tries his first exorcism. It fails, and she flees. Panic. Young Mayor Giggs, new in office, says "Close the park." | Fuzz (his wound), Smog (doesn't remember), the Mayor |
| 11 to 10 years ago | The Wall goes up, brick by brick, over one year. Hellvile labourers are paid to build it, then **kept** to pump the cloud vapor that drifts over "their" side. Tallyworn starts counting on the day it is finished. | Tallyworn ("3,652 days") |
| 10 years ago | The Mayor has the fountain **re-plumbed** so that it "keeps the giggles in and pumps the glums out". Every frown, nightmare, swallowed scream and unsent apology in Hillvile drains through a pipe under the Wall. **He never asked where it went.** It feeds the cloud. The cloud learns to talk and calls itself Lord Vapula. Hellvile is forced to pump his body around their sky. | The Mayor (half), Vapula (all) |
| Now | Vapula is too big for Hellvile. He wants to be **breathed in**: to be felt by the people who made him. Vex, a fox who thinks the cloud is "the only one who ever listened", opens **Cloud Nine Vapor** right by the Wall. | Vex, Haze (his clerk, who hates the smoke) |

**The midpoint twist (Ch3, Ep 3.4, "Under the Fountain"):** the fountain cracks. Purple sludge pipes, stamped with Anvil's hammer mark, run under the Wall. HaHaland's happiness has been Hellvile's weather for ten years. *We* made the monster.

**Why this is the right twist for the game:**
* It makes "commingling" the cure.
* It turns the jokey refrain "We don't talk about it" into the actual sin.
* It's scary: the call is coming from inside the fountain.
* It's funny: the town's entire mood system is municipal plumbing.
* It's heartfelt: nobody is the villain, just a scared cherub ten years ago.

**The rule about the demons' true names** (overrides the random `sim_soul.c` pick for authored possessions):

| Possessed | Demon's true name | Payoff |
|---|---|---|
| Steamy (Ch2) | **TALLYWORN** | Ep 4.1: "Someone shouted my name across the Wall once. First time in ten years." |
| Sprinklepop (Ch2) | **WHEEZLE** | Hellvile's Wheezle "coughs out little ghosts. Names them." He named this one "Sprinkles". Side chain I. |
| Hattie Haha (Ch3) | **HAZE** | Ep 4.2: Haze, who hates the smoke, tells Hattie a joke back. |
| Grampy Bean (Ch3) | **SOOTKIN** | Ep 4.3: the tomato seeds over the fence were from xem. |
| Lady Smog (Ch4) | **VAPULA** (a scrap of him) | The first possession, eleven years old. Fuzz's redemption. |
| The leftover breath (Ch3 end) | **GIGGLESWORTH** | The Mayor's own name. The biggest swallowed fear in town was his. |

---------------------------------------------------------------------------------------------------
## 3. Cast arcs: want, wound, payoff

The ids are roster ids. "Lines" is a rough target for this bible, against today's 167 main-plot lines (the main plot grows to about 900 lines).

| Character | Want | Wound | Arc in four moves | Payoff (what you see) |
|---|---|---|---|---|
| **You (@P)**, player, default Sunny the bunnykin | a place that has room for you | your old town "didn't have room"; you arrived with one suitcase you never unpack | Ch1 make a home. Ch2 the home turns on you. Ch3 defend it. Dark night: it's empty. Ch4 give it away (open the door) | You finally unpack the suitcase, in your Hellvile Park house, during the epilogue. Inside: a single photo frame with no photo. You put the festival polaroid in it. [NO BUTTON] |
| **Doomogachi (@E)**, the pet | to be near you | it hatched from a Hellvile egg (smuggled across by Anvil, kept by Nana) and is a little bit demon | beep. Then horns. Then it **eats the leftover breath** to save you and flees over the Wall. Then you find it | It returns with horns, forever (the one persistent change). Final post-credits button: it poops; the poop grows horns. "beep." |
| **Mayor Giggs** (0) | everyone to be happy | he built the Wall and plumbed the fountain out of fear, and lost his best friend Nox | jokes ("Hee. Hee."), then denial, then the confession, then he crosses the gate | At the summit: "I brought cupcakes." "...I brought bone soup." "...I LOVE soup." Then the line he swallowed for 11 years: *"I was scared. I'm still scared. Hold my hand?"* |
| **Nana Puddin** (1) | to feed everyone | she baked for both sides at the Mingle; she hasn't used bone flour in ten years | bakes clues into food (key muffin, map pie, relic bundt), then admits she kept the Hellvile egg | Bakes the Unity Loaf with Scorchmello and Charlotte; there's a key in it (to the gate, which no longer needs one). |
| **Pip + Mr. Flops** (42) | to be brave | Pip's bedtime stories made the Wall the monster | lost bunny, then night patrol, then Mr. Flops kidnapped by imps, then gives Mr. Flops to you on the dark night | On stage: "And Mr. Flops!" Mr. Flops now has tiny felt horns to match @E. |
| **Father Fuzz** (77) | to save every soul | he failed his first exorcism: Lady Smog, 11 years ago | Mr. Huggins dummies keep getting destroyed (runner gag), then real exorcisms, then Lady Smog again | Saves Smog, then gets a hug from Mr. Huggins the 9th, the first dummy to survive. |
| **Rivet** (13) | to be seen as more than "a runaway imp" | she held the torch while her mum welded the fountain, saw what it became, and ran | "Don't make it weird." The weapon choice, the "Days Since Last Fire" sign, recognising the pipe welds | The spoon reunion with Anvil (expanded), plus dad Sprocket admitting he read every letter. |
| **Dr. Bloom** (111) | to heal without being feared | she is Hellvile-born and was trapped on the Hillvile side when the Wall went up; she hides it under pastel scrubs | she runs the investigation (Ch2), is outed in the shelter (Ch3), then crosses (Ch4) | Reunites with her sibling Graveley (730) and opens the first clinic with doors on both sides. |
| **Steamy** (2) | everyone warm | the bath house is lonely | first possession (Ch2), then saved or lost | Saved: hosts the first mixed bath. Lost: her ghost keeps the water warm "from the other side" (side chain H). |
| **Sprinklepop** (81), the pink party bunny | parties | "sneezes glitter. Doctors say it is fine." | Cloud Nine's first customer, then possessed, then the scripted failed exorcism (SOUL LOST), then her lost soul wanders with its map upside down | You guide her soul home; she bounces into the festival with a heart balloon. |
| **Mascotty** (106), "the Starer" | to go home | a Hellvile bunny trapped in Hillvile's mascot suit for ten years; she never takes the head off and never stops facing the Wall | "Ignore that one." Waves only if you stand still. Someone thumps on the Wall at night | At the gate the head comes off. Her brother Thumpkin (894) thumps once. She thumps back. [NO BUTTON] |
| **Hattie Haha** (71) | to make everyone laugh | her whole act is not talking about things | possessed by Haze's breath: she laughs a beat late at everything | Her new set opens with "We don't talk about the other side..." and both sides laugh. |
| **Grampy Bean** (88) | to remember | forgetting things | possessed by Sootkin's breath; forgets you, then remembers you through tomatoes | Two Gardens: his tomatoes and Sootkin's sunflower in one bed. |
| **Miss Lulla** (36) | kids who sleep | she tells kids monsters aren't real | her daycare hears giggles from the Wall | Hellvile kids join nap time. "Monsters are real. They also nap." |
| **Vex** (666) | to be listened to | nobody did, except the cloud | ch2 villain, then hides in your closet during the invasion, then the exit interview | Plain water vapes. "They're terrible." He becomes Haze's employee at the Fresh Air Lounge. |
| **Lord Vapula** (999) | to be **felt** | he is made of everything nobody wanted to feel | fog, then taunts from the top of the Spire, then "Pfffft", then the leftover breath | A small grey plush cloud who rains on Sootkin's sunflower on request. |
| **Tallyworn** (712) | for the Wall to fall | 3,652 days pumping vapor | hears his name shouted at the Steamy exorcism (though he doesn't know it yet) | The last tally mark becomes a heart. He stops counting. |
| **Mother Nox** (778) | her friend back | Giggs closed the park on her | Hellvile's quest hub, then the summit | Soup. A hand held. |
| **Sootkin** (525) | to grow something where nothing grows | a sunflower hidden under the gate | possessor of Grampy's breath; help task 1 | The sunflower in the shared garden, rained on by Vapula. |
| **Lady Smog** (502) | to taste air | possessed for eleven years | the first case and the last exorcism | "I haven't tasted air in eleven years. It's DELICIOUS." She forgives Fuzz before he finishes apologising. |
| **Anvil** (595) | her daughter home | her fountain became a weapon; she sets a place at dinner every night | Ch4 forge | "It's a REALLY good spoon." Expanded in Ep 4.3. |
| **Wisp** (563), the haunting | to go home | an imp kid who cried so long at the Wall that they exhaled themselves out. Their body has slept for two years in the Hell Pits | haunts your house in Ch2 (rearranging it into their old room), helps defend it in Ch3 | Ep 4.4: you rebuild their real room; they wake up. They come to your housewarming in a body. |
| **Haze** (709) | clean air | runs the vape counter and secretly waters the plants | the true name inside Hattie | Turns Cloud Nine into the Fresh Air Lounge. |
| **Gatekeeper Grumbleguts** (Breach 7 boss, `LB_GRUMBLEGUTS`; see §3a) | a good meal and a friend (he would settle for a date) | kept the sealed gate for ten years on promises of a feast that never came; the only one who ever listened to him is his own stomach | Ch2-3 he belly-flops the gate open for Vapula; Ch4 he blocks the Underbelly and loses (and gets fed); then he is the first demon to sit down at Brimstone Bites; post-game he hosts the Feast of Rebuilding | He gets the meal and the friend. Whether he survives dessert is up to you (S186). |

---------------------------------------------------------------------------------------------------
## 3a. Gatekeeper Grumbleguts: from a Breach 7 boss card to a story character

The user's notes (`user_notes.md`, read in full) make Grumbleguts a real character. Everything in this section is canon for the bible; `levels.c`'s boss text, intro card and the Breach 7 level description are updated to match when implemented. His bestiary flavour: **"Keeper of the Sealed Gate. Also of a very large seat."**

**Look** (reference images in `story/refs/`, looked at): a chunky pot-bellied cartoon devil (round gut, stubby horns, fangs, enormous grin), with Baloo's loose, jolly, belly-forward posture, gym-demon muscle *on top of* the gut (bat wings, a thick spiked tail), and a big smooth peach-shaped cartoon rear that is always, somehow, in frame. Hell dress code, cartoon-modest: spiked collar, leather chest harness, studded belt and cuffs, a chain with the gate key on it, a riding crop as his gatekeeper's tool. He is glossier, rubberier and sharper-lined than the plush citizens, because he is a hell boss, but he lives inside the game's art style. Portrait, chibi, boss sprite, level-select preview and bestiary entry all show the belly **and** the rear. He is the only character in the game who **does not drip and does not wobble like plush: he bounces**, like a balloon, with `TW_BOING`.

**Voice.** Two speakers share his dialogue box: **GRUMBLEGUTS** (a warm, oily baritone) and **GURGLE**, his belly (voice sliders at the floor, 3-4 frames per syllable, bass babble). The belly has opinions. It is always hungry, always honest, and sides with the player about half the time. It interrupts his villain speeches, answers questions for him, and gets a `{r}` record scratch of its own on big gurgles.

**Personality.** Hell's biggest flirt. He flirts shamelessly with every *adult* in Hellvile and Hillvile: demons, citizens, the player, Father Fuzz (to Fuzz's horror), and the Gate itself. Winks, pet names ("sugarhorn", "padre", "new kid"), "come up and see my gate sometime", a little black book the size of a phone directory, a dating profile (chain U), exes who turn up in cutscenes. Bedroom eyes in the boss intro; "you're cute when you're scared" before the fight. The rear show-offs *are* the flirting: strutting away camera-side, backing into frame, bending to pick things up, admiring himself in any reflective surface, wiggling it as a taunt, going through doors rear-first, sitting on things and occasionally people. **Every rear reveal gets a record scratch.**

**Hard rules (from the notes; non-negotiable):**
1. Innuendo and implication only. No explicit acts, no genitals, no nudity beyond cartoon-smooth cheeks. Hazbin-Hotel / Cuphead-devil energy, cheeky not graphic.
2. **Kids are off limits** for any flirting: Pip, Wobbles, Tantrum, Pokefork, Wisp, Bolt, anyone with the `KID` trait, and the "edgy teens". With them he is a grumpy uncle. ("Eat your vegetables. {d}Not those. Those are mine.")
3. Everything is consensual and played for laughs. Targets can and do reject him. **Running gag: nobody is interested, except possibly the Gate.**
4. The joke is always his vanity and gluttony, never the target.
5. Good-natured slapstick. Never mean. He is lovable, and the game loves him.

**Gag ladder by chapter** (rules of §3.17 of the timing bible: establish small, repeat soon, change the context, break the frame last):

| Ch | Appetite gag | Rear gag | Flirt gag |
|---|---|---|---|
| 2 (Ep 2.7 insert) | first seen mid-meal on a mountain of Brimstone Bagels; "I only had a light snack" as a forklift beeps in with the next course | squeezes through the gate he cracked, rear-first; stuck; `[BOING]` pop-through `{r}` | to the Gate: "Don't be like that. I'll be gentle." The Gate creaks. |
| 3 (Ep 3.1 insert, Ep 3.6 floor 5 cameo) | mid-invasion he is eating the Fun Fair's cotton candy machine, machine included | backs into frame to block the Spire door; the camera cannot get around him | tries a line on Hazey the loyalist mid-foghorn; the foghorn answers |
| 4 (Ep 4.5 boss) | unbuckles his belt with a dramatic sigh before the fight; mid-fight he pauses to eat a thrown dish and *rates* it | sucks his gut in to look menacing `[HOLD 13]`, it springs back out `{r}`; the belly-flop; the trampoline bounce | "You're cute when you're scared." To Fuzz: "Padre. Come up and see my gate sometime." FUZZ: `{d}No.` |
| 4 (Ep 4.7, credits) | at the festival buffet, back to the crowd, the table empty around him; bigger every time the camera cuts back | the crowd's eye-lines; Honkers offers a napkin; he flirts with the napkin | credits cameo: waves a drumstick at the camera, then at the Gate |
| Post-game (S186) | the Feast of Rebuilding: a fullness meter on the belly | the visual thesis of the scene (§13, S186) | the Gate, repaired, now has a little heart painted on it. Nobody knows who did that. Everybody knows. |

**Fight mechanics** (Breach 7 boss arena in the Underbelly, story-entered in Ep 4.5 and replayable from the Arena):
* **The belly is the weak point.** It glows when he is hungry (every 10 s). Hits anywhere else bounce off with `*boing*` and a wink.
* **Feeding pacifies.** Throwing a Brimstone Bites dish (Brimstone Bagel, Lava Cake, Soul Soup, Hell Peppers, cooked in the cooking scene) at the belly stuns him for 8 s while he eats and rates it ("Six. {d}Needs salt."). Three dishes without a single hit ends the fight peacefully and counts toward `USTORY_PACIFIST`.
* **Belly-flop shockwave:** he jumps, hangs in the air `[FREEZE 3]`, lands `[SHK_ROAR]`; jump the ring. GURGLE gives a one-gurgle warning before each flop (the belly sides with you).
* **Burp portals:** he burps imps out of the wall portals in threes `[3x@15]`.
* **Trampoline:** touching the belly bounces the player across the arena with `[SQUASH 1.3/0.7]`; useful for reaching the upper ring.
* **Defeated:** he sits down. "...Is there dessert?" GURGLE: `{d}gurgle.` (He survives; he is a boss who gets fed, not killed.)

**Redemption (Ep 4.5 → Ch4 → post-game):** after the fight he admits what is down there scares him ("There's a THING down there. It beeps. {..} It ate my lunch."), and that nobody ever came up to see his gate. In Ch4 free roaming he turns up at **Brimstone Bites** (the Hillvile restaurant from Ep 1.1, "It's ironic!") as its first Hellvile regular, then its cook (chain T, "Feed the Gate"). He wanted a good meal and a friend. He gets both. The post-game decides whether he keeps them (S186).

**Sim hook:** in Ch4 and free play he can romance adult citizens like any other resident (`ROMANCE_*`, cross-border crushes). The dating and breakup news ticker goes wild. He is rejected almost every time. Rejections are kind.

---------------------------------------------------------------------------------------------------
## 4. The dream house: the emotional anchor

Every chapter ends at home. Every story day (`[DAY n]`) starts and ends in your bed. The house is a character, so its mood is tracked as one variable, `home_phase`, saved in the story `save_ext` chunk.

| Phase | When | Look and feel (GBA iso / PS1 2.5D) | Gameplay | Signature beat |
|---|---|---|---|---|
| **COSY** | Ch1 | Pink door cottage at (4,7). A single room, then a loft from Ep 1.5. Heart wallpaper, wood floor, morning light. | Decorate mode tutorial: place, rotate, pick up, rate. Welcome-basket furniture. Coziness score. | First night: the egg wobbles in the basket, [R21], beep. |
| **HAUNTED** | Ch2 | The same room. Things are **one cell off** when you come back. A chair faces the wall. Your rug is rotated 90°. A tiny imp-sized bed you never placed. The window shows the Wall even though the window faces the beach. | Decorate mode lists items "placed by: ???". You can't sell them; they come back. Night "Who's There" mini-events. | You put the chair back. [R13] Turn around. The chair is back. Turn again: the chair is sitting **on the bed**. [HOLD 21] "beep." |
| **BARRICADED** | Ch3 | Boards over the windows (welded), furniture stacked against the door, a shelf turned into a turret mount. Red sky through the cracks. | **House Defence** waves: furniture is cover, weapons mount on shelves, and Wisp helps (moves a dresser in front of a breach on cue). | The heart-wallpaper and the wood floor survive every wave. A heart rug with a scorch mark in the shape of a heart. |
| **EMPTY** | dark night (Interlude III) | Everything is gone: given away as barricades, burned, or just off. No pet bed. Music off. | Decorate mode can only **remove**. Then the neighbours knock, one by one, each bringing one item. | §5, "The Empty Room". |
| **OPEN** | Ch4 | The door is propped open. Hellvile furniture appears (bone lamp, soup cauldron, lava lamp that's actually lava). A second home: a Hell Pits flat. | Hosting: invite Hellvile neighbours; their reactions; a "Neighbourly" score. | Housewarming: Wisp (in a body), Nox, Tallyworn and Mascotty without her head sit on your floor. |
| **DREAM** | post-game | Build anywhere in the united park: plot, walls, rooms. | Free-play building, visiting, rating anyone's home. | The suitcase is unpacked. |

**Homes you can own:** the Pink Door Cottage (Ch1), Hillvile Towers apartment 7B (Ch2, unlocked by side mission S114, chain S), a Hell Pits flat (Ch4, Ep 4.4) and the Park Plot (post-game, E2).
**Homes you can visit and decorate for others:** Nana's bakery loft, Pip's bedroom, Steamy's bath house, Wisp's old room, Anvil's dinner table, Mother Nox's chapel kitchen and Vapula's tiny cloud room.

---------------------------------------------------------------------------------------------------
## 5. Running gags, and how each one mutates

The rules are from timing bible §3.17: establish small; repeat soon, nearly identical; then change the context, not the gag; the last time breaks the frame; one-shot gags return at the ending.

| Gag | Ch1 | Ch2 | Ch3 | Ch4 | Post-credits |
|---|---|---|---|---|---|
| **"beep."** (the pet) | tiny bubble on hatch, then again when you sit on the egg's old basket | with horns, in the corner of the haunted room, at nothing | in the middle of the invasion, between explosions, [MUTE 14] | from far away, across the Wall (the pet is in Hellvile) | the poop grows horns. beep. |
| **"...it's fine."** | Nana when the oven smokes | Sprinklepop, possessed, sneezing purple glitter | painted on the church barricade | the Mayor about crossing the gate (it is not fine; he does it anyway) | the title screen logo, on fire: "(it's fine.)" |
| **"Hee. Hee."** | the Mayor's nervous laugh | the fountain giggles "Hee. Hee." a beat late | the leftover breath says it in the Mayor's voice (scary) | the Mayor laughs at a real joke: "Ha!" [FREEZE ALL] everyone stares. "...Ha?" | — |
| **Mr. Huggins** (the practice dummy) | grave sign "Rest in pieces, Mr. Huggins the 1st" | the 2nd is destroyed in the practice | the 3rd-6th fall in the rule of three | the 9th survives and hugs Fuzz | — |
| **Nana bakes it into food** | the key in a muffin | a map in a pie | a holy relic in a bundt | a key in the Unity Loaf (to a gate that no longer exists) | — |
| **Mascotty the Starer** | in the background: "Ignore that one." | at your window, perfectly still | the only citizen not hiding during the invasion. Still facing the Wall. | the head comes off | — |
| **"Days Since Last Fire"** sign (Rivet's shop) | — | 7, then "-", then 0 | the sign is on fire | "0", decorated with flowers | the sign *itself* catches fire; it now reads 0 forever |
| **Crowned cloud prop** ("Employee of the Month", Cloud Nine) | — | on the wall of the vape shop | it is Vapula | "Employee of the Month: Vex" (side chain K) | — |
| **"My eyes are just doing a rain."** | Pip | Steamy after the exorcism | the Mayor on the dark night | Tallyworn at the last tally | — |
| **Mailbox gag** | junk mail from "The Other Side" | the letters are all addressed to Hellvile people | the mailbox is a turret | it delivers both ways (Lilypad Lou chain F) | — |
| **Grumbleguts is eating** | — | a mountain of bagels; "I only had a light snack" as the forklift arrives | the cotton-candy machine, machine included | the festival buffet, empty around him, bigger every cut | the Feast of Rebuilding (S186): the fullness meter |
| **Record scratch on the rear** | — | the first reveal: stuck in the gate he cracked, `[BOING]` | backs into frame to block a door | sucks his gut in, it springs out; the crowd's eye-lines at the buffet | S186: the whole scene is staged around it; the last beat is a lone pair of cheeks drifting down |

---------------------------------------------------------------------------------------------------
## 6. Wrongness creep: the background horror catalogue (Ch1-2)

These are small, cheap wrongnesses in the pastel. The renderer, the props and the NPC system already support most of them. Each one is **noticed once** with a tiny reaction and then **never explained** until Ch3. They are rate-limited to one per 20-40 s of walking (timing bible §5.2).

| # | Wrongness | Trigger | Comic or scare beat |
|---|---|---|---|
| W1 | The Giggling Fountain giggles **R8 late** after you pass. | Ch1 Ep 1.1 onwards | You stop, [R5], it giggles again on time. "...Hee." |
| W2 | The "Population: 500" sign has a faint scratched-out "1000" under it. | Ch1 | A on the sign: "Someone painted over a number." |
| W3 | Flowers in the beds all lean toward the Wall. | Ch1 nights | Pick one, [R13], it slowly re-leans in your hand. |
| W4 | Mascotty, perfectly still, in a crowd that bobs. | Ch1 onwards | §5 |
| W5 | One lamp post faces the wrong way, toward the Wall. | Ch1 | It faces you the next time. |
| W6 | Thumping from the Wall at night, 3 beats, then 1. | Ch1 nights | Thumpkin signalling (side chain J). |
| W7 | A citizen says "we" instead of "I". | Ch2 Ep 2.2 | "We love the weather. I mean... I." |
| W8 | Shadows lag 2 frames behind their owners. | Ch2 | A clue for Dr. Bloom. |
| W9 | Everything tastes like grape. | Ch2 | Existing small-talk topic, promoted to a clue. |
| W10 | Your reflection in the bath-house water blinks late. | Ch2 | [R5], your reflection waves. You didn't. |
| W11 | The church bells toll backwards for one bar. | Ch2 nights | Cantor Coo: "Did... did the bell just un-ring?" |
| W12 | Kids' drawings at the daycare all have a purple cloud with a crown. | Ch2 | Miss Lulla: "They all drew the same friend." |
| W13 | The fountain water turns lilac for one frame (an [INSERT 1]). | Ch2 | Only the pet reacts: "beep?" |
| W14 | The jukebox at the cafe plays SONG_OMINOUS for 2 notes, then the right song. | Ch2 | The cafe owner hits it. "It does that." |
| W15 | Your house: the chair one cell off. | Ch2 | §4 HAUNTED |

---------------------------------------------------------------------------------------------------
## 7. The shape: chapters, episodes, interludes and length

| Chapter | Episodes | Interludes and specials | Target time (main path) |
|---|---|---|---|
| **1. Welcome to HaHaland** | 1.1-1.6 | Interlude I "Night Shift" (the pet's first night insert) | 2.5 h |
| **2. Something in the Vapor** | 2.1-2.7 | Dream D1 "Teeth Like Muffins"; Flashback F1 "The Fountain Gift" | 3.0 h |
| **3. Hell Invades** | 3.1-3.7 | Flashback F2 "Mingle Night"; F3 "The Plumber's Order"; **midpoint twist** (3.4); **dark night of the soul**, Interlude III "The Empty Room" | 3.5 h |
| **4. Hellvile Park** | 4.1-4.7 | Dream D2 "The Ninth Cloud"; Flashback F4 "Cupcakes and Bone Soup" | 3.5 h |
| **Epilogue** "Happily Ever After (Mostly)" | E1-E3 | the credits parade; post-credits button | 0.5 h |
| **Post-game** | free play | NG+ "HaHaland Remembers" | — |
| **Total** | **27 episodes + 8 specials** | | **~13 h main, plus ~12-15 h of side content (§13: 96 new missions on top of the 49 existing)** |

**How the length stays honest:**
* Every episode has a **purpose** (a character arc step, a reveal, a mechanic introduced) and a **button that is a callback 1-10 minutes later** (timing bible §5.1).
* The four "talk to N random people" counters are replaced by authored vignettes: Ep 1.4, 2.2, 4.2 and 4.6.
* A story day passes when you sleep at home. Some episodes need night (`[NIGHT]`), some need a new day. This uses the life sim's clock without adding grind.

**Silence budget:** about 20-25% of cutscene time is music-free. Overworld music ducks and tape-stops but never disappears, except during the dark night.

---------------------------------------------------------------------------------------------------
## 8. CHAPTER 1: WELCOME TO HAHALAND (2.5 h, days 1-4)

**Chapter purpose:** make the player love this town and this house before anything goes wrong. The horror is *only in the background* (W1-W6). Joke density follows the dialogue curve: one punchline per 3-6 lines.
**Music:** HAHALAND_DAY and NIGHT. Every scare-adjacent beat ducks; no OMINOUS until Ep 1.6.

### Ep 1.1 "Population: 500" (arrival; existing C1_ARRIVE and C1_MAYOR, expanded)
**Purpose:** first laugh, first wrongness, meet the Mayor, establish the refrain.

| Beat | Picture | Timing |
|---|---|---|
| 1 | Chapter card. A pink bus over candy hills; the player hops in the back window. | card `[POP letters TW_DROP_LET]`, bus in at constant speed |
| 2 | Sign: "HaHaland! Population: 500 lovely citizens." | sign `[DROP TW_SIGN_DY]` `[SHK_TAP]` |
| 3 | The bus brakes. | `[BRAKE]` record-brake on the camera; the suitcase flies forward `[SQUASH 1.3/0.8]` |
| 4 | The sign updates: "...and now, @P!" (501). [R8] The "1" falls off with a *tink*. | `[BUTTON +13: tink]` |
| 5 | Fountain plaza. The Mayor (a cherub) flaps over. The crowd waves in a ripple. | `[!RIPPLE]` hearts instead of "!" |
| 6 | The Mayor's welcome speech. In the background (W4) Mascotty does not wave. | Mascotty `[STILL]` |
| 7 | The refrain line, then a giggle from the fountain R8 late (W1). | `[DUCK .5 13]` |

> MAYOR: Welcome, welcome! I'm Mayor Gigglesworth. Call me Giggs!
> MAYOR: [face HAPPY] Ooh, a new face! We get so few new faces. We had a whole fence of them once.
> MAYOR: [face SHOCK] {..} Not a fence OF faces. Faces AT a fence. Hee. Hee.
> MAYOR: That's Hillvile Lane, the beach, the Fun Fair... and over there is the Wall.
> MAYOR: [face NEUTRAL] {d}We don't talk about the other side.{punch}
> `[HOLD 13]` (the fountain giggles, `[R8]` late)
> MAYOR: [face HAPPY] Hee. Hee! Your key is with Nana! Follow the smell of nutmeg!

**Player choice** (the first player lines; this gives the hero a voice):
* "Nice to meet you!" → Mayor `[face LOVE]` "Manners! We'll keep you."
* "What's on the other side?" → `[FREEZE ALL]` `[TAPESTOP]` `[!RIPPLE]` every citizen looks at you `[HOLD 21]` `[THAW]` music back on the downbeat. Mayor: "Hee. Hee." *(sets flag SF_ASKED_OTHER_SIDE, which is remembered in Ep 3.6)*
* "...Why is that mascot staring?" → Mayor: "Ignore that one." `[BUTTON +21: Mascotty waves very slowly]`

### Ep 1.2 "The Key in the Muffin" (existing C1_KEY and C1_HOME, expanded)
**Purpose:** Nana, the egg, the first room. Teaches decorating.

* Nana's bakery. The key is baked into a muffin. Rule of three: you bite the wrong muffin `[3x@15]` (raisin, raisin, "that's a button"), and the 4th is the key `[POP TW_POP_EYE]` `[FLASH w6]`.
* She gives you the egg wrapped in a tea towel, and **hesitates** `[R13]` before letting go (a seed: she knows where it came from).

> NANA: You're the new one! You look hungry. Everyone looks hungry to me.
> NANA: The key's in a muffin. Don't ask which. I forgot. I always forget.
> NANA: [face SAD] {..}And this. An egg. It's been waiting for someone.
> NANA: [face HAPPY] The last one hatched into a little demon! Ha! Kidding.{.} Mostly.

* **The Pink Door Cottage.** The `CUT_HOME` vignette becomes the first real **interior** scene (housing mode).
  * The welcome basket holds: a bed, a heart rug, a lamp, a crate and an "egg basket".
  * Tutorial: place the bed `[DROP 24px] [SQUASH 1.22/0.78] THUD+BOING`; neighbours wobble `[JELLY 0.4 +2f]`; rotate the lamp (squash-flip); place on an invalid spot (the "nope" shake).
  * First **room rating**: the counter rolls, the grade stamp `[SLAM TW_SLAM_STAMP]`, "COSY: B+", then three imaginary visitor thought-bubbles 15 f apart: "♥", "♥♥", "...is that a crate?". `[BUTTON +13: the crate slides 1 px in shame]`
* Sleep: `[DAY 2]`.

### Ep 1.3 "Hatch Day" (existing C1_HATCH and C1_PIP to C1_BUNNY_RETURN, expanded)
**Purpose:** the pet, the "beep." runner, Pip.

* The egg wobbles at a period of 6 f, then 3 f, and hatches **on the music's downbeat** (timing bible §6.6). Hatch scene in `SCENE_PET` arg 0.
* `[R21]` of the pet just blinking. Then a tiny bubble: **"beep."** `[POP TW_POP_BUBBLE]` `[DUCK .55 6]`.
* The player names it (`@E`). Then `[INSERT 1: the LCD shows a pentagram]`. Only one frame, the "did I just see that?" seed (a full 4 f insert comes in Interlude I).
* Pip on the beach: "My eyes are just doing a rain." Find Mr. Flops (the beach trigger). **New:** Mr. Flops is lying face-down, staring at the Wall. You turn him over. His stitched smile. `[R8]` beep.

> PIP: [face SAD] I lost Mr. Flops. He's a bunny. I'm a bunny. We match.
> PIP: My eyes are just doing a rain. It's fine.
> PIP: [face HAPPY] (after) MR. FLOPS! You're damp! Were you scared?
> PIP: [face NEUTRAL] {d}...He says he saw the Wall breathe.
> PIP: [face HAPPY] He's silly! Here, a skull sticker! Make friends with EVERYONE!

### Ep 1.4 "Three Neighbours" (replaces the C1_FRIENDS counter)
**Purpose:** meet, by name, the three people who will be possessed later, so their possessions hurt. Each visit is a tiny episode with its own gameplay verb. You can do them in any order.

1. **Steamy's bath house**: a "fill the tubs" mini-task. Every tub has a rubber duck; one duck faces the Wall (W-style wrongness).
   > STEAMY: Welcome! Every tub has a duck. Every duck has a name.
   > STEAMY: That's Duckworth. That's Quackers. That's...{.} [face SHOCK] I don't know that one.
   > `[R13]` The unknown duck squeaks on its own. `[BUTTON +8: Steamy squeaks a duck back, nervously]`
2. **Hattie Haha's open mic** at the cafe: the **comic timing minigame**. Press A on the punchline beat; the crowd laughs in a ripple; a perfect press = "snort-laugh".
   * Hattie's closer: "So I said, 'Mayor, what's on the other side?' He said, 'Hee. Hee.'" `[R8]` big laugh. `[BUTTON: Mascotty in the back row doesn't laugh]`
3. **Grampy Bean's tomatoes**: pick 5 tomatoes. He calls you a different name each time ("Sunny... Sonny... Bunny... Toaster").
   > GRAMPY: These seeds came over the fence years back. Never asked who threw 'em.
   > GRAMPY: Best tomatoes in HaHaland. Don't tell anyone they're from...{.} [face SLEEPY] where was I?
* After all three: a text from Nana, "You've made friends! Come get a cupcake to share."

### Ep 1.5 "Housewarming" (absorbs C1_CUPCAKE)
**Purpose:** the house becomes *yours*; the first party; the first clear scare at the window.

* Nana's cupcake goes to Miss Lulla, as now; she mentions that "the fog giggles at night" and that the kids drew a purple friend (W12).
* **You host your first housewarming.** Invite 3 friends (Steamy, Hattie and Grampy, or anyone you've befriended). Each guest reacts to one piece of furniture (the variant pool from timing bible §6.4).
* The loft unlocks (a second floor: the iso view on GBA stacks a second diamond layer; on PS1 the camera tilts up).
* At the end of the party, everyone waves goodbye in a ripple. The door closes. `[R21]`
* `[STILL Mascotty]` at the window, perfectly still. Music `[MUTE 41]`.
  > (caption, small, cool outline) {d}Ignore that one.
  > `[R21]` Mascotty waves, slowly, 8 frames per phase. `[IRIS♥ out 5, on the window]`

### Ep 1.6 "The Giggling Fog" (existing C1_WALL, expanded; the first real scare)
**Purpose:** the first real scare; the Mayor's fear shown, not told.

* `[NIGHT]`: you're woken by thumping, 3 beats and then 1 (W6). Follow it to the Wall.
* The purple creep tint ramps from 0 to 4 as you approach (by x-cell, as today). Overworld music `[DUCK .4]`; crickets stop one at a time `[R5]` each.
* The fog bank. Choice: "Peek" / "Back away".
* **CUT_FOG, re-timed as a beat script:**

| f | event |
|---|---|
| 0 | the fog wall, idle drift; music OMINOUS low |
| 40 | two red eyes open `[POP TW_POP_EYE]` *tik* |
| 48 | two more `[R8]`; 53: two more `[R5]`; 56: two more `[R3]` (accelerating three, plus one) |
| 56 | `[MUTE 40]` all eyes `[STILL]` |
| 76 | caption types slowly: "come and PLAY with us" `{w}` |
| 96 | `[INSERT 4: a Hillvile kids' tent, eleven years ago; a cloud-shaped lady with glowing eyes]` (the first flash of flashback F2) |
| 100 | `[SMASH]` the Mayor's face fills the frame, close-up, *panting* `[SHK_POW]` |

> MAYOR: [face SHOCK] {big}NOPE.{/} Nope! Home! It's bedtime! It's always bedtime!
> MAYOR: It's just...{..} weather. Hellvile weather.
> `[R13]` (walking you home, quieter) MAYOR: [face SAD] {d}Please don't go near there again.

* Back home. Your pet is at the window, staring at the Wall. Turn it around. `[R13]` It's facing the Wall again. "beep." `[DAY 4 → Chapter 2]`

### Interlude I "Night Shift" (playable, 2 minutes; the pet's first full insert)
At night, after Ep 1.6, you can check on @E in the pet scene. Feed it a cupcake.
* Classic s04 structure: eats `[SQUASH]`, strain (8 f with 1 px jitter), plop. Then `[INSERT 4: LCD fills the screen: pentagram 2f, demon face 2f; music muted exactly 4f]`.
* Back to cute: `[ONE CHANGE: two tiny horn nubs]`, `[R2]`, "beep.", `[R8]`, then the caption `{d}...it's fine.`
* Sets `SF_PET_NUBS`. Nana notices tomorrow: "Ah. {..} He's teething." (She knows.)

---------------------------------------------------------------------------------------------------
## 9. CHAPTER 2: SOMETHING IN THE VAPOR (3 h, days 5-9)

**Chapter purpose:** the wrongness moves from the background into people, and then into **your house**. The scares are slow; the jokes are louder, because everybody is overcompensating. The haunted house is the chapter's spine.

### Ep 2.1 "Grand Opening" (existing C2_VAPOR, expanded)
* Chapter card (drifting vape puffs). Then "A few days pass" becomes `[DAY 5]`.
* Cloud Nine Vapor opens by the Wall. Its ribbon is cut by Ribbonetta, who sneezes purple.
* **Sprinklepop** (the pink party bunny) is the first in line: "Grape? I LOVE grape. I love everything!"
* Vex's pitch, the trailer s07 scene made playable: Vex leans in **too close**, and his smile is **R8 too long** `[STILL Vex mouth]`.
* The crowned-cloud "Employee of the Month" frame hangs behind the counter. The picture is a cloud wearing a crown. Its eyes follow you; nobody comments.
* **The vape choice** (as today, but it now matters, see §12):
  * **Decline**: Vex `[STILL]` `[R21]`: "...Suit yourself." Haze, behind him, gives you a tiny thumbs-up.
  * **One puff**: the s07 inhale (10 f stretch, 5 f hold), **FWOOMP** `[SHK_FWOOMP] [COVER 22: cloud fills the screen]`; two red eyes in the fog at f 10; the cloud clears to show you fine. `{d}Everything is fine.` `[BUTTON +24: a balloon pops somewhere]`. Sets SF_VAPE_SAMPLE: from now on, *you* hear the fountain's late giggle a little louder, and Vapula knows your flavour in Ep 3.6.

> VEX: Welcome to Cloud Nine! Where every breath is a hug from the inside.
> VEX: [face HAPPY] I'm from out of town.{.} WAY out.
> VEX: First puff's free. Second puff's free. {d}All the puffs are free. That's the business model.
> HAZE: (whisper) {d}...don't.

### Ep 2.2 "Grape" (replaces the C2_WEIRD counter; a playable investigation)
**Purpose:** Dr. Bloom joins the plot; you collect clues instead of counting chats.

* Dr. Bloom's clinic: "Five patients today. All say everything tastes like grape. I'm a doctor, so: that's weird."
* **Find 3 of 5 clues** (an open investigation, shown on the quest log's new "Clues" board):
  1. **Grape tongue**: give Sandwichy a blind taste test (cooking minigame); every flavour reads "GRAPE".
  2. **"We"**: catch a customer saying "We love the weather. I mean... I." (W7). Press A *during* the slip (a timing check); miss it and they repeat, smoother.
  3. **Lagging shadows**: photograph a shadow two frames behind its owner (photo verb, W8).
  4. **Backward bells**: stand at the church at night (W11).
  5. **The crowned drawings**: at Miss Lulla's daycare (W12).
* Turning in to Bloom: she takes your pulse with a cold hand. `[face SHOCK]` "Sorry. Ghoul." `[R8]` "...I mean: *cool* hands. It's a doctor thing."
  > DR. BLOOM: Grape. "We." Late shadows. It's not a bug. It's in the air.
  > DR. BLOOM: [face SAD] I've seen this before.{..} {d}Somewhere else.{punch}

### Ep 2.3 "Things Move When You're Not Looking" (new; the house becomes HAUNTED)
**Purpose:** the scariest episode in Ch1-2, and almost entirely funny.

* `[NIGHT]` You come home. Your chair is one cell off. Decorate mode marks it "placed by: ???".
* **The rule-of-three haunting** (each step is a playable moment in the room):
  1. Put the chair back. Look at the bed (camera turn: GBA rotates the iso view 90°; PS1 orbits). Look back. The chair is back where it was. `*creak*`
  2. Put it back again. Look away. It's now facing the wall, like a kid in time-out. `*creak, creak*`
  3. Put it back. Look away. `[MUTE]` Look back: **the chair is sitting on your bed**, tucked in, under the blanket. `[HOLD 21]` `[STILL ALL]`. Then from the pet bed: "beep." `[BUTTON +8: the blanket rises and falls once, breathing]`
* Topper, one R13 later: a tiny **imp-sized bed** you never placed, in the corner. A drawing taped above it: a house with two windows, and a purple cloud with a crown.
* Choice: "Sleep in the living room" / "Sleep in your bed anyway" (both cute; the second gives a Brave sticker and a gag: you wake up with the chair *cuddling* you).

> (caption) {d}Something in your house likes your house.
> (caption, next morning) {d}...It moved the rug too. It looks better, honestly.

* This starts **Wisp's haunting** (Wisp is invisible). For the rest of Ch2, each night at home one furniture piece is "rearranged". It always moves toward the layout of Wisp's real room in Hellvile, which Ep 4.4 pays off. Decorate mode keeps a "Rearranged by ???" list.

### Ep 2.4 "Steamy Isn't Home" (existing C2_SCREAM and C2_FUZZ, expanded)
* In the plaza, Steamy stands in the fountain, **completely still**, while the water flows around her. `[STILL Steamy] [TWITCH ears]`.
* The scene runs **as a beat script on the overworld** (NPC scripting: walk-to, emote on cue):

| f | event |
|---|---|
| 0 | the crowd circles, `[!RIPPLE]` |
| 30 | Steamy's head turns 180° over 6 f with a creak `[smoothstep]` |
| 36 | `[MUTE 60]`: |
| 50 | STEAMY: {d}Steamy isn't home right now. |
| +R13 | STEAMY: {d}We moved in. |
| +R8 | STEAMY: [face POSSESSED] {d}Leave a message after the scream. |
| +R21 | a scream, which is **a rubber-duck squeak**, enormous `[SHK_ROAR]`. Everybody flees in the accelerating flee curve; sweat drops every 3rd frame |
| +R8 | `[BUTTON: Mascotty doesn't flee]` |

> MAYOR: [face SHOCK] Get Father Fuzz! Get Father Fuzz! I'll... get behind you!

* **Sprinklepop is possessed too**: she's still smiling, sneezing **purple** glitter. "I'm fine! {.}{d}We're fine."
* **Father Fuzz**, priest of St. Cuddles, plush priest stuffed "with love and a little bit of buckwheat".
* **Practice ritual on Mr. Huggins the 2nd** (the 1st has a grave sign). The practice is a **scripted rule of three** (timing bible §3.11) that teaches the three exorcism parts:

| try | at | method | result |
|---|---|---|---|
| 1 | 0 | prayer rhythm (the player's input) | Mr. Huggins' arm pops off `[SLAM]` |
| 2 | +15 | holy water | his other arm. `[SLAM]` |
| 3 | +16 | the true-name pick | his head rolls off, bounces 3 times (*tink tink tink*), comes to rest **facing the Wall** `[SHK_STAMP3]` |
| topper | +13 | — | FUZZ: {d}Rest in pieces, Mr. Huggins the 2nd. |

### Ep 2.5 "The Power of Cuddles" (existing C2_EXORCISE, expanded)
**Purpose:** the first true exorcism; the first true name; the first big emotional beat.

* **Two exorcisms:** Sprinklepop (scripted to fail, so the trailer's s11 lives in the game), then Steamy (for real, odds shown).
* **Sprinklepop:** the trailer s11 beat for beat. "THE POWER OF CUDDLES COMPELS YOU!" types at 1.5 chars per frame. The answer is a tiny, instant `{d}ow.`. Then **FAIL x1 (spray bottle), x2 (a real hug), x3 (the whole bucket)** `[3x@15]` with the kazoo stepping down. Then **SOUL LOST.** `[SLAM]`
  * **Comedy-horror resolution:** her soul pops out and is literally *lost*. It holds a map upside down, waves, and wanders off toward the Wall. Her body falls asleep, smiling, at the clinic.
  * Fuzz: `[face SAD]` "We'll find her. Souls don't go far. {..}{d}They go *exactly* as far as they're scared to." (That sets up side chain I, and the theme.)
* **Steamy:** the real ritual, the full minigame. The true-name pick offers 4 Hellvile names. Clues from the possessed's taunts: "We count. We count every day." "Three thousand, six hundred and..." The answer: **TALLYWORN**.
  * Saved:
    > STEAMY: [face SICK] ...Why do I taste grape?
    > STEAMY: [face SAD] There was someone in my head. Counting. He was so tired.
    > FUZZ: The name you shouted, @P... {..} That's a real person. Over there.
    > `[R21]` `[NO BUTTON]` *(the first heartfelt beat is never undercut)*
  * Lost (three failures): Steamy's ghost floats up, translucent, holding a rubber duck. "Oh. {..} Oh, that's... cold." Fuzz: "When you're ready, we can lay her to rest." **New:** the graveyard scene gets a story beat (Ep 3.3, the graveyard nest) and side chain H.

### Flashback F1 "The Fountain Gift" (in the Workshop; a 40 s playable memory)
It is triggered the first time you weld with Rivet (Ep 2.6), and plays in a sepia palette (a palette lerp, cheap on both platforms).
* Little Rivet, aged six, holds a torch for her mum Anvil, who welds a fountain shaped like two laughing faces facing each other. "When someone laughs here, it bubbles up over THERE." A bow on it. "For the Mingle."
* `[SMASH]` back to now. Rivet's hand is shaking. `{d}"...Nothing. Weld fumes."`

### Ep 2.6 "Don't Make It Weird" (existing C2_RIVET, expanded)
* Rivet, the runaway imp welder, under a car. **"Don't make it weird."** (Three times, at R13, R8 and R3 as she gets more flustered when you thank her.)
* **The weapon choice** (Bubble Wand cute / Pump Shotgun brutal), then the workshop.
* **The "Days Since Last Fire" sign** is introduced: it reads 7. Her tail touches a spark. The sign flips: 7, then "-" for 2 frames with jitter, then **0**, in red `[SLAM]`. She puts the tail out in a bucket. Mask up `[R8]`. Thumbs up. (The trailer's s09 in-game.)
* Rivet: "Lord Vapula's cooking something big. And your Vex guy slipped into his cellar with a glowing crate."

### Dream D1 "Teeth Like Muffins" (night, forced Peek at a Dream; 90 s)
A comic nightmare in the Dream scene.
* You're at the Mingle (you've never been; it looks wrong). Everyone has Nana's muffins for teeth.
* Nana: "Eat up!" `[3x@15]`: a bite, a bite, a bite that is a **key**.
* The key opens the Wall. Behind it there is only **your own house**, with Wisp's little bed in the corner.
* Wisp (a silhouette) whispers: `{w}"is this your room? it's nice. can i stay?"` `[IRIS○ out 10 on the bed]`
* Wake up: the chair is tucked into your bed again. You're both in it. `[BUTTON: beep.]`

### Ep 2.7 "The Cellar" (existing C2_CELLAR, Hell Breach 1)
* The cellar hatch, then **Hell Breach 1: the Vape Cellar** (combat).
* **New boss beat:** Vex appears over the PA between waves (combat HUD caption, rate-limited): "Customer satisfaction is our TOP priority!" `[R26]` "...why are you shooting the merch."
* Won: the note "Too late, Hillvile. The big cloud is already rising. Kisses! - V".
* **The eruption is staged in two parts:**
  1. **On the overworld (new, cheap):** daylight. Birds sing. Music HAHALAND_DAY. `[R21]` A single bird stops. `[R8]` All the birds stop `[!RIPPLE]` on the music's beat. `[TAPESTOP]` `[SILENCE 64]` (the trailer's s03 record scratch). The camera creeps toward the Wall at 0.04 px per frame.
  2. **New insert, "The Gate Cracks"** (the Hellvile side of the Wall, 25 s; Grumbleguts' first scene, §3a). Vapula, a crowned cloud, looms over the sealed gate. Below him, **Gatekeeper Grumbleguts is mid-meal on a mountain of Brimstone Bagels**, back to the camera, rear filling the lower left of the frame.

| f | event |
|---|---|
| 0 | VAPULA: Gatekeeper. {..} The gate. |
| 20 | GRUMBLEGUTS (mouth full): I only had a light snack— `[R5]` a forklift beeps in with the next course `[DROP]` |
| 45 | GURGLE: {d}gurgle. GRUMBLEGUTS: {d}He says hello. |
| 60 | He stands, sucks his gut in to look menacing `[HOLD 13]`. It springs back out `[JELLY]` `{r}` |
| 80 | He unbuckles his belt with a dramatic sigh `[R21]`. To the Gate: Don't be like that. I'll be gentle. `[R8]` The Gate creaks. |
| 110 | Belly-flop from the top of the stairs: `[FREEZE 3 in the air]` `[SHK_ROAR]` `[FLASH r10,7,4]`. The gate **cracks**. |
| 125 | He squeezes through the crack, rear-first. Stuck. `[HOLD 13]` `[BOING]` pop-through `[SQUASH 1.4/0.7]` `{r}` |
| 140 | GRUMBLEGUTS: {d}That was the appetiser. VAPULA: {d}The feast is on the OTHER side. `[ONE CHANGE: the crack in the gate stays for the rest of the game]` |

  3. **CUT_ERUPTION** (as now, re-timed): the sky shifts in bands. The cloud swells, **grins**, and pours through the crack `[SHK_ROAR] [FLASH r10,7,4]`. "THE DEMONS OF HELL ARE HERE." `[SLAM]` caption. `[BUTTON: the Employee-of-the-Month frame falls off the Cloud Nine wall, tink]`
* Chapter 2 ends **at home**: you sprint in, the door slams `[SHK_POW]`, and the pet hides under the bed. Outside the window, red light. "beep."

---------------------------------------------------------------------------------------------------
## 10. CHAPTER 3: HELL INVADES (3.5 h, days 10-12: one long red night, then two more)

**Chapter purpose:** full DOOM. The cute town is a battlefield, but the jokes get *faster*, because panic is funny. Then the midpoint twist and the dark night take the floor out from under it all.
**Music:** INVASION, with SILENCE windows. HAHALAND_DAY plays only inside the church shelter (at a low volume, for the kids).
**Map:** HaHaland-Invaded. Tint is red and pulsing; embers and ash.

### Ep 3.1 "The Sky Ripped" (existing C3_ERUPTION and the start of C3_NESTS)
* Chapter card (embers, title jitter). Then you wake on your floor. The door is buzzing.
* **The run to the church**, a playable chase through the plaza with scripted gags on the route (one per 20-40 s; the overworld rate):
  * a demon pack chases Squeaker the clown; the clown honks; the demons flee from the honk.
  * Chief Spark puts out a fire with a fire hydrant that is itself a demon (it's side quest #21's brute, seen early).
  * Hattie and Grampy, standing in the road, **back to back, perfectly still** `[STILL] [TWITCH]`. They turn their heads toward you in sync. `[MUTE 21]`. Hattie: `{d}"Knock knock."` `[R26]` You run. *(Possessions number three and four, staged and not just told.)*
* **The church shelter.** A pastel island in the red; the kids are watching a shadow-puppet show. The Mayor's speech (as now, expanded):
  > MAYOR: [face SHOCK] The sky RIPPED. Like a cupcake wrapper. A big red cupcake.
  > MAYOR: Everyone's here. Nearly everyone. [face SAD] {..} Steamy's duck is here.
  > FUZZ: Four nests. One spire. And our friends, wearing bad tenants.
  > MAYOR: [face NEUTRAL] And it's fine! {..}{d}It's fine.{punch} `[R13]` (painted on the barricade behind him: "IT'S FINE")

### Ep 3.2 "Board Up the Windows" (new: house defence 1)
**Purpose:** the dream house becomes a fort; Wisp reveals they're friendly.

* Rivet has to weld boards over your windows (a welding verb), and asks for furniture to brace the door.
  > RIVET: Pick a dresser. Not the cute one. {..}Fine. The cute one.
* **House Defence wave 1** (the combat engine on the cottage interior map, a `combat_start_at` on a small map; §13):
  * demons come in through 2 windows and the chimney;
  * furniture is cover;
  * a shelf can mount a turret (Rivet).
* **Mid-wave, the haunting helps:** a dresser slides by itself in front of a breached window `[DROP]` *THUD*. A tiny voice: `{w}"not in MY room."`
* After the wave, Wisp manifests for the first time: a translucent imp kid, half-formed, ears flickering.
  > WISP: [face SHOCK] you can SEE me??
  > WISP: sorry about the chair. i was trying to make it like home.
  > WISP: [face SAD] my home's over the Wall. i cried there so long i... fell out of myself.
  > `[R21]` `[NO BUTTON]`
* From now on Wisp follows you inside the house; in Ch4 they're the guide to Hellvile.

### Ep 3.3 "Four Nests" (existing nests, each one now a set piece tied to a Hell Breach level)
Each nest is an open-world encounter, plus a **descent into a Breach level** that finally gives Breach 3-6 a plot use. Nests can be done in any order; each one ends with a short emotional beat.

| Nest | Set piece | Breach / boss | Character beat |
|---|---|---|---|
| **Plaza (bakery)** | Nana defends her sourdough starter with a rolling pin. Demons are stuck in the dough. | **Breach 3, Sugar Rush Sewers**, Gumdrop Gutterking ("Everything down here is CANDY. Including the floor. Don't lick the floor.") | Nana bakes you a **map into a pie**: the nests' layout. "Eat it first, read it second." |
| **Beach** | Imps kidnap Mr. Flops. Pip follows you (a brave escort with a "Pip courage" meter that recharges when you're near). | **Breach 4, Plushie Factory**, Foreman Fluffnuts, who is turning plushies into demons on a conveyor | Pip catches Mr. Flops in mid-air `[FREEZE]` `[HOLD 13]`. Pip: "I was brave!" `[R8]` "...I need a nap." |
| **Park (Fun Fair)** | The rides are possessed. The ferris wheel spins backwards; the teacups are full of eyes. | Minigame-in-story: **Whack-an-Imp** in the arcade ("Do NOT bonk Father Fuzz." You bonk Father Fuzz.) | Fuzz, rubbing his head: "Seven hundred years of training and I lose to a mallet." |
| **Graveyard** | The joke graves rise as cute ghosts (Mr. Wiggles, Grandpa Puff's moustache, the Great Cake of '89). Mr. Huggins the 1st rises in pieces. | **Breach 5, Graveyard Shift**, The Weeping Widow ("She Lost Her Plushie in 1887") | If Steamy was lost, her ghost is here keeping the dead warm in a bathtub. "It's nice, actually. Cold out." If saved: Steamy fights beside you with a hose. |

* Each nest cleared: the red tint drops one level in that region and the citizens there **come out of hiding with one line each** (the first "town heals" moment).
* With 2 nests left: Rivet's upgrade. The sign now reads 0 and is on fire. `{d}"It's a different fire."`

### Ep 3.4 "Under the Fountain" (MIDPOINT TWIST)
**Purpose:** the truth. This is the hinge of the whole game (~hour 7).

* The last nest is under the plaza. When you clear it, the **Giggling Fountain cracks** `[SHK_CRACK] [FLASH v10,5]`.
* Beneath it: **pipes**. Purple sludge pulses through them toward the Wall, stamped with a hammer mark.
* Rivet goes still. `[STILL Rivet]` `[MUTE]`
  > RIVET: [face SHOCK] {d}...That's Mum's mark.
  > RIVET: [face ANGRY] Who re-plumbed this? This was a GIFT. It was a TWO-WAY fountain!
  > `[R13]` Everyone turns, slowly, `[!RIPPLE at R3 spacing]`, to the Mayor.
* **Flashback F3 "The Plumber's Order"** (40 s, sepia): ten years ago, a young Mayor Giggs in a too-big sash signs a form: "RE: Fountain. Keep giggles IN. Pump glums OUT." The plumber asks, "Out where?" The young Mayor: `{d}"Not here."`
* Back to now. The Mayor's partial confession: the theme refrain, second time.
  > MAYOR: [face SAD] Everyone was so sad after the Mingle. So scared.
  > MAYOR: I just wanted the sad to go... away.
  > MAYOR: {d}We don't talk about the other side.{..} That was the whole point.
  > MAYOR: [face SAD] {d}I never asked where it went.{punch}
  > `[HOLD 26]` `[NO BUTTON]`
  > FUZZ: [face SAD] {..}It went to them. And it grew.
* Dr. Bloom, who has been holding it in, says it out loud **for the first time**: "I'm from over there. I've been here ten years. I was too scared to say." `[R21]` Nana takes her hand. *(The first "say it" moment: the mechanic the finale is built on.)*

### Ep 3.5 "Hattie's Last Laugh" / "Grampy's Garden" (the two story exorcisms, now authored; play in either order)

**Hattie Haha** (true name **HAZE**)
* Hattie is doing stand-up for an empty, burning cafe. She laughs a beat late at every joke.
* **The exorcism is a comedy duel.** The prayer-rhythm phase becomes the **open mic minigame** from Ep 1.4. She heckles you with your own failed beats ("You were late. You were LATE. Hee. Hee."). The true-name clue: "We hate the smoke. We water plants when no one's looking."
* Saved:
  > HATTIE: [face SICK] I was in there the whole time. Laughing on the wrong beat.
  > HATTIE: [face SAD] There was someone else in there too. She hated every second.
  > HATTIE: [face HAPPY] {..} She had great timing, though.
* Lost: Hattie's ghost tells one joke to an empty room. Nobody laughs. `[HOLD 21]` Mascotty claps, once.

**Grampy Bean** (true name **SOOTKIN**)
* In his garden, every tomato has an eye. He doesn't know you. "Who are you? {.} Sonny? {.} Toaster?"
* **The exorcism hides the true-name pick inside a memory game:** you hand him tomatoes and he remembers one detail per tomato ("The seeds came over the fence... from a little mouse-person... xe whistled...").
* Saved:
  > GRAMPY: [face HAPPY] Sunny! You're Sunny! I remember everything!
  > GRAMPY: {..} Where are my glasses?
  > GRAMPY: [face SAD] {d}...The mouse who threw the seeds. I never said thank you.

### Ep 3.6 "The Climb" (existing C3_SPIRE, Hell Breach 2, expanded)
**Purpose:** give Vapula a voice, menace and *motive*.

* `L_all_nests`: "Lord Vapula himself waits at the top. Go with love."
* The Cloud Nine Spire is **seven floors**. On each floor Vapula speaks (combat HUD captions between waves, using `lvd_boss_intro`):
  1. "COME UP, LITTLE SNACK."
  2. "You smell like Hillvile. Like frosting over a frown."
  3. (if you took the puff) "Ooh. I KNOW your flavour." / (if you declined) "You didn't even TRY me. Rude."
  4. (if you asked about the other side in Ep 1.1) "You ASKED. Nobody ever ASKS." `[R21]` "...thank you."
  5. "Every sigh. Every 'it's fine.' Every swallowed scream. That's ME."
  6. "I am every sigh you swallowed. And I am SO. HUNGRY."
  7. Silent. `[SILENCE]` Then a tiny cough. `{d}*cough*`
* **The boss fight** (Breach 2). Vapula's intro card is the `SCENE_BREACH` card. His attacks are the town's own swallowed feelings: "it's fine" pellets and Hee-Hee homing puffs.
* Won: "Lord Vapula, Duke of the Ninth Cloud, lets out one last enormous wheeze..." Then `CUT_VAPULA_DOWN` (re-timed): the deflation `pffffffff` with a pitch slide down. A small grey cloud is left: "...squeak." `[BUTTON +21: it sneezes; it's raining on itself]`
* Celebration at the church: confetti and fireworks. Too early.

### Ep 3.7 "The Leftover Breath" (the start of the dark night)
**Purpose:** the personal cost. The pet's sacrifice.

* `[NIGHT]` At home. Wisp is giddy; the pet is bouncing; the boards are coming down.
* `[R21]` The fog comes in under the door, **one tendril**, slowly. Wisp freezes.
* The leftover breath (the biggest swallowed fear in town, still uneaten) speaks **in the Mayor's voice**:
  > ???: {d}Hee.{..} Hee.
  > ???: {d}What if they hate us. What if they're right to.
* It goes for **you** (the newest, emptiest, most-alone heart in town; it says so).
* **The pet saves you.** @E gets between you and the fog and **inhales all of it**: the s07 inhale, held 5 f, *gulp*.
  `[INSERT 4: the pet's LCD fills the screen: a pentagram, then the Mayor's crying face]`
  `[ONE CHANGE: full horns]`. It looks at you. `[R8]` "beep." `[R13]` It **runs**: out the door, across the red-dark lane, toward the Wall. You chase. The gate is closed. A tiny shape squeezes under it.
* `[SILENCE]` The pet's beep, from the other side, very far away. `[R26]` "beep."
* `[IRIS○ out 10 on the gate]`. `[NO BUTTON]`

### Flashback F2 "Mingle Night" (it plays as you sit at the gate; 60 s; the full version of the 4-frame insert from Ep 1.6)
* The Mingle, eleven years ago: lanterns, both sides dancing. Little Pip as a baby. Rivet with a torch.
* The purple cloud rolls in. Lady Smog breathes it and walks into the kids' tent, saying the kids' fears in their own voices. Young Father Fuzz tries a hug; it fails; she flees into the dark. The kids scream.
* Young Mayor Giggs stands between the two sides, shaking. Nox reaches toward him across the grass. He doesn't take her hand.
  > YOUNG GIGGS: {d}...Close the park.
* `[SMASH]` (no SFX: the only silent smash cut in the game) back to the gate.

### Interlude III "The Empty Room" (THE DARK NIGHT OF THE SOUL; playable; about 8 minutes; no music)
**Purpose:** the lowest point, with the release withheld for as long as the player wants, and then given back by the town.

* You're home. The house is in **EMPTY** phase: furniture burned, given away as barricades, or knocked over. The pet bed is empty. The chair is just a chair; Wisp has gone quiet in a corner.
* **Decorate mode is open but can only remove things.** If you try to place something, the cursor shakes "nope" softly. No error sound. Just nothing.
* `[SILENCE]` The only sound is the clock (Tock's).
* You can sit on the floor (A on the floor). If you do, the camera drifts closer at 0.04 px per frame, as in s03.
* **Then the knocks.** The rule of three, slowed down to a heartbeat: each knock is a separate visitor, R26 or more apart, and the player opens the door each time.
  1. **Nana**, with a lamp. "Light's the first thing. Then soup." She puts it down. She doesn't leave. `[NO BUTTON]`
  2. **Pip**, with Mr. Flops. "He's good at being held. You can borrow him. Not forever. ...Mostly forever."
  3. **Rivet**, with the "Days Since Last Fire" sign, reset to **1**. "Don't make it weird." `[R13]` She makes it weird: she hugs you like a vice.
  4. (if saved) **Steamy**, with a rubber duck. / (if lost) Steamy's ghost, with a rubber duck that is also a ghost.
  5. **Hattie** (or her ghost), with nothing. "I didn't bring anything. I'm the bit."
  6. **Dr. Bloom**, with a blanket. "Cold hands. Warm blanket. That's the deal."
  7. **Mascotty**, who has never been inside anything. She stands in the doorway, perfectly still. `[HOLD 21]` She places a single thing: a tiny Hellvile sunflower seed. Then she leaves.
  * Each visitor places their item themselves, with the DROP animation, and the room fills up. **Music returns one instrument per visitor**, sneaking back in on the grid (timing bible §3.8). By the seventh it's the full HAHALAND_NIGHT theme, slower.
* Last knock: **the Mayor**. He doesn't come in.
  > MAYOR: [face SAD] It was mine. The breath. My fear. Your little @E ate my fear.
  > MAYOR: I'll come with you. Over there.{..} Tomorrow.
  > `[R26]`
  > MAYOR: [face ANGRY] {big}No.{/} Now.
  > `[R8]` MAYOR: [face SHOCK] {d}...Hee. Hee. Oh no. Now.
* `[IRIS♥ out 10 on the full room]` → Chapter 4.
* Saves `home_phase = OPEN`. Every item placed by a visitor is tagged "gift from X" in the catalogue permanently. They can't be sold.

---------------------------------------------------------------------------------------------------
## 11. CHAPTER 4: HELLVILE PARK (3.5 h, days 13-16)

**Chapter purpose:** the other side is *people*, and the monsters were in our pipes. The pet hunt drives the plot; the reunions are the heart; the summit is the climax; and the ending is never undercut.
**Music:** HELLVILE, which **turns out to be the same melody as HAHALAND_DAY in a minor key**. Let the player notice it in Ep 4.6 when the two play together.

### Ep 4.1 "The Gate" (existing C4_GATE, expanded)
* Chapter card (half-purple checker, a beating heart).
* You and the Mayor at the gate. Tallyworn opens it. The gate is a record scratch: crossing triggers `[TAPESTOP] [MUTE 14]`, then the HELLVILE theme on a downbeat, and the camera brakes `[BRAKE]` as Hellvile is revealed.
* Hellvile is grim **and** cosy: bone lamps, soup steam, laundry lines of black lace, kids playing hopscotch on lava with oven mitts. The rule from timing bible §3.3 applies: **hell elements do not wobble. They drip.**
* Tallyworn (heartfelt):
  > TALLYWORN: Day 3,661. Huh. I'll have to start a new wall for the tally marks.
  > TALLYWORN: [face SAD] Some days back... someone shouted my name over the Wall.
  > TALLYWORN: First time in ten years anyone said it. {..}I sat down and had a little cry.
  > (player choice: "That was me." / say nothing)
  > → "That was me": TALLYWORN: [face LOVE] {d}...Oh.{..} Thank you for saying it right. `[NO BUTTON]`
* **The pet's beep**, distant: "beep." From the north, from the Underbelly.

### Ep 4.2 "Shift Swap" (replaces the C4_PRISONERS counter)
**Purpose:** show (don't tell) Hellvile's ten years. Meet Haze; the Hattie payoff.

* The pump yard: rows of Hellvile citizens working bellows that push the vapor around the sky, under the gaze of the remaining loyalists (Hazey and their foghorn).
* **Swap shifts with 5 named workers** (a 30 s bellows rhythm minigame each). While you pump, they get a break and **say one thing**: who they were before the Wall. (This replaces "befriend 5".)
  * Clinker, a chalk poet ("I write poems on the pumps. Forty, so far.").
  * Ploddy, the cloud hauler.
  * Wheezle ("I cough little ghosts out. I name them. One went over the Wall. I called it Sprinkles.") **[seeds side chain I]**.
  * Haze, the clerk who hated the smoke. **Hattie Haha's possessor.**
  * Thumpkin, who thumps 3, then 1, on a pipe "for my sister."
* **Haze meets Hattie** (if she was saved; Hattie came along):
  > HATTIE: You. You were in my head. You have GREAT timing.
  > HAZE: [face SHOCK] {..}You were LOUD in there.
  > HATTIE: [face HAPPY] Knock knock.
  > HAZE: {d}...Who's there.
  > HATTIE: Fresh air.
  > `[R8]` Haze laughs: a snort, the first in ten years `[POP]`. The pumps stop one at a time `[R3 ripple]`.
* Hazey the loyalist blows their foghorn: "LORD VAPULA WILL... um. Is he... coming?" `[R26]` Vapula, the tiny plush cloud, floats by at head height. "...squeak." Hazey, quietly: "...Oh."

### Ep 4.3 "Mother Nox's Three Favours" (existing C4_HELP, expanded: three mini-episodes)
Mother Nox in her chapel kitchen; the potluck is on. She recognises the Mayor, `[STILL Nox]` `[HOLD 21]`, then simply turns back to the soup. (The summit is set up *by not talking*.)

1. **Sootkin's Sunflower**: bring Hillvile flowers for the secret sunflower under the gate.
   **New:** you bring Grampy's tomato seeds too. Sootkin: "...Those are MY seeds. I threw them over ten years ago!" If Grampy was saved, he's there: "The mouse! You whistled!" They whistle together. [NO BUTTON] (Side chain G continues.)
2. **Lady Smog** (the last exorcism, **true name VAPULA**, the oldest scrap of him):
   * Father Fuzz insists on leading. His hands shake. Flashback in `[INSERT 3]` frames: the kids' tent.
   * The ritual is the hardest one: rhythm, water, name. The true-name pick shows **4 names, one of which is "GIGGLESWORTH"** (a red herring, and a gut punch).
   * Saved:
     > LADY SMOG: [face SICK] I haven't tasted air in eleven years.
     > LADY SMOG: [face HAPPY] {big}It's DELICIOUS.{/}
     > FUZZ: [face SAD] Lady Smog, eleven years ago, I—
     > LADY SMOG: You hugged me. {.} Nobody else did. {d}It didn't work. It was lovely.
     > `[R26]` `[NO BUTTON]`
   * Lost (allowed): her spirit drifts out as a cloud of smog in a gown, and **curtsies to Fuzz**. Fuzz plants Mr. Huggins the 8th at her door as a guard of honour.
3. **Anvil and Rivet** (expanded from 6 lines to a full scene):
   * Anvil's house: a table set for four. One plate has never been used. Every night for ten years she set it anyway.
   * Rivet in the doorway, `[STILL]`.
     > ANVIL: [face SHOCK] ...Rivet?
     > RIVET: [face SAD] ...Mum.
     > `[HOLD 21]`
     > `[SMASH]` they hug like two vices closing. Sparks `[SHK_POW]` and the welding mask falls off.
     > RIVET: I saw the pipes. Under the fountain. Your mark.
     > ANVIL: [face SAD] They took my gift and made it a drain. I couldn't stop them.
     > ANVIL: [face HAPPY] {..}I made you a spoon. Ten years ago. For when you came back.
     > RIVET: {d}...It's a REALLY good spoon.
   * Dad Sprocket from the next room: "I didn't read your letters." `[R8]` "...I read them every night." Little Bolt drops a wrench. Rivet's sign: "DAYS SINCE LAST FIRE: 0" (a candle on the table). `[BUTTON: Rivet's tail is on fire again]`
* Help done (fixed so that it reflects Lady Smog's outcome; §12).

### Ep 4.4 "Wisp's Room" (new; the house thread pays off)
**Purpose:** the haunting was a lost kid; your house was a map home.

* Wisp guides you to Hell Pits apartment 13 (Graveley's building; Graveley lowers the rent for families with kids).
* A tiny imp body sleeps in a bed, **exactly** the bed that appeared in your house. Wisp's parents sit beside it, two years in.
* **Housing mission:** Wisp's room was cleared out "to make it less sad". You rebuild it, using decorate mode in *someone else's* home, from the memory of what Wisp kept doing to yours: the chair facing the wall ("my thinking chair"), the rotated rug, the drawing.
  * Each correct placement makes Wisp **more solid** (opacity steps from 8 to 16).
  * A wrong placement: Wisp gently moves it back. "No, it was like THIS." (a "nope" in their voice)
* When it's right: `[MUTE]`. Wisp lies down in their own body. `[HOLD 21]`. Their fingers twitch.
  > WISP'S MUM: [face SHOCK] ...Wisp?
  > WISP: [face SLEEPY] ...i went somewhere. there was a nice house. with a chair.
  > `[NO BUTTON]` *(the stillest beat in Ch4)*
* Reward: Wisp's drawing (a placeable decor item, "gift from Wisp"). You also unlock the **Hell Pits flat**, your second home, next door.

### Dream D2 "The Ninth Cloud" (night in your Hell Pits flat; 90 s)
* You float inside Vapula's memory: a sky full of floating, unsent letters, frowns and "it's fine"s, all drifting the wrong way.
* You can read three of them (A): the Mayor's ("Dear Nox, I'm sorry. I'm sorry. I'm"), Nana's ("I miss the bone-flour bread"), and Fuzz's ("I should have held on longer").
* At the centre, a tiny cloud, crying. `{w}"nobody ever wanted to feel me."`
* Player choice: "I'm sorry" / "I feel you" / (hug). Any of them: it rains, softly. `[IRIS○ out]`. This sets `SF_VAPULA_PLUSH` (it unlocks his post-game chain).

### Ep 4.5 "Find the Doomogachi" (new; Hell Breach 7, the Underbelly)
**Purpose:** the pet's arc, at the bottom of Hellvile.

* Into the Underbelly (**Breach 7**, the Hellvile Underbelly level, story-entered). The beeps get closer.
* **Gatekeeper Grumbleguts blocks the way** (the Breach 7 boss, fought here in the story; mechanics in §3a). He is lounging across the Underbelly gate, rear toward the camera, admiring himself in a puddle of lava.

| f | event |
|---|---|
| 0 | `SCENE_BREACH` boss intro card: bedroom eyes, the bestiary line "Keeper of the Sealed Gate. Also of a very large seat." |
| 30 | GRUMBLEGUTS: Well, hello, new kid. {..} You're cute when you're scared. |
| 50 | (if Fuzz is in the party) GRUMBLEGUTS: Padre. Come up and see my gate sometime. `[R13]` FUZZ: [face SHOCK] {d}No. `[R8]` GRUMBLEGUTS: {d}Playing hard to get. I respect it. |
| 80 | GURGLE: {d}gurgle? GRUMBLEGUTS: Not now. We're being menacing. `[R5]` GURGLE: {d}gurgle. |
| 95 | He unbuckles the belt with the dramatic sigh `[R21]`; sucks the gut in `[HOLD 13]`; it springs out `{r}` `[SHK_POW]` |
| 110 | GRUMBLEGUTS: {big}Nobody passes my gate!{/} {..}{d}Nobody ever tries, honestly. `[SMASH]` fight |

  * **The fight:** belly weak point, belly-flop shockwave (GURGLE warns you first), burp portals in threes, trampoline bounces. Throw a Brimstone Bites dish at the belly to stun him while he eats and rates it. Three dishes and no hits ends it in peace (`USTORY_PACIFIST` route).
  * **Defeated** (he sits down heavily; the gate behind him groans): GRUMBLEGUTS: {d}...Is there dessert? `[R8]` GURGLE: {d}gurgle. `[R13]`
    > GRUMBLEGUTS: [face SAD] There's a THING down there. It beeps.
    > GRUMBLEGUTS: {..} It ate my lunch. {d}Nobody eats my lunch.
    > GRUMBLEGUTS: [face SAD] Ten years I kept this gate. Nobody came up to see it.
    > `[R21]` GURGLE: {d}gurgle. GRUMBLEGUTS: {d}He says go on, kid. `[BUTTON +13: he straightens the harness strap and winks at the Gate]`
  * He lets you pass. From here on he is **friendly** (chain T opens at Brimstone Bites after Ep 4.6).
* At the bottom: @E, horned, huge, a **swollen purple silhouette**. The Mayor's fear is still inside it, and the fear talks in the Mayor's voice.
* **The boss is not a fight. It's a pet-care minigame under fire.** Feed, cuddle and play while the fear lashes out. Each care action shrinks it. Each fear attack is an "it's fine" pellet you have to dodge.
* At the critical moment the Mayor steps in front of you (he followed you down) and says it out loud, **for the third time, in the right way**:
  > MAYOR: [face SAD] That's mine. That's my fear. Give it back, little one.
  > MAYOR: I was scared. Of them. Of me. Of what I did.
  > MAYOR: [face SAD] {d}I'm still scared.{..} You can let go now.
  * The pet exhales: the s07 release, FWOOMP `[COVER 22]`. The cloud clears.
  * The pet is pet-sized again, **with horns** (the one persistent change). `[R2]` "beep." `[R8]` `{d}...it's fine.`
  * **This time it is fine.** `[R13]` A real "beep!" with a hop.
* `SF_PET_HORNS` is permanent. The pet's portrait gains horns in every scene.

### Ep 4.6 "The Summit" (existing C4_MAYOR, expanded; the climax)
**Purpose:** the Wall comes down, because everyone says it.

* The Mayor walks the plaza, crosses through the gate, and walks up to Nox's kitchen.
* **Flashback F4 "Cupcakes and Bone Soup"** (20 s, sepia): two kids on a blanket at the Mingle swap food. Nox: "Soup's better." Giggs: "...Cupcakes are better." Both: "...Both are better."
* **The summit:**
  > MAYOR: [face SAD] Nox. {..} I brought cupcakes.
  > NOX: `[STILL]` `[HOLD 21]` {d}...I brought bone soup.
  > MAYOR: [face LOVE] `[R8]` ...I LOVE soup.
  > NOX: I know. {..} I kept making it. Every Mingle. For ten years.
  > MAYOR: [face SAD] I was scared. I'm still scared.{..} Hold my hand?
  > `[HOLD 26]`. She takes it. `[NO BUTTON]`
  > NOX: Then I believe we have nothing left to build a Wall for.
* **"Say It"**, the finale mechanic: everyone walks to the Wall. The **Giggling Fountain is re-plumbed back to two-way** (Rivet and Anvil weld together; it's a two-player welding QTE you do as Rivet's hands).
  * The crowd lines up along both sides of the Wall. Each citizen you've befriended says **one unsaid thing** when you walk past (a hash-picked line from a pool of ~60, plus authored ones for the cast): "I'm scared of the dark." "I miss my brother." "I never liked grape."
  * Each one sends a **giggle bubble** through the fountain to the other side. The bubbles stack up against the Wall.
  * Last: **you**. A player choice: "I was lonely." / "I was scared too." / "I like it here." / "I'm home."
* **`CUT_WALL_FALLS`** (the game's best existing animation, now on a beat script):
  * `[SHK_RUMBLE 12]`; bricks fall with staggered delays; dust puffs.
  * **HAHALAND_DAY and HELLVILE play at the same time and are revealed to be the same tune** (major and minor resolving to one chord).
  * The cast runs in from both sides. Hearts rise.
  * **This beat is never undercut.** `[NO BUTTON]`. No beep. No "it's fine". The timing bible §5.4: the button goes after the credits.
* Mascotty walks into the rubble and **takes off her head** for the first time. Underneath is a small, tired bunnykin. Thumpkin thumps, once. She thumps back, once. They hug. `[HOLD 26]`

### Ep 4.7 "Hellvile Park" (existing C4_FESTIVAL and C4_STAGE; the ending)
* The united park; the festival. Instead of "chat with 4 people", there are **4 festival stalls you run** (cooking with Nana and Charlotte, fishing at the Lake of Fire, the photo booth with Shutterbug, the comedy tent with Hattie). Each takes about 3 minutes and ends with a callback cameo.
* **Hattie's set**, the third refrain:
  > HATTIE: So. {..} We don't talk about the other side.
  > `[R13]` (beat) HATTIE: [face HAPPY] {big}Because we're STANDING in it!{/}
  > `[!RIPPLE]` both sides laugh. Haze snort-laughs. The Mayor goes "Ha!" `[FREEZE ALL]` Everyone stares. "...Ha?" `[THAW]` They laugh harder.
* **Sprinklepop's soul** (if side chain I is done) bounces in holding a heart balloon. Otherwise she bounces in anyway, with the map still upside down.
* **Grumbleguts at the buffet.** Back to the crowd, rear framed front and centre, the table emptying around him. Every time the camera "accidentally" cuts back to him he is bigger `[3x]`. Honkers the clown offers him a napkin; he flirts with the napkin. The napkin is not interested. (This establishes the fullness gag that S186 pays off.)
* **The Unity Stage** speeches (existing, expanded):
  * the Mayor;
  * Nox;
  * Pip ("And Mr. Flops!" Mr. Flops has felt horns now);
  * Fuzz ("Even you, Vex");
  * Vex ("plain water vapes now. They're terrible.");
  * Rivet ("Hit it!");
  * then Tallyworn, who adds a last line: "Day one."
* **The credits parade** (as now): 500 pairs; the cast cheers; ghosts march "in loving memory". **New:** the parade is led by you, the pet (with horns) and Wisp (in a body). Grumbleguts' credits cameo: at the roadside, still eating, the buffet table empty around him; he waves a drumstick at the camera, then at the Gate.

---------------------------------------------------------------------------------------------------
## 12. EPILOGUE: "HAPPILY EVER AFTER (MOSTLY)" (0.5 h)

* **E1, the post-credits button** (the trailer's s15, in the game): back at your house. Pet, horns. Strain (8 f), plop. `[R8]` The poop grows horns `[POP]`. "beep." `[IRIS♥ out 10 on the poop]`. *THE END... of the beginning!*
* **E2, "The Suitcase"** (playable, at home): you can finally unpack the suitcase from Ep 1.1. Inside is a single empty photo frame. Shutterbug's festival polaroid goes in it. Place it anywhere. `[NO BUTTON]` This unlocks building on the **Park Plot** (DREAM phase).
* **E3, "Population: 1000"**: the sign at the town entrance. You repaint it (side mission S143); the scratched "1000" was the original number. `[BUTTON: the "0" falls off, tink. Rivet: "Don't make it weird."]`
* **The title screen changes** after the credits: the logo's HELL half is on fire. `{d}(it's fine.)`
* **E4, "The Feast of Rebuilding"** (post-game quest **S186**, §13): Grumbleguts gathers everyone you know in Hellvile Park and begs for a feast "for energy, to rebuild faster". One guest is possessed. One more exorcism. A fullness meter. Two endings, both unlocks, one of them permanent. It is the only post-game beat that can change the town forever, and it is the player's choice.

### Post-game and New Game+ ("HaHaland Remembers")
* Free roam of the united park. All post-game side content unlocks (§13).
* **NG+** (finally implemented; `MODE_NEW_GAME_PLUS`): replay Ch1-4 with:
  * the pet's horns from the start;
  * the gag props kept in your house;
  * **the wrongnesses are now readable**: the fountain's late giggle is subtitled "(someone over there just laughed)"; Mascotty's thumps are subtitled;
  * new NG+-only lines where the cast almost remember ("Have we... done this before? {d}Hee. Hee.").

---------------------------------------------------------------------------------------------------
## 12a. Continuity fixes and choice consequences this bible depends on

| Item | Fix |
|---|---|
| `SF_VAPE_SAMPLE` | Read in Ep 3.6 (Vapula's floor 3 line) and Ep 3.7 (the fog speaks to you "by flavour"). |
| `SF_WEAPON_BRUTAL` | Rivet's lines (Ep 3.2: "Did you name it? You named it."); house-defence turret skin; Fuzz's "tsk." judgement caption, rate-limited. |
| `SF_STEAMY_LOST` | Ep 3.3 graveyard nest, the Interlude III visitor, side chain H. |
| Smog lost or saved | `L_help_done` must branch (today it says "breathing" either way). |
| `SF_VEX_REFORMED` / `SF_VAPULA_PLUSH` | Used by side chains K and L. |
| `USTORY_PACIFIST` | Granted for finishing the house-defence waves and Ep 4.5 without the brutal weapon equipped. |
| Pronouns | Steamy is she (roster). `L_steamy_lost` says "him": fix it. Lagspike: pick one. Rivet is she. |
| Chapter titles | Keep the code titles; update `docs/STORY.md`. |
| Quest #9 / #16 / #26 titles | Retitle: "Pier Panorama" (9), "Streamer Scream" (16), "Spider Ride" (26), so "Graveyard Shift" stays Breach 5. |

---------------------------------------------------------------------------------------------------
## 13. NEW SIDE MISSIONS (96, S100-S195), on top of the 49 existing ones

**How to read a mission.** `Sxxx "Title"` | giver (roster id, district) | chapter window (5 = post-game) | gameplay verbs. Then the hook, 3-6 beats, the comic beat with timing, the heartfelt or scare beat, and the reward. Furniture rewards go into the house catalogue (§14.5) tagged "gift from <giver>" and cannot be sold. Every mission has **a button that fires later**: a line, a prop or a sighting 1-10 minutes after turn-in (timing bible §5.1); it is listed as "Later".

**Design rules for all 96:**
1. One mission = one trailer scene's anatomy (setup, escalation, punch, button), stretched over minutes of play.
2. One gag per 3-6 lines of dialogue; at most one `{d}` deadpan per conversation.
3. A scare mission has the release withheld once, never twice in a row.
4. Chains advance one part per chapter at most, so the town keeps changing as the story does.
5. Hillvile givers in Ch3 also stand on the invaded map (inside or beside the church). Hellvile givers open from Ep 4.1, post-game ones from the credits.
6. Cross-border chains need both sides to be reachable: parts 1-2 in Ch1-3, part 3+ in Ch4-5.

**Verb key:** talk, fetch, deliver, photo, find, combat (overworld nest or Breach level), exorcism, weld (workshop), pet, minigame (Fun Fair), fishing, cooking, concert, studio, housing (decorate your home or someone else's), creator (design a look for a citizen), escort, timing (comic-timing minigame), dream.

### 13.1 Chains (21 chains, 69 missions)

#### Chain A: Pip and Mr. Flops (Hillvile; 3 parts)

**S100 "A Bed for Mr. Flops"** | Pip (42) | Ch1 (after Ep 1.3) | housing, fetch
* Hook: Mr. Flops "has nightmares about the beach". Pip asks you to build him a bed in *your* house, "because yours has a door that locks".
* Beats: 1) Fetch 2 Scrap and a Plushie. 2) Rivet isn't in town yet, so Nana sews it: a shoebox with a tea-towel blanket. 3) Place it in your house (housing tutorial, part 2: a *tiny* item). 4) Pip inspects it and rotates it twice to face the window, then away from the window. 5) Mr. Flops is tucked in.
* Comic beat: Pip's three inspections `[3x@15]`: "Too close to the door." "Too close to the window." `[R13]` "{d}...Too close to the floor." `[BUTTON +13: the box slides 1 px]`
* Heart: Pip, leaving: "He sleeps better when someone else is in the room. {..}So do I." `[NO BUTTON]`
* Reward: 30 H$, **Mr. Flops's Bed** (furniture; in Ch2 Wisp moves it nightly). Later: in Ep 2.3, the bed is the one piece the haunting never touches.

**S101 "Night Patrol"** | Pip (42) | Ch2 | escort, photo, timing
* Hook: Pip has made a "Wall Watch" badge from a bottle cap and wants to patrol Hillvile Lane after dark "to prove monsters aren't real".
* Beats: 1) Meet at the fountain at night. 2) Walk 4 waypoints; Pip's courage meter drains in the dark and refills when you stand close. 3) At the third lamp post (W5, the one that faces the Wall) Pip hides behind you. 4) Photograph "proof there's nothing there". 5) The photo shows two red dots in the fog, far away.
* Comic beat: Pip examining the polaroid `[HOLD 13]`: "Those are... fireflies. Big, {.}red, {.}fireflies." `[R8]` "Patrol's over." They sprint home at the flee curve, dragging you.
* Scare beat: on the way back the fog giggles **in Pip's voice**. Pip doesn't notice. You do. `[MUTE 21]` `[NO BUTTON]`
* Reward: Wall Watch Badge (decor), 40 H$. Later: Miss Lulla says Pip told the daycare "fireflies can be red now".

**S102 "Flops the 1st Rides Again"** | Pip (42) | Ch3-4 | combat (Breach 4), fetch, housing
* Hook: after Ep 3.3 (beach nest), Mr. Flops is back but his twin from the Plushie Factory conveyor, "Flops the 1st", is still in there, half-demon, half-bunny.
* Beats: 1) Re-enter Breach 4 (Plushie Factory). 2) On the top floor the twin hops away every time you aim; stop aiming and it comes close. 3) Catch it with the pet (pet verb: @E sniffs it out). 4) Rivet welds the two torn ears back on (weld). 5) Pip adopts the twin "as Mr. Flops's brother, because everyone should have one".
* Comic beat: the twin has one demon eye. Pip covers it with a sticker. `[R8]` The sticker blinks. `[R13]` PIP: "{d}He's expressive."
* Heart: Pip gives *you* the twin to keep in your house: "For when I'm not here to be brave at you." Horned Flops sits by the pet bed from then on. In Ep 4.7 Mr. Flops's felt horns are explained: Pip made them "so his brother doesn't feel weird".
* Reward: **Flops the 1st** (furniture, animated: blinks every 83 frames), 80 H$, unlock Keepsake. Later: Sutura (542) in Hellvile asks who did "that beautiful ear work".

#### Chain B: Rivet, Anvil, Sprocket and Bolt (cross-border; 4 parts)

**S103 "Days Since Last Fire"** | Rivet (13) | Ch2 (after Ep 2.6) | weld, housing
* Hook: the fire marshal (Chief Spark) says Rivet's sign is "a safety hazard because it's always zero". She asks you to hang a second one at your house, "so the average goes up".
* Beats: 1) Weld the sign (a 3-part weld minigame; a spark lands on your pet, who says "beep." and is fine). 2) Place it in your house. 3) It counts real in-game days. 4) Rivet visits to "check the hinge" and her tail is on fire. The sign flips 7, "-", 0 `[SLAM]`.
* Comic beat: the flip is the trailer's s09 beat: 7, two frames of jittery "-", red 0 pulsing every 4 frames. Chief Spark, from offscreen: "{d}I heard that."
* Heart: Rivet, fixing the hinge, says the sign in her mum's forge "read 0 every day and we were proud of it". `[R8]` "Don't make it weird."
* Reward: **Days Since Last Fire sign** (furniture; resets whenever Rivet visits). Later: the day it reads 7 for the first time, Rivet sends a text: "a personal best. don't make it weird."

**S104 "Letters Not Sent"** | Rivet (13) | Ch3 (in the church shelter) | find, deliver
* Hook: Rivet has written her parents a letter every month for ten years and posted none of them. They're in a toolbox. The toolbox is in her workshop. The workshop is in a nest.
* Beats: 1) Fight through the plaza to the workshop (combat on the invaded map). 2) The toolbox is being used as a demon's stool. 3) Bring it back. 4) Rivet reads one aloud, stops at line two. 5) Ask her to carry them over the gate in Ch4 (she says no; she says yes).
* Comic beat: 120 letters all start "Dear Mum and Dad, don't make it weird." Rivet: "{d}I had a format."
* Heart: letter #1, aged 14: "I saw what they did with the fountain. I ran because I couldn't fix it. I'm learning to fix things." `[HOLD 21]` `[NO BUTTON]`
* Reward: 60 H$, Rivet's Toolbox (furniture). Later: Ep 4.3, Sprocket "didn't read your letters" / "read them every night" lands harder because you know what's in them.

**S105 "Two Marks"** | Anvil (595, Hellvile) | Ch4 (after Ep 4.3) | weld, fetch
* Hook: Anvil wants to re-stamp the fountain pipes with a **new** hammer mark, hers and Rivet's together, before Ep 4.6 re-plumbs them.
* Beats: 1) Fetch 3 Scrap from the Hellvile salvage yard (Salvagia 553 haggles). 2) Design the mark: a 4-choice picker (an anvil, a rivet, both crossed, a heart). 3) A two-handed weld minigame: you are Rivet's hands, Anvil calls the rhythm. 4) Stamp the first pipe. The sludge in it runs **clear** for one second.
* Comic beat: Anvil tests the weld by hitting it with a spoon. "{d}It's a REALLY good spoon." `[R8]` Rivet: "Mum." Anvil: "It IS."
* Heart: Anvil keeps the old mark too: "I don't want to forget what they did with it. I want to remember what we did after."
* Reward: 100 H$, Anvil's Hammer Mark (a decor stamp that appears on any furniture you weld from now on), **Forge-Iron Bed** (furniture). Later: in Ep 4.6 the welding QTE uses the new mark.

**S106 "Bolt's First Weld"** | Bolt (597, Hellvile, KID) | Ch4-5 | weld, escort, housing
* Hook: Rivet's little sibling wants to weld "like her" and has stolen a torch. Rivet is terrified. Anvil says "let xem".
* Beats: 1) Escort Bolt to the forge without them lighting anything (they light three things; Chief Spark, now cross-border, follows with a bucket). 2) Teach the weld minigame with the difficulty capped. 3) Bolt makes a lopsided stool. 4) Bolt wants it to live in **your** house "because yours gets visitors".
* Comic beat: Bolt's "DAYS SINCE LAST FIRE" sign, hand-painted, reads 0 before they start. Rivet: `[HOLD 13]` "{d}...That's my kid."
* Heart: Rivet watches Bolt weld and realises she is standing exactly where she stood at six, holding the torch for Anvil. She doesn't say anything. Anvil does: "Look at you. You're the one holding the light now." `[NO BUTTON]`
* Reward: **Bolt's Stool** (furniture; wobbles, `TW_WOBBLE`), 50 H$, Grumpy Uncle sticker. Later: Grumbleguts to Bolt: "Eat your vegetables. {d}Not those. Those are mine."

#### Chain C: Mayor Giggs (Hillvile, then cross-border; 3 parts)

**S107 "Speech Notes"** | Mayor Giggs (0) | Ch1 | find, talk
* Hook: the Mayor has lost his welcome speech. He has given it 400 times. He can't remember a word.
* Beats: 1) Find the notes: at the fountain, the bakery, and finally in Mascotty's paw (she won't let go; stand still R21 and she opens the paw). 2) Read them: every page says "Hee. Hee." in the margin where a sentence used to be. 3) Return them. He reads the speech. It is one sentence long.
* Comic beat: the speech: "Welcome to HaHaland, where {.}{.}{.}{d}hee. hee." `[HOLD 13]` MAYOR: "It kills every time."
* Scare beat: the last page of notes, in different handwriting: "You never asked where it went." The Mayor folds it very quickly. `[MUTE 14]` "Old draft."
* Reward: 20 H$, Framed Speech (furniture). Later: Ep 3.4, the plumber's order is in the same handwriting as that page.

**S108 "The Locked Drawer"** | Mayor Giggs (0) | Ch2 | find, fetch, talk
* Hook: Town Hall's bottom drawer is locked and "makes a noise". He wants you to open it so he doesn't have to.
* Beats: 1) Keysie (21) has a spare key for everyone but says the Mayor asked her to lose this one. 2) Fetch her 1 Coffee and she "finds" it. 3) Open the drawer: inside, a child's drawing of two kids on a blanket, a jar of bone-flour, and a half-eaten cupcake, eleven years old, perfectly preserved (Nana's recipe). 4) The noise is the cupcake. It giggles.
* Comic beat: the cupcake giggles R8 late, like the fountain. The Mayor: "{d}It's a fresh one." `[R13]` "{d}It is not a fresh one."
* Heart: the drawing is signed "G + N". He puts it back. He doesn't lock the drawer this time. `[NO BUTTON]`
* Reward: 40 H$. Later: the drawing appears on the Mayor's office wall in Ch4, framed.

**S109 "The Apology Tour"** | Mayor Giggs (0) | Ch4-5 | escort, talk, cooking
* Hook: after the summit, the Mayor wants to apologise to every Hellvile citizen personally. There are 500. He has baked 500 cupcakes. He has brought a wagon.
* Beats: 1) Escort the wagon through Hellvile; three stops are scripted (Tallyworn, Lady Smog, the pump yard). 2) At each, the Mayor starts a speech and gets interrupted by someone accepting a cupcake and saying "yeah, fine". 3) The wagon gets lighter; his sash gets straighter. 4) At the last stop, Honkers the clown (501) honks at him. He laughs. "Ha!" `[FREEZE ALL]`.
* Comic beat: 500 Hellvile citizens, zero speeches finished. Spreadsheet (531) hands him an itemised receipt for "apology, partial, x1".
* Heart: Tallyworn refuses the cupcake and asks for a handshake instead. The Mayor holds it a beat too long `[HOLD 21]`. "...Day one," says Tallyworn.
* Reward: 150 H$, **Mayoral Sash** (decor), unlock "Hee. Hee." jingle for the jukebox. Later: the fountain's giggle is now exactly on time. Forever.

#### Chain D: Nana Puddin, Charlotte and Scorchmello (cross-border; 4 parts)

**S110 "The Secret Ingredient"** | Nana Puddin (1) | Ch1 | cooking, fetch
* Hook: Nana's melon bread "isn't right" and hasn't been for ten years. She wants you to taste-test five batches.
* Beats: 1) Cook with Granny (the cooking scene, melon bread recipe). 2) Taste 5 variants `[3x@15]` plus two: "sweeter", "saltier", "more melon", "less melon", "{d}...the same". 3) Nana admits the missing ingredient is bone flour. "Can't get it anymore. Don't ask why."
* Comic beat: batch 5 is identical to batch 1. Nana: "I know. I just like watching you eat."
* Heart: the recipe card has a second name on it, scribbled out. "Charlotte and I used to bake together. {..} Different sides of the park." `[R13]` "Eat up."
* Reward: 30 H$, Melon Bread x3, Nana's Recipe Card (decor). Later: in Ch4 Charlotte (566) has the other half of the card.

**S111 "Muffin Recall"** | Nana Puddin (1) | Ch2 | fetch, find, talk
* Hook: a batch of muffins came out purple. Nana sold six before she noticed. Get them back before anyone eats one.
* Beats: 1) Six buyers around town; each one is one step from biting. 2) The last one, Zoomzoom (8, speedrunner), has already eaten it in under four seconds. 3) Zoomzoom now speaks in "we" for a day (W7).
* Comic beat: every buyer you reach is mid-bite `[FREEZE]` `[HOLD 8]` as you shout. Oma Fluff (27): "I was only looking at it."
* Scare beat: the recovered muffins, back in Nana's basket, are warm. Nana: "{d}I baked those yesterday." She puts the lid on. The lid lifts once, by itself.
* Reward: 40 H$, Purple Muffin (item; feeding it to the pet gives horn nubs early if Interlude I hasn't happened). Later: Zoomzoom runs a "we" joke at the festival.

**S112 "The Other Half of the Card"** | Charlotte (566, Hellvile) | Ch4 | deliver, cooking
* Hook: Charlotte, who bakes cookies that are 90% charcoal and "loves you", has a recipe card torn down the middle. Nana has the other half.
* Beats: 1) Carry the half to Nana. 2) Nana goes very still `[STILL]` and tapes them together. 3) Cook both halves at once: melon bread with bone flour. 4) Carry the loaf to Charlotte. She eats it burnt. "Perfect."
* Comic beat: Charlotte burns the perfect loaf on purpose so it "tastes like home". Nana, arriving: "{big}CHARLOTTE.{/}" `[R8]` CHARLOTTE: "{d}It's a texture."
* Heart: the two old bakers hug with flour on them. Nana: "Ten years I baked it wrong on purpose. {..} So I'd remember it was missing something." `[NO BUTTON]`
* Reward: 80 H$, **Two-Sided Recipe Card** (decor), Bone Flour (ingredient unlocked at the bakery). Later: the bakery's sign gets a second line: "& Charlotte".

**S113 "Bake for a Thousand"** | Scorchmello (669, Hellvile) + Nana (1) | Ch5 | cooking, fetch, minigame
* Hook: the Unity Loaf for the first Mingle in eleven years: one loaf, one thousand slices. Three bakers, one oven, zero agreement about toast.
* Beats: 1) Fetch ingredients from both sides (Melon, Bone Flour, Charcoal, Marshmallows). 2) The cooking minigame at triple speed with three "grannies" shouting instructions. 3) Scorchmello toasts the crust black; Nana scrapes it; Charlotte puts it back. 4) The loaf is cut on the Unity Stage.
* Comic beat: inside the loaf is a key `[POP]`. Everyone turns to Nana. NANA: "{d}Habit."
* Heart: the key is to the gate. Tallyworn hangs it on the ruined gatepost, where there is no lock. "In case anyone's ever scared again. {..} So they can see it's open."
* Reward: 200 H$, **Unity Loaf** (furniture: a loaf under glass; the pet sniffs it), unlock "Three Bakers" trophy. Later: Grumbleguts asks about the leftovers. There are none. He asks again.

#### Chain S: Hillvile Towers, apartment 7B (Hillvile; 3 parts; the second home)

**S114 "Apartment 7B"** | Keysie (21) | Ch2 | find, housing
* Hook: Keysie holds a spare key for everyone, including an apartment nobody has rented for ten years. The last tenant "moved to the other side of the park". The rent is "whatever you've got".
* Beats: 1) Climb Hillvile Towers (the facade browser becomes a door). 2) 7B is furnished in dust; the windows face the Wall. 3) Clear it (housing: remove mode) and find the tenant's things: a Hellvile bone lamp, a photo of a mousekin with a tally chalk. 4) Keysie: "Keep what you find. They won't be back." 5) You decorate your second home.
* Comic beat: the dust-sheet over the sofa is shaped like a person `[HOLD 13]`. Pull it off: a sofa. `[R8]` The sheet, on the floor, is still shaped like a person. `[R13]` Keysie: "{d}It does that."
* Scare beat: the tally chalk under the windowsill has 3,651 marks. The last one is fresh. `[MUTE 21]` (The tenant is Tallyworn. He was on this side when the Wall closed, and crossed back for his family. Nobody told anyone.)
* Reward: **Hillvile Towers 7B** (second home), Bone Lamp (furniture). Later: Ep 4.1, Tallyworn: "I had a flat on your side once. Faced the Wall. {d}Bad view."

**S115 "Neighbours"** | Gusty (92) and Curlicue (44), the 7A and 7C tenants | Ch2-3 | housing, talk, fetch
* Hook: the Towers have a "cosiest floor" contest. 7A and 7C have never won. 7B has never competed. They both want you as an ally against the ninth floor.
* Beats: 1) Reach a Cosy rating of B in 7B (housing goals). 2) Each neighbour lends an item for the contest and wants it placed "where I can see it from my door". 3) The judge is Mascotty. She stands in each doorway `[STILL]` for R21 and then gives a thumbs-up or nothing. 4) The ninth floor wins again. It is a single ghost with one candle.
* Comic beat: Mascotty judges 7B `[HOLD 21]` and, for the first time ever, nods **once**. Gusty faints. Curlicue: "{d}She likes the rug."
* Heart: the ghost on the ninth floor is Rattlecup (507), a Hellvile poltergeist who drifted over years ago and "didn't want to be a bother". She leaves biscuits. Nobody had ever gone up to say thank you.
* Reward: 60 H$, Rattlecup's Teacup (furniture; rattles at midnight, leaves a biscuit item once a day). Later: Rattlecup joins the Hellvile side of the "Say It" line in Ep 4.6: "I'm not a bother."

**S116 "Room With a View"** | Keysie (21) | Ch4-5 | housing, photo, talk
* Hook: with the Wall gone, 7B's window faces Hellvile Park. Keysie wants a photo of the view for "the brochure" (there has never been a brochure).
* Beats: 1) Photograph the view at dawn (the day/night clock). 2) Decorate 7B for a Hellvile guest: Tallyworn is coming to see his old flat. 3) He sits in the window for R26. 4) He asks to leave one thing: the tally chalk, on the sill.
* Comic beat: the brochure Keysie prints has one photo and one line: "Hillvile Towers: now with a view." `[R8]` "{d}It used to say 'now with a Wall'."
* Heart: Tallyworn, at the window: "Three thousand, six hundred and sixty-one marks. {..} I'll leave the chalk. Someone else might need to count something." `[NO BUTTON]`
* Reward: 100 H$, **Tally Wall** (decor: a wall panel of chalk marks, the last one a heart), unlock Landlord trophy. Later: the brochure is a readable prop at the Town Hall.

#### Chain E: Father Fuzz and the Mr. Huggins succession (Hillvile, then cross-border with Sutura; 4 parts)

**S117 "Buckwheat Run"** | Father Fuzz (77) | Ch2 (after Ep 2.4) | fetch, talk
* Hook: Fuzz needs stuffing for Mr. Huggins the 3rd. Love he has. Buckwheat is at the farm, and Farmer Bram (120) only talks to his carrots.
* Beats: 1) Convince Bram by complimenting a carrot (a 3-option pick; all three work; the carrot blushes). 2) Fetch 2 Buckwheat. 3) Watch Fuzz stuff the 3rd: a 20-second sewing minigame (the plush-making hobby). 4) Mr. Huggins the 3rd sits up. Fuzz hugs him. His head falls off.
* Comic beat: `[SLAM]` the head rolls three times and stops facing the Wall (as the 2nd did). Fuzz: "{d}They all do that." `[R13]` "{d}I've stopped asking."
* Heart: Fuzz keeps every destroyed Huggins in a drawer "because nobody is practice". He names them all.
* Reward: 30 H$, Mr. Huggins the 1st's Grave Marker (decor, tiny). Later: the drawer is visible in the church in Ch3; the kids have drawn faces on the heads.

**S118 "Holy Water, Hold the Water"** | Father Fuzz (77) + Sr. Lumen (354) | Ch3 | fetch, exorcism (practice), timing
* Hook: the holy water has run out mid-invasion. Sr. Lumen's scrolls say any water will do "if blessed by someone who means it". Fuzz doesn't believe he means it anymore.
* Beats: 1) Fetch water from the bath house (if Steamy was saved, she fills it; if lost, her ghost does, and it comes out warm). 2) A blessing minigame: the prayer-rhythm phase with the metronome muted; you keep the beat yourself. 3) Test it on Mr. Huggins the 4th. He survives the first two parts `[3x@15]`. 4) He loses an ear on part three. Fuzz counts it as a win.
* Comic beat: the water, once blessed, giggles. Fuzz: "{d}That's fine. {.} Is that fine?" LUMEN: "{d}The scroll says 'probably'."
* Heart: Fuzz, quietly, before part three: "Eleven years ago I let go too early. {..} Hold the beat for me." `[NO BUTTON]`
* Reward: Holy Water x3, 50 H$. Later: Ep 4.3, Fuzz keeps the beat alone.

**S119 "The Plush Surgeon"** | Sutura (542, Hellvile, night nurse) | Ch4 | deliver, fetch, housing
* Hook: Sutura stitches up the plushies of crying kids, free, at night. Xe has heard of "a priest who destroys one a week" and wants a word.
* Beats: 1) Deliver Fuzz's drawer of Huggins heads across the gate (a heavy, rattling item; the heads mutter). 2) Sutura mends them overnight: a sewing minigame, night only. 3) Return with **Mr. Huggins the 1st through 8th, repaired**, in a row. 4) Fuzz cannot speak for R26.
* Comic beat: all eight heads turn to face the Wall at once `[!RIPPLE]`. The Wall is gone. They turn back, confused. `[R8]` SUTURA: "{d}Muscle memory."
* Heart: Sutura: "Nobody's practice. {..} That's why I don't charge." Fuzz has been saying the same thing for ten years to an empty church. `[HOLD 21]`
* Reward: **The Huggins Choir** (furniture: eight plush heads on a shelf; they hum SONG_CHURCH at night), 100 H$. Later: the church's pews fill with Hellvile kids holding mended plushies.

**S120 "Mr. Huggins the 9th"** | Father Fuzz (77) | Ch5 | exorcism, talk
* Hook: Fuzz wants to do one more practice exorcism, in public, at the festival, with Sutura's stitching and Rivet's rivets. "If this one survives, I'll believe it."
* Beats: 1) Gather a crowd (talk to 5 festival-goers; each one has a reason not to come and comes anyway). 2) The practice ritual, all three parts, for real odds. 3) Mr. Huggins the 9th survives. 4) He hugs Fuzz. `[HOLD 26]`
* Comic beat: the crowd flinches at every part `[!RIPPLE]`. After part three, silence `[MUTE 21]`. Honkers honks. Everybody screams. Mr. Huggins is fine.
* Heart: Fuzz, holding a dummy that has not fallen apart: "{d}Oh. {..} Oh, that's what it feels like." Lady Smog, from the front row: "Told you it was lovely." `[NO BUTTON]`
* Reward: **Mr. Huggins the 9th** (furniture; hugs the pet when it passes), unlock "Nobody Is Practice" trophy, 150 H$. Later: the grave sign for the 1st gains a second line: "Came back. Hugged a priest."

#### Chain F: Lilypad Lou and the two-way mail (cross-border; 3 parts)

**S121 "Return to Sender"** | Lilypad Lou (97) | Ch1-2 | find, deliver
* Hook: your mailbox keeps getting junk mail "from The Other Side" (the existing gag). Lou wants it back: "Undeliverable mail is a personal failure."
* Beats: 1) Collect 5 letters from your mailbox over 3 days (the calendar). 2) Read them: coupons for "Cloud Nine Vapor: opening soon!", a tax notice from Taxfang, a chain letter that says "we". 3) Lou tries to return them to the Wall. The Wall doesn't have a slot. 4) She posts them through a crack. Something posts one back.
* Comic beat: the returned letter is addressed to "THE SNACK, Pink Door Cottage". Lou: "{d}Well. It's a valid address."
* Scare beat: inside: a single tally mark. `[MUTE 14]`
* Reward: 30 H$, Cursed Coupon (decor). Later: in Ch4 Meterman (508) tries to redeem the coupon.

**S122 "Dead Letter Office"** | Lilypad Lou (97) | Ch3 | find, combat, deliver
* Hook: the post office is a nest. There is a sack of letters inside, addressed from Hillvile to Hellvile, ten years old, never sent. Lou didn't know it existed.
* Beats: 1) Clear the post office (an overworld nest encounter). 2) Carry the sack to the church. 3) Lou reads the addresses: Nana to Charlotte, Giggs to Nox (90 of those), Fuzz to Smog, Grampy to "the mouse who whistles". 4) She decides to deliver them. All of them. In Ch4.
* Comic beat: the demon guarding the sack is using it as a pillow and does not want to fight. It offers a letter as a bribe. Lou accepts the letter and the fight.
* Heart: Lou finds one addressed in her own handwriting, to a frog in Hellvile called Warty (912). "...I was eleven." `[R13]` "{d}I'm not reading that one out."
* Reward: 60 H$, Mail Sack (decor). Later: Confetti's Frog Wedding (#44) now has a backstory.

**S123 "First Post"** | Lilypad Lou (97) + Mandible (528, Hellvile skeleton clerk) | Ch4 | deliver, talk, escort
* Hook: Lou delivers the ten-year-old sack. Mandible, who clacks his jaw at people who are late, runs Hellvile's post and considers the letters "ten years late".
* Beats: 1) Escort Lou through Hellvile with the sack; 6 scripted deliveries, each a tiny scene (Nox reads one of Giggs's 90 letters and puts the other 89 in a drawer, unread, "for later"). 2) Mandible clacks at every one. 3) The last letter is Lou's own, to Warty. She hands it over. Warty sings the reply to the moon.
* Comic beat: Mandible's jaw clacks `[3x@15]` get faster as they approach his counter, then stop dead when Lou stamps every letter "ON TIME". `[HOLD 13]` MANDIBLE: "{d}...Acceptable."
* Heart: Nox reads letter #1 of 90: "Dear Nox. I'm sorry. I'm sorry. I'm" and the page is wet. She folds it. "He never finished one. {..} That's why they never came." `[NO BUTTON]`
* Reward: 100 H$, **Two-Way Mailbox** (furniture upgrade: your mailbox now receives Hellvile mail and the news ticker), unlock Mail Carrier badge. Later: your mailbox gets a letter from Warty: a song lyric and a pickle.

#### Chain G: Grampy Bean and Sootkin (cross-border; 3 parts)

**S124 "Where Was I?"** | Grampy Bean (88) | Ch1-2 | find, talk, pet
* Hook: Grampy has lost his glasses, his trowel and "the thing I was about to say". The pet finds things.
* Beats: 1) Pet verb: @E sniffs out the glasses (in a tomato), the trowel (in the fountain, which giggles when the pet reaches in) and a seed packet with Hellvile writing. 2) Grampy reads the packet and remembers "a little mouse-person who whistled" for exactly one line, then loses it.
* Comic beat: Grampy puts the glasses on `[R5]` over his other glasses. "{d}Better."
* Heart: he keeps the empty packet in his hat. "In case I remember who to thank."
* Reward: 20 H$, Tomato Planter (furniture; grows a tomato item every 2 days). Later: Ep 3.5, the memory game uses the packet as its first clue.

**S125 "The Face Tomatoes"** | Grampy Bean (88) | Ch3 (after Ep 3.5, saved) | fetch, find
* Hook: his tomatoes still have eyes. He wants them picked "before they get opinions". Dr. Bloom wants three for study. The pet wants to eat one.
* Beats: 1) Pick 9 eye-tomatoes; each one blinks when you look away `[R2]`. 2) Deliver 3 to Bloom. 3) Bury the rest where Grampy says: by the Wall, facing Hellvile. 4) Overnight they sprout sunflowers.
* Comic beat: the pet eats one. `[HOLD 13]`. "beep." `[R8]` The pet's eyes are briefly tomatoes. `[R13]` "{d}...it's fine."
* Scare beat: one tomato whispers "{w}thank you" in a voice that isn't Grampy's. He hears it and smiles. "Xe's still over there."
* Reward: 50 H$, Eye Tomato (decor: blinks). Later: Ep 4.3, the sunflowers along the Wall are the trail to Sootkin's sunflower.

**S126 "Two Gardens"** | Sootkin (525, Hellvile) + Grampy Bean (88) | Ch4-5 | fetch, housing (someone else's garden), talk
* Hook: Sootkin's sunflower is under the gate, which is coming down. Grampy wants to move it to a bed that straddles the old Wall line. Sootkin has never moved it. Xe is scared it won't survive.
* Beats: 1) Fetch Hillvile soil and Hellvile ash. 2) Build the shared bed (housing mode on an outdoor plot: 3x3 cells; place 4 tomato plants and 1 sunflower). 3) Water it. Vapula, the plush cloud (if `SF_VAPULA_PLUSH`), rains on request. 4) Wait one day. It lives.
* Comic beat: Grampy calls Sootkin a different name every time ("Sooty, Sultry, Toaster"). Sootkin, who has waited ten years to be thanked, answers to all of them.
* Heart: Grampy: "Thank you for the seeds." SOOTKIN: "{d}...You remembered." GRAMPY: "I remember everything. {..} Where are my glasses?" They whistle together. `[NO BUTTON]`
* Reward: **Shared Flower Bed** (outdoor furniture for the Park Plot), 100 H$, unlock Green Thumbs trophy. Later: Bluebell (29) hires Morguelet (#33) *and* asks Sootkin for seeds.

#### Chain H: Steamy's bath house (Hillvile; 3 parts; branches on `SF_STEAMY_LOST`)

**S127 "Every Duck Has a Name"** | Steamy (2) | Ch1-2 | find, talk
* Hook: there is one duck in the bath house that Steamy didn't name and doesn't remember buying (Ep 1.4). She wants it gone; she can't bring herself to throw out a duck.
* Beats: 1) Find who it belongs to: three suspects (Squeaker, Tidepool, Pip). None. 2) The duck squeaks at the Wall at night. 3) Carry it to the Wall. Something squeaks back, 3 then 1. 4) Steamy keeps it. She names it "Guest".
* Comic beat: the suspect line-up: each one squeaks their own duck as an alibi `[3x@15]`. Pip's is Mr. Flops. "He's a duck today."
* Scare beat: when you set Guest on the Wall, the fog giggles **in a frog's voice**. Steamy: "{d}...That one's not mine either."
* Reward: 30 H$, Rubber Duck "Guest" (decor; squeaks at the Wall). Later: Ep 2.4, Guest is the duck in Steamy's hand when she turns her head 180°.

**S128 (saved) "Grand Re-Opening" / S128 (lost) "Steam Rises"** | Steamy (2) or her ghost | Ch3 | fetch, cooking (soap), housing (her bath house)
* Hook (saved): Steamy wants the bath house open for the people hiding in the church. "Clean is a kind of brave." (lost): her ghost wants the same, from the other side of the water.
* Beats: 1) Fetch 3 Soap (Lather's chain #13 ties in). 2) Redecorate the bath house (housing mode in her home): towels, ducks, a "NO DEMONS" sign Rivet welds. 3) First bath: the kids from the daycare. 4) (saved) Steamy sings. (lost) The water warms by itself. The ducks line up on their own.
* Comic beat (both): Mascotty arrives for a bath and gets in **with the head on** `[HOLD 21]`. The head floats. Mascotty does not. `[R13]` Everyone stares at the surface. She surfaces. Head back on. Nobody says anything.
* Heart (saved): Steamy, washing a kid's hair: "There was someone in my head who hadn't been warm in ten years. {..} I keep the water hotter now. In case." (lost): the ghost writes in the steam on the mirror: "WARM ENOUGH?" You can answer A: yes. `[NO BUTTON]`
* Reward: **Steamy's Tub** (furniture; the pet bathes in it), 60 H$. Later: Ep 3.3 graveyard nest, the bathtub of warm dead is explained.

**S129 "Mixed Bathing"** | Steamy (2) or her ghost + Scaldie (513, Hellvile, serves everything scalding) | Ch4-5 | talk, fetch, concert
* Hook: the first cross-border bath night. Scaldie wants it hotter. Steamy wants it "warm". The ducks are neutral.
* Beats: 1) Fetch Sulfur Soda for Scaldie, Cotton Candy for Steamy (a diplomatic snack exchange). 2) A temperature minigame: a slider with two hands on it (yours and the pet's). 3) The bath night: 12 citizens, 6 a side, in one pool. Warty sings. Riffmaw (511) tunes the church bells by accident. 4) Guest the duck swims to the middle.
* Comic beat: Scaldie tests the water with a finger and screams "{big}LUKEWARM!{/}" `[SHK_POW]`. `[R26]` "{d}...I'll allow it."
* Heart (saved): Steamy to Tallyworn, in the pool: "You were in my head. You were so tired." TALLYWORN: "{d}You were so warm." (lost): the ghost's steam-writing on the big window: "EVERYONE'S HERE." `[NO BUTTON]`
* Reward: 120 H$, Guest the Duck gains a tiny Hellvile hat, unlock "Mixed Bathing" bath-night event (repeatable, Fridays). Later: Scaldie opens a sauna in the bath house's back room. It is on fire. It's fine.

#### Chain I: Sprinklepop's lost soul and Wheezle (cross-border; 4 parts)

**S130 "Party Supplies"** | Sprinklepop (81) | Ch1-2 (before Ep 2.4) | fetch, minigame
* Hook: the lollipop artist who sneezes glitter needs 20 balloons for "a party I haven't been invited to yet". Twisty (43) has them. Twisty wants a balloon *you*.
* Beats: 1) Win the Fun Fair balloon minigame. 2) Deliver 20 balloons. 3) Sprinklepop sneezes: glitter, glitter, `[3x@15]` **purple** glitter. 4) "Doctors say it's fine!"
* Comic beat: the balloon-you Twisty makes has the wrong number of ears. Twisty: "{d}It's an interpretation."
* Scare beat: Sprinklepop's third sneeze is R8 late and in a lower voice. She doesn't notice. The balloons all turn to face the Wall.
* Reward: 20 H$, Balloon-You (decor; deflates slowly over the chapter and is re-inflated by any party). Later: she is first in line at Cloud Nine (Ep 2.1).

**S131 "Lost and Found"** | Dr. Bloom (111), with Sprinklepop's sleeping body at the clinic | Ch2-3 (after Ep 2.5) | find, photo, pet
* Hook: Sprinklepop's soul wandered off with its map upside down. Bloom asks you to find where it went. Souls "go exactly as far as they're scared to".
* Beats: 1) Follow a trail of glitter (sparkle spots) around HaHaland: the lollipop stand, the Fun Fair, the daycare. 2) At each stop a citizen saw "a bouncy see-through bunny reading a map wrong". 3) Photograph the soul at the Wall, waving, holding the map. 4) The pet sniffs the crack in the Wall. "beep." It went through.
* Comic beat: the soul is **still having a party**. It has set up tiny translucent balloons at each stop. The Fun Fair stop has a translucent cake. Twisty: "{d}She ordered that."
* Heart: at the Wall, the soul turns the map the right way up, looks at Hillvile, looks at Hellvile, and chooses Hellvile. Bloom, later: "She went toward the party she wasn't invited to. {..} That's not lost. That's brave."
* Reward: 50 H$, Translucent Balloon (decor). Later: Ep 4.2, Wheezle names his ghost "Sprinkles".

**S132 "Wheezle's Little Ghosts"** | Wheezle (707, Hellvile) | Ch4 | talk, find, timing
* Hook: Wheezle coughs out little ghosts and names them. One of them is bigger than the others, pink, and keeps trying to throw parties. He calls her Sprinkles. He thinks she's his.
* Beats: 1) Meet the little ghosts (6, named Tuesday through Sunday; the pink one is "Sprinkles"). 2) The timing minigame: Sprinkles sneezes glitter on the beat; press A to catch each sneeze in a jar. 3) Explain to Wheezle, gently (a 3-option choice; all three are gentle). 4) Wheezle: "...She's not mine. {..} But she can visit?"
* Comic beat: the ghosts introduce themselves `[3x@15]` "Tuesday." "Wednesday." "Thursday." `[R13]` "{d}...Also Thursday." Wheezle: "I had a long week."
* Heart: Wheezle has been coughing up ghosts because he's been swallowing laughs for ten years (the law of Vapula's Hellvile). The ghosts are his laughs. Sprinkles made them throw parties. `[NO BUTTON]`
* Reward: 80 H$, **Jar of Sneezes** (furniture; glows pink), Wheezle's friendship. Later: the little ghosts turn up at your Hell Pits flat for a party you didn't throw.

**S133 "Bubbles Bounces Back"** | Sprinklepop's soul (81) + Dr. Bloom (111) | Ch4-5 | escort, exorcism (reverse), concert
* Hook: a soul going *home* is an exorcism run backwards. Fuzz has never done one. Sr. Lumen's scroll says "probably".
* Beats: 1) Escort the soul from Hellvile to the clinic (it bounces ahead; keep it in view or it starts a party). 2) The reverse ritual: the prayer rhythm backwards (the metronome runs in reverse; the bell un-rings, W11, now funny). 3) The true-name phase asks for *her* name. The player types it from memory: SPRINKLEPOP. 4) She wakes up. She sneezes. Pink glitter.
* Comic beat: the reverse ritual's result stamp reads "EXORCISM UNFAILED x1" `[SLAM]` with the kazoo played backwards (a rising "wahhh wah wah").
* Heart: Sprinklepop, awake: "I had the best dream. Everyone was invited." `[R13]` "{d}...Was I?" The whole clinic: "{big}Yes.{/}"
* Reward: 150 H$, **Heart Balloon** (furniture; the one she carries in Ep 4.7), unlock "Unfailed" trophy, the Mixed Party event. Later: Ep 4.7, she bounces in with the heart balloon.

#### Chain J: Mascotty and Thumpkin (cross-border; 3 parts)

**S134 "Ignore That One"** | Hugsley (20, the *other* town mascot) | Ch1-2 | find, photo, talk
* Hook: Hugsley, the plush mascot who gets hugged 1,000 times a day, is worried about Mascotty, the *bunny* mascot who gets hugged zero times and never takes the head off. "It's a two-mascot town. We should talk. It won't talk."
* Beats: 1) Find Mascotty (she is always somewhere, facing the Wall). 2) Try three things: wave (nothing), hug (she lets you; she doesn't move), photo (the photo shows the head and, through the mouth-mesh, two tired eyes). 3) Stand still for R21. She waves, slowly. 4) Report to Hugsley: "It waved?!"
* Comic beat: Hugsley tries to copy the slow wave and can't go slowly. Its arm waves at full speed `[PANIC Hugsley]`. "{d}I'm a hugger, not a waver."
* Scare beat: at night, the thumping from the Wall (W6) is answered by Mascotty, thumping the ground with one foot: 3, then 1. `[MUTE 21]`
* Reward: 30 H$, Mascotty Postcard (decor; the eyes follow you). Later: she is at your window in Ep 1.5.

**S135 "Three Thumps and One"** | Prof. Fizz (69) | Ch2-3 | find, timing, fetch
* Hook: Prof. Fizz's vapor detector "beeps at everything" but it also records the Wall thumps, and they have a pattern. He wants you to record 3 nights.
* Beats: 1) Three nights at the Wall with the detector (the calendar). 2) A timing minigame: press A on each thump; the pattern is 3-1, then 3-1-2, then 3-1-2-1 (a counting code: "are you there / yes / still / yes"). 3) Decode with Fizz: it is bunny-thump for "sister". 4) Mascotty, nearby, thumps "yes".
* Comic beat: Fizz's detector also beeps at the pet, a muffin, Fizz, and silence. "It's calibrated for *everything*. That's not a flaw."
* Heart: Fizz, reading the decode: "Somebody's been asking the same question for ten years. {..} And somebody's been answering." `[NO BUTTON]`
* Reward: 50 H$, Vapor Detector (decor: beeps at the pet). Later: Ep 4.2, Thumpkin thumps the pipe "for my sister".

**S136 "The Head Comes Off"** | Thumpkin (894, Hellvile) | Ch4-5 | escort, talk, housing
* Hook: Thumpkin, the "killer bunny" who thumps the ground until the neighbours move, has thumped every night for ten years so his sister would know he was still there. Now he wants to walk her home. He is terrified she'll keep the head on.
* Beats: 1) Escort Thumpkin to Hillvile; he thumps at every lamp post (his nerves). 2) Mascotty is at the fountain `[STILL]`. 3) `[HOLD 26]`. The head comes off (if Ep 4.6 has not happened yet, this is where it comes off instead). 4) Thumpkin thumps once. She thumps back. 5) Decorate a room for her in the Hell Pits (housing in someone else's home): she asks for one thing, a window that faces *away* from where the Wall was.
* Comic beat: Thumpkin's thumps shake the fountain `[SHK_TAP]` and it giggles on each one, on time. He thumps faster to make it laugh more. The Mayor, from a distance: "{d}He's good."
* Heart: Mascotty's first words in the game (she has had none): "{d}I didn't know what you'd look like after ten years." THUMPKIN: "{d}Taller." MASCOTTY: "{d}...No." `[NO BUTTON]`
* Reward: 150 H$, **The Mascot Head** (furniture; she gives it to you; the eyes don't follow you anymore), unlock "Nobody Knows Who" trophy. Later: Hugsley gets hugged 1,001 times. The extra one is Mascotty.

#### Chain K: Vex and Haze, Cloud Nine to Fresh Air (Hellvile → Hillvile cross-border; 4 parts)

**S137 "Employee of the Month"** | Haze (709), at the Cloud Nine counter | Ch2 | talk, photo, find
* Hook: Haze, who hates the smoke, wants a photo of the "Employee of the Month" frame "for evidence". The frame shows a crowned cloud. Vex says it's "a motivational poster".
* Beats: 1) Photograph the frame (the eyes move during the shutter; the photo shows them looking at the camera). 2) Find the shop's three plants, which Haze waters secretly; one is a tomato with an eye. 3) Haze gives you a plain-water vape pen "for when the grape runs out".
* Comic beat: Vex catches you photographing the frame. `[STILL Vex, smile]` `[HOLD 21]` VEX: "He's very motivational." `[R8]` VEX: "{d}Please don't tell him I said 'he'."
* Scare beat: the photo, developed later at Shutterbug's, shows the frame **empty**. `[MUTE 14]`
* Reward: 30 H$, Plain Water Pen (item; cancels one grape effect), the Employee of the Month frame appears on your mailbox the next day (uninvited; decor). Later: Ep 2.7, the frame falls off the wall with a tink.

**S138 "Closet Space"** | Vex (666) | Ch3 (after Ep 3.1) | housing, talk, fetch
* Hook: during the invasion Vex is hiding in **your closet**. He has been there since the eruption. He would like a cushion.
* Beats: 1) Discover him (housing mode: the closet is "placed by: ???" with a cushion request). 2) Fetch him a Cotton Candy (his favourite). 3) Three conversations through the door, one per house-defence wave: he explains Cloud Nine, then Vapula, then why ("he listened"). 4) Let him out, or don't (he stays until Ch4 either way).
* Comic beat: every time a demon breaks a window, Vex says "{d}not a customer" from inside the closet. After wave 3: "{d}...I'll do a survey."
* Heart: VEX: "Nobody over there laughs. It's the law. He said if I opened a shop here, he'd listen to me. {..} He did. {d}He's the only one who ever did." `[R21]` `[NO BUTTON]`
* Reward: Vex's Cushion (furniture; he leaves it), 40 H$. Later: in Ep 3.7, when the fog comes under the door, a voice from the closet says "{d}not a customer" and the fog, for one frame, hesitates.

**S139 "Exit Interview"** | Vex (666) + Haze (709) | Ch4 | talk, fetch, housing (the shop)
* Hook: Cloud Nine Vapor is closing. Haze is taking over. Vex wants a reference letter. Haze wants the smoke out of the curtains.
* Beats: 1) Fetch 3 Soap and air out the shop (a housing clean-up mode: remove the purple props). 2) Redecorate as the **Fresh Air Lounge**: plants, open windows, the Employee of the Month frame replaced with "Employee of the Month: Vex" (Haze's one act of mercy). 3) Vex's reference letter: a 3-option choice of what to write. 4) Vex tries plain-water vapes. "They're terrible." He tries another.
* Comic beat: Haze's redecorating has one rule: "if it's purple, it goes". The pet is purple at the moment (eye tomato, S125). The pet goes outside. "beep."
* Heart: Haze: "I watered the plants for two years in that smoke. {..} I didn't know why. I think I was practising for this." Sets `SF_VEX_REFORMED`.
* Reward: 100 H$, **Fresh Air Lounge Neon** (furniture), **Employee of the Month: Vex** (decor). Later: Vex on the Unity Stage, "plain water vapes now".

**S140 "The Bubble Machine"** | Vex (666) | Ch5 | weld, minigame, concert
* Hook: Vex wants to be useful at the festival. He has a vape rig and no vapor. Rivet suggests soap.
* Beats: 1) Weld the rig into a bubble machine (weld minigame; Rivet: "if it's purple I'm leaving"). 2) Test at the Fun Fair (a bubble-popping minigame with the pet). 3) Run it at the Unity Stage concert; bubbles sync to the beat (the concert scene with a bubble layer).
* Comic beat: the first bubble is grape. `[FREEZE ALL]` `[HOLD 13]` VEX: "{d}Residue." `[R8]` It pops. Everyone is fine. Everyone checks anyway.
* Heart: Vex, watching 1,000 citizens in bubbles: "They're laughing. {..} Both sides." HAZE: "{d}Yeah. That's what it sounds like."
* Reward: **Bubble Machine** (furniture; blows bubbles when music plays), unlock "Terrible, Actually" trophy, 100 H$. Later: the news ticker: "Vex, Fresh Air Lounge employee of the month for the 3rd month running."

### 13.1 continued (stand-alone missions, group 1: S141-S150)

**S141 "The Starer in the Window"** | Hootenanny (26), night watch owl | Ch1-2 | find, photo
* Hook: Hootenanny has seen "a face" in a Hillvile Towers window every night. It never moves. It's ruining his hoedowns.
* Beats: 1) Find the window at night (the apartment facade browser). 2) Photograph it: Mascotty's head, in the dark, 9th floor. 3) Hootenanny: "...She lives there?" She does not. Nobody knows where she lives. 4) The next night the window is empty, and the one next to it isn't.
* Comic beat: Hootenanny hoots at the window `[3x@15]`. On the third hoot the head tilts 0.14 rad with a creak. Hootenanny falls off the roof (he's fine; he's an owl).
* Scare beat: `[NO BUTTON]`. The mission just ends. Hootenanny moves the hoedown indoors.
* Reward: 20 H$, Hoedown Fiddle (decor). Later: S141's window is Rattlecup's (S115).

**S142 "The Fountain Is Late"** | Prof. Fizz (69) | Ch1 | timing, photo
* Hook: Fizz has timed the fountain's giggle for a week. It is 267 ms late, exactly (R8). "Fountains aren't late. Fountains are plumbing."
* Beats: 1) A timing minigame: press A when the fountain giggles, 10 times; your presses are plotted as a histogram (all R8 late). 2) Photograph the fountain's faces. 3) Fizz notices the two faces are **not facing each other** anymore. Someone turned one of them. 4) He writes it down and underlines it three times.
* Comic beat: Fizz's stopwatch also beeps at the pet. "Everything is 267 milliseconds late today." `[R8]` "...Except that."
* Heart/seed: Fizz: "A two-faced fountain only giggles if someone laughs on the other side. {..} So who's laughing?"
* Reward: 20 H$, Fizz's Histogram (decor). Later: Ep 3.4, Rivet: "It was a TWO-WAY fountain!"

**S143 "Population: 1000"** | Mayor Giggs (0) | Ch5 | fetch, minigame, talk
* Hook: the town sign still says 500. The scratched-out 1000 underneath was the original. The Mayor wants to repaint it himself and has never held a paint roller.
* Beats: 1) Fetch paint from Rainbow Ewe (28) and Stencilla (565), one colour each. 2) A paint-roller minigame (the s12 census counter rolls `v = N*(a/22)^2.2` as you paint: 500, 750, 1000). 3) The sign reads "Population: 1000 lovely citizens. And @P." 4) `[R8]` The "0" falls off, *tink*.
* Comic beat: Rivet, passing: "Don't make it weird." The Mayor glues the 0 back on. It falls off again. Tallyworn: "{d}999. I can live with 999."
* Heart: the Mayor paints the "1" in "1000" first and stops. "I painted this over myself. Ten years ago. With this roller." He finishes.
* Reward: **Town Sign (miniature)** (decor), 100 H$, unlock "Census" trophy. Later: the title screen's subtitle changes to "Population: 1000".

**S144 "Fishing for Compliments"** | Capt. Whisk (48) | Ch1-5 | fishing, talk
* Hook: Capt. Whisk catches fish, names them and lets them go. One fish keeps coming back and he has run out of names. He wants you to catch it and name it something "that'll stick".
* Beats: 1) Fishing at the pier until the named fish bites (it has a tag: "Gerald, Gerald II, Gerry, G, Fish"). 2) Name it (text entry). 3) Let it go. 4) It comes back the next day with your name on the tag.
* Comic beat: the fish's tag has 40 crossed-out names. The last one is "Please Stop". Whisk: "{d}He wrote that one himself."
* Heart: Whisk: "You let something go forty times and it comes back forty times. {..} That's not fishing. That's a friend."
* Reward: Fishdex entry "Gerald (persistent)", Fish Trophy (decor; it's the fish; it's alive; it's fine). Later: at the Lake of Fire (Ep 4.7 stall) the fish is there. In lava. Fine.

**S145 "Bedtime for Everyone"** | Story Owl (16) | Ch1-2 | talk, dream, housing
* Hook: Story Owl reads bedtime stories to the whole street at once. Tonight's is 'The Very Hungry Demon' (quest #6's book) and he wants you to test it on the pet.
* Beats: 1) Place the pet bed near the window (housing). 2) Listen to the story at night (a 60 s scene; the pet reacts per page). 3) Peek at a Dream: the pet dreams of the book, but the demon is **small and crying** and eats only its own tail. 4) Report to Story Owl. He rewrites the ending.
* Comic beat: the book's demon eats "one apple, two pears, three plums, {d}four Hillvile." Story Owl: "It's a classic."
* Heart: the rewritten ending: "And the very hungry demon was given one cupcake, and it was enough." Story Owl: "First edit in sixty years." `[NO BUTTON]`
* Reward: 20 H$, 'The Very Hungry Demon' (decor; readable). Later: Dream D2, one of the floating letters is from the demon in the book.

**S146 "Whack-a-Nothing"** | Squeaker (64), the clown | Ch2 | minigame, find
* Hook: the Whack-an-Imp machine at the Fun Fair has started popping up imps when it's switched off.
* Beats: 1) Play Whack-an-Imp (minigame). 2) Play it switched off: the imps still come up, slower, looking at you. 3) Find the plug: it leads under the ground, toward the Wall. 4) Squeaker unplugs it. It keeps going. "{d}It's a very good machine."
* Comic beat: "Do NOT bonk Father Fuzz." You bonk Father Fuzz. He is not in the machine. He is standing next to it. `[SHK_POW]` "{d}tsk."
* Scare beat: the last imp that pops up, with the machine off, says "beep." in the pet's voice. `[MUTE 14]`
* Reward: 30 H$, Mini Whack-an-Imp (furniture; the pet plays it). Later: Ep 3.3, the Fun Fair nest, the machine is possessed for real.

**S147 "Say Cheese (Again)"** | Shutterbug (46) | Ch2-3 | photo, find
* Hook: Shutterbug's photos have started showing things that weren't there (W8, lagging shadows). He wants a "before" set of the whole town, "in case".
* Beats: 1) Photograph 8 landmarks (the fountain, the bakery, the Wall, your house, the daycare, the church, the pier, the Fun Fair). 2) Each develops with one wrongness (an extra shadow, a crown in a cloud, a chair on a roof). 3) Shutterbug pins them up as a board. 4) In Ch3 the board is in the church: it is the only record of what the town looked like.
* Comic beat: the photo of your house has Mascotty in the window. You were inside. Shutterbug: "{d}Great composition."
* Scare beat: the photo of the Wall has a hand on it, from the other side, pressed flat. `[HOLD 21]`
* Reward: 50 H$, Before Board (decor). Later: quest #45, "The Big Picture", becomes the "after" photo, and Shutterbug hangs them side by side.

**S148 "Lullaby for the Wall"** | Maestro Mew (3) | Ch2-3 | concert, studio, fetch
* Hook: the conductor has noticed the church bells un-ring (W11) on a particular note. He wants to write a piece that "rings them the right way round".
* Beats: 1) Compose a 4-bar phrase in the Studio (the song editor, constrained to the bell scale). 2) Perform it at the Concert Hall with Cantor Coo. 3) The bells ring forward. The Wall, outside, thumps on the beat. 4) Mew keeps the thump in the piece.
* Comic beat: Cantor Coo plays the lullaby at the speed of disco. Mew conducts with his tail at half speed. The orchestra follows the tail. Coo: "{d}Rude."
* Heart: Mew: "Someone over there has rhythm. {..} I'd like to meet the percussion section." (He does, in Ep 4.7: Moshpit (554) and Riffmaw.)
* Reward: the song "Lullaby for the Wall" in the jukebox, 40 H$, Conductor's Baton (decor). Later: the Hellvile pump yard works to this rhythm in Ep 4.2.

**S149 "Make Me a Face"** | Phantasma (524, Hellvile ghost who pretends to be a ghost) | Ch4 | creator, talk
* Hook: Phantasma wants a "disguise" for the festival: a cute Hillvile look. They have never seen themselves in a mirror. They are a ghost.
* Beats: 1) The Creator scene, designing a look for Phantasma (a citizen-creator variant: tabs limited to colour, ears, accessory). 2) Show them: they float through the mirror. 3) Show them a polaroid instead. 4) They cry (a happy ghost rains upward).
* Comic beat: every accessory falls through them `[3x@15]`: hat, bow, `[R13]` "{d}...a smaller hat."
* Heart: PHANTASMA: "I pretend to be a ghost so people aren't scared when they find out. {..} This face isn't scary." YOU (choice): "Neither was the old one."
* Reward: 60 H$, Phantasma's Portrait (decor), unlock Creator: Hellvile palette. Later: Phantasma hosts the festival's "fake seance", which is a real one, which is fine.

**S150 "Hellvile Hopscotch"** | Tantrum (530, Hellvile, KID) | Ch4-5 | minigame, pet
* Hook: Hellvile kids play hopscotch on lava with oven mitts. Tantrum throws very small tantrums when they lose. Xe wants a Hillvile opponent to "lose properly".
* Beats: 1) The hopscotch minigame (a rhythm-hop on 10 lava squares; the pet goes first and sizzles; it's fine). 2) Lose on purpose or win. 3) Either way, Tantrum throws a tantrum (0.5 s, two stamps). 4) Pokefork (540) pokes you with a foam pitchfork. Giggles.
* Comic beat: the tantrum is `[SHK_TAP]` x2 and a `{d}"hmph."`. Then xe offers you a mitt.
* Heart: Tantrum: "Hillvile kids don't come here." YOU: "They do now." Pip arrives with Mr. Flops wearing two tiny oven mitts. `[NO BUTTON]`
* Reward: 30 H$, Oven Mitts (decor: tiny), unlock the Lava Hopscotch minigame in free play. Later: Miss Lulla's field trip (S159).

#### Chain M: Dr. Bloom and Graveley (cross-border; 3 parts)

**S151 "Cold Hands"** | Dr. Bloom (111) | Ch1-2 | fetch, talk
* Hook: Bloom keeps her hands in warm water between patients "so nobody notices". The bath house's warm water is the best. Steamy has started asking questions.
* Beats: 1) Fetch a Hot Water Bottle (Lather's soap shop stocks them). 2) Deliver. 3) A conversation in three parts, one per visit: "it's a condition", "it's a family thing", "{d}...I'm a ghoul."
* Comic beat: Bloom warms her hands, touches a patient (Zoomzoom), who speedruns out of the clinic in under four seconds anyway. "{d}That's not the hands. That's him."
* Heart: Bloom: "Everyone here is lovely about everything. {..} I never found out if they'd be lovely about this." `[NO BUTTON]`
* Reward: 30 H$, Hot Water Bottle (decor; the pet sleeps on it). Later: Ep 3.4, she says it out loud.

**S152 "Night Clinic"** | Dr. Bloom (111) | Ch3 | fetch, combat, escort
* Hook: the clinic is outside the church. Six citizens need medicine. Bloom will go herself; she'd like company "for the cold".
* Beats: 1) Escort Bloom through the invaded plaza at night. 2) Three house calls, each a tiny scene (Oma Fluff has lost her glasses again; Snugglesby needs a hug and gives one; Lagspike is streaming the invasion to her grandma). 3) At the third, demons; Bloom holds a lantern while you fight. Her hands don't shake. 4) Back at the church, the Mayor sees her clearly for the first time: a ghoul in pastel scrubs.
* Comic beat: Lagspike's chat (her grandma) types "are those demons behind you sweetie" `[R8]` "yes grandma" `[R8]` "ok be safe xx".
* Heart: the Mayor: "Dr. Bloom. You're..." BLOOM: "{d}Cold-handed. Yes." MAYOR: "{..}...You've been here the whole time." BLOOM: "{d}Ten years." `[HOLD 21]` MAYOR: "Thank you." `[NO BUTTON]`
* Reward: Medicine x3, 60 H$, Bloom's Lantern (furniture). Later: the church kids call her "Dr. Cold Hands" and mean it kindly.

**S153 "A Clinic With Two Doors"** | Graveley (730, Hellvile ghoul landlord, Bloom's sibling) | Ch4-5 | housing, fetch, talk
* Hook: Graveley lowers rent for families with kids and has an empty crypt on the old Wall line. Bloom wants to turn it into a clinic with a door on each side.
* Beats: 1) Fetch 4 Scrap and 2 Medicine. 2) Rivet welds the second door. 3) Decorate the clinic (housing in someone else's building: a bed each side, one lamp in the middle). 4) First patient: Grumbleguts, with indigestion. Second: the pet, with horn-ache. Third: Fuzz, who doesn't need anything and sits down anyway.
* Comic beat: Graveley and Bloom have the same cold hands and shake on it `[HOLD 13]`. Both: "{d}Ooh." `[R8]` Both: "{d}Sorry."
* Heart: Graveley: "You were on the wrong side when it closed." BLOOM: "There wasn't a wrong side. {..} There was just a wall." They sit on the clinic step, one foot each side of the old line. `[NO BUTTON]`
* Reward: 120 H$, **Two-Door Clinic** (a place; Rest & heal from either side), Bloom's Scrubs (decor), unlock "Cold Hands, Warm Heart" trophy. Later: the news ticker: "Clinic reports 0 patients scared of the doctor. 1 scared of the forms."

#### Chain N: Hattie Haha and the Hellvile comedy scene (cross-border; 3 parts)

**S154 "Open Mic"** | Hattie Haha (71) | Ch1-2 | timing, talk
* Hook: the open mic at the cafe needs a warm-up act. Hattie wants you. You have no act. She says that's fine: "Timing is the act."
* Beats: 1) The comic-timing minigame: 3 jokes, press A on the punchline beat. 2) Perfect presses get a snort-laugh ripple; late ones get a cricket. 3) Hattie's closer about the Mayor. 4) Mascotty, in the back row, doesn't laugh; Hattie: "Tough room."
* Comic beat: your third joke's punchline is just "...Hee. Hee." Everyone laughs. The Mayor, in the front row, laughs loudest, then stops. `[STILL Mayor]` `[R13]` "{d}Good one."
* Heart: Hattie: "Everybody here laughs at everything. Do you know how hard that makes it to tell if you're funny?" `[R8]` "{d}That one was a joke. {..} Mostly."
* Reward: 20 H$, Mic Stand (furniture), unlock the Open Mic activity at the cafe. Later: Ep 3.5, she heckles you with your own timing.

**S155 "No Punchlines"** | Hattie Haha (71) (saved) or her ghost | Ch3-4 (after Ep 3.5) | timing, talk, find
* Hook: Hattie can't finish a joke anymore. She gets to the punchline and laughs a beat early. She wants to relearn with you, "from the setup".
* Beats: 1) Three sessions, one per day: the timing minigame with her laugh as the metronome, drifting. 2) Find her old notebook (in the burned cafe) of jokes she never told "because they were about the Wall". 3) Session three: she tells one. On time. 4) The cafe owner laughs so hard the jukebox skips to the right song.
* Comic beat: session one: "Knock knock." YOU: "Who's there?" HATTIE: `[R2]` "{d}HA. {..} No. Wait."
* Heart: the untold joke: "Why did the Hillviler cross the park? {..} {..} {d}She didn't. That's the joke. For ten years that's been the joke." Nobody laughs. Then Haze, who has crossed with you, snorts. Hattie: "{d}Finally. A room." `[NO BUTTON]`
* Reward: 60 H$, Hattie's Notebook (decor; readable: 12 jokes). Later: Ep 4.7, the set opens with the refrain.

**S156 "The Hellvile Set"** | Honkers (501, Hellvile creepy clown) + Hattie (71) | Ch4-5 | concert, timing, talk
* Hook: Honkers honks his nose at funerals and is not invited back. He has never been allowed to be funny on purpose. He wants a double act with Hattie at the festival. She wants to know if he can do timing. He cannot.
* Beats: 1) Rehearsal: the timing minigame as a duet; Honkers honks R2 early every time. 2) Hattie's fix: write his honks into the act as the metronome. 3) The set at the comedy tent (a concert-scene variant with laugh tracks on both sides). 4) Mr. Solemn (555), who hasn't smiled in 200 years, is in the front row.
* Comic beat: Honkers honks at the wrong beat for the entire set, and because Hattie wrote it that way, every honk is on time. `[3x@15]` escalation: honk, honk, `[HOLD 13]` nothing. The silence is the punchline. Mr. Solemn's mouth moves 1 px.
* Heart: Hattie, after: "Ten years nobody laughed over here. It wasn't that you weren't funny. {..} It was the law." HONKERS: "{d}I honked anyway." HATTIE: "{d}I know. I heard you. Through the Wall."
* Reward: 150 H$, **Honkers's Nose** (furniture: honks when the pet passes), unlock "Tough Room" trophy, the comedy tent as a repeatable activity. Later: the news ticker: "Mr. Solemn seen smiling. Denies it."

#### Chain O: Miss Lulla's daycare and the Hellvile kids (cross-border; 3 parts)

**S157 "Naptime Monsters"** | Miss Lulla (36) | Ch1-2 | talk, housing (the daycare), pet
* Hook: the kids won't nap. They say there's a monster under the cots. Miss Lulla says monsters aren't real. She wants you to check so she doesn't have to.
* Beats: 1) Housing mode in the daycare: rearrange the cots so none face the Wall-side window (the kids' request). 2) Check under each cot `[3x@15]`: a sock, a crayon, `[R13]` a kid's drawing of a crowned cloud with a speech bubble: "we". 3) The pet naps under the third cot and the kids nap too.
* Comic beat: Lulla gets any baby to nap in under a minute. She tries it on you. It works. You wake up 30 game-minutes later with a blanket and a gold star.
* Scare beat: when you lift the third cot's blanket, the drawing is **wet**. Nobody has been near it. `[MUTE 14]` Lulla: "{d}Juice."
* Reward: 20 H$, Gold Star (decor), the pet learns to nap on command. Later: W12's drawings are explained in S158.

**S158 "The Same Friend"** | Miss Lulla (36) | Ch2-3 | find, talk, dream
* Hook: every kid at the daycare drew the same purple friend with a crown (W12). Lulla wants to know who's been telling them stories. It isn't Story Owl.
* Beats: 1) Interview 4 kids (Pip, Twinkly, Wobbles, Homework who "isn't a kid" and drew it anyway). 2) Each describes a dream: a cloud that "listens". 3) Peek at a Dream at night: your own dream has the cloud in it. It says `{w}"nobody ever listens to ME"`. 4) Lulla hears the kids' story and says, for the first time, "Monsters are real. {..} Some of them are lonely."
* Comic beat: Homework (40) insists they drew a "normal cloud with a hat". Lulla: "{d}The hat has points." HOMEWORK: "{d}It's a fashion cloud."
* Heart: Lulla keeps every drawing and pins them in the church in Ch3: a wall of crowned clouds. The Mayor sees it and sits down. `[NO BUTTON]`
* Reward: 40 H$, Crowned Cloud Drawing (decor), unlock Dream: "The Listener". Later: Dream D2 uses the kids' drawings as the letters.

**S159 "Field Trip"** | Miss Lulla (36) + Tantrum (530) and Pokefork (540) | Ch4-5 | escort, minigame, housing
* Hook: the first joint nap time. Hillvile kids to Hellvile, Hellvile kids to Hillvile, one bus (the pink one). Lulla has never lost a child and does not intend to start.
* Beats: 1) Escort the bus across (a slow drive, a counting minigame: count heads at each stop; Pokefork hides every time). 2) Set up nap mats in Mother Nox's chapel (housing). 3) Nap time: the Hellvile kids won't sleep without a monster under the mat. Grumbleguts volunteers, lies under a mat, falls asleep first. 4) Everyone naps. Lulla gets 24 kids and one gatekeeper to sleep in under a minute.
* Comic beat: head count `[3x@15]`: 12, 11, `[R13]` 13. The 13th is the pet in a tiny bonnet.
* Heart: Lulla, watching: "Monsters are real. They also nap." She writes it on the daycare wall when she gets home. `[NO BUTTON]`
* Reward: 100 H$, Nap Mat (furniture; the pet uses it), unlock "Under a Minute" trophy. Later: Grumbleguts asks if there's a snack after nap time. There is. Lulla planned for it.

#### Chain P: Prof. Fizz, Dr. Quark and Dr. Frankly (cross-border; 3 parts)

**S160 "Beeps at Everything"** | Prof. Fizz (69) | Ch2 | fetch, find
* Hook: the vapor detector (quest #15) now beeps at everything. Fizz wants to find the one thing in town it *doesn't* beep at, "for calibration".
* Beats: 1) Carry the detector around: it beeps at 20 things (a scavenger hunt; each beep is a tiny gag caption). 2) It does not beep at Mascotty. 3) It does not beep at the Wall. 4) It beeps at **you**, if you took the puff (`SF_VAPE_SAMPLE`); otherwise at the pet.
* Comic beat: the 20 beeps accelerate `[3x]` into a continuous tone near Cloud Nine. Fizz: "{d}That's not a reading. That's a scream."
* Scare beat: Mascotty: no beep. Fizz: "Either she's the only clean thing in town, or she's not here." `[HOLD 13]`
* Reward: 30 H$, Calibration Muffin (decor). Later: Ep 4.6, the detector is silent for the first time.

**S161 "Tiny Things"** | Dr. Quark (24), the robot scientist who studies ants and quarks | Ch3 | find, photo, combat
* Hook: Quark has found that the demons are made of "very small sighs". She needs samples from three nests and a photo of one mid-sigh.
* Beats: 1) Collect 3 Vapor Samples from cleared nests (a new drop). 2) Photograph a demon at the moment it dissipates (the kill frame; a photo verb in combat). 3) Quark's analysis: "Each one is somebody's. {..} I can't tell whose."
* Comic beat: Quark's microscope shows the sigh under magnification: it is a tiny "it's fine." in a tiny font. `[R8]` QUARK: "{d}Oh no. It's ours."
* Heart: Quark has studied tiny things for forty years because "big things are frightening". This is the first time a tiny thing was frightening. She keeps studying it. `[NO BUTTON]`
* Reward: 60 H$, Microscope (furniture; the pet looks in it). Later: Ep 3.4, Quark's note is on the fountain's cracked rim.

**S162 "They All Want to Be Florists"** | Dr. Frankly (556, Hellvile mad scientist) + Prof. Fizz (69) | Ch4-5 | fetch, creator, housing
* Hook: Dr. Frankly builds monsters. They all want to be florists. Bluebell's flower shop has one apprentice (Morguelet, #33) and room for one more. Fizz thinks the monsters are "a calibration opportunity".
* Beats: 1) Meet the monsters (3, stitched, polite). 2) Creator scene: give one a florist's look (apron, flower). 3) Build it a stall in Hellvile Park (housing: a 2x2 stall). 4) First customer: Grumbleguts, buying flowers for the Gate.
* Comic beat: Igorina (503) pulls the big lever on cue every time Frankly says "alive". He says it a lot. Thunder every time. Fizz's detector beeps at the thunder.
* Heart: the monster's first bouquet is for Sootkin's bed (S126). It arranges it badly. Sootkin: "{d}It's perfect." It is not. It is perfect.
* Reward: 100 H$, **Florist Monster** (a stall in the park; sells flowers), unlock Creator: Stitched parts. Later: the Florist Monster appears in the credits parade with Bluebell.

#### Chain Q: Tallyworn and Tock (cross-border; 3 parts; continues quest #38)

**S163 "A Clock With No Hands"** | Tock (104) | Ch2-3 | fetch, find
* Hook: Tock fixes every clock in town and naps through the chimes. One clock in Town Hall has no hands and ticks anyway. He wants to know why before he fixes it.
* Beats: 1) Find the clock: in the Mayor's office, behind the drawing (S108). 2) It ticks 3,650 times an hour, exactly. 3) Fetch Tock a Milk (his favourite) while he listens. 4) He decides not to fix it. "It's counting something. {..} Let it."
* Comic beat: Tock naps through his own explanation. You wake him. "{d}...where was I. Ticking. Yes."
* Scare beat: at midnight the clock ticks **one extra time** and the whole office shifts one cell to the left. `[SHK_TAP]` `[MUTE 14]`
* Reward: 30 H$, Handless Clock (furniture; ticks). Later: Ep 4.1, Tallyworn: "Day 3,661."

**S164 "Day 3,653"** | Tallyworn (712, Hellvile) | Ch4 | talk, fetch, housing
* Hook: Tallyworn has filled his tally wall. He needs a second wall, or a reason to stop. He is not ready to stop. He asks for chalk.
* Beats: 1) Fetch chalk from Clinker (753), who uses it for poems and does not share. Trade a poem (a 3-line text entry; she rates it). 2) Help him choose a new wall (housing: a wall panel in his pump-yard shed). 3) He makes the first mark on the new wall. His hand stops. 4) "Maybe tomorrow I'll know what I'm counting."
* Comic beat: Clinker rates your poem: "Six. {d}Needs death." You add death. "{d}Four. Too much death."
* Heart: the new wall's first mark is a heart, drawn by accident when his hand shook. He leaves it. `[NO BUTTON]`
* Reward: 50 H$, Chalk (item), Clinker's Poem (decor). Later: the quest #38 reconciliation ("Counting Together") now opens with Tock already knowing about the handless clock.

**S165 "Counting Down"** | Tock (104) + Tallyworn (712) | Ch5 | weld, fetch, housing
* Hook: Tock wants to give the handless clock hands and set it to count **down** to the next Mingle. Tallyworn wants to be the one who turns it.
* Beats: 1) Weld the hands (weld minigame; Rivet: "clock hands don't need to be structural." They are structural). 2) Carry the clock to the ruined gatepost (where the key from S113 hangs). 3) Tallyworn sets it: 365 days. 4) He makes one last tally mark on the gatepost, under the key, and stops.
* Comic beat: Tock naps through the unveiling. The clock chimes. He doesn't wake. The whole crowd whispers so they don't wake him. `[DUCK .3 60]`
* Heart: TALLYWORN: "Three thousand, six hundred and sixty-five days counting up. {..} Day one counting down." He puts the chalk in his pocket. "My eyes are just doing a rain." `[NO BUTTON]`
* Reward: **Countdown Clock** (furniture copy for your home), 150 H$, unlock "Day One" trophy. Later: on the next Mingle (a calendar event), the clock chimes and Tock wakes up for it.

#### Chain R: Wisp, the haunting (your house, then Hellvile; 3 parts)

**S166 "Placed By: ???"** | (no giver; triggered by the house catalogue in Ch2) | Ch2 | housing, find
* Hook: a tiny imp-sized bed you never placed is in the corner (Ep 2.3). The catalogue lists it as "placed by: ???". You cannot sell it. You can try.
* Beats: 1) Try to remove it (it comes back by morning, one cell closer to your bed each night). 2) Place something next to it as a test: a lamp. 3) By morning the lamp is on, and turned toward the bed, as a night-light. 4) Leave a note (text entry). By morning there's a drawing on it: a house with two windows.
* Comic beat: the third night the little bed has a tiny "DO NOT SELL" sign made from your shopping list. The handwriting is terrible. The spelling is worse. `[R8]` "beep."
* Scare beat: night two, in decorate mode, the cursor moves on its own for exactly 3 frames `[INSERT 3]` and places the lamp. `[MUTE 14]` `[NO BUTTON]`
* Reward: Wisp's Drawing #1 (decor). Later: Ep 3.2, "not in MY room."

**S167 "Night Light"** | Wisp (563), visible from Ep 3.2 | Ch3 | housing, fetch, talk
* Hook: Wisp is scared of the red sky. Not the demons; the sky. They want the house to have "one window that's always daytime".
* Beats: 1) Fetch Lumina's (76) lamp oil and a Candle. 2) Rivet welds a window box; you paint a sky on it (a 4-colour picker). 3) Place it over a boarded window. 4) Wisp sits under it during house-defence waves and gets slightly more solid.
* Comic beat: the painted sky has one cloud. Wisp: "{d}no crown." YOU: "No crown." Wisp adds a tiny sun with a face. It is the Mayor's face. "{d}he looks worried." It does.
* Heart: WISP: "at home i cried at the Wall till i fell out. {..} i didn't know it was MY sky i was scared of." `[R21]` "this one's better. yours has a sun."
* Reward: **Daytime Window** (furniture; always shows day), 40 H$. Later: Ep 4.4, Wisp's real room gets one too.

**S168 "The Housewarming"** | Wisp (563), in a body | Ch4-5 (after Ep 4.4) | housing, cooking, talk
* Hook: Wisp wants to throw *you* a housewarming at your Hell Pits flat, "because you did one for me without knowing". Guests: everyone who knocked on the dark night.
* Beats: 1) Decorate the flat to a Neighbourly rating of B (Hellvile furniture counts double). 2) Cook two dishes, one per side (Soul Soup, Melon Bread). 3) The party: each visitor from Interlude III reacts to the item *they* gave you, if it's placed (a variant pool). 4) Wisp's parents bring the thinking chair.
* Comic beat: Mascotty arrives, stands in the doorway `[STILL]` `[HOLD 21]`, and this time comes in. Everyone cheers. She sits in the thinking chair, facing the wall, out of habit. Wisp: "{d}that's how you use it."
* Heart: Wisp, solid, in a body, holding a plate: "i was in your house when it was empty. {..} it's not empty now." `[NO BUTTON]`
* Reward: **The Thinking Chair** (furniture: anyone who sits in it faces the wall and says one honest thing), 150 H$, unlock "Placed By: Wisp" trophy. Later: the news ticker: "Local child reportedly 'un-floated'. Parents thrilled."

#### Chain L: the Vapula plush (post-game, needs `SF_VAPULA_PLUSH` from Dream D2; 3 parts)

**S169 "Small Weather"** | Lord Vapula (999), a plush grey cloud | Ch5 | pet, talk, fetch
* Hook: the Duke of the Ninth Cloud is now the size of a cushion and rains when he's sad. He is sad a lot. He would like a hobby.
* Beats: 1) Try three hobbies from his roster (Tarot Reading: he predicts rain; Soul Soup: he rains in it; Napping: he's good at this). 2) He asks to follow you like the pet. The pet is not okay with this. 3) The two of them negotiate: a 20 s pet-scene standoff, "beep." vs "squeak." 4) They nap on the same cushion.
* Comic beat: the tarot reading: the Tower, the Tower, `[R13]` the Tower. VAPULA: "{d}it's a rainy deck."
* Heart: VAPULA: "i was every sigh. {..} nobody wanted me, and i was so big." YOU (choice): "You're small now." VAPULA: "{d}...is that better?" The pet puts a paw on him. "beep." `[NO BUTTON]`
* Reward: **Vapula Cushion** (furniture; rains gently on nearby plants), 50 H$. Later: Sootkin's sunflower is the best-watered plant in the park.

**S170 "Vapula's Apology, Properly"** | Lord Vapula (999) + Mayor Giggs (0) | Ch5 | deliver, talk
* Hook: quest #47 had him send flowers. Now he wants to say it. He has written a speech. It is one word.
* Beats: 1) Carry the cloud to Town Hall in a basket. 2) The Mayor and the Duke, face to face: two very small, very scared things. 3) Vapula's speech: "{d}sorry." The Mayor's reply: "Hee. {..} No. Not that." `[R13]` "Me too." 4) It rains in the office. Neither of them minds.
* Comic beat: Spreadsheet (531) presents an invoice for "water damage, municipal, x1". The Mayor signs it. Vapula rains on the invoice.
* Heart: two people who were made of other people's fear, forgiving each other with one word each. `[HOLD 26]` `[NO BUTTON]`
* Reward: 100 H$, Framed Invoice (decor), unlock "Small Weather" trophy. Later: the Mayor's office has a cloud-shaped cushion on the visitor's chair.

**S171 "The Ninth Cloud"** | Lord Vapula (999) | Ch5 | combat (Breach 7 remix), dream, housing
* Hook: Vapula's old home, the Ninth Cloud, is still up there, empty, drifting. He wants to see it one more time, and then he wants it gone. He is too small to fly. You take Updraft's (17) balloon.
* Beats: 1) The balloon ride (a cutscene with the s03 pan and brake). 2) The Ninth Cloud: a floating Breach 7 remix, all mist and unsent letters (the Dream D2 set as a combat map); the enemies are the last scraps of swallowed feeling. 3) At the centre, Vapula's tiny cloud room: a throne made of "it's fine"s. 4) Housing mode: you take one thing (the throne, or a letter, or nothing). He pops the cloud. It rains on the whole park for a day.
* Comic beat: the throne, placed in your house, is uncomfortable. Grumbleguts tries to sit on it. It says "{d}it's fine" and collapses. GURGLE: "{d}gurgle."
* Heart: the rain over the united park is warm. Every citizen stands in it. Nobody runs for cover. Tallyworn: "First rain in ten years that wasn't somebody's." `[NO BUTTON]`
* Reward: **Vapula's Tiny Throne** (furniture), unlock the Ninth Cloud as an Arena level, "Duke of Nothing" trophy, 200 H$. Later: the weather forecast (Drizzle, 57) reads "mild, with a chance of plush".

#### Chain U: Grumbleguts's little black book (Hellvile and Hillvile; 3 parts; adults only, see §3a rules)

**S172 "The Little Black Book"** | Grumbleguts (Breach 7 boss; from Ep 4.5) | Ch4-5 | find, deliver, talk
* Hook: Grumbleguts's little black book, the size of a phone directory, has been "mislaid" in the Underbelly. It contains every rejection he has ever received, annotated "{d}playing hard to get". He would like it back before anyone reads it. Everyone has read it.
* Beats: 1) Find it (a demon is using it as a doorstop). 2) Carry it back; it is heavy enough to slow the player. 3) Three pages fall out, each a rejection: Fangela (519) "sniffed your ID and said no", Lady Smog "declined with a curtsey", the Gate "{d}creaked". 4) He files them under "progress".
* Comic beat: every rejection is dated and rated out of ten. The Gate's is a 9. "{d}It creaked WARMLY." `{r}` (he turns to admire himself in the lava puddle on the beat).
* Heart: the last page is blank, headed "FRIENDS". GURGLE: "{d}gurgle." GRUMBLEGUTS: "{d}He says you can sign it if you want." (Choice: sign it / sign it with a heart.) `[NO BUTTON]`
* Reward: 60 H$, **Little Black Book (replica)** (decor; the pet can't lift it). Later: the news ticker: "Grumbleguts rejected x3 today. 'Progress.'"

**S173 "Profile Picture"** | Grumbleguts + Shutterbug (46) | Ch5 | photo, creator, talk
* Hook: the dating ticker (the sim's romance news) has gone wild since the Wall fell. Grumbleguts wants a dating profile. Shutterbug has agreed to one photo. One.
* Beats: 1) Three poses `[3x@15]`: the flex (belly first), the lean (rear first; `{r}`), the "candid" (eating). 2) Shutterbug takes the one photo. It is all three somehow. 3) Write the bio in the Creator's text field (3 options; one is just "{d}gate."). 4) First reply: Honkers. Honk. Second: nobody. Third: the Gate's maintenance notice, which he counts.
* Comic beat: Shutterbug: "Say cheese." GRUMBLEGUTS: "{d}Is there cheese?" `[R8]` There is not cheese. The photo captures his disappointment. It is his best angle.
* Heart: the bio he finally writes, after you leave: "Big. Loud. Kept a gate for ten years so nobody would get hurt. {..} Likes: everyone. {d}Looking for: one."
* Reward: 80 H$, Profile Photo (decor; Grumbleguts, mid-bite, winking), Romance ticker unlock "Hell's Biggest Flirt". Later: Mother Nox's potluck seats him next to Scaldie. Scaldie says lukewarm. He says it's a start.

**S174 "The Exes"** | Grumbleguts | Ch5 | combat (Arena), talk, cooking
* Hook: three of his exes (all demons; all adults; all consensual and all over it) turn up at Brimstone Bites during his shift to "see what the fuss is about". There is no fuss. The Gate is also here, somehow.
* Beats: 1) Each ex is a mini-boss in the Arena's new "Exes" mode, with a quip and a specific reason it didn't work out ("He flirted with my sister." "He flirted with my DOOR." "He flirted with ME. It was fine, actually. I'm here for the food."). 2) Cook each one their favourite dish (cooking) to end the fight peacefully (pacifist route) or fight (brutal route; cartoon). 3) Everyone sits down to eat. Grumbleguts does not flirt once. Everyone notices. 4) GURGLE: "{d}gurgle." Translation: "he's trying."
* Comic beat: the third ex is the Gate. It is just the Gate, on a trolley, creaking. Honkers wheeled it in. Grumbleguts: "{d}You came." The Gate creaks. `{r}`
* Heart: the exes agree, over soup: "He's a lot. {..} He never once made anyone feel small." Grumbleguts, overhearing, sucks in his gut `[HOLD 13]`. It springs out. He laughs at himself. First time. `[NO BUTTON]`
* Reward: 150 H$, Arena mode "The Exes", **Gate on a Trolley** (furniture; creaks when he visits), unlock "Playing Hard to Get" trophy. Later: the little black book's FRIENDS page has four signatures.

### 13.1 continued (stand-alone missions, group 2: S175-S182)

**S175 "The Plain Human"** | Mecha Mo (116), prop maker | Ch1-5 | fetch, housing
* Hook: Mecha Mo has made a figurine of a "plain human" for a play. It is the only object in HaHaland that doesn't wobble, breathe or blink. It unsettles everyone. Nobody will buy it. Mo wants it to have a home.
* Beats: 1) Fetch 1 Scrap (a stand). 2) Place it in your house. 3) Guests react: every visitor variant pool now has a line about it ("...is it looking at me?" "It's not looking at anything." "That's worse."). 4) Mascotty stares at it for R21. It wins.
* Comic beat: in a world where everything idles on co-prime periods, the one thing that never moves is the joke. Place it next to the pet. "beep." The figurine does nothing. "beep?" Nothing. The pet moves away.
* Scare beat: none; the mission is the only pure gag in the list, by design.
* Reward: **Plain Human Figurine** (furniture; never animates), 10 H$. Later: the house-rating visitors give it a -1 "cosy" and a +3 "conversation".

**S176 "Rest in Pieces"** | Mr. Solemn (555, Hellvile undertaker, 200 years without a smile) | Ch4-5 | talk, find, housing (the graveyard)
* Hook: the Hillvile graveyard's joke graves (Mr. Wiggles, Grandpa Puff's moustache, the Great Cake of '89) offend Mr. Solemn professionally. He wants them "done properly". The joke graves' ghosts disagree.
* Beats: 1) Visit the Graveyard scene; each joke ghost has a request (a nicer font, a bigger moustache, a candle in the cake). 2) Housing mode in the graveyard: place three proper headstones with the jokes intact. 3) Mr. Solemn reads "Here lies the Great Cake of '89. It was delicious." `[HOLD 26]`. His mouth moves 1 px. 4) He commissions a fourth: "Mr. Huggins the 1st. Came back."
* Comic beat: the moustache's ghost is just a moustache. It is a very expressive moustache. It wants serif.
* Heart: Mr. Solemn: "Two hundred years. Every funeral, a clown honked. {..} I used to think that was disrespect." `[R13]` "{d}It was company."
* Reward: 80 H$, Tiny Headstone (decor; engraves whatever you type), unlock Graveyard: Proper Mode. Later: Honkers is invited back to a funeral. He honks. Mr. Solemn nods.

**S177 "Floss Everyone"** | Flossie (517, Hellvile fang dentist) | Ch4-5 | escort, minigame, pet
* Hook: Flossie flosses everyone, without asking. Hillvile has never been flossed. Flossie would like an introduction. Hillvile would like to be asked.
* Beats: 1) Escort Flossie through Hillvile; a consent minigame: press A to *ask* before each floss; the ripple of relieved citizens. 2) Three refusals (Nana: "I've got a muffin in"; Mascotty: no; the pet: "beep."; it means no). 3) Grumbleguts volunteers. It takes ten minutes. 4) Flossie learns to ask. The town learns to say yes.
* Comic beat: flossing Grumbleguts: Flossie disappears into the mouth `[HOLD 13]` and reappears with a bagel. "{d}From Tuesday."
* Heart: Flossie: "Over there nobody said no to me for ten years. {..} It wasn't because I was good." `[R8]` "I'm good, though."
* Reward: 50 H$, Giant Floss (decor), the pet's teeth gleam in its portrait for a day. Later: the news ticker: "Dental health up 400%. Consent up 100%."

**S178 "The Weeping Widow's Plushie"** | The Weeping Widow (Breach 5 boss, "She Lost Her Plushie in 1887") | Ch3-4 (after the graveyard nest) | combat (Breach 5), find, deliver
* Hook: the Widow has wept since 1887 over a lost plushie. Pip, who knows about lost plushies, wants to find it. Sutura (542), who mends them, wants to fix it.
* Beats: 1) Replay Breach 5 to its last room without killing the Widow (the pacifist variant: dodge her tears). 2) Find the plushie in the Plushie Factory (Breach 4) archives, filed under 1887. 3) Sutura mends it (night). 4) Deliver it. The Widow stops weeping. The graveyard's puddles dry. The joke graves' ghosts applaud.
* Comic beat: the plushie is a bunny. It looks exactly like Mr. Flops. Pip, holding both: `[HOLD 21]` "{d}...Grand-Flops."
* Heart: the Widow, dry-eyed for the first time in 139 years: "I didn't know what to do with my hands." SUTURA: "{d}Hold it." She does. `[NO BUTTON]`
* Reward: 120 H$, unlock Breach 5: Dry variant (the Widow is a friendly NPC), **Grand-Flops** (furniture; Pip visits it). Later: the Widow attends the Mingle and cries once, happily. The puddle is heart-shaped.

**S179 "Bone Cathedral Acoustics"** | Cantor Coo (426) + Dirgelet (766, Hellvile doom choir singer) | Ch4-5 | concert, combat (Breach 6), studio
* Hook: Bishop Bonebottom's Bone Cathedral (Breach 6) has the best acoustics in either town and is full of demons. Cantor Coo wants to play disco hymns in it. Dirgelet has big lungs and bigger feelings.
* Beats: 1) Clear Breach 6 (or sneak the pacifist route past the Bishop, who is asleep in the pulpit). 2) The Studio: write a hymn that both choirs can sing (a constrained editor: Hillvile major, Hellvile minor, one shared chorus). 3) The concert in the cathedral, both choirs, Seraphail (544) singing backwards and still sounding lovely. 4) The Bishop wakes up and joins the bass section.
* Comic beat: Cantor Coo plays at the speed of disco. Dirgelet weeps at the speed of a dirge. The piece works only because Riffmaw tunes the bells by playing too loud.
* Heart: Dirgelet: "Ten years I sang into a Wall. {..} It's a good wall for singing into. {d}It's a better one to sing through." `[NO BUTTON]`
* Reward: the song "Two Choirs" in the jukebox, 120 H$, unlock Breach 6: Choir variant, **Bone Organ (miniature)** (furniture; plays the hymn). Later: the church bells in Hillvile ring forward, on the disco beat, forever.

**S180 "The Sweet Tooth Sewers"** | Nana Puddin (1) + Gumdrop Gutterking (Breach 3 boss) | Ch3-5 | combat (Breach 3), cooking, fetch
* Hook: the Sugar Rush Sewers are made of candy because ten years of Hillvile frosting went down the drain. The Gutterking is made of it. Nana wants her sourdough starter back; it rolled down there in Ep 3.3.
* Beats: 1) Breach 3: the starter is a mini-boss, bubbling, in the Gutterking's throne room. 2) The Gutterking's deal: "Don't lick the floor. {..} Bake me something that ISN'T sugar." 3) Cook bone-flour bread (S112 unlock) and bring it. 4) He eats it, cries, gives back the starter and a bag of "the good sugar".
* Comic beat: everything the Gutterking says, he licks after. "Don't lick the floor." *lick*. "{d}Do as I say."
* Heart: Nana: "Everything sweet we didn't want went down here and grew a king." GUTTERKING: "{d}I'm a very sad king." NANA: "Come up. I'll teach you bread." He does. He can't fit through the door. Rivet widens the door.
* Reward: Sourdough Starter (item; the bakery sells sourdough from now on), 100 H$, unlock Breach 3: Bakery variant, **Candy Floor Tile** (furniture; the pet licks it). Later: the Gutterking is at the festival bakery stall, licking a loaf.

**S181 "Foreman Fluffnuts Retires"** | Foreman Fluffnuts (Breach 4 boss) + Rivet (13) | Ch4-5 | combat (Breach 4), weld, housing
* Hook: the Plushie Factory's foreman turned plushies into demons because that was the only order he ever got. With Vapula gone there are no orders. He is still running the conveyor. Rivet wants the conveyor.
* Beats: 1) Breach 4: shut down the conveyor (a weld-puzzle under fire: three valves, the timing from S105). 2) Fluffnuts, idle, does not know what to do with his hands. 3) Weld the conveyor into a furniture-maker (weld). 4) Fluffnuts's first product: a plush of you. Then a plush of the pet. With horns.
* Comic beat: the furniture-maker's first output is 40 identical plush Mr. Flopses `[3x]` accelerating. Pip, arriving: `[FREEZE]` `[HOLD 21]` "{d}...Brothers."
* Heart: FLUFFNUTS: "I made monsters because somebody asked. {..} Nobody ever asked for anything else." RIVET: "I'm asking. Make something soft." He does. He cries into it. It's absorbent.
* Reward: **Plushie Press** (a shop in the park: order plush furniture of any citizen you've befriended), 120 H$, unlock Breach 4: Soft variant. Later: Grumbleguts orders a plush of the Gate.

**S182 "The Upside-Down Map"** | Scoutmaster (15) | Ch4-5 | find, photo, talk
* Hook: the Scoutmaster has earned every badge, even sewing. There is no badge for "both sides of the park". He wants to make one and needs a map. Sprinklepop's soul left hers upside down on the Wall and it's the only one.
* Beats: 1) Find the map (it's on the gatepost, upside down). 2) Walk it, upside down: 6 landmarks that are mirror images of each other across the old Wall line (the bakery and Charlotte's; the daycare and the Hellvile playground; the bath house and Scaldie's sauna). 3) Photograph each pair. 4) The badge: two halves of a heart, one pastel, one purple. Both wobble. One drips.
* Comic beat: the Scoutmaster reads the upside-down map upside down to correct it and gets lost in his own town. "{d}I have a badge for this." `[R8]` "{d}I do not have a badge for this."
* Heart: the mirrored landmarks were never a coincidence: the park was built by people who wanted both halves to match. Nox: "We planned it together. Forty years ago. {..} Hee. Hee." (She has his laugh too.) `[NO BUTTON]`
* Reward: **Both Sides Badge** (decor; the pet wears one), 80 H$, unlock the full park map in the quest log. Later: quest #48 Explorer's Club gives the badge to every citizen who crosses.

#### Chain T: Feed the Gate (Grumbleguts; 4 parts; the last one is the post-game set piece from the user's notes)

**S183 "First Hellvile Regular"** | Grumbleguts, at Brimstone Bites | Ch4 (after Ep 4.5 and 4.6) | cooking, talk
* Hook: Grumbleguts has sat down at Brimstone Bites ("It's ironic!") and ordered the menu. The whole menu. Taco Yaki (128) is out of everything. The restaurant would like him to stay. The kitchen would like help.
* Beats: 1) Cook three Brimstone Bites dishes (cooking minigame; the Bites specialities: Brimstone Bagel, Lava Cake, Soul Soup). 2) Serve each; he rates it ("Six. Needs salt." "Eight. {d}Needs more."). 3) GURGLE rates it too, honestly. 4) He pays with the gate key. Taco Yaki frames it.
* Comic beat: he eats with his back to the room; the camera cuts back to him three times; he is bigger each time `[3x]`. Honkers offers a napkin. He flirts with the napkin. `{r}`
* Heart: GRUMBLEGUTS: "Ten years, the Duke said 'there'll be a feast when the gate opens'. {..} Gate's open." He looks at the empty plates. "{d}Huh. He was right." GURGLE: "{d}gurgle." `[NO BUTTON]`
* Reward: 80 H$, Brimstone Bites menu item "The Gatekeeper" (a dish that is all the dishes), **Framed Gate Key** (decor). Later: the news ticker: "Brimstone Bites hires second cook. First cook: relieved."

**S184 "Line Cook"** | Taco Yaki (128) + Grumbleguts | Ch4-5 | cooking, minigame, timing
* Hook: Grumbleguts wants to cook, not just eat. Taco Yaki flips takoyaki with a toothpick and a flourish and has never had an apprentice who ate the ingredients.
* Beats: 1) A cooking duet: you and Grumbleguts on two burners; every third ingredient disappears (he ate it). 2) The timing minigame: flip on the beat; his flips send takoyaki across the room, which the pet catches. 3) His first dish served to someone else: Lady Smog. She says it's lovely. He sucks his gut in `[HOLD 13]`; it springs out `{r}`. 4) GURGLE, for once, says nothing. It's full.
* Comic beat: "I only had a light snack" as a forklift beeps in with the restocking order. Taco Yaki: "{d}That's the week's."
* Heart: Grumbleguts, plating: "Nobody ever ate anything I made. Nobody ever came up to the gate." TACO YAKI: "{d}They're coming now." The queue is out the door. `[NO BUTTON]`
* Reward: 100 H$, **Chef's Hat (XXL)** (decor), unlock cooking recipe "Gatekeeper's Takoyaki". Later: Flossie flosses him after every shift. He lets her.

**S185 "Come Up and See My Gate"** | Grumbleguts | Ch5 | housing, fetch, talk
* Hook: the Gate is repaired (the crack from Ep 2.7 welded by Anvil and Rivet). Grumbleguts wants to decorate the gatehouse "for visitors". Nobody has ever visited. He has flowers (Florist Monster, S162), a heart (Stencilla), and no idea.
* Beats: 1) Housing mode in the gatehouse (a 4x4 room; his furniture is enormous, every piece is a seat). 2) Place a mirror (he insists; he admires himself; `{r}`). 3) Invite three guests (talk to three citizens; each gives a reason not to come and comes anyway). 4) The visit: Fuzz comes. He sits. He does not say "no" once, because Grumbleguts does not ask.
* Comic beat: every seat in the room is sized for him. The guests sit three to a chair. Fuzz, between Nox and Lady Smog, is lifted off the ground by the cushion. "{d}This is fine."
* Heart: GRUMBLEGUTS: "So this is it. The gate. {..} Ten years." NOX: "It's a good gate." GRUMBLEGUTS: "{d}...Nobody ever said that." A little heart appears painted on the Gate the next morning. Nobody knows who did that. Everybody knows. `[NO BUTTON]`
* Reward: **Gatehouse** (a visitable place), 150 H$, **Very Large Seat** (furniture; three citizens fit in it), unlock "Come Up Sometime" trophy. Later: the Gate's maintenance notice reads "visitors: 4".

**S186 "The Feast of Rebuilding"** (optional title card: **"What an Ass"**) | Grumbleguts | Ch5 (after the credits) | talk, cooking, exorcism, find, timing; **two endings, one permanent**
* Hook: Grumbleguts gathers **everyone you know** (your friends list and party citizens, from the sim's relationship data) in Hellvile Park and begs for a giant feast so he'll have "more energy to rebuild the town faster" (he claims). Mother Nox hosts. Nana, Charlotte and Scorchmello cook. The buffet is the length of the old Wall.
* The twist: **one of the guests is possessed**. The culprit is drawn from the citizens actually present (a hash of the save, never a kid, never the Mayor or Nox). The game tells you only: "Something at this table isn't hungry." There is **one more exorcism** to be had.
* The clock: a **fullness meter on Grumbleguts's belly** (and, per the user's notes, on his rear: the belly inflates in front, the rear behind; both are tracked). It ticks up through the quest with every course. When it fills, the outcome cutscene plays.
* Beats:
  1. **Arrival.** He eats with his back to the crowd and the camera. Low angle from behind the crowd, rear framed front and centre, every eye-line in the crowd on it. The camera keeps "accidentally" cutting back `{r}` each time. Bigger every cut `[3x]`. Honkers, napkin, flirt, napkin not interested.
  2. **Clue-gathering** (talk; 8-12 guests have one line each): who hasn't eaten, who said "we", whose shadow is late (W7, W8, W9 return as the detective kit), who laughed a beat late. Three clues narrow the culprit to one; a fourth confirms. The pet sniffs ("beep." near the culprit, "beep?" elsewhere).
  3. **Courses.** Between clue rounds, a course lands `[DROP]` and the meter rises a step; a strap pings off `[SLAM]` on steps 3 and 6; a chair gives way on 7.
  4. **The exorcism** (if you choose it in time): the full ritual on the culprit, in public, with Fuzz; the true-name phase draws from guests' names. Win: the culprit sits down dizzy, "Why do I taste grape?"; the feast ends happily; Grumbleguts is "merely too full to move for a day" and is wheeled home on the Gate's trolley, waving. Lose (three fails) or **choose not to** ("let's see what happens" is an explicit choice; so is simply running the clock) → the outcome cutscene.
  5. **The outcome cutscene** (the user's spec, staged to the letter; cartoon, non-sexual, slapstick vanity-and-gluttony punished):
     * The crowd watches him gorge. The possessed culprit sneaks behind the crowd into direct view of his rear, turns to the camera, wiggles their eyebrows, thumb pointing at it: "{d}Look who's full of himself, now?" `[HOLD 21]` `{r}`.
     * They produce **an absurdly long hose**. **The hose shot**, side-on medium: his rear fills one edge of frame, the culprit on the other, feeding the hose in like a **reverse rope climb**: hand-over-hand pushing it away and in, feet braced, tongue out, grunting, the coil on the ground shrinking. The music drops to **a single plucked note per pull**. Rule of three: a pause, "surely that's enough", it keeps going; a second pause; it keeps going. Cut to the crowd in silence `[MUTE]`. Back to the climb. The insertion itself is offscreen behind the cheeks: implied by hose motion, a squeaky SFX, and his slow dawning look over his shoulder `[R13]`.
     * **The valve crank.** He inflates like a parade balloon: squash and stretch `[BOING]`, buttons popping `[3x@15]`, harness straps pinging off, the pants' seams going, the chair collapsing, the rear ballooning biggest of all; **two false "about to pop" beats** (`[HOLD 13]` each; a last gurgle from the belly on the second), and then the **POP**:
       * CUTE gore setting: confetti, plush stuffing, hearts, a halo `[FLASH w10,5]` `[SHK_ROAR]`.
       * BRUTAL: a red shower and gibs `[FLASH r10,7,4]` `[SHK_ROAR]`, the belly landing last, still gurgling.
     * **The last beat:** a lone pair of cheeks drifting down like a balloon with a slow raspberry (cute) / landing with a squelch (brutal). The culprit gives the camera a "{d}well, that happened" look. `[IRIS○ out 10 on the cheeks]`
  6. **Consequence (permanent for that save):** Grumbleguts is **RES_DEAD** and non-interactable for the rest of the file. A graveyard plot with a comically oversized headstone: "He was full of himself." His gatehouse goes empty; his dating profile shows "deceased"; his bestiary and almanac entries update; the Brimstone Bites menu gains a memorial dish ("The Gatekeeper, in memoriam"); friends grieve and mention it; **no ghost rite can bring him back** (a flag blocks it). The culprit, de-possessed by the pop, remembers nothing and feels terrible.
* Comic beat: the entire scene is one; see above. Honkers honks at the funeral. Mr. Solemn nods.
* Heart (either branch): GURGLE's last line on the save branch, as he's wheeled home: "{d}gurgle." GRUMBLEGUTS: "{d}He says thank you for the exorcism. {..} And is there dessert." / On the pop branch: the FRIENDS page of the little black book, found in the gatehouse, now has your signature and a heart on it, and nothing else.
* Reward: **Save branch:** "Hellvile's Biggest Spoon" trophy, **Spoon of Rebuilding** (furniture), 300 H$, unlock "Feast: Saved". **Pop branch:** unlock "Feast: What an Ass", **Oversized Headstone (miniature)** (decor), 300 H$. Both are unlocks; the bestiary records which you got. Later (save branch): he rebuilds the town's gatehouse roof himself, with the energy from the feast. He was right, annoyingly.

### 13.1 continued (stand-alone missions, group 3: S187-S195)

**S187 "The Sin Bin"** | Mopsworth (547, Hellvile sin-bin janitor) | Ch4-5 | fetch, find, housing
* Hook: Mopsworth mops up sins and wrings them out. Since the fountain was re-plumbed two-way, "sins" are coming through the pipes from Hillvile: tiny ones. Unsent apologies, mostly. He needs a bigger bin.
* Beats: 1) Fetch 3 Scrap; Rivet welds a bin. 2) Find 5 Hillvile "sins" bubbling up in Hellvile: "I ate Nana's muffin and blamed the pet", "I never liked grape", "I'm scared of the dark". 3) Return them to their owners, who say them out loud (the Ep 4.6 mechanic as a side verb). 4) The bin is empty by evening. Mopsworth is unemployed. He is thrilled.
* Comic beat: the biggest "sin" is the Mayor's: a bubble the size of a bathtub that just says "hee." Mopsworth wrings it out. It takes three wrings `[3x@15]`. "{d}That's a lot of hee."
* Heart: Mopsworth, out of a job: "Ten years mopping up things nobody said. {..} Turns out you just have to say them." He becomes the park's gardener. The bin becomes a planter.
* Reward: 60 H$, **Sin Bin Planter** (furniture; grows one flower per honest thing said in the Thinking Chair, S168). Later: the Mayor says "hee" and nothing comes out of the pipes.

**S188 "The Prank Call"** | Ringring (535, Hellvile prank caller) + Ringaling (533, telemarketer) | Ch4-5 | talk, timing, find
* Hook: "Is your fridge running?" Daily, for ten years, to every phone in Hillvile. Nobody picked up; the lines were cut. Now they're not. Ringring wants to make one prank call that lands. Ringaling wants to sell something. Anything.
* Beats: 1) Help write the prank (3-option choices; the timing minigame: the pause before "running" is the joke). 2) Choose a target (the Mayor, Hattie, or Mascotty). 3) The call. The target's reaction is the payoff: Hattie improvises back; the Mayor says "Hee. Hee." and buys whatever Ringaling is selling; Mascotty picks up and says nothing for R26 and Ringring hangs up, terrified. 4) Ringaling sells one thing: a Two-Way Mailbox subscription. To Lilypad Lou.
* Comic beat: Ringring's ten years of unanswered calls are logged in a book: "7:00 a.m. No answer. 7:01 a.m. No answer." `[R13]` "{d}7:02. Hung up on myself."
* Heart: Ringring: "I kept calling so someone over there would know we were still here." RINGALING: "{d}I kept calling because of the commission." `[R8]` "...And that."
* Reward: 50 H$, Rotary Phone (furniture; rings once a day with a prank), unlock "Is Your Fridge Running" jingle. Later: Mascotty calls Ringring back. Says nothing. He's honoured.

**S189 "Pear-Flavoured Everything"** | Cauldra (527, Hellvile potion brewer) + Sandwichy (4) | Ch4-5 | cooking, fetch, minigame
* Hook: Cauldra's potions all taste of pear. Hillvile's everything tasted of grape for a chapter. Sandwichy, who cuts every crust into a heart, wants a flavour summit.
* Beats: 1) Fetch 3 ingredients from each side. 2) Cooking minigame: the blind taste test from Ep 2.2, reversed: every dish must taste of *itself*. 3) Cauldra's potion of "Not Pear". It tastes of pear. 4) Sandwichy cuts it into a heart anyway.
* Comic beat: the taste test `[3x@15]`: "Pear." "Pear." `[R13]` "{d}...Grape." Everyone freezes `[FREEZE ALL]`. It's a grape. An actual grape. Cauldra: "{d}I grew it. It's fine." It's fine.
* Heart: Cauldra: "Ten years everything tasted of pear because that's what we had. {..} You lot had grape because that's what *he* had." She makes a pear-and-grape potion. It tastes of both. "{d}That's the summit."
* Reward: 60 H$, Potion Rack (furniture), unlock recipe "Both". Later: the Fresh Air Lounge sells pear water. Vex says it's terrible. He has two.

**S190 "Creak on Cue"** | Creakster (548, Hellvile jump scarer, "a true artist") | Ch4-5 | timing, housing, talk
* Hook: Creakster makes floors creak on cue. The horror in your house in Ch2 was Wisp, but the *creaks* were him, practising from under the Wall. He wants to apologise by scoring your house "properly".
* Beats: 1) Housing mode: he marks 6 floorboards; you place furniture so a guest's path crosses them in the right order (a layout puzzle). 2) The timing minigame: creak on the beat as Mascotty walks the path (she doesn't react; that's the joke). 3) Pip walks the path. Pip reacts to every creak. 4) Creakster takes a bow. The floor creaks. He didn't do that one.
* Comic beat: the last creak `[HOLD 13]` with nobody on the board. Creakster: "{d}...I've been doing this for two hundred years and I have never been scared in my life." `[R8]` "{d}Huh."
* Scare beat: the uncued creak is Wisp, visiting, in a body, who learned it from him through the Wall. "{d}hi." Creakster screams. It's a very good scream. Everyone claps.
* Reward: 60 H$, **Creaky Floorboard** (furniture; creaks when the pet crosses it), unlock "A True Artist" trophy. Later: the Fun Fair's haunted house hires him. The reviews say "too good".

**S191 "Sulfur Shanties"** | Barnaby (768, Hellvile sulfur pirate, sings shanties so sad the fish weep) + Capt. Whisk (48) | Ch4-5 | fishing, concert
* Hook: the Lake of Fire's fish weep when Barnaby sings. Capt. Whisk names fish and lets them go. Together they want a fishing concert where nobody cries. The fish have opinions.
* Beats: 1) Fishing at the Lake of Fire (lava fish; the Fishdex grows). 2) Barnaby sings; the fish weep; the lake rises. 3) Whisk suggests a happy shanty. Barnaby has none. The Studio: write one (a constrained editor in a sea-shanty rhythm). 4) The concert on the pier: the fish don't weep. One of them is Gerald (S144). It laughs. Fish can laugh now.
* Comic beat: Barnaby's "happy" shanty is about a boat that sinks but everyone's fine. The fish weep. Whisk: "{d}Try one where the boat floats." BARNABY: "{d}...Where's the song in that."
* Heart: Barnaby: "Ten years singing to fish because the fish were the only ones allowed to cry." `[R13]` "{d}Good listeners, fish."
* Reward: the song "Boat Floats" in the jukebox, 80 H$, Fishdex: Lava fish (6 species), **Weeping Fish Tank** (furniture; the fish laugh when music plays). Later: the news ticker: "Lake of Fire water level: normal for the first time in a decade."

**S192 "Number Nine"** | Number Nine (514, Hellvile number caller, "has called it for 80 years") | Ch4-5 | talk, find, timing
* Hook: Number Nine calls "number nine" at the Hellvile bakery. Nobody has ever been number nine. They want to know who number nine *is*. The ticket is somewhere in Hillvile.
* Beats: 1) Find ticket #9 (a scavenger hunt: Keysie's spare keys, the Mayor's drawer, Mascotty's paw). 2) It's in Nana's recipe tin, from the last Mingle, eleven years ago: "Charlotte's turn: #9". 3) Bring Charlotte to the counter. 4) "Number nine." `[HOLD 26]` "{d}...Here."
* Comic beat: Number Nine has a voice like a foghorn and a body like a thimble. Every "number nine" is `[SHK_TAP]`. After eighty years they have never once said "number ten". "{d}Why would I."
* Heart: Charlotte gets her bread, eleven years late. Nana baked it. It's warm. Number Nine, for the first time: "{d}...Number ten." Nobody has a ten. They call it anyway. `[NO BUTTON]`
* Reward: 50 H$, Ticket #9 (decor), unlock "Number Ten" jingle. Later: Mandible (S123) stamps the ticket "ON TIME".

**S193 "Dronezilla Sulks"** | Dronezilla (557, Hellvile siege drone, "circles the Wall, sulking") + Prof. Fizz (69) | Ch4-5 | combat (Arena), weld, pet
* Hook: Dronezilla circled the Wall for ten years. The Wall is gone. They are still circling where it was, sulking. Fizz wants to recalibrate them. Rivet wants the parts. The pet wants to ride it.
* Beats: 1) Catch Dronezilla (an Arena mini-mode: a non-lethal chase; the pet rides it when caught; "beep."). 2) Weld a new purpose: a 3-option choice (weather drone, festival-lights drone, "{d}still a siege drone but for the buffet"). 3) Fizz calibrates it. It beeps at everything. Fizz: "{d}That's a feature now." 4) Dronezilla circles the *park*, lit up, humming Ploddy's lullaby.
* Comic beat: Dronezilla's sulk is a hover 1 px lower than normal. Fizz measures it. "Sulk confirmed: one pixel."
* Heart: DRONEZILLA: "Target: Wall. Target: not found. Target: not found. {..} {d}Target: everyone?" FIZZ: "{d}Confirmed." `[NO BUTTON]`
* Reward: 80 H$, **Dronezilla (miniature)** (furniture; circles the room), unlock Arena mode "Catch the Drone". Later: the festival's lights are a drone. The pet is on it.

**S194 "The Marquis of Mire's Dinner Party"** | Marquis (541, Hellvile loyalist, "lives in a swamp castle, smells royal") | Ch5 | housing, cooking, talk
* Hook: the last loyalist is holding a dinner party in protest, for Vapula, who is a cushion. Nobody has come. He would like you to decorate so that "it looks like people came".
* Beats: 1) Housing mode in the swamp castle: place 12 chairs. 2) Invite real guests (talk to 6; Lady Smog, Capt. Cumul (532), Tastebud (529), Hazey (788), Vapula the cushion, the Gate on its trolley). 3) Cook; Tastebud tastes for curses and hiccups. 4) The Marquis makes a toast to the Duke. The Duke rains on the soup. Everyone eats. It's good soup.
* Comic beat: Hazey tries to announce Vapula's arrival with the foghorn. Vapula is already on the table. The foghorn plays anyway. Vapula: "{d}thank you." `[R8]` HAZEY: "{d}...you're welcome, my lord."
* Heart: the Marquis: "I stayed loyal because the alternative was admitting we'd followed a cloud." LADY SMOG: "We did follow a cloud." MARQUIS: "{d}Yes. {..} He's quite a nice cushion."
* Reward: 100 H$, **Swamp Throne** (furniture; smells royal), unlock "Last Loyalist" trophy. Later: the Marquis's castle is on the park map as "Mire Manor: dinner Fridays".

**S195 "Glowbug's Constellation"** | Glowbug (12), firefly herder | Ch5 (the Mingle) | pet, minigame, photo
* Hook: Glowbug herds fireflies into star shapes on festival nights. For the first Mingle in eleven years they want to make the biggest one ever: a thousand fireflies, both sides' names. They need a second herder. The pet glows a little, now, when it's happy.
* Beats: 1) The herding minigame (the pet as the herder; the fireflies follow its glow). 2) Spell the shape: a 3-option pick (a heart; two faces; "the fountain"). 3) At midnight the constellation lights over Hellvile Park. 4) Photograph it. Shutterbug hangs it beside the "before" board (S147) and the "after" (#45).
* Comic beat: the fireflies spell "HAHALAND" and then, drifting, "HAHALANO". Glowbug: "{d}Close enough." `[R8]` A single firefly fixes the D.
* Heart: the shape lights up and 1000 citizens look up at once `[!RIPPLE]`. The Mayor and Nox, hands held. Tallyworn, not counting. Mascotty, no head. Grumbleguts, eating, looking up anyway. The pet, horned, glowing. `[HOLD 26]` `[NO BUTTON]` The game's last sincere beat: nothing undercuts it. The button is the title screen.
* Reward: **Firefly Jar** (furniture; glows at night), 200 H$, unlock "Constellation" trophy and the "Mingle Night" title-screen variant. Later: none. This is the last "later".

### 13.2 Mission index by chapter (first availability)

| Ch | Missions | Count |
|---|---|---|
| 1 | S100, S107, S110, S121, S124, S127, S130, S134, S141, S142, S144, S145, S151, S154, S157, S175 | 16 |
| 2 | S101, S103, S108, S111, S114, S115, S117, S131, S135, S137, S146, S147, S148, S158, S160, S163, S166 | 17 |
| 3 | S102, S104, S118, S122, S125, S128, S138, S152, S155, S161, S167, S178, S180 | 13 |
| 4 | S105, S106, S109, S112, S116, S119, S123, S126, S129, S132, S133, S136, S139, S149, S150, S153, S156, S159, S162, S164, S168, S172, S176, S177, S179, S181, S182, S183, S184 | 29 |
| 5 (post-game) | S113, S120, S140, S143, S165, S169, S170, S171, S173, S174, S185, S186, S187-S195 | 21 |

Verb coverage: talk (all), fetch (41), deliver (9), find (22), photo (12), combat (13), exorcism (5), weld (10), pet (9), minigame (12), fishing (2), cooking (12), concert/studio (7), housing/decorating (26), creator (3), escort (8), timing (12), dream (3). Furniture rewards: 61 pieces (§14.5). Givers: 44 Hillvile, 36 Hellvile, 6 bosses/plushes, 10 joint. Cross-border chains: B, D, E, F, G, I, J, K, M, N, O, P, Q (13).

---------------------------------------------------------------------------------------------------
## 14. GAMEPLAY INTEGRATION: where everything unlocks

### 14.1 The unlock map (main path)

| Hour (approx.) | Episode | System that opens | Notes |
|---|---|---|---|
| 0:00 | 1.1 | Overworld, dialogue with player choices, wrongness creep W1-W6 | the player's first lines; `SF_ASKED_OTHER_SIDE` |
| 0:20 | 1.2 | **Housing: decorate mode** (place, rotate, pick up, rate) in the Pink Door Cottage | the room-rating stamp; the furniture catalogue; `home_phase = COSY` |
| 0:40 | 1.3 | Pet (hatch), "beep." runner | `SCENE_PET` arg 0 |
| 1:00 | 1.4 | Open Mic **comic-timing minigame**; bath-house and garden mini-tasks | replaces the C1_FRIENDS counter |
| 1:30 | 1.5 | **Hosting** (invite guests; visitor reaction pools); the loft | `Neighbourly` score |
| 2:00 | 1.6 | CUT_FOG on a beat script; night walking; the purple tint | first `[MUTE]` windows on the overworld |
| 2:20 | Int. I | Peek at a Dream used by the story; the pet's first 4-frame insert | `SF_PET_NUBS` |
| 2:30 | 2.1 | Cloud Nine; the vape choice that matters | `SF_VAPE_SAMPLE` is read later |
| 3:00 | 2.2 | **Investigation** (Clues board in the quest log); photo verb in story; cooking minigame in story | replaces the C2_WEIRD counter |
| 3:30 | 2.3 | **Haunted house** (nightly rearrangements; "placed by: ???"); camera turn in decorate mode | `home_phase = HAUNTED`; chain R |
| 4:00 | 2.4 | Overworld NPC beat scripting (Steamy's head turn); **exorcism practice as a scripted rule of three** | Mr. Huggins the 2nd |
| 4:30 | 2.5 | **Exorcism** for real (Steamy), plus the scripted failure (Sprinklepop) | authored true names |
| 5:00 | F1, 2.6 | **Welding** (workshop), weapon choice, the sign gag | `SF_WEAPON_BRUTAL` is read later |
| 5:20 | D1 | Dream sequence (forced) | the Dream scene reused |
| 5:30 | 2.7 | **Hell Breach 1** (Vape Cellar); the Gate Cracks insert (Grumbleguts); CUT_ERUPTION | the first "boss quip" HUD captions |
| 6:00 | 3.1 | Invaded map; scripted chase gags; the church hub | INVASION music with silence windows |
| 6:20 | 3.2 | **House Defence** wave 1 (combat on the interior map); welding boards; Wisp visible | `home_phase = BARRICADED` |
| 6:40-8:00 | 3.3 | Nests → **Hell Breach 3, 4, 5** entered from the story; Whack-an-Imp in story; escort (Pip) | each nest heals its region |
| 8:00 | 3.4 | **Midpoint twist**; Flashback F3 | the fountain cracks |
| 8:20 | 3.5 | Two authored exorcisms with their own minigame skins (comedy duel; memory game) | Hattie, Grampy |
| 9:00 | 3.6 | **Hell Breach 2** (the Spire, 7 floors, Vapula speaks); boss fight | reads `SF_VAPE_SAMPLE`, `SF_ASKED_OTHER_SIDE` |
| 9:30 | 3.7, F2 | The pet leaves; Flashback F2 | `[SILENCE]` |
| 9:45 | Int. III | **Dark night**: decorate mode in remove-only; the seven knocks | `home_phase = EMPTY` → `OPEN` |
| 10:00 | 4.1 | Hellvile map; the border record scratch | HELLVILE theme = HAHALAND in minor |
| 10:20 | 4.2 | **Shift Swap** bellows rhythm minigame; the Hattie/Haze payoff | replaces the C4_PRISONERS counter |
| 10:45 | 4.3 | Three favours: outdoor housing (the shared bed), the last exorcism (Smog), the Anvil scene | `L_help_done` branches |
| 11:30 | 4.4 | **Decorating someone else's home** (Wisp's room); second home (Hell Pits flat) | opacity-as-progress |
| 11:45 | D2 | Dream D2 | `SF_VAPULA_PLUSH` |
| 12:00 | 4.5 | **Hell Breach 7** (Underbelly) entered from the story; **Grumbleguts boss** (feed-to-pacify); the pet-care boss | `SF_PET_HORNS` permanent; `USTORY_PACIFIST` route |
| 12:30 | 4.6, F4 | The summit; the **"Say It"** line; two-player welding QTE; CUT_WALL_FALLS on a beat script | never undercut |
| 13:00 | 4.7 | Festival stalls (cooking, fishing, photo, comedy as 3-minute vignettes); the stage; the credits | Grumbleguts at the buffet |
| 13:30 | E1-E4 | Post-credits button; the suitcase; the sign; the Feast | free play, NG+ |

### 14.2 Hell Breach levels: every one has a story reason now

| Breach | Level | Boss | Story entry | Post-story |
|---|---|---|---|---|
| 1 | Vape Cellar | (waves) + Vex on the PA | Ep 2.7 | Arena |
| 2 | Cloud Nine Spire | Lord Vapula, 7 floors of taunts | Ep 3.6 | Arena; S105 "Echo of the Duke" (#39) |
| 3 | Sugar Rush Sewers | Gumdrop Gutterking | Ep 3.3 plaza nest (optional descent); S180 | Bakery variant |
| 4 | Plushie Factory | Foreman Fluffnuts | Ep 3.3 beach nest (Mr. Flops); S102, S178, S181 | Soft variant; the Plushie Press shop |
| 5 | Graveyard Shift | The Weeping Widow | Ep 3.3 graveyard nest; S178 | Dry variant (the Widow is friendly) |
| 6 | Bone Cathedral | Bishop Bonebottom | Ch4 optional (Mother Nox's chapel annex); S179 | Choir variant |
| 7 | Hellvile Underbelly | Gatekeeper Grumbleguts | Ep 4.5 (mandatory; feed-to-pacify available) | Arena; S171 Ninth Cloud remix; S174 "The Exes" |

Boss intro cards (`lvd_boss_intro`) and between-wave quips are shown in the story runs too; `levels.c` boss text is updated to match §3a (Grumbleguts) and Ep 3.6 (Vapula's seven lines).

### 14.3 Invasions and nests
* **C3_NESTS** stays four nests on the invaded map, but each nest is a set piece with a named NPC, a Breach descent and a region-heal reward. Clearing a nest drops the local tint one level and brings that region's citizens out with one authored line each (12 lines per nest, hash-picked).
* **Optional Night Raids** (Ch3 only, after Ep 3.2): each night at home, a House Defence wave at the current difficulty, with Wisp's help. Clearing three raids grants "Homeowner" and the turret skin. They stop when the Spire falls.
* **Walk-in encounters** stay as today (`script_nest_encounter`) and gain Rivet's rate-limited radio quips.

### 14.4 Exorcisms (authored, with skins)

| Target | Episode | Skin on the ritual | True name |
|---|---|---|---|
| Mr. Huggins the 2nd | 2.4 | the scripted rule of three (teaches all three parts) | — |
| Sprinklepop | 2.5 | scripted failure (the trailer's s11) | WHEEZLE (never offered; she is lost by design) |
| Steamy | 2.5 | standard ritual, odds shown | TALLYWORN |
| Hattie Haha | 3.5 | the prayer phase is the Open Mic timing minigame; she heckles | HAZE |
| Grampy Bean | 3.5 | the true-name phase is a tomato memory game | SOOTKIN |
| Lady Smog | 4.3 | hardest; Fuzz leads; GIGGLESWORTH as a red herring | VAPULA |
| Sprinklepop (reverse) | S133 | the ritual backwards; "EXORCISM UNFAILED" | SPRINKLEPOP (typed) |
| Mr. Huggins the 9th | S120 | standard, in public; survives | — |
| The Feast culprit | S186 | standard, clue-gathering first, drawn from the guest list | a guest's name |
| Side quests #16, #24, #30, #37 | as today | standard | hash-picked (unchanged) |

Three failures still lose the soul (`resident.h:157`), except where the bible says "scripted". Lost souls now have consequences: graveyard beats, the Interlude III visitor, S128's branch, S133's chain.

### 14.5 Housing: visit, decorate, build, defend

**Modes.** Decorate (your homes), Decorate-for (someone else's room; a target layout with tolerance), Host (invite; reactions; Neighbourly score), Defend (Ch3 waves), Build (post-game Park Plot: plot, walls, rooms). Isometric on GBA (pre-rendered iso sprites sorted by x+y; the held item is an affine OBJ), 2.5D on PS1 (`plat_overworld` on a small cell map; furniture as textured quads). Timing per §6.4 of the timing bible.

**Decorating goals per chapter** (the "Cosy" and "Neighbourly" grades gate small rewards, never the plot):

| Phase | Goal | Reward |
|---|---|---|
| COSY (Ch1) | Cosy B+ with the welcome basket; host 3 guests (Ep 1.5) | the loft; Nana's chair |
| HAUNTED (Ch2) | keep Cosy B while ??? rearranges; place a night-light next to the little bed (S166) | Wisp's Drawing #1; the "Who's There" events stop waking you |
| BARRICADED (Ch3) | survive wave 1 (story) and 3 Night Raids (optional) without losing the heart rug | Homeowner trophy; turret skin; `USTORY_PACIFIST` progress if no brutal weapon |
| EMPTY (dark night) | none (remove-only) | the seven gifts, tagged, unsellable |
| OPEN (Ch4) | Neighbourly B at the cottage with 3 Hellvile items; rebuild Wisp's room; Hell Pits flat Cosy C | Wisp wakes; the second home; S168 |
| DREAM (post) | Park Plot: build a room with items from 10 different givers | "Everyone Brought Something" trophy |

**Furniture catalogue** (61 reward pieces from §13 plus the welcome basket, mini shops and the Plushie Press). Each has: iso sprite (GBA) / quad (PS1), footprint, rotation frames (2 or 4), an idle animation flag, a `giver` tag, a `cursed` flag (pentagram rug, Vapula's crown trophy, Little Black Book: the 4-frame hell insert with one persistent change on placement), and a reaction pool id. Gag props from the timing bible: the "Days Since Last Fire" sign (S103), the horned-poop plushie (E1), Kevin's three-eyed bust (Creator reward), the Plain Human figurine (S175).

**Save:** the layout of each owned home (cells + item id + rotation + giver tag) and `home_phase` live in a new `'HOME'` `save_ext` chunk; a cottage of 10x8 cells with up to 48 items is under 400 bytes per home, four homes under 2 KB (the GBA story slot has room; §15).

### 14.6 Free play, post-game, NG+
* **Free play island** visits (`STORY_ARG_VISIT_*`) stay plotless, but the running-gag spawner (§6.3 of the timing bible) and the housing Visit mode run there too.
* **Post-game** (after `BEAT_DONE`): E1-E4; chains L, T (S185-S186), U; the 21 Ch5 missions; the Mingle calendar event; Arena variants of Breach 3-7; the Plushie Press; the Two-Door Clinic; Mixed Bathing Fridays; the comedy tent.
* **NG+ "HaHaland Remembers"**: horns from the start; gag props kept; wrongnesses subtitled; NG+ lines; the Feast's culprit is re-drawn.
* **The title screen** changes after the credits (the logo on fire, "(it's fine.)"), after S143 ("Population: 1000") and after S195 (Mingle Night variant).

### 14.7 Choice consequences (the choices the game now remembers)

| Choice | Where it echoes |
|---|---|
| Ep 1.1 "What's on the other side?" | Ep 3.6 floor 4; the Mayor's dark-night line; NG+ |
| Ep 2.1 one puff / decline | Ep 3.6 floor 3; Ep 3.7 the fog's address; S160 |
| Ep 2.6 cute / brutal | Rivet's lines; turret skin; Fuzz's "tsk."; `USTORY_PACIFIST`; S174 route |
| Steamy saved / lost | Ep 3.3, Int. III, S118, S128, S129 |
| Hattie, Grampy, Smog saved / lost | Ep 4.2, 4.3, 4.7 variants; S125, S126, S155, S156 |
| Ep 4.1 "That was me." | Tallyworn's chain Q; his stage line |
| Ep 4.5 pacifist Grumbleguts | he says so at Brimstone Bites; S174 |
| Ep 4.6 your "Say It" line | the suitcase photo's caption in E2 |
| S186 save / pop | permanent: the town's gatekeeper |

---------------------------------------------------------------------------------------------------
## 15. ENGINE BUDGET NOTES (what this bible costs, against `story_map.md` §0, §1, §16, §18)

1. **Text.** The main plot grows from ~2,140 words to ~16,000 (about 900 lines plus ~60 "Say It" lines and ~200 variant-pool lines), side text from ~2,300 to ~30,000 words (96 missions x ~300). On GBA that is ~250 KB of ROM strings: fine (the ROM is 2.37 MB padded to 4 MB). On PS1 it must **not** be resident: this bible assumes the table-driven script from `story_map.md` §18 (opcodes: say, choice, emote, wait, shake, sfx, move, cam, flag, goto) interpreted by one resident function, with per-chapter text packs streamed into the story overlay group (each pack under 60 KB, within the 108.8 KB slot alongside the ~49 KB of scene code).
2. **Beat scripts.** `BeatEv` is 8 bytes; the biggest script here (Ep 4.6 CUT_WALL_FALLS + Say It) is about 300 events = 2.4 KB. All cutscenes together under 25 KB, ROM/overlay.
3. **Persistent state.** New flags: ~40 plot one-shots (`SF_ASKED_OTHER_SIDE`, `SF_PET_NUBS`, `SF_PET_HORNS`, per-episode done bits, the seven knocks, `SF_GRUMBLE_DEAD` and the no-rite block) plus 96 side quests x 2 bits (24 bytes) plus per-chain step counters. The 256-bit flag array is full (bit 227), so all of it goes in a new `'STRY'` `save_ext` chunk (~160 bytes) and the `'HOME'` chunk (~2 KB). Both fit the 8 KB per-chunk cap and the 40 KB GBA story slot. The `BEAT_*` enum appends ~70 new beats (episodes and sub-beats), never reorders.
4. **EWRAM (GBA).** Nothing here adds .bss beyond the two save_ext chunks' live copies (~2.2 KB) and the beat scheduler state (~64 B). Housing and house defence run in the 12 KB `scene_scratch`. Re-measure against the GBA memory agents' tree before committing; the 7.5 KB free in the snapshot is the constraint, and the 'HOME' live copy should be loaded only while a housing scene owns the scratch.
5. **CD reads (PS1).** Story→Breach→Story costs ~3 reads as today; the bible adds Breach descents in Ep 3.3 (optional) and the mandatory Breach 7 in Ep 4.5. All smash cuts, inserts, dreams and cutscenes stay inside the story group or the resident dialogue, so no gag stalls on a seek. The housing scene is a new overlay group (or joins the story group if it stays under ~55 KB).
6. **Audio.** 13 comedy SFX (timing bible §4) and 5 new songs ("Lullaby for the Wall", "Two Choirs", "Boat Floats", the "Hee. Hee." jingle, "Is Your Fridge Running"), all MML. `audio_music_mute/duck/tapestop` are a gain and a pitch multiply.
7. **Art.** 61 furniture pieces (16-32 px iso sprites, 2-4 rotations), ~12 new portraits (Mascotty with and without the head, Wisp translucent and solid, Grumbleguts, GURGLE, Sprinklepop's soul, the plush Vapula, Haze, Wheezle, Fluffnuts, the Widow, the Gutterking), 11 expressions each where they speak. `SPR_COUNT` (54 of 64) and `TEX_COUNT` (48 of 64) need a furniture sprite bank of its own rather than more `SPR_*` ids.
8. **Tests.** `story_debug_step()` must still walk every beat: each new episode gets a debug setup, and the Feast's culprit draw is seeded by the test. The side-quest table test's bounds (`n` between 40 and 50) become a second table for S100-S195 with its own bounds (96, every kind used, ≥40 Hillvile and ≥30 Hellvile givers, reachability of every giver and spot per chapter window, Ch3 Hillvile givers also on the invaded map).
9. **Accessibility.** All of the above obeys timing bible §9: `OPT_UI_BOUNCE`, `OPT_SCREEN_SHAKE` and `OPT_REDUCE_FLASH` scale amplitude, never timing; the 4-frame hell inserts are shown darkened without strobing under REDUCE_FLASH; the Feast's pop is a 2-frame flash at most.

*End of bible.*
