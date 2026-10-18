# DOOMOGACHI LIFE in HELLvile Park! — STORY BIBLE, HEART-FIRST CUT

*Working title of the expanded story: "Somebody Left the Light On".*

Scope: a rewrite-and-expansion of Story Mode for both the GBA ROM and the PS1 disc. It keeps the world, every established name and the four-chapter backbone (Ch1 Welcome to HaHaland, Ch2 Something in the Vapor, Ch3 Hell Invades, Ch4 Hellvile Park, ending with everyone commingling in Hellvile Park) and deepens it into 24 episodes, a prologue, 4 interludes (2 dreams, 1 flashback, 1 dark night), an epilogue and a post-game. Target: 10-15 h main story, 18-25 h with the 84 new side missions, housing and the post-game. Every bracketed number is a 30 Hz frame count from `comic_timing.md`; every id is from the roster or `story_map.md`. Nothing here touches the repo. The director's notes in `user_notes.md` (Gatekeeper Grumbleguts, the Feast of Rebuilding) are folded in as hard requirements (§2.9, §11.3, §12.2).

---

## 0. How to read this document

### 0.1 Timing tags (from the Comic Timing Bible)

| tag | meaning | engine op (`beat.h` / `camfx.h`) |
|---|---|---|
| `[R2] [R3] [R5] [R8] [R13] [R21] [R26]` | gap on the beat ladder: BLINK, REACT, BEAT, TAKE, HOLD, PAUSE, DEADPAN | `at` offset |
| `[HOLD Nf]` | a hold A cannot shorten: the hold is the joke, or the grief | `BOP_HOLD` |
| `[WAIT A]` | the press ends the setup; the punch fires one R5 later | `BOP_WAIT_A` |
| `[MUTE Nf]` `[DUCK .5 Nf]` | music silent / ducked, sequencer keeps running, SFX still play | `BOP_MUSIC_MUTE/DUCK` |
| `[SCRATCH]` | tape-stop 5f + `SHK_BRAKE` + `TW_BRAKE` camera + `[MUTE 14f]`, re-enter on the grid | `BOP_TAPESTOP` |
| `[FREEZE x]` `[THAW]` `[PANIC]` | clamp / resume / 2x an actor's idle clock; hoppers hang in the air | `BOP_FREEZE/THAW/PANIC` |
| `[INSERT x Nf]` | 3-4 frame tonal-whiplash insert; owns the audio; return to the SAME shot with ONE change | `BOP_INSERT` |
| `[SMASH CUT]` `[COVER CUT]` `[FLASH CUT]` | hard cut with SFX on frame 0 / a cloud or paint roller hides the change / 1f white | `BOP_CUT` |
| `[IRIS OUT heart r26 hold 5f]` `[IRIS IN]` | the trailer's exit; the button lives in the hold | `BOP_IRIS_*` |
| `[POP]` `[SLAM]` `[DROP]` | TW_POP_BUBBLE / TW_SLAM_STAMP / TW_DROP_SOFT tables | `BOP_POP` |
| `[SQUASH 1.22/0.80]` | sx/sy on the contact frame, anchored at the feet | `plat_image_xform` |
| `[SHK_IMPACT]` etc. | shake profile, timing bible App. C | `BOP_SHAKE` |
| `[FLASH 10,5]` | flash levels 0..16 per frame | `BOP_FLASH` |
| `[!RIPPLE]` | "!" bangs over every head, staggered `2 + id%4` | `BOP_ACT` |
| `[EXPR Mayor=SHOCK]` `[EMOTE Pip SWEAT]` | portrait expression / overworld bubble on cue | `BOP_EXPR` |
| `[WALK x->cell]` `[HOP]` `[TURN]` `[FLEE]` `[PAN]` `[ZOOM]` | scripted actor / camera motion | `BOP_ACT`, `BOP_ZOOM` |
| `[DEADPAN]` | text appears (no pop, no hop), actor clock 0, music muted | caption style |
| `[TYPE]` `[POP TEXT]` `[APPEAR]` | setup lines type, punch lines pop, deadpan lines appear | caption style |
| `[BUTTON +Nf]` | the after-beat | - |
| `[NO GAG]` | a sincere beat with the button deliberately withheld | - |

### 0.2 Dialogue format

Lines fit the 240x160 box: at most 3 lines of about 28 characters. `/` is a line break inside one box; a new quoted string is a new box. `@P` player (default Sunny, bunnykin), `@E` pet, `@D` the authored possessor (App. A), `@V` flavour of the day. Speakers: NARR (no portrait), PET (vocabulary: "beep.", a horned "beep.", "bep?", and once, ONCE, something else), the cast by name. Choices are `> option`.

### 0.3 The four pillars

1. **Heart first.** Six relationships carry the game: you and your Doomogachi; Wisp the Hellvile kid and the mum on the night shift; Rivet and the family she welded herself away from; Father Fuzz and the faith he is stuffed with; Vex and the Duke who used him as a footstool; Mayor Giggs and Mother Nox, who signed the same Wall on the same night. Every scare and every gag is parked next to one of those.
2. **Scary is comedy with the release removed.** Same holds, same silence, same clamped idle clocks. The fog gets no punchline until Chapter 4.
3. **Funny is delivery.** The old script's jokes were fine on paper. The new one gives them ladder gaps, inserts, stings and buttons, and every big gag has a Gatekeeper-sized butt or a tiny "beep." somewhere near it.
4. **Long because it is connected, not padded.** Every episode changes the town, the house or a relationship, and its button lands minutes or chapters later.

### 0.4 Presentation standard ("look like the mockups")

The current build's scenes are captioned slideshows over flat tile maps (`story/shots/`). The target is the trailer's look (`story/frames/`, `trailer/tk_chibi.c`): plush chibis at 1-3x with eleven expressions, breathing on co-prime periods; captions that type, pop or slam; irises; palette rot; silence. Every episode below is written to that standard, and every beat sheet is a `BeatScript` in waiting. Minimum per scene: a camera move or a cut, one expression change per line of dialogue, one SFX on every visual beat, one silence, one button.

---

## 1. The world, deepened (the lore that was missing)

### 1.1 One park, two potlucks

Hellvile Park was one park. North bank: Hillvile folk (plush, pastel, polite). South bank: Hellvile folk (horns, bones, soot; also polite, slightly louder). Once a year the two mayors met at the bandstand in the middle: Mayor Gigglesworth brought cupcakes, Mother Nox brought bone soup, and the Potluck of the Park ran until the fireworks. Nana Puddin and Charlotte judged the bake-off. Chief Spark lit the bonfire. Kids from both banks swapped plushies and baby teeth. HaHaland is the land; the park is its heart; the Wall is the scar across it.

### 1.2 Fluffy

Young Gigglesworth had a pet cloud, Fluffy, who followed him to school and rained on bullies. One potluck night Fluffy drifted south chasing fireworks and never came back. There is a joke grave in the HaHaland graveyard: "Fluffy the cloud". Giggs put it there himself, aged nine. He does not talk about it. "Hee. Hee." is the sound of a man not looking at something.

### 1.3 Eleven years ago: the Duke sells a Wall

A cloud came back over the park: enormous, purple, crowned, calling himself Lord Vapula, Duke of the Ninth Cloud, with a contract devil named Signhere (567) and a nine-hundred-page contract. He told Giggs that Hellvile was coming north to eat everyone. He told Nox that Hillvile was coming south to "sanitize" everyone. Both mayors signed a Wall on the same night, each convinced they were protecting their own. Vapula built it through the middle of the park and kept the bandstand. The gate was welded shut by an apprentice imp named Rivet, age ten, who slipped through the last glowing gap and ran north with her torch still lit. Nobody recognised the cloud. **That is the secret under the game (Ch4 Ep6): the Duke is Fluffy.** A lost pet who grew up on nobody's attention, came home, and found that the only thing anyone would give him was fear. The only thing that hurts him is laughter. He has never been invited to anything.

### 1.4 The Pump, and what a demon is

Behind the Wall, Vapula turned Hellvile into a vapor works. Citizens pump "Cloud Nine" vapor at the gate on double shifts (Tallyworn has counted 3,652 days). Vapor is not smoke: the Pump draws a sliver of soul with every shift. Vapula bottles the strongest ones and sends them north as fog. **A possessing "demon" in Hillvile is a Hellvile soul in a bottle.** Exorcism does not destroy it: a saved exorcism sends the soul home across the Wall as a wisp; a failed one (three strikes) dissipates host and soul. The player learns this at the midpoint (Ch2 Ep5) and it recolours every exorcism in the game. The things you shoot are not citizens: cloud spiders, imps of the Ninth Cloud, thunderheads with teeth, the Breach mini-bosses (Gumdrop Gutterking, Foreman Fluffnuts, the Weeping Widow, Bishop Bonebottom) are Vapula's cloud-stuff, made of vapor and spite. They pop like balloons into grape goo (CUTE) or gibs (BRUTAL). Cartoon gore belongs to them and to the hell world. **No cute character is ever harmed on screen in the cute half of a whiplash.** The one demon who is also a citizen is Gatekeeper Grumbleguts (§2.9): hell-born, not cloud-born, and that is the point of him.

### 1.5 The Doomogachi

Nana's egg is an old Potluck egg: a plush-and-brimstone pet bred in the days of one park, half Hillvile, half Hellvile. It beeps. It grows horns under stress. It is the only creature in the game that belongs to both sides, which is why it is the player's voice, why the fog calls it by name in Chapter 3, and why its poop grows horns after the credits.

---

## 2. The cast: want, wound, payoff

