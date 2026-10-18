# DOOMOGACHI LIFE in HELLvile Park! — FINAL STORY, SIDE MISSIONS AND COMIC TIMING DESIGN

*Working title of the expanded Story Mode: **"Don't Talk About the Other Side"** (the horror bible's skeleton) with the heart bible's organs grafted in per the two judges' plans. One story for the GBA ROM and the PS1 disc.*

**Status:** design, read-only against the repo (HEAD `43af439` plus the uncommitted work measured in `story_map.md`). Nothing here touches the tree. Every bracketed number is a 30 Hz frame count from `comic_timing.md`; every id is a roster id or a `story_map.md` symbol; every byte figure marked **(measured)** is from `story_map.md` or `housing_final.md`, everything else is an estimate and says so.

**Sources merged:** `bible_horror.md` (base: plumbing twist, "Say It", Mascotty/Thumpkin, Wisp's room, the seven knocks, four homes, Grumbleguts §3a and S186, Breach table, continuity fixes), `bible_heart.md` (grafts: Fluffy and the jar, the Bottle, Fuzz's crisis, Pip and Wisp as pen pals, the three player lines, Grumbleguts' lines, Anvil's hammer, Signhere, the lint rule, ~21 missions), `comic_timing.md` (the vocabulary), `story_map.md` (the engine as it stands), `housing_final.md` (the house: modes, catalog, `HOME` chunk, GBA iso and PS1 2.5D renderers), `user_notes.md` (Grumbleguts and the Feast, honoured to the letter).

**Map of this document.**
* **Part 1 — The story** (§0-§13): premise, secret history, cast, Grumbleguts, the house, running gags, wrongness, the timing contract, the shape, and the main path as beat sheets with frame tags and key dialogue: prologue, 4 chapters (27 episodes), 4 flashbacks, 2 dreams, 2 interludes, epilogue, post-game.
* **Part 2 — Side missions** (§14): 109 new missions (S100-S217, 9 retired ids), 22 chains, 25 stand-alones, each fully specced; chapter index; verb coverage.
* **Part 3 — Engine work** (§15): the story VM and its opcodes, the beat scheduler, the tween/spring module API, camera FX, dialogue beat codes, cutscene stages, overworld NPC scripting, housing hooks, audio hooks, the comedy SFX kit.
* **Part 4 — Data and memory** (§16): text and script byte estimates, the text-pack format and compression, GBA ROM placement, PS1 per-chapter streaming into the story overlay slot, resident deltas, EWRAM deltas.
* **Part 5 — Save data** (§17): the `STRY` chunk field by field, `HOME` v2 additions, flag-bit reuse, byte counts.
* **Part 6 — Testing** (§18): unit tests, runner scripts per chapter and a side-mission sample, the timing lint, the silence budget, determinism rules, GBA and PS1 harness acceptance.

---------------------------------------------------------------------------------------------------
# PART 1 — THE STORY

## 0. The pitch in one breath

You arrive in HaHaland on a pink bus with one suitcase and nowhere you belong. The town gives you a cottage with a pink door, a muffin with a key in it, and an egg that hatches into a tiny creature that says "beep."

Everyone is lovely. Everyone giggles. Nobody talks about the other side of the Wall. Then the fountain giggles half a second too late.

Over four chapters (27 episodes, 12.5-13.5 h) you learn that HaHaland's endless happiness is **plumbing**. Ten years ago the town learned to swallow every sad and scary feeling and flush it through the Giggling Fountain, under the Wall, into Hellvile. There it fed a cloud, and the cloud became **Lord Vapula, Duke of the Ninth Cloud**: literally made of every sigh Hillvile never let out. The demons that possess your friends are breath, each one named after the Hellvile pump worker whose lungs it passed through, and shouting a demon's true name at an exorcism is the first time in ten years anyone on this side has said that worker's name.

Under the plumbing there is a face. The cloud that the glums fed was **Fluffy**: a small pet cloud that followed a nine-year-old cherub to school and rained on bullies, drifted away one Mingle night, and came back thirty years later, huge and purple and lonely, to a town that was too scared to recognise him. The Mayor dug him a joke grave. The Mayor built a Wall against him. The Mayor fed him for ten years without asking where the sad went.

The cure is not a bigger gun (there are bigger guns). The cure is the thing the town forgot how to do: **say it out loud**. In the end the Wall comes down because 1000 critter-folk each say the one thing they were scared to say, and a cherub says "Hi, Fluffy." to a cloud the size of a cupcake. Then everybody commingles in Hellvile Park.

**Theme line, said three times:** *"We don't talk about the other side."*
1. Ch1: the Mayor says it as a joke ("Hee. Hee.").
2. Ch3: the Mayor says it as a confession ("...That was the whole point. I never asked where it went.").
3. Ch4: Hattie Haha opens her festival set with it, and both sides laugh ("Because we're STANDING in it!").

**Tone target:** Shaun of the Dead, Gravity Falls, Undertale and DOOM, sewn into a plush toy. The pastel world goes wrong in the background, then in your house, then in the sky. Every scare is undercut or doubled by a joke, except the ones that are not, and those are chosen. Violence is cartoon violence against demons and hell-stuff; cute characters are never shown hurt in the cute half of a whiplash cut. Nothing mocks real people.

**Pronouns (roster is canon):** Rivet she, Steamy she, Pip they, Sootkin xe, Bolt xe, Wisp they, Mascotty she, Kevin he, Grumbleguts he, GURGLE it. Every grafted heart line has been re-pronouned (the heart bible said "him" for Steamy and "her" for Sootkin).

## 1. The secret history (what the player digs up, in pieces)

| When | What happened | Who knows | Where it is revealed |
|---|---|---|---|
| ~40 years ago | Hellvile Park is one park. The **Midsummer Mingle** is held at the bandstand at its centre every year. Little Giggs (a cherub) and little Nox (an imp) are best friends: he brings cupcakes, she brings bone soup. | Nox, the Mayor, Nana, Grampy | F4 "Cupcakes and Bone Soup" |
| ~31 years ago | Nine-year-old Giggs has a pet cloud, **Fluffy**, who follows him to school and rains on bullies ("only bullies; he was very precise"). On Mingle night Fluffy chases the fireworks south, the string slips, and nobody notices. Giggs digs a joke grave in the HaHaland graveyard: "Fluffy the cloud. Drifted." He never tells anyone what it is for. | the Mayor; Nana suspects | P0 "The Egg Crossing"; W16; S196; Interlude III |
| 12 years ago | Anvil, Rivet's mum and Hellvile's best smith, welds the **Giggling Fountain** as a Mingle gift: a laugh on one side bubbles up on the other. Rivet, six, holds the torch. | Anvil, Rivet | F1 "The Fountain Gift" |
| 11 years ago, Mingle night | A strange purple cloud rolls over the festival. It is Fluffy, grown huge on thirty years of nobody's attention and Hellvile's sulfur, come home. He brushes against **Lady Smog**, a Hellvile courtier, and she breathes him in: the first possession. Eyes glowing, she stumbles into the Hillvile kids' tent saying the children's fears in their own voices. Young Father Fuzz tries a hug. It fails. She flees. Panic. Young Mayor Giggs, new in office, does not recognise the cloud and says: "Close the park." | Fuzz (his wound), Smog (doesn't remember), the Mayor (half) | Ep 1.6 insert; F2 "Mingle Night" |
| 11 to 10 years ago | The Wall goes up over one year. Hellvile labourers are paid to build it, then **kept** to pump the cloud's vapor around "their" sky under a 900-page labour contract drafted by Signhere (567). Tallyworn starts counting on the day it is finished. | Tallyworn ("3,652 days"), Signhere | Ep 4.2, Ep 4.5 |
| 10 years ago | The Mayor has the fountain **re-plumbed** so that it "keeps the giggles in and pumps the glums out". Every frown, nightmare, swallowed scream and unsent apology in Hillvile drains under the Wall into the cloud. He never asked where it went. Fed on ten years of fear, the lost pet learns to talk and calls itself **Lord Vapula, Duke of the Ninth Cloud**. | the Mayor (half), Vapula (all) | Ep 3.4 "Under the Fountain", F3 |
| Now | Vapula is too big for Hellvile. He wants to be **breathed in**: felt by the people who made him. Vex, a fox who thinks the cloud is "the only one who ever listened", opens **Cloud Nine Vapor** by the Wall. Gatekeeper Grumbleguts sits on the sealed gate waiting for a feast that was promised ten years ago. | Vex, Haze, Grumbleguts | Ch2 |

**The midpoint twist (Ep 3.4, hour ~7):** the fountain cracks; the pipes under it, stamped with Anvil's hammer mark, run under the Wall. HaHaland's happiness has been Hellvile's weather for ten years. *We* made the monster.

**The secret under the twist (Interlude III, Ep 4.3, Ep 4.6):** the monster was a pet. The plumbing is the mechanism (so "Say It" has a cause and commingling is the cure); Fluffy is the face (so the summit has a surprise and the Mayor's "Hee. Hee." has a wound under it).

**True names (overrides the random `sim_soul.c` pick for authored possessions):**

| Possessed | True name | Payoff |
|---|---|---|
| Steamy (Ch2) | **TALLYWORN** | Ep 4.1: "Someone shouted my name across the Wall once. First time in ten years." |
| Sprinklepop (Ch2) | **WHEEZLE** (never offered; she is lost by design) | chain I |
| Hattie Haha (Ch3) | **HAZE** | Ep 4.2: Haze tells Hattie a joke back |
| Grampy Bean (Ch3) | **SOOTKIN** | Ep 4.3: the tomato seeds over the fence were xyrs |
| Lady Smog (Ch4) | **FLUFFY** (the oldest scrap of Vapula; VAPULA is a near miss, GIGGLESWORTH a strike) | Ep 4.3; Ep 4.6 |
| The leftover breath (Ch3 end) | **GIGGLESWORTH** | the Mayor's own name: the biggest swallowed fear in town was his |

## 2. Cast arcs: want, wound, payoff

| Character | Want | Wound | Arc | Payoff you see |
|---|---|---|---|---|
| **You (@P)**, default Sunny the bunnykin | a place with room for you | arrived with one suitcase you never unpack | make a home; the home turns on you; defend it; it empties; open it | you speak three times in the game (§8.4); the suitcase is unpacked in E2: an empty photo frame; the festival polaroid goes in it `[NO BUTTON]` |
| **Doomogachi (@E)** | to be near you | hatched from a Mingle egg: half plush, half brimstone | beep; horn nubs; eats the leftover breath and flees over the Wall; found at the bottom of Hellvile | horns forever; it turns its back on the Mayor's fear; post-credits the poop grows horns |
| **Mayor Giggs** (0) | everyone happy | lost a cloud at nine, closed the park at thirty, plumbed the fountain at thirty-one | "Hee. Hee." → denial → confession → "I had a cloud once" → crosses the gate | "I LOVE soup." / "You're late." / "...Hi, Fluffy." / the first real laugh / "Hold my hand?" |
| **Mother Nox** (778) | her friend back | two bowls set every Mingle for ten years | the quest hub who will not talk about it | "You're late." "I closed it." "I let you." |
| **Nana Puddin** (1) | to feed everyone | her pen pal Charlotte's cookies still arrive through a loose brick every Thursday | bakes clues into food; kept the Hellvile egg | the Unity Loaf with a key in it, for a gate that no longer exists |
| **Pip + Mr. Flops** (42) | to be brave | thinks brave means not crying | lost bunny; paper boats from the other side; night patrol; Mr. Flops kidnapped; lends Mr. Flops to Wisp | "And Mr. Flops!" Mr. Flops has felt horns. "Rain's good for stuff." |
| **Father Fuzz** (77) | to save every soul | failed his first exorcism (Lady Smog, eleven years ago); believes nothing is unhuggable | Huggins dummies die (runner); the hymnal works where the hug didn't; "What am I FOR?"; leads the Smog rite | "The hug was never for the demon. It was for the one holding the demon." Mr. Huggins the 9th survives and hugs him |
| **Rivet** (13) | to be more than "a runaway imp" | held the torch for the fountain, saw what it became, ran at fourteen | "Don't make it weird" ×3; the weapon; the sign; her mum's mark on the pipes | the hammer frozen mid-swing; "It's a REALLY good spoon."; "I have one. ...I'll have two." |
| **Anvil** (595), **Sprocket** (596), **Bolt** (597) | Rivet home | a plate set every night; letters "not read"; a sibling who stole a torch | Ep 4.3 | the hinge with the squeak left in "so you'd know when someone comes home" |
| **Dr. Bloom** (111) | to heal without being feared | Hellvile-born, trapped on this side when the Wall went up; hides it under pastel scrubs | runs the Ch2 investigation; says it out loud at the midpoint; crosses | a clinic with two doors, with her sibling Graveley |
| **Steamy** (2) | everyone warm | the bath house is lonely | first possession; saved or lost | saved: the first mixed bath; lost: her ghost keeps the water warm from the other side |
| **Sprinklepop** (81) | parties | "sneezes glitter. Doctors say it is fine." | first customer; scripted failed exorcism (SOUL LOST); her soul wanders with its map upside down | guided home by a reverse rite: "EXORCISM UNFAILED x1" |
| **Mascotty** (106), "the Starer" | to go home | a Hellvile bunny trapped in Hillvile's mascot suit for ten years; never takes the head off | "Ignore that one."; waves only if you stand still; thumps 3-then-1 back at the Wall | at the rubble the head comes off; Thumpkin thumps once; she thumps back `[NO BUTTON]` |
| **Hattie Haha** (71) | to make everyone laugh | her act is not talking about things | possessed by Haze's breath, laughs a beat late | opens her festival set with the refrain |
| **Grampy Bean** (88) | to remember | forgetting | possessed by Sootkin's breath; remembers you through tomatoes | two gardens in one bed |
| **Miss Lulla** (36) | kids who sleep | tells kids monsters aren't real | hears giggles from the Wall | "Monsters are real. They also nap." |
| **Vex** (666) | to be listened to | only the cloud ever did | Ch2 villain; hides in your closet in Ch3; exit interview | scoops the small cloud into a jar; plain-water vapes ("They're terrible."); works for Haze |
| **Lord Vapula / Fluffy** (999) | to be **felt** | a lost pet made of everything nobody wanted to feel | fog; seven floors of taunts; "Pfffft"; a cupcake-sized cloud in a jar; the leftover breath | two drops of rain in the jar when the Mayor says his name; post-game he rains on sunflowers on request |
| **Tallyworn** (712) | the Wall down | 3,652 days pumping | hears his name shouted at the Steamy rite | puts the chalk down, writes anyway: "Habit."; the last mark is a heart |
| **Sootkin** (525) | to grow something where nothing grows | a sunflower under the gate | possessor of Grampy's breath | the shared bed, rained on by a cushion |
| **Lady Smog** (502) | to taste air | eleven years possessed | first case, last exorcism | "It's DELICIOUS." She forgives Fuzz before he finishes |
| **Wisp** (563) | to go home | an imp kid who cried at the Wall so long they exhaled themselves out; their body has slept two years in Hell Pits apartment 13 | haunts your house into the shape of their room; helps defend it; pen pal of Pip by paper boat | you rebuild their room from memory of what they did to yours; they wake up; a housewarming in a body |
| **Haze** (709) | clean air | runs the vape counter, hates the smoke, waters the plants | Hattie's true name | the Fresh Air Lounge |
| **Signhere** (567) | a signature | has never had one withdrawn | the 900-page labour contract | everyone tears one page; his first contract nobody regrets |
| **Gatekeeper Grumbleguts** (`LB_GRUMBLEGUTS`) | a good meal and a friend (he would settle for a date) | kept the sealed gate ten years on the promise of a feast; only his stomach ever listened | §3 | the meal, the friend, and the Feast that decides whether he keeps them |

## 3. Gatekeeper Grumbleguts (user notes honoured in full)

Everything in `user_notes.md` is canon. `levels.c` boss text, the Breach 7 intro card and the level description are updated to match. Bestiary flavour: **"Keeper of the Sealed Gate. Also of a very large seat."**

**Look** (refs in `story/refs/`): a chunky pot-bellied cartoon devil (round gut, stubby horns, fangs, enormous grin), Baloo's loose belly-forward posture, gym-demon muscle on top of the gut (bat wings, a thick spiked tail), and the Red Guy's big smooth peach-shaped cartoon rear that is always, somehow, in frame. Hell dress code, cartoon-modest: spiked collar, leather chest harness, studded belt and cuffs, chains, the gate key on a chain, a riding crop as his gatekeeper's tool. Glossier, rubberier and sharper-lined than the plush citizens; he is the only character who neither drips (hell) nor wobbles (plush): **he bounces**, `TW_BOING`. Portrait, chibi, boss sprite, level-select preview and bestiary all show belly and rear.

**Voice.** Two speakers share his box: **GRUMBLEGUTS** (warm oily baritone) and **GURGLE**, his belly (voice sliders at the floor, 3-4 frames per syllable, bass babble). GURGLE lines are plain text, never `{d}`; the big gurgles get `{r}`. The belly is always hungry, always honest, and sides with the player about half the time.

**Personality.** Hell's biggest flirt: shameless with every *adult* in both towns, the player, Father Fuzz (to Fuzz's horror) and the Gate itself. Winks, pet names ("sugar", "padre", "new kid"), "come up and see my gate sometime", a little black book the size of a phone directory, a dating profile, exes in cutscenes. The rear show-offs are the flirting: strutting away camera-side, backing into frame, bending to pick things up, admiring himself in any reflective surface, wiggling it as a taunt, doors rear-first, sitting on things and occasionally people. **Every rear reveal gets a record scratch.**

**Hard rules.** 1) Innuendo and implication only; no acts, no genitals, no nudity beyond cartoon-smooth cheeks. 2) **Kids are off limits** for any flirting (Pip, Wisp, Bolt, Tantrum, Pokefork, Wobbles, anyone with `KID`, the edgy teens): with them he is a grumpy uncle ("Go home. Eat something. {d}Not THAT."). 3) Consensual and played for laughs; targets reject him, kindly; running gag: nobody is interested, except possibly the Gate. 4) The joke is his vanity and gluttony, never the target. 5) Never mean. The game loves him.

**Gag ladder** (timing bible §3.17: establish small, repeat soon, change the context, break the frame last):

| Ch | Appetite | Rear | Flirt |
|---|---|---|---|
| 2 (Ep 2.7 insert) | mid-meal on a mountain of Brimstone Bagels; "I only had a light snack—" as a forklift beeps in with the next course | squeezes through the gate he cracked, rear-first; stuck `[HOLD 13]`; `[BOING]` pop-through `{r}` | to the Gate: "Don't be like that. I'll be gentle." The Gate creaks |
| 3 (Ep 3.1 window; Ep 3.6 floor 5) | eating the Fun Fair's cotton-candy machine, machine included | backs into frame to block the Spire door; the camera cannot get round him | "Evening, sugar. Nice buns." NANA: "They're MUFFINS." G: "I'll allow it." |
| 4 (Ep 4.1 toll; Ep 4.5 boss) | the toll: a cupcake; "The belly says yes. The belly outranks me."; unbuckles the belt with a dramatic sigh; eats a thrown dish mid-fight and rates it | sucks the gut in `[HOLD 13]`, it springs out `{r}`; belly-flop; trampoline | "You're cute when you're scared."; "Padre. Come up and see my gate sometime." FUZZ: `{d}No.` |
| 4 (Ep 4.5 after; Ep 4.6 gate; Ep 4.7) | "Is there dessert?"; Nox: "You'd have to wear a shirt." "...A TIGHT shirt?" `{r}`; the buffet empty around him, bigger every cut `[3x]` | the crowd's eye-lines at the buffet; Honkers' napkin, flirted with | "Mayor. Love the hat." "It's my head." "I know." |
| credits / E1 | still eating at the roadside; waves a drumstick at the camera, then at the Gate | the belt pings off and hits the Mayor's hat `{r}` (the festival's button, after the Wall) | — |
| post-game (S186) | the Feast: a fullness meter on belly and rear | the visual thesis of the scene | the repaired Gate has a little heart painted on it. Nobody knows who. Everybody knows |

**Fight** (Breach 7 arena, story-entered in Ep 4.5, replayable from the Arena): the belly is the weak point (glows when hungry, every 10 s; hits elsewhere `*boing*` off with a wink); throwing a Brimstone Bites dish at the belly stuns him 8 s while he eats and rates it ("Six. Needs salt."); three dishes and no hits ends it peacefully (`USTORY_PACIFIST`); belly-flop shockwave (`[FREEZE 3]` in the air, `[SHK_ROAR]`, jump the ring; GURGLE gives a one-gurgle warning); burp portals in threes `[3x@15]`; the rear-wiggle taunt is a 3 f telegraph for the sit-slam; touching the belly trampolines you `[SQUASH 1.3/0.7]`. Defeated: he sits. "...Is there dessert?" GURGLE: "gurgle." He survives.

**Redemption:** he admits the beeping THING below ate his lunch and that nobody ever came up to see his gate; Nox offers the line-cook job (the tight shirt); chain T "Feed the Gate"; the Feast (S186) decides the rest. **Sim hook:** a resident shadow record so he can romance adults like any citizen (`ROMANCE_*`, cross-border crushes), be a friend, be `RES_DEAD`. The dating ticker goes wild; he is rejected kindly every time.

## 4. The dream house: the emotional anchor (summary; mechanics in `housing_final.md`)

Every chapter ends at home. `home_phase` is one saved byte.

