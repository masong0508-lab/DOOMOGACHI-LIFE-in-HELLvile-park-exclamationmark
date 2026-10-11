# STORY BIBLE (horror-comedy-first): "Don't Talk About the Other Side"

DOOMOGACHI LIFE in HELLvile Park! is the same story on both the GBA and the PS1. The cast and the 4-chapter backbone are the canon ones from `story_map.md`. The timing vocabulary comes from `comic_timing.md`.

> **Tone target:** Shaun of the Dead, Gravity Falls, Undertale and DOOM, sewn into a plush toy.
> The pastel world slowly goes **wrong**: first in the background, then in your house, then in the sky. Every scare is undercut or doubled by a joke. Every running gag comes back at the end as something you feel.
> Violence is cartoon violence against demons. Cute characters are never shown hurt in the cute half of a whiplash cut. Nothing here mocks real people.

Cast pronouns follow `roster_list.txt` (the roster is canon). **Rivet is she, Steamy is she, Pip is they, Sootkin is xe.** Some current story lines say "him" for Steamy; §12 lists that and other continuity fixes.

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

**Homes you can own:** the Pink Door Cottage (Ch1), Hillvile Towers apartment 7B (Ch2, unlocked by side chain S114), a Hell Pits flat (Ch4) and the Park Plot (post-game).
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
| **Mailbox gag** | junk mail from "The Other Side" | the letters are all addressed to Hellvile people | the mailbox is a turret | it delivers both ways (Lilypad Lou chain) | — |

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
| **Total** | **27 episodes + 8 specials** | | **~13 h main, plus ~12-15 h of side content (§10)** |

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
  2. **CUT_ERUPTION** (as now, re-timed): the sky shifts in bands. The cloud swells, **grins**, and opens `[SHK_ROAR] [FLASH r10,7,4]`. "THE DEMONS OF HELL ARE HERE." `[SLAM]` caption. `[BUTTON: the Employee-of-the-Month frame falls off the Cloud Nine wall, tink]`
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
* Gatekeeper Grumbleguts blocks the way. **He doesn't fight; he's scared.** "There's a THING down there. It beeps."
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
* **The Unity Stage** speeches (existing, expanded):
  * the Mayor;
  * Nox;
  * Pip ("And Mr. Flops!" Mr. Flops has felt horns now);
  * Fuzz ("Even you, Vex");
  * Vex ("plain water vapes now. They're terrible.");
  * Rivet ("Hit it!");
  * then Tallyworn, who adds a last line: "Day one."
* **The credits parade** (as now): 500 pairs; the cast cheers; ghosts march "in loving memory". **New:** the parade is led by you, the pet (with horns) and Wisp (in a body).

---------------------------------------------------------------------------------------------------
## 12. EPILOGUE: "HAPPILY EVER AFTER (MOSTLY)" (0.5 h)

* **E1, the post-credits button** (the trailer's s15, in the game): back at your house. Pet, horns. Strain (8 f), plop. `[R8]` The poop grows horns `[POP]`. "beep." `[IRIS♥ out 10 on the poop]`. *THE END... of the beginning!*
* **E2, "The Suitcase"** (playable, at home): you can finally unpack the suitcase from Ep 1.1. Inside is a single empty photo frame. Shutterbug's festival polaroid goes in it. Place it anywhere. `[NO BUTTON]` This unlocks building on the **Park Plot** (DREAM phase).
* **E3, "Population: 1000"**: the sign at the town entrance. You repaint it (side mission S143); the scratched "1000" was the original number. `[BUTTON: the "0" falls off, tink. Rivet: "Don't make it weird."]`
* **The title screen changes** after the credits: the logo's HELL half is on fire. `{d}(it's fine.)`

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