| character | want | wound | payoff |
|---|---|---|---|
| **You (@P)** | a home | arrived alone with one suitcase; the game never says why; you say three lines in the whole game, each one earned | your house is the first place both towns sit down in |
| **@E the Doomogachi** | to be near you | half hell; the fog knows its name; Vapula calls it "little snack" and means "little brother" | beeps at the Duke; chooses you over the Ninth Cloud; walks the credits; its poop grows horns |
| **Mayor Giggs (0)** | everyone laughing | signed the Wall; buried a cloud at nine | walks through the gate; says "I signed it" out loud; says "Hi, Fluffy." |
| **Mother Nox (778)** | her potluck back | signed the other half; eleven years setting two bowls | "I LOVE soup." then, quieter: "You're late." |
| **Nana Puddin (1)** | to feed everyone | her pen pal Charlotte's letters stopped the week the Wall went up; she kept baking two of everything | the bake-off, "Burnt and Sweet" |
| **Pip (42) + Mr. Flops** | to be brave | thinks brave means not crying | lends Mr. Flops to Wisp for one night; "Rain's good for stuff." |
| **Father Fuzz (77)** | to hug the hurt out of things | stuffed with love "and a little bit of buckwheat"; believes nothing is unhuggable; in Ch3 a hymnal works where a hug did not | learns the hug was never for the demon; the two-faith rite with Nox |
| **Mr. Huggins** | (a plush dummy) | exorcised 400 times; torn in the Plushie Factory | stitched from scraps of both towns; a seat at the stage |
| **Rivet (13)** | to be a Hillvile person | welded the gate shut at ten; left Bolt in a crib; writes letters she sends by balloon and pretends never to send | cuts the gate open herself; "It's a REALLY good spoon." |
| **Anvil (595), Sprocket (596), Bolt (597), Nana Clank (598)** | Rivet back | a set place at dinner; a dad who "doesn't read" the letters; a sibling who learned to weld from them | dinner |
| **Vex (666)** | one "good job, Vex" | smallest demon on the Ninth Cloud; the Duke sat on him for three hundred years and calls him Vexy-Poo; practised the smile in a mirror because it was "too small" | a whole festival calls him Vex; plain water vapes; opens Cloud Ten (tea) with Haze |
| **Lord Vapula (999)** | to be feared | is Fluffy | deflates; is named; is invited |
| **Wisp (563)**, imp kid | Mum home | Ma Gloom (561) went to the night shift a year ago and did not come back; Wisp plays ghost around the house "so if Mum comes back as one she won't be scared of being one" | (saved) Ma Gloom drifts home; (lost) a candle at the Lake of Fire; either way, Mr. Flops for one night |
| **Pa Gloom (562)** | to keep the crypt and the snacks | tells Wisp "Mum's on nights" every night | stops saying it |
| **Steamy (2)** | a rubber duck in every tub | the first host | (saved) "Why do I taste grape?"; sends a duck south |
| **Hattie Haha (71)** | to be funny | possessed by Honkers (501), who honks at funerals; afterwards her laugh sounds like the fog's | a two-hander with Honkers at the Unity Stage |
| **Grampy Bean (88)** | tomatoes | possessed by Marrowmere (817), who gardens better than he does | a tomato in Hellvile soil; a shared bench |
| **Tallyworn (712), Shackles (604), Drudge (787)** | days down / a garden / a beach | the Pump | day 3,653; a garden; a beach |
| **Lady Smog (502)** | air | eleven years in a smog gown, then the Duke's last puff | "It's DELICIOUS." |
| **Haze (709)** | plants | works the Cloud Nine counter and hates the smoke | Cloud Ten's garden |
| **Candlewick (760)** | one candle to stay lit | 10,000 lit, 9,000 fingers; Nox's niece | the candle in Wisp's window |
| **Gatekeeper Grumbleguts** | a good meal and a friend (and everyone's phone number) | nobody has ever stayed for dessert | cook at Brimstone Bites; or the Feast (§11.3) |
| **Signhere (567)** | a signature | has never had one withdrawn | the contract is torn, 900 pages, by hand, by everyone |

### 2.1 The player's three lines

1. **Ch3 Ep5, the Longest Night:** the pet's name, into the fog. `@P: "@E!"`
2. **Ch4 Ep4, the crypt:** Wisp asks "Did she say anything?" Two true things and one kind lie; the game remembers which.
3. **Ch4 Ep6, the Open House:** `@P: "Come in."`

### 2.2 Fuzz's arc in four lines
Ch1: "Nothing is unhuggable." Ch2: "Okay. SOME things need holy water first." Ch3: "I hit a demon with a hymnal, @P. / It worked. The hug didn't. / What am I FOR?" Ch4: "The hug was never for the demon."

### 2.3 Vex's arc in four lines
Ch2: "Welcome to Cloud Nine! / Where every breath is a vacation!" (practised, too big). Ch3, sat on: "...yes, Your Dukeness." Ch3, later: "I left the hatch unlocked. / Don't make it weird." Ch4: "Plain water. They're terrible. / I love them."

### 2.4 Rivet's arc in four lines
Ch2: "Don't make it weird." Ch3: "I made the lock. I was ten. / It's a REALLY good lock." Ch4, at the gate: "...Mum?" Ch4, after: "Bolt. Hold the torch like this. / No. Like THIS. ...Yeah."

### 2.5 The Mayor's arc in four lines
Ch1: "We don't talk about the / other side. Hee. Hee." Ch3: "I signed it. Both of us did. / We never even READ it." Ch4, at the jar: "Hi, Fluffy." Ch4, stage: "Population: 1,001."

### 2.6 Wisp's arc in four lines
Ch4 Ep2: "BOO. ...did it work? / I'm practising." Ep4 (saved): "MUM? You smell like BUBBLES." Ep4 (lost): "Pa says she's on nights. / I know what nights means." Ep6: "Can Mr. Flops stay? He's / good at scary."

### 2.7 The pet's arc, in beeps
Ch1 hatch: "beep." (tiny bubble). Ch2, horns in its sleep: "beep." (same bubble, horns). Ch3 mid-invasion, demons flinch: "beep." (the fog echoes it). Ch3 Spire, in the jar, to the Duke: "bep?" Ch4 festival: "beep." (a whole crowd goes "awww" `[!RIPPLE]`). Post-credits: the poop says it.

### 2.8 Design brief: Lord Vapula
A balloon tyrant. A crowned purple cumulus with a tiny tornado tail, a grin too wide for his face, and a wheeze at the end of long lines because he is a balloon and knows it. Enters every scene from above. Hates laughter: a joke lands on him as a `[SHK_TAP]` and a 2 px deflate. Baritone babble. He is never kinky, never cheeky; that job belongs to the Gatekeeper. His crown always lands crooked.

### 2.9 Design brief: Gatekeeper Grumbleguts (director's notes, honoured in full)

**Role.** Keeper of the Sealed Gate, the real lock on the Wall (Rivet's weld is the physical one; his belly is the one that sits on it). Boss of Hell Breach 7 (Hellvile Underbelly, `LB_GRUMBLEGUTS`), now a story character: Vapula leans on him to crack the gate open in Ch3 Ep1 (the belly-flop that splits the sky), he is the comic antagonist of Ch4 Ep1-Ep5, and he is redeemed in Ch4 Ep5 (he just wanted a good meal and a friend) into a line cook at Brimstone Bites. Bestiary: *"Keeper of the Sealed Gate. Also of a very large seat."*

**Look** (refs in `story/refs/`): a chunky pot-bellied cartoon devil (round gut, stubby horns, fangs, enormous grin) with Baloo's loose, jolly, belly-forward posture; buff black-and-pink gym-demon shoulders, bat wings, a thick spiked tail (muscle on top of the gut: the EFFORT); and the Red Guy's big smooth cartoon butt, which he shows off for no reason at all (struts away camera-side, backs into frame, bends over to pick things up, admires it in mirrors, wiggles it as a taunt, goes butt-first through doors, sits on things and people). Glossier, more rubbery, sharper-lined than the plush citizens: he is a hell boss, slightly off the house style on purpose. Cartoon-devil-costume kink, modest: spiked collar, leather chest harness with too many buckles, studded belt and cuffs, chains, a riding crop as his gatekeeper's tool, the gate key on a chain. No nudity beyond smooth peach-shaped cheeks, no genitals, no acts. Belly AND butt always on show. Portrait, chibi, boss sprite, level-select preview, bestiary: all of it.

**Voice and the belly.** His belly has its own voice babble (low, wet, three sliders down from his) and its own opinions: always hungry, sometimes sides with the player, answers questions for him, interrupts his villain speeches (`[SCRATCH]` on every gurgle). He is hell's biggest flirt (Hazbin/Cuphead-devil energy): winks, pet names, "come up and see my gate sometime", a little black book the size of a phone directory, a dating profile ("Gatekeeper, 6'4" if you count the horns, loves long walks to the fridge"), exes popping up in cutscenes (side chain S94-S95, §12.6), bedroom eyes in the boss intro, "you're cute when you're scared". He flirts with every ADULT on both sides, the player, Father Fuzz (to Fuzz's horror), the Gate itself. Everyone turns him down (running gag: nobody is interested, except possibly the Gate). Hard boundaries: innuendo and implication only; nothing with Pip, Wisp, Bolt or any kid or teen, whom he treats like a grumpy uncle ("Go home. Eat something. Not THAT."); everything consensual and played for laughs. Mature-rated cheek, never graphic.

**Running gags (mostly cutscenes, comic-timing engine).** First seen mid-meal on a mountain of Brimstone Bagels (Ch3 Ep1). Monologue derailed by snacking. Squeezes through the gate sideways and pops through with a `[SQUASH 1.4/0.7]` BOING. Belly bounces the player like a trampoline (fight). Unbuckles his belt with a dramatic sigh before the fight. Sucks his gut in to look menacing, it springs back out `[SCRATCH]`. "Only had a light snack" while a forklift delivers the next course. Defeated, asks if there's dessert. Credits: still eating at the festival, buffet empty around him. His belly grows scene to scene and he never stops. Every butt reveal gets a `[SCRATCH]`.

**Fight mechanics** (HB7 and the Ch4 Ep5 version). The belly is the weak point; feeding it (thrown cupcakes, or Brimstone Bites dishes from cooking) pacifies it for a wave; belly-flop shockwave (jump it); it burps imps out of the portals; the butt-wiggle taunt is a telegraph (3f anticipation) for the sit-slam. Pacifist route: feed him five courses and he lies down ("USTORY_PACIFIST", finally granted).

---

## 3. The dream house: the emotional anchor

The cottage at Hillvile Lane (4,7) is the only space that changes in every chapter, and where the biggest sincere beats happen. It is also the decorating sandbox: isometric on GBA (sorted diamond tiles plus sprite stacks in mode 4; the held item an affine OBJ), 2.5D on PS1 (a small cell map via `plat_overworld`, furniture as textured quads/low-poly meshes, camera pitch about -45° indoors). Technical plan: `housing_tech.md`.

| chapter | state | decorating goal (the house grades you: `[SLAM stamp]`) | sincere beat that happens here |
|---|---|---|---|
| 1 | **Cosy.** Empty room, a crate, an egg. Nana's housewarming fills it. | "Make it yours": 6 items incl. a bed and the egg nest. Grade COSY. | the hatch; the first "beep." |
| 2 | **Haunted.** Grape smell. Things move one cell at night. A face in the wallpaper `[INSERT 4f]`. Balloon letters land in the garden. | "Keep the lights on": 3 lamps, candles count double. Grade LIT. | reading Rivet's family letters by lamplight; the pet's first horn, in its sleep |
| 3 | **Barricaded.** Welded plates on windows, neighbours sheltering, a turret made of a toaster. Defence waves at night. | "Hold the Door": barricade HP, 3 waves, 4 guests safe. Grade HELD. | the Longest Night: one wall gone, the pet missing, your first line |
| 4 | **Open.** The south wall becomes a door. Hellvile furniture with Hillvile. Two tables pushed together. | "Open House": seat 8 guests from both sides, mixed-style bonus. Grade HOME. | "Come in." Giggs and Nox at your table. Wisp asleep on Mr. Flops. |
| post | **Yours.** Two gardens, a plaque, a Hellvile annex. | weekly visitors rate it: three reactions 15f apart, escalating, then a topper | the morning the starer is at the window (1 in 20) |

Interactions use the trailer vocabulary verbatim (timing bible §6.4): place = `[DROP soft]`, THUD+BOING on contact, neighbours ripple `TW_JELLY` 2 frames late; invalid = 4f "nope" shake and a sad squash, no red flash; rotate = squash-flip with the edge-on frame; wallpaper = covered cut (a citizen runs across with a paint roller); cursed item = `[INSERT 4f]` then ONE persistent change ("the lamp grew horns") and "...it's fine." from a visitor at `[R8]`. Furniture comes from side missions (§12), both towns' shops after Ch4, Breach drops and the story (the Ch1 crate, Ch2 candles, Ch3 welded plates kept as "battle scars", Ch4 Hellvile pieces). Running-gag merchandise: the DAYS SINCE LAST FIRE sign (resets when Rivet visits), the horned-poop plushie, Kevin's three-eyed bust (only the third eye blinks, every 83+ frames), the Plain Human figurine (the only decor that never animates), the deflated Vapula plush (sighs when sat on), the crown that lands crooked however you place it, Grumbleguts' Big Chair (creaks; if a citizen sits in it they slide off).

---

## 4. Running gags and how they mutate (timing bible §3.17)

| gag | Ch1 | Ch2 | Ch3 | Ch4 | post-credits |
|---|---|---|---|---|---|
| "beep." | tiny bubble at the hatch | same bubble, horns | demons flinch; the fog beeps back | the crowd goes "awww" | the poop beeps |
| "it's fine." | Nana about the egg ("Mostly.") | a possessed neighbour, deadpan | on a sign in the ruins, hand-painted | Vex, about his tea | the title screen, on fire |
| the DAYS SINCE LAST FIRE sign | 7 at Rivet's (seen through the window) | 7 -> "-" -> 0 when you first visit | 0, in red, on fire | 1, with a cake | it counts real days; resets when Rivet visits your house |
| the starer | an apartment window, ground floor | a different window, higher | not there (his window is boarded; "!" over the boards) | at the festival, still; waves at the end | 1 in 20 mornings, your window |
| Kevin (three eyes) | creator preset; "Yes, three eyes." | only the third eye blinks during the possession scene | the third eye sees the nest first | the bust is in your house | - |
| the crooked crown | Vapula's silhouette in the fog cutscene | on Cloud Nine's employee-of-the-month board | lands crooked on the Spire | on the plush | furniture |
| Grumbleguts' butt | - | - | the belly-flop that splits the sky; three reveals | eight reveals, every one `[SCRATCH]`ed | the Feast |
| Hee. Hee. | the Mayor, twice | once, strained | none | replaced by "Hi, Fluffy." | "Hee. Hee." as a chuckle, finally a real one |

Rules: establish small, repeat nearly identically within 15-20 minutes of play, then change the context not the gag, let the last occurrence break the frame, and send every one-shot gag to the ending.

---

## 5. Structure at a glance

| part | episodes | main-path time | days | where |
|---|---|---|---|---|
| Prologue P0 *The Egg Crossing* | cutscene | 2 min | - | both banks, eleven years ago |
| **Ch1 Welcome to HaHaland** | 1.1-1.6 | 2.5 h | 1-4 | HaHaland, the house |
| Interlude I-A *Peek at a Dream: The Sunflower* | dream | 5 min | night 4 | the dream |
| **Ch2 Something in the Vapor** | 2.1-2.6 | 3 h | 5-9 | HaHaland, the house, the Cellar (HB1) |
| Interlude I-B *Flashback: Ten Years Ago Today* | playable flashback | 4 min | - | the park, one gate |
| **Ch3 Hell Invades** | 3.1-3.6 | 3.5 h | 10-12 (one long red night, then two more) | HaHaland invaded, Sewers (HB3), Factory (HB4), Graveyard (HB5), the Spire (HB2) |
| Interlude I-C *The Empty Room* | the dark night of the soul | 10 min | night 12 | the house |
| **Ch4 Hellvile Park** | 4.1-4.6 | 3.5 h | 13-17 | Hellvile, Bone Cathedral (HB6), Underbelly (HB7), the park |
| Interlude I-D *Peek at a Dream: A Small Cloud* | dream | 3 min | night 15 | the dream |
| Epilogue E *Days Since* | playable morning | 10 min | 18 | the park, the house |
| Post-game *Park Life* and *The Feast of Rebuilding* | free play + quest | 3-8 h | 19+ | everywhere |

Main path ~12.5 h without side content; the counters (C1_FRIENDS, C2_WEIRD, C4_PRISONERS, C4_FESTIVAL) are replaced by authored episodes. Silence budget inside cutscenes: 20-25%.

---

## 6. PROLOGUE P0: *The Egg Crossing* (cutscene, ~2 min, before the title's "New Game")

Purpose: plant Fluffy, the potluck and the egg in 2 minutes, as a children's-book memory, so Ch4 Ep6 can pay it off. Crayon style (the GBA draws it with `TEX_PASTEL_BRICK` scribble fills; PS1 with flat-shaded quads and a paper grain). No speaker portraits; the captions are in a child's hand.

| f | beat |
|---|---|
| 0 | `[IRIS IN circle]` on the bandstand. Two tables. A cupcake tower on one, a soup cauldron on the other. `[TYPE]` "Every year, the park had a potluck." Music: SONG_FESTIVAL, slow, music-box arrangement. |
| 90 | Kids run across the bridge both ways `[RIPPLE 0..3]`. A small bunnykin (young Giggs) with a cloud on a string. `[TYPE]` "Everybody came." |
| 180 | Fireworks. The cloud tugs the string, looks up `[EXPR cloud=LOVE]`. `[R8]` The string slips. `[HOLD 13f]` nobody notices. |
| 250 | `[PAN]` south with the cloud as the fireworks fade. `[MUTE 29f]`. `[TYPE]` "Not everybody went home." |
| 300 | `[SMASH CUT]` Eleven years later, same bandstand, a Wall through it. `[SHK_RUMBLE]`. SFX_THUD. The two tables, one each side. Two bowls on Nox's. One cupcake on Giggs'. `[APPEAR]` "Nobody talks about why." |
| 390 | `[COVER CUT]` fog. In it, a sheepkin (Nana) kneels at the river, pushing a paper boat north to south. Inside: an egg. `[TYPE]` "Except Nana. Nana sends eggs." `[R13]` A boat comes back the other way: a bag of burnt cookies. `[BUTTON +21f]` she eats one, `[EXPR SICK]`, then `[EXPR LOVE]`. |
| 480 | `[IRIS OUT heart r26 hold 5f]` on the egg. In the hold, the egg wobbles once. SFX_EGG_CRACK, tiny. Title. |

---

## 7. CHAPTER 1: *Welcome to HaHaland* (2.5 h, days 1-4)

Chapter card: pink polka, "Chapter 1" `[POP]`, title after 18f, sparkles. New: the card's last sparkle is purple and sinks instead of rising `[BUTTON +25f]`.

### Ep 1.1 *One-Way Ticket* (OW + CUT_ARRIVAL, 20 min)

**Purpose.** Arrive; meet the Mayor; learn the town's one rule; see the Wall from a distance. Plant Fluffy's grave and the starer.

**Beats.**
1. CUT_ARRIVAL, rebuilt: the bus drives in at constant speed (no ease-out) and `[SCRATCH]` brakes `TW_BRAKE` at the fountain; the player figure is thrown off the step, lands `[DROP soft]` `[SQUASH 1.22/0.80]`, suitcase lands 8f later and bounces. `[TYPE]` "Population: 500 lovely citizens." `[R8]` `[POP TEXT]` "And now, @P!" The bus leaves `[FLEE 2a+0.35a²]`. `[BUTTON +22f]` a single pink bus ticket flutters down and lands on the pet-less player's head.
2. Mayor Giggs at the fountain. The giggling fountain giggles every 7 s; the Mayor laughs with it, half a beat late, every time.
3. The rule. The Mayor points at everything except the Wall. The camera `[PAN]` toward it anyway, 0.04 px/f creep; he steps in front of the lens.
4. Nana has the key. On the way: the starer in a ground-floor apartment window `[FREEZE starer]`. Tutorial toast replaces itself: "Ignore that one."
5. Graveyard shortcut (optional, flagged if taken): "Fluffy the cloud" sign. The Mayor, if following, says nothing; `[EXPR Mayor=SAD]` for exactly `[R8]`, then `[EXPR HAPPY]`.

**Dialogue.**
MAYOR: "Welcome, welcome! Mayor / Gigglesworth. Friends call / me Giggs. Enemies don't exist!"
MAYOR: "Try the Brimstone Bites! / Named for a joke. It's / ironic! Hee. Hee." `[DUCK .5 6f]` on the Hee.
MAYOR: "Hellvile Park has two sides. / This one, and..." `[HOLD 13f]` `[EXPR Mayor=NEUTRAL]` `[FREEZE Mayor]` "...the weather side."
MAYOR: "We don't talk about the / other side. Hee. Hee." `[THAW]` `[BUTTON +22f]` the fountain giggles; he flinches.
NARR (at the grave, `[DEADPAN]`): "Fluffy the cloud. / Drifted. 11 yrs."

### Ep 1.2 *A Key in a Muffin* (OW + HOUSE, 25 min)

**Purpose.** Nana, the key, the egg; the house opens as an empty room; the decorating tutorial. Nana's "two of everything" planted.

**Beats.**
1. Nana's bakery: every tray has two of everything. One muffin has a key in it `[INSERT 3f]` x-ray of the muffin (cute hell: the key has tiny horns) `[R2]` SFX_TIK. "Don't bite. Hee. I mean: don't bite."
2. The egg from under the counter `[DROP]` on a cushion. Nana's "Mostly." lands `[R8]` after "Kidding." with `[EXPR Nana=NEUTRAL]` for exactly 8 frames.
3. Walk home. The pink door. `[IRIS IN heart]` r = 6 + 250 smooth², door `TW_JELLY`. The room is empty except a crate and a window.
4. Decorating tutorial: place the crate (it becomes a table), the egg nest, a lamp. First `[DROP soft]` THUD+BOING. First "nope" on the window. Grade COSY stamp `[SLAM]` when done. Nana, from the doorway: "Oh. Oh, it's a HOME now."
5. Night. The pet-less house. Through the window, a far purple glow at the Wall, 1 px, pulsing `[R21]` apart. Sleep. `[NO GAG]`.

**Dialogue.**
NANA: "The key's in the muffin. / Don't ask which. Bakers' / rules."
NANA: "And this is yours too. / A Doomogachi egg. Keep it / warm. Talk to it."
NANA: "The last one hatched into / a little demon. Ha! / Kidding." `[R8]` "...Mostly."
NANA (doorway): "You put a lamp by the egg. / That's exactly where my / mother put mine."

### Ep 1.3 *beep.* (HOUSE + SCENE_PET + CUT_HOME, 20 min)

**Purpose.** The hatch, on a downbeat. The first "beep." as a tiny bubble. The player gets a pet and, this time, a feeling.

**Beats.**
1. Morning. The egg wobbles, period 6f, then 3f (timing bible §6.6). SFX_EGG_CRACK on the music's beat. `[FLASH white 10,5]` `[POP TW_POP_EYE]` two eyes.
2. Hatch tutorial (SCENE_PET arg 0): feed, love, clean. The poop plops with a 3f squash and stink lines every 5f.
3. Back in the house: the pet hops to the lamp, sits in the light. `[HOLD 21f]` nothing. `[MUTE 29f]`. Then `[POP TW_POP_BUBBLE]` "beep." The smallest bubble in the game. `[DUCK .55 6f]`.
4. `[R8]` `[APPEAR]` NARR: "...it's fine." (The first use. The word "fine" will not be innocent again until Ch4.)
5. Outside: Pip crying by the fountain, heard before seen (voice babble from off-screen for 2 s).

**Dialogue.**
NARR: "@E is here!"
PET: "beep."
NARR `[DEADPAN]`: "...it's fine."

### Ep 1.4 *My Eyes Are Doing a Rain* (OW, 30 min)

**Purpose.** Pip and Mr. Flops; the beach; the first paper boat from the other side (Wisp's drawing). Pip is the heart of Ch1.

**Beats.**
1. Pip at the fountain `[EMOTE Pip SWEAT]` x3 at `[R3]` intervals. "My eyes are just doing a rain." The Mayor nearby pretends it's weather.
2. The beach. Mr. Flops is in the surf `[DROP]` with each wave, 2 px. Finding him: `[SLAM TW_SLAM_STICKER]` "FOUND!" sticker; the pet beeps at the sea `[R3]` later; a second shape in the surf: a paper boat.
3. The boat. Inside, a crayon drawing: a house, a candle in the window, a small imp with a bigger imp, and "ARE YOU THERE" in backwards letters. `[HOLD 18f]` `[MUTE 18f]`. The pet sits on it. `[EXPR pet=LOVE]`.
4. Pip's return: the skull sticker; "make friends with EVERYONE". Pip keeps the drawing: "Somebody lost this. / I'll keep it safe like / Mr. Flops." `[NO GAG]`.
5. Friends: replace the counter with three authored neighbours who each have a tiny two-beat gag: Kevin (new creator preset, three eyes: "Yes, three eyes." `[R5]` only the third blinks), Bonejangles (xylophone ribs, plays a chord when hugged), Miss Lulla (the daycare kids all shout "BEEP" at the pet `[RIPPLE]`).

**Dialogue.**
PIP: "Mr. Flops went to SEA. / He can't swim. He's / mostly cotton."
PIP (found): "He's damp. But he's still / smiling his stitched little / smile." `[R8]` "Like me."
PIP (boat): "That's not my drawing. / That's somebody's HOUSE."
KEVIN: "Yes, three eyes." `[R21]` "Nobody ever asks about / the third one. It's lonely."

### Ep 1.5 *Housewarming* (HOUSE + OW, 25 min; absorbs C1_CUPCAKE)

**Purpose.** The town comes to your house: the first decorating goal becomes a party. Nana's cupcake run to Miss Lulla at dusk. The Mayor's toast slips.

**Beats.**
1. Nana's cupcake delivery (day) then "Come home at six."
2. The party: nine citizens arrive `[DROP]` one at a time at 8f intervals, each with a gift (furniture): Nana a rug, Pip a drawing (wall item), Fuzz a cushion ("cuddle-proofed"), Kevin a bust of himself, Bonejangles a xylophone end-table, Miss Lulla a nightlight, the Mayor a framed photo of himself, Dr. Bloom a thermometer ("decorative"), Rivet (unseen; a welded coat hook left on the step with a note "don't make it weird").
3. Place them live: each placement gets a crowd reaction `[RIPPLE]` and a grade re-roll. Three reactions 15f apart, escalating; topper: the pet climbs onto the bust.
4. The toast: MAYOR "To @P! To new friends! / To fluffy—" `[FREEZE Mayor 13f]` `[MUTE 13f]` "—cupcakes. Fluffy cupcakes." `[THAW]` `[DUCK .5]` Nana looks at him for exactly `[R8]`.
5. The pet beeps toward the window. Everyone laughs. Outside, the Wall glow has moved one cell closer. Only the pet saw.

### Ep 1.6 *The Light by the Wall* (OW night + CUT_FOG + HOUSE, 30 min; the first real scare)

**Purpose.** The fog at night. The Mayor drags you away. The first horror hold with no release. Ch1 ends on a drawing and a light.

**Beats.**
1. Miss Lulla's hint ("the fog giggles at night") sends you to the Wall after dark. Music drops to HAHALAND_NIGHT, then to nothing by cell x=50 (`[DUCK]` by x-cell, matching the purple tint creep).
2. The fog trigger (55,24). Choice: peek or back away. Either way: CUT_FOG rebuilt. Seven puffs drift. Two red eyes `[POP TW_POP_EYE]`. `[HOLD 21f]` nothing. A giggle, which is the fountain's giggle, pitched down 15 semitones. "come and PLAY with us". The second "us" is a child's voice. `[INSERT 4f]` a crowned silhouette, enormous, with a tiny shape sat on it. `[MUTE 64f]` (the s03 length).
3. Back: same shot, one change: the pet's eyes are red for 2 frames, then normal. "beep." `[DEADPAN]`.
4. The Mayor arrives `[FLEE]` reversed (running TO you), grabs your sleeve. "It's just... weather. / Hellvile weather." `[EXPR Mayor=SHOCK]` held through the line; `[EXPR HAPPY]` snaps on at the last word. Walk home together; he hums the potluck song and stops when he notices.
5. Home. On the crate: a second paper boat Pip pushed under your door, with a note in Pip's hand: "ANOTHER ONE CAME. / IT SAYS HELLO." Inside: a candle, unlit, and a drawing of the same house with a light in the window. Place the candle (it becomes furniture; it can be lit). `[IRIS OUT heart hold 10f]` on the window; in the hold the far purple glow blinks once in reply. END OF CHAPTER 1. Unlock: USTORY_CH1_DONE, the song, the "it's fine." sign.

**Dialogue.**
FOG (child's voice, `[APPEAR]`, small): "are you there"
MAYOR: "Nothing to see! Nothing / to hear! It's just... / weather. Hellvile weather."
PIP's note: "ANOTHER ONE CAME. IT SAYS / HELLO. I DIDN'T OPEN IT. / (I opened it.)"

### Interlude I-A: *Peek at a Dream: The Sunflower* (SCENE_DREAM, forced on night 4, 5 min)

The pet's dream, rendered in the LCD style of the trailer's s04 at 10 px per pixel. A sunflower under a gate. A big imp hand waters it. A smaller hand holds a lunchbox. The flower faces north. The LCD pet walks toward it and `[INSERT 4f]` pentagram `[R2]` "beep." `[R8]` "...it's fine." Wake up: the pet has 1 px of horn. The game does not mention it. (Sootkin's sunflower and Ma Gloom's lunchbox, planted.)

---

## 8. CHAPTER 2: *Something in the Vapor* (3 h, days 5-9)

Chapter card: vape puffs drift; "Chapter 2" pops; title after 18f. New: one puff has two red dots in it for 3 frames `[INSERT 3f]`.

### Ep 2.1 *Grand Opening* (OW + TALK, 25 min)

**Purpose.** Vex and Cloud Nine. The vape choice now matters. Vex's tells: he is lonely and scripted.

**Beats.**
1. "A few days pass" is now played: a montage of three mornings `[SMASH CUT]`x3 at 20f each; each morning the pet has more horn and the purple glow is closer; the third morning a shop has appeared by the Wall overnight with a ribbon and a crowned cloud on the employee-of-the-month board.
2. Vex's pitch is a performed routine: he `[HOP]`s on every exclamation; the smile is drawn 2 px too wide and snaps back `[R3]` after each line (he practised it). Haze at the counter waters a plant and does not look up.
3. The choice: decline / one puff / "Where are you from?" (new). The puff: `[INSERT 4f]` the pentagram over the player's LCD HUD face, grape goo tint; `SF_VAPE_SAMPLE` now unlocks a Ch2 dream variant and a Ch4 line from Vex ("You were nice to me. / Even then. Even high.").
4. "Where are you from?": `[FREEZE Vex 13f]` `[MUTE 13f]` "...WAY out of town." `[THAW]` `[BUTTON +22f]` the crowned cloud on the board blinks.
5. Leaving: Haze, not looking up: "The plants hate it too."

**Dialogue.**
VEX: "WELCOME to Cloud Nine! / Where every breath is a / vacation!" `[HOP]`
VEX: "Grape! Cherry! Sunset / Serenity! Mystery Mauve! / ...Mystery Mauve is grape."
VEX (asked where from): "I'm from out of town." `[HOLD 13f]` "WAY out."
HAZE `[DEADPAN]`: "The plants hate it too."

### Ep 2.2 *Things Under the Wallpaper* (HOUSE + OW, 35 min)

**Purpose.** Replace the C2_WEIRD counter with a playable investigation that is ALSO the haunting of your house. Rule of three across three nights.

**Beats.**
1. Day: three authored clues, any order. (a) The fountain giggles on the wrong beat (its 7 s period is now 6.5; the Mayor laughs on the old beat; nobody else notices; the player can stand there and feel it). (b) Hattie Haha's set at the Concert Hall: her punchlines arrive one `[R8]` late and the laugh track is the fog's giggle; she thanks a heckler nobody heard. (c) Miss Lulla's "wand": the kids are playing "exorcist" with a vape pen; Fuzz confiscates it and sneezes grape.
2. Night 1: the pet stares at the wardrobe. `[FREEZE pet]`. "beep." A chair has moved one cell. `[DEADPAN]` NARR: "It was not there before."
3. Night 2: the lamp is off. Lighting it: `[INSERT 4f]` a face in the wallpaper hearts, every heart an eye. `[R2]` SFX_TIK. Back: one heart is still an eye. It blinks every 83f. The pet grows its first real horn in its sleep `[R13]` `[APPEAR]` "beep." The decorating goal LIT unlocks: 3 lamps.
4. Night 3: a balloon lands in the garden with a letter tied to it, addressed "RIVET (DO NOT OPEN) (OKAY OPEN)". It is from Bolt, age 8: "I did my first bead. / It's ugly. Nana Clank says / ugly is strong." You cannot deliver it yet. Place it on the crate: it becomes furniture (the first of 6 letters).
5. Morning: Dr. Bloom examines the pet. Cold hands. "Horns. Hm." `[R21]` "Perfectly normal. / For some things."

**Dialogue.**
HATTIE: "So I said to the fog, / 'who's there?'" `[HOLD 8f]` (the fog, off: "hee") "...Thank you. You're too / kind. Whoever you are."
FUZZ (sneezing): "GRAPE. Bless me. / No, actually, bless me."
DR. BLOOM: "Cold hands, warm heart, / cold heart too actually. / Open."

### Ep 2.3 *Steamy Isn't Home Right Now* (OW + TALK + exorcism practice, 30 min)

**Purpose.** The first possession, staged; Fuzz; the practice rite on Mr. Huggins; the midpoint set up.

**Beats.**
1. The plaza, midday, music up. Steamy is handing out rubber ducks. `[SCRATCH]` on the third duck: `[FREEZE all]` `[FLASH 7]` 1f shutter. Steamy's idle clock is the only one that keeps running, at 1.7 rad/f ear twitch. `[!RIPPLE]` 14f later. "Steamy isn't home right now. / We moved in." `[DEADPAN]` text with `{d}`.
2. The crowd `[FLEE]` with sweat drops every 3rd frame. The Mayor, from under a bench: "Get Father Fuzz!"
3. Fuzz's church: "Stuffed with love and a little bit of buckwheat." Mr. Huggins on the altar with 400 pins in him. The practice: prayer rhythm, holy water, a true name from four. Fuzz cheers each phase `[POP TW_POP_BURST]` heart burst that keeps breathing. On completion: Mr. Huggins' head falls off `[DROP]` `[R8]` "He does that. He's fine."
4. Fuzz's faith speech: `[NO GAG]` for 34f, then the button: a pin pings off him SFX_TINK +6, +10, +13.
5. Back to the plaza at dusk: Steamy sits in the fountain. The ducks float around him in a ring, all facing him.

**Dialogue.**
STEAMY (possessed, `[DEADPAN]`): "Steamy isn't home right / now. We moved in." `[R13]` "Leave a message after / the scream."
FUZZ: "Nothing is unhuggable, @P. / Nothing. Some things just / need holy water first."
FUZZ: "A demon is a frightened / thing in a loud coat. / You take the coat off."
MR. HUGGINS' head, `[APPEAR]`: "(thud.)"

### Ep 2.4 *The True Name* (SCENE_EXORCISM + CUT, 30 min: THE MIDPOINT TWIST)

**Purpose.** Exorcise Steamy. The name is authored: GLOOM. The twist lands: the demon was a mum on the night shift.

**Beats.**
1. The rite, for real. The clues for the name phase are a lunchbox, a gardening glove, a night-shift lamp, and a child's drawing of a house with a candle: the player has SEEN that drawing (Ep 1.4). The four names: Honkers, Lady Smog, Ma Gloom, Scallywag.
2. Failures use the rule of three verbatim (timing bible §3.11): spray, hug, bucket; stamp slams 15-16f apart; third gets red flash and 3 kazoo notes.
3. **Saved:** `[FLASH white 10,5]`, Steamy coughs a wisp. The wisp does not vanish. It looks at the player `[HOLD 13f]`, looks south `[R8]`, and goes, slowly, over the Wall. `[MUTE 41f]`. Steamy: "Why do I taste grape?" `[R26]` "Was... was someone IN here? / She was humming."
4. **Lost:** the candles go out one per frame. "SOUL LOST." `[SLAM]`. Steamy becomes a ghost (translucent, keeps the bath-house). The wisp does not go south. NARR `[DEADPAN]`: "Nothing went home."
5. CUT (new, "The Bottle"): Fuzz, Steamy/ghost-Steamy and the player; Fuzz holds the empty vape bottle up to the light. Inside: a tiny lamp, a tiny lunchbox. `[HOLD 21f]`. "@P. That was a PERSON." The pet beeps, once, very small. `[IRIS OUT circle hold 5f]` on the bottle. No button.

**Dialogue.**
FUZZ: "The name phase. Say what / it is, not what it's / wearing."
FUZZ (after, saved): "She went HOME. Over the / Wall. ...They go home."
FUZZ (after, lost): "I held on too long. / I always hold on too long."
STEAMY (saved): "Why do I taste grape?" `[R26]` "She was humming. A lullaby. / I know it. It's OURS."
NARR: "@D was a citizen of / Hellvile. A mother. On the / night shift."

### Ep 2.5 *Don't Make It Weird* (OW + SCENE_WORKSHOP, 30 min)

**Purpose.** Rivet, the weapon choice, the letters, and the DAYS SINCE LAST FIRE sign. Her family becomes real through the balloon letters.

**Beats.**
1. Rivet's workshop. The sign says 7. `[WAIT A]` on entering `[R5]` the tail of her welding torch sets a rag on fire; sign flips 7 -> "-" (2f jitter) -> 0, red, pulsing every 4f. "...It's fine."
2. The weapon choice: Bubble Wand or Pump Shotgun. New: a third option, "Can you fix a lock?" which she refuses `[FREEZE Rivet 13f]`.
3. Deliver Bolt's letter (Ep 2.2). She reads it with her back turned. `[HOLD 26f]` `[MUTE 26f]`. Sparks from the torch she is not holding. "Ugly is strong. Yeah. / Nana Clank says that." `[R8]` "Don't make it weird."
4. The workshop scene. Rivet's quips now include: "Back in Hellvile I welded / gates. One gate." `[R13]` "I was ten."
5. On the way out: she gives you a toaster turret, unasked ("for the house. For nothing. For no reason."). Vex's note under the door: "Your Vex guy slipped into / his cellar with a glowing / crate." The letter count on the crate: 2.

**Dialogue.**
RIVET: "Cute or brutal. Pick. / I don't judge." `[R26]` "I judge a little."
RIVET: "Don't make it weird."
RIVET (about the sign): "Seven was a record."

### Ep 2.6 *Kisses, V* (Hell Breach 1 + CUT_ERUPTION, 40 min)

**Purpose.** The cellar, the crate, Vex's half-warning, the eruption. Grumbleguts is introduced in the cutscene, mid-meal.

**Beats.**
1. The hatch under Cloud Nine. Haze looks up for the first time: "It's unlocked. / It was always unlocked." The Vape Cellar (HB1), with Vex's voice on the tannoy doing customer service during the fight ("Thank you for choosing / Cloud Nine for your / impending doom!").
2. Boss: the glowing crate is a Pump. Breaking it releases six wisps that go south. Win.
3. The note on the crate: "Too late, Hillvile. The big / cloud is already rising. / Kisses! - V". Under it, in different, smaller writing: "(get your pet indoors)". The pet is not with you (it waits at the hatch). `[SHK_RUMBLE]`.
4. CUT_ERUPTION rebuilt: pink -> purple -> blood sky bands; the cloud swells and grins; the crown lands crooked. On the far side of the Wall a shape squeezes through the gate sideways `[SQUASH 1.4/0.7]` BOING: Gatekeeper Grumbleguts, mid-bagel, mountain of bagels behind him. He looks at the camera. He turns around to reach for another bagel, slowly. `[SCRATCH]`. The Duke, enormous, sits on him. Grumbleguts' belly flops forward and the SKY CRACKS from the shockwave `[SHK_CRACK]`. "!!!" `[SLAM]`. SFX_EXPLODE, SFX_DEMON_ROAR. In the fog: five silhouettes and one tiny crowned cloudlet (Vex) being sat on.
5. `[SMASH CUT]` your house. The pet in the window. The pet's horns are full size. "beep." END OF CHAPTER 2.

**Dialogue.**
VEX (tannoy): "Please hold. Your doom is / important to us."
VAPULA (first words, through the crack): "LITTLE SNACK." `[HOLD 18f]` "I can SMELL you."
GRUMBLEGUTS (muffled, under the Duke): "I had a light snack."
BELLY: "grrrbl."

### Interlude I-B: *Flashback: Ten Years Ago Today* (CUT, ~4 min; playable last 40 s)

Rivet's memory, triggered the night after the eruption when she visits your barricade-to-be. Crayon style again. A ten-year-old imp with a torch too big for her; Nana Clank's voice: "Ugly is strong." Signhere's contract rolling across the park like a carpet. Two mayors signing with their eyes shut. The gate: the player welds the last bead (a 40 s workshop mini-sequence; the bead is ugly). The gap glows. A tiny Rivet looks back at a crib (Bolt), then runs north. `[MUTE 64f]`. The torch is still lit when she arrives; the sign at the empty workshop says 0. `[IRIS OUT circle hold 10f]`; in the hold, Sprocket picks up the first balloon letter and pretends not to read it.

---

## 9. CHAPTER 3: *Hell Invades* (3.5 h, days 10-12)

Chapter card: embers, red flash every 60f, title jitter. New: the card is drawn on the back of Signhere's contract; page "1 of 900" in the corner.

### Ep 3.1 *The Sky Ripped* (OW invaded + HOUSE, 30 min)

**Purpose.** The town turns to DOOM. The church of survivors. Fuzz throws a hymnal. Grumbleguts' second reveal. The house gets its first welded plate.

**Beats.**
1. Forced map switch to HaHaland-Invaded at night: red pulsing tint, embers, ash, INVASION. Only 1 in 3 citizens; the rest hide. The starer's window is boarded `[!]` over the boards.
2. The church: the Mayor under a pew; Nana handing out two of everything; Miss Lulla's kids chanting "it's fine" as a game. Fuzz at the door with a hymnal, out of breath. He just hit a cloud spider with it. `[HOLD 18f]`. He does not look at the hymnal. He puts it down very carefully.
3. The Mayor's speech ("The sky RIPPED") is now interrupted `[SCRATCH]` by a gurgle from outside: Grumbleguts walking past the window, backwards, butt first, eating. `[SCRATCH]` again when he bends to pick up a dropped bagel. Reveal 2 of 3. Kids: "BEEP" at him; he waves like a grumpy uncle: "Go home. Eat something. / Not THAT."
4. Rivet arrives with plates: your house becomes a shelter. Decorating goal HOLD THE DOOR unlocks: windows take plates, furniture becomes barricade HP, the toaster turret gets a slot.
5. Nests on the minimap: plaza, beach, park, graveyard. Each one is a story (Ep 3.2).

**Dialogue.**
MAYOR: "The sky RIPPED. Everyone's / in the church. The organ / is... also in the church."
FUZZ: "It worked." `[R13]` "The hymnal. It just... / worked."
GRUMBLEGUTS (through the window, winking at Nana): "Evening, sugar. Nice buns." `[SCRATCH]` NANA: "They're MUFFINS." G: "I'll allow it."

### Ep 3.2 *Four Nests* (OW combat on the map + HB3 + HB5, 60 min)

**Purpose.** The nests are set pieces, not counters. Each nest belongs to someone, and clearing it changes their line for the rest of the game.

| nest | whose | set piece | comic beat | heart beat |
|---|---|---|---|---|
| Plaza | Hattie Haha | the fountain spews grape; cloud spiders on the stage; the laugh track plays on loop | every kill gets a rimshot; the 10th a sad kazoo | Hattie is on the stage, possessed, still doing the set, to nobody (set up for Ep 3.3) |
| Beach | Pip | Pip's sandcastle fort with Mr. Flops on the battlements; waves of imps from the surf; the pier collapses under a brute `[DROP]` | Mr. Flops "fires" a shell: a seashell `[SLAM]` "pew." | Pip is not crying; Pip is scared and doing it anyway; "Rain's for later." |
| Park | Grampy Bean | the tomato plot; Grampy possessed, gardening at 2x `[PANIC]`; the plants are fine, better than fine | the demons are eating the tomatoes and leaving reviews ("4 stars. seeds.") | Marrowmere, inside Grampy, waves at the player the way the bench-sitter waves (set up for Ep 3.4) |
| Graveyard | Bonejangles | reach it through the Sugar Rush Sewers (HB3, Gumdrop Gutterking); the joke graves open; Count Snoozula's grave snores; "Fluffy the cloud"'s grave is the nest's centre: the fog pours OUT of it | Bonejangles plays the xylophone on his own ribs to keep the kids calm; a brute joins in | the Mayor is there, alone, at the grave, in the middle of the nest, not hiding. He has a shovel. He does not use it. |

Graveyard Shift (HB5, the Weeping Widow, "She Lost Her Plushie in 1887") is a night-only optional breach from the same map; clearing it returns her plushie (furniture: the Widow's Plushie, cries at 3 AM, comforts the pet).

**Dialogue (graveyard).**
MAYOR: "I'm not hiding. I'm... / visiting." `[R21]` "Clear them off him, / would you."
PET (first time demons flinch): "beep." `[R3]` the nearest imp `[EXPR SHOCK]` and backs off. The fog, from the grave, beeps back, lower.

### Ep 3.3 *Knock Knock* (SCENE_EXORCISM Hattie + CONCERT, 30 min)

**Purpose.** Hattie's exorcism, authored: a two-hander with the demon inside her (Honkers), which is the funniest and then the saddest scene in Ch3. Fuzz does not come; he sends holy water with a note.

**Beats.**
1. The Concert Hall stage. Hattie possessed, mid-set, to an audience of cloud spiders who honk. The name clues: a red nose, a funeral programme, a confetti cannon, a bottle of Cotton Candy. Names: Honkers, Snitchet, Femurfly, Hazey.
2. The rite plays over her set. Each failure is a heckle from Honkers in Hattie's voice; each success is Hattie getting a line of her own back. Prayer phase = call and response with the laugh track.
3. Saved: the wisp honks once, bows, goes south. Hattie: `[HOLD 21f]` "...he was funnier than me." `[R8]` "Don't tell anyone." Lost: the laugh track keeps playing in the empty hall; the game leaves it on for the rest of Ch3 whenever you pass. 
4. The note from Fuzz, tied to the holy water: "I can't come. I'm sorry. / Throw it, don't pour it. / - F." `[DEADPAN]`.
5. Button: on the way out the confetti cannon fires by itself, one streamer. `[BUTTON +22f]`.

**Dialogue.**
HATTIE/HONKERS: "Knock knock." PLAYER CHOICE: > Who's there? > ... > (throw water)
HATTIE/HONKERS: "Not Hattie." `[HOLD 13f]` honk.
HATTIE (saved): "Two comedians, one body. / We had NOTES for each / other."
HATTIE (saved, later, idle): "My laugh sounds like the / fog now. I'm working on / it. Working on it."

### Ep 3.4 *Grampy's Tomatoes* / *Fuzz Goes Dark* (SCENE_EXORCISM Grampy + HB4 Plushie Factory, 45 min)

**Purpose.** The second authored exorcism, gentle; then the Plushie Factory, where Fuzz loses Mr. Huggins and his faith.

**Beats.**
1. Grampy's exorcism in the tomato plot. Clues: a century-old bench, a wave, bone broth, a fossil. Names: Marrowmere, Grizzlebolt, Dreadie, Wrenchley. The rite is quiet; Marrowmere, inside, helps (the holy-water phase has no wind). Saved: the wisp sits on the bench for `[R21]` before it leaves. Grampy: "Tomatoes are better." `[R13]` "I'm furious."
2. Fuzz's church is empty; a trail of stuffing leads to the Plushie Factory (HB4). Fuzz went alone to get Mr. Huggins' "spare" (there is no spare).
3. HB4 with Fuzz as an NPC escort who refuses to fight and then does: Foreman Fluffnuts' line "we make 'em, we don't / mend 'em" `[SLAM]`. The boss tears Mr. Huggins in half on the conveyor: `[INSERT 4f]` real stuffing, then cute: it's buckwheat. `[MUTE 41f]`.
4. After the boss: Fuzz sits on the conveyor with half a plush in each hand. THE SPEECH `[NO GAG]`: "I hit a demon with a hymnal, @P. / It worked. The hug didn't. / What am I FOR?" `[HOLD 26f]`. He hands the player the holy water and the bell. "You do them. I'll... / I'll watch the kids." He walks home `[FLEE 1.1a²]` reversed: slowly.
5. Button, withheld in the factory. It lands 10 minutes later: the church bell rings at the wrong time; he is ringing it for the kids' bedtime.

**Dialogue.**
GRAMPY (possessed, `[DEADPAN]`): "Your soil's acidic. / We're fixing it."
FUZZ: "Buckwheat. He was always / buckwheat. I KNEW that."
FUZZ: "Faith is a stuffing. / You don't notice it until / something tears."

### Ep 3.5 *Hold the Door* (HOUSE DEFENCE, 40 min) and Interlude I-C: *The Empty Room* (the dark night of the soul, 10 min)

**Purpose.** The siege of your house; the loss of the pet; the Mayor's confession; the player's first line.

**Beats (defence).**
1. Night falls. Guests: Pip and Mr. Flops, Miss Lulla and two kids, Nana, Kevin, the Mayor (who insists on standing by the window "as a decoy"). Furniture is barricade HP. Three waves at the pink door; the toaster turret; a brute at the window `[SQUASH 1.3/0.8]` against the plate. Between waves: thirty seconds of quiet in which the guests talk (authored; Kevin's third eye watches the chimney).
2. Wave 3: the sky beeps. The fog comes down the chimney. `[FREEZE all]` `[MUTE 14f]`. The pet, alone, is unfrozen. It walks to the fireplace. "bep?" The fog, in the pet's voice, lower: "beep." The pet goes up the chimney. `[FLASH 7]`. `[THAW]` `[PANIC]`. Wave 3 is the longest.
3. Dawn. Held (or not: a wall is gone either way; HELD grade stamps if 4 guests safe). The room, by daylight: a hole, soot, the egg nest empty.

**Beats (Interlude I-C, no music for the whole 10 minutes).**
4. The guests leave one by one, `[DROP]`s in reverse. The Mayor stays. He sits on the crate. The confession `[NO GAG]`, with the crayon style bleeding into the room's edges for 2 frames every `[R21]`: "I signed it. Both of us did. / We never even READ it." `[HOLD 26f]` "I had a cloud once." `[HOLD 26f]` "He followed me to school."
5. The player walks the empty house. Every item they placed can be examined and has one sad line. The candle from Ep 1.6 can be lit. Lighting it is the trigger: outside, the trail: horned poops, one per 4 cells, toward the Spire. The player stands at the hole in the wall. The box opens with the player's name pill for the first time. `[WAIT A]`. `@P: "@E!"` `[R5]` far off, very small, from the top of the Spire: "beep." `[IRIS OUT circle hold 10f]`; in the hold, the candle does not go out.

**Dialogue.**
MAYOR: "He rained on bullies. / Only bullies. He was very / precise."
MAYOR: "I put a stone up. I was / nine. I never told anyone / what it was for."
MAYOR: "Go get yours back, @P. / Don't put up a stone."

### Ep 3.6 *Come Up, Little Snack* (OW + Hell Breach 2 + CUT_VAPULA_DOWN, 45 min)

**Purpose.** The Spire. Vapula taunts for the whole climb. The pet in a jar. The pet chooses you. The deflation. Vex changes sides.

**Beats.**
1. The climb: Vapula's voice between floors via `lvd_boss_intro` quips, now eight of them, escalating from menace to needy ("You dare climb MY spire?" ... "Nobody climbs it usually." ... "Did you bring anyone?"). Each floor's loot is one of Signhere's contract pages.
2. Floor 4: Vex, sat on by a brute, tied to a chair with his own shop ribbon. Freeing him: "I left the hatch unlocked. / Don't make it weird." He gives you the key to the jar.
3. The top. The pet in a jar on the Duke's throne. Vapula's speech to the pet `[NO GAG]` for 34f: "You're one of US. Half of / you is. They'll never love / the other half." `[HOLD 21f]`. The pet looks at the Duke. Looks at you. "bep?" Looks at the Duke. `[HOLD 13f]`. Then it turns around in the jar and sits with its back to him. `[SCRATCH]`.
4. The fight: Vapula's boss bar; laughter mechanic: every joke-drop (Hattie's rimshot pickups) deals 2 px of deflate. Phase 3: he inhales for 10f (anticipation), holds 5f with puffed cheeks, and the pet beeps: he laughs by accident. FWOOMP.
5. CUT_VAPULA_DOWN: pain, then "Pfffffffff." for 64 frames with the crown landing crooked at f 48; the sad little squeak at the end. He does not die: a small grey cloud the size of a cupcake drifts to the floor. Vex scoops him into the jar. Then, very carefully, puts the lid on and pokes air holes. `[BUTTON +22f]` the jar sighs. END OF CHAPTER 3. The pet rides on your head down the stairs.

**Dialogue.**
VAPULA (floor 1): "You DARE climb MY spire?"
VAPULA (floor 3): "...You're still coming? / Most people stop at three."
VAPULA (top): "LITTLE SNACK. You came / home."
PET: "bep?" `[HOLD 13f]` (turns around)
VEX (with the jar): "He's small now." `[R26]` "He was always small. / He just had a lot of air."

---

## 10. CHAPTER 4: *Hellvile Park* (3.5 h, days 13-17)

Chapter card: half purple checker, a beating heart. New: the heart has a tiny horn, and a tiny halo, and both are crooked.

### Ep 4.1 *The Gate* (OW + welding + CUT, 30 min)

**Purpose.** Crossing. Rivet admits the lock. Grumbleguts is the gate's bouncer. The border record scratch.

**Beats.**
1. The town heals (HaHaland map back; the "it's fine." signs stay). Fuzz is at the church door, not inside. Rivet at the Wall gate (55,30) with her torch: "I made the lock. I was ten. / It's a REALLY good lock." `[R26]` "Hold this."
2. Welding mini-sequence: cut your own bead (the flashback bead, mirrored). The gate swings. Behind it: a belly. Grumbleguts, backwards, leaning on the gate, reading his little black book. `[SCRATCH]`. He turns around slowly. `[SCRATCH]`. Reveal 1 of 8 for the chapter.
3. His toll: "Nobody passes without / giving me their number." Choices: > Give a fake number > Give him a cupcake > Ask about the belt. Cupcake: BELLY "grrrbl." G: "...The belly says yes. / The belly outranks me."
4. Crossing: `[SCRATCH]` the border tape-stop, 14f silence, then HELLVILE on a downbeat. The camera brakes `TW_BRAKE`. The pet's halo/horn tint flips.
5. Tallyworn at the Pump, chalking "3,652". He looks at the open gate. He puts the chalk down. `[HOLD 26f]` `[MUTE 26f]`. He writes "3,653" anyway. "Habit." `[DEADPAN]`.

**Dialogue.**
RIVET: "Hold this." (the torch) `[R13]` "Don't make it— yeah."
GRUMBLEGUTS: "Well HELLO, Hillvile. / Come up and see my gate / sometime. This is my gate."
GRUMBLEGUTS (to Fuzz, who followed): "And who is THIS stuffed / little..." FUZZ: "No." G: "Playing hard to get. / I respect it."
TALLYWORN: "Habit." `[R21]` "Tomorrow I'll write / something else. I don't / know what yet."

### Ep 4.2 *Five Hundred Other Hellos* (OW Hellvile + TALK, 40 min; replaces the C4_PRISONERS counter)

**Purpose.** Hellvile thinks you're the monster. Meet Wisp at the gate with a lantern. Mother Nox. The Hellvile neighbours are mirrors of the Hillvile ones (Charlotte/Nana, Scorchmello/Crumbelina, Honkers/Hattie).

**Beats.**
1. Citizens `[FLEE]` from you on sight with sweat drops; shutters slam `[RIPPLE]`. The first who doesn't is Wisp, standing in the road in a sheet, who says "BOO." `[HOLD 8f]` "...did it work? I'm / practising."
2. Wisp's house: a candle in the window (the drawing). Pa Gloom: "Mum's on nights." Wisp, after he's gone: "I know what nights means." `[DEADPAN]`. The lunchbox is on the hook. Saved branch: the lunchbox is GONE and the bed is unmade and Pa Gloom is in the kitchen humming and pretending nothing has changed because he cannot believe it yet. Lost branch: everything is as it was.
3. Three authored hellos (replacing "befriend 5"): Charlotte (burnt cookies, "Nana still sends eggs?" `[EXPR LOVE]`), Shackles the hellhound (dreams of a garden; shows you a seed in a matchbox), Honkers (saved branch: "Tell Hattie: the knock / knock was MINE.").
4. Mother Nox at her chapel. Two bowls on the table. She sets a third without looking. "Sit." The potluck song, hummed, the same one the Mayor hummed in Ep 1.6. She stops when she notices too.
5. The three favours are set (Ep 4.3). Button: on the way out the pet beeps at the second bowl; Nox, not turning: "That one's not yours."

**Dialogue.**
WISP: "You're from the OTHER side. / Are you a monster?" > Yes > No > beep. (the pet answers for you) WISP: "...okay. He's cute. / You can stay."
PA GLOOM (saved): "She's... upstairs. She came / in last night. Over the / Wall. Like weather." `[HOLD 21f]` "I haven't gone up yet."
NOX: "Eleven years I set two / bowls. Don't ask. Eat."

### Ep 4.3 *Three Favours* (OW, three sub-stories in any order, 60 min)

**Favour 1, Sootkin's sunflower (the dream from I-A).** Fetch Hillvile flowers; the sunflower under the gate; Sootkin has watered it on every shift with half her water ration. Replant it in the open. `[NO GAG]`. Button 2 minutes later: it turns to face the Mayor's side, and then, slowly, back. "It can't decide. / Neither could we."

**Favour 2, Lady Smog (the two-faith exorcism).** She is possessed by the Duke's last puff; her true name phase has no Hellvile names, only one: the clues are a crooked crown, a lost string, a school bag, a gravestone. The four options: Vapula, Fluffy, The Duke, Nimbus. Only "Fluffy" works; the player knows it from I-C. Fuzz is fetched by Nox herself ("You're the stuffed one. / We have the same rite. / Ours goes backwards."). The rite is played by both: Fuzz's prayer rhythm forwards, Nox's backwards, the same melody meeting in the middle (SONG_EXORCISM played forwards and reversed on two channels). Fuzz's line after: "The hug was never for the / demon." `[R13]` "It was for the one holding / the demon." Lady Smog: "I haven't tasted air in / eleven years. It's / DELICIOUS." The puff goes into the jar with the rest of him.

**Favour 3, Anvil (expanded to its full size).** Rivet will not come. Then she comes. The forge. Anvil at the anvil with her back to the door, hammering. Rivet in the doorway. `[HOLD 26f]` `[MUTE 64f]`. Rivet: "...Mum?" Anvil's hammer stops mid-swing `[FREEZE Anvil]` and stays up. `[HOLD 21f]`. "...RIVET?!" `[THAW]`. They hug "like two vices closing. Sparks fly." Then: the table. A plate set. It has been set every night for eleven years. Sprocket "not reading" a letter by the stove. Bolt with an ugly bead on a chain. Nana Clank: "Told you she'd come. Told / EVERYONE." Dinner is played: pass the dishes (a 1-minute cooking minigame variant). Rivet, with a spoon: "It's a REALLY good spoon." `[R8]` `[EXPR Rivet=LOVE]` for the first time in the game. `[NO GAG]`.

### Ep 4.4 *The Crypt and the Annex* (OW + HOUSE, 40 min)

**Purpose.** The Wisp branch pays off. Your house opens south: the Annex.

**Beats.**
1. Pa Gloom asks you to the crypt. Saved: Ma Gloom is upstairs, in bed, still a little translucent, humming. Wisp has not been let in. The player brings Wisp up. Wisp at the door `[HOLD 26f]`. "MUM? You smell like / BUBBLES." Ma Gloom: "I was in a bath-house, / love. A nice one. Tell / Steamy thank you." `[NO GAG]`. Lost: the Lake of Fire. Candlewick lights a candle that stays lit for once. Wisp asks the player's second line: "Did she say anything?" > "She was humming." (true) > "She said your name." (the kind lie) > "She hummed the lullaby." (true, if Steamy told you). The game remembers; the epilogue line changes.
2. Either branch: Wisp sees Pip's drawing in your bag (you kept the first boat). "That's MY house. / You KEPT it?" The pen pals meet (Pip has come through the gate with Nana): the two kids trade plushies. Pip: "He's good at scary. / One night. I'll be fine. / My eyes might do a rain."
3. Anvil and Rivet weld an Annex onto your cottage's south wall (covered cut: sparks fill the screen for 22f). The Annex is Hellvile-styled; the seam between the halves is a visible ugly bead. The decorating goal OPEN HOUSE unlocks with Hellvile furniture and the mixed-style bonus.
4. Cursed-item tutorial: Charlotte gives you a pentagram rug (plush). Placing it: `[INSERT 4f]` the room hellified, back with ONE change (the lamp has horns), "...it's fine." from Kevin at `[R8]`.
5. Vex opens Cloud Ten (tea) in the old shop with Haze. Plain water vapes. "They're terrible." `[R26]` "I love them."

### Ep 4.5 *The Pump* (Hell Breach 7, Hellvile Underbelly, + OW, 45 min)

**Purpose.** Shut the Pump down with Tallyworn, Shackles, Drudge and Rivet. Grumbleguts' boss fight and redemption. Signhere's contract.

**Beats.**
1. The Underbelly entrance is Grumbleguts' seat. He unbuckles his belt with a dramatic sigh `[HOLD 13f]`. Bedroom eyes `[EXPR G=LOVE]`: "You're cute when you're / scared." He sucks his gut in. `[HOLD 8f]`. It springs back `[SCRATCH]`. Boss intro card: "GATEKEEPER GRUMBLEGUTS / Keeper of the Sealed Gate. / Also of a very large seat."
2. The fight (§2.9): belly-flop shockwaves, imp burps from the portals, the butt-wiggle telegraph before the sit-slam, the trampoline. Feed him cupcakes or cooked dishes to pacify the belly for a wave. Pacifist route: five courses and he lies down (USTORY_PACIFIST).
3. Defeated or fed: "...Is there dessert?" `[R21]`. The belly, quietly: "grrbl." He admits it: nobody has ever stayed for dessert. He was guarding a gate nobody wanted open because it was the only thing anyone asked him to do. Nox, arriving: "Brimstone Bites needs a / line cook. You'd have to / wear a shirt." G: "...A TIGHT shirt?" `[SCRATCH]`.
4. The Pump room: the souls still in it (twelve bottles). Rivet cuts the main valve; Tallyworn, Shackles and Drudge turn the wheel; Vex, arriving late, opens every bottle by hand. Twelve wisps go up through the cracks in the roof toward both towns. `[MUTE 64f]`. Signhere appears with the contract: "This is in breach of / page 412." Everyone tears one page. Nine hundred pages. It takes `[HOLD 26f]` of confetti.
5. Button: Grumbleguts, in a tight shirt, carrying the forklift's next course. "Light snack."

### Ep 4.6 *Hi, Fluffy* (OW + CUT_WALL_FALLS + the stage + credits, 50 min)

**Purpose.** The summit, the naming, the Wall, the festival, the ending that makes them cry and laugh, in that order, then both.

**Beats.**
1. Fetch the Mayor. "You walked through demons / for us. I can walk through / a gate." His hands shake on the handle; Rivet's bead is still warm. Grumbleguts holds the gate: "Mayor. Love the hat." MAYOR: "It's my head." G: "I know."
2. The summit at the bandstand, both tables. The old exchange, played straight and faster: "I brought cupcakes." "...I brought bone soup." "...I LOVE soup." `[R13]` NOX, quieter: "You're late." `[HOLD 21f]` MAYOR: "I signed it." NOX: "So did I." `[NO GAG]`.
3. The jar. Vex puts it on the table between the bowls. A grey cloud the size of a cupcake. The Mayor looks at it for `[HOLD 26f]` `[MUTE]`. The crayon style bleeds in for 2 frames. "...Hi, Fluffy." `[HOLD 26f]`. The cloud rains, once, inside the jar, two drops. `[HOLD 26f]`. The pet beeps. The crowd `[RIPPLE]` "awww". The Mayor laughs: "Hee. Hee." and it is, for the first time, a real one, and the music comes back on the grid.
4. CUT_WALL_FALLS, rebuilt: Rivet cuts her own lock; Fuzz and Nox sing the rite forwards and backwards; the bricks fall with staggered gravity (keep the existing best animation) and now Grumbleguts belly-bumps the last stubborn section `[SQUASH 1.4/0.7]` and the cast runs in from both sides. Hearts rise. `[NO GAG]`, 34f straight. Then the button: his belt pings off and hits the Mayor's hat. `[SCRATCH]`.
5. The festival (replaces the C4_FESTIVAL counter with authored pairs you can watch: Hattie & Honkers' two-hander, Grampy & Marrowmere on a bench, Nana & Charlotte's bake-off "Burnt and Sweet", Steamy handing Ma Gloom a duck, Kevin and the starer side by side, the starer waving, slowly). The stage: the Mayor, Nox, Pip ("And Mr. Flops!"), Wisp ("And Mum." / or "And the candle."), Fuzz ("Even you, Vex." VEX: "...thanks for saying Vex."), Rivet ("Hit it!"), Grumbleguts at the buffet, not listening, the buffet empty around him. MAYOR: "Population: 1,001." (the pet, halo and horn, on his hat). The credits parade, 500 pairs, led by you and the pet; Vapula's jar is carried by Vex; "Everyone is together in Hellvile Park." Then the Open House: your table, both towns, "Come in." (line 3).

**Dialogue.**
MAYOR: "I brought cupcakes." NOX: "...I brought bone soup." MAYOR: "...I LOVE soup."
MAYOR: "...Hi, Fluffy."
VAPULA (jar, `[APPEAR]`, tiny): "(drip.)" `[R8]` "(drip.)"
FUZZ: "It was never for the demon."
PIP (stage): "And Mr. Flops! And Wisp! / And my eyes are doing a / rain. Rain's good for stuff."

### Interlude I-D: *Peek at a Dream: A Small Cloud* (night 15, 3 min)
The pet dreams the Duke's dream: a small cloud at a wall, waiting. The LCD pet walks up and sits next to it. Nothing happens for `[HOLD 26f]`. Then the LCD cloud rains one pixel. Wake up: the jar on your shelf has a tiny halo. The game does not mention it.

---

## 11. EPILOGUE and POST-GAME

### 11.1 *Days Since* (playable morning, 10 min)
CUT_FREEPLAY ("THE END ...of the beginning!"). Morning 18 in the united park. Tallyworn's board says "1". Grumbleguts asleep in the buffet. The DAYS SINCE LAST FIRE sign at Rivet's says 1 with a cake on it. Wisp's epilogue line depends on Ep 4.4's choice (if you lied kindly, Wisp says "I know she didn't. / Thanks anyway."). Post-credits stinger, in the LCD style: the pet's poop `[POP]`, strains for 8f with 1 px jitter, plops, `[R8]` grows horns, `[R6]` "beep." `[APPEAR]` "(it's fine.)" The title screen is now on fire (the "it's fine." logo variant unlocks).

### 11.2 Post-game structure (*Park Life*)
Free roam on the united park plus both towns. All 5 original post-game quests plus the new post-game quests (§12.7: S118-S133). Weekly house visitors who rate the room. NG+ ("Both Sides"): start a new story with the pet's horns and halo, the Annex already welded, and every "beep." bubble replaced by the crowd's "awww"; Hellvile citizens are friendly from Ch1 if you were friends before; the starer appears in your window from day 1. Unlocks: United Park, Hellvile Visits, Cloud Ten, the Gatekeeper's Big Chair.

### 11.3 *The Feast of Rebuilding* (post-game quest, director's note, 45-60 min)

**Setup.** Grumbleguts gathers EVERYONE in Hellvile Park and begs for a giant feast "for energy, to rebuild the town faster" (he claims). The guests and helpers are your actual friends list and party citizens (the sim's relationship data): they bring dishes (cooking), set tables (housing verbs in the park), tune the band (studio). A visible FULLNESS meter on his belly ticks up with every course; it tracks the belly in front AND the butt behind.

**The twist.** One guest is possessed (the culprit is drawn from the citizens present). Clue-gathering among the guests (each has a line; the possessed one's idle clock is 0 and their ears twitch at 1.7 rad/f if you stand still for `[R21]`) leads to the true-name phase of one last exorcism. The player must exorcise them BEFORE the meter fills. The player may also choose not to (ignore it, fail the timer, or pick "> let's see what happens") and watch the outcome.

**Branch A: exorcised in time.** The feast ends happily; he is merely too full to move for a day; reward "Hellvile's Biggest Spoon" trophy plus furniture (the Big Chair, the Buffet Table). Unlock/trophy.

**Branch B: the outcome cutscene (optional title card: "What an Ass").** The visual thesis of the scene is his butt: he eats with his back to the crowd and camera; low angle from behind the crowd; every eye-line on it; it grows with each course; the camera keeps "accidentally" cutting back to it `[SCRATCH]` each time.
1. Course after course. Belly up, butt up, both on the meter. A chair gives way `[DROP]`. The band plays faster `[PANIC]`.
2. The possessed guest sneaks behind the crowd into direct view of the butt, turns to the camera, wiggles their eyebrows at the viewer, thumb pointing at it: "Look who's full of / himself, now?" `[HOLD 13f]` `[SCRATCH]`.
3. The hose. A dedicated shot: side-on medium, his butt filling one edge of frame, the culprit on the other, feeding an absurdly long hose in like a REVERSE ROPE CLIMB: hand over hand, pushing it away and up, feet braced, tongue out, grunting, the coil on the ground shrinking. The music drops to a single plucked note per pull. Rule of three: pause, "surely that's enough" caption `[APPEAR]`, it keeps going. Cut to the crowd watching in silence `[MUTE]`. Back to the climb. The insertion is offscreen behind the cheeks: hose motion, a squeaky SFX, and his slow dawning look over his shoulder `[EXPR G=SHOCK]` at `[R8]`. Then the valve crank.
4. Inflation: squash and stretch, buttons popping `[RIPPLE]`, harness straps pinging off SFX_TINK, the chair, the pants' seams; the butt balloons biggest of all; a last gurgle from the belly, BELLY: "grrrbl...?" Two false "about to pop" beats `[HOLD 13f]` x2 (rule of three), then the POP: CUTE gore = confetti, plush stuffing, hearts, a halo; BRUTAL = a red shower and gibs, the belly landing last, still gurgling. The butt is the LAST thing to go: a lone pair of cheeks drifts down like a balloon with a slow raspberry (cute) / lands with a squelch (brutal). The culprit's final "well, that happened" look at the camera `[BUTTON +22f]`.

**Consequence of B (for the rest of that save file).** Grumbleguts is DECEASED and NON-INTERACTABLE: status RES_DEAD; a graveyard plot with a comically oversized headstone and a plaque "He was full of himself"; his apartment goes empty; his dating profile shows "deceased"; bestiary/almanac entry updates; a flag blocks any ghost rite from bringing him back; citizens who were his friends grieve and mention it; the Brimstone Bites menu gets a memorial dish ("The Light Snack"). Both branches are unlocks/trophies. Tone: good-natured slapstick body horror, cartoon and non-sexual: a portrait of vanity and gluttony punished.

---

## 12. SIDE MISSIONS: 84 new quests (S50-S133), 19 chains, 11 cross-border chains

The 49 existing quests (#0-48 in `story_map.md` §8) stay and are re-timed with the dialogue beat codes. The new ones follow the timing bible §5 rule: **one trailer scene = one quest arc** (SETUP 30-60 s, ESCALATION of three, a PUNCH at trailer density, a BUTTON minutes later when you pass the giver again or a prop in their house or yours changes). Every quest changes something persistent: a line, a prop, a piece of furniture, a relationship.

### 12.1 Format

`**Sxx · Title** — giver (roster id, district) · chapter window · verbs` then Hook / Beats (3-6) / Comic beat / Heart or Scare beat / Reward. Chapter 5 = post-credits. Verbs: talk, fetch, deliver, photo, find, visit, combat, exorcism, welding, pet, minigame, fishing, cooking, concert, studio, housing, creator, escort. Rewards list H$ first, then items, then furniture (F:), then unlocks (U:). Dialogue samples fit the 240x160 box (3 lines x ~28 chars; `/` = line break).

### 12.2 Chain index

| chain | quests | sides |
|---|---|---|
| WISP (the grieving kid) | S96 S97 S98 S99 S133 | Hellvile, cross with Pip |
| HATTIE & HONKERS | S56 S69 S100 S118 | cross-border |
| SPOONS (Rivet) | S52 S70 S86 S111 | cross-border at the end |
| FUZZ & MR. HUGGINS | S59 S72 S85 (+ #14) | Hillvile |
| PIP | S55 S84 S119 S120 | cross with Wisp |
| MAYOR GIGGS | S58 S74 S90 (+ S118) | Hillvile |
| NANA & CHARLOTTE (pen pals) | S57 S71 S106 S127 | cross-border |
| GRAMPY BEAN & MARROWMERE | S62 S73 S89 (+ #ep 3.4) | cross-border |
| DR. BLOOM & NURSE LEECH | S60 S88 S107 | cross-border |
| TOCK & TALLYWORN | S61 S77 S101 | cross-border |
| STEAMY & MA GLOOM | S67 S92 S116 | cross-border, branch-aware |
| VEX | S63 S64 S114 S129 S130 | Hellvile-born, Hillvile shop |
| KEVIN (three eyes) & ODDSBALL (one) | S51 S122 S121 | cross-border |
| THE STARER (Haybale, 35) | S50 S82 S123 | Hillvile |
| HAUNTESSA'S HOUSEKEEPING (decorating) | S54 S68 S91 S115 | cross with Pharaohnie |
| **FEED THE GATE** (Grumbleguts, director's note) | S102 S103 S104 S105 | Hellvile, then §11.3 |
| GRUMBLEGUTS' EXES | S94 S95 | Hellvile |
| HUGSLEY & BIG SNARL | S93 S131 | cross-border |
| SOOTKIN & SHACKLES (gardens) | S109 (+ #Favour 1) S132 | Hellvile |

Other cross-border pairs: Lilypad Lou & Flapjack (S113), Miss Lulla & the crypt kids (S112), Bonejangles & Riffmaw (S124), Dozy Doe & Batty Betty (S125), Oma Fluff & Graveley (S126), Ploddy & Maestro Mew (S128), Lady Smog & the beach (S108), Anvil & your door (S110).

### 12.3 Chapter 1 window (S50-S62)

**S50 · Ignore That One** — Shutterbug (46, Hillvile photographer) · Ch1-5 · photo, talk
Hook: Shutterbug wants a "candid" of the one citizen who never moves: the man in the ground-floor window. Beats: 1) find the window (Hillvile Towers); 2) stand still: nothing; 3) press A: the flash; `[HOLD 21f]` he does not blink; 4) `[BUTTON +22f]` as you walk away, one eye follows. Comic: the photo develops with him 1 px closer than he was. Scare: Shutterbug, developing it: "He's in all my other / photos too. In the back. / I never noticed." `[DEADPAN]`. Reward: 40 H$, F: *Framed Candid* (the figure in it moves one cell per in-game day). STARER 1/3.

**S51 · Yes, Three Eyes** — Kevin (creator preset, Hillvile) · Ch1-4 · creator, talk
Hook: Kevin wants a hat that fits. Beats: 1) SCENE_CREATOR: hats clip the third eye; 2) three tries `[R5]` apart, each hat pops back off `[POP]`; 3) a hat with a hole; 4) he looks in the mirror `[HOLD 13f]`; only the third eye blinks. Comic: "Nobody ever asks about / the third one." `[R21]` "It can see Tuesday." Heart: he wears it for the rest of the game; at the festival the hole has a tiny flag in it. Reward: 30 H$, F: *Kevin's Hat Stand*. KEVIN 1/3.

**S52 · A Really Good Spoon** — Rivet (13, Hillvile head welder) · Ch1-2 · fetch, welding
Hook: Rivet needs 3 Scrap for "a project". Beats: 1) fetch; 2) the welding minigame; 3) it is a spoon `[SLAM]` "SPOON."; 4) `[R26]` "It's a REALLY good spoon." Comic: the DAYS SINCE LAST FIRE sign through the window reads 7; a spark; 6. Heart: she makes spoons when she can't sleep; there are forty on the wall; she does not say who they were for. Reward: 50 H$, F: *Spoon Rack* (holds every spoon the chain gives you). SPOONS 1/4.