| Phase | When | Look (GBA iso / PS1 2.5D) | Play | Signature beat | Stamp |
|---|---|---|---|---|---|
| **COSY** | Ch1 | the Pink Door Cottage (4,7): one room, then the Bedroom/loft from Ep 1.5 | decorate tutorial; the welcome basket; the live-placement housewarming (nine gifts, grade re-roll per placement) | the egg wobbles in the basket, `[R21]`, "beep." | COSY (6 pieces incl. bed and egg nest; the egg hatches only in a cosy room) |
| **HAUNTED** | Ch2 | things one cell off; a chair facing the wall; a tiny imp-sized bed you never placed; the wallpaper eye | "placed by: ???" items you cannot sell; night "Who's There" events; Hauntessa and Wobblewisp as the comic counterweight (chain V) | the chair tucked into your bed, breathing | LIT (3 lamps, candles count double; stops the whispers) |
| **BARRICADED** | Ch3 | welded plates, furniture stacked at the door, a toaster turret | House Defence (inside `SCENE_HOME`): wave 1 in Ep 3.2, the siege in Ep 3.7, optional Night Raids | the heart rug with a heart-shaped scorch | HELD (4 guests safe through the siege) |
| **EMPTY** | Interlude III | everything gone or knocked over; no music | decorate mode can only **remove**; the seven knocks each place one item with the DROP animation | §12 | — (items placed by visitors are tagged "gift from X" forever) |
| **OPEN** | Ch4 | the door propped open; the **Annex**: a Hellvile wing welded onto the south wall under a covered cut of sparks, the seam a visible ugly bead; a second home (Hell Pits flat) | hosting; Decorate-for (Wisp's room); the cursed-item rule | Wisp, Nox, Tallyworn and Mascotty without her head on your floor | HOME (8 seats, both styles, Annex furnished); ETERNAL (S210: 4 Hellvile pieces + 1 cursed) |
| **DREAM** | post-game | build on the Park Plot: plot, walls, rooms | Build mode; weekly visitor ratings (three reactions 15 f apart, escalating, then a topper) | the suitcase is unpacked | weekly A-F |

**Homes you own:** the Pink Door Cottage (Ch1); Hillvile Towers 7B (Ch2, S114; Tallyworn's fresh tally mark under the sill); a Hell Pits flat (Ep 4.4); the Park Plot (E2). **Homes you decorate for others:** Pip's shoebox bed in your house (S100), the daycare cots (S157), Steamy's bath house (S128), Wisp's real room (Ep 4.4), Mascotty's room (S136), the two-door clinic (S153), the gatehouse (S185), the shared flower bed (S126), the Florist's stall (S162), Tallyworn's second wall (S164).

**The cursed-item rule** (pentagram rug, Vapula's crown trophy, the Little Black Book): placing one plays `[INSERT 4: the room hellified + glitch]`, returns to the same shot with **one** persistent change (the lamp grew horns), and a visitor says "...it's fine." at `[R8]`. **Gag furniture:** the Big Chair (citizens slide off), the crown that lands crooked however placed, the Vapula plush that sighs when sat on, Kevin's bust (only the third eye blinks, every 83+ frames), the Plain Human figurine (the only decor that never animates; HomeMart, no mission), the Days Since Last Fire sign (resets when Rivet visits), the horned-poop plushie (E1). **Housing-doc mapping:** `housing_final.md` §10 numbers episodes by the heart bible; its "Ep 1.2/1.5/2.2/3.1/3.5/4.3/4.6" are this document's Ep 1.2 / 1.5 / 2.3 / 3.1-3.2 / 3.7 / 4.4 / 4.6 respectively. Its mechanics (rooms, catalog of 306, `HOME` chunk, renderers, Hold the Door inside `SCENE_HOME`) are adopted unchanged; this document only decides *when* and *why*.

## 5. Running gags and how each one mutates

| Gag | Ch1 | Ch2 | Ch3 | Ch4 | Post-credits |
|---|---|---|---|---|---|
| **"beep."** | tiny bubble on hatch; again on the egg's old basket | with horn nubs, in the haunted room, at nothing | mid-invasion between explosions `[MUTE 14]`; demons flinch; the fog beeps back, lower | from across the Wall, far away; then a hop-beep when it is finally fine | the poop grows horns. beep. |
| **"...it's fine."** | Nana when the oven smokes | Sprinklepop, possessed, sneezing purple glitter | painted on the church barricade | the Mayor about crossing the gate (it is not; he does it anyway) | the title logo on fire: "(it's fine.)" |
| **"Hee. Hee."** | the Mayor's nervous laugh (the sound of a man not looking at something) | the fountain giggles it a beat late | the leftover breath says it in his voice (scary); S198 names it as a wound | "...Hi, Fluffy." then the first real one; "Ha!" `[FREEZE ALL]` "...Ha?" at Hattie's set | a chuckle |
| **Mr. Huggins** | grave sign "Rest in pieces, Mr. Huggins the 1st" | the 2nd dies in practice | the 3rd-6th fall in the rule of three; the 6th torn on the conveyor (buckwheat) | the 9th survives and hugs Fuzz | — |
| **Nana bakes it into food** | the key in a muffin | a map in a pie | a relic in a bundt | a key in the Unity Loaf | — |
| **Mascotty the Starer** | "Ignore that one." | at your window, perfectly still | the only citizen not hiding; still facing the Wall | the head comes off | — |
| **"Days Since Last Fire"** | — | 7 → "-" → 0 | 0 and on fire ("a different fire") | 0 with flowers; a candle | the sign itself burns; reads 0 forever |
| **Crowned-cloud prop** | the Chapter 1 card's last sparkle is purple and sinks | "Employee of the Month" at Cloud Nine; the eyes follow you | it is Vapula | "Employee of the Month: Vex" | — |
| **"My eyes are just doing a rain."** | Pip | Steamy after the rite | the Mayor on the dark night | Tallyworn at the last tally | — |
| **Paper boats** | Wisp's crayon house in the surf; a second boat with a candle | — | the same hand on the pipes (S98 equivalent in chain J) | "That's MY house. You KEPT it?" | — |
| **Mailbox** | junk mail from "The Other Side" | all addressed to Hellvile people | the mailbox is a turret | delivers both ways (chain F) | — |
| **Grumbleguts is eating** | — | bagels; "light snack"; the forklift | the cotton-candy machine | the buffet, empty around him, bigger every cut | the Feast's fullness meter |
| **Record scratch on the rear** | — | stuck in the gate, `[BOING]` | backs into frame | gut in, gut out; the buffet eye-lines | S186: the last beat is a lone pair of cheeks drifting down |

## 6. Wrongness creep: the background horror catalogue (Ch1-2)

Cheap, engine-aware, rate-limited to one per 20-40 s of walking (timing bible §5.2), each **noticed once** with a tiny reaction and **never explained** until Ch3. NG+ subtitles them.

| # | Wrongness | When | Beat |
|---|---|---|---|
| W1 | The fountain giggles **R8 late** after you pass | Ch1 Ep 1.1 on | you stop, `[R5]`, it giggles again on time. "...Hee." |
| W2 | The "Population: 500" sign has a scratched-out "1000" under it | Ch1 | A: "Someone painted over a number." |
| W3 | Flowers lean toward the Wall | Ch1 nights | pick one, `[R13]`, it re-leans in your hand |
| W4 | Mascotty, perfectly still, in a crowd that bobs | Ch1 on | §5 |
| W5 | One lamp post faces the Wall | Ch1 | it faces you the next time |
| W6 | Thumping from the Wall at night: 3, then 1 | Ch1 nights | Thumpkin (chain J) |
| W7 | A citizen says "we" instead of "I" | Ch2 | "We love the weather. I mean... I." |
| W8 | Shadows lag 2 frames | Ch2 | a clue for Dr. Bloom |
| W9 | Everything tastes like grape | Ch2 | the existing topic, promoted to a clue |
| W10 | Your reflection in the bath-house water blinks late | Ch2 | `[R5]`, it waves. You didn't |
| W11 | The church bells toll backwards for one bar | Ch2 nights | Cantor Coo: "Did the bell just un-ring?" |
| W12 | The daycare drawings all have a crowned purple cloud | Ch2 | Miss Lulla: "They all drew the same friend." |
| W13 | The fountain turns lilac for one frame (`[INSERT 1]`) | Ch2 | only the pet reacts: "beep?" |
| W14 | The cafe jukebox plays OMINOUS for 2 notes | Ch2 | the owner hits it. "It does that." |
| W15 | Your house: the chair one cell off | Ch2 | §4 HAUNTED |
| **W16** | A joke grave, "Fluffy the cloud. Drifted.", in a child's hand; the fog pours **out of it** in Ch3 | Ch1 | if the Mayor is following he says nothing; `[EXPR SAD]` for exactly `[R8]`, then `[EXPR HAPPY]` |

## 7. The timing contract (what every beat sheet below obeys; lint-checked, §18.2)

From `comic_timing.md`, the heart bible's §0.4 and the horror bible's rules, made into one checklist a test can read:

1. **Per scene (cutscene or scripted overworld beat):** at least one camera move or cut; one expression change per line of dialogue; one SFX on every visual beat; one silence (`[MUTE]`/`[SILENCE]`/`[TAPESTOP]`); one button **or** an explicit `[NO BUTTON]`.
2. **Gaps sit on the ladder** 2, 3, 5, 8, 13, 21, 26 (±1). Instant reactions R2-R3, dawning R5-R8, deadpan R18-R26.
3. **Entrances take 4-6 frames and overshoot** (POP / SLAM / DROP tables); squash anchors at the feet; plush deforms up to 34%, hardware 18-22%, hell drips and does not wobble, Grumbleguts bounces.
4. **Inserts are 3-4 frames**, own the audio for exactly their length, and return to the same shot with one persistent change; the deadpan acknowledgement lands at R8.
5. **Rule of three:** constant interval (15-16 f), escalating magnitude in every channel at once, pitch stepping down, a topper one R13 later.
6. **Deadpan ration:** at most one `{d}` per conversation; deadpan text appears, never pops; the actor's idle clock is 0; GURGLE is plain text.
7. **Withholding ration:** `[NO BUTTON]` on at most half of the sincere beats in any chapter and never twice in a row in a chain; the final story beat (CUT_WALL_FALLS and the summit) is never undercut; the button goes after the credits.
8. **Silence budget:** 20-25% of each chapter's cutscene time is music-free (the trailer is 23%); the overworld always has music except the dark night; mutes keep the sequencer running and re-enter on the grid.
9. **Density:** cutscene 1 gag per 1.5-3 s; dialogue 1 punchline per 3-6 lines; overworld 1 ambient micro-gag per 20-40 s; housing 1 reaction per placement then cooldown; combat juice per hit, jokes only between waves; heartfelt 0 for 3-5 s minimum; horror the same with no button; 5-15 s of control with no scripted gag after every big beat.
10. **Buttons are callbacks:** every episode and every side mission has a "Later" that fires 1-10 minutes after turn-in: a line, a prop in a house, a sighting. At most four "Later"s in the whole side table are news-ticker lines; the rest are prop changes.
11. **Template ration:** the construction "Ten years I X because Y" appears exactly four times in the game (Tallyworn Ep 4.1, Haze S139, Dirgelet S179, Mopsworth S187). Everyone else shows it with a prop or a habit.
12. **Shake, flash, zoom are profiles, not random numbers**; reduced-motion settings scale amplitude and never timing.

**Notation used below** (compiles to `BeatEv` rows or dialogue codes, §15): `[R2]…[R26]` ladder gaps; `[HOLD n]` unskippable hold; `[WAIT A]` the press ends the setup; `[MUTE n]` `[DUCK g n]` `[SILENCE]` `[TAPESTOP]`; `[SCRATCH]` = tape-stop 5 f + `SHK_BRAKE` + `TW_BRAKE` camera + mute 14 f; `[FREEZE x]` `[THAW]` `[PANIC x]` `[STILL x]` `[TWITCH x]`; `[INSERT n: what]` `[ONE CHANGE: what]`; `[SMASH]` `[COVER n]` `[FLASH w10,5]` `[IRIS♥ out h]` `[IRIS○ out h]`; `[POP]` `[SLAM]` `[DROP]` `[JELLY]` `[BOING]` `[BRAKE]` `[WOBBLE]`; `[SQUASH a/b]`; `[SHK_x]`; `[!RIPPLE]`; `[ZOOM]`; `[3x@15]`; `[BUTTON +n: what]`; `[NO BUTTON]`; `[DAY n]`; `[NIGHT]`. In lines: `{.}` `{..}` `{...}` pauses R5/R8/R13; `{d}` deadpan; `{!}` sting; `{big}…{/}` pops whole; `{w}` wavy; `{r}` record scratch; `{punch}` unskippable hold follows; `[face X]` portrait expression. Lines are authored to the box: `/` is a line break, `//` a page break, at most 3 lines × 28 characters per page at 240×160 (the test measures it).

## 8. The shape

| Part | Episodes | Specials | Hours | Days |
|---|---|---|---|---|
| **P0** "The Egg Crossing" | crayon prologue | — | 2 min | — |
| **Ch1 Welcome to HaHaland** | 1.1-1.6 | Interlude I "Night Shift" | 2.5 | 1-4 |
| **Ch2 Something in the Vapor** | 2.1-2.7 | F1 "The Fountain Gift"; D1 "Teeth Like Muffins" | 3.0 | 5-9 |
| **Ch3 Hell Invades** | 3.1-3.7 | F3 "The Plumber's Order" (midpoint, 3.4); F2 "Mingle Night"; **Interlude III "The Empty Room"** | 3.5 | 10-12 (one long red night, then two) |
| **Ch4 Hellvile Park** | 4.1-4.7 | D2 "The Ninth Cloud"; F4 "Cupcakes and Bone Soup" | 3.5 | 13-16 |
| **Epilogue** "Happily Ever After (Mostly)" | E1-E4 | the credits parade; the post-credits button | 0.5 | 17 |
| **Post-game** | free play, NG+ "HaHaland Remembers", the Feast (S186) | — | 3-8 | 18+ |
| **Total** | **27 episodes + 8 specials** | | **~13 h main; +12-16 h side; +3-8 h post** | |

**How the length stays honest:** the four "talk to N random people" counters (C1_FRIENDS, C2_WEIRD, C4_PRISONERS, C4_FESTIVAL) are replaced by authored vignettes with their own verbs (Ep 1.4, 2.2, 4.2, 4.7); every episode has a purpose (an arc step, a reveal, a mechanic) and a button that is a callback minutes later; story days pass by sleeping at home; chains advance one part per chapter so the town keeps changing; 109 side missions use eighteen verbs, not two.

### 8.1 Hinges
* Hour ~0.7: the first "beep." (the smallest bubble in the game).
* Hour ~2.3: the first real scare (Ep 1.6).
* Hour ~4.7: **the Bottle** (Ep 2.5): "That was a PERSON." The midpoint-before-the-midpoint.
* Hour ~7: **the midpoint** (Ep 3.4): the plumbing; "I never asked where it went."
* Hour ~8.5: Fuzz's crisis (Ep 3.3 beach nest / Breach 4).
* Hour ~9.5: the pet eats the leftover breath and runs (Ep 3.7); the dark night (Interlude III): "I had a cloud once."
* Hour ~12.3: "...Hi, Fluffy." Then "Say It". Then the Wall.

### 8.2 Flags this story reads (choices with consequences)
`SF_ASKED_OTHER_SIDE` (Ep 1.1 → Ep 3.6 floor 4, Interlude III line, NG+), `SF_VISITED_GRAVE` (Ep 1.1 → Interlude III adds "You saw the stone."), `SF_VAPE_SAMPLE` (Ep 2.1 → Ep 3.6 floor 3, Ep 3.7 the fog's address, S135), `SF_WEAPON_BRUTAL` (Ep 2.6 → Rivet's lines, turret skin, Fuzz's "tsk.", `USTORY_PACIFIST`, S174 route), `SF_STEAMY_LOST`, `SF_HATTIE_LOST`, `SF_GRAMPY_LOST`, `SF_SMOG_LOST` (→ Ep 3.3, Int. III visitors, Ep 4.2-4.3, Ep 4.7 variants, chains H, I, N, G; Smog lost: the jar never rains, the Mayor still says the name, the hold is longer), `SF_SMOG_NEAR_MISS` (picked VAPULA first), `SF_TALLY_THAT_WAS_ME` (Ep 4.1 → chain Q, his stage line), `STRY.wisp_answer` (Ep 4.4: truth / truth / kind lie → epilogue line, S206), `USTORY_PACIFIST` (Ep 4.5 → Grumbleguts' greeting, the Feast's Branch A reward doubles), `STRY.sayit_line` (Ep 4.6 → the suitcase photo's caption in E2), `STRY.feast` (S186 → permanent). The player's three spoken lines (§8.4) are the only ones that draw the name pill.

### 8.3 Days and nights (the game finally uses time)
Night-only: Ep 1.6 (after 21:00), S199, S200, S204, S211, S132's fifth duck, the Lake candle, Breach 5 in story, Night Raids, Peek at a Dream, the Ch2 whispers. Day-only: Ep 1.5 ("come home at six"), S142's three photo times, S126's two waterings. Ch2's "a few days pass" is three real mornings. Ch3 is one long red night: the sun does not rise until the Spire falls; the clock still runs so the daycare bedtime bell (Ep 3.3's delayed button) lands at 20:00. Ch4 opens at dawn; Tallyworn's board and the Days Since sign count real days from there; the epilogue's "1" is real. The Feast (S186) is weekend-only post-game (`day % 7 == 6`) so the fullness meter has a visible calendar countdown; the running-gag spawner never fires during it.

### 8.4 The player's three lines (Appendix B of the heart bible, adopted)
The player speaks **three** times. Everything else that looks like a choice is a menu of actions or of what the pet does, drawn without a name pill. The pill on the dialogue box is drawn only for:
1. **Ep 3.7, the closed gate:** `[WAIT A]`, the pill `[POP]`s with `TW_POP_BUBBLE`, the text types at the *setup* speed, not the punch speed, because it is not a joke: `@P: "@E!"` `[R5]` far off, very small: "beep."
2. **Ep 4.4, Wisp's room:** Wisp, awake, asks "Did they talk to me? While I / was out?" Two truths and one kind lie (§11.4); the game remembers.
3. **Ep 4.6, Say It:** the player's own unsaid thing, from four.

---------------------------------------------------------------------------------------------------
## 9. PROLOGUE P0 "The Egg Crossing" (cutscene, ~2 min, before New Game)

Crayon style: the GBA draws it with `TEX_PASTEL_BRICK` scribble fills over a flat palette, the PS1 with flat-shaded quads and a paper grain. No portraits; the captions are in a child's hand. Music: SONG_FESTIVAL, slow, music-box arrangement. Purpose: plant the Mingle, the cloud on a string, Nana's boats and the egg in two minutes so Ep 4.6 can land.

| f | event |
|---|---|
| 0 | `[IRIS○ in]` on the bandstand. Two tables: a cupcake tower, a soup cauldron. Caption types: "Every year, the park had / a potluck." |
| 90 | Kids run across the bridge both ways `[!RIPPLE 0-3]`. A small cherub with a cloud on a string. "Everybody came." |
| 180 | Fireworks. The cloud tugs the string, looks up `[EXPR cloud=LOVE]`. `[R8]` The string slips. `[HOLD 13]` Nobody notices. |
| 250 | `[PAN]` south with the cloud as the fireworks fade. `[MUTE 29]`. "Not everybody went home." |
| 300 | `[SMASH]` SFX_THUD. The same bandstand, a Wall through it, eleven years ago. `[SHK_RUMBLE]`. Two tables, one each side. Two bowls on Nox's. One cupcake on Giggs'. Caption **appears** (deadpan): "Nobody talks about why." |
| 390 | `[COVER 22: fog]`. In it, a sheepkin (Nana) kneels at the river pushing a paper boat north to south. Inside: an egg. "Except Nana. Nana sends / eggs." `[R13]` A boat comes back the other way: a bag of burnt cookies. `[BUTTON +21: she eats one, [EXPR SICK], then [EXPR LOVE]]` |
| 480 | `[IRIS♥ out 5]` on the egg; in the hold the egg wobbles once, SFX_EGG_CRACK tiny. Title. |

---------------------------------------------------------------------------------------------------
## 10. CHAPTER 1: WELCOME TO HAHALAND (2.5 h, days 1-4)

**Purpose:** make the player love this town and this house before anything goes wrong. Horror only in the background (W1-W6, W16). Joke density: one punchline per 3-6 lines. **Music:** HAHALAND_DAY/NIGHT; scare-adjacent beats duck; no OMINOUS until Ep 1.6. Chapter card: pink polka, "Chapter 1" `[POP]`, title after 18 f; the last sparkle is purple and sinks `[BUTTON +25]`.

### Ep 1.1 "Population: 500" (20 min; existing C1_ARRIVE + C1_MAYOR)
**Purpose:** first laugh, first wrongness, the refrain, the grave.

| Beat | Picture | Timing |
|---|---|---|
| 1 | CUT_ARRIVAL rebuilt: the pink bus drives in at constant speed (no ease-out) over candy hills; the player in the back window | letters `[DROP TW_DROP_LET]` |
| 2 | Sign: "HaHaland! Population: 500 lovely citizens." | `[DROP TW_SIGN_DY]` `[SHK_TAP]` |
| 3 | The bus brakes `[SCRATCH]`; the player is thrown off the step `[DROP soft]` `[SQUASH 1.22/0.80]`; the suitcase lands 8 f later and bounces | `[BRAKE]` camera |
| 4 | The sign updates "...and now, @P!" (501). `[R8]` The "1" falls off, *tink* | `[BUTTON +13]`; a pink ticket flutters down onto your head at +22 |
| 5 | Fountain plaza. The Mayor flaps over; the crowd waves in a ripple of hearts. Mascotty (W4) does not | `[!RIPPLE]`; `[STILL Mascotty]` |
| 6 | The Mayor points at everything except the Wall; the camera creeps toward it anyway at 0.04 px/f; he steps in front of the lens | pan |
| 7 | The refrain; the fountain giggles R8 late (W1) | `[DUCK .5 13]` |
| 8 | Nana has the key. On the way, optional: the graveyard shortcut. "Fluffy the cloud. Drifted." in a child's hand (W16). If the Mayor is following: `[EXPR SAD]` for exactly `[R8]`, then `[EXPR HAPPY]`. Sets `SF_VISITED_GRAVE` | `[DEADPAN caption]` |

> MAYOR: [HAPPY] Welcome, welcome! Mayor / Gigglesworth. Friends call me / Giggs. Enemies don't exist!
> MAYOR: Ooh, a new face! We get so / few. We had a whole fence of / them once.
> MAYOR: [SHOCK] {..}Not a fence OF faces. / Faces AT a fence. Hee. Hee.
> MAYOR: Try the Brimstone Bites! / Named for a joke. It's / ironic! Hee. Hee.
> MAYOR: Hellvile Park has two sides. / This one, and... `[HOLD 13]` `[FREEZE Mayor]` / ...the weather side.
> MAYOR: [NEUTRAL] {d}We don't talk about the / other side.{punch} `[HOLD 13]` (the fountain giggles, late) `[THAW]`
> MAYOR: [HAPPY] Hee. Hee! Your key is with / Nana! Follow the smell of / nutmeg!

**Player menu** (no pill): "Nice to meet you!" → `[face LOVE]` "Manners! We'll keep you." / "What's on the other side?" → `[FREEZE ALL]` `[TAPESTOP]` `[!RIPPLE]` every citizen looks at you `[HOLD 21]` `[THAW]`, music back on the downbeat; "Hee. Hee." (sets `SF_ASKED_OTHER_SIDE`) / "...Why is that mascot staring?" → "Ignore that one." `[BUTTON +21: Mascotty waves, very slowly]`.

### Ep 1.2 "The Key in the Muffin" (25 min; C1_KEY + C1_HOME)
**Purpose:** Nana, the egg, the first room; the decorate tutorial.
* Nana's bakery: every tray has two of everything (a seed). The key is in a muffin: bite the wrong ones `[3x@15]` (raisin, raisin, "that's a button"), the fourth `[POP TW_POP_EYE]` `[FLASH w6]`; an `[INSERT 3: x-ray of the muffin, the key has tiny horns]` `[R2]` SFX_TIK.
* The egg in a tea towel; she **hesitates** `[R13]` before letting go.
* The cottage: `[IRIS♥ in]` r = 6 + 250·smooth², the door `[JELLY]`. Empty but a crate and a window. Tutorial: place the bed `[DROP 24 px] [SQUASH 1.22/0.78]` THUD+BOING, neighbours `[JELLY 0.4 +2 f]`; rotate the lamp (squash-flip); the "nope" shake on the window. Welcome basket: bed, heart rug, lamp, crate, egg basket. First rating: the counter rolls `v = N·(a/22)^2.2`, the stamp `[SLAM TW_SLAM_STAMP]` "COSY: B+", three imaginary visitor bubbles 15 f apart ("♥", "♥♥", "...is that a crate?") `[BUTTON +13: the crate slides 1 px in shame]`.
* Night: through the window, a far purple glow at the Wall, 1 px, pulsing `[R21]` apart. Sleep `[DAY 2]`. `[NO BUTTON]`

> NANA: You're the new one! You look / hungry. Everyone looks hungry / to me.
> NANA: The key's in a muffin. Don't / ask which. I forgot. I always / forget.
> NANA: [SAD] {..}And this. An egg. It's / been waiting for someone.
> NANA: [HAPPY] The last one hatched into / a little demon! Ha! Kidding.{.} `[R8]` [NEUTRAL] Mostly.
> NANA (doorway, later): You put a lamp by the egg. / That's exactly where my / mother put hers.

### Ep 1.3 "Hatch Day" (20 min; C1_HATCH to C1_BUNNY_RETURN)
**Purpose:** the pet, the "beep." runner, Pip, and the **first paper boat**.
* The egg wobbles at a period of 6 f, then 3 f, and hatches **on the downbeat** (SCENE_PET arg 0). `[FLASH w10,5]` `[POP TW_POP_EYE]` two eyes.
* Back in the house the pet hops to the lamp and sits in the light. `[HOLD 21]` nothing. `[MUTE 29]`. Then `[POP TW_POP_BUBBLE]` **"beep."**, the smallest bubble in the game. `[DUCK .55 6]`. `[R8]` caption appears: `{d}...it's fine.` (the first use). Name it (`@E`). `[INSERT 1: the LCD shows a pentagram]`, one frame, the seed.
* Pip at the fountain `[EMOTE SWEAT]` ×3 at `[R3]`: "My eyes are just doing a rain." The beach: Mr. Flops in the surf `[DROP]` 2 px with each wave; he is lying face-down **staring at the Wall**; you turn him over; the stitched smile; `[SLAM TW_SLAM_STICKER]` "FOUND!"; the pet beeps at the sea `[R3]` later. A second shape in the surf: **a paper boat**. Inside, a crayon drawing: a house, a candle in the window, a small imp and a bigger imp, and "ARE YOU THERE" in backwards letters. `[HOLD 18]` `[MUTE 18]`. The pet sits on it `[EXPR LOVE]`.
* Pip's return: the skull sticker; "make friends with EVERYONE"; Pip keeps the drawing "safe like Mr. Flops" `[NO BUTTON]`.

> PIP: [SAD] I lost Mr. Flops. He's a / bunny. I'm a bunny. We match.
> PIP: My eyes are just doing a / rain. It's fine.
> PIP: [HAPPY] MR. FLOPS! You're damp! / Were you scared? `[R8]` [NEUTRAL] {d}...He says he saw the Wall / breathe.
> PIP (boat): That's not my drawing. / That's somebody's HOUSE. `[R13]` I'll keep it safe. Like / Mr. Flops.

### Ep 1.4 "Three Neighbours" (30 min; replaces C1_FRIENDS)
**Purpose:** meet, by name and verb, the three people who will be possessed later, so it hurts. Any order.
1. **Steamy's bath house:** a fill-the-tubs mini-task; every tub has a duck; one duck faces the Wall. "That's Duckworth. That's Quackers. That's...{.} [SHOCK] I don't know that one." `[R13]` the unknown duck squeaks by itself `[BUTTON +8: Steamy squeaks a duck back, nervously]`.
2. **Hattie Haha's open mic:** the **comic-timing minigame** (press A on the punchline beat; a perfect press = snort-laugh ripple; late = a cricket). Her closer: "So I said, 'Mayor, what's on the other side?' He said, 'Hee. Hee.'" `[R8]` big laugh `[BUTTON: Mascotty in the back row doesn't laugh]`.
3. **Grampy Bean's tomatoes:** pick 5; he calls you a different name each time ("Sunny... Sonny... Bunny... Toaster"). "These seeds came over the fence years back. Never asked who threw 'em." `[SLEEPY]` "...where was I?"
* Also met with two-beat gags: Kevin (a creator preset; "Yes, three eyes." `[R5]` only the third blinks), Bonejangles (plays a chord when hugged), Miss Lulla's kids ("BEEP" at the pet `[!RIPPLE]`). A text from Nana: "You've made friends! Come get a cupcake to share."

### Ep 1.5 "Housewarming" (25 min; absorbs C1_CUPCAKE)
**Purpose:** the house becomes yours; the live-placement party; the first clear scare at the window.
* Nana's cupcake to Miss Lulla (day); "the fog giggles at night"; the crowned drawings (W12). "Come home at six."
* **The party (housing, live):** nine citizens arrive `[DROP]` at 8 f intervals, each with a gift (furniture, tagged "gift from X"): Nana a rug, Pip a drawing (wall), Fuzz a cushion ("cuddle-proofed"), Kevin a bust of himself, Bonejangles a xylophone end-table, Miss Lulla a nightlight, the Mayor a framed photo of himself, Dr. Bloom a thermometer ("decorative"), Rivet (unseen) a welded coat hook left on the step with a note "don't make it weird". Place them live: each placement gets a crowd reaction `[!RIPPLE]` and a grade re-roll; three reactions 15 f apart, escalating; topper: the pet climbs onto the bust. The loft/Bedroom unlocks (GBA stacks a second diamond layer; PS1 tilts up).
* The toast: MAYOR "To @P! To new friends! / To fluffy—" `[FREEZE Mayor 13]` `[MUTE 13]` "—cupcakes. Fluffy cupcakes." `[THAW]`. Nana looks at him for exactly `[R8]`.
* Everyone waves goodbye in a ripple. The door closes. `[R21]`. `[STILL Mascotty]` at the window. `[MUTE 41]`. Caption, small, cool outline: `{d}Ignore that one.` `[R21]` she waves, 8 f per phase. `[IRIS♥ out 5 on the window]`.

### Ep 1.6 "The Giggling Fog" (30 min; C1_WALL; the first real scare)
* `[NIGHT]` Woken by thumping: 3, then 1 (W6). The purple tint ramps 0→4 by x-cell; music `[DUCK .4]`; crickets stop one at a time `[R5]` each.
* Choice (menu): "Peek" / "Back away". **CUT_FOG as a beat script:**

| f | event |
|---|---|
| 0 | the fog wall drifts; OMINOUS low |
| 40 | two red eyes `[POP TW_POP_EYE]` *tik* |
| 48, 53, 56 | two more each (`[R8]`, `[R5]`, `[R3]`: an accelerating three plus one) |
| 56 | `[MUTE 40]` all eyes `[STILL]` |
| 76 | caption types slowly `{w}come and PLAY with us`; the second "us" is a child's voice |
| 96 | `[INSERT 4: a Hillvile kids' tent, eleven years ago; a cloud-shaped lady with glowing eyes; behind her, a crowned silhouette with a string trailing from it]` (the first flash of F2) |
| 100 | `[SMASH]` the Mayor's face fills the frame, panting `[SHK_POW]` |

> MAYOR: [SHOCK] {big}NOPE.{/} Nope! Home! It's / bedtime! It's always / bedtime!
> MAYOR: It's just...{..} weather. / Hellvile weather.
> MAYOR (walking you home, quieter): [SAD] {d}Please don't go near there / again. (he hums the Mingle song and stops when he notices)

* Home: the pet at the window, staring at the Wall. Turn it round. `[R13]` It is facing the Wall again. "beep." On the crate: a **second paper boat** pushed under the door, with a note in Pip's hand. Inside: a candle, unlit, and the same house with a light in the window. Place the candle (furniture; it can be lit). `[IRIS♥ out 10 on the window]`; in the hold the far purple glow **blinks once in reply**. `[DAY 4 → Chapter 2]`

> PIP's note: ANOTHER ONE CAME. IT SAYS / HELLO. I DIDN'T OPEN IT. / (I opened it.)

### Interlude I "Night Shift" (playable, 2 min; the pet's first full insert)
Feed @E a cupcake in the pet scene: eats `[SQUASH]`, strain (8 f, 1 px jitter), plop; `[INSERT 4: the LCD fills the screen, pentagram 2 f, demon face 2 f; music muted exactly 4 f]`; back `[ONE CHANGE: two horn nubs]` `[R2]` "beep." `[R8]` `{d}...it's fine.` Sets `SF_PET_NUBS`. Nana, tomorrow: "Ah. {..} He's teething." (She knows.)

---------------------------------------------------------------------------------------------------
## 11. CHAPTER 2: SOMETHING IN THE VAPOR (3 h, days 5-9)

**Purpose:** the wrongness moves from the background into people, then into your house. Slow scares, louder jokes (everyone overcompensates). The haunted house is the spine; the Bottle is the floor dropping out. Chapter card: vape puffs; one puff has two red dots for `[INSERT 3]`.

### Ep 2.1 "Grand Opening" (25 min; C2_VAPOR)
* "A few days pass" is played: three mornings `[SMASH]`×3 at 20 f each; each morning the pet has more horn and the glow is closer; the third morning a shop has appeared by the Wall with a ribbon (Ribbonetta cuts it and sneezes purple) and a crowned-cloud "Employee of the Month" frame whose eyes follow you.
* **Sprinklepop** first in line: "Grape? I LOVE grape. I love everything!"
* Vex's pitch is a performed routine: he `[HOP]`s on every exclamation; the smile is drawn 2 px too wide and snaps back `[R3]` after each line; he leans in too close, the smile `[STILL]` R8 too long. Haze at the counter waters a plant and does not look up.
* **The choice (menu):** decline / one puff / "Where are you from?" (new). Decline: Vex `[STILL]` `[R21]` "...Suit yourself."; Haze gives a tiny thumbs-up. Puff: the s07 inhale (10 f stretch, 5 f hold), FWOOMP `[SHK_FWOOMP]` `[COVER 22]`, two red eyes in the fog at f 10, the cloud clears to show you fine, `{d}Everything is fine.` `[BUTTON +24: a balloon pops somewhere]`; sets `SF_VAPE_SAMPLE` (you hear the fountain's late giggle a little louder; Vapula knows your flavour; the fog addresses you "by flavour" in Ep 3.7). "Where are you from?": `[FREEZE Vex 13]` `[MUTE 13]` "...WAY out of town." `[THAW]` `[BUTTON +22: the crowned cloud on the board blinks]`.

> VEX: WELCOME to Cloud Nine! Where / every breath is a hug from / the inside!
> VEX: Grape! Cherry! Sunset / Serenity! Mystery Mauve! `[R8]` {d}...Mystery Mauve is grape.
> VEX: First puff's free. Second / puff's free. All the puffs are / free. That's the business model.
> HAZE (not looking up): {d}The plants hate it too.

### Ep 2.2 "Grape" (35 min; replaces C2_WEIRD; a playable investigation)
* Dr. Bloom's clinic: "Five patients. All say everything tastes like grape. I'm a doctor, so: that's weird." A **Clues board** opens in the quest log. **Find 3 of 5:** (1) grape tongue: Sandwichy's blind taste test (cooking minigame; every flavour reads GRAPE); (2) "we": catch a customer mid-slip (W7; press A *during* the slip, a timing check; miss and they repeat it smoother); (3) lagging shadows: photograph one (W8); (4) backward bells at the church at night (W11); (5) the crowned drawings at the daycare (W12); plus Hattie's set where the punchlines arrive R8 late and the laugh track is the fog's giggle ("Thank you. You're too kind. Whoever you are.").
* Turn in: Bloom takes your pulse with a cold hand `[SHOCK]` "Sorry. Ghoul." `[R8]` "...I mean: *cool* hands. It's a doctor thing."
> DR. BLOOM: Grape. "We." Late shadows. / It's not a bug. It's in the / air.
> DR. BLOOM: [SAD] I've seen this before.{..} / {d}Somewhere else.{punch}

### Ep 2.3 "Things Move When You're Not Looking" (30 min; new; the house becomes HAUNTED)
**Purpose:** the scariest episode in Ch1-2, almost entirely funny. Housing mechanics: `housing_final.md` §10.2.
* `[NIGHT]` The chair is one cell off; decorate mode lists it "placed by: ???".
* **The rule-of-three haunting**, each step playable in the room: 1) put it back, turn the camera to the bed (GBA rotates the iso view 90°, PS1 orbits), turn back: the chair is where it was, *creak*. 2) Again: it is now **facing the wall, like a kid in time-out**, *creak creak*. 3) Again: `[MUTE]`; turn back: **the chair is in your bed, tucked under the blanket**. `[HOLD 21]` `[STILL ALL]`. From the pet bed: "beep." `[BUTTON +8: the blanket rises and falls once, breathing]`.
* Topper `[R13]` later: a tiny **imp-sized bed** you never placed, in the corner, and taped above it a drawing of a house with two windows: **the same hand as the paper boats**.
* Night 2 (the wallpaper): light the lamp: `[INSERT 4: every heart in the wallpaper is an eye]` `[R2]` SFX_TIK; back: **one heart is still an eye**, blinking every 83 f (`HOME.haunt[2]`). The pet grows its first real horn in its sleep `[R13]`, "beep." appears. LIT unlocks (3 lamps; the eye stops blinking at LIT but stays an eye forever).
* Night 3: two pieces move toward the cellar hatch; Vex's pentagram rug is under the bed though you did not put it there. Menu: "Sleep in the living room" / "Sleep in your bed anyway" (Brave sticker; you wake with the chair cuddling you).
> (caption) {d}Something in your house likes / your house.
> (next morning) {d}...It moved the rug too. / It looks better, honestly.
* Wisp's haunting begins (invisible): each night one piece moves toward the layout of Wisp's real room; decorate mode keeps a "Rearranged by ???" list that Ep 4.4 reads.

### Ep 2.4 "Steamy Isn't Home" (30 min; C2_SCREAM + C2_FUZZ)
* The plaza, midday, music up. Steamy hands out rubber ducks. `[SCRATCH]` on the third duck (it is Guest, S127): `[FREEZE ALL]` `[FLASH w7]` one-frame shutter. Steamy's idle clock is the only one running, ears `[TWITCH 1.7 rad/f]`. Her head turns 180° over 6 f with a creak. `[MUTE 60]`. `[!RIPPLE]` at +14.
> STEAMY: {d}Steamy isn't home right now. `[R13]` We moved in. `[R8]` [POSSESSED] Leave a message after the / scream.
> `[R21]` the scream is **a rubber-duck squeak**, enormous `[SHK_ROAR]`; everyone flees on the accelerating curve, sweat drops every 3rd frame; `[BUTTON +8: Mascotty doesn't flee]`
> MAYOR (under a bench): [SHOCK] Get Father Fuzz! Get Father / Fuzz! I'll... get behind you!
* Sprinklepop is possessed too, smiling, sneezing **purple** glitter: "I'm fine! {.}{d}We're fine."
* **Father Fuzz**, plush priest of St. Cuddles, "stuffed with love and a little bit of buckwheat". Mr. Huggins the 2nd on the altar (the 1st has a grave sign). **The practice is a scripted rule of three** that teaches the three parts:

| try | at | part | result |
|---|---|---|---|
| 1 | 0 | prayer rhythm | an arm pops off `[SLAM]` |
| 2 | +15 | holy water | the other arm `[SLAM]` |
| 3 | +16 | true-name pick | the head rolls off, bounces 3 times (*tink tink tink*), rests **facing the Wall** `[SHK_STAMP3]` `[FLASH r8,5,2]` |
| topper | +13 | — | FUZZ: {d}Rest in pieces, Mr. Huggins / the 2nd. |

> FUZZ: Nothing is unhuggable, @P. / Nothing. Some things just / need holy water first.
> FUZZ: A demon is a frightened / thing in a loud coat. You / take the coat off.
> FUZZ: The name phase. Say what / it IS, not what it's / wearing.

### Ep 2.5 "The Power of Cuddles" (30 min; C2_EXORCISE) — and **"The Bottle"**
**Purpose:** the first true exorcism; the first true name; the midpoint-before-the-midpoint.
* **Sprinklepop, scripted to fail** (the trailer's s11 in the game): "THE POWER OF CUDDLES COMPELS YOU!" types at 1.5 chars/f with "!" flashing every 3 f; the answer, tiny and instant: `{d}ow.` **FAIL x1** (spray bottle) at 92, **x2** (a real hug) at +15, **x3** (the whole bucket) at +16 with `[SHK_STAMP3]`, red flash, a 1.2x counter and three kazoo notes stepping down; topper +13: candles out one per frame, **SOUL LOST.** `[SLAM]`. Her soul pops out, holds a map upside down, waves, and wanders toward the Wall. Her body falls asleep smiling at the clinic. FUZZ: [SAD] "We'll find her. Souls don't go / far. {..}{d}They go exactly as far / as they're scared to."
* **Steamy, for real** (odds shown). Clues from her taunts: "We count. We count every day." "Three thousand, six hundred and..." Props in the name phase: a chalk stub, a tally wall, a bellows, a scrap reading "3,652". Names offered: TALLYWORN, DIRGELET, MOPSWORTH, HAZE. Answer: **TALLYWORN**.
  * Saved: STEAMY: [SICK] "...Why do I taste grape?" [SAD] "There was someone in my head. / Counting. He was so tired." FUZZ: "The name you shouted, @P... / {..}That's a real person. Over / there." `[R21]`
  * Lost: Steamy's ghost floats up holding a duck: "Oh. {..} Oh, that's... cold." FUZZ: "When you're ready, we can lay / her to rest." (Graveyard beats in Ep 3.3 and chain H.) Her ghost keeps the bath house.
* **CUT "The Bottle"** (new, 30 s, both branches): Fuzz, Steamy or her ghost, you. Fuzz holds the empty vape bottle up to the light. Inside, tiny: a chalk stub, a tally mark. `[HOLD 21]` `[MUTE 41]`. FUZZ: "@P. {..}That was a PERSON." The pet beeps once, very small. `[IRIS○ out 5 on the bottle]`. **`[NO BUTTON]`.** (Every exorcism afterwards carries this.)

### Flashback F1 "The Fountain Gift" (40 s, sepia palette lerp; triggered by the first weld in Ep 2.6)
Little Rivet, six, holds a torch for Anvil, who welds a fountain shaped like two laughing faces facing each other. "When someone laughs here, it bubbles up over THERE." A bow on it. "For the Mingle." `[SMASH]` to now; Rivet's hand shakes. `{d}"...Nothing. Weld fumes."`

### Ep 2.6 "Don't Make It Weird" (30 min; C2_RIVET)
* Rivet under a car. "Don't make it weird." ×3 at `[R13]`, `[R8]`, `[R3]` as she gets more flustered when thanked.
* The weapon menu: Bubble Wand (cute) / Pump Shotgun (brutal) → `SF_WEAPON_BRUTAL`, read later (§8.2). A third option "Can you fix a lock?" is refused `[FREEZE Rivet 13]`.
* The **"Days Since Last Fire" sign** reads 7. Her tail touches a spark: 7, then "-" for 2 f with jitter, then **0** in red `[SLAM]`, pulsing every 4 f. Bucket. Mask up `[R8]`. Thumbs up (the trailer's s09).
* Her workshop quip now includes: "Back in Hellvile I welded / gates. One gate." `[R13]` "I was ten." (It is the one in the Wall.)
* "Lord Vapula's cooking something big. And your Vex guy slipped into his cellar with a glowing crate." She gives you a toaster turret "for the house. For nothing."

### Dream D1 "Teeth Like Muffins" (90 s; night, forced Peek at a Dream)
You are at the Mingle (you have never been; it looks wrong). Everyone has Nana's muffins for teeth. Nana: "Eat up!" `[3x@15]`: a bite, a bite, a bite that is a **key**. The key opens the Wall; behind it is only **your own house**, with Wisp's little bed in the corner. A silhouette whispers `{w}is this your room? it's nice. can i stay?` `[IRIS○ out 10 on the bed]`. Wake: the chair is tucked into your bed again; you are both in it. `[BUTTON: beep.]` (`SF_VAPE_SAMPLE` variant: the muffins are grape and the key is purple.)

### Ep 2.7 "The Cellar" (40 min; C2_CELLAR; Hell Breach 1)
* The hatch under Cloud Nine; Haze looks up for the first time: "It's unlocked. It was always unlocked." **Breach 1** with Vex on the PA between waves (HUD captions, rate-limited): "Customer satisfaction is our TOP priority!" `[R26]` "...why are you shooting the merch."
* The note: "Too late, Hillvile. The big cloud is already rising. Kisses! - V"; smaller underneath: "(get your pet indoors)".
* **The eruption in three parts:**
  1. **Overworld:** daylight; birds. `[R21]` One bird stops. `[R8]` All the birds stop `[!RIPPLE]` on the beat. `[TAPESTOP]` `[SILENCE 64]`. The camera creeps toward the Wall at 0.04 px/f.
  2. **Insert "The Gate Cracks"** (25 s; Grumbleguts' first scene): Vapula looms over the sealed gate; below, Grumbleguts mid-meal on a mountain of Brimstone Bagels, back to camera, rear filling lower-left.

| f | event |
|---|---|
| 0 | VAPULA: Gatekeeper. {..} The gate. |
| 20 | GRUMBLEGUTS (mouth full): I only had a light snack— `[R5]` a forklift beeps in with the next course `[DROP]` |
| 45 | GURGLE: gurgle. GRUMBLEGUTS: He says hello. |
| 60 | he stands, sucks his gut in `[HOLD 13]`; it springs out `[JELLY]` `{r}` |
| 80 | unbuckles the belt with a dramatic sigh `[R21]`. To the Gate: Don't be like that. I'll be / gentle. `[R8]` The Gate creaks |
| 110 | belly-flop from the stairs: `[FREEZE 3]` in the air, `[SHK_ROAR]` `[FLASH r10,7,4]`; the gate **cracks** |
| 125 | he squeezes through rear-first; stuck `[HOLD 13]`; `[BOING]` pop-through `[SQUASH 1.4/0.7]` `{r}` |
| 140 | GRUMBLEGUTS: That was the appetiser. VAPULA: {d}The feast is on the OTHER side. `[ONE CHANGE: the crack stays for the rest of the game]` |

  3. **CUT_ERUPTION** re-timed: sky bands pink→purple→blood; the cloud swells, grins, pours through the crack `[SHK_ROAR]` `[FLASH r10,7,4]`; the crown lands crooked; "THE DEMONS OF HELL ARE HERE." `[SLAM]` `[BUTTON: the Employee-of-the-Month frame falls off the Cloud Nine wall, tink]`.
* Ch2 ends at home: you sprint in, the door slams `[SHK_POW]`, the pet hides under the bed, red light in the window. "beep."

---------------------------------------------------------------------------------------------------
## 12. CHAPTER 3: HELL INVADES (3.5 h, days 10-12: one long red night, then two more)

**Purpose:** full DOOM. The cute town is a battlefield, the jokes get faster because panic is funny, then the midpoint twist and the dark night take the floor out. **Music:** INVASION with SILENCE windows; HAHALAND_DAY only inside the church, low, for the kids. **Map:** HaHaland-Invaded; red pulsing tint, embers, ash. Chapter card: embers, title jitter, drawn on the back of a contract page ("1 of 900" in the corner).

### Ep 3.1 "The Sky Ripped" (30 min; C3_ERUPTION + start of C3_NESTS)
* You wake on your floor; the door is buzzing. **The run to the church**: a playable chase with scripted gags at the overworld rate: a demon pack chases Squeaker the clown; he honks; the demons flee the honk. Chief Spark puts out a fire with a hydrant that is itself a demon (quest #21's brute, seen early). **Hattie and Grampy** in the road, back to back, perfectly still `[STILL] [TWITCH]`; their heads turn to you in sync; `[MUTE 21]`; HATTIE: `{d}Knock knock.` `[R26]` You run. (Possessions three and four, staged.)
* **The church shelter:** a pastel island in the red; the kids watch a shadow-puppet show; Lulla's kids chant "it's fine" as a game. **Fuzz at the door with a hymnal, out of breath. He just hit a cloud spider with it.** `[HOLD 18]`. He does not look at the hymnal. He puts it down very carefully. (Sets up Ep 3.3.)
> MAYOR: [SHOCK] The sky RIPPED. Like a / cupcake wrapper. A big red / cupcake.
> MAYOR: Everyone's here. Nearly / everyone. [SAD] {..}Steamy's duck is / here.
> FUZZ: Four nests. One spire. And / our friends, wearing bad / tenants.
> MAYOR: [NEUTRAL] And it's fine! {..}{d}It's fine.{punch} `[R13]` (painted on the barricade: "IT'S FINE")
* `[SCRATCH]` from outside: **Grumbleguts walks past the window backwards, rear first, eating**; `[SCRATCH]` again when he bends for a dropped bagel. Through the window, winking at Nana: "Evening, sugar. Nice buns." NANA: "They're MUFFINS." G: "I'll allow it." The kids shout BEEP at him; grumpy uncle: "Go home. Eat something. {d}Not THAT."
* Rivet arrives with three Welded Plates: the house's Shelter tab opens (plates snap to window slots, the toaster turret has a slot, the Score panel shows Barricade); one **daylight tutorial wave** of 3 imps at the door (door HP = Barricade; A on the door repairs +10 with a 2 f squash; the turret fires every 12 f and is re-wound every 20 shots). `home_phase = BARRICADED`.

### Ep 3.2 "Board Up the Windows" (30 min; new: house defence 1; Wisp)
* `[NIGHT]` Rivet welds boards (a welding verb) and asks for furniture to brace the door: "Pick a dresser. Not the cute one. {..}Fine. The cute one."
* **Wave 1** inside `SCENE_HOME` (`housing_final.md` §10.3): imps through 2 windows and the chimney; furniture is cover and HP. Mid-wave a dresser slides by itself in front of a breached window `[DROP]` THUD. A tiny voice: `{w}not in MY room.`
* Wisp manifests: a translucent imp kid, half-formed, ears flickering.
> WISP: [SHOCK] you can SEE me??
> WISP: sorry about the chair. i was / trying to make it like home.
> WISP: [SAD] my home's over the Wall. i / cried there so long i... fell / out of myself. `[R21]` `[NO BUTTON]`
* From now on Wisp follows you inside the house. Vex is in your closet (S138). **Night Raids** (optional, Ch3 nights): sleeping at home has a 50% chance of one wave; three clears grant "Homeowner" and the turret skin.

### Ep 3.3 "Four Nests" (80 min; the nests are set pieces tied to Breach 3-5; any order)

| Nest | Set piece | Breach / boss | Character beat |
|---|---|---|---|
| **Plaza (bakery)** | Nana defends the sourdough starter with a rolling pin; demons stuck in the dough | **Breach 3, Sugar Rush Sewers**, Gumdrop Gutterking ("Everything down here is CANDY. Including the floor. Don't lick the floor.") | Nana bakes you a **map into a pie**: "Eat it first, read it second." |
| **Beach** | Imps kidnap Mr. Flops; Pip escorts with a courage meter that recharges near you | **Breach 4, Plushie Factory**, Foreman Fluffnuts turning plushies into demons on a conveyor | **Fuzz Goes Dark** (below). Pip catches Mr. Flops mid-air `[FREEZE]` `[HOLD 13]`: "I was brave!" `[R8]` "...I need a nap." |
| **Park (Fun Fair)** | the rides are possessed: the ferris wheel spins backwards, the teacups are full of eyes | **Whack-an-Imp** in story ("Do NOT bonk Father Fuzz." You bonk Father Fuzz. He is standing next to the machine) | Fuzz, rubbing his head: "Seven hundred years of / training and I lose to a / mallet." (if before the beach: the last time he is light) |
| **Graveyard** | the joke graves rise as cute ghosts (Mr. Wiggles, Grandpa Puff's moustache, the Great Cake of '89); Mr. Huggins the 1st rises in pieces; **the fog pours OUT of "Fluffy the cloud"'s grave**, the nest's centre | **Breach 5, Graveyard Shift**, the Weeping Widow ("She Lost Her Plushie in 1887") | **The Mayor is there, alone, at the grave, in the middle of the nest, not hiding. He has a shovel. He does not use it.** If Steamy was lost her ghost keeps the dead warm in a bathtub ("It's nice, actually. Cold out."); if saved she fights beside you with a hose |

> MAYOR (graveyard): I'm not hiding. I'm... / visiting. `[R21]` Clear them off him, / would you.
> PET (first time demons flinch): beep. `[R3]` the nearest imp `[EXPR SHOCK]` backs off. The fog, from the grave, beeps back, lower. `[MUTE 14]`

**Fuzz Goes Dark** (inside the beach nest / Breach 4; heart Ep 3.4 beats 2-5 grafted):
1. Fuzz's church door is open and the hymnal is gone; a trail of stuffing leads to the Factory. He went alone for Mr. Huggins' "spare" (there is no spare).
2. Breach 4 with Fuzz as an escort who refuses to fight and then does. Fluffnuts: "We make 'em. We don't mend 'em." `[SLAM]`. **He tears Mr. Huggins the 6th in half on the conveyor:** `[INSERT 4: real stuffing]`, back to cute: it's buckwheat. `[MUTE 41]`.
3. After the boss, Fuzz sits on the conveyor with half a plush in each hand. **THE SPEECH** `[NO BUTTON]`:
> FUZZ: I hit a demon with a hymnal, / @P. It worked. The hug / didn't. `[HOLD 26]` [SAD] What am I FOR?
> FUZZ: Buckwheat. He was always / buckwheat. I KNEW that.
> FUZZ: Faith is a stuffing. You / don't notice it until / something tears.
4. He hands you the bell and the holy water: "You do them. I'll... I'll / watch the kids." He walks home slowly (the flee curve reversed).
5. **The button lands ten minutes later** (frame-exact to the clock: 20:00): the church bell rings at the wrong time. He is ringing it for the kids' bedtime. Caption appears: `{d}(bedtime.)`

* Each nest cleared: the tint drops one level in that region and its citizens come out with one authored line each (12 per nest, hash-picked). With 2 nests left: Rivet's upgrade; the sign reads 0 and is on fire. `{d}It's a different fire.`

### Ep 3.4 "Under the Fountain" (MIDPOINT TWIST; hour ~7)
* The last nest is under the plaza. Clear it and the **Giggling Fountain cracks** `[SHK_CRACK]` `[FLASH v10,5]`. Beneath: **pipes**. Purple sludge pulses toward the Wall, stamped with a hammer mark. Dr. Quark's note (S161) is on the rim.
* Rivet goes still `[STILL Rivet]` `[MUTE]`.
> RIVET: [SHOCK] {d}...That's Mum's mark.
> RIVET: [ANGRY] Who re-plumbed this? This was / a GIFT. It was a TWO-WAY / fountain!
> `[R13]` Everyone turns, slowly, `[!RIPPLE at R3 spacing]`, to the Mayor.
* **Flashback F3 "The Plumber's Order"** (40 s, sepia): ten years ago, a young Mayor in a too-big sash signs a form: "RE: Fountain. Keep giggles IN. Pump glums OUT." The plumber: "Out where?" Young Giggs: `{d}Not here.` The signature is the handwriting from S107's last page.
* The partial confession, the refrain's second time:
> MAYOR: [SAD] Everyone was so sad after / the Mingle. So scared.
> MAYOR: I just wanted the sad to / go... away.
> MAYOR: {d}We don't talk about the / other side.{..} That was the / whole point.
> MAYOR: [SAD] {d}I never asked where it / went.{punch} `[HOLD 26]` `[NO BUTTON]`
> FUZZ (over the bell, quietly): {..}It went to them. And it / grew.
* **Dr. Bloom says it out loud for the first time:** "I'm from over there. I've been / here ten years. I was too / scared to say." `[R21]` Nana takes her hand. (The first "say it" moment; the finale is built on it.)

### Ep 3.5 "Hattie's Last Laugh" / "Grampy's Garden" (the two story exorcisms, authored; either order)
**Hattie Haha** (true name **HAZE**): stand-up for an empty, burning cafe, laughing a beat late. **The rite is a comedy duel:** the prayer phase is the Ep 1.4 timing minigame and she heckles you with your own failed beats ("You were late. You were LATE. Hee. Hee."). Clue: "We hate the smoke. We water plants when no one's looking." Props: a watering can, a plain-water pen, an apron, a wilted tomato. Names: HAZE, CLINKER, PLODDY, WHEEZLE.
> HATTIE (saved): [SICK] I was in there the whole / time. Laughing on the wrong / beat.
> HATTIE: [SAD] There was someone else in / there too. She hated every / second. `[R8]` [HAPPY] {..}She had great timing, / though.
> (lost) Hattie's ghost tells one joke to an empty room. Nobody laughs. `[HOLD 21]` Mascotty claps, once.

**Grampy Bean** (true name **SOOTKIN**): every tomato has an eye; he doesn't know you ("Sonny? Toaster?"). **The true-name pick is a tomato memory game:** hand him tomatoes, one detail each ("the seeds came over the fence... a little mouse-person... xe whistled..."); the seed packet from S124 is the first clue. Names: SOOTKIN, SHACKLES, DRUDGE, CINDERS.
> GRAMPY (saved): [HAPPY] Sunny! You're Sunny! I / remember everything! {..} / Where are my glasses?
> GRAMPY: [SAD] {d}...The mouse who threw the / seeds. I never said thank / you.
> (lost) the tomatoes stay purple; the bench has one at the festival.

### Ep 3.6 "The Climb" (45 min; C3_SPIRE; Hell Breach 2)
**Purpose:** give Vapula a voice, menace and motive; **Vex scoops the jar**.
* `L_all_nests`: "Lord Vapula himself waits at the top. Go with love." Seven floors; on each, a caption between waves (`lvd_boss_intro`): 1 "COME UP, LITTLE SNACK." 2 "You smell like Hillvile. Like frosting over a frown." 3 (puff) "Ooh. I KNOW your flavour." / (declined) "You didn't even TRY me. Rude." 4 (`SF_ASKED_OTHER_SIDE`) "You ASKED. Nobody ever ASKS." `[R21]` "...thank you." / else "Nobody ever asks. Nobody ever ASKED." 5 (Grumbleguts cameo backing into the doorway to block it; the camera cannot get round him) "Every sigh. Every 'it's fine.' Every swallowed scream. That's ME." 6 "I am every sigh you swallowed. And I am SO. HUNGRY." 7 `[SILENCE]` then a tiny cough: `{d}*cough*`
* **The boss** (Breach 2): his attacks are the town's swallowed feelings: "it's fine" pellets, Hee-Hee homing puffs; laughter deals damage (Hattie's rimshot pickups, spoons on the CUTE path). Phase 3: he inhales 10 f, holds 5 f with puffed cheeks; the pet beeps; he laughs by accident. FWOOMP.
* **CUT_VAPULA_DOWN** re-timed: "Lord Vapula, Duke of the Ninth Cloud, lets out one last enormous wheeze..." *pffffffff* for 64 f with a pitch slide; the crown lands crooked at f 48; the sad little squeak. **He does not die:** a small grey cloud the size of a cupcake drifts to the floor. **Vex** (freed on floor 4, tied to a chair with his own shop ribbon: "I left the hatch unlocked. / Don't make it weird.") **scoops it into a jar, puts the lid on, pokes air holes.** `[BUTTON +22: the jar sighs]`.
> VEX: He's small now. `[R26]` He was always small. He / just had a lot of air.
* Celebration at the church: confetti, fireworks. Too early.

### Ep 3.7 "The Leftover Breath" (the siege; the pet's sacrifice; 40 min)
**Purpose:** the personal cost. The house defence pays off and empties the house.
* `[NIGHT]` The Mayor insists the celebration moves to **your house** ("the only house still standing with a door"): a housewarming callback. Guests: Pip and Mr. Flops, Miss Lulla and two kids, Nana, Kevin, the Mayor at the window "as a decoy". Wisp is giddy; the pet bounces; the boards are coming down. Then the sky **beeps**. The leftover breath (the biggest swallowed fear in town, uneaten) brings the last of the demons.
* **The siege** (`housing_final.md` §10.3's three waves): door imps, a brute at the window `[SQUASH 1.3/0.8]` against the plate, the thirty-second quiet with authored guest lines between waves (Kevin's third eye watches the chimney), the rule of three of "the door is going to give" false alarms 15 f apart (1 px, 3 px, 5 px with a red flash on the third... which holds).
* **Wave 3:** the fog comes down the chimney, one tendril. `[FREEZE ALL]` `[MUTE 14]`. It speaks **in the Mayor's voice**:
> ???: {d}Hee.{..} Hee.
> ???: {d}What if they hate us. What / if they're right to.
> (`SF_VAPE_SAMPLE`) ???: {d}Grape. {..}I know you.
* It goes for **you** (the newest, emptiest heart in town; it says so). **The pet, alone, is unfrozen.** It walks to the fireplace. "bep?" The fog, in the pet's voice, lower: "beep." The pet gets between you and the fog and **inhales all of it**: the s07 inhale, held 5 f, *gulp*. `[INSERT 4: the LCD fills the screen: a pentagram, then the Mayor's crying face]` `[ONE CHANGE: full horns]`. It looks at you. `[R8]` "beep." `[R13]` It **runs**: up the chimney `[FLASH w7]`. `[THAW]` `[PANIC]`; the longest wave.
* Dawn. A wall is gone either way (`SF_HOUSE_WALL_GONE`; a 3-tile breach with the Garden's sky behind it until Ep 4.4 welds the Annex on). **HELD** stamps if 4 guests kept safe. The egg nest is empty.
* Follow the trail of horned poops, one per 4 cells, across the red-dark lane to the Wall gate. It is closed. A tiny shape squeezed under it. **The first player line:** the box opens; `[WAIT A]`; the name pill `[POP]`s with `TW_POP_BUBBLE`; the text types at setup speed: `@P: "@E!"` `[R5]` from the other side, very far away, very small: "beep." `[SILENCE]` `[IRIS○ out 10 on the gate]` `[NO BUTTON]`

### Flashback F2 "Mingle Night" (60 s; plays as you sit at the gate; the full version of Ep 1.6's insert)
The Mingle, eleven years ago: lanterns, both sides dancing, baby Pip, Rivet with a torch. A purple cloud rolls in, enormous; a string trails from it. Lady Smog breathes it and walks into the kids' tent saying the kids' fears in their own voices. Young Fuzz tries a hug; it fails; she flees. Young Giggs stands between the two sides, shaking. He looks up at the cloud for `[HOLD 13]`, and does not recognise it. Nox reaches toward him across the grass. He doesn't take her hand. YOUNG GIGGS: `{d}...Close the park.` `[SMASH]` with no SFX (the only silent smash cut in the game) back to the gate.

### Interlude III "The Empty Room" (THE DARK NIGHT; playable; ~8 min; no music)
**Purpose:** the lowest point, held as long as the player wants, then given back by the town.
1. Home. `home_phase = EMPTY`: furniture burned, given away as barricades, or knocked over. The pet bed is empty. Wisp is quiet in a corner. **Decorate mode is open but can only remove**; placing anything gets a soft "nope" with no sound. `[SILENCE]`: the only sound is Tock's clock. A on the floor: you sit; the camera drifts closer at 0.04 px/f. Every item you placed can be examined and has one sad line.
2. **The Mayor stays.** He sits on the crate. The confession, with the crayon style bleeding into the room's edges for 2 f every `[R21]`:
> MAYOR: [SAD] It was mine. The breath. My / fear. Your little @E ate my / fear.
> MAYOR: I never asked where the sad / went. {..}I know where it went / now. `[HOLD 26]`
> MAYOR: I had a cloud once. `[HOLD 26]` He followed me to school.
> MAYOR: He rained on bullies. Only / bullies. He was very / precise.
> MAYOR: I put a stone up. I was / nine. I never told anyone / what it was for. (`SF_VISITED_GRAVE`: "You saw the stone.")
> MAYOR: Go get yours back, @P. / {d}Don't put up a stone. `[NO BUTTON]`
> MAYOR: I'll come with you. Over / there.{..} Tomorrow. (he leaves)
3. **The knocks.** The rule of three slowed to a heartbeat: each knock a separate visitor, R26 or more apart; the player opens the door each time; each visitor **places their item with the DROP animation** and the room fills; **music returns one instrument per visitor**, sneaking back in on the grid (timing bible §3.8); by the seventh it is HAHALAND_NIGHT, slower. Items are tagged "gift from X" forever and cannot be sold.
   1. **Nana**, a lamp. "Light's the first thing. Then soup." She doesn't leave. `[NO BUTTON]`
   2. **Pip**, Mr. Flops. "He's good at being held. You can borrow him. Not forever. ...Mostly forever."
   3. **Rivet**, the sign reset to **1**. "Don't make it weird." `[R13]` She makes it weird: a hug like a vice.
   4. **Steamy** (saved) with a duck / (lost) her ghost with a duck that is also a ghost.
   5. **Hattie** (or her ghost), with nothing. "I didn't bring anything. I'm the bit."
   6. **Dr. Bloom**, a blanket. "Cold hands. Warm blanket. That's the deal."
   7. **Mascotty**, who has never been inside anything. Perfectly still in the doorway `[HOLD 21]`. She places a single Hellvile sunflower seed. She leaves.
4. **Last knock: the Mayor.** He doesn't come in.
> MAYOR: [SAD] Tomorrow, I said. `[R26]` [ANGRY] {big}No.{/} Now. `[R8]` [SHOCK] {d}...Hee. Hee. Oh no. Now.
* `[IRIS♥ out 10 on the full room]` → Chapter 4. `home_phase = OPEN`.

---------------------------------------------------------------------------------------------------
## 13. CHAPTER 4: HELLVILE PARK (3.5 h, days 13-16)

**Purpose:** the other side is people and the monster was ours. The pet hunt drives the plot, the reunions are the heart, the summit is the climax, the ending is never undercut. **Music:** HELLVILE, which is HAHALAND_DAY in a minor key; the player notices in Ep 4.6 when they play together. Chapter card: half-purple checker, a beating heart with a tiny horn and a tiny halo, both crooked.

### Ep 4.1 "The Gate" (30 min; C4_GATE)
* You, the Mayor, Fuzz and Rivet at the gate. Rivet: "I made the lock. I was ten. / It's a REALLY good lock." `[R26]` "Hold this." (the torch). You cut your own bead: a short weld. The gate swings. Behind it: **a belly.** Grumbleguts, leaning on the gate, reading his little black book `[SCRATCH]`; he turns round slowly `[SCRATCH]`.
> GRUMBLEGUTS: Well HELLO, Hillvile. Come up / and see my gate sometime. / This is my gate.
> GRUMBLEGUTS: Nobody passes without / giving me their number.
> (menu: a fake number / a cupcake / "Is that a riding crop?") Cupcake: GURGLE: gurgle. `{r}` G: ...The belly says yes. The / belly outranks me.
> GRUMBLEGUTS (to Fuzz): And who is THIS stuffed little— FUZZ: [SHOCK] {d}No. G: Playing hard to get. I / respect it.
* **Crossing is a record scratch:** `[TAPESTOP]` `[MUTE 14]`, the HELLVILE theme on a downbeat, the camera `[BRAKE]`s as Hellvile is revealed: grim **and** cosy, bone lamps, soup steam, laundry lines of black lace, kids playing hopscotch on lava in oven mitts. Hell elements drip and do not wobble.
* **Tallyworn** at the Pump, chalk in hand. He looks at the open gate. He puts the chalk down. `[HOLD 26]` `[MUTE 26]`. He writes "3,661" anyway. `{d}Habit.` `[R21]` "Tomorrow I'll write something / else. I don't know what yet."
> TALLYWORN: Ten years I pumped because / nobody said stop. {..}Nobody / said my name, either. (the template's first and model use)
> TALLYWORN: [SAD] Some days back... someone / shouted my name over the / Wall.
> TALLYWORN: First time in ten years / anyone said it. {..}I sat down / and had a little cry.
> (menu: "That was me." / say nothing) → TALLYWORN: [LOVE] {d}...Oh.{..} Thank you for saying / it right. (sets `SF_TALLY_THAT_WAS_ME`)
* The pet's beep, distant, from the north: the Underbelly.

### Ep 4.2 "Shift Swap" (40 min; replaces C4_PRISONERS)
* The pump yard: rows of Hellvile citizens working bellows under the gaze of the loyalists (Hazey's foghorn). **Swap shifts with 5 named workers** (a 30 s bellows rhythm minigame each; the rhythm is "Lullaby for the Wall", S148). While you pump, each says one thing about who they were before the Wall: Clinker ("I write poems on the pumps. Forty, so far."), Ploddy, **Wheezle** ("I cough little ghosts out. I name them. One went over the Wall. I called it Sprinkles."; seeds chain I), **Haze** (Hattie's possessor), **Thumpkin** (thumps the pipe 3-then-1 "for my sister").
* **Haze meets Hattie** (if saved): HATTIE: "You. You were in my head. You / have GREAT timing." HAZE: [SHOCK] "{..}You were LOUD in there." HATTIE: "Knock knock." HAZE: `{d}...Who's there.` HATTIE: "Fresh air." `[R8]` Haze snorts, the first laugh in ten years `[POP]`; the pumps stop one at a time `[R3 ripple]`.
* Hazey blows the foghorn: "LORD VAPULA WILL... um. Is he... coming?" `[R26]` Vex walks past carrying the jar at head height. From inside: "...squeak." Hazey, quietly: "...Oh."
* Wisp (following, translucent) is scared of the red sky and points the way to Hell Pits 13 for Ep 4.4; S167's daytime window is the comic counterweight.

### Ep 4.3 "Mother Nox's Three Favours" (60 min; C4_HELP; three mini-episodes, any order)
Nox in her chapel kitchen; two bowls on the table; she sets a third without looking. She recognises the Mayor `[STILL Nox]` `[HOLD 21]` and simply turns back to the soup. (The summit is set up by not talking.) Button: the pet beeps at the second bowl; Nox, not turning: "That one's not yours."

**1. Sootkin's Sunflower.** Bring Hillvile flowers for the secret sunflower under the gate; **also Grampy's seeds**. SOOTKIN: "...Those are MY seeds. I threw / them over ten years ago!" If Grampy was saved he is there: "The mouse! You whistled!" They whistle together. `[NO BUTTON]` Button two minutes later: the replanted flower turns to face the Mayor's side, then slowly back. SOOTKIN: `{d}It can't decide.` 

**2. Lady Smog** (the last exorcism; the hardest; Fuzz leads). His hands shake; `[INSERT 3: the kids' tent]`. Nox: "You're the stuffed one. We have / the same rite. Ours goes / backwards." **The two-faith rite:** Fuzz's prayer rhythm forwards, Nox's backwards, SONG_EXORCISM on two channels meeting in the middle. Clue props: a crooked crown, a lost string, a school bag, a gravestone. Names: **VAPULA, GIGGLESWORTH, FLUFFY, NIMBUS.** The Mayor flinches `[EXPR SHOCK]` for `[R8]` when the board appears. VAPULA: a near miss, no strike; the puff laughs: `{d}That's what I'm WEARING.` (sets `SF_SMOG_NEAR_MISS`). GIGGLESWORTH: a strike and a gut punch (the Mayor sits down). **FLUFFY** completes it.
> LADY SMOG (saved): [SICK] I haven't tasted air in / eleven years. [HAPPY] {big}It's / DELICIOUS.{/}
> FUZZ: [SAD] Lady Smog, eleven years / ago, I—
> LADY SMOG: You hugged me. {.}Nobody else / did. {d}It didn't work. It was / lovely.
> FUZZ: `[R13]` The hug was never for the / demon. `[R13]` It was for the one holding / the demon. `[R26]` `[NO BUTTON]`
> (lost) her spirit drifts out as smog in a gown and **curtsies to Fuzz**; he plants Mr. Huggins the 8th at her door as a guard of honour; `SF_SMOG_LOST`: the jar never rains in Ep 4.6.
The puff goes into the jar with the rest of him.

**3. Anvil and Rivet** (from 6 lines to a scene). Rivet will not come; then she comes. The forge: Anvil at the anvil, back to the door, hammering. A table set for four; one plate has never been used; it has been set every night for ten years. Rivet in the doorway `[STILL]` `[HOLD 26]` `[MUTE 64]`.
> RIVET: [SAD] ...Mum?
> (the hammer stops mid-swing `[FREEZE Anvil]` and stays up `[HOLD 21]`)
> ANVIL: [SHOCK] ...RIVET?! `[THAW]` `[SMASH]` they hug like two vices closing; sparks `[SHK_POW]`; the welding mask falls off
> RIVET: I saw the pipes. Under the / fountain. Your mark.
> ANVIL: [SAD] They took my gift and made it / a drain. I couldn't stop / them.
> ANVIL: [HAPPY] {..}I made you a spoon. Ten / years ago. For when you came / back.
> RIVET: {d}...It's a REALLY good spoon. `[EXPR Rivet=LOVE]`, the first time in the game
Dinner is played: pass the dishes (a one-minute cooking variant). Sprocket by the stove: "I didn't read your letters." `[R8]` "...I read them every night." Bolt drops a wrench. A candle on the table; Rivet's sign reads 0. `[BUTTON: Rivet's tail is on fire again]`
* `L_help_done` now branches on Smog's outcome (§13a).

### Ep 4.4 "Wisp's Room" (40 min; new; the house thread pays off)
* Wisp guides you to **Hell Pits apartment 13** (Graveley's building). A tiny imp body sleeps in a bed, **exactly** the bed that appeared in your house. Wisp's parents sit beside it, two years in. Wisp's mum is humming (the lullaby Steamy hummed wrong).
* **Decorate-for:** the room was cleared out "to make it less sad". You rebuild it in someone else's home from the memory of what Wisp did to yours (the "Rearranged by ???" list): the chair facing the wall ("my thinking chair"), the rotated rug, the little bed's angle, the drawing. Each correct piece makes Wisp **more solid** (opacity 8→16 in steps); a wrong piece Wisp gently moves back: `{w}no, it was like THIS.`
* When it is right: `[MUTE]`. Wisp lies down in their own body. `[HOLD 21]`. Fingers twitch.
> WISP'S MUM: [SHOCK] ...Wisp?
> WISP: [SLEEPY] ...i went somewhere. there / was a nice house. with a / chair. `[NO BUTTON]` (the stillest beat in Ch4)
* **The second player line.** Wisp, awake, to you, while the parents fetch soup:
> WISP: Did they talk to me? While / I was out?
> `@P:` "Your mum hummed." (true: you heard it) / "They said your name every night." (the kind lie) / "They kept your chair." (true: it is the thinking chair) → `STRY.wisp_answer`; the epilogue line and S206's second option change.
* **The pen pals.** Wisp sees the first paper boat in your bag: "That's MY house. You KEPT it?" Pip has come through the gate with Nana. The two kids trade plushies; Pip lends Mr. Flops for one night: "He's good at scary. One night. / I'll be fine. My eyes might / do a rain." WISP: "Can Mr. Flops stay? He's / good at scary." `[BUTTON +22: Mr. Flops's stitched heart (S102) and the drawing's candle are the same colour; the pet notices and beeps]`.
* Rewards: Wisp's drawing (decor, "gift from Wisp"); the **Hell Pits flat** next door (your second home); the Annex is welded onto the cottage's south wall in a covered cut of sparks (22 f), the seam a visible ugly bead; Charlotte's pentagram rug teaches the cursed-item rule (`[INSERT 4]`, the lamp grows horns, Kevin: "...it's fine." at `[R8]`).

### Dream D2 "The Ninth Cloud" (90 s; night in the Hell Pits flat)
You float in Vapula's memory: a sky of unsent letters, frowns and "it's fine"s drifting the wrong way, written in the daycare kids' hands (S158). Read three (A): the Mayor's ("Dear Nox, I'm sorry. I'm sorry. I'm"), Nana's ("I miss the bone-flour bread"), Fuzz's ("I should have held on longer"). At the centre a small cloud at a wall, waiting, in the LCD style. **The LCD pet walks up and sits next to it.** `[HOLD 26]`. Then the LCD cloud rains one pixel. `[IRIS○ out]`. Wake: the jar on your shelf has a tiny halo. The game does not mention it. Sets `SF_VAPULA_PLUSH`.

### Ep 4.5 "Find the Doomogachi" (45 min; new; Hell Breach 7, the Underbelly)
* Into the Underbelly; the beeps get closer. **Grumbleguts blocks the way**, lounging across the gate, rear toward the camera, admiring himself in a puddle of lava.

| f | event |
|---|---|
| 0 | `SCENE_BREACH` intro card: bedroom eyes; "Keeper of the Sealed Gate. Also of a very large seat." |
| 30 | GRUMBLEGUTS: Well, hello, new kid. {..} / You're cute when you're / scared. |
| 50 | (Fuzz present) GRUMBLEGUTS: Padre. Come up and see my / gate sometime. `[R13]` FUZZ: [SHOCK] {d}No. `[R8]` G: Playing hard to get. I / respect it. |
| 80 | GURGLE: gurgle? G: Not now. We're being / menacing. `[R5]` GURGLE: gurgle. |
| 95 | the belt unbuckled with the dramatic sigh `[R21]`; gut in `[HOLD 13]`; out `{r}` `[SHK_POW]` |
| 110 | G: {big}Nobody passes my gate!{/} / {..}Nobody ever tries, honestly. `[SMASH]` fight |

* **The fight** (§3). **Defeated or fed:** he sits heavily; the gate groans. "...Is there dessert?" `[R8]` GURGLE: gurgle. `[R13]`
> GRUMBLEGUTS: [SAD] There's a THING down there. It / beeps. {..}It ate my lunch. / {d}Nobody eats my lunch.
> GRUMBLEGUTS: [SAD] Kept this gate ten years. / Nobody came up to see it.
> NOX (arriving): Brimstone Bites needs a line / cook. You'd have to wear a / shirt. G: ...A TIGHT shirt? `{r}`
> GURGLE: gurgle. G: He says go on, kid. `[BUTTON +13: he straightens a harness strap and winks at the Gate]`
* **The Pump room** (the old labour contract's engine): twelve soul bottles still in it. Rivet cuts the main valve; Tallyworn, Shackles and Drudge turn the wheel; **Vex, late, opens every bottle by hand**: twelve wisps up through the cracks in the roof toward both towns `[MUTE 64]`. **Signhere** appears with the 900-page labour contract: "This is in breach of page 412." Everyone present tears one page. Nine hundred pages. `[HOLD 26]` of confetti. (S217 follows.)
* **The bottom:** @E, horned, huge, a swollen purple silhouette; the Mayor's fear inside, talking in his voice. **The boss is a pet-care minigame under fire:** feed, cuddle, play while the fear lashes out; each care action shrinks it; each attack is an "it's fine" pellet to dodge.
* **The pet turns its back** (heart Ep 3.6 beat 3, moved here): the swollen pet hears the fear speak in the Mayor's voice; looks at the fear; looks at you; "bep?" `[HOLD 13]`; turns around `{r}`.
* The Mayor steps in front of you (he followed you down) and says it, the third time, the right way:
> MAYOR: [SAD] That's mine. That's my fear. / Give it back, little one.
> MAYOR: I was scared. Of them. Of me. / Of what I did.
> MAYOR: [SAD] {d}I'm still scared.{..} You can / let go now.
* The pet exhales: FWOOMP `[COVER 22]`. Pet-sized again, **with horns** (`SF_PET_HORNS`, permanent, every portrait). `[R2]` "beep." `[R8]` `{d}...it's fine.` **This time it is fine.** `[R13]` A real "beep!" with a hop. `USTORY_PACIFIST` if no brutal weapon was equipped through the waves and Grumbleguts was fed.
* Button: Grumbleguts, in a tight shirt, carrying the forklift's next course up the stairs: "Light snack."

### Ep 4.6 "The Summit" (50 min; C4_MAYOR; the climax)
* Fetch the Mayor: "You walked through demons for / us. I can walk through a / gate." His hands shake on the handle; Rivet's bead is still warm. Grumbleguts holds the gate: "Mayor. Love the hat." MAYOR: "It's my head." G: "I know."
* **F4 "Cupcakes and Bone Soup"** (20 s, sepia): two kids on a blanket at the Mingle swap food. NOX: "Soup's better." GIGGS: "...Cupcakes are better." Both: "...Both are better."
* **The summit** at Nox's table (the bandstand's two tables pushed together):
> MAYOR: [SAD] Nox. {..} I brought cupcakes.
> NOX: `[STILL]` `[HOLD 21]` {d}...I brought bone soup.
> MAYOR: [LOVE] `[R8]` ...I LOVE soup.
> NOX (quieter): `[R13]` You're late. `[HOLD 21]`
> MAYOR: I closed it.
> NOX: I let you. `[NO BUTTON]`
* **The jar.** Vex sets it on the table between the bowls. A grey cloud the size of a cupcake. The Mayor looks at it for `[HOLD 26]` `[MUTE]`. The crayon style bleeds in for 2 frames.
> MAYOR: ...Hi, Fluffy. `[HOLD 26]`
> (two drops of rain inside the jar; `[HOLD 26]`; if `SF_SMOG_LOST`: no rain, the hold is 39 and the Mayor says it again, smaller)
> VAPULA (tiny, appears): (drip.) `[R8]` (drip.)
> The pet beeps. The crowd `[!RIPPLE]` "awww". The Mayor: Hee. Hee. — and it is, for the first time, a real one; **the music comes back on the grid.**
> MAYOR: [SAD] I was scared. I'm still / scared.{..} Hold my hand? `[HOLD 26]` She takes it. `[NO BUTTON]`
> NOX: Then I believe we have / nothing left to build a Wall / for.
* **"Say It"** (the finale mechanic): everyone walks to the Wall. Rivet and Anvil re-plumb the fountain two-way (a two-player welding QTE: you are Rivet's hands, Anvil calls the rhythm; the new hammer mark from S105 if done). Citizens line both sides; each friend says **one unsaid thing** as you pass (hash-picked from a pool of ~60 plus authored cast lines: "I'm scared of the dark." "I miss my brother." "I never liked grape." Rattlecup: "I'm not a bother." Dr. Bloom: "I'm from here."). Each one sends a **giggle bubble** through the fountain to the other side; the bubbles stack against the Wall. Last: **you**, the third player line: "I was lonely." / "I was scared too." / "I like it here." / "I'm home." → `STRY.sayit_line`.
* **CUT_WALL_FALLS** on a beat script: `[SHK_RUMBLE 12]`; Rivet cuts her own lock; Fuzz and Nox sing the rite forwards and backwards; the bricks fall under staggered gravity (the existing best animation, kept); Grumbleguts belly-bumps the last stubborn section `[SQUASH 1.4/0.7]`; **HAHALAND_DAY and HELLVILE play at once and are revealed to be the same tune**, major and minor resolving to one chord; the cast runs in from both sides; hearts rise. **Never undercut.** `[NO BUTTON]`. No beep, no "it's fine", no belt.
* **Mascotty** walks into the rubble and takes off her head for the first time: a small, tired bunnykin. Thumpkin thumps once. She thumps back once. They hug. `[HOLD 26]` `[NO BUTTON]`

### Ep 4.7 "Hellvile Park" (C4_FESTIVAL + C4_STAGE; the ending)
* The united park; instead of "chat with 4 people", **four festival stalls you run** (3 min each, each ends with a cameo): cooking with Nana and Charlotte, fishing at the Lake of Fire (Gerald, S144, is there, in lava, fine), the photo booth with Shutterbug, the comedy tent with Hattie.
* **Hattie's set**, the third refrain: "So. {..} We don't talk about the other / side." `[R13]` [HAPPY] "{big}Because we're STANDING / in it!{/}" `[!RIPPLE]` both sides laugh; Haze snorts; the Mayor goes "Ha!" `[FREEZE ALL]` everyone stares. "...Ha?" `[THAW]` They laugh harder.
* **Sprinklepop's soul** bounces in with a heart balloon (chain I done) or with the map still upside down.
* **Grumbleguts at the buffet**: back to the crowd, rear framed front and centre, the table emptying; every time the camera "accidentally" cuts back he is bigger `[3x]`; Honkers offers a napkin; he flirts with the napkin; the napkin is not interested. **The festival's button:** his belt pings off and hits the Mayor's hat `{r}`. (Here, not at the Wall.)
* **The Unity Stage:** the Mayor ("Population: 1,001." — the pet, horned, on his hat); Nox; Pip ("And Mr. Flops!" Mr. Flops has felt horns: "so his brother doesn't feel weird"); Wisp ("And my chair."); Fuzz ("Even you, Vex." VEX: "...thanks for saying Vex."); Vex ("plain water vapes now. They're terrible."); Rivet ("Hit it!" / brutal: "Bonk it!"); Tallyworn: "Day one."
* **The credits parade** (as now, 500 pairs; ghosts "in loving memory"), led by you, the pet with horns and Wisp in a body; Vex carries the jar; Grumbleguts at the roadside still eating, waves a drumstick at the camera, then at the Gate. "Everyone is together in Hellvile Park."

---------------------------------------------------------------------------------------------------
## 13a. EPILOGUE "Happily Ever After (Mostly)" (0.5 h) and post-game

* **E1, the post-credits button** (the trailer's s15): home; the pet, horns; strain 8 f with 1 px jitter; plop; `[R8]` the poop grows horns `[POP]`; "beep." `[IRIS♥ out 10 on the poop]`. *THE END... of the beginning!* The title screen's logo is now on fire: `{d}(it's fine.)`
* **E2, "The Suitcase"**: unpack it at last: a single empty photo frame. Shutterbug's festival polaroid goes in it, captioned with your Say It line. Place it anywhere. `[NO BUTTON]`. Unlocks the **Park Plot** (DREAM phase; Build mode).
* **E3, "Population: 1000"**: S143.
* **E4, "The Feast of Rebuilding"**: S186.
* **Days Since:** morning 17: Tallyworn's board says "1"; the Days Since sign at Rivet's says 1 with a cake on it; Wisp's line depends on `STRY.wisp_answer` (kind lie: "I know she didn't say it / every night. {..}Thanks anyway."; hummed: "She still hums it. On / purpose now."; chair: "I sat in it. It's a good / chair.").
* **Post-game:** free roam; all 5 original post-game quests plus the 23 Ch5 missions; weekly house ratings; Arena variants of Breach 3-7; the Plushie Press; the Two-Door Clinic; Mixed Bathing Fridays; the comedy tent; the Mingle calendar event. **NG+ "HaHaland Remembers"** (`MODE_NEW_GAME_PLUS`, implemented): horns from the start; gag props kept; wrongnesses subtitled ("(someone over there just laughed)"); the Feast culprit re-drawn; NG+-only lines ("Have we... done this before? {d}Hee. Hee."). The title screen changes after the credits (on fire), after S143 ("Population: 1000") and after S195 (Mingle Night).

### 13b. Continuity fixes this design depends on (apply everywhere)

| Item | Fix |
|---|---|
| `SF_VAPE_SAMPLE` | read in Ep 3.6 floor 3, Ep 3.7 (the fog's "Grape. I know you."), S135, D1 variant |
| `SF_WEAPON_BRUTAL` | Rivet Ep 3.2 ("Did you name it? You named it."), turret skin, Fuzz's rate-limited "tsk." caption, Rivet's stage line, `USTORY_PACIFIST`, S174 route |
| `SF_STEAMY_LOST` | Ep 3.3 graveyard, Interlude III visitor 4, chain H giver swaps, S118's water |
| Smog lost or saved | `L_help_done` **branches** (today it says "breathing" either way); the jar's rain in Ep 4.6 |
| `SF_VEX_REFORMED`, `SF_VAPULA_PLUSH` | chains K and L |
| `USTORY_PACIFIST` | granted for the siege waves and Ep 4.5 without the brutal weapon equipped, Grumbleguts fed |
| Pronouns | Steamy is **she** (`L_steamy_lost` says "him": fix); Lagspike: pick "her" (roster); Rivet she; Sootkin xe; every grafted heart line re-pronouned |
| Chapter titles | keep the code's titles; update `docs/STORY.md` |
| Quests #9 / #16 / #26 | retitle "Pier Panorama", "Streamer Scream", "Spider Ride" so "Graveyard Shift" stays Breach 5 |
| The starer | is **Mascotty (106)**, never Haybale (35); Haybale is a farmer |
| Fluffy's grave | the sign text becomes "Fluffy the cloud. Drifted." in a child's hand (no year) |
| `town.h:73` comment | "1..4 chapters, 5 = done" |

---------------------------------------------------------------------------------------------------
# PART 2 — SIDE MISSIONS

## 14. 109 new side missions (S100-S217), on top of the 49 existing ones

**Numbering.** The horror bible's S100-S195 ids are kept where the mission survives; nine ids are **retired** and never reused (S141 merged into S134; S160 merged into S135 and S142; S175, S177, S188, S189, S190, S193, S194 cut as padding; Creakster's uncued creak lives on as one beat of S166). The 22 grafts take S196-S217. The side-quest table therefore has 118 rows, 109 live; retired rows are `KIND_NONE` and cost 2 save bits each (§17). The 49 existing quests (#0-#48) stay and are re-timed with dialogue beat codes; #9/#16/#26 are retitled (§13b).

**How to read a mission.** `Sxxx "Title"` | giver (roster id, side) | chapter window (5 = post-credits) | verbs. Then Hook, Beats, the Comic beat with timing, the Heart or Scare beat, Reward, and **Later** (the button that fires 1-10 minutes after turn-in, or on the next visit). Furniture rewards go in the house catalogue tagged "gift from <giver>" and cannot be sold. Verb key: talk, fetch, deliver, photo, find, combat (nest or Breach), exorcism, weld, pet, minigame, fishing, cooking, concert, studio, housing (yours or someone else's), creator, escort, timing (the comic-timing minigame), dream.

**Design rules for all 109** (the lint in §18.2 checks what it can): one mission = one trailer scene's anatomy stretched over minutes of play; one gag per 3-6 lines, at most one `{d}` per conversation; a scare mission withholds the release once, never twice in a row in a chain; chains advance at most one part per chapter; Hillvile givers in Ch3 also stand on the invaded map (inside or beside the church); Hellvile givers open from Ep 4.1, post-game ones from the credits; cross-border chains have parts 1-2 reachable in Ch1-3 and part 3+ in Ch4-5; at most four "Later"s are news-ticker lines (S140, S156, S168, S191); "Ten years I X because Y" appears in exactly three missions (S139, S179, S187).

### 14.1 Chains (22 chains, 84 missions)

#### Chain A: Pip and Mr. Flops (Hillvile; 3 parts)

**S100 "A Bed for Mr. Flops"** | Pip (42) | Ch1 (after Ep 1.3) | housing, fetch
* Hook: Mr. Flops "has nightmares about the beach". Pip asks you to build him a bed in *your* house, "because yours has a door that locks".
* Beats: 1) Fetch 2 Scrap and a Plushie. 2) Rivet isn't in town yet, so Nana sews it: a shoebox with a tea-towel blanket. 3) Place it (housing tutorial part 2: a *tiny* item). 4) Pip rotates it to face the window, then away from the window. 5) Mr. Flops is tucked in.
* Comic: Pip's three inspections `[3x@15]`: "Too close to the door." "Too close to the window." `[R13]` "{d}...Too close to the floor." `[BUTTON +13: the box slides 1 px]`
* Heart: Pip, leaving: "He sleeps better when someone / else is in the room. {..}So do I." `[NO BUTTON]`
* Reward: 30 H$, **Mr. Flops's Bed** (furniture). Later: in Ep 2.3 the bed is the one piece the haunting never touches; Wisp's little bed appears beside it.

**S101 "Night Patrol"** | Pip (42) | Ch2 | escort, photo, timing
* Hook: Pip has made a "Wall Watch" badge from a bottle cap and wants to patrol Hillvile Lane after dark "to prove monsters aren't real".
* Beats: 1) Meet at the fountain at night. 2) Walk 4 waypoints; Pip's courage meter drains in the dark and refills when you stand close. 3) At the lamp post that faces the Wall (W5) Pip hides behind you. 4) Photograph "proof there's nothing there". 5) The photo shows two red dots in the fog, far away.
* Comic: Pip with the polaroid `[HOLD 13]`: "Those are... fireflies. Big, {.}red, {.}fireflies." `[R8]` "Patrol's over." They sprint home on the flee curve, dragging you.
* Scare: on the way back the fog giggles **in Pip's voice**. Pip doesn't notice. You do. `[MUTE 21]` `[NO BUTTON]`
* Reward: 40 H$, Wall Watch Badge (decor). Later: the daycare's drawing wall gains "fireflies can be red now" in crayon.

**S102 "Flops the 1st Rides Again"** | Pip (42) | Ch3-4 (after the beach nest) | combat (Breach 4), pet, weld, housing
* Hook: Mr. Flops is back, but his twin from the Factory conveyor, "Flops the 1st", is still in there, half demon, half bunny.
* Beats: 1) Re-enter Breach 4. 2) The twin hops away whenever you aim; stop aiming and it comes close. 3) The pet sniffs it out (pet verb). 4) Rivet welds the torn ears back on. 5) Pip adopts it "as Mr. Flops's brother, because everyone should have one" and sews a stitched felt heart on both.
* Comic: the twin has one demon eye; Pip covers it with a sticker. `[R8]` The sticker blinks. `[R13]` PIP: "He's expressive."
* Heart: Pip gives *you* the twin: "For when I'm not here to be / brave at you." It sits by the pet bed from then on.
* Reward: **Flops the 1st** (furniture; blinks every 83 f), 80 H$. Later: Ep 4.7's felt horns are explained ("so his brother doesn't feel weird"); Sutura (542) asks who did "that beautiful ear work".

#### Chain B: Rivet, Anvil, Sprocket and Bolt (cross-border; 6 parts)

**S103 "Days Since Last Fire"** | Rivet (13) | Ch2 (after Ep 2.6) | weld, housing
* Hook: Chief Spark says Rivet's sign is "a safety hazard because it's always zero". She asks you to hang a second one at your house "so the average goes up".
* Beats: 1) Weld the sign (3-part weld; a spark lands on the pet: "beep.", fine). 2) Place it. 3) It counts real in-game days. 4) Rivet visits "to check the hinge"; her tail is on fire; the sign flips 7, "-", 0 `[SLAM]`.
* Comic: the s09 flip: 7, two jittery frames of "-", a red 0 pulsing every 4 f. Chief Spark, offscreen: "{d}I heard that."
* Heart: the sign in her mum's forge "read 0 every day and we were proud of it". `[R8]` "Don't make it weird."
* Reward: **Days Since Last Fire sign** (furniture; resets whenever Rivet visits). Later: the first day it reads 7, a text: "a personal best. don't make it weird."

