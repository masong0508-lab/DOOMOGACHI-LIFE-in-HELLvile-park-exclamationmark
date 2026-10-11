# DOOMOGACHI LIFE in HELLvile Park! — STORY BIBLE, HEART-FIRST CUT

*"A town that laughs so it never has to look at the Wall."*

Scope: a complete rewrite-and-expansion of Story Mode for both the GBA ROM and the PS1 disc. It keeps the world, every established name and the four-chapter backbone (Ch1 Welcome to HaHaland, Ch2 Something in the Vapor, Ch3 Hell Invades, Ch4 Hellvile Park) and deepens it into 26 episodes, 7 interludes, 4 dream sequences, 2 playable flashbacks, a midpoint twist, a dark night of the soul, an ending in Hellvile Park and a post-game. Target: 10–15 h main story, 15–25 h with side content and housing. Every number in brackets is a 30 Hz frame count and comes from `comic_timing.md`. Nothing here touches the repo; it is a design document written against the snapshot described in `story_map.md`.

---

## 0. How to read this document

### 0.1 Timing notation (from the Comic Timing Bible)

| tag | meaning |
|---|---|
| `[R2] [R3] [R5] [R8] [R13] [R21] [R26]` | gap on the beat ladder: BLINK, REACT, BEAT, TAKE, HOLD, PAUSE, DEADPAN |
| `[HOLD Nf]` | a hold the A button cannot shorten (`BOP_HOLD`) |
| `[WAIT_A min Nf]` | waits for A, but not before N frames |
| `[MUTE Nf]` | music muted, sequencer keeps running; SFX still play |
| `[DUCK 0.5 Nf]` | music gain to 0.5 for N frames |
| `[TAPESTOP]` | 5-frame pitch slide down, then `[MUTE 14f]`, re-enter on the grid |
| `[SMASH CUT]` | hard cut, SFX on frame 0 (WHOOSH / THUD / GLITCH) |
| `[INSERT Nf]` | 3–4 frame tonal whiplash insert; owns the soundtrack for its length; return to the same shot with ONE change |
| `[FREEZE actor]` / `[THAW]` / `[PANIC]` | clamp an actor's idle clock / resume / 2x clock |
| `[POP table]` | appear from small: `TW_POP_BUBBLE {102,307,236,266,256}` etc. |
| `[SLAM table]` | land from large: `TW_SLAM_STAMP {614,410,236,266,256}` |
| `[DROP soft]` | `t^2` fall, squash on contact, `TW_DROP_SOFT_*` |
| `[SQUASH sx/sy]` | peak deformation on the contact frame, anchored at the feet |
| `[SHK_*]` | shake profile from Appendix C of the timing bible |
| `[FLASH white 10,5]` | flash levels per frame, 0..16 |
| `[IRIS heart r26 hold 5f]` | iris out, hold on a detail, the button lives in the hold |
| `[RIPPLE 0..3]` | crowd reaction staggered by `id % 4` frames |
| `[DEADPAN]` | text appears (no pop, no hop), actor clock 0, music muted |
| `[TYPE]` / `[POP TEXT]` / `[APPEAR]` | setup lines type, punch lines pop, deadpan lines appear |
| `[BUTTON +Nf]` | the after-beat, N frames after the punch |
| `[NO GAG]` | a sincere beat with the button deliberately withheld |

### 0.2 Dialogue format

Lines are written for the 240x160 box: at most 3 lines of about 28 characters. A `/` marks a line break inside one box. A new box is a new quoted string. `@P` is the player, `@E` the pet, `@D` the authored possessor name (see Appendix A), `@V` flavour of the day.

Speaker tags: NARR (narrator, no portrait), PET (the Doomogachi: its whole vocabulary is "beep.", a horned "beep.", "bep?" and once, ONCE, something else), and the cast by name.

### 0.3 The four pillars