**S53 · Every Lamp a Friend** — Lumina (76, Hillvile lamplighter) · Ch1-3 · visit, housing
Hook: Lumina waves at every lamp in Hillvile and has noticed yours is "shy". Beats: 1) she visits your house at dusk; 2) place 2 lamps so they "can see each other"; 3) she waves at them `[RIPPLE]`; both flicker back. Comic: the pet sits in the light and beeps on the flicker. Heart: "A lamp in a window is a / promise. Somebody's coming / home to it." (Wisp's candle, planted). Reward: F: *Lumina's Lantern* (never goes out, even in Ch3).

**S54 · Notes From the Housekeeper** — Hauntessa (109, Hillvile haunted housekeeper, ghost) · Ch1-5 · housing, find
Hook: a ghost has been tidying your house and leaving notes ("your crate is a table now. you're welcome."). Beats: 1) find 3 notes; 2) she appears `[POP TW_POP_SOUL]` through the wall; 3) decorating goal: 6 items, nothing in front of the door; 4) she grades it `[SLAM stamp]` "TIDY". Comic: she rearranges your furniture while you watch, apologising. Heart: she haunts empty houses because "someone should keep them / warm for whoever's next." Reward: 40 H$, F: *Hauntessa's Duster* (a visitor in Ch4 asks if it's haunted; it is). DECOR 1/4.

