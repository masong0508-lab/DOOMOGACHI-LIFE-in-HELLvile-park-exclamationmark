# Pitch A: "ARRIVAL"

> **Superseded development draft. Do not board, record or pitch from this file.** Pitch C won; the pilot is specified by `../PILOT_BEATSHEET.md` and `../PILOT_TREATMENT.md`, and this pitch's quiet material now lives in Eps 102-104 (`../SERIES_OVERVIEW.md` §5.3). Its rating, audience and comps (TV-PG, a 10-16 family audience, kids' and family comps) are superseded: the show is adult animation, TV-MA, bound by four hard limits (`../PILOT_ROADMAP.md` §1). Known places where this draft falls short of the binding standards now in force: 3-frame and 1-frame inserts (the show's minimum is 4 frames) and a 1-frame LCD pentagram (too short: the minimum is 4 frames). See `README.md` in this folder.

## DOOMOGACHI LIFE in HELLvile Park!, Episode 101: "Population: 500 (and Sunny)"

**Angle.** The pilot is the player's arrival, and it stays faithful to the game. It adapts the prologue (P0, "The Egg Crossing") and the opening of Chapter 1: Ep 1.1 "Population: 500", Ep 1.2 "The Key in the Muffin" and Ep 1.3 "Hatch Day". Two Chapter 1 side missions supply the B and C plots: S107 "Speech Notes" and S199 "The Loose Brick". The canon is `docs/design/story/story_final.md` and the shipped script in `core/data/story/ch1/*.dsc`. Dialogue in quotes is the shipped or canon text. Where the pilot restages something, it says so in "Canon decisions" at the end.

---

## 1. Logline

A silent bunnykin with one suitcase moves to HaHaland, the happiest town anywhere. The town hands them a pink-door cottage, a key baked into a muffin and an egg that hatches into a pet whose whole vocabulary is "beep." Then they notice that the laughing fountain giggles half a second late, and that nobody in town will look at the Wall.

---

## 2. Format

* **Runtime:** a single 22:00 episode, made up of 21:50 of programme and a 10 s studio card.
* **Why not 2x11:** the canon rule for Chapter 1 is "make them love this town and this house before anything goes wrong". Love needs room. A 7-minute act gives the hatch a 21-frame hold of nothing before the "beep.", and an 11-minute slot would cut that hold first. The 11-minute format is better kept for the post-Wall "Park Life" shorts.
* **Shape:** cold open, then main title, then three acts with an eyecatch pair at each break, then a tag, short credits and a post-credits button.
* **Main title:** the "Doomtastic Times" main title cut, 0:50. It slots between 0:45 and 0:55. The constant-speed bus drive that opens Act One is the elastic shot that absorbs any difference once the master is locked.
* **Eyecatches:** two pairs, an OUT card into each break and an IN card out of it, each 5 s.
* **Streaming version:** the same cut. The eyecatches stay in as chapter markers, and the main title gets a skip-intro marker.
* **Frame rate:** 29.97 fps, so the comic-timing ladder (2/3/5/8/13/21/26 frames) carries over from the game unchanged.

### Rundown

| TC in | Segment | Length | Ends on |
|---|---|---|---|
| 00:00 | **Cold open:** "The Egg Crossing" (crayon) | 1:45 | The egg cracks on the first downbeat of the theme |
| 01:45 | **Main title:** "Doomtastic Times" (main title cut) | 0:50 | The logo, the "nothing to see here! ♥" sticker, and a purple sparkle that sinks |
| 02:35 | **Act One:** "Population: 500" | 6:00 | Fluffy's grave, and the wings stop flapping |
| 08:35 | Eyecatch pair 1 (OUT, break, IN) | 0:10 | |
| 08:45 | **Act Two:** "The Key in the Muffin" | 6:00 | The glow pulses at the Wall and the egg ticks |
| 14:45 | Eyecatch pair 2 (OUT, break, IN) | 0:10 | |
| 14:55 | **Act Three:** "Hatch Day" | 5:40 | "Old draft." The fountain giggles late |
| 20:35 | **Tag:** "Thursday" | 0:45 | "Neither did I." with no button |
| 21:20 | End credits (music-box arrangement of the theme's hook) | 0:30 | |
| 21:50 | Post-credits button, then the Dippinn Productions card | 0:10 | "and beep." |

**Elastic beats** for 21:00 or 23:00 territorial cuts are the decorating montage in Act Two, the beach search in Act Three and the crowd ripple at the dusk welcome.

### Main title plan, to be cut to the "Doomtastic Times" master (`docs/show/theme/`)

The lyric and bar grid are not locked yet, so picture is boarded by section and conformed to the master when it lands. These are the five beats:

1. **Intro.** The egg crack is the sound on frame 0, with a white flash. The camera cranes up over pastel HaHaland (built from the trailer's s02), and the title letters drop on the beat.
2. **Verse.** The cast pops in on the beat, each entrance taking 4-6 frames with overshoot, and each gets a name card in the CUTE window skin. The order is Sunny with the suitcase, the pet, Mayor Giggs, Nana, Pip with Mr. Flops, Father Fuzz, Dr. Bloom, Steamy, Hattie, Grampy and Rivet (behind a welding mask). Mascotty does not pop in. She is simply there in every shot of the verse, perfectly still.
3. **Pre-chorus.** The town pans toward the Wall (built from s03), and the song itself makes the record-scratch. The camera stops on the gate: seven padlocks, "KEEP OUT. FOREVER."
4. **Chorus.** On the lift, the hellify effect rots the same shots into Hellvile. The cute-mode FPS corridor bursts into confetti, Rivet's weld throws sparks, and Father Fuzz's hug-rite plays in silhouette. Behind the gate, one huge round silhouette bounces once. That is Grumbleguts' only appearance before his debut.
5. **Ending pose.** The frame splits at the Wall, pastel on one side and Hellvile on the other, and the halves meet on the logo. The sticker slaps over the red drip. After the last chord comes a 13-frame silence, then "beep." Then the canon chapter-card button: the last sparkle is purple, and it sinks.

**The opening changes as the series goes.** One detail updates per arc, the way anime openings do:
* After Arc 1, the far glow in the Wall shot blinks back.
* After the gate cracks (game Ep 2.7), the gate has a crack.
* After the midpoint (Ep 3.4), pipes are visible under the town.
* The finale's opening has no Wall in the pan.

### Eyecatch plan: "the two-sided card"

* **The rule** comes from comic_timing.md rule 5, the whiplash insert. The **OUT** card is pure HaHaland, scored with a 2-bar cut of the theme's hook. The **IN** card is the same card: it opens with a 4-frame hellify flash on frame 0, then returns to cute with **exactly one persistent change**. It is scored with the hook turned minor and resolves on the "it's fine" sting (a cheery chord over a wrong bass), rearranged into the theme's key.
* **Pilot pair 1** (Act One into Act Two):
  * OUT: the town sign, "Population: 500 lovely citizens. and Sunny." with the pink ticket stuck to the corner, and the Hee-Hee bell.
  * IN: the same sign, with Mascotty standing beside it, waving very slowly.
* **Pilot pair 2** (Act Two into Act Three):
  * OUT: the egg in its basket by the lamp, as the "COSY: B+" stamp slams down.
  * IN: the same, but the egg has turned to face the Wall and has a hairline crack. This hands straight into the hatch.
* **The season runner.** Mascotty is somewhere in every IN card of the series, a little closer to camera each episode. She takes the head off only in the canon finale beat (Ep 4.6).
* **Uses beyond the broadcast.** Each card is a seamless 5 s loop, ready to post as a short.

---

## 3. Target audience and rating

* **Core audience:** 10-16, built for family co-viewing, with a strong secondary audience of adult animation fans. Comparable shows are Gravity Falls, Over the Garden Wall, Hilda and Amphibia.
* **Series rating:** TV-PG (D, V) is the ceiling, never exceeded. The "D" arrives with Grumbleguts' innuendo in the gate arc, and the "V" with the cute-mode, confetti-only demon fights.
* **The pilot:** submitted as TV-PG for consistency, though its content sits at TV-Y7 level:
  * no violence;
  * no suggestive dialogue;
  * no Grumbleguts: the gate is seen but nobody behind it is seen or heard;
  * no vaping: Cloud Nine Vapor is only a "COMING SOON" sign with a purple flicker, with no product and no smoke;
  * no OMINOUS cue: the canon holds that cue back until Ep 1.6, so every scare in the pilot is made of stillness and silence.
* **Binding boundaries carried into the series bible:**
  * Gore is confetti, hearts and stuffing only. The game's BRUTAL setting and Monster Mode are out.
  * Kids and teens are never in romantic or suggestive material, or in any scene that carries it. That covers Pip, Wisp, Bolt and Miss Lulla's kids.
  * Grumbleguts flirts with adults only, stays at innuendo and slapstick, is always turned down kindly, and is never the butt of a mean joke.
  * Sunny is the audience's stand-in, so Sunny is never the target of his flirting.
  * The post-game Feast outcome and the adult-humour passes in the tone notes are game-only.

---

## 4. The pilot's story

**A-plot: Sunny makes a home.**

**Act One.** A pink bus crosses the candy hills at a constant speed and stops with a record-scratch. Sunny, a bunnykin with one suitcase, is thrown off the step, and the suitcase lands eight frames later and bounces. The town sign says "Population: 500 lovely citizens." It updates to 501 for Sunny, and the "1" falls off. Someone has added, in pencil: "and Sunny." Mayor Giggs flaps over and the crowd waves in a ripple of hearts. Mascotty, in her giant bunny head, does not wave.

The Mayor's tour points at the bakery, the fountain, the flowers and the lovely, lovely sky: at everything except the Wall. When the camera drifts toward it, he steps in front of the lens. Sunny never speaks. They point at the Wall instead, and the whole town freezes and turns to look at them. "We don't talk about the other side." The fountain giggles, late. Sunny's key is with Nana, and the shortcut to her bakery runs through the graveyard. There, past a child-sized stone reading "FLUFFY THE CLOUD. DRIFTED.", with a cupcake on it, a pair of wings behind Sunny stops flapping.

**Act Two.** Nana Puddin's bakery has two of everything. The key is in a muffin, and nobody remembers which one. Raisin, raisin, a button, and then Sunny bites down on a key with tiny horns. Nana brings out an egg in a tea towel: "It's been waiting for someone." She holds on to it a beat too long before she lets go. "The last one hatched into a little demon! Ha! Kidding. ...Mostly."

The pink-door cottage is empty except for a crate and a window. A decorating montage earns it "COSY: B+", and the crate slides one pixel in shame. Sunny leaves the suitcase by the door, still packed. Nana stops in the doorway: "You put a lamp by the egg. That's exactly where my mother put hers." That night a purple glow at the Wall pulses three times, and on the third pulse the egg ticks.

**Act Three.** The egg hatches on the downbeat of the morning music. The pet hops to the lamp and sits in the light. For a long moment nothing happens. Then, in the smallest speech bubble in the show: "beep." A caption appears: "...it's fine." Sunny writes the pet's name on the basket tag, and the tag flips over before we can read it.

At the fountain Pip is crying, because Mr. Flops is lost ("My eyes are just doing a rain."). Mr. Flops turns up on the beach, face-down in the surf and staring at the Wall. Behind him a paper boat drifts in from the south. Inside it is a crayon house with a candle in the window, two imps, and "ARE YOU THERE" in backwards letters. The pet sits on it, in love. Pip will keep it "safe. Like Mr. Flops." and gives Sunny a skull sticker ("Skulls are cute."). Sunny puts it on the suitcase.

**B-plot: the Mayor's welcome speech.** The Mayor has given his welcome speech four hundred times, and today he has lost it. Sunny finds one page per act:
* in the fountain basin (Act One);
* under a tray of twin cupcakes (Act Two);
* in Mascotty's closed paw (Act Three), which opens only after Sunny stands perfectly still.

Every page has "Hee. Hee." in the margin where a sentence used to be. At the dusk welcome he reads the whole speech. It is one sentence: "Welcome to HaHaland, where... hee. hee." "It kills every time." The crowd ripples with laughter. Miss Lulla's eleven kids shout "BEEP!" at the pet, and it answers, very quietly, "beep." Then the Mayor turns over the last page. It is in different handwriting: "You never asked where it went." He folds it very fast. "Old draft." The fountain giggles, late.

**C-plot: Nana's two of everything.** The cold open shows Nana sending an egg south by paper boat and getting burnt cookies back, and every tray in her bakery comes in pairs. The tag explains why. The pet won't settle and keeps turning to face the Wall, so Sunny follows it out into the night. Nana is there, at a brick that wiggles. She slides a muffin into the gap. A hand with soot on it takes the muffin from the other side, and a bag of cookies, 90% charcoal, comes back. She eats one: sick, then in love, the same face she made in the cold open. "Every Thursday. She never missed one." A beat. "Neither did I."

**What the pilot is about.** HaHaland never stops talking, and its hero never speaks. HaHaland never stops moving, and its hero can stand still. That is how Sunny sees what the town refuses to look at. By the end Sunny has a home, a pet and a sticker on a suitcase that is still packed. Sunny also now knows three things the town would rather they didn't:
* there is a grave for a cloud;
* someone on the other side is asking "ARE YOU THERE";
* one kindness has been crossing the Wall every Thursday.

---

## 5. The cold open in detail: "The Egg Crossing" (1:45, crayon)

* **Look:** children's-book crayon on paper grain, using the game's `PAL_CRAYON`, with a 2-frame line boil so it reads as hand-drawn. There are no faces for young Giggs or young Nox, only wings, horns, hands and bowls.
* **Captions:** in a child's hand, read aloud by an unseen storybook narrator.
* **Music:** the Midsummer Mingle song as a slow music-box arrangement.

| TC | Frames | Picture | Sound | Words |
|---|---|---|---|---|
| 00:00 | 0-90 | Black. | A music-box key winds: three clicks, 15 frames apart. On the third the lid opens and the Mingle tune begins. | |
| 00:03 | 90-480 | Circle iris in on the bandstand. Two tables: a cupcake tower on the north side, a soup cauldron on the south. Slow parallax on the crayon layers. | Music box. | "Every year, the park had a potluck." |
| 00:16 | 480-900 | Kids run across the bridge both ways, in a ripple staggered 0-3 frames. A small cherub, seen from behind, holds a cloud on a string. At the soup table a small imp sets down two bowls. | Crowd babble, kept soft under the music. | "Everybody came." |
| 00:30 | 900-1440 | Crayon fireworks on the beat. The cloud looks up with love-heart eyes. R8 later the string slips, and the shot holds for 13 frames. | A single pluck on the slip. | "Fireworks! The cloud looked up. It loved fireworks." Then: "The string slipped. Nobody noticed." |
| 00:48 | 1440-1800 | The camera pans south with the cloud as the fireworks fade. | The music **mutes for 29 frames**, stopping mid-phrase, at exactly the note where the Mayor will stop humming in Act One. | "Not everybody went home." |
| 01:00 | 1800-2220 | **SMASH.** The same bandstand, years later, with a Wall through it, and the crayon has lost its pink. Two tables, one on each side. Two bowls on Nox's side, one cupcake on Giggs's. | A thud on frame 0, then a rumble shake. | A deadpan caption **appears** without typing on: "Nobody talks about why." **The narrator does not read this one.** |
| 01:14 | 2220-2940 | A covered cut through fog. Nana, a sheepkin, kneels at the river and pushes a paper boat from north to south, with an egg inside. R13 later a boat comes back the other way. Button at +21: she eats a cookie, looks sick, then looks in love. | A splash, then a second splash. | "Except Nana. Nana sends eggs." Then: "A boat comes back the other way: burnt cookies." |
| 01:38 | 2940-3150 | Heart iris out on the egg (a 5-frame close), then a hold. The egg wobbles once. | A tiny crack on the last frame. **The crack is the first downbeat of "Doomtastic Times."** | |

---

## 6. Act breaks

* **Act One out, 08:35: the grave.** Rows of joke graves ("Rest in pieces, Mr. Huggins the 1st"), then a small stone in a child's hand: "FLUFFY THE CLOUD. DRIFTED." There is a cupcake on it. Behind Sunny, the Mayor has been following "to make sure you find Nana's. People try." His wings stop. MUTE 13. His face goes sad for exactly R8, then back to happy. The wings start again, and the flap is the frame-0 sound of the OUT eyecatch. *The question:* who was Fluffy, and why does the Mayor know?
* **Act Two out, 14:45: the glow.** Night. Sunny is asleep next to the packed suitcase. Through the window, far off at the Wall, a 1-pixel purple glow pulses three times, R21 apart. On the third pulse the egg in the lamplight ticks, with the same tiny crack sound as the cold open's last frame. Cut. *The question:* is something on the other side calling the egg?
* **Act Three out, 20:35 (runs straight into the tag): the old draft.** "Old draft." MUTE 14. The fountain giggles, R8 late. A circle iris (the creepy shape, not the heart) closes on the folded page as it goes into his breast pocket. *The question:* who wrote it? The answer is the plumber's order, in the game's Ep 3.4 flashback.

---

## 7. The button and the tag

* **Tag, "Thursday" (0:45).** The pet keeps turning to face the Wall from the window: turn it round, and R13 later it is facing the Wall again. "beep." Sunny follows it out under the stars to the Wall, where Nana is crouched at a brick that wiggles.
  * NANA: "Sweet pea. You're up late."
  * She slides a muffin in. HOLD 26 and MUTE 26.
  * A sooty hand takes it, in a 3-frame insert with a tap shake. A bag of charcoal cookies is pushed back.
  * She eats one: sick, then in love.
  * NANA: "Every Thursday. She never missed one." [R13] "Neither did I."
  * **No button.** The final sincere beat is never undercut.
* **End credits (0:30).** A music-box arrangement of the theme's hook, so the episode closes in the same instrument it opened in. The TV-size theme is kept for arc finales and specials.
* **Post-credits button (0:10).**
  * The town sign at night. Under "and Sunny." someone has added, in smaller pencil: "and beep." R8 later, from offscreen, a tiny "beep."
  * Then the Dippinn Productions card (the studio ident bumper from `trailer/assets/`), silent until its last frame, which is the pet's "beep."

---

## 8. Characters introduced and their pilot arcs

| Character | Who they are | Pilot arc | Seeds |
|---|---|---|---|
| **Sunny** (they, by show decision; a young adult in their first home) | A bunnykin newcomer who never speaks. Canon gives the protagonist exactly **three lines in the whole story**. They act by pointing, carrying things and standing still. | Arrives anonymous: the sign doesn't count them, but the pencil does. Gets a home, and something hatches for them. Ends at the Wall at night, seeing kindness cross it. The suitcase stays packed. | The suitcase is unpacked in the epilogue, revealing an empty photo frame that the festival polaroid fills. The countdown of the three lines is a series-long event. |
| **The pet** (it) | A Doomogachi, the Mingle egg that is half plush and half brimstone, and the only creature that belongs to both sides. It says "beep." | Hatches on the downbeat. Faces the Wall. Leads Sunny to Nana's brick. **Its name is never shown or said.** | Horn nubs ("He's teething."). In canon, Sunny's first spoken line (Ep 3.7) is the pet's name, so the show reveals the name in Sunny's first word. |
| **Mayor Giggs** (he) | A cherub with wings, a top hat and a bow tie, and the sing-song "Hee. Hee." tic: "the sound of a man not looking at something." | Delivers a flawless welcome with three hairline cracks: "...the weather side.", the R8 sad face at the grave, and "Old draft." He says the theme line for the first of its three times, as a joke. | The grave, the hummed Mingle tune, the page. Later: the confession, "I had a cloud once", "...Hi, Fluffy." |
| **Nana Puddin** (she) | A sheepkin baker in a chef's hat who calls everyone "sweet pea". She bakes two of everything and hides clues in food. | Gives up the egg she kept ("...Mostly."). Then chooses to let the newcomer see her Thursday secret. | Charlotte, the Thursday cookie trade across the Wall (Chain D). Her R8 looks at the Mayor. |
| **Pip** (they; a kid, friendship only) | The tiniest bunnykin, with the highest and fastest voice in the cast. Mr. Flops (he) has one floppy ear, one not-floppy ear and a stitched smile. | Thinks brave means not crying. Ends brave enough to keep a stranger's drawing safe: "That's somebody's HOUSE." | "He says he saw the Wall breathe." The paper-boat pen pal (Wisp). "Rain's good for stuff." |
| **Mascotty** (she) | The town mascot in a giant bunny head, still in a crowd that never stops moving. "Ignore that one." She waves very slowly, and only to people who stand still. | The pilot's quietest scene is between its two silent characters. Sunny stands still for R21, and she opens her paw. | The 3-then-1 thumps, her brother Thumpkin, and the head coming off in the rubble (Ep 4.6). |

**Cameos** (each gets one line or gag, with full introductions in Ep 102, "Three Neighbours"):
* Kevin (he): "Yes, three eyes." [R5] Only the third eye blinks.
* Grampy Bean (he): "Welcome, Sonny." This is his name runner at its smallest.
* Bonejangles (he): plays a chord when hugged.
* Miss Lulla (she) and her eleven kids: "BEEP!"
* Steamy (she) and Hattie Haha (she): in the crowd.

**Felt but not seen:**
* Fluffy, the cloud on a string.
* Young Giggs and young Nox, as wings and bowls.
* Charlotte (she): the sooty hand.
* Wisp (they): the boat.
* Whoever sits on the other side of the gate.

---

## 9. The five best gags

1. **"Population: 501."** The sign drops in with a tap shake: "Population: 500 lovely citizens." It updates to "...and now, Sunny!" (501). R8 later the "1" falls off with a *tink*. Button at +13, and at +22 a pink ticket flutters down onto Sunny's head. For anyone who pauses the frame, a scratched-out "1000" sits under the 500.
2. **The Mayor blocks the camera.** "There's the bakery! The fountain! The flowers! The lovely, lovely sky!" He points at everything except the Wall. The camera creeps toward it anyway at 0.04 px a frame. He steps into the lens and fills the frame with a smile: "Hee. Hee." This teaches the show's grammar in one shot: the town is hiding something from the camera itself.
3. **The freeze.** "Hellvile Park. Two sides. This one, and..." [FREEZE, HOLD 13] "...the weather side." Sunny points at the Wall. Everything stops at once: a tape-stop, and every citizen turns to look, staggered 0-3 frames, then HOLD 21. Deadpan: "We don't talk about the other side." HOLD 13. The fountain giggles, R8 (267 ms) late. The music comes back on the downbeat: "Hee. Hee!"
4. **The muffin bite test.** The setup is Nana's "The key's in a muffin. Don't ask which. I forgot. I always forget." Then raisin, raisin, "that's a button", at 15-frame intervals. The fourth bite crunches, with an eye-pop and a flash, then a 3-frame x-ray insert: **the key has tiny horns**. The button is a *tik*, R2 later, and nobody comments.
5. **"beep."** The pet sits in the lamplight. HOLD 21 with nothing happening, music muted for 29 frames. Then the smallest speech bubble in the show: "beep." R8 later a deadpan caption just appears: "...it's fine."

**The bench**, in case one of the five needs swapping:
* "COSY: B+": three imaginary visitors react ("♥", "♥♥", "...is that a crate?") and the crate slides 1 px in shame.
* Mr. Flops face-down in the surf, staring at the Wall, and the "FOUND!" sticker slam.
* "Welcome to HaHaland, where... hee. hee." "It kills every time."
* "Not a fence OF faces. Faces AT a fence. Hee. Hee."

---

## 10. How the pilot seeds the season

**Season shape.** The pilot covers P0 and game Ep 1.1-1.3. Episodes 102-104 finish Chapter 1:
* 102, "Three Neighbours";
* 103, "Housewarming", with the toast slip "To fluffy— ...cupcakes." and Rivet's coat hook ("don't make it weird");
* 104, "The Giggling Fog", which ends the arc on the glow **blinking back** and closes on the horn-nub tag.

After that come the gate cracking (the end of Chapter 2, with Grumbleguts' debut), "The Bottle" (the demons are people) as the mid-season turn, the plumbing reveal as the series midpoint, and "...Hi, Fluffy." as the finale.

| Pilot seed | Where in the pilot | Canon payoff |
|---|---|---|
| **The plumbing secret** | | |
| The fountain giggles R8 (267 ms) late | Act One, Act Three | S142, "267 milliseconds late... So who's laughing?" Then Ep 3.4: the fountain cracks and the pipes run under the Wall. The series midpoint. |
| "You never asked where it went." in other handwriting | Act Three out | The F3 flashback, "Keep giggles IN. Pump glums OUT." "Out where?" "Not here." Then the confession: "...I never asked where it went." |
| "We don't talk about the other side." (use 1 of 3, as a joke) | Act One | Use 2, the confession (Ep 3.4). Use 3, Hattie's festival opener: "Because we're STANDING in it!" |
| **Fluffy and Vapula** | | |
| The cloud on a string, and "Not everybody went home." | Cold open | Fluffy came home huge and became Lord Vapula, made of every sigh Hillvile swallowed. |
| The grave, its cupcake, the wings that stop, the sad face for R8 | Act One | Interlude III, "I had a cloud once." Ep 4.6, "...Hi, Fluffy." |
| The music box stops at one note, the same note the Mayor stops humming on | Cold open, Act One | The Mingle song, and the finale's lift into its key. |
| The purple sparkle that sinks | Main title | The crowned-cloud drawings (Ep 103), and Vapula. |
| **The Wall** | | |
| The camera creep, the freeze, the gate ("KEEP OUT. FOREVER.") | Act One, main title | The gate cracks (Ep 2.7). The Wall falls in the finale. |
| The paper boat "ARE YOU THERE", the house and the candle | Act Three | Wisp. The second boat and the candle (Ep 104), and the glow that blinks back. In Ch4: "That's MY house. You KEPT it?" |
| The 1000 painted over under the 500, and the pencil "and beep." | Act One, post-credits | The finale's "Population: 1,001." |
| Nana's pairs and the loose brick | Cold open, Act Two, tag | Charlotte (Chain D), the two-sided cookie tin, and the Mingle's two tables. |
| Mascotty's stillness | Throughout, and the eyecatches | The 3-then-1 thumps, Thumpkin, and the head coming off. |
| The key's tiny horns, and the 4-frame LCD pentagram at the naming | Act Two, Act Three | The horn nubs ("He's teething."). The pet eats the Mayor's fear (Ep 3.7). |
| Cloud Nine Vapor, "COMING SOON" | Act One, background | Vex (Ep 2.1). Its payoff: "plain water vapes. They're terrible." |
| The pet's withheld name, and the packed suitcase | Act Three, tag | Sunny's first spoken word (Ep 3.7), and the unpacked suitcase in the epilogue. |

---

## 11. Why a network or streamer would buy it

* **The mystery is already solved.** Every clue in the pilot pays off in a four-chapter story that is already written, with 27 game episodes and 8 specials mapped to beats. The writers' room is not bluffing, and buyers can read the ending before they commit.
* **Two shows in one frame.** It is a cozy life-sim (Tamagotchi and Tomodachi Life) and DOOM, in a single visual signature: the hellify rot, where pastel turns to hell pixel by pixel. The pilot keeps hell almost entirely in the main title, which makes the opening a promise the season keeps.
* **A theme that families can share.** "Say it out loud" is emotional literacy built into the plumbing rather than preached. The show is warm enough for parents and educators, strange enough for teens, and mean to nobody.
* **An anime-style opening.** "Doomtastic Times" is a J-pop-leaning opening built for clips, covers and dance edits. The score's leitmotif system has the HaHaland and Hellvile themes as the same tune in major and minor, meeting in the finale, so the music is part of the mystery.
* **A franchise that already exists.** There is a real GBA ROM, a real PS1 disc, a 78 s trailer and a studio ident. The pilot is the game's first hour. Merchandise suggests itself: the pet and its "beep.", Mr. Flops, the Population sign.
* **Cheap, fast development.** The 256-colour palette is the show's colour script. Model sheets already exist for all 1000 citizens. The trailer renderer turns a list of beats into a timed animatic, with placeholder cards, in a day.
* **A shape that suits every platform.** The 22-minute episodes have natural ad breaks. The eyecatches double as chapter markers and as 5 s social loops.

---

## 12. Risks and mitigations

| Risk | Mitigation |
|---|---|
| **A silent lead.** | It is a tradition (Keaton, Shaun the Sheep, Pingu), and a hook here: "three lines in the whole series" is marketable. The pet, the Mayor and the visual acting carry the scenes, and Sunny's actions (pointing, standing still) are staged as choices. |
| **A slow-burn pilot with no DOOM.** | The main title and the IN eyecatches deliver the hell half. Every act ends on a chill. The press kit includes the 16:9 trailer cut. |
| **Trademarks: "DOOM" and Tamagotchi** in "DOOMOGACHI" and "Doomtastic". | Clear the title and the song title with counsel before any pitch goes out. Prepare a fallback series title. The FPS look is homage, with no id assets or names on screen. |
| **Single-frame inserts and flashes.** Broadcast codes forbid subliminal images (Ofcom rule 2.11, for example), and flashing needs a photosensitive-epilepsy pass. | Play every insert at 3-4 frames, never 1. The canon's 1-frame pentagram plays at the 4 frames of the Interlude I spec. Run the hatch flash and the hellify flashes through a Harding or PSE check. |
| **Religious imagery** (pentagram, exorcism, church) in some markets. | The pilot has only one cartoon glyph, on an LCD screen. Father Fuzz's rite is hugs and hymns, never mockery. Standards and practices review starts on the series bible, not after Ep 2.7. |
| **Content later in the season** (Grumbleguts, Cloud Nine). | The show-safe Grumbleguts rules and the concept-sheet cuts are in the bible from day one: no hose close-up, no gas gag, no crop or unbuckle poses, no "...that's hot", and the harness becomes a gatekeeper uniform. Vaping is the villain's lure, satirised, with no brands. The `docs/design/story/refs/*.webp` images are third-party and never appear in pitch material. |
| **Pixel art at broadcast size.** The toolkit is 3:2, has no camera system or lip-sync, and every scene hard-codes its layout. | Re-stage on a 384x216 canvas, which scales by whole numbers to 1080p (5x) and to UHD (10x). Add a camera and rig layer, mouth-shape lip-sync, an external audio track, and named beats shared by picture and music, replacing the duplicated `FR(n)` constants. |
| **The theme song is not locked yet.** | Board the opening by section and conform it to the master. Cut the eyecatch stings from the stems. The bus drive absorbs a ±5 s difference. |
| **Gag density.** The trailer runs one gag every 1.7 s, which would be exhausting for 22 minutes. | Use the bible's densities: one punchline per 3-6 lines of dialogue, a 5-15 s rest after each big beat, and three heartfelt holds of 3-5 s with no gag (the lamp, "I'll keep it safe", "Neither did I."). |

---

## 13. Production notes: what "well produced" means for this pilot

* **Picture.**
  * 384x216 native pixel canvas, scaled nearest-neighbour by whole numbers, 29.97 fps.
  * Text stays inside title-safe. The diegetic captions ("...it's fine.") are graphics, so the delivery includes a textless master for localisation.
* **Colour script**, from the game's palette ramps:

  | Segment | Palette |
  |---|---|
  | Cold open | Crayon, losing its pink at the SMASH |
  | Act One | PASTEL_DAY sky, PASTEL_PINK and CANDY |
  | Act Two | Warm PEACH and WOOD interiors, then NIGHT with lit windows |
  | Act Three | MINT and SKY on the beach, SUNSET for the dusk welcome |
  | Tag | NIGHT_STARS and one VAPE-purple glow |

  **Rule:** the HELL and BLOOD ramps appear only in the main title, the IN-card flashes and the 4-frame insert.
* **Sound.**
  * Every cut has a sound on frame 0.
  * 20-25% of every act has no score.
  * A record scratch always goes to silence, then to a small sting.
  * The pet's "beep." is the game's own piezo motif, the one sound kept raw from the GBA.
  * Mixed at 48 kHz in stereo and 5.1, delivered at -24 LKFS (ATSC A/85), -23 LUFS (EBU R128), or each streamer's own dialogue-gated spec. -14 to -16 LUFS is for web clips only.
  * Delivery includes an M&E track and the theme stems.
* **Voices.**
  * Mayor: sing-song and maximally expressive.
  * Nana: low, slow, warm.
  * Pip: the highest and fastest voice in the cast.
  * Sunny: breath and foley only.
  * The cold-open narrator is warm, sparing, and never identified.
* **Animatic, ready in week 1.** List the pilot's beats with their durations in `trailer/timeline.c`. Unbuilt scenes render as timed placeholder cards, and contact sheets (`--sheet`) serve as thumbnail boards.

---

## 14. Canon decisions this pitch makes (flagged for the room)

1. **Sunny is "they" and a young adult.** Canon leaves the avatar's pronouns open. Sunny is never a target of romance or innuendo.
2. **The pet's name is withheld** until Sunny's first spoken line (canon Ep 3.7, where that line is the pet's name). The town calls it "the little one", and Lulla's kids call it "BEEP!"
3. **Restaged:**
   * The menu question "What's on the other side?" becomes Sunny pointing.
   * S107's speech is read at a dusk welcome.
   * The Mayor's hum, from Ep 1.6, is used once on the Act One tour and recurs in Ep 104.
   * S199 becomes the tag, with Sunny following the pet.
   * The egg's tick on the third pulse is added to Ep 1.2's night beat, which has no button in canon.
4. **Invented for the show** and consistent with canon: the skull sticker going on the suitcase, the pencil "and beep." post-credits button, the evolving opening, and the eyecatch system.
5. **Grumbleguts** is unseen and unheard in the pilot. Series picks: his crush is **the Gate**, and his catchphrase is **"Open late."** ("Sulfur kisses!" is shared with Vex).
6. **The starer is Mascotty.** The trailer's "plain human" is a separate figurine gag. Rivet's age is never stated on screen. Bolt is xe.