**S104 "Letters Not Sent"** | Rivet (13) | Ch3 (church) | find, combat, deliver
* Hook: Rivet has written her parents a letter every month for ten years and posted none. They are in a toolbox, in her workshop, in a nest.
* Beats: 1) Fight to the workshop. 2) The toolbox is a demon's stool. 3) Bring it back. 4) She reads one aloud and stops at line two. 5) Ask her to carry them over the gate in Ch4 (she says no; she says yes).
* Comic: 120 letters all begin "Dear Mum and Dad, don't make it weird." RIVET: "I had a format."
* Heart: letter #1, aged 14: "I saw what they did with the fountain. I ran because I couldn't fix it. I'm learning to fix things." `[HOLD 21]` `[NO BUTTON]`
* Reward: 60 H$, Rivet's Toolbox (furniture). Later: Ep 4.3's "I read them every night" lands harder; the toolbox in your house has one letter sticking out of it after Ep 4.3, addressed to you.

**S105 "Two Marks"** | Anvil (595, Hellvile) | Ch4 (after Ep 4.3) | weld, fetch
* Hook: Anvil wants to re-stamp the fountain pipes with a **new** hammer mark, hers and Rivet's together, before Ep 4.6 re-plumbs them.
* Beats: 1) Fetch 3 Scrap from the salvage yard (Salvagia 553 haggles). 2) Pick the mark from four (an anvil, a rivet, both crossed, a heart). 3) A two-handed weld: you are Rivet's hands, Anvil calls the rhythm. 4) Stamp the first pipe; the sludge runs clear for one second.
* Comic: Anvil tests the weld by hitting it with a spoon. "It's a REALLY good spoon." `[R8]` RIVET: "Mum." ANVIL: "It IS."
* Heart: she keeps the old mark too: "I don't want to forget what they did with it. I want to remember what we did after."
* Reward: 100 H$, Anvil's Hammer Mark (a stamp that appears on anything you weld from now on), **Forge-Iron Bed** (furniture). Later: the Ep 4.6 QTE uses the new mark.