**S55 · Bees** — Pip (42, Hillvile kid) · Ch1-2 · pet, escort
Hook: Pip is not scared of anything except the Wall, the fog and bees. There is a bee. Beats: 1) a bee on Mr. Flops' ear; 2) escort Pip to Bluebell's with the pet "guarding"; 3) three stops, Pip hiding behind the pet, then beside it, then in front `[R8]` each; 4) the bee lands on Pip's nose `[FREEZE Pip 13f]` and leaves. Comic: PIP: "I was BRAVE." `[R5]` "I'm going to cry now. / That's allowed after." Heart: brave is doing it scared. Reward: 25 H$, F: *Bee Drawing* (wall). PIP 1/4.

**S56 · Open Mic** — Hattie Haha (71, Hillvile comedian) · Ch1 · concert, talk
Hook: Hattie's set needs an audience of one. Beats: 1) Concert Hall, concert minigame as the "warm-up act"; 2) her set: three jokes `[R13]` apart; silence; 3) Fuzz laughs, alone, at the wrong one, hugely; 4) `[HOLD 18f]` HATTIE: "Thank you, Father." FUZZ: "I wasn't laughing at the / joke." `[R8]` "I was laughing at YOU." HATTIE: "...That's worse." FUZZ: "It's love, Hattie." Comic: the laugh track machine coughs. Heart: she keeps the one laugh in a jar on the piano. Reward: 40 H$, U: song "Open Mic" (jukebox). HATTIE 1/4.