1. **Heart first.** Six relationships carry the game: you and your Doomogachi; Wisp the Hellvile kid and their missing mum; Rivet and the family she welded herself away from; Father Fuzz and the faith he stuffed himself with; Vex and the Duke who used him as furniture; Mayor Giggs and Mother Nox, who used to run one park together. Every scare and every gag is parked next to one of those.
2. **Scary is comedy with the release removed.** Same holds, same silence, same clamped idle clocks. The fog does not get a punchline until Chapter 4.
3. **Funny is delivery.** The old script's jokes were good on paper; the new one gives them R-ladder gaps, inserts, stings and buttons.
4. **It feels long because it is connected, not because it is padded.** Every episode changes the town, the house or a relationship. Buttons land minutes or chapters later.

---

## 1. The world, deepened (lore that was missing)

### 1.1 Before the Wall: one park, two potlucks

Hellvile Park was one park. On the north bank of the river lived the Hillvile folk (plush, pastel, polite). On the south bank lived the Hellvile folk (horns, bones, soot, also polite, slightly louder). Once a year the two mayors met at the bandstand in the middle: Mayor Gigglesworth brought cupcakes, Mother Nox brought bone soup, and the Potluck of the Park ran until the fireworks. Nana Puddin and Charlotte judged the bake-off. Chief Spark lit the bonfire. Kids from both banks swapped plushies and teeth.

### 1.2 Eleven years ago: the Duke sells a Wall

Lord Vapula, Duke of the Ninth Cloud (a cloud nobody else ever visited), arrived with a contract devil named Signhere and a nine-hundred-page contract. He told Giggs that Hellvile was about to come north and eat everyone. He told Nox that Hillvile was about to come south and "sanitize" everyone. Both mayors signed a Wall on the same night, each one convinced they were protecting their own. Vapula built the Wall through the middle of the park and kept the bandstand. The gate was welded shut by an apprentice imp named Rivet, age ten, who then slipped through the last glowing gap and ran north with her torch still lit.

### 1.3 The Pump

Behind the Wall, Vapula turned Hellvile into a vapor works. Citizens pump "Cloud Nine" vapor at the gate on double shifts (Tallyworn has counted 3,652 days). Vapor is not just smoke: the Pump draws out a sliver of soul with every shift. Vapula bottles the strongest ones and sends them north as fog. **A possessing "demon" in Hillvile is a Hellvile soul in a bottle.** Exorcism does not destroy it: a saved exorcism sends the soul home across the Wall as a wisp; a failed one (three strikes) dissipates both host and soul. This is the midpoint twist (Ch2 Ep6) and it recolours every exorcism in the game.

### 1.4 Vapula's real demons

The things you shoot are not citizens. Cloud spiders, imps of the Ninth Cloud, thunderheads with teeth, the Breach mini-bosses (Gumdrop Gutterking, Foreman Fluffnuts, the Weeping Widow, Bishop Bonebottom, Gatekeeper Grumbleguts): all of them are Vapula's cloud-stuff, made from vapor and spite. Cartoon gore belongs to them and to the hell world only. **No cute character is ever harmed on screen in the cute half of a whiplash.**

### 1.5 The Doomogachi

Nana's egg is one of the old Potluck eggs: a plush-and-brimstone pet bred in the days of one park, half Hillvile, half Hellvile. It beeps. It grows horns under stress. It is the only creature in the game that belongs to both sides, which is why it is the player's voice, why the fog calls to it in Chapter 3, and why its poop grows horns after the credits.

---

## 2. The cast: want, wound, payoff