**S202 "The Door"** | Anvil (595) | Ch4 (after Ep 4.3) | housing, talk  *(grafted: heart S110)*
* Hook: Anvil has heard your front door squeaks and would like to see the hinge.
* Beats: 1) She visits the cottage. 2) She looks at the hinge `[HOLD 13]`: a tiny anvil stamp. 3) "I made this. Before the Wall. / For the Mingle houses." `[R13]` "It still squeaks? Good." 4) She does not oil it.
* Comic: Rivet oils it anyway; Anvil un-oils it; Rivet oils it; the door, mid-argument, squeaks by itself `[R8]`. Both: "...Leave it."
* Heart: ANVIL: "I left the squeak in on / purpose. So you'd know when / someone comes home." `[NO BUTTON]`
* Reward: **Anvil's Hinge** (furniture: the door squeaks forever; the catalogue calls it a feature). Later: the next time a knock comes at your door the squeak plays a quarter-beat before the knock; Wisp, inside: "someone's home."

**S203 "A Spoon for Mum"** | Rivet (13) | Ch4 | deliver, talk  *(grafted: heart S111)*
* Hook: Rivet has picked her best spoon off the rack and cannot carry it across herself.
* Beats: 1) Carry it to the forge. 2) Anvil takes it `[HOLD 13]`; opens a drawer: a tiny bent spoon, Rivet's, age six. 3) "I have one." `[HOLD 13]` "...I'll have two." 4) She puts them side by side on the plate that was set for ten years.
* Comic: Sprocket, "not reading" by the stove, turns a page; Bolt: "He's on chapter nine."
* Heart: the two spoons. `[NO BUTTON]`
* Reward: 50 H$, **Two Spoons** (furniture; the rack's centrepiece). Later: Rivet's workshop quip gains "Mum's got two now. {d}Don't make it weird."

**S106 "Bolt's First Weld"** | Bolt (597, Hellvile, KID) | Ch4-5 | weld, escort, housing
* Hook: Rivet's little sibling wants to weld "like her" and has stolen a torch. Rivet is terrified. Anvil says "let xem".
* Beats: 1) Escort Bolt to the forge without xem lighting anything (xe lights three things; Chief Spark, now cross-border, follows with a bucket). 2) Teach the weld with the difficulty capped. 3) A lopsided stool. 4) Bolt wants it in **your** house "because yours gets visitors".
* Comic: Bolt's hand-painted "DAYS SINCE LAST FIRE" sign reads 0 before xe starts. RIVET `[HOLD 13]`: "...That's my kid."
* Heart: Rivet realises she is standing exactly where she stood at six, holding the torch for Anvil. She says nothing. Anvil does: "Look at you. You're the one / holding the light now."
* Reward: **Bolt's Stool** (furniture; `TW_WOBBLE`), 50 H$, Grumpy Uncle sticker. Later: Grumbleguts to Bolt: "Eat your vegetables. {d}Not those. Those are mine."

#### Chain C: Mayor Giggs (Hillvile, then cross-border; 6 parts)

**S196 "Flowers for a Cloud"** | Mayor Giggs (0) | Ch1 | fetch, deliver  *(grafted: heart S58)*
* Hook: "Town beautification." Flowers for one grave, specifically.
* Beats: 1) Pick flowers (any bed). 2) The graveyard. 3) The grave: "Fluffy the cloud. Drifted." in a child's hand (W16). 4) Place them; the Mayor is there before you, pretending to be lost. `[EXPR SAD]` for `[R8]` exactly.
* Comic: "I'm inspecting the... / gravel. Hee. Hee." He inspects it. It is gravel.
* Heart: if you look closely (the quest log shows it), the handwriting on the stone matches the Mayor's speech notes.
* Reward: 30 H$, **Small Cloud Stone** (garden furniture; it rains 1 px on it whenever the Mayor visits). Later: the next time you pass the graveyard the flowers have been changed, and nobody did it in daylight.

**S107 "Speech Notes"** | Mayor Giggs (0) | Ch1 | find, talk
* Hook: he has lost his welcome speech. He has given it 400 times. He cannot remember a word.
* Beats: 1) Find the notes: the fountain, the bakery, Mascotty's paw (stand still R21; she opens it). 2) Every page has "Hee. Hee." in the margin where a sentence used to be. 3) He reads the speech. It is one sentence long.
* Comic: "Welcome to HaHaland, where {.}{.}{.}{d}hee. hee." `[HOLD 13]` "It kills every time."
* Scare: the last page, in different handwriting: "You never asked where it went." He folds it very quickly. `[MUTE 14]` "Old draft."
* Reward: 20 H$, Framed Speech (furniture). Later: Ep 3.4's plumber's order is in that handwriting.

**S108 "The Locked Drawer"** | Mayor Giggs (0) | Ch2 | find, fetch, talk
* Hook: Town Hall's bottom drawer is locked and "makes a noise". He wants you to open it so he doesn't have to.
* Beats: 1) Keysie (21) has a spare key for everyone but was asked to lose this one. 2) A Coffee and she "finds" it. 3) Inside: a child's drawing of two kids on a blanket, a jar of bone flour, a half-eaten cupcake eleven years old and perfectly preserved (Nana's recipe), and a frayed length of string with a loop at one end. 4) The noise is the cupcake. It giggles.
* Comic: it giggles R8 late, like the fountain. "{d}It's a fresh one." `[R13]` "It is not a fresh one."
* Heart: the drawing is signed "G + N". He puts it back. He doesn't lock the drawer this time. He puts the string in his pocket without a word.
* Reward: 40 H$. Later: the drawing is framed on his office wall in Ch4; the string is the lost string in Lady Smog's name phase.

**S197 "The Plaque"** | Mayor Giggs (0) | Ch2 | fetch, find  *(grafted: heart S74)*
* Hook: the Wall's foundation plaque needs polishing "for appearances".
* Beats: 1) Fetch Polish (Mintworth 56). 2) The plaque at (55,28): "ERECTED FOR THE SAFETY OF"; the rest under grime. 3) You start on the grime; the Mayor's hand lands on yours `[FREEZE Mayor]` `[HOLD 13]`: "That's enough. That's plenty. / Hee." 4) He leaves; you can keep polishing: a corner of a signature in crayon-coloured ink.
* Comic: the polish squeaks on every stroke in the fountain's rhythm; the fountain answers each squeak a beat late.
* Scare: the signature. `[MUTE 14]`
* Reward: 30 H$, Polish Tin (decor). Later: in Breach 6 (S179) the plaque's twin on the Hellvile side completes the sentence: "...EVERYONE. (G.G. / M.N.)"

**S198 "Tea for a Tic"** | Mayor Giggs (0) | Ch3 (church) | fetch, talk  *(grafted: heart S90)*
* Hook: the Mayor cannot stop: "Hee. Hee. Hee. Hee." Nana says tea.
* Beats: 1) Fetch tea: Mocha Latte (70) brews it from the one tin of Grimbrew in town. 2) Deliver under the pew. 3) He drinks; the Hee stops `[MUTE 41]`. 4) He sits with you in the silence `[HOLD 26]`. "That's worse. {..}Hee."
* Comic: Mocha Latte drew a cloud in the foam; he drinks around it; the cloud is the last thing in the cup.
* Heart: the first time the tic is named as a wound. `[NO BUTTON]`
* Reward: 40 H$, Teacup (cloud foam) (decor). Later: at the Ep 3.4 confession the cup is on the fountain's rim, empty except the foam cloud.

**S109 "The Apology Tour"** | Mayor Giggs (0) | Ch4-5 (after Ep 4.6) | escort, talk, cooking
* Hook: he wants to apologise to every Hellvile citizen personally. There are 500. He has baked 500 cupcakes and brought a wagon.
* Beats: 1) Escort the wagon; three scripted stops (Tallyworn, Lady Smog, the pump yard). 2) At each he starts a speech and someone accepts a cupcake and says "yeah, fine" before he finishes. 3) The wagon gets lighter; his sash straighter. 4) At the last stop Honkers honks at him. He laughs: "Ha!" `[FREEZE ALL]`.
* Comic: 500 citizens, zero speeches finished. Spreadsheet (531) hands him an itemised receipt: "apology, partial, x1".
* Heart: Tallyworn refuses the cupcake and asks for a handshake. The Mayor holds it a beat too long `[HOLD 21]`. "...Day one," says Tallyworn.
* Reward: 150 H$, **Mayoral Sash** (decor), the "Hee. Hee." jingle. Later: the fountain's giggle is exactly on time, forever; Fizz's histogram (S142) in your house redraws itself flat.

#### Chain D: Nana Puddin, Charlotte and Scorchmello (cross-border; 5 parts)

**S199 "The Loose Brick"** | Nana Puddin (1) | Ch1-2 (night) | deliver, find  *(grafted: heart S57)*
* Hook: Nana asks you to put a muffin in "the brick that wiggles" at the Wall after dark, and "don't wait".
* Beats: 1) Find the brick (54,26). 2) Place the muffin. 3) You wait anyway `[HOLD 26]` `[MUTE 26]`. 4) **A hand with soot on it takes the muffin from the other side** `[INSERT 3]` `[SHK_TAP]`. 5) A bag of cookies is pushed back, 90% charcoal.
* Scare: the hand.
* Heart: NANA: "Every Thursday. She never / missed one." `[R13]` "Neither did I." `[NO BUTTON]`
* Reward: 30 H$, Charcoal Cookies x3, **Cookie Tin (two-sided)** (furniture). Later: next Thursday the tin in your house has one burnt cookie in it that you did not put there.

**S110 "The Secret Ingredient"** | Nana Puddin (1) | Ch1 | cooking, fetch
* Hook: her melon bread "isn't right" and hasn't been for ten years. Taste-test five batches.
* Beats: 1) Cook with Granny (melon bread). 2) Taste 5 `[3x@15]` plus two: "sweeter", "saltier", "more melon", "less melon", "{d}...the same". 3) The missing ingredient is bone flour. "Can't get it anymore. Don't ask why."
* Comic: batch 5 is identical to batch 1. "I know. I just like watching you eat."
* Heart: the recipe card has a second name scribbled out. "Charlotte and I used to bake / together. {..}Different sides of / the park." `[R13]` "Eat up."
* Reward: 30 H$, Melon Bread x3, Nana's Recipe Card (decor). Later: Charlotte has the other half (S112).

**S111 "Muffin Recall"** | Nana Puddin (1) | Ch2 | fetch, find, talk
* Hook: a batch came out purple; she sold six before she noticed.
* Beats: 1) Six buyers, each one step from biting. 2) Zoomzoom (8) has already eaten one in under four seconds. 3) Zoomzoom speaks in "we" for a day (W7).
* Comic: every buyer you reach is mid-bite `[FREEZE]` `[HOLD 8]` as you shout. Oma Fluff (27): "I was only looking at it."
* Scare: the recovered muffins, back in the basket, are warm. "{d}I baked those yesterday." She puts the lid on. The lid lifts once by itself.
* Reward: 40 H$, Purple Muffin (item; feeding it to the pet gives horn nubs early). Later: Zoomzoom's festival joke is one word: "We."

**S112 "The Other Half of the Card"** | Charlotte (566, Hellvile) | Ch4 | deliver, cooking
* Hook: Charlotte, whose cookies are 90% charcoal and "love you", has a recipe card torn down the middle.
* Beats: 1) Carry the half to Nana. 2) Nana goes still `[STILL]` and tapes them together. 3) Cook both halves: melon bread with bone flour. 4) Carry the loaf to Charlotte. She eats it burnt. "Perfect."
* Comic: she burns the perfect loaf on purpose so it "tastes like home". NANA, arriving: "{big}CHARLOTTE.{/}" `[R8]` CHARLOTTE: "It's a texture."
* Heart: NANA: "I baked it wrong on purpose. / So I'd remember it was / missing something." `[NO BUTTON]`
* Reward: 80 H$, **Two-Sided Recipe Card** (decor), Bone Flour unlocked at the bakery. Later: the bakery sign gains a second line: "& Charlotte".

**S113 "Bake for a Thousand"** | Scorchmello (669, Hellvile) + Nana (1) | Ch5 | cooking, fetch, minigame
* Hook: the Unity Loaf for the first Mingle in eleven years: one loaf, a thousand slices, three bakers, one oven, no agreement about toast.
* Beats: 1) Fetch from both sides (Melon, Bone Flour, Charcoal, Marshmallows). 2) The cooking minigame at triple speed with three grannies shouting. 3) Scorchmello toasts the crust black; Nana scrapes it; Charlotte puts it back. 4) The loaf is cut on the Unity Stage.
* Comic: inside the loaf is a key `[POP]`. Everyone turns to Nana. "{d}Habit."
* Heart: Tallyworn hangs the key on the ruined gatepost, where there is no lock. "In case anyone's ever scared / again. {..}So they can see it's / open."
* Reward: 200 H$, **Unity Loaf** (furniture under glass; the pet sniffs it), "Three Bakers" trophy. Later: Grumbleguts asks about leftovers. There are none. He asks again.

#### Chain S: Hillvile Towers, apartment 7B (Hillvile; 3 parts; the second home)

**S114 "Apartment 7B"** | Keysie (21) | Ch2 | find, housing
* Hook: Keysie holds a spare key for an apartment nobody has rented in ten years. The last tenant "moved to the other side of the park". Rent: "whatever you've got".
* Beats: 1) Climb the Towers (the facade browser becomes a door). 2) 7B is furnished in dust; the windows face the Wall. 3) Clear it (remove mode) and find the tenant's things: a bone lamp, a photo of a mousekin with tally chalk. 4) "Keep what you find. They won't be back." 5) Decorate your second home.
* Comic: the dust sheet over the sofa is shaped like a person `[HOLD 13]`. Pull it: a sofa. `[R8]` The sheet, on the floor, is still shaped like a person. `[R13]` KEYSIE: "{d}It does that."
* Scare: tally chalk under the sill: 3,651 marks. The last one is **fresh**. `[MUTE 21]` (The tenant is Tallyworn; he crossed back for his family and nobody told anyone.)
* Reward: **Hillvile Towers 7B** (second home), Bone Lamp (furniture). Later: Ep 4.1, Tallyworn: "I had a flat on your side once. / Faced the Wall. {d}Bad view."

**S115 "Neighbours"** | Gusty (92) and Curlicue (44) | Ch2-3 | housing, talk, fetch
* Hook: the Towers' "cosiest floor" contest. 7A and 7C have never won. They want 7B as an ally against the ninth floor.
* Beats: 1) Cosy B in 7B. 2) Each neighbour lends an item to be placed "where I can see it from my door". 3) The judge is Mascotty: `[STILL]` in each doorway for R21, then a thumbs-up or nothing. 4) The ninth floor wins again: a single ghost with one candle.
* Comic: Mascotty judges 7B `[HOLD 21]` and, for the first time ever, nods **once**. Gusty faints. Curlicue: "She likes the rug."
* Heart: the ninth-floor ghost is Rattlecup (507), a Hellvile poltergeist who drifted over years ago and "didn't want to be a bother". She leaves biscuits. Nobody had gone up to say thank you.
* Reward: 60 H$, Rattlecup's Teacup (furniture; rattles at midnight, a biscuit a day). Later: Rattlecup is in the Ep 4.6 line: "I'm not a bother."

**S116 "Room With a View"** | Keysie (21) | Ch4-5 | housing, photo, talk
* Hook: with the Wall gone, 7B faces Hellvile Park. Keysie wants a photo "for the brochure" (there has never been a brochure).
* Beats: 1) Photograph the view at dawn. 2) Decorate 7B for a Hellvile guest: Tallyworn is coming to see his old flat. 3) He sits in the window for R26. 4) He asks to leave one thing: the chalk, on the sill.
* Comic: the brochure has one photo and one line: "Hillvile Towers: now with a view." `[R8]` "{d}It used to say 'now with a Wall'."
* Heart: TALLYWORN: "I'll leave the chalk. Someone / else might need to count / something." `[NO BUTTON]`
* Reward: 100 H$, **Tally Wall** (decor; the last mark a heart), Landlord trophy. Later: the brochure is a readable prop at Town Hall; its second printing has a second line: "and a chalk."

#### Chain E: Father Fuzz and the Mr. Huggins succession (Hillvile, then Sutura; 5 parts)

**S117 "Buckwheat Run"** | Father Fuzz (77) | Ch2 (after Ep 2.4) | fetch, talk
* Hook: stuffing for Mr. Huggins the 3rd. Love he has. Buckwheat is at the farm, and Farmer Bram (120) only talks to his carrots.
* Beats: 1) Compliment a carrot (three options; all work; the carrot blushes). 2) Fetch 2 Buckwheat. 3) A 20 s sewing minigame. 4) The 3rd sits up. Fuzz hugs him. His head falls off.
* Comic: `[SLAM]` the head rolls three times and stops facing the Wall, as the 2nd did. FUZZ: "{d}They all do that." `[R13]` "I've stopped asking."
* Heart: Fuzz keeps every destroyed Huggins in a drawer "because nobody is practice". He names them all.
* Reward: 30 H$, Mr. Huggins the 1st's Grave Marker (decor, tiny). Later: the drawer is visible in the church in Ch3 and the kids have drawn faces on the heads.

**S118 "Holy Water, Hold the Water"** | Father Fuzz (77) + Sr. Lumen (354) | Ch3 (before the beach nest) | fetch, exorcism (practice), timing
* Hook: the holy water ran out mid-invasion. Lumen's scrolls say any water will do "if blessed by someone who means it". Fuzz is not sure he means it anymore.
* Beats: 1) Fetch water from the bath house (saved: Steamy fills it; lost: her ghost does, and it comes out warm). 2) A blessing minigame: the prayer phase with the metronome muted; you keep the beat. 3) Test on Mr. Huggins the 4th: he survives two parts `[3x@15]`. 4) He loses an ear on part three. Fuzz counts it as a win.
* Comic: the blessed water giggles. FUZZ: "That's fine. {.}Is that fine?" LUMEN: "{d}The scroll says 'probably'."
* Heart: Fuzz, before part three: "Eleven years ago I let go too / early. {..}Hold the beat for me."
* Reward: Holy Water x3, 50 H$. Later: Ep 4.3, Fuzz keeps the beat alone.

**S201 "Buckwheat"** | Father Fuzz (77) | Ch3 (after the beach nest, in the church) | fetch, deliver  *(grafted: heart S85)*
* Hook: Fuzz, back in the church with the kids after the Factory, is leaking, and asks for "2 buckwheat, for a recipe".
* Beats: 1) Nana has two of everything; she gives you three. 2) Deliver. 3) He re-stuffs himself behind the altar. 4) `[R21]` he comes out rounder. "...Recipe went well." The kids hug him; a pin pings off `[TINK]`.
* Comic: NANA: "Tell him it's gluten-free. He / worries."
* Heart: he asks you not to tell the bishop, or Nox, or anyone, that faith needed a top-up. You do not. (The chain's release after S118's withheld one.)
* Reward: 40 H$, Buckwheat Sack (furniture). Later: the bell he rang for bedtime (Ep 3.3) now rings every night at 20:00 for the rest of Ch3.

**S119 "The Plush Surgeon"** | Sutura (542, Hellvile night nurse) | Ch4 | deliver, fetch, housing
* Hook: Sutura stitches up crying kids' plushies, free, at night. Xe has heard of "a priest who destroys one a week" and wants a word.
* Beats: 1) Deliver Fuzz's drawer of heads across the gate (heavy, rattling; the heads mutter). 2) Sutura mends them overnight (sewing minigame, night only). 3) Return with **Mr. Huggins the 1st through 8th, repaired**, in a row. 4) Fuzz cannot speak for R26.
* Comic: all eight heads turn to face the Wall at once `[!RIPPLE]`. The Wall is gone. They turn back, confused. `[R8]` SUTURA: "{d}Muscle memory."
* Heart: SUTURA: "Nobody's practice. {..}That's why / I don't charge." Fuzz has said the same thing for ten years to an empty church. `[HOLD 21]`
* Reward: **The Huggins Choir** (furniture: eight heads on a shelf; they hum SONG_CHURCH at night), 100 H$. Later: the church pews fill with Hellvile kids holding mended plushies.

**S120 "Mr. Huggins the 9th"** | Father Fuzz (77) | Ch5 | exorcism, talk
* Hook: one more practice, in public, at the festival, with Sutura's stitching and Rivet's rivets. "If this one survives, I'll believe it."
* Beats: 1) Gather a crowd (5 festival-goers; each has a reason not to come and comes). 2) The practice ritual, all three parts, real odds. 3) The 9th survives. 4) He hugs Fuzz `[HOLD 26]`.
* Comic: the crowd flinches at every part `[!RIPPLE]`. After part three, silence `[MUTE 21]`. Honkers honks. Everybody screams. Mr. Huggins is fine.
* Heart: FUZZ: "{d}Oh. {..}Oh, that's what it feels / like." LADY SMOG, front row: "Told you it was lovely." `[NO BUTTON]`
* Reward: **Mr. Huggins the 9th** (furniture; hugs the pet when it passes), "Nobody Is Practice" trophy, 150 H$. Later: the 1st's grave sign gains a second line: "Came back. Hugged a priest."

#### Chain F: Lilypad Lou and the two-way mail (cross-border; 3 parts)

**S121 "Return to Sender"** | Lilypad Lou (97) | Ch1-2 | find, deliver
* Hook: your mailbox keeps getting junk mail "from The Other Side". Lou wants it back: "Undeliverable mail is a personal failure."
* Beats: 1) Collect 5 letters over 3 days (the calendar). 2) Coupons for "Cloud Nine Vapor: opening soon!", a tax notice from Taxfang, a chain letter that says "we". 3) Lou tries to return them to the Wall; it has no slot. 4) She posts them through a crack. Something posts one back.
* Comic: the returned letter is addressed to "THE SNACK, Pink Door Cottage". LOU: "Well. It's a valid address."
* Scare: inside, a single tally mark. `[MUTE 14]`
* Reward: 30 H$, Cursed Coupon (decor). Later: Meterman (508) tries to redeem the coupon in Ch4; your mailbox gets a second tally mark the night after S114.

**S122 "Dead Letter Office"** | Lilypad Lou (97) | Ch3 | find, combat, deliver
* Hook: the post office is a nest. Inside: a sack of letters from Hillvile to Hellvile, ten years old, never sent. Lou didn't know it existed.
* Beats: 1) Clear the post office. 2) Carry the sack to the church. 3) Lou reads the addresses: Nana to Charlotte, Giggs to Nox (90 of those), Fuzz to Smog, Grampy to "the mouse who whistles". 4) She decides to deliver them. All of them. In Ch4.
* Comic: the demon guarding the sack is using it as a pillow and does not want to fight. It offers a letter as a bribe. Lou accepts the letter and the fight.
* Heart: one is in her own handwriting, to a frog in Hellvile called Warty (912). "...I was eleven." `[R13]` "{d}I'm not reading that one out."
* Reward: 60 H$, Mail Sack (decor). Later: Confetti's Frog Wedding (#44) has a backstory.