**S57 · The Loose Brick** — Nana Puddin (1, Hillvile baker) · Ch1-2 · deliver, visit (night)
Hook: Nana asks you to put a muffin in "the brick that wiggles" at the Wall, after dark, and "don't wait". Beats: 1) find the brick (54,26); 2) place the muffin; 3) you wait anyway `[HOLD 26f]` `[MUTE 26f]`; 4) a hand with soot on it takes the muffin from the other side `[INSERT 3f]` `[SHK_TAP]`; 5) a bag of cookies is pushed back, 90% charcoal. Scare: the hand. Heart: NANA: "Eleven years. Every / Thursday. She never missed / one." `[R13]` "Neither did I." Reward: 30 H$, Charcoal Cookies x3, F: *Cookie Tin (two-sided)*. NANA 1/4.

**S58 · Flowers for a Cloud** — Mayor Giggs (0, Hillvile) · Ch1 · fetch, deliver
Hook: "Town beautification." Flowers for one grave, specifically. Beats: 1) pick flowers (flower bed); 2) the graveyard; 3) the grave: "Fluffy the cloud. Drifted."; 4) place; the Mayor is there before you, pretending to be lost. `[EXPR Mayor=SAD]` for `[R8]` exactly. Comic: "I'm inspecting the... / gravel. Hee. Hee." Heart: a child's handwriting on the stone, if you look closely (the questlog shows it). Reward: 30 H$, F: *Small Cloud Stone* (garden; rains 1 px on it when the Mayor visits). MAYOR 1/3.

**S59 · Do NOT Bonk Father Fuzz** — Father Fuzz (77, Hillvile priest) · Ch1-5 · minigame, talk
Hook: the Whack-an-Imp stall has a problem: one of the heads is Fuzz. Beats: 1) the minigame (existing, "Do NOT bonk Father Fuzz"); 2) you bonk him `[SHK_POW]`; 3) `[HOLD 13f]` he pops back up smiling `[POP]`; 4) win: Mr. Huggins is the top prize; Fuzz won't let him go: "He's not a prize. He's / STAFF." Comic: every bonk, a pin pings off him SFX_TINK. Heart: he invented the stall so kids would get used to imps' faces. "Then nobody's scared of / Rivet." Reward: 40 H$, F: *Whack-an-Imp Head (Fuzz)* (squeaks when the pet sits on it). FUZZ 1/4.

**S60 · Cold Hands** — Dr. Bloom (111, Hillvile ghoul doctor) · Ch1-2 · fetch, deliver
Hook: patients flinch; Bloom wants mittens, "medical grade". Beats: 1) Oma Fluff knits them (fetch 2 Wool); 2) deliver; 3) he examines the pet in mittens; the pet beeps; 4) `[R21]` "Hm. Warm. / I don't trust it." Comic: the mittens have skulls on; Oma: "Festive." Heart: he came north before the Wall for the hospital job and never went back; his old ward partner is on the other side (S107). Reward: 40 H$, F: *Skull Mittens* (coat hook). BLOOM 1/3.

**S61 · Tick, Tock, Tally** — Tock (104, Hillvile clockmaker) · Ch1-2 · fetch, find
Hook: every clock in Tock's shop has stopped at the same minute. Beats: 1) fetch 2 Scrap for springs; 2) he winds them; they all start, all 1 min slow; 3) he naps through the chimes; 4) `[APPEAR]` on his notepad: "3,652". He does not know why he wrote it. Comic: the cuckoo comes out, looks at you, goes back in `[R8]`. Scare: all the clocks tick in Tallyworn's rhythm. Reward: 35 H$, F: *Slow Clock*. TOCK 1/3.

**S62 · Tuck-In's Rounds** — Tuck-In (19, Hillvile daycare plushie) · Ch1-2 · escort, visit
Hook: Tuck-In tucks in children, plants and grumpy old men; the grumpiest is Grampy Bean. Beats: 1) escort at dusk to the tomato plot; 2) Grampy refuses `[FREEZE Grampy]`; 3) Tuck-In tucks in the tomatoes instead; 4) Grampy, from the bench, pulls the blanket over himself `[R26]`. Comic: a tomato snores. Heart: he sits on that bench because someone used to sit on the other end; the game does not say who yet. Reward: 30 H$, F: *Tuck-In Blanket* (the pet uses it). GRAMPY 1/3.

### 12.4 Chapter 2 window (S63-S78)

**S63 · Employee of the Month** — Vex (666, Cloud Nine) · Ch2 · photo, talk
Hook: Vex wants a photo of the employee-of-the-month board for "head office". Beats: 1) the board: a crowned cloud, twelve months running; the small print says "Vex" under "runner-up" every time; 2) photo; the cloud on the board blinks `[INSERT 2f]`; 3) Vex: "He's very photogenic." `[HOLD 13f]` "He likes it when I say / that." Comic: Haze, watering: "The plant's never won / either." Heart: Vex has never been employee of the month of his own shop. Reward: 40 H$, F: *Runner-Up Ribbon*. VEX 1/5.

**S64 · Flavor Testing** — Vex · Ch2 · fetch, cooking
Hook: Vex wants to make a flavour "from actual fruit, as an experiment". Beats: 1) fetch 3 fruits (Gumball, Jellybean); 2) cooking minigame at the shop's back counter; 3) he tastes it `[HOLD 8f]`: "...It tastes like fruit." > That's good? > That's bad? VEX: "That's not what he / wants." `[DEADPAN]`; 4) he pours it out; keeps the recipe in his hat. Comic: the vapor comes out shaped like a strawberry and sulks. Heart: the first time he says "he". Reward: 50 H$, item Fruit Vape (plain), U: Cloud Ten recipe seed. VEX 2/5.

**S65 · Grape Fish** — Capt. Whisk (48, Hillvile fisher) · Ch2-3 · fishing
Hook: the fish taste of grape and Whisk refuses to name them until they stop. Beats: 1) fishing at the pier, 3 catches; 2) the Fishdex entry "Grapefish (temporary)"; 3) the third fish is purple and hums the lullaby; Whisk lets it go `[HOLD 13f]`. Comic: WHISK: "I name them. I don't / EAT them. We've talked / about this." Scare: the fish all face the Wall at night. Reward: 40 H$, U: Fishdex "Grapefish", F: *Fish Trophy (humming)*.

**S66 · Beep Translator** — Prof. Fizz (69, Hillvile) · Ch2-4 · pet, talk
Hook: Fizz's vapor detector beeps at everything, including your pet, and he wants to know what the pet is saying back. Beats: 1) bring the pet; 2) three readings `[R13]` apart: "beep." = "hello", "beep." = "hungry", "beep." = `[HOLD 13f]` "...it says 'mine'. About / you." Comic: the detector and the pet have a beep argument `[RIPPLE]`. Heart: the third reading is the only time the game translates a beep, and Fizz says the machine is probably broken. Reward: 40 H$, F: *Vapor Detector Mk I* (beeps at the pet at 3 AM).

**S67 · Night Shift at the Bath-House** — Lather (41, Hillvile soap maker) · Ch2, after Ep 2.3 · visit (night), find
Hook: Steamy's bath-house is closed; Lather left soap inside; "just grab it, don't look at the water." Beats: 1) enter at night; no music; 2) the ducks in the main tub all face the door `[HOLD 21f]`; 3) find the soap; 4) the water giggles once, pitched down; 5) leaving: a duck is on the step outside, facing the Wall. Scare: `[NO GAG]`, the whole quest; the only sound is drips at `[R3]` and `[R8]`. Button: the next day Steamy (saved) or Lather (lost) has lined the ducks up facing away from the Wall. Reward: 30 H$, item Lavender Soap, F: *Facing Duck*. STEAMY 1/3.

**S68 · Two Inches to the Left** — Hauntessa (109) · Ch2 · housing, find
Hook: "Someone ELSE is tidying your house. Badly." Beats: 1) three nights, something moves 2 inches left (keys, lamp, the egg nest); 2) wait up: Wobblewisp (972), a Hellvile poltergeist, came through with the fog "for the quiet"; 3) Hauntessa and Wobblewisp argue about tidying philosophy `[RIPPLE]`; 4) truce: he can move the Vapor Detector only. Comic: "Two inches is TASTEFUL." Scare: the first night, the chair is facing your bed. Reward: 40 H$, F: *Wobblewisp's Favourite Vase* (moves 2 in. left nightly). DECOR 2/4.

**S69 · Heckler** — Hattie Haha · Ch2 · find, talk
Hook: someone heckles Hattie's rehearsals and she can't see them. Beats: 1) attend three rehearsals at different hours; 2) the heckle ("hee") comes from outside; 3) follow it: the giggling fountain, laughing in the Mayor's voice, on the wrong beat; 4) Hattie confronts the fountain `[HOLD 13f]`: "You're not even FUNNY." The fountain stops for the first time in the game `[MUTE 29f]`. Comic: the Mayor, passing, laughs on the old beat alone. Scare: the fountain starts again as you leave, lower. Reward: 40 H$, U: photo "Heckler". HATTIE 2/4.

**S70 · A Spoon for Something Small** — Rivet · Ch2 · welding, pet
Hook: Rivet has noticed your pet eats with its face. Beats: 1) welding minigame, tiny; 2) the pet-sized spoon `[DROP]` with `[SQUASH 1.3/0.8]`; 3) the pet picks it up, looks at it, eats with its face anyway `[R26]`; 4) RIVET: "...It's a REALLY good / spoon." `[EXPR Rivet=SAD]` for 8 frames. Comic: the sign flips 3 -> "-" -> 0. Heart: she made a spoon this size once before, for someone this size, and left it in a crib. Reward: 40 H$, F: *Tiny Spoon*. SPOONS 2/4.

**S71 · A Recipe Through the Wall** — Nana Puddin · Ch2 · cooking, deliver
Hook: the cookies from S57 came with a recipe card: bone soup, in Charlotte's hand. Nana wants it made. Beats: 1) fetch 2 Bone (Bonejangles donates, reluctantly: "They grow back. / Probably."); 2) cooking minigame; 3) Nana tastes it `[HOLD 13f]`; 4) `[EXPR Nana=SICK]` `[R5]` `[EXPR LOVE]`: "It's AWFUL." `[R8]` "It's hers." Comic: Bonejangles asks for the bones back. Heart: she writes "needs salt" on the card and sends it back through the brick. Reward: 40 H$, item Bone Soup, U: recipe. NANA 2/4.

**S72 · Rest in Pieces** — Father Fuzz · Ch2, after Ep 2.3 · visit, deliver
Hook: Mr. Huggins' head came off again; Fuzz wants a proper ceremony "for the practice". Beats: 1) carry the head to the graveyard (it says "(thud.)" when you drop it); 2) SCENE_GRAVEYARD: the stone "Rest in pieces, Mr. Huggins the 1st"; 3) Hugsley attends and hugs the stone; 4) Fuzz sews the head back on at the grave `[R26]`: "There. Resurrected. / Don't tell the bishop." Comic: a second, older stone behind it: "Mr. Huggins the 0th". Heart: Fuzz made Mr. Huggins at nine, the year the cloud left; the game notes the date on the stone. Reward: 30 H$, F: *Mr. Huggins (spare head)*. FUZZ 2/4.

**S73 · Purple Tomatoes** — Grampy Bean (88, Hillvile) · Ch2 · fetch, photo
Hook: the tomatoes are purple and Grampy wants them documented "for the complaint". Beats: 1) fetch 1 Fertiliser; 2) photo of the plot at dawn; 3) the tomatoes hum; 4) GRAMPY: "They taste BETTER. / I'm furious." Comic: Shutterbug's photo shows a second shadow on the bench. Heart: Grampy admits he's been talking to the tomatoes since "the other end of the bench went quiet". Reward: 35 H$, item Purple Tomato x2, F: *Tomato Pot (humming)*. GRAMPY 2/3.