| character | want | wound | payoff |
|---|---|---|---|
| **You (@P)**, bunnykin "Sunny" by default | a home | you arrived alone with one suitcase; the game never says why, and you say almost nothing (3 lines total, each one earned) | your house becomes the first place both towns sit down in |
| **@E the Doomogachi** | to be near you | it is half hell; the fog knows its name | it beeps at the Duke; it walks in the credits beside you; its poop grows horns |
| **Mayor Giggs** | everyone laughing | he signed the Wall; "Hee. Hee." is the sound of a man not looking at something | walks through the gate; says "I signed it" out loud to Nox |
| **Mother Nox** | her potluck back | she signed the other half; eleven years setting two bowls | "I LOVE soup." then, quieter: "You're late." |
| **Nana Puddin** | to feed everyone | her pen pal Charlotte's letters stopped the week the Wall went up; she kept baking two of everything | the bake-off ("Burnt and Sweet") |
| **Pip + Mr. Flops** | to be brave | lost Mr. Flops; thinks brave means not crying | gives Mr. Flops to Wisp for one night and is fine; "My eyes are doing a rain. It's okay. Rain's good for stuff." |
| **Father Fuzz** | to hug the hurt out of things | he is literally stuffed with love "and a little bit of buckwheat"; believes nothing is unhuggable; loses Hattie (or Steamy) and discovers the hug sent someone's mum home or nowhere | learns the hug was never for the demon; performs the two-faith exorcism with Nox |
| **Mr. Huggins** | (a plush dummy) | has been exorcised 400 times | gets a grave AND a seat at the stage |
| **Rivet** | to be a Hillvile person | welded the gate shut at ten, left Bolt in a crib, writes letters she sends by balloon and pretends never to send | cuts the gate open herself; teaches Bolt the first bead; "It's a REALLY good spoon." |
| **Anvil / Sprocket / Bolt / Nana Clank** | Rivet back | a set place at dinner; a dad who "doesn't read" the letters; a sibling who learned to weld from the letters | dinner |
| **Vex** | one "good job, Vex" | the smallest demon on the Ninth Cloud; Vapula sat on him for three hundred years; he practised the smile in a mirror because the Duke said it was "too small" | is called "Vex" and not "Vexy-Poo" by a whole festival; plain water vapes; opens Cloud Ten (tea) |
| **Lord Vapula** | to be feared | a Duke of a cloud nobody visits; the only thing that hurts him is laughter; he has never been invited to anything | deflates; becomes a plush; is invited |
| **Wisp (563)**, imp kid | Mum home | Ma Gloom went to the night shift at the Pump a year ago and did not come back; Wisp plays ghost around the house "so if Mum comes back as one she won't be scared of being one" | (saved branch) Ma Gloom drifts home; (lost branch) a candle at the Lake of Fire with Pa Gloom and Auntie Ash's stew; either way, Pip's plush for one night |
| **Pa Gloom** | to keep the crypt and the snacks | tells Wisp "Mum's on nights" every night | stops saying it |
| **Steamy** | a rubber duck in every tub | the first host | (saved) "Why do I taste grape?"; sends a duck to Ma Gloom |
| **Hattie Haha** | to be funny | possessed in Ch3; afterwards every laugh sounds like the fog's | the comeback show at the Unity Stage |
| **Grampy Bean** | tomatoes | possessed in Ch3; the tomatoes rot | a tomato from Hellvile soil |
| **Tallyworn / Sootkin / Shackles / Drudge** | down days / a sunflower / a garden / a beach | the gate | the gate opens on day 3,653 |
| **Lady Smog** | air | eleven years of smog gown | "It's DELICIOUS." |
| **Haze (709)** | plants | works the Cloud Nine counter and hates the smoke | runs Cloud Ten's garden |
| **Candlewick (760)** | one candle to stay lit | 10,000 candles, 9,000 burned fingers; Nox's niece | the candle in Wisp's window |
| **Miss Lulla, Dr. Bloom, Chief Spark, Updraft, Shutterbug, Lagspike, Sutura** | (side chains, §8) | | |

### 2.1 The player's three lines

The player is silent: everybody else talks, the pet beeps, the player nods. Three times in the whole game a line is typed into the box with the player's name pill, and each one is a plot event:

1. **Ch3 Ep5 (The Longest Night):** the pet's name, shouted into the fog. `@P: "@E!"`
2. **Ch4 Ep4 (The Crypt):** to Wisp, when asked "Did she say anything?" The player gets a choice of two true things and one kind lie. The game remembers which.
3. **Ch4 Ep6 (the Open House):** `@P: "Come in."`

### 2.2 Lord Vapula: design brief (per the director's note)

The director asked for a big-butt devil in the lineage of the Red Guy from *Cow and Chicken*: pantsless, cheeky in every sense, forever finding a reason to turn around, and "slightly BDSM-y, this is hell we're talking about". Within the plush-critter art direction:

* **Silhouette.** Pear. The Ninth Cloud is a cushion he never leaves; when he stands up, the cloud is revealed to have two dents. The cloud itself is drawn as a butt-shaped cumulus with a tiny tornado tail.
* **Costume.** A spiked collar, a leather harness with far too many buckles, opera gloves, a tiny cape that covers exactly nothing, and a riding crop he calls "the Executive Wand". No trousers, ever; a smog-leaf doily appears over the relevant area whenever the camera gets too close, and the doily sweats. He refers to his court as "my naughty little clerks" and they visibly age every time he says it.
* **Business.** He enters every scene backwards. He sits on everything: thrones, Vex, the camera (a 3-frame black-out with a squeak, `[INSERT 3f]`), the Mayor's desk in the flashback. He moons the gate during the Ch3 eruption (the cutscene is composed so the thunderhead he exhales rises from, well, the wrong end). He cracks the crop and hits himself with it on the backswing every time (`[R3]` yelp, `[R8]` "...as intended").
* **Voice.** Baritone babble sliders; a wheeze at the end of long lines (he is a balloon and knows it).
* **Rating.** Cartoon cheek, never genitals, never pain-for-pain's-sake; the kink is a costume and a running gag the long-suffering Hellvile citizens roll their eyes at ("He calls it a dungeon. It's a basement with a nice rug."). The safe word, revealed in Ch4, is "cupcake". This is what finally deflates him.

---

## 3. The dream house: the emotional anchor

The player's cottage at Hillvile Lane (4,7) is the only space in the game that changes in every chapter, and it is where the game's biggest sincere beats happen. It is also the decorating sandbox: isometric on GBA (sorted diamond tiles plus sprite stacks in mode 4, the held item an affine OBJ), 2.5D on PS1 (a small cell map rendered by `plat_overworld` with furniture as textured quads/low-poly meshes, camera pitch lifted to about -45° indoors).

| chapter | state | decorating goal (the house rates you; a grade stamp `[SLAM stamp]`) | sincere beat that happens here |
|---|---|---|---|
| 1 | **Cosy.** Empty room, a crate, an egg. Nana's housewarming fills it. | "Make it yours" (place 6 items incl. a bed and the egg nest). Goal rating COSY. | the hatch; the first "beep." |
| 2 | **Haunted.** Grape smell. Things move 1 cell at night. A face in the wallpaper (`[INSERT 4f]`). Balloon letters land in the garden. | "Keep the lights on" (place 3 lamps; candles count double). Goal rating LIT. | reading Rivet's family letters by lamplight; the pet grows its first horn in its sleep |
| 3 | **Barricaded.** Welded plates on windows, neighbours sheltering, a turret made of a toaster. House-defence waves at night. | "Hold the Door" (barricade HP, 3 waves, keep 4 guests safe). Rating HELD. | the Longest Night: the living room with one wall gone, the pet missing, your first spoken line |
| 4 | **Open.** The south wall becomes a door. Hellvile furniture mixed with Hillvile. Two tables pushed together. | "Open House" (seat 8 guests from both sides; mixed-style bonus). Rating HOME. | "Come in." Mayor and Nox at your table. Wisp asleep on Mr. Flops. |
| post | **Yours.** Everything unlocked, two gardens, a plaque. | free; weekly visitors rate it (three reactions 15 frames apart, escalating, then a topper) | the morning the starer is at the window (1 in 20) |

Housing interactions use the trailer vocabulary verbatim (timing bible §6.4): place = `[DROP soft]` with THUD+BOING on contact and a 2-frame-late `TW_JELLY` ripple on neighbours; invalid spot = 4-frame "nope" shake plus a sad squash, no red flash; rotate = squash-flip with the edge-on frame; wallpaper = covered cut (a citizen runs across with a paint roller); cursed item = `[INSERT 4f]` then one persistent change ("the lamp grew horns") and "...it's fine." from a visitor at `[R8]`.

Furniture comes from side missions (§8 rewards), from both towns' shops after Ch4, from Breach drops and from the story (the Ch1 crate, Ch2 candles, Ch3 welded plates that stay as "battle scars" you can keep or remove, Ch4 Hellvile pieces). Running-gag merchandise: the DAYS SINCE LAST FIRE sign (resets when Rivet visits), the horned-poop plushie, Kevin's three-eyed bust (only the third eye blinks, every 83+ frames), the Plain Human figurine (the only decor that never animates), the deflated Vapula plush (sighs when sat on), the crown that lands crooked no matter how you place it.