**S123 "First Post"** | Lilypad Lou (97) + Mandible (528, Hellvile) | Ch4 | deliver, talk, escort
* Hook: Lou delivers the sack. Mandible, who clacks his jaw at latecomers, runs Hellvile's post and considers the letters "ten years late".
* Beats: 1) Escort Lou; six scripted deliveries, each a tiny scene (Nox reads one of Giggs's 90 and puts the other 89 in a drawer, unread, "for later"). 2) Mandible clacks at every one. 3) Lou's own letter to Warty; he sings the reply to the moon.
* Comic: Mandible's clacks `[3x@15]` accelerate toward his counter, then stop dead when Lou stamps every letter "ON TIME". `[HOLD 13]` "{d}...Acceptable."
* Heart: Nox reads #1 of 90: "Dear Nox. I'm sorry. I'm sorry. I'm" and the page is wet. "He never finished one. {..}That's / why they never came." `[NO BUTTON]`
* Reward: 100 H$, **Two-Way Mailbox** (furniture upgrade: Hellvile mail and the news ticker), Mail Carrier badge. Later: a letter from Warty: a song lyric and a pickle.

#### Chain G: Grampy Bean and Sootkin (cross-border; 3 parts)

**S124 "Where Was I?"** | Grampy Bean (88) | Ch1-2 | find, talk, pet
* Hook: Grampy has lost his glasses, his trowel and "the thing I was about to say". The pet finds things.
* Beats: 1) @E sniffs out the glasses (in a tomato), the trowel (in the fountain, which giggles when the pet reaches in) and a seed packet with Hellvile writing. 2) He reads the packet and remembers "a little mouse-person who whistled" for exactly one line, then loses it.
* Comic: he puts the glasses on `[R5]` over his other glasses. "Better."
* Heart: he keeps the empty packet in his hat "in case I remember who to thank".
* Reward: 20 H$, Tomato Planter (furniture; a tomato every 2 days). Later: Ep 3.5's memory game uses the packet as its first clue.

**S125 "The Face Tomatoes"** | Grampy Bean (88) | Ch3 (after Ep 3.5, saved) | fetch, find
* Hook: the tomatoes still have eyes. He wants them picked "before they get opinions". Bloom wants three. The pet wants one.
* Beats: 1) Pick 9; each blinks when you look away `[R2]`. 2) Deliver 3 to Bloom. 3) Bury the rest by the Wall, facing Hellvile. 4) Overnight they sprout sunflowers.
* Comic: the pet eats one. `[HOLD 13]`. "beep." `[R8]` Its eyes are briefly tomatoes. `[R13]` `{d}...it's fine.`
* Scare: one tomato whispers `{w}thank you` in a voice that isn't Grampy's. He hears it and smiles. "Xe's still over there."
* Reward: 50 H$, Eye Tomato (decor; blinks). Later: the sunflowers along the Wall are the trail to Sootkin's in Ep 4.3.

**S126 "Two Gardens"** | Sootkin (525, Hellvile) + Grampy Bean (88) | Ch4-5 | fetch, housing (outdoor plot), talk
* Hook: Sootkin's sunflower is under a gate that is coming down. Grampy wants to move it to a bed straddling the old Wall line. Sootkin has never moved it. Xe is scared it won't survive.
* Beats: 1) Fetch Hillvile soil and Hellvile ash. 2) Build the shared bed (a 3x3 outdoor housing plot: 4 tomato plants, 1 sunflower). 3) Water it; the plush cloud (if `SF_VAPULA_PLUSH`) rains on request. 4) Wait one day. It lives.
* Comic: Grampy calls Sootkin a different name every time ("Sooty, Sultry, Toaster"). Sootkin, who has waited ten years to be thanked, answers to all of them.
* Heart: GRAMPY: "Thank you for the seeds." SOOTKIN: "{d}...You remembered." GRAMPY: "I remember everything. {..} / Where are my glasses?" They whistle together.
* Reward: **Shared Flower Bed** (outdoor furniture for the Park Plot), 100 H$, Green Thumbs trophy. Later: Bluebell (29) asks Sootkin for seeds; a tomato appears in your mailbox with a sunflower seed stuck to it.

#### Chain H: Steamy's bath house (Hillvile; 4 parts; branches on `SF_STEAMY_LOST`)

**S127 "Every Duck Has a Name"** | Steamy (2) | Ch1-2 | find, talk
* Hook: one duck Steamy didn't name and doesn't remember buying (Ep 1.4). She wants it gone and cannot throw out a duck.
* Beats: 1) Three suspects (Squeaker, Tidepool, Pip). None. 2) The duck squeaks at the Wall at night. 3) Carry it to the Wall; something squeaks back, 3 then 1. 4) Steamy keeps it and names it "Guest".
* Comic: the line-up: each suspect squeaks their own duck as an alibi `[3x@15]`. Pip's is Mr. Flops. "He's a duck today."
* Scare: when you set Guest on the Wall the fog giggles **in a frog's voice**. STEAMY: "{d}...That one's not mine either."
* Reward: 30 H$, Rubber Duck "Guest" (decor; squeaks at the Wall). Later: Guest is the duck in Steamy's hand in Ep 2.4.

**S200 "Night Shift at the Bath-House"** | Lather (41) | Ch2 (after Ep 2.4, night) | find  *(grafted: heart S67)*
* Hook: the bath house is closed since Steamy was taken; Lather left soap inside; "just grab it, don't look at the water."
* Beats: 1) Enter at night; **no music for the whole quest**. 2) The ducks in the main tub all face the door `[HOLD 21]`. 3) Find the soap. 4) The water giggles once, pitched down 15 semitones. 5) Leaving: a duck is on the step outside, facing the Wall.
* Scare: `[NO GAG]` the whole quest; the only sounds are drips at `[R3]` and `[R8]`.
* Heart: none spoken. Later (the next day): Steamy (saved) or Lather (lost) has lined the ducks up facing away from the Wall. That is the button.
* Reward: 30 H$, Lavender Soap, **Facing Duck** (furniture; it faces whatever faced the Wall last).

**S128 (saved) "Grand Re-Opening" / (lost) "Steam Rises"** | Steamy (2) or her ghost | Ch3 | fetch, cooking (soap), housing (her bath house)
* Hook: (saved) Steamy wants the bath house open for the people in the church: "Clean is a kind of brave." (lost) her ghost wants the same, from the other side of the water.
* Beats: 1) Fetch 3 Soap (#13 ties in). 2) Redecorate the bath house (housing in her home): towels, ducks, a "NO DEMONS" sign Rivet welds. 3) First bath: the daycare kids. 4) (saved) Steamy sings. (lost) The water warms itself; the ducks line up on their own.
* Comic (both): Mascotty arrives for a bath and gets in **with the head on** `[HOLD 21]`. The head floats. Mascotty does not. `[R13]` Everyone stares at the surface. She surfaces. Head back on. Nobody says anything.
* Heart (saved): Steamy, washing a kid's hair: "There was someone in my head / who hadn't been warm in ten / years. I keep it hotter now." (lost): the ghost writes in the steam on the mirror: "WARM ENOUGH?" You answer A: yes. `[NO BUTTON]`
* Reward: **Steamy's Tub** (furniture; the pet bathes in it), 60 H$. Later: Ep 3.3's bathtub of warm dead is explained.

**S129 "Mixed Bathing"** | Steamy (2) or her ghost + Scaldie (513, Hellvile) | Ch4-5 | talk, fetch, concert
* Hook: the first cross-border bath night. Scaldie wants it hotter. Steamy wants "warm". The ducks are neutral.
* Beats: 1) A diplomatic snack exchange (Sulfur Soda for Scaldie, Cotton Candy for Steamy). 2) A temperature minigame: one slider, two hands (yours and the pet's). 3) The bath night: 12 citizens, 6 a side; Warty sings; Riffmaw tunes the church bells by accident. 4) Guest the duck swims to the middle.
* Comic: Scaldie tests the water with a finger and screams "{big}LUKEWARM!{/}" `[SHK_POW]`. `[R26]` "{d}...I'll allow it."
* Heart (saved): STEAMY to Tallyworn, in the pool: "You were in my head. You were / so tired." TALLYWORN: "You were so warm." (lost): the ghost's steam-writing on the big window: "EVERYONE'S HERE."
* Reward: 120 H$, Guest gains a tiny Hellvile hat, the "Mixed Bathing" Friday event. Later: Scaldie opens a sauna in the back room. It is on fire. It's fine.

#### Chain I: Sprinklepop's lost soul and Wheezle (cross-border; 4 parts)

**S130 "Party Supplies"** | Sprinklepop (81) | Ch1-2 (before Ep 2.4) | fetch, minigame
* Hook: the lollipop artist who sneezes glitter needs 20 balloons for "a party I haven't been invited to yet". Twisty (43) wants to make a balloon *you* first.
* Beats: 1) Win the Fun Fair balloon minigame. 2) Deliver 20. 3) She sneezes: glitter, glitter, `[3x@15]` **purple** glitter. 4) "Doctors say it's fine!"
* Comic: the balloon-you has the wrong number of ears. TWISTY: "It's an interpretation."
* Scare: the third sneeze is R8 late and in a lower voice. She doesn't notice. The balloons all turn to face the Wall.
* Reward: 20 H$, Balloon-You (decor; deflates over the chapter; any party re-inflates it). Later: she is first in line at Cloud Nine.

**S131 "Lost and Found"** | Dr. Bloom (111) | Ch2-3 (after Ep 2.5) | find, photo, pet
* Hook: Sprinklepop's soul wandered off with its map upside down. Souls "go exactly as far as they're scared to".
* Beats: 1) Follow a glitter trail (sparkle spots): the lollipop stand, the Fun Fair, the daycare. 2) Each stop: someone saw "a bouncy see-through bunny reading a map wrong". 3) Photograph the soul at the Wall, waving. 4) The pet sniffs the crack. "beep." It went through.
* Comic: the soul is **still having a party**: tiny translucent balloons at each stop; a translucent cake at the Fun Fair. TWISTY: "{d}She ordered that."
* Heart: at the Wall the soul turns the map the right way up, looks at Hillvile, looks at Hellvile, and chooses Hellvile. BLOOM, later: "She went toward the party she / wasn't invited to. {..}That's not / lost. That's brave."
* Reward: 50 H$, Translucent Balloon (decor). Later: Ep 4.2, Wheezle names his ghost "Sprinkles".

**S132 "Wheezle's Little Ghosts"** | Wheezle (707, Hellvile) | Ch4 | talk, find, timing
* Hook: Wheezle coughs out little ghosts and names them. One is bigger, pink, and keeps throwing parties. He calls her Sprinkles. He thinks she's his.
* Beats: 1) Meet the ghosts (Tuesday through Sunday; the pink one is Sprinkles). 2) Timing minigame: Sprinkles sneezes glitter on the beat; catch each sneeze in a jar. 3) Explain to Wheezle, gently (three options; all gentle). 4) "...She's not mine. {..}But she can / visit?"
* Comic: the ghosts introduce themselves `[3x@15]` "Tuesday." "Wednesday." "Thursday." `[R13]` "{d}...Also Thursday." WHEEZLE: "I had a long week."
* Heart: the ghosts are his swallowed laughs (laughing was against the law); Sprinkles taught them to throw parties.
* Reward: 80 H$, **Jar of Sneezes** (furniture; glows pink). Later: the little ghosts turn up at your Hell Pits flat for a party you didn't throw.

**S133 "Bubbles Bounces Back"** | Sprinklepop's soul (81) + Dr. Bloom (111) | Ch4-5 | escort, exorcism (reverse), concert
* Hook: a soul going *home* is an exorcism run backwards. Fuzz has never done one. The scroll says "probably".
* Beats: 1) Escort the soul from Hellvile to the clinic (it bounces ahead; keep it in view or it starts a party). 2) The reverse ritual: the metronome in reverse; the bell un-rings (W11, now funny). 3) The true-name phase asks for *her* name; you type SPRINKLEPOP from memory. 4) She wakes. Pink glitter.
* Comic: the stamp reads **"EXORCISM UNFAILED x1"** `[SLAM]` with the kazoo played backwards (a rising "wahhh wah wah").
* Heart: SPRINKLEPOP: "I had the best dream. Everyone / was invited." `[R13]` "{d}...Was I?" The whole clinic: "{big}Yes.{/}"
* Reward: 150 H$, **Heart Balloon** (furniture), "Unfailed" trophy, the Mixed Party event. Later: Ep 4.7, she bounces in with the balloon.

#### Chain J: Mascotty and Thumpkin (cross-border; 4 parts)

**S134 "Ignore That One"** | Hugsley (20) + Hootenanny (26) | Ch1-2 | find, photo, talk  *(absorbs S141)*
* Hook: Hugsley, the plush mascot hugged 1,000 times a day, is worried about Mascotty, the bunny mascot hugged zero times who never takes the head off. Hootenanny the night-watch owl has seen "a face" in a Towers window every night that never moves.
* Beats: 1) Find Mascotty (always somewhere, facing the Wall). 2) Try three things: wave (nothing), hug (she lets you; she doesn't move), photo (through the mouth-mesh, two tired eyes). 3) Stand still for R21; she waves, slowly. 4) At night, Hootenanny's window: photograph it: Mascotty's head, 9th floor, dark. Hootenanny hoots at it `[3x@15]`; on the third the head tilts 0.14 rad with a creak; he falls off the roof (he is an owl; fine). 5) The next night that window is empty and the one next to it isn't. Report to Hugsley: "It waved?!"
* Comic: Hugsley tries to copy the slow wave and can't go slowly `[PANIC Hugsley]`. "{d}I'm a hugger, not a waver."
* Scare: the thumping from the Wall (W6) is answered by Mascotty, thumping the ground with one foot: 3, then 1. `[MUTE 21]`
* Reward: 30 H$, Mascotty Postcard (decor; the eyes follow you), Hoedown Fiddle (decor). Later: she is at your window in Ep 1.5; S141's window is Rattlecup's (S115).

**S135 "Three Thumps and One"** | Prof. Fizz (69) | Ch2-3 | find, timing, fetch  *(absorbs S160)*
* Hook: Fizz's vapor detector (#15) "beeps at everything", but it also records the Wall thumps, and they have a pattern. Three nights.
* Beats: 1) Three nights at the Wall with the detector (the calendar); on the way it beeps at 20 things (a scavenger hunt with gag captions), not at Mascotty, not at the Wall, and at **you** if `SF_VAPE_SAMPLE`, otherwise at the pet. 2) Timing minigame: press A on each thump; 3-1, then 3-1-2, then 3-1-2-1 ("are you there / yes / still / yes"). 3) Decode with Fizz: bunny-thump for "sister". 4) Mascotty, nearby, thumps "yes".
* Comic: the 20 beeps accelerate `[3x]` into a continuous tone near Cloud Nine. FIZZ: "That's not a reading. That's a / scream."
* Scare: Mascotty: no beep. FIZZ: "Either she's the only clean / thing in town, or she's not / here." `[HOLD 13]`
* Heart: FIZZ: "Somebody's been asking the same / question for ten years. {..}And / somebody's been answering."
* Reward: 50 H$, Vapor Detector (decor: beeps at the pet). Later: Ep 4.2, Thumpkin thumps the pipe "for my sister"; in Ep 4.6 the detector in your house is silent for the first time.

**S204 "Cocoa Through the Boards"** | Lumina (76) | Ch3 (night) | deliver  *(grafted: heart S82; target: Mascotty's boarded window)*
* Hook: Lumina lights every lamp still standing; the lamp behind Mascotty's boarded Towers window is dark. She sends cocoa.
* Beats: 1) Carry the cocoa to the boards. 2) Push it through the gap. 3) `[HOLD 26]` `[MUTE 26]`; a hand takes it. 4) `[R21]` the cup comes back out, empty, **washed**.
* Scare: `[NO GAG]`.
* Heart: none spoken. Later (next morning): "!" over the boards, then inside the "!" a smiley drawn with the head's mouth-mesh pattern. That is the button.
* Reward: 30 H$, **Washed Cup** (furniture).

**S136 "The Head Comes Off"** | Thumpkin (894, Hellvile) | Ch4-5 | escort, talk, housing
* Hook: Thumpkin, the "killer bunny" who thumps the ground until the neighbours move, has thumped every night for ten years so his sister would know he was still there. He wants to walk her home. He is terrified she'll keep the head on.
* Beats: 1) Escort him to Hillvile; he thumps at every lamp post (nerves). 2) Mascotty at the fountain `[STILL]`. 3) `[HOLD 26]`. The head comes off (if Ep 4.6 hasn't happened yet, it comes off here instead). 4) He thumps once; she thumps back. 5) Decorate a room for her in the Hell Pits (housing in someone else's home): one request, a window that faces *away* from where the Wall was.
* Comic: his thumps shake the fountain `[SHK_TAP]`; it giggles on each one, on time; he thumps faster to make it laugh more. THE MAYOR, from a distance: "He's good."
* Heart: Mascotty's first words in the game: "{d}I didn't know what you'd look / like after ten years." THUMPKIN: "Taller." MASCOTTY: "...No." `[NO BUTTON]`
* Reward: 150 H$, **The Mascot Head** (furniture; she gives it to you; the eyes don't follow you anymore), "Nobody Knows Who" trophy. Later: Hugsley gets hugged 1,001 times. The extra one is Mascotty.

#### Chain K: Vex and Haze, Cloud Nine to Fresh Air (4 parts)

**S137 "Employee of the Month"** | Haze (709), at the Cloud Nine counter | Ch2 | talk, photo, find
* Hook: Haze wants a photo of the "Employee of the Month" frame "for evidence". Vex says it's "a motivational poster".
* Beats: 1) Photograph the frame (the eyes move during the shutter; the photo has them looking at the camera). 2) Find the shop's three plants Haze waters secretly; one is a tomato with an eye. 3) Haze gives you a plain-water pen "for when the grape runs out".
* Comic: Vex catches you `[STILL Vex, smile]` `[HOLD 21]`: "He's very motivational." `[R8]` "{d}Please don't tell him I said / 'he'."
* Scare: developed later at Shutterbug's, the photo shows the frame **empty**. `[MUTE 14]`
* Reward: 30 H$, Plain Water Pen (item; cancels one grape effect); the frame appears on your mailbox the next day, uninvited (decor). Later: Ep 2.7, it falls off the wall with a tink.

**S138 "Closet Space"** | Vex (666) | Ch3 (after Ep 3.1) | housing, talk, fetch
* Hook: during the invasion Vex is hiding in **your closet**. Since the eruption. He would like a cushion.
* Beats: 1) Discover him (housing: the closet is "placed by: ???" with a cushion request). 2) Fetch him Cotton Candy. 3) Three conversations through the door, one per siege wave: Cloud Nine, then Vapula, then why ("he listened"). 4) Let him out, or don't (he stays until Ch4 either way).
* Comic: every time a window breaks: "{d}not a customer" from the closet. After wave 3: "...I'll do a survey."
* Heart: VEX: "Nobody over there laughs. It's / the law. He said if I opened / a shop here, he'd listen." `[R21]` "He did. He's the only one who / ever did." `[NO BUTTON]`
* Reward: Vex's Cushion (furniture), 40 H$. Later: in Ep 3.7, when the fog comes down the chimney, a voice from the closet says "{d}not a customer" and the fog, for one frame, hesitates.

**S139 "Exit Interview"** | Vex (666) + Haze (709) | Ch4 | talk, fetch, housing (the shop)
* Hook: Cloud Nine is closing; Haze is taking over; Vex wants a reference letter; Haze wants the smoke out of the curtains.
* Beats: 1) Fetch 3 Soap; air out the shop (a housing clean-up: remove the purple props). 2) Redecorate as the **Fresh Air Lounge**: plants, open windows, the frame replaced with "Employee of the Month: Vex" (Haze's one act of mercy). 3) The reference letter: three options. 4) Vex tries plain-water vapes. "They're terrible." He has another.
* Comic: Haze's rule: "if it's purple, it goes". The pet is purple right now (S125). The pet goes outside. "beep."
* Heart: HAZE: "Ten years I watered plants in / that smoke because somebody / had to. {..}I think I was / practising for this." (one of the three allowed uses). Sets `SF_VEX_REFORMED`.
* Reward: 100 H$, **Fresh Air Lounge Neon**, **Employee of the Month: Vex** (decor). Later: Vex on the Unity Stage, "plain water vapes now".

**S140 "The Bubble Machine"** | Vex (666) | Ch5 | weld, minigame, concert
* Hook: Vex wants to be useful at the festival. He has a vape rig and no vapor. Rivet suggests soap.
* Beats: 1) Weld the rig into a bubble machine ("if it's purple I'm leaving"). 2) Test it at the Fun Fair (a bubble-popping minigame with the pet). 3) Run it at the Unity Stage; bubbles sync to the beat (a bubble layer on the concert scene).
* Comic: the first bubble is grape. `[FREEZE ALL]` `[HOLD 13]` VEX: "{d}Residue." `[R8]` It pops. Everyone is fine. Everyone checks anyway.
* Heart: Vex, watching 1,000 citizens in bubbles: "They're laughing. {..}Both sides." HAZE: "Yeah. That's what it sounds / like."
* Reward: **Bubble Machine** (furniture; blows bubbles when music plays), "Terrible, Actually" trophy, 100 H$. Later (ticker 1 of 4): "Vex, Fresh Air Lounge employee of the month for the 3rd month running."

#### Chain M: Dr. Bloom and Graveley (cross-border; 3 parts)

**S151 "Cold Hands"** | Dr. Bloom (111) | Ch1-2 | fetch, talk
* Hook: Bloom keeps her hands in warm water between patients "so nobody notices". Steamy has started asking questions.
* Beats: 1) Fetch a Hot Water Bottle (Lather's shop). 2) Deliver. 3) A conversation in three parts, one per visit: "it's a condition", "it's a family thing", "{d}...I'm a ghoul."
* Comic: she warms her hands and touches Zoomzoom, who speedruns out of the clinic in under four seconds anyway. "That's not the hands. That's / him."
* Heart: "Everyone here is lovely about / everything. {..}I never found out / if they'd be lovely about this." `[NO BUTTON]`
* Reward: 30 H$, Hot Water Bottle (decor; the pet sleeps on it). Later: Ep 3.4, she says it out loud.

**S152 "Night Clinic"** | Dr. Bloom (111) | Ch3 | escort, combat, fetch
* Hook: six citizens outside the church need medicine. Bloom will go herself; she'd like company "for the cold".
* Beats: 1) Escort her through the plaza at night. 2) Three house calls (Oma Fluff has lost her glasses again; Snugglesby needs a hug and gives one; Lagspike is streaming the invasion to her grandma). 3) Demons at the third; Bloom holds the lantern while you fight; her hands don't shake. 4) Back at the church the Mayor sees her clearly for the first time: a ghoul in pastel scrubs.
* Comic: Lagspike's chat (her grandma): "are those demons behind you sweetie" `[R8]` "yes grandma" `[R8]` "ok be safe xx".
* Heart: MAYOR: "Dr. Bloom. You're..." BLOOM: "{d}Cold-handed. Yes." MAYOR: "{..}...You've been here the whole / time." BLOOM: "Ten years." `[HOLD 21]` MAYOR: "Thank you."
* Reward: Medicine x3, 60 H$, Bloom's Lantern (furniture). Later: the church kids call her "Dr. Cold Hands" and mean it kindly; the lantern in your house is lit every night after Ep 3.4.

**S153 "A Clinic With Two Doors"** | Graveley (730, Hellvile) | Ch4-5 | housing, fetch, talk
* Hook: Graveley has an empty crypt on the old Wall line. Bloom wants a clinic with a door on each side.
* Beats: 1) Fetch 4 Scrap and 2 Medicine. 2) Rivet welds the second door. 3) Decorate the clinic (someone else's building: a bed each side, one lamp in the middle). 4) First patient: Grumbleguts, indigestion. Second: the pet, horn-ache. Third: Fuzz, who needs nothing and sits down anyway.
* Comic: Graveley and Bloom have the same cold hands and shake on it `[HOLD 13]`. Both: "Ooh." `[R8]` Both: "Sorry."
* Heart: GRAVELEY: "You were on the wrong side when / it closed." BLOOM: "There wasn't a wrong side. / {..}There was just a wall." They sit on the step, one foot each side of the old line. `[NO BUTTON]`
* Reward: 120 H$, **Two-Door Clinic** (a place; Rest & heal from either side), Bloom's Scrubs (decor), "Cold Hands, Warm Heart" trophy. Later: the clinic's sign-in sheet (a readable prop) has one name scared of the doctor: nobody; one scared of the forms: Grumbleguts.

#### Chain N: Hattie Haha and the Hellvile comedy scene (cross-border; 3 parts)

**S154 "Open Mic"** | Hattie Haha (71) | Ch1-2 | timing, talk
* Hook: the open mic needs a warm-up act. You have no act. "Timing is the act."
* Beats: 1) Three jokes; A on the punchline beat. 2) Perfect presses: a snort-laugh ripple; late: a cricket. 3) Hattie's closer about the Mayor. 4) Mascotty, back row, doesn't laugh. HATTIE: "Tough room."
* Comic: your third punchline is "...Hee. Hee." Everyone laughs. The Mayor, front row, laughs loudest, then stops `[STILL Mayor]` `[R13]` "{d}Good one."
* Heart: HATTIE: "Everybody here laughs at / everything. Do you know how / hard that makes it to tell / if you're funny?" `[R8]` "That one was a joke. Mostly."
* Reward: 20 H$, Mic Stand (furniture), the Open Mic activity. Later: Ep 3.5, she heckles you with your own timing.

**S155 "No Punchlines"** | Hattie Haha (71) (saved) or her ghost | Ch3-4 (after Ep 3.5) | timing, talk, find
* Hook: she can't finish a joke; she laughs a beat early at the punchline. Relearn with you, "from the setup".
* Beats: 1) Three sessions, one per day: the timing minigame with her laugh as the metronome, drifting. 2) Find her old notebook (in the burned cafe) of jokes never told "because they were about the Wall". 3) Session three: she tells one. On time. 4) The cafe owner laughs so hard the jukebox skips to the right song.
* Comic: session one: "Knock knock." YOU: "Who's there?" HATTIE `[R2]`: "{d}HA. {..}No. Wait."
* Heart: the untold joke: "Why did the Hillviler cross the / park? {..}{..}She didn't. That's the / joke. For ten years that's been / the joke." Nobody laughs. Then Haze, who crossed with you, snorts. HATTIE: "Finally. A room."
* Reward: 60 H$, Hattie's Notebook (decor; 12 readable jokes). Later: Ep 4.7's set opens with the refrain; the notebook in your house gains a thirteenth page: "Because we're STANDING in it."

**S156 "The Hellvile Set"** | Honkers (501, Hellvile) + Hattie (71) | Ch4-5 | concert, timing, talk
* Hook: Honkers honks his nose at funerals and is never invited back. He has never been allowed to be funny on purpose. He wants a double act. She wants to know if he can do timing. He cannot.
* Beats: 1) Rehearsal as a duet; Honkers honks R2 early every time. 2) Hattie's fix: write his honks into the act as the metronome. 3) The set at the comedy tent (a concert variant with laugh tracks on both sides). 4) Mr. Solemn (555), who hasn't smiled in 200 years, in the front row.
* Comic: he honks on the wrong beat the entire set and, because she wrote it that way, every honk is on time. `[3x@15]`: honk, honk, `[HOLD 13]` nothing. The silence is the punchline. Mr. Solemn's mouth moves 1 px.
* Heart: HATTIE, after: "It wasn't that you weren't / funny over there. {..}It was the / law." HONKERS: "{d}I honked anyway." HATTIE: "I know. I heard you. Through / the Wall."
* Reward: 150 H$, **Honkers's Nose** (furniture; honks when the pet passes), "Tough Room" trophy, the comedy tent as a repeatable activity. Later (ticker 2 of 4): "Mr. Solemn seen smiling. Denies it."

#### Chain O: Miss Lulla's daycare and the Hellvile kids (cross-border; 3 parts)

**S157 "Naptime Monsters"** | Miss Lulla (36) | Ch1-2 | talk, housing (the daycare), pet
* Hook: the kids won't nap: there's a monster under the cots. Lulla says monsters aren't real and wants you to check so she doesn't have to.
* Beats: 1) Housing in the daycare: rearrange the cots so none face the Wall-side window. 2) Check under each `[3x@15]`: a sock, a crayon, `[R13]` a drawing of a crowned cloud with a speech bubble: "we". 3) The pet naps under the third cot and the kids nap too.
* Comic: Lulla gets any baby to nap in under a minute. She tries it on you. It works. You wake 30 game-minutes later with a blanket and a gold star.
* Scare: the drawing under the third blanket is **wet**. Nobody has been near it. `[MUTE 14]` LULLA: "{d}Juice."
* Reward: 20 H$, Gold Star (decor), the pet learns to nap on command. Later: W12's drawings are explained in S158.

**S158 "The Same Friend"** | Miss Lulla (36) | Ch2-3 | find, talk, dream
* Hook: every kid drew the same purple friend with a crown (W12). Who has been telling them stories? It isn't Story Owl.
* Beats: 1) Interview 4 kids (Pip, Twinkly, Wobbles, Homework who "isn't a kid" and drew it anyway). 2) Each describes a dream: a cloud that "listens". 3) Peek at a Dream: your own dream has the cloud; it says `{w}nobody ever listens to ME`. 4) Lulla says, for the first time: "Monsters are real. {..}Some of / them are lonely."
* Comic: Homework (40) drew "a normal cloud with a hat". LULLA: "{d}The hat has points." HOMEWORK: "It's a fashion cloud."
* Heart: Lulla pins every drawing in the church in Ch3: a wall of crowned clouds. The Mayor sees it and sits down. `[NO BUTTON]`
* Reward: 40 H$, Crowned Cloud Drawing (decor), Dream "The Listener". Later: D2's floating letters are in the kids' hands.

**S159 "Field Trip"** | Miss Lulla (36) + Tantrum (530), Pokefork (540) + Dozy Doe (34), Batty Betty (920) | Ch4-5 | escort, minigame, housing  *(absorbs heart S125)*
* Hook: the first joint nap time: Hillvile kids to Hellvile, Hellvile kids to Hillvile, one pink bus. Lulla has never lost a child.
* Beats: 1) Escort the bus (a slow drive; count heads at each stop; Pokefork hides every time). 2) Nap mats in Nox's chapel (housing). 3) The Hellvile kids won't sleep without a monster under the mat. Grumbleguts volunteers, lies under a mat, falls asleep first. 4) The nap-off: Dozy Doe (200 citizens trained) vs Batty Betty (naps upside down, "yoga"): a breathe-on-the-beat rhythm minigame. 5) Everyone naps: 24 kids, one gatekeeper, under a minute.
* Comic: head count `[3x@15]`: 12, 11, `[R13]` 13. The 13th is the pet in a tiny bonnet. The belly snores in a different key from the rest of him.
* Heart: LULLA, watching: "Monsters are real. They also / nap." She writes it on the daycare wall when she gets home.
* Reward: 100 H$, Nap Mat (furniture; the pet uses it), "Under a Minute" trophy. Later: Grumbleguts asks if there's a snack after nap time. There is. Lulla planned for it.

#### Chain P: Prof. Fizz, Dr. Quark and Dr. Frankly (cross-border; 3 parts)

**S142 "The Fountain Is Late"** | Prof. Fizz (69) | Ch1 | timing, photo
* Hook: Fizz has timed the fountain's giggle for a week. It is 267 ms late, exactly (R8). "Fountains aren't late. Fountains are plumbing."
* Beats: 1) Press A when the fountain giggles, 10 times; your presses are plotted as a histogram (all R8 late). 2) Photograph the fountain's two faces. 3) They are **not facing each other** anymore; someone turned one. 4) He writes it down and underlines it three times.
* Comic: his stopwatch also beeps at the pet. "Everything is 267 milliseconds / late today." `[R8]` "...Except that."
* Seed: "A two-faced fountain only / giggles if someone laughs on / the other side. {..}So who's / laughing?"
* Reward: 20 H$, Fizz's Histogram (decor). Later: Ep 3.4, Rivet: "It was a TWO-WAY fountain!"

**S161 "Tiny Things"** | Dr. Quark (24) | Ch3 | find, photo, combat
* Hook: Quark has found that the demons are made of "very small sighs". Samples from three nests and a photo of one mid-sigh.
* Beats: 1) Collect 3 Vapor Samples from cleared nests (a new drop). 2) Photograph a demon at the frame it dissipates (a photo verb in combat). 3) Quark's analysis: "Each one is somebody's. {..}I / can't tell whose."
* Comic: under the microscope the sigh is a tiny "it's fine." in a tiny font. `[R8]` QUARK: "{d}Oh no. It's ours."
* Heart: she has studied tiny things for forty years because "big things are frightening". This is the first frightening tiny thing. She keeps studying it.
* Reward: 60 H$, Microscope (furniture; the pet looks in it). Later: Ep 3.4, her note is on the fountain's cracked rim.

**S162 "They All Want to Be Florists"** | Dr. Frankly (556, Hellvile) + Prof. Fizz (69) | Ch4-5 | fetch, creator, housing
* Hook: Frankly builds monsters. They all want to be florists. Bluebell has one apprentice (Morguelet, #33) and room for one more. Fizz thinks the monsters are "a calibration opportunity".
* Beats: 1) Meet the monsters (3, stitched, polite). 2) Creator: give one a florist's look (apron, flower). 3) Build it a 2x2 stall in the park (housing). 4) First customer: Grumbleguts, buying flowers for the Gate.
* Comic: Igorina (503) pulls the big lever every time Frankly says "alive". He says it a lot. Thunder every time. Fizz's detector beeps at the thunder.
* Heart: the monster's first bouquet is for Sootkin's bed (S126). It arranges it badly. SOOTKIN: "{d}It's perfect." It is not. It is perfect.
* Reward: 100 H$, **Florist Monster** (a stall; sells flowers), Creator: Stitched parts. Later: the Florist Monster is in the credits parade with Bluebell.

#### Chain Q: Tallyworn and Tock (cross-border; 3 parts; continues quest #38)

**S163 "A Clock With No Hands"** | Tock (104) | Ch2-3 | fetch, find  *(absorbs heart S77)*
* Hook: Tock fixes every clock and naps through the chimes. One clock in Town Hall has no hands and ticks anyway. He wants to know why before he fixes it.
* Beats: 1) Find it: in the Mayor's office, behind the drawing (S108). 2) It ticks 3,650 times an hour, exactly. 3) Fetch Tock a Milk while he listens. 4) At midnight (night visit, no music): twelve chimes, descending, a thirteenth `[HOLD 26]`; the cuckoo comes out and does not go back in; the thirteenth chime is the fountain's giggle. 5) He decides not to fix it. "It's counting something. {..}Let / it."
* Comic: Tock naps through his own explanation. You wake him. "...where was I. Ticking. Yes."
* Scare: at the thirteenth tick the whole office shifts one cell to the left `[SHK_TAP]` `[MUTE 14]`. Next morning his pad reads "3,651", with 3,652 crossed out. "Counting DOWN now. Don't know / to what."
* Reward: 30 H$, Handless Clock (furniture; ticks), Cuckoo (out) (decor). Later: Ep 4.1, Tallyworn: "Day 3,661."