**S74 · The Plaque** — Mayor Giggs · Ch2 · fetch, visit
Hook: the Wall's foundation plaque needs polishing "for appearances". Beats: 1) fetch 1 Polish (Mintworth); 2) the plaque at (55,28): "ERECTED FOR THE SAFETY OF"; the rest is under grime; 3) you start on the grime; the Mayor's hand on yours `[FREEZE Mayor]` `[HOLD 13f]`: "That's enough. That's / plenty. Hee." Comic: the polish squeaks SFX_SQUEAK on every stroke, in rhythm with the fountain. Scare: under the grime, a corner of a signature in crayon-coloured ink. Reward: 30 H$, F: *Polish Tin*. MAYOR 2/3. (Pays off in I-C and Ep 4.6.)

**S75 · Explorer's Club: Under** — Scoutmaster (15, Hillvile) · Ch2-3 · visit (HB3)
Hook: the Explorer's Club has never had a "below" badge. Beats: 1) enter Sugar Rush Sewers (HB3) from the graveyard grate; 2) reach the grape main (no need to beat Gutterking); 3) a chalk drawing on the pipe wall: a house, a candle, "ARE YOU THERE", backwards; 4) photo. Comic: the Scoutmaster has a badge for "saw something unsettling" already made. Heart: the drawing is the same hand as Pip's paper boat. Reward: 40 H$, U: badge "Under", F: *Badge Board*.

**S76 · Weather Report** — Drizzle (57, Hillvile weather bunny) · Ch2 · photo (x3), talk
Hook: Drizzle is always right and wants to be wrong once. Beats: 1) photo the purple cloud at 09:00, 15:00, 21:00; 2) each photo it is bigger and has moved north; 3) DRIZZLE, with the three photos: "Fair. Fair. Grape." `[HOLD 13f]` "I'd like to be wrong. / Once. Today would be good." Comic: her umbrella opens by itself on the third photo. Scare: in the 21:00 photo the cloud has a crown. Reward: 40 H$, F: *Weather Vane (points at the Wall)*.

**S77 · Thirteen** — Tock · Ch2 (night) · visit, find
Hook: Tock's clocks chime at midnight, backwards, and he sleeps through it. Beats: 1) visit the shop at 00:00; no music; 2) twelve chimes, descending; 3) a thirteenth `[HOLD 26f]`; 4) the cuckoo comes out and does not go back in. `[NO GAG]`. Button, next morning: Tock has written "3,651" on the pad and crossed out 3,652. "Counting DOWN now. / Don't know to what." Scare: the thirteenth chime is the fountain's giggle. Reward: 40 H$, F: *Cuckoo (out)*. TOCK 2/3.

**S78 · Wands** — Miss Lulla (36, Hillvile daycare) · Ch2 · find (x3), talk
Hook: the kids have hidden three more "wands" (vape pens) and will only say where in riddles. Beats: 1) three kid-logic riddles ("where the floor is lava": the sandbox); 2) find; 3) Button Nose (18) has buttoned one to Tuck-In's coat; 4) the kids play exorcist on the pet; it beeps; they declare it saved `[RIPPLE]` "YAY". Comic: one riddle is just "no." Heart: the kids' drawing wall now has the Wall on it, with a door drawn in crayon. Reward: 35 H$, Vape Pen x3 (hand to Fuzz for +friendship), F: *Kids' Wall Drawing*.

### 12.5 Chapter 3 window (S79-S93), all on the invaded map unless noted

**S79 · Days Since** — Chief Spark (99, Hillvile fire chief) · Ch3 · fetch, welding
Hook: the town needs a DAYS SINCE LAST FIRE sign "for morale"; the town is on fire. Beats: 1) fetch 3 Scrap; 2) welding minigame with Rivet; 3) hang it on the church: it reads 0; 4) `[HOLD 13f]` it catches fire. SPARK: "...Good. Now we know." Comic: Hosey (124) waters it and the flowerbed in one pass. Heart: Spark rescues "cats, kites and feelings"; he rescues the Mayor's hat from a brute during the quest and gives it back without a word. Reward: 50 H$, F: *DAYS SINCE LAST FIRE sign* (counts real in-game days; resets when Rivet visits).

**S80 · Balloon Evacuation** — Updraft (17, Hillvile balloonist) · Ch3 · escort, combat
Hook: Updraft has never come down for lunch and won't now; three citizens need lifting out of the plaza. Beats: 1) escort Oma Fluff, Twinkly and Squeaker through two packs; 2) each boards the basket `[DROP]`; Squeaker's shoes squeak on every step `[SFX_SQUEAK]`; 3) the balloon rises; Twinkly: "I'm a CLOUD now." `[HOLD 8f]` everyone looks at the sky `[EXPR SHOCK]`; "...a nice one." Comic: Updraft eats a sandwich through the whole fight. Heart: Oma Fluff leaves her glasses with you "for whoever's next". Reward: 60 H$, F: *Toy Balloon* (floats to the ceiling).

**S81 · Smileys on the Boards** — Squishy (39, Hillvile window washer) · Ch3 · fetch, visit
Hook: every window is boarded, and Squishy is still doing his rounds. Beats: 1) fetch 3 Soap; 2) follow his round: he draws a smiley on each board; 3) at the starer's boards he hesitates `[HOLD 13f]`, draws one with three eyes; 4) a knock from inside, once. Comic: SQUISHY: "Repeat customer." Scare: the knock. Reward: 35 H$, F: *Boarded Window (smiley)* (Ch4 variant: unboarded).

**S82 · Cocoa Through the Boards** — Lumina (76) · Ch3 (night) · deliver
Hook: Lumina lights every lamp still standing; the starer's lamp is behind boards. Beats: 1) carry cocoa to the boards; 2) push it through the gap; 3) `[HOLD 26f]` `[MUTE 26f]`; a hand takes it; 4) `[R21]` the cup comes back out, empty, washed. Scare: `[NO GAG]`. Button, next morning: "!" over the boards, then a smiley with three eyes inside the "!" Reward: 30 H$, F: *Washed Cup*. STARER 2/3.

**S83 · Vapor Detector Mk II** — Prof. Fizz (69) · Ch3 · combat, pet
Hook: the Mk II beeps at nests from afar; it also beeps at your pet, louder. Beats: 1) carry the detector; 2) a hidden fifth nest in the Hillvile Towers lobby, found by the beep; 3) combat (one pack); 4) the detector and the pet beep at each other; the detector loses `[SLAM]` "ERROR". Comic: FIZZ: "It's not broken. / It's INTIMIDATED." Scare: the lobby's elevator dings for a floor the building doesn't have. Reward: 60 H$, 2 Scrap, F: *Mk II (sulking)*.

**S84 · Mr. Flops Gets Stitches** — Pip (42) · Ch3, after Ep 3.2 beach · fetch, deliver
Hook: Mr. Flops took a shell for the fort and has a tear. Beats: 1) fetch Thread from Fluffernut (78); 2) Boo-Boo Bea (107) does the stitches and gives Mr. Flops a sticker "for emotional boo-boos"; 3) Pip watches `[HOLD 13f]`: "Does it hurt him?" BEA: "Only the brave part." `[R8]` PIP: "That's all of him." Heart: `[NO GAG]`. Button: the stitches are a tiny heart; Wisp notices them in Ch4. Reward: 30 H$, F: *Boo-Boo Sticker Sheet*. PIP 2/4.

**S85 · Buckwheat** — Father Fuzz · Ch3, after Ep 3.4 · fetch, deliver
Hook: Fuzz, in the church with the kids, is leaking, and asks for "2 buckwheat, for a recipe". Beats: 1) Nana has two of everything; she gives you three; 2) deliver; he re-stuffs himself behind the altar; 3) `[R21]` he comes out rounder; "...Recipe went well."; 4) the kids hug him; a pin pings off `[TINK]`. Comic: Nana: "Tell him it's / gluten-free. He worries." Heart: he asks you not to tell the bishop, or Nox, or anyone, that faith needed a top-up. Reward: 40 H$, F: *Buckwheat Sack*. FUZZ 3/4.

**S86 · The Spoon Launcher** — Rivet · Ch3 · welding, combat
Hook: Rivet has had an idea about spoons and self-defence. Beats: 1) welding minigame (a long one); 2) the Spoon Launcher: CUTE class, fires spoons that bonk `[SHK_POW]`; 3) test it on a pack; each hit a `[TINK]`; 4) RIVET: "It's a REALLY good / launcher." `[R26]` "Say it." > It's a really good launcher. > It's a spoon. RIVET: "Both. Yeah." Comic: the demons collect the spoons. Heart: she cannot stop making things for you to hold. Reward: weapon Spoon Launcher, 60 H$. SPOONS 3/4.

**S87 · Thirteen Bells** — Cantor Coo (426, Hillvile) · Ch3, after #24 (night) · visit, exorcism (echo)
Hook: Bellwether is saved, but the bell tower still rings backwards at midnight. Beats: 1) climb the tower at 00:00; 2) twelve backwards bells, then a thirteenth; 3) the bell rope moves by itself `[HOLD 26f]`; 4) a wisp on the rope, confused; 5) the player says the lullaby's first line (A on the prompt); the wisp lets go and goes south. `[NO GAG]`. Scare: the whole quest. Heart: not every soul needed a rite; some only needed the tune. Reward: 50 H$, U: song "Thirteen Bells", F: *Small Bell* (rings at 00:00, once, forwards).

**S88 · House Call** — Dr. Bloom (111) · Ch3 · escort, combat
Hook: Grampy has not come to the church and Bloom is going to him. Beats: 1) escort through the park nest edges; Bloom refuses to run ("Doctors walk."); 2) Grampy on his bench, exorcised or not depending on Ep 3.4 order; 3) Bloom takes his pulse with cold hands; GRAMPY: "COLD." BLOOM: "Alive, then." Comic: Bloom prescribes "one tomato, orally". Heart: Bloom sits on the other end of the bench for `[R21]`, and Grampy lets him. Reward: 50 H$, Medicine x2. BLOOM 2/3.

**S89 · The Screaming Crop** — Grampy Bean (88) · Ch3 · combat, fetch
Hook: the tomatoes scream when the imps pick them, and Grampy wants them picked properly. Beats: 1) clear the plot (one pack); 2) pick 4 tomatoes: each screams `[SFX_SQUEAK pitched]` then sighs; 3) the imps have left reviews `[APPEAR]` "4 stars. seeds."; 4) Marrowmere (if saved in Ep 3.4) waves from the far side of the Wall in the cutaway. Comic: the last tomato screams "FINE." Heart: Grampy keeps two for the bench. Reward: 50 H$, Purple Tomato x4, F: *Tomato Review Board*. GRAMPY 3/3.

**S90 · Tea for a Tic** — Mayor Giggs (0) · Ch3 · fetch, talk
Hook: the Mayor cannot stop: "Hee. Hee. Hee. Hee." Nana says tea. Beats: 1) fetch tea (Grimbrew is across the Wall; Mocha Latte (70) brews it); 2) deliver under the pew; 3) he drinks; the Hee stops `[MUTE 41f]`; 4) he sits with you in the silence `[HOLD 26f]` `[NO GAG]`; "That's worse. / Hee." Comic: Mocha Latte drew a cloud in the foam; the Mayor drinks around it. Heart: the first time the tic is named as a wound. Reward: 40 H$, F: *Teacup (cloud foam)*. MAYOR 3/3.

**S91 · I Tidied the Rubble** — Hauntessa (109) · Ch3 · housing
Hook: your house is a barricade and a ghost has opinions. Beats: 1) decorating goal: barricade HP 100+ with no furniture blocking the fireplace (Kevin's rule); 2) she grades it `[SLAM]` "HELD?"; 3) a note on the toaster turret: "i oiled it. you're welcome. / don't aim at the lamp." Comic: she dusts the brute at the window. Heart: she has haunted this house before, between tenants, and "kept it warm". Reward: 40 H$, F: *Welded Plate (battle scar)* (kept after Ch3 if you want). DECOR 3/4.

**S92 · Ducks Facing South** — Steamy (2, saved) or Lather (41, lost) · Ch3 · find (x5), talk
Hook: five rubber ducks have left the bath-house and are drifting toward the Wall. Beats: 1) find 5 ducks along the river; 2) each faces south; 3) the fifth is at the Wall's loose brick; 4) Steamy (saved): "She liked them. The one / who was in me. She said / they were brave." `[HOLD 13f]` He leaves one in the brick. Lost: Lather leaves it, and the quest is `[NO GAG]`. Comic: a duck is in Grumbleguts' belly-print on the Wall. Heart: a duck crosses before anyone does. Reward: 40 H$, F: *Rubber Duck (southbound)*. STEAMY 2/3.

**S93 · The Hug Audit** — Twisty (43, Hillvile balloon twister) · Ch3, after #30 · talk, combat
Hook: Hugsley is saved and hugging again, "a bit too well". Beats: 1) watch Hugsley hug a cloud spider: it pops into grape goo `[SHK_POW]`; 2) Hugsley: "...I meant that one."; 3) three more hugs, each `[R13]` apart, each pop bigger; the third gets confetti; 4) TWISTY: "He's our best weapon. / Don't tell him." Comic: Hugsley hugs the player; the screen squashes `[SQUASH 1.1/0.9]`. Heart: Hugsley has counted every hug since "the bus": 1,000 a day, except the eleven years of none from the south. Reward: 50 H$, F: *Hug Counter*. HUGSLEY 1/2.

### 12.6 Chapter 4 window (S94-S117), Hellvile and the gate

**S94 · Little Black Book** — Gatekeeper Grumbleguts (hell-born, the gate) · Ch4 · deliver (x3), talk
Hook: Grumbleguts has decided to "close some chapters" and his black book is the size of a phone directory. Beats: 1) deliver notes to three exes: Fangela ("I sniffed his ID. It / said 'single'. It lied."), Lady Smog (`[FREEZE Smog]` "No."), and the Gate (it creaks; he blushes); 2) each refusal `[SCRATCH]`; 3) he backs into frame to hear the results `[SCRATCH]`. Comic: BELLY: "grrrbl." G: "The belly says you / did great. The belly is / kind." Heart: the book is mostly blank after page 2; he writes new names in big letters so it looks full. Reward: 50 H$, F: *Little Black Book* (opens to a page that says "YOU? (no)"). EXES 1/2.

**S95 · Ex-Demons Anonymous** — Oddsball (522, Hellvile bookie) · Ch4 · visit, talk
Hook: Oddsball has taken bets on how many exes will show up to a "support group" at Brimstone Bites. Beats: 1) gather Fangela, Cloudzilla (510), Kodiak (849), Lady Smog (declines by letter); 2) they compare notes `[RIPPLE]`: all were told "you're cute when you're scared"; 3) Grumbleguts walks in butt-first `[SCRATCH]` carrying a tray; 4) everyone leaves except the Gate, which is a door, which stays. Comic: Oddsball loses the bet (he bet zero). Heart: Grumbleguts sits down at the empty table and eats the tray himself; the belly is quiet for once. Reward: 40 H$, F: *Support Group Chair*. EXES 2/2.

**S96 · A Light at the Gate** — Wisp (563, Hellvile kid) · Ch4 · fetch, deliver
Hook: the candle in Wisp's window is nearly out and "Mum needs to see it from the gate". Beats: 1) fetch Lamp Oil from Lampwick (862), who hugs the bottle goodbye; 2) deliver; 3) Wisp relights it `[HOLD 13f]`; 4) WISP: "It's not for GHOSTS. / It's for Mum. Ghosts can / use it too though." Saved branch: Ma Gloom, upstairs, moves the candle one cell so it's easier to see. Comic: Pa Gloom pretends the oil is for the crypt. Heart: `[NO GAG]`. Reward: 30 H$, F: *Window Candle* (lit; Ch5: see S133). WISP 1/5.

**S97 · Mum's Toast** — Wisp · Ch4 · cooking
Hook: Wisp wants toast "the way Mum does it", which is burnt. Beats: 1) cooking minigame with Crispin (838) ("Burn it. No, MORE."); 2) three tries: too pale, too pale, black; 3) Wisp eats it `[HOLD 13f]` `[EXPR LOVE]`; 4) saved: Ma Gloom from upstairs: "Too dark." WISP: "She says it's perfect." Lost: Wisp: "...Perfect." `[DEADPAN]`. Comic: Crispin cries at pale toast. Heart: the lunchbox on the hook is used for the first time in a year (saved) or stays on the hook (lost). Reward: 30 H$, U: recipe "Mum's Toast", F: *Lunchbox*. WISP 2/5.

**S98 · Chalk on the Pipe** — Clinker (753, Hellvile chalk poet) with Wisp · Ch4, before Ep 4.5 · photo, escort
Hook: the Underbelly maintenance is going to wash the pipes; Wisp's drawings are on them. Beats: 1) escort Wisp into the first Underbelly chamber (no combat yet); 2) photo 3 drawings: the house, "ARE YOU THERE", and a new one: a bunny with a stitched heart; 3) Clinker adds a poem under each; 4) Wisp: "That one's for Pip. / They don't know yet." Comic: Clinker's forty-first poem is just "pipe". Heart: `[NO GAG]`. Reward: 40 H$, U: photo set "Pipe Gallery", F: *Chalk Box*. WISP 3/5.

**S99 · Taller** — Wisp · Ch4 · housing, talk
Hook: Wisp wants to be measured against your door frame "because ours has Mum's marks and I don't want to go past hers yet". Beats: 1) housing: the door frame becomes a height board; 2) Wisp stands against it `[HOLD 13f]`; 3) a mark; 4) Pip (if S84 done) insists on one too, on tiptoe; `[SCRATCH]` when caught. Comic: the pet wants a mark; it is 1 px. Heart: saved: Ma Gloom comes to the door and adds hers, slightly lower than it used to be. Lost: Wisp asks you to draw Mum's mark "about here" from memory. Reward: F: *Height Marks* (permanent, both kids' names). WISP 4/5.

**S100 · Deadpan Lessons** — Hattie Haha (71) + Honkers (501, Hellvile clown) · Ch4 · concert, talk
Hook: Honkers says Hattie's problem is "too much face" and offers lessons. Beats: 1) Concert Hall or the Hellvile stage; 2) three exercises: say "knock knock" without moving `[FREEZE Hattie]`; honk at a funeral (Mr. Solemn walks past; he is not dead; he is close); the slow turn to camera; 3) the concert minigame as a two-hander; 4) Hattie lands one line `[DEADPAN]` and Honkers honks once, approvingly. Comic: HONKERS: "You laughed at your own / joke." HATTIE: "I did NOT." HONKERS: "Your ears did." Heart: Honkers has never been invited back anywhere; Hattie books him. Reward: 50 H$, U: song "Knock Knock (duet)". HATTIE 3/4.

**S101 · Day One** — Tallyworn (712) + Tock (104) · Ch4 · deliver, studio
Hook: Tock's countdown and Tallyworn's count-up have reached the same number. Beats: 1) bring Tock through the gate (he naps on the way); 2) the chalkboard "3,653" and the notepad "3,653"; 3) `[HOLD 26f]` `[MUTE 26f]`; 4) they wipe both together; Tallyworn writes "1". Tock sets every clock to the right time; the chimes, forwards. Studio: a 1-bar song of two clocks. Comic: TOCK: "I'm going to nap now. / Wake me when it's two." Heart: `[NO GAG]`. Reward: 50 H$, F: *Counter Board* (counts your days in the united park). TOCK 3/3.

**S102 · Feed the Gate 1/4: A Light Snack** — Gatekeeper Grumbleguts · Ch4, after Ep 4.1 · cooking, deliver
Hook: the belly has asked for you by name. Beats: 1) cook any dish; 2) deliver at the gate; 3) he eats it with ENORMOUS effort: squash and stretch, three bites `[R13]` apart, sweat, the harness creaks; 4) `[HOLD 13f]` "...Light snack." The forklift arrives with the next course. Comic: BELLY: "more." G: "It said thank you." Heart: he asks if you'll stay while he eats; nobody ever has; you do, for `[R21]`, and the quest counts it. Reward: 40 H$, F: *Forklift Pallet (empty)*. FEED 1/4.