**S164 "Day 3,653"** | Tallyworn (712, Hellvile) | Ch4 | talk, fetch, housing
* Hook: Tallyworn has filled his tally wall. He needs a second wall, or a reason to stop. He is not ready to stop. He asks for chalk.
* Beats: 1) Fetch chalk from Clinker (753), who uses it for poems and does not share: trade a poem (a 3-line text entry; she rates it). 2) Choose a new wall (housing: a wall panel in his pump-yard shed). 3) He makes the first mark; his hand stops. 4) "Maybe tomorrow I'll know what / I'm counting."
* Comic: Clinker rates your poem: "Six. {d}Needs death." You add death. "Four. Too much death."
* Heart: the new wall's first mark is a heart, drawn by accident when his hand shook. He leaves it. `[NO BUTTON]`
* Reward: 50 H$, Chalk (item), Clinker's Poem (decor). Later: #38 "Counting Together" opens with Tock already knowing about the handless clock.

**S165 "Counting Down"** | Tock (104) + Tallyworn (712) | Ch5 | weld, fetch, housing
* Hook: Tock wants to give the handless clock hands and set it to count **down** to the next Mingle. Tallyworn wants to be the one who turns it.
* Beats: 1) Weld the hands (Rivet: "clock hands don't need to be structural." They are structural). 2) Carry the clock to the ruined gatepost (where S113's key hangs). 3) Tallyworn sets it: 365 days. 4) One last tally mark on the gatepost, under the key, and he stops.
* Comic: Tock naps through the unveiling. The clock chimes. He doesn't wake. The whole crowd whispers so as not to wake him `[DUCK .3 60]`.
* Heart: TALLYWORN: "Three thousand, six hundred and / sixty-five days counting up. / {..}Day one counting down." He pockets the chalk. "My eyes are just doing a rain."
* Reward: **Countdown Clock** (furniture copy), 150 H$, "Day One" trophy. Later: on the next Mingle (calendar event) the clock chimes and Tock wakes up for it.

#### Chain R: Wisp, the haunting (your house, then Hellvile; 5 parts)

**S166 "Placed By: ???"** | (no giver; triggered by the house catalogue in Ch2) | Ch2 | housing, find  *(absorbs Creakster's uncued creak)*
* Hook: a tiny imp-sized bed you never placed is in the corner (Ep 2.3). The catalogue lists it "placed by: ???". You cannot sell it. You can try.
* Beats: 1) Remove it (it comes back by morning, one cell closer to your bed each night). 2) Place a lamp next to it as a test. 3) By morning the lamp is on, turned toward the bed as a night-light. 4) Leave a note (text entry); by morning there's a drawing on it: a house with two windows.
* Comic: the third night the little bed has a tiny "DO NOT SELL" sign made from your shopping list. The handwriting is terrible. The spelling is worse. `[R8]` "beep."
* Scare: night two, in decorate mode, the cursor moves on its own for exactly 3 frames `[INSERT 3]` and places the lamp. `[MUTE 14]`. Then, with nobody on it, **a floorboard creaks**, once, on no beat at all `[HOLD 13]`. (Creakster, 548, practising from under the Wall; S166 is the only time it is uncued.) `[NO BUTTON]`
* Reward: Wisp's Drawing #1 (decor). Later: Ep 3.2, "not in MY room."

**S167 "Night Light"** | Wisp (563), visible from Ep 3.2 | Ch3 | housing, fetch, talk
* Hook: Wisp is scared of the red sky. Not the demons; the sky. They want "one window that's always daytime".
* Beats: 1) Fetch Lumina's lamp oil and a Candle. 2) Rivet welds a window box; you paint a sky on it (a 4-colour picker). 3) Place it over a boarded window. 4) Wisp sits under it during the waves and gets slightly more solid.
* Comic: the painted sky has one cloud. WISP: "{d}no crown." YOU: "No crown." Wisp adds a tiny sun with a face. It is the Mayor's face. "he looks worried." It does.
* Heart: WISP: "at home i cried at the Wall / till i fell out. {..}i didn't know / it was MY sky i was scared of." `[R21]` "this one's better. yours has a / sun."
* Reward: **Daytime Window** (furniture; always day), 40 H$. Later: Ep 4.4, Wisp's real room gets one too.

**S205 "Taller"** | Wisp (563), in a body | Ch4 (after Ep 4.4) | housing, talk  *(grafted: heart S99)*
* Hook: Wisp wants to be measured against **your** door frame "because ours has Mum's marks from before and I don't want to go past them yet".
* Beats: 1) Housing: the door frame becomes a height board. 2) Wisp stands against it `[HOLD 13]`. 3) A mark. 4) Pip (through the gate) insists on one too, on tiptoe; `{r}` when caught.
* Comic: the pet wants a mark. It is 1 px. The pet looks at it for `[R21]` and beeps, satisfied.
* Heart: Wisp's mum comes to the door and adds hers, and then, a little lower, where it used to be, Wisp's old one from two years ago. "You grew. In our house. While / you were out." `[NO BUTTON]`
* Reward: **Height Marks** (permanent furniture on your door frame, both kids' names and a 1 px dash). Later: every in-game month the pet's dash is re-measured and is still 1 px; Wisp's goes up.

**S168 "The Housewarming"** | Wisp (563), in a body | Ch4-5 (after Ep 4.4) | housing, cooking, talk
* Hook: Wisp wants to throw *you* a housewarming at your Hell Pits flat, "because you did one for me without knowing". Guests: everyone who knocked on the dark night.
* Beats: 1) Decorate the flat to Neighbourly B (Hellvile furniture counts double). 2) Cook two dishes, one per side (Soul Soup, Melon Bread). 3) The party: each visitor from Interlude III reacts to the item *they* gave you, if it's placed. 4) Wisp's parents bring the thinking chair. 5) Anyone who sits in it faces the wall and **says one honest thing** (the Say It verb as furniture; S187 feeds on it).
* Comic: Mascotty arrives, stands in the doorway `[STILL]` `[HOLD 21]`, and this time comes in. Everyone cheers. She sits in the thinking chair, facing the wall, out of habit. WISP: "{d}that's how you use it."
* Heart: WISP, holding a plate: "i was in your house when it / was empty. {..}it's not empty / now."
* Reward: **The Thinking Chair** (furniture), 150 H$, "Placed By: Wisp" trophy. Later (ticker 3 of 4): "Local child reportedly 'un-floated'. Parents thrilled."

**S206 "A Candle Goes Out"** | Wisp (563) | Ch5 (after S168) | talk, housing  *(grafted: heart S133; the chain's true finale)*
* Hook: the candle from the second paper boat (Ep 1.6) has burned in your window since Ch1, and a twin of it in Wisp's window since Ep 4.4. Wisp's mum asks you to help Wisp let theirs go out: "They're home. It can rest."
* Beats: 1) Wisp's window in apartment 13 at dusk. 2) WISP: "If I blow it out, is it giving / up?" (menu: "No." / "It's letting it rest." / say nothing; if `STRY.wisp_answer` was the kind lie, a fourth option appears: "I lied, before. Your mum / hummed. That was the true / one."). 3) `[HOLD 26]` `[MUTE 64]`. Wisp blows. 4) Wisp's mum's hand on their shoulder: "There. Now I can SEE out." Pip arrives with a lantern "for the room, not the window." The pet beeps once, very small.
* Comic: **none.** This is the one side mission with no comic beat and the button withheld entirely.
* Reward: **Window Candle (out)** replaces your lit one; "Somebody Left the Light On" trophy. Later: none.

#### Chain L: the Vapula plush, who is Fluffy (post-game; needs `SF_VAPULA_PLUSH`; 3 parts)

**S169 "Small Weather"** | Lord Vapula (999), a plush grey cloud in a jar | Ch5 | pet, talk, fetch
* Hook: the Duke of the Ninth Cloud is the size of a cushion and rains when he's sad. He is sad a lot. He would like a hobby.
* Beats: 1) Try three hobbies from his roster (Tarot: he predicts rain; Soul Soup: he rains in it; Napping: he's good at this). 2) He asks to follow you like the pet. The pet is not okay with this. 3) A 20 s pet-scene standoff: "beep." vs "squeak." 4) They nap on the same cushion.
* Comic: the tarot reading: the Tower, the Tower, `[R13]` the Tower. VAPULA: "{d}it's a rainy deck."
* Heart: VAPULA: "i was every sigh. {..}nobody / wanted me, and i was so big." YOU (menu): "You're small now." VAPULA: "...is that better?" The pet puts a paw on him. "beep."
* Reward: **Vapula Cushion** (furniture; rains gently on nearby plants), 50 H$. Later: Sootkin's sunflower is the best-watered plant in the park.

**S170 "Vapula's Apology, Properly"** | Lord Vapula (999) + Mayor Giggs (0) | Ch5 | deliver, talk
* Hook: quest #47 had him send flowers. Now he wants to say it. He has written a speech. It is one word.
* Beats: 1) Carry the cloud to Town Hall in a basket. 2) Face to face: two very small, very scared things. 3) VAPULA: "{d}sorry." MAYOR: "Hee. {..}No. Not that." `[R13]` "Me too." 4) It rains in the office. Neither minds.
* Comic: Spreadsheet (531) presents an invoice: "water damage, municipal, x1". The Mayor signs it. Vapula rains on the invoice.
* Heart: two people made of other people's fear, forgiving each other with one word each. `[HOLD 26]` `[NO BUTTON]`
* Reward: 100 H$, Framed Invoice (decor), "Small Weather" trophy. Later: the Mayor's office has a cloud-shaped cushion on the visitor's chair, and the string from the drawer tied to its handle.

**S171 "The Ninth Cloud"** | Lord Vapula (999) | Ch5 | combat (Breach 7 remix), dream, housing
* Hook: his old home, the Ninth Cloud, is still up there, empty, drifting. He wants to see it once more, and then he wants it gone. He is too small to fly. You take Updraft's (17) balloon.
* Beats: 1) The balloon ride (a cutscene with the s03 pan and brake). 2) The Ninth Cloud: a floating Breach 7 remix of mist and unsent letters (the D2 set as a combat map); the enemies are the last scraps of swallowed feeling. 3) At the centre, his tiny cloud room: a throne made of "it's fine"s. 4) Housing: take one thing (the throne, a letter, or nothing). He pops the cloud. It rains on the whole park for a day.
* Comic: the throne, placed in your house, is uncomfortable. Grumbleguts sits on it. It says "{d}it's fine" and collapses. GURGLE: gurgle. `{r}`
* Heart: the rain over the united park is warm. Every citizen stands in it. Nobody runs for cover. TALLYWORN: "First rain in ten years that / wasn't somebody's."
* Reward: **Vapula's Tiny Throne** (furniture), the Ninth Cloud as an Arena level, "Duke of Nothing" trophy, 200 H$. Later: Drizzle's (57) forecast reads "mild, with a chance of plush".

#### Chain U: Grumbleguts's little black book (3 parts; adults only, §3 rules)

**S172 "The Little Black Book"** | Grumbleguts | Ch4-5 | find, deliver, talk
* Hook: his little black book, the size of a phone directory, has been "mislaid" in the Underbelly. It contains every rejection he has ever received, annotated "playing hard to get". He would like it back before anyone reads it. Everyone has read it.
* Beats: 1) Find it (a demon is using it as a doorstop). 2) Carry it back; it is heavy enough to slow you. 3) Three pages fall out: Fangela (519) "sniffed your ID and said no", Lady Smog "declined with a curtsey", the Gate "creaked". 4) He files them under "progress".
* Comic: every rejection is dated and rated out of ten. The Gate's is a 9. "It creaked WARMLY." `{r}` (he turns to admire himself in the lava puddle on the beat).
* Heart: the last page is blank, headed "FRIENDS". GURGLE: gurgle. GRUMBLEGUTS: "{d}He says you can sign it if you / want." (menu: sign it / sign it with a heart.)
* Reward: 60 H$, **Little Black Book (replica)** (decor; cursed: the pet can't lift it). Later: the replica in your house opens by itself to a page that says "YOU? (no)" in his handwriting, then "(yes)" in GURGLE's.

**S173 "Profile Picture"** | Grumbleguts + Shutterbug (46) | Ch5 | photo, creator, talk
* Hook: the dating ticker has gone wild since the Wall fell. He wants a profile. Shutterbug has agreed to one photo. One.
* Beats: 1) Three poses `[3x@15]`: the flex (belly first), the lean (rear first `{r}`), the "candid" (eating). 2) The one photo. It is all three somehow. 3) The bio in the Creator's text field (3 options; one is just "gate."). 4) First reply: Honkers. Honk. Second: nobody. Third: the Gate's maintenance notice, which he counts.
* Comic: SHUTTERBUG: "Say cheese." G: "Is there cheese?" `[R8]` There is not cheese. The photo captures his disappointment. It is his best angle.
* Heart: the bio he writes after you leave: "Big. Loud. Kept a gate for / ten years so nobody would get / hurt. {..}Likes: everyone. / {d}Looking for: one."
* Reward: 80 H$, Profile Photo (decor; mid-bite, winking), the "Hell's Biggest Flirt" romance ticker. Later: Nox's potluck seats him next to Scaldie. Scaldie says lukewarm. He says it's a start.

**S174 "The Exes"** | Grumbleguts + Oddsball (522) | Ch5 | combat (Arena), talk, cooking  *(absorbs heart S95)*
* Hook: Oddsball has taken bets on how many exes will turn up to a "support group" at Brimstone Bites during his shift. Three do (all demons; all adults; all over it) "to see what the fuss is about". There is no fuss. The Gate is also here, somehow.
* Beats: 0) The group: Fangela, Cloudzilla (510), Kodiak (849); they compare notes `[!RIPPLE]`: all were told "you're cute when you're scared"; Grumbleguts walks in rear-first `{r}` with a tray; everyone leaves except the Gate, which is a door, which stays. Oddsball bet zero and loses. 1) Each ex is a mini-boss in the Arena's new "Exes" mode with a quip ("He flirted with my sister." "He flirted with my DOOR." "He flirted with ME. It was fine, actually. I'm here for the food."). 2) Cook each their favourite dish to end the fight peacefully (pacifist) or fight (brutal; cartoon). 3) Everyone sits down to eat. He does not flirt once. Everyone notices. 4) GURGLE: gurgle. Translation: "he's trying."
* Comic: the third ex is the Gate, on a trolley, creaking. Honkers wheeled it in. G: "{d}You came." The Gate creaks. `{r}`
* Heart: the exes, over soup: "He's a lot. {..}He never once made / anyone feel small." Grumbleguts, overhearing, sucks in his gut `[HOLD 13]`. It springs out. He laughs at himself. First time.
* Reward: 150 H$, Arena mode "The Exes", **Gate on a Trolley** (furniture; creaks when he visits), "Playing Hard to Get" trophy. Later: the FRIENDS page has four signatures.

#### Chain T: Feed the Gate (Grumbleguts; 5 parts; the last is the post-game set piece from the user's notes)

**S183 "First Hellvile Regular"** | Grumbleguts + Mother Nox (778), at Brimstone Bites | Ch4 (after Ep 4.5 and 4.6) | cooking, talk, minigame  *(absorbs heart S104)*
* Hook: he has sat down at Brimstone Bites ("It's ironic!") and ordered the menu. The whole menu. Taco Yaki (128) is out of everything. The restaurant would like him to stay. The kitchen would like help.
* Beats: 1) Cook three Bites dishes (Brimstone Bagel, Lava Cake, Soul Soup). 2) Serve each; he rates it ("Six. Needs salt." "Eight. Needs more."). 3) **The belly judges:** your best dish vs Nox's bone soup; he eats both with effort; GURGLE: gurgle. `[HOLD 8]` G: "The belly says... tie." `{r}` as he turns to leave. NOX: "The belly is a coward." GURGLE: gurgle? NOX: "...Fair." 4) He pays with the gate key. Taco Yaki frames it.
* Comic: he eats with his back to the room; the camera cuts back three times; he is bigger each time `[3x]`. Honkers offers a napkin. He flirts with the napkin.
* Heart: G: "He said there'd be a feast when / the gate opened. {..}Gate's / open." He looks at the empty plates. "Huh. He was right." GURGLE: gurgle.
* Reward: 80 H$, menu item "The Gatekeeper" (all the dishes), **Framed Gate Key** (decor), recipe "Tie Soup". Later: the framed key in your house has a second, smaller key hung on it by nobody: the key to the gatehouse (S185).

**S184 "Line Cook"** | Taco Yaki (128) + Grumbleguts | Ch4-5 | cooking, minigame, timing
* Hook: he wants to cook, not just eat. Taco Yaki flips takoyaki with a toothpick and a flourish and has never had an apprentice who ate the ingredients.
* Beats: 1) A cooking duet: two burners; every third ingredient disappears (he ate it). 2) Flip on the beat; his flips send takoyaki across the room; the pet catches them. 3) His first dish served to someone else: Lady Smog. She says it's lovely. He sucks his gut in `[HOLD 13]`; it springs out `{r}`. 4) GURGLE, for once, says nothing. It's full.
* Comic: "I only had a light snack" as a forklift beeps in with the restocking order. TACO YAKI: "{d}That's the week's."
* Heart: G, plating: "Nobody ever ate anything I / made. Nobody ever came up to / the gate." TACO YAKI: "They're coming now." The queue is out the door.
* Reward: 100 H$, **Chef's Hat (XXL)** (decor), recipe "Gatekeeper's Takoyaki". Later: Flossie (517) flosses him after every shift. He lets her.

**S207 "Someone for Dessert"** | Grumbleguts | Ch5 (after S184) | deliver, talk  *(grafted: heart S105)*
* Hook: he has made dessert for two and wants you to find "anyone" to eat the other one.
* Beats: 1) Invite three citizens (any; each refuses, `{r}` each). 2) The fourth is Mr. Solemn (555), who has not smiled in 200 years; he sits down because "someone should". 3) They eat in silence `[HOLD 26]` `[MUTE 26]`. 4) SOLEMN: "Close." `[R8]` G: "To what?" SOLEMN: "Once."
* Comic: the dessert is a cake shaped like his own rear. Solemn eats it without comment.
* Heart: `[NO BUTTON]`. He accepts Nox's line-cook job the next morning and wears the tight shirt.
* Reward: 80 H$, **The Big Chair** (furniture; creaks; citizens slide off; three fit in it), "Hellvile's Biggest Spoon" eligibility for S186. Later: S176 pays Solemn off.

**S185 "Come Up and See My Gate"** | Grumbleguts | Ch5 | housing, fetch, talk
* Hook: the Gate is repaired (the Ep 2.7 crack welded by Anvil and Rivet). He wants to decorate the gatehouse "for visitors". Nobody has ever visited. He has flowers (S162), a heart (Stencilla) and no idea.
* Beats: 1) Housing in the gatehouse (a 4x4 room; his furniture is enormous; every piece is a seat). 2) Place a mirror (he insists; he admires himself `{r}`). 3) Invite three guests (each gives a reason not to come and comes anyway). 4) The visit: Fuzz comes. He sits. He does not say "no" once, because Grumbleguts does not ask.
* Comic: every seat is sized for him; the guests sit three to a chair. Fuzz, between Nox and Lady Smog, is lifted off the ground by the cushion. "{d}This is fine."
* Heart: G: "So this is it. The gate. {..}Ten / years." NOX: "It's a good gate." G: "...Nobody ever said that." A little heart appears painted on the Gate the next morning. Nobody knows who did that. Everybody knows.
* Reward: **Gatehouse** (a visitable place), 150 H$, **Very Large Seat** (furniture), "Come Up Sometime" trophy. Later: the Gate's maintenance notice reads "visitors: 4".

**S186 "The Feast of Rebuilding"** (optional title card: **"What an Ass"**) | Grumbleguts | Ch5 (after the credits; weekend only, `day % 7 == 6`) | talk, cooking, exorcism, find, timing; **two endings, one permanent**
* Hook: Grumbleguts gathers **everyone you know** (your friends list and party citizens, from the sim's relationship data) in Hellvile Park and begs for a giant feast so he'll have "more energy to rebuild the town faster" (he claims). Nox hosts. Nana, Charlotte and Scorchmello cook. The buffet is the length of the old Wall.
* The twist: **one guest is possessed** (drawn by a hash of the save from the citizens present; never a kid, never the Mayor or Nox; the possessor is a leftover Pump bottle Signhere "forgot to file"). The game tells you only: "Something at this table isn't hungry." One more exorcism.
* The clock: a **fullness meter on his belly and his rear** (both tracked). It rises with every course. When it fills, the outcome cutscene plays.
* Beats:
  1. **Arrival.** He eats with his back to the crowd and camera: low angle from behind the crowd, rear framed front and centre, every eye-line on it; the camera "accidentally" cuts back `{r}` each time, bigger every cut `[3x]`; Honkers, napkin, flirt, napkin not interested.
  2. **Clue-gathering** (8-12 guests have one line each): who hasn't eaten, who said "we", whose shadow is late, who laughed a beat late (W7, W8, W9 as the detective kit); the possessed one's idle clock is 0 and their ears twitch at 1.7 rad/f if you stand still for R21. Three clues narrow it to one; a fourth confirms. The pet sniffs ("beep." near the culprit, "beep?" elsewhere).
  3. **Courses.** Between clue rounds a course lands `[DROP]` and the meter rises a step; a strap pings off `[SLAM]` on steps 3 and 6; a chair gives way on 7.
  4. **The exorcism** (if chosen in time): the full ritual on the culprit, in public, with Fuzz; the true-name phase draws from guests' names. Win: the culprit sits down dizzy ("Why do I taste grape?"); the feast ends happily; he is "merely too full to move for a day" and is wheeled home on the Gate's trolley, waving. Lose (three fails), or **choose not to** ("let's see what happens" is an explicit menu item; so is running the clock) → the outcome.
  5. **The outcome cutscene** (the user's spec, staged to the letter; cartoon, non-sexual, vanity and gluttony punished):
     * The crowd watches him gorge. The culprit sneaks behind the crowd into direct view of his rear, turns to the camera, wiggles their eyebrows, thumb pointing at it: "Look who's full of himself, / now?" `[HOLD 21]` `{r}`.
     * **The hose shot:** side-on medium; his rear fills one edge of frame, the culprit on the other, feeding **an absurdly long hose** in like a **reverse rope climb**: hand over hand, pushing it away and in, feet braced, tongue out, grunting, the coil on the ground shrinking; the music drops to **a single plucked note per pull**. Rule of three: a pause, caption appears "surely that's enough", it keeps going; a second pause; it keeps going. Cut to the crowd in silence `[MUTE]`. Back to the climb. The insertion is offscreen behind the cheeks: hose motion, a squeaky SFX, his slow dawning look over his shoulder `[EXPR SHOCK]` at `[R13]`.
     * **The valve crank.** He inflates like a parade balloon: `[BOING]` squash and stretch, buttons popping `[3x@15]`, harness straps pinging off `[TINK]`, the pants' seams, the chair collapsing, the rear ballooning biggest of all; **two false "about to pop" beats** (`[HOLD 13]` each; a last gurgle from the belly on the second, GURGLE: "grrrbl...?"), then the **POP**: CUTE: confetti, plush stuffing, hearts, a halo `[FLASH w10,5]` `[SHK_ROAR]`. BRUTAL: a red shower and gibs `[FLASH r10,7,4]` `[SHK_ROAR]`, the belly landing last, still gurgling.
     * **The last beat:** a lone pair of cheeks drifting down like a balloon with a slow raspberry (cute) / landing with a squelch (brutal). The culprit gives the camera a "well, that happened" look. `[IRIS○ out 10 on the cheeks]`.
  6. **Consequence (permanent for that save; `STRY.feast = 2`, `SF_GRUMBLE_DEAD`):** Grumbleguts is **RES_DEAD** and non-interactable for the rest of the file; a graveyard plot with a comically oversized headstone, "He was full of himself."; the gatehouse goes empty and sealed; his dating profile reads "deceased"; bestiary and almanac update; the Brimstone Bites menu gains a memorial dish, **"The Light Snack"**; friends grieve and mention it; the credits cameo is removed (the buffet is full and nobody is at it); **no ghost rite can bring him back** (`SF_GRUMBLE_NO_RITE` blocks it). The culprit remembers nothing and feels terrible.
* Comic: the entire scene is one; Honkers honks at the funeral; Mr. Solemn nods.
* Heart (save branch): GURGLE's last line as he's wheeled home: gurgle. G: "He says thank you for the / exorcism. {..}And is there / dessert." (pop branch): the FRIENDS page of the black book, found in the gatehouse, has your signature and a heart on it, and nothing else.
* Reward: **Save:** "Hellvile's Biggest Spoon" trophy, **Spoon of Rebuilding** (furniture; doubled with a second spoon if `USTORY_PACIFIST`), 300 H$, "Feast: Saved". **Pop:** "Feast: What an Ass", **Oversized Headstone (miniature)** (decor), 300 H$. Later (save): he rebuilds the gatehouse roof himself with the energy from the feast. He was right, annoyingly.

#### Chain V: Housekeeping (Hauntessa, Wobblewisp, Pharaohnie; 3 parts)  *(grafted: heart S54, S68, S115)*

**S208 "Notes From the Housekeeper"** | Hauntessa (109, Hillvile haunted housekeeper, ghost) | Ch1-2 | housing, find
* Hook: a ghost has been tidying your house and leaving notes ("your crate is a table now. you're welcome.").
* Beats: 1) Find 3 notes. 2) She appears `[POP TW_POP_SOUL]` through the wall, apologising. 3) Decorating goal: 6 items, nothing in front of the door. 4) She grades it `[SLAM]` "TIDY".
* Comic: she rearranges your furniture while you watch, apologising on every move; the pet moves it back; they take turns for `[3x@15]`; she gives up on the pet bed.
* Heart: she haunts empty houses because "someone should keep them warm / for whoever's next."
* Reward: 40 H$, **Hauntessa's Duster** (furniture; a visitor in Ch4 asks if it's haunted; it is). Later: the morning after Ep 2.3's chair night there is a note on the chair: "not me. (i'd have centred it.)"

**S209 "Two Inches to the Left"** | Hauntessa (109) | Ch2 (after Ep 2.3) | housing, find
* Hook: "Someone ELSE is tidying your house. Badly."
* Beats: 1) Three nights, something moves 2 inches left (keys, lamp, the egg basket); the piece Wisp moves goes a whole cell, so the two can be told apart. 2) Wait up: Wobblewisp (972), a Hellvile poltergeist who came through with the fog "for the quiet". 3) Hauntessa and Wobblewisp argue tidying philosophy `[!RIPPLE]` of knocked-over cups. 4) Truce: he may move the Vapor Detector only.
* Comic: WOBBLEWISP: "Two inches is TASTEFUL." HAUNTESSA: "Two inches is a DRAUGHT."
* Scare: the first night the chair is facing your bed, and it is neither of them.
* Reward: 40 H$, **Wobblewisp's Favourite Vase** (furniture; moves 2 inches left nightly). Later: in Ep 3.7 the vase is the one thing in the house the siege does not knock over; it has moved 2 inches left.

**S210 "Cosy, Dark, Eternal"** | Hauntessa (109) + Pharaohnie (673, Hellvile tomb realtor) | Ch4 (after the Annex) | housing
* Hook: the Annex needs rating "by both traditions".
* Beats: 1) Goal: 4 Hellvile pieces in the Annex and the mixed-style bonus. 2) Pharaohnie's walkthrough: "Cosy. Dark. {..}Where's / eternal?" 3) Place a cursed item (`[INSERT 4]`, one change, "...it's fine." from Kevin at `[R8]`). 4) Two stamps `[SLAM]`×2, 15 f apart: "TIDY" and "ETERNAL".
* Comic: Pharaohnie tries to sell your house to Hauntessa. Hauntessa makes an offer. Pharaohnie accepts. Neither can hold a pen.
* Heart: Hauntessa has never been inside a house someone stayed in; she dusts around the height marks (S205), not over them.
* Reward: **Realtor's Sign ("MUST SEE")** (decor), ETERNAL stamp (gates Pharaohnie's weekly visitors). Later: the realtor's sign in your garden gets a second line overnight: "SOLD (to a ghost) (she's keeping it warm)".

### 14.2 Stand-alone missions (25)

**S143 "Population: 1000"** | Mayor Giggs (0) | Ch5 | fetch, minigame, talk
* Hook: the town sign still says 500. The scratched-out 1000 underneath was the original. He wants to repaint it himself and has never held a roller.
* Beats: 1) Fetch paint from Rainbow Ewe (28) and Stencilla (565), one colour each. 2) A roller minigame: the s12 counter rolls `v = N·(a/22)^2.2` as you paint: 500, 750, 1000. 3) "Population: 1000 lovely citizens. And @P." 4) `[R8]` the "0" falls off, *tink*.
* Comic: RIVET, passing: "Don't make it weird." He glues the 0 on. It falls off again. TALLYWORN: "{d}999. I can live with 999."
* Heart: he paints the "1" in "1000" first and stops. "I painted this over myself. / Ten years ago. With this / roller." He finishes.
* Reward: **Town Sign (miniature)** (decor), 100 H$, "Census" trophy. Later: the title screen's subtitle reads "Population: 1000".

**S144 "Fishing for Compliments"** | Capt. Whisk (48) | Ch1-5 | fishing, talk  *(absorbs heart S65 as its Ch2 variant)*
* Hook: Whisk catches fish, names them and lets them go. One keeps coming back and he has run out of names.
* Beats: 1) Fish until the tagged one bites ("Gerald, Gerald II, Gerry, G, Fish"). 2) Name it (text entry). 3) Let it go. 4) It comes back the next day with your name on the tag. **Ch2 variant:** the fish taste of grape and face the Wall at night; the third catch is purple and hums the lullaby; Whisk lets it go `[HOLD 13]`.
* Comic: the tag has 40 crossed-out names; the last is "Please Stop". WHISK: "{d}He wrote that one himself."
* Heart: "You let something go forty / times and it comes back forty / times. {..}That's not fishing. / That's a friend."
* Reward: Fishdex "Gerald (persistent)", "Grapefish (temporary)", Fish Trophy (decor; alive; fine). Later: at the Lake of Fire stall in Ep 4.7 the fish is there. In lava. Fine.

**S145 "Bedtime for Everyone"** | Story Owl (16) | Ch1-2 | talk, dream, housing
* Hook: Story Owl reads to the whole street at once. Tonight: 'The Very Hungry Demon' (#6's book), to be tested on the pet.
* Beats: 1) Place the pet bed near the window (housing). 2) A 60 s story scene; the pet reacts per page. 3) Peek at a Dream: the pet dreams the book, but the demon is **small and crying** and eats only its own tail. 4) Story Owl rewrites the ending.
* Comic: the demon eats "one apple, two pears, three plums, {d}four Hillvile." STORY OWL: "It's a classic."
* Heart: the new ending: "And the very hungry demon was given one cupcake, and it was enough." "First edit in sixty years."
* Reward: 20 H$, 'The Very Hungry Demon' (decor; readable). Later: D2, one of the floating letters is from the demon in the book.

**S146 "Whack-a-Nothing"** | Squeaker (64) | Ch2 | minigame, find  *(absorbs heart S59's "STAFF" beat)*
* Hook: the Whack-an-Imp machine pops up imps when it's switched off.
* Beats: 1) Play it. 2) Play it switched off: the imps still come, slower, looking at you. 3) Find the plug: it runs under the ground toward the Wall. 4) Squeaker unplugs it. It keeps going. "{d}It's a very good machine."
* Comic: "Do NOT bonk Father Fuzz." You bonk Father Fuzz. He is not in the machine. He is next to it. `[SHK_POW]` "tsk." The top prize is Mr. Huggins; Fuzz won't let him go: "He's not a prize. He's STAFF."
* Scare: the last imp that pops up with the machine off says "beep." in the pet's voice. `[MUTE 14]`
* Reward: 30 H$, Mini Whack-an-Imp (furniture; the pet plays it). Later: Ep 3.3, the Fun Fair nest: the machine is possessed for real.

**S147 "Say Cheese (Again)"** | Shutterbug (46) | Ch2-3 | photo, find
* Hook: his photos show things that weren't there (W8). A "before" set of the town, "in case".
* Beats: 1) Photograph 8 landmarks. 2) Each develops with one wrongness (an extra shadow, a crown in a cloud, a chair on a roof). 3) He pins them as a board. 4) In Ch3 the board is in the church: the only record of what the town looked like.
* Comic: the photo of your house has Mascotty in the window. You were inside. SHUTTERBUG: "{d}Great composition."
* Scare: the photo of the Wall has a hand pressed flat on it, from the other side. `[HOLD 21]`
* Reward: 50 H$, Before Board (decor). Later: #45 becomes the "after" and he hangs them side by side.

**S148 "Lullaby for the Wall"** | Maestro Mew (3) | Ch2-3 | concert, studio, fetch
* Hook: the bells un-ring (W11) on a particular note. He wants a piece that "rings them the right way round".
* Beats: 1) Compose 4 bars in the Studio (constrained to the bell scale). 2) Perform with Cantor Coo. 3) The bells ring forward. The Wall thumps on the beat. 4) Mew keeps the thump in the piece.
* Comic: Cantor Coo plays it at the speed of disco. Mew conducts with his tail at half speed. The orchestra follows the tail. COO: "{d}Rude."
* Heart: MEW: "Someone over there has rhythm. / {..}I'd like to meet the / percussion section." (He does, in Ep 4.7: Moshpit (554) and Riffmaw.)
* Reward: the song "Lullaby for the Wall", 40 H$, Conductor's Baton (decor). Later: the pump yard in Ep 4.2 works to this rhythm.

**S149 "Make Me a Face"** | Phantasma (524, Hellvile) | Ch4 | creator, talk
* Hook: Phantasma, a ghost who pretends to be a ghost, wants a "disguise" for the festival: a cute Hillvile look. They have never seen themselves in a mirror.
* Beats: 1) Creator: a citizen-look variant (colour, ears, accessory). 2) Show them: they float through the mirror. 3) A polaroid instead. 4) They cry (a happy ghost rains upward).
* Comic: every accessory falls through them `[3x@15]`: hat, bow, `[R13]` "{d}...a smaller hat."
* Heart: PHANTASMA: "I pretend to be a ghost so / people aren't scared when they / find out. {..}This face isn't / scary." YOU (menu): "Neither was the old one."
* Reward: 60 H$, Phantasma's Portrait (decor), Creator: Hellvile palette. Later: Phantasma hosts the festival's "fake seance", which is a real one, which is fine.

**S150 "Hellvile Hopscotch"** | Tantrum (530, Hellvile, KID) | Ch4-5 | minigame, pet
* Hook: Hellvile kids play hopscotch on lava with oven mitts. Tantrum wants a Hillvile opponent to "lose properly".
* Beats: 1) A rhythm-hop on 10 lava squares (the pet goes first and sizzles; fine). 2) Lose on purpose or win. 3) Either way, a tantrum (0.5 s, two stamps). 4) Pokefork (540) pokes you with a foam pitchfork. Giggles.
* Comic: the tantrum is `[SHK_TAP]`×2 and "{d}hmph." Then xe offers you a mitt.
* Heart: TANTRUM: "Hillvile kids don't come here." YOU: "They do now." Pip arrives with Mr. Flops in two tiny oven mitts.
* Reward: 30 H$, Oven Mitts (decor, tiny), Lava Hopscotch in free play. Later: S159's field trip.

**S176 "Rest in Pieces"** | Mr. Solemn (555, Hellvile undertaker) | Ch4-5 | talk, find, housing (the graveyard)
* Hook: the joke graves offend him professionally. He wants them "done properly". The joke graves' ghosts disagree.
* Beats: 1) Each joke ghost has a request (a nicer font, a bigger moustache, a candle in the cake). 2) Housing in the graveyard: three proper headstones with the jokes intact. 3) He reads "Here lies the Great Cake of '89. It was delicious." `[HOLD 26]`. His mouth moves 1 px. 4) He commissions a fourth: "Mr. Huggins the 1st. Came back."
* Comic: the moustache's ghost is just a moustache. It is very expressive. It wants serif.
* Heart: SOLEMN: "Two hundred years. Every / funeral, a clown honked. {..}I / used to think that was / disrespect." `[R13]` "{d}It was company."
* Reward: 80 H$, Tiny Headstone (decor; engraves what you type), Graveyard: Proper Mode. Later: Honkers is invited back to a funeral. He honks. Mr. Solemn nods. Fluffy's stone is the one grave he leaves exactly as it is.

**S178 "The Weeping Widow's Plushie"** | the Weeping Widow (Breach 5 boss) | Ch3-4 (after the graveyard nest) | combat (Breach 5), find, deliver
* Hook: the Widow has wept since 1887 over a lost plushie. Pip, who knows about lost plushies, wants to find it. Sutura wants to fix it.
* Beats: 1) Breach 5 to its last room without killing her (dodge her tears). 2) The plushie is in the Factory archives (Breach 4), filed under 1887. 3) Sutura mends it (night). 4) Deliver. The puddles dry. The joke ghosts applaud.
* Comic: the plushie is a bunny exactly like Mr. Flops. Pip, holding both `[HOLD 21]`: "{d}...Grand-Flops."
* Heart: the Widow, dry-eyed for the first time in 139 years: "I didn't know what to do with / my hands." SUTURA: "Hold it." She does. `[NO BUTTON]`
* Reward: 120 H$, Breach 5: Dry variant (the Widow is friendly), **Grand-Flops** (furniture; Pip visits it). Later: the Widow attends the Mingle and cries once, happily. The puddle is heart-shaped.

**S179 "Bone Cathedral Acoustics"** | Cantor Coo (426) + Dirgelet (766, Hellvile) | Ch4-5 | concert, combat (Breach 6), studio
* Hook: the Bone Cathedral (Breach 6) has the best acoustics in either town and is full of demons. Coo wants disco hymns in it. Dirgelet has big lungs and bigger feelings.
* Beats: 1) Clear Breach 6 or sneak past the Bishop asleep in the pulpit. 2) The Hellvile twin of the Wall's plaque is in the crypt: "...EVERYONE. (G.G. / M.N.)" completes S197's sentence. 3) Studio: a hymn both choirs can sing (Hillvile major, Hellvile minor, one shared chorus). 4) The concert: both choirs; Seraphail (544) sings backwards and still sounds lovely; the Bishop wakes and joins the bass section.
* Comic: Coo plays at disco speed; Dirgelet weeps at dirge speed; it works only because Riffmaw tunes the bells by playing too loud.
* Heart: DIRGELET: "Ten years I sang into a Wall / because it was the only thing / that didn't tell me to stop. / {..}It's a better one to sing / through." (allowed use 2 of 3)
* Reward: the song "Two Choirs", 120 H$, Breach 6: Choir variant, **Bone Organ (miniature)** (furniture). Later: the Hillvile bells ring forward on the disco beat, forever.

**S180 "The Sweet Tooth Sewers"** | Nana Puddin (1) + Gumdrop Gutterking (Breach 3 boss) | Ch3-5 | combat (Breach 3), cooking, fetch
* Hook: the sewers are candy because ten years of frosting went down the drain. The Gutterking is made of it. Nana's sourdough starter rolled down there in Ep 3.3.
* Beats: 1) The starter is a mini-boss, bubbling, in the throne room. 2) The Gutterking's deal: "Don't lick the floor. {..}Bake / me something that ISN'T / sugar." 3) Bake bone-flour bread (S112). 4) He eats it, cries, returns the starter and "the good sugar".
* Comic: everything he says, he licks after. "Don't lick the floor." *lick* "{d}Do as I say."
* Heart: NANA: "Everything sweet we didn't want / went down here and grew a / king." GUTTERKING: "I'm a very sad king." NANA: "Come up. I'll teach you bread." He can't fit through the door. Rivet widens the door.
* Reward: Sourdough Starter (the bakery sells sourdough from now on), 100 H$, Breach 3: Bakery variant, **Candy Floor Tile** (furniture; the pet licks it). Later: the Gutterking is at the festival bakery stall, licking a loaf.

**S181 "Foreman Fluffnuts Retires"** | Foreman Fluffnuts (Breach 4 boss) + Rivet (13) | Ch4-5 | combat (Breach 4), weld, housing
* Hook: the foreman turned plushies into demons because that was the only order he ever got. There are no orders now. He is still running the conveyor. Rivet wants the conveyor.
* Beats: 1) Shut the conveyor down (a weld puzzle under fire: three valves on S105's rhythm). 2) Fluffnuts, idle, doesn't know what to do with his hands. 3) Weld the conveyor into a furniture-maker. 4) His first product: a plush of you. Then of the pet. With horns.
* Comic: the first output is 40 identical plush Mr. Flopses `[3x]` accelerating. PIP, arriving `[FREEZE]` `[HOLD 21]`: "{d}...Brothers."
* Heart: FLUFFNUTS: "I made monsters because / somebody asked. {..}Nobody ever / asked for anything else." RIVET: "I'm asking. Make something / soft." He does. He cries into it. It's absorbent.
* Reward: **Plushie Press** (a shop: plush furniture of any befriended citizen), 120 H$, Breach 4: Soft variant. Later: Grumbleguts orders a plush of the Gate.

**S182 "The Upside-Down Map"** | Scoutmaster (15) | Ch4-5 | find, photo, talk
* Hook: he has every badge, even sewing. There is no badge for "both sides of the park". He needs a map; Sprinklepop's soul left hers upside down on the Wall and it's the only one.
* Beats: 1) Find the map (on the gatepost, upside down). 2) Walk it upside down: 6 landmarks that mirror each other across the old Wall line (bakery and Charlotte's; daycare and the Hellvile playground; bath house and Scaldie's sauna). 3) Photograph each pair. 4) The badge: two halves of a heart, one pastel, one purple. Both wobble. One drips.
* Comic: he reads the upside-down map upside down to correct it and gets lost in his own town. "I have a badge for this." `[R8]` "{d}I do not have a badge for this."
* Heart: the mirrored landmarks were never a coincidence. NOX: "We planned it together. Forty / years ago. {..}Hee. Hee." (She has his laugh too.) `[NO BUTTON]`
* Reward: **Both Sides Badge** (decor; the pet wears one), 80 H$, the full park map in the quest log. Later: #48 gives the badge to every citizen who crosses.

**S187 "The Sin Bin"** | Mopsworth (547, Hellvile janitor) | Ch4-5 | fetch, find, housing
* Hook: Mopsworth mops up sins and wrings them out. Since the fountain was re-plumbed two-way, tiny Hillvile "sins" come through the pipes: unsent apologies, mostly. He needs a bigger bin.
* Beats: 1) Fetch 3 Scrap; Rivet welds a bin. 2) Find 5 bubbles in Hellvile: "I ate Nana's muffin and blamed the pet", "I never liked grape", "I'm scared of the dark". 3) Return them to their owners, who sit in the Thinking Chair (S168) and say them out loud. 4) The bin is empty by evening. Mopsworth is unemployed. He is thrilled.
* Comic: the biggest "sin" is the Mayor's: a bubble the size of a bathtub that just says "hee." Three wrings `[3x@15]`. "{d}That's a lot of hee."
* Heart: MOPSWORTH: "Ten years I mopped up things / nobody said because somebody / had to hold them. {..}Turns out / you just have to say them." (allowed use 3 of 3.) He becomes the park's gardener. The bin becomes a planter.
* Reward: 60 H$, **Sin Bin Planter** (furniture; grows one flower per honest thing said in the Thinking Chair). Later: the Mayor says "hee" and nothing comes out of the pipes.

**S191 "Sulfur Shanties"** | Barnaby (768, Hellvile) + Capt. Whisk (48) | Ch4-5 | fishing, studio, concert
* Hook: the Lake of Fire's fish weep when Barnaby sings. Whisk names fish and lets them go. A fishing concert where nobody cries. The fish have opinions.
* Beats: 1) Fishing at the Lake (lava fish; the Fishdex grows). 2) Barnaby sings; the fish weep; the lake rises. 3) Whisk suggests a happy shanty; Barnaby has none; the Studio writes one (a sea-shanty rhythm editor). 4) The concert on the pier: the fish don't weep. One is Gerald (S144). It laughs. Fish can laugh now.
* Comic: the "happy" shanty is about a boat that sinks but everyone's fine. The fish weep. WHISK: "Try one where the boat floats." BARNABY: "{d}...Where's the song in that."
* Heart: Barnaby keeps a tally of fish who cried at each song on the pier post; the new song's column is empty; he carves a smile in it.
* Reward: the song "Boat Floats", 80 H$, Fishdex: Lava fish (6 species), **Weeping Fish Tank** (furniture; the fish laugh when music plays). Later (ticker 4 of 4): "Lake of Fire water level: normal for the first time in a decade."

**S192 "Number Nine"** | Number Nine (514, Hellvile) | Ch4-5 | talk, find, timing
* Hook: Number Nine calls "number nine" at the Hellvile bakery. Nobody has ever been number nine. The ticket is somewhere in Hillvile.
* Beats: 1) A scavenger hunt: Keysie's spares, the Mayor's drawer, Mascotty's paw. 2) It is in Nana's recipe tin, from the last Mingle: "Charlotte's turn: #9". 3) Bring Charlotte to the counter. 4) "Number nine." `[HOLD 26]` "{d}...Here."
* Comic: a voice like a foghorn, a body like a thimble; every "number nine" is `[SHK_TAP]`; in eighty years they have never said "number ten". "Why would I."
* Heart: Charlotte gets her bread, eleven years late; Nana baked it; it's warm. Number Nine, for the first time: "...Number ten." Nobody has a ten. They call it anyway.
* Reward: 50 H$, Ticket #9 (decor), the "Number Ten" jingle. Later: Mandible (S123) stamps the ticket "ON TIME".

**S195 "Glowbug's Constellation"** | Glowbug (12) | Ch5 (the Mingle) | pet, minigame, photo
* Hook: Glowbug herds fireflies into shapes on festival nights. For the first Mingle in eleven years: a thousand fireflies, both sides' names. The pet glows a little now when it's happy.
* Beats: 1) The herding minigame (the pet as herder; fireflies follow its glow). 2) Pick the shape (a heart; two faces; "the fountain"). 3) Midnight: the constellation over Hellvile Park. 4) Photograph it; Shutterbug hangs it beside the "before" (S147) and "after" (#45).
* Comic: the fireflies spell "HAHALAND" and then, drifting, "HAHALANO". GLOWBUG: "{d}Close enough." `[R8]` A single firefly fixes the D.
* Heart: 1000 citizens look up at once `[!RIPPLE]`. The Mayor and Nox, hands held. Tallyworn, not counting. Mascotty, no head. Grumbleguts, eating, looking up anyway. The pet, horned, glowing. `[HOLD 26]` `[NO BUTTON]`. The game's last sincere beat. The button is the title screen.
* Reward: **Firefly Jar** (furniture), 200 H$, "Constellation" trophy, the "Mingle Night" title variant. Later: none. This is the last "later".

**S211 "Thirteen Bells"** | Cantor Coo (426) | Ch3 (night, after #24) | find, exorcism (echo)  *(grafted: heart S87)*
* Hook: Bellwether is saved, but the tower still rings backwards at midnight.
* Beats: 1) Climb the tower at 00:00; no music. 2) Twelve backwards bells, then a thirteenth. 3) The bell rope moves by itself `[HOLD 26]`. 4) A wisp on the rope, confused. 5) A on the prompt: you hum the lullaby's first line (S148); the wisp lets go and goes south.
* Scare: the whole quest. `[NO GAG]`
* Heart: not every soul needed a rite; some only needed the tune. Coo, after, writes "no rite" in the margin of the hymnal.
* Reward: 50 H$, the song "Thirteen Bells", **Small Bell** (furniture; rings at 00:00, once, forwards). Later: the tower rings twelve at midnight from now on; on the first Mingle it rings thirteen on purpose.

**S212 "The Hug Audit"** | Twisty (43) | Ch3 (after #30) | talk, combat  *(grafted: heart S93)*
* Hook: Hugsley is saved and hugging again, "a bit too well".
* Beats: 1) Watch Hugsley hug a cloud spider: it pops into grape goo `[SHK_POW]`. 2) HUGSLEY: "...I meant that one." 3) Three more hugs `[R13]` apart, each pop bigger; the third gets confetti. 4) TWISTY: "He's our best weapon. Don't / tell him."
* Comic: Hugsley hugs you; the screen squashes `[SQUASH 1.1/0.9]`.
* Heart: Hugsley has counted every hug since "the bus": 1,000 a day, except the eleven years of none from the south.
* Reward: 50 H$, **Hug Counter** (furniture). Later: the counter in your house ticks up by one every time the pet is held.

**S213 "One Eye, Three Eyes"** | Kevin (creator preset) + Oddsball (522, Hellvile) | Ch5 | creator, escort, talk  *(grafted: heart S51 + S122)*
* Hook: Kevin wants a hat that fits (every hat clips the third eye), and his third eye has never seen south.
* Beats: 1) Creator: three hats `[R5]` apart each pop back off `[POP]`; a hat with a hole; he looks in the mirror `[HOLD 13]`; only the third eye blinks. 2) Escort him through the gate. 3) Oddsball, a cacoblob with one eye, at the market. 4) They stare `[HOLD 26]` `[MUTE 26]`. KEVIN: "Yes, three." ODDSBALL: "Yes, one." 5) They blink at the same time `[R8]`, and that's the friendship.
* Comic: Oddsball bets on which eye will blink first and loses to the third. KEVIN: "Nobody ever asks about the / third one." `[R21]` "{d}It can see Tuesday."
* Heart: four eyes between them: finally the right number for a game of cards.
* Reward: 40 H$, Kevin's Hat Stand (furniture; the hat has a hole and a tiny flag in it), Eye Chart (both) (decor). Later: Kevin's bust in your house blinks its third eye at the same moment the Eye Chart's one eye does, every 83 frames.

**S214 "The Longest Lullaby"** | Ploddy (701, Hellvile) + Maestro Mew (3) | Ch5 | concert  *(grafted: heart S128)*
* Hook: Ploddy hums lullabies to clouds all night; Mew wants it conducted.
* Beats: 1) The Concert Hall at night. 2) Mew conducts with his tail; the lullaby is "Lullaby for the Wall" slowed to a hum, the one Wisp's mum hums. 3) The jar cloud on a cushion in the front row **rains one pixel on the beat**. 4) The hall sleeps.
* Comic: the orchestra follows the tail off the stage.
* Heart: `[NO BUTTON]`.
* Reward: the song "Hellvile Lullaby", **Cloud Cushion** (furniture). Later: the jar on your shelf rains one pixel whenever the jukebox plays it.

**S215 "Two Brave Kids"** | Shutterbug (46) + Pip (42) + Wisp (563) | Ch5 | photo  *(grafted: heart S119)*
* Hook: Shutterbug wants the two kids and both plushies in one frame.
* Beats: 1) The bandstand. 2) The photo. Wisp's mum is in the back, faint, because the flash caught her mid-step. 3) WISP: "Is Mum in it?" SHUTTERBUG `[HOLD 13]`: "She's in it. Trust me." 4) Mr. Flops blinks in the photo; he has button eyes.
* Comic: Flops the 1st (S102) photobombs with his demon eye and the sticker.
* Heart: `[NO BUTTON]`.
* Reward: **Two Brave Kids (framed)** (furniture). Later: Wisp's mum has the same photo on her wall, and in hers she is in focus.

**S216 "Free Hugs (Both Sides)"** | Hugsley (20) + Big Snarl (855, Hellvile) | Ch5 | talk, minigame  *(grafted: heart S131)*
* Hook: Hugsley wants to hug everyone in Hellvile; Big Snarl is the bouncer.
* Beats: 1) Bring a squeaky toy (Snarl's rule, #36). 2) Hugsley hugs Big Snarl: Snarl squeaks `[HOLD 8]`; "...Again." 3) A hug line forms `[!RIPPLE]`; the gentle / gentler / Grumbleguts minigame. 4) Grumbleguts asks for a hug; Hugsley gives him one; GURGLE says "grrrbl" in a tone nobody has heard before.
* Comic: Hugsley hugs the Gate; it creaks; GRUMBLEGUTS: "Hey."
* Heart: the counter passes 1,000 in a day for the first time in eleven years, with both sides.
* Reward: 50 H$, Hug Counter upgrade (two digits more). Later: Big Snarl keeps the squeaky toy and squeaks it once at anyone who crosses, as a welcome.

**S217 "Signhere's Receipt"** | Signhere (567, Hellvile contract devil) | Ch5 (after Ep 4.5) | deliver, talk  *(grafted: heart S117)*
* Hook: Signhere needs everyone who tore a page in Ep 4.5 to sign an acknowledgment that they tore it. It is 900 pages.
* Beats: 1) Carry it to 6 cast members; each signs one letter of their name. 2) Grumbleguts signs with a rear-print `{r}`. 3) The pet signs with a beep (a tiny horn-shaped mark). 4) Signhere files it: "This is in breach of / nothing. I don't know what / to do with that."
* Comic: the acknowledgment has a 900-page appendix.
* Heart: his first contract nobody regrets. He keeps the pen.
* Reward: 50 H$, **Contract Confetti Jar** (furniture). Later: a single page of the old contract is nailed to the gatehouse door with "VOID" across it in GURGLE's handwriting.

### 14.3 Mission index by chapter (first availability)

| Ch | Missions | Count |
|---|---|---|
| 1 | S100, S107, S110, S121, S124, S127, S130, S134, S142, S144, S145, S151, S154, S157, S196, S199, S208 | 17 |
| 2 | S101, S103, S108, S111, S114, S115, S117, S131, S135, S137, S146, S147, S148, S158, S163, S166, S197, S200, S209 | 19 |
| 3 | S102, S104, S118, S122, S125, S128, S138, S152, S155, S161, S167, S178, S180, S198, S201, S204, S211, S212 | 18 |
| 4 | S105, S106, S109, S112, S116, S119, S123, S126, S129, S132, S133, S136, S139, S149, S150, S153, S156, S159, S162, S164, S168, S172, S176, S179, S181, S182, S183, S184, S202, S203, S205, S210 | 32 |
| 5 (post-game) | S113, S120, S140, S143, S165, S169, S170, S171, S173, S174, S185, S186, S187, S191, S192, S195, S206, S207, S213, S214, S215, S216, S217 | 23 |
| **total** | | **109** |

**Chains (22, 84 missions):** A S100-S102 · B S103 S104 S105 S202 S203 S106 · C S196 S107 S108 S197 S198 S109 · D S199 S110-S113 · S S114-S116 · E S117 S118 S201 S119 S120 · F S121-S123 · G S124-S126 · H S127 S200 S128 S129 · I S130-S133 · J S134 S135 S204 S136 · K S137-S140 · M S151-S153 · N S154-S156 · O S157-S159 · P S142 S161 S162 · Q S163-S165 · R S166 S167 S205 S168 S206 · L S169-S171 · U S172-S174 · T S183 S184 S207 S185 S186 · V S208-S210. Cross-border chains: B, C, D, E, F, G, H, I, J, K, M, N, O, P, Q, R, V (17). **Stand-alones (25):** S143-S150, S176, S178-S182, S187, S191, S192, S195, S211-S217.