**S103 · Feed the Gate 2/4: Bagel Mountain** — Grumbleguts · Ch4 · fetch, escort
Hook: Chef Scorch (659) has his Brimstone Bagels; the belly wants "a mountain". Beats: 1) fetch 3 bagels; Scorch's hot sauce melts your spoon (`[TINK]` of the Spoon Rack losing one); 2) escort the forklift (Boltsy, 671, drives) from the market to the gate through a late pack; 3) he eats the mountain in one scene with his back to camera `[SCRATCH]` x2; 4) belt notch 2. Comic: he squeezes through his own gate sideways `[SQUASH 1.4/0.7]` BOING to get the last bagel. Heart: Scorch admits the bagels were always for him; nobody else can eat them. Reward: 50 H$, F: *Bagel Mountain (decorative, 1/10 scale)*. FEED 2/4.

**S104 · Feed the Gate 3/4: The Belly Judges** — Grumbleguts + Mother Nox (778) · Ch4 · cooking, minigame
Hook: a bake-off. Your dish vs Nox's bone soup. The belly is the judge. Beats: 1) cooking minigame (your best); 2) the tasting: he eats both with effort; 3) BELLY: "grrrbl." `[HOLD 8f]` G: "The belly says... tie." `[SCRATCH]` on his butt turning to leave; 4) Nox: "The belly is a coward." BELLY: "grrrbl?" NOX: "...Fair." Comic: he sucks his gut in for the verdict; it springs back `[SCRATCH]`. Heart: Nox offers him the line-cook job for the first time here; he says no because "cooks have to stay". Reward: 60 H$, U: recipe "Tie Soup", F: *Judge's Bib*. FEED 3/4.

**S105 · Feed the Gate 4/4: Someone for Dessert** — Grumbleguts · Ch4, after Ep 4.5 · deliver, talk
Hook: he has made dessert for two and wants you to find "anyone" to eat the other one. Beats: 1) invite three citizens (any; each refuses with a `[SCRATCH]`); 2) the fourth is Mr. Solemn (555), who has not smiled in 200 years; he sits down because "someone should"; 3) they eat in silence `[HOLD 26f]` `[MUTE 26f]`; 4) Solemn: "Close." `[R8]` G: "To what?" SOLEMN: "Once." Comic: the dessert is a cake shaped like his own butt; Solemn eats it without comment. Heart: `[NO GAG]`; Grumbleguts takes the line-cook job the next morning and wears the tight shirt. Reward: 80 H$, F: *The Big Chair* (creaks; citizens slide off), U: "Hellvile's Biggest Spoon" eligible (§11.3 Branch A). FEED 4/4.

**S106 · Dear Friend** — Nana Puddin (1) + Charlotte (566) · Ch4 · deliver, talk
Hook: Nana has written a letter and signed it with her name for the first time in eleven years. Beats: 1) carry it through the gate; 2) Charlotte reads it with a cookie in her mouth `[FREEZE Charlotte 13f]`; 3) `[EXPR LOVE]`; she writes back on the same page; 4) deliver back; Nana reads; `[HOLD 26f]`. Comic: CHARLOTTE: "She says 'needs salt'. / About the SOUP. Eleven / years, and 'needs salt'." Heart: the two signatures on one page become furniture. Reward: 40 H$, F: *One Page, Two Names*. NANA 3/4.

**S107 · Cold Hands, Warm Leeches** — Dr. Bloom (111) + Nurse Leech (687, Hellvile) · Ch4 · reconcile, deliver
Hook: Bloom will not go to the Hellvile clinic; Leech will not come to the hospital; both have patients the other could fix. Beats: 1) carry a chart south; Leech prescribes leeches for it; 2) carry it north; Bloom prescribes mittens; 3) the patient is Grampy Bean, who accepts both; 4) the two meet at the gate and shake hands `[HOLD 13f]`: "Cold." "Warm." "Hm." Comic: Leech prescribes a leech for sadness; Bloom takes it. Heart: they were ward partners before the Wall; they argue about a patient from eleven years ago, who is fine. Reward: 50 H$, Medicine x3, F: *Leech Jar (decorative)*. BLOOM 3/3.

**S108 · Eleven Years of Air** — Lady Smog (502) · Ch4, after Favour 2 · escort, photo
Hook: Lady Smog has not been outside her gown since the Duke came, and wants "weather. Real weather." Beats: 1) escort her to the Hillvile beach; she flinches at every gust `[EMOTE SWEAT]`; 2) at the shore `[HOLD 26f]`; 3) she breathes `[MUTE 29f]`: "It's DELICIOUS."; 4) photo. Comic: the gown tries to leave by itself and she steps on it. Heart: she takes the gown off (there is a smaller, plainer courtier underneath) and folds it, and does not throw it away. Reward: 50 H$, F: *Folded Smog Gown*.

**S109 · Seeds Both Sides** — Sootkin (525) + Shackles (604) · Ch4, after Favour 1 · fetch, housing
Hook: the gate sunflower has seeds; Shackles has a matchbox with one more; both want a garden on both sides. Beats: 1) fetch 2 Seed Packets from Bluebell; 2) plant one in your garden (housing) and one at the Pump; 3) water them over two in-game days; 4) both sprout on the same morning `[RIPPLE]`. Comic: Shackles' sprout faces the Pump's fridge. Heart: Sootkin has never grown a flower in the sun. Reward: 40 H$, F: *Sunflower (garden)* (turns north/south undecided; Ch5: faces up). GARDENS 1/2.

**S110 · The Door** — Anvil (595, Hellvile blacksmith) · Ch4, after Favour 3 · housing, talk
Hook: Anvil has heard your door squeaks. Beats: 1) she visits the cottage; 2) she looks at the hinge `[HOLD 13f]`: a tiny anvil stamp; 3) ANVIL: "I made this. Before the / Wall. For the potluck / houses." `[R13]` "It still squeaks? Good."; 4) she does not oil it: "I left the squeak in / on purpose. So you'd know / when someone comes home." Comic: Rivet oils it anyway; Anvil un-oils it. Heart: `[NO GAG]`. Reward: F: *Anvil's Hinge* (the door squeaks forever; it is a feature).

**S111 · A Spoon for Mum** — Rivet (13) · Ch4 · deliver, talk
Hook: Rivet has picked her best spoon off the rack and cannot carry it across herself. Beats: 1) carry it to the forge; 2) Anvil takes it `[HOLD 13f]`; opens a drawer: a tiny bent spoon, Rivet's, age six; 3) "I have one." `[HOLD 13f]` "...I'll have two."; 4) she puts them side by side on the table set for one more. Comic: Sprocket, "not reading" at the stove, turns a page. Heart: `[NO GAG]`. Reward: 50 H$, F: *Two Spoons* (the rack's centrepiece). SPOONS 4/4.

**S112 · Daycare Exchange** — Miss Lulla (36) + Graveley (730, Hellvile landlord) · Ch4 · escort, minigame
Hook: the crypt kids (Morguelet, Knucklehead, Grrrtrude) have never napped above ground; the daycare has never napped a ghoul. Beats: 1) escort three kids through the gate; 2) nap time: Lulla gets Morguelet down in under a minute; Knucklehead pushes Button Nose in a puddle, then fishes them out, then naps; 3) Grrrtrude growls at Lilypad Lou, then licks him; 4) Graveley lowers nobody's rent and denies it. Comic: Dozy Doe (34) coaches the ghouls' nap; the ghouls are better at it. Heart: Morguelet falls asleep holding a flower from Bluebell's; nobody takes it away. Reward: 40 H$, F: *Nap Mat (crypt pattern)*.

**S113 · Cross-Border Post** — Lilypad Lou (97) + Flapjack (808, Hellvile night mail) · Ch4 · deliver (x5), find
Hook: the first mailbag across. Flapjack reads addresses upside down. Beats: 1) five letters, five wrong doors `[RIPPLE]`; 2) sort them out; the fifth has no address: "to the baker"; 3) Binny (744) knows: it is eleven years old, from Charlotte to Nana, found in a bin the week the Wall went up; 4) deliver it. Nana reads it `[HOLD 26f]`: "She said she was sorry. / She never did ANYTHING." Comic: LOU: "Every address by heart." FLAPJACK: "Every address by foot." Heart: the old letter becomes the last page in the Pen Pals furniture. Reward: 50 H$, F: *Mailbag (two flags)*.

**S114 · Plain Water** — Vex (666) · Ch4, after Ep 4.4 · fetch, talk
Hook: Cloud Ten needs water for tea, and Vex wants it from "the laughing one". Beats: 1) fill a bottle at the giggling fountain; it giggles into the bottle; 2) carry it; the bottle giggles on every step; 3) Vex makes tea; it is terrible; "I love it."; 4) Haze waters the plant with the rest; the plant perks up `[POP]`. Comic: the kettle giggles when it boils. Heart: VEX: "You were nice to me. / Even then." (if SF_VAPE_SAMPLE: "Even high.") Reward: 40 H$, F: *Cloud Ten Teapot* (giggles when it boils). VEX 3/5.

**S115 · Cosy, Dark, Eternal** — Hauntessa (109) + Pharaohnie (673, Hellvile tomb realtor) · Ch4, after the Annex · housing
Hook: the Annex needs rating by "both traditions". Beats: 1) decorating goal: Hellvile set in the Annex (4 pieces) and the mixed-style bonus; 2) Pharaohnie's walkthrough: "Cosy. Dark. / ...Where's eternal?"; 3) place a cursed item (`[INSERT 4f]`, one change, "it's fine."); 4) both stamps `[SLAM]` x2, 15f apart: "TIDY" and "ETERNAL". Comic: Pharaohnie tries to sell your house to Hauntessa. Heart: Hauntessa has never been inside a house someone stayed in; she dusts the height marks around, not over. Reward: F: *Realtor's Sign ("MUST SEE")*, U: Open House eligible. DECOR 4/4.

**S116 · Shared Head** — Steamy (2) + Ma Gloom (561) [saved] / Steamy's ghost + Candlewick (760) [lost] · Ch4 · visit, deliver
Hook (saved): Steamy wants to meet "the lady who was in me" and bring her a duck. Beats: 1) escort Steamy to the crypt; 2) Ma Gloom at the door `[HOLD 13f]`; 3) they compare notes: she left the lights on in him; he hummed her lullaby wrong; 4) the duck goes on the windowsill by the candle. Hook (lost): ghost-Steamy wants a candle at the Lake of Fire; Candlewick lights it and, for once, it stays lit. Comic (saved): MA GLOOM: "You snore." STEAMY: "YOU snore." MA GLOOM: "I was a GUEST." Heart: both branches `[NO GAG]` at the end. Reward: 40 H$, F: *Duck on a Sill* / *Lake Candle*. STEAMY 3/3.

**S117 · Signhere's Receipt** — Signhere (567, Hellvile contract devil) · Ch4, after Ep 4.5 · deliver, talk
Hook: Signhere needs everyone who tore a page to sign an acknowledgment that they tore it. It is 900 pages. Beats: 1) carry it to 6 cast members; each signs one letter of their name; 2) Grumbleguts signs with a buttprint `[SCRATCH]`; 3) the pet signs with a beep (a tiny horn-shaped mark); 4) Signhere files it: "This is in breach of / nothing. I don't know / what to do with that." Comic: the acknowledgment has a 900-page appendix. Heart: Signhere's first contract nobody regrets. Reward: 50 H$, F: *Contract Confetti Jar*.

### 12.7 Post-game window (S118-S133)

**S118 · Double Act** — Hattie Haha + Honkers · Ch5 · concert
Hook: the Unity Stage, one night only. Beats: 1) concert minigame as the two-hander; 2) the Mayor is in the front row; the knock-knock lands; 3) he laughs: "Hee. Hee." and it is real; the whole crowd turns to look at him `[FREEZE all 13f]`; 4) NANA: "...There he is." Comic: Honkers honks at the laugh; it's a compliment. Heart: Hattie keeps the laugh; this one doesn't fit in the jar. Reward: 60 H$, U: song "Double Act", U: photo "The First Laugh". HATTIE 4/4.

**S119 · Two Brave Kids** — Pip + Wisp · Ch5 · photo
Hook: Shutterbug wants the two kids and both plushies in one frame. Beats: 1) the bandstand; 2) the photo; saved branch: Ma Gloom in the back, faint; 3) Wisp: "Is Mum in it?" SHUTTERBUG: `[HOLD 13f]` "She's in it. Trust me."; 4) lost branch: Wisp holds the candle in the frame instead. Comic: Mr. Flops blinks in the photo; he has button eyes. Heart: `[NO GAG]`. Reward: F: *Two Brave Kids (framed)*. PIP 3/4.

**S120 · Lifeguard Flops** — Pip · Ch5 · fishing, pet
Hook: Pip is teaching Mr. Flops to swim so "he never goes to sea by accident again". Beats: 1) the pier; fishing minigame variant: keep Mr. Flops afloat with the rod; 2) three rounds; the pet "guards"; 3) PIP: "He's mostly cotton. / I'll hold him." Comic: Splash (63) does a backstroke demonstration in the fountain. Heart: Pip's eyes do a rain; "Rain's good for stuff." Reward: 40 H$, F: *Lifeguard Flops Chair*. PIP 4/4.

**S121 · Kevin's Bust** — Kevin + Chisel (105, Hillvile sculptor) · Ch5 · creator, housing
Hook: Chisel wants to sculpt Kevin "accurately". Beats: 1) SCENE_CREATOR: pose Kevin; 2) the bust in marshmallow; 3) placed in your house, only the third eye blinks, every 83+ frames; 4) visitors' reactions: three, 15f apart, the topper is the pet sitting on it. Comic: Chisel did two eyes first "out of habit". Heart: KEVIN: "It's lonely." `[R21]` "Less, now." Reward: F: *Kevin's Bust*. KEVIN 3/3.

**S122 · One Eye, Three Eyes** — Kevin + Oddsball (522) · Ch5 · escort, talk
Hook: Kevin's third eye has never seen south. Beats: 1) escort him through the gate; 2) Oddsball, a cacoblob with one eye, at the market; 3) they stare `[HOLD 26f]` `[MUTE 26f]`; KEVIN: "Yes, three." ODDSBALL: "Yes, one."; 4) they blink at the same time `[R8]`, and that's the friendship. Comic: Oddsball bets on which eye will blink first and loses to the third. Heart: four eyes between them, finally the right number for a game of cards. Reward: 40 H$, F: *Eye Chart (both)*. KEVIN 2/3.

**S123 · He Waved** — Lumina (76) · Ch5 · visit, photo
Hook: the starer (Haybale, 35) is at the festival, still. Beats: 1) find him at the edge of the park; 2) stand perfectly still for `[R21]` (the player must not press anything); 3) he waves, slowly, 8f phases; 4) photo. Button: 1 in 20 mornings afterwards he is at your window, and if you stand still, he waves. Comic: Lumina waves back at him like a lamp. Heart: "He's been there the whole / time. He just wanted to see / how it ended." Reward: F: *The Starer's Curtain* (he appears behind it 1 in 20 mornings). STARER 3/3.

**S124 · Bones & Riffs** — Bonejangles (5) + Riffmaw (511) · Ch5 · studio, concert
Hook: a xylophone-and-doom-guitar collaboration. Beats: 1) SCENE_STUDIO: a 4-bar track; 2) Riffmaw's volume tunes the church bells; Fuzz comes out; 3) Bonejangles plays his own ribs for the solo; 4) the concert. Comic: Headbang (628)'s skull comes loose in the front row; Safetypin (570) fixes it. Heart: the song is the potluck song, in two tempos, meeting in the middle. Reward: 60 H$, U: song "Bones & Riffs".

**S125 · Nap Time in Hell** — Dozy Doe (34) + Batty Betty (920) · Ch5 · minigame
Hook: Dozy Doe has trained 200 citizens to nap; Betty naps upside down and calls it yoga. Beats: 1) a nap-off (rhythm minigame: breathe on the beat); 2) three rounds, Betty upside down; 3) Grumbleguts joins and falls asleep instantly, on the crowd, `[SQUASH 1.3/0.8]` for the crowd; 4) Dozy declares him "naturally gifted". Comic: the belly snores in a different key. Heart: Dozy admits she cannot nap herself. Betty teaches her: "Hang on. Let go." Reward: 40 H$, F: *Yoga Bat Hook*.

**S126 · Game Night, Both Sides** — Graveley (730) + Oma Fluff (27) · Ch5 · minigame, talk
Hook: Graveley's crypt kids' game night needs a fourth; Oma Fluff has not played since "1974. Or was it." Beats: 1) the board game (minigame); 2) Oma's glasses (from #8) are the wrong ones; Graveley has the other pair; 3) `[HOLD 13f]` they remember: the potluck, 1974, a game night that ran till the fireworks; 4) they swap back. Comic: the kids win every round. Heart: `[NO GAG]`. Reward: 40 H$, F: *Board Game (both sides' pieces)*.

**S127 · Burnt and Sweet** — Nana Puddin + Charlotte · Ch5 · cooking, minigame
Hook: the bake-off, revived; Grumbleguts' belly is the judge. Beats: 1) cook alongside Nana; 2) Charlotte's entry is 89% charcoal (progress); 3) the belly judges: "grrrbl." `[HOLD 8f]` G: "Tie." NANA & CHARLOTTE, together: "ALWAYS a tie."; 4) the trophy is cut in half with a bread knife. Comic: Scorchmello and Crumbelina (#40) protest the tie and are given the other half of the knife. Heart: Nana's bakery trays go back to one of everything, except Thursdays. Reward: 60 H$, F: *Half a Trophy (x2)*. NANA 4/4.

**S128 · The Longest Lullaby** — Ploddy (701) + Maestro Mew (3) · Ch5 · concert
Hook: Ploddy hums lullabies to clouds all night; Mew wants it conducted. Beats: 1) the Concert Hall at night; 2) Mew conducts with his tail; the lullaby is Ma Gloom's, which Steamy hummed wrong; 3) the small grey cloud in the jar (on a cushion in the front row) rains one pixel on the beat; 4) the hall sleeps. Comic: the orchestra follows the tail off the stage. Heart: `[NO GAG]`. Reward: U: song "Hellvile Lullaby", F: *Cloud Cushion*.

**S129 · The Little Mouth** — Vex · Ch5 · deliver (x3), talk
Hook: Vex has written three apologies and cannot deliver them himself. Beats: 1) to Haze: she reads it, waters the plant with the envelope; "Accepted."; 2) to the Mayor: "Dear Mayor, I sold you a / vacation." `[HOLD 13f]` MAYOR: "I bought it." ; 3) to Rivet: "Don't make it weird." VEX (via you): "I'm making it weird." RIVET: `[R26]` "...Yeah okay."; 4) the three replies, taped in his hat. Comic: the letters are on Cloud Nine stationery with the logo scribbled out. Heart: the Duke never apologised to anyone, and Vex learned the word by himself. Reward: 50 H$, F: *Three Replies (hat)*. VEX 4/5.

**S130 · Cloud Ten Garden** — Vex + Haze (709) · Ch5 · fetch, housing
Hook: the old vape shop's back room is a garden now, and Haze needs plants. Beats: 1) fetch 3 plants (Bluebell, Sweetpea, Sootkin's sunflower seeds); 2) plant them; 3) the plants lean toward Vex `[HOLD 13f]`; HAZE: "They like you. / They never liked the / smoke."; 4) a sign goes up: "CLOUD TEN. Tea. Plants. / Plain water. It's fine." Comic: the plants hate the plain water vape too. Heart: Vex is employee of the month, of a shop with two employees, and the board says "Vex" in Haze's handwriting. Reward: F: *Employee of the Month (Vex)*. VEX 5/5.