**Verb coverage (109):** talk (all), fetch (44), deliver (14), find (24), photo (13), combat (14), exorcism (6), weld (10), pet (9), minigame (13), fishing (2), cooking (13), concert/studio (9), housing (34), creator (4), escort (9), timing (13), dream (3). **Givers:** 47 Hillvile, 41 Hellvile, 6 bosses/plushes, 15 joint; **furniture rewards:** 74 pieces (the housing catalogue's "story and gag" category plus friendship gifts). **Withheld buttons:** 31 of 109 missions end on `[NO BUTTON]`/`[NO GAG]` (28%); no chain withholds twice in a row (checked: B S104→S105, C S198→S109, D S199→S110, E S118→S201, H S200→S128, J S204→S136, R S205→S168→S206). **Deadpan:** no mission conversation has more than one `{d}`. **News-ticker "Later"s:** 4 (S140, S156, S168, S191).

---------------------------------------------------------------------------------------------------
# PART 3 — ENGINE WORK

## 15. What the engine needs (all C99, integer only, no malloc, no stdio; both platforms)

### 15.1 The shape of the change
Today (`story_map.md` §3) every beat is hand-written C: `switch` statements, `StoryLine[]` arrays and C callbacks handed to `ui_say_done`. That is why 73 KB of story stays resident on PS1 (the callbacks escape into the dialogue queue) and why nothing can be re-timed. The expansion replaces three things:

1. **Story logic becomes bytecode** run by one resident interpreter (`story_vm.c`, ~6 KB). Scripts and text are data in per-chapter packs (§16). The C that remains is the dispatcher (`story.c`), the sub-scene push/resume glue, the side-quest table walker and the debug hooks.
2. **Cutscenes, scripted overworld moments and dialogue beats become `BeatScript`s** run by the beat scheduler (`beat.c`, ~3 KB; timing bible §7.2), with named anchors so re-timing a beat drags its SFX, shake and captions.
3. **Motion becomes tables and springs** (`tween.c`, ~2 KB), **screen effects become profiles** (`camfx.c`, ~2.5 KB), and **audio gets four hooks** (mute, duck, tape-stop, resume-on-beat).

Everything below is additive to the registries listed in `housing_final.md` §9.6 (12-slot `save_ext`, new scenes, unlock ids).

### 15.2 The story VM (`core/src/story_vm.c`, `core/include/doomo/story_vm.h`)

```c
typedef struct { uint8_t op; uint8_t a; uint16_t b; } StoryOp;   /* 4 B, ROM / pack */
typedef struct { uint16_t first_op, n_ops; uint8_t n_labels, pack; } StoryScript;
```
One script per episode, special, side mission and conversation hook. Labels are op indices (a `uint16_t` table after the ops). The VM state is 64 B of `.bss`: `pc`, a 4-deep call stack, 16 `uint16_t` vars, the active pack id, the pending sub-scene and its resume label.

| op | a | b | effect |
|---|---|---|---|
| `SAY` | who (resident id, 0xFE narrator, 0xFF player) | text id | queue a line; the line's own beat codes do the rest |
| `SAY_X` | expr (`EXPR_*`) | text id | as SAY with the portrait expression set for the line |
| `CHOICE` | n (2-4) | text id of the question; the n option text ids and n jump labels follow as `DATA` ops | a menu (no name pill); the chosen branch jumps |
| `PLAYER_LINE` | n | as CHOICE | the three pill lines: draws the name pill, types at setup speed (§8.4) |
| `JIF` / `JIFN` | flag or var id | label | jump if (not) set; vars compare against `a` via `JLT`/`JGE` |
| `SETF` / `CLRF` | flag id | — | story flag bits, `STRY` bits, `SF_*` |
| `SETV` / `ADDV` | var id | value | STRY counters (knocks, meter, heights) |
| `BEAT` | script id hi | script id lo | run a `BeatScript` to completion (the VM yields) |
| `WAIT` / `HOLD` | frames | — | yield n frames; HOLD cannot be skipped |
| `WAIT_A` | min frames | — | yield until A (the press ends the setup) |
| `PUSH` | kind (`PEND_*`) | arg | push combat / exorcism / cutscene / pet / workshop / home / cooking / concert / creator / dream; resume at the next op with the result in var 0 |
| `POSSESS` / `UNPOSSESS` | resident id | true-name id (authored possessor) | writes resident status + corruption; the exorcism scene reads the authored possessor table |
| `GIVE` / `TAKE` | item id | count | inventory |
| `REWARD` | H$ / 10 | item id, unlock id follows as `DATA` | toast + SFX |
| `BEFRIEND` | resident id | delta | friendship |
| `TELEPORT` | map id | cell (x<<8 \| y) | `ow_enter_map` |
| `CAST` | resident id | spot id \| flags | place or remove a cast member (replaces `script_cast` literals) |
| `NPC` | actor | act id \| arg | overworld NPC scripting: WALK_TO, HOP, TURN, EMOTE, FLEE, FREEZE, THAW, PANIC, STILL, TWITCH, FACE (§15.8) |
| `CAM` | mode | arg | PAN_TO (cell), FOLLOW, BRAKE, CREEP (px/f Q8), ZOOM (Q8.8), RESET |
| `SFX` / `MUSIC` / `MUTE` / `DUCK` / `TAPESTOP` / `RESUME_ON_BEAT` | id or gain | frames | audio hooks |
| `SHAKE` / `PUNCH` / `FLASH` / `FADE` / `IRIS` / `TINT` / `PAL_LERP` | profile / colour / palette id | arg | `camfx` (§15.5); `PAL_LERP` selects sepia, crayon, hellify, lilac-frame |
| `INSERT` | insert id | frames (3-4) | tonal whiplash: owns the audio for exactly `frames`, then returns to the same shot; the ONE CHANGE is a following `SETF`/`HOME` op |
| `CUT` | shot id | — | hard cut to a cutscene stage shot (§15.7) with SFX on frame 0 |
| `FREEZE_CAP` | caption style | text id | freeze-frame (all clocks clamped, shutter flash 7) + a caption that types/pops/appears per style |
| `HOME` | sub-op | arg | `SET_PHASE`, `PLACE_GIFT` (furn id; giver from the catalogue), `REMOVE_ONLY`, `GOAL` (stamp id), `DECORATE_FOR` (target layout id, tolerance), `HAUNT_STEP`, `DEFEND` (wave table id) |
| `QUEST` | quest id | state | side-quest state (offered / active / done) and chain gating |
| `SET_BEAT` | beat id | — | `story_set_beat` (chapter unlocks, HUD objective) |
| `CALL` / `RET` | script id | — | subroutines (shared gags: the Days Since flip, the pet's insert, the border scratch) |
| `RNG` | var id | pool id | a deterministic pick (§18.4) |
| `DATA` | — | payload | inline operands for CHOICE/REWARD |
| `END` | — | — | |

Sizes: ~300 ops per episode or special (35 → ~42 KB), ~90 per side mission (109 → ~39 KB), ~5 KB of shared subroutines and hooks: **~86 KB of scripts**, in ROM on GBA, streamed per chapter/episode on PS1 (§16.5). `story_debug_step()` keeps working by running the VM under autowin until the next `SET_BEAT`.

### 15.3 The beat scheduler (`core/include/doomo/beat.h`, timing bible §7.2, adopted with four additions)
`BeatEv {at:u16, anchor:u8, op:u8, a:i16, b:i16}` (8 B). Ops: `END, LABEL, WAIT_A, HOLD, SAY, CAPTION, POP, EXPR, ACT, FREEZE, THAW, PANIC, HITSTOP, SHAKE, PUNCH, ZOOM, FLASH, FADE, IRIS_OUT, IRIS_IN, CUT, INSERT, SFX, MUSIC, MUSIC_MUTE, MUSIC_DUCK, TAPESTOP, FLAG` plus the additions **`STILL`** (idle clock 0 for deadpan and horror), **`TWITCH`** (possessed ears 1.7 rad/f), **`NPC`** (the overworld actor ops), **`CAM`**, **`TINT`/`PAL_LERP`**, **`NOBUTTON`** (a marker the lint reads), and **`SHOT`** (select a stage shot: backdrop, actor layout, camera). `beat_tick()` fires events when `label_frame[anchor] + at == now`; A moves only anchors (`WAIT_A`), so the frame-exact gaps inside a punch survive; `beat_age(label)` drives draw code (`tw_tab(&TW_SLAM_STAMP, beat_age(L_FAIL1))`); `beat_clock(actor)` gives each actor its own clamped / 2x / normal clock. State: 64 B + a 32-entry label frame table. The biggest script (Ep 4.6: summit + Say It + CUT_WALL_FALLS) is ~300 events = 2.4 KB; all ~45 scripted scenes total **≤ 61 KB**.

### 15.4 The tween/spring module (`core/include/doomo/tween.h`, timing bible §7.1, adopted verbatim)
```c
fix16 tw_t(int age, int dur);  fix16 tw_smooth(fix16 t);  fix16 tw_in2(fix16 t);  fix16 tw_out2(fix16 t);
fix16 tw_back(fix16 t, fix16 s);            /* UI slides only */
fix16 tw_bounce(fix16 t);                   /* 33-entry LUT of tk_bounce, clamp to 1.0 at i>=30 */
int   tw_lerp(int a, int b, fix16 t);
typedef struct { const int16_t *v; uint8_t n; } TwTab;   int tw_tab(const TwTab *t, int age);
typedef struct { int16_t sx, sy; int8_t dy; } TwSquash;
TwSquash tw_squash(const TwTab *sx, const TwTab *sy, const TwTab *dy, int age, int amount256);
extern const TwTab TW_POP_BUBBLE, TW_POP_LETTER, TW_POP_SOUL, TW_POP_LOCKUP, TW_POP_EYE, TW_POP_MENU, TW_POP_BLOB, TW_POP_BURST,
                   TW_SLAM_STAMP, TW_SLAM_COUNTER, TW_SLAM_STICKER, TW_SLAM_STICK2,
                   TW_DROP_TITLE_SX/SY/DY, TW_DROP_LET_SX/SY/DY, TW_DROP_SOFT_SX/SY/DY,
                   TW_HEART_LAND_SQ, TW_DEVICE_DY, TW_SIGN_DY, TW_CLUNK, TW_CLAP, TW_WINDOW_BOING_DY,
                   TW_BRAKE_DX, TW_ZOOM_WOBBLE_DY, TW_HOP1, TW_HOP2, TW_TAG_DY, TW_DEADPAN_DY;
enum { TW_JELLY, TW_SLAM4, TW_SLAM7, TW_SLAM8, TW_BRAKE, TW_BOING, TW_WOBBLE, TW_FOLLOW, TW_SPRING_COUNT };
typedef struct { int32_t e, ep, target; int16_t a1, a2; } TwSpring;   /* Q4 px, Q14 coeffs, 16 B */
void tw_spring_init(TwSpring*, int preset, int32_t pos_q4);  void tw_spring_target(TwSpring*, int32_t q4);
void tw_spring_punch(TwSpring*, int32_t d_q4);  void tw_spring_shove(TwSpring*, int32_t dv_q4);  int32_t tw_spring_step(TwSpring*);
```
Tables ~400 B ROM, LUT 66 B, presets 32 B; each live spring 16 B RAM (the camera uses two, the held furniture one, the boss belly one). Squash and stretch draws through one new platform primitive, `plat_image_xform(ax, ay, img, sx_q8, sy_q8, flags, color)` (GBA: IWRAM scaled span blit for the 5-6 wobble frames, or an affine OBJ for the four hero actors; PS1: one `POLY_FT4`), anchored at the feet. Grumbleguts' sprite uses `TW_BOING` on every landing; hell props never wobble (a per-sprite `NO_SQUASH` flag); plush deforms up to 34%.

### 15.5 Camera FX (`core/include/doomo/camfx.h`, timing bible §7.3)
`camfx_shake(profile)` (the 14 profiles of Appendix C as data, ~250 B), `camfx_punch(dx, dy)` (a `TW_SLAM4` kick), `camfx_zoom_snap(scale_q8, frames)` (GBA BG2 affine for 1-2 frames under a flash then cut; PS1 GTE `H` change), `camfx_flash(color, level16, frames)`, `camfx_fade(level16)`, `camfx_iris(cx, cy, r, heart, rim)`, `camfx_tick()`, `camfx_offset()`. Platform hooks: `plat_post_offset` (GBA `REG_BG2X/Y` at flip; PS1 E5 draw offset with a second E5 before the HUD), `plat_post_scale`, `plat_post_blend` (GBA `BLDY`; PS1 additive/subtractive TILE), `plat_post_iris` (GBA software spans to start; PS1 row TILEs). `PAL_LERP` is a 256-entry palette lerp DMA'd at vblank (GBA, ~5k cycles per step) / a CLUT swap (PS1): sepia (flashbacks), crayon (P0 and the confession bleed), hellify (inserts), lilac-one-frame (W13). `OPT_SCREEN_SHAKE`/`OPT_REDUCE_FLASH`/`OPT_UI_BOUNCE` scale amplitude and never timing (timing bible §9).

### 15.6 Dialogue (`ui_dialog.c` additions)
* **Per-line record**: `{who:i16, expr:u8, flags:u8, text:u16}` (6 B) in the pack; `flags` carry `DEADPAN`, `PUNCH`, `PILL` (the three player lines), `SHOUT`.
* **Inline beat codes** parsed by the typewriter and skipped by the renderer: `{p N}` `{.}` `{..}` `{...}` `{d}` `{!}` `{big}…{/}` `{w}` `{r}` `{cut N}` `{punch}`; punctuation holds (`,` 3, `.` 6, `!` 4 with a box hop, `?` 5 with a portrait tilt, `...` 5/5/5, newline after `...` R8).
* **Per-glyph hop** `TW_HOP1` on reveal, scaled by `OPT_UI_BOUNCE`; setup lines type (24 px/f), punch lines pop whole, deadpan lines appear.
* **Portraits**: a talk bob on every new line ({1.06/0.94, 1.0} over 2 f), a shout stretch (0.92/1.10 then `TW_JELLY`), `{d}` clamps the portrait clock; the expression comes from the line record (all 11 are finally used); possessed speakers add `TWITCH`.
* **The name pill** is drawn only for `PILL` lines; its first appearance `[POP]`s with `TW_POP_BUBBLE`.
* **Second speaker in one box**: GURGLE lines use a tiny secondary portrait slot (the belly) with its own voice sliders; `{r}` on big gurgles.
* **Box budget**: 3 lines × 28 chars at 240 wide, 4 lines at 320; the pack tool rejects any page that does not fit (§18.2). `ui_say` copies the decoded string into the existing 200-char queue entry, so nothing resident points into a pack.

### 15.7 Cutscenes (`cutscene.c` rebuilt on `beat.h`)
`Panel {who, min_t, text}` becomes a **stage**: `{backdrop id, actor slots (up to 8: sprite kind, resident id or prop, x, y, flip), camera (pan, zoom, pitch on PS1), palette}` plus a `BeatScript`. Shots (`BOP_SHOT`) switch stages inside one script, which is how `[SMASH]`, `[COVER]` and `[INSERT]` work without a scene switch. Draw code reads `beat_age(label)` for every entrance (DROP/POP/SLAM), `beat_clock(actor)` for every idle, and the `camfx` state for everything else. Rebuilt cuts: CHAPTER1-4 cards, ARRIVAL, HOME (now the housing scene), FOG, ERUPTION (+ "The Gate Cracks" insert), VAPULA_DOWN (+ the jar), WALL_FALLS (kept animation, re-timed, Grumbleguts' bump added, never undercut), FREEPLAY; new: P0, THE BOTTLE, F1-F4, D1-D2 (in the Dream scene), the Interlude III knocks, the summit, Say It, the Feast outcome (both gore settings), E1, the Ninth Cloud balloon ride. Skipping: START skips the whole cutscene; A completes typing and never shortens a `HOLD` in a punch beat. Chibi actors come from `bodyspr_image`/`figure_image` with eleven expressions and co-prime idle periods (breath 39.3 f, tail 22.4 f, ears 97 f, blink 83 f), phase-offset by id.

### 15.8 Overworld NPC and camera scripting (`overworld_npc.c`, `overworld.c`)
New NPC states driven by `BOP_NPC`/`OP_NPC`: `WALK_TO(cell)`, `HOP`, `TURN(dir)`, `FACE(actor)`, `EMOTE(bubble)`, `FLEE(dir)` (the `2a + 0.35a²` curve, staggered by `seed % 7`, sweat drops every 3rd frame), `FREEZE`/`THAW`/`PANIC`/`STILL`/`TWITCH`, `HEAD_TURN(rad, frames)` (Steamy's 180° creak, Mascotty's 0.14 rad tilt). Eight script slots (12 B each) in `OwScratch`. A **camera field** in `DoomoOverworldView` (`pan_x/y`, `zoom_q8`, `creep_q8`, `brake`): PS1 pans and zooms through GTE `H`; GBA pans the map, brakes with `TW_BRAKE`, and zooms only 1-2 frames under a flash. The **double-take** (stop R3, head turn 2 f, "!" bang `bounce/4`, emote pop, tik; once per NPC per visit), the **border record scratch** (tape-stop 5 f, 14 f silence, theme on the downbeat, camera brake), and the **running-gag spawner** (a ROM table `{gag, weight, cooldown_min, chapter_mask, night_only}`: the starer at a window; the crowned cloud prop; a pet "beep." during a sincere line, never during a `NOBUTTON` beat; the Days Since flip as you pass Rivet's; Grumbleguts backing into frame in Hellvile, max once per 4 min; Wobblewisp moving a prop; cadence one per 20-40 s, none for 5-15 s after a big beat) live here. The **wrongness table** W1-W16 is the same mechanism with `noticed` bits in `STRY`.

### 15.9 Housing hooks (the story's calls into `housing_final.md`'s `SCENE_HOME`)
Through the resident `home_api.c` (~1 KB): `home_set_phase(phase)`, `home_place_gift(furn)` (DROP animation, giver from the catalogue, unsellable), `home_remove_only(on)`, `home_goal_check(goal)` → stamp, `home_decorate_for(layout id, tolerance)` → progress 0-8 (drives Wisp's opacity), `home_haunt_step(night)`, `home_defend(wave table)` → guests safe count, `home_rivet_visits()` (the sign), `home_days_since_fire()`. The siege's chimney beat is a `BeatScript` the home scene runs at wave 3 through `beat.h` (no group crossing: `housing_final.md` §9.5). New catalogue rows for the 74 furniture rewards above plus the gag pieces; `furngen` recipes.

### 15.10 Audio (`audio.h` additions)
`audio_music_mute(frames)` (the sequencer keeps running; re-enters on the grid), `audio_music_duck(gain_q8, frames)`, `audio_tapestop(frames)` (pitch × 2^(-15/12) ramp, then mute), `audio_music_resume_on_beat()`, `audio_music_dual(song_a, song_b)` (Ep 4.6: HAHALAND_DAY and HELLVILE at once, two channels each), `audio_sfx_pitch(sfx, semitones)` (the kazoo stepping down, the duck squeak pitched down 15). The 13-sound comedy kit (SCRATCH, BOING, POP, THUD, WHOOSH, KAZOO, BEEP, GLITCH, TIK, SHWIP, TINK, FANFARE, CHOIR; PSG recipes ~0 B or PCM 300-800 B each on GBA, ADPCM 1-4 KB each on PS1) plus **GURGLE's voice** (a bass babble preset) and the **plucked note** of the hose climb. New songs (MML): "Mingle" (music box in P0, hummed by Giggs and Nox, full in Ep 4.6, forwards+reversed as the two-faith rite), "Lullaby for the Wall", "Two Choirs", "Boat Floats", "Thirteen Bells", "Hellvile Lullaby", "Double Act", the "Hee. Hee." jingle, "Number Ten". Every cue hangs off a beat label, never a duplicated frame constant.

### 15.11 Exorcism, combat, pet and Breach hooks
* **Authored possessor table** (`exorcism.h`): `{host, possessor, clue props[4], names offered[4], answer, near_miss, strike_herring, saved exit, lost residue}` for Steamy/TALLYWORN, Sprinklepop/WHEEZLE (scripted fail), Hattie/HAZE (prayer phase = the timing minigame; heckles), Grampy/SOOTKIN (name phase = tomato memory game), Smog/FLUFFY (near miss VAPULA, herring GIGGLESWORTH; two-faith rite with the song reversed on the second channel), the Feast culprit (names drawn from guests), Mr. Huggins 2nd/4th/9th (practice skins), Sprinklepop reverse (**EXORCISM UNFAILED**: metronome reversed, bell un-rings, the player types the name, the kazoo reversed), S211's rite-less wisp (a single prompt). Three failures still lose the soul (`resident.h:157`) except where scripted; losses now have consequences (§13b).
* **Combat**: a photo verb (S161's kill frame), a Vapor Sample drop, the HUD boss captions in story runs (`lvd_boss_intro` for Vapula's seven floors and Grumbleguts), the house-defence wave tables, the "it's fine" pellet and Hee-Hee puff projectiles, the rimshot/spoon laughter pickups, Grumbleguts' mechanics in `levels.c` (belly weak point, feed-to-stun with rating captions, belly-flop with GURGLE warning, burp portals ×3, trampoline, the 3 f rear-wiggle telegraph), Breach 3-7 story entries and post-story variants (Bakery, Soft, Dry, Choir, Exes, the Ninth Cloud remix), `USTORY_PACIFIST` finally granted.
* **Pet**: the hatch on the downbeat; the 1-frame and 4-frame inserts; horn nubs / full horns / halo tint as persistent form flags; the **pet-care boss** (SCENE_PET under fire: care actions shrink the swollen form, pellets to dodge, the Mayor's lines on a beat script, the turn-around); "beep." as a dialogue answer; the demons-flinch aura in Ch3; the nap-on-command and glow-when-happy flags.
* **Sim**: Grumbleguts gets a **resident shadow record** (romance, friendship, `RES_DEAD`, the no-rite lock) so the sim treats him as a citizen; Kevin stays a creator preset moved in through the creator's town-write; the Say It pool (~60 lines + cast lines, hash-picked per friend); the Feast reads the friends list for guests and the culprit.

### 15.12 Build order (so the story can be played end to end early)
1. `tween`, `camfx`, `beat` + the audio hooks + `plat_image_xform` (foundation, ~10 KB code; unit-tested against the float originals as `story/tools/ctest/t.c` does).
2. The text-pack tool and `story_vm` with SAY/CHOICE/flags/PUSH only; port the existing 28 beats to scripts so `test_story.c` passes unchanged (the old `BEAT_*` ids alias onto episodes).
3. Dialogue beat codes, per-line expressions, the pill.
4. Cutscene stages; rebuild the 11 existing cuts on `beat.h`; the lint test goes live.
5. NPC and camera scripting; the spawner and wrongness tables.
6. Housing hooks against `SCENE_HOME` as it lands (`housing_final.md` §12 order).
7. Chapters 1-2 scripts and packs; then 3; then 4; then the epilogue; side chains alongside the chapter that opens them.
8. Exorcism skins, Grumbleguts' fight, the pet-care boss, Breach entries.
9. The Feast (S186), NG+, the title-screen variants.

---------------------------------------------------------------------------------------------------
# PART 4 — DATA AND MEMORY

## 16. Where the bytes live

### 16.1 Inventory (estimates; words × 5.8 B average including spaces, punctuation and beat codes)

| Content | Words | Raw bytes | Notes |
|---|---:|---:|---|
| Main path (P0, 27 episodes, 8 specials: ~950 lines + captions) | ~17,500 | ~102 KB | today 2,140 words / 12.3 KB (measured) |
| Say It pool (60) + cast lines (36) | ~900 | 5 KB | |
| Nest region lines (48), cast idle lines per chapter (~120), NG+ lines (40), wrongness captions/subtitles (60) | ~2,200 | 13 KB | |
| Side missions: 109 × (offer, hint, thanks, ~10 scene lines, Later) | ~35,000 | 203 KB | today 2,300 words / 12.2 KB (measured) |
| Existing 49 quests re-timed, small talk | ~3,600 | 21 KB | unchanged size, new codes |
| **Text total** | **~59,000** | **~344 KB raw** | **~172 KB packed** at the conservative 50% (§16.3) |
| Story VM scripts (§15.2) | — | ~86 KB | binary, not compressed |
| Beat scripts (§15.3) | — | ~61 KB | binary |
| Stage/shot tables, possessor table, spawner, wrongness, pools | — | ~6 KB | |
| Palettes (sepia, crayon, hellify, lilac) | — | 2 KB | |
| New songs (9, MML) | — | ~9 KB | |
| Comedy SFX kit + GURGLE voice + pluck | — | GBA ~8 KB PCM / PS1 ~30 KB ADPCM | |
| New sprite/portrait art (Mascotty with and without the head, Wisp translucent and solid, Grumbleguts and GURGLE, Sprinklepop's soul, the plush Vapula, Haze, Wheezle, Fluffnuts, the Widow, the Gutterking, Signhere, Kevin; 11 expressions where they speak) | — | ~60 KB | portraits are rendered from sliders where a roster row exists; bespoke art only for the non-roster bodies |
| Housing (catalog, iso sprites, prims) | — | ~0.8 MB | `housing_final.md` (measured estimate) |

### 16.2 The text pack format (`core/tools/textpack.py` → `core/gen/text_*.c` and `ps1/iso/TEXT.BIN`)
A **pack** is the unit of streaming: one per episode or special (35), one "chapter common" per chapter (5: idle lines, region lines, that chapter's Say It subset, small-talk topics), one per side chain or stand-alone group (26), one for the Feast. Layout:
```
u16 magic 'TX', u8 version, u8 pack id, u16 n_strings, u16 n_lines, u16 n_scripts, u16 n_beats
u16 str_off[n_strings]        byte offsets of the Huffman bit streams (each string byte-aligned, ≤ 7 bits wasted)
DlgLine lines[n_lines]        6 B each (who, expr, flags, text)
StoryScript scripts[]; StoryOp ops[]; uint16 labels[]
BeatScript beats[]; BeatEv evs[]
u8 streams[]                  the strings
```
Text ids are `(pack << 10) | index`; the VM resolves them through the loaded-pack table. Decoding is per string into the caller's buffer (`txt_get(id, out, cap)`; the dialogue queue entry is the buffer), never into a resident copy. Pack sizes (packed): episode 4-12 KB (Ep 4.6 is the largest at ~12 KB: ~90 lines + a 300-event beat script + a 320-op script), chapter common ≤ 6 KB, side chain ≤ 8 KB, the Feast ~10 KB.

### 16.3 Compression
A **static dictionary + canonical Huffman**, built offline over all text: the 512 most frequent words and phrases (with their trailing space: "the ", "you ", "Hee. Hee.", "{..}", "[face SAD]", "Don't make it weird", "it's fine", "beep.") become single tokens; literals are bytes; the token alphabet (≤ 768 symbols) is Huffman-coded with one global canonical table (lengths ≤ 16; table ~1.6 KB in ROM / resident on PS1). Measured on comparable English game text this gives 40-45% of raw; the plan budgets **50%**. Decode cost: a canonical table walk of ~15 cycles per bit; a 200-char line is ~1,000 bits = ~15k cycles on GBA (2.7% of a tick) once, when the line opens; the typewriter then reads the decoded buffer. The dictionary also lets the box-fitting test measure lines on the tokens' expansions. Scripts and beat scripts stay raw (dense binary; ~10% gain is not worth the decode path). If the ROM needed it, the same codec can be applied to the side-quest offer/hint/thanks of the 49 old quests; it is not needed.

### 16.4 GBA placement
Everything in §16.1 is `const` in ROM: packs as byte arrays (the pack table points straight into ROM, so "loading" a pack is a pointer assignment), scripts and beat scripts as arrays, art as today. **ROM:** 2,428,644 B today (measured) + ~0.8 MB housing + ~172 KB packed text + ~86 KB scripts + ~61 KB beat scripts + ~6 KB tables + ~9 KB songs + ~8 KB SFX + ~60 KB art ≈ **3.63 MB of the 4 MB file** (uncompressed text would be 3.80 MB: too close, which is why the packs are compressed on GBA too). **EWRAM (.bss) deltas:** VM state 64 B, beat scheduler 96 B (state + label table), `camfx` 48 B, two camera springs 32 B, text decoder state 16 B, NPC script slots 96 B (inside `OwScratch`, so 0 net), spawner cooldowns 32 B, the `STRY` live copy 96 B, the authored-possessor working row 24 B: **~400 B** net, plus `HOME` (2,609 B worst, `housing_final.md`; 3,367 B with v2, §17.2). Against the snapshot's 7,680 B free (measured) that leaves ~3.9 KB; against the memory pass's promised ≥ 24 KB it is comfortable. Removed: `story_script.o` .bss 72 B and the ad hoc cutscene state 16 B. **IWRAM:** the scaled span blit (~300 B) is the only new IWRAM code; or use affine OBJ and add none. The 12 KB `scene_scratch` hosts the housing editor, the siege, the Feast's guest table (friends list snapshot, 64 × 4 B) and the cutscene stage state.

### 16.5 PS1 placement (slot 111,376 B = 108.8 KB, measured; story group 49,776 B today)
* **Resident shrinks.** `story_script.c`, `story_side.c` and `story_talk.c` lose their resident text and rodata (27.7 KB text + 45.6 KB rodata, measured) because scripts and strings are pack data and the only callbacks left are the VM's own. New resident code: `story_vm` ~6 KB, `beat` ~3 KB, `tween` ~2 KB, `camfx` ~2.5 KB, text decoder + Huffman table ~3 KB, dialogue beat codes ~1.5 KB, spawner/wrongness ~1 KB, `home_api` ~1 KB (housing doc): **~20 KB**. Net resident **−53 KB**, which is most of what task #79 (PS1 RAM headroom ≥ 128 KB) needs.
* **The story group grows.** Scene code: today's 49.8 KB + cutscene stages and the beat-driven renderer (~8 KB) + NPC/camera scripting (~3 KB) + the Say It walk and the summit (~2 KB) + the Feast scene (~4 KB) ≈ **67 KB**, plus a **36 KB pack window** reserved inside the group's image (a `.bss`-like region so it costs disc nothing): one episode pack slot (≤ 12 KB), one chapter-common slot (≤ 6 KB), two side-pack slots (≤ 8 KB each, LRU), 2 KB of pack directory. **Total ≈ 103 KB < 108.8 KB.** The window is filled by raw CD reads from `TEXT.BIN` (a directory of 67 packs; `cd_read_range`), not by the overlay loader.
* **When packs load.** The episode pack at each `SET_BEAT` to a new episode (a read of 4-12 KB, ~0.15-0.2 s, at a moment that already has a chapter card, an iris or a cut); the chapter-common pack at each chapter; a side pack when a chain's giver first comes within talk range or its quest is active (prefetched while walking; the player never waits). When another group (combat, exorcism, home) takes the slot, the window is clobbered; on return the three reads already needed (`docs/PS1.md`) gain one more short read for the packs that were live (the VM remembers their ids). **No smash cut, insert, iris, dream or knock crosses a group** (same rule as the housing doc): cuts into combat, exorcism, cooking, concert, creator, pet and home happen only at episode boundaries or quest steps, never mid-gag.
* **Cutscene stages** reuse the overworld's billboard array and `plat_overworld` on small cell maps where they need 3D (the summit, the gate, the Spire top); flat stages (P0, F1-F4, the Bottle, the Feast hose shot) are 2D prims.
* **Disc:** `TEXT.BIN` ~330 KB (172 KB text + 86 KB scripts + 61 KB beats + directory), plus ~30 KB ADPCM, plus the housing group (~95 KB, housing doc). Against the 2 MB disc budget (task #82) that is ~0.45 MB more.

### 16.6 CPU
Per 30 Hz tick: the VM O(ops fired) ≈ 50-200 cycles; the beat scheduler ~50 cycles; springs 15 cycles per axis; a palette lerp step 5k cycles (GBA) only during flashbacks and inserts; a scaled blit 30k cycles only on wobble frames; text decode 15k cycles only when a line opens. Nothing here changes the heavy scenes' budgets (tasks #18, #35).

---------------------------------------------------------------------------------------------------
# PART 5 — SAVE DATA

## 17. Additions (both platforms; `save_ext` chunks; the 8 KB per-chunk cap and the 40 KB GBA story slot)

### 17.1 Flag bits reused (no new array)
The 28 reserved bits 228-255 become the **hot** one-shots that resident code reads every frame or at scene entry: `SF_ASKED_OTHER_SIDE`, `SF_VISITED_GRAVE`, `SF_PET_NUBS`, `SF_PET_HORNS`, `SF_PET_HALO`, `SF_HATTIE_LOST`, `SF_GRAMPY_LOST`, `SF_SMOG_LOST`, `SF_SMOG_NEAR_MISS`, `SF_TALLY_THAT_WAS_ME`, `SF_GRUMBLE_DEAD`, `SF_GRUMBLE_NO_RITE`, `SF_GRUMBLE_FRIENDLY`, `SF_HOUSE_PLATED`, `SF_HOUSE_TURRET`, `SF_HOUSE_HELD`, `SF_HOUSE_WALL_GONE` (the housing doc's four), `SF_WISP_AWAKE`, `SF_FOUNTAIN_TWOWAY`, `SF_MASCOTTY_HEAD_OFF`, `SF_FEAST_DONE`, `SF_SUITCASE_OPEN`, `SF_SIGN_1000`, `SF_BOTTLE_SEEN`, `SF_NGPLUS` (25 of 28; `SF_31_FREE`, `SF_VEX_REFORMED`, `SF_VAPULA_PLUSH`, `SF_STEAMY_LOST`, `SF_VAPE_SAMPLE`, `SF_WEAPON_BRUTAL` already exist). The spare "met" bits 36-39, 42-47, 48-55, 58-63 (26 bits) hold the Interlude III knock bits (8), the W1-W16 noticed bits (16) and two spares. The `BEAT_*` enum appends 36 ids (P0, 27 episodes, 8 specials) after `BEAT_DONE` and never reorders; the 28 old ids alias onto episodes.

### 17.2 New chunk `'STRY'` (v1; **96 B**; GBA live copy 96 B of `.bss`)

| Field | Bytes | Holds |
|---|---:|---|
| `ver`, `home_phase` | 2 | chunk version; COSY/HAUNTED/BARRICADED/EMPTY/OPEN/DREAM |
| `episode_done[5]` | 5 | 36 episode/special bits |
| `side2[30]` | 30 | 118 table rows × 2 bits (offered/active/done; retired rows stay 0) |
| `branch` | 1 | `wisp_answer` (2 bits: none/hummed/lie/chair), `sayit_line` (2), `feast` (2: none/saved/popped), `puff` (1), `brutal` (1) |
| `knocks`, `nests` | 2 | knocks received 0-8; nests cleared (4 bits) + Night Raids won (3) + hidden fifth nest (1) |
| `clues` | 1 | Ch2 clue board (5 bits) + Steamy/Hattie/Grampy/Smog rite attempted (3) |
| `wrong[2]` | 2 | W1-W16 noticed (duplicated from the met bits for the NG+ subtitle pass; one source wins on load) |
| `sayit_said[12]` | 12 | 96 bits: which pool and cast lines have been said (no repeats in NG+) |
| `heights[4]` | 4 | door-frame marks: Wisp, Pip, pet, Wisp's old mark (px) |
| `fountain_q8` | 2 | the giggle's lateness in frames Q8 (R8 → 0 after S109) |
| `feast_seed`, `feast_meter`, `feast_clues` | 4 | the culprit draw seed (from the save hash at quest start), 0-9 meter, 12 clue bits + 4 confirm bits |
| `grumble` | 2 | friendship (4 bits), romance state (4), dishes fed (4), pacifist (1), exes met (3) |
| `gag_cool[8]` | 8 | spawner cooldowns in minutes (so a reload does not fire everything) |
| `hee_count`, `tally_days` | 3 | S198's counter; Tallyworn's board (u16) |
| `pet_form` | 1 | nubs/horns/halo/glow/nap-on-command/bonnet-seen |
| `ngplus` | 1 | run count (2), subtitles on (1), Feast re-drawn (1) |
| spare | 16 | |
| **total** | **96** | |

Chain step counters are derived from the quest bits (each step requires the previous), so they cost nothing.

### 17.3 `'HOME'` v2 (additive to `housing_final.md` §9.2's 2,609 B)
Three more homes, each one Room record (10 B header + 40 × 6 B = 250 B): Hillvile Towers 7B, the Hell Pits flat, the Park Plot (+750 B); a visitor/rating log (8 B); `decorate_for` progress for Wisp's room (1 B). **Worst case 3,368 B.** Gift tags cost nothing: every gift is a unique catalogue row whose `giver` and `unsellable` are catalogue properties, not save state.

### 17.4 Totals against caps

| Slot | Today (measured) | + `HOME` v2 worst + `STRY` | Cap |
|---|---:|---:|---:|
| GBA story flash slot | 13,504 B | **16,968 B (16.6 KB)** | 40 KB |
| GBA free play | 59,148 B | 62,612 B (61.1 KB) (+ the customisation design's `LOOK`/`OPTX`/`UNL2` ≈ 71.3 KB) | 88 KB |
| PS1 memory card (story) | 2 blocks | 2 blocks (typical house ~500 B compressed; the one 3-block edge case is the housing doc's) | 15 |
| Per-chunk cap | — | `STRY` 96 B, `HOME` 3,368 B | 8,192 B |

Registry edit: `save_ext` becomes a 12-slot array (housing doc); `HOME` takes slot 5, `STRY` slot 6, the customisation design's chunks 7-9. The `size()` callbacks emit exactly what `write()` does; the test round-trips both.

---------------------------------------------------------------------------------------------------
# PART 6 — TESTING

## 18. How it is tested

### 18.1 Unit tests (`core/tests/`, run by `make -C core test`; all native, deterministic)

| Test | Checks |
|---|---|
| `story_full_playthrough_reaches_the_credits` (kept) | the whole arc through the real overworld and the VM under `story_debug_autowin(1)` (now covering combat, exorcism, cooking, concert, housing goals, the pet-care boss, the Say It walk and the welding QTE); A through dialogue and cutscenes; START through the credits; asserts the key flags, `home_phase` transitions, unlocks and maps per episode |
| `story_debug_jump_sets_up_every_beat` (kept, 36 new beats) | every episode and special has a debug setup, an objective and a reachable target; old `BEAT_*` ids alias correctly |
| `story_vm_scripts_valid` (new) | every script in every pack terminates under autowin within 10,000 ops; every jump target is inside its script; every flag/var id is declared; every text id exists in a pack the VM can have loaded at that point (episode, chapter-common or that chain's side pack) |
| `story_text_fits_the_box` (new) | every string renders at 240×160 with the real font metrics to ≤ 3 lines × box width per page (`/` and `//` honoured) and at 320×240 to ≤ 4; no page overflows; measured count today: 1 of 22 main-path lines fails and is fixed |
| `textpack_roundtrip` (new) | encode → decode every string byte-identical on the native codec; the GBA and PS1 decoders are the same C file; pack sizes ≤ their slot caps (episode 12 KB, common 6 KB, side 8 KB); `TEXT.BIN` directory offsets valid |
| `beat_script_timing` (new, **the lint**) | for every `BeatScript`: ≥ 1 `SHOT`/`CUT`/`CAM`/`ZOOM`; ≥ 1 `EXPR` per `SAY`; ≥ 1 `SFX` per visual op (POP/SLAM/DROP/SHAKE/FLASH/INSERT/CUT); ≥ 1 `MUSIC_MUTE`/`TAPESTOP`/silence; a `L_BUTTON` label 12-25 f after `L_PUNCH` **or** a `NOBUTTON` marker; every `HOLD` in a punch beat (after `L_PUNCH`) is unskippable; every `INSERT` is 3-4 frames; the gaps between consecutive anchored events sit within ±1 of a ladder rung for ≥ 75% of gaps (warn below 75%, fail below 60%); `3x` sequences have equal intervals ±1 |
| `silence_budget` (new) | per chapter, summed over its cutscene and scripted beat scripts: music-free frames are 20-25% of total; Interlude III is exempt (100%) and counted separately |
| `story_style_rations` (new) | per conversation ≤ 1 `{d}`; whole-game `{d}` count < 80; `NOBUTTON` ≤ 50% of sincere beats per chapter (sincere = tagged `HEART` in the line flags); no chain has two consecutive `NOBUTTON` endings; the string "Ten years I" occurs exactly 4 times across all packs; news-ticker "Later" strings ≤ 4 |
| `story_side_quests_table` (updated) | the old table: n between 40 and 50, as today; the new table: 118 rows, **109 live**, every verb used, ≥ 45 Hillvile and ≥ 35 Hellvile givers, chapter counts (17/19/18/32/23), every chain's steps monotone (step k requires k−1), giver and spot reachable within the chapter window, Ch3 Hillvile givers also on the invaded map, retired ids are `KIND_NONE`, `STRY.side2` has room (118 × 2 ≤ 240 bits) |
| `story_side_quests_play` (updated) | a sample of 12 missions through the real talk path with autowin: S100 (housing), S101 (escort/photo/timing), S118 (practice rite), S133 (reverse rite), S135 (three nights), S142 (timing), S164 (text entry), S166 (haunting), S183 (cooking + judge), S186 (both branches, seeded), S200 (no music), S206 (no comic beat) |
| `story_possessors_authored` (new) | the authored table overrides `sim_soul.c` for the listed hosts; the hash-picked side cases exclude the authored ids; FLUFFY completes Smog, VAPULA costs no strike, GIGGLESWORTH costs one; three fails lose the soul except scripted cases |
| `story_choice_consequences` (new) | each flag in §8.2 is written where the design says and read where it says (a table of write/read sites the test walks: e.g. `SF_VAPE_SAMPLE` → Ep 3.6 floor 3 caption, Ep 3.7 fog line, S135) |
| `story_save_round_trip_mid_story` (kept, extended) | `STRY` and `HOME` v2 round-trip at every episode boundary; `size()` == bytes written; worst cases exactly 96 B and 3,368 B; the story slot < 40 KB with the measured 13,504 B island |
| `story_grumbleguts_feast` (new) | with a fixed seed: the culprit is never a kid, the Mayor or Nox; the meter fills in 9 courses; Branch A leaves him alive and friendly; Branch B sets `RES_DEAD`, `SF_GRUMBLE_DEAD`, `SF_GRUMBLE_NO_RITE`, the headstone, the memorial dish, and `graveyard_rite(grumbleguts)` refuses |
| `story_pet_forms` (new) | nubs after Interlude I, horns after Ep 3.7, portrait variants exist for all 11 expressions × 3 forms |
| `tween_matches_trailer` (new) | `tw_bounce` LUT within 0.2% of `tk_bounce`; the spring presets' step responses match the fitted tables (from `story/tools/springs.py`); `TW_*` tables equal Appendix A |
| `story_overworld_maps_are_valid_and_reachable`, `story_every_target_and_npc_spot_is_reachable` (kept) | plus the interior maps and the outdoor plots (S126, S162) |
| `story_small_talk_has_something_to_say`, `story_flags_and_formatting`, `story_places_and_activities_menu`, `story_overworld_walks_and_collides`, `story_nest_encounter_fights_real_demons` (kept) | unchanged |
| `home_*` (housing doc §11.1) | unchanged; `home_defend_waves` gains the Ep 3.7 chimney beat and the HELD condition |

### 18.2 Runner scripts (`core/tests/scripts/`, played at 240×160 **and** 320×240 by `run_scripts.sh`; `# assert-no-stubs` on every one; shots compared by eye at both resolutions per the project rule)

| Script | What it plays and shoots |
|---|---|
| `story_p0.txt` | the prologue: `shot p0_string_slips` (f 188), `shot p0_wall` (f 300), `shot p0_egg_iris` |
| `story_ch1.txt` (extends the existing 137-line script) | every Ch1 episode via `scene STORY 1xx`: the bus brake frame, the Mayor's `[FREEZE]`, the key muffin insert frame, the first "beep." bubble (the smallest in the game: `shot beep_first`), the paper boat, the housewarming's ninth DROP, Mascotty at the window, CUT_FOG f 96 insert, the candle; `expect-scene HOME` at Ep 1.2 |
| `story_int1.txt` | the pet's 4-frame insert: `shot pet_insert_f2`, `shot pet_nubs` |
| `story_ch2.txt` | Cloud Nine (both puff branches), the Clues board, the chair rule of three (`shot chair_wall`, `shot chair_bed`), Steamy's head turn frame, the practice rule of three stamps, SOUL LOST, **the Bottle** (`shot bottle_hold`), the sign flip (`shot sign_dash`), D1, the Gate Cracks insert (`shot grumble_stuck`) |
| `story_ch3.txt` (extends `story_invasion`) | the church, wave 1 (`shot wisp_appear`), each nest's beat (`shot mayor_at_grave`), the Fuzz speech frame, the fountain crack, F3, the confession hold (`shot confession_hold26`), both exorcism skins, the seven Spire captions, the jar (`shot vex_jar`), the siege wave 3 freeze (`shot chimney_freeze`), the gate and the pill (`shot pill_first`) |
| `story_int3.txt` | the empty room in remove-only mode, each knock's DROP (`shot knock_1..7`), the Mayor's "Now." |
| `story_ch4.txt` | the gate toll (cupcake branch), the border scratch, Tallyworn's chalk hold, Shift Swap, each Favour (the frozen hammer: `shot anvil_hammer`), the Smog board with FLUFFY (`shot smog_names`), Wisp's room at opacity 8 and 16, D2's pixel of rain, Grumbleguts' intro (`shot grumble_gut_in`, `shot grumble_gut_out`), the Pump confetti hold, the pet turning its back, the summit ("Hi, Fluffy." `shot jar_rain`), Say It, CUT_WALL_FALLS (`shot wall_f40`), Mascotty's head, the buffet `[3x]`, the belt on the hat, the stage, the credits |
| `story_epilogue.txt` | E1's horned poop, the suitcase, the title on fire |
| `story_feast_a.txt` / `story_feast_b.txt` | the Feast with `seed 7`: clue rounds, the strap pings (`shot feast_strap3`), A: the exorcism and the trolley; B: "let's see what happens", the hose climb (`shot hose_climb`, `shot hose_crowd`), two false pops, the POP in CUTE and in BRUTAL (`opt gore 1`), the cheeks (`shot feast_cheeks`), the headstone, Brimstone Bites' menu |
| `side_sample.txt` | the 12 missions of `story_side_quests_play` with one shot each at their punch frame (S101's polaroid, S166's cursor insert, S183's tie, S200's facing ducks, S206's blown candle) |
| `side_chains.txt` | chain gating: a Ch2 part is not offered before its Ch1 part; cross-border part 3 is not offered before Ep 4.1; retired ids never appear in the quest log |
| `home_*.txt` (housing doc §11.2) | unchanged, with `home_defend.txt` extended to the Ep 3.7 chimney beat |

Runner commands used: `scene STORY <100+beat>` / `<200+beat>` (night), `push CUTSCENE n`, `press`, `hold`, `wait N`, `shot name`, `expect-scene`, `opt text_speed 3`, `opt gore 0|1`, `seed N` (new: fixes `story_rng`), `expect-text "..."` (new: asserts the top dialogue line), `expect-flag SF_X 0|1` (new).

### 18.3 Platform harnesses
* **GBA** (`gba/harness`, libmgba; `make -C gba test`): `gba/scripts/story_ch1.txt` and `story_ch4.txt` replay the runner beats on the ROM with `fps` labels: `fps cutscene_wall` (≥ 28 during CUT_WALL_FALLS), `fps summit` (≥ 28), `fps feast_pop` (≥ 24 with 60 confetti particles), `fps siege_w3` (≥ 20, the housing doc's bound); `profile 2000000 beat_tick`; `memreport` must show the `STRY` live copy ≤ 96 B, `HOME` ≤ 3,368 B, no new IWRAM symbol except the span blit (or none), EWRAM free ≥ the memory pass's figure minus 3.8 KB; `coresave` scripts extended with `STRY` so the streamed save round-trips through the 17 KB window.
* **PS1** (`ps1/harness/run.py`, PCSX-Redux, `--cue`): `ps1/harness/scripts/story.txt` plays Ep 1.1, 2.5, 3.7, 4.6 and the Feast; `expect-log "ovl: story"`, `expect-log "txt: pack ep46"` (the pack read at the episode boundary), `expect-log "txt: refetch 3"` after a combat return; `ovl_plan.py`'s report asserts the story group ≤ the slot and lists what stayed resident (acceptance: only `story_vm`, `beat`, `tween`, `camfx`, `textpack`, `home_api` and `story.c`'s dispatcher); `fps` ≥ 28 in Redux on the summit and ≥ 22 on the Feast pop; `dshot` of the DROP mid-squash frame and the iris hold; `memcard_check.py` confirms 2 blocks.

### 18.4 Determinism (so every shot above is reproducible and both platforms match)
* **One RNG for the story:** `story_rng(ctx)` is an xorshift seeded from `(save hash ^ day ^ beat ^ ctx)`; every hash-pick goes through it: the Feast culprit, Say It lines, nest region lines, visitor reactions, the spawner's picks and the starer's 1-in-20. No wall-clock anywhere. The runner's `seed N` overrides the save hash.
* **Frame-exact scripts:** every hold, gap and insert is an integer frame count in ROM data; the beat scheduler advances on `beat_tick()` once per 30 Hz tick; player pacing moves anchors only. Two runs with the same inputs produce the same frames on the same platform.
* **Platform parity:** `story_fingerprint()` is an FNV hash over `STRY`, the 256 flag bits, `home_phase`, the side table states and the pet form after a scripted run; `test_story` records goldens per script and the GBA/PS1 harnesses assert the same fingerprint after the same script (the same mechanism as `resident_fingerprint`). Per-frame pixel parity is not asserted (the renderers differ by design); timing parity is: the harness logs the tick at which each `L_PUNCH` label fires and the test compares it to the native run.
* **Autowin covers everything interactive:** `story_debug_autowin(1)` resolves combat, exorcism (including the typed name and the two-faith rite), cooking, concert, creator, the welding QTEs, housing goals and `decorate_for`, the pet-care boss and the Feast's clue rounds, so the full playthrough test needs no minigame input.
* **Text speed and options are fixed by the runner** (`opt text_speed 3`, `opt ui_bounce 2`, `opt shake 2`, `opt reduce_flash 0`) so holds are the only timing in play; a second pass runs `opt reduce_flash 1` and asserts that only amplitudes changed (the punch labels fire on the same ticks).

### 18.5 Docs to update when this lands
`docs/STORY.md` (chapter and episode list, 27 episodes, 109 + 49 quests, the three player lines, the retired ids), `docs/GAME_DESIGN.md` (play-time target 12-14 h main, 25-35 h with side content and housing), `levels.c` boss text for Grumbleguts and Vapula's seven floors, `docs/GBA.md` and `docs/PS1.md` (the `STRY` chunk, `TEXT.BIN`, the pack window, the resident delta), `core/tests/README` (the new runner commands), the trailer README's `FR(n)` warning gets a line: "the game does not do this; cues hang off beat labels".

*End of document.*