**S131 · Free Hugs (Both Sides)** — Hugsley (20) + Big Snarl (855) · Ch5 · talk, minigame
Hook: Hugsley wants to hug everyone in Hellvile; Big Snarl is the bouncer. Beats: 1) bring a squeaky toy (Big Snarl's rule); 2) Hugsley hugs Big Snarl: Snarl squeaks `[SFX_SQUEAK]` `[HOLD 8f]`; "...Again."; 3) a hug line forms `[RIPPLE]`; the hug-audit minigame (gentle, gentler, Grumbleguts); 4) Grumbleguts asks for a hug; Hugsley gives him one; the belly says "grrrbl" in a tone nobody has heard before. Comic: Hugsley hugs the Gate; it creaks; Grumbleguts: "Hey." Heart: the hug counter passes 1,000 for the first time in eleven years, with both sides. Reward: 50 H$, F: *Hug Counter (two digits more)*. HUGSLEY 2/2.

**S132 · Twelve Candles** — Candlewick (760) + Candlenab (883) · Ch5 (night) · find (x12), visit
Hook: the twelve wisps from the Pump went home; Candlewick wants a candle lit where each landed, and Candlenab has been stealing candles to do exactly that. Beats: 1) find 12 landing spots (both towns; each is a small scorch with a flower in it); 2) light each; Candlewick burns a finger on every one `[TINK]` x12; 3) the twelfth is at the Lake of Fire, lost-branch Steamy's or Ma Gloom's; 4) the twelve flames flicker in the lullaby's rhythm `[MUTE 64f]`. Scare: the Lake at night, no music, the walk there. Heart: `[NO GAG]`; Candlenab gives the last candle to Wisp. Reward: 60 H$, F: *Twelve Candles (garden)*. GARDENS 2/2.

**S133 · A Candle Goes Out** — Wisp (563) · Ch5, after S96-S99 · talk, housing
Hook (saved): Ma Gloom asks you to help Wisp let the window candle go out: "She's home. It can rest." Hook (lost): Wisp asks you themself. Beats: 1) the window at dusk; 2) WISP: "If I blow it out, is it / giving up?" > No. > It's letting her rest. > (say nothing); 3) `[HOLD 26f]` `[MUTE 64f]`; Wisp blows; 4) saved: Ma Gloom's hand on Wisp's shoulder; "There. Now I can SEE / out." Lost: Pip arrives with a lantern "for the room, not the window." Both: the pet beeps, once, very small. Comic: none. The quest has no comic beat; it is the one place in the side content with the button withheld entirely. Reward: F: *Window Candle (out)* replaces S96's lit one; U: trophy "Somebody Left the Light On". WISP 5/5.

---

## 13. Gameplay integration notes

### 13.1 Where everything unlocks (main path)

| system | first touched | story use | repeatable / free play |
|---|---|---|---|
| Housing: decorate | Ep 1.2 (tutorial, goal COSY) | Ep 1.5 Housewarming; Ep 2.2 goal LIT; Ep 3.1 goal HOLD THE DOOR; Ep 4.4 Annex + goal OPEN HOUSE; S115 goal ETERNAL | always, from the pink door; weekly visitors rate it post-game |
| Housing: visit others | Ep 4.2 (Wisp's crypt), Ep 4.3 (the forge) | Ep 4.4 (crypt upstairs), S110 | post-game: any friend's door with level >= 2 opens a small interior (template rooms per archetype; 6 templates) |
| House defence | Ep 3.1 (tutorial wave, daylight) | Ep 3.5 (three waves, the siege) | Ch3 nights: "Night Raid" at your door if you sleep at home (1 wave, optional, drops Scrap); post-game activity "Hold the Door" at the Arena with the house as the map |
| Hell Breach 1 Vape Cellar | Ep 2.6 | story | Arena from Ch3 |
| HB3 Sugar Rush Sewers (Gumdrop Gutterking) | S75 (visit only) | Ep 3.2 graveyard route (boss) | Arena from Ch3 |
| HB5 Graveyard Shift (Weeping Widow) | Ep 3.2 (night, optional) | the Widow's Plushie (furniture) | Arena from Ch3; night only in story |
| HB4 Plushie Factory (Foreman Fluffnuts) | Ep 3.4 | story, Fuzz escort | Arena from Ch4 |
| HB2 Cloud Nine Spire (Vapula) | Ep 3.6 | story, eight taunts, laughter mechanic | Arena from Ch4; #39 Echo of the Duke |
| HB6 Bone Cathedral (Bishop Bonebottom) | Ep 4.4 side door | Nox's crypt; the Wall's foundation stone "FOR THE SAFETY OF" completes here: "...EVERYONE. (G.G. / M.N.)" | Arena from Ch4 |
| HB7 Hellvile Underbelly (Grumbleguts) | Ep 4.5 | the Pump; pacifist route | Arena from Ch5; the pacifist route is a separate record |
| Invasions (nests) | Ep 3.1 | four authored nests + the hidden fifth (S83) | Ch3: packs respawn at night as wanderers until the Spire; post-game: "Echo nests" at the Arena only (the park stays clean) |
| Exorcism | Ep 2.3 practice | authored rites: Steamy/Ma Gloom, Hattie/Honkers, Grampy/Marrowmere, Lady Smog/Fluffy, Mr. Flops-adjacent (none: plush can't be possessed; Fuzz checks), the Feast culprit; plus side #14 #16 #24 #30 #37 and S87's rite-less wisp | the practice altar stays open (Mr. Huggins, no stakes) |
| Welding | Ep 2.5 | Ep 4.1 (your bead), S52 S70 S86 S79, I-B flashback bead | workshop always from Ep 2.5 |
| Pet | Ep 1.3 | horns Ep 2.2, full horns Ep 2.6, the jar Ep 3.6, halo Ep 4.6; "beep." as a dialogue answer (Ep 4.2) | SCENE_PET always; the pet's form is cosmetic + the "demons flinch" aura in Ch3 |
| Fishing / cooking | S65 / S64 | Ep 4.3 dinner (cooking variant), S97 S104 S127 | Pier, Bone Pier, Cafe, Brimstone Bites |
| Concert / studio | S56 | Ep 3.3 (the rite plays over a set), Ep 4.6 stage | Concert Hall, Unity Stage, Town Hall studio |
| Creator | S51 (Kevin's hat) | Kevin as a preset; S121 | Town Hall |
| Dreams | I-A (forced) | I-D (forced), SF_VAPE_SAMPLE variant in Ch2 | Peek at a Dream at night, home and apartments |
| Graveyard scene | S58 S72 | lost branches (Steamy, Hattie, Grampy, Ma Gloom's candle), Fluffy's stone | always |
| Free play island visits | after Ep 4.6 | STORY_ARG_VISIT_* now also allow side quests S118-S133 | post-game |

### 13.2 Day/night and calendar gating (the game finally uses time)

* Night-only: Ep 1.6 (after 21:00), S57, S67, S77, S82, S87, S92's fifth duck, S132, HB5, Night Raids, Peek at a Dream, the fog's whispers in Ep 2.2.
* Day-only: Ep 1.5 ("come home at six" ends it), the Housewarming guests, S76's three photo times, S109's two waterings.
* Ch2's "a few days pass" is three real mornings; Ch3 is one long red night (the sun does not rise until the Spire falls; the clock still runs so the daycare's bedtime bell in Ep 3.4 lands at 20:00).
* Chapter 4 opens at dawn; S101 and the DAYS SINCE sign count real days from there; the epilogue's "1" is real.
* The Feast (§11.3) is weekend-only post-game (day % 7 == 6) so the fullness meter has a visible calendar countdown; the running-gag spawner never fires during it.

### 13.3 Branch matrix (what the save remembers and where it pays off)

| flag | set | read |
|---|---|---|
| Steamy saved / lost | Ep 2.4 | Ep 2.5 Fuzz's lines; #13 hidden; S67 S92 S116 giver swap; Ep 4.2 Honkers/Smog pool; Ep 4.6 Steamy & Ma Gloom's duck or Candlewick's candle; S132's twelfth candle |
| Ma Gloom home (always, if Steamy saved; a wisp without a host does not dissipate) | Ep 2.4 | Ep 4.2 Pa Gloom; Ep 4.4 upstairs vs the Lake; S96-S99 S116 S119 S133 variants; Wisp's stage line |
| Hattie saved / lost | Ep 3.3 | S100 S118 exist only if saved; lost: the laugh track in the hall for the rest of Ch3, a second two-hander at the stage played by Honkers alone |
| Grampy saved / lost | Ep 3.4 | S88 S89 lines; the bench at the festival has one or two |
| Lady Smog saved (Fluffy named) | Favour 2 | Ep 4.6 the jar has "the rest of him"; if the player fails three times the Duke's last puff dissipates and the jar cloud never rains in Ep 4.6 (the Mayor still says "Hi, Fluffy."; the hold is longer) |
| SF_VAPE_SAMPLE | Ep 2.1 | Ch2 dream variant; S114 line; the fog in Ep 2.2 knows your name |
| SF_WEAPON_BRUTAL | Ep 2.5 | Fuzz's judgement captions in combat ("We judge a little."); Rivet's Ep 4.6 stage line ("Hit it!" vs "Bonk it!"); the Spire's rimshot pickups are spoons on the CUTE path |
| Wisp's question (true / true / kind lie) | Ep 4.4 | Epilogue line; S133's second option |
| USTORY_PACIFIST (five courses) | Ep 4.5 / HB7 | Grumbleguts' Ch5 greeting; the Feast's Branch A reward doubles |
| Feast A / B | §11.3 | everything in §11.3's consequence list; the credits cameo is removed in B (the buffet is full and nobody is at it) |
| Fluffy's grave visited (Ep 1.1) | Ep 1.1 | the Mayor's I-C confession adds one line ("You saw the stone.") |

### 13.4 Decorating goals and what they gate

| goal | requirement | stamp | gates |
|---|---|---|---|
| COSY | 6 items incl. bed + egg nest | COSY | Ep 1.3 hatch (the egg hatches only in a cosy room) |
| TIDY (S54) | 6 items, door clear | TIDY | Hauntessa chain |
| LIT | 3 lamps (candles x2) | LIT | Ch2 nights get shorter whispers per lamp; at 3 the wallpaper eye stops blinking |
| HOLD THE DOOR | barricade HP >= 100, turret placed | HELD | Ep 3.5 outcome (4 guests safe = all four cast lines in I-C) |
| OPEN HOUSE | 8 seats, both styles, Annex furnished | HOME | Ep 4.6's final scene; weekly visitors post-game |
| ETERNAL (S115) | 4 Hellvile pieces + 1 cursed item | ETERNAL | the Realtor's Sign; Pharaohnie's visitors |
| room rating (post) | weekly | A-F | three visitor reactions 15f apart, escalating, then a topper; "cursed" items trigger `[INSERT 4f]` + "it's fine." |

### 13.5 Running-gag spawner (overworld, weighted, with cooldowns)

A table in ROM: `{gag, weight, cooldown_minutes, chapter_mask, night_only}`. Entries: the starer at a window (R21 still -> wave); the crowned cloud as a background prop (Ch2-3); a pet "beep." during a sincere NPC line (never during a `[NO GAG]` beat; the scheduler checks); an NPC double-take when you pass (once per NPC per visit, S-chain NPCs get authored lines); the DAYS SINCE sign flipping as you pass Rivet's; Grumbleguts backing into frame anywhere in Hellvile (Ch4-5, `[SCRATCH]`, max once per 4 minutes); Wobblewisp moving a prop 2 inches (Ch2+, after S68). Cadence from the timing bible §5: one ambient micro-gag per 20-40 s of walking, none for 5-15 s after any big beat.

### 13.6 Silence budget and music

* Every cutscene above carries its own `[MUTE]`s; the chapter totals land at 21-24% of cutscene time music-free (counted per beat sheet).
* New stingers from the trailer kit (SCRATCH, BOING, POP, THUD, WHOOSH, KAZOO, BEEP, GLITCH, TIK, SHWIP, TINK, FANFARE, CHOIR) plus one new song, "Potluck" (music-box in P0, hummed by Giggs and Nox, full band at Ep 4.6, forwards+reversed as the two-faith rite).
* The border crossing always `[SCRATCH]`es; the Ch4 Hellvile theme re-enters on a downbeat (`audio_music_resume_on_beat`).

### 13.7 Length budget (how it feels long without padding)

| | hours | made of |
|---|---|---|
| main path | 12.5 | 24 episodes, 4 interludes, prologue, epilogue; no counters |
| new side missions | 9-11 | 84 quests at 5-10 min; chains spaced across chapters |
| existing side missions | 4-5 | 49 quests, re-timed |
| housing | 2-4 | 6 goals, visitors, furniture from 60+ sources |
| Breach / Arena / activities touched by the story | 2-3 | HB1-7 in story order, Night Raids, concerts |
| post-game | 3-8 | Park Life, the Feast, NG+ "Both Sides" |

The connective tissue is callbacks: 31 quests have a button that lands 1-10 minutes later; 19 chains span two or more chapters; every main-path episode changes at least one NPC's idle line, one prop or one piece of your house.

---

## 14. Implementation notes (mapped to `story_map.md`'s constraints)

1. **Beats are appended, never reordered** (`story.h:74`). New `BEAT_*` ids after `BEAT_DONE`: one per episode (P0, 1.1-1.6, IA, 2.1-2.6, IB, 3.1-3.6, IC, 4.1-4.6, ID, E, FEAST) = 32 new beats; `beat_ch[]` extends; the questlog's progress bar reads the new table. The old 28 beats map onto episodes (C1_MAYOR -> 1.1, C1_KEY -> 1.2, C1_HOME/HATCH -> 1.3, C1_PIP/BUNNY/RETURN -> 1.4, C1_CUPCAKE -> 1.5, C1_WALL -> 1.6, C2_VAPOR -> 2.1, C2_WEIRD -> 2.2, C2_SCREAM/FUZZ -> 2.3, C2_EXORCISE -> 2.4, C2_RIVET -> 2.5, C2_CELLAR -> 2.6, C3_ERUPTION -> 3.1, C3_NESTS -> 3.2-3.5, C3_SPIRE -> 3.6, C4_GATE -> 4.1, C4_PRISONERS -> 4.2, C4_HELP -> 4.3, C4_MAYOR -> 4.4-4.6, C4_FESTIVAL/STAGE -> 4.6) so `story_debug_jump` and `test_story.c` keep working with the old ids as aliases.
2. **Flags.** The 28 reserved bits + `SF_31_FREE` + spare "met" bits (36-39, 42-47, 48-55, 58-63) cover the main-path one-shots listed in §13.3 (about 30). Everything else (84 new quests x 2 bits = 168 bits; house layout; height marks; the Counter Board; the Feast state; Grumbleguts RES_DEAD lock; the starer's 1-in-20) goes in two new `save_ext` chunks: `'STR2'` (<= 256 B: quest bits, episode one-shots, branch answers, counters) and `'HOME'` (<= 1,024 B: 64 placed items x 6 B + goals + visitor log). Both fit the 8 KB per-chunk cap and the 40 KB GBA story slot. GBA EWRAM cost is the resident copy of those two chunks (about 1.3 KB) against 7.5 KB free; the decorate scene's working set lives in the 12 KB scene scratch.
3. **Text and scripts are data, not callbacks.** Every beat sheet above is a `BeatScript` (timing bible §7.2; 8 B per event, a 60 s cutscene about 1.2 KB) and every conversation a table of `{who, expr, beatcode, text}` rows run by one resident interpreter. That keeps the roughly 110 KB of new text in GBA ROM and, on PS1, in the story overlay group (49.8 KB today, 108.8 KB slot) or a per-chapter streamed pack, instead of resident RAM. Side-quest rows follow the existing `SideQuest` shape plus `chain_id/step`, `branch_mask` and up to 6 `beat` ids.
4. **PS1 cuts stay inside the story group.** Every `[SMASH CUT]`, `[INSERT]` and `[COVER CUT]` above cuts between the overworld, a cutscene and dialogue (same group or resident). Cuts into combat, exorcism, cooking, concert, creator or the pet happen only at episode boundaries, where a CD read is acceptable, and never mid-gag.
5. **New runtime pieces** (from the timing bible, all integer): `tween.h` (tables, bounce LUT, springs), `beat.h` (the scheduler, named anchors so re-timing a beat drags its SFX), `camfx.h` (shake, punch, zoom, flash, iris), `plat_image_xform` (squash about the feet), per-line dialogue beat codes (`{p N} {.} {..} {...} {d} {!} {big} {w} {r} {cut N} {punch}`), `audio_music_mute/duck/tapestop/resume_on_beat`, the 13-sound comedy SFX kit, NPC scripting ops (walk-to, hop, turn, emote, flee, freeze, panic) and a camera field in `DoomoOverworldView` (pan, zoom on PS1 via GTE H; GBA zoom via BG2 affine for 1-2 frames under a flash, otherwise cut).
6. **Housing** (`housing_tech.md`): a 12x10 cell interior map per house (cottage, Annex, 6 archetype templates for friends' homes), furniture catalog of about 120 entries in ROM (not inventory items: `ITEM_COUNT` is at 63/64), 16 free texture ids for interior sets, iso sprites on GBA (sorted by x+y, the held item an affine OBJ), textured quads on PS1.
7. **New named characters without new roster ids:** Kevin is a creator preset who moves in through the creator's town-write (replacing an unnamed Hillvile slot chosen at first use); the starer is Haybale (35, "wakes up as a scarecrow"); Grumbleguts is a boss entity (`LB_GRUMBLEGUTS`) given a resident shadow record for romance/dating/RES_DEAD so the sim can treat him as a citizen in Ch4-5. Everyone else is a real roster row (App. A).
8. **Tests to keep or update:** `story_full_playthrough_reaches_the_credits` (now via episodes, autowin covers combat/exorcism/cooking/concert/housing goals), `story_debug_jump_sets_up_every_beat` (32 new beats), `story_side_quests_table` (n between 120 and 140; every kind used; chains validated: each step requires the previous), the reachability tests on the interior maps, a new `beat_script_timing` test that asserts every `[HOLD]` in a punch beat is unskippable and every `[INSERT]` is 3-4 frames, and a `silence_budget` test (20-25% per cutscene).
9. **Docs to update:** `docs/STORY.md` (chapter titles, quest count, episode list), `levels.c` boss intro text for Grumbleguts (bestiary line, the flirt, the belly), `docs/GAME_DESIGN.md` play-time target (10-15 h main).

---

## Appendix A: authored possessors (the `@D` table)

| host | possessor (roster id) | clue props in the name phase | the four names offered | saved: the wisp's exit | lost: what stays |
|---|---|---|---|---|---|
| Steamy (2), Ep 2.4 | Ma Gloom (561), night-shift worker, packs everyone lunch | lunchbox, gardening glove, night-shift lamp, child's drawing of a house with a candle | Honkers, Lady Smog, Ma Gloom, Scallywag | looks at you, looks south, goes over the Wall; Steamy hums her lullaby wrong | ghost-Steamy keeps the bath-house; Ma Gloom's wisp still goes home (a mother's soul does not dissipate with a host; the game says so in Fuzz's line) |
| Hattie Haha (71), Ep 3.3 | Honkers (501), creepy clown, honks at funerals | red nose, funeral programme, confetti cannon, Cotton Candy | Honkers, Snitchet, Femurfly, Hazey | honks once, bows, goes south; Hattie's laugh sounds like the fog | the laugh track plays in the empty hall for the rest of Ch3; Honkers performs alone at the stage |
| Grampy Bean (88), Ep 3.4 | Marrowmere (817), fossil elder, same bench for a century, waves | century-old bench, a wave, bone broth, a fossil | Marrowmere, Grizzlebolt, Dreadie, Wrenchley | sits on the bench for R21, then goes; the bench has two at the festival | the bench has one; the tomatoes stay purple |
| Lady Smog (502), Favour 2 | Lord Vapula's last puff (999), i.e. Fluffy | crooked crown, a lost string, a school bag, a gravestone | Vapula, Fluffy, The Duke, Nimbus (only Fluffy works) | into the jar with the rest of him | the puff dissipates; the jar never rains in Ep 4.6 |
| Bellwether (#24), Hugsley (#30), Lagspike (#16), Hazey's host (#37), Sr. Lumen's cases (#14) | hash-picked from Hellvile SOFTIE/LOYALIST rows as today, now excluding the authored ids above and the Gloom family | existing | existing | the wisp always goes south; the possessor gets one line in Ch4 if met ("You shouted my name.") | existing loss rules |
| the Feast culprit (§11.3) | drawn from the guests present (your friends list); the possessor is a leftover Pump bottle Signhere "forgot to file" | the guest's own job/food/hobby props | four guests' names | the feast ends; Branch A | Branch B |

## Appendix B: the three player lines and where the box shows the player's name pill

The player speaks exactly three times (§2.1). Every other "choice" is a menu of actions or of what the pet does. The name pill on the dialogue box is drawn only for those three lines, so the first time it appears (Ep 3.5, the hole in the wall) is itself a beat: `[WAIT A]` then the pill `[POP]`s with `TW_POP_BUBBLE`, and the text types at the setup speed, not the punch speed, because it is not a joke.
